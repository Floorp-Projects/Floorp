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
 *   Alec Flett <alecf@netscape.com>
 *   Seth Spitzer <sspitzer@netscape.com>
 *   Bhuvan Racham <racham@netscape.com>
 *   David Bienvenu <bienvenu@mozilla.org>
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

/*
 * The account manager service - manages all accounts, servers, and identities
 */

#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsISupportsArray.h"
#include "nsMsgAccountManager.h"
#include "nsMsgBaseCID.h"
#include "nsMsgCompCID.h"
#include "nsMsgDBCID.h"
#include "prmem.h"
#include "prcmon.h"
#include "prthread.h"
#include "plstr.h"
#include "nsString.h"
#include "nsXPIDLString.h"
#include "nsUnicharUtils.h"
#include "nscore.h"
#include "nsCRT.h"  // for nsCRT::strtok
#include "prprf.h"
#include "nsIMsgFolderCache.h"
#include "nsFileStream.h"
#include "nsIFileStreams.h"
#include "nsMsgUtils.h"
#include "nsIFileSpec.h" 
#include "nsILocalFile.h"
#include "nsIURL.h"
#include "nsNetCID.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsISmtpService.h"
#include "nsIMsgBiffManager.h"
#include "nsIMsgPurgeService.h"
#include "nsIObserverService.h"
#include "nsIMsgMailSession.h"
#include "nsIEventQueueService.h"
#include "nsIDirectoryService.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsMsgFolderFlags.h"
#include "nsIRDFService.h"
#include "nsRDFCID.h"
#include "nsIImapIncomingServer.h" 
#include "nsIImapUrl.h"
#include "nsIMessengerOSIntegration.h"
#include "nsICategoryManager.h"
#include "nsISupportsPrimitives.h"
#include "nsMsgFilterService.h"
#include "nsIMsgFilter.h"
#include "nsIMsgSearchSession.h"
#include "nsIDBChangeListener.h"
#include "nsIDBFolderInfo.h"
#include "nsIMsgHdr.h"
#include "nsILineInputStream.h"
#include "nsNetUtil.h"
#define PREF_MAIL_ACCOUNTMANAGER_ACCOUNTS "mail.accountmanager.accounts"
#define PREF_MAIL_ACCOUNTMANAGER_DEFAULTACCOUNT "mail.accountmanager.defaultaccount"
#define PREF_MAIL_ACCOUNTMANAGER_LOCALFOLDERSSERVER "mail.accountmanager.localfoldersserver"
#define PREF_MAIL_SERVER_PREFIX "mail.server."
#define ACCOUNT_PREFIX "account"
#define SERVER_PREFIX "server"
#define ID_PREFIX "id"
#define ABOUT_TO_GO_OFFLINE_TOPIC "network:offline-about-to-go-offline"
#define ACCOUNT_DELIMITER ","
#define APPEND_ACCOUNTS_VERSION_PREF_NAME "append_preconfig_accounts.version"
#define MAILNEWS_ROOT_PREF "mailnews."
#define PREF_MAIL_ACCOUNTMANAGER_APPEND_ACCOUNTS "mail.accountmanager.appendaccounts"

static NS_DEFINE_CID(kMsgAccountCID, NS_MSGACCOUNT_CID);
static NS_DEFINE_CID(kMsgFolderCacheCID, NS_MSGFOLDERCACHE_CID);
static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);

// use this to search for all servers with the given hostname/iid and
// put them in "servers"
typedef struct _findServerEntry {
  const char *hostname;
  const char *username;
  const char *type;
  PRBool useRealSetting;
  nsIMsgIncomingServer *server;
} findServerEntry;

typedef struct _findServerByKeyEntry {
  const char *key;
  PRInt32 index;
} findServerByKeyEntry;

// use this to search for all servers that match "server" and
// put all identities in "identities"
typedef struct _findIdentitiesByServerEntry {
  nsISupportsArray *identities;
  nsIMsgIncomingServer *server;
} findIdentitiesByServerEntry;

typedef struct _findServersByIdentityEntry {
  nsISupportsArray *servers;
  nsIMsgIdentity *identity;
} findServersByIdentityEntry;

typedef struct _findAccountByKeyEntry {
    const char* key;
    nsIMsgAccount* account;
} findAccountByKeyEntry;


NS_IMPL_THREADSAFE_ISUPPORTS5(nsMsgAccountManager,
                              nsIMsgAccountManager,
                              nsIObserver,
                              nsISupportsWeakReference,
                              nsIUrlListener,
                              nsIFolderListener)

nsMsgAccountManager::nsMsgAccountManager() :
  m_accountsLoaded(PR_FALSE),
  m_emptyTrashInProgress(PR_FALSE),
  m_cleanupInboxInProgress(PR_FALSE),
  m_haveShutdown(PR_FALSE),
  m_shutdownInProgress(PR_FALSE),
  m_userAuthenticated(PR_FALSE)
{
}

nsMsgAccountManager::~nsMsgAccountManager()
{
  nsresult rv;

  if(!m_haveShutdown)
  {
    Shutdown();
    //Don't remove from Observer service in Shutdown because Shutdown also gets called
    //from xpcom shutdown observer.  And we don't want to remove from the service in that case.
    nsCOMPtr<nsIObserverService> observerService = 
	     do_GetService("@mozilla.org/observer-service;1", &rv);
    if (NS_SUCCEEDED(rv))
    {    
      observerService->RemoveObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID);
      observerService->RemoveObserver(this, ABOUT_TO_GO_OFFLINE_TOPIC);
    }
  }
}

nsresult nsMsgAccountManager::Init()
{
  nsresult rv;

  rv = NS_NewISupportsArray(getter_AddRefs(m_accounts));
  if(NS_FAILED(rv)) return rv;

  rv = NS_NewISupportsArray(getter_AddRefs(mFolderListeners));

  nsCOMPtr<nsIObserverService> observerService = 
           do_GetService("@mozilla.org/observer-service;1", &rv);
  if (NS_SUCCEEDED(rv))
  {    
    observerService->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, PR_TRUE);
    observerService->AddObserver(this, "quit-application" , PR_TRUE);
    observerService->AddObserver(this, ABOUT_TO_GO_OFFLINE_TOPIC, PR_TRUE);
    observerService->AddObserver(this, "session-logout", PR_TRUE);
    observerService->AddObserver(this, "profile-before-change", PR_TRUE);
  }

  return NS_OK;
}

nsresult nsMsgAccountManager::Shutdown()
{
  if (m_haveShutdown)     // do not shutdown twice
    return NS_OK;

  nsresult rv;

  SaveVirtualFolders();
	
  nsCOMPtr<nsIMsgDBService> msgDBService = do_GetService(NS_MSGDB_SERVICE_CONTRACTID, &rv);
  if (msgDBService)
  {
    PRInt32 numVFListeners = m_virtualFolderListeners.Count();
    for(PRInt32 i = 0; i < numVFListeners; i++)
      msgDBService->UnregisterPendingListener(m_virtualFolderListeners[i]);
  }
  if(m_msgFolderCache)
    WriteToFolderCache(m_msgFolderCache);

  (void)ShutdownServers();
  (void)UnloadAccounts();
  
  //shutdown removes nsIIncomingServer listener from biff manager, so do it after accounts have been unloaded
  nsCOMPtr<nsIMsgBiffManager> biffService = do_GetService(NS_MSGBIFFMANAGER_CONTRACTID, &rv);
  if (NS_SUCCEEDED(rv) && biffService)
    biffService->Shutdown();

  //shutdown removes nsIIncomingServer listener from purge service, so do it after accounts have been unloaded
  nsCOMPtr<nsIMsgPurgeService> purgeService = do_GetService(NS_MSGPURGESERVICE_CONTRACTID, &rv);
  if (NS_SUCCEEDED(rv) && purgeService)
    purgeService->Shutdown();
  
  m_msgFolderCache = nsnull;

  m_haveShutdown = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP
nsMsgAccountManager::GetShutdownInProgress(PRBool *_retval)
{
    NS_ENSURE_ARG_POINTER(_retval);
    *_retval = m_shutdownInProgress;
    return NS_OK;
}

NS_IMETHODIMP
nsMsgAccountManager::GetUserNeedsToAuthenticate(PRBool *aRetval)
{
  NS_ENSURE_ARG_POINTER(aRetval);
  if (!m_userAuthenticated)
    return m_prefs->GetBoolPref("mail.password_protect_local_cache", aRetval);
  *aRetval = !m_userAuthenticated;
  return NS_OK;
}

NS_IMETHODIMP
nsMsgAccountManager::SetUserNeedsToAuthenticate(PRBool aUserNeedsToAuthenticate)
{
    m_userAuthenticated = !aUserNeedsToAuthenticate;
    return NS_OK;
}

NS_IMETHODIMP nsMsgAccountManager::Observe(nsISupports *aSubject, const char *aTopic, const PRUnichar *someData)
{
  if(!nsCRT::strcmp(aTopic,NS_XPCOM_SHUTDOWN_OBSERVER_ID))
  {
    Shutdown();
    return NS_OK;
  }
  
  if (!nsCRT::strcmp(aTopic,"quit-application"))
  {
    m_shutdownInProgress = PR_TRUE;
    return NS_OK;
  }
  
  if (!nsCRT::strcmp(aTopic, ABOUT_TO_GO_OFFLINE_TOPIC))
  {
    nsAutoString dataString(NS_LITERAL_STRING("offline"));
    if (someData)
    {
      nsAutoString someDataString(someData);
      if (dataString == someDataString)
        CloseCachedConnections();
    }
    return NS_OK;
  }
  
  if (!nsCRT::strcmp(aTopic, "session-logout"))
  {
    m_incomingServers.Enumerate(hashLogoutOfServer, nsnull);
    return NS_OK;
  }
  
  if (!nsCRT::strcmp(aTopic, "profile-before-change"))
  {
    Shutdown();
    return NS_OK;
  }
	
 return NS_OK;
}

nsresult
nsMsgAccountManager::getPrefService()
{

  // get the prefs service
  nsresult rv = NS_OK;
  
  if (!m_prefs)
    m_prefs = do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);

  if (NS_FAILED(rv)) return rv;
  /* m_prefs is good now */
  return NS_OK;
}

void
nsMsgAccountManager::getUniqueKey(const char* prefix,
                                  nsHashtable *hashTable,
                                  nsCString& aResult)
{
  PRInt32 i=1;
  PRBool unique=PR_FALSE;

  do {
    aResult=prefix;
    aResult.AppendInt(i++);
    nsCStringKey hashKey(aResult);
    void* hashElement = hashTable->Get(&hashKey);
    
    if (!hashElement) unique=PR_TRUE;
  } while (!unique);

}

void
nsMsgAccountManager::getUniqueAccountKey(const char *prefix,
                                         nsISupportsArray *accounts,
                                         nsCString& aResult)
{
  PRInt32 i=1;
  PRBool unique = PR_FALSE;
  
  findAccountByKeyEntry findEntry;
  findEntry.account = nsnull;
  
  do {
      aResult = prefix;
      aResult.AppendInt(i++);
      findEntry.key = aResult.get();
    
    accounts->EnumerateForwards(findAccountByKey, (void *)&findEntry);

    if (!findEntry.account) unique=PR_TRUE;
    findEntry.account = nsnull;
  } while (!unique);

}

nsresult
nsMsgAccountManager::CreateIdentity(nsIMsgIdentity **_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  
  nsresult rv;

  nsCAutoString key;
  getUniqueKey(ID_PREFIX, &m_identities, key);

  rv = createKeyedIdentity(key.get(), _retval);

  return rv;
}

nsresult
nsMsgAccountManager::GetIdentity(const char* key,
                                 nsIMsgIdentity **_retval)
{
  if (!_retval) return NS_ERROR_NULL_POINTER;
  // null or empty key does not return an identity!
  if (!key || !key[0]) {
    *_retval = nsnull;
    return NS_OK;
  }

  nsresult rv;
  // check for the identity in the hash table
  nsCStringKey hashKey(key);
  nsISupports *idsupports = (nsISupports*)m_identities.Get(&hashKey);
  nsCOMPtr<nsIMsgIdentity> identity = do_QueryInterface(idsupports, &rv);

  if (NS_SUCCEEDED(rv)) {
    *_retval = identity;
    NS_ADDREF(*_retval);
    return NS_OK;
  }

  // identity doesn't exist. create it.
  rv = createKeyedIdentity(key, _retval);

  return rv;
}

/*
 * the shared identity-creation code
 * create an identity and add it to the accountmanager's list.
 */
