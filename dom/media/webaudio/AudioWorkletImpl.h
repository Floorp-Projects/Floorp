/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef AudioWorkletImpl_h
#define AudioWorkletImpl_h

#include "mozilla/dom/WorkletImpl.h"
#include "mozilla/dom/AudioWorkletGlobalScope.h"

namespace mozilla {

class AudioNodeTrack;

namespace dom {
class AudioContext;
}

class AudioWorkletImpl final : public WorkletImpl {
 public:
  // Methods for parent thread only:

  static already_AddRefed<dom::Worklet> CreateWorklet(
      dom::AudioContext* aContext, ErrorResult& aRv);

  JSObject* WrapWorklet(JSContext* aCx, dom::Worklet* aWorklet,
                        JS::Handle<JSObject*> aGivenProto) override;

  nsresult SendControlMessage(already_AddRefed<nsIRunnable> aRunnable) override;

  nsContentPolicyType ContentPolicyType() const override {
    return nsIContentPolicy::TYPE_INTERNAL_AUDIOWORKLET;
  }

  // Execution thread only.
  dom::AudioWorkletGlobalScope* GetGlobalScope() {
    return static_cast<dom::AudioWorkletGlobalScope*>(
        WorkletImpl::GetGlobalScope());
  }

  // Any thread:
  AudioNodeTrack* DestinationTrack() const { return mDestinationTrack; }

  void OnAddModuleStarted() const override;
  void OnAddModulePromiseSettled() const override;

 protected:
  // Execution thread only.
  already_AddRefed<dom::WorkletGlobalScope> ConstructGlobalScope() override;

 private:
  AudioWorkletImpl(nsPIDOMWindowInner* aWindow, nsIPrincipal* aPrincipal,
                   AudioNodeTrack* aDestinationTrack);
  ~AudioWorkletImpl();

  const RefPtr<AudioNodeTrack> mDestinationTrack;
};

}  // namespace mozilla

#endif  // AudioWorkletImpl_h
