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
   FolderFrame.cpp -- Folder window stuff
   Created: Chris Toshok <toshok@netscape.com>, 29-Aug-96.
 */



#include "MozillaApp.h"
#include "FolderFrame.h"
#include "FolderView.h"
#include "FolderMenu.h"
#include "Dashboard.h"
#include "Command.h"
#ifdef CLEAN_UP_NEWS
#include "MailDownloadFrame.h"
#include "Xfe/Xfe.h"
#endif
#include "xpassert.h"

#include "libi18n.h"
#include "intl_csi.h"

#ifdef DEBUG_toshok
#define D(x) x
#else
#define D(x)
#endif

#include "xpgetstr.h"
extern int XFE_SILLY_NAME_FOR_SEEMINGLY_UNAMEABLE_THING;
extern int XFE_MN_FOLDER_TITLE;
extern int XFE_INBOX_DOESNT_EXIST;

/* there is only one folder frame. */
static XFE_FolderFrame *theFrame = NULL;

MenuSpec XFE_FolderFrame::file_menu_spec[] = {
  { "newSubmenu",		CASCADEBUTTON, (MenuSpec *) &XFE_Frame::new_submenu_spec },
  { xfeCmdNewFolder,		PUSHBUTTON },
  { xfeCmdNewNewsHost,		PUSHBUTTON },
  { xfeCmdOpenSelected,		PUSHBUTTON },
  MENU_SEPARATOR,
  { xfeCmdRenameFolder,		PUSHBUTTON },
  { xfeCmdEmptyTrash,		PUSHBUTTON },
  { xfeCmdCompressFolders,	PUSHBUTTON },
  { xfeCmdCleanUpDisk,      PUSHBUTTON },
  MENU_SEPARATOR,
  { "newMsgSubmenu",	CASCADEBUTTON, (MenuSpec *) &XFE_Frame::newMsg_submenu_spec },
  { xfeCmdSendMessagesInOutbox,	PUSHBUTTON },
#if 0
  { xfeCmdUpdateMessageCount,	PUSHBUTTON },
#endif
  { xfeCmdAddNewsgroup,		PUSHBUTTON },
  //MENU_SEPARATOR,
  //{ xfeCmdGoOffline,		PUSHBUTTON },
  MENU_SEPARATOR,
  { xfeCmdClose,		PUSHBUTTON },
  { xfeCmdExit,			PUSHBUTTON },
  { NULL }
};

MenuSpec XFE_FolderFrame::edit_menu_spec[] = {
	{ xfeCmdUndo,			PUSHBUTTON },
	{ xfeCmdRedo,			PUSHBUTTON },
	MENU_SEPARATOR,
	{ xfeCmdCut,			PUSHBUTTON },
	{ xfeCmdCopy,			PUSHBUTTON },
	{ xfeCmdPaste,		PUSHBUTTON },
	{ xfeCmdDeleteFolder,		PUSHBUTTON },
	{ xfeCmdSelectAll,		PUSHBUTTON },
	MENU_SEPARATOR,
	{ xfeCmdSearch,		        PUSHBUTTON },
	{ xfeCmdSearchAddress,	    PUSHBUTTON },
	MENU_SEPARATOR,
	{ xfeCmdEditConfiguration,	PUSHBUTTON },
	{ xfeCmdModerateDiscussion,	PUSHBUTTON },
	MENU_SEPARATOR,
	{ xfeCmdEditMailFilterRules,PUSHBUTTON },
	{ xfeCmdViewProperties,	    PUSHBUTTON },
	{ xfeCmdEditPreferences,	PUSHBUTTON },
	{ NULL }
};

