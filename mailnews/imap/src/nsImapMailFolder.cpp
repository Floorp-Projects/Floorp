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
 * Copyright (C) 1998, 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "msgCore.h"
#include "nsImapMailFolder.h"
#include "nsIEnumerator.h"
#include "nsIFolderListener.h"
#include "nsCOMPtr.h"
#include "nsIRDFService.h"
#include "nsIRDFDataSource.h"
#include "nsRDFCID.h"
#include "nsFileStream.h"
#include "nsMsgDBCID.h"
#include "nsMsgFolderFlags.h"
#include "nsLocalFolderSummarySpec.h"
#include "nsImapFlagAndUidState.h"
#include "nsParseMailbox.h"
#include "nsIEventQueueService.h"
#include "nsXPComCIID.h"
#include "nsIImapUrl.h"
#include "nsImapUtils.h"
#include "nsMsgUtils.h"
#include "nsIMsgMailSession.h"
#include "nsImapMessage.h"
#include "nsIWebShell.h"

// we need this because of an egcs 1.0 (and possibly gcc) compiler bug
// that doesn't allow you to call ::nsISupports::GetIID() inside of a class
// that multiply inherits from nsISupports

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kCMailDB, NS_MAILDB_CID);
static NS_DEFINE_CID(kImapProtocolCID, NS_IMAPPROTOCOL_CID);
static NS_DEFINE_CID(kCImapDB, NS_IMAPDB_CID);
static NS_DEFINE_CID(kCImapService, NS_IMAPSERVICE_CID);
static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_CID(kMsgMailSessionCID, NS_MSGMAILSESSION_CID);

////////////////////////////////////////////////////////////////////////////////
// for temp message hack
#ifdef XP_UNIX
#define MESSAGE_PATH "/usr/tmp/tempMessage.eml"
#endif

#ifdef XP_PC
#define MESSAGE_PATH  "c:\\temp\\tempMessage.eml"
#endif

#ifdef XP_MAC
#define MESSAGE_PATH  "tempMessage.eml"
#endif

nsImapMailFolder::nsImapMailFolder() :
	nsMsgFolder(), m_pathName(""), m_mailDatabase(nsnull),
    m_initialized(PR_FALSE), m_haveReadNameFromDB(PR_FALSE),
    m_msgParser(nsnull), m_curMsgUid(0), m_nextMessageByteLength(0),
    m_urlRunning(PR_FALSE)
{
    //XXXX This is a hack for the moment.  I'm assuming the only listener is
    //our rdf:mailnews datasource. 
    //In reality anyone should be able to listen to folder changes. 
    
    nsIRDFService* rdfService = nsnull;
    nsIRDFDataSource* datasource = nsnull;
	m_tempMessageFile = nsnull;
    nsresult rv = nsServiceManager::GetService(kRDFServiceCID,
                                               nsIRDFService::GetIID(),
                                               (nsISupports**) &rdfService);
    if(NS_SUCCEEDED(rv))
    {
        rv = rdfService->GetDataSource("rdf:mailnewsfolders", &datasource);
        if(NS_SUCCEEDED(rv))
        {
            nsIFolderListener *folderListener;
            rv = datasource->QueryInterface(nsIFolderListener::GetIID(),
                                            (void**)&folderListener);
            if(NS_SUCCEEDED(rv))
            {
                AddFolderListener(folderListener);
                NS_RELEASE(folderListener);
            }
            NS_RELEASE(datasource);
        }
        nsServiceManager::ReleaseService(kRDFServiceCID, rdfService);
    }

    // Get current thread envent queue
    nsIEventQueueService* pEventQService;
    m_eventQueue = nsnull;
    rv = nsServiceManager::GetService(kEventQueueServiceCID,
                                      nsIEventQueueService::GetIID(),
                                      (nsISupports**)&pEventQService);
    if (NS_SUCCEEDED(rv) && pEventQService)
        pEventQService->GetThreadEventQueue(PR_GetCurrentThread(),
                                            &m_eventQueue);
    if (pEventQService)
        nsServiceManager::ReleaseService(kEventQueueServiceCID,
                                         pEventQService);

	m_msgParser = nsnull;
//  NS_INIT_REFCNT(); done by superclass
}

nsImapMailFolder::~nsImapMailFolder()
{
    if (m_mailDatabase)
        m_mailDatabase->Close(PR_TRUE);
}

NS_IMPL_ADDREF_INHERITED(nsImapMailFolder, nsMsgFolder)
NS_IMPL_RELEASE_INHERITED(nsImapMailFolder, nsMsgFolder)

NS_IMETHODIMP nsImapMailFolder::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
	if (!aInstancePtr) return NS_ERROR_NULL_POINTER;
	*aInstancePtr = nsnull;

    if (aIID.Equals(nsIMsgImapMailFolder::GetIID()))
	{
		*aInstancePtr = NS_STATIC_CAST(nsIMsgImapMailFolder*, this);
	}              
	else if (aIID.Equals(nsIDBChangeListener::GetIID()))
	{
		*aInstancePtr = NS_STATIC_CAST(nsIDBChangeListener*, this);
	}
	else if(aIID.Equals(nsICopyMessageListener::GetIID()))
	{
		*aInstancePtr = NS_STATIC_CAST(nsICopyMessageListener*, this);
	}
	else if (aIID.Equals(nsIImapMailFolderSink::GetIID()))
	{
		*aInstancePtr = NS_STATIC_CAST(nsIImapMailFolderSink*, this);
	}
	else if (aIID.Equals(nsIImapMessageSink::GetIID()))
	{
		*aInstancePtr = NS_STATIC_CAST(nsIImapMessageSink*, this);
	}
	else if (aIID.Equals(nsIImapExtensionSink::GetIID()))
	{
		*aInstancePtr = NS_STATIC_CAST(nsIImapExtensionSink*, this);
	}
	else if (aIID.Equals(nsIImapMiscellaneousSink::GetIID()))
	{
		*aInstancePtr = NS_STATIC_CAST(nsIImapMiscellaneousSink*, this);
	}

	if(*aInstancePtr)
	{
		AddRef();
		return NS_OK;
	}

	return nsMsgFolder::QueryInterface(aIID, aInstancePtr);
}

NS_IMETHODIMP nsImapMailFolder::GetPathName(nsNativeFileSpec& aPathName)
{
    nsFileSpec nopath("");
    if (m_pathName == nopath) 
    {
        nsresult rv = nsImapURI2Path(kImapRootURI, mURI, m_pathName);
        if (NS_FAILED(rv)) return rv;
    }
    aPathName = m_pathName;
	return NS_OK;
}

NS_IMETHODIMP nsImapMailFolder::Enumerate(nsIEnumerator* *result)
{
    nsresult rv = NS_OK;
    nsIEnumerator* folders;
    nsIEnumerator* messages;
    rv = GetSubFolders(&folders);
    if (NS_FAILED(rv)) return rv;
    rv = GetMessages(&messages);
    if (NS_FAILED(rv)) return rv;
    return NS_NewConjoiningEnumerator(folders, messages, 
                                      (nsIBidirectionalEnumerator**)result);
}

