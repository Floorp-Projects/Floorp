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

#include "nsIMsgAccountManager.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsMsgAccountManager.h"
#include "nsHashtable.h"
#include "nsMsgBaseCID.h"
#include "nsIPref.h"
#include "prmem.h"

static NS_DEFINE_CID(kMsgAccountCID, NS_MSGACCOUNT_CID);
static NS_DEFINE_CID(kPrefServiceCID, NS_PREF_CID);


struct findEntry {
  nsIMsgAccount *account;
  PRBool found;
  nsHashKey* hashKey;
};


class nsMsgAccountManager : public nsIMsgAccountManager {
public:

  nsMsgAccountManager();
  virtual ~nsMsgAccountManager();
  
  NS_DECL_ISUPPORTS
  
  /* nsIMsgAccount createAccount (in nsIMsgIncomingServer server,
     in nsIMsgIdentity identity); */
  NS_IMETHOD createAccount(nsIMsgIncomingServer *server,
                           nsIMsgIdentity *identity,
                           nsIMsgAccount **_retval) ;
  
  /* nsIMsgAccount addAccount (in nsIMsgIncomingServer server,
     in nsIMsgIdentity identity, in string accountKey); */
  NS_IMETHOD createAccountWithKey(nsIMsgIncomingServer *server,
                                  nsIMsgIdentity *identity, 
                                  const char *accountKey,
                                  nsIMsgAccount **_retval) ;

  NS_IMETHOD addAccount(nsIMsgAccount *account,
                        const char *accountKey);
  
  /* attribute nsIMsgAccount defaultAccount; */
  NS_IMETHOD GetDefaultAccount(nsIMsgAccount * *aDefaultAccount) ;
  NS_IMETHOD SetDefaultAccount(nsIMsgAccount * aDefaultAccount) ;
  
  /* nsIEnumerator getAccounts (); */
  NS_IMETHOD getAccounts(nsIEnumerator **_retval) ;
  
  /* string getAccountKey (in nsIMsgAccount account); */
  NS_IMETHOD getAccountKey(nsIMsgAccount *account, char **_retval) ;

  /* nsIEnumerator getAllIdentities (); */
  NS_IMETHOD getAllIdentities(nsIEnumerator **_retval) ;

  /* nsIEnumerator getAllServers (); */
  NS_IMETHOD getAllServers(nsIEnumerator **_retval) ;

  NS_IMETHOD LoadAccounts();
private:
  nsHashtable *m_accounts;
  nsIMsgAccount *m_defaultAccount;

  nsHashKey *findAccount(nsIMsgAccount *);
  static PRBool hashTableFindAccount(nsHashKey *aKey, void *aData,
                                     void *closure);
  static PRBool hashTableFindFirst(nsHashKey *aKey, void *aData,
                                   void *closure);


  nsIMsgAccount *LoadPreferences(char *accountKey);
  
  nsIPref *m_prefs;
};


NS_IMPL_ISUPPORTS(nsMsgAccountManager, GetIID());

nsMsgAccountManager::nsMsgAccountManager() :
  m_defaultAccount(0),
  m_prefs(0)
{
  NS_INIT_REFCNT();
  m_accounts = new nsHashtable;
}

nsMsgAccountManager::~nsMsgAccountManager()
{
  delete m_accounts;
}

/*
 * generate a relevant, understandable key from the given server and
 * identity, make sure it's unique, then pass to addAccount
 *
 */
NS_IMETHODIMP
nsMsgAccountManager::createAccount(nsIMsgIncomingServer *server,
                                   nsIMsgIdentity *identity,
                                   nsIMsgAccount **_retval)
{
  const char *key = "default";

  return createAccountWithKey(server, identity, key, _retval);
}

/* nsIMsgAccount addAccount (in nsIMsgIncomingServer server, in nsIMsgIdentity
   identity, in string accountKey); */
NS_IMETHODIMP
nsMsgAccountManager::createAccountWithKey(nsIMsgIncomingServer *server,
                                          nsIMsgIdentity *identity, 
                                          const char *accountKey,
                                          nsIMsgAccount **_retval)
{
  nsresult rv;
  nsIMsgAccount *account=nsnull;

  rv = nsComponentManager::CreateInstance(kMsgAccountCID,
                                          nsnull,
                                          nsIMsgAccount::GetIID(),
                                          (void **)&account);

  if (NS_SUCCEEDED(rv)) {
    rv = account->SetIncomingServer(server);
    rv = account->addIdentity(identity);
  }

  return addAccount(account, accountKey);
}

