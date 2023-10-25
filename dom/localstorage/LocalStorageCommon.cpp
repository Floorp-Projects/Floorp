/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LocalStorageCommon.h"

#include <cstdint>
#include "MainThreadUtils.h"
#include "mozilla/Assertions.h"
#include "mozilla/Atomics.h"
#include "mozilla/Logging.h"
#include "mozilla/OriginAttributes.h"
#include "mozilla/Preferences.h"
#include "mozilla/RefPtr.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/dom/StorageUtils.h"
#include "mozilla/dom/quota/ResultExtensions.h"
#include "mozilla/ipc/PBackgroundSharedTypes.h"
#include "mozilla/net/WebSocketFrame.h"
#include "nsDebug.h"
#include "nsError.h"
#include "nsIURL.h"
#include "nsNetUtil.h"
#include "nsPrintfCString.h"
#include "nsString.h"
#include "nsStringFlags.h"
#include "nsXULAppAPI.h"

namespace mozilla::dom {

using namespace mozilla::net;

namespace {

StaticMutex gNextGenLocalStorageMutex;
Atomic<int32_t> gNextGenLocalStorageEnabled(-1);
LazyLogModule gLogger("LocalStorage");

}  // namespace

const char16_t* kLocalStorageType = u"localStorage";

bool NextGenLocalStorageEnabled() {
  if (XRE_IsParentProcess()) {
    StaticMutexAutoLock lock(gNextGenLocalStorageMutex);

    if (gNextGenLocalStorageEnabled == -1) {
      // Ideally all this Mutex stuff would be replaced with just using
      // an AtStartup StaticPref, but there are concerns about this causing
      // deadlocks if this access needs to init the AtStartup cache.
      bool enabled =
          !StaticPrefs::
              dom_storage_enable_unsupported_legacy_implementation_DoNotUseDirectly();

      gNextGenLocalStorageEnabled = enabled ? 1 : 0;
    }

    return !!gNextGenLocalStorageEnabled;
  }

  return CachedNextGenLocalStorageEnabled();
}

void RecvInitNextGenLocalStorageEnabled(const bool aEnabled) {
  MOZ_ASSERT(!XRE_IsParentProcess());
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(gNextGenLocalStorageEnabled == -1);

  gNextGenLocalStorageEnabled = aEnabled ? 1 : 0;
}

bool CachedNextGenLocalStorageEnabled() {
  MOZ_ASSERT(gNextGenLocalStorageEnabled != -1);

  return !!gNextGenLocalStorageEnabled;
}

Result<std::pair<nsCString, nsCString>, nsresult> GenerateOriginKey2(
    const mozilla::ipc::PrincipalInfo& aPrincipalInfo) {
  if (aPrincipalInfo.type() !=
          mozilla::ipc::PrincipalInfo::TNullPrincipalInfo &&
      aPrincipalInfo.type() !=
          mozilla::ipc::PrincipalInfo::TContentPrincipalInfo) {
    return Err(NS_ERROR_UNEXPECTED);
  }

  Result<nsCOMPtr<nsIPrincipal>, nsresult> p =
      PrincipalInfoToPrincipal(aPrincipalInfo);
  if (p.isErr()) {
    return Err(p.unwrapErr());
  }

  nsCOMPtr<nsIPrincipal> principal = p.unwrap();
  if (!principal) {
    return Err(NS_ERROR_NULL_POINTER);
  }

  nsCString originKey;
  nsresult rv = principal->GetStorageOriginKey(originKey);
  if (NS_FAILED(rv)) {
    return Err(rv);
  }

  nsCString originAttrSuffix;
  rv = principal->GetOriginSuffix(originAttrSuffix);
  if (NS_FAILED(rv)) {
    return Err(rv);
  }

  return std::make_pair(std::move(originAttrSuffix), std::move(originKey));
}

LogModule* GetLocalStorageLogger() { return gLogger; }

}  // namespace mozilla::dom
