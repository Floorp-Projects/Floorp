/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   David Bienvenu <bienvenu@netscape.com>
 *   Jeff Tsai <jefft@netscape.com>
 *   Scott MacGregor <mscott@netscape.com>
 *   Seth Spitzer <sspitzer@netscape.com>
 *   Alecf Flett <alecf@netscape.com>
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

#include "msgCore.h"
#include "nsMsgImapCID.h"

#include "nsString.h"

#include "nsIMAPHostSessionList.h"
#include "nsImapIncomingServer.h"
#include "nsIMsgAccountManager.h"
#include "nsIMsgIdentity.h"
#include "nsIImapUrl.h"
#include "nsIUrlListener.h"
#include "nsIEventQueue.h"
#include "nsIImapProtocol.h"
#include "nsISupportsArray.h"
#include "nsVoidArray.h"
#include "nsCOMPtr.h"
#include "nsImapStringBundle.h"
#include "nsIPref.h"
#include "nsMsgFolderFlags.h"
#include "nsTextFormatter.h"
#include "prmem.h"
#include "plstr.h"
#include "nsXPIDLString.h"
#include "nsIMsgFolder.h"
#include "nsIMsgWindow.h"
#include "nsIMsgImapMailFolder.h"
#include "nsImapUtils.h"
#include "nsIRDFService.h"
#include "nsRDFCID.h"
#include "nsINetSupportDialogService.h"
#include "nsEnumeratorUtils.h"
#include "nsIEventQueueService.h"
#include "nsIMsgMailNewsUrl.h"
#include "nsIImapService.h"
#include "nsMsgI18N.h"
#include "nsAutoLock.h"
#include "nsIImapMockChannel.h"
// for the memory cache...
#include "nsINetDataCacheManager.h"
#include "nsINetDataCache.h"
#include "nsICachedNetData.h"

#include "nsITimer.h"
static NS_DEFINE_CID(kCImapHostSessionList, NS_IIMAPHOSTSESSIONLIST_CID);
static NS_DEFINE_CID(kImapProtocolCID, NS_IMAPPROTOCOL_CID);
static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kNetSupportDialogCID, NS_NETSUPPORTDIALOG_CID);
static NS_DEFINE_CID(kPrefServiceCID, NS_PREF_CID);
static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_CID(kMsgLogonRedirectorServiceCID, NS_MSGLOGONREDIRECTORSERVICE_CID);
static NS_DEFINE_CID(kImapServiceCID, NS_IMAPSERVICE_CID);
static NS_DEFINE_CID(kSubscribableServerCID, NS_SUBSCRIBABLESERVER_CID);

NS_IMPL_ADDREF_INHERITED(nsImapIncomingServer, nsMsgIncomingServer)
NS_IMPL_RELEASE_INHERITED(nsImapIncomingServer, nsMsgIncomingServer)

NS_INTERFACE_MAP_BEGIN(nsImapIncomingServer)
	NS_INTERFACE_MAP_ENTRY(nsIImapServerSink)
	NS_INTERFACE_MAP_ENTRY(nsIImapIncomingServer)
	NS_INTERFACE_MAP_ENTRY(nsIMsgLogonRedirectionRequester)
	NS_INTERFACE_MAP_ENTRY(nsISubscribableServer)
	NS_INTERFACE_MAP_ENTRY(nsIUrlListener)
NS_INTERFACE_MAP_END_INHERITING(nsMsgIncomingServer)

nsImapIncomingServer::nsImapIncomingServer()
{    
    NS_INIT_REFCNT();
    nsresult rv;
	rv = NS_NewISupportsArray(getter_AddRefs(m_connectionCache));
    rv = NS_NewISupportsArray(getter_AddRefs(m_urlQueue));
	m_capability = kCapabilityUndefined;
	m_waitingForConnectionInfo = PR_FALSE;
	m_redirectedLogonRetries = 0;
	mDoingSubscribeDialog = PR_FALSE;
	mDoingLsub = PR_FALSE;
}

nsImapIncomingServer::~nsImapIncomingServer()
{
    CloseCachedConnections();
}

NS_IMETHODIMP nsImapIncomingServer::SetKey(const char * aKey)  // override nsMsgIncomingServer's implementation...
{
	nsMsgIncomingServer::SetKey(aKey);

	// okay now that the key has been set, we need to add ourselves to the
	// host session list...

	// every time we create an imap incoming server, we need to add it to the
	// host session list!! 

	nsresult rv;
	NS_WITH_SERVICE(nsIImapHostSessionList, hostSession, kCImapHostSessionList, &rv);
    if (NS_FAILED(rv)) return rv;

	hostSession->AddHostToList(aKey, this);
  nsMsgImapDeleteModel deleteModel = nsMsgImapDeleteModels::MoveToTrash; // default to trash
  GetDeleteModel(&deleteModel);
  hostSession->SetDeleteIsMoveToTrashForHost(aKey, deleteModel == nsMsgImapDeleteModels::MoveToTrash); 
  hostSession->SetShowDeletedMessagesForHost(aKey, deleteModel == nsMsgImapDeleteModels::IMAPDelete);

  nsXPIDLCString onlineDir;
  rv = GetServerDirectory(getter_Copies(onlineDir));
  if (NS_FAILED(rv)) return rv;
  if (onlineDir)
      hostSession->SetOnlineDirForHost(aKey, (const char *) onlineDir);

  nsXPIDLCString personalNamespace;
  nsXPIDLCString publicNamespace;
  nsXPIDLCString otherUsersNamespace;

  rv = GetPersonalNamespace(getter_Copies(personalNamespace));
  if (NS_FAILED(rv)) return rv;
  rv = GetPublicNamespace(getter_Copies(publicNamespace));
  if (NS_FAILED(rv)) return rv;
  rv = GetOtherUsersNamespace(getter_Copies(otherUsersNamespace));
  if (NS_FAILED(rv)) return rv;

  if (!personalNamespace && !publicNamespace && !otherUsersNamespace)
      personalNamespace = "\"\"";

  hostSession->SetNamespaceFromPrefForHost(aKey, personalNamespace,
                                           kPersonalNamespace);

  if (publicNamespace && PL_strlen(publicNamespace))
      hostSession->SetNamespaceFromPrefForHost(aKey, publicNamespace,
                                               kPublicNamespace);

  if (otherUsersNamespace && PL_strlen(otherUsersNamespace))
      hostSession->SetNamespaceFromPrefForHost(aKey, otherUsersNamespace,
                                               kOtherUsersNamespace);
  return rv;
}

// construct the pretty name to show to the user if they haven't
// specified one. This should be overridden for news and mail.
NS_IMETHODIMP
nsImapIncomingServer::GetConstructedPrettyName(PRUnichar **retval) 
{
    
  nsXPIDLCString username;
  nsXPIDLCString hostName;
  nsresult rv;

  NS_WITH_SERVICE(nsIMsgAccountManager, accountManager,
                  NS_MSGACCOUNTMANAGER_PROGID, &rv);
  if (NS_FAILED(rv)) return rv;

  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsISupportsArray> identities;
  rv = accountManager->GetIdentitiesForServer(this,
                                              getter_AddRefs(identities));
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIMsgIdentity> identity;

  rv = identities->QueryElementAt(0, NS_GET_IID(nsIMsgIdentity),
                                  (void **)getter_AddRefs(identity));

  nsAutoString emailAddress;

  if (NS_SUCCEEDED(rv) && identity)
  {
    nsXPIDLCString identityEmailAddress;
    identity->GetEmail(getter_Copies(identityEmailAddress));
    emailAddress.AssignWithConversion(identityEmailAddress);
  }
  else
  {
    rv = GetUsername(getter_Copies(username));
    if (NS_FAILED(rv)) return rv;
    rv = GetHostName(getter_Copies(hostName));
    if (NS_FAILED(rv)) return rv;
    if ((const char*)username && (const char *) hostName &&
        PL_strcmp((const char*)username, "")!=0) {
      emailAddress.AssignWithConversion(username);
      emailAddress.AppendWithConversion("@");
      emailAddress.AppendWithConversion(hostName);

    }
  }
  rv = GetStringBundle();
	if (m_stringBundle)
  {
    nsXPIDLString prettyName;
    const PRUnichar *formatStrings[] =
    {
        emailAddress.GetUnicode(),
    };
    rv = m_stringBundle->FormatStringFromID(IMAP_DEFAULT_ACCOUNT_NAME,
                                    formatStrings, 1,
                                    getter_Copies(prettyName));
    *retval = nsCRT::strdup(prettyName);
    return NS_OK;
  }
  else
    return rv;
}


NS_IMETHODIMP nsImapIncomingServer::GetLocalStoreType(char ** type)
{
    NS_ENSURE_ARG_POINTER(type);
    *type = nsCRT::strdup("imap");
    return NS_OK;
}

NS_IMETHODIMP
nsImapIncomingServer::GetServerDirectory(char **serverDirectory)
{
    return GetCharValue("server_sub_directory", serverDirectory);
}

NS_IMETHODIMP
nsImapIncomingServer::SetServerDirectory(const char *serverDirectory)
{
    nsCAutoString dirString(serverDirectory);

    if (dirString.Length() > 0)
    {
        if (dirString.Last() != '/')
            dirString += '/';
    }
    nsXPIDLCString serverKey;
    nsresult rv = GetKey(getter_Copies(serverKey));
    if (NS_SUCCEEDED(rv))
    {
        NS_WITH_SERVICE(nsIImapHostSessionList, hostSession,
                        kCImapHostSessionList, &rv);
        if (NS_SUCCEEDED(rv))
            hostSession->SetOnlineDirForHost(serverKey, dirString.GetBuffer());
    }
    return SetCharValue("server_sub_directory", dirString.GetBuffer());
}


NS_IMETHODIMP
nsImapIncomingServer::GetOverrideNamespaces(PRBool *bVal)
{
    return GetBoolValue("override_namespaces", bVal);
}

NS_IMETHODIMP
nsImapIncomingServer::SetOverrideNamespaces(PRBool bVal)
{
    nsXPIDLCString serverKey;
    GetKey(getter_Copies(serverKey));
    if (serverKey)
    {
        nsresult rv;
        NS_WITH_SERVICE(nsIImapHostSessionList, hostSession,
                        kCImapHostSessionList, &rv);
        if (NS_SUCCEEDED(rv))
            hostSession->SetNamespacesOverridableForHost(serverKey, bVal);
    }
    return SetBoolValue("override_namespaces", bVal);
}

NS_IMETHODIMP
nsImapIncomingServer::GetUsingSubscription(PRBool *bVal)
{
    return GetBoolValue("using_subscription", bVal);
}

