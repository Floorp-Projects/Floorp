/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GMPTimerParent.h"
#include "nsComponentManagerUtils.h"
#include "mozilla/Unused.h"
#include "nsAutoPtr.h"

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

GMPTimerParent::GMPTimerParent(nsISerialEventTarget* aGMPEventTarget)
  : mGMPEventTarget(aGMPEventTarget)
  , mIsOpen(true)
{
}

mozilla::ipc::IPCResult
GMPTimerParent::RecvSetTimer(const uint32_t& aTimerId,
                             const uint32_t& aTimeoutMs)
{
  LOGD(("%s::%s: %p mIsOpen=%d", __CLASS__, __FUNCTION__, this, mIsOpen));

  MOZ_ASSERT(mGMPEventTarget->IsOnCurrentThread());

  if (!mIsOpen) {
    return IPC_OK();
  }

  nsresult rv;
  nsAutoPtr<Context> ctx(new Context());
  ctx->mTimer = do_CreateInstance("@mozilla.org/timer;1", &rv);
  NS_ENSURE_SUCCESS(rv, IPC_OK());

  ctx->mId = aTimerId;
  rv = ctx->mTimer->SetTarget(mGMPEventTarget);
  NS_ENSURE_SUCCESS(rv, IPC_OK());
  ctx->mParent = this;

  rv =
    ctx->mTimer->InitWithNamedFuncCallback(&GMPTimerParent::GMPTimerExpired,
                                           ctx,
                                           aTimeoutMs,
                                           nsITimer::TYPE_ONE_SHOT,
                                           "gmp::GMPTimerParent::RecvSetTimer");
  NS_ENSURE_SUCCESS(rv, IPC_OK());

  mTimers.PutEntry(ctx.forget());

  return IPC_OK();
}

void
GMPTimerParent::Shutdown()
{
  LOGD(("%s::%s: %p mIsOpen=%d", __CLASS__, __FUNCTION__, this, mIsOpen));

  MOZ_ASSERT(mGMPEventTarget->IsOnCurrentThread());

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
  MOZ_ASSERT(mGMPEventTarget->IsOnCurrentThread());

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
