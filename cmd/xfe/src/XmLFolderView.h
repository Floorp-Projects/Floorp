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
   XmlFolderView.h -- class definition for XFE_XmlFolderView
   Created: Tao Cheng <tao@netscape.com>, 12-nov-96
 */

#ifndef _xfe_xmlfolderview_h
#define _xfe_xmlfolderview_h

#include "MNView.h"

class XFE_XmLTabView;

// This is a general wrapper of XmLFloder widget
class XFE_XmLFolderView: public XFE_MNView {

public:
  XFE_XmLFolderView(XFE_Component *top, /* the parent folderDialog */
					Widget         parent, 
					int           *tabNameId, 
					int nTabs);
  virtual ~XFE_XmLFolderView();

  //
  XFE_XmLTabView* addTab(int tabNameId);
  void addTab(XFE_XmLTabView* tab, XP_Bool show=TRUE);

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
  XFE_XmLTabView *m_activeTab;
}; /* XFE_XmLFolderView */

#endif /* _xfe_xmlfolderview_h */
