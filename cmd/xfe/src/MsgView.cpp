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
   MsgView.cpp -- class for displaying the actual text (well, html) of a message.
   Created: Chris Toshok <toshok@netscape.com>, 7-Aug-96.
   */



#include "MsgView.h"
#include "FolderMenu.h"
#include "xpassert.h"
#include "xfe.h"
#include "msgcom.h"
#include "xpgetstr.h"
#include "prefapi.h"

#include "ViewGlue.h"
#include "MNSearchFrame.h"
#include "ThreadFrame.h"
#include "ThreadView.h"
#include "ThreePaneView.h"
#include "MozillaApp.h"	// to get the MsgFrame
#include "Xfe/Xfe.h"
#include <intl_csi.h>       /* to get/set doc_csid/win_csid */

#include "prefs.h"

#include <Xm/Label.h>

#ifdef DEBUG_toshok
#define D(x) x
#else
#define D(x)
#endif

extern int MK_MSG_CANCEL_MESSAGE;

extern "C" 
{
	void fe_print_cb (Widget widget, XtPointer closure, XtPointer call_data);
}

MenuSpec XFE_MsgView::separator_spec[] = {
	MENU_SEPARATOR,
	{ NULL }
};

MenuSpec XFE_MsgView::repl_spec[] = {
	// Setting the call data to non-null is a signal to commandToString
	// not to reset the menu string
	{ xfeCmdReplyToSender,    PUSHBUTTON, NULL, NULL, NULL, "reply-to-sender" },
	{ xfeCmdReplyToAll,               PUSHBUTTON, NULL, NULL, NULL, "reply-to-all" },
	{ xfeCmdForwardMessage,	PUSHBUTTON },
	{ xfeCmdForwardMessageQuoted,	PUSHBUTTON },
	{ NULL }
};

MenuSpec XFE_MsgView::addr_spec[] = {
	{ "addToAddrBkSubmenu", CASCADEBUTTON, (MenuSpec*)&addToAddrbk_submenu_spec },
	{ NULL }
};

MenuSpec XFE_MsgView::filemsg_spec[] = {
  { "fileSubmenu",              DYNA_CASCADEBUTTON, NULL, NULL, False, (void*)xfeCmdMoveMessage, XFE_FolderMenu::generate },
  { xfeCmdDeleteMessage,	PUSHBUTTON },
  { xfeCmdSaveMessagesAs,	PUSHBUTTON },
  { xfeCmdPrint,		PUSHBUTTON },
  { NULL }
};

MenuSpec XFE_MsgView::openLinkNew_spec[] = {
  { xfeCmdOpenLinkNew , PUSHBUTTON },
  { NULL },
};
MenuSpec XFE_MsgView::openLinkEdit_spec[] = {
  { xfeCmdOpenLinkEdit, PUSHBUTTON },
  { NULL },
};
MenuSpec XFE_MsgView::showImage_spec[] = {
  { xfeCmdShowImage, PUSHBUTTON },
  { NULL },
};
MenuSpec XFE_MsgView::stopLoading_spec[] = {
  { xfeCmdStopLoading, PUSHBUTTON },
  { NULL },
};
MenuSpec XFE_MsgView::openImage_spec[] = {
  { xfeCmdOpenImage, PUSHBUTTON },
  { NULL },
};
MenuSpec XFE_MsgView::saveLink_spec[] = {
  { xfeCmdSaveLink, PUSHBUTTON },
  { NULL },
};
MenuSpec XFE_MsgView::saveImage_spec[] = {
  { xfeCmdSaveImage, PUSHBUTTON },
  { NULL },
};
MenuSpec XFE_MsgView::copy_spec[] = {
  { xfeCmdCopy, PUSHBUTTON },
  { NULL },
};
MenuSpec XFE_MsgView::copyLink_spec[] = {
  { xfeCmdCopyLink, PUSHBUTTON },
  { NULL },
};
MenuSpec XFE_MsgView::copyImage_spec[] = {
  { xfeCmdCopyImage, PUSHBUTTON },
  { NULL },
};


MenuSpec XFE_MsgView::addToAddrbk_submenu_spec[] = {
    { xfeCmdAddSenderToAddressBook, PUSHBUTTON },
    { xfeCmdAddAllToAddressBook, PUSHBUTTON },
    { NULL }
};

const char *XFE_MsgView::messageHasChanged = "XFE_MsgView::messageHasChanged";
const char *XFE_MsgView::lastMsgDeleted = "XFE_MsgView::lastMsgDeleted";
const char *XFE_MsgView::spacebarAtMsgBottom = "XFE_MsgView::spacebarAtMsgBottom";

