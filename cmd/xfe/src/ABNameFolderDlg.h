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
   ABNameFolderDlg.h -- class definition for ABNameFolderDlg
   Created: Tao Cheng <tao@netscape.com>, 12-nov-96
 */

#ifndef _xfe_abnamefolderdlg_h
#define _xfe_abnamefolderdlg_h

#include "PropertySheetDialog.h"

#include "addrbook.h"

class XFE_AddrBookView;
// 
class XFE_ABNameFolderDlg: public XFE_PropertySheetDialog {
public:
	XFE_ABNameFolderDlg(Widget    parent,
						char     *name,
						Boolean   modal,
						MWContext *context);
#if defined(USE_ABCOM)
	XFE_ABNameFolderDlg(MSG_Pane  *personPane,
						MWContext *context);
	void setDlgValues(MSG_Pane *pane);
#endif /* USE_ABCOM */
  virtual ~XFE_ABNameFolderDlg();

  //
  virtual void setDlgValues(ABID entry, Boolean newUser);
  virtual void setDlgValues(ABID entry, PersonEntry* pPerson, Boolean newUser);

  //
  Boolean           IsNewUser() { return m_newUser;}
  ABID              getEntryID() { return m_entry;}
  PersonEntry&      getPersonEntry() { return m_personEntry;}

  char*             getFullname();
  void              setCardName(char *name);
  DIR_Server*       getABDir();

protected:
  virtual void apply();

private:
  PersonEntry       m_personEntry;
  Boolean           m_newUser;
  ABID              m_entry;

}; /* XFE_PropertySheetDialog */

extern "C" void 
fe_showABCardPropertyDlg(Widget parent, MWContext *context, ABID entry, 
						 XP_Bool newuser);

extern "C" int 
fe_ShowPropertySheetForEntry(MSG_Pane *pane, MWContext *context);

#endif /* _xfe_xmlfolderdialog_h */
