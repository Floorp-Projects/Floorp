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

/* imap related libmsg stuff */ 

#include "msg.h"
#include "errcode.h"
#include "msgimap.h"
#include "maildb.h"
#include "mailhdr.h"
#include "msgprefs.h"
#include "msgurlq.h"
#include "msgdbvw.h"
#include "grpinfo.h"
#include "msgtpane.h"
#include "msgspane.h"
#include "imap.h"
#include "msgfpane.h"
#include "pmsgfilt.h"
#include "prsembst.h"
#include "xpgetstr.h"
#include "xplocale.h"
#include "imapoff.h"
#include "msgurlq.h"
#include "prefapi.h"
#include "msgundmg.h"
#include "msgundac.h"
#include "libi18n.h"
#include "msgmpane.h"
#include "prtime.h"
#include "imaphost.h"
#include "subpane.h"
#include "prthread.h"
#include "xp_hash.h"
#include "pw_public.h"
#include "msgundmg.h"
#include "prlong.h"
#include "msgbiff.h"
#include "netutils.h"

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
	extern int MK_MSG_ONLINE_IMAP_WITH_NO_BODY;
	extern int MK_MSG_ONLINE_COPY_FAILED;
	extern int MK_MSG_ONLINE_MOVE_FAILED;
	extern int MK_MSG_CANT_FIND_SNM;
	extern int MK_MSG_NO_UNDO_DURING_IMAP_FOLDER_LOAD;
	extern int MK_MSG_BOGUS_SERVER_MAILBOX_UID_STATE;
	extern int MK_POP3_NO_MESSAGES;
	extern int MK_MSG_LOTS_NEW_IMAP_FOLDERS;
	extern int MK_MSG_IMAP_DIR_PROMPT;
	extern int MK_MSG_IMAP_DISCOVERING_MAILBOX;
	extern int XP_ALERT_OFFLINE_MODE_SELECTED;
	extern int MK_MSG_IMAP_INBOX_NAME;
	extern int XP_MSG_PASSWORD_FAILED;
	extern int XP_MSG_IMAP_ACL_READ_RIGHT;
	extern int XP_MSG_IMAP_ACL_WRITE_RIGHT;
	extern int XP_MSG_IMAP_ACL_LOOKUP_RIGHT;
	extern int XP_MSG_IMAP_ACL_INSERT_RIGHT;
	extern int XP_MSG_IMAP_ACL_DELETE_RIGHT;
	extern int XP_MSG_IMAP_ACL_CREATE_RIGHT;
	extern int XP_MSG_IMAP_ACL_SEEN_RIGHT;
	extern int XP_MSG_IMAP_ACL_POST_RIGHT;
	extern int XP_MSG_IMAP_ACL_ADMINISTER_RIGHT;
	extern int XP_MSG_IMAP_ACL_FULL_RIGHTS;
	extern int XP_MSG_IMAP_UNKNOWN_USER;
	extern int XP_MSG_IMAP_PERSONAL_FOLDER_TYPE_NAME;
	extern int XP_MSG_IMAP_PERSONAL_SHARED_FOLDER_TYPE_NAME;
	extern int XP_MSG_IMAP_PUBLIC_FOLDER_TYPE_NAME;
	extern int XP_MSG_IMAP_OTHER_USERS_FOLDER_TYPE_NAME;	
	extern int XP_MSG_IMAP_PERSONAL_FOLDER_TYPE_DESCRIPTION;
	extern int XP_MSG_IMAP_PERSONAL_SHARED_FOLDER_TYPE_DESCRIPTION;
	extern int XP_MSG_IMAP_PUBLIC_FOLDER_TYPE_DESCRIPTION;
	extern int XP_MSG_IMAP_OTHER_USERS_FOLDER_TYPE_DESCRIPTION;	
}

// This string is defined in the ACL RFC to be "anyone"
#define IMAP_ACL_ANYONE_STRING "anyone"

// forward

extern "C" int32
NET_BlockingWrite (PRFileDesc *filedes, const void * buf, unsigned int nbyte);

void PostMessageCopyUrlExitFunc(URL_Struct *URL_s, int status, MWContext *window_id);
void UpdateStandAloneIMAPBiff(const IDArray& keysToFetch);

static
void PostEmptyImapTrashExitFunc(URL_Struct *URL_s, int status, MWContext* /*window_id*/)
{
	// if this was launched from a folder pane.  then start compress folders url.
	if ((status >= 0) && (URL_s->msg_pane->GetPaneType() == MSG_FOLDERPANE))
		((MSG_FolderPane *) URL_s->msg_pane)->CompressAllFolders();
}


// This exit function will get called when you pass false in the loadingFolder argument to
// MSG_IMAPFolderInfoMail::StartUpdateOfNewlySelectedFolder
void ImapFolderClearLoadingFolder(URL_Struct *URL_s, int /*status*/, MWContext* window_id)
{
	if (URL_s->msg_pane && !URL_s->msg_pane->GetActiveImapFiltering())
	{
		MSG_IMAPFolderInfoMail *loadedFolder = URL_s->msg_pane->GetLoadingImapFolder();
		URL_s->msg_pane->SetLoadingImapFolder(NULL);
		if (loadedFolder)
		{
			loadedFolder->SetGettingMail(FALSE);
			loadedFolder->NotifyFolderLoaded(URL_s->msg_pane);	// dmb - fix stop watch when no connections?
		}
	}
}

// This exit function will get called when you pass true in the loadingFolder argument to
// MSG_IMAPFolderInfoMail::StartUpdateOfNewlySelectedFolder
void ImapFolderSelectCompleteExitFunction(URL_Struct *URL_s, int /*status*/, MWContext* window_id)
{
	if (URL_s->msg_pane && !URL_s->msg_pane->GetActiveImapFiltering())
	{
		MSG_IMAPFolderInfoMail *loadedFolder = URL_s->msg_pane->GetLoadingImapFolder();
		URL_s->msg_pane->SetLoadingImapFolder(NULL);
		
		XP_Bool	sendFolderLoaded = (loadedFolder != NULL);

		if (sendFolderLoaded)
		{
			MSG_Master *master = URL_s->msg_pane->GetMaster();
			loadedFolder->SetGettingMail(FALSE);
			if (master->IsCachePasswordProtected() && !master->IsUserAuthenticated() && !NET_IsOffline())
			{
				sendFolderLoaded = FALSE;
				char *alert = XP_GetString(XP_MSG_PASSWORD_FAILED);
#ifndef XP_UNIX
				FE_Alert(window_id, alert);
#else
				FE_Alert_modal(window_id, alert);
#endif
#ifndef XP_MAC
				URL_Struct *url_struct = NET_CreateURLStruct("mailbox:", NET_SUPER_RELOAD);
				if (url_struct)
					FE_GetURL(window_id, url_struct);
#endif
				FE_PaneChanged(URL_s->msg_pane, TRUE, MSG_PaneNotifyFolderDeleted, (uint32) loadedFolder);
			}
		}
		if (sendFolderLoaded)
		{
			FE_PaneChanged(URL_s->msg_pane, TRUE, MSG_PaneNotifyFolderLoaded, (uint32) loadedFolder);
			loadedFolder->NotifyFolderLoaded(URL_s->msg_pane);	// dmb - fix stop watch when no connections?
		}
	}
#ifdef DEBUG_bienvenu
	else if (URL_s->msg_pane)
		XP_Trace("pane has active imap filtering so we didn't send a folderloaded notification\n");
#endif
}

// static
void MSG_IMAPFolderInfoMail::ReportErrorFromBuildIMAPFolderTree(MSG_Master *mailMaster, const char *fileName, XP_FileType filetype)
{
	XP_FILE_NATIVE_PATH failedDir = WH_FileName(fileName,filetype);
	char *errormsg = failedDir ? PR_smprintf(XP_GetString(MK_COULD_NOT_CREATE_DIRECTORY), failedDir) : (char *)NULL;
	MWContext *alertContext = mailMaster->GetFirstPane() ? mailMaster->GetFirstPane()->GetContext() : (MWContext *)NULL;
	if (errormsg)
		FE_Alert(alertContext, errormsg);
	FREEIF(failedDir);
	FREEIF(errormsg);
}


MSG_IMAPFolderInfoContainer*
MSG_IMAPFolderInfoMail::BuildIMAPFolderTree (MSG_Master *mailMaster, 
                                             uint8 depth, 
                                             MSG_FolderArray * parentArray)
{
    // the imap folders start at level 1
    XP_ASSERT(depth == 1);  
    
    XP_Bool errorReturnRootContainerOnly = FALSE;
        
    // make sure the root imap folder exists
    XP_StatStruct imapfolderst;
    if (XP_Stat("", &imapfolderst, xpImapRootDirectory))
    {
        XP_MakeDirectory("", xpImapRootDirectory);
        if (XP_Stat("", &imapfolderst, xpImapRootDirectory))
        {
            errorReturnRootContainerOnly = TRUE;       // can't create dir, why?
        }
    }
    else if (!S_ISDIR(imapfolderst.st_mode))
    	errorReturnRootContainerOnly = TRUE;
    
    if (errorReturnRootContainerOnly)
    	ReportErrorFromBuildIMAPFolderTree(mailMaster,"", xpImapRootDirectory);

	MSG_Prefs *prefs = mailMaster->GetPrefs();
	if (prefs)
	{
		int32 profileAge = prefs->GetStartingMailNewsProfileAge();
		// Upgrade default IMAP host prefs
		if (! (profileAge & MSG_IMAP_DEFAULT_HOST_UPGRADE_FLAG))	// have we upgraded the default host?
		{
			prefs->SetMailNewsProfileAgeFlag(MSG_IMAP_DEFAULT_HOST_UPGRADE_FLAG);
			MSG_IMAPHost::UpgradeDefaultServerPrefs(mailMaster);
		}
	}

	char *serverList = NULL;
	MSG_IMAPFolderInfoContainer *container = NULL;

	PREF_CopyCharPref("network.hosts.imap_servers", &serverList);
	char *serverPtr = serverList;
	while (serverPtr)
	{
		char *endPtr = XP_STRCHR(serverPtr, ',');
		if (endPtr)
			*endPtr++ = '\0';
		MSG_IMAPFolderInfoContainer *curContainer = BuildIMAPServerTree(mailMaster, serverPtr, parentArray, errorReturnRootContainerOnly, depth);
		if (!container)
			container = curContainer;	// we will return first container;
		serverPtr = endPtr;
	}
	FREEIF(serverList);	  
	return container; // return first container.
}

/* static */
MSG_IMAPFolderInfoContainer* MSG_IMAPFolderInfoMail::BuildIMAPServerTree(MSG_Master *mailMaster, 
                                             const char *imapServerName, 
                                             MSG_FolderArray * parentArray,
											 XP_Bool errorReturnRootContainerOnly,
											 int depth)
{
    XP_StatStruct imapfolderst;
	// see if there's a :port in the server name.  If so, strip it off when
	// creating the server directory.
	char *port = 0;
	if (imapServerName)
	{
		port = XP_STRCHR(imapServerName,':');
		if (port)
			*port=0;
	}
	const char *imapServerDir = imapServerName;

#if defined(XP_WIN16) || defined(XP_MAC) || defined(XP_OS2)

#if defined(XP_WIN16) || defined(XP_OS2)
	const int limit = 8;
#elif XP_MAC
	const int limit = 31;
#endif

	int len = 0;
	if ((len = XP_STRLEN(imapServerDir)) > limit) {
		imapServerDir = imapServerDir + len - limit;
	}
#endif

	MSG_IMAPHost *imapHost = MSG_IMAPHost::AddHostFromPrefs(imapServerName, mailMaster);
	XP_Bool alreadyReportedError = errorReturnRootContainerOnly;
	if (!errorReturnRootContainerOnly && XP_Stat(imapServerDir, &imapfolderst, xpImapServerDirectory))
    {
        XP_MakeDirectory(imapServerDir, xpImapServerDirectory);
        if (XP_Stat(imapServerDir, &imapfolderst, xpImapServerDirectory))
        {
            errorReturnRootContainerOnly = TRUE;       // can't create dir, why?
        }
    }
    else if (!errorReturnRootContainerOnly && !S_ISDIR(imapfolderst.st_mode))
    	errorReturnRootContainerOnly = TRUE;
    
    if (errorReturnRootContainerOnly && !alreadyReportedError)
    	ReportErrorFromBuildIMAPFolderTree(mailMaster,imapServerDir, xpImapServerDirectory);

    // create the root level imap box 
    char *defaultImapDirectory = XP_STRDUP(mailMaster->GetPrefs()->GetIMAPFolderDirectory());
    char *imapDirLeafName = XP_STRRCHR(defaultImapDirectory, '/');
    XP_ASSERT(imapDirLeafName);
	imapDirLeafName++;
	*imapDirLeafName = '\0';
	char *imapDirectory = XP_AppendStr(defaultImapDirectory, imapServerName);

    MSG_IMAPFolderInfoContainer *rootIMAPfolder = new MSG_IMAPFolderInfoContainer(imapServerName,1, imapHost, mailMaster);
	imapHost->SetHostInfo(rootIMAPfolder);

    if (rootIMAPfolder && parentArray)	// need to figure out where to add imap host. - just before local host
	{
		int i = 0;
		for (; i < parentArray->GetSize(); i++)
		{
			MSG_FolderInfo *folder = parentArray->GetAt(i);
			if (folder && folder->GetFlags() & MSG_FOLDER_FLAG_DIRECTORY && folder->GetType() != FOLDER_IMAPSERVERCONTAINER)	// local mail
				break;
		}
        parentArray->InsertAt(i, rootIMAPfolder);
	}
    
    // start the buildfoldertree recursion from the inbox
    if (rootIMAPfolder && !errorReturnRootContainerOnly)
    {
        // there is a similar loop in MSG_FolderInfoMail::BuildFolderTree
        // I have my own here because I would have to mangle MSG_FolderInfoMail::BuildFolderTree
        // a lot to make it work with the MSG_IMAPFolderInfoContainer type
        
        XP_Dir dir = XP_OpenDir (imapDirectory, xpMailFolder);
        if (dir)
        {
            XP_DirEntryStruct *entry = NULL;
            for (entry = XP_ReadDir(dir); entry; entry = XP_ReadDir(dir))
            {
				// ###phil not nice, but now that ShouldIgnoreFile is in the folderInfoMail,
				// we have no way to ask here, since the IMAP container isn't a folderInfoMail
				if (StaticShouldIgnoreFile(entry->d_name))
					continue;
                
                char *subName = (char*) XP_ALLOC(XP_STRLEN(imapDirectory) + XP_STRLEN(entry->d_name) + 2); // '/' + '\0'
                if (subName)
                {
                    XP_STRCPY (subName, imapDirectory);
                    if (*entry->d_name != '/')
                        XP_STRCAT (subName, "/");
                    XP_STRCAT (subName, entry->d_name);
                    BuildFolderTree (subName, depth + 1, rootIMAPfolder->GetSubFolders(), mailMaster, TRUE, imapHost);
                    FREEIF(subName);
                }
            }
            

			MSG_FolderInfo *foundInbox = NULL;
			MSG_IMAPHostTable *imapHostTable = mailMaster->GetIMAPHostTable();
			XP_ASSERT(imapHostTable);	// we just added one, right?
			MSG_IMAPHost *defaultIMAPHost = imapHostTable->GetDefaultHost();

			// Always make sure there's an IMAP INBOX on the default server, if we're using IMAP.
			// We also always want to create an INBOX if there are no folders for a given host.
			int32 numberOfInboxesFound = rootIMAPfolder->GetFoldersWithFlag(MSG_FOLDER_FLAG_INBOX, &foundInbox, 1);
            int32 numberOfmailboxesFound = rootIMAPfolder->GetNumSubFolders();
			if ((imapHost == defaultIMAPHost) ?	(numberOfInboxesFound == 0) : (numberOfmailboxesFound == 0))
			{
	            char *inboxPath = (char*) XP_ALLOC (XP_STRLEN(imapHost->GetLocalDirectory()) + XP_STRLEN(INBOX_FOLDER_NAME) + 2); // '/' + '\0'
	            if (inboxPath)
	            {
	            	XP_STRCPY(inboxPath, imapDirectory);
	            	XP_STRCAT(inboxPath, "/");
	            	XP_STRCAT(inboxPath, INBOX_FOLDER_NAME);
	            	
					// create the new database, set the online name
					MailDB *newDb = NULL;
					XP_Bool wasCreated=FALSE;
					ImapMailDB::Open(inboxPath, TRUE, &newDb, mailMaster, &wasCreated);
					if (newDb)
					{
						newDb->m_dbFolderInfo->SetMailboxName("INBOX");	// RFC inbox name
	            		MSG_IMAPFolderInfoMail *inboxFolder = new MSG_IMAPFolderInfoMail(inboxPath, mailMaster, 2, MSG_FOLDER_FLAG_MAIL | MSG_FOLDER_FLAG_INBOX, imapHost);
						if (imapHost->GetDefaultOfflineDownload())
							inboxFolder->SetFolderPrefFlags(inboxFolder->GetFolderPrefFlags() | MSG_FOLDER_PREF_OFFLINE);

		                newDb->SetFolderInfoValid(inboxPath,0,0);
						newDb->Close();
		                if (inboxFolder && rootIMAPfolder)
		                {
		                    // inboxFolder->SetIsOnlineVerified(TRUE);
		                    XPPtrArray *rootSubfolderArray = (XPPtrArray *) rootIMAPfolder->GetSubFolders();
		                    rootSubfolderArray->Add(inboxFolder);
		                }
					}
	            }
            }

			XP_CloseDir(dir);
        }
    }

    FREEIF(imapDirectory);
    return rootIMAPfolder;
}

void 
MSG_IMAPFolderInfoMail::MatchPathWithPersonalNamespaceThenSetFlag(const char *path,  
		  const char *personalNameSpace, uint32 folderFlag, const char *onlineName)
{
	if (!path || NET_URL_Type(path) != IMAP_TYPE_URL)
		return;

    char * folderName = NET_ParseURL(path, GET_PATH_PART);

	if (!folderName || !*folderName)
	{
		StrAllocCopy(folderName, "/");
		StrAllocCat(folderName, msg_DefaultFolderName(folderFlag));
	}

	if (folderName && *folderName && personalNameSpace && *personalNameSpace)
	{
	    char *newN = PR_smprintf("/%s%s", personalNameSpace, folderName+1);
        if(newN) 
		{ 
		    XP_FREEIF(folderName); folderName = newN;
		}
	}
    if (folderName && *folderName && XP_STRCMP(folderName+1, onlineName) == 0)
	    SetFlag(folderFlag);
    XP_FREEIF(folderName);
}

void
MSG_IMAPFolderInfoMail::SetPrefFolderFlag()
{
	const char *path = GetPathname();
	const char *personalNamespacePrefix = GetIMAPHost() ?
		GetIMAPHost()->GetDefaultNamespacePrefixOfType(kPersonalNamespace) : (char *)NULL;

	if (!path)
		return;

	char *onlineName =
		CreateOnlineVersionOfLocalPathString(*(m_master->GetPrefs()), path); 
	char *draftsPath = msg_MagicFolderName (m_master->GetPrefs(),
											MSG_FOLDER_FLAG_DRAFTS, NULL);
	char *templatesPath = msg_MagicFolderName (m_master->GetPrefs(),
											   MSG_FOLDER_FLAG_TEMPLATES,
											   NULL); 
	
	ClearFlag(MSG_FOLDER_FLAG_DRAFTS);
	MatchPathWithPersonalNamespaceThenSetFlag(draftsPath, personalNamespacePrefix,
											  MSG_FOLDER_FLAG_DRAFTS, onlineName); 
	XP_FREEIF(draftsPath);

	ClearFlag(MSG_FOLDER_FLAG_TEMPLATES);
	MatchPathWithPersonalNamespaceThenSetFlag(templatesPath, personalNamespacePrefix,
											  MSG_FOLDER_FLAG_TEMPLATES,  onlineName);  
	XP_FREEIF(templatesPath);

	char *imapSentMail = NULL;
	char *imapSentNews = NULL;

	ClearFlag(MSG_FOLDER_FLAG_SENTMAIL);
	PREF_CopyCharPref("mail.imap_sentmail_path", &imapSentMail);
	MatchPathWithPersonalNamespaceThenSetFlag(imapSentMail, personalNamespacePrefix,
											  MSG_FOLDER_FLAG_SENTMAIL,  onlineName); 
	XP_FREEIF(imapSentMail);

	PREF_CopyCharPref("news.imap_sentmail_path", &imapSentNews);
	MatchPathWithPersonalNamespaceThenSetFlag(imapSentNews, personalNamespacePrefix,
											  MSG_FOLDER_FLAG_SENTMAIL,  onlineName); 
	XP_FREEIF(imapSentNews);

	XP_FREEIF(onlineName);
}

MSG_IMAPFolderInfoMail::MSG_IMAPFolderInfoMail(char* pathname,
                                               MSG_Master *mailMaster,
                                               uint8 depth,
                                               uint32 flags,
											   MSG_IMAPHost *host) : 
                            m_DownLoadState(kNoDownloadInProgress),
                            MSG_FolderInfoMail(pathname, mailMaster, depth, flags | MSG_FOLDER_FLAG_IMAPBOX | MSG_FOLDER_FLAG_MAIL)
{
	m_canBeInFolderPane = TRUE;
	m_VerifiedAsOnlineFolder = FALSE;
    m_OnlineHierSeparator = kOnlineHierarchySeparatorUnknown;
    m_OnlineFolderName = NULL; 
	m_runningIMAPUrl = MSG_NotRunning;
	m_requiresCleanup = FALSE;
    m_LoadingInContext = NULL;
    m_TimeStampOfLastList = LL_ZERO;
	m_uploadInfo = NULL;
	m_hasOfflineEvents = FALSE;
	m_host = host;
	m_folderNeedsSubscribing = FALSE;
	m_folderNeedsACLListed = TRUE;
	m_ownerUserName = NULL;
	m_folderACL = NULL;
	m_adminUrl = NULL;
	XP_BZERO(&m_mailboxSpec, sizeof(m_mailboxSpec));
	if (flags & MSG_FOLDER_FLAG_INBOX)	// LATER ON WE WILL ENABLE ANY FOLDER, NOT JUST INBOXES
		MSG_Biff_Master::AddBiffFolder(pathname, host->GetCheckNewMail(),
		host->GetBiffInterval(), TRUE, (char*) host->GetHostName());
}

void MSG_IMAPFolderInfoMail::SetTimeStampOfLastListNow() { m_TimeStampOfLastList = PR_Now(); }


#define kBogusImapFolderName "+=+=+="

MSG_IMAPFolderInfoMail::~MSG_IMAPFolderInfoMail()
{
	MSG_Master *master = NULL;
	TNavigatorImapConnection *imapConnection = NULL;
	
	master = GetMaster();
	if (master && (GetFlags() & MSG_FOLDER_FLAG_INBOX))
	{
		imapConnection = (TNavigatorImapConnection*) master->UnCacheImapConnection(GetHostName(), m_OnlineFolderName);
		MSG_IMAP_KillConnection(imapConnection);	// stop the connection and kill it
	}
	
	// MSG_Biff_Master::RemoveBiffFolder(m_pathName);	// Done in MSG_FolderInfoMail destructor
	if (m_OnlineFolderName && XP_STRCMP(m_OnlineFolderName, kBogusImapFolderName))
    	XP_FREE(m_OnlineFolderName);
	delete m_folderACL;
	FREEIF(m_ownerUserName);
	FREEIF(m_adminUrl);
}

MSG_IMAPFolderInfoMail	*MSG_IMAPFolderInfoMail::GetIMAPFolderInfoMail() 
{
	return this;
}

MsgERR MSG_IMAPFolderInfoMail::OnCloseFolder ()
{
	// Tell the connection cache this folder is closing.
	m_master->ImapFolderClosed(this);
	return eSUCCESS;
}

void MSG_IMAPFolderInfoMail::SetOnlineName(const char *name)
{	
	if (m_OnlineFolderName && XP_STRCMP(m_OnlineFolderName, kBogusImapFolderName))
    	XP_FREE(m_OnlineFolderName);
	m_OnlineFolderName = XP_STRDUP(name);
	
	if (m_OnlineFolderName)
	{
		MailDB *mailDb = NULL;
		XP_Bool wasCreated=FALSE;
		ImapMailDB::Open(m_pathName, TRUE, &mailDb, m_master, &wasCreated);
		if (mailDb)
		{
			mailDb->m_dbFolderInfo->SetMailboxName(m_OnlineFolderName);
			mailDb->Close();

	    	// tell the kids, their online path is different now
			int numberOfChildren = GetNumSubFolders();
			int status = 0;
			for (int childIndex = 0; (childIndex < numberOfChildren) && !status; childIndex++)
			{
				MSG_IMAPFolderInfoMail *currentChild = (MSG_IMAPFolderInfoMail *) GetSubFolder(childIndex);
				status = currentChild->ParentRenamed(m_OnlineFolderName);
			}

		}
	}
}

#define kIMAPFolderCacheFormat "\t%s"
#define kIMAPFolderInfoCacheVersion 2

MsgERR MSG_IMAPFolderInfoMail::WriteToCache (XP_File f)
{
	// This function is coupled tightly with ReadFromCache,
	// and loosely with ReadDBFolderInfo

	XP_FilePrintf (f, "\t%d", kIMAPFolderInfoCacheVersion);
	XP_FilePrintf (f, kIMAPFolderCacheFormat, GetOnlineName());

	return MSG_FolderInfoMail::WriteToCache (f);
}

MsgERR MSG_IMAPFolderInfoMail::ReadFromCache (char *buf)
{
	MsgERR err = eSUCCESS;
	char	*saveBuf = buf;
	int version;
	int tokensRead = sscanf (buf, "\t%d", &version);
	SkipCacheTokens (&buf, tokensRead);

	if (version == 2)
	{
		char *tmpName = (char*) XP_ALLOC(XP_STRLEN(buf));
		if (tmpName)
		{
			char *name = buf + 1; // skip over tab;
			char *secondTab = XP_STRCHR(name, '\t');
			if (secondTab)
			{
				*secondTab = '\0';
				XP_FREEIF(m_OnlineFolderName);
				m_OnlineFolderName = XP_STRDUP(name); // kinda hacky. maybe scan/printf wasn't such a good idea
				*secondTab = '\t';
			}
			SkipCacheTokens (&buf, 1);
			XP_FREE(tmpName);
		}
		else
			err = eOUT_OF_MEMORY;
//		m_HaveReadNameFromDB = TRUE;
	}
	else if (version == 1)
	{
		// this was really a MSG_FolderInfoMail generated line so reset buf 
		// and call base class.
		buf = saveBuf;
	}
	else
		err = eUNKNOWN;

	err = MSG_FolderInfoMail::ReadFromCache (buf);

	return err;
}

MsgERR MSG_IMAPFolderInfoMail::ReadDBFolderInfo (XP_Bool force /* = FALSE */)
{
	MsgERR err = eSUCCESS;

	if (m_master->InitFolderFromCache (this) && m_OnlineFolderName)
		return err;

	if (!m_OnlineFolderName || force)
	{
		MailDB *mailDb = NULL;
		XP_Bool wasCreated=FALSE;
		ImapMailDB::Open(m_pathName, TRUE, &mailDb, m_master, &wasCreated);
		if (mailDb)
		{
			XPStringObj serverPathName;
			mailDb->m_dbFolderInfo->GetMailboxName(serverPathName);
			if (XP_STRLEN(serverPathName))
			{
				// this is a current db that was created with the path name
				// m_name was read from the db by MSG_FolderInfoMail::MSG_FolderInfoMail
				// for imap folders, the name in the db is the full server path
				XP_FREEIF(m_OnlineFolderName);
				m_OnlineFolderName = XP_STRDUP(serverPathName);
				
				// set m_name to the leaf of this.
				if (!XP_STRCASECMP(m_OnlineFolderName,"INBOX"))
				{
					char *imapInboxName = XP_GetString(MK_MSG_IMAP_INBOX_NAME);
					if (imapInboxName && *imapInboxName)
						SetName(imapInboxName);
					else
						SetName("Inbox");
				}
				else
				{
					char *leafSlash = NULL;
					const char *namespacePrefix = m_host->GetNamespacePrefixForFolder(m_OnlineFolderName);
					XP_Bool folderIsNamespace = (namespacePrefix &&
						!XP_STRCMP(namespacePrefix, m_OnlineFolderName));
					if (!folderIsNamespace)
						leafSlash = XP_STRRCHR(m_OnlineFolderName, '/');
					if (leafSlash)
					{
						SetName(++leafSlash);
					}
					else
					{
						SetName(m_OnlineFolderName); // this is the leaf
					}
				}
			}
			else	// this db is probably from before we store the path in it. 
			{		// Take a wild guess.  Works for most platforms.  A failed
					// guess will be deleted when we delete non verified folders
				m_OnlineFolderName = CreateOnlineVersionOfLocalPathString( *m_master->GetPrefs(), m_pathName);
				mailDb->m_dbFolderInfo->SetMailboxName(m_OnlineFolderName);
			}

			// this is probably the first time we opened this db, so while we are here with an open
			// db, preload a few more values that would cause a db open during mailbox discovery
			char hierSeparator = GetOnlineHierarchySeparator();

			// Call base class, but make sure it knows not to get the name again
			m_HaveReadNameFromDB = TRUE;
			err = MSG_FolderInfoMail::ReadDBFolderInfo(force);

			mailDb->Close();
		}
		else
		{
			// can't open the DB?  Well take a wild guess.  Most stuff will be pretty horked but we can't return NULL
			m_OnlineFolderName = CreateOnlineVersionOfLocalPathString( *m_master->GetPrefs(), m_pathName);
		}
	}

	return err;
}


const char* MSG_IMAPFolderInfoMail::GetOnlineName() 
{
	ReadDBFolderInfo();
	
	if (!m_OnlineFolderName)	// memory allocation error?
	{
		MSG_IMAPFolderInfoMail *nonConstThis = (MSG_IMAPFolderInfoMail *) this;
		nonConstThis->m_OnlineFolderName = kBogusImapFolderName;	// invalid imap name
	}
		
	return m_OnlineFolderName;
}

MSG_IMAPHost * MSG_IMAPFolderInfoMail::GetIMAPHost() 
{
	return m_host;
}

MSG_IMAPFolderInfoContainer	* MSG_IMAPFolderInfoMail::GetIMAPContainer() 
{
	return (m_host) ? m_host->GetHostFolderInfo() : (MSG_IMAPFolderInfoContainer *)NULL;
}

const char* MSG_IMAPFolderInfoMail::GetHostName()
{
	return (m_host) ? m_host->GetHostName() : GetMaster()->GetPrefs()->GetPopHost();
}

