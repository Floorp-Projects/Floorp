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
   MNView.cpp -- Superclass for all mail news views (folder, thread, and message.)
   Created: Chris Toshok <toshok@netscape.com>, 7-Aug-96.
   */


#include "MozillaApp.h"
#include "ViewGlue.h"
#include "Frame.h"

#ifdef MOZ_MAIL_NEWS
#include "MNView.h"
#include "MNListView.h"
#include "MailDownloadFrame.h"
#include "NewsPromptDialog.h"
#include "ComposeFrame.h"
#include "ABListSearchView.h"
#include "ThreadView.h"
#include "dirprefs.h"
#endif

#include "Outlinable.h"
#include "Outliner.h"
#include "xfe.h"
#include "xp_mem.h"
#include "addrbook.h"
#include "xp_time.h"
#include <Xfe/Xfe.h>

#include "prefapi.h"

#include <dirent.h>
#include <signal.h>
#include <sys/wait.h>		/* for all the movemail crud */

#ifdef SCO_SV
#include "mcom_db.h"
#define _SVID3          /* for statvfs.h */
#endif

#include "xpgetstr.h"

#ifdef MOZ_MAIL_NEWS

extern int XFE_MAIL_PRIORITY_NONE;
extern int XFE_MAIL_PRIORITY_LOWEST;
extern int XFE_MAIL_PRIORITY_LOW;
extern int XFE_MAIL_PRIORITY_NORMAL;
extern int XFE_MAIL_PRIORITY_HIGH;
extern int XFE_MAIL_PRIORITY_HIGHEST;
extern int XFE_MAIL_SPOOL_UNKNOWN;
extern int XFE_UNABLE_TO_OPEN;
extern int XFE_NO_KEEP_ON_SERVER_WITH_MOVEMAIL;

extern int XFE_COULDNT_FORK_FOR_MOVEMAIL;
extern int XFE_PROBLEMS_EXECUTING;
extern int XFE_TERMINATED_ABNORMALLY;
extern int XFE_APP_EXITED_WITH_STATUS;
extern int XFE_COULDNT_FORK_FOR_DELIVERY;

extern int XFE_GET_NEXT_N_MSGS;

extern int XFE_MARKBYDATE_CAPTION;
extern int XFE_MARKBYDATE;
extern int XFE_DATE_MUST_BE_MM_DD_YY;

MSG_Master *XFE_MNView::m_master = NULL;
MSG_Prefs *XFE_MNView::m_prefs = NULL;

MSG_BIFF_STATE XFE_MNView::m_biffstate = MSG_BIFF_Unknown;

fe_icon XFE_MNView::inboxIcon = { 0 };
fe_icon XFE_MNView::inboxOpenIcon = { 0 };
fe_icon XFE_MNView::draftsIcon = { 0 };
fe_icon XFE_MNView::draftsOpenIcon = { 0 };
fe_icon XFE_MNView::templatesIcon = { 0 };
fe_icon XFE_MNView::templatesOpenIcon = { 0 };
fe_icon XFE_MNView::filedMailIcon = { 0 };
fe_icon XFE_MNView::filedMailOpenIcon = { 0 };
fe_icon XFE_MNView::outboxIcon = { 0 };
fe_icon XFE_MNView::outboxOpenIcon = { 0 };
fe_icon XFE_MNView::trashIcon = { 0 };
fe_icon XFE_MNView::folderIcon = { 0 };
fe_icon XFE_MNView::folderOpenIcon = { 0 };
fe_icon XFE_MNView::folderServerIcon = { 0 };
fe_icon XFE_MNView::folderServerOpenIcon = { 0 };
fe_icon XFE_MNView::newsgroupIcon = { 0 };
fe_icon XFE_MNView::mailLocalIcon = { 0 };
fe_icon XFE_MNView::mailServerIcon = { 0 };
fe_icon XFE_MNView::newsServerIcon = { 0 };
fe_icon XFE_MNView::newsServerSecureIcon = { 0 };
fe_icon XFE_MNView::mailMessageReadIcon = { 0 };
fe_icon XFE_MNView::mailMessageUnreadIcon = { 0 };
fe_icon XFE_MNView::newsPostIcon = { 0 };
fe_icon XFE_MNView::draftIcon = { 0 };
fe_icon XFE_MNView::templateIcon = { 0 };
fe_icon XFE_MNView::newsNewIcon = { 0 };
fe_icon XFE_MNView::msgReadIcon = { 0 };
fe_icon XFE_MNView::msgUnreadIcon = { 0 };
fe_icon XFE_MNView::deletedIcon = { 0 };
fe_icon XFE_MNView::msgFlagIcon = { 0 };
fe_icon XFE_MNView::collectionsIcon = { 0 };
fe_icon XFE_MNView::mailMessageAttachIcon = { 0 };

fe_icon XFE_MNView::openSpoolIcon = { 0 };
fe_icon XFE_MNView::closedSpoolIcon = { 0 };
fe_icon XFE_MNView::openSpoolIgnoredIcon = { 0 };
fe_icon XFE_MNView::closedSpoolIgnoredIcon = { 0 };
fe_icon XFE_MNView::openSpoolWatchedIcon = { 0 };
fe_icon XFE_MNView::closedSpoolWatchedIcon = { 0 };
fe_icon XFE_MNView::openSpoolNewIcon = { 0 };
fe_icon XFE_MNView::closedSpoolNewIcon = { 0 };

typedef struct fe_mn_incstate {
  MWContext* context;
  char filename[512];
  XP_Bool wascrashbox;
  XP_File file;
} fe_mn_incstate;

static void fe_incdone(void* closure, XP_Bool result);
static XP_Bool fe_run_movemail (MWContext *context, const char *from, const char *to);
extern "C" char* fe_mn_getmailbox(void);

const char * XFE_MNView::bannerNeedsUpdating = "XFE_MNView::bannerNeedsUpdating";
const char * XFE_MNView::foldersHaveChanged = "XFE_MNView::foldersHaveChanged";
const char * XFE_MNView::newsgroupsHaveChanged = "XFE_MNView::newsgroupsHaveChanged";
const char * XFE_MNView::folderChromeNeedsUpdating = "XFE_MNView::folderChromeNeedsUpdating";
const char * XFE_MNView::MNChromeNeedsUpdating = "XFE_MNView::MNChromeNeedsUpdating";
const char * XFE_MNView::msgWasDeleted = "XFE_MNView::msgWasDeleted";
const char * XFE_MNView::folderDeleted = "XFE_MNView::folderDeleted";

