/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#if !defined(MediaDecoderStateMachine_h__)
#  define MediaDecoderStateMachine_h__

#  include "AudioDeviceInfo.h"
#  include "ImageContainer.h"
#  include "MediaDecoder.h"
#  include "MediaDecoderOwner.h"
#  include "MediaDecoderStateMachineBase.h"
#  include "MediaFormatReader.h"
#  include "MediaQueue.h"
#  include "MediaSink.h"
#  include "MediaStatistics.h"
#  include "MediaTimer.h"
#  include "SeekJob.h"
#  include "mozilla/Attributes.h"
#  include "mozilla/ReentrantMonitor.h"
#  include "mozilla/StateMirroring.h"
#  include "nsThreadUtils.h"

namespace mozilla {

class AbstractThread;
class AudioSegment;
class DecodedStream;
class DOMMediaStream;
class ReaderProxy;
class TaskQueue;

extern LazyLogModule gMediaDecoderLog;

DDLoggedTypeDeclName(MediaDecoderStateMachine);

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
class MediaDecoderStateMachine
    : public MediaDecoderStateMachineBase,
      public DecoderDoctorLifeLogger<MediaDecoderStateMachine> {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MediaDecoderStateMachine, override)

  using TrackSet = MediaFormatReader::TrackSet;

 public:
  using FrameID = mozilla::layers::ImageContainer::FrameID;
  MediaDecoderStateMachine(MediaDecoder* aDecoder, MediaFormatReader* aReader);

  nsresult Init(MediaDecoder* aDecoder) override;

  // Enumeration for the valid decoding states
  enum State {
    DECODER_STATE_DECODING_METADATA,
    DECODER_STATE_DORMANT,
    DECODER_STATE_DECODING_FIRSTFRAME,
    DECODER_STATE_DECODING,
    DECODER_STATE_LOOPING_DECODING,
    DECODER_STATE_SEEKING_ACCURATE,
    DECODER_STATE_SEEKING_FROMDORMANT,
    DECODER_STATE_SEEKING_NEXTFRAMESEEKING,
    DECODER_STATE_SEEKING_VIDEOONLY,
    DECODER_STATE_BUFFERING,
    DECODER_STATE_COMPLETED,
    DECODER_STATE_SHUTDOWN
  };

  RefPtr<GenericPromise> RequestDebugInfo(
      dom::MediaDecoderStateMachineDebugInfo& aInfo) override;

  size_t SizeOfVideoQueue() const override;

  size_t SizeOfAudioQueue() const override;

  // Sets the video decode mode. Used by the suspend-video-decoder feature.
  void SetVideoDecodeMode(VideoDecodeMode aMode) override;

  RefPtr<GenericPromise> InvokeSetSink(
      const RefPtr<AudioDeviceInfo>& aSink) override;

  void InvokeSuspendMediaSink() override;
  void InvokeResumeMediaSink() override;

  bool IsCDMProxySupported(CDMProxy* aProxy) override;

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

  // Initialization that needs to happen on the task queue. This is the first
  // task that gets run on the task queue, and is dispatched from the MDSM
  // constructor immediately after the task queue is created.
  void InitializationTask(MediaDecoder* aDecoder) override;

  RefPtr<MediaDecoder::SeekPromise> Seek(const SeekTarget& aTarget) override;

  RefPtr<ShutdownPromise> Shutdown() override;

  RefPtr<ShutdownPromise> FinishShutdown();

  // Update the playback position. This can result in a timeupdate event
  // and an invalidate of the frame being dispatched asynchronously if
  // there is no such event currently queued.
  // Only called on the decoder thread. Must be called with
  // the decode monitor held.
  void UpdatePlaybackPosition(const media::TimeUnit& aTime);

  // Schedules the shared state machine thread to run the state machine.
  void ScheduleStateMachine();

  // Invokes ScheduleStateMachine to run in |aTime|,
  // unless it's already scheduled to run earlier, in which case the
  // request is discarded.
  void ScheduleStateMachineIn(const media::TimeUnit& aTime);

  bool HaveEnoughDecodedAudio() const;
  bool HaveEnoughDecodedVideo() const;

  // The check is used to store more video frames than usual when playing 4K+
  // video.
  bool IsVideoDataEnoughComparedWithAudio() const;

  // Returns true if we're currently playing. The decoder monitor must
  // be held.
  bool IsPlaying() const;