// make sure the m_OnlineFolderName is pulled from the db and that m_name
// is the leaf of this m_OnlineFolderName
const char* MSG_IMAPFolderInfoMail::GetName()   // Name of this folder (as presented to user).
{
	if (!m_OnlineFolderName)
		GetOnlineName();	// lazy initialize from db
	return m_name;
}

void MSG_IMAPFolderInfoMail::SetIsOnlineVerified(XP_Bool verified)
{
    m_VerifiedAsOnlineFolder = verified;
}

void MSG_IMAPFolderInfoMail::SetHierarchyIsOnlineVerified(XP_Bool verified)
{
	// recursion for the children
	int numberOfChildren = GetNumSubFolders();
	for (int childIndex = 0; childIndex < numberOfChildren; childIndex++)
	{
		MSG_IMAPFolderInfoMail *currentChild = (MSG_IMAPFolderInfoMail *) GetSubFolder(childIndex);
		if (currentChild)
			currentChild->SetHierarchyIsOnlineVerified(verified);
	}
	// this level
	SetIsOnlineVerified(verified);
}

XP_Bool MSG_IMAPFolderInfoMail::GetIsOnlineVerified()
{
    return m_VerifiedAsOnlineFolder;
}

int32 MSG_IMAPFolderInfoMail::GetUnverifiedFolders(MSG_IMAPFolderInfoMail** result,
                                           int32 resultsize) 
{
    int num = 0;
    if (!GetIsOnlineVerified()) {
        if (result && num < resultsize) {
            result[num] = this;
        }
        num++;
    }
    MSG_IMAPFolderInfoMail *folder = NULL;
    for (int i=0; i < m_subFolders->GetSize(); i++) {
        folder = (MSG_IMAPFolderInfoMail*) m_subFolders->GetAt(i);
        num += folder->GetUnverifiedFolders(result + num, resultsize - num);
    }
    return num;
}

void MSG_IMAPFolderInfoMail::SetRunningIMAPUrl(MSG_RunningState runningIMAPUrl) 
{
	m_runningIMAPUrl = runningIMAPUrl;
}

MSG_RunningState MSG_IMAPFolderInfoMail::GetRunningIMAPUrl() 
{
	return m_runningIMAPUrl;
}

void MSG_IMAPFolderInfoMail::SetHasOfflineEvents(XP_Bool hasOfflineEvents) 
{
	m_hasOfflineEvents = hasOfflineEvents; 
}

XP_Bool	MSG_IMAPFolderInfoMail::GetHasOfflineEvents() 
{
	return m_hasOfflineEvents; 
}

// define the message download stream functions, implemented at the bottom of file
unsigned int MessageDownLoadStreamWriteReady(NET_StreamClass *stream);

int  IMAPHandleBlockForDataBaseCreate(NET_StreamClass *stream, const char *str, int32 msgUID);
void IMAPFinishStreamForDataBaseCreate(NET_StreamClass *stream);
void IMAPAbortStreamForDataBaseCreate (NET_StreamClass *stream, int status);

int  IMAPHandleBlockForMessageCopyDownload(NET_StreamClass *stream, const char *str, int32 strlength);
void IMAPFinishStreamForMessageCopyDownload(NET_StreamClass *stream);
void IMAPAbortStreamForMessageCopyDownload(NET_StreamClass *stream, int status);

int MSG_IMAPFolderInfoMail::CreateMessageWriteStream(msg_move_state *state, uint32 msgSize, uint32 msg_flags)
{
	imapMessageFlagsType imapFlags = kNoImapMsgFlag;
	
	if (msg_flags & MSG_FLAG_READ)
		imapFlags |= kImapMsgSeenFlag;
	if (msg_flags & MSG_FLAG_REPLIED)
		imapFlags |= kImapMsgAnsweredFlag;
	if (msg_flags & MSG_FLAG_MARKED)
		imapFlags |= kImapMsgFlaggedFlag;

    // graph the progress for the user
    state->totalSizeForProgressGraph = msgSize;
    FE_GraphProgressInit(state->sourcePane->GetContext(),
                         NULL,
                         state->totalSizeForProgressGraph);

    // remember these if we have to upload this later.
	state->msgSize = msgSize;
	state->msg_flags = msg_flags;
    // upload the size for this message
	if (state->imap_connection)
	{
		IMAP_UploadAppendMessageSize(state->imap_connection, msgSize,imapFlags); 
		state->haveUploadedMessageSize = TRUE;
	}
	else
	{
		XP_Trace(" no connection to append size to - what to do?\n");
	}
    
    // wait until the connection tells us which socket to write to
    state->uploadToIMAPsocket = NULL;
    

    return 0;
}

XP_Bool MSG_IMAPFolderInfoMail::MessageWriteStreamIsReadyForWrite(msg_move_state *state)
{
    return state->uploadToIMAPsocket != 0;
}


uint32   MSG_IMAPFolderInfoMail::SeekToEndOfMessageStore(msg_move_state * /*state*/)
{return 0;} // we don't seek for online folder



uint32   MSG_IMAPFolderInfoMail::WriteMessageBuffer(msg_move_state *state, char *buffer, uint32 bytesToWrite, MailMessageHdr *offlineHdr)
{
	uint32 bytesWritten = 0;
	if (offlineHdr && state->destDB)
		bytesWritten = offlineHdr->AddToOfflineMessage(buffer, bytesToWrite, state->destDB->GetDB());
	else
    	bytesWritten = NET_BlockingWrite(state->uploadToIMAPsocket, buffer, bytesToWrite);

	return bytesWritten; // success
}


void   MSG_IMAPFolderInfoMail::CloseMessageWriteStream(msg_move_state *state)
{
    // see if this will notify the end of a message
    NET_BlockingWrite(state->uploadToIMAPsocket, CRLF, 2);
    /*	wrong context
    FE_GraphProgressDestroy (GetFolderPaneContext(), 
                             NULL,
                             state->totalSizeForProgressGraph,
                             state->messageBytesMovedSoFar);    
    */
    // imap was waiting for a specific number of bytes.  Once they got there, we were done.
    (state->uploadCompleteFunction) (state->uploadCompleteArgument);
}

void   MSG_IMAPFolderInfoMail::SaveMessageHeader(msg_move_state * /*state*/, MailMessageHdr * /*hdr*/, uint32 /*messageKey*/, MessageDB * /*srcDB = NULL*/)
{}  // we don't know the message key until our next full sync with the mailbox



class IMAPdownLoadStreamData {
public:
	IMAPdownLoadStreamData();
	~IMAPdownLoadStreamData();

	void Clear();

    MWContext *urlContext;
    uint32    msgSize;
	ParseOutgoingMessage outgoingParser;
	char	*obuffer;
	int32	obufferSize;
	int32	obufferIndex;
	char	*m_mozillaStatus;
};

IMAPdownLoadStreamData::IMAPdownLoadStreamData()
{
	Clear();
}

IMAPdownLoadStreamData::~IMAPdownLoadStreamData()
{
	XP_FREEIF(m_mozillaStatus);
}


void IMAPdownLoadStreamData::Clear()
{
	urlContext = NULL;
	msgSize = 0;
	obuffer = NULL;
	obufferSize = 0;
	obufferIndex = 0;
	m_mozillaStatus = NULL;
}

NET_StreamClass *MSG_IMAPFolderInfoMail::CreateDownloadMessageStream(ImapActiveEntry *ce,
                                                                     uint32 size, const char *content_type,
																	 XP_Bool content_modified)
{
	MSG_Pane *urlPane = IMAP_GetActiveEntryPane(ce);
	OfflineImapGoOnlineState *offlineImapGoOnlineState = (urlPane) ? urlPane->GetGoOnlineState() : (OfflineImapGoOnlineState *)NULL;
	DownloadOfflineImapState *downloadOfflineImapState = (offlineImapGoOnlineState) 
		? offlineImapGoOnlineState->GetDownloadOfflineImapState() : (DownloadOfflineImapState *)NULL;

    NET_StreamClass *returnStream = (NET_StreamClass *) XP_ALLOC(sizeof(NET_StreamClass));
    IMAPdownLoadStreamData *imapStreamData = new IMAPdownLoadStreamData;
    
    if (returnStream && imapStreamData) // all streams share some characteristics
    {
        imapStreamData->urlContext   = urlPane->GetContext();
        imapStreamData->msgSize      = size;
		imapStreamData->obuffer		 = NULL;
		imapStreamData->obufferSize	 = 0;
		imapStreamData->obufferIndex = 0;
        
        returnStream->window_id      = urlPane->GetContext();
        returnStream->data_object    = imapStreamData;
        returnStream->is_write_ready = MessageDownLoadStreamWriteReady;
        returnStream->is_multipart   = FALSE;
    }
    
    switch (m_DownLoadState) {
        case kDownLoadingAllMessageHeaders:
            if (returnStream)
            {
                returnStream->name      = XP_STRDUP("kDownLoadingAllMessageHeaders");
                returnStream->put_block = IMAPHandleBlockForDataBaseCreate;
                returnStream->complete  = IMAPFinishStreamForDataBaseCreate;
                returnStream->abort     = IMAPAbortStreamForDataBaseCreate;
            }
            break;
        case kDownLoadMessageForCopy:
            if (returnStream)
            {
                returnStream->name      = XP_STRDUP("kDownLoadMessageForCopy");
                returnStream->put_block = IMAPHandleBlockForMessageCopyDownload;
                returnStream->complete  = IMAPFinishStreamForMessageCopyDownload;
                returnStream->abort     = IMAPAbortStreamForMessageCopyDownload;
            }
            break;
        case kDownLoadMessageForOfflineDB:
            if (returnStream)
            {
            	delete imapStreamData;
            	returnStream->data_object = NULL;	// set differently by CreateOfflineImapStream
            										// so lets not leak.
            										
                returnStream->name      = XP_STRDUP("kDownLoadMessageForOfflineDB");
                DownloadOfflineImapState::CreateOfflineImapStream(returnStream, this, NULL, downloadOfflineImapState);
				if (!downloadOfflineImapState && offlineImapGoOnlineState)
				{
					DownloadOfflineImapState *downloadState = (DownloadOfflineImapState *) returnStream->data_object;
					offlineImapGoOnlineState->SetDownloadOfflineImapState(downloadState);
					downloadState->SetDeleteOnComplete(FALSE);
				}
            }
            break;
        default:
            {
            	delete imapStreamData;
            	
            	XP_Bool captureOffline = (GetFolderPrefFlags() & MSG_FOLDER_PREF_OFFLINE) != 0;
            	
		        NET_StreamClass *displayStream = IMAP_CreateDisplayStream(ce,captureOffline, size, content_type);
		        
		        // If this is an offline imap folder, don't pass up the chance to cache this message.
		        // Note that we would not be here if there already was a body
				// Don't do this if the message body has been modified (parts left out)
		        if (captureOffline && !content_modified)
                	DownloadOfflineImapState::CreateOfflineImapStream(returnStream, this, displayStream);
                else
                {
                	// this is a pure display stream.  mkimap4.cpp in libnet will expect currentIMAPfolder==NULL in this case
		        	urlPane->GetContext()->currentIMAPfolder = NULL;    
                	XP_FREE(returnStream);
                	returnStream = displayStream;
                }
            }
    }
    return returnStream;
}

#define ServerBugWorkAroundString "Status: 0"

uint32 MSG_IMAPFolderInfoMail::WriteBlockToImapServer(DBMessageHdr *msgHeader, //move this header
														msg_move_state *state,	// track it in this state
														MailMessageHdr *offlineHdr, // NON-NULL means offline append
														XP_Bool actuallySendToServer)
{
    // copy state->size or one message
	uint32 messageLength = msgHeader->GetByteLength();
	uint32 bytesLeft = messageLength - state->messageBytesMovedSoFar;
	uint32 nRead=0;
	uint32 nOfBytesWritten=0;
	
    XP_Bool lastCharacterOfPrevBufferWasCR = FALSE;
    
    do {    // if the last char of this buffer is CR, then do another read/write
            // we don't want to store that tidbit of info for the next visit
        uint32 maxBytesItsSafeToConvert = state->size/2;
        uint32 numberOfBytesToRead = bytesLeft > maxBytesItsSafeToConvert ? maxBytesItsSafeToConvert : bytesLeft;
        nRead = XP_FileRead (state->buf, numberOfBytesToRead, state->infid);
        XP_ASSERT(nRead == numberOfBytesToRead);
        char *currentWriteableBuffer = state->buf;
        if (state->messageBytesMovedSoFar == 0) // skip the first line.  It's the Berkely envelope
        {
        	// there might be one or more newlines before the envelope, so eat chars until we get the envelope 
        	char *endOfLine = FindNextLineTerminatingCharacter(currentWriteableBuffer);
        	while (!msg_IsEnvelopeLine(currentWriteableBuffer, (endOfLine - currentWriteableBuffer) + 1))
        	{
        		currentWriteableBuffer = endOfLine;
	            if (*currentWriteableBuffer && (*++currentWriteableBuffer == LF))   // skip the line terminator, any kind
	                currentWriteableBuffer++;
	            endOfLine = FindNextLineTerminatingCharacter(currentWriteableBuffer);
        	}
        	
        	// ok, so now currentWriteableBuffer is at the envelope, endOfLine is at the next eol char
        	// advance currentWriteableBuffer to leave room only for ServerBugWorkAroundString
        	currentWriteableBuffer += (endOfLine - currentWriteableBuffer) - XP_STRLEN(ServerBugWorkAroundString);
        	
        	// replace the envelope with the mail server bug work around
        	char eolChar = *endOfLine;
        	XP_STRCPY(currentWriteableBuffer, ServerBugWorkAroundString);
        	*endOfLine = eolChar;	// replace NULL left by XP_STRCPY
        	
            currentWriteableBuffer = FindNextLineTerminatingCharacter(currentWriteableBuffer);
            if (*currentWriteableBuffer && (*++currentWriteableBuffer == LF))   // skip the line terminator, any kind
                currentWriteableBuffer++;
        }
        uint32 nOfBytesToWrite = ConvertBufferToRFC822LineTermination(currentWriteableBuffer, 
                                                                      nRead - (currentWriteableBuffer - state->buf),
                                                                      &lastCharacterOfPrevBufferWasCR);
        if (actuallySendToServer)
        	nOfBytesWritten += WriteMessageBuffer(state, currentWriteableBuffer, nOfBytesToWrite, offlineHdr);
        else
        	nOfBytesWritten += nOfBytesToWrite;
        state->messageBytesMovedSoFar += nRead;
        bytesLeft -= nRead;
    } while (lastCharacterOfPrevBufferWasCR && (state->messageBytesMovedSoFar < messageLength));
   
   	return nOfBytesWritten;
}


uint32 MSG_IMAPFolderInfoMail::MessageCopySize(DBMessageHdr *msgHeader, msg_move_state *state)
{
	// seek to the beginning of the message
    XP_FileSeek (state->infid, 
				msgHeader->GetMessageOffset(),
				SEEK_SET);


	uint32 nOfBytesWritten = 0;
	state->messageBytesMovedSoFar = 0;
	while (state->messageBytesMovedSoFar < msgHeader->GetByteLength())
		nOfBytesWritten += WriteBlockToImapServer(msgHeader, state, NULL, FALSE);	// count only, no writing!
	
	state->messageBytesMovedSoFar = 0;	// we actually did not write anything
	return nOfBytesWritten;
}



MsgERR MSG_IMAPFolderInfoMail::OfflineImapMsgToLocalFolder(MailMessageHdr *sourceHeader, MSG_FolderInfoMail *destFolder, MessageDB *sourceDB, msg_move_state *state)
{
	MsgERR stopit = 0;
	
	if (sourceHeader->GetOfflineMessageLength(state->destDB->GetDB()))
	{
		XP_File writeFid = XP_FileOpen (destFolder->GetPathname(), xpMailFolder, XP_FILE_APPEND_BIN);
		if (writeFid)
		{
    		XP_StatStruct destFolderStat;
    		uint32 newMsgPosition = 0;
		    if (!XP_Stat(destFolder->GetPathname(), &destFolderStat, xpMailFolder))
		        newMsgPosition = destFolderStat.st_size;

			uint32 offlineMsgSize = sourceHeader->WriteOfflineMessage(writeFid, sourceDB->GetDB());
			XP_FileClose( writeFid );
			
			if (offlineMsgSize == 0)
			{
				// cruds. truncate back to where we started the file 
				XP_FileTruncate(destFolder->GetPathname(), xpMailFolder, newMsgPosition);
				stopit = MK_MSG_ERROR_WRITING_MAIL_FOLDER;
			}
			
			if (!stopit)
			{
				MailMessageHdr *newMailHdr = new MailMessageHdr;
				if (newMailHdr)
				{
					newMailHdr->CopyFromMsgHdr(sourceHeader, sourceDB->GetDB(), state->destDB->GetDB());
					newMailHdr->PurgeOfflineMessage(state->destDB->GetDB());
					newMailHdr->SetMessageKey(newMsgPosition);
					newMailHdr->SetMessageSize(offlineMsgSize);
					
					stopit = state->destDB->AddHdrToDB(newMailHdr, NULL, TRUE);
			        MailDB::SetFolderInfoValid(destFolder->GetPathname(), 0, 0);
			        destFolder->SummaryChanged();
				}
				else
					stopit = MK_MSG_ERROR_WRITING_MAIL_FOLDER;

				MailDB *sourceMailDB = sourceDB->GetMailDB();
				if (state->ismove && sourceMailDB)
				{
					// the only playback action needed to to delete the source message
					DBOfflineImapOperation	*op = sourceMailDB->GetOfflineOpForKey(sourceHeader->GetMessageKey(), TRUE);
					if (op)
					{
						op->SetImapFlagOperation(op->GetNewMessageFlags() | kImapMsgDeletedFlag);
						delete op;
					}
					else
						stopit = MK_MSG_ERROR_WRITING_MAIL_FOLDER;

				}
			}
		}
		else
			stopit = MK_MSG_ERROR_WRITING_MAIL_FOLDER;
	}
	else
		stopit = MK_MSG_ONLINE_IMAP_WITH_NO_BODY;
	
	return stopit;
}


DBOfflineImapOperation *MSG_IMAPFolderInfoMail::GetClearedOriginalOp(DBOfflineImapOperation *op, MailDB **originalDB)
{
	DBOfflineImapOperation *returnOp = NULL;
	XP_ASSERT(op->GetOperationFlags() & kMoveResult);
	
	XPStringObj originalBoxName;
	MessageKey originalKey;
	op->GetSourceInfo(originalBoxName, originalKey);
	
	MSG_FolderInfoMail *sourceFolder = GetMaster()->FindImapMailFolder(originalBoxName);
	if (!sourceFolder)
		sourceFolder = GetMaster()->FindMailFolder(originalBoxName,FALSE);
	if (sourceFolder)
	{
		if (sourceFolder->GetFlags() & MSG_FOLDER_FLAG_IMAPBOX)
		{
			XP_Bool wasCreated=FALSE;
			ImapMailDB::Open(sourceFolder->GetPathname(), TRUE, originalDB, GetMaster(), &wasCreated);
		}
		else
			MailDB::Open(sourceFolder->GetPathname(), TRUE, originalDB);
		
		if (*originalDB)
		{
			returnOp = (*originalDB)->GetOfflineOpForKey(originalKey, FALSE);
			if (returnOp)
			{
				XPStringObj moveDestination;
				returnOp->GetMoveDestination((*originalDB)->GetDB(), moveDestination);
				if (!XP_STRCMP(moveDestination, this->GetOnlineName()))
					returnOp->ClearMoveOperation((*originalDB)->GetDB());
			}
		}
	}
	
	return returnOp;
}

DBOfflineImapOperation *MSG_IMAPFolderInfoMail::GetOriginalOp(DBOfflineImapOperation *op, MailDB **originalDB)
{
	DBOfflineImapOperation *returnOp = NULL;
	
	XPStringObj originalBoxName;
	MessageKey originalKey;
	op->GetSourceInfo(originalBoxName, originalKey);
	
	MSG_FolderInfoMail *sourceFolder = GetMaster()->FindImapMailFolder(originalBoxName);
	if (!sourceFolder)
		sourceFolder = GetMaster()->FindMailFolder(originalBoxName,FALSE);
	if (sourceFolder)
	{
		if (sourceFolder->GetFlags() & MSG_FOLDER_FLAG_IMAPBOX)
		{
			XP_Bool wasCreated=FALSE;
			ImapMailDB::Open(sourceFolder->GetPathname(), TRUE, originalDB, GetMaster(), &wasCreated);
		}
		else
			MailDB::Open(sourceFolder->GetPathname(), TRUE, originalDB);
		
		if (*originalDB)
		{
			returnOp = (*originalDB)->GetOfflineOpForKey(originalKey, FALSE);
		}
	}
	
	return returnOp;
}

MsgERR MSG_IMAPFolderInfoMail::BeginOfflineCopy (MSG_FolderInfo *dstFolder, 
									  MessageDB *sourceDB,
                                      IDArray *srcArray, 
                                      int32 srcCount,
                                      msg_move_state *state)
{
	MsgERR stopit = 0;
	XP_Bool operationOnMoveResult = FALSE;
	MailDB *sourceMailDB = sourceDB ? sourceDB->GetMailDB() : 0;
	MSG_IMAPFolderInfoMail *dstImapFolder = dstFolder->GetIMAPFolderInfoMail();
	XP_Bool deleteToTrash = FALSE;
	if (GetMaster() && GetMaster()->GetPrefs())
		deleteToTrash = DeleteIsMoveToTrash();
	
	if (sourceMailDB)
	{
		UndoManager *undoManager = NULL;

		if (state && state->sourcePane)
			undoManager = state->sourcePane->GetUndoManager();

		XP_Bool shouldUndoOffline = undoManager && NET_IsOffline();
		if (shouldUndoOffline)
			undoManager->StartBatch();
		// save the future ops in the source DB, if this is not a imap->local copy/move
		
		if (state->destDB && !stopit) // offline delete
		{
			// get the highest key in the dest db, so we can make up our fake keys
			XP_Bool highWaterDeleted = FALSE;
			XP_Bool haveWarnedUserAboutMoveTargets = FALSE;
			MessageKey fakeBase = 1;
			fakeBase += state->destDB->ListHighwaterMark();
			
			// put fake message in destination db, delete source if move
			for (int sourceKeyIndex=0; !stopit && (sourceKeyIndex < srcCount); sourceKeyIndex++)
			{
				XP_Bool	messageReturningHome = FALSE;
				char *originalBoxName = XP_STRDUP(GetOnlineName());
				MessageKey originalKey = srcArray->GetAt(sourceKeyIndex);
			
				if (dstFolder->GetFlags() & MSG_FOLDER_FLAG_IMAPBOX)
				{
					DBOfflineImapOperation	*sourceOp = sourceMailDB->GetOfflineOpForKey(originalKey, TRUE);
					if (sourceOp)
					{
						MailDB *originalDB = NULL;
						if (sourceOp->GetOperationFlags() == kMoveResult) // offline move
						{
							// gracious me, we are moving something we already moved while offline!
							// find the original operation and clear it!
							DBOfflineImapOperation *originalOp = GetClearedOriginalOp(sourceOp, &originalDB);
							if (originalOp)
							{
								XPStringObj originalString;
								sourceOp->GetSourceInfo(originalString,originalKey);
								FREEIF(originalBoxName);
								originalBoxName = XP_STRDUP(originalString);
								originalKey = sourceOp->GetMessageKey();
								
								if (state->ismove)
									sourceMailDB->DeleteOfflineOp(sourceOp->GetMessageKey());
								delete sourceOp;
								
								sourceOp = originalOp;
								if (dstImapFolder && !XP_STRCMP(originalBoxName, dstImapFolder->GetOnlineName()))
								{
									messageReturningHome = TRUE;
									originalDB->DeleteOfflineOp(originalOp->GetMessageKey());
								}
							}
						}
						
						if (!messageReturningHome)
						{
							const char *destinationPath = ((MSG_IMAPFolderInfoMail *) dstFolder)->GetOnlineName();
							int32 opType = kMsgCopy;
								
							if (state->ismove)
							{
								sourceOp->SetMessageMoveOperation(destinationPath); // offline move
								opType = kMsgMoved;
							}
							else
								sourceOp->AddMessageCopyOperation(destinationPath); // offline copy
							if (shouldUndoOffline && undoManager->GetState() == UndoIdle)
							{	// only need undo if we're really offline and not pseudo offline
								OfflineIMAPUndoAction *undoAction = new 
										OfflineIMAPUndoAction(state->sourcePane, (MSG_FolderInfo*) this, sourceOp->GetMessageKey(), opType,
										sourceMailDB->GetFolderInfo(), dstFolder, 0, NULL, 0);
								if (undoAction)
									undoManager->AddUndoAction(undoAction);
							}
							delete sourceOp;
						}
						if (originalDB)
							originalDB->Close();
					}
					else
						stopit = MK_MSG_ERROR_WRITING_MAIL_FOLDER;
					
				}

				MailMessageHdr *mailHdr = sourceMailDB->GetMailHdrForKey(originalKey);
				if (mailHdr)
				{
					XP_Bool successfulCopy = FALSE;
					highWaterDeleted = !highWaterDeleted && state->ismove && deleteToTrash &&
										(mailHdr->GetMessageKey() == sourceMailDB->m_dbFolderInfo->m_LastMessageUID);
					
					if (dstFolder->GetFlags() & MSG_FOLDER_FLAG_IMAPBOX)
					{
						MailMessageHdr *newMailHdr = new MailMessageHdr;
						if (newMailHdr)
						{
							newMailHdr->CopyFromMsgHdr(mailHdr, sourceMailDB->GetDB(), state->destDB->GetDB());
							newMailHdr->SetMessageKey(fakeBase + sourceKeyIndex);
							stopit = state->destDB->AddHdrToDB(newMailHdr, NULL, TRUE);
						}
						else
							stopit = MK_MSG_ERROR_WRITING_MAIL_FOLDER;
						
						if (!stopit)
						{
							DBOfflineImapOperation	*destOp = state->destDB->GetOfflineOpForKey(fakeBase + sourceKeyIndex, TRUE);
							if (destOp)
							{
								// check if this is a move back to the original mailbox, in which case
								// we just delete the offline operation.
								if (messageReturningHome)
								{
									state->destDB->DeleteOfflineOp(destOp->GetMessageKey());
								}
								else
								{
									state->destDB->SetSourceMailbox(destOp, originalBoxName, originalKey);
									if (shouldUndoOffline && undoManager->GetState() == UndoIdle)
									{
										OfflineIMAPUndoAction *undoAction = new 
											OfflineIMAPUndoAction(state->sourcePane, (MSG_FolderInfo*) this, destOp->GetMessageKey(), kAddedHeader,
												state->destDB->GetFolderInfo(), dstFolder, 0, newMailHdr);
										if (undoAction)
											undoManager->AddUndoAction(undoAction);
									}
								}
								delete destOp;
							}
							else
								stopit = MK_MSG_ERROR_WRITING_MAIL_FOLDER;
						}
						successfulCopy = stopit == 0;
					}
					else
					{
						MSG_FolderInfoMail *mailDest = dstFolder->GetMailFolderInfo();
						if (mailDest)
						{
							stopit = OfflineImapMsgToLocalFolder(mailHdr, mailDest, sourceMailDB, state);
							successfulCopy = (stopit == 0);
						}
					}
					
					
					if (state->ismove && successfulCopy)	
					{
						int32 opType = kDeletedMsg;
						if (!deleteToTrash)
							opType = kMsgMarkedDeleted;
						if (shouldUndoOffline && undoManager->GetState() == UndoIdle) {
							OfflineIMAPUndoAction *undoAction = new 
								OfflineIMAPUndoAction(state->sourcePane, (MSG_FolderInfo*) this, mailHdr->GetMessageKey(), opType,
										sourceMailDB->GetFolderInfo(), dstFolder, 0, mailHdr);
							if (undoAction)
								undoManager->AddUndoAction(undoAction);
						}
						if (deleteToTrash)
							sourceMailDB->DeleteMessage(mailHdr->GetMessageKey(), NULL, FALSE);
						else
							sourceMailDB->MarkImapDeleted(mailHdr->GetMessageKey(),TRUE,NULL); // offline delete
					}
						
					delete mailHdr;
					
					if (!successfulCopy)
						highWaterDeleted = FALSE;
				}
				FREEIF(originalBoxName);
			}
			
			
            if (sourceMailDB->m_dbFolderInfo->GetNumVisibleMessages() && highWaterDeleted)
            {
                ListContext *listContext = NULL;
                DBMessageHdr *highHdr = NULL;
                if (sourceMailDB->ListLast(&listContext, &highHdr) == eSUCCESS)
                {   
                    sourceMailDB->m_dbFolderInfo->m_LastMessageUID = highHdr->GetMessageKey();
                    delete highHdr;
//                    sourceMailDB->m_dbFolderInfo->setDirty(); DMB TODO
                    sourceMailDB->ListDone(listContext);
                }
            }
            
            state->destDB->Commit();
        	SummaryChanged();
        	dstFolder->SummaryChanged();
        	
		}

		// load the next message in the message pane if needed
		if (state->nextKeyToLoad && (state->sourcePane->GetPaneType() == MSG_MESSAGEPANE))
		{
			MSG_MessagePane *messagePane = (MSG_MessagePane *) state->sourcePane;
			messagePane->LoadMessage(this, state->nextKeyToLoad, NULL, TRUE);
		}
		if (shouldUndoOffline)
			undoManager->EndBatch();
	}
	
	return stopit ? stopit : -1;
}


