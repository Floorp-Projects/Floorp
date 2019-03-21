/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LocalStorageCommon.h"

#include "mozilla/dom/ContentChild.h"
#include "mozilla/net/MozURL.h"

namespace mozilla {
namespace dom {

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
      bool enabled = GetCurrentNextGenPrefValue();
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

nsresult GenerateOriginKey2(const PrincipalInfo& aPrincipalInfo,
                            nsACString& aOriginAttrSuffix,
                            nsACString& aOriginKey) {
  OriginAttributes attrs;
  nsCString spec;

  switch (aPrincipalInfo.type()) {
    case PrincipalInfo::TNullPrincipalInfo: {
      const NullPrincipalInfo& info = aPrincipalInfo.get_NullPrincipalInfo();

      attrs = info.attrs();
      spec = info.spec();

      break;
    }

    case PrincipalInfo::TContentPrincipalInfo: {
      const ContentPrincipalInfo& info =
          aPrincipalInfo.get_ContentPrincipalInfo();

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
    return NS_ERROR_UNEXPECTED;
  }

  attrs.CreateSuffix(aOriginAttrSuffix);

  RefPtr<MozURL> specURL;
  nsresult rv = MozURL::Init(getter_AddRefs(specURL), spec);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

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
  rv = CreateReversedDomain(domainOrigin, reverseDomain);
  if (NS_FAILED(rv)) {
    return rv;
  }

  aOriginKey.Append(reverseDomain);

  // Append scheme
  aOriginKey.Append(':');
  aOriginKey.Append(specURL->Scheme());

  // Append port if any
  int32_t port = specURL->RealPort();
  if (port != -1) {
    aOriginKey.Append(nsPrintfCString(":%d", port));
  }

  return NS_OK;
}

LogModule* GetLocalStorageLogger() { return gLogger; }

}  // namespace dom
}  // namespace mozilla
