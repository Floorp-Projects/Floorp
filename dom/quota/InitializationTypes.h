/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_quota_InitializationTypes_h
#define mozilla_dom_quota_InitializationTypes_h

#include "mozilla/Telemetry.h"
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
};

MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(Initialization)

class InitializationInfo final {
  Initialization mInitializationAttempts = Initialization::None;

 public:
  template <typename SuccessFunction>
  class AutoInitializationAttempt {
    InitializationInfo& mOwner;
    const Initialization mInitialization;
    const SuccessFunction mSuccessFunction;

   public:
    AutoInitializationAttempt(InitializationInfo& aOwner,
                              const Initialization aInitialization,
                              SuccessFunction&& aSuccessFunction)
        : mOwner(aOwner),
          mInitialization(aInitialization),
          mSuccessFunction(std::move(aSuccessFunction)) {}

    ~AutoInitializationAttempt() {
      if (mOwner.InitializationAttempted(mInitialization)) {
        return;
      }

      mOwner.ReportFirstInitializationAttempt(mInitialization,
                                              mSuccessFunction());
    }
  };

  template <typename SuccessFunction>
  AutoInitializationAttempt<SuccessFunction> RecordFirstInitializationAttempt(
      const Initialization aInitialization,
      SuccessFunction&& aSuccessFunction) {
    return AutoInitializationAttempt<SuccessFunction>(
        *this, aInitialization, std::move(aSuccessFunction));
  }

  void RecordFirstInitializationAttempt(const Initialization aInitialization,
                                        const nsresult aRv) {
    if (InitializationAttempted(aInitialization)) {
      return;
    }

    ReportFirstInitializationAttempt(aInitialization, NS_SUCCEEDED(aRv));
  }

  void AssertInitializationAttempted(const Initialization aInitialization) {
    MOZ_ASSERT(InitializationAttempted(aInitialization));
  }

  void ResetInitializationAttempts() {
    mInitializationAttempts = Initialization::None;
  }

 private:
  // TODO: Use constexpr here once bug 1594094 is addressed.
  static nsLiteralCString GetInitializationString(
      const Initialization aInitialization) {
    switch (aInitialization) {
      case Initialization::Storage:
        return NS_LITERAL_CSTRING("Storage");
      case Initialization::TemporaryStorage:
        return NS_LITERAL_CSTRING("TemporaryStorage");
      case Initialization::DefaultRepository:
        return NS_LITERAL_CSTRING("DefaultRepository");
      case Initialization::TemporaryRepository:
        return NS_LITERAL_CSTRING("TemporaryRepository");
      case Initialization::UpgradeStorageFrom0_0To1_0:
        return NS_LITERAL_CSTRING("UpgradeStorageFrom0_0To1_0");
      case Initialization::UpgradeStorageFrom1_0To2_0:
        return NS_LITERAL_CSTRING("UpgradeStorageFrom1_0To2_0");
      case Initialization::UpgradeStorageFrom2_0To2_1:
        return NS_LITERAL_CSTRING("UpgradeStorageFrom2_0To2_1");
      case Initialization::UpgradeStorageFrom2_1To2_2:
        return NS_LITERAL_CSTRING("UpgradeStorageFrom2_1To2_2");
      case Initialization::UpgradeStorageFrom2_2To2_3:
        return NS_LITERAL_CSTRING("UpgradeStorageFrom2_2To2_3");

      default:
        MOZ_CRASH("Bad initialization value!");
    }
  }

  bool InitializationAttempted(const Initialization aInitialization) const {
    return static_cast<bool>(mInitializationAttempts & aInitialization);
  }

  void ReportFirstInitializationAttempt(const Initialization aInitialization,
                                        const bool aSuccess) {
    MOZ_ASSERT(!InitializationAttempted(aInitialization));

    mInitializationAttempts |= aInitialization;
    Telemetry::Accumulate(Telemetry::QM_FIRST_INITIALIZATION_ATTEMPT,
                          GetInitializationString(aInitialization),
                          static_cast<uint32_t>(aSuccess));
  }
};

}  // namespace quota
}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_quota_InitializationTypes_h
