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
#include "nsMsgImapCID.h"

#include "nsString.h"

#include "nsIImapIncomingServer.h"
#include "nsIMAPHostSessionList.h"
#include "nsMsgIncomingServer.h"
#include "nsImapIncomingServer.h"
#include "nsIImapUrl.h"
#include "nsIUrlListener.h"
#include "nsIEventQueue.h"
#include "nsIImapProtocol.h"
#include "nsISupportsArray.h"
#include "nsVoidArray.h"
#include "nsCOMPtr.h"
#include "nsImapStringBundle.h"
#include "nsIPref.h"

#include "prmem.h"
#include "plstr.h"
#include "nsXPIDLString.h"
#include "nsIMsgFolder.h"
#include "nsIImapServerSink.h"
#include "nsImapUtils.h"
#include "nsIRDFService.h"
#include "nsRDFCID.h"
#include "nsINetSupportDialogService.h"

static NS_DEFINE_CID(kCImapHostSessionList, NS_IIMAPHOSTSESSIONLIST_CID);
static NS_DEFINE_CID(kImapProtocolCID, NS_IMAPPROTOCOL_CID);
static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kNetSupportDialogCID, NS_NETSUPPORTDIALOG_CID);

/* get some implementation from nsMsgIncomingServer */
class nsImapIncomingServer : public nsMsgIncomingServer,
                             public nsIImapIncomingServer,
							 public nsIImapServerSink
                             
{
public:
    NS_DECL_ISUPPORTS_INHERITED

    nsImapIncomingServer();
    virtual ~nsImapIncomingServer();

    // overriding nsMsgIncomingServer methods
	NS_IMETHOD SetKey(char * aKey);  // override nsMsgIncomingServer's implementation...
	NS_IMETHOD GetServerURI(char * *aServerURI);

	// we support the nsIImapIncomingServer interface
    NS_IMETHOD GetMaximumConnectionsNumber(PRInt32* maxConnections);
    NS_IMETHOD SetMaximumConnectionsNumber(PRInt32 maxConnections);

    NS_IMETHOD GetTimeOutLimits(PRInt32* minutes);
    NS_IMETHOD SetTimeOutLimits(PRInt32 minutes);
    
    NS_IMETHOD GetImapConnectionAndLoadUrl(nsIEventQueue* aClientEventQueue,
                                           nsIImapUrl* aImapUrl,
                                           nsIUrlListener* aUrlListener = 0,
                                           nsISupports* aConsumer = 0,
                                           nsIURI** aURL = 0);
    NS_IMETHOD LoadNextQueuedUrl();
    NS_IMETHOD RemoveConnection(nsIImapProtocol* aImapConnection);

	/* attribute string personal namespace; */
	NS_IMETHOD GetPersonalNamespace(char * *aPersonalNamespace);
	NS_IMETHOD SetPersonalNamespace(char * aPersonalNamespace);

	/* attribute string public namespace; */
	NS_IMETHOD GetPublicNamespace(char * *aPublicNamespace);
	NS_IMETHOD SetPublicNamespace(char * aPublicNamespace);

		/* attribute string personal namespace; */
	NS_IMETHOD GetOtherUsersNamespace(char * *aOtherUsersNamespace);
	NS_IMETHOD SetOtherUsersNamespace(char * aOtherUsersNamespace);

	NS_IMETHOD PerformBiff();

	NS_IMETHOD GetUnverifiedFolders(nsISupportsArray *aFolderArray, PRInt32 *aNumUnverifiedFolders);

	// nsIImapServerSink impl
	NS_IMETHOD PossibleImapMailbox(const char *folderPath);
	NS_IMETHOD DiscoveryDone();
	NS_IMETHOD  FEAlert(const PRUnichar *aString);
	NS_IMETHOD  FEAlertFromServer(const char *aString);
private:
    nsresult CreateImapConnection (nsIEventQueue* aEventQueue,
                                   nsIImapUrl* aImapUrl,
                                   nsIImapProtocol** aImapConnection);
    PRBool ConnectionTimeOut(nsIImapProtocol* aImapConnection);
	char *m_rootFolderPath;
    nsCOMPtr<nsISupportsArray> m_connectionCache;
    nsCOMPtr<nsISupportsArray> m_urlQueue;
    nsVoidArray m_urlConsumers;
};


