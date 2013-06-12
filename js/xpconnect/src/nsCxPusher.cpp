/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCxPusher.h"

#include "nsIScriptContext.h"
#include "mozilla/dom/EventTarget.h"
#include "nsJSUtils.h"
#include "nsDOMJSUtils.h"
#include "mozilla/Util.h"
#include "xpcprivate.h"

using mozilla::dom::EventTarget;
using mozilla::DebugOnly;

NS_EXPORT
nsCxPusher::nsCxPusher()
    : mScriptIsRunning(false),
      mPushedSomething(false)
{
}

NS_EXPORT
nsCxPusher::~nsCxPusher()
{
  Pop();
}

bool
nsCxPusher::Push(EventTarget *aCurrentTarget)
{
  if (mPushedSomething) {
    NS_ERROR("Whaaa! No double pushing with nsCxPusher::Push()!");

    return false;
  }

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
      DoPush(cx);
    }

    // Nothing to do here, I guess.  Have to return true so that event firing
    // will still work correctly even if there is no associated JSContext
    return true;
  }

  JSContext* cx = scx ? scx->GetNativeContext() : nullptr;

  // If there's no native context in the script context it must be
  // in the process or being torn down. We don't want to notify the
  // script context about scripts having been evaluated in such a
  // case, calling with a null cx is fine in that case.
  Push(cx);
  return true;
}

bool
nsCxPusher::RePush(EventTarget *aCurrentTarget)
{
  if (!mPushedSomething) {
    return Push(aCurrentTarget);
  }

  if (aCurrentTarget) {
    nsresult rv;
    nsIScriptContext* scx =
      aCurrentTarget->GetContextForEventHandlers(&rv);
    if (NS_FAILED(rv)) {
      Pop();
      return false;
    }

    // If we have the same script context and native context is still
    // alive, no need to Pop/Push.
    if (scx && scx == mScx &&
        scx->GetNativeContext()) {
      return true;
    }
  }

  Pop();
  return Push(aCurrentTarget);
}

NS_EXPORT_(void)
nsCxPusher::Push(JSContext *cx)
{
  MOZ_ASSERT(!mPushedSomething, "No double pushing with nsCxPusher::Push()!");
  MOZ_ASSERT(cx);

  // Hold a strong ref to the nsIScriptContext, just in case
  // XXXbz do we really need to?  If we don't get one of these in Pop(), is
  // that really a problem?  Or do we need to do this to effectively root |cx|?
  mScx = GetScriptContextFromJSContext(cx);

  DoPush(cx);
}

void
nsCxPusher::DoPush(JSContext* cx)
{
  // NB: The GetDynamicScriptContext is historical and might not be sane.
  if (cx && nsJSUtils::GetDynamicScriptContext(cx) &&
      xpc::IsJSContextOnStack(cx))
  {
    // If the context is on the stack, that means that a script
    // is running at the moment in the context.
    mScriptIsRunning = true;
  }

  if (!xpc::PushJSContext(cx)) {
    MOZ_CRASH();
  }

  // Enter a request for the duration that the cx is on the stack if non-null.
  // NB: We call UnmarkGrayContext so that this can obsolete the need for the
  // old XPCAutoRequest as well.
  if (cx) {
    mAutoRequest.construct(cx);
    xpc_UnmarkGrayContext(cx);
  }

  mPushedSomething = true;
#ifdef DEBUG
  mPushedContext = cx;
  if (cx)
    mCompartmentDepthOnEntry = js::GetEnterCompartmentDepth(cx);
#endif
}

void
nsCxPusher::PushNull()
{
  DoPush(nullptr);
}

NS_EXPORT_(void)
nsCxPusher::Pop()
{
  if (!mPushedSomething) {
    mScx = nullptr;
    mPushedSomething = false;

    NS_ASSERTION(!mScriptIsRunning, "Huh, this can't be happening, "
                 "mScriptIsRunning can't be set here!");

    return;
  }

  // Leave the request before popping.
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
  xpc::PopJSContext();

  if (!mScriptIsRunning && mScx) {
    // No JS is running in the context, but executing the event handler might have
    // caused some JS to run. Tell the script context that it's done.

    mScx->ScriptEvaluated(true);
  }

  mScx = nullptr;
  mScriptIsRunning = false;
  mPushedSomething = false;
}

namespace mozilla {

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
  MOZ_ASSERT(!mCx, "mCx should not be initialized!");

  MOZ_GUARD_OBJECT_NOTIFIER_INIT;

  nsXPConnect *xpc = nsXPConnect::XPConnect();
  if (!aSafe) {
    mCx = xpc->GetCurrentJSContext();
  }

  if (!mCx) {
    mCx = xpc->GetSafeJSContext();
    mPusher.Push(mCx);
  }
}

AutoJSContext::operator JSContext*() const
{
  return mCx;
}

AutoSafeJSContext::AutoSafeJSContext(MOZ_GUARD_OBJECT_NOTIFIER_ONLY_PARAM_IN_IMPL)
  : AutoJSContext(true MOZ_GUARD_OBJECT_NOTIFIER_PARAM_TO_PARENT)
{
}

AutoPushJSContext::AutoPushJSContext(JSContext *aCx) : mCx(aCx)
{
  if (mCx && mCx != nsXPConnect::XPConnect()->GetCurrentJSContext()) {
    mPusher.Push(mCx);
  }
}

} // namespace mozilla
