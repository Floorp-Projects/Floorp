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
   AddrBookFrame.cpp -- address book window stuff.
   Created: Chris Toshok <toshok@netscape.com>, 29-Aug-96.
   Revised: Tao Cheng <tao@netscape.com>, 01-nov-96
 */

#include "rosetta.h"
#include "AddrBookFrame.h"
#include "LdapSearchFrame.h"

#include "AB2PaneView.h"
#include "AddrBookView.h"

#include "ComposeView.h"
#include "ComposeFolderView.h"
#include "Dashboard.h"
#include <Xfe/Xfe.h>

#include "xpassert.h"

#include "prefs.h"
#include "prefapi.h"

#include <Xm/Form.h>
#include <Xm/Label.h>
#include <Xm/PushB.h>
#include <Xm/TextF.h>
#include <Xm/DialogS.h>
#include <Xm/FileSB.h>

#include "DtWidgets/ComboBox.h"

extern "C" {
#include "xfe.h"

XP_List* FE_GetDirServers();
};

#if defined(USE_ABCOM)
MenuSpec XFE_AddrBookFrame::new_sub_menu_spec[] = {
  { xfeCmdOpenBrowser,		PUSHBUTTON },
  { xfeCmdComposeMessage,	PUSHBUTTON },
  MENU_PUSHBUTTON(xfeCmdNewBlank),
  MENU_SEPARATOR,
  { xfeCmdAddToAddressBook,	PUSHBUTTON },
  { xfeCmdABNewList,		PUSHBUTTON },
  { xfeCmdABNewPAB,		PUSHBUTTON },
  { xfeCmdABNewLDAPDirectory,		PUSHBUTTON },
  { NULL }
};

MenuSpec XFE_AddrBookFrame::file_menu_spec[] = {
  { "newSubmenu",		CASCADEBUTTON,
	(MenuSpec*)&new_sub_menu_spec },
  MENU_SEPARATOR,
  { xfeCmdImport,		PUSHBUTTON },
  { xfeCmdSaveAs,		PUSHBUTTON },
  MENU_SEPARATOR,
  { xfeCmdABCall,		PUSHBUTTON },
  MENU_SEPARATOR,
  { xfeCmdClose,		PUSHBUTTON },
  { xfeCmdExit,		PUSHBUTTON },
  { NULL }
};
#else
MenuSpec XFE_AddrBookFrame::file_menu_spec[] = {
  { "newSubmenu",		CASCADEBUTTON,
	(MenuSpec*)&XFE_Frame::new_menu_spec },
  { xfeCmdAddToAddressBook,	PUSHBUTTON },
  { xfeCmdABNewList,		PUSHBUTTON },
#if defined(USE_ABCOM)
  { xfeCmdABNewPAB,		PUSHBUTTON },
  { xfeCmdABNewLDAPDirectory,		PUSHBUTTON },
#else
  { xfeCmdImport,		PUSHBUTTON },
#endif /* USE_ABCOM */
  MENU_SEPARATOR,
  { xfeCmdImport,		PUSHBUTTON },
  { xfeCmdSaveAs,		PUSHBUTTON },
  MENU_SEPARATOR,
  { xfeCmdABCall,		PUSHBUTTON },
  MENU_SEPARATOR,
  { xfeCmdClose,		PUSHBUTTON },
  { xfeCmdExit,		PUSHBUTTON },
  { NULL }
};
#endif /* USE_ABCOM */

MenuSpec XFE_AddrBookFrame::edit_menu_spec[] = {
  { xfeCmdUndo,			PUSHBUTTON },
  { xfeCmdRedo,			PUSHBUTTON },
  MENU_SEPARATOR,
#if 0
  /* do not take out yet */
  { xfeCmdCut,			PUSHBUTTON },
  { xfeCmdCopy,	        PUSHBUTTON },
  { xfeCmdPaste,		PUSHBUTTON },
#endif
  { xfeCmdABDeleteEntry,PUSHBUTTON },
#if defined(USE_ABCOM)
  { xfeCmdABDeleteAllEntries,PUSHBUTTON },
#endif /* USE_ABCOM */
  { xfeCmdSelectAll,	PUSHBUTTON },
  MENU_SEPARATOR,
  { xfeCmdDisplayHTMLDomainsDialog,	PUSHBUTTON },
  { xfeCmdViewProperties,	PUSHBUTTON },
  { xfeCmdEditPreferences,	PUSHBUTTON },
  { NULL }
};

