/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#include "msgCore.h"
#include "prprf.h"

#include <stdio.h>
#include <assert.h>

#ifdef XP_PC
#include <windows.h>
#endif

#include "plstr.h"
#include "plevent.h"

#include "nsIURL.h"
#include "nsIServiceManager.h"
#include "nsIComponentManager.h"
#include "nsString.h"

#include "nsIUrlListener.h"

#include "nsIPref.h"
#include "nsIImapUrl.h"
#include "nsIImapProtocol.h"
#include "nsIImapLog.h"
#include "nsIMsgIncomingServer.h"
#include "nsIImapService.h"
#include "nsIMsgMailSession.h"
#include "nsIImapLog.h"
#include "nsIImapMailFolderSink.h"
#include "nsIImapMessageSink.h"
#include "nsIImapExtensionSink.h"
#include "nsIImapMiscellaneousSink.h"

#include "nsIEventQueueService.h"
#include "nsXPComCIID.h"
#include "nsFileSpec.h"
#include "nsMsgDBCID.h"

#include "nsMsgDatabase.h"
#include "nsLocalFolderSummarySpec.h"
#include "nsImapFlagAndUidState.h"
#include "nsParseMailbox.h"

#ifdef XP_PC
#define NETLIB_DLL "netlib.dll"
#define XPCOM_DLL  "xpcom32.dll"
#define PREF_DLL	 "xppref32.dll"
#define MSGIMAP_DLL "msgimap.dll"
#else
#ifdef XP_MAC
#include "nsMacRepository.h"
#else
#define NETLIB_DLL "libnetlib.so"
#define XPCOM_DLL  "libxpcom.so"
#define PREF_DLL	 "pref.so"   // mscott: is this right?
#define MSGIMAP_DLL "libmsgimap.so"
#endif
#endif

// this is only needed as long as our libmime hack is in place
#include "prio.h"

#ifdef XP_UNIX
#define ARTICLE_PATH "/usr/tmp/tempArticle.eml"
#define ARTICLE_PATH_URL ARTICLE_PATH
#endif

#ifdef XP_PC
#define ARTICLE_PATH  "c:\\temp\\tempArticle.eml"
#define ARTICLE_PATH_URL "C|/temp/tempArticle.eml"
#endif

/////////////////////////////////////////////////////////////////////////////////
// Define keys for all of the interfaces we are going to require for this test
/////////////////////////////////////////////////////////////////////////////////

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_CID(kImapUrlCID, NS_IMAPURL_CID);
static NS_DEFINE_CID(kImapProtocolCID, NS_IMAPPROTOCOL_CID);
static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);
static NS_DEFINE_CID(kCImapService, NS_IMAPSERVICE_CID);
static NS_DEFINE_CID(kCImapDB, NS_IMAPDB_CID);


/////////////////////////////////////////////////////////////////////////////////
// Define default values to be used to drive the test
/////////////////////////////////////////////////////////////////////////////////

#define DEFAULT_HOST		"nsmail-2.mcom.com"
#define DEFAULT_PORT		IMAP_PORT
#define DEFAULT_URL_TYPE	"imap://"	/* do NOT change this value until netlib re-write is done...*/

