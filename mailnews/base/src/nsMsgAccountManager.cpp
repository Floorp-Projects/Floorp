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

#if defined(DEBUG_alecf) || defined(DEBUG_sspitzer) || defined(DEBUG_seth)
#define DEBUG_ACCOUNTMANAGER 1
#endif

static NS_DEFINE_CID(kMsgAccountCID, NS_MSGACCOUNT_CID);
static NS_DEFINE_CID(kMsgIdentityCID, NS_MSGIDENTITY_CID);
static NS_DEFINE_CID(kPrefServiceCID, NS_PREF_CID);
static NS_DEFINE_CID(kMsgBiffManagerCID, NS_MSGBIFFMANAGER_CID);
static NS_DEFINE_CID(kProfileCID, NS_PROFILE_CID);
static NS_DEFINE_CID(kCNetSupportDialogCID, NS_NETSUPPORTDIALOG_CID);

// use this to search all accounts for the given account and store the
// resulting key in hashKey
typedef struct _findAccountEntry {
  nsIMsgAccount *account;
  PRBool found;
  nsHashKey* hashKey;
} findAccountEntry;


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

typedef struct _findIdentityByKeyEntry {
  const char *key;
  nsIMsgIdentity *identity;
} findIdentityByKeyEntry;

class nsMsgAccountManager : public nsIMsgAccountManager {
public:

  nsMsgAccountManager();
  virtual ~nsMsgAccountManager();
  
  NS_DECL_ISUPPORTS
  
  /* nsIMsgAccount createAccount (in nsIMsgIncomingServer server,
     in nsIMsgIdentity identity); */
  NS_IMETHOD CreateAccount(nsIMsgIncomingServer *server,
                           nsIMsgIdentity *identity,
                           nsIMsgAccount **_retval) ;
  
  /* nsIMsgAccount AddAccount (in nsIMsgIncomingServer server,
     in nsIMsgIdentity identity, in string accountKey); */
  NS_IMETHOD CreateAccountWithKey(nsIMsgIncomingServer *server,
                                  nsIMsgIdentity *identity, 
                                  const char *accountKey,
                                  nsIMsgAccount **_retval) ;

  NS_IMETHOD AddAccount(nsIMsgAccount *account);
  NS_IMETHOD RemoveAccount(nsIMsgAccount *account);
  
  /* attribute nsIMsgAccount defaultAccount; */
  NS_IMETHOD GetDefaultAccount(nsIMsgAccount * *aDefaultAccount) ;
  NS_IMETHOD SetDefaultAccount(nsIMsgAccount * aDefaultAccount) ;
  
  NS_IMETHOD GetAccounts(nsISupportsArray **_retval) ;
  
  /* string getAccountKey (in nsIMsgAccount account); */
  NS_IMETHOD getAccountKey(nsIMsgAccount *account, char **_retval) ;

  /* nsISupportsArray GetAllIdentities (); */
  NS_IMETHOD GetAllIdentities(nsISupportsArray **_retval) ;

  /* nsISupportsArray GetAllServers (); */
  NS_IMETHOD GetAllServers(nsISupportsArray **_retval) ;

  NS_IMETHOD LoadAccounts();
  NS_IMETHOD UnloadAccounts();
  NS_IMETHOD MigratePrefs();
  NS_IMETHOD WriteToFolderCache(nsIMsgFolderCache *folderCache);

  /* nsIMsgIdentity GetIdentityByKey (in string key); */
  NS_IMETHOD GetIdentityByKey(const char *key, nsIMsgIdentity **_retval);

  NS_IMETHOD FindServer(const char* username,
                        const char* hostname,
                        const char *type,
                        nsIMsgIncomingServer* *aResult);

  NS_IMETHOD GetIdentitiesForServer(nsIMsgIncomingServer *server,
                                    nsISupportsArray **_retval);

  NS_IMETHOD GetServersForIdentity(nsIMsgIdentity *identity,
                                   nsISupportsArray **_retval);

  //Add/remove an account to/from the Biff Manager if it has Biff turned on.
  nsresult AddAccountToBiff(nsIMsgAccount *account);
  nsresult RemoveAccountFromBiff(nsIMsgAccount *account);
  
private:
  PRBool m_accountsLoaded;
  
  nsHashtable *m_accounts;
  nsCOMPtr<nsIMsgAccount> m_defaultAccount;

  nsHashKey *findAccount(nsIMsgAccount *);
  // hash table enumerators

  // add the server to the nsISupportsArray closure
  static PRBool addServerToArray(nsHashKey *aKey, void *aData, void *closure);

  // add all identities in the account
  static PRBool addAccountsToArray(nsHashKey *aKey, void *aData,
                                   void* closure);
  static PRBool addIdentitiesToArray(nsHashKey *aKey, void *aData,
                                     void *closure);
  static PRBool hashTableFindAccount(nsHashKey *aKey, void *aData,
                                     void *closure);
  static PRBool hashTableFindFirst(nsHashKey *aKey, void *aData,
                                   void *closure);

  static PRBool hashTableRemoveAccount(nsHashKey *aKey, void *aData,
                                       void *closure);

  static PRBool hashTableWriteFolderCache(nsHashKey *aKey, void *aData,
                                       void *closure);

  static PRBool findIdentitiesForServer(nsHashKey *aKey,
                                        void *aData, void *closure);
  static PRBool findServersForIdentity (nsHashKey *aKey,
                                        void *aData, void *closure);

  // remove all of the servers from the Biff Manager
  static PRBool hashTableRemoveAccountFromBiff(nsHashKey *aKey, void *aData, void *closure);

