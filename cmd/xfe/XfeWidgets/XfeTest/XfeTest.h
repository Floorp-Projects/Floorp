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
/* Name:		<XfeTest/XfeTest.h>										*/
/* Description:	Xfe widget tests main header file.						*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/


#ifndef _XfeTest_h_				/* start XfeTest.h		*/
#define _XfeTest_h_

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <Xfe/XfeAll.h>
#include <Xfe/BmButton.h>
#include <Xfe/BmCascade.h>

#include <Xm/XmAll.h>

#ifdef __cplusplus				/* start C++		*/
extern "C" {
#endif

typedef struct
{
	String      name;
	char **     data;
	Pixmap      pixmap;
	Pixmap      mask;
} XfeGraphicRec,*XfeGraphic;

/*----------------------------------------------------------------------*/
#define XfeFree(_m) \
{ if (((char *) (_m)) != NULL) { XtFree((char *) (_m)); (_m) = NULL; } }
/*----------------------------------------------------------------------*/
#define XfeMalloc(_t,_n) \
(_t *) XtMalloc(sizeof(_t) * (_n))
/*----------------------------------------------------------------------*/
#define XfeCalloc(_t,_n) \
(_t *) XtCalloc((_n),sizeof(_t))
/*----------------------------------------------------------------------*/
#define XfeRealloc(_m,_t,_n) \
(_t *) XtRealloc((char *) (_m),sizeof(_t) * (_n))
/*----------------------------------------------------------------------*/
#define XfeMemcpy(_d,_s,_t,_n) \
memcpy((void *) (_d),(void *) (_s),sizeof(_t) * (_n))
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
typedef enum
{
	XfeMENU_BM_PANE,
	XfeMENU_BM_PUSH,
	XfeMENU_LABEL,
	XfeMENU_PANE,
	XfeMENU_PUSH,
	XfeMENU_RADIO,
	XfeMENU_SEP,
	XfeMENU_TOGGLE
} XfeMenuItemType;
/*----------------------------------------------------------------------*/
typedef struct _XfeMenuItemRec
{
	String						name;
	XfeMenuItemType				type;
	XtCallbackProc				action_cb;
	struct _XfeMenuItemRec *	pane_items;
	XtPointer					client_data;
} XfeMenuItemRec,*XfeMenuItem;
/*----------------------------------------------------------------------*/
typedef struct
{
	String				name;
	XfeMenuItemRec *	items;
} XfeMenuPaneRec,*XfeMenuPane;
/*----------------------------------------------------------------------*/
extern Widget
XfeMenuItemCreate			(Widget				pw,
							 XfeMenuItem		data,
							 XtPointer			client_data);
/*----------------------------------------------------------------------*/
extern Widget
XfeMenuItemCreate2			(Widget				pw,
							 String				name,
							 XfeMenuItemType	type,
							 XtCallbackProc		action_cb,
							 XfeMenuItemRec *	pane_items,
							 XtPointer			client_data);
/*----------------------------------------------------------------------*/
extern Widget
XfeMenuPaneCreate			(Widget				pw,
							 XfeMenuPane		data,
							 XtPointer			client_data);
/*----------------------------------------------------------------------*/
extern Widget
XfeMenuPaneCreate2			(Widget				pw,
							 String				name,
							 XfeMenuItemRec *	items,
							 XtPointer			client_data);
/*----------------------------------------------------------------------*/
extern Widget
XfeMenuBarCreate			(Widget				pw,
							 String				name,
							 XfeMenuPaneRec *	items,
							 XtPointer			client_data,
							 ArgList			av,
							 Cardinal			ac);
/*----------------------------------------------------------------------*/
extern Widget
XfePopupMenuCreate			(Widget				pw,
							 String				name,
							 XfeMenuPaneRec *	items,
							 XtPointer			client_data,
							 ArgList			av,
							 Cardinal			ac);
/*----------------------------------------------------------------------*/
extern Widget
XfeToolBarItemCreate		(Widget				pw,
							 XfeMenuItem		data,
							 XtPointer			client_data);
/*----------------------------------------------------------------------*/
extern Widget
XfeToolBarPaneCreate		(Widget				pw,
							 XfeMenuPane		data,
							 XtPointer			client_data);
/*----------------------------------------------------------------------*/
extern Widget
XfeToolBarCreate			(Widget				pw,
							 String				name,
							 XfeMenuPaneRec *	items,
							 XtPointer			client_data);
/*----------------------------------------------------------------------*/
extern String
XfeNameIndex				(String				prefix,
							 Cardinal			i);
/*----------------------------------------------------------------------*/
extern String
XfeNameString				(String				prefix,
							 String				name);
/*----------------------------------------------------------------------*/
extern Boolean
XfeAllocatePixmapFromFile	(char *				filename,
							 Display *			dpy,
							 Drawable			d,
							 Colormap			colormap,
							 Cardinal			closeness,
							 Cardinal			depth,
							 Pixel				bg,
							 Pixmap *			pixmap,
							 Pixmap *			mask);
/*----------------------------------------------------------------------*/
extern Boolean
XfeAllocatePixmapFromData	(char **			data,
							 Display *			dpy,
							 Drawable			d,
							 Colormap			colormap,
							 Cardinal			closeness,
							 Cardinal			depth,
							 Pixel				bg,
							 Pixmap *			pixmap,
							 Pixmap *			mask);
/*----------------------------------------------------------------------*/
extern void
XfeAppCreate				(char *				app_name,
							 int *				argc,
							 String *			argv);
/*----------------------------------------------------------------------*/
extern void
XfeAppCreateSimple			(char *				app_name,
							 int *				argc,
							 String *			argv,
							 char *				frame_name,
							 Widget *			frame_out,
							 Widget *			form_out);
/*----------------------------------------------------------------------*/
extern Widget
XfeFrameCreate				(char *				frame_name,
							 ArgList			args,
							 Cardinal			num_args);
/*----------------------------------------------------------------------*/
extern Widget
XfeFrameCreateWithChrome	(char *				frame_name,
							 ArgList			args,
							 Cardinal			num_args);
/*----------------------------------------------------------------------*/
extern Pixmap
XfeGetPixmapFromFile		(Widget				w,
							 char *				filename);
/*----------------------------------------------------------------------*/
extern Pixmap
XfeGetPixmapFromData		(Widget				w,
							 char **			data);
/*----------------------------------------------------------------------*/
extern XfePixmapTable
XfeAllocatePixmapTable		(Widget				w,
							 String *			files,
							 Cardinal			num_files);
/*----------------------------------------------------------------------*/
extern Widget
XfeCreateManagedForm		(Widget				pw,
							 String				name,
							 ArgList			args,
							 Cardinal			n);
/*----------------------------------------------------------------------*/
extern Widget
XfeCreateManagedFrame		(Widget				pw,
							 String				name,
							 ArgList			args,
							 Cardinal			n);
/*----------------------------------------------------------------------*/
extern Widget
XfeCreateManagedRowColumn	(Widget				pw,
							 String				name,
							 ArgList			args,
							 Cardinal			n);
/*----------------------------------------------------------------------*/
extern void
XfeAddEditresSupport		(Widget				shell);
/*----------------------------------------------------------------------*/
extern void
XfeRegisterStringToWidgetCvt(void);
/*----------------------------------------------------------------------*/
extern XtAppContext
XfeAppContext				(void);
/*----------------------------------------------------------------------*/
extern Widget
XfeAppShell					(void);
/*----------------------------------------------------------------------*/
extern void
XfeAppMainLoop				(void);
/*----------------------------------------------------------------------*/
extern XtIntervalId
XfeAppAddTimeOut			(unsigned long			interval,
							 XtTimerCallbackProc	proc,
							 XtPointer				client_data);
/*----------------------------------------------------------------------*/
extern Pixel
XfeButtonRaiseBackground		(Widget);
/*----------------------------------------------------------------------*/
extern Pixel
XfeButtonArmBackground			(Widget);
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
extern Widget
XfeCreateLoadedToolBar		(Widget				pw,
							 String				name,
							 String				item_prefix,
							 Cardinal			tool_count,
							 Cardinal			sep_count,
							 XtCallbackProc		arm_cb,
							 XtCallbackProc		disarm_cb,
							 XtCallbackProc		activate_cb,
							 WidgetList *		tool_items_out);
/*----------------------------------------------------------------------*/
extern void
XfeLoadToolBar				(Widget				tool_bar,
							 String				item_prefix,
							 Cardinal			tool_count,
							 Cardinal			sep_count,
							 XtCallbackProc		arm_cb,
							 XtCallbackProc		disarm_cb,
							 XtCallbackProc		activate_cb,
							 WidgetList *		tool_items_out);
/*----------------------------------------------------------------------*/
extern Widget
XfeCreateLoadedDashBoard	(Widget				pw,
							 String				name,
							 String				tool_prefix,
							 XtCallbackProc		tool_cb,
							 Cardinal			tool_count,
							 String				task_prefix,
							 Boolean			task_large,
							 XtCallbackProc		task_cb,
							 Cardinal			task_count,
							 Widget *			tool_bar_out,
							 Widget *			progress_bar_out,
							 Widget *			status_bar_out,
							 Widget *			task_bar_out,
							 WidgetList *		tool_items_out,
							 WidgetList *		task_items_out);
/*----------------------------------------------------------------------*/
extern void
XfeLoadTaskBar				(Widget				task_bar,
							 Boolean			large,
							 String				task_prefix,
							 XtCallbackProc		task_cb,
							 Cardinal			task_count,
							 WidgetList *		task_items_out);
/*----------------------------------------------------------------------*/
extern Widget
XfeCreateLoadedTaskBar		(Widget				pw,
							 String				name,
							 Boolean			large,
							 String				task_prefix,
							 XtCallbackProc		task_cb,
							 Cardinal			task_count,
							 WidgetList *		task_items_out);
/*----------------------------------------------------------------------*/
extern Widget
XfeCreateLoadedToolBox		(Widget				pw,
							 String				name,
							 ArgList			av,
							 Cardinal			ac);
/*----------------------------------------------------------------------*/
extern Widget
XfeCreateLoadedTab			(Widget				pw,
							 String				name,
							 ArgList			av,
							 Cardinal			ac);
/*----------------------------------------------------------------------*/
extern Widget
XfeCreateFormAndButton		(Widget				pw,
							 String				name,
							 String				button_name,
							 Dimension			offset,
							 Boolean			use_gadget,
							 ArgList			av,
							 Cardinal			ac);
/*----------------------------------------------------------------------*/
extern void
XfeWidgetToggleManaged		(Widget				w);
/*----------------------------------------------------------------------*/
extern void
XfeWidgetFlash				(Widget				w,
							 Cardinal			ms,
							 Cardinal			n);
/*----------------------------------------------------------------------*/
extern void
XfeResourceToggleBoolean	(Widget				w,
							 String				name);
/*----------------------------------------------------------------------*/

extern void	XfeActivateCallback		(Widget,XtPointer,XtPointer);
extern void	XfeArmCallback			(Widget,XtPointer,XtPointer);
extern void	XfeDestroyCallback		(Widget,XtPointer,XtPointer);
extern void	XfeDisarmCallback		(Widget,XtPointer,XtPointer);
extern void	XfeDockCallback			(Widget,XtPointer,XtPointer);
extern void	XfeEditCallback			(Widget,XtPointer,XtPointer);
extern void	XfeEnterCallback		(Widget,XtPointer,XtPointer);
extern void	XfeExitCallback			(Widget,XtPointer,XtPointer);
extern void	XfeFreeDataCallback		(Widget,XtPointer,XtPointer);
extern void	XfeLabelSelectionChangedCallback	(Widget,XtPointer,XtPointer);
extern void	XfeLeaveCallback		(Widget,XtPointer,XtPointer);
extern void	XfeLowerCallback		(Widget,XtPointer,XtPointer);
extern void	XfeMapCallback			(Widget,XtPointer,XtPointer);
extern void	XfeRaiseCallback		(Widget,XtPointer,XtPointer);
extern void	XfeResizeCallback		(Widget,XtPointer,XtPointer);
extern void	XfeToolBoxCloseCallback	(Widget,XtPointer,XtPointer);
extern void	XfeToolBoxOpenCallback	(Widget,XtPointer,XtPointer);
extern void	XfeUndockCallback		(Widget,XtPointer,XtPointer);
extern void	XfeUnmapCallback		(Widget,XtPointer,XtPointer);

extern XfeTruncateXmStringProc XfeGetTruncateXmStringProc (void);

extern void
XfeAnimationCreate(Widget			w,
				   char ***			animation_data,
				   Pixmap **		pixmaps_out,
				   Cardinal *		num_pixmaps_out);

extern void
XfeGetMain20x20Animation(Widget		w,
						 Pixmap **	pixmaps_out,
						 Cardinal *	num_pixmaps_out);

extern void
XfeGetMain40x40Animation(Widget		w,
						 Pixmap **	pixmaps_out,
						 Cardinal *	num_pixmaps_out);

extern void
XfeGetTransparent20x20Animation(Widget		w,
								Pixmap **	pixmaps_out,
								Cardinal *	num_pixmaps_out);

extern void
XfeGetTransparent40x40Animation(Widget		w,
								Pixmap **	pixmaps_out,
								Cardinal *	num_pixmaps_out);
/*----------------------------------------------------------------------*/
extern XfeGraphic
XfeGetGraphic				(Widget         w,
							 char *         name);
/*----------------------------------------------------------------------*/
extern Pixmap
XfeGetPixmap				(Widget			w,
							 char *			name);
/*----------------------------------------------------------------------*/
extern Pixmap
XfeGetMask					(Widget			w,
							 char *			name);
/*----------------------------------------------------------------------*/
extern XmFontList
XfeGetFontList				(String			name);
/*----------------------------------------------------------------------*/
extern XmString
XfeGetXmString				(String			name);
/*----------------------------------------------------------------------*/
extern XmFontList *
XfeGetFontListTable			(String *		names,
							 Cardinal		n);
/*----------------------------------------------------------------------*/
extern XmString *
XfeGetXmStringTable			(String *		names,
							 Cardinal		n);
/*----------------------------------------------------------------------*/
extern void
XfeFreeXmStringTable		(XmString *		table,
							 Cardinal		n);
/*----------------------------------------------------------------------*/
extern Pixel
XfeGetPixel					(String			name);
/*----------------------------------------------------------------------*/
extern Cursor
XfeGetCursor				(String			name);
/*----------------------------------------------------------------------*/


/*----------------------------------------------------------------------*/
extern void
XfeStringToUpper				(String		s);
/*----------------------------------------------------------------------*/
extern void
XfeStringToLower				(String		s);
/*----------------------------------------------------------------------*/

#ifdef __cplusplus				/* end C++		*/
}
#endif

#endif						/* end XfeTest.h		*/
