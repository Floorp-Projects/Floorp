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

#include "nsIMsgAccountManager.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsISupportsArray.h"
#include "nsMsgAccountManager.h"
#include "nsHashtable.h"
#include "nsMsgBaseCID.h"
#include "nsIPref.h"
#include "nsCOMPtr.h"
#include "prmem.h"
#include "plstr.h"
#include "nsString.h"
#include "nsXPIDLString.h"
#include "nsIMsgBiffManager.h"
#include "nscore.h"
#include "nsIProfile.h"
#include "nsCRT.h"  // for nsCRT::strtok
#include "prprf.h"
#include "nsINetSupportDialogService.h"
#include "nsIMsgFolderCache.h"
#include "nsFileStream.h"
#include "nsMsgUtils.h"
#include "nsSpecialSystemDirectory.h"

// this should eventually be moved to the pop3 server for upgrading
#include "nsIPop3IncomingServer.h"
// this should eventually be moved to the imap server for upgrading
#include "nsIImapIncomingServer.h"
// this should eventually be moved to the nntp server for upgrading
#include "nsINntpIncomingServer.h"

#define BUF_STR_LEN 1024

#if defined(DEBUG_alecf) || defined(DEBUG_sspitzer) || defined(DEBUG_seth)
#define DEBUG_ACCOUNTMANAGER 1
#endif

static NS_DEFINE_CID(kMsgAccountCID, NS_MSGACCOUNT_CID);
static NS_DEFINE_CID(kMsgIdentityCID, NS_MSGIDENTITY_CID);
static NS_DEFINE_CID(kPrefServiceCID, NS_PREF_CID);
static NS_DEFINE_CID(kMsgBiffManagerCID, NS_MSGBIFFMANAGER_CID);
static NS_DEFINE_CID(kProfileCID, NS_PROFILE_CID);
static NS_DEFINE_CID(kCNetSupportDialogCID, NS_NETSUPPORTDIALOG_CID);

#define PREF_MAIL_ACCOUNTMANAGER_ACCOUNTS "mail.accountmanager.accounts"
#define PREF_NEWS_DIRECTORY "news.directory"

/* we are going to clear these after migration */
#define PREF_4X_MAIL_IDENTITY_USEREMAIL "mail.identity.useremail"
#define PREF_4X_MAIL_IDENTITY_USERNAME "mail.identity.username"
#define PREF_4X_MAIL_IDENTITY_REPLY_TO "mail.identity.reply_to"    
#define PREF_4X_MAIL_IDENTITY_ORGANIZATION "mail.identity.organization"
#define PREF_4X_MAIL_COMPOSE_HTML "mail.compose_html"
#define PREF_4X_MAIL_POP_NAME "mail.pop_name"
#define PREF_4X_MAIL_REMEMBER_PASSWORD "mail.remember_password"
#define PREF_4X_MAIL_POP_PASSWORD "mail.pop_password"
#define PREF_4X_NETWORK_HOSTS_POP_SERVER "network.hosts.pop_server"
#define PREF_4X_MAIL_CHECK_NEW_MAIL "mail.check_new_mail"
#define PREF_4X_MAIL_CHECK_TIME "mail.check_time"
#define PREF_4X_MAIL_LEAVE_ON_SERVER "mail.leave_on_server"
#define PREF_4X_MAIL_DELETE_MAIL_LEFT_ON_SERVER "mail.delete_mail_left_on_server"
#define PREF_4X_NETWORK_HOSTS_SMTP_SERVER "network.hosts.smtp_server"
#define PREF_4X_MAIL_SMTP_NAME "mail.smtp_name"
#define PREF_4X_MAIL_SERVER_TYPE "mail.server_type"
#define PREF_4X_NETWORK_HOSTS_IMAP_SERVER "network.hosts.imap_servers"

#ifdef DEBUG_seth
#define DEBUG_CLEAR_PREF 1
#endif

// use this to search for all servers with the given hostname/iid and
// put them in "servers"
typedef struct _findServerEntry {
  const char *hostname;
  const char *username;
  const char *type;
  nsIMsgIncomingServer *server;
} findServerEntry;


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

class nsMsgAccountManager : public nsIMsgAccountManager,
                            public nsIShutdownListener
{
public:

  nsMsgAccountManager();
  virtual ~nsMsgAccountManager();
  
  NS_DECL_ISUPPORTS

  /* nsIShutdownListener methods */

  NS_IMETHOD OnShutdown(const nsCID& aClass, nsISupports *service);
  
  /* nsIMsgAccountManager methods */
  
  NS_DECL_NSIMSGACCOUNTMANAGER
  
  //Add/remove an account to/from the Biff Manager if it has Biff turned on.
  nsresult AddServerToBiff(nsIMsgIncomingServer *account);
  nsresult RemoveServerFromBiff(nsIMsgIncomingServer *account);
private:

  PRBool m_accountsLoaded;
  
  nsISupportsArray *m_accounts;
  nsHashtable m_identities;
  nsHashtable m_incomingServers;
  nsCOMPtr<nsIMsgAccount> m_defaultAccount;

  nsCString accountKeyList;
  
  /* internal creation routines - updates m_identities and m_incomingServers */
  nsresult createKeyedAccount(const char* key,
                              nsIMsgAccount **_retval);
  nsresult createKeyedServer(const char*key,
                             const char* type,
                             nsIMsgIncomingServer **_retval);

  nsresult createKeyedIdentity(const char* key,
                               nsIMsgIdentity **_retval);
  
  // hash table enumerators


  //
  static PRBool hashElementToArray(nsHashKey *aKey, void *aData,
                                   void *closure);

  // called by EnumerateRemove to release all elements
  static PRBool hashElementRelease(nsHashKey *aKey, void *aData,
                                   void *closure);

  // remove all of the servers from the Biff Manager
  static PRBool removeServerFromBiff(nsHashKey *aKey, void *aData,
                                     void *closure);

  //
  // account enumerators
  // ("element" is always an account)
  //
  
  // append the account keys to the given string
  static PRBool getAccountList(nsISupports *aKey, void *aData);

  // find the identities that correspond to the given server
  static PRBool findIdentitiesForServer(nsISupports *element, void *aData);

  // find the servers that correspond to the given identity
  static PRBool findServersForIdentity (nsISupports *element, void *aData);

  // find the account with the given key
  static PRBool findAccountByKey (nsISupports *element, void *aData);

  // load up the servers into the given nsISupportsArray
  static PRBool getServersToArray(nsISupports *element, void *aData);

  // load up the identities into the given nsISupportsArray
  static PRBool getIdentitiesToArray(nsISupports *element, void *aData);

  // add identities if they don't alreadby exist in the given nsISupportsArray
  static PRBool addIdentityIfUnique(nsISupports *element, void *aData);
  
  //
  // server enumerators
  // ("element" is always a server)
  //
  
  // find the server given by {username, hostname, type}
  static PRBool findServer(nsISupports *aElement, void *data);

  // write out the server's cache through the given folder cache
  static PRBool writeFolderCache(nsHashKey *aKey, void *aData, void *closure);

  PRBool isUnique(nsIMsgIncomingServer *server);
  nsresult upgradePrefs();
  nsresult createSpecialFile(nsFileSpec & dir, const char *specialFileName);
  nsresult CopyIdentity(nsIMsgIdentity *srcIdentity, nsIMsgIdentity *destIdentity);
  
  PRInt32 MigrateImapAccounts(nsIMsgIdentity *identity);
  nsresult MigrateImapAccount(nsIMsgIdentity *identity, const char *hostname, PRInt32 accountNum);
  
  PRInt32 MigratePopAccounts(nsIMsgIdentity *identity);
  
  PRInt32 MigrateNewsAccounts(nsIMsgIdentity *identity, PRInt32 baseAccountNum);
  nsresult MigrateNewsAccount(nsIMsgIdentity *identity, const char *hostname, const char *newsrcfile, PRInt32 accountNum);

  nsresult getPrefService();
  nsIPref *m_prefs;
};


NS_IMPL_ADDREF(nsMsgAccountManager)
NS_IMPL_RELEASE(nsMsgAccountManager)
  
nsresult
nsMsgAccountManager::QueryInterface(const nsIID& iid, void **result)
{
  nsresult rv = NS_NOINTERFACE;
  if (! result)
    return NS_ERROR_NULL_POINTER;

  void *res = nsnull;
  if (iid.Equals(nsCOMTypeInfo<nsIMsgAccountManager>::GetIID()) ||
      iid.Equals(nsCOMTypeInfo<nsISupports>::GetIID()))
    res = NS_STATIC_CAST(nsIMsgAccountManager*, this);
  else if (iid.Equals(nsCOMTypeInfo<nsIShutdownListener>::GetIID()))
    res = NS_STATIC_CAST(nsIShutdownListener*, this);

  if (res) {
    NS_ADDREF(this);
    *result = res;
    rv = NS_OK;
  }

  return rv;
}

