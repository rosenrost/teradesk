/*
 * Teradesk. Copyright (c)       1993, 1994, 2002  W. Klaren,
 *                                     2002, 2003  H. Robbers,
 *                         2003, 2004, 2005, 2006  Dj. Vukovic
 *
 * This file is part of Teradesk.
 *
 * Teradesk is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Teradesk is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Teradesk; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


#include <np_aes.h>	
#include <stdlib.h>
#include <string.h>
#include <vdi.h>
#include <boolean.h>
#include <mint.h>
#include <xdialog.h>
#include <internal.h>
#include <library.h>

#include "resource.h"
#include "stringf.h"
#include "desk.h"
#include "error.h"
#include "events.h"
#include "open.h"
#include "file.h"		/* moved up */
#include "lists.h" 
#include "slider.h"
#include "xfilesys.h"
#include "copy.h"
#include "config.h"	
#include "font.h"
#include "window.h" 	/* before dir.h and viewer.h */
#include "showinfo.h"
#include "dir.h"
#include "icon.h"
#include "icontype.h"
#include "prgtype.h"
#include "screen.h"
#include "viewer.h"
#include "video.h"
#include "filetype.h" /* must be before applik.h */
#include "applik.h"


typedef enum
{
	ICN_NO_UPDATE,
	ICN_REDRAW,
	ICN_DELETE
} icn_upd_type;

typedef struct
{
	int x;
	int y;
	ITMTYPE item_type;
	ITMTYPE tgt_type;
	boolean link;
	int icon_index;
	boolean selected;
	boolean newstate;
	INAME label; 		/* label of assigned icon */ 
	union
	{
		int drv;
		const char *name;
	} icon_dat;
	icn_upd_type update;
	INAME icon_name;	/* name of icon in resource */ 
} ICON;					/* Data for one desktop icon */


typedef struct
{
	ICON ic;
	VLNAME name;
} ICONINFO;			


typedef struct
{
	ITM_INTVARS;
} DSK_WINDOW;

static ICON 
	*desk_icons;	/* An array of desktop ICONs */

OBJECT 
	*icons,	/* use rsrc_load for icons. */
	*desktop;

static int 
	m_icnx, 
	m_icny, 
	max_icons;

static boolean
	icd_islink;

boolean 
	noicons = FALSE;

extern int 
	aes_version,
	xe_mbshift;

int 
	*icon_data, 
	n_icons;	/* number of icons in the icons file */

WINDOW 
	*desk_window;

INAME 
	iname;

XDINFO 
	icd_info;

static ITMTYPE 
	icd_itm_type, /* type of an item entered by name in the dialog */
	icd_tgt_type;

static void 
	**svicntree, 
	*svicnrshdr;

int
	sl_noop; /* if 1, do not open icon dialog- it is already open */

int arrow_form_do ( XDINFO *info, int *oldbutton ); 
static int dsk_hndlkey(WINDOW *w, int dummy_scancode, int dummy_keystate);
static void dsk_hndlbutton(WINDOW *w, int x, int y, int n, int button_state, int keystate);
static void dsk_top(WINDOW *w);
static void incr_pos(int *x, int *y);
static void dsk_diskicons(int *x, int *y, int ic, char *name);
static int chng_icon(int object);


static WD_FUNC dsk_functions =
{
	dsk_hndlkey,
	wd_hndlbutton,
	0L,				/* redraw */
	0L,				/* topped */
	0L,				/* bottomed */
	0L,				/* newtop */
	0L,				/* closed */
	0L,				/* fulled */
	0L,				/* arrowed */
	0L,				/* hslid */
	0L,				/* vslid */
	0L,				/* sized */
	0L,				/* moved */
	0L,				/* hndlmenu */
	0L,				/* top */
	0L,				/* iconify */
	0L 				/* uniconify */
};

static int icn_find(WINDOW *w, int x, int y);
static boolean icn_state(WINDOW *w, int icon);
static ITMTYPE icn_type(WINDOW *w, int icon);
static ITMTYPE icn_tgttype(WINDOW *w, int icon);
static int icn_icon(WINDOW *w, int icon);
static char *icn_name(WINDOW *w, int icon);
static char *icn_fullname(WINDOW *w, int icon);
static int icn_attrib(WINDOW *w, int icon, int mode, XATTR *attrib);
static boolean icn_islink(WINDOW *w, int icon);
static boolean icn_open(WINDOW *w, int item, int kstate);
static boolean icn_copy(WINDOW *dw, int dobject, WINDOW *sw, int n, int *list, ICND *icns, int x, int y, int kstate);
static boolean icn_userdef(WINDOW *dw, int dobject, WINDOW *sw, int n, int *list, int kstate);
static void icn_select(WINDOW *w, int selected, int mode, boolean draw);
static void icn_rselect(WINDOW *w, int x, int y);
static void icn_nselected(WINDOW *w, int *n, int *sel);
static boolean icn_xlist(WINDOW *w, int *nselected, int *nvisible, int **sel_list, ICND **icnlist, int mx, int my);
static boolean icn_list(WINDOW *w, int *nselected, int **sel_list);
static void icn_set_update(WINDOW *w, wd_upd_type type, const char *fname1, const char *fname2);
static void icn_do_update(WINDOW *w);

static ITMFUNC itm_func =
{
	icn_find,					/* itm_find */
	icn_state,					/* itm_state */
	icn_type,					/* itm_type */
	icn_tgttype,				/* target object type */
	icn_icon,					/* itm_icon */
	icn_name,					/* itm_name */
	icn_fullname,				/* itm_fullname */
	icn_attrib,					/* itm_attrib */
	icn_islink,					/* itm_islink */
	icn_open,					/* itm_open */
	icn_copy,					/* itm_copy */
	icn_select,					/* itm_select */
	icn_rselect,				/* itm_rselect */
	icn_xlist,					/* itm_xlist */
	icn_list,					/* itm_list */
	0L,							/* wd_path */
	icn_set_update,				/* wd_set_update */
	icn_do_update,				/* wd_do_update */
	0L							/* wd_seticons */
};


/*
 * Mouse form (like a small icon) for placing a desktop icon
 */

static const MFORM icnm =
{
 8, 8, 1, 1, 0,
 0x1FF8, 0x1008, 0x1008, 0x1008, 0x1008, 0x1008, 0x1008, 0x1008,
 0x1008, 0x1008, 0x1008, 0x1008, 0xF00F, 0x8001, 0x8001, 0xFFFF,
 0x0000, 0x0FF0, 0x0FF0, 0x0FF0, 0x0FF0, 0x0FF0, 0x0FF0, 0x0FF0,
 0x0FF0, 0x0FF0, 0x0FF0, 0x0FF0, 0x0FF0, 0x7FFE, 0x7FFE, 0x0000
};


/*
 * Funktie voor het opnieuw tekenen van de desktop.
 */

static void form_dialall(int what)
{
	form_dial(what, 0, 0, 0, 0, 
	screen_info.dsk.x, 
	screen_info.dsk.y, 
	screen_info.dsk.w, 
	screen_info.dsk.h);
}


void dsk_draw(void)
{
	form_dialall(FMD_START);
	form_dialall(FMD_FINISH);
}


/*
 * Determine whether an icon item is a file/folder/program/link or not
 */

boolean isfile(ITMTYPE type)
{
	if 
	(
		(type == ITM_FOLDER)  || 
		(type == ITM_FILE)    || 
		(type == ITM_PROGRAM) || 
		(type == ITM_LINK) 
	)
		return TRUE;

	return FALSE;
}


/*
 * Similar to above, but include network objects
 */

static boolean isfilenet(ITMTYPE type)
{
	if(isfile(type) || type == ITM_NETOB)
		return TRUE;

	return FALSE;
}


/*
 * Set desktop icon type correctly for executable files (programs)
 * and also for network objects.
 * This is needed after changes in the list of executable filetypes,
 * or after a link is made to reference a network object.
 * Also used to fix compatibility issues for configuration files
 * earlier than V3.60
 */

void icn_fix_ictype(void)
{
	int i;
	ICON *ic = desk_icons;

	for( i = 0; i < max_icons; i++ )
	{
		/* Is this either ITM_FILE or ITM_PROGRAM (not a folder) ? */

		if(isfileprog(ic->item_type))
		{
			char *tgtname = x_fllink((char *)(ic->icon_dat.name));

			/* Is this or the target item an executable file type ? */

#if _MINT_
			if(ic->link && x_netob(tgtname))
			{
				ic->item_type = ITM_FILE;
				ic->tgt_type = ITM_NETOB;
			}
			else 
#endif
			if(prg_isprogram(fn_get_name(tgtname)))
			{
				ic->tgt_type = ITM_PROGRAM;
#if _MINT_
				if ( !ic->link )
#endif
					ic->item_type = ITM_PROGRAM;
			}
			else
			{
				/* Maybe this was a program once, but now is just a file */

				ic->item_type = ITM_FILE;
				ic->tgt_type = ITM_FILE;
			}

			free(tgtname);
		}

		ic++;
	}
}


/* 
 * Redraw icons which may be partially obscured by other objects
 */