MsgERR MSG_IMAPFolderInfoMail::BeginCopyingMessages (MSG_FolderInfo *dstFolder, 
									  MessageDB *sourceDB,
                                      IDArray *srcArray, 
                                      MSG_UrlQueue *urlQueue,
                                      int32 srcCount,
                                      MessageCopyInfo *copyInfo)
{
    MsgERR returnError = CheckForLegalCopy(dstFolder);

	msg_move_state *state = &copyInfo->moveState;

    if (returnError != 0)
        return returnError; 
    
    state->destfolder = dstFolder;
        
    if (NET_IsOffline() || (!urlQueue && ShouldPerformOperationOffline() && state->destDB))
    	return BeginOfflineCopy(dstFolder, sourceDB, srcArray, srcCount, state);
    
    // build a list of message UIDs
    char *idString = AllocateImapUidString(*srcArray);

    char *copyURL = NULL;
    state->sourcePane->GetContext()->msgCopyInfo->imapOnLineCopyState = kInProgress;
    
    MSG_IMAPFolderInfoMail *onlineDestinationFolder = dstFolder->GetIMAPFolderInfoMail();

    if (FOLDER_MAIL == dstFolder->GetType())        // online to offline
    {
    	// will be set in MSG_StartImapMessageToOfflineFolderDownload
        // m_DownLoadState = kDownLoadMessageForCopy;

        state->sourcePane->GetContext()->mailMaster             = m_master;
		state->sourcePane->GetContext()->currentIMAPfolder      = this;

        if (idString)
        {
            copyURL = CreateImapOnToOfflineCopyUrl(GetHostName(), 
                                                   GetOnlineName(),
                                                   GetOnlineHierarchySeparator(),
                                                   idString,
                                                   "OFFLINEFOLDER",
                                                   TRUE,    // ids are uids
                                                   state->ismove);
        }
    }
	// check if we're copying within the same host
    else if (onlineDestinationFolder && onlineDestinationFolder->GetIMAPHost() == GetIMAPHost())
    {
        
        if (idString)
        {
            copyURL = CreateImapOnlineCopyUrl(GetHostName(), 
                                              GetOnlineName(),
                                              GetOnlineHierarchySeparator(),
                                              idString,
                                              onlineDestinationFolder->GetOnlineName(),
                                              onlineDestinationFolder->GetOnlineHierarchySeparator(),
                                              TRUE, // ids are uids
                                              state->ismove);
        }
    }
	// copying across hosts
	else if (onlineDestinationFolder)
	{
        IDArray keysToSave;
        for (int32 i = 0; i < srcCount; i++)
            keysToSave.Add (srcArray->GetAt(i));
	    FREEIF(idString);
		return DownloadToTempFileAndUpload(copyInfo, keysToSave, dstFolder, sourceDB);

	}
	MSG_UrlQueue::AddUrlToPane(copyURL, PostMessageCopyUrlExitFunc, state->sourcePane);
    FREEIF(copyURL);
    
    FREEIF(idString);

    return returnError;
}


XP_Bool 
MSG_IMAPFolderInfoMail::UndoMoveCopyMessagesHelper(MSG_Pane *urlPane, 
												   const IDArray &keysToUndo, 
												   UndoObject *undoObject)
{
	UndoManager *undoManager = urlPane->GetUndoManager();
	URL_Struct *url_struct = NULL;
	
	if (urlPane->GetContext() == GetFolderLoadingContext())
	{
		FE_Alert(urlPane->GetContext(), XP_GetString(MK_MSG_NO_UNDO_DURING_IMAP_FOLDER_LOAD));
		return FALSE;
	}

	if (undoManager->GetState() == UndoUndoing) {
		StartUpdateOfNewlySelectedFolder(urlPane, FALSE, NULL, &keysToUndo, TRUE);
	}
	else if (undoManager->GetState() == UndoRedoing) {
		StartUpdateOfNewlySelectedFolder(urlPane, FALSE, NULL, &keysToUndo, FALSE);
	}

	if (url_struct) {
		url_struct->msg_pane = urlPane;
		undoObject->UndoURLHook(url_struct);
		urlPane->GetURL(url_struct, FALSE);
		return TRUE;
	}

	return FALSE;
}


void	   MSG_IMAPFolderInfoMail::NotifyFolderLoaded(MSG_Pane *urlPane, XP_Bool /*loadWasInterrupted*/ /* default false */)
{
	// if we were loading in urlPane context, then clear the loading context
	if (urlPane)
		m_LoadingInContext = NULL;
		
	// MSG_PaneNotifyFolderLoaded now handled in the ImapFolderSelectCompleteExitFunction
}


int
MSG_IMAPFolderInfoMail::FinishCopyingMessages(MWContext *currentContext,
											  MSG_FolderInfo *srcFolder, 
											  MSG_FolderInfo * dstFolder, 
											  MessageDB* /*srcDB*/,
											  IDArray	*srcArray, 
											  int32 srcCount,
											  msg_move_state *state)
{
	int returnErr = 0;
    
	if (!dstFolder->TestSemaphore(this))
	{
		returnErr = dstFolder->AcquireSemaphore(this);
     	if (returnErr)
     	{
 	        FE_Alert (currentContext, XP_GetString(returnErr));
 	        return MK_MSG_FOLDER_BUSY;
     	}
	}
 
	MessageDBView * view;
	// added to handle search as view...
	if (state->sourcePane && state->sourcePane->GetPaneType() == MSG_SEARCHPANE)
		view = ((MSG_SearchPane *) state->sourcePane)->GetView(srcFolder);
    else
		view = (state->sourcePane) ? state->sourcePane->GetMsgView() : 0;

    // during debugging I realized that the state can change from 
    // kInProgress -> kSuccessfulCopy -> kSuccessfulDelete before this
    // function gets called.  Code written to process interim states
    // may not get called - km
    
    switch (state->sourcePane->GetContext()->msgCopyInfo->imapOnLineCopyState) {
        case kInProgress:
            returnErr = 0;          // tell ProcessMailbox to keep it coming..
            break;
        case kSuccessfulCopy:
            if (!state->ismove)     // This is the final state for copy
            {
                state->moveCompleted = TRUE;
                
                // The download worked, update the destination panes
                if (dstFolder->GetType() == FOLDER_MAIL)
                    ((MSG_FolderInfoMail *) dstFolder)->CloseOfflineDestinationDbAfterMove(state);
                // else imap, handled by url exit function
                returnErr = MK_CONNECTED;
                UpdatePendingCounts(dstFolder, srcArray);
            }
			// search as view modification...inform search pane, that we are done with this view
//			if (state->sourcePane && state->sourcePane->GetPaneType() == MSG_SEARCHPANE)
//				((MSG_SearchPane *) state->sourcePane)->CloseView(srcFolder);
            
            break;
        case kFailedCopy:
            returnErr = MK_MSG_ONLINE_COPY_FAILED;
            break;
        case kSuccessfulDelete:
			{
			XP_Bool showDeletedMessages = ShowDeletedMessages();
			
			// if we filtering, assume missing are unread, otherwise read.
			// don't diddle counts if playing back offline ops because the counts have already been diddled.
			if (!m_master->GetPlayingBackOfflineOps())	
				UpdatePendingCounts(dstFolder, srcArray, TRUE, !state->sourcePane->GetActiveImapFiltering());

            state->moveCompleted = TRUE;
            // The download worked, update the destination panes
            if (dstFolder->GetType() == FOLDER_MAIL)
                ((MSG_FolderInfoMail *) dstFolder)->CloseOfflineDestinationDbAfterMove(state);
                // else imap, handled by url exit function


			if (!showDeletedMessages)
			{
	            // Run the delete loop forwards 
	            // so the indices will be right for DeleteMessagesByIndex
	            MSG_ViewIndex doomedIndex = 0; 
	            XP_Bool highWaterUIDwasDeleted = FALSE;
	            MailDB *affectedDB = NULL;
	            
	            if (view)
	            {
	            	affectedDB = (MailDB *) view->GetDB();
	            	
					IDArray	viewIndices;
					int total = srcArray->GetSize();	// Check for bug 113085 rb
					XP_ASSERT(srcCount == total);
	                for (int32 i = 0; i < total; i++)
	                {
	                    // should we stop on error?
	                    MessageKey doomedKey = srcArray->GetAt(i);
	                    doomedIndex = view->FindViewIndex(doomedKey);
	                    if (doomedIndex != MSG_VIEWINDEXNONE)
	                    {
		                    viewIndices.Add(doomedIndex);
		                    
		                    // Tell the FE we're deleting this message in case they want to close
		                    // any open message windows.
		                    if (MSG_THREADPANE == state->sourcePane->GetPaneType())
		                        FE_PaneChanged (state->sourcePane, TRUE, MSG_PaneNotifyMessageDeleted, (uint32) doomedKey);
	                    }
	                    
	                    if ((doomedKey == affectedDB->m_dbFolderInfo->m_LastMessageUID) && !highWaterUIDwasDeleted)
	                        highWaterUIDwasDeleted = TRUE;
	                }
	                if (viewIndices.GetSize() != 0)
	                	returnErr = view->DeleteMessagesByIndex (viewIndices.GetData(), viewIndices.GetSize(), TRUE /* delete from db */);
	                else
	                	returnErr = 0;
	            }
	            else
	            {
	            	// we are here because we did a get new mail from somewhere besides the inbox thread pane
	            	// and some new messages were filtered to other folders
					XP_Bool wasCreated=FALSE;
					ImapMailDB::Open(GetPathname(), FALSE, &affectedDB, GetMaster(), &wasCreated);
					if (affectedDB)
					{
						IDArray	messageKeys;
						messageKeys.SetSize(srcCount);
						for (int32 msgIndex=0; msgIndex < srcCount; msgIndex++)
						{
	                    	MessageKey doomedKey = srcArray->GetAt(msgIndex);
							messageKeys.SetAt(msgIndex, doomedKey);
							
		                    if ((doomedKey == affectedDB->m_dbFolderInfo->m_LastMessageUID) && !highWaterUIDwasDeleted)
		                        highWaterUIDwasDeleted = TRUE;
						}
							
						returnErr = affectedDB->DeleteMessages(messageKeys, NULL);
					}
					// else, how could we fail to open this db, oh well at least we don't mess with it
	            }
	                
	            if (highWaterUIDwasDeleted && affectedDB)
	            {
	                // our sync info with the state of the server mailbox is stale
	                // grab the highest message keyleft, if there is one
	                
	                // also, check for 0 messages here
	                
	                if (affectedDB->m_dbFolderInfo->GetNumVisibleMessages())
	                {
	                    ListContext *listContext = NULL;
	                    DBMessageHdr *highHdr = NULL;
	                    if (affectedDB->ListLast(&listContext, &highHdr) == eSUCCESS)
	                    {   
	                        affectedDB->m_dbFolderInfo->m_LastMessageUID = highHdr->GetMessageKey();
	                        delete highHdr;
// DMB TODO	                        affectedDB->m_dbFolderInfo->setDirty();
	                        affectedDB->ListDone(listContext);
	                    }
	                }
	            }
	            // if I opened the db, I should close it
	            if (!view && affectedDB)
	            {
	            	affectedDB->Close();
	            	affectedDB = NULL;
	            }
				if (view)
					SetUrlForNextMessageLoad(state,view,doomedIndex);
	        }
	        else
	        {
			    DBFolderInfo *folderInfo = NULL;
			    MessageDB     *messagedb = NULL;
	        	MsgERR opendbErr = GetDBFolderInfoAndDB(&folderInfo, &messagedb);
	        	if (opendbErr == 0)
    			{
    				MailDB *mailDB = messagedb->GetMailDB();
			    	if (mailDB)
			    		SetIMAPDeletedFlag(mailDB, *srcArray);
			    	messagedb->Close();
			    	messagedb = NULL;
			    }
	        }
                
            srcFolder->SummaryChanged();
            

			// Begin Undo hook
			{
				UndoManager *undoManager = state->sourcePane->GetUndoManager();
				if (undoManager && undoManager->GetState() == UndoIdle) {
					IMAPMoveCopyMessagesUndoAction *undoAction = new 
						IMAPMoveCopyMessagesUndoAction(state->sourcePane,
													   srcFolder,
													   dstFolder,
													   state->ismove,
													   srcArray->GetAt(0),
													   state->nextKeyToLoad);
					if (undoAction) {
						int32 i;
						for (i=0; i < srcCount; i++) 
							undoAction->AddKey(srcArray->GetAt(i));
						undoManager->AddUndoAction(undoAction);
					}
				}
			}
			// End Undo hook
            
            delete srcArray;   // shouldn't this be deleted everywhere???
			
			// search as view modification...inform search pane, that we are done with this view
//			if (state->sourcePane && state->sourcePane->GetPaneType() == MSG_SEARCHPANE)
//				((MSG_SearchPane *) state->sourcePane)->CloseView(srcFolder);
			
            returnErr = MK_CONNECTED;   // add code to delete from view
			}	
            break;
        case kFailedDelete:
            // The download worked, update the destination panes
            if (dstFolder->GetType() == FOLDER_MAIL)
                dstFolder->SummaryChanged();
                // else imap, handled by url exit function

            returnErr = MK_MSG_ONLINE_MOVE_FAILED;   
            break;
    }


    return returnErr; 
}


void MSG_IMAPFolderInfoMail::StoreImapFlags(MSG_Pane *paneForFlagUrl, imapMessageFlagsType flags, XP_Bool addFlags, 
											const IDArray &keysToFlag, MSG_UrlQueue	*urlQueue /* = NULL */)
{
	URL_Struct *url = NULL;
	
	// fire off the url to do this marking
	if (urlQueue || !NET_IsOffline() /* ShouldPerformOperationOffline() */)
	{
		// If we are not offline, we want to add the flag changes to the url queue. Otherwise our
		// changes are reflected only after offline ops are executed. The screen wont match the
		// flag states if they get new mail. Bug #108636
		char *idString = MSG_IMAPFolderInfoMail::AllocateImapUidString(keysToFlag);
		char *urlString;
		
		if (addFlags)
			urlString = CreateImapAddMessageFlagsUrl(GetHostName(),
													 GetOnlineName(),
													 GetOnlineHierarchySeparator(),
													 idString,
													 flags,
													 TRUE);
		else
			urlString = CreateImapSubtractMessageFlagsUrl(GetHostName(),
													 GetOnlineName(),
													 GetOnlineHierarchySeparator(),
													 idString,
													 flags,
													 TRUE);

		url = NET_CreateURLStruct (urlString, NET_DONT_RELOAD);
		url->internal_url = TRUE;
		url->msg_pane = paneForFlagUrl;
		MSG_UrlQueue::AddUrlToPane(url, NULL, paneForFlagUrl, TRUE);

		FREEIF(urlString);
		FREEIF( idString );
	}
	else
	{
		MailDB *mailDb = NULL; // change flags offline
		XP_Bool wasCreated=FALSE;

		ImapMailDB::Open(GetPathname(), TRUE, &mailDb, GetMaster(), &wasCreated);
		if (mailDb)
		{
			UndoManager *undoManager = NULL;
			MSG_Pane *fPane = mailDb->GetMaster()->FindFirstPaneOfType(MSG_FOLDERPANE);
			uint32 total = keysToFlag.GetSize();

			if (fPane)
				undoManager = fPane->GetUndoManager();

			for (int keyIndex=0; keyIndex < total; keyIndex++)
			{
				DBOfflineImapOperation	*op = mailDb->GetOfflineOpForKey(keysToFlag[keyIndex], TRUE);
				if (op)
				{
					MailDB *originalDB = NULL;
					if (op->GetOperationFlags() & kMoveResult)
					{
						// get the op in the source db and change the flags there
						DBOfflineImapOperation	*originalOp = GetOriginalOp(op, &originalDB);
						if (originalOp)
						{
							if (undoManager && undoManager->GetState() == UndoIdle && NET_IsOffline()) {
								OfflineIMAPUndoAction *undoAction = new 
										OfflineIMAPUndoAction(paneForFlagUrl, (MSG_FolderInfo*) this, op->GetMessageKey(), kFlagsChanged,
										this, NULL, flags, NULL, addFlags);
								if (undoAction)
									undoManager->AddUndoAction(undoAction);
							}
							delete op;
							op = originalOp;
						}
					}
					
					if (addFlags)
						op->SetImapFlagOperation(op->GetNewMessageFlags() | flags);
					else
						op->SetImapFlagOperation(op->GetNewMessageFlags() & ~flags);
					delete op;

					if (originalDB)
					{
						originalDB->Close();
						originalDB = NULL;
					}
				}
			}
			mailDb->Close();
			mailDb = NULL;
		}
	}
}



MsgERR MSG_IMAPFolderInfoMail::DeleteAllMessages(MSG_Pane *paneForDeleteUrl, XP_Bool deleteSubFoldersToo /* = TRUE */)	// implements empty trash
{
	MsgERR preFlightError = 0;
	

	if (NET_IsOffline())
	{
		MailDB *mailDB = NULL;
		XP_Bool wasCreated=FALSE;
		ImapMailDB::Open(GetPathname(), TRUE, &mailDB, GetMaster(), &wasCreated);
		if (mailDB)
		{
			MessageKey fakeId = mailDB->GetUnusedFakeId();
			DBOfflineImapOperation *op = mailDB->GetOfflineOpForKey(fakeId, TRUE);
			if (op)
			{
				op->SetDeleteAllMsgs();
				delete op;
			}
		}
		URL_Struct *url_struct = NET_CreateURLStruct("mailbox:?null", NET_SUPER_RELOAD);
		if (url_struct)
		{
			url_struct->msg_pane = paneForDeleteUrl;
		    url_struct->pre_exit_fn = PostEmptyImapTrashExitFunc;
		    paneForDeleteUrl->GetURL(url_struct, FALSE);  
		}
		return 0;
	}

	// this url will delete the subfolders as well.  Preflight this with the user.
	if (deleteSubFoldersToo && HasSubFolders())
	{
		MSG_FolderIterator iterator(this);
		MSG_FolderInfo *currentFolder = iterator.First();
		while (currentFolder && eSUCCESS == preFlightError)
		{
			if (currentFolder != this) 
				preFlightError = paneForDeleteUrl->PreflightDeleteFolder(currentFolder, TRUE /*getUserConfirmation*/);
			currentFolder = iterator.Next();
		}
	}
	
	if (eSUCCESS == preFlightError)
	{
		char *trashUrl = CreateImapDeleteAllMessagesUrl(GetHostName(), 
		                                                GetOnlineName(),
		                                                GetOnlineHierarchySeparator());
		if (trashUrl)
		{
			MSG_UrlQueue::AddUrlToPane(trashUrl, PostEmptyImapTrashExitFunc, paneForDeleteUrl);
		    XP_FREE(trashUrl);
		}
    }
	return preFlightError;
}

void MSG_IMAPFolderInfoMail::DeleteSpecifiedMessages(MSG_Pane *paneForDeleteUrl,
    												 const IDArray &keysToDelete,
    												 MessageKey loadKeyOnExit)
{
	// see if we should "undelete" a selection of deleted messages
	XP_Bool doUndelete = FALSE;
	if (loadKeyOnExit == MSG_MESSAGEKEYNONE)
	{
		MailDB *newDb = NULL;
		XP_Bool wasCreated=FALSE;
		ImapMailDB::Open(GetPathname(), TRUE, &newDb, GetMaster(), &wasCreated);
		if (newDb)
		{
			doUndelete = newDb->AllMessageKeysImapDeleted(keysToDelete);
        	if (doUndelete)
        		SetIMAPDeletedFlag(newDb,keysToDelete,FALSE);
			newDb->Close();
		}
	}


	// build the index string
    char *idString = AllocateImapUidString(keysToDelete);

	XP_Bool shouldRunInBackground = ShouldPerformOperationOffline();
    if (doUndelete || shouldRunInBackground)
    {
    	StoreImapFlags(paneForDeleteUrl, kImapMsgDeletedFlag, !doUndelete, keysToDelete);
    	if (ShouldPerformOperationOffline() && !doUndelete)
    	{
    		MessagesWereDeleted(paneForDeleteUrl, FALSE, idString);
    		if ((loadKeyOnExit != MSG_MESSAGEKEYNONE) && (paneForDeleteUrl->GetPaneType() == MSG_MESSAGEPANE))
    		{
    			// do loading of next message while offline
    			((MSG_MessagePane *) paneForDeleteUrl)->LoadMessage(this, loadKeyOnExit, NULL, TRUE);
    		}
    	}
    }
    else
    {
	    // do the url boogie
	    char *deleteUrl = CreateImapDeleteMessageUrl(GetHostName(), 
	                                                 GetOnlineName(),
	                                                 GetOnlineHierarchySeparator(),
	                                                 idString,
	                                                 TRUE);	// ids are uids
	    if (deleteUrl)
	    {
	        URL_Struct *url_struct = NET_CreateURLStruct(deleteUrl, NET_SUPER_RELOAD);
	        if (url_struct)
	        {
	        	if ((loadKeyOnExit != MSG_MESSAGEKEYNONE) && (paneForDeleteUrl->GetPaneType() == MSG_MESSAGEPANE))
	        	{
	        		// url exit function will load next message
	        		((MSG_MessagePane *) paneForDeleteUrl)->SetPostDeleteLoadKey(loadKeyOnExit);
	        		url_struct->pre_exit_fn = MSG_MessagePane::PostDeleteLoadExitFunction;
	        	}
	            paneForDeleteUrl->GetURL(url_struct, FALSE);  
	        } 
	        XP_FREE(deleteUrl);
	    }
    }
    FREEIF(idString);
}

	// MSG_FolderInfoMail::Adopt does to much.  We already know the
	// newFolder pathname and parent sbd exist, so use this during
	// imap folder discovery
void MSG_IMAPFolderInfoMail::LightWeightAdopt(MSG_IMAPFolderInfoMail *newChild)
{
	GetSubFolders()->Add (newChild);
//	GetSubFolders()->InsertAt (0, newChild);
//	GetSubFolders()->QuickSort (CompareFolders);
}


// Try and see if we have any mail pending, if Biff does not run it is because either
// we have not yet connected to the server or because we are busy talking to it, in
// which case we don't need to run biff here since the server will talk to us anyway.
// Called if the user has selected not to get the mail headers automatically when new
// mail arrives

void MSG_IMAPFolderInfoMail::Biff(MWContext *biffContext)
{
	MailDB	*folderDB;
	XP_Bool wasCreated;
	uint32 highWaterUid = 0;
	
	if (!NET_IsOffline()) {
		if (ImapMailDB::Open(GetPathname(), FALSE, &folderDB, GetMaster(), &wasCreated) == eSUCCESS)
		{
			highWaterUid = folderDB->m_dbFolderInfo->m_LastMessageUID;
			char *biffUrl = CreateImapBiffUrl(GetHostName(), 
											  GetOnlineName(),
											  GetOnlineHierarchySeparator(),
											  highWaterUid);
			if (biffUrl)
			{
	    		URL_Struct *url_struct = NET_CreateURLStruct(biffUrl, NET_SUPER_RELOAD);
	    		if (url_struct)
	    		{
					biffContext->imapURLPane = NULL;	// might have a bogus url pane, so clear it.
	    			biffContext->mailMaster = GetMaster();// so the biff will use the cached connection
					// we know biff isn't running in a pane
	    			msg_GetURL(biffContext, url_struct, FALSE);
	    		}
	    		XP_FREE(biffUrl);
			}
       		folderDB->Close();
		}
	}
}



// Check to see if there is new mail waiting for us at the server, and then see if we should
// read the headers or not.

XP_Bool MSG_IMAPFolderInfoMail::MailCheck(MWContext *biffContext)
{
	MSG_Master *master;
	XP_Bool newMail = FALSE;

	if (NET_IsOffline())
		return FALSE;
	master = GetMaster();
	if (master && (GetFlags() & MSG_FOLDER_FLAG_IMAPBOX))
	{
		TNavigatorImapConnection *imapConnection = NULL;

		imapConnection = (TNavigatorImapConnection*) master->UnCacheImapConnection(GetHostName(), m_OnlineFolderName);
		if (!imapConnection) {
			biffContext->mailMaster = master;
			Biff(biffContext);	// start a connection
		}
		if (imapConnection)
		{
			if (!biffContext->mailMaster)
				biffContext->mailMaster = master;
			if (IMAP_CheckNewMail(imapConnection))	// if new mail, and they want headers we get in here
			{
				newMail = TRUE;
				master->TryToCacheImapConnection(imapConnection, GetHostName(), m_OnlineFolderName); // put back for biff to use
				// we should really find a pane that's not busy, if possible. Or create a progress pane!
				MSG_Pane *pane = master->FindFirstPaneOfType(MSG_FOLDERPANE);
				if (!pane)
					pane = master->FindFirstPaneOfType(MSG_THREADPANE);
				if (!pane)
					pane = master->FindFirstPaneOfType(MSG_MESSAGEPANE);
				if (pane /* && !XP_IsContextBusy(pane->GetContext()) */)
					StartUpdateOfNewlySelectedFolder(pane, FALSE, NULL, NULL, FALSE, FALSE);
			} else {
				if (IMAP_NewMailDetected(imapConnection))
					newMail = TRUE;
				master->TryToCacheImapConnection(imapConnection, GetHostName(), m_OnlineFolderName); // put back for biff to use
				Biff(biffContext);		// just check for new mail and get flags
			}
		}
	}
	return newMail;
}



void  MSG_IMAPFolderInfoMail::MessagesWereDeleted(MSG_Pane *urlPane,
												  XP_Bool deleteAllMsgs, 
													const char *doomedKeyString)
{
	XP_Bool showDeletedMessages = ShowDeletedMessages();

	if (deleteAllMsgs)
	{
		TDBFolderInfoTransfer *originalInfo = NULL;
		MailDB	*folderDB;
		XP_Bool wasCreated;
		if (ImapMailDB::Open(GetPathname(), FALSE, &folderDB, GetMaster(), &wasCreated) == eSUCCESS)
		{
			originalInfo = new TDBFolderInfoTransfer(*folderDB->m_dbFolderInfo);
			folderDB->ForceClosed();
		}
			
		// Remove summary file.
		XP_FileRemove(GetPathname(), xpMailFolderSummary);
		
		// Create a new summary file, update the folder message counts, and
		// Close the summary file db.
		if (ImapMailDB::Open(GetPathname(), TRUE, &folderDB, GetMaster(), &wasCreated) == eSUCCESS)
		{
			if (originalInfo)
			{
				originalInfo->TransferFolderInfo(*folderDB->m_dbFolderInfo);
				delete originalInfo;
			}
			SummaryChanged();
			folderDB->Close();
		}

		// Reload any thread pane because it's invalid now.
		MSG_ThreadPane* threadPane = NULL;
		if (m_master != NULL)	
			threadPane = m_master->FindThreadPaneNamed(GetPathname());
		if (threadPane != NULL)
			threadPane->ReloadFolder();
		return;
	}

	char *keyTokenString = XP_STRDUP(doomedKeyString);
	IDArray affectedMessages;
	ParseUidString(keyTokenString, affectedMessages);

	if (doomedKeyString && !showDeletedMessages)
	{
		MessageDBView *trashView = urlPane->GetMsgView();

		if (trashView && keyTokenString)
		{
			IDArray viewIndexArray;

			for (uint32 i = 0; i < affectedMessages.GetSize(); i++)
				viewIndexArray.Add(trashView->FindViewIndex(affectedMessages.GetAt(i)));

			trashView->DeleteMessagesByIndex (viewIndexArray.GetData(), viewIndexArray.GetSize(), TRUE /* delete from db */);
			SummaryChanged();
		}
		
	}
	else if (doomedKeyString)	// && !imapDeleteIsMoveToTrash
	{
		MailDB	*folderDB;
		XP_Bool wasCreated;
		if (ImapMailDB::Open(GetPathname(), FALSE, &folderDB, GetMaster(), &wasCreated) == eSUCCESS)
		{
			SetIMAPDeletedFlag(folderDB, affectedMessages);
			folderDB->Close();
		}
	}
	FREEIF(keyTokenString);
}


// store MSG_FLAG_IMAP_DELETED in the specified mailhdr records
void MSG_IMAPFolderInfoMail::SetIMAPDeletedFlag(MailDB *mailDB, const IDArray &msgids, XP_Bool markDeleted)
{
	MsgERR markStatus = 0;
	uint32 total = msgids.GetSize();

	for (int index=0; !markStatus && (index < total); index++)
	{
		markStatus = mailDB->MarkImapDeleted(msgids[index], markDeleted, NULL);
	}
}

void MSG_IMAPFolderInfoMail::OpenThreadView(MailDB *mailDB,mailbox_spec *adoptedBoxSpec)
{   
    // update all of the database flags
    if (adoptedBoxSpec && adoptedBoxSpec->folderSelected && adoptedBoxSpec->flagState)
    {
        int numberOfFlags = IMAP_GetFlagStateNumberOfMessages(adoptedBoxSpec->flagState);
        for (int flagIndex=0; flagIndex < numberOfFlags; flagIndex++)
        {
            imap_uid currentUID        = IMAP_GetUidOfMessage(flagIndex, adoptedBoxSpec->flagState);
            imapMessageFlagsType serverflags = IMAP_GetMessageFlags(flagIndex, adoptedBoxSpec->flagState);

			MailMessageHdr	*mailHdr = mailDB->GetMailHdrForKey(currentUID);
			if (mailHdr)
			{
				// noop calls that set db flags can cause a view invalidation and excessive
				uint32 dbFlags = mailHdr->GetFlags();
				
				if ( ((dbFlags & MSG_FLAG_READ) == 0) != ((serverflags & kImapMsgSeenFlag) == 0))
					mailDB->MarkRead(   currentUID, (serverflags & kImapMsgSeenFlag) != 0);
					
				if ( ((dbFlags & MSG_FLAG_REPLIED) == 0) != ((serverflags & kImapMsgAnsweredFlag) == 0))
					mailDB->MarkReplied(   currentUID, (serverflags & kImapMsgAnsweredFlag) != 0);
					
				if ( ((dbFlags & MSG_FLAG_MARKED) == 0) != ((serverflags & kImapMsgFlaggedFlag) == 0))
					mailDB->MarkMarked(   currentUID, (serverflags & kImapMsgFlaggedFlag) != 0);
				
				if ( ((dbFlags & MSG_FLAG_IMAP_DELETED) == 0) != ((serverflags & kImapMsgDeletedFlag) == 0))
					mailDB->MarkImapDeleted(currentUID, (serverflags & kImapMsgDeletedFlag) != 0, NULL);
				
				delete mailHdr;
           	}
        }
    }

    // find the thread pane in the master
    MSG_ThreadPane *threadPane = m_master->FindThreadPaneNamed(GetPathname());
    MessageDBView  *threadView = NULL;
        
    // reinitialize the db view of the thread pane, if it has not been opened yet
    if (threadPane)
        threadView = threadPane->GetMsgView();
    
    if (threadPane && threadView && !threadView->GetDB())
    {
        uint32 pCount=0;
        threadPane->StartingUpdate(MSG_NotifyAll, 0, 0);
        threadView->Open(mailDB, threadView->GetViewType() , &pCount);
        threadPane->EndingUpdate(MSG_NotifyAll, 0, 0);
    }
    else                    // if there is no thread pane then we should close the db we would
        mailDB->Close();    // have passed to its view.
}



// hierarchy separating character on the server
char MSG_IMAPFolderInfoMail::GetOnlineHierarchySeparator()
{
	if (m_OnlineHierSeparator == kOnlineHierarchySeparatorUnknown)
	{
	    MailDB *mailDB=NULL;
	    XP_Bool     dbWasCreated=FALSE;
	    
	    MsgERR dbStatus = ImapMailDB::Open(GetPathname(),
	                                     TRUE, // create if necessary
	                                     &mailDB,
	                                     m_master,
	                                     &dbWasCreated);
	    
	    if (dbStatus == eSUCCESS)
	    {
	    	m_OnlineHierSeparator = (char) mailDB->m_dbFolderInfo->GetIMAPHierarchySeparator();
	    	mailDB->Close();
	    }
	}
	
	return m_OnlineHierSeparator ? m_OnlineHierSeparator : kOnlineHierarchySeparatorUnknown;
}

