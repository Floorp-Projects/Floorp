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
   ABAddrSearchView.h -- class definition for XFE_AddrSearchView
   Created: Tao Cheng <tao@netscape.com>, 20-nov-96
 */

#ifndef _ABADDRSEARCHVIEW_H_
#define _ABADDRSEARCHVIEW_H_

#include "ABListSearchView.h"
#include "addrbook.h"

class XFE_AddresseeView;

class XFE_AddrSearchView: public XFE_ABListSearchView
{

public:

  XFE_AddrSearchView(XFE_Component *toplevel_component, 
					 Widget         parent, 
					 XFE_View      *parent_view, 
					 MWContext     *context,
					 XP_List       *directories);
  virtual  ~XFE_AddrSearchView();
  void setAddressee(XFE_AddresseeView *view) { m_addresseeView = view;}
  void setAdd2ABBtn(Widget w) {m_addToAddressBtn = w;}
  void setPropertyBtn(Widget w) {m_propertyBtn = w;}
  void setToBtn(Widget w) {m_toBtn = w;}
  void setCcBtn(Widget w) {m_ccBtn = w;}
  void setBccBtn(Widget w) {m_bccBtn = w;}

  virtual void     doubleClickBody(const OutlineButtonFuncData *data);
  //
  static void toCallback(Widget, XtPointer, XtPointer);
  static void ccCallback(Widget, XtPointer, XtPointer);
  static void bccCallback(Widget, XtPointer, XtPointer);
  static void addToAddressCallback(Widget, XtPointer, XtPointer);

protected:
  virtual void toCB(Widget w, XtPointer callData);
  virtual void ccCB(Widget w, XtPointer callData);
  virtual void bccCB(Widget w, XtPointer callData);
  virtual void addToAddressCB(Widget w, XtPointer callData);

  StatusID_t *makePair(const int ind, SEND_STATUS status);
 
  XFE_CALLBACK_DECL(updateCommands)

private:

  Widget        m_addToAddressBtn;
  Widget        m_propertyBtn;
  //
  MSG_Master   *m_master;
  //
  Widget        m_toBtn;
  Widget        m_ccBtn;
  Widget        m_bccBtn;

  //
  XFE_AddresseeView *m_addresseeView;
};

#endif /* _ABADDRSEARCHVIEW_H_ */