nsresult nsImapMailFolder::AddDirectorySeparator(nsFileSpec &path)
{
	nsresult rv = NS_OK;
	if (nsCRT::strcmp(mURI, kImapRootURI) == 0) {
      // don't concat the full separator with .sbd
    }
    else {
      nsAutoString sep;
      rv = nsGetMailFolderSeparator(sep);
      if (NS_FAILED(rv)) return rv;

      // see if there's a dir with the same name ending with .sbd
      // unfortunately we can't just say:
      //          path += sep;
      // here because of the way nsFileSpec concatenates
      nsAutoString str((nsFilePath)path);
      str += sep;
      path = nsFilePath(str);
    }

	return rv;
}

static PRBool
nsShouldIgnoreFile(nsString& name)
{
    PRInt32 len = name.Length();
    if (len > 4 && name.RFind(".msf", PR_TRUE) == len -4)
    {
        name.SetLength(len-4); // truncate the string
        return PR_FALSE;
    }
    return PR_TRUE;
}

nsresult nsImapMailFolder::AddSubfolder(nsAutoString name, 
                                        nsIMsgFolder **child)
{
	if(!child)
		return NS_ERROR_NULL_POINTER;

	nsresult rv = NS_OK;
	nsIRDFService* rdf;
	rv = nsServiceManager::GetService(kRDFServiceCID,
                                    nsIRDFService::GetIID(),
                                    (nsISupports**)&rdf);

	if(NS_FAILED(rv))
		return rv;

	nsAutoString uri;
	uri.Append(mURI);
	uri.Append('/');

	uri.Append(name);
	char* uriStr = uri.ToNewCString();
	if (uriStr == nsnull) 
		return NS_ERROR_OUT_OF_MEMORY;

	nsIRDFResource* res;
	rv = rdf->GetResource(uriStr, &res);
	if (NS_FAILED(rv))
		return rv;
	nsCOMPtr<nsIMsgFolder> folder(do_QueryInterface(res, &rv));
	if (NS_FAILED(rv))
		return rv;        
	delete[] uriStr;
	folder->SetFlag(MSG_FOLDER_FLAG_MAIL);

	if(name == "Inbox")
		folder->SetFlag(MSG_FOLDER_FLAG_INBOX);
	else if(name == "Trash")
		folder->SetFlag(MSG_FOLDER_FLAG_TRASH);
  
	mSubFolders->AppendElement(folder);
    folder->SetDepth(mDepth+1);
	*child = folder;
	NS_ADDREF(*child);
    (void)nsServiceManager::ReleaseService(kRDFServiceCID, rdf);

	return rv;
}

nsresult nsImapMailFolder::CreateSubFolders(nsFileSpec &path)
{
	nsresult rv = NS_OK;
	nsAutoString currentFolderNameStr;
	nsIMsgFolder *child;
	char *folderName;
	for (nsDirectoryIterator dir(path); dir.Exists(); dir++) {
		nsFileSpec currentFolderPath = (nsFileSpec&)dir;

		folderName = currentFolderPath.GetLeafName();
		currentFolderNameStr = folderName;
		if (nsShouldIgnoreFile(currentFolderNameStr))
		{
			PL_strfree(folderName);
			continue;
		}

		AddSubfolder(currentFolderNameStr, &child);
		NS_IF_RELEASE(child);
		PL_strfree(folderName);
    }
	return rv;
}

NS_IMETHODIMP nsImapMailFolder::GetSubFolders(nsIEnumerator* *result)
{
    if (!m_initialized)
    {
        nsresult rv = NS_OK;
        nsNativeFileSpec path;
        rv = GetPathName(path);
        if (NS_FAILED(rv)) return rv;

        rv = AddDirectorySeparator(path);
        if(NS_FAILED(rv)) return rv;
        
        // we have to treat the root folder specially, because it's name
        // doesn't end with .sbd

        PRInt32 newFlags = MSG_FOLDER_FLAG_MAIL;
		if (mDepth == 0)
		{
#if 0
			// temporary until we do folder discovery correctly.
			nsAutoString name("Inbox");
			nsIMsgFolder *child;

			AddSubfolder(name, &child);
			if (NS_SUCCEEDED(GetDatabase()))
			{
				nsIDBFolderInfo *dbFolderInfo = nsnull;
				m_mailDatabase->GetDBFolderInfo(&dbFolderInfo);
				if (dbFolderInfo)
				{
					dbFolderInfo->SetMailboxName(name);
					NS_RELEASE(dbFolderInfo);
				}
			}
			if (child)
				NS_RELEASE(child);
#endif
		}
        if (path.IsDirectory()) {
            newFlags |= (MSG_FOLDER_FLAG_DIRECTORY | MSG_FOLDER_FLAG_ELIDED);
            SetFlag(newFlags);
            rv = CreateSubFolders(path);
        }
        else 
        {
            UpdateSummaryTotals();
            // Look for a directory for this mail folder, and recurse into it.
            // e.g. if the folder is "inbox", look for "inbox.sbd". 
#if 0
            char *folderName = path->GetLeafName();
            char *newLeafName = (char*)malloc(PL_strlen(folderName) +
                                              PL_strlen(kDirExt) + 2);
            PL_strcpy(newLeafName, folderName);
            PL_strcat(newLeafName, kDirExt);
            path->SetLeafName(newLeafName);
            if(folderName)
                nsCRT::free(folderName);
            if(newLeafName)
                nsCRT::free(newLeafName);
#endif
        }

        if (NS_FAILED(rv)) return rv;
        m_initialized = PR_TRUE;      // XXX do this on failure too?
    }
    return mSubFolders->Enumerate(result);
}

NS_IMETHODIMP nsImapMailFolder::AddUnique(nsISupports* element)
{
    nsresult rv = NS_ERROR_FAILURE;
    return rv;
}

NS_IMETHODIMP nsImapMailFolder::ReplaceElement(nsISupports* element,
                                               nsISupports* newElement)
{
    nsresult rv = NS_ERROR_FAILURE;
    return rv;
}


//Makes sure the database is open and exists.  If the database is valid then
//returns NS_OK.  Otherwise returns a failure error value.
nsresult nsImapMailFolder::GetDatabase()
{
	nsresult folderOpen = NS_OK;
	if (m_mailDatabase == nsnull)
	{
		nsNativeFileSpec path;
		nsresult rv = GetPathName(path);
		if (NS_FAILED(rv)) return rv;

		nsIMsgDatabase * mailDBFactory = nsnull;

		rv = nsComponentManager::CreateInstance(kCImapDB, nsnull, nsIMsgDatabase::GetIID(), (void **) &mailDBFactory);
		if (NS_SUCCEEDED(rv) && mailDBFactory)
		{
			folderOpen = mailDBFactory->Open(path, PR_TRUE, (nsIMsgDatabase **) &m_mailDatabase, PR_TRUE);
	
			NS_RELEASE(mailDBFactory);
		}

		if(m_mailDatabase)
		{
			m_mailDatabase->AddListener(this);

			// if we have to regenerate the folder, run the parser url.
			if(folderOpen == NS_MSG_ERROR_FOLDER_SUMMARY_MISSING || folderOpen == NS_MSG_ERROR_FOLDER_SUMMARY_OUT_OF_DATE)
			{
			}
			else
			{
				//Otherwise we have a valid database so lets extract necessary info.
				UpdateSummaryTotals();
			}
		}
	}
	return folderOpen;
}