class nsIMAP4TestDriver  : public nsIUrlListener, 
                           public nsIImapLog,
                           public nsIImapMailFolderSink,
                           public nsIImapMessageSink,
                           public nsIImapExtensionSink, 
                           public nsIImapMiscellaneousSink
{
public:
	NS_DECL_ISUPPORTS

	// nsIUrlListener support
	NS_IMETHOD OnStartRunningUrl(nsIURL * aUrl);
	NS_IMETHOD OnStopRunningUrl(nsIURL * aUrl, nsresult aExitCode);

	// nsIImapLog support
	NS_IMETHOD HandleImapLogData (const char * aLogData);

    // nsIImapMailFolderSink support
    NS_IMETHOD PossibleImapMailbox(nsIImapProtocol* aProtocol,
                                   mailbox_spec* aSpec);
    NS_IMETHOD MailboxDiscoveryDone(nsIImapProtocol* aProtocol);
    // Tell mail master about the newly selected mailbox
    NS_IMETHOD UpdateImapMailboxInfo(nsIImapProtocol* aProtocol,
                                     mailbox_spec* aSpec);
    NS_IMETHOD UpdateImapMailboxStatus(nsIImapProtocol* aProtocol,
                                       mailbox_spec* aSpec);
    NS_IMETHOD ChildDiscoverySucceeded(nsIImapProtocol* aProtocol);
    NS_IMETHOD OnlineFolderDelete(nsIImapProtocol* aProtocol,
                                  const char* folderName);
    NS_IMETHOD OnlineFolderCreateFailed(nsIImapProtocol* aProtocol,
                                        const char* folderName);
    NS_IMETHOD OnlineFolderRename(nsIImapProtocol* aProtocol,
                                  folder_rename_struct* aStruct);
    NS_IMETHOD SubscribeUpgradeFinished(nsIImapProtocol* aProtocol,
                              EIMAPSubscriptionUpgradeState* aState);
    NS_IMETHOD PromptUserForSubscribeUpdatePath(nsIImapProtocol* aProtocol,
                                                PRBool* aBool);
    NS_IMETHOD FolderIsNoSelect(nsIImapProtocol* aProtocol,
                                FolderQueryInfo* aInfo);
    
    NS_IMETHOD SetupHeaderParseStream(nsIImapProtocol* aProtocol,
                                   StreamInfo* aStreamInfo);
    
    NS_IMETHOD ParseAdoptedHeaderLine(nsIImapProtocol* aProtocol,
                                   msg_line_info* aMsgLineInfo);
    
    NS_IMETHOD NormalEndHeaderParseStream(nsIImapProtocol* aProtocol);
    
    NS_IMETHOD AbortHeaderParseStream(nsIImapProtocol* aProtocol);
    
    // nsIImapMessageSink support
    NS_IMETHOD SetupMsgWriteStream(nsIImapProtocol* aProtocol,
                                   StreamInfo* aStreamInfo);
    
    NS_IMETHOD ParseAdoptedMsgLine(nsIImapProtocol* aProtocol,
                                   msg_line_info* aMsgLineInfo);
    
    NS_IMETHOD NormalEndMsgWriteStream(nsIImapProtocol* aProtocol);
    
    NS_IMETHOD AbortMsgWriteStream(nsIImapProtocol* aProtocol);
    
    // message move/copy related methods
    NS_IMETHOD OnlineCopyReport(nsIImapProtocol* aProtocol,
                                ImapOnlineCopyState* aCopyState);
    NS_IMETHOD BeginMessageUpload(nsIImapProtocol* aProtocol);
    NS_IMETHOD UploadMessageFile(nsIImapProtocol* aProtocol,
                                 UploadMessageInfo* aMsgInfo);

    // message flags operation
    NS_IMETHOD NotifyMessageFlags(nsIImapProtocol* aProtocol,
                                  FlagsKeyStruct* aKeyStruct);

    NS_IMETHOD NotifyMessageDeleted(nsIImapProtocol* aProtocol,
                                    delete_message_struct* aStruct);
    NS_IMETHOD GetMessageSizeFromDB(nsIImapProtocol* aProtocol,
                                    MessageSizeInfo* sizeInfo);

    // nsIImapExtensionSink support
  
    NS_IMETHOD SetUserAuthenticated(nsIImapProtocol* aProtocol,
								  PRBool aBool);
    NS_IMETHOD SetMailServerUrls(nsIImapProtocol* aProtocol,
                                 const char* hostName);
    NS_IMETHOD SetMailAccountUrl(nsIImapProtocol* aProtocol,
                                 const char* hostName);
    NS_IMETHOD ClearFolderRights(nsIImapProtocol* aProtocol,
                                 nsIMAPACLRightsInfo* aclRights);
    NS_IMETHOD AddFolderRights(nsIImapProtocol* aProtocol,
                               nsIMAPACLRightsInfo* aclRights);
    NS_IMETHOD RefreshFolderRights(nsIImapProtocol* aProtocol,
                                   nsIMAPACLRightsInfo* aclRights);
    NS_IMETHOD FolderNeedsACLInitialized(nsIImapProtocol* aProtocol,
                                         nsIMAPACLRightsInfo* aclRights);
    NS_IMETHOD SetFolderAdminURL(nsIImapProtocol* aProtocol,
                                 FolderQueryInfo* aInfo);
    
    // nsIImapMiscellaneousSink support
	
	NS_IMETHOD AddSearchResult(nsIImapProtocol* aProtocol, 
                               const char* searchHitLine);
	NS_IMETHOD GetArbitraryHeaders(nsIImapProtocol* aProtocol,
                                   GenericInfo* aInfo);
	NS_IMETHOD GetShouldDownloadArbitraryHeaders(nsIImapProtocol* aProtocol,
                                                 GenericInfo* aInfo);
    NS_IMETHOD GetShowAttachmentsInline(nsIImapProtocol* aProtocol,
                                        PRBool* aBool);
	NS_IMETHOD HeaderFetchCompleted(nsIImapProtocol* aProtocol);
	NS_IMETHOD UpdateSecurityStatus(nsIImapProtocol* aProtocol);
	// ****
	NS_IMETHOD FinishImapConnection(nsIImapProtocol* aProtocol);
	NS_IMETHOD SetImapHostPassword(nsIImapProtocol* aProtocol,
                                   GenericInfo* aInfo);
	NS_IMETHOD GetPasswordForUser(nsIImapProtocol* aProtocol,
                                  const char* userName);
	NS_IMETHOD SetBiffStateAndUpdate(nsIImapProtocol* aProtocol,
                                     nsMsgBiffState biffState);
	NS_IMETHOD GetStoredUIDValidity(nsIImapProtocol* aProtocol,
                                    uid_validity_info* aInfo);
	NS_IMETHOD LiteSelectUIDValidity(nsIImapProtocol* aProtocol,
                                     PRUint32 uidValidity);
	NS_IMETHOD FEAlert(nsIImapProtocol* aProtocol,
                       const char* aString);
	NS_IMETHOD FEAlertFromServer(nsIImapProtocol* aProtocol,
                                 const char* aString);
	NS_IMETHOD ProgressStatus(nsIImapProtocol* aProtocol,
                              const char* statusMsg);
	NS_IMETHOD PercentProgress(nsIImapProtocol* aProtocol,
                               ProgressInfo* aInfo);
	NS_IMETHOD PastPasswordCheck(nsIImapProtocol* aProtocol);
	NS_IMETHOD CommitNamespaces(nsIImapProtocol* aProtocol,
                                const char* hostName);
	NS_IMETHOD CommitCapabilityForHost(nsIImapProtocol* aProtocol,
                                       const char* hostName);
	NS_IMETHOD TunnelOutStream(nsIImapProtocol* aProtocol,
                               msg_line_info* aInfo);
	NS_IMETHOD ProcessTunnel(nsIImapProtocol* aProtocol,
                             TunnelInfo *aInfo);

	nsIMAP4TestDriver(PLEventQueue *queue);
	virtual ~nsIMAP4TestDriver();

	// run driver initializes the instance, lists the commands, runs the command and when
	// the command is finished, it reads in the next command and continues...theoretically,
	// the client should only ever have to call RunDriver(). It should do the rest of the 
	// work....
	nsresult RunDriver(); 

	// User drive commands
	nsresult ListCommands();   // will list all available commands to the user...i.e. "get groups, get article, etc."
	nsresult ReadAndDispatchCommand(); // reads a command number in from the user and calls the appropriate command generator
	nsresult PromptForUserData(const char * userPrompt);

	// command handlers
	nsresult OnCommand();   // send a command to the imap server
	nsresult OnRunIMAPCommand();
	nsresult OnGet();
	nsresult OnIdentityCheck();
	nsresult OnTestUrlParsing();
	nsresult OnSelectFolder();
	nsresult OnFetchMessage();
	nsresult OnExit(); 
protected:
	char m_command[500];	// command to run
	char m_urlString[500];	// string representing the current url being run. Includes host AND command specific data.
	char m_userData[250];	// generic string buffer for storing the current user entered data...
	char m_urlSpec[200];	// "imap://hostname:port/" it does not include the command specific data...

	// host and port info...
	PRUint32	m_port;
	char		m_host[200];		

	nsIImapUrl * m_url; 
	nsIImapProtocol * m_IMAP4Protocol; // running protocol instance
	nsParseMailMessageState *m_msgParser ;
	nsMsgKey			m_curMsgUid;
	PRInt32			m_nextMessageByteLength;
	nsIMsgDatabase * m_mailDB ;
	PRBool	    m_runTestHarness;
	PRBool		m_runningURL;	// are we currently running a url? this flag is set to false when the url finishes...

	// part of temporary libmime converstion trick......these should go away once MIME uses a new stream
	// converter interface...
	PRFileDesc* m_tempArticleFile;


	nsresult InitializeProtocol(const char * urlSpec);
	PRBool m_protocolInitialized; 
    PLEventQueue *m_eventQueue;

	void FindKeysToAdd(const nsMsgKeyArray &existingKeys, nsMsgKeyArray &keysToFetch, nsImapFlagAndUidState *flagState);
	void FindKeysToDelete(const nsMsgKeyArray &existingKeys, nsMsgKeyArray &keysToFetch, nsImapFlagAndUidState *flagState);
	void PrepareToAddHeadersToMailDB(nsIImapProtocol* aProtocol, const nsMsgKeyArray &keysToFetch, mailbox_spec *boxSpec);
	void TweakHeaderFlags(nsIImapProtocol* aProtocol, nsIMessage *tweakMe);

};

nsIMAP4TestDriver::nsIMAP4TestDriver(PLEventQueue *queue)
{
	NS_INIT_REFCNT();
	m_urlSpec[0] = '\0';
	m_urlString[0] = '\0';
	m_url = nsnull;
	m_protocolInitialized = PR_FALSE;
	m_runTestHarness = PR_TRUE;
	m_runningURL = PR_FALSE;
    m_eventQueue = queue;

	m_IMAP4Protocol = nsnull; // we can't create it until we have a url...
	m_msgParser = nsnull;
	m_mailDB = nsnull;
	m_curMsgUid = nsMsgKey_None;

	// until we read from the prefs, just use the default, I guess.
	strcpy(m_urlSpec, DEFAULT_URL_TYPE);
	strcat(m_urlSpec, DEFAULT_HOST);
	strcat(m_urlSpec, "/");
}

NS_IMPL_ADDREF(nsIMAP4TestDriver)
NS_IMPL_RELEASE(nsIMAP4TestDriver)

nsresult 
nsIMAP4TestDriver::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
    if (nsnull == aInstancePtr)
        return NS_ERROR_NULL_POINTER;

    *aInstancePtr = nsnull;

    if (aIID.Equals(nsIUrlListener::GetIID()))
    {
        *aInstancePtr = (void*)(nsIUrlListener*)this;
    }
    else if (aIID.Equals(nsIImapLog::GetIID()))
    {
        *aInstancePtr = (void*)(nsIImapLog*)this;
    }
    else if (aIID.Equals(nsIImapMailFolderSink::GetIID()))
    {
        *aInstancePtr = (void*)(nsIImapMailFolderSink*)this;
    }
    else if (aIID.Equals(nsIImapMessageSink::GetIID()))
    {
        *aInstancePtr = (void*)(nsIImapMessageSink*)this;
    }
    else if (aIID.Equals(nsIImapExtensionSink::GetIID()))
    {
        *aInstancePtr = (void*)(nsIImapExtensionSink*)this;
    }
    else if (aIID.Equals(nsIImapMiscellaneousSink::GetIID()))
    {
        *aInstancePtr = (void*)(nsIImapMiscellaneousSink*)this;
    }
    else if (aIID.Equals(kISupportsIID))
    {
        *aInstancePtr = (void*)(nsISupports*)(nsIUrlListener*)this;
    }
    else
        return NS_NOINTERFACE;

    NS_ADDREF_THIS();
    return NS_OK;
}

    // nsIImapMailFolderSink support
NS_IMETHODIMP
nsIMAP4TestDriver::PossibleImapMailbox(nsIImapProtocol* aProtocol,
                                       mailbox_spec* aSpec)
{
	printf("We found folder on host %s with name %s.\n", aSpec->hostName ? aSpec->hostName : "", aSpec->allocatedPathName ? aSpec->allocatedPathName : "");
    return NS_OK;
}

