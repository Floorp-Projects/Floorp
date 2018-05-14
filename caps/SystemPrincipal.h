/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* The privileged system principal. */

#ifndef SystemPrincipal_h
#define SystemPrincipal_h

#include "nsIPrincipal.h"
#include "nsJSPrincipals.h"

#include "mozilla/BasePrincipal.h"

#define NS_SYSTEMPRINCIPAL_CID \
{ 0x4a6212db, 0xaccb, 0x11d3, \
{ 0xb7, 0x65, 0x0, 0x60, 0xb0, 0xb6, 0xce, 0xcb }}
#define NS_SYSTEMPRINCIPAL_CONTRACTID "@mozilla.org/systemprincipal;1"


class SystemPrincipal final : public mozilla::BasePrincipal
{
  SystemPrincipal()
    : BasePrincipal(eSystemPrincipal)
  {
  }

public:
  static already_AddRefed<SystemPrincipal> Create();

  static PrincipalKind Kind() { return eSystemPrincipal; }

  NS_DECL_NSISERIALIZABLE
  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr) override;
  NS_IMETHOD GetHashValue(uint32_t* aHashValue) override;
  NS_IMETHOD GetURI(nsIURI** aURI) override;
  NS_IMETHOD GetDomain(nsIURI** aDomain) override;
  NS_IMETHOD SetDomain(nsIURI* aDomain) override;
  NS_IMETHOD GetCsp(nsIContentSecurityPolicy** aCsp) override;
  NS_IMETHOD SetCsp(nsIContentSecurityPolicy* aCsp) override;
  NS_IMETHOD EnsureCSP(nsIDocument* aDocument, nsIContentSecurityPolicy** aCSP) override;
  NS_IMETHOD GetPreloadCsp(nsIContentSecurityPolicy** aPreloadCSP) override;
  NS_IMETHOD EnsurePreloadCSP(nsIDocument* aDocument, nsIContentSecurityPolicy** aCSP) override;
  NS_IMETHOD GetBaseDomain(nsACString& aBaseDomain) override;
  NS_IMETHOD GetAddonId(nsAString& aAddonId) override;

  virtual nsresult GetScriptLocation(nsACString &aStr) override;

protected:
  virtual ~SystemPrincipal(void) {}

  bool SubsumesInternal(nsIPrincipal *aOther,
                        DocumentDomainConsideration aConsideration) override
  {
    return true;
  }

  bool MayLoadInternal(nsIURI* aURI) override
  {
    return true;
  }
};

#endif // SystemPrincipal_h