NS_IMPL_ADDREF_INHERITED(nsImapIncomingServer, nsMsgIncomingServer)
NS_IMPL_RELEASE_INHERITED(nsImapIncomingServer, nsMsgIncomingServer)

NS_IMETHODIMP nsImapIncomingServer::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
	if (!aInstancePtr) return NS_ERROR_NULL_POINTER;
	*aInstancePtr = nsnull;

    if (aIID.Equals(nsIImapServerSink::GetIID()))
	{
		*aInstancePtr = NS_STATIC_CAST(nsIImapServerSink*, this);
	}              
	else if(aIID.Equals(nsIImapIncomingServer::GetIID()))
	{
		*aInstancePtr = NS_STATIC_CAST(nsIImapIncomingServer*, this);
	}
	if(*aInstancePtr)
	{
		AddRef();
		return NS_OK;
	}

	return nsMsgIncomingServer::QueryInterface(aIID, aInstancePtr);
}

                            
nsImapIncomingServer::nsImapIncomingServer() : m_rootFolderPath(nsnull)
{    
    NS_INIT_REFCNT();
    nsresult rv;
	rv = NS_NewISupportsArray(getter_AddRefs(m_connectionCache));
    rv = NS_NewISupportsArray(getter_AddRefs(m_urlQueue));
}

nsImapIncomingServer::~nsImapIncomingServer()
{
	PR_FREEIF(m_rootFolderPath);
}

NS_IMETHODIMP nsImapIncomingServer::SetKey(char * aKey)  // override nsMsgIncomingServer's implementation...
{
	nsMsgIncomingServer::SetKey(aKey);

	// okay now that the key has been set, we need to add ourselves to the
	// host session list...

	// every time we create an imap incoming server, we need to add it to the
	// host session list!! 

	// get the user and host name and the fields to the host session list.
	char * userName = nsnull;
	char * hostName = nsnull;
	
	nsresult rv = GetHostName(&hostName);
	rv = GetUsername(&userName);

	NS_WITH_SERVICE(nsIImapHostSessionList, hostSession, kCImapHostSessionList, &rv);
    if (NS_FAILED(rv)) return rv;

	hostSession->AddHostToList(hostName, userName);

	char *personalNamespace = nsnull;
	char *publicNamespace = nsnull;
	char *otherUsersNamespace = nsnull;

	rv = GetPersonalNamespace(&personalNamespace);
	rv = GetPublicNamespace(&publicNamespace);
	rv = GetOtherUsersNamespace(&otherUsersNamespace);

	if (!personalNamespace && !publicNamespace && !otherUsersNamespace)
		personalNamespace = PL_strdup("\"\"");

	if (NS_SUCCEEDED(rv))
	{
		hostSession->SetNamespaceFromPrefForHost(hostName, userName, personalNamespace, kPersonalNamespace);
		PR_FREEIF(personalNamespace);
	}

	if (NS_SUCCEEDED(rv))
	{
		hostSession->SetNamespaceFromPrefForHost(hostName, userName, publicNamespace, kPublicNamespace);
		PR_FREEIF(publicNamespace);
	}

	if (NS_SUCCEEDED(rv))
	{
		hostSession->SetNamespaceFromPrefForHost(hostName, userName, otherUsersNamespace, kOtherUsersNamespace);
		PR_FREEIF(otherUsersNamespace);
	}

	PR_FREEIF(userName);
	PR_FREEIF(hostName);

	return rv;
}

NS_IMETHODIMP nsImapIncomingServer::GetServerURI(char ** aServerURI)
{
	nsresult rv = NS_OK;

    nsXPIDLCString hostname;
    rv = GetHostName(getter_Copies(hostname));

    nsXPIDLCString username;
    rv = GetUsername(getter_Copies(username));
    
    if (NS_FAILED(rv)) return rv;

    *aServerURI = PR_smprintf("imap://%s@%s", (const char*)username,
                              (const char*)hostname);

	return rv;
}