XFE_MsgView::XFE_MsgView(XFE_Component *toplevel_component,
			 Widget parent,
			 XFE_View *parent_view,
			 MWContext *context,
			 MSG_Pane *p)
  : XFE_MNView(toplevel_component, parent_view, context, p)
{

  m_frameDeleted = False;
  m_updateThread = False;

  // create common parent for pane and attachment panel
  Widget panedW=XmCreatePanedWindow(parent,"msgViewPane",NULL,0);
  XtVaSetValues(panedW,
                XmNshadowThickness,0,
                XmNmarginWidth,0,
                XmNmarginHeight,0,
                NULL);


  m_htmlView = new XFE_HTMLView(toplevel_component, panedW, this, m_contextData);
  addView(m_htmlView);

  // create attachment panel
  m_attachApplHeight=0;
  m_attachUserHeight=0;
  m_attachPanel = new XFE_ReadAttachPanel(m_contextData);
  m_attachPanel->createWidgets(panedW);
  XtVaSetValues(m_attachPanel->getBaseWidget(),XmNskipAdjust,TRUE,NULL);
  m_attachPanel->setPaneHeight(1);

  /* test frame type
   */
  XFE_Frame *frame = (XFE_Frame*)m_toplevel;
  if (frame &&
	  FRAME_MAILNEWS_MSG == frame->getType()) {
	  /* standalone frame
	   */		  
	  frame->registerInterest(XFE_Frame::allConnectionsCompleteCallback,
								this,
								(XFE_FunctionNotification)allConnectionsComplete_cb);
  }/* if */

  m_htmlView->registerInterest(XFE_HTMLView::spacebarAtPageBottom,
							   this,
							   (XFE_FunctionNotification)spaceAtMsgEnd_cb);

  m_htmlView->registerInterest(XFE_HTMLView::popupNeedsShowing,
							   this,
							   (XFE_FunctionNotification)showPopup_cb);

  XFE_MozillaApp::theApp()->registerInterest(XFE_MozillaApp::refreshMsgWindow,
											 this,
											 (XFE_FunctionNotification)refresh_cb);

  if (!p)
    setPane(MSG_CreateMessagePane(m_contextData,
				  XFE_MNView::getMaster()));

  m_folderInfo = NULL;
  m_messageKey = MSG_MESSAGEKEYNONE;

  m_popup = NULL;

  m_htmlView->show();

  setBaseWidget(panedW);
}

XFE_MsgView::~XFE_MsgView()
{

  if (m_attachPanel) {
    delete m_attachPanel;
    m_attachPanel=NULL;
  }

  if (m_popup)
	  delete m_popup;

  /* unregister from frame
   */
  /* test frame type
   */
  XFE_Frame *frame = (XFE_Frame*)m_toplevel;
  if (frame &&
	  FRAME_MAILNEWS_MSG == frame->getType()) {
	  /* standalone frame
	   */		  
	  frame->unregisterInterest(XFE_Frame::allConnectionsCompleteCallback,
								  this,
								  (XFE_FunctionNotification)allConnectionsComplete_cb);
  }/* if */

  m_htmlView->unregisterInterest(XFE_HTMLView::spacebarAtPageBottom,
								 this,
								 (XFE_FunctionNotification)spaceAtMsgEnd_cb);
  
  m_htmlView->unregisterInterest(XFE_HTMLView::popupNeedsShowing,
								 this,
								 (XFE_FunctionNotification)showPopup_cb);

  XFE_MozillaApp::theApp()->unregisterInterest(XFE_MozillaApp::refreshMsgWindow,
											   this,
											   (XFE_FunctionNotification)refresh_cb);
  
  destroyPane();

  // XFE_View destroys m_htmlView for us.
}

XFE_CALLBACK_DEFN(XFE_MsgView, allConnectionsComplete)(XFE_NotificationCenter *,
													   void *,
													   void *)
{
	/* 1. check if deleted/filed
	 */
	if (m_updateThread) {
		/* 2. get threadframe with 
		 *    XFE_ThreadFrame::frameForInfo(MSG_FolderInfo *info)
		 */
		MSG_FolderInfo *folderInfo = MSG_GetCurFolder(m_pane);
		XFE_ThreadFrame *tFrame = 
			XFE_ThreadFrame::frameForInfo(folderInfo);
		if (tFrame) {
			/* 3. urge to update msg line and body
			 */
#ifdef USE_3PANE
			XFE_ThreadView *tview = 0;
            XFE_ThreePaneView* tpview = (XFE_ThreePaneView*)tFrame->getView();
            if (tpview)
                tview = (XFE_ThreadView*)tpview->getThreadView();
#else
			XFE_ThreadView *tview = 
				(XFE_ThreadView *) tFrame->getView();
#endif
            if (tview)
				tview->processCmdQueue();
		}/* if tFrame */
		m_updateThread = False;
	}/* if */
}

void
XFE_MsgView::loadMessage(MSG_FolderInfo *info,
			 MessageKey key)
{

  MSG_FolderLine folderLine;

  m_folderInfo = info;
  m_messageKey = key;

  MSG_GetFolderLineById(XFE_MNView::getMaster(), m_folderInfo, &folderLine);
  
  if (folderLine.flags & MSG_FOLDER_FLAG_NEWSGROUP
	  || folderLine.flags & MSG_FOLDER_FLAG_CATEGORY)
	  m_displayingNewsgroup = TRUE;
  else
	  m_displayingNewsgroup = FALSE;


  MSG_LoadMessage(m_pane, info, MSG_MESSAGEKEYNONE);

  // set up attachment notify callback
  static MSG_MessagePaneCallbacks attachmentCb = {
      XFE_MsgView::AttachmentCountCb,
      XFE_MsgView::ToggleAttachmentPanelCb
  };
  MSG_SetMessagePaneCallbacks(m_pane,&attachmentCb,(void*)this);

  // remove previous message's attachments before displaying next message
  if (m_attachPanel) {
      m_attachPanel->removeAllAttachments();
      m_attachPanel->setPaneHeight(1);
  }

  // reset panel height memory for next message
  m_attachApplHeight=0;
  m_attachUserHeight=0;

  MSG_LoadMessage(m_pane, info, key);

}


// called as each attachment is processed
void
XFE_MsgView::AttachmentCountCb(MSG_Pane*,void *closure,int32 count,XP_Bool done)
{
  if (closure) {
    XFE_MsgView *m=(XFE_MsgView*)closure;
    m->attachmentCountCb(count,done);
  }
}


