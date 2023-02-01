/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* The privileged system principal. */

#include "nscore.h"
#include "SystemPrincipal.h"
#include "mozilla/ClearOnShutdown.h"
#include "nsCOMPtr.h"
#include "nsReadableUtils.h"
#include "nsCRT.h"
#include "nsString.h"
#include "nsIClassInfoImpl.h"
#include "pratom.h"

using namespace mozilla;

NS_IMPL_CLASSINFO(SystemPrincipal, nullptr, nsIClassInfo::SINGLETON,
                  NS_SYSTEMPRINCIPAL_CID)
NS_IMPL_QUERY_INTERFACE_CI(SystemPrincipal, nsIPrincipal, nsISerializable)
NS_IMPL_CI_INTERFACE_GETTER(SystemPrincipal, nsIPrincipal, nsISerializable)

static constexpr nsLiteralCString kSystemPrincipalSpec =
    "[System Principal]"_ns;

SystemPrincipal::SystemPrincipal()
    : BasePrincipal(eSystemPrincipal, kSystemPrincipalSpec,
                    OriginAttributes()) {}

static StaticMutex sSystemPrincipalMutex;
static StaticRefPtr<SystemPrincipal> sSystemPrincipal
    MOZ_GUARDED_BY(sSystemPrincipalMutex);

already_AddRefed<SystemPrincipal> SystemPrincipal::Get() {
  StaticMutexAutoLock lock(sSystemPrincipalMutex);
  return do_AddRef(sSystemPrincipal);
}

already_AddRefed<SystemPrincipal> SystemPrincipal::Init() {
  AssertIsOnMainThread();
  StaticMutexAutoLock lock(sSystemPrincipalMutex);
  if (MOZ_UNLIKELY(sSystemPrincipal)) {
    MOZ_ASSERT_UNREACHABLE("SystemPrincipal::Init() may only be called once");
  } else {
    sSystemPrincipal = new SystemPrincipal();
  }
  return do_AddRef(sSystemPrincipal);
}

void SystemPrincipal::Shutdown() {
  AssertIsOnMainThread();
  StaticMutexAutoLock lock(sSystemPrincipalMutex);
  MOZ_ASSERT(sSystemPrincipal);
  sSystemPrincipal = nullptr;
}

nsresult SystemPrincipal::GetScriptLocation(nsACString& aStr) {
  aStr.Assign(kSystemPrincipalSpec);
  return NS_OK;
}

///////////////////////////////////////
// Methods implementing nsIPrincipal //
///////////////////////////////////////

uint32_t SystemPrincipal::GetHashValue() { return NS_PTR_TO_INT32(this); }

NS_IMETHODIMP
SystemPrincipal::GetURI(nsIURI** aURI) {
  *aURI = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
SystemPrincipal::GetIsOriginPotentiallyTrustworthy(bool* aResult) {
  *aResult = true;
  return NS_OK;
}

NS_IMETHODIMP
SystemPrincipal::GetDomain(nsIURI** aDomain) {
  *aDomain = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
SystemPrincipal::SetDomain(nsIURI* aDomain) { return NS_OK; }

NS_IMETHODIMP
SystemPrincipal::GetBaseDomain(nsACString& aBaseDomain) {
  // No base domain for chrome.
  return NS_OK;
}

NS_IMETHODIMP
SystemPrincipal::GetAddonId(nsAString& aAddonId) {
  aAddonId.Truncate();
  return NS_OK;
};

//////////////////////////////////////////
// Methods implementing nsISerializable //
//////////////////////////////////////////

NS_IMETHODIMP
SystemPrincipal::Read(nsIObjectInputStream* aStream) {
  // no-op: CID is sufficient to identify the mSystemPrincipal singleton
  return NS_OK;
}

NS_IMETHODIMP
SystemPrincipal::Write(nsIObjectOutputStream* aStream) {
  // Read is used still for legacy principals
  MOZ_RELEASE_ASSERT(false, "Old style serialization is removed");
  return NS_OK;
}
