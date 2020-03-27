/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "PaintWorkletImpl.h"

#include "PaintWorkletGlobalScope.h"
#include "mozilla/dom/Worklet.h"
#include "mozilla/dom/WorkletThread.h"

namespace mozilla {

/* static */ already_AddRefed<dom::Worklet> PaintWorkletImpl::CreateWorklet(
    nsPIDOMWindowInner* aWindow, nsIPrincipal* aPrincipal) {
  MOZ_ASSERT(NS_IsMainThread());

  RefPtr<PaintWorkletImpl> impl = new PaintWorkletImpl(aWindow, aPrincipal);
  return MakeAndAddRef<dom::Worklet>(aWindow, std::move(impl));
}

PaintWorkletImpl::PaintWorkletImpl(nsPIDOMWindowInner* aWindow,
                                   nsIPrincipal* aPrincipal)
    : WorkletImpl(aWindow, aPrincipal) {
#ifdef RELEASE_OR_BETA
  MOZ_CRASH("This code should not go to release/beta yet!");
#endif
}

PaintWorkletImpl::~PaintWorkletImpl() = default;

already_AddRefed<dom::WorkletGlobalScope>
PaintWorkletImpl::ConstructGlobalScope() {
  dom::WorkletThread::AssertIsOnWorkletThread();

  return MakeAndAddRef<dom::PaintWorkletGlobalScope>(this);
}

}  // namespace mozilla