// show panel as soon as we detect attachments, display attachment icons when
// all attachments have been loaded.
void
XFE_MsgView::attachmentCountCb(int32 count,XP_Bool done)
{
  if (count>0) {
    // Prepare to display attachment panel if there is at least 1 attachment
    // The panel will be shown as soon as we detect attachments, but will fill
    // up only when all attachments are loaded. Gives user early-warning of
    // attachments. If we held off managing panel until all attachments
    // are loaded, it will cause unexpected relayout after user has started
    // reading body of message.
    
    // erase previous message attachments
    m_attachPanel->removeAllAttachments();

    // add icons only when all attachments have been loaded
    if (done) {
      MSG_AttachmentData* data;
      done = FALSE;
      MSG_GetViewedAttachments(m_pane, &data, &done);
      XP_ASSERT(done);
      if (data) {
          m_attachPanel->addAttachments(m_pane,data);
          m_attachPanel->updateDisplay();
      }

      // if user has tried to resize panel blindly, then
      // expand it to the preferred height. This ignores
      // their guess, but they were probably wrong anyway,
      // and should have been more patient. Yeah.
      m_attachApplHeight=0;
      m_attachUserHeight=0;
      if (m_attachPanel->getPaneHeight()<10) {
          // panel effectively closed. make it completely closed for neatness.
          m_attachPanel->setPaneHeight(1);
      }
      else {
          // panel is open, reset height to ideal
          setAttachPrefHeight();
      }
    }

    // manage attachment panel
    m_attachPanel->show();
    
  }
  else {
    // count==0 - either load is in-progress, or there are no attachments
    if (done) {
        // definitely no attachments - hide attachment panel and
        // remove old attachment data
        if (m_attachPanel) {
            m_attachPanel->hide();
            m_attachPanel->removeAllAttachments();
        }
    }
  }
}

// called when user clicks attachment icon in message header
void
XFE_MsgView::ToggleAttachmentPanelCb(MSG_Pane*,void *closure)
{
  if (closure) {
    XFE_MsgView *m=(XFE_MsgView*)closure;
    m->toggleAttachmentPanelCb();
  }
}

void
XFE_MsgView::toggleAttachmentPanelCb()
{
    if (!m_attachPanel)
        return;

    int currentHeight=m_attachPanel->getPaneHeight();
    
    if (currentHeight<10) {
        // expand attachment panel
        // if user hasn't set size, choose size to show all attachments
        if (!m_attachUserHeight)
            setAttachPrefHeight();
        else
            m_attachPanel->setPaneHeight(m_attachUserHeight);
    }
    else {
        // contract attachment panel

        // if user changed pane size, remember it
        if (!m_attachApplHeight || currentHeight!=m_attachApplHeight)
            m_attachUserHeight=currentHeight;
        
        m_attachPanel->setPaneHeight(1);
    }
}


void
XFE_MsgView::setAttachPrefHeight()
{
    if (!m_attachPanel || !getBaseWidget())
        return;

    // pick a default height for the attachment panel.
    // Make large enough to view all attachments without
    // scrolling, up to a maximum of 70% of the message
    // pane height - don't want to obscure the attachment
    // show/hide link in the message header.
    
    Dimension parentHeight;
    XtVaGetValues(getBaseWidget(),XmNheight,&parentHeight,NULL);

    int prefHeight=m_attachPanel->getPreferredHeight();

    if (prefHeight > (7*parentHeight/10))
        prefHeight=(7*parentHeight/10);
    
    m_attachApplHeight=prefHeight;
    m_attachPanel->setPaneHeight(m_attachApplHeight);
}

MSG_FolderInfo *
XFE_MsgView::getFolderInfo()
{
  return m_folderInfo;
}

MessageKey
XFE_MsgView::getMessageKey()
{
  return m_messageKey;
}

void
XFE_MsgView::paneChanged(XP_Bool asynchronous,
			 MSG_PANE_CHANGED_NOTIFY_CODE code,
			 int32 value)
{
  switch (code)
    {
	case MSG_PaneNotifyFolderDeleted:
		{
			/* test frame type
			 */
			XFE_Frame *frame = (XFE_Frame*)m_toplevel;
			if (frame &&
				FRAME_MAILNEWS_MSG == frame->getType()) {
				/* standalone frame
				 */	
				if (!m_frameDeleted) {
					frame->delete_response();
					m_frameDeleted = True;
				}/* if */
				
			}/* if */
		}
		/* shall we update banner or simply return? NO
		 */
		return;

    case MSG_PaneNotifyMessageLoaded:
		notifyInterested(messageHasChanged, (void*)value);

		{
			MSG_FolderInfo *folderInfo = MSG_GetCurFolder(m_pane);

			/* We need to update this to reflect what the real folderInfo is
			 */
			m_folderInfo = folderInfo;

			MessageKey key = (MessageKey) value;
			
			if (MSG_GetBacktrackState(m_pane) == MSG_BacktrackIdle)
				MSG_AddBacktrackMessage(m_pane, folderInfo, key);
			else
				MSG_SetBacktrackState(m_pane, MSG_BacktrackIdle);
		}
		break;
	case MSG_PaneNotifyLastMessageDeleted:
		notifyInterested(lastMsgDeleted, (void*)value);
		break;
	default:
		break;
    }

  XFE_MNView::paneChanged(asynchronous, code, value);
}