void draw_icrects( WINDOW *w, OBJECT *tree, RECT *r1)
{
	RECT r2, d;

	xw_getfirst(w, &r2);
	while (r2.w != 0 && r2.h != 0)
	{
		if (xd_rcintersect(r1, &r2, &d))
			objc_draw(tree, ROOT, MAX_DEPTH, d.x, d.y, d.w, d.h);
		xw_getnext(w, &r2);
	}
}


#define WF_OWNER	20

/*
 * Redraw part of the desktop.
 */

static void redraw_desk(RECT *r1)
{
	int owner;
	boolean redraw = TRUE;

	wind_update(BEG_UPDATE);

	/* In newer (multitasking) systems, redraw only what is owned by the desktop ? */

	/* Should this perhaps be 399 or 340 instead of 400 ? (i.e. Magic & Falcon TOS) */

	if ( aes_version >= 0x399)
	{
		xw_get(NULL, WF_OWNER, &owner);
		if (ap_id != owner)
			redraw = FALSE;
	}

	/* Actually redraw... */

	if (redraw)
		draw_icrects( NULL, desktop, r1);

	wind_update(END_UPDATE);
}


/* 
 * Wis een icoon van het beeldscherm. Deze routine wist
 * niet de informatie van het icoon. Deze funktie is nodig als
 * een icoon veranderd is. 
 * This routine in fact -hides- an icon object and redraws part of
 * the desktop in the location of the icon.
 */

static void erase_icon(int object)
{
	RECT r;
	int o1 = object + 1;

	xd_objrect(desktop, o1, &r);
	desktop[o1].ob_flags = HIDETREE;
	redraw_desk(&r);
	desktop[o1].ob_flags = NORMAL;
}


/*
 * Draw an icon (i.e. draw part of the desk within the rectangle)
 */

static void draw_icon(int object)
{
	RECT r;

	/* Find location of the required icon, then draw it */

	xd_objrect(desktop, object + 1, &r);
	redraw_desk(&r);
}


/*
 * Combine the two above.
 * Note: currently, this will redraw the rectangle twice!
 */

static void redraw_icon(int object)
{
	erase_icon(object);
	draw_icon(object);
}


/* 
 * routines voor selecteren/deselecteren van iconen
 * mode = 0 : deselect all, select selected
 *        1 : toggle selected
 *        2 : deselect selected
 *        3 : select selected
 */ 

static void dsk_setnws(void)
{
	int i;
	ICON *icn = desk_icons;

	for (i = 0; i < max_icons; i++)
	{
		if (icn->item_type != ITM_NOTUSED)
			icn->selected = icn->newstate;

		icn++;
	}
}


/*
 * Draw/redraw selected or deselected icons, i.e. those the state of which 
 * has changed. In order to improve speed, a summary surrounding rectangle
 * for all changed icons is computed and then all icons are redrawn
 * by one call to objc_draw, without redrawing the background. 
 * In case of colour icons, which may be animated and change their outline,
 * this is not satisfactory. In that case, draw each icon separately
 * including the background- which is noticeably slower.
 */

static void dsk_drawsel(void)
{
	int 
		i;						/* object counter */ 

	RECT 
		c, 						/* icon object rectangle */
		r = {-1, -1, -1, -1};	/* summary rectangle */

	ICON *icn = desk_icons;

	desktop[0].ob_type = G_IBOX;

	for (i = 0; i < max_icons; i++)
	{
		if (icn->item_type != ITM_NOTUSED)
		{
			if (icn->selected == icn->newstate)
				/* State has not changed; hide the icon temporarily */
				obj_hide(desktop[i + 1]);
			else
			{
				/* State has changed; draw the icon */

				desktop[i + 1].ob_state = (icn->newstate) ? SELECTED : NORMAL;
				xd_objrect(desktop, i + 1, &c);

				if ( colour_icons )
				{
					/* Draw each colour icon separately, -with background- */

					desktop[0].ob_type = G_BOX;
					redraw_desk(&c);
				}
				else
				{
					/* For monochrome, update surrounding rectangle for all icons */

					if ((c.x < r.x) || (r.x < 0)) /* left edge minimum  */
						r.x = c.x;
					if ((c.y < r.y) || (r.y < 0)) /* upper edge minimum */
						r.y = c.y;
					c.x += desktop[i + 1].r.w - 1;
					c.y += desktop[i + 1].r.h - 1;
					if (c.x > r.w)                /* right edge maximum */
						r.w = c.x;
					if (c.y > r.h)                /* lower edge maximum */
						r.h = c.y;
				}
			}
		}

		icn++;
	}

	/* This will happen only for mono icons */

	if ( r.x >= 0 )
	{
		r.w -= r.x + 1;
		r.h -= r.y + 1;
		redraw_desk(&r);
	}

	/* Set temporarily hidden icons to visible again */

	icn = desk_icons;

	for (i = 0; i < max_icons; i++)
	{
		if (icn->selected == icn->newstate)
			obj_unhide(desktop[i + 1]);

		icn++;
	}

	desktop[0].ob_type = G_BOX;
}


/*
 * Compute rubberbox rectangle. Beware of asymmetrical wind_update()
 */

void rubber_rect(int x1, int x2, int y1, int y2, RECT *r)
{
	int h;

	arrow_mouse();
	wind_update(END_MCTRL);

	if ((h = x2 - x1) < 0)
	{
		r->x = x2;
		r->w = -h + 1;
	}
	else
	{
		r->x = x1;
		r->w = h + 1;
	}

	if ((h = y2 - y1) < 0)
	{
		r->y = y2;
		r->h = -h + 1;
	}
	else
	{
		r->y = y1;
		r->h = h + 1;
	}
}


/*
 * A size-saving aux. function for setting-up rubberbox drawing
 * Beware of asymmetrical wind_update()
 */

void start_rubberbox(void)
{
	set_rect_default();
	wind_update(BEG_MCTRL);
	graf_mouse(POINT_HAND, NULL);
}


/*
 * Rubberbox function 
 */

static void rubber_box(int x, int y, RECT *r)
{
	int 
		x1, x2, ox, y1, y2, oy, kstate;

	boolean 
		released;

	x1 = x2 = ox = x;
	y1 = y2 = oy = y;

	start_rubberbox();
	draw_rect(x1, y1, x2, y2);

	/* Loop until mouse button is released */

	do
	{
		released = xe_mouse_event(0, &x2, &y2, &kstate);

		/* Menu bar is inaccessible */

		if (y2 < screen_info.dsk.y)
			y2 = screen_info.dsk.y;

		if (released || (x2 != ox) || (y2 != oy))
		{
			draw_rect(x1, y1, ox, oy);
			if (!released)
			{
				draw_rect(x1, y1, x2, y2);
				ox = x2;
				oy = y2;
			}
		}
	}
	while (!released);

	rubber_rect(x1, x2, y1, y2, r);
}


/*
 * Change the selection state of some items
 * mode = 0: select selected and deselect the rest.
 * mode = 1: invert the status of selected.
 * mode = 2: deselect selected.
 * mode = 3: select selected.
 * mode = 4: select all (maybe)
 * External consideration is needed for mode 4
 */

void changestate(int mode, boolean *newstate, int i, int selected, boolean iselected, boolean iselected4)
{
	switch(mode)
	{
		case 0:
			*newstate = (i == selected) ? TRUE : FALSE;
			break;
		case 1:
			*newstate = iselected;
			break;
		case 2:
			*newstate = (i == selected) ? FALSE : iselected;
			break;
		case 3:
			*newstate = (i == selected) ? TRUE : iselected;
			break;
		case 4:
			*newstate = iselected4;
			break;
	}
}


/*
 * Externe funkties voor iconen.
 */

static void icn_select(WINDOW *w, int selected, int mode, boolean draw)
{
	int i;

	ICON *icn = desk_icons;

	/* Newstates after the switch below is equal to selection state of all icons */

	for (i = 0; i < max_icons; i++)
	{
		if (icn->item_type != ITM_NOTUSED)
			changestate(mode, &(icn->newstate), i, selected, icn->selected, TRUE);
		icn++;
	}

	if (mode == 1)
		desk_icons[selected].newstate = (desk_icons[selected].selected) ? FALSE : TRUE;

	if (draw)
		dsk_drawsel();

	dsk_setnws();
}


static void icn_rselect(WINDOW *w, int x, int y)
{
	int i;
	RECT r1, r2;
	ICONBLK *monoblk;
	boolean sel;
	OBJECT *deskto;

	ICON *icn = desk_icons;

	rubber_box(x, y, &r1);

	for (i = 0; i < max_icons; i++)
	{
		sel = icn->selected;
		deskto = &desktop[i + 1];

		if (icn->item_type != ITM_NOTUSED)
		{
			monoblk = &(deskto->ob_spec.ciconblk->monoblk);

			r2.x = desktop[0].r.x + deskto->r.x + monoblk->ic.x;
			r2.y = desktop[0].r.y + deskto->r.y + monoblk->ic.y;
			r2.w = monoblk->ic.w;
			r2.h = monoblk->ic.h;

			if (rc_intersect2(&r1, &r2))
				icn->newstate = !sel;
			else
			{
				r2.x = desktop[0].r.x + deskto->r.x + monoblk->tx.x;
				r2.y = desktop[0].r.y + deskto->r.y + monoblk->tx.y;
				r2.w = monoblk->tx.w;
				r2.h = monoblk->tx.h;
				if (rc_intersect2(&r1, &r2))
					icn->newstate = !sel;
				else
					icn->newstate = sel;
			}
		}

		icn++;
	}

	dsk_drawsel();
	dsk_setnws();
}


