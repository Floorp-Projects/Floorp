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
   MsgFrame.cpp -- Msg window stuff
   Created: Chris Toshok <toshok@netscape.com>, 29-Aug-96.
 */


#include "rosetta.h"
#include "MsgFrame.h"
#include "MsgView.h"
#include "ThreadView.h"
#include "AttachmentMenu.h"
#include "FolderMenu.h"
#include "Command.h"
#include "xpassert.h"
#include "MozillaApp.h"
#include "Dashboard.h"
#include "MailDownloadFrame.h"
#include "Xfe/Xfe.h"

#include "libi18n.h"
#include "libmime.h"
#include "intl_csi.h"
#include "felocale.h"


#include "xpgetstr.h"
extern int XFE_MESSAGE;
extern int XFE_MESSAGE_SUBTITLE;
extern int XFE_MN_UNREAD_AND_TOTAL;

extern "C" void fe_set_scrolled_default_size(MWContext *context);

MenuSpec XFE_MsgFrame::file_menu_spec[] = {
  { "newSubmenu", CASCADEBUTTON, (MenuSpec *) &XFE_ThreadFrame::new_submenu_spec },
  MENU_SEPARATOR,
  { "saveMsgAs", CASCADEBUTTON, (MenuSpec *) &XFE_ThreadFrame::save_submenu_spec },
  MENU_SEPARATOR,
  { xfeCmdRenameFolder,		PUSHBUTTON },
  { xfeCmdEmptyTrash,		PUSHBUTTON },
  { xfeCmdCompressAllFolders,	PUSHBUTTON },
  { xfeCmdCleanUpDisk,      PUSHBUTTON },
  MENU_SEPARATOR,
  { "newMsgSubmenu", CASCADEBUTTON, (MenuSpec *) &XFE_Frame::newMsg_submenu_spec },
  { xfeCmdSendMessagesInOutbox,	PUSHBUTTON },
  { xfeCmdAddNewsgroup,		    PUSHBUTTON },
  MENU_SEPARATOR,
  { xfeCmdPrint,		PUSHBUTTON },
  MENU_SEPARATOR,
  { xfeCmdClose,		PUSHBUTTON },
  { xfeCmdExit,			PUSHBUTTON },
  { NULL }
};

MenuSpec XFE_MsgFrame::offline_submenu_spec[] = {
  { xfeCmdGetSelectedMessagesForOffline, PUSHBUTTON },
  { xfeCmdGetFlaggedMessagesForOffline,	 PUSHBUTTON },
  { xfeCmdChooseMessagesForOffline,	 PUSHBUTTON },
  { NULL }
};

MenuSpec XFE_MsgFrame::edit_menu_spec[] = {
  { xfeCmdUndo,			PUSHBUTTON },
  { xfeCmdRedo,			PUSHBUTTON },
  MENU_SEPARATOR,
  { xfeCmdCut,			PUSHBUTTON },
  { xfeCmdCopy,			PUSHBUTTON },
  { xfeCmdPaste,		PUSHBUTTON },
  { xfeCmdDeleteMessage,	PUSHBUTTON },

  { "selectSubmenu",	CASCADEBUTTON, (MenuSpec *) &XFE_Frame::select_submenu_spec },
  MENU_SEPARATOR,
  { xfeCmdFindInObject,		PUSHBUTTON },
  { xfeCmdFindAgain,		PUSHBUTTON },
  { xfeCmdSearch,		PUSHBUTTON },
  { xfeCmdSearchAddress,	PUSHBUTTON },
  MENU_SEPARATOR,
  { xfeCmdEditMailFilterRules,PUSHBUTTON },
  { xfeCmdEditPreferences,	PUSHBUTTON },
  { NULL }
};

