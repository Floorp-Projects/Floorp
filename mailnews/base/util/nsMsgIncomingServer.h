/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
#include "nsIFileSpec.h"

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
  nsCString m_password;
  PRBool m_serverBusy;

protected:
  nsCString m_serverKey;
  void getPrefName(const char *serverKey, const char *pref, nsCString& fullPrefName);
  void getDefaultPrefName(const char *pref, nsCString& fullPrefName);

  // these are private pref getters and setters for the password
  // field. Callers should be using Get/Set Password
  NS_IMETHOD GetPrefPassword(char * *aPassword);
  NS_IMETHOD SetPrefPassword(const char * aPassword);
  
  nsCOMPtr <nsIFolder> m_rootFolder;
  nsCOMPtr <nsIMsgRetentionSettings> m_retentionSettings;
  nsCOMPtr <nsIMsgDownloadSettings> m_downloadSettings;
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

  // member variable for to check if we need display startup page
  PRBool m_displayStartupPage;
};

#endif // nsMsgIncomingServer_h__