NS_IMETHODIMP
nsIMAP4TestDriver::MailboxDiscoveryDone(nsIImapProtocol* aProtocol)
{
    printf("**** nsIMAP4TestDriver::MailboxDiscoveryDone\r\n");
    return NS_OK;
}

// Tell msglib about the newly selected mailbox
NS_IMETHODIMP
nsIMAP4TestDriver::UpdateImapMailboxInfo(nsIImapProtocol* aProtocol,
                                     mailbox_spec* aSpec)
{
    printf("**** nsIMAP4TestDriver::UpdateImapMailboxInfo\r\n");

	nsIMsgDatabase * mailDBFactory;
	nsresult rv = nsComponentManager::CreateInstance(kCImapDB, nsnull, nsIMsgDatabase::GetIID(), (void **) &mailDBFactory);
	nsString	pathName = "/tmp/";
	pathName+= aSpec->allocatedPathName;
	nsFileSpec dbName(pathName);

	if (NS_SUCCEEDED(rv) && mailDBFactory)
	{
		// if we pass in PR_TRUE for upgrading, the db code will ignore the summary out of date problem
		// for now.
		rv = mailDBFactory->Open(dbName, PR_TRUE, (nsIMsgDatabase **) &m_mailDB, PR_TRUE);
	}
//	rv = nsMailDatabase::Open(folder, PR_TRUE, &m_mailDB, PR_FALSE);
    if (NS_FAILED(rv)) 
	{
		if (mailDBFactory)
			NS_RELEASE(mailDBFactory);
		return rv;
	}
    
    if (aSpec->folderSelected)
    {
    	nsMsgKeyArray existingKeys;
    	nsMsgKeyArray keysToDelete;
    	nsMsgKeyArray keysToFetch;
		nsIDBFolderInfo *dbFolderInfo = nsnull;
		PRInt32 imapUIDValidity = 0;

		rv = m_mailDB->GetDBFolderInfo(&dbFolderInfo);

		if (NS_SUCCEEDED(rv) && dbFolderInfo)
			dbFolderInfo->GetImapUidValidity(&imapUIDValidity);
    	m_mailDB->ListAllKeys(existingKeys);
    	if (m_mailDB->ListAllOfflineDeletes(&existingKeys) > 0)
			existingKeys.QuickSort();
    	if ((imapUIDValidity != aSpec->folder_UIDVALIDITY)	/* &&	// if UIDVALIDITY Changed 
    		!NET_IsOffline() */)
    	{

			nsIMsgDatabase *saveMailDB = m_mailDB;
#if TRANSFER_INFO
			TNeoFolderInfoTransfer *originalInfo = NULL;
			originalInfo = new TNeoFolderInfoTransfer(dbFolderInfo);
#endif // 0
			m_mailDB->ForceClosed();
			m_mailDB = NULL;
				
			nsLocalFolderSummarySpec	summarySpec(dbName);
			// Remove summary file.
			summarySpec.Delete(PR_FALSE);
			
			// Create a new summary file, update the folder message counts, and
			// Close the summary file db.
			rv = mailDBFactory->Open(dbName, PR_TRUE, &m_mailDB, PR_FALSE);
			if (NS_SUCCEEDED(rv))
			{
#if TRANSFER_INFO
				if (originalInfo)
				{
					originalInfo->TransferFolderInfo(*m_mailDB->m_dbFolderInfo);
					delete originalInfo;
				}
				SummaryChanged();
#endif
			}
			// store the new UIDVALIDITY value
			rv = m_mailDB->GetDBFolderInfo(&dbFolderInfo);

			if (NS_SUCCEEDED(rv) && dbFolderInfo)
    			dbFolderInfo->SetImapUidValidity(aSpec->folder_UIDVALIDITY);
    										// delete all my msgs, the keys are bogus now
											// add every message in this folder
			existingKeys.RemoveAll();
//			keysToDelete.CopyArray(&existingKeys);

			if (aSpec->flagState)
			{
				nsMsgKeyArray no_existingKeys;
	  			FindKeysToAdd(no_existingKeys, keysToFetch, aSpec->flagState);
    		}
    	}		
    	else if (!aSpec->flagState /*&& !NET_IsOffline() */)	// if there are no messages on the server
    	{
			keysToDelete.CopyArray(&existingKeys);
    	}
    	else /* if ( !NET_IsOffline()) */
    	{
    		FindKeysToDelete(existingKeys, keysToDelete, aSpec->flagState);

			// if this is the result of an expunge then don't grab headers
			if (!(aSpec->box_flags & kJustExpunged))
				FindKeysToAdd(existingKeys, keysToFetch, aSpec->flagState);
    	}
    	
    	
    	if (keysToDelete.GetSize())
    	{
			PRUint32 total;

    		PRBool highWaterDeleted = FALSE;
			// It would be nice to notify RDF or whoever of a mass delete here.
    		m_mailDB->DeleteMessages(&keysToDelete,NULL);
			total = keysToDelete.GetSize();
			nsMsgKey highWaterMark = nsMsgKey_None;
		}
	   	if (keysToFetch.GetSize())
    	{			
            PrepareToAddHeadersToMailDB(aProtocol, keysToFetch, aSpec);
			if (aProtocol)
				aProtocol->NotifyBodysToDownload(NULL, 0/*keysToFetch.GetSize() */);
    	}
    	else 
    	{
            // let the imap libnet module know that we don't need headers
			if (aProtocol)
				aProtocol->NotifyHdrsToDownload(NULL, 0);
			// wait until we can get body id monitor before continuing.
//			IMAP_BodyIdMonitor(adoptedBoxSpec->connection, TRUE);
			// I think the real fix for this is to seperate the header ids from body id's.
			// this is for fetching bodies for offline use
			if (aProtocol)
				aProtocol->NotifyBodysToDownload(NULL, 0/*keysToFetch.GetSize() */);
//			NotifyFetchAnyNeededBodies(aSpec->connection, mailDB);
//			IMAP_BodyIdMonitor(adoptedBoxSpec->connection, FALSE);
    	}
    }


    if (NS_FAILED(rv))

    {
        dbName.Delete(PR_FALSE);
    }
 	if (mailDBFactory)
		NS_RELEASE(mailDBFactory);
    return NS_OK;
}


// both of these algorithms assume that key arrays and flag states are sorted by increasing key.
void nsIMAP4TestDriver::FindKeysToDelete(const nsMsgKeyArray &existingKeys, nsMsgKeyArray &keysToDelete, nsImapFlagAndUidState *flagState)
{
	PRBool imapDeleteIsMoveToTrash = /* DeleteIsMoveToTrash() */ PR_TRUE;
	PRUint32 total = existingKeys.GetSize();
	PRInt32 index;

	int onlineIndex=0; // current index into flagState
	for (PRUint32 keyIndex=0; keyIndex < total; keyIndex++)
	{
		PRUint32 uidOfMessage;

		flagState->GetNumberOfMessages(&index);
		while ((onlineIndex < index) && 
			   (flagState->GetUidOfMessage(onlineIndex, &uidOfMessage), (existingKeys[keyIndex] > uidOfMessage) ))
		{
			onlineIndex++;
		}
		
		imapMessageFlagsType flags;
		flagState->GetUidOfMessage(onlineIndex, &uidOfMessage);
		flagState->GetMessageFlags(onlineIndex, &flags);
		// delete this key if it is not there or marked deleted
		if ( (onlineIndex >= index ) ||
			 (existingKeys[keyIndex] != uidOfMessage) ||
			 ((flags & kImapMsgDeletedFlag) && imapDeleteIsMoveToTrash) )
		{
			nsMsgKey doomedKey = existingKeys[keyIndex];
			if ((PRInt32) doomedKey < 0 && doomedKey != nsMsgKey_None)
				continue;
			else
				keysToDelete.Add(existingKeys[keyIndex]);
		}
		
		flagState->GetUidOfMessage(onlineIndex, &uidOfMessage);
		if (existingKeys[keyIndex] == uidOfMessage) 
			onlineIndex++;
	}
}