MenuSpec XFE_AddrBookFrame::view_menu_spec[] = {
	{ xfeCmdToggleNavigationToolbar,	PUSHBUTTON },
	MENU_SEPARATOR,
	{ xfeCmdABByType,	
	  TOGGLEBUTTON, NULL, "AddrBookViewSortTypeGroup" },
	{ xfeCmdABByName,	
	  TOGGLEBUTTON, NULL, "AddrBookViewSortTypeGroup" },
	{ xfeCmdABByEmailAddress,	
	  TOGGLEBUTTON, NULL, "AddrBookViewSortTypeGroup" },
	{ xfeCmdABByNickName,	
	  TOGGLEBUTTON, NULL, "AddrBookViewSortTypeGroup" },
	{ xfeCmdABByCompany,	
	  TOGGLEBUTTON, NULL, "AddrBookViewSortTypeGroup" },
	{ xfeCmdABByLocality,	
	  TOGGLEBUTTON, NULL, "AddrBookViewSortTypeGroup" },
	MENU_SEPARATOR,
	{ xfeCmdSortAscending,	
	  TOGGLEBUTTON, NULL, "AddrBookViewSortOrderGroup" },
	{ xfeCmdSortDescending,	
	  TOGGLEBUTTON, NULL, "AddrBookViewSortOrderGroup" },
	MENU_SEPARATOR,
	{ xfeCmdABvCard,	PUSHBUTTON },
	{ NULL }
};

MenuSpec XFE_AddrBookFrame::menu_bar_spec[] = {
  { xfeMenuFile, 	CASCADEBUTTON, 
    (MenuSpec*)&XFE_AddrBookFrame::file_menu_spec },
  { xfeMenuEdit, 	CASCADEBUTTON, 
    (MenuSpec*)&XFE_AddrBookFrame::edit_menu_spec },
  { xfeMenuView, 	CASCADEBUTTON, 
    (MenuSpec*)&XFE_AddrBookFrame::view_menu_spec },
  { xfeMenuWindow, 	CASCADEBUTTON, 
    (MenuSpec*)&XFE_Frame::window_menu_spec },
  { xfeMenuHelp, 	CASCADEBUTTON,
    (MenuSpec*)&XFE_Frame::help_menu_spec },
  { NULL }
};

ToolbarSpec XFE_AddrBookFrame::toolbar_spec[] = {
  { xfeCmdAddToAddressBook,	PUSHBUTTON, &MNAB_NewPerson_group},
  { xfeCmdABNewList,	        PUSHBUTTON, &MNAB_NewList_group},
  { xfeCmdABEditEntry,		PUSHBUTTON, &MNAB_Properties_group},

  {  // XX mail only
    xfeCmdComposeMessage,
    CASCADEBUTTON,
    &MNTB_Compose_group, NULL, NULL, NULL, // Icons
    compose_message_submenu_spec, // Submenu spec
    NULL, NULL, // Generate proc/arg
    XFE_TOOLBAR_DELAY_LONG // Popup delay
  },

  { xfeCmdABCall,	        PUSHBUTTON, &MNAB_Call_group},
  { xfeCmdABDeleteEntry,	PUSHBUTTON, &MNTB_Trash_group},
  { xfeCmdStopLoading,		PUSHBUTTON, &TB_Stop_group },
  { NULL }
};

MenuSpec XFE_AddrBookFrame::frame_popup_spec[] = {
  { xfeCmdAddToAddressBook,	PUSHBUTTON },
  { xfeCmdABNewList,		PUSHBUTTON },
  { NULL }
};


extern "C" {
	Widget fe_CreateFileSelectionBox(Widget, char*, Arg*, Cardinal);
	const char *FE_UsersFullName (void);
};

#include "xpgetstr.h"

extern int XFE_AB_FRAME_TITLE;
/* Hack:
 * pixmaps for the address book. 
 */
