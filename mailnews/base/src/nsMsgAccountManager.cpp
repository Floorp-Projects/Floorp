/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 */

#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsISupportsArray.h"
#include "nsMsgAccountManager.h"
#include "nsMsgBaseCID.h"
#include "nsMsgCompCID.h"
#include "prmem.h"
#include "plstr.h"
#include "nsString.h"
#include "nsXPIDLString.h"
#include "nscore.h"
#include "nsIProfile.h"
#include "nsIFileLocator.h"
#include "nsFileLocations.h"
#include "nsCRT.h"  // for nsCRT::strtok
#include "prprf.h"
#include "nsINetSupportDialogService.h"
#include "nsIMsgFolderCache.h"
#include "nsFileStream.h"
#include "nsMsgUtils.h"
#include "nsSpecialSystemDirectory.h"
#include "nsIFileLocator.h" 
#include "nsIFileSpec.h" 
#include "nsFileLocations.h" 
#include "nsIURL.h"
#include "nsISmtpService.h"
#include "nsString.h"
#include "nsIMsgBiffManager.h"
#include "nsIObserverService.h"

// this should eventually be moved to the pop3 server for upgrading
#include "nsIPop3IncomingServer.h"
// this should eventually be moved to the imap server for upgrading
#include "nsIImapIncomingServer.h"
// this should eventually be moved to the nntp server for upgrading
#include "nsINntpIncomingServer.h"
// this should eventually be moved to the no server for upgrading
#include "nsINoIncomingServer.h"

#define BUF_STR_LEN 1024

#if defined(DEBUG_alecf) || defined(DEBUG_sspitzer_) || defined(DEBUG_seth_)
#define DEBUG_ACCOUNTMANAGER 1
#endif

static NS_DEFINE_CID(kMsgAccountCID, NS_MSGACCOUNT_CID);
static NS_DEFINE_CID(kMsgIdentityCID, NS_MSGIDENTITY_CID);
static NS_DEFINE_CID(kPrefServiceCID, NS_PREF_CID);
static NS_DEFINE_CID(kMsgBiffManagerCID, NS_MSGBIFFMANAGER_CID);
static NS_DEFINE_CID(kProfileCID, NS_PROFILE_CID);
static NS_DEFINE_CID(kCNetSupportDialogCID, NS_NETSUPPORTDIALOG_CID);
static NS_DEFINE_CID(kStandardUrlCID, NS_STANDARDURL_CID);   
static NS_DEFINE_CID(kSmtpServiceCID, NS_SMTPSERVICE_CID);   
static NS_DEFINE_CID(kFileLocatorCID,       NS_FILELOCATOR_CID);
static NS_DEFINE_IID(kIFileLocatorIID,      NS_IFILELOCATOR_IID);
static NS_DEFINE_CID(kMsgFolderCacheCID, NS_MSGFOLDERCACHE_CID);

#define IMAP_SCHEMA "imap:/"
#define IMAP_SCHEMA_LENGTH 6
#define MAILBOX_SCHEMA "mailbox:/"
#define MAILBOX_SCHEMA_LENGTH 9

#define POP_4X_MAIL_TYPE 0
#define IMAP_4X_MAIL_TYPE 1
#ifdef HAVE_MOVEMAIL
#define MOVEMAIL_4X_MAIL_TYPE 2
#endif /* HAVE_MOVEMAIL */

#define PREF_MAIL_ACCOUNTMANAGER_ACCOUNTS "mail.accountmanager.accounts"
/* TODO:  do we want to clear these after migration? */
#define PREF_NEWS_DIRECTORY "news.directory"
#define PREF_MAIL_DIRECTORY "mail.directory"
#define PREF_PREMIGRATION_MAIL_DIRECTORY "premigration.mail.directory"
#define PREF_PREMIGRATION_NEWS_DIRECTORY "premigration.news.directory"
#define PREF_IMAP_DIRECTORY "mail.imap.root_dir"
#define PREF_MAIL_DEFAULT_SENDLATER_URI "mail.default_sendlater_uri"

/* 
 * TODO:  these need to be put into a string bundle
 * see bug #18364
 */
#define LOCAL_MAIL_FAKE_HOST_NAME "Local Folders"
#define LOCAL_MAIL_FAKE_USER_NAME "nobody"
#ifdef HAVE_MOVEMAIL
#define MOVEMAIL_FAKE_HOST_NAME "movemail"
#endif /* HAVE_MOVEMAIL */
#define NEW_MAIL_DIR_NAME	"Mail"
#define NEW_NEWS_DIR_NAME	"News"
#define NEW_IMAPMAIL_DIR_NAME	"ImapMail"
#define DEFAULT_4X_DRAFTS_FOLDER_NAME "Drafts"
#define DEFAULT_4X_SENT_FOLDER_NAME "Sent"
#define DEFAULT_4X_TEMPLATES_FOLDER_NAME "Templates"
#define UNSENT_MESSAGES_FOLDER_NAME "Unsent Messages"

/* we are going to clear these after migration */
#define PREF_4X_MAIL_IDENTITY_USEREMAIL "mail.identity.useremail"
#define PREF_4X_MAIL_IDENTITY_USERNAME "mail.identity.username"
#define PREF_4X_MAIL_IDENTITY_REPLY_TO "mail.identity.reply_to"    
#define PREF_4X_MAIL_IDENTITY_ORGANIZATION "mail.identity.organization"
#define PREF_4X_MAIL_SIGNATURE_FILE "mail.signature_file"
#define PREF_4X_MAIL_SIGNATURE_DATE "mail.signature_date"
#define PREF_4X_MAIL_COMPOSE_HTML "mail.html_compose"
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
#define PREF_4X_NEWS_MAX_ARTICLES "news.max_articles"
#define PREF_4X_NEWS_NOTIFY_ON "news.notify.on"
#define PREF_4X_NEWS_NOTIFY_SIZE "news.notify.size"
#define PREF_4X_NEWS_MARK_OLD_READ "news.mark_old_read"

#define CONVERT_4X_URI(IDENTITY,DEFAULT_FOLDER_NAME,MACRO_GETTER,MACRO_SETTER) \
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
    macro_rv = Convert4XUri((const char *)macro_oldStr, DEFAULT_FOLDER_NAME, &converted_uri); \
    if (NS_FAILED(macro_rv)) { \
      IDENTITY->MACRO_SETTER("");	\
    } \
    else { \
      IDENTITY->MACRO_SETTER(converted_uri); \
    } \
    PR_FREEIF(converted_uri); \
  }	\
}

#define COPY_IDENTITY_FILE_VALUE(SRC_ID,DEST_ID,MACRO_GETTER,MACRO_SETTER) 	\
	{	\
		nsresult macro_rv;	\
		nsCOMPtr <nsIFileSpec>macro_spec;   \
        	macro_rv = SRC_ID->MACRO_GETTER(getter_AddRefs(macro_spec)); \
        	if (NS_FAILED(macro_rv)) return macro_rv;	\
        	DEST_ID->MACRO_SETTER(macro_spec);     \
	}

#define COPY_IDENTITY_INT_VALUE(SRC_ID,DEST_ID,MACRO_GETTER,MACRO_SETTER) 	\
	{	\
		    nsresult macro_rv;	\
        	PRInt32 macro_oldInt;	\
        	macro_rv = SRC_ID->MACRO_GETTER(&macro_oldInt);	\
        	if (NS_FAILED(macro_rv)) return macro_rv;	\
        	DEST_ID->MACRO_SETTER(macro_oldInt);     \
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
                	DEST_ID->MACRO_SETTER(macro_oldStr);	\
        	}	\
	}

static const PRUnichar unicharEmptyString[] = { (PRUnichar)'\0' };

#define COPY_IDENTITY_WSTR_VALUE(SRC_ID,DEST_ID,MACRO_GETTER,MACRO_SETTER) 	\
	{	\
        	nsXPIDLString macro_oldStr;	\
		    nsresult macro_rv;	\
        	macro_rv = SRC_ID->MACRO_GETTER(getter_Copies(macro_oldStr));	\
        	if (NS_FAILED(macro_rv)) return macro_rv;	\
        	if (!macro_oldStr) {	\
                	DEST_ID->MACRO_SETTER(unicharEmptyString);	\
        	}	\
        	else {	\
                	DEST_ID->MACRO_SETTER(macro_oldStr);	\
        	}	\
	}