XP_Bool XFE_MNView::m_messageDownloadInProgress = False;
int  XFE_MNView::m_numFolderBeingLoaded = 0; // Indicate how many folder are loaded


XFE_MNView::XFE_MNView(XFE_Component *toplevel_component,
		       XFE_View *parent_view, 
		       MWContext *context,
		       MSG_Pane *p) 
  : XFE_View(toplevel_component, parent_view, context)
{
  m_pane = p;

  // a safe default;
  m_displayingNewsgroup = FALSE;

  // initialize the mail master
  getMaster();


  XFE_MozillaApp::theApp()->registerInterest(
		XFE_MozillaApp::biffStateChanged,
		this,
		(XFE_FunctionNotification) updateBiffState_cb);
}

XFE_MNView::~XFE_MNView()
{
  XFE_MozillaApp::theApp()->unregisterInterest(
		XFE_MozillaApp::biffStateChanged,
		this,
		(XFE_FunctionNotification)updateBiffState_cb);
}

void
XFE_MNView::destroyPane()
{
	if (m_pane)
		{
			XP_InterruptContext(m_contextData);
			MSG_DestroyPane(m_pane);
			
			m_pane = NULL;
		}
}

MSG_Pane *
XFE_MNView::getPane()
{
  return m_pane;
}

void
XFE_MNView::setPane(MSG_Pane *p)
{
  m_pane = p;

  // we only bind the view to the pane when
  // the m_pane is not NULL. Otherwise, don't bind.
  if ( m_pane ) 
	  MSG_SetFEData(m_pane, this);
}

void
XFE_MNView::updateCompToolbar()
{
	// NOTE:  we are using this to wait for the MSG layer to finish all
	//        of it's connections before we attempt to initialize the 
	//        Editor...
}

MSG_MotionType
XFE_MNView::commandToMsgNav(CommandType cmd)
{
  MSG_MotionType msg_cmd = (MSG_MotionType)~0;

#define BEGIN_MN_MSG_MAP() if (0)
#define MN_MSGMAP(the_cmd, the_msg_cmd) else if (cmd == (the_cmd)) msg_cmd = (the_msg_cmd)
  BEGIN_MN_MSG_MAP();
  MN_MSGMAP(xfeCmdBack, MSG_Back);
  MN_MSGMAP(xfeCmdForward, MSG_Forward);
  MN_MSGMAP(xfeCmdNextMessage, MSG_NextMessage);
  MN_MSGMAP(xfeCmdNextUnreadNewsgroup, MSG_NextUnreadGroup);
  MN_MSGMAP(xfeCmdNextUnreadMessage, MSG_NextUnreadMessage);
  MN_MSGMAP(xfeCmdNextUnreadThread, MSG_NextUnreadThread);
  MN_MSGMAP(xfeCmdNextFlaggedMessage, MSG_NextFlagged);
  MN_MSGMAP(xfeCmdFirstFlaggedMessage, MSG_FirstFlagged);
  MN_MSGMAP(xfeCmdFirstUnreadMessage, MSG_FirstUnreadMessage);
  MN_MSGMAP(xfeCmdPreviousMessage, MSG_PreviousMessage);
  MN_MSGMAP(xfeCmdPreviousUnreadMessage, MSG_PreviousUnreadMessage);
  MN_MSGMAP(xfeCmdPreviousFlaggedMessage, MSG_PreviousFlagged);
#undef MN_MSGMAP
#undef BEGIN_MN_MSG_MAP

  return msg_cmd;
}

