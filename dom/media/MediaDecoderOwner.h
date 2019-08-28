/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef MediaDecoderOwner_h_
#define MediaDecoderOwner_h_

#include "mozilla/UniquePtr.h"
#include "MediaInfo.h"
#include "MediaSegment.h"
#include "nsSize.h"
#include "TrackID.h"

namespace mozilla {

class AbstractThread;
class GMPCrashHelper;
class VideoFrameContainer;
class MediaInfo;
class MediaResult;

namespace dom {
class Document;
class HTMLMediaElement;
}  // namespace dom

class MediaDecoderOwner {
 public:
  // Called by the media decoder to indicate that the download is progressing.
  virtual void DownloadProgressed() = 0;

  // Dispatch an asynchronous event to the decoder owner
  virtual void DispatchAsyncEvent(const nsAString& aName) = 0;

  // Triggers a recomputation of readyState.
  virtual void UpdateReadyState() = 0;

  /**
   * Fires a timeupdate event. If aPeriodic is true, the event will only
   * be fired if we've not fired a timeupdate event (for any reason) in the
   * last 250ms, as required by the spec when the current time is periodically
   * increasing during playback.
   */
  virtual void FireTimeUpdate(bool aPeriodic) = 0;

  // Return true if decoding should be paused
  virtual bool GetPaused() = 0;

  // Called by the video decoder object, on the main thread,
  // when it has read the metadata containing video dimensions,
  // etc.
  // Must take ownership of MetadataTags aTags argument.
  virtual void MetadataLoaded(const MediaInfo* aInfo,
                              UniquePtr<const MetadataTags> aTags) = 0;

  // Called by the decoder object, on the main thread,
  // when it has read the first frame of the video or audio.
  virtual void FirstFrameLoaded() = 0;

  // Called by the decoder object, on the main thread,
  // when the resource has a network error during loading.
  // The decoder owner should call Shutdown() on the decoder and drop the
  // reference to the decoder to prevent further calls into the decoder.
  virtual void NetworkError(const MediaResult& aError) = 0;

  // Called by the decoder object, on the main thread, when the
  // resource has a decode error during metadata loading or decoding.
  // The decoder owner should call Shutdown() on the decoder and drop the
  // reference to the decoder to prevent further calls into the decoder.
  virtual void DecodeError(const MediaResult& aError) = 0;

  // Called by the decoder object, on the main thread, when the
  // resource has a decode issue during metadata loading or decoding, but can
  // continue decoding.
  virtual void DecodeWarning(const MediaResult& aError) = 0;

  // Return true if media element error attribute is not null.
  virtual bool HasError() const = 0;

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

  // Called by the media decoder to indicate whether the media cache has
  // suspended the channel.
  virtual void NotifySuspendedByCache(bool aSuspendedByCache) = 0;

  // called to notify that the principal of the decoder's media resource has
  // changed.
  virtual void NotifyDecoderPrincipalChanged() = 0;

  // The status of the next frame which might be available from the decoder
  enum NextFrameStatus {
    // The next frame of audio/video is available
    NEXT_FRAME_AVAILABLE,
    // The next frame of audio/video is unavailable because the decoder
    // is paused while it buffers up data
    NEXT_FRAME_UNAVAILABLE_BUFFERING,
    // The next frame of audio/video is unavailable for the decoder is seeking.
    NEXT_FRAME_UNAVAILABLE_SEEKING,
    // The next frame of audio/video is unavailable for some other reasons
    NEXT_FRAME_UNAVAILABLE,
    // Sentinel value
    NEXT_FRAME_UNINITIALIZED
  };

  // Called by media decoder when the audible state changed
  virtual void SetAudibleState(bool aAudible) = 0;

  // Notified by the decoder that XPCOM shutdown has begun.
  // The decoder owner should call Shutdown() on the decoder and drop the
  // reference to the decoder to prevent further calls into the decoder.
  virtual void NotifyXPCOMShutdown() = 0;

  // Dispatches a "encrypted" event to the HTMLMediaElement, with the
  // provided init data. Actual dispatch may be delayed until HAVE_METADATA.
  // Main thread only.
  virtual void DispatchEncrypted(const nsTArray<uint8_t>& aInitData,
                                 const nsAString& aInitDataType) = 0;

  // Called by the media decoder to create audio/video tracks and add to its
  // owner's track list.
  virtual void ConstructMediaTracks(const MediaInfo* aInfo) = 0;

  // Called by the media decoder to removes all audio/video tracks from its
  // owner's track list.
  virtual void RemoveMediaTracks() = 0;

  // Called by the media decoder to notify the owner to resolve a seek promise.
  virtual void AsyncResolveSeekDOMPromiseIfExists() = 0;

  // Called by the media decoder to notify the owner to reject a seek promise.
  virtual void AsyncRejectSeekDOMPromiseIfExists() = 0;

  // Notified by the decoder that a decryption key is required before emitting
  // further output.
  virtual void NotifyWaitingForKey() {}

  /*
   * Methods that are used only in Gecko go here. We provide defaul
   * implementations so they can compile in Servo without modification.
   */
  // Return an abstract thread on which to run main thread runnables.
  virtual AbstractThread* AbstractMainThread() const { return nullptr; }

  // Get the HTMLMediaElement object if the decoder is being used from an
  // HTML media element, and null otherwise.
  virtual dom::HTMLMediaElement* GetMediaElement() { return nullptr; }

  // Called by the media decoder and the video frame to get the
  // ImageContainer containing the video data.
  virtual VideoFrameContainer* GetVideoFrameContainer() { return nullptr; }

  // Return the decoder owner's owner document.
  virtual mozilla::dom::Document* GetDocument() const { return nullptr; }

  // Called by the media decoder to create a GMPCrashHelper.
  virtual already_AddRefed<GMPCrashHelper> CreateGMPCrashHelper() {
    return nullptr;
  }

  // Called by the frame container to notify the layout engine that the
  // size of the image has changed, or the video needs to be be repainted
  // for some other reason.
  virtual void Invalidate(bool aImageSizeChanged,
                          Maybe<nsIntSize>& aNewIntrinsicSize,
                          bool aForceInvalidate) {}

  // Called after the MediaStream we're playing rendered a frame to aContainer
  // with a different principalHandle than the previous frame.
  virtual void PrincipalHandleChangedForVideoFrameContainer(
      VideoFrameContainer* aContainer,
      const PrincipalHandle& aNewPrincipalHandle) {}

  /*
   * Servo only methods go here. Please provide default implementations so they
   * can build in Gecko without any modification.
   */
};

}  // namespace mozilla

#endif
