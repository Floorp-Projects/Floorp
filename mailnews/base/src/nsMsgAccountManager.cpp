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

#include "nsCRT.h"  // for nsCRT::strtok

// this should eventually be moved to the pop3 server for upgrading
#include "nsIPop3IncomingServer.h"

#if defined(DEBUG_alecf) || defined(DEBUG_sspitzer) || defined(DEBUG_seth)
#define DEBUG_ACCOUNTMANAGER 1
#endif

static NS_DEFINE_CID(kMsgAccountCID, NS_MSGACCOUNT_CID);
static NS_DEFINE_CID(kMsgIdentityCID, NS_MSGIDENTITY_CID);
static NS_DEFINE_CID(kPrefServiceCID, NS_PREF_CID);
static NS_DEFINE_CID(kMsgBiffManagerCID, NS_MSGBIFFMANAGER_CID);


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

  static PRBool findIdentitiesForServer(nsHashKey *aKey,
                                        void *aData, void *closure);
  static PRBool findServersForIdentity (nsHashKey *aKey,
                                        void *aData, void *closure);

  // remove all of the servers from the Biff Manager
  static PRBool hashTableRemoveAccountFromBiff(nsHashKey *aKey, void *aData, void *closure);

  // nsISupportsArray enumerators
  static PRBool findServerByName(nsISupports *aElement, void *data);

  PRBool isUnique(nsIMsgIncomingServer *server);
  nsresult upgradePrefs();
  nsIMsgAccount *LoadAccount(nsString& accountKey);
  
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
  // release the default account
  m_defaultAccount=nsnull;
  
  if (m_prefs) nsServiceManager::ReleaseService(kPrefServiceCID, m_prefs);
  m_accounts->Enumerate(hashTableRemoveAccountFromBiff, this);
  m_accounts->Enumerate(hashTableRemoveAccount, this);
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
                                          nsIMsgAccount::GetIID(),
                                          (void **)getter_AddRefs(account));

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


    // check for uniqueness
    nsCOMPtr<nsIMsgIncomingServer> server;
    rv = account->GetIncomingServer(getter_AddRefs(server));
    if (NS_FAILED(rv)) return rv;
    if (!isUnique(server)) {
      // this means the server was found, which is bad.
      return NS_ERROR_UNEXPECTED;
    }

    
    nsXPIDLCString accountKey;
    account->GetKey(getter_Copies(accountKey));
    nsCStringKey key(accountKey);
    
#ifdef DEBUG_alecf
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
  
  if (NS_SUCCEEDED(rv))
    return PR_FALSE;
  return PR_TRUE;
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

#ifdef DEBUG_alecf
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
  nsMsgAccountManager* accountManager = (nsMsgAccountManager*) closure;

  // remove from hashtable
  NS_RELEASE(account);
  accountManager->m_accounts->Remove(aKey);

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
                                      nsIPref::GetIID(),
                                      (nsISupports**)&m_prefs);
    if (NS_FAILED(rv)) return rv;
  }

  // mail.accountmanager.accounts is the main entry point for all accounts
  nsXPIDLCString accountList;
  rv = m_prefs->CopyCharPref("mail.accountmanager.accounts",
                             getter_Copies(accountList));

  if (NS_FAILED(rv) || !accountList || !accountList[0]) {
    // create default bogus accounts
    printf("No accounts. I'll try to migrate 4.x prefs..\n");
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
#ifdef DEBUG_ACCOUNTMANAGER
        printf("accountKey = %s\n", str.GetBuffer());
#endif
        account = getter_AddRefs(LoadAccount(str));
        if (account) {
          AddAccount(account);
        }
        else
          return NS_ERROR_NULL_POINTER;
        str = "";
      }
#ifdef DEBUG_ACCOUNTMANAGER
      else {
        printf("nothing between two commas. ignore and keep going...\n");
      }
#endif
      token = nsCRT::strtok(rest, ",", &rest);
    }

    /* finished loading accounts */
    return NS_OK;
}

nsIMsgAccount *
nsMsgAccountManager::LoadAccount(nsString& accountKey)
{

  printf("Loading preferences for account: %s\n", accountKey.GetBuffer());
  
  nsIMsgAccount *account = nsnull;
  nsresult rv;
  rv = nsComponentManager::CreateInstance(kMsgAccountCID,
                                          nsnull,
                                          nsIMsgAccount::GetIID(),
                                          (void **)&account);
#ifdef DEBUG_alecf
  if (NS_FAILED(rv)) printf("Could not create an account\n");
#endif
  
  if (NS_FAILED(rv)) return nsnull;

  account->SetKey(NS_CONST_CAST(char*,accountKey.GetBuffer()));
  
  return account;
}