NS_IMPL_SERVERPREF_INT(nsImapIncomingServer, MaximumConnectionsNumber,
                       "max_cached_connections");

NS_IMPL_SERVERPREF_INT(nsImapIncomingServer, TimeOutLimits,
                       "timeout");

NS_IMPL_SERVERPREF_STR(nsImapIncomingServer, PersonalNamespace, "namespace.personal");

NS_IMPL_SERVERPREF_STR(nsImapIncomingServer, PublicNamespace, "namespace.public");

NS_IMPL_SERVERPREF_STR(nsImapIncomingServer, OtherUsersNamespace, "namespace.other_users");

NS_IMETHODIMP
nsImapIncomingServer::GetImapConnectionAndLoadUrl(nsIEventQueue*
                                                  aClientEventQueue,
                                                  nsIImapUrl* aImapUrl,
                                                  nsIUrlListener*
                                                  aUrlListener,
                                                  nsISupports* aConsumer,
                                                  nsIURI** aURL)
{
    nsresult rv = NS_OK;
    nsIImapProtocol* aProtocol = nsnull;
    
    rv = CreateImapConnection(aClientEventQueue, aImapUrl, &aProtocol);
    if (NS_FAILED(rv)) return rv;

	nsCOMPtr<nsIMsgMailNewsUrl> mailnewsurl = do_QueryInterface(aImapUrl, &rv);
    if (NS_SUCCEEDED(rv) && mailnewsurl && aUrlListener)
        mailnewsurl->RegisterListener(aUrlListener);

    if (aProtocol)
    {
        rv = aProtocol->LoadUrl(mailnewsurl, aConsumer);
        // *** jt - in case of the time out situation or the connection gets
        // terminated by some unforseen problems let's give it a second chance
        // to run the url
        if (NS_FAILED(rv))
        {
            rv = aProtocol->LoadUrl(mailnewsurl, aConsumer);
        }
        else
        {
            // *** jt - alert user that error has occurred
        }   
    }
    else
    {   // unable to get an imap connection to run the url; add to the url
        // queue
        PR_CEnterMonitor(this);
		nsCOMPtr <nsISupports> supports(do_QueryInterface(aImapUrl));
		if (supports)
			m_urlQueue->AppendElement(supports);
        m_urlConsumers.AppendElement((void*)aConsumer);
        NS_IF_ADDREF(aConsumer);
        PR_CExitMonitor(this);
    }
    if (aURL)
    {
        *aURL = mailnewsurl;
        NS_IF_ADDREF(*aURL);
    }

    return rv;
}

// checks to see if there are any queued urls on this incoming server,
// and if so, tries to run the oldest one.
NS_IMETHODIMP
nsImapIncomingServer::LoadNextQueuedUrl()
{
    PRUint32 cnt = 0;
    nsresult rv = NS_OK;

    PR_CEnterMonitor(this);
    m_urlQueue->Count(&cnt);
    if (cnt > 0)
    {
        nsCOMPtr<nsISupports>
            aSupport(getter_AddRefs(m_urlQueue->ElementAt(0)));
        nsCOMPtr<nsIImapUrl>
            aImapUrl(do_QueryInterface(aSupport, &rv));

        if (aImapUrl)
        {
            nsISupports *aConsumer =
                (nsISupports*)m_urlConsumers.ElementAt(0);

            NS_IF_ADDREF(aConsumer);
            
            nsIImapProtocol * protocolInstance = nsnull;
            rv = CreateImapConnection(nsnull, aImapUrl,
                                               &protocolInstance);
            if (NS_SUCCEEDED(rv) && protocolInstance)
            {
				nsCOMPtr<nsIURI> url = do_QueryInterface(aImapUrl, &rv);
				if (NS_SUCCEEDED(rv) && url)
					rv = protocolInstance->LoadUrl(url, aConsumer);
                m_urlQueue->RemoveElementAt(0);
                m_urlConsumers.RemoveElementAt(0);
            }

            NS_IF_RELEASE(aConsumer);
        }
    }
    PR_CExitMonitor(this);
    return rv;
}


