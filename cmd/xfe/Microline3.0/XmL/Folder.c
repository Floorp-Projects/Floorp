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

/*
 * The following source code is part of the Microline Widget Library.
 * The Microline widget library is made available to Mozilla developers
 * under the Netscape Public License (NPL) by Neuron Data.  To learn
 * more about Neuron Data, please visit the Neuron Data Home Page at
 * http://www.neurondata.com.
 */

#include "FolderP.h"
#include <X11/StringDefs.h>
#include <Xm/DrawnB.h>
#include <Xm/Label.h>
#include <Xm/Form.h>

#include <stdio.h>
#include <stdlib.h>

#ifdef SUNOS4
int fprintf(FILE *, char *, ...);
#endif

/* Create and Destroy */
static void ClassInitialize();
static void Initialize(Widget req, Widget newW, 
	ArgList args, Cardinal *nargs);
static void Destroy(Widget w);

/* Geometry, Drawing, Entry and Picking */
static void Realize(Widget w, XtValueMask *valueMask,
	XSetWindowAttributes *attr);
static void Redisplay(Widget w, XExposeEvent *event, Region region);
static void Layout(XmLFolderWidget f, int resizeIfNeeded);
static void LayoutTopBottom(XmLFolderWidget f, int resizeIfNeeded);
static void LayoutLeftRight(XmLFolderWidget f, int resizeIfNeeded);
static void Resize(Widget w);
static XtGeometryResult GeometryManager(Widget w, XtWidgetGeometry *request,
	XtWidgetGeometry *);
static void ChangeManaged(Widget w);
static void ConstraintInitialize(Widget, Widget w,
	ArgList args, Cardinal *nargs);
static void ConstraintDestroy(Widget w);
static void SetActiveTab(XmLFolderWidget f, Widget w, XEvent *event,
	Boolean notify);
static void DrawTabPixmap(XmLFolderWidget f, Widget tab, int active);
static void DrawManagerShadowLeftRight(XmLFolderWidget f, XRectangle *rect);
static void DrawManagerShadowTopBottom(XmLFolderWidget f, XRectangle *rect);
static void DrawTabHighlight(XmLFolderWidget f, Widget w);
static void SetTabPlacement(XmLFolderWidget f, Widget tab);
static void GetTabRect(XmLFolderWidget f, Widget tab, XRectangle *rect,
	int includeShadow);
static void DrawTabShadowArcTopBottom(XmLFolderWidget f, Widget w);
static void DrawTabShadowArcLeftRight(XmLFolderWidget f, Widget w);
static void DrawTabShadowLineTopBottom(XmLFolderWidget f, Widget w);
static void DrawTabShadowLineLeftRight(XmLFolderWidget f, Widget w);
static void DrawTabShadowNoneTopBottom(XmLFolderWidget f, Widget w);
static void DrawTabShadowNoneLeftRight(XmLFolderWidget f, Widget w);
static void SetGC(XmLFolderWidget f, int type);

/* Getting and Setting Values */
static Boolean SetValues(Widget curW, Widget reqW, Widget newW, 
	ArgList args, Cardinal *nargs);
static Boolean ConstraintSetValues(Widget curW, Widget, Widget newW,
	ArgList, Cardinal *); 
static void CopyFontList(XmLFolderWidget f);
static Boolean CvtStringToCornerStyle(Display *dpy, XrmValuePtr args,
	Cardinal *numArgs, XrmValuePtr fromVal, XrmValuePtr toVal,
	XtPointer *data);
static Boolean CvtStringToFolderResizePolicy(Display *dpy, XrmValuePtr args,
	Cardinal *numArgs, XrmValuePtr fromVal, XrmValuePtr toVal,
	XtPointer *data);
static Boolean CvtStringToTabPlacement(Display *dpy, XrmValuePtr args,
	Cardinal *numArgs, XrmValuePtr fromVal, XrmValuePtr toVal,
	XtPointer *data);

/* Utility */
static void GetCoreBackground(Widget w, int, XrmValue *value);
static void GetDefaultTabWidgetClass(Widget w, int, XrmValue *value);
static void GetManagerForeground(Widget w, int, XrmValue *value);
static Boolean ServerDrawsArcsLarge(Display *dpy, int debug);

/* Actions, Callbacks and Handlers */
static void Activate(Widget w, XEvent *event, String *, Cardinal *);
static void PrimActivate(Widget w, XtPointer, XtPointer);
static void PrimFocusIn(Widget w, XEvent *event, String *, Cardinal *);
static void PrimFocusOut(Widget w, XEvent *event, String *, Cardinal *);

static XtActionsRec actions[] =
	{
	{ "XmLFolderActivate",      Activate     },
	{ "XmLFolderPrimFocusIn",   PrimFocusIn  },
	{ "XmLFolderPrimFocusOut",  PrimFocusOut },
	};

#define MAX_TAB_ROWS 64

#define GC_SHADOWBOT  0
#define GC_SHADOWTOP  1
#define GC_BLANK      2
#define GC_UNSET      3

/* Folder Translations */

static char translations[] =
"<Btn1Down>: XmLFolderActivate()\n\
<EnterWindow>:   ManagerEnter()\n\
<LeaveWindow>:   ManagerLeave()\n\
<FocusOut>:      ManagerFocusOut()\n\
<FocusIn>:       ManagerFocusIn()";

/* Primitive Child Translations */

static char primTranslations[] =
"<FocusIn>: XmLFolderPrimFocusIn() PrimitiveFocusIn()\n\
<FocusOut>: XmLFolderPrimFocusOut() PrimitiveFocusOut()";

static XtResource resources[] =
	{
		/* Folder Resources */
		{
		XmNtabWidgetClass, XmCTabWidgetClass,
		XmRWidgetClass, sizeof(WidgetClass),
		XtOffset(XmLFolderWidget, folder.tabWidgetClass),
		XmRCallProc, (XtPointer)GetDefaultTabWidgetClass,
		},
		{
		XmNactivateCallback, XmCCallback,
		XmRCallback, sizeof(XtCallbackList),
		XtOffset(XmLFolderWidget, folder.activateCallback),
		XmRImmediate, (XtPointer)0,
		},
		{
		XmNactiveTab, XmCActiveTab,
		XmRInt, sizeof(int),
		XtOffset(XmLFolderWidget, folder.activeTab),
		XmRImmediate, (XtPointer)-1,
		},
		{
		XmNautoSelect, XmCAutoSelect,
		XmRBoolean, sizeof(Boolean),
		XtOffset(XmLFolderWidget, folder.autoSelect),
		XmRImmediate, (XtPointer)True,
		},
		{
		XmNblankBackground, XmCBlankBackground,
		XmRPixel, sizeof(Pixel),
		XtOffset(XmLFolderWidget, folder.blankBg),
		XmRCallProc, (XtPointer)GetCoreBackground,
		},
		{
		XmNblankBackgroundPixmap, XmCBlankBackgroundPixmap,
		XmRManForegroundPixmap, sizeof(Pixmap),
		XtOffset(XmLFolderWidget, folder.blankPix),
		XmRImmediate, (XtPointer)XmUNSPECIFIED_PIXMAP,
		},
		{
		XmNcornerDimension, XmCCornerDimension,
		XmRDimension, sizeof(Dimension),
		XtOffset(XmLFolderWidget, folder.cornerDimension),
		XmRImmediate, (XtPointer)2,
		},
		{
		XmNcornerStyle, XmCCornerStyle,
		XmRCornerStyle, sizeof(unsigned char),
		XtOffset(XmLFolderWidget, folder.cornerStyle),
		XmRImmediate, (XtPointer)XmCORNER_ARC,
		},
		{
		XmNfontList, XmCFontList,
		XmRFontList, sizeof(XmFontList),
		XtOffset(XmLFolderWidget, folder.fontList),
		XmRImmediate, (XtPointer)0,
		},
		{
		XmNhighlightThickness, XmCHighlightThickness,
		XmRDimension, sizeof(Dimension),
		XtOffset(XmLFolderWidget, folder.highlightThickness),
		XmRImmediate, (XtPointer)2,
		},
		{
		XmNinactiveBackground, XmCInactiveBackground,
		XmRPixel, sizeof(Pixel),
		XtOffset(XmLFolderWidget, folder.inactiveBg),
		XmRCallProc, (XtPointer)GetCoreBackground,
		},
		{
		XmNinactiveForeground, XmCInactiveForeground,
		XmRPixel, sizeof(Pixel),
		XtOffset(XmLFolderWidget, folder.inactiveFg),
		XmRCallProc, (XtPointer)GetManagerForeground,
		},
		{
		XmNmarginHeight, XmCMarginHeight,
		XmRDimension, sizeof(Dimension),
		XtOffset(XmLFolderWidget, folder.marginHeight),
		XmRImmediate, (XtPointer)0,
		},
		{
		XmNmarginWidth, XmCMarginWidth,
		XmRDimension, sizeof(Dimension),
		XtOffset(XmLFolderWidget, folder.marginWidth),
		XmRImmediate, (XtPointer)0,
		},
		{
		XmNpixmapMargin, XmCPixmapMargin,
		XmRDimension, sizeof(Dimension),
		XtOffset(XmLFolderWidget, folder.pixmapMargin),
		XmRImmediate, (XtPointer)2,
		},
		{
		XmNresizePolicy, XmCFolderResizePolicy,
		XmRFolderResizePolicy, sizeof(unsigned char),
		XtOffset(XmLFolderWidget, folder.resizePolicy),
		XmRImmediate, (XtPointer)XmRESIZE_STATIC,
		},
		{
		XmNrotateWhenLeftRight, XmCRotateWhenLeftRight,
		XmRBoolean, sizeof(Boolean),
		XtOffset(XmLFolderWidget, folder.allowRotate),
		XmRImmediate, (XtPointer)True,
		},
		{
		XmNspacing, XmCSpacing,
		XmRDimension, sizeof(Dimension),
		XtOffset(XmLFolderWidget, folder.spacing),
		XmRImmediate, (XtPointer)0,
		},
		{
		XmNtabBarHeight, XmCTabBarHeight,
		XmRDimension, sizeof(Dimension),
		XtOffset(XmLFolderWidget, folder.tabBarHeight),
		XmRImmediate, (XtPointer)0,
		},
		{
		XmNtabCount, XmCTabCount,
		XmRInt, sizeof(int),
		XtOffset(XmLFolderWidget, folder.tabCount),
		XmRImmediate, (XtPointer)0,
		},
		{
		XmNtabPlacement, XmCTabPlacement,
		XmRTabPlacement, sizeof(unsigned char),
		XtOffset(XmLFolderWidget, folder.tabPlacement),
		XmRImmediate, (XtPointer)XmFOLDER_TOP,
		},
		{
		XmNtabsPerRow, XmCTabsPerRow,
		XmRInt, sizeof(int),
		XtOffset(XmLFolderWidget, folder.tabsPerRow),
		XmRImmediate, (XtPointer)0,
		},
		{
		XmNtabWidgetList, XmCReadOnly,
		XmRPointer, sizeof(XtPointer),
		XtOffset(XmLFolderWidget, folder.tabs),
		XmRImmediate, (XtPointer)0,
		},
		{
		XmNtabTranslations, XmCTranslations,
		XmRTranslationTable, sizeof(XtTranslations),
		XtOffset(XmLFolderWidget, folder.primTrans),
		XmRString, (XtPointer)primTranslations,
		},
		{
		XmNdebugLevel, XmCDebugLevel,
		XmRInt, sizeof(int),
		XtOffset(XmLFolderWidget, folder.debugLevel),
		XmRImmediate, (XtPointer)0,
		},
		/* Overridden inherited resources */
		{
		XmNshadowThickness, XmCShadowThickness,
		XmRHorizontalDimension, sizeof(Dimension),
		XtOffset(XmLFolderWidget, manager.shadow_thickness),
		XmRImmediate, (XtPointer)2,
		},
    };

static XtResource constraint_resources[] =
	{
		/* Folder Constraint Resources */
		{
		XmNtabFreePixmaps, XmCTabFreePixmaps,
		XmRBoolean, sizeof(Boolean),
		XtOffset(XmLFolderConstraintPtr, folder.freePix),
		XmRImmediate, (XtPointer)False,
		},
		{
		XmNtabInactivePixmap, XmCTabInactivePixmap,
		XmRPrimForegroundPixmap, sizeof(Pixmap),
		XtOffset(XmLFolderConstraintPtr, folder.inactPix),
		XmRImmediate, (XtPointer)XmUNSPECIFIED_PIXMAP,
		},
		{
		XmNtabManagedName, XmCTabManagedName,
		XmRString, sizeof(char *),
		XtOffset(XmLFolderConstraintPtr, folder.managedName),
		XmRImmediate, (XtPointer)0,
		},
		{
		XmNtabManagedWidget, XmCTabManagedWidget,
		XmRWidget, sizeof(Widget),
		XtOffset(XmLFolderConstraintPtr, folder.managedW),
		XmRImmediate, (XtPointer)0,
		},
		{
		XmNtabPixmap, XmCTabPixmap,
		XmRPrimForegroundPixmap, sizeof(Pixmap),
		XtOffset(XmLFolderConstraintPtr, folder.pix),
		XmRImmediate, (XtPointer)XmUNSPECIFIED_PIXMAP,
		},
	};

XmLFolderClassRec xmlFolderClassRec =
	{
		{ /* core_class */
		(WidgetClass)&xmManagerClassRec,          /* superclass         */
		"XmLFolder",                              /* class_name         */
		sizeof(XmLFolderRec),                     /* widget_size        */
		ClassInitialize,                          /* class_init         */
		0,                                        /* class_part_init    */
		FALSE,                                    /* class_inited       */
		(XtInitProc)Initialize,                   /* initialize         */
		0,                                        /* initialize_hook    */
		(XtRealizeProc)Realize,                   /* realize            */
		(XtActionList)actions,                    /* actions            */
		(Cardinal)XtNumber(actions),              /* num_actions        */
		resources,                                /* resources          */
		XtNumber(resources),                      /* num_resources      */
		NULLQUARK,                                /* xrm_class          */
		TRUE,                                     /* compress_motion    */
		XtExposeCompressMultiple,                 /* compress_exposure  */
		TRUE,                                     /* compress_enterlv   */
		TRUE,                                     /* visible_interest   */
		(XtWidgetProc)Destroy,                    /* destroy            */
		(XtWidgetProc)Resize,                     /* resize             */
		(XtExposeProc)Redisplay,                  /* expose             */
		(XtSetValuesFunc)SetValues,               /* set_values         */
		0,                                        /* set_values_hook    */
		XtInheritSetValuesAlmost,                 /* set_values_almost  */
		0,                                        /* get_values_hook    */
		0,                                        /* accept_focus       */
		XtVersion,                                /* version            */
		0,                                        /* callback_private   */
		translations,                             /* tm_table           */
		0,                                        /* query_geometry     */
		0,                                        /* display_acceleratr */
		0,                                        /* extension          */
		},
		{ /* composite_class */
		(XtGeometryHandler)GeometryManager,       /* geometry_manager   */
		(XtWidgetProc)ChangeManaged,              /* change_managed     */
		XtInheritInsertChild,                     /* insert_child       */
		XtInheritDeleteChild,                     /* delete_child       */
		0,                                        /* extension          */
		},
		{ /* constraint_class */
		constraint_resources,	                  /* subresources       */
		XtNumber(constraint_resources),           /* subresource_count  */
		sizeof(XmLFolderConstraintRec),           /* constraint_size    */
		(XtInitProc)ConstraintInitialize,         /* initialize         */
		(XtWidgetProc)ConstraintDestroy,          /* destroy            */
		(XtSetValuesFunc)ConstraintSetValues,     /* set_values         */
		0,                                        /* extension          */
		},
		{ /* manager_class */
		XtInheritTranslations,                    /* translations       */
		0,                                        /* syn resources      */
		0,                                        /* num syn_resources  */
		0,                                        /* get_cont_resources */
		0,                                        /* num_get_cont_resrc */
		XmInheritParentProcess,                   /* parent_process     */
		0,                                        /* extension          */
		},
		{ /* folder_class */
		0,                                        /* unused             */
		}
	};

WidgetClass xmlFolderWidgetClass = (WidgetClass)&xmlFolderClassRec;

/*
   Create and Destroy
*/

static void 
ClassInitialize(void)
{
  XmLInitialize();
  
  XtSetTypeConverter(XmRString, XmRCornerStyle,
		     CvtStringToCornerStyle, 0, 0, XtCacheNone, 0);
  XtSetTypeConverter(XmRString, XmRFolderResizePolicy,
		     CvtStringToFolderResizePolicy, 0, 0, XtCacheNone, 0);
  XtSetTypeConverter(XmRString, XmRTabPlacement,
		     CvtStringToTabPlacement, 0, 0, XtCacheNone, 0);
}

