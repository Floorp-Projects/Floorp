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
   ComposeFolderView.h -- class definition for ComposeFolderView
   Created: Dora Hsu<dora@netscape.com>, 23-Sept-96.
 */



#ifndef _xfe_composefolderview_h
#define _xfe_composefolderview_h


#include "MNView.h"

#include "AddressFolderView.h"
#include "OptionFolderView.h"
#include "ComposeAttachFolderView.h"

#include "msgcom.h"
#include <Xm/Xm.h>

/* Compose Folder View */


class XFE_ComposeFolderView : public XFE_MNView
{
public:
  XFE_ComposeFolderView(XFE_Component *toplevel_component, XFE_View *parent_view,
			MSG_Pane *p = NULL, MWContext *context = NULL);


  // Public functions
  XFE_AddressFolderView* getAddrFolderView() { 
    return m_addressFolderViewAlias;}
  XFE_ComposeAttachFolderView* getAttachFolderView() { 
    return m_attachFolderViewAlias;}

  virtual XP_Bool isCommandSelected(CommandType cmd, void* calldata = NULL,
									XFE_CommandInfo* i = NULL);
  virtual Boolean isCommandEnabled(CommandType command, void *calldata = NULL,
								   XFE_CommandInfo* i = NULL);
  virtual Boolean handlesCommand(CommandType command, void *calldata = NULL,
								   XFE_CommandInfo* i = NULL);
  virtual void doCommand(CommandType command, void *calldata = NULL,
								   XFE_CommandInfo* i = NULL);

  void   createWidgets(Widget parent, XP_Bool usePlainText);
  void   updateHeaderInfo();
  void 	 selectPriority(MSG_PRIORITY priority);
  Boolean isPrioritySelected(MSG_PRIORITY priority );

protected:

private:
	Widget  m_tabGroupW;
	Widget 	m_addressFormW;
	Widget 	m_attachFormW;
	Widget	m_frameW;

	int	m_tabNumber;
	Widget  m_optionFormW;
	Widget  m_optionW;
	MSG_PRIORITY 	m_selectedPriority;
	static fe_icon *addressIcon;
	static fe_icon *attachIcon;
	static fe_icon *optionIcon;

  XFE_AddressFolderView *m_addressFolderViewAlias;
  XFE_OptionFolderView *m_optionFolderViewAlias;
  XFE_ComposeAttachFolderView *m_attachFolderViewAlias;

  void	 setupIcons();
  void	 folderActivate(int);


  static void folderActivateCallback(Widget, XtPointer, XtPointer);
};


#endif /* _xfe_composefolderview_h */
