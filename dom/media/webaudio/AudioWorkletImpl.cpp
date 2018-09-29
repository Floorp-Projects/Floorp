/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "AudioWorkletImpl.h"

#include "AudioContext.h"
#include "mozilla/dom/AudioWorkletBinding.h"
#include "mozilla/dom/AudioWorkletGlobalScope.h"
#include "mozilla/dom/Worklet.h"
#include "mozilla/dom/WorkletThread.h"
#include "nsGlobalWindowInner.h"

namespace mozilla {

/* static */ already_AddRefed<dom::Worklet>
AudioWorkletImpl::CreateWorklet(dom::AudioContext* aContext, ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsPIDOMWindowInner> window = aContext->GetOwner();
  if (NS_WARN_IF(!window)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }
  nsCOMPtr<nsIPrincipal> principal =
    nsGlobalWindowInner::Cast(window)->GetPrincipal();
  if (NS_WARN_IF(!principal)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<AudioWorkletImpl> impl = new AudioWorkletImpl(window, principal);
  return MakeAndAddRef<dom::Worklet>(window, std::move(impl));
}

AudioWorkletImpl::AudioWorkletImpl(nsPIDOMWindowInner* aWindow,
                                   nsIPrincipal* aPrincipal)
  : WorkletImpl(aWindow, aPrincipal)
{
}

AudioWorkletImpl::~AudioWorkletImpl() = default;

JSObject*
AudioWorkletImpl::WrapWorklet(JSContext* aCx, dom::Worklet* aWorklet,
                              JS::Handle<JSObject*> aGivenProto)
{
  MOZ_ASSERT(NS_IsMainThread());
  return dom::AudioWorklet_Binding::Wrap(aCx, aWorklet, aGivenProto);
}

already_AddRefed<dom::WorkletGlobalScope>
AudioWorkletImpl::ConstructGlobalScope()
{
  dom::WorkletThread::AssertIsOnWorkletThread();

  return MakeAndAddRef<dom::AudioWorkletGlobalScope>(this);
}

} // namespace mozilla
