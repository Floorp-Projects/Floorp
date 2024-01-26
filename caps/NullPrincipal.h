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

#define NS_NULLPRINCIPAL_CID                         \
  {                                                  \
    0xbd066e5f, 0x146f, 0x4472, {                    \
      0x83, 0x31, 0x7b, 0xfd, 0x05, 0xb1, 0xed, 0x90 \
    }                                                \
  }

#define NS_NULLPRINCIPAL_SCHEME "moz-nullprincipal"

namespace mozilla {

class JSONWriter;

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
  NS_IMETHOD GetPrecursorPrincipal(nsIPrincipal** aPrecursor) override;

  // Create a NullPrincipal, inheriting origin attributes from the given
  // principal.
  // If aInheritFrom is a content principal, or has a content principal
  // precursor, it will be used as the precursor for this principal.
  static already_AddRefed<NullPrincipal> CreateWithInheritedAttributes(
      nsIPrincipal* aInheritFrom);

  // Create a new NullPrincipal with the specified OriginAttributes.
  //
  // If `aNullPrincipalURI` is specified, it must be a NS_NULLPRINCIPAL_SCHEME
  // URI previously created using `NullPrincipal::CreateURI`, and will be used
  // as the origin URI for this principal.
  static already_AddRefed<NullPrincipal> Create(
      const OriginAttributes& aOriginAttributes,
      nsIURI* aNullPrincipalURI = nullptr);

  static already_AddRefed<NullPrincipal> CreateWithoutOriginAttributes();

  // Generates a new unique `moz-nullprincipal:` URI. If `aPrecursor` is
  // specified, it will be included in the generated URI as the null principal's
  // precursor.
  //
  // The `aPrincipalID` attribute is used to force the creation of a
  // deterministic NullPrincipal in situations where that is required. Avoid
  // using this parameter unless absolutely necessary.
  static already_AddRefed<nsIURI> CreateURI(nsIPrincipal* aPrecursor = nullptr,
                                            const nsID* aPrincipalID = nullptr);

  virtual nsresult GetScriptLocation(nsACString& aStr) override;

  nsresult GetSiteIdentifier(SiteIdentifier& aSite) override {
    aSite.Init(this);
    return NS_OK;
  }

  virtual nsresult WriteJSONInnerProperties(JSONWriter& aWriter) override;

  // Serializable keys are the valid enum fields the serialization supports
  enum SerializableKeys : uint8_t { eSpec = 0, eSuffix, eMax = eSuffix };

  static constexpr char SpecKey = '0';
  static_assert(eSpec == 0);
  static constexpr char SuffixKey = '1';
  static_assert(eSuffix == 1);

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
  FRIEND_TEST(NullPrincipalPrecursor, EscapingRoundTrips);

  static void EscapePrecursorQuery(nsACString& aPrecursorQuery);
  static void UnescapePrecursorQuery(nsACString& aPrecursorQuery);
};

}  // namespace mozilla

#endif  // mozilla_NullPrincipal_h
