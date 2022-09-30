/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioStreamTrack.h"

#include "MediaTrackGraph.h"
#include "nsContentUtils.h"

namespace mozilla::dom {

void AudioStreamTrack::AddAudioOutput(void* aKey) {
  if (Ended()) {
    return;
  }
  if (CrossGraphPort* cgm = mCrossGraphs.Get(aKey)) {
    cgm->AddAudioOutput(aKey);
    return;
  }
  mTrack->AddAudioOutput(aKey);
}

void AudioStreamTrack::RemoveAudioOutput(void* aKey) {
  if (Ended()) {
    return;
  }
  if (auto entry = mCrossGraphs.Lookup(aKey)) {
    // The audio output for this track is directed to a non-default device.
    // The CrossGraphPort for this output is no longer required so remove it.
    entry.Remove();
    return;
  }
  mTrack->RemoveAudioOutput(aKey);
}

void AudioStreamTrack::SetAudioOutputVolume(void* aKey, float aVolume) {
  if (Ended()) {
    return;
  }
  if (CrossGraphPort* cgm = mCrossGraphs.Get(aKey)) {
    cgm->SetAudioOutputVolume(aKey, aVolume);
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
                                        ReadyState(), Muted(), mConstraints));
}

void AudioStreamTrack::SetReadyState(MediaStreamTrackState aState) {
  if (!mCrossGraphs.IsEmpty() && !Ended() &&
      mReadyState == MediaStreamTrackState::Live &&
      aState == MediaStreamTrackState::Ended) {
    mCrossGraphs.Clear();
  }
  MediaStreamTrack::SetReadyState(aState);
}

RefPtr<GenericPromise> AudioStreamTrack::SetAudioOutputDevice(
    void* aKey, AudioDeviceInfo* aSink) {
  MOZ_ASSERT(aSink);
  MOZ_ASSERT(!mCrossGraphs.Get(aKey),
             "A previous audio output for this aKey should have been removed");

  if (Ended()) {
    return GenericPromise::CreateAndResolve(true, __func__);
  }

  UniquePtr<CrossGraphPort> manager =
      CrossGraphPort::Connect(this, aSink, mWindow);
  if (!manager) {
    // We are setting the default output device.
    return GenericPromise::CreateAndResolve(true, __func__);
  }

  // We are setting a non-default output device.
  const UniquePtr<CrossGraphPort>& crossGraph = mCrossGraphs.WithEntryHandle(
      aKey, [&manager](auto entry) -> UniquePtr<CrossGraphPort>& {
        return entry.Insert(std::move(manager));
      });
  return crossGraph->EnsureConnected();
}

}  // namespace mozilla::dom
