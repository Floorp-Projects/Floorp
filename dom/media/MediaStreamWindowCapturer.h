/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MediaStreamWindowCapturer_h
#define MediaStreamWindowCapturer_h

#include "DOMMediaStream.h"

namespace mozilla {
namespace dom {
class AudioStreamTrack;
class MediaStreamTrack;
}  // namespace dom

/**
 * Given a DOMMediaStream and a window id, this class will pipe the audio from
 * all live audio tracks in the stream to the MediaStreamGraph's window capture
 * mechanism.
 */
class MediaStreamWindowCapturer : public DOMMediaStream::TrackListener {
 public:
  MediaStreamWindowCapturer(DOMMediaStream* aStream, uint64_t aWindowId);
  ~MediaStreamWindowCapturer();

  void NotifyTrackAdded(const RefPtr<dom::MediaStreamTrack>& aTrack) override;
  void NotifyTrackRemoved(const RefPtr<dom::MediaStreamTrack>& aTrack) override;

  struct CapturedTrack {
    CapturedTrack(dom::MediaStreamTrack* aTrack, uint64_t aWindowID);
    ~CapturedTrack();

    const WeakPtr<dom::MediaStreamTrack> mTrack;
    const RefPtr<MediaInputPort> mPort;
  };

  const WeakPtr<DOMMediaStream> mStream;
  const uint64_t mWindowId;

 protected:
  void AddTrack(dom::AudioStreamTrack* aTrack);
  void RemoveTrack(dom::AudioStreamTrack* aTrack);

  nsTArray<UniquePtr<CapturedTrack>> mTracks;
};
}  // namespace mozilla

#endif /* MediaStreamWindowCapturer_h */
