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
   NavCenterView.h - Aurora/NavCenter view class
   Created: Stephen Lamm <slamm@netscape.com>, 05-Nov-97.
 */



#include "NavCenterView.h"
#include "IconGroup.h"

#include <Xfe/XfeAll.h>

#if DEBUG_slamm
#define D(x) x
#else
#define D(x)
#endif

typedef struct _SelectorCBStruct {
  XFE_NavCenterView *ncview;
  HT_View view;
} SelectorCBStruct;

XFE_NavCenterView::XFE_NavCenterView(XFE_Component *toplevel_component,
                                     Widget parent, XFE_View *parent_view,
                                     MWContext *context)
  : XFE_View(toplevel_component, parent_view, context)
{
  D(printf("XFE_NavCenterView Constructor\n"););

#if 0
  Widget rdf_parent = XtVaCreateManagedWidget("pane", xfePaneWidgetClass,
                                        parent, NULL);

  m_selector = XtVaCreateManagedWidget("selector",
                   xfeToolScrollWidgetClass,
                   rdf_parent,
                   XmNpaneChildType,       XmPANE_CHILD_ATTACHMENT_ONE,
                   XmNpaneChildAttachment, XmPANE_CHILD_ATTACH_LEFT,
                   NULL);
  /*Widget form = XtVaCreateManagedWidget("form",
                   xmFormWidgetClass,
                   pane,
                   XmNpaneChildType,       XmPANE_CHILD_ATTACHMENT_TWO,
                   XmNpaneChildAttachment, XmPANE_CHILD_ATTACH_RIGHT,
                   XmNshadowThickness, 2,
                   XmNshadowType, XmSHADOW_IN,
                   NULL);*/
#endif /*0*/
  Widget nav_form = XtVaCreateManagedWidget("nav_form",
                                            xmFormWidgetClass,
                                            parent,
                                            NULL);

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
/*
                   XmNmarginTop,        2,
                   XmNmarginBottom,     2,
                   XmNmarginLeft,       2,
                   XmNmarginRight,      2,
*/
                   XmNspacing,          0,
                   XmNshadowThickness,  0,
                   NULL);
  Widget toolbar;
  XtVaGetValues(m_selector,XmNtoolBar,&toolbar,NULL);
  XtVaSetValues(toolbar,
                XmNshadowThickness,      0,
                NULL);

  Widget rdf_parent = XtVaCreateManagedWidget("rdf_form",
                          xmFormWidgetClass,
                          nav_form,
                          XmNtopAttachment,    XmATTACH_FORM,
                          XmNbottomAttachment, XmATTACH_FORM,
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

  m_htview = NULL;
  m_pane = NULL;
  m_rdfview = new XFE_RDFView(this, rdf_parent, 
                              NULL, context, m_htview);

  HT_Notification ns = new HT_NotificationStruct;
  ns->notifyProc = notify_cb;
  ns->data = this;

  m_pane = HT_NewPane(ns);
  HT_SetPaneFEData(m_pane, this);

  // add our subviews to the list of subviews for command dispatching and
  // deletion.
  addView(m_rdfview);

  XtManageChild(m_selector);
  m_rdfview->show();
  XtManageChild(nav_form);

  setBaseWidget(nav_form);
}


XFE_NavCenterView::~XFE_NavCenterView()
{
	D(printf("XFE_NavCenterView DESTRUCTING\n"););
}

//////////////////////////////////////////////////////////////////////////
XP_Bool
XFE_NavCenterView::isCommandEnabled(CommandType cmd,
								   void *calldata, XFE_CommandInfo*)
{
  if (cmd == xfeCmdChangeRDFView)
    {
      return TRUE;
    }
  else
    {
      return XFE_View::isCommandEnabled(cmd, calldata);
    }
}

void
XFE_NavCenterView::doCommand(CommandType cmd, void *calldata, XFE_CommandInfo*info)
{
  if (cmd == xfeCmdChangeRDFView)
    {
      int viewNum = (int)calldata;
      HT_View view = HT_GetNthView(m_pane,viewNum);

      setRDFView(view);

      return;
    }
  else
    {
      XFE_View::doCommand(cmd,calldata,info);
    }
}

Boolean
XFE_NavCenterView::handlesCommand(CommandType cmd, void *calldata,
                                  XFE_CommandInfo* info)
{
  if (cmd == xfeCmdChangeRDFView)
    {
      return TRUE;
    }
  else
    {
      return XFE_View::handlesCommand(cmd, calldata, info);
    }
}

XP_Bool
XFE_NavCenterView::isCommandSelected(CommandType cmd,
                                    void *calldata, XFE_CommandInfo* info)
{
    {
      return XFE_View::isCommandSelected(cmd, calldata, info);
    }
}

char *
XFE_NavCenterView::commandToString(CommandType cmd, void* calldata, 
                                   XFE_CommandInfo* info)
{
  if (cmd == xfeCmdChangeRDFView)
    {
      int viewNum = (int)calldata;

      return HT_GetViewName(HT_GetNthView(m_pane, viewNum));
    }
  return NULL;
}

//////////////////////////////////////////////////////////////////////////
void notify_cb(HT_Notification ns, HT_Resource n, 
                  HT_Event whatHappened)
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
      
      if (m_htview != view)
        setRDFView(view);
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
  
  Widget button = 
	  XtVaCreateManagedWidget(widget_name,
							  xfeButtonWidgetClass,
							  toolbar,
							  XmNlabelAlignment, XmALIGNMENT_BEGINNING,
							  NULL);

  // Temp bookmark title hack
  if (XP_STRNCMP(label,"Bookmarks for", 13) == 0) {
      label = "Bookmarks";
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

  printf("destroy selector cbdata\n");

  delete cbdata;
}
