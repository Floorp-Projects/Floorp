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
#include "felocale.h"
#include "intl_csi.h"
//#include "RDFImage.h" 

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
                                             ERDFPaneMode       mode,
											 MWContext *		context) :
	XFE_RDFTreeView(toplevel, parent, parent_view,  context),
	_viewLabel(NULL),
	_controlToolBar(NULL),
	_addBookmarkControl(NULL),
	_closeControl(NULL),
	_manageControl(NULL),
	_htmlPaneForm(NULL),
	_divider(NULL),
	_htmlPane(NULL),
    _paneMode(mode)
{


    if (getPaneMode() == RDF_PANE_DOCKED)
        createViewLabel(); 

    createDivider();

	createTree();

    if (getPaneMode() != RDF_PANE_STANDALONE)
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

    void * data= NULL;

	// Control Tool Bar
	_controlToolBar = 
		XtVaCreateManagedWidget("controlToolBar",
								xfeToolBarWidgetClass,
								getBaseWidget(),
								XmNorientation,				XmHORIZONTAL,
								XmNusePreferredWidth,		True,
								XmNusePreferredHeight,		True,
								NULL);

    if (getPaneMode() == RDF_PANE_POPUP) {
	   // Add Bookmark
	   _addBookmarkControl = XtVaCreateManagedWidget("addBookmark",
												  xfeButtonWidgetClass,
												  _controlToolBar,
												  NULL);
    }

	// Close
	_closeControl = XtVaCreateManagedWidget("close",
											xfeButtonWidgetClass,
											_controlToolBar,
											NULL);

    if (getPaneMode() == RDF_PANE_POPUP) {
	// Mode
	_manageControl = XtVaCreateManagedWidget("Manage",
										   xfeButtonWidgetClass,
										   _controlToolBar,
										   NULL);
    }


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
    

	// Divider
	_divider = 
		XtVaCreateManagedWidget("divider",
								xfeDividerWidgetClass,
								getBaseWidget(),

								// Form constraints
								XmNrightAttachment,		XmATTACH_FORM,
								XmNleftAttachment,		XmATTACH_FORM,
								XmNbottomAttachment,	XmATTACH_FORM,

								// Divider resources
								XmNorientation,			XmVERTICAL,
								XmNdividerType,			XmDIVIDER_PERCENTAGE,
								XmNdividerTarget,		1,
								NULL);
    if (_viewLabel) {
      XtVaSetValues(_divider, XmNtopAttachment, XmATTACH_WIDGET,
                    XmNtopWidget, _viewLabel, NULL);
    } else {
      XtVaSetValues(_divider, XmNtopAttachment, XmATTACH_FORM, NULL);
    }
}
//////////////////////////////////////////////////////////////////////////
void
XFE_RDFChromeTreeView::createHtmlPaneFromResources()
{

   void * data=NULL;
   PRUint32 htmlPaneHeight=0;
   Boolean      isPercentage = False;
   char *       htmlPaneURL = NULL;


   HT_GetTemplateData(HT_TopNode(_ht_view),  gNavCenter->RDF_HTMLHeight, HT_COLUMN_STRING, &data);
   if (data)
   {
     char * answer = (char *) data;
     char * t=NULL;
     if (XP_STRSTR(answer, "%")) {
       // Using % method to determine the height
       t = '\0';
       isPercentage = True;
       htmlPaneHeight = atoi(answer);
       D(printf("htmlPane  Percent height = %d\n", htmlPaneHeight););
       
     }
     else
       htmlPaneHeight = atoi(answer);
   }
   // Just for testing purposes now,
   isPercentage = True;
   htmlPaneHeight = 50;
   
   if (!htmlPaneHeight)  // Height is 0. No HTML Pane.
     return;
   
   HT_GetTemplateData(HT_TopNode(_ht_view),  gNavCenter->RDF_HTMLURL, HT_COLUMN_STRING, &data);
   if (data)
   {
     htmlPaneURL = (char *) data;
     D(printf("HTMlURL = %s\n", (char *) data););
   }
   else    // Just for testing purposes now.
     htmlPaneURL = "http://www.mozilla.org";

   if (!htmlPaneURL)  // No URL. No Pane.
     return;

   if (isPercentage) {

     createHtmlPanePercent(htmlPaneHeight, htmlPaneURL);
   }
   else {
     createHtmlPaneFixed(htmlPaneHeight, htmlPaneURL);

   }
}
//////////////////////////////////////////////////////////////////////////
//
// Set the HTML pane height (as a percentage of the view)
//
//////////////////////////////////////////////////////////////////////////
void
XFE_RDFChromeTreeView::createHtmlPanePercent(PRUint32 heightPercent, char * htmlUrl)
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
		createHtmlPane(htmlUrl);
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
XFE_RDFChromeTreeView::createHtmlPaneFixed(PRUint32 heightFixed, char * htmlUrl)
{
	Dimension height = (Dimension) heightFixed;
	
	// Create the html pane if needed (for the first time)
	if ((heightFixed > 0) && (_htmlPane == NULL))
	{
		createHtmlPane(htmlUrl);
	}

	XP_ASSERT( _htmlPaneForm != NULL );
//	XP_ASSERT( _htmlPane != NULL );

	XtVaSetValues(_divider,
				  XmNdividerType,			XmDIVIDER_FIXED_SIZE,
				  XmNdividerFixedSize,		height,
				  NULL);
}
//////////////////////////////////////////////////////////////////////////
void
XFE_RDFChromeTreeView::createHtmlPane(char * url)
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
     * creation call. So call them explicitly.
     */
    fe_init_image_callbacks(context);
    fe_InitColormap(context);


	_htmlPane = new XFE_HTMLView(m_toplevel,
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
    
     XtRealizeWidget(getBaseWidget());

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

       /*
    XtAddEventHandler((_htmlPane->getBaseWidget()), StructureNotifyMask, True, 
                      (XtEventHandler) htmlPaneExposeEH, (XtPointer) _htmlPane);
                      */


}