NS_IMETHODIMP
nsImapIncomingServer::RemoveConnection(nsIImapProtocol* aImapConnection)
{
    PR_CEnterMonitor(this);

    if (aImapConnection)
        m_connectionCache->RemoveElement(aImapConnection);

    PR_CExitMonitor(this);
    return NS_OK;
}

PRBool
nsImapIncomingServer::ConnectionTimeOut(nsIImapProtocol* aConnection)
{
    PRBool retVal = PR_FALSE;
    if (!aConnection) return retVal;
    nsresult rv;

    PR_CEnterMonitor(this);
    PRInt32 timeoutInMinutes = 0;
    rv = GetTimeOutLimits(&timeoutInMinutes);
    if (NS_FAILED(rv) || timeoutInMinutes <= 0 || timeoutInMinutes > 29)
    {
        timeoutInMinutes = 29;
        SetTimeOutLimits(timeoutInMinutes);
    }

    PRTime cacheTimeoutLimits;

    LL_I2L(cacheTimeoutLimits, timeoutInMinutes * 60 * 1000000); // in
                                                              // microseconds
    PRTime lastActiveTimeStamp;
    rv = aConnection->GetLastActiveTimeStamp(&lastActiveTimeStamp);

    PRTime elapsedTime;
    LL_SUB(elapsedTime, PR_Now(), lastActiveTimeStamp);
    PRTime t;
    LL_SUB(t, elapsedTime, cacheTimeoutLimits);
    if (LL_GE_ZERO(t))
    {
        nsCOMPtr<nsIImapProtocol> aProtocol(do_QueryInterface(aConnection,
                                                              &rv));
        if (NS_SUCCEEDED(rv) && aProtocol)
        {
            m_connectionCache->RemoveElement(aConnection);
            aProtocol->TellThreadToDie(PR_TRUE);
            retVal = PR_TRUE;
        }
    }
    PR_CExitMonitor(this);
    return retVal;
}

nsresult
nsImapIncomingServer::CreateImapConnection(nsIEventQueue *aEventQueue, 
                                           nsIImapUrl * aImapUrl, 
                                           nsIImapProtocol ** aImapConnection)
{
	nsresult rv = NS_OK;
	PRBool canRunUrl = PR_FALSE;
    PRBool hasToWait = PR_FALSE;
	nsCOMPtr<nsIImapProtocol> connection;
    nsCOMPtr<nsIImapProtocol> freeConnection;
    PRBool isBusy = PR_FALSE;
    PRBool isInboxConnection = PR_FALSE;

    PR_CEnterMonitor(this);

    PRInt32 maxConnections = 5; // default to be five
    rv = GetMaximumConnectionsNumber(&maxConnections);
    if (NS_FAILED(rv) || maxConnections == 0)
    {
        maxConnections = 5;
        rv = SetMaximumConnectionsNumber(maxConnections);
    }
    else if (maxConnections < 2)
    {   // forced to use at least 2
        maxConnections = 2;
        rv = SetMaximumConnectionsNumber(maxConnections);
    }

    *aImapConnection = nsnull;
	// iterate through the connection cache for a connection that can handle this url.
	PRUint32 cnt;
    nsCOMPtr<nsISupports> aSupport;

    rv = m_connectionCache->Count(&cnt);
    if (NS_FAILED(rv)) return rv;
    for (PRUint32 i = 0; i < cnt && !canRunUrl && !hasToWait; i++) 
	{
        aSupport = getter_AddRefs(m_connectionCache->ElementAt(i));
        connection = do_QueryInterface(aSupport);
		if (connection)
			rv = connection->CanHandleUrl(aImapUrl, canRunUrl, hasToWait);
        if (NS_FAILED(rv)) 
        {
            connection = null_nsCOMPtr();
            continue;
        }
        if (!freeConnection && !canRunUrl && !hasToWait && connection)
        {
            rv = connection->IsBusy(isBusy, isInboxConnection);
            if (NS_FAILED(rv)) continue;
            if (!isBusy && !isInboxConnection)
                freeConnection = connection;
        }
	}
    
    if (ConnectionTimeOut(connection))
        connection = null_nsCOMPtr();
    if (ConnectionTimeOut(freeConnection))
        freeConnection = null_nsCOMPtr();

	// if we got here and we have a connection, then we should return it!
	if (canRunUrl && connection)
	{
		*aImapConnection = connection;
		NS_IF_ADDREF(*aImapConnection);
	}
    else if (hasToWait)
    {
        // do nothing; return NS_OK; for queuing
    }
	else if (cnt < maxConnections && aEventQueue)
	{	
		// create a new connection and add it to the connection cache
		// we may need to flag the protocol connection as busy so we don't get
        // a race 
		// condition where someone else goes through this code 
		nsIImapProtocol * protocolInstance = nsnull;
		rv = nsComponentManager::CreateInstance(kImapProtocolCID, nsnull,
                                                nsIImapProtocol::GetIID(),
                                                (void **) &protocolInstance);
		if (NS_SUCCEEDED(rv) && protocolInstance)
        {
            NS_WITH_SERVICE(nsIImapHostSessionList, hostSession,
                            kCImapHostSessionList, &rv);
            if (NS_SUCCEEDED(rv))
                rv = protocolInstance->Initialize(hostSession, aEventQueue);
        }
		
		// take the protocol instance and add it to the connectionCache
		if (protocolInstance)
			m_connectionCache->AppendElement(protocolInstance);
		*aImapConnection = protocolInstance; // this is already ref counted.

	}
    else if (freeConnection)
    {
        *aImapConnection = freeConnection;
        NS_IF_ADDREF(*aImapConnection);
    }
    else // cannot get anyone to handle the url queue it
    {
        // queue the url
    }

    PR_CExitMonitor(this);
	return rv;
}