nsresult
nsMsgAccountManager::createKeyedIdentity(const char* key,
                                         nsIMsgIdentity ** aIdentity)
{
  nsresult rv;
  nsCOMPtr<nsIMsgIdentity> identity;
  rv = nsComponentManager::CreateInstance(NS_MSGIDENTITY_CONTRACTID,
                                          nsnull,
                                          NS_GET_IID(nsIMsgIdentity),
                                          getter_AddRefs(identity));
  if (NS_FAILED(rv)) return rv;
  
  identity->SetKey(key);
  
  nsCStringKey hashKey(key);

  // addref for the hash table`
  nsISupports* idsupports = identity;
  NS_ADDREF(idsupports);
  m_identities.Put(&hashKey, (void *)idsupports);

  *aIdentity = identity;
  NS_ADDREF(*aIdentity);
  
  return NS_OK;
}

nsresult
nsMsgAccountManager::CreateIncomingServer(const char* username,
                                          const char* hostname,
                                          const char* type,
                                          nsIMsgIncomingServer **_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  nsresult rv = LoadAccounts();
  if (NS_FAILED(rv)) return rv;

  nsCAutoString key;
  getUniqueKey(SERVER_PREFIX, &m_incomingServers, key);
  return createKeyedServer(key.get(), username, hostname, type, _retval);
}

nsresult
nsMsgAccountManager::GetIncomingServer(const char* key,
                                       nsIMsgIncomingServer **_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  nsresult rv=NS_OK;
  
  nsCStringKey hashKey(key);
  nsCOMPtr<nsIMsgIncomingServer> server =
    do_QueryInterface((nsISupports*)m_incomingServers.Get(&hashKey), &rv);

  if (NS_SUCCEEDED(rv)) {
    NS_ADDREF(*_retval = server);
    return NS_OK;
  }

  // server doesn't exist, so create it
  // this is really horrible because we are doing our own prefname munging
  // instead of leaving it up to the incoming server.
  // this should be fixed somehow so that we can create the incoming server
  // and then read from the incoming server's attributes
  
  // in order to create the right kind of server, we have to look
  // at the pref for this server to get the username, hostname, and type
  nsCAutoString serverPrefPrefix(PREF_MAIL_SERVER_PREFIX);
  serverPrefPrefix += key;
  
  nsCAutoString serverPref;

  //
  // .type
  serverPref = serverPrefPrefix;
  serverPref += ".type";
  nsXPIDLCString serverType;
  rv = m_prefs->GetCharPref(serverPref.get(), getter_Copies(serverType));
  NS_ENSURE_SUCCESS(rv, NS_ERROR_NOT_INITIALIZED);
  
  //
  // .userName
  serverPref = serverPrefPrefix;
  serverPref += ".userName";
  nsXPIDLCString username;
  rv = m_prefs->GetCharPref(serverPref.get(), getter_Copies(username));

  // .hostname
  serverPref = serverPrefPrefix;
  serverPref += ".hostname";
  nsXPIDLCString hostname;
  rv = m_prefs->GetCharPref(serverPref.get(), getter_Copies(hostname));
  NS_ENSURE_SUCCESS(rv, NS_ERROR_NOT_INITIALIZED);
  
    // the server type doesn't exist. That's bad.

  rv = createKeyedServer(key, username, hostname, serverType, _retval);
  NS_ENSURE_SUCCESS(rv, rv);

  return rv;
}

/*
 * create a server when you know the key and the type
 */
