/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Seth Spitzer <sspitzer@netscape.com>
 * Bhuvan Racham <racham@netscape.com>
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

#include <windows.h>

#include "nsMessengerWinIntegration.h"
#include "nsIMsgAccountManager.h"
#include "nsIMsgMailSession.h"
#include "nsIMsgIncomingServer.h"
#include "nsIMsgIdentity.h"
#include "nsIMsgAccount.h"
#include "nsIRDFResource.h"
#include "nsIMsgFolder.h"
#include "nsCOMPtr.h"
#include "nsMsgBaseCID.h"
#include "nsMsgFolderFlags.h"
#include "nsDirectoryServiceDefs.h"
#include "nsIProfile.h"
#include "nsIDirectoryService.h"

#define XP_SHSetUnreadMailCounts "SHSetUnreadMailCountW"
#define XP_SHEnumerateUnreadMailAccounts "SHEnumerateUnreadMailAccountsW"
#define UNREADMAILNODEKEY "Software\\Microsoft\\Windows\\CurrentVersion\\UnreadMail\\"
#define SHELL32_DLL "shell32.dll"
#define DOUBLE_QUOTE "\""
#define PROFILE_COMMANDLINE_ARG " -p "
#define MAIL_COMMANDLINE_ARG " -mail"
#define TIMER_INTERVAL_PREF "mail.windows_xp_integration.unread_count_interval"

nsMessengerWinIntegration::nsMessengerWinIntegration()
{
  NS_INIT_ISUPPORTS();

  mDefaultServerAtom = getter_AddRefs(NS_NewAtom("DefaultServer"));
  mTotalUnreadMessagesAtom = getter_AddRefs(NS_NewAtom("TotalUnreadMessages"));

  mFirstTimeFolderUnreadCountChanged = PR_TRUE;
  mInboxURI = nsnull;
  mEmail = nsnull;
}

nsMessengerWinIntegration::~nsMessengerWinIntegration()
{
  if (mUnreadCountUpdateTimer) {
    mUnreadCountUpdateTimer->Cancel();
    mUnreadCountUpdateTimer = nsnull;
  }

  // one last attempt, update the registry
  nsresult rv = UpdateRegistryWithCurrent();
  NS_ASSERTION(NS_SUCCEEDED(rv), "failed to update registry on shutdown");
  
  CRTFREEIF(mInboxURI);
  CRTFREEIF(mEmail);
}

NS_IMPL_ADDREF(nsMessengerWinIntegration);
NS_IMPL_RELEASE(nsMessengerWinIntegration);

NS_INTERFACE_MAP_BEGIN(nsMessengerWinIntegration)
   NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIMessengerOSIntegration)
   NS_INTERFACE_MAP_ENTRY(nsIMessengerOSIntegration)
   NS_INTERFACE_MAP_ENTRY(nsIFolderListener)
NS_INTERFACE_MAP_END


nsresult
nsMessengerWinIntegration::ResetCurrent()
{
  CRTFREEIF(mInboxURI);
  CRTFREEIF(mEmail);

  mCurrentUnreadCount = -1;
  mLastUnreadCountWrittenToRegistry = -1;

  mDefaultAccountMightHaveAnInbox = PR_TRUE;

  return NS_OK;
}