NS_IMETHODIMP nsImapIncomingServer::PerformBiff()
{
	nsresult rv;

	nsCOMPtr<nsIFolder> rootFolder;
	rv = GetRootFolder(getter_AddRefs(rootFolder));
	if(NS_SUCCEEDED(rv))
	{
		nsCOMPtr<nsIMsgFolder> rootMsgFolder = do_QueryInterface(rootFolder);
		if(rootMsgFolder)
			rv = rootMsgFolder->GetNewMessages();
	}

	return rv;
}
    

nsresult NS_NewImapIncomingServer(const nsIID& iid,
                                  void **result)
{
    nsImapIncomingServer *server;
    if (!result) return NS_ERROR_NULL_POINTER;
    server = new nsImapIncomingServer();
    return server->QueryInterface(iid, result);
}

// nsIImapServerSink impl
NS_IMETHODIMP nsImapIncomingServer::PossibleImapMailbox(const char *folderPath)
{
	nsresult rv;
    PRBool found = PR_FALSE;
    nsCOMPtr<nsIMsgImapMailFolder> hostFolder;
    nsCOMPtr<nsIMsgFolder> aFolder;
 
    nsAutoString folderName = folderPath;
    nsCAutoString uri;
    uri.Append(kImapRootURI);
    uri.Append('/');

    char *username;
    GetUsername(&username);
    uri.Append(username);
    uri.Append('@');
	char *hostName = nsnull;
	GetHostName(&hostName);

	if (hostName)
	{
		uri.Append(hostName);
		nsAllocator::Free(hostName);
	}
#if 0    
    PRInt32 leafPos = folderName.RFindChar('/');
    if (leafPos > 0)
    {
        uri.Append('/');
        nsAutoString parentName(folderName);
        parentName.Truncate(leafPos);
        uri.Append(parentName);
    }
#endif 

	nsCOMPtr<nsIFolder> rootFolder;
    rv = GetRootFolder(getter_AddRefs(rootFolder));

    if(NS_FAILED(rv))
        return rv;
    nsCOMPtr<nsIMsgFolder> a_nsIFolder(do_QueryInterface(rootFolder, &rv));

    if (NS_FAILED(rv))
		return rv;

	hostFolder = do_QueryInterface(a_nsIFolder, &rv);
	if (NS_FAILED(rv))
		return rv;

	nsCOMPtr <nsIMsgFolder> child;

//	nsCString possibleName(aSpec->allocatedPathName);

	uri.Append('/');
	uri.Append(folderPath);
	a_nsIFolder->GetChildWithURI(uri, PR_TRUE, getter_AddRefs(child));

	if (child)
		found = PR_TRUE;
    if (!found)
    {
        hostFolder->CreateClientSubfolderInfo(folderPath);
    }
    
	return NS_OK;
}

