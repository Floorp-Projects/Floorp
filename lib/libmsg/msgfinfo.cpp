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
#include "csid.h" // for CS_UNKNOWN

#include "msgimap.h"
#include "maildb.h"
#include "mailhdr.h"
#include "msgprefs.h"
#include "msgfpane.h"

#include "grpinfo.h"
#include "newsdb.h"
#include "msgtpane.h"
#include "msgspane.h"
#include "msgmpane.h"
#include "msgdbvw.h"
#include "prsembst.h"
#include "xpgetstr.h"
#include "nwsartst.h"
#include "msgundmg.h"
#include "msgundac.h"
#include "maildb.h"
#include "xplocale.h"
#include "msgurlq.h"
#include "newshost.h"
#include "hosttbl.h"
#include "newsset.h"
#include "grec.h"
#include "prefapi.h"
#include "newsdb.h"
#include "idarray.h"
#include "imapoff.h"
#include "imaphost.h"
#include "msgbiff.h"

#if defined(XP_WIN16) || defined(XP_OS2)
#define MAX_FILE_LENGTH_WITHOUT_EXTENSION 8
#elif defined(XP_MAC)
#define MAX_FILE_LENGTH_WITHOUT_EXTENSION 26
#elif defined(XP_WIN32)
#define MAX_FILE_LENGTH_WITHOUT_EXTENSION 256
#else
#define MAX_FILE_LENGTH_WITHOUT_EXTENSION 32000
#endif


void PostMessageCopyUrlExitFunc (URL_Struct *URL_s, int status, MWContext *window_id);
void OfflineOpExitFunction (URL_Struct *URL_s, int status, MWContext *window_id);

/* Some prototypes for routines in mkpop3.c */

extern "C" char* ReadPopData(char *name);
extern "C" void SavePopData(char *data);
extern "C" void net_pop3_delete_if_in_server(char *data, char *uidl, XP_Bool *changed);
extern "C" void KillPopData(char* data);


extern "C"
{
    extern int MK_MSG_ERROR_WRITING_MAIL_FOLDER;
    extern int MK_OUT_OF_MEMORY;
    extern int MK_MSG_CANT_COPY_TO_SAME_FOLDER;
    extern int MK_MSG_CANT_COPY_TO_QUEUE_FOLDER;
    extern int MK_MSG_CANT_COPY_TO_QUEUE_FOLDER_OLD;
    extern int MK_MSG_CANT_COPY_TO_DRAFTS_FOLDER;
    extern int MK_MSG_CANT_CREATE_FOLDER;
    extern int MK_COULD_NOT_CREATE_DIRECTORY;
    extern int MK_UNABLE_TO_DELETE_FILE;
    extern int MK_UNABLE_TO_OPEN_FILE;
    extern int MK_MSG_FOLDER_ALREADY_EXISTS;
    extern int MK_MSG_FOLDER_BUSY;
    extern int MK_MSG_CANT_DELETE_NEWS_DB;
	extern int MK_MSG_IMAP_CONTAINER_NAME;
	extern int MK_MSG_CANT_MOVE_FOLDER;
	extern int MK_MSG_ONLINE_IMAP_WITH_NO_BODY;
	extern int MK_MSG_LOCAL_MAIL;
	extern int MK_NEWS_DISCUSSIONS_ON;
	extern int MK_MSG_DELETING_MSGS_STATUS;
	extern int MK_MSG_COPYING_MSGS_STATUS;
	extern int MK_MSG_MOVING_MSGS_STATUS;
	extern int MK_MSG_COPY_TARGET_NO_SELECT;
	extern int MK_MSG_TMP_FOLDER_UNWRITABLE;
	extern int MK_POP3_OUT_OF_DISK_SPACE;
}

MSG_FolderInfo::MSG_FolderInfo(const char* n, uint8 d, XPCompareFunc* comparator)
	: m_subFolders(NULL)
	, m_flags(0)
	, m_name((n) ? XP_STRDUP(n) : 0)			
{

    m_depth = d;
    m_numUnreadMessages = -1;
    m_numTotalMessages = 0;
    m_master = NULL;
    m_semaphoreHolder = NULL;
    m_prefFlags = 0;
	m_csid = CS_UNKNOWN;
	m_lastMessageLoaded	= MSG_MESSAGEKEYNONE;
	m_numPendingUnreadMessages = 0;
	m_numPendingTotalMessages  = 0;
	m_subFolders = new MSG_FolderArray (comparator);

	m_isCachable = TRUE;
}

MSG_FolderInfo::~MSG_FolderInfo() 
{
#ifdef DEBUG
	m_subFolders->VerifySort();
#endif // DEBUG
    for (int i = 0; i < m_subFolders->GetSize(); i++)
        delete m_subFolders->GetAt(i);

    delete m_subFolders;
    
	if (m_name)
        XP_FREE(m_name);
}

MsgERR MSG_FolderInfo::OnCloseFolder ()
{
	return eSUCCESS;
}

MWContext *MSG_FolderInfo::GetFolderPaneContext()
{
    // find the folder pane context
    MSG_Pane  *folderPane        = m_master->FindFirstPaneOfType(MSG_FOLDERPANE);
    MWContext *folderPaneContext = NULL;
    if (folderPane)
        folderPaneContext = folderPane->GetContext();
    if (!folderPane || !folderPaneContext)
    {
        XP_ASSERT(FALSE);
        return NULL;
    }
    return folderPaneContext;
}


void MSG_FolderInfo::StartAsyncCopyMessagesInto (MSG_FolderInfo *dstFolder, 
                                  MSG_Pane* sourcePane,
								  MessageDB *sourceDB,
                                  IDArray *srcArray, 
                                  int32 srcCount,
                                  MWContext *currentContext,
                                  MSG_UrlQueue *urlQueue,
                                  XP_Bool deleteAfterCopy,
                                  MessageKey nextKeyToLoad)
{
	// General note: If either the source or destination folder is an IMAP folder then we add the copy info struct
	// to the end of the current context's chain of copy info structs then fire off an IMAP URL.
	// However, local folders don't work this way! We must add the copy info struct to the URL queue where it will be fired
	// at its leisure. 

    MessageCopyInfo *copyInfo = (MessageCopyInfo *) XP_ALLOC(sizeof(MessageCopyInfo));
    
    if (copyInfo)
    {
        XP_BZERO (copyInfo, sizeof(MessageCopyInfo));
        copyInfo->srcFolder = this;
        copyInfo->dstFolder = dstFolder;
        copyInfo->nextCopyInfo = NULL;
        copyInfo->dstIMAPfolderUpdated=FALSE;
        copyInfo->offlineFolderPositionOfMostRecentMessage = 0;
		copyInfo->srcDB = sourceDB;
        copyInfo->srcArray  = srcArray;
        copyInfo->srcCount  = srcCount;
        
        copyInfo->moveState.ismove     = deleteAfterCopy;
        copyInfo->moveState.sourcePane = sourcePane;
        copyInfo->moveState.nextKeyToLoad = nextKeyToLoad;
        copyInfo->moveState.urlForNextKeyLoad = NULL;
        copyInfo->moveState.moveCompleted = FALSE;
        copyInfo->moveState.finalDownLoadMessageSize = 0;
        copyInfo->moveState.imap_connection = 0;
		copyInfo->moveState.haveUploadedMessageSize = FALSE;
        
        MsgERR openErr = eSUCCESS;
        XP_Bool wasCreated;
        if (dstFolder->GetType() == FOLDER_MAIL)
            openErr = MailDB::Open (dstFolder->GetMailFolderInfo()->GetPathname(), FALSE, &copyInfo->moveState.destDB, FALSE);
        else if (dstFolder->GetType() == FOLDER_IMAPMAIL && !IsNews())
            openErr = ImapMailDB::Open (dstFolder->GetMailFolderInfo()->GetPathname(), FALSE, &copyInfo->moveState.destDB, 
                                        sourcePane->GetMaster(), &wasCreated);

                    
        if (!dstFolder->GetMailFolderInfo() || (openErr != eSUCCESS))
            copyInfo->moveState.destDB = NULL;
        
        // let the front end know that we are starting a long update
        sourcePane->StartingUpdate(MSG_NotifyNone, 0, 0);
        
        if ((this->GetType() == FOLDER_IMAPMAIL) || (dstFolder->GetType() == FOLDER_IMAPMAIL))
        {
			// add this copyinfo struct to the end
			if (currentContext->msgCopyInfo != NULL)
			{
        		MessageCopyInfo *endingNode = currentContext->msgCopyInfo;
        		while (endingNode->nextCopyInfo != NULL)
        			endingNode = endingNode->nextCopyInfo;
        		endingNode->nextCopyInfo = copyInfo;
			}
			else
        		currentContext->msgCopyInfo = copyInfo;

            // BeginCopyMessages will fire an IMAP url.  The IMAP
            // module will call FinishCopyMessages so that the whole
            // shebang is handled as one IMAP url.  Previously the copy
            // happened with a mailbox url and IMAP url running together
            // in the same context.  This worked on mac only.
            MsgERR copyErr = BeginCopyingMessages(dstFolder, sourceDB, srcArray,urlQueue,srcCount,copyInfo);
            if (0 != copyErr)
            {
       			CleanupCopyMessagesInto(&currentContext->msgCopyInfo);
       			
       			if (!NET_IsOffline() && ((int32) copyErr < -1) )
        			FE_Alert (sourcePane->GetContext(), XP_GetString(copyErr));
            }
        }
        else
        {
			// okay, add this URL to our URL queue.
			URL_Struct *url_struct = NET_CreateURLStruct("mailbox:copymessages", NET_DONT_RELOAD);
			if (url_struct) 
			{
				MSG_UrlQueue::AddLocalMsgCopyUrlToPane(copyInfo, url_struct, PostMessageCopyUrlExitFunc, sourcePane, FALSE);
			}
        }
    }
}


void MSG_FolderInfo::CleanupCopyMessagesInto (MessageCopyInfo **info)
{
	if (!info || !*info)
		return;

	MSG_Pane *sourcePane = (*info)->moveState.sourcePane;

	XP_Bool searchPane = sourcePane ? sourcePane->GetPaneType() == MSG_SEARCHPANE : FALSE;
	
    if ((*info)->moveState.destDB != NULL)
    {
        (*info)->moveState.destDB->Close();
        (*info)->moveState.destDB = NULL;
    }
    if ((*info)->dstFolder->TestSemaphore(this))
        (*info)->dstFolder->ReleaseSemaphore(this);

	// if we were a search pane, and an error occurred, close the view on this action..
	if (sourcePane != NULL && searchPane)
		((MSG_SearchPane *) sourcePane)->CloseView((*info)->srcFolder);
		
    
    // tell the fe that we are finished with
    // out backend driven update.  They can
    // now do things like load the next message.
    
    // now that an imap copy message is at most 2 urls, we can end the
    // the update here.  Now this is this only ending update and resource
    // cleanup for message copying  
    sourcePane->EndingUpdate(MSG_NotifyNone, 0, 0);
	
	// tell the FE that we're done copying so they can re-enable 
	// selection if they've decided to disable it during the copy
	
	// I don't think we want to do this if we are a search pane but i haven't been able to 
	// justify why yet!!

	if (!searchPane)
		FE_PaneChanged(sourcePane, TRUE, MSG_PaneNotifyCopyFinished, 0);

    // EndingUpdate may have caused an interruption of this context and cleaning up the 
    // url queue may have deleted this  MessageCopyInfo already
    if (*info)
    {
		MessageCopyInfo *deleteMe = *info;
		*info = deleteMe->nextCopyInfo;        // but nextCopyInfo == NULL. this causes the fault later on  (MSCOTT)
		XP_FREE(deleteMe);
    }
}


XP_Bool MSG_FolderInfo::HasSubFolders() const
{
    return m_subFolders->GetSize() > 0;
}

int MSG_FolderInfo::GetNumSubFolders() const
{
    return m_subFolders->GetSize();
}

MSG_FolderInfo* MSG_FolderInfo::GetSubFolder(int which) const
{
    XP_ASSERT(which >= 0 && which < m_subFolders->GetSize());
    return m_subFolders->GetAt (which);
}

void MSG_FolderInfo::RemoveSubFolder (MSG_FolderInfo *which)
{
    int idx = m_subFolders->FindIndex (0, which);
    if (idx != -1)
        m_subFolders->RemoveAt (idx);
    else
        XP_ASSERT(FALSE); //someone asked to remove a folder we don't own

	XP_ASSERT(m_subFolders->FindIndex(0, which) == -1);
    if (m_subFolders->GetSize() == 0)
    {
		// Our last child was deleted, so reset our hierarchy bits and tell the panes that this 
		// folderInfo changed, which eventually redraws the expand/collapse widget in the folder pane
        m_flags &= ~MSG_FOLDER_FLAG_DIRECTORY;
        m_flags &= ~MSG_FOLDER_FLAG_ELIDED;
		m_master->BroadcastFolderChanged (this);
    }

}

XP_Bool MSG_FolderInfo::GetAdminUrl(MWContext * /* context */, MSG_AdminURLType /* type */)
{
	return FALSE;
}

XP_Bool MSG_FolderInfo::HaveAdminUrl(MSG_AdminURLType /* type */)
{
	return FALSE;
}

XP_Bool MSG_FolderInfo::DeleteIsMoveToTrash()
{
	return FALSE;
}

XP_Bool MSG_FolderInfo::ShowDeletedMessages()
{
	return FALSE;
}


void MSG_FolderInfo::ChangeNumPendingUnread(int32 delta)
{
    DBFolderInfo   *folderInfo;
    MessageDB       *db;
    MsgERR err = GetDBFolderInfoAndDB(&folderInfo, &db);
    if (err == eSUCCESS)
    {
        if (folderInfo)
        {
            m_numPendingUnreadMessages += delta;
            folderInfo->ChangeImapUnreadPendingMessages(delta);
        }
        if (db)
            db->Close();
    }
}

void MSG_FolderInfo::ChangeNumPendingTotalMessages(int32 delta)
{
    DBFolderInfo   *folderInfo;
    MessageDB       *db;
    MsgERR err = GetDBFolderInfoAndDB(&folderInfo, &db);
    if (err == eSUCCESS)
    {
        if (folderInfo)
        {
            m_numPendingTotalMessages += delta;
            folderInfo->ChangeImapTotalPendingMessages(delta);
        }
        if (db)
            db->Close();
    }
}


void    MSG_FolderInfo::SetFolderPrefFlags(int32 flags)
{
    DBFolderInfo   *folderInfo;
    MessageDB       *db;

	flags |= MSG_FOLDER_PREF_CACHED;

	// don't do anything if the flags aren't changing (modulo the cached bit)
	if (flags != GetFolderPrefFlags())
	{
		MsgERR err = GetDBFolderInfoAndDB(&folderInfo, &db);
		if (err == eSUCCESS)
		{
			if (folderInfo)
			{
				m_prefFlags = flags;
				folderInfo->SetFlags(flags & ~MSG_FOLDER_PREF_CACHED);
			}
			if (db)
				db->Close();
		}
	}
}

int32   MSG_FolderInfo::GetFolderPrefFlags()
{
	ReadDBFolderInfo();
    return m_prefFlags;
}

void MSG_FolderInfo::SetFolderCSID(int16 csid)
{
    DBFolderInfo   *folderInfo;
    MessageDB       *db;
    MsgERR err = GetDBFolderInfoAndDB(&folderInfo, &db);
    if (err == eSUCCESS)
    {
        if (folderInfo)
        {
            m_csid = csid;
            folderInfo->SetCSID(m_csid);
        }
        if (db)
            db->Close();
    }
}

int16 MSG_FolderInfo::GetFolderCSID()
{
	ReadDBFolderInfo();
	return m_csid;
}

void MSG_FolderInfo::SetLastMessageLoaded(MessageKey lastMessageLoaded)
{
    DBFolderInfo   *folderInfo;
    MessageDB       *db;
    MsgERR err = GetDBFolderInfoAndDB(&folderInfo, &db);
    if (err == eSUCCESS)
    {
        if (folderInfo)
        {
            m_lastMessageLoaded = lastMessageLoaded;
            folderInfo->SetLastMessageLoaded(lastMessageLoaded);
        }
        if (db)
            db->Close();
    }
}

MessageKey MSG_FolderInfo::GetLastMessageLoaded()
{
	ReadDBFolderInfo();
	return m_lastMessageLoaded;
}

void MSG_FolderInfo::RememberPassword(const char * /* password */)
{
}

char *MSG_FolderInfo::GetRememberedPassword()
{
	return NULL;
}

XP_Bool MSG_FolderInfo::UserNeedsToAuthenticateForFolder(XP_Bool displayOnly)
{
	return FALSE;
}

const char *MSG_FolderInfo::GetUserName()
{
	return "";
}
const char *MSG_FolderInfo::GetHostName()
{
	return "";
}

const char *MSG_FolderInfoMail::GetUserName()
{
	return NET_GetPopUsername();
}

const char *MSG_FolderInfoMail::GetHostName()
{
	return m_master->GetPrefs()->GetPopHost();
}

// We're a local folder - if we're using pop, check to see if we've got the pop password.
// If we're using imap, check if we've cached the password on the default imap host.
char *MSG_FolderInfoMail::GetRememberedPassword()
{
	XP_Bool serverIsIMAP = m_master->GetPrefs()->GetMailServerIsIMAP4();
	char *savedPassword = NULL; 
	if (serverIsIMAP)
	{
		MSG_IMAPHost *defaultIMAPHost = m_master->GetIMAPHostTable()->GetDefaultHost();
		if (defaultIMAPHost)
		{
			MSG_FolderInfo *hostFolderInfo = defaultIMAPHost->GetHostFolderInfo();
			MSG_FolderInfo *defaultHostIMAPInbox = NULL;
			if (hostFolderInfo->GetFoldersWithFlag(MSG_FOLDER_FLAG_INBOX, &defaultHostIMAPInbox, 1) == 1 
				&& defaultHostIMAPInbox != NULL)
			{
				savedPassword = defaultHostIMAPInbox->GetRememberedPassword();
			}
		}
	}
	else
	{
		MSG_FolderInfo *offlineInbox = NULL;
		if (m_flags & MSG_FOLDER_FLAG_INBOX)
		{
			char *retPassword = NULL;
			MailDB *mailDb = NULL;
			XP_Bool wasCreated=FALSE;
			MailDB::Open(m_pathName, FALSE, &mailDb, FALSE);
			if (mailDb)
			{
				XPStringObj cachedPassword;
				mailDb->GetCachedPassword(cachedPassword);
				retPassword = XP_STRDUP(cachedPassword);
				mailDb->Close();

			}
			return retPassword;
		}
		if (m_master->GetLocalMailFolderTree()->GetFoldersWithFlag(MSG_FOLDER_FLAG_INBOX, &offlineInbox, 1) && offlineInbox)
			savedPassword = offlineInbox->GetRememberedPassword();
	}
	return savedPassword;
}


XP_Bool MSG_FolderInfoMail::UserNeedsToAuthenticateForFolder(XP_Bool /* displayOnly */)
{
	XP_Bool ret = FALSE;
	if (m_master->IsCachePasswordProtected() && !m_master->IsUserAuthenticated() && !m_master->AreLocalFoldersAuthenticated())
	{
		char *savedPassword = GetRememberedPassword();
		if (savedPassword && XP_STRLEN(savedPassword))
			ret = TRUE;
		FREEIF(savedPassword);
	}
	return ret;
}


int32 MSG_FolderInfo::GetNumPendingUnread(XP_Bool deep) const
{
    int32 total = m_numPendingUnreadMessages;
    if (deep)
    {
        MSG_FolderInfo *folder;
        for (int i = 0; i < m_subFolders->GetSize(); i++)
        {
            folder = m_subFolders->GetAt(i);
			if (folder)
				total += folder->GetNumPendingUnread(deep);
        }
    }
    return total;
}

int32 MSG_FolderInfo::GetNumPendingTotalMessages(XP_Bool deep) const
{
    int32 total = m_numPendingTotalMessages;
    if (deep)
    {
        MSG_FolderInfo *folder;
        for (int i = 0; i < m_subFolders->GetSize(); i++)
        {
            folder = m_subFolders->GetAt(i);
            total += folder->GetNumPendingTotalMessages(deep);
        }
    }
    return total;
}


int32 MSG_FolderInfo::GetNumUnread(XP_Bool deep) const
{
    int32 total = m_numUnreadMessages;
    if (deep)
    {
        MSG_FolderInfo *folder;
        for (int i = 0; i < m_subFolders->GetSize(); i++)
        {
            folder = m_subFolders->GetAt(i);
			if (folder)
			{
				int32 num = folder->GetNumUnread(deep);
				if (num >= 0) // it's legal for counts to be negative if we don't know
					total += num;
			}
        }
    }
    return total;
}

