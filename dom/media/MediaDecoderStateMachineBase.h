/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_MEDIADECODERSTATEMACHINEBASE_H_
#define DOM_MEDIA_MEDIADECODERSTATEMACHINEBASE_H_

#include "DecoderDoctorDiagnostics.h"
#include "MediaDecoder.h"
#include "MediaDecoderOwner.h"
#include "MediaEventSource.h"
#include "MediaInfo.h"
#include "MediaMetadataManager.h"
#include "ReaderProxy.h"
#include "VideoFrameContainer.h"
#include "mozilla/dom/MediaDebugInfoBinding.h"
#include "mozilla/Variant.h"
#include "nsISupportsImpl.h"

class AudioDeviceInfo;

namespace mozilla {

class AbstractThread;
class FrameStatistics;
class MediaFormatReader;
class TaskQueue;

struct MediaPlaybackEvent {
  enum EventType {
    PlaybackStarted,
    PlaybackStopped,
    PlaybackProgressed,
    PlaybackEnded,
    SeekStarted,
    Invalidate,
    EnterVideoSuspend,
    ExitVideoSuspend,
    StartVideoSuspendTimer,
    CancelVideoSuspendTimer,
    VideoOnlySeekBegin,
    VideoOnlySeekCompleted,
  } mType;

  using DataType = Variant<Nothing, int64_t>;
  DataType mData;

  MOZ_IMPLICIT MediaPlaybackEvent(EventType aType)
      : mType(aType), mData(Nothing{}) {}

  template <typename T>
  MediaPlaybackEvent(EventType aType, T&& aArg)
      : mType(aType), mData(std::forward<T>(aArg)) {}
};

enum class VideoDecodeMode : uint8_t { Normal, Suspend };

/**
 * The state machine class. This manages the decoding and seeking in the
 * MediaDecoderReader on the decode task queue, and A/V sync on the shared
 * state machine thread, and controls the audio "push" thread.
 *
 * All internal state is synchronised via the decoder monitor. State changes
 * are propagated by scheduling the state machine to run another cycle on the
 * shared state machine thread.
 */
class MediaDecoderStateMachineBase {
 public:
  NS_INLINE_DECL_PURE_VIRTUAL_REFCOUNTING

  using FirstFrameEventSourceExc =
      MediaEventSourceExc<UniquePtr<MediaInfo>, MediaDecoderEventVisibility>;
  using MetadataEventSourceExc =
      MediaEventSourceExc<UniquePtr<MediaInfo>, UniquePtr<MetadataTags>,
                          MediaDecoderEventVisibility>;
  using NextFrameStatus = MediaDecoderOwner::NextFrameStatus;

  MediaDecoderStateMachineBase(MediaDecoder* aDecoder,
                               MediaFormatReader* aReader);

  virtual nsresult Init(MediaDecoder* aDecoder);

  RefPtr<ShutdownPromise> BeginShutdown();

  // Seeks to the decoder to aTarget asynchronously.
  RefPtr<MediaDecoder::SeekPromise> InvokeSeek(const SeekTarget& aTarget);

  virtual size_t SizeOfVideoQueue() const = 0;
  virtual size_t SizeOfAudioQueue() const = 0;

  // Sets the video decode mode. Used by the suspend-video-decoder feature.
  virtual void SetVideoDecodeMode(VideoDecodeMode aMode) = 0;

  virtual RefPtr<GenericPromise> InvokeSetSink(
      const RefPtr<AudioDeviceInfo>& aSink) = 0;
  virtual void InvokeSuspendMediaSink() = 0;
  virtual void InvokeResumeMediaSink() = 0;

  virtual RefPtr<GenericPromise> RequestDebugInfo(
      dom::MediaDecoderStateMachineDebugInfo& aInfo) = 0;

  // Returns the state machine task queue.
  TaskQueue* OwnerThread() const { return mTaskQueue; }

  MetadataEventSourceExc& MetadataLoadedEvent() { return mMetadataLoadedEvent; }

