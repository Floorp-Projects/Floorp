/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   David Bienvenu <bienvenu@netscape.com>
 *   Jeff Tsai <jefft@netscape.com>
 *   Scott MacGregor <mscott@netscape.com>
 *   Seth Spitzer <sspitzer@netscape.com>
 *   Alecf Flett <alecf@netscape.com>
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "msgCore.h"
#include "nsMsgImapCID.h"

#include "nsString.h"
#include "nsReadableUtils.h"

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
#include "nsEnumeratorUtils.h"
#include "nsIEventQueueService.h"
#include "nsIMsgMailNewsUrl.h"
#include "nsIImapService.h"
#include "nsMsgI18N.h"
#include "nsMsgUtf7Utils.h"
#include "nsAutoLock.h"
#include "nsIImapMockChannel.h"
#include "nsIPrompt.h"
#include "nsIWindowWatcher.h"
// for the memory cache...
#include "nsICacheEntryDescriptor.h"
#include "nsImapUrl.h"
#include "nsFileStream.h"
#include "nsIMsgProtocolInfo.h"

#include "nsITimer.h"
#include "nsMsgUtils.h"

static NS_DEFINE_CID(kCImapHostSessionList, NS_IIMAPHOSTSESSIONLIST_CID);
static NS_DEFINE_CID(kImapProtocolCID, NS_IMAPPROTOCOL_CID);
static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);
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
  m_canHaveFilters = PR_TRUE;
  m_userAuthenticated = PR_FALSE;
  m_readPFCName = PR_FALSE;
}

nsImapIncomingServer::~nsImapIncomingServer()
{
    nsresult rv;
    rv = ClearInner();
    NS_ASSERTION(NS_SUCCEEDED(rv), "ClearInner failed");

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
	nsCOMPtr<nsIImapHostSessionList> hostSession = 
	         do_GetService(kCImapHostSessionList, &rv);
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
      personalNamespace.Adopt(nsCRT::strdup("\"\""));

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

  nsCOMPtr<nsIMsgAccountManager> accountManager = 
           do_GetService(NS_MSGACCOUNTMANAGER_CONTRACTID, &rv);
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
    rv = GetRealUsername(getter_Copies(username));
    if (NS_FAILED(rv)) return rv;
    rv = GetRealHostName(getter_Copies(hostName));
    if (NS_FAILED(rv)) return rv;
    if ((const char*)username && (const char *) hostName &&
        PL_strcmp((const char*)username, "")!=0) {
      emailAddress.AssignWithConversion(username);
      emailAddress.AppendWithConversion("@");
      emailAddress.AppendWithConversion(hostName);

    }
  }
  rv = GetFormattedName(emailAddress.get(), retval);
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
        nsCOMPtr<nsIImapHostSessionList> hostSession = 
                 do_GetService(kCImapHostSessionList, &rv);
        if (NS_SUCCEEDED(rv))
            hostSession->SetOnlineDirForHost(serverKey, dirString.get());
    }
    return SetCharValue("server_sub_directory", dirString.get());
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
        nsCOMPtr<nsIImapHostSessionList> hostSession = 
                 do_GetService(kCImapHostSessionList, &rv);
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
        nsCOMPtr<nsIImapHostSessionList> hostSession = 
                 do_GetService(kCImapHostSessionList, &rv);
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

NS_IMPL_SERVERPREF_BOOL(nsImapIncomingServer, StoreReadMailInPFC,
                        "store_read_mail_in_pfc");
			
NS_IMPL_SERVERPREF_BOOL(nsImapIncomingServer, StoreSentMailInPFC,
                        "store_sent_mail_in_pfc");

NS_IMPL_SERVERPREF_BOOL(nsImapIncomingServer, DownloadBodiesOnGetNewMail,
                        "download_bodies_on_get_new_mail");
//NS_IMPL_SERVERPREF_INT(nsImapIncomingServer, DeleteModel,
//                       "delete_model");