int32 MSG_FolderInfo::GetTotalMessages(XP_Bool deep) const
{
    int32 total = m_numTotalMessages;
    if (deep)
    {
        MSG_FolderInfo *folder;
        for (int i = 0; i < m_subFolders->GetSize(); i++)
        {
            folder = m_subFolders->GetAt(i);
            int32 num = folder->GetTotalMessages (deep);
            if (num >= 0) // it's legal for counts to be negative if we don't know
                total += num;
        }
    }
    return total;
}

int32	MSG_FolderInfo::GetExpungedBytesCount() const
{
	return 0;
}


const char *MSG_FolderInfo::GetPrettyName()  
{
	return m_name;
}

// return pretty name if any, otherwise ugly name.
const char *MSG_FolderInfo::GetPrettiestName()
{
	const char *prettyName = GetPrettyName();
	return (prettyName) ? prettyName : GetName();
}

int MSG_FolderInfo::GetNumSubFoldersToDisplay() const 
{
	return GetNumSubFolders();
}

MSG_FolderArray *MSG_FolderInfo::GetSubFolders () 
{
	return m_subFolders; 
}

void MSG_FolderInfo::GetVisibleSubFolders (MSG_FolderArray &subFolders)
{
	// The folder pane uses this routine to work around the fact
	// that unsubscribed newsgroups are children of the news host.
	// We can't count those when computing folder pane view indexes.

	for (int i = 0; i < m_subFolders->GetSize(); i++)
	{
		MSG_FolderInfo *f = m_subFolders->GetAt(i);
		if (f && f->CanBeInFolderPane())
			subFolders.Add (f);
	}
}

void MSG_FolderInfo::SummaryChanged()
{
    UpdateSummaryTotals();
    if (m_master)
        m_master->BroadcastFolderChanged(this);
}

MsgERR MSG_FolderInfo::Rename (const char *newName)
{
    MsgERR status = 0;
    MSG_FolderInfo *parentFolderInfo;

	parentFolderInfo = m_master->GetFolderTree()->FindParentOf(this);
	parentFolderInfo->m_subFolders->Remove(this);
	SetName(newName);
	parentFolderInfo->m_subFolders->Add(this);
	if (!GetName())
        status = MK_OUT_OF_MEMORY;
    return status;
}

const char* MSG_FolderInfo::GetName() 
{   // Name of this folder (as presented to user).
    return m_name;
}
    
void MSG_FolderInfo::SetName(const char *name) 
{
	FREEIF(m_name);
	m_name = XP_STRDUP(name);
}


XP_Bool MSG_FolderInfo::ContainsChildNamed (const char *name)
{
	return FindChildNamed(name) != NULL;
}

MSG_FolderInfo *MSG_FolderInfo::FindChildNamed (const char *name)
{
    MSG_FolderInfo *folder = NULL;

    for (int i = 0; i < m_subFolders->GetSize(); i++)
    {
        folder = m_subFolders->GetAt(i);
        // case-insensitive compare is probably LCD across OS filesystems
        if (!strcasecomp(folder->GetName(), name))
            return folder;
    }

    return NULL;
}

MSG_FolderInfo *MSG_FolderInfo::FindParentOf (MSG_FolderInfo *child)
{
    MSG_FolderInfo *result = NULL;

    for (int i = 0; i < m_subFolders->GetSize() && result == NULL; i++)
    {
        if (m_subFolders->GetAt (i) == child)
            result = this;
    }

    for (int j = 0; j < m_subFolders->GetSize() && result == NULL; j++)
    {
        MSG_FolderInfo *folder = m_subFolders->GetAt (j);
        result = folder->FindParentOf (child);
    }

    return result;
}

XP_Bool MSG_FolderInfo::IsParentOf (MSG_FolderInfo *child, XP_Bool deep)
{
    for (int i = 0; i < m_subFolders->GetSize(); i++)
    {
        MSG_FolderInfo *folder = m_subFolders->GetAt(i);
        if (folder == child || (deep && folder->IsParentOf(child, deep)))
            return TRUE;
    }
    return FALSE;
}

XP_Bool MSG_FolderInfo::UpdateSummaryTotals() 
{
	return FALSE;
} 

uint32  MSG_FolderInfo::GetFlags() const 
{
	return m_flags;
}

uint8   MSG_FolderInfo::GetDepth() const 
{
	return m_depth;
}

void    MSG_FolderInfo::SetDepth(uint8 depth) 
{
	m_depth = depth;
}

MSG_FolderInfoMail      *MSG_FolderInfo::GetMailFolderInfo() {return NULL;}
MSG_FolderInfoNews      *MSG_FolderInfo::GetNewsFolderInfo() {return NULL;}
MSG_IMAPFolderInfoMail	*MSG_FolderInfo::GetIMAPFolderInfoMail() {return NULL;}
MSG_IMAPFolderInfoContainer	*MSG_FolderInfo::GetIMAPFolderInfoContainer() {return NULL;}
MSG_IMAPHost			*MSG_FolderInfo::GetIMAPHost() {return NULL;}


XP_Bool					MSG_FolderInfo::CanCreateChildren () { return FALSE; }
XP_Bool					MSG_FolderInfo::CanBeRenamed () { return FALSE; }

void					MSG_FolderInfo::MarkAllRead(MWContext *context, XP_Bool deep) 
{
    MessageDB		*db;
    DBFolderInfo   *folderInfo;
	if (GetDBFolderInfoAndDB(&folderInfo, &db) == eSUCCESS)
	{
		if (db)
		{
			db->MarkAllRead(context);
			db->Close();
            UpdateSummaryTotals();
            GetMaster()->BroadcastFolderChanged(this);
		}
	}
	if (deep)
	{
		for (int i = 0; i < GetNumSubFolders(); i++)
		{
			MSG_FolderInfo *subFolder = GetSubFolder(i);
			if (subFolder)
				subFolder->MarkAllRead(context, deep);
		}
	}
}

int32					MSG_FolderInfo::GetTotalMessagesInDB() const 
{
	return m_numTotalMessages;	// news overrides this, since m_numTotalMessages for news is server-based
}


void MSG_FolderInfo::ToggleFlag (uint32 whichFlag)
{
    XP_ASSERT(whichFlag != MSG_FOLDER_FLAG_SENTMAIL);
    m_flags ^= whichFlag;
}


void MSG_FolderInfo::SetFlag (uint32 whichFlag)
{
    m_flags |= whichFlag;
}


void MSG_FolderInfo::ClearFlag(uint32 whichFlag)
{
    m_flags &= ~whichFlag;
}

void MSG_FolderInfo::SetFlagInAllFolderPanes(uint32 which)
{
	SetFlag(which);

	MSG_FolderPane *eachPane;
	for (eachPane = (MSG_FolderPane *) m_master->FindFirstPaneOfType(MSG_FOLDERPANE); eachPane; 
			eachPane =  (MSG_FolderPane *) m_master->FindNextPaneOfType(eachPane->GetNextPane(), MSG_FOLDERPANE))
	{
	    eachPane->SetFlagForFolder(this, which);
	}
}

void MSG_FolderInfo::GetExpansionArray (MSG_FolderArray &array)
{
    // the application of flags in GetExpansionArray is subtly different
    // than in GetFoldersWithFlag 

    for (int i = 0; i < m_subFolders->GetSize(); i++)
    {
        MSG_FolderInfo *folder = m_subFolders->GetAt(i);
		array.InsertAt(array.GetSize(), folder);
		if (!(folder->GetFlags() & MSG_FOLDER_FLAG_ELIDED))
			folder->GetExpansionArray(array);
    }
}


int32 MSG_FolderInfo::GetFoldersWithFlag(uint32 f, MSG_FolderInfo** result, int32 resultsize) 
{
    int num = 0;
    if ((f & m_flags) == f) {
        if (result && num < resultsize) {
            result[num] = this;
        }
        num++;
    }
    MSG_FolderInfo *folder = NULL;
    for (int i=0; i < m_subFolders->GetSize(); i++) {
        folder = m_subFolders->GetAt(i);
		// CAREFUL! if NULL ise passed in for result then the caller
		// still wants the full count!  Otherwise, the result should be at most the
		// number that the caller asked for.
		if (!result)
			num += folder->GetFoldersWithFlag(f, NULL, 0);
		else if (num < resultsize)
			num += folder->GetFoldersWithFlag(f, result + num, resultsize - num);
		else
			break;
    }
    return num;
}


char * MSG_FolderInfo::EscapeMessageId (const char *messageId)
{
    char *id = NULL;

    /* Take off bracketing <>, and quote special characters. */
    if (messageId[0] == '<') {
        char *i2;
        id = XP_STRDUP(messageId + 1);
        if (!id) 
            return 0;
        if (*id && id[XP_STRLEN(id) - 1] == '>')
            id[XP_STRLEN(id) - 1] = '\0';
        i2 = NET_Escape (id, URL_XALPHAS);
        XP_FREE (id);
        id = i2;
    }
    else
        id = NET_Escape (messageId, URL_XALPHAS);
    return id;
}


MsgERR MSG_FolderInfo::AcquireSemaphore (void *semHolder)
{
    XP_ASSERT(semHolder != NULL);
    MsgERR err = 0;

    if (m_semaphoreHolder == NULL)
        m_semaphoreHolder = semHolder;
    else
        err = MK_MSG_FOLDER_BUSY;

    return err;
}


void MSG_FolderInfo::ReleaseSemaphore (void *semHolder)
{
    XP_ASSERT(m_semaphoreHolder == semHolder);

    if (!m_semaphoreHolder || m_semaphoreHolder == semHolder)
        m_semaphoreHolder = NULL;
}


XP_Bool MSG_FolderInfo::RequiresCleanup()
{
	return FALSE;
}

void	MSG_FolderInfo::ClearRequiresCleanup()
{
}


XP_Bool MSG_FolderInfo::TestSemaphore (void *semHolder)
{
    XP_ASSERT(semHolder);

    if (m_semaphoreHolder == semHolder)
        return TRUE;
    return FALSE;
}


XP_Bool MSG_FolderInfo::CanBeInFolderPane () 
{ 
	return TRUE; 
}


XP_Bool MSG_FolderInfo::KnowsSearchNntpExtension()
{
	return FALSE;
}


XP_Bool MSG_FolderInfo::AllowsPosting ()
{
	return TRUE;
}

XP_Bool MSG_FolderInfo::DisplayRecipients ()
{
	if (m_flags & MSG_FOLDER_FLAG_SENTMAIL && !(m_flags & MSG_FOLDER_FLAG_INBOX))
		return TRUE;
	else 
	{
		// Only mail folders can be FCC folders
		if (m_flags & MSG_FOLDER_FLAG_MAIL || m_flags & MSG_FOLDER_FLAG_IMAPBOX) 
		{
			// There's one FCC folder for sent mail, and one for sent news
			MSG_FolderInfo *fccFolders[2]; 
			int numFccFolders = m_master->GetFolderTree()->GetFoldersWithFlag (MSG_FOLDER_FLAG_SENTMAIL, fccFolders, 2);
			for (int i = 0; i < numFccFolders; i++)
				if (fccFolders[i]->IsParentOf (this))
					return TRUE;
		}
	}
	return FALSE;
}

MsgERR MSG_FolderInfo::ReadDBFolderInfo (XP_Bool force /* = FALSE */)
{
	// Since it turns out to be pretty expensive to open and close
	// the DBs all the time, if we have to open it once, get everything
	// we might need while we're here

	MsgERR err = eUNKNOWN;
	if (force || !(m_prefFlags & MSG_FOLDER_PREF_CACHED))
    {
        DBFolderInfo   *folderInfo;
        MessageDB       *db;
        err = GetDBFolderInfoAndDB(&folderInfo, &db);
        if (err == eSUCCESS)
        {
			m_isCachable = TRUE;
            if (folderInfo)
            {
                m_prefFlags = folderInfo->GetFlags();
                m_prefFlags |= MSG_FOLDER_PREF_CACHED;
                folderInfo->SetFlags(m_prefFlags);

				m_numTotalMessages = folderInfo->GetNumMessages();
				m_numUnreadMessages = folderInfo->GetNumNewMessages();
				
				m_numPendingTotalMessages = folderInfo->GetImapTotalPendingMessages();
				m_numPendingUnreadMessages = folderInfo->GetImapUnreadPendingMessages();

				m_csid = folderInfo->GetCSID();
				if (db && !db->HasNew() && m_numPendingUnreadMessages <= 0)
					ClearFlag(MSG_FOLDER_FLAG_GOT_NEW);
            }
            if (db)
                db->Close();
        }
    }

	return err;
}

#define kFolderInfoCacheFormat1 "\t%08lX\t%08lX\t%ld\t%ld\t%d"
#define kFolderInfoCacheFormat2 "\t%08lX\t%08lX\t%ld\t%ld\t%ld\t%ld\t%d"

#define kFolderInfoCacheVersion 2

MsgERR MSG_FolderInfo::WriteToCache (XP_File f)
{
	// This function is coupled tightly with ReadFromCache,
	// and loosely with ReadDBFolderInfo

	XP_FilePrintf (f, "\t%d", kFolderInfoCacheVersion);
	XP_FilePrintf (f, kFolderInfoCacheFormat2, (long) m_flags, (long) m_prefFlags, 
		(long) m_numTotalMessages, (long) m_numPendingTotalMessages, 
		(long) m_numUnreadMessages, (long) m_numPendingUnreadMessages, (int) m_csid);

	return eSUCCESS;
}


MsgERR MSG_FolderInfo::ReadFromCache (char *buf)
{
	// This function is coupled tightly with WriteToCache,
	// and loosely with ReadDBFolderInfo

	MsgERR err = eSUCCESS;
	int version;
	int tokensRead = sscanf (buf, "\t%d", &version);
	SkipCacheTokens (&buf, tokensRead);

	if ((version == 1) || (version == 2))
	{
		uint32 tempFlags;
		int tempCsid;
		if (version == 1)
			sscanf (buf, kFolderInfoCacheFormat1, &tempFlags, &m_prefFlags, 
				&m_numTotalMessages, &m_numUnreadMessages, &tempCsid);
		else
			sscanf (buf, kFolderInfoCacheFormat2, &tempFlags, &m_prefFlags, 
				&m_numTotalMessages, &m_numPendingTotalMessages, &m_numUnreadMessages, &m_numPendingUnreadMessages, &tempCsid);

		m_prefFlags |= MSG_FOLDER_PREF_CACHED;
		// I'm chicken to just blast in all the flags, and the real
		// reason I'm storing them is to get elided, so just get that

		if (tempFlags & MSG_FOLDER_FLAG_DIRECTORY)
		{
			m_flags |= MSG_FOLDER_FLAG_DIRECTORY;
			if (!(tempFlags & MSG_FOLDER_FLAG_ELIDED))
				m_flags &= ~MSG_FOLDER_FLAG_ELIDED;
			else
				m_flags |= MSG_FOLDER_FLAG_ELIDED;
		}

		// Let's remember the IMAP folder types, too
		if (tempFlags & MSG_FOLDER_FLAG_IMAP_PERSONAL)
			m_flags |= MSG_FOLDER_FLAG_IMAP_PERSONAL;
		if (tempFlags & MSG_FOLDER_FLAG_IMAP_PUBLIC)
			m_flags |= MSG_FOLDER_FLAG_IMAP_PUBLIC;
		if (tempFlags & MSG_FOLDER_FLAG_IMAP_OTHER_USER)
			m_flags |= MSG_FOLDER_FLAG_IMAP_OTHER_USER;
		if (tempFlags & MSG_FOLDER_FLAG_PERSONAL_SHARED)
			m_flags |= MSG_FOLDER_FLAG_PERSONAL_SHARED;

		// Can't scan a %d into a short on 32 bit platforms
		m_csid = (int16) tempCsid;
	}
	else
		err = eUNKNOWN;

	return err;
}


XP_Bool MSG_FolderInfo::IsCachable()
{
	// If we haven't opened the DB for this, we don't know what the real
	// cache values should be, so don't write anything.

	return m_isCachable;
}


void MSG_FolderInfo::SkipCacheTokens (char **ppBuf, int numTokens)
{
	for (int i = 0; i < numTokens; i++)
	{
		while (**ppBuf == '\t')
			(*ppBuf)++;
		while (**ppBuf != '\t')
			(*ppBuf)++;
	}
}


const char *MSG_FolderInfo::GetRelativePathName ()
{
	return NULL;
}


/*static*/ int MSG_FolderInfo::CompareFolderDepth (const void *v1, const void *v2)
{
	MSG_FolderInfo *f1 = *(MSG_FolderInfo**) v1;
	MSG_FolderInfo *f2 = *(MSG_FolderInfo**) v2;

	// Sort shallowest folders to the top
	return f1->m_depth - f2->m_depth;
}


int32 MSG_FolderInfo::GetSizeOnDisk ()
{
	return 0;
}

const char *MSG_FolderInfo::GenerateUniqueSubfolderName(const char* /*prefix*/, MSG_FolderInfo* /*otherFolder*/)
{
	/* override in MSG_FolderInfoMail */
	return NULL;
}


static const char* NameFromPathname(const char* pathname) 
{
	char* ptr = XP_STRRCHR(pathname, '/');
	if (ptr) 
		return ptr + 1;
	return pathname;
}

int32	MSG_FolderInfoMail::GetExpungedBytesCount() const
{
	return m_expungedBytes;
}

void MSG_FolderInfoMail::SetExpungedBytesCount(int32 count)
{
	m_expungedBytes = count;
}


XP_Bool MSG_FolderInfoMail::IsMail () { return TRUE; }
XP_Bool MSG_FolderInfoMail::IsNews () { return FALSE; }
XP_Bool MSG_FolderInfoMail::CanCreateChildren () { return TRUE; }

#ifdef XP_WIN16
#define RETURN_COMPARE_LONG_VALUES(b,a) \
				if (b > a)\
					return 1;\
				else if (b < a)\
					return -1;\
				else\
					return 0;
#else
#define RETURN_COMPARE_LONG_VALUES(b,a) return b - a;
#endif

static inline uint32 MakeFakeSortFlags(uint32 folderFlags)
{
    const uint32 kSystemMailboxMask = (0x1F00);
	uint32 result = folderFlags & kSystemMailboxMask;
	if (folderFlags & MSG_FOLDER_FLAG_TEMPLATES)
		result = 0x3FF; // just below drafts.
	return result;
}

int MSG_FolderInfoMail::CompareFolders(const void* a, const void* b) 
{
    MSG_FolderInfoMail *aFolder = *((MSG_FolderInfoMail**) a);
    MSG_FolderInfoMail *bFolder = *((MSG_FolderInfoMail**) b);

    if (aFolder->GetDepth() == 2)
    {
        // This bit-twiddling controls the order that system mailboxes are
        // shown according to the bitflags and comments in msgcom.h. This code
        // relies on the bit positions of the flags (e.g. MSG_FOLDER_FLAG_INBOX)
		

        uint32 aFlags = MakeFakeSortFlags(aFolder->GetFlags());
        uint32 bFlags = MakeFakeSortFlags(bFolder->GetFlags());
        if (aFlags != 0 || bFlags != 0)
            if (aFlags != bFlags)
            	RETURN_COMPARE_LONG_VALUES(bFlags, aFlags);

		// Sort IMAP Namespaces at the end of the list
		const uint32 kImapNamespaceMask = 0x300000;
		uint32 nsFlagsA = aFolder->GetFlags() & kImapNamespaceMask;
		uint32 nsFlagsB = bFolder->GetFlags() & kImapNamespaceMask;
		if (nsFlagsA != 0 || nsFlagsB != 0)
			if (nsFlagsA != nsFlagsB)
            	RETURN_COMPARE_LONG_VALUES(nsFlagsA, nsFlagsB);

		// Else let the name break the tie (jrm had 2 sent-mail folders!!)
    }

    return strcasecomp(aFolder->GetName(), bFolder->GetName());
}


XP_Bool MSG_FolderInfoMail::ShouldIgnoreFile (const char *name)
{
    if (name[0] == '.' || name[0] == '#' || name[XP_STRLEN(name) - 1] == '~')
        return TRUE;

    if (!XP_STRCASECMP (name, "rules.dat"))
        return TRUE;

#if defined (XP_WIN) || defined (XP_MAC) || defined(XP_OS2)
    // don't add summary files to the list of folders;
    //don't add popstate files to the list either, or rules (sort.dat). 
    if ((XP_STRLEN(name) > 4 &&
        !XP_STRCASECMP(name + XP_STRLEN(name) - 4, ".snm")) ||
        !XP_STRCASECMP(name, "popstate.dat") ||
        !XP_STRCASECMP(name, "sort.dat") ||
        !XP_STRCASECMP(name, "mailfilt.log") ||
        !XP_STRCASECMP(name, "filters.js") ||
        !XP_STRCASECMP(name + XP_STRLEN(name) - 4, ".toc"))
        return TRUE;
#endif

    return FALSE;
}