  FirstFrameEventSourceExc& FirstFrameLoadedEvent() {
    return mFirstFrameLoadedEvent;
  }

  MediaEventSourceExc<RefPtr<VideoFrameContainer>>&
  OnSecondaryVideoContainerInstalled() {
    return mOnSecondaryVideoContainerInstalled;
  }

  TimedMetadataEventSource& TimedMetadataEvent() {
    return mMetadataManager.TimedMetadataEvent();
  }

  MediaEventSource<MediaPlaybackEvent>& OnPlaybackEvent() {
    return mOnPlaybackEvent;
  }
  MediaEventSource<MediaResult>& OnPlaybackErrorEvent() {
    return mOnPlaybackErrorEvent;
  }

  MediaEventSource<DecoderDoctorEvent>& OnDecoderDoctorEvent() {
    return mOnDecoderDoctorEvent;
  }

  MediaEventSource<NextFrameStatus>& OnNextFrameStatus() {
    return mOnNextFrameStatus;
  }

  MediaEventProducer<VideoInfo, AudioInfo>& OnTrackInfoUpdatedEvent() {
    return mReader->OnTrackInfoUpdatedEvent();
  }

  MediaEventSource<void>& OnMediaNotSeekable() const;

  AbstractCanonical<media::NullableTimeUnit>* CanonicalDuration() {
    return &mDuration;
  }
  AbstractCanonical<media::TimeUnit>* CanonicalCurrentPosition() {
    return &mCurrentPosition;
  }
  AbstractCanonical<bool>* CanonicalIsAudioDataAudible() {
    return &mIsAudioDataAudible;
  }
  AbstractCanonical<media::TimeIntervals>* CanonicalBuffered() const;

  void DispatchSetFragmentEndTime(const media::TimeUnit& aEndTime);
  void DispatchCanPlayThrough(bool aCanPlayThrough);
  void DispatchIsLiveStream(bool aIsLiveStream);
  void DispatchSetPlaybackRate(double aPlaybackRate);

 protected:
  virtual ~MediaDecoderStateMachineBase() = default;

  bool HasAudio() const { return mInfo.ref().HasAudio(); }
  bool HasVideo() const { return mInfo.ref().HasVideo(); }
  const MediaInfo& Info() const { return mInfo.ref(); }

  virtual void SetPlaybackRate(double aPlaybackRate) = 0;
  virtual void SetIsLiveStream(bool aIsLiveStream) = 0;
  virtual void SetCanPlayThrough(bool aCanPlayThrough) = 0;
  virtual void SetFragmentEndTime(const media::TimeUnit& aFragmentEndTime) = 0;

  virtual void BufferedRangeUpdated() = 0;
  virtual void VolumeChanged() = 0;
  virtual void PreservesPitchChanged() = 0;
  virtual void PlayStateChanged() = 0;
  virtual void LoopingChanged() = 0;

  // Init tasks which should be done on the task queue.
  virtual void InitializationTask(MediaDecoder* aDecoder);

  virtual RefPtr<ShutdownPromise> Shutdown() = 0;

  virtual RefPtr<MediaDecoder::SeekPromise> Seek(const SeekTarget& aTarget) = 0;

  void DecodeError(const MediaResult& aError);

  // Functions used by assertions to ensure we're calling things
  // on the appropriate threads.
  bool OnTaskQueue() const;

  bool IsRequestingAudioData() const { return mAudioDataRequest.Exists(); }
  bool IsRequestingVideoData() const { return mVideoDataRequest.Exists(); }
  bool IsWaitingAudioData() const { return mAudioWaitRequest.Exists(); }
  bool IsWaitingVideoData() const { return mVideoWaitRequest.Exists(); }

  void* const mDecoderID;
  const RefPtr<AbstractThread> mAbstractMainThread;
  const RefPtr<FrameStatistics> mFrameStats;
  const RefPtr<VideoFrameContainer> mVideoFrameContainer;
  const RefPtr<TaskQueue> mTaskQueue;
  const RefPtr<ReaderProxy> mReader;
  mozilla::MediaMetadataManager mMetadataManager;