  // nsISupportsArray enumerators
  static PRBool findServerByName(nsISupports *aElement, void *data);
  static PRBool findIdentityByKey(nsISupports *aElement, void *data);

  PRBool isUnique(nsIMsgIncomingServer *server);
  nsresult upgradePrefs();
  nsresult createSpecialFile(nsFileSpec & dir, const char *specialFileName);
  
  PRInt32 MigrateImapAccounts(nsIMsgIdentity *identity);
  nsresult MigrateImapAccount(nsIMsgIdentity *identity, const char *hostname, PRInt32 accountNum);
  
  PRInt32 MigratePopAccounts(nsIMsgIdentity *identity);
  
  PRInt32 MigrateNewsAccounts(nsIMsgIdentity *identity, PRInt32 baseAccountNum);
  nsresult MigrateNewsAccount(nsIMsgIdentity *identity, const char *hostname, const char *newsrcfile, PRInt32 accountNum);

  nsIMsgAccount *LoadAccount(nsString& accountKey);
  nsresult SetPasswordForServer(nsIMsgIncomingServer * server);
  nsIPref *m_prefs;
};


NS_IMPL_ISUPPORTS(nsMsgAccountManager, GetIID());

nsMsgAccountManager::nsMsgAccountManager() :
  m_accountsLoaded(PR_FALSE),
  m_defaultAccount(null_nsCOMPtr()),
  m_prefs(0)
{
  NS_INIT_REFCNT();
  m_accounts = new nsHashtable;
}

nsMsgAccountManager::~nsMsgAccountManager()
{
  
  if (m_prefs) nsServiceManager::ReleaseService(kPrefServiceCID, m_prefs);
  UnloadAccounts();
  delete m_accounts;
}

/*
 * generate a relevant, understandable key from the given server and
 * identity, make sure it's unique, then pass to AddAccount
 *
 */
NS_IMETHODIMP
nsMsgAccountManager::CreateAccount(nsIMsgIncomingServer *server,
                                   nsIMsgIdentity *identity,
                                   nsIMsgAccount **_retval)
{
  const char *key = "default";

  return CreateAccountWithKey(server, identity, key, _retval);
}

/* nsIMsgAccount AddAccount (in nsIMsgIncomingServer server, in nsIMsgIdentity
   identity, in string accountKey); */
NS_IMETHODIMP
nsMsgAccountManager::CreateAccountWithKey(nsIMsgIncomingServer *server,
                                          nsIMsgIdentity *identity, 
                                          const char *accountKey,
                                          nsIMsgAccount **_retval)
{
  nsresult rv;
  nsCOMPtr<nsIMsgAccount> account=nsnull;

  rv = nsComponentManager::CreateInstance(kMsgAccountCID,
                                          nsnull,
                                          nsCOMTypeInfo<nsIMsgAccount>::GetIID(),
                                          getter_AddRefs(account));

  if (NS_SUCCEEDED(rv)) {
    rv = account->SetIncomingServer(server);
    rv = account->addIdentity(identity);
  }

  account->SetKey(NS_CONST_CAST(char*, accountKey));
  rv = AddAccount(account);

  // pointer has already been addreffed by CreateInstance
  if (NS_SUCCEEDED(rv))
    *_retval = account;

  return rv;
  
}

NS_IMETHODIMP
nsMsgAccountManager::AddAccount(nsIMsgAccount *account)
{
    nsresult rv;
    rv = LoadAccounts();
    if (NS_FAILED(rv)) return rv;


    nsXPIDLCString accountKey;
    account->GetKey(getter_Copies(accountKey));
    nsStringKey key(accountKey);
    
    // check for uniqueness
    nsCOMPtr<nsIMsgIncomingServer> server;
    rv = account->GetIncomingServer(getter_AddRefs(server));
    if (NS_FAILED(rv)) return rv;
    if (!isUnique(server)) {
#ifdef DEBUG_ACCOUNTMANAGER
      printf("nsMsgAccountManager::AddAccount(%s) failed because server was not unique\n", (const char*)accountKey);
#endif
      // this means the server was found, which is bad.
      return NS_ERROR_UNEXPECTED;
    }

#ifdef DEBUG_ACCOUNTMANAGER
    printf("Adding account %s\n", (const char *)accountKey);
#endif

    if (m_accounts->Exists(&key))
      return NS_ERROR_UNEXPECTED;
        
    // do an addref for the storage in the hash table
    NS_ADDREF(account);
    m_accounts->Put(&key, account);
    
    if (m_accounts->Count() == 1)
      m_defaultAccount = dont_QueryInterface(account);

	AddAccountToBiff(account);
    return NS_OK;
}

nsresult
nsMsgAccountManager::AddAccountToBiff(nsIMsgAccount *account)
{
	nsresult rv;
	nsCOMPtr<nsIMsgIncomingServer> server;
	PRBool doBiff = PR_FALSE;

	NS_WITH_SERVICE(nsIMsgBiffManager, biffManager, kMsgBiffManagerCID, &rv);

	if(NS_FAILED(rv))
		return rv;

	rv = account->GetIncomingServer(getter_AddRefs(server));

	if(NS_SUCCEEDED(rv))
	{
		rv = server->GetDoBiff(&doBiff);
	}

	if(NS_SUCCEEDED(rv) && doBiff)
	{
		rv = biffManager->AddServerBiff(server);
	}

	return rv;
}