/*
 * Count the selected desktop icons
 */

static void icn_nselected(WINDOW *w, int *n, int *sel)
{
	int i, s, c = 0;

	for (i = 0; i < max_icons; i++)
	{
		if ((desk_icons[i].item_type != ITM_NOTUSED) && desk_icons[i].selected )
		{
			c++;
			s = i;
		}
	}

	*n = c;
	*sel = (c == 1) ? s : -1;
}


/*
 * Note: WINDOW *w parameter is not used in the following functions 
 * but must exist because these are ITMFUNCs
 */

static boolean icn_state(WINDOW *w, int icon)
{
	return desk_icons[icon].selected;
}


/* 
 * Return type of a desktop icon item 
 */

static ITMTYPE icn_type(WINDOW *w, int icon)
{
	return desk_icons[icon].item_type;
}


/*
 * Return type of the target object of a desktop icon
 */

static ITMTYPE icn_tgttype(WINDOW *w, int icon)
{
	return desk_icons[icon].tgt_type;
}


static int icn_icon(WINDOW *w, int icon)
{
	return desk_icons[icon].icon_index;
}


/*
 * Return pointer to the name of a (desktop) object
 */

static char *icn_name(WINDOW *w, int icon)
{
	ICON *icn = &desk_icons[icon];

	/* 
	 * "name" must exist after return from this function and be 
	 * sufficiently long to accomodate string MDRVNAME + drive letter
	 */
	
	static char name[20]; 

	if (isfile(icn->item_type))
		/* this is a file, folder, program or link- treat as a file(name) */
		return fn_get_name(icn->icon_dat.name);
	else
	{
		if (icn->item_type == ITM_DRIVE)
		{
			/* this is a disk volume. Return text "drive X" */

			sprintf(name, get_freestring(MDRVNAME), (char)icn->icon_dat.drv);
			return name;
		}
		else if(icn->item_type == ITM_NETOB)
			return (char *)(icn->icon_dat.name);
	}

	return NULL;
}


/*
 * Return the identification of the icon for an object given by name.
 * This routine is used when assigning window icons from desktop ones.
 */

int icn_iconid(const char *name)
{
	int i;
	ICON *icn = desk_icons;

	if(name)
	{
		for( i = 0; i < max_icons; i++ )
		{
			if( isfile(icn->item_type) )
			{
				if( strcmp(name, icn->icon_dat.name) == 0 )
					return icn->icon_index;
			}

			icn++;
		}
	}

	return -1;
}


/*
 * Get fullname of an object to which the icon has been assigned.
 * When an item can not have a name, or there is an error, return NULL
 * (i.e. only files, folders, programs and disk volumes can have fullnames).
 * This routine allocates space for the name. 
 */

static char *icn_fullname(WINDOW *w, int icon)
{
	ICON *icn = &desk_icons[icon];
	char *s;

	if (icn->item_type == ITM_DRIVE)
	{
		if ((s = strdup(adrive)) != NULL)
			s[0] = icn->icon_dat.drv;
	}
	else
		s = (isfilenet(icn->item_type)) ? strdup(icn->icon_dat.name) : NULL;

	return s;
}


/*
 * Get attributes of an object to which a desktop icon has been assigned
 */

static int icn_attrib(WINDOW *w, int icon, int mode, XATTR *attr)
{
	ICON *icn = &desk_icons[icon];

	if (isfile(icn->item_type))
		return (int)x_attr(mode, FS_INQ, icn->icon_dat.name, attr);

	return 0;
}


/*
 * Does a desktop icon represent a link?
 */

static boolean icn_islink(WINDOW *w, int icon)
{
	return desk_icons[icon].link;
}


void icn_coords(int *coords, RECT *tr, RECT *ir)
{
	int *icndcoords = coords;

	*icndcoords++ = tr->x;
	*icndcoords++ = tr->y;
	*icndcoords++ = ir->x;
	*icndcoords++ = tr->y;
	*icndcoords++ = ir->x;
	*icndcoords++ = ir->y;
	*icndcoords++ = ir->x + ir->w;
	*icndcoords++ = ir->y;
	*icndcoords++ = coords[6] /* ir.x + ir.w */ ;
	*icndcoords++ = tr->y;
	*icndcoords++ = tr->x + tr->w;
	*icndcoords++ = tr->y;
	*icndcoords++ = coords[10] /* tr.x + tr.w */ ;
	*icndcoords++ = tr->y + tr->h;
	*icndcoords++ = tr->x;
	*icndcoords++ = coords[13] /* tr.y + tr.h */ ;
	*icndcoords++ = tr->x;
	*icndcoords   = tr->y + 1;
}



static void get_icnd(int object, ICND *icnd, int mx, int my)
{
	RECT tr, ir;
	ICONBLK *h;		
	OBJECT *desktopobj = &desktop[object + 1];

	icnd->item = object;
	icnd->np = 9;
	icnd->m_x = desktopobj->r.x + desktopobj->r.w / 2 - mx + screen_info.dsk.x;
	icnd->m_y = desktopobj->r.y + desktopobj->r.h / 2 - my + screen_info.dsk.y;

	h = &(desktopobj->ob_spec.ciconblk->monoblk);

	tr.x = desktopobj->r.x + h->tx.x - mx + screen_info.dsk.x;
	tr.y = desktopobj->r.y + h->tx.y - my + screen_info.dsk.y;
	tr.w = h->tx.w - 1;
	tr.h = h->tx.h - 1;
	ir.x = desktopobj->r.x + h->ic.x - mx + screen_info.dsk.x;
	ir.y = desktopobj->r.y + h->ic.y - my + screen_info.dsk.y;
	ir.w = h->ic.w - 1;

	icn_coords(icnd->coords, &tr, &ir);
}


/*
 * Create a list of selected icon items.
 * This routine will return FALSE and a NULL list pointer
 * if no list has been created. It can return FALSE even if
 * there are selected items (if list allocation failed).
 */

static boolean icn_list(WINDOW *w, int *nselected, int **sel_list)
{
	int i, j = 0, n = 0, *list;

	/* First count the selected items */

	for (i = 0; i < max_icons; i++)
		if ((desk_icons[i].item_type != ITM_NOTUSED) && desk_icons[i].selected )
			n++;

	/* If nothing is selected, return FALSE */

	if ((*nselected = n) == 0)
	{
		*sel_list = NULL;
		return FALSE;
	}

	/* Then create a list... */

	list = malloc_chk((long)n * sizeof(int));

	*sel_list = list;

	if (list)
	{
		for (i = 0; i < max_icons; i++)
			if ((desk_icons[i].item_type != ITM_NOTUSED) && desk_icons[i].selected )
				list[j++] = i;

		return TRUE;
	}

	return FALSE;	
}


/* 
 * Routine voor het maken van een lijst met geseleketeerde iconen. 
 */

static boolean icn_xlist(WINDOW *w, int *nsel, int *nvis, int **sel_list, ICND **icns, int mx, int my)
{
	int i, *list, n;
	ICND *icnlist;

	if (!icn_list(desk_window, nsel, sel_list))
		return FALSE;

	n = *nsel;
	*nvis = n;

	if (n == 0)
	{
		*icns = NULL;
		return TRUE;
	}

	icnlist = malloc_chk((long)n * sizeof(ICND));
	*icns = icnlist;

	if (icnlist)
	{
		list = *sel_list;

		for (i = 0; i < n; i++)
			get_icnd(list[i], &icnlist[i], mx, my);

		return TRUE;
	}

	free(*sel_list);
	return FALSE;

}


/*
 * Find the icon at position x,y in window w
 */

static int icn_find(WINDOW *w, int x, int y)
{
	int ox, oy, ox2, oy2, i, object = -1;
	ICONBLK *p;	
	OBJECT *de;


	objc_offset(desktop, 0, &ox, &oy);

	if ((i = desktop[0].ob_head) == -1)
		return -1;

	while (i != 0)
	{
		de = &desktop[i];

		if (de->ob_type == G_ICON || de->ob_type == G_CICON)
		{
			RECT h;

			ox2 = ox + de->r.x;
			oy2 = oy + de->r.y;
			p = &(de->ob_spec.ciconblk->monoblk);

			h.x = ox2 + p->ic.x;
			h.y = oy2 + p->ic.y;
			h.w = p->ic.w;
			h.h = p->ic.h;

			if (inrect(x, y, &h))
				object = i - 1;
			else
			{
				h.x = ox2 + p->tx.x;
				h.y = oy2 + p->tx.y;
				h.w = p->tx.w;
				h.h = p->tx.h;

				if (inrect(x, y, &h))
					object = i - 1;
			}
		}

		i = de->ob_next;
	}
	return object;
}