void nsIMAP4TestDriver::FindKeysToAdd(const nsMsgKeyArray &existingKeys, nsMsgKeyArray &keysToFetch, nsImapFlagAndUidState *flagState)
{
	PRBool showDeletedMessages = PR_FALSE /* ShowDeletedMessages() */;

	int dbIndex=0; // current index into existingKeys
	PRInt32 existTotal, numberOfKnownKeys;
	PRInt32 index;
	
	existTotal = numberOfKnownKeys = existingKeys.GetSize();
	flagState->GetNumberOfMessages(&index);
	for (PRInt32 flagIndex=0; flagIndex < index; flagIndex++)
	{
		PRUint32 uidOfMessage;
		flagState->GetUidOfMessage(flagIndex, &uidOfMessage);
		while ( (flagIndex < numberOfKnownKeys) && (dbIndex < existTotal) &&
				existingKeys[dbIndex] < uidOfMessage) 
			dbIndex++;
		
		if ( (flagIndex >= numberOfKnownKeys)  || 
			 (dbIndex >= existTotal) ||
			 (existingKeys[dbIndex] != uidOfMessage ) )
		{
			numberOfKnownKeys++;

			imapMessageFlagsType flags;
			flagState->GetMessageFlags(flagIndex, &flags);
			if (showDeletedMessages || ! (flags & kImapMsgDeletedFlag))
			{
				keysToFetch.Add(uidOfMessage);
			}
		}
	}
}

void nsIMAP4TestDriver::PrepareToAddHeadersToMailDB(nsIImapProtocol* aProtocol, const nsMsgKeyArray &keysToFetch,
                                                mailbox_spec *boxSpec)
{
    PRUint32 *theKeys = (PRUint32 *) PR_Malloc( keysToFetch.GetSize() * sizeof(PRUint32) );
    if (theKeys)
    {
		PRUint32 total = keysToFetch.GetSize();

        for (int keyIndex=0; keyIndex < total; keyIndex++)
        	theKeys[keyIndex] = keysToFetch[keyIndex];
        
//        m_DownLoadState = kDownLoadingAllMessageHeaders;

        nsresult res = NS_OK; /*ImapMailDB::Open(m_pathName,
                                         TRUE, // create if necessary
                                         &mailDB,
                                         m_master,
                                         &dbWasCreated); */

		// don't want to download headers in a composition pane
        if (NS_SUCCEEDED(res))
        {
#if 0
			SetParseMailboxState(new ParseIMAPMailboxState(m_master, m_host, this,
														   urlQueue,
														   boxSpec->flagState));
	        boxSpec->flagState = NULL;		// adopted by ParseIMAPMailboxState
			GetParseMailboxState()->SetPane(url_pane);

            GetParseMailboxState()->SetDB(mailDB);
            GetParseMailboxState()->SetIncrementalUpdate(TRUE);
	        GetParseMailboxState()->SetMaster(m_master);
	        GetParseMailboxState()->SetContext(url_pane->GetContext());
	        GetParseMailboxState()->SetFolder(this);
	        
	        GetParseMailboxState()->BeginParsingFolder(0);
#endif // 0 hook up parsing later.
	        // the imap libnet module will start downloading message headers imap.h
			if (aProtocol)
				aProtocol->NotifyHdrsToDownload(theKeys, total /*keysToFetch.GetSize() */);
			// now, tell it we don't need any bodies.
			if (aProtocol)
				aProtocol->NotifyBodysToDownload(NULL, 0);
        }
        else
        {
			if (aProtocol)
				aProtocol->NotifyHdrsToDownload(NULL, 0);
        }
    }
}


NS_IMETHODIMP
nsIMAP4TestDriver::UpdateImapMailboxStatus(nsIImapProtocol* aProtocol,
                                       mailbox_spec* aSpec)
{
    printf("**** nsIMAP4TestDriver::UpdateImapMailboxStatus\r\n");
    return NS_OK;
}

NS_IMETHODIMP
nsIMAP4TestDriver::ChildDiscoverySucceeded(nsIImapProtocol* aProtocol)
{
    printf("**** nsIMAP4TestDriver::ChildDiscoverySucceeded\r\n");
    return NS_OK;
}

NS_IMETHODIMP
nsIMAP4TestDriver::OnlineFolderDelete(nsIImapProtocol* aProtocol,
                                  const char* folderName)
{
    printf("**** nsIMAP4TestDriver::OnlineFolderDelete\r\n");
    return NS_OK;
}

NS_IMETHODIMP
nsIMAP4TestDriver::OnlineFolderCreateFailed(nsIImapProtocol* aProtocol,
                                        const char* folderName)
{
    printf("**** nsIMAP4TestDriver::OnlineFolderCreateFailed\r\n");
    return NS_OK;
}

NS_IMETHODIMP
nsIMAP4TestDriver::OnlineFolderRename(nsIImapProtocol* aProtocol,
                                  folder_rename_struct* aStruct)
{
    printf("**** nsIMAP4TestDriver::OnlineFolderRename\r\n");
    return NS_OK;
}

NS_IMETHODIMP
nsIMAP4TestDriver::SubscribeUpgradeFinished(nsIImapProtocol* aProtocol,
                              EIMAPSubscriptionUpgradeState* aState)
{
    printf("**** nsIMAP4TestDriver::SubscribeUpgradeFinished\r\n");
    return NS_OK;
}

NS_IMETHODIMP
nsIMAP4TestDriver::PromptUserForSubscribeUpdatePath(nsIImapProtocol* aProtocol,
                                                PRBool* aBool)
{
    printf("**** nsIMAP4TestDriver::PromptUserForSubscribeUpdatePath\r\n");
    return NS_OK;
}

NS_IMETHODIMP
nsIMAP4TestDriver::FolderIsNoSelect(nsIImapProtocol* aProtocol,
                                FolderQueryInfo* aInfo)
{
    printf("**** nsIMAP4TestDriver::FolderIsNoSelect\r\n");
    return NS_OK;
}

