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
   LdapSearchView.h -- class definitions for ldap search view.
   Created: Dora Hsu <dora@netscape.com>, 15-Dec-96.
 */

#ifndef _xfe_ldapsearchview_h
#define _xfe_ldapsearchview_h

#include "MNSearchView.h"
#include "Outliner.h"
#include "Command.h"
#include "dirprefs.h"
#include "BrowserFrame.h"
#include "Dashboard.h"
#include "AddressFolderView.h"


class XFE_LdapSearchView: public XFE_MNSearchView
{
public:
	XFE_LdapSearchView(XFE_Component *toplevel_component,
					   Widget parent,
					   XFE_Frame * parent_frame,
					   XFE_MNView *mn_parentView,
					   MWContext *context, MSG_Pane *p); 

  virtual ~XFE_LdapSearchView();

  static const char* CloseLdap;
  static const int SEARCH_OUTLINER_COLUMN_NAME;
  static const int SEARCH_OUTLINER_COLUMN_EMAIL;
  static const int SEARCH_OUTLINER_COLUMN_COMPANY;
  static const int SEARCH_OUTLINER_COLUMN_PHONE;
  static const int SEARCH_OUTLINER_COLUMN_LOCALITY;

  virtual char *getColumnHeaderText(int column);
  virtual char *getColumnText(int column);
  virtual fe_icon *getColumnIcon(int column);
  static void toAddrBookCallback(Widget, XtPointer, XtPointer);
  static void toComposeCallback(Widget, XtPointer, XtPointer);

  virtual void paneChanged(XP_Bool /*asynchronous*/,
                                MSG_PANE_CHANGED_NOTIFY_CODE /* notify_code */,
                                int32 /*value*/);
  void addSelectedToAddressPane(XFE_AddressFolderView*,SEND_STATUS);    
  virtual void toggleActionButtonState(Boolean on);
  void createWidgets(Widget formParent); // override to adopt US_WEST behavior

protected:
  // dialog callbacks
  virtual void toAddrBookCB(Widget w, XtPointer callData);
  virtual void toComposeCB(Widget w, XtPointer callData);

  virtual void addDefaultFolders();
  virtual void buildHeaderOption();
  virtual void prepSearchScope();
  virtual void doubleClick(const OutlineButtonFuncData *data);
  virtual void miscCmd(); // For Help btn
  virtual void handleClose();
  virtual void doLayout();
  void buildResultTable();
  void buildResult();
  void clickHeader(const OutlineButtonFuncData *data);

private:
  void initialize();
  void changeScope();
  static void folderOptionCallback(Widget w, XtPointer clientData, XtPointer);


  DIR_Server* getDirServer();

  /* Search Tool Commands */
  Widget m_toAddrBook;
  Widget m_toCompose;
  
  XP_List    *m_directories;
  XFE_BrowserFrame *m_browserFrame;
};
#endif
