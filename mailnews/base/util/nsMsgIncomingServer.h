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

#ifndef nsMsgIncomingServer_h__
#define nsMsgIncomingServer_h__

#include "nsIMsgIncomingServer.h"
#include "nsIPref.h"
#include "nsIMsgFilterList.h"
#include "msgCore.h"
#include "nsIFolder.h"
#include "nsCOMPtr.h"
#include "nsWeakReference.h"
#include "nsIMsgDatabase.h"

class nsIMsgFolderCache;
class nsIMsgProtocolInfo;

/*
 * base class for nsIMsgIncomingServer - derive your class from here
 * if you want to get some free implementation
 * 
 * this particular implementation is not meant to be used directly.
 */

class NS_MSG_BASE nsMsgIncomingServer : public nsIMsgIncomingServer,
                                        public nsSupportsWeakReference
{
 public:
  nsMsgIncomingServer();
  virtual ~nsMsgIncomingServer();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIMSGINCOMINGSERVER
  
private:
  nsIPref *m_prefs;
  nsCString m_serverKey;
  nsCString m_password;
  PRBool m_serverBusy;

protected:
  void getPrefName(const char *serverKey, const char *pref, nsCString& fullPrefName);
  void getDefaultPrefName(const char *pref, nsCString& fullPrefName);

  // these are private pref getters and setters for the password
  // field. Callers should be using Get/Set Password
  NS_IMETHOD GetPrefPassword(char * *aPassword);
  NS_IMETHOD SetPrefPassword(const char * aPassword);
  
  nsCOMPtr <nsIFolder> m_rootFolder;
  nsCOMPtr <nsIMsgRetentionSettings> m_retentionSettings;
  nsresult getDefaultCharPref(const char *pref, char **);
  nsresult getDefaultUnicharPref(const char *pref, PRUnichar **);
  nsresult getDefaultBoolPref(const char *pref, PRBool *);
  nsresult getDefaultIntPref(const char *pref, PRInt32 *);
  
  nsresult CreateRootFolder();
  nsresult StorePassword();  // stuff the password in the single signon database

  nsresult getProtocolInfo(nsIMsgProtocolInfo **aResult);
  nsCOMPtr<nsIFileSpec> mFilterFile;
  nsCOMPtr<nsIMsgFilterList> mFilterList;
  // pref callback to clear the user prefs
  static void clearPrefEnum(const char  *aPref, void *aClosure);

  // member variable for canHaveFilters
  PRBool m_canHaveFilters;
};

#endif // nsMsgIncomingServer_h__