/********************************************************************
 *																	*
 * Funkties voor het verversen van de desktop.						*
 *																	*
 ********************************************************************/

/*
 * Remove a desktop icon
 */

void remove_icon(int object, boolean draw)
{
	ITMTYPE type = icn_type(desk_window, object);

	if (draw)
		erase_icon(object);


/* is this correct ? Maybe the other way round; see below

	objc_delete(desktop, object + 1);
	free(desktop[object + 1].ob_spec.ciconblk);	

*/

	free(desktop[object + 1].ob_spec.ciconblk);	
	objc_delete(desktop, object + 1);

	if (isfilenet(type))
		free(desk_icons[object].icon_dat.name);

	desk_icons[object].item_type = ITM_NOTUSED;
}


/*
 * Set icon name from the full name of the item.
 * Icon label is changed only if it reflects real item name,
 * otherwise, icon label is kept.
 */

static void icn_set_name(ICON *icn, char *newiname)
{
	/* If label is same as (old) item name, copy new name */

	if (!x_netob(icn->icon_dat.name) && strncmp(icn->label, fn_get_name(icn->icon_dat.name), 12) == 0)
		strsncpy(icn->label, fn_get_name(newiname), sizeof(icn->label));

	/* If name has changed, deallocate old name string, use new one */

	if(strcmp(icn->icon_dat.name, newiname) != 0)
	{
		free(icn->icon_dat.name);
		icn->icon_dat.name = newiname;
	}
	else
		free(newiname);
}


static void icn_set_update(WINDOW *w, wd_upd_type type, const char *fname1, const char *fname2)
{
	int i;
	ICON *icon = desk_icons;
	char *new;

	for (i = 0; i < max_icons; i++)
	{
		if (isfilenet(icon->item_type))
		{
			if (strcmp(icon->icon_dat.name, fname1) == 0)
			{
				if (type == WD_UPD_DELETED)
					icon->update = ICN_DELETE;

				if (type == WD_UPD_MOVED)
				{
					if (strcmp(fname2, icon->icon_dat.name) != 0)
					{
						if ((new = strdup(fname2)) == NULL)
							return;
						else
						{
							icon->update = ICN_REDRAW;
							icn_set_name(icon, new);
						}
					}
				}
			}
		}

		icon++;
	}
}


static void dsk_do_update(void)
{
	int i;
	ICON *icn = desk_icons;


	for (i = 0; i < max_icons; i++)
	{
		if (icn->item_type != ITM_NOTUSED)
		{
			if (icn->update == ICN_DELETE)
				remove_icon(i, TRUE);

			if (icn->update == ICN_REDRAW)
			{
				redraw_icon(i);
				icn->update = ICN_NO_UPDATE;
			}
		}

		icn++;
	}
}


/*
 * Item function, must have WINDOW * as argument
 */

static void icn_do_update(WINDOW *w)
{
	dsk_do_update();
}


/********************************************************************
 *																	*
 * Funktie voor het openen van iconen.								*
 *																	*
 ********************************************************************/

/*
 * Open an object represented by a desktop icon
 */

static boolean icn_open(WINDOW *w, int item, int kstate)
{
	ICON *icn = &desk_icons[item];
	ITMTYPE type = icn->item_type;


	if (isfilenet(type)) /* file, folder, program or link ? */
	{
		/* If the selected object can be opened... */

		int button;

		if (icn->tgt_type != ITM_NETOB && !x_exist(icn->icon_dat.name, EX_FILE | EX_DIR))
		{
			/* Object does not exist */

			if ((button = alert_printf(1, AAPPNFND, icn->label)) == 3)
				return FALSE;

			if (button == 2)
			{
				remove_icon(item, TRUE);
				return FALSE;
			}

			sl_noop = 0;
			button = chng_icon(item);
			xd_close(&icd_info);
			dsk_do_update();

			if(button != CHNICNOK)
				return FALSE;
		}
	}

	return item_open(w, item, kstate, NULL, NULL);
}


/*
 * Provide index of the default icon for the specified item type.
 * First try a default icon according to item type;
 * if that does not succeed, return index of the first icon.
 * Issue an alert that an icon is missing (but only if such an
 * alert has not been issued earlier).
 * String 'name' is retrieved internally but not used further.
 */

int default_icon(ITMTYPE type)
{
	int it, ic = 0;
	INAME name;

	if (!noicons)
	{
		alert_iprint(MICNFND);
		noicons = TRUE;
	}

	/* Choose appropriate name string, depending on item type */

	switch(type)
	{
		case ITM_FOLDER:
		case ITM_PREVDIR:
			it = FOINAME;
			break;
		case ITM_PROGRAM:
			it = APINAME;
			break;
		case ITM_LINK:
		case ITM_FILE:
		case ITM_NETOB:
			it = FIINAME;
			break;
		case ITM_TRASH:
			it = TRINAME;
			break;
		case ITM_PRINTER:
			it = PRINAME;
			break;
		default:
			it = 0;
	}

	/* 
	 * Retrieve icon index from icon name. If icon with the specified
	 * name is not found, return -1; if id is 0, return index
	 * of the first icon in the file
	 */

	ic = rsrc_icon_rscid(it, (char *)&name);

	/* Beware; this can return -1 */

	return ic;
}


/*
 * Return index of an icon given by its name in the resource file.
 * If icon with the specified name is not found, return -1.
 * If name is NULL, return index of the first icon in the file.
 */

int rsrc_icon(const char *name)
{
	int i = 0;
	CICONBLK *h;

	do
	{
		OBJECT *ic = icons + i;
		if (ic->ob_type == G_ICON || ic->ob_type == G_CICON)
		{
			if (!name)
				return i;
			h = ic->ob_spec.ciconblk;
			if (strnicmp(h->monoblk.ib_ptext, name, 12) == 0)
				return i;
		}
	}
	while ((icons[i++].ob_flags & LASTOB) == 0);

	return -1;
}


/* 
 * This routine gets icon id and name from language-dependent name as 
 * defined in desktop.rsc. The routine is used for some basic icon types 
 * which should always exist in the icon resource file (floppy, disk, file, 
 * folder, etc)
 *  
 * id           = index of icon name string in DESKTOP.RSC (e.g. HDINAME, FINAME...)
 * name         = returned icon name from DESKTOP.RSC
 * return value = index of icon in ICONS.RSC 
 *
 * If id is 0, or an icon with the name can not be found,
 * index of the first icon is returned. It is assumed that there is
 * at least one icon in the file.
 * 
 */

int rsrc_icon_rscid ( int id, char *name )
{
	char *nnn = NULL;
	int ic;

	if (id)
		nnn = get_freestring(id);

	ic = rsrc_icon(nnn);
	if ( ic < 0 )
		ic = rsrc_icon(NULL);

	strsncpy(name, icons[ic].ob_spec.ciconblk->monoblk.ib_ptext, sizeof(INAME));

	return ic;
}


/*
 * Find disk volume label in order to insert it as icon label
 * if that is not specified. Don't do that for floppies, as most likely
 * they will not be in the drive. Beware that this may cause problems with
 * other ejectable disks too.
 * Note A:\ = drive 0, B:\ = drive 1, etc.
 */

static void icn_disklabel(int drive, char *ilabel)
{
	INAME vlabel;

	if(drive > 1 && *ilabel == 0 && x_getlabel(drive, vlabel) == 0)
		strcpy(ilabel, vlabel);
} 


/* 
 * An aux. size-reducing function which initializes some object data
 * mostly for icon objects.
 */

void init_obj(OBJECT *obj, int otype)
{
	obj->ob_next = -1;
	obj->ob_head = -1;
	obj->ob_tail = -1;
	obj->ob_flags = NONE;
	obj->ob_state = NORMAL;
	obj->ob_type = otype;
	obj->r.w = ICON_W;
	obj->r.h = ICON_H;
}


/*
 * Add an icon onto the desktop; return index of the icon in desk_icons[].
 * Note1: in case of an error, fname will be deallocated.
 * Note2: see similar object-setting code in set_obji() in windows.c
 */

