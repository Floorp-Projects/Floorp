/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#include "nscore.h"
#include "nsIMsgAccountManager.h"
#include "nsCOMPtr.h"
#include "nsHashtable.h"
#include "nsISmtpServer.h"
#include "nsIPref.h"
#include "nsIMsgFolderCache.h"
#include "nsIObserver.h"
#include "nsWeakReference.h"

/*
 * some platforms (like Windows and Mac) use a map file, because of
 * file name length limitations.
 */
#if defined(XP_UNIX) || defined(XP_BEOS)
/* in 4.x, the prefix was ".newsrc-", in 5.0, the profile migrator code copies the newsrc files from
 * ~/.newsrc-* to ~/.mozilla/<profile>/News/newsrc-*
 */
#define NEWSRC_FILE_PREFIX_5x "newsrc-"
#else
#define USE_NEWSRC_MAP_FILE

// in the fat file, the hostname is prefix by this string:
#define PSUEDO_NAME_PREFIX "newsrc-"

#if defined(XP_PC)
#define NEWS_FAT_FILE_NAME "fat"
/*
 * on the PC, the fat file stores absolute paths to the newsrc files
 * on the Mac, the fat file stores relative paths to the newsrc files
 */
#define NEWS_FAT_STORES_ABSOLUTE_NEWSRC_FILE_PATHS 1
#elif defined(XP_MAC)
#define NEWS_FAT_FILE_NAME "NewsFAT"
#else
#error dont_know_what_your_news_fat_file_is
#endif /* XP_PC, XP_MAC */

#endif /* XP_UNIX || XP_BEOS */

#ifdef XP_UNIX
#define HAVE_MOVEMAIL 1
#endif /* HAVE_MOVEMAIL */

class nsMsgAccountManager
	: public nsIMsgAccountManager,
		public nsIObserver,
		public nsSupportsWeakReference
{
public:

  nsMsgAccountManager();
  virtual ~nsMsgAccountManager();
  
  NS_DECL_ISUPPORTS
 
  /* nsIMsgAccountManager methods */
  
  NS_DECL_NSIMSGACCOUNTMANAGER
  NS_DECL_NSIOBSERVER  

  nsresult Init();
  nsresult Shutdown();

private:

  PRBool m_accountsLoaded;
  PRBool m_alreadySetNntpDefaultLocalPath;
  PRBool m_alreadySetImapDefaultLocalPath;
  
  nsCOMPtr <nsIMsgFolderCache>	m_msgFolderCache;
  nsISupportsArray *m_accounts;
  nsHashtable m_identities;
  nsHashtable m_incomingServers;
  nsCOMPtr<nsIMsgAccount> m_defaultAccount;
  nsCOMPtr<nsISupportsArray> m_incomingServerListeners;

  nsCAutoString accountKeyList;
  
  PRBool m_haveShutdown;


  /* internal creation routines - updates m_identities and m_incomingServers */
  nsresult createKeyedAccount(const char* key,
                              nsIMsgAccount **_retval);
  nsresult createKeyedServer(const char*key,
                             const char* type,
                             nsIMsgIncomingServer **_retval);

  nsresult createKeyedIdentity(const char* key,
                               nsIMsgIdentity **_retval);

  /* internal destruction routines - fixes prefs */
  nsresult removeKeyedAccount(const char *key);
    
  // hash table enumerators


  //
  static PRBool hashElementToArray(nsHashKey *aKey, void *aData,
                                   void *closure);

  // called by EnumerateRemove to release all elements
  static PRBool hashElementRelease(nsHashKey *aKey, void *aData,
                                   void *closure);

  // Send unload server notification.
  static PRBool hashUnloadServer(nsHashKey *aKey, void *aData,
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

  static PRBool findServerIndexByServer(nsISupports *element, void *aData);
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
  static PRBool closeCachedConnections(nsHashKey *aKey, void *aData, void *closure);

  // methods for migration / upgrading
  nsresult MigrateIdentity(nsIMsgIdentity *identity);
  nsresult MigrateSmtpServer(nsISmtpServer *server);
  nsresult CopyIdentity(nsIMsgIdentity *srcIdentity, nsIMsgIdentity *destIdentity);
  nsresult SetNewsCcAndFccValues(nsIMsgIdentity *identity);
  nsresult SetMailCcAndFccValues(nsIMsgIdentity *identity);
   
  nsresult MigrateImapAccounts(nsIMsgIdentity *identity);
  nsresult MigrateImapAccount(nsIMsgIdentity *identity, const char *hostAndPort);
  
  nsresult MigrateOldImapPrefs(nsIMsgIncomingServer *server, const char *hostAndPort);
  
  nsresult MigratePopAccount(nsIMsgIdentity *identity);

#ifdef HAVE_MOVEMAIL
  nsresult MigrateMovemailAccount(nsIMsgIdentity *identity);
#endif /* HAVE_MOVEMAIL */
  
  nsresult MigrateLocalMailAccount(nsIMsgIdentity *identity);

  nsresult MigrateOldMailPrefs(nsIMsgIncomingServer *server);
  
  nsresult MigrateNewsAccounts(nsIMsgIdentity *identity);
  nsresult MigrateNewsAccount(nsIMsgIdentity *identity, const char *hostAndPort, nsFileSpec &newsrcfile, nsFileSpec &newsHostsDir);
  nsresult MigrateOldNntpPrefs(nsIMsgIncomingServer *server, const char *hostAndPort, nsFileSpec &newsrcfile);

  nsresult ProceedWithMigration(PRInt32 oldMailType);
  
  static char *getUniqueKey(const char* prefix, nsHashtable *hashTable);
  static char *getUniqueAccountKey(const char* prefix,
                                   nsISupportsArray *accounts);

  nsresult Convert4XUri(const char *old_uri, const char *default_folder_name, char **new_uri);
 
  nsresult SetSendLaterUriPref(nsIMsgIncomingServer *server);
 
  nsresult getPrefService();
  nsIPref *m_prefs;
};