nsresult
nsMessengerWinIntegration::Init()
{
  nsresult rv;

  // get pref service
  nsCOMPtr<nsIPref> prefService;
  prefService = do_GetService(NS_PREF_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv,rv);

  // first check the timer value
  PRInt32 timerInterval;
  prefService->GetIntPref(TIMER_INTERVAL_PREF, &timerInterval);

  // return if the timer value is negative or ZERO
  if (timerInterval <= 0) {
    return NS_OK;
  }
  else {
    // set the interval for timer. 
    // multiply the value extracted (in seconds) from prefs 
    // with 1000 to convert the interval into milliseconds
    mIntervalTime = timerInterval * 1000; 
  }

  // get directory service to build path for shell dll
  nsCOMPtr<nsIProperties> directoryService = 
    do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv,rv);

  // get path strings needed for unread mail count update
  nsCOMPtr<nsIFile> systemDir;
  rv = directoryService->Get(NS_OS_SYSTEM_DIR, 
                             NS_GET_IID(nsIFile), 
                             getter_AddRefs(systemDir));
  NS_ENSURE_SUCCESS(rv,rv);

  // get path to shell dll.
  nsXPIDLCString shellFile;
  rv = systemDir->GetPath(getter_Copies(shellFile));
  NS_ENSURE_SUCCESS(rv,rv);

  mShellDllPath.Assign(shellFile);
  mShellDllPath.Append("\\");
  mShellDllPath.Append(SHELL32_DLL);

  // load shell dll. If no such dll found, return 
  HMODULE hModule = ::LoadLibrary(mShellDllPath);
  if (!hModule)
    return NS_OK;

  // get process addresses for the unerad mail count functions
  if (hModule) {
    mSHSetUnreadMailCount = (fnSHSetUnreadMailCount)GetProcAddress(hModule, XP_SHSetUnreadMailCounts);
    mSHEnumerateUnreadMailAccounts = (fnSHEnumerateUnreadMailAccounts)GetProcAddress(hModule, XP_SHEnumerateUnreadMailAccounts);
  }

  // if failed to get either of the process addresses, this is not XP platform
  // time to erturn.
  if (!mSHSetUnreadMailCount || !mSHEnumerateUnreadMailAccounts)
	  return NS_OK;

  nsCOMPtr <nsIMsgAccountManager> accountManager = 
    do_GetService(NS_MSGACCOUNTMANAGER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv,rv);

  // because we care if the default server changes
  rv = accountManager->AddRootFolderListener(this);
  NS_ENSURE_SUCCESS(rv,rv);

  nsCOMPtr<nsIMsgMailSession> mailSession =
    do_GetService(NS_MSGMAILSESSION_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv,rv);

  // because we care if the unread total count changes
  rv = mailSession->AddFolderListener(this, nsIFolderListener::boolPropertyChanged | nsIFolderListener::intPropertyChanged);
  NS_ENSURE_SUCCESS(rv,rv);

  // get current profile name to fill in commandliner. 
  nsCOMPtr<nsIProfile> profileService = 
    do_GetService(NS_PROFILE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv,rv);

  profileService->GetCurrentProfile(getter_Copies(mProfileName));

  // get application path 
  char appPath[_MAX_PATH] = {0};
  GetModuleFileName(nsnull, appPath, sizeof(appPath));
  WORD wideFormatAppPath[_MAX_PATH*2] = {0};
  MultiByteToWideChar(CP_ACP, 0, appPath, nsCRT::strlen(appPath), wideFormatAppPath, _MAX_PATH*2);
  mAppName.Assign((PRUnichar *)wideFormatAppPath);

  rv = ResetCurrent();
  NS_ENSURE_SUCCESS(rv,rv);

  rv = SetupUnreadCountUpdateTimer();
  NS_ENSURE_SUCCESS(rv,rv);

  return NS_OK;
}

NS_IMETHODIMP
nsMessengerWinIntegration::OnItemPropertyChanged(nsISupports *, nsIAtom *, char const *, char const *)
{
  return NS_OK;
}

NS_IMETHODIMP
nsMessengerWinIntegration::OnItemUnicharPropertyChanged(nsISupports *, nsIAtom *, const PRUnichar *, const PRUnichar *)
{
  return NS_OK;
}

NS_IMETHODIMP
nsMessengerWinIntegration::OnItemRemoved(nsISupports *, nsISupports *, const char *)
{
  return NS_OK;
}

NS_IMETHODIMP
nsMessengerWinIntegration::OnItemPropertyFlagChanged(nsISupports *, nsIAtom *, PRUint32, PRUint32)
{
  return NS_OK;
}

NS_IMETHODIMP
nsMessengerWinIntegration::OnItemAdded(nsISupports *, nsISupports *, const char *)
{
  return NS_OK;
}


