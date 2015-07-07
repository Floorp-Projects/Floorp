/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This is the principal that has no rights and can't be accessed by
 * anything other than itself and chrome; null principals are not
 * same-origin with anything but themselves.
 */

#ifndef nsNullPrincipal_h__
#define nsNullPrincipal_h__

#include "nsIPrincipal.h"
#include "nsJSPrincipals.h"
#include "nsIScriptSecurityManager.h"
#include "nsCOMPtr.h"
#include "nsIContentSecurityPolicy.h"

#include "mozilla/BasePrincipal.h"

class nsIURI;

#define NS_NULLPRINCIPAL_CID \
{ 0x34a19ab6, 0xca47, 0x4098, \
  { 0xa7, 0xb8, 0x4a, 0xfc, 0xdd, 0xcd, 0x8f, 0x88 } }
#define NS_NULLPRINCIPAL_CONTRACTID "@mozilla.org/nullprincipal;1"

#define NS_NULLPRINCIPAL_SCHEME "moz-nullprincipal"

class nsNullPrincipal final : public mozilla::BasePrincipal
{
public:
  // This should only be used by deserialization, and the factory constructor.
  // Other consumers should use the Create and CreateWithInheritedAttributes
  // methods.
  nsNullPrincipal() {}

  NS_DECL_NSISERIALIZABLE

  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr) override;
  NS_IMETHOD GetHashValue(uint32_t* aHashValue) override;
  NS_IMETHOD GetURI(nsIURI** aURI) override;
  NS_IMETHOD GetDomain(nsIURI** aDomain) override;
  NS_IMETHOD SetDomain(nsIURI* aDomain) override;
  NS_IMETHOD CheckMayLoad(nsIURI* uri, bool report, bool allowIfInheritsPrincipal) override;
  NS_IMETHOD GetIsNullPrincipal(bool* aIsNullPrincipal) override;
  NS_IMETHOD GetBaseDomain(nsACString& aBaseDomain) override;
  nsresult GetOriginInternal(nsACString& aOrigin) override;

  // Returns null on failure.
  static already_AddRefed<nsNullPrincipal> CreateWithInheritedAttributes(nsIPrincipal *aInheritFrom);

  // Returns null on failure.
  static already_AddRefed<nsNullPrincipal>
    Create(const mozilla::OriginAttributes& aOriginAttributes = mozilla::OriginAttributes());

  nsresult Init(const mozilla::OriginAttributes& aOriginAttributes = mozilla::OriginAttributes());

  virtual void GetScriptLocation(nsACString &aStr) override;

 protected:
  virtual ~nsNullPrincipal() {}

  bool SubsumesInternal(nsIPrincipal* aOther, DocumentDomainConsideration aConsideration) override
  {
    return aOther == this;
  }

  nsCOMPtr<nsIURI> mURI;
  nsCOMPtr<nsIContentSecurityPolicy> mCSP;
};

#endif // nsNullPrincipal_h__
