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
#include "prenv.h"
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
#include "nsIFileLocator.h" 
#include "nsFileLocations.h" 

// this should eventually be moved to the pop3 server for upgrading
#include "nsIPop3IncomingServer.h"
// this should eventually be moved to the imap server for upgrading
#include "nsIImapIncomingServer.h"
// this should eventually be moved to the nntp server for upgrading
#include "nsINntpIncomingServer.h"
// this should eventually be moved to the no server for upgrading
#include "nsINoIncomingServer.h"

#define BUF_STR_LEN 1024

#if defined(DEBUG_alecf) || defined(DEBUG_sspitzer) || defined(DEBUG_seth)
#define DEBUG_ACCOUNTMANAGER 1
#endif

#ifdef DEBUG_seth
#define HEED_DIR_PREFS 1
#endif 

static NS_DEFINE_CID(kMsgAccountCID, NS_MSGACCOUNT_CID);
static NS_DEFINE_CID(kMsgIdentityCID, NS_MSGIDENTITY_CID);
static NS_DEFINE_CID(kPrefServiceCID, NS_PREF_CID);
static NS_DEFINE_CID(kMsgBiffManagerCID, NS_MSGBIFFMANAGER_CID);
static NS_DEFINE_CID(kProfileCID, NS_PROFILE_CID);
static NS_DEFINE_CID(kCNetSupportDialogCID, NS_NETSUPPORTDIALOG_CID);
static NS_DEFINE_CID(kFileLocatorCID, NS_FILELOCATOR_CID); 

#define IMAP_SCHEMA "imap:/"
#define IMAP_SCHEMA_LENGTH 6
#define MAILBOX_SCHEMA "mailbox:/"
#define MAILBOX_SCHEMA_LENGTH 9

#define POP_4X_MAIL_TYPE 0
#define IMAP_4X_MAIL_TYPE 1
#define MOVEMAIL_4X_MAIL_TYPE 2

#define PREF_MAIL_ACCOUNTMANAGER_ACCOUNTS "mail.accountmanager.accounts"
/* TODO:  do we want to clear these after migration? */
#define PREF_NEWS_DIRECTORY "news.directory"
#define PREF_MAIL_DIRECTORY "mail.directory"
#define PREF_PREMIGRATION_MAIL_DIRECTORY "premigration.mail.directory"
#define PREF_PREMIGRATION_NEWS_DIRECTORY "premigration.news.directory"
#define PREF_IMAP_DIRECTORY "mail.imap.root_dir"

// TODO:  these need to be put into a string bundle
#define LOCAL_MAIL_FAKE_HOST_NAME "Local Mail"
#define LOCAL_MAIL_FAKE_USER_NAME "nobody"
#define NEW_MAIL_DIR_NAME	"Mail"
#define NEW_NEWS_DIR_NAME	"News"
#define NEW_IMAPMAIL_DIR_NAME	"ImapMail"

#define PREF_MAIL_ROOT_NNTP 	"mail.root.nntp"
#define PREF_MAIL_ROOT_POP3	"mail.root.pop3"
#define PREF_MAIL_ROOT_IMAP	"mail.root.imap"
#define PREF_MAIL_ROOT_NONE	"mail.root.none"
#define PREF_MAIL_NEWSRC_ROOT    "mail.newsrc_root"

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
#define PREF_4X_MAIL_USE_IMAP_SENTMAIL "mail.use_imap_sentmail"
#define PREF_4X_NEWS_USE_IMAP_SENTMAIL "news.use_imap_sentmail"
#define PREF_4X_MAIL_IMAP_SENTMAIL_PATH "mail.imap_sentmail_path"
#define PREF_4X_NEWS_IMAP_SENTMAIL_PATH "news.imap_sentmail_path"
#define PREF_4X_MAIL_DEFAULT_CC "mail.default_cc"
#define PREF_4X_NEWS_DEFAULT_CC "news.default_cc"
#define PREF_4X_MAIL_DEFAULT_FCC "mail.default_fcc"
#define PREF_4X_NEWS_DEFAULT_FCC "news.default_fcc"
#define PREF_4X_MAIL_USE_DEFAULT_CC "mail.use_default_cc"
#define PREF_4X_NEWS_USE_DEFAULT_CC "news.use_default_cc"
#define PREF_4X_MAIL_DEFAULT_DRAFTS "mail.default_drafts"
#define PREF_4X_MAIL_DEFAULT_TEMPLATES "mail.default_templates"
#define PREF_4X_MAIL_CC_SELF "mail.cc_self"
#define PREF_4X_NEWS_CC_SELF "news.cc_self"
#define PREF_4X_MAIL_USE_FCC "mail.use_fcc"
#define PREF_4X_NEWS_USE_FCC "news.use_fcc"

#define CONVERT_4X_URI(IDENTITY,MACRO_GETTER,MACRO_SETTER) \
{ \
  nsXPIDLCString macro_oldStr; \
  nsresult macro_rv; \
  macro_rv = IDENTITY->MACRO_GETTER(getter_Copies(macro_oldStr));	\
  if (NS_FAILED(macro_rv)) return macro_rv;	\
  if (!macro_oldStr) { \
    IDENTITY->MACRO_SETTER("");	\
  }\
  else {	\
    char *converted_uri = nsnull; \
    macro_rv = Convert4XUri((const char *)macro_oldStr, &converted_uri); \
    if (NS_FAILED(macro_rv)) { \
      printf("FAIL: %s -> %s\n",(const char *)macro_oldStr,""); \
      IDENTITY->MACRO_SETTER("");	\
    } \
    else { \
      printf("PASS: %s -> %s\n",(const char *)macro_oldStr,converted_uri); \
      IDENTITY->MACRO_SETTER(converted_uri); \
    } \
    PR_FREEIF(converted_uri); \
  }	\
}

#define COPY_IDENTITY_BOOL_VALUE(SRC_ID,DEST_ID,MACRO_GETTER,MACRO_SETTER) 	\
	{	\
		    nsresult macro_rv;	\
        	PRBool macro_oldBool;	\
        	macro_rv = SRC_ID->MACRO_GETTER(&macro_oldBool);	\
        	if (NS_FAILED(macro_rv)) return macro_rv;	\
        	DEST_ID->MACRO_SETTER(macro_oldBool);     \
	}

#define COPY_IDENTITY_STR_VALUE(SRC_ID,DEST_ID,MACRO_GETTER,MACRO_SETTER) 	\
	{	\
        	nsXPIDLCString macro_oldStr;	\
		    nsresult macro_rv;	\
        	macro_rv = SRC_ID->MACRO_GETTER(getter_Copies(macro_oldStr));	\
        	if (NS_FAILED(macro_rv)) return macro_rv;	\
        	if (!macro_oldStr) {	\
                	DEST_ID->MACRO_SETTER("");	\
        	}	\
        	else {	\
                	DEST_ID->MACRO_SETTER(NS_CONST_CAST(char*,(const char*)macro_oldStr));	\
        	}	\
	}

#define MIGRATE_SIMPLE_STR_PREF(PREFNAME,MACRO_OBJECT,MACRO_METHOD) \
  { \
    nsresult macro_rv; \
    char *macro_oldStr = nsnull; \
    macro_rv = m_prefs->CopyCharPref(PREFNAME, &macro_oldStr); \
    if (NS_SUCCEEDED(macro_rv)) { \
      MACRO_OBJECT->MACRO_METHOD(macro_oldStr); \
      PR_FREEIF(macro_oldStr); \
    } \
  }

#define MIGRATE_SIMPLE_INT_PREF(PREFNAME,MACRO_OBJECT,MACRO_METHOD) \
  { \
    nsresult macro_rv; \
    PRInt32 oldInt; \
    macro_rv = m_prefs->GetIntPref(PREFNAME, &oldInt); \
    if (NS_SUCCEEDED(macro_rv)) { \
      MACRO_OBJECT->MACRO_METHOD(oldInt); \
    } \
  }