NS_IMETHODIMP nsIMAP4TestDriver::SetupHeaderParseStream(nsIImapProtocol* aProtocol,
                               StreamInfo* aStreamInfo)
{
    printf("**** nsIMAP4TestDriver::SetupHeaderParseStream\r\n");

	m_nextMessageByteLength = aStreamInfo->size;
	if (!m_msgParser)
	{
		m_msgParser = new nsParseMailMessageState;
		m_msgParser->SetMailDB(m_mailDB);
	}
	else
		m_msgParser->Clear();
	if (m_msgParser)
	{
		m_msgParser->m_state =  MBOX_PARSE_HEADERS;           
		return NS_OK;
	}
	else
		return NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP nsIMAP4TestDriver::ParseAdoptedHeaderLine(nsIImapProtocol* aProtocol,
                               msg_line_info* aMsgLineInfo)
{
    printf("**** nsIMAP4TestDriver::ParseAdoptedHeaderLine\r\n");
   // we can get blocks that contain more than one line, 
    // but they never contain partial lines
	char *str = aMsgLineInfo->adoptedMessageLine;
	m_curMsgUid = aMsgLineInfo->uidOfMessage;
	m_msgParser->m_envelope_pos = m_curMsgUid;	// OK, this is silly (but we'll fix it). m_envelope_pos, for local folders,
												// is the msg key. Setting this will set the msg key for the new header.

	PRInt32 len = nsCRT::strlen(str);
    char *currentEOL  = PL_strstr(str, LINEBREAK);
    const char *currentLine = str;
    while (currentLine < (str + len))
    {
        if (currentEOL)
        {
            m_msgParser->ParseFolderLine(currentLine, (currentEOL + LINEBREAK_LEN) - currentLine);
            currentLine = currentEOL + LINEBREAK_LEN;
            currentEOL  = XP_STRSTR(currentLine, LINEBREAK);
        }
        else
        {
			m_msgParser->ParseFolderLine(currentLine, PL_strlen(currentLine));
            currentLine = str + len + 1;
        }
    }
    return NS_OK;
}


NS_IMETHODIMP nsIMAP4TestDriver::NormalEndHeaderParseStream(nsIImapProtocol* aProtocol)
{
    printf("**** nsIMAP4TestDriver::NormalEndHeaderParseStream\r\n");
	if (m_msgParser && m_msgParser->m_newMsgHdr)
	{
		m_msgParser->m_newMsgHdr->SetMessageKey(m_curMsgUid);
		TweakHeaderFlags(aProtocol, m_msgParser->m_newMsgHdr);
		// here we need to tweak flags from uid state..
		m_mailDB->AddNewHdrToDB(m_msgParser->m_newMsgHdr, PR_TRUE);
		m_msgParser->FinishHeader();
		if (m_mailDB)
			m_mailDB->Commit(kLargeCommit);	// don't really want to do this for every message...
											// but I can't find the event that means we've finished getting headers
	}
    return NS_OK;
}

NS_IMETHODIMP nsIMAP4TestDriver::AbortHeaderParseStream(nsIImapProtocol* aProtocol)
{
    printf("**** nsIMAP4TestDriver::AbortHeaderParseStream\r\n");
    return NS_OK;
}

void nsIMAP4TestDriver::TweakHeaderFlags(nsIImapProtocol* aProtocol, nsIMessage *tweakMe)
{
	if (m_mailDB && aProtocol && tweakMe)
	{
		tweakMe->SetMessageKey(m_curMsgUid);
		tweakMe->SetMessageSize(m_nextMessageByteLength);
		
		PRBool foundIt = FALSE;
		imapMessageFlagsType imap_flags;
		nsresult res = aProtocol->GetFlagsForUID(m_curMsgUid, &foundIt, &imap_flags);
		if (NS_SUCCEEDED(res) && foundIt)
		{
			// make a mask and clear these message flags
			PRUint32 mask = MSG_FLAG_READ | MSG_FLAG_REPLIED | MSG_FLAG_MARKED | MSG_FLAG_IMAP_DELETED;
			PRUint32 dbHdrFlags;

			tweakMe->GetFlags(&dbHdrFlags);
			tweakMe->AndFlags(~mask, &dbHdrFlags);
			
			// set the new value for these flags
			PRUint32 newFlags = 0;
			if (imap_flags & kImapMsgSeenFlag)
				newFlags |= MSG_FLAG_READ;
			else // if (imap_flags & kImapMsgRecentFlag)
				newFlags |= MSG_FLAG_NEW;

			// Okay here is the MDN needed logic (if DNT header seen):
			/* if server support user defined flag:
					MDNSent flag set => clear kMDNNeeded flag
					MDNSent flag not set => do nothing, leave kMDNNeeded on
			   else if 
					not MSG_FLAG_NEW => clear kMDNNeeded flag
					MSG_FLAG_NEW => do nothing, leave kMDNNeeded on
			 */
			PRUint16 userFlags;
			nsresult res = aProtocol->GetSupportedUserFlags(&userFlags);
			if (NS_SUCCEEDED(res) && (userFlags & (kImapMsgSupportUserFlag |
													  kImapMsgSupportMDNSentFlag)))
			{
				if (imap_flags & kImapMsgMDNSentFlag)
				{
					newFlags |= MSG_FLAG_MDN_REPORT_SENT;
					if (dbHdrFlags & MSG_FLAG_MDN_REPORT_NEEDED)
						tweakMe->AndFlags(~MSG_FLAG_MDN_REPORT_NEEDED, &dbHdrFlags);
				}
			}
			else
			{
				if (!(imap_flags & kImapMsgRecentFlag) && 
					dbHdrFlags & MSG_FLAG_MDN_REPORT_NEEDED)
					tweakMe->AndFlags(~MSG_FLAG_MDN_REPORT_NEEDED, &dbHdrFlags);
			}

			if (imap_flags & kImapMsgAnsweredFlag)
				newFlags |= MSG_FLAG_REPLIED;
			if (imap_flags & kImapMsgFlaggedFlag)
				newFlags |= MSG_FLAG_MARKED;
			if (imap_flags & kImapMsgDeletedFlag)
				newFlags |= MSG_FLAG_IMAP_DELETED;
			if (imap_flags & kImapMsgForwardedFlag)
				newFlags |= MSG_FLAG_FORWARDED;

			if (newFlags)
				tweakMe->OrFlags(newFlags, &dbHdrFlags);
		}
	}
}    
    
    // nsIImapMessageSink support
NS_IMETHODIMP
nsIMAP4TestDriver::SetupMsgWriteStream(nsIImapProtocol* aProtocol,
                                   StreamInfo* aStreamInfo)
{
    printf("**** nsIMAP4TestDriver::SetupMsgWriteStream\r\n");
  // we are about to display an article so open up a temp file on the article...
  PR_Delete(ARTICLE_PATH);
  m_tempArticleFile = PR_Open(ARTICLE_PATH, PR_WRONLY | PR_CREATE_FILE | PR_TRUNCATE, 00700);
    return NS_OK;
}

    
NS_IMETHODIMP
nsIMAP4TestDriver::ParseAdoptedMsgLine(nsIImapProtocol* aProtocol,
                                   msg_line_info* aMsgLineInfo)
{
    printf("**** nsIMAP4TestDriver::ParseAdoptedMsgLine\r\n");
	if (m_tempArticleFile)
		PR_Write(m_tempArticleFile,(void *) aMsgLineInfo->adoptedMessageLine, PL_strlen(aMsgLineInfo->adoptedMessageLine));
    return NS_OK;
}

    
NS_IMETHODIMP
nsIMAP4TestDriver::NormalEndMsgWriteStream(nsIImapProtocol* aProtocol)
{
    printf("**** nsIMAP4TestDriver::NormalEndMsgWriteStream\r\n");
	if (m_tempArticleFile)
		PR_Close(m_tempArticleFile);
    return NS_OK;
}

    
NS_IMETHODIMP
nsIMAP4TestDriver::AbortMsgWriteStream(nsIImapProtocol* aProtocol)
{
    printf("**** nsIMAP4TestDriver::AbortMsgWriteStream\r\n");
    return NS_OK;
}

    
    // message move/copy related methods
NS_IMETHODIMP
nsIMAP4TestDriver::OnlineCopyReport(nsIImapProtocol* aProtocol,
                                ImapOnlineCopyState* aCopyState)
{
    printf("**** nsIMAP4TestDriver::OnlineCopyReport\r\n");
    return NS_OK;
}

NS_IMETHODIMP
nsIMAP4TestDriver::BeginMessageUpload(nsIImapProtocol* aProtocol)
{
    printf("**** nsIMAP4TestDriver::BeginMessageUpload\r\n");
    return NS_OK;
}

NS_IMETHODIMP
nsIMAP4TestDriver::UploadMessageFile(nsIImapProtocol* aProtocol,
                                 UploadMessageInfo* aMsgInfo)
{
    printf("**** nsIMAP4TestDriver::UploadMessageFile\r\n");
    return NS_OK;
}


    // message flags operation
NS_IMETHODIMP
nsIMAP4TestDriver::NotifyMessageFlags(nsIImapProtocol* aProtocol,
                                  FlagsKeyStruct* aKeyStruct)
{
    printf("**** nsIMAP4TestDriver::NotifyMessageFlags\r\n");
    return NS_OK;
}


NS_IMETHODIMP
nsIMAP4TestDriver::NotifyMessageDeleted(nsIImapProtocol* aProtocol,
                                    delete_message_struct* aStruct)
{
    printf("**** nsIMAP4TestDriver::NotifyMessageDeleted\r\n");
    return NS_OK;
}

NS_IMETHODIMP
nsIMAP4TestDriver::GetMessageSizeFromDB(nsIImapProtocol* aProtocol,
                                    MessageSizeInfo* sizeInfo)
{
    printf("**** nsIMAP4TestDriver::GetMessageSizeFromDB\r\n");
	nsresult rv = NS_ERROR_FAILURE;
	if (sizeInfo && sizeInfo->id)
	{
		PRUint32 key = atoi(sizeInfo->id);
		nsIMessage *mailHdr = nsnull;
		NS_ASSERTION(sizeInfo->idIsUid, "ids must be uids to get message size");
		if (sizeInfo->idIsUid)
			rv = m_mailDB->GetMsgHdrForKey(key, &mailHdr);
		if (NS_SUCCEEDED(rv) && mailHdr)
		{
			rv = mailHdr->GetMessageSize(&sizeInfo->size);
			NS_RELEASE(mailHdr);
		}
	}
    return rv;
}


    // nsIImapExtensionSink support
  
NS_IMETHODIMP
nsIMAP4TestDriver::SetUserAuthenticated(nsIImapProtocol* aProtocol,
								  PRBool aBool)
{
    printf("**** nsIMAP4TestDriver::SetUserAuthenticated\r\n");
    return NS_OK;
}

NS_IMETHODIMP
nsIMAP4TestDriver::SetMailServerUrls(nsIImapProtocol* aProtocol,
                                 const char* hostName)
{
    printf("**** nsIMAP4TestDriver::SetMailServerUrls\r\n");
    return NS_OK;
}

NS_IMETHODIMP
nsIMAP4TestDriver::SetMailAccountUrl(nsIImapProtocol* aProtocol,
                                 const char* hostName)
{
    printf("**** nsIMAP4TestDriver::SetMailAccountUrl\r\n");
    return NS_OK;
}

NS_IMETHODIMP
nsIMAP4TestDriver::ClearFolderRights(nsIImapProtocol* aProtocol,
                                 nsIMAPACLRightsInfo* aclRights)
{
    printf("**** nsIMAP4TestDriver::ClearFolderRights\r\n");
    return NS_OK;
}

NS_IMETHODIMP
nsIMAP4TestDriver::AddFolderRights(nsIImapProtocol* aProtocol,
                               nsIMAPACLRightsInfo* aclRights)
{
    printf("**** nsIMAP4TestDriver::AddFolderRights\r\n");
    return NS_OK;
}

NS_IMETHODIMP
nsIMAP4TestDriver::RefreshFolderRights(nsIImapProtocol* aProtocol,
                                   nsIMAPACLRightsInfo* aclRights)
{
    printf("**** nsIMAP4TestDriver::RefreshFolderRights\r\n");
    return NS_OK;
}

NS_IMETHODIMP
nsIMAP4TestDriver::FolderNeedsACLInitialized(nsIImapProtocol* aProtocol,
                                         nsIMAPACLRightsInfo* aclRights)
{
    printf("**** nsIMAP4TestDriver::FolderNeedsACLInitialized\r\n");
    return NS_OK;
}

NS_IMETHODIMP
nsIMAP4TestDriver::SetFolderAdminURL(nsIImapProtocol* aProtocol,
                                 FolderQueryInfo* aInfo)
{
    printf("**** nsIMAP4TestDriver::SetFolderAdminURL\r\n");
    return NS_OK;
}

    
    // nsIImapMiscellaneousSink support
	
NS_IMETHODIMP
nsIMAP4TestDriver::AddSearchResult(nsIImapProtocol* aProtocol, 
                               const char* searchHitLine)
{
    printf("**** nsIMAP4TestDriver::AddSearchResult\r\n");
    return NS_OK;
}

NS_IMETHODIMP
nsIMAP4TestDriver::GetArbitraryHeaders(nsIImapProtocol* aProtocol,
                                   GenericInfo* aInfo)
{
    printf("**** nsIMAP4TestDriver::GetArbitraryHeaders\r\n");
    return NS_OK;
}

NS_IMETHODIMP
nsIMAP4TestDriver::GetShouldDownloadArbitraryHeaders(nsIImapProtocol* aProtocol,
                                                 GenericInfo* aInfo)
{
    printf("**** nsIMAP4TestDriver::GetShouldDownloadArbitraryHeaders\r\n");
    return NS_OK;
}

NS_IMETHODIMP
nsIMAP4TestDriver::GetShowAttachmentsInline(nsIImapProtocol* aProtocol,
                                            PRBool* aBool)
{
    printf("**** nsIMAP4TestDriver::GetShowAttachmentsInLine\r\n");
    return NS_OK;
}

NS_IMETHODIMP
nsIMAP4TestDriver::HeaderFetchCompleted(nsIImapProtocol* aProtocol)
{
    printf("**** nsIMAP4TestDriver::HeaderFetchCompleted\r\n");
    return NS_OK;
}

NS_IMETHODIMP
nsIMAP4TestDriver::UpdateSecurityStatus(nsIImapProtocol* aProtocol)
{
    printf("**** nsIMAP4TestDriver::UpdateSecurityStatus\r\n");
    return NS_OK;
}

	// ****
NS_IMETHODIMP
nsIMAP4TestDriver::FinishImapConnection(nsIImapProtocol* aProtocol)
{
    printf("**** nsIMAP4TestDriver::FinishImapConnection\r\n");
    return NS_OK;
}

NS_IMETHODIMP
nsIMAP4TestDriver::SetImapHostPassword(nsIImapProtocol* aProtocol,
                                   GenericInfo* aInfo)
{
    printf("**** nsIMAP4TestDriver::SetImapHostPassword\r\n");
    return NS_OK;
}

NS_IMETHODIMP
nsIMAP4TestDriver::GetPasswordForUser(nsIImapProtocol* aProtocol,
                                  const char* userName)
{
    printf("**** nsIMAP4TestDriver::GetPasswordForUser\r\n");
    return NS_OK;
}

NS_IMETHODIMP
nsIMAP4TestDriver::SetBiffStateAndUpdate(nsIImapProtocol* aProtocol,
                                     nsMsgBiffState biffState)
{
    printf("**** nsIMAP4TestDriver::SetBiffStateAndUpdate\r\n");
    return NS_OK;
}

NS_IMETHODIMP
nsIMAP4TestDriver::GetStoredUIDValidity(nsIImapProtocol* aProtocol,
                                    uid_validity_info* aInfo)
{
    printf("**** nsIMAP4TestDriver::GetStoredUIDValidity\r\n");
    return NS_OK;
}

NS_IMETHODIMP
nsIMAP4TestDriver::LiteSelectUIDValidity(nsIImapProtocol* aProtocol,
                                     PRUint32 uidValidity)
{
    printf("**** nsIMAP4TestDriver::LiteSelectUIDValidity\r\n");
    return NS_OK;
}

NS_IMETHODIMP
nsIMAP4TestDriver::FEAlert(nsIImapProtocol* aProtocol,
                       const char* aString)
{
    printf("**** nsIMAP4TestDriver::FEAlert\r\n");
    return NS_OK;
}

NS_IMETHODIMP
nsIMAP4TestDriver::FEAlertFromServer(nsIImapProtocol* aProtocol,
                                 const char* aString)
{
    printf("**** nsIMAP4TestDriver::FEAlertFromServer\r\n");
    return NS_OK;
}

NS_IMETHODIMP
nsIMAP4TestDriver::ProgressStatus(nsIImapProtocol* aProtocol,
                              const char* statusMsg)
{
    printf("**** nsIMAP4TestDriver::ProgressStatus\r\n");
    return NS_OK;
}

NS_IMETHODIMP
nsIMAP4TestDriver::PercentProgress(nsIImapProtocol* aProtocol,
                               ProgressInfo* aInfo)
{
    printf("**** nsIMAP4TestDriver::PercentProgress\r\n");
    return NS_OK;
}

NS_IMETHODIMP
nsIMAP4TestDriver::PastPasswordCheck(nsIImapProtocol* aProtocol)
{
    printf("**** nsIMAP4TestDriver::PastPasswordCheck\r\n");
    return NS_OK;
}

NS_IMETHODIMP
nsIMAP4TestDriver::CommitNamespaces(nsIImapProtocol* aProtocol,
                                const char* hostName)
{
    printf("**** nsIMAP4TestDriver::CommitNamespaces\r\n");
    return NS_OK;
}

NS_IMETHODIMP
nsIMAP4TestDriver::CommitCapabilityForHost(nsIImapProtocol* aProtocol,
                                       const char* hostName)
{
    printf("**** nsIMAP4TestDriver::CommitCapabilityForHost\r\n");
    return NS_OK;
}

NS_IMETHODIMP
nsIMAP4TestDriver::TunnelOutStream(nsIImapProtocol* aProtocol,
                               msg_line_info* aInfo)
{
    printf("**** nsIMAP4TestDriver::TunnelOutStream\r\n");
    return NS_OK;
}

NS_IMETHODIMP
nsIMAP4TestDriver::ProcessTunnel(nsIImapProtocol* aProtocol,
                             TunnelInfo *aInfo)
{
    printf("**** nsIMAP4TestDriver::ProcessTunnel\r\n");
    return NS_OK;
}


nsresult nsIMAP4TestDriver::InitializeProtocol(const char * urlString)
{
	nsresult rv = NS_OK;

	// we used to create an imap url here...but I don't think we need to.
	// we should create the url before we run each unique imap command...

	// now create a protocol instance using the imap service! 

	nsIImapService * imapService = nsnull;
	rv = nsServiceManager::GetService(kCImapService, nsIImapService::GetIID(), (nsISupports **) &imapService);

	if (NS_SUCCEEDED(rv) && imapService)
	{
		imapService->CreateImapConnection(m_eventQueue, &m_IMAP4Protocol);
		nsServiceManager::ReleaseService(kCImapService, imapService);
        if (m_IMAP4Protocol)
            m_protocolInitialized = PR_TRUE;
            
	}

	return rv;
}

nsIMAP4TestDriver::~nsIMAP4TestDriver()
{
	NS_IF_RELEASE(m_url);
	NS_IF_RELEASE(m_IMAP4Protocol);
	if (m_mailDB)
		m_mailDB->Commit(kLargeCommit);
	NS_IF_RELEASE(m_mailDB);
}

nsresult nsIMAP4TestDriver::RunDriver()
{
	nsresult status = NS_OK;

	while (m_runTestHarness)
	{
		if (!m_runningURL) // if we aren't running the url anymore, ask ueser for another command....
		{
			status = ReadAndDispatchCommand();
		}  // if running url
#ifdef XP_UNIX

        PL_ProcessPendingEvents(m_eventQueue);

#endif
#ifdef XP_PC	
		MSG msg;
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) 
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
#endif

	} // until the user has stopped running the url (which is really the test session.....

	return status;
}