XFE_Frame* XFE_AddrBookFrame::m_theFrame = 0;
//
XFE_AddrBookFrame::XFE_AddrBookFrame(Widget toplevel, 
									 XFE_Frame *parent_frame,
									 Chrome *chromespec) 
  : XFE_Frame("AddressBook",
			  toplevel, 
			  parent_frame,
			  FRAME_ADDRESSBOOK,
              chromespec,
			  False /* HTML */,
			  True  /* menuBar */, 
			  True /* toolBar */, 
			  True  /* dashBoard */,
			  True /* destroyOnClose */),
	m_popup(0)
{
  m_abView = 0;
  setToolbar(toolbar_spec);
  /* create the addressbook view; 
   * import from XFE_BookmarkFrame::XFE_BookmarkFrame
   */
  XFE_AB2PaneView *view = new XFE_AB2PaneView(this, 
											  getChromeParent(),
											  NULL,
											  m_context,
											  AB_BOOK);
  m_abView = (XFE_AddrBookView *) view->getEntriesListView();

  /* Attachment
   */
  XtVaSetValues(view->getBaseWidget(),
		XmNleftAttachment, XmATTACH_FORM,
		XmNtopAttachment, XmATTACH_FORM,
		XmNrightAttachment, XmATTACH_FORM,
		XmNbottomAttachment, XmATTACH_FORM,
		NULL);

  setView(view);
  setMenubar(menu_bar_spec);

  char title[kMaxFullNameLength+64];
  PR_snprintf(title, sizeof(title),
              XP_GetString(XFE_AB_FRAME_TITLE),
    	      FE_UsersFullName());
  setTitle(title);

  /* show 
   */
  view->show();

  m_dashboard->setShowStatusBar(True);
  m_dashboard->setShowProgressBar(True);
  //
  HG01283

  // Configure the toolbox for the first time
  configureToolbox();
}

XFE_AddrBookFrame::~XFE_AddrBookFrame()
{
	m_theFrame = 0;
}

XP_Bool XFE_AddrBookFrame::handlesCommand(CommandType cmd, 
										  void */* calldata */,
										  XFE_CommandInfo* /* info */)
{
#if defined(USE_ABCOM)
  if (cmd == xfeCmdShowPopup
	  || cmd == xfeCmdStopLoading
	  || cmd == xfeCmdDisplayHTMLDomainsDialog
	  || cmd == xfeCmdEditPreferences
	  )
	  return TRUE;
#else
  if (cmd == xfeCmdShowPopup
	  || cmd == xfeCmdImport
	  || cmd == xfeCmdSaveAs
	  || cmd == xfeCmdStopLoading
	  || cmd == xfeCmdUndo
	  || cmd == xfeCmdRedo
	  || cmd == xfeCmdFindInObject
	  || cmd == xfeCmdFindAgain
	  || cmd == xfeCmdSearchAddress
	  || cmd == xfeCmdEditPreferences
	  || cmd == xfeCmdABByType
	  || cmd == xfeCmdABByName
	  || cmd == xfeCmdABByEmailAddress
	  || cmd == xfeCmdABByCompany
	  || cmd == xfeCmdABByLocality
	  || cmd == xfeCmdABByNickName
	  || cmd == xfeCmdSortAscending
	  || cmd == xfeCmdSortDescending
	  || cmd == xfeCmdAddToAddressBook
	  || cmd == xfeCmdABNewList
	  || cmd == xfeCmdABNewPAB
	  || cmd == xfeCmdABNewLDAPDirectory
	  || cmd == xfeCmdViewProperties
	  || cmd == xfeCmdDisplayHTMLDomainsDialog
	  || cmd == xfeCmdABEditEntry
	  || cmd == xfeCmdABDeleteEntry
	  || cmd == xfeCmdABDeleteAllEntries
	  || cmd == xfeCmdABCall
	  || cmd == xfeCmdABvCard)
	  return TRUE;
#endif /* USE_ABCOM */
  else
	  /* Default
	   */
	  return XFE_Frame::handlesCommand(cmd);
}

char *XFE_AddrBookFrame::commandToString(CommandType cmd,
						   void *calldata, XFE_CommandInfo* info)
{
	XFE_Command* handler = getCommand(cmd);

	if (handler != NULL)
		return handler->getLabel(this, info);

	if (cmd == xfeCmdToggleNavigationToolbar) {
		char *res = NULL;
		
		if (m_toolbar->isShown())
			res = "hideNavToolbarCmdString";
		else
			res = "showNavToolbarCmdString";
		return stringFromResource(res);
	}
	else
		return XFE_Frame::commandToString(cmd, calldata, info);
}