Boolean
XFE_MsgView::isCommandSelected( CommandType cmd, void *calldata, XFE_CommandInfo*)
{
  MSG_CommandType msg_cmd;
  XP_Bool selectable = False;;

  msg_cmd = commandToMsgCmd(cmd);
      
  if (msg_cmd != (MSG_CommandType)~0)
  {
	  
     if ( MSG_GetToggleStatus(m_pane, msg_cmd, NULL, 0 ) == MSG_Checked) 
        selectable = True;
     return (Boolean)selectable;
  }
  return XFE_MNView::isCommandSelected(cmd, calldata);
}

Boolean 
XFE_MsgView::isCommandEnabled(CommandType cmd, void *calldata, XFE_CommandInfo*)
{
#define IS_CMD(command) cmd == (command)

  XP_Bool selectable = FALSE;

    // DeleteMessage stands for CancelMessage in the ThreadView,
    // so intercept it early:
    if ( (IS_CMD(xfeCmdDeleteMessage) || IS_CMD(xfeCmdCancelMessages))
         && isDisplayingNews() )
    {
      MSG_CommandStatus(m_pane, MSG_CancelMessage,
                        NULL, 0,
                        &selectable, NULL, NULL, NULL);
      return selectable;
    }

  MSG_MotionType nav_cmd;
  MSG_CommandType msg_cmd;

  nav_cmd = commandToMsgNav(cmd);

  if (IS_CMD(xfeCmdSetPriorityHighest)
      || IS_CMD(xfeCmdSetPriorityHigh)
      || IS_CMD(xfeCmdSetPriorityNormal)
      || IS_CMD(xfeCmdSetPriorityLow)
      || IS_CMD(xfeCmdSetPriorityLowest)
      || IS_CMD(xfeCmdSetPriorityNone)
      || IS_CMD(xfeCmdPrint)
	  || IS_CMD(xfeCmdCopyMessage)
	  || IS_CMD(xfeCmdMoveMessage)
      || IS_CMD(xfeCmdIgnoreThread)
      || IS_CMD(xfeCmdWatchThread)
	  || IS_CMD(xfeCmdMarkMessageByDate))
    {
      MSG_ViewIndex index;
      MessageKey key;
      MSG_FolderInfo *info;
      
      MSG_GetCurMessage(m_pane, &info, &key, &index);

      return (key != MSG_MESSAGEKEYNONE);
    }
  else if (IS_CMD(xfeCmdMommy)
      || IS_CMD(xfeCmdNewFolder)
      || IS_CMD(xfeCmdRenameFolder)
      || IS_CMD(xfeCmdEmptyTrash)
      || IS_CMD(xfeCmdAddNewsgroup)
      || IS_CMD(xfeCmdUpdateMessageCount)
      || IS_CMD(xfeCmdPrintSetup)
      || IS_CMD(xfeCmdPrintPreview))

	  {
		  return True;
	  }
  else if (IS_CMD(xfeCmdEditPreferences) ||
           IS_CMD(xfeCmdWrapLongLines) )
    {
      return True;
    }
  else if ( IS_CMD(xfeCmdEditConfiguration) )
  {
     MSG_ViewIndex index ;
     MessageKey key;
     MSG_FolderInfo *info;
     XP_Bool selectable = FALSE;
     MSG_CommandType msg_cmd = commandToMsgCmd(cmd);


        MSG_GetCurMessage(m_pane, &info, &key, &index);
        MSG_CommandStatus(m_pane, msg_cmd,
                        &index, 1,
                        &selectable, NULL, NULL, NULL);

        return (selectable && !(isDisplayingNews()) );
  }
  else if ( IS_CMD(xfeCmdModerateDiscussion) )
  {
     MSG_ViewIndex index ;
     MessageKey key;
     MSG_FolderInfo *info;
     XP_Bool selectable = FALSE;
     MSG_CommandType msg_cmd = commandToMsgCmd(cmd);


        MSG_GetCurMessage(m_pane, &info, &key, &index);
        MSG_CommandStatus(m_pane, msg_cmd,
                        &index, 1,
                        &selectable, NULL, NULL, NULL);

        return (selectable && (isDisplayingNews()) );
  }
  else if (IS_CMD(xfeCmdGetNextNNewMsgs))
	  {
		  return m_displayingNewsgroup;
	  }
#ifdef DEBUG_akkana
  else if (IS_CMD(xfeCmdEditMessage))
	  {
		  return !m_displayingNewsgroup;
	  }
#endif
  else if (nav_cmd != (MSG_MotionType)~0)
    {
      MSG_ViewIndex index;
      MessageKey key;
      MSG_FolderInfo *info;
      
      MSG_GetCurMessage(m_pane, &info, &key, &index);
      
      if (MSG_NavigateStatus(m_pane, nav_cmd, index, &selectable, NULL) < 0)
		  selectable = FALSE;
      
      return selectable;
    }
  else 
    {
      msg_cmd = commandToMsgCmd(cmd);
      
      if (msg_cmd != (MSG_CommandType)~0)
	{
	  
		if (IS_CMD(xfeCmdGetNewMessages)) {
			int num_inboxes = MSG_GetFoldersWithFlag(XFE_MNView::getMaster(),
													 MSG_FOLDER_FLAG_INBOX,
													 NULL, 0);
			if ((num_inboxes > 0) ||
				(MSG_CommandStatus(m_pane, 
								   msg_cmd, NULL, 0, 
								   &selectable, NULL, NULL, NULL) < 0)) {
				selectable = FALSE;
			}/* if */
		}/* else */
		else if (MSG_CommandStatus(m_pane, msg_cmd, NULL, 0, &selectable, NULL, NULL, NULL) < 0)
	    selectable = FALSE;

	  return selectable;
	}
    }

  return XFE_MNView::isCommandEnabled(cmd, calldata);
#undef IS_CMD
}

