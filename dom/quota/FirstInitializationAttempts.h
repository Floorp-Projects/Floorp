/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_QUOTA_FIRSTINITIALIZATIONATTEMPTS_H_
#define DOM_QUOTA_FIRSTINITIALIZATIONATTEMPTS_H_

#include <cstdint>
#include <utility>
#include "ErrorList.h"

namespace mozilla::dom::quota {

template <typename Initialization, typename StringGenerator>
class FirstInitializationAttempts {
  Initialization mFirstInitializationAttempts = Initialization::None;

 public:
  class FirstInitializationAttemptImpl {
    using FirstInitializationAttemptsType =
        FirstInitializationAttempts<Initialization, StringGenerator>;

    FirstInitializationAttemptsType& mOwner;
    const Initialization mInitialization;

   public:
    FirstInitializationAttemptImpl(FirstInitializationAttemptsType& aOwner,
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

}  // namespace mozilla::dom::quota

#endif  // DOM_QUOTA_FIRSTINITIALIZATIONATTEMPTS_H_
