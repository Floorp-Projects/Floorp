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
   ThreadFrame.cpp -- Thread window stuff
   Created: Chris Toshok <toshok@netscape.com>, 29-Aug-96.
 */



#include "MozillaApp.h"
#include "FolderFrame.h"
#include "FolderView.h"
#include "FolderMenu.h"
#include "AttachmentMenu.h"
#include "ThreadFrame.h"
#include "ThreadView.h"
#include "Dashboard.h"
#include "MsgView.h"
#include "ViewGlue.h"
#include "xpassert.h"
#include "prefapi.h"
#ifdef CLEAN_UP_NEWS
#include "Xfe/Xfe.h"
#include "MailDownloadFrame.h"
#endif

#ifdef USE_3PANE
#include "ThreePaneView.h"
#endif

#include "libi18n.h"
#include "intl_csi.h"

#ifdef DEBUG_toshok
#define D(x) x
#else
#define D(x)
#endif

#ifdef DEBUG_ramiro
#define DR(x) x
#else
#define DR(x)
#endif

#ifdef DEBUG_dora
#define DD(x) x
#else
#define DD(x)
#endif

#include "xpgetstr.h"
extern int XFE_FOLDER_ON_SERVER;
extern int XFE_FOLDER_ON_LOCAL_MACHINE;
extern int XFE_MN_UNREAD_AND_TOTAL;
extern int XFE_INBOX_DOESNT_EXIST;
extern int XFE_SEND_UNSENTMAIL;


#define THREEPANEVIEW_SHOW_PREF "mail.threadpane.3pane"
//
MenuSpec XFE_ThreadFrame::file_menu_spec[] = {
	{ "newSubmenu",		        CASCADEBUTTON, (MenuSpec *) &XFE_Frame::new_submenu_spec },
	{ xfeCmdNewFolder,		    PUSHBUTTON },
	{ xfeCmdOpenSelected,		PUSHBUTTON },
//      { "openAttachmentsSubmenu",	DYNA_CASCADEBUTTON, 
//          NULL, NULL, False, NULL, XFE_AttachmentMenu::generate },
	MENU_SEPARATOR,
	{ xfeCmdSaveMessagesAs,	PUSHBUTTON },
	{ xfeCmdEditMessage,	PUSHBUTTON },
	MENU_SEPARATOR,
	{ xfeCmdEmptyTrash,		PUSHBUTTON },
	{ xfeCmdCompressFolders,PUSHBUTTON },
	{ xfeCmdCompressAllFolders,PUSHBUTTON },
	MENU_SEPARATOR,
	{ "newMsgSubmenu",		CASCADEBUTTON, (MenuSpec *) &XFE_Frame::newMsg_submenu_spec },
	{ xfeCmdSendMessagesInOutbox,	PUSHBUTTON },
    { xfeCmdCleanUpDisk, PUSHBUTTON },
#if 0
	{ xfeCmdUpdateMessageCount,	    PUSHBUTTON },
#endif
	{ xfeCmdAddNewsgroup,		    PUSHBUTTON },
	//MENU_SEPARATOR,
	//{ xfeCmdGoOffline,		PUSHBUTTON },
	MENU_SEPARATOR,
	//{ xfeCmdPrintSetup,		PUSHBUTTON },
	//{ xfeCmdPrintPreview,	PUSHBUTTON },
	{ xfeCmdPrint,		    PUSHBUTTON },
	MENU_SEPARATOR,
	{ xfeCmdClose,		    PUSHBUTTON },
	{ xfeCmdExit,			PUSHBUTTON },
	{ NULL }
};

MenuSpec XFE_ThreadFrame::edit_menu_spec[] = {
	{ xfeCmdUndo,			PUSHBUTTON },
	{ xfeCmdRedo,			PUSHBUTTON },
	MENU_SEPARATOR,
	{ xfeCmdCut,			PUSHBUTTON },
	{ xfeCmdCopy,			PUSHBUTTON },
	{ xfeCmdPaste,		    PUSHBUTTON },
	{ xfeCmdDeleteMessage,	PUSHBUTTON },

	{ "selectSubmenu",		CASCADEBUTTON, (MenuSpec *) &XFE_Frame::select_submenu_spec },
	MENU_SEPARATOR,
	{ xfeCmdFindInObject,		PUSHBUTTON },
	{ xfeCmdFindAgain,		PUSHBUTTON },
	{ xfeCmdSearch,		    PUSHBUTTON },
	{ xfeCmdSearchAddress,	PUSHBUTTON },
	MENU_SEPARATOR,
	{ xfeCmdEditConfiguration,	PUSHBUTTON },
	{ xfeCmdModerateDiscussion,	PUSHBUTTON },
	MENU_SEPARATOR,
	{ xfeCmdEditMailFilterRules,PUSHBUTTON },
    //xxxAdd Folder/Discussion Properties
	{ xfeCmdEditPreferences,	PUSHBUTTON },
	{ NULL }
};

