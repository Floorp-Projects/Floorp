/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*

Each media element for a media file has one thread called the "audio thread".

The audio thread  writes the decoded audio data to the audio
hardware. This is done in a separate thread to ensure that the
audio hardware gets a constant stream of data without
interruption due to decoding or display. At some point
AudioStream will be refactored to have a callback interface
where it asks for data and this thread will no longer be
needed.

The element/state machine also has a TaskQueue which runs in a
SharedThreadPool that is shared with all other elements/decoders. The state
machine dispatches tasks to this to call into the MediaDecoderReader to
request decoded audio or video data. The Reader will callback with decoded
sampled when it has them available, and the state machine places the decoded
samples into its queues for the consuming threads to pull from.

The MediaDecoderReader can choose to decode asynchronously, or synchronously
and return requested samples synchronously inside it's Request*Data()
functions via callback. Asynchronous decoding is preferred, and should be
used for any new readers.

Synchronisation of state between the thread is done via a monitor owned
by MediaDecoder.

The lifetime of the audio thread is controlled by the state machine when
it runs on the shared state machine thread. When playback needs to occur
the audio thread is created and an event dispatched to run it. The audio
thread exits when audio playback is completed or no longer required.

A/V synchronisation is handled by the state machine. It examines the audio
playback time and compares this to the next frame in the queue of video
frames. If it is time to play the video frame it is then displayed, otherwise
it schedules the state machine to run again at the time of the next frame.

Frame skipping is done in the following ways:

  1) The state machine will skip all frames in the video queue whose
     display time is less than the current audio time. This ensures
     the correct frame for the current time is always displayed.

  2) The decode tasks will stop decoding interframes and read to the
     next keyframe if it determines that decoding the remaining
     interframes will cause playback issues. It detects this by:
       a) If the amount of audio data in the audio queue drops
          below a threshold whereby audio may start to skip.
       b) If the video queue drops below a threshold where it
          will be decoding video data that won't be displayed due
          to the decode thread dropping the frame immediately.
     TODO: In future we should only do this when the Reader is decoding
           synchronously.

When hardware accelerated graphics is not available, YCbCr conversion
is done on the decode task queue when video frames are decoded.

The decode task queue pushes decoded audio and videos frames into two
separate queues - one for audio and one for video. These are kept
separate to make it easy to constantly feed audio data to the audio
hardware while allowing frame skipping of video data. These queues are
threadsafe, and neither the decode, audio, or state machine should
be able to monopolize them, and cause starvation of the other threads.

Both queues are bounded by a maximum size. When this size is reached
the decode tasks will no longer request video or audio depending on the
queue that has reached the threshold. If both queues are full, no more
decode tasks will be dispatched to the decode task queue, so other
decoders will have an opportunity to run.

During playback the audio thread will be idle (via a Wait() on the
monitor) if the audio queue is empty. Otherwise it constantly pops
audio data off the queue and plays it with a blocking write to the audio
hardware (via AudioStream).

*/
#if !defined(MediaDecoderStateMachine_h__)
#  define MediaDecoderStateMachine_h__

#  include "ImageContainer.h"
#  include "MediaDecoder.h"
#  include "MediaDecoderOwner.h"
#  include "MediaEventSource.h"
#  include "MediaFormatReader.h"
#  include "MediaMetadataManager.h"
#  include "MediaQueue.h"
#  include "MediaSink.h"
#  include "MediaStatistics.h"
#  include "MediaTimer.h"
#  include "SeekJob.h"
#  include "mozilla/Attributes.h"
#  include "mozilla/ReentrantMonitor.h"
#  include "mozilla/StateMirroring.h"
#  include "mozilla/dom/MediaDebugInfoBinding.h"
#  include "nsAutoPtr.h"
#  include "nsThreadUtils.h"

namespace mozilla {

class AbstractThread;
class AudioSegment;
class DecodedStream;
class DOMMediaStream;
class OutputStreamManager;
class ReaderProxy;
class TaskQueue;

extern LazyLogModule gMediaDecoderLog;

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

DDLoggedTypeDeclName(MediaDecoderStateMachine);

/*
  The state machine class. This manages the decoding and seeking in the
  MediaDecoderReader on the decode task queue, and A/V sync on the shared
  state machine thread, and controls the audio "push" thread.

  All internal state is synchronised via the decoder monitor. State changes
  are propagated by scheduling the state machine to run another cycle on the
  shared state machine thread.

  See MediaDecoder.h for more details.
*/
class MediaDecoderStateMachine
    : public DecoderDoctorLifeLogger<MediaDecoderStateMachine> {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MediaDecoderStateMachine)