MSG_CommandType
XFE_MNView::commandToMsgCmd(CommandType cmd)
{
  MSG_CommandType msg_cmd = (MSG_CommandType)~0;

#define BEGIN_MN_MSG_MAP() if (0)
#define MN_MSGMAP(the_cmd, the_msg_cmd) else if (cmd == (the_cmd)) msg_cmd = (the_msg_cmd)
  BEGIN_MN_MSG_MAP();
  MN_MSGMAP(xfeCmdAddAllToAddressBook, MSG_AddAll);
  MN_MSGMAP(xfeCmdAddNewsgroup, MSG_AddNewsGroup);
  MN_MSGMAP(xfeCmdAddSenderToAddressBook, MSG_AddSender);
  MN_MSGMAP(xfeCmdCancelMessages, MSG_CancelMessage);
  MN_MSGMAP(xfeCmdClearNewGroups, MSG_ClearNew); // subscribe ui only
  MN_MSGMAP(xfeCmdCollapseAll, MSG_CollapseAll); // subscribe ui only
  MN_MSGMAP(xfeCmdComposeArticle, MSG_PostNew);
  MN_MSGMAP(xfeCmdComposeArticleHTML, MSG_PostNew);
  MN_MSGMAP(xfeCmdComposeArticlePlain, MSG_PostNew);
  MN_MSGMAP(xfeCmdComposeMessage, MSG_MailNew);
  MN_MSGMAP(xfeCmdComposeMessageHTML, MSG_MailNew);
  MN_MSGMAP(xfeCmdComposeMessagePlain, MSG_MailNew);
  MN_MSGMAP(xfeCmdCompressAllFolders, MSG_CompressAllFolders);
  MN_MSGMAP(xfeCmdCompressFolders, MSG_CompressFolder);
  MN_MSGMAP(xfeCmdDeleteFolder, MSG_DeleteFolder);
  MN_MSGMAP(xfeCmdDeleteMessage, MSG_DeleteMessage);
  MN_MSGMAP(xfeCmdEditAddress, MSG_EditAddress);
  MN_MSGMAP(xfeCmdEmptyTrash, MSG_EmptyTrash);
  MN_MSGMAP(xfeCmdExpandAll, MSG_ExpandAll); // subscribe ui only
  MN_MSGMAP(xfeCmdFetchGroupList, MSG_FetchGroupList); // subscribe ui only
  MN_MSGMAP(xfeCmdForwardMessage, MSG_ForwardMessage);
  MN_MSGMAP(xfeCmdForwardMessageQuoted, MSG_ForwardMessageQuoted);
  MN_MSGMAP(xfeCmdForwardMessageInLine, MSG_ForwardMessageInline);
  MN_MSGMAP(xfeCmdGetNewGroups, MSG_CheckForNew); // subscribe ui only
  MN_MSGMAP(xfeCmdGetNewMessages, MSG_GetNewMail);
  MN_MSGMAP(xfeCmdGetNextNNewMsgs, MSG_GetNextChunkMessages);
  MN_MSGMAP(xfeCmdIgnoreThread, MSG_ToggleThreadKilled);
  MN_MSGMAP(xfeCmdWatchThread, MSG_ToggleThreadWatched);
  MN_MSGMAP(xfeCmdMarkAllMessagesRead, MSG_MarkAllRead);
  MN_MSGMAP(xfeCmdMarkMessage, MSG_MarkMessages);
  MN_MSGMAP(xfeCmdMarkMessageRead, MSG_MarkMessagesRead);
  MN_MSGMAP(xfeCmdMarkMessageUnread, MSG_MarkMessagesUnread);
  MN_MSGMAP(xfeCmdMarkThreadRead, MSG_MarkThreadRead);
  MN_MSGMAP(xfeCmdModerateDiscussion, MSG_ModerateNewsgroup);
  MN_MSGMAP(xfeCmdNewCategory, MSG_NewCategory);
  MN_MSGMAP(xfeCmdNewFolder, MSG_NewFolder);
  MN_MSGMAP(xfeCmdOpenAddressBook, MSG_EditAddressBook);
  MN_MSGMAP(xfeCmdOpenFolder, MSG_OpenFolder);
  MN_MSGMAP(xfeCmdOpenMessageAsDraft, MSG_OpenMessageAsDraft);
  MN_MSGMAP(xfeCmdEditMessage, MSG_OpenMessageAsDraft);
  MN_MSGMAP(xfeCmdPrint, MSG_Print);
  MN_MSGMAP(xfeCmdRedo, MSG_Redo);
  MN_MSGMAP(xfeCmdRenameFolder, MSG_DoRenameFolder);
  MN_MSGMAP(xfeCmdReplyToAll, MSG_ReplyToAll);
  MN_MSGMAP(xfeCmdReplyToNewsgroup, MSG_PostReply);
  MN_MSGMAP(xfeCmdReplyToSender, MSG_ReplyToSender);
  MN_MSGMAP(xfeCmdReplyToSenderAndNewsgroup, MSG_PostAndMailReply);
  MN_MSGMAP(xfeCmdResort, MSG_ReSort);
  MN_MSGMAP(xfeCmdRetrieveMarkedMessages, MSG_RetrieveMarkedMessages);
  MN_MSGMAP(xfeCmdRetrieveSelectedMessages, MSG_RetrieveSelectedMessages);
  MN_MSGMAP(xfeCmdRot13Message, MSG_Rot13Message);
  MN_MSGMAP(xfeCmdSaveMessagesAs, MSG_SaveMessagesAs);
  MN_MSGMAP(xfeCmdSaveAsTemplate, MSG_SaveTemplate );
  MN_MSGMAP(xfeCmdSaveMessagesAsAndDelete, MSG_SaveMessagesAsAndDelete);
  MN_MSGMAP(xfeCmdSendMessagesInOutbox, MSG_DeliverQueuedMessages);
  MN_MSGMAP(xfeCmdShowAllHeaders, MSG_ShowAllHeaders);
  MN_MSGMAP(xfeCmdShowNormalHeaders, MSG_ShowSomeHeaders);
  MN_MSGMAP(xfeCmdShowBriefHeaders, MSG_ShowMicroHeaders);
  MN_MSGMAP(xfeCmdShowAllMessages, MSG_ShowAllMessages);
  MN_MSGMAP(xfeCmdShowOnlyUnreadMessages, MSG_ShowOnlyUnreadMessages);
  MN_MSGMAP(xfeCmdSortBackward, MSG_SortBackward);
  MN_MSGMAP(xfeCmdSortByDate, MSG_SortByDate);
  MN_MSGMAP(xfeCmdSortByMessageNumber, MSG_SortByMessageNumber);
  MN_MSGMAP(xfeCmdSortByPriority, MSG_SortByPriority);
  MN_MSGMAP(xfeCmdSortBySender, MSG_SortBySender);
  MN_MSGMAP(xfeCmdSortBySubject, MSG_SortBySubject);
  MN_MSGMAP(xfeCmdSortByThread, MSG_SortByThread);
  MN_MSGMAP(xfeCmdSortBySize, MSG_SortBySize);
  MN_MSGMAP(xfeCmdSortByStatus, MSG_SortByStatus);
  MN_MSGMAP(xfeCmdSortByFlag, MSG_SortByFlagged);
  MN_MSGMAP(xfeCmdSortByUnread, MSG_SortByUnread);
  MN_MSGMAP(xfeCmdToggleKilledThreads, MSG_ViewKilledThreads);
  MN_MSGMAP(xfeCmdToggleMessageRead, MSG_ToggleMessageRead);
  MN_MSGMAP(xfeCmdToggleSubscribe, MSG_ToggleSubscribed); // subscribe ui only.
  MN_MSGMAP(xfeCmdToggleThreadKilled, MSG_ToggleThreadKilled);
  MN_MSGMAP(xfeCmdToggleThreadWatched, MSG_ToggleThreadWatched);
  MN_MSGMAP(xfeCmdUndo, MSG_Undo);
  MN_MSGMAP(xfeCmdUnmarkMessage, MSG_UnmarkMessages);
  MN_MSGMAP(xfeCmdUnsubscribe, MSG_Unsubscribe);
  MN_MSGMAP(xfeCmdViewAllThreads, MSG_ViewAllThreads);
  MN_MSGMAP(xfeCmdViewAttachmentsAsLinks, MSG_AttachmentsAsLinks);
  MN_MSGMAP(xfeCmdViewAttachmentsInline, MSG_AttachmentsInline);
  MN_MSGMAP(xfeCmdViewNew, MSG_ViewNewOnly);
  MN_MSGMAP(xfeCmdViewThreadsWithNew, MSG_ViewThreadsWithNew);
  MN_MSGMAP(xfeCmdViewWatchedThreadsWithNew, MSG_ViewWatchedThreadsWithNew);
  MN_MSGMAP(xfeCmdWrapLongLines, MSG_WrapLongLines);
  MN_MSGMAP(xfeCmdEditConfiguration, MSG_ManageMailAccount);
  MN_MSGMAP(xfeCmdModerateDiscussion, MSG_ModerateNewsgroup);
#undef BEGIN_MN_MSG_MAP
#undef MN_MSG_MAP

  return msg_cmd;
}

