/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_quota_InitializationTypes_h
#define mozilla_dom_quota_InitializationTypes_h

#include <cstdint>
#include <utility>
#include "ErrorList.h"
#include "mozilla/TypedEnumBits.h"

namespace mozilla {
namespace dom {
namespace quota {

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

MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(Initialization)

class InitializationInfo final {
  Initialization mFirstInitializationAttempts = Initialization::None;

 public:
  class FirstInitializationAttemptImpl {
    InitializationInfo& mOwner;
    const Initialization mInitialization;

   public:
    FirstInitializationAttemptImpl(InitializationInfo& aOwner,
                                   const Initialization aInitialization)
        : mOwner(aOwner), mInitialization(aInitialization) {}

    bool Recorded() const {
      return mOwner.FirstInitializationAttemptRecorded(mInitialization);
    }

    bool Pending() const {
      return mOwner.FirstInitializationAttemptPending(mInitialization);
    }

    void Record(const nsresult aRv) const {
      mOwner.RecordFirstInitializationAttempt(mInitialization, aRv);
    }

    void MaybeRecord(const nsresult aRv) const {
      mOwner.MaybeRecordFirstInitializationAttempt(mInitialization, aRv);
    }
  };

  FirstInitializationAttemptImpl FirstInitializationAttempt(
      const Initialization aInitialization) {
    return FirstInitializationAttemptImpl(*this, aInitialization);
  }

  bool FirstInitializationAttemptRecorded(
      const Initialization aInitialization) const {
    return static_cast<bool>(mFirstInitializationAttempts & aInitialization);
  }

  bool FirstInitializationAttemptPending(
      const Initialization aInitialization) const {
    return !(mFirstInitializationAttempts & aInitialization);
  }

  void RecordFirstInitializationAttempt(const Initialization aInitialization,
                                        nsresult aRv);

  void MaybeRecordFirstInitializationAttempt(
      const Initialization aInitialization, const nsresult aRv) {
    if (FirstInitializationAttemptRecorded(aInitialization)) {
      return;
    }

    RecordFirstInitializationAttempt(aInitialization, aRv);
  }

  void ResetFirstInitializationAttempts() {
    mFirstInitializationAttempts = Initialization::None;
  }
};

}  // namespace quota
}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_quota_InitializationTypes_h
