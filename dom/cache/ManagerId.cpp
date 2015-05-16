/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/cache/ManagerId.h"
#include "nsIPrincipal.h"
#include "nsProxyRelease.h"
#include "nsRefPtr.h"
#include "nsThreadUtils.h"

namespace mozilla {
namespace dom {
namespace cache {

// static
nsresult
ManagerId::Create(nsIPrincipal* aPrincipal, ManagerId** aManagerIdOut)
{
  MOZ_ASSERT(NS_IsMainThread());

  // The QuotaManager::GetInfoFromPrincipal() has special logic for system
  // and about: principals.  We currently don't need the system principal logic
  // because ManagerId only uses the origin for in memory comparisons.  We
  // also don't do any special logic to host the same Cache for different about:
  // pages, so we don't need those checks either.
  //
  // But, if we get the same QuotaManager directory for different about:
  // origins, we probably only want one Manager instance.  So, we might
  // want to start using the QM's concept of origin uniqueness here.
  //
  // TODO: consider using QuotaManager's modified origin here (bug 1112071)

  nsAutoCString origin;
  nsresult rv = aPrincipal->GetOrigin(origin);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  uint32_t appId;
  rv = aPrincipal->GetAppId(&appId);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  bool inBrowserElement;
  rv = aPrincipal->GetIsInBrowserElement(&inBrowserElement);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  nsRefPtr<ManagerId> ref = new ManagerId(aPrincipal, origin, appId,
                                          inBrowserElement);
  ref.forget(aManagerIdOut);

  return NS_OK;
}

already_AddRefed<nsIPrincipal>
ManagerId::Principal() const
{
  MOZ_ASSERT(NS_IsMainThread());
  nsCOMPtr<nsIPrincipal> ref = mPrincipal;
  return ref.forget();
}

ManagerId::ManagerId(nsIPrincipal* aPrincipal, const nsACString& aOrigin,
                     uint32_t aAppId, bool aInBrowserElement)
    : mPrincipal(aPrincipal)
    , mOrigin(aOrigin)
    , mAppId(aAppId)
    , mInBrowserElement(aInBrowserElement)
{
  MOZ_ASSERT(mPrincipal);
}

ManagerId::~ManagerId()
{
  // If we're already on the main thread, then default destruction is fine
  if (NS_IsMainThread()) {
    return;
  }

  // Otherwise we need to proxy to main thread to do the release

  // The PBackground worker thread shouldn't be running after the main thread
  // is stopped.  So main thread is guaranteed to exist here.
  nsCOMPtr<nsIThread> mainThread = do_GetMainThread();
  MOZ_ASSERT(mainThread);

  NS_ProxyRelease(mainThread, mPrincipal.forget().take());
}

} // namespace cache
} // namespace dom
} // namespace mozilla