#define MIGRATE_SIMPLE_FILE_PREF_TO_BOOL_PREF(PREFNAME,MACRO_OBJECT,MACRO_METHOD) \
  { \
    nsresult macro_rv; \
    nsCOMPtr <nsIFileSpec>macro_spec;	\
    macro_rv = m_prefs->GetFilePref(PREFNAME, getter_AddRefs(macro_spec)); \
    if (NS_SUCCEEDED(macro_rv)) { \
	char *macro_oldStr = nsnull; \
	macro_rv = macro_spec->GetUnixStyleFilePath(&macro_oldStr);	\
    	if (NS_SUCCEEDED(macro_rv) && macro_oldStr && (PL_strlen(macro_oldStr) > 0)) { \
		MACRO_OBJECT->MACRO_METHOD(PR_TRUE); \
	}	\
	else {	\
		MACRO_OBJECT->MACRO_METHOD(PR_FALSE); \
	}	\
	PR_FREEIF(macro_oldStr); \
    } \
  }


#define MIGRATE_SIMPLE_FILE_PREF_TO_CHAR_PREF(PREFNAME,MACRO_OBJECT,MACRO_METHOD) \
  { \
    nsresult macro_rv; \
    nsCOMPtr <nsIFileSpec>macro_spec;	\
    macro_rv = m_prefs->GetFilePref(PREFNAME, getter_AddRefs(macro_spec)); \
    if (NS_SUCCEEDED(macro_rv)) { \
	char *macro_oldStr = nsnull; \
	macro_rv = macro_spec->GetUnixStyleFilePath(&macro_oldStr);	\
    	if (NS_SUCCEEDED(macro_rv)) { \
		MACRO_OBJECT->MACRO_METHOD(macro_oldStr); \
	}	\
	PR_FREEIF(macro_oldStr); \
    } \
  }