NS_IMETHODIMP
nsImapIncomingServer::SetUsingSubscription(PRBool bVal)
{
    nsXPIDLCString serverKey;
    GetKey(getter_Copies(serverKey));
    if (serverKey)
    {
        nsresult rv;
        NS_WITH_SERVICE(nsIImapHostSessionList, hostSession,
                        kCImapHostSessionList, &rv);
        if (NS_SUCCEEDED(rv))
            hostSession->SetHostIsUsingSubscription(serverKey, bVal);
    }
    return SetBoolValue("using_subscription", bVal);
}

NS_IMPL_SERVERPREF_BOOL(nsImapIncomingServer, DualUseFolders,
                        "dual_use_folders");
			
			
NS_IMPL_SERVERPREF_STR(nsImapIncomingServer, AdminUrl,
                       "admin_url");

NS_IMPL_SERVERPREF_BOOL(nsImapIncomingServer, CleanupInboxOnExit,
                        "cleanup_inbox_on_exit");
			
NS_IMPL_SERVERPREF_BOOL(nsImapIncomingServer, OfflineDownload,
                        "offline_download");

NS_IMPL_SERVERPREF_INT(nsImapIncomingServer, MaximumConnectionsNumber,
                       "max_cached_connections");

NS_IMPL_SERVERPREF_INT(nsImapIncomingServer, EmptyTrashThreshhold,
                       "empty_trash_threshhold");

//NS_IMPL_SERVERPREF_INT(nsImapIncomingServer, DeleteModel,
//                       "delete_model");

NS_IMETHODIMP								   	
nsImapIncomingServer::GetDeleteModel(PRInt32 *retval)
{						
  PRBool isAOLServer = PR_FALSE;

  NS_ENSURE_ARG(retval);

  GetIsAOLServer(&isAOLServer);
  if (isAOLServer)
  {
    *retval = nsMsgImapDeleteModels::DeleteNoTrash;
    return NS_OK;
  }
  nsresult ret = GetIntValue("delete_model", retval);
  return ret;
}

NS_IMETHODIMP	   								\
nsImapIncomingServer::SetDeleteModel(PRInt32 ivalue)
{												\
  return SetIntValue("delete_model", ivalue);			
}

NS_IMPL_SERVERPREF_INT(nsImapIncomingServer, TimeOutLimits,
                       "timeout");

NS_IMPL_SERVERPREF_INT(nsImapIncomingServer, CapabilityPref,
                       "capability");

NS_IMPL_SERVERPREF_STR(nsImapIncomingServer, PersonalNamespace,
                       "namespace.personal");

NS_IMPL_SERVERPREF_STR(nsImapIncomingServer, PublicNamespace,
                       "namespace.public");

NS_IMPL_SERVERPREF_STR(nsImapIncomingServer, OtherUsersNamespace,
                       "namespace.other_users");

NS_IMPL_SERVERPREF_BOOL(nsImapIncomingServer, FetchByChunks,
                       "fetch_by_chunks");

NS_IMPL_SERVERPREF_BOOL(nsImapIncomingServer, MimePartsOnDemand,
                       "mime_parts_on_demand");

NS_IMPL_SERVERPREF_BOOL(nsImapIncomingServer, AOLMailboxView,
                       "aol_mailbox_view");

NS_IMETHODIMP
nsImapIncomingServer::GetIsAOLServer(PRBool *aBool)
{
	if (!aBool)
		return NS_ERROR_NULL_POINTER;
	*aBool = ((m_capability & kAOLImapCapability) != 0);
	return NS_OK;
}

NS_IMETHODIMP
nsImapIncomingServer::SetIsAOLServer(PRBool aBool)
{
	if (aBool)
		m_capability |= kAOLImapCapability;
	else
		m_capability &= ~kAOLImapCapability;
	return NS_OK;
}


