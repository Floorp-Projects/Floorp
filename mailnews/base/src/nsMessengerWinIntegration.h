/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 *   Bhuvan Racham <racham@netscape.com>
 *   Scott MacGregor <mscott@netscape.com>
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

#ifndef __nsMessengerWinIntegration_h
#define __nsMessengerWinIntegration_h

#include <windows.h>

// shellapi.h is needed to build with WIN32_LEAN_AND_MEAN
#include <shellapi.h>

#include "nsIMessengerOSIntegration.h"
#include "nsIFolderListener.h"
#include "nsIAlertsService.h"
#include "nsIAtom.h"
#include "nsITimer.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsIPref.h"
#include "nsInt64.h"
#include "nsISupportsArray.h"

// this function is exported by shell32.dll version 5.60 or later (Windows XP or greater)
extern "C"
{
typedef HRESULT (__stdcall *fnSHSetUnreadMailCount)(LPCWSTR pszMailAddress, DWORD dwCount, LPCWSTR pszShellExecuteCommand);
typedef HRESULT (__stdcall *fnSHEnumerateUnreadMailAccounts)(HKEY hKeyUser, DWORD dwIndex, LPCWSTR pszMailAddress, int cchMailAddress);
typedef BOOL (_stdcall *fnShellNotifyW)(DWORD dwMessage, PNOTIFYICONDATAW lpdata);
}

#define NS_MESSENGERWININTEGRATION_CID \
  {0xf62f3d3a, 0x1dd1, 0x11b2, \
    {0xa5, 0x16, 0xef, 0xad, 0xb1, 0x31, 0x61, 0x5c}}

class nsIStringBundle; 

class nsMessengerWinIntegration : public nsIMessengerOSIntegration,
                                  public nsIFolderListener,
                                  public nsIAlertListener
{
public:
  nsMessengerWinIntegration();
  virtual ~nsMessengerWinIntegration();
  virtual nsresult Init();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIMESSENGEROSINTEGRATION
  NS_DECL_NSIFOLDERLISTENER
  NS_DECL_NSIALERTLISTENER

private:
  
  static NOTIFYICONDATAW mWideBiffIconData; 
  static NOTIFYICONDATA  mAsciiBiffIconData;

  void InitializeBiffStatusIcon(); 
  void FillToolTipInfo();
  void GenericShellNotify(DWORD aMessage);
  void SetToolTipStringOnIconData(const PRUnichar * aToolTipString);
  void DestroyBiffIcon();
  void RevertToNonUnicodeShellAPI();

  PRUint32 GetToolTipSize(); // available space for the tooltip string
  nsresult ShowAlertMessage(const PRUnichar * aAlertTitle, const PRUnichar * aAlertText, const char * aFolderURI);
  nsresult GetFirstFolderWithNewMail(char ** aFolderURI);

  nsresult GetStringBundle(nsIStringBundle **aBundle);
  nsCOMPtr<nsISupportsArray> mFoldersWithNewMail;  // keep track of all the root folders with pending new mail
  fnShellNotifyW mShellNotifyWideChar;
  nsCOMPtr<nsIAtom> mBiffStateAtom;
  PRUint32 mCurrentBiffState;

  PRPackedBool mUseWideCharBiffIcon; 
  PRPackedBool mStoreUnreadCounts;   // for windows XP, we do a lot of work to store the last unread count for the inbox
                                     // this flag is set to true when we are doing that
  PRPackedBool mBiffIconVisible;
  PRPackedBool mBiffIconInitialized;
  PRPackedBool mSuppressBiffIcon;
  PRPackedBool mAlertInProgress;
  
  // "might" because we don't know until we check 
  // what type of server is associated with the default account
  PRPackedBool    mDefaultAccountMightHaveAnInbox;

  // first time the unread count changes, we need to update registry
  PRPackedBool mFirstTimeFolderUnreadCountChanged;

  nsresult ResetCurrent();
  nsresult RemoveCurrentFromRegistry();
  nsresult UpdateRegistryWithCurrent();
  nsresult SetupInbox();

  nsresult SetupUnreadCountUpdateTimer();
  static void OnUnreadCountUpdateTimer(nsITimer *timer, void *osIntegration);
  nsresult UpdateUnreadCount();

  nsCOMPtr <nsIAtom> mDefaultServerAtom;
  nsCOMPtr <nsIAtom> mTotalUnreadMessagesAtom;
  nsCOMPtr <nsITimer> mUnreadCountUpdateTimer;

  fnSHSetUnreadMailCount mSHSetUnreadMailCount;
  fnSHEnumerateUnreadMailAccounts mSHEnumerateUnreadMailAccounts;

  char *mInboxURI;
  char *mEmail;

  nsAutoString mAppName;      
  nsAutoString mEmailPrefix;  
  nsCString mShellDllPath;

#ifdef MOZ_THUNDERBIRD
  nsString mProfilePath;
#else
  nsXPIDLString mProfileName;
#endif


  PRInt32   mCurrentUnreadCount;
  PRInt32   mLastUnreadCountWrittenToRegistry;

  nsInt64   mIntervalTime;
};

#endif // __nsMessengerWinIntegration_h