PRBool
nsImapMailFolder::FindAndSelectFolder(nsISupports* aElement,
                                      void* aData)
{
    const char *target = (const char*) aData;
    NS_ASSERTION (target && *target, "Opps ... null target folder name");
    if (!target || !*target) return PR_FALSE; // stop everything

    nsresult rv = NS_ERROR_FAILURE;
    PRBool keepGoing = PR_TRUE;
    nsCOMPtr<nsIMsgFolder> aImapMailFolder(do_QueryInterface(aElement,
                                                             &rv));
    if (NS_FAILED(rv)) return keepGoing;
    char *folderName = nsnull;
    PRUint32 depth = 0;
    aImapMailFolder->GetDepth(&depth);
    // Get current thread envent queue
    nsIEventQueueService* pEventQService;
    PLEventQueue* eventQueue = nsnull;

    rv = nsServiceManager::GetService(kEventQueueServiceCID,
                                      nsIEventQueueService::GetIID(),
                                      (nsISupports**)&pEventQService);
    if (NS_SUCCEEDED(rv) && pEventQService)
        pEventQService->GetThreadEventQueue(PR_GetCurrentThread(),
                                            &eventQueue);
    if (pEventQService)
        nsServiceManager::ReleaseService(kEventQueueServiceCID,
                                         pEventQService);

    rv = aImapMailFolder->GetName(&folderName);
    if (folderName && *folderName && PL_strcasecmp(folderName, target) == 0 &&
        depth == 1)
    {
        nsIImapService* imapService = nsnull;
        nsCOMPtr<nsIUrlListener>aUrlListener(do_QueryInterface(aElement, &rv));

        rv = nsServiceManager::GetService(kCImapService,
                                          nsIImapService::GetIID(),
                                          (nsISupports **) &imapService);
        if (NS_SUCCEEDED(rv) && imapService)
        {
            rv = imapService->SelectFolder(eventQueue,
                                           aImapMailFolder, aUrlListener,
                                           nsnull);
        }
        if (imapService)
            nsServiceManager::ReleaseService(kCImapService, imapService);

        keepGoing = PR_FALSE;
    }
    delete [] folderName;
    return keepGoing;
}

NS_IMETHODIMP nsImapMailFolder::GetMessages(nsIEnumerator* *result)
{
    nsresult rv;
	if (result)
		*result = nsnull;

	rv = GetDatabase();

	if(NS_SUCCEEDED(rv))
	{
		nsIEnumerator *msgHdrEnumerator = nsnull;
		nsMessageFromMsgHdrEnumerator *messageEnumerator = nsnull;
		rv = m_mailDatabase->EnumerateMessages(&msgHdrEnumerator);
		if(NS_SUCCEEDED(rv))
			rv = NS_NewMessageFromMsgHdrEnumerator(msgHdrEnumerator,
												   this, &messageEnumerator);
		*result = messageEnumerator;
		NS_IF_RELEASE(msgHdrEnumerator);
	}
	else
		return rv;


	if (!NS_SUCCEEDED(rv))
		return rv;

    nsIImapService* imapService = nsnull;

    rv = nsServiceManager::GetService(kCImapService, nsIImapService::GetIID(),
                                      (nsISupports **) &imapService);
    if (NS_FAILED(rv))
    {
        if (imapService)
            nsServiceManager::ReleaseService(kCImapService, imapService);
        return rv;
    }
    if (imapService && m_eventQueue)
    {
        if (mDepth == 0)
        {
            rv = imapService->DiscoverAllFolders(m_eventQueue, this, this,
                                                 nsnull);
            mSubFolders->EnumerateForwards(FindAndSelectFolder, 
                                           (void*) "Inbox");
        }
        else
        {
            rv = imapService->DiscoverChildren(m_eventQueue, this, this,
                                               nsnull);
            rv = imapService->SelectFolder(m_eventQueue, this, this, nsnull);
        }
        m_urlRunning = PR_TRUE;
    }
    if (imapService)
            nsServiceManager::ReleaseService(kCImapService, imapService);

	return rv;
}

NS_IMETHODIMP nsImapMailFolder::GetThreads(nsIEnumerator** threadEnumerator)
{
    nsresult rv = NS_ERROR_FAILURE;
    return rv;
}

NS_IMETHODIMP nsImapMailFolder::GetThreadForMessage(nsIMessage *message,
                                                    nsIMsgThread **thread)
{
    nsresult rv = NS_ERROR_FAILURE;
    return rv;
}

NS_IMETHODIMP nsImapMailFolder::CreateSubfolder(const char *folderName)
{
	nsresult rv = NS_OK;
    
	nsFileSpec path;
    nsIMsgFolder *child = nsnull;
	//Get a directory based on our current path.
	rv = CreateDirectoryForFolder(path);
	if(NS_FAILED(rv))
		return rv;


	//Now we have a valid directory or we have returned.
	//Make sure the new folder name is valid
	path += folderName;
	path.MakeUnique();

	nsOutputFileStream outputStream(path);	
   
	// Create an empty database for this mail folder, set its name from the user  
	nsIMsgDatabase * mailDBFactory = nsnull;

	rv = nsComponentManager::CreateInstance(kCMailDB, nsnull, nsIMsgDatabase::GetIID(), (void **) &mailDBFactory);
	if (NS_SUCCEEDED(rv) && mailDBFactory)
	{
        nsIMsgDatabase *unusedDB = NULL;
		rv = mailDBFactory->Open(path, PR_TRUE, (nsIMsgDatabase **) &unusedDB, PR_TRUE);

        if (NS_SUCCEEDED(rv) && unusedDB)
        {
			//need to set the folder name
			nsIDBFolderInfo *folderInfo;
			rv = unusedDB->GetDBFolderInfo(&folderInfo);
			if(NS_SUCCEEDED(rv))
			{
				nsString folderNameStr(folderName);
				// ### DMB used to be leafNameFromUser?
				folderInfo->SetMailboxName(folderNameStr);
				NS_IF_RELEASE(folderInfo);
			}

			//Now let's create the actual new folder
			nsAutoString folderNameStr(folderName);
			rv = AddSubfolder(folderName, &child);
            unusedDB->SetSummaryValid(PR_TRUE);
            unusedDB->Close(PR_TRUE);
        }
        else
        {
			path.Delete(PR_FALSE);
            rv = NS_MSG_CANT_CREATE_FOLDER;
        }
		NS_IF_RELEASE(mailDBFactory);
	}
	if(rv == NS_OK && child)
	{
		nsISupports *folderSupports;

		rv = child->QueryInterface(kISupportsIID, (void**)&folderSupports);
		if(NS_SUCCEEDED(rv))
		{
			NotifyItemAdded(folderSupports);
			NS_IF_RELEASE(folderSupports);
		}
	}
	NS_IF_RELEASE(child);
	return rv;
}
    
NS_IMETHODIMP nsImapMailFolder::RemoveSubFolder (nsIMsgFolder *which)
{
    nsresult rv = NS_ERROR_FAILURE;
    return rv;
}

NS_IMETHODIMP nsImapMailFolder::Delete ()
{
    nsresult rv = NS_ERROR_FAILURE;
    return rv;
}

NS_IMETHODIMP nsImapMailFolder::Rename (const char *newName)
{
    nsresult rv = NS_ERROR_FAILURE;
    return rv;
}

