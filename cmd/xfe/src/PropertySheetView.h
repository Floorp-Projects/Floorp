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
   XmlFolderView.h -- class definition for XFE_XmlFolderView
   Created: Tao Cheng <tao@netscape.com>, 12-nov-96
 */



#ifndef _xfe_xmlfolderview_h
#define _xfe_xmlfolderview_h

#include "MNView.h"

class XFE_PropertyTabView;

// This is a general wrapper of a widget
class XFE_PropertySheetView: public XFE_MNView {

public:
  XFE_PropertySheetView(XFE_Component *top, /* the parent folderDialog */
					Widget         parent, 
					int           *tabNameId, 
					int nTabs);
  virtual ~XFE_PropertySheetView();

  //
  XFE_PropertyTabView* addTab(int tabNameId);
  void addTab(XFE_PropertyTabView* tab);

  virtual void setDlgValues();
  virtual void getDlgValues();
  virtual void apply();

protected:

  // invoked when you switch tabs
  virtual void tabActivate(int pos);
  static  void tabActivateCB(Widget, XtPointer, XtPointer);

private:
  /* m_widget is the folder widget 
   */
  /* m_subviews contains the sub folder view: XFE_FolderView
   */
  XFE_PropertyTabView *m_activeTab;
}; /* XFE_PropertySheetView */

#endif /* _xfe_xmlfolderview_h */