nsresult nsMsgAccountManager::RemoveAccountFromBiff(nsIMsgAccount *account)
{
	nsresult rv;
	nsCOMPtr<nsIMsgIncomingServer> server;
	PRBool doBiff = PR_FALSE;

	NS_WITH_SERVICE(nsIMsgBiffManager, biffManager, kMsgBiffManagerCID, &rv);

	if(NS_FAILED(rv))
		return rv;

	rv = account->GetIncomingServer(getter_AddRefs(server));

	if(NS_SUCCEEDED(rv))
	{
		rv = server->GetDoBiff(&doBiff);
	}

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
  
  nsHashKey *key = findAccount(aAccount);
  if (!key) return NS_ERROR_UNEXPECTED;
  
  NS_RELEASE(aAccount);
  m_accounts->Remove(key);
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
#endif
    
    findAccountEntry entry = { nsnull, PR_FALSE, nsnull };

    m_accounts->Enumerate(hashTableFindFirst, (void *)&entry);

    // are there ANY entries?
    if (!entry.found) return NS_ERROR_UNEXPECTED;

#ifdef DEBUG_ACCOUNTMANAGER
    printf("Account (%p) found\n", entry.account);
#endif
    m_defaultAccount = dont_QueryInterface(entry.account);
    
  }
  *aDefaultAccount = m_defaultAccount;
  NS_ADDREF(*aDefaultAccount);
  return NS_OK;
}