NS_IMETHODIMP nsImapMailFolder::GetChildNamed(nsString& name, nsISupports **
                                              aChild)
{
    nsresult rv = NS_ERROR_FAILURE;
    return rv;
}

NS_IMETHODIMP nsImapMailFolder::GetName(char ** name)
{
	nsresult result = NS_OK;

    if(!name)
        return NS_ERROR_NULL_POINTER;
    
    if (!m_haveReadNameFromDB)
    {
        if (mDepth == 0) 
        {
            char *hostName = nsnull;
            GetHostName(&hostName);
            SetName(hostName);
            PR_FREEIF(hostName);
            m_haveReadNameFromDB = PR_TRUE;
            *name = mName.ToNewCString();
            return NS_OK;
        }
        else
        {
            //Need to read the name from the database
			result = GetDatabase();
			if (NS_SUCCEEDED(result) && m_mailDatabase)
			{
				nsString folderName;

				nsIDBFolderInfo *dbFolderInfo = nsnull;
				m_mailDatabase->GetDBFolderInfo(&dbFolderInfo);
				if (dbFolderInfo)
				{
					dbFolderInfo->GetMailboxName(folderName);
					m_haveReadNameFromDB = PR_TRUE;
					NS_RELEASE(dbFolderInfo);
					*name = folderName.ToNewCString();
					return NS_OK;
				}
			}
        }
    }
	nsAutoString folderName;
	nsImapURI2Name(kImapRootURI, mURI, folderName);
	*name = folderName.ToNewCString();
    
    return NS_OK;
}

NS_IMETHODIMP nsImapMailFolder::GetPrettyName(nsString& prettyName)
{
    if (mDepth == 1) {
        char *hostName = nsnull;
        GetHostName(&hostName);
        prettyName = PL_strdup(hostName);
        PR_FREEIF(hostName);
    }
    else {
        nsresult rv = NS_ERROR_NULL_POINTER;
        /**** what is this??? doesn't make sense to me
        char *pName = prettyName.ToNewCString();
        if (pName)
            rv = nsMsgFolder::GetPrettyName(&pName);
        delete[] pName;
        *****/
        return rv;
    }
    
    return NS_OK;
}
    
NS_IMETHODIMP nsImapMailFolder::BuildFolderURL(char **url)
{
    nsresult rv = NS_ERROR_FAILURE;
    return rv;
}
    
NS_IMETHODIMP nsImapMailFolder::UpdateSummaryTotals() 
{
    nsresult rv = NS_ERROR_FAILURE;
    return rv;
}
    
NS_IMETHODIMP nsImapMailFolder::GetExpungedBytesCount(PRUint32 *count)
{
    nsresult rv = NS_ERROR_FAILURE;
    return rv;
}

NS_IMETHODIMP nsImapMailFolder::GetDeletable (PRBool *deletable)
{
    nsresult rv = NS_ERROR_FAILURE;
    return rv;
}

NS_IMETHODIMP nsImapMailFolder::GetCanCreateChildren (PRBool
                                                      *canCreateChildren) 
{
    nsresult rv = NS_ERROR_FAILURE;
    return rv;
}

NS_IMETHODIMP nsImapMailFolder::GetCanBeRenamed (PRBool *canBeRenamed)
{
    nsresult rv = NS_ERROR_FAILURE;
    return rv;
}

NS_IMETHODIMP nsImapMailFolder::GetRequiresCleanup(PRBool *requiresCleanup)
{
    nsresult rv = NS_ERROR_FAILURE;
    return rv;
}

    
NS_IMETHODIMP nsImapMailFolder::GetSizeOnDisk(PRUint32 size)
{
    nsresult rv = NS_ERROR_FAILURE;
    return rv;
}
    
NS_IMETHODIMP nsImapMailFolder::GetUsersName(char** userName)
{
    nsresult rv = NS_ERROR_NULL_POINTER;
    nsIMsgMailSession *session = nsnull;
    NS_PRECONDITION (userName, "Oops ... null userName pointer");
    if (!userName)
        return rv;
    else
        *userName = nsnull;

    rv = nsServiceManager::GetService(kMsgMailSessionCID,
                                      nsIMsgMailSession::GetIID(),
                                      (nsISupports **)&session);
    
    if (NS_SUCCEEDED(rv) && session) 
    {
      nsIMsgIncomingServer *server = nsnull;
      rv = session->GetCurrentServer(&server);

      if (NS_SUCCEEDED(rv) && server)
          rv = server->GetUserName(userName);
      NS_IF_RELEASE (server);
    }

    if (session)
        nsServiceManager::ReleaseService(kMsgMailSessionCID, session);

    return rv;
}

NS_IMETHODIMP nsImapMailFolder::GetHostName(char** hostName)
{
    nsresult rv = NS_ERROR_NULL_POINTER;
    nsIMsgMailSession *session = nsnull;

    NS_PRECONDITION (hostName, "Oops ... null userName pointer");
    if (!hostName)
        return rv;
    else
        *hostName = nsnull;

    rv = nsServiceManager::GetService(kMsgMailSessionCID,
                                      nsIMsgMailSession::GetIID(),
                                      (nsISupports **)&session);
    
    if (NS_SUCCEEDED(rv) && session) 
    {
      nsIMsgIncomingServer *server = nsnull;
      rv = session->GetCurrentServer(&server);

      if (NS_SUCCEEDED(rv) && server)
          rv = server->GetHostName(hostName);
      NS_IF_RELEASE (server);
    }

    if (session)
        nsServiceManager::ReleaseService(kMsgMailSessionCID, session);

    return rv;
}

NS_IMETHODIMP nsImapMailFolder::UserNeedsToAuthenticateForFolder(PRBool
                                                                 displayOnly,
                                                                 PRBool
                                                                 *authenticate)
{
    nsresult rv = NS_ERROR_FAILURE;
    return rv;
}

NS_IMETHODIMP nsImapMailFolder::RememberPassword(char *password)
{
    nsresult rv = NS_ERROR_FAILURE;
    return rv;
}

NS_IMETHODIMP nsImapMailFolder::GetRememberedPassword(char ** password)
{
    nsresult rv = NS_ERROR_FAILURE;
    return rv;
}


NS_IMETHODIMP nsImapMailFolder::Adopt(nsIMsgFolder *srcFolder, 
                                      PRUint32 *outPos)
{
    nsresult rv = NS_ERROR_FAILURE;
    return rv;
}

nsresult nsImapMailFolder::GetDBFolderInfoAndDB(
    nsIDBFolderInfo **folderInfo, nsIMsgDatabase **db)
{
    nsresult rv = NS_ERROR_FAILURE;
    return rv;
}

NS_IMETHODIMP nsImapMailFolder::DeleteMessage(nsIMessage* message)
{
    nsresult rv = NS_ERROR_FAILURE;
    return rv;
}

