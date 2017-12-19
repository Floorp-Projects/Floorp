/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsRefreshTimer.h"

#include "nsIURI.h"
#include "nsIPrincipal.h"

#include "nsDocShell.h"

NS_IMPL_ADDREF(nsRefreshTimer)
NS_IMPL_RELEASE(nsRefreshTimer)

NS_INTERFACE_MAP_BEGIN(nsRefreshTimer)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsITimerCallback)
  NS_INTERFACE_MAP_ENTRY(nsITimerCallback)
  NS_INTERFACE_MAP_ENTRY(nsINamed)
NS_INTERFACE_MAP_END_THREADSAFE

nsRefreshTimer::nsRefreshTimer(nsDocShell* aDocShell,
                               nsIURI* aURI,
                               nsIPrincipal* aPrincipal,
                               int32_t aDelay, bool aRepeat, bool aMetaRefresh)
  : mDocShell(aDocShell), mURI(aURI), mPrincipal(aPrincipal),
    mDelay(aDelay), mRepeat(aRepeat),
    mMetaRefresh(aMetaRefresh)
{
}

nsRefreshTimer::~nsRefreshTimer()
{
}

NS_IMETHODIMP
nsRefreshTimer::Notify(nsITimer* aTimer)
{
  NS_ASSERTION(mDocShell, "DocShell is somehow null");

  if (mDocShell && aTimer) {
    // Get the delay count to determine load type
    uint32_t delay = 0;
    aTimer->GetDelay(&delay);
    mDocShell->ForceRefreshURIFromTimer(mURI, mPrincipal, delay, mMetaRefresh, aTimer);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsRefreshTimer::GetName(nsACString& aName)
{
  aName.AssignLiteral("nsRefreshTimer");
  return NS_OK;
}