  using TrackSet = MediaFormatReader::TrackSet;

 public:
  typedef MediaDecoderOwner::NextFrameStatus NextFrameStatus;
  typedef mozilla::layers::ImageContainer::FrameID FrameID;
  MediaDecoderStateMachine(MediaDecoder* aDecoder, MediaFormatReader* aReader);

  nsresult Init(MediaDecoder* aDecoder);

  // Enumeration for the valid decoding states
  enum State {
    DECODER_STATE_DECODING_METADATA,
    DECODER_STATE_DORMANT,
    DECODER_STATE_DECODING_FIRSTFRAME,
    DECODER_STATE_DECODING,
    DECODER_STATE_LOOPING_DECODING,
    DECODER_STATE_SEEKING,
    DECODER_STATE_BUFFERING,
    DECODER_STATE_COMPLETED,
    DECODER_STATE_SHUTDOWN
  };

  // Returns the state machine task queue.
  TaskQueue* OwnerThread() const { return mTaskQueue; }

  RefPtr<GenericPromise> RequestDebugInfo(
      dom::MediaDecoderStateMachineDebugInfo& aInfo);

  void SetOutputStreamPrincipal(nsIPrincipal* aPrincipal);
  // If an OutputStreamManager does not exist, one will be created.
  void EnsureOutputStreamManager(SharedDummyStream* aDummyStream);
  // If an OutputStreamManager exists, tracks matching aLoadedInfo will be
  // created unless they already exist in the manager.
  void EnsureOutputStreamManagerHasTracks(const MediaInfo& aLoadedInfo);
  // Add an output stream to the output stream manager. The manager must have
  // been created through EnsureOutputStreamManager() before this.
  void AddOutputStream(DOMMediaStream* aStream);
  // Remove an output stream added with AddOutputStream. If the last output
  // stream was removed, we will also tear down the OutputStreamManager.
  void RemoveOutputStream(DOMMediaStream* aStream);

  // Seeks to the decoder to aTarget asynchronously.
  RefPtr<MediaDecoder::SeekPromise> InvokeSeek(const SeekTarget& aTarget);

  void DispatchSetPlaybackRate(double aPlaybackRate) {
    OwnerThread()->DispatchStateChange(NewRunnableMethod<double>(
        "MediaDecoderStateMachine::SetPlaybackRate", this,
        &MediaDecoderStateMachine::SetPlaybackRate, aPlaybackRate));
  }

  RefPtr<ShutdownPromise> BeginShutdown();