MenuSpec XFE_ThreadFrame::view_menu_spec[] = {
    { xfeCmdToggleNavigationToolbar,PUSHBUTTON },
    { xfeCmdToggleLocationToolbar,    PUSHBUTTON },
	{ xfeCmdToggleMessageExpansion,	PUSHBUTTON },
	MENU_SEPARATOR,
	{ "sortSubmenu",		CASCADEBUTTON, (MenuSpec *) &XFE_Frame::sort_submenu_spec },
	{ "expandCollapseSubmenu",		CASCADEBUTTON, (MenuSpec *) &XFE_Frame::expand_collapse_submenu_spec },
	{ "threadSubmenu",		CASCADEBUTTON, (MenuSpec *) &XFE_Frame::threads_submenu_spec },
	{ "headersSubmenu",		CASCADEBUTTON, (MenuSpec *) &XFE_Frame::headers_submenu_spec },
    // This should just be a toggle.  -slamm
	{ "attachmentsSubmenu",	CASCADEBUTTON, (MenuSpec *) &XFE_Frame::attachments_submenu_spec },
    MENU_SEPARATOR,
    { xfeCmdIncreaseFont,		PUSHBUTTON },
    { xfeCmdDecreaseFont,		PUSHBUTTON },
	MENU_SEPARATOR,
	{ xfeCmdReload,		PUSHBUTTON },
	{ xfeCmdShowImages,		PUSHBUTTON },
	{ xfeCmdRefresh,		PUSHBUTTON },
	{ xfeCmdStopLoading,		PUSHBUTTON },
	MENU_SEPARATOR,
	{ xfeCmdRot13Message,		PUSHBUTTON },
	{ xfeCmdWrapLongLines,		TOGGLEBUTTON },
	MENU_SEPARATOR,
	{ xfeCmdViewPageSource,	PUSHBUTTON },
	{ xfeCmdViewPageInfo,	PUSHBUTTON },
	MENU_SEPARATOR,
	{ "encodingSubmenu",    CASCADEBUTTON, (MenuSpec *) &XFE_Frame::encoding_menu_spec },
	{ NULL }
};

//
MenuSpec XFE_ThreadFrame::go_menu_spec[] = {
	{ xfeCmdNextMessage,			PUSHBUTTON },
	{ xfeCmdNextUnreadMessage,		PUSHBUTTON },
	{ xfeCmdNextFlaggedMessage,		PUSHBUTTON },
	{ xfeCmdNextUnreadThread,		PUSHBUTTON },
	{ xfeCmdNextCollection,		    PUSHBUTTON },
	{ xfeCmdNextUnreadCollection, 	PUSHBUTTON },
	MENU_SEPARATOR,
	{ xfeCmdPreviousMessage,		PUSHBUTTON },
	{ xfeCmdPreviousUnreadMessage,	PUSHBUTTON },
	{ xfeCmdPreviousFlaggedMessage,	PUSHBUTTON },
	MENU_SEPARATOR,
	{ xfeCmdFirstFlaggedMessage,	PUSHBUTTON },
	MENU_SEPARATOR,
    { xfeCmdBack,			PUSHBUTTON },
    { xfeCmdForward,		PUSHBUTTON },
	{ NULL }
};

//
MenuSpec XFE_ThreadFrame::offline_submenu_spec[] = {
	{ xfeCmdGetSelectedMessagesForOffline, PUSHBUTTON },
	{ xfeCmdGetFlaggedMessagesForOffline,	 PUSHBUTTON },
	{ xfeCmdChooseMessagesForOffline,	 PUSHBUTTON },
	{ NULL }
};

MenuSpec XFE_ThreadFrame::ignore_submenu_spec[] = {
	{ xfeCmdIgnoreThread,		PUSHBUTTON },
	{ NULL }
};

MenuSpec XFE_ThreadFrame::message_menu_spec[] = {
	{ xfeCmdComposeMessage,		PUSHBUTTON },
	{ "replySubmenu",			CASCADEBUTTON, (MenuSpec *) &XFE_Frame::reply_submenu_spec },
	{ xfeCmdForwardMessage,		PUSHBUTTON },
	{ xfeCmdForwardMessageQuoted,		PUSHBUTTON },
	//{ xfeCmdInviteToNewsgroup,		PUSHBUTTON },
	MENU_SEPARATOR,
	{ "addToABSubmenu",			CASCADEBUTTON, (MenuSpec *) &XFE_Frame::addrbk_submenu_spec },
	{ "fileSubmenu",			DYNA_CASCADEBUTTON, NULL, NULL, False, (void*)xfeCmdMoveMessage, XFE_FolderMenu::generate },
	{ "copySubmenu",			DYNA_CASCADEBUTTON, NULL, NULL, False, (void*)xfeCmdCopyMessage, XFE_FolderMenu::generate },
	MENU_SEPARATOR,
	{ "markSubmenu",			CASCADEBUTTON, (MenuSpec *) &XFE_Frame::mark_submenu_spec },
	{ xfeCmdMarkMessage,			PUSHBUTTON },
	{ xfeCmdUnmarkMessage,		PUSHBUTTON },
	MENU_SEPARATOR,
	{ xfeCmdIgnoreThread,		PUSHBUTTON },
	{ xfeCmdWatchThread,		PUSHBUTTON },
	{ NULL }
};

