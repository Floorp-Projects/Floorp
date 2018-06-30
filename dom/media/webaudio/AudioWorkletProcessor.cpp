/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "AudioWorkletProcessor.h"

#include "mozilla/dom/AudioWorkletNodeBinding.h"
#include "mozilla/dom/AudioWorkletProcessorBinding.h"
#include "mozilla/dom/MessagePort.h"
#include "nsIGlobalObject.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(AudioWorkletProcessor, mParent)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(AudioWorkletProcessor, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(AudioWorkletProcessor, Release)

AudioWorkletProcessor::AudioWorkletProcessor(nsIGlobalObject* aParent)
  : mParent(aParent)
{
}

/* static */ already_AddRefed<AudioWorkletProcessor>
AudioWorkletProcessor::Constructor(const GlobalObject& aGlobal,
                                   const AudioWorkletNodeOptions& aOptions,
                                   ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> global =
      do_QueryInterface(aGlobal.GetAsSupports());
  MOZ_ASSERT(global);

  RefPtr<AudioWorkletProcessor> audioWorkletProcessor =
      new AudioWorkletProcessor(global);

  return audioWorkletProcessor.forget();
}

JSObject*
AudioWorkletProcessor::WrapObject(JSContext* aCx,
                                  JS::Handle<JSObject*> aGivenProto)
{
  return AudioWorkletProcessor_Binding::Wrap(aCx, this, aGivenProto);
}

MessagePort*
AudioWorkletProcessor::GetPort(ErrorResult& aRv) const
{
  aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
  return nullptr;
}

} // namespace dom
} // namespace mozilla