NS_IMETHODIMP nsImapMailFolder::PossibleImapMailbox(
	nsIImapProtocol* aProtocol, mailbox_spec* aSpec)
{
	nsresult rv;
    PRBool found = PR_FALSE;
    nsIMsgFolder* aFolder = nsnull;
    nsISupports* aItem;
    nsIBidirectionalEnumerator *aEnumerator = nsnull;

    rv = NS_NewISupportsArrayEnumerator(mSubFolders, &aEnumerator);
    if (NS_FAILED(rv)) return rv;
    
    rv = aEnumerator->First();
    while (rv == NS_OK)
    {
        rv = aEnumerator->CurrentItem(&aItem);
        if (rv != NS_OK) break;
        aFolder = nsnull;
        rv = aItem->QueryInterface(nsIMsgFolder::GetIID(), (void**) &aFolder);
        aItem->Release();
        if (rv == NS_OK && aFolder)
        {
            char* aName = nsnull;
            aFolder->GetName(&aName);
            NS_RELEASE (aFolder);
            if (PL_strcmp(aName, aSpec->allocatedPathName) == 0)
            {
                found = PR_TRUE;
                break;
            }
        }
        rv = aEnumerator->Next();
    }
    if (!found)
    {
        nsIMsgFolder *child = nsnull;
        nsAutoString folderName = aSpec->allocatedPathName;
        AddSubfolder(folderName, &child);
        NS_IF_RELEASE(child);
    }
    aEnumerator->Release();
	return NS_OK;
}

NS_IMETHODIMP nsImapMailFolder::MailboxDiscoveryDone(
	nsIImapProtocol* aProtocol)
{
	nsresult rv = NS_ERROR_FAILURE;
	return rv;
}

