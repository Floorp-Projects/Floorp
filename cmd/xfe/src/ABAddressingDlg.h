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
   ABAddressingDlg.h -- class definition for XFE_ABAddressingDlg
   Created: Tao Cheng <tao@netscape.com>, 20-nov-96
 */

#ifndef _ABADDRESSINGDLG_H_
#define _ABADDRESSINGDLG_H_

#if defined(GLUE_COMPO_CONTEXT)
#include "ViewDashBDlg.h"
#else
#include "ViewDialog.h"
#endif /* GLUE_COMPO_CONTEXT */

#include "addrbook.h"

extern "C" {
#include "addrbk.h"
};

class  XFE_AddrSearchView;
class  XFE_AddresseeView;

//
#if defined(GLUE_COMPO_CONTEXT)
class XFE_ABAddressingDlg: public XFE_ViewDashBDlg
#else
class XFE_ABAddressingDlg: public XFE_ViewDialog 
#endif /* GLUE_COMPO_CONTEXT */
{

public:

  XFE_ABAddressingDlg(Widget           parent,
					  char            *name,
					  ABAddrMsgCBProc  proc, 
					  void            *callData,
					  Boolean          modal,
					  MWContext       *context);

  virtual ~XFE_ABAddressingDlg();

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