void MSG_IMAPFolderInfoMail::SetOnlineHierarchySeparator(char separator)
{
	if (separator != m_OnlineHierSeparator)
	{
	    MailDB *mailDB=NULL;
	    XP_Bool     dbWasCreated=FALSE;
	    	    
	    MsgERR dbStatus = ImapMailDB::Open(GetPathname(),
	                                     TRUE, // create if necessary
	                                     &mailDB,
	                                     m_master,
	                                     &dbWasCreated);
	    
	    if (dbStatus == eSUCCESS)
	    {
	    	if (m_OnlineHierSeparator == kOnlineHierarchySeparatorUnknown)
				m_OnlineHierSeparator = (char) mailDB->m_dbFolderInfo->GetIMAPHierarchySeparator();
			if (separator != m_OnlineHierSeparator)
			{
				m_OnlineHierSeparator = separator;
	    		mailDB->m_dbFolderInfo->SetIMAPHierarchySeparator(m_OnlineHierSeparator);
	    	}
	    		
	    	mailDB->Close();
	    }
	}
}



// both of these algorithms assume that key arrays and flag states are sorted by increasing key.
void MSG_IMAPFolderInfoMail::FindKeysToDelete(const IDArray &existingKeys, IDArray &keysToDelete, TImapFlagAndUidState *flagState)
{
	XP_Bool imapDeleteIsMoveToTrash = DeleteIsMoveToTrash();
	uint32 total = existingKeys.GetSize();
	uint32 index;

	int onlineIndex=0; // current index into flagState
	for (uint32 keyIndex=0; keyIndex < total; keyIndex++)
	{
		index = IMAP_GetFlagStateNumberOfMessages(flagState);
		while ((onlineIndex < index) && 
			   (existingKeys[keyIndex] > IMAP_GetUidOfMessage(onlineIndex, flagState)) )
		{
			onlineIndex++;
		}
		
		// delete this key if it is not there or marked deleted
		if ( (onlineIndex >= IMAP_GetFlagStateNumberOfMessages(flagState) ) ||
			 (existingKeys[keyIndex] != IMAP_GetUidOfMessage(onlineIndex, flagState)) ||
			 ((IMAP_GetMessageFlags(onlineIndex, flagState) & kImapMsgDeletedFlag) && imapDeleteIsMoveToTrash) )
		{
			MessageKey doomedKey = existingKeys[keyIndex];
			if ((int32) doomedKey < 0 && doomedKey != kIdNone && doomedKey != kIdPending)
				continue;
			else
				keysToDelete.Add(existingKeys[keyIndex]);
		}
		
		if (existingKeys[keyIndex] == IMAP_GetUidOfMessage(onlineIndex, flagState))
			onlineIndex++;
	}
}

void MSG_IMAPFolderInfoMail::FindKeysToAdd(const IDArray &existingKeys, IDArray &keysToFetch, TImapFlagAndUidState *flagState)
{
	XP_Bool showDeletedMessages = ShowDeletedMessages();

	int dbIndex=0; // current index into existingKeys
	uint32 existTotal, numberOfKnownKeys;
	uint32 index;
	
	existTotal = numberOfKnownKeys = existingKeys.GetSize();
	index = IMAP_GetFlagStateNumberOfMessages(flagState);
	for (uint32 flagIndex=0; flagIndex < index; flagIndex++)
	{
		while ( (flagIndex < numberOfKnownKeys) && (dbIndex < existTotal) &&
				(existingKeys[dbIndex] < IMAP_GetUidOfMessage(flagIndex, flagState)) )
			dbIndex++;
		
		if ( (flagIndex >= numberOfKnownKeys)  || 
			 (dbIndex >= existTotal) ||
			 (existingKeys[dbIndex] != IMAP_GetUidOfMessage(flagIndex , flagState) ) )
		{
			numberOfKnownKeys++;
			if (showDeletedMessages || ! (IMAP_GetMessageFlags(flagIndex, flagState) & kImapMsgDeletedFlag))
			{
				keysToFetch.Add(IMAP_GetUidOfMessage(flagIndex, flagState));
			}
		}
	}
}

void MSG_IMAPFolderInfoMail::UpdateFolderStatus(mailbox_spec *adoptedBoxSpec, MSG_Pane * /*url_pane*/)
{
    MailDB *mailDB=NULL;
    XP_Bool     dbWasCreated=FALSE;
    
    MsgERR dbStatus = ImapMailDB::Open(GetPathname(),
                                     TRUE, // create if necessary
                                     &mailDB,
                                     m_master,
                                     &dbWasCreated);
   
	if (dbStatus == eSUCCESS)
	{
		DBFolderInfo *groupInfo = mailDB->m_dbFolderInfo;
		// diddle the imap pending counts to reflect the accurate counts according to the server
		int32 totalMessages = groupInfo->GetNumMessages() + groupInfo->GetImapTotalPendingMessages();
		int32 totalUnreadMessages = groupInfo->GetNumNewMessages() + groupInfo->GetImapUnreadPendingMessages();
		int32 totalDelta = adoptedBoxSpec->number_of_messages - totalMessages;
		int32 numNewDelta = adoptedBoxSpec->number_of_unseen_messages - totalUnreadMessages;
   		groupInfo->ChangeImapTotalPendingMessages(totalDelta - groupInfo->GetImapTotalPendingMessages());
   		groupInfo->ChangeImapUnreadPendingMessages(numNewDelta - groupInfo->GetImapUnreadPendingMessages());
		SummaryChanged();
		mailDB->Close();
	}
}

void MSG_IMAPFolderInfoMail::UpdateNewlySelectedFolder(mailbox_spec *adoptedBoxSpec, MSG_Pane *url_pane)
{
    MailDB *mailDB=NULL;
    XP_Bool     dbWasCreated=FALSE;
    
	MSG_Biff_Master::MailCheckEnable((char*) GetPathname(), (XP_Bool) ~NET_IsOffline());	// enable or disable start mail checking if allowed

    MsgERR dbStatus = ImapMailDB::Open(GetPathname(),
                                     TRUE, // create if necessary
                                     &mailDB,
                                     m_master,
                                     &dbWasCreated);
   
	// remember the info in the maibox_spec. This is currently only used
	// for cacheless view to know how big to make the view, but there
	// could be other uses. It might be better to try to get hold
	// of this from the connection object in the libnet imap thread
	// instead of a potentially stale copy of the data.
	m_mailboxSpec = *adoptedBoxSpec;
    if ((dbStatus == eSUCCESS) && adoptedBoxSpec->folderSelected)
    {
    	IDArray existingKeys;
    	IDArray keysToDelete;
    	IDArray keysToFetch;


    	mailDB->ListAllIds(existingKeys);
    	if (mailDB->ListAllOfflineDeletes(existingKeys) > 0)
			existingKeys.QuickSort();
    	if ((mailDB->m_dbFolderInfo->GetImapUidValidity() != adoptedBoxSpec->folder_UIDVALIDITY)	&&	// if UIDVALIDITY Changed 
    		!NET_IsOffline())
    	{
											// store the new UIDVALIDITY value
    		mailDB->m_dbFolderInfo->SetImapUidValidity(adoptedBoxSpec->folder_UIDVALIDITY);
    										// delete all my msgs, the keys are bogus now
											// add every message in this folder
			keysToDelete.CopyArray(&existingKeys);

			if (adoptedBoxSpec->flagState)
			{
				IDArray no_existingKeys;
    			FindKeysToAdd(no_existingKeys, keysToFetch, adoptedBoxSpec->flagState);
    		}
    	}		
    	else if (!adoptedBoxSpec->flagState && !NET_IsOffline())	// if there are no messages on the server
    	{
			keysToDelete.CopyArray(&existingKeys);
    	}
    	else if (!NET_IsOffline())
    	{
    		FindKeysToDelete(existingKeys, keysToDelete, adoptedBoxSpec->flagState);

			// if this is the result of an expunge then don't grab headers
			if (!(adoptedBoxSpec->box_flags & kJustExpunged))
				FindKeysToAdd(existingKeys, keysToFetch, adoptedBoxSpec->flagState);
    	}
    	
    	
    	if (keysToDelete.GetSize())
    	{
			uint32 total;

    		XP_Bool highWaterDeleted = FALSE;
            url_pane->StartingUpdate(MSG_NotifyNone, 0, 0);
    		mailDB->DeleteMessages(keysToDelete,NULL);
			url_pane->EndingUpdate(MSG_NotifyNone, 0, 0);
			total = keysToDelete.GetSize();

    		for (uint32 deleteKeyIndex=0; !highWaterDeleted && deleteKeyIndex < total; deleteKeyIndex++)
    		{
    			highWaterDeleted = (keysToDelete[deleteKeyIndex] == mailDB->m_dbFolderInfo->m_LastMessageUID);
    		}
    		
    		if (highWaterDeleted)
    		{
				if (mailDB->m_dbFolderInfo->GetNumVisibleMessages())
				{
					ListContext *listContext = NULL;
					DBMessageHdr *currentHdr = NULL;
					if ((mailDB->ListLast(&listContext, &currentHdr) == eSUCCESS) &&
						currentHdr)
					{
						mailDB->m_dbFolderInfo->m_LastMessageUID = currentHdr->GetMessageKey();
						delete currentHdr;
						mailDB->ListDone(listContext);
					}
					else
						mailDB->m_dbFolderInfo->m_LastMessageUID = 0;
				}
				else
					mailDB->m_dbFolderInfo->m_LastMessageUID = 0;
//		    	mailDB->m_dbFolderInfo->setDirty();	DMB TODO
    		}
    		
			SummaryChanged();
    	}
    	
		// if this is the INBOX, tell the stand-alone biff about the new high water mark
		if (GetFlags() & MSG_FOLDER_FLAG_INBOX)
		{
			UpdateStandAloneIMAPBiff(keysToFetch);
		}

    	if (keysToFetch.GetSize())
    	{
			MSG_SetBiffStateAndUpdateFE(MSG_BIFF_NewMail);
			// Ending update called in NotifyFetchAnyNeededBodies 
            url_pane->StartingUpdate(MSG_NotifyNone, 0, 0);

            OpenThreadView(mailDB,adoptedBoxSpec);
            mailDB = NULL;  // storage adopted by OpenThreadView            

            AddToSummaryMailDB(keysToFetch, url_pane, adoptedBoxSpec);
    	}
    	else if (url_pane) // somehow xfe gets here during biff(!) with no url_pane
    	{
			// Ending update called in NotifyFetchAnyNeededBodies 
            url_pane->StartingUpdate(MSG_NotifyNone, 0, 0);
            // let the imap libnet module know that we don't need headers
            IMAP_DoNotDownLoadAnyMessageHeadersForMailboxSelect(adoptedBoxSpec->connection);
			// wait until we can get body id monitor before continuing.
			IMAP_BodyIdMonitor(adoptedBoxSpec->connection, TRUE);
			// I think the real fix for this is to seperate the header ids from body id's.
			NotifyFetchAnyNeededBodies(url_pane, adoptedBoxSpec->connection, mailDB);
			IMAP_BodyIdMonitor(adoptedBoxSpec->connection, FALSE);
            
            OpenThreadView(mailDB,adoptedBoxSpec);
            mailDB = NULL;  // storage adopted by OpenThreadView
	    	if (GetGettingMail())
			{
				MSG_Pane *msgPane = url_pane;
				MSG_Pane* parentpane = MSG_GetParentPane(url_pane);
				if (parentpane) 
					msgPane = parentpane;
	    		NET_Progress(msgPane->GetContext(), XP_GetString(MK_POP3_NO_MESSAGES));
			}
    	}
    }


    if (dbStatus != eSUCCESS)

    {
        if (XP_FileRemove(GetPathname(), xpMailFolderSummary) == 0)
			UpdateNewlySelectedFolder(adoptedBoxSpec, url_pane);
		else
		{
			// This DB is a read only file?  We are hosed here.
			// Let the user cancel this select.  If we interrupt here
			// we could re-enter NET_InterruptIMAP4.
		}
    }
    else
    {
		if (!adoptedBoxSpec->folderSelected)
		{
	    	// we got here because the select url failed!  Open what we've got
	    	NotifyFolderLoaded(url_pane, TRUE);	// TRUE was "interrupted"
    	    OpenThreadView(mailDB,adoptedBoxSpec);
		}	
    	IMAP_FreeBoxSpec(adoptedBoxSpec);
    }
	SetGettingMail(FALSE);
}


XP_Bool MSG_IMAPFolderInfoMail::StaticShouldIgnoreFile(const char *fileName)
{
	XP_Bool ignoreIt = TRUE;
	// for imap we ignore everything that is not a summary mail file
	// or a directory
	
	// check for directory
	// cannot use XP_OpenDir for the check because sometimes fileName is a leaf name
	// XP_OpenDir expects a full path. So, fall back on ".sbd" convention
	if (XP_STRLEN(fileName) > XP_STRLEN(".sbd"))
		ignoreIt = 0 != XP_STRCASECMP(fileName + XP_STRLEN(fileName) - XP_STRLEN(".sbd"), ".sbd");
	
	if (ignoreIt)
	{
		// platform specific checks for summary mail files
#if defined (XP_WIN) || defined (XP_MAC) || defined (XP_OS2)
		if (XP_STRLEN(fileName) > XP_STRLEN(".snm"))
			ignoreIt = 0 != XP_STRCASECMP(fileName + XP_STRLEN(fileName) - XP_STRLEN(".snm"), ".snm");
#endif

#if defined (XP_UNIX)
		if (XP_STRLEN(fileName) > XP_STRLEN(".summary"))
			ignoreIt = (0 != XP_STRCASECMP(fileName + XP_STRLEN(fileName) - XP_STRLEN(".summary"), ".summary")) || (*fileName != '.');
#endif
	}
	
	return ignoreIt;
}


XP_Bool MSG_IMAPFolderInfoMail::ShouldIgnoreFile (const char *name)
{
	return StaticShouldIgnoreFile(name);
}
        
char *MSG_IMAPFolderInfoMail::CreateMailboxNameFromDbName(const char *dbName)
{
    char *mailboxName = (char*) XP_ALLOC(XP_STRLEN(dbName) + 1);
    if (mailboxName) 
    {
	XP_STRCPY(mailboxName, dbName);
#if defined (XP_WIN) || defined (XP_MAC) || defined(XP_OS2)
        // mac and windows append ".snm"
        if (XP_STRLEN(mailboxName) > XP_STRLEN(".snm"))
            *(mailboxName + XP_STRLEN(mailboxName) - XP_STRLEN(".snm")) = '\0';
#endif
#if defined (XP_UNIX)
        // unix format is ".boxname.summary"
        if (XP_STRLEN(mailboxName) > XP_STRLEN(".summary"))
        {
        	// we edit the leaf, so find it
        	char *leafNode = XP_STRRCHR(mailboxName, '/');
        	if (leafNode)
        		leafNode++;
        	else
        		leafNode = mailboxName;
        	
            *(leafNode + XP_STRLEN(leafNode) - XP_STRLEN(".summary")) = '\0';
            XP_STRCPY(leafNode, leafNode + 1);
        }
#endif
	}

    return mailboxName;
}


void MSG_IMAPFolderInfoMail::GetFilterSearchUrlString(MSG_Filter *currenFilter, char **imapUrlString)
{
    *imapUrlString = NULL;
    
    if (currenFilter->IsRule())
    {
        MSG_Rule *rule=NULL;
        if (currenFilter->GetRule(&rule) == FilterError_Success)
        {
            msg_SearchOnlineMail::Encode(imapUrlString, rule->GetTermList(), 
										 INTL_DefaultWinCharSetID(0),
										 GetFolderCSID() & ~CS_AUTO);
        }
    }
}

void MSG_IMAPFolderInfoMail::QueueUpImapFilterUrls(MSG_UrlQueue *urlQueue)
{
    MSG_FilterList *filters = NULL;
    if (MSG_FilterList::Open(GetMaster(), filterInbox, NULL, this, &filters) == FilterError_Success)
    {
        int32 filtCount=0;
        filters->GetFilterCount(&filtCount);
        for (int32 filterIndex=0; filterIndex < filtCount; filterIndex++)
        {
            MSG_Filter *currenFilter=NULL;
            if (filters->GetFilterAt(filterIndex, &currenFilter) == FilterError_Success)
            {
                char *searchCommandString = NULL;
                GetFilterSearchUrlString(currenFilter, &searchCommandString);
                if (searchCommandString)
                {
                    char *searchUrl = CreateImapSearchUrl(GetHostName(), 
                                                          GetOnlineName(),
                                                          GetOnlineHierarchySeparator(),
                                                          searchCommandString,
                                                          TRUE);    // use uids
                    if (searchUrl)
                        urlQueue->AddUrlAt(urlQueue->GetCursor() + filterIndex + 1, searchUrl, NULL);
					FREEIF(searchUrl);
                }
            }
        }
        
        filters->Close(); 
    }
}

#define WHITESPACE " \015\012"     // token delimiter msgfinfo.h

IDArray *MSG_IMAPFolderInfoMail::CreateFilterIDArray(const char *imapSearchResultString, int &numberOfHits)
{
    // I expect here a string of UID's, the '* SEARCH' should be already stripped
    
    // make one pass to count the number of hits
    numberOfHits = 0;
	IDArray	*retIDArray = new IDArray;
    char *hitTokenString = XP_STRDUP(imapSearchResultString); 
    if (hitTokenString)
    {
        char *hitUidToken = XP_STRTOK(hitTokenString, WHITESPACE);
        while (hitUidToken)
        {
            numberOfHits++;
            hitUidToken = XP_STRTOK(NULL, WHITESPACE);
        }
        FREEIF(hitTokenString);
    }
    
    // now create the array - do we really need db anymore?
    if (numberOfHits)
    {
        MailDB *imapDB = NULL;
        XP_Bool wasCreated;
        if (ImapMailDB::Open (GetPathname(), FALSE, &imapDB, GetMaster(), &wasCreated) == eSUCCESS)
        {
            hitTokenString = XP_STRDUP(imapSearchResultString); 
            if (hitTokenString)     
            {
                char *hitUidToken = XP_STRTOK(hitTokenString, WHITESPACE);
                while (hitUidToken)
                {
                    MessageKey hitUid;
                    sscanf(hitUidToken, "%ld", &hitUid);
                    retIDArray->Add(hitUid);
                    hitUidToken = XP_STRTOK(NULL, WHITESPACE);
                }
                FREEIF(hitTokenString);
            }
            
            imapDB->Close();
        }
    }
    
    return retIDArray;
}

void MSG_IMAPFolderInfoMail::LogRuleHit(MSG_Filter *filter, DBMessageHdr *msgHdr, MWContext *context)
{
    char    *filterName = "";
    time_t  date;
    char    dateStr[40];    /* 30 probably not enough */
    MSG_RuleActionType actionType;
    MSG_Rule    *rule;
    void                *value;

    MailDB *imapDB = NULL;
    XP_Bool wasCreated;
    if (ImapMailDB::Open (GetPathname(), FALSE, &imapDB, GetMaster(), &wasCreated) == eSUCCESS)
    {
        MSG_DBHandle db = (imapDB) ? imapDB->GetDB() : 0;
        XPStringObj  author;
        XPStringObj  subject;

        FILE *logFile = XP_FileOpen("", xpMailFilterLog, XP_FILE_APPEND);   // will this create?
        
        filter->GetName(&filterName);
        if (filter->GetRule(&rule) != FilterError_Success)
            return;
        rule->GetAction(&actionType, &value);
        date = msgHdr->GetDate();
        XP_StrfTime(context, dateStr, sizeof(dateStr), XP_DATE_TIME_FORMAT,
                    localtime(&date));
        msgHdr->GetAuthor(author, db);
        msgHdr->GetSubject(subject, TRUE, db);
        XP_FilePrintf(logFile, "Applied filter \"%s\" to message from %s - %s at %s\n", filterName, 
            (const char *) author, (const char *) subject, dateStr);
        char *actionStr = rule->GetActionStr(actionType);
        char *actionValue = "";
        if (actionType == acMoveToFolder)
            actionValue = (char *) value;
        XP_FilePrintf(logFile, "Action = %s %s\n\n", actionStr, actionValue);
    }
}

void MSG_IMAPFolderInfoMail::StartUpdateOfNewlySelectedFolder(MSG_Pane *paneOfCommand, 
                                                              XP_Bool loadingFolder,
                                                              MSG_UrlQueue *urlQueueForSelectURL,   // can be NULL
                                                              const IDArray *keysToUndoRedo,		// can be NULL
                                                  			  XP_Bool undo,
															  XP_Bool playbackOfflineEvents, /* = TRUE */
															  Net_GetUrlExitFunc *selectExitFunction /* = NULL */)
{
    XP_ASSERT(paneOfCommand);
    
	// pane could be gone - just return for now...
	if (!MSG_Pane::PaneInMasterList(paneOfCommand))
		return;
	SetGettingMail(TRUE);
	if (playbackOfflineEvents)
    {	// limit the scope of this db
    
		DBFolderInfo *folderInfo = NULL;
		MessageDB     *messagedb = NULL;
    
		// if there are any offline operations that have not been played back, then do it
		MsgERR opendbErr = GetDBFolderInfoAndDB(&folderInfo, &messagedb);
		if ((opendbErr == 0) && !NET_IsOffline())
		{
    		IDArray offlineOperationKeys;
    		MailDB		*mailDB = messagedb->GetMailDB();
    		if (mailDB)
    			mailDB->ListAllOfflineOpIds(offlineOperationKeys);
    		messagedb->Close();
    		messagedb = NULL;
    		
			XP_Bool offlineCreate = GetFolderPrefFlags() & MSG_FOLDER_PREF_CREATED_OFFLINE;

    		if (offlineCreate || offlineOperationKeys.GetSize() > 0)
    		{
				// this can't be right if we have a url queue, because this code 
				// doesn't use it.	The bug arises if we select the inbox in order to
				// download new headers to filter them before going offline,
				// we don't want to play back these events.
				// So, if we don't have a url queue, or it's not the inbox, play back
				// any offline events.
				XP_ASSERT(!urlQueueForSelectURL);
				// make sure this flag is turned on
				SetFolderPrefFlags(GetFolderPrefFlags() | MSG_FOLDER_PREF_OFFLINEEVENTS);
    			OfflineImapGoOnlineState *goOnline = new OfflineImapGoOnlineState(paneOfCommand, this);
    			if (goOnline)
    			{
    				// will force a MSG_PaneNotifyFolderLoaded if offline playbasck fails
	        		if (loadingFolder)
	        			paneOfCommand->SetLoadingImapFolder(this);
	        			
    				goOnline->ProcessNextOperation();
					SetGettingMail(FALSE);
    				return;
    			}
    		}
		}
    
		if ((opendbErr == 0) && messagedb)
    		messagedb->Close();
    
    }	// limit the scopeof this db


	XP_Bool selectFolder = !GetFolderLoadingContext();

    if (selectFolder)
    {
		// if we're selecting the inbox, and the user wants us to cleanupinbox on exit,
		// just set requiresCleanup to TRUE. It would be nicest if we knew a message
		// had been moved or deleted, but I'm not sure where that would be known.
		if (m_flags & MSG_FOLDER_FLAG_INBOX)
		{
			if (GetIMAPHost()->GetExpungeInboxOnExit())
				m_requiresCleanup = TRUE;
		}
	    char *undoString = NULL;
	    if (keysToUndoRedo)
	    {
	    	undoString = AllocateImapUidString(*keysToUndoRedo);
	    	if (undoString)
	    	{
	    		// enough space for uno mas
	    		StrAllocCat(undoString, " ");
	    		if (undoString)
	    		{
	    			XP_MEMMOVE(undoString + 1, undoString, XP_STRLEN(undoString) - 1);
	    			if (undo)
	    				*undoString = '-';
	    			else
	    				*undoString = '+';
	    		}
	    	}
	    }

	    char *selectMailboxesURL = GetOnlineName() ? CreateImapMailboxSelectUrl(GetHostName(), 
	                                                          GetOnlineName(),
	                                                          GetOnlineHierarchySeparator(),
	                                                          undoString) 
													 : 0;
#ifdef DEBUG_bienvenu
	if (!XP_STRCMP(GetName(), "r-thompson"))
	    selectMailboxesURL = CreateImapMailboxLITESelectUrl(GetHostName(), 
	                                                          GetOnlineName(),
	                                                          GetOnlineHierarchySeparator());
#endif
	    if (selectMailboxesURL)
	    {
	    	// we are getting these messages for real now, so stop tricking the fe.
	    	if (GetNumPendingUnread())
				ChangeNumPendingUnread(-GetNumPendingUnread());
			if (GetNumPendingTotalMessages())
			{
				ChangeNumPendingTotalMessages(-GetNumPendingTotalMessages());
				SummaryChanged();
			}
			
	        paneOfCommand->GetContext()->mailMaster = m_master;
	        paneOfCommand->GetContext()->imapURLPane = paneOfCommand;
	        
//			Revisit the url queue later, see if we can enable the following case
//			urlQueueForSelectURL = MSG_UrlQueue::AddUrlToPane( selectMailboxesURL,
//										ImapFolderSelectCompleteExitFunction, paneOfCommand);
//			paneOfCommand->SetLoadingImapFolder(this);
	        
 	        MSG_UrlQueue *urlQueue = urlQueueForSelectURL;

			if (!urlQueue)
				urlQueueForSelectURL = urlQueue = MSG_UrlQueue::FindQueue(paneOfCommand);
 	        if (!urlQueue)
 	            urlQueue = new MSG_ImapLoadFolderUrlQueue(paneOfCommand);
 	        if (urlQueue)
 	        {
				urlQueue->AddInterruptCallback(urlQueue->HandleFolderLoadInterrupt);
				// if the url queue is in a different pane, this pane won't get it's
				// loading imap folder cleared, so don't set it.
				if (paneOfCommand == urlQueue->GetPane())
				{
        			paneOfCommand->SetLoadingImapFolder(this);
			    	// lock others from loading this folder or loading a message this this pane
    				m_LoadingInContext = paneOfCommand->GetContext();
				}
    	
 	        	if (loadingFolder)
 	        	{
					if (!selectExitFunction)	// don't override what the caller may have asked for
 	        			selectExitFunction = ImapFolderSelectCompleteExitFunction;
 	        	}
				else if (!selectExitFunction)
					selectExitFunction = ImapFolderClearLoadingFolder;
 	        		
 	            urlQueue->AddUrl( selectMailboxesURL, selectExitFunction);
 	                                    
 	            if (!urlQueueForSelectURL)  // do we need to start this url queue?
 	                urlQueue->GetNextUrl();
 	        }
	        FREEIF(selectMailboxesURL);
	        
	    }
	    FREEIF(undoString);
    }
    else
    {
    	// this folder is already being loaded elsewhere, so just open the view and notify loaded
	    MailDB *mailDB=NULL;
	    XP_Bool     dbWasCreated=FALSE;
	    
	    MsgERR dbStatus = ImapMailDB::Open(GetPathname(),
	                                     TRUE, // create if necessary
	                                     &mailDB,
	                                     m_master,
	                                     &dbWasCreated);
	   
		XP_Trace("folder already loading, or so we think\n");
	    if (dbStatus == eSUCCESS)
	    {
            OpenThreadView(mailDB,NULL);
            mailDB = NULL;  // storage adopted by OpenThreadView

			FE_PaneChanged(paneOfCommand, FALSE, MSG_PaneProgressDone, 0);
			// tell the fe that this folder is loaded
			NotifyFolderLoaded(paneOfCommand);
	    }
    }
}

void MSG_IMAPFolderInfoMail::AddToSummaryMailDB(const IDArray &keysToFetch,
                                                MSG_Pane *url_pane,
                                                mailbox_spec *boxSpec)
{
    uint32 *theKeys = (uint32 *) XP_ALLOC( keysToFetch.GetSize() * sizeof(uint32) );
    if (theKeys)
    {
		uint32 total = keysToFetch.GetSize();

        for (int keyIndex=0; keyIndex < total; keyIndex++)
        	theKeys[keyIndex] = keysToFetch[keyIndex];
        
        m_DownLoadState = kDownLoadingAllMessageHeaders;

        url_pane->GetContext()->imapURLPane         = url_pane;
        url_pane->GetContext()->mailMaster          = m_master;
        url_pane->GetContext()->currentIMAPfolder   = this;

        char *currentUrl = IMAP_GetCurrentConnectionUrl(boxSpec->connection);
        MSG_UrlQueue *urlQueue = MSG_UrlQueue::FindQueue(currentUrl, url_pane->GetContext());
		FREEIF(currentUrl);

#ifdef DEBUG_bienvenu        
//		XP_ASSERT(urlQueue && urlQueue->IsIMAPLoadFolderUrlQueue());
#endif
        SetParseMailboxState(new ParseIMAPMailboxState(m_master, m_host, this,
													   urlQueue,
													   boxSpec->flagState));
		GetParseMailboxState()->SetPane(url_pane);

        boxSpec->flagState = NULL;		// adopted by ParseIMAPMailboxState
        MailDB *mailDB=NULL;
        XP_Bool     dbWasCreated=FALSE;
        
        MsgERR status = ImapMailDB::Open(m_pathName,
                                         TRUE, // create if necessary
                                         &mailDB,
                                         m_master,
                                         &dbWasCreated);

        if (status == eSUCCESS)
        {
            GetParseMailboxState()->SetDB(mailDB);
            GetParseMailboxState()->SetIncrementalUpdate(TRUE);
	        GetParseMailboxState()->SetMaster(m_master);
	        GetParseMailboxState()->SetContext(url_pane->GetContext());
	        GetParseMailboxState()->SetFolder(this);
	        
	        GetParseMailboxState()->BeginParsingFolder(0);

	        // the imap libnet module will start downloading message headers imap.h
	        IMAP_DownLoadMessagesForMailboxSelect(boxSpec->connection, theKeys, total /*keysToFetch.GetSize() */);
        }
        else
        {
            IMAP_DoNotDownLoadAnyMessageHeadersForMailboxSelect(boxSpec->connection);
        }
    }


}


// returns TRUE if it's an inbox, and the user has selected it, and they've set the pref to cleanup inbox on exit.
XP_Bool MSG_IMAPFolderInfoMail::RequiresCleanup()
{
	// if this is the trash, check if this host has auto empty trash turned on
	if (m_flags & MSG_FOLDER_FLAG_TRASH)
	{
		MSG_IMAPHost *host = GetIMAPHost();
		if (host && host->GetEmptyTrashOnExit())
		{
			 if (GetTotalMessages() + GetNumPendingTotalMessages() > host->GetEmptyTrashThreshhold())
				 m_requiresCleanup = TRUE;
		}
	}
	return m_requiresCleanup;
}