static void 
Initialize(Widget req, 
	   Widget newW, 
	   ArgList args, 
	   Cardinal *narg) 
{
  Display *dpy;
  /*  Window root;*/
  XmLFolderWidget f, request;
  
  f = (XmLFolderWidget)newW;
  dpy = XtDisplay((Widget)f);
  request = (XmLFolderWidget)req;
  
  if (f->core.width <= 0) 
    f->core.width = 100;
  if (f->core.height <= 0) 
    f->core.height = 100;
  
  f->folder.gc = 0;
  
  f->folder.tabAllocCount = 32;
  f->folder.tabs = (Widget *)malloc(sizeof(Widget) * 32);
  f->folder.tabHeight = 0;
  f->folder.tabWidth = 0;
  f->folder.activeW = 0;
  f->folder.focusW = 0;
  f->folder.allowLayout = 1;
  f->folder.activeRow = -1;
  CopyFontList(f);
  
  if (f->folder.tabBarHeight)
    {
      XmLWarning((Widget)f, "Initialize() - can't set tabBarHeight");
      f->folder.tabBarHeight = 0;
    }
  if (f->folder.tabCount)
    {
      XmLWarning((Widget)f, "Initialize() - can't set tabCount");
      f->folder.tabCount = 0;
    }
  if (f->folder.activeTab != -1)
    {
      XmLWarning((Widget)f, "Initialize() - can't set activeTab");
      f->folder.activeTab = -1;
    }
  if (f->folder.cornerDimension < 1)
    {
      XmLWarning((Widget)f, "Initialize() - cornerDimension can't be < 1");
      f->folder.cornerDimension = 1;
    }
  f->folder.serverDrawsArcsLarge = ServerDrawsArcsLarge(dpy,
							f->folder.debugLevel);
}

static void 
Destroy(Widget w)
{
  XmLFolderWidget f;
  Display *dpy;
  
  f = (XmLFolderWidget)w;
  dpy = XtDisplay(w);
  if (f->folder.debugLevel)
    fprintf(stderr, "Folder destroy: \n");
  if (f->folder.tabs)
    free((char *)f->folder.tabs);
  if (f->folder.gc)
    XFreeGC(dpy, f->folder.gc);
  XmFontListFree(f->folder.fontList);
}

/*
  Geometry, Drawing, Entry and Picking
  */

static void 
Realize(Widget w, 
	XtValueMask *valueMask, 
	XSetWindowAttributes *attr)
{
  XmLFolderWidget f;
  Display *dpy;
  WidgetClass superClass;
  XtRealizeProc realize;
  XGCValues values;
  XtGCMask mask;
  
  f = (XmLFolderWidget)w;
  dpy = XtDisplay(f);
  superClass = xmlFolderWidgetClass->core_class.superclass;
  realize = superClass->core_class.realize;
  (*realize)(w, valueMask, attr);
  
  if (!f->folder.gc)
    {
      values.foreground = f->manager.foreground;
      mask = GCForeground;
      f->folder.gc = XCreateGC(dpy, XtWindow(f), mask, &values);
      if (f->folder.autoSelect == True && f->folder.tabCount)
	XmLFolderSetActiveTab(w, 0, False);
    }
}

static void 
Redisplay(Widget w, 
	  XExposeEvent *event, 
	  Region region)
{
  Display *dpy;
  Window win;
  XmLFolderWidget f;
  XmLFolderConstraintRec *fc;
  XRectangle eRect, rRect, rect;
  /* XSegment *topSeg, *botSeg; */
  /*  int tCount, bCount; */
  Widget tab;
  int i, st, ht; /*, x, y; */
  
  f = (XmLFolderWidget)w;
  if (!XtIsRealized(w))
    return;
  if (!f->core.visible)
    return;
  dpy = XtDisplay(f);
  win = XtWindow(f);
  st = f->manager.shadow_thickness;
  ht = f->folder.highlightThickness;
  
  if (event)
    {
      eRect.x = event->x;
      eRect.y = event->y;
      eRect.width = event->width;
      eRect.height = event->height;
      if (f->folder.debugLevel > 1)
	fprintf(stderr, "XmLFolder: Redisplay x %d y %d w %d h %d\n",
		event->x, event->y, event->width, event->height);
    }
  else
    {
      eRect.x = 0;
      eRect.y = 0;
      eRect.width = f->core.width;
      eRect.height = f->core.height;
    }
  if (!eRect.width || !eRect.height)
    return;
  
  if (f->folder.tabPlacement == XmFOLDER_TOP)
    {
      rRect.x = 0;
      rRect.y = f->folder.tabHeight;
      rRect.width = f->core.width;
      rRect.height = f->core.height - f->folder.tabHeight;
    }
  else if (f->folder.tabPlacement == XmFOLDER_BOTTOM)
    {
      rRect.x = 0;
      rRect.y = 0;
      rRect.width = f->core.width;
      rRect.height = f->core.height - f->folder.tabHeight;
    }
  if (f->folder.tabPlacement == XmFOLDER_LEFT)
    {
      rRect.x = f->folder.tabWidth;
      rRect.y = 0;
      rRect.width = f->core.width - f->folder.tabWidth;
      rRect.height = f->core.height;
    }
  if (f->folder.tabPlacement == XmFOLDER_RIGHT)
    {
      rRect.x = 0;
      rRect.y = 0;
      rRect.width = f->core.width - f->folder.tabWidth;
      rRect.height = f->core.height;
    }
  if (XmLRectIntersect(&eRect, &rRect) != XmLRectOutside)
    {
      if (f->folder.tabPlacement == XmFOLDER_TOP ||
	  f->folder.tabPlacement == XmFOLDER_BOTTOM)
	DrawManagerShadowTopBottom(f, &rRect);
      else
	DrawManagerShadowLeftRight(f, &rRect);
    }

  if (!f->folder.tabCount)
    return;

  rRect.x = 0;
  rRect.y = 0;
  rRect.width = 0;
  rRect.height = 0;

	/* Draw tabs */
  for (i = 0; i < f->folder.tabCount; i++)
    {
      tab = f->folder.tabs[i];
      if (!XtIsManaged(tab))
	continue;
      fc = (XmLFolderConstraintRec *)(tab->core.constraints);
      GetTabRect(f, tab, &rRect, 0);

      /* include spacing in intersect test */
      rect = rRect;
      if (f->folder.tabPlacement == XmFOLDER_TOP ||
	  f->folder.tabPlacement == XmFOLDER_BOTTOM)
	rect.width += f->folder.spacing;
      else
	rect.height += f->folder.spacing;

      /* include indent in intersect test */
      if (f->folder.tabsPerRow)
	{
	  if (rRect.x == 2)
	    rect.x = 0;
	  if (rRect.y == 2)
	    rect.y = 0;
	  if (rRect.x + rRect.width == f->core.width - 2)
	    rect.width += 2;
	  if (rRect.y + rRect.height == f->core.height - 2)
	    rect.height += 2;
	}

      if (XmLRectIntersect(&eRect, &rect) == XmLRectOutside)
	continue;
      if (event && XRectInRegion(region, rect.x, rect.y,
				 rect.width, rect.height) == RectangleOut)
	continue;

      if (f->folder.debugLevel > 1)
	fprintf(stderr, "XmLFolder: Redisplay tab for widget %d\n", i);
      if (tab == f->folder.activeW)
	{
	  XtVaSetValues(tab,
			XmNbackground, f->core.background_pixel,
			XmNforeground, f->manager.foreground,
			NULL);
	}
      else
	{
	  XSetForeground(dpy, f->folder.gc, f->folder.inactiveBg);
	  XFillRectangle(dpy, win, f->folder.gc,
			 rRect.x, rRect.y, rRect.width, rRect.height);
	  XtVaSetValues(tab,
			XmNbackground, f->folder.inactiveBg,
			XmNforeground, f->folder.inactiveFg,
			NULL);
	}

      if (f->folder.tabPlacement == XmFOLDER_TOP ||
	  f->folder.tabPlacement == XmFOLDER_BOTTOM)
	{
	  if (f->folder.cornerStyle == XmCORNER_LINE)
	    DrawTabShadowLineTopBottom(f, tab);
	  else if (f->folder.cornerStyle == XmCORNER_ARC)
	    DrawTabShadowArcTopBottom(f, tab);
	  else
	    DrawTabShadowNoneTopBottom(f, tab);
	}
      else
	{
	  if (f->folder.cornerStyle == XmCORNER_LINE)
	    DrawTabShadowLineLeftRight(f, tab);
	  else if (f->folder.cornerStyle == XmCORNER_ARC)
	    DrawTabShadowArcLeftRight(f, tab);
	  else
	    DrawTabShadowNoneLeftRight(f, tab);
	}

      if (f->folder.focusW == tab)
	DrawTabHighlight(f, tab);

      if (tab == f->folder.activeW &&
	  fc->folder.pix != XmUNSPECIFIED_PIXMAP &&
	  (fc->folder.maxPixWidth || fc->folder.maxPixHeight))
	DrawTabPixmap(f, tab, 1);
      else if (tab != f->folder.activeW &&
	       fc->folder.inactPix != XmUNSPECIFIED_PIXMAP &&
	       (fc->folder.maxPixWidth || fc->folder.maxPixHeight))
	DrawTabPixmap(f, tab, 0);

      SetGC(f, GC_BLANK);

      /* draw indent */
      if (f->folder.tabsPerRow)
	{
	  if (rRect.x == 2)
	    {
	      rect = rRect;
	      rect.x = 0;
	      rect.width = 2;
	      XFillRectangle(dpy, win, f->folder.gc, rect.x, rect.y,
			     rect.width, rect.height);
	    }
	  if (rRect.y == 2)
	    {
	      rect = rRect;
	      rect.y = 0;
	      rect.height = 2;
	      XFillRectangle(dpy, win, f->folder.gc, rect.x, rect.y,
			     rect.width, rect.height);
	    }
	  if (rRect.x + rRect.width == f->core.width - 2)
	    {
	      rect = rRect;
	      rect.x = f->core.width - 2;
	      rect.width = 2;
	      XFillRectangle(dpy, win, f->folder.gc, rect.x, rect.y,
			     rect.width, rect.height);
	    }
	  if (rRect.y + rRect.height == f->core.height - 2)
	    {
	      rect = rRect;
	      rect.y = f->core.height - 2;
	      rect.height = 2;
	      XFillRectangle(dpy, win, f->folder.gc, rect.x, rect.y,
			     rect.width, rect.height);
	    }
	}

      if (f->folder.spacing)
	{
	  if (f->folder.tabPlacement == XmFOLDER_TOP ||
	      f->folder.tabPlacement == XmFOLDER_BOTTOM)
	    XFillRectangle(dpy, win, f->folder.gc, rRect.x + rRect.width,
			   rRect.y, f->folder.spacing, rRect.height);
	  else
	    XFillRectangle(dpy, win, f->folder.gc, rRect.x,
			   rRect.y + rRect.height, rRect.width, f->folder.spacing);
	}

      SetGC(f, GC_UNSET);
    }

  /* Draw empty area */
  if (!f->folder.tabsPerRow)
    {
      if (f->folder.tabPlacement == XmFOLDER_TOP ||
	  f->folder.tabPlacement == XmFOLDER_BOTTOM)
	{
	  rRect.x += rRect.width + f->folder.spacing;
	  if ((int)f->core.width > rRect.x)
	    {
	      if (f->folder.tabPlacement == XmFOLDER_TOP)
		rRect.y = 0;
	      else
		rRect.y = f->core.height - f->folder.tabHeight;
	      rRect.width = f->core.width - rRect.x;
	      rRect.height = f->folder.tabHeight;
	      SetGC(f, GC_BLANK);
	      XFillRectangle(dpy, win, f->folder.gc,
			     rRect.x, rRect.y, rRect.width, rRect.height);
	      SetGC(f, GC_UNSET);
	    }
	}
      else
	{
	  rRect.y += rRect.height + f->folder.spacing;
	  if ((int)f->core.height > rRect.y)
	    {
	      if (f->folder.tabPlacement == XmFOLDER_LEFT)
		rRect.x = 0;
	      else
		rRect.x = f->core.width - f->folder.tabWidth;
	      rRect.width = f->folder.tabWidth;
	      rRect.height = f->core.height - rRect.y;
	      SetGC(f, GC_BLANK);
	      XFillRectangle(dpy, win, f->folder.gc,
			     rRect.x, rRect.y, rRect.width, rRect.height);
	      SetGC(f, GC_UNSET);
	    }
	}
    }
}

static void 
Layout(XmLFolderWidget f, 
       int resizeIfNeeded)
{
  /*  Window win;*/

  if (!f->folder.allowLayout)
    return;
  f->folder.allowLayout = 0;
  if (f->folder.tabPlacement == XmFOLDER_LEFT ||
      f->folder.tabPlacement == XmFOLDER_RIGHT)
    LayoutLeftRight(f, resizeIfNeeded);
  else
    LayoutTopBottom(f, resizeIfNeeded);
  if (XtIsRealized((Widget)f) && f->core.visible)
    XClearArea(XtDisplay(f), XtWindow(f), 0, 0, 0, 0, True);
  f->folder.allowLayout = 1;
}