#define MIGRATE_SIMPLE_BOOL_PREF(PREFNAME,MACRO_OBJECT,MACRO_METHOD) \
  { \
    nsresult macro_rv; \
    PRBool macro_oldBool; \
    macro_rv = m_prefs->GetBoolPref(PREFNAME, &macro_oldBool); \
    if (NS_SUCCEEDED(macro_rv)) { \
      MACRO_OBJECT->MACRO_METHOD(macro_oldBool); \
    } \
  }

#define MIGRATE_STR_PREF(PREFFORMATSTR,PREFFORMATVALUE,INCOMINGSERVERPTR,INCOMINGSERVERMETHOD) \
  { \
    nsresult macro_rv; \
    char prefName[BUF_STR_LEN]; \
    char *macro_oldStr = nsnull; \
    PR_snprintf(prefName, BUF_STR_LEN, PREFFORMATSTR, PREFFORMATVALUE); \
    macro_rv = m_prefs->CopyCharPref(prefName, &macro_oldStr); \
    if (NS_SUCCEEDED(macro_rv)) { \
      INCOMINGSERVERPTR->INCOMINGSERVERMETHOD(macro_oldStr); \
      PR_FREEIF(macro_oldStr); \
    } \
  }

#define MIGRATE_INT_PREF(PREFFORMATSTR,PREFFORMATVALUE,INCOMINGSERVERPTR,INCOMINGSERVERMETHOD) \
  { \
    nsresult macro_rv; \
    PRInt32 oldInt; \
    char prefName[BUF_STR_LEN]; \
    PR_snprintf(prefName, BUF_STR_LEN, PREFFORMATSTR, PREFFORMATVALUE); \
    macro_rv = m_prefs->GetIntPref(prefName, &oldInt); \
    if (NS_SUCCEEDED(macro_rv)) { \
      INCOMINGSERVERPTR->INCOMINGSERVERMETHOD(oldInt); \
    } \
  }

