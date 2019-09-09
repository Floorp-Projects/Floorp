/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "WorkletImpl.h"

#include "Worklet.h"
#include "WorkletThread.h"

#include "mozilla/BasePrincipal.h"
#include "mozilla/NullPrincipal.h"
#include "mozilla/dom/RegisterWorkletBindings.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/WorkletBinding.h"

namespace mozilla {

// ---------------------------------------------------------------------------
// WorkletLoadInfo

WorkletLoadInfo::WorkletLoadInfo(nsPIDOMWindowInner* aWindow)
    : mInnerWindowID(aWindow->WindowID()) {
  MOZ_ASSERT(NS_IsMainThread());
  nsPIDOMWindowOuter* outerWindow = aWindow->GetOuterWindow();
  if (outerWindow) {
    mOuterWindowID = outerWindow->WindowID();
  } else {
    mOuterWindowID = 0;
  }
}

// ---------------------------------------------------------------------------
// WorkletImpl

WorkletImpl::WorkletImpl(nsPIDOMWindowInner* aWindow, nsIPrincipal* aPrincipal)
    : mOriginAttributes(BasePrincipal::Cast(aPrincipal)->OriginAttributesRef()),
      mPrincipal(NullPrincipal::CreateWithInheritedAttributes(aPrincipal)),
      mWorkletLoadInfo(aWindow),
      mTerminated(false) {}

WorkletImpl::~WorkletImpl() {
  MOZ_ASSERT(!mGlobalScope);
  MOZ_ASSERT(!mPrincipal || NS_IsMainThread());
}

JSObject* WorkletImpl::WrapWorklet(JSContext* aCx, dom::Worklet* aWorklet,
                                   JS::Handle<JSObject*> aGivenProto) {
  MOZ_ASSERT(NS_IsMainThread());
  return dom::Worklet_Binding::Wrap(aCx, aWorklet, aGivenProto);
}

dom::WorkletGlobalScope* WorkletImpl::GetGlobalScope() {
  dom::WorkletThread::AssertIsOnWorkletThread();

  if (mGlobalScope) {
    return mGlobalScope;
  }

  dom::AutoJSAPI jsapi;
  jsapi.Init();
  JSContext* cx = jsapi.cx();

  mGlobalScope = ConstructGlobalScope();

  JS::Rooted<JSObject*> global(cx);
  NS_ENSURE_TRUE(mGlobalScope->WrapGlobalObject(cx, &global), nullptr);

  JSAutoRealm ar(cx, global);

  // Init Web IDL bindings
  if (!dom::RegisterWorkletBindings(cx, global)) {
    return nullptr;
  }

  JS_FireOnNewGlobalObject(cx, global);

  return mGlobalScope;
}

void WorkletImpl::NotifyWorkletFinished() {
  MOZ_ASSERT(NS_IsMainThread());

  if (mTerminated) {
    return;
  }

  // Release global scope on its thread.
  SendControlMessage(NS_NewRunnableFunction(
      "WorkletImpl::NotifyWorkletFinished",
      [self = RefPtr<WorkletImpl>(this)]() { self->mGlobalScope = nullptr; }));

  mTerminated = true;
  if (mWorkletThread) {
    mWorkletThread->Terminate();
    mWorkletThread = nullptr;
  }
  mPrincipal = nullptr;
}

nsresult WorkletImpl::SendControlMessage(
    already_AddRefed<nsIRunnable> aRunnable) {
  MOZ_ASSERT(NS_IsMainThread());

  // TODO: bug 1492011 re ConsoleWorkletRunnable.
  if (mTerminated) {
    return NS_ERROR_ILLEGAL_DURING_SHUTDOWN;
  }

  if (!mWorkletThread) {
    // Thread creation. FIXME: this will change.
    mWorkletThread = dom::WorkletThread::Create(this);
    if (!mWorkletThread) {
      return NS_ERROR_UNEXPECTED;
    }
  }

  return mWorkletThread->DispatchRunnable(std::move(aRunnable));
}

}  // namespace mozilla