// a factory that deals with pathname to long for us to append ".snm" to. Useful when the user imports
// a mailbox file with a long name.  If there is a new name then pathname is replaced.
MSG_FolderInfoMail *MSG_FolderInfoMail::CreateMailFolderInfo(char* pathname, MSG_Master *master, uint8 depth, uint32 flags)
{
	int renameStatus = 0;
	char *leafName = XP_STRDUP(NameFromPathname (pathname));	// we have to dup it because we need it after pathname is deleted
	
	char *mangledPath = NULL;
	char *mangledLeaf = NULL;

	const int charLimit = MAX_FILE_LENGTH_WITHOUT_EXTENSION;	// set on platform specific basis

	char *baseDir = XP_STRDUP(pathname);
	if (baseDir)
	{
		char *lastSlash = XP_STRRCHR(baseDir, '/');
		if (lastSlash)
			*lastSlash = '\0';
	}
	
	if (leafName && (XP_STRLEN(leafName) > charLimit))
		mangledLeaf = CreatePlatformLeafNameForDisk(leafName, master, baseDir);
	
	if (leafName && mangledLeaf && XP_STRCMP(leafName, mangledLeaf))
	{
		// the user must have imported a mailbox with a file name to long for this platform.
		mangledPath = XP_STRDUP(pathname);	// this is enough room, we will shorten it
		if (mangledPath)
		{
			char *leafPtr = XP_STRRCHR(mangledPath, '/');
			if (leafPtr)
			{
				XP_STRCPY(++leafPtr, mangledLeaf);
				
				// rename the mailbox file
				renameStatus = XP_FileRename(pathname, xpMailFolder, mangledPath, xpMailFolder);
				
				XP_FREE(pathname);
				pathname = mangledPath;
			}
		}
	}
	
	MSG_FolderInfoMail *newFolder = new MSG_FolderInfoMail (pathname, master, depth, flags);
	
	if (newFolder && mangledPath && (0 == renameStatus))
	{
		// if this folder was created with a new name, save the old pretty leaf name in the db
		MailDB  *mailDB;
		MsgERR openErr = MailDB::Open (newFolder->GetPathname(), TRUE, &mailDB, TRUE);;

		DBFolderInfo *info = NULL;
		if (openErr == eSUCCESS && mailDB)
		{
			info = mailDB->GetDBFolderInfo();
			newFolder->SetName(leafName);
			info->SetMailboxName(leafName); 
			mailDB->SetSummaryValid(FALSE);
			mailDB->Close();
		}
	}
	
	
	FREEIF(mangledLeaf);
	FREEIF(leafName);
	FREEIF(baseDir);
	
	return newFolder;
}

void
MSG_FolderInfoMail::SetPrefFolderFlag()
{
	const char *path = GetPathname();

	if (!path)
		return;

	char *localMailFcc = NULL;
	char *localNewsFcc = NULL;

	MSG_Prefs::GetXPDirPathPref("mail.default_fcc", FALSE, &localMailFcc);
	MSG_Prefs::GetXPDirPathPref("news.default_fcc", FALSE, &localNewsFcc);

	const char *mailDirectory = m_master->GetPrefs()->GetFolderDirectory();
	if (!localMailFcc || !*localMailFcc || XP_STRCMP(mailDirectory, localMailFcc) == 0)
	{
		XP_FREEIF(localMailFcc);
		localMailFcc = PR_smprintf("%s/%s", mailDirectory, SENT_FOLDER_NAME);
	}

	if (!localNewsFcc || !*localNewsFcc || XP_STRCMP(mailDirectory, localNewsFcc) == 0)
	{
		XP_FREEIF(localNewsFcc);
		localNewsFcc = PR_smprintf("%s/%s", mailDirectory, SENT_FOLDER_NAME);
	}

	if (XP_STRCMP(path, localMailFcc) == 0 || 
		XP_STRCMP(path, localNewsFcc) == 0)
		SetFlag(MSG_FOLDER_FLAG_SENTMAIL);
	else
		ClearFlag(MSG_FOLDER_FLAG_SENTMAIL);
		
	XP_FREEIF(localMailFcc);
	XP_FREEIF(localNewsFcc);

	char *draftsPath = msg_MagicFolderName (m_master->GetPrefs(),
											MSG_FOLDER_FLAG_DRAFTS, NULL);

	// +8 skip "mailbox:" part of url
	if ( draftsPath && NET_URL_Type(draftsPath) == MAILBOX_TYPE_URL &&
		 XP_STRCMP(draftsPath+8, mailDirectory) == 0)
	{
		// Default case
		XP_FREEIF(draftsPath);
		draftsPath = PR_smprintf("mailbox:%s/%s", mailDirectory, DRAFTS_FOLDER_NAME);
	}

	if ( draftsPath && ((NET_URL_Type(draftsPath) == MAILBOX_TYPE_URL &&
						 XP_STRCMP(path, draftsPath+8) == 0) ||
						XP_STRCMP(path, draftsPath) == 0) )
		SetFlag(MSG_FOLDER_FLAG_DRAFTS);
	else
		ClearFlag(MSG_FOLDER_FLAG_DRAFTS);
	
	XP_FREEIF(draftsPath);
	
	char *templatesPath = msg_MagicFolderName (m_master->GetPrefs(),
											   MSG_FOLDER_FLAG_TEMPLATES,
											   NULL); 

	if ( templatesPath && NET_URL_Type(templatesPath) == MAILBOX_TYPE_URL &&
		 XP_STRCMP(templatesPath+8, mailDirectory) == 0)
	{
		// Default case
		XP_FREEIF(templatesPath);
		templatesPath = PR_smprintf("mailbox:%s/%s", mailDirectory, TEMPLATES_FOLDER_NAME);
	}

	if ( templatesPath && 
		 ((NET_URL_Type(templatesPath) == MAILBOX_TYPE_URL && 
		   XP_STRCMP(path, templatesPath+8) == 0) ||
		  XP_STRCMP(path, templatesPath) == 0) )
		SetFlag(MSG_FOLDER_FLAG_TEMPLATES);
	else
		ClearFlag(MSG_FOLDER_FLAG_TEMPLATES);
	
	XP_FREEIF(templatesPath);
}

MSG_FolderInfoMail*
MSG_FolderInfoMail::BuildFolderTree (const char *path, uint8 depth, MSG_FolderArray *parentArray,
                                        MSG_Master *master, XP_Bool buildingImapTree /*= FALSE*/, MSG_IMAPHost *host /* = NULL */)
{
    const char *kDirExt = ".sbd";

    // newPath will be owned by the folder info, so don't delete it here
    char *newPath = (char*) XP_ALLOC(XP_STRLEN(path) + XP_STRLEN(kDirExt) + 1);
    if (!newPath)
        return NULL;
    XP_STRCPY (newPath, path);

    uint32 newFlags = MSG_FOLDER_FLAG_MAIL;
	const char *leafName = NameFromPathname (path);
	if (buildingImapTree)
    {
        const char *dbLeafName = leafName;
		// this allocates a string, so remember to free it at the end.
        leafName = MSG_IMAPFolderInfoMail::CreateMailboxNameFromDbName(dbLeafName);
    }

    if (depth == 2)             // Gross.  "depth 2" means that this is a top
                                // level folder.  (See, depth 0 is the root,
                                // and depth 1 is the container for all
                                // local mail, so depth 2 is top level folder.)
    {
        // certain folder names are magic when in the base directory
#ifdef XP_MAC
        char* escapedName = NULL;
        do {
	        escapedName = NET_Escape(INBOX_FOLDER_NAME, URL_PATH);
	        if (escapedName && !XP_FILENAMECMP(leafName, escapedName) && 
	            (!master->GetPrefs()->GetMailServerIsIMAP4() || buildingImapTree))
	        {
	            newFlags |= MSG_FOLDER_FLAG_INBOX;
	            break;
	        }
			XP_FREEIF(escapedName);   
	        escapedName = NET_Escape(TRASH_FOLDER_NAME, URL_PATH);
	        if (escapedName && !XP_FILENAMECMP(leafName, escapedName))
	        {
	            newFlags |= MSG_FOLDER_FLAG_TRASH;
	            break;
	        }
			XP_FREEIF(escapedName);   
	        escapedName = NET_Escape(MSG_GetQueueFolderName(), URL_PATH);
	        if (escapedName && !XP_FILENAMECMP(leafName, escapedName))
	        {
	            newFlags |= MSG_FOLDER_FLAG_QUEUE;
	            break;
	        }
			XP_FREEIF(escapedName);   
        } while (0);
        XP_FREEIF(escapedName);
#else
        if (!XP_FILENAMECMP(leafName, INBOX_FOLDER_NAME) && 
            (!master->GetPrefs()->GetMailServerIsIMAP4() || buildingImapTree))
            newFlags |= MSG_FOLDER_FLAG_INBOX;
        else if (!XP_FILENAMECMP(leafName, TRASH_FOLDER_NAME))
            newFlags |= MSG_FOLDER_FLAG_TRASH;
        else if (!XP_FILENAMECMP(leafName, MSG_GetQueueFolderName()))
            newFlags |= MSG_FOLDER_FLAG_QUEUE;
#endif
    }

    MSG_FolderInfoMail *newFolder = NULL;
    XP_Dir dir = XP_OpenDir (newPath, xpMailFolder);
    if (dir)
    {
        // newPath specifies a filesystem directory
        char *lastFour = &newPath[XP_STRLEN(newPath) - XP_STRLEN(kDirExt)];
        if (XP_STRCASECMP(lastFour, kDirExt))
        {
            // if the path doesn't contain .sbd, we can scan it. .sbd directories were
            // created with the Navigator UI, and will be picked up when their
            // corresponding mail folder is found.
            newFlags |= (MSG_FOLDER_FLAG_DIRECTORY | MSG_FOLDER_FLAG_ELIDED);
            if (buildingImapTree)
                newFolder = new MSG_IMAPFolderInfoMail (newPath, master, depth, newFlags, host);
            else
            	newFolder = new MSG_FolderInfoMail (newPath, master, depth, newFlags);
            if (newFolder)
			{
                newFolder->SetMaster(master);
				newFolder->SetPrefFolderFlag();
			}
            if (newFolder && parentArray)
                parentArray->Add (newFolder);

            XP_DirEntryStruct *entry = NULL;
            for (entry = XP_ReadDir(dir); entry; entry = XP_ReadDir(dir))
            {
                if (newFolder->ShouldIgnoreFile (entry->d_name))
                    continue;
                
                char *subName = (char*) XP_ALLOC(XP_STRLEN(newPath) + XP_STRLEN(entry->d_name) + 5);
                if (subName)
                {
                    XP_STRCPY (subName, newPath);
                    if (*entry->d_name != '/')
                        XP_STRCAT (subName, "/");
                    XP_STRCAT (subName, entry->d_name);
                    BuildFolderTree (subName, depth + 1, newFolder->m_subFolders, master, buildingImapTree, host);
                    FREEIF(subName);
                }
            }
        }
        else
        {
            // This is a .SBD directory that we created. We're going to ignore it
            // for the purpose of building the tree, so delete the pathname
			FREEIF(newPath);
        }
        XP_CloseDir (dir);
    }
    else
    {
        // newPath specifies a Berkeley mail folder or an imap db
        if (buildingImapTree)
        {
            char *imapBoxName = MSG_IMAPFolderInfoMail::CreateMailboxNameFromDbName(newPath);
            if (imapBoxName)
            {
                FREEIF(newPath);
                newPath = imapBoxName;
                newFolder = new MSG_IMAPFolderInfoMail (imapBoxName, master, depth, newFlags, host);
            }
        }
        else
        {
        	newFolder = CreateMailFolderInfo (newPath, master, depth, newFlags);
        }
        if (newFolder)
        {
			newFolder->SetMaster(master);
			newFolder->SetPrefFolderFlag();
			if (parentArray)
			{
				if (parentArray->FindIndex(0, newFolder) == -1)
					parentArray->Add (newFolder); // This maintains the sort order (XPSortedPtrArrray).
				else if (buildingImapTree)	// this is probably one of those duplicate imap databases - blow it away.
				{
					delete newFolder;
					newFolder = NULL;
		            char *imapBoxName = MSG_IMAPFolderInfoMail::CreateMailboxNameFromDbName(path);
					XP_FileRemove(imapBoxName, xpMailFolderSummary);
					XP_Trace("removing duplicate db %s\n", leafName);
					FREEIF(imapBoxName);
				}
			}

			if (newFolder)
			{
				newFolder->UpdateSummaryTotals();
				int32 flags = newFolder->GetFolderPrefFlags();	// cache folder pref flags

				// Look for a directory for this mail folder, and recurse into it.
				// e.g. if the folder is "inbox", look for "inbox.sbd". 
				XP_Dir subDir = XP_OpenDir(newFolder->GetPathname(), xpMailSubdirectory);
				if (subDir)
				{
					// If we knew it was a directory before getting here, we must have
					// found that out from the folder cache. In that case, the elided bit
					// is already what it should be, and we shouldn't change it. Otherwise
					// the default setting is collapsed.
					// NOTE: these flags do not affect the sort order, so we don't have to call
					// QuickSort after changing them.
					if (!(newFolder->m_flags & MSG_FOLDER_FLAG_DIRECTORY))
						newFolder->m_flags |= MSG_FOLDER_FLAG_ELIDED;
					newFolder->m_flags |= MSG_FOLDER_FLAG_DIRECTORY;

					XP_DirEntryStruct *entry = NULL;
					for (entry = XP_ReadDir(subDir); entry; entry = XP_ReadDir(subDir))
					{
						if (newFolder->ShouldIgnoreFile (entry->d_name))
							continue;

						char *subPath = (char*) XP_ALLOC(XP_STRLEN(newFolder->GetPathname()) + XP_STRLEN(kDirExt) + XP_STRLEN(entry->d_name) + 2);
						if (subPath)
						{
							XP_STRCPY (subPath, newFolder->GetPathname());
							XP_STRCAT (subPath, kDirExt);
							if (*entry->d_name != '/')
                        		XP_STRCAT (subPath, "/");
							XP_STRCAT (subPath, entry->d_name);
							BuildFolderTree (subPath, depth + 1, newFolder->m_subFolders, master, buildingImapTree, host);
							FREEIF(subPath);
						}
					}
					XP_CloseDir (subDir);
				}
			}
		}
    }
    
	if (buildingImapTree && leafName)
		XP_FREE((char *) leafName);

	if (newFolder && !buildingImapTree && (newFolder->GetFlags() & MSG_FOLDER_FLAG_INBOX))
	{
		// LATER ON WE WILL ENABLE ANY FOLDER, NOT JUST INBOXES, This is a POP3 folder, Imap folders
		// are added to the Biff Master in their constructor
		XP_Bool getMail = FALSE, activate = FALSE;
		int32 interval = 0;

		PREF_GetBoolPref("mail.check_new_mail", &getMail);
		PREF_GetIntPref("mail.check_time", &interval);

		MSG_Biff_Master::AddBiffFolder((char*) newFolder->GetPathname(), getMail,
						interval, TRUE, NULL /* no host name for POP */);
	}
    return newFolder;
}

MsgERR  MSG_FolderInfo::SaveMessages(IDArray * array, 
										 const char * fileName, 
                                         MSG_Pane *pane, 
										 MessageDB * msgDB,
										 int (*doneCB)(void *, int status), void *state) 
{
    DownloadArticlesToFolder::SaveMessages(array, fileName, pane, this, msgDB, doneCB, state);
    return eSUCCESS;
}


XP_Bool	MSG_FolderInfo::ShouldPerformOperationOffline()
{
#ifdef XP_UNIX
	return NET_IsOffline();
#else
	if (NET_IsOffline())
		return TRUE;
	if (GetType() == FOLDER_IMAPMAIL)
	{
		MSG_IMAPFolderInfoMail *imapFolder = (MSG_IMAPFolderInfoMail *) this;
		return imapFolder->GetRunningIMAPUrl();
	}
	return FALSE;
#endif
}

MSG_FolderInfoMail::MSG_FolderInfoMail(char* path, MSG_Master *master, uint8 depth, uint32 flags) :
    MSG_FolderInfo(NameFromPathname(path), depth, CompareFolders)
{
    // if we're a top level folder, we don't have unread messages
    if ( depth < 2 ) {
        m_numUnreadMessages = 0;
    }

	m_HaveReadNameFromDB = FALSE;
	m_expungedBytes = 0;
	m_pathName = path;
    m_flags = flags;
    m_parseMailboxState = NULL;
	m_master = master;
	m_gettingMail = FALSE;
}


const char *MSG_FolderInfoMail::GetPrettyName()
{
	if (m_depth == 1) {
		// Depth == 1 means we are on the mail server level
		// override the name here to say "Local Mail"
		if (GetType() == FOLDER_MAIL)
			return (XP_GetString(MK_MSG_LOCAL_MAIL));
		else return MSG_FolderInfo::GetPrettyName();
	}
	else return MSG_FolderInfo::GetPrettyName();
}

// this override pulls the value from the db
const char* MSG_FolderInfoMail::GetName()    // Name of this folder (as presented to user).
{
	if (!m_HaveReadNameFromDB)
	{
		if (m_depth == 1) 
		{
			SetName(XP_GetString(MK_MSG_LOCAL_MAIL));
			m_HaveReadNameFromDB = TRUE;
		}
		else
			ReadDBFolderInfo();
	}

#if defined(XP_WIN16) || defined(XP_OS2_HACK)
   // (OS/2) DSR082197 - Mike thinks this is OK, but wants me to flag it in case we run into NLV problems.
	// For Win16, try to make the filenames look presentable using the same
	// mechanism as the Explorer. These macros are not i18n aware, but neither
	// is the FAT filesystem, so no data can be lost here.
   //
	// For Win16, try to make the filenames look presentable, but only 
	// if we're using a single byte character set. It would be nice to have
	// an XP API for this, but apparently we don't.
	static XP_Bool guiCsidIsDoubleBye = (XP_Bool) ::GetSystemMetrics (SM_DBCSENABLED);
	if (m_name && !guiCsidIsDoubleBye)
	{
		int i = 0;
		while (m_name[i] != '\0')
		{
			if (i == 0)
				m_name[i] = XP_TO_UPPER(m_name[i]);
			else
				m_name[i] = XP_TO_LOWER(m_name[i]);
			i++;
		}
	}
#endif
	
	return m_name;
}

const char *MSG_FolderInfoMail::GenerateUniqueSubfolderName(const char *prefix, MSG_FolderInfo *otherFolder)
{
	/* only try 256 times */
	for (int count = 0; (count < 256); count++)
	{
		char *uniqueName = PR_smprintf("%s%d",prefix,count);
		if ((!ContainsChildNamed(uniqueName)) &&
			(otherFolder ? !(otherFolder->ContainsChildNamed(uniqueName)) : TRUE))
			return (uniqueName);
		else
			FREEIF(uniqueName);
	}
	return NULL;
}

XP_Bool MSG_FolderInfoMail::MessageWriteStreamIsReadyForWrite(msg_move_state * /*state*/)
{
    return TRUE;
}

// since we are writing to file, we don't care
// ahead of time how big the message is
// or what its imap flags are
int MSG_FolderInfoMail::CreateMessageWriteStream(msg_move_state *state, uint32 /*msgSize*/, uint32 /*flags*/)
{
    MsgERR returnError = 0;
    XP_StatStruct destFolderStat;

    if (!XP_Stat(this->GetPathname(), &destFolderStat, xpMailFolder))
        state->writestate.position = destFolderStat.st_size;
    
    state->writestate.fid = XP_FileOpen (this->GetPathname(), xpMailFolder, XP_FILE_APPEND_BIN);
    if (!state->writestate.fid)
        returnError = MK_MSG_ERROR_WRITING_MAIL_FOLDER;
    
    return returnError;
}


uint32 MSG_FolderInfoMail::MessageCopySize(DBMessageHdr *msgHeader, msg_move_state * /*state*/)
{
    return msgHeader->GetByteLength();
}

static void PostMessageCopyUrlExitCompleted(URL_Struct *URL_s, int status, MWContext *window_id)
{
    if ((window_id->msgCopyInfo->dstFolder->GetType() == FOLDER_IMAPMAIL) && !window_id->msgCopyInfo->dstIMAPfolderUpdated )
    {
    	MSG_IMAPFolderInfoMail *dstImapFolder = (MSG_IMAPFolderInfoMail *) window_id->msgCopyInfo->dstFolder;
        window_id->msgCopyInfo->dstIMAPfolderUpdated = TRUE;
        
		dstImapFolder->FinishUploadFromFile(window_id->msgCopyInfo, status);

        // if the destination folder is open in a thread pane then really run the update
        // otherwise we rely on tricking the FE until the user opens this folder.
		MSG_Pane *threadPane = window_id->imapURLPane->GetMaster()->FindThreadPaneNamed(dstImapFolder->GetPathname());
		
        if (threadPane)
        {
            MSG_UrlQueue *queue = MSG_UrlQueue::FindQueue(URL_s->address,window_id);
            dstImapFolder->StartUpdateOfNewlySelectedFolder(threadPane, FALSE /*pretend we're loading folder, was FALSE*/, queue, 
														NULL, FALSE, FALSE);
        }
    }
    MSG_MessageCopyIsCompleted (&window_id->msgCopyInfo); 
    if (URL_s->msg_pane->GetGoOnlineState())
    	OfflineOpExitFunction(  URL_s, status, window_id);


	if (MSG_Pane::PaneInMasterList(window_id->imapURLPane))
	{
		MSG_FolderPane *folderPane = (MSG_FolderPane *) window_id->imapURLPane->GetMaster()->FindFirstPaneOfType (MSG_FOLDERPANE);
		if (folderPane)
			folderPane->PostProcessRemoteCopyAction();
	}
	else
		XP_ASSERT(FALSE);	// where'd our pane go? Probably a progress pane, but still.
}