MenuSpec XFE_FolderFrame::view_menu_spec[] = {
  { xfeCmdToggleNavigationToolbar,	PUSHBUTTON },
  { xfeCmdToggleLocationToolbar,	PUSHBUTTON },
  MENU_SEPARATOR,
  { "expandCollapseSubmenu",    CASCADEBUTTON,
    (MenuSpec *) &XFE_Frame::expand_collapse_submenu_spec },
// Move Folders out of the menu for now -- see bug 68204.
//  { "moveSubmenu",		DYNA_CASCADEBUTTON, NULL, NULL, False,
//    (void*)xfeCmdMoveFoldersInto, XFE_FolderMenu::generate },
  MENU_SEPARATOR,
  { xfeCmdStopLoading,			PUSHBUTTON },
  { NULL }
};

MenuSpec XFE_FolderFrame::menu_bar_spec[] = {
  { xfeMenuFile, 	CASCADEBUTTON, file_menu_spec },
  { xfeMenuEdit, 	CASCADEBUTTON, edit_menu_spec },
  { xfeMenuView, 	CASCADEBUTTON, view_menu_spec },
  { xfeMenuWindow,	CASCADEBUTTON, XFE_Frame::window_menu_spec },
  { xfeMenuHelp, 	CASCADEBUTTON, XFE_Frame::help_menu_spec },
  { NULL }
};

ToolbarSpec XFE_FolderFrame::toolbar_spec[] = {
  { xfeCmdGetNewMessages,	PUSHBUTTON, &MNTB_GetMsg_group },
  {  // XX mail only
    xfeCmdComposeMessage,
    CASCADEBUTTON,
    &MNTB_Compose_group, NULL, NULL, NULL, 	// Icons
    compose_message_submenu_spec,   		// Submenu spec
    NULL, NULL,  				// Generate proc/arg
    XFE_TOOLBAR_DELAY_LONG     			// Popup delay
  },
  { xfeCmdNewFolder,		PUSHBUTTON, &MNTB_NewFolder_group },
  { xfeCmdAddNewsgroup,		PUSHBUTTON, &MNTB_AddGroup_group },
#if 0
  { xfeCmdDeleteFolder,         PUSHBUTTON, &MNTB_Trash_group },
#endif
  { xfeCmdStopLoading,		PUSHBUTTON, &TB_Stop_group },
  { NULL }
};

XFE_FolderFrame::XFE_FolderFrame(Widget toplevel,
								 XFE_Frame *parent_frame,
								 Chrome *chromespec) 
  : XFE_Frame("MailFolder", toplevel, parent_frame,
			  FRAME_MAILNEWS_FOLDER, chromespec)
{
  INTL_CharSetInfo c = LO_GetDocumentCharacterSetInfo(m_context);
  INTL_SetCSIWinCSID(c, 
		INTL_DocToWinCharSetID (CONTEXT_DATA (m_context)->xfe_doc_csid));

  geometryPrefName = "mail.folder";

D(	printf ("in XFE_FolderFrame::XFE_FolderFrame()\n");)

  // create the folder view
  XFE_FolderView *view = new XFE_FolderView(this, getViewParent(), NULL, m_context);
  char banner_title[1024];

  m_banner = new XFE_MNBanner(this, m_toolbox);

  setView(view);

  PR_snprintf(banner_title, sizeof(banner_title),
              XP_GetString( XFE_SILLY_NAME_FOR_SEEMINGLY_UNAMEABLE_THING ),
    	      FE_UsersFullName());

  m_banner->setTitle( banner_title );
  m_banner->setSubtitle("");

  // window title
  char title[1024];
  PR_snprintf(title, sizeof(title),
              XP_GetString(XFE_MN_FOLDER_TITLE),
    	      FE_UsersFullName());

  setTitle(title );

  XtVaSetValues(view->getBaseWidget(),
		XmNleftAttachment, XmATTACH_FORM,
		XmNtopAttachment, XmATTACH_FORM,
		XmNrightAttachment, XmATTACH_FORM,
		XmNbottomAttachment, XmATTACH_FORM,
		NULL);

  setMenubar(menu_bar_spec);
  setToolbar(toolbar_spec);

//  m_banner->show();
  view->show();


  XFE_MozillaApp::theApp()->registerInterest(XFE_MNView::MNChromeNeedsUpdating,
											 this,
											 (XFE_FunctionNotification)MNChromeUpdate_cb);

  {
	  Pixel bg_pixel;

	  XtVaGetValues(m_banner->getBaseWidget(),
					XmNbackground, &bg_pixel,
					NULL);

	  if (!XFE_MNView::collectionsIcon.pixmap)
		  fe_NewMakeIcon(m_widget,
						 getFGPixel(),
						 bg_pixel,
						 &XFE_MNView::collectionsIcon,
						 NULL, 
						 MN_MessageCenter.width, MN_MessageCenter.height,
						 MN_MessageCenter.mono_bits, MN_MessageCenter.color_bits,
						 MN_MessageCenter.mask_bits, FALSE);
  }

  m_banner->setProxyIcon(&XFE_MNView::collectionsIcon);

  m_dashboard->setShowStatusBar(True);
  m_dashboard->setShowProgressBar(True);

  // Configure the toolbox for the first time
  configureToolbox();

D(	printf ("leaving XFE_FolderFrame::XFE_FolderFrame()\n");)
}

