/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 *   David Bienvenu <bienvenu@nventure.com>
 *   Jeff Tsai <jefft@netscape.com>
 *   Scott MacGregor <mscott@netscape.com>
 *   Seth Spitzer <sspitzer@netscape.com>
 *   Alec Flett <alecf@netscape.com>
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "msgCore.h"
#include "nsMsgImapCID.h"

#include "netCore.h"

#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsISupportsObsolete.h"

#include "nsIMAPHostSessionList.h"
#include "nsImapIncomingServer.h"
#include "nsIMsgAccountManager.h"
#include "nsIMsgIdentity.h"
#include "nsIImapUrl.h"
#include "nsIUrlListener.h"
#include "nsIEventQueue.h"
#include "nsImapProtocol.h"
#include "nsISupportsArray.h"
#include "nsVoidArray.h"
#include "nsCOMPtr.h"
#include "nsImapStringBundle.h"
#include "nsIPrefBranch.h"
#include "nsIPrefService.h"
#include "nsMsgFolderFlags.h"
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
#include "nsAutoLock.h"
#include "nsIImapMockChannel.h"
#include "nsIPrompt.h"
#include "nsIWindowWatcher.h"
// for the memory cache...
#include "nsICacheEntryDescriptor.h"
#include "nsImapUrl.h"
#include "nsFileStream.h"
#include "nsIMsgProtocolInfo.h"
#include "nsIMsgMailSession.h"
#include "nsIMAPNamespace.h"
#include "nsISignatureVerifier.h"

#include "nsITimer.h"
#include "nsMsgUtils.h"

#define PREF_TRASH_FOLDER_NAME "trash_folder_name"
#define DEFAULT_TRASH_FOLDER_NAME "Trash"

static NS_DEFINE_CID(kImapProtocolCID, NS_IMAPPROTOCOL_CID);
static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_CID(kImapServiceCID, NS_IMAPSERVICE_CID);
static NS_DEFINE_CID(kSubscribableServerCID, NS_SUBSCRIBABLESERVER_CID);
static NS_DEFINE_CID(kCImapHostSessionListCID, NS_IIMAPHOSTSESSIONLIST_CID);

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
  m_readRedirectorType = PR_FALSE;
}

nsImapIncomingServer::~nsImapIncomingServer()
{
    nsresult rv = ClearInner();
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
	   do_GetService(kCImapHostSessionListCID, &rv);
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
  NS_ENSURE_SUCCESS(rv,rv);

  nsCOMPtr<nsIMsgIdentity> identity;
  rv = accountManager->GetFirstIdentityForServer(this, getter_AddRefs(identity));
  NS_ENSURE_SUCCESS(rv,rv);

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
      emailAddress.AppendLiteral("@");
      emailAddress.AppendWithConversion(hostName);

    }
  }

  rv = GetFormattedStringFromID(emailAddress.get(), IMAP_DEFAULT_ACCOUNT_NAME, retval);
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
    nsXPIDLCString serverKey;
    nsresult rv = GetKey(getter_Copies(serverKey));
    if (NS_SUCCEEDED(rv))
    {
        nsCOMPtr<nsIImapHostSessionList> hostSession = 
                 do_GetService(kCImapHostSessionListCID, &rv);
        if (NS_SUCCEEDED(rv))
            hostSession->SetOnlineDirForHost(serverKey, serverDirectory);
    }
    return SetCharValue("server_sub_directory", serverDirectory);
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
                 do_GetService(kCImapHostSessionListCID, &rv);
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
                 do_GetService(kCImapHostSessionListCID, &rv);
        if (NS_SUCCEEDED(rv))
            hostSession->SetHostIsUsingSubscription(serverKey, bVal);
    }
    return SetBoolValue("using_subscription", bVal);
}

NS_IMPL_SERVERPREF_BOOL(nsImapIncomingServer, DualUseFolders,
                        "dual_use_folders")
			
			
NS_IMPL_SERVERPREF_STR(nsImapIncomingServer, AdminUrl,
                       "admin_url")

NS_IMPL_SERVERPREF_BOOL(nsImapIncomingServer, CleanupInboxOnExit,
                        "cleanup_inbox_on_exit")
			
NS_IMPL_SERVERPREF_BOOL(nsImapIncomingServer, OfflineDownload,
                        "offline_download")

NS_IMPL_SERVERPREF_INT(nsImapIncomingServer, MaximumConnectionsNumber,
                       "max_cached_connections")

NS_IMPL_SERVERPREF_INT(nsImapIncomingServer, EmptyTrashThreshhold,
                       "empty_trash_threshhold")

NS_IMPL_SERVERPREF_BOOL(nsImapIncomingServer, StoreReadMailInPFC,
                        "store_read_mail_in_pfc")
			
NS_IMPL_SERVERPREF_BOOL(nsImapIncomingServer, StoreSentMailInPFC,
                        "store_sent_mail_in_pfc")

NS_IMPL_SERVERPREF_BOOL(nsImapIncomingServer, DownloadBodiesOnGetNewMail,
                        "download_bodies_on_get_new_mail")

NS_IMPL_SERVERPREF_BOOL(nsImapIncomingServer, UseIdle,
                        "use_idle")
//NS_IMPL_SERVERPREF_INT(nsImapIncomingServer, DeleteModel,
//                       "delete_model")

NS_IMETHODIMP								   	
nsImapIncomingServer::GetDeleteModel(PRInt32 *retval)
{						
  NS_ENSURE_ARG(retval);

  nsXPIDLCString redirectorType;
  GetRedirectorType(getter_Copies(redirectorType));
  if (redirectorType.Equals("aol"))
  {
    PRBool suppressPseudoView = PR_FALSE;
    GetBoolAttribute("suppresspseudoview", &suppressPseudoView);
    if (!suppressPseudoView)
      *retval = nsMsgImapDeleteModels::DeleteNoTrash;
    else
      *retval = nsMsgImapDeleteModels::IMAPDelete;
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
        do_GetService(kCImapHostSessionListCID, &rv);
    NS_ENSURE_SUCCESS(rv,rv);
    hostSession->SetDeleteIsMoveToTrashForHost(m_serverKey.get(), ivalue == nsMsgImapDeleteModels::MoveToTrash); 
    hostSession->SetShowDeletedMessagesForHost(m_serverKey.get(), ivalue == nsMsgImapDeleteModels::IMAPDelete);
  }
  return rv;
}

NS_IMPL_SERVERPREF_INT(nsImapIncomingServer, TimeOutLimits,
                       "timeout")

NS_IMPL_SERVERPREF_INT(nsImapIncomingServer, CapabilityPref,
                       "capability")

NS_IMPL_SERVERPREF_STR(nsImapIncomingServer, PersonalNamespace,
                       "namespace.personal")

NS_IMPL_SERVERPREF_STR(nsImapIncomingServer, PublicNamespace,
                       "namespace.public")

NS_IMPL_SERVERPREF_STR(nsImapIncomingServer, OtherUsersNamespace,
                       "namespace.other_users")

NS_IMPL_SERVERPREF_BOOL(nsImapIncomingServer, FetchByChunks,
                       "fetch_by_chunks")

NS_IMPL_SERVERPREF_BOOL(nsImapIncomingServer, MimePartsOnDemand,
                       "mime_parts_on_demand")

NS_IMPL_SERVERPREF_BOOL(nsImapIncomingServer, AOLMailboxView,
                       "aol_mailbox_view")

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
  
  rv = GetImapConnection(aClientEventQueue, aImapUrl, getter_AddRefs(aProtocol));
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIMsgMailNewsUrl> mailnewsurl = do_QueryInterface(aImapUrl, &rv);
  if (aProtocol)
  {
    rv = aProtocol->LoadImapUrl(mailnewsurl, aConsumer);
    // *** jt - in case of the time out situation or the connection gets
    // terminated by some unforseen problems let's give it a second chance
    // to run the url
    if (NS_FAILED(rv))
    {
      NS_ASSERTION(PR_FALSE, "shouldn't get an error loading url");
      rv = aProtocol->LoadImapUrl(mailnewsurl, aConsumer);
    }
  }
  else
  {   // unable to get an imap connection to run the url; add to the url
    // queue
    nsImapProtocol::LogImapUrl("queuing url", aImapUrl);
    PR_CEnterMonitor(this);
    nsCOMPtr <nsISupports> supports(do_QueryInterface(aImapUrl));
    if (supports)
      m_urlQueue->AppendElement(supports);
    m_urlConsumers.AppendElement((void*)aConsumer);
    NS_IF_ADDREF(aConsumer);
    PR_CExitMonitor(this);
    // let's try running it now - maybe the connection is free now.
    PRBool urlRun;
    rv = LoadNextQueuedUrl(nsnull, &urlRun);
  }

  return rv;
}

