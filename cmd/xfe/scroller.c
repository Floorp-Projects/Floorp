/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
   scroller.c --- defines a subclass of XmScrolledWindow
   Created: Jamie Zawinski <jwz@netscape.com>, 23-Jul-94.
 */


#include <stdio.h>
#include <stdlib.h>
#include "mozilla.h"
#include "scroller.h"
#include "scrollerP.h"

#include <Xm/ScrollBarP.h>

static void scroller_resize (Widget);

#if XmVersion >= 2000

/*
 * There is a backward compatibility bug in motif 2.0 that prevents
 * the XmScrolledWindow class from behaving just as the motif 1.2
 * one.  For whatever reasons, the mozilla scrolling code depends on
 * the 1.2 behavior.  Fixing the problem in Mozilla should not be
 * hard.  The logic in scroll.c should probably be updated to work
 * with both motif 1.2 and 2.0.
 *
 * The current hackery solution (yes its disgusting) is to use the
 * XmScrolledWindow code from 1.2.  In defense of Netscape:  The only
 * reason whay such a sickening solution was used, was to make the
 * free mozilla date with building motif 2.0 code.
 *
 * -ramiro
 *
 */
#ifdef FEATURE_MOTIF20_SCROLLED_WINDOW_HACKING
#define SCROLLER_SC_CLASS_REC		xmHackedWindowClassRec
#define SCROLLER_SC_CONSTRAINT_SIZE	0
#else
#define SCROLLER_SC_CLASS_REC		xmScrolledWindowClassRec
#define SCROLLER_SC_CONSTRAINT_SIZE	sizeof(XmScrolledWindowConstraintRec)
#endif /* FEATURE_MOTIF20_SCROLLED_WINDOW_HACKING */

#else

#define SCROLLER_SC_CLASS_REC		xmScrolledWindowClassRec
#define SCROLLER_SC_CONSTRAINT_SIZE	0

#endif /* XmVersion >= 2000 */

ScrollerClassRec scrollerClassRec =
{
  {
    /* core_class fields      */
    /* superclass         */    (WidgetClass) &SCROLLER_SC_CLASS_REC,
    /* class_name         */    "Scroller",
    /* widget_size        */    sizeof(ScrollerRec),
    /* class_initialize   */    NULL,
    /* class_partinit     */    NULL /* ClassPartInitialize */,
    /* class_inited       */	FALSE,
    /* initialize         */    NULL /* Initialize */,
    /* Init hook	  */    NULL,
    /* realize            */    XtInheritRealize /* Realize */,
    /* actions		  */	NULL /* ScrolledWActions */,
    /* num_actions	  */	0 /* XtNumber(ScrolledWActions) */,
    /* resources          */    NULL /* resources */,
    /* num_resources      */    0 /* XtNumber(resources) */,
    /* xrm_class          */    NULLQUARK,
    /* compress_motion	  */	TRUE,
    /* compress_exposure  */	TRUE,
    /* compress_enterleave*/	TRUE,
    /* visible_interest   */    FALSE,
    /* destroy            */    NULL,
    /* resize             */    scroller_resize,
    /* expose             */    XtInheritExpose /* (XtExposeProc) Redisplay */,
    /* set_values         */    NULL /* (XtSetValuesFunc )SetValues */,
    /* set values hook    */    NULL,
    /* set values almost  */    XtInheritSetValuesAlmost,
    /* get values hook    */    NULL,
    /* accept_focus       */    NULL,
    /* Version            */    XtVersion,
    /* PRIVATE cb list    */    NULL,
    /* tm_table		  */    XtInheritTranslations,
    /* query_geometry     */    XtInheritQueryGeometry /* QueryProc */,
    /* display_accelerator*/    NULL,
    /* extension          */    NULL,
  },
  {
/* composite_class fields */
    /* geometry_manager   */    XtInheritGeometryManager /*(XtGeometryHandler )GeometryManager*/,
    /* change_managed     */    XtInheritChangeManaged /*(XtWidgetProc) ChangeManaged*/,
    /* insert_child	  */	XtInheritInsertChild /*(XtArgsProc) InsertChild*/,	
    /* delete_child	  */	XtInheritDeleteChild,
    /* Extension          */    NULL,
  },{
/* Constraint class Init */
    NULL,
    0,
	SCROLLER_SC_CONSTRAINT_SIZE,
    NULL,
    NULL,
    NULL,
    NULL
      
  },
/* Manager Class */
   {		
      XmInheritTranslations/*ScrolledWindowXlations*/,     /* translations        */    
      NULL /*get_resources*/,			/* get resources      	  */
      0 /*XtNumber(get_resources)*/,		/* num get_resources 	  */
      NULL,					/* get_cont_resources     */
      0,					/* num_get_cont_resources */
      XmInheritParentProcess,                   /* parent_process         */
      NULL,					/* extension           */    
   },

 {
/* Scrolled Window class - none */     
	 NULL
 },

 {
/* Scroller class - none */     
     /* mumble */               0
 }
};

WidgetClass scrollerClass = (WidgetClass)&scrollerClassRec;


static void scroller_resize (Widget widget)
{
  Scroller scroller = (Scroller) widget;

  /* Invoke the resize procedure of the superclass.
     Probably there's some nominally more portable way to do this
     (yeah right, like any of these slot names could possibly change
     and have any existing code still work.)
   */
  widget->core.widget_class->core_class.superclass->core_class.resize (widget);

  /* Now run our callback (yeah, I should use a real callback, so sue me.) */
  scroller->scroller.resize_hook (widget, scroller->scroller.resize_arg);
}
