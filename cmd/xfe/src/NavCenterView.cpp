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

#include <Xm/Form.h>

#ifdef MOZ_SELECTOR_BAR
#include <Xfe/ToolScroll.h>
#endif

#if DEBUG_slamm
#define D(x) x
#else
#define D(x)
#endif

//////////////////////////////////////////////////////////////////////////
XFE_NavCenterView::XFE_NavCenterView(XFE_Component *toplevel_component,
                                     Widget parent, XFE_View *parent_view,
                                     MWContext *context)

  : XFE_View(toplevel_component, parent_view, context),
    XFE_RDFBase(),
#ifdef MOZ_SELECTOR_BAR
    _selector(0),
#endif
    _rdftree(0),
    _isStandalone(0)
{
  m_viewType = VIEW_NAVCENTER;

  _isStandalone = (parent_view == NULL);

  // Register the MWContext in the XP list.
  XP_SetLastActiveContext(context);


  Widget navCenterMainForm = 
	  XtVaCreateManagedWidget("navCenterMainForm",
							  xmFormWidgetClass,
							  parent,
							  NULL);

  setBaseWidget(navCenterMainForm);

#ifdef MOZ_SELECTOR_BAR
  /* Create the selector bar only in the docked mode */
  if (!_isStandalone)
     createSelectorBar();
#endif

  createTree();

  doAttachments();

#ifdef MOZ_SELECTOR_BAR
  if (_selector)
     newPane();
#endif

  addView(_rdftree);

#ifdef MOZ_SELECTOR_BAR
  if (_selector)
    XtManageChild(_selector);
#endif /*MOZ_SELECTOR_BAR*/

  _rdftree->show();
}


