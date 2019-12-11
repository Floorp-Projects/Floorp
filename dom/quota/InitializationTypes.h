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
                              const SuccessFunction&& aSuccessFunction)
        : mOwner(aOwner),
          mInitialization(aInitialization),
          mSuccessFunction(aSuccessFunction) {}

    ~AutoInitializationAttempt() {
      if (!(mOwner.mInitializationAttempts & mInitialization)) {
        mOwner.mInitializationAttempts |= mInitialization;
        Telemetry::Accumulate(Telemetry::QM_FIRST_INITIALIZATION_ATTEMPT,
                              mOwner.GetInitializationString(mInitialization),
                              static_cast<uint32_t>(mSuccessFunction()));
      }
    }
  };

  template <typename SuccessFunction>
  AutoInitializationAttempt<SuccessFunction> RecordFirstInitializationAttempt(
      const Initialization aInitialization,
      SuccessFunction&& aSuccessFunction) {
    return AutoInitializationAttempt<SuccessFunction>(
        *this, aInitialization, std::move(aSuccessFunction));
  }

  void AssertInitializationAttempted(Initialization aInitialization) {
    MOZ_ASSERT(mInitializationAttempts & aInitialization);
  }

  void ResetInitializationAttempts() {
    mInitializationAttempts = Initialization::None;
  }

 private:
  // TODO: Use constexpr here once bug 1594094 is addressed.
  nsLiteralCString GetInitializationString(Initialization aInitialization) {
    switch (aInitialization) {
      case Initialization::Storage:
        return NS_LITERAL_CSTRING("Storage");
      case Initialization::TemporaryStorage:
        return NS_LITERAL_CSTRING("TemporaryStorage");

      default:
        MOZ_CRASH("Bad initialization value!");
    }
  }
};

}  // namespace quota
}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_quota_InitializationTypes_h
