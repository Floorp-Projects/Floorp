/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 *   Seth Spitzer <sspitzer@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsMsgIncomingServer_h__
#define nsMsgIncomingServer_h__

#include "nsIMsgIncomingServer.h"
#include "nsIPrefBranch.h"
#include "nsIMsgFilterList.h"
#include "msgCore.h"
#include "nsIMsgFolder.h"
#include "nsCOMPtr.h"
#include "nsWeakReference.h"
#include "nsIMsgDatabase.h"
#include "nsIFileSpec.h"
#include "nsISpamSettings.h"
#include "nsIMsgFilterPlugin.h"

class nsIMsgFolderCache;
class nsIMsgProtocolInfo;

/*
 * base class for nsIMsgIncomingServer - derive your class from here
 * if you want to get some free implementation
 * 
 * this particular implementation is not meant to be used directly.
 */

#undef  IMETHOD_VISIBILITY
#define IMETHOD_VISIBILITY NS_VISIBILITY_DEFAULT

class NS_MSG_BASE nsMsgIncomingServer : public nsIMsgIncomingServer,
                                        public nsSupportsWeakReference
{
 public:
  nsMsgIncomingServer();
  virtual ~nsMsgIncomingServer();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIMSGINCOMINGSERVER

protected:
  nsCString m_serverKey;
  void getPrefName(const char *serverKey, const char *pref, nsCString& fullPrefName);
  void getDefaultPrefName(const char *pref, nsCString& fullPrefName);

  nsresult ConfigureTemporaryReturnReceiptsFilter(nsIMsgFilterList *filterList);
  nsresult ConfigureTemporaryServerSpamFilters(nsIMsgFilterList *filterList);

  // these are private pref getters and setters for the password
  // field. Callers should be using Get/Set Password
  NS_IMETHOD GetPrefPassword(char * *aPassword);
  NS_IMETHOD SetPrefPassword(const char * aPassword);
  PRBool     PasswordProtectLocalCache();
  
  nsCOMPtr <nsIMsgFolder> m_rootFolder;
  nsCOMPtr <nsIMsgRetentionSettings> m_retentionSettings;
  nsCOMPtr <nsIMsgDownloadSettings> m_downloadSettings;
  nsresult getDefaultCharPref(const char *pref, char **);
  nsresult getDefaultUnicharPref(const char *pref, PRUnichar **);
  nsresult getDefaultBoolPref(const char *pref, PRBool *);
  nsresult getDefaultIntPref(const char *pref, PRInt32 *);
  
  nsresult CreateLocalFolder(nsIFileSpec *path, const char *folderName);
  nsresult GetDeferredServers(nsIMsgIncomingServer *server, nsISupportsArray **_retval);

  nsresult CreateRootFolder();

  nsresult InternalSetHostName(const char *aHostname, const char *prefName);

  nsresult getProtocolInfo(nsIMsgProtocolInfo **aResult);
  nsCOMPtr <nsIFileSpec> mFilterFile;
  nsCOMPtr <nsIMsgFilterList> mFilterList;

private:
  nsIPrefBranch *m_prefBranch;
  nsCString m_password;
  PRUint32 m_biffState;
  PRPackedBool m_serverBusy;
  nsCOMPtr <nsISpamSettings> mSpamSettings;
  nsCOMPtr<nsIMsgFilterPlugin> mFilterPlugin;  // XXX should be a list

protected:
  PRPackedBool m_canHaveFilters;
  PRPackedBool m_displayStartupPage;
  PRPackedBool mPerformingBiff;
};

#undef  IMETHOD_VISIBILITY
#define IMETHOD_VISIBILITY NS_VISIBILITY_HIDDEN

#endif // nsMsgIncomingServer_h__
