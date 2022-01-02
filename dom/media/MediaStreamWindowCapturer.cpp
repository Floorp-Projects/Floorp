/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaStreamWindowCapturer.h"

#include "AudioStreamTrack.h"
#include "DOMMediaStream.h"
#include "MediaTrackGraph.h"

namespace mozilla {
using dom::AudioStreamTrack;
using dom::MediaStreamTrack;

MediaStreamWindowCapturer::CapturedTrack::CapturedTrack(
    MediaStreamTrack* aTrack, uint64_t aWindowID)
    : mTrack(aTrack),
      mPort(aTrack->Graph()->ConnectToCaptureTrack(aWindowID,
                                                   aTrack->GetTrack())) {}

MediaStreamWindowCapturer::CapturedTrack::~CapturedTrack() {
  if (mPort) {
    mPort->Destroy();
  }
}

MediaStreamWindowCapturer::MediaStreamWindowCapturer(DOMMediaStream* aStream,
                                                     uint64_t aWindowId)
    : mStream(aStream), mWindowId(aWindowId) {
  mStream->RegisterTrackListener(this);
  nsTArray<RefPtr<AudioStreamTrack>> tracks;
  mStream->GetAudioTracks(tracks);
  for (const auto& t : tracks) {
    if (t->Ended()) {
      continue;
    }
    AddTrack(t);
  }
}

MediaStreamWindowCapturer::~MediaStreamWindowCapturer() {
  if (mStream) {
    mStream->UnregisterTrackListener(this);
  }
}

void MediaStreamWindowCapturer::NotifyTrackAdded(
    const RefPtr<MediaStreamTrack>& aTrack) {
  if (AudioStreamTrack* at = aTrack->AsAudioStreamTrack()) {
    AddTrack(at);
  }
}

void MediaStreamWindowCapturer::NotifyTrackRemoved(
    const RefPtr<MediaStreamTrack>& aTrack) {
  if (AudioStreamTrack* at = aTrack->AsAudioStreamTrack()) {
    RemoveTrack(at);
  }
}

void MediaStreamWindowCapturer::AddTrack(AudioStreamTrack* aTrack) {
  if (aTrack->Ended()) {
    return;
  }
  mTracks.AppendElement(MakeUnique<CapturedTrack>(aTrack, mWindowId));
}

void MediaStreamWindowCapturer::RemoveTrack(AudioStreamTrack* aTrack) {
  for (size_t i = mTracks.Length(); i > 0; --i) {
    if (mTracks[i - 1]->mTrack == aTrack) {
      mTracks.RemoveElementAt(i - 1);
      break;
    }
  }
}
}  // namespace mozilla
