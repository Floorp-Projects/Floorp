/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
/*-----------------------------------------*/
/*																		*/
/* Name:		<Xfe/Xfe.h>												*/
/* Description:	Xfe widgets main header file.							*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/


#ifndef _XfeXfe_h_								/* start Xfe.h			*/
#define _XfeXfe_h_

#include <Xm/Xm.h>								/* Motif public defs	*/
#include <Xfe/StringDefs.h>						/* Xfe public str defs	*/
#include <Xfe/Defaults.h>						/* Xfe default res vals	*/
#include <Xfe/ChildrenUtil.h>					/* Children utils		*/
#include <Xfe/Converters.h>						/* Converters			*/
#include <Xfe/DialogUtil.h>						/* Dialog utils			*/
#include <Xfe/Draw.h>							/* Drawing utils		*/
#include <Xfe/Find.h>							/* Finding utils		*/
#include <Xfe/Geometry.h>						/* Geometry utils		*/
#include <Xfe/ListUtil.h>						/* List utils			*/
#include <Xfe/MenuUtil.h>						/* Menu/RowCol utils	*/
#include <Xfe/RepType.h>						/* Representation types	*/
#include <Xfe/Resources.h>						/* Resource utils		*/
#include <Xfe/ShellUtil.h>						/* Shell utils			*/
#include <Xfe/StringUtil.h>						/* XmString  utils		*/
#include <Xfe/WmUtil.h>							/* Window manager utils	*/

#ifdef DEBUG
#include <Xfe/Debug.h>							/* Debugging stuff		*/
#endif

#include <assert.h>								/* Assert				*/

