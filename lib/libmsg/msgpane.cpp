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

#include "rosetta.h"
#include "msg.h"
#include "errcode.h"
#include "xpgetstr.h"
#include "libi18n.h"
#include "imapoff.h"

#include "msgpane.h"
#include "msgprefs.h"
#include "msgmast.h"

#include "msgdbvw.h"
#include "msgmpane.h"	// to get static mailto methods.
#include "msgtpane.h"
#include "msgfpane.h"
#include "msgundac.h"
#include "newsdb.h"

#include "xp_time.h"
#include "xplocale.h"
#include "msg.h"
#include "prsembst.h"
#include "msgmast.h"
#include "msgimap.h"
#include "maildb.h"
#include "mailhdr.h"
#include "msgrulet.h"

#include "msgcmfld.h"
#include "newshost.h"
#include "imaphost.h"
#include "hosttbl.h"
#include "nwsartst.h"
#include "grpinfo.h"
#include "msgdlqml.h"
#include "prefapi.h"
#include "listngst.h"
#include "newsset.h"
#include "thrlstst.h"
#include "xp_qsort.h"
#include "msgfcach.h"
#include "intl_csi.h"
#include "xlate.h"
#include "msgurlq.h"
#include "msgbiff.h"
#include "pw_public.h"
#include "mime.h"

extern "C"
{
extern int MK_MSG_ADDRESS_BOOK;
extern int MK_MSG_COMPRESS_ALL_FOLDER;
extern int MK_MSG_EMPTY_TRASH_FOLDER;
extern int MK_MSG_ERROR_WRITING_MAIL_FOLDER;
extern int MK_MSG_GET_NEW_MAIL;
extern int MK_MSG_GET_NEW_DISCUSSION_MSGS;
extern int MK_MSG_NEW_MAIL_MESSAGE;
extern int MK_MSG_NO_POP_HOST;
extern int MK_OUT_OF_MEMORY;
extern int MK_MSG_SAVE_MESSAGE_AS;
extern int MK_MSG_SAVE_MESSAGES_AS;
extern int MK_MSG_OPEN_DRAFT;
extern int MK_MSG_ID_NOT_IN_FOLDER;
extern int MK_MSG_FOLDER_UNREADABLE;
extern int MK_MSG_DELIV_NEW_MSGS;
extern int MK_MSG_QUEUED_DELIVERY_FAILED;
extern int MK_MSG_NEWS_HOST_TABLE_INVALID;
extern int MK_MSG_CANCEL_MESSAGE;
extern int MK_MSG_MESSAGE_CANCELLED;
extern int MK_MSG_MARK_SEL_AS_READ;
extern int MK_MSG_MARK_SEL_AS_UNREAD;
extern int MK_MSG_MARK_THREAD_READ;
extern int MK_MSG_MARK_ALL_READ;
extern int MK_MSG_BACKTRACK;
extern int MK_MSG_GO_FORWARD;
extern int MK_MSG_UNABLE_MANAGE_MAIL_ACCOUNT;
extern int MK_POP3_NO_MESSAGES;
extern int MK_MSG_MANAGE_MAIL_ACCOUNT;
extern int MK_MSG_CANT_DELETE_RESERVED_FOLDER;
extern int MK_MSG_PANES_OPEN_ON_FOLDER;
extern int MK_MSG_DELETE_FOLDER_MESSAGES;
extern int MK_MSG_NO_POST_TO_DIFFERENT_HOSTS_ALLOWED;
extern int MK_MSG_GROUP_NOT_ON_SERVER;
extern int MK_MSG_NEW_NEWSGROUP;
extern int MK_MSG_ADVANCE_TO_NEXT_FOLDER;
extern int MK_MSG_FLAG_MESSAGE;
extern int MK_MSG_UNFLAG_MESSAGE;
extern int MK_MSG_RETRIEVE_FLAGGED;
extern int MK_MSG_RETRIEVE_SELECTED;
XP_Bool NET_IsNewsMessageURL (const char *url);
} 

#ifdef XP_WIN

class MSG_SaveMessagesAsTextState : public MSG_ZapIt
{
public:
	MSG_SaveMessagesAsTextState (MSG_Pane *pane, const IDArray &idArrays, XP_File file);
	~MSG_SaveMessagesAsTextState ();

	static void SaveMsgAsTextComplete(PrintSetup *print);

	void SaveNextMessage();
public:
	int m_curMsgIndex;
	MSG_Pane *m_pane;
	PrintSetup m_print;
	IDArray	m_msgKeys;
};

#endif

PaneListener::PaneListener(MSG_Pane *pPane)
{
	m_pPane = pPane;
	m_keysChanging = FALSE;
	m_keyChanged = FALSE;
}

PaneListener::~PaneListener()
{
}
	
void PaneListener::OnViewChange(MSG_ViewIndex startIndex, int32 numChanged, 
		MSG_NOTIFY_CODE changeType, ChangeListener * /* instigator */)
{
	m_pPane->StartingUpdate(changeType, startIndex, numChanged);
	m_pPane->EndingUpdate(changeType, startIndex, numChanged);
}

void PaneListener::OnViewStartChange(MSG_ViewIndex startIndex, int32 numChanged, 
		MSG_NOTIFY_CODE changeType, ChangeListener * /* instigator */)
{
	m_pPane->StartingUpdate(changeType, startIndex, numChanged);
}

void PaneListener::OnViewEndChange(MSG_ViewIndex startIndex, int32 numChanged, 
		MSG_NOTIFY_CODE changeType, ChangeListener * /* instigator */)
{
	m_pPane->EndingUpdate(changeType, startIndex, numChanged);
}

void PaneListener::OnKeyChange(MessageKey /*keyChanged*/,
									 int32 /*flags*/, 
									 ChangeListener * instigator)
{
	if (m_pPane->GetFolder() != NULL)
		m_pPane->GetFolder()->UpdateSummaryTotals();
	if (!m_keysChanging && this == instigator)
		m_pPane->GetMaster()->BroadcastFolderChanged(m_pPane->GetFolder());
	m_keyChanged = TRUE;

}

void PaneListener::OnAnnouncerGoingAway (ChangeAnnouncer * instigator)
{
	MessageDBView	*view = m_pPane->GetMsgView();

	if (view != NULL && view == instigator)
	{
		view->Remove(this);
		m_pPane->SetMsgView(NULL);
	}
}

void PaneListener::OnAnnouncerChangingView(ChangeAnnouncer* /*instigator*/, MessageDBView* newView)
{
	m_pPane->SwitchView(newView);
}


MSG_Pane* MSG_Pane::MasterList = NULL;



// forward declarations...
static int32
msg_incorporate_handle_line(char* line, uint32 length, void* closure);

#ifndef XP_OS2
static 
#else
extern "OPTLINK"
#endif
int32 msg_writemsg_handle_line(char* line, uint32 length, void* closure);


typedef struct msg_incorporate_state
{
	MWContext				*context;
	MSG_FolderInfoMail	*inbox;
	MSG_Pane		*pane;
	const char* dest;
	const char *destName;
	int32 start_length;
	msg_write_state writestate;
//	int numdup;
	char *ibuffer;
	uint32 ibuffer_size;
	uint32 ibuffer_fp;
#ifdef MANGLE_INTERNAL_ENVELOPE_LINES
	XP_Bool mangle_from;			/* True if "From " lines need to be subject
								   to the Usual Mangling Conventions.*/
#endif /* MANGLE_INTERNAL_ENVELOPE_LINES */
	char* headers;
	uint32 headers_length;
	uint32 headers_maxlength;
	XP_Bool gathering_headers;
	XP_Bool expect_multiple;
	XP_Bool expect_envelope;
	ParseMailboxState *incparsestate; /* Parse state for messages */
	int status;
} msg_incorporate_state;



XP_Bool MSG_Pane::m_warnedInvalidHostTable = FALSE;

MSG_Pane::MSG_Pane(MWContext* context, MSG_Master* master) {
	m_context = context;
	m_nextInMasterList = MasterList;

	if (!MasterList && master->FolderTreeExists())
		XP_FileRemove ("", xpFolderCache);

	MasterList = this;

	m_master = master;
	if (master) {
		m_nextPane = master->GetFirstPane();
		master->SetFirstPane(this);
		m_prefs = master->GetPrefs();
		m_prefs->AddNotify(this);

		if (FALSE == m_warnedInvalidHostTable && NULL == m_master->GetHostTable() &&
			m_master->IsCollabraEnabled())
		{
			FE_Alert (context, XP_GetString(MK_MSG_NEWS_HOST_TABLE_INVALID));
			m_warnedInvalidHostTable = TRUE;
		}

		m_context->mailMaster = master;
	}
	m_numNewGroups = 0;
	m_undoManager = 0;
	m_backtrackManager = 0;
	
	m_ImapFilterData = NULL;
	m_background = NULL;
	m_urlChain = NULL;
	m_progressContext = NULL;
	m_PreImapFolderVerifyUrlExitFunction = NULL;
	m_requestForReturnReceipt = FALSE;
	m_sendingMDNInProgress = FALSE;

	m_displayRecipients = msg_DontKnow;

	m_entryPropSheetFunc = NULL; // FEs must register a person entry property sheet call back
}


MSG_Pane::~MSG_Pane() {
	UnregisterFromPaneList();

	if (m_master) {
		m_master->GetPrefs()->RemoveNotify(this);
	}

	if (m_undoManager)
	{
		m_undoManager->Release();
		m_undoManager = NULL;
	}

	if (m_backtrackManager)
		delete m_backtrackManager;
	
	if (m_ImapFilterData)
		delete m_ImapFilterData;

	if (NULL == m_master->GetFirstPane())
	{
		// write out folder cache since we're the last pane to die.
		MSG_FolderCache cache;
		cache.WriteToDisk (GetMaster()->GetFolderTree());

		m_master->CloseCachedImapConnections();
	}

	if (m_actionInfo)
	{
		// make sure to close down the cached draft imap connection
		if (m_actionInfo->m_folderInfo && 
			m_actionInfo->m_folderInfo->GetType() == FOLDER_IMAPMAIL &&
			m_actionInfo->m_folderInfo->GetFlags() & MSG_FOLDER_FLAG_DRAFTS &&
			!m_master->FindPaneOfType(m_actionInfo->m_folderInfo, MSG_THREADPANE))
			m_master->ImapFolderClosed(m_actionInfo->m_folderInfo);

		delete m_actionInfo;
	}

	FREEIF(m_incUidl);

	if (m_progressContext)
		PW_DestroyProgressContext(m_progressContext);
}


MSG_Pane* MSG_Pane::GetFirstPaneForContext(MWContext *context)
{

	if (context)
		return GetNextPaneForContext(NULL, context);
	
	return(NULL);
}

MSG_Pane* MSG_Pane::GetNextPaneForContext(MSG_Pane *startPane, MWContext *context)
{
	MSG_Pane* result = NULL;
	result = (startPane) ? startPane->m_nextInMasterList : MasterList;

	for (; result ; result = result->m_nextInMasterList) 
	{
		if (result->GetContext() == context)
			return result;
	}
	return NULL;
}


// Remove a pane from the pane list
// Note that if after we remove ourselves from the list, the only pane left
// belongs to the Biff (Check for New Mail) Master then we tell it to go away, which will cause
// its own hidden progress window and context to be deleted.

void MSG_Pane::UnregisterFromPaneList()
{
	if (m_master) {
		MSG_Pane* tmp = m_master->GetFirstPane();
		if (tmp == this) {
			m_master->SetFirstPane(m_nextPane);
		} else {
			for (; tmp ; tmp = tmp->m_nextPane) {
				if (tmp->m_nextPane == this) {
					tmp->m_nextPane = m_nextPane;
					break;
				}
			}
		}
		tmp = m_master->GetFirstPane();		// Check and see if biff is the only one left
		if (tmp && !tmp->m_nextPane && (MSG_Biff_Master::GetPane() == tmp))
		{
			MSG_BiffCleanupContext(NULL);	// will use its own context and recurse here once
		}
	}

	MSG_Pane** ptr;
	for (ptr = &MasterList ; *ptr ; ptr = &((*ptr)->m_nextInMasterList)) {
		if (*ptr == this) {
			*ptr = this->m_nextInMasterList;
			break;
		}
	}
}

// this method can be used to find out if a pane has been deleted...
/*static*/ XP_Bool MSG_Pane::PaneInMasterList(MSG_Pane *pane)
{
	MSG_Pane* curPane;
	XP_Bool ret = FALSE;
	// it will return FALSE if pane is NULL
	for (curPane = MasterList ; curPane ; curPane = curPane->m_nextInMasterList) 
	{
		if (curPane == pane)
		{
			ret = TRUE;
			break;
		}
	}
	return ret;
}


MSG_Pane* MSG_Pane::FindPane(MWContext* context, MSG_PaneType type, XP_Bool contextMustMatch /* = FALSE */) {
  MSG_Pane* result;
  for (result = MasterList ; result ; result = result->m_nextInMasterList) {
	if (result->GetContext() == context && (type == MSG_ANYPANE ||
											result->GetPaneType() == type)) {
	  return result;
	}
  }
  if (!contextMustMatch)
  {
	  for (result = MasterList ; result ; result = result->m_nextInMasterList) {
		if (type == MSG_ANYPANE || result->GetPaneType() == type) {
		  return result;
		}
	  }
  }
  return NULL;
}

/* static */ MSG_PaneType MSG_Pane::PaneTypeForURL(const char *url)
{
	MSG_PaneType retType = MSG_ANYPANE;
	int urlType = NET_URL_Type(url);
	char *idStr;
	char *imapFetchStr;
	char *folderName = NULL;

	switch (urlType)
	{
		case IMAP_TYPE_URL:
			if (!XP_STRCMP(url, "IMAP:"))
				return MSG_FOLDERPANE;
			folderName = NET_ParseURL(url, GET_PATH_PART);
			if (!folderName)	// if it's just a host, return folder pane
			{
				retType = MSG_FOLDERPANE;
				break;
			}
			// note fall through...
		case MAILBOX_TYPE_URL:
			if (!XP_STRCMP(url, "mailbox:"))
				return MSG_FOLDERPANE;
			idStr =  XP_STRSTR (url, "?id=");
			imapFetchStr = XP_STRSTR(url, "?fetch");
			retType = (idStr || imapFetchStr) ? MSG_MESSAGEPANE : MSG_THREADPANE;
			break;
		case NEWS_TYPE_URL:
		{
			if (!XP_STRCMP(url, "news:") || !XP_STRCMP(url, "snews:"))
				return MSG_FOLDERPANE;
			// check if we have news://news/<article id>
			if (NET_IsNewsMessageURL (url))
				retType = MSG_MESSAGEPANE;
			// MSG_RequiresNewsWindow means thread pane...
			else if (MSG_RequiresNewsWindow(url))
				retType = MSG_THREADPANE;
			break;
		}
		case MAILTO_TYPE_URL:
			retType = MSG_COMPOSITIONPANE;
			break;
		default:
			break;
	}
	FREEIF(folderName);
	return retType;
}

// is the passed motion type one that causes us to open the next folder with
// unread messages?
XP_Bool MSG_Pane::NavigationGoesToNextFolder(MSG_MotionType motionType)
{
	return (motionType == MSG_NextUnreadMessage || motionType == MSG_NextUnreadThread
			|| motionType == MSG_NextUnreadGroup 
			|| motionType == MSG_ReadMore  || motionType == MSG_LaterMessage);

}

/* inline virtuals moved to cpp file to help compilers that don't implement virtuals
	defined in headers well.
*/
MSG_PaneType MSG_Pane::GetPaneType() {return MSG_PANE;}
void MSG_Pane::NotifyPrefsChange(NotifyCode /*code*/) {}
MSG_Pane* MSG_Pane::GetParentPane() {return NULL;}
MessageDBView *MSG_Pane::GetMsgView() {return NULL;}
void MSG_Pane::SetMsgView(MessageDBView *) {}
void MSG_Pane::SwitchView(MessageDBView *) {}
MSG_FolderInfo *MSG_Pane::GetFolder() {return NULL;}
void MSG_Pane::SetFolder(MSG_FolderInfo *) {}
PaneListener *MSG_Pane::GetListener() {return NULL;}

void MSG_Pane::SetFEData(void* data) {
	m_fedata = data;
}

void* MSG_Pane::GetFEData() {
	return m_fedata;
}

XP_Bool MSG_Pane::IsLinePane() {
	return FALSE;
}

MWContext* MSG_Pane::GetContext() {
	return m_context;
}

MSG_Prefs* MSG_Pane::GetPrefs() {
	return m_prefs;
}

void MSG_Pane::CrushUpdateLevelToZero()
{
	// assume that any Starting/Ending pairs that are not
	// in the same scope were a wrapper of the form
	// (MSG_NotifyNone, 0, 0);
	// This is a for loop rather than while(m_numstack) to prevent
	// an endless loop if an override of EndingUpdate does not use
	// m_numstack
	for (int updateLevel = m_numstack; updateLevel > 0; updateLevel--)
		EndingUpdate(MSG_NotifyNone, 0, 0);
}

void MSG_Pane::StartingUpdate(MSG_NOTIFY_CODE /*code*/, MSG_ViewIndex /*where*/,
							  int32 /*num*/)
{
#if DEBUG_ricardob
  if (m_numstack > 1)
	  XP_ASSERT(FALSE);
#endif
  m_numstack++;
}

void MSG_Pane::EndingUpdate(MSG_NOTIFY_CODE /*code*/, MSG_ViewIndex /*where*/,
							  int32 /*num*/)
{
  m_numstack--;
}

void PaneListener::StartKeysChanging()
{
	m_keysChanging = TRUE;
	m_keyChanged = FALSE;
}

void PaneListener::EndKeysChanging()
{
	m_keysChanging = FALSE;
	if (m_keyChanged)
	{
		m_keyChanged = FALSE;
		m_pPane->GetMaster()->BroadcastFolderChanged(m_pPane->GetFolder());
	}
}

MsgERR
MSG_Pane::ComposeNewMessage()
{
	return (MSG_Mail(GetContext())) ? 0 : MK_OUT_OF_MEMORY;
}

