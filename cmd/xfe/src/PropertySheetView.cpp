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
   PropertySheetView.cpp -- class definition for PropertySheetView
   Created: Tao Cheng <tao@netscape.com>, 12-nov-96
 */



#include "PropertySheetView.h"
#include "PropertyTabView.h"
#include <XmL/Folder.h>

XFE_PropertySheetView::XFE_PropertySheetView(XFE_Component *top,
				     Widget parent, /* the dialog */
				     int *tabNameId, int nTabs):
  XFE_MNView(top, NULL, NULL, NULL)
{
  /* 1. Create the folder
   */

  Widget folder = XtCreateWidget("xmlFolder",
				 xmlFolderWidgetClass,
				 parent, NULL, 0);  
  XtAddCallback(folder, 
		XmNactivateCallback, XFE_PropertySheetView::tabActivateCB, this);

  /* set base widget
   */
  setBaseWidget(folder);

  /* 2. Create tab view
   */
  if (tabNameId)
    for (int i=0; i < nTabs; i++) {
      XFE_PropertyTabView *tab = new XFE_PropertyTabView(top, this,
					       tabNameId[i]);
      addView(tab);
      tab->show();
    }/* for i */
#if defined(DEBUG_tao_)
  else {
   printf("\n  Warning: [XFE_PropertySheetView::XFE_PropertySheetView]tabNameId is NULL!!");
  }/* else */
#endif

  /* set pos 0 as the default active tab
   */
  tabActivate(0);
}


XFE_PropertySheetView::~XFE_PropertySheetView()
{
}

void
XFE_PropertySheetView::tabActivate(int pos)
{
  getDlgValues();
  /* set active view
   */
  if (m_subviews && m_subviews[pos]) {
    /* if need to handle ex-active tab
     */

    /* reset activeTab
     */
    m_activeTab = (XFE_PropertyTabView *)m_subviews[pos];
    m_activeTab->setDlgValues();
  }/* if */
}

void
XFE_PropertySheetView::tabActivateCB(Widget /*w*/,
				 XtPointer clientData,
				 XtPointer callData)
{
  XFE_PropertySheetView *obj = (XFE_PropertySheetView *)clientData;
  XmLFolderCallbackStruct *cbs = (XmLFolderCallbackStruct*)callData;

  obj->tabActivate(cbs->pos);
}

XFE_PropertyTabView* XFE_PropertySheetView::addTab(int tabNameId)
{
  XFE_PropertyTabView *tab = new XFE_PropertyTabView(getToplevel(), this, 
					   tabNameId);
  addView(tab);
  tab->show();
  return tab;
}/* XFE_PropertySheetView::addTab */

void XFE_PropertySheetView::addTab(XFE_PropertyTabView* tab, XP_Bool show)
{
  addView(tab);
  if (show) tab->show();
}/* XFE_PropertySheetView::addTab */

void XFE_PropertySheetView::setDlgValues()
{
  if (m_subviews && m_numsubviews) {
    for (int i=0; i < m_numsubviews; i++) {
      ((XFE_PropertyTabView *) m_subviews[i])->setDlgValues();
    }/* for i */
  }/* if */
}/* XFE_PropertySheetView::setDlgValues() */

void XFE_PropertySheetView::getDlgValues()
{
  if (m_subviews && m_numsubviews) {
    for (int i=0; i < m_numsubviews; i++) {
      ((XFE_PropertyTabView *) m_subviews[i])->getDlgValues();
    }/* for i */
  }/* if */
}/* XFE_PropertySheetView::getDlgValues() */

void XFE_PropertySheetView::apply()
{
  if (m_subviews && m_numsubviews) {
    for (int i=0; i < m_numsubviews; i++) {
      ((XFE_PropertyTabView *) m_subviews[i])->apply();
    }/* for i */
  }/* if */
}/* XFE_PropertySheetView::apply() */