nsresult nsIMAP4TestDriver::ReadAndDispatchCommand()
{
	nsresult status = NS_OK;
	PRInt32 command = 0; 
	char commandString[5];
	commandString[0] = '\0';

	printf("Enter command number: ");
	scanf("%[^\n]", commandString);
	if (commandString && *commandString)
	{
		command = atoi(commandString);
	}
	scanf("%c", commandString);  // eat the extra CR

	// now switch on command to the appropriate 
	switch (command)
	{
	case 0:
		status = ListCommands();
		break;
	case 1:
		status = OnRunIMAPCommand();
		break;
	case 2:
		status = OnIdentityCheck();
		break;
	case 3:
		status = OnTestUrlParsing();
		break;
	case 4: 
		status = OnSelectFolder();
		break;
	case 5:
		status = OnFetchMessage();
		break;
	default:
		status = OnExit();
		break;
	}

	return status;
}

nsresult nsIMAP4TestDriver::ListCommands()
{
	printf("Commands currently available: \n");
	printf("0) List available commands. \n");
	printf("1) Run IMAP Command. \n");
	printf("2) Check identity information.\n");
	printf("3) Test url parsing. \n");
	printf("4) Select Folder. \n");
	printf("5) Download a message. \n");
	printf("9) Exit the test application. \n");
	return NS_OK;
}