static int add_icon(ITMTYPE type, ITMTYPE tgttype, boolean link, int icon, const char *text, 
                    int ch, int x, int y, boolean draw, const char *fname)
{
	int 
		i = 0,
		ix, iy, 
		icon_no;

	CICONBLK 
		*h = NULL;			/* ciconblk (the largest) */
	
	ICON 
		*icn;

	OBJECT 
		*deskto;


	/* Find the first unused slot in desktop icons */

	while ((desk_icons[i].item_type != ITM_NOTUSED) && (i < max_icons - 1))
		i++;

	icn = &desk_icons[i]; /* pointer to a slot in icons list */
	deskto = &desktop[i + 1]; /* pointer to a desktop object */

	/* Check for some errors */

	if(icon < 0)
		goto errexit;

	if (icn->item_type != ITM_NOTUSED)
	{
		alert_iprint(MTMICONS); 
		goto errexit;
	}

	if ((h = malloc_chk(sizeof(CICONBLK))) == NULL)		/* ciconblk (the largest) */
	{
		icn->item_type = ITM_NOTUSED;
		goto errexit;
	}

	icon_no = min(n_icons - 1, icon);

	/* Put the icon there */

	ix = min(x, m_icnx);
	iy = min(y, m_icny);

	icn->x = ix;
	icn->y = iy;

	icn->tgt_type = tgttype;
	icn->link = link;
	icn->item_type = (link) ? ITM_FILE : type;
	icn->icon_index = icon_no;
	icn->selected = FALSE;
	icn->update = ICN_NO_UPDATE;

	init_obj(deskto, icons[icon_no].ob_type);
	if(link)
		deskto->ob_state |= CHECKED;

/* better not until background is correctly handled in AESes
	if (isfile(type) && x_attr(1, FS_INQ, fname, &attr) >= 0 && ((attr.attr & FA_HIDDEN) != 0 ))
		deskto->ob_state |= DISABLED;
*/

	deskto->r.x = ix * ICON_W;
	deskto->r.y = iy * ICON_H;
	deskto->ob_spec.ciconblk = h;
	*h = *icons[icon_no].ob_spec.ciconblk;

	strsncpy(icn->label, text, sizeof(INAME));	 
	strsncpy(icn->icon_name, icons[icon_no].ob_spec.ciconblk->monoblk.ib_ptext, sizeof(INAME));	

	h->monoblk.ib_char &= 0xFF00;
	h->monoblk.ib_ptext = icn->label;

	icn->icon_dat.name = NULL;
	icn->icon_dat.drv = 0;

	switch (type) 
	{
		case ITM_DRIVE:
		{
			icn->icon_dat.drv = ch;
			h->monoblk.ib_char |= ch;
			break;
		}
		case ITM_FOLDER:
		case ITM_PROGRAM:
		case ITM_FILE:
		case ITM_NETOB:
		{
			icn->icon_dat.name = fname;
		}
		case ITM_TRASH:
		case ITM_PRINTER:
		{
			h->monoblk.ib_char |= 0x20;
			break;
		}
	}

	objc_add(desktop, 0, i + 1);

	if (draw)
		draw_icon(i);

	return i;

	/* In case of an error, deallocate structures and exit */

	errexit:;

	if (isfilenet(type))
		free(fname);

	free(h);

	return -1;

}


/*
 * A shorter form of add_icon() mostly to be used for devices
 * which can not be links and do not have a filename attached.
 * Return index of this icon in desk_icons[]
 */

static int add_devicon(ITMTYPE type, int icon, const char *text, int ch, int x, int y)
{
	return add_icon(type, type, FALSE, icon, text, ch, x, y, TRUE, NULL);
}


/*
 * Compute (round-up) position of an icon on the desktop
 */

static void comp_icnxy(int mx, int my, int *x, int *y)
{
	/* Note: do not use min() here, this is shorter */

	*x = (mx - screen_info.dsk.x) / ICON_W;
	if (*x > m_icnx)
		*x = m_icnx;
	*y = (my - screen_info.dsk.y) / ICON_H;
	if (*y > m_icny)
		*y = m_icny;
}


static void get_iconpos(int *x, int *y)
{
	int dummy, mx, my;

	wind_update(BEG_MCTRL);
	graf_mouse(USER_DEF, &icnm);
	evnt_button(1, 1, 1, &mx, &my, &dummy, &dummy);
	arrow_mouse();
	wind_update(END_MCTRL);
	comp_icnxy(mx, my, x, y);
}


/* 
 * Get type of desktop icon (trashcan/printer/drive) from the dialog
 */

static ITMTYPE get_icntype(void)
{
	int object = xd_get_rbutton(addicon, ICBTNS);

	switch (object)
	{
		case ADISK:
			return ITM_DRIVE;
		case APRINTER:
			return ITM_PRINTER;
		case ATRASH:
			return ITM_TRASH;
		default:
			return ITM_FILE;
	}
}


/*
 * Enter desk icon type (trash/printer/drive) into dialog (set radio button).
 * Return start object for dialog redraw.
 */

static int set_icntype(ITMTYPE type)
{
	int startobj, object;

	switch (type)
	{
		case ITM_DRIVE:
			object = ADISK;
			startobj = DRIVEID;
			break;
		case ITM_PRINTER:
			object = APRINTER;
			startobj = ICNLABEL;
			break;
		case ITM_TRASH:
			object = ATRASH;
			startobj = ICNLABEL;
			break;
		default:
			object = AFILE;
			startobj = IFNAME;
			break;
	}

	xd_set_rbutton(addicon, ICBTNS, object);
	return startobj;
}


/*
 * Initiate the icon-selector slider
 */

void icn_sl_init(int line, SLIDER *sl)
{
	sl->type = 0;
	sl->up_arrow = ICNUP;
	sl->down_arrow = ICNDWN;
	sl->slider = ICSLIDER;	/* slider object                        */
	sl->sparent = ICPARENT;	/* slider parent (background) object    */
	sl->lines = 1;			/* number of visible lines in the box   */
	sl->n = n_icons;		/* number of items in the list          */
	sl->line = line;		/* index of first item shown in the box */
	sl->list = NULL;		/* pointer to list of items to be shown */
	sl->set_selector = set_iselector;
	sl->first = 0;			/* first object in the scrolled box     */
	sl->findsel = 0L;

	/* note: sl_init() could come here ... */

	addicon[ICONDATA].ob_state &= ~SELECTED;
}


void set_iselector(SLIDER *slider, boolean draw, XDINFO *info)
{
	OBJECT *h1, *ic = icons + slider->line;

	h1 = addicon + ICONDATA;
	h1->ob_type = ic->ob_type;
	h1->ob_spec = ic->ob_spec;

	/* Center the icon image in the background box */

	h1->r.x = (addicon[ICONBACK].r.w - ic->r.w ) / 2;
	h1->r.y = (addicon[ICONBACK].r.h - ic->r.h ) / 2;
	h1->r.w = ic->r.w;
	h1->r.h = ic->r.h;

	/* 
	 * In low resolutions, move the icon object for a little bit,  
	 * otherwise it goes out of the background box (why?)
	 */

	if(screen_info.dsk.h < 300)
		h1->r.y -= 8;

	if (draw)
		xd_draw(info, ICONBACK, 1);
}


/*
 * Currently, this routine is used only for setting icons.
 * If sl_noop == 1, the dialog will not be opened (assuming already open).
 * This routine may open, but never closes the icon selector dialog.
 */

int icn_dialog(SLIDER *sl_info, int *icon_no, int startobj, int bckpatt, int bckcol)
{
	int 
		rdro = ROOT,
		button;

	boolean
		again = FALSE;

	static const char 
		so[] = {DRIVEID, ICNLABEL, ICNLABEL, IFNAME};

	/* Background colour and pattern */

	addicon[ICONBACK].ob_spec.obspec.fillpattern = bckpatt;
	addicon[ICONBACK].ob_spec.obspec.interiorcol = bckcol;

	/* Initialize slider */

	icn_sl_init(*icon_no, sl_info);
	sl_init(addicon, sl_info);

	/* Loop until told to stop... */

	do
	{
		switch(startobj)
		{
			case DRIVEID:
				obj_unhide(addicon[DRIVEID]);
				goto setmore;
			case ICNLABEL:
			case ICNTYPE:
				obj_hide(addicon[DRIVEID]);
				setmore:;
				obj_hide(addicon[INAMBOX]);
				break;
			case IFNAME:
				obj_unhide(addicon[INAMBOX]);
				obj_hide(addicon[DRIVEID]);
				xd_init_shift(&addicon[IFNAME], dirname);
		}

		if ( !again && sl_noop == 0 )
			xd_open(addicon, &icd_info);
		else
			xd_drawdeep(&icd_info, rdro);

		sl_noop = 1;

		button = sl_form_do(addicon, startobj, sl_info, &icd_info) & 0x7FFF;

		again = FALSE;

		/* The following if-thens yield smaller code than a case structure */

		if(button >= ADISK && button <= AFILE)
		{
			xd_set_rbutton(addicon, ICBTNS, button);
			startobj = (int)so[button - ADISK];
			rdro = ROOT; 
			*iconlabel = 0;
			again = TRUE;
		}
		else if (button == ICONDATA)
		{
			rdro = ICONBACK;
			again = TRUE;
		}
		else if( button == CHNICNOK && *dirname )
		{
			icd_islink = FALSE;
			icd_itm_type  = diritem_type(dirname);
			icd_tgt_type = icd_itm_type;
#if _MINT_
			if(mint && (icd_itm_type != ITM_NOTUSED))
			{
				VLNAME tgtname;
				if(x_rdlink(sizeof(VLNAME), (char *)(&tgtname), (const char *)dirname) == 0)
				{
					icd_islink = TRUE;
					icd_tgt_type = diritem_type(tgtname);
				}
			}
#endif
			if(icd_tgt_type == ITM_NOTUSED)
			{
				xd_buttnorm(&icd_info, button);
				again = TRUE; 
				rdro = ROOT;
			}			
		}
	}
	while( again );

	xd_buttnorm(&icd_info, button);

	*icon_no = sl_info->line;
	return button;
}


/* 
 * Aux. size-optimization function; hide or unhide some dialog objects
 */

