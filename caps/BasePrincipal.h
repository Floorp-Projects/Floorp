/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_BasePrincipal_h
#define mozilla_BasePrincipal_h

#include "nsJSPrincipals.h"

#include "mozilla/Attributes.h"
#include "mozilla/OriginAttributes.h"

class nsAtom;
class nsIContentSecurityPolicy;
class nsIObjectOutputStream;
class nsIObjectInputStream;
class nsIURI;

class ExpandedPrincipal;

namespace Json {
class Value;
}
namespace mozilla {
namespace dom {
class Document;
}
namespace extensions {
class WebExtensionPolicy;
}

class BasePrincipal;

// Content principals (and content principals embedded within expanded
// principals) stored in SiteIdentifier are guaranteed to contain only the
// eTLD+1 part of the original domain. This is used to determine whether two
// origins are same-site: if it's possible for two origins to access each other
// (maybe after mutating document.domain), then they must have the same site
// identifier.
class SiteIdentifier {
 public:
  void Init(BasePrincipal* aPrincipal) {
    MOZ_ASSERT(aPrincipal);
    mPrincipal = aPrincipal;
  }

  bool IsInitialized() const { return !!mPrincipal; }

  bool Equals(const SiteIdentifier& aOther) const;

 private:
  friend class ::ExpandedPrincipal;

  BasePrincipal* GetPrincipal() const {
    MOZ_ASSERT(IsInitialized());
    return mPrincipal;
  }

  RefPtr<BasePrincipal> mPrincipal;
};

/*
 * Base class from which all nsIPrincipal implementations inherit. Use this for
 * default implementations and other commonalities between principal
 * implementations.
 *
 * We should merge nsJSPrincipals into this class at some point.
 */
class BasePrincipal : public nsJSPrincipals {
 public:
  // Warning: this enum impacts Principal serialization into JSON format.
  // Only update if you know exactly what you are doing
  enum PrincipalKind {
    eNullPrincipal = 0,
    eContentPrincipal,
    eExpandedPrincipal,
    eSystemPrincipal,
    eKindMax = eSystemPrincipal
  };

  explicit BasePrincipal(PrincipalKind aKind);

  template <typename T>
  bool Is() const {
    return mKind == T::Kind();
  }

  template <typename T>
  T* As() {
    MOZ_ASSERT(Is<T>());
    return static_cast<T*>(this);
  }

  enum DocumentDomainConsideration {
    DontConsiderDocumentDomain,
    ConsiderDocumentDomain
  };
  bool Subsumes(nsIPrincipal* aOther,
                DocumentDomainConsideration aConsideration);