MenuSpec XFE_MsgFrame::view_menu_spec[] = {
{ "showSubmenu",            CASCADEBUTTON, (MenuSpec *) &XFE_ThreadFrame::show_submenu_spec },
  MENU_SEPARATOR,
  { "headersSubmenu",     CASCADEBUTTON, (MenuSpec *) &XFE_Frame::headers_submenu_spec },
  // This should just be a toggle.  -slamm
  { "attachmentsSubmenu", CASCADEBUTTON, (MenuSpec *) &XFE_Frame::attachments_submenu_spec },
  MENU_SEPARATOR,
  { xfeCmdIncreaseFont,		PUSHBUTTON },
  { xfeCmdDecreaseFont,		PUSHBUTTON },
  MENU_SEPARATOR,
  { xfeCmdReload,		    PUSHBUTTON },
  { xfeCmdShowImages,		PUSHBUTTON },
  { xfeCmdRefresh,          PUSHBUTTON },
  { xfeCmdStopLoading,		PUSHBUTTON },
  //{ xfeCmdStopAnimations,	PUSHBUTTON },
  MENU_SEPARATOR,
  { xfeCmdRot13Message,		PUSHBUTTON },
  { xfeCmdWrapLongLines,	TOGGLEBUTTON },
  MENU_SEPARATOR,
  { xfeCmdViewPageSource,	PUSHBUTTON },
  { xfeCmdViewPageInfo,		PUSHBUTTON },
  MENU_SEPARATOR,
  { "encodingSubmenu",      CASCADEBUTTON,(MenuSpec*)&XFE_Frame::encoding_menu_spec },
  { NULL }
};

MenuSpec XFE_MsgFrame::message_menu_spec[] = {
  { xfeCmdComposeMessage,		PUSHBUTTON },
  { "replySubmenu",	CASCADEBUTTON, (MenuSpec *) &XFE_Frame::reply_submenu_spec },
  { xfeCmdForwardMessage,		PUSHBUTTON },
  { xfeCmdForwardMessageQuoted,		PUSHBUTTON },
  { xfeCmdForwardMessageInLine, 	PUSHBUTTON },
  { xfeCmdEditMessage,			PUSHBUTTON },
  MENU_SEPARATOR,
  { "addToABSubmenu", CASCADEBUTTON, (MenuSpec *) &XFE_Frame::addrbk_submenu_spec },
  { "fileSubmenu",  DYNA_CASCADEBUTTON, NULL, NULL, 
	False, (void*)xfeCmdMoveMessage, XFE_FolderMenu::generate },
  { "copySubmenu",  DYNA_CASCADEBUTTON, NULL, NULL, 
	False, (void*)xfeCmdCopyMessage, XFE_FolderMenu::generate },
  MENU_SEPARATOR,
  { "markSubmenu",	  CASCADEBUTTON, (MenuSpec *) &XFE_ThreadFrame::mark_submenu_spec },
  { xfeCmdMarkMessage,			PUSHBUTTON },
  { xfeCmdUnflagMessage,		PUSHBUTTON },
  MENU_SEPARATOR,
  { xfeCmdIgnoreThread,         PUSHBUTTON },
  { xfeCmdWatchThread,          PUSHBUTTON },
  { NULL }
};

MenuSpec XFE_MsgFrame::go_menu_spec[] = {
  { xfeCmdNextMessage,			PUSHBUTTON },
  { xfeCmdNextUnreadMessage,		PUSHBUTTON },
  { xfeCmdNextFlaggedMessage,		PUSHBUTTON },
  { xfeCmdNextUnreadThread,		PUSHBUTTON },
  { xfeCmdNextCollection,		PUSHBUTTON },
  { xfeCmdNextUnreadCollection, 	PUSHBUTTON },
  MENU_SEPARATOR,
  { xfeCmdPreviousMessage,		PUSHBUTTON },
  { xfeCmdPreviousUnreadMessage,	PUSHBUTTON },
  { xfeCmdPreviousFlaggedMessage,	PUSHBUTTON },
  MENU_SEPARATOR,
  { xfeCmdFirstFlaggedMessage,		PUSHBUTTON },
  MENU_SEPARATOR,
  { xfeCmdBack,         PUSHBUTTON },
  { xfeCmdForward,		PUSHBUTTON },
  { NULL }
};

MenuSpec XFE_MsgFrame::menu_bar_spec[] = {
  { xfeMenuFile, 	CASCADEBUTTON, file_menu_spec },
  { xfeMenuEdit, 	CASCADEBUTTON, edit_menu_spec },
  { xfeMenuView, 	CASCADEBUTTON, view_menu_spec },
  { xfeMenuGo,	 	CASCADEBUTTON, XFE_ThreadFrame::go_menu_spec },
  { xfeMenuMessage, 	CASCADEBUTTON, message_menu_spec },
  { xfeMenuWindow,	CASCADEBUTTON, XFE_Frame::window_menu_spec },
  { xfeMenuHelp, 	CASCADEBUTTON, XFE_Frame::help_menu_spec },
  { NULL }
};

