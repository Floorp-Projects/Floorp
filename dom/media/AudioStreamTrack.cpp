/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioStreamTrack.h"

#include "MediaStreamGraph.h"
#include "nsContentUtils.h"

namespace mozilla {
namespace dom {

void AudioStreamTrack::AddAudioOutput(void* aKey) {
  if (Ended()) {
    return;
  }
  mStream->AddAudioOutput(aKey);
}

void AudioStreamTrack::RemoveAudioOutput(void* aKey) {
  if (Ended()) {
    return;
  }
  mStream->RemoveAudioOutput(aKey);
}

void AudioStreamTrack::SetAudioOutputVolume(void* aKey, float aVolume) {
  if (Ended()) {
    return;
  }
  mStream->SetAudioOutputVolume(aKey, aVolume);
}

void AudioStreamTrack::GetLabel(nsAString& aLabel, CallerType aCallerType) {
  if (nsContentUtils::ResistFingerprinting(aCallerType)) {
    aLabel.AssignLiteral("Internal Microphone");
    return;
  }
  MediaStreamTrack::GetLabel(aLabel, aCallerType);
}

already_AddRefed<MediaStreamTrack> AudioStreamTrack::CloneInternal() {
  return do_AddRef(new AudioStreamTrack(mWindow, mInputStream, mTrackID,
                                        mSource, ReadyState(), mConstraints));
}

}  // namespace dom
}  // namespace mozilla