//
MenuSpec XFE_ThreadFrame::menu_bar_spec[] = {
	{ xfeMenuFile, 	CASCADEBUTTON, file_menu_spec },
	{ xfeMenuEdit, 	CASCADEBUTTON, edit_menu_spec },
	{ xfeMenuView, 	CASCADEBUTTON, view_menu_spec },
	{ xfeMenuGo, 		CASCADEBUTTON, go_menu_spec },
	{ xfeMenuMessage, 	CASCADEBUTTON, message_menu_spec },
	{ xfeMenuWindow,	CASCADEBUTTON, XFE_Frame::window_menu_spec },
	{ xfeMenuHelp, 	CASCADEBUTTON, XFE_Frame::help_menu_spec },
	{ NULL }
};

ToolbarSpec XFE_ThreadFrame::toolbar_spec[] = {
	{ xfeCmdGetNewMessages,	PUSHBUTTON, &MNTB_GetMsg_group},
	{  // XX mail only
		xfeCmdComposeMessage,	
		CASCADEBUTTON, 
		&MNTB_Compose_group, NULL, NULL, NULL,				// Icons
		compose_message_submenu_spec,									// Submenu spec
		NULL, NULL,											// Generate proc/arg
		XFE_TOOLBAR_DELAY_LONG								// Popup delay
	},
	{  // XX newsgroup only
		xfeCmdComposeArticle,	
		CASCADEBUTTON, 
		&MNTB_Compose_group, NULL, NULL, NULL,				// Icons
		compose_article_submenu_spec,									// Submenu spec
		NULL, NULL,											// Generate proc/arg
		XFE_TOOLBAR_DELAY_LONG								// Popup delay
	},
	TOOLBAR_SEPARATOR,
	{  // XX mail only
		xfeCmdReplyToSender,	
		CASCADEBUTTON, 
		&MNTB_Reply_group, NULL, NULL, NULL,				// Icons
		reply_submenu_spec,									// Submenu spec
		NULL, NULL,											// Generate proc/arg
		XFE_TOOLBAR_DELAY_LONG								// Popup delay
	},
	{  // XX news only
		xfeCmdReplyToNewsgroup,	
		CASCADEBUTTON, 
		&MNTB_Reply_group, NULL, NULL, NULL,				// Icons
		reply_submenu_spec,									// Submenu spec
		NULL, NULL,											// Generate proc/arg
		XFE_TOOLBAR_DELAY_LONG								// Popup delay
	},
	{ xfeCmdForwardMessage,	PUSHBUTTON, &MNTB_Forward_group },
	TOOLBAR_SEPARATOR,
	{
		xfeCmdCopyMessage,  // XX news only
		DYNA_CASCADEBUTTON, 
		&MNTB_File_group, NULL, NULL, NULL, NULL,
		XFE_FolderMenu::generate, (void*)xfeCmdCopyMessage,
		XFE_TOOLBAR_DELAY_SHORT
	},
	{
		xfeCmdMoveMessage, // XX mail only
		DYNA_CASCADEBUTTON, 
		&MNTB_File_group, NULL, NULL, NULL, NULL,
		XFE_FolderMenu::generate, (void*)xfeCmdMoveMessage,
		XFE_TOOLBAR_DELAY_SHORT
	},
	{
		xfeCmdNextUnreadMessage,
		CASCADEBUTTON, 
		&MNTB_Next_group, NULL, NULL, NULL,					// Icons
		next_submenu_spec,									// Submenu spec
		NULL, NULL,											// Generate proc/arg
		XFE_TOOLBAR_DELAY_LONG								// Popup delay
	},
	{ xfeCmdPrint,			PUSHBUTTON, &TB_Print_group },
	{ xfeCmdViewSecurity,		PUSHBUTTON, 
			&TB_Unsecure_group,
                       	&TB_Secure_group,
                       	&MNTB_SignUnsecure_group,
                        &MNTB_SignSecure_group},
	{ xfeCmdMarkMessageRead, // XX news only
	  CASCADEBUTTON, 
	  &MNTB_MarkRead_group, NULL, NULL, NULL,				// Icons
	  mark_submenu_spec,									// Submenu spec
	  NULL, NULL,											// Generate proc/arg
	  XFE_TOOLBAR_DELAY_LONG								// Popup delay
	},
	{ xfeCmdDeleteMessage,	PUSHBUTTON, &MNTB_Trash_group }, // XX mail only
	{ xfeCmdStopLoading,		PUSHBUTTON, &TB_Stop_group },
	{ NULL }
};

