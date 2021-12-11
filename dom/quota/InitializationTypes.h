/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_QUOTA_INITIALIZATIONTYPES_H_
#define DOM_QUOTA_INITIALIZATIONTYPES_H_

#include "mozilla/TypedEnumBits.h"
#include "mozilla/dom/quota/FirstInitializationAttempts.h"
#include "nsLiteralString.h"
#include "nsStringFwd.h"
#include "nsTHashMap.h"

namespace mozilla {
struct CreateIfNonExistent;
}

namespace mozilla::dom::quota {

enum class Initialization {
  None = 0,
  Storage = 1 << 0,
  TemporaryStorage = 1 << 1,
  DefaultRepository = 1 << 2,
  TemporaryRepository = 1 << 3,
  UpgradeStorageFrom0_0To1_0 = 1 << 4,
  UpgradeStorageFrom1_0To2_0 = 1 << 5,
  UpgradeStorageFrom2_0To2_1 = 1 << 6,
  UpgradeStorageFrom2_1To2_2 = 1 << 7,
  UpgradeStorageFrom2_2To2_3 = 1 << 8,
  UpgradeFromIndexedDBDirectory = 1 << 9,
  UpgradeFromPersistentStorageDirectory = 1 << 10,
};

enum class OriginInitialization {
  None = 0,
  PersistentOrigin = 1 << 0,
  TemporaryOrigin = 1 << 1,
};

MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(Initialization)
MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(OriginInitialization)

class StringGenerator final {
 public:
  // TODO: Use constexpr here once bug 1594094 is addressed.
  static nsLiteralCString GetString(Initialization aInitialization);

  // TODO: Use constexpr here once bug 1594094 is addressed.
  static nsLiteralCString GetString(OriginInitialization aOriginInitialization);
};

using OriginInitializationInfo =
    FirstInitializationAttempts<OriginInitialization, StringGenerator>;

class InitializationInfo
    : public FirstInitializationAttempts<Initialization, StringGenerator> {
  nsTHashMap<nsCStringHashKey, OriginInitializationInfo>
      mOriginInitializationInfos;

 public:
  OriginInitializationInfo& MutableOriginInitializationInfoRef(
      const nsACString& aOrigin) {
    return *mOriginInitializationInfos.Lookup(aOrigin);
  }

  OriginInitializationInfo& MutableOriginInitializationInfoRef(
      const nsACString& aOrigin, const CreateIfNonExistent&) {
    return mOriginInitializationInfos.LookupOrInsert(aOrigin);
  }

  void ResetOriginInitializationInfos() { mOriginInitializationInfos.Clear(); }
};

}  // namespace mozilla::dom::quota

#endif  // DOM_QUOTA_INITIALIZATIONTYPES_H_
