/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GMPTimerParent.h"
#include "nsComponentManagerUtils.h"
#include "mozilla/unused.h"

namespace mozilla {

#ifdef LOG
#undef LOG
#endif

extern LogModule* GetGMPLog();

#define LOGD(msg) MOZ_LOG(GetGMPLog(), mozilla::LogLevel::Debug, msg)
#define LOG(level, msg) MOZ_LOG(GetGMPLog(), (level), msg)

#ifdef __CLASS__
#undef __CLASS__
#endif
#define __CLASS__ "GMPParent"

namespace gmp {

GMPTimerParent::GMPTimerParent(nsIThread* aGMPThread)
  : mGMPThread(aGMPThread)
  , mIsOpen(true)
{
}

bool
GMPTimerParent::RecvSetTimer(const uint32_t& aTimerId,
                             const uint32_t& aTimeoutMs)
{
  LOGD(("%s::%s: %p mIsOpen=%d", __CLASS__, __FUNCTION__, this, mIsOpen));

  MOZ_ASSERT(mGMPThread == NS_GetCurrentThread());

  if (!mIsOpen) {
    return true;
  }

  nsresult rv;
  nsAutoPtr<Context> ctx(new Context());
  ctx->mTimer = do_CreateInstance("@mozilla.org/timer;1", &rv);
  NS_ENSURE_SUCCESS(rv, true);

  ctx->mId = aTimerId;
  rv = ctx->mTimer->SetTarget(mGMPThread);
  NS_ENSURE_SUCCESS(rv, true);
  ctx->mParent = this;

  rv = ctx->mTimer->InitWithFuncCallback(&GMPTimerParent::GMPTimerExpired,
                                          ctx,
                                          aTimeoutMs,
                                          nsITimer::TYPE_ONE_SHOT);
  NS_ENSURE_SUCCESS(rv, true);

  mTimers.PutEntry(ctx.forget());

  return true;
}

void
GMPTimerParent::Shutdown()
{
  LOGD(("%s::%s: %p mIsOpen=%d", __CLASS__, __FUNCTION__, this, mIsOpen));

  MOZ_ASSERT(mGMPThread == NS_GetCurrentThread());

  for (auto iter = mTimers.Iter(); !iter.Done(); iter.Next()) {
    Context* context = iter.Get()->GetKey();
    context->mTimer->Cancel();
    delete context;
  }

  mTimers.Clear();
  mIsOpen = false;
}

void
GMPTimerParent::ActorDestroy(ActorDestroyReason aWhy)
{
  LOGD(("%s::%s: %p mIsOpen=%d", __CLASS__, __FUNCTION__, this, mIsOpen));

  Shutdown();
}

/* static */
void
GMPTimerParent::GMPTimerExpired(nsITimer *aTimer, void *aClosure)
{
  MOZ_ASSERT(aClosure);
  nsAutoPtr<Context> ctx(static_cast<Context*>(aClosure));
  MOZ_ASSERT(ctx->mParent);
  if (ctx->mParent) {
    ctx->mParent->TimerExpired(ctx);
  }
}

void
GMPTimerParent::TimerExpired(Context* aContext)
{
  LOGD(("%s::%s: %p mIsOpen=%d", __CLASS__, __FUNCTION__, this, mIsOpen));
  MOZ_ASSERT(mGMPThread == NS_GetCurrentThread());

  if (!mIsOpen) {
    return;
  }

  uint32_t id = aContext->mId;
  mTimers.RemoveEntry(aContext);
  if (id) {
    Unused << SendTimerExpired(id);
  }
}

} // namespace gmp
} // namespace mozilla
