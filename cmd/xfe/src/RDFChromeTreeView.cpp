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
   RDFChromeTreeView.cpp -- view of rdf data
   Created: Stephen Lamm <slamm@netscape.com>, 5-Nov-97.
 */



#include "RDFChromeTreeView.h"
#include "Command.h"
#include "xfe2_extern.h"
#include "xpgetstr.h"

#include <XmL/Tree.h>
#include <Xfe/Button.h>
#include <Xfe/ToolBar.h>
#include <Xm/Label.h>

#define TREE_NAME "RdfTree"

extern "C" RDF_NCVocab  gNavCenter;

#ifdef DEBUG_slamm
#define D(x) x
#else
#define D(x)
#endif

#define SECONDS_PER_DAY		86400L

typedef struct _closeRdfViewCBStruct {

  XFE_RDFChromeTreeView *  rdfview;
  XFE_NavCenterView *  ncview;
 } closeRdfViewCBStruct;

//////////////////////////////////////////////////////////////////////////
XFE_RDFChromeTreeView::XFE_RDFChromeTreeView(XFE_Component *	toplevel, 
											 Widget				parent,
											 XFE_View *			parent_view, 
											 MWContext *		context) :
	XFE_RDFTreeView(toplevel, parent, parent_view, context),
	_viewLabel(NULL),
	_controlToolBar(NULL),
	_addBookmarkControl(NULL),
	_closeControl(NULL),
	_modeControl(NULL),
	_htmlPaneForm(NULL),
	_htmlPane(NULL),
	_htmlPaneHeight(0)
{
    createControlToolbar();

    createViewLabel();

    doAttachments();

    XtManageChild(_tree);
}
//////////////////////////////////////////////////////////////////////////
XFE_RDFChromeTreeView::~XFE_RDFChromeTreeView()
{
}
//////////////////////////////////////////////////////////////////////////
void
XFE_RDFChromeTreeView::createControlToolbar()
{
	// Control Tool Bar
	_controlToolBar = 
		XtVaCreateManagedWidget("controlToolBar",
								xfeToolBarWidgetClass,
								getBaseWidget(),
								XmNorientation,				XmHORIZONTAL,
								XmNusePreferredWidth,		True,
								XmNusePreferredHeight,		True,
								XmNchildForceWidth,			False,
								XmNchildForceHeight,		True,
								XmNchildUsePreferredWidth,	True,
								XmNchildUsePreferredHeight,	False,
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

    /*
	closeRdfViewCBStruct *  cb_str = new closeRdfViewCBStruct;
	
	cb_str->rdfview = this;
	cb_str->ncview = (XFE_NavCenterView *)parent_view;
	*/
	XtAddCallback(_closeControl, XmNactivateCallback, (XtCallbackProc)closeRdfView_cb , (void *)this);

	
}
//////////////////////////////////////////////////////////////////////////
void
XFE_RDFChromeTreeView::createViewLabel()
{
	// View label
	_viewLabel = 
		XtVaCreateManagedWidget("viewLabel",
								xmLabelWidgetClass,
								getBaseWidget(),
								XmNalignment,			XmALIGNMENT_BEGINNING,
								NULL);
                                     
}
//////////////////////////////////////////////////////////////////////////
void
XFE_RDFChromeTreeView::createHtmlPane()
{
	_htmlPaneForm = XtVaCreateWidget("htmlPaneForm",
									 xmFormWidgetClass,
									 getBaseWidget(),
									 XmNshadowThickness,		0,
									 NULL);
#if 0
	_htmlPane = new XFE_HTMLView(this,
								 _htmlPaneForm,
								 NULL,
								 getContext());

	addView(_htmlPane);

    XtVaSetValues(_htmlPane->getBaseWidget(),
                  XmNtopAttachment,			XmATTACH_FORM,
                  XmNrightAttachment,		XmATTACH_FORM,
                  XmNleftAttachment,		XmATTACH_FORM,
                  XmNbottomAttachment,		XmATTACH_FORM,
                  NULL);
	
	_htmlPane->show();

	// HACK HACK HACK
	_htmlPane->getURL(NET_CreateURLStruct("http://dunk",NET_DONT_RELOAD));
#endif
}
//////////////////////////////////////////////////////////////////////////
void
XFE_RDFChromeTreeView::doAttachments()
{
    XtVaSetValues(_controlToolBar,
                  XmNtopAttachment,			XmATTACH_FORM,
                  XmNrightAttachment,		XmATTACH_FORM,
                  XmNleftAttachment,		XmATTACH_NONE,
                  XmNbottomAttachment,		XmATTACH_NONE,
                  NULL);

    XtVaSetValues(_viewLabel,
                  XmNtopAttachment,		XmATTACH_WIDGET,
                  XmNtopWidget,			_controlToolBar,
                  XmNrightAttachment,	XmATTACH_FORM,
                  XmNleftAttachment,	XmATTACH_FORM,
                  XmNbottomAttachment,	XmATTACH_NONE,
                  NULL);

	// 0%
	if (_htmlPaneHeight == 0)
	{
		XtVaSetValues(_tree,
					  XmNtopAttachment,		XmATTACH_WIDGET,
					  XmNtopWidget,			_viewLabel,
					  XmNrightAttachment,	XmATTACH_FORM,
					  XmNleftAttachment,	XmATTACH_FORM,
					  XmNbottomAttachment,	XmATTACH_FORM,
					  NULL);
	}
	// 100%
	else if (_htmlPaneHeight == 100)
	{
		XtVaSetValues(_htmlPaneForm,
					  XmNtopAttachment,		XmATTACH_WIDGET,
					  XmNtopWidget,			_viewLabel,
					  XmNrightAttachment,	XmATTACH_FORM,
					  XmNleftAttachment,	XmATTACH_FORM,
					  XmNbottomAttachment,	XmATTACH_FORM,
					  NULL);
		
	}
	// 0% < height < 100%
	else
	{
		int max_html_pane_height = 
			XfeHeight(getBaseWidget()) -
			XfeHeight(_controlToolBar) -
			XfeHeight(_viewLabel);

		XP_ASSERT( max_html_pane_height > 0 );
		
		int html_pane_height = max_html_pane_height * _htmlPaneHeight / 100;
		
		XP_ASSERT( _htmlPane != NULL );
		
		XtVaSetValues(_htmlPaneForm,
//					  XmNheight,		50,
					  XmNheight,		html_pane_height,
					  NULL);

		XtVaSetValues(_htmlPaneForm,
					  XmNtopAttachment,		XmATTACH_NONE,
					  XmNrightAttachment,	XmATTACH_FORM,
					  XmNleftAttachment,	XmATTACH_FORM,
					  XmNbottomAttachment,	XmATTACH_FORM,
					  NULL);

		XtVaSetValues(_tree,
					  XmNtopAttachment,		XmATTACH_WIDGET,
					  XmNtopWidget,			_viewLabel,
					  XmNrightAttachment,	XmATTACH_FORM,
					  XmNleftAttachment,	XmATTACH_FORM,
					  XmNbottomAttachment,	XmATTACH_WIDGET,
					  XmNbottomWidget,		_htmlPaneForm,
					  NULL);

//		_htmlPane->show();
	}
}
//////////////////////////////////////////////////////////////////////////
void
XFE_RDFChromeTreeView::updateRoot() 
{
    char * label = HT_GetViewName(_ht_view);
    
    // XXX  Aurora NEED TO LOCALIZE  XXX
    XmString xmstr = XmStringCreateLocalized(label);
    
    XtVaSetValues(_viewLabel,XmNlabelString,xmstr,NULL);
        
    XmStringFree(xmstr);
        
    // Set the HT properties
    setHTTitlebarProperties(_ht_view, _viewLabel);

    XFE_RDFTreeView::updateRoot();
}
//////////////////////////////////////////////////////////////////////////
void
XFE_RDFChromeTreeView::notify(HT_Resource			n, 
                    HT_Event			whatHappened)
{
  D(debugEvent(n, whatHappened,"RV"););

	// HT_EVENT_VIEW_SELECTED
	if (whatHappened == HT_EVENT_VIEW_SELECTED)
	{
        setHTView(HT_GetView(n));
	}
    XFE_RDFTreeView::notify(n, whatHappened);
}
//////////////////////////////////////////////////////////////////////
void
XFE_RDFChromeTreeView::closeRdfView_cb(Widget /* w */, XtPointer clientData, XtPointer /* callData */)
{


  XFE_RDFChromeTreeView * obj = (XFE_RDFChromeTreeView *) clientData;

  Widget parent = XtParent(obj->getBaseWidget());
  XtUnmanageChild(parent);



#ifdef MOZ_SELECTOR_BAR

#if 0
  closeRdfViewCBStruct * obj = (closeRdfViewCBStruct *) clientData; 
  XFE_NavCenterView * ncview = obj->ncview;
//  Widget nc_base_widget = ncview->getBaseWidget();
  Widget  selector  = (Widget )ncview->getSelector();
/*   XtVaSetValues(nc_base_widget, XmNresizable, True, NULL); */
  XtUnmanageChild(parent);

  XtUnmanageChild(selector);  
  XtVaSetValues(selector, XmNrightAttachment, XmATTACH_FORM, 
                          XmNleftAttachment, XmATTACH_FORM,
                          XmNtopAttachment, XmATTACH_FORM,
                          XmNbottomAttachment, XmATTACH_FORM,
                          NULL);
  XtManageChild(selector);
#endif

#endif /*MOZ_SELECTOR_BAR*/
}
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
//
// XFE_RDFChromeTreeView public methods.
//
//////////////////////////////////////////////////////////////////////////
void
XFE_RDFChromeTreeView::setHTTitlebarProperties(HT_View view, Widget  titleBar)
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
      gotit = fe_GetPixelFromRGBString(getContext(), (char *) data, &pixel);
      if (gotit)
         XtSetArg(av[ac], XmNforeground, pixel); ac++;
   }

   /* titleBarBGColor */
   HT_GetTemplateData(HT_TopNode(view),  gNavCenter->titleBarBGColor, HT_COLUMN_STRING, &data);
   if (data)
   {
      gotit = fe_GetPixelFromRGBString(getContext(), (char *) data, &pixel);
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
//////////////////////////////////////////////////////////////////////////
//
// Set the HTML pane height (as a percentage of the view)
//
//////////////////////////////////////////////////////////////////////////
void
XFE_RDFChromeTreeView::setHtmlPaneHeight(PRUint32 height)
{
	// Make sure it actually changed
	if (_htmlPaneHeight == height)
	{
		return;
	}

	// Make sure its not more than 100%
	if (_htmlPaneHeight > 100)
	{
		_htmlPaneHeight = 100;
	}

	// Create the html pane if needed (for the first time)
	if ((height > 0) && (_htmlPane == NULL))
	{
		createHtmlPane();
	}

	_htmlPaneHeight = height;

	XP_ASSERT( _htmlPaneForm != NULL );
	XP_ASSERT( _htmlPane != NULL );

	// Show the html pane form if needed
	XfeSetManagedState(_htmlPaneForm,_htmlPaneHeight != 0);

	// Show the tree if needed
	XfeSetManagedState(_tree,_htmlPaneHeight != 100);
	
	// Redo attachments
    doAttachments();
}
//////////////////////////////////////////////////////////////////////////