  NS_IMETHOD GetOrigin(nsACString& aOrigin) final;
  NS_IMETHOD GetAsciiOrigin(nsACString& aOrigin) override;
  NS_IMETHOD GetOriginNoSuffix(nsACString& aOrigin) final;
  NS_IMETHOD Equals(nsIPrincipal* other, bool* _retval) final;
  NS_IMETHOD EqualsConsideringDomain(nsIPrincipal* other, bool* _retval) final;
  NS_IMETHOD EqualsURI(nsIURI* aOtherURI, bool* _retval) override;
  NS_IMETHOD Subsumes(nsIPrincipal* other, bool* _retval) final;
  NS_IMETHOD SubsumesConsideringDomain(nsIPrincipal* other,
                                       bool* _retval) final;
  NS_IMETHOD SubsumesConsideringDomainIgnoringFPD(nsIPrincipal* other,
                                                  bool* _retval) final;
  NS_IMETHOD CheckMayLoad(nsIURI* uri, bool allowIfInheritsPrincipal) final;
  NS_IMETHOD CheckMayLoadWithReporting(nsIURI* uri,
                                       bool allowIfInheritsPrincipal,
                                       uint64_t innerWindowID) final;
  NS_IMETHOD GetAddonPolicy(nsISupports** aResult) final;
  NS_IMETHOD GetIsNullPrincipal(bool* aResult) override;
  NS_IMETHOD GetIsContentPrincipal(bool* aResult) override;
  NS_IMETHOD GetIsExpandedPrincipal(bool* aResult) override;
  NS_IMETHOD GetIsSystemPrincipal(bool* aResult) override;
  NS_IMETHOD SchemeIs(const char* aScheme, bool* aResult) override;
  NS_IMETHOD IsURIInPrefList(const char* aPref, bool* aResult) override;
  NS_IMETHOD IsL10nAllowed(nsIURI* aURI, bool* aResult) override;
  NS_IMETHOD GetAboutModuleFlags(uint32_t* flags) override;
  NS_IMETHOD GetIsAddonOrExpandedAddonPrincipal(bool* aResult) override;
  NS_IMETHOD GetOriginAttributes(JSContext* aCx,
                                 JS::MutableHandle<JS::Value> aVal) final;
  NS_IMETHOD GetAsciiSpec(nsACString& aSpec) override;
  NS_IMETHOD GetExposablePrePath(nsACString& aResult) override;
  NS_IMETHOD GetExposableSpec(nsACString& aSpec) override;
  NS_IMETHOD GetHostPort(nsACString& aRes) override;
  NS_IMETHOD GetHost(nsACString& aRes) override;
  NS_IMETHOD GetPrepath(nsACString& aResult) override;
  NS_IMETHOD GetOriginSuffix(nsACString& aOriginSuffix) final;
  NS_IMETHOD GetIsIpAddress(bool* aIsIpAddress) override;
  NS_IMETHOD GetIsOnion(bool* aIsOnion) override;
  NS_IMETHOD GetIsInIsolatedMozBrowserElement(
      bool* aIsInIsolatedMozBrowserElement) final;
  NS_IMETHOD GetUserContextId(uint32_t* aUserContextId) final;
  NS_IMETHOD GetPrivateBrowsingId(uint32_t* aPrivateBrowsingId) final;
  NS_IMETHOD GetSiteOrigin(nsACString& aOrigin) override;
  NS_IMETHOD IsThirdPartyURI(nsIURI* uri, bool* aRes) override;
  NS_IMETHOD IsThirdPartyPrincipal(nsIPrincipal* uri, bool* aRes) override;
  NS_IMETHOD GetIsOriginPotentiallyTrustworthy(bool* aResult) override;
  NS_IMETHOD IsSameOrigin(nsIURI* aURI, bool aIsPrivateWin,
                          bool* aRes) override;
  NS_IMETHOD GetPrefLightCacheKey(nsIURI* aURI, bool aWithCredentials,
                                  nsACString& _retval) override;
  NS_IMETHOD GetAsciiHost(nsACString& aAsciiHost) override;
  NS_IMETHOD GetLocalStorageQuotaKey(nsACString& aRes) override;
  NS_IMETHOD AllowsRelaxStrictFileOriginPolicy(nsIURI* aURI,
                                               bool* aRes) override;
  NS_IMETHOD CreateReferrerInfo(mozilla::dom::ReferrerPolicy aReferrerPolicy,
                                nsIReferrerInfo** _retval) override;
  NS_IMETHOD GetIsScriptAllowedByPolicy(
      bool* aIsScriptAllowedByPolicy) override;
  nsresult ToJSON(nsACString& aJSON);
  static already_AddRefed<BasePrincipal> FromJSON(const nsACString& aJSON);
  // Method populates a passed Json::Value with serializable fields
  // which represent all of the fields to deserialize the principal
  virtual nsresult PopulateJSONObject(Json::Value& aObject);

  virtual bool AddonHasPermission(const nsAtom* aPerm);

  virtual bool IsContentPrincipal() const { return false; };

  static BasePrincipal* Cast(nsIPrincipal* aPrin) {
    return static_cast<BasePrincipal*>(aPrin);
  }

  static BasePrincipal& Cast(nsIPrincipal& aPrin) {
    return *static_cast<BasePrincipal*>(&aPrin);
  }

  static const BasePrincipal* Cast(const nsIPrincipal* aPrin) {
    return static_cast<const BasePrincipal*>(aPrin);
  }

  static const BasePrincipal& Cast(const nsIPrincipal& aPrin) {
    return *static_cast<const BasePrincipal*>(&aPrin);
  }

  static already_AddRefed<BasePrincipal> CreateContentPrincipal(
      const nsACString& aOrigin);

  // These following method may not create a content principal in case it's
  // not possible to generate a correct origin from the passed URI. If this
  // happens, a NullPrincipal is returned.

  static already_AddRefed<BasePrincipal> CreateContentPrincipal(
      nsIURI* aURI, const OriginAttributes& aAttrs);