void MSG_IMAPFolderInfoMail::ClearRequiresCleanup()
{
	m_requiresCleanup = FALSE;
}


extern int32 msg_ParseFolderLine (char *line, uint32 line_size, void *closure);
extern MessageDB *GetDBFromState(struct msg_FolderParseState *state);

int  MSG_IMAPFolderInfoMail::HandleBlockForDataBaseCreate(MSG_Pane *pane,
														  const char *str, 
                                                          int32 len, 
                                                          int32 msgUID,
                                                          uint32 msgSize)
{
        // RFC822 messages do not contain the BSD 'From - <date>' message
        // header that our mailbox parser expects.  Create one for each msg.
    ParseIMAPMailboxState *parser = (ParseIMAPMailboxState *) GetParseMailboxState();
    if (NULL == parser)
    {
#ifndef XP_UNIX
    	XP_ASSERT(FALSE);
#endif
#if 0
        SetParseMailboxState(new ParseIMAPMailboxState(m_master, m_host, this,
													   NULL,
													   boxSpec->flagState));
		GetParseMailboxState()->SetPane(pane);

        boxSpec->flagState = NULL;		// adopted by ParseIMAPMailboxState
        MailDB *mailDB=NULL;
        XP_Bool     dbWasCreated=FALSE;
        
        MsgERR status = ImapMailDB::Open(m_pathName,
                                         TRUE, // create if necessary
                                         &mailDB,
                                         m_master,
                                         &dbWasCreated);

        if (status == eSUCCESS)
        {
            GetParseMailboxState()->SetDB(mailDB);
            GetParseMailboxState()->SetIncrementalUpdate(TRUE);
	        GetParseMailboxState()->SetMaster(m_master);
	        GetParseMailboxState()->SetContext(pane->GetContext());
	        GetParseMailboxState()->SetFolder(this);
		}	        
#endif
		// Catch a problem relating to a stream created based on the value of 
		// m_DownLoadState.
		// This is an extremely rare edge case that depends on the timing of when you
		// click on message in a thread pane while the folder is being loaded.
		// If you try to load the message after the folder starts loading and before
		// the folder load starts downloading headers, and the message bytes arrive
		// after the header bytes are finished, you will get into this state.
		// The effect of this fix is to not crash and cancel the current url.
		// I have only witnessed this happening when using the X-client.
		return -1;
    }
    
    MessageDB *msgDB = parser->GetDB();
    XP_ASSERT(msgDB);
    
    int returnLength = 0;
    
    if (msgDB->m_dbFolderInfo->m_LastMessageUID != msgUID)
    {
        // before we change the tracked UID, let the parser know the
        // UID 
        parser->SetPublishUID(msgDB->m_dbFolderInfo->m_LastMessageUID);

        msgDB->m_dbFolderInfo->m_LastMessageUID = msgUID;
        char *envelopeString = msg_GetDummyEnvelope();  // not allocated, do not free
            

        returnLength += HandleBlockForDataBaseCreate(pane, envelopeString, XP_STRLEN(envelopeString), msgUID, msgSize);

        // let the parser know the message size for the next msg header publish
        parser->SetPublishByteLength(msgSize);
    }
    
    // we can get blocks that contain more than one line, 
    // but they never contain partial lines
    char *currentEOL  = XP_STRSTR(str, LINEBREAK);
    const char *currentLine = str;
    while (currentLine < (str + len))
    {
        if (currentEOL)
        {
            parser->ParseFolderLine(currentLine, (currentEOL + LINEBREAK_LEN) - currentLine);
            currentLine = currentEOL + LINEBREAK_LEN;
            currentEOL  = XP_STRSTR(currentLine, LINEBREAK);
        }
        else
        {
            parser->ParseFolderLine(currentLine, strlen(currentLine));
            currentLine = str + len + 1;
        }
    }
        
    return returnLength + len;
}


void MSG_IMAPFolderInfoMail::FinishStreamForDataBaseCreate()
{
}


void MSG_IMAPFolderInfoMail::ParseUidString(char *uidString, IDArray &keys)
{
	// This is in the form <id>,<id>, or <id1>:<id2>
	char curChar = *uidString;
	XP_Bool isRange = FALSE;
	int32	curToken;
	int32	saveStartToken=0;

	for (char *curCharPtr = uidString; curChar && *curCharPtr;)
	{
		char *currentKeyToken = curCharPtr;
		curChar = *curCharPtr;
		while (curChar != ':' && curChar != ',' && curChar != '\0')
			curChar = *curCharPtr++;
		*(curCharPtr - 1) = '\0';
		curToken = atol(currentKeyToken);
		if (isRange)
		{
			while (saveStartToken < curToken)
				keys.Add(saveStartToken++);
		}
		keys.Add(curToken);
		isRange = (curChar == ':');
		if (isRange)
			saveStartToken = curToken + 1;
	}
}

// create a string of uids, suitable for passing to imap url create functions
char *MSG_IMAPFolderInfoMail::AllocateImapUidString(const IDArray &keys)
{
	int blocksAllocated = 1;
	char *returnIdString = (char *) XP_ALLOC(256);
	if (returnIdString)
	{
		IDArray copyOfKeys;

		copyOfKeys.InsertAt(0, &keys);

		// sort keys if they're not already sorted so ranges will work
		copyOfKeys.QuickSort(MSG_Pane::CompareViewIndices);
		char *currentidString = returnIdString;
		*returnIdString  = 0;
		
		int32 startSequence = (copyOfKeys.GetSize() > 0) ? copyOfKeys[0] : -1;
		int32 curSequenceEnd = startSequence;
		uint32 total = copyOfKeys.GetSize();

		for (int keyIndex=0; returnIdString && (keyIndex < total); keyIndex++)
		{
			int32 curKey = copyOfKeys[keyIndex];
			int32 nextKey = (keyIndex + 1 < total) ? copyOfKeys[keyIndex + 1] : -1;
			XP_Bool lastKey = (nextKey == -1);

			if (lastKey)
				curSequenceEnd = curKey;
			if (nextKey == curSequenceEnd + 1 && !lastKey)
			{
				curSequenceEnd = nextKey;
				continue;
			}
			else if (curSequenceEnd > startSequence)
			{
				sprintf(currentidString, "%ld:%ld,", startSequence, curSequenceEnd);
				startSequence = nextKey;
				curSequenceEnd = startSequence;
			}
			else
			{
				startSequence = nextKey;
				curSequenceEnd = startSequence;
				sprintf(currentidString, "%ld,", copyOfKeys[keyIndex]);
			}
			currentidString += XP_STRLEN(currentidString);
			if ((currentidString + 20) > (returnIdString + (blocksAllocated * 256)))
			{
				returnIdString = (char *) XP_REALLOC(returnIdString, ++blocksAllocated*256);
				if (returnIdString)
					currentidString = returnIdString + XP_STRLEN(returnIdString);
			}
		}
	}
	
	if (returnIdString && *returnIdString)
		*(returnIdString + XP_STRLEN(returnIdString) - 1) = 0;	// eat the comma
	
	return returnIdString;
}

// create a fixed size array of uids.  Used for downloading bodies
uint32	*MSG_IMAPFolderInfoMail::AllocateImapUidArray(const IDArray &keys)
{
	uint32 *returnArray = (uint32 *) XP_ALLOC(keys.GetSize() * sizeof(uint32));
	uint32 total = keys.GetSize();

	if (returnArray)
		for(int keyIndex=0; keyIndex < total; keyIndex++)
			returnArray[keyIndex] = keys[keyIndex];
	return returnArray;
}

// when we get here, we're done filtering.  Let the fe know its ok to load a message
void FilteringCompleteExitFunction (URL_Struct *URL_s, int status, MWContext *window_id)
{
	if (URL_s->msg_pane && URL_s->msg_pane->GetMaster())
	{
		// find the imap inbox
		MSG_IMAPFolderInfoMail	*urlFolder = window_id->currentIMAPfolder;
		MSG_IMAPFolderInfoMail *imapInbox;
		if (urlFolder)
			imapInbox = URL_s->msg_pane->GetMaster()->FindImapMailFolder(urlFolder->GetHostName(), "INBOX", NULL, FALSE);
		else
			imapInbox = URL_s->msg_pane->GetMaster()->FindImapMailFolder("INBOX");
		if (imapInbox)
			imapInbox->NotifyFolderLoaded(URL_s->msg_pane);
		
		// make sure the inbox notfies the FE that its loaded
		URL_s->msg_pane->SetActiveImapFiltering(FALSE);
		ImapFolderSelectCompleteExitFunction(URL_s, status, window_id);
	}
}


void MSG_IMAPFolderInfoMail::NotifyFetchAnyNeededBodies(MSG_Pane *urlPane, TNavigatorImapConnection *imapConnection, MailDB *folderDb)
{
	uint32 *bodyFetchIds = NULL; 
	uint32 numberOfBodies= 0;
	if (!urlPane)
	{
		XP_ASSERT(FALSE);	// shouldn't happen, but it does - need to understand why.
		return;
	}
	if ((GetFolderPrefFlags() & MSG_FOLDER_PREF_OFFLINE) && urlPane->GetGoOnlineState() && !urlPane->GetGoOnlineState()->ProcessingStaleFolderUpdate())
	{
		m_DownLoadState = kDownLoadMessageForOfflineDB;
		IDArray bodyKeys;
		
		folderDb->GetIdsWithNoBodies(bodyKeys);
		
		if (bodyKeys.GetSize())
		{
	        urlPane->GetContext()->imapURLPane         = urlPane;
	        urlPane->GetContext()->mailMaster          = m_master;
	        urlPane->GetContext()->currentIMAPfolder   = this;

			bodyFetchIds = AllocateImapUidArray(bodyKeys); 
			if (bodyFetchIds)
				numberOfBodies = bodyKeys.GetSize();
		}
	}
	
	IMAP_DownLoadMessageBodieForMailboxSelect(imapConnection, bodyFetchIds, numberOfBodies);
	
	
    urlPane->EndingUpdate(MSG_NotifyNone, 0, 0);

	NotifyFolderLoaded(urlPane);	// release loading lock on this folder
}

void MSG_IMAPFolderInfoMail::AllFolderHeadersAreDownloaded(MSG_Pane *urlPane, TNavigatorImapConnection *imapConnection)
{
    ParseIMAPMailboxState *currentParser = (ParseIMAPMailboxState *) GetParseMailboxState();
    if (currentParser)
    {
		{	// a block here to limit the scope of this db.
	        MessageDB *folderDB = NULL;
	        DBFolderInfo *dbInfo = NULL;
			
			// since there is a parser this should get the db from the cache
			MsgERR err = GetDBFolderInfoAndDB(&dbInfo, &folderDB);
    		if (err == eSUCCESS)
			{
				currentParser->SetPublishUID(dbInfo->m_LastMessageUID);
		        currentParser->DoneParsingFolder();
				NotifyFetchAnyNeededBodies(urlPane, imapConnection, folderDB->GetMailDB());
				folderDB->Close();
	        }
        }


		// if this is the inbox, apply the filters...
		if (GetFlags() & MSG_FOLDER_FLAG_INBOX)
		{
			MSG_FolderIterator folderIterator(m_master->GetFolderTree());

			MSG_FolderInfo		*curFolder;
			XPPtrArray			foldersToFireFilters;

			curFolder = folderIterator.First();
			MSG_UrlQueue *urlQueue =  currentParser->GetFilterUrlQueue(); 
			if (urlQueue)
				urlQueue->SetSpecialIndexOfNextUrl(urlQueue->GetCursor() + 1);

			// build up list of folders that need filter move actions fired.
			while(curFolder)
			{
				MSG_FolderInfoMail *mailFolder = curFolder->GetMailFolderInfo();
				if (mailFolder)
				{
					int numHits = mailFolder->GetImapIdsToMoveFromInbox()->GetSize();
					if (numHits > 0)
					{
						IDArray *copyOfIds = new IDArray;
						copyOfIds->InsertAt(0, mailFolder->GetImapIdsToMoveFromInbox());
						// maybe we want to do this when we're sure it worked?
						mailFolder->ClearImapIdsToMoveFromInbox();
						StartAsyncCopyMessagesInto(mailFolder,
												   urlPane, NULL /*sourceDB*/,	// don't need sourceDB for IMAP...
												   copyOfIds, numHits,
												   urlPane->GetContext(),
												   urlQueue,
												   TRUE,
												   MSG_MESSAGEKEYNONE);
						foldersToFireFilters.Add(curFolder);
					}
				}
				curFolder = folderIterator.Next();
			}
		
			// now that we know there are more filter urls to run, do not let this select url's exit function
			// do the MSG_PaneNotifyFolderLoaded.  We'll make it happen in FilteringCompleteExitFunction
			urlPane->SetActiveImapFiltering(TRUE);
			if (urlQueue)
			{
				urlQueue->SetSpecialIndexOfNextUrl(MSG_UrlQueue::kNoSpecialIndex);
				urlQueue->AddUrl(kImapFilteringCompleteURL, FilteringCompleteExitFunction, urlPane);
				// tell queue to put new things before the filtering complete exit function
				urlQueue->SetSpecialIndexOfNextUrl(urlQueue->GetSize() - 1);
			}
		}

        delete currentParser;
#ifdef DEBUG_bienvenu
		XP_Trace("clearing parse mailbox state in AllFolderHeadersAreDownloaded\n");
#endif
        SetParseMailboxState(NULL);
    }
	else
		XP_Trace("current parser is null\n");
}

void MSG_IMAPFolderInfoMail::AbortStreamForDataBaseCreate (int /*status*/)
{
    FinishStreamForDataBaseCreate();    // I think its the same deal! -km
}


FolderType MSG_IMAPFolderInfoMail::GetType()
{
    return FOLDER_IMAPMAIL;
}

MSG_IMAPFolderInfoMail *MSG_IMAPFolderInfoMail::FindImapMailOnline(const char* onlineServerName)
{
	MSG_IMAPFolderInfoMail *thePrize = NULL;

	
	if (GetOnlineName() && !XP_STRCMP(GetOnlineName(), onlineServerName))
	{
		/*
		// The online folder names match.  Then see if the namespace has been stripped off.
		// If it has, we want to make sure that we still want that to be the case.
		// Otherwise, we say the folders don't match.
		const char *personalDir = m_host->GetPersonalNamespacePrefix();
		XP_ASSERT(personalDir);

	    if (personalDir && (XP_STRLEN(personalDir) >= 1) &&	// personal namespace prefix exists
			IMAP_GetNumberOfNamespacesForHost(GetHostName()) == 1)	// only one namespace
		{
			// we should have stripped off the namespace
			if (GetFolderPrefFlags() & MSG_FOLDER_PREF_NAMESPACE_STRIPPED)
				thePrize = this;
		}
		else
		{
			// we should not have stripped off the namespace
			if (!(GetFolderPrefFlags() & MSG_FOLDER_PREF_NAMESPACE_STRIPPED))
				thePrize = this;
		}
		*/
		thePrize = this;
	}

	if (!thePrize)
	{
		int numberOfChildren = GetNumSubFolders();
		for (int childIndex = 0; (childIndex < numberOfChildren) && !thePrize; childIndex++)
		{
			MSG_IMAPFolderInfoMail *currentChild = (MSG_IMAPFolderInfoMail *) GetSubFolder(childIndex);
			thePrize = currentChild->FindImapMailOnline(onlineServerName);
		}
	}
	
	return thePrize;
}

MsgERR MSG_IMAPFolderInfoMail::ParentRenamed (const char * parentOnlineName)
{
	MsgERR status = 0;
	const char *currentOnlineName = GetOnlineName();
	char *newName = (char *) XP_ALLOC(XP_STRLEN(currentOnlineName) + XP_STRLEN(parentOnlineName) + 2); // worse case
	if (newName)
	{
		// append my leaf to parent path
		XP_STRCPY(newName, parentOnlineName);
		if (*parentOnlineName != '\0')	// true if promoting to root and no root parent dir
			XP_STRCAT(newName, "/");
		
		const char *currentLeaf = XP_STRRCHR(currentOnlineName, '/');
		if (currentLeaf)
			currentLeaf++;
		else
			currentLeaf = currentOnlineName;
		XP_STRCAT(newName, currentLeaf);
		
    	SetOnlineName(newName);	// reset this node's name
    	
    	
    	FREEIF(newName);
	}
	else
		status = MK_OUT_OF_MEMORY;
	
	return status;
}

MsgERR MSG_IMAPFolderInfoMail::Rename (const char * newName)
{
    // call the ancestor hotline
    MsgERR status = MSG_FolderInfoMail::Rename (newName);
    if (status == 0)
    {
        // fixup the online name
        // replace the leaf of the m_OnlineFolderName with newName
        char *newOnlineName = (char *) XP_ALLOC(XP_STRLEN(newName) + XP_STRLEN(GetOnlineName()) + 1);
        if (newOnlineName)
        {
        	XP_STRCPY(newOnlineName, GetOnlineName());
        	char *leafTarget = XP_STRRCHR(newOnlineName, '/');
        	if (leafTarget)
        		leafTarget++;
        	else
        		leafTarget = newOnlineName;
        	XP_STRCPY(leafTarget, newName);
        	
        	SetOnlineName(newOnlineName);	// reset this node's name
        	
        	FREEIF(newOnlineName);
        }
        else
        	status = MK_OUT_OF_MEMORY;
    }
    return status;
}

char *MSG_IMAPFolderInfoMail::BuildUrl (MessageDB *db, MessageKey key)
{

    char *url = NULL;
    if (db) 
    {
		// This is a message fetch URL
        char idList[10];
        sprintf(idList, "%ld", (long)key);
        url = CreateImapMessageFetchUrl(GetHostName(), 
                                        GetOnlineName(), 
                                        GetOnlineHierarchySeparator(),
                                        idList,             // messageKey
                                        TRUE);              // This is a UID
        
    }
    else
	{
		// This is a URL to the folder itself
		if (!(m_flags & MSG_FOLDER_FLAG_IMAP_PUBLIC))
		{
			// this folder is owned by someone (maybe us).  Adjust the URL accordingly.  
			url = PR_smprintf("%s//%s@%s/%s", "IMAP:", GetFolderOwnerUserName(), GetHostName(), GetOwnersOnlineFolderName());
		}
		else	// public, or default
			url = PR_smprintf("%s//%s/%s", "IMAP:", GetHostName(), GetOnlineName());
	}
    
    return url;
}

char *MSG_IMAPFolderInfoMail::SetupHeaderFetchUrl(MSG_Pane *pane, MessageDB *db, MessageKey startSeq, MessageKey endSeq)
{
    char *url = NULL;
    char idList[20];
	ParseIMAPMailboxState *parseState;

    sprintf(idList, "%ld:%ld", (long)startSeq, (long)endSeq);

#ifdef DEBUG_bienvenu
	XP_Trace("fetching idList %s\n", idList);
#endif
    m_DownLoadState = kDownLoadingAllMessageHeaders;

//    url_pane->GetContext()->imapURLPane         = url_pane;
//    url_pane->GetContext()->mailMaster          = m_master;
//    url_pane->GetContext()->currentIMAPfolder   = this;

    pane->GetContext()->mailMaster          = m_master; // rb testing
    
//    MSG_UrlQueue *urlQueue = MSG_UrlQueue::FindQueue(IMAP_GetCurrentConnectionUrl(boxSpec->connection), url_pane->GetContext());
    parseState = new ParseIMAPMailboxState(m_master, m_host, this, /*urlQueue*/ NULL, NULL /*boxSpec->flagState*/) ;
    SetParseMailboxState (parseState);
  //  boxSpec->flagState = NULL;		// adopted by ParseIMAPMailboxState
    GetParseMailboxState()->SetDB(db->GetMailDB());
	parseState->SetNextSequenceNum(startSeq);
    GetParseMailboxState()->SetIncrementalUpdate(TRUE);
	GetParseMailboxState()->SetMaster(m_master);
//	GetParseMailboxState()->SetContext(url_pane->GetContext());
	GetParseMailboxState()->SetFolder(this);

	GetParseMailboxState()->SetPane(pane);

	// hook up the view to the parse mailbox state.
	// convince context that we're downloading messages to get parsed.
	// This is the wrong place for this but...
	pane->GetContext()->currentIMAPfolder = this;
	if (pane->GetMsgView())
	{
		GetParseMailboxState()->SetView(pane->GetMsgView());
		for (MSG_ViewIndex viewIndex = startSeq - 1; viewIndex < endSeq - 1; viewIndex++)
			pane->GetMsgView()->SetKeyByIndex(viewIndex, kIdPending);
	}
	    
    GetParseMailboxState()->BeginParsingFolder(0);

    return CreateImapMessageHeaderUrl(GetHostName(), 
                                    GetOnlineName(), 
                                    GetOnlineHierarchySeparator(),
                                    idList,             // sequence range
                                    FALSE);              // This is a message sequence number
        
}


int MSG_IMAPFolderInfoMail::SaveFlaggedMessagesToDB(MSG_Pane *pane, MessageDB *db, IDArray &keysToSave)
{
	// this kinda sucks, because we're doing it in the foreground, but we don't have the infrastructure
	// to do it in the background for imap.
	db->ListMatchingKeys(MessageDB::MatchFlaggedNotOffline, keysToSave);
	return SaveMessagesToDB(pane, db, keysToSave);
}


int MSG_IMAPFolderInfoMail::SaveMessagesToDB(MSG_Pane *pane, MessageDB *db, IDArray &keysToSave)
{
	char *downloadURL = SetupMessageBodyFetchUrl(pane, db, keysToSave);

	URL_Struct *url_s = NET_CreateURLStruct(downloadURL, NET_DONT_RELOAD);
	if (!url_s)
		return eUNKNOWN;

	url_s->allow_content_change = FALSE;

	MSG_UrlQueue::AddUrlToPane (url_s, NULL, pane);
	return 0;
}

char *MSG_IMAPFolderInfoMail::SetupMessageBodyFetchUrl(MSG_Pane *pane, MessageDB* /*db*/, IDArray &keysToDownload)
{
    char *url = NULL;
	char *idList = AllocateImapUidString(keysToDownload);

#ifdef DEBUG_bienvenu
	XP_Trace("fetching idList %s\n", idList);
#endif
    m_DownLoadState = kDownLoadMessageForOfflineDB;

	pane->GetContext()->currentIMAPfolder = this;
	    
    return CreateImapMessageFetchUrl(GetHostName(), 
                                    GetOnlineName(), 
                                    GetOnlineHierarchySeparator(),
                                    idList,             // sequence range
                                    TRUE);              // This is a message sequence number
        
}

// libmsg always uses the canonical '/' as the directory separator
// libnet will translate those chars to the the server
// dependent directory separator string.
char *MSG_IMAPFolderInfoMail::CreateOnlineVersionOfLocalPathString
                                (MSG_Prefs& /*prefs*/,
                                 const char *localPath) const
{
    const char *leafName = localPath + 
                     		strlen(m_host->GetLocalDirectory()) + 1;
    
	const char *rootPrefix = m_host->GetRootNamespacePrefix();
	XP_ASSERT(rootPrefix);
    char *onlineReturnName = XP_STRDUP(rootPrefix);
    if (onlineReturnName)
    {
    	int lengthOfCanonicalInboxName = XP_STRLEN(INBOX_FOLDER_NAME);
    	if (!XP_STRNCMP(leafName, INBOX_FOLDER_NAME, lengthOfCanonicalInboxName))
    	{
    		// do not prepend server sub dir for INBOX
    		*onlineReturnName = '\0';
    		// convert canonical INBOX_FOLDER_NAME to "INBOX"
        	StrAllocCat(onlineReturnName, "INBOX");
			// the inbox can have children on some servers
        	if (XP_STRLEN(leafName) > lengthOfCanonicalInboxName)
    			StrAllocCat(onlineReturnName, leafName + lengthOfCanonicalInboxName); 
    	}
    	else
        	StrAllocCat(onlineReturnName, leafName);
    }
    
    
    // squish any ".sbd/" substrings to "/"
    char *sbdSubString = XP_STRSTR(onlineReturnName, ".sbd/");
    while(sbdSubString)
    {
        XP_STRCPY(sbdSubString,         // destination
                  sbdSubString + 4);    // source

        sbdSubString = XP_STRSTR(sbdSubString + 4, ".sbd/");         
    }
    
    // covert escaped spaces to spaces
    char *escapedSpaceString = XP_STRSTR(onlineReturnName, "%20");
    while(escapedSpaceString)
    {
    	*escapedSpaceString++ = ' ';
        XP_STRCPY(escapedSpaceString,         // destination
                  escapedSpaceString + 2);    // source

        escapedSpaceString = XP_STRSTR(escapedSpaceString, "%20");         
    }
    
    
    

    return onlineReturnName;
}

void MSG_IMAPFolderInfoMail::URLFinished(URL_Struct *URL_s, int /*status*/, MWContext* /*window_id*/)
{
    DBFolderInfo *folderInfo = NULL;
    MessageDB     *messagedb = NULL;
    
	MSG_IMAPFolderInfoMail *folder = NULL;
	if (URL_s->msg_pane)
	{
		MSG_FolderInfo *mailFolder = URL_s->msg_pane->GetFolder();
		if (mailFolder && mailFolder->GetType() == FOLDER_IMAPMAIL)
			folder = (MSG_IMAPFolderInfoMail *) mailFolder;
	}
	if (!folder)
		return;
	// I thought this should be true...but it's not.
//	XP_ASSERT(folder == window_id->currentIMAPfolder);

    // if there are any offline operations that have not been played back, then try to just do it
	// this isn't right because we don't want to play real offline events here.
    MsgERR opendbErr = folder->GetDBFolderInfoAndDB(&folderInfo, &messagedb);
    if ((opendbErr == 0) && !NET_IsOffline())
    {
    	IDArray offlineOperationKeys;
    	MailDB		*mailDB = messagedb->GetMailDB();
    	if (mailDB)
    		mailDB->ListAllOfflineOpIds(offlineOperationKeys);
    	messagedb->Close();
    	messagedb = NULL;

		folder->SetHasOfflineEvents(FALSE);	// turn off this flag, so we won't confuse MSG_IMAPURLFinished
    	
    	if (offlineOperationKeys.GetSize() > 0)
    	{
    		OfflineImapGoOnlineState *goOnline = new OfflineImapGoOnlineState(URL_s->msg_pane, (MSG_IMAPFolderInfoMail *) folder);
    		if (goOnline)
    		{
				goOnline->SetPseudoOffline(TRUE);

    			// will force a MSG_PaneNotifyFolderLoaded if offline playbasck fails
//	        	if (loadingFolder)
//	        		paneOfCommand->SetLoadingImapFolder(this);
	        		
    			goOnline->ProcessNextOperation();
    			return;
    		}
    	}
    }
    
    if ((opendbErr == 0) && messagedb)
    	messagedb->Close();
}

const char *MSG_IMAPFolderInfoMail::GetFolderOwnerUserName()
{

	if ((m_flags & MSG_FOLDER_FLAG_IMAP_PERSONAL) ||
		!(m_flags & (MSG_FOLDER_FLAG_IMAP_PUBLIC | MSG_FOLDER_FLAG_IMAP_OTHER_USER)))
	{
		// this is one of our personal mail folders
		// return our username on this host
		return GetIMAPHost()->GetUserName();
	}

	// the only other type of owner is if it's in the other users' namespace
	if (!(m_flags & MSG_FOLDER_FLAG_IMAP_OTHER_USER))
		return NULL;

	if (m_ownerUserName)
		return m_ownerUserName;

	char *onlineName = XP_STRDUP(GetOnlineName());
	if (onlineName)
	{
		const char *otherUserNS = m_host->GetNamespacePrefixForFolder(onlineName);
		if (otherUserNS)
		{
			char *otherUserName = onlineName + XP_STRLEN(otherUserNS);
			if (otherUserName)
			{
				char *nextDelimiter = XP_STRCHR(otherUserName, GetOnlineHierarchySeparator());
				if (nextDelimiter)
					*nextDelimiter = 0;

				m_ownerUserName = XP_STRDUP(otherUserName);	// freed in the destructor
			}
		}
		XP_FREE(onlineName);
	}
	
	return m_ownerUserName;
}

// returns the online folder name, with the other users' namespace and his username
// stripped out
const char *MSG_IMAPFolderInfoMail::GetOwnersOnlineFolderName()
{
	const char *folder = GetOnlineName();
	if (m_flags & MSG_FOLDER_FLAG_IMAP_OTHER_USER)
	{
		const char *user = GetFolderOwnerUserName();
		if (folder && user)
		{
			char *where = XP_STRSTR(folder, user);
			if (where)
			{
				char *relativeFolder = where + XP_STRLEN(user) + 1;
				if (!relativeFolder)	// root of this user's personal namespace
					return (PR_smprintf(""));	// leak of one character?
				else
					return relativeFolder;
			}
		}
		return folder;
	}
	else if (!(m_flags & MSG_FOLDER_FLAG_IMAP_PUBLIC))
	{
		// we own this folder
		const char *personalNS = m_host->GetNamespacePrefixForFolder(folder);
		if (personalNS)
		{
			const char *where = folder + XP_STRLEN(personalNS);
			if (where)
				return where;
			else	// root of our personal namespace
				return (PR_smprintf(""));	// leak of one character?
		}
		else
			return folder;
	}
	else
		return folder;
}

XP_Bool MSG_IMAPFolderInfoMail::GetAdminUrl(MWContext *context, MSG_AdminURLType type)
{
	return m_host->RunAdminURL(context, this, type);
}

XP_Bool MSG_IMAPFolderInfoMail::HaveAdminUrl(MSG_AdminURLType type)
{
	return m_host->HaveAdminURL(type);
}

MSG_IMAPFolderACL	*MSG_IMAPFolderInfoMail::GetFolderACL()
{
	if (!m_folderACL)
		m_folderACL = new MSG_IMAPFolderACL(this);
	return m_folderACL;
}

char *MSG_IMAPFolderInfoMail::CreateACLRightsStringForFolder()
{
	GetFolderACL();	// lazy create
	if (m_folderACL)
	{
		return m_folderACL->CreateACLRightsString();
	}
	return NULL;
}

// Allocates and returns a string naming this folder's type
// i.e. "Personal Folder"
// The caller should free the returned value.
char *MSG_IMAPFolderInfoMail::GetTypeNameForFolder()
{
	if (m_flags & MSG_FOLDER_FLAG_IMAP_PUBLIC)
	{
		return PR_smprintf(XP_GetString(XP_MSG_IMAP_PUBLIC_FOLDER_TYPE_NAME));
	}
	if (m_flags & MSG_FOLDER_FLAG_IMAP_OTHER_USER)
	{
		return PR_smprintf(XP_GetString(XP_MSG_IMAP_OTHER_USERS_FOLDER_TYPE_NAME));
	}
	// personal folder
	if (GetFolderACL()->GetIsFolderShared())
	{
		return PR_smprintf(XP_GetString(XP_MSG_IMAP_PERSONAL_SHARED_FOLDER_TYPE_NAME));
	}
	else
	{
		return PR_smprintf(XP_GetString(XP_MSG_IMAP_PERSONAL_FOLDER_TYPE_NAME));
	}
}