NS_IMETHODIMP nsImapMailFolder::UpdateImapMailboxInfo(
	nsIImapProtocol* aProtocol,	mailbox_spec* aSpec)
{
	nsresult rv = NS_ERROR_FAILURE;
    nsIMsgDatabase* mailDBFactory;
    nsNativeFileSpec dbName;

    GetPathName(dbName);

    rv = nsComponentManager::CreateInstance(kCImapDB, nsnull,
                                            nsIMsgDatabase::GetIID(),
                                            (void **) &mailDBFactory);
    if (NS_FAILED(rv)) return rv;

    if (!m_mailDatabase)
    {
        // if we pass in PR_TRUE for upgrading, the db code will ignore the
        // summary out of date problem for now.
        rv = mailDBFactory->Open(dbName, PR_TRUE, (nsIMsgDatabase **)
                                 &m_mailDatabase, PR_TRUE);
        if (NS_FAILED(rv))
        { 
            NS_IF_RELEASE (mailDBFactory);
            return rv;
        }
        if (!m_mailDatabase) 
        {
            NS_IF_RELEASE (mailDBFactory);
            return NS_ERROR_NULL_POINTER;
        }
        m_mailDatabase->AddListener(this);
    }
    if (aSpec->folderSelected)
    {
     	nsMsgKeyArray existingKeys;
    	nsMsgKeyArray keysToDelete;
    	nsMsgKeyArray keysToFetch;
		nsIDBFolderInfo *dbFolderInfo = nsnull;
		PRInt32 imapUIDValidity = 0;

		rv = m_mailDatabase->GetDBFolderInfo(&dbFolderInfo);

		if (NS_SUCCEEDED(rv) && dbFolderInfo)
			dbFolderInfo->GetImapUidValidity(&imapUIDValidity);
    	m_mailDatabase->ListAllKeys(existingKeys);
    	if (m_mailDatabase->ListAllOfflineDeletes(&existingKeys) > 0)
			existingKeys.QuickSort();
    	if ((imapUIDValidity != aSpec->folder_UIDVALIDITY)	/* && // if UIDVALIDITY Changed 
    		!NET_IsOffline() */)
    	{

			nsIMsgDatabase *saveMailDB = m_mailDatabase;
#if TRANSFER_INFO
			TNeoFolderInfoTransfer *originalInfo = NULL;
			originalInfo = new TNeoFolderInfoTransfer(dbFolderInfo);
#endif // 0
			m_mailDatabase->ForceClosed();
			m_mailDatabase = NULL;
				
			nsLocalFolderSummarySpec	summarySpec(dbName);
			// Remove summary file.
			summarySpec.Delete(PR_FALSE);
			
			// Create a new summary file, update the folder message counts, and
			// Close the summary file db.
			rv = mailDBFactory->Open(dbName, PR_TRUE, &m_mailDatabase, PR_FALSE);
			if (NS_SUCCEEDED(rv))
			{
#if TRANSFER_INFO
				if (originalInfo)
				{
					originalInfo->TransferFolderInfo(*m_mailDatabase->m_dbFolderInfo);
					delete originalInfo;
				}
				SummaryChanged();
                m_mailDatabase->AddListener(this);
#endif
			}
			// store the new UIDVALIDITY value
			rv = m_mailDatabase->GetDBFolderInfo(&dbFolderInfo);

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
    		m_mailDatabase->DeleteMessages(&keysToDelete,NULL);
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
        dbName.Delete(PR_FALSE);

    NS_IF_RELEASE (mailDBFactory);
	return rv;
}

NS_IMETHODIMP nsImapMailFolder::UpdateImapMailboxStatus(
	nsIImapProtocol* aProtocol,	mailbox_spec* aSpec)
{
	nsresult rv = NS_ERROR_FAILURE;
	return rv;
}

NS_IMETHODIMP nsImapMailFolder::ChildDiscoverySucceeded(
	nsIImapProtocol* aProtocol)
{
	nsresult rv = NS_ERROR_FAILURE;
	return rv;
}

NS_IMETHODIMP nsImapMailFolder::OnlineFolderDelete(
	nsIImapProtocol* aProtocol, const char* folderName)
{
	nsresult rv = NS_ERROR_FAILURE;
	return rv;
}

NS_IMETHODIMP nsImapMailFolder::OnlineFolderCreateFailed(
	nsIImapProtocol* aProtocol, const char* folderName)
{
	nsresult rv = NS_ERROR_FAILURE;
	return rv;
}

NS_IMETHODIMP nsImapMailFolder::OnlineFolderRename(
    nsIImapProtocol* aProtocol, folder_rename_struct* aStruct) 
{
    nsresult rv = NS_ERROR_FAILURE;
	return rv;
}

NS_IMETHODIMP nsImapMailFolder::SubscribeUpgradeFinished(
	nsIImapProtocol* aProtocol, EIMAPSubscriptionUpgradeState* aState)
{
	nsresult rv = NS_ERROR_FAILURE;
	return rv;
}

NS_IMETHODIMP nsImapMailFolder::PromptUserForSubscribeUpdatePath(
	nsIImapProtocol* aProtocol,	PRBool* aBool)
{
	nsresult rv = NS_ERROR_FAILURE;
	return rv;
}

NS_IMETHODIMP nsImapMailFolder::FolderIsNoSelect(nsIImapProtocol* aProtocol,
												 FolderQueryInfo* aInfo)
{
	nsresult rv = NS_ERROR_FAILURE;
	return rv;
}

NS_IMETHODIMP nsImapMailFolder::SetupHeaderParseStream(
    nsIImapProtocol* aProtocol, StreamInfo* aStreamInfo)
{
    nsresult rv = NS_ERROR_FAILURE;

	m_nextMessageByteLength = aStreamInfo->size;
	if (!m_msgParser)
	{
		m_msgParser = new nsParseMailMessageState;
		m_msgParser->SetMailDB(m_mailDatabase);
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
    return rv;
}

NS_IMETHODIMP nsImapMailFolder::ParseAdoptedHeaderLine(
    nsIImapProtocol* aProtocol, msg_line_info* aMsgLineInfo)
{
    // we can get blocks that contain more than one line, 
    // but they never contain partial lines
	char *str = aMsgLineInfo->adoptedMessageLine;
	m_curMsgUid = aMsgLineInfo->uidOfMessage;
	m_msgParser->m_envelope_pos = m_curMsgUid;	// OK, this is silly (but
                                                // we'll fix
                                                // it). m_envelope_pos, for
                                                // local folders, 
    // is the msg key. Setting this will set the msg key for the new header.

	PRInt32 len = nsCRT::strlen(str);
    char *currentEOL  = PL_strstr(str, LINEBREAK);
    const char *currentLine = str;
    while (currentLine < (str + len))
    {
        if (currentEOL)
        {
            m_msgParser->ParseFolderLine(currentLine, 
                                         (currentEOL + LINEBREAK_LEN) -
                                         currentLine);
            currentLine = currentEOL + LINEBREAK_LEN;
            currentEOL  = PL_strstr(currentLine, LINEBREAK);
        }
        else
        {
			m_msgParser->ParseFolderLine(currentLine, PL_strlen(currentLine));
            currentLine = str + len + 1;
        }
    }
    return NS_OK;
}
    
NS_IMETHODIMP nsImapMailFolder::NormalEndHeaderParseStream(nsIImapProtocol*
                                                           aProtocol)
{
	if (m_msgParser && m_msgParser->m_newMsgHdr)
	{
		m_msgParser->m_newMsgHdr->SetMessageKey(m_curMsgUid);
		TweakHeaderFlags(aProtocol, m_msgParser->m_newMsgHdr);
		// here we need to tweak flags from uid state..
		m_mailDatabase->AddNewHdrToDB(m_msgParser->m_newMsgHdr, PR_TRUE);
		m_msgParser->FinishHeader();
		if (m_mailDatabase)
			m_mailDatabase->Commit(kLargeCommit);	// don't really want to do this
                                            // for every message... 
											// but I can't find the event that
                                            // means we've finished getting
                                            // headers 
	}
    return NS_OK;
}
    
NS_IMETHODIMP nsImapMailFolder::AbortHeaderParseStream(nsIImapProtocol*
                                                       aProtocol)
{
    nsresult rv = NS_ERROR_FAILURE;
    return rv;
}
 
NS_IMETHODIMP nsImapMailFolder::CreateMessageFromMsgDBHdr(nsIMsgDBHdr *msgDBHdr, nsIMessage **message)
{
	
    nsresult rv; 
    NS_WITH_SERVICE(nsIRDFService, rdfService, kRDFServiceCID, &rv); 
    if (NS_FAILED(rv)) return rv;

	char* msgURI = nsnull;
	nsFileSpec path;
	nsMsgKey key;
    nsIRDFResource* res;

	rv = msgDBHdr->GetMessageKey(&key);

	if(NS_SUCCEEDED(rv))
		rv = GetPathName(path);

	if(NS_SUCCEEDED(rv))
		rv = nsBuildImapMessageURI(path, key, &msgURI);

	if(NS_SUCCEEDED(rv))
	{
		rv = rdfService->GetResource(msgURI, &res);
    }
	if(msgURI)
		PR_smprintf_free(msgURI);

	if(NS_SUCCEEDED(rv))
	{
		nsIMessage *messageResource;
		rv = res->QueryInterface(nsIMessage::GetIID(), (void**)&messageResource);
		if(NS_SUCCEEDED(rv))
		{
			//We know from our factory that imap message resources are going to be
			//nsImapMessages.
			nsImapMessage *imapMessage = NS_STATIC_CAST(nsImapMessage*, messageResource);
			imapMessage->SetMsgDBHdr(msgDBHdr);
			*message = messageResource;
		}
		NS_IF_RELEASE(res);
	}
	return rv;
}

  
NS_IMETHODIMP nsImapMailFolder::OnKeyChange(nsMsgKey aKeyChanged, 
											PRInt32 aFlags, 
											nsIDBChangeListener * aInstigator)
{
	nsresult rv = NS_ERROR_FAILURE;
	return rv;
}

NS_IMETHODIMP nsImapMailFolder::OnKeyDeleted(nsMsgKey aKeyChanged, 
											 PRInt32 aFlags, 
											 nsIDBChangeListener * aInstigator)
{
	nsIMsgDBHdr *pMsgDBHdr = nsnull;
	nsresult rv = m_mailDatabase->GetMsgHdrForKey(aKeyChanged, &pMsgDBHdr);
	if(NS_SUCCEEDED(rv))
	{
		nsIMessage *message = nsnull;
		rv = CreateMessageFromMsgDBHdr(pMsgDBHdr, &message);
		if(NS_SUCCEEDED(rv))
		{
			nsISupports *msgSupports;
			if(NS_SUCCEEDED(message->QueryInterface(kISupportsIID, (void**)&msgSupports)))
			{
				PRUint32 i;
				for(i = 0; i < mListeners->Count(); i++)
				{
					nsIFolderListener *listener = (nsIFolderListener*)mListeners->ElementAt(i);
					listener->OnItemRemoved(this, msgSupports);
					NS_RELEASE(listener);
				}
				NS_IF_RELEASE(msgSupports);
			}
			NS_IF_RELEASE(message);
			UpdateSummaryTotals();
		}
		NS_IF_RELEASE(pMsgDBHdr);
	}

	return NS_OK;
}

NS_IMETHODIMP nsImapMailFolder::OnKeyAdded(nsMsgKey aKeyChanged, 
										   PRInt32 aFlags, 
										   nsIDBChangeListener * aInstigator)
{
	nsresult rv;
	nsIMsgDBHdr *pMsgDBHdr;
	rv = m_mailDatabase->GetMsgHdrForKey(aKeyChanged, &pMsgDBHdr);
	if(NS_SUCCEEDED(rv))
	{
		nsIMessage *message;
		rv = CreateMessageFromMsgDBHdr(pMsgDBHdr, &message);
		if(NS_SUCCEEDED(rv))
		{
			nsISupports *msgSupports;
			if(message && NS_SUCCEEDED(message->QueryInterface(kISupportsIID, (void**)&msgSupports)))
			{
				NotifyItemAdded(msgSupports);
				NS_IF_RELEASE(msgSupports);
			}
			UpdateSummaryTotals();
			NS_IF_RELEASE(message);
		}
		NS_IF_RELEASE(pMsgDBHdr);
	}
	return NS_OK;}

NS_IMETHODIMP nsImapMailFolder::OnAnnouncerGoingAway(nsIDBChangeAnnouncer *
													 instigator)
{
	nsresult rv = NS_ERROR_FAILURE;
	return rv;
}

NS_IMETHODIMP nsImapMailFolder::BeginCopy(nsIMessage *message)
{
	nsresult rv = NS_ERROR_FAILURE;
	return rv;
}

NS_IMETHODIMP nsImapMailFolder::CopyData(nsIInputStream *aIStream,
										 PRInt32 aLength)
{
	nsresult rv = NS_ERROR_FAILURE;
	return rv;
}

NS_IMETHODIMP nsImapMailFolder::EndCopy(PRBool copySucceeded)
{
	nsresult rv = NS_ERROR_FAILURE;
	return rv;
}

// both of these algorithms assume that key arrays and flag states are sorted by increasing key.
void nsImapMailFolder::FindKeysToDelete(const nsMsgKeyArray &existingKeys, nsMsgKeyArray &keysToDelete, nsImapFlagAndUidState *flagState)
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

void nsImapMailFolder::FindKeysToAdd(const nsMsgKeyArray &existingKeys, nsMsgKeyArray &keysToFetch, nsImapFlagAndUidState *flagState)
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

void nsImapMailFolder::PrepareToAddHeadersToMailDB(nsIImapProtocol* aProtocol, const nsMsgKeyArray &keysToFetch,
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


void nsImapMailFolder::TweakHeaderFlags(nsIImapProtocol* aProtocol, nsIMsgDBHdr *tweakMe)
{
	if (m_mailDatabase && aProtocol && tweakMe)
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

NS_IMETHODIMP
nsImapMailFolder::SetupMsgWriteStream(nsIImapProtocol* aProtocol,
                                      StreamInfo* aStreamInfo)
{
	// create a temp file to write the message into. We need to do this because
	// we don't have pluggable converters yet. We want to let mkfile do the work of 
	// converting the message from RFC-822 to HTML before displaying it...
	PR_Delete(MESSAGE_PATH);
	m_tempMessageFile = PR_Open(MESSAGE_PATH, PR_WRONLY | PR_CREATE_FILE | PR_TRUNCATE, 00700);
    return NS_OK;
}

NS_IMETHODIMP 
nsImapMailFolder::ParseAdoptedMsgLine(nsIImapProtocol* aProtocol,
                                      msg_line_info* aMsgLineInfo)
{
   if (m_tempMessageFile)                                                      
         PR_Write(m_tempMessageFile,(void *) aMsgLineInfo->adoptedMessageLine, 
			 PL_strlen(aMsgLineInfo->adoptedMessageLine));
                                                                                                                                                  
     return NS_OK;                                                               
}
    
NS_IMETHODIMP
nsImapMailFolder::NormalEndMsgWriteStream(nsIImapProtocol* aProtocol)
{
	nsresult res = NS_OK;
	if (m_tempMessageFile)
	{
		nsIWebShell *webShell;

		PR_Close(m_tempMessageFile);
		m_tempMessageFile = nsnull;
		res = aProtocol->GetDisplayStream(&webShell);
		if (NS_SUCCEEDED(res) && webShell)
		{
			nsFilePath filePath(MESSAGE_PATH);
			nsFileURL  fileURL(filePath);
			char * message_path_url = PL_strdup(fileURL.GetAsString());

			res = webShell->LoadURL(nsAutoString(message_path_url).GetUnicode(), nsnull, PR_TRUE, nsURLReload, 0);

			PR_FREEIF(message_path_url);
		}
	}

	return res;
}

NS_IMETHODIMP
nsImapMailFolder::AbortMsgWriteStream(nsIImapProtocol* aProtocol)
{
    return NS_ERROR_FAILURE;
}
    
    // message move/copy related methods
NS_IMETHODIMP 
nsImapMailFolder::OnlineCopyReport(nsIImapProtocol* aProtocol,
                                   ImapOnlineCopyState* aCopyState)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsImapMailFolder::BeginMessageUpload(nsIImapProtocol* aProtocol)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsImapMailFolder::UploadMessageFile(nsIImapProtocol* aProtocol,
                                    UploadMessageInfo* aMsgInfo)
{
    return NS_ERROR_FAILURE;
}

    // message flags operation
NS_IMETHODIMP
nsImapMailFolder::NotifyMessageFlags(nsIImapProtocol* aProtocol,
                                     FlagsKeyStruct* aKeyStruct)
{
    nsMsgKey msgKey = aKeyStruct->key;
    imapMessageFlagsType flags = aKeyStruct->flags;
	if (NS_SUCCEEDED(GetDatabase()) && m_mailDatabase)
    {
        m_mailDatabase->MarkRead(msgKey, (flags & kImapMsgSeenFlag) != 0, nsnull);
        m_mailDatabase->MarkReplied(msgKey, (flags & kImapMsgAnsweredFlag) != 0, nsnull);
        m_mailDatabase->MarkMarked(msgKey, (flags & kImapMsgFlaggedFlag) != 0, nsnull);
    }
    return NS_OK;
}

NS_IMETHODIMP
nsImapMailFolder::NotifyMessageDeleted(nsIImapProtocol* aProtocol,
                                       delete_message_struct* aStruct)
{
	PRBool deleteAllMsgs = aStruct->deleteAllMsgs;
	const char *doomedKeyString = aStruct->msgIdString;

	PRBool showDeletedMessages = ShowDeletedMessages();

	if (deleteAllMsgs)
	{
#ifdef HAVE_PORT		
		TNeoFolderInfoTransfer *originalInfo = NULL;
		nsIMsgDatabase *folderDB;
		if (ImapMailDB::Open(GetPathname(), FALSE, &folderDB, GetMaster(), &wasCreated) == eSUCCESS)
		{
			originalInfo = new TNeoFolderInfoTransfer(*folderDB->m_neoFolderInfo);
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
				originalInfo->TransferFolderInfo(*folderDB->m_neoFolderInfo);
				delete originalInfo;
			}
			SummaryChanged();
			folderDB->Close();
		}
#endif
		// ### DMB - how to do this? Reload any thread pane because it's invalid now.
		return NS_OK;
	}

	char *keyTokenString = PL_strdup(doomedKeyString);
	nsMsgKeyArray affectedMessages;
	ParseUidString(keyTokenString, affectedMessages);

	if (doomedKeyString && !showDeletedMessages)
	{
		if (affectedMessages.GetSize() > 0)	// perhaps Search deleted these messages
		{
			GetDatabase();
			if (m_mailDatabase)
				m_mailDatabase->DeleteMessages(&affectedMessages, nsnull);
		}
		
	}
	else if (doomedKeyString)	// && !imapDeleteIsMoveToTrash
	{
		GetDatabase();
		if (m_mailDatabase)
			SetIMAPDeletedFlag(m_mailDatabase, affectedMessages, nsnull);
	}
	PR_FREEIF(keyTokenString);
	return NS_OK;
}

PRBool nsImapMailFolder::ShowDeletedMessages()
{
//	return (m_host->GetIMAPDeleteModel() == MSG_IMAPDeleteIsIMAPDelete);
	return PR_FALSE;
}


void nsImapMailFolder::ParseUidString(char *uidString, nsMsgKeyArray &keys)
{
	// This is in the form <id>,<id>, or <id1>:<id2>
	char curChar = *uidString;
	PRBool isRange = PR_FALSE;
	int32	curToken;
	int32	saveStartToken=0;

	for (char *curCharPtr = uidString; curChar && *curCharPtr;)
	{
		char *currentKeyToken = curCharPtr;
		curChar = *curCharPtr;
		while (curChar != ':' && curChar != ',' && curChar != '\0')
			curChar = *curCharPtr++;
		*(curCharPtr - 1) = '\0';
		curToken = atoi(currentKeyToken);
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


// store MSG_FLAG_IMAP_DELETED in the specified mailhdr records
void nsImapMailFolder::SetIMAPDeletedFlag(nsIMsgDatabase *mailDB, const nsMsgKeyArray &msgids, PRBool markDeleted)
{
	nsresult markStatus = 0;
	PRUint32 total = msgids.GetSize();

	for (PRUint32 index=0; !markStatus && (index < total); index++)
	{
		markStatus = mailDB->MarkImapDeleted(msgids[index], markDeleted, nsnull);
	}
}


NS_IMETHODIMP
nsImapMailFolder::GetMessageSizeFromDB(nsIImapProtocol* aProtocol,
                                       MessageSizeInfo* sizeInfo)
{
	nsresult rv = NS_ERROR_FAILURE;
	if (sizeInfo && sizeInfo->id && m_mailDatabase)
	{
		PRUint32 key = atoi(sizeInfo->id);
		nsIMsgDBHdr *mailHdr = nsnull;
		NS_ASSERTION(sizeInfo->idIsUid, "ids must be uids to get message size");
		if (sizeInfo->idIsUid)
			rv = m_mailDatabase->GetMsgHdrForKey(key, &mailHdr);
		if (NS_SUCCEEDED(rv) && mailHdr)
		{
			rv = mailHdr->GetMessageSize(&sizeInfo->size);
			NS_RELEASE(mailHdr);
		}
	}
    return rv;
}

NS_IMETHODIMP
nsImapMailFolder::OnStartRunningUrl(nsIURL *aUrl)
{
	NS_PRECONDITION(aUrl, "just a sanity check since this is a test program");
	m_urlRunning = PR_TRUE;
    return NS_OK;
}

NS_IMETHODIMP
nsImapMailFolder::OnStopRunningUrl(nsIURL *aUrl, nsresult aExitCode)
{
	NS_PRECONDITION(aUrl, "just a sanity check since this is a test program");
	nsresult rv = NS_OK;
	m_urlRunning = PR_FALSE;
	if (aUrl)
	{
		// query it for a mailnews interface for now....
		nsIMsgMailNewsUrl * mailUrl = nsnull;
		rv = aUrl->QueryInterface(nsIMsgMailNewsUrl::GetIID(),
                                  (void **) mailUrl);
		if (NS_SUCCEEDED(rv) && mailUrl)
		{
			mailUrl->UnRegisterListener(this);
            NS_RELEASE (mailUrl);
		}
	}
	return NS_OK;
}

    // nsIImapExtensionSink methods
NS_IMETHODIMP
nsImapMailFolder::SetUserAuthenticated(nsIImapProtocol* aProtocol,
                                       PRBool aBool)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsImapMailFolder::SetMailServerUrls(nsIImapProtocol* aProtocol,
                                    const char* hostName)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsImapMailFolder::SetMailAccountUrl(nsIImapProtocol* aProtocol,
                                    const char* hostName)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsImapMailFolder::ClearFolderRights(nsIImapProtocol* aProtocol,
                                    nsIMAPACLRightsInfo* aclRights)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsImapMailFolder::AddFolderRights(nsIImapProtocol* aProtocol,
                                  nsIMAPACLRightsInfo* aclRights)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsImapMailFolder::RefreshFolderRights(nsIImapProtocol* aProtocol,
                                      nsIMAPACLRightsInfo* aclRights)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsImapMailFolder::FolderNeedsACLInitialized(nsIImapProtocol* aProtocol,
                                            nsIMAPACLRightsInfo* aclRights)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsImapMailFolder::SetFolderAdminURL(nsIImapProtocol* aProtocol,
                                    FolderQueryInfo* aInfo)
{
    return NS_ERROR_FAILURE;
}

    
    // nsIImapMiscellaneousSink methods
NS_IMETHODIMP
nsImapMailFolder::AddSearchResult(nsIImapProtocol* aProtocol, 
                                  const char* searchHitLine)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsImapMailFolder::GetArbitraryHeaders(nsIImapProtocol* aProtocol,
                                      GenericInfo* aInfo)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsImapMailFolder::GetShouldDownloadArbitraryHeaders(nsIImapProtocol* aProtocol,
                                                    GenericInfo* aInfo)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsImapMailFolder::GetShowAttachmentsInline(nsIImapProtocol* aProtocol,
                                           PRBool* aBool)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsImapMailFolder::HeaderFetchCompleted(nsIImapProtocol* aProtocol)
{
    return NS_OK;
}

NS_IMETHODIMP
nsImapMailFolder::UpdateSecurityStatus(nsIImapProtocol* aProtocol)
{
    return NS_ERROR_FAILURE;
}

	// ****
NS_IMETHODIMP
nsImapMailFolder::FinishImapConnection(nsIImapProtocol* aProtocol)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsImapMailFolder::SetImapHostPassword(nsIImapProtocol* aProtocol,
                                      GenericInfo* aInfo)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsImapMailFolder::GetPasswordForUser(nsIImapProtocol* aProtocol,
                                     const char* userName)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsImapMailFolder::SetBiffStateAndUpdate(nsIImapProtocol* aProtocol,
                                        nsMsgBiffState biffState)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsImapMailFolder::GetStoredUIDValidity(nsIImapProtocol* aProtocol,
                                       uid_validity_info* aInfo)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsImapMailFolder::LiteSelectUIDValidity(nsIImapProtocol* aProtocol,
                                        PRUint32 uidValidity)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsImapMailFolder::FEAlert(nsIImapProtocol* aProtocol,
                          const char* aString)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsImapMailFolder::FEAlertFromServer(nsIImapProtocol* aProtocol,
                                    const char* aString)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsImapMailFolder::ProgressStatus(nsIImapProtocol* aProtocol,
                                 const char* statusMsg)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsImapMailFolder::PercentProgress(nsIImapProtocol* aProtocol,
                                  ProgressInfo* aInfo)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsImapMailFolder::PastPasswordCheck(nsIImapProtocol* aProtocol)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsImapMailFolder::CommitNamespaces(nsIImapProtocol* aProtocol,
                                   const char* hostName)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsImapMailFolder::CommitCapabilityForHost(nsIImapProtocol* aProtocol,
                                          const char* hostName)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsImapMailFolder::TunnelOutStream(nsIImapProtocol* aProtocol,
                                  msg_line_info* aInfo)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsImapMailFolder::ProcessTunnel(nsIImapProtocol* aProtocol,
                                TunnelInfo *aInfo)
{
    return NS_ERROR_FAILURE;
}

nsresult
nsImapMailFolder::CreateDirectoryForFolder(nsFileSpec &path) //** dup
{
	nsresult rv = NS_OK;

	rv = GetPathName(path);
	if(NS_FAILED(rv))
		return rv;

	if(!path.IsDirectory())
	{
		//If the current path isn't a directory, add directory separator
		//and test it out.
		rv = AddDirectorySeparator(path);
		if(NS_FAILED(rv))
			return rv;

		//If that doesn't exist, then we have to create this directory
		if(!path.IsDirectory())
		{
			//If for some reason there's a file with the directory separator
			//then we are going to fail.
			if(path.Exists())
			{
				return NS_MSG_COULD_NOT_CREATE_DIRECTORY;
			}
			//otherwise we need to create a new directory.
			else
			{
				path.CreateDirectory();
				//Above doesn't return an error value so let's see if
				//it was created.
				if(!path.IsDirectory())
					return NS_MSG_COULD_NOT_CREATE_DIRECTORY;
			}
		}
	}

	return rv;
}

