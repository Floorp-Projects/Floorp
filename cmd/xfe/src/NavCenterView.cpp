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
   NavCenterView.cpp - Aurora/NavCenter view class
   Created: Stephen Lamm <slamm@netscape.com>, 05-Nov-97.
 */



#include "NavCenterView.h"
#include "HTMLView.h"
#include "RDFView.h"
#include "IconGroup.h"


#include <Xfe/XfeAll.h>

#if DEBUG_slamm
#define D(x) x
#else
#define D(x)
#endif

extern "C"
{
void ImageCompleteCallback(XtPointer client_data);

};


typedef struct _SelectorCBStruct {
  XFE_NavCenterView *ncview;
  HT_View view;
} SelectorCBStruct;


// Selector bar images list
/* static */ RDFImageList * 
XFE_NavCenterView::selectorBarImagesCache = NULL;

/* static */ int 
XFE_NavCenterView::m_numRDFImagesLoaded = 0;
//////////////////////////////////////////////////////////////////////////

// Max number of images in cache - to be replaced with very clerver hash table
/* static */ const unsigned int 
XFE_NavCenterView::MaxRdfImages = 30;

// Initialize the selector bar images list. This s'd someday be a hash list
/* static */ void 
XFE_NavCenterView::imageCacheInitialize()
{
	if (selectorBarImagesCache == NULL)
	{
		selectorBarImagesCache= new RDFImageList[MaxRdfImages];
	}
}
//////////////////////////////////////////////////////////////////////////
XFE_NavCenterView::XFE_NavCenterView(XFE_Component *toplevel_component,
                                     Widget parent, XFE_View *parent_view,
                                     MWContext *context)
  : XFE_View(toplevel_component, parent_view, context)
{
  D(printf("XFE_NavCenterView Constructor\n"););

  // Make sure the cache is up and running.
  XFE_NavCenterView::imageCacheInitialize();

  // This may need to become a constructor argument,
  // but this is good enough for now.
  m_isStandalone = (parent_view == NULL);

  // Set the view type.
  m_viewType = VIEW_NAVCENTER;

  Widget nav_form = XtVaCreateManagedWidget("nav_form",
                                            xmFormWidgetClass,
                                            parent,
                                            NULL);
  setBaseWidget(nav_form);

  m_selector = XtVaCreateManagedWidget("selector",
                   xfeToolScrollWidgetClass,
                   nav_form,
                   XmNtopAttachment,    XmATTACH_FORM,
                   XmNbottomAttachment, XmATTACH_FORM,
                   XmNleftAttachment,   XmATTACH_FORM,
                   XmNrightAttachment,  XmATTACH_NONE,
                   XmNtopOffset,        0,
                   XmNbottomOffset,     0,
                   XmNleftOffset,       0,
                   XmNrightOffset,      0,
                   XmNspacing,          0,
                   XmNshadowThickness,  0,
                   XmNselectionPolicy, XmTOOL_BAR_SELECT_SINGLE,
                   NULL);
  Widget toolbar;
  XtVaGetValues(m_selector,XmNtoolBar,&toolbar,NULL);
  XtVaSetValues(toolbar,
                XmNshadowThickness,      0,
                NULL);

  rdf_parent = XtVaCreateManagedWidget("rdf_form",
                          xmFormWidgetClass,
                          nav_form,
                          XmNtopAttachment,    XmATTACH_FORM,
#ifdef HTML_PANE
                          XmNbottomAttachment, XmATTACH_NONE,
#else
                          XmNbottomAttachment, XmATTACH_FORM,
#endif
                          XmNleftAttachment,   XmATTACH_WIDGET,
                          XmNleftWidget,       m_selector,
                          XmNrightAttachment,  XmATTACH_FORM,
                          XmNtopOffset,        0,
                          XmNbottomOffset,     1,
                          XmNleftOffset,       0,
                          XmNrightOffset,      0,
                          XmNshadowThickness,  2,
                          XmNshadowType,       XmSHADOW_IN,
                          NULL);
#ifdef HTML_PANE
  m_htmlview = new XFE_HTMLView(this, nav_form, NULL, context);
  Widget html_base = m_htmlview->getBaseWidget();

  XtVaSetValues(html_base,
                XmNtopAttachment,    XmATTACH_WIDGET,
                XmNtopWidget,        rdf_parent,
                XmNbottomAttachment, XmATTACH_FORM,
                XmNleftAttachment,   XmATTACH_WIDGET,
                XmNleftWidget,       m_selector,
                XmNrightAttachment,  XmATTACH_FORM,
                NULL);
#endif

  m_htview = NULL;
  m_pane = NULL;
  m_rdfview = new XFE_RDFView(this, rdf_parent, this,
                              context, m_isStandalone);

  HT_Notification ns = new HT_NotificationStruct;
  ns->notifyProc = notify_cb;
  ns->data = this;

  m_pane = HT_NewPane(ns);
  HT_SetPaneFEData(m_pane, this);

  // add our subviews to the list of subviews for command dispatching and
  // deletion.
  addView(m_rdfview);
#ifdef HTML_PANE
  addView(m_htmlview);
#endif 

  XtManageChild(m_selector);
  m_rdfview->show();
#ifdef HTML_PANE
  m_htmlview->show();
  m_htmlview->getURL(NET_CreateURLStruct("http://dunk/",NET_DONT_RELOAD));
#endif

  XtManageChild(nav_form);
}