//This is a long one.  I'd like to break it down but some of the work
//that is done here just isn't really useful elsewhere.  This handles
//serveral selection cases in the threadwindow of the Mail and News pane.
//If the individual selected something which we can use to populate the
//send to lines, it get added into the addressee fields with the group label.
//If nothing of any use was selected, the "mailto" label is used with a blank.
//They are not allowed to select groups across news servers.  Such an act
//will result in an informative error message and bring you back to the
//mail and news pane.  
MsgERR
MSG_Pane::ComposeMessageToMany(MSG_ViewIndex* indices, int32 numIndices)
{
	if (!indices || !numIndices)
	{   //We do this instead because nothing was selected
		return ComposeNewMessage();
	}
	
	if ( !(GetPaneType() == MSG_FOLDERPANE))
	{
		return ComposeNewMessage();
	}

	URL_Struct *url_struct = NULL;
	MSG_FolderLine folderLine;       //temporary storage until put on the array
	MSG_FolderLine *pFolders = NULL; //an array of these things

	MSG_FolderInfo *pFolder = NULL;  
	MSG_FolderInfoNews *pNewsFolderRemember = NULL;
	MSG_FolderInfoNews *pTemp = NULL;
	
	int nIndexToFirstSeenHost =0;
	int i = 0; //index
	char *pHost1 = NULL;
	char *pHost2 = NULL;
	char * pszMailGroupList = NULL;

	char *pURL = NULL;
	HG87729
	XP_Bool sign_p    = MSG_GetNewsSigningPreference();
	char *buffer = NULL; //used for reallocation of strings


	pszMailGroupList = (char*)XP_ALLOC(sizeof '\0');

	if (!pszMailGroupList)//bail! 
		return eOUT_OF_MEMORY;

	pszMailGroupList[0] = '\0';

	pFolders =  new MSG_FolderLine[numIndices];

	if (!pFolders)//bail!
	{
		XP_FREE(pszMailGroupList);
		return eOUT_OF_MEMORY;
	}

	HG83738
	//Get and remember the first folder in the selection.  
	//We do this so we can make sure the selection does not 
	//span over different news hosts.  If it does we complain 
	//and exit message compose.
	for ( i = 0; i < numIndices && !pNewsFolderRemember; i++)
	{
		pFolder = MSG_GetFolderInfo(this, indices[i]);
		if(pFolder)
			pNewsFolderRemember = pFolder->GetNewsFolderInfo(); 
		if (pNewsFolderRemember)
		{
			pHost2 = pNewsFolderRemember->GetHostUrl();
			nIndexToFirstSeenHost  = i;
		}
	}
	//iterate over the indices saving and comparing folderline data.
	//Also we making sure that news groups elected do not span
	//over multiple hosts.
	for (i = nIndexToFirstSeenHost; i < numIndices; i++)
	{
		MSG_GetFolderLineByIndex(this,
								indices[i],
								1,
								&folderLine);
		pFolders[i] = folderLine; 
		pFolder = MSG_GetFolderInfo(this, indices[i]);
		if (pFolder)
		{   //we are checking to make sure we have the same host
			//throughout the selection.
			pTemp = pFolder->GetNewsFolderInfo();
			if (pTemp)
			{
				pHost1 = pTemp->GetHostUrl();
				
				if (pHost1 && pHost2)
				{
					if (XP_STRCASECMP(pHost1,pHost2) != 0)
					{
						XP_FREE(pszMailGroupList);
						XP_FREE(pHost1);
						XP_FREE(pHost2);
						return MK_MSG_NO_POST_TO_DIFFERENT_HOSTS_ALLOWED;
					}
				}
				else
				{
					if (pHost2) XP_FREE(pHost2);//being safe!!!
					if (pHost1) XP_FREE(pHost1);//being safe!!!
					XP_FREE(pszMailGroupList);
					return MK_MSG_NO_POST_TO_DIFFERENT_HOSTS_ALLOWED;
				}
				XP_FREE(pHost1);
				pHost1 = NULL;
			}
		}
	}
	
    //Here we are allocating space for the names of groups
	//in the selection.  
	for (i = nIndexToFirstSeenHost; i< numIndices; i++)
	{
		if (pFolders[i].flags & MSG_FOLDER_FLAG_NEWSGROUP)
		{
			int loc = strlen(pszMailGroupList);
			buffer = (char*)XP_REALLOC(pszMailGroupList,strlen(pFolders[i].name) + sizeof ',' + strlen(pszMailGroupList) + sizeof '\0');
			if (!buffer)
			{
				XP_FREE(pszMailGroupList);
				if (pHost2) XP_FREE(pHost2);//being safe!!!
				return eOUT_OF_MEMORY;
			}
			else
			{
				buffer[loc] = '\0';
				pszMailGroupList = buffer;
				XP_STRCAT(pszMailGroupList,pFolders[i].name);
				XP_STRCAT(pszMailGroupList,",");
			}
		}	
	}
	//Remove the trailing comma
	if (pszMailGroupList[XP_STRLEN(pszMailGroupList) -1] == ',')
		pszMailGroupList[XP_STRLEN(pszMailGroupList) -1] = '\0'; 	

	pURL = MakeMailto (0, 0, pszMailGroupList,
					   0, 0, 0, pHost2,
					   HG92192, sign_p);

	//clean up
	XP_FREE(pszMailGroupList);
	if (pHost2) XP_FREE(pHost2);
	delete [] pFolders;

	if(pURL)
	{
		url_struct = NET_CreateURLStruct (pURL, NET_NORMAL_RELOAD);	
		XP_FREE(pURL);
	}
	else
	{
		return eOUT_OF_MEMORY;
	}

	if (!url_struct)
	{
		return eOUT_OF_MEMORY;
	}

	url_struct->internal_url = TRUE;
	GetURL (url_struct, FALSE);

	//It's all good!!
	return eSUCCESS; //success!
}




MsgERR
MSG_Pane::ComposeNewsMessage(MSG_FolderInfo *folder)
{
	char *host = 0;
	char *url = 0;
	URL_Struct *url_struct = 0;

	HG21872
	XP_Bool sign_p    = MSG_GetNewsSigningPreference();

	XP_ASSERT (folder);
	if (!folder->IsNews())
		return eFAILURE;
	MSG_FolderInfoNews *newsFolder = (MSG_FolderInfoNews *) folder;

	host = ComputeNewshostArg ();
	url = MakeMailto (0, 0, newsFolder->GetNewsgroupName(),
					  0, 0, 0, host,
					  HG12021, sign_p);
	url_struct = NET_CreateURLStruct (url, NET_NORMAL_RELOAD);
	if (url) 
        XP_FREE(url);

	if (!url_struct) return eOUT_OF_MEMORY;
	url_struct->internal_url = TRUE;
	GetURL (url_struct, FALSE);
	return eSUCCESS;

}


char*
MSG_Pane::CreateForwardSubject(MessageHdrStruct* header)
{
	char *fwd_subject = 0;
	const char *subject = 0;
	char *conv_subject = 0;
	INTL_CharSetInfo c = LO_GetDocumentCharacterSetInfo(GetContext());

	subject = header->m_subject;

	while (XP_IS_SPACE(*subject)) subject++;

	conv_subject = IntlDecodeMimePartIIStr(subject, INTL_GetCSIWinCSID(c), FALSE);
	if (conv_subject == NULL)
		conv_subject = (char *) subject;
	fwd_subject =
		(char *) XP_ALLOC((conv_subject ? XP_STRLEN(conv_subject) : 0) + 20);
	if (!fwd_subject) goto FAIL;

	XP_STRCPY (fwd_subject, "[Fwd: ");
	if (header->m_flags & kHasRe) {
		XP_STRCAT (fwd_subject, "Re: ");
	}
	XP_STRCAT (fwd_subject, conv_subject);
	XP_STRCAT (fwd_subject, "]");

FAIL:
	if (conv_subject != subject) {
		FREEIF(conv_subject);
	}
	return fwd_subject;
}

void
MSG_Pane::InterruptContext(XP_Bool /*safetoo*/)
{
	XP_InterruptContext(m_context);
}

/* static */ void MSG_Pane::SaveMessagesAsCB(MWContext* context, char *file_name,  void *closure)
{
	MSG_Pane	*pane = (MSG_Pane*) closure;
	if (pane)
		XP_ASSERT(pane->m_context == context);
	else
		return;

#ifdef XP_WIN
	char *dot = NULL;

	XP_ASSERT (file_name);
	if (!file_name) return;

	dot = XP_STRRCHR(file_name, '.');

	if (dot && !XP_STRNCASECMP(dot, ".txt", 4))
	{
		XP_ASSERT(pane->m_saveKeys.GetSize() >= 1);
		XP_File theFile = XP_FileOpen(file_name, xpURL,
									  XP_FILE_WRITE_BIN);
		XP_ASSERT(theFile);
		if (!theFile) return;

		int32 width = 76;
		MSG_SaveMessagesAsTextState *saveMsgState = 
				new MSG_SaveMessagesAsTextState(pane, pane->m_saveKeys, theFile);

		XP_ASSERT(saveMsgState);
		if (!saveMsgState)
		{
			XP_FileClose(theFile);
			return;
		}
		saveMsgState->SaveNextMessage();
	}
	else
#endif
	{
		MessageDBView	*view = pane->GetMsgView();
		if (view && view->GetDB() != NULL && pane->GetFolder() != NULL)
			pane->GetFolder()->SaveMessages(&pane->m_saveKeys,
											file_name,
											pane,view->GetDB());
	}
}


MsgERR
MSG_Pane::GetMailForAFolder(MSG_FolderInfo *folder)
{
	return GetNewMail(this, folder);
}


MsgERR
MSG_Pane::DoCommand(MSG_CommandType command, MSG_ViewIndex* indices,
					int32 numIndices)
{
	MsgERR status = 0;
	MessageDBView	*view = GetMsgView();
	MSG_FolderInfo	*folder = GetFolder();
	switch (command) {
	case MSG_DeliverQueuedMessages:
		status = DeliverQueuedMessages();
		break;
	case MSG_GetNewMail:
		status = GetNewMail(this, folder);
		break;
	case MSG_GetNextChunkMessages:
		status = GetNewNewsMessages(this, GetFolder(), FALSE);
		break;
	case MSG_EditAddressBook:
		(void) FE_GetAddressBookContext(this, TRUE);
		break;
	case MSG_CancelMessage:
		if (indices)
			status = CancelMessage(indices[0]);
		break;
	case MSG_MailNew:
		status = ComposeMessageToMany(indices, numIndices);
		break;
    case MSG_NewNewsgroup:
		{
			MSG_FolderInfo *folder = GetFolder();
			XP_ASSERT(NewNewsgroupStatus(folder));
			status = NewNewsgroup (folder, FOLDER_CATEGORYCONTAINER == folder->GetType());
		}
        break;
	case MSG_MarkMessages:
	case MSG_UnmarkMessages:
	case MSG_MarkMessagesRead:
	case MSG_MarkMessagesUnread:
	case MSG_ToggleMessageRead:
	case MSG_Delete:
	case MSG_DeleteNoTrash:
	case MSG_MarkThreadRead:
		{
			UndoMarkChangeListener *undoMarkListener = NULL;

			// since the FE could have constructed the list of indices in
			// any order (e.g. order of discontiguous selection), we have to
			// sort the indices in order to find out which MSG_ViewIndex will
			// be deleted first.
			if (numIndices > 1)
				XP_QSORT (indices, numIndices, sizeof(MSG_ViewIndex), CompareViewIndices);

			if (command == MSG_MarkThreadRead && view) 
			{
				undoMarkListener = new UndoMarkChangeListener(this, GetFolder(), 
															  MSG_MarkMessagesRead);
				if (undoMarkListener)
					view->Add(undoMarkListener);
			}
			else if (command != MSG_Delete && 
					 command != MSG_DeleteNoTrash)
				GetUndoManager()->AddUndoAction(
					new MarkMessageUndoAction(this, command, indices, 
											  numIndices, GetFolder()));
			StartingUpdate(MSG_NotifyNone, 0, 0);
			ApplyCommandToIndices(command, indices, numIndices);
			if (undoMarkListener) 
			{
				view->Remove(undoMarkListener);
				delete undoMarkListener;
			}
			EndingUpdate(MSG_NotifyNone, 0, 0);
		}
		break;
	case MSG_MarkAllRead:
		if (view && view->GetDB() != NULL)
		{
			IDArray thoseMarked;
			UndoMarkChangeListener changeListener(this, GetFolder(), MSG_MarkMessagesRead);
		  // The destructor of UndoMarkChangeListener will add the undoAction
		  // to the undo Manager 
			StartingUpdate(MSG_NotifyNone, 0, 0);
			// Add the changeListener to the view
			view->Add(&changeListener);
			status = view->GetDB()->MarkAllRead(GetContext(), &thoseMarked);
			// Remove the changeListener from the view
			view->Remove(&changeListener);
			EndingUpdate(MSG_NotifyNone, 0, 0);
			
			if (status == 0)
			{
				if (GetFolder()->GetType() == FOLDER_IMAPMAIL)
					((MSG_IMAPFolderInfoMail *) GetFolder())->StoreImapFlags(this, kImapMsgSeenFlag, TRUE, thoseMarked);
			}	
		}
		break;
	case MSG_ToggleThreadKilled:
		return view->ToggleIgnored(indices, numIndices, NULL);
	case MSG_ToggleThreadWatched:
		return view->ToggleWatched(indices, numIndices);
	case MSG_SaveMessagesAs:
		if (view != NULL)
		{
			char *title = (numIndices > 1
				   ? XP_GetString(MK_MSG_SAVE_MESSAGE_AS)
				   : XP_GetString(MK_MSG_SAVE_MESSAGES_AS));

			SaveIndicesAsKeys(indices, numIndices); 
			FE_PromptForFileName (GetContext(), title, 0, /* default_path */
				FALSE, FALSE, (ReadFileNameCallbackFunction) MSG_Pane::SaveMessagesAsCB,
								this);
		}
		break;
	case MSG_OpenMessageAsDraft:
	  status = OpenMessageAsDraft(indices, numIndices);
	  break;
	case MSG_ForwardMessageInline:
		status = OpenMessageAsDraft(indices, numIndices, TRUE);
		break;
	case MSG_RetrieveMarkedMessages:
		if (view != NULL && GetFolder() != NULL)
		{
			if (GetFolder()->IsNews())
			{
				status = DownloadNewsArticlesToNewsDB::SaveMessages(GetContext(), GetFolder(), view->GetDB(), NULL);
			}
			else
			{
				MSG_IMAPFolderInfoMail *imapFolder = GetFolder()->GetIMAPFolderInfoMail();
				status = imapFolder->SaveFlaggedMessagesToDB(this, view->GetDB(), m_saveKeys);
			}
			if (status == MK_CONNECTED)
				status = eSUCCESS;
		}
		break;
	case MSG_RetrieveSelectedMessages:
		if (view != NULL && GetFolder())
		{
			SaveIndicesAsKeys(indices, numIndices); 
			if (GetFolder()->IsNews())
				status = DownloadNewsArticlesToNewsDB::SaveMessages(GetContext(), GetFolder(), view->GetDB(), &m_saveKeys);
			else
			{
				MSG_IMAPFolderInfoMail *imapFolder = GetFolder()->GetIMAPFolderInfoMail();
				status = imapFolder->SaveMessagesToDB(this, view->GetDB(), m_saveKeys);
			}
		}

		break;
	case MSG_Undo:
		if (GetUndoManager()->CanUndo())
			GetUndoManager()->Undo();
		break;
	case MSG_Redo:
		if (GetUndoManager()->CanRedo())
			GetUndoManager()->Redo();
		break;
    case MSG_CompressAllFolders:
		status = CompressAllFolders();
        break;
    case MSG_EmptyTrash:
		status = EmptyTrash(GetFolder());
		break;
	case MSG_ManageMailAccount:
		if (folder && folder->IsMail())
			status = ManageMailAccount(folder);
		break;
  default:
#ifdef DEBUG
	  FE_Alert (GetContext(), "command not implemented");
#endif
	break;
  }
  return status;
}


MsgERR
MSG_Pane::GetCommandStatus(MSG_CommandType command,
						   const MSG_ViewIndex* indices, int32 numIndices,
						   XP_Bool *selectable_pP,
						   MSG_COMMAND_CHECK_STATE *selected_pP,
						   const char **display_stringP,
						   XP_Bool *plural_pP)
{
	const char *display_string = 0;
	MessageDBView	*view = GetMsgView();
	MSG_FolderInfo  *folder = GetFolder();
	MSG_FolderInfoMail *mailFolder = (folder) ? folder->GetMailFolderInfo() : 0;
	XP_Bool plural_p = FALSE;
	XP_Bool selectable_p = FALSE;
	XP_Bool selected_p = FALSE;
	XP_Bool selected_used_p = FALSE;
	MSG_FolderInfoMail *queueFolder;
	MessageDB* db = NULL;
	
	if (view)
		db = view->GetDB();

	switch(command) {

	case MSG_DeliverQueuedMessages:
		display_string = XP_GetString(MK_MSG_DELIV_NEW_MSGS);
		queueFolder = (MSG_FolderInfoMail *) FindFolderOfType(MSG_FOLDER_FLAG_QUEUE);
		selectable_p = FALSE;
		if (queueFolder)
		{
			XP_StatStruct folderst;
			if (!XP_Stat(queueFolder->GetPathname(), &folderst, xpMailFolder))
			{
				// If we keep the db up to date (and send change notification to
				// the folder info, we could use the folder info.
				if (folderst.st_size > 0)
				{
					int32 totalMessages = queueFolder->GetTotalMessages(FALSE);
					if (totalMessages > 0)
						selectable_p = TRUE;
					else if (totalMessages == 0)
						selectable_p = FALSE;
				}
			}
		}
		break;

	case MSG_GetNextChunkMessages:
		// can't set display string because it's based on chunk size
		if (folder && folder->IsNews())	
		{
			MSG_FolderInfoNews *newsFolder = folder->GetNewsFolderInfo();
			if (newsFolder && !newsFolder->GetListNewsGroupState())
				selectable_p = TRUE;	// only enable when it's not downloading
		}
		break;
	case MSG_GetNewMail:
		if (!folder || folder->IsMail())	// if no folder we're in folder pane
		{
			MSG_FolderInfo *inbox = FindFolderOfType(MSG_FOLDER_FLAG_INBOX);
			display_string = XP_GetString(MK_MSG_GET_NEW_MAIL);
			// get new mail valid if the inbox is not locked. If no inbox, 
			// GetNewMail should create one, if FindFolderOfType doesn't?
			// Mail filters will need to check if move to folder filters
			// point at a locked folder.
			selectable_p = (inbox) ? !inbox->IsLocked() : TRUE;
			if (selectable_p)	// disable if this folder is reparsing.
				selectable_p = (mailFolder) ? !mailFolder->GetParseMailboxState() : TRUE;
			if (selectable_p)	// disable if imap load in progress
				selectable_p = GetLoadingImapFolder() == NULL;
			if (selectable_p)	// don't enable get new mail if we're doing something else
				selectable_p = !m_showingProgress;

		}
		else if (folder && folder->IsNews())
		{
			MSG_FolderInfoNews *newsFolder = folder->GetNewsFolderInfo();
			display_string = XP_GetString(MK_MSG_GET_NEW_DISCUSSION_MSGS);
			if (newsFolder && !newsFolder->GetListNewsGroupState())
				selectable_p = TRUE;	// only enable when it's not downloading
		}
		else
			selectable_p = TRUE;

		// The beta timebomb is supposed to disable GetNewMail
		if (selectable_p)
			selectable_p = !NET_CheckForTimeBomb(GetContext());
		break;
	case MSG_NewNewsgroup:
		display_string = XP_GetString(MK_MSG_NEW_NEWSGROUP);
		selectable_p = GetFolder() && NewNewsgroupStatus (GetFolder());
		break;
	case MSG_EditAddressBook:
		display_string = XP_GetString(MK_MSG_ADDRESS_BOOK);
		selectable_p = TRUE;
	break;

	case MSG_CancelMessage:
		selectable_p = (view && view->GetDB() && view->GetDB()->GetNewsDB() && numIndices == 1);
		display_string = XP_GetString(MK_MSG_CANCEL_MESSAGE);
		break;
	case MSG_MailNew:
		display_string = XP_GetString(MK_MSG_NEW_MAIL_MESSAGE);
		// don't enable compose if we're parsing a folder
		selectable_p = (mailFolder) ? !mailFolder->GetParseMailboxState() : TRUE;
		break;
	case MSG_MarkAllRead:
		display_string = XP_GetString(MK_MSG_MARK_ALL_READ);
		selectable_p =  view && (view->GetSize() > 0);
		break;
	case MSG_MarkMessagesRead:
		display_string = XP_GetString(MK_MSG_MARK_SEL_AS_READ);
		selectable_p = FALSE;
		if (view && db)
		{
			for ( ; numIndices > 0 ; numIndices--, indices++)
			{
				XP_Bool isRead;
				if (db->IsRead(view->GetAt(*indices), &isRead) == eSUCCESS) 
				{
					if (!isRead)
					{
						selectable_p = TRUE;
						break;
					}
				}
			}
		}
		break;
	case MSG_MarkMessagesUnread:
		display_string = XP_GetString(MK_MSG_MARK_SEL_AS_UNREAD);
		selectable_p = FALSE;
		if (view && db)
		{
			for ( ; numIndices > 0 ; numIndices--, indices++) 
			{
				XP_Bool isRead;
				if (db->IsRead(view->GetAt(*indices), &isRead) == eSUCCESS) 
				{
					if (isRead)
					{
						selectable_p = TRUE;
						break;
					}
				}
			}
		}
		break;
	case MSG_MarkThreadRead:
		display_string = XP_GetString(MK_MSG_MARK_THREAD_READ);
		selectable_p = view && (numIndices == 1);
		break;
	case MSG_ToggleThreadKilled:
	case MSG_ToggleThreadWatched:
		selectable_p = view && (numIndices == 1);
		break;
	case MSG_UnmarkMessages:
	case MSG_MarkMessages:
		selectable_p = FALSE;
		display_string = XP_GetString((command == MSG_MarkMessages) 
			? MK_MSG_FLAG_MESSAGE : MK_MSG_UNFLAG_MESSAGE);
		for ( ; numIndices > 0 ; numIndices--, indices++) 
		{
			XP_Bool isMarked;	// could check the view's flag array...
			if ((view && db) && db->IsMarked(view->GetAt(*indices), &isMarked) == eSUCCESS) 
			{
				if ((command == MSG_UnmarkMessages) ? isMarked : !isMarked)
				{
					selectable_p = TRUE;
					break;
				}
			}
		}
		break;
	case MSG_OpenMessageAsDraft:
#ifdef XP_MAC
		if (folder != NULL && folder->IsNews())
			break;
#endif
	case MSG_ForwardMessageInline:
	case MSG_SaveMessagesAs:
		// ### dmb add display strings!
		selectable_p = view && (numIndices > 0);
		break;
	case MSG_RetrieveMarkedMessages:
		display_string = XP_GetString(MK_MSG_RETRIEVE_FLAGGED);
		selectable_p = (view != NULL && (folder->IsNews() || folder->GetIMAPFolderInfoMail()));
		break;
	case MSG_RetrieveSelectedMessages:
		display_string = XP_GetString(MK_MSG_RETRIEVE_SELECTED);
		selectable_p = view != NULL && (folder->IsNews() || folder->GetIMAPFolderInfoMail()) && (numIndices > 0);
		break;
	case MSG_Undo:
		if (m_undoManager)
			selectable_p = m_undoManager->CanUndo();
		break;
	case MSG_Redo:
		if (m_undoManager)
			selectable_p = m_undoManager->CanRedo();
		break;
	case MSG_CompressAllFolders:
		display_string = XP_GetString(MK_MSG_COMPRESS_ALL_FOLDER);
		selectable_p = TRUE; /* #### && any_folder_needs_compression */
		break;
	case MSG_EmptyTrash:
		display_string = XP_GetString(MK_MSG_EMPTY_TRASH_FOLDER);
		selectable_p = TRUE; /* #### && trash_needs_emptied */
		break;
	case MSG_ManageMailAccount:
		display_string = XP_GetString(MK_MSG_MANAGE_MAIL_ACCOUNT);
		selectable_p = (folder && folder->IsMail() && folder->HaveAdminUrl(MSG_AdminServer));
		break;
  default:
//	XP_ASSERT(0);
	break;
  }
  if (selectable_pP)
	*selectable_pP = selectable_p;
  if (selected_pP)
  {
    if (selected_used_p)
    {
      if (selected_p)
        *selected_pP = MSG_Checked;
      else
        *selected_pP = MSG_Unchecked;
    }
    else
    {
      *selected_pP = MSG_NotUsed;
    }
  }
  if (display_stringP)
	*display_stringP = display_string;
  if (plural_pP)
	*plural_pP = plural_p;

  return 0;
}