XFE_FolderFrame::~XFE_FolderFrame()
{
D(	printf ("in XFE_FolderFrame::~XFE_FolderFrame()\n");)

	XFE_MozillaApp::theApp()->unregisterInterest(XFE_MNView::MNChromeNeedsUpdating,
												 this,
												 (XFE_FunctionNotification)MNChromeUpdate_cb);

	theFrame = NULL;

D(	printf ("leaving XFE_FolderFrame::~XFE_FolderFrame()\n");)
}

XP_Bool
XFE_FolderFrame::isCommandEnabled(CommandType cmd,
								   void *calldata, XFE_CommandInfo*)
{
	if (cmd == xfeCmdToggleLocationToolbar)
		return True;
    else if (cmd == xfeCmdCleanUpDisk)
        return True;
	else
		return XFE_Frame::isCommandEnabled(cmd, calldata);
}

void
XFE_FolderFrame::doCommand(CommandType cmd,
                            void *calldata, XFE_CommandInfo* info)
{
	if (cmd == xfeCmdToggleLocationToolbar)
    {
		if (m_banner)
        {
			// Toggle the showing state
			m_banner->toggle();
			
			// Configure the logo
			configureLogo();
			
			// Do the attachments
			doAttachments();
			
			// Update prefs
			toolboxItemChangeShowing(m_banner);

			// Update chrome
			notifyInterested(XFE_View::chromeNeedsUpdating);
        }

		return;
    } else if ( (cmd == xfeCmdComposeMessage) ||
                (cmd == xfeCmdComposeMessagePlain) ||
                (cmd == xfeCmdComposeMessageHTML) || 
                (cmd == xfeCmdComposeArticle) ||
                (cmd == xfeCmdComposeArticlePlain) ||
                (cmd == xfeCmdComposeArticleHTML) )

        {
                XFE_FolderView *tview = (XFE_FolderView*)m_view;

                tview->doCommand(cmd, calldata,info);  

        }

	else
		XFE_Frame::doCommand(cmd, calldata, info);
}

XP_Bool
XFE_FolderFrame::handlesCommand(CommandType cmd,
				 void *calldata, XFE_CommandInfo*)
{
	if (cmd == xfeCmdToggleLocationToolbar)
		return True;
	else
		return XFE_Frame::handlesCommand(cmd, calldata);
}

char *
XFE_FolderFrame::commandToString(CommandType cmd,
								  void *calldata, XFE_CommandInfo*)
{
	if (cmd == xfeCmdToggleLocationToolbar)
	{
		char *res = NULL;
		
		if (m_banner->isShown())
            res = "hideLocationToolbarCmdString";
        else
            res = "showLocationToolbarCmdString";
		
        return stringFromResource(res);
    }
	else
    {
		return XFE_Frame::commandToString(cmd, calldata);
    }
}

void
XFE_FolderFrame::show()
{
  XFE_Frame::show();
}