void PostMessageCopyUrlExitFunc (URL_Struct *URL_s, int status,
								 MWContext *window_id)
{
    XP_ASSERT(window_id->msgCopyInfo);
    
    if (window_id->msgCopyInfo)
	{
		if (window_id->msgCopyInfo->moveState.urlForNextKeyLoad)
		{
			XP_ASSERT(window_id->msgCopyInfo->moveState.sourcePane);
        
			URL_Struct *urlStruct = NET_CreateURLStruct(window_id->msgCopyInfo->moveState.urlForNextKeyLoad, NET_DONT_RELOAD);
			FREEIF (window_id->msgCopyInfo->moveState.urlForNextKeyLoad);
			window_id->msgCopyInfo->moveState.urlForNextKeyLoad = NULL;

			if ((window_id->msgCopyInfo->srcFolder->GetType() == FOLDER_IMAPMAIL) &&
        		(window_id->msgCopyInfo->moveState.sourcePane->GetPaneType() == MSG_MESSAGEPANE))
			{
        		XP_DELETE(urlStruct);	// don't need it, but we need to delete urlForNextKeyLoad before we start next url
				MSG_MessagePane *messagePane = (MSG_MessagePane *) window_id->msgCopyInfo->moveState.sourcePane;
				// let the message pane load the message.  It needs to reset it's own state for loading a new message
				// just blasting the message into it never called any MSG_MessagePane functions.
				messagePane->LoadMessage(window_id->msgCopyInfo->srcFolder,
										window_id->msgCopyInfo->moveState.nextKeyToLoad,
										PostMessageCopyUrlExitFunc,
										TRUE);
			}
			else if (window_id->msgCopyInfo->moveState.sourcePane->GetPaneType() == MSG_MESSAGEPANE)
			{
				// MSG_MessagePane::OpenMessageSock will get called by mailbox url
				MSG_Pane *savePane = window_id->msgCopyInfo->moveState.sourcePane;
			    MSG_MessageCopyIsCompleted (&window_id->msgCopyInfo); 
				MSG_UrlQueue::AddUrlToPane(urlStruct, NULL, savePane);
			}
			else
			{// probably a move from a thread pane, no message load, we are done
        		XP_DELETE(urlStruct);	// don't need it, but we need to delete urlForNextKeyLoad before we start next url
				PostMessageCopyUrlExitCompleted (URL_s, status, window_id);
			}
		}
		else
		{
			PostMessageCopyUrlExitCompleted (URL_s, status, window_id);
		}
	}
}

void MSG_FolderInfoMail::CloseOfflineDestinationDbAfterMove(msg_move_state *state)
{
    if (state->destDB)
    {
        state->destDB->Close();
        state->destDB = NULL;
        MailDB::SetFolderInfoValid(GetPathname(), 0, 0);
        SummaryChanged();
    }
}


MsgERR MSG_FolderInfoMail::CheckForLegalCopy(MSG_FolderInfo *dstFolder)
{
    XP_ASSERT(dstFolder);
    int err = 0;
    
    if (!dstFolder)
        err = MK_MSG_ERROR_WRITING_MAIL_FOLDER;
    else if ( (FOLDER_MAIL != dstFolder->GetType()) && (FOLDER_IMAPMAIL != dstFolder->GetType()) ) 
        err =  MK_MSG_ERROR_WRITING_MAIL_FOLDER; 
    else if (this == dstFolder)
        err = MK_MSG_CANT_COPY_TO_SAME_FOLDER;
    else if (dstFolder->GetFlags() & (MSG_FOLDER_FLAG_QUEUE)) {
		const char *q = MSG_GetQueueFolderName();
		if (q) {
			if (!strcasecomp(q,QUEUE_FOLDER_NAME_OLD))
				err = MK_MSG_CANT_COPY_TO_QUEUE_FOLDER_OLD;
			else
				err = MK_MSG_CANT_COPY_TO_QUEUE_FOLDER;
		}
		else
			err = MK_MSG_CANT_COPY_TO_QUEUE_FOLDER;
	}
    else if (dstFolder->GetFolderPrefFlags() & MSG_FOLDER_PREF_IMAPNOSELECT)
        err = MK_MSG_COPY_TARGET_NO_SELECT;

#ifdef DISABL_COPY_MSG_TO_DRAFTS
    else if (dstFolder->GetFlags() & (MSG_FOLDER_FLAG_DRAFTS))
        err = MK_MSG_CANT_COPY_TO_DRAFTS_FOLDER;
#endif

	return err;
}


MsgERR MSG_FolderInfoMail::BeginOfflineAppend(MSG_IMAPFolderInfoMail *dstFolder, 
												MessageDB *sourceDB,
												IDArray *srcArray, 
												int32 srcCount,
												msg_move_state *state)
{
	MsgERR stopit = 0;
	MailDB *sourceMailDB = sourceDB ? sourceDB->GetMailDB() : 0;
	if (sourceMailDB)
	{
		for (int sourceKeyIndex=0; !stopit && (sourceKeyIndex < srcCount); sourceKeyIndex++)
		{
			if (state->destDB)
			{
				MessageKey fakeKey = 1;
				fakeKey += state->destDB->ListHighwaterMark();
			
				MailMessageHdr *mailHdr = sourceMailDB->GetMailHdrForKey(srcArray->GetAt(sourceKeyIndex));
				if (mailHdr)
				{
					MailMessageHdr *newMailHdr = new MailMessageHdr;
					newMailHdr->CopyFromMsgHdr(mailHdr, sourceMailDB->GetDB(), state->destDB->GetDB());
					newMailHdr->SetMessageKey(fakeKey);
					
					uint32 onlineMsgSize = dstFolder->MessageCopySize(mailHdr, state);	// does its own seek
					
				    XP_FileSeek (state->infid, 	// seek to beginning of message
				                 mailHdr->GetMessageOffset(), 
				                 SEEK_SET);

					uint32 bytesWritten = 0;
					while (bytesWritten < onlineMsgSize)
						bytesWritten += dstFolder->WriteBlockToImapServer(mailHdr, state, newMailHdr, TRUE);
						
					newMailHdr->SetMessageSize(onlineMsgSize);
					newMailHdr->OrFlags(kOffline);
					
					stopit = state->destDB->AddHdrToDB(newMailHdr, NULL, TRUE);
					
					DBOfflineImapOperation	*fakeOp = state->destDB->GetOfflineOpForKey(fakeKey, TRUE);
					if (fakeOp)
					{
						fakeOp->SetSourceMailbox(GetPathname(), srcArray->GetAt(sourceKeyIndex));
						delete fakeOp;
					}
					else
						stopit = MK_MSG_ERROR_WRITING_MAIL_FOLDER;
					
					DBOfflineImapOperation	*op = sourceMailDB->GetOfflineOpForKey(srcArray->GetAt(sourceKeyIndex), TRUE);
					if (op)
					{
						const char *destinationPath = dstFolder->GetOnlineName();
							
						op->AddMessageCopyOperation(destinationPath);
						delete op;
					}
					else
						stopit = MK_MSG_ERROR_WRITING_MAIL_FOLDER;
					delete mailHdr;
				}
				else
					stopit = MK_MSG_ERROR_WRITING_MAIL_FOLDER;
			}		
			else
				stopit = MK_MSG_ERROR_WRITING_MAIL_FOLDER;
		}
		
		if (state->destDB)
		{
	        state->destDB->Commit();
	        sourceMailDB->Commit();
	    	dstFolder->SummaryChanged();
			FE_Progress(state->sourcePane->GetContext(), "");
    	}
	}
	else
		stopit = MK_MSG_ERROR_WRITING_MAIL_FOLDER;
	
	return stopit ? stopit : -1;
}



MsgERR MSG_FolderInfoMail::BeginCopyingMessages (MSG_FolderInfo *dstFolder, 
												 MessageDB *sourceDB,
                                                 IDArray *srcArray,
                                                 MSG_UrlQueue *urlQueue,
                                                 int32 srcCount,
                                                 MessageCopyInfo *copyInfo)
{
    MsgERR err = CheckForLegalCopy(dstFolder);

    if (err != 0)
        return err; 

    msg_move_state *state = &copyInfo->moveState;
    state->destfolder = dstFolder;
    state->infid = XP_FileOpen (GetPathname(), xpMailFolder, XP_FILE_READ_BIN);
    if (state->infid) 
    {
        state->midMessage = FALSE;
        state->messageBytesMovedSoFar = 0;
        
        state->size = 10240;
		while (!state->buf && (state->size > 1024))
		{
			state->buf = new char[state->size];
			if (!state->buf)
				state->size /= 2;
		}
        if (state->buf)
            err = 0;
        else
            err = MK_OUT_OF_MEMORY;
    }
    else
        err = MK_MSG_ERROR_WRITING_MAIL_FOLDER;
        
    
    if ((dstFolder->GetType() == FOLDER_IMAPMAIL) && NET_IsOffline())
    {
    	return BeginOfflineAppend((MSG_IMAPFolderInfoMail *)dstFolder, 
												sourceDB,
												srcArray, 
												srcCount,
												state);
    }

    if ((err == 0) && (dstFolder->GetType() == FOLDER_IMAPMAIL))
    {   
		state->sourcePane->GetContext()->msgCopyInfo->imapOnLineCopyState = kInProgress;
        // make sure that the URL we are firing does not start writing bytes to
        // an uninitialized socket
        state->uploadToIMAPsocket = NULL;

        MSG_FolderInfoMail *dstMailFolder = dstFolder->GetMailFolderInfo();
		DBMessageHdr *msgHdr = sourceDB->GetDBHdrForKey(srcArray->GetAt(0));
		// remember this up front so we can tell the imap connection about it when we get told about the connection.
		if (dstMailFolder && msgHdr)
		{
			state->msgSize = dstMailFolder->MessageCopySize(msgHdr, state);
			state->msg_flags = msgHdr->GetFlags();
			delete msgHdr;
		}

        // fire off the imap url to get the server ready for the message upload
        MSG_IMAPFolderInfoMail *imapDstFolder = (MSG_IMAPFolderInfoMail *) dstFolder;
        char *url_string = CreateImapOffToOnlineCopyUrl(imapDstFolder->GetHostName(),
                                                        imapDstFolder->GetOnlineName(),
                                                        imapDstFolder->GetOnlineHierarchySeparator());

        if (urlQueue)
            urlQueue->AddUrl(url_string, PostMessageCopyUrlExitFunc);
        else
        {
            URL_Struct *url_struct = NET_CreateURLStruct(url_string, NET_DONT_RELOAD);

            // after this url is done, this exit function will fire a url to update
            // the destination folder 

            if (url_struct)
            {
                url_struct->pre_exit_fn = PostMessageCopyUrlExitFunc;
                state->sourcePane->GetURL(url_struct, TRUE);
            }
        }
		FREEIF(url_string);
    }

    if (err == 0)
    {
        MSG_FolderInfoMail *dstMailFolder = dstFolder->GetMailFolderInfo();
		DBMessageHdr *msgHdr = sourceDB->GetDBHdrForKey(srcArray->GetAt(0));
		if (dstMailFolder && msgHdr)
		{
			err = dstMailFolder->CreateMessageWriteStream(state, dstMailFolder->MessageCopySize(msgHdr, state), msgHdr->GetFlags());
			delete msgHdr;
		}
		else
			err = MK_CONNECTED; /* (rb) eID_NOT_FOUND; The message has already been deleted, but the UI has not
								 cached up, the user pressed the delete key way too fast. Instead say we are done. */
    }

    return err;
}

uint32 MSG_FolderInfoMail::SeekToEndOfMessageStore(msg_move_state *state)
{
    uint32 newMsgPosition=0;
	// state->writestate.fid can be 0 if the destination file is read only
    if (state->writestate.fid && !XP_FileSeek (state->writestate.fid, 0, SEEK_END))
        newMsgPosition = ftell (state->writestate.fid);
    
    return newMsgPosition;
}

uint32   MSG_FolderInfoMail::WriteMessageBuffer(msg_move_state *state, char *buffer, uint32 bytesToWrite, MailMessageHdr * /*for offline append*/)
{
    XP_FileWrite (buffer, bytesToWrite, state->writestate.fid);
	if (ferror(state->writestate.fid))
		return MK_MSG_ERROR_WRITING_MAIL_FOLDER;
	return 0;
}

void   MSG_FolderInfoMail::CloseMessageWriteStream(msg_move_state *state)
{
    if (state->writestate.fid)
    {
        XP_FileClose (state->writestate.fid);
        state->writestate.fid = NULL;
    }
}


void   MSG_FolderInfoMail::SaveMessageHeader(msg_move_state *state, MailMessageHdr *hdr, uint32 messageKey, MessageDB * srcDB)
{
    if (state->destDB != NULL)
    {
        MailMessageHdr *newHdr = new MailMessageHdr();
        if (newHdr)
        {
            MsgERR  msgErr;
            MessageDBView *view = state->sourcePane->GetMsgView();

			// hack for the search pane case!!! hopefully I won't have to do this too often...
			if (state->sourcePane->GetPaneType() == MSG_SEARCHPANE)
			{
				if (srcDB == NULL)
					return;    // nothing we can do here..
				newHdr->CopyFromMsgHdr (hdr, srcDB->GetDB(), state->destDB->GetDB());

			}
			else 
				newHdr->CopyFromMsgHdr (hdr, view->GetDB()->GetDB(), state->destDB->GetDB());

            // set new byte offset, since the offset in the old file is certainly wrong
            newHdr->SetMessageKey (messageKey); 
            // PHP impedance mismatch on error types
            msgErr = state->destDB->AddHdrToDB (newHdr, NULL, TRUE);    // tell db to notify
            
            // if this was an IMAP download, then we must tweak the message size to account
            // for the envelope and line termination differences
            if (state->finalDownLoadMessageSize)
            {
                newHdr->SetByteLength(state->finalDownLoadMessageSize);
                newHdr->SetMessageSize(state->finalDownLoadMessageSize);
            }
            
            // update the folder info
//          mailDb->m_dbFolderInfo->m_folderSize += hdr->GetByteLength();
//          mailDb->m_dbFolderInfo->setDirty();
        }
    }   
}



int MSG_FolderInfoMail::FinishCopyingMessages (MWContext *currentContext,
                                       MSG_FolderInfo *srcFolder, 
                                       MSG_FolderInfo *dstFolder, 
									   MessageDB *sourceDB,
                                       IDArray *srcArray, 
                                       int32 srcCount,
                                       msg_move_state *state)
{
    int err = 0;
	// int32 diskSpace = 0;
	uint32 messageLength = 0;
    
    if (!state->infid)
        return MK_MSG_ERROR_WRITING_MAIL_FOLDER;
    
    if (!dstFolder->TestSemaphore(this))
    {
    	err = dstFolder->AcquireSemaphore(this);
    	if (err)
    	{
	        FE_Alert (currentContext, XP_GetString(err));
	        return MK_MSG_FOLDER_BUSY;
    	}
    }

	MSG_FolderInfoMail *dstMailFolder = (MSG_FolderInfoMail *) dstFolder;
	//diskSpace = FE_DiskSpaceAvailable(currentContext, dstMailFolder->GetPathname()); // too slow probably

    // seek any relevant files to the correct position
    
    MailMessageHdr *mailHdr = (MailMessageHdr *) sourceDB->GetDBHdrForKey(srcArray->GetAt(state->msgIndex));
	if (mailHdr)
		messageLength = mailHdr->GetByteLength();
	/* When checking for disk space available, take in consideration possible database
		changes, therefore ask for a little more than what the message size is.
	Also, due to disk sector sizes, allocation blocks, etc. The space "available" may be greater
	than the actual space usable. */
	//if((diskSpace +3096) <= messageLength)
	//	return(MK_POP3_OUT_OF_DISK_SPACE);
    
	if (!mailHdr)
		return(MK_POP3_OUT_OF_DISK_SPACE);

    XP_FileSeek (state->infid, 
                 mailHdr->GetMessageOffset() + state->messageBytesMovedSoFar, 
                 SEEK_SET);
                 
    uint32 storePosition = dstMailFolder->SeekToEndOfMessageStore(state);
    UndoManager *undoManager = NULL;

    undoManager = state->sourcePane->GetUndoManager();

    
    // write header info if this starts a new message in an offline folder
    if (!state->midMessage)
    {
		const char	*statusMsgTemplate;
		char		*statusMsg;

		statusMsgTemplate = XP_GetString(state->ismove ? MK_MSG_MOVING_MSGS_STATUS : MK_MSG_COPYING_MSGS_STATUS);
		if (statusMsgTemplate)
		{
			statusMsg = PR_smprintf(statusMsgTemplate, state->msgIndex + 1, srcCount, dstMailFolder->GetName());
			FE_Progress (currentContext, statusMsg);
			int32 percent = (int32) ((100l * (int32)(state->msgIndex + 1)) / (int32)(srcCount));
			FE_SetProgressBarPercent(currentContext, percent);
			FREEIF(statusMsg);
		}
		if 	(dstMailFolder->GetType() == FOLDER_MAIL)
		{
			dstMailFolder->SaveMessageHeader(state, mailHdr, storePosition, sourceDB);  // sourceDB added by mscott
			// Undo stuff... Who knows what am I doing here
			if (undoManager && undoManager->GetState() == UndoIdle)
				state->sourcePane->GetUndoManager()->AddMsgKey(storePosition);
			// end Undo stuff
		}
    }
    
	HG87963
    // copy 2k or up to 10k of data or one message
    ImapOnlineCopyState imapAppendState = state->sourcePane->GetContext()->msgCopyInfo->imapOnLineCopyState;

    if (dstMailFolder->MessageWriteStreamIsReadyForWrite(state))
    {
        uint32 bytesLeft = messageLength - state->messageBytesMovedSoFar;

        uint32 nRead=0;
        if (dstFolder->GetType() == FOLDER_IMAPMAIL)
        {
        	if ((imapAppendState == kReadyForAppendData) && (bytesLeft > 0))
        	{
            	((MSG_IMAPFolderInfoMail *) dstFolder)->WriteBlockToImapServer(mailHdr, state, NULL, TRUE);	// write it now.
            	
            	// close the imap stream so we can determine success/fail from server
            	if (state->messageBytesMovedSoFar >= messageLength)
            		dstMailFolder->CloseMessageWriteStream(state);
            }
        }
        else
        {
            // do a block read, this is not line based IO
            nRead = XP_FileRead (state->buf, bytesLeft > state->size ? state->size : bytesLeft, state->infid);
			err = dstMailFolder->WriteMessageBuffer(state, state->buf, nRead, NULL);
			if (err == 0)
				state->messageBytesMovedSoFar += nRead;
			else
			{
				// wrote a partial message and somehow we are out of space, back out our changes
				// by removing the message and header
				XP_FileClose (state->writestate.fid);	// close before truncating
				state->writestate.fid = NULL;
				if (mailHdr)
				{
					err = XP_FileTruncate (dstMailFolder->GetPathname(), xpMailFolder, storePosition);
					sourceDB->DeleteHeader(mailHdr, NULL, FALSE);	
					delete mailHdr;
				}
				if (state->buf)
				{
					delete [] state->buf;
					state->buf = NULL;
				}
				return MK_POP3_OUT_OF_DISK_SPACE;
			}
        }
        
    }
    
    XP_Bool failedImapAppend = imapAppendState == kFailedAppend;
    if (failedImapAppend)
    	state->msgIndex = srcCount;	// do not process any more appends.
    
    if ((state->messageBytesMovedSoFar < messageLength) || (imapAppendState == kReadyForAppendData))
        state->midMessage = TRUE;
    else
    {
        state->midMessage = FALSE;
        state->messageBytesMovedSoFar = 0;
        state->msgIndex++;
        if ((dstFolder->GetType() == FOLDER_IMAPMAIL) && (state->msgIndex < srcCount) )
        {
            // create the next IMAP message stream.  It will upload the size
            MSG_FolderInfoMail *dstMailFolder = (MSG_FolderInfoMail *) dstFolder;
			DBMessageHdr *msgHdr = sourceDB->GetDBHdrForKey(srcArray->GetAt(state->msgIndex));
            err = dstMailFolder->CreateMessageWriteStream(
                                state, 
                                dstMailFolder->MessageCopySize(msgHdr, state), msgHdr->GetFlags());
			delete msgHdr;
        }
    }
        
    // if this is the end of the copy, close the source and destination
    if (state->msgIndex == srcCount)
    {
        MSG_ViewIndex doomedIndex = MSG_VIEWINDEXNONE;
        
        if (dstMailFolder->GetType() == FOLDER_MAIL)
        {
        	dstMailFolder->CloseMessageWriteStream(state);
            dstMailFolder->CloseOfflineDestinationDbAfterMove(state);
        }
        else   // let the imap libnet module know that we are done with appends
        {
			if (!failedImapAppend) 
				UpdatePendingCounts(dstMailFolder, srcArray);
            IMAP_UploadAppendMessageSize(state->imap_connection, 0,0); 
        }



        // *** Undo hook up - local folder only for now
        if (!failedImapAppend && undoManager && (undoManager->GetState() == UndoIdle) &&
			(dstMailFolder->GetType() == FOLDER_MAIL))
        {
            MoveCopyMessagesUndoAction *undoAction =
                new MoveCopyMessagesUndoAction(srcFolder,
                                               dstFolder,
                                               state->ismove,
                                               state->sourcePane,
                                               srcArray->GetAt(0), 
                                               state->nextKeyToLoad);

            if (undoAction)
            {
				uint i;
                for (i=0; i < srcCount; i++)
                    undoAction->AddDstKey(undoManager->GetAndRemoveMsgKey());
                for (i=0; i < srcCount; i++)
                    undoAction->AddSrcKey(srcArray->GetAt(i));
                undoManager->AddUndoAction(undoAction);
            }
        }
        // *** End Undo Hook up

		MessageDBView * view = NULL;
		// a search pane does not have a view, we need to work around this fact...
		if (state->sourcePane && state->sourcePane->GetPaneType() == MSG_SEARCHPANE) // make sure we can cast..
			view = ((MSG_SearchPane *) state->sourcePane)->GetView(srcFolder);
		else
			view = state->sourcePane ? state->sourcePane->GetMsgView() : 0;

        // if this was a move delete the source messages
        int32 i;
        if (state->ismove && !failedImapAppend)
        {
            // Run the delete loop forwards 
            // so the indices will be right for DeleteMessagesByIndex
            if (state->sourcePane)
            {
				IDArray	viewIndices;	
                for (i = 0; i < srcCount; i++)
                {
                    // should we stop on error?
                    state->msgIndex--;
                    if (view)
                    {
                        MessageKey key = srcArray->GetAt(i);

                        // Tell the FE we're deleting this message in case they want to close
                        // any open message windows.
                        if (MSG_THREADPANE == state->sourcePane->GetPaneType())
                            FE_PaneChanged (state->sourcePane, TRUE, MSG_PaneNotifyMessageDeleted, (uint32) key);

                        doomedIndex = view->FindViewIndex(key);
						viewIndices.Add(doomedIndex);
                    }
                }
				if (view)
				{
					//const char	*statusMsgTemplate;
					//char		*statusMsg;

					//statusMsgTemplate = XP_GetString(MK_MSG_DELETING_MSGS_STATUS);
					//if (statusMsgTemplate)
					//{
					//	statusMsg = PR_smprintf(statusMsgTemplate, state->msgIndex, srcCount);
					//	FE_Progress (currentContext, statusMsg);
					//	FREEIF(statusMsg);
					//}

					state->sourcePane->StartingUpdate(MSG_NotifyNone, 0, 0);

					if (srcFolder->GetType() != FOLDER_IMAPMAIL)
						DeleteMsgsOnServer(state, sourceDB, srcArray, srcCount);
                    err = view->DeleteMessagesByIndex (viewIndices.GetData(), viewIndices.GetSize(), TRUE /* delete from db */);
					
					state->sourcePane->EndingUpdate(MSG_NotifyNone, 0, 0);
				}
            }
            srcFolder->SummaryChanged();
        }

		if (state->infid)
        {
            XP_FileClose (state->infid);
            state->infid = NULL;
        }
		if (state->buf)
		{
			delete [] state->buf;
			state->buf = NULL;
		}
        delete srcArray;

		if (view)
			SetUrlForNextMessageLoad(state,view,doomedIndex); // should we do this if we are a search pane? 

		// search as view modification..inform search pane that we are done with this view
//		if (state->sourcePane && state->sourcePane->GetPaneType() == MSG_SEARCHPANE)
//			((MSG_SearchPane *) state->sourcePane)->CloseView(srcFolder);

        err = MK_CONNECTED;
    }
    delete mailHdr;

    return err;
}


