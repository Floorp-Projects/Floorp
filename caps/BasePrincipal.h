/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_BasePrincipal_h
#define mozilla_BasePrincipal_h

#include "nsJSPrincipals.h"

#include "mozilla/Attributes.h"
#include "mozilla/dom/ChromeUtilsBinding.h"
#include "nsIScriptSecurityManager.h"

class nsIContentSecurityPolicy;
class nsIObjectOutputStream;
class nsIObjectInputStream;
class nsIURI;

class nsExpandedPrincipal;

namespace mozilla {

// Base OriginAttributes class. This has several subclass flavors, and is not
// directly constructable itself.
class OriginAttributes : public dom::OriginAttributesDictionary
{
public:
  OriginAttributes() {}

  OriginAttributes(uint32_t aAppId, bool aInIsolatedMozBrowser)
  {
    mAppId = aAppId;
    mInIsolatedMozBrowser = aInIsolatedMozBrowser;
  }

  explicit OriginAttributes(const OriginAttributesDictionary& aOther)
    : OriginAttributesDictionary(aOther)
  {}

  // This method 'clones' the OriginAttributes ignoring the addonId value becaue
  // this is computed from the principal URI and never propagated.
  void Inherit(const OriginAttributes& aAttrs);

  void SetFirstPartyDomain(const bool aIsTopLevelDocument, nsIURI* aURI);

  enum {
    STRIP_FIRST_PARTY_DOMAIN = 0x01,
    STRIP_ADDON_ID = 0x02,
    STRIP_USER_CONTEXT_ID = 0x04,
  };

  inline void StripAttributes(uint32_t aFlags)
  {
    if (aFlags & STRIP_FIRST_PARTY_DOMAIN) {
      mFirstPartyDomain.Truncate();
    }

    if (aFlags & STRIP_ADDON_ID) {
      mAddonId.Truncate();
    }

    if (aFlags & STRIP_USER_CONTEXT_ID) {
      mUserContextId = nsIScriptSecurityManager::DEFAULT_USER_CONTEXT_ID;
    }
  }

  bool operator==(const OriginAttributes& aOther) const
  {
    return mAppId == aOther.mAppId &&
           mInIsolatedMozBrowser == aOther.mInIsolatedMozBrowser &&
           mAddonId == aOther.mAddonId &&
           mUserContextId == aOther.mUserContextId &&
           mPrivateBrowsingId == aOther.mPrivateBrowsingId &&
           mFirstPartyDomain == aOther.mFirstPartyDomain;
  }

  bool operator!=(const OriginAttributes& aOther) const
  {
    return !(*this == aOther);
  }

  // Serializes/Deserializes non-default values into the suffix format, i.e.
  // |!key1=value1&key2=value2|. If there are no non-default attributes, this
  // returns an empty string.
  void CreateSuffix(nsACString& aStr) const;

  // Don't use this method for anything else than debugging!
  void CreateAnonymizedSuffix(nsACString& aStr) const;

  MOZ_MUST_USE bool PopulateFromSuffix(const nsACString& aStr);

  // Populates the attributes from a string like
  // |uri!key1=value1&key2=value2| and returns the uri without the suffix.
  MOZ_MUST_USE bool PopulateFromOrigin(const nsACString& aOrigin,
                                       nsACString& aOriginNoSuffix);

  // Helper function to match mIsPrivateBrowsing to existing private browsing
  // flags. Once all other flags are removed, this can be removed too.
  void SyncAttributesWithPrivateBrowsing(bool aInPrivateBrowsing);

  // check if "privacy.firstparty.isolate" is enabled.
  static bool IsFirstPartyEnabled();

  // check if the access of window.opener across different FPDs is restricted.
  // We only restrict the access of window.opener when first party isolation
  // is enabled and "privacy.firstparty.isolate.restrict_opener_access" is on.
  static bool IsRestrictOpenerAccessForFPI();

  // returns true if the originAttributes suffix has mPrivateBrowsingId value
  // different than 0.
  static bool IsPrivateBrowsing(const nsACString& aOrigin);
};

class OriginAttributesPattern : public dom::OriginAttributesPatternDictionary
{
public:
  // To convert a JSON string to an OriginAttributesPattern, do the following:
  //
  // OriginAttributesPattern pattern;
  // if (!pattern.Init(aJSONString)) {
  //   ... // handle failure.
  // }
  OriginAttributesPattern() {}

  explicit OriginAttributesPattern(const OriginAttributesPatternDictionary& aOther)
    : OriginAttributesPatternDictionary(aOther) {}