XFE_ThreadFrame::XFE_ThreadFrame(Widget toplevel, XFE_Frame *parent_frame,
								 Chrome *chromespec)
	: XFE_Frame("MailThread", toplevel, parent_frame,
				FRAME_MAILNEWS_THREAD, chromespec, True)
{
  INTL_CharSetInfo c = LO_GetDocumentCharacterSetInfo(m_context);
  INTL_SetCSIWinCSID(c, 
		INTL_DocToWinCharSetID (CONTEXT_DATA (m_context)->xfe_doc_csid));

    geometryPrefName = "mail.thread";

	// create the thread view
#ifdef USE_3PANE
	XFE_ThreePaneView *view3;
#endif
	XFE_ThreadView *tview;
	XFE_ProxyIcon *proxy;

	if (parent_frame)
	  fe_copy_context_settings(m_context, parent_frame->getContext());

	m_banner = new XFE_MNBanner(this, m_toolbox);
	
#ifdef USE_3PANE

        XP_Bool show_folder;
        PREF_GetBoolPref(THREEPANEVIEW_SHOW_PREF, &show_folder);

        m_banner->setShowFolder(show_folder); /* This has to be set before view 
					  creation */
	view3 = new XFE_ThreePaneView(this, getChromeParent(), NULL, m_context);

	tview = (XFE_ThreadView*)(view3->getThreadView());

#else
	tview = new XFE_ThreadView(this, getChromeParent(), NULL, m_context);
#endif


	m_dropdown = new XFE_FolderDropdown(this, m_banner->getComponentParent(), False, True, True);
	
	m_dropdown->registerInterest(XFE_FolderDropdown::folderSelected, 
								 this,
								 (XFE_FunctionNotification)dropdownFolderLoad_cb);

	m_banner->setTitleComponent(m_dropdown);
	m_banner->setMommyIcon(&MN_Mommy_group);

	tview->registerInterest(XFE_MNView::bannerNeedsUpdating,
							this,
							(XFE_FunctionNotification)updateBanner_cb);

#ifdef USE_3PANE
	setView(view3);
#else
	setView(tview);
#endif


	setMenubar(menu_bar_spec);
	setToolbar(toolbar_spec);

//	m_banner->show();
#ifdef USE_3PANE
	view3->show();
#else
	tview->show();
#endif

	/* we need the different folder icons initialized in the FolderView for our proxy. */
	{
		Pixel bg_pixel;
		
		XtVaGetValues(m_banner->getBaseWidget(), XmNbackground, &bg_pixel, 0);

		XFE_FolderView::initFolderIcons(m_widget, bg_pixel, getFGPixel());
	}

	// safe defaults.  They will be overridden when we update the banner.
	m_banner->setProxyIcon(&XFE_MNView::folderIcon);
	proxy = m_banner->getProxyIcon();
#if !defined(USE_MOTIF_DND)
	proxy->setDragType(FE_DND_MAIL_FOLDER, tview, &XFE_ThreadView::source_drop_func);
#endif

	// Configure the dashboard
	XP_ASSERT( m_dashboard != NULL );
	
	m_dashboard->setShowSecurityIcon(True);
	m_dashboard->setShowSignedIcon(True);
	m_dashboard->setShowStatusBar(True);
	m_dashboard->setShowProgressBar(True);

  // Configure the toolbox for the first time
  configureToolbox();

	XFE_MozillaApp::theApp()->registerInterest(XFE_MNView::MNChromeNeedsUpdating,
											   this,
											   (XFE_FunctionNotification)MNChromeUpdate_cb);

	XFE_MozillaApp::theApp()->registerInterest(XFE_MNView::folderChromeNeedsUpdating,
											   this,
											   (XFE_FunctionNotification)FolderChromeUpdate_cb);

	XFE_MozillaApp::theApp()->registerInterest(XFE_MNView::folderDeleted,
											   this,
											   (XFE_FunctionNotification)folderDeleted_cb);

}



XFE_ThreadFrame::~XFE_ThreadFrame()
{
	D(printf ("In XFE_ThreadFrame::~XFE_ThreadFrame()\n");)

	XFE_MozillaApp::theApp()->unregisterInterest(XFE_MNView::MNChromeNeedsUpdating,
												 this,
												 (XFE_FunctionNotification)MNChromeUpdate_cb);
	
	XFE_MozillaApp::theApp()->unregisterInterest(XFE_MNView::folderChromeNeedsUpdating,
												 this,
												 (XFE_FunctionNotification)FolderChromeUpdate_cb);

#ifdef USE_3PANE
	XFE_ThreePaneView *view3 = (XFE_ThreePaneView*)m_view;
	XFE_ThreadView *tview = (XFE_ThreadView*)(view3->getThreadView());
#else
	XFE_ThreadView *tview = (XFE_ThreadView*)m_view;
#endif
	tview->unregisterInterest(XFE_MNView::bannerNeedsUpdating,
                                                 this,
                                                 (XFE_FunctionNotification)updateBanner_cb);

	XFE_MozillaApp::theApp()->unregisterInterest(XFE_MNView::folderDeleted,
												 this,
												 (XFE_FunctionNotification)folderDeleted_cb);
}

XFE_CALLBACK_DEFN(XFE_ThreadFrame, mommy)(XFE_NotificationCenter *,
										  void *,
										  void *)
{
	fe_showFolders(XtParent(m_widget), this, NULL);
}