XFE_NavCenterView::~XFE_NavCenterView()
{
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
      HT_View view = HT_GetView(n);

      selectHTView(view);
    }
    break;
  case HT_EVENT_VIEW_ADDED: 
    {
#ifdef MOZ_SELECTOR_BAR
      if (!_isStandalone && _selector) {
        HT_View view = HT_GetView(n);
        addHTView(view);
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
void
XFE_NavCenterView::finishPaneCreate()
{
#ifdef MOZ_SELECTOR_BAR
  if (_selector) {
    HT_SetPaneFEData(_ht_pane, this);

    _ht_view = HT_GetSelectedView(_ht_pane);
  }
  else
    XFE_RDFBase::finishPaneCreate();
#else
  XFE_RDFBase::finishPaneCreate();
#endif

  // This function takes a valid MWContext argument only when the
  // NavCenterView is docked.  However, we need a HT_Pane-to-context
  // mapping for properties and Find context menus to work. I think this
  // function should take a valid MWContext even in standalone mode
  // because we need these menu options to work in standalone mode too.
  // Maybe sitemaps will be screwed up when we start to use them.
  XP_RegisterNavCenter(_ht_pane, m_contextData);

  // Need to figure out what to do in popup state
  if (!_isStandalone)
     XP_DockNavCenter(_ht_pane, m_contextData);
}
//////////////////////////////////////////////////////////////////////
void
XFE_NavCenterView::deletePane()
{
  if (_ht_pane)
  {
      if (!_isStandalone)
          XP_UndockNavCenter(_ht_pane);
      XP_UnregisterNavCenter(_ht_pane);
  }

  XFE_RDFBase::deletePane();
}
//////////////////////////////////////////////////////////////////////
void
XFE_NavCenterView::selectHTView(HT_View view) 
{
    if (view != HT_GetSelectedView(_ht_pane))
    {
        HT_SetSelectedView(_ht_pane, view);
    }

    _ht_view = view;
    _rdftree->setHTView(view);

#ifdef MOZ_SELECTOR_BAR
    // The following block of code is to make sure that the view that is
    // currently selected appears as selected in the selector bar. This
    // is primarily useful when the user makes a node in the Rdftree as
    // a workspace. But it is currently broken since for some reason,
    // the selector bar is not responding to the
    // XfeToolBarSetSelectedButton() call

    Widget   toolbar;
    if (_selector) {
    XtVaGetValues(_selector, XmNtoolBar, &toolbar, NULL);

    Widget   button = XtNameToWidget(toolbar, HT_GetViewName(view));
    
    if (toolbar && button)
    {
        XfeToolBarSetSelectedButton(toolbar, button);
    }
    }
#endif
}
//////////////////////////////////////////////////////////////////////
#ifdef MOZ_SELECTOR_BAR
void
XFE_NavCenterView::addHTView(HT_View htview) 
{
//  static int counter=1;

  XFE_RDFImage *   rdfImage = NULL;
  char * imageURL = "http://people.netscape.com/radha/sony/images/LocationProxy.gif";
  Pixmap   image = (Pixmap)NULL;
  Pixmap   mask = (Pixmap)NULL;

  PRInt32  w,h;


  XP_ASSERT(htview);
  if (!htview) return;

  Widget toolbar;

  XtVaGetValues(_selector,XmNtoolBar,&toolbar,NULL);

  char *label = HT_GetViewName(htview);
  char *icon_url = HT_GetWorkspaceLargeIconURL(htview);

  char name[8];
  int ret = sscanf(icon_url, "icon/large:workspace,%s", name);
  if (ret != 0) {
    icon_url = XP_Cat("about:",name,".gif",NULL);

    // Load the icon here.
    // copy windows(navcntr.cpp)? call IL_GetImage? or IL_GetImagePixmap?

    XP_FREE(icon_url);
  }

  uint32 index = HT_GetViewIndex(htview);

  char widget_name[128];
  sprintf(widget_name,"button%d", index);
  


  // Temp bookmark title hack
  if (XP_STRNCMP(label,"Bookmarks for", 13) == 0) 
  {
      label = "Bookmarks";
  }
  // similar hack for Navigator internals
  if (XP_STRCASECMP(label,"Navigator internals") == 0)
  {
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
                   /*                    XmNbuttonLayout, XmBUTTON_PIXMAP_ONLY,  */
                   NULL);


  }
  else {
    //  Create  the image object and register callback

    rdfImage = new XFE_RDFImage(m_toplevel, (void *) this, imageURL,
                                CONTEXT_DATA(m_contextData)->colormap,
                                button);
    rdfImage->setCompleteCallback((completeCallbackPtr)image_complete_cb,
                                  (void *) button);
    rdfImage->loadImage();
  }


  XfeSetXmStringPSZ(button, XmNlabelString,
                    XmFONTLIST_DEFAULT_TAG, label);

  XtAddCallback(button,
                XmNactivateCallback,
                &XFE_NavCenterView::selector_activate_cb,
                (XtPointer) htview);
}
//////////////////////////////////////////////////////////////////////
void
XFE_NavCenterView::selector_activate_cb(Widget		/* w */,
                                        XtPointer	 clientData , 
                                        XtPointer	/* callData */)
{	
   HT_View htView = (HT_View)clientData;
   HT_Pane htPane = HT_GetPane(htView);

  XFE_NavCenterView * nc = (XFE_NavCenterView *)HT_GetPaneFEData(htPane);

  nc->setRdfTree(htView);
}
//////////////////////////////////////////////////////////////////////
void
XFE_NavCenterView::setRdfTree(HT_View    view)
{

  Widget toolbar;
  //  WidgetList tool_items = NULL;
  XtVaGetValues(_selector,XmNtoolBar,&toolbar,NULL);
  //XfeToolBarSetSelectedButton(toolbar, xxx);

  HT_SetSelectedView(_ht_pane, view);
  _ht_view = view;


}
//////////////////////////////////////////////////////////////////////
Widget 
XFE_NavCenterView::getSelector(void)
{
  return _selector;
}
#endif  /* MOZ_SELECTOR_BAR  */
//////////////////////////////////////////////////////////////////////
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

//////////////////////////////////////////////////////////////////////
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
     rdfImage->RDFNewPixmap(image, (PRBool)mask);
}