NS_IMETHODIMP
nsMessengerWinIntegration::OnItemBoolPropertyChanged(nsISupports *aItem,
                                                         nsIAtom *aProperty,
                                                         PRBool aOldValue,
                                                         PRBool aNewValue)
{
  if (aProperty == mDefaultServerAtom) {
    nsresult rv;

    // this property changes multiple times
    // on account deletion or when the user changes their
    // default account.  ResetCurrent() will set
    // mInboxURI to null, so we use that
    // to prevent us from attempting to remove 
    // something from the registry that has already been removed
    if (mInboxURI && mEmail) {
      rv = RemoveCurrentFromRegistry();
      NS_ENSURE_SUCCESS(rv,rv);
    }

    // reset so we'll go get the new default server next time
    rv = ResetCurrent();
    NS_ENSURE_SUCCESS(rv,rv);

    rv = UpdateUnreadCount();
    NS_ENSURE_SUCCESS(rv,rv);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsMessengerWinIntegration::OnItemEvent(nsIFolder *, nsIAtom *)
{
  return NS_OK;
}

NS_IMETHODIMP
nsMessengerWinIntegration::OnItemIntPropertyChanged(nsISupports *aItem, nsIAtom *aProperty, PRInt32 aOldValue, PRInt32 aNewValue)
{
  nsresult rv;

  if (aProperty == mTotalUnreadMessagesAtom) {
    nsCOMPtr <nsIRDFResource> folderResource = do_QueryInterface(aItem, &rv);
    NS_ENSURE_SUCCESS(rv,rv);

    const char *itemURI = nsnull;
    rv = folderResource->GetValueConst(&itemURI);
    NS_ENSURE_SUCCESS(rv,rv);

    // Today, if the user brings up the mail app and gets his/her mail and read 
    // couple of messages and quit the app before the first timer action is 
    // triggered, app will not get the opportunity to update the registry 
    // latest unread count thus displaying essentially the previous sessions's data. 
    // That is not desirable. So, we can avoid that situation by updating the 
    // registry first time the total unread count is changed. That will update 
    // the registry to reflect the first user action that alters unread mail count.
    // As the user reads some of the messages, the latest unread mail count 
    // is kept track of. Now, if the user quits before the first timer is triggered,
    // the registry is updated one last time via UpdateRegistryWithCurrent() as we will
    // have all information needed available to do so.
    if (mFirstTimeFolderUnreadCountChanged) {
      mFirstTimeFolderUnreadCountChanged = PR_FALSE;

      rv = UpdateUnreadCount();
      NS_ENSURE_SUCCESS(rv,rv);
    }

    if (itemURI && mInboxURI && !nsCRT::strcmp(itemURI, mInboxURI)) {
      mCurrentUnreadCount = aNewValue;
    }
  }
  return NS_OK;
}

void 
nsMessengerWinIntegration::OnUnreadCountUpdateTimer(nsITimer *timer, void *osIntegration)
{
  nsMessengerWinIntegration *winIntegration = (nsMessengerWinIntegration*)osIntegration;

  nsresult rv = winIntegration->UpdateUnreadCount();
  NS_ASSERTION(NS_SUCCEEDED(rv), "updating unread count failed");
}

nsresult
nsMessengerWinIntegration::RemoveCurrentFromRegistry()
{
  // If Windows XP, open the registry and get rid of old account registry entries
  // If there is a email prefix, get it and use it to build the registry key.
  // Otherwise, just the email address will be the registry key.
  nsAutoString currentUnreadMailCountKey;
  if (!mEmailPrefix.IsEmpty()) {
    currentUnreadMailCountKey.Assign(mEmailPrefix);
    currentUnreadMailCountKey.AppendWithConversion(mEmail);
  }
  else {
    currentUnreadMailCountKey.AssignWithConversion(mEmail);
  }


  WCHAR registryUnreadMailCountKey[_MAX_PATH] = {0};
  // Enumerate through registry entries to delete the key matching 
  // currentUnreadMailCountKey
  int index = 0;
  while (SUCCEEDED(mSHEnumerateUnreadMailAccounts(HKEY_CURRENT_USER, 
                                                  index, 
                                                  registryUnreadMailCountKey, 
                                                  sizeof(registryUnreadMailCountKey))))
  {
    if (wcscmp(registryUnreadMailCountKey, currentUnreadMailCountKey.get())==0) {
      nsAutoString deleteKey;
      deleteKey.Assign(NS_LITERAL_STRING(UNREADMAILNODEKEY).get());
      deleteKey.Append(currentUnreadMailCountKey.get());

      if (!deleteKey.IsEmpty()) {
        // delete this key and berak out of the loop
        RegDeleteKey(HKEY_CURRENT_USER, 
                     NS_ConvertUCS2toUTF8(deleteKey).get());
        break;
      }
      else {
        index++;
      }
    }
    else {
      index++;
    }
  }
  return NS_OK;
}

nsresult 
nsMessengerWinIntegration::UpdateRegistryWithCurrent()
{
  if (!mInboxURI || !mEmail) 
    return NS_OK;

  // only update the registry if the count has changed
  // and if the unread count is valid
  if ((mCurrentUnreadCount < 0) || (mCurrentUnreadCount == mLastUnreadCountWrittenToRegistry)) {
    return NS_OK;
  }

  // Commadliner has to be built in the form of statement
  // which can be open the mailer app to the default user account
  // For given profile 'foo', commandliner will be built as 
  // ""<absolute path to application>" -p foo -mail" where absolute
  // path to application is extracted from mAppName
  nsAutoString commandLinerForAppLaunch;
  commandLinerForAppLaunch.Assign(NS_LITERAL_STRING(DOUBLE_QUOTE).get());
  commandLinerForAppLaunch.Append(mAppName.get());
  commandLinerForAppLaunch.Append(NS_LITERAL_STRING(DOUBLE_QUOTE).get());
  commandLinerForAppLaunch.Append(NS_LITERAL_STRING(PROFILE_COMMANDLINE_ARG).get());
  commandLinerForAppLaunch.Append(mProfileName.get());
  commandLinerForAppLaunch.Append(NS_LITERAL_STRING(MAIL_COMMANDLINE_ARG).get());

  if (!commandLinerForAppLaunch.IsEmpty())
  {
    nsAutoString pBuffer;

    if (!mEmailPrefix.IsEmpty()) {
      pBuffer.Assign(mEmailPrefix.get());					
      pBuffer.AppendWithConversion(mEmail);
    }
    else {
      pBuffer.AssignWithConversion(mEmail);
    }

    // Write the info into the registry
    HRESULT hr = mSHSetUnreadMailCount(pBuffer.get(), 
                                       mCurrentUnreadCount, 
                                       commandLinerForAppLaunch.get());
  }

  // do this last
  mLastUnreadCountWrittenToRegistry = mCurrentUnreadCount;

  return NS_OK;
}

nsresult 
nsMessengerWinIntegration::SetupInbox()
{
  nsresult rv;

  // get default account
  nsCOMPtr <nsIMsgAccountManager> accountManager = 
    do_GetService(NS_MSGACCOUNTMANAGER_CONTRACTID, &rv); 
  NS_ENSURE_SUCCESS(rv,rv);

  nsCOMPtr <nsIMsgAccount> account;
  rv = accountManager->GetDefaultAccount(getter_AddRefs(account));
  if (NS_FAILED(rv)) {
    // this can happen if we launch mail on a new profile
    // we don't have a default account yet
    mDefaultAccountMightHaveAnInbox = PR_FALSE;
    return NS_OK;
  }

  // get incoming server
  nsCOMPtr <nsIMsgIncomingServer> server;
  rv = account->GetIncomingServer(getter_AddRefs(server));
  NS_ENSURE_SUCCESS(rv,rv);

  nsXPIDLCString type;
  rv = server->GetType(getter_Copies(type));
  NS_ENSURE_SUCCESS(rv,rv);

  // we only care about imap and pop3
  if (!(nsCRT::strcmp(type.get(), "imap")) ||
      !(nsCRT::strcmp(type.get(), "pop3"))) {
    // imap and pop3 account should have an Inbox
    mDefaultAccountMightHaveAnInbox = PR_TRUE;

    // get the redirector type, use it to get the prefix
    // todo remove this extra copy
    nsXPIDLCString redirectorType;
    server->GetRedirectorType(getter_Copies(redirectorType));

    if (redirectorType) {
      nsCAutoString providerPrefixPref;
      providerPrefixPref.Assign("mail.");
      providerPrefixPref.Append(redirectorType);
      providerPrefixPref.Append(".unreadMailCountRegistryKeyPrefix");

      // get pref service
      nsCOMPtr<nsIPref> prefService;
      prefService = do_GetService(NS_PREF_CONTRACTID, &rv);
      NS_ENSURE_SUCCESS(rv,rv);

      nsXPIDLString prefixValue;
      prefService->CopyUnicharPref(providerPrefixPref.get(), getter_Copies(prefixValue));
      if (prefixValue.get())
        mEmailPrefix.Assign(prefixValue);
      else
        mEmailPrefix.Truncate(0);
    }
    else {
        mEmailPrefix.Truncate(0);
    }


    // Get user's email address
    nsCOMPtr<nsIMsgIdentity> identity;
    rv = account->GetDefaultIdentity(getter_AddRefs(identity));
    NS_ENSURE_SUCCESS(rv,rv);
 
    if (!identity)
      return NS_ERROR_FAILURE;
 
    rv = identity->GetEmail(&mEmail);
    NS_ENSURE_SUCCESS(rv,rv);
    
    nsCOMPtr<nsIFolder> rootFolder;
    rv = server->GetRootFolder(getter_AddRefs(rootFolder));
    nsCOMPtr<nsIMsgFolder> rootMsgFolder = do_QueryInterface(rootFolder, &rv);;
    NS_ENSURE_SUCCESS(rv,rv);
 
    if (!rootMsgFolder)
      return NS_ERROR_FAILURE;
 
    PRUint32 numFolders = 0;
    nsCOMPtr <nsIMsgFolder> inboxFolder;
    rv = rootMsgFolder->GetFoldersWithFlag(MSG_FOLDER_FLAG_INBOX, 1, &numFolders, getter_AddRefs(inboxFolder));
    NS_ENSURE_SUCCESS(rv,rv);
 
    if (!inboxFolder)
     return NS_ERROR_FAILURE;
 
    rv = inboxFolder->GetURI(&mInboxURI);
    NS_ENSURE_SUCCESS(rv,rv);

    rv = inboxFolder->GetNumUnread(PR_FALSE, &mCurrentUnreadCount);
    NS_ENSURE_SUCCESS(rv,rv);
  }
  else {
    // the default account is valid, but it's not something 
    // that we expect to have an inbox.  (local folders, news accounts)
    // set this flag to avoid calling SetupInbox() every time
    // the timer goes off.
    mDefaultAccountMightHaveAnInbox = PR_FALSE;
  }

  return NS_OK;
}

nsresult
nsMessengerWinIntegration::UpdateUnreadCount()
{
  nsresult rv;

  if (mDefaultAccountMightHaveAnInbox && !mInboxURI) {
    rv = SetupInbox();
    NS_ENSURE_SUCCESS(rv,rv);
  }
  
  rv = UpdateRegistryWithCurrent();
  NS_ENSURE_SUCCESS(rv,rv);

  return NS_OK;
}

nsresult
nsMessengerWinIntegration::SetupUnreadCountUpdateTimer()
{
  PRUint32 timeInMSUint32 = (PRUint32) mIntervalTime;
  if (mUnreadCountUpdateTimer) {
    mUnreadCountUpdateTimer->Cancel();
  }

  mUnreadCountUpdateTimer = do_CreateInstance("@mozilla.org/timer;1");
  mUnreadCountUpdateTimer->Init(OnUnreadCountUpdateTimer, (void*)this, timeInMSUint32, NS_PRIORITY_NORMAL, NS_TYPE_REPEATING_SLACK);

  return NS_OK;
}