static void icn_hidesome(void)
{
	static const int 
		items[] = {ICSHFIL,ICSHFLD,ICSHPRG,ICNTYPE,ICNTYPT,DRIVEID,ADDBUTT,0};

	rsc_hidemany(addicon, items);
	obj_unhide(addicon[CHNBUTT]);
	obj_unhide(addicon[ICNLABEL]);	
	obj_unhide(addicon[ICBTNS]);
	xd_set_child(addicon, ICBTNS, 0);
}


/* 
 * Manually install one or more desktop icons
 */

void dsk_insticon(WINDOW *w, int n, int *list)
{
	int 
		x, 
		y, 
		startobj,
		icnind,
		icon_no,
		nn = n,
		i = 0,
		button = 0; 

	ITMTYPE 
		itype;

	SLIDER 
		sl_info;

	char
		*name,
		*nameonly,
		*ifname = NULL;

	get_iconpos(&x, &y);
	rsc_title(addicon, AITITLE, DTADDICN);
	icn_hidesome();
	obj_hide(addicon[CHICNRM]);

	if(n == 0)
		nn = 1;

	sl_noop = 0;

	options.cprefs &= ~CF_FOLL;

	while( (i < nn) && (button != CHNICNAB) )
	{
		*drvid = 0;
		*iconlabel = 0;
		icd_islink = FALSE;
		name = NULL;

		if(n > 0)
		{
			ITMTYPE ttype;
			boolean link;
			int item;

			if((item = list[i]) < 0)
				break;

#if _MINT_
			reread:;

			if(mint)
			{
				set_opt(addicon, options.cprefs, CF_FOLL, IFOLLNK);
				obj_unhide(addicon[IFOLLNK]);
			}
#endif
			ttype = itm_tgttype(w, item);

			itm_follow(w, item, &link, (char **)(&name), &itype);

			if(name == NULL)
			{
				xform_error(ENSMEM);
				break; /* go to update */
			}
			else
			{
 				nameonly = fn_get_name(name);
				cramped_name(nameonly, iconlabel, (int)sizeof(INAME));
				icon_no = icnt_geticon( nameonly, itype, ttype );
				strsncpy(dirname, name, sizeof(VLNAME));
				button = AFILE;
				startobj = IFNAME;
				free(name); /* name is not needed anymore */
			}
		}
		else /* (n <= 0) */
		{
			*dirname = 0;
			xd_set_child(addicon, ICBTNS, 1);

			icon_no = rsrc_icon_rscid(HDINAME, iconlabel);
			button = ADISK;
			startobj = DRIVEID;
		}

		xd_set_rbutton(addicon, ICBTNS, button);

		button = icn_dialog(&sl_info, &icon_no, startobj, options.dsk_pattern, options.dsk_color);

#if_MINT_
		if(button == IFOLLNK)
		{
			/* 
			 * If state of the  'follow link' has changed, return to the
			 * beginning and redisplay the dialog.
			 */

			options.cprefs ^= CF_FOLL;
			goto reread;
		}
#endif

		if (button == CHNICNOK)
		{
			itype = get_icntype();

			if(itype == ITM_DRIVE)
			{
				if(*drvid < 'A')
				{
					dsk_diskicons(&x, &y, icon_no, iconlabel);
					break;
				}
				
				icn_disklabel((int)(*drvid - 'A'), iconlabel);
			}
			else
				*drvid = 0;

			if(itype == ITM_FILE) /* file or folder or program or link */
			{
				itype = icd_itm_type;
				ifname = strdup(dirname);
			}
			else
				icd_tgt_type = itype;

			icnind = add_icon
			(
				itype, 
				icd_tgt_type, 
				icd_islink, 
				icon_no, 
				iconlabel, 
				*drvid & 0x5F, 
				x, y, 
				FALSE,
				ifname /* deallocated inside if add_icon fails */
			);

			if(icnind < 0)
				break;
			desk_icons[icnind].update = ICN_REDRAW;
			incr_pos(&x, &y);
		}

		i++;

	} /* loop */

	xd_close(&icd_info);
	dsk_do_update();
	obj_unhide(addicon[CHICNRM]);
	obj_hide(addicon[IFOLLNK]);
}


/*
 * Size optimization function for confirming that an itemtype is
 * that of a trashcan or a printer. Return string index
 * corresponding to item type, or 0 if type is not a printer
 * or a trashcan.
 * Now this routine is extended to handle unknown objects and network objects.
 */

int trash_or_print(ITMTYPE type)
{
	switch(type)
	{
		case ITM_TRASH:
			return MTRASHCN;
		case ITM_PRINTER:
			return MPRINTER;
		case ITM_NOTUSED:
			return MUNKNOWN;
		case ITM_NETOB:
			return MNETOB;
		default:
			return 0;
	}
}


/*
 * Handle the dialog for editing a desk icon. 
 * Parameter 'object' is the index of the icon in desk_icons[]
 */

static int chng_icon(int object)
{
	int button = -1, icon_no, startobj;
	ICON *icn = &desk_icons[object];
	SLIDER sl_info;

	rsc_title(addicon, AITITLE, DTCHNICN);
	icn_hidesome();

	startobj = set_icntype(icn->item_type); /* set type into dialog  */

	if(startobj == IFNAME)
	{
		*drvid = 0;
		strsncpy(dirname, icn->icon_dat.name, sizeof(VLNAME));
	}
	else
		*drvid = (char)icn->icon_dat.drv;

	strcpy(iconlabel, icn->label);
	icon_no = icn->icon_index;

	/* Dialog for selecting icon and icon type */

	button = icn_dialog(&sl_info, &icon_no, startobj, options.dsk_pattern, options.dsk_color);

	if(button == CHICNRM) 
		icn->update  = ICN_DELETE;

	if (button == CHNICNOK)
	{
		CICONBLK *h = desktop[object + 1].ob_spec.ciconblk;		/* ciconblk (the largest) */

		icn->update = ICN_REDRAW;
		desktop[object + 1].ob_type = icons[icon_no].ob_type;	/* may change colours ;-) */
		icn->icon_index = icon_no;

		strsncpy(icn->label, iconlabel, sizeof(INAME));	
		strsncpy(icn->icon_name, icons[icon_no].ob_spec.ciconblk->monoblk.ib_ptext, sizeof(INAME));

		*h = *icons[icon_no].ob_spec.ciconblk;
		h->monoblk.ib_ptext = icn->label;
		h->monoblk.ib_char &= 0xFF00;

		icn->item_type = get_icntype(); /* get icontype from the dialog */

		if(startobj == DRIVEID)
		{
			if (icn->item_type != ITM_DRIVE)
				*drvid = 0;
			icn->icon_dat.drv = *drvid & 0x5F;
			h->monoblk.ib_char |= (int)(*drvid) & 0x5F;
		}
		else
		{
			h->monoblk.ib_char |= 0x20;

			if(startobj == IFNAME)
			{
				char *new;

				/* Note: must not clear the filename */

				if(*dirname && (new = strdup(dirname)) != NULL)
				{
					icn_set_name(icn, new);
					icn->item_type = icd_itm_type;
					icn->tgt_type = icd_tgt_type;
					icn->link = icd_islink;
				}
				else
					button = CHNICNAB;
			}
		}
	}

	return button;
}


/*
 * Move a number of icons to a new position on the desktop
 */

static void mv_icons(ICND *icns, int n, int mx, int my)
{
	int i, x, y, obj;

	for (i = 0; i < n; i++)
	{
		obj = icns[i].item;
		erase_icon(obj);
		x = (mx + icns[i].m_x - screen_info.dsk.x) / ICON_W;
		y = (my + icns[i].m_y - screen_info.dsk.y) / ICON_H;

		/* Note: do not use min() here, this is shorter */
		if ( x > m_icnx )
			x = m_icnx;
		if ( y > m_icny )
			y = m_icny;

		desk_icons[obj].x = x;
		desk_icons[obj].y = y;
		desktop[obj + 1].r.x = x * ICON_W;
		desktop[obj + 1].r.y = y * ICON_H;

		draw_icon(obj);
	}
}


/*
 * Install a desktop icon by moving a directory item to the desktop,
 * or manipulate desktop icons in other ways.
 */