void
XFE_MsgView::doCommand(CommandType cmd,
					   void *calldata, XFE_CommandInfo* info)
{
#define IS_CMD(command) cmd == (command)
  
  if (IS_CMD(xfeCmdMommy))
	  {
		  fe_showMessages(XtParent(getToplevel()->getBaseWidget()),
						  ViewGlue_getFrame(m_contextData),
						  NULL,
						  m_folderInfo,
						  fe_globalPrefs.reuse_thread_window,
						  False, MSG_MESSAGEKEYNONE);
	  }
  else if (IS_CMD(xfeCmdGetNewMessages))
    {
		getNewMail();
    }
  else if (IS_CMD(xfeCmdGetNextNNewMsgs))
	  {
		  getNewNews();
	  }
  else if (IS_CMD(xfeCmdMarkMessageByDate))
    {
		  markReadByDate();
    }
  else if (IS_CMD(xfeCmdEditPreferences))
    {
      fe_showMailNewsPreferences(getToplevel(), m_contextData);
    }
  else if (IS_CMD(xfeCmdPrint))
    {
	fe_print_cb  (CONTEXT_WIDGET (m_contextData),
                     (XtPointer) m_contextData, NULL);
 
	getToplevel()->notifyInterested(XFE_View::chromeNeedsUpdating);

    }
  else if (IS_CMD(xfeCmdSearch))
    {
		// We don't need to call XtParent on the base widget because
        // the base widget is the real toplevel widget already...dora 12/31/96
		// Grabbed this from ThreadView.cpp  -slamm 2/19/96
		fe_showMNSearch(XfeAncestorFindApplicationShell(getToplevel()->getBaseWidget()),
						ViewGlue_getFrame(m_contextData),
						NULL, this, m_folderInfo);
    }
  else if (IS_CMD(xfeCmdSetPriorityHighest)
	   || IS_CMD(xfeCmdSetPriorityHigh)
	   || IS_CMD(xfeCmdSetPriorityNormal)
	   || IS_CMD(xfeCmdSetPriorityLow)
	   || IS_CMD(xfeCmdSetPriorityLowest)
	   || IS_CMD(xfeCmdSetPriorityNone))
    {
      MSG_PRIORITY priority = commandToPriority(cmd);

      MSG_SetPriority(m_pane, m_messageKey, priority);
    }
  else if (IS_CMD(xfeCmdGetNewMessages))
    {
      getNewMail();
    }
  else if ( IS_CMD(xfeCmdEditConfiguration) ||
        IS_CMD(xfeCmdModerateDiscussion) )
  {
          MSG_ViewIndex index ;
          MessageKey key;
          MSG_FolderInfo *info;
          MSG_CommandType msg_cmd = commandToMsgCmd(cmd);

          MSG_GetCurMessage(m_pane, &info, &key, &index);

          MSG_Command(m_pane, msg_cmd, &index, 1);
  }
  else if (isDisplayingNews()
      && (IS_CMD(xfeCmdDeleteMessage) || IS_CMD(xfeCmdCancelMessages)))
    {
        // If this is a news article, then Delete Message
        // is really Cancel Message:
        MSG_Command(m_pane, MSG_CancelMessage, NULL, 0);
		m_updateThread = True;
    }
  else if (IS_CMD(xfeCmdMoveMessage))
	{
		MSG_FolderInfo *info = (MSG_FolderInfo*)calldata;
	    MSG_ViewIndex index;
		MSG_FolderInfo *curFolderInfo;
		MessageKey key;
		MSG_GetCurMessage(m_pane, &curFolderInfo, &key, &index);

		if (index && info) {
			MSG_MoveMessagesIntoFolder(m_pane, &index, 1, info);
			m_updateThread = True;
		}/* if */
    }
  else if (cmd == xfeCmdChangeDocumentEncoding)
    {
      int/*16*/ new_doc_csid = (int/*16*/)calldata;
      INTL_CharSetInfo c = LO_GetDocumentCharacterSetInfo(m_contextData);
      
      if (new_doc_csid != m_contextData->fe.data->xfe_doc_csid) 
        {
          /*
           * Somebody is setting the doc_csid to a non-zero
           * value, thereby screwing up our GetDefaultDocCSID
           * routine. Make sure it gets set to zero here
           * in the mail and news cases. Don't want to set
           * it to zero in the Browser case, since it is
           * legitimate there when the HTTP charset has been
           * set. We override the charset in the mail and
           * news cases.
           */
          INTL_SetCSIDocCSID(c, new_doc_csid);      
          m_contextData->fe.data->xfe_doc_csid = new_doc_csid;
          INTL_SetCSIWinCSID(c,
          INTL_DocToWinCharSetID(new_doc_csid));
          // now let our observers know that the encoding
          // for this window needs to be changed.
          getToplevel()->notifyInterested(XFE_Frame::encodingChanged, calldata);
	      MSG_SetFolderCSID(m_folderInfo, m_contextData->fe.data->xfe_doc_csid);
        }
    }
  else if (cmd == xfeCmdSetDefaultDocumentEncoding)
    {
    }
  else
    {
      MSG_CommandType msg_cmd = commandToMsgCmd(cmd);
      MSG_MotionType nav_cmd = commandToMsgNav(cmd);

	  if (info) {
		  CONTEXT_DATA(m_contextData)->stealth_cmd =
			  ((info->event->type == ButtonRelease) &&
			   (info->event->xkey.state & ShiftMask));
	  }

      if (nav_cmd == (MSG_MotionType)~0)
	{
	  if ((msg_cmd == (MSG_CommandType)~0) ||
	      (msg_cmd == MSG_MailNew) ||
	      (msg_cmd == MSG_PostNew) )
	    {
	      XFE_MNView::doCommand(cmd, calldata, info);
	    }
	  else
	    {
	 	  if (msg_cmd == MSG_DeleteMessage)
			  /* stop loading since we are deleting this message:
			   * shall we check if it is stopable?
			   */
			  XP_InterruptContext(m_contextData);

	      MSG_Command(m_pane, msg_cmd, NULL, 0);

		  if (msg_cmd == MSG_DeleteMessage ||
			  msg_cmd == MSG_CancelMessage) {
			  m_updateThread = True;
		  }/* if */
			  
			  
 		  /* stand alone msgpane
 		   */
 		  if (IS_CMD(xfeCmdUndo) 
 			  || IS_CMD(xfeCmdRedo)) {
 			  MessageKey key = MSG_MESSAGEKEYNONE;
 			  MSG_FolderInfo *folder = NULL;
 
 			  if ( UndoComplete == MSG_GetUndoStatus(m_pane) ) {
 				  if (MSG_GetUndoMessageKey(m_pane, &folder, &key) && folder) 
 					  loadMessage(folder, key);
 			  }/* if */
 		  }/* if */
 
		  XFE_MozillaApp::theApp()->notifyInterested(XFE_MNView::folderChromeNeedsUpdating, getFolderInfo());
	    }
	}
      else
	{
	  MSG_ViewIndex index, threadIndex;
	  MessageKey key;
	  MessageKey resultId;
	  MSG_FolderInfo *info;
	  
	  MSG_GetCurMessage(m_pane, &info, &key, &index);

	  // MsgView might be invoked from Search Dialog
	  // In this circumstances, m_folderInfo was
	  // not initialized. Therefore, we will see a crash when one
	  // tries to do Next Msg.
	  // We try to initialize m_folderInfo here base on current msg
	  // msg folder info. 

	  // The above comment is not valid if we set m_folderInfo when MessageLoad
	  // notification arrives
	  //
	  if (!m_folderInfo) 
		  m_folderInfo = info;

	  MSG_FolderInfo *curInfo = info;
	  MSG_ViewNavigate(m_pane, nav_cmd, index,
					   &resultId, &index, &threadIndex, 
					   &info);

	  // ViewNavigate gives a NULL folderinfo if the folder
	  // info won't be changing.
	  if (!info) {
		  int idx = (int) index;
		  if (idx >= 0) {
			  info = m_folderInfo;
			  loadMessage(info, resultId);
		  }/* if */
	  }/* if */
	  else if (curInfo != info) {
		  /* folder changed
		   */
		  /* get parent view
		   */
		  XFE_ThreadView *threadView = (XFE_ThreadView *)getParent();

		  /* load new folder 
		   */
		  if (threadView) {
			  /* in thread win
			   */
			  threadView->loadFolder(info);
			  threadView->setPendingCmdSelByKey(selectByKey, resultId);
		  }/* if */
		  else {
			  /* in message win
			   */
			  MWContext *thrContext = fe_showMessages(XtParent(getToplevel()->getBaseWidget()),
													  ViewGlue_getFrame(m_contextData),
													  NULL,
													  info,
													  fe_globalPrefs.reuse_thread_window,
													  False, resultId);
			  if (thrContext) {
				  XFE_ThreadFrame *tFrame = (XFE_ThreadFrame *) ViewGlue_getFrame(thrContext);
				  threadView = (XFE_ThreadView *) tFrame->getView();
				  if (threadView) {
					  switch (nav_cmd) {
					  case MSG_NextMessage:
					  threadView->setPendingCmdSelByKey(selectFirstUnread, resultId);
					  }/* switch */
				  }/* if */
			  }/* if */

			  /* load myself
			   */
		  }/* else */
	  }/* else if */

	}
    }
#undef IS_CMD
}