XFE_NavCenterView::~XFE_NavCenterView()
{
	D(printf("XFE_NavCenterView DESTRUCTING\n"););
    if (m_pane)
        HT_DeletePane(m_pane);
}

//////////////////////////////////////////////////////////////////////////
void notify_cb(HT_Notification ns, HT_Resource n, 
                  HT_Event whatHappened, void *token, uint32 tokenType)
{
  XFE_NavCenterView* theView = (XFE_NavCenterView*)ns->data;
    
  theView->notify(ns, n, whatHappened);
}

void
XFE_NavCenterView::notify(HT_Notification ns, HT_Resource n, 
                  HT_Event whatHappened)
{
  switch (whatHappened) {
  case HT_EVENT_VIEW_CLOSED:
    D(printf("HT_Event: %s on %s\n","HT_EVENT_VIEW_CLOSED",
             HT_GetNodeName(n)););
    break;
  case HT_EVENT_VIEW_SELECTED:
    {
      D(printf("HT_Event: %s on %s\n","HT_EVENT_VIEW_SELECTED",
               HT_GetNodeName(n)););
      
      HT_View view = HT_GetView(n);

      /* The following block of code is to make sure that the view that is
       * currently selected appears as selected in the selector bar. This is
       * primarily useful when the user makes a node in the RDFView as a
       * workspace. But it is currently broken since for some reason, the
       * selector bar is not responding to the XfeToolBarSetSelectedButton() 
       * call
       */

       // Get the toolbar
       Widget toolbar;
       XtVaGetValues(m_selector, XmNtoolBar, &toolbar, NULL);
   
       // Get the label of the button
       char * label = HT_GetViewName(view);
       Widget button = NULL;
       // Obtain the widget id from the button's name
       button = XtNameToWidget(toolbar, label);

       if (toolbar && button)
         XfeToolBarSetSelectedButton(toolbar, button);

       if ((m_htview != view)  && toolbar && button) {
          // Set the RDFView
          setRDFView(view);
          // Make sure the corresponding button is marked as selected.
          XfeToolBarSetSelectedButton(toolbar, button);

       }
      
    }
    break;
  case HT_EVENT_VIEW_ADDED: 
    {
      D(printf("HT_Event: %s on %s\n","HT_EVENT_VIEW_ADDED",
               HT_GetNodeName(n)););
      
      HT_View view = HT_GetView(n);
      
      addRDFView(view);

      return;
    }
    break;
  case HT_EVENT_NODE_ADDED:
  case HT_EVENT_NODE_DELETED_DATA:
  case HT_EVENT_NODE_DELETED_NODATA:
  case HT_EVENT_NODE_VPROP_CHANGED:
  case HT_EVENT_NODE_SELECTION_CHANGED:
  case HT_EVENT_NODE_OPENCLOSE_CHANGED: 
  case HT_EVENT_NODE_OPENCLOSE_CHANGING:
    break;
  default:
    D(printf("HT_Event(%d): Unknown type on %s\n",whatHappened,HT_GetNodeName(n)););
    break;
  }
  // Pass through to the outliner
  // xxxShould check to make sure that it applies to rdfview's view.
  m_rdfview->notify(ns,n,whatHappened);
}
//////////////////////////////////////////////////////////////////////
void
XFE_NavCenterView::setRDFView(HT_View view) 
{
  Widget toolbar;
  //  WidgetList tool_items = NULL;
  XtVaGetValues(m_selector,XmNtoolBar,&toolbar,NULL);
  //XfeToolBarSetSelectedButton(toolbar, xxx);

  HT_SetSelectedView(m_pane, view);
  m_htview = view;
}
//////////////////////////////////////////////////////////////////////
void
XFE_NavCenterView::addRDFView(HT_View view) 
{
 
  static int counter=1;

  XFE_RDFImage *   rdfImage = NULL;

  XP_ASSERT(view);
  if (!view) return;

  Widget toolbar;
  //  WidgetList tool_items = NULL;

  XtVaGetValues(m_selector,XmNtoolBar,&toolbar,NULL);

  char *label = HT_GetViewName(view);
  char *icon_url = HT_GetWorkspaceLargeIconURL(view);

  char name[8];
  int ret = sscanf(icon_url, "icon/large:workspace,%s", name);
  if (ret != 0) {
    icon_url = XP_Cat("about:",name,".gif",NULL);

    // Load the icon here.
    // copy windows(navcntr.cpp)? call IL_GetImage? or IL_GetImagePixmap?

    XP_FREE(icon_url);
  }

  uint32 index = HT_GetViewIndex(view);

  char widget_name[128];
  sprintf(widget_name,"button%d", index);
  


  // Temp bookmark title hack
  if (XP_STRNCMP(label,"Bookmarks for", 13) == 0) {
      label = "Bookmarks";
  }
  // similar hack for Navigator internals
  if (XP_STRCASECMP(label,"Navigator internals") == 0) {
      label = "Nav Internals";
  }

  Widget  button = 
	  XtVaCreateManagedWidget(label,
							  xfeButtonWidgetClass,
							  toolbar,
							  NULL);
  
  // Load the image  here.

       rdfImage = new XFE_RDFImage(m_toplevel, "file:/u/radha/icons/LocationProxy.gif", CONTEXT_DATA(m_contextData)->colormap, button);
//    rdfImage = new XFE_RDFImage(m_toplevel, "file:/m/homepages/radha/sony/smile1.gif", CONTEXT_DATA(m_contextData)->colormap, button);
    rdfImage->setCompleteCallback((completeCallbackPtr)ImageCompleteCallback, (void *) button);
    rdfImage->loadImage();

    // Save the image handle in the cache
    selectorBarImagesCache[m_numRDFImagesLoaded].widget = button;
    selectorBarImagesCache[m_numRDFImagesLoaded].rdfImage = rdfImage;
    m_numRDFImagesLoaded++;


  XfeSetXmStringPSZ(button, XmNlabelString,
                    XmFONTLIST_DEFAULT_TAG, label);

  SelectorCBStruct *cbdata = new SelectorCBStruct;
  cbdata->ncview = this;
  cbdata->view = view;
  XtAddCallback(button,
                XmNactivateCallback,
                &XFE_NavCenterView::selector_activate_cb,
                (XtPointer) cbdata);
  XtAddCallback(button,
                XmNdestroyCallback,
                &XFE_NavCenterView::selector_destroy_cb,
                (XtPointer) cbdata);

}
//////////////////////////////////////////////////////////////////////
void
XFE_NavCenterView::selector_activate_cb(Widget		w,
                                        XtPointer	clientData, 
                                        XtPointer	callData)
{	
  SelectorCBStruct *cbdata = (SelectorCBStruct *)clientData;

  cbdata->ncview->setRDFView(cbdata->view);
}
//////////////////////////////////////////////////////////////////////
void
XFE_NavCenterView::selector_destroy_cb(Widget		w,
                                        XtPointer	clientData, 
                                        XtPointer	callData)
{	
  SelectorCBStruct *cbdata = (SelectorCBStruct *)clientData;

  delete cbdata;
}

