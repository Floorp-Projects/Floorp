/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsRefreshTimer_h__
#define nsRefreshTimer_h__

#include "nsINamed.h"
#include "nsITimer.h"

#include "nsCOMPtr.h"

class nsDocShell;
class nsIURI;
class nsIPrincipal;

class nsRefreshTimer : public nsITimerCallback
                     , public nsINamed
{
public:
  nsRefreshTimer(nsDocShell* aDocShell,
                 nsIURI* aURI,
                 nsIPrincipal* aPrincipal,
                 int32_t aDelay,
                 bool aRepeat,
                 bool aMetaRefresh);

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSITIMERCALLBACK
  NS_DECL_NSINAMED

  int32_t GetDelay() { return mDelay ;}

  RefPtr<nsDocShell> mDocShell;
  nsCOMPtr<nsIURI> mURI;
  nsCOMPtr<nsIPrincipal> mPrincipal;
  int32_t mDelay;
  bool mRepeat;
  bool mMetaRefresh;

private:
  virtual ~nsRefreshTimer();
};

#endif /* nsRefreshTimer_h__ */