/* static */int MSG_Pane::CompareViewIndices (const void *v1, const void *v2)
{
	MSG_ViewIndex i1 = *(MSG_ViewIndex*) v1;
	MSG_ViewIndex i2 = *(MSG_ViewIndex*) v2;
	return i1 - i2;
}

MsgERR 
MSG_Pane::AddToAddressBook(MSG_CommandType command, MSG_ViewIndex*indices, int32 numIndices, AB_ContainerInfo * destAB)
{
	MsgERR status = 0;
	MessageDBView	*view = GetMsgView();
	if (!view)
		return eFAILURE;

	for (int32 i = 0; i < numIndices; i++)
	{
		switch (command)
		{
		case MSG_AddSender:
			if (status == 0)
				status = view->AddSenderToABByIndex(this, GetContext(), indices[i], (i == numIndices - 1), DisplayingRecipients(), destAB);
				break;
		case MSG_AddAll:
			if (status == 0)
				status = view->AddAllToABByIndex(this, GetContext(), indices[i], (i == numIndices - 1), destAB);
			break;
		default:
			XP_ASSERT(FALSE);
		}
	}

	return status;
}

MsgERR
MSG_Pane::ApplyCommandToIndices(MSG_CommandType command, MSG_ViewIndex* indices,
					int32 numIndices)
{
	MsgERR status = 0;
	IDArray	imapUids;
	MessageDBView	*view = GetMsgView();

	if (!view)
		return eFAILURE;

	XP_Bool thisIsImapThreadPane = GetFolder()->GetType() == FOLDER_IMAPMAIL;

	if (command == MSG_Delete)
		status = TrashMessages (indices, numIndices);
	else if (command == MSG_DeleteNoTrash)
		status = DeleteMessages(GetFolder(), indices, numIndices);
	else
	{
		for (int32 i = 0; i < numIndices; i++)
		{
			if (thisIsImapThreadPane)
				imapUids.Add(view->GetAt(indices[i]));
			
			switch (command)
			{
			case MSG_MarkMessagesRead:
				status = view->SetReadByIndex(indices[i], TRUE);
				break;
			case MSG_MarkMessagesUnread:
				status = view->SetReadByIndex(indices[i], FALSE);
				break;
			case MSG_ToggleMessageRead:
				status = view->ToggleReadByIndex(indices[i]);
				break;
			case MSG_MarkMessages:
				status = view->MarkMarkedByIndex(indices[i], TRUE);
				break;
			case MSG_UnmarkMessages:
				status = view->MarkMarkedByIndex(indices[i], FALSE);
				break;
			case MSG_MarkThreadRead:
				status = view->SetThreadOfMsgReadByIndex(indices[i], TRUE);
				break;
			case MSG_AddSender:
				if (status == 0)
					status = view->AddSenderToABByIndex(GetContext(), indices[i], (i == numIndices - 1), DisplayingRecipients());
				break;
			case MSG_AddAll:
				if (status == 0)
					status = view->AddAllToABByIndex(GetContext(), indices[i], (i == numIndices - 1));
				break;
			default:
				XP_ASSERT(FALSE);
				break;
			}
		}
		
		if (thisIsImapThreadPane)
		{
			imapMessageFlagsType flags = kNoImapMsgFlag;
			XP_Bool addFlags = FALSE;
			XP_Bool isRead = FALSE;

			switch (command)
			{
			case MSG_MarkMessagesRead:
				flags |= kImapMsgSeenFlag;
				addFlags = TRUE;
				break;
			case MSG_MarkMessagesUnread:
				flags |= kImapMsgSeenFlag;
				addFlags = FALSE;
				break;
			case MSG_ToggleMessageRead:
				{
					flags |= kImapMsgSeenFlag;
					view->GetDB()->IsRead(view->GetAt(indices[0]), &isRead);
					if (isRead)
						addFlags = TRUE;
					else
						addFlags = FALSE;
				}
				break;
			case MSG_MarkMessages:
				flags |= kImapMsgFlaggedFlag;
				addFlags = TRUE;
				break;
			case MSG_UnmarkMessages:
				flags |= kImapMsgFlaggedFlag;
				addFlags = FALSE;
				break;
			default:
				break;
			}
			
			if (flags != kNoImapMsgFlag)	// can't get here without thisIsImapThreadPane == TRUE
				((MSG_IMAPFolderInfoMail *) GetFolder())->StoreImapFlags(this, flags, addFlags, imapUids);
			
		}
	}
	return status;
}


MSG_COMMAND_CHECK_STATE
MSG_Pane::GetToggleStatus(MSG_CommandType command, MSG_ViewIndex* indices,
						  int32 numindices)
{
  MSG_COMMAND_CHECK_STATE result = MSG_NotUsed;
  if (GetCommandStatus(command, indices, numindices, NULL, &result,
					   NULL, NULL) != 0) {
	return MSG_NotUsed;
  }
  return result;
}


MsgERR
MSG_Pane::SetToggleStatus(MSG_CommandType command,
						  MSG_ViewIndex* indices, int32 numindices,
						  MSG_COMMAND_CHECK_STATE value)
{
	MsgERR	status = eSUCCESS;

	MSG_COMMAND_CHECK_STATE old = GetToggleStatus(command, indices, numindices);
	if (old == MSG_NotUsed) return eUNKNOWN;
	if (old != value) 
	{
		if ((status = DoCommand(command, indices, numindices)) == eSUCCESS)
		{
			if (GetToggleStatus(command, indices, numindices) != value) 
			{
				XP_ASSERT(0);
				return eUNKNOWN;
			}
		}
	}
	return status;
}

ParseMailboxState *MSG_Pane::GetParseMailboxState(const char *folderName)
{
	MSG_Master *master = GetMaster();
	MSG_FolderInfo	*folder = GetFolder();
	if (folder != NULL && folder->IsMail() && folder->GetMailFolderInfo()->GetPathname() &&
		folderName && !XP_STRCASECMP(folder->GetMailFolderInfo()->GetPathname(), folderName))
		return ((MSG_FolderInfoMail *)folder)->GetParseMailboxState();
	else
		return (master) ? master->GetParseMailboxState(folderName) : 0;
}

int MSG_Pane::BeginOpenFolderSock(const char *folderName,
							  const char *message_id, int32 msgnum,
							  void **folder_ptr)
{
	ParseMailboxState *parseState = GetParseMailboxState(folderName);

	if (parseState != NULL)
	{
		return parseState->BeginOpenFolderSock(folderName, message_id, msgnum, folder_ptr);
	}
	else
	{
//		XP_ASSERT(FALSE);
		return MK_CONNECTED;
	}
}

int MSG_Pane::FinishOpenFolderSock(const char* folderName,
							   const char* message_id,
							   int32 msgnum, void** folder_ptr)
{
	ParseMailboxState *parseState = GetParseMailboxState(folderName);
	if (parseState != NULL)
	{
		return parseState->ParseMoreFolderSock(folderName, message_id, msgnum, folder_ptr);
	}
	else
	{
		return MK_CONNECTED;
	}
}

void MSG_Pane::CloseFolderSock(const char* folderName, const char* message_id,
						   int32 msgnum, void* folder_ptr)
{
	MSG_Master *master = GetMaster();
	ParseMailboxState *parseState = GetParseMailboxState(folderName);
	if (parseState != NULL)
	{
		parseState->CloseFolderSock(folderName, message_id, msgnum, folder_ptr);
		master->ClearParseMailboxState(folderName);
	}
	else
	{
//		XP_ASSERT(FALSE);
	}

}

MSG_ViewIndex MSG_Pane::GetThreadIndexOfMsg(MessageKey key)
{
	MessageDBView *view = GetMsgView();
	if (!view)
		return MSG_VIEWINDEXNONE;
	else
		return view->ThreadIndexOfMsg(key);
}

MsgERR MSG_Pane::GetKeyFromMessageId (const char *message_id, 
                                      MessageKey *outId)
{
	// Since it's relatively expensive to work in char* message ids,
	// convert it to a MessageKey at the earliest opportunity for 
	// better database performance
	MessageDBView *view = GetMsgView();
	if (!view || !view->GetDB()) return eUNKNOWN;

	*outId = view->GetDB()->GetMessageKeyForID (message_id);
	return eSUCCESS;
}

int MSG_Pane::MarkMessageKeyRead(MessageKey key, const char *xref)
{
	int		status = 0;
	MessageDBView *view = GetMsgView();
	if (!view) 
		return -1;
	MessageDB *db = view->GetDB();
	if (db)
	{
		XP_Bool isRead;
		if (db->IsRead(key, &isRead) == eSUCCESS && isRead)
			return status; 
	}

	MSG_ViewIndex unusedIndex;
	StartingUpdate(MSG_NotifyNone, 0, 0);
	MsgERR err = view->SetReadByID (key, TRUE, &unusedIndex);
	status = (err == eSUCCESS) ? 0 : -1; // PHP Need better error code
	// if we're reading a mail message, set the summary file valid,
	// because we've just read a message. 
	if (db && db->GetMailDB())
		db->GetMailDB()->SetSummaryValid();

	MSG_FolderInfoNews *newsFolder = GetFolder() ? GetFolder()->GetNewsFolderInfo() : 0;
	if (xref && newsFolder)
	{
		const char *rest = xref;
		const char *group_start = 0, *group_end = 0;
		while (XP_IS_SPACE (*rest))
			rest++;
		group_start = rest;
		XP_Bool done = FALSE;
		while (!done) 
		{
			char* name;
			uint32 n;
			switch (*rest) 
			{
			case ':':
				group_end = rest;
				rest++;
				break;
			case ',':
			case ' ':
			case 0:
				n = 0;
				if (group_end) 
				{
					unsigned long naturalLong; // %l is 64 bits on OSF1
					sscanf ( group_end+1, "%lu", &naturalLong);
					n = (uint32) naturalLong;
				}
				if (n > 0)
				{
					MSG_FolderInfoNews * crossPostFolder;
					name = (char *) XP_ALLOC (group_end - group_start + 1);
					if (! name) 
					{
						status = MK_OUT_OF_MEMORY;
						done = TRUE;
					}
					XP_MEMCPY (name, group_start, group_end - group_start);
					name[group_end - group_start] = 0;

					crossPostFolder =
						m_master->FindNewsFolder(newsFolder->GetHost(),
												 name, FALSE);
					if (crossPostFolder) 
					{
						crossPostFolder->GetSet()->Add(n);
					}
					XP_FREE (name);
				}

				if (*rest == 0) 
					done = TRUE;
				rest++;
				group_start = rest;
				group_end = 0;
				break;
			default:
				rest++;
				break;
			}
		}
	}
	EndingUpdate(MSG_NotifyNone, 0, 0);
	return status; 
}


MsgERR MSG_Pane::ViewNavigate(MSG_MotionType motion,
						MSG_ViewIndex startIndex, 
						MessageKey * resultKey, MSG_ViewIndex * resultIndex, 
						MSG_ViewIndex * pThreadIndex,
						MSG_FolderInfo ** pFolderInfo)
{
	MsgERR	ret = eSUCCESS;
	MessageDBView	*view = GetMsgView();
	MSG_FolderInfo *folder = GetFolder();
	// *** After enabling the Backtrack feature on a stand alone Message pane,
	// it is possible to ask for the navigate status without a view.
	if (!folder) return eNullView;
	if (view)
	{
		switch (motion)
		{
		case MSG_Back:
			*resultKey = GetBacktrackManager()->GoBack(pFolderInfo);
			if (pFolderInfo && *pFolderInfo)
				*resultIndex = MSG_VIEWINDEXNONE;
			else
				*resultIndex = view->FindKey(*resultKey, TRUE);
			return eSUCCESS;
		case MSG_Forward:
			*resultKey = GetBacktrackManager()->GoForward(pFolderInfo);
			if (pFolderInfo && *pFolderInfo)
				*resultIndex = MSG_VIEWINDEXNONE;
			else
				*resultIndex = view->FindKey(*resultKey, TRUE);
			return eSUCCESS;
		case MSG_NextFolder:
			if (pFolderInfo)
			{
				*pFolderInfo = folder;
				do
				{
					*pFolderInfo = m_master->FindNextFolder(*pFolderInfo);
					if (*pFolderInfo && (*pFolderInfo)->IsNews())
					{
						MSG_FolderInfoNews *newsFolder = (*pFolderInfo)->GetNewsFolderInfo();
						if (newsFolder->IsCategory() || !newsFolder->IsSubscribed())
							continue;
					}
					break;
				}
				while (*pFolderInfo);
			}
			*resultIndex = MSG_VIEWINDEXNONE;
			return eSUCCESS;
		case MSG_NextUnreadGroup:
			// It would be better if this happened in the background...
			if (view && view->GetDB())
				view->GetDB()->MarkAllRead(GetContext());
			*resultIndex = MSG_VIEWINDEXNONE;
			*resultKey = MSG_MESSAGEKEYNONE;
			ret = eSUCCESS;
			break;
		default:
			ret = view->Navigate(startIndex, motion, resultKey, 
				resultIndex, pThreadIndex);
		}
	}

	MSG_NCFValue crossFolderPref = GetPrefs()->GetNavCrossesFolders();
	if (motion == MSG_NextUnreadGroup)
		crossFolderPref = MSG_NCFDoIt;

	if (ret == eSUCCESS && resultIndex && *resultIndex == MSG_VIEWINDEXNONE && crossFolderPref != MSG_NCFDont
		&& NavigationGoesToNextFolder(motion))
	{
		MSG_FolderInfo *nextFolderWithUnread = m_master->FindNextFolderWithUnread(folder);
		MSG_FolderInfo *curCategoryContainer = MSG_GetCategoryContainerForCategory(folder);
		if (nextFolderWithUnread && (!curCategoryContainer || curCategoryContainer != MSG_GetCategoryContainerForCategory(nextFolderWithUnread))
			&& crossFolderPref == MSG_NCFPrompt)
		{
			const char *prompt = XP_GetString (MK_MSG_ADVANCE_TO_NEXT_FOLDER);
			char *message = PR_smprintf (prompt, nextFolderWithUnread->GetPrettiestName());
			if (message)
			{
				XP_Bool continueNavigate = FE_Confirm (GetContext(), message);
				XP_FREE(message);
				if (!continueNavigate)
					nextFolderWithUnread = NULL;
			}
		}
		if (pFolderInfo)
			*pFolderInfo = nextFolderWithUnread;
	}
	return ret;
}

MsgERR MSG_Pane::GetNavigateStatus(MSG_MotionType motion, MSG_ViewIndex index, 
							  XP_Bool * selectable_p,
							  const char ** display_string)
{
	MsgERR		ret = eSUCCESS;
	MessageDBView	*view = GetMsgView();
	MSG_FolderInfo	*folder = GetFolder();

	if (!view && selectable_p)
		*selectable_p = FALSE;

	if (view)
	{
		switch (motion) 
		{
		case MSG_Back:
			if (selectable_p)
				*selectable_p = GetBacktrackManager()->CanGoBack();
			if (display_string)
				*display_string = XP_GetString(MK_MSG_BACKTRACK);
			return eSUCCESS;
		case MSG_Forward:
			if (selectable_p)
				*selectable_p = GetBacktrackManager()->CanGoForward();
			if (display_string)
				*display_string = XP_GetString(MK_MSG_GO_FORWARD);
			return eSUCCESS;
		default:
			break;
		}

		ret = view->GetNavigateStatus(motion, index, selectable_p, display_string);
		// 
		if (selectable_p && folder && (folder->GetNumUnread() + folder->GetNumPendingUnread())== 0 && (motion == MSG_NextUnreadMessage || motion == MSG_NextUnreadThread))
			*selectable_p = FALSE;
	}
	// even though Later goes to next folder, enabling it is based on whether there's anything in the view.
	MSG_NCFValue crossFolderPref = GetPrefs()->GetNavCrossesFolders();
	if (motion == MSG_NextUnreadGroup)
		crossFolderPref = MSG_NCFDoIt;

	if (ret == eSUCCESS && selectable_p && !*selectable_p && NavigationGoesToNextFolder(motion) && 
		crossFolderPref != MSG_NCFDont && motion != MSG_LaterMessage)
	{
		MSG_FolderInfo *nextFolderWithUnread = m_master->FindNextFolderWithUnread(folder);
		if (nextFolderWithUnread != NULL)
			*selectable_p = TRUE;
	}
	return ret;
}


/* returns a folder object of the given magic type, creating it if necessary.
   Context must be a mail window. */
MSG_FolderInfo *
MSG_Pane::FindFolderOfType(int type)
{
	MSG_FolderInfo* folder = NULL;
	XP_ASSERT (type == MSG_FOLDER_FLAG_INBOX ||
			 type == MSG_FOLDER_FLAG_TRASH ||
			 type == MSG_FOLDER_FLAG_DRAFTS || 
			 type == MSG_FOLDER_FLAG_QUEUE ||
			 type == MSG_FOLDER_FLAG_SENTMAIL ||
			 type == MSG_FOLDER_FLAG_TEMPLATES);

	int num = GetMaster()->GetFolderTree()->GetFoldersWithFlag(type, &folder, 1);

	// If we didn't find the folder
	if (!folder || num < 1) 
	{
		// should be changed later when we figure out how to find
		// online sent, drafts, templates, etc.
		if (GetMaster()->GetIMAPHostTable())	
		{
			// IMAP folder / server
			// Don't try to create these automatically.  ?
		}
		else
		{
			// Local (POP)
			folder = GetMaster()->FindMagicMailFolder(type, TRUE);
		}

		if (folder)	// folder may not exist - and we don't create yet.
		{
			XP_ASSERT (folder);
			XP_ASSERT (folder->GetFlags() & type);
			XP_ASSERT (folder->GetFlags() & MSG_FOLDER_FLAG_MAIL);
			XP_ASSERT (! (folder->GetFlags() & MSG_FOLDER_FLAG_NEWSGROUP)); 
			XP_ASSERT (! (folder->GetFlags() & MSG_FOLDER_FLAG_NEWS_HOST)); 
		}
	}
	return folder;
}


MSG_FolderInfo*
MSG_Pane::FindMailFolder(const char* pathname, XP_Bool createIfMissing)
{
	return GetMaster()->FindMailFolder(pathname, createIfMissing);
}