XP_Bool
XFE_AddrBookFrame::isCommandEnabled(CommandType cmd, void *, XFE_CommandInfo*)
{
  /* first we handle the commands we know about.. 
   * xfeCmdClose xfeCmdEditPreferences 
   */
  if (cmd == xfeCmdOpenInbox
      || cmd == xfeCmdOpenNewsgroups
      || cmd == xfeCmdOpenFolders

      || cmd == xfeCmdEditPreferences

      || cmd == xfeCmdOpenBookmarks

#ifdef JAVA
      || cmd == xfeCmdJavaConsole 
#endif /* JAVA */

	  || cmd == xfeCmdSelectAll

#if !defined(USE_ABCOM)
	  || cmd == xfeCmdSearchAddress
	  || cmd == xfeCmdABvCard
#else
	  || cmd == xfeCmdABvCard
#endif /* USE_ABCOM */

	  || cmd == xfeCmdDisplayHTMLDomainsDialog) {
	  return TRUE;
  }/* if */
  else if (cmd == xfeCmdOpenAddressBook) {
	  return FALSE;
  }/* else if */
  else if (cmd == xfeCmdStopLoading) {
	  return m_abView?m_abView->isSearching():False;
  }/* else if */
  else {
    return XFE_Frame::isCommandEnabled(cmd);
  }/* else */
}

char *XFE_AddrBookFrame::getDocString(CommandType cmd)
{
	if ((cmd == xfeCmdComposeMessage ||
	    (cmd == xfeCmdComposeMessageHTML) ||
	    (cmd == xfeCmdComposeMessagePlain) ||
		cmd == xfeCmdABCall) && m_abView)
		return m_abView->getDocString(cmd);
	return XFE_Frame::getDocString(cmd);
}

char *XFE_AddrBookFrame::getTipString(CommandType /* cmd */)
{
	return NULL;
}

