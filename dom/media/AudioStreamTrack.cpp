/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioStreamTrack.h"

#include "MediaTrackGraph.h"
#include "nsContentUtils.h"

namespace mozilla {
namespace dom {

void AudioStreamTrack::AddAudioOutput(void* aKey) {
  if (Ended()) {
    return;
  }
  mTrack->AddAudioOutput(aKey);
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
  if (nsContentUtils::ResistFingerprinting(aCallerType)) {
    aLabel.AssignLiteral("Internal Microphone");
    return;
  }
  MediaStreamTrack::GetLabel(aLabel, aCallerType);
}

already_AddRefed<MediaStreamTrack> AudioStreamTrack::CloneInternal() {
  return do_AddRef(new AudioStreamTrack(mWindow, mInputTrack, mSource,
                                        ReadyState(), mConstraints));
}

}  // namespace dom
}  // namespace mozilla
