/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
   RDFView.cpp -- view of rdf data
   Created: Stephen Lamm <slamm@netscape.com>, 5-Nov-97.
 */



#include "RDFView.h"
#include "Command.h"
#include "xfe2_extern.h"
#include "xpgetstr.h"

#include <XmL/Tree.h>
#include <Xfe/Button.h>
#include <Xfe/ToolBar.h>
#include <Xm/Label.h>

#define TREE_NAME "RdfTree"

extern "C" RDF_NCVocab  gNavCenter;

extern "C"
{
   extern PRBool fe_getPixelFromRGB(MWContext *, char * rgbString, Pixel * pixel);
};

#ifdef DEBUG_slamm
#define D(x) x
#else
#define D(x)
#endif

#define SECONDS_PER_DAY		86400L

typedef struct _closeRdfViewCBStruct {

  XFE_RDFView *  rdfview;
  XFE_NavCenterView *  ncview;
 } closeRdfViewCBStruct;

//////////////////////////////////////////////////////////////////////////
XFE_RDFView::XFE_RDFView(XFE_Component *	toplevel, 
						 Widget				parent,
                         XFE_View *			parent_view, 
						 MWContext *		context) :
	XFE_View(toplevel, parent_view, context),
	_ht_rdfView(NULL),
	_viewLabel(NULL),
	_controlToolBar(NULL),
	_addBookmarkControl(NULL),
	_closeControl(NULL),
	_modeControl(NULL),
	_rdfTreeView(NULL),
	_standAloneState(False)
{
	// Main form
	Widget rdfMainForm = XtVaCreateWidget("rdfMainForm",
									   xmFormWidgetClass,
									   parent,
									   XmNshadowThickness,		0,
									   NULL);

	setBaseWidget(rdfMainForm);

	// Control Tool Bar
	_controlToolBar = 
		XtVaCreateManagedWidget("controlToolBar",
								xfeToolBarWidgetClass,
								rdfMainForm,
								XmNtopAttachment,			XmATTACH_FORM,
								XmNrightAttachment,			XmATTACH_FORM,
								XmNleftAttachment,			XmATTACH_NONE,
								XmNbottomAttachment,		XmATTACH_NONE,
								XmNorientation,				XmHORIZONTAL,
								XmNusePreferredWidth,		True,
								XmNusePreferredHeight,		True,
								XmNchildForceWidth,			False,
								XmNchildForceHeight,		True,
								XmNchildUsePreferredWidth,	True,
								XmNchildUsePreferredHeight,	False,
								NULL);

	// View label
	_viewLabel = 
		XtVaCreateManagedWidget("viewLabel",
								xmLabelWidgetClass,
								rdfMainForm,
								XmNtopAttachment,		XmATTACH_WIDGET,
								XmNtopWidget,			_controlToolBar,
								XmNrightAttachment,		XmATTACH_FORM,
								XmNleftAttachment,		XmATTACH_FORM,
								XmNbottomAttachment,	XmATTACH_NONE,
								XmNalignment,			XmALIGNMENT_BEGINNING,
								NULL);
                                     
	// Add Bookmark
	_addBookmarkControl = XtVaCreateManagedWidget("addBookmark",
												  xfeButtonWidgetClass,
												  _controlToolBar,
												  NULL);
	// Close
	_closeControl = XtVaCreateManagedWidget("close",
											xfeButtonWidgetClass,
											_controlToolBar,
											NULL);
#if 0
	// Mode
	_modeControl = XtVaCreateManagedWidget("mode",
										   xfeButtonWidgetClass,
										   _controlToolBar,
										   NULL);
#endif

	closeRdfViewCBStruct *  cb_str = new closeRdfViewCBStruct;
	
	cb_str->rdfview = this;
	cb_str->ncview = (XFE_NavCenterView *)parent_view;
	
#ifdef NOT_YET
	XtAddCallback(closeRDFView, XmNactivateCallback, (XtCallbackProc)closeRdfView_cb , (void *)cb_str);
#endif    /* NOT_YET */
	
	// Create the rdf tree view
	_rdfTreeView = new XFE_RDFTreeView(toplevel, 
									   rdfMainForm,
									   this, 
									   context);

	// Place the tree underneath the view label
	XtVaSetValues(_rdfTreeView->getBaseWidget(),
				  XmNtopAttachment,		XmATTACH_WIDGET,
				  XmNtopWidget,			_viewLabel,
				  XmNrightAttachment,	XmATTACH_FORM,
				  XmNleftAttachment,	XmATTACH_FORM,
				  XmNbottomAttachment,	XmATTACH_FORM,
				  NULL);

	// Show the tree
	_rdfTreeView->show();
}
//////////////////////////////////////////////////////////////////////////
XFE_RDFView::~XFE_RDFView()
{
}
//////////////////////////////////////////////////////////////////////////
void
XFE_RDFView::notify(HT_Notification		ns,
					HT_Resource			n, 
                    HT_Event			whatHappened)
{
	// HT_EVENT_VIEW_SELECTED
	if (whatHappened == HT_EVENT_VIEW_SELECTED)
	{
		HT_View		htView = HT_GetView(n);
		char *		label = HT_GetViewName(htView);

		XP_ASSERT( _rdfTreeView != NULL );

		if (_rdfTreeView->getHTView() != htView)
		{
			_rdfTreeView->setHTView(htView);

			// XXX  Aurora NEED TO LOCALIZE  XXX
			XmString xmstr = XmStringCreateLocalized(label);

			XtVaSetValues(_viewLabel,XmNlabelString,xmstr,NULL);

			XmStringFree(xmstr);
            // Set the HT properties
            setHTTitlebarProperties(htView, _viewLabel);
		}
	}
	else
	{
		XP_ASSERT( _rdfTreeView != NULL );

		_rdfTreeView->notify(ns,n,whatHappened);
	}
}
//////////////////////////////////////////////////////////////////////
void
XFE_RDFView::setHTView(HT_View htview)
{
	XP_ASSERT( _rdfTreeView != NULL );

	_rdfTreeView->setHTView(htview);
}
//////////////////////////////////////////////////////////////////////
void
XFE_RDFView::closeRdfView_cb(Widget /* w */, XtPointer clientData, XtPointer /* callData */)
{

  closeRdfViewCBStruct * obj = (closeRdfViewCBStruct *) clientData;
  XFE_NavCenterView * ncview = obj->ncview;

  Widget  selector  = (Widget )ncview->getSelector();

//  Widget nc_base_widget = ncview->getBaseWidget();
  Widget parent = XtParent(obj->rdfview->_controlToolBar);


/*   XtVaSetValues(nc_base_widget, XmNresizable, True, NULL); */
  XtUnmanageChild(parent);
  XtUnmanageChild(selector);  

  XtVaSetValues(selector, XmNrightAttachment, XmATTACH_FORM, 
                          XmNleftAttachment, XmATTACH_FORM,
                          XmNtopAttachment, XmATTACH_FORM,
                          XmNbottomAttachment, XmATTACH_FORM,
                          NULL);
  XtManageChild(selector);
}
//////////////////////////////////////////////////////////////////////////
//
// Toggle the stand alone state
//
//////////////////////////////////////////////////////////////////////////
void
XFE_RDFView::setStandAloneState(XP_Bool state)
{
	XP_ASSERT( _rdfTreeView != NULL );

	_rdfTreeView->setStandAloneState(state);
}
//////////////////////////////////////////////////////////////////////////
XP_Bool
XFE_RDFView::getStandAloneState()
{
	return _standAloneState;
}
//////////////////////////////////////////////////////////////////////////