XFE_CALLBACK_DEFN(XFE_FolderFrame, MNChromeUpdate)(XFE_NotificationCenter*,
												   void*, void*)
{
	notifyInterested(XFE_View::chromeNeedsUpdating);
}

void
XFE_FolderFrame::show(MSG_FolderInfo *info)
{
  XFE_FolderView *fview = (XFE_FolderView*)m_view;

  show();

  fview->selectFolder(info);
}

void
XFE_FolderFrame::show(MSG_ViewIndex index)
{
	MSG_FolderInfo *info = NULL;
	XFE_FolderView *fview = (XFE_FolderView*)m_view;

	if (!index) {
		int m_inboxes = MSG_GetFoldersWithFlag(XFE_MNView::getMaster(),
											   MSG_FOLDER_FLAG_INBOX,
											   NULL, 0);
		if (m_inboxes == 1) {
			if (1 != MSG_GetFoldersWithFlag(XFE_MNView::getMaster(),
											MSG_FOLDER_FLAG_INBOX,
											&info, 1))
				info = NULL;
		}/* if */
		if (!info) {
			// Prompt the user to enter the email address
			char tmp[256];
			XP_SAFE_SPRINTF(tmp, sizeof(tmp),
							"%s",
							XP_GetString(XFE_INBOX_DOESNT_EXIST));
			fe_Alert_2(CONTEXT_WIDGET(m_context), tmp);

		}/* if */
	}/* if */
	else {
		info = MSG_GetFolderInfo(fview->getPane(),
								 index);
	}/* else */

	show();
	if (info && fview)
		fview->selectFolder(info);
}

extern "C" MWContext*
fe_showFoldersWithSelected(Widget toplevel,
						   XFE_Frame *parent_frame,
						   Chrome *chromespec,
						   MSG_FolderInfo *info)
{
  if (theFrame == NULL)
    theFrame = new XFE_FolderFrame(toplevel, parent_frame, chromespec);
  
  if (info)
    theFrame->show(info);
  else
    theFrame->show();

  return theFrame->getContext();
}

extern "C" MWContext*
fe_showFolders(Widget toplevel, XFE_Frame *parent_frame, Chrome *chromespec)
{
D(	printf ("in fe_showFolders()\n");)

  // we are assuming that the first folder with the MAIL flag
  // is the mail server...  I hope this is an ok assumption to
  // make.

  if (theFrame == NULL)
	  theFrame = new XFE_FolderFrame(toplevel, parent_frame, chromespec);
  
  theFrame->show((MSG_ViewIndex)0);

D(	printf ("leaving fe_showFolders()\n");)
	
	return theFrame->getContext();
}

extern "C" MWContext*
fe_showNewsgroups(Widget toplevel,
				  XFE_Frame *parent_frame,
				  Chrome *chromespec)
{
D(	printf ("in fe_showNewsgroups()\n");)
  MSG_FolderInfo *folderinfo = NULL;

  if (MSG_GetFoldersWithFlag(XFE_MNView::getMaster(),
			     MSG_FOLDER_FLAG_NEWS_HOST,
			     NULL, 0) > 0)
    {
      MSG_GetFoldersWithFlag(XFE_MNView::getMaster(),
			     MSG_FOLDER_FLAG_NEWS_HOST,
			     &folderinfo, 1);
    }

D(	printf ("leaving fe_showNewsgroups()\n");)

  return fe_showFoldersWithSelected(toplevel, parent_frame, chromespec, folderinfo);
}

