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

#include "mozilla/dom/ChromeUtilsBinding.h"

class nsIContentSecurityPolicy;
class nsILoadContext;
class nsIObjectOutputStream;
class nsIObjectInputStream;

class nsExpandedPrincipal;

namespace mozilla {

class OriginAttributes : public dom::OriginAttributesDictionary
{
public:
  OriginAttributes() {}
  OriginAttributes(uint32_t aAppId, bool aInBrowser)
  {
    mAppId = aAppId;
    mInBrowser = aInBrowser;
  }
  explicit OriginAttributes(const OriginAttributesDictionary& aOther)
    : OriginAttributesDictionary(aOther) {}

  bool operator==(const OriginAttributes& aOther) const
  {
    return mAppId == aOther.mAppId &&
           mInBrowser == aOther.mInBrowser &&
           mAddonId == aOther.mAddonId &&
           mUserContextId == aOther.mUserContextId &&
           mSignedPkg == aOther.mSignedPkg;
  }
  bool operator!=(const OriginAttributes& aOther) const
  {
    return !(*this == aOther);
  }

  // The docshell often influences the origin attributes of content loaded
  // inside of it, and in some cases also influences the origin attributes of
  // content loaded in child docshells. We say that a given attribute "lives on
  // the docshell" to indicate that this attribute is specified by the docshell
  // (if any) associated with a given content document.
  //
  // In practice, this usually means that we need to store a copy of those
  // attributes on each docshell, or provide methods on the docshell to compute
  // them on-demand.
  // We could track each of these attributes individually, but since the
  // majority of the existing origin attributes currently live on the docshell,
  // it's cleaner to simply store an entire OriginAttributes struct on each
  // docshell, and selectively copy them to child docshells and content
  // principals in a manner that implements our desired semantics.
  //
  // This method is used to propagate attributes from parent to child
  // docshells.
  void InheritFromDocShellParent(const OriginAttributes& aParent);

  // Copy from the origin attributes of the nsILoadContext.
  bool CopyFromLoadContext(nsILoadContext* aLoadContext);

  // Serializes/Deserializes non-default values into the suffix format, i.e.
  // |!key1=value1&key2=value2|. If there are no non-default attributes, this
  // returns an empty string.
  void CreateSuffix(nsACString& aStr) const;
  bool PopulateFromSuffix(const nsACString& aStr);

  // Populates the attributes from a string like
  // |uri!key1=value1&key2=value2| and returns the uri without the suffix.
  bool PopulateFromOrigin(const nsACString& aOrigin,
                          nsACString& aOriginNoSuffix);
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

    if (mInBrowser.WasPassed() && mInBrowser.Value() != aAttrs.mInBrowser) {
      return false;
    }

    if (mAddonId.WasPassed() && mAddonId.Value() != aAttrs.mAddonId) {
      return false;
    }

    if (mUserContextId.WasPassed() && mUserContextId.Value() != aAttrs.mUserContextId) {
      return false;
    }

    if (mSignedPkg.WasPassed() && mSignedPkg.Value() != aAttrs.mSignedPkg) {
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
  NS_IMETHOD CheckMayLoad(nsIURI* uri, bool report, bool allowIfInheritsPrincipal) final;
  NS_IMETHOD GetCsp(nsIContentSecurityPolicy** aCsp) override;
  NS_IMETHOD SetCsp(nsIContentSecurityPolicy* aCsp) override;
  NS_IMETHOD GetCspJSON(nsAString& outCSPinJSON) override;
  NS_IMETHOD GetIsNullPrincipal(bool* aIsNullPrincipal) override;
  NS_IMETHOD GetJarPrefix(nsACString& aJarPrefix) final;
  NS_IMETHOD GetOriginAttributes(JSContext* aCx, JS::MutableHandle<JS::Value> aVal) final;
  NS_IMETHOD GetOriginSuffix(nsACString& aOriginSuffix) final;
  NS_IMETHOD GetAppStatus(uint16_t* aAppStatus) final;
  NS_IMETHOD GetAppId(uint32_t* aAppStatus) final;
  NS_IMETHOD GetIsInBrowserElement(bool* aIsInBrowserElement) final;
  NS_IMETHOD GetUnknownAppId(bool* aUnknownAppId) final;
  NS_IMETHOD GetUserContextId(uint32_t* aUserContextId) final;

  virtual bool IsOnCSSUnprefixingWhitelist() override { return false; }

  virtual bool IsCodebasePrincipal() const { return false; };

  static BasePrincipal* Cast(nsIPrincipal* aPrin) { return static_cast<BasePrincipal*>(aPrin); }
  static already_AddRefed<BasePrincipal>
  CreateCodebasePrincipal(nsIURI* aURI, const OriginAttributes& aAttrs);
  static already_AddRefed<BasePrincipal> CreateCodebasePrincipal(const nsACString& aOrigin);

  const OriginAttributes& OriginAttributesRef() { return mOriginAttributes; }
  uint32_t AppId() const { return mOriginAttributes.mAppId; }
  uint32_t UserContextId() const { return mOriginAttributes.mUserContextId; }
  bool IsInBrowserElement() const { return mOriginAttributes.mInBrowser; }

protected:
  virtual ~BasePrincipal();

  virtual nsresult GetOriginInternal(nsACString& aOrigin) = 0;
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
  OriginAttributes mOriginAttributes;
};

} // namespace mozilla

#endif /* mozilla_BasePrincipal_h */