NS_IMETHODIMP
nsMsgAccountManager::SetDefaultAccount(nsIMsgAccount * aDefaultAccount)
{
  if (!findAccount(aDefaultAccount)) return NS_ERROR_UNEXPECTED;
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


/* map account->key by enumerating all accounts and returning the key
 * when the account is found */
nsHashKey *
nsMsgAccountManager::findAccount(nsIMsgAccount *aAccount)
{
  if (!aAccount) return nsnull;
  
  findAccountEntry entry = {aAccount, PR_FALSE, nsnull };

  m_accounts->Enumerate(hashTableFindAccount, (void *)&entry);

  /* at the end of enumeration, entry should contain the results */
  if (entry.found) return entry.hashKey;

  return nsnull;
}

/* this hashtable enumeration function will search for the given
 * account, and stop the enumeration if it's found.
 * to report results, it will set the found and account fields
 * of the closure
 */
PRBool
nsMsgAccountManager::hashTableFindAccount(nsHashKey *aKey,
                                          void *aData,
                                          void *closure)
{
  // compare them as nsIMsgAccount*'s - not sure if that
  // really makes a difference
  nsIMsgAccount *thisAccount = (nsIMsgAccount *)aData;
  findAccountEntry *entry = (findAccountEntry *)closure;

  // when found, fix up the findAccountEntry
  if (thisAccount == entry->account) {
    entry->found = PR_TRUE;
    entry->hashKey = aKey;
    return PR_FALSE;               // stop the madness!
  }
  return PR_TRUE;
}

/* enumeration that stops at first found account */
PRBool
nsMsgAccountManager::hashTableFindFirst(nsHashKey *aKey,
                                          void *aData,
                                          void *closure)
{
  
  findAccountEntry *entry = (findAccountEntry *)closure;

  entry->hashKey = aKey;
  entry->account = (nsIMsgAccount *)aData;
  entry->found = PR_TRUE;

#ifdef DEBUG_ACCOUNTMANAGER
  printf("hashTableFindFirst: returning first account %p/%p\n",
         aData, entry->account);
#endif
  
  return PR_FALSE;                 // stop enumerating immediately
}

// enumaration for removing accounts from the BiffManager
PRBool nsMsgAccountManager::hashTableRemoveAccountFromBiff(nsHashKey *aKey, void *aData, void *closure)
{
	nsIMsgAccount* account = (nsIMsgAccount *)aData;
	nsMsgAccountManager *accountManager = (nsMsgAccountManager*)closure;

	accountManager->RemoveAccountFromBiff(account);
	
	return PR_TRUE;

}

// enumeration for removing accounts from the account manager
PRBool
nsMsgAccountManager::hashTableRemoveAccount(nsHashKey *aKey, void *aData,
                                            void*closure)
{
  nsIMsgAccount* account = (nsIMsgAccount*)aData;

  // remove from hashtable
  NS_RELEASE(account);

  return PR_TRUE;
}


// enumaration for writing out accounts to folder cache.
PRBool nsMsgAccountManager::hashTableWriteFolderCache(nsHashKey *aKey, void *aData, void *closure)
{
	nsIMsgAccount* account = (nsIMsgAccount *)aData;
	nsIMsgFolderCache *folderCache = (nsIMsgFolderCache *) closure;

	nsCOMPtr <nsIMsgIncomingServer> server;

	account->GetIncomingServer(getter_AddRefs(server));

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
  
  m_accounts->Enumerate(addAccountsToArray, (void *)accounts);

  *_retval = accounts;
  NS_ADDREF(*_retval);

  return NS_OK;
}

/* string getAccountKey (in nsIMsgAccount account); */
NS_IMETHODIMP
nsMsgAccountManager::getAccountKey(nsIMsgAccount *account, char **_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


PRBool
nsMsgAccountManager::addAccountsToArray(nsHashKey *key, void *aData, void *closure)
{
  nsISupportsArray *array = (nsISupportsArray*)closure;
  nsIMsgAccount *account = (nsIMsgAccount*) aData;

  array->AppendElement(account);
  return PR_TRUE;
}

PRBool
nsMsgAccountManager::addIdentitiesToArray(nsHashKey *key, void *aData, void *closure)
{
  nsISupportsArray *array = (nsISupportsArray*)closure;
  nsIMsgAccount* account = (nsIMsgAccount *)aData;

  nsCOMPtr<nsISupportsArray> identities;

  // add each list of identities to the list
  nsresult rv = NS_OK;
  rv = account->GetIdentities(getter_AddRefs(identities));
  array->AppendElements(identities);
  
  return PR_TRUE;
}

PRBool
nsMsgAccountManager::addServerToArray(nsHashKey *key, void *aData,
                                      void *closure)
{
  nsISupportsArray *array = (nsISupportsArray *)closure;
  nsIMsgAccount *account = (nsIMsgAccount *)aData;
  nsCOMPtr<nsIMsgIncomingServer> server;
  nsresult rv = account->GetIncomingServer(getter_AddRefs(server));
  if (NS_SUCCEEDED(rv))
    array->AppendElement(server);
  return PR_TRUE;
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
  m_accounts->Enumerate(addIdentitiesToArray, (void *)identities);

  // convert nsISupportsArray->nsISupportsArray
  // when do we free the nsISupportsArray?
  *_retval = identities;
  NS_ADDREF(*_retval);
  return rv;
}

/* nsISupportsArray GetAllServers (); */
NS_IMETHODIMP
nsMsgAccountManager::GetAllServers(nsISupportsArray **_retval)
{
  nsresult rv;
  rv = LoadAccounts();
  if (NS_FAILED(rv)) return rv;
  
  nsISupportsArray *servers;
  rv = NS_NewISupportsArray(&servers);

  if (NS_FAILED(rv)) return rv;

  // convert hash table->nsISupportsArray of servers
  if (m_accounts)
    m_accounts->Enumerate(addServerToArray, (void *)servers);
  
  *_retval = servers;
  return rv;
}

nsresult
nsMsgAccountManager::LoadAccounts()
{
  nsresult rv;

  // for now safeguard multiple calls to this function
  if (m_accountsLoaded)
    return NS_OK;
  
  m_accountsLoaded = PR_TRUE;
  // get the prefs service
  if (!m_prefs) {
    rv = nsServiceManager::GetService(kPrefServiceCID,
                                      nsCOMTypeInfo<nsIPref>::GetIID(),
                                      (nsISupports**)&m_prefs);
    if (NS_FAILED(rv)) return rv;
  }

  // mail.accountmanager.accounts is the main entry point for all accounts
  nsXPIDLCString accountList;
  rv = m_prefs->CopyCharPref("mail.accountmanager.accounts",
                             getter_Copies(accountList));

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
    nsString str("",eOneByte);

    token = nsCRT::strtok(rest, ",", &rest);
    while (token && *token) {
      str = token;
      str.StripWhitespace();
      
      if (str != "") {
        account = getter_AddRefs(LoadAccount(str));
        if (account) {
          rv = AddAccount(account);
          if (NS_FAILED(rv)) {

#ifdef DEBUG_ACCOUNTMANAGER
            printf("Error adding account %s\n", str.GetBuffer());
#endif
            // warn the user here?
            // don't break out of the loop because there might
            // be other accounts worth loading
          }
        }
        str = "";
      }
      token = nsCRT::strtok(rest, ",", &rest);
    }

    /* finished loading accounts */
    return NS_OK;
}

nsresult
nsMsgAccountManager::UnloadAccounts()
{
  // release the default account
  m_defaultAccount=nsnull;
  m_accounts->Enumerate(hashTableRemoveAccountFromBiff, this);
  m_accounts->Enumerate(hashTableRemoveAccount, this);
  m_accounts->Reset();
  m_accountsLoaded = PR_FALSE;
  return NS_OK;
}


NS_IMETHODIMP nsMsgAccountManager::WriteToFolderCache(nsIMsgFolderCache *folderCache)
{
	m_accounts->Enumerate(hashTableWriteFolderCache, folderCache);
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

nsIMsgAccount *
nsMsgAccountManager::LoadAccount(nsString& accountKey)
{
#ifdef DEBUG_ACCOUNTMANAGER
  printf("Loading preferences for account: %s\n", accountKey.GetBuffer());
#endif
  nsIMsgAccount *account = nsnull;
  nsresult rv;
  rv = nsComponentManager::CreateInstance(kMsgAccountCID,
                                          nsnull,
                                          nsCOMTypeInfo<nsIMsgAccount>::GetIID(),
                                          (void **)&account);
#ifdef DEBUG_ACCOUNTMANAGER
  if (NS_FAILED(rv)) printf("Could not create an account\n");
#endif
  
  if (NS_FAILED(rv)) return nsnull;

  account->SetKey(NS_CONST_CAST(char*,accountKey.GetBuffer()));
  
  return account;
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
	rv = specialFile->exists(&specialFileExists);
	if (NS_FAILED(rv)) return rv;

	if (!specialFileExists) {
		rv = specialFile->touch();
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
  
	if (!m_prefs) {
		rv = nsServiceManager::GetService(kPrefServiceCID,
                                      nsCOMTypeInfo<nsIPref>::GetIID(),
                                      (nsISupports**)&m_prefs);
		if (NS_FAILED(rv)) return rv;
	}

    rv = m_prefs->GetIntPref("mail.server_type", &oldMailType);
    if (NS_FAILED(rv)) {
#ifdef DEBUG_ACCOUNTMANAGER
        printf("Tried to upgrade old prefs, but couldn't find server type!\n");
#endif
        return rv;
    }

    nsCOMPtr<nsIMsgIdentity> identity;
    rv = nsComponentManager::CreateInstance(kMsgIdentityCID,
                                            nsnull,
                                            nsCOMTypeInfo<nsIMsgIdentity>::GetIID(),
                                            getter_AddRefs(identity));
    identity->SetKey("identity1");

    // identity stuff
    rv = m_prefs->CopyCharPref("mail.identity.useremail", &oldstr);
    if (NS_SUCCEEDED(rv)) {
      identity->SetEmail(oldstr);
      PR_FREEIF(oldstr);
      oldstr = nsnull;
    }
    
    rv = m_prefs->CopyCharPref("mail.identity.username", &oldstr);
    if (NS_SUCCEEDED(rv)) {
      identity->SetFullName(oldstr);
      PR_FREEIF(oldstr);
      oldstr = nsnull;
    }
    
    rv = m_prefs->CopyCharPref("mail.identity.reply_to", &oldstr);
    if (NS_SUCCEEDED(rv)) {
      identity->SetReplyTo(oldstr);
      PR_FREEIF(oldstr);
      oldstr = nsnull;
    }
    
    rv = m_prefs->CopyCharPref("mail.identity.organization", &oldstr);
    if (NS_SUCCEEDED(rv)) {
      identity->SetOrganization(oldstr);
      PR_FREEIF(oldstr);
      oldstr = nsnull;
    }
    
    rv = m_prefs->GetBoolPref("mail.compose_html", &oldbool);
    if (NS_SUCCEEDED(rv)) {
      identity->SetComposeHtml(oldbool);
    }
    
    rv = m_prefs->CopyCharPref("network.hosts.smtp_server", &oldstr);
    if (NS_SUCCEEDED(rv)) {
      identity->SetSmtpHostname(oldstr);
      PR_FREEIF(oldstr);
      oldstr = nsnull;
    }
    
    rv = m_prefs->CopyCharPref("mail.smtp_name", &oldstr);
    if (NS_SUCCEEDED(rv)) {
      identity->SetSmtpUsername(oldstr);
      PR_FREEIF(oldstr);
      oldstr = nsnull;
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
    m_prefs->SetCharPref("mail.accountmanager.accounts","account1");
    m_prefs->SetCharPref("mail.account.account1.identities","identity1");
    m_prefs->SetCharPref("mail.account.account1.server","server1");

    char *oldAccountsValueBuf=nsnull;
    char newAccountsValueBuf[1024];
    char prefNameBuf[1024];
    char prefValueBuf[1024];

    // handle the rest of the accounts.
    for (PRInt32 i=2;i<=numAccounts;i++) {
      rv = m_prefs->CopyCharPref("mail.accountmanager.accounts", &oldAccountsValueBuf);
      if (NS_SUCCEEDED(rv)) {
        PR_snprintf(newAccountsValueBuf, 1024, "%s,account%d",oldAccountsValueBuf,i);
        m_prefs->SetCharPref("mail.accountmanager.accounts", newAccountsValueBuf);
      }
      PR_FREEIF(oldAccountsValueBuf);
      oldAccountsValueBuf = nsnull;
      
      PR_snprintf(prefNameBuf, 1024, "mail.account.account%d.identities", i);
      // in 4.x, we only had one identity, so everyone gets that one in 5.0 when they upgrade
      m_prefs->SetCharPref(prefNameBuf, "identity1");

      PR_snprintf(prefNameBuf, 1024, "mail.account.account%d.server", i);
      PR_snprintf(prefValueBuf, 1024, "server%d", i);
      m_prefs->SetCharPref(prefNameBuf, prefValueBuf);
    }

    return NS_OK;
}

nsresult
nsMsgAccountManager::SetPasswordForServer(nsIMsgIncomingServer * server)
{
  PRBool retval = PR_FALSE;
  nsString password;

#ifdef PROMPTPASSWORDWORKS 
  nsresult rv;
  NS_WITH_SERVICE(nsINetSupportDialogService,dialog,kCNetSupportDialogCID,&rv);
  if (NS_FAILED(rv)) return rv;

  if (dialog) {
    nsString message("", eOneByte);

    nsXPIDLCString thisHostname;
    rv = server->GetHostName(getter_Copies(thisHostname));
    if (NS_FAILED(rv)) return rv;
    
    message = "We can't migrate 4.5 prefs yet.  Enter password for the server at ";
    message += thisHostname;
    message += ".  Keep in mind, it will be stored in the clear in your prefs50.js file";
    
    dialog->PromptPassword(message, password, &retval);
  }
#endif /* PROMPTPASSWORDWORKS */

  if (retval) {
#ifdef DEBUG_ACCOUNTMANAGER
    printf("password = %s\n", password.GetBuffer());
#endif
    server->SetPassword((char *)(password.GetBuffer()));        
  }
  else {
    server->SetPassword("enter your clear text password here");
  }
  
  return NS_OK;
}

PRInt32
nsMsgAccountManager::MigratePopAccounts(nsIMsgIdentity *identity)
{
  nsresult rv;
  
  nsCOMPtr<nsIMsgAccount> account;
  nsCOMPtr<nsIMsgIncomingServer> server;
  
  rv = nsComponentManager::CreateInstance(kMsgAccountCID,
                                          nsnull,
                                          nsCOMTypeInfo<nsIMsgAccount>::GetIID(),
                                          getter_AddRefs(account));
  if (NS_FAILED(rv)) return 0;

  rv = nsComponentManager::CreateInstance("component://netscape/messenger/server&type=pop3",
                                          nsnull,
                                          nsCOMTypeInfo<nsIMsgIncomingServer>::GetIID(),
                                          getter_AddRefs(server));
  if (NS_FAILED(rv)) return 0;

  account->SetKey("account1");
  server->SetKey("server1");
  
  account->SetIncomingServer(server);
  account->addIdentity(identity);

  // adds account to the hash table.
  AddAccount(account);

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
	
    rv = m_prefs->CopyCharPref("mail.pop_name", &oldstr);
    if (NS_SUCCEEDED(rv)) {
      server->SetUsername(oldstr);
      PR_FREEIF(oldstr);
      oldstr = nsnull;
    }
    
#ifdef CAN_UPGRADE_4x_PASSWORDS
    rv = m_prefs->CopyCharPref("mail.pop_password", &oldstr);
    if (NS_SUCCEEDED(rv)) {
      server->SetPassword(oldstr);
      PR_FREEIF(oldstr);
      oldstr = nsnull;
    }
#else
    rv = SetPasswordForServer(server);
    if (NS_FAILED(rv)) return rv;
#endif /* CAN_UPGRADE_4x_PASSWORDS */

    char *hostname=nsnull;
    rv = m_prefs->CopyCharPref("network.hosts.pop_server", &hostname);
    if (NS_SUCCEEDED(rv)) {
      server->SetHostName(hostname);
    }
    
    rv = m_prefs->GetBoolPref("mail.check_new_mail", &oldbool);
    if (NS_SUCCEEDED(rv)) {
      server->SetDoBiff(oldbool);
    }
    
    rv = m_prefs->GetIntPref("mail.check_time", &oldint);
    if (NS_SUCCEEDED(rv)) {
      server->SetBiffMinutes(oldint);
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
    
    rv = mailDir->exists(&dirExists);
    if (NS_FAILED(rv)) return 0;
    
    if (!dirExists) {
      mailDir->createDir();
    }
    
    char *str = nsnull;
    mailDir->GetNativePath(&str);
    
    if (str && *str) {
      server->SetLocalPath(str);
      PR_FREEIF(str);
      str = nsnull;
    }
    
    rv = mailDir->exists(&dirExists);
    if (NS_FAILED(rv)) return 0;
    
    if (!dirExists) {
      mailDir->createDir();
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
	
    rv = m_prefs->GetBoolPref("mail.leave_on_server", &oldbool);
    if (NS_SUCCEEDED(rv)) {
      popServer->SetLeaveMessagesOnServer(oldbool);
    }
    
    rv = m_prefs->GetBoolPref("mail.delete_mail_left_on_server", &oldbool);
    if (NS_SUCCEEDED(rv)) {
      popServer->SetDeleteMailLeftOnServer(oldbool);
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
  rv = m_prefs->CopyCharPref("network.hosts.imap_servers", &hostList);
  if (NS_FAILED(rv)) return 0;

  if (!hostList || !*hostList) return 0;
  
  char *token = nsnull;
  char *rest = NS_CONST_CAST(char*,(const char*)hostList);
  nsString str("",eOneByte);
      
  token = nsCRT::strtok(rest, ",", &rest);
  while (token && *token) {
    str = token;
    str.StripWhitespace();
    
    if (str != "") {
	  numAccounts++;
      // str is the hostname
      if (NS_FAILED(MigrateImapAccount(identity,str.GetBuffer(),numAccounts))) {
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
nsMsgAccountManager::MigrateImapAccount(nsIMsgIdentity *identity, const char *hostname, PRInt32 accountNum)
{
  nsresult rv;
  PRInt32 oldint;
  PRBool oldbool;

  if (!hostname) return NS_ERROR_NULL_POINTER;
  if (accountNum < 1) return NS_ERROR_FAILURE;
  
  nsCOMPtr<nsIMsgAccount> account;
  nsCOMPtr<nsIMsgIncomingServer> server;
  
  rv = nsComponentManager::CreateInstance(kMsgAccountCID,
                                          nsnull,
                                          nsCOMTypeInfo<nsIMsgAccount>::GetIID(),
                                          getter_AddRefs(account));
  if (NS_FAILED(rv)) return rv;

  rv = nsComponentManager::CreateInstance("component://netscape/messenger/server&type=imap",
                                          nsnull,
                                          nsCOMTypeInfo<nsIMsgIncomingServer>::GetIID(),
                                          getter_AddRefs(server));
  if (NS_FAILED(rv)) return rv;

  char accountStr[1024];
  char serverStr[1024];

  PR_snprintf(accountStr,1024,"account%d",accountNum);
#ifdef DEBUG_ACCOUNTMANAGER
  printf("account str = %s\n",accountStr);
#endif
  account->SetKey(accountStr);
  PR_snprintf(serverStr,1024,"server%d",accountNum);
#ifdef DEBUG_ACCOUNTMANAGER
  printf("server str = %s\n",serverStr);
#endif
  server->SetKey(serverStr);
  
  account->SetIncomingServer(server);
  account->addIdentity(identity);

  // adds account to the hash table.
  AddAccount(account);

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

  char prefName[1024];
  PR_snprintf(prefName, 1024, "mail.imap.server.%s.userName",hostname);
  rv = m_prefs->CopyCharPref(prefName, &oldstr);
  if (NS_SUCCEEDED(rv)) {
    server->SetUsername(oldstr);
    PR_FREEIF(oldstr);
    oldstr = nsnull;
  }

#ifdef CAN_UPGRADE_4x_PASSWORDS
  // upgrade the password
  PR_snprintf(prefName, 1024, "mail.imap.server.%s.password",hostname);
  rv = m_prefs->CopyCharPref(prefName, &oldstr);
  if (NS_SUCCEEDED(rv)) {
    server->SetPassword("enter your clear text password here");
    PR_FREEIF(oldstr);
    oldstr = nsnull;
  }
#else
  rv = SetPasswordForServer(server);
  if (NS_FAILED(rv)) return rv;
#endif /* CAN_UPGRADE_4x_PASSWORDS */

  // upgrade the biff prefs
  PR_snprintf(prefName, 1024, "mail.imap.server.%s.check_new_mail",hostname);
  rv = m_prefs->GetBoolPref(prefName, &oldbool);
  if (NS_SUCCEEDED(rv)) {
    server->SetDoBiff(oldbool);
  }

  PR_snprintf(prefName, 1024, "mail.imap.server.%s.check_time",hostname);
  rv = m_prefs->GetIntPref(prefName, &oldint);
  if (NS_SUCCEEDED(rv)) {
    server->SetBiffMinutes(oldint);
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
  
  rv = imapMailDir->exists(&dirExists);
  if (NS_FAILED(rv)) return rv;
  
  if (!dirExists) {
    imapMailDir->createDir();
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
    
	rv = m_prefs->CopyCharPref("news.directory", &news_directory_value);
	if (NS_SUCCEEDED(rv)) {
      dirWithTheNewsrcFiles = news_directory_value;
      PR_FREEIF(news_directory_value);
      news_directory_value = nsnull;
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
	
	nsCOMPtr<nsIMsgAccount> account;
	nsCOMPtr<nsIMsgIncomingServer> server;
	
	rv = nsComponentManager::CreateInstance(kMsgAccountCID,
		nsnull,
		nsCOMTypeInfo<nsIMsgAccount>::GetIID(),
		getter_AddRefs(account));
	if (NS_FAILED(rv)) return rv;
	
	rv = nsComponentManager::CreateInstance("component://netscape/messenger/server&type=nntp",
		nsnull,
		nsCOMTypeInfo<nsIMsgIncomingServer>::GetIID(),
		getter_AddRefs(server));
	if (NS_FAILED(rv)) return rv;

	char accountStr[1024];
	char serverStr[1024];
	
	PR_snprintf(accountStr,1024,"account%d",accountNum);
#ifdef DEBUG_ACCOUNTMANAGER
	printf("account str = %s\n",accountStr);
#endif
	account->SetKey(accountStr);
	PR_snprintf(serverStr,1024,"server%d",accountNum);
#ifdef DEBUG_ACCOUNTMANAGER
	printf("server str = %s\n",serverStr);
#endif
	server->SetKey(serverStr);
	
	account->SetIncomingServer(server);
	account->addIdentity(identity);
	
	// adds account to the hash table.
	AddAccount(account);
	
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
	char *oldstr = nsnull;
	
	// we don't handle nntp servers that accept username / passwords yet
	char prefName[1024];
	PR_snprintf(prefName, 1024, "???nntp.server.%s.userName",hostname);
	rv = m_prefs->CopyCharPref(prefName, &oldstr);
	if (NS_SUCCEEDED(rv)) {
		server->SetUsername(oldstr);
		PR_FREEIF(oldstr);
		oldstr = nsnull;
	}

#ifdef CAN_UPGRADE_4x_PASSWORDS
	// upgrade the password
	PR_snprintf(prefName, 1024, "???nntp.server.%s.password",hostname);
	rv = m_prefs->CopyCharPref(prefName, &oldstr);
	if (NS_SUCCEEDED(rv)) {
		server->SetPassword("enter your clear text password here");
		PR_FREEIF(oldstr);
		oldstr = nsnull;
	}
#else
	rv = SetPasswordForServer(server);
	if (NS_FAILED(rv)) return rv;
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
	nsAutoString alteredHost ((const char *) "host-", eOneByte);
	alteredHost += hostname;
	NS_MsgHashIfNecessary(alteredHost);	
	dir += alteredHost;

	rv = NS_NewFileSpecWithSpec(dir, getter_AddRefs(newsDir));
	if (NS_FAILED(rv)) return rv;

	char *str = nsnull;
	newsDir->GetNativePath(&str);
	
	if (str && *str) {
		server->SetLocalPath(str);
		PR_FREEIF(str);
		str = nsnull;
	}
	
	rv = newsDir->exists(&dirExists);
	if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
	
	if (!dirExists) {
		newsDir->createDir();
	}
	
	nntpServer->SetNewsrcFilePath((char *)newsrcfile);

	return NS_OK;
}

NS_IMETHODIMP
nsMsgAccountManager::GetIdentityByKey(const char *key,
                                      nsIMsgIdentity **_retval)
{
  // there might be a better way, do it the cheesy way for now
  nsresult rv;
  
  nsCOMPtr<nsISupportsArray> identities;
  rv = GetAllIdentities(getter_AddRefs(identities));

  findIdentityByKeyEntry findEntry;
  findEntry.key = key;
  findEntry.identity = nsnull;

  identities->EnumerateForwards(findIdentityByKey, (void *)&findEntry);

  if (findEntry.identity) {
    *_retval = findEntry.identity;
    NS_ADDREF(*_retval);
  }
  return NS_OK;

}

PRBool
nsMsgAccountManager::findIdentityByKey(nsISupports *aElement, void *data)
{
  nsresult rv;
  nsCOMPtr<nsIMsgIdentity> identity = do_QueryInterface(aElement, &rv);
  if (NS_FAILED(rv)) return PR_TRUE;

  findIdentityByKeyEntry *entry = (findIdentityByKeyEntry*) data;

  nsXPIDLCString key;
  rv = identity->GetKey(getter_Copies(key));
  if (NS_FAILED(rv)) return rv;
  
  if (key && entry->key && !PL_strcmp(key, entry->key)) {
    entry->identity = identity;
    NS_ADDREF(entry->identity);
    return PR_FALSE;
  }

  return PR_TRUE;
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
  
  servers->EnumerateForwards(findServerByName, (void *)&serverInfo);

  if (!serverInfo.server) return NS_ERROR_UNEXPECTED;
  *aResult = serverInfo.server;
  NS_ADDREF(*aResult);
  
  return NS_OK;

}


// if the aElement matches the given hostname, add it to the given array
PRBool
nsMsgAccountManager::findServerByName(nsISupports *aElement, void *data)
{
  nsCOMPtr<nsIMsgIncomingServer> server = do_QueryInterface(aElement);
  if (!server) return PR_TRUE;

  findServerEntry *entry = (findServerEntry*) data;

  nsresult rv;
  
  nsXPIDLCString thisHostname;
  rv = server->GetHostName(getter_Copies(thisHostname));
  if (NS_FAILED(rv)) return PR_TRUE;

  char *username=nsnull;
  rv = server->GetUsername(&username);
  if (NS_FAILED(rv)) return PR_TRUE;
  if (!username) username=PL_strdup("");
  
  nsXPIDLCString thisType;
  rv = server->GetType(getter_Copies(thisType));
  if (NS_FAILED(rv)) return PR_TRUE;
  
  if (PL_strcasecmp(entry->hostname, thisHostname)==0 &&
      PL_strcmp(entry->username, username)==0 &&
      PL_strcmp(entry->type, thisType)==0) {
    entry->server = server;
    return PR_FALSE;            // stop on first find
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
  
  m_accounts->Enumerate(findIdentitiesForServer,
                        (void *)&identityInfo);

  // do an addref for the caller.
  *_retval = identities;
  NS_ADDREF(*_retval);

  return NS_OK;
}

PRBool
nsMsgAccountManager::findIdentitiesForServer(nsHashKey *key,
                                             void *aData, void *closure)
{
  nsresult rv;
  nsIMsgAccount* account = (nsIMsgAccount*)aData;
  findIdentitiesByServerEntry *entry = (findIdentitiesByServerEntry*)closure;
  
  nsIMsgIncomingServer* thisServer;
  rv = account->GetIncomingServer(&thisServer);
  if (NS_FAILED(rv)) return PR_TRUE;
  
  // ugh, this is bad. Need a better comparison method
  if (entry->server == thisServer) {
    // add all these elements to the nsISupports array
    nsCOMPtr<nsISupportsArray> theseIdentities;
    rv = account->GetIdentities(getter_AddRefs(theseIdentities));
    if (NS_SUCCEEDED(rv))
      rv = entry->identities->AppendElements(theseIdentities);
  }

  NS_RELEASE(thisServer);

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
  
  m_accounts->Enumerate(findServersForIdentity,
                        (void *)&serverInfo);

  // do an addref for the caller.
  *_retval = servers;
  NS_ADDREF(*_retval);

  return NS_OK;
}
  
PRBool
nsMsgAccountManager::findServersForIdentity(nsHashKey *key,
                                             void *aData, void *closure)
{
  nsresult rv;
  nsIMsgAccount* account = (nsIMsgAccount*)aData;
  findServersByIdentityEntry *entry = (findServersByIdentityEntry*)closure;

  nsCOMPtr<nsISupportsArray> identities;
  account->GetIdentities(getter_AddRefs(identities));

  PRUint32 idCount=0;
  identities->Count(&idCount);

  PRUint32 id;
  for (id=0; id<idCount; id++) {
    // don't use nsCOMPtrs because of the horrible pointer comparison below
    nsISupports *thisSupports = identities->ElementAt(id);
    if (!thisSupports) continue;
    
    nsIMsgIdentity *thisIdentity;
    rv = thisSupports->QueryInterface(nsCOMTypeInfo<nsIMsgIdentity>::GetIID(),
                                      (void **)&thisIdentity);
    NS_RELEASE(thisSupports);
    if (NS_SUCCEEDED(rv)) {

	  char *thisIdentityKey = nsnull, *identityKey = nsnull;

	  rv = entry->identity->GetKey(&identityKey);
	  if(NS_SUCCEEDED(rv))
		  rv = thisIdentity->GetKey(&thisIdentityKey);

	  //SP:  Don't compare pointers.  Identities are getting created multiple times with
	  // the same id.  Therefore ptr tests don't succeed, but key tests do.  We should probably
	  // be making sure we only have one identity per key.  But that is a different problem that
	  // needs to be solved.
      if (NS_SUCCEEDED(rv) && PL_strcmp(identityKey, thisIdentityKey) == 0) {
        nsCOMPtr<nsIMsgIncomingServer> thisServer;
        rv = account->GetIncomingServer(getter_AddRefs(thisServer));
        if (NS_SUCCEEDED(rv)) {
          entry->servers->AppendElement(thisServer);

          // release everything NOW since we're breaking out of the loop
          NS_RELEASE(thisIdentity);
		  if(thisIdentityKey)
		    PL_strfree(thisIdentityKey);
		  if(identityKey)
		    PL_strfree(identityKey);
          break;
        }
      }
	  if(thisIdentityKey)
		  PL_strfree(thisIdentityKey);
	  if(identityKey)
		  PL_strfree(identityKey);
      NS_RELEASE(thisIdentity);
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
