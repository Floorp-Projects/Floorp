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
   BrowserFrame.h -- class definition for the browser frame class
   Created: Spence Murray <spence@netscape.com>, 17-Oct-96.
 */



#ifndef _xfe_browserframe_h
#define _xfe_browserframe_h

#include "Frame.h"
#include "URLBar.h"
#include "Dashboard.h"
#include "xp_core.h"
#include "BrowserDrop.h"
#include "PersonalToolbar.h"
#include <Xm/Xm.h>

class XFE_BrowserFrame : public XFE_Frame
{
public:
  XFE_BrowserFrame(Widget toplevel, XFE_Frame *parent_frame, Chrome *chromespec);
  virtual ~XFE_BrowserFrame();

  virtual void updateToolbar();
  
  virtual XP_Bool isCommandEnabled(CommandType cmd, void *calldata = NULL,
						 XFE_CommandInfo* = NULL);
  virtual void doCommand(CommandType cmd, void *calldata = NULL,
						 XFE_CommandInfo* = NULL);
  virtual XP_Bool handlesCommand(CommandType cmd, void *calldata = NULL,
						 XFE_CommandInfo* = NULL);
  virtual char *commandToString(CommandType cmd, void *calldata = NULL,
						 XFE_CommandInfo* = NULL);

  virtual int getURL(URL_Struct *url, Boolean skip_get_url);

  virtual void queryChrome(Chrome * chrome);
  virtual void respectChrome(Chrome * chrome);

  static void bringToFrontOrMakeNew(Widget toplevel);
                                          
private:

  XFE_PersonalToolbar *		m_personalToolbar;
  XFE_URLBar *				m_urlBar;
  XFE_BrowserDrop *			m_browserDropSite;

  XP_Bool					m_notification_added;

  static MenuSpec menu_bar_spec[];
  static MenuSpec file_menu_spec[];
  static MenuSpec edit_menu_spec[];
  static MenuSpec view_menu_spec[];
  static MenuSpec go_menu_spec[];

  //static MenuSpec help_menu_spec[];

  // static MenuSpec file_bookmarks_menu_spec[];
  // static MenuSpec navigate_menu_spec[];

  static ToolbarSpec toolbar_spec[];

  XFE_CALLBACK_DECL(navigateToURL) // URL_Struct is sent in callData
  XFE_CALLBACK_DECL(newPageLoading) // URL_Struct is sent in callData

  // update the toolbar appearance
  XFE_CALLBACK_DECL(updateToolbarAppearance)

  // Toolbox methods
  virtual void		toolboxItemSnap			(XFE_ToolboxItem * item);
  virtual void		toolboxItemClose		(XFE_ToolboxItem * item);
  virtual void		toolboxItemOpen		(XFE_ToolboxItem * item);
  virtual void		toolboxItemChangeShowing(XFE_ToolboxItem * item);

  virtual void		configureToolbox	();

};

extern "C" MWContext *fe_showBrowser(Widget toplevel, XFE_Frame *parent_frame, Chrome *chromespec, URL_Struct *url);

extern "C" MWContext *fe_reuseBrowser(MWContext *context,
                                      URL_Struct *url);
#endif /* _xfe_browserframe_h */
