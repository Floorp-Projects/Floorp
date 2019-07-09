/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ContentPrincipal_h
#define mozilla_ContentPrincipal_h

#include "nsCOMPtr.h"
#include "nsJSPrincipals.h"
#include "nsTArray.h"
#include "nsIContentSecurityPolicy.h"
#include "nsIProtocolHandler.h"
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
  NS_DECL_NSISERIALIZABLE
  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr) override;
  uint32_t GetHashValue() override;
  NS_IMETHOD GetURI(nsIURI** aURI) override;
  NS_IMETHOD GetDomain(nsIURI** aDomain) override;
  NS_IMETHOD SetDomain(nsIURI* aDomain) override;
  NS_IMETHOD GetBaseDomain(nsACString& aBaseDomain) override;
  NS_IMETHOD GetAddonId(nsAString& aAddonId) override;
  NS_IMETHOD GetSiteOrigin(nsACString& aSiteOrigin) override;
  bool IsContentPrincipal() const override { return true; }

  ContentPrincipal();

  static PrincipalKind Kind() { return eContentPrincipal; }

  // Init() must be called before the principal is in a usable state.
  nsresult Init(nsIURI* aURI, const OriginAttributes& aOriginAttributes,
                const nsACString& aOriginNoSuffix);
  nsresult Init(ContentPrincipal* aOther,
                const OriginAttributes& aOriginAttributes);

  virtual nsresult GetScriptLocation(nsACString& aStr) override;

  nsresult GetSiteIdentifier(SiteIdentifier& aSite) override;

  static nsresult GenerateOriginNoSuffixFromURI(nsIURI* aURI,
                                                nsACString& aOrigin);

  extensions::WebExtensionPolicy* AddonPolicy();

  nsCOMPtr<nsIURI> mDomain;
  nsCOMPtr<nsIURI> mURI;

  virtual nsresult PopulateJSONObject(Json::Value& aObject) override;
  // Serializable keys are the valid enum fields the serialization supports
  enum SerializableKeys { eURI = 0, eDomain, eSuffix, eMax = eSuffix };
  // KeyVal is a lightweight storage that passes
  // SerializableKeys and values after JSON parsing in the BasePrincipal to
  // FromProperties
  struct KeyVal {
    bool valueWasSerialized;
    nsCString value;
    SerializableKeys key;
  };
  static already_AddRefed<BasePrincipal> FromProperties(
      nsTArray<ContentPrincipal::KeyVal>& aFields);

 protected:
  virtual ~ContentPrincipal();

  bool SubsumesInternal(nsIPrincipal* aOther,
                        DocumentDomainConsideration aConsideration) override;
  bool MayLoadInternal(nsIURI* aURI) override;

 private:
  Maybe<WeakPtr<extensions::WebExtensionPolicy>> mAddon;
};

}  // namespace mozilla

#define NS_PRINCIPAL_CONTRACTID "@mozilla.org/principal;1"
#define NS_PRINCIPAL_CID                             \
  {                                                  \
    0x653e0e4d, 0x3ee4, 0x45fa, {                    \
      0xb2, 0x72, 0x97, 0xc2, 0x0b, 0xc0, 0x1e, 0xb8 \
    }                                                \
  }

#endif  // mozilla_ContentPrincipal_h
