/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioStreamTrack.h"

#include "MediaTrackGraph.h"
#include "nsContentUtils.h"

namespace mozilla::dom {

RefPtr<GenericPromise> AudioStreamTrack::AddAudioOutput(
    void* aKey, AudioDeviceInfo* aSink) {
  if (Ended()) {
    return GenericPromise::CreateAndResolve(true, __func__);
  }

  mTrack->AddAudioOutput(aKey, aSink);
  return mTrack->Graph()->NotifyWhenDeviceStarted(aSink);
}

void AudioStreamTrack::RemoveAudioOutput(void* aKey) {
  if (Ended()) {
    return;
  }

  mTrack->RemoveAudioOutput(aKey);
}

void AudioStreamTrack::SetAudioOutputVolume(void* aKey, float aVolume) {
  if (Ended()) {
    return;
  }

  mTrack->SetAudioOutputVolume(aKey, aVolume);
}

void AudioStreamTrack::GetLabel(nsAString& aLabel, CallerType aCallerType) {
  nsIGlobalObject* global =
      GetParentObject() ? GetParentObject()->AsGlobal() : nullptr;
  if (nsContentUtils::ShouldResistFingerprinting(aCallerType, global,
                                                 RFPTarget::StreamTrackLabel)) {
    aLabel.AssignLiteral("Internal Microphone");
    return;
  }
  MediaStreamTrack::GetLabel(aLabel, aCallerType);
}

already_AddRefed<MediaStreamTrack> AudioStreamTrack::CloneInternal() {
  return do_AddRef(new AudioStreamTrack(mWindow, mInputTrack, mSource,
                                        ReadyState(), Muted(), mConstraints));
}

}  // namespace mozilla::dom
