/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
  
  nsCOMPtr <nsIMsgFolderCache>	m_msgFolderCache;
  nsCOMPtr<nsIAtom> kDefaultServerAtom;
  nsISupportsArray *m_accounts;
  nsHashtable m_identities;
  nsHashtable m_incomingServers;
  nsCOMPtr<nsIMsgAccount> m_defaultAccount;
  nsCOMPtr<nsISupportsArray> m_incomingServerListeners;

  nsCAutoString mAccountKeyList;
  
  PRBool m_haveShutdown;


  /* internal creation routines - updates m_identities and m_incomingServers */
  nsresult createKeyedAccount(const char* key,
                              nsIMsgAccount **_retval);
  nsresult createKeyedServer(const char*key,
                             const char* username,
                             const char* password,
                             const char* type,
                             nsIMsgIncomingServer **_retval);

  nsresult createKeyedIdentity(const char* key,
                               nsIMsgIdentity **_retval);

  /* internal destruction routines - fixes prefs */
  nsresult removeKeyedAccount(const char *key);


  // sets the pref for the defualt server
  nsresult setDefaultAccountPref(nsIMsgAccount *aDefaultAccount);

  // fires notifications to the appropriate root folders
  nsresult notifyDefaultServerChange(nsIMsgAccount *aOldAccount,
                                     nsIMsgAccount *aNewAccount);
    
  // hash table enumerators

  // add each member of a hash table to an nsISupports array
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

  // load up the identities into the given nsISupportsArray
  static PRBool getIdentitiesToArray(nsISupports *element, void *aData);

  // add identities if they don't alreadby exist in the given nsISupportsArray
  static PRBool addIdentityIfUnique(nsISupports *element, void *aData);

  //
  // server enumerators
  // ("element" is always a server)
  //
  
  // load up the servers into the given nsISupportsArray
  static PRBool getServersToArray(nsHashKey* aKey, void *element, void *aData);

  // find the server given by {username, hostname, type}
  static PRBool findServer(nsISupports *aElement, void *data);

  // write out the server's cache through the given folder cache
  static PRBool writeFolderCache(nsHashKey *aKey, void *aData, void *closure);
  static PRBool closeCachedConnections(nsHashKey *aKey, void *aData, void *closure);

  static char *getUniqueKey(const char* prefix, nsHashtable *hashTable);
  static char *getUniqueAccountKey(const char* prefix,
                                   nsISupportsArray *accounts);

  
  nsresult SetSendLaterUriPref(nsIMsgIncomingServer *server);
 
  nsresult getPrefService();
  nsIPref *m_prefs;

  //
  // root folder listener stuff
  //
  
  // this array is for folder listeners that are supposed to be listening
  // on the root folders.
  // When a new server is created, all of the the folder listeners
  //    should be added to the new server
  // When a new listener is added, it should be added to all root folders.
  // similar for when servers are deleted or listeners removed
  nsCOMPtr<nsISupportsArray> mFolderListeners;
  
  // add and remove listeners from the given server
  static PRBool addListener(nsHashKey *aKey, void *element, void *aData);
  static PRBool removeListener(nsHashKey *aKey, void *element, void *aData);
  
  // folder listener enumerators
  static PRBool addListenerToFolder(nsISupports *element, void *data);
  static PRBool removeListenerFromFolder(nsISupports *element, void *data);
};