NS_IMETHODIMP								   	
nsImapIncomingServer::GetDeleteModel(PRInt32 *retval)
{						
  PRBool isAOLServer = PR_FALSE;

  NS_ENSURE_ARG(retval);

  GetIsAOLServer(&isAOLServer);
  nsXPIDLCString hostName;
  GetHostName(getter_Copies(hostName));

  if (isAOLServer && ((const char *) hostName) && !nsCRT::strcmp(hostName, "imap.mail.aol.com"))
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
  nsresult rv = SetIntValue("delete_model", ivalue);			
  if (NS_SUCCEEDED(rv))
  {
    nsCOMPtr<nsIImapHostSessionList> hostSession = 
        do_GetService(kCImapHostSessionList, &rv);
    NS_ENSURE_SUCCESS(rv,rv);
    hostSession->SetDeleteIsMoveToTrashForHost(m_serverKey, ivalue == nsMsgImapDeleteModels::MoveToTrash); 
    hostSession->SetShowDeletedMessagesForHost(m_serverKey, ivalue == nsMsgImapDeleteModels::IMAPDelete);
  }
  return rv;
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
    nsCOMPtr<nsISupports> aSupport(getter_AddRefs(m_urlQueue->ElementAt(0)));
    nsCOMPtr<nsIImapUrl> aImapUrl(do_QueryInterface(aSupport, &rv));
    nsCOMPtr<nsIMsgMailNewsUrl> aMailNewsUrl(do_QueryInterface(aSupport, &rv));

    if (aImapUrl)
    {
      nsCOMPtr <nsIImapMockChannel> mockChannel;

      if (NS_SUCCEEDED(aImapUrl->GetMockChannel(getter_AddRefs(mockChannel))) && mockChannel)
      {
        nsCOMPtr<nsIRequest> request = do_QueryInterface(mockChannel);
        if (!request)
          return NS_ERROR_FAILURE;
        request->GetStatus(&rv);
        if (!NS_SUCCEEDED(rv))
        {
          nsresult res;
          removeUrlFromQueue = PR_TRUE;

          mockChannel->Close(); // try closing it to get channel listener nulled out.

          if (aMailNewsUrl)
          {
            nsCOMPtr<nsICacheEntryDescriptor>  cacheEntry;
            res = aMailNewsUrl->GetMemCacheEntry(getter_AddRefs(cacheEntry));
            if (NS_SUCCEEDED(res) && cacheEntry)
              cacheEntry->Doom();
          }
        }
      }
      // Note that we're relying on no one diddling the rv from the mock channel status
      // between the place we set it and here.
      if (NS_SUCCEEDED(rv))
      {
        nsISupports *aConsumer = (nsISupports*)m_urlConsumers.ElementAt(0);
        NS_IF_ADDREF(aConsumer);
      
        nsCOMPtr <nsIImapProtocol>  protocolInstance ;
        rv = CreateImapConnection(nsnull, aImapUrl, getter_AddRefs(protocolInstance));
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
  PRBool userCancelled = PR_FALSE;

  rv = m_connectionCache->Count(&cnt);
  if (NS_FAILED(rv)) return rv;
  // loop until we find a connection that can run the url, or doesn't have to wait?
  for (PRUint32 i = 0; i < cnt && !canRunUrlImmediately && !canRunButBusy; i++) 
  {
    aSupport = getter_AddRefs(m_connectionCache->ElementAt(i));
    connection = do_QueryInterface(aSupport);
    if (connection)
    {
      if (ConnectionTimeOut(connection))
      {
        connection = nsnull;
        i--; cnt--; // if the connection times out, we'll remove it from the array,
            // so we need to adjust the array index.
      }
      else
        rv = connection->CanHandleUrl(aImapUrl, &canRunUrlImmediately, &canRunButBusy);
    }
    if (NS_FAILED(rv)) 
    {
        connection = nsnull;
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
      connection = nsnull;
  }
  
  if (ConnectionTimeOut(connection))
      connection = nsnull;
  if (ConnectionTimeOut(freeConnection))
    freeConnection = nsnull;
  
  if (!canRunButBusy && redirectLogon && (!connection || !canRunUrlImmediately))
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
      
      rv = RequestOverrideInfo(aMsgWindow);
      if (m_waitingForConnectionInfo)
      canRunButBusy = PR_TRUE;
      else 
        userCancelled = PR_TRUE;
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
  else if (userCancelled)
  {
    rv = NS_BINDING_ABORTED;  // user cancelled
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
#ifdef DEBUG_bienvenu
    NS_ASSERTION(PR_FALSE, "no one will handle this, I don't think");
#endif
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
        nsCOMPtr<nsIImapHostSessionList> hostSession = 
                 do_GetService(kCImapHostSessionList, &rv);
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
    nsCOMPtr<nsIEventQueueService> pEventQService = 
             do_GetService(kEventQueueServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;
	if (!pEventQService) return NS_ERROR_FAILURE;
 
    rv = pEventQService->GetThreadEventQueue(NS_CURRENT_THREAD, getter_AddRefs(queue));
    if (NS_FAILED(rv)) return rv;
    rv = imapService->DiscoverAllFolders(queue, rootMsgFolder, this, aMsgWindow, nsnull);
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
			rv = rootMsgFolder->GetNewMessages(nsnull, nsnull);
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
  mFilterList = nsnull; // clear this to cut shutdown leaks.
  return rv;
}

const char *nsImapIncomingServer::GetPFCName()
{
  if (!m_readPFCName)
  {
    if(NS_SUCCEEDED(GetStringBundle()))
    {
		  nsXPIDLString pfcName;
		  nsresult res = m_stringBundle->GetStringFromID(IMAP_PERSONAL_FILING_CABINET, getter_Copies(pfcName));
      if (NS_SUCCEEDED(res))
        m_pfcName = NS_ConvertUCS2toUTF8(pfcName).get();
    }
    m_readPFCName = PR_TRUE;
  }
  return m_pfcName.get();
}

NS_IMETHODIMP nsImapIncomingServer::GetIsPFC(const char *folderName, PRBool *result)
{
  NS_ENSURE_ARG(result);
  NS_ENSURE_ARG(folderName);
  *result = !nsCRT::strcmp(GetPFCName(), folderName);
  return NS_OK;
}

NS_IMETHODIMP nsImapIncomingServer::GetPFC(PRBool createIfMissing, nsIMsgFolder **pfcFolder)
{
  nsCOMPtr<nsIFolder> rootFolder;
  nsresult rv = GetRootFolder(getter_AddRefs(rootFolder));
  if (NS_SUCCEEDED(rv) && rootFolder)
  {
    nsCOMPtr <nsIMsgFolder> rootMsgFolder = do_QueryInterface(rootFolder);
    nsCOMPtr <nsIMsgFolder> pfcParent;
    nsXPIDLCString serverUri;
    rv = GetServerURI(getter_Copies(serverUri));
    NS_ENSURE_SUCCESS(rv, rv);
    nsCAutoString folderUri(serverUri);
    folderUri.ReplaceSubstring("imap://", "mailbox://");
    folderUri.Append("/");
    folderUri.Append(GetPFCName());

    rootMsgFolder->GetChildWithURI(folderUri, PR_FALSE, PR_FALSE /*caseInsensitive */, pfcFolder);
    if (!*pfcFolder && createIfMissing)
    {
			// get the URI from the incoming server
	    nsCOMPtr<nsIRDFResource> res;
      nsCOMPtr<nsIFileSpec> pfcFileSpec;
	    nsCOMPtr<nsIRDFService> rdf(do_GetService(kRDFServiceCID, &rv));
	    rv = rdf->GetResource((const char *)folderUri, getter_AddRefs(res));
	    NS_ENSURE_SUCCESS(rv, rv);
      nsCOMPtr <nsIMsgFolder> parentToCreate = do_QueryInterface(res, &rv);
      NS_ENSURE_SUCCESS(rv, rv);
      parentToCreate->SetParent(rootFolder);
      parentToCreate->GetPath(getter_AddRefs(pfcFileSpec));

      nsFileSpec path;
      pfcFileSpec->GetFileSpec(&path);
     	nsOutputFileStream outputStream(path, PR_WRONLY | PR_CREATE_FILE, 00600);	
      // can't call CreateStorageIfMissing because our parent is an imap folder
      // and that would create an imap folder.
      // parentToCreate->CreateStorageIfMissing(nsnull);
      *pfcFolder = parentToCreate;
      rootFolder->NotifyItemAdded(rootFolder, parentToCreate, "folderView");
      nsCOMPtr <nsISupports> supports = do_QueryInterface(parentToCreate);
      NS_ASSERTION(supports, "couldn't get isupports from imap folder");
      if (supports)
        rootFolder->AppendElement(supports);

      NS_IF_ADDREF(*pfcFolder);
    }
  }
  return rv;
}

nsresult nsImapIncomingServer::GetPFCForStringId(PRBool createIfMissing, PRInt32 stringId, nsIMsgFolder **aFolder)
{
  NS_ENSURE_ARG_POINTER(aFolder);
  nsCOMPtr <nsIMsgFolder> pfcParent;

  nsresult rv = GetPFC(createIfMissing, getter_AddRefs(pfcParent));
  NS_ENSURE_SUCCESS(rv, rv);
  nsXPIDLCString pfcURI;
  pfcParent->GetURI(getter_Copies(pfcURI));

  rv = GetStringBundle();
  NS_ENSURE_SUCCESS(rv, rv);
	nsXPIDLString pfcName;
	rv = m_stringBundle->GetStringFromID(stringId, getter_Copies(pfcName));
  NS_ENSURE_SUCCESS(rv, rv);
  nsCAutoString pfcMailUri(pfcURI);
//  pfcMailUri.Append(".sbd");
  pfcMailUri.Append("/");
  pfcMailUri.AppendWithConversion(pfcName.get());
  pfcParent->GetChildWithURI(pfcMailUri, PR_FALSE, PR_FALSE /* caseInsensitive*/, aFolder);
  if (!*aFolder && createIfMissing)
  {
		// get the URI from the incoming server
	  nsCOMPtr<nsIRDFResource> res;
	  nsCOMPtr<nsIRDFService> rdf(do_GetService(kRDFServiceCID, &rv));
	  rv = rdf->GetResource((const char *)pfcMailUri, getter_AddRefs(res));
	  NS_ENSURE_SUCCESS(rv, rv);
    nsCOMPtr <nsIMsgFolder> parentToCreate = do_QueryInterface(res, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    parentToCreate->SetParent(pfcParent);
    parentToCreate->CreateStorageIfMissing(nsnull);
    *aFolder = parentToCreate;
    NS_IF_ADDREF(*aFolder);
  }
  return rv;
}

NS_IMETHODIMP nsImapIncomingServer::GetReadMailPFC(PRBool createIfMissing, nsIMsgFolder **aFolder)
{
  NS_ENSURE_ARG_POINTER(aFolder);
  return GetPFCForStringId(createIfMissing, IMAP_PFC_READ_MAIL, aFolder);
}

NS_IMETHODIMP nsImapIncomingServer::GetSentMailPFC(PRBool createIfMissing, nsIMsgFolder **aFolder)
{
  NS_ENSURE_ARG_POINTER(aFolder);
  return GetPFCForStringId(createIfMissing, IMAP_PFC_SENT_MAIL, aFolder);
}

// nsIImapServerSink impl
NS_IMETHODIMP nsImapIncomingServer::PossibleImapMailbox(const char *folderPath, PRUnichar hierarchyDelimiter, PRInt32 boxFlags)
{
  // folderPath is in canonical format, i.e., hierarchy separator has been replaced with '/'
  nsresult rv;
  PRBool found = PR_FALSE;
  PRBool haveParent = PR_FALSE;
  nsCOMPtr<nsIMsgImapMailFolder> hostFolder;
  nsCOMPtr<nsIMsgFolder> aFolder;
  PRBool explicitlyVerify = PR_FALSE;
    
  if (!folderPath || !*folderPath) return NS_ERROR_NULL_POINTER;

  nsCOMPtr<nsIFolder> rootFolder;
  rv = GetRootFolder(getter_AddRefs(rootFolder));

  if(NS_FAILED(rv))
    return rv;
  nsCOMPtr<nsIMsgFolder> a_nsIFolder(do_QueryInterface(rootFolder, &rv));

  if (NS_FAILED(rv))
    return rv;

  if (mDoingSubscribeDialog) 
  {
    rv = AddTo(folderPath, mDoingLsub /* add as subscribed */, mDoingLsub /* change if exists */);
    NS_ENSURE_SUCCESS(rv,rv);
    // Make sure the imapmailfolder object has the right delimiter because the unsubscribed
    // folders (those not in the 'lsub' list) have the delimiter set to the default ('^').
    if (a_nsIFolder && folderPath && (*folderPath))
    {
      nsCOMPtr<nsIMsgFolder> msgFolder;
      nsCOMPtr<nsIFolder> subFolder;
      rv = a_nsIFolder->FindSubFolder(folderPath, getter_AddRefs(subFolder));
      NS_ENSURE_SUCCESS(rv,rv);
      msgFolder = do_QueryInterface(subFolder, &rv);
      NS_ENSURE_SUCCESS(rv,rv);
      nsCOMPtr<nsIMsgImapMailFolder> imapFolder = do_QueryInterface(msgFolder, &rv);
      NS_ENSURE_SUCCESS(rv,rv);
      imapFolder->SetHierarchyDelimiter(hierarchyDelimiter);
      return rv;
    }
  }

  hostFolder = do_QueryInterface(a_nsIFolder, &rv);
  if (NS_FAILED(rv))
    return rv;

  nsCAutoString dupFolderPath(folderPath);
  if (dupFolderPath.Last() == hierarchyDelimiter)
  {
      dupFolderPath.SetLength(dupFolderPath.Length()-1);
      // *** this is what we did in 4.x in order to list uw folder only
      // mailbox in order to get the \NoSelect flag
      explicitlyVerify = !(boxFlags & kNameSpace);
  }
  nsCAutoString tempFolderName(dupFolderPath);
  nsCAutoString tokenStr, remStr, changedStr;
  PRInt32 slashPos = tempFolderName.FindChar('/');
  if (slashPos > 0)
  {
    tempFolderName.Left(tokenStr,slashPos);
    tempFolderName.Right(remStr, tempFolderName.Length()-slashPos);
  }
  else
    tokenStr.Assign(tempFolderName);
  
  if ((nsCRT::strcasecmp(tokenStr.get(), "INBOX")==0) && (nsCRT::strcmp(tokenStr.get(), "INBOX") != 0))
    changedStr.Append("INBOX");
  else
    changedStr.Append(tokenStr);

  if (slashPos > 0 ) 
    changedStr.Append(remStr);

  dupFolderPath.Assign(changedStr);
  nsCAutoString folderName(dupFolderPath);
        
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
        folderName.Cut(0, leafPos + 1);	// get rid of the parent name
		haveParent = PR_TRUE;
        parentUri.Append('/');
        parentUri.Append(parentName);
    }
    if (nsCRT::strcasecmp("INBOX", folderPath) == 0 &&
        hierarchyDelimiter == kOnlineHierarchySeparatorNil)
    {
        hierarchyDelimiter = '/'; // set to default in this case (as in 4.x)
        hostFolder->SetHierarchyDelimiter(hierarchyDelimiter);
    }

    // Check to see if we need to ignore this folder (like AOL's 'RECYCLE_OUT').
    PRBool hideFolder;
    rv = HideFolderName(dupFolderPath.get(), &hideFolder);
    if (hideFolder)
      return NS_OK;

	nsCOMPtr <nsIMsgFolder> child;

//	nsCString possibleName(aSpec->allocatedPathName);


  	uri.Append('/');
	uri.Append(dupFolderPath);


    PRBool caseInsensitive = (nsCRT::strcasecmp("INBOX", dupFolderPath) == 0);
    a_nsIFolder->GetChildWithURI(uri, PR_TRUE, caseInsensitive, getter_AddRefs(child));
	if (child)
		found = PR_TRUE;
    if (!found)
    {
		// trying to find/discover the parent
		if (haveParent)
		{	
            nsCOMPtr <nsIMsgFolder> parent;
            caseInsensitive = (nsCRT::strcasecmp("INBOX", parentName) == 0);
            a_nsIFolder->GetChildWithURI(parentUri, PR_TRUE, caseInsensitive, getter_AddRefs(parent));
			if (!parent /* || parentFolder->GetFolderNeedsAdded()*/)
            {
				PossibleImapMailbox(parentName, hierarchyDelimiter, kNoselect |		// be defensive
											((boxFlags  &	 //only inherit certain flags from the child
											(kPublicMailbox | kOtherUsersMailbox | kPersonalMailbox)))); 
			}
		}

        hostFolder->CreateClientSubfolderInfo(dupFolderPath, hierarchyDelimiter,boxFlags);
        caseInsensitive = (nsCRT::strcasecmp("INBOX", dupFolderPath)== 0);
		a_nsIFolder->GetChildWithURI(uri, PR_TRUE, caseInsensitive , getter_AddRefs(child));
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
      if (boxFlags & kImapTrash)
      {
        PRInt32 deleteModel;
        GetDeleteModel(&deleteModel);
        if (deleteModel == nsMsgImapDeleteModels::MoveToTrash)
          child->SetFlag(MSG_FOLDER_FLAG_TRASH);
      }
      imapFolder->SetBoxFlags(boxFlags);
      imapFolder->SetExplicitlyVerify(explicitlyVerify);
      imapFolder->GetOnlineName(getter_Copies(onlineName));
      if (boxFlags & kNewlyCreatedFolder)
      {
        PRBool setNewFoldersForOffline = PR_FALSE;
        GetOfflineDownload(&setNewFoldersForOffline);
        if (setNewFoldersForOffline)
          child->SetFlag(MSG_FOLDER_FLAG_OFFLINE);
      }
      // online name needs to use the correct hierarchy delimiter (I think...)
      // or the canonical path - one or the other, but be consistent.
      dupFolderPath.ReplaceChar('/', hierarchyDelimiter);
      if (hierarchyDelimiter != '/')
        nsImapUrl::UnescapeSlashes(NS_CONST_CAST(char*, dupFolderPath.get()));

      if (! (onlineName.get()) || nsCRT::strlen(onlineName.get()) == 0
				|| nsCRT::strcmp(onlineName.get(), dupFolderPath))
				imapFolder->SetOnlineName(dupFolderPath);
      if (hierarchyDelimiter != '/')
        nsImapUrl::UnescapeSlashes(NS_CONST_CAST(char*, folderName.get()));
			if (NS_SUCCEEDED(CreatePRUnicharStringFromUTF7(folderName, getter_Copies(unicodeName))))
				child->SetName(unicodeName);
      // Call ConvertFolderName() and HideFolderName() to do special folder name
      // mapping and hiding, if configured to do so. For example, need to hide AOL's
      // 'RECYCLE_OUT' & convert a few AOL folder names. Regular imap accounts
      // will do no-op in the calls.
      nsXPIDLString convertedName;
      PRBool hideFolder;
      rv = HideFolderName(onlineName.get(), &hideFolder);
      if (hideFolder)
      {
        nsCOMPtr<nsISupports> support(do_QueryInterface(child, &rv));
        a_nsIFolder->PropagateDelete(child, PR_FALSE, nsnull);
      }
      else
      {
        rv = ConvertFolderName(onlineName.get(), getter_Copies(convertedName));
        if (NS_SUCCEEDED(rv))
          child->SetPrettyName(convertedName);
      }
    }
  }
  return NS_OK;
}

NS_IMETHODIMP nsImapIncomingServer::GetRedirectorType(char **redirectorType)
{
  nsresult rv;
  
  // Differentiate 'aol' and non-aol redirector type.
  rv = GetCharValue("redirector_type", redirectorType);
  if (*redirectorType && !nsCRT::strcasecmp(*redirectorType, "aol"))
      {
    nsXPIDLCString hostName;
    GetHostName(getter_Copies(hostName));

    // Change redirector_type from "aol" to "netscape" if necessary
    if (hostName.get() && !nsCRT::strcasecmp(hostName, "imap.mail.netcenter.com"))
      SetRedirectorType("netscape");
  }

  return NS_OK;
}

NS_IMETHODIMP nsImapIncomingServer::SetRedirectorType(const char *redirectorType)
{
  return (SetCharValue("redirector_type", redirectorType));
}

NS_IMETHODIMP nsImapIncomingServer::GetTrashFolderByRedirectorType(char **specialTrashName)
{
  NS_ENSURE_ARG_POINTER(specialTrashName);
  *specialTrashName = nsnull;
  nsresult rv;

  // see if it has a predefined trash folder name. The pref setting is like:
  //    pref("imap.aol.treashFolder", "RECYCLE");  where the redirector type = 'aol'
  nsCAutoString prefName;
  rv = CreatePrefNameWithRedirectorType(".trashFolder", prefName);
  if (NS_FAILED(rv)) 
    return NS_OK; // return if no redirector type

  nsCOMPtr <nsIPref> prefs = do_GetService(NS_PREF_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv,rv);

  rv = prefs->GetCharPref(prefName.get(), specialTrashName);
  if (NS_SUCCEEDED(rv) && ((!*specialTrashName) || (!**specialTrashName)))
    return NS_ERROR_FAILURE;
  else
    return rv;
      }

NS_IMETHODIMP nsImapIncomingServer::AllowFolderConversion(PRBool *allowConversion)
{
  NS_ENSURE_ARG_POINTER(allowConversion);

  nsresult rv;
  PRInt32 stringId = 0;
  *allowConversion = PR_FALSE;

  // See if the redirector type allows folder name conversion. The pref setting is like:
  //    pref("imap.aol.convertFolders",true);     where the redirector type = 'aol'
  // Construct pref name (like "imap.aol.hideFolders.RECYCLE_OUT") and get the setting.
  nsCAutoString prefName;
  rv = CreatePrefNameWithRedirectorType(".convertFolders", prefName);
  if (NS_FAILED(rv)) 
    return NS_OK; // return if no redirector type

  nsCOMPtr <nsIPref> prefs = do_GetService(NS_PREF_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv,rv);

  // In case this pref is not set we need to return NS_OK.
  rv = prefs->GetBoolPref(prefName.get(), allowConversion);
  return NS_OK;
    }

NS_IMETHODIMP nsImapIncomingServer::ConvertFolderName(const char *originalName, PRUnichar **convertedName)
{
  NS_ENSURE_ARG_POINTER(convertedName);

  nsresult rv = NS_OK;
  PRInt32 stringId = 0;
  *convertedName = nsnull;

  nsCOMPtr <nsIPref> prefs = do_GetService(NS_PREF_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv,rv);

  // See if the redirector type allows folder name conversion.
  PRBool allowConversion;
  rv = AllowFolderConversion(&allowConversion);
  if (NS_SUCCEEDED(rv) && !allowConversion)
    return NS_ERROR_FAILURE;

  // Get string bundle based on redirector type and convert folder name.
  nsCOMPtr<nsIStringBundle> stringBundle;
  nsCAutoString propertyURL;
  nsXPIDLCString redirectorType;
  GetRedirectorType(getter_Copies(redirectorType));
  if (!redirectorType)
    return NS_ERROR_FAILURE; // return if no redirector type

  propertyURL = "chrome://messenger/locale/";
  propertyURL.Append(redirectorType);
  propertyURL.Append("-imap.properties");

  nsCOMPtr<nsIStringBundleService> sBundleService = do_GetService(NS_STRINGBUNDLE_CONTRACTID, &rv);
  if (NS_SUCCEEDED(rv) && (nsnull != sBundleService)) 
    rv = sBundleService->CreateBundle(propertyURL, getter_AddRefs(stringBundle));
  if (NS_SUCCEEDED(rv))
    rv = stringBundle->GetStringFromName(NS_ConvertASCIItoUCS2(originalName).get(), convertedName);

  if (NS_SUCCEEDED(rv) && ((!*convertedName) || (!**convertedName)))
    return NS_ERROR_FAILURE;
  else
    return rv;
  }

NS_IMETHODIMP nsImapIncomingServer::HideFolderName(const char *folderName, PRBool *hideFolder)
{
  NS_ENSURE_ARG_POINTER(hideFolder);

  nsresult rv;
  *hideFolder = PR_FALSE;

  if (!folderName || !*folderName) return NS_OK;

  // See if the redirector type allows folder hiding. The pref setting is like:
  //    pref("imap.aol.hideFolders.RECYCLE_OUT",true);    where the redirector type = 'aol'
  nsCAutoString prefName;
  rv = CreatePrefNameWithRedirectorType(".hideFolder.", prefName);
  if (NS_FAILED(rv)) 
    return NS_OK; // return if no redirector type

  nsCOMPtr <nsIPref> prefs = do_GetService(NS_PREF_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv,rv);

  prefName.Append(folderName);
  // In case this pref is not set we need to return NS_OK.
  prefs->GetBoolPref(prefName.get(), hideFolder);
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
        nsXPIDLCString uri;
        rv = rootFolder->GetURI(getter_Copies(uri));
        if (NS_SUCCEEDED(rv) && uri)
        {
          nsCAutoString uriString(uri);
          uriString.Append('/');
          uriString.Append(name);
          nsCOMPtr<nsIRDFService> rdf(do_GetService(kRDFServiceCID, &rv));
          if (NS_FAILED(rv)) return rv;
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
        }
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

NS_IMETHODIMP nsImapIncomingServer::OnlineFolderRename(nsIMsgWindow *msgWindow, const char *oldName, const char *newName)
{
    nsresult rv = NS_ERROR_FAILURE;
    if (newName && *newName)
    {
        nsCOMPtr<nsIMsgFolder> me;
        rv = GetFolder(oldName, getter_AddRefs(me));
        if (NS_FAILED(rv)) return rv;
        
		nsCOMPtr<nsIMsgFolder> parent ;
        
		nsCAutoString newNameString(newName);
		nsCAutoString parentName;
		PRInt32 folderStart = newNameString.RFindChar('/');
		if (folderStart > 0)
		{
           newNameString.Left(parentName, folderStart);
		   rv = GetFolder(parentName.get(),getter_AddRefs(parent));
		}
		else  // root is the parent
		{
		   nsCOMPtr<nsIFolder> rootFolder;
           rv = GetRootFolder(getter_AddRefs(rootFolder));
		   parent = do_QueryInterface(rootFolder,&rv);
		}
        if (NS_SUCCEEDED(rv) && parent) 
        {
        nsCOMPtr<nsIMsgImapMailFolder> folder;
        folder = do_QueryInterface(me, &rv);
        if (NS_SUCCEEDED(rv))
            folder->RenameLocal(newName,parent);

		nsCOMPtr<nsIMsgImapMailFolder> parentImapFolder = do_QueryInterface(parent);
        if (parentImapFolder)
            parentImapFolder->RenameClient(msgWindow, me,oldName, newName);
        }
    }
    return rv;
}

NS_IMETHODIMP  nsImapIncomingServer::FolderIsNoSelect(const char *aFolderName, PRBool *result) 
{
   if (!result)
      return NS_ERROR_NULL_POINTER;   
   nsCOMPtr<nsIMsgFolder> msgFolder;
   nsresult rv = GetFolder(aFolderName, getter_AddRefs(msgFolder));
   if (NS_SUCCEEDED(rv) && msgFolder)
   {
     PRUint32 flags;
     msgFolder->GetFlags(&flags);
     *result = ((flags & MSG_FOLDER_FLAG_IMAP_NOSELECT) != 0);
   }
   else
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

NS_IMETHODIMP  nsImapIncomingServer::FolderVerifiedOnline(const char *folderName, PRBool *aResult) 
{
  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = PR_FALSE;
  nsCOMPtr<nsIFolder> rootFolder;
  nsresult rv = GetRootFolder(getter_AddRefs(rootFolder));
  if (NS_SUCCEEDED(rv) && rootFolder)
  {
    nsCOMPtr<nsIFolder> aFolder;
    rv = rootFolder->FindSubFolder(folderName, getter_AddRefs(aFolder));
    if (NS_SUCCEEDED(rv) && aFolder)
    {
      nsCOMPtr<nsIImapMailFolderSink> imapFolder = do_QueryInterface(aFolder);
      if (imapFolder)
        imapFolder->GetFolderVerifiedOnline(aResult);
    }
  }
  return rv;
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
    nsCOMPtr<nsIPref> prefs = do_GetService(NS_PREF_CONTRACTID, &rv);
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
  {
    nsCOMPtr<nsIWindowWatcher> wwatch(do_GetService("@mozilla.org/embedcomp/window-watcher;1"));
    if (wwatch)
      wwatch->GetNewPrompter(0, getter_AddRefs(dialog));
  }

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
  {
    nsCOMPtr<nsIWindowWatcher> wwatch(do_GetService("@mozilla.org/embedcomp/window-watcher;1"));
    if (wwatch)
      wwatch->GetNewPrompter(0, getter_AddRefs(dialog));
  }

	if (aString)
	{
		// skip over the first two words, I guess.
		char *whereRealMessage = PL_strchr(aString, ' ');
		if (whereRealMessage)
			whereRealMessage++;
		if (whereRealMessage)
			whereRealMessage = PL_strchr(whereRealMessage, ' ');
		if (whereRealMessage)
		{
		    PRInt32 len = PL_strlen(whereRealMessage)-1;
		    if (len > 0 && whereRealMessage[len] !=  '.')
        	     whereRealMessage[len] = '.';
		}
    
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
			rv = dialog->Alert(nsnull, message.get());

			PR_Free(serverSaidPrefix);
		}
	}

  return rv;
}

#define IMAP_MSGS_URL       "chrome://messenger/locale/imapMsgs.properties"

nsresult nsImapIncomingServer::GetStringBundle()
{
  nsresult res;
	if (!m_stringBundle)
	{
		char*       propertyURL = NULL;

		propertyURL = IMAP_MSGS_URL;

		nsCOMPtr<nsIStringBundleService> sBundleService = do_GetService(NS_STRINGBUNDLE_CONTRACTID, &res); 
		if (NS_SUCCEEDED(res) && (nsnull != sBundleService)) 
		{
			res = sBundleService->CreateBundle(propertyURL, getter_AddRefs(m_stringBundle));
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
			*aString = ToNewUnicode(resultString);
		}
		else
		{
			*aString = ptrv;
		}
	}
	else
	{
		res = NS_OK;
		*aString = ToNewUnicode(resultString);
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
  {
    nsCOMPtr <nsIMsgImapMailFolder> imapRoot = do_QueryInterface(rootFolder);
    if (imapRoot)
      imapRoot->SetVerifiedAsOnlineFolder(PR_TRUE); // don't need to verify the root.
		rv = GetUnverifiedSubFolders(rootFolder, aFoldersArray, aNumUnverifiedFolders);
  }
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

NS_IMETHODIMP nsImapIncomingServer::GetServerRequiresPasswordForBiff(PRBool *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  // if the user has already been authenticated, we've got the password
  *_retval = !m_userAuthenticated;
  return NS_OK;
}


NS_IMETHODIMP nsImapIncomingServer::PromptForPassword(char ** aPassword,
                                                      nsIMsgWindow * aMsgWindow)
{
    nsXPIDLString passwordTemplate;
    IMAPGetStringByID(IMAP_ENTER_PASSWORD_PROMPT, getter_Copies(passwordTemplate));
    nsXPIDLString passwordTitle; 
    IMAPGetStringByID(IMAP_ENTER_PASSWORD_PROMPT_TITLE, getter_Copies(passwordTitle));
    PRUnichar *passwordText = nsnull;
    nsXPIDLCString hostName;
    nsXPIDLCString userName;
    PRBool okayValue;

    GetRealHostName(getter_Copies(hostName));
    GetRealUsername(getter_Copies(userName));

    passwordText = nsTextFormatter::smprintf(passwordTemplate, (const char *) userName, (const char *) hostName);
    nsresult rv =  GetPasswordWithUI(passwordText, passwordTitle, aMsgWindow,
                                     &okayValue, aPassword);
    nsTextFormatter::smprintf_free(passwordText);
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
	nsCOMPtr<nsIImapHostSessionList> hostSession = 
	         do_GetService(kCImapHostSessionList, &rv);
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

NS_IMPL_GETSET(nsImapIncomingServer, UserAuthenticated, PRBool, m_userAuthenticated);

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

	*manageMailAccountUrl = ToNewCString(m_manageMailAccountUrl);
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
	nsCAutoString contractID(NS_MSGLOGONREDIRECTORSERVICE_CONTRACTID);
	nsXPIDLCString redirectorType;

	GetRedirectorType(getter_Copies(redirectorType));
	contractID.Append('/');
	contractID.Append(redirectorType);

	m_logonRedirector = do_GetService(contractID.get(), &rv);
	if (m_logonRedirector && NS_SUCCEEDED(rv))
	{
		nsCOMPtr <nsIMsgLogonRedirectionRequester> logonRedirectorRequester;
		rv = QueryInterface(NS_GET_IID(nsIMsgLogonRedirectionRequester), getter_AddRefs(logonRedirectorRequester));
		if (NS_SUCCEEDED(rv))
		{
			nsXPIDLCString password;
			nsXPIDLCString userName;
      PRBool requiresPassword = PR_TRUE;

      GetRealUsername(getter_Copies(userName));
      m_logonRedirector->RequiresPassword(userName, &requiresPassword);
      if (requiresPassword)
      {
  			GetPassword(getter_Copies(password));

			  if (!((const char *) password) || nsCRT::strlen((const char *) password) == 0)
				  PromptForPassword(getter_Copies(password), aMsgWindow);

        // if we still don't have a password then the user must have hit cancel so just
        // fall out...
        if (!((const char *) password) || nsCRT::strlen((const char *) password) == 0)
        {
          // be sure to clear the waiting for connection info flag because we aren't waiting
          // anymore for a connection...
          m_waitingForConnectionInfo = PR_FALSE;
          return NS_OK;
        }
      }

      nsCOMPtr<nsIPrompt> dialogPrompter;
      if (aMsgWindow)
        aMsgWindow->GetPromptDialog(getter_AddRefs(dialogPrompter));
  		rv = m_logonRedirector->Logon(userName, password, redirectorType, dialogPrompter, logonRedirectorRequester, nsMsgLogonRedirectionServiceIDs::Imap);
      if (NS_FAILED(rv)) return OnLogonRedirectionError(nsnull, PR_TRUE);
		}
	}

	return rv;
}

NS_IMETHODIMP nsImapIncomingServer::OnLogonRedirectionError(const PRUnichar *pErrMsg, PRBool badPassword)
{
	nsresult rv = NS_OK;

	nsXPIDLString progressString;
	GetImapStringByID(IMAP_LOGIN_FAILED, getter_Copies(progressString));
	
  nsCOMPtr<nsIMsgWindow> msgWindow;
  PRUint32 urlQueueCnt = 0;
	// pull the url out of the queue so we can get the msg window, and try to rerun it.
	m_urlQueue->Count(&urlQueueCnt);

	if (urlQueueCnt > 0)
	{
		nsCOMPtr<nsISupports> supportCtxt(getter_AddRefs(m_urlQueue->ElementAt(0)));
		nsCOMPtr<nsIMsgMailNewsUrl> mailnewsUrl(do_QueryInterface(supportCtxt, &rv));
    if (mailnewsUrl)
      mailnewsUrl->GetMsgWindow(getter_AddRefs(msgWindow));
  }

  FEAlert(progressString, msgWindow);


	if (m_logonRedirector)
	{
		nsXPIDLCString userName;

		GetRealUsername(getter_Copies(userName));
		m_logonRedirector->Logoff(userName);
	}

	if (badPassword)
		SetPassword(nsnull);


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
		  nsCOMPtr<nsIEventQueueService> pEventQService = 
		           do_GetService(kEventQueueServiceCID, &rv); 
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
	nsCOMPtr<nsIEventQueueService> pEventQService = 
	         do_GetService(kEventQueueServiceCID, &rv); 
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
				protocolInstance->OverrideConnectionInfo(pHost, pPort, cookie.get());
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

nsresult
nsImapIncomingServer::SetDelimiterFromHierarchyDelimiter()
{
    nsresult rv = NS_OK;

    nsCOMPtr<nsIImapService> imapService = do_GetService(kImapServiceCID, &rv);
    NS_ENSURE_SUCCESS(rv,rv);
    if (!imapService) return NS_ERROR_FAILURE;
    
    nsCOMPtr<nsIFolder> rootFolder;
    rv = GetRootFolder(getter_AddRefs(rootFolder));
    NS_ENSURE_SUCCESS(rv,rv);
    
    nsCOMPtr<nsIMsgImapMailFolder> rootMsgFolder = do_QueryInterface(rootFolder, &rv);
    NS_ENSURE_SUCCESS(rv,rv);
    if (!rootMsgFolder) return NS_ERROR_FAILURE;
    
    PRUnichar delimiter = '/';
    rv = rootMsgFolder->GetHierarchyDelimiter(&delimiter);
    NS_ENSURE_SUCCESS(rv,rv);
 
#ifdef DEBUG_seth
    printf("setting delimiter to %c\n",char(delimiter));
#endif
    if (delimiter == kOnlineHierarchySeparatorUnknown) {
        delimiter = '/';
#ifdef DEBUG_seth
        printf("..no, override delimiter to %c\n",char(delimiter));
#endif
    }

    rv = SetDelimiter(char(delimiter));     
    NS_ENSURE_SUCCESS(rv,rv);
    
    return NS_OK;
}

NS_IMETHODIMP
nsImapIncomingServer::StartPopulatingWithUri(nsIMsgWindow *aMsgWindow, PRBool aForceToServer /*ignored*/, const char *uri)
{
	nsresult rv;
#ifdef DEBUG_sspitzer
	printf("in StartPopulatingWithUri(%s)\n",uri);
#endif
	mDoingSubscribeDialog = PR_TRUE;	

    rv = EnsureInner();
    NS_ENSURE_SUCCESS(rv,rv);
    rv = mInner->StartPopulatingWithUri(aMsgWindow, aForceToServer, uri);
    NS_ENSURE_SUCCESS(rv,rv);

    rv = SetDelimiterFromHierarchyDelimiter();
    NS_ENSURE_SUCCESS(rv,rv);

    rv = SetShowFullName(PR_FALSE);
    NS_ENSURE_SUCCESS(rv,rv);

    nsXPIDLCString serverUri;
    rv = GetServerURI(getter_Copies(serverUri));
    NS_ENSURE_SUCCESS(rv,rv);

	nsCOMPtr<nsIImapService> imapService = do_GetService(kImapServiceCID, &rv);
    NS_ENSURE_SUCCESS(rv,rv);
	if (!imapService) return NS_ERROR_FAILURE;

    /* 
    if uri = imap://user@host/foo/bar, the serverUri is imap://user@host
    to get path from uri, skip over imap://user@host + 1 (for the /)
    */
    const char *path = uri + nsCRT::strlen((const char *)serverUri) + 1;

    rv = imapService->GetListOfFoldersWithPath(this, aMsgWindow, path);
    NS_ENSURE_SUCCESS(rv,rv);

    return NS_OK;
}

NS_IMETHODIMP
nsImapIncomingServer::StartPopulating(nsIMsgWindow *aMsgWindow, PRBool aForceToServer /*ignored*/)
{
	nsresult rv;
#ifdef DEBUG_sspitzer
	printf("in StartPopulating()\n");
#endif
	mDoingSubscribeDialog = PR_TRUE;	

    rv = EnsureInner();
    NS_ENSURE_SUCCESS(rv,rv);
    rv = mInner->StartPopulating(aMsgWindow, aForceToServer);
    NS_ENSURE_SUCCESS(rv,rv);

    rv = SetDelimiterFromHierarchyDelimiter();
    NS_ENSURE_SUCCESS(rv,rv);

    rv = SetShowFullName(PR_FALSE);
    NS_ENSURE_SUCCESS(rv,rv);

	nsCOMPtr<nsIImapService> imapService = do_GetService(kImapServiceCID, &rv);
    NS_ENSURE_SUCCESS(rv,rv);
	if (!imapService) return NS_ERROR_FAILURE;

    rv = imapService->GetListOfFoldersOnServer(this, aMsgWindow);
    NS_ENSURE_SUCCESS(rv,rv);

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

    // xxx todo get msgWindow from url
    nsCOMPtr<nsIMsgWindow> msgWindow;

    nsCOMPtr<nsIImapUrl> imapUrl = do_QueryInterface(url);
    if (imapUrl) {
        nsImapAction imapAction = nsIImapUrl::nsImapTest;
        imapUrl->GetImapAction(&imapAction);
        switch (imapAction) {
        case nsIImapUrl::nsImapDiscoverAllAndSubscribedBoxesUrl:
        case nsIImapUrl::nsImapDiscoverChildrenUrl:
            rv = UpdateSubscribed();
            if (NS_FAILED(rv)) return rv;
            mDoingSubscribeDialog = PR_FALSE;
            rv = StopPopulating(msgWindow);
            if (NS_FAILED(rv)) return rv;
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
    nsresult rv = EnsureInner();
    NS_ENSURE_SUCCESS(rv,rv);
	return mInner->SetIncomingServer(aServer);
}

NS_IMETHODIMP
nsImapIncomingServer::SetShowFullName(PRBool showFullName)
{
    nsresult rv = EnsureInner();
    NS_ENSURE_SUCCESS(rv,rv);
	return mInner->SetShowFullName(showFullName);
}

NS_IMETHODIMP
nsImapIncomingServer::GetDelimiter(char *aDelimiter)
{
    nsresult rv = EnsureInner();
    NS_ENSURE_SUCCESS(rv,rv);
    return mInner->GetDelimiter(aDelimiter);
}

NS_IMETHODIMP
nsImapIncomingServer::SetDelimiter(char aDelimiter)
{
    nsresult rv = EnsureInner();
    NS_ENSURE_SUCCESS(rv,rv);
	return mInner->SetDelimiter(aDelimiter);
}

NS_IMETHODIMP
nsImapIncomingServer::SetAsSubscribed(const char *path)
{
    nsresult rv = EnsureInner();
    NS_ENSURE_SUCCESS(rv,rv);
	return mInner->SetAsSubscribed(path);
}

NS_IMETHODIMP
nsImapIncomingServer::UpdateSubscribed()
{
#ifdef DEBUG_sspitzer
	printf("for imap, do this when we populate\n");
#endif
	return NS_OK;
}

NS_IMETHODIMP
nsImapIncomingServer::AddTo(const char *aName, PRBool addAsSubscribed,PRBool changeIfExists)
{
    nsresult rv = EnsureInner();
    NS_ENSURE_SUCCESS(rv,rv);

    // quick check if the name we are passed is really modified UTF-7
    // if it isn't, ignore it.  (otherwise, we'll crash.  see #63186)
    // there is a bug in the UW IMAP server where it can send us
    // folder names as literals, instead of MUTF7
    unsigned char *ptr = (unsigned char *)aName;
    PRBool nameIsClean = PR_TRUE;
    while (*ptr) {
        if (*ptr > 127) {
            nameIsClean = PR_FALSE;
            break;
        }
        ptr++;
    }

    NS_ASSERTION(nameIsClean,"folder path was not in UTF7, ignore it");
    if (!nameIsClean) return NS_OK;

    return mInner->AddTo(aName, addAsSubscribed, changeIfExists);
}

NS_IMETHODIMP
nsImapIncomingServer::StopPopulating(nsIMsgWindow *aMsgWindow)
{
	nsresult rv;

    nsCOMPtr<nsISubscribeListener> listener;
    rv = GetSubscribeListener(getter_AddRefs(listener));
    if (NS_FAILED(rv)) return rv;
    if (!listener) return NS_ERROR_FAILURE;

    rv = listener->OnDonePopulating();
    if (NS_FAILED(rv)) return rv;
	
    rv = EnsureInner();
    NS_ENSURE_SUCCESS(rv,rv);
	rv = mInner->StopPopulating(aMsgWindow);
    NS_ENSURE_SUCCESS(rv,rv);

    //xxx todo when do I set this to null?
    //rv = ClearInner();
    //NS_ENSURE_SUCCESS(rv,rv);
	return NS_OK;
}


NS_IMETHODIMP
nsImapIncomingServer::SubscribeCleanup()
{
	nsresult rv;
    rv = ClearInner();
    NS_ENSURE_SUCCESS(rv,rv);
	return NS_OK;
}

NS_IMETHODIMP
nsImapIncomingServer::SetSubscribeListener(nsISubscribeListener *aListener)
{
    nsresult rv = EnsureInner();
    NS_ENSURE_SUCCESS(rv,rv);
	return mInner->SetSubscribeListener(aListener);
}

NS_IMETHODIMP
nsImapIncomingServer::GetSubscribeListener(nsISubscribeListener **aListener)
{
    nsresult rv = EnsureInner();
    NS_ENSURE_SUCCESS(rv,rv);
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

  // Locate the folder so that the correct hierarchical delimiter is used in the
  // folder pathnames, otherwise root's (ie, '^') is used and this is wrong.
  nsCAutoString folderCName;
  folderCName.AppendWithConversion(aName);
  nsCOMPtr<nsIMsgFolder> msgFolder;
  nsCOMPtr<nsIFolder> subFolder;
  if (rootMsgFolder && aName && (*aName))
  {
    rv = rootMsgFolder->FindSubFolder(folderCName.get(), getter_AddRefs(subFolder));
    if (NS_SUCCEEDED(rv))
      msgFolder = do_QueryInterface(subFolder);
  }

	nsCOMPtr<nsIEventQueue> queue;
    // get the Event Queue for this thread...
    nsCOMPtr<nsIEventQueueService> pEventQService = 
             do_GetService(kEventQueueServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = pEventQService->GetThreadEventQueue(NS_CURRENT_THREAD, getter_AddRefs(queue));
    if (NS_FAILED(rv)) return rv;

	if (subscribe) {
			rv = imapService->SubscribeFolder(queue,
                               msgFolder,
                               aName,
                               nsnull, nsnull);
	}
	else {
			rv = imapService->UnsubscribeFolder(queue,
                               msgFolder,
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

NS_IMETHODIMP
nsImapIncomingServer::SetState(const char *path, PRBool state, PRBool *stateChanged)
{
    nsresult rv = EnsureInner();
    NS_ENSURE_SUCCESS(rv,rv);
    return mInner->SetState(path, state, stateChanged);
}

NS_IMETHODIMP
nsImapIncomingServer::HasChildren(const char *path, PRBool *aHasChildren)
{
    nsresult rv = EnsureInner();
    NS_ENSURE_SUCCESS(rv,rv);
    return mInner->HasChildren(path, aHasChildren);
}

NS_IMETHODIMP
nsImapIncomingServer::IsSubscribed(const char *path, PRBool *aIsSubscribed)
{
    nsresult rv = EnsureInner();
    NS_ENSURE_SUCCESS(rv,rv);
    return mInner->IsSubscribed(path, aIsSubscribed);
}

NS_IMETHODIMP
nsImapIncomingServer::GetLeafName(const char *path, PRUnichar **aLeafName)
{
    nsresult rv = EnsureInner();
    NS_ENSURE_SUCCESS(rv,rv);
    return mInner->GetLeafName(path, aLeafName);
}

NS_IMETHODIMP
nsImapIncomingServer::GetFirstChildURI(const char * path, char **aResult)
{
    nsresult rv = EnsureInner();
    NS_ENSURE_SUCCESS(rv,rv);
    return mInner->GetFirstChildURI(path, aResult);
}


NS_IMETHODIMP
nsImapIncomingServer::GetChildren(const char *path, nsISupportsArray *array)
{
    nsresult rv = EnsureInner();
    NS_ENSURE_SUCCESS(rv,rv);
    return mInner->GetChildren(path, array);
}

nsresult
nsImapIncomingServer::EnsureInner()
{
    nsresult rv = NS_OK;

    if (mInner) return NS_OK;

    mInner = do_CreateInstance(kSubscribableServerCID,&rv);
    NS_ENSURE_SUCCESS(rv,rv);
    if (!mInner) return NS_ERROR_FAILURE;

    rv = SetIncomingServer(this);
    NS_ENSURE_SUCCESS(rv,rv);

    return NS_OK;
}

nsresult
nsImapIncomingServer::ClearInner()
{
    nsresult rv = NS_OK;

    if (mInner) {
        rv = mInner->SetSubscribeListener(nsnull);
        NS_ENSURE_SUCCESS(rv,rv);

        rv = mInner->SetIncomingServer(nsnull);
        NS_ENSURE_SUCCESS(rv,rv);

        mInner = nsnull;
    }
    return NS_OK;
}

NS_IMETHODIMP
nsImapIncomingServer::CommitSubscribeChanges()
{
    return ReDiscoverAllFolders();
}

NS_IMETHODIMP
nsImapIncomingServer::GetCanBeDefaultServer(PRBool *canBeDefaultServer)
{
    *canBeDefaultServer = PR_TRUE;
    return NS_OK;
}

NS_IMETHODIMP
nsImapIncomingServer::GetCanSearchMessages(PRBool *canSearchMessages)
{
    NS_ENSURE_ARG_POINTER(canSearchMessages);
    *canSearchMessages = PR_TRUE;
    return NS_OK;
}

nsresult
nsImapIncomingServer::CreateHostSpecificPrefName(const char *prefPrefix, nsCAutoString &prefName)
{
  NS_ENSURE_ARG_POINTER(prefPrefix);

  nsXPIDLCString hostName;
  nsresult rv = GetHostName(getter_Copies(hostName));
  NS_ENSURE_SUCCESS(rv,rv);

  prefName = prefPrefix;
  prefName.Append(".");
  prefName.Append(hostName.get());

  return NS_OK;
}

NS_IMETHODIMP
nsImapIncomingServer::GetSupportsDiskSpace(PRBool *aSupportsDiskSpace)
{
  NS_ENSURE_ARG_POINTER(aSupportsDiskSpace);
  nsCAutoString prefName;
  nsresult rv = CreateHostSpecificPrefName("default_supports_diskspace", prefName);
  NS_ENSURE_SUCCESS(rv,rv);

  nsCOMPtr<nsIPref> prefs = do_GetService(NS_PREF_CONTRACTID, &rv);
  if(NS_SUCCEEDED(rv)) {
     rv = prefs->GetBoolPref(prefName.get(), aSupportsDiskSpace);
  }

  // Couldn't get the default value with the hostname.
  // Fall back on IMAP default value
  if (NS_FAILED(rv)) {
     // set default value
     *aSupportsDiskSpace = PR_TRUE;
  }
  return NS_OK;
}

// count number of non-busy connections in cache
NS_IMETHODIMP
nsImapIncomingServer::GetNumIdleConnections(PRInt32 *aNumIdleConnections)
{
  NS_ENSURE_ARG_POINTER(aNumIdleConnections);
  *aNumIdleConnections = 0;
  
  nsresult rv = NS_OK;
  nsCOMPtr<nsIImapProtocol> connection;
  PRBool isBusy = PR_FALSE;
  PRBool isInboxConnection;
  PR_CEnterMonitor(this);
  
  PRUint32 cnt;
  nsCOMPtr<nsISupports> aSupport;
  
  rv = m_connectionCache->Count(&cnt);
  if (NS_FAILED(rv)) return rv;
  // loop counting idle connections
  for (PRUint32 i = 0; i < cnt; i++) 
  {
    aSupport = getter_AddRefs(m_connectionCache->ElementAt(i));
    connection = do_QueryInterface(aSupport);
    if (connection)
    {
      rv = connection->IsBusy(&isBusy, &isInboxConnection);
      if (NS_FAILED(rv)) 
        continue;
      if (!isBusy)
        (*aNumIdleConnections)++;
    }
  }
  PR_CExitMonitor(this);
  return rv;
}


/** 
 * Get the preference that tells us whether the imap server in question allows
 * us to create subfolders. Some ISPs might not want users to create any folders
 * besides the existing ones. Then a pref in the format 
 * imap.<redirector type>.canCreateFolders should be added as controller pref
 * to mailnews.js
 * We do want to identify all those servers that don't allow creation of subfolders 
 * and take them out of the account picker in the Copies and Folder panel.
 */
NS_IMETHODIMP
nsImapIncomingServer::GetCanCreateFoldersOnServer(PRBool *aCanCreateFoldersOnServer)
{
    NS_ENSURE_ARG_POINTER(aCanCreateFoldersOnServer);

    nsresult rv;

    // Initialize aCanCreateFoldersOnServer true, a default value for IMAP
    *aCanCreateFoldersOnServer = PR_TRUE;

    nsCAutoString prefName;
    nsCOMPtr<nsIPref> prefs = do_GetService(NS_PREF_CONTRACTID, &rv);

    nsXPIDLCString serverKey;
    rv = GetKey(getter_Copies(serverKey));

    // Time to check if this server has the pref 
    // (mail.server.<serverkey>.canCreateFolders) already set
    nsMsgIncomingServer::getPrefName(serverKey, 
                                     "canCreateFolders", 
                                     prefName);
    rv = prefs->GetBoolPref(prefName.get(), aCanCreateFoldersOnServer);

    // If the server pref is not set in then look at the 
    // pref set with redirector type
    if (NS_FAILED(rv)) {
        rv = CreatePrefNameWithRedirectorType(".canCreateFolders", prefName);
        if (NS_FAILED(rv)) 
            return NS_OK; // return if no redirector type

        if(NS_SUCCEEDED(rv)) {
            rv = prefs->GetBoolPref(prefName.get(), aCanCreateFoldersOnServer);
        }
    }

    // Couldn't get the default value with the redirector type.
    // Fall back on IMAP default value
    if (NS_FAILED(rv)) {
        // set default value
        *aCanCreateFoldersOnServer = PR_TRUE;
    }
    return NS_OK;
}

NS_IMETHODIMP
nsImapIncomingServer::GetOfflineSupportLevel(PRInt32 *aSupportLevel)
{
    NS_ENSURE_ARG_POINTER(aSupportLevel);
    nsresult rv = NS_OK;
    
    rv = GetIntValue("offline_support_level", aSupportLevel);
    if (*aSupportLevel != OFFLINE_SUPPORT_LEVEL_UNDEFINED) return rv;
    
    nsCAutoString prefName;
    rv = CreateHostSpecificPrefName("default_offline_support_level", prefName);
    NS_ENSURE_SUCCESS(rv,rv);

    nsCOMPtr<nsIPref> prefs = do_GetService(NS_PREF_CONTRACTID, &rv);
    if(NS_SUCCEEDED(rv)) {
      rv = prefs->GetIntPref(prefName.get(), aSupportLevel);
    } 

    // Couldn't get the pref value with the hostname. 
    // Fall back on IMAP default value
    if (NS_FAILED(rv)) {
      // set default value
      *aSupportLevel = OFFLINE_SUPPORT_LEVEL_REGULAR;
    }
    return NS_OK;
}

// Called only during the migration process. This routine enables the generation of 
// unique account name based on the username, hostname and the port. If the port 
// is valid and not a default one, it will be appended to the account name.
NS_IMETHODIMP
nsImapIncomingServer::GeneratePrettyNameForMigration(PRUnichar **aPrettyName)
{
    NS_ENSURE_ARG_POINTER(aPrettyName);
    nsresult rv = NS_OK;
   
    nsXPIDLCString userName;
    nsXPIDLCString hostName;

    /**
     * Pretty name for migrated account is of format username@hostname:<port>,
     * provided the port is valid and not the default
     */

    // Get user name to construct pretty name
    rv = GetUsername(getter_Copies(userName));
    NS_ENSURE_SUCCESS(rv, rv);
    
    // Get host name to construct pretty name
    rv = GetHostName(getter_Copies(hostName));
    NS_ENSURE_SUCCESS(rv, rv);

    PRInt32 defaultServerPort;
    PRInt32 defaultSecureServerPort;

    nsCOMPtr <nsIMsgProtocolInfo> protocolInfo = do_GetService("@mozilla.org/messenger/protocol/info;1?type=imap", &rv);
    NS_ENSURE_SUCCESS(rv,rv);

    // Get the default port
    rv = protocolInfo->GetDefaultServerPort(PR_FALSE, &defaultServerPort);
    NS_ENSURE_SUCCESS(rv,rv);

    // Get the default secure port
    rv = protocolInfo->GetDefaultServerPort(PR_TRUE, &defaultSecureServerPort);
    NS_ENSURE_SUCCESS(rv,rv);

    // Get the current server port
    PRInt32 serverPort = PORT_NOT_SET;
    rv = GetPort(&serverPort);
    NS_ENSURE_SUCCESS(rv,rv);

    // Is the server secure ?
    PRBool isSecure = PR_FALSE;
    rv = GetIsSecure(&isSecure);
    NS_ENSURE_SUCCESS(rv,rv);

    // Is server port a default port ?
    PRBool isItDefaultPort = PR_FALSE;
    if (((serverPort == defaultServerPort) && !isSecure)|| 
        ((serverPort == defaultSecureServerPort) && isSecure)) {
        isItDefaultPort = PR_TRUE;
    }

    // Construct pretty name from username and hostname
    nsAutoString constructedPrettyName;
    constructedPrettyName.AssignWithConversion(userName);
    constructedPrettyName.AppendWithConversion("@");
    constructedPrettyName.AppendWithConversion(hostName);

    // If the port is valid and not default, add port value to the pretty name
    if ((serverPort > 0) && (!isItDefaultPort)) {
        constructedPrettyName.AppendWithConversion(":");
        constructedPrettyName.AppendInt(serverPort);
    }

    // Format the pretty name
    rv = GetFormattedName(constructedPrettyName.get(), aPrettyName); 
    NS_ENSURE_SUCCESS(rv, rv);

    return rv;
}

// Take the pretty name and return a formatted account name
nsresult
nsImapIncomingServer::GetFormattedName(const PRUnichar *prettyName, PRUnichar **retval)
{
    nsresult rv;
    rv = GetStringBundle();
    if (m_stringBundle)
    {
        const PRUnichar *formatStrings[] =
        {
            prettyName,
        };
        rv = m_stringBundle->FormatStringFromID(IMAP_DEFAULT_ACCOUNT_NAME,
                                    formatStrings, 1,
                                    retval);
        NS_ENSURE_SUCCESS(rv, rv);
    }
    return rv;
}

nsresult
nsImapIncomingServer::CreatePrefNameWithRedirectorType(const char *prefSuffix, nsCAutoString &prefName)
{
    NS_ENSURE_ARG_POINTER(prefSuffix);

    nsXPIDLCString redirectorType;
    nsresult rv = GetRedirectorType(getter_Copies(redirectorType));
    if (NS_FAILED(rv)) 
        return rv;
    if (!redirectorType)
        return NS_ERROR_FAILURE;
 
    prefName.Assign("imap.");
    prefName.Append(redirectorType);
    prefName.Append(prefSuffix);

    return NS_OK;
}

NS_IMETHODIMP
nsImapIncomingServer::GetCanFileMessagesOnServer(PRBool *aCanFileMessagesOnServer)
{
    NS_ENSURE_ARG_POINTER(aCanFileMessagesOnServer);

    nsresult rv;

    // Initialize aCanFileMessagesOnServer true, a default value for IMAP
    *aCanFileMessagesOnServer = PR_TRUE;

    nsCAutoString prefName;
    nsCOMPtr<nsIPref> prefs = do_GetService(NS_PREF_CONTRACTID, &rv);

    nsXPIDLCString serverKey;
    rv = GetKey(getter_Copies(serverKey));

    // Time to check if this server has the pref 
    // (mail.server.<serverkey>.canFileMessages) already set
    nsMsgIncomingServer::getPrefName(serverKey, 
                                     "canFileMessages", 
                                     prefName);
    rv = prefs->GetBoolPref(prefName.get(), aCanFileMessagesOnServer);

    // If the server pref is not set in then look at the 
    // pref set with redirector type
    if (NS_FAILED(rv)) {
        rv = CreatePrefNameWithRedirectorType(".canFileMessages", prefName);
        if (NS_FAILED(rv)) 
            return NS_OK; //  return if no redirector type

        if(NS_SUCCEEDED(rv)) {
            rv = prefs->GetBoolPref(prefName.get(), aCanFileMessagesOnServer);
        }
    }

    // Couldn't get the default value with the redirector type.
    // Fall back on IMAP default value
    if (NS_FAILED(rv)) {
        // set default value
        *aCanFileMessagesOnServer = PR_TRUE;
    }
    return NS_OK;
}

NS_IMETHODIMP
nsImapIncomingServer::SetSearchValue(const char *searchValue)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsImapIncomingServer::GetSupportsSubscribeSearch(PRBool *retVal)
{
   *retVal = PR_FALSE;
   return NS_OK;
}

NS_IMETHODIMP
nsImapIncomingServer::GetFilterScope(nsMsgSearchScopeValue *filterScope)
{
   NS_ENSURE_ARG_POINTER(filterScope);

   *filterScope = nsMsgSearchScope::onlineMailFilter;
   return NS_OK;
}

NS_IMETHODIMP
nsImapIncomingServer::GetSearchScope(nsMsgSearchScopeValue *searchScope)
{
   NS_ENSURE_ARG_POINTER(searchScope);
   
   if (WeAreOffline()) {
     *searchScope = nsMsgSearchScope::offlineMail;
   }
   else {
     *searchScope = nsMsgSearchScope::onlineMail;
   }
   return NS_OK;
}

// Gets new messages (imap /Select) for this folder and all subfolders except 
// Trash. This is a recursive function. Gets new messages for current folder
// first, then calls self recursively for each subfolder.
NS_IMETHODIMP 
nsImapIncomingServer::GetNewMessagesAllFolders(nsIMsgFolder *aRootFolder, nsIMsgWindow *aWindow)
{
  nsresult retval = NS_OK;

  if (!aRootFolder)
    return retval;

  // If this is the trash folder, we don't need to do anything here or with
  // any subfolders.
  PRUint32 flags = 0;
  aRootFolder->GetFlags(&flags);
  if (flags & MSG_FOLDER_FLAG_TRASH)
    return retval;

  // Get new messages for this folder. 
  aRootFolder->UpdateFolder(aWindow);

  // Loop through all subfolders to get new messages for them.
  nsCOMPtr<nsIEnumerator> aEnumerator;
  retval = aRootFolder->GetSubFolders(getter_AddRefs(aEnumerator));
  NS_ASSERTION((NS_SUCCEEDED(retval) && aEnumerator), "GetSubFolders() failed to return enumerator.");
  if (NS_FAILED(retval)) 
    return retval;

  nsresult more = aEnumerator->First();

  while (NS_SUCCEEDED(more))
  {
    nsCOMPtr<nsISupports> aSupport;
    nsresult rv = aEnumerator->CurrentItem(getter_AddRefs(aSupport));
    NS_ASSERTION((NS_SUCCEEDED(rv) && aSupport), "CurrentItem() failed.");
			
    nsCOMPtr<nsIMsgFolder> msgFolder = do_QueryInterface(aSupport, &rv);
    NS_ASSERTION((NS_SUCCEEDED(rv) && msgFolder), "nsIMsgFolder service not found.");

    retval = GetNewMessagesAllFolders(msgFolder, aWindow);

    more = aEnumerator->Next();
  }

  return retval;
}

NS_IMETHODIMP
nsImapIncomingServer::OnUserOrHostNameChanged(const char *oldName, const char *newName)
{
  nsresult rv;
  // 1. Do common things in the base class.
  rv = nsMsgIncomingServer::OnUserOrHostNameChanged(oldName, newName);
  NS_ENSURE_SUCCESS(rv,rv);

  // 2. Reset 'HaveWeEverDiscoveredFolders' flag so the new folder list can be
  //    reloaded (ie, DiscoverMailboxList() will be invoked in nsImapProtocol).
  nsCOMPtr<nsIImapHostSessionList> hostSessionList = do_GetService(kCImapHostSessionList, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  nsXPIDLCString serverKey;
  rv = GetKey(getter_Copies(serverKey));
  NS_ENSURE_SUCCESS(rv, rv);
  hostSessionList->SetHaveWeEverDiscoveredFoldersForHost(serverKey.get(), PR_FALSE);

  // 3. Make all the existing folders 'unverified' so that they can be
  //    removed from the folder pane after users log into the new server.
  ResetFoldersToUnverified(nsnull);
  return NS_OK;
}