static void 
LayoutTopBottom(XmLFolderWidget f, 
		int resizeIfNeeded)
{
  Display *dpy;
  Window root;
  int i, tabNum, x, y, w, h, pad1, pad2;
  int rowNum, numRows, rowHeight, rowX, rowY;
  WidgetList children;
  Widget tab, child;
  XmLFolderConstraintRec *fc;
  XtGeometryResult result;
  unsigned int inactPixHeight, pixHeight;
  unsigned int inactPixWidth, pixWidth;
  unsigned int pixBW, pixDepth;
  Dimension height, minHeight;
  Dimension width, minWidth, borderWidth;
  Dimension co;
  int st, ht;
  Boolean map, isManaged;
  struct
  {
    int width, height, numTabs, y;
  } rows[MAX_TAB_ROWS];

  dpy = XtDisplay(f);
  children = f->composite.children;
  st = f->manager.shadow_thickness;
  ht = f->folder.highlightThickness;

  /* calculate corner offset */
  if (f->folder.cornerStyle == XmCORNER_LINE)
    co = (Dimension)((double)f->folder.cornerDimension * .5 + .99);
  else if (f->folder.cornerStyle == XmCORNER_ARC)
    co = (Dimension)((double)f->folder.cornerDimension * .3 + .99);
  else
    co = 0;

  /* caculate tabHeight, minWidth, minHeight, row y positions, */
  /* row heights and tab pixmap dimensions */
  rowX = 0;
  rowY = 0;
  rowHeight = 0;
  rowNum = 0;
  tabNum = 0;
  minWidth = 0;
  for (i = 0; i < f->folder.tabCount; i++)
    {
      tab = f->folder.tabs[i];
      if (!XtIsManaged(tab))
	continue;
      fc = (XmLFolderConstraintRec *)(tab->core.constraints);

      /* check for start of a new row */
      fc->folder.firstInRow = False;
      if (!tabNum)
	fc->folder.firstInRow = True;
      if (f->folder.tabsPerRow && tabNum == f->folder.tabsPerRow)
	{
	  fc->folder.firstInRow = True;

	  /* store prev row values and start next row */
	  if (rowX)
	    rowX -= f->folder.spacing;
	  rows[rowNum].y = rowY;
	  rows[rowNum].width = rowX;
	  rows[rowNum].height = rowHeight;
	  rows[rowNum].numTabs = tabNum;
	  if (f->folder.debugLevel)
	    {
	      fprintf(stderr, "XmLFolder: Layout: ");
	      fprintf(stderr, "row %d: y %d w %d h %d numTabs %d\n",
		      rowNum, rowY, rowX, rowHeight, tabNum);
	    }
	  rowY += rowHeight;
	  rowHeight = 0;
	  if (rowX > (int)minWidth)
	    minWidth = rowX;
	  rowX = 0;
	  tabNum = 0;
	  rowNum++;
	  if (rowNum == MAX_TAB_ROWS - 1)
	    {
	      XmLWarning((Widget)f, "Layout ERROR - too many rows\n");
	      return;
	    }
	}

      /* make sure row height > maximum tab height */
      height = co + st + tab->core.height + tab->core.border_width * 2 +
	f->folder.marginHeight * 2 + ht * 2;
      if ((int)height > rowHeight)
	rowHeight = height;

      /* calc pixmap dimensions/maximum pixmap height */
      fc->folder.pixWidth = 0;
      fc->folder.pixHeight = 0;
      fc->folder.inactPixWidth = 0;
      fc->folder.inactPixHeight = 0;
      fc->folder.maxPixWidth = 0;
      fc->folder.maxPixHeight = 0;
      if (fc->folder.pix != XmUNSPECIFIED_PIXMAP)
	{
	  XGetGeometry(dpy, fc->folder.pix, &root,
		       &x, &y, &pixWidth, &pixHeight, &pixBW, &pixDepth);
	  fc->folder.pixWidth = pixWidth;
	  fc->folder.maxPixWidth = pixWidth;
	  fc->folder.pixHeight = pixHeight;
	  fc->folder.maxPixHeight = pixHeight;
	  height = co + st + pixHeight + f->folder.marginHeight * 2 + ht * 2;
	  if ((int)height > rowHeight)
	    rowHeight = height;
	}
      if (fc->folder.inactPix != XmUNSPECIFIED_PIXMAP)
	{
	  XGetGeometry(dpy, fc->folder.inactPix, &root, &x, &y,
		       &inactPixWidth, &inactPixHeight, &pixBW, &pixDepth);
	  fc->folder.inactPixWidth = inactPixWidth;
	  if (inactPixWidth > fc->folder.maxPixWidth)
	    fc->folder.maxPixWidth = inactPixWidth;
	  fc->folder.inactPixHeight = inactPixHeight;
	  if (inactPixHeight > fc->folder.maxPixHeight)
	    fc->folder.maxPixHeight = inactPixHeight;
	  height = co + st + inactPixHeight +
	    f->folder.marginHeight * 2 + ht * 2;
	  if ((int)height > rowHeight)
	    rowHeight = height;
	}

      /* increment rowX to move on to the next tab */
      rowX += st * 2 + co * 2 + f->folder.marginWidth * 2 + ht * 2 +
	XtWidth(tab) + tab->core.border_width * 2;
      if (fc->folder.maxPixWidth)
	rowX += fc->folder.maxPixWidth + f->folder.pixmapMargin;
      rowX += f->folder.spacing;

      tabNum++;
      fc->folder.row = rowNum;
    }

  /* complete calcuations for last row */
  if (rowX)
    rowX -= f->folder.spacing;
  rows[rowNum].y = rowY;
  rows[rowNum].width = rowX;
  rows[rowNum].height = rowHeight;
  rows[rowNum].numTabs = tabNum;
  numRows = rowNum + 1;
  if (f->folder.debugLevel)
    {
      fprintf(stderr, "XmLFolder: Layout: ");
      fprintf(stderr, "row %d: y %d w %d h %d numTabs %d\n",
	      rowNum, rowY, rowX, rowHeight, tabNum);
    }
  f->folder.tabHeight = rowY + rowHeight;
  f->folder.tabBarHeight = f->folder.tabHeight;
  minHeight = f->folder.tabHeight;
  if ((int)minWidth < rowX)
    minWidth = rowX;

  /* add space for indent of upper rows */
  if (f->folder.tabsPerRow && minWidth)
    minWidth += 4;

  if (f->folder.debugLevel)
    {
      fprintf(stderr, "XmLFolder: Layout: ");
      fprintf(stderr, "tab bar minimum w %d h %d\n",
	      (int)minWidth, (int)minHeight);
    }

  /* calculate width and height of non-tab children ensure */
  /* minWidth > width and minHeight > height */
  for (i = 0; i < f->composite.num_children; i++)
    {
      child = children[i];
      if (XtIsSubclass(child, xmPrimitiveWidgetClass))
	continue;

      height = XtHeight(child) + f->folder.tabHeight + st * 2;
      if (XtIsWidget(child))
	height += child->core.border_width * 2;
      if (height > minHeight)
	minHeight = height;

      width = XtWidth(child) + st * 2;
      if (XtIsWidget(child))
	width += child->core.border_width * 2;
      if (width > minWidth)
	minWidth = width;
    }

  if (f->folder.debugLevel)
    {
      fprintf(stderr, "XmLFolder: Layout: ");
      fprintf(stderr, "with non-tabs minimum w %d h %d\n",
	      (int)minWidth, (int)minHeight);
    }

  /* Resize folder if needed */
  if (resizeIfNeeded && f->folder.resizePolicy != XmRESIZE_NONE)
    {
      if (minWidth <= 0)
	minWidth = 1;
      if (minHeight <= 0)
	minHeight = 1;
      result = XtMakeResizeRequest((Widget)f, minWidth, minHeight,
				   &width, &height);
      if (result == XtGeometryAlmost)
	XtMakeResizeRequest((Widget)f, width, height, NULL, NULL);
    }

  /* move active row to bottom */
  tab = f->folder.activeW;
  if (tab)
    {
      fc = (XmLFolderConstraintRec *)(tab->core.constraints);
      rowNum = fc->folder.row;
      f->folder.activeRow = rowNum;
      rows[rowNum].y = f->folder.tabHeight - rows[rowNum].height;
      for (i = rowNum + 1; i < numRows; i++)
	rows[i].y -= rows[rowNum].height;
    }
  else
    f->folder.activeRow = -1;

  /* configure tab children */
  for (i = 0; i < f->folder.tabCount; i++)
    {
      tab = f->folder.tabs[i];
      if (!XtIsManaged(tab))
	continue;
      fc = (XmLFolderConstraintRec *)(tab->core.constraints);
      rowNum = fc->folder.row;

      /* calculate tab x */
      if (fc->folder.firstInRow == True)
	{
	  if (f->folder.tabsPerRow && rowNum != f->folder.activeRow)
	    x = 2;
	  else
	    x = 0;
	}
      fc->folder.x = x;
      x += st + co + f->folder.marginWidth + ht;
      if (fc->folder.maxPixWidth)
	x += fc->folder.maxPixWidth + f->folder.pixmapMargin;

      /* calculate tab y and tab height */
      fc->folder.height = rows[rowNum].height;
      if (f->folder.tabPlacement == XmFOLDER_TOP)
	{
	  fc->folder.y = rows[rowNum].y;
	  y = fc->folder.y + fc->folder.height - f->folder.marginHeight -
	    ht - XtHeight(tab) - tab->core.border_width * 2;
	}
      else
	{
	  fc->folder.y = f->core.height - rows[rowNum].y -
	    rows[rowNum].height;
	  y = fc->folder.y + f->folder.marginHeight + ht;
	}

      /* calculate tab padding */
      pad1 = 0;
      pad2 = 0;
      w = f->core.width - rows[rowNum].width;
      if (rowNum != f->folder.activeRow)
	w -= 4;
      if (f->folder.tabsPerRow && w > 0)
	{
	  pad1 = w / (rows[rowNum].numTabs * 2);
	  pad2 = pad1;
	  if (fc->folder.firstInRow == True)
	    pad2 += w - (pad1 * rows[rowNum].numTabs * 2);
	}
      x += pad1;

      /* move tab widget into place */
      XtMoveWidget(tab, x, y);

      /* calculate tab width and move to next tab */
      x += pad2 + XtWidth(tab) + tab->core.border_width * 2 + ht +
	f->folder.marginWidth + co + st;
      fc->folder.width = x - fc->folder.x; 
      x += f->folder.spacing;
    }

  /* configure non-tab children */
  for (i = 0; i < f->composite.num_children; i++)
    {
      child = children[i];
      if (XtIsSubclass(child, xmPrimitiveWidgetClass))
	continue;
      if (f->folder.resizePolicy == XmRESIZE_NONE)
	continue;

      w = (int)f->core.width - st * 2;
      h = (int)f->core.height - (int)f->folder.tabHeight - st * 2;
      if (h <= 0 || w <= 0)
	continue;
      /* manager widgets will not configure correctly unless they */
      /* are managed, so manage then unmapped if they are unmanaged */
      isManaged = True;
      if (!XtIsManaged(child))
	{
	  XtVaGetValues(child,
			XmNmappedWhenManaged, &map,
			NULL);
	  XtVaSetValues(child,
			XmNmappedWhenManaged, False,
			NULL);
	  XtManageChild(child);
	  isManaged = False;
	}
      x = st;
      if (f->folder.tabPlacement == XmFOLDER_TOP)
	y = f->folder.tabHeight + st;
      else
	y = st;
      width = w;
      height = h;
      borderWidth = 0;
      if (XtIsWidget(child))
	borderWidth = child->core.border_width;
      XtConfigureWidget(child, x, y, width, height, borderWidth);
      if (isManaged == False)
	{
	  XtUnmanageChild(child);
	  XtVaSetValues(child, XmNmappedWhenManaged, map, NULL);
	}
    }
}

static void 
LayoutLeftRight(XmLFolderWidget f, 
		int resizeIfNeeded)
{
  Display *dpy;
  Window root;
  int i, tabNum, x, y, w, h, pad1, pad2;
  int rowNum, numRows, rowWidth, rowX, rowY;
  WidgetList children;
  Widget tab, child;
  XmLFolderConstraintRec *fc;
  XtGeometryResult result;
  unsigned int inactPixHeight, pixHeight;
  unsigned int inactPixWidth, pixWidth;
  unsigned int pixBW, pixDepth;
  Dimension height, minHeight;
  Dimension width, minWidth, borderWidth;
  Dimension co;
  int st, ht;
  Boolean map, isManaged;
  struct
  {
    int width, height, numTabs, x;
  } rows[MAX_TAB_ROWS];

  dpy = XtDisplay(f);
  children = f->composite.children;
  st = f->manager.shadow_thickness;
  ht = f->folder.highlightThickness;

  /* calculate corner offset */
  if (f->folder.cornerStyle == XmCORNER_LINE)
    co = (Dimension)((double)f->folder.cornerDimension * .5 + .99);
  else if (f->folder.cornerStyle == XmCORNER_ARC)
    co = (Dimension)((double)f->folder.cornerDimension * .3 + .99);
  else
    co = 0;

  /* caculate tabWidth, minWidth, minHeight, row x positions, */
  /* row widths and tab pixmap dimensions */
  rowX = 0;
  rowY = 0;
  rowWidth = 0;
  rowNum = 0;
  tabNum = 0;
  minHeight = 0;
  for (i = 0; i < f->folder.tabCount; i++)
    {
      tab = f->folder.tabs[i];
      if (!XtIsManaged(tab))
	continue;
      fc = (XmLFolderConstraintRec *)(tab->core.constraints);

      /* check for start of a new row */
      fc->folder.firstInRow = False;
      if (!tabNum)
	fc->folder.firstInRow = True;
      if (f->folder.tabsPerRow && tabNum == f->folder.tabsPerRow)
	{
	  fc->folder.firstInRow = True;

	  /* store prev row values and start next row */
	  if (rowY)
	    rowY -= f->folder.spacing;
	  rows[rowNum].x = rowX;
	  rows[rowNum].height = rowY;
	  rows[rowNum].width = rowWidth;
	  rows[rowNum].numTabs = tabNum;
	  if (f->folder.debugLevel)
	    {
	      fprintf(stderr, "XmLFolder: Layout: ");
	      fprintf(stderr, "row %d: x %d w %d h %d numTabs %d\n",
		      rowNum, rowX, rowWidth, rowY, tabNum);
	    }
	  rowX += rowWidth;
	  rowWidth = 0;
	  if (rowY > (int)minHeight)
	    minHeight = rowY;
	  rowY = 0;
	  tabNum = 0;
	  rowNum++;
	  if (rowNum == MAX_TAB_ROWS - 1)
	    {
	      XmLWarning((Widget)f, "Layout ERROR - too many rows\n");
	      return;
	    }
	}

      /* make sure row width > maximum tab width */
      width = co + st + tab->core.width + tab->core.border_width * 2 +
	f->folder.marginHeight * 2 + ht * 2;
      if ((int)width > rowWidth)
	rowWidth = width;

      /* calc pixmap dimensions/maximum pixmap width */
      pixWidth = 0;
      pixHeight = 0;
      fc->folder.pixWidth = 0;
      fc->folder.pixHeight = 0;
      fc->folder.inactPixWidth = 0;
      fc->folder.inactPixHeight = 0;
      fc->folder.maxPixWidth = 0;
      fc->folder.maxPixHeight = 0;
      if (fc->folder.pix != XmUNSPECIFIED_PIXMAP)
	{
	  XGetGeometry(dpy, fc->folder.pix, &root,
		       &x, &y, &pixWidth, &pixHeight, &pixBW, &pixDepth);
	  fc->folder.pixWidth = pixWidth;
	  fc->folder.maxPixWidth = pixWidth;
	  fc->folder.pixHeight = pixHeight;
	  fc->folder.maxPixHeight = pixHeight;
	  width = co + st + pixWidth + f->folder.marginHeight * 2 + ht * 2;
	  if ((int)width > rowWidth)
	    rowWidth = width;
	}
      if (fc->folder.inactPix != XmUNSPECIFIED_PIXMAP)
	{
	  XGetGeometry(dpy, fc->folder.inactPix, &root, &x, &y,
		       &inactPixWidth, &inactPixHeight, &pixBW, &pixDepth);
	  fc->folder.inactPixWidth = inactPixWidth;
	  if (inactPixWidth > fc->folder.maxPixWidth)
	    fc->folder.maxPixWidth = inactPixWidth;
	  fc->folder.inactPixHeight = inactPixHeight;
	  if (inactPixHeight > fc->folder.maxPixHeight)
	    fc->folder.maxPixHeight = inactPixHeight;
	  width = co + st + inactPixWidth + 
	    f->folder.marginHeight * 2 + ht * 2;
	  if ((int)width > rowWidth)
	    rowWidth = width;
	}

      /* increment rowY to move on to the next tab */
      rowY += st * 2 + co * 2 + f->folder.marginWidth * 2 + ht * 2 +
	XtHeight(tab) + tab->core.border_width * 2;

      if (fc->folder.maxPixHeight)
	rowY += fc->folder.maxPixHeight + f->folder.pixmapMargin;
      rowY += f->folder.spacing;

      tabNum++;
      fc->folder.row = rowNum;
    }

  /* complete calcuations for last row */
  if (rowY)
    rowY -= f->folder.spacing;
  rows[rowNum].x = rowX;
  rows[rowNum].height = rowY;
  rows[rowNum].width = rowWidth;
  rows[rowNum].numTabs = tabNum;
  numRows = rowNum + 1;
  if (f->folder.debugLevel)
    {
      fprintf(stderr, "XmLFolder: Layout: ");
      fprintf(stderr, "row %d: x %d w %d h %d numTabs %d\n",
	      rowNum, rowX, rowWidth, rowY, tabNum);
    }
  f->folder.tabWidth = rowX + rowWidth;
  f->folder.tabBarHeight = f->folder.tabWidth;
  minWidth = f->folder.tabWidth;
  if ((int)minHeight < rowY)
    minHeight = rowY;

  /* add space for indent of upper rows */
  if (f->folder.tabsPerRow && minHeight)
    minHeight += 4;

  if (f->folder.debugLevel)
    {
      fprintf(stderr, "XmLFolder: Layout: ");
      fprintf(stderr, "tab bar minimum w %d h %d\n",
	      (int)minWidth, (int)minHeight);
    }

  /* calculate width and height of non-tab children ensure */
  /* minWidth > width and minHeight > height */
  for (i = 0; i < f->composite.num_children; i++)
    {
      child = children[i];
      if (XtIsSubclass(child, xmPrimitiveWidgetClass))
	continue;

      height = XtHeight(child) + st * 2;
      if (XtIsWidget(child))
	height += f->core.border_width * 2;
      if (height > minHeight)
	minHeight = height;

      width = XtWidth(child) + f->folder.tabWidth + st * 2;
      if (XtIsWidget(child))
	width += f->core.border_width * 2;
      if (width > minWidth)
	minWidth = width;
    }

  if (f->folder.debugLevel)
    {
      fprintf(stderr, "XmLFolder: Layout: ");
      fprintf(stderr, "with non-tabs minimum w %d h %d\n",
	      (int)minWidth, (int)minHeight);
    }

  /* Resize folder if needed */
  if (resizeIfNeeded && f->folder.resizePolicy != XmRESIZE_NONE)
    {
      if (minWidth <= 0)
	minWidth = 1;
      if (minHeight <= 0)
	minHeight = 1;
      result = XtMakeResizeRequest((Widget)f, minWidth, minHeight,
				   &width, &height);
      if (result == XtGeometryAlmost)
	XtMakeResizeRequest((Widget)f, width, height, NULL, NULL);
    }
  /* move active row to bottom */
  tab = f->folder.activeW;
  if (tab)
    {
      fc = (XmLFolderConstraintRec *)(tab->core.constraints);
      rowNum = fc->folder.row;
      f->folder.activeRow = rowNum;
      rows[rowNum].x = f->folder.tabWidth - rows[rowNum].width;
      for (i = rowNum + 1; i < numRows; i++)
	rows[i].x -= rows[rowNum].width;
    }
  else
    f->folder.activeRow = -1;

  /* configure tab children */
  for (i = 0; i < f->folder.tabCount; i++)
    {
      tab = f->folder.tabs[i];
      if (!XtIsManaged(tab))
	continue;
      fc = (XmLFolderConstraintRec *)(tab->core.constraints);
      rowNum = fc->folder.row;

      /* calculate tab x */
      if (fc->folder.firstInRow == True)
	{
	  if (f->folder.tabsPerRow && rowNum != f->folder.activeRow)
	    y = 2;
	  else
	    y = 0;
	}
      fc->folder.y = y;
      y += st + co + f->folder.marginWidth + ht;
      if (fc->folder.maxPixHeight)
	y += fc->folder.maxPixHeight + f->folder.pixmapMargin;

      /* calculate tab x and tab width */
      fc->folder.width = rows[rowNum].width;
      if (f->folder.tabPlacement == XmFOLDER_LEFT)
	{
	  fc->folder.x = rows[rowNum].x;
	  x = fc->folder.x + fc->folder.width - f->folder.marginHeight -
	    ht - XtWidth(tab) - tab->core.border_width * 2;
	}
      else
	{
	  fc->folder.x = f->core.width - rows[rowNum].x -
	    rows[rowNum].width;
	  x = fc->folder.x + f->folder.marginHeight + ht;
	}

      /* calculate tab padding */
      pad1 = 0;
      pad2 = 0;
      h = f->core.height - rows[rowNum].height;
      if (rowNum != f->folder.activeRow)
	h -= 4;
      if (f->folder.tabsPerRow && h > 0)
	{
	  pad1 = h / (rows[rowNum].numTabs * 2);
	  pad2 = pad1;
	  if (fc->folder.firstInRow == True)
	    pad2 += h - (pad1 * rows[rowNum].numTabs * 2);
	}
      y += pad1;

      /* move tab widget into place */
      XtMoveWidget(tab, x, y);

      /* calculate tab height and move to next tab */
      y += pad2 + XtHeight(tab) + tab->core.border_width * 2 + ht +
	f->folder.marginWidth + co + st;
      fc->folder.height = y - fc->folder.y; 
      y += f->folder.spacing;
    }

  /* configure non-tab children */
  for (i = 0; i < f->composite.num_children; i++)
    {
      child = children[i];
      if (XtIsSubclass(child, xmPrimitiveWidgetClass))
	continue;
      if (f->folder.resizePolicy == XmRESIZE_NONE)
	continue;

      w = (int)f->core.width - (int)f->folder.tabWidth - st * 2;
      h = (int)f->core.height - st * 2;
      if (h <= 0 || w <= 0)
	continue;
      /* manager widgets will not configure correctly unless they */
      /* are managed, so manage then unmapped if they are unmanaged */
      isManaged = True;
      if (!XtIsManaged(child))
	{
	  XtVaGetValues(child,
			XmNmappedWhenManaged, &map,
			NULL);
	  XtVaSetValues(child,
			XmNmappedWhenManaged, False,
			NULL);
	  XtManageChild(child);
	  isManaged = False;
	}
      y = st;
      if (f->folder.tabPlacement == XmFOLDER_LEFT)
	x = f->folder.tabWidth + st;
      else
	x = st;
      width = w;
      height = h;
      borderWidth = 0;
      if (XtIsWidget(child))
	borderWidth = child->core.border_width;
      XtConfigureWidget(child, x, y, width, height, borderWidth);
      if (isManaged == False)
	{
	  XtUnmanageChild(child);
	  XtVaSetValues(child, XmNmappedWhenManaged, map, NULL);
	}
    }
}

