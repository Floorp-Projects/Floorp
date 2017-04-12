/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This is the principal that has no rights and can't be accessed by
 * anything other than itself and chrome; null principals are not
 * same-origin with anything but themselves.
 */

#ifndef NullPrincipal_h
#define NullPrincipal_h

#include "nsIPrincipal.h"
#include "nsJSPrincipals.h"
#include "nsIScriptSecurityManager.h"
#include "nsCOMPtr.h"
#include "nsIContentSecurityPolicy.h"

#include "mozilla/BasePrincipal.h"

class nsIDocShell;
class nsIURI;

#define NS_NULLPRINCIPAL_CID \
{ 0xbd066e5f, 0x146f, 0x4472, \
  { 0x83, 0x31, 0x7b, 0xfd, 0x05, 0xb1, 0xed, 0x90 } }
#define NS_NULLPRINCIPAL_CONTRACTID "@mozilla.org/nullprincipal;1"

#define NS_NULLPRINCIPAL_SCHEME "moz-nullprincipal"

class NullPrincipal final : public mozilla::BasePrincipal
{
public:
  // This should only be used by deserialization, and the factory constructor.
  // Other consumers should use the Create and CreateWithInheritedAttributes
  // methods.
  NullPrincipal()
    : BasePrincipal(eNullPrincipal)
  {
  }

  NS_DECL_NSISERIALIZABLE

  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr) override;
  NS_IMETHOD GetHashValue(uint32_t* aHashValue) override;
  NS_IMETHOD SetCsp(nsIContentSecurityPolicy* aCsp) override;
  NS_IMETHOD GetURI(nsIURI** aURI) override;
  NS_IMETHOD GetDomain(nsIURI** aDomain) override;
  NS_IMETHOD SetDomain(nsIURI* aDomain) override;
  NS_IMETHOD GetBaseDomain(nsACString& aBaseDomain) override;
  NS_IMETHOD GetAddonId(nsAString& aAddonId) override;

  static already_AddRefed<NullPrincipal> CreateWithInheritedAttributes(nsIPrincipal* aInheritFrom);

  // Create NullPrincipal with origin attributes from docshell.
  // If aIsFirstParty is true, and the pref 'privacy.firstparty.isolate' is also
  // enabled, the mFirstPartyDomain value of the origin attributes will be set
  // to an unique value.
  static already_AddRefed<NullPrincipal>
  CreateWithInheritedAttributes(nsIDocShell* aDocShell, bool aIsFirstParty = false);

  static already_AddRefed<NullPrincipal>
  Create(const mozilla::OriginAttributes& aOriginAttributes = mozilla::OriginAttributes(),
         nsIURI* aURI = nullptr);

  nsresult Init(const mozilla::OriginAttributes& aOriginAttributes = mozilla::OriginAttributes(),
                nsIURI* aURI = nullptr);

  virtual nsresult GetScriptLocation(nsACString &aStr) override;

 protected:
  virtual ~NullPrincipal() = default;

  bool SubsumesInternal(nsIPrincipal* aOther, DocumentDomainConsideration aConsideration) override
  {
    return aOther == this;
  }

  bool MayLoadInternal(nsIURI* aURI) override;

  nsCOMPtr<nsIURI> mURI;

private:
  // If aIsFirstParty is true, this NullPrincipal will be initialized base on
  // the aOriginAttributes with FirstPartyDomain set to an unique value, and this
  // value is generated from mURI.path, with ".mozilla" appending at the end.
  nsresult Init(const mozilla::OriginAttributes& aOriginAttributes, bool aIsFirstParty);
};

#endif // NullPrincipal_h__
