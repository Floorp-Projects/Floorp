/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
   ABNameSecueralTabView.h -- class definition for ABNameSecueralTabView
   Created: Tao Cheng <tao@netscape.com>, 12-nov-96
 */

#ifndef _xfe_abnamesecutabview_h
#define _xfe_abnamesecutabview_h

#include "PropertyTabView.h"

class XFE_ABNameSecuTabView: public XFE_PropertyTabView {
public:
  XFE_ABNameSecuTabView(XFE_Component *top,
			XFE_View      *view /* the parent view */);
  virtual ~XFE_ABNameSecuTabView();

  virtual void setDlgValues();

  static void showCallback(Widget, XtPointer, XtPointer);

protected:
  virtual void showCB(Widget, XtPointer);
  virtual void apply();
  virtual void getDlgValues();

private:
  /* m_widget is the tab form
   */
  Widget m_securLabel;
  Widget m_securMsgLabel;
  Widget m_securExpireDateLabel;
  Widget m_showCertiBtn;
}; /* XFE_ABNameSecuTabView */

#endif /* _xfe_abnamesecutabview_h */
