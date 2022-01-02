/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ContentPrincipal_h
#define mozilla_ContentPrincipal_h

#include "nsCOMPtr.h"
#include "nsJSPrincipals.h"
#include "nsTArray.h"
#include "nsNetUtil.h"
#include "nsScriptSecurityManager.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/extensions/WebExtensionPolicy.h"

namespace Json {
class Value;
}

namespace mozilla {

class ContentPrincipal final : public BasePrincipal {
 public:
  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr) override;
  uint32_t GetHashValue() override;
  NS_IMETHOD GetURI(nsIURI** aURI) override;
  NS_IMETHOD GetDomain(nsIURI** aDomain) override;
  NS_IMETHOD SetDomain(nsIURI* aDomain) override;
  NS_IMETHOD GetBaseDomain(nsACString& aBaseDomain) override;
  NS_IMETHOD GetAddonId(nsAString& aAddonId) override;
  NS_IMETHOD GetSiteOriginNoSuffix(nsACString& aSiteOrigin) override;
  bool IsContentPrincipal() const override { return true; }

  ContentPrincipal(nsIURI* aURI, const OriginAttributes& aOriginAttributes,
                   const nsACString& aOriginNoSuffix);
  ContentPrincipal(ContentPrincipal* aOther,
                   const OriginAttributes& aOriginAttributes);

  static PrincipalKind Kind() { return eContentPrincipal; }

  virtual nsresult GetScriptLocation(nsACString& aStr) override;

  nsresult GetSiteIdentifier(SiteIdentifier& aSite) override;

  static nsresult GenerateOriginNoSuffixFromURI(nsIURI* aURI,
                                                nsACString& aOrigin);

  extensions::WebExtensionPolicy* AddonPolicy();

  virtual nsresult PopulateJSONObject(Json::Value& aObject) override;
  // Serializable keys are the valid enum fields the serialization supports
  enum SerializableKeys : uint8_t {
    eURI = 0,
    eDomain,
    eSuffix,
    eMax = eSuffix
  };
  typedef mozilla::BasePrincipal::KeyValT<SerializableKeys> KeyVal;

  static already_AddRefed<BasePrincipal> FromProperties(
      nsTArray<ContentPrincipal::KeyVal>& aFields);

  class Deserializer : public BasePrincipal::Deserializer {
   public:
    NS_IMETHOD Read(nsIObjectInputStream* aStream) override;
  };

 protected:
  virtual ~ContentPrincipal();

  bool SubsumesInternal(nsIPrincipal* aOther,
                        DocumentDomainConsideration aConsideration) override;
  bool MayLoadInternal(nsIURI* aURI) override;

 private:
  const nsCOMPtr<nsIURI> mURI;
  nsCOMPtr<nsIURI> mDomain;
  Maybe<WeakPtr<extensions::WebExtensionPolicy>> mAddon;
};

}  // namespace mozilla

#define NS_PRINCIPAL_CID                             \
  {                                                  \
    0x653e0e4d, 0x3ee4, 0x45fa, {                    \
      0xb2, 0x72, 0x97, 0xc2, 0x0b, 0xc0, 0x1e, 0xb8 \
    }                                                \
  }

#endif  // mozilla_ContentPrincipal_h
