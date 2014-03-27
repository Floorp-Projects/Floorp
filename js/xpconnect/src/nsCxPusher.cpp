/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCxPusher.h"

#include "nsIScriptContext.h"
#include "mozilla/dom/EventTarget.h"
#include "nsDOMJSUtils.h"
#include "xpcprivate.h"
#include "WorkerPrivate.h"

using mozilla::dom::EventTarget;
using mozilla::DebugOnly;

bool
nsCxPusher::Push(EventTarget *aCurrentTarget)
{
  MOZ_ASSERT(mPusher.empty());
  NS_ENSURE_TRUE(aCurrentTarget, false);
  nsresult rv;
  nsIScriptContext* scx =
    aCurrentTarget->GetContextForEventHandlers(&rv);
#ifdef DEBUG_smaug
  NS_ENSURE_SUCCESS(rv, false);
#else
  if(NS_FAILED(rv)) {
    return false;
  }
#endif

  if (!scx) {
    // The target may have a special JS context for event handlers.
    JSContext* cx = aCurrentTarget->GetJSContextForEventHandlers();
    if (cx) {
      mPusher.construct(cx);
    }

    // Nothing to do here, I guess.  Have to return true so that event firing
    // will still work correctly even if there is no associated JSContext
    return true;
  }

  mPusher.construct(scx->GetNativeContext());
  return true;
}

bool
nsCxPusher::RePush(EventTarget *aCurrentTarget)
{
  if (mPusher.empty()) {
    return Push(aCurrentTarget);
  }

  if (aCurrentTarget) {
    nsresult rv;
    nsIScriptContext* scx =
      aCurrentTarget->GetContextForEventHandlers(&rv);
    if (NS_FAILED(rv)) {
      mPusher.destroy();
      return false;
    }

    // If we have the same script context and native context is still
    // alive, no need to Pop/Push.
    if (scx && scx == mPusher.ref().GetScriptContext() &&
        scx->GetNativeContext()) {
      return true;
    }
  }

  mPusher.destroy();
  return Push(aCurrentTarget);
}

void
nsCxPusher::Push(JSContext *cx)
{
  mPusher.construct(cx);
}

void
nsCxPusher::PushNull()
{
  // Note: The Maybe<> template magic seems to need the static_cast below to
  // work right on some older compilers.
  mPusher.construct(static_cast<JSContext*>(nullptr), /* aAllowNull = */ true);
}

void
nsCxPusher::Pop()
{
  if (!mPusher.empty())
    mPusher.destroy();
}