Boolean
XFE_MsgView::handlesCommand(CommandType cmd, void *calldata, XFE_CommandInfo*)
{
#define IS_CMD(command) cmd == (command)

  if ( IS_CMD(xfeCmdSaveAs)
      || IS_CMD(xfeCmdSaveMessagesAs)

      || IS_CMD(xfeCmdAddSenderToAddressBook)
      || IS_CMD(xfeCmdAddAllToAddressBook)

	  || IS_CMD(xfeCmdSaveMessagesAs)
	  || IS_CMD(xfeCmdGetNewMessages)
	  || IS_CMD(xfeCmdGetNextNNewMsgs)
      || IS_CMD(xfeCmdPrint)

	  || IS_CMD(xfeCmdSendMessagesInOutbox)

      || IS_CMD(xfeCmdNextUnreadMessage)
      || IS_CMD(xfeCmdPreviousUnreadMessage)
      || IS_CMD(xfeCmdNextMessage)
      || IS_CMD(xfeCmdPreviousMessage)
      || IS_CMD(xfeCmdNextUnreadThread)
      || IS_CMD(xfeCmdNextCollection)
      || IS_CMD(xfeCmdNextUnreadCollection)
      || IS_CMD(xfeCmdFirstFlaggedMessage)
      || IS_CMD(xfeCmdPreviousFlaggedMessage)
      || IS_CMD(xfeCmdNextFlaggedMessage)

      || IS_CMD(xfeCmdUndo)
      || IS_CMD(xfeCmdRedo)
      || IS_CMD(xfeCmdSearch)

      || IS_CMD(xfeCmdDeleteMessage)
      || IS_CMD(xfeCmdEditPreferences)

      || IS_CMD(xfeCmdMarkMessageRead)
      || IS_CMD(xfeCmdMarkMessageUnread)
	  || IS_CMD(xfeCmdMarkMessageByDate)
	  || IS_CMD(xfeCmdMarkAllMessagesRead)
	  || IS_CMD(xfeCmdMarkThreadRead)
	  || IS_CMD(xfeCmdMarkMessageForLater)
	  || IS_CMD(xfeCmdMarkMessage)

      || IS_CMD(xfeCmdReplyToSender)
      || IS_CMD(xfeCmdReplyToAll)
      || IS_CMD(xfeCmdReplyToNewsgroup)
      || IS_CMD(xfeCmdReplyToSenderAndNewsgroup)
      || IS_CMD(xfeCmdForwardMessage)
      || IS_CMD(xfeCmdForwardMessageQuoted)
      || IS_CMD(xfeCmdShowAllHeaders)
      || IS_CMD(xfeCmdShowNormalHeaders)
      || IS_CMD(xfeCmdShowBriefHeaders)
      || IS_CMD(xfeCmdViewAttachmentsInline)
      || IS_CMD(xfeCmdViewAttachmentsAsLinks)
      || IS_CMD(xfeCmdRot13Message)
      || IS_CMD(xfeCmdMoveMessage)
      || IS_CMD(xfeCmdCopyMessage)

      || IS_CMD(xfeCmdNewFolder)
      || IS_CMD(xfeCmdRenameFolder)
      || IS_CMD(xfeCmdEmptyTrash)
      || IS_CMD(xfeCmdCompressAllFolders)
      || IS_CMD(xfeCmdAddNewsgroup)
      || IS_CMD(xfeCmdUpdateMessageCount)
      || IS_CMD(xfeCmdPrintSetup)
      || IS_CMD(xfeCmdPrintPreview)
      || IS_CMD(xfeCmdEditMessage)

      || IS_CMD(xfeCmdToggleThreadKilled)
      || IS_CMD(xfeCmdToggleThreadWatched)

      /* priorities. */
      || IS_CMD(xfeCmdSetPriorityHighest)
      || IS_CMD(xfeCmdSetPriorityHigh)
      || IS_CMD(xfeCmdSetPriorityNormal)
      || IS_CMD(xfeCmdSetPriorityLow)
      || IS_CMD(xfeCmdSetPriorityLowest)
      || IS_CMD(xfeCmdSetPriorityNone)

      || IS_CMD(xfeCmdIgnoreThread)
      || IS_CMD(xfeCmdWatchThread)
      || IS_CMD(xfeCmdModerateDiscussion)

      || IS_CMD(xfeCmdWrapLongLines)
	  || IS_CMD(xfeCmdMommy))
    {
      return True;
    }
  else
    {
      return XFE_MNView::handlesCommand(cmd, calldata);
    }
#undef IS_CMD
}