nsMsgAccountManager::nsMsgAccountManager() :
  m_accountsLoaded(PR_FALSE),
  m_defaultAccount(null_nsCOMPtr()),
  m_prefs(0)
{
  NS_INIT_REFCNT();
  NS_NewISupportsArray(&m_accounts);
}

nsMsgAccountManager::~nsMsgAccountManager()
{
  
  if (m_prefs) nsServiceManager::ReleaseService(kPrefServiceCID, m_prefs);
  UnloadAccounts();
  NS_IF_RELEASE(m_accounts);
}

nsresult
nsMsgAccountManager::getPrefService() {

  // get the prefs service
  nsresult rv = NS_OK;
  
  if (!m_prefs)
    rv = nsServiceManager::GetService(kPrefServiceCID,
                                      nsCOMTypeInfo<nsIPref>::GetIID(),
                                      (nsISupports**)&m_prefs,
                                      this);
  if (NS_FAILED(rv)) return rv;

  /* m_prefs is good now */
  return NS_OK;
}

/* called if the prefs service goes offline */
NS_IMETHODIMP
nsMsgAccountManager::OnShutdown(const nsCID& aClass, nsISupports *service)
{
  if (aClass.Equals(kPrefServiceCID)) {
    if (m_prefs) nsServiceManager::ReleaseService(kPrefServiceCID, m_prefs);
    m_prefs = nsnull;
  }

  return NS_OK;
}

nsresult
nsMsgAccountManager::CreateIdentity(nsIMsgIdentity **_retval)
{
  if (!_retval) return NS_ERROR_NULL_POINTER;

  /* XXX create an identity key that is unique within m_identities */
  const char *key="xxxx";

  return createKeyedIdentity(key, _retval);
}