MsgERR MSG_Pane::MarkReadByDate (time_t startDate, time_t endDate)
{
	MsgERR	err = eUNKNOWN;  // ### dmb
	IDArray thoseMarked;

	MessageDBView	*view = GetMsgView();
	if (view && view->GetDB())
	{
	  UndoMarkChangeListener changeListener(this, GetFolder(), MSG_MarkMessagesRead);
	  // The destructor of UndoMarkChangeListener will add the undoAction
	  // to the undo Manager
		StartingUpdate(MSG_NotifyNone, 0, 0);
		// Add the changeListener to the view
		view->Add(&changeListener);
		err = view->GetDB()->MarkReadByDate(startDate, endDate, GetContext(), &thoseMarked);
		// Remove the changeListener from the view
		view->Remove(&changeListener);
		EndingUpdate(MSG_NotifyNone, 0, 0);
	}
	
	if (err == 0)
	{
		if (GetFolder()->GetType() == FOLDER_IMAPMAIL)
			((MSG_IMAPFolderInfoMail *) GetFolder())->StoreImapFlags(this, kImapMsgSeenFlag, TRUE, thoseMarked);
	}
	
	return err;	
}


#ifdef XP_UNIX
void
MSG_Pane::IncorporateFromFile(XP_File infid,
									void (* donefunc)(void*, XP_Bool),
									void* doneclosure)
{
	MWContext		*context = GetContext();
	MSG_FolderInfo* folder;
	msg_incorporate_state *state;
	int size;
	char* buf;
	int l;
	int status = 0;

	folder = FindFolderOfType(MSG_FOLDER_FLAG_INBOX);

	XP_ASSERT (folder->IsMail());
	if (!folder->IsMail())
	{
		XP_ASSERT(FALSE);
		return ;	// shouldn't call GetNewMail on non-mail pane.
	}

	msg_InterruptContext(context, FALSE);

	state = CreateIncorporateState ();
	if (!state)
	{
		status = MK_OUT_OF_MEMORY;
		goto FAIL;
	}

	BeginMailDelivery();

	state->expect_envelope = TRUE;
	state->expect_multiple = TRUE;
	state->writestate.inhead = TRUE;

	status = OpenDestFolder(state);
	if (status < 0) 
		goto FAIL;

	size = 512 * 2;
    buf=(char *)NULL;
	do 
	{
		size /= 2;
		buf = (char*)XP_ALLOC(size);
	}
	while (buf == NULL && size > 0);
	if (!buf) 
	{
		status = MK_OUT_OF_MEMORY;
		goto FAIL;
	}

	for (;;) 
	{
		l = XP_FileRead(buf, size, infid);
		if (l < 0) 
		{
		  status = MK_MSG_ERROR_WRITING_MAIL_FOLDER;
		  goto FAIL;
		}
		if (l == 0) break;

		/* Warning: don't call msg_incorporate_stream_write() here, because
		   that uses msg_incorporate_handle_pop_line() instead of
		   msg_incorporate_handle_line(), which will break -- in the pop
		   case, we hack '.' at the beginning of the line, and when
		   incorporating from a file, we don't.
		 */
		l = msg_LineBuffer (buf, l,
							&state->ibuffer,
							&state->ibuffer_size,
							&state->ibuffer_fp,
							TRUE, msg_incorporate_handle_line, state);

		if (l < 0) 
		{
			status = MK_MSG_ERROR_WRITING_MAIL_FOLDER;
			goto FAIL;
		}
	}

	FAIL:
	FREEIF(buf);
	if (status < 0)
		state->status = status;
	status = CloseDestFolder(state);

	if (state->incparsestate) 
	{
		state->incparsestate->DoneParsingFolder();
	}
//	status = msg_end_incorporate (state);
	XP_FREE (state);
	if (status < 0) 
	{
		FE_Alert(context, XP_GetString(status));
	}
	if (donefunc)
		(*donefunc)(doneclosure, status >= 0);
	EndMailDelivery();
}
#endif


MsgERR
MSG_Pane::GetNewNewsMessages(MSG_Pane *parentPane, MSG_FolderInfo *folder, XP_Bool getOld /* = FALSE */)
{
	char *url = folder->BuildUrl(NULL, MSG_MESSAGEKEYNONE);
	if (!url)
		return eOUT_OF_MEMORY;

	MessageDBView	*view = parentPane->GetMsgView();
	MSG_FolderInfoNews *newsFolder = folder->GetNewsFolderInfo();
	XP_ASSERT(newsFolder);
	const char* groupname = newsFolder->GetNewsgroupName();
	MSG_Master *master = GetMaster();
	URL_Struct *url_struct;
	url_struct = NET_CreateURLStruct(url, NET_DONT_RELOAD);
	ListNewsGroupState * listState = new ListNewsGroupState(url, groupname, this);
	listState->SetMaster(master);
	listState->SetView(view);
	listState->SetGetOldMessages(getOld);	// get messages below highwater mark if we don't have them
	newsFolder->SetListNewsGroupState(listState);

	ClearNewInOpenFolders(folder);

	int status = GetURL(url_struct, FALSE);
	if (status == MK_INTERRUPTED || status == MK_OFFLINE)	
	{
		StartingUpdate(MSG_NotifyAll, 0, 0);
		EndingUpdate(MSG_NotifyAll, 0, 0);
		FE_PaneChanged(this, TRUE, MSG_PaneNotifyFolderLoaded, (uint32) folder);
	}
	FREEIF(url);
	return (status >= 0) ? eSUCCESS : status;
}

void MSG_Pane::ClearNewInOpenFolders(MSG_FolderInfo *folderOfGetNewMsgs)
{
	MSG_ThreadPane *eachPane;
	FolderType folderType = folderOfGetNewMsgs->GetType();

	for (eachPane = (MSG_ThreadPane *) m_master->FindFirstPaneOfType(MSG_THREADPANE); eachPane; 
			eachPane =  (MSG_ThreadPane *) m_master->FindNextPaneOfType(eachPane->GetNextPane(), MSG_THREADPANE))
	{
		XP_Bool clearNewBits = FALSE;
		MSG_FolderInfo *curFolder = eachPane->GetFolder();

		if (!curFolder)	// FE is clearing current folder - wow!
			continue;

		switch (folderType)
		{
		case FOLDER_MAIL:
			// if both local folders, clear new bits.
			if (curFolder->GetType() == folderType)
				clearNewBits = TRUE;
			break;
		case FOLDER_IMAPMAIL:
			{
				MSG_IMAPFolderInfoMail *curIMAPFolder = curFolder->GetIMAPFolderInfoMail();
				MSG_IMAPFolderInfoMail *imapFolderOfGetNewMsgs = folderOfGetNewMsgs->GetIMAPFolderInfoMail();

				if (curIMAPFolder && imapFolderOfGetNewMsgs)
				{
					// if both are imap folders on the same host, clear new bits.
					// if public folder, only clear new bit on that folder;
					if (curIMAPFolder->GetIMAPHost() == imapFolderOfGetNewMsgs->GetIMAPHost())
					{
						if (imapFolderOfGetNewMsgs->GetFlags() & MSG_FOLDER_FLAG_IMAP_PUBLIC)
							clearNewBits = (curFolder == folderOfGetNewMsgs);
						else
							clearNewBits = TRUE;
					}
				}
				break;
			}
		case FOLDER_NEWSGROUP:
			clearNewBits = (curFolder == folderOfGetNewMsgs);
			break;
		default:
			clearNewBits = FALSE;
			break;
		}
		if (clearNewBits)
		    eachPane->ClearNewBits(TRUE);
	}
}

MsgERR
MSG_Pane::GetNewMail(MSG_Pane* /*parentPane*/, MSG_FolderInfo *folder) 
{
	char *url;
	URL_Struct *url_struct;
	MWContext		*context = GetContext();
	
	if (NET_IsOffline())	// let master go online, so IMAP gets a chance.
	{
		m_master->SynchronizeOffline(this, FALSE, TRUE, TRUE, FALSE, FALSE);
		return eSUCCESS;
	}
	if (!MSG_Biff_Master::NikiCallingGetNewMail())
		MSG_SetBiffStateAndUpdateFE(MSG_BIFF_NoMail);	// The user is clicking get mail, so clear notification

	MSG_FolderInfo *folderToUpdate = NULL;
	MSG_FolderInfoMail *mailFolder = NULL;
	MSG_IMAPFolderInfoMail *imapFolder = NULL;

	if (folder)
	{
		MSG_IMAPFolderInfoContainer	*imapContainer = NULL;

		imapFolder = folder->GetIMAPFolderInfoMail();
		if (imapFolder)
		{
			// get new mail on public folder just updates that folder
			if (imapFolder->GetFlags() & MSG_FOLDER_FLAG_IMAP_PUBLIC)
				folderToUpdate = imapFolder;
			else
				imapContainer = imapFolder->GetIMAPContainer();
		}
		else if (folder->GetType() == FOLDER_IMAPSERVERCONTAINER)
			imapContainer = (MSG_IMAPFolderInfoContainer *) folder;

		if (imapContainer)
			imapContainer->GetFoldersWithFlag(MSG_FOLDER_FLAG_INBOX, &folderToUpdate, 1);
	}

	if (!folderToUpdate)
		folderToUpdate = FindFolderOfType (MSG_FOLDER_FLAG_INBOX);

	if (GetPrefs()->GetMailServerIsIMAP4())
	{			
		if (!folderToUpdate)
		{
			XP_ASSERT(FALSE);
			return 0;
		}
		imapFolder = folderToUpdate->GetIMAPFolderInfoMail(); 

		if (imapFolder)
		{
			if (imapFolder->GetGettingMail() && MSG_Biff_Master::NikiCallingGetNewMail())
				return eSUCCESS;
			ClearNewInOpenFolders(imapFolder);
			imapFolder->StartUpdateOfNewlySelectedFolder(this,FALSE,NULL);
		}
		// SetGettingNewMail gets set in StartUpdateOfNewlyselectedFolder
		return 0;
	}
	
	
	mailFolder = folderToUpdate->GetMailFolderInfo();
	if (mailFolder && mailFolder->GetGettingMail() && MSG_Biff_Master::NikiCallingGetNewMail())
		return eSUCCESS;	// exit if from biff, otherwise we leave progress dialog hanging forever

	const char *host = GetPrefs()->GetPopHost();
	if (!host || !*host)
	{
#ifdef XP_MAC
		FE_EditPreference(PREF_PopHost);
#endif
		return MK_MSG_NO_POP_HOST;
	}
	url = (char *) XP_ALLOC (XP_STRLEN (host) + 10 +
						   (GetUIDL() ? XP_STRLEN(GetUIDL()) + 10 : 0));
	if (!url) 
		return MK_OUT_OF_MEMORY;
	XP_STRCPY (url, "pop3://");
	XP_STRCAT (url, host);
	if (GetUIDL()) {
		XP_STRCAT(url, "?uidl=");
		XP_STRCAT(url, GetUIDL());
	}

	url_struct = NET_CreateURLStruct (url, NET_NORMAL_RELOAD);
	XP_FREE (url);
	if (!url_struct) 
		return MK_OUT_OF_MEMORY;
	url_struct->internal_url = TRUE;

	url_struct->pre_exit_fn = MSG_Pane::GetNewMailExit;

	// For POP, we must be able to write to the Inbox in order to GetNewMail
	if (folderToUpdate->AcquireSemaphore(this) != 0)
	{
		XP_FREE(url_struct);
		return 0;
	}
	if (mailFolder)
		mailFolder->SetGettingMail(TRUE);
	/* If there is a biff context around, interrupt it.  If we're in the
	 middle of doing a biff check, we don't want that to botch up our
	 getting of new mail. */
	msg_InterruptContext(XP_FindContextOfType(context, MWContextBiff), FALSE);
	GetURL (url_struct, FALSE);
	return 0;
}

/* static */
void MSG_Pane::GetNewMailExit (URL_Struct *URL_s, int status, MWContext* /*window_id*/)
{
	if ((status >= 0) && (URL_s->msg_pane != NULL))
	{
		MSG_Pane *pane = URL_s->msg_pane;
		// this url chain is for going on and offline, with get new mail as 
		// part of the process. Normally, it should be NULL.
		if (pane->GetURLChain())
			pane->GetURLChain()->GetNextURL();
	}

	if (URL_s->msg_pane)
	{
		MSG_FolderInfo *inbox = URL_s->msg_pane->FindFolderOfType(MSG_FOLDER_FLAG_INBOX);
		if (inbox)
		{
			inbox->ReleaseSemaphore (URL_s->msg_pane);
			MSG_FolderInfoMail *mailF = NULL;
			mailF = inbox->GetMailFolderInfo();
			if (mailF)
				mailF->SetGettingMail(FALSE);
		}
	}
	else
		XP_ASSERT(FALSE); // we didn't unlock the folder. very bad
}


XP_Bool MSG_Pane::BeginMailDelivery()
{
	// should create parse state.
	MailDB				*mailDB = NULL;
	MSG_FolderInfoMail* inbox;
	if (GetUIDL()) {
		inbox = (MSG_FolderInfoMail*) GetFolder();
		if (!inbox) return FALSE;
	} else {
		inbox = (MSG_FolderInfoMail *) FindFolderOfType(MSG_FOLDER_FLAG_INBOX);
	}
	
	if (!inbox)
	{
		char *inboxName = m_master->GetPrefs()->MagicFolderName (MSG_FOLDER_FLAG_INBOX);
		inbox = (MSG_FolderInfoMail *) FindMailFolder(inboxName, TRUE);
		XP_FREE(inboxName);
	}
	if (!inbox)
		return FALSE;

	ParseNewMailState *parseMailboxState = new ParseNewMailState(m_master, inbox);
	if (parseMailboxState)
	{
		// Tell the FE to open the GetNewMail dialog
		FE_PaneChanged (this, FALSE, MSG_PanePastPasswordCheck, 0);

		parseMailboxState->SetMaster(m_master);
		parseMailboxState->SetFolder(inbox);	// so we can update counts
		parseMailboxState->SetPane(this);

		parseMailboxState->SetIncrementalUpdate(GetUIDL() == NULL);
		if (GetUIDL()) {
			parseMailboxState->DisableFilters();
		}
		if (MailDB::Open(parseMailboxState->GetMailboxName(), FALSE, &mailDB) != eSUCCESS)
		{
			XP_FileType tmptype;
			char * tmpdbName = FE_GetTempFileFor(GetContext(), parseMailboxState->GetMailboxName(), xpMailFolderSummary,
				&tmptype);
			if (tmpdbName)
				if (MailDB::Open(tmpdbName, TRUE, &mailDB, TRUE) == eSUCCESS)
					parseMailboxState->SetUsingTempDB(TRUE, tmpdbName);
		}

		parseMailboxState->SetDB(mailDB);

		parseMailboxState->SetContext(GetContext());
	//		parseMailboxState->SetView(GetMsgView());
		m_master->SetParseMailboxState(parseMailboxState);
	}
	else
		return FALSE;

	return TRUE;
}


void	MSG_Pane::EndMailDelivery()
{
	m_master->ClearParseMailboxState();
}


void*
MSG_Pane::IncorporateBegin(FO_Present_Types /*format_out*/,
								 char* pop3_uidl,
								 URL_Struct* url,
								 uint32 flags)
{
	char *sep = msg_GetDummyEnvelope();
	msg_incorporate_state *state;
	int status;
	state = CreateIncorporateState ();
	if (!state)
		return 0;

	status = OpenDestFolder(state);

	state->writestate.flags = flags;

	/* Write out a dummy mailbox (From_) line, since POP doesn't give us one. */
	if (status >= 0)
		status = msg_incorporate_handle_line (sep, XP_STRLEN (sep), state);

	if (status < 0)
	{
		XP_FREE (state);
		return 0;
	}
	XP_ASSERT(state->mangle_from && state->writestate.inhead);
	state->mangle_from = TRUE;
	state->writestate.inhead = TRUE;
	state->writestate.uidl = (pop3_uidl ? XP_STRDUP (pop3_uidl) : 0);

	if (GetUIDL() && url) {
		// We're going to download the rest of a partial message.  Set things
		// up so that when that finishes, we delete the partial stub and
		// show the full thing.
		if (!url->msg_pane)
			url->msg_pane = this;
		XP_ASSERT(url->msg_pane == this);
		if (url->msg_pane == this) {
			url->pre_exit_fn = MSG_Pane::IncorporateShufflePartial_s;
		}
	}

	return state;
}

#ifdef XP_MAC
XP_File gIncorporateFID = NULL;
const char* gIncorporatePath = NULL;
#endif

msg_incorporate_state *
MSG_Pane::CreateIncorporateState ()
{
	msg_incorporate_state *state;
	MSG_FolderInfoMail* inbox;

	if (GetUIDL()) {
		/* We're filling in a partial message.  Treat the current folder as the
		   "inbox", since that's the only folder we want to "inc" to now.  
		The fact that GetUIDL returns something means our message may be partial,
		but once the state is initialized always check for MSG_FLAG_PARTIAL in the
		state flags instead. We always save UIDLs now. */
		inbox = (MSG_FolderInfoMail*) GetFolder();
	} else {
		inbox = (MSG_FolderInfoMail *) FindFolderOfType(MSG_FOLDER_FLAG_INBOX);
	}

	if (!inbox) 
		return 0;

	state = XP_NEW_ZAP (msg_incorporate_state);
	if (!state) 
		return 0;

	state->context = m_context;
	state->inbox = inbox;
	state->pane = this;

	state->mangle_from = FALSE;
	state->writestate.inhead = FALSE;
	return state;
}

// forward declaration
#ifndef XP_OS2
static
#else
extern "OPTLINK"
#endif
int32 msg_incorporate_handle_pop_line(char* line, uint32 length, void* closure);

MsgERR
MSG_Pane::IncorporateWrite(void*closure, const char*block, int32 length)
{
	msg_incorporate_state *state = (msg_incorporate_state *) closure;
	return msg_LineBuffer (block, length,
						 &state->ibuffer,
						 &state->ibuffer_size,
						 &state->ibuffer_fp,
						 TRUE, msg_incorporate_handle_pop_line, state);
}


MsgERR
MSG_Pane::IncorporateComplete(void* closure)
{
	msg_incorporate_state *state = (msg_incorporate_state *) closure;

	/* better have been a full CRLF on that last line... */
	XP_ASSERT(state && !state->ibuffer_fp);

	const char* dest = state->dest;
	int status = 0;
	status = CloseDestFolder(state);
	if (state->status < 0) 
		status = state->status;

	if (status < 0 && dest)
	{
		XP_FileTruncate(dest, xpMailFolder, state->start_length);
		if (state->incparsestate) 
		{
			state->incparsestate->AbortNewHeader();
			state->incparsestate->DoneParsingFolder();
			if (m_master)
				m_master->ClearParseMailboxState();
			//	  f->force_reparse_hack = TRUE;
			//	  msg_SelectedMailSummaryFileChanged(state->context, NULL);
			//	  msg_SaveMailSummaryChangesNow(state->context);
			//	  f->first_incorporated_offset = 2; /* Magic value### */
		}
	}
	else
	{
		state->incparsestate->SetIncrementalUpdate(TRUE);
		state->incparsestate->DoneParsingFolder();
	}

	FREEIF (state->ibuffer);
	FREEIF (state->writestate.uidl);
	XP_FREE (state);
	return status;
}


MsgERR
MSG_Pane::IncorporateAbort(void* closure, int status)
{
	msg_incorporate_state* state = (msg_incorporate_state*) closure;
	XP_ASSERT(state);
	if (state) state->status = status;
	return IncorporateComplete(closure);
}

void
MSG_Pane::ClearSenderAuthedFlag(void* closure)
{
	msg_incorporate_state* state = (msg_incorporate_state*) closure;
	XP_ASSERT(state);
	if (state)
		state->writestate.flags &= ~MSG_FLAG_SENDER_AUTHED;
}


void
MSG_Pane::IncorporateShufflePartial_s(URL_Struct *url, int status,
									  MWContext *context)
{
	XP_ASSERT(url && url->msg_pane);
	if (url && url->msg_pane) {
		url->msg_pane->IncorporateShufflePartial(url, status, context);
	}
}


void
MSG_Pane::IncorporateShufflePartial(URL_Struct * /* url */, int /* status */,
									MWContext * /* context */)
{
	// The real version of this is in MSG_MessagePane.  This one really
	// ought never get called.
	XP_ASSERT(0);
	FREEIF(m_incUidl);
}

