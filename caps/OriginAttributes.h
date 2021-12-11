/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_OriginAttributes_h
#define mozilla_OriginAttributes_h

#include "mozilla/dom/ChromeUtilsBinding.h"
#include "mozilla/StaticPrefs_privacy.h"
#include "nsIScriptSecurityManager.h"

namespace mozilla {

class OriginAttributes : public dom::OriginAttributesDictionary {
 public:
  OriginAttributes() = default;

  explicit OriginAttributes(bool aInIsolatedMozBrowser) {
    mInIsolatedMozBrowser = aInIsolatedMozBrowser;
  }

  explicit OriginAttributes(const OriginAttributesDictionary& aOther)
      : OriginAttributesDictionary(aOther) {}

  void SetFirstPartyDomain(const bool aIsTopLevelDocument, nsIURI* aURI,
                           bool aForced = false);
  void SetFirstPartyDomain(const bool aIsTopLevelDocument,
                           const nsACString& aDomain);
  void SetFirstPartyDomain(const bool aIsTopLevelDocument,
                           const nsAString& aDomain, bool aForced = false);

  void SetPartitionKey(nsIURI* aURI);
  void SetPartitionKey(const nsACString& aDomain);
  void SetPartitionKey(const nsAString& aDomain);

  enum {
    STRIP_FIRST_PARTY_DOMAIN = 0x01,
    STRIP_USER_CONTEXT_ID = 0x02,
    STRIP_PRIVATE_BROWSING_ID = 0x04,
    STRIP_PARITION_KEY = 0x08,
  };

  inline void StripAttributes(uint32_t aFlags) {
    if (aFlags & STRIP_FIRST_PARTY_DOMAIN) {
      mFirstPartyDomain.Truncate();
    }

    if (aFlags & STRIP_USER_CONTEXT_ID) {
      mUserContextId = nsIScriptSecurityManager::DEFAULT_USER_CONTEXT_ID;
    }

    if (aFlags & STRIP_PRIVATE_BROWSING_ID) {
      mPrivateBrowsingId =
          nsIScriptSecurityManager::DEFAULT_PRIVATE_BROWSING_ID;
    }

    if (aFlags & STRIP_PARITION_KEY) {
      mPartitionKey.Truncate();
    }
  }

  bool operator==(const OriginAttributes& aOther) const {
    return EqualsIgnoringFPD(aOther) &&
           mFirstPartyDomain == aOther.mFirstPartyDomain &&
           // FIXME(emilio, bug 1667440): Should this be part of
           // EqualsIgnoringFPD instead?
           mPartitionKey == aOther.mPartitionKey;
  }

  bool operator!=(const OriginAttributes& aOther) const {
    return !(*this == aOther);
  }

  [[nodiscard]] bool EqualsIgnoringFPD(const OriginAttributes& aOther) const {
    return mInIsolatedMozBrowser == aOther.mInIsolatedMozBrowser &&
           mUserContextId == aOther.mUserContextId &&
           mPrivateBrowsingId == aOther.mPrivateBrowsingId &&
           mGeckoViewSessionContextId == aOther.mGeckoViewSessionContextId;
  }

  [[nodiscard]] bool EqualsIgnoringPartitionKey(
      const OriginAttributes& aOther) const {
    return EqualsIgnoringFPD(aOther) &&
           mFirstPartyDomain == aOther.mFirstPartyDomain;
  }

  // Serializes/Deserializes non-default values into the suffix format, i.e.
  // |^key1=value1&key2=value2|. If there are no non-default attributes, this
  // returns an empty string.
  void CreateSuffix(nsACString& aStr) const;

  // Like CreateSuffix, but returns an atom instead of producing a string.
  already_AddRefed<nsAtom> CreateSuffixAtom() const;

  // Don't use this method for anything else than debugging!
  void CreateAnonymizedSuffix(nsACString& aStr) const;

  [[nodiscard]] bool PopulateFromSuffix(const nsACString& aStr);

  // Populates the attributes from a string like
  // |uri^key1=value1&key2=value2| and returns the uri without the suffix.
  [[nodiscard]] bool PopulateFromOrigin(const nsACString& aOrigin,
                                        nsACString& aOriginNoSuffix);

  // Helper function to match mIsPrivateBrowsing to existing private browsing
  // flags. Once all other flags are removed, this can be removed too.
  void SyncAttributesWithPrivateBrowsing(bool aInPrivateBrowsing);

  // check if "privacy.firstparty.isolate" is enabled.
  static inline bool IsFirstPartyEnabled() {
    return StaticPrefs::privacy_firstparty_isolate();
  }