//////////////////////////////////////////////////////////////////////////
/* virtual */ void
XFE_RDFChromeTreeView::doAttachments()
{
  if (_controlToolBar) {
	// Control toolbar on top
    XtVaSetValues(_controlToolBar,
                  XmNtopAttachment,			XmATTACH_FORM,
                  XmNrightAttachment,		XmATTACH_FORM,
                  XmNleftAttachment,		XmATTACH_NONE,
                  XmNbottomAttachment,		XmATTACH_NONE,
                  NULL);
  }

  if (_viewLabel) {
	// View label
    XtVaSetValues(_viewLabel,
                  XmNtopAttachment,		XmATTACH_WIDGET,
                  XmNtopWidget,			_controlToolBar,
                  XmNrightAttachment,	XmATTACH_FORM,
                  XmNleftAttachment,	XmATTACH_FORM,
                  XmNbottomAttachment,	XmATTACH_NONE,
                  NULL);
  }

}
//////////////////////////////////////////////////////////////////////////
ERDFPaneMode
XFE_RDFChromeTreeView::getPaneMode()
{
  return _paneMode;

}
//////////////////////////////////////////////////////////////////////////
void
XFE_RDFChromeTreeView::updateRoot() 
{
    char * label = HT_GetViewName(_ht_view);

    if (_viewLabel) {
    // XXX  Aurora NEED TO LOCALIZE  XXX
    XmString xmstr = XmStringCreateLocalized(label);

    XtVaSetValues(_viewLabel,XmNlabelString,xmstr,NULL);
        
    XmStringFree(xmstr);
    // Set the HT properties
    setHTTitlebarProperties(_ht_view);
    setHTControlbarProperties(_ht_view);
    createHtmlPaneFromResources();
    }
        

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
}


