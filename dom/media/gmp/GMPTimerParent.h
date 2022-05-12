/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GMPTimerParent_h_
#define GMPTimerParent_h_

#include "mozilla/gmp/PGMPTimerParent.h"
#include "nsITimer.h"
#include "nsCOMPtr.h"
#include "nsTHashSet.h"
#include "mozilla/Monitor.h"

namespace mozilla::gmp {

class GMPTimerParent : public PGMPTimerParent {
  friend class PGMPTimerParent;

 public:
  NS_INLINE_DECL_REFCOUNTING(GMPTimerParent)
  explicit GMPTimerParent(nsISerialEventTarget* aGMPEventTarget);

  void Shutdown();

 protected:
  mozilla::ipc::IPCResult RecvSetTimer(const uint32_t& aTimerId,
                                       const uint32_t& aTimeoutMs);
  void ActorDestroy(ActorDestroyReason aWhy) override;

 private:
  ~GMPTimerParent() = default;

  static void GMPTimerExpired(nsITimer* aTimer, void* aClosure);

  struct Context {
    Context() : mId(0) { MOZ_COUNT_CTOR(Context); }
    MOZ_COUNTED_DTOR(Context)
    nsCOMPtr<nsITimer> mTimer;
    RefPtr<GMPTimerParent>
        mParent;  // Note: live timers keep the GMPTimerParent alive.
    uint32_t mId;
  };

  void TimerExpired(Context* aContext);

  nsTHashSet<Context*> mTimers;

  nsCOMPtr<nsISerialEventTarget> mGMPEventTarget;

  bool mIsOpen;
};

}  // namespace mozilla::gmp

#endif  // GMPTimerParent_h_