///////////////////////////////////////////////////////////////////////////////////
// Begin protocol specific command url generation code...gee that's a mouthful....
///////////////////////////////////////////////////////////////////////////////////

nsresult nsIMAP4TestDriver::OnStartRunningUrl(nsIURL * aUrl)
{
	NS_PRECONDITION(aUrl, "just a sanity check since this is a test program");
	m_runningURL = PR_TRUE;
	return NS_OK;
}

nsresult nsIMAP4TestDriver::OnStopRunningUrl(nsIURL * aUrl, nsresult aExitCode)
{
	NS_PRECONDITION(aUrl, "just a sanity check since this is a test program");
	nsresult rv = NS_OK;
	m_runningURL = PR_FALSE;
	if (aUrl)
	{
		// query it for a mailnews interface for now....
		nsIMsgMailNewsUrl * mailUrl = nsnull;
		rv = aUrl->QueryInterface(nsIMsgMailNewsUrl::GetIID(), (void **) mailUrl);
		if (NS_SUCCEEDED(rv))
		{
			// our url must be done so release it...
			NS_IF_RELEASE(m_url);
			m_url = nsnull;
			mailUrl->UnRegisterListener(this);
		}
	}

	return NS_OK;
}

nsresult nsIMAP4TestDriver::HandleImapLogData (const char * aLogData)
{
	// for now, play dumb and just spit out what we were given...
	if (aLogData)
	{
		printf(aLogData);
		printf("\n");
	}

	return NS_OK;
}


nsresult nsIMAP4TestDriver::OnExit()
{
	printf("Terminating IMAP4 test harness....\n");
	m_runTestHarness = PR_FALSE; // next time through the test driver loop, we'll kick out....
	return NS_OK;
}

static NS_DEFINE_CID(kCMsgMailSessionCID, NS_MSGMAILSESSION_CID); 

nsresult nsIMAP4TestDriver::OnIdentityCheck()
{
	nsIMsgMailSession * mailSession = nsnull;
	nsresult result = nsServiceManager::GetService(kCMsgMailSessionCID,
												   nsIMsgMailSession::GetIID(),
                                                   (nsISupports **) &mailSession);
	if (NS_SUCCEEDED(result) && mailSession)
	{
		nsIMsgIncomingServer * server = nsnull;
		result = mailSession->GetCurrentServer(&server);
		if (NS_SUCCEEDED(result) && server)
		{
			char * value = nsnull;
			server->GetHostName(&value);
			printf("Imap Server: %s\n", value ? value : "");
			server->GetUserName(&value);
			printf("User Name: %s\n", value ? value : "");
			server->GetPassword(&value);
			printf("Imap Password: %s\n", value ? value : "");

		}
		else
			printf("Unable to retrieve the msgIdentity....\n");

		nsServiceManager::ReleaseService(kCMsgMailSessionCID, mailSession);
	}
	else
		printf("Unable to retrieve the mail session service....\n");

	return result;
}

nsresult nsIMAP4TestDriver::OnSelectFolder()
{
	// go get the imap service and ask it to select a folder
	// mscott - i may want to cache this in the test harness class
	// since we'll be using it for pretty much all the commands

	nsIImapService * imapService = nsnull;
	nsresult rv = nsServiceManager::GetService(kCImapService, nsIImapService::GetIID(), (nsISupports **) &imapService);

	if (NS_SUCCEEDED(rv) && imapService)
	{
		rv = imapService->SelectFolder(m_eventQueue, this /* imap folder sink */, this /* url listener */, nsnull);
		nsServiceManager::ReleaseService(kCImapService, imapService);
		m_runningURL = PR_TRUE; // we are now running a url...
	}

	return rv;
}

nsresult nsIMAP4TestDriver::OnFetchMessage()
{
	// go get the imap service and ask it to load a message

	nsresult rv = NS_OK;
	char	uidString[200];

	PL_strcpy(uidString, "1");
	// prompt for the command to run ....
	printf("Enter UID(s) of message(s) to fetch [%s]: ", uidString);
	scanf("%[^\n]", uidString);


	nsIImapService * imapService = nsnull;
	rv = nsServiceManager::GetService(kCImapService, nsIImapService::GetIID(), (nsISupports **) &imapService);

	if (NS_SUCCEEDED(rv) && imapService)
	{
		rv = imapService->FetchMessage(m_eventQueue, this /* imap folder sink */, this, /* imap message sink */ this /* url listener */, nsnull,
			uidString, PR_TRUE);
		nsServiceManager::ReleaseService(kCImapService, imapService);
		m_runningURL = PR_TRUE; // we are now running a url...
	}

	return rv;
}