// A message is being deleted from a POP3 mail file, so check and see if we have the message
// being deleted in the server. If so, then we need to remove the message from the server as well.
// We have saved the UIDL of the message in the popstate.dat file and we must match this uidl, so
// read the message headers and see if we have it, then mark the message for deletion from the server.
// The next time we look at mail the message will be deleted from the server.

void MSG_FolderInfoMail::DeleteMsgsOnServer(msg_move_state *state, MessageDB *sourceDB, IDArray *srcArray, int32 srcCount)
{
	char		*uidl;
	char		*header = NULL;
	uint32		offset = 0, err = 0, size = 0, len = 0, i = 0;
	MailMessageHdr *hdr = NULL;
	MessageKey key = 0;
	XP_Bool leave_on_server = FALSE;
	XP_Bool changed = FALSE;
	char *popData = NULL;
	
	PREF_GetBoolPref("mail.leave_on_server", &leave_on_server);
	offset = XP_FileTell(state->infid);
	header = (char*) XP_ALLOC(512);
	for (i = 0; header && (i < srcCount); i++)
	{
		/* get uidl for this message */
		uidl = NULL;
		hdr = ((MailDB*) sourceDB)->GetMailHdrForKey(srcArray->GetAt(i));
		if (hdr && ((hdr->GetFlags() & kPartial) || leave_on_server))
		{
			len = 0;
			err = XP_FileSeek(state->infid, hdr->GetMessageOffset(), SEEK_SET); /* GetMessageKey */
			len = hdr->GetMessageSize();
			while ((len > 0) && !uidl)
			{
				size = len;
				if (size > 512)
					size = 512;
				if (XP_FileReadLine(header, size, state->infid))
				{
					size = strlen(header);
					if (!size)
						len = 0;
					else {
						len -= size;
						uidl = XP_STRSTR(header, X_UIDL);
					}
				} else
					len = 0;
			}
			if (uidl)
			{
				if (!popData)
					popData = ReadPopData((char*) MSG_GetPopHost(GetMaster()->GetPrefs()));
				uidl += X_UIDL_LEN + 2; // skip UIDL: header
				len = strlen(uidl);
				if (len >= 2)
					uidl[len - 2] = 0;	// get rid of return-newline at end of string
				net_pop3_delete_if_in_server(popData, uidl, &changed);
			}
		}
		delete hdr;
	}
	FREEIF(header);
	if (popData)
	{
		if (changed)
			SavePopData(popData);
		KillPopData(popData);
		popData = NULL;
	}
	err = XP_FileSeek(state->infid, offset, SEEK_SET);
}


// This algorithm loads the next message url into the move state
// This function is shared by local and imap copies
void MSG_FolderInfoMail::SetUrlForNextMessageLoad(msg_move_state *state, MessageDBView *view, MSG_ViewIndex doomedIndex)
{
    //if (state->nextKeyToLoad!=MSG_MESSAGEKEYNONE)
    {
        // in general, the next key to load is the message currently at the
        // position of the last key deleted, i.e., doomedIndex.
        // If that's not valid, use state->nextKeyToLoad.
        MessageKey nextKeyToLoad = view->GetAt(doomedIndex);
		if (nextKeyToLoad == MSG_MESSAGEKEYNONE)
			nextKeyToLoad = state->nextKeyToLoad;
        else
        	state->nextKeyToLoad = nextKeyToLoad;	// used in PostMessageCopyUrlExitFunc

        if (nextKeyToLoad != MSG_MESSAGEKEYNONE)
            state->urlForNextKeyLoad = BuildUrl(view->GetDB(), nextKeyToLoad);
        else if (state->ismove)
            FE_PaneChanged(state->sourcePane, TRUE, MSG_PaneNotifyLastMessageDeleted, 0);
    }
}


    // returns a pointer, within the passed buffer, to the 
    // next CR, LF, or '\0'
char *MSG_FolderInfoMail::FindNextLineTerminatingCharacter(char *bufferToSearch)
{
    char *returnValue = bufferToSearch;
    while ( *returnValue && (*returnValue != CR) && (*returnValue != LF) )
        returnValue++;
    
    return returnValue;
}

    
    // This function ensures that each line in the buffer ends in
    // CRLF, as required by RFC822 and therefore IMAP APPEND.
    // 
    // This function assumes that there is enough room in the buffer to 
    // double its size.  The return value is the new buffer size.
uint32 MSG_FolderInfoMail::ConvertBufferToRFC822LineTermination(char *buffer, uint32 bufferSize, XP_Bool *lastCharacterOfPrevBufferWasCR)
{
    // check for a <CR><LF> stradling buffer boundaries
    if (*lastCharacterOfPrevBufferWasCR && (*buffer == LF))
    {
        // we added a LF last time, shifting the buffer seems inefficient but this condition is very rare
        XP_MEMMOVE(buffer,      // destination
                   buffer+1,    // source
                   --bufferSize);
    }
    
    *lastCharacterOfPrevBufferWasCR = *(buffer + bufferSize - 1) == CR;
    
    
    
    // adjust the rest of the buffer
    uint32 adjustedBufferSize = bufferSize;
    *(buffer + bufferSize) = '\0';  // write a '\0' at the end so FindNextLineTerminatingCharacter will stop
    
    char *currentLineTerminator = FindNextLineTerminatingCharacter(buffer);
    
    if (*currentLineTerminator)
    {
        do {
            if (XP_STRNCMP(currentLineTerminator, CRLF, 2))
            {
                XP_MEMMOVE(currentLineTerminator+2,     // destination
                           currentLineTerminator+1,     // source
                           adjustedBufferSize - (currentLineTerminator - buffer));
                           
                *currentLineTerminator++ = CR;
                *currentLineTerminator++ = LF;
                adjustedBufferSize++;
            }
			else
				currentLineTerminator += 2;
            currentLineTerminator = FindNextLineTerminatingCharacter(currentLineTerminator);
        } while (*currentLineTerminator);
    }
    
    return adjustedBufferSize;
}


char *MSG_FolderInfoMail::CreatePlatformLeafNameForDisk(const char *userLeafName, MSG_Master *master, MSG_FolderInfoMail *parent)
{
	char *leafName = NULL;
	char *baseDir = NULL;
	
	if (parent->m_depth > 1)
	{
		baseDir = (char *) XP_ALLOC(XP_STRLEN(parent->GetPathname()) + XP_STRLEN(".sbd") + 1);
		if (baseDir)
		{
			XP_STRCPY(baseDir, parent->GetPathname());
			XP_STRCAT(baseDir, ".sbd");
		}
	}
	else
		baseDir = XP_STRDUP(parent->GetPathname());
	
	leafName = CreatePlatformLeafNameForDisk(userLeafName, master, baseDir);
	
	FREEIF(baseDir);
	
	return leafName;
}




char *MSG_FolderInfoMail::CreatePlatformLeafNameForDisk(const char *userLeafName, MSG_Master *, const char *baseDir)
{
	const int charLimit = MAX_FILE_LENGTH_WITHOUT_EXTENSION;	// set on platform specific basis
#if XP_MAC
	const char *illegalChars = ":";
#elif defined(XP_WIN16) || defined(XP_OS2) 
	const char *illegalChars = "\"/\\[]:;=,|?<>*$. ";
#elif defined(XP_WIN32)
	const char *illegalChars = "\"/\\[]:;=,|?<>*$";
#else		// UNIX
	const char *illegalChars = "";
#endif

	// if we weren't passed in anything, return NULL
	if (!userLeafName || !baseDir)
		return NULL;

	// mangledLeaf is the new leaf name.
	// If userLeafName	(a) contains all legal characters
	//					(b) is within the valid length for the given platform
	//					(c) does not already exist on the disk
	// then we simply return XP_STRDUP(userLeafName)
	// Otherwise we mangle it
	char *mangledLeaf = NULL;

	// leafLength is the length of mangledLeaf which we will return
	// if userLeafName is greater than the maximum allowed for this
	// platform, then we truncate and mangle it.  Otherwise leave it alone.
	int32 leafLength = MIN(XP_STRLEN(userLeafName),charLimit);

	// Create the path for the summary file, to see if it already exists
	// on the disk
	int dirNameLength = XP_STRLEN(baseDir);
	
	// mangledPath is the entire path to the newly mangled leaf name
	int allocatedSizeForMangledPath = dirNameLength + leafLength + 2;
	char *mangledPath = (char *) XP_ALLOC(allocatedSizeForMangledPath);	// 2 == length of '/' and '\0'
	if (!mangledPath)
		return NULL;
	
	// copy the given leaf name into the mangled path for now, truncating it
	// at the maximum length.
	// enough for 676 names with matching prefix
	XP_STRCPY(mangledPath, baseDir);
	XP_STRCAT(mangledPath, "/");				// 3rd argument is amount of space left available for cat
	XP_STRNCAT_SAFE(mangledPath, userLeafName, allocatedSizeForMangledPath - XP_STRLEN(mangledPath));
	// mangledPath[XP_STRLEN(mangledPath)] = '\0';

	// pathLeaf points to the location of the leaf name in mangledPath
	char *pathLeaf = mangledPath + dirNameLength + 1;

	// after this while, if (*illegalCharacterPtr == 0), there are no illegal characters
	const char *illegalCharacterPtr = pathLeaf;
	while (*illegalCharacterPtr && !XP_STRCHR(illegalChars, *illegalCharacterPtr) && (*illegalCharacterPtr >= 31))
		illegalCharacterPtr++;


	XP_StatStruct possibleNameStat;
	if (!*illegalCharacterPtr && (0 != XP_Stat(mangledPath, &possibleNameStat, xpMailFolderSummary))) {
		// if there are no illegal characters
		// and the file doesn't already exist, then don't do anything to the string
		// Note that this might be truncated to charLength, but if so, the file still
		// does not exist, so we are OK.
		mangledLeaf = XP_STRDUP(pathLeaf);
		XP_FREE(mangledPath);
		return mangledLeaf;
	}


	// if we are here, then any of the following may apply:
	//		(a) there were illegal characters
	//		(b) the file already existed

	// First, replace all illegal characters with '_'
	for (char *possibleillegal = pathLeaf; *possibleillegal; possibleillegal++)
		if (XP_STRCHR(illegalChars, *possibleillegal) || (*possibleillegal < 31) )
			*possibleillegal = '_';


	// Now, we have to loop until we find a filename that doesn't already
	// exist on the disk
	XP_Bool nameSpaceExhausted = FALSE;

	if (0 == XP_Stat(mangledPath, &possibleNameStat, xpMailFolderSummary)) {
		if (leafLength >= 2) pathLeaf[leafLength - 2] = 'A';
		pathLeaf[leafLength - 1] = 'A';	// leafLength must be at least 1
		// pathLeaf[leafLength] = 0;
	}


	while (!nameSpaceExhausted && (0 == XP_Stat(mangledPath, &possibleNameStat, xpMailFolderSummary)))
	{

		if (leafLength >= 2)
		{
			if (++(pathLeaf[leafLength - 1]) > 'Z')
			{
				pathLeaf[leafLength - 1] = 'A';
				nameSpaceExhausted = (pathLeaf[leafLength - 2])++ == 'Z';
			}
		}
		else
		{
			nameSpaceExhausted = (++(pathLeaf[leafLength - 1]) == 'Z');
		}
	}

	mangledLeaf = XP_STRDUP(pathLeaf);
	
	XP_FREE(mangledPath);
	return mangledLeaf;
}

MsgERR MSG_FolderInfoMail::CreateSubfolder (const char *leafNameFromUser, MSG_FolderInfo **ppOutFolder, int32 *pOutPos)
{
    MsgERR status = 0;
    *ppOutFolder = NULL;
    *pOutPos = 0;
    XP_StatStruct stat;
    
    
    // Only create a .sbd pathname if we're not in the root folder. The root folder
    // e.g. c:\netscape\mail has to behave differently than subfolders.
    if (m_depth > 1)
    {
        // Look around in our directory to get a subdirectory, creating it 
        // if necessary
        XP_BZERO (&stat, sizeof(stat));
        if (0 == XP_Stat (m_pathName, &stat, xpMailSubdirectory))
        {
            if (!S_ISDIR(stat.st_mode))
                status = MK_COULD_NOT_CREATE_DIRECTORY; // a file .sbd already exists
        }
        else {
            status = XP_MakeDirectory (m_pathName, xpMailSubdirectory);
			if (status == -1)
				status = MK_COULD_NOT_CREATE_DIRECTORY;
		}
    }
    
	char *leafNameForDisk = CreatePlatformLeafNameForDisk(leafNameFromUser,m_master, this);
	if (!leafNameForDisk)
		status = MK_OUT_OF_MEMORY;

    if (0 == status) //ok so far
    {
        // Now that we have a suitable parent directory created/identified, 
        // we can create the new mail folder inside the parent dir. Again,

        char *newFolderPath = (char*) XP_ALLOC(XP_STRLEN(m_pathName) + XP_STRLEN(leafNameForDisk) + XP_STRLEN(".sbd/") + 1);
        if (newFolderPath)
        {
            XP_STRCPY (newFolderPath, m_pathName);
            if (m_depth == 1)
                XP_STRCAT (newFolderPath, "/");
            else
                XP_STRCAT (newFolderPath, ".sbd/");
            XP_STRCAT (newFolderPath, leafNameForDisk);

	        if (0 != XP_Stat (newFolderPath, &stat, xpMailFolder))
			{
				XP_File file = XP_FileOpen(newFolderPath, xpMailFolder, XP_FILE_WRITE_BIN);
				if (file)
				{
 					// Create an empty database for this mail folder, set its name from the user  
  					MailDB *unusedDb = NULL;
 					MailDB::Open(newFolderPath, TRUE, &unusedDb, TRUE);
  					if (unusedDb)
  					{
 						unusedDb->m_dbFolderInfo->SetMailboxName(leafNameFromUser);
 					
						MSG_FolderInfoMail *newFolder = BuildFolderTree (newFolderPath, m_depth + 1, m_subFolders, m_master);
 						if (newFolder)
 						{
 							// so we don't show ??? in totals
 							newFolder->SummaryChanged();
 							*ppOutFolder = newFolder;
 							*pOutPos = m_subFolders->FindIndex (0, newFolder);
 						}
 						else
 							status = MK_OUT_OF_MEMORY;
 						unusedDb->SetFolderInfoValid(newFolderPath,0,0);
  						unusedDb->Close();
  					}
					else
					{
						XP_FileClose(file);
						file = NULL;
						XP_FileRemove (newFolderPath, xpMailFolder);
						status = MK_MSG_CANT_CREATE_FOLDER;
					}
					if (file)
					{
						XP_FileClose(file);
						file = NULL;
					}
				}
				else
					status = MK_MSG_CANT_CREATE_FOLDER;
			}
			else
				status = MK_MSG_FOLDER_ALREADY_EXISTS;
			FREEIF(newFolderPath);
        }
        else
            status = MK_OUT_OF_MEMORY;
    }
    FREEIF(leafNameForDisk);
    return status;
}


void MSG_FolderInfoMail::RemoveSubFolder (MSG_FolderInfo *which)
{
    // Let the base class do list management
    MSG_FolderInfo::RemoveSubFolder (which);

    // Derived class is responsible for managing the subdirectory
    if (0 == m_subFolders->GetSize())
        XP_RemoveDirectory (m_pathName, xpMailSubdirectory);
}

void MSG_FolderInfoMail::SetGettingMail(XP_Bool flag)
{
	m_gettingMail = flag;
}

XP_Bool MSG_FolderInfoMail::GetGettingMail()
{
	return m_gettingMail;
}


MsgERR MSG_FolderInfoMail::Delete ()
{
    MessageDB   *db;
    // remove the summary file
    MsgERR status = CloseDatabase (m_pathName, &db);
    if (0 == status)
    {
        if (db != NULL)
            db->Close();    // decrement ref count, so it will leave cache
        XP_FileRemove (m_pathName, xpMailFolderSummary);
    }

    if ((0 == status) && (GetType() == FOLDER_MAIL))
	{
        // remove the mail folder file
        status = XP_FileRemove (m_pathName, xpMailFolder);

		// if the delete seems to have failed, but the file doesn't
		// exist, that's not really an error condition, is it now?
		if (status)
		{
			XP_StatStruct fileStat;
	        if (0 == XP_Stat(m_pathName, &fileStat, xpMailFolder))
				status = 0;
		}
	}


    if (0 != status)
        status = MK_UNABLE_TO_DELETE_FILE;
    return status;  
}


