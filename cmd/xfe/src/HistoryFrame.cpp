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
   HistoryFrame.cpp -- History window stuff
   Created: Stephen Lamm <slamm@netscape.com>, 24-Feb-96.
 */



#include "HistoryFrame.h"
#include "HistoryView.h"
#include "xpassert.h"
#include "xfe2_extern.h"
#include "abdefn.h"				// for kMaxFullNameLength
#include "xpgetstr.h"			// for XP_GetString()
#include "Dashboard.h"

extern int XFE_HISTORY_FRAME_TITLE;

static XFE_HistoryFrame *theFrame = NULL;

// Basically, anything with "" around it hasn't been wired-up/implemented.
MenuSpec XFE_HistoryFrame::file_menu_spec[] = {
  { "newSubmenu",           CASCADEBUTTON,
	(MenuSpec*)&XFE_Frame::new_menu_spec },
  MENU_SEPARATOR,
  { xfeCmdSaveAs,           PUSHBUTTON },
  MENU_SEPARATOR,
  { xfeCmdOpenSelected,     PUSHBUTTON },
  { xfeCmdAddBookmark,      PUSHBUTTON },
  { xfeCmdAddToToolbar,     PUSHBUTTON },
  //{ xfeCmdCreateShortcut,   PUSHBUTTON },  // Create desktop shortcut
  //MENU_SEPARATOR,
  //{ xfeCmdPrintSetup,		PUSHBUTTON },
  //{ xfeCmdPrint,            PUSHBUTTON },
  MENU_SEPARATOR,
  { xfeCmdClose,            PUSHBUTTON },  
  { xfeCmdExit,             PUSHBUTTON },
  { NULL }
};

MenuSpec XFE_HistoryFrame::edit_menu_spec[] = {
  { xfeCmdUndo,		PUSHBUTTON },
  { xfeCmdRedo,		PUSHBUTTON },
  MENU_SEPARATOR,
  { xfeCmdCut,		PUSHBUTTON },
  { xfeCmdCopy,		PUSHBUTTON },
  { xfeCmdDelete,	PUSHBUTTON },
  { xfeCmdSelectAll,	PUSHBUTTON },
  MENU_SEPARATOR,
  //{ xfeCmdSearch,	PUSHBUTTON },  // We should have this, but it isn't ready
#ifdef MOZ_MAIL_NEWS
  { xfeCmdSearchAddress,	PUSHBUTTON },
  MENU_SEPARATOR,
#endif
  { xfeCmdEditPreferences,	PUSHBUTTON },
  { NULL }
};

MenuSpec XFE_HistoryFrame::view_menu_spec[] = {
  { xfeCmdSortByTitle, TOGGLEBUTTON, NULL, "sortByRadioGroup",
    False, (void*)eHSC_TITLE },
  { xfeCmdSortByLocation, TOGGLEBUTTON, NULL, "sortByRadioGroup", 
    False, (void*)eHSC_LOCATION },
  { xfeCmdSortByDateFirstVisited, TOGGLEBUTTON, NULL, "sortByRadioGroup",
    False, (void*)eHSC_FIRSTVISIT },
  { xfeCmdSortByDateLastVisited, TOGGLEBUTTON, NULL, "sortByRadioGroup",
    False, (void*)eHSC_LASTVISIT },
  { xfeCmdSortByExpirationDate, TOGGLEBUTTON, NULL, "sortByRadioGroup", 
    False, (void*)eHSC_EXPIRES },
  { xfeCmdSortByVisitCount, TOGGLEBUTTON, NULL, "sortByRadioGroup", 
    False, (void*)eHSC_VISITCOUNT },
  MENU_SEPARATOR,
  { xfeCmdSortAscending, TOGGLEBUTTON, NULL, "ascendDescendRadioGroup", 
    False },
  { xfeCmdSortDescending, TOGGLEBUTTON, NULL, "ascendDescendRadioGroup", 
    False },
  //MENU_SEPARATOR,
  //{ xfeCmdMoreOptions,  PUSHBUTTON },
  //{ xfeCmdFewerOptions, PUSHBUTTON },
  { NULL }
};

MenuSpec XFE_HistoryFrame::menu_bar_spec[] = {
  { xfeMenuFile,	CASCADEBUTTON, file_menu_spec },
  { xfeMenuEdit,	CASCADEBUTTON, edit_menu_spec },
  { xfeMenuView,	CASCADEBUTTON, view_menu_spec },
  { xfeMenuWindow,	CASCADEBUTTON, XFE_Frame::window_menu_spec },
  { xfeMenuHelp, 	CASCADEBUTTON, XFE_Frame::help_menu_spec },
  { NULL }
};
//////////////////////////////////////////////////////////////////////////
XFE_HistoryFrame::XFE_HistoryFrame(Widget toplevel, XFE_Frame *parent_frame, Chrome *chromespec) 
  : XFE_Frame("history", toplevel, parent_frame,
	      FRAME_HISTORY, chromespec, False, True, False, True, False)
{
  // create the History view
  XFE_HistoryView *view = new XFE_HistoryView(this, getViewParent(), NULL, m_context);
  setView(view);

  XtVaSetValues(view->getBaseWidget(),
  		XmNleftAttachment, XmATTACH_FORM,
		XmNtopAttachment, XmATTACH_FORM,
		XmNrightAttachment, XmATTACH_FORM,
		XmNbottomAttachment, XmATTACH_FORM,
		NULL);

  setMenubar(menu_bar_spec);

  //
  // Make the bookmark frame title more reasonable
  //
  char title[kMaxFullNameLength+64];

  PR_snprintf(title, 
			  sizeof(title),
              XP_GetString(XFE_HISTORY_FRAME_TITLE),
    	      FE_UsersFullName());

  setTitle(title);

  view->show();

  m_dashboard->setShowStatusBar(True);
  m_dashboard->setShowProgressBar(True);
}
//////////////////////////////////////////////////////////////////////////
XFE_HistoryFrame::~XFE_HistoryFrame()
{
}
//////////////////////////////////////////////////////////////////////////
void
XFE_HistoryFrame::showHistory(Widget toplevel, XFE_Frame *parent_frame, Chrome *chromespec)
{
	fe_showHistory(toplevel, parent_frame, chromespec);
}
//////////////////////////////////////////////////////////////////////////
XP_Bool
XFE_HistoryFrame::isCommandEnabled(CommandType cmd,
								   void *calldata, XFE_CommandInfo*)
{
  if (cmd == xfeCmdClose)
    return True;
  else
    return XFE_Frame::isCommandEnabled(cmd, calldata);
}

extern "C" MWContext*
fe_showHistory(Widget toplevel, XFE_Frame *parent_frame, Chrome *chromespec)
{
  /* Check to see if we have access to the global history database. */
  if (fe_globalData.all_databases_locked)
	return NULL;

  fe_createHistory(toplevel, parent_frame, chromespec);

  theFrame->show();

  return theFrame->getContext();
}

extern "C" void
fe_createHistory(Widget toplevel,
				 XFE_Frame *parent_frame,
				 Chrome *chromespec)
{
  if (theFrame == NULL)
    theFrame = new XFE_HistoryFrame(toplevel, parent_frame, chromespec);
}
