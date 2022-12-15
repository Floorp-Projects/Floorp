/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "AudioWorkletProcessor.h"

#include "mozilla/dom/AudioWorkletNodeBinding.h"
#include "mozilla/dom/AudioWorkletProcessorBinding.h"
#include "mozilla/dom/AudioWorkletGlobalScope.h"
#include "mozilla/dom/MessagePort.h"
#include "mozilla/dom/WorkletGlobalScope.h"
#include "nsIGlobalObject.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(AudioWorkletProcessor, mParent, mPort)

AudioWorkletProcessor::AudioWorkletProcessor(nsIGlobalObject* aParent,
                                             MessagePort* aPort)
    : mParent(aParent), mPort(aPort) {}

AudioWorkletProcessor::~AudioWorkletProcessor() = default;

/* static */
already_AddRefed<AudioWorkletProcessor> AudioWorkletProcessor::Constructor(
    const GlobalObject& aGlobal, ErrorResult& aRv) {
  nsCOMPtr<WorkletGlobalScope> global =
      do_QueryInterface(aGlobal.GetAsSupports());
  MOZ_ASSERT(global);
  RefPtr<MessagePort> port = static_cast<AudioWorkletGlobalScope*>(global.get())
                                 ->TakePortForProcessorCtor();
  if (!port) {
    aRv.ThrowTypeError<MSG_ILLEGAL_CONSTRUCTOR>();
    return nullptr;
  }
  RefPtr<AudioWorkletProcessor> audioWorkletProcessor =
      new AudioWorkletProcessor(global, port);

  return audioWorkletProcessor.forget();
}

JSObject* AudioWorkletProcessor::WrapObject(JSContext* aCx,
                                            JS::Handle<JSObject*> aGivenProto) {
  return AudioWorkletProcessor_Binding::Wrap(aCx, this, aGivenProto);
}

}  // namespace mozilla::dom
