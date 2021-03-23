/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GMPTimerParent.h"

#include "GMPLog.h"
#include "mozilla/Unused.h"
#include "nsComponentManagerUtils.h"

namespace mozilla {

extern LogModule* GetGMPLog();

#ifdef __CLASS__
#  undef __CLASS__
#endif
#define __CLASS__ "GMPTimerParent"

namespace gmp {

GMPTimerParent::GMPTimerParent(nsISerialEventTarget* aGMPEventTarget)
    : mGMPEventTarget(aGMPEventTarget), mIsOpen(true) {}

mozilla::ipc::IPCResult GMPTimerParent::RecvSetTimer(
    const uint32_t& aTimerId, const uint32_t& aTimeoutMs) {
  GMP_LOG_DEBUG("%s::%s: %p mIsOpen=%d", __CLASS__, __FUNCTION__, this,
                mIsOpen);

  MOZ_ASSERT(mGMPEventTarget->IsOnCurrentThread());

  if (!mIsOpen) {
    return IPC_OK();
  }

  nsresult rv;
  UniquePtr<Context> ctx(new Context());

  rv = NS_NewTimerWithFuncCallback(
      getter_AddRefs(ctx->mTimer), &GMPTimerParent::GMPTimerExpired, ctx.get(),
      aTimeoutMs, nsITimer::TYPE_ONE_SHOT, "gmp::GMPTimerParent::RecvSetTimer",
      mGMPEventTarget);
  NS_ENSURE_SUCCESS(rv, IPC_OK());

  ctx->mId = aTimerId;
  ctx->mParent = this;

  mTimers.Insert(ctx.release());

  return IPC_OK();
}

void GMPTimerParent::Shutdown() {
  GMP_LOG_DEBUG("%s::%s: %p mIsOpen=%d", __CLASS__, __FUNCTION__, this,
                mIsOpen);

  MOZ_ASSERT(mGMPEventTarget->IsOnCurrentThread());

  for (Context* context : mTimers) {
    context->mTimer->Cancel();
    delete context;
  }

  mTimers.Clear();
  mIsOpen = false;
}

void GMPTimerParent::ActorDestroy(ActorDestroyReason aWhy) {
  GMP_LOG_DEBUG("%s::%s: %p mIsOpen=%d", __CLASS__, __FUNCTION__, this,
                mIsOpen);

  Shutdown();
}

/* static */
void GMPTimerParent::GMPTimerExpired(nsITimer* aTimer, void* aClosure) {
  MOZ_ASSERT(aClosure);
  UniquePtr<Context> ctx(static_cast<Context*>(aClosure));
  MOZ_ASSERT(ctx->mParent);
  if (ctx->mParent) {
    ctx->mParent->TimerExpired(ctx.get());
  }
}

void GMPTimerParent::TimerExpired(Context* aContext) {
  GMP_LOG_DEBUG("%s::%s: %p mIsOpen=%d", __CLASS__, __FUNCTION__, this,
                mIsOpen);
  MOZ_ASSERT(mGMPEventTarget->IsOnCurrentThread());

  if (!mIsOpen) {
    return;
  }

  uint32_t id = aContext->mId;
  mTimers.Remove(aContext);
  if (id) {
    Unused << SendTimerExpired(id);
  }
}

}  // namespace gmp
}  // namespace mozilla

#undef __CLASS__