  // Playback rate. 1.0 : normal speed, 0.5 : two times slower.
  double mPlaybackRate;

  // Event producers
  MediaEventProducerExc<UniquePtr<MediaInfo>, UniquePtr<MetadataTags>,
                        MediaDecoderEventVisibility>
      mMetadataLoadedEvent;
  MediaEventProducerExc<UniquePtr<MediaInfo>, MediaDecoderEventVisibility>
      mFirstFrameLoadedEvent;
  MediaEventProducerExc<RefPtr<VideoFrameContainer>>
      mOnSecondaryVideoContainerInstalled;
  MediaEventProducer<MediaPlaybackEvent> mOnPlaybackEvent;
  MediaEventProducer<MediaResult> mOnPlaybackErrorEvent;
  MediaEventProducer<DecoderDoctorEvent> mOnDecoderDoctorEvent;
  MediaEventProducer<NextFrameStatus> mOnNextFrameStatus;

  // The buffered range. Mirrored from the decoder thread.
  Mirror<media::TimeIntervals> mBuffered;

  // The current play state, mirrored from the main thread.
  Mirror<MediaDecoder::PlayState> mPlayState;

  // Volume of playback. 0.0 = muted. 1.0 = full volume.
  Mirror<double> mVolume;

  // Pitch preservation for the playback rate.
  Mirror<bool> mPreservesPitch;

  // Whether to seek back to the start of the media resource
  // upon reaching the end.
  Mirror<bool> mLooping;

  // Duration of the media. This is guaranteed to be non-null after we finish
  // decoding the first frame.
  Canonical<media::NullableTimeUnit> mDuration;

  // The time of the current frame, corresponding to the "current
  // playback position" in HTML5. This is referenced from 0, which is the
  // initial playback position.
  Canonical<media::TimeUnit> mCurrentPosition;

  // Used to distinguish whether the audio is producing sound.
  Canonical<bool> mIsAudioDataAudible;

  // Stores presentation info required for playback.
  Maybe<MediaInfo> mInfo;

  // True if the media is seekable (i.e. supports random access).
  bool mMediaSeekable = true;

  // True if the media is seekable only in buffered ranges.
  bool mMediaSeekableOnlyInBufferedRanges = false;

  // True if we've decoded first frames (thus having the start time) and
  // notified the FirstFrameLoaded event. Note we can't initiate seek until the
  // start time is known which happens when the first frames are decoded or we
  // are playing an MSE stream (the start time is always assumed 0).
  bool mSentFirstFrameLoadedEvent = false;

  // True if we should not decode/preroll unnecessary samples, unless we're
  // played. "Prerolling" in this context refers to when we decode and
  // buffer decoded samples in advance of when they're needed for playback.
  // This flag is set for preload=metadata media, and means we won't
  // decode more than the first video frame and first block of audio samples
  // for that media when we startup, or after a seek. When Play() is called,
  // we reset this flag, as we assume the user is playing the media, so
  // prerolling is appropriate then. This flag is used to reduce the overhead
  // of prerolling samples for media elements that may not play, both
  // memory and CPU overhead.
  bool mMinimizePreroll;

  // Only one of a given pair of ({Audio,Video}DataPromise, WaitForDataPromise)
  // should exist at any given moment.
  using AudioDataPromise = MediaFormatReader::AudioDataPromise;
  using VideoDataPromise = MediaFormatReader::VideoDataPromise;
  using WaitForDataPromise = MediaFormatReader::WaitForDataPromise;
  MozPromiseRequestHolder<AudioDataPromise> mAudioDataRequest;
  MozPromiseRequestHolder<VideoDataPromise> mVideoDataRequest;
  MozPromiseRequestHolder<WaitForDataPromise> mAudioWaitRequest;
  MozPromiseRequestHolder<WaitForDataPromise> mVideoWaitRequest;

 private:
  WatchManager<MediaDecoderStateMachineBase> mWatchManager;
};

}  // namespace mozilla

#endif  // DOM_MEDIA_MEDIADECODERSTATEMACHINEBASE_H_
