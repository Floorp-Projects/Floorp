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

class nsIContentSecurityPolicy;
class nsIObjectOutputStream;
class nsIObjectInputStream;
class nsIURI;

class ExpandedPrincipal;

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
  enum PrincipalKind {
    eNullPrincipal,
    eCodebasePrincipal,
    eExpandedPrincipal,
    eSystemPrincipal
  };

  explicit BasePrincipal(PrincipalKind aKind);

  enum DocumentDomainConsideration { DontConsiderDocumentDomain, ConsiderDocumentDomain};
  bool Subsumes(nsIPrincipal* aOther, DocumentDomainConsideration aConsideration);

  NS_IMETHOD GetOrigin(nsACString& aOrigin) final;
  NS_IMETHOD GetOriginNoSuffix(nsACString& aOrigin) final;
  NS_IMETHOD Equals(nsIPrincipal* other, bool* _retval) final;
  NS_IMETHOD EqualsConsideringDomain(nsIPrincipal* other, bool* _retval) final;
  NS_IMETHOD Subsumes(nsIPrincipal* other, bool* _retval) final;
  NS_IMETHOD SubsumesConsideringDomain(nsIPrincipal* other, bool* _retval) final;
  NS_IMETHOD SubsumesConsideringDomainIgnoringFPD(nsIPrincipal* other, bool* _retval) final;
  NS_IMETHOD CheckMayLoad(nsIURI* uri, bool report, bool allowIfInheritsPrincipal) final;
  NS_IMETHOD GetCsp(nsIContentSecurityPolicy** aCsp) override;
  NS_IMETHOD SetCsp(nsIContentSecurityPolicy* aCsp) override;
  NS_IMETHOD EnsureCSP(nsIDOMDocument* aDocument, nsIContentSecurityPolicy** aCSP) override;
  NS_IMETHOD GetPreloadCsp(nsIContentSecurityPolicy** aPreloadCSP) override;
  NS_IMETHOD EnsurePreloadCSP(nsIDOMDocument* aDocument, nsIContentSecurityPolicy** aCSP) override;
  NS_IMETHOD GetCspJSON(nsAString& outCSPinJSON) override;
  NS_IMETHOD GetIsNullPrincipal(bool* aResult) override;
  NS_IMETHOD GetIsCodebasePrincipal(bool* aResult) override;
  NS_IMETHOD GetIsExpandedPrincipal(bool* aResult) override;
  NS_IMETHOD GetIsSystemPrincipal(bool* aResult) override;
  NS_IMETHOD GetOriginAttributes(JSContext* aCx, JS::MutableHandle<JS::Value> aVal) final;
  NS_IMETHOD GetOriginSuffix(nsACString& aOriginSuffix) final;
  NS_IMETHOD GetAppStatus(uint16_t* aAppStatus) final;
  NS_IMETHOD GetAppId(uint32_t* aAppStatus) final;
  NS_IMETHOD GetIsInIsolatedMozBrowserElement(bool* aIsInIsolatedMozBrowserElement) final;
  NS_IMETHOD GetUnknownAppId(bool* aUnknownAppId) final;
  NS_IMETHOD GetUserContextId(uint32_t* aUserContextId) final;
  NS_IMETHOD GetPrivateBrowsingId(uint32_t* aPrivateBrowsingId) final;

  virtual bool AddonHasPermission(const nsAString& aPerm);

  virtual bool IsCodebasePrincipal() const { return false; };

  static BasePrincipal* Cast(nsIPrincipal* aPrin) { return static_cast<BasePrincipal*>(aPrin); }
  static already_AddRefed<BasePrincipal>
  CreateCodebasePrincipal(nsIURI* aURI, const OriginAttributes& aAttrs);
  static already_AddRefed<BasePrincipal> CreateCodebasePrincipal(const nsACString& aOrigin);

  const OriginAttributes& OriginAttributesRef() final { return mOriginAttributes; }
  uint32_t AppId() const { return mOriginAttributes.mAppId; }
  uint32_t UserContextId() const { return mOriginAttributes.mUserContextId; }
  uint32_t PrivateBrowsingId() const { return mOriginAttributes.mPrivateBrowsingId; }
  bool IsInIsolatedMozBrowserElement() const { return mOriginAttributes.mInIsolatedMozBrowser; }

  PrincipalKind Kind() const { return mKind; }

  already_AddRefed<BasePrincipal> CloneStrippingUserContextIdAndFirstPartyDomain();

  // Helper to check whether this principal is associated with an addon that
  // allows unprivileged code to load aURI.  aExplicit == true will prevent
  // use of all_urls permission, requiring the domain in its permissions.
  bool AddonAllowsLoad(nsIURI* aURI, bool aExplicit = false);

  // Call these to avoid the cost of virtual dispatch.
  inline bool FastEquals(nsIPrincipal* aOther);
  inline bool FastEqualsConsideringDomain(nsIPrincipal* aOther);
  inline bool FastSubsumes(nsIPrincipal* aOther);
  inline bool FastSubsumesConsideringDomain(nsIPrincipal* aOther);
  inline bool FastSubsumesConsideringDomainIgnoringFPD(nsIPrincipal* aOther);