nsresult
nsMsgAccountManager::GetIdentity(const char* key,
                                 nsIMsgIdentity **_retval)
{
  if (!_retval) return NS_ERROR_NULL_POINTER;

  nsresult rv;

  // check for the identity in the hash table
  nsStringKey hashKey(key);
  nsCOMPtr<nsISupports> idsupports = (nsISupports*)m_identities.Get(&hashKey);
  nsCOMPtr<nsIMsgIdentity> identity = do_QueryInterface(idsupports);

  if (identity) {
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
  rv = nsComponentManager::CreateInstance(NS_MSGIDENTITY_PROGID,
                                          nsnull,
                                          NS_GET_IID(nsIMsgIdentity),
                                          getter_AddRefs(identity));
  if (NS_FAILED(rv)) return rv;
  
  identity->SetKey(NS_CONST_CAST(char *,key));
  
  nsStringKey hashKey(key);

  // addref for the hash table
  nsISupports* idsupports = identity;
  NS_ADDREF(idsupports);
  m_identities.Put(&hashKey, (void *)idsupports);

  *aIdentity = identity;
  NS_ADDREF(*aIdentity);
  
  return NS_OK;
}

nsresult
nsMsgAccountManager::CreateIncomingServer(const char* type,
                                          nsIMsgIncomingServer **_retval)
{

  if (!_retval) return NS_ERROR_NULL_POINTER;
  /* XXX todo - create a key that is unique among m_incomingServers */
  const char *key = "xxxx";

  return createKeyedServer(key, type, _retval);

}

nsresult
nsMsgAccountManager::GetIncomingServer(const char* key,
                                       nsIMsgIncomingServer **_retval)
{
  if (!_retval) return NS_ERROR_NULL_POINTER;

  nsresult rv=NS_OK;
  
  nsStringKey hashKey(key);
  nsCOMPtr<nsIMsgIncomingServer> server =
    do_QueryInterface((nsISupports*)m_incomingServers.Get(&hashKey));

  if (server) {
    *_retval = server;
    NS_ADDREF(*_retval);
    return NS_OK;
  }

  // server doesn't exist, so create it
  
  // in order to create the right kind of server, we have to look
  // at the pref for this server to get the type
  const char *serverTypePref =
    PR_smprintf("mail.server.%s.type", key);

  // serverType is the short server type, like "pop3", "imap", etc
  nsXPIDLCString serverType;
  rv = m_prefs->CopyCharPref(serverTypePref, getter_Copies(serverType));
  
    // the server type doesn't exist. That's bad.
  if (NS_FAILED(rv))
    return NS_ERROR_NOT_INITIALIZED;

  rv = createKeyedServer(key, serverType, _retval);
  if (NS_FAILED(rv)) return rv;

  return rv;
}

/*
 * create a server when you know the key and the type
 */
nsresult
nsMsgAccountManager::createKeyedServer(const char* key,
                                       const char* type,
                                       nsIMsgIncomingServer ** aServer)
{
  nsresult rv;

  nsCOMPtr<nsIMsgIncomingServer> server;
  //construct the progid
  const char* serverProgID =
    PR_smprintf(NS_MSGINCOMINGSERVER_PROGID_PREFIX "%s", type);
  
  // finally, create the server
  rv = nsComponentManager::CreateInstance(serverProgID,
                                          nsnull,
                                          NS_GET_IID(nsIMsgIncomingServer),
                                          getter_AddRefs(server));
  PR_smprintf_free(NS_CONST_CAST(char*, serverProgID));
  if (NS_FAILED(rv)) return rv;
  
  server->SetKey(NS_CONST_CAST(char *,key));

  nsStringKey hashKey(key);

  // addref for the hashtable
  nsISupports* serversupports = server;
  NS_ADDREF(serversupports);
  m_incomingServers.Put(&hashKey, serversupports);

  // add to biff
  AddServerToBiff(server);

  *aServer = server;
  NS_ADDREF(*aServer);
  
  return NS_OK;
}

nsresult
nsMsgAccountManager::AddServerToBiff(nsIMsgIncomingServer *server)
{
	nsresult rv;
	PRBool doBiff = PR_FALSE;

	NS_WITH_SERVICE(nsIMsgBiffManager, biffManager, kMsgBiffManagerCID, &rv);

	if(NS_FAILED(rv))
		return rv;

    rv = server->GetDoBiff(&doBiff);

	if(NS_SUCCEEDED(rv) && doBiff)
	{
		rv = biffManager->AddServerBiff(server);
	}

	return rv;
}

nsresult
nsMsgAccountManager::RemoveServerFromBiff(nsIMsgIncomingServer *server)
{
	nsresult rv;
	PRBool doBiff = PR_FALSE;

	NS_WITH_SERVICE(nsIMsgBiffManager, biffManager, kMsgBiffManagerCID, &rv);

	if(NS_FAILED(rv))
		return rv;

    rv = server->GetDoBiff(&doBiff);

	if(NS_SUCCEEDED(rv) && doBiff)
	{
		rv = biffManager->RemoveServerBiff(server);
	}

	return rv;
}

NS_IMETHODIMP
nsMsgAccountManager::RemoveAccount(nsIMsgAccount *aAccount)
{
  nsresult rv;
  rv = LoadAccounts();
  if (NS_FAILED(rv)) return rv;
  
  rv = m_accounts->RemoveElement(aAccount);
#ifdef DEBUG_alecf
  if (NS_FAILED(rv))
    printf("error removing account. perhaps we need a NS_STATIC_CAST?\n");
#endif

  return NS_OK;
}

/* get the default account. If no default account, pick the first account */
NS_IMETHODIMP
nsMsgAccountManager::GetDefaultAccount(nsIMsgAccount * *aDefaultAccount)
{
  nsresult rv;
  rv = LoadAccounts();
  if (NS_FAILED(rv)) return rv;
  
  if (!aDefaultAccount) return NS_ERROR_NULL_POINTER;

  if (!m_defaultAccount) {
#ifdef DEBUG_ACCOUNTMANAGER
    printf("No default account. Looking for one..\n");
    PRUint32 count;
    m_accounts->Count(&count);
    printf("There are %d accounts\n", count);
#endif
    
    nsCOMPtr<nsISupports> element;
    rv = m_accounts->GetElementAt(0, getter_AddRefs(element));

    if (NS_SUCCEEDED(rv))
      m_defaultAccount = do_QueryInterface(element);
  }
  
  *aDefaultAccount = m_defaultAccount;
  NS_IF_ADDREF(*aDefaultAccount);
  return NS_OK;
}


NS_IMETHODIMP
nsMsgAccountManager::SetDefaultAccount(nsIMsgAccount * aDefaultAccount)
{
  // make sure it's in the account list
  
  m_defaultAccount = dont_QueryInterface(aDefaultAccount);
  return NS_OK;
}

PRBool
nsMsgAccountManager::isUnique(nsIMsgIncomingServer *server)
{
  nsresult rv;
  nsXPIDLCString username;
  nsXPIDLCString hostname;
  nsXPIDLCString type;
  
  // make sure this server is unique
  rv = server->GetUsername(getter_Copies(username));
  if (NS_FAILED(rv)) return rv;
    
  rv = server->GetHostName(getter_Copies(hostname));
  if (NS_FAILED(rv)) return rv;

  rv = server->GetType(getter_Copies(type));
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIMsgIncomingServer> dupeServer;
  rv = FindServer(username, hostname, type, getter_AddRefs(dupeServer));

  // if the server was not found, then it really is unique.
  if (NS_FAILED(rv)) return PR_TRUE;
  
  // if we're here, then we found it, but maybe it's literally the
  // same server that we're searching for.

  // return PR_FALSE on error because if one of the keys doesn't
  // exist, then it's likely a different server
  
  nsXPIDLCString dupeKey;
  rv = dupeServer->GetKey(getter_Copies(dupeKey));
  if (NS_FAILED(rv)) return PR_FALSE;
  
  nsXPIDLCString serverKey;
  server->GetKey(getter_Copies(serverKey));
  if (NS_FAILED(rv)) return PR_FALSE;
  
  if (!PL_strcmp(dupeKey, serverKey))
    // it's unique because this EXACT server is the only one that exists
    return PR_TRUE;
  else
    // there is already a server with this {username, hostname, type}
    return PR_FALSE;
}




// enumaration for removing accounts from the BiffManager
PRBool
nsMsgAccountManager::removeServerFromBiff(nsHashKey *aKey, void *aData,
                                          void *closure)
{
    nsCOMPtr<nsIMsgIncomingServer> server =
      do_QueryInterface((nsISupports*)aData);
	nsMsgAccountManager *accountManager = (nsMsgAccountManager*)closure;

	accountManager->RemoveServerFromBiff(server);
	
	return PR_TRUE;

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
  nsCOMPtr<nsIMsgIdentity> identity = do_QueryInterface(element);
  nsISupportsArray *array = (nsISupportsArray*)aData;

  nsresult rv;
  
  nsXPIDLCString key;
  rv = identity->GetKey(getter_Copies(key));
  if (NS_FAILED(rv)) return PR_TRUE;

  PRUint32 count=0;
  rv = array->Count(&count);
  if (NS_FAILED(rv)) return PR_TRUE;
  
  PRBool found=PR_FALSE;
  PRUint32 i;
  for (i=0; i<count; i++) {
    nsCOMPtr<nsISupports> element;
    array->GetElementAt(i, getter_AddRefs(element));

    nsCOMPtr<nsIMsgIdentity> thisIdentity =
      do_QueryInterface(element, &rv);
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
  nsCOMPtr<nsIMsgAccount> account = do_QueryInterface(element);
  
  nsresult rv;
  
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
  m_accounts->EnumerateForwards(getServersToArray,
                                (void *)(nsISupportsArray*)servers);
  *_retval = servers;
  NS_ADDREF(*_retval);
  return rv;
}

PRBool
nsMsgAccountManager::getServersToArray(nsISupports *element, void *aData)
{
  nsCOMPtr<nsIMsgAccount> account = do_QueryInterface(element);
  nsISupportsArray *array = (nsISupportsArray*)aData;

  nsresult rv;
  
  nsCOMPtr<nsIMsgIncomingServer> server;
  rv = account->GetIncomingServer(getter_AddRefs(server));

  if (NS_SUCCEEDED(rv)) 
    array->AppendElement(server);

  return PR_TRUE;
}

nsresult
nsMsgAccountManager::LoadAccounts()
{
  nsresult rv;

  // for now safeguard multiple calls to this function
  if (m_accountsLoaded)
    return NS_OK;
  
  m_accountsLoaded = PR_TRUE;
  // mail.accountmanager.accounts is the main entry point for all accounts

  nsXPIDLCString accountList;
  rv = getPrefService();
  if (NS_SUCCEEDED(rv)) {
    rv = m_prefs->CopyCharPref(PREF_MAIL_ACCOUNTMANAGER_ACCOUNTS,
                                        getter_Copies(accountList));
  }
  
  if (NS_FAILED(rv) || !accountList || !accountList[0]) {
#ifdef DEBUG_ACCOUNTMANAGER
    printf("No accounts. I'll try to migrate 4.x prefs..\n");
#endif
    rv = upgradePrefs();
    return rv;
  }

    /* parse accountList and run loadAccount on each string, comma-separated */
#ifdef DEBUG_ACCOUNTMANAGER
    printf("accountList = %s\n", (const char*)accountList);
#endif
   
    nsCOMPtr<nsIMsgAccount> account;
    char *token = nsnull;
    char *rest = NS_CONST_CAST(char*,(const char*)accountList);
    nsCAutoString str;

    token = nsCRT::strtok(rest, ",", &rest);
    while (token && *token) {
      str = token;
      str.StripWhitespace();
      
      if (!str.IsEmpty()) {
          rv = GetAccount(str.GetBuffer(), getter_AddRefs(account));
      }
      token = nsCRT::strtok(rest, ",", &rest);
    }

    /* finished loading accounts */
    return NS_OK;
}

PRBool
nsMsgAccountManager::getAccountList(nsISupports *element, void *aData)
{
  nsresult rv;
  nsCString* accountList = (nsCString*) aData;
  nsCOMPtr<nsIMsgAccount> account = do_QueryInterface(element);

  nsXPIDLCString key;
  rv = account->GetKey(getter_Copies(key));
  if (NS_FAILED(rv)) return PR_TRUE;

  if ((*accountList).IsEmpty())
    (*accountList) += key;
  else {
    (*accountList) += ',';
    (*accountList) += key;
  }

  return PR_TRUE;
}

nsresult
nsMsgAccountManager::UnloadAccounts()
{
  // release the default account
  m_defaultAccount=nsnull;
  m_incomingServers.Enumerate(removeServerFromBiff, this);

  m_accounts->Clear();          // will release all elements
  m_identities.Reset(hashElementRelease, nsnull);
  m_incomingServers.Reset(hashElementRelease, nsnull);
  m_accountsLoaded = PR_FALSE;
  return NS_OK;
}


NS_IMETHODIMP
nsMsgAccountManager::WriteToFolderCache(nsIMsgFolderCache *folderCache)
{
	m_incomingServers.Enumerate(writeFolderCache, folderCache);
	return folderCache->Close();
}

nsresult
nsMsgAccountManager::MigratePrefs()
{
#ifdef DEBUG_ACCOUNTMANAGER
	printf("nsMsgAccountManager::MigratePrefs()\n");
#endif

	// do nothing right now.
	// right now, all the migration happens when we don't find
	// the mail.accountmanager.accounts pref in LoadAccounts()
	return NS_OK;
}

nsresult
nsMsgAccountManager::createKeyedAccount(const char* key,
                                        nsIMsgAccount ** aAccount)
{
    
  nsIMsgAccount *account = nsnull;
  nsresult rv;
  rv = nsComponentManager::CreateInstance(kMsgAccountCID,
                                          nsnull,
                                          NS_GET_IID(nsIMsgAccount),
                                          (void **)&account);
  
  if (NS_FAILED(rv)) return rv;
  account->SetKey(NS_CONST_CAST(char*,(const char*)key));

  // add to internal nsISupportsArray
  m_accounts->AppendElement(NS_STATIC_CAST(nsISupports*, account));

  // add to string list
  if (accountKeyList.IsEmpty())
    accountKeyList = key;
  else {
    accountKeyList += ",";
    accountKeyList += key;
  }

  rv = getPrefService();
  if (NS_SUCCEEDED(rv))
    m_prefs->SetCharPref(PREF_MAIL_ACCOUNTMANAGER_ACCOUNTS,
                         accountKeyList.GetBuffer());

  *aAccount = account;
  NS_ADDREF(*aAccount);
  
  return NS_OK;
}

nsresult
nsMsgAccountManager::CreateAccount(nsIMsgAccount **_retval)
{
    if (!_retval) return NS_ERROR_NULL_POINTER;

    const char *key="xxxx";

    return createKeyedAccount(key, _retval);
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

PRBool
nsMsgAccountManager::findAccountByKey(nsISupports* element, void *aData)
{
    nsCOMPtr<nsIMsgAccount> account = do_QueryInterface(element);
    findAccountByKeyEntry *entry = (findAccountByKeyEntry*) aData;

    nsXPIDLCString key;
    account->GetKey(getter_Copies(key));
    if (PL_strcmp(key, entry->key)==0) {
        entry->account = account;
        return PR_FALSE;        // stop when found
    }

    return PR_TRUE;
}

nsresult
nsMsgAccountManager::createSpecialFile(nsFileSpec & dir, const char *specialFileName)
{
	if (!specialFileName) return NS_ERROR_NULL_POINTER;

	nsresult rv;
	nsFileSpec file(dir);
	file += specialFileName;

	nsCOMPtr <nsIFileSpec> specialFile;
	rv = NS_NewFileSpecWithSpec(file, getter_AddRefs(specialFile));
    if (NS_FAILED(rv)) return rv;

	PRBool specialFileExists;
	rv = specialFile->Exists(&specialFileExists);
	if (NS_FAILED(rv)) return rv;

	if (!specialFileExists) {
		rv = specialFile->Touch();
	}

	return rv;
}
nsresult
nsMsgAccountManager::upgradePrefs()
{
    nsresult rv;
    PRInt32 oldMailType;
    PRInt32 numAccounts = 0;
    char *oldstr = nsnull;
    PRBool oldbool;

    rv = getPrefService();
    if (NS_FAILED(rv)) return rv;

    rv = m_prefs->GetIntPref(PREF_4X_MAIL_SERVER_TYPE, &oldMailType);
    if (NS_FAILED(rv)) {
#ifdef DEBUG_ACCOUNTMANAGER
        printf("Tried to upgrade old prefs, but couldn't find server type!\n");
#endif
        return rv;
    }
#ifdef DEBUG_CLEAR_PREF
    // clear the 4.x pref to avoid confusion
    rv = m_prefs->ClearUserPref(PREF_4X_MAIL_SERVER_TYPE);
    NS_ASSERTION(NS_SUCCEEDED(rv), "failed to clear 4.x pref");
#endif

    nsCOMPtr<nsIMsgIdentity> identity;
    rv = createKeyedIdentity("identity1", getter_AddRefs(identity));
    if (NS_FAILED(rv)) return rv;

    // identity stuff
    rv = m_prefs->CopyCharPref(PREF_4X_MAIL_IDENTITY_USEREMAIL, &oldstr);
    if (NS_SUCCEEDED(rv)) {
      if (oldstr) {
      	identity->SetEmail(oldstr);
      }
      else {
        identity->SetEmail("");
      }
      PR_FREEIF(oldstr);
      oldstr = nsnull;

#ifdef DEBUG_CLEAR_PREF
      // clear the 4.x pref to avoid confusion
      rv = m_prefs->ClearUserPref(PREF_4X_MAIL_IDENTITY_USEREMAIL);
      NS_ASSERTION(NS_SUCCEEDED(rv), "failed to clear 4.x pref");
#endif
    }
    
    rv = m_prefs->CopyCharPref(PREF_4X_MAIL_IDENTITY_USERNAME, &oldstr);
    if (NS_SUCCEEDED(rv)) {
      if (oldstr) {
      	identity->SetFullName(oldstr);
      }
      else {
        identity->SetFullName("");
      }
      PR_FREEIF(oldstr);
      oldstr = nsnull;

#ifdef DEBUG_CLEAR_PREF
      // clear the 4.x pref to avoid confusion
      rv = m_prefs->ClearUserPref(PREF_4X_MAIL_IDENTITY_USERNAME);
      NS_ASSERTION(NS_SUCCEEDED(rv), "failed to clear 4.x pref");
#endif
    }
    rv = m_prefs->CopyCharPref(PREF_4X_MAIL_IDENTITY_REPLY_TO, &oldstr);
    if (NS_SUCCEEDED(rv)) {
      if (oldstr) {
      	identity->SetReplyTo(oldstr);
      }
      else {
        identity->SetReplyTo("");
      }
      PR_FREEIF(oldstr);
      oldstr = nsnull;
      
#ifdef DEBUG_CLEAR_PREF
      // clear the 4.x pref to avoid confusion
      rv = m_prefs->ClearUserPref(PREF_4X_MAIL_IDENTITY_REPLY_TO);
      NS_ASSERTION(NS_SUCCEEDED(rv), "failed to clear 4.x pref");
#endif
    }

    rv = m_prefs->CopyCharPref(PREF_4X_MAIL_IDENTITY_ORGANIZATION, &oldstr);
    if (NS_SUCCEEDED(rv)) {
      if (oldstr) {
      	identity->SetOrganization(oldstr);
      }
      else {
        identity->SetOrganization("");
      }
      PR_FREEIF(oldstr);
      oldstr = nsnull;
      
#ifdef DEBUG_CLEAR_PREF
      // clear the 4.x pref to avoid confusion
      rv = m_prefs->ClearUserPref(PREF_4X_MAIL_IDENTITY_ORGANIZATION);
      NS_ASSERTION(NS_SUCCEEDED(rv), "failed to clear 4.x pref");
#endif
    }

    rv = m_prefs->GetBoolPref(PREF_4X_MAIL_COMPOSE_HTML, &oldbool);
    if (NS_SUCCEEDED(rv)) {
      identity->SetComposeHtml(oldbool);

#ifdef DEBUG_CLEAR_PREF
      // clear the 4.x pref to avoid confusion
      rv = m_prefs->ClearUserPref(PREF_4X_MAIL_COMPOSE_HTML);
      NS_ASSERTION(NS_SUCCEEDED(rv), "failed to clear 4.x pref");
#endif
    }

    rv = m_prefs->CopyCharPref(PREF_4X_NETWORK_HOSTS_SMTP_SERVER, &oldstr);
    if (NS_SUCCEEDED(rv)) {
      if (oldstr) {
        identity->SetSmtpHostname(oldstr);
      }
      else {
        identity->SetSmtpHostname("");
      }
      PR_FREEIF(oldstr);
      oldstr = nsnull;
      
#ifdef DEBUG_CLEAR_PREF
      // clear the 4.x pref to avoid confusion
      rv = m_prefs->ClearUserPref(PREF_4X_NETWORK_HOSTS_SMTP_SERVER);
      NS_ASSERTION(NS_SUCCEEDED(rv), "failed to clear 4.x pref");
#endif
    }

    rv = m_prefs->CopyCharPref(PREF_4X_MAIL_SMTP_NAME, &oldstr);
    if (NS_SUCCEEDED(rv)) {
      if (oldstr) {
      	identity->SetSmtpUsername(oldstr);
      }
      else {
        identity->SetSmtpUsername("");
      }
      PR_FREEIF(oldstr);
      oldstr = nsnull;

#ifdef DEBUG_CLEAR_PREF
      // clear the 4.x pref to avoid confusion
      rv = m_prefs->ClearUserPref(PREF_4X_MAIL_SMTP_NAME);
      NS_ASSERTION(NS_SUCCEEDED(rv), "failed to clear 4.x pref");
#endif
    }
    
    if ( oldMailType == 0) {      // POP
      numAccounts += MigratePopAccounts(identity);
	}
    else if (oldMailType == 1) {  // IMAP
      numAccounts += MigrateImapAccounts(identity);
	}
    else {
#ifdef DEBUG_ACCOUNTMANAGER
        printf("Unrecognized server type %d\n", oldMailType);
#endif
        return NS_ERROR_UNEXPECTED;
    }

	numAccounts += MigrateNewsAccounts(identity, numAccounts);

    if (numAccounts == 0) return NS_ERROR_FAILURE;
    
    // we still need to create these additional prefs.
    // assume there is at least one account
    m_prefs->SetCharPref(PREF_MAIL_ACCOUNTMANAGER_ACCOUNTS,"account1");
    m_prefs->SetCharPref("mail.account.account1.identities","identity1");
    m_prefs->SetCharPref("mail.account.account1.server","server1");

    char *oldAccountsValueBuf=nsnull;
    char newAccountsValueBuf[BUF_STR_LEN];
    char prefNameBuf[BUF_STR_LEN];
    char prefValueBuf[BUF_STR_LEN];

    // handle the rest of the accounts.
    for (PRInt32 i=2;i<=numAccounts;i++) {
      rv = m_prefs->CopyCharPref(PREF_MAIL_ACCOUNTMANAGER_ACCOUNTS, &oldAccountsValueBuf);
      if (NS_SUCCEEDED(rv)) {
        PR_snprintf(newAccountsValueBuf, BUF_STR_LEN, "%s,account%d",oldAccountsValueBuf,i);
        m_prefs->SetCharPref(PREF_MAIL_ACCOUNTMANAGER_ACCOUNTS,
                                   newAccountsValueBuf);
      }
      PR_FREEIF(oldAccountsValueBuf);
      oldAccountsValueBuf = nsnull;
      
      PR_snprintf(prefNameBuf, BUF_STR_LEN, "mail.account.account%d.identities", i);
      PR_snprintf(prefValueBuf, BUF_STR_LEN, "identity%d", i);
      m_prefs->SetCharPref(prefNameBuf, prefValueBuf);

      PR_snprintf(prefNameBuf, BUF_STR_LEN, "mail.account.account%d.server", i);
      PR_snprintf(prefValueBuf, BUF_STR_LEN, "server%d", i);
      m_prefs->SetCharPref(prefNameBuf, prefValueBuf);
    }

    return NS_OK;
}

PRInt32
nsMsgAccountManager::MigratePopAccounts(nsIMsgIdentity *identity)
{
  nsresult rv;
  
  nsCOMPtr<nsIMsgAccount> account;
  nsCOMPtr<nsIMsgIncomingServer> server;

  rv = createKeyedAccount("account1", getter_AddRefs(account));
  if (NS_FAILED(rv)) return 0;

  rv = createKeyedServer("server1", "pop3", getter_AddRefs(server));
  if (NS_FAILED(rv)) return 0;
  
  account->SetIncomingServer(server);
  account->AddIdentity(identity);

  // now upgrade all the prefs
  char *oldstr = nsnull;
  PRInt32 oldint;
  PRBool oldbool;

  nsFileSpec profileDir;
  
  NS_WITH_SERVICE(nsIProfile, profile, kProfileCID, &rv);
  if (NS_FAILED(rv)) return 0;
  
  rv = profile->GetCurrentProfileDir(&profileDir);
  if (NS_FAILED(rv)) return 0;
  

  // pop stuff
  // some of this ought to be moved out into the POP implementation
  nsCOMPtr<nsIPop3IncomingServer> popServer;
  popServer = do_QueryInterface(server, &rv);
  if (NS_SUCCEEDED(rv)) {
    server->SetType("pop3");

    rv = m_prefs->CopyCharPref(PREF_4X_MAIL_POP_NAME, &oldstr);
    if (NS_SUCCEEDED(rv)) {
      server->SetUsername(oldstr);
      PR_FREEIF(oldstr);
      oldstr = nsnull;

#ifdef DEBUG_CLEAR_PREF
      // clear the 4.x pref to avoid confusion
      rv = m_prefs->ClearUserPref(PREF_4X_MAIL_POP_NAME);
      NS_ASSERTION(NS_SUCCEEDED(rv), "failed to clear 4.x pref");
#endif
    }
    
    rv = m_prefs->GetBoolPref(PREF_4X_MAIL_REMEMBER_PASSWORD, &oldbool);
    if (NS_SUCCEEDED(rv)) {
      server->SetRememberPassword(oldbool);
      
#ifdef DEBUG_CLEAR_PREF
      // clear the 4.x pref to avoid confusion
      rv = m_prefs->ClearUserPref(PREF_4X_MAIL_REMEMBER_PASSWORD);
      NS_ASSERTION(NS_SUCCEEDED(rv), "failed to clear 4.x pref");
#endif
    }
      
#ifdef CAN_UPGRADE_4x_PASSWORDS
    rv = m_prefs->CopyCharPref(PREF_4X_MAIL_POP_PASSWORD, &oldstr);
    if (NS_SUCCEEDED(rv)) {
      server->SetPassword(oldstr);
      PR_FREEIF(oldstr);
      oldstr = nsnull;

#ifdef DEBUG_CLEAR_PREF
      // clear the 4.x pref to avoid confusion
      rv = m_prefs->ClearUserPref(PREF_4X_MAIL_POP_PASSWORD);
      NS_ASSERTION(NS_SUCCEEDED(rv), "failed to clear 4.x pref");
#endif
    }
#endif /* CAN_UPGRADE_4x_PASSWORDS */

    char *hostname=nsnull;
    rv = m_prefs->CopyCharPref(PREF_4X_NETWORK_HOSTS_POP_SERVER, &hostname);
    if (NS_SUCCEEDED(rv)) {
      server->SetHostName(hostname);

#ifdef DEBUG_CLEAR_PREF
      // clear the 4.x pref to avoid confusion
      rv = m_prefs->ClearUserPref(PREF_4X_NETWORK_HOSTS_POP_SERVER);
      NS_ASSERTION(NS_SUCCEEDED(rv), "failed to clear 4.x pref");
#endif
    }
    
    rv = m_prefs->GetBoolPref(PREF_4X_MAIL_CHECK_NEW_MAIL, &oldbool);
    if (NS_SUCCEEDED(rv)) {
      server->SetDoBiff(oldbool);

#ifdef DEBUG_CLEAR_PREF
      // clear the 4.x pref to avoid confusion
      rv = m_prefs->ClearUserPref(PREF_4X_MAIL_CHECK_NEW_MAIL);
      NS_ASSERTION(NS_SUCCEEDED(rv), "failed to clear 4.x pref");
#endif
    }
    
    rv = m_prefs->GetIntPref(PREF_4X_MAIL_CHECK_TIME, &oldint);
    if (NS_SUCCEEDED(rv)) {
      server->SetBiffMinutes(oldint);
      
#ifdef DEBUG_CLEAR_PREF
      // clear the 4.x pref to avoid confusion
      rv = m_prefs->ClearUserPref(PREF_4X_MAIL_CHECK_TIME);
      NS_ASSERTION(NS_SUCCEEDED(rv), "failed to clear 4.x pref");
#endif
    }
    
    // create the directory structure for this pop account
    // under <profile dir>/Mail/<hostname>
    nsCOMPtr <nsIFileSpec> mailDir;
    nsFileSpec dir(profileDir);
    PRBool dirExists;
    
    // turn profileDir into the mail dir.
    dir += "Mail";
    if (!dir.Exists()) {
      dir.CreateDir();
    }
    dir += hostname;
    PR_FREEIF(hostname);
    
    rv = NS_NewFileSpecWithSpec(dir, getter_AddRefs(mailDir));
    if (NS_FAILED(rv)) return 0;
    
    rv = mailDir->Exists(&dirExists);
    if (NS_FAILED(rv)) return 0;
    
    if (!dirExists) {
      mailDir->CreateDir();
    }
    
    char *str = nsnull;
    mailDir->GetNativePath(&str);
    
    if (str && *str) {
      server->SetLocalPath(str);
      PR_FREEIF(str);
      str = nsnull;
    }
    
    rv = mailDir->Exists(&dirExists);
    if (NS_FAILED(rv)) return 0;
    
    if (!dirExists) {
      mailDir->CreateDir();
    }
    
    // create the files for the special folders.
    // this needs to be i18N.
    rv = createSpecialFile(dir,"Inbox");
    if (NS_FAILED(rv)) return 0;
    
    rv = createSpecialFile(dir,"Sent");
    if (NS_FAILED(rv)) return 0;
    
    rv = createSpecialFile(dir,"Trash");
    if (NS_FAILED(rv)) return 0;
    
    rv = createSpecialFile(dir,"Drafts");
    if (NS_FAILED(rv)) return 0;
    
    rv = createSpecialFile(dir,"Templates");
    if (NS_FAILED(rv)) return 0;
    
    rv = createSpecialFile(dir,"Unsent Message");
    if (NS_FAILED(rv)) return 0;

    rv = m_prefs->GetBoolPref(PREF_4X_MAIL_LEAVE_ON_SERVER, &oldbool);
    if (NS_SUCCEEDED(rv)) {
      popServer->SetLeaveMessagesOnServer(oldbool);
      
#ifdef DEBUG_CLEAR_PREF
      // clear the 4.x pref to avoid confusion
      rv = m_prefs->ClearUserPref(PREF_4X_MAIL_LEAVE_ON_SERVER);
      NS_ASSERTION(NS_SUCCEEDED(rv), "failed to clear 4.x pref");
#endif
    }
    
    rv = m_prefs->GetBoolPref(PREF_4X_MAIL_DELETE_MAIL_LEFT_ON_SERVER, &oldbool);
    if (NS_SUCCEEDED(rv)) {
      popServer->SetDeleteMailLeftOnServer(oldbool);
      
#ifdef DEBUG_CLEAR_PREF
      // clear the 4.x pref to avoid confusion
      rv = m_prefs->ClearUserPref(PREF_4X_MAIL_DELETE_MAIL_LEFT_ON_SERVER);
      NS_ASSERTION(NS_SUCCEEDED(rv), "failed to clear 4.x pref");
#endif
    }
  }
  
  // one account created!
  return 1;
}

PRInt32
nsMsgAccountManager::MigrateImapAccounts(nsIMsgIdentity *identity)
{
  nsresult rv;
  PRInt32 numAccounts = 0;
  char *hostList=nsnull;
  rv = getPrefService();
  if (NS_FAILED(rv)) return 0;

  rv = m_prefs->CopyCharPref(PREF_4X_NETWORK_HOSTS_IMAP_SERVER, &hostList);
  if (NS_FAILED(rv)) return 0;
  
#ifdef DEBUG_CLEAR_PREF
  // clear the 4.x pref to avoid confusion
  rv = m_prefs->ClearUserPref(PREF_4X_NETWORK_HOSTS_IMAP_SERVER);
  NS_ASSERTION(NS_SUCCEEDED(rv), "failed to clear 4.x pref");
#endif
  
  if (!hostList || !*hostList) return 0;
  
  char *token = nsnull;
  char *rest = NS_CONST_CAST(char*,(const char*)hostList);
  nsCAutoString str;
      
  token = nsCRT::strtok(rest, ",", &rest);
  while (token && *token) {
    str = token;
    str.StripWhitespace();
    
    if (!str.IsEmpty()) {
	  numAccounts++;
      // str is the hostname
      if (NS_FAILED(MigrateImapAccount(identity,str,numAccounts))) {
		// failed to migrate.  bail out.
        return 0;
      }
      str = "";
    }
    token = nsCRT::strtok(rest, ",", &rest);
  }
  PR_FREEIF(hostList);
  return numAccounts;
}

nsresult
nsMsgAccountManager::CopyIdentity(nsIMsgIdentity *srcIdentity, nsIMsgIdentity *destIdentity)
{
	nsresult rv;
	nsXPIDLCString oldstr;
	PRBool oldbool;

	rv = srcIdentity->GetEmail(getter_Copies(oldstr)); 
	if (NS_FAILED(rv)) return rv;
	if (!oldstr) {
		destIdentity->SetEmail("");
	}
	else {
		destIdentity->SetEmail(NS_CONST_CAST(char*,(const char*)oldstr));
	}

	rv = srcIdentity->GetReplyTo(getter_Copies(oldstr)); 
	if (NS_FAILED(rv)) return rv;
	if (!oldstr) {
		destIdentity->SetReplyTo("");
	}
	else {
		destIdentity->SetReplyTo(NS_CONST_CAST(char*,(const char*)oldstr)); 
	}

	rv = srcIdentity->GetComposeHtml(&oldbool); 
	if (NS_FAILED(rv)) return rv;
	destIdentity->SetComposeHtml(oldbool); 

	rv = srcIdentity->GetFullName(getter_Copies(oldstr)); 
	if (NS_FAILED(rv)) return rv;
	if (!oldstr) {
		destIdentity->SetFullName("");
	}
	else {
		destIdentity->SetFullName(NS_CONST_CAST(char*,(const char*)oldstr)); 
	}
	
	rv = srcIdentity->GetOrganization(getter_Copies(oldstr)); 
	if (NS_FAILED(rv)) return rv;
	if (!oldstr) {
		destIdentity->SetOrganization("");
	}
	else {
		destIdentity->SetOrganization(NS_CONST_CAST(char*,(const char*)oldstr)); 
	}
	rv = srcIdentity->GetSmtpHostname(getter_Copies(oldstr));
	if (NS_FAILED(rv)) return rv;
	if (!oldstr) {
		destIdentity->SetSmtpHostname("");
	}
	else {
		destIdentity->SetSmtpHostname(NS_CONST_CAST(char*,(const char*)oldstr));
	}

	rv = srcIdentity->GetSmtpUsername(getter_Copies(oldstr));
	if (NS_FAILED(rv)) return rv;
	if (!oldstr) {
		destIdentity->SetSmtpUsername("");
	}
	else {
		destIdentity->SetSmtpUsername(NS_CONST_CAST(char*,(const char*)oldstr));
	}

	return rv;
}

nsresult
nsMsgAccountManager::MigrateImapAccount(nsIMsgIdentity *identity, const char *hostname, PRInt32 accountNum)
{
  nsresult rv;
  PRInt32 oldint;
  PRBool oldbool;

  if (!hostname) return NS_ERROR_NULL_POINTER;
  if (accountNum < 1) return NS_ERROR_FAILURE;
  
  //
  // create the account
  //
  char accountStr[BUF_STR_LEN];
  PR_snprintf(accountStr,BUF_STR_LEN,"account%d",accountNum);
#ifdef DEBUG_ACCOUNTMANAGER
  printf("account str = %s\n",accountStr);
#endif
  
  nsCOMPtr<nsIMsgAccount> account;
  rv = createKeyedAccount(accountStr, getter_AddRefs(account));
  if (NS_FAILED(rv)) return rv;

  //
  // create the server
  //
  char serverStr[BUF_STR_LEN];
  PR_snprintf(serverStr,BUF_STR_LEN,"server%d",accountNum);
#ifdef DEBUG_ACCOUNTMANAGER
  printf("server str = %s\n",serverStr);
#endif

  nsCOMPtr<nsIMsgIncomingServer> server;
  rv = createKeyedServer(serverStr, "imap", getter_AddRefs(server));
  if (NS_FAILED(rv)) return rv;

  //
  // create the identity
  //
  char identityStr[BUF_STR_LEN];
  PR_snprintf(identityStr,BUF_STR_LEN,"identity%d",accountNum);

  nsCOMPtr<nsIMsgIdentity> copied_identity;
  rv = createKeyedIdentity(identityStr, getter_AddRefs(copied_identity));
  if (NS_FAILED(rv)) return rv;

  //
  // connect all three together
  //
  account->SetIncomingServer(server);

  rv = CopyIdentity(identity,copied_identity);
  if (NS_FAILED(rv)) return rv;

  account->AddIdentity(copied_identity);

  // now upgrade all the prefs
  char *oldstr = nsnull;

  nsFileSpec profileDir;
  
  NS_WITH_SERVICE(nsIProfile, profile, kProfileCID, &rv);
  if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
  
  rv = profile->GetCurrentProfileDir(&profileDir);
  if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
  
  // some of this ought to be moved out into the IMAP implementation
  nsCOMPtr<nsIImapIncomingServer> imapServer;
  imapServer = do_QueryInterface(server, &rv);
  if (NS_FAILED(rv)) {
    return rv;
  }
  
  server->SetType("imap");
  server->SetHostName((char *)hostname);

  char prefName[BUF_STR_LEN];
  PR_snprintf(prefName, BUF_STR_LEN, "mail.imap.server.%s.userName",hostname);
  rv = m_prefs->CopyCharPref(prefName, &oldstr);
  if (NS_SUCCEEDED(rv)) {
    server->SetUsername(oldstr);
    PR_FREEIF(oldstr);
    oldstr = nsnull;

#ifdef DEBUG_CLEAR_PREF
    // clear the 4.x pref to avoid confusion
    rv = m_prefs->ClearUserPref(prefName);
    NS_ASSERTION(NS_SUCCEEDED(rv), "failed to clear 4.x pref");
#endif
  }

  PR_snprintf(prefName, BUF_STR_LEN, "mail.imap.server.%s.remember_password",hostname);
  rv = m_prefs->GetBoolPref(prefName, &oldbool);
  if (NS_SUCCEEDED(rv)) {
    server->SetRememberPassword(oldbool);

#ifdef DEBUG_CLEAR_PREF
    // clear the 4.x pref to avoid confusion
    rv = m_prefs->ClearUserPref(prefName);
    NS_ASSERTION(NS_SUCCEEDED(rv), "failed to clear 4.x pref");
#endif
  }
  

#ifdef CAN_UPGRADE_4x_PASSWORDS
  // upgrade the password
  PR_snprintf(prefName, BUF_STR_LEN, "mail.imap.server.%s.password",hostname);
  rv = m_prefs->CopyCharPref(prefName, &oldstr);
  if (NS_SUCCEEDED(rv)) {
    server->SetPassword(oldStr);
    PR_FREEIF(oldstr);
    oldstr = nsnull;

#ifdef DEBUG_CLEAR_PREF
    // clear the 4.x pref to avoid confusion
    rv = m_prefs->ClearUserPref(prefName);
    NS_ASSERTION(NS_SUCCEEDED(rv), "failed to clear 4.x pref");
#endif
  }
#endif /* CAN_UPGRADE_4x_PASSWORDS */

  // upgrade the biff prefs
  PR_snprintf(prefName, BUF_STR_LEN, "mail.imap.server.%s.check_new_mail",hostname);
  rv = m_prefs->GetBoolPref(prefName, &oldbool);
  if (NS_SUCCEEDED(rv)) {
    server->SetDoBiff(oldbool);

#ifdef DEBUG_CLEAR_PREF
    // clear the 4.x pref to avoid confusion
    rv = m_prefs->ClearUserPref(prefName);
    NS_ASSERTION(NS_SUCCEEDED(rv), "failed to clear 4.x pref");
#endif
  }

  PR_snprintf(prefName, BUF_STR_LEN, "mail.imap.server.%s.check_time",hostname);
  rv = m_prefs->GetIntPref(prefName, &oldint);
  if (NS_SUCCEEDED(rv)) {
    server->SetBiffMinutes(oldint);

#ifdef DEBUG_CLEAR_PREF
    // clear the 4.x pref to avoid confusion
    rv = m_prefs->ClearUserPref(prefName);
    NS_ASSERTION(NS_SUCCEEDED(rv), "failed to clear 4.x pref");
#endif
  }
  
  // create the directory structure for this pop account
  // under <profile dir>/Mail/<hostname>
  nsCOMPtr <nsIFileSpec> imapMailDir;
  nsFileSpec dir(profileDir);
  PRBool dirExists;
  
  // turn profileDir into the mail dir.
  dir += "ImapMail";
  if (!dir.Exists()) {
    dir.CreateDir();
  }
  dir += hostname;
  
  rv = NS_NewFileSpecWithSpec(dir, getter_AddRefs(imapMailDir));
  if (NS_FAILED(rv)) return rv;

  char *str = nsnull;
  imapMailDir->GetNativePath(&str);

  if (str && *str) {
    server->SetLocalPath(str);
    PR_FREEIF(str);
    str = nsnull;
  }
  
  rv = imapMailDir->Exists(&dirExists);
  if (NS_FAILED(rv)) return rv;
  
  if (!dirExists) {
    imapMailDir->CreateDir();
  }
  
  return NS_OK;
}
  
#ifdef USE_NEWSRC_MAP_FILE
#define NEWSRC_MAP_FILE_COOKIE "netscape-newsrc-map-file"
#endif /* USE_NEWSRC_MAP_FILE */

PRInt32
nsMsgAccountManager::MigrateNewsAccounts(nsIMsgIdentity *identity, PRInt32 baseAccountNum)
{
	PRInt32 numAccounts = 0;
	nsresult rv;

	// there should be one imap or one pop by this point.
	// if there isn't, bail now?
	//if (accountNum < 1) return 0;

	nsFileSpec profileDir;
	
	NS_WITH_SERVICE(nsIProfile, profile, kProfileCID, &rv);
	if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
	
	rv = profile->GetCurrentProfileDir(&profileDir);
	if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

	nsFileSpec newsDir(profileDir);
	
	// turn profileDir into the News dir.
	newsDir += "News";
	if (!newsDir.Exists()) {
		newsDir.CreateDir();
	}

#ifdef USE_NEWSRC_MAP_FILE	
	// use news.directory to find the fat file
	// for each news server in the fat file call MigrateNewsAccount();
	
	// use news.directory?
	nsFileSpec fatFile(newsDir);
	fatFile += NEWS_FAT_FILE_NAME;
	
	char buffer[512];
	char psuedo_name[512];
	char filename[512];
	char is_newsgroup[512];
	PRBool ok;

	nsInputFileStream inputStream(fatFile);
	
	if (inputStream.eof()) {
		inputStream.close();
		return 0;
	}
	
    /* we expect the first line to be NEWSRC_MAP_FILE_COOKIE */
	ok = inputStream.readline(buffer, sizeof(buffer));
	
    if ((!ok) || (PL_strncmp(buffer, NEWSRC_MAP_FILE_COOKIE, PL_strlen(NEWSRC_MAP_FILE_COOKIE)))) {
		inputStream.close();
		return 0;
	}   
	
	while (!inputStream.eof()) {
		char * p;
		PRInt32 i;
		
		ok = inputStream.readline(buffer, sizeof(buffer));
		if (!ok) {
			inputStream.close();
			return 0;
		}  
		
		/* sspitzer todo:  replace this with nsString code */

		/*
		This used to be scanf() call which would incorrectly
		parse long filenames with spaces in them.  - JRE
		*/
		
		filename[0] = '\0';
		is_newsgroup[0]='\0';
		
		for (i = 0, p = buffer; *p && *p != '\t' && i < 500; p++, i++)
			psuedo_name[i] = *p;
		psuedo_name[i] = '\0';
		if (*p) 
		{
			for (i = 0, p++; *p && *p != '\t' && i < 500; p++, i++)
				filename[i] = *p;
			filename[i]='\0';
			if (*p) 
			{
				for (i = 0, p++; *p && *p != '\r' && i < 500; p++, i++)
					is_newsgroup[i] = *p;
				is_newsgroup[i]='\0';
			}
		}
		
		if(!PL_strncmp(is_newsgroup, "TRUE", 4)) {
#ifdef DEBUG_ACCOUNTMANAGER
			printf("is_newsgroups_file = TRUE\n");
#endif
		}
		else {
#ifdef DEBUG_ACCOUNTMANAGER
          printf("is_newsgroups_file = FALSE\n");
  
          printf("psuedo_name=%s,filename=%s\n", psuedo_name, filename);
#endif			
#ifdef NEWS_FAT_STORES_ABSOLUTE_NEWSRC_FILE_PATHS
			// most likely, the fat file has been copied (or moved ) from
			// its old location.  So the absolute file paths will be wrong.
			// all we care about is the leaf, so use that.
			nsFileSpec oldRcFile(filename);
			char *leaf = oldRcFile.GetLeafName();

			// use news.directory instead of newsDir?
			nsFileSpec rcFile(newsDir);
			rcFile += leaf;
			nsCRT::free(leaf);
			leaf = nsnull;
#else
		    // use news.directory instead of newsDir?
			nsFileSpec rcFile(newsDir);
			rcFile += filename;
#endif /* NEWS_FAT_STORES_ABSOLUTE_NEWSRC_FILE_PATHS */

			numAccounts++;
				
			// psuedo-name is of the form newsrc-<host> or snewsrc-<host>.  
			// right now, we can't handle snewsrc, so if we get one of those
			// gracefully handle it by ignoring it.
			if (PL_strncmp(PSUEDO_NAME_PREFIX,psuedo_name,PL_strlen(PSUEDO_NAME_PREFIX)) != 0) {
				continue;
			}

			// check that there is a hostname to get after the "newsrc-" part
			NS_ASSERTION(PL_strlen(psuedo_name) > PL_strlen(PSUEDO_NAME_PREFIX), "psuedo_name is too short");
			if (PL_strlen(psuedo_name) <= PL_strlen(PSUEDO_NAME_PREFIX)) {
				return 0;
			}

			char *hostname = psuedo_name + PL_strlen(PSUEDO_NAME_PREFIX);
#ifdef DEBUG_ACCOUNTMANAGER
            printf("rcFile?  should it be a const char *?\n");
#endif
			if (NS_FAILED(MigrateNewsAccount(identity, hostname, rcFile, baseAccountNum + numAccounts))) {
				// failed to migrate.  bail out
				return 0;
			}
		}
	}
	
	inputStream.close();

#else
    char *news_directory_value = nsnull;
	nsFileSpec dirWithTheNewsrcFiles;
    
	rv = m_prefs->CopyCharPref(PREF_NEWS_DIRECTORY, &news_directory_value);
	if (NS_SUCCEEDED(rv)) {
      dirWithTheNewsrcFiles = news_directory_value;
      PR_FREEIF(news_directory_value);
      news_directory_value = nsnull;

#if 0
      /* leave "news.directory" pref, we may still be using it in 5.0 */
      
      // clear the 4.x pref to avoid confusion
      rv = m_prefs->ClearUserPref(PREF_NEWS_DIRECTORY);
      NS_ASSERTION(NS_SUCCEEDED(rv), "failed to clear 4.x pref");
#endif
	}
    else {
      // if that fails, use the home directory
      
#ifdef XP_UNIX
      nsSpecialSystemDirectory homeDir(nsSpecialSystemDirectory::Unix_HomeDirectory);
#elif XP_BEOS
      nsSpecialSystemDirectory homeDir(nsSpecialSystemDirectory::BeOS_HomeDirectory);
#else
#error where_are_your_newsrc_files
#endif /* XP_UNIX, XP_BEOS */

      dirWithTheNewsrcFiles = homeDir;
    }

    for (nsDirectoryIterator i(dirWithTheNewsrcFiles, PR_FALSE); i.Exists(); i++) {
      nsFileSpec possibleRcFile = i.Spec();

      char *filename = possibleRcFile.GetLeafName();
#ifdef DEBUG_ACCOUNTMANAGER
      printf("leaf = %s\n", filename);
#endif
      
      if ((PL_strncmp(NEWSRC_FILE_PREFIX, filename, PL_strlen(NEWSRC_FILE_PREFIX)) == 0) && (PL_strlen(filename) > PL_strlen(NEWSRC_FILE_PREFIX))) {
#ifdef DEBUG_ACCOUNTMANAGER
        printf("found a newsrc file!\n");
#endif
        numAccounts++;

        char *hostname = filename + PL_strlen(NEWSRC_FILE_PREFIX);
        if (NS_FAILED(MigrateNewsAccount(identity, hostname, possibleRcFile, baseAccountNum + numAccounts))) {
          // failed to migrate.  bail out
          return 0;
        }
      }
      nsCRT::free(filename);
      filename = nsnull;
    }
#endif /* USE_NEWSRC_MAP_FILE */

	return numAccounts;
}

nsresult
nsMsgAccountManager::MigrateNewsAccount(nsIMsgIdentity *identity, const char *hostname, const char *newsrcfile, PRInt32 accountNum)
{  
	nsresult rv;
	
	if (!newsrcfile) return NS_ERROR_NULL_POINTER;
	if (!hostname) return NS_ERROR_NULL_POINTER;
	if (accountNum < 1) return NS_ERROR_FAILURE;
	

    //
    // create the account
    //
	char accountStr[BUF_STR_LEN];
	PR_snprintf(accountStr,BUF_STR_LEN,"account%d",accountNum);
#ifdef DEBUG_ACCOUNTMANAGER
	printf("account str = %s\n",accountStr);
#endif
	nsCOMPtr<nsIMsgAccount> account;
    rv = createKeyedAccount(accountStr, getter_AddRefs(account));
	if (NS_FAILED(rv)) return rv;

    //
    // create the server
    //
	char serverStr[BUF_STR_LEN];
	PR_snprintf(serverStr,BUF_STR_LEN,"server%d",accountNum);
#ifdef DEBUG_ACCOUNTMANAGER
	printf("server str = %s\n",serverStr);
#endif
	nsCOMPtr<nsIMsgIncomingServer> server;
    rv = createKeyedServer(serverStr, "nntp", getter_AddRefs(server));
	if (NS_FAILED(rv)) return rv;

    //
    // create the identity
    //
	char identityStr[BUF_STR_LEN];
	PR_snprintf(identityStr,BUF_STR_LEN,"identity%d",accountNum);
	
	nsCOMPtr<nsIMsgIdentity> copied_identity;
    rv = createKeyedIdentity(identityStr, getter_AddRefs(copied_identity));
	if (NS_FAILED(rv)) return rv;

	rv = CopyIdentity(identity,copied_identity);
	if (NS_FAILED(rv)) return rv;

    //
    // hook them together
    //
	account->SetIncomingServer(server);
	account->AddIdentity(copied_identity);
	
	// now upgrade all the prefs
	nsFileSpec profileDir;
	
	NS_WITH_SERVICE(nsIProfile, profile, kProfileCID, &rv);
	if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
	
	rv = profile->GetCurrentProfileDir(&profileDir);
	if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
	
	// some of this ought to be moved out into the NNTP implementation
	nsCOMPtr<nsINntpIncomingServer> nntpServer;
	nntpServer = do_QueryInterface(server, &rv);
	if (NS_FAILED(rv)) {
		return rv;
	}
	
	server->SetType("nntp");
	server->SetHostName((char *)hostname);

#ifdef SUPPORT_SNEWS
#error THIS_CODE_ISNT_DONE_YET
	char *oldstr = nsnull;
	
	// we don't handle nntp servers that accept username / passwords yet
	char prefName[BUF_STR_LEN];
	PR_snprintf(prefName, BUF_STR_LEN, "???nntp.server.%s.userName",hostname);
	rv = m_prefs->CopyCharPref(prefName, &oldstr);
	if (NS_SUCCEEDED(rv)) {
		server->SetUsername(oldstr);
		PR_FREEIF(oldstr);
		oldstr = nsnull;

#ifdef DEBUG_CLEAR_PREF
        // clear the 4.x pref to avoid confusion
        rv = m_prefs->ClearUserPref(prefName);
        NS_ASSERTION(NS_SUCCEEDED(rv), "failed to clear 4.x pref");
#endif
	}

#ifdef CAN_UPGRADE_4x_PASSWORDS
	// upgrade the password
	PR_snprintf(prefName, BUF_STR_LEN, "???nntp.server.%s.password",hostname);
	rv = m_prefs->CopyCharPref(prefName, &oldstr);
	if (NS_SUCCEEDED(rv)) {
		server->SetPassword(oldStr);
        PR_FREEIF(oldstr);
		oldstr = nsnull;

#ifdef DEBUG_CLEAR_PREF
        // clear the 4.x pref to avoid confusion
        rv = m_prefs->ClearUserPref(prefName);
        NS_ASSERTION(NS_SUCCEEDED(rv), "failed to clear 4.x pref");
#endif
	}
#endif /* CAN_UPGRADE_4x_PASSWORDS */
#endif /* SUPPORT_SNEWS */
		
	// create the directory structure for this pop account
	// under <profile dir>/News/host-<hostname>
	nsCOMPtr <nsIFileSpec> newsDir;
	nsFileSpec dir(profileDir);
	PRBool dirExists;
	
	// turn profileDir into the News dir.
	dir += "News";
	if (!dir.Exists()) {
		dir.CreateDir();
	}

	// can't do dir += "host-"; dir += hostname; 
	// because += on a nsFileSpec inserts a separator
	// so we'd end up with host-/<hostname> and not host-<hostname>
	nsCAutoString alteredHost ((const char *) "host-");
	alteredHost += hostname;
	NS_MsgHashIfNecessary(alteredHost);	
	dir += (const char *) alteredHost;

	rv = NS_NewFileSpecWithSpec(dir, getter_AddRefs(newsDir));
	if (NS_FAILED(rv)) return rv;

	char *str = nsnull;
	newsDir->GetNativePath(&str);
	
	if (str && *str) {
		server->SetLocalPath(str);
		PR_FREEIF(str);
		str = nsnull;
	}
	
	rv = newsDir->Exists(&dirExists);
	if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
	
	if (!dirExists) {
		newsDir->CreateDir();
	}
	
	nntpServer->SetNewsrcFilePath((char *)newsrcfile);

	return NS_OK;
}

NS_IMETHODIMP
nsMsgAccountManager::FindServer(const char* username,
                                const char* hostname,
                                const char* type,
                                nsIMsgIncomingServer** aResult)
{
  nsresult rv;
  nsCOMPtr<nsISupportsArray> servers;
  
  rv = GetAllServers(getter_AddRefs(servers));
  if (NS_FAILED(rv)) return rv;

  findServerEntry serverInfo;
  serverInfo.hostname = hostname;
  // username might be blank, pass "" instead
  serverInfo.username = username ? username : ""; 
  serverInfo.type = type;
  serverInfo.server = *aResult = nsnull;
  
  servers->EnumerateForwards(findServer, (void *)&serverInfo);

  if (!serverInfo.server) return NS_ERROR_UNEXPECTED;
  *aResult = serverInfo.server;
  NS_ADDREF(*aResult);
  
  return NS_OK;

}


// if the aElement matches the given hostname, add it to the given array
PRBool
nsMsgAccountManager::findServer(nsISupports *aElement, void *data)
{
  nsCOMPtr<nsIMsgIncomingServer> server = do_QueryInterface(aElement);
  if (!server) return PR_TRUE;

  findServerEntry *entry = (findServerEntry*) data;

  nsresult rv;
  
  nsXPIDLCString thisHostname;
  rv = server->GetHostName(getter_Copies(thisHostname));
  if (NS_FAILED(rv)) return PR_TRUE;

  nsXPIDLCString thisUsername;
  rv = server->GetUsername(getter_Copies(thisUsername));
  if (NS_FAILED(rv)) return PR_TRUE;
 
  
  nsXPIDLCString thisType;
  rv = server->GetType(getter_Copies(thisType));
  if (NS_FAILED(rv)) return PR_TRUE;
 
  if (PL_strcasecmp(entry->hostname, thisHostname)==0 &&
      PL_strcmp(entry->type, thisType)==0) {
        // if we aren't looking for a username, don't compare.  we have a match
  	if (PL_strcmp(entry->username,"")==0) {
		entry->server = server;
		return PR_FALSE;            // stop on first find 
        }
        else {
      		if (PL_strcmp(entry->username, thisUsername)==0) {
		   entry->server = server;
		   return PR_FALSE;            // stop on first find 
		}
	}
  }

  return PR_TRUE;
}

NS_IMETHODIMP
nsMsgAccountManager::GetIdentitiesForServer(nsIMsgIncomingServer *server,
                                            nsISupportsArray **_retval)
{
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
  nsCOMPtr<nsIMsgAccount> account = do_QueryInterface(element);
  findIdentitiesByServerEntry *entry = (findIdentitiesByServerEntry*)aData;
  
  nsCOMPtr<nsIMsgIncomingServer> thisServer;
  rv = account->GetIncomingServer(getter_AddRefs(thisServer));
  if (NS_FAILED(rv)) return PR_TRUE;
  
  nsXPIDLCString serverKey;
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
  nsCOMPtr<nsIMsgAccount> account = do_QueryInterface(element);
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
      thisIdentity = do_QueryInterface(thisSupports);

    if (thisIdentity) {

      nsXPIDLCString thisIdentityKey;
      rv = thisIdentity->GetKey(getter_Copies(thisIdentityKey));

      if (NS_SUCCEEDED(rv) && PL_strcmp(identityKey, thisIdentityKey) == 0) {
        nsCOMPtr<nsIMsgIncomingServer> thisServer;
        rv = account->GetIncomingServer(getter_AddRefs(thisServer));
        
        if (NS_SUCCEEDED(rv)) {
          entry->servers->AppendElement(thisServer);
          break;
        }
        
      }
    }
  }

  return PR_TRUE;
}


nsresult
NS_NewMsgAccountManager(const nsIID& iid, void **result)
{
  nsMsgAccountManager* manager;
  if (!result) return NS_ERROR_NULL_POINTER;
  
  manager = new nsMsgAccountManager();
  
  return manager->QueryInterface(iid, result);
}