namespace mozilla {

AutoCxPusher::AutoCxPusher(JSContext* cx, bool allowNull)
{
  MOZ_ASSERT_IF(!allowNull, cx);

  // Hold a strong ref to the nsIScriptContext, if any. This ensures that we
  // only destroy the mContext of an nsJSContext when it is not on the cx stack
  // (and therefore not in use). See nsJSContext::DestroyJSContext().
  if (cx)
    mScx = GetScriptContextFromJSContext(cx);

  XPCJSContextStack *stack = XPCJSRuntime::Get()->GetJSContextStack();
  if (!stack->Push(cx)) {
    MOZ_CRASH();
  }
  mStackDepthAfterPush = stack->Count();

#ifdef DEBUG
  mPushedContext = cx;
  mCompartmentDepthOnEntry = cx ? js::GetEnterCompartmentDepth(cx) : 0;
#endif

  // Enter a request and a compartment for the duration that the cx is on the
  // stack if non-null.
  if (cx) {
    mAutoRequest.construct(cx);

    // DOM JSContexts don't store their default compartment object on the cx.
    JSObject *compartmentObject = mScx ? mScx->GetWindowProxy()
                                       : js::DefaultObjectForContextOrNull(cx);
    if (compartmentObject)
      mAutoCompartment.construct(cx, compartmentObject);
  }
}

AutoCxPusher::~AutoCxPusher()
{
  // GC when we pop a script entry point. This is a useful heuristic that helps
  // us out on certain (flawed) benchmarks like sunspider, because it lets us
  // avoid GCing during the timing loop.
  //
  // NB: We need to take care to only do this if we're in a compartment,
  // otherwise JS_MaybeGC will segfault.
  if (mScx && !mAutoCompartment.empty())
    JS_MaybeGC(nsXPConnect::XPConnect()->GetCurrentJSContext());

  // Leave the compartment and request before popping.
  mAutoCompartment.destroyIfConstructed();
  mAutoRequest.destroyIfConstructed();

  // When we push a context, we may save the frame chain and pretend like we
  // haven't entered any compartment. This gets restored on Pop(), but we can
  // run into trouble if a Push/Pop are interleaved with a
  // JSAutoEnterCompartment. Make sure the compartment depth right before we
  // pop is the same as it was right after we pushed.
  MOZ_ASSERT_IF(mPushedContext, mCompartmentDepthOnEntry ==
                                js::GetEnterCompartmentDepth(mPushedContext));
  DebugOnly<JSContext*> stackTop;
  MOZ_ASSERT(mPushedContext == nsXPConnect::XPConnect()->GetCurrentJSContext());
  XPCJSRuntime::Get()->GetJSContextStack()->Pop();
  mScx = nullptr;
}

bool
AutoCxPusher::IsStackTop()
{
  uint32_t currentDepth = XPCJSRuntime::Get()->GetJSContextStack()->Count();
  MOZ_ASSERT(currentDepth >= mStackDepthAfterPush);
  return currentDepth == mStackDepthAfterPush;
}

AutoJSContext::AutoJSContext(MOZ_GUARD_OBJECT_NOTIFIER_ONLY_PARAM_IN_IMPL)
  : mCx(nullptr)
{
  Init(false MOZ_GUARD_OBJECT_NOTIFIER_PARAM_TO_PARENT);
}

AutoJSContext::AutoJSContext(bool aSafe MOZ_GUARD_OBJECT_NOTIFIER_PARAM_IN_IMPL)
  : mCx(nullptr)
{
  Init(aSafe MOZ_GUARD_OBJECT_NOTIFIER_PARAM_TO_PARENT);
}

void
AutoJSContext::Init(bool aSafe MOZ_GUARD_OBJECT_NOTIFIER_PARAM_IN_IMPL)
{
  JS::AutoAssertNoGC nogc;
  MOZ_ASSERT(!mCx, "mCx should not be initialized!");

  MOZ_GUARD_OBJECT_NOTIFIER_INIT;

  nsXPConnect *xpc = nsXPConnect::XPConnect();
  if (!aSafe) {
    mCx = xpc->GetCurrentJSContext();
  }

  if (!mCx) {
    mCx = xpc->GetSafeJSContext();
    mPusher.construct(mCx);
  }
}

AutoJSContext::operator JSContext*() const
{
  return mCx;
}

ThreadsafeAutoJSContext::ThreadsafeAutoJSContext(MOZ_GUARD_OBJECT_NOTIFIER_ONLY_PARAM_IN_IMPL)
{
  MOZ_GUARD_OBJECT_NOTIFIER_INIT;

  if (NS_IsMainThread()) {
    mCx = nullptr;
    mAutoJSContext.construct();
  } else {
    mCx = mozilla::dom::workers::GetCurrentThreadJSContext();
    mRequest.construct(mCx);
  }
}

ThreadsafeAutoJSContext::operator JSContext*() const
{
  if (mCx) {
    return mCx;
  } else {
    return mAutoJSContext.ref();
  }
}

AutoSafeJSContext::AutoSafeJSContext(MOZ_GUARD_OBJECT_NOTIFIER_ONLY_PARAM_IN_IMPL)
  : AutoJSContext(true MOZ_GUARD_OBJECT_NOTIFIER_PARAM_TO_PARENT)
  , mAc(mCx, XPCJSRuntime::Get()->GetJSContextStack()->GetSafeJSContextGlobal())
{
}

ThreadsafeAutoSafeJSContext::ThreadsafeAutoSafeJSContext(MOZ_GUARD_OBJECT_NOTIFIER_ONLY_PARAM_IN_IMPL)
{
  MOZ_GUARD_OBJECT_NOTIFIER_INIT;

  if (NS_IsMainThread()) {
    mCx = nullptr;
    mAutoSafeJSContext.construct();
  } else {
    mCx = mozilla::dom::workers::GetCurrentThreadJSContext();
    mRequest.construct(mCx);
  }
}

ThreadsafeAutoSafeJSContext::operator JSContext*() const
{
  if (mCx) {
    return mCx;
  } else {
    return mAutoSafeJSContext.ref();
  }
}

AutoPushJSContext::AutoPushJSContext(JSContext *aCx) : mCx(aCx)
{
  if (mCx && mCx != nsXPConnect::XPConnect()->GetCurrentJSContext()) {
    mPusher.construct(mCx);
  }
}

} // namespace mozilla
