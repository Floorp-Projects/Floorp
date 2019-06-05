/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ExpandedPrincipal_h
#define ExpandedPrincipal_h

#include "nsCOMPtr.h"
#include "nsJSPrincipals.h"
#include "nsTArray.h"
#include "nsNetUtil.h"
#include "mozilla/BasePrincipal.h"

class nsIContentSecurityPolicy;

namespace Json {
class Value;
}

class ExpandedPrincipal : public nsIExpandedPrincipal,
                          public mozilla::BasePrincipal {
 public:
  static already_AddRefed<ExpandedPrincipal> Create(
      nsTArray<nsCOMPtr<nsIPrincipal>>& aAllowList,
      const mozilla::OriginAttributes& aAttrs);

  static PrincipalKind Kind() { return eExpandedPrincipal; }

  // For use from the XPCOM factory constructor only.  Do not ever use this
  // constructor by hand!
  ExpandedPrincipal();

  NS_DECL_NSIEXPANDEDPRINCIPAL
  NS_DECL_NSISERIALIZABLE

  NS_IMETHOD_(MozExternalRefCountType) AddRef() override {
    return nsJSPrincipals::AddRef();
  };
  NS_IMETHOD_(MozExternalRefCountType) Release() override {
    return nsJSPrincipals::Release();
  };
  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr) override;
  uint32_t GetHashValue() override;
  NS_IMETHOD GetURI(nsIURI** aURI) override;
  NS_IMETHOD GetDomain(nsIURI** aDomain) override;
  NS_IMETHOD SetDomain(nsIURI* aDomain) override;
  NS_IMETHOD GetBaseDomain(nsACString& aBaseDomain) override;
  NS_IMETHOD GetAddonId(nsAString& aAddonId) override;
  virtual bool AddonHasPermission(const nsAtom* aPerm) override;
  virtual nsresult GetScriptLocation(nsACString& aStr) override;

  bool AddonAllowsLoad(nsIURI* aURI, bool aExplicit = false);

  void SetCsp(nsIContentSecurityPolicy* aCSP);

  // Returns the principal to inherit when this principal requests the given
  // URL. See BasePrincipal::PrincipalToInherit.
  nsIPrincipal* PrincipalToInherit(nsIURI* aRequestedURI = nullptr);

  nsresult GetSiteIdentifier(mozilla::SiteIdentifier& aSite) override;

  virtual nsresult PopulateJSONObject(Json::Value& aObject) override;
  // Serializable keys are the valid enum fields the serialization supports
  enum SerializableKeys { eSpecs = 0, eSuffix, eMax = eSuffix };
  // KeyVal is a lightweight storage that passes
  // SerializableKeys and values after JSON parsing in the BasePrincipal to
  // FromProperties
  struct KeyVal {
    bool valueWasSerialized;
    nsCString value;
    SerializableKeys key;
  };
  static already_AddRefed<BasePrincipal> FromProperties(
      nsTArray<ExpandedPrincipal::KeyVal>& aFields);

 protected:
  explicit ExpandedPrincipal(nsTArray<nsCOMPtr<nsIPrincipal>>& aAllowList);

  virtual ~ExpandedPrincipal();

  bool SubsumesInternal(nsIPrincipal* aOther,
                        DocumentDomainConsideration aConsideration) override;

  bool MayLoadInternal(nsIURI* aURI) override;

 private:
  nsTArray<nsCOMPtr<nsIPrincipal>> mPrincipals;
  nsCOMPtr<nsIContentSecurityPolicy> mCSP;
};

#define NS_EXPANDEDPRINCIPAL_CONTRACTID "@mozilla.org/expandedprincipal;1"
#define NS_EXPANDEDPRINCIPAL_CID                     \
  {                                                  \
    0xe8ee88b0, 0x5571, 0x4086, {                    \
      0xa4, 0x5b, 0x39, 0xa7, 0x16, 0x90, 0x6b, 0xdb \
    }                                                \
  }

#endif  // ExpandedPrincipal_h