// Allocates and returns a string describing this folder's type
// i.e. "This is a personal folder.  It is shared."
// The caller should free the returned value.
char *MSG_IMAPFolderInfoMail::GetTypeDescriptionForFolder()
{
	if (m_flags & MSG_FOLDER_FLAG_IMAP_PUBLIC)
	{
		return PR_smprintf(XP_GetString(XP_MSG_IMAP_PUBLIC_FOLDER_TYPE_DESCRIPTION));
	}
	if (m_flags & MSG_FOLDER_FLAG_IMAP_OTHER_USER)
	{
		const char *owner = GetFolderOwnerUserName();
		if (owner)
		{
			return PR_smprintf(XP_GetString(XP_MSG_IMAP_OTHER_USERS_FOLDER_TYPE_DESCRIPTION), owner);
		}
		else
		{
			// Another user's folder, for which we couldn't find an owner name
			XP_ASSERT(FALSE);
			return PR_smprintf(XP_GetString(XP_MSG_IMAP_OTHER_USERS_FOLDER_TYPE_DESCRIPTION),
				XP_GetString(XP_MSG_IMAP_UNKNOWN_USER));
		}
	}
	// personal folder
	if (GetFolderACL()->GetIsFolderShared())
	{
		return PR_smprintf(XP_GetString(XP_MSG_IMAP_PERSONAL_SHARED_FOLDER_TYPE_DESCRIPTION));
	}
	else
	{
		return PR_smprintf(XP_GetString(XP_MSG_IMAP_PERSONAL_FOLDER_TYPE_DESCRIPTION));
	}
}

void MSG_IMAPFolderInfoMail::AddFolderRightsForUser(const char *userName, const char *rights)
{
	SetFolderNeedsACLListed(FALSE);
	GetFolderACL()->SetFolderRightsForUser(userName, rights);
}

void MSG_IMAPFolderInfoMail::ClearAllFolderACLRights()
{
	SetFolderNeedsACLListed(FALSE);
	delete m_folderACL;
	m_folderACL = new MSG_IMAPFolderACL(this);
}

void MSG_IMAPFolderInfoMail::RefreshFolderACLRightsView()
{
	if (GetFolderACL()->GetIsFolderShared())
	{
		SetFlag(MSG_FOLDER_FLAG_PERSONAL_SHARED);
	}
	else
	{
		ClearFlag(MSG_FOLDER_FLAG_PERSONAL_SHARED);
	}

	// refresh the view in any folder panes
    if (m_master)
	{
		m_master->BroadcastFolderChanged(this);
	}
}

void	MSG_IMAPFolderInfoMail::SetAdminUrl(const char *adminUrl)
{
	FREEIF(m_adminUrl);
	m_adminUrl = XP_STRDUP(adminUrl);
}


XP_Bool MSG_IMAPFolderInfoMail::GetCanIOpenThisFolder()
{
	if (GetFolderPrefFlags() & MSG_FOLDER_PREF_IMAPNOSELECT)
		return FALSE;

	return (GetFolderACL()->GetCanIReadFolder());
}

// returns TRUE if another folder can be dropped into this folder
XP_Bool	MSG_IMAPFolderInfoMail::GetCanDropFolderIntoThisFolder()
{
	// no inferiors allowed
	if (GetFolderPrefFlags() & MSG_FOLDER_PREF_IMAPNOINFERIORS)
		return FALSE;

	return GetFolderACL()->GetCanICreateSubfolder();
}

// returns TRUE if messages can be dropped into this folder
XP_Bool MSG_IMAPFolderInfoMail::GetCanDropMessagesIntoFolder()
{
	if (GetFolderPrefFlags() & MSG_FOLDER_PREF_IMAPNOSELECT)
		return FALSE;

	return (GetFolderACL()->GetCanIInsertInFolder());
}

// returns TRUE if messages can be dragged from of this folder (but not deleted)
XP_Bool MSG_IMAPFolderInfoMail::GetCanDragMessagesFromThisFolder()
{
	if (GetFolderPrefFlags() & MSG_FOLDER_PREF_IMAPNOSELECT)
		return FALSE;

	return GetFolderACL()->GetCanIReadFolder();
}

// returns TRUE if we can delete messages in this folder
XP_Bool MSG_IMAPFolderInfoMail::GetCanDeleteMessagesInFolder()
{
	return GetFolderACL()->GetCanIDeleteInFolder();
}

XP_Bool MSG_IMAPFolderInfoMail::AllowsPosting ()
{
	return (m_flags & MSG_FOLDER_FLAG_IMAP_PUBLIC);
}

void MSG_IMAPFolderInfoMail::RememberPassword(const char * password)
{
	MailDB *mailDb = NULL;
	XP_Bool wasCreated=FALSE;
	ImapMailDB::Open(m_pathName, TRUE, &mailDb, m_master, &wasCreated);
	if (mailDb)
	{
		mailDb->SetCachedPassword(password);
		mailDb->Close();
	}
}

char *MSG_IMAPFolderInfoMail::GetRememberedPassword()
{
	char *retPassword = NULL;
	if (m_flags & MSG_FOLDER_FLAG_INBOX)
	{
		MailDB *mailDb = NULL;
		XP_Bool wasCreated=FALSE;
		ImapMailDB::Open(m_pathName, TRUE, &mailDb, m_master, &wasCreated);
		if (mailDb)
		{
			XPStringObj cachedPassword;
			mailDb->GetCachedPassword(cachedPassword);
			retPassword = XP_STRDUP(cachedPassword);
			mailDb->Close();

		}
	}
	else
	{
		MSG_FolderInfo *inbox = NULL;
		if (GetIMAPContainer()->GetFoldersWithFlag(MSG_FOLDER_FLAG_INBOX, &inbox, 1) == 1 
			&& inbox != NULL)
		{
			retPassword = inbox->GetRememberedPassword();
		}
	}
	return retPassword;
}

const char *MSG_IMAPFolderInfoMail::GetUserName()
{
	return GetIMAPHost()->GetUserName();
}

XP_Bool MSG_IMAPFolderInfoMail::UserNeedsToAuthenticateForFolder(XP_Bool displayOnly)
{
	if (m_master->IsCachePasswordProtected() && !m_master->IsUserAuthenticated() && !m_master->AreLocalFoldersAuthenticated())
	{
		char *savedPassword = GetRememberedPassword();
		XP_Bool havePassword = savedPassword && XP_STRLEN(savedPassword);
		FREEIF(savedPassword);

		if (havePassword)
		{
			// If we're offline or  we're not authenticated, get authenticated
			if (NET_IsOffline() /* && displayOnly */ )
				return TRUE;
		}
	}
	return FALSE;
}


unsigned int MessageDownLoadStreamWriteReady(NET_StreamClass *stream)
{	
    return MAX_WRITE_READY;
}

int  IMAPHandleBlockForDataBaseCreate(NET_StreamClass *stream, const char *str, int32 msgUID)
{
	void *data_object=stream->data_object;
    MWContext *urlPaneContext = ((IMAPdownLoadStreamData *) data_object)->urlContext;
    MSG_IMAPFolderInfoMail *imapFolder          = urlPaneContext->currentIMAPfolder;	
    return imapFolder->HandleBlockForDataBaseCreate(urlPaneContext->imapURLPane, str,
                                                    strlen(str), 
                                                    msgUID,
                                                    ((IMAPdownLoadStreamData *) data_object)->msgSize);
}

void IMAPFinishStreamForDataBaseCreate(NET_StreamClass *stream)
{
	IMAPdownLoadStreamData *downloadData = (IMAPdownLoadStreamData *) stream->data_object;
    MWContext *urlPaneContext = downloadData->urlContext;
    MSG_IMAPFolderInfoMail *imapFolder          = urlPaneContext->currentIMAPfolder;	
    imapFolder->FinishStreamForDataBaseCreate();
    delete downloadData;
}

extern "C" void NotifyHeaderFetchCompleted(MWContext *currentContext, TNavigatorImapConnection *imapConnection)
{
	if (currentContext->currentIMAPfolder)
		currentContext->currentIMAPfolder->AllFolderHeadersAreDownloaded(currentContext->imapURLPane, imapConnection);
	else
		XP_ASSERT(FALSE);
}


void IMAPAbortStreamForDataBaseCreate (NET_StreamClass *stream, int status)
{
	IMAPdownLoadStreamData *downloadData = (IMAPdownLoadStreamData *) stream->data_object;
    MWContext *urlPaneContext = downloadData->urlContext;
    MSG_IMAPFolderInfoMail *imapFolder          = urlPaneContext->currentIMAPfolder;	
    imapFolder->AbortStreamForDataBaseCreate(status);
    delete downloadData;
}

#ifndef XP_OS2
static 
#else
extern "OPTLINK"
#endif
int32 IMAPHandleDownloadLine(char* line, uint32 length, void* closure)
{
  ParseOutgoingMessage *outgoingParser = (ParseOutgoingMessage *) closure;

  return outgoingParser->ParseFolderLine(line, length);
}

int  IMAPHandleBlockForMessageCopyDownload(NET_StreamClass *stream, const char *str, int32 msgUID)
{
    int returnValue = 0;
	IMAPdownLoadStreamData *downloadStreamData = (IMAPdownLoadStreamData *) stream->data_object;
    MWContext *urlPaneContext = downloadStreamData->urlContext;
    MSG_FolderInfoMail     *localFolder         = (MSG_FolderInfoMail *) urlPaneContext->msgCopyInfo->dstFolder;	

    if (!urlPaneContext->msgCopyInfo->moveState.writestate.fid)
    {
    	int createError = localFolder->CreateMessageWriteStream(&urlPaneContext->msgCopyInfo->moveState,0,0);   // download msg size and flags don't matter
    	if (createError != 0)
    	{
    		FE_Alert(urlPaneContext, XP_GetString(createError));
    		return createError;
    	}
        

       if (urlPaneContext->msgCopyInfo->moveState.writestate.fid)
       {
	        downloadStreamData->outgoingParser.Clear();
			downloadStreamData->outgoingParser.SetOutFile(urlPaneContext->msgCopyInfo->moveState.writestate.fid);
			downloadStreamData->outgoingParser.SetMailDB(urlPaneContext->msgCopyInfo->moveState.destDB);

            char *envelopeString = msg_GetDummyEnvelope();  // not allocated, do not free
            uint32 storePosition = localFolder->SeekToEndOfMessageStore(&urlPaneContext->msgCopyInfo->moveState);
			downloadStreamData->outgoingParser.Init(storePosition);
            urlPaneContext->msgCopyInfo->offlineFolderPositionOfMostRecentMessage = storePosition;
			downloadStreamData->outgoingParser.StartNewEnvelope(envelopeString, strlen(envelopeString));

			// Conjure up an X-Mozilla-Status line from the source IMAP database so the
			// message will have the right flags in the local mail folder. We don't 
			// apparently have a DB for filtering IMAP to local, but we should still 
			// put the X-Mozilla-Status header into the local folder.
			uint32 xMozillaFlags = 0;
			MessageDB *db = urlPaneContext->msgCopyInfo->srcDB;
			if (db)
			{
				DBMessageHdr *dbHdr = db->GetDBHdrForKey (msgUID);
				xMozillaFlags = dbHdr->GetMozillaStatusFlags() & ~MSG_FLAG_RUNTIME_ONLY;
				delete dbHdr;
			}

			char *mozStatus = PR_smprintf("%04.4x", (int) xMozillaFlags);
			if (mozStatus)
			{
				downloadStreamData->m_mozillaStatus = mozStatus;
				downloadStreamData->outgoingParser.m_mozstatus.value = mozStatus;
				downloadStreamData->outgoingParser.m_mozstatus.length = XP_STRLEN(mozStatus);

				// If the message on the server has an X-Mozilla-Status, it must be bogus,
				// so make sure our local opinion of X-Mozilla-Status is the one we keep
				downloadStreamData->outgoingParser.m_IgnoreXMozillaStatus = TRUE; 
			}
	   }
    }
    
    if (urlPaneContext->msgCopyInfo->moveState.writestate.fid)
    {
        localFolder->SeekToEndOfMessageStore(&urlPaneContext->msgCopyInfo->moveState);
        returnValue = msg_LineBuffer (str, strlen(str),
						  &downloadStreamData->obuffer, (uint32 *)&downloadStreamData->obufferSize,
						  (uint32*)&downloadStreamData->obufferIndex,
						  FALSE, IMAPHandleDownloadLine,
						  &downloadStreamData->outgoingParser);
        // note that we need to update the db with the position of the start of a message,
    }
    
    return returnValue;
}

void IMAPFinishStreamForMessageCopyDownload(NET_StreamClass *stream)
{
	IMAPdownLoadStreamData *downloadStreamData = (IMAPdownLoadStreamData *) stream->data_object;
    MWContext *urlPaneContext = downloadStreamData->urlContext;
    MSG_FolderInfoMail     *localFolder         = (MSG_FolderInfoMail *) urlPaneContext->msgCopyInfo->dstFolder;	
    
    // if there are any bytes left in the buffer, send them.  Happens when last line ends in <CR>
    if (downloadStreamData->obufferIndex)
    {
	    IMAPHandleDownloadLine(downloadStreamData->obuffer, 
	    						downloadStreamData->obufferIndex,
	    						&downloadStreamData->outgoingParser);
	}
    							
    
    // save the message header so relevant panes are updated
    MailDB::SetFolderInfoValid(localFolder->GetPathname(), 0, 0);
            
    msg_move_state *state   = &urlPaneContext->msgCopyInfo->moveState;
 	  downloadStreamData->outgoingParser.FinishHeader();
//   MailMessageHdr *mailHdr = (MailMessageHdr*) urlPaneContext->msgCopyInfo->srcDB->GetDBHdrForKey(urlPaneContext->msgCopyInfo->srcArray->GetAt(state->msgIndex));
    
    uint32 endOfMessageStore = localFolder->SeekToEndOfMessageStore(state);
    localFolder->CloseMessageWriteStream(&urlPaneContext->msgCopyInfo->moveState);

    // tweak the msg size because line termination and envelope lines are different
    state->finalDownLoadMessageSize = endOfMessageStore - urlPaneContext->msgCopyInfo->offlineFolderPositionOfMostRecentMessage;

	if (downloadStreamData->outgoingParser.m_newMsgHdr)
	{
	   if (!(downloadStreamData->outgoingParser.m_newMsgHdr->GetFlags() & kIsRead))
	   		downloadStreamData->outgoingParser.m_newMsgHdr->OrFlags(kNew);
	   downloadStreamData->outgoingParser.m_newMsgHdr->SetMessageKey (urlPaneContext->msgCopyInfo->offlineFolderPositionOfMostRecentMessage); 
	   if (state->destDB)
			state->destDB->AddHdrToDB (downloadStreamData->outgoingParser.m_newMsgHdr, NULL, TRUE);    // tell db to notify
		//  we must tweak the message size to account
		// for the envelope and line termination differences
		if (state->finalDownLoadMessageSize)
		{
			downloadStreamData->outgoingParser.m_newMsgHdr->SetByteLength(state->finalDownLoadMessageSize);
			downloadStreamData->outgoingParser.m_newMsgHdr->SetMessageSize(state->finalDownLoadMessageSize);
		}
		downloadStreamData->outgoingParser.m_newMsgHdr = NULL;
	}
	if (downloadStreamData->obuffer)
		XP_FREE (downloadStreamData->obuffer);

    // for IMAP downloads, this is the only place where msgIndex is used
    // advance it so we do not copy the same header n times during a multimessage 
    // download.
    state->msgIndex++;
    delete downloadStreamData;
}

void IMAPAbortStreamForMessageCopyDownload(NET_StreamClass *stream, int /*status*/)
{
	IMAPdownLoadStreamData *downloadStreamData = (IMAPdownLoadStreamData *) stream->data_object;
    MWContext *urlPaneContext = downloadStreamData->urlContext;
    MSG_FolderInfoMail     *localFolder = (MSG_FolderInfoMail *) urlPaneContext->msgCopyInfo->moveState.destfolder;	
    localFolder->CloseMessageWriteStream(&urlPaneContext->msgCopyInfo->moveState);
    delete downloadStreamData;
}




extern "C" NET_StreamClass *CreateIMAPDownloadMessageStream(ImapActiveEntry *ce, uint32 size,
															const char *content_type, XP_Bool content_modified)
{
	MSG_Pane *urlPane = IMAP_GetActiveEntryPane(ce);
	MWContext *currentContext = (urlPane) ? urlPane->GetContext() : (MWContext *)NULL;
	
    NET_StreamClass *returnStream = (NET_StreamClass *)NULL;
    if (currentContext && currentContext->currentIMAPfolder)
        returnStream = currentContext->currentIMAPfolder->CreateDownloadMessageStream(ce, size, content_type, content_modified);
    
    return returnStream;
}

extern "C" void UpdateIMAPMailboxStatus(mailbox_spec *adoptedBoxSpec, MWContext *currentContext)
{
    MSG_IMAPFolderInfoMail *mailFolder = adoptedBoxSpec->allocatedPathName ?
    										currentContext->mailMaster->FindImapMailFolder(adoptedBoxSpec->hostName, adoptedBoxSpec->allocatedPathName, NULL, FALSE) :
    										(MSG_IMAPFolderInfoMail *)NULL;
    if (mailFolder && currentContext->imapURLPane)	// imap biff gets here for XFE somehow with null imapurlpane
    {
    	mailFolder->UpdateFolderStatus(adoptedBoxSpec, currentContext->imapURLPane);
    }
}

extern "C" void UpdateIMAPMailboxInfo(mailbox_spec *adoptedBoxSpec, MWContext *currentContext)
{
    if (!currentContext->mailMaster) // bogus !
 		return; // currentContext->mailMaster = (MSG_Master*) adoptedBoxSpec->connection->fCurrentEntry->window_id;
    MSG_IMAPFolderInfoMail *mailFolder = adoptedBoxSpec->allocatedPathName ?
    										currentContext->mailMaster->FindImapMailFolder(adoptedBoxSpec->hostName, adoptedBoxSpec->allocatedPathName, NULL, FALSE) :
    										(MSG_IMAPFolderInfoMail *)NULL;
    if (mailFolder && currentContext->imapURLPane)	// imap biff gets here for XFE somehow with null imapurlpane
    {
    	mailFolder->UpdateNewlySelectedFolder(adoptedBoxSpec, currentContext->imapURLPane);
    }
    else
    {
    	// we should never get here, some how we can't find folder info that started this update
    	
    	// if we have a mailbox name, alert the user.  Otherwise this was probably an interrupt context
    	if (adoptedBoxSpec->allocatedPathName)
    	{
	    	const char *errorFormat = XP_GetString(MK_MSG_CANT_FIND_SNM);
	    	char *errorMessage = (char *) XP_ALLOC(XP_STRLEN(errorFormat) + XP_STRLEN(adoptedBoxSpec->allocatedPathName) + 1);
	    	if (errorMessage)
	    	{
	    		sprintf(errorMessage, errorFormat, adoptedBoxSpec->allocatedPathName);
	    		FE_Alert(currentContext, errorMessage);
	    		XP_FREE(errorMessage);
	    	}
    	}
   		IMAP_DoNotDownLoadAnyMessageHeadersForMailboxSelect(adoptedBoxSpec->connection);
    }
}

extern "C" int32 GetUIDValidityForSpecifiedImapFolder(const char *hostName, const char *canonicalimapName, MWContext *currentContext)
{
	int32 rvalue = kUidUnknown;
	
	XP_ASSERT(canonicalimapName);

	if (currentContext->mailMaster && canonicalimapName)
	{
		MSG_IMAPFolderInfoMail *mailFolder = currentContext->mailMaster->FindImapMailFolder(hostName, canonicalimapName, NULL, FALSE);
		
		if (mailFolder)
		{
			DBFolderInfo *folderInfo = NULL;
			MessageDB *msgDb = NULL;
			mailFolder->GetDBFolderInfoAndDB(&folderInfo,&msgDb);
			if (folderInfo)
				rvalue = folderInfo->GetImapUidValidity();
			if (msgDb)
				msgDb->Close();
		}
	}
	
	return rvalue;
}


static
XP_Bool FolderNamesMatch(const char *name1, const char *name2)
{
	XP_Bool theyMatch = XP_STRCMP(name1, name2) == 0;
	if (!theyMatch)
	{
		char *nonConstName1 = (char *) name1;
		char *slash1 = 0;
		char *nonConstName2 = (char *) name2;
		char *slash2 = 0;
		
		// if one or both end in slash, ignore it
		if ( *nonConstName1 && (*(nonConstName1 + XP_STRLEN(nonConstName1) - 1 ) == '/') )
		{
			slash1 = nonConstName1 + XP_STRLEN(nonConstName1) - 1;
			*slash1 = '\0';
		}
		if ( *nonConstName2 && (*(nonConstName2 + XP_STRLEN(nonConstName2) - 1 ) == '/') )
		{
			slash2 = nonConstName2 + XP_STRLEN(nonConstName2) - 1;
			*slash2 = '\0';
		}
			
		theyMatch = XP_STRCMP(nonConstName1, nonConstName2) == 0;
		
		// replace the slashes, we promised 'const'
		if (slash1) *slash1 = '/';
		if (slash2) *slash2 = '/';
	}
	
	return theyMatch;
}

// forward
static
void DeleteNonVerifiedImapFolders(MSG_FolderInfo* parentFolder, 
                                  MSG_FolderPane *folderPane,
                                  MSG_Pane **urlPane);  


// obsolete ?
static enum EMailboxDiscoverStatus GiveUserAChanceToChangeServerDir(MSG_Pane *imapURLPane, const char* /*hostName*/)
{
	EMailboxDiscoverStatus discoveryStatus = eContinue;

	// I don't think we want this stopgap anymore, now that we're using subscription.
	if (FALSE /*!FE_Confirm(imapURLPane->GetContext(), XP_GetString(MK_MSG_LOTS_NEW_IMAP_FOLDERS))*/)
	{
		discoveryStatus = eCancelled; 
		char onlineDir[256];
		onlineDir[0] = '\0';
		int stringSize = 256;
		PREF_GetCharPref("mail.imap.server_sub_directory", 
						 onlineDir, &stringSize);

		char *newImapDir = FE_Prompt(imapURLPane->GetContext(), XP_GetString(MK_MSG_IMAP_DIR_PROMPT), onlineDir);
		
		if (newImapDir && XP_STRCMP(newImapDir, onlineDir ))
		{
			discoveryStatus = eNewServerDirectory;
			PREF_SetCharPref("mail.imap.server_sub_directory", newImapDir);
			//IMAP_SetNamespacesFromPrefs(hostName, newImapDir, "", "");
			
			// set all existing imap folders to be non verified and delete them from the folder pane
			MSG_FolderIterator folderIterator(imapURLPane->GetMaster()->GetImapMailFolderTree());
			MSG_FolderInfo *currentFolder = folderIterator.First();
			while (currentFolder)
			{
				if ((currentFolder->GetType() == FOLDER_IMAPMAIL) && !(currentFolder->GetFlags() & MSG_FOLDER_FLAG_INBOX))
					((MSG_IMAPFolderInfoMail *)currentFolder)->SetIsOnlineVerified(FALSE);
				currentFolder = folderIterator.Next();
			}
			
			DeleteNonVerifiedImapFolders(imapURLPane->GetMaster()->GetImapMailFolderTree(), 
										(MSG_FolderPane *) imapURLPane->GetMaster()->FindFirstPaneOfType(MSG_FOLDERPANE), &imapURLPane);
			XP_FREE(newImapDir);
		}
		
	}
	
	return discoveryStatus;
}


static enum EMailboxDiscoverStatus DiscoverIMAPMailboxForSubscribe(mailbox_spec *adoptedBoxSpec, MWContext *currentContext)
{
	char *folderName = adoptedBoxSpec->allocatedPathName;
	if (currentContext->imapURLPane)
	{
		MSG_Pane *pane = currentContext->imapURLPane;
		XP_ASSERT(pane->GetPaneType() == MSG_SUBSCRIBEPANE);
		if (pane->GetPaneType() == MSG_SUBSCRIBEPANE)
		{
			uint32 folder_flags = 0;
			if (adoptedBoxSpec->box_flags & kPersonalMailbox)
				folder_flags |= MSG_GROUPNAME_FLAG_IMAP_PERSONAL;
			if (adoptedBoxSpec->box_flags & kPublicMailbox)
				folder_flags |= MSG_GROUPNAME_FLAG_IMAP_PUBLIC;
			if (adoptedBoxSpec->box_flags & kOtherUsersMailbox)
				folder_flags |= MSG_GROUPNAME_FLAG_IMAP_OTHER_USER;
			if (adoptedBoxSpec->box_flags & kNoselect)
				folder_flags |= MSG_GROUPNAME_FLAG_IMAP_NOSELECT;
			((MSG_SubscribePane *)pane)->AddIMAPGroupToList(folderName, adoptedBoxSpec->hierarchySeparator, adoptedBoxSpec->discoveredFromLsub, folder_flags);
		}
	}
    IMAP_FreeBoxSpec(adoptedBoxSpec);
	return eContinue;
}

/* static */ MSG_IMAPFolderInfoMail *MSG_IMAPFolderInfoMail::CreateNewIMAPFolder(MSG_IMAPFolderInfoMail *parentFolder, MWContext *currentContext, 
	const char *mailboxName, const char *onlineLeafName, uint32 mailboxFlags, MSG_IMAPHost *host)
{
	MailDB *newDb = NULL;
	XP_Bool wasCreated=FALSE;
	MSG_IMAPFolderInfoMail *newFolder = NULL;

	// convert spaces back to %20 quote char
	char *offlinePathLeafTokenString = XP_PlatformPartialPathToXPPartialPath(onlineLeafName);
	
	// calculate a leaf name that works for this client's platform
	char *diskLeafName;
	if (parentFolder)
		diskLeafName = MSG_FolderInfoMail::CreatePlatformLeafNameForDisk(offlinePathLeafTokenString,currentContext->mailMaster,parentFolder);
	else
		diskLeafName = MSG_FolderInfoMail::CreatePlatformLeafNameForDisk(offlinePathLeafTokenString,currentContext->mailMaster, host->GetLocalDirectory());
	int newPathStringLength = parentFolder ? XP_STRLEN(parentFolder->GetPathname()) : 
	    									 XP_STRLEN(host->GetLocalDirectory());
	newPathStringLength += XP_STRLEN(diskLeafName) + 6;	// 6 enough for .sbd/ and \0
	char *newboxPath = (char*) XP_ALLOC (newPathStringLength);

	if (newboxPath)
	{
		XP_STRCPY(newboxPath, parentFolder ? parentFolder->GetPathname() : host->GetLocalDirectory());
    
		if (parentFolder)
		{
			// make sure the parent .sbd dir exists
			int dirStatus = 0;
			XP_StatStruct stat;
			XP_BZERO (&stat, sizeof(stat));
			if (0 == XP_Stat (newboxPath, &stat, xpMailSubdirectory))
			{
				if (!S_ISDIR(stat.st_mode))
					dirStatus = MK_COULD_NOT_CREATE_DIRECTORY; // a file .sbd already exists
			}
			else
				dirStatus = XP_MakeDirectory (newboxPath, xpMailSubdirectory);
			if ((0 != dirStatus) && currentContext)
			{
				char *str = PR_smprintf(XP_GetString(MK_COULD_NOT_CREATE_DIRECTORY),newboxPath);
				if (str)
				{
					FE_Alert(currentContext, str);
					XP_FREE(str);
				}
			}
			XP_STRCAT(newboxPath,".sbd");
		}
    
		XP_STRCAT(newboxPath, "/");
		XP_STRCAT(newboxPath, diskLeafName);
    
		char *progressString = PR_smprintf(XP_GetString(MK_MSG_IMAP_DISCOVERING_MAILBOX),onlineLeafName);
		if (progressString)
		{
			NET_Progress(currentContext, progressString);
			XP_FREE(progressString);
		}

		// create the new database, set the online name
		ImapMailDB::Open(newboxPath, TRUE, &newDb, currentContext->mailMaster, &wasCreated);
		if (newDb)
		{
			newDb->m_dbFolderInfo->SetMailboxName(mailboxName);
			newFolder = new MSG_IMAPFolderInfoMail(newboxPath, 
													currentContext->mailMaster, 
													parentFolder ? parentFolder->GetDepth() + 1 : 2, 
													MSG_FOLDER_FLAG_MAIL, host);
													
			if (host->GetDefaultOfflineDownload())
				newFolder->SetFolderPrefFlags(newFolder->GetFolderPrefFlags() | MSG_FOLDER_PREF_OFFLINE);

			newDb->SetFolderInfoValid(newboxPath,0,0);
			
			
			// set this flag now because it is used in the compare function when adding the folder
			// to its parent sub folder array
			if (mailboxFlags & kImapTrash)
				newFolder->SetFlag(MSG_FOLDER_FLAG_TRASH);

			// set the mailbox type flags, if they exist
			if (mailboxFlags & kPersonalMailbox)
				newFolder->SetFlag(MSG_FOLDER_FLAG_IMAP_PERSONAL);
			else if (mailboxFlags & kPublicMailbox)
				newFolder->SetFlag(MSG_FOLDER_FLAG_IMAP_PUBLIC);
			else if (mailboxFlags & kOtherUsersMailbox)
				newFolder->SetFlag(MSG_FOLDER_FLAG_IMAP_OTHER_USER);

			XP_Bool addToFolderPane = FALSE;
			if (parentFolder)
			{
				if (!parentFolder->HasSubFolders())
				{
					// if this is a new parent, its initial state should be collapsed
					parentFolder->SetFlagInAllFolderPanes(MSG_FOLDER_FLAG_ELIDED);
				}
                
				// MSG_FolderInfoMail::Adopt does too much.  We already know the
				// newFolder pathname and parent sbd exist
				parentFolder->LightWeightAdopt(newFolder);
				parentFolder->SummaryChanged();
				addToFolderPane = !(parentFolder->GetFlags() & MSG_FOLDER_FLAG_ELIDED);
			}
			else    // no parent
			{
				MSG_FolderInfoContainer *imapRootFolder = host->GetHostFolderInfo();
				if (imapRootFolder)
				{
					XPPtrArray *rootSubfolderArray = (XPPtrArray *) imapRootFolder->GetSubFolders();
					rootSubfolderArray->Add(newFolder);
					imapRootFolder->SummaryChanged();
					addToFolderPane = TRUE;
				}
			}
			
			// add the new folder to the folder pane trees so the fe can see it.
			
			if (addToFolderPane)
			{
				currentContext->mailMaster->BroadcastFolderAdded (newFolder);
				newFolder->SummaryChanged();
			}
		}
	}
	if (newDb)
		newDb->Close();
	return newFolder;
}

