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

namespace mozilla {
namespace dom {
class Document;
}
namespace extensions {
class WebExtensionPolicy;
}

class BasePrincipal;

// Codebase principals (and codebase principals embedded within expanded
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
  enum PrincipalKind {
    eNullPrincipal,
    eCodebasePrincipal,
    eExpandedPrincipal,
    eSystemPrincipal
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
  NS_IMETHOD GetOriginNoSuffix(nsACString& aOrigin) final;
  NS_IMETHOD Equals(nsIPrincipal* other, bool* _retval) final;
  NS_IMETHOD EqualsConsideringDomain(nsIPrincipal* other, bool* _retval) final;
  NS_IMETHOD Subsumes(nsIPrincipal* other, bool* _retval) final;
  NS_IMETHOD SubsumesConsideringDomain(nsIPrincipal* other,
                                       bool* _retval) final;
  NS_IMETHOD SubsumesConsideringDomainIgnoringFPD(nsIPrincipal* other,
                                                  bool* _retval) final;
  NS_IMETHOD CheckMayLoad(nsIURI* uri, bool report,
                          bool allowIfInheritsPrincipal) final;
  NS_IMETHOD GetAddonPolicy(nsISupports** aResult) final;
  NS_IMETHOD GetCsp(nsIContentSecurityPolicy** aCsp) override;
  NS_IMETHOD SetCsp(nsIContentSecurityPolicy* aCsp) override;
  NS_IMETHOD EnsureCSP(dom::Document* aDocument,
                       nsIContentSecurityPolicy** aCSP) override;
  NS_IMETHOD GetPreloadCsp(nsIContentSecurityPolicy** aPreloadCSP) override;
  NS_IMETHOD EnsurePreloadCSP(dom::Document* aDocument,
                              nsIContentSecurityPolicy** aCSP) override;
  NS_IMETHOD GetCspJSON(nsAString& outCSPinJSON) override;
  NS_IMETHOD GetIsNullPrincipal(bool* aResult) override;
  NS_IMETHOD GetIsCodebasePrincipal(bool* aResult) override;
  NS_IMETHOD GetIsExpandedPrincipal(bool* aResult) override;
  NS_IMETHOD GetIsSystemPrincipal(bool* aResult) override;
  NS_IMETHOD GetIsAddonOrExpandedAddonPrincipal(bool* aResult) override;
  NS_IMETHOD GetOriginAttributes(JSContext* aCx,
                                 JS::MutableHandle<JS::Value> aVal) final;
  NS_IMETHOD GetOriginSuffix(nsACString& aOriginSuffix) final;
  NS_IMETHOD GetIsInIsolatedMozBrowserElement(
      bool* aIsInIsolatedMozBrowserElement) final;
  NS_IMETHOD GetUserContextId(uint32_t* aUserContextId) final;
  NS_IMETHOD GetPrivateBrowsingId(uint32_t* aPrivateBrowsingId) final;
  NS_IMETHOD GetSiteOrigin(nsACString& aOrigin) override;

  virtual bool AddonHasPermission(const nsAtom* aPerm);

  virtual bool IsCodebasePrincipal() const { return false; };

  static BasePrincipal* Cast(nsIPrincipal* aPrin) {
    return static_cast<BasePrincipal*>(aPrin);
  }

  static const BasePrincipal* Cast(const nsIPrincipal* aPrin) {
    return static_cast<const BasePrincipal*>(aPrin);
  }

  static already_AddRefed<BasePrincipal> CreateCodebasePrincipal(
      const nsACString& aOrigin);

  // These following method may not create a codebase principal in case it's
  // not possible to generate a correct origin from the passed URI. If this
  // happens, a NullPrincipal is returned.

  static already_AddRefed<BasePrincipal> CreateCodebasePrincipal(
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

  already_AddRefed<BasePrincipal> CloneForcingFirstPartyDomain(nsIURI* aURI);

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

  /**
   * Returns true if this principal's CSP should override a document's CSP for
   * loads that it triggers. Currently true for system principal, for expanded
   * principals which subsume the document principal, and add-on codebase
   * principals regardless of whether they subsume the document principal.
   */
  bool OverridesCSP(nsIPrincipal* aDocumentPrincipal) {
    // SystemPrincipal can override the page's CSP by definition.
    if (mKind == eSystemPrincipal) {
      return true;
    }

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

  void SetHasExplicitDomain() { mHasExplicitDomain = true; }

  // Either of these functions should be called as the last step of the
  // initialization of the principal objects.  It's typically called as the
  // last step from the Init() method of the child classes.
  void FinishInit(const nsACString& aOriginNoSuffix,
                  const OriginAttributes& aOriginAttributes);
  void FinishInit(BasePrincipal* aOther,
                  const OriginAttributes& aOriginAttributes);

  nsCOMPtr<nsIContentSecurityPolicy> mCSP;
  nsCOMPtr<nsIContentSecurityPolicy> mPreloadCSP;

 private:
  static already_AddRefed<BasePrincipal> CreateCodebasePrincipal(
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
  auto other = Cast(aOther);
  if (Kind() != other->Kind()) {
    // Principals of different kinds can't be equal.
    return false;
  }

  // Two principals are considered to be equal if their origins are the same.
  // If the two principals are codebase principals, their origin attributes
  // (aka the origin suffix) must also match.
  if (Kind() == eSystemPrincipal) {
    return this == other;
  }

  if (Kind() == eCodebasePrincipal || Kind() == eNullPrincipal) {
    return mOriginNoSuffix == other->mOriginNoSuffix &&
           mOriginSuffix == other->mOriginSuffix;
  }

  MOZ_ASSERT(Kind() == eExpandedPrincipal);
  return mOriginNoSuffix == other->mOriginNoSuffix;
}

inline bool BasePrincipal::FastEqualsConsideringDomain(nsIPrincipal* aOther) {
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
  // If two principals are equal, then they both subsume each other.
  if (FastEquals(aOther)) {
    return true;
  }

  // Otherwise, fall back to the slow path.
  return Subsumes(aOther, DontConsiderDocumentDomain);
}

inline bool BasePrincipal::FastSubsumesConsideringDomain(nsIPrincipal* aOther) {
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
  if (Kind() == eCodebasePrincipal &&
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
