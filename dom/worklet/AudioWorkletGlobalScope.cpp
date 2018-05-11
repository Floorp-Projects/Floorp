/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioWorkletGlobalScope.h"
#include "WorkletPrincipal.h"
#include "mozilla/dom/AudioWorkletGlobalScopeBinding.h"
#include "mozilla/dom/FunctionBinding.h"

namespace mozilla {
namespace dom {

AudioWorkletGlobalScope::AudioWorkletGlobalScope()
  : mCurrentFrame(0)
  , mCurrentTime(0)
  , mSampleRate(0.0)
{}

bool
AudioWorkletGlobalScope::WrapGlobalObject(JSContext* aCx,
                                          JS::MutableHandle<JSObject*> aReflector)
{
  JS::CompartmentOptions options;
  return AudioWorkletGlobalScopeBinding::Wrap(aCx, this, this,
                                              options,
                                              WorkletPrincipal::GetWorkletPrincipal(),
                                              true, aReflector);
}

void
AudioWorkletGlobalScope::RegisterProcessor(const nsAString& aType,
                                           VoidFunction& aProcessorCtor)
{
  // Nothing to do here.
}

uint64_t AudioWorkletGlobalScope::CurrentFrame() const
{
  return mCurrentFrame;
}

double AudioWorkletGlobalScope::CurrentTime() const
{
  return mCurrentTime;
}

float AudioWorkletGlobalScope::SampleRate() const
{
  return mSampleRate;
}

} // dom namespace
} // mozilla namespace