//////////////////////////////////////////////////////////////////////////
//
// XFE_RDFChromeTreeView public methods.
//
//////////////////////////////////////////////////////////////////////////
void
XFE_RDFChromeTreeView::setHTTitlebarProperties(HT_View view)
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
   if (!data)
     data = "http://people.netscape.com/radha/sony/grn005.gif";
    if (data)
    {

       XFE_RDFImage   * rdfImage = NULL;
       Pixmap       image=(Pixmap)NULL, mask=(Pixmap)NULL;

       rdfImage = XFE_RDFImage::isImageAvailable((char *)data);
       if (rdfImage) {
          image = rdfImage->getPixmap();
          mask = rdfImage->getMask();
         XtSetArg(av[ac], XmNbackgroundPixmap, image); ac++;  
  }
  else {
    //  Create  the image object and register callback

    rdfImage = new XFE_RDFImage(m_toplevel, (void *) this, 
                                (char *)data,
                                CONTEXT_DATA(getContext())->colormap,
                                _viewLabel);

    rdfImage->setCompleteCallback((completeCallbackPtr)RDFImage_complete_cb,
                                  (void *) _viewLabel);
    rdfImage->loadImage();
  }


    }

    XtSetValues(_viewLabel, av, ac);
}
//////////////////////////////////////////////////////////////////////////
void
XFE_RDFChromeTreeView::setHTControlbarProperties(HT_View view)
{

   Arg           av[30];
   Cardinal      ac=0;
   void *        data=NULL;
   Pixel         pixel;
   PRBool        gotit=False;

   ////////////////  CONTROLBAR PROPERTIES   ///////////////////

   ac = 0;

   /* controlStripFGColor */
   HT_GetTemplateData(HT_TopNode(view),  gNavCenter->controlStripFGColor, HT_COLUMN_STRING, &data);
   if (data)
   {
      gotit = fe_GetPixelFromRGBString(getContext(), (char *) data, &pixel);
      if (gotit)
         XtSetArg(av[ac], XmNforeground, pixel); ac++;
   }

   /* controlStripBGColor */
   HT_GetTemplateData(HT_TopNode(view),  gNavCenter->controlStripBGColor, HT_COLUMN_STRING, &data);
   if (data)
   {
      gotit = fe_GetPixelFromRGBString(getContext(), (char *) data, &pixel);
      if (gotit)
         XtSetArg(av[ac], XmNbackground, pixel); ac++;
   }

   /* controlStripBGURL */
   HT_GetTemplateData(HT_TopNode(view),  gNavCenter->controlStripBGURL, HT_COLUMN_STRING, &data);
   if (!data)
     data = "http://people.netscape.com/radha/sony/grn005.gif";
    if (data)
    {

        XFE_RDFImage   * rdfImage = NULL;
        Pixmap       image=(Pixmap)NULL, mask=(Pixmap)NULL;

        rdfImage = XFE_RDFImage::isImageAvailable((char *)data);
        if (rdfImage) {
           image = rdfImage->getPixmap();
           mask = rdfImage->getMask();
           XtSetArg(av[ac], XmNbackgroundPixmap, image); 
           ac++;  
        }
        else {
         //  Create  the image object and register callback

          rdfImage = new XFE_RDFImage(m_toplevel, (void *) this, 
                                (char *)data,
                                CONTEXT_DATA(getContext())->colormap,
                                _controlToolBar);

          rdfImage->setCompleteCallback((completeCallbackPtr)RDFImage_complete_cb,
                                  (void *) _controlToolBar);
          rdfImage->loadImage();
        }

    }
    XtSetValues(_controlToolBar, av, ac);


     
#ifdef NOT_YET
   /* controlStripAddText */
   HT_GetTemplateData(HT_TopNode(view),  gNavCenter->controlStripAddText, HT_COLUMN_STRING, &data);
   if (data)
   {

       str = fe_ConvertToXmString((unsigned char *) data,
                                   INTL_GetCSIWinCSID(charSetInfo) ,
                                   NULL, XmFONT_IS_FONT, &font_list);
       XtVaSetValues(_addBookmarkControl, XmNlabelString, str, NULL);
   }


   /* controlStripEditText */
   HT_GetTemplateData(HT_TopNode(view),  gNavCenter->controlStripEditText, HT_COLUMN_STRING, &data);
   if (data)
   {
       str = fe_ConvertToXmString((unsigned char *) data,
                                   INTL_GetCSIWinCSID(charSetInfo) ,
                                   NULL, XmFONT_IS_FONT, &font_list);
       XtVaSetValues(_manageControl, XmNlabelString, str, NULL);     

   }

#endif
    /* Get the charset info ready to set labels strings for buttons */

    MWContext * context = getContext();
    INTL_CharSetInfo charSetInfo =
            LO_GetDocumentCharacterSetInfo(context);
    XmString str= NULL;
    XmFontList    font_list;

   /* controlStripCloseText */
   HT_GetTemplateData(HT_TopNode(view),  gNavCenter->controlStripCloseText, HT_COLUMN_STRING, &data);
   if (data)
   {
       str = fe_ConvertToXmString((unsigned char *) data,
                                   INTL_GetCSIWinCSID(charSetInfo) ,
                                   NULL, XmFONT_IS_FONT, &font_list);
       XtVaSetValues(_closeControl, XmNlabelString, str, NULL);     

   }




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
//////////////////////////////////////////////////////////////////////////
/*static*/ void
XFE_RDFChromeTreeView::RDFImage_complete_cb(XtPointer client_data)
{
     callbackClientData * cb = (callbackClientData *) client_data;
     Widget w = (Widget )cb->widget;


     XtVaSetValues(w, 
				   XmNbackgroundPixmap, cb->image, 
                   NULL);
     XP_FREE(cb);
}