nsresult
nsMsgAccountManager::createKeyedServer(const char* key,
                                       const char* username,
                                       const char* hostname,
                                       const char* type,
                                       nsIMsgIncomingServer ** aServer)
{
  nsresult rv;

  //construct the contractid
  nsCAutoString serverContractID(NS_MSGINCOMINGSERVER_CONTRACTID_PREFIX);
  serverContractID += type;
  
  // finally, create the server
  nsCOMPtr<nsIMsgIncomingServer> server =
           do_CreateInstance(serverContractID.get(), &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  
  server->SetKey(key);
  server->SetType(type);
  server->SetUsername(username);
  server->SetHostName(hostname);

  nsCStringKey hashKey(key);

  // addref for the hashtable
  nsISupports* serversupports = server;
  NS_ADDREF(serversupports);
  m_incomingServers.Put(&hashKey, serversupports);

  // now add all listeners that are supposed to be
  // waiting on root folders
  nsCOMPtr<nsIMsgFolder> rootFolder;
  rv = server->GetRootFolder(getter_AddRefs(rootFolder));
  NS_ENSURE_SUCCESS(rv, rv);
  mFolderListeners->EnumerateForwards(addListenerToFolder,
                                      (void *)(nsIMsgFolder*)rootFolder);
  NS_ADDREF(*aServer = server);
  
  return NS_OK;
}

PRBool
nsMsgAccountManager::addListenerToFolder(nsISupports *element, void *data)
{
  nsresult rv;
  nsIMsgFolder *rootFolder = (nsIMsgFolder *)data;
  nsCOMPtr<nsIFolderListener> listener = do_QueryInterface(element, &rv);
  NS_ENSURE_SUCCESS(rv, PR_TRUE);

  rootFolder->AddFolderListener(listener);
  return PR_TRUE;
}

PRBool
nsMsgAccountManager::removeListenerFromFolder(nsISupports *element, void *data)
{
  nsresult rv;
  nsIMsgFolder *rootFolder = (nsIMsgFolder *)data;
  nsCOMPtr<nsIFolderListener> listener = do_QueryInterface(element, &rv);
  NS_ENSURE_SUCCESS(rv, PR_TRUE);

  rootFolder->RemoveFolderListener(listener);
  return PR_TRUE;
}

NS_IMETHODIMP
nsMsgAccountManager::DuplicateAccount(nsIMsgAccount *aAccount)
{ 
  NS_ENSURE_ARG_POINTER(aAccount);
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsMsgAccountManager::RemoveIdentity(nsIMsgIdentity *aIdentity)
{
  // finish this
  return NS_OK;
}

NS_IMETHODIMP
nsMsgAccountManager::RemoveAccount(nsIMsgAccount *aAccount)
{
  NS_ENSURE_ARG_POINTER(aAccount);
  nsresult rv;
  rv = LoadAccounts();
  if (NS_FAILED(rv)) return rv;

  // order is important!
  // remove it from the prefs first
  nsXPIDLCString key;
  rv = aAccount->GetKey(getter_Copies(key));
  if (NS_FAILED(rv)) return rv;
  
  rv = removeKeyedAccount(key);
  if (NS_FAILED(rv)) return rv;

  // we were able to save the new prefs (i.e. not locked) so now remove it
  // from the account manager... ignore the error though, because the only
  // possible problem is that it wasn't in the hash table anyway... and if
  // so, it doesn't matter.
  m_accounts->RemoveElement(aAccount);

  // if it's the default, clear the default account
  if (m_defaultAccount.get() == aAccount) {
    SetDefaultAccount(nsnull);
  }

  // XXX - need to figure out if this is the last time this server is
  // being used, and only send notification then.
  // (and only remove from hashtable then too!)
  nsCOMPtr<nsIMsgIncomingServer> server;
  rv = aAccount->GetIncomingServer(getter_AddRefs(server));
  if (NS_SUCCEEDED(rv) && server) {
    nsXPIDLCString serverKey;
    rv = server->GetKey(getter_Copies(serverKey));
    NS_ENSURE_SUCCESS(rv,rv);

    LogoutOfServer(server); // close cached connections and forget session password

    // invalidate the FindServer() cache if we are removing the cached server
    if (m_lastFindServerResult) {
        nsXPIDLCString cachedServerKey;
        rv = m_lastFindServerResult->GetKey(getter_Copies(cachedServerKey));
        NS_ENSURE_SUCCESS(rv,rv);
    
        if (!nsCRT::strcmp((const char *)serverKey,(const char *)cachedServerKey)) {
            rv = SetLastServerFound(nsnull,"","","");
            NS_ENSURE_SUCCESS(rv,rv);
        }
    }

    nsCStringKey hashKey(serverKey);
    
    nsIMsgIncomingServer* removedServer =
      (nsIMsgIncomingServer*) m_incomingServers.Remove(&hashKey);

    //NS_ASSERTION(server.get() == removedServer, "Key maps to different server. something wacky is going on");

    // remove reference from hashtable
    NS_IF_RELEASE(removedServer);
    
    nsCOMPtr<nsIMsgFolder> rootFolder;
    server->GetRootFolder(getter_AddRefs(rootFolder));
    nsCOMPtr<nsISupportsArray> allDescendents;
    NS_NewISupportsArray(getter_AddRefs(allDescendents));
    rootFolder->ListDescendents(allDescendents);
    PRUint32 cnt =0;
    rv = allDescendents->Count(&cnt);
    NS_ENSURE_SUCCESS(rv,rv);
    for (PRUint32 i=0; i< cnt;i++)
    {
      nsCOMPtr<nsIMsgFolder> folder = do_QueryElementAt(allDescendents, i, &rv);
      folder->ForceDBClosed();
    }
   
    mFolderListeners->EnumerateForwards(removeListenerFromFolder,
                                        (void*)rootFolder);
    
    NotifyServerUnloaded(server);


    rv = server->RemoveFiles();
    NS_ASSERTION(NS_SUCCEEDED(rv), "failed to remove the files associated with server");

    
    // now clear out the server once and for all.
    // watch out! could be scary
    server->ClearAllValues();

    rootFolder->Shutdown(PR_TRUE);
  }
  nsCOMPtr<nsISupportsArray> identityArray;
  
  rv = aAccount->GetIdentities(getter_AddRefs(identityArray));
  if (NS_SUCCEEDED(rv)) {

    PRUint32 count=0;
    identityArray->Count(&count);

    PRUint32 i;
    for (i=0; i<count; i++) {
      nsCOMPtr<nsIMsgIdentity> identity;
      rv = identityArray->QueryElementAt(i, NS_GET_IID(nsIMsgIdentity),
                                         (void **)getter_AddRefs(identity));
      if (NS_SUCCEEDED(rv))
        // clear out all identity information.
        // watch out! could be scary
        identity->ClearAllValues();
    }

  }

  aAccount->ClearAllValues();
  return NS_OK;
}

// remove the account with the given key.
// note that this does NOT remove any of the related prefs
// (like the server, identity, etc)
nsresult
nsMsgAccountManager::removeKeyedAccount(const char *key)
{
  nsresult rv;
  rv = getPrefService();
  if (NS_FAILED(rv)) return rv;

  nsXPIDLCString accountList;
  rv = m_prefs->GetCharPref(PREF_MAIL_ACCOUNTMANAGER_ACCOUNTS, getter_Copies(accountList));
  if (NS_FAILED(rv)) return rv;

  // reconstruct the new account list, re-adding all accounts except
  // the one with 'key'
  nsCAutoString newAccountList;
  char *newStr;
  char *rest = accountList.BeginWriting();
  
  char *token = nsCRT::strtok(rest, ",", &newStr);
  while (token) {
    nsCAutoString testKey(token);
    testKey.StripWhitespace();

    // re-add the candidate key only if it's not the key we're looking for
    if (!testKey.IsEmpty() && !testKey.Equals(key)) {
      if (!newAccountList.IsEmpty())
        newAccountList += ',';
      newAccountList += testKey;
    }

    token = nsCRT::strtok(newStr, ",", &newStr);
  }

  // Update mAccountKeyList to reflect the deletion
  mAccountKeyList = newAccountList;

  // now write the new account list back to the prefs
  rv = m_prefs->SetCharPref(PREF_MAIL_ACCOUNTMANAGER_ACCOUNTS,
                              newAccountList.get());
  if (NS_FAILED(rv)) return rv;


  return NS_OK;
}

/* get the default account. If no default account, pick the first account */
NS_IMETHODIMP
nsMsgAccountManager::GetDefaultAccount(nsIMsgAccount * *aDefaultAccount)
{
  NS_ENSURE_ARG_POINTER(aDefaultAccount);
  nsresult rv;
  rv = LoadAccounts();
  if (NS_FAILED(rv)) return rv;
  
  PRUint32 count;
  if (!m_defaultAccount) {
    m_accounts->Count(&count);
    if (count == 0) {
      *aDefaultAccount=nsnull;
      return NS_ERROR_FAILURE;
    }

    nsXPIDLCString defaultKey;
    rv = m_prefs->GetCharPref(PREF_MAIL_ACCOUNTMANAGER_DEFAULTACCOUNT, getter_Copies(defaultKey));
    
    if (NS_SUCCEEDED(rv)) {
      GetAccount(defaultKey, getter_AddRefs(m_defaultAccount));
    } else {
      PRUint32 index;
      PRBool foundValidDefaultAccount = PR_FALSE;
      for (index = 0; index < count; index++) {
        nsCOMPtr<nsIMsgAccount> aAccount;
        rv = m_accounts->QueryElementAt(index, NS_GET_IID(nsIMsgAccount),
                                        (void **)getter_AddRefs(aAccount));
        if (NS_SUCCEEDED(rv)) {
          // get incoming server
          nsCOMPtr <nsIMsgIncomingServer> server;
          rv = aAccount->GetIncomingServer(getter_AddRefs(server));
          NS_ENSURE_SUCCESS(rv,rv);
          
          PRBool canBeDefaultServer = PR_FALSE;
          if (server)
            server->GetCanBeDefaultServer(&canBeDefaultServer);
          
          // if this can serve as default server, set it as default and
          // break outof the loop.
          if (canBeDefaultServer) {
            SetDefaultAccount(aAccount);
            foundValidDefaultAccount = PR_TRUE;
            break;
          }
        }
      }
      if (!foundValidDefaultAccount) {
        // get the first account and use it.
        // we need to fix this scenario.
        nsCOMPtr<nsIMsgAccount> firstAccount;
        rv = m_accounts->QueryElementAt(0, NS_GET_IID(nsIMsgAccount),
                                       (void **)getter_AddRefs(firstAccount));

        SetDefaultAccount(firstAccount);
      }
    }
  }
  
  *aDefaultAccount = m_defaultAccount;
  NS_IF_ADDREF(*aDefaultAccount);
  return NS_OK;
}


NS_IMETHODIMP
nsMsgAccountManager::SetDefaultAccount(nsIMsgAccount * aDefaultAccount)
{
  if (m_defaultAccount != aDefaultAccount)
  {
    nsCOMPtr<nsIMsgAccount> oldAccount = m_defaultAccount;

    m_defaultAccount = aDefaultAccount;

    // it's ok if this fails
    setDefaultAccountPref(aDefaultAccount);

    // ok if notifications fail
    notifyDefaultServerChange(oldAccount, aDefaultAccount);
  }
  return NS_OK;
}

// fire notifications
nsresult
nsMsgAccountManager::notifyDefaultServerChange(nsIMsgAccount *aOldAccount,
                                               nsIMsgAccount *aNewAccount)
{
  nsresult rv;

  nsCOMPtr<nsIMsgIncomingServer> server;
  nsCOMPtr<nsIMsgFolder> rootFolder;
  
  // first tell old server it's no longer the default
  if (aOldAccount) {
    rv = aOldAccount->GetIncomingServer(getter_AddRefs(server));
    if (NS_SUCCEEDED(rv) && server) {
      rv = server->GetRootFolder(getter_AddRefs(rootFolder));
      if (NS_SUCCEEDED(rv) && rootFolder)
        rootFolder->NotifyBoolPropertyChanged(kDefaultServerAtom,
                                                        PR_TRUE, PR_FALSE);
    }
  }
    
    // now tell new server it is.
  if (aNewAccount) {
    rv = aNewAccount->GetIncomingServer(getter_AddRefs(server));
    if (NS_SUCCEEDED(rv) && server) {
      rv = server->GetRootFolder(getter_AddRefs(rootFolder));
      if (NS_SUCCEEDED(rv) && rootFolder)
        rootFolder->NotifyBoolPropertyChanged(kDefaultServerAtom,
                                              PR_FALSE, PR_TRUE);
    }
  }

  if (aOldAccount && aNewAccount)  //only notify if the user goes and changes default account
  {
    nsCOMPtr<nsIObserverService> observerService = 
      do_GetService("@mozilla.org/observer-service;1", &rv);

    if (NS_SUCCEEDED(rv))
      observerService->NotifyObservers(nsnull,"mailDefaultAccountChanged",nsnull);
  }

  return NS_OK;
}

nsresult
nsMsgAccountManager::setDefaultAccountPref(nsIMsgAccount* aDefaultAccount)
{
  nsresult rv;
  
  rv = getPrefService();
  NS_ENSURE_SUCCESS(rv,rv);

  if (aDefaultAccount) {
    nsXPIDLCString key;
    rv = aDefaultAccount->GetKey(getter_Copies(key));
    NS_ENSURE_SUCCESS(rv, rv);
    
    rv = m_prefs->SetCharPref(PREF_MAIL_ACCOUNTMANAGER_DEFAULTACCOUNT, key);
    NS_ENSURE_SUCCESS(rv,rv);
  }
  else
    // don't care if this fails
    m_prefs->ClearUserPref(PREF_MAIL_ACCOUNTMANAGER_DEFAULTACCOUNT);

  return NS_OK;
}
    

// enumaration for sending unload notifications
PRBool
nsMsgAccountManager::hashUnloadServer(nsHashKey *aKey, void *aData,
                                          void *closure)
{
    nsresult rv;
    nsCOMPtr<nsIMsgIncomingServer> server =
      do_QueryInterface((nsISupports*)aData, &rv);
    if (NS_FAILED(rv)) return PR_TRUE;
    
	nsMsgAccountManager *accountManager = (nsMsgAccountManager*)closure;
	accountManager->NotifyServerUnloaded(server);

	nsCOMPtr<nsIMsgFolder> rootFolder;
	rv = server->GetRootFolder(getter_AddRefs(rootFolder));

    accountManager->mFolderListeners->EnumerateForwards(removeListenerFromFolder,
                                        (void *)(nsIMsgFolder*)rootFolder);

	if(NS_SUCCEEDED(rv))
		rootFolder->Shutdown(PR_TRUE);

	return PR_TRUE;

}

/* static */ void nsMsgAccountManager::LogoutOfServer(nsIMsgIncomingServer *aServer)
{
    nsresult rv = aServer->Shutdown();
    NS_ASSERTION(NS_SUCCEEDED(rv), "Shutdown of server failed");
    rv = aServer->ForgetSessionPassword();
    NS_ASSERTION(NS_SUCCEEDED(rv), "failed to remove the password associated with server");
}

/* static */ PRBool
nsMsgAccountManager::hashLogoutOfServer(nsHashKey *aKey, void *aData,
                                               void *closure)
{
    nsresult rv;
    nsCOMPtr<nsIMsgIncomingServer> server =
      do_QueryInterface((nsISupports*)aData, &rv);
    if (NS_SUCCEEDED(rv))
      LogoutOfServer(server);

    return PR_TRUE;
}

NS_IMETHODIMP nsMsgAccountManager::GetFolderCache(nsIMsgFolderCache* *aFolderCache)
{
  if (!aFolderCache) return NS_ERROR_NULL_POINTER;

  nsresult rv = NS_OK;

  if (!m_msgFolderCache)
  {
    rv = nsComponentManager::CreateInstance(kMsgFolderCacheCID,
                                            NULL,
                                            NS_GET_IID(nsIMsgFolderCache),
                                            getter_AddRefs(m_msgFolderCache));
    if (NS_FAILED(rv))
		return rv;

    nsCOMPtr<nsIFile> cacheFile;
    nsCOMPtr <nsIFileSpec> cacheFileSpec;
    
    rv = NS_GetSpecialDirectory(NS_APP_MESSENGER_FOLDER_CACHE_50_DIR, getter_AddRefs(cacheFile));
    if (NS_FAILED(rv)) return rv;
    
    // TODO: Make nsIMsgFolderCache::Init take an nsIFile and
    // avoid this conversion.
    rv = NS_NewFileSpecFromIFile(cacheFile, getter_AddRefs(cacheFileSpec));
    if (NS_FAILED(rv)) return rv;
               
    m_msgFolderCache->Init(cacheFileSpec);
  }

  *aFolderCache = m_msgFolderCache;
  NS_IF_ADDREF(*aFolderCache);
  return rv;
}


// enumaration for writing out accounts to folder cache.
PRBool nsMsgAccountManager::writeFolderCache(nsHashKey *aKey, void *aData,
                                             void *closure)
{
    nsIMsgIncomingServer *server = (nsIMsgIncomingServer*)aData;
	nsIMsgFolderCache *folderCache = (nsIMsgFolderCache *)closure;

	server->WriteToFolderCache(folderCache);
	return PR_TRUE;
}

// enumeration for empty trash on exit
PRBool PR_CALLBACK nsMsgAccountManager::cleanupOnExit(nsHashKey *aKey, void *aData,
                                             void *closure)
{
  nsIMsgIncomingServer *server = (nsIMsgIncomingServer*)aData;
  PRBool emptyTrashOnExit = PR_FALSE;
  PRBool cleanupInboxOnExit = PR_FALSE;
  nsresult rv;
    
  if (WeAreOffline())
    return PR_TRUE;

  server->GetEmptyTrashOnExit(&emptyTrashOnExit);
  nsCOMPtr <nsIImapIncomingServer> imapserver = do_QueryInterface(server);
  if (imapserver)
    imapserver->GetCleanupInboxOnExit(&cleanupInboxOnExit);
  if (emptyTrashOnExit || cleanupInboxOnExit)
  {
    nsCOMPtr<nsIMsgFolder> root;
    server->GetRootFolder(getter_AddRefs(root));
    nsXPIDLCString type;
    server->GetType(getter_Copies(type));
    if (root)
    {
      nsCOMPtr<nsIMsgFolder> folder;
      folder = do_QueryInterface(root);
      if (folder)
      {
         nsXPIDLCString passwd;
         PRBool serverRequiresPasswordForAuthentication = PR_TRUE;
         PRBool isImap = (type ? PL_strcmp(type, "imap") == 0 :
                          PR_FALSE);
         if (isImap)
         {
           server->GetServerRequiresPasswordForBiff(&serverRequiresPasswordForAuthentication);
           server->GetPassword(getter_Copies(passwd));
         }
         if (!isImap || (isImap && (!serverRequiresPasswordForAuthentication || (passwd && 
                        strlen((const char*) passwd)))))
         {
           nsCOMPtr<nsIUrlListener> urlListener;
           nsCOMPtr<nsIMsgAccountManager> accountManager = 
                    do_GetService(NS_MSGACCOUNTMANAGER_CONTRACTID, &rv);
           if (NS_FAILED(rv)) return rv;
           nsCOMPtr<nsIEventQueueService> pEventQService = 
                    do_GetService(kEventQueueServiceCID, &rv);
           if (NS_FAILED(rv)) return rv;
           nsCOMPtr<nsIEventQueue> eventQueue;
           pEventQService->GetThreadEventQueue(NS_CURRENT_THREAD,
                                    getter_AddRefs(eventQueue)); 
           if (isImap)
             urlListener = do_QueryInterface(accountManager, &rv);
                    
           if (isImap && cleanupInboxOnExit)
           {
             nsCOMPtr<nsIEnumerator> aEnumerator;
             folder->GetSubFolders(getter_AddRefs(aEnumerator));
             nsCOMPtr<nsISupports> aSupport;
             rv = aEnumerator->First();
             while (NS_SUCCEEDED(rv))
             {  
               rv = aEnumerator->CurrentItem(getter_AddRefs(aSupport));
               nsCOMPtr<nsIMsgFolder>inboxFolder = do_QueryInterface(aSupport);
               PRUint32 flags;
               inboxFolder->GetFlags(&flags);
               if (flags & MSG_FOLDER_FLAG_INBOX)
               {
                 rv = inboxFolder->Compact(urlListener, nsnull /* msgwindow */);
                 if (NS_SUCCEEDED(rv))
                   accountManager->SetFolderDoingCleanupInbox(inboxFolder);
                 break;
                }
                else
                  rv = aEnumerator->Next();
             }
           }
                          
           if (emptyTrashOnExit)
           {
             rv = folder->EmptyTrash(nsnull, urlListener);
             if (isImap && NS_SUCCEEDED(rv))
               accountManager->SetFolderDoingEmptyTrash(folder);
           }
                    
           if (isImap && urlListener)
           {
             PRBool inProgress = PR_FALSE;
             if (cleanupInboxOnExit)
             {
               accountManager->GetCleanupInboxInProgress(&inProgress);
               while (inProgress)
               {
                 accountManager->GetCleanupInboxInProgress(&inProgress);
                 PR_CEnterMonitor(folder);
                 PR_CWait(folder, PR_MicrosecondsToInterval(1000UL));
                 PR_CExitMonitor(folder);
                 if (eventQueue)
                   eventQueue->ProcessPendingEvents();
               }
             }
             if (emptyTrashOnExit)
             {
               accountManager->GetEmptyTrashInProgress(&inProgress);
               while (inProgress)
               {
                 accountManager->GetEmptyTrashInProgress(&inProgress);
                 PR_CEnterMonitor(folder);
                 PR_CWait(folder, PR_MicrosecondsToInterval(1000UL));
                 PR_CExitMonitor(folder);
                 if (eventQueue)
                   eventQueue->ProcessPendingEvents();
               }
             }
           } 
         }
       }     
     }
   }
   return PR_TRUE;
}

PRBool nsMsgAccountManager::closeCachedConnections(nsHashKey *aKey, void *aData, void *closure)
{
  nsIMsgIncomingServer *server = (nsIMsgIncomingServer*)aData;

  server->CloseCachedConnections();

  return PR_TRUE;
}

PRBool nsMsgAccountManager::shutdown(nsHashKey *aKey, void *aData, void *closure)
{
  nsIMsgIncomingServer *server = (nsIMsgIncomingServer*)aData;

  server->Shutdown();

  return PR_TRUE;
}


/* readonly attribute nsISupportsArray accounts; */
NS_IMETHODIMP
nsMsgAccountManager::GetAccounts(nsISupportsArray **_retval)
{
  nsresult rv;
  
  rv = LoadAccounts();
  if (NS_FAILED(rv)) return rv;
  
  nsCOMPtr<nsISupportsArray> accounts;
  NS_NewISupportsArray(getter_AddRefs(accounts));

  accounts->AppendElements(m_accounts);

  *_retval = accounts;
  NS_ADDREF(*_retval);

  return NS_OK;
}

PRBool
nsMsgAccountManager::hashElementToArray(nsHashKey *aKey, void *aData,
                                        void *closure)
{
    nsISupports* element = (nsISupports*)aData;
    nsISupportsArray* array = (nsISupportsArray*)closure;

    array->AppendElement(element);
    return PR_TRUE;
}

PRBool
nsMsgAccountManager::hashElementRelease(nsHashKey *aKey, void *aData,
                                        void *closure)
{
  nsISupports* element = (nsISupports*)aData;

  NS_RELEASE(element);

  return PR_TRUE;               // return true to remove this element
}

/* nsISupportsArray GetAllIdentities (); */
NS_IMETHODIMP
nsMsgAccountManager::GetAllIdentities(nsISupportsArray **_retval)
{
  nsresult rv;
  rv = LoadAccounts();
  if (NS_FAILED(rv)) return rv;
  
  nsCOMPtr<nsISupportsArray> identities;
  rv = NS_NewISupportsArray(getter_AddRefs(identities));
  if (NS_FAILED(rv)) return rv;

  // convert hash table->nsISupportsArray of identities
  m_accounts->EnumerateForwards(getIdentitiesToArray,
                                (void *)(nsISupportsArray*)identities);
  // convert nsISupportsArray->nsISupportsArray
  // when do we free the nsISupportsArray?
  *_retval = identities;
  NS_ADDREF(*_retval);
  return rv;
}

PRBool
nsMsgAccountManager::addIdentityIfUnique(nsISupports *element, void *aData)
{
  nsresult rv;
  nsCOMPtr<nsIMsgIdentity> identity = do_QueryInterface(element, &rv);
  if (NS_FAILED(rv)) {
    printf("addIdentityIfUnique problem\n");
    return PR_TRUE;
  }
  
  nsISupportsArray *array = (nsISupportsArray*)aData;

  
  nsXPIDLCString key;
  rv = identity->GetKey(getter_Copies(key));
  if (NS_FAILED(rv)) return PR_TRUE;

  PRUint32 count=0;
  rv = array->Count(&count);
  if (NS_FAILED(rv)) return PR_TRUE;
  
  PRBool found=PR_FALSE;
  PRUint32 i;
  for (i=0; i<count; i++) {
    nsCOMPtr<nsISupports> thisElement;
    array->GetElementAt(i, getter_AddRefs(thisElement));

    nsCOMPtr<nsIMsgIdentity> thisIdentity =
      do_QueryInterface(thisElement, &rv);
    if (NS_FAILED(rv)) continue;

    nsXPIDLCString thisKey;
    thisIdentity->GetKey(getter_Copies(thisKey));
    if (PL_strcmp(key, thisKey)==0) {
      found = PR_TRUE;
      break;
    }
  }

  if (!found)
    array->AppendElement(identity);

  return PR_TRUE;
}

PRBool
nsMsgAccountManager::getIdentitiesToArray(nsISupports *element, void *aData)
{
  nsresult rv;
  nsCOMPtr<nsIMsgAccount> account = do_QueryInterface(element, &rv);
  if (NS_FAILED(rv)) return PR_TRUE;
  
  
  nsCOMPtr<nsISupportsArray> identities;
  rv = account->GetIdentities(getter_AddRefs(identities));
  if (NS_FAILED(rv)) return PR_TRUE;

  identities->EnumerateForwards(addIdentityIfUnique, aData);
  
  return PR_TRUE;
}

/* nsISupportsArray GetAllServers (); */
NS_IMETHODIMP
nsMsgAccountManager::GetAllServers(nsISupportsArray **_retval)
{
  nsresult rv;
  rv = LoadAccounts();
  if (NS_FAILED(rv)) return rv;
  
  nsCOMPtr<nsISupportsArray> servers;
  rv = NS_NewISupportsArray(getter_AddRefs(servers));
  if (NS_FAILED(rv)) return rv;

  // enumerate by going through the list of accounts, so that we
  // get the order correct
  m_incomingServers.Enumerate(getServersToArray,
                              (void *)(nsISupportsArray*)servers);
  *_retval = servers;
  NS_ADDREF(*_retval);
  return rv;
}

PRBool PR_CALLBACK
nsMsgAccountManager::getServersToArray(nsHashKey *aKey,
                                       void *element,
                                       void *aData)
{
  nsresult rv;
  nsCOMPtr<nsIMsgIncomingServer> server =
    do_QueryInterface((nsISupports*)element, &rv);
  if (NS_FAILED(rv)) return PR_TRUE;
  
  nsISupportsArray *array = (nsISupportsArray*)aData;
  
  nsCOMPtr<nsISupports> serverSupports = do_QueryInterface(server);
  if (NS_SUCCEEDED(rv)) 
    array->AppendElement(serverSupports);

  return PR_TRUE;
}

nsresult
nsMsgAccountManager::LoadAccounts()
{
  nsresult rv;

  // for now safeguard multiple calls to this function
  if (m_accountsLoaded)
    return NS_OK;

  kDefaultServerAtom = do_GetAtom("DefaultServer");
  
  //Ensure biff service has started
  nsCOMPtr<nsIMsgBiffManager> biffService = 
           do_GetService(NS_MSGBIFFMANAGER_CONTRACTID, &rv);

  if (NS_SUCCEEDED(rv))
    biffService->Init();

  //Ensure purge service has started
  nsCOMPtr<nsIMsgPurgeService> purgeService = 
           do_GetService(NS_MSGPURGESERVICE_CONTRACTID, &rv);

  if (NS_SUCCEEDED(rv))
    purgeService->Init();
  
  // Ensure messenger OS integration service has started
  // note, you can't expect the integrationService to be there
  // we don't have OS integration on all platforms.
  nsCOMPtr<nsIMessengerOSIntegration> integrationService =
           do_GetService(NS_MESSENGEROSINTEGRATION_CONTRACTID, &rv);

  // mail.accountmanager.accounts is the main entry point for all accounts
  nsXPIDLCString accountList;
  rv = getPrefService();
  if (NS_SUCCEEDED(rv)) {
    rv = m_prefs->GetCharPref(PREF_MAIL_ACCOUNTMANAGER_ACCOUNTS, getter_Copies(accountList));
    
    /** 
     * Check to see if we need to add pre-configured accounts.
     * Following prefs are important to note in understanding the procedure here.
     *
     * 1. pref("mailnews.append_preconfig_accounts.version", version number);
     * This pref registers the current version in the user prefs file. A default value 
     * is stored in mailnews.js file. If a given vendor needs to add more preconfigured 
     * accounts, the default version number can be increased. Comparing version 
     * number from user's prefs file and the default one from mailnews.js, we
     * can add new accounts and any other version level changes that need to be done.
     *
     * 2. pref("mail.accountmanager.appendaccounts", <comma separated account list>);
     * This pref contains the list of pre-configured accounts that ISP/Vendor wants to
     * to add to the existing accounts list. 
     */
    nsCOMPtr<nsIPrefService> prefservice(do_GetService(NS_PREFSERVICE_CONTRACTID, &rv));
    NS_ENSURE_SUCCESS(rv,rv);
    
    nsCOMPtr<nsIPrefBranch> defaultsPrefBranch;
    rv = prefservice->GetDefaultBranch(MAILNEWS_ROOT_PREF, getter_AddRefs(defaultsPrefBranch));
    NS_ENSURE_SUCCESS(rv,rv);

    nsCOMPtr<nsIPrefBranch> prefBranch;
    rv = prefservice->GetBranch(MAILNEWS_ROOT_PREF, getter_AddRefs(prefBranch));
    NS_ENSURE_SUCCESS(rv,rv);

    PRInt32 appendAccountsCurrentVersion=0;
    PRInt32 appendAccountsDefaultVersion=0;
    rv = prefBranch->GetIntPref(APPEND_ACCOUNTS_VERSION_PREF_NAME, &appendAccountsCurrentVersion);
    NS_ENSURE_SUCCESS(rv,rv);

    rv = defaultsPrefBranch->GetIntPref(APPEND_ACCOUNTS_VERSION_PREF_NAME, &appendAccountsDefaultVersion);
    NS_ENSURE_SUCCESS(rv,rv);

    // Update the account list if needed
    if ((appendAccountsCurrentVersion <= appendAccountsDefaultVersion)) {

      // Get a list of pre-configured accounts
      nsXPIDLCString appendAccountList;
      rv = m_prefs->GetCharPref(PREF_MAIL_ACCOUNTMANAGER_APPEND_ACCOUNTS, getter_Copies(appendAccountList));

      // If there are pre-configured accounts, we need to add them to the existing list.
      if (!appendAccountList.IsEmpty()) {
        if (!accountList.IsEmpty()) {
          nsCStringArray existingAccountsArray;
          existingAccountsArray.ParseString(accountList.get(), ACCOUNT_DELIMITER);

          // Tokenize the data and add each account if it is not already there 
          // in the user's current mailnews account list
          char *newAccountStr;
          char *preConfigAccountsStr = ToNewCString(appendAccountList);
  
          char *token = nsCRT::strtok(preConfigAccountsStr, ACCOUNT_DELIMITER, &newAccountStr);

          nsCAutoString newAccount;
          while (token) {
            if (token && *token) {
              newAccount.Assign(token);
              newAccount.StripWhitespace();

              if (existingAccountsArray.IndexOf(newAccount) == -1) {
                accountList += ",";
                accountList += newAccount;
              }
            }
            token = nsCRT::strtok(newAccountStr, ACCOUNT_DELIMITER, &newAccountStr);
          }
          PR_Free(preConfigAccountsStr);
        }
        else {
          accountList = appendAccountList;
        }
        // Increase the version number so that updates will happen as and when needed
        rv = prefBranch->SetIntPref(APPEND_ACCOUNTS_VERSION_PREF_NAME, appendAccountsCurrentVersion + 1);
      }
    }
  }

  m_accountsLoaded = PR_TRUE;  //It is ok to return null accounts like when we create new profile
  m_haveShutdown = PR_FALSE;
  
  if (!accountList || !accountList[0])
    return NS_OK;
  
    /* parse accountList and run loadAccount on each string, comma-separated */   
    nsCOMPtr<nsIMsgAccount> account;
    char *newStr;
    char *rest = accountList.BeginWriting();
    nsCAutoString str;

    char *token = nsCRT::strtok(rest, ",", &newStr);
    while (token) {
      str = token;
      str.StripWhitespace();
      
      if (!str.IsEmpty()) 
          rv = GetAccount(str.get(), getter_AddRefs(account));

      // force load of accounts (need to find a better way to do this
      nsCOMPtr<nsISupportsArray> identities;
      account->GetIdentities(getter_AddRefs(identities));
      
      nsCOMPtr<nsIMsgIncomingServer> server;
      account->GetIncomingServer(getter_AddRefs(server));

      token = nsCRT::strtok(newStr, ",", &newStr);
    }

    
    LoadVirtualFolders();
    nsCOMPtr<nsIMsgMailSession> mailSession = do_GetService(NS_MSGMAILSESSION_CONTRACTID, &rv); 

    if (NS_SUCCEEDED(rv))
      mailSession->AddFolderListener(this, nsIFolderListener::added | nsIFolderListener::removed);
    /* finished loading accounts */
    return NS_OK;
}

PRBool
nsMsgAccountManager::getAccountList(nsISupports *element, void *aData)
{
  nsresult rv;
  nsCAutoString* accountList = (nsCAutoString*) aData;
  nsCOMPtr<nsIMsgAccount> account = do_QueryInterface(element, &rv);
  if (NS_FAILED(rv)) return PR_TRUE;
  
  nsXPIDLCString key;
  rv = account->GetKey(getter_Copies(key));
  if (NS_FAILED(rv)) return PR_TRUE;

  if ((*accountList).IsEmpty())
    (*accountList).Append(key);
  else {
    (*accountList) += ',';
    (*accountList).Append(key);
  }

  return PR_TRUE;
}

// this routine goes through all the identities and makes sure
// that the special folders for each identity have the
// correct special folder flags set, e.g, the Sent folder has
// the sent flag set.
//
// it also goes through all the spam settings for each account
// and makes sure the folder flags are set there, too
NS_IMETHODIMP
nsMsgAccountManager::SetSpecialFolders()
{
  nsresult rv;
	nsCOMPtr<nsIRDFService> rdf = do_GetService("@mozilla.org/rdf/rdf-service;1", &rv);
  NS_ENSURE_SUCCESS(rv,rv); 

  nsCOMPtr<nsISupportsArray> identities;
  GetAllIdentities(getter_AddRefs(identities));

  PRUint32 idCount=0;
  identities->Count(&idCount);

  PRUint32 id;
  nsXPIDLCString identityKey;
  
  for (id=0; id<idCount; id++) 
  {
    nsCOMPtr<nsISupports> thisSupports;
    rv = identities->GetElementAt(id, getter_AddRefs(thisSupports));
    if (NS_FAILED(rv)) continue;
    
    nsCOMPtr<nsIMsgIdentity>
      thisIdentity = do_QueryInterface(thisSupports, &rv);

    if (NS_SUCCEEDED(rv) && thisIdentity)
    {
      nsXPIDLCString folderUri;
      nsCOMPtr<nsIRDFResource> res;
      nsCOMPtr<nsIMsgFolder> folder;
      thisIdentity->GetFccFolder(getter_Copies(folderUri));
      if (folderUri && NS_SUCCEEDED(rdf->GetResource(folderUri, getter_AddRefs(res))))
      {
        folder = do_QueryInterface(res, &rv);
        nsCOMPtr <nsIMsgFolder> parent;
        if (folder && NS_SUCCEEDED(rv))
        {
          rv = folder->GetParent(getter_AddRefs(parent));
          if (NS_SUCCEEDED(rv) && parent)
            rv = folder->SetFlag(MSG_FOLDER_FLAG_SENTMAIL);
        }
      }
      thisIdentity->GetDraftFolder(getter_Copies(folderUri));
      if (folderUri && NS_SUCCEEDED(rdf->GetResource(folderUri, getter_AddRefs(res))))
      {
        folder = do_QueryInterface(res, &rv);
        if (NS_SUCCEEDED(rv))
          rv = folder->SetFlag(MSG_FOLDER_FLAG_DRAFTS);
      }
      thisIdentity->GetStationeryFolder(getter_Copies(folderUri));
      if (folderUri && NS_SUCCEEDED(rdf->GetResource(folderUri, getter_AddRefs(res))))
      {
        folder = do_QueryInterface(res, &rv);
        if (folder && NS_SUCCEEDED(rv))
        {
          nsCOMPtr <nsIMsgFolder> parent;
          rv = folder->GetParent(getter_AddRefs(parent));
          if (NS_SUCCEEDED(rv) && parent) // only set flag if folder is real
            rv = folder->SetFlag(MSG_FOLDER_FLAG_TEMPLATES);
        }
      }
    }
  }

  // XXX todo
  // get all servers
  // get all spam settings for each server
  // set the JUNK folder flag on the spam folders, right?
  return NS_OK;
}

NS_IMETHODIMP
nsMsgAccountManager::UnloadAccounts()
{
  // release the default account
  kDefaultServerAtom = nsnull;
  m_defaultAccount=nsnull;
  m_incomingServers.Enumerate(hashUnloadServer, this);

  m_accounts->Clear();          // will release all elements
  m_identities.Reset(hashElementRelease, nsnull);
  m_incomingServers.Reset(hashElementRelease, nsnull);
  m_accountsLoaded = PR_FALSE;
  mAccountKeyList.Truncate(0);
  SetLastServerFound(nsnull,"","","");
  return NS_OK;
}

NS_IMETHODIMP
nsMsgAccountManager::ShutdownServers()
{
  m_incomingServers.Enumerate(shutdown, nsnull);
  return NS_OK;
}

NS_IMETHODIMP
nsMsgAccountManager::CloseCachedConnections()
{
  m_incomingServers.Enumerate(closeCachedConnections, nsnull);
  return NS_OK;
}

NS_IMETHODIMP
nsMsgAccountManager::CleanupOnExit()
{
  m_incomingServers.Enumerate(cleanupOnExit, nsnull);
  return NS_OK;
}

NS_IMETHODIMP
nsMsgAccountManager::WriteToFolderCache(nsIMsgFolderCache *folderCache)
{
	m_incomingServers.Enumerate(writeFolderCache, folderCache);
	return folderCache->Close();
}

nsresult
nsMsgAccountManager::createKeyedAccount(const char* key,
                                        nsIMsgAccount ** aAccount)
{
    
  nsCOMPtr<nsIMsgAccount> account;
  nsresult rv;
  rv = nsComponentManager::CreateInstance(kMsgAccountCID,
                                          nsnull,
                                          NS_GET_IID(nsIMsgAccount),
                                          (void **)getter_AddRefs(account));
  
  if (NS_FAILED(rv)) return rv;
  account->SetKey(key);

  // add to internal nsISupportsArray
  m_accounts->AppendElement(NS_STATIC_CAST(nsISupports*, account));

  // add to string list
  if (mAccountKeyList.IsEmpty())
    mAccountKeyList = key;
  else {
    mAccountKeyList += ",";
    mAccountKeyList += key;
  }

  rv = getPrefService();
  if (NS_SUCCEEDED(rv))
    m_prefs->SetCharPref(PREF_MAIL_ACCOUNTMANAGER_ACCOUNTS,
                         mAccountKeyList.get());

  *aAccount = account;
  NS_ADDREF(*aAccount);
  
  return NS_OK;
}

nsresult
nsMsgAccountManager::CreateAccount(nsIMsgAccount **_retval)
{
    NS_ENSURE_ARG_POINTER(_retval);

    nsCAutoString key;
    getUniqueAccountKey(ACCOUNT_PREFIX, m_accounts, key);

    return createKeyedAccount(key.get(), _retval);
}

nsresult
nsMsgAccountManager::GetAccount(const char* key,
                                nsIMsgAccount **_retval)
{
    if (!_retval) return NS_ERROR_NULL_POINTER;

    findAccountByKeyEntry findEntry;
    findEntry.key = key;
    findEntry.account = nsnull;
    
    m_accounts->EnumerateForwards(findAccountByKey, (void *)&findEntry);

    if (findEntry.account) {
        *_retval = findEntry.account;
        NS_ADDREF(*_retval);
        return NS_OK;
    }

    // not found, create on demand
    return createKeyedAccount(key, _retval);
}

nsresult
nsMsgAccountManager::FindServerIndex(nsIMsgIncomingServer* server,
                                     PRInt32* result)
{
  NS_ENSURE_ARG_POINTER(server);
  nsresult rv;
  
  nsXPIDLCString key;
  rv = server->GetKey(getter_Copies(key));

  findServerByKeyEntry findEntry;
  findEntry.key = key;
  findEntry.index = -1;
  
  // do this by account because the account list is in order
  m_accounts->EnumerateForwards(findServerIndexByServer, (void *)&findEntry);

  // even if the search failed, we can return index.
  // this means that all servers not in the array return an index higher
  // than all "registered" servers
  *result = findEntry.index;
  
  return NS_OK;
}

PRBool
nsMsgAccountManager::findServerIndexByServer(nsISupports *element, void *aData)
{
  nsresult rv;
  
  nsCOMPtr<nsIMsgAccount> account = do_QueryInterface(element);
  findServerByKeyEntry *entry = (findServerByKeyEntry*) aData;

  // increment the index;
  entry->index++;
  
  nsCOMPtr<nsIMsgIncomingServer> server;
  rv = account->GetIncomingServer(getter_AddRefs(server));
  if (!server || NS_FAILED(rv)) return PR_TRUE;
  
  nsXPIDLCString key;
  rv = server->GetKey(getter_Copies(key));
  if (NS_FAILED(rv)) return PR_TRUE;

  // stop when found,
  // index will be set to the current index 
  if (nsCRT::strcmp(key, entry->key)==0)
    return PR_FALSE;
  
  return PR_TRUE;
}

PRBool
nsMsgAccountManager::findAccountByKey(nsISupports* element, void *aData)
{
    nsresult rv;
    nsCOMPtr<nsIMsgAccount> account = do_QueryInterface(element, &rv);
    if (NS_FAILED(rv)) return PR_TRUE;
    
    findAccountByKeyEntry *entry = (findAccountByKeyEntry*) aData;

    nsXPIDLCString key;
    account->GetKey(getter_Copies(key));
    if (PL_strcmp(key, entry->key)==0) {
        entry->account = account;
        return PR_FALSE;        // stop when found
    }

    return PR_TRUE;
}

NS_IMETHODIMP nsMsgAccountManager::AddIncomingServerListener(nsIIncomingServerListener *serverListener)
{
   m_incomingServerListeners.AppendObject(serverListener);
   return NS_OK;
}

NS_IMETHODIMP nsMsgAccountManager::RemoveIncomingServerListener(nsIIncomingServerListener *serverListener)
{
    m_incomingServerListeners.RemoveObject(serverListener);
    return NS_OK;
}


NS_IMETHODIMP nsMsgAccountManager::NotifyServerLoaded(nsIMsgIncomingServer *server)
{
	PRInt32 count = m_incomingServerListeners.Count();
	
	for(PRInt32 i = 0; i < count; i++)
	{
		nsIIncomingServerListener* listener = m_incomingServerListeners[i];
		listener->OnServerLoaded(server);
	}

	return NS_OK;
}

NS_IMETHODIMP nsMsgAccountManager::NotifyServerUnloaded(nsIMsgIncomingServer *server)
{
	PRInt32 count = m_incomingServerListeners.Count();
	server->SetFilterList(nsnull); // clear this to cut shutdown leaks. we are always passing valid non-null server here. 
	
	for(PRInt32 i = 0; i < count; i++)
	{
		nsIIncomingServerListener* listener = m_incomingServerListeners[i];
		listener->OnServerUnloaded(server);
	}

	return NS_OK;
}

NS_IMETHODIMP nsMsgAccountManager::NotifyServerChanged(nsIMsgIncomingServer *server)
{
	PRInt32 count = m_incomingServerListeners.Count();
	
	for(PRInt32 i = 0; i < count; i++)
	{
		nsIIncomingServerListener* listener = m_incomingServerListeners[i];
		listener->OnServerChanged(server);
	}

	return NS_OK;
}

nsresult
nsMsgAccountManager::InternalFindServer(const char* username,
                                const char* hostname,
                                const char* type,
                                PRBool useRealSetting,
                                nsIMsgIncomingServer** aResult)
{
  nsresult rv;
  nsCOMPtr<nsISupportsArray> servers;
 
  // If 'useRealSetting' is set then we want to scan all existing accounts
  // to make sure there's no duplicate including those whose host and/or
  // user names have been changed.
  if (!useRealSetting &&
      (!nsCRT::strcmp((hostname?hostname:""),m_lastFindServerHostName.get())) &&
      (!nsCRT::strcmp((username?username:""),m_lastFindServerUserName.get())) &&
      (!nsCRT::strcmp((type?type:""),m_lastFindServerType.get())) &&
      m_lastFindServerResult) 
  {
    NS_ADDREF(*aResult = m_lastFindServerResult);
    return NS_OK;
  }

  rv = GetAllServers(getter_AddRefs(servers));
  if (NS_FAILED(rv)) return rv;

  findServerEntry serverInfo;

  // "" acts as the wild card.

  // hostname might be blank, pass "" instead
  serverInfo.hostname = hostname ? hostname : "";
  // username might be blank, pass "" instead
  serverInfo.username = username ? username : "";
  // type might be blank, pass "" instead
  serverInfo.type = type ? type : "";
  serverInfo.useRealSetting = useRealSetting;

  serverInfo.server = *aResult = nsnull;
  
  servers->EnumerateForwards(findServer, (void *)&serverInfo);

  if (!serverInfo.server) return NS_ERROR_UNEXPECTED;

  // cache for next time
  rv = SetLastServerFound(serverInfo.server, hostname, username, type);
  NS_ENSURE_SUCCESS(rv,rv);

  *aResult = serverInfo.server;
  NS_ADDREF(*aResult);
  
  return NS_OK;

}

NS_IMETHODIMP
nsMsgAccountManager::FindServer(const char* username,
                                const char* hostname,
                                const char* type,
                                nsIMsgIncomingServer** aResult)
{
  return(InternalFindServer(username, hostname, type, PR_FALSE, aResult));
}

// Interface called by UI js only (always return true).
NS_IMETHODIMP
nsMsgAccountManager::FindRealServer(const char* username,
                                const char* hostname,
                                const char* type,
                                nsIMsgIncomingServer** aResult)
{
  InternalFindServer(username, hostname, type, PR_TRUE, aResult);
  return NS_OK;
}

PRBool
nsMsgAccountManager::findAccountByServerKey(nsISupports *element,
                                          void *aData)
{
  nsresult rv;
  findAccountByKeyEntry *entry = (findAccountByKeyEntry*)aData;
  nsCOMPtr<nsIMsgAccount> account =
    do_QueryInterface(element, &rv);
  if (NS_FAILED(rv)) return PR_TRUE;

  nsCOMPtr<nsIMsgIncomingServer> server;
  rv = account->GetIncomingServer(getter_AddRefs(server));
  if (!server || NS_FAILED(rv)) return PR_TRUE;

  nsXPIDLCString key;
  rv = server->GetKey(getter_Copies(key));
  if (NS_FAILED(rv)) return PR_TRUE;

  // if the keys are equal, the servers are equal
  if (PL_strcmp(key, entry->key)==0) {
    entry->account = account;
    return PR_FALSE;            // stop on first found account
  }

  return PR_TRUE;
}

NS_IMETHODIMP
nsMsgAccountManager::FindAccountForServer(nsIMsgIncomingServer *server,
                                            nsIMsgAccount **aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  
  if (!server) {
    (*aResult) = nsnull;
    return NS_OK;
  }
  
  nsresult rv;

  nsXPIDLCString key;
  rv = server->GetKey(getter_Copies(key));
  if (NS_FAILED(rv)) return rv;
  
  findAccountByKeyEntry entry;
  entry.key = key;
  entry.account = nsnull;

  m_accounts->EnumerateForwards(findAccountByServerKey, (void *)&entry);

  if (entry.account) {
    *aResult = entry.account;
    NS_ADDREF(*aResult);
  }
  return NS_OK;
}

// if the aElement matches the given hostname, add it to the given array
PRBool
nsMsgAccountManager::findServer(nsISupports *aElement, void *data)
{
  nsresult rv;
  
  nsCOMPtr<nsIMsgIncomingServer> server = do_QueryInterface(aElement, &rv);
  if (NS_FAILED(rv)) return PR_TRUE;

  findServerEntry *entry = (findServerEntry*) data;
  
  nsXPIDLCString thisHostname;
  if (entry->useRealSetting)
    rv = server->GetRealHostName(getter_Copies(thisHostname));
  else
  rv = server->GetHostName(getter_Copies(thisHostname));
  if (NS_FAILED(rv)) return PR_TRUE;

  nsXPIDLCString thisUsername;
  if (entry->useRealSetting)
    rv = server->GetRealUsername(getter_Copies(thisUsername));
  else
  rv = server->GetUsername(getter_Copies(thisUsername));
  if (NS_FAILED(rv)) return PR_TRUE;
 
  nsXPIDLCString thisType;
  rv = server->GetType(getter_Copies(thisType));
  if (NS_FAILED(rv)) return PR_TRUE;
 
  // treat "" as a wild card, so if the caller passed in "" for the desired attribute
  // treat it as a match
  PRBool checkType = PL_strcmp(entry->type, "");
  PRBool checkHostname = PL_strcmp(entry->hostname,"");
  PRBool checkUsername = PL_strcmp(entry->username,"");
  if ((!checkType || (PL_strcmp(entry->type, thisType)==0)) && 
      (!checkHostname || (PL_strcasecmp(entry->hostname, thisHostname)==0)) && 
      (!checkUsername || (PL_strcmp(entry->username, thisUsername)==0))) 
  {
    entry->server = server;
    return PR_FALSE;            // stop on first find 
  }
  
  return PR_TRUE;
}

NS_IMETHODIMP
nsMsgAccountManager::GetFirstIdentityForServer(nsIMsgIncomingServer *aServer, nsIMsgIdentity **aIdentity)
{
  NS_ENSURE_ARG_POINTER(aServer);
  NS_ENSURE_ARG_POINTER(aIdentity);

  nsCOMPtr<nsISupportsArray> identities;
  nsresult rv = GetIdentitiesForServer(aServer, getter_AddRefs(identities));
  NS_ENSURE_SUCCESS(rv, rv);

  // not all servers have identities
  // for example, Local Folders
  PRUint32 numIdentities;
  rv = identities->Count(&numIdentities);
  NS_ENSURE_SUCCESS(rv, rv);

  if (numIdentities > 0) {
    nsCOMPtr<nsIMsgIdentity> identity;
    rv = identities->QueryElementAt(0, NS_GET_IID(nsIMsgIdentity),
                                  (void **)getter_AddRefs(identity));
    NS_ENSURE_SUCCESS(rv, rv);
    NS_IF_ADDREF(*aIdentity = identity);
  }
  else
    *aIdentity = nsnull;
  return rv;
}

NS_IMETHODIMP
nsMsgAccountManager::GetIdentitiesForServer(nsIMsgIncomingServer *server,
                                            nsISupportsArray **_retval)
{
  NS_ENSURE_ARG_POINTER(server);
  NS_ENSURE_ARG_POINTER(_retval);
  nsresult rv;
  rv = LoadAccounts();
  if (NS_FAILED(rv)) return rv;
  
  nsCOMPtr<nsISupportsArray> identities;
  rv = NS_NewISupportsArray(getter_AddRefs(identities));
  if (NS_FAILED(rv)) return rv;
  
  findIdentitiesByServerEntry identityInfo;
  identityInfo.server = server;
  identityInfo.identities = identities;
  
  m_accounts->EnumerateForwards(findIdentitiesForServer,
                                (void *)&identityInfo);

  // do an addref for the caller.
  *_retval = identities;
  NS_ADDREF(*_retval);

  return NS_OK;
}

PRBool
nsMsgAccountManager::findIdentitiesForServer(nsISupports* element, void *aData)
{
  nsresult rv;
  nsCOMPtr<nsIMsgAccount> account = do_QueryInterface(element, &rv);
  if (NS_FAILED(rv)) return PR_TRUE;
  
  findIdentitiesByServerEntry *entry = (findIdentitiesByServerEntry*)aData;
  
  nsCOMPtr<nsIMsgIncomingServer> thisServer;
  rv = account->GetIncomingServer(getter_AddRefs(thisServer));
  if (NS_FAILED(rv)) return PR_TRUE;
  nsXPIDLCString serverKey;
	
  NS_ASSERTION(thisServer, "thisServer is null");
  NS_ASSERTION(entry, "entry is null");
  NS_ASSERTION(entry->server, "entry->server is null");
  // if this happens, bail.
  if (!thisServer || !entry || !(entry->server)) return PR_TRUE;

  entry->server->GetKey(getter_Copies(serverKey));
  nsXPIDLCString thisServerKey;
  thisServer->GetKey(getter_Copies(thisServerKey));
  if (PL_strcmp(serverKey, thisServerKey)==0) {
    // add all these elements to the nsISupports array
    nsCOMPtr<nsISupportsArray> theseIdentities;
    rv = account->GetIdentities(getter_AddRefs(theseIdentities));
    if (NS_SUCCEEDED(rv))
      rv = entry->identities->AppendElements(theseIdentities);
  }

  return PR_TRUE;
}

NS_IMETHODIMP
nsMsgAccountManager::GetServersForIdentity(nsIMsgIdentity *identity,
                                           nsISupportsArray **_retval)
{
  nsresult rv;
  rv = LoadAccounts();
  if (NS_FAILED(rv)) return rv;
  
  nsCOMPtr<nsISupportsArray> servers;
  rv = NS_NewISupportsArray(getter_AddRefs(servers));
  if (NS_FAILED(rv)) return rv;
  
  findServersByIdentityEntry serverInfo;
  serverInfo.identity = identity;
  serverInfo.servers = servers;
  
  m_accounts->EnumerateForwards(findServersForIdentity,
                                (void *)&serverInfo);

  // do an addref for the caller.
  *_retval = servers;
  NS_ADDREF(*_retval);

  return NS_OK;
}
  
PRBool
nsMsgAccountManager::findServersForIdentity(nsISupports *element, void *aData)
{
  nsresult rv;
  nsCOMPtr<nsIMsgAccount> account = do_QueryInterface(element, &rv);
  if (NS_FAILED(rv)) return PR_TRUE;
  
  findServersByIdentityEntry *entry = (findServersByIdentityEntry*)aData;

  nsCOMPtr<nsISupportsArray> identities;
  account->GetIdentities(getter_AddRefs(identities));

  PRUint32 idCount=0;
  identities->Count(&idCount);

  PRUint32 id;
  nsXPIDLCString identityKey;
  rv = entry->identity->GetKey(getter_Copies(identityKey));

  
  for (id=0; id<idCount; id++) {

    // convert supports->Identity
    nsCOMPtr<nsISupports> thisSupports;
    rv = identities->GetElementAt(id, getter_AddRefs(thisSupports));
    if (NS_FAILED(rv)) continue;
    
    nsCOMPtr<nsIMsgIdentity>
      thisIdentity = do_QueryInterface(thisSupports, &rv);

    if (NS_SUCCEEDED(rv)) {

      nsXPIDLCString thisIdentityKey;
      rv = thisIdentity->GetKey(getter_Copies(thisIdentityKey));

      if (NS_SUCCEEDED(rv) && PL_strcmp(identityKey, thisIdentityKey) == 0) {
        nsCOMPtr<nsIMsgIncomingServer> thisServer;
        rv = account->GetIncomingServer(getter_AddRefs(thisServer));
        
        if (thisServer && NS_SUCCEEDED(rv)) {
          entry->servers->AppendElement(thisServer);
          break;
        }
        
      }
    }
  }

  return PR_TRUE;
}

nsresult
nsMsgAccountManager::AddRootFolderListener(nsIFolderListener *aListener)
{
  nsresult rv;
  NS_ENSURE_TRUE(aListener, NS_OK);
  // first add listener to the list
  rv = mFolderListeners->AppendElement(aListener);
  
  // now add the listener to all loaded accounts
  m_incomingServers.Enumerate(addListener, (void *)aListener);

  return NS_OK;
}

nsresult
nsMsgAccountManager::RemoveRootFolderListener(nsIFolderListener *aListener)
{
  nsresult rv;
  NS_ENSURE_TRUE(aListener, NS_OK);
  // remove the listener from the notification list
  rv = mFolderListeners->RemoveElement(aListener);
  
  // remove the listener from the individual folders
  m_incomingServers.Enumerate(removeListener, (void *)aListener);

  return NS_OK;
}

// enumeration functions
PRBool
nsMsgAccountManager::addListener(nsHashKey *aKey, void *element, void *aData)
{
  nsIMsgIncomingServer *server = (nsIMsgIncomingServer *)element;
  nsIFolderListener* listener = (nsIFolderListener *)aData;

  nsresult rv;
  
  nsCOMPtr<nsIMsgFolder> rootFolder;
  rv = server->GetRootFolder(getter_AddRefs(rootFolder));
  NS_ENSURE_SUCCESS(rv, PR_TRUE);

  rv = rootFolder->AddFolderListener(listener);
  NS_ENSURE_SUCCESS(rv, PR_TRUE);
  
  return PR_TRUE;
}

PRBool
nsMsgAccountManager::removeListener(nsHashKey *aKey, void *element, void *aData)
{
  nsIMsgIncomingServer *server = (nsIMsgIncomingServer *)element;
  nsIFolderListener* listener = (nsIFolderListener *)aData;

  nsresult rv;
  
  nsCOMPtr<nsIMsgFolder> rootFolder;
  rv = server->GetRootFolder(getter_AddRefs(rootFolder));
  NS_ENSURE_SUCCESS(rv, PR_TRUE);

  rv = rootFolder->RemoveFolderListener(listener);
  NS_ENSURE_SUCCESS(rv, PR_TRUE);
  
  return PR_TRUE;
}

NS_IMETHODIMP nsMsgAccountManager::SetLocalFoldersServer(nsIMsgIncomingServer *aServer)
{
	nsresult rv;
	if (!aServer) return NS_ERROR_NULL_POINTER;

	nsXPIDLCString key;
	rv = aServer->GetKey(getter_Copies(key));
	if (NS_FAILED(rv)) return rv;
	
	rv = m_prefs->SetCharPref(PREF_MAIL_ACCOUNTMANAGER_LOCALFOLDERSSERVER, (const char *)key);
	return rv;
}

NS_IMETHODIMP nsMsgAccountManager::GetLocalFoldersServer(nsIMsgIncomingServer **aServer)
{
	nsXPIDLCString serverKey;
	nsresult rv;

	if (!aServer) return NS_ERROR_NULL_POINTER;

  if (!m_prefs) {
    rv = getPrefService();
    NS_ENSURE_SUCCESS(rv,rv);
  }
	rv = m_prefs->GetCharPref(PREF_MAIL_ACCOUNTMANAGER_LOCALFOLDERSSERVER, getter_Copies(serverKey));

	if (NS_SUCCEEDED(rv) && ((const char *)serverKey)) {
		rv = GetIncomingServer(serverKey, aServer);
		if (!*aServer) return NS_ERROR_FAILURE;
		return rv;
	}

	// try ("nobody","Local Folders","none"), and work down to any "none" server.
	rv = FindServer("nobody","Local Folders","none",aServer);
	if (NS_FAILED(rv) || !*aServer) {
		rv = FindServer("nobody",nsnull,"none",aServer);
		if (NS_FAILED(rv) || !*aServer) {
			rv = FindServer(nsnull,"Local Folders","none",aServer);	
			if (NS_FAILED(rv) || !*aServer) {
				rv = FindServer(nsnull,nsnull,"none",aServer);
			}
		}
	}

	if (NS_FAILED(rv)) return rv;
	if (!*aServer) return NS_ERROR_FAILURE;
	
	rv = SetLocalFoldersServer(*aServer);
	return rv;
}
  // nsIUrlListener methods

NS_IMETHODIMP
nsMsgAccountManager::OnStartRunningUrl(nsIURI * aUrl)
{
    return NS_OK;
}

NS_IMETHODIMP
nsMsgAccountManager::OnStopRunningUrl(nsIURI * aUrl, nsresult aExitCode)
{
  if (aUrl)
  {
    nsCOMPtr<nsIImapUrl> imapUrl = do_QueryInterface(aUrl); 
    if (imapUrl)
    {
      nsImapAction imapAction = nsIImapUrl::nsImapTest;
      imapUrl->GetImapAction(&imapAction);
      switch(imapAction)
      {
        case nsIImapUrl::nsImapExpungeFolder:
          if (m_folderDoingCleanupInbox)
          {
            PR_CEnterMonitor(m_folderDoingCleanupInbox);
            PR_CNotifyAll(m_folderDoingCleanupInbox);
            m_cleanupInboxInProgress = PR_FALSE;
            PR_CExitMonitor(m_folderDoingCleanupInbox);
            m_folderDoingCleanupInbox=nsnull;   //reset to nsnull
          }
          break;
        case nsIImapUrl::nsImapDeleteAllMsgs:
          if (m_folderDoingEmptyTrash) 
          {
            PR_CEnterMonitor(m_folderDoingEmptyTrash);
            PR_CNotifyAll(m_folderDoingEmptyTrash);
            m_emptyTrashInProgress = PR_FALSE;
            PR_CExitMonitor(m_folderDoingEmptyTrash);
            m_folderDoingEmptyTrash = nsnull;  //reset to nsnull;
          }
          break;
        default:
          break;
       } 
     }
   }
   return NS_OK;
}

NS_IMETHODIMP
nsMsgAccountManager::SetFolderDoingEmptyTrash(nsIMsgFolder *folder)
{
  m_folderDoingEmptyTrash = folder;
  m_emptyTrashInProgress = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP
nsMsgAccountManager::GetEmptyTrashInProgress(PRBool *bVal)
{
  NS_ENSURE_ARG_POINTER(bVal);
  *bVal = m_emptyTrashInProgress;
  return NS_OK;
}

NS_IMETHODIMP
nsMsgAccountManager::SetFolderDoingCleanupInbox(nsIMsgFolder *folder)
{
  m_folderDoingCleanupInbox = folder;
  m_cleanupInboxInProgress = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP
nsMsgAccountManager::GetCleanupInboxInProgress(PRBool *bVal)
{
  NS_ENSURE_ARG_POINTER(bVal);
  *bVal = m_cleanupInboxInProgress;
  return NS_OK;
}
nsresult 
nsMsgAccountManager::SetLastServerFound(nsIMsgIncomingServer *server, const char *hostname, const char *username, const char *type)
{
    m_lastFindServerResult = server;
    m_lastFindServerHostName = hostname;
    m_lastFindServerUserName = username;
    m_lastFindServerType = type;
    
    return NS_OK;
}

NS_IMETHODIMP
nsMsgAccountManager::SaveAccountInfo()
{
  nsresult rv;
  nsCOMPtr<nsIPrefService> pref(do_GetService(NS_PREFSERVICE_CONTRACTID, &rv));
  NS_ENSURE_SUCCESS(rv,rv);
  return pref->SavePrefFile(nsnull);
}

NS_IMETHODIMP
nsMsgAccountManager::GetChromePackageName(const char *aExtensionName, char **aChromePackageName)
{
  NS_ENSURE_ARG_POINTER(aExtensionName);
  NS_ENSURE_ARG_POINTER(aChromePackageName);
 
  nsresult rv;
  nsCOMPtr<nsICategoryManager> catman = do_GetService(NS_CATEGORYMANAGER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv,rv);

  nsCOMPtr<nsISimpleEnumerator> e;
  rv = catman->EnumerateCategory(MAILNEWS_ACCOUNTMANAGER_EXTENSIONS, getter_AddRefs(e));
  if(NS_SUCCEEDED(rv) && e) {
    while (PR_TRUE) {
      nsCOMPtr<nsISupportsCString> catEntry;
      rv = e->GetNext(getter_AddRefs(catEntry));
      if (NS_FAILED(rv) || !catEntry) 
        break;

      nsCAutoString entryString;
      rv = catEntry->GetData(entryString);
      if (NS_FAILED(rv))
         break;

      nsXPIDLCString contractidString;
      rv = catman->GetCategoryEntry(MAILNEWS_ACCOUNTMANAGER_EXTENSIONS, entryString.get(), getter_Copies(contractidString));
      if (NS_FAILED(rv)) 
        break;

      nsCOMPtr <nsIMsgAccountManagerExtension> extension = do_GetService(contractidString.get(), &rv);
      if (NS_FAILED(rv) || !extension)
        break;
      
      nsXPIDLCString name;
      rv = extension->GetName(getter_Copies(name));
      if (NS_FAILED(rv))
        break;
      
      if (!strcmp(name.get(), aExtensionName))
        return extension->GetChromePackageName(aChromePackageName);
    }
  }
  return NS_ERROR_UNEXPECTED;
}

class VirtualFolderChangeListener : public nsIDBChangeListener
{
public:
  VirtualFolderChangeListener();
  ~VirtualFolderChangeListener() {};

  NS_DECL_ISUPPORTS
  NS_DECL_NSIDBCHANGELISTENER

  nsresult Init();

  nsCOMPtr <nsIMsgFolder> m_virtualFolder; // folder we're listening to db changes on behalf of.
  nsCOMPtr <nsIMsgFolder> m_folderWatching; // folder whose db we're listening to.
  nsCOMPtr <nsISupportsArray> m_searchTerms;
  nsCOMPtr <nsIMsgSearchSession> m_searchSession;
  PRBool m_searchOnMsgStatus;
};

NS_IMPL_ISUPPORTS1(VirtualFolderChangeListener, nsIDBChangeListener)

VirtualFolderChangeListener::VirtualFolderChangeListener() : m_searchOnMsgStatus(PR_FALSE)
{
}

nsresult VirtualFolderChangeListener::Init()
{
  nsCOMPtr <nsIMsgDatabase> msgDB;
  nsCOMPtr <nsIDBFolderInfo> dbFolderInfo;

  nsresult rv = m_virtualFolder->GetDBFolderInfoAndDB(getter_AddRefs(dbFolderInfo), getter_AddRefs(msgDB));
  if (NS_SUCCEEDED(rv) && msgDB)
  {
    nsXPIDLCString searchTermString;
    dbFolderInfo->GetCharPtrProperty("searchStr", getter_Copies(searchTermString));
    nsCOMPtr<nsIMsgFilterService> filterService = do_GetService(NS_MSGFILTERSERVICE_CONTRACTID, &rv);
    nsCOMPtr<nsIMsgFilterList> filterList;
    rv = filterService->GetTempFilterList(m_virtualFolder, getter_AddRefs(filterList));
    NS_ENSURE_SUCCESS(rv, rv);
    nsCOMPtr <nsIMsgFilter> tempFilter;
    filterList->CreateFilter(NS_LITERAL_STRING("temp").get(), getter_AddRefs(tempFilter));
    NS_ENSURE_SUCCESS(rv, rv);
    filterList->ParseCondition(tempFilter, searchTermString);
    NS_ENSURE_SUCCESS(rv, rv);
    m_searchSession = do_CreateInstance(NS_MSGSEARCHSESSION_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsISupportsArray> searchTerms;
    rv = tempFilter->GetSearchTerms(getter_AddRefs(searchTerms));
    NS_ENSURE_SUCCESS(rv, rv);

    m_searchSession->AddScopeTerm(nsMsgSearchScope::offlineMail, m_folderWatching);

    // add each item in termsArray to the search session
    PRUint32 numTerms;
    searchTerms->Count(&numTerms);
    for (PRUint32 i = 0; i < numTerms; i++)
    {
      nsCOMPtr <nsIMsgSearchTerm> searchTerm (do_QueryElementAt(searchTerms, i));
      nsMsgSearchAttribValue attrib;
      searchTerm->GetAttrib(&attrib);
      if (attrib == nsMsgSearchAttrib::MsgStatus)
        m_searchOnMsgStatus = true;
      m_searchSession->AppendTerm(searchTerm);
    }
  }
  return rv;
}

  /**
   * nsIDBChangeListener
   */
NS_IMETHODIMP VirtualFolderChangeListener::OnKeyChange(nsMsgKey aKeyChanged, PRUint32 aOldFlags, PRUint32 aNewFlags, nsIDBChangeListener *aInstigator)
{
  nsCOMPtr <nsIMsgDatabase> msgDB;

  nsresult rv = m_folderWatching->GetMsgDatabase(nsnull, getter_AddRefs(msgDB));
  nsCOMPtr <nsIMsgDBHdr> msgHdr;
  rv = msgDB->GetMsgHdrForKey(aKeyChanged, getter_AddRefs(msgHdr));
  NS_ENSURE_SUCCESS(rv, rv);
  PRBool oldMatch = PR_FALSE, newMatch = PR_FALSE;
  rv = m_searchSession->MatchHdr(msgHdr, msgDB, &newMatch);
  NS_ENSURE_SUCCESS(rv, rv);
  if (m_searchOnMsgStatus)
  {
    // if status is a search criteria, check if the header matched before
    // it changed, in order to determine if we need to bump the counts. 
    msgHdr->SetFlags(aOldFlags);
    rv = m_searchSession->MatchHdr(msgHdr, msgDB, &oldMatch);
    msgHdr->SetFlags(aNewFlags); // restore new flags even on match failure.
    NS_ENSURE_SUCCESS(rv, rv);
  }
  else
    oldMatch = newMatch;
  // we don't want to change the total counts if this virtual folder is open in a view,
  // because we won't remove the header from view while its open.
  if (oldMatch != newMatch || (oldMatch && (aOldFlags & MSG_FLAG_READ) != (aNewFlags & MSG_FLAG_READ)))
  {
    nsCOMPtr <nsIMsgDatabase> virtDatabase;
    nsCOMPtr <nsIDBFolderInfo> dbFolderInfo;

    rv = m_virtualFolder->GetDBFolderInfoAndDB(getter_AddRefs(dbFolderInfo), getter_AddRefs(virtDatabase));
    PRInt32 totalDelta = 0,  unreadDelta = 0;
    if (oldMatch != newMatch)
      totalDelta = (oldMatch) ? -1 : 1;
    PRBool msgHdrIsRead;
    msgHdr->GetIsRead(&msgHdrIsRead);
    if (oldMatch == newMatch) // read flag changed state
      unreadDelta = (msgHdrIsRead) ? -1 : 1; 
    else if (oldMatch) // else header should removed
      unreadDelta = (aOldFlags & MSG_FLAG_READ) ? 0 : -1;
    else               // header should be added
      unreadDelta = (aNewFlags & MSG_FLAG_READ) ? 0 : 1;
    if (unreadDelta)
      dbFolderInfo->ChangeNumUnreadMessages(unreadDelta);
    if (totalDelta)
      dbFolderInfo->ChangeNumMessages(totalDelta);
    m_virtualFolder->UpdateSummaryTotals(PR_TRUE); // force update from db.
    virtDatabase->Commit(nsMsgDBCommitType::kLargeCommit);
  }
  return rv;
}

NS_IMETHODIMP VirtualFolderChangeListener::OnKeyDeleted(nsMsgKey aKeyDeleted, nsMsgKey aParentKey, PRInt32 aFlags, nsIDBChangeListener *aInstigator)
{
  nsCOMPtr <nsIMsgDatabase> msgDB;

  nsresult rv = m_folderWatching->GetMsgDatabase(nsnull, getter_AddRefs(msgDB));
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr <nsIMsgDBHdr> msgHdr;
  rv = msgDB->GetMsgHdrForKey(aKeyDeleted, getter_AddRefs(msgHdr));
  NS_ENSURE_SUCCESS(rv, rv);
  PRBool match = PR_FALSE;
  rv = m_searchSession->MatchHdr(msgHdr, msgDB, &match);
  if (match)
  {
    nsCOMPtr <nsIMsgDatabase> virtDatabase;
    nsCOMPtr <nsIDBFolderInfo> dbFolderInfo;

    nsresult rv = m_virtualFolder->GetDBFolderInfoAndDB(getter_AddRefs(dbFolderInfo), getter_AddRefs(virtDatabase));
    PRBool msgHdrIsRead;
    msgHdr->GetIsRead(&msgHdrIsRead);
    if (!msgHdrIsRead)
      dbFolderInfo->ChangeNumUnreadMessages(-1);
    dbFolderInfo->ChangeNumMessages(-1);
    m_virtualFolder->UpdateSummaryTotals(PR_TRUE); // force update from db.
    virtDatabase->Commit(nsMsgDBCommitType::kLargeCommit);
  }
  return rv;
}

NS_IMETHODIMP VirtualFolderChangeListener::OnKeyAdded(nsMsgKey aNewKey, nsMsgKey aParentKey, PRInt32 aFlags, nsIDBChangeListener *aInstigator)
{
  nsCOMPtr <nsIMsgDatabase> msgDB;

  nsresult rv = m_folderWatching->GetMsgDatabase(nsnull, getter_AddRefs(msgDB));
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr <nsIMsgDBHdr> msgHdr;
  rv = msgDB->GetMsgHdrForKey(aNewKey, getter_AddRefs(msgHdr));
  NS_ENSURE_SUCCESS(rv, rv);
  PRBool match = PR_FALSE;
  rv = m_searchSession->MatchHdr(msgHdr, msgDB, &match);
  if (match)
  {
    nsCOMPtr <nsIMsgDatabase> virtDatabase;
    nsCOMPtr <nsIDBFolderInfo> dbFolderInfo;

    rv = m_virtualFolder->GetDBFolderInfoAndDB(getter_AddRefs(dbFolderInfo), getter_AddRefs(virtDatabase));
    PRBool msgHdrIsRead;
    msgHdr->GetIsRead(&msgHdrIsRead);
    if (!msgHdrIsRead)
      dbFolderInfo->ChangeNumUnreadMessages(1);
    dbFolderInfo->ChangeNumMessages(1);
    m_virtualFolder->UpdateSummaryTotals(true); // force update from db.
    virtDatabase->Commit(nsMsgDBCommitType::kLargeCommit);
  }
  return rv;
}

NS_IMETHODIMP VirtualFolderChangeListener::OnParentChanged(nsMsgKey aKeyChanged, nsMsgKey oldParent, nsMsgKey newParent, nsIDBChangeListener *aInstigator)
{
  return NS_OK;
}

NS_IMETHODIMP VirtualFolderChangeListener::OnAnnouncerGoingAway(nsIDBChangeAnnouncer *instigator)
{
  nsresult rv;
  nsCOMPtr<nsIMsgDBService> msgDBService = do_GetService(NS_MSGDB_SERVICE_CONTRACTID, &rv);
  if (msgDBService)
    msgDBService->UnregisterPendingListener(this);
  return rv;
}

NS_IMETHODIMP VirtualFolderChangeListener::OnReadChanged(nsIDBChangeListener *aInstigator)
{
  return NS_OK;
}

NS_IMETHODIMP VirtualFolderChangeListener::OnJunkScoreChanged(nsIDBChangeListener *aInstigator)
{
  return NS_OK;
}

nsresult nsMsgAccountManager::GetVirtualFoldersFile(nsCOMPtr<nsILocalFile>& file)
{
  nsCOMPtr<nsIFile> profileDir;
  nsresult rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR, getter_AddRefs(profileDir));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = profileDir->AppendNative(nsDependentCString("virtualFolders.dat"));
  if (NS_SUCCEEDED(rv))
    file = do_QueryInterface(profileDir, &rv);
  return rv;
}

nsresult nsMsgAccountManager::LoadVirtualFolders()
{
  nsCOMPtr <nsILocalFile> file;
  GetVirtualFoldersFile(file);
  if (!file)
    return NS_ERROR_FAILURE;

  nsresult rv;
  nsCOMPtr<nsIMsgDBService> msgDBService = do_GetService(NS_MSGDB_SERVICE_CONTRACTID, &rv);
  if (msgDBService) 
  {
     NS_ENSURE_SUCCESS(rv, rv);
     nsCOMPtr<nsIFileInputStream> fileStream = do_CreateInstance(NS_LOCALFILEINPUTSTREAM_CONTRACTID, &rv);
     NS_ENSURE_SUCCESS(rv, rv);

     rv = fileStream->Init(file,  PR_RDONLY, 0664, PR_FALSE);  //just have to read the messages
     nsCOMPtr <nsILineInputStream> lineInputStream(do_QueryInterface(fileStream));

    PRBool isMore = PR_TRUE;
    nsCAutoString buffer;

    while (isMore &&
           NS_SUCCEEDED(lineInputStream->ReadLine(buffer, &isMore)))
    {
      if (buffer.Length() > 0)
      {
        nsCOMPtr <nsIMsgFolder> folder;
        nsCOMPtr<nsIRDFService> rdf(do_GetService("@mozilla.org/rdf/rdf-service;1", &rv));
        NS_ENSURE_SUCCESS(rv, rv);

        nsCOMPtr<nsIRDFResource> resource;
        rv = rdf->GetResource(buffer, getter_AddRefs(resource));
        NS_ENSURE_SUCCESS(rv, rv);

        folder = do_QueryInterface(resource, &rv);
        NS_ENSURE_SUCCESS(rv, rv);
        if (folder)
        {
          // need to add the folder as a sub-folder of its parent.
          PRInt32 lastSlash = buffer.RFindChar('/');
          nsDependentCSubstring parentUri(buffer, 0, lastSlash);
          rdf->GetResource(parentUri, getter_AddRefs(resource));
          nsCOMPtr <nsIMsgFolder> parentFolder = do_QueryInterface(resource);
          if (parentFolder)
          {
            nsCOMPtr <nsIMsgFolder> childFolder;
            nsAutoString currentFolderNameStr;
            CopyUTF8toUTF16(Substring(buffer,  lastSlash + 1, buffer.Length()), currentFolderNameStr);
            rv =  parentFolder->AddSubfolder(currentFolderNameStr, getter_AddRefs(childFolder));
            if (!childFolder)
              childFolder = folder; // or just use folder?

            nsCOMPtr <nsIMsgDatabase> db;
            childFolder->GetMsgDatabase(nsnull, getter_AddRefs(db)); // force db to get created.
            nsCOMPtr <nsIDBFolderInfo> dbFolderInfo;
            rv = db->GetDBFolderInfo(getter_AddRefs(dbFolderInfo));
            childFolder->SetFlag(MSG_FOLDER_FLAG_VIRTUAL);
            nsXPIDLCString realFolderUri;
            dbFolderInfo->GetCharPtrProperty("searchFolderUri", getter_Copies(realFolderUri));
            // if we supported cross folder virtual folders, we'd have a list of folders uris,
            // and we'd have to add a pending listener for each of them.
            if (realFolderUri.Length())
            {
              // we need to load the db for the actual folder so that many hdrs to download
              // will return false...
              rdf->GetResource(realFolderUri, getter_AddRefs(resource));
              nsCOMPtr <nsIMsgFolder> realFolder = do_QueryInterface(resource);
              VirtualFolderChangeListener *dbListener = new VirtualFolderChangeListener();
              m_virtualFolderListeners.AppendObject(dbListener);
              dbListener->m_virtualFolder = childFolder;
              dbListener->m_folderWatching = realFolder;
              dbListener->Init();
              msgDBService->RegisterPendingListener(realFolder, dbListener);
            }
          }
        }
      }
    }
  }
  return rv;
}



NS_IMETHODIMP nsMsgAccountManager::SaveVirtualFolders()
{

  nsCOMPtr<nsISupportsArray> allServers;
  nsresult rv = GetAllServers(getter_AddRefs(allServers));
  nsCOMPtr <nsILocalFile> file;
  if (allServers)
  {
    PRUint32 count = 0;
    allServers->Count(&count);
    PRInt32 i;
    nsCOMPtr <nsIOutputStream> outputStream;
    for (i = 0; i < count; i++) 
    {
      nsCOMPtr<nsIMsgIncomingServer> server = do_QueryElementAt(allServers, i);
      if (server)
      {
        nsCOMPtr <nsIMsgFolder> rootFolder;
        server->GetRootFolder(getter_AddRefs(rootFolder));
        if (rootFolder)
        {
          nsCOMPtr <nsISupportsArray> virtualFolders;
          rv = rootFolder->GetAllFoldersWithFlag(MSG_FOLDER_FLAG_VIRTUAL, getter_AddRefs(virtualFolders));
          NS_ENSURE_SUCCESS(rv, rv);
          PRUint32 vfCount;
          virtualFolders->Count(&vfCount);
          for (PRInt32 folderIndex = 0; folderIndex < vfCount; folderIndex++)
          {
            if (!outputStream)
            {
              GetVirtualFoldersFile(file);
              rv = NS_NewLocalFileOutputStream(getter_AddRefs(outputStream),
                                               file,
                                               PR_CREATE_FILE | PR_WRONLY | PR_TRUNCATE,
                                               0664);
            }
            nsCOMPtr <nsIRDFResource> folderRes (do_QueryElementAt(virtualFolders, folderIndex));
            const char *uri;
            folderRes->GetValueConst(&uri);
            PRUint32 writeCount;
            outputStream->Write(uri, strlen(uri), &writeCount);
            outputStream->Write("\n", 1, &writeCount);
          }
        }
      }
   }
   if (outputStream)
    outputStream->Close();
  }
  return rv;
}


NS_IMETHODIMP nsMsgAccountManager::OnItemAdded(nsIRDFResource *parentItem, nsISupports *item)
{
  nsCOMPtr<nsIMsgFolder> folder = do_QueryInterface(item);
  NS_ENSURE_TRUE(folder, NS_OK); // just kick out with a success code if the item in question is not a folder
  PRUint32 folderFlags;
  folder->GetFlags(&folderFlags);
  nsresult rv = NS_OK;
  // need to make sure this isn't happening during loading of virtualfolders.dat
  if (folderFlags & MSG_FOLDER_FLAG_VIRTUAL)
  {
    // When a new virtual folder is added, need to create a db Listener for it.
    nsCOMPtr<nsIMsgDBService> msgDBService = do_GetService(NS_MSGDB_SERVICE_CONTRACTID, &rv);
    if (msgDBService)
    {
      VirtualFolderChangeListener *dbListener = new VirtualFolderChangeListener();
      dbListener->m_virtualFolder = folder;
      nsCOMPtr <nsIMsgDatabase> virtDatabase;
      nsCOMPtr <nsIDBFolderInfo> dbFolderInfo;
      m_virtualFolderListeners.AppendObject(dbListener);

      rv = folder->GetDBFolderInfoAndDB(getter_AddRefs(dbFolderInfo), getter_AddRefs(virtDatabase));
      NS_ENSURE_SUCCESS(rv, rv);
      nsXPIDLCString srchFolderUri;
      dbFolderInfo->GetCharPtrProperty("searchFolderUri", getter_Copies(srchFolderUri));
      // if we supported cross folder virtual folders, we'd have a list of folders uris,
      // and we'd have to add a pending listener for each of them.
      rv = GetExistingFolder(srchFolderUri.get(), getter_AddRefs(dbListener->m_folderWatching));
      if (dbListener->m_folderWatching)
        msgDBService->RegisterPendingListener(dbListener->m_folderWatching, dbListener);
    }
    rv = SaveVirtualFolders();
  }
  return rv;
}

NS_IMETHODIMP nsMsgAccountManager::OnItemRemoved(nsIRDFResource *parentItem, nsISupports *item)
{
  nsCOMPtr<nsIMsgFolder> folder = do_QueryInterface(item);
  NS_ENSURE_TRUE(folder, NS_OK); // just kick out with a success code if the item in question is not a folder
  nsresult rv = NS_OK;
  PRUint32 folderFlags;
  folder->GetFlags(&folderFlags);
  if (folderFlags & MSG_FOLDER_FLAG_VIRTUAL) // if we removed a VF, flush VF list to disk.
    rv = SaveVirtualFolders();

  // need to check if the underlying folder for a VF was removed, in which case we need to
  // remove the virtual folder.

 return rv;
}

NS_IMETHODIMP nsMsgAccountManager::OnItemPropertyChanged(nsIRDFResource *item, nsIAtom *property, const char *oldValue, const char *newValue)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMsgAccountManager::OnItemIntPropertyChanged(nsIRDFResource *item, nsIAtom *property, PRInt32 oldValue, PRInt32 newValue)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMsgAccountManager::OnItemBoolPropertyChanged(nsIRDFResource *item, nsIAtom *property, PRBool oldValue, PRBool newValue)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMsgAccountManager::OnItemUnicharPropertyChanged(nsIRDFResource *item, nsIAtom *property, const PRUnichar *oldValue, const PRUnichar *newValue)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP nsMsgAccountManager::OnItemPropertyFlagChanged(nsISupports *item, nsIAtom *property, PRUint32 oldFlag, PRUint32 newFlag)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMsgAccountManager::OnItemEvent(nsIMsgFolder *aFolder, nsIAtom *aEvent)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}