// checks to see if there are any queued urls on this incoming server,
// and if so, tries to run the oldest one. Returns true if the url is run
// on the passed in protocol connection.
NS_IMETHODIMP
nsImapIncomingServer::LoadNextQueuedUrl(nsIImapProtocol *aProtocol, PRBool *aResult)
{
  PRUint32 cnt = 0;
  nsresult rv = NS_OK;
  PRBool urlRun = PR_FALSE;
  PRBool keepGoing = PR_TRUE;
  nsCOMPtr <nsIImapProtocol>  protocolInstance ;
  
  nsAutoCMonitor mon(this);
  m_urlQueue->Count(&cnt);

  while (cnt > 0 && !urlRun && keepGoing)
  {
    nsCOMPtr<nsIImapUrl> aImapUrl(do_QueryElementAt(m_urlQueue, 0, &rv));
    nsCOMPtr<nsIMsgMailNewsUrl> aMailNewsUrl(do_QueryInterface(aImapUrl, &rv));
    
    PRBool removeUrlFromQueue = PR_FALSE;
    if (aImapUrl)
    {
      nsImapProtocol::LogImapUrl("considering playing queued url", aImapUrl);
      rv = DoomUrlIfChannelHasError(aImapUrl, &removeUrlFromQueue);
      NS_ENSURE_SUCCESS(rv, rv);
      // if we didn't doom the url, lets run it.
      if (!removeUrlFromQueue)
      {
        nsISupports *aConsumer = (nsISupports*)m_urlConsumers.ElementAt(0);
        NS_IF_ADDREF(aConsumer);
        
        nsImapProtocol::LogImapUrl("creating protocol instance to play queued url", aImapUrl);
        rv = GetImapConnection(nsnull, aImapUrl, getter_AddRefs(protocolInstance));
        if (NS_SUCCEEDED(rv) && protocolInstance)
        {
          nsCOMPtr<nsIURI> url = do_QueryInterface(aImapUrl, &rv);
          if (NS_SUCCEEDED(rv) && url)
          {
            nsImapProtocol::LogImapUrl("playing queued url", aImapUrl);
            rv = protocolInstance->LoadImapUrl(url, aConsumer);
            NS_ASSERTION(NS_SUCCEEDED(rv), "failed running queued url");
            urlRun = PR_TRUE;
            removeUrlFromQueue = PR_TRUE;
          }
        }
        else
        {
          nsImapProtocol::LogImapUrl("failed creating protocol instance to play queued url", aImapUrl);
          keepGoing = PR_FALSE;
        }
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
    *aResult = urlRun && aProtocol && aProtocol == protocolInstance;
  
  return rv;
}

NS_IMETHODIMP
nsImapIncomingServer::AbortQueuedUrls()
{
  PRUint32 cnt = 0;
  nsresult rv = NS_OK;
  
  nsAutoCMonitor mon(this);
  m_urlQueue->Count(&cnt);
  
  while (cnt > 0)
  {
    nsCOMPtr<nsIImapUrl> aImapUrl(do_QueryElementAt(m_urlQueue, cnt - 1, &rv));
    PRBool removeUrlFromQueue = PR_FALSE;
    
    if (aImapUrl)
    {
      rv = DoomUrlIfChannelHasError(aImapUrl, &removeUrlFromQueue);
      NS_ENSURE_SUCCESS(rv, rv);
      if (removeUrlFromQueue)
      {
        m_urlQueue->RemoveElementAt(cnt - 1);
        m_urlConsumers.RemoveElementAt(cnt - 1);
      }
    }
    cnt--;
  }
  
  return rv;
}

// if this url has a channel with an error, doom it and its mem cache entries,
// and notify url listeners.
nsresult nsImapIncomingServer::DoomUrlIfChannelHasError(nsIImapUrl *aImapUrl, PRBool *urlDoomed)
{
  nsresult rv = NS_OK;

  nsCOMPtr<nsIMsgMailNewsUrl> aMailNewsUrl(do_QueryInterface(aImapUrl, &rv));
  
  if (aMailNewsUrl && aImapUrl)
  {
    nsCOMPtr <nsIImapMockChannel> mockChannel;
    
    if (NS_SUCCEEDED(aImapUrl->GetMockChannel(getter_AddRefs(mockChannel))) && mockChannel)
    {
      nsresult requestStatus;
      nsCOMPtr<nsIRequest> request = do_QueryInterface(mockChannel);
      if (!request)
        return NS_ERROR_FAILURE;
      request->GetStatus(&requestStatus);
      if (NS_FAILED(requestStatus))
      {
        nsresult res;
        *urlDoomed = PR_TRUE;
        nsImapProtocol::LogImapUrl("dooming url", aImapUrl);
        
        mockChannel->Close(); // try closing it to get channel listener nulled out.
        
        if (aMailNewsUrl)
        {
          nsCOMPtr<nsICacheEntryDescriptor>  cacheEntry;
          res = aMailNewsUrl->GetMemCacheEntry(getter_AddRefs(cacheEntry));
          if (NS_SUCCEEDED(res) && cacheEntry)
            cacheEntry->Doom();
          // we're aborting this url - tell listeners
          aMailNewsUrl->SetUrlState(PR_FALSE, NS_MSG_ERROR_URL_ABORTED);
        }
      }
    }
  }
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
nsImapIncomingServer::GetImapConnection(nsIEventQueue *aEventQueue, 
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
  PRBool redirectLogon = !redirectorType.IsEmpty();

  PRInt32 maxConnections = 5; // default to be five
  rv = GetMaximumConnectionsNumber(&maxConnections);
  if (NS_FAILED(rv) || maxConnections == 0)
  {
      maxConnections = 5;
      rv = SetMaximumConnectionsNumber(maxConnections);
  }
  else if (maxConnections < 1)
  {   // forced to use at least 1
      maxConnections = 1;
      rv = SetMaximumConnectionsNumber(maxConnections);
  }

  PRUint32 cnt;
  rv = m_connectionCache->Count(&cnt);
  if (NS_FAILED(rv)) return rv;

  *aImapConnection = nsnull;
	// iterate through the connection cache for a connection that can handle this url.
  PRBool userCancelled = PR_FALSE;

  // loop until we find a connection that can run the url, or doesn't have to wait?
  for (PRUint32 i = 0; i < cnt && !canRunUrlImmediately && !canRunButBusy; i++) 
  {
    connection = do_QueryElementAt(m_connectionCache, i);
    if (connection)
    {
      if (ConnectionTimeOut(connection))
      {
        connection = nsnull;
        i--; cnt--; // if the connection times out, we'll remove it from the array,
            // so we need to adjust the array index.
      }
      else
      {
        rv = connection->CanHandleUrl(aImapUrl, &canRunUrlImmediately, &canRunButBusy);
#ifdef DEBUG_bienvenu
        nsXPIDLCString curSelectedFolderName;
        if (connection)    
          connection->GetSelectedMailboxName(getter_Copies(curSelectedFolderName));
        // check that no other connection is in the same selected state.
        if (!curSelectedFolderName.IsEmpty())
        {
          for (PRUint32 j = 0; j < cnt; j++)
          {
            if (j != i)
            {
              nsCOMPtr<nsIImapProtocol> otherConnection = do_QueryElementAt(m_connectionCache, j);
              if (otherConnection)
              {
                nsXPIDLCString otherSelectedFolderName;
                otherConnection->GetSelectedMailboxName(getter_Copies(otherSelectedFolderName));
                NS_ASSERTION(!curSelectedFolderName.Equals(otherSelectedFolderName), "two connections selected on same folder");
              }

            }
          }
        }
#endif // DEBUG_bienvenu
      }
    }
    if (NS_FAILED(rv)) 
    {
        connection = nsnull;
        rv = NS_OK; // don't want to return this error, just don't use the connection
        continue;
    }

    // if this connection is wrong, but it's not busy, check if we should designate
    // it as the free connection.
    if (!canRunUrlImmediately && !canRunButBusy && connection)
    {
        rv = connection->IsBusy(&isBusy, &isInboxConnection);
        if (NS_FAILED(rv)) 
          continue;
        // if max connections is <= 1, we have to re-use the inbox connection.
        if (!isBusy && (!isInboxConnection || maxConnections <= 1))
        {
          if (!freeConnection)
            freeConnection = connection;
          else  // check which is the better free connection to use.
          {     // We prefer one not in the selected state.
            nsXPIDLCString selectedFolderName;
            connection->GetSelectedMailboxName(getter_Copies(selectedFolderName));
            if (selectedFolderName.IsEmpty())
              freeConnection = connection;
          }
        }
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
  nsImapState requiredState;
  aImapUrl->GetRequiredImapState(&requiredState);
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
  // CanHandleUrl will pretend that some types of urls require a selected state url
  // (e.g., a folder delete or msg append) but we shouldn't create new connections
  // for these types of urls if we have a free connection. So we check the actual
  // required state here.
  else if (cnt < ((PRUint32)maxConnections) && aEventQueue 
      && (!freeConnection || requiredState == nsIImapUrl::nsImapSelectedState))
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
      // caller will queue the url
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
  // a race condition where someone else goes through this code 

  PRBool useSecAuth;
  GetUseSecAuth(&useSecAuth);
  nsresult rv;
  // pre-flight that we have nss - on the ui thread
  if (useSecAuth)
  {
    nsCOMPtr<nsISignatureVerifier> verifier = do_GetService(SIGNATURE_VERIFIER_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  nsIImapProtocol * protocolInstance = nsnull;
  rv = nsComponentManager::CreateInstance(kImapProtocolCID, nsnull,
    NS_GET_IID(nsIImapProtocol),
    (void **) &protocolInstance);
  if (NS_SUCCEEDED(rv) && protocolInstance)
  {
    nsCOMPtr<nsIImapHostSessionList> hostSession = 
      do_GetService(kCImapHostSessionListCID, &rv);
    if (NS_SUCCEEDED(rv))
      rv = protocolInstance->Initialize(hostSession, this, aEventQueue);
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
        connection = do_QueryElementAt(m_connectionCache, i);
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
    PRBool isBusy = PR_FALSE, isInbox = PR_FALSE;
    PRUint32 cnt = 0;
    nsXPIDLCString curFolderName;

    rv = m_connectionCache->Count(&cnt);
    if (NS_FAILED(rv)) return rv;

    PR_CEnterMonitor(this);
    
    for (PRUint32 i=0; i < cnt; i++)
    {
        connection = do_QueryElementAt(m_connectionCache, i);
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
    if (password.IsEmpty())
        return NS_OK;

    rv = ResetFoldersToUnverified(nsnull);
    
    nsCOMPtr<nsIMsgFolder> rootMsgFolder;
    rv = GetRootFolder(getter_AddRefs(rootMsgFolder));
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

NS_IMETHODIMP nsImapIncomingServer::PerformBiff(nsIMsgWindow* aMsgWindow)
{
  
  nsCOMPtr<nsIMsgFolder> rootMsgFolder;
  nsresult rv = GetRootMsgFolder(getter_AddRefs(rootMsgFolder));
  if(NS_SUCCEEDED(rv))
  {
    SetPerformingBiff(PR_TRUE);
    rv = rootMsgFolder->GetNewMessages(aMsgWindow, nsnull);
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
  
  nsresult rv = m_connectionCache->Count(&cnt);
  if (NS_FAILED(rv)) return rv;
  
  for (PRUint32 i = cnt; i>0; i--)
  {
    connection = do_QueryElementAt(m_connectionCache, i-1);
    if (connection)
      connection->TellThreadToDie(PR_TRUE);
  }

  PR_CExitMonitor(this);
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
        CopyUTF16toUTF8(pfcName, m_pfcName);
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
  nsresult rv;
  nsCOMPtr<nsIMsgAccountManager> accountManager = 
           do_GetService(NS_MSGACCOUNTMANAGER_CONTRACTID, &rv);
  if (NS_SUCCEEDED(rv))
  {
    nsCOMPtr <nsIMsgIncomingServer> server; 
    rv = accountManager->GetLocalFoldersServer(getter_AddRefs(server)); 
    if (NS_SUCCEEDED(rv) && server)
    {
      return server->GetRootMsgFolder(pfcFolder);
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
  pfcMailUri.Append(NS_ConvertUCS2toUTF8(pfcName).get());
  pfcParent->GetChildWithURI(pfcMailUri.get(), PR_FALSE, PR_FALSE /* caseInsensitive*/, aFolder);
  if (!*aFolder && createIfMissing)
  {
		// get the URI from the incoming server
	  nsCOMPtr<nsIRDFService> rdf = do_GetService("@mozilla.org/rdf/rdf-service;1", &rv);
    NS_ENSURE_SUCCESS(rv,rv);

    nsCOMPtr<nsIRDFResource> res;
	  rv = rdf->GetResource(pfcMailUri, getter_AddRefs(res));
	  NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr <nsIMsgFolder> parentToCreate = do_QueryInterface(res, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    
    parentToCreate->SetParent(pfcParent);
    parentToCreate->CreateStorageIfMissing(nsnull);
    NS_IF_ADDREF(*aFolder = parentToCreate);
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
// aNewFolder will not be set if we're listing for the subscribe UI, since that's the way 4.x worked.
NS_IMETHODIMP nsImapIncomingServer::PossibleImapMailbox(const char *folderPath, PRUnichar hierarchyDelimiter, 
                                                        PRInt32 boxFlags, PRBool *aNewFolder)
{
  // folderPath is in canonical format, i.e., hierarchy separator has been replaced with '/'
  nsresult rv;
  PRBool found = PR_FALSE;
  PRBool haveParent = PR_FALSE;
  nsCOMPtr<nsIMsgImapMailFolder> hostFolder;
  nsCOMPtr<nsIMsgFolder> aFolder;
  PRBool explicitlyVerify = PR_FALSE;
    
  if (!folderPath || !*folderPath || !aNewFolder) return NS_ERROR_NULL_POINTER;

  *aNewFolder = PR_FALSE;
  nsCOMPtr<nsIMsgFolder> a_nsIFolder;
  rv = GetRootFolder(getter_AddRefs(a_nsIFolder));

  if(NS_FAILED(rv))
    return rv;

  nsCAutoString dupFolderPath(folderPath);
  if (dupFolderPath.Last() == hierarchyDelimiter)
  {
      dupFolderPath.SetLength(dupFolderPath.Length()-1);
      // *** this is what we did in 4.x in order to list uw folder only
      // mailbox in order to get the \NoSelect flag
      explicitlyVerify = !(boxFlags & kNameSpace);
  }
  if (mDoingSubscribeDialog) 
  {
    // Make sure the imapmailfolder object has the right delimiter because the unsubscribed
    // folders (those not in the 'lsub' list) have the delimiter set to the default ('^').
    if (a_nsIFolder && !dupFolderPath.IsEmpty())
    {
      nsCOMPtr<nsIMsgFolder> msgFolder;
      PRBool isNamespace = PR_FALSE;
      PRBool noSelect = PR_FALSE;

      rv = a_nsIFolder->FindSubFolder(dupFolderPath, getter_AddRefs(msgFolder));
      NS_ENSURE_SUCCESS(rv,rv);
      m_subscribeFolders.AppendObject(msgFolder);
      noSelect = (boxFlags & kNoselect) != 0;
      nsCOMPtr<nsIMsgImapMailFolder> imapFolder = do_QueryInterface(msgFolder, &rv);
      NS_ENSURE_SUCCESS(rv,rv);
      imapFolder->SetHierarchyDelimiter(hierarchyDelimiter);
      isNamespace = (boxFlags & kNameSpace) != 0;
      if (!isNamespace)
        rv = AddTo(dupFolderPath.get(), mDoingLsub && !noSelect/* add as subscribed */, !noSelect, mDoingLsub /* change if exists */);
      NS_ENSURE_SUCCESS(rv,rv);
      return rv;
    }
  }

  hostFolder = do_QueryInterface(a_nsIFolder, &rv);
  if (NS_FAILED(rv))
    return rv;

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
  
  
  PRBool caseInsensitive = (nsCRT::strcasecmp("INBOX", dupFolderPath.get()) == 0);
  a_nsIFolder->GetChildWithURI(uri.get(), PR_TRUE, caseInsensitive, getter_AddRefs(child));
  // if we couldn't find this folder by URI, tell the imap code it's a new folder to us
  *aNewFolder = !child;
  if (child)
    found = PR_TRUE;
  if (!found)
  {
    // trying to find/discover the parent
    if (haveParent)
    {	
      nsCOMPtr <nsIMsgFolder> parent;
      PRBool parentIsNew;
      caseInsensitive = (nsCRT::strcasecmp("INBOX", parentName.get()) == 0);
      a_nsIFolder->GetChildWithURI(parentUri.get(), PR_TRUE, caseInsensitive, getter_AddRefs(parent));
      if (!parent /* || parentFolder->GetFolderNeedsAdded()*/)
      {
        PossibleImapMailbox(parentName.get(), hierarchyDelimiter, kNoselect |	// be defensive
          ((boxFlags  &	//only inherit certain flags from the child
          (kPublicMailbox | kOtherUsersMailbox | kPersonalMailbox))), &parentIsNew); 
      }
    }
    
    hostFolder->CreateClientSubfolderInfo(dupFolderPath.get(), hierarchyDelimiter,boxFlags, PR_FALSE);
    caseInsensitive = (nsCRT::strcasecmp("INBOX", dupFolderPath.get())== 0);
    a_nsIFolder->GetChildWithURI(uri.get(), PR_TRUE, caseInsensitive , getter_AddRefs(child));
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
        nsImapUrl::UnescapeSlashes(dupFolderPath.BeginWriting());
      
      if (onlineName.IsEmpty()
        || nsCRT::strcmp(onlineName.get(), dupFolderPath.get()))
        imapFolder->SetOnlineName(dupFolderPath.get());
      if (hierarchyDelimiter != '/')
        nsImapUrl::UnescapeSlashes(folderName.BeginWriting());
      if (NS_SUCCEEDED(CopyMUTF7toUTF16(folderName, unicodeName)))
        child->SetPrettyName(unicodeName);
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

        //make sure rv value is not crunched, it is used to SetPrettyName
        nsXPIDLCString redirectorType;
        GetRedirectorType(getter_Copies(redirectorType)); //Sent mail folder as per aol/netscape webmail
        if ((redirectorType.EqualsLiteral("aol") && onlineName.EqualsLiteral("Sent Items"))
          || (redirectorType.EqualsLiteral("netscape") && onlineName.EqualsLiteral("Sent")))
          //we know that we don't allowConversion for netscape webmail so just use the onlineName
          child->SetFlag(MSG_FOLDER_FLAG_SENTMAIL);

        else if (redirectorType.EqualsLiteral("netscape") && onlineName.EqualsLiteral("Draft"))
          child->SetFlag(MSG_FOLDER_FLAG_DRAFTS);

        else if (redirectorType.EqualsLiteral("aol") && onlineName.EqualsLiteral("RECYCLE"))
          child->SetFlag(MSG_FOLDER_FLAG_TRASH);

        if (NS_SUCCEEDED(rv))
          child->SetPrettyName(convertedName);
      }
    }
  }
  if (!found && child)
    child->SetMsgDatabase(nsnull); // close the db, so we don't hold open all the .msf files for new folders
  return NS_OK;
}

NS_IMETHODIMP nsImapIncomingServer::AddFolderRights(const char *mailboxName, const char *userName, const char *rights)
{
  nsCOMPtr <nsIMsgFolder> rootFolder;
  nsresult rv = GetRootFolder(getter_AddRefs(rootFolder));
  if(NS_SUCCEEDED(rv) && rootFolder)
  {
    nsCOMPtr <nsIMsgImapMailFolder> imapRoot = do_QueryInterface(rootFolder);
    if (imapRoot)
    {
      nsCOMPtr <nsIMsgImapMailFolder> foundFolder;
      rv = imapRoot->FindOnlineSubFolder(mailboxName, getter_AddRefs(foundFolder));
      if (NS_SUCCEEDED(rv) && foundFolder)
        return foundFolder->AddFolderRights(userName, rights);
    }
  }
  return rv;
}

NS_IMETHODIMP nsImapIncomingServer::FolderNeedsACLInitialized(const char *folderPath, PRBool *aNeedsACLInitialized)
{
  NS_ENSURE_ARG_POINTER(aNeedsACLInitialized);
  nsCOMPtr <nsIMsgFolder> rootFolder;
  nsresult rv = GetRootFolder(getter_AddRefs(rootFolder));
  if(NS_SUCCEEDED(rv) && rootFolder)
  {
    nsCOMPtr <nsIMsgImapMailFolder> imapRoot = do_QueryInterface(rootFolder);
    if (imapRoot)
    {
      nsCOMPtr <nsIMsgImapMailFolder> foundFolder;
      rv = imapRoot->FindOnlineSubFolder(folderPath, getter_AddRefs(foundFolder));
      if (NS_SUCCEEDED(rv) && foundFolder)
      {
        nsCOMPtr <nsIImapMailFolderSink> folderSink = do_QueryInterface(foundFolder);
        if (folderSink)
          return folderSink->GetFolderNeedsACLListed(aNeedsACLInitialized);
      }
    }
  }
  *aNeedsACLInitialized = PR_FALSE; // maybe we want to say TRUE here...
  return NS_OK;
}

NS_IMETHODIMP nsImapIncomingServer::RefreshFolderRights(const char *folderPath)
{
  nsCOMPtr <nsIMsgFolder> rootFolder;
  nsresult rv = GetRootFolder(getter_AddRefs(rootFolder));
  if(NS_SUCCEEDED(rv) && rootFolder)
  {
    nsCOMPtr <nsIMsgImapMailFolder> imapRoot = do_QueryInterface(rootFolder);
    if (imapRoot)
    {
      nsCOMPtr <nsIMsgImapMailFolder> foundFolder;
      rv = imapRoot->FindOnlineSubFolder(folderPath, getter_AddRefs(foundFolder));
      if (NS_SUCCEEDED(rv) && foundFolder)
        return foundFolder->RefreshFolderRights();
    }
  }
  return rv;
}

NS_IMETHODIMP nsImapIncomingServer::GetRedirectorType(char **redirectorType)
{
  if (m_readRedirectorType)
  {
    *redirectorType = ToNewCString(m_redirectorType);
    return NS_OK;
  }
  nsresult rv;
  
  // Differentiate 'aol' and non-aol redirector type.
  rv = GetCharValue("redirector_type", redirectorType);
  m_redirectorType = *redirectorType;
  m_readRedirectorType = PR_TRUE;

  if (*redirectorType)  
  {
    // we used to use "aol" as the redirector type  
    // for both aol mail and webmail
    // this code migrates webmail accounts to use "netscape" as the
    // redirectory type
    if (!nsCRT::strcasecmp(*redirectorType, "aol"))  
    {
      nsXPIDLCString hostName;
      GetHostName(getter_Copies(hostName));
    
      if (hostName.get() && !nsCRT::strcasecmp(hostName, "imap.mail.netcenter.com"))
        SetRedirectorType("netscape");
    }
  }
  else 
  {
    // for people who have migrated from 4.x or outlook, or mistakenly
    // created redirected accounts as regular imap accounts, 
    // they won't have redirector type set properly
    // this fixes the redirector type for them automatically
    nsCAutoString prefName;
    rv = CreateHostSpecificPrefName("default_redirector_type", prefName);
    NS_ENSURE_SUCCESS(rv,rv);

    nsXPIDLCString defaultRedirectorType;
  
    nsCOMPtr <nsIPrefService> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv,rv);

    nsCOMPtr<nsIPrefBranch> prefBranch; 
    rv = prefs->GetBranch(nsnull, getter_AddRefs(prefBranch)); 
    NS_ENSURE_SUCCESS(rv,rv);

    rv = prefBranch->GetCharPref(prefName.get(), getter_Copies(defaultRedirectorType));
    if (NS_SUCCEEDED(rv) && !defaultRedirectorType.IsEmpty()) 
    {
      // only set redirectory type in memory
      // if we call SetRedirectorType() that sets it in prefs
      // which makes this automatic redirector type repair permanent
      m_redirectorType = defaultRedirectorType.get();
    }
  }
  return NS_OK;
}

NS_IMETHODIMP nsImapIncomingServer::SetRedirectorType(const char *redirectorType)
{
  m_redirectorType = redirectorType;
  return (SetCharValue("redirector_type", redirectorType));
}

NS_IMETHODIMP nsImapIncomingServer::GetTrashFolderByRedirectorType(char **specialTrashName)
{
  NS_ENSURE_ARG_POINTER(specialTrashName);
  *specialTrashName = nsnull;
  nsresult rv;

  // see if it has a predefined trash folder name. The pref setting is like:
  //    pref("imap.aol.trashFolder", "RECYCLE");  where the redirector type = 'aol'
  nsCAutoString prefName;
  rv = CreatePrefNameWithRedirectorType(".trashFolder", prefName);
  if (NS_FAILED(rv)) 
    return NS_OK; // return if no redirector type

  nsCOMPtr<nsIPrefBranch> prefBranch = do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv,rv);

  rv = prefBranch->GetCharPref(prefName.get(), specialTrashName);
  if (NS_SUCCEEDED(rv) && ((!*specialTrashName) || (!**specialTrashName)))
    return NS_ERROR_FAILURE;

  return rv;
}

NS_IMETHODIMP nsImapIncomingServer::AllowFolderConversion(PRBool *allowConversion)
{
  NS_ENSURE_ARG_POINTER(allowConversion);

  nsresult rv;
  *allowConversion = PR_FALSE;

  // See if the redirector type allows folder name conversion. The pref setting is like:
  //    pref("imap.aol.convertFolders",true);     where the redirector type = 'aol'
  // Construct pref name (like "imap.aol.hideFolders.RECYCLE_OUT") and get the setting.
  nsCAutoString prefName;
  rv = CreatePrefNameWithRedirectorType(".convertFolders", prefName);
  if (NS_FAILED(rv)) 
    return NS_OK; // return if no redirector type

  nsCOMPtr<nsIPrefBranch> prefBranch = do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv,rv);

  // In case this pref is not set we need to return NS_OK.
  prefBranch->GetBoolPref(prefName.get(), allowConversion);
  return NS_OK;
}

NS_IMETHODIMP nsImapIncomingServer::ConvertFolderName(const char *originalName, PRUnichar **convertedName)
{
  NS_ENSURE_ARG_POINTER(convertedName);

  nsresult rv = NS_OK;
  *convertedName = nsnull;

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
    rv = sBundleService->CreateBundle(propertyURL.get(), getter_AddRefs(stringBundle));
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

  nsCOMPtr<nsIPrefBranch> prefBranch = do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv,rv);

  prefName.Append(folderName);
  // In case this pref is not set we need to return NS_OK.
  prefBranch->GetBoolPref(prefName.get(), hideFolder);
  return NS_OK;
}

nsresult nsImapIncomingServer::GetFolder(const char* name, nsIMsgFolder** pFolder)
{
    nsresult rv = NS_ERROR_NULL_POINTER;
    if (!name || !*name || !pFolder) return rv;
    *pFolder = nsnull;
    nsCOMPtr<nsIMsgFolder> rootFolder;
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
    if (NS_FAILED(rv)) 
      return rv;
        
		nsCOMPtr<nsIMsgFolder> parent;
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
      rv = GetRootFolder(getter_AddRefs(parent));
    }
    if (NS_SUCCEEDED(rv) && parent) 
    {
      nsCOMPtr<nsIMsgImapMailFolder> folder;
      folder = do_QueryInterface(me, &rv);
      if (NS_SUCCEEDED(rv))
      {
        folder->RenameLocal(newName,parent);
        nsCOMPtr<nsIMsgImapMailFolder> parentImapFolder = do_QueryInterface(parent);

        if (parentImapFolder)
          parentImapFolder->RenameClient(msgWindow, me,oldName, newName);
        
        nsCOMPtr <nsIMsgFolder> newFolder;
        rv = GetFolder(newName, getter_AddRefs(newFolder));
        if (NS_SUCCEEDED(rv))
        {
          nsCOMPtr <nsIAtom> folderRenameAtom;
          folderRenameAtom = do_GetAtom("RenameCompleted");
          newFolder->NotifyFolderEvent(folderRenameAtom);
        }
      }
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
  nsCOMPtr <nsIMsgFolder> rootFolder;
  nsresult rv = GetRootFolder(getter_AddRefs(rootFolder));
  if(NS_SUCCEEDED(rv) && rootFolder)
  {
    nsCOMPtr <nsIMsgImapMailFolder> imapRoot = do_QueryInterface(rootFolder);
    if (imapRoot)
    {
      nsCOMPtr <nsIMsgImapMailFolder> foundFolder;
      rv = imapRoot->FindOnlineSubFolder(aFolderName, getter_AddRefs(foundFolder));
      if (NS_SUCCEEDED(rv) && foundFolder)
        return foundFolder->SetAdminUrl(aFolderAdminUrl);
    }
  }
  return rv;
}

NS_IMETHODIMP  nsImapIncomingServer::FolderVerifiedOnline(const char *folderName, PRBool *aResult) 
{
  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = PR_FALSE;
  nsCOMPtr<nsIMsgFolder> rootFolder;
  nsresult rv = GetRootFolder(getter_AddRefs(rootFolder));
  if (NS_SUCCEEDED(rv) && rootFolder)
  {
    nsCOMPtr<nsIMsgFolder> aFolder;
    rv = rootFolder->FindSubFolder(nsDependentCString(folderName), getter_AddRefs(aFolder));
    if (NS_SUCCEEDED(rv) && aFolder)
    {
      nsCOMPtr<nsIMsgImapMailFolder> imapFolder = do_QueryInterface(aFolder);
      if (imapFolder)
        imapFolder->GetVerifiedAsOnlineFolder(aResult);
    }
  }
  return rv;
}

NS_IMETHODIMP nsImapIncomingServer::DiscoveryDone()
{
  nsresult rv = NS_ERROR_FAILURE;
  //	m_haveDiscoveredAllFolders = PR_TRUE;
  
  if (mDoingSubscribeDialog)
    return NS_OK;
  nsCOMPtr<nsIMsgFolder> rootMsgFolder;
  rv = GetRootFolder(getter_AddRefs(rootMsgFolder));
  if (NS_SUCCEEDED(rv) && rootMsgFolder)
  {
    rootMsgFolder->SetPrefFlag();
    
    // Verify there is only one trash folder. Another might be present if 
    // the trash name has been changed.
    PRUint32 numFolders;
    rv = rootMsgFolder->GetFoldersWithFlag(MSG_FOLDER_FLAG_TRASH, 0, &numFolders, NULL);
    
    if (NS_SUCCEEDED(rv) && numFolders > 1)
    {
      nsXPIDLString trashName;
      if (NS_SUCCEEDED(GetTrashFolderName(getter_Copies(trashName))))
      {
        nsIMsgFolder *trashFolders[2];
        if (NS_SUCCEEDED(rootMsgFolder->GetFoldersWithFlag(MSG_FOLDER_FLAG_TRASH, 2, 
          &numFolders, trashFolders)))
        {
          for (PRUint32 i = 0; i < numFolders; i++)
          {
            nsXPIDLString folderName;
            if (NS_SUCCEEDED(trashFolders[i]->GetName(getter_Copies(folderName))))
            {
              if (!folderName.Equals(trashName))
                trashFolders[i]->ClearFlag(MSG_FOLDER_FLAG_TRASH);
            }

            NS_RELEASE(trashFolders[i]);
          }
        }
      }
    }
  }
  
  PRInt32 numUnverifiedFolders;
  nsCOMPtr<nsISupportsArray> unverifiedFolders;
  
  rv = NS_NewISupportsArray(getter_AddRefs(unverifiedFolders));
  if(NS_FAILED(rv))
    return rv;
  
  PRBool usingSubscription = PR_TRUE;
  GetUsingSubscription(&usingSubscription);

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
      nsCOMPtr<nsIMsgFolder> currentFolder = do_QueryInterface(element, &rv);
      if (NS_FAILED(rv))
        continue;
      if ((!usingSubscription || (NS_SUCCEEDED(currentImapFolder->GetExplicitlyVerify(&explicitlyVerify)) && explicitlyVerify)) ||
        ((NS_SUCCEEDED(currentFolder->GetHasSubFolders(&hasSubFolders)) && hasSubFolders)
        && !NoDescendentsAreVerified(currentFolder)))
      {
        PRBool isNamespace;
        currentImapFolder->GetIsNamespace(&isNamespace);
        if (!isNamespace) // don't list namespaces explicitly
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
      }
      else
      {
        DeleteNonVerifiedFolders(currentFolder);
      }
    }
  }
  
  return rv;
}

nsresult nsImapIncomingServer::DeleteNonVerifiedFolders(nsIMsgFolder *curFolder)
{
  PRBool autoUnsubscribeFromNoSelectFolders = PR_TRUE;
  nsresult rv;
  nsCOMPtr<nsIPrefBranch> prefBranch = do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
  if (NS_SUCCEEDED(rv))
    prefBranch->GetBoolPref("mail.imap.auto_unsubscribe_from_noselect_folders", &autoUnsubscribeFromNoSelectFolders);
  
  nsCOMPtr<nsIEnumerator> subFolders;
  
  rv = curFolder->GetSubFolders(getter_AddRefs(subFolders));
  if(NS_SUCCEEDED(rv))
  {
    nsAdapterEnumerator *simpleEnumerator = new nsAdapterEnumerator(subFolders);
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
    
    nsCOMPtr<nsIMsgFolder> parent;
    rv = curFolder->GetParent(getter_AddRefs(parent));
    
    
    if (NS_SUCCEEDED(rv) && parent)
    {
      nsCOMPtr<nsIMsgImapMailFolder> imapParent = do_QueryInterface(parent);
      if (imapParent)
        imapParent->RemoveSubFolder(curFolder);
    }
    
    return rv;
}


PRBool nsImapIncomingServer::NoDescendentsAreVerified(nsIMsgFolder *parentFolder)
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
          nsCOMPtr <nsIMsgFolder> childFolder = do_QueryInterface(child, &rv);
          rv = childImapFolder->GetVerifiedAsOnlineFolder(&childVerified);
          nobodyIsVerified = !childVerified && NoDescendentsAreVerified(childFolder);
        }
      }
    }
    delete simpleEnumerator;
  }
  
  return nobodyIsVerified;
}


PRBool nsImapIncomingServer::AllDescendentsAreNoSelect(nsIMsgFolder *parentFolder)
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
void nsImapIncomingServer::UnsubscribeFromAllDescendents(nsIMsgFolder *parentFolder)
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
    nsCOMPtr<nsIWindowWatcher> wwatch(do_GetService(NS_WINDOWWATCHER_CONTRACTID));
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

  if (dialog)
  {
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
    
		PRUnichar *serverSaidPrefix = nsnull;
		GetImapStringByID(IMAP_SERVER_SAID, &serverSaidPrefix);
		if (serverSaidPrefix)
		{
			nsAutoString message(serverSaidPrefix);
      // the alert string from the server IS UTF-8!!! We must convert it to unicode
      // correctly before appending it to our error message string...
			message.Append(NS_ConvertUTF8toUCS2(whereRealMessage ? whereRealMessage : aString));
			rv = dialog->Alert(nsnull, message.get());

			PR_Free(serverSaidPrefix);
		}
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
		static const char propertyURL[] = IMAP_MSGS_URL;

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
  nsresult res = NS_OK;
  

  GetStringBundle();
  if (m_stringBundle)
  {
    res = m_stringBundle->GetStringFromID(aMsgId, aString);
    if (NS_SUCCEEDED(res))
      return res;
  }
  nsAutoString	resultString(NS_LITERAL_STRING("String ID "));
  resultString.AppendInt(aMsgId);
  *aString = ToNewUnicode(resultString);
  return NS_OK;
}

NS_IMETHODIMP  nsImapIncomingServer::FormatStringWithHostNameByID(PRInt32 aMsgId, PRUnichar **aString)
{
  nsresult res = NS_OK;
  
  GetStringBundle();
  if (m_stringBundle)
  {
    nsXPIDLCString hostName;
    res = GetRealHostName(getter_Copies(hostName));
    if (NS_SUCCEEDED(res))
    {
      nsAutoString hostStr;
      hostStr.AssignWithConversion(hostName.get());
      const PRUnichar *params[] = { hostStr.get() };
      res = m_stringBundle->FormatStringFromID(aMsgId, params, 1, aString);
      if (NS_SUCCEEDED(res))
        return res;
    }
  }
  nsAutoString	resultString(NS_LITERAL_STRING("String ID "));
  resultString.AppendInt(aMsgId);
  *aString = ToNewUnicode(resultString);
  return NS_OK;
}

nsresult nsImapIncomingServer::ResetFoldersToUnverified(nsIMsgFolder *parentFolder)
{
  nsresult rv = NS_OK;
  if (!parentFolder) 
  {
    nsCOMPtr<nsIMsgFolder> rootFolder;
    rv = GetRootFolder(getter_AddRefs(rootFolder));
    if (NS_FAILED(rv)) return rv;
    return ResetFoldersToUnverified(rootFolder);
  }
  else 
  {
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
      && moreFolders) 
    {
      nsCOMPtr<nsISupports> child;
      rv = simpleEnumerator->GetNext(getter_AddRefs(child));
      if (NS_SUCCEEDED(rv) && child) 
      {
        nsCOMPtr<nsIMsgFolder> childFolder = do_QueryInterface(child,
          &rv);
        if (NS_SUCCEEDED(rv) && childFolder) 
        {
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
  nsCOMPtr<nsIMsgFolder> rootFolder;
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

nsresult nsImapIncomingServer::GetUnverifiedSubFolders(nsIMsgFolder *parentFolder, nsISupportsArray *aFoldersArray, PRInt32 *aNumUnverifiedFolders)
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
    nsAdapterEnumerator *simpleEnumerator = new nsAdapterEnumerator(subFolders);
    if (simpleEnumerator == nsnull)
      return NS_ERROR_OUT_OF_MEMORY;
    PRBool moreFolders;
    
    while (NS_SUCCEEDED(simpleEnumerator->HasMoreElements(&moreFolders)) && moreFolders)
    {
      nsCOMPtr<nsISupports> child;
      rv = simpleEnumerator->GetNext(getter_AddRefs(child));
      if (NS_SUCCEEDED(rv) && child) 
      {
        nsCOMPtr <nsIMsgFolder> childFolder = do_QueryInterface(child, &rv);
        if (NS_SUCCEEDED(rv) && childFolder)
        {
          rv = GetUnverifiedSubFolders(childFolder, aFoldersArray, aNumUnverifiedFolders);
          if (NS_FAILED(rv))
            break;
        }
      }
    }
    delete simpleEnumerator;
  }
  return rv;
}

NS_IMETHODIMP nsImapIncomingServer::ForgetSessionPassword()
{
  nsresult rv = nsMsgIncomingServer::ForgetSessionPassword();
  NS_ENSURE_SUCCESS(rv,rv);

  // fix for bugscape bug #15485
  // if we use turbo, and we logout, we need to make sure
  // the server doesn't think it's authenticated.
  // the biff timer continues to fire when you use turbo
  // (see #143848).  if we exited, we've set the password to null
  // but if we're authenticated, and the biff timer goes off
  // we'll still perform biff, because we use m_userAuthenticated
  // to determine if we require a password for biff.
  // (if authenticated, we don't require a password
  // see nsMsgBiffManager::PerformBiff())
  // performing biff without a password will pop up the prompt dialog
  // which is pretty wacky, when it happens after you quit the application
  m_userAuthenticated = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP nsImapIncomingServer::GetServerRequiresPasswordForBiff(PRBool *aServerRequiresPasswordForBiff)
{
  NS_ENSURE_ARG_POINTER(aServerRequiresPasswordForBiff);
  // if the user has already been authenticated, we've got the password
  *aServerRequiresPasswordForBiff = !m_userAuthenticated;
  return NS_OK;
}

NS_IMETHODIMP nsImapIncomingServer::ForgetPassword()
{
  return nsMsgIncomingServer::ForgetPassword();
}


NS_IMETHODIMP nsImapIncomingServer::PromptForPassword(char ** aPassword,
                                                      nsIMsgWindow * aMsgWindow)
{
    nsXPIDLString passwordTitle; 
    IMAPGetStringByID(IMAP_ENTER_PASSWORD_PROMPT_TITLE, getter_Copies(passwordTitle));
    nsXPIDLCString userName;
    PRBool okayValue;
    GetRealUsername(getter_Copies(userName));

    nsCAutoString promptValue(userName);

    nsCAutoString prefName;
    nsresult rv = CreatePrefNameWithRedirectorType(".hide_hostname_for_password", prefName);
    NS_ENSURE_SUCCESS(rv,rv);

    nsCOMPtr<nsIPrefBranch> prefBranch = do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv,rv);

    PRBool hideHostnameForPassword = PR_FALSE;
    rv = prefBranch->GetBoolPref(prefName.get(), &hideHostnameForPassword);
    if (NS_SUCCEEDED(rv) && hideHostnameForPassword) 
    {
      // for certain redirector types, we don't want to show the
      // hostname to the user when prompting for password
    }
    else 
    {
      nsXPIDLCString hostName;
      GetRealHostName(getter_Copies(hostName));
      promptValue.Append("@");
      promptValue.Append(hostName);
    }

    nsXPIDLString passwordText;
    rv = GetFormattedStringFromID(NS_ConvertASCIItoUCS2(promptValue).get(), IMAP_ENTER_PASSWORD_PROMPT, getter_Copies(passwordText));
    NS_ENSURE_SUCCESS(rv,rv);

    rv =  GetPasswordWithUI(passwordText, passwordTitle, aMsgWindow,
                                     &okayValue, aPassword);
    return (okayValue) ? rv : NS_MSG_PASSWORD_PROMPT_CANCELLED;
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
    do_GetService(kCImapHostSessionListCID, &rv);
  if (NS_FAILED(rv)) 
    return rv;
  
  return hostSession->CommitNamespacesForHost(this);
  
}

NS_IMETHODIMP nsImapIncomingServer::PseudoInterruptMsgLoad(nsIMsgFolder *aImapFolder, nsIMsgWindow *aMsgWindow, PRBool *interrupted)
{
  nsresult rv = NS_OK;
  nsCOMPtr<nsIImapProtocol> connection;
  
  PR_CEnterMonitor(this);
  
  // iterate through the connection cache for a connection that is loading
  // a message in this folder and should be pseudo-interrupted.
  PRUint32 cnt;
  
  rv = m_connectionCache->Count(&cnt);
  if (NS_FAILED(rv)) return rv;
  for (PRUint32 i = 0; i < cnt; i++) 
  {	
    connection = do_QueryElementAt(m_connectionCache, i);
    if (connection)
      rv = connection->PseudoInterruptMsgLoad(aImapFolder, aMsgWindow, interrupted);
  }
  
  PR_CExitMonitor(this);
  return rv;
}

NS_IMETHODIMP nsImapIncomingServer::ResetNamespaceReferences()
{

  nsCOMPtr <nsIMsgFolder> rootFolder;
  nsresult rv = GetRootFolder(getter_AddRefs(rootFolder));
  if (NS_SUCCEEDED(rv) && rootFolder)
  {
    nsCOMPtr <nsIMsgImapMailFolder> imapFolder = do_QueryInterface(rootFolder);
    if (imapFolder)
      rv = imapFolder->ResetNamespaceReferences();
  }
  return rv;
}

NS_IMETHODIMP nsImapIncomingServer::SetUserAuthenticated(PRBool aUserAuthenticated)
{
  m_userAuthenticated = aUserAuthenticated;
  if (aUserAuthenticated)
    StorePassword();
  return NS_OK;
}

NS_IMETHODIMP nsImapIncomingServer::GetUserAuthenticated(PRBool *aUserAuthenticated)
{
  NS_ENSURE_ARG_POINTER(aUserAuthenticated);
  *aUserAuthenticated = m_userAuthenticated;
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
      m_logonRedirector->RequiresPassword(userName, redirectorType.get(), &requiresPassword);
      
      if (requiresPassword)
      {
        GetPassword(getter_Copies(password));
        
        if (password.IsEmpty())
          PromptForPassword(getter_Copies(password), aMsgWindow);
        
        if (password.IsEmpty())  // if still empty then the user canceld out of the password dialog
        {
          // be sure to clear the waiting for connection info flag because we aren't waiting
          // anymore for a connection...
          m_waitingForConnectionInfo = PR_FALSE;
          return NS_OK;
        }
      }
      else
      {
        SetUserAuthenticated(PR_TRUE);  // we are already authenicated
      }
      
      nsCOMPtr<nsIPrompt> dialogPrompter;
      if (aMsgWindow)
        aMsgWindow->GetPromptDialog(getter_AddRefs(dialogPrompter));
      rv = m_logonRedirector->Logon(userName, password, redirectorType, dialogPrompter, logonRedirectorRequester, nsMsgLogonRedirectionServiceIDs::Imap);
      if (NS_FAILED(rv)) 
        return OnLogonRedirectionError(nsnull, PR_TRUE);
    }
  }
  
  return rv;
}

NS_IMETHODIMP nsImapIncomingServer::OnLogonRedirectionError(const PRUnichar *pErrMsg, PRBool badPassword)
{
  nsresult rv = NS_OK;
  
  nsXPIDLString progressString;
  GetImapStringByID(IMAP_REDIRECT_LOGIN_FAILED, getter_Copies(progressString));
  
  nsCOMPtr<nsIMsgWindow> msgWindow;
  PRUint32 urlQueueCnt = 0;
  // pull the url out of the queue so we can get the msg window, and try to rerun it.
  m_urlQueue->Count(&urlQueueCnt);
  
  nsCOMPtr<nsISupports> aSupport;
  nsCOMPtr<nsIImapUrl> aImapUrl;
  nsCOMPtr<nsIMsgMailNewsUrl> mailnewsUrl;
  if (urlQueueCnt > 0)
  {
    aSupport = getter_AddRefs(m_urlQueue->ElementAt(0));
    aImapUrl = do_QueryInterface(aSupport, &rv);
    mailnewsUrl = do_QueryInterface(aSupport, &rv);
  }

  if (mailnewsUrl)
    mailnewsUrl->GetMsgWindow(getter_AddRefs(msgWindow));
  
  // don't put up alert if no msg window - it means we're biffing.
  if (msgWindow)
    FEAlert(progressString, msgWindow);
  
  

  // If password is bad then clean up all cached passwords.
  if (badPassword)
    ForgetPassword();
  
  PRBool resetUrlState = PR_FALSE;
  if (badPassword && ++m_redirectedLogonRetries <= 3)
  {
    // this will force a reprompt for the password.
    // ### DMB TODO display error message?
    if (urlQueueCnt > 0)
    {
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
        rv = GetImapConnection(aEventQueue, aImapUrl, getter_AddRefs(protocolInstance));
        // If users cancel the login then we need to reset url state.
        if (rv == NS_BINDING_ABORTED)
          resetUrlState = PR_TRUE;
      }
    }
  }
  else
    resetUrlState = PR_TRUE;

  // Either user cancel (2nd, 3rd or 4th) login or all tries fail we'll
  // have to reset url state so that next login will work correctly.
  if  (resetUrlState)
  {
    m_redirectedLogonRetries = 0; // reset so next attempt will start at 0.
    m_waitingForConnectionInfo = PR_FALSE;
    if (urlQueueCnt > 0)
    {
      // Reset url state. 
      if (mailnewsUrl)
        mailnewsUrl->SetUrlState(PR_FALSE, NS_MSG_ERROR_URL_ABORTED);

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
    nsCOMPtr<nsIImapUrl> aImapUrl(do_QueryElementAt(m_urlQueue, 0, &rv));
    
    if (aImapUrl)
    {
      nsCOMPtr<nsISupports> aConsumer = (nsISupports*)m_urlConsumers.ElementAt(0);
      
      nsCOMPtr <nsIImapProtocol>  protocolInstance ;
      rv = GetImapConnection(aEventQueue, aImapUrl, getter_AddRefs(protocolInstance));
      m_waitingForConnectionInfo = PR_FALSE;
      if (NS_SUCCEEDED(rv) && protocolInstance)
      {
        protocolInstance->OverrideConnectionInfo(pHost, pPort, cookie.get());
        nsCOMPtr<nsIURI> url = do_QueryInterface(aImapUrl, &rv);
        if (NS_SUCCEEDED(rv) && url)
        {
          rv = protocolInstance->LoadImapUrl(url, aConsumer);
          urlRun = PR_TRUE;
        }
        
        m_urlQueue->RemoveElementAt(0);
        m_urlConsumers.RemoveElementAt(0);
      }
    }    
  }
  else
  {
    m_waitingForConnectionInfo = PR_FALSE;
    NS_ASSERTION(PR_FALSE, "got redirection response with no queued urls");
  // Need to clear this even if we don't have any urls in the queue.
  // Otherwise, we'll never clear it and we'll never request override info.
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
    
    nsCOMPtr<nsIMsgFolder> rootFolder;
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
    const char *path = uri + strlen((const char *)serverUri) + 1;

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
        case nsIImapUrl::nsImapFolderStatus:
          {
            PRInt32 folderCount = m_foldersToStat.Count();
            m_foldersToStat.RemoveObjectAt(folderCount - 1);
            if (folderCount > 1)
              m_foldersToStat[folderCount - 2]->UpdateStatus(this, nsnull);
          }
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
nsImapIncomingServer::AddTo(const char *aName, PRBool addAsSubscribed, PRBool aSubscribable, PRBool changeIfExists)
{
  nsresult rv = EnsureInner();
  NS_ENSURE_SUCCESS(rv,rv);
  
  // quick check if the name we are passed is really modified UTF-7
  // if it isn't, ignore it.  (otherwise, we'll crash.  see #63186)
  // there is a bug in the UW IMAP server where it can send us
  // folder names as literals, instead of MUTF7
  unsigned char *ptr = (unsigned char *)aName;
  PRBool nameIsClean = PR_TRUE;
  while (*ptr) 
  {
    if (*ptr > 127) 
    {
      nameIsClean = PR_FALSE;
      break;
    }
    ptr++;
  }
  
  NS_ASSERTION(nameIsClean,"folder path was not in UTF7, ignore it");
  if (!nameIsClean) return NS_OK;
  
  return mInner->AddTo(aName, addAsSubscribed, aSubscribable, changeIfExists);
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
  m_subscribeFolders.Clear();
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
  return SubscribeToFolder(aName, PR_TRUE, nsnull);
}

NS_IMETHODIMP
nsImapIncomingServer::Unsubscribe(const PRUnichar *aName)
{
  return SubscribeToFolder(aName, PR_FALSE, nsnull);
}

NS_IMETHODIMP
nsImapIncomingServer::SubscribeToFolder(const PRUnichar *aName, PRBool subscribe, nsIURI **aUri)
{
  nsresult rv;
  nsCOMPtr<nsIImapService> imapService = do_GetService(kImapServiceCID, &rv);
  if (NS_FAILED(rv)) return rv;
  if (!imapService) return NS_ERROR_FAILURE;
  
  nsCOMPtr<nsIMsgFolder> rootMsgFolder;
  rv = GetRootFolder(getter_AddRefs(rootMsgFolder));
  if (NS_FAILED(rv)) return rv;
  if (!rootMsgFolder) return NS_ERROR_FAILURE;
  
  // Locate the folder so that the correct hierarchical delimiter is used in the
  // folder pathnames, otherwise root's (ie, '^') is used and this is wrong.
  
  // aName is not a genuine UTF-16 but just a zero-padded modified UTF-7 
  NS_LossyConvertUTF16toASCII folderCName(aName);
  nsCOMPtr<nsIMsgFolder> msgFolder;
  if (rootMsgFolder && aName && (*aName))
  {
    rv = rootMsgFolder->FindSubFolder(folderCName, getter_AddRefs(msgFolder));
  }
  
  nsCOMPtr<nsIEventQueue> queue;
  // get the Event Queue for this thread...
  nsCOMPtr<nsIEventQueueService> pEventQService = 
    do_GetService(kEventQueueServiceCID, &rv);
  if (NS_FAILED(rv)) return rv;
  
  rv = pEventQService->GetThreadEventQueue(NS_CURRENT_THREAD, getter_AddRefs(queue));
  if (NS_FAILED(rv)) return rv;

  nsAutoString unicodeName;
  rv = CopyMUTF7toUTF16(folderCName, unicodeName);
  NS_ENSURE_SUCCESS(rv, rv);

  if (subscribe)
    rv = imapService->SubscribeFolder(queue, msgFolder, unicodeName.get(), nsnull, aUri);
  else 
    rv = imapService->UnsubscribeFolder(queue, msgFolder, unicodeName.get(), nsnull, nsnull);
  
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
nsImapIncomingServer::IsSubscribable(const char *path, PRBool *aIsSubscribable)
{
    nsresult rv = EnsureInner();
    NS_ENSURE_SUCCESS(rv,rv);
    return mInner->IsSubscribable(path, aIsSubscribable);
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
nsImapIncomingServer::GetCanCompactFoldersOnServer(PRBool *canCompactFoldersOnServer)
{
    NS_ENSURE_ARG_POINTER(canCompactFoldersOnServer);

    // Initialize canCompactFoldersOnServer true, a default value for IMAP
    *canCompactFoldersOnServer = PR_TRUE;

    GetPrefForServerAttribute("canCompactFoldersOnServer", canCompactFoldersOnServer);

    return NS_OK;
}

NS_IMETHODIMP
nsImapIncomingServer::GetCanUndoDeleteOnServer(PRBool *canUndoDeleteOnServer)
{
    NS_ENSURE_ARG_POINTER(canUndoDeleteOnServer);

    // Initialize canUndoDeleteOnServer true, a default value for IMAP
    *canUndoDeleteOnServer = PR_TRUE;

    GetPrefForServerAttribute("canUndoDeleteOnServer", canUndoDeleteOnServer);

    return NS_OK;
}

NS_IMETHODIMP
nsImapIncomingServer::GetCanSearchMessages(PRBool *canSearchMessages)
{
    NS_ENSURE_ARG_POINTER(canSearchMessages);

    // Initialize canSearchMessages true, a default value for IMAP
    *canSearchMessages = PR_TRUE;

    GetPrefForServerAttribute("canSearchMessages", canSearchMessages);

    return NS_OK;
}

NS_IMETHODIMP
nsImapIncomingServer::GetCanEmptyTrashOnExit(PRBool *canEmptyTrashOnExit)
{
    NS_ENSURE_ARG_POINTER(canEmptyTrashOnExit);

    // Initialize canEmptyTrashOnExit true, a default value for IMAP
    *canEmptyTrashOnExit = PR_TRUE;

    GetPrefForServerAttribute("canEmptyTrashOnExit", canEmptyTrashOnExit);

    return NS_OK;
}

NS_IMETHODIMP
nsImapIncomingServer::GetIsSecureServer(PRBool *isSecureServer)
{
    NS_ENSURE_ARG_POINTER(isSecureServer);

    // Initialize isSecureServer true, a default value for IMAP
    *isSecureServer = PR_TRUE;

    GetPrefForServerAttribute("isSecureServer", isSecureServer);

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

  nsCOMPtr<nsIPrefBranch> prefBranch = do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
  if (NS_SUCCEEDED(rv) && prefBranch) {
     rv = prefBranch->GetBoolPref(prefName.get(), aSupportsDiskSpace);
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
  
  rv = m_connectionCache->Count(&cnt);
  if (NS_FAILED(rv)) return rv;
  // loop counting idle connections
  for (PRUint32 i = 0; i < cnt; i++) 
  {
    connection = do_QueryElementAt(m_connectionCache, i);
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

    // Initialize aCanCreateFoldersOnServer true, a default value for IMAP
    *aCanCreateFoldersOnServer = PR_TRUE;

    GetPrefForServerAttribute("canCreateFolders", aCanCreateFoldersOnServer);

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

    nsCOMPtr<nsIPrefBranch> prefBranch = do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv) && prefBranch) {
      rv = prefBranch->GetIntPref(prefName.get(), aSupportLevel);
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
    constructedPrettyName.AppendLiteral("@");
    constructedPrettyName.AppendWithConversion(hostName);

    // If the port is valid and not default, add port value to the pretty name
    if ((serverPort > 0) && (!isItDefaultPort)) {
        constructedPrettyName.AppendLiteral(":");
        constructedPrettyName.AppendInt(serverPort);
    }

    // Format the pretty name
    rv = GetFormattedStringFromID(constructedPrettyName.get(), IMAP_DEFAULT_ACCOUNT_NAME, aPrettyName);
    NS_ENSURE_SUCCESS(rv, rv);

    return rv;
}

nsresult
nsImapIncomingServer::GetFormattedStringFromID(const PRUnichar *aValue, PRInt32 aID, PRUnichar **aResult)
{
    NS_ENSURE_ARG_POINTER(aResult);

    nsresult rv;
    rv = GetStringBundle();
    if (m_stringBundle)
    {
        const PRUnichar *formatStrings[] =
        {
            aValue,
        };
        rv = m_stringBundle->FormatStringFromID(aID,
                                    formatStrings, 1,
                                    aResult);
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

nsresult
nsImapIncomingServer::GetPrefForServerAttribute(const char *prefSuffix, PRBool *prefValue)
{
  NS_ENSURE_ARG_POINTER(prefSuffix);
  nsresult rv;
  nsCAutoString prefName;
  nsCOMPtr<nsIPrefBranch> prefBranch = do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);

  nsXPIDLCString serverKey;
  rv = GetKey(getter_Copies(serverKey));

  // Time to check if this server has the pref 
  // (mail.server.<serverkey>.prefSuffix) already set
  nsMsgIncomingServer::getPrefName(serverKey, 
                                   prefSuffix, 
                                   prefName);
  rv = prefBranch->GetBoolPref(prefName.get(), prefValue);

  // If the server pref is not set in then look at the 
  // pref set with redirector type
  if (NS_FAILED(rv)) 
  {
    nsCAutoString redirectorType;
    redirectorType.Assign(".");
    redirectorType.Append(prefSuffix);

    rv = CreatePrefNameWithRedirectorType(redirectorType.get(), prefName);

    if (NS_SUCCEEDED(rv)) 
      rv = prefBranch->GetBoolPref(prefName.get(), prefValue);
  }

  return rv;
}

NS_IMETHODIMP
nsImapIncomingServer::GetCanFileMessagesOnServer(PRBool *aCanFileMessagesOnServer)
{
    NS_ENSURE_ARG_POINTER(aCanFileMessagesOnServer);

    // Initialize aCanFileMessagesOnServer true, a default value for IMAP
    *aCanFileMessagesOnServer = PR_TRUE;

    GetPrefForServerAttribute("canFileMessages", aCanFileMessagesOnServer);

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

// This is a recursive function. It gets new messages for current folder
// first if it is marked, then calls itself recursively for each subfolder.
NS_IMETHODIMP 
nsImapIncomingServer::GetNewMessagesForNonInboxFolders(nsIMsgFolder *aFolder,
                                                       nsIMsgWindow *aWindow,
                                                       PRBool forceAllFolders,
                                                       PRBool performingBiff)
{
  nsresult retval = NS_OK;
  static PRBool gGotStatusPref = PR_FALSE;
  static PRBool gUseStatus = PR_FALSE;
  if (!aFolder)
    return retval;

  PRBool isServer;
  (void) aFolder->GetIsServer(&isServer);
  // Check this folder for new messages if it is marked to be checked
  // or if we are forced to check all folders
  PRUint32 flags = 0;
  aFolder->GetFlags(&flags);
  if ((forceAllFolders && 
    !(flags & (MSG_FOLDER_FLAG_INBOX | MSG_FOLDER_FLAG_TRASH | MSG_FOLDER_FLAG_JUNK | MSG_FOLDER_FLAG_IMAP_NOSELECT)))
    || (flags & MSG_FOLDER_FLAG_CHECK_NEW))
  {
    // Get new messages for this folder.
    aFolder->SetGettingNewMessages(PR_TRUE);
    if (performingBiff)
    {
      nsresult rv;
      nsCOMPtr<nsIMsgImapMailFolder> imapFolder = do_QueryInterface(aFolder, &rv);
      if (imapFolder)
        imapFolder->SetPerformingBiff(PR_TRUE);
    }
    PRBool isOpen = PR_FALSE;
    nsCOMPtr <nsIMsgMailSession> mailSession = do_GetService(NS_MSGMAILSESSION_CONTRACTID);
    if (mailSession && aFolder)
      mailSession->IsFolderOpenInWindow(aFolder, &isOpen);
    // eventually, the gGotStatusPref should go away, once we work out the kinks
    // from using STATUS.
    if (!gGotStatusPref)
    {
      nsCOMPtr<nsIPrefBranch> prefBranch = do_GetService(NS_PREFSERVICE_CONTRACTID);
      if(prefBranch)  
        prefBranch->GetBoolPref("mail.imap.use_status_for_biff", &gUseStatus);
      gGotStatusPref = PR_TRUE;
    }
    if (gUseStatus && !isOpen)
    {
      nsCOMPtr <nsIMsgImapMailFolder> imapFolder = do_QueryInterface(aFolder);
      if (imapFolder && !isServer)
        m_foldersToStat.AppendObject(imapFolder);
        //imapFolder->UpdateStatus(this, nsnull /* aWindow - null window will prevent alerts */);
    }
    else
    {
      aFolder->UpdateFolder(aWindow);
    }
  }

  // Loop through all subfolders to get new messages for them.
  nsCOMPtr<nsIEnumerator> aEnumerator;
  retval = aFolder->GetSubFolders(getter_AddRefs(aEnumerator));
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

    retval = GetNewMessagesForNonInboxFolders(msgFolder, aWindow, forceAllFolders, performingBiff);

    more = aEnumerator->Next();
  }

  if (isServer)
  {
    PRInt32 folderCount = m_foldersToStat.Count();
    if (folderCount > 0)
      m_foldersToStat[folderCount - 1]->UpdateStatus(this, nsnull);
  }
  return retval;
}

NS_IMETHODIMP
nsImapIncomingServer::GetArbitraryHeaders(char **aResult)
{
  nsCOMPtr <nsIMsgFilterList> filterList;  
  nsresult rv = GetFilterList(nsnull, getter_AddRefs(filterList));
  NS_ENSURE_SUCCESS(rv,rv);

  rv = filterList->GetArbitraryHeaders(aResult);
  return rv;
}

NS_IMETHODIMP
nsImapIncomingServer::GetShowAttachmentsInline(PRBool *aResult)
{
  *aResult = PR_TRUE; // true per default
 
  nsresult rv; 
  nsCOMPtr<nsIPrefBranch> prefBranch = do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv,rv);
  
  prefBranch->GetBoolPref("mail.inline_attachments", aResult);
  return NS_OK; // In case this pref is not set we need to return NS_OK.
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
  nsCOMPtr<nsIImapHostSessionList> hostSessionList = do_GetService(kCImapHostSessionListCID, &rv);
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

// use canonical format in originalUri & convertedUri
NS_IMETHODIMP
nsImapIncomingServer::GetUriWithNamespacePrefixIfNecessary(PRInt32 namespaceType,
                                                           const char *originalUri,
                                                           char **convertedUri)
{
  NS_ENSURE_ARG_POINTER(convertedUri);
  *convertedUri = nsnull;

  nsresult rv = NS_OK;
  nsXPIDLCString serverKey;
  rv = GetKey(getter_Copies(serverKey));
  if (NS_FAILED(rv)) return rv;
  nsCOMPtr<nsIImapHostSessionList> hostSessionList = do_GetService(kCImapHostSessionListCID, &rv);
  nsIMAPNamespace *ns = nsnull;
  rv = hostSessionList->GetDefaultNamespaceOfTypeForHost(serverKey, (EIMAPNamespaceType)namespaceType, ns);
  if (ns)
  {
    nsCAutoString namespacePrefix(ns->GetPrefix());
    if (!namespacePrefix.IsEmpty())
    {
      // check if namespacePrefix is the same as the online directory; if so, ignore it.

      nsXPIDLCString onlineDir;
      rv = GetServerDirectory(getter_Copies(onlineDir));
      NS_ENSURE_SUCCESS(rv, rv);
      if (!onlineDir.IsEmpty())
      {
        char delimiter = ns->GetDelimiter();
          if ( onlineDir.Last() != delimiter )
            onlineDir += delimiter;
          if (onlineDir.Equals(namespacePrefix))
            return NS_OK;
      }

      namespacePrefix.ReplaceChar(ns->GetDelimiter(), '/'); // use canonical format
      nsCAutoString resultUri(originalUri);
      PRInt32 index = resultUri.Find("//");           // find scheme
      index = resultUri.Find("/", PR_FALSE, index+2); // find '/' after scheme
      if (resultUri.Find(namespacePrefix.get(), PR_FALSE, index+1) != index+1) // Necessary to insert namespace prefix
        resultUri.Insert(namespacePrefix, index+1);   // insert namespace prefix
      *convertedUri = nsCRT::strdup(resultUri.get());
    }
  }
  return rv;
}

NS_IMETHODIMP nsImapIncomingServer::GetTrashFolderName(PRUnichar **retval)
{
  nsresult rv = GetUnicharValue(PREF_TRASH_FOLDER_NAME, retval);
  if (NS_FAILED(rv))
    return rv;

  if (!*retval || !**retval)
  {
      // if GetUnicharValue() above returned allocated empty string, we must free it first
      // before allocating space and assigning the default value
      if (*retval)
          nsMemory::Free(*retval);
      *retval = ToNewUnicode(NS_LITERAL_STRING(DEFAULT_TRASH_FOLDER_NAME));
  }
  return NS_OK;
}

NS_IMETHODIMP nsImapIncomingServer::SetTrashFolderName(const PRUnichar *chvalue)
{
  // clear trash flag from the old pref
  nsXPIDLString oldTrashName;
  nsresult rv = GetTrashFolderName(getter_Copies(oldTrashName));
  if (NS_SUCCEEDED(rv))
  {
    nsCAutoString oldTrashNameUtf7;
    rv = CopyUTF16toMUTF7(oldTrashName, oldTrashNameUtf7);
    if (NS_SUCCEEDED(rv))
    {
      nsCOMPtr<nsIMsgFolder> oldFolder;
      rv = GetFolder(oldTrashNameUtf7.get(), getter_AddRefs(oldFolder));
      if (NS_SUCCEEDED(rv) && oldFolder)
      {
        oldFolder->ClearFlag(MSG_FOLDER_FLAG_TRASH);
      }
    }
  }
  
  return SetUnicharValue(PREF_TRASH_FOLDER_NAME, chvalue);
}

NS_IMETHODIMP
nsImapIncomingServer::GetMsgFolderFromURI(nsIMsgFolder *aFolderResource, const char *aURI, nsIMsgFolder **aFolder)
{
  nsCOMPtr<nsIMsgFolder> rootMsgFolder;
  nsresult rv = GetRootMsgFolder(getter_AddRefs(rootMsgFolder));
  NS_ENSURE_SUCCESS(rv, rv);
  if (!rootMsgFolder)
    return NS_ERROR_UNEXPECTED;

  nsCOMPtr <nsIMsgFolder> msgFolder;
  PRBool namespacePrefixAdded = PR_FALSE;

  // Make sure an specific IMAP folder has correct personal namespace
  // See bugzilla bug 90494 (http://bugzilla.mozilla.org/show_bug.cgi?id=90494)
  nsXPIDLCString folderUriWithNamespace;
  GetUriWithNamespacePrefixIfNecessary(kPersonalNamespace, aURI, getter_Copies(folderUriWithNamespace));
  if (!folderUriWithNamespace.IsEmpty()) {
    namespacePrefixAdded = PR_TRUE;
    rv = rootMsgFolder->GetChildWithURI(folderUriWithNamespace, PR_TRUE, PR_FALSE, getter_AddRefs(msgFolder));
  }
  else {
    rv = rootMsgFolder->GetChildWithURI(aURI, PR_TRUE, PR_FALSE, getter_AddRefs(msgFolder));
  }

  if (NS_FAILED(rv) || !msgFolder) {
    // we didn't find the folder so we will have to create new one.
    if (namespacePrefixAdded)
    {
      nsCOMPtr <nsIRDFService> rdf = do_GetService("@mozilla.org/rdf/rdf-service;1", &rv);
      NS_ENSURE_SUCCESS(rv,rv);

      nsCOMPtr<nsIRDFResource> resource;
      rv = rdf->GetResource(folderUriWithNamespace, getter_AddRefs(resource));
      NS_ENSURE_SUCCESS(rv,rv);
      
      nsCOMPtr <nsIMsgFolder> folderResource;
      folderResource = do_QueryInterface(resource, &rv);
      NS_ENSURE_SUCCESS(rv,rv);
      msgFolder = folderResource;
    }
    else
      msgFolder = aFolderResource;
  }

  NS_IF_ADDREF(*aFolder = msgFolder);
  return NS_OK;
}

NS_IMETHODIMP
nsImapIncomingServer::CramMD5Hash(const char *decodedChallenge, const char *key, char **result)
{
  unsigned char resultDigest[DIGEST_LENGTH];
  nsresult rv = MSGCramMD5(decodedChallenge, strlen(decodedChallenge), 
        key, strlen(key), resultDigest);
  NS_ENSURE_SUCCESS(rv, rv);
  *result = (char *) malloc(DIGEST_LENGTH);
  if (*result)
    memcpy(*result, resultDigest, DIGEST_LENGTH);
  return (*result) ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

