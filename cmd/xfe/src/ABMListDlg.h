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
   ABMListDlg.h -- class definition for XFE_ABMListDlg
   Created: Tao Cheng <tao@netscape.com>, 12-nov-96
 */

#ifndef _ABMLISTDLG_H_
#define _ABMLISTDLG_H_


#include "ViewDialog.h"

#include "addrbook.h"

class XFE_AddrBookView;

class XFE_ABMListDlg: public XFE_ViewDialog 
{

public:
  XFE_ABMListDlg(XFE_View  *view, /* the parent view */
				 Widget     parent,
				 char      *name,
				 Boolean    modal,
				 MWContext *context);
#if defined(USE_ABCOM)
  XFE_ABMListDlg(MSG_Pane  *pane,
				 MWContext *context);
#endif /* USE_ABCOM */

  virtual void setDlgValues();

  ~XFE_ABMListDlg();

  //
  void createUI();
  virtual void setDlgValues(ABID entry, Boolean newList);
  virtual void getDlgValues();

  //
  MailingListEntry& getMList() { return m_mailListEntry;}
  Boolean           IsNewList() { return m_newList;}
  ABID              getEntryID() { return m_entry;}
  XFE_AddrBookView* getABView() { return m_abView;}

  void Initialize();
  static void entryTTYValChgCallback(Widget, XtPointer, XtPointer);

protected:
  virtual void cancel();
  virtual void apply();
  virtual void ok();
  virtual void entryTTYValChgCB(Widget, XtPointer);

private:
  Widget            m_textFs[4];

  XFE_AddrBookView *m_abView;

  MailingListEntry  m_mailListEntry;
  Boolean           m_newList;
  ABID              m_entry;

  DIR_Server       *m_dir;
#if defined(USE_ABCOM)
  MSG_Pane         *m_pane;
#endif /* USE_ABCOM */
  MLPane           *m_mListPane;
  ABook            *m_AddrBook;
};

#if defined(USE_ABCOM)
extern "C" int
fe_ShowPropertySheetForMList(MSG_Pane *pane, MWContext *context);
#endif /* USE_ABCOM */

#endif /* _ABMLISTDLG_H_ */