static void 
Resize(Widget w)
{
  XmLFolderWidget f;

  f = (XmLFolderWidget)w;
  Layout(f, 0);
}

static XtGeometryResult 
GeometryManager(Widget w, 
		XtWidgetGeometry *request, 
		XtWidgetGeometry *allow)
{
  XmLFolderWidget f;

  f = (XmLFolderWidget)XtParent(w);
  if (f->folder.resizePolicy != XmRESIZE_STATIC ||
      XtIsSubclass(w, xmPrimitiveWidgetClass))
    {
      if (request->request_mode & CWWidth)
	w->core.width = request->width;
      if (request->request_mode & CWHeight)
	w->core.height = request->height;
      if (request->request_mode & CWX)
	w->core.x = request->x;
      if (request->request_mode & CWY)
	w->core.y = request->y;
      if (request->request_mode & CWBorderWidth)
	w->core.border_width = request->border_width;
      Layout(f, 1);
      return XtGeometryYes;
    }
  return XtGeometryNo;
}

static void 
ChangeManaged(Widget w)
{
  XmLFolderWidget f;

  f = (XmLFolderWidget)w;
  Layout(f, 1);
  _XmNavigChangeManaged(w);
}

static void 
ConstraintInitialize(Widget req, 
		     Widget w, 
		     ArgList args, 
		     Cardinal *narg)
{
  XmLFolderWidget f;
  XmLFolderConstraintRec *fc;

  if (!XtIsRectObj(w))
    return;
  f = (XmLFolderWidget)XtParent(w);
  if (f->folder.debugLevel)
    fprintf(stderr, "XmLFolder: Constraint Init\n");
  fc = (XmLFolderConstraintRec *)(w->core.constraints);
  fc->folder.x = 0;
  fc->folder.y = 0;
  fc->folder.width = 0;
  fc->folder.height = 0;
  fc->folder.maxPixWidth = 0;
  fc->folder.maxPixHeight = 0;
  fc->folder.row = -1;
  fc->folder.firstInRow = False;
  if (fc->folder.managedName)
    fc->folder.managedName = (char *)strdup(fc->folder.managedName);
  if (XtIsSubclass(w, xmPrimitiveWidgetClass))
    {
      XtOverrideTranslations(w, f->folder.primTrans);
      XtAddCallback(w, XmNactivateCallback, PrimActivate, 0);
      XtVaSetValues(w,
		    XmNhighlightThickness, 0,
		    XmNshadowThickness, 0,
		    NULL);
      if (XtIsSubclass(w, xmLabelWidgetClass))
	XtVaSetValues(w, XmNfillOnArm, False, NULL);

      /* add child to tabs list */
      if (f->folder.tabAllocCount < f->folder.tabCount + 1)
	{
	  f->folder.tabAllocCount *= 2;
	  f->folder.tabs = (Widget *)realloc((char *)f->folder.tabs,
					     sizeof(Widget) * f->folder.tabAllocCount);
	}
      f->folder.tabs[f->folder.tabCount++] = w;

    }
  if (XmIsDrawnButton(w))
    SetTabPlacement(f, w);

#ifdef XmLEVAL
  if (f->folder.tabCount > 6)
    {
      fprintf(stderr, "XmL: evaluation version only supports <= 6 tabs\n");
      exit(0);
    }
#endif
}

static void 
ConstraintDestroy(Widget w)
{
  XmLFolderWidget f;
  XmLFolderConstraintRec *fc;
  int i, j, activePos;
	
  if (!XtIsRectObj(w))
    return;
  f = (XmLFolderWidget)XtParent(w);
  if (f->folder.debugLevel)
    fprintf(stderr, "XmLFolder: Constraint Destroy\n");
  if (f->folder.focusW == w)
    f->folder.focusW = 0;
  fc = (XmLFolderConstraintRec *)(w->core.constraints);
  if (fc->folder.managedName)
    free((char *)fc->folder.managedName);
  if (fc->folder.freePix == True)
    {
      if (fc->folder.pix != XmUNSPECIFIED_PIXMAP)
	XFreePixmap(XtDisplay(w), fc->folder.pix);
      if (fc->folder.inactPix != XmUNSPECIFIED_PIXMAP)
	XFreePixmap(XtDisplay(w), fc->folder.inactPix);
    }
  if (XtIsSubclass(w, xmPrimitiveWidgetClass))
    {
      XtRemoveCallback(w, XmNactivateCallback, PrimActivate, 0);

      /* remove child from tabs list and calculate active pos */
      activePos = -1;
      j = 0;
      for (i = 0; i < f->folder.tabCount; i++)
	if (f->folder.tabs[i] != w)
	  {
	    if (f->folder.activeW == f->folder.tabs[i])
	      activePos = j;
	    f->folder.tabs[j++] = f->folder.tabs[i];
	  }
      if (j != f->folder.tabCount - 1)
	XmLWarning((Widget)f, "ConstraintDestroy() - bad child list");
      f->folder.tabCount = j;
      f->folder.activeTab = activePos;
      if (activePos == -1)
	f->folder.activeW = 0;
    }
}

static void 
DrawTabPixmap(XmLFolderWidget f, 
	      Widget tab, 
	      int active)
{
  Display *dpy;
  Window win;
  int x, y;
  Pixmap pixmap;
  Dimension pixWidth, pixHeight, ht; 
  XmLFolderConstraintRec *fc;

  x = 0;
  y = 0;
  dpy = XtDisplay(f);
  win = XtWindow(f);
  fc = (XmLFolderConstraintRec *)(tab->core.constraints);
  ht = f->folder.highlightThickness;
  if (active)
    {
      pixWidth = fc->folder.pixWidth;
      pixHeight = fc->folder.pixHeight;
      pixmap = fc->folder.pix;
    }
  else
    {
      pixWidth = fc->folder.inactPixWidth;
      pixHeight = fc->folder.inactPixHeight;
      pixmap = fc->folder.inactPix;
    }
  if (f->folder.tabPlacement == XmFOLDER_TOP)
    {
      x = tab->core.x - pixWidth - ht - f->folder.pixmapMargin;
      y = tab->core.y + tab->core.height - pixHeight; 
    }
  else if (f->folder.tabPlacement == XmFOLDER_BOTTOM)
    {
      x = tab->core.x - fc->folder.pixWidth - ht - f->folder.pixmapMargin;
      y = tab->core.y;
    }
  else if (f->folder.tabPlacement == XmFOLDER_LEFT)
    {
      x = tab->core.x + tab->core.width - pixWidth;
      y = tab->core.y - pixHeight - f->folder.pixmapMargin - ht;
    }
  else if (f->folder.tabPlacement == XmFOLDER_RIGHT)
    {
      x = tab->core.x;
      y = tab->core.y - pixHeight - f->folder.pixmapMargin - ht;
    }
  XCopyArea(dpy, pixmap, win, f->folder.gc, 0, 0, pixWidth, pixHeight, x, y);
}

static void 
DrawManagerShadowTopBottom(XmLFolderWidget f, 
			   XRectangle *rect)
{
  Display *dpy;
  Window win;
  XmLFolderConstraintRec *fc;
  XSegment *topSeg, *botSeg;
  int i, bCount, tCount, st, botOff;

  dpy = XtDisplay(f);
  win = XtWindow(f);
  st = f->manager.shadow_thickness;
  if (!st)
    return;
  botOff = f->core.height - f->folder.tabHeight - 1;

  topSeg = (XSegment *)malloc(sizeof(XSegment) * st * 2);
  botSeg = (XSegment *)malloc(sizeof(XSegment) * st * 2);

  /* top shadow */
  fc = 0;
  if (f->folder.activeW)
    fc = (XmLFolderConstraintRec *)(f->folder.activeW->core.constraints);
  tCount = 0;
  if (fc)
    for (i = 0; i < st; i++)
      {
	topSeg[tCount].x1 = rect->x + i;
	topSeg[tCount].y1 = rect->y + i;
	topSeg[tCount].x2 = fc->folder.x + i;
	topSeg[tCount].y2 = rect->y + i;
	if (rect->x != fc->folder.x)
	  tCount++;
	topSeg[tCount].x1 = rect->x + fc->folder.x +
	  fc->folder.width - i - 1;
	topSeg[tCount].y1 = rect->y + i;
	topSeg[tCount].x2 = rect->x + rect->width - i - 1;
	topSeg[tCount].y2 = rect->y + i;
	if (fc->folder.x + fc->folder.width != rect->width)
	  tCount++;
      }
  else
    for (i = 0; i < st; i++)
      {
	topSeg[tCount].x1 = rect->x + i;
	topSeg[tCount].y1 = rect->y + i;
	topSeg[tCount].x2 = rect->x + rect->width - i - 1;
	topSeg[tCount].y2 = rect->y + i;
	tCount++;
      }
  if (f->folder.tabPlacement == XmFOLDER_BOTTOM)
    for (i = 0 ; i < tCount; i++)
      {
	topSeg[i].y1 = botOff - topSeg[i].y1;
	topSeg[i].y2 = botOff - topSeg[i].y2;
      }
  if (tCount)
    {
      if (f->folder.tabPlacement == XmFOLDER_BOTTOM)
	SetGC(f, GC_SHADOWBOT);
      else	
	SetGC(f, GC_SHADOWTOP);
      XDrawSegments(dpy, win, f->folder.gc, topSeg, tCount);
      SetGC(f, GC_UNSET);
    }

  /* left shadow */
  tCount = 0;
  for (i = 0; i < st; i++)
    {
      topSeg[tCount].x1 = rect->x + i;
      topSeg[tCount].y1 = rect->y + i;
      topSeg[tCount].x2 = rect->x + i;
      topSeg[tCount].y2 = rect->y + rect->height - i - 1;
      tCount++;
    }
  if (tCount)
    {
      SetGC(f, GC_SHADOWTOP);
      XDrawSegments(dpy, win, f->folder.gc, topSeg, tCount);
      SetGC(f, GC_UNSET);
    }

  /* right shadow */
  bCount = 0;
  for (i = 0; i < st; i++)
    {
      botSeg[bCount].x1 = rect->x + rect->width - i - 1;
      botSeg[bCount].y1 = rect->y + i;
      botSeg[bCount].x2 = rect->x + rect->width - i - 1;
      botSeg[bCount].y2 = rect->y + rect->height - i - 1;
      bCount++;
    }
  if (bCount)
    {
      SetGC(f, GC_SHADOWBOT);
      XDrawSegments(dpy, win, f->folder.gc, botSeg, bCount);
      SetGC(f, GC_UNSET);
    }

  /* bottom shadow */
  bCount = 0;
  for (i = 0; i < st; i++)
    {
      botSeg[bCount].x1 = rect->x + i;
      botSeg[bCount].y1 = rect->y + rect->height - i - 1;
      botSeg[bCount].x2 = rect->x + rect->width - i - 1;
      botSeg[bCount].y2 = rect->y + rect->height - i - 1;
      if (f->folder.tabPlacement == XmFOLDER_BOTTOM)
	{
	  botSeg[bCount].y1 = botOff - botSeg[bCount].y1;
	  botSeg[bCount].y2 = botOff - botSeg[bCount].y2;
	}
      bCount++;
    }
  if (bCount)
    {
      if (f->folder.tabPlacement == XmFOLDER_TOP)
	SetGC(f, GC_SHADOWBOT);
      else	
	SetGC(f, GC_SHADOWTOP);
      XDrawSegments(dpy, win, f->folder.gc, botSeg, bCount);
      SetGC(f, GC_UNSET);
    }
  free((char *)topSeg);
  free((char *)botSeg);
}