//////////////////////////////////////////////////////////////////////
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
//////////////////////////////////////////////////////////////////////
#ifdef MOZ_SELECTOR_BAR
/*static*/ void
XFE_NavCenterView::image_complete_cb(XtPointer client_data)
{
     callbackClientData * cb = (callbackClientData *) client_data;
     Widget button = (Widget )cb->widget;
     Dimension b_width=0, b_height=0;

#ifdef DEBUG_radha
     printf("Inside image_complete_cb\n");
#endif

     XtUnmanageChild(button);
     XtVaGetValues(button, XmNwidth, &b_width, XmNheight, &b_height, NULL);

     XtVaSetValues(button, /*  XmNheight,(cb->height + b_height),*/
				   XmNpixmap, cb->image, 
				   XmNpixmapMask, cb->mask,
                   XmNbuttonLayout, XmBUTTON_PIXMAP_ONLY,
                   NULL);
     XtManageChild(button);
     XP_FREE(cb);
}
#endif /*MOZ_SELECTOR_BAR*/
//////////////////////////////////////////////////////////////////////////
#ifdef MOZ_SELECTOR_BAR
void
XFE_NavCenterView::createSelectorBar()
{
  _selector = XtVaCreateManagedWidget("selector",
                   xfeToolScrollWidgetClass,
                   getBaseWidget(),
                   XmNtopOffset,        0,
                   XmNbottomOffset,     0,
                   XmNleftOffset,       0,
                   XmNrightOffset,      0,
                   XmNspacing,          0,
                   XmNshadowThickness,  0,
                   XmNselectionPolicy, XmTOOL_BAR_SELECT_SINGLE,
                   NULL);
  Widget toolbar;
  XtVaGetValues(_selector,XmNtoolBar,&toolbar,NULL);
  XtVaSetValues(toolbar,
                XmNshadowThickness,      0,
                NULL);
}
#endif /*MOZ_SELECTOR_BAR*/
//////////////////////////////////////////////////////////////////////////
void
XFE_NavCenterView::createTree()
{

	_rdftree = new XFE_RDFChromeTreeView(m_toplevel, getBaseWidget(),
										 this, _isStandalone ? RDF_PANE_STANDALONE : RDF_PANE_DOCKED, m_contextData);
	
	_rdftree->setStandAlone(_isStandalone);
    
}
//////////////////////////////////////////////////////////////////////////
void
XFE_NavCenterView::doAttachments()
{
#ifdef MOZ_SELECTOR_BAR
  if (_selector) {
    XtVaSetValues(_selector,
                  XmNtopAttachment,    XmATTACH_FORM,
                  XmNbottomAttachment, XmATTACH_FORM,
                  XmNleftAttachment,   XmATTACH_FORM,
                  XmNrightAttachment,  XmATTACH_NONE,
                  NULL);

    XtVaSetValues(_rdftree->getBaseWidget(),
                  XmNtopAttachment,    XmATTACH_FORM,
                  XmNbottomAttachment, XmATTACH_FORM,
                  XmNleftAttachment,   XmATTACH_WIDGET,
                  XmNleftWidget,       _selector,
                  XmNrightAttachment,  XmATTACH_FORM,
                  NULL);
  }
  else
#endif
    {
    XtVaSetValues(_rdftree->getBaseWidget(),
                  XmNtopAttachment,    XmATTACH_FORM,
                  XmNbottomAttachment, XmATTACH_FORM,
                  XmNleftAttachment,   XmATTACH_FORM,
                  XmNrightAttachment,  XmATTACH_FORM,
                  NULL);
  }
}
//////////////////////////////////////////////////////////////////////////