XFE_CALLBACK_DEFN(XFE_ThreadFrame, updateBanner)(XFE_NotificationCenter *,
												 void *,
												 void *)
{
	MSG_FolderLine folderline;
#ifdef USE_3PANE
        XFE_ThreadView *t_view = (XFE_ThreadView*)(
			((XFE_ThreePaneView*)m_view)->getThreadView());
#else
        XFE_ThreadView *t_view = (XFE_ThreadView*)m_view;
#endif
	XFE_ProxyIcon *proxy;

	updateReadAndTotalCounts();

	if (MSG_GetFolderLineById(XFE_MNView::getMaster(),
							  t_view->getFolderInfo(), &folderline))
		{

			m_dropdown->selectFolder(t_view->getFolderInfo(),False);
			
			m_banner->setProxyIcon(XFE_FolderView::treeInfoToIcon(folderline.level - 1,
																  folderline.flags,
																  False));

			proxy = m_banner->getProxyIcon();
								   
			if (folderline.flags & MSG_FOLDER_FLAG_NEWSGROUP)
				{
#if !defined(USE_MOTIF_DND)
					proxy->setDragType(FE_DND_NEWS_FOLDER, t_view, &XFE_ThreadView::source_drop_func);
#endif

					m_banner->setSubtitle("");
				}
			else
				{
#if !defined(USE_MOTIF_DND)
					proxy->setDragType(FE_DND_MAIL_FOLDER, t_view, &XFE_ThreadView::source_drop_func);
#endif
					
					if (folderline.flags & MSG_FOLDER_FLAG_IMAPBOX)
						m_banner->setSubtitle(XP_GetString (XFE_FOLDER_ON_SERVER ));
					else
						m_banner->setSubtitle(XP_GetString ( XFE_FOLDER_ON_LOCAL_MACHINE ));
				}
		}
}

XFE_CALLBACK_DEFN(XFE_ThreadFrame, folderDeleted)(XFE_NotificationCenter*,
					       void *, void *cdata)
{
	/* Need to check if this folder ?
	 * hide or switch ?
	 */
	MSG_FolderInfo *info = (MSG_FolderInfo *) cdata;
	if (info == getFolderInfo())
		delete_response();
}


void
XFE_ThreadFrame::loadFolder(MSG_FolderInfo *folderInfo)
{
#ifdef USE_3PANE
	XFE_ThreePaneView *view3 = (XFE_ThreePaneView*)m_view;
	XFE_ThreadView *tview = (XFE_ThreadView*)(view3->getThreadView());
#else
	XFE_ThreadView *tview = (XFE_ThreadView*)m_view;
#endif
	MSG_FolderLine folderline;
	uint16 foldercsid;

	foldercsid = MSG_GetFolderCSID(folderInfo);
	if ((CS_UNKNOWN==foldercsid) || (CS_DEFAULT==foldercsid))
		{
			foldercsid = INTL_DefaultDocCharSetID(NULL);
			MSG_SetFolderCSID(folderInfo, foldercsid);
		}
	CONTEXT_DATA(m_context)->xfe_doc_csid = foldercsid;
	if (MSG_GetFolderLineById(XFE_MNView::getMaster(),
							  folderInfo, &folderline))
		{
			if (folderline.flags & MSG_FOLDER_FLAG_NEWSGROUP)
				{
					m_toolbar->hideButton(xfeCmdComposeMessage, PUSHBUTTON);
					m_toolbar->hideButton(xfeCmdDeleteMessage, PUSHBUTTON);
					m_toolbar->hideButton(xfeCmdReplyToSender, CASCADEBUTTON);
					m_toolbar->hideButton(xfeCmdMoveMessage, CASCADEBUTTON);

					m_toolbar->showButton(xfeCmdComposeArticle, PUSHBUTTON);
					m_toolbar->showButton(xfeCmdReplyToNewsgroup, CASCADEBUTTON);
					m_toolbar->showButton(xfeCmdMarkMessageRead, CASCADEBUTTON);
					m_toolbar->showButton(xfeCmdCopyMessage, CASCADEBUTTON);
				}
			else
				{
					m_toolbar->showButton(xfeCmdComposeMessage, PUSHBUTTON);
					m_toolbar->showButton(xfeCmdDeleteMessage, PUSHBUTTON);
					m_toolbar->showButton(xfeCmdReplyToSender, CASCADEBUTTON);
					m_toolbar->showButton(xfeCmdMoveMessage, CASCADEBUTTON);
					
					m_toolbar->hideButton(xfeCmdComposeArticle, PUSHBUTTON);
					m_toolbar->hideButton(xfeCmdReplyToNewsgroup, CASCADEBUTTON);
					m_toolbar->hideButton(xfeCmdMarkMessageRead, CASCADEBUTTON);
					m_toolbar->hideButton(xfeCmdCopyMessage, CASCADEBUTTON);
				}
		}

#ifdef USE_3PANE
	view3->selectFolder(folderInfo);
	DD(printf("ThreadFrame::loadFolder x%x \n", folderInfo);)
#endif
	tview->loadFolder(folderInfo);
}

MSG_FolderInfo *
XFE_ThreadFrame::getFolderInfo()
{
#ifdef USE_3PANE
	XFE_ThreadView *tview = (XFE_ThreadView*)(
			((XFE_ThreePaneView*)m_view)->getThreadView());
#else
	XFE_ThreadView *tview = (XFE_ThreadView*)m_view;
#endif

	return tview->getFolderInfo();
}

void
XFE_ThreadFrame::updateReadAndTotalCounts()
{
	MSG_FolderLine folderline;
#ifdef USE_3PANE
        XFE_ThreadView *t_view = (XFE_ThreadView*)(
			((XFE_ThreePaneView*)m_view)->getThreadView());
#else
	XFE_ThreadView *t_view = (XFE_ThreadView*)m_view;
#endif
	MSG_FolderInfo *info = t_view->getFolderInfo();

	if (info)
		{
			if (MSG_GetFolderLineById(XFE_MNView::getMaster(),
									  info, &folderline))
				{
					char buf[200];
					
					PR_snprintf(buf, sizeof(buf), 
								XP_GetString(XFE_MN_UNREAD_AND_TOTAL),
								folderline.unseen,
								folderline.total);
					
					m_banner->setInfo(buf);
				}
		}
}