static void 
DrawManagerShadowLeftRight(XmLFolderWidget f, 
			   XRectangle *rect)
{
  Display *dpy;
  Window win;
  XmLFolderConstraintRec *fc;
  XSegment *topSeg, *botSeg;
  int i, bCount, tCount, st, rightOff;

  dpy = XtDisplay(f);
  win = XtWindow(f);
  st = f->manager.shadow_thickness;
  if (!st)
    return;
  rightOff = f->core.width - f->folder.tabWidth - 1;

  topSeg = (XSegment *)malloc(sizeof(XSegment) * st * 2);
  botSeg = (XSegment *)malloc(sizeof(XSegment) * st * 2);

  /* left shadow */
  fc = 0;
  if (f->folder.activeW)
    fc = (XmLFolderConstraintRec *)(f->folder.activeW->core.constraints);
  tCount = 0;
  if (fc)
    for (i = 0; i < st; i++)
      {
	topSeg[tCount].x1 = rect->x + i;
	topSeg[tCount].y1 = rect->y + i;
	topSeg[tCount].x2 = rect->x + i;
	topSeg[tCount].y2 = fc->folder.y + i;
	if (rect->y != fc->folder.y)
	  tCount++;
	topSeg[tCount].x1 = rect->x + i;
	topSeg[tCount].y1 = rect->y + fc->folder.y +
	  fc->folder.height - i - 1;
	topSeg[tCount].x2 = rect->x + i;
	topSeg[tCount].y2 = rect->y + rect->height - i - 1;
	if (fc->folder.y + fc->folder.height != rect->height)
	  tCount++;
      }
  else
    for (i = 0; i < st; i++)
      {
	topSeg[tCount].x1 = rect->x + i;
	topSeg[tCount].y1 = rect->y + i;
	topSeg[tCount].x2 = rect->x + i;
	topSeg[tCount].y2 = rect->y + rect->height - i - 1;
	tCount++;
      }
  if (f->folder.tabPlacement == XmFOLDER_RIGHT)
    for (i = 0 ; i < tCount; i++)
      {
	topSeg[i].x1 = rightOff - topSeg[i].x1;
	topSeg[i].x2 = rightOff - topSeg[i].x2;
      }
  if (tCount)
    {
      if (f->folder.tabPlacement == XmFOLDER_RIGHT)
	SetGC(f, GC_SHADOWBOT);
      else	
	SetGC(f, GC_SHADOWTOP);
      XDrawSegments(dpy, win, f->folder.gc, topSeg, tCount);
      SetGC(f, GC_UNSET);
    }

  /* top shadow */
  tCount = 0;
  for (i = 0; i < st; i++)
    {
      topSeg[tCount].x1 = rect->x + i;
      topSeg[tCount].y1 = rect->y + i;
      topSeg[tCount].x2 = rect->x + rect->width - i - 1;
      topSeg[tCount].y2 = rect->y + i;
      tCount++;
    }
  if (tCount)
    {
      SetGC(f, GC_SHADOWTOP);
      XDrawSegments(dpy, win, f->folder.gc, topSeg, tCount);
      SetGC(f, GC_UNSET);
    }

  /* bottom shadow */
  bCount = 0;
  for (i = 0; i < st; i++)
    {
      botSeg[bCount].x1 = rect->x + i;
      botSeg[bCount].y1 = rect->y + rect->height - i - 1;
      botSeg[bCount].x2 = rect->x + rect->width - i - 1;
      botSeg[bCount].y2 = rect->y + rect->height - i - 1;
      bCount++;
    }
  if (bCount)
    {
      SetGC(f, GC_SHADOWBOT);
      XDrawSegments(dpy, win, f->folder.gc, botSeg, bCount);
      SetGC(f, GC_UNSET);
    }

  /* right shadow */
  bCount = 0;
  for (i = 0; i < st; i++)
    {
      botSeg[bCount].x1 = rect->x + rect->width - i - 1;
      botSeg[bCount].y1 = rect->y + i;
      botSeg[bCount].x2 = rect->x + rect->width - i - 1;
      botSeg[bCount].y2 = rect->y + rect->height - i - 1;
      if (f->folder.tabPlacement == XmFOLDER_RIGHT)
	{
	  botSeg[bCount].x1 = rightOff - botSeg[bCount].x1;
	  botSeg[bCount].x2 = rightOff - botSeg[bCount].x2;
	}
      bCount++;
    }
  if (bCount)
    {
      if (f->folder.tabPlacement == XmFOLDER_LEFT)
	SetGC(f, GC_SHADOWBOT);
      else	
	SetGC(f, GC_SHADOWTOP);
      XDrawSegments(dpy, win, f->folder.gc, botSeg, bCount);
      SetGC(f, GC_UNSET);
    }
  free((char *)topSeg);
  free((char *)botSeg);
}

static void 
DrawTabShadowArcTopBottom(XmLFolderWidget f,
			  Widget w)
{
  XmLFolderConstraintRec *fc;
  Display *dpy;
  Window win;
  XSegment *topSeg, *botSeg;
  XRectangle rect, rect2;
  XArc arc;
  int tCount, bCount;
  int i, st, cd, botOff;

  dpy = XtDisplay(f);
  win = XtWindow(f);
  fc = (XmLFolderConstraintRec *)(w->core.constraints);
  botOff = 2 * fc->folder.y + fc->folder.height - 1;
  st = f->manager.shadow_thickness;
  if (!st)
    return;
  cd = f->folder.cornerDimension;

  tCount = 0;
  bCount = 0;
  topSeg = (XSegment *)malloc(sizeof(XSegment) * st);
  botSeg = (XSegment *)malloc(sizeof(XSegment) * st);
  for (i = 0; i < st; i++)
    {
      /* left tab shadow */
      topSeg[tCount].x1 = fc->folder.x + i;
      topSeg[tCount].y1 = fc->folder.y + cd + st;
      topSeg[tCount].x2 = fc->folder.x + i;
      topSeg[tCount].y2 = fc->folder.y + fc->folder.height - 1;
      if (w == f->folder.activeW)
	topSeg[tCount].y2 += i;
      if (f->folder.tabPlacement == XmFOLDER_BOTTOM)
	{
	  topSeg[tCount].y1 = botOff - topSeg[tCount].y1;
	  topSeg[tCount].y2 = botOff - topSeg[tCount].y2;
	}
      tCount++;

      /* right tab shadow */
      botSeg[bCount].x1 = fc->folder.x + fc->folder.width - i - 1;
      botSeg[bCount].y1 = fc->folder.y + cd + st;
      botSeg[bCount].x2 = fc->folder.x + fc->folder.width - i - 1;
      botSeg[bCount].y2 = fc->folder.y + fc->folder.height - 1;
      if (w == f->folder.activeW)
	botSeg[bCount].y2 += i;
      if (f->folder.tabPlacement == XmFOLDER_BOTTOM)
	{
	  botSeg[bCount].y1 = botOff - botSeg[bCount].y1;
	  botSeg[bCount].y2 = botOff - botSeg[bCount].y2;
	}
      bCount++;
    }
  if (tCount)
    {
      SetGC(f, GC_SHADOWTOP);
      XDrawSegments(dpy, win, f->folder.gc, topSeg, tCount);
      SetGC(f, GC_UNSET);
    }
  if (bCount)
    {
      SetGC(f, GC_SHADOWBOT);
      XDrawSegments(dpy, win, f->folder.gc, botSeg, bCount);
      SetGC(f, GC_UNSET);
    }
  tCount = 0;
  for (i = 0; i < st; i++)
    {
      /* top tab shadow */
      topSeg[tCount].x1 = fc->folder.x + cd + st;
      topSeg[tCount].y1 = fc->folder.y + i;
      topSeg[tCount].x2 = fc->folder.x + fc->folder.width - cd - st - 1;
      topSeg[tCount].y2 = fc->folder.y + i;
      if (f->folder.tabPlacement == XmFOLDER_BOTTOM)
	{
	  topSeg[tCount].y1 = botOff - topSeg[tCount].y1;
	  topSeg[tCount].y2 = botOff - topSeg[tCount].y2;
	}
      tCount++;
    }
  if (tCount)
    {
      if (f->folder.tabPlacement == XmFOLDER_BOTTOM)
	SetGC(f, GC_SHADOWBOT);
      else
	SetGC(f, GC_SHADOWTOP);
      XDrawSegments(dpy, win, f->folder.gc, topSeg, tCount);
      SetGC(f, GC_UNSET);
    }
  free((char *)topSeg);
  free((char *)botSeg);

  /* left corner blank background */
  rect.x = fc->folder.x;
  rect.y = fc->folder.y;
  rect.width = cd + st;
  rect.height = cd + st;
  if (f->folder.tabPlacement == XmFOLDER_BOTTOM)
    rect.y = fc->folder.y + fc->folder.height - rect.height;
  SetGC(f, GC_BLANK);
  XFillRectangle(dpy, win, f->folder.gc, rect.x, rect.y,
		 rect.width, rect.height);
  SetGC(f, GC_UNSET);

  /* left arc */
  /* various X Servers have problems drawing arcs - so set clip rect */
  /* and draw two circles, one smaller than the other, for corner */
  XSetClipRectangles(dpy, f->folder.gc, 0, 0, &rect, 1, Unsorted);
  arc.x = rect.x;
  arc.y = rect.y;
  arc.width = rect.width * 2;
  arc.height = rect.height * 2;
  if (f->folder.serverDrawsArcsLarge == True)
    {
      arc.width -= 1;
      arc.height -= 1;
    }
  arc.angle1 = 0 * 64;
  arc.angle2 = 360 * 64;
  if (f->folder.tabPlacement == XmFOLDER_BOTTOM)
    arc.y = fc->folder.y + fc->folder.height - arc.height;
  SetGC(f, GC_SHADOWTOP);
  XFillArc(dpy, win, f->folder.gc, arc.x, arc.y, arc.width, arc.height,
	   arc.angle1, arc.angle2);
  XDrawArc(dpy, win, f->folder.gc, arc.x, arc.y, arc.width, arc.height,
	   arc.angle1, arc.angle2);
  SetGC(f, GC_UNSET);

  rect2 = rect;
  rect2.x += st;
  rect2.width -= st;
  rect2.height -= st;
  if (f->folder.tabPlacement == XmFOLDER_TOP)
    rect2.y += st;
  XSetClipRectangles(dpy, f->folder.gc, 0, 0, &rect2, 1, Unsorted);
  if (w == f->folder.activeW)
    XSetForeground(dpy, f->folder.gc, f->core.background_pixel);
  else
    XSetForeground(dpy, f->folder.gc, f->folder.inactiveBg);
  arc.x += st;
  arc.y += st;
  arc.width -= st * 2;
  arc.height -= st * 2;
  XFillArc(dpy, win, f->folder.gc, arc.x, arc.y, arc.width, arc.height,
	   arc.angle1, arc.angle2);
  XDrawArc(dpy, win, f->folder.gc, arc.x, arc.y, arc.width, arc.height,
	   arc.angle1, arc.angle2);
  XSetClipMask(dpy, f->folder.gc, None);

  /* right corner blank background */
  rect.x = fc->folder.x + fc->folder.width - cd - st;
  SetGC(f, GC_BLANK);
  XFillRectangle(dpy, win, f->folder.gc, rect.x, rect.y,
		 rect.width, rect.height);
  SetGC(f, GC_UNSET);

  /* right arc */
  XSetClipRectangles(dpy, f->folder.gc, 0, 0, &rect, 1, Unsorted);
  arc.x = rect.x - cd - st;
  arc.y = rect.y;
  arc.width = rect.width * 2;
  arc.height = rect.height * 2;
  if (f->folder.serverDrawsArcsLarge == True)
    {
      arc.width -= 1;
      arc.height -= 1;
    }
  arc.angle1 = 0 * 64;
  arc.angle2 = 360 * 64;
  if (f->folder.tabPlacement == XmFOLDER_BOTTOM)
    arc.y = fc->folder.y + fc->folder.height - arc.height;
  SetGC(f, GC_SHADOWBOT);
  XFillArc(dpy, win, f->folder.gc, arc.x, arc.y, arc.width, arc.height,
	   arc.angle1, arc.angle2);
  XDrawArc(dpy, win, f->folder.gc, arc.x, arc.y, arc.width, arc.height,
	   arc.angle1, arc.angle2);
  SetGC(f, GC_UNSET);

  rect2 = rect;
  rect2.width -= st;
  rect2.height -= st;
  if (f->folder.tabPlacement == XmFOLDER_TOP)
    rect2.y += st;
  XSetClipRectangles(dpy, f->folder.gc, 0, 0, &rect2, 1, Unsorted);
  if (w == f->folder.activeW)
    XSetForeground(dpy, f->folder.gc, f->core.background_pixel);
  else
    XSetForeground(dpy, f->folder.gc, f->folder.inactiveBg);
  arc.x += st;
  arc.y += st;
  arc.width -= st * 2;
  arc.height -= st * 2;
  XFillArc(dpy, win, f->folder.gc, arc.x, arc.y, arc.width, arc.height,
	   arc.angle1, arc.angle2);
  XDrawArc(dpy, win, f->folder.gc, arc.x, arc.y, arc.width, arc.height,
	   arc.angle1, arc.angle2);
  XSetClipMask(dpy, f->folder.gc, None);
}

static void 
DrawTabShadowArcLeftRight(XmLFolderWidget f,
			  Widget w)
{
  XmLFolderConstraintRec *fc;
  Display *dpy;
  Window win;
  XSegment *topSeg, *botSeg;
  XRectangle rect, rect2;
  XArc arc;
  int tCount, bCount;
  int i, st, cd, rightOff;

  dpy = XtDisplay(f);
  win = XtWindow(f);
  fc = (XmLFolderConstraintRec *)(w->core.constraints);
  rightOff = 2 * fc->folder.x + fc->folder.width - 1;
  st = f->manager.shadow_thickness;
  if (!st)
    return;
  cd = f->folder.cornerDimension;

  tCount = 0;
  bCount = 0;
  topSeg = (XSegment *)malloc(sizeof(XSegment) * st);
  botSeg = (XSegment *)malloc(sizeof(XSegment) * st);
  for (i = 0; i < st; i++)
    {
      /* top tab shadow */
      topSeg[tCount].x1 = fc->folder.x + cd + st;
      topSeg[tCount].y1 = fc->folder.y + i;
      topSeg[tCount].x2 = fc->folder.x + fc->folder.width - 1;
      if (w == f->folder.activeW)
	topSeg[tCount].x2 += i;
      topSeg[tCount].y2 = fc->folder.y + i;
      if (f->folder.tabPlacement == XmFOLDER_RIGHT)
	{
	  topSeg[tCount].x1 = rightOff - topSeg[tCount].x1;
	  topSeg[tCount].x2 = rightOff - topSeg[tCount].x2;
	}
      tCount++;

      /* bottom tab shadow */
      botSeg[bCount].x1 = fc->folder.x + cd + st;
      botSeg[bCount].y1 = fc->folder.y + fc->folder.height - i - 1;
      botSeg[bCount].x2 = fc->folder.x + fc->folder.width - 1;
      if (w == f->folder.activeW)
	botSeg[bCount].x2 += i;
      botSeg[bCount].y2 = fc->folder.y + fc->folder.height - i - 1;
      if (f->folder.tabPlacement == XmFOLDER_RIGHT)
	{
	  botSeg[bCount].x1 = rightOff - botSeg[bCount].x1;
	  botSeg[bCount].x2 = rightOff - botSeg[bCount].x2;
	}
      bCount++;
    }
  if (tCount)
    {
      SetGC(f, GC_SHADOWTOP);
      XDrawSegments(dpy, win, f->folder.gc, topSeg, tCount);
      SetGC(f, GC_UNSET);
    }
  if (bCount)
    {
      SetGC(f, GC_SHADOWBOT);
      XDrawSegments(dpy, win, f->folder.gc, botSeg, bCount);
      SetGC(f, GC_UNSET);
    }
  tCount = 0;
  for (i = 0; i < st; i++)
    {
      /* left tab shadow */
      topSeg[tCount].x1 = fc->folder.x + i;
      topSeg[tCount].y1 = fc->folder.y + cd + st;
      topSeg[tCount].x2 = fc->folder.x + i;
      topSeg[tCount].y2 = fc->folder.y + fc->folder.height - cd - st - 1;
      if (f->folder.tabPlacement == XmFOLDER_RIGHT)
	{
	  topSeg[tCount].x1 = rightOff - topSeg[tCount].x1;
	  topSeg[tCount].x2 = rightOff - topSeg[tCount].x2;
	}
      tCount++;
    }
  if (tCount)
    {
      if (f->folder.tabPlacement == XmFOLDER_RIGHT)
	SetGC(f, GC_SHADOWBOT);
      else
	SetGC(f, GC_SHADOWTOP);
      XDrawSegments(dpy, win, f->folder.gc, topSeg, tCount);
      SetGC(f, GC_UNSET);
    }
  free((char *)topSeg);
  free((char *)botSeg);

  /* top corner blank background */
  rect.x = fc->folder.x;
  rect.y = fc->folder.y;
  rect.width = cd + st;
  rect.height = cd + st;
  if (f->folder.tabPlacement == XmFOLDER_RIGHT)
    rect.x = fc->folder.x + fc->folder.width - rect.width;
  SetGC(f, GC_BLANK);
  XFillRectangle(dpy, win, f->folder.gc, rect.x, rect.y,
		 rect.width, rect.height);
  SetGC(f, GC_UNSET);

  /* top arc */
  XSetClipRectangles(dpy, f->folder.gc, 0, 0, &rect, 1, Unsorted);
  arc.x = rect.x;
  arc.y = rect.y;
  arc.width = rect.width * 2;
  arc.height = rect.height * 2;
  if (f->folder.serverDrawsArcsLarge == True)
    {
      arc.width -= 1;
      arc.height -= 1;
    }
  arc.angle1 = 0 * 64;
  arc.angle2 = 360 * 64;
  if (f->folder.tabPlacement == XmFOLDER_RIGHT)
    arc.x = fc->folder.x + fc->folder.width - arc.width;
  SetGC(f, GC_SHADOWTOP);
  XFillArc(dpy, win, f->folder.gc, arc.x, arc.y, arc.width, arc.height,
	   arc.angle1, arc.angle2);
  XDrawArc(dpy, win, f->folder.gc, arc.x, arc.y, arc.width, arc.height,
	   arc.angle1, arc.angle2);
  SetGC(f, GC_UNSET);

  rect2 = rect;
  rect2.width -= st;
  rect2.height -= st;
  rect2.y += st;
  if (f->folder.tabPlacement == XmFOLDER_LEFT)
    rect2.x += st;
  XSetClipRectangles(dpy, f->folder.gc, 0, 0, &rect2, 1, Unsorted);
  if (w == f->folder.activeW)
    XSetForeground(dpy, f->folder.gc, f->core.background_pixel);
  else
    XSetForeground(dpy, f->folder.gc, f->folder.inactiveBg);
  arc.x += st;
  arc.y += st;
  arc.width -= st * 2;
  arc.height -= st * 2;
  XFillArc(dpy, win, f->folder.gc, arc.x, arc.y, arc.width, arc.height,
	   arc.angle1, arc.angle2);
  XDrawArc(dpy, win, f->folder.gc, arc.x, arc.y, arc.width, arc.height,
	   arc.angle1, arc.angle2);
  XSetClipMask(dpy, f->folder.gc, None);

  /* bottom corner blank background */
  rect.y = fc->folder.y + fc->folder.height - cd - st;
  SetGC(f, GC_BLANK);
  XFillRectangle(dpy, win, f->folder.gc, rect.x, rect.y,
		 rect.width, rect.height);
  SetGC(f, GC_UNSET);

  /* bottom arc */
  XSetClipRectangles(dpy, f->folder.gc, 0, 0, &rect, 1, Unsorted);
  arc.x = rect.x;
  arc.y = rect.y - cd - st;
  arc.width = rect.width * 2;
  arc.height = rect.height * 2;
  if (f->folder.serverDrawsArcsLarge == True)
    {
      arc.width -= 1;
      arc.height -= 1;
    }
  arc.angle1 = 0 * 64;
  arc.angle2 = 360 * 64;
  if (f->folder.tabPlacement == XmFOLDER_RIGHT)
    arc.x = fc->folder.x + fc->folder.width - arc.width;
  SetGC(f, GC_SHADOWBOT);
  XFillArc(dpy, win, f->folder.gc, arc.x, arc.y, arc.width, arc.height,
	   arc.angle1, arc.angle2);
  XDrawArc(dpy, win, f->folder.gc, arc.x, arc.y, arc.width, arc.height,
	   arc.angle1, arc.angle2);
  SetGC(f, GC_UNSET);

  rect2 = rect;
  rect2.width -= st;
  rect2.height -= st;
  if (f->folder.tabPlacement == XmFOLDER_LEFT)
    rect2.x += st;
  XSetClipRectangles(dpy, f->folder.gc, 0, 0, &rect2, 1, Unsorted);
  if (w == f->folder.activeW)
    XSetForeground(dpy, f->folder.gc, f->core.background_pixel);
  else
    XSetForeground(dpy, f->folder.gc, f->folder.inactiveBg);
  arc.x += st;
  arc.y += st;
  arc.width -= st * 2;
  arc.height -= st * 2;
  XFillArc(dpy, win, f->folder.gc, arc.x, arc.y, arc.width, arc.height,
	   arc.angle1, arc.angle2);
  XDrawArc(dpy, win, f->folder.gc, arc.x, arc.y, arc.width, arc.height,
	   arc.angle1, arc.angle2);
  XSetClipMask(dpy, f->folder.gc, None);
}