#ifdef __cplusplus								/* start C++			*/
extern "C" {
#endif

/*----------------------------------------------------------------------*/
/*																		*/
/* Event loop proc														*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef void	(*XfeEventLoopProc)		(void);

/*----------------------------------------------------------------------*/
/*																		*/
/* Callback Reasons														*/
/*																		*/
/*----------------------------------------------------------------------*/
enum
{
    XmCR_ACTION  = (XmCR_OBSCURED_TRAVERSAL + 999),/* Generic action	*/
    XmCR_ANIMATION,								/* Logo animation		*/
	XmCR_TITLE_CHANGED,							/* Title changed		*/
	XmCR_TOOL_BAR_SELECTION_CHANGED,			/* TB selection changed	*/
	XmCR_TOOL_BAR_VALUE_CHANGED,				/* TB value changed		*/
    XmCR_BEFORE_REALIZE,						/* Before realize		*/
    XmCR_BEFORE_RESIZE,							/* Before resize		*/
    XmCR_BUTTON_3_DOWN,							/* Button 3 down		*/
    XmCR_BUTTON_3_UP,							/* Button 3 up			*/
    XmCR_CHANGE_MANAGED,						/* Change managed		*/
    XmCR_CLOSE,									/* Task bar close		*/
    XmCR_DELETE_WINDOW,							/* Delete window		*/
    XmCR_DOCK,									/* Task bar dock		*/
    XmCR_ENTER,									/* Enter				*/
    XmCR_FIRST_MAP,								/* First map			*/
    XmCR_FLOATING_MAP,							/* Floating map			*/
    XmCR_FLOATING_UNMAP,						/* Floating unmap		*/
    XmCR_GRAB,									/* Grab					*/
    XmCR_LEAVE,									/* Leave				*/
    XmCR_LOWER,									/* Lower				*/
    XmCR_MOVE,									/* Move					*/
    XmCR_POPDOWN,								/* Popdown				*/
    XmCR_POPULATE,								/* Populate				*/
    XmCR_POPUP,									/* Popup				*/
    XmCR_RAISE,									/* Raise				*/
    XmCR_REALIZE,								/* Realize				*/
    XmCR_SAVE_YOURSELF,							/* Save yourself		*/
    XmCR_SELECTION_CHANGED,						/* Selection changed	*/
    XmCR_SUBMENU_MAP,							/* Submenu map			*/
    XmCR_SUBMENU_TEAR,							/* Submenu torn/untorn	*/
    XmCR_SUBMENU_UNMAP,							/* Submenu unmap		*/
    XmCR_TASK_BAR,								/* Task bar active		*/
    XmCR_TOGGLE_BOX,							/* Toggle box			*/
    XmCR_TOGGLE_SELECTION,						/* Toggle selection		*/
    XmCR_TOOL_BOX_ALLOW,						/* Tool box allow		*/
    XmCR_TOOL_BOX_CLOSE,						/* Tool box close		*/
    XmCR_TOOL_BOX_DRAG_END,						/* Tool box drag end	*/
    XmCR_TOOL_BOX_DRAG_MOTION,					/* Tool box drag motion	*/
    XmCR_TOOL_BOX_DRAG_START,					/* Tool box drag start	*/
    XmCR_TOOL_BOX_NEW_ITEM,						/* Tool box new item    */
    XmCR_TOOL_BOX_OPEN,							/* Tool box open		*/
    XmCR_TOOL_BOX_SNAP,							/* Tool box snap		*/
    XmCR_TOOL_BOX_SWAP,							/* Tool box swap		*/
    XmCR_UNDOCK,								/* Task bar undock		*/ 
    XmCR_UNGRAB,								/* Ungrab				*/
    XmCR_XFE_LAST_REASON						/* Last reason marker	*/
};

/*----------------------------------------------------------------------*/
/*																		*/
/* Pixmap Table															*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef Pixmap * XfePixmapTable;

/*----------------------------------------------------------------------*/
/*																		*/
/* Pixmap utilities														*/
/*																		*/
/*----------------------------------------------------------------------*/
extern Boolean
XfePixmapExtent				(Display *		dpy,
							 Pixmap			pixmap,
							 Dimension *	width,
							 Dimension *	height,
							 Cardinal *		depth);
/*----------------------------------------------------------------------*/
extern Boolean
XfePixmapGood				(Pixmap pixmap);
/*----------------------------------------------------------------------*/
extern Pixmap
XfeInsensitiveTile			(Screen *	screen,
							 int		depth,
							 Pixel		fg,
							 Pixel		bg);
/*----------------------------------------------------------------------*/
extern Pixmap
XfeInsensitiveStipple		(Screen *	screen,
							 int		depth,
							 Pixel		fg,
							 Pixel		bg);
/*----------------------------------------------------------------------*/
extern void
XfePixmapClear				(Display *	dpy,
							 Pixmap		pixmap,
							 GC			gc,
							 Dimension	width,
							 Dimension	height);
/*----------------------------------------------------------------------*/
extern Pixmap
XfePixmapCheck				(Widget			w,
							 Pixmap			pixmap,
							 Dimension *	width_out,
							 Dimension *	height_out);
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Gc utilities															*/
/*																		*/
/*----------------------------------------------------------------------*/
extern GC
XfeAllocateStringGc				(Widget		w,
								 XmFontList	font_list,
								 Pixel		fg,
								 Pixel		bg,
								 Boolean	sensitive);
/*----------------------------------------------------------------------*/
extern GC
XfeAllocateColorGc				(Widget		w,
								 Pixel		fg,
								 Pixel		bg,
								 Boolean	sensitive);
/*----------------------------------------------------------------------*/
extern GC
XfeAllocateTileGc				(Widget		w,
								 Pixmap		tile_pixmap);
/*----------------------------------------------------------------------*/
extern GC
XfeAllocateTransparentGc		(Widget		w);
/*----------------------------------------------------------------------*/
extern GC
XfeAllocateSelectionGc			(Widget		w,
								 Dimension	thickness,
								 Pixel		fg,
								 Pixel		bg);
/*----------------------------------------------------------------------*/
extern GC
XfeAllocateCopyGc				(Widget		w);
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Clipping functions													*/
/*																		*/
/*----------------------------------------------------------------------*/
extern void
XfeClipSeg						(XSegment *		seg_src,
								 XSegment *		seg_dst,
								 XRectangle *	clip_rect);
/*----------------------------------------------------------------------*/
extern void
XfeClipRect						(XRectangle *	rect_src,
								 XRectangle *	rect_dst,
								 XRectangle *	clip_rect);
/*----------------------------------------------------------------------*/
extern Boolean
XfeRectInRect					(XRectangle *	rect,
								 XRectangle *	clip_rect);
/*----------------------------------------------------------------------*/
extern Boolean
XfePointInRect					(XRectangle *	rect,
								 int			x,
								 int			y);
/*----------------------------------------------------------------------*/
extern void 
XfeRectSet						(XRectangle *	rect,
								 Position		x,
								 Position		y,
								 Dimension		width,
								 Dimension		height);
/*----------------------------------------------------------------------*/
extern void 
XfeRectCopy						(XRectangle *	dst,
								 XRectangle *	src);
/*----------------------------------------------------------------------*/
extern void 
XfePointSet						(XPoint *		point,
								 Position		x,
								 Position		y);
/*----------------------------------------------------------------------*/
	
/*----------------------------------------------------------------------*/
/*																		*/
/* Simple Widget access functions										*/
/*																		*/
/*----------------------------------------------------------------------*/
extern Widget		XfeWindowedWidget		(Widget w);
extern Boolean		XfeIsViewable			(Widget w);
extern Boolean		XfeIsAlive				(Widget w);
extern XtPointer	XfeUserData				(Widget w);
extern XtPointer	XfeInstancePointer		(Widget w);
extern Colormap		XfeColormap				(Widget w);
extern Cardinal		XfeDepth				(Widget w);
extern Pixel		XfeBackground			(Widget w);
extern Pixel		XfeForeground			(Widget w);
extern Visual *		XfeVisual				(Widget w);
extern String		XfeClassNameForWidget	(Widget w);

/*----------------------------------------------------------------------*/
/*																		*/
/* Simple WidgetClass access functions									*/
/*																		*/
/*----------------------------------------------------------------------*/
extern String		XfeClassName			(WidgetClass wc);
extern WidgetClass	XfeSuperClass			(WidgetClass wc);

/*----------------------------------------------------------------------*/
/*																		*/
/* XmFontList															*/
/*																		*/
/*----------------------------------------------------------------------*/
extern XmFontList
XfeXmFontListCopy			(Widget			w,
							 XmFontList		font_list,
							 unsigned char	font_type);
/*----------------------------------------------------------------------*/
/* extern */ XmFontList
XfeFastAccessFontList		(Widget			w);
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Colors																*/
/*																		*/
/*----------------------------------------------------------------------*/
extern Pixel
XfeSelectPixel				(Widget			w,
							 Pixel			base);
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Cursor																*/
/*																		*/
/*----------------------------------------------------------------------*/
extern void
XfeCursorDefine				(Widget			w,
							 Cursor			cursor);
/*----------------------------------------------------------------------*/
extern void
XfeCursorUndefine			(Widget			w);
/*----------------------------------------------------------------------*/
extern Boolean
XfeCursorGood				(Cursor			cursor);
/*----------------------------------------------------------------------*/
extern Cursor				
XfeCursorGetDragHand		(Widget			w);
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Simple Math															*/
/*																		*/
/*----------------------------------------------------------------------*/
#define XfeAbs(a)		(( (a) < 0 ) ? -(a) : (a) )
#define XfeMax(a,b)		(( (a) > (b) ) ? (a) : (b) )
#define XfeMin(a,b)		(( (a) < (b) ) ? (a) : (b) )
#define XfeSgn(a)		(( (a) < 0 ) ? -1 : 1 )
#define XfeOdd(a)		((((a) % 2) != 0) ? True : False)
#define XfeEven(a)		((((a) % 2) == 0) ? True : False)
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Translation / Action functions										*/
/*																		*/
/*----------------------------------------------------------------------*/
extern void
XfeOverrideTranslations			(Widget			w,
								 String			table);
/*----------------------------------------------------------------------*/
extern void
XfeAddActions					(Widget			w,
								 XtActionList	actions,
								 Cardinal		num_actions);
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Management															*/
/*																		*/
/*----------------------------------------------------------------------*/
extern void
XfeSetManagedState				(Widget			w,
								 Boolean		state);
/*----------------------------------------------------------------------*/
extern void
XfeToggleManagedState			(Widget			w);
/*----------------------------------------------------------------------*/


/*----------------------------------------------------------------------*/
/*																		*/
/* Explicit invocation of core methods.  								*/
/*																		*/
/*----------------------------------------------------------------------*/
extern void
XfeExpose						(Widget			w,
								 XEvent *		event,
								 Region			region);
/*----------------------------------------------------------------------*/
extern void
XfeResize						(Widget			w);
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Sleep routine.														*/
/*																		*/
/*----------------------------------------------------------------------*/
extern void						
XfeSleep						(Widget				w,
								 XfeEventLoopProc	proc,
								 int				ms);
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XEvent functions														*/
/*																		*/
/*----------------------------------------------------------------------*/
extern Boolean
XfeEventGetXY					(XEvent *			event,
								 int *				x_out,
								 int *				y_out);
/*----------------------------------------------------------------------*/
extern Boolean
XfeEventGetRootXY				(XEvent *			event,
								 int *				x_out,
								 int *				y_out);
/*----------------------------------------------------------------------*/
extern Modifiers
XfeEventGetModifiers			(XEvent *			event);
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Test whether a widget is a private component of an XfeManager parent */
/*																		*/
/*----------------------------------------------------------------------*/
extern Boolean		XfeIsPrivateComponent	(Widget	w);
/*----------------------------------------------------------------------*/

#ifdef __cplusplus								/* end C++				*/
}
#endif

#endif											/* end Xfe.h			*/
