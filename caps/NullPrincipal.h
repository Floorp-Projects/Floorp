/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This is the principal that has no rights and can't be accessed by
 * anything other than itself and chrome; null principals are not
 * same-origin with anything but themselves.
 */

#ifndef mozilla_NullPrincipal_h
#define mozilla_NullPrincipal_h

#include "nsIPrincipal.h"
#include "nsJSPrincipals.h"
#include "nsCOMPtr.h"

#include "mozilla/BasePrincipal.h"
#include "gtest/MozGtestFriend.h"

class nsIDocShell;
class nsIURI;
namespace Json {
class Value;
}

#define NS_NULLPRINCIPAL_CID                         \
  {                                                  \
    0xbd066e5f, 0x146f, 0x4472, {                    \
      0x83, 0x31, 0x7b, 0xfd, 0x05, 0xb1, 0xed, 0x90 \
    }                                                \
  }

#define NS_NULLPRINCIPAL_SCHEME "moz-nullprincipal"

namespace mozilla {

class NullPrincipal final : public BasePrincipal {
 public:
  static PrincipalKind Kind() { return eNullPrincipal; }

  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr) override;
  uint32_t GetHashValue() override;
  NS_IMETHOD GetURI(nsIURI** aURI) override;
  NS_IMETHOD GetIsOriginPotentiallyTrustworthy(bool* aResult) override;
  NS_IMETHOD GetDomain(nsIURI** aDomain) override;
  NS_IMETHOD SetDomain(nsIURI* aDomain) override;
  NS_IMETHOD GetBaseDomain(nsACString& aBaseDomain) override;
  NS_IMETHOD GetAddonId(nsAString& aAddonId) override;

  static already_AddRefed<NullPrincipal> CreateWithInheritedAttributes(
      nsIPrincipal* aInheritFrom);

  // Create NullPrincipal with origin attributes from docshell.
  // If aIsFirstParty is true, and the pref 'privacy.firstparty.isolate' is also
  // enabled, the mFirstPartyDomain value of the origin attributes will be set
  // to an unique value.
  static already_AddRefed<NullPrincipal> CreateWithInheritedAttributes(
      nsIDocShell* aDocShell, bool aIsFirstParty = false);
  static already_AddRefed<NullPrincipal> CreateWithInheritedAttributes(
      const OriginAttributes& aOriginAttributes, bool aIsFirstParty = false);

  static already_AddRefed<NullPrincipal> Create(
      const OriginAttributes& aOriginAttributes, nsIURI* aURI = nullptr);

  static already_AddRefed<NullPrincipal> CreateWithoutOriginAttributes();

  static already_AddRefed<nsIURI> CreateURI();

  virtual nsresult GetScriptLocation(nsACString& aStr) override;

  nsresult GetSiteIdentifier(SiteIdentifier& aSite) override {
    aSite.Init(this);
    return NS_OK;
  }

  virtual nsresult PopulateJSONObject(Json::Value& aObject) override;

  // Serializable keys are the valid enum fields the serialization supports
  enum SerializableKeys : uint8_t { eSpec = 0, eSuffix, eMax = eSuffix };
  typedef mozilla::BasePrincipal::KeyValT<SerializableKeys> KeyVal;

  static already_AddRefed<BasePrincipal> FromProperties(
      nsTArray<NullPrincipal::KeyVal>& aFields);

  class Deserializer : public BasePrincipal::Deserializer {
   public:
    NS_IMETHOD Read(nsIObjectInputStream* aStream) override;
  };

 protected:
  NullPrincipal(nsIURI* aURI, const nsACString& aOriginNoSuffix,
                const OriginAttributes& aOriginAttributes);

  virtual ~NullPrincipal() = default;

  bool SubsumesInternal(nsIPrincipal* aOther,
                        DocumentDomainConsideration aConsideration) override {
    MOZ_ASSERT(aOther);
    return FastEquals(aOther);
  }

  bool MayLoadInternal(nsIURI* aURI) override;

  const nsCOMPtr<nsIURI> mURI;

 private:
  FRIEND_TEST(OriginAttributes, NullPrincipal);

  // If aIsFirstParty is true, this NullPrincipal will be initialized based on
  // the aOriginAttributes with FirstPartyDomain set to a unique value.
  // This value is generated from mURI.path, with ".mozilla" appended at the
  // end. aURI is used for testing purpose to assign specific UUID rather than
  // random generated one.
  static already_AddRefed<NullPrincipal> CreateInternal(
      const OriginAttributes& aOriginAttributes, bool aIsFirstParty,
      nsIURI* aURI = nullptr);
};

}  // namespace mozilla

#endif  // mozilla_NullPrincipal_h
