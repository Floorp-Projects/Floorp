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

#include "nscore.h"
#include "nsIMsgAccountManager.h"
#include "nsCOMPtr.h"
#include "nsHashtable.h"
#include "nsISmtpServer.h"
#include "nsIPref.h"

/*
 * some platforms (like Windows and Mac) use a map file, because of
 * file name length limitations.
 */
#if defined(XP_UNIX) || defined(XP_BEOS)
// if you don't use the fat file, then you need to specify the newsrc file prefix you use
#define NEWSRC_FILE_PREFIX ".newsrc-"
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
  PRBool m_alreadySetNntpDefaultLocalPath;
  PRBool m_alreadySetImapDefaultLocalPath;
  
  nsISupportsArray *m_accounts;
  nsHashtable m_identities;
  nsHashtable m_incomingServers;
  nsCOMPtr<nsIMsgAccount> m_defaultAccount;

  nsCAutoString accountKeyList;
  
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
  
  nsresult CreateLocalMailAccount(nsIMsgIdentity *identity);
  nsresult MigrateLocalMailAccount(nsIMsgIdentity *identity);
  nsresult MigrateOldPopPrefs(nsIMsgIncomingServer *server, const char *hostname);
  
  nsresult MigrateNewsAccounts(nsIMsgIdentity *identity);
  nsresult MigrateNewsAccount(nsIMsgIdentity *identity, const char *hostAndPort, nsFileSpec &newsrcfile, nsFileSpec &newsHostsDir);
  nsresult MigrateOldNntpPrefs(nsIMsgIncomingServer *server, const char *hostAndPort, nsFileSpec &newsrcfile);

  nsresult ProceedWithMigration(PRInt32 oldMailType);
  
  static char *getUniqueKey(const char* prefix, nsHashtable *hashTable);
  static char *getUniqueAccountKey(const char* prefix,
                                   nsISupportsArray *accounts);

  nsresult Convert4XUri(const char *old_uri, const char *default_folder_name, char **new_uri);
  
  nsresult getPrefService();
  nsIPref *m_prefs;
};