NS_IMETHODIMP
nsMsgAccountManager::addAccount(nsIMsgAccount *account,
                                const char *accountKey)
{
#ifdef DEBUG_alecf
    printf("Adding account %p\n", account);
#endif
    
    nsCStringKey key(accountKey);
    
    if (m_accounts->Exists(&key))
      return NS_ERROR_UNEXPECTED;
        
    m_accounts->Put(&key, account);

    if (m_accounts->Count() == 1)
      m_defaultAccount = account;

    return NS_OK;
}


/* get the default account. If no default account, pick the first account */
NS_IMETHODIMP
nsMsgAccountManager::GetDefaultAccount(nsIMsgAccount * *aDefaultAccount)
{
  if (!aDefaultAccount) return NS_ERROR_NULL_POINTER;

  if (!m_defaultAccount) {
#ifdef DEBUG_alecf
    printf("No default account. Looking for one..\n");
#endif
    
    findEntry entry = { nsnull, PR_FALSE, nsnull };

    m_accounts->Enumerate(hashTableFindFirst, (void *)&entry);

    // are there ANY entries?
    if (!entry.found) return NS_ERROR_UNEXPECTED;

#ifdef DEBUG_alecf
    printf("Account (%p) found\n", entry.account);
#endif
    m_defaultAccount = entry.account;
    
  }
  *aDefaultAccount = m_defaultAccount;
  NS_ADDREF(*aDefaultAccount);
  return NS_OK;
}


NS_IMETHODIMP
nsMsgAccountManager::SetDefaultAccount(nsIMsgAccount * aDefaultAccount)
{
  if (!findAccount(aDefaultAccount)) return NS_ERROR_UNEXPECTED;
  m_defaultAccount = aDefaultAccount;
  return NS_OK;
}

/* map account->key by enumerating all accounts and returning the key
 * when the account is found */
nsHashKey *
nsMsgAccountManager::findAccount(nsIMsgAccount *aAccount)
{
  findEntry entry = {aAccount, PR_FALSE, nsnull };

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
  findEntry *entry = (findEntry *)closure;

  // when found, fix up the findEntry
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
  
  findEntry *entry = (findEntry *)closure;

  entry->hashKey = aKey;
  entry->account = (nsIMsgAccount *)aData;
  entry->found = PR_TRUE;

#ifdef DEBUG_alecf
  printf("hashTableFindFirst: returning first account %p/%p\n",
         aData, entry->account);
#endif
  
  return PR_FALSE;                 // stop enumerating immediately
}



/* nsIEnumerator getAccounts (); */
NS_IMETHODIMP
nsMsgAccountManager::getAccounts(nsIEnumerator **_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* string getAccountKey (in nsIMsgAccount account); */
NS_IMETHODIMP
nsMsgAccountManager::getAccountKey(nsIMsgAccount *account, char **_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


/* nsIEnumerator getAllIdentities (); */
NS_IMETHODIMP
nsMsgAccountManager::getAllIdentities(nsIEnumerator **_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* nsIEnumerator getAllServers (); */
NS_IMETHODIMP
nsMsgAccountManager::getAllServers(nsIEnumerator **_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
nsMsgAccountManager::LoadAccounts()
{
  nsresult rv;

  NS_IF_RELEASE(m_prefs);
  rv = nsServiceManager::GetService(kPrefServiceCID,
                                    nsIPref::GetIID(),
                                    (nsISupports**)&m_prefs);
  if (NS_FAILED(rv)) return rv;
  
  char *accountList;
  rv = m_prefs->CopyCharPref("mail.accountmanager.accounts", &accountList);

  if (NS_FAILED(rv) || !accountList || !accountList[0]) {
    // create default bogus accounts
    printf("No accounts. creating default\n");
    accountList = PL_strdup("default");
  }

  char *accountKey;

  /* XXX todo: parse accountList and run loadAccount on each string,
   * probably comma-separated */
  accountKey = PL_strdup(accountList);
  nsIMsgAccount *account = LoadPreferences(accountKey);
  if (account) addAccount(account, accountKey);
  PR_Free(accountKey);

  /* finished loading accounts */
  PR_Free(accountList);

  return NS_OK;
}

nsIMsgAccount *
nsMsgAccountManager::LoadPreferences(char *accountKey)
{

  printf("Loading preferences for account: %s\n", accountKey);
  
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

  account->LoadPreferences(m_prefs, accountKey);
  
  return account;
}

nsresult
NS_NewMsgAccountManager(const nsIID& iid, void **result)
{
  nsMsgAccountManager* manager;
  if (!result) return NS_ERROR_NULL_POINTER;
  
  manager = new nsMsgAccountManager();
  
  return manager->QueryInterface(iid, result);
}
