/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MediaSink_h_
#define MediaSink_h_

#include "mozilla/RefPtr.h"
#include "mozilla/MozPromise.h"
#include "nsISupportsImpl.h"
#include "MediaInfo.h"

namespace mozilla {

class TimeStamp;

namespace media {

/**
 * A consumer of audio/video data which plays audio and video tracks and
 * manages A/V sync between them.
 *
 * A typical sink sends audio/video outputs to the speaker and screen.
 * However, there are also sinks which capture the output of an media element
 * and send the output to a MediaStream.
 *
 * This class is used to move A/V sync management and audio/video rendering
 * out of MDSM so it is possible for subclasses to do external rendering using
 * specific hardware which is required by TV projects and CDM.
 *
 * Note this class is not thread-safe and should be called from the state
 * machine thread only.
 */
class MediaSink {
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MediaSink);
  typedef mozilla::TrackInfo::TrackType TrackType;

  struct PlaybackParams {
    PlaybackParams()
      : mVolume(1.0) , mPlaybackRate(1.0) , mPreservesPitch(true) {}
    double mVolume;
    double mPlaybackRate;
    bool mPreservesPitch;
  };

  // Return the playback parameters of this sink.
  // Can be called in any state.
  virtual const PlaybackParams& GetPlaybackParams() const = 0;

  // Set the playback parameters of this sink.
  // Can be called in any state.
  virtual void SetPlaybackParams(const PlaybackParams& aParams) = 0;

  // Return a promise which is resolved when the track finishes
  // or null if no such track.
  // Must be called after playback starts.
  virtual RefPtr<GenericPromise> OnEnded(TrackType aType) = 0;

  // Return the end time of the audio/video data that has been consumed
  // or -1 if no such track.
  // Must be called after playback starts.
  virtual int64_t GetEndTime(TrackType aType) const = 0;

  // Return playback position of the media.
  // Since A/V sync is always maintained by this sink, there is no need to
  // specify whether we want to get audio or video position.
  // aTimeStamp returns the timeStamp corresponding to the returned position
  // which is used by the compositor to derive the render time of video frames.
  // Must be called after playback starts.
  virtual int64_t GetPosition(TimeStamp* aTimeStamp = nullptr) const = 0;

  // Return true if there are data consumed but not played yet.
  // Can be called in any state.
  virtual bool HasUnplayedFrames(TrackType aType) const = 0;

  // Set volume of the audio track.
  // Do nothing if this sink has no audio track.
  // Can be called in any state.
  virtual void SetVolume(double aVolume) {}

  // Set the playback rate.
  // Can be called in any state.
  virtual void SetPlaybackRate(double aPlaybackRate) {}

  // Whether to preserve pitch of the audio track.
  // Do nothing if this sink has no audio track.
  // Can be called in any state.
  virtual void SetPreservesPitch(bool aPreservesPitch) {}

  // Pause/resume the playback. Only work after playback starts.
  virtual void SetPlaying(bool aPlaying) = 0;

  // Single frame rendering operation may need to be done before playback
  // started (1st frame) or right after seek completed or playback stopped.
  // Do nothing if this sink has no video track. Can be called in any state.
  virtual void Redraw(const VideoInfo& aInfo) {};

  // Begin a playback session with the provided start time and media info.
  // Must be called when playback is stopped.
  virtual void Start(int64_t aStartTime, const MediaInfo& aInfo) = 0;

  // Finish a playback session.
  // Must be called after playback starts.
  virtual void Stop() = 0;

  // Return true if playback has started.
  // Can be called in any state.
  virtual bool IsStarted() const = 0;

  // Return true if playback is started and not paused otherwise false.
  // Can be called in any state.
  virtual bool IsPlaying() const = 0;

  // Called on the state machine thread to shut down the sink. All resources
  // allocated by this sink should be released.
  // Must be called after playback stopped.
  virtual void Shutdown() {}

protected:
  virtual ~MediaSink() {}
};

} // namespace media
} // namespace mozilla

#endif //MediaSink_h_