// If currentContext is NULL, we are simply discovering this folder and adding it to the folder list automatically, as if
// the user clicked on a link to a folder that's not in his list yet.
// Master should never be NULL.
static enum EMailboxDiscoverStatus DiscoverIMAPMailboxForFolderPane(mailbox_spec *adoptedBoxSpec, MSG_Master *master,
																	MWContext *currentContext, XP_Bool broadcastDiscovery)
{
	MailDB *newDb = NULL;
	XP_Bool wasCreated=FALSE;
	XP_Bool shouldStripNamespace = TRUE;
	XP_Bool folderIsNew = FALSE;
	XP_Bool folderIsNamespace = FALSE;

	EMailboxDiscoverStatus discoveryStatus = eContinue;

	MSG_IMAPHost *host = master->GetIMAPHost(adoptedBoxSpec->hostName);
	if (!host)
	{
		// We discovered an IMAP folder on a host we don't know about.  This should never happen.
		XP_ASSERT(FALSE);
		return discoveryStatus;
	}

	// Check to see if we already know about it.  Maybe we created it when we found its child.
	MSG_IMAPFolderInfoMail *newFolder = master->FindImapMailFolder(adoptedBoxSpec->hostName, adoptedBoxSpec->allocatedPathName, NULL, FALSE);

	if (!newFolder) // if we haven't seen this guy before, then create it
	{
		folderIsNew = TRUE;
		MSG_IMAPFolderInfoMail *parentFolder = NULL;

		const char *namespacePrefix = host->GetNamespacePrefixForFolder(adoptedBoxSpec->allocatedPathName);
		if (namespacePrefix &&
			!XP_STRCMP(namespacePrefix, adoptedBoxSpec->allocatedPathName))
		{
			folderIsNamespace = TRUE;	// the folder we are discovering is actually a namespace prefix.
		}

		if (!folderIsNamespace)	// if the folder is really a namespace, don't break up the hierarchy
		{
			// make sure we know about the parent
			char *parentFolderName = XP_STRDUP(adoptedBoxSpec->allocatedPathName);

			if (parentFolderName)	// trying to find/discover the parent
			{
				char *lastSlash = XP_STRRCHR(parentFolderName, '/');
				if (lastSlash)	// if there is a hierarchy, there is a parent
				{
					*lastSlash = '\0';

					if (namespacePrefix &&
						(XP_STRLEN(namespacePrefix) >= 1) &&
						host->GetShouldStripThisNamespacePrefix(namespacePrefix))
					{
						// this namespace is being stripped
						parentFolder = NULL;
					}
					else
					{
						const char *parentNamespace = host->GetNamespacePrefixForFolder(parentFolderName);
						if (parentNamespace &&
							!XP_STRNCMP(parentNamespace, parentFolderName, XP_STRLEN(parentFolderName)))
						{
							// The parent is a namespace itself.  Find the folder for that namespace.
							parentFolder = master->FindImapMailFolder(adoptedBoxSpec->hostName, parentNamespace, NULL, FALSE);
						}
						else
						{
							parentFolder = master->FindImapMailFolder(adoptedBoxSpec->hostName, parentFolderName, NULL, FALSE);
							if (!parentFolder)
							{
								mailbox_spec *parentSpec = (mailbox_spec *) XP_ALLOC( sizeof(mailbox_spec));
								if (parentSpec)
								{
									parentSpec->folderSelected = FALSE;
									parentSpec->hierarchySeparator = adoptedBoxSpec->hierarchySeparator;
									parentSpec->allocatedPathName = XP_STRDUP(parentFolderName);
									parentSpec->hostName = XP_STRDUP(adoptedBoxSpec->hostName);
									parentSpec->flagState = NULL;
									parentSpec->discoveredFromLsub = FALSE;
									parentSpec->box_flags = 0;
									parentSpec->onlineVerified = FALSE;

									DiscoverIMAPMailbox(parentSpec,     // storage adopted
														master,
														currentContext,
														broadcastDiscovery);
									parentFolder = master->FindImapMailFolder(adoptedBoxSpec->hostName, parentFolderName, NULL, FALSE);
								}
							}
						}
					}
				}
				XP_FREE(parentFolderName);
			}
		}

		if (folderIsNamespace && host->GetShouldStripThisNamespacePrefix(namespacePrefix))
		{
			// We've discovered a namespace (only) that we're going to strip off.
			// So don't discover a folderinfo for it.
			return eContinue;
		}

		// now create this folder
	    const char *onlineLeafName = NULL;
		if (!folderIsNamespace)	// only break out the hierarchy if this folder is not a namespace itself
			onlineLeafName = XP_STRRCHR(adoptedBoxSpec->allocatedPathName, '/');

	    if (!onlineLeafName)
	    	onlineLeafName = adoptedBoxSpec->allocatedPathName;	// this is a root level folder
	    else
	    	onlineLeafName++;	// eat the '/'

	    // convert spaces back to %20 quote char
	    char *offlinePathLeafTokenString = XP_PlatformPartialPathToXPPartialPath(onlineLeafName);
	    
	    // calculate a leaf name that works for this client's platform
	    char *diskLeafName;
	    if (parentFolder)
	    	diskLeafName = MSG_FolderInfoMail::CreatePlatformLeafNameForDisk(offlinePathLeafTokenString,master,parentFolder);
	    else
	    	diskLeafName = MSG_FolderInfoMail::CreatePlatformLeafNameForDisk(offlinePathLeafTokenString,master, host->GetLocalDirectory());
	    int newPathStringLength = parentFolder ? XP_STRLEN(parentFolder->GetPathname()) : 
	    										 XP_STRLEN(host->GetLocalDirectory());
	    newPathStringLength += XP_STRLEN(diskLeafName) + 6;	// 6 enough for .sbd/ and \0
	    char *newboxPath = (char*) XP_ALLOC (newPathStringLength);

		if (newboxPath)
		{
            XP_STRCPY(newboxPath, parentFolder ? parentFolder->GetPathname() : host->GetLocalDirectory());
            
            if (parentFolder)
            {
            	// make sure the parent .sbd dir exists
            	int dirStatus = 0;
			    XP_StatStruct stat;
			    XP_BZERO (&stat, sizeof(stat));
			    if (0 == XP_Stat (newboxPath, &stat, xpMailSubdirectory))
			    {
			        if (!S_ISDIR(stat.st_mode))
			            dirStatus = MK_COULD_NOT_CREATE_DIRECTORY; // a file .sbd already exists
			    }
			    else
			        dirStatus = XP_MakeDirectory (newboxPath, xpMailSubdirectory);
				if ((0 != dirStatus) && currentContext)
				{
					char *str = PR_smprintf(XP_GetString(MK_COULD_NOT_CREATE_DIRECTORY),newboxPath);
					if (str)
					{
						FE_Alert(currentContext, str);
						XP_FREE(str);
					}
				}
            	XP_STRCAT(newboxPath,".sbd");
            }
            
            XP_STRCAT(newboxPath, "/");
            XP_STRCAT(newboxPath, diskLeafName);
            
			if (currentContext)
			{
				char *progressString = PR_smprintf(XP_GetString(MK_MSG_IMAP_DISCOVERING_MAILBOX),onlineLeafName);
				if (progressString)
				{
            		NET_Progress(currentContext, progressString);
					XP_FREE(progressString);
				}
			}

			// create the new database, set the online name
			ImapMailDB::Open(newboxPath, TRUE, &newDb, master, &wasCreated);
			if (newDb)
			{
				newDb->m_dbFolderInfo->SetMailboxName(adoptedBoxSpec->allocatedPathName);
				newFolder = new MSG_IMAPFolderInfoMail(newboxPath, 
														master, 
														parentFolder ? parentFolder->GetDepth() + 1 : 2, 
														MSG_FOLDER_FLAG_MAIL, host);
														
				if (host->GetDefaultOfflineDownload())
					newFolder->SetFolderPrefFlags(newFolder->GetFolderPrefFlags() | MSG_FOLDER_PREF_OFFLINE);

	            newDb->SetFolderInfoValid(newboxPath,0,0);
				
				
				// set this flag now because it is used in the compare function when adding the folder
				// to its parent sub folder array
			    if (adoptedBoxSpec->box_flags & kImapTrash)
			    	newFolder->SetFlag(MSG_FOLDER_FLAG_TRASH);

				// set the mailbox type flags, if they exist
				if (adoptedBoxSpec->box_flags & kPersonalMailbox)
					newFolder->SetFlag(MSG_FOLDER_FLAG_IMAP_PERSONAL);
				else if (adoptedBoxSpec->box_flags & kPublicMailbox)
					newFolder->SetFlag(MSG_FOLDER_FLAG_IMAP_PUBLIC);
				else if (adoptedBoxSpec->box_flags & kOtherUsersMailbox)
					newFolder->SetFlag(MSG_FOLDER_FLAG_IMAP_OTHER_USER);

				XP_Bool addToFolderPane = FALSE;
                if (parentFolder)
                {
                    if (!parentFolder->HasSubFolders())
                    {
                    	// if this is a new parent, its initial state should be collapsed
                    	parentFolder->SetFlagInAllFolderPanes(MSG_FOLDER_FLAG_ELIDED);
                    }
                    	
                    // MSG_FolderInfoMail::Adopt does too much.  We already know the
                    // newFolder pathname and parent sbd exist
                    parentFolder->LightWeightAdopt(newFolder);
                    parentFolder->SummaryChanged();
                    addToFolderPane = !(parentFolder->GetFlags() & MSG_FOLDER_FLAG_ELIDED);
                }
		        else    // no parent
		        {
		            MSG_FolderInfoContainer *imapRootFolder = master->GetImapMailFolderTreeForHost(adoptedBoxSpec->hostName);
		            if (imapRootFolder)
		            {
		                XPPtrArray *rootSubfolderArray = (XPPtrArray *) imapRootFolder->GetSubFolders();
		                rootSubfolderArray->Add(newFolder);
		                imapRootFolder->SummaryChanged();
		                addToFolderPane = TRUE;
		            }
		        }
		        
		        
		        // add the new folder to the folder pane trees so the fe can see it.
				if (addToFolderPane && currentContext)
				{
					if (broadcastDiscovery)
						master->BroadcastFolderAdded (newFolder, currentContext->imapURLPane);
		            newFolder->SummaryChanged();
				}
		        
			}
		}

		FREEIF(offlinePathLeafTokenString);
		FREEIF(diskLeafName);
	}
	
	if (newFolder && (discoveryStatus == eContinue) )
	{	    	
		MSG_IMAPHost *host = master->GetIMAPHost(adoptedBoxSpec->hostName);
		XP_ASSERT(host);
		XP_Bool usingSubscription = host ? host->GetIsHostUsingSubscription() : FALSE;

		if (adoptedBoxSpec->box_flags & kImapTrash)
	    	newFolder->SetFlag(MSG_FOLDER_FLAG_TRASH);

		// set the mailbox type flags, if they exist
		if (adoptedBoxSpec->box_flags & kPersonalMailbox)
			newFolder->SetFlag(MSG_FOLDER_FLAG_IMAP_PERSONAL);
		else if (adoptedBoxSpec->box_flags & kPublicMailbox)
			newFolder->SetFlag(MSG_FOLDER_FLAG_IMAP_PUBLIC);
		else if (adoptedBoxSpec->box_flags & kOtherUsersMailbox)
			newFolder->SetFlag(MSG_FOLDER_FLAG_IMAP_OTHER_USER);

		newFolder->SetIsOnlineVerified(adoptedBoxSpec->onlineVerified);
	    newFolder->SetOnlineHierarchySeparator(adoptedBoxSpec->hierarchySeparator);

	    int32 prefFlags = newFolder->GetFolderPrefFlags();
	    
	    // for each imap folder flag, record it if its different
	    prefFlags = (adoptedBoxSpec->box_flags & kMarked)      ? (prefFlags | MSG_FOLDER_PREF_IMAPMARKED) : (prefFlags & ~MSG_FOLDER_PREF_IMAPMARKED);
	    prefFlags = (adoptedBoxSpec->box_flags & kUnmarked)    ? (prefFlags | MSG_FOLDER_PREF_IMAPUNMARKED) : (prefFlags & ~MSG_FOLDER_PREF_IMAPUNMARKED);
	    prefFlags = (adoptedBoxSpec->box_flags & kNoinferiors) ? (prefFlags | MSG_FOLDER_PREF_IMAPNOINFERIORS) : (prefFlags & ~MSG_FOLDER_PREF_IMAPNOINFERIORS);
	    prefFlags = (adoptedBoxSpec->box_flags & kNoselect)    ? (prefFlags | MSG_FOLDER_PREF_IMAPNOSELECT) : (prefFlags & ~MSG_FOLDER_PREF_IMAPNOSELECT);
		// good time to clear this flag.
		prefFlags &= ~MSG_FOLDER_PREF_CREATED_OFFLINE;

		// once we've discovered it using LSUB, it sticks
		if (!(prefFlags & MSG_FOLDER_PREF_LSUB_DISCOVERED))
			prefFlags = usingSubscription ? (prefFlags | MSG_FOLDER_PREF_LSUB_DISCOVERED) : (prefFlags & ~MSG_FOLDER_PREF_LSUB_DISCOVERED);
	
		// Set a flag on whether or not we've stripped off the namespace
		//if (folderIsNew)
		//	prefFlags = shouldStripNamespace ? (prefFlags | MSG_FOLDER_PREF_NAMESPACE_STRIPPED) : (prefFlags & ~MSG_FOLDER_PREF_NAMESPACE_STRIPPED);

	    if (prefFlags != newFolder->GetFolderPrefFlags())
	    	newFolder->SetFolderPrefFlags(prefFlags);	    	
	}
        
	if (newDb) 
		newDb->Close(); 

    IMAP_FreeBoxSpec(adoptedBoxSpec);
    
	if (discoveryStatus == eContinue && folderIsNew)
		discoveryStatus = eContinueNew;

    return discoveryStatus;
}


extern "C" enum EMailboxDiscoverStatus DiscoverIMAPMailbox(mailbox_spec *adoptedBoxSpec, MSG_Master *master,
														   MWContext *currentContext, XP_Bool broadcastDiscovery)
{
	// Not sure why this happens (pane is deleted), but it's a guaranteed crash.
	if (currentContext && !MSG_Pane::PaneInMasterList(currentContext->imapURLPane))
		currentContext->imapURLPane = NULL;
	if (currentContext && currentContext->imapURLPane && 
		(currentContext->imapURLPane->GetPaneType() ==
		 MSG_SUBSCRIBEPANE)) 
		return DiscoverIMAPMailboxForSubscribe(adoptedBoxSpec,
											   currentContext);	
	else if (currentContext && currentContext->imapURLPane &&
			 currentContext->imapURLPane->IMAPListInProgress())
	{
		currentContext->imapURLPane->SetIMAPListMailboxExist(TRUE);
		return eContinue;
	}
	else
		return DiscoverIMAPMailboxForFolderPane(adoptedBoxSpec,
												master,
												currentContext,
												broadcastDiscovery); 
}

extern "C" void ReportSuccessOfOnlineDelete(MWContext *currentContext, const char *hostName, const char *mailboxName)
{
    XP_ASSERT(currentContext && currentContext->mailMaster);
	if (currentContext && currentContext->mailMaster)
	{
		MSG_IMAPFolderInfoMail *doomedFolder = currentContext->mailMaster->FindImapMailFolder(hostName, mailboxName, NULL, FALSE);
		XP_ASSERT(doomedFolder);
		if (doomedFolder)
		{
			MSG_FolderInfo *tree = currentContext->mailMaster->GetImapMailFolderTree();
			XP_ASSERT(tree);
			if (tree)
			{
				// I'm leaving the FindParent split out in case we put in parent pointers, 
				// which would make this an O(1) operation
				MSG_FolderInfo *parent = tree->FindParentOf (doomedFolder); 
				XP_ASSERT(parent);
				if (parent)
					parent->PropagateDelete ((MSG_FolderInfo**) &doomedFolder); // doomedFolder is null on return
			}
		}
	}
}

extern "C" void ReportFailureOfOnlineCreate(MWContext *currentContext, const char* /*serverMessage*/)
{
	// Tell the FE object that was trying to create the folder that it failed. This is
	// their signal to do something with the New Folder dialog. Not sure if they *should* take
	// it down, but this is enough info for them to clean up.
	//
	// If we need to send this to more panes than just the URLPane, we should create a new 
	// broadcast thing in the master 

    XP_ASSERT(currentContext->imapURLPane);
	if (currentContext->imapURLPane)
		FE_PaneChanged (currentContext->imapURLPane, FALSE, MSG_PaneNotifyNewFolderFailed, 0);	
}

extern "C" void ReportSuccessOfOnlineRename(MWContext *currentContext, folder_rename_struct *names)
{
    XP_ASSERT(currentContext->mailMaster);
    MSG_IMAPFolderInfoMail *renameFolder = currentContext->mailMaster->FindImapMailFolder(names->fOldName);
    
    if (renameFolder)
    {
        // calculate the leaf of the new name
        char *newleafName = XP_STRRCHR(names->fNewName, '/');
        if (newleafName)
        	newleafName++;
        else
        	newleafName = names->fNewName;	// renaming root level box
        
        // if the renameFolder->GetName() == newleafName, then this is a hierarchy move
        if (0 == XP_STRCMP(renameFolder->GetName(), newleafName))
        {
        	// find the new parent folder
        	MSG_FolderInfo *parentFolder = NULL;
        	char *parentOnlineName = XP_STRDUP(names->fNewName);
        	char *slash = XP_STRRCHR(parentOnlineName,'/');
        	if (slash)
        	{
        		*slash = 0;
        		parentFolder = currentContext->mailMaster->FindImapMailFolder(parentOnlineName);
        	}
        	else
        	{
        		// moving to root level
        		parentFolder = currentContext->mailMaster->GetImapMailFolderTree();
        	}
        	
        	// fix up the folder pane
	        MSG_FolderPane *folderPane = (MSG_FolderPane *) currentContext->mailMaster->FindFirstPaneOfType(MSG_FOLDERPANE);
	        if (folderPane)
	        {
	        	// folder pane
	        	folderPane->PerformLegalFolderMove(renameFolder, parentFolder);
	        }
        }
        else
        {
	        // do the rename
	        MSG_Pane *folderPane = currentContext->mailMaster->FindFirstPaneOfType(MSG_FOLDERPANE);
	        if (folderPane)
	            ((MSG_FolderPane *) folderPane)->RenameOfflineFolder(renameFolder, newleafName);
        }
    }
}

static 
XP_Bool NoDescendantsAreVerified(MSG_IMAPFolderInfoMail *parentFolder)
{
	XP_Bool nobodyIsVerified = TRUE;
	int numberOfSubfolders = parentFolder->GetNumSubFolders();
	
	for (int childIndex=0; nobodyIsVerified && (childIndex < numberOfSubfolders); childIndex++)
	{
		MSG_IMAPFolderInfoMail *currentChild = (MSG_IMAPFolderInfoMail *) parentFolder->GetSubFolder(childIndex);
		nobodyIsVerified = !currentChild->GetIsOnlineVerified() && NoDescendantsAreVerified(currentChild);
	}
	
	return nobodyIsVerified;
}

static
XP_Bool AllDescendantsAreNoSelect(MSG_IMAPFolderInfoMail *parentFolder)
{
	XP_Bool allDescendantsAreNoSelect = TRUE;
	int numberOfSubfolders = parentFolder->GetNumSubFolders();
	
	for (int childIndex=0; allDescendantsAreNoSelect && (childIndex < numberOfSubfolders); childIndex++)
	{
		MSG_IMAPFolderInfoMail *currentChild = (MSG_IMAPFolderInfoMail *) parentFolder->GetSubFolder(childIndex);
		allDescendantsAreNoSelect = (currentChild->GetFolderPrefFlags() & MSG_FOLDER_PREF_IMAPNOSELECT) &&
									AllDescendantsAreNoSelect(currentChild);
	}
	
	return allDescendantsAreNoSelect;
}

static
void UnsubscribeFromAllDescendants(MSG_IMAPFolderInfoMail *parentFolder, MSG_Pane *pane)
{
	int numberOfSubfolders = parentFolder->GetNumSubFolders();
	
	for (int childIndex=0; childIndex < numberOfSubfolders; childIndex++)
	{
		MSG_IMAPFolderInfoMail *currentChild = (MSG_IMAPFolderInfoMail *) parentFolder->GetSubFolder(childIndex);
		char *unsubscribeUrl = CreateIMAPUnsubscribeMailboxURL(currentChild->GetHostName(), currentChild->GetOnlineName());	// unsubscribe from child
		if (unsubscribeUrl)
		{
			MSG_UrlQueue::AddUrlToPane(unsubscribeUrl, NULL, pane);
			XP_FREE(unsubscribeUrl);
		}
		UnsubscribeFromAllDescendants(currentChild, pane);	// unsubscribe from its children
	}
}

static
void DeleteNonVerifiedImapFolders(MSG_FolderInfo* parentFolder, 
                                  MSG_FolderPane *folderPane,// can be null
                                  MSG_Pane **url_pane)   
{
	XP_Bool autoUnsubscribeFromNoSelectFolders = TRUE;
	PREF_GetBoolPref("mail.imap.auto_unsubscribe_from_noselect_folders", &autoUnsubscribeFromNoSelectFolders);
    int numberOfSubFolders = parentFolder->GetNumSubFolders();
    for (int folderIndex = 0; folderIndex < numberOfSubFolders; )
    {
        MSG_FolderInfo* currentFolder = parentFolder->GetSubFolder(folderIndex);
        if (currentFolder && (currentFolder->GetType() == FOLDER_IMAPMAIL))
        {
            MSG_IMAPFolderInfoMail *currentImapFolder = (MSG_IMAPFolderInfoMail *) currentFolder;
            
            MSG_IMAPFolderInfoMail *parentImapFolder = (parentFolder->GetType() == FOLDER_IMAPMAIL) ? 
            												(MSG_IMAPFolderInfoMail *) parentFolder :
            												(MSG_IMAPFolderInfoMail *)NULL;
            
            // if the parent is the imap container or an imap folder whose children were listed, then this bool is true.
            // We only delete .snm files whose parent's children were listed											
            XP_Bool parentChildrenWereListed =	(parentImapFolder == NULL) || 
            									(LL_CMP(parentImapFolder->GetTimeStampOfLastList(), >= , IMAP_GetTimeStampOfNonPipelinedList()));

			MSG_IMAPHost *imapHost = currentImapFolder->GetIMAPHost();
#ifdef DEBUG_chrisf
			XP_ASSERT(imapHost);
#endif
           	XP_Bool usingSubscription = imapHost ? imapHost->GetIsHostUsingSubscription() : TRUE;
			XP_Bool folderIsNoSelectFolder = (currentImapFolder->GetFolderPrefFlags() & MSG_FOLDER_PREF_IMAPNOSELECT);
			XP_Bool shouldDieBecauseNoSelect = usingSubscription ?
									(folderIsNoSelectFolder ? (NoDescendantsAreVerified(currentImapFolder) || AllDescendantsAreNoSelect(currentImapFolder)): FALSE)
									: FALSE;

            if (shouldDieBecauseNoSelect ||
				((usingSubscription ? TRUE : parentChildrenWereListed) && !currentImapFolder->GetIsOnlineVerified() && NoDescendantsAreVerified(currentImapFolder)))
            {
                // This folder is going away.
				// Give notification so that folder menus can be rebuilt.
				if (*url_pane)
				{
					XPPtrArray referringPanes;
					uint32 total;

					(*url_pane)->GetMaster()->FindPanesReferringToFolder(currentFolder,&referringPanes);
					total = referringPanes.GetSize();
					for (int i=0; i < total;i++)
					{
						MSG_Pane *currentPane = (MSG_Pane *) referringPanes.GetAt(i);
						if (currentPane)
						{
							if (currentPane->GetFolder() == currentFolder)
							{
								currentPane->SetFolder(NULL);
								FE_PaneChanged(currentPane, TRUE, MSG_PaneNotifyFolderDeleted, (uint32)currentFolder);
							}
						}
					}

                	FE_PaneChanged(*url_pane, TRUE, MSG_PaneNotifyFolderDeleted, (uint32)currentFolder);

					// If we are running the IMAP subscribe upgrade, and we are deleting the folder that we'd normally
					// try to load after the process completes, then tell the pane not to load that folder.
					if (((MSG_ThreadPane *)(*url_pane))->GetIMAPUpgradeFolder() == currentFolder)
						((MSG_ThreadPane *)(*url_pane))->SetIMAPUpgradeFolder(NULL);

                	if ((*url_pane)->GetFolder() == currentFolder)
						*url_pane = NULL;


					if (shouldDieBecauseNoSelect && autoUnsubscribeFromNoSelectFolders && usingSubscription)
					{
						char *unsubscribeUrl = CreateIMAPUnsubscribeMailboxURL(imapHost->GetHostName(), currentImapFolder->GetOnlineName());
						if (unsubscribeUrl)
						{
							if (url_pane)
								MSG_UrlQueue::AddUrlToPane(unsubscribeUrl, NULL, *url_pane);
							else if (folderPane)
								MSG_UrlQueue::AddUrlToPane(unsubscribeUrl, NULL, folderPane);
#ifdef DEBUG_chrisf
							else XP_ASSERT(FALSE);
#endif
							XP_FREE(unsubscribeUrl);
						}

						if (AllDescendantsAreNoSelect(currentImapFolder) && (currentImapFolder->GetNumSubFolders() > 0))
						{
							// This folder has descendants, all of which are also \NoSelect.
							// We'd like to unsubscribe from all of these as well.
							if (url_pane)
								UnsubscribeFromAllDescendants(currentImapFolder, *url_pane);
							else if (folderPane)
								UnsubscribeFromAllDescendants(currentImapFolder, folderPane);
						}

					}
				}
				else
				{
#ifdef DEBUG_chrisf
					XP_ASSERT(FALSE);
#endif
				}

				parentFolder->PropagateDelete(&currentFolder); // currentFolder is null on return
				numberOfSubFolders--;
				folderIndex--;
            }
            else
            {
                if (currentFolder->HasSubFolders())
                    DeleteNonVerifiedImapFolders(currentFolder, folderPane, url_pane);
            }
        }
        folderIndex++;  // not in for statement because we modify it
    }
}


static void DeleteNonVerifiedExitFunction (URL_Struct *URL_s, int status, MWContext *window_id)
{
	if (URL_s->msg_pane)
	{
	    MSG_Master *master = URL_s->msg_pane->GetMaster();
		MSG_Pane   *notificationPane = URL_s->msg_pane;
		if (notificationPane->GetParentPane() != NULL)
			notificationPane = notificationPane->GetParentPane();

		Net_GetUrlExitFunc *existingExit = URL_s->msg_pane->GetPreImapFolderVerifyUrlExitFunction();
	    if (existingExit)
	    {
	    	URL_s->msg_pane->SetPreImapFolderVerifyUrlExitFunction(NULL);
	    	(existingExit) (URL_s, status, window_id);
	    }

		if (MSG_Pane::PaneInMasterList(notificationPane))
		{
			char *host = NET_ParseURL (URL_s->address, GET_HOST_PART);

			if (host && (status >= 0))	// don't delete non-verified folders if there was some kind of error (such as interruption)
			{
				// trash any MSG_IMAPFolderInfoMail's for folders that were not online verified
				MSG_FolderInfoContainer *imapContainer = master->GetImapMailFolderTreeForHost(host);
				MSG_FolderPane *folderPane = (MSG_FolderPane *) master->FindFirstPaneOfType(MSG_FOLDERPANE);
				DeleteNonVerifiedImapFolders(imapContainer, folderPane, &notificationPane);
				XP_FREE(host);
			}
		}
	}
}


extern "C" void ReportMailboxDiscoveryDone(MWContext *currentContext, URL_Struct *URL_s)
{
    XP_ASSERT(currentContext->mailMaster);

	if (currentContext->imapURLPane && (currentContext->imapURLPane->GetPaneType() == MSG_SUBSCRIBEPANE))
	{
		// Finished discovering folders for the subscribe pane.
		((MSG_SubscribePane *)(currentContext->imapURLPane))->ReportIMAPFolderDiscoveryFinished();
	}
	else
	{
		// only do this if we're discovering folders for real (i.e. not subscribe UI)

		// if this profile has not yet dealt with the mail.imap.delete_is_move_to_trash pref yet,
		// deal with it now.  If there is a trash folder change the preference to delete to trash.
		if (currentContext->mailMaster)
		{
			MSG_Prefs *prefs = currentContext->mailMaster->GetPrefs();
			if (!(prefs->GetStartingMailNewsProfileAge() & MSG_IMAP_DELETE_MODEL_UPGRADE_FLAG))
			{
				// 0 -> 1 transition is reserved for the imap delete model trick
				prefs->SetMailNewsProfileAgeFlag(MSG_IMAP_DELETE_MODEL_UPGRADE_FLAG);
				
				if (!prefs->IMAPMessageDeleteIsMoveToTrash())
				{
					MSG_IMAPFolderInfoContainer *tree = currentContext->mailMaster->GetImapMailFolderTree();
					if (tree)
					{
	            		MSG_FolderInfo *trashFolder = NULL;
	            		tree->GetFoldersWithFlag (MSG_FOLDER_FLAG_TRASH, &trashFolder, 1);
	            		if (trashFolder)
	            		{
	            			// so a trash folder exists, lets change the delete model
	            			PREF_SetBoolPref("mail.imap.delete_is_move_to_trash",TRUE);
	            		}
					}
				}
			}
		}
    
		if (URL_s && URL_s->msg_pane && !URL_s->msg_pane->GetPreImapFolderVerifyUrlExitFunction())
		{
    		URL_s->msg_pane->SetPreImapFolderVerifyUrlExitFunction(URL_s->pre_exit_fn);
    		URL_s->pre_exit_fn = DeleteNonVerifiedExitFunction;
		}

	    XP_ASSERT(currentContext->imapURLPane);
		if (currentContext->imapURLPane)
    		currentContext->imapURLPane->SetNumberOfNewImapMailboxes(0);

		// Go through folders and find if there are still any that are left unverified.
		// If so, manually LIST them to see if we can find out any info about them.
		char *hostName = NET_ParseURL(URL_s->address, GET_HOST_PART);
		if (hostName && currentContext->mailMaster && currentContext->imapURLPane)
		{
			MSG_FolderInfoContainer *hostContainerInfo = currentContext->mailMaster->GetImapMailFolderTreeForHost(hostName);
			MSG_IMAPFolderInfoContainer *hostInfo = hostContainerInfo ? hostContainerInfo->GetIMAPFolderInfoContainer() : (MSG_IMAPFolderInfoContainer *)NULL;
			if (hostInfo)
			{
				// for each folder

				int32 numberOfUnverifiedFolders = hostInfo->GetUnverifiedFolders(NULL, 0);
				if (numberOfUnverifiedFolders > 0)
				{
					MSG_IMAPFolderInfoMail **folderList = (MSG_IMAPFolderInfoMail **)XP_ALLOC(sizeof(MSG_IMAPFolderInfoMail*) * numberOfUnverifiedFolders);
					if (folderList)
					{
						int32 numUsed = hostInfo->GetUnverifiedFolders(folderList, numberOfUnverifiedFolders);
						for (int32 k = 0; k < numUsed; k++)
						{
							MSG_IMAPFolderInfoMail *currentFolder = folderList[k];
							if ((currentFolder->GetNumSubFolders() > 0) && !NoDescendantsAreVerified(currentFolder))
							{
								// If there are no subfolders and this is unverified, we don't want to run
								// this url.  That is, we want to undiscover the folder.
								// If there are subfolders and no descendants are verified, we want to 
								// undiscover all of the folders.
								// Only if there are subfolders and at least one of them is verified do we want
								// to refresh that folder's flags, because it won't be going away.
								char *url = CreateIMAPListFolderURL(hostName, currentFolder->GetOnlineName());
								if (url)
								{
									MSG_UrlQueue::AddUrlToPane(url, NULL, currentContext->imapURLPane);
									XP_FREE(url);
								}
							}
						}
						XP_FREE(folderList);
					}
				}
			}
			XP_FREE(hostName);
		}
		else
		{
			XP_ASSERT(FALSE);
		}
	}

}