MsgERR MSG_FolderInfo::PropagateDelete (MSG_FolderInfo **ppFolder, XP_Bool deleteStorage)
{
	MsgERR status = 0;

	// first, find the folder we're looking to delete
	for (int i = 0; i < m_subFolders->GetSize() && *ppFolder; i++)
	{
		MSG_FolderInfo *child = m_subFolders->GetAt(i);
		if (*ppFolder == child)
		{
			// maybe delete disk storage for it, and its subfolders
			status = child->RecursiveDelete(deleteStorage);	

			if (status == 0) 
			{
				// Send out a broadcast message that this folder is going away.
				// Many important things happen on this broadcast.
				m_master->BroadcastFolderDeleted (child);

				RemoveSubFolder(child);
				delete child;
				*ppFolder = NULL;	// stop looking
			}
		}
		else if ((*ppFolder)->GetDepth() > child->GetDepth())
		{ 
			status = child->PropagateDelete (ppFolder, deleteStorage);
		}
	}
	
	return status;
}


MsgERR MSG_FolderInfo::RecursiveDelete (XP_Bool deleteStorage)
{
	// If deleteStorage is TRUE, recursively deletes disk storage for this folder
	// and all its subfolders.
	// Regardless of deleteStorage, always unlinks them from the children lists and
	// frees memory for the subfolders but NOT for _this_

	MsgERR status = 0;
	
	for (int k = 0; k < m_subFolders->GetSize() && !status; k++)
	{
		MSG_FolderInfo *child = m_subFolders->GetAt(k);
		status = child->RecursiveDelete(deleteStorage);  // recur
		RemoveSubFolder(child);  // unlink it from this's child list
		delete child;  // free memory
	}

	// now delete the disk storage for _this_
	if (deleteStorage && (status == 0))
			status = Delete();

	return status;

}


MsgERR MSG_FolderInfoMail::CloseDatabase (const char *path, MessageDB **outDb)
{
    MsgERR err = eDBNotOpen;
    
    char* dbName = WH_FileName(path, xpMailFolderSummary);
    if (!dbName) return eOUT_OF_MEMORY;
    MessageDB *db =  MessageDB::FindInCache(dbName);
    XP_FREE(dbName);
    if (db)
        err = db->CloseDB();

    if (outDb)
        *outDb = db;

    // This is not really an error condition when trying to close the DB
    if (eDBNotOpen == err)
        err = 0;

    return err;
}


MsgERR MSG_FolderInfoMail::ReopenDatabase (MessageDB *db, const char *newPath)
{
    char* dbName = WH_FileName (newPath, xpMailFolderSummary);
    if (!dbName) return eOUT_OF_MEMORY;
    MsgERR err = db->OpenDB (dbName, FALSE /*create?*/);
    if (eSUCCESS == err)
        err = db->OnNewPath (newPath);
    XP_FREE(dbName);
    return err;
}


XP_Bool MSG_FolderInfoMail::CanBeRenamed ()
{
    // The root mail folder can't be renamed
    if (m_depth < 2)
        return FALSE;

    // Here's a weird case necessitated because we don't have a separate
    // preference for any folder name except the FCC folder (Sent). Others
    // are known by name, and as such, can't be renamed. I guess.
    if (m_flags & MSG_FOLDER_FLAG_TRASH ||
        m_flags & MSG_FOLDER_FLAG_DRAFTS ||
        m_flags & MSG_FOLDER_FLAG_QUEUE ||
        m_flags & MSG_FOLDER_FLAG_INBOX ||
		m_flags & MSG_FOLDER_FLAG_TEMPLATES)
        return FALSE;

    return TRUE;
}


MsgERR MSG_FolderInfoMail::Rename (const char * newUserLeafName)
{
    // change the leaf name (stored separately)
    MsgERR status = MSG_FolderInfo::Rename (newUserLeafName);
    if (status == 0)
    {
    	char *baseDir = XP_STRDUP(m_pathName);
    	if (baseDir)
    	{
	        char *base_slash = XP_STRRCHR (baseDir, '/'); 
	        if (base_slash)
	            *base_slash = '\0';
        }

    	char *leafNameForDisk = CreatePlatformLeafNameForDisk(newUserLeafName,m_master, baseDir);
    	if (!leafNameForDisk)
    		status = MK_OUT_OF_MEMORY;

        if (0 == status)
		{
		        // calculate the new path name
	        char *newPath = (char*) XP_ALLOC(XP_STRLEN(m_pathName) + XP_STRLEN(leafNameForDisk) + 1);
	        XP_STRCPY (newPath, m_pathName);
	        char *slash = XP_STRRCHR (newPath, '/'); 
	        if (slash)
	            XP_STRCPY (slash + 1, leafNameForDisk);

	        // rename the mail summary file, if there is one
	        MessageDB *db = NULL;
	        status = CloseDatabase (m_pathName, &db);
	        
			XP_StatStruct fileStat;
	        if (!XP_Stat(m_pathName, &fileStat, xpMailFolderSummary))
				status = XP_FileRename(m_pathName, xpMailFolderSummary, newPath, xpMailFolderSummary);
			if (0 == status)
			{
				if (db)
				{
					if (ReopenDatabase (db, newPath) == 0)
	                	db->m_dbFolderInfo->SetMailboxName(newUserLeafName);
				}
				else
				{
					MailDB *mailDb = NULL;
					MailDB::Open(newPath, TRUE, &mailDb, TRUE);
					if (mailDb)
					{
						mailDb->m_dbFolderInfo->SetMailboxName(newUserLeafName);
						mailDb->Close();
					}
				}
			}

			// rename the mail folder file, if its local
			if ((status == 0) && (GetType() == FOLDER_MAIL))
				status = XP_FileRename (m_pathName, xpMailFolder, newPath, xpMailFolder);

			if (status == 0)
			{
				// rename the subdirectory if there is one
				if (m_subFolders->GetSize() > 0)
				status = XP_FileRename (m_pathName, xpMailSubdirectory, newPath, xpMailSubdirectory);

				// tell all our children about the new pathname
				if (status == 0)
				{
					int startingAt = XP_STRLEN (newPath) - XP_STRLEN (leafNameForDisk) + 1; // add one for trailing '/'
					status = PropagateRename (leafNameForDisk, startingAt);
				}
			}
		}
    	FREEIF(baseDir);
    }
    return status;
}


MsgERR MSG_FolderInfoMail::PropagateRename (const char *newName, int startingAt)
{
	XP_ASSERT(newName);
	XP_ASSERT(m_pathName);

	// change our own pathname in memory to reflect the new name
	MsgERR err = 0;
	char *walkPath = &m_pathName[startingAt];
	char *diffEnd = XP_STRCHR (walkPath, '/');
	int diffLen = diffEnd ? (XP_STRLEN(diffEnd) + 1) : 0;

	char *newPath = (char*) XP_ALLOC (startingAt + XP_STRLEN(newName) + diffLen + 5);
	if (newPath)
	{
		XP_STRNCPY_SAFE (newPath, m_pathName, startingAt);
		XP_STRCAT (newPath, newName);
		if (diffLen > 0)
		{
			XP_STRCAT (newPath, ".sbd");
			XP_STRNCAT_SAFE (newPath, diffEnd, diffLen);
		}
		XP_FREE (m_pathName);
		m_pathName = newPath;
	}
	else
		err = MK_OUT_OF_MEMORY;

	// recurse down the subfolders, changing their pathname in memory
	MSG_FolderInfoMail *folder = NULL;
	for (int i = 0; i < m_subFolders->GetSize() && err == 0; i++)
	{
		folder = (MSG_FolderInfoMail*) m_subFolders->GetAt(i);
		XP_ASSERT(folder->IsMail());
		err = folder->PropagateRename (newName, startingAt);
	}

    return err;
}


MsgERR MSG_FolderInfoMail::PropagateAdopt (const char *parentName, uint8 parentDepth)
{
    XP_ASSERT(parentName);
    MSG_FolderInfoMail *child = NULL;
    MsgERR err = eSUCCESS;

    // Build the new path to this folder
    const char *pathNameLeaf = XP_STRRCHR (m_pathName, '/'); 
    XP_ASSERT(pathNameLeaf);
    
    char *newPathName = (char*) XP_ALLOC (XP_STRLEN(parentName) + XP_STRLEN(pathNameLeaf) + XP_STRLEN(".sbd/") + 1);
    XP_STRCPY (newPathName, parentName);
    if (parentDepth > 1)
        XP_STRCAT (newPathName, ".sbd");
    XP_STRCAT (newPathName, pathNameLeaf);

    // Move the mail folder new parent's tree
    int mailboxRenameStatus = 0;
    if (GetType() == FOLDER_MAIL)   // only rename local mailbox file, imap mailbox is not here - km
        mailboxRenameStatus = XP_FileRename(m_pathName, xpMailFolder, newPathName, xpMailFolder);
    if (0 == mailboxRenameStatus)
    {
        // Close the summary file and move it
        MessageDB *db = NULL;
        err = CloseDatabase (m_pathName, &db);
        int dataBaseRenameStatus = 0;
		XP_StatStruct fileStat;
        if (!XP_Stat(m_pathName, &fileStat, xpMailFolderSummary))
            dataBaseRenameStatus = XP_FileRename(m_pathName, xpMailFolderSummary, newPathName, xpMailFolderSummary);
        if (0 == dataBaseRenameStatus)
        {
            if (db)
                err = ReopenDatabase (db, newPathName);

            // Depth at new location may be different
            m_depth = parentDepth + 1;

            if (m_subFolders->GetSize() > 0)
            {
                // Create a subdirectory in the new location
                XP_MakeDirectory (newPathName, xpMailSubdirectory);
                
                // Adopt our children into the new tree
                for (int i = 0; i < m_subFolders->GetSize(); i++)
                {
                    child = (MSG_FolderInfoMail*) m_subFolders->GetAt (i);
                    err = child->PropagateAdopt (newPathName, m_depth);
                }

                // Remove the old subdirectory since its children have been moved
                XP_RemoveDirectory (m_pathName, xpMailSubdirectory);
            }

            // Now we're really done with the old name
            XP_FREE(m_pathName);
            m_pathName = newPathName;
            newPathName = NULL;
        }
        else
            err = MK_MSG_CANT_CREATE_FOLDER;
    }
    else
        err = MK_MSG_CANT_CREATE_FOLDER;

    if (newPathName)
        XP_FREE(newPathName); // only for error conditions

    return err;
}


MsgERR MSG_FolderInfoMail::Adopt (MSG_FolderInfo *srcFolder, int32 *pOutPos)
{
	MsgERR err = eSUCCESS;
	XP_ASSERT (srcFolder->GetType() == GetType());	// we can only adopt the same type of folder
	MSG_FolderInfoMail *mailFolder = (MSG_FolderInfoMail*) srcFolder;

	if (srcFolder == this)
		return MK_MSG_CANT_COPY_TO_SAME_FOLDER;

	if (ContainsChildNamed(mailFolder->GetName()))
		return MK_MSG_FOLDER_ALREADY_EXISTS;	
	
	// If we aren't already a directory, create the directory and set the flag bits
	if (0 == m_subFolders->GetSize())
	{
		XP_Dir dir = XP_OpenDir (m_pathName, xpMailSubdirectory);
		if (dir)
			XP_CloseDir (dir);
		else
		{
			XP_MakeDirectory (m_pathName, xpMailSubdirectory);
			dir = XP_OpenDir (m_pathName, xpMailSubdirectory);
			if (dir)
				XP_CloseDir (dir);
			else
				err = MK_COULD_NOT_CREATE_DIRECTORY;
		}
		if (eSUCCESS == err)
		{
			m_flags |= MSG_FOLDER_FLAG_DIRECTORY;
			m_flags |= MSG_FOLDER_FLAG_ELIDED;
		}
	}

	// Recurse the tree to adopt srcFolder's children
	err = mailFolder->PropagateAdopt (m_pathName, m_depth);

	// Add the folder to our tree in the right sorted position
	if (eSUCCESS == err)
	{
		XP_ASSERT(m_subFolders->FindIndex(0, srcFolder) == -1);
		*pOutPos = m_subFolders->Add (srcFolder);
	}

	return err;
}

MSG_FolderInfoMail::~MSG_FolderInfoMail() 
{
	if (m_pathName)
		MSG_Biff_Master::RemoveBiffFolder(m_pathName);
	FREEIF(m_pathName);
}


FolderType MSG_FolderInfoMail::GetType() 
{
    return FOLDER_MAIL;
}


char *MSG_FolderInfoMail::BuildUrl (MessageDB *db, MessageKey key) 
{
    const char *urlScheme = "mailbox:";
    const char *urlSeparator = "?id=";

    char *url = NULL;
    if (db) 
	{
		DBMessageHdr *msgHdr = db->GetDBHdrForKey(key);
        if (msgHdr) 
		{
			XPStringObj messageId;
			msgHdr->GetMessageId(messageId, db->GetDB());

            char *escapedId = EscapeMessageId (messageId);
            if (escapedId) {
                char *messageNumStr = PR_smprintf("&number=%ld", key);

                url = (char*) XP_ALLOC (XP_STRLEN(m_pathName) + 
                                        XP_STRLEN(escapedId) + XP_STRLEN(urlScheme) + 
                                        XP_STRLEN(urlSeparator) + 
                                        XP_STRLEN(messageNumStr) + 1 /*null terminator*/);

                if (url) {
                    XP_STRCPY (url, urlScheme);
                    XP_STRCAT (url, m_pathName);
                    XP_STRCAT (url, urlSeparator);
                    XP_STRCAT (url, escapedId);
                    XP_STRCAT(url, messageNumStr);
                }
                XP_FREE(escapedId);
                XP_FREE(messageNumStr);
            }
			delete msgHdr;
        }
    } else {
        url = PR_smprintf("%s%s", urlScheme, m_pathName);
    }
    return url;
}

MsgERR MSG_FolderInfoMail::ReadDBFolderInfo (XP_Bool force /* = FALSE */)
{
	MsgERR err = eSUCCESS;

	if (m_master->InitFolderFromCache (this))
		return err;

	if (force || !m_HaveReadNameFromDB)
    {
		DBFolderInfo *folderInfo;
		MessageDB *db;

		err = GetDBFolderInfoAndDB (&folderInfo, &db);
		if (eSUCCESS == err)
		{
			// Get the number of expunged byted
			m_expungedBytes = folderInfo->m_expunged_bytes;

			// Get the real mailbox name. Use this flag to preserve the
			// contract we have with the FE, which is that we're allowed
			// to give out an unallocated pointer, and must never free it.
			if (!m_HaveReadNameFromDB)
			{
				if (db)
				{
					XPStringObj userBoxName;
					db->m_dbFolderInfo->GetMailboxName(userBoxName);
					if (XP_STRLEN(userBoxName))			
						SetName(userBoxName);
					// else, this db is old or this folder was created from
					// a mailbox file and no corresponding db file.
				}
			}

			// Tell the base class to go get all its stuff
			err = MSG_FolderInfo::ReadDBFolderInfo (force);

			if (db)
				db->Close();
		}

		// Set this so we don't keep asking if we didn't get it.
		m_HaveReadNameFromDB = TRUE;
	}
	return err;
}

#define kMailFolderCacheFormat "\t%s\t%ld"
#define kMailFolderCacheVersion 1

MsgERR MSG_FolderInfoMail::WriteToCache (XP_File f)
{
	XP_FilePrintf (f, "\t%d", kMailFolderCacheVersion);
	XP_FilePrintf (f, kMailFolderCacheFormat, GetName(), (long) m_expungedBytes);

	return MSG_FolderInfo::WriteToCache (f);
}

MsgERR MSG_FolderInfoMail::ReadFromCache (char *buf)
{
	MsgERR err = eSUCCESS;
	int version;
	int tokensRead = sscanf (buf, "\t%d", &version);
	SkipCacheTokens (&buf, tokensRead);

	if (version == 1)
	{
		// Use natural long tmp to prevent 64-bit OSs from scribbling 64 bits into an int32
		long tmpExpungedBytes;
		char *tmpName = (char*) XP_ALLOC(XP_STRLEN(buf));
		if (tmpName)
		{
			sscanf (buf, kMailFolderCacheFormat, tmpName, &tmpExpungedBytes);
			char *name = buf + 1; // skip over tab;
			char *secondTab = XP_STRCHR(name, '\t');
			if (secondTab)
			{
				*secondTab = '\0';
				SetName (name); // kinda hacky. maybe scan/printf wasn't such a good idea
				*secondTab = '\t';
			}
			SkipCacheTokens (&buf, 2);
			m_expungedBytes = (int32) tmpExpungedBytes;
			XP_FREE(tmpName);
		}
		else
			err = eOUT_OF_MEMORY;
		m_HaveReadNameFromDB = TRUE;
	}
	else
		err = eUNKNOWN;

	err = MSG_FolderInfo::ReadFromCache (buf);

	return err;
}

const char *MSG_FolderInfoMail::GetRelativePathName ()
{
#if 0
	// ###phil What we really want here is the profile directory, but 
	// there's no XP way to get that yet. jonm says he'll add a pref.
	const char *userMailDir = m_master->GetPrefs()->GetFolderDirectory();
	return m_pathName + XP_STRLEN(userMailDir);
#else
	return m_pathName;
#endif
}

MsgERR  MSG_FolderInfoMail::GetDBFolderInfoAndDB(DBFolderInfo **folderInfo, MessageDB **db)
{
    MailDB  *mailDB;

    MsgERR openErr;
    XP_Bool wasCreated;
    XP_ASSERT(db && folderInfo);
    if (!db || !folderInfo)
        return eBAD_PARAMETER;

    if (GetType() == FOLDER_MAIL)
        openErr = MailDB::Open (GetMailFolderInfo()->GetPathname(), FALSE, &mailDB, FALSE);
    else
        openErr = ImapMailDB::Open (GetMailFolderInfo()->GetPathname(), FALSE, &mailDB, 
                                        GetMaster(), &wasCreated);

    *db = mailDB;
    if (openErr == eSUCCESS && *db)
        *folderInfo = (*db)->GetDBFolderInfo();
    return openErr;
}

XP_Bool MSG_FolderInfoMail::UpdateSummaryTotals()
{
	MsgERR err = ReadDBFolderInfo (TRUE /*force read*/); 

	// If we asked, but didn't get any, stop asking
	if (m_numUnreadMessages == -1)
		m_numUnreadMessages = -2;

	return err == eSUCCESS;
}


MSG_FolderInfoMail* MSG_FolderInfoMail::FindPathname(const char* p) 
{
    if (XP_FILENAMECMP(m_pathName, p) == 0) 
        return this;
    for (int i=0 ; i < m_subFolders->GetSize() ; i++) {
        MSG_FolderInfoMail* sub = (MSG_FolderInfoMail*) m_subFolders->GetAt(i);
        FolderType folderType = sub->GetType();
        XP_ASSERT((folderType == FOLDER_MAIL) ||
                  (folderType == FOLDER_IMAPMAIL)); // descends from MSG_FolderInfoMail
        MSG_FolderInfoMail* result = sub->FindPathname(p);
        if (result) 
            return result;
    }
    return NULL;
}

XP_Bool MSG_FolderInfoMail::IsDeletable ()
{
    // These are specified in the "Mail/News Windows" UI spec
    if ((m_flags & MSG_FOLDER_FLAG_TRASH) && !DeleteIsMoveToTrash())
    	return TRUE;	// allow delete of trash if we don't use trash
    if (m_depth == 1)
        return FALSE;
    if (m_flags & MSG_FOLDER_FLAG_INBOX || 
        m_flags & MSG_FOLDER_FLAG_DRAFTS || 
        m_flags & MSG_FOLDER_FLAG_TRASH ||
		m_flags & MSG_FOLDER_FLAG_TEMPLATES)
        return FALSE;
    return TRUE;
}

XP_Bool MSG_FolderInfoMail::RequiresCleanup()
{
	if (m_expungedBytes > 0)
	{
		int32 purgeThreshhold = m_master->GetPrefs()->GetPurgeThreshhold();
		XP_Bool purgePrompt = m_master->GetPrefs()->GetPurgeThreshholdEnabled();;
		return (purgePrompt && m_expungedBytes / 1000L > purgeThreshhold);
	}
	return FALSE;
}


int32 MSG_FolderInfoMail::GetSizeOnDisk ()
{
	int32 ret = 0;
	XP_StatStruct st;

    if (!XP_Stat(GetPathname(), &st, xpMailFolder))
		ret += st.st_size;

    if (!XP_Stat(GetPathname(), &st, xpMailFolderSummary))
		ret += st.st_size;

	return ret;
}