XFE_CALLBACK_DEFN(XFE_ThreadFrame, dropdownFolderLoad)(XFE_NotificationCenter*,
													   void *, void *calldata)
{
	MSG_FolderInfo *info = (MSG_FolderInfo*)calldata;
	
	loadFolder(info);
}

XFE_CALLBACK_DEFN(XFE_ThreadFrame, MNChromeUpdate)(XFE_NotificationCenter*,
												   void*, void*)
{
	notifyInterested(XFE_View::chromeNeedsUpdating);

	updateReadAndTotalCounts();
}

XFE_CALLBACK_DEFN(XFE_ThreadFrame, FolderChromeUpdate)(XFE_NotificationCenter*,
													   void*, void *cd)
{
	MSG_FolderInfo *info = (MSG_FolderInfo*)cd;

	if (info == getFolderInfo())
		{
			notifyInterested(XFE_View::chromeNeedsUpdating);
			
			updateReadAndTotalCounts();
		}
}

XFE_ThreadFrame *
XFE_ThreadFrame::frameForInfo(MSG_FolderInfo *info)
{
	XP_List *thread_frame_list = XFE_MozillaApp::theApp()->getFrameList( FRAME_MAILNEWS_THREAD );
	XP_List *current_list_entry;

	for (current_list_entry = thread_frame_list;
		 current_list_entry != NULL;
		 current_list_entry = current_list_entry->next)
		{
			if (current_list_entry->object
				&& ((XFE_Frame*)current_list_entry->object)->getType() == FRAME_MAILNEWS_THREAD // sanity check
				&& ((XFE_ThreadFrame*)current_list_entry->object)->getFolderInfo() == info)
				{
					D( printf ("Found one.\n");)
						return (XFE_ThreadFrame*)current_list_entry->object;
				}
		}

	return NULL;
}

extern "C" MWContext*
fe_showInbox(Widget toplevel, XFE_Frame *parent_frame,
			 Chrome *chromespec, XP_Bool with_reuse, XP_Bool getNewMail)
{
	MSG_FolderInfo *inbox_info = NULL;
	int num_inboxes;

	num_inboxes = MSG_GetFoldersWithFlag(XFE_MNView::getMaster(),
										 MSG_FOLDER_FLAG_INBOX,
										 NULL, 0);
	if (num_inboxes > 0) {
		int32 error = MSG_GetFoldersWithFlag(XFE_MNView::getMaster(),
											 MSG_FOLDER_FLAG_INBOX,
											 &inbox_info, 1);
		if (error > 0)
			return fe_showMessages(toplevel, parent_frame, 
								   chromespec, inbox_info, 
								   with_reuse, getNewMail, 
								   MSG_MESSAGEKEYNONE);
	}/* if */

	char tmp[256];
	XP_SAFE_SPRINTF(tmp, sizeof(tmp),
					"%s",
					XP_GetString(XFE_INBOX_DOESNT_EXIST));
	if (toplevel)
		fe_Alert_2(toplevel, tmp);
	return NULL;
}