int
MSG_Pane::CloseDestFolder(msg_incorporate_state* state)
{
	int status = 0;
	if (state->dest == NULL) 
		return 0;
	if (state->writestate.fid) {
		if (XP_FileClose(state->writestate.fid) != 0) {
			status = MK_MSG_ERROR_WRITING_MAIL_FOLDER;
			state->status = status;
		}
		state->writestate.fid = 0;
	}

	if (state->writestate.writefunc) {
		state->writestate.writefunc = NULL;
	} else {
		/* Make sure the folder appears in our lists. */
		// ### DMB - This won't do that...but it will create the folder
		(void) FindMailFolder(state->dest, TRUE);

	}
	return status;
}

int
MSG_Pane::OpenDestFolder(msg_incorporate_state* state)
{
	int status;
	XP_StatStruct folderst;
	status = CloseDestFolder(state);
	if (status < 0) 
		return status;

	state->dest = state->inbox->GetPathname();
	state->destName = state->inbox->GetName();
	state->writestate.fid = NULL;

	if (!XP_Stat((char*) state->dest, &folderst, xpMailFolder)) {
		state->writestate.position = folderst.st_size;
	} else {
		state->writestate.position = 0;
	}
	state->start_length = state->writestate.position;
	if (state->writestate.fid == NULL) {
		state->writestate.fid = XP_FileOpen(state->dest,
										xpMailFolder, XP_FILE_APPEND_BIN);
	}
	if (!state->writestate.fid) {
		state->status = MK_MSG_ERROR_WRITING_MAIL_FOLDER;
		return state->status;
	}


  /* Now, if we're in the folder that's going to get this message, then set
	 things up so that as we write new messages out to it, we also send the
	 lines to the parsing code to update our message list. */
	if (state->incparsestate == NULL)
	{
		// ### dmb - hack cast - do we need to add typesafe accessor 
		// in m_master?
		ParseNewMailState *parseMailboxState = 
			(ParseNewMailState *) m_master->GetMailboxParseState();

		state->incparsestate = parseMailboxState;
		parseMailboxState->BeginParsingFolder(state->writestate.position);

		if (state->incparsestate) 
		{
			state->writestate.writefunc = ParseMailboxState::LineBufferCallback;
			state->writestate.writeclosure = state->incparsestate;
		}
	}
	return 0;
}


static int32
msg_incorporate_handle_line(char* line, uint32 length, void* closure)
{
	msg_incorporate_state* state = (msg_incorporate_state*) closure;
	char* line2 = NULL;
	int status;

	if (length > 5 && line[0] == 'F' && XP_STRNCMP(line, "From ", 5) == 0)
	{
		if (
#ifdef MANGLE_INTERNAL_ENVELOPE_LINES
		  !state->mangle_from ||
#endif /* MANGLE_INTERNAL_ENVELOPE_LINES */
		  state->expect_multiple)
		{
			/* This is the envelope, and we should treat it as such. */
			state->writestate.inhead = TRUE;
#ifdef EMIT_CONTENT_LENGTH
		  state->writestate.header_bytes = 0;
#endif /* EMIT_CONTENT_LENGTH */
		  if (!state->expect_multiple) 
			  state->mangle_from = TRUE;
		  state->expect_envelope = FALSE;
		}
#ifdef MANGLE_INTERNAL_ENVELOPE_LINES
		else
		{
		  /* Uh oh, we got a line back from the POP3 server that began with
			 "From ".  This must be a POP3 server that is not sendmail based
			 (maybe it's MMDF, or maybe it's non-Unix.)  We must follow the
			 Usual Mangling Conventions, since we are using the
			 BSD mailbox format, and if we let this line get out to the folder
			 like this, we (or other software) won't be able to parse it later.

			 Note: it is correct to mangle all lines beginning with "From ",
			 not just those that look like parsable message delimiters.
			 Though we might cope, other software won't.
		   */
			line2 = (char *) XP_ALLOC (length + 2);
			if (!line2) {
				status = MK_OUT_OF_MEMORY;
				goto FAIL;
			}
			line2[0] = '>';
			XP_MEMCPY (line2+1, line, length);
			line2[length+1] = 0;
			line = line2;
			length++;
		}
#endif /* MANGLE_INTERNAL_ENVELOPE_LINES */
	}

	if (state->expect_envelope) 
	{
		/* We're doing movemail, we should have received an envelope
		   as the first line, and we didn't.  So make one up... */
		char *sep = msg_GetDummyEnvelope();
		status = msg_incorporate_handle_line(sep, XP_STRLEN(sep), state);
		if (status < 0) 
			return status;
	}

	if (state->writestate.inhead) 
	{
		status = msg_GrowBuffer(state->headers_length + length + 1, sizeof(char),
								1024, &(state->headers),
								&(state->headers_maxlength));
		if (status < 0) 
			goto FAIL;
		XP_MEMCPY(state->headers + state->headers_length, line, length);
		state->headers_length += length;
		if (line[0] == CR || line[0] == LF || length == 0) 
		{
			char *ibuffer = NULL;
			uint32 ibuffer_size = 0;
			uint32 ibuffer_fp = 0;
			state->headers[state->headers_length] = '\0';
			if (state->writestate.fid)
				XP_FileFlush(state->writestate.fid);
			status = msg_LineBuffer(state->headers, state->headers_length, &ibuffer,
								  &ibuffer_size, &ibuffer_fp, FALSE,
								  msg_writemsg_handle_line, &(state->writestate));
			if (status >= 0 && state->writestate.inhead) {
				/* Looks like that last blank line didn't make it to
				   msg_writemsg_handle_line.  This can happen if msg_LineBuffer isn't
				   quite sure that a line ended yet and is waiting for more data.
				   We'll shove another blank line directly to msg_writemsg_handle_line,
				   which ought to fix things up. */
				XP_ASSERT(ibuffer_fp == 1); /* We do have some data that didn't make
										   it out, right? */
				msg_writemsg_handle_line(line, length, &(state->writestate));
				XP_ASSERT(!state->writestate.inhead); /* It got the blank line this
													 time, right? */
			}
			FREEIF(ibuffer);
			FREEIF(state->headers);
			state->headers_length = 0;
			state->headers_maxlength = 0;
		}
	} 
	else
	{
		status = msg_writemsg_handle_line(line, length, &(state->writestate));
	}
FAIL:
	FREEIF(line2);
	return status;
}


#ifndef XP_OS2
static
#else
extern "OPTLINK"
#endif

int32 msg_incorporate_handle_pop_line(char* line, uint32 length, void* closure)
{
	if (!NET_POP3TooEarlyForEnd(0) && (length > 0 && line[0] == '.'))
	{
		/* Take off the first dot. */
		line++;
		length--;

		if (length == LINEBREAK_LEN &&
		  !XP_STRNCMP (line, LINEBREAK, LINEBREAK_LEN))
			/* This line contained only a single dot, which means it was
			   the "end of message" marker.  This means this function will
			   not be called again for this message (it better not!  That
			   would mean something wasn't properly quoting lines.)
			 */
			return 0;
	}
	return msg_incorporate_handle_line(line, length, closure);
}

#ifndef XP_OS2
static 
#else
extern "OPTLINK"
#endif
int32 msg_writemsg_handle_line(char* line, uint32 length, void* closure)
{
	msg_write_state* state = (msg_write_state*) closure;
	char* buf = 0;
	uint32 buflength;
	if (state->numskip > 0) {
		state->numskip--;
		return 0;
	}
	if (state->inhead) {

		if (line[0] == CR || line[0] == LF || length == 0) {
  
			/* If we're at the end of the headers block, write out the
			 X-Mozilla-Status and X-UIDL headers.
			*/
			uint16 flags = 0;
			XP_Bool write_status_header = TRUE; // ###DMB: was FALSE

			// remember this to set statusOffset in DB.
			state->statusPosition = state->position;

			flags = state->flags;

			if (write_status_header)
			{
				buf = PR_smprintf(X_MOZILLA_STATUS_FORMAT LINEBREAK, flags);
				if (buf) {
					buflength = X_MOZILLA_STATUS_LEN + 6 + LINEBREAK_LEN;
					XP_ASSERT(buflength == XP_STRLEN(buf));
					/* don't increment state->header_bytes here; that applies to
					the headers in the old file, not the new one. */
					if (XP_FileWrite(buf, buflength, state->fid) < buflength) {
						XP_FREE(buf);
						return MK_MSG_ERROR_WRITING_MAIL_FOLDER;
					}
					if (state->writefunc) {
						(*state->writefunc)(buf, buflength, state->writeclosure);
					}
					state->position += buflength;
					XP_FREEIF(buf);
				}
				// 
				uint32 flags2 = (state->flags &
								 (MSG_FLAG_MDN_REPORT_NEEDED |
								  MSG_FLAG_MDN_REPORT_SENT |
								  MSG_FLAG_TEMPLATE));
				buf = PR_smprintf(X_MOZILLA_STATUS2_FORMAT LINEBREAK,
								  flags2);
				if (buf)
				{
					buflength = X_MOZILLA_STATUS2_LEN + 10 + LINEBREAK_LEN;
					XP_ASSERT(buflength == XP_STRLEN(buf));
					/* don't increment state->header_bytes here; that applies to
					the headers in the old file, not the new one. */
					if (XP_FileWrite(buf, buflength, state->fid) < buflength) {
						XP_FREE(buf);
						return MK_MSG_ERROR_WRITING_MAIL_FOLDER;
					}
					if (state->writefunc) {
						(*state->writefunc)(buf, buflength, state->writeclosure);
					}
					state->position += buflength;
					XP_FREEIF(buf);
				}
			}

			if (state->uidl)
			{
				/* UIDLs are very useful when leaving messages on the server. */
				buflength = X_UIDL_LEN + 2 + XP_STRLEN (state->uidl) + LINEBREAK_LEN;
				buf = (char*) XP_ALLOC(buflength + 1);
				if (buf) {
					XP_STRCPY (buf, X_UIDL ": ");
					XP_STRCAT (buf, state->uidl);
					XP_STRCAT (buf, LINEBREAK);
					XP_ASSERT(buflength == XP_STRLEN(buf));
					/* don't increment state->header_bytes here; that applies to
					   the headers in the old file, not the new one. */
					if (XP_FileWrite(buf, buflength, state->fid) < buflength) {
						XP_FREE(buf);
						return MK_MSG_ERROR_WRITING_MAIL_FOLDER;
					}
					if (state->writefunc) {
						(*state->writefunc)(buf, buflength, state->writeclosure);
					}
					state->position += buflength;
					XP_FREE(buf);
				}
			}


			/* Now fall through to write out the blank line */
			state->inhead = FALSE;
		}
		else 
		{
			/* If this is the X-Mozilla-Status header, don't write it (since we
			will rewrite it again at the end of the header block.) */
			if (!XP_STRNCMP(line, X_MOZILLA_STATUS, X_MOZILLA_STATUS_LEN)) 
				return 0;
			/* Likewise, if we are writing a UIDL header, discard an existing one.
			(In the case of copying a `partial' message from one folder to
			another, state->uidl will be false, so we will simply copy the
			existing X-UIDL header like it was any other.)
			*/
			if (state->uidl && !XP_STRNCMP(line, X_UIDL, X_UIDL_LEN)) 
				return 0;
		}
	}
	if (XP_FileWrite(line, length, state->fid) < length) {
		return MK_MSG_ERROR_WRITING_MAIL_FOLDER;	/* ### Need to make sure we
													   handle out of disk space
													   properly. */
	}
	if (state->writefunc) {
		(*state->writefunc)(line, length, state->writeclosure);
	}

	state->position += length;
	return 0;
}


MsgERR
MSG_Pane::UpdateNewsCounts(MSG_NewsHost* host)
{
	XP_ASSERT(host);
	if (!host) return eUNKNOWN;

	URL_Struct* url_struct = NET_CreateURLStruct(host->GetURLBase(),
												 NET_DONT_RELOAD);
	if (!url_struct) return eOUT_OF_MEMORY;
	url_struct->pre_exit_fn = MSG_Pane::UpdateNewsCountsDone_s;
	GetURL(url_struct, FALSE);
	return 0;
}


void
MSG_Pane::UpdateNewsCountsDone_s(URL_Struct* url_struct,
								 int status, MWContext*)
{
	XP_ASSERT(url_struct->msg_pane);
	if (url_struct->msg_pane) {
		url_struct->msg_pane->UpdateNewsCountsDone(status);
	}
}

void 
MSG_Pane::UpdateNewsCountsDone(int /*status*/)
{
}

MsgERR MSG_Pane::CloseView()
{
	MessageDBView	*view = GetMsgView();
	if (view != NULL)
	{
		if (GetListener())
			view->Remove(GetListener());
		view->Close();
	}
	SetMsgView(NULL);
	return eSUCCESS;
}


MsgERR MSG_Pane::ListThreads()
{
	MessageDBView	*msgView = GetMsgView();
	MSG_FolderInfo *folder = GetFolder();
	if (!msgView || !folder)
		return eFAILURE;
	MSG_ListThreadState *listThreadState = new MSG_ListThreadState(msgView, this, TRUE, folder->GetTotalMessagesInDB());
	if (listThreadState)
	{
		listThreadState->Begin(this);
		return eSUCCESS;
	}
	else
		return eOUT_OF_MEMORY;
}


int MSG_Pane::GetURL (URL_Struct *url, XP_Bool isSafe)
{
	//###phil should we check that msg_pane is null? who would clear it, a preExit func?
	url->msg_pane = this;
	m_context->imapURLPane = this;
	return msg_GetURL (m_context, url, isSafe);
}

URL_Struct * MSG_Pane::ConstructUrlForMessage(MessageKey key)
{
	MSG_FolderInfo *folder = GetFolder();
	MessageDBView  *view = GetMsgView();
	URL_Struct *retUrl = NULL;

	char *urlStr = NULL;

	if (!folder || ! view || !view->GetDB())
		return NULL;
	if (key == MSG_MESSAGEKEYNONE)
		urlStr = folder->BuildUrl(NULL, key);
	else
		urlStr = folder->BuildUrl(view->GetDB(), key);
	if (urlStr != NULL)
	{
		retUrl = NET_CreateURLStruct(urlStr, NET_DONT_RELOAD);
		XP_FREE(urlStr);
	}
	return retUrl;
}

/* static */ URL_Struct *  MSG_Pane::ConstructUrl(MSG_FolderInfo *folder)
{
	URL_Struct *retUrl = NULL;

	char *urlStr = NULL;

	if (!folder)
		return NULL;
	urlStr = folder->BuildUrl(NULL, MSG_MESSAGEKEYNONE);
	if (urlStr != NULL)
	{
		retUrl = NET_CreateURLStruct(urlStr, NET_DONT_RELOAD);
		XP_FREE(urlStr);
	}
	return retUrl;
}

int MSG_Pane::SaveIndicesAsKeys(MSG_ViewIndex* indices, int32 numIndices)
{
	MessageDBView	*view = GetMsgView();
	if (view == NULL)
		return 0;
	m_saveKeys.RemoveAll();
	for (int32 i = 0; i < numIndices; i++)
	{
		m_saveKeys.Add(view->GetAt(indices[i]));
	}
	return m_saveKeys.GetSize();
}

class FolderRunningIMAPURLStack
{
public:
	FolderRunningIMAPURLStack(MSG_FolderInfo *folder);
	~FolderRunningIMAPURLStack();
protected:
	 MSG_IMAPFolderInfoMail *m_imapFolder;
	 XP_Bool				 m_wasRunningIMAPUrl;
};

FolderRunningIMAPURLStack::FolderRunningIMAPURLStack(MSG_FolderInfo *folder)
{
	m_imapFolder = folder->GetIMAPFolderInfoMail();
	m_wasRunningIMAPUrl = m_imapFolder && (m_imapFolder->GetRunningIMAPUrl() != MSG_NotRunning);

	if (m_imapFolder)
		m_imapFolder->SetRunningIMAPUrl(MSG_RunningOnline);
}

FolderRunningIMAPURLStack::~FolderRunningIMAPURLStack()
{
	if (!m_wasRunningIMAPUrl && m_imapFolder)
		m_imapFolder->SetRunningIMAPUrl(MSG_NotRunning);
}

XP_Bool MSG_Pane::ShouldDeleteInBackground()
{
	XP_Bool ret = FALSE;
	// ideally, this would be virtual 
	if (GetPaneType() == MSG_MESSAGEPANE)
		ret = TRUE;
	else if (GetPaneType() == MSG_THREADPANE)
	{
		// ah, FE's collapsing away the message by hiding it, but not destroying it.
		// So we have to check the message key to see if it's displaying a message.
		// This means we won't background delete if there's a multiple selection...
		MSG_MessagePane *msgPane = (MSG_MessagePane *) MSG_FindPaneOfContext(GetContext(), MSG_MESSAGEPANE);
		if (msgPane)
		{
			MessageKey messageKey = MSG_MESSAGEKEYNONE;
			msgPane->GetCurMessage(NULL, &messageKey, NULL);
			MessageDBView *view = GetMsgView();
			// if we're deleting the last message in the view, don't want to run
			// in background because there might not be another message to load.
			if (messageKey != MSG_MESSAGEKEYNONE && view && view->GetSize() > 1)
				ret = TRUE;
		}
	}
	return ret;
}

MsgERR MSG_Pane::TrashMessages (MSG_ViewIndex *indices, int32 numIndices)
{
	MsgERR err = eSUCCESS;
	MSG_FolderInfo *folder = GetFolder();
	if (!folder)
		return eFAILURE;

	XP_Bool imapDeleteIsMoveToTrash = folder->DeleteIsMoveToTrash();
	if (!(folder->GetFlags() & MSG_FOLDER_FLAG_TRASH) &&
	    !((folder->GetType() == FOLDER_IMAPMAIL) && !imapDeleteIsMoveToTrash) )
	{
		// First, copy these messages into the trash folder, leaving the old
		// headers valid until we're done
		XP_ASSERT (folder->IsMail());
		char *path = NULL;
		MSG_FolderInfoMail *trashFolder = NULL;
		if (folder->GetType() == FOLDER_MAIL)
		{
			path 		= m_master->GetPrefs()->MagicFolderName (MSG_FOLDER_FLAG_TRASH);
			trashFolder = m_master->FindMailFolder (path, TRUE);
		}
		else
		{
			MSG_IMAPFolderInfoMail *imapInfo = folder->GetIMAPFolderInfoMail();
			if (imapInfo)
			{
				MSG_FolderInfo *foundTrash = MSG_GetTrashFolderForHost(imapInfo->GetIMAPHost());
				trashFolder = foundTrash ? foundTrash->GetMailFolderInfo() : (MSG_FolderInfoMail *)NULL;
			}
		}
			
		FREEIF(path);
		if (NULL != trashFolder)
		{
			if (ShouldDeleteInBackground())
			{
				FolderRunningIMAPURLStack folderStack(folder);
				err = CopyMessages (indices, numIndices, trashFolder, TRUE);
			}
			else
				err = CopyMessages (indices, numIndices, trashFolder, TRUE);
		}
	}
	else if (folder)	// must be deleting from trash. So do it for real.
	{
		err = DeleteMessages(folder, indices, numIndices);
		// else what other folder type here?
	}
	
	return err;
}

MsgERR MSG_Pane::DeleteMessages (MSG_FolderInfo *folder, MSG_ViewIndex *indices, int32 numIndices)
{
	MessageDBView *view = GetMsgView();
	if (!view)
		return eFAILURE;
	if (folder->GetType() == FOLDER_MAIL)
		return view->DeleteMessagesByIndex(indices, numIndices, TRUE /* delete from db */);
	else if (folder->GetType() == FOLDER_IMAPMAIL)
	{
		// translate the view indices to msg keys
		IDArray keyArray;
		for (int32 currentIndexIndex = 0; currentIndexIndex < numIndices; currentIndexIndex++)
			keyArray.Add(view->GetAt(indices[currentIndexIndex]));
		
		// the folder will delete them asynch
		((MSG_IMAPFolderInfoMail *) folder)->DeleteSpecifiedMessages(this, keyArray);
	}
	return eSUCCESS;
}