MSG_PRIORITY
XFE_MNView::commandToPriority(CommandType cmd)
{
  MSG_PRIORITY priority = MSG_PriorityNotSet;

#define BEGIN_MN_PRI_MAP() if (0)
#define MN_PRIMAP(the_cmd, the_priority) else if (cmd == (the_cmd)) priority = (the_priority)

  BEGIN_MN_PRI_MAP();
  MN_PRIMAP(xfeCmdSetPriorityHighest, MSG_HighestPriority);
  MN_PRIMAP(xfeCmdSetPriorityHigh, MSG_HighPriority);
  MN_PRIMAP(xfeCmdSetPriorityNormal, MSG_NormalPriority);
  MN_PRIMAP(xfeCmdSetPriorityLow, MSG_LowPriority);
  MN_PRIMAP(xfeCmdSetPriorityLowest, MSG_LowestPriority);

#undef BEGIN_MN_MSG_MAP
#undef MN_MSG_MAP

  return priority;
}

char *
XFE_MNView::priorityToString(MSG_PRIORITY priority)
{
  switch (priority)
    {
    case MSG_PriorityNotSet:
    case MSG_NoPriority:
    case MSG_NormalPriority:
      return "";
    case MSG_HighPriority:
      return XP_GetString(XFE_MAIL_PRIORITY_HIGH);
    case MSG_LowestPriority:
      return XP_GetString(XFE_MAIL_PRIORITY_LOWEST);
    case MSG_LowPriority:
      return XP_GetString(XFE_MAIL_PRIORITY_LOW);
    case MSG_HighestPriority:
      return XP_GetString(XFE_MAIL_PRIORITY_HIGHEST);
    default:
      XP_ASSERT(0);
      return 0;
    }
}

char *
XFE_MNView::commandToString(CommandType cmd, void * calldata, XFE_CommandInfo* info)
{
#define IS_CMD(command) cmd == (command)  
	if (IS_CMD(xfeCmdNewFolder)) 
		{
			if (m_displayingNewsgroup)
				return stringFromResource("newCategoryCmdString");
			else
				return stringFromResource("newFolderCmdString");
		}
	else if (IS_CMD(xfeCmdDeleteMessage)) 
		{
			if (m_displayingNewsgroup)
				return stringFromResource("cancelMessageCmdString");
			else
				return stringFromResource("deleteMessageCmdString");
		}
	else if (IS_CMD(xfeCmdNextCollection)) 
		{
			if (m_displayingNewsgroup)
				return stringFromResource("nextDiscussionCmdString");
			else
				return stringFromResource("nextFolderCmdString");
		}
	else if (IS_CMD(xfeCmdNextUnreadCollection)) 
		{
			if (m_displayingNewsgroup)
				return stringFromResource("nextUnreadDiscussionCmdString");
			else
				return stringFromResource("nextUnreadFolderCmdString");
		}
	else if (IS_CMD(xfeCmdGetNextNNewMsgs)) 
	  {
		  static char buf[100];
		  int32 num;

		  if (PREF_GetIntPref("news.max_articles", &num) < 0) {
            // Bad pref read, so make something up
            num = 500;
          }

		  PR_snprintf(buf, sizeof(buf), XP_GetString(XFE_GET_NEXT_N_MSGS), num);

		  return buf;
	  }
	else
		return XFE_View::commandToString(cmd, calldata, info);
		
#undef IS_CMD
}

void
XFE_MNView::markReadByDate()
{
	char *date;
	char cur_date_buf[9];
	struct tm *current_tm;
	time_t entered_time;
	time_t foo;
	
	foo = time(NULL);
	current_tm = localtime(&foo);
	
	PR_snprintf (cur_date_buf, sizeof(cur_date_buf), "%02d/%02d/%02d",
				 current_tm->tm_mon + 1,
				 current_tm->tm_mday,
				 current_tm->tm_year);
	
	date = XFE_PromptWithCaption(m_contextData, 
								 /* get title from resources
								  */
								 "markMessagesRead",
								 XP_GetString(XFE_MARKBYDATE),
								 cur_date_buf);
	
	if (date)
		{
			entered_time = XP_ParseTimeString(date, False);
			
			if (!entered_time)
				{
					FE_Alert(m_contextData, XP_GetString(XFE_DATE_MUST_BE_MM_DD_YY));
				}
			else
				{
					MSG_MarkReadByDate(m_pane, 0, entered_time);
				}
		}
	
	if (fe_IsContextDestroyed(m_contextData)) 
		{
			XP_FREE(CONTEXT_DATA(m_contextData));
			XP_FREE(m_contextData);
		}
}

void
XFE_MNView::getNewNews()
{
	int status;
	const char *string = 0;
	XP_Bool selectable_p = FALSE;
	MSG_COMMAND_CHECK_STATE selected_p = MSG_NotUsed;

	status = MSG_CommandStatus (m_pane,
								MSG_GetNextChunkMessages, NULL, 0,
								&selectable_p, &selected_p,
								&string, 0);

	if (status < 0 || !selectable_p)
		return;

	XFE_MailDownloadFrame *download_frame = 
		new XFE_MailDownloadFrame(XtParent(getToplevel()->getBaseWidget()),
								  /* Tao: we might need to check if this returns a 
								   * non-NULL frame
								   */
								  ViewGlue_getFrame(m_contextData),
								  m_pane);
	
	download_frame->startNewsDownload();
}