nsresult
nsMsgAccountManager::upgradePrefs()
{
    nsresult rv;
    nsCOMPtr<nsIMsgAccount> account;
    nsCOMPtr<nsIMsgIdentity> identity;
    nsCOMPtr<nsIMsgIncomingServer> server;

    rv = nsComponentManager::CreateInstance(kMsgAccountCID,
                                            nsnull,
                                            nsIMsgAccount::GetIID(),
                                            (void **)&account);
    
    rv = nsComponentManager::CreateInstance(kMsgIdentityCID,
                                            nsnull,
                                            nsIMsgIdentity::GetIID(),
                                            (void **)&identity);

    const char* serverProgID;
    PRInt32 oldMailType;

    rv = m_prefs->GetIntPref("mail.server_type", &oldMailType);
    if (NS_FAILED(rv)) {
        printf("Tried to upgrade old prefs, but couldn't find server type!\n");
        return rv;
    }

    if ( oldMailType == 0)       // POP
        serverProgID="component://netscape/messenger/server&type=pop3";
    else if (oldMailType == 1)   // IMAP
        serverProgID="component://netscape/messenger/server&type=imap";
    else {
        printf("Unrecognized server type %d\n", oldMailType);
        return NS_ERROR_UNEXPECTED;
    }

    rv = nsComponentManager::CreateInstance(serverProgID,
                                            nsnull,
                                            nsIMsgIncomingServer::GetIID(),
                                            (void **)&server);
    account->SetKey("account1");
    server->SetKey("server1");
    identity->SetKey("identity1");
    
    account->SetIncomingServer(server);
    account->addIdentity(identity);
    AddAccount(account);

    // now upgrade all the prefs
    char *oldstr;
    PRInt32 oldint;
    PRBool oldbool;

    // identity stuff
    rv = m_prefs->CopyCharPref("mail.identity.useremail", &oldstr);
    if (NS_SUCCEEDED(rv)) {
        identity->SetEmail(oldstr);
        PR_Free(oldstr);
    }

    rv = m_prefs->CopyCharPref("mail.identity.vcard.fn", &oldstr);
    if (NS_SUCCEEDED(rv)) {
        identity->SetFullName(oldstr);
        PR_Free(oldstr);
    }
    
    rv = m_prefs->CopyCharPref("mail.identity.reply_to", &oldstr);
    if (NS_SUCCEEDED(rv)) {
        identity->SetReplyTo(oldstr);
        PR_Free(oldstr);
    }

    rv = m_prefs->CopyCharPref("mail.identity.organization", &oldstr);
    if (NS_SUCCEEDED(rv)) {
        identity->SetOrganization(oldstr);
        PR_Free(oldstr);
    }
    
    rv = m_prefs->GetBoolPref("mail.compose_html", &oldbool);
    if (NS_SUCCEEDED(rv)) {
        identity->SetComposeHtml(oldbool);
    }

    rv = m_prefs->CopyCharPref("mail.smtp_server", &oldstr);
    if (NS_SUCCEEDED(rv)) {
        identity->SetSmtpHostname(oldstr);
        PR_Free(oldstr);
    }
    
    rv = m_prefs->CopyCharPref("mail.smtp_name", &oldstr);
    if (NS_SUCCEEDED(rv)) {
        identity->SetSmtpUsername(oldstr);
        PR_Free(oldstr);
    }
        
    // generic server stuff

    // pop stuff
    // some of this ought to be moved out into the POP implementation
    nsCOMPtr<nsIPop3IncomingServer> popServer;
    popServer = do_QueryInterface(server, &rv);
    if (NS_SUCCEEDED(rv)) {

      rv = m_prefs->CopyCharPref("mail.pop_name", &oldstr);
      if (NS_SUCCEEDED(rv)) {
        server->SetUsername(oldstr);
        PR_Free(oldstr);
      }

      rv = m_prefs->CopyCharPref("mail.pop_password", &oldstr);
      if (NS_SUCCEEDED(rv)) {
        server->SetPassword(oldstr);
        PR_Free(oldstr);
      }

      rv = m_prefs->CopyCharPref("network.hosts.pop_server", &oldstr);
      if (NS_SUCCEEDED(rv)) {
        server->SetHostName(oldstr);
        PR_Free(oldstr);
      }

      rv = m_prefs->GetBoolPref("mail.check_new_mail", &oldbool);
      if (NS_SUCCEEDED(rv)) {
        server->SetDoBiff(oldbool);
      }
      
      rv = m_prefs->GetIntPref("mail.check_time", &oldint);
      if (NS_SUCCEEDED(rv)) {
        server->SetBiffMinutes(oldint);
      }

      rv = m_prefs->CopyCharPref("mail.directory", &oldstr);
      if (NS_SUCCEEDED(rv)) {
        server->SetLocalPath(oldstr);
        PR_Free(oldstr);
      }

      rv = m_prefs->GetBoolPref("mail.leave_on_server", &oldbool);
      if (NS_SUCCEEDED(rv)) {
        popServer->SetLeaveMessagesOnServer(oldbool);
      }
      
      rv = m_prefs->GetBoolPref("mail.delete_mail_left_on_server", &oldbool);
      if (NS_SUCCEEDED(rv)) {
        popServer->SetDeleteMailLeftOnServer(oldbool);
      }
      
      
    }

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
    rv = thisSupports->QueryInterface(nsIMsgIdentity::GetIID(),
                                      (void **)&thisIdentity);
    NS_RELEASE(thisSupports);
    if (NS_SUCCEEDED(rv)) {

      // if the identity is found, add this server to this element
      // bad bad bad - pointer comparisons are evil.
      if (entry->identity == thisIdentity) {
        nsCOMPtr<nsIMsgIncomingServer> thisServer;
        rv = account->GetIncomingServer(getter_AddRefs(thisServer));
        if (NS_SUCCEEDED(rv)) {
          entry->servers->AppendElement(thisServer);

          // release everything NOW since we're breaking out of the loop
          NS_RELEASE(thisIdentity);
          break;
        }
      }
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
