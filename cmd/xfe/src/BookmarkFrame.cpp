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
   BookmarkFrame.cpp -- Bookmark window stuff
   Created: Chris Toshok <toshok@netscape.com>, 7-Aug-96.
 */



#include "BookmarkFrame.h"
#include "BookmarkView.h"
#include "xpassert.h"
#include "abdefn.h"				// for kMaxFullNameLength
#include "xpgetstr.h"			// for XP_GetString()
#include "Dashboard.h"

#if DEBUG_dora
#define D(x) x
#else
#define D(x)
#endif

extern int XFE_BM_FRAME_TITLE;

// The singleton bookmark frame
/* static */ XFE_BookmarkFrame * XFE_BookmarkFrame::m_bookmarkFrame = NULL;

MenuSpec XFE_BookmarkFrame::file_menu_spec[] = {
  { "newSubmenu",           CASCADEBUTTON,
	(MenuSpec*)&XFE_Frame::new_menu_spec },
	{ xfeCmdNewBookmark,  PUSHBUTTON },
	{ xfeCmdNewFolder,    PUSHBUTTON },
	{ xfeCmdNewSeparator, PUSHBUTTON },
	{ xfeCmdOpenBookmarkFile,        PUSHBUTTON },
	{ xfeCmdImport,       PUSHBUTTON },
	MENU_SEPARATOR,
	{ xfeCmdSaveAs,	    PUSHBUTTON },
	MENU_SEPARATOR,
	{ xfeCmdOpenSelected, PUSHBUTTON },
	{ xfeCmdAddToToolbar,  PUSHBUTTON },
    //{ xfeCmdCreateShortcut, PUSHBUTTON },  // desktop shortcut
	{ xfeCmdMakeAlias,  PUSHBUTTON },
	//MENU_SEPARATOR,
    //{ xfeCmdGoOffline, PUSHBUTTON },
	//MENU_SEPARATOR,
	MENU_SEPARATOR,
	{ xfeCmdClose,	    PUSHBUTTON },  
    { xfeCmdExit,		PUSHBUTTON },
	{ NULL }
};

MenuSpec XFE_BookmarkFrame::edit_menu_spec[] = {
  { xfeCmdUndo,		PUSHBUTTON },
  { xfeCmdRedo,		PUSHBUTTON },
  MENU_SEPARATOR,
  { xfeCmdCut,		PUSHBUTTON },
  { xfeCmdCopy,		PUSHBUTTON },
  { xfeCmdPaste,	PUSHBUTTON },
  { xfeCmdDelete,	PUSHBUTTON },
  { xfeCmdSelectAll,	PUSHBUTTON },
  MENU_SEPARATOR,
  { xfeCmdFindInObject,	PUSHBUTTON },
  { xfeCmdFindAgain,	PUSHBUTTON },
#ifdef MOZ_MAIL_NEWS
  { xfeCmdSearchAddress,	    PUSHBUTTON },
#endif
  MENU_SEPARATOR,
  { xfeCmdBookmarkProperties,    PUSHBUTTON },
  { NULL }
};

MenuSpec XFE_BookmarkFrame::view_menu_spec[] = {
  { xfeCmdSortByTitle,           TOGGLEBUTTON, NULL, "sortByRadioGroup", False },
  { xfeCmdSortByLocation,        TOGGLEBUTTON, NULL, "sortByRadioGroup", False },
  { xfeCmdSortByDateLastVisited, TOGGLEBUTTON, NULL, "sortByRadioGroup", False },
  { xfeCmdSortByDateCreated,     TOGGLEBUTTON, NULL, "sortByRadioGroup", False },
  MENU_SEPARATOR,
  { xfeCmdSortAscending,  TOGGLEBUTTON, NULL, "ascendDescendRadioGroup", False },
  { xfeCmdSortDescending, TOGGLEBUTTON, NULL, "ascendDescendRadioGroup", False },
  MENU_SEPARATOR,
  { xfeCmdMoveBookmarkUp,   PUSHBUTTON },
  { xfeCmdMoveBookmarkDown, PUSHBUTTON },
  MENU_SEPARATOR,
  { xfeCmdBookmarksWhatsNew, PUSHBUTTON },
  MENU_SEPARATOR,
  { xfeCmdSetToolbarFolder, PUSHBUTTON },
  { xfeCmdSetNewBookmarkFolder,	PUSHBUTTON },
  { xfeCmdSetBookmarkMenuFolder, PUSHBUTTON },
  { NULL }
};