void
XFE_MNView::getNewMail()
{
  int status;
  const char *string = 0;
  XP_Bool selectable_p = FALSE;
  MSG_COMMAND_CHECK_STATE selected_p = MSG_NotUsed;

  if ( fe_globalPrefs.use_movemail_p &&
       fe_globalPrefs.movemail_warn &&
       !fe_MovemailWarning(getContext()) ) return;

  status = MSG_CommandStatus (m_pane,
			      MSG_GetNewMail, NULL, 0,
                              &selectable_p, &selected_p,
                              &string, 0);

  if (status < 0 || !selectable_p)
    return;

  if (!fe_globalPrefs.use_movemail_p)
    {
		XFE_MailDownloadFrame *download_frame = 
			new XFE_MailDownloadFrame(XtParent(getToplevel()->getBaseWidget()),
									  /* Tao: we might need to check if this returns a 
									   * non-NULL frame
									   */
									  ViewGlue_getFrame(m_contextData),
									  m_pane);
		
		download_frame->startDownload();
    }
  else
    {
      /* Use movemail to get the mail out of the spool area and into
	 our home directory, and then read out of the resultant file
	 (without locking.)
	 */
      char *mailbox;
      char *s;
      XP_File file;
      XP_StatStruct st;
      fe_mn_incstate* state = 0;
	  
      /* First, prevent the user from assuming that `Leave on Server' will
	     work with movemail.  A better way to do this might be to change the
	     selection/sensitivity of the dialog buttons, but this will do.
	     (bug 11561)
	     */
      if (fe_globalPrefs.pop3_leave_mail_on_server)
	{
	  FE_Alert(m_contextData, XP_GetString(XFE_NO_KEEP_ON_SERVER_WITH_MOVEMAIL));
	  return;
	}
	  
      state = XP_NEW_ZAP(fe_mn_incstate);
      if (!state) return;
      state->context = m_contextData;
	  
	  /* If this doesn't exist, there's something seriously wrong */
      XP_ASSERT (fe_globalPrefs.movemail_program);
	  
      mailbox = fe_mn_getmailbox();
      if(!mailbox)
	{
	  FE_Alert(m_contextData, XP_GetString(XFE_MAIL_SPOOL_UNKNOWN));
	  return;
	}
	  
      s = (char *) MSG_GetFolderDirectory(MSG_GetPrefs(m_pane));
      PR_snprintf (state->filename, sizeof (state->filename),
		   "%s/.netscape.mail-recovery", s);
	  
      if (!stat (state->filename, &st))
	{
	  if (st.st_size == 0)
	    /* zero length - a mystery. */
	    unlink (state->filename);
	  else
	    {
	      /* The crashbox already exists - that means we didn't delete it
		 last time, possibly because we crashed.  So, read it now.
		 */
	      file = fopen (state->filename, "r");
	      if (!file)
		{
		  char buf[1024];
		  PR_snprintf (buf, sizeof (buf), XP_GetString( XFE_UNABLE_TO_OPEN ),
			       state->filename);
		  fe_perror (m_contextData, buf);
		  return;
		}
	      state->file = file;
	      state->wascrashbox = TRUE;
	      MSG_IncorporateFromFile (m_pane, file, fe_incdone, state);
	      return;
	    }
	}
	  
      /* Now move mail into the tmp file.
	   */
      if (fe_globalPrefs.builtin_movemail_p)
	{
	  if (!fe_MoveMail (m_contextData, mailbox, state->filename))
	    {
	      return;
	    }
	}
      else
	{
	  if (!fe_run_movemail (m_contextData, mailbox, state->filename))
	    return;
	}
	  
      /* Now move the mail from the crashbox to the folder.
	   */
      file = fopen (state->filename, "r");
      if (file)
	{
	  state->file = file;
	  MSG_IncorporateFromFile(m_pane, file, fe_incdone, state);
	}
    }
}

void 
XFE_MNView::paneChanged(XP_Bool /*asynchronous*/,
                        MSG_PANE_CHANGED_NOTIFY_CODE notify_code,
                        int32 /*value*/)
{
  notifyInterested(XFE_MNView::bannerNeedsUpdating, (void*)notify_code);
}

MSG_Master *
XFE_MNView::getMaster()
{
  if (m_master == NULL)
    {
      //      XFE_MNView::m_prefs = MSG_CreatePrefs();
      m_master = MSG_InitializeMail(fe_mailNewsPrefs);
    }

  return m_master;
}

void
XFE_MNView::destroyMasterAndShutdown()
{
	MWContext *biffcontext = XP_FindContextOfType(NULL, MWContextBiff);
	XP_ASSERT(biffcontext);
	MSG_BiffCleanupContext(biffcontext);

  if (m_master)
    MSG_DestroyMaster(m_master);

  MSG_ShutdownMsgLib();
}

XP_Bool
XFE_MNView::isDisplayingNews()
{
  return m_displayingNewsgroup;
}

Boolean
XFE_MNView::handlesCommand(CommandType cmd, void* calldata, XFE_CommandInfo* )
{
#define IS_CMD(command) cmd == (command)
if (  IS_CMD(xfeCmdComposeMessage)
      || IS_CMD(xfeCmdComposeMessageHTML)
      || IS_CMD(xfeCmdComposeMessagePlain)
      || IS_CMD(xfeCmdComposeArticle)
      || IS_CMD(xfeCmdComposeArticleHTML)
      || IS_CMD(xfeCmdComposeArticlePlain)
      || IS_CMD(xfeCmdEditConfiguration) 
      || IS_CMD(xfeCmdModerateDiscussion) )
    return True;
else if (IS_CMD(xfeCmdCleanUpDisk))
    return True;
else
   {
	return XFE_View::handlesCommand(cmd, calldata);
   }
#undef IS_CMD
}