Widget 
XFE_NavCenterView::getSelector(void)
{
  return (m_selector);

}

 void 
XFE_NavCenterView::handleDisplayPixmap(Widget w, IL_Pixmap * image, IL_Pixmap * mask, jint width, jint height)
{
   XFE_RDFImage *  rdfImage;

   printf("In NavCenterView:handleDisplayPixmap\n");
    // Get handle to the RDFImage object from the cache

    for (int i = 0; i < m_numRDFImagesLoaded; i ++)
    {
       if (w == selectorBarImagesCache[i].widget)
       {
         rdfImage = selectorBarImagesCache[i].rdfImage;
         break;
       }
    }
    rdfImage->RDFDisplayPixmap(image, mask, width, height);
}


void
XFE_NavCenterView::handleNewPixmap(Widget w, IL_Pixmap * image, Boolean mask)
{
     XFE_RDFImage *  rdfImage;

    printf("In NavCenterView:handlenewPixmap\n");
    // Get handle to the RDFImage object from the cache

    for (int i = 0; i < m_numRDFImagesLoaded; i ++)
    {
       if (w == selectorBarImagesCache[i].widget)
       {
         rdfImage = selectorBarImagesCache[i].rdfImage;
         break;
       }
    }
    rdfImage->RDFNewPixmap(image, mask);
}


void 
XFE_NavCenterView::handleImageComplete(Widget w, IL_Pixmap * image)
{


   XFE_RDFImage *  rdfImage;
   printf("In NavCenterView:handleImageComplete\n");
    // Get handle to the RDFImage object from the cache

    for (int i = 0; i < m_numRDFImagesLoaded; i ++)
    {
       if (w == selectorBarImagesCache[i].widget)
       {
         rdfImage = selectorBarImagesCache[i].rdfImage;
         break;
       }
    }
    rdfImage->RDFImageComplete(image);
}

extern "C" {
void ImageCompleteCallback(XtPointer client_data)
{
     callbackClientData * cb = (callbackClientData *) client_data;
     Widget button = (Widget )cb->widget;
     Dimension b_width=0, b_height=0;

     printf("Inside ImageCompleteCallback\n");

     XtUnmanageChild(button);
     XtVaGetValues(button, XmNwidth, &b_width, XmNheight, &b_height, NULL);

     XtVaSetValues(button, XmNheight,(cb->height + b_height),
                           XmNpixmap, cb->image, 
                           XmNpixmapMask, cb->mask,
                           XmNbuttonLayout, XmBUTTON_LABEL_ON_BOTTOM,
                           XmNlabelAlignment, XmALIGNMENT_CENTER,
                   NULL);
     XtManageChild(button);
     XP_FREE(cb);

}

};