/*static*/ void MSG_Pane::CancelDone (URL_Struct *url, int status, MWContext *context)
{
	if (status >= 0)
	{
		MSG_Pane	*pane = url->msg_pane;
		FE_Alert (context, XP_GetString (MK_MSG_MESSAGE_CANCELLED));
		if (pane != NULL)
		{
			MessageKey key = MSG_MESSAGEKEYNONE;
			char *idPart = NewsGroupDB::GetGroupNameFromURL(url->address);
			if (idPart)
			{
				char *msgId = XP_STRDUP (NET_UnEscape (idPart));

				if (msgId && MSG_GetKeyFromMessageId(pane, msgId, &key) == 0)
				{
					MessageDBView *view = pane->GetMsgView();
					if (view && view->GetDB())
						view->GetDB()->DeleteMessage(key, NULL, TRUE /* delete from db */);
					FREEIF(msgId);
				}
				FREEIF(idPart);
			}

		}
	}
  /* else, the FE exit routine will present the error message. */
}
int MSG_Pane::CancelMessage(MSG_ViewIndex index)
{
	URL_Struct *url_struct;
 /* Get the message ID of the current message, and open a URL of the form
	 "news://host/message@id?cancel" and mknews.c will do all the work. */
	MSG_FolderInfo *folder = GetFolder();
	if (!folder)
		return -1;
	MSG_FolderInfoNews *newsFolder = folder->GetNewsFolderInfo();
	if (!newsFolder)
		return -1;
	MessageDBView	*view = GetMsgView();
	if (!view)
		return -1;
	char *msgUrl = newsFolder->BuildUrl(view->GetDB(), view->GetAt(index));
	if (!msgUrl)
		return MK_OUT_OF_MEMORY;
	msgUrl = XP_AppendStr(msgUrl, "?cancel");
	if (!msgUrl)
		return MK_OUT_OF_MEMORY;
	/* use NET_NORMAL_RELOAD so we don't fail to
	 cancel messages that are in the cache */
	url_struct = NET_CreateURLStruct (msgUrl, NET_NORMAL_RELOAD);
	XP_FREE (msgUrl);
	if (!url_struct)
		return MK_OUT_OF_MEMORY;
	url_struct->internal_url = TRUE;
	url_struct->pre_exit_fn = MSG_Pane::CancelDone;
	return GetURL (url_struct, FALSE);
}


// Helper function for copy and move
MSG_DragEffect MSG_Pane::DragMessagesStatus (
                     const MSG_ViewIndex * indices,
                     int32 numIndices,
                     const char * folderPath,
					 MSG_DragEffect request)
{
	if (!folderPath)
	 	return MSG_Drag_Not_Allowed;
	// search for the folderPath in offline mail tree and imap tree
	// avoid the root tree because it contains news folders.
	MSG_FolderInfo *destFolder =
		GetMaster()->FindMailFolder(folderPath, FALSE);
	if (!destFolder)
	{
 		MSG_FolderInfoContainer *imapFolders = GetMaster()->GetImapMailFolderTree();
 		if (imapFolders)
			destFolder = imapFolders->FindMailPathname(folderPath);
	}
	if (!destFolder)
	 	return MSG_Drag_Not_Allowed;
	return DragMessagesStatus(indices, numIndices, destFolder, request);
}

MsgERR MSG_Pane::CopyMessages (
                     const MSG_ViewIndex *indices,
                     int32 numIndices,
                     const char *folderPath,
					 XP_Bool deleteAfterCopy)
{
	MsgERR returnErr = eFAILURE;

	if (folderPath)
	{
		// search for the folderPath in offline mail tree and imap tree
		// avoid the root tree because it contains news folders.
		MSG_FolderInfo *destFolder =
			GetMaster()->FindMailFolder(folderPath, FALSE);
		if (!destFolder)
 		{
     		MSG_FolderInfoContainer *imapFolders = GetMaster()->GetImapMailFolderTree();
     		if (imapFolders)
 				destFolder = imapFolders->FindMailPathname(folderPath);
 		}
	
		// do the copy
		if (destFolder)
			returnErr = CopyMessages (indices, numIndices, destFolder, deleteAfterCopy);
	}
	
	return returnErr;
}

// Helper function for copy and move
MSG_DragEffect MSG_Pane::DragMessagesStatus (
                     const MSG_ViewIndex * /*indices*/,
                     int32 numIndices,
                     MSG_FolderInfo* destFolder,
					 MSG_DragEffect request)
{
	MSG_FolderInfo *sourceFolder = NULL;
	if (numIndices > 0)
	{
		sourceFolder = GetFolder();
		MessageDBView *view = GetMsgView();
		if (!sourceFolder || !view)
			return MSG_Drag_Not_Allowed;

		if (sourceFolder == destFolder)
			return MSG_Drag_Not_Allowed;
	}

	// Which destinations can have messages added to them?

	if (destFolder->GetDepth() <= 1) // root of tree or server.
		return MSG_Drag_Not_Allowed; // (needed because local mail server has type "FOLDER_MAIL").
		
	FolderType destType = destFolder->GetType();
	if (destType == FOLDER_CONTAINERONLY // can't drag a message to a server
	|| destType == FOLDER_CATEGORYCONTAINER
	|| destType == FOLDER_IMAPSERVERCONTAINER
	|| destType == FOLDER_NEWSGROUP) // should we offer to post the message? - jrm
	{
		return MSG_Drag_Not_Allowed;
	}

	// check IMAP ACLs of the destination folder, if it's IMAP
	MSG_IMAPFolderInfoMail *imapDest = destFolder->GetIMAPFolderInfoMail();
	if (imapDest)
	{
		if (!imapDest->GetCanDropMessagesIntoFolder())
			return MSG_Drag_Not_Allowed;
	}

	if (!sourceFolder)
	{
		// If there's no source folder, this is all we can know.
		// (From info about the destination folder alone.)
		return request;
	}

	FolderType sourceType = sourceFolder->GetType();

	// check IMAP ACLs of source folder, to see if we're allowed to copy out
	MSG_IMAPFolderInfoMail *imapSrc = sourceFolder->GetIMAPFolderInfoMail();
	if (imapSrc)
	{
		if (!imapSrc->GetCanDragMessagesFromThisFolder())
			return MSG_Drag_Not_Allowed;
	}


	// Which drags are required to be copies?
	
	XP_Bool mustCopy = FALSE;
	XP_Bool preferCopy = FALSE;


	if ((sourceType == FOLDER_MAIL)	
		!= (destType == FOLDER_MAIL))
		preferCopy = TRUE;
	else if (imapSrc && imapDest && imapSrc->GetIMAPHost() != imapDest->GetIMAPHost())
		preferCopy = TRUE;
	else if ((sourceType == FOLDER_NEWSGROUP) ||
			(imapSrc && !imapSrc->GetCanDeleteMessagesInFolder()))
		mustCopy = TRUE;

	if (mustCopy)
		return (MSG_DragEffect)(request & MSG_Require_Copy);
	
	if (preferCopy && (request & MSG_Require_Copy))
		return MSG_Require_Copy;
	// Now, if they don't care, give them a move
	if ((request & MSG_Require_Move) == MSG_Require_Move)
		return MSG_Require_Move;
		
	// Otherwise, give them what they want
	return request;
}

MsgERR MSG_Pane::CopyMessages (
					 const MSG_ViewIndex *indices,
					 int32 numIndices,
					 MSG_FolderInfo *destFolder,
					 XP_Bool deleteAfterCopy)
{
	MsgERR err = eSUCCESS;

	if (0 == numIndices)
		return err;

	MSG_FolderInfo *folder = GetFolder();
	MessageDBView *view = GetMsgView();
	if (!folder || !view)
		return eFAILURE;

	IDArray *ids = new IDArray;
	if (!ids)
		return eOUT_OF_MEMORY;
	
	ResolveIndices (view, indices, numIndices, ids);
	MessageKey nextKeyToLoad = MSG_MESSAGEKEYNONE;

	if (deleteAfterCopy && indices) 
	{
		nextKeyToLoad = view->GetAt(*(indices+numIndices-1)+1); // last index + 1
		if (nextKeyToLoad == MSG_MESSAGEKEYNONE)
			nextKeyToLoad = view->GetAt(*indices-1); // first index - 1
	}

	folder->StartAsyncCopyMessagesInto (destFolder, 
										this, 
										view->GetDB(),
										ids, 
										ids->GetSize(),
										m_context, 
										NULL, // do not run in url queue
										deleteAfterCopy,
										nextKeyToLoad);

	return err;
}

MsgERR MSG_Pane::MoveMessages (const MSG_ViewIndex *indices,
							  int32 numIndices,
							  MSG_FolderInfo *folder)
{
	MsgERR err = CopyMessages (indices, numIndices, folder, TRUE);
	return err;
}


// Translate m_view indices into message keys. No big deal, but maybe useful other places
void MSG_Pane::ResolveIndices (MessageDBView *view, const MSG_ViewIndex *indices, int32 numIndices, IDArray *ids)
{
	XP_ASSERT(view);
	if (!view)
		return;

	MessageKey key = MSG_MESSAGEKEYNONE;
	for (int i = 0; i < numIndices; i++)
	{
		key = view->GetAt (indices[i]);
		XP_ASSERT(MSG_MESSAGEKEYNONE != key);
		if (MSG_MESSAGEKEYNONE != key)
			ids->Add (key);
	}
}

/* This function opens a message and returns a handle to that message in the
 * msg_ptr pointer.
 *
 * The message handle will be passed to MSG_ReadMessage and MSG_CloseMessage
 * to read data and to close the message
 *
 * Return values: return a negative return value listed in merrors.h to
 * signify an error.  return zero (0) on success.
 *
 * !Set message_ptr to NULL on error!
 */



struct MessageLoadingState {
  MailMessageHdr* hdr;
  int32 bytesRemaining;
  XP_File file;
  XP_Bool discarded_envelope_p;
  XP_Bool wrote_fake_id_p;
};

int
MSG_Pane::OpenMessageSock(const char *folder_name,
						  const char *msg_id, int32 msgnum,
						  void * /*folder_ptr*/, void **message_ptr,
						  int32 *content_length)
{
	MessageLoadingState *state = XP_NEW_ZAP(MessageLoadingState);
	MessageDBView* view = GetMsgView();
	  

	if (!state) return MK_OUT_OF_MEMORY;

	*message_ptr = state;

	// ###tw  Need to use msgnum when possible; msg_id can be ambiguous.
	state->hdr = NULL;
	if (msgnum != MSG_MESSAGEKEYNONE)
	{
	  if (view) {
		state->hdr = (MailMessageHdr *)view->GetDB()->GetDBHdrForKey(msgnum);
	  }
	}

	if (state->hdr == NULL && view)
		state->hdr = (MailMessageHdr*) view->GetDB()->GetDBMessageHdrForID(msg_id);

	if (!state->hdr) 
		return MK_MSG_ID_NOT_IN_FOLDER;

	state->bytesRemaining = state->hdr->GetByteLength();

	state->file = XP_FileOpen(folder_name, xpMailFolder, XP_FILE_READ_BIN);
	if (!state->file) 
		return MK_MSG_FOLDER_UNREADABLE;

	*content_length = state->bytesRemaining;

	/* #### does this return a status code? */
	XP_FileSeek (state->file, state->hdr->GetMessageOffset(), SEEK_SET);

	return 0;
}

/* this function should work just like UNIX read(3)
 *
 * "buffer" should be filled up to the size of "buffer_size"
 * with message data.
 *
 * Return values
 *	Return the number of bytes put into "buffer", or
 *  Return zero(0) at end of message, or
 *  Return a negative error value from merrors.h or sys/errno.h
 */
int
MSG_Pane::ReadMessageSock(const char * /*folder_name*/,
						  void *message_ptr,
						  const char * /*message_id*/,
						  int32 /*msgnum*/, char * buffer,
						  int32 buffer_size)
{
  MessageLoadingState *state = (MessageLoadingState *) message_ptr;
  int L;
  XP_ASSERT (state);
  if (! state) return -1;

  XP_ASSERT (state->hdr && state->file);
  if (!state->hdr || !state->file)
	return -1;

  if (state->bytesRemaining == 0)
	return 0;

  if (!state->discarded_envelope_p &&
	  !state->wrote_fake_id_p)
	{
	  MessageDBView* view = GetMsgView();
	  XPStringObj messageId;
	  MSG_DBHandle  db = (view ? view->GetDB()->GetDB() : 0);
	  /* Before emitting any of the `real' data, emit a dummy Message-ID
		 header if this was an IDless message.  This is so that the MIME
		 parsing code will call MSG_ActivateReplyOptions() with an ID
		 that it can use when generating (among other things) the URL
		 to be used to forward this message to another user.
	   */
	  state->hdr->GetMessageId(messageId, db);
	  const char *id = (const char *) messageId;

	  state->wrote_fake_id_p = TRUE;
	  if (id && !XP_STRNCMP (HG02700, id, 4))
		{
		  XP_ASSERT (buffer_size > (int32) (XP_STRLEN(id) + 40));
		  XP_STRCPY (buffer, "Message-ID: <");
		  XP_STRCAT (buffer, id);
		  XP_STRCAT (buffer, ">" LINEBREAK);
		  return XP_STRLEN (buffer);
		}
	}

  L = XP_FileRead (buffer,
				   (state->bytesRemaining <= buffer_size
					? state->bytesRemaining
					: buffer_size),
				   state->file);
  if (L > 0)
	state->bytesRemaining -= L;

  if (L > 0 && !state->discarded_envelope_p)
	{
	  char *s;
	  for (s = buffer; s < buffer + L; s++)
		if (*s == CR || *s == LF)
		  {
			if (*s == CR && *(s+1) == LF)
			  s++;
			s++;
			break;
		  }
	  if (s != buffer)
		{
		  /* Take the first line off the front of the buffer */
		  uint32 off = s - buffer;
		  L -= off;
		  for (s = buffer; s < buffer + L; s++)
			*s = *(s+off);
		  state->discarded_envelope_p = TRUE;
		}
	  else
		{
		  /* discard this whole buffer */
		  L = 0;
		}
	}


  return L;
}

/* This function should close a message opened
 * by MSG_OpenMessage
 */
void
MSG_Pane::CloseMessageSock(const char * /*folder_name*/,
						   const char * /*message_id*/, int32 /*msgnum*/,
						   void *message_ptr)
{
  MessageLoadingState *state = (MessageLoadingState *) message_ptr;

	if (state) 
	{
		if (state->hdr) 
		{
			delete state->hdr;
			state->hdr = NULL;
		}
		if (state->file) 
		{
			XP_FileClose(state->file);
			state->file = NULL;
		}
		XP_FREE(state);
	}
}

XP_Bool MSG_Pane::SetMessagePriority(MessageKey key, MSG_PRIORITY priority)
{
	XP_Bool ret;
	MessageDBView *view = GetMsgView();
	if (!view || !view->GetDB())
		return FALSE;
	ret = view->GetDB()->SetPriority(key, priority);
	if (ret)
	{
		MSG_ViewIndex viewIndex = view->FindKey(key, FALSE);
		if (viewIndex != MSG_VIEWINDEXNONE)
		{
			StartingUpdate(MSG_NotifyChanged, viewIndex, 1);
			EndingUpdate(MSG_NotifyChanged, viewIndex, 1);
		}
	}
	return ret;
}

char *
MSG_Pane::ComputeNewshostArg()
{
	MSG_FolderInfo *folder = GetFolder();
	if (folder)
	{
		MSG_FolderInfoNews *newsFolder = folder->GetNewsFolderInfo();
		if (newsFolder != NULL)
			return newsFolder->GetHostUrl();
	}
	// For reference, in 3.0, this routine was called
	// msg_compute_newshost_arg().
	return NULL;
}


int32
MSG_Pane::GetNewsRCCount(MSG_NewsHost* host)
{
	return host->GetNumGroupsNeedingCounts();
}


char*
MSG_Pane::GetNewsRCGroup(MSG_NewsHost* host)
{
	return host->GetFirstGroupNeedingCounts();
}


int
MSG_Pane::DisplaySubscribedGroup(MSG_NewsHost* host,
								 const char *group,
								 int32 oldest_message,
								 int32 youngest_message,
								 int32 total_messages,
								 XP_Bool nowvisiting)
{
	if (!host) return 0;
	MSG_FolderInfoNews* info = host->FindGroup(group);
	host->SetGroupSucceeded(TRUE);
	if (!info && nowvisiting) // let's try autosubscribe...
	{
		info = host->AddGroup(group);
	}
	if (!info)
		return 0;
	else if (!info->IsSubscribed())
		info->Subscribe(TRUE, this);

	if (!info) return 0;
	info->UpdateSummaryFromNNTPInfo(oldest_message, youngest_message,
									total_messages);
	return 0;
}

int
MSG_Pane::AddNewNewsGroup(MSG_NewsHost* host,
						  const char* groupname,
						  int32 /*oldest*/,
						  int32 /*youngest*/,
						  const char *flags,
						  XP_Bool bXactiveFlags)
{
	XP_ASSERT(host);
	if (!host) return -1;

	int status = host->NoticeNewGroup(groupname);
	if (status < 0) return status;
	if (status > 0) m_numNewGroups++;

	XP_Bool		bIsCategoryContainer = FALSE;
	XP_Bool		bIsProfile = FALSE;

	while (flags && *flags)
	{
		char flag = toupper(*flags);
		flags++;
		switch (flag)
		{
			case 'C':
				bIsCategoryContainer = TRUE;
				break;
			case 'P':
			case 'V':
				bIsProfile = TRUE;
				break;
			default:
				break;
		}
	}
	if (bXactiveFlags)
	{
		host->SetIsCategoryContainer(groupname, bIsCategoryContainer);
		host->SetIsProfile(groupname, bIsProfile);
	}

	if (status > 0) {
		// If this really is a new newsgroup, then if it's a category of a
		// subscribed newsgroup, then automatically subscribe to it.
		char* containerName = host->GetCategoryContainer(groupname);
		if (containerName) {
			MSG_FolderInfoNews* categoryInfo = host->FindGroup(containerName);
			if (categoryInfo && categoryInfo->IsSubscribed()) {
				host->AddGroup(groupname);
				// this autosubscribes categories of subscribed newsgroups.
			}
			delete [] containerName;
		}
	}
	return status;
}


XP_Bool
MSG_Pane::AddGroupsAsNew()
{
	return TRUE;
}


int
MSG_Pane::BeginCompressFolder(URL_Struct* url,
									const char* foldername,
									void** closure) 
{
	MSG_CompressState *compressState = 
		MSG_CompressState::Create(m_master, GetContext(), url, foldername);
	*closure = compressState;
	if (compressState == NULL)
		return MK_OUT_OF_MEMORY;
	return compressState->BeginCompression();
}

int
MSG_Pane::FinishCompressFolder(URL_Struct* /*url*/,
							   const char* /*foldername*/,
							   void* closure) 
{
	MSG_CompressState *compressState = (MSG_CompressState *) closure;
	return compressState->CompressSomeMore();
}

int
MSG_Pane::CloseCompressFolderSock(URL_Struct* /*url*/,
										void* closure) 
{
	MSG_CompressState *compressState = (MSG_CompressState *) closure;
	return compressState->FinishCompression();
}

UndoManager*
MSG_Pane::GetUndoManager()
{
	if (!m_undoManager)
	{
		// try to use the undo manager for a pane with matching context, since the fe
		// is going to use the pane somewhat indiscriminately.
		MSG_Pane *pane = GetFirstPaneForContext(GetContext());
		while (pane)
		{
			if (pane != this)
			{
				if (pane->m_undoManager)
				{
					m_undoManager = pane->m_undoManager;
					m_undoManager->AddRefCount();
					return m_undoManager;
				}
			}
			pane = GetNextPaneForContext(pane, GetContext());
		}

		m_undoManager = new UndoManager(this, 2000);
		if (m_undoManager)
			m_undoManager->Init();
	}
	return m_undoManager;
}


BacktrackManager*
MSG_Pane::GetBacktrackManager()
{
	if (!m_backtrackManager)
		m_backtrackManager = new BacktrackManager (this);
	return m_backtrackManager;
}

void MSG_Pane::OnFolderKeysAreInvalid (MSG_FolderInfo *folderInfo)
{
	if (m_undoManager)
		m_undoManager->RemoveActions(folderInfo);
	if (m_backtrackManager)
		m_backtrackManager->RemoveEntries(folderInfo);
}

void MSG_Pane::OnFolderChanged(MSG_FolderInfo *) {}
void MSG_Pane::OnFolderAdded (MSG_FolderInfo *, MSG_Pane *) {}

void MSG_Pane::OnFolderDeleted (MSG_FolderInfo *folder) 
{
	FE_PaneChanged (this, FALSE, MSG_PaneNotifyFolderDeleted, (uint32) folder);
}

MsgERR
MSG_Pane::DeliverQueuedMessages()
{
	URL_Struct* url;

	if (NET_IsOffline())	// let master go online, so IMAP gets a chance.
	{
		m_master->SynchronizeOffline(this, FALSE, FALSE, TRUE, FALSE, FALSE);
	}
	else
	{
		url = NET_CreateURLStruct("mailbox:?deliver-queued", NET_NORMAL_RELOAD);
		if (!url) return MK_OUT_OF_MEMORY;
		url->internal_url = TRUE;
        url->pre_exit_fn = PostDeliverQueuedExitFunc;
		GetURL(url, FALSE);
	}
	return 0;
}