void MSG_FolderInfoMail::UpdatePendingCounts(MSG_FolderInfo * dstFolder, 
											 IDArray	*srcArray,
											 XP_Bool countUnread, XP_Bool missingAreRead)
{
    if (dstFolder->GetType() == FOLDER_IMAPMAIL)
    {
		XP_Bool wasCreated=FALSE;

		dstFolder->ChangeNumPendingTotalMessages(srcArray->GetSize());

		if (countUnread)
		{
			MailDB *affectedDB = NULL;
			ImapMailDB::Open(GetPathname(), FALSE, &affectedDB, GetMaster(), &wasCreated);
			if (affectedDB)
			{
				// count the moves that were unread
				int numUnread = 0;
				for (int keyIndex=0; keyIndex < srcArray->GetSize(); keyIndex++)
				{
					// if the key is not there, then assume what the caller tells us to.
					XP_Bool isRead = missingAreRead;
					affectedDB->IsRead((*srcArray)[keyIndex], &isRead);
					if (!isRead)
						numUnread++;
				}
			
				if (numUnread)
					dstFolder->ChangeNumPendingUnread(numUnread);
				affectedDB->Close();
			}
		}
		dstFolder->SummaryChanged();
	}
} 


MSG_FolderInfoNews::MSG_FolderInfoNews(char *groupName, msg_NewsArtSet* set,
									   XP_Bool subscribed, MSG_NewsHost* host,
									   uint8 depth)
: MSG_FolderInfo(groupName, depth)
{
	m_flags |= MSG_FOLDER_FLAG_NEWSGROUP;
	m_host = host;
	m_set = set;
	m_subscribed = subscribed;
	m_dbfilename = NULL;
	m_XPDBFileName = NULL;
	m_XPRuleFileName = NULL;
	m_prettyName = NULL;
	m_master = m_host->GetMaster();
	m_listNewsGroupState = NULL;
	if (m_subscribed) {
#ifdef XP_MAC
		// Should be done on all platforms: macintosh-only for beta-paranoid reasons.
		m_flags |= MSG_FOLDER_FLAG_SUBSCRIBED;
#endif
		if (m_host->IsCategoryContainer(groupName))
			m_flags |=
				MSG_FOLDER_FLAG_CAT_CONTAINER | MSG_FOLDER_FLAG_CATEGORY;
		if (m_host->IsProfile(groupName))
			m_flags |= MSG_FOLDER_FLAG_PROFILE_GROUP;
	}

	m_wantNewTotals = TRUE;
	m_purgeStatus = PurgeMaybeNeeded;
	m_username = NULL;
	m_password = NULL;
	m_nntpHighwater = 0;
	m_nntpTotalArticles = -1;
	m_totalInDB = 0;
	m_unreadInDB = 0;
	m_autoSubscribed = FALSE;
	m_isCachable = FALSE;
}

MSG_FolderInfoNews::~MSG_FolderInfoNews()
{
    FREEIF(m_dbfilename);
    FREEIF(m_XPDBFileName);
    delete [] m_prettyName;
	m_prettyName = NULL;
	FREEIF(m_username);
	FREEIF(m_password);
	delete m_set;
}
    

char *MSG_FolderInfoNews::BuildUrl (MessageDB * db, MessageKey key)
{
	char* url = NULL;
	const char* groupname = GetNewsgroupName();
	const char* urlbase = GetHost()->GetURLBase();

	if (db == NULL) {

		if (key == MSG_MESSAGEKEYNONE)
			url = PR_smprintf("%s/%s", urlbase, groupname);
	}
	else
	{
		DBMessageHdr *header = db->GetDBHdrForKey (key); 

		if (header != NULL) 
		{
			XPStringObj messageId;
			header->GetMessageId(messageId, db->GetDB());
			char *escapedId = EscapeMessageId (messageId);
			if (escapedId) 
			{
				url = PR_smprintf("%s/%s", urlbase, escapedId);
				XP_FREE(escapedId);
			}
			delete header;
		}
	}
	return url;
}

/* Compute a value for the "?newshost=" parameter.  This gets passed in
   with all of the mailto's, even the ones that don't have to do with
   posting, because the user might decide later to type in some newsgroups,
   and we need to know which host to send them to.  Perhaps the UI should
   have a way of selecting the host - say you wanted to forward a message
   frome one news server to another.  But it doesn't now.  
   The caller must free the returned pointer.
*/
char *MSG_FolderInfoNews::GetHostUrl()
{
	const char *hostName;
	HG26477

	hostName = GetHost()->getNameAndPort();
	MSG_NewsHost *newsHost = GetHost(); 
	if (newsHost)
	{
		HG78365
		if (hostName != NULL)
		{
			int32 length = XP_STRLEN(hostName) + 20;
			char *host = (char *) XP_ALLOC (length);
			if (!host)
				return 0; /* #### MK_OUT_OF_MEMORY */
			XP_STRCPY (host, hostName);
			HG89732
			return host;
		}
	}
	return NULL;
}

FolderType MSG_FolderInfoNews::GetType() {
  return FOLDER_NEWSGROUP;
}

// more virtual inlines for the compiler-challenged platforms
XP_Bool MSG_FolderInfoNews::IsMail () { return FALSE; }
XP_Bool MSG_FolderInfoNews::IsNews () { return TRUE; }
XP_Bool MSG_FolderInfoNews::IsDeletable () { return TRUE; }
MSG_FolderInfoNews	*MSG_FolderInfoNews::GetNewsFolderInfo() {return this;}

int32					MSG_FolderInfoNews::GetTotalMessagesInDB() const 
{
	return m_totalInDB;	// override this, since m_numTotalMessages for news is server-based
}


MsgERR MSG_FolderInfoNews::CreateSubfolder (const char *, MSG_FolderInfo **, int32 *)
{
    //PHP maybe we can use this to create categories
    return 0;
}

MsgERR MSG_FolderInfoNews::Delete ()
{
    return 0;
}

MsgERR MSG_FolderInfoNews::Rename (const char * /*newName*/)
{
    return MK_UNABLE_TO_OPEN_FILE;
}

MsgERR MSG_FolderInfoNews::Adopt (MSG_FolderInfo *src, int32 *newIdx)
{
	MsgERR err = MK_MSG_CANT_MOVE_FOLDER;

	if (0 == m_host->ReorderGroup (src->GetNewsFolderInfo(), this, newIdx))
		err = eSUCCESS;

    return err;
}


msg_NewsArtSet*
MSG_FolderInfoNews::GetSet()
{
    return m_set;
}

XP_Bool
MSG_FolderInfoNews::IsSubscribed()
{
    return m_subscribed;
}

XP_Bool
MSG_FolderInfoNews::IsCategory()
{
	return m_subscribed && m_host->IsCategory(GetNewsgroupName());
	// ###tw  Probably should keep this info cached.
}

XP_Bool MSG_FolderInfoNews::CanBeInFolderPane () 
{ 
	return m_subscribed; 
}

const char *MSG_FolderInfoNews::GetRelativePathName ()
{
	// xp db file name ought to be unique enough
	return GetXPDBFileName();
}

const char*
MSG_FolderInfoNews::GetDBFileName()
{
    if (!m_dbfilename) {
        const char* dbFileName = GetXPDBFileName();
        if (!dbFileName) return NULL;
        m_dbfilename = WH_FileName(dbFileName, xpXoverCache);
    }
    return m_dbfilename;
}

char *
MSG_FolderInfoNews::GetUniqueFileName(const char *extension)
{
	char* tmp;
	const char* newsgroupName = GetNewsgroupName();
	const char* dbName = m_host->GetDBDirName();		
	if (XP_STRLEN(newsgroupName) > MAX_FILE_LENGTH_WITHOUT_EXTENSION)
	{
	    // Use a hex version of the unique id (forcing 8.3 format).
	    long uniqueID = (long)m_host->GetUniqueID(newsgroupName);
	    tmp = PR_smprintf("%s/08%lX.%s", dbName, uniqueID, extension);
    }
    else
	    tmp = PR_smprintf("%s/%s.%s", dbName, newsgroupName, extension);
    if (!tmp) 
        return NULL;
    return tmp;
#undef MAX_FILE_LENGTH_WITHOUT_EXTENSION
}

const char *
MSG_FolderInfoNews::GetXPDBFileName()
{
    if (!m_XPDBFileName)
    {
		m_XPDBFileName = GetUniqueFileName("snm");
    }
    return m_XPDBFileName;
}

const char *
MSG_FolderInfoNews::GetXPRuleFileName()
{
    if (!m_XPRuleFileName)
	{
		m_XPRuleFileName = GetUniqueFileName("dat");
	}
	return m_XPRuleFileName;
}

const char *MSG_FolderInfoNews::GetPrettyName()
{ 
    if (!m_prettyName) {
		m_prettyName = m_host->GetPrettyName(GetNewsgroupName());
    }
    return m_prettyName;
}

void MSG_FolderInfoNews::ClearPrettyName()
{
	if (m_prettyName)
	{
		delete [] m_prettyName;
		m_prettyName = NULL;
	}
}

NewsGroupDB *MSG_FolderInfoNews::OpenNewsDB(XP_Bool deleteInvalidDB /* = FALSE */, MsgERR *pErr /* = NULL */)
{
    NewsGroupDB *newsDB = NULL;
    char *url = BuildUrl(NULL, MSG_MESSAGEKEYNONE);
    MsgERR  err = eOUT_OF_MEMORY;

    if (url != NULL)
    {
        if ((err = NewsGroupDB::Open(url, m_host->GetMaster(), &newsDB)) != eSUCCESS)
            newsDB = NULL;
		if (err != eSUCCESS && deleteInvalidDB)
		{
			const char *dbFileName = GetXPDBFileName();

			XP_Trace("blowing away old database file\n");
			if (dbFileName != NULL)
			{
				XP_FileRemove(dbFileName, xpXoverCache);
		        err = NewsGroupDB::Open(url, m_host->GetMaster(), &newsDB);
			}
		}
		HG87535
        XP_FREE(url);
    }
    if (pErr)
        *pErr = err;
    return newsDB;
}

void	MSG_FolderInfoNews::MarkAllRead(MWContext *context, XP_Bool deep)
{
	MSG_FolderInfo::MarkAllRead(context, deep);
	if (m_set && 1 <= m_nntpHighwater)
	{
		m_set->AddRange(1, m_nntpHighwater);
        UpdateSummaryTotals();
        GetMaster()->BroadcastFolderChanged(this);
	}
}

MsgERR  MSG_FolderInfoNews::GetDBFolderInfoAndDB(DBFolderInfo **folderInfo, MessageDB **db)
{
    MsgERR  err;

    XP_ASSERT(db && folderInfo);
    if (!db || !folderInfo)
        return eBAD_PARAMETER;

    if ((*db = OpenNewsDB(FALSE, &err)) != NULL)
        *folderInfo = (*db) ->GetDBFolderInfo();

    return err;
}


MsgERR MSG_FolderInfoNews::ReadFromCache (char *buf)
{
	MsgERR err = MSG_FolderInfo::ReadFromCache (buf);
	if (eSUCCESS == err)
		m_isCachable = TRUE;
	return err;
}


XP_Bool MSG_FolderInfoNews::UpdateSummaryTotals()
{
    MessageDB   *newsDB;
    DBFolderInfo   *folderInfo;
    XP_Bool gotTotals = FALSE;

    MsgERR err = GetDBFolderInfoAndDB(&folderInfo, &newsDB);
    if (err == eSUCCESS && newsDB)
    {
        if (folderInfo != NULL)
        {
			m_isCachable = TRUE;
            m_totalInDB = m_numTotalMessages = folderInfo->GetNumMessages();
            m_unreadInDB = folderInfo->GetNumNewMessages();
			m_csid = folderInfo->GetCSID();
			if (m_nntpHighwater)
			{
				m_numUnreadMessages = m_set->CountMissingInRange(1, m_nntpHighwater);
				m_numTotalMessages = m_nntpTotalArticles;
				// can't be more unread than on the server (there can, but it's a bit weird)
				if (m_numUnreadMessages > m_numTotalMessages)
				{
					m_numUnreadMessages = m_numTotalMessages;
					int32 deltaInDB = MIN(m_totalInDB - m_unreadInDB, m_numUnreadMessages);
					// if we know there are read messages in the db, subtract that from the unread total
					if (deltaInDB > 0)
						m_numUnreadMessages -= deltaInDB;
				}
			}
			else
				m_numUnreadMessages = folderInfo->GetNumNewMessages();
		    XP_ASSERT(m_numUnreadMessages >= 0);
            gotTotals = TRUE;
			if (m_purgeStatus == PurgeMaybeNeeded)
				m_purgeStatus = newsDB->GetNewsDB()->PurgeNeeded(GetMaster()->GetPurgeHdrInfo(), GetMaster()->GetPurgeArtInfo()) 
					? PurgeNeeded : PurgeNotNeeded;
        }
        newsDB->Close();
    }
    if (!gotTotals) {
        // Well, we tried.  Set to -2 (meaning don't bother trying again),
        // unless we already got a guess from somewhere else, in which case
        // we'll keep that guess.
        if (m_numUnreadMessages < 0) {
            m_numUnreadMessages = -2;   // well, we tried...
        }
    }

    return gotTotals;
}


void
MSG_FolderInfoNews::UpdateSummaryFromNNTPInfo(int32 oldest,
                                              int32 youngest,
                                              int32 total)
{
    /* First, mark all of the articles now known to be expired as read. */
    if (oldest > 1) m_set->AddRange(1, oldest - 1);
    /* Now search the newsrc line and figure out how many of these
       messages are marked as unread. */
	// make sure youngest is a least 1. MSNews seems to return a youngest of 0.
	if (youngest == 0) 
		youngest = 1;
    int32 unread = m_set->CountMissingInRange(oldest, youngest);
    XP_ASSERT(unread >= 0);
    if (unread < 0) return;
    if (unread > total) {
        /* This can happen when the newsrc file shows more unread than exist
           in the group (total is not necessarily `end - start'.) */
        unread = total;
		int32 deltaInDB = m_totalInDB - m_unreadInDB;
		// if we know there are read messages in the db, subtract that from the unread total
		if (deltaInDB > 0)
			unread -= deltaInDB;
    }
    m_wantNewTotals = FALSE;
    m_numUnreadMessages = unread;
    m_numTotalMessages = total;
	m_nntpHighwater = youngest;
    m_master->BroadcastFolderChanged(this);
	m_nntpTotalArticles = total;
}

MsgERR MSG_FolderInfoNews::Subscribe(XP_Bool bSubscribe, MSG_Pane *invokingPane /* = NULL */)
{
    if (!bSubscribe)
    {
        // Try to delete the database to free up disk space
        if ((!m_XPDBFileName || 0 != XP_FileRemove (m_XPDBFileName, xpXoverCache)) && invokingPane)
        {
            const char *prompt = XP_GetString (MK_MSG_CANT_DELETE_NEWS_DB);
            char *message = PR_smprintf (prompt, GetName());
            if (message)
            {
                XP_Bool continueUnsubscribe = FE_Confirm (invokingPane->GetContext(), message);
                XP_FREE(message);
                if (!continueUnsubscribe)
                    return eUNKNOWN; //don't continue unsubscribing
            }
        }
    }

    if (m_subscribed != bSubscribe)
    {
        // Remember the new setting and rewrite the news rc file
        m_subscribed = bSubscribe;
#ifdef XP_MAC
		// Should be done on all platforms: macintosh-only for beta-paranoid reasons.
        m_flags ^= MSG_FOLDER_FLAG_SUBSCRIBED;
#endif
        GetHost()->MarkDirty();
    }
	return eSUCCESS; //continue unsubscribing
}


typedef struct tIMAPUploadInfo
{
	MSG_IMAPFolderInfoMail	*m_destFolder;
	MSG_FolderInfoMail		*m_srcFolder;
	MSG_Pane				*m_sourcePane;
	MailDB					*m_sourceDB;
	IDArray					*m_idArrayForTempDB;
	IDArray					*m_sourceIDArray;
	XP_Bool					 m_isMove;
} IMAPUploadInfo;

void MSG_IMAPFolderInfoMail::FinishUploadFromFile(struct MessageCopyInfo*, int status)
{
	if (m_uploadInfo)
	{
		if (m_uploadInfo->m_sourceDB)
			m_uploadInfo->m_sourceDB->Close();
		XP_FileRemove(m_uploadInfo->m_srcFolder->GetPathname(), xpMailFolder);
		XP_FileRemove(m_uploadInfo->m_srcFolder->GetPathname(), xpMailFolderSummary);
		MSG_FolderInfoMail *localTree = GetMaster()->GetLocalMailFolderTree();
		localTree->RemoveSubFolder(m_uploadInfo->m_srcFolder);
		// delete the original messages
		if (m_uploadInfo->m_isMove && status == 0)
		{
			MSG_IMAPFolderInfoMail *imapFolder = (m_uploadInfo->m_sourcePane->GetFolder())
				? m_uploadInfo->m_sourcePane->GetFolder()->GetIMAPFolderInfoMail() : 0;

			if (imapFolder)
				imapFolder->DeleteSpecifiedMessages(m_uploadInfo->m_sourcePane, *m_uploadInfo->m_sourceIDArray);
		}
		// unfortunately, PostMessageCopyExit hasn't been called yet - this leaks the folder info
		// which isn't acceptable - need to run this after we've called CleanupCopyMessagesInto
		// delete m_uploadInfo->m_srcFolder;
		delete m_uploadInfo->m_sourceIDArray;
		delete m_uploadInfo;
		m_uploadInfo = NULL;
	}
}

void MSG_IMAPFolderInfoMail::UploadMessagesFromFile(MSG_FolderInfoMail *srcFolder, MSG_Pane *sourcePane, MailDB *mailDB)
{
	m_uploadInfo->m_idArrayForTempDB = new IDArray;
	MWContext	*sourceContext = sourcePane->GetContext();

	// send a matching ending update - because of double copy nature of download+upload
	sourcePane->EndingUpdate(MSG_NotifyNone, 0, 0);

	MSG_FinishCopyingMessages(sourceContext);
	
	// search as view modification..inform search pane that we are done with this view
	// mscott: we have to do this here because when we copy news aticles to an IMAP folder,
	// we need to tell the search pane to close its DB view when the operation is done. 
	// This calback happens in ::CleanupMessageCopy for everyone else except for this case!!!
	// Why here? By the time we reach ::CleanupMessageCopy, the src folder is the temp file 
	// the news articles were copied to and not the news db view the search pane is keeping open....*sigh*
	if (sourcePane && sourcePane->GetPaneType() == MSG_SEARCHPANE && sourceContext && sourceContext->msgCopyInfo)
		((MSG_SearchPane *) sourcePane)->CloseView(sourceContext->msgCopyInfo->srcFolder);

	// clear out the copyinfo from the news source copy
	FREEIF(sourceContext->msgCopyInfo);

	if (mailDB)
	{
		mailDB->ListAllIds(m_uploadInfo->m_idArrayForTempDB);
		if (m_uploadInfo->m_idArrayForTempDB->GetSize() > 0)
		{
			srcFolder->StartAsyncCopyMessagesInto (this, 
										sourcePane, 
										mailDB,
										m_uploadInfo->m_idArrayForTempDB, 
										m_uploadInfo->m_idArrayForTempDB->GetSize(),
										sourcePane->GetContext(), 
										NULL, // do not run in url queue
										FALSE,
										MSG_MESSAGEKEYNONE);
		}
		else
		{
			XP_ASSERT(FALSE);
			FinishUploadFromFile(NULL, -1);
		}
		// we need to know when this is done so we can delete the tmp folder and folder info, and close the db.
	}
}

/* static */ int MSG_IMAPFolderInfoMail::UploadMsgsInFolder(void *uploadInfo, int status)
{
	IMAPUploadInfo *imapUploadInfo = (IMAPUploadInfo *) uploadInfo;

	imapUploadInfo->m_destFolder->m_uploadInfo = imapUploadInfo;
	if (status >= 0)
	{
		imapUploadInfo->m_destFolder->UploadMessagesFromFile(imapUploadInfo->m_srcFolder, imapUploadInfo->m_sourcePane, imapUploadInfo->m_sourceDB);
	}
	else
	{
		MSG_FinishCopyingMessages(imapUploadInfo->m_sourcePane->GetContext());
		imapUploadInfo->m_destFolder->FinishUploadFromFile(imapUploadInfo->m_sourcePane->GetContext()->msgCopyInfo, status);
	}
	return 0;
}

MsgERR MSG_FolderInfoNews::BeginCopyingMessages (MSG_FolderInfo *dstFolder, 
									  MessageDB * /* sourceDB */,
                                      IDArray *srcArray, 
                                      MSG_UrlQueue * /*urlQueue will use for news -> imap */,
                                      int32 srcCount,
                                      MessageCopyInfo *copyInfo) 
{ 
	FolderType	dstFolderType = dstFolder->GetType();
    NewsGroupDB *newsDB = OpenNewsDB();

	msg_move_state *state = &copyInfo->moveState;
    state->sourceNewsDB = newsDB;

    if (newsDB != NULL)
    {
        IDArray keysToSave;
        for (int32 i = 0; i < srcCount; i++)
            keysToSave.Add (srcArray->GetAt(i));

		if (dstFolderType == FOLDER_MAIL)
		{
			const char *fileName = ((MSG_FolderInfoMail*) dstFolder)->GetPathname();

			return SaveMessages (&keysToSave, fileName, state->sourcePane, newsDB);
		}
		else if (dstFolderType == FOLDER_IMAPMAIL)
		{
			return DownloadToTempFileAndUpload(copyInfo, keysToSave, dstFolder, newsDB);
		}
	}
	return 0;
}