void
XFE_MNView::doCommand(CommandType cmd, void *calldata, XFE_CommandInfo* info)
{
  if ( (cmd == xfeCmdComposeMessage) 
	|| (cmd == xfeCmdComposeArticle) )
  {
        if (info) {
                                  CONTEXT_DATA(m_contextData)->stealth_cmd =
                                          ((info->event->type == ButtonRelease) &&
                                           (info->event->xkey.state & ShiftMask));
                          }

            MSG_Command(m_pane, commandToMsgCmd(cmd), NULL, 0);
   }
   else if ( (cmd == xfeCmdComposeMessageHTML) 
		|| (cmd == xfeCmdComposeArticleHTML) )

   {
            CONTEXT_DATA(m_contextData)->stealth_cmd = (fe_globalPrefs.send_html_msg == False);
            MSG_Command(m_pane, commandToMsgCmd(cmd), NULL, 0);
   }
   else if ( (cmd == xfeCmdComposeMessagePlain)
		|| (cmd == xfeCmdComposeArticlePlain) )
   {
            CONTEXT_DATA(m_contextData)->stealth_cmd = (fe_globalPrefs.send_html_msg == True) ;
            MSG_Command(m_pane, commandToMsgCmd(cmd), NULL, 0);
   }
   else if (cmd == xfeCmdCleanUpDisk)
   {
       if (MSG_CleanupNeeded(getMaster()))
       {
           // create progress pane
           XFE_MailDownloadFrame* progressFrame = 
               new XFE_MailDownloadFrame(XfeAncestorFindApplicationShell(getBaseWidget()),
										 /* Tao: we might need to check if this returns a 
										  * non-NULL frame
										  */
                                         ViewGlue_getFrame(m_contextData),
                                         getPane());
           progressFrame->cleanUpNews();
       }
   }
   else 
   {
	XFE_View::doCommand(cmd, calldata, info);
   }


}

MSG_BIFF_STATE
XFE_MNView::getBiffState()
{
  return m_biffstate;
}

void
XFE_MNView::setBiffState(MSG_BIFF_STATE state)
{
  m_biffstate = state;
}

//////////////////////////////////////////////////////////////////////////
XFE_CALLBACK_DEFN(XFE_MNView, updateBiffState)(XFE_NotificationCenter*,
						void *, void*cd)
{
  MSG_BIFF_STATE state = (MSG_BIFF_STATE)cd;

  // Update the desktop mail icons
  fe_InitIcons(m_contextData, state);
}
//////////////////////////////////////////////////////////////////////////


extern "C" void
FE_PaneChanged(MSG_Pane *pane, XP_Bool asynchronous,
               MSG_PANE_CHANGED_NOTIFY_CODE code, int32 value)
{

  XFE_MNView *view = (XFE_MNView*)MSG_GetFEData(pane);

  if (view)
    view->paneChanged(asynchronous, code, value);

}

extern "C" void
FE_UpdateBiff(MSG_BIFF_STATE state)
{
  XFE_MNView::setBiffState(state);
  XFE_MozillaApp::theApp()->notifyInterested(XFE_MozillaApp::biffStateChanged,
					     (void*)state);
}

extern "C" void
FE_UpdateToolbar (MWContext *context)
{
  XFE_Frame *f = ViewGlue_getFrame(context);

  if (f)
    f->notifyInterested(XFE_View::chromeNeedsUpdating, NULL);
}

extern "C" void
FE_UpdateCompToolbar(MSG_Pane* comppane)
{
  XFE_MNView *v = (XFE_MNView*)MSG_GetFEData(comppane);

  if (v) {
	  v->updateCompToolbar();
	  v->getToplevel()->notifyInterested(XFE_View::chromeNeedsUpdating, NULL);
  }
}

extern "C" {
    MSG_Pane *fe_showCompose(Widget, Chrome *, MWContext *, MSG_CompositionFields*, const char *, MSG_EditorType editorType,XP_Bool defaultAddressIsNewsgroup);

    MSG_Pane *
    FE_CreateCompositionPane(MWContext* old_context,
                             MSG_CompositionFields* fields,
                             const char* initialText,
                             MSG_EditorType editorType)
    {

      const char *real_addr = FE_UsersMailAddress();
 
      if ( MISC_ValidateReturnAddress(old_context, real_addr) < 0 )
	return NULL;

      MSG_Pane* pane = fe_showCompose(XtParent(CONTEXT_WIDGET(old_context)),
									  NULL,
                                      old_context, /* required for XP to retrieve url info */ 
                                      fields, initialText, editorType, False);

      return pane; /* return NULL would be okay ?! */
    }
}

extern "C" void
FE_DestroyMailCompositionContext(MWContext* context)
{
  XFE_Frame *f = ViewGlue_getFrame(XP_GetNonGridContext(context));
  XFE_ComposeFrame *cf;

  XP_ASSERT (f && f->getType() == FRAME_MAILNEWS_COMPOSE);

  cf = (XFE_ComposeFrame*)f;

  cf->destroyWhenAllConnectionsComplete();
}

extern "C" MWContext*
FE_GetAddressBookContext(MSG_Pane* pane, XP_Bool /*viewnow*/)
{
  return MSG_GetContext(pane);
}

extern "C" const char*
FE_GetFolderDirectory(MWContext* /*context*/)
{
  XP_ASSERT (fe_globalPrefs.mail_directory);
  return fe_globalPrefs.mail_directory;
}

extern "C" void
FE_ListChangeStarting(MSG_Pane* pane, XP_Bool asynchronous,
		      MSG_NOTIFY_CODE notify, MSG_ViewIndex where, int32 num)
{
  XFE_MNListView *list_view;
  XFE_Outliner *outliner;
  //  int numsel;
  //  MSG_ViewIndex *sellist;

  list_view = (XFE_MNListView*)MSG_GetFEData(pane);

  if (!list_view)
    return;

  MSG_PaneType paneType = MSG_GetPaneType(pane);
  if (paneType == MSG_THREADPANE &&
	  (notify == MSG_NotifyNone ||
	   notify == MSG_NotifyScramble ||
	   notify == MSG_NotifyAll)) {
	  ((XFE_ThreadView *) list_view)->
		  listChangeStarting(asynchronous, notify, where, num);
  }/* else if */

  outliner = list_view->getOutliner();

  if (!outliner)
    return;

  outliner->listChangeStarting(asynchronous, notify, where, num, MSG_GetNumLines(pane));
}

