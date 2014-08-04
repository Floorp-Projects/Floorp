/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GMPTimerParent_h_
#define GMPTimerParent_h_

#include "mozilla/gmp/PGMPTimerParent.h"
#include "nsITimer.h"
#include "nsCOMPtr.h"
#include "nsClassHashtable.h"
#include "nsHashKeys.h"
#include "nsAutoPtr.h"
#include "mozilla/Monitor.h"
#include "nsIThread.h"

namespace mozilla {
namespace gmp {

class GMPTimerParent : public PGMPTimerParent {
public:
  NS_INLINE_DECL_REFCOUNTING(GMPTimerParent)
  GMPTimerParent(nsIThread* aGMPThread);

protected:
  virtual bool RecvSetTimer(const uint32_t& aTimerId,
                            const uint32_t& aTimeoutMs) MOZ_OVERRIDE;
  virtual void ActorDestroy(ActorDestroyReason aWhy) MOZ_OVERRIDE;

private:
  ~GMPTimerParent() {}

  static void GMPTimerExpired(nsITimer *aTimer, void *aClosure);

  struct Context {
    Context() {
      MOZ_COUNT_CTOR(Context);
    }
    ~Context() {
      MOZ_COUNT_DTOR(Context);
    }
    nsCOMPtr<nsITimer> mTimer;
    nsRefPtr<GMPTimerParent> mParent; // Note: live timers keep the GMPTimerParent alive.
    uint32_t mId;
  };

  static PLDHashOperator
  CancelTimers(nsPtrHashKey<Context>* aContext, void* aClosure);

  void TimerExpired(Context* aContext);

  nsTHashtable<nsPtrHashKey<Context>> mTimers;

  nsCOMPtr<nsIThread> mGMPThread;
};

} // namespace gmp
} // namespace mozilla

#endif // GMPTimerParent_h_
