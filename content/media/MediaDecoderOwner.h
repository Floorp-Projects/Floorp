/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef MediaDecoderOwner_h_
#define MediaDecoderOwner_h_
#include "AbstractMediaDecoder.h"

class nsHTMLMediaElement;

namespace mozilla {

class VideoFrameContainer;

class MediaDecoderOwner
{
public:
  // Called by the media decoder to indicate that the download has stalled
  // (no data has arrived for a while).
  virtual void DownloadStalled() = 0;

  // Dispatch a synchronous event to the decoder owner
  virtual nsresult DispatchEvent(const nsAString& aName) = 0;

  // Dispatch an asynchronous event to the decoder owner
  virtual nsresult DispatchAsyncEvent(const nsAString& aName) = 0;

  /**
   * Fires a timeupdate event. If aPeriodic is true, the event will only
   * be fired if we've not fired a timeupdate event (for any reason) in the
   * last 250ms, as required by the spec when the current time is periodically
   * increasing during playback.
   */
  virtual void FireTimeUpdate(bool aPeriodic) = 0;

  // Get the nsHTMLMediaElement object if the decoder is being used from an
  // HTML media element, and null otherwise.
  virtual nsHTMLMediaElement* GetMediaElement()
  {
    return nullptr;
  }

  // Return true if decoding should be paused
  virtual bool GetPaused() = 0;

  /**
   * Called when data has been written to the underlying audio stream.
   */
  virtual void NotifyAudioAvailable(float* aFrameBuffer, uint32_t aFrameBufferLength,
                                    float aTime) = 0;

  // Called by the video decoder object, on the main thread,
  // when it has read the metadata containing video dimensions,
  // etc.
  virtual void MetadataLoaded(int aChannels,
                              int aRate,
                              bool aHasAudio,
                              bool aHasVideo,
                              const MetadataTags* aTags) = 0;

  // Called by the video decoder object, on the main thread,
  // when it has read the first frame of the video
  // aResourceFullyLoaded should be true if the resource has been
  // fully loaded and the caller will call ResourceLoaded next.
  virtual void FirstFrameLoaded(bool aResourceFullyLoaded) = 0;

  // Called by the video decoder object, on the main thread,
  // when the resource has completed downloading.
  virtual void ResourceLoaded() = 0;

  // Called by the video decoder object, on the main thread,
  // when the resource has a network error during loading.
  virtual void NetworkError() = 0;

  // Called by the video decoder object, on the main thread, when the
  // resource has a decode error during metadata loading or decoding.
  virtual void DecodeError() = 0;

  // Called by the video decoder object, on the main thread, when the
  // resource load has been cancelled.
  virtual void LoadAborted() = 0;

  // Called by the video decoder object, on the main thread,
  // when the video playback has ended.
  virtual void PlaybackEnded() = 0;

  // Called by the video decoder object, on the main thread,
  // when the resource has started seeking.
  virtual void SeekStarted() = 0;

  // Called by the video decoder object, on the main thread,
  // when the resource has completed seeking.
  virtual void SeekCompleted() = 0;

  // Called by the media stream, on the main thread, when the download
  // has been suspended by the cache or because the element itself
  // asked the decoder to suspend the download.
  virtual void DownloadSuspended() = 0;

  // Called by the media stream, on the main thread, when the download
  // has been resumed by the cache or because the element itself
  // asked the decoder to resumed the download.
  // If aForceNetworkLoading is True, ignore the fact that the download has
  // previously finished. We are downloading the middle of the media after
  // having downloaded the end, we need to notify the element a download in
  // ongoing.
  virtual void DownloadResumed(bool aForceNetworkLoading = false) = 0;

  // Notify that enough data has arrived to start autoplaying.
  // If the element is 'autoplay' and is ready to play back (not paused,
  // autoplay pref enabled, etc), it should start playing back.
  virtual void NotifyAutoplayDataReady() = 0;

  // Called by the media decoder to indicate whether the media cache has
  // suspended the channel.
  virtual void NotifySuspendedByCache(bool aIsSuspended) = 0;

  // called to notify that the principal of the decoder's media resource has changed.
  virtual void NotifyDecoderPrincipalChanged() = 0;

  // The status of the next frame which might be available from the decoder
  enum NextFrameStatus {
    // The next frame of audio/video is available
    NEXT_FRAME_AVAILABLE,
    // The next frame of audio/video is unavailable because the decoder
    // is paused while it buffers up data
    NEXT_FRAME_UNAVAILABLE_BUFFERING,
    // The next frame of audio/video is unavailable for some other reasons
    NEXT_FRAME_UNAVAILABLE,
    // Sentinel value
    NEXT_FRAME_UNINITIALIZED
  };

  // Called by the decoder when some data has been downloaded or
  // buffering/seeking has ended. aNextFrameAvailable is true when
  // the data for the next frame is available. This method will
  // decide whether to set the ready state to HAVE_CURRENT_DATA,
  // HAVE_FUTURE_DATA or HAVE_ENOUGH_DATA.
  virtual void UpdateReadyStateForData(NextFrameStatus aNextFrame) = 0;

  // Called by the media decoder and the video frame to get the
  // ImageContainer containing the video data.
  virtual VideoFrameContainer* GetVideoFrameContainer() = 0;
};

}

#endif

