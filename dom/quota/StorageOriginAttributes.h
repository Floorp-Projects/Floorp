/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_QUOTA_STORAGEORIGINATTRIBUTES_H_
#define DOM_QUOTA_STORAGEORIGINATTRIBUTES_H_

#include "mozilla/OriginAttributes.h"

namespace mozilla {

// A class which can handle origin attributes which are not fully supported
// in OriginAttributes class anymore.
class StorageOriginAttributes {
 public:
  StorageOriginAttributes() : mInIsolatedMozBrowser(false) {}

  explicit StorageOriginAttributes(bool aInIsolatedMozBrowser)
      : mInIsolatedMozBrowser(aInIsolatedMozBrowser) {}

  bool InIsolatedMozBrowser() const { return mInIsolatedMozBrowser; }

  uint32_t UserContextId() const { return mOriginAttributes.mUserContextId; }

  // New getters can be added here incrementally.

  void SetInIsolatedMozBrowser(bool aInIsolatedMozBrowser) {
    mInIsolatedMozBrowser = aInIsolatedMozBrowser;
  }

  void SetUserContextId(uint32_t aUserContextId) {
    mOriginAttributes.mUserContextId = aUserContextId;
  }

  // New setters can be added here incrementally.

  // Serializes/Deserializes non-default values into the suffix format, i.e.
  // |^key1=value1&key2=value2|. If there are no non-default attributes, this
  // returns an empty string
  void CreateSuffix(nsACString& aStr) const;

  [[nodiscard]] bool PopulateFromSuffix(const nsACString& aStr);

  // Populates the attributes from a string like
  // |uri^key1=value1&key2=value2| and returns the uri without the suffix.
  [[nodiscard]] bool PopulateFromOrigin(const nsACString& aOrigin,
                                        nsACString& aOriginNoSuffix);

 private:
  OriginAttributes mOriginAttributes;

  bool mInIsolatedMozBrowser;
};

}  // namespace mozilla

#endif  // DOM_QUOTA_STORAGEORIGINATTRIBUTES_H_