void
FE_ListChangeFinished(MSG_Pane* pane, XP_Bool asynchronous,
		      MSG_NOTIFY_CODE notify, MSG_ViewIndex where, int32 num)
{
  XFE_MNListView *list_view;
  XFE_Outliner *outliner;
  //  int numsel;
  //  MSG_ViewIndex *sellist;

  MSG_PaneType paneType = MSG_GetPaneType(pane);
  list_view = (XFE_MNListView*)MSG_GetFEData(pane);

  //
  // restore this quote out: use pane type to prevent mail/news pane 
  // get into this flow wrongfully
  //
  if (!list_view) {
    return;
  }

  if (notify == MSG_NotifyInsertOrDelete) {
	  if (paneType == MSG_ADDRPANE ||
		  paneType == AB_ABPANE ||
		  paneType == AB_PICKERPANE) {
		  list_view->listChangeFinished(asynchronous, notify, where, num);
	  }/* if */
	  else if (paneType == MSG_THREADPANE) {
		  /* we do this hack :
		   * 1. to make our code concise; only threadview need to handle this
		   *    currently.
		   * 2. need to deal with general cases when the time comes!
		   */
		  list_view->listChangeFinished(asynchronous, notify, where, num);
	  }/* else threadPane */
  }/* if */
  else if (paneType == MSG_THREADPANE &&
		   (notify == MSG_NotifyNone ||
			notify == MSG_NotifyChanged ||
			notify == MSG_NotifyScramble ||
			notify == MSG_NotifyAll)) {
	  list_view->listChangeFinished(asynchronous, notify, where, num);
  }/* else if */

  outliner = list_view->getOutliner();

  if (!outliner)
    return;


  outliner->listChangeFinished(asynchronous, notify, where, num, MSG_GetNumLines(pane));
}

MSG_Master *
fe_getMNMaster()
{
  return XFE_MNView::getMaster();
}

static void
fe_incdone(void* closure, XP_Bool result)
{
  fe_mn_incstate* state = (fe_mn_incstate*) closure;
  XFE_Frame *f = ViewGlue_getFrame(state->context);
  XP_ASSERT(f);

  if (result) {
    unlink(state->filename);
    if (state->file) {
      fclose(state->file);
      state->file = NULL;
    }
    if (state->wascrashbox) {
      CommandType cmd = xfeCmdGetNewMessages;
      if (f
	  && f->handlesCommand(cmd))
	f->doCommand(cmd);
    } else {
      FE_UpdateBiff(MSG_BIFF_NoMail);
	  // XXX hack to get the pending command stuff to execute.  We need to fake
	  // an allConnectionsComplete message here, since the backend doesn't notify
	  // us.
	  f->notifyInterested(XFE_Frame::allConnectionsCompleteCallback);
    }
  }
  XP_FREE(state);
}

extern "C" char*
fe_mn_getmailbox(void)
{
  static char* mailbox = NULL;
  if (mailbox == NULL) {
    mailbox = getenv("MAIL");
    if (mailbox && *mailbox)
      {
	/* Bug out if $MAIL is set to something silly: obscenely long,
	   or not an absolute path.  Returning 0 will cause the error
	   message about "please set $MAIL ..." to be shown.
	 */
	if (strlen(mailbox) >= 1024) /* NAME_LEN */
	  mailbox = 0;
	else if (mailbox[0] != '/')
	  mailbox = 0;
	else
	  mailbox = strdup(mailbox);   /* copy result of getenv */
      }
    else
      {
	static const char *heads[] = {
	  "/var/spool/mail",
	  "/usr/spool/mail",
	  "/var/mail",
	  "/usr/mail",
	  0 };
	const char **head = heads;
	while (*head)
	  {
	    struct stat st;
	    if (!stat (*head, &st) && S_ISDIR(st.st_mode))
	      break;
	    head++;
	  }
	if (*head)
	  {
	    char *uid = 0, *name = 0;
	    fe_DefaultUserInfo (&uid, &name, True);
	    if(uid && *uid)
	      mailbox = PR_smprintf("%s/%s", *head, uid);
	    if(uid) XP_FREE(uid);
	    if(name) XP_FREE(name);
	  }
      }
  }
  return mailbox;
}

static XP_Bool
fe_run_movemail (MWContext *context, const char *from, const char *to)
{
  struct sigaction newact, oldact;
  pid_t forked;
  int ac = 0;
  char **av = (char **) malloc (10 * sizeof (char *));
  av [ac++] = strdup (fe_globalPrefs.movemail_program);
  av [ac++] = strdup (from);
  av [ac++] = strdup (to);
  av [ac++] = 0;

  /* Setup signals so that SIGCHLD is ignored as we want to do waitpid */
  sigaction(SIGCHLD, NULL, &oldact);

#if (defined(SOLARIS) && defined(SOLARIS_24) && defined(NS_USE_NATIVE)) || (defined UNIXWARE)
  // Native Solaris2.4, Unixware.
  newact.sa_handler = (void (*)())SIG_DFL;
#elif (defined(SOLARIS) && !defined(NS_USE_NATIVE))
  // Solaris g++
  newact.sa_handler = (void (*)(...))SIG_DFL;
#else
  // Default. Native Solaris 2.5, IRIX, HPUX.  
  newact.sa_handler = SIG_DFL;
#endif

  newact.sa_flags = 0;
  sigfillset(&newact.sa_mask);
  sigaction (SIGCHLD, &newact, NULL);

  errno = 0;
  switch (forked = fork ())
    {
    case -1:
      while (--ac >= 0)
	free (av [ac]);
      free (av);
      /* fork() failed (errno is meaningful.) */
      fe_perror (context, XP_GetString( XFE_COULDNT_FORK_FOR_MOVEMAIL ) );
      /* Reset SIGCHLD handler before returning */
      sigaction (SIGCHLD, &oldact, NULL);
      return FALSE;
    case 0:
      {
	execvp (av[0], av);
	exit (-1);	/* execvp() failed (this exits the child fork.) */
	return FALSE; // not reached, but needed to shut the compiler up.
      }
    default:
      {
	/* This is the "old" process (subproc pid is in `forked'.) */
	char buf [2048];
	int status = 0;
	pid_t waited_pid;

	while (--ac >= 0)
	  free (av [ac]);
	free (av);

	/* wait for the other process (and movemail) to terminate. */
	waited_pid = waitpid (forked, &status, 0);

	/* Reset the SIG CHILD signal handler */
	sigaction(SIGCHLD, &oldact, NULL);

	if (waited_pid <= 0 || waited_pid != forked)
	  {
	    /* We didn't fork properly, or something.  Let's hope errno
	       has a meaningful value... (can it?) */
	    PR_snprintf (buf, sizeof (buf), XP_GetString( XFE_PROBLEMS_EXECUTING ),
		     fe_globalPrefs.movemail_program);
	    fe_perror (context, buf);
	    return FALSE;
	  }

	if (!WIFEXITED (status))
	  {
	    /* Dumped core or was killed or something.  Let's hope errno
	       has a meaningful value... (can it?) */
	    PR_snprintf (buf, sizeof (buf),
		 XP_GetString( XFE_TERMINATED_ABNORMALLY ),
		 fe_globalPrefs.movemail_program);
	    fe_perror (context, buf);
	    return FALSE;
	  }

	if (WEXITSTATUS (status) != 0)
	  {
	    PR_snprintf (buf, sizeof (buf), XP_GetString( XFE_APP_EXITED_WITH_STATUS ),
		     fe_globalPrefs.movemail_program,
		     WEXITSTATUS (status));
	    FE_Alert (context, buf);
	    return FALSE;
	  }
	/* Else, exited with code 0 */
	return TRUE;
      }
    }
}