static boolean icn_copy(WINDOW *dw, int dobject, WINDOW *sw, int n,
						int *list, ICND *icns, int x, int y, int kstate)
{
	int i;

	/* 
	 * A specific situation can arise when object are moved on the desktop:
	 * if selected icons are moved so that the cursor is, at the time 
	 * of button release, on one of the selected icons, a copy is attempted
	 * instead of icon movement. Code below cures this problem.
	 * An opportunity is taken to check if only icons should be
	 * removed or files and folders really deleted.
	 */

	if ( dobject > -1 && sw == desk_window && dw == desk_window )
	{
		if(itm_type(dw, dobject) == ITM_TRASH)
		{
			int b = alert_printf(1, AREMICNS);

			if(b == 1)
			{
				dsk_chngicon(n, list, FALSE);
				return TRUE;
			}
			else if (b == 3)
				return FALSE;			
		}

		for ( i = 0; i < n; i++ )
		{
			if ( list[i] == dobject )
			{
				dobject = -1;
				break;
			}
		}
	}

	/* Now, act accordingly */

	if (dobject != -1)

		/* Copy to an object */

		return item_copy(dw, dobject, sw, n, list, kstate);
	else
	{
		if (sw == desk_window)
		{
			/* Move the icons about on the desktop */

			mv_icons(icns, n, x, y);
			return FALSE;
		}
		else
		{
			/* Install desktop icons */

			int item, ix, iy, icon;
			ITMTYPE type, ttype;
			INAME tolabel;
			boolean link;
			const char *fname, *nameonly;

			comp_icnxy(x, y, &ix, &iy);

			/* 
			 * Note: currently it is not possible to follow links.
			 * A dialog could be opened here to enable it.
			 * If the user wishes to follow the links when installing
			 * desktop icons, he should 'Set desk icons...'
			 */

			options.cprefs &= ~CF_FOLL;

			for(i = 0; i < n; i++)
			{
				if((item = list[i]) < 0) /* was -1 earlier */
					return FALSE;

				ttype = itm_tgttype(sw, item);

				itm_follow(sw, item, &link, (char **)(&fname), &type);

				/* 
				 * Note: name will be deallocated in add_icon() if it fails;
				 * Otherwise it must be kept; add_icon just uses the pointer
				 */

				if(fname == NULL)
					return FALSE;
				else
				{
					nameonly = fn_get_name(fname);
					cramped_name( nameonly, tolabel, (int)sizeof(INAME) );
					icon = icnt_geticon(nameonly, type, ttype);
					add_icon(type, ttype, link, icon, tolabel, 0, ix, iy, TRUE, fname);
					incr_pos(&ix, &iy); 
				}
			}

			return TRUE;
		}
	}
}


/* 
 * funkties voor het laden en opslaan van de posities van de iconen. 
 * Remove all desktop icons 
 */

static void rem_all_icons(void)
{
	int i;

	for (i = 0; i < max_icons; i++)
		if (desk_icons[i].item_type != ITM_NOTUSED)
			remove_icon(i, FALSE);
}


/*
 * Set desktop background pattern and colour
 */

void set_dsk_background(int pattern, int color)
{
	options.dsk_pattern = (unsigned char)pattern;
	options.dsk_color = (unsigned char)color;
	desktop[0].ob_spec.obspec.fillpattern = pattern;
	desktop[0].ob_spec.obspec.interiorcol = color;
}


/*
 * Dummy routine: desktop keyboard handler
 */

static int dsk_hndlkey(WINDOW *w, int dummy_scancode, int dummy_keystate)
{
	return 0;
}


/*
 * (Re)generate desktop
 */

void regen_desktop(OBJECT *desk_tree)
{
	wind_set(0, WF_NEWDESK, desk_tree, 0);
	dsk_draw();
}


static ICONINFO this;


/*
 * Configuration table for one desktop icon
 */

CfgEntry Icon_table[] =
{
	{CFG_HDR,0,"icon" },
	{CFG_BEG},
	{CFG_S, 0, "name", this.ic.icon_name	},
	{CFG_S, 0, "labl", this.ic.label		},
	{CFG_D, 0, "type", &this.ic.item_type	},
	{CFG_D, 0, "tgtt", &this.ic.tgt_type	},
	{CFG_D, 0, "link", &this.ic.link		},
	{CFG_D, 0, "driv", &this.ic.icon_dat.drv},
	{CFG_S, 0, "path", this.name			},
	{CFG_D, 0, "xpos", &this.ic.x			},
	{CFG_D, 0, "ypos", &this.ic.y			},
	{CFG_END},
	{CFG_LAST}
};


/*
 * Load or save configuration of one desktop icon.
 * Beware: this.ic.icon_dat.drv is 0 to 26; NOT 'A' to 'Z'
 */

static CfgNest icon_cfg
{
	int i;

	*error = 0;

	if (io == CFG_SAVE)
	{
		ICON *ic = desk_icons;

		for (i = 0; i < max_icons; i++)
		{
			if (ic->item_type != ITM_NOTUSED)
			{
				this.ic = *ic;
				this.name[0] = 0;
	
				if (ic->item_type == ITM_DRIVE)
					this.ic.icon_dat.drv -= 'A';
				else
					this.ic.icon_dat.drv = 0;

				if(ic->tgt_type == ic->item_type)
					this.ic.tgt_type = 0; /* for smaller config files */
	
				if (isfilenet(ic->item_type))
					strcpy(this.name, ic->icon_dat.name);

				*error = CfgSave(file, Icon_table, lvl, CFGEMP); 
			}

			ic++;

			if ( *error != 0 )
				break;
		}
	}
	else
	{
		memclr(&this, sizeof(this));

		*error = CfgLoad(file, Icon_table, (int)sizeof(VLNAME), lvl); 

		if ( *error == 0 )
		{
			if (   this.ic.icon_name[0] == 0 
				|| ((this.ic.item_type < ITM_DRIVE
				|| this.ic.item_type > ITM_FILE)
				&& this.ic.item_type != ITM_NETOB) 
				|| this.ic.icon_dat.drv > 'Z' - 'A'
				)
					*error = EFRVAL;
			else
			{
				int nl, icon;
				char *name = NULL;

				/* SOME icon index should always be provided */

				icon = rsrc_icon(this.ic.icon_name); /* find by name */
				if (icon < 0)
					icon = default_icon(this.ic.item_type); /* display an alert */

				nl = (int)strlen(this.name);

				if( nl ) 
					name = strdup(this.name);
				else if(isfilenet(this.ic.item_type))
					name = strdup(empty);

				if (nl == 0 || name != NULL)
				{
					if (this.ic.tgt_type == 0) /* for smaller config files */
						this.ic.tgt_type = this.ic.item_type;

					*error = add_icon
							(
								(ITMTYPE)this.ic.item_type,
								(ITMTYPE)this.ic.tgt_type, 
								this.ic.link,
								icon,
								this.ic.label,
								this.ic.icon_dat.drv + 'A',
								this.ic.x,
								this.ic.y,
								FALSE,
								name
						 	);
				}
				else
					*error = ENSMEM;
			}
		}
	}
}


/*
 * Configuration table for desktop icons
 */

static CfgEntry DskIcons_table[] =
{
	{CFG_HDR,0,"deskicons" },
	{CFG_BEG},
	{CFG_NEST,0,"icon", icon_cfg },		/* Repeating group */
	{CFG_ENDG},
	{CFG_LAST}
};


/*
 * Load or save configuration of desktop icons
 */

CfgNest dsk_config
{
	*error = handle_cfg(file, DskIcons_table, lvl, CFGEMP, io, rem_all_icons, dsk_default);

	if ( io == CFG_LOAD && *error >= 0 )
		regen_desktop(desktop);
}


/*
 * Load the icon file. Result: TRUE if no error
 * Use standard functions of the AES 
 */

boolean load_icons(void)
{
	const char 
		*colour_irsc = "cicons.rsc", 	/* name of the colour icons file */
		*mono_irsc = "icons.rsc";		/* name of the mono icons file */

	void 
		**svtree = _GemParBlk.glob.ptree,
		*svrshdr = _GemParBlk.glob.rshdr;

	int 
		error;

	/* 
	 * Geneva 4 returns information that it supports
	 * colour icons, but that doesn't seem to work; thence a fix below:
	 * if there is no colour icons file fall back to black/white.
	 * Therefore, in Geneva 4 (and other similar cases, if any), remove 
	 * cicons.rsc from TeraDesk folder. Geneva 6 seems to work ok.
	 */

	if (!xd_aes4_0 || (xshel_find(colour_irsc, &error) == NULL))
		colour_icons = FALSE;

	if (rsrc_load( colour_icons ? colour_irsc : mono_irsc) == 0)
	{
		alert_abort(MICNFRD); 
		return FALSE;
	}
	else
	{
		int i = 0;

		rsrc_gaddr(R_TREE, 0, &icons);		/* That's all you need. */
		svicntree = _GemParBlk.glob.ptree;
		svicnrshdr = _GemParBlk.glob.rshdr;
		n_icons = 0;
		icons++;

		do 
		{
			n_icons++; 
		}
		while ( (icons[i++].ob_flags&LASTOB) == 0 );
	}

	_GemParBlk.glob.ptree = svtree;
	_GemParBlk.glob.rshdr = svrshdr;

	return TRUE;
}


void free_icons(void)
{
	void 
		**svtree = _GemParBlk.glob.ptree,
		*svrshdr = _GemParBlk.glob.rshdr;

	_GemParBlk.glob.ptree = svicntree;
	_GemParBlk.glob.rshdr = svicnrshdr;

	rsrc_free();

	_GemParBlk.glob.ptree = svtree;
	_GemParBlk.glob.rshdr = svrshdr;
}


/* 
 * Routine voor het initialiseren van de desktop. 
 * Return FALSE in case of error
 */

