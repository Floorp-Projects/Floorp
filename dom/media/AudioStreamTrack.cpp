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
  if (UniquePtr<CrossGraphPort>* cgm = mCrossGraphs.Get(aKey)) {
    (*cgm)->AddAudioOutput(aKey);
    return;
  }
  mTrack->AddAudioOutput(aKey);
}

void AudioStreamTrack::RemoveAudioOutput(void* aKey) {
  if (Ended()) {
    return;
  }
  if (UniquePtr<CrossGraphPort>* cgm = mCrossGraphs.Get(aKey)) {
    (*cgm)->RemoveAudioOutput(aKey);
    return;
  }
  mTrack->RemoveAudioOutput(aKey);
}

void AudioStreamTrack::SetAudioOutputVolume(void* aKey, float aVolume) {
  if (Ended()) {
    return;
  }
  if (UniquePtr<CrossGraphPort>* cgm = mCrossGraphs.Get(aKey)) {
    (*cgm)->SetAudioOutputVolume(aKey, aVolume);
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
    for (const auto& data : mCrossGraphs.Values()) {
      (*data)->Destroy();
      data->reset();
    }
    mCrossGraphs.Clear();
  }
  MediaStreamTrack::SetReadyState(aState);
}

RefPtr<GenericPromise> AudioStreamTrack::SetAudioOutputDevice(
    void* key, AudioDeviceInfo* aSink) {
  MOZ_ASSERT(aSink);

  if (Ended()) {
    return GenericPromise::CreateAndResolve(true, __func__);
  }

  UniquePtr<CrossGraphPort> manager =
      CrossGraphPort::Connect(this, aSink, mWindow);
  if (!manager) {
    // We are setting the default output device.
    auto entry = mCrossGraphs.Lookup(key);
    if (entry) {
      // There is an existing non-default output device for this track. Remove
      // it.
      (*entry.Data())->Destroy();
      entry.Remove();
    }
    return GenericPromise::CreateAndResolve(true, __func__);
  }

  // We are setting a non-default output device.
  UniquePtr<CrossGraphPort>& crossGraphPtr = *mCrossGraphs.GetOrInsertNew(key);
  if (crossGraphPtr) {
    // This key already has a non-default output device set. Destroy it.
    crossGraphPtr->Destroy();
  }

  crossGraphPtr = std::move(manager);
  return crossGraphPtr->EnsureConnected();
}

}  // namespace mozilla::dom
