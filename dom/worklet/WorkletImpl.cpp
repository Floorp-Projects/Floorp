/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "WorkletImpl.h"

#include "AudioWorkletGlobalScope.h"
#include "PaintWorkletGlobalScope.h"
#include "Worklet.h"
#include "WorkletThread.h"

#include "mozilla/BasePrincipal.h"
#include "mozilla/dom/AudioWorkletBinding.h"
#include "mozilla/dom/DOMPrefs.h"
#include "mozilla/dom/RegisterWorkletBindings.h"
#include "mozilla/dom/WorkletBinding.h"

namespace mozilla {

// ---------------------------------------------------------------------------
// WorkletLoadInfo

WorkletLoadInfo::WorkletLoadInfo(nsPIDOMWindowInner* aWindow, nsIPrincipal* aPrincipal)
  : mInnerWindowID(aWindow->WindowID())
  , mDumpEnabled(dom::DOMPrefs::DumpEnabled())
  , mOriginAttributes(BasePrincipal::Cast(aPrincipal)->OriginAttributesRef())
  , mPrincipal(aPrincipal)
{
  MOZ_ASSERT(NS_IsMainThread());
  nsPIDOMWindowOuter* outerWindow = aWindow->GetOuterWindow();
  if (outerWindow) {
    mOuterWindowID = outerWindow->WindowID();
  } else {
    mOuterWindowID = 0;
  }
}

WorkletLoadInfo::~WorkletLoadInfo()
{
  MOZ_ASSERT(!mPrincipal || NS_IsMainThread());
}

// ---------------------------------------------------------------------------
// WorkletImpl

/* static */ already_AddRefed<dom::Worklet>
WorkletImpl::CreateWorklet(nsPIDOMWindowInner* aWindow,
                           nsIPrincipal* aPrincipal,
                           WorkletType aWorkletType)
{
  MOZ_ASSERT(NS_IsMainThread());

  RefPtr<WorkletImpl> impl = new WorkletImpl(aWindow, aPrincipal, aWorkletType);
  return MakeAndAddRef<dom::Worklet>(aWindow, std::move(impl));
}

WorkletImpl::WorkletImpl(nsPIDOMWindowInner* aWindow,
                         nsIPrincipal* aPrincipal,
                         WorkletType aWorkletType)
  : mWorkletLoadInfo(aWindow, aPrincipal)
  , mWorkletType(aWorkletType)
{
}

WorkletImpl::~WorkletImpl() = default;

JSObject*
WorkletImpl::WrapWorklet(JSContext* aCx, dom::Worklet* aWorklet,
                         JS::Handle<JSObject*> aGivenProto)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (mWorkletType == eAudioWorklet) {
    return dom::AudioWorklet_Binding::Wrap(aCx, aWorklet, aGivenProto);
  } else {
    return dom::Worklet_Binding::Wrap(aCx, aWorklet, aGivenProto);
  }
}

already_AddRefed<dom::WorkletGlobalScope>
WorkletImpl::CreateGlobalScope(JSContext* aCx)
{
  dom::WorkletThread::AssertIsOnWorkletThread();

  RefPtr<dom::WorkletGlobalScope> scope;

  switch (mWorkletType) {
    case eAudioWorklet:
      scope = new dom::AudioWorkletGlobalScope(this);
      break;
    case ePaintWorklet:
      scope = new dom::PaintWorkletGlobalScope(this);
      break;
  }

  JS::Rooted<JSObject*> global(aCx);
  NS_ENSURE_TRUE(scope->WrapGlobalObject(aCx, &global), nullptr);

  JSAutoRealm ar(aCx, global);

  // Init Web IDL bindings
  if (!dom::RegisterWorkletBindings(aCx, global)) {
    return nullptr;
  }

  JS_FireOnNewGlobalObject(aCx, global);

  return scope.forget();
}

dom::WorkletThread*
WorkletImpl::GetOrCreateThread()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!mWorkletThread) {
    // Thread creation. FIXME: this will change.
    mWorkletThread = dom::WorkletThread::Create(mWorkletLoadInfo);
  }

  return mWorkletThread;
}

void
WorkletImpl::TerminateThread()
{
  MOZ_ASSERT(NS_IsMainThread());
  if (!mWorkletThread) {
    return;
  }

  mWorkletThread->Terminate();
  mWorkletThread = nullptr;
  mWorkletLoadInfo.mPrincipal = nullptr;
}

nsresult
WorkletImpl::DispatchRunnable(already_AddRefed<nsIRunnable> aRunnable)
{
  // TODO: bug 1492011 re ConsoleWorkletRunnable.
  MOZ_ASSERT(mWorkletThread);
  return mWorkletThread->DispatchRunnable(std::move(aRunnable));
}

} // namespace mozilla
