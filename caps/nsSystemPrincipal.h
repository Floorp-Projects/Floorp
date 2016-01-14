/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* The privileged system principal. */

#ifndef nsSystemPrincipal_h__
#define nsSystemPrincipal_h__

#include "nsIPrincipal.h"
#include "nsJSPrincipals.h"

#include "mozilla/BasePrincipal.h"

#define NS_SYSTEMPRINCIPAL_CID \
{ 0x4a6212db, 0xaccb, 0x11d3, \
{ 0xb7, 0x65, 0x0, 0x60, 0xb0, 0xb6, 0xce, 0xcb }}
#define NS_SYSTEMPRINCIPAL_CONTRACTID "@mozilla.org/systemprincipal;1"


class nsSystemPrincipal final : public mozilla::BasePrincipal
{
public:
  NS_DECL_NSISERIALIZABLE
  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr) override;
  NS_IMETHOD GetHashValue(uint32_t* aHashValue) override;
  NS_IMETHOD GetURI(nsIURI** aURI) override;
  NS_IMETHOD GetDomain(nsIURI** aDomain) override;
  NS_IMETHOD SetDomain(nsIURI* aDomain) override;
  NS_IMETHOD GetCsp(nsIContentSecurityPolicy** aCsp) override;
  NS_IMETHOD EnsureCSP(nsIDOMDocument* aDocument, nsIContentSecurityPolicy** aCSP) override;
  NS_IMETHOD GetPreloadCsp(nsIContentSecurityPolicy** aPreloadCSP) override;
  NS_IMETHOD EnsurePreloadCSP(nsIDOMDocument* aDocument, nsIContentSecurityPolicy** aCSP) override;
  NS_IMETHOD GetBaseDomain(nsACString& aBaseDomain) override;
  nsresult GetOriginInternal(nsACString& aOrigin) override;

  nsSystemPrincipal() {}

  virtual void GetScriptLocation(nsACString &aStr) override;

protected:
  virtual ~nsSystemPrincipal(void) {}

  bool SubsumesInternal(nsIPrincipal *aOther, DocumentDomainConsideration aConsideration) override
  {
    return true;
  }

  bool MayLoadInternal(nsIURI* aURI) override
  {
    return true;
  }

  PrincipalKind Kind() override { return eSystemPrincipal; }
};

#endif // nsSystemPrincipal_h__