boolean dsk_init(void)
{
	int i, error;

	/* Open the desktop window */

	if ((desk_window = xw_open_desk(DESK_WIND, &dsk_functions,
									sizeof(DSK_WINDOW), &error)) == NULL)
	{
		if (error == XDNSMEM)
			xform_error(ENSMEM);

		return FALSE;
	}

	((DSK_WINDOW *) desk_window)->itm_func = &itm_func;

	/* 
	 * Determine the number of icons that can fit on the screen.
	 * Allow 50% overlapping, and also always allow for at least 64 icons. 
	 * This should permit that configuration files for at least ST-High 
	 * can be loaded in ST-Low resolution
	 */

	m_icnx = screen_info.dsk.w / ICON_W;
	m_icny = screen_info.dsk.h / ICON_H;
	max_icons = max(m_icnx * m_icny * 3 / 2, 64);
	m_icnx--;
	m_icny--;

	if 
	(   
		((desktop = malloc_chk((long)(max_icons + 1) * sizeof(OBJECT))) == NULL)
	    || ((desk_icons = malloc_chk((long)max_icons * sizeof(ICON))) == NULL)
	)
	{
		free(desktop);
		return FALSE;
	}

	init_obj(&desktop[0], G_BOX);

	desktop[0].ob_spec.obspec.framesize = 0;
	desktop[0].ob_spec.obspec.textmode = 1;
	desktop[0].r = screen_info.dsk;

	for (i = 0; i < max_icons; i++)
		desk_icons[i].item_type = ITM_NOTUSED;

#ifdef MEMDEBUG
	atexit(rem_all_icons);
#endif
	return TRUE;
}


static void incr_pos(int *x, int *y)
{
	if ((*x += 1) > m_icnx)
	{
		*x = 0;
		*y += 1;
	}
}


/*
 * Add missing disk icons A to Z. Icon and label are the same for all
 * disks (i.e. it is possible here that a floppy gets the hard-disk icon.
 * If icon name (label) is not specified, disk volume label is read
 * (except for A and B disks).
 */

static void dsk_diskicons(int *x, int *y, int ic, char *iname)
{
	int d, i, j;
	long drives = drvmap();
	boolean have;

	/* Check all drive letters A to Z (drives '0' to '9' not supported) */

	for (i = 0; i < 26; i++)
	{
		d = 'A' + i;
		have = FALSE;
	
		/* Is there an icon for it ? */

		for ( j = 0; j < max_icons; j++ )
		{
			if
			(
				(desk_icons[j].item_type == ITM_DRIVE) &&
				(desk_icons[j].icon_dat.drv == d)
			)
			{
				have = TRUE;
				break;
			}
		}

		if (!have && btst(drives, i))
		{
			/* If icon label is not specified, use disk volume labels */

			int icnind;
			INAME thelabel;

			strcpy(thelabel, iname); /* needed for more than one drive */
			icn_disklabel(i, thelabel);

			icnind = add_devicon(ITM_DRIVE, ic, thelabel, d, *x, *y); 

			if( icnind < 0)
				break;
			desk_icons[icnind].update = ICN_REDRAW;

			incr_pos(x, y);
		}
	}
}


/*
 * Remove all icons, then add default ones (disk drives, printer and trash can)
 */

void dsk_default(void)
{
	int x = 2, y = 0, ic;

	rem_all_icons();

	/* Identify icons by name, not index;  use names from the rsc file. */

	/* Two floppies in the first row */

	ic = rsrc_icon_rscid( FLINAME, iname );
	add_devicon(ITM_DRIVE, ic, iname, 'A', 0, 0);
	add_devicon(ITM_DRIVE, ic, iname, 'B', 1, 0);
	
	/* Hard disks; continue after the floppies */

	ic = rsrc_icon_rscid ( HDINAME, iname );
	dsk_diskicons(&x, &y, ic, iname);

	/* Printer and trashcan in the next two rows */

	y++;
	ic = rsrc_icon_rscid ( PRINAME, iname );
	add_devicon(ITM_PRINTER, ic, iname, 0, 0, y);	

	y++;
	ic = rsrc_icon_rscid ( TRINAME, iname );
	add_devicon(ITM_TRASH, ic, iname, 0, 0, y);
}


/* 
 * Change a desktop icon (or more than one of them).
 * This routine should be activated only if n > 0.
 * The routine also handles removal of a whole group of icons
 * without opening the dialog, if dialog == FALSE.
 */

void dsk_chngicon(int n, int *list, boolean dialog)
{
	int 
		button = -1,
		i = 0;

	sl_noop = 0;

	while ((i < n) && (button != CHNICNAB))
	{
		if(dialog)
			button = chng_icon(list[i]); /* The dialog */
		else
			desk_icons[list[i]].update = ICN_DELETE;

		i++;
	}

	if(dialog)
		xd_close(&icd_info);

	/* 
	 * When all is finished and the dialog is closed, 
	 * redraw affected icons. 
	 */

	dsk_do_update();
}


/*
 * Limit a colour index to values existing in the current video mode 
 */

int limcolor(int col)
{
	return minmax(0, col, xd_ncolors - 1);
}


/*
 * Limit a pattern index to one among the first 8 because patterns
 * specified in OBSPEC are limited to 3-bit indexes.
 */

int limpattern(int pat)
{
	return minmax(1, pat, min(7, xd_npatterns - 1));
}


/* 
 * Set colour in a colour / pattern selector box.
 * Colour is limited to range available in the current video mode. 
 */

void set_selcol(XDINFO *info, int obj, int col)
{
	wdoptions[obj].ob_spec.obspec.interiorcol = limcolor(col);
	xd_drawthis(info, obj);
}


/*
 * Set pattern in a colour / pattern selector box
 */

static void set_selpat(XDINFO *info, int obj, int pat)
{
	wdoptions[obj].ob_spec.obspec.fillpattern = limpattern( pat );
	xd_drawthis(info, obj);
}


/*
 * Handle window options dialog
 */

void dsk_options(void)
{
	int 
		color, 
		pattern,
		wcolor, 
		wpattern,
		button = -1, 
		oldbutton = -1; 	/* aux for arrow_form_do */

	XDINFO 
		info;

	boolean
		ok = FALSE,
		stop = FALSE, 
		draw = FALSE;

	char
		*tabsize = wdoptions[TABSIZE].ob_spec.tedinfo->te_ptext;


	/* Initial states */
	
	wdoptions[DSKPAT].ob_spec.obspec.fillpattern = limpattern((int)options.dsk_pattern);
	wdoptions[DSKPAT].ob_spec.obspec.interiorcol = limcolor((int)options.dsk_color) ;
	wdoptions[WINPAT].ob_spec.obspec.fillpattern = limpattern((int)options.win_pattern);
	wdoptions[WINPAT].ob_spec.obspec.interiorcol = limcolor( (int)options.win_color);
	itoa(options.tabsize, tabsize, 10);	
	set_opt( wdoptions, options.sexit, SAVE_WIN, SOPEN);

	/* Open the dialog */

	if(chk_xd_open(wdoptions, &info) >= 0)
	{
		while (!stop)
		{
			color = wdoptions[DSKPAT].ob_spec.obspec.interiorcol;
			pattern = wdoptions[DSKPAT].ob_spec.obspec.fillpattern;
			wcolor = wdoptions[WINPAT].ob_spec.obspec.interiorcol; 
			wpattern = wdoptions[WINPAT].ob_spec.obspec.fillpattern;
			button = arrow_form_do( &info, &oldbutton );

			switch (button)
			{
			case WOVIEWER:
			case WODIR:
				if ( wd_type_setfont( (button == WOVIEWER) ? DTVFONT : DTDFONT) )
					ok = TRUE;
				break;
			case DSKCUP:
				color += 2;
			case DSKCDOWN:
				color--;
				set_selcol(&info, DSKPAT, color);
				break;
			case DSKPUP:
				pattern += 2;
			case DSKPDOWN:
				pattern--;
				set_selpat(&info, DSKPAT, pattern);
				break;
			case WINCUP:
				wcolor += 2;
			case WINCDOWN:
				wcolor--;
				set_selcol(&info, WINPAT, wcolor);
				break;
			case WINPUP:
				wpattern += 2;
			case WINPDOWN:
				wpattern--;
				set_selpat(&info, WINPAT, wpattern);
				break;
			case WOPTOK:
				/* Desktop pattern & colour */	

				if ((options.dsk_pattern != (unsigned char)pattern) ||
					(options.dsk_color != (unsigned char)color))
				{
					set_dsk_background(pattern, color);
					draw = TRUE;
				}

				/* window pattern & colour */

				if ((options.win_pattern != (unsigned char)wpattern) ||
					(options.win_color != (unsigned char)wcolor))
				{
					options.win_pattern= (unsigned char)wpattern;
					options.win_color= (unsigned char)wcolor; 
					ok = TRUE;
				}		

				/* Tab size */

				if ((options.tabsize = atoi(tabsize)) < 1)
					options.tabsize = 1;
				
				/* Save open windows? */

				get_opt(wdoptions, (int *)&options.sexit, SAVE_WIN, SOPEN);

			default:
				stop = TRUE;
				break;
			}
		}

		xd_buttnorm(&info, button);
		xd_close(&info);

		/* Updates must not be done before the dialogs are closed */

		if (draw)
			redraw_desk(&screen_info.dsk);
		if (ok)
			wd_drawall();
	}
}
