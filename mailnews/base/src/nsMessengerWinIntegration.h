/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef __nsMessengerWinIntegration_h
#define __nsMessengerWinIntegration_h

#include <windows.h>

#include "nsIMessengerOSIntegration.h"
#include "nsIFolderListener.h"
#include "nsIAtom.h"
#include "nsITimer.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsIPref.h"
#include "nsInt64.h"

// this function is exported by shell32.dll version 5.60 or later (Windows XP or greater)
extern "C"
{
typedef HRESULT (__stdcall *fnSHSetUnreadMailCount)(LPCWSTR pszMailAddress, DWORD dwCount, LPCWSTR pszShellExecuteCommand);
typedef HRESULT (__stdcall *fnSHEnumerateUnreadMailAccounts)(HKEY hKeyUser, DWORD dwIndex, LPCWSTR pszMailAddress, int cchMailAddress);
}

#define NS_MESSENGERWININTEGRATION_CID \
  {0xf62f3d3a, 0x1dd1, 0x11b2, \
    {0xa5, 0x16, 0xef, 0xad, 0xb1, 0x31, 0x61, 0x5c}}

class nsMessengerWinIntegration : public nsIMessengerOSIntegration,
                                  public nsIFolderListener
{
public:
  nsMessengerWinIntegration();
  virtual ~nsMessengerWinIntegration();
  virtual nsresult Init();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIMESSENGEROSINTEGRATION
  NS_DECL_NSIFOLDERLISTENER

private:
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
  nsXPIDLString mProfileName;  
  nsCString mShellDllPath;

  PRInt32   mCurrentUnreadCount;
  PRInt32   mLastUnreadCountWrittenToRegistry;

  nsInt64   mIntervalTime;
 
  // "might" because we don't know until we check 
  // what type of server is associated with the default account
  PRBool    mDefaultAccountMightHaveAnInbox;

  // first time the unread count changes, we need to update registry
  PRBool mFirstTimeFolderUnreadCountChanged;
};

#endif // __nsMessengerWinIntegration_h