/* static */
void MSG_Pane::PostDeliverQueuedExitFunc (URL_Struct *URL_s, int status, MWContext* /*window_id*/)
{
	// if this was launched from a thread pane.  then start compress folders url.
	if ((status >= 0) && (URL_s->msg_pane != NULL))
	{
		MSG_Pane *pane = URL_s->msg_pane;
		MSG_Master	*master = pane->GetMaster();
		if (master != NULL)	
		{
			char *folderPath = master->GetPrefs()->MagicFolderName (MSG_FOLDER_FLAG_QUEUE);
			if (folderPath)
			{
				MSG_ThreadPane* threadPane = master->FindThreadPaneNamed(folderPath);
				if (threadPane)
					threadPane->ReloadFolder();
				FREEIF(folderPath);
			}
		}
	}
}

int
MSG_Pane::BeginDeliverQueued(URL_Struct* /*url*/, void** closure) 
{
	const char *folderPath;
	
	if (m_master)
		folderPath = m_master->GetPrefs()->MagicFolderName (MSG_FOLDER_FLAG_QUEUE);
	else
	{
		XP_ASSERT(FALSE);
		return MK_MSG_QUEUED_DELIVERY_FAILED;
	}


	MSG_DeliverQueuedMailState	*deliverMailState = new MSG_DeliverQueuedMailState(folderPath, this);

	if (!deliverMailState) 
		return MK_OUT_OF_MEMORY;
	*closure = deliverMailState;
	deliverMailState->SetContext(GetContext());
	return (MK_WAITING_FOR_CONNECTION);
}

int
MSG_Pane::FinishDeliverQueued(URL_Struct* url, void* closure) 
{
	MSG_DeliverQueuedMailState	*deliverMailState = (MSG_DeliverQueuedMailState *) closure;
	return deliverMailState->DeliverMoreQueued(url);
}

/* static */void
MSG_Pane::GetNextURLInChain_CB(URL_Struct* urlstruct, int /*status*/,  MWContext* /*context*/)
{
	if (urlstruct->msg_pane)
	{
		urlstruct->msg_pane->GetNextURLInChain();
	}
}

void 
MSG_Pane::GetNextURLInChain()
{
	if (m_urlChain)
		m_urlChain->GetNextURL();
}

int
MSG_Pane::CloseDeliverQueuedSock(URL_Struct* url,
									   void* closure) 
{
	int	ret;
	MSG_DeliverQueuedMailState	*deliverMailState = (MSG_DeliverQueuedMailState *) closure;
	ret = deliverMailState->CloseDeliverQueuedSock(url);
	if (m_urlChain)
		url->pre_exit_fn = MSG_Pane::GetNextURLInChain_CB;
	return ret;
}

void MSG_Pane::StoreImapFilterClosureData( tImapFilterClosure *closureData )
{
	m_ImapFilterData = closureData;
}
void MSG_Pane::ClearImapFilterClosureData()
{
	m_ImapFilterData = NULL;
}

tImapFilterClosure *MSG_Pane::GetImapFilterClosureData()
{
	return m_ImapFilterData;
}


MsgERR
MSG_Pane::CheckForNew(MSG_NewsHost* host)
{
	InterruptContext(FALSE);
	XP_ASSERT(m_hostCheckingForNew == NULL);
	XP_ASSERT(host);
	if (!host) return eUNKNOWN;
	m_hostCheckingForNew = host;
	m_checkForNewStartTime = time((time_t*) 0);
	time_t lasttime = host->getLastUpdate();
	char* url =
		PR_smprintf("%s/%s",
					m_hostCheckingForNew->GetURLBase(),
					(lasttime == 0) ? "*" : "?newgroups");
	if (!url) 
		return eOUT_OF_MEMORY;
	URL_Struct* urlstruct = NET_CreateURLStruct(url, NET_DONT_RELOAD);
	XP_FREE(url);
	if (!urlstruct) 
		return eOUT_OF_MEMORY;
	urlstruct->pre_exit_fn = MSG_Pane::CheckForNewDone_s;
	return GetURL(urlstruct, FALSE); 
}


void
MSG_Pane::CheckForNewDone_s(URL_Struct* url_struct, int status,
							MWContext* context)
{
	if (status == MK_EMPTY_NEWS_LIST) {
		// There is a bug in mknews.c that causes this return code
		// sometimes.  Just patch it here for now...
		status = 0;
	}

	MSG_Pane* pane = url_struct->msg_pane;
	XP_ASSERT(pane);
	if (!pane || !MSG_Pane::PaneInMasterList(pane)) return;
	pane->CheckForNewDone(url_struct, status, context);
}


void
MSG_Pane::CheckForNewDone(URL_Struct* /*url_struct*/, int status,
						  MWContext* /*context*/)
{
	XP_ASSERT(m_hostCheckingForNew);
	if (!m_hostCheckingForNew) return;
	if (status >= 0) {
		m_hostCheckingForNew->setLastUpdate(m_checkForNewStartTime);
	}
	m_hostCheckingForNew->SaveHostInfo();
	m_hostCheckingForNew = NULL;
}

static void
OpenMessageAsDraftExit (URL_Struct *url_struct,
						int /*status*/,
						MWContext* context)
{
	XP_ASSERT (url_struct && context);

	if (!url_struct) return;

	NET_FreeURLStruct ( url_struct );
}

MsgERR
MSG_Pane::OpenMessageAsDraft(MSG_ViewIndex* indices, int32 numIndices,
							 XP_Bool bFwdInline)
{
  MsgERR status = eUNKNOWN;
  MessageDBView *view = GetMsgView();
  MSG_FolderInfo *folder = GetFolder();

  XP_ASSERT(indices && view && folder);
  if (!indices || !view || !folder)
	return status;

  status = eSUCCESS;

  for (int32 i=0; i < numIndices; i++) {
	MessageKey key = view->GetAt(*(indices+i));
	if (MSG_MESSAGEKEYNONE != key) {
	  char *url = folder->BuildUrl(view->GetDB(), key);
	  if (NULL != url) {
		URL_Struct* url_struct = NET_CreateURLStruct(url, NET_DONT_RELOAD);
		XP_FREEIF(url);
		if (url_struct) {
		  MSG_PostDeliveryActionInfo *actionInfo = 
			new MSG_PostDeliveryActionInfo(folder);
		  if (actionInfo) {
			actionInfo->m_msgKeyArray.Add(key);
			if (bFwdInline)
				actionInfo->m_flags |= MSG_FLAG_FORWARDED;
			else if (folder->GetFlags() & 
					 (MSG_FOLDER_FLAG_DRAFTS | MSG_FOLDER_FLAG_QUEUE))
				actionInfo->m_flags |= MSG_FLAG_EXPUNGED;

			url_struct->fe_data = (void *) actionInfo;
			url_struct->msg_pane = this;
			url_struct->allow_content_change = FALSE;
#if 0
			NET_GetURL (url_struct, FO_OPEN_DRAFT, m_context,
						OpenMessageAsDraftExit);
#else
			MSG_UrlQueue::AddUrlToPane (url_struct, OpenMessageAsDraftExit, this, TRUE, FO_OPEN_DRAFT);
#endif
		  }
		  else {
			NET_FreeURLStruct(url_struct);
			status = (MsgERR) MK_OUT_OF_MEMORY;
			// continue for other messages
		  }
		}
		else {
		  status = (MsgERR) MK_OUT_OF_MEMORY;
		}
	  }
	  else {
		status = (MsgERR) MK_OUT_OF_MEMORY;
	  }
	}
  }
  return status;
}


XP_Bool MSG_Pane::ModerateNewsgroupStatus (MSG_FolderInfo *folder)
{
	
	MSG_FolderInfoNews *newsFolder = folder->GetNewsFolderInfo();
	if (newsFolder)
	{
		MSG_NewsHost *host = newsFolder->GetHost();
		return NULL != host->QueryPropertyForGet("MODURL");
	}
	return FALSE; // command not enabled
}


MsgERR MSG_Pane::ModerateNewsgroup (MSG_FolderInfo *folder)
{
	MsgERR status = eSUCCESS;

	MSG_FolderInfoNews *newsFolder = folder->GetNewsFolderInfo();
	if (newsFolder)
	{
		MSG_NewsHost *host = newsFolder->GetHost();
		URL_Struct *url_s = NET_CreateURLStruct (host->QueryPropertyForGet("MODURL"), NET_NORMAL_RELOAD);
		if (url_s)
		{
			// Provide the name of the newsgroup so the server knows what we're talking about
			char *stuffToChop = XP_STRSTR (url_s->address, "[ngc]");
			if (stuffToChop)
			{
				*stuffToChop = '\0';
				StrAllocCat (url_s->address, folder->GetName());
			}
			GetURL (url_s, TRUE);
		}
		else
			status = eOUT_OF_MEMORY;
	}

	return status;
}


XP_Bool MSG_Pane::NewNewsgroupStatus (MSG_FolderInfo *folder)
{
	MSG_NewsHost *host = NULL;
	MSG_FolderInfoNews *newsFolder = folder->GetNewsFolderInfo();

	if (newsFolder)
		host = newsFolder->GetHost();
	else if (folder->GetFlags() & MSG_FOLDER_FLAG_NEWS_HOST)
		host = ((MSG_NewsFolderInfoContainer*) folder)->GetHost();

	if (host && (NULL != host->QueryPropertyForGet("NGURL")))
		return TRUE;
	return FALSE;
}


MsgERR MSG_Pane::NewNewsgroup (MSG_FolderInfo *folder, XP_Bool createCategory)
{
	MsgERR status = eSUCCESS;
	MSG_NewsHost *host = NULL;
	MSG_FolderInfoNews *newsFolder = folder->GetNewsFolderInfo();

	if (newsFolder)
		host = newsFolder->GetHost();
	else if (folder->GetFlags() & MSG_FOLDER_FLAG_NEWS_HOST)
		host = ((MSG_NewsFolderInfoContainer*) folder)->GetHost();
	
	if (host)
	{
		char *ngUrl = XP_STRDUP(host->QueryPropertyForGet("NGURL"));
		if (ngUrl)
		{
			char *stuffToChop = XP_STRSTR (ngUrl, "[ngc]");
			if (stuffToChop)
				*stuffToChop = '\0';

			char *qualifiedUrl = PR_smprintf ("%s%s", ngUrl, createCategory ? newsFolder->GetNewsgroupName() : "*");
			if (qualifiedUrl)
			{
				URL_Struct *url_s = NET_CreateURLStruct (qualifiedUrl, NET_NORMAL_RELOAD);
				if (url_s)
					GetURL (url_s, TRUE);
				else
					status = eOUT_OF_MEMORY;
				FREEIF(qualifiedUrl);
			}
			else
				status = eOUT_OF_MEMORY;
			FREEIF(ngUrl);
		}
	}
	return status;
}

/////////////////////////////////////////////////////////////////////////////////////
// #### jht moved from folder pane
MsgERR MSG_Pane::CompressAllFolders()
{
    URL_Struct *url;
    char* buf;

    //  ### mw Found these comments in CompressFolder above:
    //
    //  This probably invalidates undo state.
    //  Do we need to commit the open db first?

    //  ### mw Is there a better way to do this?
    buf = PR_smprintf("mailbox:?compress-folder");
    if (!buf) return MK_OUT_OF_MEMORY;
    url = NET_CreateURLStruct(buf, NET_NORMAL_RELOAD);
    XP_FREE(buf);
    if (!url) 
        return eOUT_OF_MEMORY;
    url->internal_url = TRUE;
    GetURL(url, FALSE);
    return 0;
}

// passed in folder is used to figure out from context, which trash the user might
// be talking about.
MsgERR MSG_Pane::EmptyImapTrash(MSG_IMAPHost *host)
{
	// Find the default trash, if no folder passed in.
	if (!host && MSG_GetDefaultIMAPHost(m_master))
		host = MSG_GetDefaultIMAPHost(m_master);
	
	MSG_FolderInfo *trashFolder = NULL;
	if (host)
		trashFolder = host->GetTrashFolderForHost();

	if (trashFolder && trashFolder->DeleteIsMoveToTrash())
		return ((MSG_IMAPFolderInfoMail *) trashFolder)->DeleteAllMessages(this, TRUE);
	else
		return eUNKNOWN;	// nothing to do for imap delete model
							// return an error so that EmptyTrash will perform
							// what the exit function for the DeleteAllMessages
							// url would have done
}


MsgERR MSG_Pane::PreflightDeleteFolder (MSG_FolderInfo *folder, XP_Bool getUserConfirmation)
{
    MsgERR err = 0;

	// It's possible to get past GetCommandStatus's check if the folder itself is deletable
	// but one of its children is not. E.g. if the user moves the Inbox into another folder, 
	// and then tries to delete that folder, the Inbox is still magic, and we shouldn't delete it.
	if (!folder->IsDeletable())
	{
		char *prompt = PR_smprintf (XP_GetString(MK_MSG_CANT_DELETE_RESERVED_FOLDER), folder->GetName());
		if (prompt)
		{
			FE_Alert (GetContext(), prompt);
			XP_FREE(prompt);
		}
		return eUNKNOWN;
	}

    // Be sure to ask all the children too
    MSG_FolderArray *subFolders = folder->GetSubFolders();
    for (int i = 0; i < subFolders->GetSize(); i++)
    {
        err = PreflightDeleteFolder (subFolders->GetAt(i), getUserConfirmation);
        if (err)
            return err;
    }

    // Prevent the user from really deleting any folder which has a thread pane open
    XPPtrArray panes;
    GetMaster()->FindPanesReferringToFolder (folder, &panes);
    if (panes.GetSize() > 0)
    {
		char *prompt = PR_smprintf (XP_GetString (MK_MSG_PANES_OPEN_ON_FOLDER), folder->GetName());
		if (prompt)
		{
			FE_Alert (GetContext(), prompt);
			XP_FREE(prompt);
		}
        return eUNKNOWN;
    }

    // Make sure they want to delete any mail folder
    if (folder->IsMail() && getUserConfirmation)
    {
        char *prompt = PR_smprintf (XP_GetString (MK_MSG_DELETE_FOLDER_MESSAGES), folder->GetName());
		if (prompt)
		{
			XP_Bool userCancelled = !FE_Confirm (GetContext(), prompt);
			XP_FREE(prompt);
			if (userCancelled)
				return eUNKNOWN;
		}
    }

	// If this folder is a filter rule target, the user can disable the 
	// rule or cancel the delete operation.
	MSG_RuleTracker tracker (GetMaster(), MSG_FolderPane::RuleTrackCB, this);
	if (!tracker.WatchDeleteFolders (&folder, 1))
		return eUNKNOWN;

    return err;
}


	// move some implementation from MSG_FolderPane.  Compress the one
	// specified mail folder whether it be imap or pop
MsgERR MSG_Pane::CompressOneMailFolder(MSG_FolderInfoMail *folder)
{
    URL_Struct* url;
    char* buf;
    
	if (folder->GetType() == FOLDER_MAIL)
    	buf = PR_smprintf("mailbox:%s?compress-folder", folder->GetPathname());
    else
    {
    	MSG_IMAPFolderInfoMail *imapFolder = folder->GetIMAPFolderInfoMail();
    	buf = CreateImapMailboxExpungeUrl(imapFolder->GetHostName(),
    									  imapFolder->GetOnlineName(),
    									  imapFolder->GetOnlineHierarchySeparator());
    }
    	
    if (!buf) return MK_OUT_OF_MEMORY;
    url = NET_CreateURLStruct(buf, NET_NORMAL_RELOAD);
    XP_FREE(buf);
    if (!url) 
        return eOUT_OF_MEMORY;
    url->internal_url = TRUE;
	MSG_UrlQueue::AddUrlToPane(url, NULL, this);
//    GetURL(url, FALSE);
    return 0;
}



MsgERR MSG_Pane::EmptyTrash(MSG_FolderInfo *folder)
{
// Note that like 3.0, we don't deal with partially downloaded messages
// What they would be doing in the trash is beyond me, though. They are still
// left on the pop server.
	MSG_IMAPHost *imapHost = NULL;
	XP_Bool usingImap = (m_master->GetIMAPHostTable() != NULL);
	if (usingImap)
		imapHost = folder ? folder->GetIMAPHost() : 0;

	MailDB	*trashDB;
	char *trashPath = m_master->GetPrefs()->MagicFolderName (MSG_FOLDER_FLAG_TRASH);
	MSG_FolderInfo *trashFolder = FindMailFolder(trashPath, TRUE);

	TDBFolderInfoTransfer *originalInfo = NULL;
	if (MailDB::Open(trashPath, TRUE, &trashDB) == eSUCCESS)
	{
		originalInfo = new TDBFolderInfoTransfer(*trashDB->m_dbFolderInfo);
		trashDB->ForceClosed();
	}

	// Truncate trash folder, and remove summary file.
	XP_FileClose(XP_FileOpen(trashPath, xpMailFolder, XP_FILE_WRITE_BIN));
	XP_FileRemove(trashPath, xpMailFolderSummary);

	// Run through any subfolders which live in the trash, and wipe 'em out
	// run the loop backwards so the indices stay valid
	MSG_FolderInfo *tree = GetMaster()->GetFolderTree();
	if (trashFolder)
	{
		for (int i = trashFolder->GetSubFolders()->GetSize() - 1; i >= 0; i--)
		{
			MSG_FolderInfo *folder = trashFolder->GetSubFolders()->GetAt(i);

			// Remove the folder from the disk, and from the folder pane
			MSG_FolderInfoMail *mailFolder = folder->GetMailFolderInfo();
			if (mailFolder) 
				tree->PropagateDelete((MSG_FolderInfo **) &mailFolder);
			else
				XP_ASSERT(FALSE);
		}
	}
	// Create a new summary file, update the folder message counts, and
	// Close the summary file db.
	if (MailDB::Open(trashPath, TRUE, &trashDB, TRUE) == eSUCCESS)
	{
		if (trashFolder != NULL)
			trashFolder->SummaryChanged();
		if (originalInfo)
		{
			originalInfo->TransferFolderInfo(*trashDB->m_dbFolderInfo);
			delete originalInfo;
		}
		trashDB->SetSummaryValid(TRUE);
		trashDB->Close();
	}

	// Reload any trash thread pane because it's invalid now.
	MSG_ThreadPane* threadPane = NULL;
	if (m_master != NULL)	
		threadPane = m_master->FindThreadPaneNamed(trashPath);
	if (threadPane != NULL)
		threadPane->ReloadFolder();

	MsgERR imapError = eSUCCESS;

	if (m_master->GetIMAPHostTable())	// If there's a host table, we have IMAP hosts
		imapError = EmptyImapTrash(imapHost);

    // Start a Compress All Folders action.
    // If we are imap, we will compress on exit function
    // of empty trash url
	// if the imap empty trash fails (and the URL never gets fired)
	// then compress now
    if ((!usingImap) ||
		(usingImap && (imapError != eSUCCESS)))
    	CompressAllFolders();

	FREEIF(trashPath);
	return 0;
}


void MSG_Pane::ManageMailAccountExitFunc(URL_Struct *url,
											   int status,
											   MWContext *context)
{
  MSG_Pane *pane = url->msg_pane;
  char *alert = NULL;

  if ((status >= 0 || status == MK_POP3_NO_MESSAGES ||
	   status == MK_CONNECTED) && pane) {
	const char * mailUrl = pane->GetMaster()->GetMailAccountURL();
	if (mailUrl) {
	  URL_Struct *url_struct = NET_CreateURLStruct(mailUrl, NET_NORMAL_RELOAD);
	  if (url_struct) {
		url_struct->msg_pane = pane;
		msg_GetURL(context, url_struct, FALSE);
	  }
	}
	else {
		alert = XP_GetString(MK_MSG_UNABLE_MANAGE_MAIL_ACCOUNT);
	}
  }
  else if (status < 0) {
	alert = XP_GetString(MK_MSG_UNABLE_MANAGE_MAIL_ACCOUNT);
  }

  if (alert)
	FE_Alert(context, alert);

  if (url)
	NET_FreeURLStruct(url);
}