void XFE_AddrBookFrame::doCommand(CommandType cmd, 
								  void *calldata, XFE_CommandInfo* info)
{
#if defined(USE_ABCOM)
  /* first we handle the commands we know about. 
   */
  if (cmd == xfeCmdComposeMessage) 
	  composeMessage();
  else if (cmd == xfeCmdStopLoading) 
	  m_abView->stopSearch();
  else if (cmd == xfeCmdComposeMessageHTML) 
  {
     CONTEXT_DATA(m_context)->stealth_cmd = 
			(fe_globalPrefs.send_html_msg == False);
     composeMessage();
  }
  else if ( cmd == xfeCmdComposeMessagePlain) 
  {
     CONTEXT_DATA(m_context)->stealth_cmd = 
			(fe_globalPrefs.send_html_msg == True);
     composeMessage();
  }
  else	if (cmd == xfeCmdShowPopup) {
		// Finish up the popup
		XEvent *event = info->event;

		if (m_popup)
			delete m_popup;
		
		m_popup = 
			new XFE_PopupMenu("popup",(XFE_Frame *) this,
							  XfeAncestorFindApplicationShell(getBaseWidget()));
		m_popup->addMenuSpec(frame_popup_spec);
		m_popup->position (event);
		m_popup->show();
  }/* else if */
  else if (cmd == xfeCmdEditPreferences)
    fe_showMailNewsPreferences(this, (MWContext*)m_view->getContext());

  else if (cmd == xfeCmdDisplayHTMLDomainsDialog)
    
	  MSG_DisplayHTMLDomainsDialog((MWContext*)m_view->getContext());
  /* ToolBar
   */
  else
    /* Default
     */
    XFE_Frame::doCommand(cmd, calldata, info);
#else
  /* first we handle the commands we know about. 
   */
  if (cmd == xfeCmdComposeMessage) 
	  composeMessage();
  else if (cmd == xfeCmdStopLoading) 
	  m_abView->stopSearch();
  else if (cmd == xfeCmdComposeMessageHTML) 
  {
     CONTEXT_DATA(m_context)->stealth_cmd = 
			(fe_globalPrefs.send_html_msg == False);
     composeMessage();
  }
  else if ( cmd == xfeCmdComposeMessagePlain) 
  {
     CONTEXT_DATA(m_context)->stealth_cmd = 
			(fe_globalPrefs.send_html_msg == True);
     composeMessage();
  }
  else if (cmd == xfeCmdImport) {
	  import();
  }
  else if (cmd == xfeCmdSaveAs)
    saveAs();
  /* edit_menu_spec
   */
  else if (cmd == xfeCmdUndo)
    undo();
  else if (cmd == xfeCmdRedo)
    redo();
  else if (cmd == xfeCmdABDeleteEntry)
    abDelete();
  else if (cmd == xfeCmdABDeleteAllEntries)
    abDeleteAllEntries();
  else if (cmd == xfeCmdFindInObject)
    printf("\n %s", xfeCmdComposeMessage);
  else if (cmd == xfeCmdFindAgain)
    printf("\n %s", xfeCmdComposeMessage);
  else if (cmd == xfeCmdSearchAddress) {
	  fe_showLdapSearch(XfeAncestorFindApplicationShell(getBaseWidget()), this, (Chrome*)NULL);
  }/* else if */
  else if (cmd == xfeCmdEditPreferences)
    fe_showMailNewsPreferences(this, (MWContext*)m_view->getContext());

  /* view_menu_spec
   */
  else if (cmd == xfeCmdABByType) {
    m_abView->setSortType(AB_SortByTypeCmd);
  }
  else if (cmd == xfeCmdABByName) {
    m_abView->setSortType(AB_SortByFullNameCmd);
  }
  else if (cmd == xfeCmdABByEmailAddress) {
    m_abView->setSortType(AB_SortByEmailAddress);
  }
  else if (cmd == xfeCmdABByCompany) {
    m_abView->setSortType(AB_SortByCompanyName);
  }
  else if (cmd == xfeCmdABByLocality) {
    m_abView->setSortType(AB_SortByLocality);
  }
  else if (cmd == xfeCmdABByNickName) {
    m_abView->setSortType(AB_SortByNickname);
  }
  else if (cmd == xfeCmdSortAscending) {
    m_abView->setAscending(True);
  }
  else if (cmd == xfeCmdSortDescending) {
    m_abView->setAscending(False);
  }
  /* item_menu_spec
   */
  else if (cmd == xfeCmdAddToAddressBook)
    /* Add new user
     */
    addToAddressBook();
  else if (cmd == xfeCmdABNewList)
    /* Add new list
     */
    newList();
  else if (cmd == xfeCmdViewProperties)
    /* Edit property
     */
    viewProperties();

  else if (cmd == xfeCmdDisplayHTMLDomainsDialog)
    
	  MSG_DisplayHTMLDomainsDialog((MWContext*)m_view->getContext());
  /* ToolBar
   */
  else if (cmd == xfeCmdABEditEntry)
    /* Edit property
     */
    viewProperties();
  else if (cmd == xfeCmdABDeleteEntry)
    abDelete();
  else if (cmd == xfeCmdABCall)
	  abCall();
  else if (cmd == xfeCmdABvCard)
	  abVCard();
  else	if (cmd == xfeCmdShowPopup) {
		// Finish up the popup
		XEvent *event = info->event;

		if (m_popup)
			delete m_popup;
		
		m_popup = new XFE_PopupMenu("popup",(XFE_Frame *) this,
									XfeAncestorFindApplicationShell(getBaseWidget()));
		m_popup->addMenuSpec(frame_popup_spec);
		m_popup->position (event);
		m_popup->show();
  }/* else if */
  else
    /* Default
     */
    XFE_Frame::doCommand(cmd, calldata, info);
#endif /* USE_ABCOM */
}/* XFE_AddrBookFrame::doCommand() */


void XFE_AddrBookFrame::openBrowser() {

}

void XFE_AddrBookFrame::composeMessage() {
  MSG_Pane *pane = NULL;
  pane = MSG_Mail((MWContext*)m_abView->getContext());

  ABAddrMsgCBProcStruc *pairs = ((XFE_AddrBookView*)m_abView)->getSelections();

  if (pairs && pairs->m_count) {
	  XFE_ComposeView *composeView = (XFE_ComposeView *)MSG_GetFEData(pane);

	  if (composeView) {
		  XFE_ComposeFolderView *composeFolderView = 
			  (XFE_ComposeFolderView *)composeView->getComposeFolderView();
		  
		  if (composeFolderView)
			  composeFolderView->getAddrFolderView()->addrMsgCB(pairs);
	  }/* if */
  }/* if */
}