static void 
DrawTabShadowLineTopBottom(XmLFolderWidget f,
			   Widget w)
{
  XmLFolderConstraintRec *fc;
  Display *dpy;
  Window win;
  XSegment *topSeg, *botSeg;
  XPoint points[4];
  int tCount, bCount, botOff, i, st, cd, y;

  dpy = XtDisplay(f);
  win = XtWindow(f);
  fc = (XmLFolderConstraintRec *)(w->core.constraints);
  botOff = 2 * fc->folder.y + fc->folder.height - 1;
  st = f->manager.shadow_thickness;
  if (!st)
    return;
  cd = f->folder.cornerDimension;

  tCount = 0;
  bCount = 0;
  topSeg = (XSegment *)malloc(sizeof(XSegment) * st);
  botSeg = (XSegment *)malloc(sizeof(XSegment) * st);
  for (i = 0; i < st; i++)
    {
      /* left tab shadow */
      topSeg[tCount].x1 = fc->folder.x + i;
      topSeg[tCount].y1 = fc->folder.y + cd + st;
      topSeg[tCount].x2 = fc->folder.x + i;
      topSeg[tCount].y2 = fc->folder.y + fc->folder.height - 1;
      if (w == f->folder.activeW)
	topSeg[tCount].y2 += i;
      if (f->folder.tabPlacement == XmFOLDER_BOTTOM)
	{
	  topSeg[tCount].y1 = botOff - topSeg[tCount].y1;
	  topSeg[tCount].y2 = botOff - topSeg[tCount].y2;
	}
      tCount++;
      /* right tab shadow */
      botSeg[bCount].x1 = fc->folder.x + fc->folder.width - i - 1;
      botSeg[bCount].y1 = fc->folder.y + cd + st;
      botSeg[bCount].x2 = fc->folder.x + fc->folder.width - i - 1;
      botSeg[bCount].y2 = fc->folder.y + fc->folder.height - 1;
      if (w == f->folder.activeW)
	botSeg[bCount].y2 += i;
      if (f->folder.tabPlacement == XmFOLDER_BOTTOM)
	{
	  botSeg[bCount].y1 = botOff - botSeg[bCount].y1;
	  botSeg[bCount].y2 = botOff - botSeg[bCount].y2;
	}
      bCount++;
    }
  if (tCount)
    {
      SetGC(f, GC_SHADOWTOP);
      XDrawSegments(dpy, win, f->folder.gc, topSeg, tCount);
      SetGC(f, GC_UNSET);
    }
  if (bCount)
    {
      SetGC(f, GC_SHADOWBOT);
      XDrawSegments(dpy, win, f->folder.gc, botSeg, bCount);
      SetGC(f, GC_UNSET);
    }
  tCount = 0;
  for (i = 0; i < st; i++)
    {
      /* top tab shadow */
      topSeg[tCount].x1 = fc->folder.x + cd + st;
      topSeg[tCount].y1 = fc->folder.y + i;
      topSeg[tCount].x2 = fc->folder.x + fc->folder.width - cd - st - 1;
      topSeg[tCount].y2 = fc->folder.y + i;
      if (f->folder.tabPlacement == XmFOLDER_BOTTOM)
	{
	  topSeg[tCount].y1 = botOff - topSeg[tCount].y1;
	  topSeg[tCount].y2 = botOff - topSeg[tCount].y2;
	}
      tCount++;
    }
  if (tCount)
    {
      if (f->folder.tabPlacement == XmFOLDER_TOP)
	SetGC(f, GC_SHADOWTOP);
      else
	SetGC(f, GC_SHADOWBOT);
      XDrawSegments(dpy, win, f->folder.gc, topSeg, tCount);
      SetGC(f, GC_UNSET);
    }
  free((char *)topSeg);
  free((char *)botSeg);

  /* left top line */
  points[0].x = fc->folder.x;
  points[0].y = fc->folder.y + cd + st - 1;
  points[1].x = fc->folder.x + cd + st - 1;
  points[1].y = fc->folder.y;
  points[2].x = fc->folder.x + cd + st - 1;
  points[2].y = fc->folder.y + cd + st - 1;
  if (f->folder.tabPlacement == XmFOLDER_BOTTOM)
    {
      points[0].y = botOff - points[0].y;
      points[1].y = botOff - points[1].y;
      points[2].y = botOff - points[2].y;
    }
  y = fc->folder.y;
  if (f->folder.tabPlacement == XmFOLDER_BOTTOM)
    y = fc->folder.y + fc->folder.height - cd - st;
  SetGC(f, GC_BLANK);
  XFillRectangle(dpy, win, f->folder.gc, fc->folder.x, y, cd + st, cd + st);
  SetGC(f, GC_UNSET);
  SetGC(f, GC_SHADOWTOP);
  XFillPolygon(dpy, win, f->folder.gc, points, 3, Nonconvex,
	       CoordModeOrigin);
  points[3].x = points[0].x;
  points[3].y = points[0].y;
  XDrawLines(dpy, win, f->folder.gc, points, 4, CoordModeOrigin);
  SetGC(f, GC_UNSET);
  if (w == f->folder.activeW)
    XSetForeground(dpy, f->folder.gc, f->core.background_pixel);
  else
    XSetForeground(dpy, f->folder.gc, f->folder.inactiveBg);
  points[0].x += st;
  points[1].y += st;
  if (f->folder.tabPlacement == XmFOLDER_BOTTOM)
    points[1].y -= st * 2;
  XFillPolygon(dpy, win, f->folder.gc, points, 3,
	       Nonconvex, CoordModeOrigin);
  points[3].x = points[0].x;
  points[3].y = points[0].y;
  XDrawLines(dpy, win, f->folder.gc, points, 4, CoordModeOrigin);

  /* right top line */
  points[0].x = fc->folder.x + fc->folder.width - 1;
  points[0].y = fc->folder.y + cd + st - 1;
  points[1].x = fc->folder.x + fc->folder.width - cd - st;
  points[1].y = fc->folder.y;
  points[2].x = fc->folder.x + fc->folder.width - cd - st;
  points[2].y = fc->folder.y + cd + st - 1;
  if (f->folder.tabPlacement == XmFOLDER_BOTTOM)
    {
      points[0].y = botOff - points[0].y;
      points[1].y = botOff - points[1].y;
      points[2].y = botOff - points[2].y;
    }
  y = fc->folder.y;
  if (f->folder.tabPlacement == XmFOLDER_BOTTOM)
    y = fc->folder.y + fc->folder.height - cd - st;
  SetGC(f, GC_BLANK);
  XFillRectangle(dpy, win, f->folder.gc, fc->folder.x + fc->folder.width -
		 cd - st, y, cd + st, cd + st);
  SetGC(f, GC_UNSET);
  if (f->folder.tabPlacement == XmFOLDER_TOP)
    SetGC(f, GC_SHADOWTOP);
  else
    SetGC(f, GC_SHADOWBOT);
  XFillPolygon(dpy, win, f->folder.gc, points, 3, Nonconvex,
	       CoordModeOrigin);
  points[3].x = points[0].x;
  points[3].y = points[0].y;
  XDrawLines(dpy, win, f->folder.gc, points, 4, CoordModeOrigin);
  SetGC(f, GC_UNSET);
  if (w == f->folder.activeW)
    XSetForeground(dpy, f->folder.gc, f->core.background_pixel);
  else
    XSetForeground(dpy, f->folder.gc, f->folder.inactiveBg);
  points[0].x -= st;
  points[1].y += st;
  if (f->folder.tabPlacement == XmFOLDER_BOTTOM)
    points[1].y -= st * 2;
  XFillPolygon(dpy, win, f->folder.gc, points, 3, Nonconvex,
	       CoordModeOrigin);
  points[3].x = points[0].x;
  points[3].y = points[0].y;
  XDrawLines(dpy, win, f->folder.gc, points, 4, CoordModeOrigin);
}

static void 
DrawTabShadowLineLeftRight(XmLFolderWidget f,
			   Widget w)
{
  XmLFolderConstraintRec *fc;
  Display *dpy;
  Window win;
  XSegment *topSeg, *botSeg;
  XPoint points[4];
  int tCount, bCount, rightOff, i, st, cd, x;

  dpy = XtDisplay(f);
  win = XtWindow(f);
  fc = (XmLFolderConstraintRec *)(w->core.constraints);
  rightOff = 2 * fc->folder.x + fc->folder.width - 1;
  st = f->manager.shadow_thickness;
  if (!st)
    return;
  cd = f->folder.cornerDimension;

  tCount = 0;
  bCount = 0;
  topSeg = (XSegment *)malloc(sizeof(XSegment) * st);
  botSeg = (XSegment *)malloc(sizeof(XSegment) * st);
  for (i = 0; i < st; i++)
    {
      /* top tab shadow */
      topSeg[tCount].x1 = fc->folder.x + cd + st;
      topSeg[tCount].y1 = fc->folder.y + i;
      topSeg[tCount].x2 = fc->folder.x + fc->folder.width - 1;
      if (w == f->folder.activeW)
	topSeg[tCount].x2 += i;
      topSeg[tCount].y2 = fc->folder.y + i;
      if (f->folder.tabPlacement == XmFOLDER_RIGHT)
	{
	  topSeg[tCount].x1 = rightOff - topSeg[tCount].x1;
	  topSeg[tCount].x2 = rightOff - topSeg[tCount].x2;
	}
      tCount++;
      /* bottom tab shadow */
      botSeg[bCount].x1 = fc->folder.x + cd + st;
      botSeg[bCount].y1 = fc->folder.y + fc->folder.height - i - 1;
      botSeg[bCount].x2 = fc->folder.x + fc->folder.width - 1;
      if (w == f->folder.activeW)
	botSeg[bCount].x2 += i;
      botSeg[bCount].y2 = fc->folder.y + fc->folder.height - i - 1;
      if (f->folder.tabPlacement == XmFOLDER_RIGHT)
	{
	  botSeg[bCount].x1 = rightOff - botSeg[bCount].x1;
	  botSeg[bCount].x2 = rightOff - botSeg[bCount].x2;
	}
      bCount++;
    }
  if (tCount)
    {
      SetGC(f, GC_SHADOWTOP);
      XDrawSegments(dpy, win, f->folder.gc, topSeg, tCount);
      SetGC(f, GC_UNSET);
    }
  if (bCount)
    {
      SetGC(f, GC_SHADOWBOT);
      XDrawSegments(dpy, win, f->folder.gc, botSeg, bCount);
      SetGC(f, GC_UNSET);
    }
  tCount = 0;
  for (i = 0; i < st; i++)
    {
      /* left tab shadow */
      topSeg[tCount].x1 = fc->folder.x + i;
      topSeg[tCount].y1 = fc->folder.y + cd + st;
      topSeg[tCount].x2 = fc->folder.x + i;
      topSeg[tCount].y2 = fc->folder.y + fc->folder.height - cd - st - 1;
      if (f->folder.tabPlacement == XmFOLDER_RIGHT)
	{
	  topSeg[tCount].x1 = rightOff - topSeg[tCount].x1;
	  topSeg[tCount].x2 = rightOff - topSeg[tCount].x2;
	}
      tCount++;
    }
  if (tCount)
    {
      if (f->folder.tabPlacement == XmFOLDER_LEFT)
	SetGC(f, GC_SHADOWTOP);
      else
	SetGC(f, GC_SHADOWBOT);
      XDrawSegments(dpy, win, f->folder.gc, topSeg, tCount);
      SetGC(f, GC_UNSET);
    }
  free((char *)topSeg);
  free((char *)botSeg);

  /* top line */
  points[0].x = fc->folder.x + cd + st - 1;
  points[0].y = fc->folder.y;
  points[1].x = fc->folder.x;
  points[1].y = fc->folder.y + cd + st - 1;
  points[2].x = fc->folder.x + cd + st - 1;
  points[2].y = fc->folder.y + cd + st - 1;
  if (f->folder.tabPlacement == XmFOLDER_RIGHT)
    {
      points[0].x = rightOff - points[0].x;
      points[1].x = rightOff - points[1].x;
      points[2].x = rightOff - points[2].x;
    }
  if (f->folder.tabPlacement == XmFOLDER_RIGHT)
    x = fc->folder.x + fc->folder.width - cd - st;
  else
    x = fc->folder.x;
  SetGC(f, GC_BLANK);
  XFillRectangle(dpy, win, f->folder.gc, x, fc->folder.y, cd + st, cd + st);
  SetGC(f, GC_UNSET);
  SetGC(f, GC_SHADOWTOP);
  XFillPolygon(dpy, win, f->folder.gc, points, 3, Nonconvex,
	       CoordModeOrigin);
  points[3].x = points[0].x;
  points[3].y = points[0].y;
  XDrawLines(dpy, win, f->folder.gc, points, 4, CoordModeOrigin);
  SetGC(f, GC_UNSET);
  if (w == f->folder.activeW)
    XSetForeground(dpy, f->folder.gc, f->core.background_pixel);
  else
    XSetForeground(dpy, f->folder.gc, f->folder.inactiveBg);
  points[0].y += st;
  points[1].x += st;
  if (f->folder.tabPlacement == XmFOLDER_RIGHT)
    points[1].x -= st * 2;
  XFillPolygon(dpy, win, f->folder.gc, points, 3,
	       Nonconvex, CoordModeOrigin);
  points[3].x = points[0].x;
  points[3].y = points[0].y;
  XDrawLines(dpy, win, f->folder.gc, points, 4, CoordModeOrigin);

  /* bottom line */
  points[0].x = fc->folder.x + cd + st - 1;
  points[0].y = fc->folder.y + fc->folder.height - 1;
  points[1].x = fc->folder.x;
  points[1].y = fc->folder.y + fc->folder.height - cd - st;
  points[2].x = fc->folder.x + cd + st - 1;
  points[2].y = fc->folder.y + fc->folder.height - cd - st;
  if (f->folder.tabPlacement == XmFOLDER_RIGHT)
    {
      points[0].x = rightOff - points[0].x;
      points[1].x = rightOff - points[1].x;
      points[2].x = rightOff - points[2].x;
    }
  if (f->folder.tabPlacement == XmFOLDER_RIGHT)
    x = fc->folder.x + fc->folder.width - cd - st;
  else
    x = fc->folder.x;
  SetGC(f, GC_BLANK);
  XFillRectangle(dpy, win, f->folder.gc, x, fc->folder.y +
		 fc->folder.height - cd - st, cd + st, cd + st);
  SetGC(f, GC_UNSET);
  SetGC(f, GC_SHADOWBOT);
  XFillPolygon(dpy, win, f->folder.gc, points, 3, Nonconvex,
	       CoordModeOrigin);
  points[3].x = points[0].x;
  points[3].y = points[0].y;
  XDrawLines(dpy, win, f->folder.gc, points, 4, CoordModeOrigin);
  SetGC(f, GC_UNSET);
  if (w == f->folder.activeW)
    XSetForeground(dpy, f->folder.gc, f->core.background_pixel);
  else
    XSetForeground(dpy, f->folder.gc, f->folder.inactiveBg);
  points[0].y -= st;
  points[1].x += st;
  if (f->folder.tabPlacement == XmFOLDER_RIGHT)
    points[1].x -= st * 2;
  XFillPolygon(dpy, win, f->folder.gc, points, 3, Nonconvex,
	       CoordModeOrigin);
  points[3].x = points[0].x;
  points[3].y = points[0].y;
  XDrawLines(dpy, win, f->folder.gc, points, 4, CoordModeOrigin);
}

