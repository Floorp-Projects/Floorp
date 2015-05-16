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
{ 0xa0bd8b42, 0xf6bf, 0x4fb9, \
  { 0x93, 0x42, 0x90, 0xbf, 0xc9, 0xb7, 0xa1, 0xab } }
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
  NS_IMETHOD Equals(nsIPrincipal* other, bool* _retval) override;
  NS_IMETHOD EqualsConsideringDomain(nsIPrincipal* other, bool* _retval) override;
  NS_IMETHOD GetHashValue(uint32_t* aHashValue) override;
  NS_IMETHOD GetURI(nsIURI** aURI) override;
  NS_IMETHOD GetDomain(nsIURI** aDomain) override;
  NS_IMETHOD SetDomain(nsIURI* aDomain) override;
  NS_IMETHOD GetOrigin(nsACString& aOrigin) override;
  NS_IMETHOD Subsumes(nsIPrincipal* other, bool* _retval) override;
  NS_IMETHOD SubsumesConsideringDomain(nsIPrincipal* other, bool* _retval) override;
  NS_IMETHOD CheckMayLoad(nsIURI* uri, bool report, bool allowIfInheritsPrincipal) override;
  NS_IMETHOD GetJarPrefix(nsACString& aJarPrefix) override;
  NS_IMETHOD GetAppStatus(uint16_t* aAppStatus) override;
  NS_IMETHOD GetAppId(uint32_t* aAppStatus) override;
  NS_IMETHOD GetIsInBrowserElement(bool* aIsInBrowserElement) override;
  NS_IMETHOD GetUnknownAppId(bool* aUnknownAppId) override;
  NS_IMETHOD GetIsNullPrincipal(bool* aIsNullPrincipal) override;
  NS_IMETHOD GetBaseDomain(nsACString& aBaseDomain) override;

  // Returns null on failure.
  static already_AddRefed<nsNullPrincipal> CreateWithInheritedAttributes(nsIPrincipal *aInheritFrom);

  // Returns null on failure.
  static already_AddRefed<nsNullPrincipal>
    Create(uint32_t aAppId = nsIScriptSecurityManager::NO_APP_ID,
           bool aInMozBrowser = false);

  nsresult Init(uint32_t aAppId = nsIScriptSecurityManager::NO_APP_ID,
                bool aInMozBrowser = false);

  virtual void GetScriptLocation(nsACString &aStr) override;

 protected:
  virtual ~nsNullPrincipal() {}

  nsCOMPtr<nsIURI> mURI;
  nsCOMPtr<nsIContentSecurityPolicy> mCSP;
  uint32_t mAppId;
  bool mInMozBrowser;
};

#endif // nsNullPrincipal_h__