  const OriginAttributes& OriginAttributesRef() final {
    return mOriginAttributes;
  }
  extensions::WebExtensionPolicy* AddonPolicy();
  uint32_t UserContextId() const { return mOriginAttributes.mUserContextId; }
  uint32_t PrivateBrowsingId() const {
    return mOriginAttributes.mPrivateBrowsingId;
  }
  bool IsInIsolatedMozBrowserElement() const {
    return mOriginAttributes.mInIsolatedMozBrowser;
  }

  PrincipalKind Kind() const { return mKind; }

  already_AddRefed<BasePrincipal> CloneForcingOriginAttributes(
      const OriginAttributes& aOriginAttributes);

  // If this is an add-on content script principal, returns its AddonPolicy.
  // Otherwise returns null.
  extensions::WebExtensionPolicy* ContentScriptAddonPolicy();

  // Helper to check whether this principal is associated with an addon that
  // allows unprivileged code to load aURI.  aExplicit == true will prevent
  // use of all_urls permission, requiring the domain in its permissions.
  bool AddonAllowsLoad(nsIURI* aURI, bool aExplicit = false);

  // Call these to avoid the cost of virtual dispatch.
  inline bool FastEquals(nsIPrincipal* aOther);
  inline bool FastEqualsConsideringDomain(nsIPrincipal* aOther);
  inline bool FastSubsumes(nsIPrincipal* aOther);
  inline bool FastSubsumesConsideringDomain(nsIPrincipal* aOther);
  inline bool FastSubsumesIgnoringFPD(nsIPrincipal* aOther);
  inline bool FastSubsumesConsideringDomainIgnoringFPD(nsIPrincipal* aOther);

  // Fast way to check whether we have a system principal.
  inline bool IsSystemPrincipal() const;

  // Returns the principal to inherit when a caller with this principal loads
  // the given URI.
  //
  // For most principal types, this returns the principal itself. For expanded
  // principals, it returns the first sub-principal which subsumes the given URI
  // (or, if no URI is given, the last allowlist principal).
  nsIPrincipal* PrincipalToInherit(nsIURI* aRequestedURI = nullptr);

  /* Returns true if this principal's CSP should override a document's CSP for
   * loads that it triggers. Currently true for expanded principals which
   * subsume the document principal, and add-on content principals regardless
   * of whether they subsume the document principal.
   */
  bool OverridesCSP(nsIPrincipal* aDocumentPrincipal) {
    MOZ_ASSERT(aDocumentPrincipal);

    // Expanded principals override CSP if and only if they subsume the document
    // principal.
    if (mKind == eExpandedPrincipal) {
      return FastSubsumes(aDocumentPrincipal);
    }
    // Extension principals always override the CSP non-extension principals.
    // This is primarily for the sake of their stylesheets, which are usually
    // loaded from channels and cannot have expanded principals.
    return (AddonPolicy() &&
            !BasePrincipal::Cast(aDocumentPrincipal)->AddonPolicy());
  }

  uint32_t GetOriginNoSuffixHash() const { return mOriginNoSuffix->hash(); }
  uint32_t GetOriginSuffixHash() const { return mOriginSuffix->hash(); }

  virtual nsresult GetSiteIdentifier(SiteIdentifier& aSite) = 0;

 protected:
  virtual ~BasePrincipal();

  // Note that this does not check OriginAttributes. Callers that depend on
  // those must call Subsumes instead.
  virtual bool SubsumesInternal(nsIPrincipal* aOther,
                                DocumentDomainConsideration aConsider) = 0;

  // Internal, side-effect-free check to determine whether the concrete
  // principal would allow the load ignoring any common behavior implemented in
  // BasePrincipal::CheckMayLoad.
  virtual bool MayLoadInternal(nsIURI* aURI) = 0;
  friend class ::ExpandedPrincipal;

  // Helper for implementing CheckMayLoad and CheckMayLoadWithReporting.
  nsresult CheckMayLoadHelper(nsIURI* aURI, bool aAllowIfInheritsPrincipal,
                              bool aReport, uint64_t aInnerWindowID);

  void SetHasExplicitDomain() { mHasExplicitDomain = true; }

  // Either of these functions should be called as the last step of the
  // initialization of the principal objects.  It's typically called as the
  // last step from the Init() method of the child classes.
  void FinishInit(const nsACString& aOriginNoSuffix,
                  const OriginAttributes& aOriginAttributes);
  void FinishInit(BasePrincipal* aOther,
                  const OriginAttributes& aOriginAttributes);

