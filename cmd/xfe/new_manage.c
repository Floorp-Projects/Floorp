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
   new_manage.c --- defines a subclass of XmManager
   Created: Eric Bina <ebina@netscape.com>, 17-Aug-94.

   Excerts from the X Toolkit Intrinsics Programming Manual - O'Reilly:

	"Writing a general-purpose composite widget is not a trivial task
	and should only be done when other options fail."
	"A truly general-purpose composite widget is a large, complex piece 
	of software.  You should leave this programming to the widget writers
	who write commercial widget sets, and concentrate on things that are
	more important in your application."
 */


#include <stdio.h>
#include <stdlib.h>
#include "mozilla.h"
#include "new_manage.h"
#include "new_manageP.h"

static XtGeometryResult
GeometryManager(Widget w, XtWidgetGeometry *request, XtWidgetGeometry *reply);

static void
ChangeManaged(Widget w);


NewManageClassRec newManageClassRec =
{
  {
/* core_class fields      */
    /* superclass         */    (WidgetClass) &xmManagerClassRec,
    /* class_name         */    "NewManage",
    /* widget_size        */    sizeof(NewManageRec),
    /* class_initialize   */    NULL,
    /* class_partinit     */    NULL /* ClassPartInitialize */,
    /* class_inited       */	FALSE,
    /* initialize         */    NULL /* Initialize */,
    /* Init hook	  */    NULL,
    /* realize            */    XtInheritRealize /* Realize */,
    /* actions		  */	NULL,
    /* num_actions	  */	0,
    /* resources          */    NULL /* resources */,
    /* num_resources      */    0 /* XtNumber(resources) */,
    /* xrm_class          */    NULLQUARK,
    /* compress_motion	  */	TRUE,
    /* compress_exposure  */	TRUE,
    /* compress_enterleave*/	TRUE,
    /* visible_interest   */    FALSE,
    /* destroy            */    NULL,
    /* resize             */    NULL,
    /* expose             */    XtInheritExpose /* (XtExposeProc) Redisplay */,
    /* set_values         */    NULL /* (XtSetValuesFunc )SetValues */,
    /* set values hook    */    NULL,
    /* set values almost  */    XtInheritSetValuesAlmost,
    /* get values hook    */    NULL,
    /* accept_focus       */    NULL,
    /* Version            */    XtVersion,
    /* PRIVATE cb list    */    NULL,
    /* tm_table		  */    XtInheritTranslations,
    /* query_geometry     */    NULL /* QueryProc */,
    /* display_accelerator*/    NULL,
    /* extension          */    NULL,
  },
  {
/* composite_class fields */
    /* geometry_manager   */    GeometryManager /*(XtGeometryHandler )GeometryManager*/,
    /* change_managed     */    ChangeManaged /*(XtWidgetProc) ChangeManaged*/,
    /* insert_child	  */	XtInheritInsertChild /*(XtArgsProc) InsertChild*/,	
    /* delete_child	  */	XtInheritDeleteChild,
    /* Extension          */    NULL,
  },{
/* Constraint class Init */
    NULL,
    0,
    0,
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
/* NewManage class - none */     
     /* mumble */               0
 }
};


WidgetClass newManageClass = (WidgetClass)&newManageClassRec;


static XtGeometryResult
GeometryManager(Widget w, XtWidgetGeometry *request, XtWidgetGeometry *reply)
{
	if (request->request_mode & XtCWQueryOnly)
	{
		return(XtGeometryYes);
	}

	if (request->request_mode & CWX)
	{
		XtX(w) = request->x;
	}
	if (request->request_mode & CWY)
	{
		XtY(w) = request->y;
	}
	if (request->request_mode & CWWidth)
	{
		XtWidth(w) = request->width;
	}
	if (request->request_mode & CWHeight)
	{
		XtHeight(w) = request->height;
	}
	if (request->request_mode & CWBorderWidth)
	{
		XtBorderWidth(w) = request->border_width;
	}

	return(XtGeometryYes);
}


static void
ChangeManaged(Widget w)
{
	return;
}

