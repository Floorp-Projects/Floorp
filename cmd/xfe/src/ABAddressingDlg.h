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
   ABAddressingDlg.h -- class definition for XFE_ABAddressingDlg
   Created: Tao Cheng <tao@netscape.com>, 20-nov-96
 */

#ifndef _ABADDRESSINGDLG_H_
#define _ABADDRESSINGDLG_H_

#include "ViewDialog.h"

#include "addrbook.h"

extern "C" {
#include "addrbk.h"
};

class  XFE_AddrSearchView;
class  XFE_AddresseeView;

//
class XFE_ABAddressingDlg: public XFE_ViewDialog 
{

public:

  XFE_ABAddressingDlg(Widget           parent,
		      char            *name,
		      ABAddrMsgCBProc  proc, 
		      void            *callData,
		      Boolean          modal,
		      MWContext *context);

  ~XFE_ABAddressingDlg();

  enum { AB_SEARCH_VIEW = 0,
	 AB_ADDRESSEE_VIEW,
	 AB_LAST
  } AB_VIEWTYPE;

  void addAddressees(ABAddrMsgCBProcStruc *);

protected:
  virtual void cancel();
  virtual void ok();

private:

  XFE_AddrSearchView    *m_addrSearchView;
  XFE_AddresseeView     *m_addresseeView;

  ABAddrMsgCBProc        m_cbProc;
  void                  *m_callData;

  MWContext             *m_searchContext;
};

#endif /* _ABADDRESSINGDLG_H_ */