  // KeyValT holds a principal subtype-specific key value and the associated
  // parsed value after JSON parsing.
  template <typename SerializedKey>
  struct KeyValT {
    static_assert(sizeof(SerializedKey) == 1,
                  "SerializedKey should be a uint8_t");
    SerializedKey key;
    bool valueWasSerialized;
    nsCString value;
  };

 private:
  static already_AddRefed<BasePrincipal> CreateContentPrincipal(
      nsIURI* aURI, const OriginAttributes& aAttrs,
      const nsACString& aOriginNoSuffix);

  inline bool FastSubsumesIgnoringFPD(
      nsIPrincipal* aOther, DocumentDomainConsideration aConsideration);

  RefPtr<nsAtom> mOriginNoSuffix;
  RefPtr<nsAtom> mOriginSuffix;

  OriginAttributes mOriginAttributes;
  PrincipalKind mKind;
  bool mHasExplicitDomain;
  bool mInitialized;
};

inline bool BasePrincipal::FastEquals(nsIPrincipal* aOther) {
  MOZ_ASSERT(aOther);

  auto other = Cast(aOther);
  if (Kind() != other->Kind()) {
    // Principals of different kinds can't be equal.
    return false;
  }

  // Two principals are considered to be equal if their origins are the same.
  // If the two principals are content principals, their origin attributes
  // (aka the origin suffix) must also match.
  if (Kind() == eSystemPrincipal) {
    return this == other;
  }

  if (Kind() == eContentPrincipal || Kind() == eNullPrincipal) {
    return mOriginNoSuffix == other->mOriginNoSuffix &&
           mOriginSuffix == other->mOriginSuffix;
  }

  MOZ_ASSERT(Kind() == eExpandedPrincipal);
  return mOriginNoSuffix == other->mOriginNoSuffix;
}

inline bool BasePrincipal::FastEqualsConsideringDomain(nsIPrincipal* aOther) {
  MOZ_ASSERT(aOther);

  // If neither of the principals have document.domain set, we use the fast path
  // in Equals().  Otherwise, we fall back to the slow path below.
  auto other = Cast(aOther);
  if (!mHasExplicitDomain && !other->mHasExplicitDomain) {
    return FastEquals(aOther);
  }

  return Subsumes(aOther, ConsiderDocumentDomain) &&
         other->Subsumes(this, ConsiderDocumentDomain);
}

inline bool BasePrincipal::FastSubsumes(nsIPrincipal* aOther) {
  MOZ_ASSERT(aOther);

  // If two principals are equal, then they both subsume each other.
  if (FastEquals(aOther)) {
    return true;
  }

  // Otherwise, fall back to the slow path.
  return Subsumes(aOther, DontConsiderDocumentDomain);
}

inline bool BasePrincipal::FastSubsumesConsideringDomain(nsIPrincipal* aOther) {
  MOZ_ASSERT(aOther);

  // If neither of the principals have document.domain set, we hand off to
  // FastSubsumes() which has fast paths for some special cases. Otherwise, we
  // fall back to the slow path below.
  if (!mHasExplicitDomain && !Cast(aOther)->mHasExplicitDomain) {
    return FastSubsumes(aOther);
  }

  return Subsumes(aOther, ConsiderDocumentDomain);
}

inline bool BasePrincipal::FastSubsumesIgnoringFPD(
    nsIPrincipal* aOther, DocumentDomainConsideration aConsideration) {
  MOZ_ASSERT(aOther);

  if (Kind() == eContentPrincipal &&
      !dom::ChromeUtils::IsOriginAttributesEqualIgnoringFPD(
          mOriginAttributes, Cast(aOther)->mOriginAttributes)) {
    return false;
  }

  return SubsumesInternal(aOther, aConsideration);
}

inline bool BasePrincipal::FastSubsumesIgnoringFPD(nsIPrincipal* aOther) {
  return FastSubsumesIgnoringFPD(aOther, DontConsiderDocumentDomain);
}

inline bool BasePrincipal::FastSubsumesConsideringDomainIgnoringFPD(
    nsIPrincipal* aOther) {
  return FastSubsumesIgnoringFPD(aOther, ConsiderDocumentDomain);
}

inline bool BasePrincipal::IsSystemPrincipal() const {
  return Kind() == eSystemPrincipal;
}

}  // namespace mozilla

inline bool nsIPrincipal::IsSystemPrincipal() const {
  return mozilla::BasePrincipal::Cast(this)->IsSystemPrincipal();
}

#endif /* mozilla_BasePrincipal_h */
