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
#include <Xfe/Divider.h>
#include <Xm/Label.h>


#define TREE_NAME "RdfTree"
#define MM_PER_INCH      (25.4)
#define POINTS_PER_INCH  (72.0)

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
extern "C"
{
CL_Compositor *fe_create_compositor (MWContext *context);
XFE_Frame * fe_getFrameFromContext(MWContext* context);
void fe_url_exit (URL_Struct *url, int status, MWContext *context);
void htmlPaneExposeEH(Widget, XtPointer, XEvent *, Boolean *);
void kdebug_printWidgetTree(Widget w, int column);
void fe_set_scrolled_default_size(MWContext *);
void fe_get_final_context_resources(MWContext *);
void fe_find_scrollbar_sizes(MWContext *);

MWContext * fe_CreateNewContext(MWContextType, Widget, fe_colormap * , XP_Bool);
}

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
	_divider(NULL),
	_htmlPane(NULL)
{
	createViewLabel();

	createDivider();

	createTree();

    createControlToolbar();

    doAttachments();
}
//////////////////////////////////////////////////////////////////////////
XFE_RDFChromeTreeView::~XFE_RDFChromeTreeView()
{
}
//////////////////////////////////////////////////////////////////////////
void
XFE_RDFChromeTreeView::createControlToolbar()
{
	XP_ASSERT( _controlToolBar == NULL );
	XP_ASSERT( XfeIsAlive(getBaseWidget()) );

	// Control Tool Bar
	_controlToolBar = 
		XtVaCreateManagedWidget("controlToolBar",
								xfeToolBarWidgetClass,
								getBaseWidget(),
								XmNorientation,				XmHORIZONTAL,
								XmNusePreferredWidth,		True,
								XmNusePreferredHeight,		True,
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
	XP_ASSERT( XfeIsAlive(getBaseWidget()) );
	XP_ASSERT( _viewLabel == NULL );

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
XFE_RDFChromeTreeView::createDivider()
{
	XP_ASSERT( XfeIsAlive(getBaseWidget()) );
	XP_ASSERT( _divider == NULL );
    XP_ASSERT( XfeIsAlive(_viewLabel) );

	// Divider
	_divider = 
		XtVaCreateManagedWidget("divider",
								xfeDividerWidgetClass,
								getBaseWidget(),

								// Form constraints
								XmNtopAttachment,		XmATTACH_WIDGET,
								XmNtopWidget,			_viewLabel,
								XmNrightAttachment,		XmATTACH_FORM,
								XmNleftAttachment,		XmATTACH_FORM,
								XmNbottomAttachment,	XmATTACH_FORM,

								// Divider resources
								XmNorientation,			XmVERTICAL,
								XmNdividerType,			XmDIVIDER_PERCENTAGE,
								XmNdividerTarget,		1,
								NULL);

}
//////////////////////////////////////////////////////////////////////////
void
XFE_RDFChromeTreeView::createHtmlPane()
{
	XP_ASSERT( XfeIsAlive(_divider) );
	XP_ASSERT( _htmlPaneForm == NULL );
	XP_ASSERT( _htmlPane == NULL );
    MWContext *     context = NULL;
    XFE_Frame *  frame = fe_getFrameFromContext(getContext());



	_htmlPaneForm = XtVaCreateManagedWidget("htmlPaneForm",
											xmFormWidgetClass,
											_divider,
											XmNshadowThickness,		0,
											NULL);

    context = fe_CreateNewContext(MWContextPane, _htmlPaneForm, 
                                  CONTEXT_DATA(getContext())->colormap,
                                  TRUE);

    ViewGlue_addMapping(frame, context);

    /* For some reason these 2 functions are not called in the context 
     * creation call above
     */
    fe_init_image_callbacks(context);
    fe_InitColormap(context);


	_htmlPane = new XFE_HTMLView(this,
								 _htmlPaneForm,
								 this,
								 context);

	addView(_htmlPane);

    XtVaSetValues(_htmlPane->getBaseWidget(),
                  XmNtopAttachment,			XmATTACH_FORM,
                  XmNrightAttachment,		XmATTACH_FORM,
                  XmNleftAttachment,		XmATTACH_FORM,
                  XmNbottomAttachment,		XmATTACH_FORM,
                  NULL);
	
	_htmlPane->show();
    

    XtAddEventHandler((_htmlPane->getBaseWidget()), StructureNotifyMask, True, 
                      (XtEventHandler) htmlPaneExposeEH, (XtPointer) _htmlPane);


}

//////////////////////////////////////////////////////////////////////////
/* virtual */ void
XFE_RDFChromeTreeView::doAttachments()
{
	// Control toolbar on top
    XtVaSetValues(_controlToolBar,
                  XmNtopAttachment,			XmATTACH_FORM,
                  XmNrightAttachment,		XmATTACH_FORM,
                  XmNleftAttachment,		XmATTACH_NONE,
                  XmNbottomAttachment,		XmATTACH_NONE,
                  NULL);

	// View label
    XtVaSetValues(_viewLabel,
                  XmNtopAttachment,		XmATTACH_WIDGET,
                  XmNtopWidget,			_controlToolBar,
                  XmNrightAttachment,	XmATTACH_FORM,
                  XmNleftAttachment,	XmATTACH_FORM,
                  XmNbottomAttachment,	XmATTACH_NONE,
                  NULL);

	// Divider
    XtVaSetValues(_divider,
				  XmNtopAttachment,		XmATTACH_WIDGET,
				  XmNtopWidget,			_viewLabel,
				  XmNrightAttachment,	XmATTACH_FORM,
				  XmNleftAttachment,	XmATTACH_FORM,
				  XmNbottomAttachment,	XmATTACH_FORM,
				  NULL);
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
Widget
XFE_RDFChromeTreeView::getTreeParent() 
{
	XP_ASSERT( XfeIsAlive(_divider) );

	return _divider;
}
//////////////////////////////////////////////////////////////////////////
void
XFE_RDFChromeTreeView::notify(HT_Resource		n, 
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
XFE_RDFChromeTreeView::setHtmlPaneHeightPercent(PRUint32 heightPercent)
{
	int percentage = (int) heightPercent;

	// Make sure its not more than 100%
	if (percentage > 100)
	{
		percentage = 100;
	}

	// Create the html pane if needed (for the first time)
	if ((heightPercent > 0) && (_htmlPane == NULL))
	{
		createHtmlPane();
	}

	XP_ASSERT( _htmlPaneForm != NULL );
//	XP_ASSERT( _htmlPane != NULL );

	XtVaSetValues(_divider,
				  XmNdividerType,			XmDIVIDER_PERCENTAGE,
				  XmNdividerPercentage,		percentage,
				  NULL);
}
//////////////////////////////////////////////////////////////////////////
void
XFE_RDFChromeTreeView::setHtmlPaneHeightFixed(PRUint32 heightFixed)
{
	Dimension height = (Dimension) heightFixed;
	
	// Create the html pane if needed (for the first time)
	if ((heightFixed > 0) && (_htmlPane == NULL))
	{
		createHtmlPane();
	}

	XP_ASSERT( _htmlPaneForm != NULL );
//	XP_ASSERT( _htmlPane != NULL );

	XtVaSetValues(_divider,
				  XmNdividerType,			XmDIVIDER_FIXED_SIZE,
				  XmNdividerFixedSize,		height,
				  NULL);
}
//////////////////////////////////////////////////////////////////////////
extern "C"
void 
htmlPaneExposeEH(Widget w, XtPointer clientData, XEvent * event, Boolean* continue_to_dispatch)
{

    XFE_HTMLView *  htmlview = (XFE_HTMLView *)clientData;
    MWContext *  context = htmlview->getContext();
    char * url = getenv("HTMLPANEURL");
    if (!url)
      url = "http://www.mozilla.org";

    if (event && (event->type == MapNotify))
	{
		// We only need this event handler to be called once
		XtRemoveEventHandler(w,StructureNotifyMask,True,
							 htmlPaneExposeEH, clientData);

        if (!XtIsRealized(w))
           XtRealizeWidget(w);

        // Create the compositor
        context->compositor = fe_create_compositor(context);
        XtVaSetValues (CONTEXT_DATA (context)->scrolled, XmNinitialFocus,
                 CONTEXT_DATA (context)->drawing_area, 0);

        // Do the required scroller voodoo
        fe_set_scrolled_default_size(context);
        fe_get_final_context_resources(context);
        fe_find_scrollbar_sizes(context);
        fe_InitScrolling(context); 
        
        // Load the URL
        if (url)
          NET_GetURL(NET_CreateURLStruct(url, NET_DONT_RELOAD),
               FO_CACHE_AND_PRESENT, context,
               fe_url_exit);

    }

}  /* htmlPaneExposeEH  */

