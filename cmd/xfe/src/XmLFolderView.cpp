/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1996
 * Netscape Communications Corporation.  All Rights Reserved.
 */


/* -*- Mode: C++; tab-width: 4 -*-
   XmLFolderView.cpp -- class definition for XmLFolderView
   Created: Tao Cheng <tao@netscape.com>, 12-nov-96
 */

#include "XmLFolderView.h"
#include "TabView.h"
#include <XmL/Folder.h>

XFE_XmLFolderView::XFE_XmLFolderView(XFE_Component *top,
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
		XmNactivateCallback, XFE_XmLFolderView::tabActivateCB, this);

  /* set base widget
   */
  setBaseWidget(folder);

  /* 2. Create tab view
   */
  if (tabNameId)
    for (int i=0; i < nTabs; i++) {
      XFE_XmLTabView *tab = new XFE_XmLTabView(top, this,
					       tabNameId[i]);
      addView(tab);
      tab->show();
    }/* for i */
#if defined(DEBUG_tao_)
  else {
   printf("\n  Warning: [XFE_XmLFolderView::XFE_XmLFolderView]tabNameId is NULL!!");
  }/* else */
#endif

  /* set pos 0 as the default active tab
   */
  tabActivate(0);
}


XFE_XmLFolderView::~XFE_XmLFolderView()
{
}

void
XFE_XmLFolderView::tabActivate(int pos)
{
  getDlgValues();
  /* set active view
   */
  if (m_subviews && m_subviews[pos]) {
    /* if need to handle ex-active tab
     */

    /* reset activeTab
     */
    m_activeTab = (XFE_XmLTabView *)m_subviews[pos];
    m_activeTab->setDlgValues();
  }/* if */
}

void
XFE_XmLFolderView::tabActivateCB(Widget /*w*/,
				 XtPointer clientData,
				 XtPointer callData)
{
  XFE_XmLFolderView *obj = (XFE_XmLFolderView *)clientData;
  XmLFolderCallbackStruct *cbs = (XmLFolderCallbackStruct*)callData;

  obj->tabActivate(cbs->pos);
}

XFE_XmLTabView* XFE_XmLFolderView::addTab(int tabNameId)
{
  XFE_XmLTabView *tab = new XFE_XmLTabView(getToplevel(), this, 
					   tabNameId);
  addView(tab);
  tab->show();
  return tab;
}/* XFE_XmLFolderView::addTab */

void XFE_XmLFolderView::addTab(XFE_XmLTabView* tab, XP_Bool show)
{
  addView(tab);
  if (show) tab->show();
}/* XFE_XmLFolderView::addTab */

void XFE_XmLFolderView::setDlgValues()
{
  if (m_subviews && m_numsubviews) {
    for (int i=0; i < m_numsubviews; i++) {
      ((XFE_XmLTabView *) m_subviews[i])->setDlgValues();
    }/* for i */
  }/* if */
}/* XFE_XmLFolderView::setDlgValues() */

void XFE_XmLFolderView::getDlgValues()
{
  if (m_subviews && m_numsubviews) {
    for (int i=0; i < m_numsubviews; i++) {
      ((XFE_XmLTabView *) m_subviews[i])->getDlgValues();
    }/* for i */
  }/* if */
}/* XFE_XmLFolderView::getDlgValues() */

void XFE_XmLFolderView::apply()
{
  if (m_subviews && m_numsubviews) {
    for (int i=0; i < m_numsubviews; i++) {
      ((XFE_XmLTabView *) m_subviews[i])->apply();
    }/* for i */
  }/* if */
}/* XFE_XmLFolderView::apply() */