  // Sets mMediaSeekable to false.
  void SetMediaNotSeekable();

  // Resets all states related to decoding and aborts all pending requests
  // to the decoders.
  void ResetDecode(const TrackSet& aTracks = TrackSet(TrackInfo::kAudioTrack,
                                                      TrackInfo::kVideoTrack));

  void SetVideoDecodeModeInternal(VideoDecodeMode aMode);

  RefPtr<GenericPromise> SetSink(RefPtr<AudioDeviceInfo> aDevice);

  // Shutdown MediaSink on suspend to clean up resources.
  void SuspendMediaSink();
  // Create a new MediaSink, it must have been stopped first.
  void ResumeMediaSink();

 protected:
  virtual ~MediaDecoderStateMachine();

  void BufferedRangeUpdated() override;
  void VolumeChanged() override;
  void PreservesPitchChanged() override;
  void PlayStateChanged() override;
  void LoopingChanged() override;
  void UpdateSecondaryVideoContainer() override;

  void ReaderSuspendedChanged();

  // Inserts a sample into the Audio/Video queue.
  // aSample must not be null.
  void PushAudio(AudioData* aSample);
  void PushVideo(VideoData* aSample);

  void OnAudioPopped(const RefPtr<AudioData>& aSample);
  void OnVideoPopped(const RefPtr<VideoData>& aSample);

  void AudioAudibleChanged(bool aAudible);

  void SetPlaybackRate(double aPlaybackRate) override;
  void SetIsLiveStream(bool aIsLiveStream) override {
    mIsLiveStream = aIsLiveStream;
  }
  void SetCanPlayThrough(bool aCanPlayThrough) override {
    mCanPlayThrough = aCanPlayThrough;
  }
  void SetFragmentEndTime(const media::TimeUnit& aEndTime) override {
    // A negative number means we don't have a fragment end time at all.
    mFragmentEndTime = aEndTime >= media::TimeUnit::Zero()
                           ? aEndTime
                           : media::TimeUnit::Invalid();
  }

  void StreamNameChanged();
  void UpdateOutputCaptured();
  void OutputPrincipalChanged();

  MediaQueue<AudioData>& AudioQueue() { return mAudioQueue; }
  MediaQueue<VideoData>& VideoQueue() { return mVideoQueue; }

  const MediaQueue<AudioData>& AudioQueue() const { return mAudioQueue; }
  const MediaQueue<VideoData>& VideoQueue() const { return mVideoQueue; }

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

  // Always create mediasink which contains an AudioSink or DecodedStream
  // inside.
  already_AddRefed<MediaSink> CreateMediaSink();

  // Stops the media sink and shut it down.
  // The decoder monitor must be held with exactly one lock count.
  // Called on the state machine thread.
  void StopMediaSink();

  // Create and start the media sink.
  // The decoder monitor must be held with exactly one lock count.
  // Called on the state machine thread.
  // If start fails an NS_ERROR_FAILURE is returned.
  nsresult StartMediaSink();

  // Notification method invoked when mIsVisible changes.
  void VisibilityChanged();

  // Sets internal state which causes playback of media to pause.
  // The decoder monitor must be held.
  void StopPlayback();

  // If the conditions are right, sets internal state which causes playback
  // of media to begin or resume.
  // Must be called with the decode monitor held.
  void MaybeStartPlayback();

  void EnqueueFirstFrameLoadedEvent();

  // Start a task to decode audio.
  void RequestAudioData();

  // Start a task to decode video.
  // @param aRequestNextVideoKeyFrame
  // If aRequestNextKeyFrame is true, will request data for the next keyframe
  // after aCurrentTime.
  void RequestVideoData(const media::TimeUnit& aCurrentTime,
                        bool aRequestNextKeyFrame = false);

  void WaitForData(MediaData::Type aType);

  // Returns the "current playback position" in HTML5, which is in the range
  // [0,duration].  The first frame of the media resource corresponds to 0
  // regardless of any codec-specific internal time code.
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
  media::TimeUnit GetDecodedAudioDuration() const;

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

  // If we've got more than this number of decoded video frames waiting in
  // the video queue, we will not decode any more video frames until some have
  // been consumed by the play state machine thread.
  // Must hold monitor.
  uint32_t GetAmpleVideoFrames() const;