static void 
DrawTabShadowNoneTopBottom(XmLFolderWidget f,
			   Widget w)
{
  XmLFolderConstraintRec *fc;
  Display *dpy;
  Window win;
  XSegment *topSeg, *botSeg;
  int i, st, botOff, tCount, bCount;

  dpy = XtDisplay(f);
  win = XtWindow(f);
  fc = (XmLFolderConstraintRec *)(w->core.constraints);
  botOff = 2 * fc->folder.y + fc->folder.height - 1;
  st = f->manager.shadow_thickness;
  if (!st)
    return;

  tCount = 0;
  bCount = 0;
  topSeg = (XSegment *)malloc(sizeof(XSegment) * st * 2);
  botSeg = (XSegment *)malloc(sizeof(XSegment) * st * 2);
  for (i = 0; i < st; i++)
    {
      /* left tab shadow */
      topSeg[tCount].x1 = fc->folder.x + i;
      topSeg[tCount].y1 = fc->folder.y + i;
      topSeg[tCount].x2 = fc->folder.x + i;
      topSeg[tCount].y2 = fc->folder.y + fc->folder.height - 1;
      if (w == f->folder.activeW)
	topSeg[tCount].y2 += i;
      if (f->folder.tabPlacement == XmFOLDER_BOTTOM)
	{
	  topSeg[tCount].y1 = botOff - topSeg[tCount].y1;
	  topSeg[tCount].y2 = botOff - topSeg[tCount].y2;
	}
      tCount++;

      /* right tab shadow */
      botSeg[bCount].x1 = fc->folder.x + fc->folder.width - 1 - i;
      botSeg[bCount].y1 = fc->folder.y + i;
      botSeg[bCount].x2 = fc->folder.x + fc->folder.width - 1 - i;
      botSeg[bCount].y2 = fc->folder.y + fc->folder.height - 1;
      if (w == f->folder.activeW)
	botSeg[bCount].y2 += i;
      if (f->folder.tabPlacement == XmFOLDER_BOTTOM)
	{
	  botSeg[bCount].y1 = botOff - botSeg[bCount].y1;
	  botSeg[bCount].y2 = botOff - botSeg[bCount].y2;
	}
      bCount++;
    }
  if (tCount)
    {
      SetGC(f, GC_SHADOWTOP);
      XDrawSegments(dpy, win, f->folder.gc, topSeg, tCount);
      SetGC(f, GC_UNSET);
    }
  if (bCount)
    {
      SetGC(f, GC_SHADOWBOT);
      XDrawSegments(dpy, win, f->folder.gc, botSeg, bCount);
      SetGC(f, GC_UNSET);
    }

  tCount = 0;
  for (i = 0; i < st; i++)
    {
      /* top tab shadow */
      topSeg[tCount].x1 = fc->folder.x + i + 1;
      topSeg[tCount].y1 = fc->folder.y + i;
      topSeg[tCount].x2 = fc->folder.x + fc->folder.width - i - 1;
      topSeg[tCount].y2 = fc->folder.y + i;
      if (f->folder.tabPlacement == XmFOLDER_BOTTOM)
	{
	  topSeg[tCount].y1 = botOff - topSeg[tCount].y1;
	  topSeg[tCount].y2 = botOff - topSeg[tCount].y2;
	}
      tCount++;
    }
  if (tCount)
    {
      if (f->folder.tabPlacement == XmFOLDER_TOP)
	SetGC(f, GC_SHADOWTOP);
      else
	SetGC(f, GC_SHADOWBOT);
      XDrawSegments(dpy, win, f->folder.gc, topSeg, tCount);
      SetGC(f, GC_UNSET);
    }
  free((char *)topSeg);
  free((char *)botSeg);
}

static void 
DrawTabShadowNoneLeftRight(XmLFolderWidget f,
			   Widget w)
{
  XmLFolderConstraintRec *fc;
  Display *dpy;
  Window win;
  XSegment *topSeg, *botSeg;
  int i, st, rightOff, tCount, bCount;

  dpy = XtDisplay(f);
  win = XtWindow(f);
  fc = (XmLFolderConstraintRec *)(w->core.constraints);
  rightOff = 2 * fc->folder.x + fc->folder.width - 1;
  st = f->manager.shadow_thickness;
  if (!st)
    return;

  tCount = 0;
  bCount = 0;
  topSeg = (XSegment *)malloc(sizeof(XSegment) * st * 2);
  botSeg = (XSegment *)malloc(sizeof(XSegment) * st * 2);
  for (i = 0; i < st; i++)
    {
      /* bottom tab shadow */
      topSeg[tCount].x1 = fc->folder.x + i;
      topSeg[tCount].y1 = fc->folder.y + i;
      topSeg[tCount].x2 = fc->folder.x + fc->folder.width - 1;
      if (w == f->folder.activeW)
	topSeg[tCount].x2 += i;
      topSeg[tCount].y2 = fc->folder.y + i;
      if (f->folder.tabPlacement == XmFOLDER_RIGHT)
	{
	  topSeg[tCount].x1 = rightOff - topSeg[tCount].x1;
	  topSeg[tCount].x2 = rightOff - topSeg[tCount].x2;
	}
      tCount++;

      /* top tab shadow */
      botSeg[bCount].x1 = fc->folder.x + i;
      botSeg[bCount].y1 = fc->folder.y + fc->folder.height - i - 1;
      botSeg[bCount].x2 = fc->folder.x + fc->folder.width - 1;
      if (w == f->folder.activeW)
	botSeg[bCount].x2 += i;
      botSeg[bCount].y2 = fc->folder.y + fc->folder.height - i - 1;
      if (f->folder.tabPlacement == XmFOLDER_RIGHT)
	{
	  botSeg[bCount].x1 = rightOff - botSeg[bCount].x1;
	  botSeg[bCount].x2 = rightOff - botSeg[bCount].x2;
	}
      bCount++;
    }
  if (tCount)
    {
      SetGC(f, GC_SHADOWTOP);
      XDrawSegments(dpy, win, f->folder.gc, topSeg, tCount);
      SetGC(f, GC_UNSET);
    }
  if (bCount)
    {
      SetGC(f, GC_SHADOWBOT);
      XDrawSegments(dpy, win, f->folder.gc, botSeg, bCount);
      SetGC(f, GC_UNSET);
    }

  tCount = 0;
  for (i = 0; i < st; i++)
    {
      /* left tab shadow */
      topSeg[tCount].x1 = fc->folder.x + i;
      topSeg[tCount].y1 = fc->folder.y + i + 1;
      topSeg[tCount].x2 = fc->folder.x + i;
      topSeg[tCount].y2 = fc->folder.y + fc->folder.height - i - 1;
      if (f->folder.tabPlacement == XmFOLDER_RIGHT)
	{
	  topSeg[tCount].x1 = rightOff - topSeg[tCount].x1;
	  topSeg[tCount].x2 = rightOff - topSeg[tCount].x2;
	}
      tCount++;
    }
  if (tCount)
    {
      if (f->folder.tabPlacement == XmFOLDER_RIGHT)
	SetGC(f, GC_SHADOWBOT);
      else
	SetGC(f, GC_SHADOWTOP);
      XDrawSegments(dpy, win, f->folder.gc, topSeg, tCount);
      SetGC(f, GC_UNSET);
    }
  free((char *)topSeg);
  free((char *)botSeg);
}

static void 
SetGC(XmLFolderWidget f,
      int type)
{
  Display *dpy;
  XGCValues values;
  XtGCMask mask;

  dpy = XtDisplay(f);
  if (type == GC_SHADOWBOT)
    {
      mask = GCForeground;
      values.foreground = f->manager.bottom_shadow_color;
      if (f->manager.bottom_shadow_pixmap != XmUNSPECIFIED_PIXMAP)
	{
	  mask |= GCFillStyle | GCTile;
	  values.fill_style = FillTiled;
	  values.tile = f->manager.bottom_shadow_pixmap;
	}
      XChangeGC(dpy, f->folder.gc, mask, &values);
    }
  else if (type == GC_SHADOWTOP)
    {
      mask = GCForeground;
      values.foreground = f->manager.top_shadow_color;
      if (f->manager.top_shadow_pixmap != XmUNSPECIFIED_PIXMAP)
	{
	  mask |= GCFillStyle | GCTile;
	  values.fill_style = FillTiled;
	  values.tile = f->manager.top_shadow_pixmap;
	}
      XChangeGC(dpy, f->folder.gc, mask, &values);
    }
  else if (type == GC_BLANK)
    {
      mask = GCForeground;
      values.foreground = f->folder.blankBg;
      if (f->folder.blankPix != XmUNSPECIFIED_PIXMAP)
	{
	  mask |= GCFillStyle | GCTile;
	  values.fill_style = FillTiled;
	  values.tile = f->folder.blankPix;
	}
      XChangeGC(dpy, f->folder.gc, mask, &values);
    }
  else
    {
      mask = GCFillStyle;
      values.fill_style = FillSolid;
      XChangeGC(dpy, f->folder.gc, mask, &values);
    }
}

static void 
DrawTabHighlight(XmLFolderWidget f,
		 Widget w)
{
  XmLFolderConstraintRec *fc;
  Display *dpy;
  Window win;
  int ht;

  if (!XtIsRealized(w))
    return;
  if (!f->core.visible)
    return;
  dpy = XtDisplay(f);
  win = XtWindow(f);
  fc = (XmLFolderConstraintRec *)(w->core.constraints);
  ht = f->folder.highlightThickness;
  if (f->folder.focusW == w)
    XSetForeground(dpy, f->folder.gc, f->manager.highlight_color);
  else
    {
      if (f->folder.activeW == w)
	XSetForeground(dpy, f->folder.gc, f->core.background_pixel);
      else
	XSetForeground(dpy, f->folder.gc, f->folder.inactiveBg);
    }
  XFillRectangle(dpy, win, f->folder.gc,
		 w->core.x - ht, w->core.y - ht,
		 XtWidth(w) + w->core.border_width * 2 + ht * 2, 
		 XtHeight(w) + w->core.border_width * 2 + ht * 2);
}

static void 
SetTabPlacement(XmLFolderWidget f,
		Widget tab)
{
  if (!XmIsDrawnButton(tab))
    return;
  if (f->folder.allowRotate == True &&
      f->folder.tabPlacement == XmFOLDER_LEFT)
    XmLDrawnButtonSetType(tab, XmDRAWNB_STRING, XmDRAWNB_UP);
  else if (f->folder.allowRotate == True &&
	   f->folder.tabPlacement == XmFOLDER_RIGHT)
    XmLDrawnButtonSetType(tab, XmDRAWNB_STRING, XmDRAWNB_DOWN);
  else
    XmLDrawnButtonSetType(tab, XmDRAWNB_STRING, XmDRAWNB_RIGHT);
  if (XtIsRealized(tab))
    XClearArea(XtDisplay(tab), XtWindow(tab), 0, 0, 0, 0, True);
}

static void 
GetTabRect(XmLFolderWidget f,
	   Widget tab,
	   XRectangle *rect,
	   int includeShadow)
{
  XmLFolderConstraintRec *fc;
  int st;

  st = f->manager.shadow_thickness;
  fc = (XmLFolderConstraintRec *)(tab->core.constraints);
  rect->x = fc->folder.x;
  rect->y = fc->folder.y;
  rect->width = fc->folder.width;
  rect->height = fc->folder.height;
  if (includeShadow)
    {
      if (f->folder.tabPlacement == XmFOLDER_TOP)
	rect->height += st;
      else if (f->folder.tabPlacement == XmFOLDER_BOTTOM)
	{
	  rect->y -= st;
	  rect->height += st;
	}
      else if (f->folder.tabPlacement == XmFOLDER_LEFT)
	rect->width += st;
      else
	{
	  rect->x -= st;
	  rect->width += st;
	}
    }
}

static void 
SetActiveTab(XmLFolderWidget f,
	     Widget w,
	     XEvent *event,
	     Boolean notify)
{
  XmLFolderCallbackStruct cbs;
  XmLFolderConstraintRec *fc, *cfc;
  int i, j, pos;
  Display *dpy;
  Window win;
  Widget activeW, child, mw, managedW;
  WidgetList children;
  XRectangle rect;
  char *name, buf[256];

  win = (Window)NULL;

  if (f->folder.activeW == w)
    return;
  dpy = 0;
  if (XtIsRealized((Widget)f) && f->core.visible)
    {
      dpy = XtDisplay(f);
      win = XtWindow(f);
    }

  pos = -1;
  for (i = 0; i < f->folder.tabCount; i++)
    if (w == f->folder.tabs[i])
      pos = i;

  cbs.allowActivate = 1;
  cbs.layoutNeeded = 0;
  if (notify == True)
    {
      if (f->folder.debugLevel)
	fprintf(stderr, "XmLFolder: activated %d\n", pos);
      cbs.reason = XmCR_ACTIVATE;
      cbs.event = event;
      cbs.pos = pos;
      XtCallCallbackList((Widget)f, f->folder.activateCallback, &cbs);
    }
  if (!cbs.allowActivate)
    return;

  /* redraw current active tab */
  activeW = f->folder.activeW;
  if (activeW && dpy)
    {
      GetTabRect(f, activeW, &rect, 1);
      XClearArea(dpy, win, rect.x, rect.y, rect.width,
		 rect.height, True);
    }

  f->folder.activeTab = pos;
  f->folder.activeW = w;
  if (!w)
    return;

  /* Not sure this needs to be in the 3.0 (Microline) stuff */
  PrimFocusIn(w, NULL, NULL, NULL);

  /* if we selected a tab not in active row - move row into place */
  if (f->folder.tabsPerRow)
    {
      fc = (XmLFolderConstraintRec *)(w->core.constraints);
      if (fc->folder.row != f->folder.activeRow)
	Layout(f, False);
    }

  /* manage this tabs managed widget if it exists */
  children = f->composite.children;
  f->folder.allowLayout = 0;
  for (i = 0; i < f->composite.num_children; i++)
    {
      child = children[i];
      if (!XtIsSubclass(child, xmPrimitiveWidgetClass))
	continue;
      fc = (XmLFolderConstraintRec *)(child->core.constraints);
      managedW = 0;
      if (fc->folder.managedName)
	{
	  for (j = 0; j < f->composite.num_children; j++)
	    {
	      mw = children[j];
	      if (XtIsSubclass(mw, xmPrimitiveWidgetClass))
		continue;
	      name = XtName(mw);
	      if (name && !strcmp(name, fc->folder.managedName))
		managedW = mw;
	      cfc = (XmLFolderConstraintRec *)(mw->core.constraints);
	      name = cfc->folder.managedName;
	      if (name && !strcmp(name, fc->folder.managedName))
		managedW = mw;
	    }
	  if (!managedW)
	    {
	      sprintf(buf, "SetActiveTab() - managed widget named ");
	      strcat(buf, fc->folder.managedName);
	      strcat(buf, " not found");
	      XmLWarning(child, buf);
	    }
	}
      else
	managedW = fc->folder.managedW;
      if (managedW)
	{
	  if (w == child && !XtIsManaged(managedW))
	    XtManageChild(managedW);
	  if (w != child && XtIsManaged(managedW))
	    XtUnmanageChild(managedW);
	}
    }
  f->folder.allowLayout = 1;

  /* redraw new active tab */
  if (dpy)
    {
      GetTabRect(f, w, &rect, 1);
      XClearArea(dpy, win, rect.x, rect.y, rect.width, rect.height, True);
      XmProcessTraversal(w, XmTRAVERSE_CURRENT);
    }

  if (cbs.layoutNeeded)
	Layout(f, 0);
}