  // Set the media fragment end time.
  void DispatchSetFragmentEndTime(const media::TimeUnit& aEndTime) {
    RefPtr<MediaDecoderStateMachine> self = this;
    nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction(
        "MediaDecoderStateMachine::DispatchSetFragmentEndTime",
        [self, aEndTime]() {
          // A negative number means we don't have a fragment end time at all.
          self->mFragmentEndTime = aEndTime >= media::TimeUnit::Zero()
                                       ? aEndTime
                                       : media::TimeUnit::Invalid();
        });
    nsresult rv = OwnerThread()->Dispatch(r.forget());
    MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));
    Unused << rv;
  }

  void DispatchCanPlayThrough(bool aCanPlayThrough) {
    RefPtr<MediaDecoderStateMachine> self = this;
    nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction(
        "MediaDecoderStateMachine::DispatchCanPlayThrough",
        [self, aCanPlayThrough]() { self->mCanPlayThrough = aCanPlayThrough; });
    OwnerThread()->DispatchStateChange(r.forget());
  }

  void DispatchIsLiveStream(bool aIsLiveStream) {
    RefPtr<MediaDecoderStateMachine> self = this;
    nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction(
        "MediaDecoderStateMachine::DispatchIsLiveStream",
        [self, aIsLiveStream]() { self->mIsLiveStream = aIsLiveStream; });
    OwnerThread()->DispatchStateChange(r.forget());
  }

  TimedMetadataEventSource& TimedMetadataEvent() {
    return mMetadataManager.TimedMetadataEvent();
  }

  MediaEventSource<void>& OnMediaNotSeekable() const;

  MediaEventSourceExc<UniquePtr<MediaInfo>, UniquePtr<MetadataTags>,
                      MediaDecoderEventVisibility>&
  MetadataLoadedEvent() {
    return mMetadataLoadedEvent;
  }

  MediaEventSourceExc<nsAutoPtr<MediaInfo>, MediaDecoderEventVisibility>&
  FirstFrameLoadedEvent() {
    return mFirstFrameLoadedEvent;
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

  size_t SizeOfVideoQueue() const;

  size_t SizeOfAudioQueue() const;

  // Sets the video decode mode. Used by the suspend-video-decoder feature.
  void SetVideoDecodeMode(VideoDecodeMode aMode);

  RefPtr<GenericPromise> InvokeSetSink(RefPtr<AudioDeviceInfo> aSink);

  void SetSecondaryVideoContainer(
      const RefPtr<VideoFrameContainer>& aSecondary);

 private:
  class StateObject;
  class DecodeMetadataState;
  class DormantState;
  class DecodingFirstFrameState;
  class DecodingState;
  class LoopingDecodingState;
  class SeekingState;
  class AccurateSeekingState;
  class NextFrameSeekingState;
  class NextFrameSeekingFromDormantState;
  class VideoOnlySeekingState;
  class BufferingState;
  class CompletedState;
  class ShutdownState;

  static const char* ToStateStr(State aState);
  const char* ToStateStr();

  void GetDebugInfo(dom::MediaDecoderStateMachineDebugInfo& aInfo);

  // Functions used by assertions to ensure we're calling things
  // on the appropriate threads.
  bool OnTaskQueue() const;

  // Initialization that needs to happen on the task queue. This is the first
  // task that gets run on the task queue, and is dispatched from the MDSM
  // constructor immediately after the task queue is created.
  void InitializationTask(MediaDecoder* aDecoder);

  // Sets the audio-captured state and recreates the media sink if needed.
  // A manager must be passed in if setting the audio-captured state to true.
  void SetAudioCaptured(bool aCaptured,
                        OutputStreamManager* aManager = nullptr);

  RefPtr<MediaDecoder::SeekPromise> Seek(const SeekTarget& aTarget);

  RefPtr<ShutdownPromise> Shutdown();

  RefPtr<ShutdownPromise> FinishShutdown();

  // Update the playback position. This can result in a timeupdate event
  // and an invalidate of the frame being dispatched asynchronously if
  // there is no such event currently queued.
  // Only called on the decoder thread. Must be called with
  // the decode monitor held.
  void UpdatePlaybackPosition(const media::TimeUnit& aTime);

  bool HasAudio() const { return mInfo.ref().HasAudio(); }
  bool HasVideo() const { return mInfo.ref().HasVideo(); }
  const MediaInfo& Info() const { return mInfo.ref(); }

  // Schedules the shared state machine thread to run the state machine.
  void ScheduleStateMachine();

  // Invokes ScheduleStateMachine to run in |aTime|,
  // unless it's already scheduled to run earlier, in which case the
  // request is discarded.
  void ScheduleStateMachineIn(const media::TimeUnit& aTime);

  bool HaveEnoughDecodedAudio();
  bool HaveEnoughDecodedVideo();

  // Returns true if we're currently playing. The decoder monitor must
  // be held.
  bool IsPlaying() const;

  // Sets mMediaSeekable to false.
  void SetMediaNotSeekable();

  // Resets all states related to decoding and aborts all pending requests
  // to the decoders.
  void ResetDecode(TrackSet aTracks = TrackSet(TrackInfo::kAudioTrack,
                                               TrackInfo::kVideoTrack));

  void SetVideoDecodeModeInternal(VideoDecodeMode aMode);

  // Set new sink device and restart MediaSink if playback is started.
  // Returned promise will be resolved with true if the playback is
  // started and false if playback is stopped after setting the new sink.
  // Returned promise will be rejected with value NS_ERROR_ABORT
  // if the action fails or it is not supported.
  // If there are multiple pending requests only the last one will be
  // executed, for all previous requests the promise will be resolved
  // with true or false similar to above.
  RefPtr<GenericPromise> SetSink(RefPtr<AudioDeviceInfo> aSink);

 protected:
  virtual ~MediaDecoderStateMachine();

  void BufferedRangeUpdated();

  void ReaderSuspendedChanged();

  // Inserts a sample into the Audio/Video queue.
  // aSample must not be null.
  void PushAudio(AudioData* aSample);
  void PushVideo(VideoData* aSample);

  void OnAudioPopped(const RefPtr<AudioData>& aSample);
  void OnVideoPopped(const RefPtr<VideoData>& aSample);

  void AudioAudibleChanged(bool aAudible);

  void VolumeChanged();
  void SetPlaybackRate(double aPlaybackRate);
  void PreservesPitchChanged();
  void LoopingChanged();

  MediaQueue<AudioData>& AudioQueue() { return mAudioQueue; }
  MediaQueue<VideoData>& VideoQueue() { return mVideoQueue; }

  // True if we are low in decoded audio/video data.
  // May not be invoked when mReader->UseBufferingHeuristics() is false.
  bool HasLowDecodedData();

  bool HasLowDecodedAudio();

  bool HasLowDecodedVideo();

  bool OutOfDecodedAudio();

  bool OutOfDecodedVideo() {
    MOZ_ASSERT(OnTaskQueue());
    return IsVideoDecoding() && VideoQueue().GetSize() <= 1;
  }

  // Returns true if we're running low on buffered data.
  bool HasLowBufferedData();

  // Returns true if we have less than aThreshold of buffered data available.
  bool HasLowBufferedData(const media::TimeUnit& aThreshold);

  // Return the current time, either the audio clock if available (if the media
  // has audio, and the playback is possible), or a clock for the video.
  // Called on the state machine thread.
  // If aTimeStamp is non-null, set *aTimeStamp to the TimeStamp corresponding
  // to the returned stream time.
  media::TimeUnit GetClock(TimeStamp* aTimeStamp = nullptr) const;

  // Update only the state machine's current playback position (and duration,
  // if unknown).  Does not update the playback position on the decoder or
  // media element -- use UpdatePlaybackPosition for that.  Called on the state
  // machine thread, caller must hold the decoder lock.
  void UpdatePlaybackPositionInternal(const media::TimeUnit& aTime);

  // Update playback position and trigger next update by default time period.
  // Called on the state machine thread.
  void UpdatePlaybackPositionPeriodically();

  MediaSink* CreateAudioSink();

  // Always create mediasink which contains an AudioSink or StreamSink inside.
  // A manager must be passed in if aAudioCaptured is true.
  already_AddRefed<MediaSink> CreateMediaSink(
      bool aAudioCaptured, OutputStreamManager* aManager = nullptr);

  // Stops the media sink and shut it down.
  // The decoder monitor must be held with exactly one lock count.
  // Called on the state machine thread.
  void StopMediaSink();

  // Create and start the media sink.
  // The decoder monitor must be held with exactly one lock count.
  // Called on the state machine thread.
  // If start fails an NS_ERROR_FAILURE is returned.
  nsresult StartMediaSink();

  // Notification method invoked when mPlayState changes.
  void PlayStateChanged();

  // Notification method invoked when mIsVisible changes.
  void VisibilityChanged();

  // Sets internal state which causes playback of media to pause.
  // The decoder monitor must be held.
  void StopPlayback();

  // If the conditions are right, sets internal state which causes playback
  // of media to begin or resume.
  // Must be called with the decode monitor held.
  void MaybeStartPlayback();

  // Moves the decoder into the shutdown state, and dispatches an error
  // event to the media element. This begins shutting down the decoder.
  // The decoder monitor must be held. This is only called on the
  // decode thread.
  void DecodeError(const MediaResult& aError);

  void EnqueueFirstFrameLoadedEvent();

  // Start a task to decode audio.
  void RequestAudioData();

  // Start a task to decode video.
  void RequestVideoData(const media::TimeUnit& aCurrentTime);

  void WaitForData(MediaData::Type aType);

  bool IsRequestingAudioData() const { return mAudioDataRequest.Exists(); }
  bool IsRequestingVideoData() const { return mVideoDataRequest.Exists(); }
  bool IsWaitingAudioData() const { return mAudioWaitRequest.Exists(); }
  bool IsWaitingVideoData() const { return mVideoWaitRequest.Exists(); }

  // Returns the "media time". This is the absolute time which the media
  // playback has reached. i.e. this returns values in the range
  // [mStartTime, mEndTime], and mStartTime will not be 0 if the media does
  // not start at 0. Note this is different than the "current playback
  // position", which is in the range [0,duration].
  media::TimeUnit GetMediaTime() const {
    MOZ_ASSERT(OnTaskQueue());
    return mCurrentPosition;
  }

  // Returns an upper bound on the number of microseconds of audio that is
  // decoded and playable. This is the sum of the number of usecs of audio which
  // is decoded and in the reader's audio queue, and the usecs of unplayed audio
  // which has been pushed to the audio hardware for playback. Note that after
  // calling this, the audio hardware may play some of the audio pushed to
  // hardware, so this can only be used as a upper bound. The decoder monitor
  // must be held when calling this. Called on the decode thread.
  media::TimeUnit GetDecodedAudioDuration();

  void FinishDecodeFirstFrame();

  // Performs one "cycle" of the state machine.
  void RunStateMachine();

  bool IsStateMachineScheduled() const;

  // These return true if the respective stream's decode has not yet reached
  // the end of stream.
  bool IsAudioDecoding();
  bool IsVideoDecoding();

 private:
  // Resolved by the MediaSink to signal that all audio/video outstanding
  // work is complete and identify which part(a/v) of the sink is shutting down.
  void OnMediaSinkAudioComplete();
  void OnMediaSinkVideoComplete();

  // Rejected by the MediaSink to signal errors for audio/video.
  void OnMediaSinkAudioError(nsresult aResult);
  void OnMediaSinkVideoError();

  void* const mDecoderID;
  const RefPtr<AbstractThread> mAbstractMainThread;
  const RefPtr<FrameStatistics> mFrameStats;
  const RefPtr<VideoFrameContainer> mVideoFrameContainer;

  // Task queue for running the state machine.
  RefPtr<TaskQueue> mTaskQueue;

  // State-watching manager.
  WatchManager<MediaDecoderStateMachine> mWatchManager;

  // True if we've dispatched a task to run the state machine but the task has
  // yet to run.
  bool mDispatchedStateMachine;

  // Used to dispatch another round schedule with specific target time.
  DelayedScheduler mDelayedScheduler;

  // Queue of audio frames. This queue is threadsafe, and is accessed from
  // the audio, decoder, state machine, and main threads.
  MediaQueue<AudioData> mAudioQueue;
  // Queue of video frames. This queue is threadsafe, and is accessed from
  // the decoder, state machine, and main threads.
  MediaQueue<VideoData> mVideoQueue;

  UniquePtr<StateObject> mStateObj;

  media::TimeUnit Duration() const {
    MOZ_ASSERT(OnTaskQueue());
    return mDuration.Ref().ref();
  }

  // FrameID which increments every time a frame is pushed to our queue.
  FrameID mCurrentFrameID;

  // Media Fragment end time.
  media::TimeUnit mFragmentEndTime = media::TimeUnit::Invalid();

  // The media sink resource.  Used on the state machine thread.
  RefPtr<MediaSink> mMediaSink;

  const RefPtr<ReaderProxy> mReader;

  // The end time of the last audio frame that's been pushed onto the media sink
  // in microseconds. This will approximately be the end time
  // of the audio stream, unless another frame is pushed to the hardware.
  media::TimeUnit AudioEndTime() const;

  // The end time of the last rendered video frame that's been sent to
  // compositor.
  media::TimeUnit VideoEndTime() const;

  // The end time of the last decoded audio frame. This signifies the end of
  // decoded audio data. Used to check if we are low in decoded data.
  media::TimeUnit mDecodedAudioEndTime;

  // The end time of the last decoded video frame. Used to check if we are low
  // on decoded video data.
  media::TimeUnit mDecodedVideoEndTime;

  // Playback rate. 1.0 : normal speed, 0.5 : two times slower.
  double mPlaybackRate;

  // If we've got more than this number of decoded video frames waiting in
  // the video queue, we will not decode any more video frames until some have
  // been consumed by the play state machine thread.
  // Must hold monitor.
  uint32_t GetAmpleVideoFrames() const;

  // Our "ample" audio threshold. Once we've this much audio decoded, we
  // pause decoding.
  media::TimeUnit mAmpleAudioThreshold;

  // Only one of a given pair of ({Audio,Video}DataPromise, WaitForDataPromise)
  // should exist at any given moment.
  using AudioDataPromise = MediaFormatReader::AudioDataPromise;
  using VideoDataPromise = MediaFormatReader::VideoDataPromise;
  using WaitForDataPromise = MediaFormatReader::WaitForDataPromise;
  MozPromiseRequestHolder<AudioDataPromise> mAudioDataRequest;
  MozPromiseRequestHolder<VideoDataPromise> mVideoDataRequest;
  MozPromiseRequestHolder<WaitForDataPromise> mAudioWaitRequest;
  MozPromiseRequestHolder<WaitForDataPromise> mVideoWaitRequest;

  const char* AudioRequestStatus() const;
  const char* VideoRequestStatus() const;

  void OnSuspendTimerResolved();
  void CancelSuspendTimer();

  bool IsInSeamlessLooping() const;

  bool mCanPlayThrough = false;

  bool mIsLiveStream = false;

  // True if we shouldn't play our audio (but still write it to any capturing
  // streams). When this is true, the audio thread will never start again after
  // it has stopped.
  bool mAudioCaptured;

  // True if all audio frames are already rendered.
  bool mAudioCompleted = false;

  // True if all video frames are already rendered.
  bool mVideoCompleted = false;

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

  // Stores presentation info required for playback.
  Maybe<MediaInfo> mInfo;

  mozilla::MediaMetadataManager mMetadataManager;

  // True if we've decoded first frames (thus having the start time) and
  // notified the FirstFrameLoaded event. Note we can't initiate seek until the
  // start time is known which happens when the first frames are decoded or we
  // are playing an MSE stream (the start time is always assumed 0).
  bool mSentFirstFrameLoadedEvent;

  // True if video decoding is suspended.
  bool mVideoDecodeSuspended;

  // True if the media is seekable (i.e. supports random access).
  bool mMediaSeekable = true;

  // True if the media is seekable only in buffered ranges.
  bool mMediaSeekableOnlyInBufferedRanges = false;

  // Track enabling video decode suspension via timer
  DelayedScheduler mVideoDecodeSuspendTimer;

  // Data about MediaStreams that are being fed by the decoder.
  // Main thread only.
  RefPtr<OutputStreamManager> mOutputStreamManager;

  // Principal used by output streams. Main thread only.
  nsCOMPtr<nsIPrincipal> mOutputStreamPrincipal;

  // Track the current video decode mode.
  VideoDecodeMode mVideoDecodeMode;

  // Track the complete & error for audio/video separately
  MozPromiseRequestHolder<MediaSink::EndedPromise> mMediaSinkAudioEndedPromise;
  MozPromiseRequestHolder<MediaSink::EndedPromise> mMediaSinkVideoEndedPromise;

  MediaEventListener mAudioQueueListener;
  MediaEventListener mVideoQueueListener;
  MediaEventListener mAudibleListener;
  MediaEventListener mOnMediaNotSeekable;

  MediaEventProducerExc<UniquePtr<MediaInfo>, UniquePtr<MetadataTags>,
                        MediaDecoderEventVisibility>
      mMetadataLoadedEvent;
  MediaEventProducerExc<nsAutoPtr<MediaInfo>, MediaDecoderEventVisibility>
      mFirstFrameLoadedEvent;

  MediaEventProducer<MediaPlaybackEvent> mOnPlaybackEvent;
  MediaEventProducer<MediaResult> mOnPlaybackErrorEvent;

  MediaEventProducer<DecoderDoctorEvent> mOnDecoderDoctorEvent;

  MediaEventProducer<NextFrameStatus> mOnNextFrameStatus;

  const bool mIsMSE;

  bool mSeamlessLoopingAllowed;

  // If media was in looping and had reached to the end before, then we need
  // to adjust sample time from clock time to media time.
  void AdjustByLooping(media::TimeUnit& aTime) const;
  Maybe<media::TimeUnit> mAudioDecodedDuration;

  // Current playback position in the stream in bytes.
  int64_t mPlaybackOffset = 0;

 private:
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

  // Used to count the number of pending requests to set a new sink.
  Atomic<int> mSetSinkRequestsCount;

 public:
  AbstractCanonical<media::TimeIntervals>* CanonicalBuffered() const;

  AbstractCanonical<media::NullableTimeUnit>* CanonicalDuration() {
    return &mDuration;
  }
  AbstractCanonical<media::TimeUnit>* CanonicalCurrentPosition() {
    return &mCurrentPosition;
  }
  AbstractCanonical<bool>* CanonicalIsAudioDataAudible() {
    return &mIsAudioDataAudible;
  }
};

}  // namespace mozilla

#endif