protected:
  virtual ~BasePrincipal();

  virtual nsresult GetOriginInternal(nsACString& aOrigin) = 0;
  // Note that this does not check OriginAttributes. Callers that depend on
  // those must call Subsumes instead.
  virtual bool SubsumesInternal(nsIPrincipal* aOther, DocumentDomainConsideration aConsider) = 0;

  // Internal, side-effect-free check to determine whether the concrete
  // principal would allow the load ignoring any common behavior implemented in
  // BasePrincipal::CheckMayLoad.
  virtual bool MayLoadInternal(nsIURI* aURI) = 0;
  friend class ::ExpandedPrincipal;

  // This function should be called as the last step of the initialization of the
  // principal objects.  It's typically called as the last step from the Init()
  // method of the child classes.
  void FinishInit();

  nsCOMPtr<nsIContentSecurityPolicy> mCSP;
  nsCOMPtr<nsIContentSecurityPolicy> mPreloadCSP;
  nsCOMPtr<nsIAtom> mOriginNoSuffix;
  nsCOMPtr<nsIAtom> mOriginSuffix;
  OriginAttributes mOriginAttributes;
  PrincipalKind mKind;
  bool mDomainSet;
};

inline bool
BasePrincipal::FastEquals(nsIPrincipal* aOther)
{
  auto other = Cast(aOther);
  if (Kind() != other->Kind()) {
    // Principals of different kinds can't be equal.
    return false;
  }

  // Two principals are considered to be equal if their origins are the same.
  // If the two principals are codebase principals, their origin attributes
  // (aka the origin suffix) must also match.
  // If the two principals are null principals, they're only equal if they're
  // the same object.
  if (Kind() == eNullPrincipal || Kind() == eSystemPrincipal) {
    return this == other;
  }

  if (mOriginNoSuffix) {
    if (Kind() == eCodebasePrincipal) {
      return mOriginNoSuffix == other->mOriginNoSuffix &&
             mOriginSuffix == other->mOriginSuffix;
    }

    MOZ_ASSERT(Kind() == eExpandedPrincipal);
    return mOriginNoSuffix == other->mOriginNoSuffix;
  }

  // If mOriginNoSuffix is null on one of our principals, we must fall back
  // to the slow path.
  return Subsumes(aOther, DontConsiderDocumentDomain) &&
         other->Subsumes(this, DontConsiderDocumentDomain);
}

inline bool
BasePrincipal::FastEqualsConsideringDomain(nsIPrincipal* aOther)
{
  // If neither of the principals have document.domain set, we use the fast path
  // in Equals().  Otherwise, we fall back to the slow path below.
  auto other = Cast(aOther);
  if (!mDomainSet && !other->mDomainSet) {
    return FastEquals(aOther);
  }

  return Subsumes(aOther, ConsiderDocumentDomain) &&
         other->Subsumes(this, ConsiderDocumentDomain);
}

inline bool
BasePrincipal::FastSubsumes(nsIPrincipal* aOther)
{
  // If two principals are equal, then they both subsume each other.
  // We deal with two special cases first:
  // Null principals only subsume each other if they are equal, and are only
  // equal if they're the same object.
  // Also, if mOriginNoSuffix is null, FastEquals falls back to the slow path
  // using Subsumes, so we don't want to use it in that case to avoid an
  // infinite recursion.
  auto other = Cast(aOther);
  if (Kind() == eNullPrincipal && other->Kind() == eNullPrincipal) {
    return this == other;
  }
  if (mOriginNoSuffix && FastEquals(aOther)) {
    return true;
  }

  // Otherwise, fall back to the slow path.
  return Subsumes(aOther, DontConsiderDocumentDomain);
}

inline bool
BasePrincipal::FastSubsumesConsideringDomain(nsIPrincipal* aOther)
{
  // If neither of the principals have document.domain set, we hand off to
  // FastSubsumes() which has fast paths for some special cases. Otherwise, we fall
  // back to the slow path below.
  if (!mDomainSet && !Cast(aOther)->mDomainSet) {
    return FastSubsumes(aOther);
  }

  return Subsumes(aOther, ConsiderDocumentDomain);
}

inline bool
BasePrincipal::FastSubsumesConsideringDomainIgnoringFPD(nsIPrincipal* aOther)
{
  if (Kind() == eCodebasePrincipal &&
      !dom::ChromeUtils::IsOriginAttributesEqualIgnoringFPD(
            mOriginAttributes, Cast(aOther)->mOriginAttributes)) {
    return false;
  }

 return SubsumesInternal(aOther, ConsiderDocumentDomain);
}

} // namespace mozilla

#endif /* mozilla_BasePrincipal_h */