extern "C" MWContext*
fe_showMessages(Widget toplevel, XFE_Frame *parent_frame, Chrome *chromespec, 
				MSG_FolderInfo *info, XP_Bool with_reuse, XP_Bool getNewMail, MessageKey key)
{
	XFE_ThreadFrame *theFrame = NULL;
	MSG_FolderInfo *curFolderInfo = NULL;
	XP_Bool getNewNow = True; /* use this flag to tell when to get new mail
								 */

	D(	printf ("in fe_showThread()\n");)

	if (with_reuse)
		{
			XP_List *thread_frame_list = 
				XFE_MozillaApp::theApp()->getFrameList( FRAME_MAILNEWS_THREAD );
     
			if (!XP_ListIsEmpty(thread_frame_list))
				{
					theFrame = (XFE_ThreadFrame*)XP_ListTopObject( thread_frame_list );
				}
			else
				{
					theFrame = new XFE_ThreadFrame(toplevel, parent_frame, chromespec);
					getNewNow = False;
				}

			/* query cur folder info
			 */
			curFolderInfo = theFrame->getFolderInfo();

			if (theFrame)
				{
					if ( key != MSG_MESSAGEKEYNONE)
					{
				
#ifdef USE_3PANE
        					XFE_ThreadView *t_view = (XFE_ThreadView*)( 
							((XFE_ThreePaneView*)(theFrame->getView()))->getThreadView());
					  	t_view->openWithKey(key);
#else
					  ((XFE_ThreadView*)(theFrame->getView()))->openWithKey(key);
#endif
					  /* only raise the window to be visible if thread window is not mapped */
					  /* has to be shown before loading folder, otherwise, 
						 it will cause an abort in lay.c */
					  if ( !theFrame->isShown())
					  	theFrame->show(); 

					  /* we need to wait until the folder is loaded
					   */
					  if ((curFolderInfo == NULL) ||
						  (curFolderInfo != info))
						  getNewNow = False;
					  else
						  getNewNow = True;

					  theFrame->loadFolder(info);

					}
 					else
					{
					theFrame->show();

					if ((curFolderInfo == NULL) ||
						(curFolderInfo != info)) {
						theFrame->loadFolder(info);
						getNewNow = False;
					}
					else
						getNewNow = True;
					}
				}
		}
	else
		{
			theFrame = XFE_ThreadFrame::frameForInfo(info);
			if (!theFrame)
				{
					theFrame = new XFE_ThreadFrame(toplevel, parent_frame, chromespec);
					if ( key != MSG_MESSAGEKEYNONE)
					{
#ifdef USE_3PANE
        				XFE_ThreadView *t_view = (XFE_ThreadView*)( 
							((XFE_ThreePaneView*)(theFrame->getView()))->getThreadView());
					t_view->openWithKey(key);
#else
					((XFE_ThreadView*)(theFrame->getView()))->openWithKey(key);
#endif

					//  ((XFE_ThreadView*)(theFrame->getView()))->openWithKey(key);
					 /* has to be shown before loading folder, otherwise, 
					    it will cause an abort in lay.c */
					  if ( !theFrame->isShown() )
						theFrame->show();
					}
					else
					   theFrame->show();
					theFrame->loadFolder(info);

					/* get after folder is loaded
					 */
					getNewNow = False;
				}
			else
				{
					/* query cur folder info
					 */
					curFolderInfo = theFrame->getFolderInfo();

					/* need to test if folder is loaded
					 */
					if ((curFolderInfo == NULL) ||
						(curFolderInfo != info)) {
						getNewNow = False;
					}
					else
						getNewNow = True;

					if ( key != MSG_MESSAGEKEYNONE)
					{
#ifdef USE_3PANE
        				XFE_ThreadView *t_view = (XFE_ThreadView*)( 
							((XFE_ThreePaneView*)(theFrame->getView()))->getThreadView());
					t_view->openWithKey(key);
#else
					((XFE_ThreadView*)(theFrame->getView()))->openWithKey(key);
#endif
					 /* has to be shown before loading folder, otherwise, 
					    it will cause an abort in lay.c */
					  if ( !theFrame->isShown() )
						theFrame->show();


					  theFrame->loadFolder(info);
					}
					else
					  theFrame->show();
				}
		}
	
	if (getNewMail)
		if (theFrame->isCommandEnabled(xfeCmdGetNewMessages)) {
			if (getNewNow)
				/* folder already loaded
				 */
				theFrame->doCommand(xfeCmdGetNewMessages);			
			else
			{
				/* set flag to get new msg after folder is loaded
				 */
#ifdef USE_3PANE
        			XFE_ThreadView *t_view = (XFE_ThreadView*)( 
							((XFE_ThreePaneView*)(theFrame->getView()))->getThreadView());
				t_view->setGetNewMsg(True);
#else
				((XFE_ThreadView*)(theFrame->getView()))->setGetNewMsg(True);				
#endif

			}
		}/* if */

	D(	printf ("leaving fe_showThread()\n");)

	return theFrame->getContext();
}

XP_Bool
XFE_ThreadFrame::isCommandEnabled(CommandType cmd,
								   void *calldata, XFE_CommandInfo*)
{
	if (cmd == xfeCmdToggleLocationToolbar)
		return True;
    else if (cmd == xfeCmdToggleKilledThreads)
        return True;
    else if (cmd == xfeCmdCleanUpDisk)
        return True;
	else
		return XFE_Frame::isCommandEnabled(cmd, calldata);
}

void
XFE_ThreadFrame::doCommand(CommandType cmd,
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
		m_view->doCommand(cmd, calldata,info);	
	}
	else
		XFE_Frame::doCommand(cmd, calldata, info);
}

XP_Bool
XFE_ThreadFrame::handlesCommand(CommandType cmd,
				 void *calldata, XFE_CommandInfo*)
{
	if (cmd == xfeCmdToggleLocationToolbar)
		return True;
	else
		return XFE_Frame::handlesCommand(cmd, calldata);
}