NS_IMETHODIMP nsImapIncomingServer::DiscoveryDone()
{
	nsresult rv = NS_ERROR_FAILURE;
//	m_haveDiscoveredAllFolders = PR_TRUE;
#if 0
	if (currentContext->imapURLPane && (currentContext->imapURLPane->GetPaneType() == MSG_SUBSCRIBEPANE))
	{
		// Finished discovering folders for the subscribe pane.
		((MSG_SubscribePane *)(currentContext->imapURLPane))->ReportIMAPFolderDiscoveryFinished();
	}
	else
	{
		// only do this if we're discovering folders for real (i.e. not subscribe UI)

  
		if (URL_s && URL_s->msg_pane && !URL_s->msg_pane->GetPreImapFolderVerifyUrlExitFunction())
		{
    		URL_s->msg_pane->SetPreImapFolderVerifyUrlExitFunction(URL_s->pre_exit_fn);
    		URL_s->pre_exit_fn = DeleteNonVerifiedExitFunction;
		}

	    XP_ASSERT(currentContext->imapURLPane);

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
							if (currentFolder->GetExplicitlyVerify() ||
								((currentFolder->GetNumSubFolders() > 0) && !NoDescendantsAreVerified(currentFolder)))
							{
								// If there are no subfolders and this is unverified, we don't want to run
								// this url.  That is, we want to undiscover the folder.
								// If there are subfolders and no descendants are verified, we want to 
								// undiscover all of the folders.
								// Only if there are subfolders and at least one of them is verified do we want
								// to refresh that folder's flags, because it won't be going away.
								currentFolder->SetExplicitlyVerify(FALSE);
								char *url = CreateIMAPListFolderURL(hostName, currentFolder->GetOnlineName(), currentFolder->GetOnlineHierarchySeparator());
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

#endif
	return rv;
}


NS_IMETHODIMP
nsImapIncomingServer::FEAlert(const PRUnichar* aString)
{
	nsresult rv;
	NS_WITH_SERVICE(nsIPrompt, dialog, kNetSupportDialogCID, &rv);

	rv = dialog->Alert(nsAutoString(aString).GetUnicode());
    return rv;
}

NS_IMETHODIMP  nsImapIncomingServer::FEAlertFromServer(const char *aString)
{
	nsresult rv = NS_OK;
	NS_WITH_SERVICE(nsIPrompt, dialog, kNetSupportDialogCID, &rv);

	const char *serverSaid = aString;
	if (serverSaid)
	{
		// skip over the first two words, I guess.
		char *whereRealMessage = PL_strchr(serverSaid, ' ');
		if (whereRealMessage)
			whereRealMessage++;
		if (whereRealMessage)
			whereRealMessage = PL_strchr(whereRealMessage, ' ');
		if (whereRealMessage)
			whereRealMessage++;

		PRUnichar *serverSaidPrefix = IMAPGetStringByID(IMAP_SERVER_SAID);
		if (serverSaidPrefix)
		{
			nsAutoString message(serverSaidPrefix);
			message += whereRealMessage ? whereRealMessage : serverSaid;
			rv = dialog->Alert(message.GetUnicode());

			PR_Free(serverSaidPrefix);
		}
	}

    return rv;
}

NS_IMETHODIMP nsImapIncomingServer::GetUnverifiedFolders(nsISupportsArray *aFoldersArray, PRInt32 *aNumUnverifiedFolders)
{

	if (!aFoldersArray && !aNumUnverifiedFolders)
		return NS_ERROR_NULL_POINTER;

	nsCOMPtr<nsIFolder> rootFolder;
	nsresult rv = GetRootFolder(getter_AddRefs(rootFolder));
	if(NS_SUCCEEDED(rv))
	{
		nsCOMPtr<nsIMsgFolder> rootMsgFolder = do_QueryInterface(rootFolder);
		if(rootMsgFolder)
		{
		}
	}
	return NS_OK;
}

