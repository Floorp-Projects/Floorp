/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#if !defined(AudioSink_h__)
#define AudioSink_h__

#include "mozilla/MozPromise.h"
#include "mozilla/RefPtr.h"
#include "nsISupportsImpl.h"

#include "MediaSink.h"

namespace mozilla {

class MediaData;
template <class T> class MediaQueue;

namespace media {

/*
 * Define basic APIs for derived class instance to operate or obtain
 * information from it.
 */
class AudioSink {
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(AudioSink)
  AudioSink(MediaQueue<MediaData>& aAudioQueue)
    : mAudioQueue(aAudioQueue)
  {}

  typedef MediaSink::PlaybackParams PlaybackParams;

  // Return a promise which will be resolved when AudioSink finishes playing,
  // or rejected if any error.
  virtual RefPtr<GenericPromise> Init(const PlaybackParams& aParams) = 0;

  virtual int64_t GetEndTime() const = 0;
  virtual int64_t GetPosition() = 0;

  // Check whether we've pushed more frames to the audio
  // hardware than it has played.
  virtual bool HasUnplayedFrames() = 0;

  // Shut down the AudioSink's resources.
  virtual void Shutdown() = 0;

  // Change audio playback setting.
  virtual void SetVolume(double aVolume) = 0;
  virtual void SetPlaybackRate(double aPlaybackRate) = 0;
  virtual void SetPreservesPitch(bool aPreservesPitch) = 0;

  // Change audio playback status pause/resume.
  virtual void SetPlaying(bool aPlaying) = 0;

protected:
  virtual ~AudioSink() {}

  virtual MediaQueue<MediaData>& AudioQueue() const {
    return mAudioQueue;
  }

  // To queue audio data (no matter it's plain or encoded or encrypted, depends
  // on the subclass)
  MediaQueue<MediaData>& mAudioQueue;
};

} // namespace media
} // namespace mozilla

#endif
