/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_BasePrincipal_h
#define mozilla_BasePrincipal_h

#include "nsIPrincipal.h"
#include "nsIScriptSecurityManager.h"
#include "nsJSPrincipals.h"

class nsIObjectOutputStream;
class nsIObjectInputStream;

namespace mozilla {

/*
 * Base class from which all nsIPrincipal implementations inherit. Use this for
 * default implementations and other commonalities between principal
 * implementations.
 *
 * We should merge nsJSPrincipals into this class at some point.
 */
class BasePrincipal : public nsJSPrincipals
{
public:
  BasePrincipal() {}

  enum DocumentDomainConsideration { DontConsiderDocumentDomain, ConsiderDocumentDomain};
  bool Subsumes(nsIPrincipal* aOther, DocumentDomainConsideration aConsideration);

  NS_IMETHOD Equals(nsIPrincipal* other, bool* _retval) final;
  NS_IMETHOD EqualsConsideringDomain(nsIPrincipal* other, bool* _retval) final;
  NS_IMETHOD Subsumes(nsIPrincipal* other, bool* _retval) final;
  NS_IMETHOD SubsumesConsideringDomain(nsIPrincipal* other, bool* _retval) final;
  NS_IMETHOD GetCsp(nsIContentSecurityPolicy** aCsp) override;
  NS_IMETHOD SetCsp(nsIContentSecurityPolicy* aCsp) override;
  NS_IMETHOD GetIsNullPrincipal(bool* aIsNullPrincipal) override;
  NS_IMETHOD GetJarPrefix(nsACString& aJarPrefix) final;
  NS_IMETHOD GetAppStatus(uint16_t* aAppStatus) final;
  NS_IMETHOD GetAppId(uint32_t* aAppStatus) final;
  NS_IMETHOD GetIsInBrowserElement(bool* aIsInBrowserElement) final;
  NS_IMETHOD GetUnknownAppId(bool* aUnknownAppId) final;

  virtual bool IsOnCSSUnprefixingWhitelist() override { return false; }

  static BasePrincipal* Cast(nsIPrincipal* aPrin) { return static_cast<BasePrincipal*>(aPrin); }

  struct OriginAttributes {
    // NB: If you add any members here, you need to update Serialize/Deserialize
    // and bump the CIDs of all the principal implementations that invoke those
    // methods.
    uint32_t mAppId;
    bool mIsInBrowserElement;

    OriginAttributes() : mAppId(nsIScriptSecurityManager::NO_APP_ID), mIsInBrowserElement(false) {}
    OriginAttributes(uint32_t aAppId, bool aIsInBrowserElement)
      : mAppId(aAppId), mIsInBrowserElement(aIsInBrowserElement) {}
    bool operator==(const OriginAttributes& aOther) const
    {
      return mAppId == aOther.mAppId &&
             mIsInBrowserElement == aOther.mIsInBrowserElement;
    }
    bool operator!=(const OriginAttributes& aOther) const
    {
      return !(*this == aOther);
    }

    void Serialize(nsIObjectOutputStream* aStream) const;
    nsresult Deserialize(nsIObjectInputStream* aStream);
  };

  const OriginAttributes& OriginAttributesRef() { return mOriginAttributes; }
  uint32_t AppId() const { return mOriginAttributes.mAppId; }
  bool IsInBrowserElement() const { return mOriginAttributes.mIsInBrowserElement; }

protected:
  virtual ~BasePrincipal() {}

  virtual bool SubsumesInternal(nsIPrincipal* aOther, DocumentDomainConsideration aConsider) = 0;

  nsCOMPtr<nsIContentSecurityPolicy> mCSP;
  OriginAttributes mOriginAttributes;
};

} // namespace mozilla

#endif /* mozilla_BasePrincipal_h */