NS_IMETHODIMP
nsImapIncomingServer::GetImapConnectionAndLoadUrl(nsIEventQueue * aClientEventQueue,
                                                  nsIImapUrl* aImapUrl,
                                                  nsISupports* aConsumer)
{
  nsresult rv = NS_OK;
  nsCOMPtr <nsIImapProtocol> aProtocol;
  
  rv = CreateImapConnection(aClientEventQueue, aImapUrl, getter_AddRefs(aProtocol));
  if (NS_FAILED(rv)) return rv;

	nsCOMPtr<nsIMsgMailNewsUrl> mailnewsurl = do_QueryInterface(aImapUrl, &rv);
  if (aProtocol)
  {
    rv = aProtocol->LoadUrl(mailnewsurl, aConsumer);
    // *** jt - in case of the time out situation or the connection gets
    // terminated by some unforseen problems let's give it a second chance
    // to run the url
    if (NS_FAILED(rv))
    {
      NS_ASSERTION(PR_FALSE, "shouldn't get an error loading url");
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

  return rv;
}

// checks to see if there are any queued urls on this incoming server,
// and if so, tries to run the oldest one.
NS_IMETHODIMP
nsImapIncomingServer::LoadNextQueuedUrl(PRBool *aResult)
{
  PRUint32 cnt = 0;
  nsresult rv = NS_OK;
	PRBool urlRun = PR_FALSE;
  PRBool removeUrlFromQueue = PR_FALSE;
  PRBool keepGoing = PR_TRUE;

  nsAutoCMonitor(this);
  m_urlQueue->Count(&cnt);

  while (cnt > 0 && !urlRun && keepGoing)
  {
    nsCOMPtr<nsISupports>
        aSupport(getter_AddRefs(m_urlQueue->ElementAt(0)));
    nsCOMPtr<nsIImapUrl>
        aImapUrl(do_QueryInterface(aSupport, &rv));

    if (aImapUrl)
    {
      nsCOMPtr <nsIImapMockChannel> mockChannel;
      nsCOMPtr<nsIURI> url(do_QueryInterface(aSupport, &rv));

      if (NS_SUCCEEDED(aImapUrl->GetMockChannel(getter_AddRefs(mockChannel))) && mockChannel)
      {
        mockChannel->GetStatus(&rv);
        if (!NS_SUCCEEDED(rv))
        {
          nsresult res;
          removeUrlFromQueue = PR_TRUE;

          mockChannel->Close(); // try closing it to get channel listener nulled out.

          nsCOMPtr<nsINetDataCacheManager> cacheManager = do_GetService(NS_NETWORK_CACHE_MANAGER_PROGID, &res);
          if (NS_SUCCEEDED(res) && cacheManager)
          {
            nsCOMPtr<nsICachedNetData>  cacheEntry;
            // Retrieve an existing cache entry or create a new one if none exists for the
            // given URL.
            nsXPIDLCString urlCString; 
            // eventually we are going to want to use the url spec - the query/ref part 'cause that doesn't
            // distinguish urls.......
            url->GetSpec(getter_Copies(urlCString));
            // for now, truncate of the query part so we don't duplicate urls in the cache...
            char * anchor = PL_strrchr(urlCString, '?');
            if (anchor)
              *anchor = '\0';
            res = cacheManager->GetCachedNetData(urlCString, 0, 0, nsINetDataCacheManager::BYPASS_PERSISTENT_CACHE,
                                                getter_AddRefs(cacheEntry));
            if (NS_SUCCEEDED(res) && cacheEntry)
              cacheEntry->Delete();
          }
        }
      }
      // Note that we're relying on no one diddling the rv from the mock channel status
      // between the place we set it and here.
      if (NS_SUCCEEDED(rv))
      {
        nsISupports *aConsumer =
            (nsISupports*)m_urlConsumers.ElementAt(0);

        NS_IF_ADDREF(aConsumer);
      
        nsCOMPtr <nsIImapProtocol>  protocolInstance ;
        rv = CreateImapConnection(nsnull, aImapUrl,
                                           getter_AddRefs(protocolInstance));
        if (NS_SUCCEEDED(rv) && protocolInstance)
        {
		      nsCOMPtr<nsIURI> url = do_QueryInterface(aImapUrl, &rv);
		      if (NS_SUCCEEDED(rv) && url)
		      {
			      rv = protocolInstance->LoadUrl(url, aConsumer);
            NS_ASSERTION(NS_SUCCEEDED(rv), "failed running queued url");
			      urlRun = PR_TRUE;
            removeUrlFromQueue = PR_TRUE;
		      }
        }
        else
          keepGoing = PR_FALSE;
        NS_IF_RELEASE(aConsumer);
      }
      if (removeUrlFromQueue)
      {
        m_urlQueue->RemoveElementAt(0);
        m_urlConsumers.RemoveElementAt(0);
      }
    }
    m_urlQueue->Count(&cnt);
  }
	if (aResult)
		*aResult = urlRun;

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
            aProtocol->TellThreadToDie(PR_FALSE);
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
	PRBool canRunUrlImmediately = PR_FALSE;
  PRBool canRunButBusy = PR_FALSE;
	nsCOMPtr<nsIImapProtocol> connection;
  nsCOMPtr<nsIImapProtocol> freeConnection;
  PRBool isBusy = PR_FALSE;
  PRBool isInboxConnection = PR_FALSE;
	nsXPIDLCString redirectorType;

  PR_CEnterMonitor(this);

	GetRedirectorType(getter_Copies(redirectorType));
	PRBool redirectLogon = ((const char *) redirectorType && nsCRT::strlen((const char *) redirectorType) > 0);

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
  // loop until we find a connection that can run the url, or doesn't have to wait?
  for (PRUint32 i = 0; i < cnt && !canRunUrlImmediately && !canRunButBusy; i++) 
	{
    aSupport = getter_AddRefs(m_connectionCache->ElementAt(i));
    connection = do_QueryInterface(aSupport);
		if (connection)
			rv = connection->CanHandleUrl(aImapUrl, &canRunUrlImmediately, &canRunButBusy);
    if (NS_FAILED(rv)) 
    {
        connection = null_nsCOMPtr();
        continue;
    }
    // if we haven't found a free connection, and this connection
    // is wrong, but it's not busy.
    if (!freeConnection && !canRunUrlImmediately && !canRunButBusy && connection)
    {
        rv = connection->IsBusy(&isBusy, &isInboxConnection);
        if (NS_FAILED(rv)) 
          continue;
        if (!isBusy && !isInboxConnection)
            freeConnection = connection;
    }
    // don't leave this loop with connection set if we can't use it!
    if (!canRunButBusy && !canRunUrlImmediately)
      connection = null_nsCOMPtr();
  }
  
  if (ConnectionTimeOut(connection))
      connection = null_nsCOMPtr();
  if (ConnectionTimeOut(freeConnection))
      freeConnection = null_nsCOMPtr();

	if (redirectLogon && (!connection || !canRunUrlImmediately))
	{
		// here's where we'd start the asynchronous process of requesting a connection to the 
		// AOL Imap server and getting back an ip address, port #, and cookie.
		// if m_overRideUrlConnectionInfo is true, then we can go ahead and make a connection to this server.
		// We should set some sort of timer on this override info.
		if (!m_waitingForConnectionInfo)
		{
			m_waitingForConnectionInfo = PR_TRUE;
			nsCOMPtr<nsIMsgMailNewsUrl> mailnewsUrl = do_QueryInterface(aImapUrl, &rv);
			nsCOMPtr<nsIMsgWindow> aMsgWindow;
			if (NS_SUCCEEDED(rv)) 
				rv = mailnewsUrl->GetMsgWindow(getter_AddRefs(aMsgWindow));

			RequestOverrideInfo(aMsgWindow);
			canRunButBusy = PR_TRUE;
		}
	}
	// if we got here and we have a connection, then we should return it!
	if (canRunUrlImmediately && connection)
	{
		*aImapConnection = connection;
		NS_IF_ADDREF(*aImapConnection);
	}
  else if (canRunButBusy)
  {
      // do nothing; return NS_OK; for queuing
  }
	else if (cnt < ((PRUint32)maxConnections) && aEventQueue)
	{	
		rv = CreateProtocolInstance(aEventQueue, aImapConnection);
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

nsresult
nsImapIncomingServer::CreateProtocolInstance(nsIEventQueue *aEventQueue, 
                                           nsIImapProtocol ** aImapConnection)
{
	// create a new connection and add it to the connection cache
	// we may need to flag the protocol connection as busy so we don't get
    // a race 
	// condition where someone else goes through this code 
	nsIImapProtocol * protocolInstance = nsnull;
	nsresult rv = nsComponentManager::CreateInstance(kImapProtocolCID, nsnull,
                                            NS_GET_IID(nsIImapProtocol),
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
	return rv;
}

NS_IMETHODIMP nsImapIncomingServer::CloseConnectionForFolder(nsIMsgFolder *aMsgFolder)
{
    nsresult rv = NS_OK;
    nsCOMPtr<nsIImapProtocol> connection;
    nsCOMPtr<nsISupports> aSupport;
    PRBool isBusy = PR_FALSE, isInbox = PR_FALSE;
    PRUint32 cnt = 0;
    nsXPIDLCString inFolderName;
    nsXPIDLCString connectionFolderName;
    nsCOMPtr <nsIMsgImapMailFolder> imapFolder = do_QueryInterface(aMsgFolder);

    if (!imapFolder)
      return NS_ERROR_NULL_POINTER;

    rv = m_connectionCache->Count(&cnt);
    if (NS_FAILED(rv)) return rv;

    imapFolder->GetOnlineName(getter_Copies(inFolderName));
    PR_CEnterMonitor(this);
    
    for (PRUint32 i=0; i < cnt; i++)
    {
        aSupport = getter_AddRefs(m_connectionCache->ElementAt(i));
        connection = do_QueryInterface(aSupport);
        if (connection)
        {
            rv = connection->GetSelectedMailboxName(getter_Copies(connectionFolderName));
            if (PL_strcmp(connectionFolderName,inFolderName) == 0)
            {
                rv = connection->IsBusy(&isBusy, &isInbox);
                if (!isBusy)
                    rv = connection->TellThreadToDie(PR_TRUE);
                break; // found it, end of the loop
            }
        }
    }

    PR_CExitMonitor(this);
    return rv;
}

NS_IMETHODIMP nsImapIncomingServer::ResetConnection(const char* folderName)
{
    nsresult rv = NS_OK;
    nsCOMPtr<nsIImapProtocol> connection;
    nsCOMPtr<nsISupports> aSupport;
    PRBool isBusy = PR_FALSE, isInbox = PR_FALSE;
    PRUint32 cnt = 0;
    nsXPIDLCString curFolderName;

    rv = m_connectionCache->Count(&cnt);
    if (NS_FAILED(rv)) return rv;

    PR_CEnterMonitor(this);
    
    for (PRUint32 i=0; i < cnt; i++)
    {
        aSupport = getter_AddRefs(m_connectionCache->ElementAt(i));
        connection = do_QueryInterface(aSupport);
        if (connection)
        {
            rv = connection->GetSelectedMailboxName(getter_Copies(curFolderName));
            if (PL_strcmp(curFolderName,folderName) == 0)
            {
                rv = connection->IsBusy(&isBusy, &isInbox);
                if (!isBusy)
                    rv = connection->ResetToAuthenticatedState();
                break; // found it, end of the loop
            }
        }
    }

    PR_CExitMonitor(this);
    return rv;
}

NS_IMETHODIMP 
nsImapIncomingServer::PerformExpand(nsIMsgWindow *aMsgWindow)
{
    nsXPIDLCString password;
    nsresult rv;

    
    rv = GetPassword(getter_Copies(password));
    if (NS_FAILED(rv)) return rv;
    if (!(const char*) password || 
        nsCRT::strlen((const char*) password) == 0)
        return NS_OK;

    rv = ResetFoldersToUnverified(nsnull);
    
    nsCOMPtr<nsIFolder> rootFolder;
    rv = GetRootFolder(getter_AddRefs(rootFolder));
	if (NS_FAILED(rv)) return rv;
	if (!rootFolder) return NS_ERROR_FAILURE;
	
    nsCOMPtr<nsIMsgFolder> rootMsgFolder = do_QueryInterface(rootFolder,
                                                                 &rv);
	if (NS_FAILED(rv)) return rv;
	if (!rootMsgFolder) return NS_ERROR_FAILURE;

    nsCOMPtr<nsIImapService> imapService = do_GetService(kImapServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;
    if (!imapService) return NS_ERROR_FAILURE;
    nsCOMPtr<nsIEventQueue> queue;
    // get the Event Queue for this thread...
    NS_WITH_SERVICE(nsIEventQueueService, pEventQService, kEventQueueServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;
	if (!pEventQService) return NS_ERROR_FAILURE;
 
    rv = pEventQService->GetThreadEventQueue(NS_CURRENT_THREAD, getter_AddRefs(queue));
    if (NS_FAILED(rv)) return rv;
    rv = imapService->DiscoverAllFolders(queue, rootMsgFolder, this, nsnull);
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
			rv = rootMsgFolder->GetNewMessages(nsnull);
	}

	return rv;
}
    

NS_IMETHODIMP
nsImapIncomingServer::CloseCachedConnections()
{

	nsCOMPtr<nsIImapProtocol> connection;
    PR_CEnterMonitor(this);

	// iterate through the connection cache closing open connections.
	PRUint32 cnt;
    nsCOMPtr<nsISupports> aSupport;

    nsresult rv = m_connectionCache->Count(&cnt);
    if (NS_FAILED(rv)) return rv;

    for (PRUint32 i = cnt; i>0; i--)
	{
        aSupport = getter_AddRefs(m_connectionCache->ElementAt(i-1));
        connection = do_QueryInterface(aSupport);
		if (connection)
        connection->TellThreadToDie(PR_TRUE);
	}
    
    PR_CExitMonitor(this);
	return rv;
}

// nsIImapServerSink impl
NS_IMETHODIMP nsImapIncomingServer::PossibleImapMailbox(const char *folderPath, PRUnichar hierarchyDelimiter, PRInt32 boxFlags)
{
	nsresult rv;
    PRBool found = PR_FALSE;
	PRBool haveParent = PR_FALSE;
    nsCOMPtr<nsIMsgImapMailFolder> hostFolder;
    nsCOMPtr<nsIMsgFolder> aFolder;
    PRBool explicitlyVerify = PR_FALSE;
    
    if (!folderPath || !*folderPath) return NS_ERROR_NULL_POINTER;

	if (mDoingSubscribeDialog) {
		rv = AddToSubscribeDS(folderPath, mDoingLsub /* add as subscribed */, mDoingLsub /* change if exists */);
		return rv;
	}

    nsCAutoString dupFolderPath(folderPath);
    if (dupFolderPath.Last() == '/')
    {
        dupFolderPath.SetLength(dupFolderPath.Length()-1);
        // *** this is what we did in 4.x in order to list uw folder only
        // mailbox in order to get the \NoSelect flag
        explicitlyVerify = !(boxFlags & kNameSpace);
    }

    nsCAutoString folderName = dupFolderPath;
        
    nsCAutoString uri;
	nsXPIDLCString serverUri;

	GetServerURI(getter_Copies(serverUri));

    uri.Assign(serverUri);

    PRInt32 leafPos = folderName.RFindChar('/');

    nsCAutoString parentName(folderName);
	nsCAutoString parentUri(uri);

    if (leafPos > 0)
	{
		// If there is a hierarchy, there is a parent.
		// Don't strip off slash if it's the first character
        parentName.Truncate(leafPos);
		haveParent = PR_TRUE;
		parentUri.Append('/');
		parentUri.Append(parentName);
		folderName.Cut(0, leafPos + 1);	// get rid of the parent name
	}


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

    if (nsCRT::strcasecmp("INBOX", folderPath) == 0 &&
        hierarchyDelimiter == kOnlineHierarchySeparatorNil)
    {
        hierarchyDelimiter = '/'; // set to default in this case (as in 4.x)
        hostFolder->SetHierarchyDelimiter(hierarchyDelimiter);
    }

	nsCOMPtr <nsIMsgFolder> child;

//	nsCString possibleName(aSpec->allocatedPathName);

	uri.Append('/');
	uri.Append(dupFolderPath);


	a_nsIFolder->GetChildWithURI(uri, PR_TRUE, getter_AddRefs(child));

	if (child)
		found = PR_TRUE;
    if (!found)
    {
		// trying to find/discover the parent
		if (haveParent)
		{											
			nsCOMPtr <nsIMsgFolder> parent;
			a_nsIFolder->GetChildWithURI(parentUri, PR_TRUE, getter_AddRefs(parent));
			if (!parent /* || parentFolder->GetFolderNeedsAdded()*/)
			{
				PossibleImapMailbox(parentName, hierarchyDelimiter,kNoselect |		// be defensive
											((boxFlags &	// only inherit certain flags from the child
											(kPublicMailbox | kOtherUsersMailbox | kPersonalMailbox)))); 
			}
		}

        hostFolder->CreateClientSubfolderInfo(dupFolderPath, hierarchyDelimiter);
		a_nsIFolder->GetChildWithURI(uri, PR_TRUE, getter_AddRefs(child));
    }
	if (child)
	{
		nsCOMPtr <nsIMsgImapMailFolder> imapFolder = do_QueryInterface(child);
		if (imapFolder)
		{
      PRBool isAOLServer = PR_FALSE;

      GetIsAOLServer(&isAOLServer);

			nsXPIDLCString onlineName;
			nsXPIDLString unicodeName;
			imapFolder->SetVerifiedAsOnlineFolder(PR_TRUE);
			imapFolder->SetHierarchyDelimiter(hierarchyDelimiter);
			imapFolder->SetBoxFlags(boxFlags);
            imapFolder->SetExplicitlyVerify(explicitlyVerify);
			imapFolder->GetOnlineName(getter_Copies(onlineName));
			if (! ((const char*) onlineName) || nsCRT::strlen((const char *) onlineName) == 0
				|| nsCRT::strcmp((const char *) onlineName, dupFolderPath))
				imapFolder->SetOnlineName(dupFolderPath);
			if (NS_SUCCEEDED(CreatePRUnicharStringFromUTF7(folderName, getter_Copies(unicodeName))))
				child->SetName(unicodeName);
      if (isAOLServer && onlineName && !nsCRT::strcasecmp(onlineName, "RECYCLE"))
      {
        nsAutoString trashName; trashName.AssignWithConversion("Trash");
        child->SetPrettyName(trashName.GetUnicode()); // don't localize - it's a semi-reserved name for IMAP
      }
		}
    }
	return NS_OK;
}

nsresult nsImapIncomingServer::GetFolder(const char* name, nsIMsgFolder** pFolder)
{
    nsresult rv = NS_ERROR_NULL_POINTER;
    if (!name || !*name || !pFolder) return rv;
    *pFolder = nsnull;
    nsCOMPtr<nsIFolder> rootFolder;
    rv = GetRootFolder(getter_AddRefs(rootFolder));
    if (NS_SUCCEEDED(rv) && rootFolder)
    {
        char* uri = nsnull;
        rv = rootFolder->GetURI(&uri);
        if (NS_SUCCEEDED(rv) && uri)
        {
            nsAutoString uriAutoString; uriAutoString.AssignWithConversion(uri);
            uriAutoString.AppendWithConversion('/');
            uriAutoString.AppendWithConversion(name);
            NS_WITH_SERVICE(nsIRDFService, rdf, kRDFServiceCID, &rv);
            if (NS_FAILED(rv)) return rv;
            char* uriString = uriAutoString.ToNewCString();
            nsCOMPtr<nsIRDFResource> res;
            rv = rdf->GetResource(uriString, getter_AddRefs(res));
            if (NS_SUCCEEDED(rv))
            {
                nsCOMPtr<nsIMsgFolder> folder(do_QueryInterface(res, &rv));
                if (NS_SUCCEEDED(rv) && folder)
                {
                    *pFolder = folder;
                    NS_ADDREF(*pFolder);
                }
            }
            delete [] uriString;
        }
        PR_FREEIF(uri);
    }
    return rv;
}


NS_IMETHODIMP  nsImapIncomingServer::OnlineFolderDelete(const char *aFolderName) 
{
	return NS_OK;
}

NS_IMETHODIMP  nsImapIncomingServer::OnlineFolderCreateFailed(const char *aFolderName) 
{
	return NS_OK;
}

NS_IMETHODIMP nsImapIncomingServer::OnlineFolderRename(const char *oldName, const char *newName)
{
    nsresult rv = NS_ERROR_FAILURE;
    if (newName && *newName)
    {
        nsCOMPtr<nsIFolder> iFolder;
        nsCOMPtr<nsIMsgImapMailFolder> parent;
        nsCOMPtr<nsIMsgFolder> me;
        rv = GetFolder(oldName, getter_AddRefs(me));
        if (NS_FAILED(rv)) return rv;

        nsCOMPtr<nsIMsgImapMailFolder> folder;
        folder = do_QueryInterface(me, &rv);
        if (NS_SUCCEEDED(rv))
            folder->RenameLocal(newName);

        nsCOMPtr<nsIFolder> rootFolder;
        rv = GetRootFolder(getter_AddRefs(rootFolder));
        if (NS_SUCCEEDED(rv))
        {
			PRUnichar hierarchyDelimiter = '/';
            if (parent)
                parent->GetHierarchyDelimiter(&hierarchyDelimiter);

            nsCOMPtr<nsIMsgImapMailFolder> imapRootFolder =
                do_QueryInterface(rootFolder, &rv);
            if (NS_SUCCEEDED(rv))
            rv = imapRootFolder->CreateClientSubfolderInfo(newName, hierarchyDelimiter);
        }
    }
	return rv;
}

NS_IMETHODIMP  nsImapIncomingServer::FolderIsNoSelect(const char *aFolderName, PRBool *result) 
{
	if (!result)
		return NS_ERROR_NULL_POINTER;

	*result = PR_FALSE;
	return NS_OK;
}

NS_IMETHODIMP nsImapIncomingServer::SetFolderAdminURL(const char *aFolderName, const char *aFolderAdminUrl)
{
    nsresult rv = NS_ERROR_FAILURE;
	return rv;
}

NS_IMETHODIMP  nsImapIncomingServer::SubscribeUpgradeFinished(PRBool bringUpSubscribeUI) 
{
	return NS_OK;
}

NS_IMETHODIMP nsImapIncomingServer::DiscoveryDone()
{
	nsresult rv = NS_ERROR_FAILURE;
	// first, we need some indication of whether this is the subscribe UI running or not.
	// The subscribe UI doesn't want to delete non-verified folders, or go through the
	// extra list process. I'll leave that as an exercise for Seth for now. I think 
	// we'll probably need to add some state to the imap url to indicate whether the
	// subscribe UI started the url, and need to pass that through to discovery done.

//	m_haveDiscoveredAllFolders = PR_TRUE;

  nsCOMPtr<nsIFolder> rootFolder;
  rv = GetRootFolder(getter_AddRefs(rootFolder));
  if (NS_SUCCEEDED(rv) && rootFolder)
  {
	  if (NS_FAILED(rv)) return rv;
      nsCOMPtr<nsIMsgFolder> rootMsgFolder = do_QueryInterface(rootFolder, &rv);
      if (rootMsgFolder)
        rootMsgFolder->SetPrefFlag();
  }

	PRInt32 numUnverifiedFolders;
	nsCOMPtr<nsISupportsArray> unverifiedFolders;

	rv = NS_NewISupportsArray(getter_AddRefs(unverifiedFolders));
	if(NS_FAILED(rv))
		return rv;

	rv = GetUnverifiedFolders(unverifiedFolders, &numUnverifiedFolders);
	if (numUnverifiedFolders > 0)
	{
		for (PRInt32 k = 0; k < numUnverifiedFolders; k++)
		{
			PRBool explicitlyVerify = PR_FALSE;
			PRBool hasSubFolders = PR_FALSE;
			nsCOMPtr<nsISupports> element;
			unverifiedFolders->GetElementAt(k, getter_AddRefs(element));

			nsCOMPtr<nsIMsgImapMailFolder> currentImapFolder = do_QueryInterface(element, &rv);
			nsCOMPtr<nsIFolder> currentFolder = do_QueryInterface(element, &rv);
			if (NS_FAILED(rv))
				continue;
			if ((NS_SUCCEEDED(currentImapFolder->GetExplicitlyVerify(&explicitlyVerify)) && explicitlyVerify) ||
				((NS_SUCCEEDED(currentFolder->GetHasSubFolders(&hasSubFolders)) && hasSubFolders)
					&& !NoDescendentsAreVerified(currentFolder)))
			{
				// If there are no subfolders and this is unverified, we don't want to run
				// this url.  That is, we want to undiscover the folder.
				// If there are subfolders and no descendants are verified, we want to 
				// undiscover all of the folders.
				// Only if there are subfolders and at least one of them is verified do we want
				// to refresh that folder's flags, because it won't be going away.
				currentImapFolder->SetExplicitlyVerify(PR_FALSE);
				currentImapFolder->List();
			}
			else
			{
				DeleteNonVerifiedFolders(currentFolder);
			}
		}
	}

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

	    PR_ASSERT(currentContext->imapURLPane);

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
					MSG_IMAPFolderInfoMail **folderList = (MSG_IMAPFolderInfoMail **)PR_Malloc(sizeof(MSG_IMAPFolderInfoMail*) * numberOfUnverifiedFolders);
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
								currentFolder->SetExplicitlyVerify(PR_FALSE);
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
			PR_ASSERT(PR_FALSE);
		}
	}

#endif
	return rv;
}

nsresult nsImapIncomingServer::DeleteNonVerifiedFolders(nsIFolder *curFolder)
{
	PRBool autoUnsubscribeFromNoSelectFolders = PR_TRUE;
	nsresult rv;
	NS_WITH_SERVICE(nsIPref, prefs, kPrefServiceCID, &rv);
	if(NS_SUCCEEDED(rv))
	{
		rv = prefs->GetBoolPref("mail.imap.auto_unsubscribe_from_noselect_folders", &autoUnsubscribeFromNoSelectFolders);
	}
//	return rv;
	nsCOMPtr<nsIEnumerator> subFolders;

	rv = curFolder->GetSubFolders(getter_AddRefs(subFolders));
	if(NS_SUCCEEDED(rv))
	{
		nsAdapterEnumerator *simpleEnumerator =	new nsAdapterEnumerator(subFolders);
		if (simpleEnumerator == nsnull)
			return NS_ERROR_OUT_OF_MEMORY;
		PRBool moreFolders;

		while (NS_SUCCEEDED(simpleEnumerator->HasMoreElements(&moreFolders)) && moreFolders)
		{
			nsCOMPtr<nsISupports> child;
			rv = simpleEnumerator->GetNext(getter_AddRefs(child));
			if (NS_SUCCEEDED(rv) && child) 
			{
				PRBool childVerified = PR_FALSE;
				nsCOMPtr <nsIMsgImapMailFolder> childImapFolder = do_QueryInterface(child, &rv);
				if (NS_SUCCEEDED(rv) && childImapFolder)
				{
					PRUint32 flags;

					nsCOMPtr <nsIMsgFolder> childFolder = do_QueryInterface(child, &rv);
					rv = childImapFolder->GetVerifiedAsOnlineFolder(&childVerified);

					rv = childFolder->GetFlags(&flags);
					PRBool folderIsNoSelectFolder = NS_SUCCEEDED(rv) && ((flags & MSG_FOLDER_FLAG_IMAP_NOSELECT) != 0);

		           	PRBool usingSubscription = PR_TRUE;
					GetUsingSubscription(&usingSubscription);
					if (usingSubscription)
					{
						PRBool folderIsNameSpace = PR_FALSE;
						PRBool noDescendentsAreVerified = NoDescendentsAreVerified(childFolder);
						PRBool shouldDieBecauseNoSelect = (folderIsNoSelectFolder ? 
							((noDescendentsAreVerified || AllDescendentsAreNoSelect(childFolder)) && !folderIsNameSpace)
							: PR_FALSE);
						if (!childVerified && (noDescendentsAreVerified || shouldDieBecauseNoSelect))
						{
						}

					}
					else
					{
					}
				}
			}
		}
		delete simpleEnumerator;
	}

#if 0
            
            MSG_IMAPFolderInfoMail *parentImapFolder = (parentFolder->GetType() == FOLDER_IMAPMAIL) ? 
            												(MSG_IMAPFolderInfoMail *) parentFolder :
            												(MSG_IMAPFolderInfoMail *)NULL;
            
            // if the parent is the imap container or an imap folder whose children were listed, then this bool is true.
            // We only delete .snm files whose parent's children were listed											
            XP_Bool parentChildrenWereListed =	(parentImapFolder == NULL) || 
            									(LL_CMP(parentImapFolder->GetTimeStampOfLastList(), >= , IMAP_GetTimeStampOfNonPipelinedList()));

			MSG_IMAPHost *imapHost = currentImapFolder->GetIMAPHost();
           	PRBool usingSubscription = imapHost ? imapHost->GetIsHostUsingSubscription() : PR_TRUE;
			PRBool folderIsNoSelectFolder = (currentImapFolder->GetFolderPrefFlags() & MSG_FOLDER_FLAG_IMAP_NOSELECT) != 0;
			PRBool shouldDieBecauseNoSelect = usingSubscription ?
									(folderIsNoSelectFolder ? ((NoDescendantsAreVerified(currentImapFolder) || AllDescendantsAreNoSelect(currentImapFolder)) && !currentImapFolder->GetFolderIsNamespace()): PR_FALSE)
									: PR_FALSE;
			PRBool offlineCreate = (currentImapFolder->GetFolderPrefFlags() & MSG_FOLDER_FLAG_CREATED_OFFLINE) != 0;

            if (!currentImapFolder->GetExplicitlyVerify() && !offlineCreate &&
				((autoUnsubscribeFromNoSelectFolders && shouldDieBecauseNoSelect) ||
				((usingSubscription ? PR_TRUE : parentChildrenWereListed) && !currentImapFolder->GetIsOnlineVerified() && NoDescendantsAreVerified(currentImapFolder))))
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
								FE_PaneChanged(currentPane, PR_TRUE, MSG_PaneNotifyFolderDeleted, (uint32)currentFolder);
							}
						}
					}

                	FE_PaneChanged(*url_pane, PR_TRUE, MSG_PaneNotifyFolderDeleted, (uint32)currentFolder);

					// If we are running the IMAP subscribe upgrade, and we are deleting the folder that we'd normally
					// try to load after the process completes, then tell the pane not to load that folder.
					if (((MSG_ThreadPane *)(*url_pane))->GetIMAPUpgradeFolder() == currentFolder)
						((MSG_ThreadPane *)(*url_pane))->SetIMAPUpgradeFolder(NULL);

                	if ((*url_pane)->GetFolder() == currentFolder)
						*url_pane = NULL;


					if (shouldDieBecauseNoSelect && autoUnsubscribeFromNoSelectFolders && usingSubscription)
					{
						char *unsubscribeUrl = CreateIMAPUnsubscribeMailboxURL(imapHost->GetHostName(), currentImapFolder->GetOnlineName(), currentImapFolder->GetOnlineHierarchySeparator());
						if (unsubscribeUrl)
						{
							if (url_pane)
								MSG_UrlQueue::AddUrlToPane(unsubscribeUrl, NULL, *url_pane);
							else if (folderPane)
								MSG_UrlQueue::AddUrlToPane(unsubscribeUrl, NULL, folderPane);
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
					PR_ASSERT(PR_FALSE);
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

#endif // 0

    nsCOMPtr<nsIFolder> parent;
	nsCOMPtr<nsIMsgFolder> msgFolder = do_QueryInterface(curFolder);
    rv = curFolder->GetParent(getter_AddRefs(parent));


    if (NS_SUCCEEDED(rv) && parent)
	{
		nsCOMPtr<nsIMsgImapMailFolder> imapParent = do_QueryInterface(parent);
		if (imapParent)
			imapParent->RemoveSubFolder(msgFolder);
	}

	return rv;
}

PRBool nsImapIncomingServer::NoDescendentsAreVerified(nsIFolder *parentFolder)
{
	PRBool nobodyIsVerified = PR_TRUE;
	
	nsCOMPtr<nsIEnumerator> subFolders;

	nsresult rv = parentFolder->GetSubFolders(getter_AddRefs(subFolders));
	if(NS_SUCCEEDED(rv))
	{
		nsAdapterEnumerator *simpleEnumerator =	new nsAdapterEnumerator(subFolders);
		if (simpleEnumerator == nsnull)
			return NS_ERROR_OUT_OF_MEMORY;
		PRBool moreFolders;

		while (NS_SUCCEEDED(simpleEnumerator->HasMoreElements(&moreFolders)) && moreFolders && nobodyIsVerified)
		{
			nsCOMPtr<nsISupports> child;
			rv = simpleEnumerator->GetNext(getter_AddRefs(child));
			if (NS_SUCCEEDED(rv) && child) 
			{
				PRBool childVerified = PR_FALSE;
				nsCOMPtr <nsIMsgImapMailFolder> childImapFolder = do_QueryInterface(child, &rv);
				if (NS_SUCCEEDED(rv) && childImapFolder)
				{
					nsCOMPtr <nsIFolder> childFolder = do_QueryInterface(child, &rv);
					rv = childImapFolder->GetVerifiedAsOnlineFolder(&childVerified);
					nobodyIsVerified = !childVerified && NoDescendentsAreVerified(childFolder);
				}
			}
		}
		delete simpleEnumerator;
	}

	return nobodyIsVerified;
}


PRBool nsImapIncomingServer::AllDescendentsAreNoSelect(nsIFolder *parentFolder)
{
	PRBool allDescendentsAreNoSelect = PR_TRUE;
	nsCOMPtr<nsIEnumerator> subFolders;

	nsresult rv = parentFolder->GetSubFolders(getter_AddRefs(subFolders));
	if(NS_SUCCEEDED(rv))
	{
		nsAdapterEnumerator *simpleEnumerator =	new nsAdapterEnumerator(subFolders);
		if (simpleEnumerator == nsnull)
			return NS_ERROR_OUT_OF_MEMORY;
		PRBool moreFolders;

		while (NS_SUCCEEDED(simpleEnumerator->HasMoreElements(&moreFolders)) && moreFolders && allDescendentsAreNoSelect)
		{
			nsCOMPtr<nsISupports> child;
			rv = simpleEnumerator->GetNext(getter_AddRefs(child));
			if (NS_SUCCEEDED(rv) && child) 
			{
				PRBool childIsNoSelect = PR_FALSE;
				nsCOMPtr <nsIMsgImapMailFolder> childImapFolder = do_QueryInterface(child, &rv);
				if (NS_SUCCEEDED(rv) && childImapFolder)
				{
					PRUint32 flags;

					nsCOMPtr <nsIMsgFolder> childFolder = do_QueryInterface(child, &rv);
					rv = childFolder->GetFlags(&flags);
					childIsNoSelect = NS_SUCCEEDED(rv) && (flags & MSG_FOLDER_FLAG_IMAP_NOSELECT);
					allDescendentsAreNoSelect = !childIsNoSelect && AllDescendentsAreNoSelect(childFolder);
				}
			}
		}
		delete simpleEnumerator;
	}
#if 0
	int numberOfSubfolders = parentFolder->GetNumSubFolders();
	
	for (int childIndex=0; allDescendantsAreNoSelect && (childIndex < numberOfSubfolders); childIndex++)
	{
		MSG_IMAPFolderInfoMail *currentChild = (MSG_IMAPFolderInfoMail *) parentFolder->GetSubFolder(childIndex);
		allDescendentsAreNoSelect = (currentChild->GetFolderPrefFlags() & MSG_FOLDER_PREF_IMAPNOSELECT) &&
									AllDescendentsAreNoSelect(currentChild);
	}
#endif // 0
	return allDescendentsAreNoSelect;
}


#if 0
void nsImapIncomingServer::UnsubscribeFromAllDescendents(nsIFolder *parentFolder)
{
	int numberOfSubfolders = parentFolder->GetNumSubFolders();
	
	for (int childIndex=0; childIndex < numberOfSubfolders; childIndex++)
	{
		MSG_IMAPFolderInfoMail *currentChild = (MSG_IMAPFolderInfoMail *) parentFolder->GetSubFolder(childIndex);
		char *unsubscribeUrl = CreateIMAPUnsubscribeMailboxURL(currentChild->GetHostName(), currentChild->GetOnlineName(), currentChild->GetOnlineHierarchySeparator());	// unsubscribe from child
		if (unsubscribeUrl)
		{
			MSG_UrlQueue::AddUrlToPane(unsubscribeUrl, NULL, pane);
			XP_FREE(unsubscribeUrl);
		}
		UnsubscribeFromAllDescendants(currentChild);	// unsubscribe from its children
	}
}
#endif // 0


NS_IMETHODIMP
nsImapIncomingServer::FEAlert(const PRUnichar* aString, nsIMsgWindow * aMsgWindow)
{
	nsresult rv = NS_OK;
  nsCOMPtr<nsIPrompt> dialog;
  if (aMsgWindow)
    aMsgWindow->GetPromptDialog(getter_AddRefs(dialog));

  if (!dialog) // if we didn't get one, use the default....
    dialog = do_GetService(kNetSupportDialogCID);

  if (dialog)
	  rv = dialog->Alert(nsnull, aString);
  return rv;
}

NS_IMETHODIMP  nsImapIncomingServer::FEAlertFromServer(const char *aString, nsIMsgWindow * aMsgWindow)
{
	nsresult rv = NS_OK;
  
  nsCOMPtr<nsIPrompt> dialog;
  if (aMsgWindow)
    aMsgWindow->GetPromptDialog(getter_AddRefs(dialog));

  if (!dialog) // if we didn't get one, use the default....
    dialog = do_GetService(kNetSupportDialogCID);

	if (aString)
	{
		// skip over the first two words, I guess.
		char *whereRealMessage = PL_strchr(aString, ' ');
		if (whereRealMessage)
			whereRealMessage++;
		if (whereRealMessage)
			whereRealMessage = PL_strchr(whereRealMessage, ' ');
		if (whereRealMessage)
			whereRealMessage++;
    
    // the alert string from the server IS UTF-8!!! We must convert it to unicode
    // correctly before appending it to our error message string...

    nsAutoString unicodeAlertString;
    nsAutoString charset;
    charset.AppendWithConversion("UTF-8");
    ConvertToUnicode(charset, whereRealMessage ? whereRealMessage : aString, unicodeAlertString);
    
		PRUnichar *serverSaidPrefix = nsnull;
		GetImapStringByID(IMAP_SERVER_SAID, &serverSaidPrefix);
		if (serverSaidPrefix)
		{
			nsAutoString message(serverSaidPrefix);
			message.Append(unicodeAlertString);
			rv = dialog->Alert(nsnull, message.GetUnicode());

			PR_Free(serverSaidPrefix);
		}
	}

  return rv;
}

/* This is the next generation string retrieval call */
static NS_DEFINE_CID(kStringBundleServiceCID, NS_STRINGBUNDLESERVICE_CID);

#define IMAP_MSGS_URL       "chrome://messenger/locale/imapMsgs.properties"

nsresult nsImapIncomingServer::GetStringBundle()
{
  nsresult res;
	if (!m_stringBundle)
	{
		char*       propertyURL = NULL;

		propertyURL = IMAP_MSGS_URL;

		NS_WITH_SERVICE(nsIStringBundleService, sBundleService, kStringBundleServiceCID, &res); 
		if (NS_SUCCEEDED(res) && (nsnull != sBundleService)) 
		{
			nsILocale   *locale = nsnull;

			res = sBundleService->CreateBundle(propertyURL, locale, getter_AddRefs(m_stringBundle));
		}
	}
  return (m_stringBundle) ? NS_OK : res;
}

NS_IMETHODIMP  nsImapIncomingServer::GetImapStringByID(PRInt32 aMsgId, PRUnichar **aString)
{
	nsAutoString	resultString; resultString.AssignWithConversion("???");
	nsresult res = NS_OK;

  GetStringBundle();
	if (m_stringBundle)
	{
		PRUnichar *ptrv = nsnull;
		res = m_stringBundle->GetStringFromID(aMsgId, &ptrv);

		if (NS_FAILED(res)) 
		{
			resultString.AssignWithConversion("[StringID");
			resultString.AppendInt(aMsgId, 10);
			resultString.AssignWithConversion("?]");
			*aString = resultString.ToNewUnicode();
		}
		else
		{
			*aString = ptrv;
		}
	}
	else
	{
		res = NS_OK;
		*aString = resultString.ToNewUnicode();
	}
	return res;
}

nsresult nsImapIncomingServer::ResetFoldersToUnverified(nsIFolder *parentFolder)
{
    nsresult rv = NS_OK;
    if (!parentFolder) {
        nsCOMPtr<nsIFolder> rootFolder;
        rv = GetRootFolder(getter_AddRefs(rootFolder));
        if (NS_FAILED(rv)) return rv;
        return ResetFoldersToUnverified(rootFolder);
    }
    else {
        nsCOMPtr<nsIEnumerator> subFolders;
        nsCOMPtr<nsIMsgImapMailFolder> imapFolder =
            do_QueryInterface(parentFolder, &rv);
        if (NS_FAILED(rv)) return rv;
        rv = imapFolder->SetVerifiedAsOnlineFolder(PR_FALSE);
        rv = parentFolder->GetSubFolders(getter_AddRefs(subFolders));
        if (NS_FAILED(rv)) return rv;
        nsAdapterEnumerator *simpleEnumerator = new
            nsAdapterEnumerator(subFolders);
        if (!simpleEnumerator) return NS_ERROR_OUT_OF_MEMORY;
        PRBool moreFolders = PR_FALSE;
        while (NS_SUCCEEDED(simpleEnumerator->HasMoreElements(&moreFolders))
               && moreFolders) {
            nsCOMPtr<nsISupports> child;
            rv = simpleEnumerator->GetNext(getter_AddRefs(child));
            if (NS_SUCCEEDED(rv) && child) {
                nsCOMPtr<nsIFolder> childFolder = do_QueryInterface(child,
                                                                    &rv);
                if (NS_SUCCEEDED(rv) && childFolder) {
                    rv = ResetFoldersToUnverified(childFolder);
                    if (NS_FAILED(rv)) break;
                }
            }
        }
        delete simpleEnumerator;
    }
    return rv;
}

nsresult nsImapIncomingServer::GetUnverifiedFolders(nsISupportsArray *aFoldersArray, PRInt32 *aNumUnverifiedFolders)
{
	// can't have both be null, but one null is OK, since the caller
	// may just be trying to count the number of unverified folders.
	if (!aFoldersArray && !aNumUnverifiedFolders)
		return NS_ERROR_NULL_POINTER;

	if (aNumUnverifiedFolders)
		*aNumUnverifiedFolders = 0;
	nsCOMPtr<nsIFolder> rootFolder;
	nsresult rv = GetRootFolder(getter_AddRefs(rootFolder));
	if(NS_SUCCEEDED(rv) && rootFolder)
		rv = GetUnverifiedSubFolders(rootFolder, aFoldersArray, aNumUnverifiedFolders);
	return rv;
}

nsresult nsImapIncomingServer::GetUnverifiedSubFolders(nsIFolder *parentFolder, nsISupportsArray *aFoldersArray, PRInt32 *aNumUnverifiedFolders)
{
	nsresult rv = NS_OK;

	nsCOMPtr <nsIMsgImapMailFolder> imapFolder = do_QueryInterface(parentFolder);
	PRBool verified = PR_FALSE, explicitlyVerify = PR_FALSE;
	if (imapFolder)
	{
		rv = imapFolder->GetVerifiedAsOnlineFolder(&verified);
        if (NS_SUCCEEDED(rv))
            rv = imapFolder->GetExplicitlyVerify(&explicitlyVerify);

		if (NS_SUCCEEDED(rv) && (!verified || explicitlyVerify))
		{
			if (aFoldersArray)
			{
				nsCOMPtr <nsISupports> supports = do_QueryInterface(imapFolder);
				aFoldersArray->AppendElement(supports);
			}
			if (aNumUnverifiedFolders)
				(*aNumUnverifiedFolders)++;
		}
	}
	nsCOMPtr<nsIEnumerator> subFolders;

	rv = parentFolder->GetSubFolders(getter_AddRefs(subFolders));
	if(NS_SUCCEEDED(rv))
	{
		nsAdapterEnumerator *simpleEnumerator =	new nsAdapterEnumerator(subFolders);
		if (simpleEnumerator == nsnull)
			return NS_ERROR_OUT_OF_MEMORY;
		PRBool moreFolders;

		while (NS_SUCCEEDED(simpleEnumerator->HasMoreElements(&moreFolders)) && moreFolders)
		{
			nsCOMPtr<nsISupports> child;
			rv = simpleEnumerator->GetNext(getter_AddRefs(child));
			if (NS_SUCCEEDED(rv) && child) 
			{
				nsCOMPtr <nsIFolder> childFolder = do_QueryInterface(child, &rv);
				if (NS_SUCCEEDED(rv) && childFolder)
				{
					rv = GetUnverifiedSubFolders(childFolder, aFoldersArray, aNumUnverifiedFolders);
					if (!NS_SUCCEEDED(rv))
						break;
				}
			}
		}
		delete simpleEnumerator;
	}
	return rv;
}

NS_IMETHODIMP nsImapIncomingServer::PromptForPassword(char ** aPassword,
                                                      nsIMsgWindow * aMsgWindow)
{
    PRUnichar *passwordTemplate = IMAPGetStringByID(IMAP_ENTER_PASSWORD_PROMPT);
    PRUnichar *passwordTitle = IMAPGetStringByID(IMAP_ENTER_PASSWORD_PROMPT_TITLE);
    PRUnichar *passwordText = nsnull;
    nsXPIDLCString hostName;
    nsXPIDLCString userName;
    PRBool okayValue;

    GetHostName(getter_Copies(hostName));
    GetUsername(getter_Copies(userName));

    passwordText = nsTextFormatter::smprintf(passwordTemplate, (const char *) userName, (const char *) hostName);
    nsresult rv =  GetPasswordWithUI(passwordText, passwordTitle, aMsgWindow,
                                     &okayValue, aPassword);
    nsTextFormatter::smprintf_free(passwordText);
    nsCRT::free(passwordTemplate);
    nsCRT::free(passwordTitle);
    return rv;
}

// for the nsIImapServerSink interface
NS_IMETHODIMP  nsImapIncomingServer::SetCapability(PRUint32 capability)
{
	m_capability = capability;
	SetCapabilityPref(capability);
	return NS_OK;
}

NS_IMETHODIMP  nsImapIncomingServer::CommitNamespaces()
{

	nsresult rv;
	NS_WITH_SERVICE(nsIImapHostSessionList, hostSession, kCImapHostSessionList, &rv);
    if (NS_FAILED(rv)) 
		return rv;

	return hostSession->CommitNamespacesForHost(this);

}

NS_IMETHODIMP nsImapIncomingServer::PseudoInterruptMsgLoad(nsIImapUrl *aImapUrl, PRBool *interrupted)
{
	nsresult rv = NS_OK;
	nsCOMPtr<nsIImapProtocol> connection;

  PR_CEnterMonitor(this);

	// iterate through the connection cache for a connection that is loading
	// a message in this folder and should be pseudo-interrupted.
	PRUint32 cnt;
  nsCOMPtr<nsISupports> aSupport;

  rv = m_connectionCache->Count(&cnt);
  if (NS_FAILED(rv)) return rv;
  for (PRUint32 i = 0; i < cnt; i++) 
  {	
    aSupport = getter_AddRefs(m_connectionCache->ElementAt(i));
    connection = do_QueryInterface(aSupport);
		if (connection)
			rv = connection->PseudoInterruptMsgLoad(aImapUrl, interrupted);
	}
    
  PR_CExitMonitor(this);
	return rv;
}

NS_IMETHODIMP nsImapIncomingServer::ResetNamespaceReferences()
{
	return NS_ERROR_NOT_IMPLEMENTED;
}

#if FINISHED_PORTED_NAMESPACE_STUFF
		int numberOfChildren = GetNumSubFolders();
	for (int childIndex = 0; childIndex < numberOfChildren; childIndex++)
	{
		MSG_IMAPFolderInfoMail *currentChild = (MSG_IMAPFolderInfoMail *) GetSubFolder(childIndex);
		currentChild->ResetNamespaceReferences();
	}

	void MSG_IMAPFolderInfoMail::SetFolderIsNamespace(XP_Bool isNamespace)
{
	m_folderIsNamespace = isNamespace;
}

void MSG_IMAPFolderInfoMail::InitializeFolderCreatedOffline()
{
	TIMAPNamespace *ns = IMAPNS_GetNamespaceForFolder(m_host->GetHostName(), GetOnlineName(), '/');
	SetOnlineHierarchySeparator(IMAPNS_GetDelimiterForNamespace(ns));
}

TIMAPNamespace *MSG_IMAPFolderInfoMail::GetNamespaceForFolder()
{
	if (!m_namespace)
	{
#ifdef DEBUG_bienvenu
		// Make sure this isn't causing us to open the database
		PR_ASSERT(m_OnlineHierSeparator != kOnlineHierarchySeparatorUnknown);
#endif
		m_namespace = IMAPNS_GetNamespaceForFolder(m_host->GetHostName(), GetOnlineName(), GetOnlineHierarchySeparator());
		PR_ASSERT(m_namespace);
		if (m_namespace)
		{
			IMAPNS_SuggestHierarchySeparatorForNamespace(m_namespace, GetOnlineHierarchySeparator());
			m_folderIsNamespace = IMAPNS_GetFolderIsNamespace(m_host->GetHostName(), GetOnlineName(), GetOnlineHierarchySeparator(), m_namespace);
		}
	}
	return m_namespace;
}

void MSG_IMAPFolderInfoMail::SetNamespaceForFolder(TIMAPNamespace *ns)
{
#ifdef DEBUG_bienvenu
	NS_ASSERTION(ns, "null namespace");
#endif
	m_namespace = ns;
}

void MSG_IMAPFolderInfoMail::ResetNamespaceReferences()
{
	// this
	m_namespace = IMAPNS_GetNamespaceForFolder(GetHostName(), GetOnlineName(), GetOnlineHierarchySeparator());
	NS_ASSERTION(m_namespace, "resetting null namespace");
	if (m_namespace)
		m_folderIsNamespace = IMAPNS_GetFolderIsNamespace(GetHostName(), GetOnlineName(), GetOnlineHierarchySeparator(), m_namespace);
	else
		m_folderIsNamespace = PR_FALSE;

	// children
	int numberOfChildren = GetNumSubFolders();
	for (int childIndex = 0; childIndex < numberOfChildren; childIndex++)
	{
		MSG_IMAPFolderInfoMail *currentChild = (MSG_IMAPFolderInfoMail *) GetSubFolder(childIndex);
		currentChild->ResetNamespaceReferences();
	}
}

#endif  //FINISHED_PORTED_NAMESPACE_STUFF

NS_IMETHODIMP  nsImapIncomingServer::SetUserAuthenticated(PRBool authenticated)
{
	return NS_OK;
}

/* void SetMailServerUrls (in string manageMailAccount, in string manageLists, in string manageFilters); */
NS_IMETHODIMP  nsImapIncomingServer::SetMailServerUrls(const char *manageMailAccount, const char *manageLists, const char *manageFilters)
{
	return SetManageMailAccountUrl((char *) manageMailAccount);
}

NS_IMETHODIMP nsImapIncomingServer::SetManageMailAccountUrl(const char *manageMailAccountUrl)
{
	m_manageMailAccountUrl = manageMailAccountUrl;
	return NS_OK;
}

NS_IMETHODIMP nsImapIncomingServer::GetManageMailAccountUrl(char **manageMailAccountUrl)
{
	if (!manageMailAccountUrl)
		return NS_ERROR_NULL_POINTER;

	*manageMailAccountUrl = m_manageMailAccountUrl.ToNewCString();
	return NS_OK;
}

NS_IMETHODIMP nsImapIncomingServer::RemoveChannelFromUrl(nsIMsgMailNewsUrl *aUrl, PRUint32 statusCode)
{
  nsresult rv = NS_OK;
  if (aUrl)
  {
    nsCOMPtr<nsIImapUrl> imapUrl = do_QueryInterface(aUrl);
    if (imapUrl)
      rv = imapUrl->RemoveChannel(statusCode);
  }

  return rv;
}

NS_IMETHODIMP nsImapIncomingServer::CreatePRUnicharStringFromUTF7(const char * aSourceString, PRUnichar **aUnicodeStr)
{
	return CreateUnicodeStringFromUtf7(aSourceString, aUnicodeStr);
}

nsresult nsImapIncomingServer::RequestOverrideInfo(nsIMsgWindow *aMsgWindow)
{

	nsresult rv;
	nsCAutoString progID(NS_MSGLOGONREDIRECTORSERVICE_PROGID);
	nsXPIDLCString redirectorType;

	GetRedirectorType(getter_Copies(redirectorType));
	progID.Append('/');
	progID.Append(redirectorType);

	m_logonRedirector = do_GetService(progID.GetBuffer(), &rv);
	if (m_logonRedirector && NS_SUCCEEDED(rv))
	{
		nsCOMPtr <nsIMsgLogonRedirectionRequester> logonRedirectorRequester;
		rv = QueryInterface(NS_GET_IID(nsIMsgLogonRedirectionRequester), getter_AddRefs(logonRedirectorRequester));
		if (NS_SUCCEEDED(rv))
		{
			nsXPIDLCString password;
			nsXPIDLCString userName;
      PRBool requiresPassword = PR_TRUE;

			GetUsername(getter_Copies(userName));
      m_logonRedirector->RequiresPassword(userName, &requiresPassword);
      if (requiresPassword)
      {
  			GetPassword(getter_Copies(password));

			  if (!((const char *) password) || nsCRT::strlen((const char *) password) == 0)
				  PromptForPassword(getter_Copies(password), aMsgWindow);

        // if we still don't have a password then the user must have hit cancel so just
        // fall out...
        if (!((const char *) password) || nsCRT::strlen((const char *) password) == 0)
          return NS_OK;
      }

  		rv = m_logonRedirector->Logon(userName, password, logonRedirectorRequester, nsMsgLogonRedirectionServiceIDs::Imap);
		}
	}

	return rv;
}

NS_IMETHODIMP nsImapIncomingServer::OnLogonRedirectionError(const PRUnichar *pErrMsg, PRBool badPassword)
{
	nsresult rv = NS_OK;

	nsXPIDLString progressString;
	GetImapStringByID(IMAP_LOGIN_FAILED, getter_Copies(progressString));

	FEAlert(progressString, nsnull);

	if (m_logonRedirector)
	{
		nsXPIDLCString userName;

		GetUsername(getter_Copies(userName));
		m_logonRedirector->Logoff(userName);
	}

	if (badPassword)
		SetPassword(nsnull);

	PRUint32 urlQueueCnt = 0;

	// pull the url out of the queue so we can get the msg window, and try to rerun it.
	m_urlQueue->Count(&urlQueueCnt);
	if (badPassword && ++m_redirectedLogonRetries <= 3)
	{
		// this will force a reprompt for the password.
		// ### DMB TODO display error message?
		if (urlQueueCnt > 0)
		{
			nsCOMPtr<nsISupports>
		  aSupport(getter_AddRefs(m_urlQueue->ElementAt(0)));
			nsCOMPtr<nsIImapUrl> aImapUrl(do_QueryInterface(aSupport, &rv));

		  nsCOMPtr <nsIImapProtocol> imapProtocol;
		  nsCOMPtr <nsIEventQueue> aEventQueue;
		  // Get current thread envent queue
		  NS_WITH_SERVICE(nsIEventQueueService, pEventQService, kEventQueueServiceCID, &rv); 
		  if (NS_SUCCEEDED(rv) && pEventQService)
			  pEventQService->GetThreadEventQueue(NS_CURRENT_THREAD,
												getter_AddRefs(aEventQueue));

			if (aImapUrl)
			{       
				nsCOMPtr <nsIImapProtocol>  protocolInstance ;
				m_waitingForConnectionInfo = PR_FALSE;
				rv = CreateImapConnection(aEventQueue, aImapUrl,
												   getter_AddRefs(protocolInstance));
			}
		}
	}
	else
	{
		m_redirectedLogonRetries = 0; // reset so next attempt will start at 0.
		if (urlQueueCnt > 0)
		{
			m_urlQueue->RemoveElementAt(0);
			m_urlConsumers.RemoveElementAt(0);
		}
	}
  
  return rv;
}
  
  /* Logon Redirection Progress */
NS_IMETHODIMP nsImapIncomingServer::OnLogonRedirectionProgress(nsMsgLogonRedirectionState pState)
{
	return NS_OK;
}

  /* reply with logon redirection data. */
NS_IMETHODIMP nsImapIncomingServer::OnLogonRedirectionReply(const PRUnichar *pHost, unsigned short pPort, const char *pCookieData,  unsigned short pCookieSize)
{
	PRBool urlRun = PR_FALSE;
	nsresult rv;
	nsCOMPtr <nsIImapProtocol> imapProtocol;
	nsCOMPtr <nsIEventQueue> aEventQueue;
	nsCAutoString cookie(pCookieData, pCookieSize);
    // Get current thread envent queue
	NS_WITH_SERVICE(nsIEventQueueService, pEventQService, kEventQueueServiceCID, &rv); 
  if (NS_SUCCEEDED(rv) && pEventQService)
        pEventQService->GetThreadEventQueue(NS_CURRENT_THREAD,
                                            getter_AddRefs(aEventQueue));
  // we used to logoff the external requestor...we no longer need to do
  // that.

	m_redirectedLogonRetries = 0; // we got through, so reset this counter.

  PRUint32 cnt = 0;

  m_urlQueue->Count(&cnt);
  if (cnt > 0)
  {
    nsCOMPtr<nsISupports> aSupport(getter_AddRefs(m_urlQueue->ElementAt(0)));
    nsCOMPtr<nsIImapUrl> aImapUrl(do_QueryInterface(aSupport, &rv));

    if (aImapUrl)
    {
      nsISupports *aConsumer = (nsISupports*)m_urlConsumers.ElementAt(0);
      NS_IF_ADDREF(aConsumer);
      
			nsCOMPtr <nsIImapProtocol>  protocolInstance ;
			rv = CreateImapConnection(aEventQueue, aImapUrl, getter_AddRefs(protocolInstance));
			m_waitingForConnectionInfo = PR_FALSE;
			if (NS_SUCCEEDED(rv) && protocolInstance)
      {
				protocolInstance->OverrideConnectionInfo(pHost, pPort, cookie.GetBuffer());
				nsCOMPtr<nsIURI> url = do_QueryInterface(aImapUrl, &rv);
				if (NS_SUCCEEDED(rv) && url)
				{
					rv = protocolInstance->LoadUrl(url, aConsumer);
					urlRun = PR_TRUE;
				}
        
        m_urlQueue->RemoveElementAt(0);
        m_urlConsumers.RemoveElementAt(0);
      }
        
      NS_IF_RELEASE(aConsumer);
    }    
  }

  return rv;
}

NS_IMETHODIMP
nsImapIncomingServer::PopulateSubscribeDatasourceWithUri(nsIMsgWindow *aMsgWindow, PRBool aForceToServer /*ignored*/, const char *uri)
{
	nsresult rv;
#ifdef DEBUG_sspitzer
	printf("in PopulateSubscribeDatasourceWithUri(%s)\n",uri);
#endif
	mDoingSubscribeDialog = PR_TRUE;	

	// xxx todo can we do: rv = mInner->StartPopulatingSubscribeDS(); ?
    rv = StartPopulatingSubscribeDS();
    if (NS_FAILED(rv)) return rv;

	nsCOMPtr<nsIImapService> imapService = do_GetService(kImapServiceCID, &rv);
	if (NS_FAILED(rv)) return rv;
	if (!imapService) return NS_ERROR_FAILURE;

	nsXPIDLCString serverUri;
    rv = GetServerURI(getter_Copies(serverUri));
	if (NS_FAILED(rv)) return rv;

	/* 
		if uri = imap://user@host/foo/bar, the serverUri is imap://user@host
	 	to get path from uri, skip over imap://user@host + 1 (for the /)
	*/
	const char *path = uri + nsCRT::strlen((const char *)serverUri) + 1;
#ifdef DEBUG_seth
	printf("path = %s\n",path);
#endif

    rv = imapService->BuildSubscribeDatasourceWithPath(this, aMsgWindow, path);
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}

NS_IMETHODIMP
nsImapIncomingServer::PopulateSubscribeDatasource(nsIMsgWindow *aMsgWindow, PRBool aForceToServer /*ignored*/)
{
	nsresult rv;
#ifdef DEBUG_sspitzer
	printf("in PopulateSubscribeDatasource()\n");
#endif
	mDoingSubscribeDialog = PR_TRUE;	

    rv = StartPopulatingSubscribeDS();
    if (NS_FAILED(rv)) return rv;

	nsCOMPtr<nsIImapService> imapService = do_GetService(kImapServiceCID, &rv);
	if (NS_FAILED(rv)) return rv;
	if (!imapService) return NS_ERROR_FAILURE;

    rv = imapService->BuildSubscribeDatasource(this, aMsgWindow);
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}

NS_IMETHODIMP
nsImapIncomingServer::OnStartRunningUrl(nsIURI *url)
{
    return NS_OK;
}

NS_IMETHODIMP
nsImapIncomingServer::OnStopRunningUrl(nsIURI *url, nsresult exitCode)
{
    nsresult rv = exitCode;

    nsCOMPtr<nsIImapUrl> imapUrl = do_QueryInterface(url);
    if (imapUrl) {
        nsImapAction imapAction = nsIImapUrl::nsImapTest;
        imapUrl->GetImapAction(&imapAction);
        switch (imapAction) {
        case nsIImapUrl::nsImapDiscoverAllAndSubscribedBoxesUrl:
        case nsIImapUrl::nsImapDiscoverChildrenUrl:
            rv = UpdateSubscribedInSubscribeDS();
            if (NS_FAILED(rv)) return rv;
            mDoingSubscribeDialog = PR_FALSE;
            rv = StopPopulatingSubscribeDS();
            break;
        case nsIImapUrl::nsImapDiscoverAllBoxesUrl:
            DiscoveryDone();
            break;
        default:
            break;
        }
    }

    return NS_OK;
}

NS_IMETHODIMP
nsImapIncomingServer::SetIncomingServer(nsIMsgIncomingServer *aServer)
{
	NS_ASSERTION(mInner,"not initialized");
	if (!mInner) return NS_ERROR_FAILURE;
	return mInner->SetIncomingServer(aServer);
}

NS_IMETHODIMP
nsImapIncomingServer::SetShowFullName(PRBool showFullName)
{
	NS_ASSERTION(mInner,"not initialized");
	if (!mInner) return NS_ERROR_FAILURE;
	return mInner->SetShowFullName(showFullName);
}

NS_IMETHODIMP
nsImapIncomingServer::SetDelimiter(char aDelimiter)
{
	NS_ASSERTION(mInner,"not initialized");
	if (!mInner) return NS_ERROR_FAILURE;
	return mInner->SetDelimiter(aDelimiter);
}

NS_IMETHODIMP
nsImapIncomingServer::SetAsSubscribedInSubscribeDS(const char *aURI)
{
	NS_ASSERTION(mInner,"not initialized");
	if (!mInner) return NS_ERROR_FAILURE;
	return mInner->SetAsSubscribedInSubscribeDS(aURI);
}

NS_IMETHODIMP
nsImapIncomingServer::UpdateSubscribedInSubscribeDS()
{
#ifdef DEBUG_sspitzer
	printf("for imap, do this when we populate\n");
#endif
	return NS_OK;
}

NS_IMETHODIMP
nsImapIncomingServer::AddToSubscribeDS(const char *aName, PRBool addAsSubscribed,PRBool changeIfExists)
{
	NS_ASSERTION(mInner,"not initialized");
	if (!mInner) return NS_ERROR_FAILURE;
	return mInner->AddToSubscribeDS(aName, addAsSubscribed, changeIfExists);
}


NS_IMETHODIMP
nsImapIncomingServer::SetPropertiesInSubscribeDS(const char *uri, const PRUnichar *aName, nsIRDFResource *aResource, PRBool subscribed, PRBool changeIfExists)
{
	NS_ASSERTION(mInner,"not initialized");
	if (!mInner) return NS_ERROR_FAILURE;
	return mInner->SetPropertiesInSubscribeDS(uri,aName,aResource,subscribed,changeIfExists);
}

NS_IMETHODIMP
nsImapIncomingServer::FindAndAddParentToSubscribeDS(const char *uri, const char *serverUri, const char *aName, nsIRDFResource *aChildResource)
{
	NS_ASSERTION(mInner,"not initialized");
	if (!mInner) return NS_ERROR_FAILURE;
	return mInner->FindAndAddParentToSubscribeDS(uri,serverUri,aName,aChildResource);
}

NS_IMETHODIMP
nsImapIncomingServer::StopPopulatingSubscribeDS()
{
	nsresult rv;

    nsCOMPtr<nsISubscribeListener> listener;
    rv = GetSubscribeListener(getter_AddRefs(listener));
    if (NS_FAILED(rv)) return rv;
    if (!listener) return NS_ERROR_FAILURE;

    rv = listener->OnStopPopulating();
    if (NS_FAILED(rv)) return rv;
	
	NS_ASSERTION(mInner,"not initialized");
	if (!mInner) return NS_ERROR_FAILURE;
	rv = mInner->StopPopulatingSubscribeDS();
	if (NS_FAILED(rv)) return rv;

	return NS_OK;
}


NS_IMETHODIMP
nsImapIncomingServer::SubscribeCleanup()
{
	nsresult rv;
	if (mInner) {
		rv = mInner->SetSubscribeListener(nsnull);
		if (NS_FAILED(rv)) return rv;

		mInner = nsnull;
	}
	return NS_OK;
}

NS_IMETHODIMP
nsImapIncomingServer::StartPopulatingSubscribeDS()
{
	nsresult rv;

	NS_ASSERTION(mInner,"not initialized");
	if (!mInner) return NS_ERROR_FAILURE;

    rv = SetIncomingServer(this);
    if (NS_FAILED(rv)) return rv;

    rv = SetDelimiter('/');		// is this aways the case?  need to talk to jefft
    if (NS_FAILED(rv)) return rv;

	rv = SetShowFullName(PR_FALSE);
    if (NS_FAILED(rv)) return rv;

	return mInner->StartPopulatingSubscribeDS();
}

NS_IMETHODIMP
nsImapIncomingServer::SetSubscribeListener(nsISubscribeListener *aListener)
{
	nsresult rv;
    mInner = do_CreateInstance(kSubscribableServerCID,&rv);
    if (NS_FAILED(rv)) return rv;
    if (!mInner) return NS_ERROR_FAILURE;
	return mInner->SetSubscribeListener(aListener);
}

NS_IMETHODIMP
nsImapIncomingServer::GetSubscribeListener(nsISubscribeListener **aListener)
{
	NS_ASSERTION(mInner,"not initialized");
	if (!mInner) return NS_ERROR_FAILURE;
	return mInner->GetSubscribeListener(aListener);
}

NS_IMETHODIMP
nsImapIncomingServer::Subscribe(const PRUnichar *aName)
{
	return SubscribeToFolder(aName, PR_TRUE);
}

NS_IMETHODIMP
nsImapIncomingServer::Unsubscribe(const PRUnichar *aName)
{
	return SubscribeToFolder(aName, PR_FALSE);
}

nsresult
nsImapIncomingServer::SubscribeToFolder(const PRUnichar *aName, PRBool subscribe)
{
	nsresult rv;
	nsCOMPtr<nsIImapService> imapService = do_GetService(kImapServiceCID, &rv);
	if (NS_FAILED(rv)) return rv;
	if (!imapService) return NS_ERROR_FAILURE;

    nsCOMPtr<nsIFolder> rootFolder;
    rv = GetRootFolder(getter_AddRefs(rootFolder));
	if (NS_FAILED(rv)) return rv;
    nsCOMPtr<nsIMsgFolder> rootMsgFolder = do_QueryInterface(rootFolder, &rv);
	if (NS_FAILED(rv)) return rv;
    if (!rootMsgFolder) return NS_ERROR_FAILURE;

	nsCOMPtr<nsIEventQueue> queue;
    // get the Event Queue for this thread...
    NS_WITH_SERVICE(nsIEventQueueService, pEventQService, kEventQueueServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = pEventQService->GetThreadEventQueue(NS_CURRENT_THREAD, getter_AddRefs(queue));
    if (NS_FAILED(rv)) return rv;

	if (subscribe) {
			rv = imapService->SubscribeFolder(queue,
                               rootMsgFolder,
                               aName,
                               nsnull, nsnull);
	}
	else {
			rv = imapService->UnsubscribeFolder(queue,
                               rootMsgFolder,
                               aName,
                               nsnull, nsnull);
	}

	if (NS_FAILED(rv)) return rv;
	return NS_OK;
}

NS_IMETHODIMP
nsImapIncomingServer::SetDoingLsub(PRBool doingLsub)
{
	mDoingLsub = doingLsub;
	return NS_OK;
}

NS_IMETHODIMP
nsImapIncomingServer::GetDoingLsub(PRBool *doingLsub)
{
	if (!doingLsub) return NS_ERROR_NULL_POINTER;

	*doingLsub = mDoingLsub;
	return NS_OK;
}

NS_IMETHODIMP
nsImapIncomingServer::ReDiscoverAllFolders()
{
    nsresult rv = PerformExpand(nsnull);
    return rv;
}