char*
XFE_MsgView::commandToString(CommandType cmd, void * calldata, XFE_CommandInfo* info)
{
#define IS_CMD(command) cmd == (command)  

    if ( (IS_CMD(xfeCmdDeleteMessage) || IS_CMD(xfeCmdCancelMessages))
         && isDisplayingNews() )
        return XP_GetString(MK_MSG_CANCEL_MESSAGE);
	else if ( calldata &&
			  (IS_CMD(xfeCmdReplyToSender) || IS_CMD(xfeCmdReplyToAll)
			   || IS_CMD(xfeCmdReplyToNewsgroup)
			   || IS_CMD(xfeCmdReplyToSenderAndNewsgroup) ) )
		{
			// if calldata is non-null for a Reply button,
			// that's a signal not to change the
			// widget name in the resources file:
			return 0;
		}
	
	MSG_CommandType msg_cmd = commandToMsgCmd(cmd);
	MSG_MotionType msg_nav = commandToMsgNav(cmd);
	const char *display_string = NULL;
	
	if (msg_cmd != (MSG_CommandType)~0)
		{
			if (IS_CMD(xfeCmdGetNewMessages)) {
				int num_inboxes = MSG_GetFoldersWithFlag(XFE_MNView::getMaster(),
														 MSG_FOLDER_FLAG_INBOX,
														 NULL, 0);
				if ((num_inboxes >0) ||
					(MSG_CommandStatus(m_pane, msg_cmd, 
									   NULL, 0, NULL, NULL, 
									   (const char **)&display_string, NULL) < 0)) {
					return NULL;
				}/* if */
				return (char*)display_string;
			}/* if */
			else if (MSG_CommandStatus(m_pane, msg_cmd, 
									   NULL, 0, NULL, NULL, 
									   (const char **)&display_string, NULL) < 0)
				return NULL;
			else
                        {
                          if ( (cmd == xfeCmdComposeMessageHTML ) ||
                             ( cmd == xfeCmdComposeMessagePlain) ||
                             ( cmd == xfeCmdComposeArticleHTML) ||
                             ( cmd == xfeCmdComposeArticlePlain) )
                          {
                              return NULL;
                          }
                          else
                             return (char*)display_string;
                        }

		}
	else if (msg_cmd != (MSG_MotionType)~0)
		{
			MessageKey key;
			MSG_ViewIndex index;
			MSG_FolderInfo *info;
			
			MSG_GetCurMessage(m_pane, &info, &key, &index);
			
			if (index == MSG_VIEWINDEXNONE
				|| MSG_NavigateStatus(m_pane, msg_nav, 
									  (MSG_ViewIndex)index,
									  NULL, (const char **)&display_string) < 0)
				return NULL;
			else
                        {
                          if ( (cmd == xfeCmdComposeMessageHTML ) ||
                             ( cmd == xfeCmdComposeMessagePlain) ||
                             ( cmd == xfeCmdComposeArticleHTML) ||
                             ( cmd == xfeCmdComposeArticlePlain) )
                          {
                              return NULL;
                          }
                          else
                             return (char*)display_string;
                        }
		}
	else if (IS_CMD(xfeCmdFindInObject))
		{
			/* short circuit HTMLView's "Find in Page/Frame" and keep
			   our "Find in Message." */
			return NULL;
		}
	else 
		return XFE_MNView::commandToString(cmd, calldata, info);
#undef IS_CMD
}