ToolbarSpec XFE_MsgFrame::toolbar_spec[] = {
	{ xfeCmdGetNewMessages,	PUSHBUTTON, &MNTB_GetMsg_group },
        {  // XX mail only
                xfeCmdComposeMessage,
                CASCADEBUTTON,
                &MNTB_Compose_group, NULL, NULL, NULL,                          // Icons
                compose_message_submenu_spec, 					// Submenu spec
                NULL, NULL, 							// Generate proc/arg
                XFE_TOOLBAR_DELAY_LONG 						// Popup delay
        },
        {  // XX newsgroup only
                xfeCmdComposeArticle,
                CASCADEBUTTON,
                &MNTB_Compose_group, NULL, NULL, NULL,                          // Icons
                compose_article_submenu_spec, 					// Submenu spec
                NULL, NULL, 							// Generate proc/arg
                XFE_TOOLBAR_DELAY_LONG 						// Popup delay
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
	{ xfeCmdCopyMessage, // XX news only
	  DYNA_CASCADEBUTTON, 
	  &MNTB_File_group, NULL, NULL, NULL, NULL,
	  XFE_FolderMenu::generate, (void*)xfeCmdCopyMessage,
	  XFE_TOOLBAR_DELAY_SHORT
	},
	{ xfeCmdMoveMessage, // XX mail only
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
	HG71611
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

XFE_MsgFrame::XFE_MsgFrame(Widget toplevel, XFE_Frame *parent_frame, Chrome *chromespec)
  : XFE_Frame("MailMsg", toplevel, parent_frame, FRAME_MAILNEWS_MSG, chromespec, True)
{
  INTL_CharSetInfo c = LO_GetDocumentCharacterSetInfo(m_context);
  INTL_SetCSIWinCSID(c, 
	    INTL_DocToWinCharSetID (CONTEXT_DATA (m_context)->xfe_doc_csid));

  geometryPrefName = "mail.msg";

  // create the msg view
  XFE_MsgView *mview;
 
  mview = new XFE_MsgView(this, getChromeParent(), NULL, m_context);
  mview->registerInterest(XFE_MsgView::spacebarAtMsgBottom,
			  this,
			  (XFE_FunctionNotification)spaceAtMsgEnd_cb);

  mview->registerInterest(XFE_MsgView::messageHasChanged,
			  this,
			  (XFE_FunctionNotification)msgLoaded_cb);

  mview->registerInterest(XFE_MsgView::lastMsgDeleted,
			  this,
			  (XFE_FunctionNotification)msgDeleted_cb);

  m_banner = new XFE_MNBanner(this, m_toolbox);

  setView(mview);

  setMenubar(menu_bar_spec);
  setToolbar(toolbar_spec);

  fe_set_scrolled_default_size(m_context);

//  m_banner->show();
  mview->show();

  m_banner->setTitle( XP_GetString ( XFE_MESSAGE ));
  m_banner->setSubtitle( "" );
  m_banner->setMommyIcon(&MN_Mommy_group);

  // safe defaults.  They will be overridden when we update the banner.

  // Configure the dashboard
  XP_ASSERT( m_dashboard != NULL );

  HG11111
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

  XFE_MozillaApp::theApp()->registerInterest(XFE_MNView::msgWasDeleted,
											 this,
											 (XFE_FunctionNotification)msgDeleted_cb);

  XFE_MozillaApp::theApp()->registerInterest(XFE_MNView::folderDeleted,
											 this,
											 (XFE_FunctionNotification)folderDeleted_cb);
}

XFE_MsgFrame::~XFE_MsgFrame()
{
  XFE_MozillaApp::theApp()->unregisterInterest(XFE_MNView::MNChromeNeedsUpdating,
											   this,
											   (XFE_FunctionNotification)MNChromeUpdate_cb);
  
  XFE_MozillaApp::theApp()->unregisterInterest(XFE_MNView::folderChromeNeedsUpdating,
											   this,
											   (XFE_FunctionNotification)FolderChromeUpdate_cb);

  XFE_MozillaApp::theApp()->unregisterInterest(XFE_MNView::msgWasDeleted,
											   this,
											   (XFE_FunctionNotification)msgDeleted_cb);

  XFE_MozillaApp::theApp()->unregisterInterest(XFE_MNView::folderDeleted,
											   this,
											   (XFE_FunctionNotification)folderDeleted_cb);
}

XP_Bool
XFE_MsgFrame::isCommandEnabled(CommandType cmd,
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
XFE_MsgFrame::doCommand(CommandType cmd,
                            void *calldata, XFE_CommandInfo* info)
{
	if (cmd == xfeCmdToggleLocationToolbar)
    {
		if (m_banner)
        {
			// Toggle the showing state
			m_banner->toggleShowingState();
			
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
                XFE_MsgView *tview = (XFE_MsgView*)m_view;

                tview->doCommand(cmd, calldata,info);  

        }

	else
		XFE_Frame::doCommand(cmd, calldata, info);
}

XP_Bool
XFE_MsgFrame::handlesCommand(CommandType cmd,
				 void *calldata, XFE_CommandInfo*)
{
	if (cmd == xfeCmdToggleLocationToolbar)
		return True;
	else
		return XFE_Frame::handlesCommand(cmd, calldata);
}

char *
XFE_MsgFrame::commandToString(CommandType cmd,
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

void XFE_MsgFrame::setButtonsByContext(MWContextType cxType)
{
    if (cxType == MWContextNewsMsg || cxType == MWContextNews)
    {
        m_toolbar->hideButton(xfeCmdComposeMessage, PUSHBUTTON);
        m_toolbar->hideButton(xfeCmdDeleteMessage, PUSHBUTTON);
        m_toolbar->hideButton(xfeCmdReplyToSender, CASCADEBUTTON);
        m_toolbar->hideButton(xfeCmdMoveMessage, CASCADEBUTTON);

        m_toolbar->showButton(xfeCmdMarkMessageRead, CASCADEBUTTON);
        m_toolbar->showButton(xfeCmdComposeArticle, PUSHBUTTON);
        m_toolbar->showButton(xfeCmdReplyToNewsgroup, CASCADEBUTTON);
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

void
XFE_MsgFrame::allConnectionsComplete(MWContext  *context)
{
	XFE_Frame::allConnectionsComplete(context);
	updateReadAndTotalCounts();
}

void
XFE_MsgFrame::loadMessage(MSG_FolderInfo *folder_info,
						  MessageKey msg_key)
{
  XFE_MsgView *mview = (XFE_MsgView*)m_view;
  MSG_FolderLine folderline;

  if (MSG_GetFolderLineById(XFE_MNView::getMaster(),
							folder_info, &folderline))
	  {
		  if (folderline.flags & MSG_FOLDER_FLAG_NEWSGROUP)
              setButtonsByContext(MWContextNewsMsg);
		  else
              setButtonsByContext(MWContextMailMsg);
	  }

  mview->loadMessage(folder_info, msg_key);
}

MSG_Pane *
XFE_MsgFrame::getPane()
{
  XFE_MsgView *mview = (XFE_MsgView*)m_view;

  return mview->getPane();
}

MSG_FolderInfo *
XFE_MsgFrame::getFolderInfo()
{
  XFE_MsgView *mview = (XFE_MsgView*)m_view;

  return mview->getFolderInfo();
}

MessageKey
XFE_MsgFrame::getMessageKey()
{
  XFE_MsgView *mview = (XFE_MsgView*)m_view;

  return mview->getMessageKey();
}

XFE_CALLBACK_DEFN(XFE_MsgFrame, spaceAtMsgEnd)(XFE_NotificationCenter*,
					       void *, void *)
{
  if (m_view->isCommandEnabled(xfeCmdNextUnreadMessage))
    m_view->doCommand(xfeCmdNextUnreadMessage);
}

XFE_CALLBACK_DEFN(XFE_MsgFrame, msgLoaded)(XFE_NotificationCenter*,
                                           void*, void*)
{
  MSG_FolderInfo *folder_info;
  MessageKey msg_key;
  MSG_MessageLine threadline;
  MSG_FolderLine folderline;
  MSG_ViewIndex index;
  XFE_MsgView *mview = (XFE_MsgView*)m_view;

  updateReadAndTotalCounts();

  MSG_GetCurMessage(mview->getPane(), &folder_info, &msg_key, &index);

  if (MSG_GetThreadLineById(mview->getPane(),
                            msg_key, &threadline))
    {
      char subtitle[1024];
      char *foldername = "";
	  char *subject;
	  char *author;

      m_banner->setTitle( XP_GetString ( XFE_MESSAGE ));

      if (MSG_GetFolderLineById(XFE_MNView::getMaster(),
                                folder_info, &folderline))
        {
			char buf[200];

			foldername = (char*)folderline.name;
			
			PR_snprintf(buf, sizeof(buf), XP_GetString(XFE_MN_UNREAD_AND_TOTAL), folderline.unseen, folderline.total);
			
			m_banner->setInfo(buf);
        }
         
      INTL_DECODE_MIME_PART_II(subject, threadline.subject, fe_LocaleCharSetID, FALSE);
      INTL_DECODE_MIME_PART_II(author, threadline.author, fe_LocaleCharSetID, FALSE);
      PR_snprintf(subtitle, sizeof(subtitle),
                  XP_GetString( XFE_MESSAGE_SUBTITLE ),
                  subject,
                  author,
                  foldername);

	  XP_FREE(subject);
	  XP_FREE(author);
      m_banner->setSubtitle(subtitle);
      notifyInterested(XFE_View::chromeNeedsUpdating);
    }
}

void
XFE_MsgFrame::updateReadAndTotalCounts()
{
	MSG_FolderLine folderline;
	MSG_FolderInfo *folder_info;
	MessageKey msg_key;
	MSG_ViewIndex index;
	XFE_MsgView *mview = (XFE_MsgView*)m_view;

	MSG_GetCurMessage(mview->getPane(), &folder_info, &msg_key, &index);

	if (folder_info)
		{
			if (MSG_GetFolderLineById(XFE_MNView::getMaster(),
									  folder_info, &folderline))
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
	
XFE_CALLBACK_DEFN(XFE_MsgFrame, MNChromeUpdate)(XFE_NotificationCenter*,
												void*, void*)
{
	notifyInterested(XFE_View::chromeNeedsUpdating);

	updateReadAndTotalCounts();
}

XFE_CALLBACK_DEFN(XFE_MsgFrame, FolderChromeUpdate)(XFE_NotificationCenter*,
													void*, void *cd)
{
	MSG_FolderInfo *info = (MSG_FolderInfo*)cd;

	if (info == getFolderInfo())
		{
			notifyInterested(XFE_View::chromeNeedsUpdating);

			updateReadAndTotalCounts();
		}
}

XFE_CALLBACK_DEFN(XFE_MsgFrame, msgDeleted)(XFE_NotificationCenter*,
					       void *, void *cdata)
{
	/* Need to check if this folder ?
	 */
	MessageKey id = (MessageKey) cdata;
	if (id == getMessageKey())
		hide();
}

XFE_CALLBACK_DEFN(XFE_MsgFrame, folderDeleted)(XFE_NotificationCenter*,
					       void *, void *cdata)
{
	
	/* Need to check if this folder ?
	 */
	MSG_FolderInfo *info = (MSG_FolderInfo *) cdata;
	if (info == getFolderInfo())
		delete_response();
}

XFE_MsgFrame *
XFE_MsgFrame::frameForMessage(MSG_FolderInfo *info, MessageKey key)
{
  XP_List *msg_frame_list = XFE_MozillaApp::theApp()->getFrameList( FRAME_MAILNEWS_MSG );
  XP_List *current_list_entry;

  for (current_list_entry = msg_frame_list;
       current_list_entry != NULL;
       current_list_entry = current_list_entry->next)
    {
      if (current_list_entry->object
	  && ((XFE_Frame*)current_list_entry->object)->getType() == FRAME_MAILNEWS_MSG // sanity check
	  && ((XFE_MsgFrame*)current_list_entry->object)->getFolderInfo() == info
	  && ((XFE_MsgFrame*)current_list_entry->object)->getMessageKey() == key)
	{
	  return (XFE_MsgFrame*)current_list_entry->object;
	}
    }

  return NULL;
}

MWContext*
fe_showMsg(Widget toplevel,
		   XFE_Frame *parent_frame,
		   Chrome *chromespec,
		   MSG_FolderInfo *folder_info,
		   MessageKey msg_key,
		   XP_Bool with_reuse)
{

	XFE_MsgFrame *theFrame = NULL;

	if ( with_reuse )
		{
			XP_List *msg_frame_list = XFE_MozillaApp::theApp()->getFrameList( FRAME_MAILNEWS_MSG );
			
			if (!XP_ListIsEmpty(msg_frame_list))
				{
					theFrame = (XFE_MsgFrame*)XP_ListTopObject( msg_frame_list );
				}
			
			if (theFrame)
				{
					theFrame->show();
					theFrame->loadMessage( folder_info, msg_key );
					return theFrame->getContext();
				}
		}
	else
		{
			theFrame = XFE_MsgFrame::frameForMessage( folder_info, msg_key );
			
			if (theFrame)
				{
					theFrame->show();
					return theFrame->getContext();
				}
		}
	
	// if we get here, either we were trying to reuse and there wasn't a
	// messageframe at all, or we were not trying to reuse and just looking for
	// for the messageframe displaying this message already and failed to find it.
	// At any rate, we need to pop up another window.
	theFrame = new XFE_MsgFrame(toplevel, parent_frame, chromespec);
	theFrame->show();
	theFrame->loadMessage( folder_info, msg_key );
	
	return theFrame->getContext();
}

int
XFE_MsgFrame::getSecurityStatus()
{
 XFE_MailSecurityStatusType status = XFE_UNSECURE_UNSIGNED;
 HG98200
 return status;
}

//////////////////////////////////////////////////////////////////////////
//
// Toolbox methods
//
//////////////////////////////////////////////////////////////////////////
void
XFE_MsgFrame::toolboxItemSnap(XFE_ToolboxItem * item)
{
	XP_ASSERT( item == m_toolbar || item == m_banner );

	// Navigation
	fe_globalPrefs.messenger_navigation_toolbar_position = m_toolbar->getPosition();

	// Location
	fe_globalPrefs.messenger_location_toolbar_position = m_banner->getPosition();
}
//////////////////////////////////////////////////////////////////////////
void
XFE_MsgFrame::toolboxItemClose(XFE_ToolboxItem * item)
{
	XP_ASSERT( item != NULL );

	// Navigation
	if (item == m_toolbar)
	{
		fe_globalPrefs.messenger_navigation_toolbar_open = False;
	}
	// Location
	else if (item == m_banner)
	{
		fe_globalPrefs.messenger_location_toolbar_open = False;
	}
}
//////////////////////////////////////////////////////////////////////////
void
XFE_MsgFrame::toolboxItemOpen(XFE_ToolboxItem * item)
{
	XP_ASSERT( item != NULL );

	// Navigation
	if (item == m_toolbar)
	{
		fe_globalPrefs.messenger_navigation_toolbar_open = True;
	}
	// Location
	else if (item == m_banner)
	{
		fe_globalPrefs.messenger_location_toolbar_open = True;
	}
}
//////////////////////////////////////////////////////////////////////////
void
XFE_MsgFrame::toolboxItemChangeShowing(XFE_ToolboxItem * item)
{
	XP_ASSERT( item != NULL );

	// Navigation
	if (item == m_toolbar)
	{
		fe_globalPrefs.messenger_navigation_toolbar_showing = item->isShown();
	}
	// Location
	else if (item == m_banner)
	{
		fe_globalPrefs.messenger_location_toolbar_showing = item->isShown();
	}
}
//////////////////////////////////////////////////////////////////////////
void
XFE_MsgFrame::configureToolbox()
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
		m_toolbar->setShowing(fe_globalPrefs.messenger_navigation_toolbar_showing);
		m_toolbar->setOpen(fe_globalPrefs.messenger_navigation_toolbar_open);
		m_toolbar->setPosition(fe_globalPrefs.messenger_navigation_toolbar_position);
	}

	// Location
	if (m_banner)
	{
		m_banner->setShowing(fe_globalPrefs.messenger_location_toolbar_showing);
		m_banner->setOpen(fe_globalPrefs.messenger_location_toolbar_open);
		m_banner->setPosition(fe_globalPrefs.messenger_location_toolbar_position);
	}
}
extern int XFE_SEND_UNSENTMAIL;

XP_Bool 
XFE_MsgFrame::isOkToClose()
{
   Boolean haveQueuedMail = False;
   XFE_MsgView *tview = (XFE_MsgView*)m_view;

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


        // See if we need to clean up based on disk space prefs
        if (MSG_CleanupNeeded(tview->getMaster()))
        {
            // create progress pane
            XFE_MailDownloadFrame* progressFrame = 
                new XFE_MailDownloadFrame(XfeAncestorFindApplicationShell(getBaseWidget()),
                                          this,
                                          tview->getPane());
            progressFrame->cleanUpNews();
            return True;
        }
    }
    return True;

}

//////////////////////////////////////////////////////////////////////////
