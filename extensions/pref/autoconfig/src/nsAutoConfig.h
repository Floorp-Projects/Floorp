/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsAutoConfig_h
#define nsAutoConfig_h

#include "nsITimer.h"
#include "nsIFile.h"
#include "nsINamed.h"
#include "nsIObserver.h"
#include "nsIStreamListener.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsWeakReference.h"
#include "nsString.h"

class nsAutoConfig final : public nsITimerCallback,
                           public nsIStreamListener,
                           public nsIObserver,
                           public nsSupportsWeakReference,
                           public nsINamed

{
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSIOBSERVER
  NS_DECL_NSITIMERCALLBACK
  NS_DECL_NSINAMED

  nsAutoConfig();
  nsresult Init();

  void SetConfigURL(const char* aConfigURL);

 protected:
  virtual ~nsAutoConfig();
  nsresult downloadAutoConfig();
  nsresult readOfflineFile();
  nsresult evaluateLocalFile(nsIFile* file);
  nsresult writeFailoverFile();
  nsresult getEmailAddr(nsACString& emailAddr);
  nsresult PromptForEMailAddress(nsACString& emailAddress);
  nsCString mBuf;
  nsCOMPtr<nsIPrefBranch> mPrefBranch;
  bool mLoaded;
  nsCOMPtr<nsITimer> mTimer;
  nsCString mConfigURL;
};

#endif