int MSG_FolderInfoNews::FinishCopyingMessages(MWContext * /*context*/,
                                       MSG_FolderInfo * /* srcFolder */, 
                                       MSG_FolderInfo * dstFolder, 
									   MessageDB * /*sourceDB*/,
                                      IDArray * /*srcArray*/, 
                                      int32 /*srcCount*/,
                                      msg_move_state * state) 
{
    if (dstFolder && dstFolder->TestSemaphore(this))
        return MK_WAITING_FOR_CONNECTION;
    else
    {
        if (state->sourceNewsDB)
        {
            state->sourceNewsDB->Close();
            state->sourceNewsDB = NULL;
        }
        return  MK_CONNECTED;
    }
}

int MSG_FolderInfo::DownloadToTempFileAndUpload(MessageCopyInfo *copyInfo, IDArray &keysToSave, MSG_FolderInfo *dstFolder, MessageDB *sourceDB)
{
	XP_FileType tmptype;
	msg_move_state *state = &copyInfo->moveState;
	MSG_FolderInfoMail *dstMailFolder = dstFolder->GetMailFolderInfo();

	char *fileName = FE_GetTempFileFor(state->sourcePane->GetContext(), dstMailFolder->GetPathname(), xpMailFolder,
							   &tmptype);
	if (!fileName)
		return MK_OUT_OF_MEMORY;

	// this is unfortunate, but we'd like to have the tmp folder info in the folder tree so
	// that downloading the news articles will update the database. We're going to have
	// to remove it when we're done. This scheme also has the disadvantage of putting
	// all the messages in the same tmp file, and then uploading one by one, but it's 
	// the easiest way to hook the existing code. If we did it one message at a time,
	// we wouldn't need the database (or would we?).
	// We could pass a dest folder to saveMessages, null for the save as case.
	// Then, we wouldn't need to put it in the folder tree...

	MSG_FolderInfoMail *localTree = GetMaster()->GetLocalMailFolderTree();
	MSG_FolderInfoMail *tmpFolderInfo = new MSG_FolderInfoMail(XP_STRDUP(fileName), m_master, 2, 0);

	FREEIF(fileName);
	if (!tmpFolderInfo)
		return MK_OUT_OF_MEMORY;

	localTree->GetSubFolders()->Add(tmpFolderInfo);

	IMAPUploadInfo *uploadInfo = new IMAPUploadInfo;

	if (!uploadInfo)
		return MK_OUT_OF_MEMORY;

	// replace the dstFolder in the copy info to be the temp file to clean up locks correctly
	copyInfo->dstFolder = tmpFolderInfo;

	uploadInfo->m_destFolder = dstMailFolder->GetIMAPFolderInfoMail();
	uploadInfo->m_sourcePane = state->sourcePane;
	uploadInfo->m_srcFolder = tmpFolderInfo;
	uploadInfo->m_isMove = copyInfo->moveState.ismove;
	uploadInfo->m_sourceIDArray = new IDArray;
	uploadInfo->m_sourceIDArray->CopyArray(keysToSave);	// copy source id array.
	XP_File	outFp = XP_FileOpen (tmpFolderInfo->GetPathname(), xpMailFolder, XP_FILE_WRITE_BIN);
	if (outFp)
		XP_FileClose(outFp);
	if (copyInfo->moveState.destDB)
		copyInfo->moveState.destDB->Close();
	MsgERR err = MailDB::Open(tmpFolderInfo->GetPathname(), TRUE, &uploadInfo->m_sourceDB, FALSE);
	if ((err == eSUCCESS || err == eNoSummaryFile) && uploadInfo->m_sourceDB)
	{
		copyInfo->moveState.destDB = uploadInfo->m_sourceDB;
		uploadInfo->m_sourceDB->AddUseCount();
		state->sourcePane->GetContext()->currentIMAPfolder = this->GetIMAPFolderInfoMail();
    	MSG_StartImapMessageToOfflineFolderDownload(state->sourcePane->GetContext());
	// UploadMsgsInFolder needs to know the dstFolder, the source fileName, and the sourcePane, to run the context in.
		return SaveMessages(&keysToSave, tmpFolderInfo->GetPathname(), state->sourcePane, sourceDB, MSG_IMAPFolderInfoMail::UploadMsgsInFolder, uploadInfo);
	}
	else
		return MK_MSG_TMP_FOLDER_UNWRITABLE;
}

XP_Bool MSG_FolderInfoNews::RequiresCleanup()
{
	return (m_purgeStatus == PurgeNeeded);
}

void MSG_FolderInfoNews::ClearRequiresCleanup()
{
	m_purgeStatus = PurgeNotNeeded;
}

void MSG_FolderInfoNews::SetNewsgroupUsername(const char *username)
{
  if (username)
	StrAllocCopy (m_username, username);
  else
	FREEIF(m_username);
}


const char * MSG_FolderInfoNews::GetNewsgroupUsername(void)
{
  return m_username;
}


void MSG_FolderInfoNews::SetNewsgroupPassword(const char *password)
{
  if (password)
	StrAllocCopy(m_password, password);
  else
	FREEIF(m_password);
  RememberPassword(password);	// save it in the db for offline use.
}


const char * MSG_FolderInfoNews::GetNewsgroupPassword(void)
{
  return m_password;
}


XP_Bool MSG_FolderInfoNews::KnowsSearchNntpExtension()
{
	// The host must know the SEARCH extension AND the group must be in the LIST SEARCHES response
	return m_host->QueryExtension("SEARCH") && m_host->QuerySearchableGroup (GetName());
}


XP_Bool MSG_FolderInfoNews::AllowsPosting ()
{
	// Don't allow posting to virtual newsgroups
	if (m_flags & MSG_FOLDER_FLAG_PROFILE_GROUP)
		return FALSE;

	// The news host can tell us in the connection banner if it doesn't
	// allow posting. Honor that setting here.
	if (!m_host || !m_host->GetPostingAllowed())
		return FALSE;

	return TRUE;
}


int32 MSG_FolderInfoNews::GetSizeOnDisk ()
{
	int32 ret = 0;
	XP_StatStruct st;

    if (!XP_Stat(GetXPDBFileName(), &st, xpXoverCache))
		ret += st.st_size;

	return ret;
}


FolderType MSG_FolderInfoCategoryContainer::GetType() 
{
    return FOLDER_CATEGORYCONTAINER;
}

MSG_FolderInfoCategoryContainer::MSG_FolderInfoCategoryContainer(char *groupName, msg_NewsArtSet* set,
                       XP_Bool subscribed, MSG_NewsHost* host, uint8 depth)
    : MSG_FolderInfoNews(groupName, set, subscribed, host, depth)
{
}

MSG_FolderInfoCategoryContainer::~MSG_FolderInfoCategoryContainer()
{
}

MSG_FolderInfoNews *
MSG_FolderInfoCategoryContainer::BuildCategoryTree(MSG_FolderInfoNews *parent,
												   const char *groupName,
												   msg_GroupRecord* group,
												   uint8 depth,
												   MSG_Master *master)
{
	m_host->AssureAllDescendentsLoaded(group);
	
	msg_GroupRecord* child;
    int32 result = 0;

	// slip in a new parent which represents the root category.
    for (child = group->GetChildren() ; child ; child = child->GetSibling()) {
        MSG_FolderInfoNews *info = NULL;
        char *catName = PR_smprintf("%s.%s", groupName, child->GetPartName());
        if (!catName)
            break;

        if (child->IsGroup()) {
            info = m_host->FindGroup(catName);
            // this relies on auto-subscribe of categories!
            if (info && info->IsSubscribed()) {
                // set the level correctly for category pane.
                info->SetDepth(depth + 1);
                info->SetFlag(MSG_FOLDER_FLAG_CATEGORY);

                if (child->GetChildren()) 
                    info->SetFlag(MSG_FOLDER_FLAG_DIRECTORY);

				if (!parent->IsParentOf(info, FALSE))
					parent->GetSubFolders()->Add(info);
#ifdef DEBUG_bienvenu1
                XP_Trace("Adding %s at depth %d\n", catName, depth + 1);
#endif
//              parent = info;
                result++;
            }
        }
		BuildCategoryTree(info ? info : parent, catName, child,
						  depth + ((info) ? 1 : 0), master);

        XP_FREE(catName);
    }
    return parent;
}

// while it would be nice to put this in the right place, rebuilding the category
// tree is really easy.
MSG_FolderInfoNews *
MSG_FolderInfoCategoryContainer::AddToCategoryTree(MSG_FolderInfoNews* /*parent*/,
												   const char* /*groupName*/,
												   msg_GroupRecord* group,
												   MSG_Master *master)
{
	MSG_FolderInfoNews *retFolder = NULL;
	msg_GroupRecord *categoryParent = group->GetParent();
	if (categoryParent)
	{
		char	*categoryParentName = categoryParent->GetFullName();
		if (categoryParentName)
		{
			MSG_FolderInfoNews *parent = m_host->FindGroup(categoryParentName);
			if (parent)
			{
				// make sure we're using the root category...
				parent = parent->GetNewsFolderInfo();
				parent->GetSubFolders()->RemoveAll();
				retFolder = BuildCategoryTree(parent, categoryParentName, categoryParent, parent->GetDepth(), master);
			}
			delete [] categoryParentName;
		}
	}
	return retFolder;
}

MSG_FolderInfoCategoryContainer *MSG_FolderInfoNews::CloneIntoCategoryContainer()
{
	msg_NewsArtSet *newSet = m_set->Create(m_set->Output(), m_host);
	MSG_FolderInfoCategoryContainer *retFolder = 
		new MSG_FolderInfoCategoryContainer(m_name, newSet, m_subscribed, m_host, m_depth);
	if (retFolder)
	{
		retFolder->GetSubFolders()->Add(this);
	}
	// due to load time "optimization" for unsubscribed category containers, we may not know this!
	SetFlag(MSG_FOLDER_FLAG_CATEGORY); 
	return retFolder;
}

MSG_FolderInfoNews *MSG_FolderInfoCategoryContainer::CloneIntoNewsFolder()
{
	msg_NewsArtSet *newSet = m_set->Create(m_set->Output(), m_host);
	MSG_FolderInfoNews *retFolder = new MSG_FolderInfoNews(m_name, newSet, m_subscribed, m_host, m_depth);
	if (retFolder)
		retFolder->GetSubFolders()->InsertAt(0, m_subFolders);

	return retFolder;
}

// Well, this is a long shot, but lets try always returning the root
// category when this gets called.
MSG_FolderInfoNews *MSG_FolderInfoCategoryContainer::GetNewsFolderInfo()
{
	return GetRootCategory();
}

MSG_FolderInfoNews *MSG_FolderInfoCategoryContainer::GetRootCategory()
{
	return this;
}


MSG_FolderInfoContainer::MSG_FolderInfoContainer(const char* name, uint8 depth, XPCompareFunc* func)
	: MSG_FolderInfo(name, depth, func)
{
    m_flags |= MSG_FOLDER_FLAG_DIRECTORY;
    m_numUnreadMessages = 0;
}

MSG_FolderInfoContainer::~MSG_FolderInfoContainer()
{
}

FolderType 
MSG_FolderInfoContainer::GetType()
{
    return FOLDER_CONTAINERONLY;
}

char*
MSG_FolderInfoContainer::BuildUrl(MessageDB*, MessageKey)
{
    return NULL;
}

MsgERR
MSG_FolderInfoContainer::BeginCopyingMessages(MSG_FolderInfo *, 
											  MessageDB *,
                                              IDArray *, 
                                              MSG_UrlQueue *,
                                              int32,
                                              MessageCopyInfo *)
{
    XP_ASSERT(0);
    return eUNKNOWN;
}


int 
MSG_FolderInfoContainer::FinishCopyingMessages(MWContext *,
                                               MSG_FolderInfo *, 
                                               MSG_FolderInfo *, 
 											MessageDB * /*sourceDB*/,
                                              IDArray *, 
                                               int32,
                                               msg_move_state *)
{
    XP_ASSERT(0);
    return -1;
}

MsgERR  MSG_FolderInfoContainer::SaveMessages(IDArray*, const char *,
											  MSG_Pane *, MessageDB *,
											  int (*)(void *, int status) , void *)
{
    XP_ASSERT(0);
    return eUNKNOWN;
}

MsgERR
MSG_FolderInfoContainer::CreateSubfolder(const char *, MSG_FolderInfo**, int32 *)
{
    XP_ASSERT(0);
    return eUNKNOWN;
}


MsgERR
MSG_FolderInfoContainer::Delete()
{
    XP_ASSERT(0);
    return eUNKNOWN;
}


MsgERR
MSG_FolderInfoContainer::Adopt(MSG_FolderInfo *, int32*)
{
    return MK_MSG_CANT_MOVE_FOLDER;
}

MSG_FolderInfoMail* MSG_FolderInfoContainer::FindMailPathname(const char* p) 
{
    for (int i=0 ; i < m_subFolders->GetSize() ; i++) {
        MSG_FolderInfoMail* sub = (MSG_FolderInfoMail*) m_subFolders->GetAt(i);
        FolderType folderType = sub->GetType();
        XP_ASSERT((folderType == FOLDER_MAIL) ||
                  (folderType == FOLDER_IMAPMAIL)); // descends from MSG_FolderInfoMail
        MSG_FolderInfoMail* result = sub->FindPathname(p);
        if (result) 
            return result;
    }
    return NULL;
}

MSG_NewsFolderInfoContainer::MSG_NewsFolderInfoContainer(MSG_NewsHost* host,
														 uint8 depth)
	: MSG_FolderInfoContainer(host->getFullUIName(), depth, NULL)
{
	m_host = host;
	m_prettyName = NULL;
}

MSG_NewsFolderInfoContainer::~MSG_NewsFolderInfoContainer()
{
	FREEIF(m_prettyName);
}

XP_Bool 
MSG_NewsFolderInfoContainer::IsDeletable()
{
	return TRUE;
}


MsgERR
MSG_NewsFolderInfoContainer::Delete()
{
	return 0;
}


XP_Bool MSG_NewsFolderInfoContainer::KnowsSearchNntpExtension()
{
	return m_host->QueryExtension ("SEARCH");
}

XP_Bool MSG_NewsFolderInfoContainer::AllowsPosting()
{
	// The news host can tell us in the connection banner if it doesn't
	// allow posting. Honor that setting here.
	if (!m_host || !m_host->GetPostingAllowed())
		return FALSE;

	return TRUE;
}


int MSG_NewsFolderInfoContainer::GetNumSubFoldersToDisplay() const 
{
	// Since unsubscribed newsgroups have folderInfos in the tree, the news
	// host will appear to have children (and the FEs draw the twisty)
	// even when there are no subscribed groups. So consider whether the
	// groups are subscribed when counting up children for display purposes

	int numTotal, numSubscribed;
	numTotal = numSubscribed = GetNumSubFolders();

	for (int i = 0; i < numTotal; i++)
	{
		MSG_FolderInfoNews *newsFolder = GetSubFolder(i)->GetNewsFolderInfo();
		XP_ASSERT(newsFolder);
		if (newsFolder && !newsFolder->IsSubscribed())
			numSubscribed--;
	}

	return numSubscribed;
}

/*
--------------------------------------------------------------
MSG_FolderIterator
--------------------------------------------------------------
 */

// Construct a new folder iterator.
// ### mw What if someone else messes with this folder tree
//        while we're iterating over it?
MSG_FolderIterator::MSG_FolderIterator(MSG_FolderInfo *parent)
    : m_masterParent(parent), m_currentParent(parent), m_currentIndex(-1)
{
    Init();
}

MSG_FolderIterator::~MSG_FolderIterator()
{
    Init(); // This removes any stack we may have had.
}

// Initialize the iterator to a known state 
// (pointing at the first element in the folder tree).
void
MSG_FolderIterator::Init(void)
{
    // If we happen to have any stack left from a previous iteration,
    // then remove it.
    if (m_stack.GetSize() > 0)
    {
        // ### mw clumsy, but clean. I need to better understand the way that
        //        delete[] is used in XPPtrArray::SetSize(); perhaps I can
        //        call that instead of doing this.
        for(int i=0; i < m_stack.GetSize(); i++)
        {
            MSG_FolderIteratorStackElement *elem = 
                (MSG_FolderIteratorStackElement *) m_stack.GetAt(i);
            if (elem) delete elem;
        }
        m_stack.RemoveAt(0, m_stack.GetSize());
    }

    // Using a NULL parent means that we're operating on the
    // master parent folder.
    m_currentParent = NULL;
    m_currentIndex = 0;
}

// Push the currently iterating folder onto the stack.
// This is used when one of its subfolders contains subfolders.
XP_Bool
MSG_FolderIterator::Push(void)
{
    XP_Bool result = FALSE;
    MSG_FolderInfo *nextParent = NextNoAdvance();
    XP_ASSERT(nextParent != NULL);

    // Create a new layer to add to the stack.
    MSG_FolderIteratorStackElement *elem = new MSG_FolderIteratorStackElement;
    
    XP_ASSERT(elem != NULL);
    if (elem)
    {
        // Save current state info.
        elem->m_parent = m_currentParent;
        elem->m_currentIndex = m_currentIndex;
        
        // Add the new layer to the stack at the end.
        m_stack.InsertAt(m_stack.GetSize(), elem, 1);

        // Reset the current parent/index to reflect the new element.
        m_currentParent = nextParent;
        m_currentIndex = 0;
        
        result = TRUE; // signal that we successfully pushed
    }

    return result;
}

// Pop the most recently iterating folder container from the stack.
// This is typically used when one finishes iterating over a subfolder
// which itself contains subfolders.
XP_Bool
MSG_FolderIterator::Pop(void)
{
    XP_Bool result = FALSE;

    if (m_stack.GetSize() > 0)
    {
        // Get the most recent (highest numbered) element from the stack.
        MSG_FolderIteratorStackElement *elem = 
            (MSG_FolderIteratorStackElement *) m_stack[m_stack.GetSize() - 1];
        if (elem)
        {
            // Got a stack element. Shrink the stack.
            m_stack.RemoveAt(m_stack.GetSize() - 1, 1);
            
            // Restore state information from the popped stack element.
            m_currentParent = elem->m_parent;
            m_currentIndex = elem->m_currentIndex + 1;
            
            // Now that we don't need it, delete the stack element.
            delete elem;
            
            // Signal that we successfully popped.
            result = TRUE;
        }
    }
    return result;
}

MSG_FolderInfo *
MSG_FolderIterator::First()
{
    // Reset our iteration state.
    Init();
    // We're now pointing at the first folder info element. Return it.
    return Next();
}

MSG_FolderInfo *
MSG_FolderIterator::NextNoAdvance()
{
    MSG_FolderInfo *returnInfo = NULL;

    if ((!m_currentParent) && (m_currentIndex == 0))
        returnInfo = m_masterParent;
	else
	{
		// Have we run out of folders at this depth?
		while ((m_currentParent) && (m_currentIndex >= m_currentParent->GetNumSubFolders()))
		{
			// Try to pop out.
			if (!Pop())	// if we can't Pop, we must be done.
				break;
		}
		if ((m_currentParent) && 
				 (m_currentIndex < m_currentParent->GetNumSubFolders()))
		{
			// Get the folder we're sitting on.
			returnInfo = m_currentParent->GetSubFolder(m_currentIndex);
		}
	}
    // Return the folder.
    return returnInfo;
}

MSG_FolderInfo *
MSG_FolderIterator::Next()
{
    MSG_FolderInfo *returnInfo = NextNoAdvance();

    if (returnInfo)
        // Advance to the next folder, if any.
        Advance();

    // Return the folder.
    return returnInfo;
}

void
MSG_FolderIterator::Advance()
{

    // If the current folder sits atop subfolders, push down
    // into the subfolders.
    MSG_FolderInfo *currFolder = NextNoAdvance();

    if (currFolder && currFolder->HasSubFolders() > 0)
    {
        // Drill down to the next level.
        Push();
    }
    else
    {
        // Increment from where we are.
        m_currentIndex++;
    }
}

// Test whether we have more folders through which to iterate.
XP_Bool
MSG_FolderIterator::More()
{
    // We should be sitting on top of the next folder if it exists, so
    // test the next folder for existence.
    XP_Bool result = FALSE;
    if (!m_currentParent && m_currentIndex == 0)
        result = TRUE;
    if (m_currentParent && 
        m_currentIndex < m_currentParent->GetNumSubFolders())
        result = TRUE;

    return result;
}

// Return the advanced-to folder if successful, NULL otherwise.
MSG_FolderInfo *
MSG_FolderIterator::AdvanceToFolder(MSG_FolderInfo *desiredFolder)
{
	MSG_FolderInfo *curFolder = First();

	while(curFolder && curFolder != desiredFolder)
	{
		curFolder = Next();
	}
	return curFolder;
}