  static inline bool UseSiteForFirstPartyDomain() {
    if (IsFirstPartyEnabled()) {
      return StaticPrefs::privacy_firstparty_isolate_use_site();
    }
    return StaticPrefs::privacy_dynamic_firstparty_use_site();
  }

  // check if the access of window.opener across different FPDs is restricted.
  // We only restrict the access of window.opener when first party isolation
  // is enabled and "privacy.firstparty.isolate.restrict_opener_access" is on.
  static inline bool IsRestrictOpenerAccessForFPI() {
    // We always want to restrict window.opener if first party isolation is
    // disabled.
    return !StaticPrefs::privacy_firstparty_isolate() ||
           StaticPrefs::privacy_firstparty_isolate_restrict_opener_access();
  }

  // Check whether we block the postMessage across different FPDs when the
  // targetOrigin is '*'.
  [[nodiscard]] static inline bool IsBlockPostMessageForFPI() {
    return StaticPrefs::privacy_firstparty_isolate() &&
           StaticPrefs::privacy_firstparty_isolate_block_post_message();
  }

  // returns true if the originAttributes suffix has mPrivateBrowsingId value
  // different than 0.
  static bool IsPrivateBrowsing(const nsACString& aOrigin);

  // Parse a partitionKey of the format "(<scheme>,<baseDomain>,[port])" into
  // its components.
  // Returns false if the partitionKey cannot be parsed because the format is
  // invalid.
  static bool ParsePartitionKey(const nsAString& aPartitionKey,
                                nsAString& outScheme, nsAString& outBaseDomain,
                                int32_t& outPort);
};

class OriginAttributesPattern : public dom::OriginAttributesPatternDictionary {
 public:
  // To convert a JSON string to an OriginAttributesPattern, do the following:
  //
  // OriginAttributesPattern pattern;
  // if (!pattern.Init(aJSONString)) {
  //   ... // handle failure.
  // }
  OriginAttributesPattern() = default;

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

    if (mGeckoViewSessionContextId.WasPassed() &&
        mGeckoViewSessionContextId.Value() !=
            aAttrs.mGeckoViewSessionContextId) {
      return false;
    }

    // If both mPartitionKey and mPartitionKeyPattern are passed, mPartitionKey
    // takes precedence.
    if (mPartitionKey.WasPassed()) {
      if (mPartitionKey.Value() != aAttrs.mPartitionKey) {
        return false;
      }
    } else if (mPartitionKeyPattern.WasPassed()) {
      auto& pkPattern = mPartitionKeyPattern.Value();

      if (pkPattern.mScheme.WasPassed() || pkPattern.mBaseDomain.WasPassed() ||
          pkPattern.mPort.WasPassed()) {
        if (aAttrs.mPartitionKey.IsEmpty()) {
          return false;
        }

        nsString scheme;
        nsString baseDomain;
        int32_t port;
        bool success = OriginAttributes::ParsePartitionKey(
            aAttrs.mPartitionKey, scheme, baseDomain, port);
        if (!success) {
          return false;
        }

        if (pkPattern.mScheme.WasPassed() &&
            pkPattern.mScheme.Value() != scheme) {
          return false;
        }
        if (pkPattern.mBaseDomain.WasPassed() &&
            pkPattern.mBaseDomain.Value() != baseDomain) {
          return false;
        }
        if (pkPattern.mPort.WasPassed() && pkPattern.mPort.Value() != port) {
          return false;
        }
      }
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

    if (mGeckoViewSessionContextId.WasPassed() &&
        aOther.mGeckoViewSessionContextId.WasPassed() &&
        mGeckoViewSessionContextId.Value() !=
            aOther.mGeckoViewSessionContextId.Value()) {
      return false;
    }

    if (mPartitionKey.WasPassed() && aOther.mPartitionKey.WasPassed() &&
        mPartitionKey.Value() != aOther.mPartitionKey.Value()) {
      return false;
    }

    if (mPartitionKeyPattern.WasPassed() &&
        aOther.mPartitionKeyPattern.WasPassed()) {
      auto& self = mPartitionKeyPattern.Value();
      auto& other = aOther.mPartitionKeyPattern.Value();

      if (self.mScheme.WasPassed() && other.mScheme.WasPassed() &&
          self.mScheme.Value() != other.mScheme.Value()) {
        return false;
      }
      if (self.mBaseDomain.WasPassed() && other.mBaseDomain.WasPassed() &&
          self.mBaseDomain.Value() != other.mBaseDomain.Value()) {
        return false;
      }
      if (self.mPort.WasPassed() && other.mPort.WasPassed() &&
          self.mPort.Value() != other.mPort.Value()) {
        return false;
      }
    }

    return true;
  }
};

}  // namespace mozilla

#endif /* mozilla_OriginAttributes_h */