  // Performs a match of |aAttrs| against this pattern.
  bool Matches(const OriginAttributes& aAttrs) const
  {
    if (mAppId.WasPassed() && mAppId.Value() != aAttrs.mAppId) {
      return false;
    }

    if (mInIsolatedMozBrowser.WasPassed() && mInIsolatedMozBrowser.Value() != aAttrs.mInIsolatedMozBrowser) {
      return false;
    }

    if (mAddonId.WasPassed() && mAddonId.Value() != aAttrs.mAddonId) {
      return false;
    }

    if (mUserContextId.WasPassed() && mUserContextId.Value() != aAttrs.mUserContextId) {
      return false;
    }

    if (mPrivateBrowsingId.WasPassed() && mPrivateBrowsingId.Value() != aAttrs.mPrivateBrowsingId) {
      return false;
    }

    if (mFirstPartyDomain.WasPassed() && mFirstPartyDomain.Value() != aAttrs.mFirstPartyDomain) {
      return false;
    }

    return true;
  }

  bool Overlaps(const OriginAttributesPattern& aOther) const
  {
    if (mAppId.WasPassed() && aOther.mAppId.WasPassed() &&
        mAppId.Value() != aOther.mAppId.Value()) {
      return false;
    }

    if (mInIsolatedMozBrowser.WasPassed() &&
        aOther.mInIsolatedMozBrowser.WasPassed() &&
        mInIsolatedMozBrowser.Value() != aOther.mInIsolatedMozBrowser.Value()) {
      return false;
    }

    if (mAddonId.WasPassed() && aOther.mAddonId.WasPassed() &&
        mAddonId.Value() != aOther.mAddonId.Value()) {
      return false;
    }

    if (mUserContextId.WasPassed() && aOther.mUserContextId.WasPassed() &&
        mUserContextId.Value() != aOther.mUserContextId.Value()) {
      return false;
    }

    if (mPrivateBrowsingId.WasPassed() && aOther.mPrivateBrowsingId.WasPassed() &&
        mPrivateBrowsingId.Value() != aOther.mPrivateBrowsingId.Value()) {
      return false;
    }

    if (mFirstPartyDomain.WasPassed() && aOther.mFirstPartyDomain.WasPassed() &&
        mFirstPartyDomain.Value() != aOther.mFirstPartyDomain.Value()) {
      return false;
    }

    return true;
  }
};

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
  BasePrincipal();

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
  NS_IMETHOD GetAddonId(nsAString& aAddonId) final;
  NS_IMETHOD GetIsInIsolatedMozBrowserElement(bool* aIsInIsolatedMozBrowserElement) final;
  NS_IMETHOD GetUnknownAppId(bool* aUnknownAppId) final;
  NS_IMETHOD GetUserContextId(uint32_t* aUserContextId) final;
  NS_IMETHOD GetPrivateBrowsingId(uint32_t* aPrivateBrowsingId) final;

  bool EqualsIgnoringAddonId(nsIPrincipal *aOther);

  virtual bool AddonHasPermission(const nsAString& aPerm);

  virtual bool IsCodebasePrincipal() const { return false; };

  static BasePrincipal* Cast(nsIPrincipal* aPrin) { return static_cast<BasePrincipal*>(aPrin); }
  static already_AddRefed<BasePrincipal>
  CreateCodebasePrincipal(nsIURI* aURI, const OriginAttributes& aAttrs);
  static already_AddRefed<BasePrincipal> CreateCodebasePrincipal(const nsACString& aOrigin);

  const OriginAttributes& OriginAttributesRef() override { return mOriginAttributes; }
  uint32_t AppId() const { return mOriginAttributes.mAppId; }
  uint32_t UserContextId() const { return mOriginAttributes.mUserContextId; }
  uint32_t PrivateBrowsingId() const { return mOriginAttributes.mPrivateBrowsingId; }
  bool IsInIsolatedMozBrowserElement() const { return mOriginAttributes.mInIsolatedMozBrowser; }

  enum PrincipalKind {
    eNullPrincipal,
    eCodebasePrincipal,
    eExpandedPrincipal,
    eSystemPrincipal
  };

  virtual PrincipalKind Kind() = 0;

  already_AddRefed<BasePrincipal> CloneStrippingUserContextIdAndFirstPartyDomain();

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
  friend class ::nsExpandedPrincipal;

  // Helper to check whether this principal is associated with an addon that
  // allows unprivileged code to load aURI.
  bool AddonAllowsLoad(nsIURI* aURI);

  nsCOMPtr<nsIContentSecurityPolicy> mCSP;
  nsCOMPtr<nsIContentSecurityPolicy> mPreloadCSP;
  OriginAttributes mOriginAttributes;
};

} // namespace mozilla

#endif /* mozilla_BasePrincipal_h */
