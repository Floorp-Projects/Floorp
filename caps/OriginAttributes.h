/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_OriginAttributes_h
#define mozilla_OriginAttributes_h

#include "mozilla/dom/ChromeUtils.h"
#include "mozilla/dom/ChromeUtilsBinding.h"
#include "nsIScriptSecurityManager.h"

namespace mozilla {

class OriginAttributes : public dom::OriginAttributesDictionary {
 public:
  OriginAttributes() {}

  explicit OriginAttributes(bool aInIsolatedMozBrowser) {
    mInIsolatedMozBrowser = aInIsolatedMozBrowser;
  }

  explicit OriginAttributes(const OriginAttributesDictionary& aOther)
      : OriginAttributesDictionary(aOther) {}

  void SetFirstPartyDomain(const bool aIsTopLevelDocument, nsIURI* aURI,
                           bool aForced = false);
  void SetFirstPartyDomain(const bool aIsTopLevelDocument,
                           const nsACString& aDomain);

  enum {
    STRIP_FIRST_PARTY_DOMAIN = 0x01,
    STRIP_USER_CONTEXT_ID = 0x02,
  };

  inline void StripAttributes(uint32_t aFlags) {
    if (aFlags & STRIP_FIRST_PARTY_DOMAIN) {
      mFirstPartyDomain.Truncate();
    }

    if (aFlags & STRIP_USER_CONTEXT_ID) {
      mUserContextId = nsIScriptSecurityManager::DEFAULT_USER_CONTEXT_ID;
    }
  }

  bool operator==(const OriginAttributes& aOther) const {
    return mInIsolatedMozBrowser == aOther.mInIsolatedMozBrowser &&
           mUserContextId == aOther.mUserContextId &&
           mPrivateBrowsingId == aOther.mPrivateBrowsingId &&
           mFirstPartyDomain == aOther.mFirstPartyDomain;
  }

  bool operator!=(const OriginAttributes& aOther) const {
    return !(*this == aOther);
  }

  MOZ_MUST_USE bool EqualsIgnoringFPD(const OriginAttributes& aOther) const {
    return mInIsolatedMozBrowser == aOther.mInIsolatedMozBrowser &&
           mUserContextId == aOther.mUserContextId &&
           mPrivateBrowsingId == aOther.mPrivateBrowsingId;
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
  static inline bool IsFirstPartyEnabled() { return sFirstPartyIsolation; }

  // check if the access of window.opener across different FPDs is restricted.
  // We only restrict the access of window.opener when first party isolation
  // is enabled and "privacy.firstparty.isolate.restrict_opener_access" is on.
  static inline bool IsRestrictOpenerAccessForFPI() {
    // We always want to restrict window.opener if first party isolation is
    // disabled.
    return !sFirstPartyIsolation || sRestrictedOpenerAccess;
  }

  // Check whether we block the postMessage across different FPDs when the
  // targetOrigin is '*'.
  static inline MOZ_MUST_USE bool IsBlockPostMessageForFPI() {
    return sFirstPartyIsolation && sBlockPostMessageForFPI;
  }

  // returns true if the originAttributes suffix has mPrivateBrowsingId value
  // different than 0.
  static bool IsPrivateBrowsing(const nsACString& aOrigin);

  static void InitPrefs();

 private:
  static bool sFirstPartyIsolation;
  static bool sRestrictedOpenerAccess;
  static bool sBlockPostMessageForFPI;
};

class OriginAttributesPattern : public dom::OriginAttributesPatternDictionary {
 public:
  // To convert a JSON string to an OriginAttributesPattern, do the following:
  //
  // OriginAttributesPattern pattern;
  // if (!pattern.Init(aJSONString)) {
  //   ... // handle failure.
  // }
  OriginAttributesPattern() {}

  explicit OriginAttributesPattern(
      const OriginAttributesPatternDictionary& aOther)
      : OriginAttributesPatternDictionary(aOther) {}

  // Performs a match of |aAttrs| against this pattern.
  bool Matches(const OriginAttributes& aAttrs) const {
    if (mInIsolatedMozBrowser.WasPassed() &&
        mInIsolatedMozBrowser.Value() != aAttrs.mInIsolatedMozBrowser) {
      return false;
    }

    if (mUserContextId.WasPassed() &&
        mUserContextId.Value() != aAttrs.mUserContextId) {
      return false;
    }

    if (mPrivateBrowsingId.WasPassed() &&
        mPrivateBrowsingId.Value() != aAttrs.mPrivateBrowsingId) {
      return false;
    }

    if (mFirstPartyDomain.WasPassed() &&
        mFirstPartyDomain.Value() != aAttrs.mFirstPartyDomain) {
      return false;
    }

    return true;
  }

  bool Overlaps(const OriginAttributesPattern& aOther) const {
    if (mInIsolatedMozBrowser.WasPassed() &&
        aOther.mInIsolatedMozBrowser.WasPassed() &&
        mInIsolatedMozBrowser.Value() != aOther.mInIsolatedMozBrowser.Value()) {
      return false;
    }

    if (mUserContextId.WasPassed() && aOther.mUserContextId.WasPassed() &&
        mUserContextId.Value() != aOther.mUserContextId.Value()) {
      return false;
    }

    if (mPrivateBrowsingId.WasPassed() &&
        aOther.mPrivateBrowsingId.WasPassed() &&
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

}  // namespace mozilla

#endif /* mozilla_OriginAttributes_h */
