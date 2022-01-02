/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* The privileged system principal. */

#ifndef mozilla_SystemPrincipal_h
#define mozilla_SystemPrincipal_h

#include "nsIPrincipal.h"
#include "nsJSPrincipals.h"

#include "mozilla/BasePrincipal.h"

#define NS_SYSTEMPRINCIPAL_CID                      \
  {                                                 \
    0x4a6212db, 0xaccb, 0x11d3, {                   \
      0xb7, 0x65, 0x0, 0x60, 0xb0, 0xb6, 0xce, 0xcb \
    }                                               \
  }
#define NS_SYSTEMPRINCIPAL_CONTRACTID "@mozilla.org/systemprincipal;1"

namespace Json {
class Value;
}

namespace mozilla {

class SystemPrincipal final : public BasePrincipal, public nsISerializable {
  SystemPrincipal();

 public:
  static already_AddRefed<SystemPrincipal> Create();

  static PrincipalKind Kind() { return eSystemPrincipal; }

  NS_IMETHOD_(MozExternalRefCountType) AddRef() override {
    return nsJSPrincipals::AddRef();
  };
  NS_IMETHOD_(MozExternalRefCountType) Release() override {
    return nsJSPrincipals::Release();
  };

  NS_DECL_NSISERIALIZABLE
  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr) override;
  uint32_t GetHashValue() override;
  NS_IMETHOD GetURI(nsIURI** aURI) override;
  NS_IMETHOD GetDomain(nsIURI** aDomain) override;
  NS_IMETHOD SetDomain(nsIURI* aDomain) override;
  NS_IMETHOD GetBaseDomain(nsACString& aBaseDomain) override;
  NS_IMETHOD GetAddonId(nsAString& aAddonId) override;
  NS_IMETHOD GetIsOriginPotentiallyTrustworthy(bool* aResult) override;

  virtual nsresult GetScriptLocation(nsACString& aStr) override;

  nsresult GetSiteIdentifier(SiteIdentifier& aSite) override {
    aSite.Init(this);
    return NS_OK;
  }

 protected:
  virtual ~SystemPrincipal() = default;

  bool SubsumesInternal(nsIPrincipal* aOther,
                        DocumentDomainConsideration aConsideration) override {
    return true;
  }

  bool MayLoadInternal(nsIURI* aURI) override { return true; }
};

}  // namespace mozilla

#endif  // mozilla_SystemPrincipal_h
