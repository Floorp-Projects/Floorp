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
   AddrBookFrame.h -- class definitions for address book frames.
   Created: Chris Toshok <toshok@netscape.com>, 29-Aug-96.
   Revised: Tao Cheng <tao@netscape.com>, 01-nov-96
 */

#ifndef _xfe_addressbookframe_h
#define _xfe_addressbookframe_h

#include "Frame.h"
#include "PopupMenu.h"

#include "xp_core.h"
#include <Xm/Xm.h>

class XFE_AddrBookView;

class XFE_AddrBookFrame : public XFE_Frame
{
public:
  XFE_AddrBookFrame(Widget toplevel, XFE_Frame *parent_frame, Chrome *chromespec = NULL);
  virtual ~XFE_AddrBookFrame();

  // Dealing with command
  virtual XP_Bool isOkToClose();
  virtual char *commandToString(CommandType cmd, void *calldata = NULL,
						 XFE_CommandInfo* = NULL);
  virtual XP_Bool handlesCommand(CommandType cmd, void *calldata = NULL,
						 XFE_CommandInfo* info = NULL);
  virtual XP_Bool isCommandEnabled(CommandType cmd, 
				   void *calldata = NULL,
						 XFE_CommandInfo* = NULL);
  virtual void doCommand(CommandType cmd, void *calldata = NULL,
						 XFE_CommandInfo* = NULL);

  //
  static XFE_Frame *m_theFrame;

protected:

  // tooltips and doc string
  virtual char *getDocString(CommandType cmd);
  virtual char *getTipString(CommandType cmd);

  virtual void openBrowser();
  virtual void composeMessage();
  virtual void abAddToMessage();
  virtual void import();
  virtual void saveAs();
  virtual void close();

  virtual void undo();
  virtual void redo();
  virtual void abDelete();
  virtual void abDeleteAllEntries();

  virtual void addToAddressBook();
  virtual void newList();
  virtual void viewProperties();
  virtual void abCall();
  virtual void abVCard();
 
private:

  XFE_AddrBookView *m_abView;

  /* menu specs
   */
  static MenuSpec menu_bar_spec[];
  static ToolbarSpec toolbar_spec[];
#if defined(USE_ABCOM)
  static MenuSpec new_sub_menu_spec[];
#endif /* USE_ABCOM */
  static MenuSpec file_menu_spec[];
  static MenuSpec edit_menu_spec[];
  static MenuSpec view_menu_spec[];
  // static MenuSpec navigate_menu_spec[];
  //  static MenuSpec help_menu_spec[];
  XFE_PopupMenu *m_popup;

  static MenuSpec frame_popup_spec[];

  // Toolbox methods
  virtual void		toolboxItemClose		(XFE_ToolboxItem * item);
  virtual void		toolboxItemOpen		(XFE_ToolboxItem * item);
  virtual void		toolboxItemChangeShowing(XFE_ToolboxItem * item);

  virtual void		configureToolbox	();
};

extern "C" MWContext* fe_showAddrBook(Widget toplevel, XFE_Frame *parent_frame, Chrome *chromespec);

#endif /* _xfe_mailfilterframe_h */