/*
   Utility
*/

static void 
GetCoreBackground(Widget w, 
		  int offset, 
		  XrmValue *value)
{
  value->addr = (caddr_t)&w->core.background_pixel;
}

static void 
GetDefaultTabWidgetClass(Widget w, 
		  int offset, 
		  XrmValue *value)
{
  value->addr = (caddr_t)&xmDrawnButtonWidgetClass;
}

static void 
GetManagerForeground(Widget w,
		     int offset,
		     XrmValue *value)
{
  XmLFolderWidget f;

  f = (XmLFolderWidget)w;
  value->addr = (caddr_t)&f->manager.foreground;
}

static Boolean 
ServerDrawsArcsLarge(Display *dpy,
		     int debug)
{
  Pixmap pixmap;
  XImage *image;
  Window root;
  long pixel;
  int x, y, width, height, result;
  GC gc;

  root = DefaultRootWindow(dpy);
  width = 5;
  height = 5;
  pixmap = XCreatePixmap(dpy, root, width, height, 1);
  gc = XCreateGC(dpy, pixmap, 0L, NULL);
  XSetForeground(dpy, gc, 0L);
  XFillRectangle(dpy, pixmap, gc, 0, 0, width, height);
  XSetForeground(dpy, gc, 1L);
  XDrawArc(dpy, pixmap, gc, 0, 0, width, height, 0, 360 * 64);
  image = XGetImage(dpy, pixmap, 0, 0, width, height, AllPlanes, ZPixmap);
  if (debug)
    {
      fprintf(stderr, "Test X server drawn arc (%d by %d):\n",
	      width, height);
      for (y = 0; y < height; y++)
	{
	  fprintf(stderr, " ");
	  for (x = 0; x < width; x++)
	    {
	      pixel = XGetPixel(image, x, y);
	      if (pixel == 0L)
		fprintf(stderr, ".");
	      else
		fprintf(stderr, "X");
	    }
	  fprintf(stderr, "\n");
	}
    }
  if (XGetPixel(image, width - 1, height / 2) != 1L)
    {
      result = 1;
      if (debug)
	fprintf(stderr, "X Server Draws Arcs 1 Pixel Large\n");
    }
  else
    {
      result = 0;
      if (debug)
	fprintf(stderr, "X Server Draws Arcs Within Bounds\n");
    }
  XDestroyImage(image);
  XFreeGC(dpy, gc);
  XFreePixmap(dpy, pixmap);
  return result;
}

/*
   Getting and Setting Values
*/

static Boolean 
SetValues(Widget curW,
	  Widget reqW,
	  Widget newW,
	  ArgList args,
	  Cardinal *narg)
{
  XmLFolderWidget f;
  XmLFolderWidget cur;
  int i;
  Boolean needsLayout, needsRedisplay;
 
  f = (XmLFolderWidget)newW;
  cur = (XmLFolderWidget)curW;
  needsLayout = False;
  needsRedisplay = False;
#define NE(value) (f->value != cur->value)
  if (NE(folder.tabBarHeight))
    {
      XmLWarning((Widget)f, "SetValues() - can't set tabBarHeight");
      f->folder.tabBarHeight = cur->folder.tabBarHeight;
    }
  if (NE(folder.tabCount))
    {
      XmLWarning((Widget)f, "SetValues() - can't set tabCount");
      f->folder.tabCount = cur->folder.tabCount;
    }
  if (NE(folder.activeTab))
    {
      XmLWarning((Widget)f, "SetValues() - can't set activeTab");
      f->folder.activeTab = cur->folder.activeTab;
    }
  if (f->folder.cornerDimension < 1)
    {
      XmLWarning((Widget)f, "SetValues() - cornerDimension can't be < 1");
      f->folder.cornerDimension = cur->folder.cornerDimension;
    }
  if (NE(folder.tabPlacement) ||
      NE(folder.allowRotate))
    {
      f->folder.allowLayout = 0;
      for (i = 0; i < f->folder.tabCount; i++)
	SetTabPlacement(f, f->folder.tabs[i]);
      f->folder.allowLayout = 1;
      needsLayout = True;
    }
  if (NE(folder.inactiveBg) ||
      NE(folder.blankBg) ||
      NE(folder.blankPix) ||
      NE(folder.inactiveFg))
    needsRedisplay = True;
  if (NE(folder.cornerDimension) ||
      NE(folder.cornerStyle) ||
      NE(folder.highlightThickness) ||
      NE(folder.marginHeight) ||
      NE(folder.marginWidth) ||
      NE(folder.pixmapMargin) ||
      NE(manager.shadow_thickness) ||
      NE(folder.tabsPerRow) ||
      NE(folder.spacing))
    needsLayout = True;
  if (NE(folder.fontList))
    {
      XmFontListFree(cur->folder.fontList);
      CopyFontList(f);
    }
#undef NE
  if (needsLayout == True) 
    Layout(f, 1);
  return needsRedisplay;
}

static void 
CopyFontList(XmLFolderWidget f)
{
  if (!f->folder.fontList)
    f->folder.fontList = XmLFontListCopyDefault((Widget)f);
  else
    f->folder.fontList = XmFontListCopy(f->folder.fontList);
  if (!f->folder.fontList)
    XmLWarning((Widget)f, "- fatal error - font list NULL");
}

static Boolean 
ConstraintSetValues(Widget curW,
		    Widget reqW,
		    Widget newW,
		    ArgList args,
		    Cardinal *narg)
{
  XmLFolderConstraintRec *cons, *curCons;
  XmLFolderWidget f;
  int i, hasLabelChange;

  f = (XmLFolderWidget)XtParent(newW);
  if (!XtIsRectObj(newW))
    return False;
  cons = (XmLFolderConstraintRec *)newW->core.constraints;
  curCons = (XmLFolderConstraintRec *)curW->core.constraints;
#define NE(value) (cons->value != curCons->value)
  if (NE(folder.managedName))
    {
      if (curCons->folder.managedName)
	free((char *)curCons->folder.managedName);
      if (cons->folder.managedName)
	cons->folder.managedName = (char *)strdup(cons->folder.managedName);
    }
#undef NE
  hasLabelChange = 0;
  if (XtIsSubclass(newW, xmPrimitiveWidgetClass))
    {
      for (i = 0; i < *narg; i++)
	if (args[i].name && !strcmp(args[i].name, XmNlabelString))
	  hasLabelChange = 1;
      if (hasLabelChange &&
	  (f->folder.tabPlacement == XmFOLDER_LEFT ||
	   f->folder.tabPlacement == XmFOLDER_RIGHT))
	{
	  f->folder.allowLayout = 0;
	  for (i = 0; i < f->folder.tabCount; i++)
	    SetTabPlacement(f, f->folder.tabs[i]);
	  f->folder.allowLayout = 1;
	}
    }
  if (hasLabelChange ||
      curCons->folder.pix != cons->folder.pix ||
      curCons->folder.inactPix != cons->folder.inactPix)
    Layout((XmLFolderWidget)XtParent(curW), 1);
  return False;
}

static Boolean 
CvtStringToCornerStyle(Display *dpy,
		       XrmValuePtr args,
		       Cardinal *narg,
		       XrmValuePtr fromVal,
		       XrmValuePtr toVal,
		       XtPointer *data)
{
  static XmLStringToUCharMap map[] =
  {
    { "CORNER_NONE", XmCORNER_NONE },
    { "CORNER_LINE", XmCORNER_LINE },
    { "CORNER_ARC", XmCORNER_ARC },
    { 0, 0 },
  };

  return XmLCvtStringToUChar(dpy, "XmRCornerStyle", map, fromVal, toVal);
}

static Boolean 
CvtStringToFolderResizePolicy(Display *dpy,
			      XrmValuePtr args,
			      Cardinal *narg,
			      XrmValuePtr fromVal,
			      XrmValuePtr toVal,
			      XtPointer *data)
{
  static XmLStringToUCharMap map[] =
  {
    { "RESIZE_NONE", XmRESIZE_NONE },
    { "RESIZE_STATIC", XmRESIZE_STATIC },
    { "RESIZE_DYNAMIC", XmRESIZE_DYNAMIC },
    { 0, 0 },
  };

  return XmLCvtStringToUChar(dpy, "XmRFolderResizePolicy", map,
			     fromVal, toVal);
}

static Boolean 
CvtStringToTabPlacement(Display *dpy,
			XrmValuePtr args,
			Cardinal *narg,
			XrmValuePtr fromVal,
			XrmValuePtr toVal,
			XtPointer *data)
{
  static XmLStringToUCharMap map[] =
  {
    { "FOLDER_TOP", XmFOLDER_TOP },
    { "FOLDER_LEFT", XmFOLDER_LEFT },
    { "FOLDER_BOTTOM", XmFOLDER_BOTTOM },
    { "FOLDER_RIGHT", XmFOLDER_RIGHT },
    { 0, 0 },
  };

  return XmLCvtStringToUChar(dpy, "XmRTabPlacement", map, fromVal, toVal);
}

/*
   Actions, Callbacks and Handlers
*/

static void 
Activate(Widget w,
	 XEvent *event,
	 String *params,
	 Cardinal *nparam)
{
  XmLFolderWidget f;
  XButtonEvent *be;
  XRectangle rect;
  Widget tab;
  int i;

  f = (XmLFolderWidget)w;
  if (!event || event->type != ButtonPress)
    return;
  be = (XButtonEvent *)event;
  if (f->folder.debugLevel)
    fprintf(stderr, "XmLFolder: ButtonPress %d %d\n", be->x, be->y);
  for (i = 0; i < f->folder.tabCount; i++)
    {
      tab = f->folder.tabs[i];
      if (!XtIsManaged(tab) || !XtIsSensitive(tab))
	continue;
      GetTabRect(f, tab, &rect, 0); 
      if (be->x > rect.x && be->x < rect.x + (int)rect.width &&
	  be->y > rect.y && be->y < rect.y + (int)rect.height)
	{
	  if (f->folder.debugLevel)
	    fprintf(stderr, "XmLFolder: Pressed tab %d\n", i);
	  SetActiveTab(f, tab, event, True);
	  return;
	}
    }
}

static void 
PrimActivate(Widget w,
	     XtPointer clientData,
	     XtPointer callData)
{
  XmLFolderWidget f;
  XmAnyCallbackStruct *cbs;

  f = (XmLFolderWidget)XtParent(w);
  cbs = (XmAnyCallbackStruct *)callData;
  SetActiveTab(f, w, cbs->event, True);
}

static void 
PrimFocusIn(Widget w,
	    XEvent *event,
	    String *params,
	    Cardinal *nparam)
{
  XmLFolderWidget f;
  Widget prevW;

  f = (XmLFolderWidget)XtParent(w);
  prevW = f->folder.focusW;
  f->folder.focusW = w;
  DrawTabHighlight(f, w);
  if (prevW)
    DrawTabHighlight(f, prevW);
  XmProcessTraversal(w, XmTRAVERSE_CURRENT);
}

static void 
PrimFocusOut(Widget w,
	     XEvent *event,
	     String *params,
	     Cardinal *nparam)
{
  XmLFolderWidget f;
  Widget prevW;

  f = (XmLFolderWidget)XtParent(w);
  prevW = f->folder.focusW;
  f->folder.focusW = 0;
  if (prevW)
    DrawTabHighlight(f, prevW);
  DrawTabHighlight(f, w);
}

/*
   Public Functions
*/

Widget 
XmLCreateFolder(Widget parent,
		char *name,
		ArgList arglist,
		Cardinal argcount)
{
  return XtCreateWidget(name, xmlFolderWidgetClass, parent,
			arglist, argcount);
}

Widget 
XmLFolderAddBitmapTab(Widget w,
		      XmString string,
		      char *bitmapBits,
		      int bitmapWidth,
		      int bitmapHeight)
{
  XmLFolderWidget f;
  Widget tab;
  Pixmap pix, inactPix;
  Window root;
  Display *dpy;
  int depth;
  char name[20];

  if (!XmLIsFolder(w))
    {
      XmLWarning(w, "AddBitmapTab() - widget not a XmLFolder");
      return 0;
    }
  f = (XmLFolderWidget)w;
  dpy = XtDisplay(w);
  root = DefaultRootWindow(dpy);
  depth = DefaultDepthOfScreen(XtScreen(w));
  pix = XCreatePixmapFromBitmapData(dpy, root, bitmapBits,
				    bitmapWidth, bitmapHeight, f->manager.foreground,
				    f->core.background_pixel, depth);
  inactPix = XCreatePixmapFromBitmapData(dpy, root, bitmapBits,
					 bitmapWidth, bitmapHeight, f->folder.inactiveFg,
					 f->folder.inactiveBg, depth);
  sprintf(name, "tab%d", f->folder.tabCount);
  tab = XtVaCreateManagedWidget(name,
				xmDrawnButtonWidgetClass, w,
				XmNfontList, f->folder.fontList,
				XmNmarginWidth, 0,
				XmNmarginHeight, 0,
				XmNlabelString, string,
				XmNtabPixmap, pix,
				XmNtabInactivePixmap, inactPix,
				XmNtabFreePixmaps, True,
				NULL);
  return tab;
}

Widget 
XmLFolderAddBitmapTabForm(Widget w, 
			  XmString string, 
			  char *bitmapBits,
			  int bitmapWidth,
			  int bitmapHeight)
{
  Widget form, tab;
  XmLFolderWidget f;
  char name[20];

  if (!XmLIsFolder(w))
    {
      XmLWarning(w, "AddBitmapTabForm() - widget not a XmLFolder");
      return 0;
    }
  f = (XmLFolderWidget)w;
  tab = XmLFolderAddBitmapTab(w, string, bitmapBits,
			      bitmapWidth, bitmapHeight);
  sprintf(name, "form%d", f->folder.tabCount);
  form = XtVaCreateManagedWidget(name,
				 xmFormWidgetClass, w,
				 XmNbackground, f->core.background_pixel,
				 NULL);
  XtVaSetValues(tab, XmNtabManagedWidget, form, NULL);
  return form;
}

Widget 
XmLFolderAddTab(Widget w, 
		XmString string)
{
  Widget tab;
  XmLFolderWidget f;
  char name[20];

  if (!XmLIsFolder(w))
    {
      XmLWarning(w, "AddTab() - widget not a XmLFolder");
      return 0;
    }
  f = (XmLFolderWidget)w;
  sprintf(name, "tab%d", f->folder.tabCount);
  tab = XtVaCreateManagedWidget(name,
				xmDrawnButtonWidgetClass, w,
				XmNfontList, f->folder.fontList,
				XmNmarginWidth, 0,
				XmNmarginHeight, 0,
				XmNlabelString, string,
				NULL);
  return tab;
}

Widget 
XmLFolderAddTabFromClass(Widget w, 
		XmString string)
{
  Widget tab;
  XmLFolderWidget f;
  char name[20];

  if (!XmLIsFolder(w))
    {
      XmLWarning(w, "AddTab() - widget not a XmLFolder");
      return 0;
    }
  f = (XmLFolderWidget)w;
  sprintf(name, "tab%d", f->folder.tabCount);

  tab = XtVaCreateManagedWidget(name,
								f->folder.tabWidgetClass,
/*  								xmDrawnButtonWidgetClass,  */
								w,
								XmNfontList, f->folder.fontList,
/* 				XmNmarginWidth, 0, */
/* 				XmNmarginHeight, 0, */
								XmNlabelString, string,
								NULL);
  return tab;
}

Widget 
XmLFolderAddTabForm(Widget w, 
		    XmString string)
{
  Widget form, tab;
  XmLFolderWidget f;
  char name[20];

  if (!XmLIsFolder(w))
    {
      XmLWarning(w, "AddBitmapTabForm() - widget not a XmLFolder");
      return 0;
    }
  f = (XmLFolderWidget)w;
  tab = XmLFolderAddTab(w, string);
  sprintf(name, "form%d", f->folder.tabCount);
  form = XtVaCreateManagedWidget(name,
				 xmFormWidgetClass, w,
				 XmNbackground, f->core.background_pixel,
				 NULL);
  XtVaSetValues(tab, XmNtabManagedWidget, form, NULL);
  return form;
}

void 
XmLFolderSetActiveTab(Widget w,
		      int position,
		      Boolean notify)
{
  XmLFolderWidget f;

  if (!XmLIsFolder(w))
    {
      XmLWarning(w, "SetActiveTab() - widget not a XmLFolder");
      return;
    }
  f = (XmLFolderWidget)w;
  if (position < 0 || position >= f->folder.tabCount)
    {
      XmLWarning(w, "SetActiveTab() - invalid position");
      return;
    }
  SetActiveTab(f, f->folder.tabs[position], 0, notify);
}