//////////////////////////////////////////////////////////////////////////
//
// Toolbox methods
//
//////////////////////////////////////////////////////////////////////////
void
XFE_FolderFrame::toolboxItemSnap(XFE_ToolboxItem * item)
{
	XP_ASSERT( item == m_toolbar || item == m_banner );

	// Navigation
	fe_globalPrefs.folders_navigation_toolbar_position = m_toolbar->getPosition();

	// Location
	fe_globalPrefs.folders_location_toolbar_position = m_banner->getPosition();
}
//////////////////////////////////////////////////////////////////////////
void
XFE_FolderFrame::toolboxItemClose(XFE_ToolboxItem * item)
{
	XP_ASSERT( item != NULL );

	// Navigation
	if (item == m_toolbar)
	{
		fe_globalPrefs.folders_navigation_toolbar_open = False;
	}
	// Location
	else if (item == m_banner)
	{
		fe_globalPrefs.folders_location_toolbar_open = False;
	}
}
//////////////////////////////////////////////////////////////////////////
void
XFE_FolderFrame::toolboxItemOpen(XFE_ToolboxItem * item)
{
	XP_ASSERT( item != NULL );

	// Navigation
	if (item == m_toolbar)
	{
		fe_globalPrefs.folders_navigation_toolbar_open = True;
	}
	// Location
	else if (item == m_banner)
	{
		fe_globalPrefs.folders_location_toolbar_open = True;
	}
}
//////////////////////////////////////////////////////////////////////////
void
XFE_FolderFrame::toolboxItemChangeShowing(XFE_ToolboxItem * item)
{
	XP_ASSERT( item != NULL );

	// Navigation
	if (item == m_toolbar)
	{
		fe_globalPrefs.folders_navigation_toolbar_showing = item->isShown();
	}
	// Location
	else if (item == m_banner)
	{
		fe_globalPrefs.folders_location_toolbar_showing = item->isShown();
	}
}
//////////////////////////////////////////////////////////////////////////
void
XFE_FolderFrame::configureToolbox()
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

	// Navigation
	if (m_toolbar)
	{
		m_toolbar->setShowing(fe_globalPrefs.folders_navigation_toolbar_showing);
		m_toolbar->setOpen(fe_globalPrefs.folders_navigation_toolbar_open);
		m_toolbar->setPosition(fe_globalPrefs.folders_navigation_toolbar_position);
	}

	// Location
	if (m_banner)
	{
		m_banner->setShowing(fe_globalPrefs.folders_location_toolbar_showing);
		m_banner->setOpen(fe_globalPrefs.folders_location_toolbar_open);
		m_banner->setPosition(fe_globalPrefs.folders_location_toolbar_position);
	}

}

extern int XFE_SEND_UNSENTMAIL;

XP_Bool 
XFE_FolderFrame::isOkToClose()
{
   Boolean haveQueuedMail = False;
   XFE_FolderView *tview = (XFE_FolderView*)m_view;

   if ( XFE_MozillaApp::theApp()->mailNewsWindowCount() == 1 )
   {    

   MSG_CommandStatus(tview->getPane(), MSG_DeliverQueuedMessages, NULL, 0,
                &haveQueuedMail, NULL, NULL, NULL );

   if (haveQueuedMail) {
          void * sendNow = 0;
          const char *buf =  XP_GetString (XFE_SEND_UNSENTMAIL);
          sendNow = fe_dialog (CONTEXT_WIDGET(m_context),
                                 "sendNow", buf, TRUE, 0, TRUE, FALSE, 0);
          if (sendNow) {
            MSG_Command (tview->getPane(), MSG_DeliverQueuedMessages, NULL, 0);
            return False;
          }
        }

#ifdef CLEAN_UP_NEWS    // see comment in ThreadFrame.cpp
#ifdef DEBUG_akkana
	    printf("FolderFrame: Checking whether cleanup is needed\n");
#endif

        // See if we need to clean up based on disk space prefs
        if (MSG_CleanupNeeded(tview->getMaster()))
        {
#ifdef DEBUG_akkana
            printf("Cleaning up\n");
#endif
            // create progress pane
            XFE_MailDownloadFrame* progressFrame = 
                new XFE_MailDownloadFrame(XfeAncestorFindApplicationShell(getBaseWidget()),
                                          this,
                                          tview->getPane());
            progressFrame->cleanUpNews();
            return True;
        }
#endif /* CLEAN_UP_NEWS */
    }
    return True;

}
//////////////////////////////////////////////////////////////////////////