#define MIGRATE_BOOL_PREF(PREFFORMATSTR,PREFFORMATVALUE,INCOMINGSERVERPTR,INCOMINGSERVERMETHOD) \
  { \
    nsresult macro_rv; \
    PRBool macro_oldBool; \
    char prefName[BUF_STR_LEN]; \
    PR_snprintf(prefName, BUF_STR_LEN, PREFFORMATSTR, PREFFORMATVALUE); \
    macro_rv = m_prefs->GetBoolPref(prefName, &macro_oldBool); \
    if (NS_SUCCEEDED(macro_rv)) { \
      INCOMINGSERVERPTR->INCOMINGSERVERMETHOD(macro_oldBool); \
    } \
  }


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

  static PRBool findAccountByServerKey (nsISupports *element, void *aData);

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

  // methods for migration / upgrading
  nsresult MigrateIdentity(nsIMsgIdentity *identity);
  nsresult CopyIdentity(nsIMsgIdentity *srcIdentity, nsIMsgIdentity *destIdentity);
  nsresult SetNewsCcAndFccValues(nsIMsgIdentity *identity);
  nsresult SetMailCcAndFccValues(nsIMsgIdentity *identity);
   
  nsresult MigrateImapAccounts(nsIMsgIdentity *identity);
  nsresult MigrateImapAccount(nsIMsgIdentity *identity, const char *hostname);
  
  nsresult MigrateOldImapPrefs(nsIMsgIncomingServer *server, const char *hostname);
  
  nsresult MigratePopAccounts(nsIMsgIdentity *identity);
  
  nsresult MigrateLocalMailAccounts(nsIMsgIdentity *identity);
  nsresult MigrateOldPopPrefs(nsIMsgIncomingServer *server, const char *hostname);
  
  nsresult MigrateNewsAccounts(nsIMsgIdentity *identity);
  nsresult MigrateNewsAccount(nsIMsgIdentity *identity, const char *hostname, nsFileSpec &newsrcfile, nsFileSpec &newsHostsDir);
  nsresult MigrateOldNntpPrefs(nsIMsgIncomingServer *server, const char *hostname, nsFileSpec &newsrcfile);
  nsresult SetServerRootPref(const char *pref_name, nsFileSpec & dir);

  nsresult ProceedWithMigration(PRInt32 oldMailType);
  
  static char *getUniqueKey(const char* prefix, nsHashtable *hashTable);
  static char *getUniqueAccountKey(const char* prefix,
                                   nsISupportsArray *accounts);

  nsresult Convert4XUri(const char *old_uri, char **new_uri);
  
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
nsMsgAccountManager::getPrefService()
{

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

char *
nsMsgAccountManager::getUniqueKey(const char* prefix, nsHashtable *hashTable)
{
  PRInt32 i=1;
  char key[30];
  PRBool unique=PR_FALSE;

  do {
    PR_snprintf(key, 10, "%s%d",prefix, i++);
    nsStringKey hashKey(key);
    void* hashElement = hashTable->Get(&hashKey);
    
    if (!hashElement) unique=PR_TRUE;
  } while (!unique);

  return nsCRT::strdup(key);
}

char *
nsMsgAccountManager::getUniqueAccountKey(const char *prefix,
                                         nsISupportsArray *accounts)
{
  PRInt32 i=1;
  char key[30];
  PRBool unique = PR_FALSE;
  
  findAccountByKeyEntry findEntry;
  findEntry.key = key;
  findEntry.account = nsnull;
  
  do {
    PR_snprintf(key, 10, "%s%d", prefix, i++);
    
    accounts->EnumerateForwards(findAccountByKey, (void *)&findEntry);

    if (!findEntry.account) unique=PR_TRUE;
    findEntry.account = nsnull;
  } while (!unique);

  return nsCRT::strdup(key);
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

  char *key = getUniqueKey("id", &m_identities);

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
  const char *key = getUniqueKey("server", &m_incomingServers);
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
    do_QueryInterface((nsISupports*)m_incomingServers.Get(&hashKey), &rv);

  if (NS_SUCCEEDED(rv)) {
    *_retval = server;
    NS_ADDREF(*_retval);
    return NS_OK;
  }

  // server doesn't exist, so create it
  
  // in order to create the right kind of server, we have to look
  // at the pref for this server to get the type
  const char *serverTypePref =
    PR_smprintf("mail.server.%s.type", key);

  // serverType is the short server type, like "pop3","imap","nntp","none", etc
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
    PRUint32 count;
    m_accounts->Count(&count);
#ifdef DEBUG_ACCOUNTMANAGER
    printf("There are %d accounts\n", count);
#endif
    if (count == 0) {
      *aDefaultAccount=nsnull;
      return NS_ERROR_FAILURE;
    }

    nsCOMPtr<nsISupports> element;
    rv = m_accounts->GetElementAt(0, getter_AddRefs(element));

    if (NS_SUCCEEDED(rv)) {
      m_defaultAccount = do_QueryInterface(element, &rv);
      NS_ASSERTION(NS_SUCCEEDED(rv), "GetDefaultAccount(): bad array");
    }
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

// enumaration for removing accounts from the BiffManager
PRBool
nsMsgAccountManager::removeServerFromBiff(nsHashKey *aKey, void *aData,
                                          void *closure)
{
    nsresult rv;
    nsCOMPtr<nsIMsgIncomingServer> server =
      do_QueryInterface((nsISupports*)aData, &rv);
    if (NS_FAILED(rv)) return PR_TRUE;
    
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
  m_accounts->EnumerateForwards(getServersToArray,
                                (void *)(nsISupportsArray*)servers);
  *_retval = servers;
  NS_ADDREF(*_retval);
  return rv;
}

PRBool
nsMsgAccountManager::getServersToArray(nsISupports *element, void *aData)
{
  nsresult rv;
  nsCOMPtr<nsIMsgAccount> account = do_QueryInterface(element, &rv);
  if (NS_FAILED(rv)) return rv;
  
  nsISupportsArray *array = (nsISupportsArray*)aData;
  
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
    rv = UpgradePrefs();
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
  nsCOMPtr<nsIMsgAccount> account = do_QueryInterface(element, &rv);
  if (NS_FAILED(rv)) return PR_TRUE;
  
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

    const char *key=getUniqueAccountKey("account", m_accounts);

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

nsresult
nsMsgAccountManager::ProceedWithMigration(PRInt32 oldMailType)
{
  char *prefvalue = nsnull;
  nsresult rv = NS_OK;
  
  if (oldMailType == POP_4X_MAIL_TYPE) {
    // if they were using pop, "mail.pop_name" must have been set
    // otherwise, they don't really have anything to migrate
    rv = m_prefs->CopyCharPref(PREF_4X_MAIL_POP_NAME, &prefvalue);
    if (NS_SUCCEEDED(rv)) {
	    if (!prefvalue || (PL_strlen(prefvalue) == 0)) {
	      rv = NS_ERROR_FAILURE;
	    }
    }
  }
  else if (oldMailType == IMAP_4X_MAIL_TYPE) {
    // if they were using imap, "network.hosts.imap_servers" must have been set
    // otherwise, they don't really have anything to migrate
    rv = m_prefs->CopyCharPref(PREF_4X_NETWORK_HOSTS_IMAP_SERVER, &prefvalue);
    if (NS_SUCCEEDED(rv)) {
	    if (!prefvalue || (PL_strlen(prefvalue) == 0)) {
	      rv = NS_ERROR_FAILURE;
	    }
    }
  }
  else if (oldMailType == MOVEMAIL_4X_MAIL_TYPE) {
    printf("sorry, migration of movemail not supported yet.\n");
    NS_ASSERTION(0, "movemail migration not supported yet.");
    rv = NS_ERROR_UNEXPECTED;
  }
  else {
#ifdef DEBUG_ACCOUNTMANAGER
    printf("Unrecognized server type %d\n", oldMailType);
#endif
    rv = NS_ERROR_UNEXPECTED;
  }

  PR_FREEIF(prefvalue);
  return rv;
}

NS_IMETHODIMP
nsMsgAccountManager::UpgradePrefs()
{
    nsresult rv;
    PRInt32 oldMailType;

    rv = getPrefService();
    if (NS_FAILED(rv)) return rv;

    rv = m_prefs->GetIntPref(PREF_4X_MAIL_SERVER_TYPE, &oldMailType);
    if (NS_FAILED(rv)) {
#ifdef DEBUG_ACCOUNTMANAGER
        printf("Tried to upgrade old prefs, but couldn't find server type!\n");
#endif
        return rv;
    }

    // because mail.server_type defaults to 0 (pop) it will look the user
    // has something to migrate, even with an empty prefs.js file
    // ProceedWithMigration will check if there is something to migrate
    // if not, NS_FAILED(rv) will be true, and we'll return.
    // this plays nicely with msgMail3PaneWindow.js, which will launch the
    // Account Manager if UpgradePrefs() fails.
    rv = ProceedWithMigration(oldMailType);
    if (NS_FAILED(rv)) {
#ifdef DEBUG_ACCOUNTMANAGER
	printf("FAIL:  don't proceed with migration.\n");
#endif
	// because we are failing to migrate, the mail.root.* prefs
	// will not be set.  we set them here.
	NS_WITH_SERVICE(nsIFileLocator, locator, kFileLocatorCID, &rv);
        if (NS_FAILED(rv) || !locator) return NS_ERROR_FAILURE;

        nsCOMPtr <nsIFileSpec> spec;
        rv = locator->GetFileLocation(nsSpecialFileSpec::App_UserProfileDirectory50, getter_AddRefs(spec));
        if (NS_FAILED(rv) || !spec) return NS_ERROR_FAILURE; 

	// both "none" and "pop3" are rooted at <profile>/Mail
	rv = spec->AppendRelativeUnixPath(NEW_MAIL_DIR_NAME);
	if (NS_FAILED(rv)) return rv;
        NS_ASSERTION(m_prefs,"m_prefs is null");
  	if (!m_prefs) return NS_ERROR_FAILURE;

  	rv = m_prefs->SetFilePref(PREF_MAIL_ROOT_NONE, spec, PR_FALSE /* set default */);
	if (NS_FAILED(rv)) return rv;
  	rv = m_prefs->SetFilePref(PREF_MAIL_ROOT_POP3, spec, PR_FALSE /* set default */);
	if (NS_FAILED(rv)) return rv;
 
	// "nntp" is rooted at <profile>/News
	rv = spec->SetLeafName(NEW_NEWS_DIR_NAME);
	if (NS_FAILED(rv)) return rv;
  	rv = m_prefs->SetFilePref(PREF_MAIL_ROOT_NNTP, spec, PR_FALSE /* set default */);
	if (NS_FAILED(rv)) return rv;

	// root the newsrc files to <profile>/News, except on UNIX
	// on UNIX, set it to ~.
	// this may change
#ifdef XP_UNIX
	char *unixHomeDirectory = PR_GetEnv("HOME");
	rv = m_prefs->SetCharPref(PREF_MAIL_NEWSRC_ROOT, unixHomeDirectory);
        if (NS_FAILED(rv)) return rv;
#else
	rv = m_prefs->SetFilePref(PREF_MAIL_NEWSRC_ROOT, spec, PR_FALSE /* set default */);
        if (NS_FAILED(rv)) return rv;
#endif	/* XP_UNIX */

	// "imap" is rooted at <profile>/ImapMail
	rv = spec->SetLeafName(NEW_IMAPMAIL_DIR_NAME);
	if (NS_FAILED(rv)) return rv;
  	rv = m_prefs->SetFilePref(PREF_MAIL_ROOT_IMAP, spec, PR_FALSE /* set default */);
	if (NS_FAILED(rv)) return rv;
	
	// return NS_ERROR_FAILURE because we failed to upgrade the pref.js
	// by returning a failure code, this will case the Account Wizard
	// to be automatically opened.
	return NS_ERROR_FAILURE;
    }
#ifdef DEBUG_ACCOUNTMANAGER
    else {
        printf("PASS:  proceed with migration.\n");
    }
#endif 

    // create a dummy identity, for migration only
    nsCOMPtr<nsIMsgIdentity> identity;
    rv = createKeyedIdentity("migration", getter_AddRefs(identity));
    if (NS_FAILED(rv)) return rv;

    rv = MigrateIdentity(identity);
    if (NS_FAILED(rv)) return rv;    
    
    if ( oldMailType == POP_4X_MAIL_TYPE) {  
      rv = MigratePopAccounts(identity);
      if (NS_FAILED(rv)) return rv;
	}
    else if (oldMailType == IMAP_4X_MAIL_TYPE) {
      rv = MigrateImapAccounts(identity);
      if (NS_FAILED(rv)) return rv;
      
      // if they had IMAP, they also had "Local Mail"
      // we need to migrate that, too.  
      rv = MigrateLocalMailAccounts(identity);
      if (NS_FAILED(rv)) return rv;
	}
    else if (oldMailType == MOVEMAIL_4X_MAIL_TYPE) {
      printf("sorry, migration of movemail not supported yet.\n");
      NS_ASSERTION(0, "movemail migration not supported yet.");
      return NS_ERROR_UNEXPECTED;
    }
    else {
#ifdef DEBUG_ACCOUNTMANAGER
      printf("Unrecognized server type %d\n", oldMailType);
#endif
      return NS_ERROR_UNEXPECTED;
    }

	rv = MigrateNewsAccounts(identity);
    if (NS_FAILED(rv)) return rv;

    // remove dummy migration identity
    return NS_OK;
}

nsresult
nsMsgAccountManager::MigrateIdentity(nsIMsgIdentity *identity)
{
  MIGRATE_SIMPLE_STR_PREF(PREF_4X_MAIL_IDENTITY_USEREMAIL,identity,SetEmail)
  MIGRATE_SIMPLE_STR_PREF(PREF_4X_MAIL_IDENTITY_USERNAME,identity,SetFullName)
  MIGRATE_SIMPLE_STR_PREF(PREF_4X_MAIL_IDENTITY_REPLY_TO,identity,SetReplyTo)
  MIGRATE_SIMPLE_STR_PREF(PREF_4X_MAIL_IDENTITY_ORGANIZATION,identity,SetOrganization)
  MIGRATE_SIMPLE_BOOL_PREF(PREF_4X_MAIL_COMPOSE_HTML,identity,SetComposeHtml)
  MIGRATE_SIMPLE_STR_PREF(PREF_4X_NETWORK_HOSTS_SMTP_SERVER,identity,SetSmtpHostname)
  MIGRATE_SIMPLE_STR_PREF(PREF_4X_MAIL_SMTP_NAME,identity,SetSmtpUsername)
    
  MIGRATE_SIMPLE_STR_PREF(PREF_4X_MAIL_DEFAULT_DRAFTS,identity,SetDraftFolder)
  CONVERT_4X_URI(identity,GetDraftFolder,SetDraftFolder)
    
  MIGRATE_SIMPLE_STR_PREF(PREF_4X_MAIL_DEFAULT_TEMPLATES,identity,SetStationaryFolder)
  CONVERT_4X_URI(identity,GetStationaryFolder,SetStationaryFolder)
    
  // what about the new 5.0 spam folder pref?
    
  return NS_OK;
}

nsresult
nsMsgAccountManager::SetNewsCcAndFccValues(nsIMsgIdentity *identity)
{
  nsresult rv;
  
  MIGRATE_SIMPLE_BOOL_PREF(PREF_4X_NEWS_CC_SELF,identity,SetBccSelf)
  MIGRATE_SIMPLE_BOOL_PREF(PREF_4X_NEWS_USE_DEFAULT_CC,identity,SetBccOthers)
  MIGRATE_SIMPLE_STR_PREF(PREF_4X_NEWS_DEFAULT_CC,identity,SetBccList)
  MIGRATE_SIMPLE_BOOL_PREF(PREF_4X_NEWS_USE_FCC,identity,SetDoFcc)
      
  PRBool news_used_uri_for_sent_in_4x;
  rv = m_prefs->GetBoolPref(PREF_4X_NEWS_USE_IMAP_SENTMAIL, &news_used_uri_for_sent_in_4x);
  if (news_used_uri_for_sent_in_4x) {
    MIGRATE_SIMPLE_STR_PREF(PREF_4X_NEWS_IMAP_SENTMAIL_PATH,identity,SetFccFolder)
  }
  else {
    MIGRATE_SIMPLE_STR_PREF(PREF_4X_NEWS_DEFAULT_FCC,identity,SetFccFolder)
  }
  CONVERT_4X_URI(identity,GetFccFolder,SetFccFolder)

  return NS_OK;
}

nsresult
nsMsgAccountManager::SetMailCcAndFccValues(nsIMsgIdentity *identity)
{
  nsresult rv;
  
  MIGRATE_SIMPLE_BOOL_PREF(PREF_4X_MAIL_CC_SELF,identity,SetBccSelf)
  MIGRATE_SIMPLE_BOOL_PREF(PREF_4X_MAIL_USE_DEFAULT_CC,identity,SetBccOthers)
  MIGRATE_SIMPLE_STR_PREF(PREF_4X_MAIL_DEFAULT_CC,identity,SetBccList)
  MIGRATE_SIMPLE_BOOL_PREF(PREF_4X_MAIL_USE_FCC,identity,SetDoFcc)
    
  PRBool imap_used_uri_for_sent_in_4x;
  rv = m_prefs->GetBoolPref(PREF_4X_MAIL_USE_IMAP_SENTMAIL, &imap_used_uri_for_sent_in_4x);
  if (imap_used_uri_for_sent_in_4x) {
    MIGRATE_SIMPLE_STR_PREF(PREF_4X_MAIL_IMAP_SENTMAIL_PATH,identity,SetFccFolder)
  }
  else {
    MIGRATE_SIMPLE_STR_PREF(PREF_4X_MAIL_DEFAULT_FCC,identity,SetFccFolder)
  }
  CONVERT_4X_URI(identity,GetFccFolder,SetFccFolder)
    
  return NS_OK;
}

// caller will free the memory
nsresult
nsMsgAccountManager::Convert4XUri(const char *old_uri, char **new_uri)
{
  nsresult rv;
  *new_uri = nsnull;

  if (!old_uri) {
    return NS_ERROR_NULL_POINTER;
  }
 
  // if the old_uri is "", don't attempt any conversion
  if (PL_strlen(old_uri) == 0) {
	*new_uri = PR_smprintf("");
        return NS_OK;
  }

#ifdef DEBUG_ACCOUNTMANAGER
  printf("old 4.x folder uri = >%s<\n", old_uri);
#endif

  if (PL_strncasecmp(IMAP_SCHEMA,old_uri,IMAP_SCHEMA_LENGTH) == 0) {
    // 4.x IMAP uri's began with "IMAP:/".  we need that to be "imap:/"
#ifdef DEBUG_ACCOUNTMANAGER
    printf("new_uri = %s%s\n",IMAP_SCHEMA,old_uri + IMAP_SCHEMA_LENGTH);
#endif
    *new_uri = PR_smprintf("%s%s",IMAP_SCHEMA,old_uri + IMAP_SCHEMA_LENGTH);
    return NS_OK;
  }

  char *usernameAtHostname = nsnull;
  char *mail_directory_value = nsnull;
  rv = m_prefs->CopyCharPref(PREF_PREMIGRATION_MAIL_DIRECTORY, &mail_directory_value);
  if (NS_FAILED(rv) || !mail_directory_value || (PL_strlen(mail_directory_value) == 0)) {
#ifdef DEBUG_ACCOUNTMANAGER
    printf("%s was not set, attempting to use %s instead.\n",PREF_PREMIGRATION_MAIL_DIRECTORY,PREF_MAIL_DIRECTORY);
#endif
    PR_FREEIF(mail_directory_value);

    rv = m_prefs->CopyCharPref(PREF_MAIL_DIRECTORY, &mail_directory_value);
    if (NS_FAILED(rv)) return rv;

    if (!mail_directory_value || (PL_strlen(mail_directory_value) == 0)) {
      NS_ASSERTION(0,"failed to get a base value for the mail.directory");
      return NS_ERROR_UNEXPECTED;
    }
  }

  PRInt32 oldMailType;
  rv = m_prefs->GetIntPref(PREF_4X_MAIL_SERVER_TYPE, &oldMailType);
  if (NS_FAILED(rv)) return rv;

  if (oldMailType == POP_4X_MAIL_TYPE) {
    char *pop_username = nsnull;
    char *pop_hostname = nsnull;

    rv = m_prefs->CopyCharPref(PREF_4X_MAIL_POP_NAME, &pop_username);
    if (NS_FAILED(rv)) return rv;

    rv = m_prefs->CopyCharPref(PREF_4X_NETWORK_HOSTS_POP_SERVER, &pop_hostname);
    if (NS_FAILED(rv)) return rv;

    usernameAtHostname = PR_smprintf("%s@%s",pop_username, pop_hostname);

    PR_FREEIF(pop_username);
    PR_FREEIF(pop_hostname);
  }
  else if (oldMailType == IMAP_4X_MAIL_TYPE) {
    usernameAtHostname = PR_smprintf("%s@%s",LOCAL_MAIL_FAKE_USER_NAME,LOCAL_MAIL_FAKE_HOST_NAME);
  }
  else if (oldMailType == MOVEMAIL_4X_MAIL_TYPE) {
    printf("sorry, movemail migration not supported yet.\n");
    NS_ASSERTION(0, "movemail migration not supported yet.");
    return NS_ERROR_FAILURE;
  }
  else {
#ifdef DEBUG_ACCOUNTMANAGER
    printf("Unrecognized server type %d\n", oldMailType);
#endif
    return NS_ERROR_UNEXPECTED;
  }

  if (PL_strncasecmp(MAILBOX_SCHEMA,old_uri,MAILBOX_SCHEMA_LENGTH) == 0) {
#ifdef DEBUG_ACCOUNTMANAGER
    printf("turn %s into %s/%s/(%s - %s)\n",old_uri,MAILBOX_SCHEMA,usernameAtHostname,old_uri + MAILBOX_SCHEMA_LENGTH,mail_directory_value);
#endif
    // the extra -1 is because in 4.x, we had this:
    // mailbox:<PATH> instead of mailbox:/<PATH>
    *new_uri = PR_smprintf("%s/%s/%s",MAILBOX_SCHEMA,usernameAtHostname,old_uri + MAILBOX_SCHEMA_LENGTH + PL_strlen(mail_directory_value) -1);
  }
  else {
#ifdef DEBUG_ACCOUNTMANAGER
    printf("turn %s into %s/%s/(%s - %s)\n",old_uri,MAILBOX_SCHEMA,usernameAtHostname,old_uri,mail_directory_value);
#endif
    *new_uri = PR_smprintf("%s/%s/%s",MAILBOX_SCHEMA,usernameAtHostname,old_uri + PL_strlen(mail_directory_value));
  }

  if (!*new_uri) {
#ifdef DEBUG_ACCOUNTMANAGER
    printf("failed to convert 4.x uri: %s\n", old_uri);
#endif
    NS_ASSERTION(0,"uri conversion code not complete");
    return NS_ERROR_FAILURE;
  }

  PR_FREEIF(usernameAtHostname);
  PR_FREEIF(mail_directory_value);

  return NS_OK;
}

nsresult
nsMsgAccountManager::MigrateLocalMailAccounts(nsIMsgIdentity *identity) 
{
  nsresult rv;
  
  // create the account
  nsCOMPtr<nsIMsgAccount> account;
  rv = CreateAccount(getter_AddRefs(account));
  if (NS_FAILED(rv)) return rv;

  // create the server
  nsCOMPtr<nsIMsgIncomingServer> server;
  rv = CreateIncomingServer("none", getter_AddRefs(server));
  if (NS_FAILED(rv)) return rv;

  // create the identity
  nsCOMPtr<nsIMsgIdentity> copied_identity;
  rv = CreateIdentity(getter_AddRefs(copied_identity));
  if (NS_FAILED(rv)) return rv;

  // make this new identity to copy of the identity
  // that we created out of the 4.x prefs
  rv = CopyIdentity(identity,copied_identity);
  if (NS_FAILED(rv)) return rv;

  rv = SetMailCcAndFccValues(copied_identity);
  if (NS_FAILED(rv)) return rv;
    
  // the server needs a username
  server->SetUsername(LOCAL_MAIL_FAKE_USER_NAME);

  // hook them together
  account->SetIncomingServer(server);
  account->AddIdentity(copied_identity);
  
  // now upgrade all the prefs
  // some of this ought to be moved out into the NONE implementation
  nsCOMPtr<nsINoIncomingServer> noServer;
  noServer = do_QueryInterface(server, &rv);
  if (NS_FAILED(rv)) return rv;

  // "none" is the type we use for migrate Local Mail
  server->SetType("none");
  server->SetHostName(LOCAL_MAIL_FAKE_HOST_NAME);
    
  // create the directory structure for old 4.x "Local Mail"
  // under <profile dir>/Mail/Local Mail or
  // <"mail.directory" pref>/Local Mail
  nsCOMPtr <nsIFileSpec> mailDir;
  nsFileSpec dir;
  PRBool dirExists;

  char *mail_directory_value = nsnull;
#ifdef HEED_DIR_PREFS
  rv = m_prefs->CopyCharPref(PREF_MAIL_DIRECTORY, &mail_directory_value);
#else
  rv = NS_ERROR_FAILURE;
#endif /* HEED_DIR_PREFS */
  // if the "mail.directory" pref is set, use that.
  if (NS_SUCCEEDED(rv) && mail_directory_value && (PL_strlen(mail_directory_value) > 0)) {
    dir = mail_directory_value;
    PR_FREEIF(mail_directory_value);
  }
  else {    
    nsFileSpec profileDir;
    
    NS_WITH_SERVICE(nsIProfile, profile, kProfileCID, &rv);
    if (NS_FAILED(rv)) return rv;
    
    rv = profile->GetCurrentProfileDir(&profileDir);
    if (NS_FAILED(rv)) return rv;
    
    dir = profileDir;

    // we want <profile>/Mail, not <profile>
    dir += NEW_MAIL_DIR_NAME;

    // create <profile>/Mail if it doesn't exist
    if (!dir.Exists()) {
      dir.CreateDir();
    }
  }

  rv = SetServerRootPref(PREF_MAIL_ROOT_NONE, dir); 
  if (NS_FAILED(rv)) return rv;

  // at this point, dir == the directory that will contain the "Local Mail" folder.
  // "Local Mail" in 4.x was under Users/sspitzer/Mail
  // in 5.0, it's under Users50/sspitzer/Mail/Local Mail.
  dir += LOCAL_MAIL_FAKE_HOST_NAME;
  
  rv = NS_NewFileSpecWithSpec(dir, getter_AddRefs(mailDir));
  if (NS_FAILED(rv)) return rv;
  
  rv = mailDir->Exists(&dirExists);
  if (NS_FAILED(rv)) return rv;
  
  if (!dirExists) {
    mailDir->CreateDir();
  }
  
  server->SetLocalPath(mailDir);
  
  rv = mailDir->Exists(&dirExists);
  if (NS_FAILED(rv)) return rv;
  
  if (!dirExists) {
    mailDir->CreateDir();
  }
  
  return NS_OK;
}

nsresult
nsMsgAccountManager::MigratePopAccounts(nsIMsgIdentity *identity)
{
  nsresult rv;
  
  nsCOMPtr<nsIMsgAccount> account;
  nsCOMPtr<nsIMsgIncomingServer> server;

  rv = CreateAccount(getter_AddRefs(account));
  if (NS_FAILED(rv)) return rv;

  rv = CreateIncomingServer("pop3", getter_AddRefs(server));
  if (NS_FAILED(rv)) return rv;

  // create the identity
  nsCOMPtr<nsIMsgIdentity> copied_identity;
  rv = CreateIdentity(getter_AddRefs(copied_identity));
  if (NS_FAILED(rv)) return rv;

  // make this new identity to copy of the identity
  // that we created out of the 4.x prefs
  rv = CopyIdentity(identity,copied_identity);
  if (NS_FAILED(rv)) return rv;

  rv = SetMailCcAndFccValues(copied_identity);
  if (NS_FAILED(rv)) return rv;

  // hook them together
  account->SetIncomingServer(server);
  account->AddIdentity(copied_identity);
  
  // now upgrade all the prefs
  nsCOMPtr <nsIFileSpec> mailDir;
  nsFileSpec dir;
  PRBool dirExists;

  server->SetType("pop3");
  char *hostname=nsnull;
  rv = m_prefs->CopyCharPref(PREF_4X_NETWORK_HOSTS_POP_SERVER, &hostname);
  if (NS_SUCCEEDED(rv)) {
    server->SetHostName(hostname);
  }

  rv = MigrateOldPopPrefs(server, hostname);
  if (NS_FAILED(rv)) return rv;

  char *mail_directory_value = nsnull;
#ifdef HEED_DIR_PREFS
  rv = m_prefs->CopyCharPref(PREF_MAIL_DIRECTORY, &mail_directory_value);
#else
  rv = NS_ERROR_FAILURE;
#endif /* HEED_DIR_PREFS */
  // create the directory structure for old 4.x pop mail
  // under <profile dir>/Mail/<pop host name> or
  // <"mail.directory" pref>/<pop host name>
  //
  // if the "mail.directory" pref is set, use that.
  if (NS_SUCCEEDED(rv) && mail_directory_value && (PL_strlen(mail_directory_value) > 0)) {
    dir = mail_directory_value;
    PR_FREEIF(mail_directory_value);
  }
  else {
    nsFileSpec profileDir;
    
    NS_WITH_SERVICE(nsIProfile, profile, kProfileCID, &rv);
    if (NS_FAILED(rv)) return rv;
    
    rv = profile->GetCurrentProfileDir(&profileDir);
    if (NS_FAILED(rv)) return rv;

    dir = profileDir;
    
    // we wan't <profile>/Mail, not <profile>
    dir += NEW_MAIL_DIR_NAME;

    // create <profile>/Mail if it doesn't exist
    if (!dir.Exists()) {
      dir.CreateDir();
    }
  }
  rv = SetServerRootPref(PREF_MAIL_ROOT_POP3, dir); 
  if (NS_FAILED(rv)) return rv;
  
  // we want .../Mail/<hostname>, not .../Mail
  dir += hostname;
  PR_FREEIF(hostname);
  
  rv = NS_NewFileSpecWithSpec(dir, getter_AddRefs(mailDir));
  if (NS_FAILED(rv)) return rv;
  
  rv = mailDir->Exists(&dirExists);
  if (NS_FAILED(rv)) return rv;
  
  if (!dirExists) {
    mailDir->CreateDir();
  }

  server->SetLocalPath(mailDir);
  
  rv = mailDir->Exists(&dirExists);
  if (NS_FAILED(rv)) return rv;
  
  if (!dirExists) {
    mailDir->CreateDir();
  }

  return NS_OK;
}

nsresult
nsMsgAccountManager::MigrateOldPopPrefs(nsIMsgIncomingServer * server, const char *hostname)
{
  nsresult rv;
    
  nsCOMPtr<nsIPop3IncomingServer> popServer;
  popServer = do_QueryInterface(server, &rv);
  if (NS_FAILED(rv)) return rv;
 
  MIGRATE_SIMPLE_STR_PREF(PREF_4X_MAIL_POP_NAME,server,SetUsername)
  MIGRATE_SIMPLE_BOOL_PREF(PREF_4X_MAIL_REMEMBER_PASSWORD,server,SetRememberPassword)
#ifdef CAN_UPGRADE_4x_PASSWORDS
  MIGRATE_SIMPLE_STR_PREF(PREF_4X_MAIL_POP_PASSWORD,server,SetPassword)
#endif /* CAN_UPGRADE_4x_PASSWORDS */
  MIGRATE_SIMPLE_BOOL_PREF(PREF_4X_MAIL_CHECK_NEW_MAIL,server,SetDoBiff)
  MIGRATE_SIMPLE_INT_PREF(PREF_4X_MAIL_CHECK_TIME,server,SetBiffMinutes)
  MIGRATE_SIMPLE_BOOL_PREF(PREF_4X_MAIL_LEAVE_ON_SERVER,popServer,SetLeaveMessagesOnServer)
  MIGRATE_SIMPLE_BOOL_PREF(PREF_4X_MAIL_DELETE_MAIL_LEFT_ON_SERVER,popServer,SetDeleteMailLeftOnServer) 
  
  return NS_OK;
}

nsresult
nsMsgAccountManager::MigrateImapAccounts(nsIMsgIdentity *identity)
{
  nsresult rv;
  char *hostList=nsnull;
  rv = getPrefService();
  if (NS_FAILED(rv)) return rv;

  rv = m_prefs->CopyCharPref(PREF_4X_NETWORK_HOSTS_IMAP_SERVER, &hostList);
  if (NS_FAILED(rv)) return rv;
  
  if (!hostList || !*hostList) return NS_OK;  // NS_ERROR_FAILURE?
  
  char *token = nsnull;
  char *rest = NS_CONST_CAST(char*,(const char*)hostList);
  nsCAutoString str;
      
  token = nsCRT::strtok(rest, ",", &rest);
  while (token && *token) {
    str = token;
    str.StripWhitespace();
    
    if (!str.IsEmpty()) {
      // str is the hostname
      rv = MigrateImapAccount(identity,str);
      if  (NS_FAILED(rv)) {
        // failed to migrate.  bail.
        return rv;
      }
      str = "";
    }
    token = nsCRT::strtok(rest, ",", &rest);
  }
  PR_FREEIF(hostList);
  return NS_OK;
}

nsresult
nsMsgAccountManager::CopyIdentity(nsIMsgIdentity *srcIdentity, nsIMsgIdentity *destIdentity)
{
	COPY_IDENTITY_BOOL_VALUE(srcIdentity,destIdentity,GetComposeHtml,SetComposeHtml)
        COPY_IDENTITY_STR_VALUE(srcIdentity,destIdentity,GetEmail,SetEmail)
        COPY_IDENTITY_STR_VALUE(srcIdentity,destIdentity,GetReplyTo,SetReplyTo)
        COPY_IDENTITY_STR_VALUE(srcIdentity,destIdentity,GetFullName,SetFullName)
        COPY_IDENTITY_STR_VALUE(srcIdentity,destIdentity,GetOrganization,SetOrganization)
        COPY_IDENTITY_STR_VALUE(srcIdentity,destIdentity,GetSmtpHostname,SetSmtpHostname)
        COPY_IDENTITY_STR_VALUE(srcIdentity,destIdentity,GetSmtpUsername,SetSmtpUsername)
        COPY_IDENTITY_STR_VALUE(srcIdentity,destIdentity,GetDraftFolder,SetDraftFolder)
        COPY_IDENTITY_STR_VALUE(srcIdentity,destIdentity,GetStationaryFolder,SetStationaryFolder)

	return NS_OK;
}

nsresult
nsMsgAccountManager::MigrateImapAccount(nsIMsgIdentity *identity, const char *hostname)
{
  nsresult rv;

  if (!hostname) return NS_ERROR_NULL_POINTER;
  
  // create the account
  nsCOMPtr<nsIMsgAccount> account;
  rv = CreateAccount(getter_AddRefs(account));
  if (NS_FAILED(rv)) return rv;

  // create the server
  nsCOMPtr<nsIMsgIncomingServer> server;
  rv = CreateIncomingServer("imap", getter_AddRefs(server));
  if (NS_FAILED(rv)) return rv;

  // create the identity
  nsCOMPtr<nsIMsgIdentity> copied_identity;
  rv = CreateIdentity(getter_AddRefs(copied_identity));
  if (NS_FAILED(rv)) return rv;

  // make this new identity to copy of the identity
  // that we created out of the 4.x prefs
  rv = CopyIdentity(identity,copied_identity);
  if (NS_FAILED(rv)) return rv;
  
  rv = SetMailCcAndFccValues(copied_identity);
  if (NS_FAILED(rv)) return rv;  
  
  // hook them together
  account->SetIncomingServer(server);
  account->AddIdentity(copied_identity);

  // now upgrade all the prefs
  nsFileSpec profileDir;
  
  NS_WITH_SERVICE(nsIProfile, profile, kProfileCID, &rv);
  if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
  
  rv = profile->GetCurrentProfileDir(&profileDir);
  if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
  
  server->SetType("imap");
  server->SetHostName((char *)hostname);
  
  rv = MigrateOldImapPrefs(server, hostname);
  if (NS_FAILED(rv)) return rv;
  
  nsCOMPtr <nsIFileSpec> imapMailDir;
  nsFileSpec dir;
  PRBool dirExists;

  char *imap_directory_value = nsnull;
#ifdef HEED_DIR_PREFS
  rv = m_prefs->CopyCharPref(PREF_IMAP_DIRECTORY, &imap_directory_value);
#else
  rv = NS_ERROR_FAILURE;
#endif /* HEED_DIR_PREFS */
  // if the "mail.imap.root_dir" pref is set, use that.
  if (NS_SUCCEEDED(rv) && imap_directory_value && (PL_strlen(imap_directory_value) > 0)) {
    dir = imap_directory_value;
    PR_FREEIF(imap_directory_value);
  }
  else {
    dir = profileDir;
  
    // we want <profile>/ImapMail, not <profile>
    dir += NEW_IMAPMAIL_DIR_NAME;
    if (!dir.Exists()) {
      dir.CreateDir();
    }
  }

  rv = SetServerRootPref(PREF_MAIL_ROOT_IMAP, dir); 
  if (NS_FAILED(rv)) return rv;
  
  // we want .../ImapMail/<hostname>, not .../ImapMail
  dir += hostname;
  
  rv = NS_NewFileSpecWithSpec(dir, getter_AddRefs(imapMailDir));
  if (NS_FAILED(rv)) return rv;

  server->SetLocalPath(imapMailDir);
  
  rv = imapMailDir->Exists(&dirExists);
  if (NS_FAILED(rv)) return rv;
  
  if (!dirExists) {
    imapMailDir->CreateDir();
  }
  
  return NS_OK;
}

nsresult
nsMsgAccountManager::MigrateOldImapPrefs(nsIMsgIncomingServer *server, const char *hostname)
{
  nsresult rv;

  // some of this ought to be moved out into the IMAP implementation
  nsCOMPtr<nsIImapIncomingServer> imapServer;
  imapServer = do_QueryInterface(server, &rv);
  if (NS_FAILED(rv)) return rv;

  // upgrade the msg incoming server prefs
  MIGRATE_STR_PREF("mail.imap.server.%s.userName",hostname,server,SetUsername)
  MIGRATE_BOOL_PREF("mail.imap.server.%s.remember_password",hostname,server,SetRememberPassword)
#ifdef CAN_UPGRADE_4x_PASSWORDS
  MIGRATE_STR_PREF("mail.imap.server.%s.password",hostname,server,SetPassword)
#endif /* CAN_UPGRADE_4x_PASSWORDS */
  // upgrade the imap incoming server specific prefs
  MIGRATE_BOOL_PREF("mail.imap.server.%s.check_new_mail",hostname,server,SetDoBiff)
  MIGRATE_INT_PREF("mail.imap.server.%s.check_time",hostname,server,SetBiffMinutes)
  MIGRATE_STR_PREF("mail.imap.server.%s.admin_url",hostname,imapServer,SetAdminUrl)
  MIGRATE_INT_PREF("mail.imap.server.%s.capability",hostname,imapServer,SetCapabilityPref)
  MIGRATE_BOOL_PREF("mail.imap.server.%s.cleanup_inbox_on_exit",hostname,imapServer,SetCleanupInboxOnExit)
  MIGRATE_INT_PREF("mail.imap.server.%s.delete_model",hostname,imapServer,SetDeleteModel)
  MIGRATE_BOOL_PREF("mail.imap.server.%s.dual_use_folders",hostname,imapServer,SetDualUseFolders)
  MIGRATE_BOOL_PREF("mail.imap.server.%s.empty_trash_on_exit",hostname,imapServer,SetEmptyTrashOnExit)
  MIGRATE_INT_PREF("mail.imap.server.%s.empty_trash_threshhold",hostname,imapServer,SetEmptyTrashThreshhold)
  MIGRATE_STR_PREF("mail.imap.server.%s.namespace.other_users",hostname,imapServer,SetOtherUsersNamespace)
  MIGRATE_STR_PREF("mail.imap.server.%s.namespace.personal",hostname,imapServer,SetPersonalNamespace)
  MIGRATE_STR_PREF("mail.imap.server.%s.namespace.public",hostname,imapServer,SetPublicNamespace)
  MIGRATE_BOOL_PREF("mail.imap.server.%s.offline_download",hostname,imapServer,SetOfflineDownload)
  MIGRATE_BOOL_PREF("mail.imap.server.%s.override_namespaces",hostname,imapServer,SetOverrideNamespaces)
  MIGRATE_BOOL_PREF("mail.imap.server.%s.using_subscription",hostname,imapServer,SetUsingSubscription)

  return NS_OK;
}
  
#ifdef USE_NEWSRC_MAP_FILE
#define NEWSRC_MAP_FILE_COOKIE "netscape-newsrc-map-file"
#endif /* USE_NEWSRC_MAP_FILE */

nsresult
nsMsgAccountManager::MigrateNewsAccounts(nsIMsgIdentity *identity)
{
	nsresult rv;
    nsFileSpec newsrcDir; // the directory that holds the newsrc files (and the fat file, if we are using one)
    nsFileSpec newsHostsDir; // the directory that holds the host directory, and the summary files.
	nsFileSpec profileDir;
    char *news_directory_value = nsnull;

    // the host directories will be under <profile dir>/News or
    // <"news.directory" pref>/News.
    //
    // the newsrc file don't necessarily live under there.
    //
    // if we didn't use the map file (UNIX) then we want to force the newsHostsDir to be
    // <profile>/News.  in 4.x, on UNIX, the summary files lived in ~/.netscape/xover-cache/<hostname>
    // in 5.0, they will live under <profile>/News/<hostname>, like the other platforms.
    // in 4.x, on UNIX, the "news.directory" pref pointed to the directory to where
    // the newsrc files lived.  we don't want that for the newsHostsDir.
#ifdef USE_NEWSRC_MAP_FILE
#ifdef HEED_DIR_PREFS
    rv = m_prefs->CopyCharPref(PREF_NEWS_DIRECTORY, &news_directory_value);
#else
    rv = NS_ERROR_FAILURE;
#endif /* HEED_DIR_PREFS */
#else
    rv = NS_ERROR_FAILURE;
#endif /* USE_NEWSRC_MAP_FILE */
	if (NS_SUCCEEDED(rv) && news_directory_value && (PL_strlen(news_directory_value) > 0)) {
      newsHostsDir = news_directory_value;
      PR_FREEIF(news_directory_value);
    }
    else {
      NS_WITH_SERVICE(nsIProfile, profile, kProfileCID, &rv);
      if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
      
      rv = profile->GetCurrentProfileDir(&profileDir);
      if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
      
      newsHostsDir = profileDir;
      
      // we want <profile>/News, not <profile>
      newsHostsDir += NEW_NEWS_DIR_NAME;
      
      // create <profile>/News if it doesn't exist
      if (!newsHostsDir.Exists()) {
		newsHostsDir.CreateDir();
      }
    }

    rv = SetServerRootPref(PREF_MAIL_ROOT_NNTP, newsHostsDir); 
    if (NS_FAILED(rv)) return rv;

#ifdef USE_NEWSRC_MAP_FILE	
    // if we are using the fat file, it lives in the newsHostsDir.
    newsrcDir = newsHostsDir;

    // the fat file is really <newsrcDir>/<fat file name>
    // example:  News\fat on Windows, News\NewsFAT on the Mac
    // don't worry, after we migrate, the fat file is going away
    nsFileSpec fatFile(newsrcDir);
	fatFile += NEWS_FAT_FILE_NAME;

    // for each news server in the fat file call MigrateNewsAccount();
	char buffer[512];
	char psuedo_name[512];
	char filename[512];
	char is_newsgroup[512];
	PRBool ok;

	nsInputFileStream inputStream(fatFile);
	
	if (inputStream.eof()) {
		inputStream.close();
		return NS_ERROR_FAILURE;
	}
	
    /* we expect the first line to be NEWSRC_MAP_FILE_COOKIE */
	ok = inputStream.readline(buffer, sizeof(buffer));
	
    if ((!ok) || (PL_strncmp(buffer, NEWSRC_MAP_FILE_COOKIE, PL_strlen(NEWSRC_MAP_FILE_COOKIE)))) {
		inputStream.close();
		return NS_ERROR_FAILURE;
	}   
	
	while (!inputStream.eof()) {
		char * p;
		PRInt32 i;
		
		ok = inputStream.readline(buffer, sizeof(buffer));
		if (!ok) {
			inputStream.close();
			return NS_ERROR_FAILURE;
		}  
		
		/* TODO: replace this with nsString code? */

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
		
		if(PL_strncmp(is_newsgroup, "TRUE", 4)) {
#ifdef NEWS_FAT_STORES_ABSOLUTE_NEWSRC_FILE_PATHS
			// most likely, the fat file has been copied (or moved ) from
			// its old location.  So the absolute file paths will be wrong.
			// all we care about is the leaf, so use that.
			nsFileSpec oldRcFile(filename);
			char *leaf = oldRcFile.GetLeafName();

			nsFileSpec rcFile(newsrcDir);
			rcFile += leaf;
			nsCRT::free(leaf);
			leaf = nsnull;
#else
			nsFileSpec rcFile(newsrcDir);
			rcFile += filename;
#endif /* NEWS_FAT_STORES_ABSOLUTE_NEWSRC_FILE_PATHS */

			// psuedo-name is of the form newsrc-<host> or snewsrc-<host>.  
			// right now, we can't handle snewsrc, so if we get one of those
			// gracefully handle it by ignoring it.
			if (PL_strncmp(PSUEDO_NAME_PREFIX,psuedo_name,PL_strlen(PSUEDO_NAME_PREFIX)) != 0) {
				continue;
			}

			// check that there is a hostname to get after the "newsrc-" part
			NS_ASSERTION(PL_strlen(psuedo_name) > PL_strlen(PSUEDO_NAME_PREFIX), "psuedo_name is too short");
			if (PL_strlen(psuedo_name) <= PL_strlen(PSUEDO_NAME_PREFIX)) {
				return NS_ERROR_FAILURE;
			}

			char *hostname = psuedo_name + PL_strlen(PSUEDO_NAME_PREFIX);
            rv = MigrateNewsAccount(identity, hostname, rcFile, newsHostsDir);
            if (NS_FAILED(rv)) {
				// failed to migrate.  bail out
				return rv;
			}
		}
	}
	
	inputStream.close();
#else /* USE_NEWSRC_MAP_FILE */
    rv = m_prefs->CopyCharPref(PREF_PREMIGRATION_NEWS_DIRECTORY, &news_directory_value);
    if (NS_FAILED(rv) || !news_directory_value || (PL_strlen(news_directory_value) == 0)) {
#ifdef DEBUG_ACCOUNTMANAGER
      printf("%s was not set, attempting to use %s instead.\n",PREF_PREMIGRATION_NEWS_DIRECTORY,PREF_NEWS_DIRECTORY);
#endif
      PR_FREEIF(news_directory_value);
      rv = m_prefs->CopyCharPref(PREF_NEWS_DIRECTORY, &news_directory_value);
    }
    
	if (NS_SUCCEEDED(rv) && news_directory_value && (PL_strlen(news_directory_value) > 0)) {
      newsrcDir = news_directory_value;
      PR_FREEIF(news_directory_value);
    }
    else {
      // "news.directory" and "premigration.news.directory" fail, use the home directory.
#ifdef XP_UNIX
      nsSpecialSystemDirectory homeDir(nsSpecialSystemDirectory::Unix_HomeDirectory);
#elif XP_BEOS
      nsSpecialSystemDirectory homeDir(nsSpecialSystemDirectory::BeOS_HomeDirectory);
#else
#error where_are_your_newsrc_files
#endif /* XP_UNIX, XP_BEOS */
      newsrcDir = homeDir;
	}

    for (nsDirectoryIterator i(newsrcDir, PR_FALSE); i.Exists(); i++) {
      nsFileSpec possibleRcFile = i.Spec();

      char *filename = possibleRcFile.GetLeafName();
      
      if ((PL_strncmp(NEWSRC_FILE_PREFIX, filename, PL_strlen(NEWSRC_FILE_PREFIX)) == 0) && (PL_strlen(filename) > PL_strlen(NEWSRC_FILE_PREFIX))) {
#ifdef DEBUG_ACCOUNTMANAGER
        printf("found a newsrc file: %s\n", filename);
#endif
        char *hostname = filename + PL_strlen(NEWSRC_FILE_PREFIX);
        rv = MigrateNewsAccount(identity, hostname, possibleRcFile, newsHostsDir);
        if (NS_FAILED(rv)) {
          // failed to migrate.  bail out
          return rv;
        }
      }
      nsCRT::free(filename);
      filename = nsnull;
    }
#endif /* USE_NEWSRC_MAP_FILE */

    rv = SetServerRootPref(PREF_MAIL_NEWSRC_ROOT, newsrcDir); 
    if (NS_FAILED(rv)) return rv;

	return NS_OK;
}

nsresult
nsMsgAccountManager::MigrateNewsAccount(nsIMsgIdentity *identity, const char *hostname, nsFileSpec & newsrcfile, nsFileSpec & newsHostsDir)
{  
	nsresult rv;
	nsFileSpec thisNewsHostsDir = newsHostsDir;
        if (!identity) return NS_ERROR_NULL_POINTER;
	if (!hostname) return NS_ERROR_NULL_POINTER;

    // create the account
	nsCOMPtr<nsIMsgAccount> account;
    rv = CreateAccount(getter_AddRefs(account));
	if (NS_FAILED(rv)) return rv;

    // create the server
	nsCOMPtr<nsIMsgIncomingServer> server;
    rv = CreateIncomingServer("nntp", getter_AddRefs(server));
	if (NS_FAILED(rv)) return rv;

    // create the identity
	nsCOMPtr<nsIMsgIdentity> copied_identity;
    rv = CreateIdentity(getter_AddRefs(copied_identity));
	if (NS_FAILED(rv)) return rv;

    // make this new identity to copy of the identity
    // that we created out of the 4.x prefs
	rv = CopyIdentity(identity,copied_identity);
	if (NS_FAILED(rv)) return rv;

    rv = SetNewsCcAndFccValues(copied_identity);
    if (NS_FAILED(rv)) return rv;

    // hook them together
	account->SetIncomingServer(server);
	account->AddIdentity(copied_identity);
	
	// now upgrade all the prefs
	server->SetType("nntp");
    server->SetHostName((char *)hostname);

    rv = MigrateOldNntpPrefs(server, hostname, newsrcfile);
    if (NS_FAILED(rv)) return rv;
		
	// can't do dir += "host-"; dir += hostname; 
	// because += on a nsFileSpec inserts a separator
	// so we'd end up with host-/<hostname> and not host-<hostname>
	nsCAutoString alteredHost ((const char *) "host-");
	alteredHost += hostname;
	NS_MsgHashIfNecessary(alteredHost);	
	thisNewsHostsDir += (const char *) alteredHost;

    nsCOMPtr <nsIFileSpec> newsDir;
    PRBool dirExists;
	rv = NS_NewFileSpecWithSpec(thisNewsHostsDir, getter_AddRefs(newsDir));
	if (NS_FAILED(rv)) return rv;

    server->SetLocalPath(newsDir);
		
	rv = newsDir->Exists(&dirExists);
	if (NS_FAILED(rv)) return rv;
    	
	if (!dirExists) {
		newsDir->CreateDir();
	}

	return NS_OK;
}

nsresult
nsMsgAccountManager::MigrateOldNntpPrefs(nsIMsgIncomingServer *server, const char *hostname, nsFileSpec & newsrcfile)
{
  nsresult rv;
  
  // some of this ought to be moved out into the NNTP implementation
  nsCOMPtr<nsINntpIncomingServer> nntpServer;
  nntpServer = do_QueryInterface(server, &rv);
  if (NS_FAILED(rv)) return rv;
    
#ifdef SUPPORT_SNEWS
#error THIS_CODE_ISNT_DONE_YET
  MIGRATE_STR_PREF("???nntp.server.%s.userName",hostname,server,SetUsername)
#ifdef CAN_UPGRADE_4x_PASSWORDS
  MIGRATE_STR_PREF("???nntp.server.%s.password",hostname,server,SetPassword)
#endif /* CAN_UPGRADE_4x_PASSWORDS */
#endif /* SUPPORT_SNEWS */
	
  nsCOMPtr <nsIFileSpec> path;
  rv = NS_NewFileSpecWithSpec(newsrcfile, getter_AddRefs(path));
  if (NS_FAILED(rv)) return rv;

  nntpServer->SetNewsrcFilePath(path);
    
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
  if (NS_FAILED(rv)) return PR_TRUE;

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
  nsresult rv;
  *aResult = nsnull;

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
  nsCOMPtr<nsIMsgAccount> account = do_QueryInterface(element, &rv);
  if (NS_FAILED(rv)) return PR_TRUE;
  
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
nsMsgAccountManager::SetServerRootPref(const char *pref_name, nsFileSpec & dir)
{
  nsresult rv;
  if (!pref_name) return NS_ERROR_NULL_POINTER;

  nsCOMPtr <nsIFileSpec> dirForPrefs;
  rv = NS_NewFileSpecWithSpec(dir, getter_AddRefs(dirForPrefs));
  if (NS_FAILED(rv)) return rv;

  NS_ASSERTION(m_prefs,"m_prefs is null");
  if (!m_prefs) return NS_ERROR_FAILURE;
  rv = m_prefs->SetFilePref(pref_name, dirForPrefs, PR_FALSE /* set default */);
  return rv;
}

nsresult
NS_NewMsgAccountManager(const nsIID& iid, void **result)
{
  nsMsgAccountManager* manager;
  if (!result) return NS_ERROR_NULL_POINTER;
  
  manager = new nsMsgAccountManager();
  
  return manager->QueryInterface(iid, result);
}
