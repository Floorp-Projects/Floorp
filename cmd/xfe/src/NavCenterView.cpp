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
#include "RDFChromeTreeView.h"
#include "IconGroup.h"
#include "xp_ncent.h"

#include <Xfe/XfeAll.h>

#if DEBUG_slamm
#define D(x) x
#else
#define D(x)
#endif

extern "C"
{
void ncview_image_complete_cb(XtPointer client_data);

};


#ifdef MOZ_SELECTOR_BAR
typedef struct _SelectorCBStruct {
  XFE_NavCenterView *ncview;
  HT_View view;
} SelectorCBStruct;
#endif /*MOZ_SELECTOR_BAR*/

//////////////////////////////////////////////////////////////////////////
XFE_NavCenterView::XFE_NavCenterView(XFE_Component *toplevel_component,
                                     Widget parent, XFE_View *parent_view,
                                     MWContext *context)

  : XFE_View(toplevel_component, parent_view, context)
{
  D(printf("XFE_NavCenterView Constructor\n"););


  // This may need to become a constructor argument,
  // but this is good enough for now.
  m_isStandalone = (parent_view == NULL);

  // Set the view type.
  m_viewType = VIEW_NAVCENTER;

  Widget navCenterMainForm = 
	  XtVaCreateManagedWidget("navCenterMainForm",
							  xmFormWidgetClass,
							  parent,
							  NULL);

  setBaseWidget(navCenterMainForm);

#ifdef MOZ_SELECTOR_BAR
  m_selector = XtVaCreateManagedWidget("selector",
                   xfeToolScrollWidgetClass,
                   navCenterMainForm,
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
#else
  // hack to pick the first view added (this will go away soon)
  _firstViewAdded = 0;
#endif /*MOZ_SELECTOR_BAR*/

  m_rdfview = new XFE_RDFChromeTreeView(this, m_widget, this, context);

  XtVaSetValues(m_rdfview->getBaseWidget(),
                XmNtopAttachment,    XmATTACH_FORM,
#ifdef HTML_PANE
                XmNbottomAttachment, XmATTACH_NONE,
#else
                XmNbottomAttachment, XmATTACH_FORM,
#endif
#ifdef MOZ_SELECTOR_BAR
                XmNleftAttachment,   XmATTACH_WIDGET,
                XmNleftWidget,       m_selector,
#else
                XmNleftAttachment,   XmATTACH_FORM,
#endif /*MOZ_SELECTOR_BAR*/
                XmNrightAttachment,  XmATTACH_FORM,


// Put this stuff in the resource file
//                 XmNtopOffset,        0,
//                 XmNbottomOffset,     1,
//                 XmNleftOffset,       0,
//                 XmNrightOffset,      0,
//                 XmNshadowThickness,  2,
//                 XmNshadowType,       XmSHADOW_IN,
                NULL);

  m_rdfview->setStandAloneState(m_isStandalone);

#ifdef HTML_PANE
  m_htmlview = new XFE_HTMLView(this, navCenterMainForm, NULL, context);
  Widget html_base = m_htmlview->getBaseWidget();

  XtVaSetValues(html_base,
                XmNtopAttachment,    XmATTACH_WIDGET,
                XmNtopWidget,        m_rdfview->getBaseWidget(),
                XmNbottomAttachment, XmATTACH_FORM,
                XmNleftAttachment,   XmATTACH_WIDGET,
#ifdef MOZ_SELECTOR_BAR
                XmNleftAttachment,   XmATTACH_WIDGET,
                XmNleftWidget,       m_selector,
#else
                XmNleftAttachment,   XmATTACH_FORM,
#endif /*MOZ_SELECTOR_BAR*/
                XmNrightAttachment,  XmATTACH_FORM,
                NULL);
#endif

  newPane();

  // Register the MWContext in the XP list.
  XP_SetLastActiveContext(context);


  /** 
    * We need to register our MWContext and pane with the XP's list. 
    * This function takes a valid MWContext argument only when the 
    * NavCenterView is docked. I don't know why?. i also don't know what 
    * happens in the popup mode. In XFE however, we need a HT_Pane-to-context
    * mapping for properties and Find context menus to work. I think this
    *  function s'd  take a valid MWContext even in standalone mode because 
    * we need these menu options to work in standalone mode too. So, I'm
    *  going to call this function with a valid MWContext irespective of 
    * what the mode is. Maybe sitemap will be screwed up when we get there. 
    * But  we are not there yet. 
    **/
  XP_RegisterNavCenter(_ht_pane, context);

  // Need to figure out what to do in popup state
  if (!m_isStandalone)
     XP_DockNavCenter(_ht_pane, context);

  // add our subviews to the list of subviews for command dispatching and
  // deletion.
  addView(m_rdfview);
#ifdef HTML_PANE
  addView(m_htmlview);
#endif 

#ifdef MOZ_SELECTOR_BAR
  XtManageChild(m_selector);
#endif /*MOZ_SELECTOR_BAR*/
  m_rdfview->show();
#ifdef HTML_PANE
  m_htmlview->show();
  m_htmlview->getURL(NET_CreateURLStruct("http://dunk/",NET_DONT_RELOAD));
#endif

}


XFE_NavCenterView::~XFE_NavCenterView()
{
	D(printf("XFE_NavCenterView DESTRUCTING\n"););
    if (_ht_pane)
    {
        if (!m_isStandalone)
          XP_UndockNavCenter(_ht_pane);
        XP_UnregisterNavCenter(_ht_pane);
    }
    // Remove yourself from XFE_RDFImage's listener list
    XFE_RDFImage::removeListener(this);

}
//////////////////////////////////////////////////////////////////////////
void
XFE_NavCenterView::notify(HT_Resource		n, 
						  HT_Event			whatHappened)
{
  D(debugEvent(n, whatHappened,"NCV"););

  switch (whatHappened) {
  case HT_EVENT_VIEW_CLOSED:
    break;
  case HT_EVENT_VIEW_SELECTED:
    {
#ifdef MOZ_SELECTOR_BAR
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

       if ((_ht_view != view)  && toolbar && button) {
          // Set the RDFView
          setRDFView(view);
          // Make sure the corresponding button is marked as selected.
          XfeToolBarSetSelectedButton(toolbar, button);

       }
#endif /*MOZ_SELECTOR_BAR*/      
    }
    break;
  case HT_EVENT_VIEW_ADDED: 
    {
#ifdef MOZ_SELECTOR_BAR
      HT_View view = HT_GetView(n);
      
      addRDFView(view);
#else
      // hack to pick the first view added (this will go away soon)
      if (!_firstViewAdded) {
          HT_View view = HT_GetView(n);
          HT_Pane pane = HT_GetPane(view);
          m_rdfview->setHTView(view);
          HT_SetSelectedView(pane, view);
          _ht_view = view;

          _firstViewAdded = 1;
      }
#endif
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
  XFE_RDFBase::notify(n, whatHappened);
}
//////////////////////////////////////////////////////////////////////
#ifdef MOZ_SELECTOR_BAR
void
XFE_NavCenterView::setRDFView(HT_View view) 
{
  Widget toolbar;
  //  WidgetList tool_items = NULL;
  XtVaGetValues(m_selector,XmNtoolBar,&toolbar,NULL);
  //XfeToolBarSetSelectedButton(toolbar, xxx);

  m_rdfview->setHTView(view);

  HT_SetSelectedView(_ht_pane, view);
  _ht_view = view;
}
//////////////////////////////////////////////////////////////////////
void
XFE_NavCenterView::addRDFView(HT_View view) 
{
 
//  static int counter=1;

  XFE_RDFImage *   rdfImage = NULL;
  char * imageURL = "http://people.netscape.com/radha/sony/images/LocationProxy.gif";
  Pixmap   image = (Pixmap)NULL;
  Pixmap   mask = (Pixmap)NULL;
  PRInt32  w,h;


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

  /* Check if the image is already available in the
   * RDFImage Cache. If so, use it 
   */

  rdfImage = XFE_RDFImage::isImageAvailable(imageURL);
  if (rdfImage) {
     image = rdfImage->getPixmap();
     mask = rdfImage->getMask();
  
     XtVaGetValues(button, 
                   XmNwidth, &w,
                   XmNheight, &h,
                   NULL);
   
     XtVaSetValues(button, 				   
                   XmNpixmap, image, 
				   XmNpixmapMask, mask,
                   XmNwidth, (unsigned int)(w = rdfImage->getImageWidth()),
                   XmNheight, (unsigned int)(h = rdfImage->getImageHeight()),
				   XmNbuttonLayout, XmBUTTON_PIXMAP_ONLY,
                   NULL);


  }
  else {
    /*  Create  the image object and register callback */ 

       rdfImage = new XFE_RDFImage(m_toplevel, (void *) this, imageURL, CONTEXT_DATA(m_contextData)->colormap, button);
    rdfImage->setCompleteCallback((completeCallbackPtr)ncview_image_complete_cb, (void *) button);
    rdfImage->loadImage();
  }


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
XFE_NavCenterView::selector_activate_cb(Widget		/* w */,
                                        XtPointer	clientData, 
                                        XtPointer	/* callData */)
{	
  SelectorCBStruct *cbdata = (SelectorCBStruct *)clientData;

  cbdata->ncview->setRDFView(cbdata->view);
}
//////////////////////////////////////////////////////////////////////
void
XFE_NavCenterView::selector_destroy_cb(Widget		/* w */,
                                        XtPointer	clientData, 
                                        XtPointer	/* callData */)
{	
  SelectorCBStruct *cbdata = (SelectorCBStruct *)clientData;

  delete cbdata;
}

Widget 
XFE_NavCenterView::getSelector(void)
{
  return (m_selector);

}
#endif /*MOZ_SELECTOR_BAR*/

 void 
XFE_NavCenterView::handleDisplayPixmap(Widget w, IL_Pixmap * image, IL_Pixmap * mask, PRInt32  width, PRInt32 height)
{
   XFE_RDFImage *  rdfImage;

#ifdef DEBUG_radha
   printf("In NavCenterView:handleDisplayPixmap\n");
#endif
    // Get handle to the RDFImage object from the cache

   rdfImage = XFE_RDFImage::getRDFImageObject(w);
   if (rdfImage)
     rdfImage->RDFDisplayPixmap(image, mask, width, height);
}


void
XFE_NavCenterView::handleNewPixmap(Widget w, IL_Pixmap * image, Boolean mask)
{
     XFE_RDFImage *  rdfImage;

#ifdef DEBUG_radha
    printf("In NavCenterView:handlenewPixmap\n");
#endif
    // Get handle to the RDFImage object from the cache

   rdfImage = XFE_RDFImage::getRDFImageObject(w);
   if (rdfImage)
     rdfImage->RDFNewPixmap(image, mask);
}


void 
XFE_NavCenterView::handleImageComplete(Widget w, IL_Pixmap * image)
{


   XFE_RDFImage *  rdfImage;

#ifdef DEBUG_radha
   printf("In NavCenterView:handleImageComplete\n");
#endif
    // Get handle to the RDFImage object from the cache

   rdfImage = XFE_RDFImage::getRDFImageObject(w);
   if (rdfImage)
    rdfImage->RDFImageComplete(image);
}

extern "C" {
void ncview_image_complete_cb(XtPointer client_data)
{
     callbackClientData * cb = (callbackClientData *) client_data;
     Widget button = (Widget )cb->widget;
     Dimension b_width=0, b_height=0;

#ifdef DEBUG_radha
     printf("Inside ncview_ImageCompleteCallback\n");
#endif

     XtUnmanageChild(button);
     XtVaGetValues(button, XmNwidth, &b_width, XmNheight, &b_height, NULL);

     XtVaSetValues(button,/*  XmNheight,(cb->height + b_height), */
				   XmNpixmap, cb->image, 
				   XmNpixmapMask, cb->mask,
				   XmNbuttonLayout, XmBUTTON_PIXMAP_ONLY,
                   NULL);
     XtManageChild(button);
     XP_FREE(cb);

}

};