char *
XFE_ThreadFrame::commandToString(CommandType cmd,
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

#undef DEBUG_dora

int
XFE_ThreadFrame::getSecurityStatus()
{
 XP_Bool is_signed = False;
 XP_Bool is_encrypted = False;
#ifdef USE_3PANE
 XFE_ThreadView *tview = (XFE_ThreadView*)(
			((XFE_ThreePaneView*)m_view)->getThreadView());
#else
 XFE_ThreadView *tview = (XFE_ThreadView*)m_view;
#endif

 XFE_MailSecurityStatusType status = XFE_UNSECURE_UNSIGNED;  // Choose a safe default.

#ifdef DEBUG_dora
 printf("XFE_ThreadFrame::getSecurityStatus\n");
#endif

 MIME_GetMessageCryptoState(m_context, 0, 0, &is_signed, &is_encrypted);

 if (tview && tview->isDisplayingNews() )
 {
   // If this is displaying news, we decide if a newsgroup is secure(encrypted)
   // or not by checking the security status ...instead of the crypto state

#ifdef DEBUG_dora
    printf("News Thread Frame context...\n");
#endif
    is_encrypted = XFE_Frame::getSecurityStatus() == XFE_SECURE;
 }

 if (is_encrypted && is_signed )
 {
#ifdef DEBUG_dora
     printf("signed and encrypted %d\n", XFE_SECURE_SIGNED);
#endif
     status = XFE_SECURE_SIGNED;
 }
 else if (!is_encrypted && is_signed)
 {
#ifdef DEBUG_dora
     printf("signed and NO encrypted %d\n", XFE_UNSECURE_SIGNED);
#endif
     status = XFE_UNSECURE_SIGNED;
 }
 else if (is_encrypted && !is_signed)
 {
#ifdef DEBUG_dora
     printf("NO signed and  encrypted %d\n", XFE_SECURE_UNSIGNED);
#endif
     status = XFE_SECURE_UNSIGNED;
 }
 else if (!is_encrypted && !is_signed )
 {
#ifdef DEBUG_dora
     printf("NO signed and  NO encrypted %d\n", XFE_UNSECURE_UNSIGNED);
#endif
     status = XFE_UNSECURE_UNSIGNED;
 } else {
	 // Error.  Play it safe, go unsecure.
	 XP_ASSERT(0);
	 status = XFE_UNSECURE_UNSIGNED;
 }

 return status;
}


//////////////////////////////////////////////////////////////////////////
//
// Toolbox methods
//
//////////////////////////////////////////////////////////////////////////
void
XFE_ThreadFrame::toolboxItemSnap(XFE_ToolboxItem * item)
{
	XP_ASSERT( item == m_toolbar || item == m_banner );

	// Navigation
	fe_globalPrefs.messages_navigation_toolbar_position = m_toolbar->getPosition();

	// Location
	fe_globalPrefs.messages_location_toolbar_position = m_banner->getPosition();
}
//////////////////////////////////////////////////////////////////////////
void
XFE_ThreadFrame::toolboxItemClose(XFE_ToolboxItem * item)
{
	XP_ASSERT( item != NULL );

	// Navigation
	if (item == m_toolbar)
	{
		fe_globalPrefs.messages_navigation_toolbar_open = False;
	}
	// Location
	else if (item == m_banner)
	{
		fe_globalPrefs.messages_location_toolbar_open = False;
	}
}
//////////////////////////////////////////////////////////////////////////
void
XFE_ThreadFrame::toolboxItemOpen(XFE_ToolboxItem * item)
{
	XP_ASSERT( item != NULL );

	// Navigation
	if (item == m_toolbar)
	{
		fe_globalPrefs.messages_navigation_toolbar_open = True;
	}
	// Location
	else if (item == m_banner)
	{
		fe_globalPrefs.messages_location_toolbar_open = True;
	}
}
//////////////////////////////////////////////////////////////////////////
void
XFE_ThreadFrame::toolboxItemChangeShowing(XFE_ToolboxItem * item)
{
	XP_ASSERT( item != NULL );

	// Navigation
	if (item == m_toolbar)
	{
		fe_globalPrefs.messages_navigation_toolbar_showing = item->isShown();
	}
	// Location
	else if (item == m_banner)
	{
		fe_globalPrefs.messages_location_toolbar_showing = item->isShown();
	}
}
//////////////////////////////////////////////////////////////////////////
void
XFE_ThreadFrame::configureToolbox()
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
		m_toolbar->setShowing(fe_globalPrefs.messages_navigation_toolbar_showing);
		m_toolbar->setOpen(fe_globalPrefs.messages_navigation_toolbar_open);
		m_toolbar->setPosition(fe_globalPrefs.messages_navigation_toolbar_position);
	}

	// Location
	if (m_banner)
	{
		m_banner->setShowing(fe_globalPrefs.messages_location_toolbar_showing);
		m_banner->setOpen(fe_globalPrefs.messages_location_toolbar_open);
		m_banner->setPosition(fe_globalPrefs.messages_location_toolbar_position);
	}
}


XP_Bool
XFE_ThreadFrame::isOkToClose()
{
   Boolean haveQueuedMail = False;
#ifdef USE_3PANE
   XFE_ThreadView *tview = (XFE_ThreadView*)(
			((XFE_ThreePaneView*)m_view)->getThreadView());
#else
   XFE_ThreadView *tview = (XFE_ThreadView*)m_view;
#endif

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

#ifdef CLEAN_UP_NEWS
    // This works, but is commented out because it's annoying:
    // MSG_CleanupNeeded() always returns true, even if you haven't
    // read news at all in this session, so the user has to sit
    // and wait for the cleanup dialog every time he exits.

#ifdef DEBUG_akkana
    printf("ThreadFrame: Checking whether cleanup is needed\n");
#endif

    // See if we need to clean up based on disk space prefs
    if (MSG_CleanupNeeded(tview->getMaster()))
    {
#ifdef DEBUG_akkana
        printf("\07Cleaning up\n");
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

XFE_CALLBACK_DEFN(XFE_ThreadFrame,showFolder)(XFE_NotificationCenter *,
                                 void *, void* cd)
{
  XP_Bool showFolder = (XP_Bool)((int32)cd);

  XP_ASSERT(m_banner!= NULL);

  printf("threadframe...showFolder\n");
  m_banner->setShowFolder(showFolder);

}

//////////////////////////////////////////////////////////////////////////