nsresult nsIMAP4TestDriver::OnRunIMAPCommand()
{
	nsresult rv = NS_OK;

	PL_strcpy(m_command, "capability");
	// prompt for the command to run ....
	printf("Enter IMAP command to run [%s]: ", m_command);
	scanf("%[^\n]", m_command);


	m_urlString[0] = '\0';
	PL_strcpy(m_urlString, m_urlSpec);
	PL_strcat(m_urlString, "?");
	PL_strcat(m_urlString, m_command);

	if (m_protocolInitialized == PR_FALSE)
		rv = InitializeProtocol(m_urlString);

	// create a url to process the request.
    rv = nsComponentManager::CreateInstance(kImapUrlCID, nsnull,
                                            nsIImapUrl::GetIID(), (void **)
                                            &m_url);
	if (NS_SUCCEEDED(rv) && m_url)
    {
        m_url->SetImapLog(this);
        m_url->SetImapMailFolderSink(this);
        m_url->SetImapMessageSink(this);
        m_url->SetImapExtensionSink(this);
        m_url->SetImapMiscellaneousSink(this);

		rv = m_url->SetSpec(m_urlString); // reset spec
		m_url->RegisterListener(this);
    }
	
	if (NS_SUCCEEDED(rv))
	{
		rv = m_IMAP4Protocol->LoadUrl(m_url, nsnull /* probably need a consumer... */);
		m_runningURL = PR_TRUE; // we are now running a url...
	} // if user provided the data...

	return rv;
}

nsresult nsIMAP4TestDriver::OnGet()
{
	nsresult rv = NS_OK;

	m_urlString[0] = '\0';
	PL_strcpy(m_urlString, m_urlSpec);
#ifdef HAVE_SERVICE_MGR
	nsIIMAP4Service * IMAP4Service = nsnull;
	nsServiceManager::GetService(kIMAP4ServiceCID, nsIIMAP4Service::GetIID(),
                                 (nsISupports **)&IMAP4Service); // XXX probably need shutdown listener here

	if (IMAP4Service)
	{
		IMAP4Service->GetNewMail(nsnull, nsnull);
	}

	nsServiceManager::ReleaseService(kIMAP4ServiceCID, IMAP4Service);
#endif // HAVE_SERVICE_MGR
#if 0

	if (m_protocolInitialized == PR_FALSE)
		InitializeProtocol(m_urlString);
	else
		rv = m_url->SetSpec(m_urlString); // reset spec
	
	// load the correct newsgroup interface as an event sink...
	if (NS_SUCCEEDED(rv))
	{
		 // before we re-load, assume it is a group command and configure our IMAP4URL correctly...

		rv = m_IMAP4Protocol->Load(m_url);
	} // if user provided the data...
#endif
	return rv;
}


/* strip out non-printable characters */
static void strip_nonprintable(char *string) {
    char *dest, *src;

    dest=src=string;
    while (*src) {
        if (isprint(*src)) {
            (*src)=(*dest);
            src++; dest++;
        } else {
            src++;
        }
    }
    (*dest)='\0';
}


// prints the userPrompt and then reads in the user data. Assumes urlData has already been allocated.
// it also reconstructs the url string in m_urlString but does NOT reload it....
nsresult nsIMAP4TestDriver::PromptForUserData(const char * userPrompt)
{
	char tempBuffer[500];
	tempBuffer[0] = '\0'; 

	if (userPrompt)
		printf(userPrompt);
	else
		printf("Enter data for command: ");

    fgets(tempBuffer, sizeof(tempBuffer), stdin);
    strip_nonprintable(tempBuffer);
	// only replace m_userData if the user actually entered a valid line...
	// this allows the command function to set a default value on m_userData before
	// calling this routine....
	if (tempBuffer && *tempBuffer)
		PL_strcpy(m_userData, tempBuffer);

	return NS_OK;
}


nsresult nsIMAP4TestDriver::OnTestUrlParsing()
{
	nsresult rv = NS_OK; 
	char * hostName = nsnull;
	PRUint32 port = IMAP_PORT;

	char * displayString = nsnull;
	
	PL_strcpy(m_userData, DEFAULT_HOST);

	displayString = PR_smprintf("Enter a host name for the imap url [%s]: ", m_userData);
	rv = PromptForUserData(displayString);
	PR_FREEIF(displayString);

	hostName = PL_strdup(m_userData);

	PL_strcpy(m_userData, "143");
	displayString = PR_smprintf("Enter port number if any for the imap url [%d] (use 0 to skip port field): ", IMAP_PORT);
	rv = PromptForUserData(displayString);
	PR_FREEIF(displayString);
	
	port = atol(m_userData);

	// as we had more functionality to the imap url, we'll probably need to ask for more information than just
	// the host and the port...

	nsIImapUrl * imapUrl = nsnull;
	
	nsComponentManager::CreateInstance(kImapUrlCID, nsnull /* progID */, nsIImapUrl::GetIID(), (void **) &imapUrl);
	if (imapUrl)
	{
		char * urlSpec = nsnull;
		if (m_port > 0) // did the user specify a port? 
			urlSpec = PR_smprintf("imap://%s:%d", hostName, port);
		else
			urlSpec = PR_smprintf("imap://%s", hostName);

		imapUrl->SetSpec("imap://nsmail-2.mcom.com:143/test");
		
		const char * urlHost = nsnull;
		PRUint32 urlPort = 0;

		imapUrl->GetHost(&urlHost);
		imapUrl->GetHostPort(&urlPort);

		printf("Host name test: %s\n", PL_strcmp(urlHost, hostName) == 0 ? "PASSED." : "FAILED!");
		if (port > 0) // did the user try to test the port?
			printf("Port test: %s\n", port == urlPort ? "PASSED." : "FAILED!");

		NS_IF_RELEASE(imapUrl);
	}
	else
		printf("Failure!! Unable to create an imap url. Registration problem? \n");

	PR_FREEIF(hostName);

	return rv;
}


int main()
{
    PLEventQueue *queue;
    nsresult result;

	// register all the components we might need - what's the imap service going to be called?
//    nsComponentManager::RegisterComponent(kNetServiceCID, NULL, NULL, NETLIB_DLL, PR_FALSE, PR_FALSE);
	nsComponentManager::RegisterComponent(kEventQueueServiceCID, NULL, NULL, XPCOM_DLL, PR_FALSE, PR_FALSE);
//	nsComponentManager::RegisterComponent(kRDFServiceCID, nsnull, nsnull, RDF_DLL, PR_TRUE, PR_TRUE);
	nsComponentManager::RegisterComponent(kPrefCID, nsnull, nsnull, PREF_DLL, PR_TRUE, PR_TRUE);
	// IMAP Service goes here?
    nsComponentManager::RegisterComponent(kImapUrlCID, nsnull, nsnull,
                                          MSGIMAP_DLL, PR_FALSE, PR_FALSE);

    nsComponentManager::RegisterComponent(kImapProtocolCID, nsnull, nsnull,
                                          MSGIMAP_DLL, PR_FALSE, PR_FALSE);

	// make sure prefs get initialized and loaded..
	// mscott - this is just a bad bad bad hack right now until prefs
	// has the ability to take nsnull as a parameter. Once that happens,
	// prefs will do the work of figuring out which prefs file to load...
	NS_WITH_SERVICE(nsIPref, prefs, kPrefCID, &result); 
//	if (NS_SUCCEEDED(result) && prefs)
//	{
//		prefs->Startup("prefs50.js");
//	}

	// Create the Event Queue for the test app thread...a standin for the ui thread
    nsIEventQueueService* pEventQService;
    result = nsServiceManager::GetService(kEventQueueServiceCID,
                                          nsIEventQueueService::GetIID(),
                                          (nsISupports**)&pEventQService);
	if (NS_FAILED(result)) return result;

    result = pEventQService->CreateThreadEventQueue();

	if (NS_FAILED(result)) return result;

    pEventQService->GetThreadEventQueue(PR_GetCurrentThread(),&queue);
    if (NS_FAILED(result) || !queue) 
	{
        printf("unable to get event queue.\n");
        return 1;
    }

	nsServiceManager::ReleaseService(kEventQueueServiceCID, pEventQService);

	// okay, everything is set up, now we just need to create a test driver and run it...
	nsIMAP4TestDriver * driver = new nsIMAP4TestDriver(queue);
	if (driver)
	{
		NS_ADDREF(driver);
		driver->RunDriver();
		// when it kicks out...it is done....so delete it...
		NS_RELEASE(driver);
	}

	// shut down:
    return 0;

}