extern "C" void ReportSuccessOfOnlineCopy(MWContext *currentContext, enum ImapOnlineCopyState copyState)
{
	if (currentContext->msgCopyInfo)
	{
		currentContext->msgCopyInfo->imapOnLineCopyState = copyState;
		MSG_FinishCopyingMessages(currentContext);
	}
    
    if (copyState == kFailedCopy)
    	FE_Alert(currentContext, XP_GetString(MK_MSG_ONLINE_COPY_FAILED));
    if (copyState == kFailedDelete)
    	FE_Alert(currentContext, XP_GetString(MK_MSG_ONLINE_MOVE_FAILED));
}

extern "C" void ReportSuccessOfChildMailboxDiscovery(MWContext *currentContext)
{
	if (!currentContext || !currentContext->imapURLPane)
		return;
	MSG_Pane *pane = currentContext->imapURLPane;
	if (pane->GetPaneType() == MSG_SUBSCRIBEPANE)
	{
		((MSG_SubscribePane *)pane)->IMAPChildDiscoverySuccessful();
	}
}


extern "C" 
void BeginMessageUpload(MWContext *currentContext, 
                        PRFileDesc *ioSocket,
                        UploadCompleteFunctionPointer *completeFunction, 
                        void *completionFunctionArgument)
{
    msg_move_state *moveState = &currentContext->msgCopyInfo->moveState;
    moveState->uploadToIMAPsocket     = ioSocket;   // this will be detected and the copy will start
    moveState->uploadCompleteFunction = completeFunction;
    moveState->uploadCompleteArgument = completionFunctionArgument;
}


extern "C" XP_Bool MSG_ShouldUpgradeToIMAPSubscription(MSG_Master *mast)
{
	MSG_Prefs *prefs = mast->GetPrefs();
	XP_ASSERT(prefs);
	if (prefs)
	{
		int32 profileAge = prefs->GetStartingMailNewsProfileAge();
		if (profileAge & MSG_IMAP_SUBSCRIBE_UPGRADE_FLAG)	// we've upgraded to subscription already
			return FALSE;
		else
			return TRUE;
	}
	else
		return FALSE;
}

extern "C" void MSG_ReportSuccessOfUpgradeToIMAPSubscription(MWContext *context, enum EIMAPSubscriptionUpgradeState *state)
{
    XP_ASSERT(context->mailMaster);

	if (!context->mailMaster)
		return;

	if (*state == kBringUpSubscribeUI)
	{
		// force it to bring up the subscribe UI
		context->mailMaster->TryUpgradeToIMAPSubscription(context, TRUE, NULL, NULL);
	}
	else	// write out the prefs, all done!
	{
		context->mailMaster->SetIMAPSubscriptionUpgradedPrefs();
	}

	// storage now owned by us
	FREEIF(state);
}

extern "C" XP_Bool MSG_GetFolderNeedsSubscribing(MSG_FolderInfo *folder)
{
	if (folder)
	{
		MSG_IMAPFolderInfoMail *imapInfo = folder->GetIMAPFolderInfoMail();
		if (imapInfo)
			return imapInfo->GetFolderNeedsSubscribing();
		else
			return FALSE;
	}
	else
		return FALSE;
}

extern "C" XP_Bool MSG_GetFolderNeedsACLListed(MSG_FolderInfo *folder)
{
	if (folder)
	{
		MSG_IMAPFolderInfoMail *imapInfo = folder->GetIMAPFolderInfoMail();
		if (imapInfo)
			return imapInfo->GetFolderNeedsACLListed();
		else
			return FALSE;
	}
	else
		return FALSE;
}

extern "C" void
MSG_AddFolderRightsForUser(MSG_Master *master, const char *hostName, const char*mailboxName,
										   const char *userName, const char *rights)
{

	MSG_IMAPFolderInfoMail *fi = master->FindImapMailFolder(hostName, mailboxName, NULL, FALSE);
	if (fi)
	{
		fi->AddFolderRightsForUser(userName, rights);
	}
	else
	{
		// we don't have info about this folder... maybe it's an unsolicited response?  probably not.
		XP_ASSERT(FALSE);
	}
}

extern "C" void 
MSG_ClearFolderRightsForFolder(MSG_Master *master, const char *hostName, const char *mailboxName)
{
	MSG_IMAPFolderInfoMail *fi = master->FindImapMailFolder(hostName, mailboxName, NULL, FALSE);
	if (fi)
	{
		fi->ClearAllFolderACLRights();
	}
	else
	{
		// we don't have info about this folder... maybe it's an unsolicited response?  probably not.
		XP_ASSERT(FALSE);
	}
}

extern "C" void
MSG_RefreshFolderRightsViewForFolder(MSG_Master *master, const char *hostName, const char *mailboxName)
{
	MSG_IMAPFolderInfoMail *fi = master->FindImapMailFolder(hostName, mailboxName, NULL, FALSE);
	if (fi)
	{
		fi->RefreshFolderACLRightsView();
	}
	else
	{
		// we don't have info about this folder... maybe it's an unsolicited response?  probably not.
		XP_ASSERT(FALSE);
	}
}


/*
--------------------------------------------------------------
MSG_IMAPFolderInfoContainer
--------------------------------------------------------------
 */

MSG_IMAPFolderInfoContainer::MSG_IMAPFolderInfoContainer (const char *name, uint8 depth, MSG_IMAPHost *host, MSG_Master *master) 
: MSG_FolderInfoContainer (name, depth, MSG_FolderInfoMail::CompareFolders) 
{
	m_prettyName = NULL;
	m_flags |= MSG_FOLDER_FLAG_MAIL | MSG_FOLDER_FLAG_IMAPBOX | MSG_FOLDER_FLAG_IMAP_SERVER;
	m_host = host;
	m_foldersNeedUpdating = FALSE;
	m_master = master;
}

const char *MSG_IMAPFolderInfoContainer::GetPrettyName()  
{
    if (!m_prettyName)
	{
		m_prettyName = m_host->GetPrettyName();
		if (!m_prettyName)
			m_prettyName = PR_smprintf (XP_GetString(MK_MSG_IMAP_CONTAINER_NAME), GetName());
	}
    return m_prettyName;
}

FolderType MSG_IMAPFolderInfoContainer::GetType()
{
	return FOLDER_IMAPSERVERCONTAINER;
}

XP_Bool 
MSG_IMAPFolderInfoContainer::IsDeletable()
{
	return TRUE;
}


MSG_IMAPFolderInfoMail *MSG_IMAPFolderInfoContainer::FindImapMailOnline(const char* onlineServerName, const char *owner, XP_Bool createIfMissing)
{
	MSG_IMAPFolderInfoMail *thePrize = NULL;
	char *ourOnlineServerName = XP_STRDUP(onlineServerName);
	XP_ASSERT(ourOnlineServerName);
	if (!ourOnlineServerName)
		return NULL;

	if (owner && *owner)
	{
		// if there was an owner of this folder specified in the URL
		if (!XP_STRCMP(owner, m_host->GetUserName()))
		{
			// the owner is us.  Prepend the personal namespace, if it's not INBOX (protocol-defined)
			if (XP_STRCASECMP(onlineServerName, "INBOX"))
			{
				const char *prefix = m_host->GetDefaultNamespacePrefixOfType(kPersonalNamespace);
				if (prefix)
				{
					ourOnlineServerName = PR_smprintf("%s%s", prefix, onlineServerName);
				}
			}
		}
		else
		{
			// the owner is not us.  Prepend the other users namespace
			const char *prefix = m_host->GetDefaultNamespacePrefixOfType(kOtherUsersNamespace);
			if (prefix)
			{
				ourOnlineServerName = PR_smprintf("%s%s%c%s", prefix, owner, prefix[XP_STRLEN(prefix)-1], onlineServerName);
			}
		}
	}	

	int numberOfChildren = GetNumSubFolders();
	for (int childIndex = 0; (childIndex < numberOfChildren) && !thePrize; childIndex++)
	{
		MSG_IMAPFolderInfoMail *currentChild = (MSG_IMAPFolderInfoMail *) GetSubFolder(childIndex);
		thePrize = currentChild->FindImapMailOnline(ourOnlineServerName);
	}

	if (!thePrize && createIfMissing)
	{
		mailbox_spec *createdSpec = (mailbox_spec *) XP_ALLOC( sizeof(mailbox_spec));
		if (createdSpec)
		{
			createdSpec->folderSelected = FALSE;
			createdSpec->hierarchySeparator = '/';	// we don't know anything about this folder yet.  Try '/'.
			createdSpec->allocatedPathName = XP_STRDUP(ourOnlineServerName);
			createdSpec->hostName = XP_STRDUP(GetHostName());
			createdSpec->flagState = NULL;
			createdSpec->discoveredFromLsub = FALSE;
			createdSpec->box_flags = kNoselect;		// default, until we've discovered otherwise
			createdSpec->onlineVerified = FALSE;
        
			DiscoverIMAPMailbox(createdSpec, m_master, NULL, TRUE);

			thePrize = FindImapMailOnline(ourOnlineServerName, NULL, FALSE);	// don't try to create it again

			if (thePrize)
				thePrize->SetFolderNeedsSubscribing(TRUE);
		}
	}
	
	FREEIF(ourOnlineServerName);

	return thePrize;
}

char *MSG_IMAPFolderInfoContainer::BuildUrl (MessageDB * /* db */, MessageKey /* key */)
{
    return PR_smprintf("%s//%s", "IMAP:", GetHostName());
}

XP_Bool MSG_IMAPFolderInfoContainer::GetAdminUrl(MWContext *context, MSG_AdminURLType type)
{
	return m_host->RunAdminURL(context, this, type);
}
XP_Bool MSG_IMAPFolderInfoContainer::HaveAdminUrl(MSG_AdminURLType type)
{
	return m_host->HaveAdminURL(type);
}

int32 MSG_IMAPFolderInfoContainer::GetUnverifiedFolders(MSG_IMAPFolderInfoMail** result,
                                           int32 resultsize) 
{
    int num = 0;
    MSG_IMAPFolderInfoMail *folder = NULL;
    for (int i=0; i < m_subFolders->GetSize(); i++) {
        folder = (MSG_IMAPFolderInfoMail*) m_subFolders->GetAt(i);
        num += folder->GetUnverifiedFolders(result + num, resultsize - num);
    }
    return num;
}

XP_Bool MSG_IMAPFolderInfoMail::DeleteIsMoveToTrash()
{
	return m_host->GetDeleteIsMoveToTrash();
}

XP_Bool MSG_IMAPFolderInfoMail::ShowDeletedMessages()
{
	return (m_host->GetIMAPDeleteModel() == MSG_IMAPDeleteIsIMAPDelete);
}

#ifdef FE_SetBiffInfoDefined		// only Win32 has the stand-alone biff for now
									// to enable this on other platforms, simply add them to the
									// #ifdef around FE_SetBiffInfo in msgcom.h
									// and implement FE_SetBiffInfo in the FE


void UpdateStandAloneIMAPBiff(const IDArray& keysToFetch)
{
	// there are new keys to fetch here
	// if this is the INBOX, we want to record the highest UID 
	// we've ever seen for purposes of the stand-alone biff
	// affectedDB->m_dbFolderInfo->m_LastMessageUID;
	uint32 highWaterUID = 0;
	static const char szHighWaterKey[] = "IMAPHighWaterUID";
	uint32 total = keysToFetch.GetSize();

	for (uint32 fetchIndex = 0; fetchIndex < total; fetchIndex++)
	{
		if (keysToFetch[fetchIndex] > highWaterUID)
			highWaterUID = keysToFetch[fetchIndex];
	}
	if (highWaterUID != 0)
	{
		FE_SetBiffInfo(MSG_IMAPHighWaterMark, highWaterUID);
	}
}

#else

void UpdateStandAloneIMAPBiff(const IDArray& /*keysToFetch*/)
{
}

#endif		// #ifdef FE_SetBiffInfoDefined

MsgERR MSG_IMAPFolderInfoContainer::Adopt (MSG_FolderInfo *srcFolder, int32 *pOutPos)
{
	MsgERR err = eSUCCESS;
	XP_ASSERT (srcFolder->GetType() == FOLDER_IMAPMAIL);	// we can only adopt imap folders msgfinfo.h
	MSG_FolderInfoMail *mailFolder = (MSG_FolderInfoMail*) srcFolder;

	if (srcFolder == this)
		return MK_MSG_CANT_COPY_TO_SAME_FOLDER;

	if (ContainsChildNamed(mailFolder->GetName()))
		return MK_MSG_FOLDER_ALREADY_EXISTS;	

	// Recurse the tree to adopt srcFolder's children
	err = mailFolder->PropagateAdopt (m_host->GetLocalDirectory(), m_depth);
	// Add the folder to our tree in the right sorted position
	if (eSUCCESS == err)
	{
		XP_ASSERT(m_subFolders->FindIndex(0, srcFolder) == -1);
		*pOutPos = m_subFolders->Add (srcFolder);
//		m_subFolders.InsertAt (0, srcFolder);
//		m_subFolders.QuickSort (MSG_FolderInfoMail::CompareFolders);
//		*pOutPos = m_subFolders.FindIndex (0, srcFolder);
	}

	return err;
}

MSG_IMAPHost *MSG_IMAPFolderInfoContainer::GetIMAPHost() {return m_host;}

const char* MSG_IMAPFolderInfoContainer::GetHostName()
{
	return (m_host) ? m_host->GetHostName() : GetMaster()->GetPrefs()->GetPopHost();
}


MSG_IMAPFolderInfoContainer::~MSG_IMAPFolderInfoContainer () 
{
	FREEIF(m_prettyName);
}

MSG_IMAPFolderInfoContainer	*MSG_IMAPFolderInfoContainer::GetIMAPFolderInfoContainer()
{
	return this;
}


///////// MSG_IMAPFolderACL class ///////////////////////////////

static int
imap_hash_strcmp (const void *a, const void *b)
{
  return XP_STRCMP ((const char *) a, (const char *) b);
}

static XP_Bool imap_freehashrights(XP_HashTable /*table*/, const void* key,
										void* value, void* /*closure*/)
{
	char *valueChar = (char*) value;
	char *userChar = (char*) key;
	FREEIF(valueChar);
	FREEIF(userChar);
	return TRUE;
}

MSG_IMAPFolderACL::MSG_IMAPFolderACL(MSG_IMAPFolderInfoMail *folder)
{
	XP_ASSERT(folder);
	m_folder = folder;
	m_rightsHash = XP_HashTableNew(24, XP_StringHash, imap_hash_strcmp);
	m_aclCount = 0;
	BuildInitialACLFromCache();
}

MSG_IMAPFolderACL::~MSG_IMAPFolderACL()
{
	XP_MapRemhash(m_rightsHash, imap_freehashrights, NULL);
	XP_HashTableDestroy(m_rightsHash);
}

// We cache most of our own rights in the MSG_FOLDER_PREF_* flags
void MSG_IMAPFolderACL::BuildInitialACLFromCache()
{
	char *myrights = 0;

	if (m_folder->GetFolderPrefFlags() & MSG_FOLDER_PREF_IMAP_ACL_READ)
		StrAllocCat(myrights,"r");
	if (m_folder->GetFolderPrefFlags() & MSG_FOLDER_PREF_IMAP_ACL_STORE_SEEN)
		StrAllocCat(myrights,"s");
	if (m_folder->GetFolderPrefFlags() & MSG_FOLDER_PREF_IMAP_ACL_WRITE)
		StrAllocCat(myrights,"w");
	if (m_folder->GetFolderPrefFlags() & MSG_FOLDER_PREF_IMAP_ACL_INSERT)
		StrAllocCat(myrights,"i");
	if (m_folder->GetFolderPrefFlags() & MSG_FOLDER_PREF_IMAP_ACL_POST)
		StrAllocCat(myrights,"p");
	if (m_folder->GetFolderPrefFlags() & MSG_FOLDER_PREF_IMAP_ACL_CREATE_SUBFOLDER)
		StrAllocCat(myrights,"c");
	if (m_folder->GetFolderPrefFlags() & MSG_FOLDER_PREF_IMAP_ACL_DELETE)
		StrAllocCat(myrights,"d");
	if (m_folder->GetFolderPrefFlags() & MSG_FOLDER_PREF_IMAP_ACL_ADMINISTER)
		StrAllocCat(myrights,"a");

	if (myrights && *myrights)
		SetFolderRightsForUser(NULL, myrights);
	FREEIF(myrights);
}

void MSG_IMAPFolderACL::UpdateACLCache()
{
	if (GetCanIReadFolder())
		m_folder->SetFolderPrefFlags(m_folder->GetFolderPrefFlags() | MSG_FOLDER_PREF_IMAP_ACL_READ);
	else
		m_folder->SetFolderPrefFlags(m_folder->GetFolderPrefFlags() & ~MSG_FOLDER_PREF_IMAP_ACL_READ);

	if (GetCanIStoreSeenInFolder())
		m_folder->SetFolderPrefFlags(m_folder->GetFolderPrefFlags() | MSG_FOLDER_PREF_IMAP_ACL_STORE_SEEN);
	else
		m_folder->SetFolderPrefFlags(m_folder->GetFolderPrefFlags() & ~MSG_FOLDER_PREF_IMAP_ACL_STORE_SEEN);

	if (GetCanIWriteFolder())
		m_folder->SetFolderPrefFlags(m_folder->GetFolderPrefFlags() | MSG_FOLDER_PREF_IMAP_ACL_WRITE);
	else
		m_folder->SetFolderPrefFlags(m_folder->GetFolderPrefFlags() & ~MSG_FOLDER_PREF_IMAP_ACL_WRITE);

	if (GetCanIInsertInFolder())
		m_folder->SetFolderPrefFlags(m_folder->GetFolderPrefFlags() | MSG_FOLDER_PREF_IMAP_ACL_INSERT);
	else
		m_folder->SetFolderPrefFlags(m_folder->GetFolderPrefFlags() & ~MSG_FOLDER_PREF_IMAP_ACL_INSERT);

	if (GetCanIPostToFolder())
		m_folder->SetFolderPrefFlags(m_folder->GetFolderPrefFlags() | MSG_FOLDER_PREF_IMAP_ACL_POST);
	else
		m_folder->SetFolderPrefFlags(m_folder->GetFolderPrefFlags() & ~MSG_FOLDER_PREF_IMAP_ACL_POST);

	if (GetCanICreateSubfolder())
		m_folder->SetFolderPrefFlags(m_folder->GetFolderPrefFlags() | MSG_FOLDER_PREF_IMAP_ACL_CREATE_SUBFOLDER);
	else
		m_folder->SetFolderPrefFlags(m_folder->GetFolderPrefFlags() & ~MSG_FOLDER_PREF_IMAP_ACL_CREATE_SUBFOLDER);

	if (GetCanIDeleteInFolder())
		m_folder->SetFolderPrefFlags(m_folder->GetFolderPrefFlags() | MSG_FOLDER_PREF_IMAP_ACL_DELETE);
	else
		m_folder->SetFolderPrefFlags(m_folder->GetFolderPrefFlags() & ~MSG_FOLDER_PREF_IMAP_ACL_DELETE);

	if (GetCanIAdministerFolder())
		m_folder->SetFolderPrefFlags(m_folder->GetFolderPrefFlags() | MSG_FOLDER_PREF_IMAP_ACL_ADMINISTER);
	else
		m_folder->SetFolderPrefFlags(m_folder->GetFolderPrefFlags() & ~MSG_FOLDER_PREF_IMAP_ACL_ADMINISTER);
}

XP_Bool MSG_IMAPFolderACL::SetFolderRightsForUser(const char *userName, const char *rights)
{
	XP_Bool rv = FALSE;
	const char *myUserName = m_folder->GetIMAPHost()->GetUserName();
	char *ourUserName = NULL;

	if (!userName)
		ourUserName = XP_STRDUP(myUserName);
	else
		ourUserName = XP_STRDUP(userName);

	char *rightsWeOwn = XP_STRDUP(rights);
	if (rightsWeOwn && ourUserName)
	{
		char *oldValue = (char *)XP_Gethash(m_rightsHash, ourUserName, NULL);
		if (oldValue)
		{
			XP_FREE(oldValue);
			XP_Remhash(m_rightsHash, ourUserName);
			m_aclCount--;
			XP_ASSERT(m_aclCount >= 0);
		}
		m_aclCount++;
		rv = (XP_Puthash(m_rightsHash, ourUserName, rightsWeOwn) == 0);

	}

	if (ourUserName && 
		(!XP_STRCMP(ourUserName, myUserName) || !XP_STRCMP(ourUserName, IMAP_ACL_ANYONE_STRING)))
	{
		// if this is setting an ACL for me, cache it in the folder pref flags
		UpdateACLCache();
	}

	return rv;
}

const char *MSG_IMAPFolderACL::GetRightsStringForUser(const char *userName)
{
	if (!userName)
		userName = m_folder->GetIMAPHost()->GetUserName();

	return (const char *)XP_Gethash(m_rightsHash, userName, NULL);
}

// First looks for individual user;  then looks for 'anyone' if the user isn't found.
// Returns defaultIfNotFound, if neither are found.
XP_Bool MSG_IMAPFolderACL::GetFlagSetInRightsForUser(const char *userName, char flag, XP_Bool defaultIfNotFound)
{
	const char *flags = GetRightsStringForUser(userName);
	if (!flags)
	{
		const char *anyoneFlags = GetRightsStringForUser(IMAP_ACL_ANYONE_STRING);
		if (!anyoneFlags)
			return defaultIfNotFound;
		else
			return (XP_STRCHR(anyoneFlags, flag) != NULL);
	}
	else
		return (XP_STRCHR(flags, flag) != NULL);
}

XP_Bool MSG_IMAPFolderACL::GetCanUserLookupFolder(const char *userName)
{
	return GetFlagSetInRightsForUser(userName, 'l', FALSE);
}
	
XP_Bool MSG_IMAPFolderACL::GetCanUserReadFolder(const char *userName)
{
	return GetFlagSetInRightsForUser(userName, 'r', FALSE);
}

XP_Bool	MSG_IMAPFolderACL::GetCanUserStoreSeenInFolder(const char *userName)
{
	return GetFlagSetInRightsForUser(userName, 's', FALSE);
}

XP_Bool MSG_IMAPFolderACL::GetCanUserWriteFolder(const char *userName)
{
	return GetFlagSetInRightsForUser(userName, 'w', FALSE);
}

XP_Bool	MSG_IMAPFolderACL::GetCanUserInsertInFolder(const char *userName)
{
	return GetFlagSetInRightsForUser(userName, 'i', FALSE);
}

XP_Bool	MSG_IMAPFolderACL::GetCanUserPostToFolder(const char *userName)
{
	return GetFlagSetInRightsForUser(userName, 'p', FALSE);
}

XP_Bool	MSG_IMAPFolderACL::GetCanUserCreateSubfolder(const char *userName)
{
	return GetFlagSetInRightsForUser(userName, 'c', FALSE);
}

XP_Bool	MSG_IMAPFolderACL::GetCanUserDeleteInFolder(const char *userName)
{
	return GetFlagSetInRightsForUser(userName, 'd', FALSE);
}

XP_Bool	MSG_IMAPFolderACL::GetCanUserAdministerFolder(const char *userName)
{
	return GetFlagSetInRightsForUser(userName, 'a', FALSE);
}

XP_Bool MSG_IMAPFolderACL::GetCanILookupFolder()
{
	return GetFlagSetInRightsForUser(NULL, 'l', TRUE);
}

XP_Bool MSG_IMAPFolderACL::GetCanIReadFolder()
{
	return GetFlagSetInRightsForUser(NULL, 'r', TRUE);
}

XP_Bool	MSG_IMAPFolderACL::GetCanIStoreSeenInFolder()
{
	return GetFlagSetInRightsForUser(NULL, 's', TRUE);
}

XP_Bool MSG_IMAPFolderACL::GetCanIWriteFolder()
{
	return GetFlagSetInRightsForUser(NULL, 'w', TRUE);
}

XP_Bool	MSG_IMAPFolderACL::GetCanIInsertInFolder()
{
	return GetFlagSetInRightsForUser(NULL, 'i', TRUE);
}

XP_Bool	MSG_IMAPFolderACL::GetCanIPostToFolder()
{
	return GetFlagSetInRightsForUser(NULL, 'p', TRUE);
}

XP_Bool	MSG_IMAPFolderACL::GetCanICreateSubfolder()
{
	return GetFlagSetInRightsForUser(NULL, 'c', TRUE);
}

XP_Bool	MSG_IMAPFolderACL::GetCanIDeleteInFolder()
{
	return GetFlagSetInRightsForUser(NULL, 'd', TRUE);
}

XP_Bool	MSG_IMAPFolderACL::GetCanIAdministerFolder()
{
	return GetFlagSetInRightsForUser(NULL, 'a', TRUE);
}

// We use this to see if the ACLs think a folder is shared or not.
// We will define "Shared" in 5.0 to mean:
// At least one user other than the currently authenticated user has at least one
// explicitly-listed ACL right on that folder.
XP_Bool	MSG_IMAPFolderACL::GetIsFolderShared()
{
	// If we have more than one ACL count for this folder, which means that someone
	// other than ourself has rights on it, then it is "shared."
	if (m_aclCount > 1)
		return TRUE;

	// Or, if "anyone" has rights to it, it is shared.
	const char *anyonesRights = (const char *)XP_Gethash(m_rightsHash, IMAP_ACL_ANYONE_STRING, NULL);

	return (anyonesRights != NULL);
}

XP_Bool MSG_IMAPFolderACL::GetDoIHaveFullRightsForFolder()
{
	return (GetCanIReadFolder() &&
			GetCanIWriteFolder() &&
			GetCanIInsertInFolder() &&
			GetCanIAdministerFolder() &&
			GetCanICreateSubfolder() &&
			GetCanIDeleteInFolder() &&
			GetCanILookupFolder() &&
			GetCanIStoreSeenInFolder() &&
			GetCanIPostToFolder());
}

// Returns a newly allocated string describing these rights
char *MSG_IMAPFolderACL::CreateACLRightsString()
{
	char *rv = NULL;

	if (GetDoIHaveFullRightsForFolder())
	{
		rv = PR_smprintf(XP_GetString(XP_MSG_IMAP_ACL_FULL_RIGHTS));
	}
	else
	{
		if (GetCanIReadFolder())
		{
			if (rv) StrAllocCat(rv, ", ");
			StrAllocCat(rv, XP_GetString(XP_MSG_IMAP_ACL_READ_RIGHT));
		}
		if (GetCanIWriteFolder())
		{
			if (rv) StrAllocCat(rv, ", ");
			StrAllocCat(rv, XP_GetString(XP_MSG_IMAP_ACL_WRITE_RIGHT));
		}
		if (GetCanIInsertInFolder())
		{
			if (rv) StrAllocCat(rv, ", ");
			StrAllocCat(rv, XP_GetString(XP_MSG_IMAP_ACL_INSERT_RIGHT));
		}
		if (GetCanILookupFolder())
		{
			if (rv) StrAllocCat(rv, ", ");
			StrAllocCat(rv, XP_GetString(XP_MSG_IMAP_ACL_LOOKUP_RIGHT));
		}
		if (GetCanIStoreSeenInFolder())
		{
			if (rv) StrAllocCat(rv, ", ");
			StrAllocCat(rv,  XP_GetString(XP_MSG_IMAP_ACL_SEEN_RIGHT));
		}
		if (GetCanIDeleteInFolder())
		{
			if (rv) StrAllocCat(rv, ", ");
			StrAllocCat(rv, XP_GetString(XP_MSG_IMAP_ACL_DELETE_RIGHT));
		}
		if (GetCanICreateSubfolder())
		{
			if (rv) StrAllocCat(rv, ", ");
			StrAllocCat(rv, XP_GetString(XP_MSG_IMAP_ACL_CREATE_RIGHT));
		}
		if (GetCanIPostToFolder())
		{
			if (rv) StrAllocCat(rv, ", ");
			StrAllocCat(rv, XP_GetString(XP_MSG_IMAP_ACL_POST_RIGHT));
		}
		if (GetCanIAdministerFolder())
		{
			if (rv) StrAllocCat(rv, ", ");
			StrAllocCat(rv, XP_GetString(XP_MSG_IMAP_ACL_ADMINISTER_RIGHT));
		}
	}

	return rv;
}


XP_Bool MSG_IsFolderACLInitialized(MSG_Master *master, const char *folderName, const char *hostName)
{
	MSG_IMAPFolderInfoMail *fi = master->FindImapMailFolder(hostName, folderName, NULL, FALSE);
	if (fi)
		return (fi->GetFolderPrefFlags() & MSG_FOLDER_PREF_IMAP_ACL_RETRIEVED);
	else
		return FALSE;
}

extern "C" XP_Bool
MSG_IsFolderNoSelect(MSG_Master *master, const char *folderName, const char *hostName)
{
	MSG_IMAPFolderInfoMail *fi = master->FindImapMailFolder(hostName, folderName, NULL, FALSE);
	if (fi)
		return (fi->GetFolderPrefFlags() & MSG_FOLDER_PREF_IMAPNOSELECT);
	else
		return FALSE;
}