  // Our "ample" audio threshold. Once we've this much audio decoded, we
  // pause decoding.
  media::TimeUnit mAmpleAudioThreshold;

  const char* AudioRequestStatus() const;
  const char* VideoRequestStatus() const;

  void OnSuspendTimerResolved();
  void CancelSuspendTimer();

  bool IsInSeamlessLooping() const;

  bool mCanPlayThrough = false;

  bool mIsLiveStream = false;

  // True if all audio frames are already rendered.
  bool mAudioCompleted = false;

  // True if all video frames are already rendered.
  bool mVideoCompleted = false;

  // True if video decoding is suspended.
  bool mVideoDecodeSuspended;

  // Track enabling video decode suspension via timer
  DelayedScheduler mVideoDecodeSuspendTimer;

  // Track the current video decode mode.
  VideoDecodeMode mVideoDecodeMode;

  // Track the complete & error for audio/video separately
  MozPromiseRequestHolder<MediaSink::EndedPromise> mMediaSinkAudioEndedPromise;
  MozPromiseRequestHolder<MediaSink::EndedPromise> mMediaSinkVideoEndedPromise;

  MediaEventListener mAudioQueueListener;
  MediaEventListener mVideoQueueListener;
  MediaEventListener mAudibleListener;
  MediaEventListener mOnMediaNotSeekable;

  const bool mIsMSE;

  const bool mShouldResistFingerprinting;

  bool mSeamlessLoopingAllowed;

  // If media was in looping and had reached to the end before, then we need
  // to adjust sample time from clock time to media time.
  void AdjustByLooping(media::TimeUnit& aTime) const;

  // These are used for seamless looping. When looping has been enable at least
  // once, `mOriginalDecodedDuration` would be set to the larger duration
  // between two tracks.
  media::TimeUnit mOriginalDecodedDuration;
  Maybe<media::TimeUnit> mAudioTrackDecodedDuration;
  Maybe<media::TimeUnit> mVideoTrackDecodedDuration;

  bool HasLastDecodedData(MediaData::Type aType);

  // Current playback position in the stream in bytes.
  int64_t mPlaybackOffset = 0;

  // For seamless looping video, we don't want to trigger skip-to-next-keyframe
  // after reaching video EOS. Because we've reset the demuxer to 0, and are
  // going to request data from start. If playback hasn't looped back, the media
  // time would still be too large, which makes the reader think the playback is
  // way behind and performs unnecessary skipping. Eg. Media is 10s long,
  // reaching EOS at 8s, requesting data at 9s. Assume media's keyframe interval
  // is 3s, which means keyframes will appear on 0s, 3s, 6s and 9s. If we use
  // current time as a threshold, the reader sees the next key frame is 3s but
  // the threashold is 9s, which usually happens when the decoding is too slow.
  // But that is not the case for us, we should by pass thskip-to-next-keyframe
  // logic until the media loops back.
  bool mBypassingSkipToNextKeyFrameCheck = false;

 private:
  // Audio stream name
  Mirror<nsAutoString> mStreamName;

  // The device used with SetSink, or nullptr if no explicit device has been
  // set.
  Mirror<RefPtr<AudioDeviceInfo>> mSinkDevice;

  // Whether all output should be captured into mOutputTracks, halted, or not
  // captured.
  Mirror<MediaDecoder::OutputCaptureState> mOutputCaptureState;

  // A dummy track used to access the right MediaTrackGraph instance. Needed
  // since there's no guarantee that output tracks are present.
  Mirror<nsMainThreadPtrHandle<SharedDummyTrack>> mOutputDummyTrack;

  // Tracks to capture data into.
  Mirror<CopyableTArray<RefPtr<ProcessedMediaTrack>>> mOutputTracks;

  // PrincipalHandle to feed with data captured into mOutputTracks.
  Mirror<PrincipalHandle> mOutputPrincipal;

  Canonical<PrincipalHandle> mCanonicalOutputPrincipal;

  // Track when MediaSink is supsended. When that happens some actions are
  // restricted like starting the sink or changing sink id. The flag is valid
  // after Initialization. TaskQueue thread only.
  bool mIsMediaSinkSuspended = false;

 public:
  AbstractCanonical<PrincipalHandle>* CanonicalOutputPrincipal() {
    return &mCanonicalOutputPrincipal;
  }
};

}  // namespace mozilla

#endif