/* If we're set up to deliver mail/news by running a program rather
   than by talking to SMTP/NNTP, this does it.

   Returns positive if delivery via program was successful;
   Returns negative if delivery failed;
   Returns 0 if delivery was not attempted (in which case we
   should use SMTP/NNTP instead.)

   $NS_MSG_DELIVERY_HOOK names a program which is invoked with one argument,
   a tmp file containing a message.  (Lines are terminated with CRLF.)
   This program is expected to parse the To, CC, BCC, and Newsgroups headers,
   and effect delivery to mail and/or news.  It should exit with status 0
   iff successful.

   #### This really wants to be defined in libmsg, but it wants to
   be able to use fe_perror, so...
 */
extern "C" int
msg_DeliverMessageExternally(MWContext *context, const char *msg_file)
{
  struct sigaction newact, oldact;
  static char *cmd = 0;
  if (!cmd)
    {
      /* The first time we're invoked, look up the command in the
	 environment.  Use "" as the `no command' tag. */
      cmd = getenv("NS_MSG_DELIVERY_HOOK");
      if (!cmd)
	cmd = "";
      else
	cmd = XP_STRDUP(cmd);
    }

  if (!cmd || !*cmd)
    /* No external command -- proceed with "normal" delivery. */
    return 0;
  else
    {
      pid_t forked;
      int ac = 0;
      char **av = (char **) malloc (10 * sizeof (char *));
      av [ac++] = XP_STRDUP (cmd);
      av [ac++] = XP_STRDUP (msg_file);
      av [ac++] = 0;

      /* Setup signals so that SIGCHLD is ignored as we want to do waitpid */
      sigaction(SIGCHLD, NULL, &oldact);

#if (defined(SOLARIS) && defined(SOLARIS_24) && defined(NS_USE_NATIVE)) || (defined UNIXWARE)
  // Native Solaris2.4, Unixware.
  newact.sa_handler = (void (*)())SIG_DFL;
#elif (defined(SOLARIS) && !defined(NS_USE_NATIVE))
  // g++
  newact.sa_handler = (void (*)(...))SIG_DFL;
#else
  // Native Solaris 2.5, IRIX, etc.
  newact.sa_handler = SIG_DFL;
#endif

      newact.sa_flags = 0;
      sigfillset(&newact.sa_mask);
      sigaction (SIGCHLD, &newact, NULL);

      errno = 0;
      switch (forked = fork ())
	{
	case -1:
	  {
	    while (--ac >= 0)
	      XP_FREE (av [ac]);
	    XP_FREE (av);
	    /* fork() failed (errno is meaningful.) */
	    fe_perror (context, XP_GetString( XFE_COULDNT_FORK_FOR_DELIVERY ));
	    /* Reset SIGCHLD signal hander before returning */
	    sigaction(SIGCHLD, &oldact, NULL);
	    return -1;
	  }
	case 0:
	  {
	    execvp (av[0], av);
	    exit (-1);	/* execvp() failed (this exits the child fork.) */
	    return -1; // not reached, but needed to shut the IRIX 6.2 compiler up
	  }
	default:
	  {
	    /* This is the "old" process (subproc pid is in `forked'.) */
	    char buf [2048];
	    int status = 0;
	    pid_t waited_pid;
	    
	    while (--ac >= 0)
	      XP_FREE (av [ac]);
	    XP_FREE (av);

	    /* wait for the other process to terminate. */
	    waited_pid = waitpid (forked, &status, 0);

	    /* Reset SIG CHILD signal handler */
	    sigaction(SIGCHLD, &oldact, NULL);

	    if (waited_pid <= 0 || waited_pid != forked)
	      {
		/* We didn't fork properly, or something.  Let's hope errno
		   has a meaningful value... (can it?) */
		PR_snprintf (buf, sizeof (buf),
			     XP_GetString( XFE_PROBLEMS_EXECUTING ),
			     cmd);
		fe_perror (context, buf);
		return -1;
	      }

	    if (!WIFEXITED (status))
	      {
		/* Dumped core or was killed or something.  Let's hope errno
		   has a meaningful value... (can it?) */
		PR_snprintf (buf, sizeof (buf),
			     XP_GetString( XFE_TERMINATED_ABNORMALLY ),
			     cmd);
		fe_perror (context, buf);
		return -1;
	      }

	    if (WEXITSTATUS (status) != 0)
	      {
		PR_snprintf (buf, sizeof (buf),
			     XP_GetString( XFE_APP_EXITED_WITH_STATUS ),
			     cmd,
			     WEXITSTATUS (status));
		FE_Alert (context, buf);
		return -1;
	      }
	    /* Else, exited with code 0 */
	    return 1;
	  }
	}
    }
}

extern "C" XP_Bool 
FE_NewsDownloadPrompt(MWContext *context,
					  int32 numMessagesToDownload,
					  XP_Bool *downloadAll)
{
	XFE_Frame *f = ViewGlue_getFrame(XP_GetNonGridContext(context));
	XP_Bool ret_val;
	XFE_NewsPromptDialog *dialog;
	if (!f)
		return False;

	dialog = new XFE_NewsPromptDialog(f->getBaseWidget(), numMessagesToDownload);

	ret_val = dialog->post();

	*downloadAll = dialog->getDownloadAll();

	delete dialog;

	return ret_val;
}

extern "C" MSG_Master*
FE_GetMaster() {
	return fe_getMNMaster();
}

#endif  // MOZ_MAIL_NEWS