#define MIGRATE_SIMPLE_FILE_PREF_TO_FILE_PREF(PREFNAME,MACRO_OBJECT,MACRO_METHOD) \
  { \
    nsresult macro_rv; \
    nsCOMPtr <nsIFileSpec>macro_spec;	\
    macro_rv = m_prefs->GetFilePref(PREFNAME, getter_AddRefs(macro_spec)); \
    if (NS_SUCCEEDED(macro_rv)) { \
	MACRO_OBJECT->MACRO_METHOD(macro_spec); \
    } \
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

#define MIGRATE_SIMPLE_WSTR_PREF(PREFNAME,MACRO_OBJECT,MACRO_METHOD) \
  { \
    nsresult macro_rv; \
    PRUnichar *macro_oldStr = nsnull; \
    macro_rv = m_prefs->CopyUnicharPref(PREFNAME, &macro_oldStr); \
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



NS_IMPL_ISUPPORTS3(nsMsgAccountManager, nsIMsgAccountManager, nsIObserver, nsISupportsWeakReference)

nsMsgAccountManager::nsMsgAccountManager() :
  m_accountsLoaded(PR_FALSE),
  m_defaultAccount(null_nsCOMPtr()),
  m_haveShutdown(PR_FALSE),
  m_prefs(0)
{
  NS_INIT_REFCNT();

  m_alreadySetImapDefaultLocalPath = PR_FALSE;
  m_alreadySetNntpDefaultLocalPath = PR_FALSE;
}

nsMsgAccountManager::~nsMsgAccountManager()
{
  nsresult rv;

  if(!m_haveShutdown)
  {
    Shutdown();
	//Don't remove from Observer service in Shutdown because Shutdown also gets called
	//from xpcom shutdown observer.  And we don't want to remove from the service in that case.
	NS_WITH_SERVICE (nsIObserverService, observerService, NS_OBSERVERSERVICE_PROGID, &rv);
    if (NS_SUCCEEDED(rv))
	{    
      nsAutoString topic(NS_XPCOM_SHUTDOWN_OBSERVER_ID);
      observerService->RemoveObserver(this, topic.GetUnicode());
	}
  }

  NS_IF_RELEASE(m_accounts);
}

nsresult nsMsgAccountManager::Init()
{
  nsresult rv;

  rv = NS_NewISupportsArray(&m_accounts);
  if(NS_FAILED(rv)) return rv;

  rv = NS_NewISupportsArray(getter_AddRefs(m_incomingServerListeners));
  if(NS_FAILED(rv)) return rv;

  NS_WITH_SERVICE (nsIObserverService, observerService, NS_OBSERVERSERVICE_PROGID, &rv);
  if (NS_SUCCEEDED(rv))
  {    
    nsAutoString topic(NS_XPCOM_SHUTDOWN_OBSERVER_ID);
    observerService->AddObserver(this, topic.GetUnicode());
  }

  return NS_OK;
}

nsresult nsMsgAccountManager::Shutdown()
{

	if(m_msgFolderCache)
	{
	  WriteToFolderCache(m_msgFolderCache);
	}
  CloseCachedConnections();
  UnloadAccounts();

  if (m_prefs) nsServiceManager::ReleaseService(kPrefServiceCID, m_prefs);


  m_haveShutdown = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP nsMsgAccountManager::Observe(nsISupports *aSubject, const PRUnichar *aTopic, const PRUnichar *someData)
{
  nsAutoString topicString(aTopic);
  nsAutoString shutdownString(NS_XPCOM_SHUTDOWN_OBSERVER_ID);

  if(topicString == shutdownString)
  {
    Shutdown();
  }
	
 return NS_OK;
}

nsresult
nsMsgAccountManager::getPrefService()
{

  // get the prefs service
  nsresult rv = NS_OK;
  
  if (!m_prefs)
    rv = nsServiceManager::GetService(kPrefServiceCID,
                                      NS_GET_IID(nsIPref),
                                      (nsISupports**)&m_prefs);
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
  // null or empty key does not return an identity!
  if (!key || !key[0]) {
    *_retval = nsnull;
    return NS_OK;
  }

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
  
  PR_smprintf_free(NS_CONST_CAST(char*, serverTypePref));
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
  
  server->SetKey(key);
  server->SetType(type);

  nsStringKey hashKey(key);

  // addref for the hashtable
  nsISupports* serversupports = server;
  NS_ADDREF(serversupports);
  m_incomingServers.Put(&hashKey, serversupports);

  NotifyServerLoaded(server);

  *aServer = server;
  NS_ADDREF(*aServer);
  
  return NS_OK;
}


NS_IMETHODIMP
nsMsgAccountManager::RemoveAccount(nsIMsgAccount *aAccount)
{
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
  
  // XXX - need to figure out if this is the last time this server is
  // being used, and only send notification then.
  nsCOMPtr<nsIMsgIncomingServer> server;
  rv = aAccount->GetIncomingServer(getter_AddRefs(server));
  if(NS_SUCCEEDED(rv))
  {
    NotifyServerUnloaded(server);
  }
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
  rv = m_prefs->CopyCharPref(PREF_MAIL_ACCOUNTMANAGER_ACCOUNTS,
                             getter_Copies(accountList));
  
  if (NS_FAILED(rv)) return rv;

  // reconstruct the new account list, re-adding all accounts except
  // the one with 'key'
  nsCString newAccountList;
  char *rest = NS_CONST_CAST(char *,(const char*)accountList);

  char *token = nsCRT::strtok(rest, ",", &rest);
  while (token) {
    nsCAutoString testKey(token);
    testKey.StripWhitespace();

    // re-add the candidate key only if it's not the key we're looking for
    if (!testKey.IsEmpty() && !testKey.Equals(key)) {
      if (!newAccountList.IsEmpty())
        newAccountList += ',';
      newAccountList += testKey;
    }

    token = nsCRT::strtok(rest, ",", &rest);
  }

  // now write the new account list back to the prefs
  rv = m_prefs->SetCharPref(PREF_MAIL_ACCOUNTMANAGER_ACCOUNTS,
                              newAccountList.GetBuffer());
  if (NS_FAILED(rv)) return rv;


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

	nsCOMPtr<nsIFolder> rootFolder;
	rv = server->GetRootFolder(getter_AddRefs(rootFolder));
	if(NS_SUCCEEDED(rv))
		rootFolder->Shutdown(PR_TRUE);

	return PR_TRUE;

}

nsresult nsMsgAccountManager::GetFolderCache(nsIMsgFolderCache* *aFolderCache)
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

    nsCOMPtr <nsIFileSpec> cacheFile;
    NS_WITH_SERVICE(nsIFileLocator, locator, kFileLocatorCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = locator->GetFileLocation(nsSpecialFileSpec::App_MessengerFolderCache50, getter_AddRefs(cacheFile));
    if (NS_FAILED(rv)) return rv;

    m_msgFolderCache->Init(cacheFile);

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

// enumaration for closing cached connections.
PRBool nsMsgAccountManager::closeCachedConnections(nsHashKey *aKey, void *aData,
                                             void *closure)
{
    nsIMsgIncomingServer *server = (nsIMsgIncomingServer*)aData;

	server->CloseCachedConnections();
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
  
  //Ensure biff service has started
  NS_WITH_SERVICE(nsIMsgBiffManager, biffService, kMsgBiffManagerCID, &rv);
  
  // mail.accountmanager.accounts is the main entry point for all accounts

  nsXPIDLCString accountList;
  rv = getPrefService();
  if (NS_SUCCEEDED(rv)) {
    rv = m_prefs->CopyCharPref(PREF_MAIL_ACCOUNTMANAGER_ACCOUNTS,
                                        getter_Copies(accountList));
  }
  
  if (NS_FAILED(rv) || !accountList || !accountList[0]) {
#ifdef DEBUG_ACCOUNTMANAGER
    printf("No accounts.\n");
#endif
    return NS_OK;
  }

  m_accountsLoaded = PR_TRUE;
  
    /* parse accountList and run loadAccount on each string, comma-separated */
#ifdef DEBUG_ACCOUNTMANAGER
    printf("accountList = %s\n", (const char*)accountList);
#endif
   
    nsCOMPtr<nsIMsgAccount> account;
    char *token = nsnull;
    char *rest = NS_CONST_CAST(char*,(const char*)accountList);
    nsCAutoString str;

    token = nsCRT::strtok(rest, ",", &rest);
    while (token) {
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
  nsCAutoString* accountList = (nsCAutoString*) aData;
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
  m_incomingServers.Enumerate(hashUnloadServer, this);

  m_accounts->Clear();          // will release all elements
  m_identities.Reset(hashElementRelease, nsnull);
  m_incomingServers.Reset(hashElementRelease, nsnull);
  m_accountsLoaded = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
nsMsgAccountManager::CloseCachedConnections()
{
	m_incomingServers.Enumerate(closeCachedConnections, nsnull);
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
  if (NS_FAILED(rv)) return PR_TRUE;
  
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

nsresult
nsMsgAccountManager::ProceedWithMigration(PRInt32 oldMailType)
{
  char *prefvalue = nsnull;
  nsresult rv = NS_OK;
  
  if ((oldMailType == POP_4X_MAIL_TYPE) 
#ifdef HAVE_MOVEMAIL
	|| (oldMailType == MOVEMAIL_4X_MAIL_TYPE) 
#endif /* HAVE_MOVEMAIL */
	) {
    // if they were using pop or movemail, "mail.pop_name" must have been set
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
      return rv;
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
    
    nsCOMPtr<nsISmtpServer> smtpServer;
    NS_WITH_SERVICE(nsISmtpService, smtpService, kSmtpServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;    

    rv = smtpService->GetDefaultServer(getter_AddRefs(smtpServer));
    if (NS_FAILED(rv)) return rv;    

    rv = MigrateSmtpServer(smtpServer);
    if (NS_FAILED(rv)) return rv;    

    if ( oldMailType == POP_4X_MAIL_TYPE) {
      // in 4.x, you could only have one pop account
      rv = MigratePopAccount(identity);
      if (NS_FAILED(rv)) return rv;

      // everyone gets a local mail account in 5.0
      rv = CreateLocalMailAccount(identity, PR_TRUE);
      if (NS_FAILED(rv)) return rv;
    }
    else if (oldMailType == IMAP_4X_MAIL_TYPE) {
      rv = MigrateImapAccounts(identity);
      if (NS_FAILED(rv)) return rv;
      
      // if they had IMAP in 4.x, they also had "Local Mail"
      // we need to migrate that, too.  
      rv = MigrateLocalMailAccount(identity);
      if (NS_FAILED(rv)) return rv;
    }
#ifdef HAVE_MOVEMAIL
    else if (oldMailType == MOVEMAIL_4X_MAIL_TYPE) {
	// if 4.x, you could only have one movemail account
	rv = MigrateMovemailAccount(identity);
	if (NS_FAILED(rv)) return rv;

        // everyone gets a local mail account in 5.0
        rv = CreateLocalMailAccount(identity, PR_TRUE);
        if (NS_FAILED(rv)) return rv;
    }
#endif /* HAVE_MOVEMAIL */
    else {
#ifdef DEBUG_ACCOUNTMANAGER
      printf("Unrecognized server type %d\n", oldMailType);
#endif
      return NS_ERROR_UNEXPECTED;
    }

    rv = MigrateNewsAccounts(identity);
    if (NS_FAILED(rv)) return rv;

    // we're done migrating, let's save the prefs
    rv = m_prefs->SavePrefFile();
    if (NS_FAILED(rv)) return rv;

    // XXX TODO: remove dummy migration identity
    return NS_OK;
}

NS_IMETHODIMP nsMsgAccountManager::AddIncomingServerListener(nsIIncomingServerListener *serverListener)
{
    m_incomingServerListeners->AppendElement(serverListener);
    return NS_OK;
}

NS_IMETHODIMP nsMsgAccountManager::RemoveIncomingServerListener(nsIIncomingServerListener *serverListener)
{
    m_incomingServerListeners->RemoveElement(serverListener);
    return NS_OK;
}


NS_IMETHODIMP nsMsgAccountManager::NotifyServerLoaded(nsIMsgIncomingServer *server)
{
	nsresult rv;
	PRUint32 count;
	rv = m_incomingServerListeners->Count(&count);
	if (NS_FAILED(rv)) return rv;

	
	for(PRUint32 i = 0; i < count; i++)
	{
		nsCOMPtr<nsIIncomingServerListener> listener = 
			getter_AddRefs((nsIIncomingServerListener*)m_incomingServerListeners->ElementAt(i));
		listener->OnServerLoaded(server);
	}

	return NS_OK;
}

NS_IMETHODIMP nsMsgAccountManager::NotifyServerUnloaded(nsIMsgIncomingServer *server)
{
	nsresult rv;
	PRUint32 count;
	rv = m_incomingServerListeners->Count(&count);
	if (NS_FAILED(rv)) return rv;

	
	for(PRUint32 i = 0; i < count; i++)
	{
		nsCOMPtr<nsIIncomingServerListener> listener = 
			getter_AddRefs((nsIIncomingServerListener*)m_incomingServerListeners->ElementAt(i));
		listener->OnServerUnloaded(server);
	}

	return NS_OK;
}


nsresult
nsMsgAccountManager::MigrateIdentity(nsIMsgIdentity *identity)
{
  /* NOTE:  if you add prefs here, make sure you update CopyIdentity() */
  MIGRATE_SIMPLE_STR_PREF(PREF_4X_MAIL_IDENTITY_USEREMAIL,identity,SetEmail)
  MIGRATE_SIMPLE_WSTR_PREF(PREF_4X_MAIL_IDENTITY_USERNAME,identity,SetFullName)
  MIGRATE_SIMPLE_STR_PREF(PREF_4X_MAIL_IDENTITY_REPLY_TO,identity,SetReplyTo)
  MIGRATE_SIMPLE_WSTR_PREF(PREF_4X_MAIL_IDENTITY_ORGANIZATION,identity,SetOrganization)
  MIGRATE_SIMPLE_BOOL_PREF(PREF_4X_MAIL_COMPOSE_HTML,identity,SetComposeHtml)
  MIGRATE_SIMPLE_STR_PREF(PREF_4X_MAIL_DEFAULT_DRAFTS,identity,SetDraftFolder)
  MIGRATE_SIMPLE_FILE_PREF_TO_FILE_PREF(PREF_4X_MAIL_SIGNATURE_FILE,identity,SetSignature);
  MIGRATE_SIMPLE_FILE_PREF_TO_BOOL_PREF(PREF_4X_MAIL_SIGNATURE_FILE,identity,SetAttachSignature);
  MIGRATE_SIMPLE_INT_PREF(PREF_4X_MAIL_SIGNATURE_DATE,identity,SetSignatureDate);
  CONVERT_4X_URI(identity,DEFAULT_4X_DRAFTS_FOLDER_NAME,GetDraftFolder,SetDraftFolder)
  MIGRATE_SIMPLE_STR_PREF(PREF_4X_MAIL_DEFAULT_TEMPLATES,identity,SetStationeryFolder)
  CONVERT_4X_URI(identity,DEFAULT_4X_TEMPLATES_FOLDER_NAME,GetStationeryFolder,SetStationeryFolder)
  /* NOTE:  if you add prefs here, make sure you update CopyIdentity() */
  return NS_OK;
}

nsresult
nsMsgAccountManager::MigrateSmtpServer(nsISmtpServer *server)
{
  MIGRATE_SIMPLE_STR_PREF(PREF_4X_NETWORK_HOSTS_SMTP_SERVER,server,SetHostname)
  MIGRATE_SIMPLE_STR_PREF(PREF_4X_MAIL_SMTP_NAME,server,SetUsername)
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
  if (NS_FAILED(rv)) {
	  MIGRATE_SIMPLE_FILE_PREF_TO_CHAR_PREF(PREF_4X_NEWS_DEFAULT_FCC,identity,SetFccFolder)
  }
  else {
	  if (news_used_uri_for_sent_in_4x) {
	    MIGRATE_SIMPLE_STR_PREF(PREF_4X_NEWS_IMAP_SENTMAIL_PATH,identity,SetFccFolder)
	  }
	  else {
	    MIGRATE_SIMPLE_FILE_PREF_TO_CHAR_PREF(PREF_4X_NEWS_DEFAULT_FCC,identity,SetFccFolder)
	  }
  }
  CONVERT_4X_URI(identity,DEFAULT_4X_SENT_FOLDER_NAME,GetFccFolder,SetFccFolder)

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
  if (NS_FAILED(rv)) {
	MIGRATE_SIMPLE_FILE_PREF_TO_CHAR_PREF(PREF_4X_MAIL_DEFAULT_FCC,identity,SetFccFolder)
  }
  else {
	if (imap_used_uri_for_sent_in_4x) {
		MIGRATE_SIMPLE_STR_PREF(PREF_4X_MAIL_IMAP_SENTMAIL_PATH,identity,SetFccFolder)
	}
	else {
		MIGRATE_SIMPLE_FILE_PREF_TO_CHAR_PREF(PREF_4X_MAIL_DEFAULT_FCC,identity,SetFccFolder)
	}
  }
  CONVERT_4X_URI(identity,DEFAULT_4X_SENT_FOLDER_NAME,GetFccFolder,SetFccFolder)
    
  return NS_OK;
}

// caller will free the memory
nsresult
nsMsgAccountManager::Convert4XUri(const char *old_uri, const char *default_folder_name, char **new_uri)
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
#endif /* DEBUG_ACCOUNTMANAGER */

  if (PL_strncasecmp(IMAP_SCHEMA,old_uri,IMAP_SCHEMA_LENGTH) == 0) {
	nsCOMPtr <nsIURL> url;
	nsXPIDLCString hostname;
	nsXPIDLCString username;

	rv = nsComponentManager::CreateInstance(kStandardUrlCID, nsnull, NS_GET_IID(nsIURL), getter_AddRefs(url));
        if (NS_FAILED(rv)) return rv;

        rv = url->SetSpec(old_uri);
        if (NS_FAILED(rv)) return rv;

        rv = url->GetHost(getter_Copies(hostname));
        if (NS_FAILED(rv)) return rv;
        rv = url->GetPreHost(getter_Copies(username));  
	if (NS_FAILED(rv)) return rv;

	// in 4.x, mac and windows stored the URI as IMAP://<hostname> 
	// if the URI was the default folder on the server.
	// If it wasn't the default folder, they would have stored it as
	if (!username || (PL_strlen((const char *)username) == 0)) {
		char *imap_username = nsnull;
		char *prefname = nsnull;
		prefname = PR_smprintf("mail.imap.server.%s.userName", (const char *)hostname);
		if (!prefname) return NS_ERROR_FAILURE;

		rv = m_prefs->CopyCharPref(prefname, &imap_username);
		PR_FREEIF(prefname);
		if (NS_FAILED(rv) || !imap_username || !*imap_username) {
			*new_uri = PR_smprintf("");
			return NS_ERROR_FAILURE;
		}
		else {
			// new_uri = imap://<username>@<hostname>/<folder name>
#ifdef DEBUG_ACCOUNTMANAGER
			printf("new_uri = %s/%s@%s/%s\n",IMAP_SCHEMA, imap_username, (const char *)hostname, default_folder_name);
#endif /* DEBUG_ACCOUNTMANAGER */
			*new_uri = PR_smprintf("%s/%s@%s/%s",IMAP_SCHEMA, imap_username, (const char *)hostname, default_folder_name);
			return NS_OK;      
		}
        }
        else {
		// IMAP uri's began with "IMAP:/".  we need that to be "imap:/"
#ifdef DEBUG_ACCOUNTMANAGER
		printf("new_uri = %s%s\n",IMAP_SCHEMA,old_uri+IMAP_SCHEMA_LENGTH);
#endif /* DEBUG_ACCOUNTMANAGER */
		*new_uri = PR_smprintf("%s%s",IMAP_SCHEMA,old_uri+IMAP_SCHEMA_LENGTH);
		return NS_OK;
	}
  }

  char *usernameAtHostname = nsnull;
  nsCOMPtr <nsIFileSpec> mail_dir;
  char *mail_directory_value = nsnull;
  rv = m_prefs->GetFilePref(PREF_PREMIGRATION_MAIL_DIRECTORY, getter_AddRefs(mail_dir));
  if (NS_SUCCEEDED(rv)) {
	rv = mail_dir->GetUnixStyleFilePath(&mail_directory_value);
  }
  if (NS_FAILED(rv) || !mail_directory_value || (PL_strlen(mail_directory_value) == 0)) {
#ifdef DEBUG_ACCOUNTMANAGER
    printf("%s was not set, attempting to use %s instead.\n",PREF_PREMIGRATION_MAIL_DIRECTORY,PREF_MAIL_DIRECTORY);
#endif
    PR_FREEIF(mail_directory_value);

    rv = m_prefs->GetFilePref(PREF_MAIL_DIRECTORY, getter_AddRefs(mail_dir));
    if (NS_SUCCEEDED(rv)) {
	rv = mail_dir->GetUnixStyleFilePath(&mail_directory_value);
    } 

    if (NS_FAILED(rv) || !mail_directory_value || (PL_strlen(mail_directory_value) == 0)) {
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
#ifdef HAVE_MOVEMAIL
  else if (oldMailType == MOVEMAIL_4X_MAIL_TYPE) {
	char *movemail_username = nsnull; 

	rv = m_prefs->CopyCharPref(PREF_4X_MAIL_POP_NAME, &movemail_username);
	if (NS_FAILED(rv)) return rv; 
	
	usernameAtHostname = PR_smprintf("%s@%s",movemail_username,MOVEMAIL_FAKE_HOST_NAME);

	PR_FREEIF(movemail_username);
  }
#endif /* HAVE_MOVEMAIL */
  else {
#ifdef DEBUG_ACCOUNTMANAGER
    printf("Unrecognized server type %d\n", oldMailType);
#endif
    return NS_ERROR_UNEXPECTED;
  }

  const char *folderPath;

  // mail_directory_value is already in UNIX style at this point...
  if (PL_strncasecmp(MAILBOX_SCHEMA,old_uri,MAILBOX_SCHEMA_LENGTH) == 0) {
#ifdef DEBUG_ACCOUNTMANAGER
	printf("turn %s into %s/%s/(%s - %s)\n",old_uri,MAILBOX_SCHEMA,usernameAtHostname,old_uri + MAILBOX_SCHEMA_LENGTH,mail_directory_value);
#endif
	// the extra -1 is because in 4.x, we had this:
	// mailbox:<PATH> instead of mailbox:/<PATH> 
	folderPath = old_uri + MAILBOX_SCHEMA_LENGTH + PL_strlen(mail_directory_value) -1;
  }
  else {
#ifdef DEBUG_ACCOUNTMANAGER
    printf("turn %s into %s/%s/(%s - %s)\n",old_uri,MAILBOX_SCHEMA,usernameAtHostname,old_uri,mail_directory_value);
#endif
	folderPath = old_uri + PL_strlen(mail_directory_value);
  }
  

  // if folder path is "", then the URI was mailbox:/<foobar>
  // and the directory pref was <foobar>
  // this meant it was reall <foobar>/<default folder name>
  // this insanity only happened on mac and windows.
  if (!folderPath || (PL_strlen(folderPath) == 0)) {
	*new_uri = PR_smprintf("%s/%s/%s",MAILBOX_SCHEMA,usernameAtHostname, default_folder_name);
  }
  else {
	// if the folder path starts with a /, we don't need to add one.
	// the reason for this is on UNIX, we store mail.directory as "/home/sspitzer/nsmail"
	// but on windows, its "C:\foo\bar\mail"
	*new_uri = PR_smprintf("%s/%s%s%s",MAILBOX_SCHEMA,usernameAtHostname,(folderPath[0] == '/')?"":"/",folderPath);
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

NS_IMETHODIMP
nsMsgAccountManager::CreateLocalMailAccount(nsIMsgIdentity *identity, PRBool migrating)
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

  // "none" is the type we use for migrate 4.x "Local Mail"
  server->SetType("none");
  server->SetHostName(LOCAL_MAIL_FAKE_HOST_NAME);

  // we don't want "nobody at Local Folders" to show up in the
  // folder pane, so we set the pretty name to "Local Folders"
  nsAutoString localMailFakeHostName(LOCAL_MAIL_FAKE_HOST_NAME);
  server->SetPrettyName(localMailFakeHostName.ToNewUnicode());

  // the server needs a username
  server->SetUsername(LOCAL_MAIL_FAKE_USER_NAME);


  // create the identity
  nsCOMPtr<nsIMsgIdentity> copied_identity;
  rv = CreateIdentity(getter_AddRefs(copied_identity));
  if (NS_FAILED(rv)) return rv;

    // this only makes sense if we have 4.x prefs, but we don't if identity is null
  if (identity) {
    // make this new identity to copy of the identity
    // that we created out of the 4.x prefs
    rv = CopyIdentity(identity,copied_identity);
    if (NS_FAILED(rv)) return rv;

    // only set the cc and fcc values if we were migrating.
    // otherwise, we won't have them.
    if (migrating) {
	rv = SetMailCcAndFccValues(copied_identity);
	if (NS_FAILED(rv)) return rv;
    }
  }
  else {
    char *profileName = nsnull;
    NS_WITH_SERVICE(nsIProfile, profile, kProfileCID, &rv);
    if (NS_FAILED(rv)) return rv;
    
    rv = profile->GetCurrentProfile(&profileName);
    if (NS_FAILED(rv)) return rv;

    rv = copied_identity->SetEmail(profileName);
    // find out the proper way to delete this
    // until then, leak it.
    // PR_FREEIF(profileName);
    if (NS_FAILED(rv)) return rv;
  }
  
  // hook them together
  account->SetIncomingServer(server);
  account->AddIdentity(copied_identity);

  nsCOMPtr<nsINoIncomingServer> noServer;
  noServer = do_QueryInterface(server, &rv);
  if (NS_FAILED(rv)) return rv;
   
  // create the directory structure for old 4.x "Local Mail"
  // under <profile dir>/Mail/Local Folders or
  // <"mail.directory" pref>/Local Folders 
  nsCOMPtr <nsIFileSpec> mailDir;
  nsFileSpec dir;
  PRBool dirExists;

  // if the "mail.directory" pref is set, use that.
  // if they used -installer, this pref will point to where their files got copied
  // this only makes sense when we are migrating
  // for a new profile, that pref won't be set.
  if (identity && migrating) {
    rv = m_prefs->GetFilePref(PREF_MAIL_DIRECTORY, getter_AddRefs(mailDir));
  }
  else {
    rv = NS_ERROR_FAILURE;
  }

  if (NS_FAILED(rv)) {
    // we want <profile>/Mail
    NS_WITH_SERVICE(nsIFileLocator, locator, kFileLocatorCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = locator->GetFileLocation(nsSpecialFileSpec::App_MailDirectory50, getter_AddRefs(mailDir));
    if (NS_FAILED(rv)) return rv;
  }

  // set the default local path for "none"
  rv = server->SetDefaultLocalPath(mailDir);
  if (NS_FAILED(rv)) return rv;

  rv = mailDir->Exists(&dirExists);  
  if (!dirExists) {
    mailDir->CreateDir();
  }

  // set the local path for this "none" server
  //
  // we need to set this to <profile>/Mail/Local Folders, because that's where
  // the 4.x "Local Mail" (when using imap) got copied.
  // it would be great to use the server key, but we don't know it
  // when we are copying of the mail.
  rv = mailDir->AppendRelativeUnixPath(LOCAL_MAIL_FAKE_HOST_NAME);
  if (NS_FAILED(rv)) return rv; 
  rv = server->SetLocalPath(mailDir);
  if (NS_FAILED(rv)) return rv;
    
  rv = mailDir->Exists(&dirExists);
  if (!dirExists) {
    mailDir->CreateDir();
  }
  
  return NS_OK;
}

nsresult
nsMsgAccountManager::MigrateLocalMailAccount(nsIMsgIdentity *identity) 
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
  // "none" is the type we use for migrating 4.x "Local Mail"
  server->SetType("none");
  server->SetHostName(LOCAL_MAIL_FAKE_HOST_NAME);

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

  // we don't want "nobody at Local Folders" to show up in the
  // folder pane, so we set the pretty name to "Local Folders"
  nsAutoString localMailFakeHostName(LOCAL_MAIL_FAKE_HOST_NAME);
  server->SetPrettyName(localMailFakeHostName.ToNewUnicode());
  
  // create the directory structure for old 4.x "Local Mail"
  // under <profile dir>/Mail/Local Folders or
  // <"mail.directory" pref>/Local Folders 
  nsCOMPtr <nsIFileSpec> mailDir;
  nsFileSpec dir;
  PRBool dirExists;

  // if the "mail.directory" pref is set, use that.
  // if they used -installer, this pref will point to where their files got copied
  rv = m_prefs->GetFilePref(PREF_MAIL_DIRECTORY, getter_AddRefs(mailDir));
  if (NS_FAILED(rv)) {
    // we want <profile>/Mail
    NS_WITH_SERVICE(nsIFileLocator, locator, kFileLocatorCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = locator->GetFileLocation(nsSpecialFileSpec::App_MailDirectory50, getter_AddRefs(mailDir));
    if (NS_FAILED(rv)) return rv;
  }

  // set the default local path for "none"
  rv = server->SetDefaultLocalPath(mailDir);
  if (NS_FAILED(rv)) return rv;

  rv = mailDir->Exists(&dirExists);  
  if (!dirExists) {
    mailDir->CreateDir();
  }

  // set the local path for this "none" server
  //
  // we need to set this to <profile>/Mail/Local Folders, because that's where
  // the 4.x "Local Mail" (when using imap) got copied.
  // it would be great to use the server key, but we don't know it
  // when we are copying of the mail.
  rv = mailDir->AppendRelativeUnixPath(LOCAL_MAIL_FAKE_HOST_NAME);
  if (NS_FAILED(rv)) return rv; 
  rv = server->SetLocalPath(mailDir);
  if (NS_FAILED(rv)) return rv;
    
  rv = mailDir->Exists(&dirExists);
  if (!dirExists) {
    mailDir->CreateDir();
  }
  
  // pass the "Local Folders" server so the send later uri pref 
  // will be "mailbox://nobody@Local Folders/Unsent Messages"
  rv = SetSendLaterUriPref(server);
  return rv;
}

#ifdef HAVE_MOVEMAIL
nsresult
nsMsgAccountManager::MigrateMovemailAccount(nsIMsgIdentity *identity)
{
  nsresult rv;
  
  nsCOMPtr<nsIMsgAccount> account;
  nsCOMPtr<nsIMsgIncomingServer> server;

  rv = CreateAccount(getter_AddRefs(account));
  if (NS_FAILED(rv)) return rv;

  // for right now, none.  eventually, we'll have "movemail"
  rv = CreateIncomingServer("none", getter_AddRefs(server));
  if (NS_FAILED(rv)) return rv;

  // "none" is the type we use for migrating 4.x "Local Mail"
  server->SetType("none");
  server->SetHostName(MOVEMAIL_FAKE_HOST_NAME);  

  // create the identity
  nsCOMPtr<nsIMsgIdentity> copied_identity;
  rv = CreateIdentity(getter_AddRefs(copied_identity));
  if (NS_FAILED(rv)) return rv;

  // make this new identity to copy of the identity
  // that we created out of the 4.x prefs
  rv = CopyIdentity(identity,copied_identity);
  if (NS_FAILED(rv)) return rv;

  // XXX: this probably won't work yet...
  // the cc and fcc values
  rv = SetMailCcAndFccValues(copied_identity);
  if (NS_FAILED(rv)) return rv;

  // hook them together
  account->SetIncomingServer(server);
  account->AddIdentity(copied_identity);
  
  // now upgrade all the prefs
  nsCOMPtr <nsIFileSpec> mailDir;
  nsFileSpec dir;
  PRBool dirExists;

  rv = MigrateOldMailPrefs(server);
  if (NS_FAILED(rv)) return rv;

  // if they used -installer, this pref will point to where their files got copied
  rv = m_prefs->GetFilePref(PREF_MAIL_DIRECTORY, getter_AddRefs(mailDir));
  // create the directory structure for old 4.x pop mail
  // under <profile dir>/Mail/movemail or
  // <"mail.directory" pref>/movemail
  //
  // if the "mail.directory" pref is set, use that.
  if (NS_FAILED(rv)) {
    // we wan't <profile>/Mail
    NS_WITH_SERVICE(nsIFileLocator, locator, kFileLocatorCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = locator->GetFileLocation(nsSpecialFileSpec::App_MailDirectory50, getter_AddRefs(mailDir));
    if (NS_FAILED(rv)) return rv;
  }

  // set the default local path for "none" (eventually, "movemail")
  rv = server->SetDefaultLocalPath(mailDir);
  if (NS_FAILED(rv)) return rv;

  rv = mailDir->Exists(&dirExists);
  if (!dirExists) {
    mailDir->CreateDir();
  }

  // we want .../Mail/movemail, not .../Mail
  rv = mailDir->AppendRelativeUnixPath(MOVEMAIL_FAKE_HOST_NAME);
  if (NS_FAILED(rv)) return rv;
  
  // set the local path for this "none" (eventually, "movemail" server)
  rv = server->SetLocalPath(mailDir);
  if (NS_FAILED(rv)) return rv;
    
  rv = mailDir->Exists(&dirExists);
  if (!dirExists) {
    mailDir->CreateDir();
  }
    
  // pass the server so the send later uri pref 
  // will be something like "mailbox://sspitzer@movemail/Unsent Messages"
  rv = SetSendLaterUriPref(server);
  return rv;
}
#endif /* HAVE_MOVEMAIL */

nsresult
nsMsgAccountManager::MigratePopAccount(nsIMsgIdentity *identity)
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
  char *hostAndPort=nsnull;
  rv = m_prefs->CopyCharPref(PREF_4X_NETWORK_HOSTS_POP_SERVER, &hostAndPort);
  if (NS_FAILED(rv)) return rv;

  nsCString hostname(hostAndPort);
  PRInt32 colonPos = hostname.FindChar(':');
  if (colonPos != -1) {
        nsCString portStr(hostAndPort + colonPos);
        hostname.Truncate(colonPos);
        PRInt32 err;
        PRInt32 port = portStr.ToInteger(&err);
        NS_ASSERTION(err == 0, "failed to get the port\n");
        if (NS_SUCCEEDED(rv)) {
                server->SetPort(port);
        }
  }
#ifdef DEBUG_ACCOUNTMANAGER
  PRInt32 portValue;
  rv = server->GetPort(&portValue);
  printf("HOSTNAME = %s\n", (const char *)hostname);
  printf("PORT = %d\n", portValue);
#endif /* DEBUG_ACCOUNTMANAGER */
  server->SetHostName((const char *)hostname);

  rv = MigrateOldMailPrefs(server);
  if (NS_FAILED(rv)) return rv;

  // if they used -installer, this pref will point to where their files got copied
  rv = m_prefs->GetFilePref(PREF_MAIL_DIRECTORY, getter_AddRefs(mailDir));
  // create the directory structure for old 4.x pop mail
  // under <profile dir>/Mail/<pop host name> or
  // <"mail.directory" pref>/<pop host name>
  //
  // if the "mail.directory" pref is set, use that.
  if (NS_FAILED(rv)) {
    // we wan't <profile>/Mail
    NS_WITH_SERVICE(nsIFileLocator, locator, kFileLocatorCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = locator->GetFileLocation(nsSpecialFileSpec::App_MailDirectory50, getter_AddRefs(mailDir));
    if (NS_FAILED(rv)) return rv;
  }

  // set the default local path for "pop3"
  rv = server->SetDefaultLocalPath(mailDir);
  if (NS_FAILED(rv)) return rv;

  rv = mailDir->Exists(&dirExists);
  if (!dirExists) {
    mailDir->CreateDir();
  }

  // we want .../Mail/<hostname>, not .../Mail
  rv = mailDir->AppendRelativeUnixPath(hostAndPort);
  PR_FREEIF(hostAndPort);
  if (NS_FAILED(rv)) return rv;
  
  // set the local path for this "pop3" server
  rv = server->SetLocalPath(mailDir);
  if (NS_FAILED(rv)) return rv;
    
  rv = mailDir->Exists(&dirExists);
  if (!dirExists) {
    mailDir->CreateDir();
  }
    
  // pass the pop server so the send later uri pref 
  // will be something like "mailbox://sspitzer@tintin/Unsent Messages"
  rv = SetSendLaterUriPref(server);
  return rv;
}
nsresult 
nsMsgAccountManager::SetSendLaterUriPref(nsIMsgIncomingServer *server)
{
	nsresult rv;

	// set "mail.default_sendlater_uri" to something like
	// mailbox://nobody@Local Folders/Unsent Messages"
	// mailbox://sspitzer@tintin/Unsent Messages"
	//
	// note, the schema is mailbox:/ 
	// Unsent is an off-line thing, and that needs to be
	// on the disk, not on an imap server.
	nsXPIDLCString username;
	rv = server->GetUsername(getter_Copies(username));
	if (NS_FAILED(rv)) return rv;

	nsXPIDLCString hostname;
	rv = server->GetHostName(getter_Copies(hostname));
	if (NS_FAILED(rv)) return rv;

	char *sendLaterUriStr = nsnull;
	sendLaterUriStr = PR_smprintf("%s/%s@%s/%s", MAILBOX_SCHEMA, (const char *)username, (const char *)hostname, UNSENT_MESSAGES_FOLDER_NAME);
	m_prefs->SetCharPref(PREF_MAIL_DEFAULT_SENDLATER_URI, sendLaterUriStr);
	PR_FREEIF(sendLaterUriStr);

	return NS_OK;
}

nsresult
nsMsgAccountManager::MigrateOldMailPrefs(nsIMsgIncomingServer * server)
{
  nsresult rv;
    
  MIGRATE_SIMPLE_STR_PREF(PREF_4X_MAIL_POP_NAME,server,SetUsername)
  MIGRATE_SIMPLE_BOOL_PREF(PREF_4X_MAIL_REMEMBER_PASSWORD,server,SetRememberPassword)
#ifdef CAN_UPGRADE_4x_PASSWORDS
  MIGRATE_SIMPLE_STR_PREF(PREF_4X_MAIL_POP_PASSWORD,server,SetPassword)
#else
  rv = server->SetPassword(nsnull);
  if (NS_FAILED(rv)) return rv;
#endif /* CAN_UPGRADE_4x_PASSWORDS */
  MIGRATE_SIMPLE_BOOL_PREF(PREF_4X_MAIL_CHECK_NEW_MAIL,server,SetDoBiff)
  MIGRATE_SIMPLE_INT_PREF(PREF_4X_MAIL_CHECK_TIME,server,SetBiffMinutes)

  nsCOMPtr<nsIPop3IncomingServer> popServer;
  popServer = do_QueryInterface(server, &rv);
  if (NS_SUCCEEDED(rv) && popServer) {
	  MIGRATE_SIMPLE_BOOL_PREF(PREF_4X_MAIL_LEAVE_ON_SERVER,popServer,SetLeaveMessagesOnServer)
	  MIGRATE_SIMPLE_BOOL_PREF(PREF_4X_MAIL_DELETE_MAIL_LEFT_ON_SERVER,popServer,SetDeleteMailLeftOnServer) 
  }
  else {
	// could be a movemail server, in that case do nothing.
  }

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
        COPY_IDENTITY_WSTR_VALUE(srcIdentity,destIdentity,GetFullName,SetFullName)
        COPY_IDENTITY_WSTR_VALUE(srcIdentity,destIdentity,GetOrganization,SetOrganization)
        COPY_IDENTITY_STR_VALUE(srcIdentity,destIdentity,GetDraftFolder,SetDraftFolder)
        COPY_IDENTITY_STR_VALUE(srcIdentity,destIdentity,GetStationeryFolder,SetStationeryFolder)
	COPY_IDENTITY_BOOL_VALUE(srcIdentity,destIdentity,GetAttachSignature,SetAttachSignature)
	COPY_IDENTITY_FILE_VALUE(srcIdentity,destIdentity,GetSignature,SetSignature)
	COPY_IDENTITY_INT_VALUE(srcIdentity,destIdentity,GetSignatureDate,SetSignatureDate)

	return NS_OK;
}

nsresult
nsMsgAccountManager::MigrateImapAccount(nsIMsgIdentity *identity, const char *hostAndPort)
{
  nsresult rv;

  if (!hostAndPort) return NS_ERROR_NULL_POINTER;
 
  
  // create the account
  nsCOMPtr<nsIMsgAccount> account;
  rv = CreateAccount(getter_AddRefs(account));
  if (NS_FAILED(rv)) return rv;

  // create the server
  nsCOMPtr<nsIMsgIncomingServer> server;
  rv = CreateIncomingServer("imap", getter_AddRefs(server));
  if (NS_FAILED(rv)) return rv;
  server->SetType("imap");

  nsCString hostname(hostAndPort);
  PRInt32 colonPos = hostname.FindChar(':');
  if (colonPos != -1) {
	nsCString portStr(hostAndPort + colonPos);
	hostname.Truncate(colonPos);
	PRInt32 err;
	PRInt32 port = portStr.ToInteger(&err);
	NS_ASSERTION(err == 0, "failed to get the port\n");
	if (NS_SUCCEEDED(rv)) {
		server->SetPort(port);
	}
  }
#ifdef DEBUG_ACCOUNTMANAGER
  PRInt32 portValue;
  rv = server->GetPort(&portValue);
  printf("HOSTNAME = %s\n", (const char *)hostname);
  printf("PORT = %d\n", portValue);
#endif /* DEBUG_ACCOUNTMANAGER */
  server->SetHostName((const char *)hostname);

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

  rv = MigrateOldImapPrefs(server, hostAndPort);
  if (NS_FAILED(rv)) return rv;
  
  nsCOMPtr <nsIFileSpec> imapMailDir;
  nsFileSpec dir;
  PRBool dirExists;

  // if they used -installer, this pref will point to where their files got copied
  rv = m_prefs->GetFilePref(PREF_IMAP_DIRECTORY, getter_AddRefs(imapMailDir));
  // if the "mail.imap.root_dir" pref is set, use that.
  if (NS_FAILED(rv)) {
    // we want <profile>/ImapMail
    NS_WITH_SERVICE(nsIFileLocator, locator, kFileLocatorCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = locator->GetFileLocation(nsSpecialFileSpec::App_ImapMailDirectory50, getter_AddRefs(imapMailDir));
    if (NS_FAILED(rv)) return rv;
  }

  // we only need to do this once
  if (!m_alreadySetImapDefaultLocalPath) {
    // set the default local path for "imap"
    rv = server->SetDefaultLocalPath(imapMailDir);
    if (NS_FAILED(rv)) return rv;

    m_alreadySetImapDefaultLocalPath = PR_TRUE;
  }
  
  // we want .../ImapMail/<hostname>, not .../ImapMail
  rv = imapMailDir->AppendRelativeUnixPath((const char *)hostname);
  if (NS_FAILED(rv)) return rv;

  // set the local path for this "imap" server
  rv = server->SetLocalPath(imapMailDir);
  if (NS_FAILED(rv)) return rv;
   
  rv = imapMailDir->Exists(&dirExists);
  if (!dirExists) {
    imapMailDir->CreateDir();
  }
  
  return NS_OK;
}

nsresult
nsMsgAccountManager::MigrateOldImapPrefs(nsIMsgIncomingServer *server, const char *hostAndPort)
{
  nsresult rv;

  // some of this ought to be moved out into the IMAP implementation
  nsCOMPtr<nsIImapIncomingServer> imapServer;
  imapServer = do_QueryInterface(server, &rv);
  if (NS_FAILED(rv)) return rv;

  // upgrade the msg incoming server prefs
  MIGRATE_STR_PREF("mail.imap.server.%s.userName",hostAndPort,server,SetUsername)
  MIGRATE_BOOL_PREF("mail.imap.server.%s.remember_password",hostAndPort,server,SetRememberPassword)
#ifdef CAN_UPGRADE_4x_PASSWORDS
  MIGRATE_STR_PREF("mail.imap.server.%s.password",hostAndPort,server,SetPassword)
#else 
  rv = server->SetPassword(nsnull);
  if (NS_FAILED(rv)) return rv;
#endif /* CAN_UPGRADE_4x_PASSWORDS */
  // upgrade the imap incoming server specific prefs
  MIGRATE_BOOL_PREF("mail.imap.server.%s.check_new_mail",hostAndPort,server,SetDoBiff)
  MIGRATE_INT_PREF("mail.imap.server.%s.check_time",hostAndPort,server,SetBiffMinutes)
  MIGRATE_STR_PREF("mail.imap.server.%s.admin_url",hostAndPort,imapServer,SetAdminUrl)
  MIGRATE_INT_PREF("mail.imap.server.%s.capability",hostAndPort,imapServer,SetCapabilityPref)
  MIGRATE_BOOL_PREF("mail.imap.server.%s.cleanup_inbox_on_exit",hostAndPort,imapServer,SetCleanupInboxOnExit)
  MIGRATE_INT_PREF("mail.imap.server.%s.delete_model",hostAndPort,imapServer,SetDeleteModel)
  MIGRATE_BOOL_PREF("mail.imap.server.%s.dual_use_folders",hostAndPort,imapServer,SetDualUseFolders)
  MIGRATE_BOOL_PREF("mail.imap.server.%s.empty_trash_on_exit",hostAndPort,imapServer,SetEmptyTrashOnExit)
  MIGRATE_INT_PREF("mail.imap.server.%s.empty_trash_threshhold",hostAndPort,imapServer,SetEmptyTrashThreshhold)
  MIGRATE_STR_PREF("mail.imap.server.%s.namespace.other_users",hostAndPort,imapServer,SetOtherUsersNamespace)
  MIGRATE_STR_PREF("mail.imap.server.%s.namespace.personal",hostAndPort,imapServer,SetPersonalNamespace)
  MIGRATE_STR_PREF("mail.imap.server.%s.namespace.public",hostAndPort,imapServer,SetPublicNamespace)
  MIGRATE_BOOL_PREF("mail.imap.server.%s.offline_download",hostAndPort,imapServer,SetOfflineDownload)
  MIGRATE_BOOL_PREF("mail.imap.server.%s.override_namespaces",hostAndPort,imapServer,SetOverrideNamespaces)
  MIGRATE_BOOL_PREF("mail.imap.server.%s.using_subscription",hostAndPort,imapServer,SetUsingSubscription)

  return NS_OK;
}
  
#ifdef USE_NEWSRC_MAP_FILE
#define NEWSRC_MAP_FILE_COOKIE "netscape-newsrc-map-file"
#endif /* USE_NEWSRC_MAP_FILE */

nsresult
nsMsgAccountManager::MigrateNewsAccounts(nsIMsgIdentity *identity)
{
    nsresult rv;
    nsCOMPtr <nsIFileSpec> newsDir;
    nsFileSpec newsrcDir; // the directory that holds the newsrc files (and the fat file, if we are using one)
    nsFileSpec newsHostsDir; // the directory that holds the host directory, and the summary files.
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
    // if they used -installer, this pref will point to where their files got copied
    rv = m_prefs->GetFilePref(PREF_NEWS_DIRECTORY, getter_AddRefs(newsDir));
    if (NS_SUCCEEDED(rv)) {
    	rv = newsDir->GetFileSpec(&newsHostsDir);
    }
#else
    rv = NS_ERROR_FAILURE;
#endif /* USE_NEWSRC_MAP_FILE */
    if (NS_FAILED(rv)) {
	NS_WITH_SERVICE(nsIFileLocator, locator, kFileLocatorCID, &rv);
	if (NS_FAILED(rv)) return rv;

	rv = locator->GetFileLocation(nsSpecialFileSpec::App_NewsDirectory50, getter_AddRefs(newsDir));
	if (NS_FAILED(rv)) return rv;
    }
 
    PRBool dirExists;
    rv = newsDir->Exists(&dirExists);
    if (!dirExists) {
	newsDir->CreateDir();
    }
    
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
    rv = m_prefs->GetFilePref(PREF_NEWS_DIRECTORY, getter_AddRefs(newsDir));
    if (NS_FAILED(rv)) return rv;
    
    rv = newsDir->GetFileSpec(&newsrcDir);
    if (NS_FAILED(rv)) return rv;

    for (nsDirectoryIterator i(newsrcDir, PR_FALSE); i.Exists(); i++) {
      nsFileSpec possibleRcFile = i.Spec();

      char *filename = possibleRcFile.GetLeafName();
      
      if ((PL_strncmp(NEWSRC_FILE_PREFIX_5x, filename, PL_strlen(NEWSRC_FILE_PREFIX_5x)) == 0) && (PL_strlen(filename) > PL_strlen(NEWSRC_FILE_PREFIX_5x))) {
#ifdef DEBUG_ACCOUNTMANAGER
        printf("found a newsrc file: %s\n", filename);
#endif
        char *hostname = filename + PL_strlen(NEWSRC_FILE_PREFIX_5x);
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

	return NS_OK;
}

nsresult
nsMsgAccountManager::MigrateNewsAccount(nsIMsgIdentity *identity, const char *hostAndPort, nsFileSpec & newsrcfile, nsFileSpec & newsHostsDir)
{  
	nsresult rv;
	nsFileSpec thisNewsHostsDir = newsHostsDir;
    if (!identity) return NS_ERROR_NULL_POINTER;
	if (!hostAndPort) return NS_ERROR_NULL_POINTER;

    // create the account
	nsCOMPtr<nsIMsgAccount> account;
    rv = CreateAccount(getter_AddRefs(account));
	if (NS_FAILED(rv)) return rv;

    // create the server
	nsCOMPtr<nsIMsgIncomingServer> server;
    rv = CreateIncomingServer("nntp", getter_AddRefs(server));
	if (NS_FAILED(rv)) return rv;
	// now upgrade all the prefs
	server->SetType("nntp");

	nsCString hostname(hostAndPort);
	PRInt32 colonPos = hostname.FindChar(':');
	if (colonPos != -1) {
		nsCString portStr(hostAndPort + colonPos);
		hostname.Truncate(colonPos);
		PRInt32 err;
		PRInt32 port = portStr.ToInteger(&err);
		NS_ASSERTION(err == 0, "failed to get the port\n");
		if (NS_SUCCEEDED(rv)) {
			server->SetPort(port);
		}
	}
#ifdef DEBUG_ACCOUNTMANAGER
	PRInt32 portValue;
	rv = server->GetPort(&portValue);
	printf("HOSTNAME = %s\n", (const char *)hostname);
	printf("PORT = %d\n", portValue);
#endif /* DEBUG_ACCOUNTMANAGER */
    server->SetHostName((const char *)hostname);

    // we only need to do this once
    if (!m_alreadySetNntpDefaultLocalPath) {
      nsCOMPtr <nsIFileSpec>nntpRootDir;
      rv = NS_NewFileSpecWithSpec(newsHostsDir, getter_AddRefs(nntpRootDir));
      if (NS_FAILED(rv)) return rv;
      
      // set the default local path for "nntp"
      rv = server->SetDefaultLocalPath(nntpRootDir);
      if (NS_FAILED(rv)) return rv;

      // set the newsrc root for "nntp"
      // we really want <profile>/News or /home/sspitzer/
      // not <profile>/News/news.rc or /home/sspitzer/.newsrc-news
      nsFileSpec newsrcFileDir;
      newsrcfile.GetParent(newsrcFileDir);
      if (NS_FAILED(rv)) return rv;
      
      nsCOMPtr <nsIFileSpec>newsrcRootDir;
      rv = NS_NewFileSpecWithSpec(newsrcFileDir, getter_AddRefs(newsrcRootDir));
      if (NS_FAILED(rv)) return rv;
            
      nsCOMPtr<nsINntpIncomingServer> nntpServer;
      nntpServer = do_QueryInterface(server, &rv);
      if (NS_FAILED(rv)) return rv;
      rv = nntpServer->SetNewsrcRootPath(newsrcRootDir);
      if (NS_FAILED(rv)) return rv;

      m_alreadySetNntpDefaultLocalPath = PR_TRUE;
    }
    
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
	
    rv = MigrateOldNntpPrefs(server, hostAndPort, newsrcfile);
    if (NS_FAILED(rv)) return rv;
		
	// can't do dir += "host-"; dir += hostname; 
	// because += on a nsFileSpec inserts a separator
	// so we'd end up with host-/<hostname> and not host-<hostname>
	nsCAutoString alteredHost ((const char *) "host-");
	alteredHost += hostAndPort;
	NS_MsgHashIfNecessary(alteredHost);	
	thisNewsHostsDir += (const char *) alteredHost;

    nsCOMPtr <nsIFileSpec> newsDir;
    PRBool dirExists;
	rv = NS_NewFileSpecWithSpec(thisNewsHostsDir, getter_AddRefs(newsDir));
	if (NS_FAILED(rv)) return rv;

    // set the local path for this "nntp" server
    rv = server->SetLocalPath(newsDir);
    if (NS_FAILED(rv)) return rv;
    
	rv = newsDir->Exists(&dirExists);
	if (!dirExists) {
		newsDir->CreateDir();
	}

	return NS_OK;
}

nsresult
nsMsgAccountManager::MigrateOldNntpPrefs(nsIMsgIncomingServer *server, const char *hostAndPort, nsFileSpec & newsrcfile)
{
  nsresult rv;
  
  // some of this ought to be moved out into the NNTP implementation
  nsCOMPtr<nsINntpIncomingServer> nntpServer;
  nntpServer = do_QueryInterface(server, &rv);
  if (NS_FAILED(rv)) return rv;

  MIGRATE_SIMPLE_INT_PREF(PREF_4X_NEWS_NOTIFY_SIZE,nntpServer,SetNotifySize)
  MIGRATE_SIMPLE_BOOL_PREF(PREF_4X_NEWS_NOTIFY_ON,nntpServer,SetNotifyOn)
  MIGRATE_SIMPLE_BOOL_PREF(PREF_4X_NEWS_MARK_OLD_READ,nntpServer,SetMarkOldRead)
  MIGRATE_SIMPLE_INT_PREF(PREF_4X_NEWS_MAX_ARTICLES,nntpServer,SetMaxArticles)
            
    
#ifdef SUPPORT_SNEWS
#error THIS_CODE_ISNT_DONE_YET
  MIGRATE_STR_PREF("???nntp.server.%s.userName",hostAndPort,server,SetUsername)
#ifdef CAN_UPGRADE_4x_PASSWORDS
  MIGRATE_STR_PREF("???nntp.server.%s.password",hostAndPort,server,SetPassword)
#else
  rv = server->SetPassword(nsnull);
  if (NS_FAILED(rv)) return rv;
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
	
#ifdef DEBUG_ACCOUNTMANAGER
  printf("FindServer(%s,%s,%s,??)\n", username,hostname,type);
#endif
 
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
        
        if (NS_SUCCEEDED(rv)) {
          entry->servers->AppendElement(thisServer);
          break;
        }
        
      }
    }
  }

  return PR_TRUE;
}

NS_IMETHODIMP
nsMsgAccountManager::GetCurrentIdentity(nsIMsgIdentity** id)
{
  NS_ENSURE_ARG_POINTER(id);
  nsCOMPtr<nsISupportsArray> identities;

  nsresult rv = GetAllIdentities(getter_AddRefs(identities));
  if (NS_FAILED(rv)) return rv;

  return identities->QueryElementAt(0, NS_GET_IID(nsIMsgIdentity),
                                    (void **)id);
}

NS_IMETHODIMP
nsMsgAccountManager::GetCurrentServer(nsIMsgIncomingServer ** server)
{
  NS_ENSURE_ARG_POINTER(server);

  nsCOMPtr<nsISupportsArray> servers;

  nsresult rv = GetAllServers(getter_AddRefs(servers));
  if (NS_FAILED(rv)) return rv;

  return servers->QueryElementAt(0, NS_GET_IID(nsIMsgIncomingServer),
                                    (void **)server);
}