MsgERR MSG_Pane::ManageMailAccount(MSG_FolderInfo *folder)
{
	MsgERR status = 0;
#ifdef DEBUG_bienvenu
	if (folder && folder->GetIMAPFolderInfoMail() && folder->GetIMAPFolderInfoMail()->HaveAdminUrl(MSG_AdminFolder))
	{
		folder->GetAdminUrl(GetContext(), MSG_AdminFolder);
		return status;
	}
#endif
	folder->GetAdminUrl(GetContext(), MSG_AdminServer);
	return status;
/*
	char *getMailAccountUrl = NULL;
	URL_Struct *url_struct;
	const char *host = GetPrefs()->GetPopHost();
	const char *mailUrl = GetMaster()->GetMailAccountURL();

	if (mailUrl) 
	{
		// this code used to make sure the mail url contained the host name. Why? if (strcasestr(mailUrl, host)) {
		URL_Struct *url_struct = NET_CreateURLStruct(mailUrl, NET_NORMAL_RELOAD);
		if (url_struct) 
		{
			url_struct->msg_pane = this;
			msg_GetURL(GetContext(), url_struct, FALSE);
		}
	}

	if (!host || !*host)
	{
#ifdef XP_MAC
		FE_EditPreference(PREF_PopHost);
#endif
		return MK_MSG_NO_POP_HOST;
	}

	if (GetPrefs()->GetMailServerIsIMAP4())
	{	
		getMailAccountUrl = CreateImapManageMailAccountUrl(host);
		if (!getMailAccountUrl)
			return MK_OUT_OF_MEMORY;
		GetContext()->mailMaster = GetMaster();
	}
	else {

		getMailAccountUrl = (char *) XP_ALLOC(256);
		if (!getMailAccountUrl)
			return MK_OUT_OF_MEMORY;

		XP_STRCPY(getMailAccountUrl, "pop3://");
		XP_STRCAT(getMailAccountUrl, host);
		XP_STRCAT(getMailAccountUrl, "?gurl");
	}
	url_struct = NET_CreateURLStruct (getMailAccountUrl, NET_NORMAL_RELOAD);
	XP_FREEIF(getMailAccountUrl);

	if (!url_struct)
	  return MK_OUT_OF_MEMORY;

	url_struct->msg_pane = this;
	url_struct->internal_url = TRUE;
	GetContext()->imapURLPane = this;
	MSG_UrlQueue::AddUrlToPane (url_struct, OpenMessageAsDraftExit, this, TRUE);

	return status;
*/
}

void MSG_Pane::GroupNotFound(MSG_NewsHost* host, const char *group, XP_Bool opening)
{
	if (opening)
	{
		MSG_FolderInfoNews* info = host->FindGroup(group);
		MSG_FolderInfo* curFolder = GetFolder();
		MSG_FolderInfoNews *curNewsFolder = (curFolder) ? curFolder->GetNewsFolderInfo() : (MSG_FolderInfoNews *)NULL;

		// make sure the group not found is the current group
		if (info && curNewsFolder && !XP_STRCMP(info->GetNewsgroupName(), curNewsFolder->GetNewsgroupName()))
		{
			XP_Bool autoSubscribed = (info) ? info->GetAutoSubscribed() : FALSE;
			XP_Bool unsubscribe = autoSubscribed;

			if (!autoSubscribed)
			{
				char *unsubscribePrompt = PR_smprintf(XP_GetString(MK_MSG_GROUP_NOT_ON_SERVER), group, host->getStr());
				if (unsubscribePrompt && GetContext())
				{
					unsubscribe = FE_Confirm(GetContext(), unsubscribePrompt);
					FREEIF(unsubscribePrompt);
				}
			}
			if (unsubscribe)
			{
				host->GroupNotFound(group, opening);
			}
		}
	}
	else if (host->IsCategory(group))
		host->GroupNotFound(group, opening);
}

XP_Bool MSG_Pane::DisplayingRecipients() 
{
	if (msg_DontKnow == m_displayRecipients)
	{
		// Since MSG_FolderInfo::DisplayRecipients can walk the folder tree in 
		// order to figure out whether it's a child of an FCC folder, we don't
		// want to do that on every commandStatus from the FE. So cache the
		// displayRecipients state in the pane

		MSG_FolderInfo *folder = GetFolder();
		if (folder && folder->DisplayRecipients())
			m_displayRecipients = msg_Yes;
		else
			m_displayRecipients = msg_No;
	}

	XP_ASSERT(msg_DontKnow != m_displayRecipients); // should have figured this out above
	return (msg_Yes == m_displayRecipients) ? TRUE : FALSE;
}

void MSG_Pane::SetRequestForReturnReceipt(XP_Bool bRequested)
{
	m_requestForReturnReceipt = bRequested;
}

XP_Bool MSG_Pane::GetRequestForReturnReceipt()
{
	return m_requestForReturnReceipt;
}

void MSG_Pane::SetSendingMDNInProgress(XP_Bool inProgress)
{
	m_sendingMDNInProgress = inProgress;
}

XP_Bool MSG_Pane::GetSendingMDNInProgress()
{
	return m_sendingMDNInProgress;
}

MSG_PostDeliveryActionInfo *
MSG_Pane::GetPostDeliveryActionInfo ()
{
  return m_actionInfo;
}

void
MSG_Pane::SetPostDeliveryActionInfo ( MSG_PostDeliveryActionInfo *actionInfo )
{
  if (m_actionInfo) 
	delete m_actionInfo;
  m_actionInfo = actionInfo;
}

void
MSG_Pane::SetIMAPListInProgress(XP_Bool inProgress)
{
	m_imapListInProgress = inProgress;
}

XP_Bool 
MSG_Pane::IMAPListInProgress() 
{ 
	return m_imapListInProgress; 
}

void
MSG_Pane::SetIMAPListMailboxExist(XP_Bool bExist)
{
	m_imapListMailboxExist = bExist;
}

XP_Bool
MSG_Pane::IMAPListMailboxExist()
{
	return m_imapListMailboxExist;
}

/* static */ void 
MSG_Pane::PostDeleteIMAPOldDraftUID(URL_Struct* url, int status, MWContext*)
{
	if (status >= 0)
	{
		MSG_PostDeliveryActionInfo *actionInfo = (MSG_PostDeliveryActionInfo *) url->fe_data;
		if (actionInfo)
		{
			if (actionInfo->m_folderInfo)
			{
				MSG_IMAPFolderInfoMail *imapFolderInfo = actionInfo->m_folderInfo->GetIMAPFolderInfoMail();
				char *dbName = WH_FileName (imapFolderInfo->GetPathname(), xpMailFolderSummary);
				MailDB *db = NULL;
				if (dbName)
					db = (MailDB*) MessageDB::FindInCache(dbName);
				XP_FREEIF(dbName);
				if (db)
				{
					db->DeleteMessage(actionInfo->m_msgKeyArray.GetAt(0));
					MSG_Pane *urlPane = NULL;

					urlPane =
						url->msg_pane->GetMaster()->FindPaneOfType
									  (imapFolderInfo, MSG_MESSAGEPANE);

					if (!urlPane)
						urlPane = url->msg_pane->GetMaster()->FindPaneOfType
										(imapFolderInfo, MSG_THREADPANE);

					if (!urlPane)
						urlPane = url->msg_pane;

					char *urlString = CreateImapMailboxLITESelectUrl(imapFolderInfo->GetHostName(),
															   imapFolderInfo->GetOnlineName(),
															   imapFolderInfo->GetOnlineHierarchySeparator());
					if (urlString)
					{
						URL_Struct *url_struct =
							NET_CreateURLStruct(urlString,
												NET_NORMAL_RELOAD);
						if (url_struct)
						{
							imapFolderInfo->SetFolderLoadingContext(url->msg_pane->GetContext());
							url->msg_pane->SetLoadingImapFolder(imapFolderInfo);
							url_struct->fe_data = (void *) imapFolderInfo;
							url_struct->internal_url = TRUE;
							url_struct->msg_pane = urlPane;
							urlPane->GetContext()->imapURLPane = urlPane;
							MSG_UrlQueue::AddUrlToPane (url_struct, 
														PostLiteSelectExitFunc,
														urlPane, TRUE);
						}
						XP_FREEIF(urlString);
					}
				}
			}
			actionInfo->m_msgKeyArray.RemoveAt(0);
		}
	}
	NET_FreeURLStruct(url);
}

void
MSG_Pane::DeleteIMAPOldDraftUID(MSG_PostDeliveryActionInfo *actionInfo, MSG_Pane *urlPane)
{
	if (NET_IsOffline())
	{
		if (actionInfo && actionInfo->m_folderInfo &&
			actionInfo->m_msgKeyArray.GetSize() >= 1)
		{
			MSG_IMAPFolderInfoMail *mailFolderInfo =
				actionInfo->m_folderInfo->GetIMAPFolderInfoMail();
			XP_ASSERT(mailFolderInfo);
			if (mailFolderInfo)
			{
				MailDB *mailDB = NULL;
				MailDB::Open(mailFolderInfo->GetPathname(), FALSE, &mailDB);
				if (mailDB)
				{
					DBOfflineImapOperation *op = NULL;
					MessageKey doomedKey = actionInfo->m_msgKeyArray.GetAt(0);
					if ((int32) doomedKey >= 0) // real message key
					{			// add new offline delete operation
						op = mailDB->GetOfflineOpForKey(doomedKey, TRUE);
						if (op)
						{
							op->SetImapFlagOperation(op->GetNewMessageFlags() |
													 kImapMsgDeletedFlag);
							delete op;
						}
					}
					else
					{		
						// this is a fake one which has been added to the
						// database as an offline append draft operation; we
						// have to delete it from the offline operations queue
						mailDB->DeleteOfflineOp(doomedKey);
					}
					mailDB->DeleteMessage(doomedKey);
					actionInfo->m_msgKeyArray.RemoveAt(0);
					mailDB->Close();
				}
			}
		}
	}
	else
	{
		MSG_Pane *pane = urlPane ? urlPane : this;
		if (actionInfo &&
			actionInfo->m_msgKeyArray.GetSize() >= 1)
		{
			char *deleteUrl = NULL;
			char *idString = PR_smprintf("%ld", actionInfo->m_msgKeyArray.GetAt(0));
			MSG_IMAPFolderInfoMail *mailFolderInfo = NULL;

			XP_ASSERT(idString);

			if (actionInfo->m_folderInfo) 
			{
				XP_ASSERT(actionInfo->m_folderInfo->GetType() == FOLDER_IMAPMAIL);
				mailFolderInfo =
					actionInfo->m_folderInfo->GetIMAPFolderInfoMail();
			}
			else
			{
				char *defaultDrafts = NULL;
				PREF_CopyCharPref("mail.default_drafts", &defaultDrafts);
				actionInfo->m_folderInfo = GetMaster()->GetFolderInfo(defaultDrafts, FALSE);
				mailFolderInfo =  actionInfo->m_folderInfo ?
					actionInfo->m_folderInfo->GetIMAPFolderInfoMail() :
					(MSG_IMAPFolderInfoMail *)NULL;

				XP_FREEIF(defaultDrafts);
			}

			if (mailFolderInfo)
			{

				deleteUrl = CreateImapDeleteMessageUrl(
									   mailFolderInfo->GetHostName(),
									   mailFolderInfo->GetOnlineName(),
									   mailFolderInfo->GetOnlineHierarchySeparator(),
									   idString,
									   TRUE); // ids are uids
			}

			if (deleteUrl)
			{
				URL_Struct *url_struct = NET_CreateURLStruct(deleteUrl, NET_NORMAL_RELOAD);
				if (url_struct)
				{
					url_struct->internal_url = TRUE;
					url_struct->fe_data = actionInfo;
					url_struct->msg_pane = pane;
					GetContext()->imapURLPane = pane;
					MSG_UrlQueue::AddUrlToPane ( url_struct, PostDeleteIMAPOldDraftUID,
												 pane, TRUE );
				} 
				XP_FREEIF(deleteUrl);
			}
			XP_FREEIF(idString);
		}
	}
}

/* static */ void MSG_Pane::PostLiteSelectExitFunc( URL_Struct *url,
													int status,
													MWContext *context)
{
	MSG_IMAPFolderInfoMail *imapFolderInfo = (MSG_IMAPFolderInfoMail *)url->fe_data;
	if (imapFolderInfo)
	{
		imapFolderInfo = imapFolderInfo->GetIMAPFolderInfoMail();
		if (imapFolderInfo)
			url->msg_pane->SetLoadingImapFolder(imapFolderInfo);
	}

	url->fe_data = NULL;
	ImapFolderSelectCompleteExitFunction(url, status, context);
	NET_FreeURLStruct(url);
}


void MSG_Pane::AdoptProgressContext(MWContext *context)
{
	if (m_progressContext)
		PW_DestroyProgressContext(m_progressContext);
	m_progressContext = context;
}


#ifdef GENERATINGPOWERPC
#pragma global_optimizer on
#endif

char*
MSG_Pane::MakeMailto(const char *to, const char *cc,
					const char *newsgroups,
					const char *subject, const char *references,
					const char *attachment, const char *host_data,
					XP_Bool xxx_p, XP_Bool sign_p)
{
	char *to2 = 0, *cc2 = 0;
	char *out, *head;
	char *qto, *qcc, *qnewsgroups, *qsubject, *qreferences;
	char *qattachment, *qhost_data;
	char *url;
	char *me = MIME_MakeFromField();
	char *to_plus_me = 0;

	to2 = MSG_RemoveDuplicateAddresses (to, ((cc && *cc) ? me : 0), TRUE /*removeAliasesToMe*/);
	if (to2 && !*to2) {
		XP_FREE(to2);
		to2 = 0;
	}

	/* This to_plus_me business is so that, in reply-to-all of a message
	   to which I was a recipient, I don't go into the CC field (that's
	   what BCC/FCC are for.) */
	if (to2 && cc && me) {
		to_plus_me = (char *) XP_ALLOC(XP_STRLEN(to2) + XP_STRLEN(me) + 10);
	}
	if (to_plus_me) {
		XP_STRCPY(to_plus_me, me);
		XP_STRCAT(to_plus_me, ", ");
		XP_STRCAT(to_plus_me, to2);
	}
	FREEIF(me);

	cc2 = MSG_RemoveDuplicateAddresses (cc, (to_plus_me ? to_plus_me : to2), TRUE /*removeAliasesToMe*/);
	if (cc2 && !*cc2) {
		XP_FREE(cc2);
		cc2 = 0;
	}

	FREEIF(to_plus_me);

	/* Catch the case of "Reply To All" on a message that was from me.
	   In that case, we've got an empty To: field at this point.
	   What we should do is, promote the first CC address to the To:
	   field.  But I'll settle for promoting all of them.
	   */
	if (cc2 && *cc2 && (!to2 || !*to2)) {
		FREEIF(to2);
		to2 = cc2;
		cc2 = 0;
	}

	qto		  = to2		   ? NET_Escape (to2, URL_XALPHAS)		  : 0;
	qcc		  = cc2		   ? NET_Escape (cc2, URL_XALPHAS)		  : 0;
	qnewsgroups = newsgroups ? NET_Escape (newsgroups, URL_XALPHAS) : 0;
	qsubject	  = subject    ? NET_Escape (subject, URL_XALPHAS)    : 0;
	qreferences = references ? NET_Escape (references, URL_XALPHAS) : 0;
	qattachment = attachment ? NET_Escape (attachment, URL_XALPHAS) : 0;
	qhost_data  = host_data  ? NET_Escape (host_data, URL_XALPHAS)  : 0;

	url = (char *)
		XP_ALLOC ((qto         ? XP_STRLEN(qto)			+ 15 : 0) +
				  (qcc         ? XP_STRLEN(qcc)			+ 15 : 0) +
				  (qnewsgroups ? XP_STRLEN(qnewsgroups) + 15 : 0) +
				  (qsubject    ? XP_STRLEN(qsubject)	+ 15 : 0) +
				  (qreferences ? XP_STRLEN(qreferences) + 15 : 0) +
				  (qhost_data  ? XP_STRLEN(qhost_data)  + 15 : 0) +
				  (qattachment ? XP_STRLEN(qattachment) + 15 : 0) +
				  60);
	if (!url) goto FAIL;
	XP_STRCPY (url, "mailto:");
	head = url + XP_STRLEN (url);
	out = head;
# define PUSH_STRING(S) XP_STRCPY(out, S), out += XP_STRLEN(S)
# define PUSH_PARM(prefix,var) \
	if (var) {				   \
		if (out == head)	   \
			*out++ = '?';	   \
		else				   \
			*out++ = '&';	   \
		PUSH_STRING (prefix);  \
		*out++ = '=';		   \
		PUSH_STRING (var);	   \
	}						   \

		PUSH_PARM("to", qto);
		PUSH_PARM("cc", qcc);
		PUSH_PARM("newsgroups", qnewsgroups);
		PUSH_PARM("subject", qsubject);
		PUSH_PARM("references", qreferences);
		PUSH_PARM("attachment", qattachment);
		PUSH_PARM("newshost", qhost_data);
		{
		  char *t = "true";  /* avoid silly compiler warning */
		  HG92725
		  if (sign_p)    PUSH_PARM("sign", t);
		}
# undef PUSH_PARM
# undef PUSH_STRING

FAIL:
		FREEIF (to2);
		FREEIF (cc2);
		FREEIF (qto);
		FREEIF (qcc);
		FREEIF (qnewsgroups);
		FREEIF (qsubject);
		FREEIF (qreferences);
		FREEIF (qattachment);
		FREEIF (qhost_data);

		return url;
}

#ifdef GENERATINGPOWERPC
#pragma global_optimizer off
#endif


MSG_PaneURLChain::MSG_PaneURLChain(MSG_Pane *pane)
{
	m_pane = pane;
}

MSG_PaneURLChain::~MSG_PaneURLChain()
{
}

// override to chain urls. return non-zero to continue.
int	MSG_PaneURLChain::GetNextURL()
{
	return 0;
}

MSG_PostDeliveryActionInfo::MSG_PostDeliveryActionInfo(MSG_FolderInfo *folder)
{
  //XP_ASSERT (folder); //*jefft allowing this in case of the folder was not subscribed
  m_folderInfo = folder;
}

#ifdef XP_WIN

MSG_SaveMessagesAsTextState::MSG_SaveMessagesAsTextState 
	(MSG_Pane *pane, const IDArray &idArray, XP_File file)
{
	XP_ASSERT(pane);
	m_pane = pane;

	int32 width = 76;
	uint i,size = idArray.GetSize();

	XL_InitializeTextSetup(&m_print);

	PREF_GetIntPref("mailnews.wraplenth", &width);
	if (width == 0) width = 72;
	else if (width < 10) width = 10;
	else if (width > 30000) width = 30000;
	m_print.width = (int) width;

	m_print.carg = this;
	m_print.completion = MSG_SaveMessagesAsTextState::SaveMsgAsTextComplete;
	m_print.out = file;
	for (i = 0; i < size; i++)
		m_msgKeys.Add(idArray.GetAt(i));

	m_curMsgIndex = -1;
}

MSG_SaveMessagesAsTextState::~MSG_SaveMessagesAsTextState()
{
	XP_FileClose(m_print.out);
	if (m_print.url)
		NET_FreeURLStruct(m_print.url);
}

void MSG_SaveMessagesAsTextState::SaveNextMessage()
{
	XP_ASSERT (m_curMsgIndex < (int) m_msgKeys.GetSize());
	++m_curMsgIndex;
	if (m_print.url)
		NET_FreeURLStruct(m_print.url);

	m_print.url = m_pane->ConstructUrlForMessage (m_msgKeys.GetAt(m_curMsgIndex));
	if (!m_print.url) 
		delete this;
	else
		XL_TranslateText(m_pane->GetContext(), m_print.url, &m_print);
}

/* static */ void
MSG_SaveMessagesAsTextState::SaveMsgAsTextComplete(PrintSetup *print)
{
	// Something is really wrong if print is NULL
	XP_ASSERT (print);
	if (!print) return;

	MSG_SaveMessagesAsTextState *saveMsgState = 
		(MSG_SaveMessagesAsTextState *) print->carg;
	
	XP_ASSERT(saveMsgState);
	if (!saveMsgState)
	{
		// something is really wrong do as much as we can then return
		XP_FileClose(print->out);
		return;
	}
	else
	{
		if ((saveMsgState->m_curMsgIndex+1)<
			(int) saveMsgState->m_msgKeys.GetSize())
		{
			XP_FileWrite(LINEBREAK, LINEBREAK_LEN, saveMsgState->m_print.out);
			saveMsgState->SaveNextMessage();
		}
		else
			delete saveMsgState;
	}
}

#endif
