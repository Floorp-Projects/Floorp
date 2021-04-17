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
#include "mozilla/StaticPrefs_network.h"
#include "mozilla/dom/StorageUtils.h"
#include "mozilla/ipc/PBackgroundSharedTypes.h"
#include "mozilla/net/MozURL.h"
#include "mozilla/net/WebSocketFrame.h"
#include "nsDebug.h"
#include "nsError.h"
#include "nsICookieService.h"
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

void MaybeEnableNextGenLocalStorage() {
  if (StaticPrefs::dom_storage_next_gen_DoNotUseDirectly()) {
    return;
  }

  if (!Preferences::GetBool("dom.storage.next_gen_auto_enabled_by_cause1")) {
    if (StaticPrefs::network_cookie_lifetimePolicy() ==
        nsICookieService::ACCEPT_SESSION) {
      Preferences::SetBool("dom.storage.next_gen", true);
      Preferences::SetBool("dom.storage.next_gen_auto_enabled_by_cause1", true);
    }
  }
}

bool NextGenLocalStorageEnabled() {
  if (XRE_IsParentProcess()) {
    StaticMutexAutoLock lock(gNextGenLocalStorageMutex);

    if (gNextGenLocalStorageEnabled == -1) {
      // Ideally all this Mutex stuff would be replaced with just using
      // an AtStartup StaticPref, but there are concerns about this causing
      // deadlocks if this access needs to init the AtStartup cache.
      bool enabled = StaticPrefs::dom_storage_next_gen_DoNotUseDirectly();
      gNextGenLocalStorageEnabled = enabled ? 1 : 0;
    }

    return !!gNextGenLocalStorageEnabled;
  }

  MOZ_ASSERT(NS_IsMainThread());

  if (gNextGenLocalStorageEnabled == -1) {
    bool enabled = Preferences::GetBool("dom.storage.next_gen", false);
    gNextGenLocalStorageEnabled = enabled ? 1 : 0;
  }

  return !!gNextGenLocalStorageEnabled;
}

bool CachedNextGenLocalStorageEnabled() {
  MOZ_ASSERT(gNextGenLocalStorageEnabled != -1);

  return !!gNextGenLocalStorageEnabled;
}

Result<std::pair<nsCString, nsCString>, nsresult> GenerateOriginKey2(
    const mozilla::ipc::PrincipalInfo& aPrincipalInfo) {
  OriginAttributes attrs;
  nsCString spec;

  switch (aPrincipalInfo.type()) {
    case mozilla::ipc::PrincipalInfo::TNullPrincipalInfo: {
      const auto& info = aPrincipalInfo.get_NullPrincipalInfo();

      attrs = info.attrs();
      spec = info.spec();

      break;
    }

    case mozilla::ipc::PrincipalInfo::TContentPrincipalInfo: {
      const auto& info = aPrincipalInfo.get_ContentPrincipalInfo();

      attrs = info.attrs();
      spec = info.spec();

      break;
    }

    default: {
      spec.SetIsVoid(true);

      break;
    }
  }

  if (spec.IsVoid()) {
    return Err(NS_ERROR_UNEXPECTED);
  }

  nsCString originAttrSuffix;
  attrs.CreateSuffix(originAttrSuffix);

  RefPtr<MozURL> specURL;
  LS_TRY(MozURL::Init(getter_AddRefs(specURL), spec));

  nsCString host(specURL->Host());
  uint32_t length = host.Length();
  if (length > 0 && host.CharAt(0) == '[' && host.CharAt(length - 1) == ']') {
    host = Substring(host, 1, length - 2);
  }

  nsCString domainOrigin(host);

  if (domainOrigin.IsEmpty()) {
    // For the file:/// protocol use the exact directory as domain.
    if (specURL->Scheme().EqualsLiteral("file")) {
      domainOrigin.Assign(specURL->Directory());
    }
  }

  // Append reversed domain
  nsAutoCString reverseDomain;
  nsresult rv = StorageUtils::CreateReversedDomain(domainOrigin, reverseDomain);
  if (NS_FAILED(rv)) {
    return Err(rv);
  }

  nsCString originKey = reverseDomain;

  // Append scheme
  originKey.Append(':');
  originKey.Append(specURL->Scheme());

  // Append port if any
  int32_t port = specURL->RealPort();
  if (port != -1) {
    originKey.AppendPrintf(":%d", port);
  }

  return std::make_pair(std::move(originAttrSuffix), std::move(originKey));
}

LogModule* GetLocalStorageLogger() { return gLogger; }

}  // namespace mozilla::dom