XFE_CALLBACK_DEFN(XFE_MsgView, showPopup)(XFE_NotificationCenter *,
										  void *, void *calldata)
{
	D(printf("XFE_MsgView::showPopup()\n");)
	XEvent *event = (XEvent *)calldata;

	URL_Struct *url_under_mouse = m_htmlView->URLUnderMouse();
	URL_Struct *image_under_mouse = m_htmlView->ImageUnderMouse();
	URL_Struct *background_under_mouse = m_htmlView->BackgroundUnderMouse();

	XP_Bool isBrowserLink = FALSE;
	XP_Bool isAttachmentLink = FALSE;
	XP_Bool isLink = url_under_mouse != NULL;
	XP_Bool isImage = image_under_mouse != NULL;
	XP_Bool needSeparator = FALSE;
	XP_Bool haveAddedItems = FALSE;

	if (m_popup)
		delete m_popup;

	m_popup = new XFE_PopupMenu("popup",(XFE_Frame*)m_toplevel,
					XfeAncestorFindApplicationShell(m_widget));

	if (url_under_mouse != NULL
		&& XP_STRNCMP ("mailto:", url_under_mouse->address, 7) != 0
		&& XP_STRNCMP ("telnet:", url_under_mouse->address, 7) != 0
		&& XP_STRNCMP ("tn3270:", url_under_mouse->address, 7) != 0
		&& XP_STRNCMP ("rlogin:", url_under_mouse->address, 7) != 0
		&& XP_STRNCMP ("addbook:", url_under_mouse->address, 8) != 0)
		{
			isBrowserLink = TRUE;
		}

	if (url_under_mouse != NULL
		&& XP_STRNCASECMP ("mailbox:", url_under_mouse->address, 8) != 0
		&& XP_STRSTR(url_under_mouse->address, "part?"))
		{
			isAttachmentLink = TRUE;
		}

	/* turn off everything if the user right clicks on the paperclip icon */
	if (url_under_mouse != NULL
		&& XP_STRCMP ("mailbox:displayattachments", url_under_mouse->address) == 0)
	  {
		isBrowserLink = FALSE;
		isAttachmentLink = FALSE;
		isImage = FALSE;
	  }

#define ADD_MENU_SEPARATOR {                     \
    if (haveAddedItems) {                        \
      needSeparator = TRUE;                      \
      haveAddedItems = FALSE;                    \
    }                                            \
}

#define ADD_SPEC(theSpec) {                      \
  if (needSeparator)                             \
    m_popup->addMenuSpec(separator_spec);        \
  m_popup->addMenuSpec(theSpec);                 \
  needSeparator = FALSE;                         \
  haveAddedItems = TRUE;                         \
}

	if (isBrowserLink)                       ADD_SPEC ( openLinkNew_spec );
#ifdef EDITOR
	if (isBrowserLink)						 ADD_SPEC ( openLinkEdit_spec );
#endif

	ADD_MENU_SEPARATOR;
	
	ADD_SPEC ( repl_spec );

	ADD_MENU_SEPARATOR;

	ADD_SPEC ( addr_spec );
	
	ADD_MENU_SEPARATOR;

	ADD_SPEC ( filemsg_spec );

	if (isCommandEnabled(xfeCmdStopLoading)) ADD_SPEC ( stopLoading_spec );
	ADD_MENU_SEPARATOR;
	if (isImage)                             ADD_SPEC ( openImage_spec );
	ADD_MENU_SEPARATOR;
	if (isBrowserLink)                       ADD_SPEC ( saveLink_spec );
	if (isImage)                             ADD_SPEC ( saveImage_spec );
	ADD_MENU_SEPARATOR;
	if (isCommandEnabled(xfeCmdCopy))        ADD_SPEC ( copy_spec );
	if (isLink)                              ADD_SPEC ( copyLink_spec );
	if (isImage)                             ADD_SPEC ( copyImage_spec );

	// Finish up the popup
	m_popup->position (event);
	m_popup->show();
}

XFE_CALLBACK_DEFN(XFE_MsgView, spaceAtMsgEnd)(XFE_NotificationCenter *,
											  void *, void */*callData*/)
{
  notifyInterested(spacebarAtMsgBottom);
}

XFE_CALLBACK_DEFN(XFE_MsgView, refresh)(XFE_NotificationCenter *,
										void *, void */*callData*/)
{
	m_htmlView->doCommand(xfeCmdRefresh);
}