MenuSpec XFE_BookmarkFrame::menu_bar_spec[] = {
  { xfeMenuFile,	CASCADEBUTTON, file_menu_spec },
  { xfeMenuEdit,	CASCADEBUTTON, edit_menu_spec },
  { xfeMenuView,	CASCADEBUTTON, view_menu_spec },
  { xfeMenuWindow,	CASCADEBUTTON, XFE_Frame::window_menu_spec },
  { xfeMenuHelp, 	CASCADEBUTTON, XFE_Frame::help_menu_spec },
  { NULL }
};

MWContext *XFE_BookmarkFrame::main_bm_context = NULL;

XFE_BookmarkFrame::XFE_BookmarkFrame(Widget toplevel,
									 XFE_Frame *parent_frame,
									 Chrome *chromespec) 
  : XFE_Frame("bookmarks", toplevel, 
			  parent_frame,
			  FRAME_BOOKMARK, 
			  chromespec,
			  False,
			  True,
			  False,
			  True,
			  False) // m_destroyOnClose, always keep one Bookmark instance
{
  D(printf("XFE_BookmarkFrame SUB-SYSTEM INSTANCIATING\n");)

  XP_ASSERT( m_bookmarkFrame == NULL );

  // create the bookmark view
  XFE_BookmarkView *view = new XFE_BookmarkView(this, getViewParent(), 
												NULL, m_context);
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
              XP_GetString(XFE_BM_FRAME_TITLE),
    	      FE_UsersFullName());

  setTitle(title);

  view->show();

  m_dashboard->setShowStatusBar(True);
  m_dashboard->setShowProgressBar(True);

  realize();

  main_bm_context = m_context;

  m_bookmarkFrame = this;
}

XFE_BookmarkFrame::~XFE_BookmarkFrame()
{
	D(printf("XFE_BookmarkFrame SUB-SYSTEM DESTRUCTING\n");)

	BM_SaveBookmarks(main_bm_context, NULL);
	m_bookmarkFrame = NULL;
	main_bm_context = NULL;
}

extern "C" MWContext*
fe_showBookmarks(Widget toplevel, XFE_Frame *parent_frame, Chrome *chromespec)
{
  fe_createBookmarks(toplevel, parent_frame, chromespec);

  XFE_BookmarkFrame * bm_frame = XFE_BookmarkFrame::getBookmarkFrame();

  XP_ASSERT( bm_frame != NULL );

  bm_frame->show();

  return bm_frame->getContext();
}

extern "C" MWContext* fe_getBookmarkContext()
{
	return XFE_BookmarkFrame::main_bm_context;
}

extern "C" void
fe_createBookmarks(Widget toplevel,
				   XFE_Frame *parent_frame,
				   Chrome *chromespec)
{
	XFE_BookmarkFrame::createBookmarkFrame(toplevel,parent_frame,chromespec);
}
//////////////////////////////////////////////////////////////////////////
XP_Bool
XFE_BookmarkFrame::isCommandEnabled(CommandType cmd,
								   void *calldata, XFE_CommandInfo*)
{
  if (cmd == xfeCmdClose)
    return True;
  else
    return XFE_Frame::isCommandEnabled(cmd, calldata);
}

//////////////////////////////////////////////////////////////////////////

/* static */ void 
XFE_BookmarkFrame::createBookmarkFrame(Widget		toplevel, 
									   XFE_Frame *	parent_frame, 
									   Chrome *		chromespec)
{
	if (m_bookmarkFrame == NULL)
	{
		m_bookmarkFrame = new XFE_BookmarkFrame(toplevel, 
												parent_frame, 
												chromespec);
	}
}
//////////////////////////////////////////////////////////////////////////
/* static */ XFE_BookmarkFrame * 
XFE_BookmarkFrame::getBookmarkFrame()
{
	XP_ASSERT( m_bookmarkFrame != NULL );

	return m_bookmarkFrame;
}
//////////////////////////////////////////////////////////////////////////