void XFE_AddrBookFrame::abAddToMessage() 
{
}

void XFE_AddrBookFrame::import() 
{

  AB_ImportFromFile(((XFE_AddrBookView *)m_abView)->getABPane(),
					(MWContext *)m_abView->getContext());

}/* XFE_AddrBookFrame::import() */

void XFE_AddrBookFrame::saveAs()
{

	AB_ExportToFile(((XFE_AddrBookView *)m_view)->getABPane(),
					(MWContext *)m_abView->getContext());

}

XP_Bool XFE_AddrBookFrame::isOkToClose()
{
  XFE_ABListSearchView *searchView = (XFE_ABListSearchView *) m_abView;
  searchView->unRegisterInterested();
  return TRUE;
}/* XFE_AddrBookFrame::isOkToClose() */

void XFE_AddrBookFrame::close() {
  delete_response();
}

void XFE_AddrBookFrame::undo() {
  m_abView->undo();
}

void XFE_AddrBookFrame::redo() {
  m_abView->redo();
}

void XFE_AddrBookFrame::abDelete() {
  m_abView->abDelete();
}

void XFE_AddrBookFrame::abDeleteAllEntries() {
  m_abView->abDeleteAllEntries();
}

void XFE_AddrBookFrame::addToAddressBook() {
  /* shall this be new user ??
   */
  m_abView->newUser();
}

void XFE_AddrBookFrame::abVCard() {
  /* shall this be new user ??
   */
  m_abView->abVCard();
}

void XFE_AddrBookFrame::newList() {
  /* popup new lsit
   */
  m_abView->newList();
}

void XFE_AddrBookFrame::viewProperties() { 
  /* edit property
   */
  m_abView->editProperty();
}

void XFE_AddrBookFrame::abCall() { 
  m_abView->abCall();
}

/* C API
 */
extern "C"  MWContext*
fe_showAddrBook(Widget toplevel, XFE_Frame *parent_frame, Chrome *chromespec)
{

	if (!XFE_AddrBookFrame::m_theFrame)
		XFE_AddrBookFrame::m_theFrame = 
			new XFE_AddrBookFrame(toplevel, parent_frame, chromespec);

	XFE_AddrBookFrame::m_theFrame->show();
	
	return XFE_AddrBookFrame::m_theFrame->getContext();
}

//////////////////////////////////////////////////////////////////////////
//
// Toolbox methods
//
//////////////////////////////////////////////////////////////////////////
void
XFE_AddrBookFrame::toolboxItemClose(XFE_ToolboxItem * item)
{
	XP_ASSERT( item != NULL );

	// Address_Book_Toolbar
	if (item == m_toolbar)
	{
		fe_globalPrefs.address_book_address_book_toolbar_open = False;
	}
}
//////////////////////////////////////////////////////////////////////////
void
XFE_AddrBookFrame::toolboxItemOpen(XFE_ToolboxItem * item)
{
	XP_ASSERT( item != NULL );

	// Address_Book_Toolbar
	if (item == m_toolbar)
	{
		fe_globalPrefs.address_book_address_book_toolbar_open = True;
	}
}
//////////////////////////////////////////////////////////////////////////
void
XFE_AddrBookFrame::toolboxItemChangeShowing(XFE_ToolboxItem * item)
{
	XP_ASSERT( item != NULL );

	// Address_Book_Toolbar
	if (item == m_toolbar)
	{
		fe_globalPrefs.address_book_address_book_toolbar_showing = item->isShown();
	}
}
//////////////////////////////////////////////////////////////////////////
void
XFE_AddrBookFrame::configureToolbox()
{
	// If a the frame was constructed with a chromespec, then we ignore
	// all the preference magic.
	if (m_chromespec_provided)
	{
		return;
	}

	// Make sure the toolbox is alive
	if (!m_toolbox || (m_toolbox && !m_toolbox->isAlive()))
	{
		return;
	}

	// Address_Book_Toolbar
	if (m_toolbar)
	{
		m_toolbar->setShowing(fe_globalPrefs.address_book_address_book_toolbar_showing);
		m_toolbar->setOpen(fe_globalPrefs.address_book_address_book_toolbar_open);
	}
}
//////////////////////////////////////////////////////////////////////////