void
XFE_RDFView::setHTTitlebarProperties(HT_View view, Widget  titleBar)
{

   Arg           av[30];
   Cardinal      ac=0;
   void *        data=NULL;
   Pixel         pixel;
   PRBool        gotit=False;

   ////////////////  TITLEBAR PROPERTIES   ///////////////////

   ac = 0;
   /* titleBarFGColor */
   HT_GetTemplateData(HT_TopNode(view),  gNavCenter->titleBarFGColor, HT_COLUMN_STRING, &data);
   if (data)
   {
      gotit = fe_getPixelFromRGB(getContext(), (char *) data, &pixel);
      if (gotit)
         XtSetArg(av[ac], XmNforeground, pixel); ac++;
   }



   /* titleBarBGColor */
   HT_GetTemplateData(HT_TopNode(view),  gNavCenter->titleBarBGColor, HT_COLUMN_STRING, &data);
   if (data)
   {
      gotit = fe_getPixelFromRGB(getContext(), (char *) data, &pixel);
      if (gotit)
         XtSetArg(av[ac], XmNbackground, pixel); ac++;
   }

   /* titleBarBGURL */
   HT_GetTemplateData(HT_TopNode(view),  gNavCenter->titleBarBGURL, HT_COLUMN_STRING, &data);
    if (data)
    {
       /* Do the RDFImage thing */

    }


    XtSetValues(titleBar, av, ac);


}


