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
#include "nsMsgAccountManager.h"
#include "nsHashtable.h"
#include "nsMsgBaseCID.h"
#include "nsIPref.h"
#include "prmem.h"

// this should eventually be moved to the pop3 server for upgrading
#include "nsIPop3IncomingServer.h"

static NS_DEFINE_CID(kMsgAccountCID, NS_MSGACCOUNT_CID);
static NS_DEFINE_CID(kMsgIdentityCID, NS_MSGIDENTITY_CID);
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

  NS_IMETHOD addAccount(nsIMsgAccount *account);
  
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

  nsresult upgradePrefs();
  nsIMsgAccount *LoadAccount(char *accountKey);
  
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

  account->SetKey((char *)accountKey);
  return addAccount(account);
}

NS_IMETHODIMP
nsMsgAccountManager::addAccount(nsIMsgAccount *account)
{
#ifdef DEBUG_alecf
    printf("Adding account %p\n", account);
#endif
    char *accountKey=nsnull;
    account->GetKey(&accountKey);
    
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
    printf("No accounts. I'll try to migrate 4.x prefs..\n");
    upgradePrefs();
  }

  char *accountKey;

  /* XXX todo: parse accountList and run loadAccount on each string,
   * probably comma-separated */
  accountKey = PL_strdup(accountList);
  nsIMsgAccount *account = LoadAccount(accountKey);
  if (account) addAccount(account);
  PR_Free(accountKey);

  /* finished loading accounts */
  PR_Free(accountList);

  return NS_OK;
}

nsIMsgAccount *
nsMsgAccountManager::LoadAccount(char *accountKey)
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

  account->SetKey(accountKey);
  
  return account;
}

nsresult
nsMsgAccountManager::upgradePrefs()
{
    nsresult rv;
    nsIMsgAccount *account;
    nsIMsgIdentity *identity;
    nsIMsgIncomingServer *server;

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
    else if (oldMailType == 1)
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
    addAccount(account);

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
    
    rv = m_prefs->GetBoolPref("mail.send_html", &oldbool);
    if (NS_SUCCEEDED(rv)) {
        identity->SetUseHtml(oldbool);
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
    nsIPop3IncomingServer *popServer;
    rv = server->QueryInterface(nsIPop3IncomingServer::GetIID(),
                                (void **)&popServer);
    if (NS_SUCCEEDED(rv)) {

      rv = m_prefs->CopyCharPref("mail.pop_name", &oldstr);
      if (NS_SUCCEEDED(rv)) {
        server->SetUserName(oldstr);
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
        popServer->SetRootFolderPath(oldstr);
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

nsresult
NS_NewMsgAccountManager(const nsIID& iid, void **result)
{
  nsMsgAccountManager* manager;
  if (!result) return NS_ERROR_NULL_POINTER;
  
  manager = new nsMsgAccountManager();
  
  return manager->QueryInterface(iid, result);
}
