/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
/* 
   ABNameCTalkTab.h -- class definition for XFE_ABNameCTalkTabView
   Created: Tao Cheng <tao@netscape.com>, 12-nov-96
 */

#ifndef _xfe_abnamectalktabview_h
#define _xfe_abnamectalktabview_h

#include "PropertyTabView.h"

class XFE_ABNameCTalkTabView: public XFE_PropertyTabView {
public:
  XFE_ABNameCTalkTabView(XFE_Component *top, /* the parent folderDialog */
			 XFE_View      *view /* the parent view */);
  virtual ~XFE_ABNameCTalkTabView();

  virtual void setDlgValues();

  static void comboSelCallback(Widget, XtPointer, XtPointer);

protected:
  virtual void comboSelCB(Widget w, XtPointer callData);
  virtual void apply();
  virtual void getDlgValues();

private:
  /* m_widget is the tab form
   */
  Widget m_serverCombo;
  Widget m_typeinTextForm;	
  Widget m_textFs[2];
  Widget m_serverExample;

  /* server type 
   */
  short  m_serverType;

  void setServerState(short serverType, char* serverName);	
}; /* XFE_ABNameCTalkTabView */

#endif /* _xfe_abnamectalktabview_h */
