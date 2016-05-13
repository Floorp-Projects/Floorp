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
#define MediaDecoderStateMachine_h__

#include "mozilla/Attributes.h"
#include "mozilla/ReentrantMonitor.h"
#include "mozilla/StateMirroring.h"

#include "nsThreadUtils.h"
#include "MediaCallbackID.h"
#include "MediaDecoder.h"
#include "MediaDecoderReader.h"
#include "MediaDecoderOwner.h"
#include "MediaEventSource.h"
#include "MediaMetadataManager.h"
#include "MediaStatistics.h"
#include "MediaTimer.h"
#include "ImageContainer.h"
#include "SeekTask.h"

namespace mozilla {

namespace media {
class MediaSink;
}

class AudioSegment;
class DecodedStream;
class MediaDecoderReaderWrapper;
class OutputStreamManager;
class TaskQueue;

extern LazyLogModule gMediaDecoderLog;
extern LazyLogModule gMediaSampleLog;

enum class MediaEventType : int8_t {
  PlaybackStarted,
  PlaybackStopped,
  PlaybackEnded,
  DecodeError,
  Invalidate
};

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
{
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MediaDecoderStateMachine)
public:
  typedef MediaDecoderOwner::NextFrameStatus NextFrameStatus;
  typedef mozilla::layers::ImageContainer::FrameID FrameID;
  MediaDecoderStateMachine(MediaDecoder* aDecoder,
                           MediaDecoderReader* aReader,
                           bool aRealTime = false);

  nsresult Init(MediaDecoder* aDecoder);

  void SetMediaDecoderReaderWrapperCallback();
  void CancelMediaDecoderReaderWrapperCallback();

  // Enumeration for the valid decoding states
  enum State {
    DECODER_STATE_DECODING_METADATA,
    DECODER_STATE_WAIT_FOR_CDM,
    DECODER_STATE_DORMANT,
    DECODER_STATE_DECODING,
    DECODER_STATE_SEEKING,
    DECODER_STATE_BUFFERING,
    DECODER_STATE_COMPLETED,
    DECODER_STATE_SHUTDOWN,
    DECODER_STATE_ERROR
  };

  void DumpDebugInfo();

  void AddOutputStream(ProcessedMediaStream* aStream, bool aFinishWhenEnded);
  // Remove an output stream added with AddOutputStream.
  void RemoveOutputStream(MediaStream* aStream);

  // Seeks to the decoder to aTarget asynchronously.
  RefPtr<MediaDecoder::SeekPromise> InvokeSeek(SeekTarget aTarget);

  // Set/Unset dormant state.
  void DispatchSetDormant(bool aDormant);

  RefPtr<ShutdownPromise> BeginShutdown();

  // Notifies the state machine that should minimize the number of samples
  // decoded we preroll, until playback starts. The first time playback starts
  // the state machine is free to return to prerolling normally. Note
  // "prerolling" in this context refers to when we decode and buffer decoded
  // samples in advance of when they're needed for playback.
  void DispatchMinimizePrerollUntilPlaybackStarts()
  {
    RefPtr<MediaDecoderStateMachine> self = this;
    nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction([self] () -> void
    {
      MOZ_ASSERT(self->OnTaskQueue());
      self->mMinimizePreroll = true;

      // Make sure that this arrives before playback starts, otherwise this won't
      // have the intended effect.
      MOZ_DIAGNOSTIC_ASSERT(self->mPlayState == MediaDecoder::PLAY_STATE_LOADING);
    });
    OwnerThread()->Dispatch(r.forget());
  }

  // Set the media fragment end time. aEndTime is in microseconds.
  void DispatchSetFragmentEndTime(int64_t aEndTime)
  {
    RefPtr<MediaDecoderStateMachine> self = this;
    nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction([self, aEndTime] () {
      self->mFragmentEndTime = aEndTime;
    });
    OwnerThread()->Dispatch(r.forget());
  }

  void DispatchAudioOffloading(bool aAudioOffloading)
  {
    RefPtr<MediaDecoderStateMachine> self = this;
    nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction([=] () {
      if (self->mAudioOffloading != aAudioOffloading) {
        self->mAudioOffloading = aAudioOffloading;
        self->ScheduleStateMachine();
      }
    });
    OwnerThread()->Dispatch(r.forget());
  }

  // Drop reference to mResource. Only called during shutdown dance.
  void BreakCycles() {
    MOZ_ASSERT(NS_IsMainThread());
    mResource = nullptr;
  }

  TimedMetadataEventSource& TimedMetadataEvent() {
    return mMetadataManager.TimedMetadataEvent();
  }

  MediaEventSource<void>& OnMediaNotSeekable() const;

  MediaEventSourceExc<nsAutoPtr<MediaInfo>,
                      nsAutoPtr<MetadataTags>,
                      MediaDecoderEventVisibility>&
  MetadataLoadedEvent() { return mMetadataLoadedEvent; }

  MediaEventSourceExc<nsAutoPtr<MediaInfo>,
                      MediaDecoderEventVisibility>&
  FirstFrameLoadedEvent() { return mFirstFrameLoadedEvent; }

  MediaEventSource<MediaEventType>&
  OnPlaybackEvent() { return mOnPlaybackEvent; }

  MediaEventSource<MediaDecoderEventVisibility>&
  OnSeekingStart() { return mOnSeekingStart; }

  // Immutable after construction - may be called on any thread.
  bool IsRealTime() const { return mRealTime; }

  size_t SizeOfVideoQueue() const;

  size_t SizeOfAudioQueue() const;

private:
  // Functions used by assertions to ensure we're calling things
  // on the appropriate threads.
  bool OnTaskQueue() const;

  // Initialization that needs to happen on the task queue. This is the first
  // task that gets run on the task queue, and is dispatched from the MDSM
  // constructor immediately after the task queue is created.
  void InitializationTask(MediaDecoder* aDecoder);

  void SetDormant(bool aDormant);

  void SetAudioCaptured(bool aCaptured);

  void ReadMetadata();

  RefPtr<MediaDecoder::SeekPromise> Seek(SeekTarget aTarget);

  RefPtr<ShutdownPromise> Shutdown();

  RefPtr<ShutdownPromise> FinishShutdown();

  // Update the playback position. This can result in a timeupdate event
  // and an invalidate of the frame being dispatched asynchronously if
  // there is no such event currently queued.
  // Only called on the decoder thread. Must be called with
  // the decode monitor held.
  void UpdatePlaybackPosition(int64_t aTime);

  // Causes the state machine to switch to buffering state, and to
  // immediately stop playback and buffer downloaded data. Called on
  // the state machine thread.
  void StartBuffering();

  bool CanPlayThrough();

  MediaStatistics GetStatistics();

  // This is called on the state machine thread and audio thread.
  // The decoder monitor must be obtained before calling this.
  bool HasAudio() const {
    MOZ_ASSERT(OnTaskQueue());
    return mInfo.HasAudio();
  }

  // This is called on the state machine thread and audio thread.
  // The decoder monitor must be obtained before calling this.
  bool HasVideo() const {
    MOZ_ASSERT(OnTaskQueue());
    return mInfo.HasVideo();
  }

  // Should be called by main thread.
  bool HaveNextFrameData();

  // Must be called with the decode monitor held.
  bool IsBuffering() const {
    MOZ_ASSERT(OnTaskQueue());
    return mState == DECODER_STATE_BUFFERING;
  }

  // Must be called with the decode monitor held.
  bool IsSeeking() const {
    MOZ_ASSERT(OnTaskQueue());
    return mState == DECODER_STATE_SEEKING;
  }

  // Returns the state machine task queue.
  TaskQueue* OwnerThread() const { return mTaskQueue; }

  // Schedules the shared state machine thread to run the state machine.
  void ScheduleStateMachine();

  // Invokes ScheduleStateMachine to run in |aMicroseconds| microseconds,
  // unless it's already scheduled to run earlier, in which case the
  // request is discarded.
  void ScheduleStateMachineIn(int64_t aMicroseconds);

  void OnDelayedSchedule()
  {
    MOZ_ASSERT(OnTaskQueue());
    mDelayedScheduler.CompleteRequest();
    ScheduleStateMachine();
  }

  void NotReached() { MOZ_DIAGNOSTIC_ASSERT(false); }

  // Discard audio/video data that are already played by MSG.
  void DiscardStreamData();
  bool HaveEnoughDecodedAudio();
  bool HaveEnoughDecodedVideo();

  // Returns true if the state machine has shutdown or is in the process of
  // shutting down. The decoder monitor must be held while calling this.
  bool IsShutdown();

  // Returns true if we're currently playing. The decoder monitor must
  // be held.
  bool IsPlaying() const;

  // TODO: Those callback function may receive demuxed-only data.
  // Need to figure out a suitable API name for this case.
  void OnAudioDecoded(MediaData* aAudioSample);
  void OnVideoDecoded(MediaData* aVideoSample, TimeStamp aDecodeStartTime);
  void OnNotDecoded(MediaData::Type aType, MediaDecoderReader::NotDecodedReason aReason);
  void OnAudioNotDecoded(MediaDecoderReader::NotDecodedReason aReason)
  {
    MOZ_ASSERT(OnTaskQueue());
    OnNotDecoded(MediaData::AUDIO_DATA, aReason);
  }
  void OnVideoNotDecoded(MediaDecoderReader::NotDecodedReason aReason)
  {
    MOZ_ASSERT(OnTaskQueue());
    OnNotDecoded(MediaData::VIDEO_DATA, aReason);
  }

  // Resets all state related to decoding and playback, emptying all buffers
  // and aborting all pending operations on the decode task queue.
  void Reset(MediaDecoderReader::TargetQueues aQueues = MediaDecoderReader::AUDIO_VIDEO);

protected:
  virtual ~MediaDecoderStateMachine();

  void SetState(State aState);

  void BufferedRangeUpdated();

  // Inserts MediaData* samples into their respective MediaQueues.
  // aSample must not be null.

  void Push(MediaData* aSample, MediaData::Type aSampleType);

  void OnAudioPopped(const RefPtr<MediaData>& aSample);
  void OnVideoPopped(const RefPtr<MediaData>& aSample);

  void AudioAudibleChanged(bool aAudible);

  void VolumeChanged();
  void LogicalPlaybackRateChanged();
  void PreservesPitchChanged();

  MediaQueue<MediaData>& AudioQueue() { return mAudioQueue; }
  MediaQueue<MediaData>& VideoQueue() { return mVideoQueue; }

  // True if our buffers of decoded audio are not full, and we should
  // decode more.
  bool NeedToDecodeAudio();

  // True if our buffers of decoded video are not full, and we should
  // decode more.
  bool NeedToDecodeVideo();

  // Returns true if we've got less than aAudioUsecs microseconds of decoded
  // and playable data. The decoder monitor must be held.
  //
  // May not be invoked when mReader->UseBufferingHeuristics() is false.
  bool HasLowDecodedData(int64_t aAudioUsecs);

  bool OutOfDecodedAudio();

  bool OutOfDecodedVideo()
  {
    MOZ_ASSERT(OnTaskQueue());
    return IsVideoDecoding() && !VideoQueue().IsFinished() && VideoQueue().GetSize() <= 1;
  }


  // Returns true if we're running low on data which is not yet decoded.
  // The decoder monitor must be held.
  bool HasLowUndecodedData();

  // Returns true if we have less than aUsecs of undecoded data available.
  bool HasLowUndecodedData(int64_t aUsecs);

  // Returns true when there's decoded audio waiting to play.
  // The decoder monitor must be held.
  bool HasFutureAudio();

  // Returns true if we recently exited "quick buffering" mode.
  bool JustExitedQuickBuffering();

  // Recomputes mNextFrameStatus, possibly dispatching notifications to interested
  // parties.
  void UpdateNextFrameStatus();

  // Return the current time, either the audio clock if available (if the media
  // has audio, and the playback is possible), or a clock for the video.
  // Called on the state machine thread.
  // If aTimeStamp is non-null, set *aTimeStamp to the TimeStamp corresponding
  // to the returned stream time.
  int64_t GetClock(TimeStamp* aTimeStamp = nullptr) const;

  void SetStartTime(int64_t aStartTimeUsecs);

  // Update only the state machine's current playback position (and duration,
  // if unknown).  Does not update the playback position on the decoder or
  // media element -- use UpdatePlaybackPosition for that.  Called on the state
  // machine thread, caller must hold the decoder lock.
  void UpdatePlaybackPositionInternal(int64_t aTime);

  // Update playback position and trigger next update by default time period.
  // Called on the state machine thread.
  void UpdatePlaybackPositionPeriodically();

  media::MediaSink* CreateAudioSink();

  // Always create mediasink which contains an AudioSink or StreamSink inside.
  already_AddRefed<media::MediaSink> CreateMediaSink(bool aAudioCaptured);

  // Stops the media sink and shut it down.
  // The decoder monitor must be held with exactly one lock count.
  // Called on the state machine thread.
  void StopMediaSink();

  // Create and start the media sink.
  // The decoder monitor must be held with exactly one lock count.
  // Called on the state machine thread.
  void StartMediaSink();

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

  // Check to see if we don't have enough data to play up to the next frame.
  // If we don't, switch to buffering mode.
  void MaybeStartBuffering();

  // Moves the decoder into decoding state. Called on the state machine
  // thread. The decoder monitor must be held.
  void StartDecoding();

  // Moves the decoder into the shutdown state, and dispatches an error
  // event to the media element. This begins shutting down the decoder.
  // The decoder monitor must be held. This is only called on the
  // decode thread.
  void DecodeError();

  // Dispatches a task to the decode task queue to begin decoding metadata.
  // This is threadsafe and can be called on any thread.
  // The decoder monitor must be held.
  nsresult EnqueueDecodeMetadataTask();

  // Dispatches a LoadedMetadataEvent.
  // This is threadsafe and can be called on any thread.
  // The decoder monitor must be held.
  void EnqueueLoadedMetadataEvent();

  void EnqueueFirstFrameLoadedEvent();

  // Clears any previous seeking state and initiates a new seek on the decoder.
  // The decoder monitor must be held.
  void InitiateSeek(SeekJob aSeekJob);

  // Clears any previous seeking state and initiates a video-only seek on the
  // decoder to catch up the video to the current audio position, when recovering
  // from video decoding being suspended in background.
  void InitiateVideoDecodeRecoverySeek();

  nsresult DispatchAudioDecodeTaskIfNeeded();

  // Ensures a task to decode audio has been dispatched to the decode task queue.
  // If a task to decode has already been dispatched, this does nothing,
  // otherwise this dispatches a task to do the decode.
  // This is called on the state machine or decode threads.
  // The decoder monitor must be held.
  nsresult EnsureAudioDecodeTaskQueued();
  // Start a task to decode audio.
  // The decoder monitor must be held.
  void RequestAudioData();

  nsresult DispatchVideoDecodeTaskIfNeeded();

  // Ensures a task to decode video has been dispatched to the decode task queue.
  // If a task to decode has already been dispatched, this does nothing,
  // otherwise this dispatches a task to do the decode.
  // The decoder monitor must be held.
  nsresult EnsureVideoDecodeTaskQueued();
  // Start a task to decode video.
  // The decoder monitor must be held.
  void RequestVideoData();

  // Re-evaluates the state and determines whether we need to dispatch
  // events to run the decode, or if not whether we should set the reader
  // to idle mode. This is threadsafe, and can be called from any thread.
  // The decoder monitor must be held.
  void DispatchDecodeTasksIfNeeded();

  // Returns the "media time". This is the absolute time which the media
  // playback has reached. i.e. this returns values in the range
  // [mStartTime, mEndTime], and mStartTime will not be 0 if the media does
  // not start at 0. Note this is different than the "current playback position",
  // which is in the range [0,duration].
  int64_t GetMediaTime() const {
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
  int64_t GetDecodedAudioDuration();

  // Promise callbacks for metadata reading.
  void OnMetadataRead(MetadataHolder* aMetadata);
  void OnMetadataNotRead(ReadMetadataFailureReason aReason);

  // Checks whether we're finished decoding first audio and/or video packets.
  // If so will trigger firing loadeddata event.
  // If there are any queued seek, will change state to DECODER_STATE_SEEKING
  // and return true.
  bool MaybeFinishDecodeFirstFrame();
  // Return true if we are currently decoding the first frames.
  bool IsDecodingFirstFrame();
  void FinishDecodeFirstFrame();

  // Completes the seek operation, moves onto the next appropriate state.
  void SeekCompleted();

  // Queries our state to see whether the decode has finished for all streams.
  // If so, we move into DECODER_STATE_COMPLETED and schedule the state machine
  // to run.
  // The decoder monitor must be held.
  void CheckIfDecodeComplete();

  // Performs one "cycle" of the state machine. Polls the state, and may send
  // a video frame to be displayed, and generally manages the decode. Called
  // periodically via timer to ensure the video stays in sync.
  nsresult RunStateMachine();

  bool IsStateMachineScheduled() const;

  // Returns true if we're not playing and the decode thread has filled its
  // decode buffers and is waiting. We can shut the decode thread down in this
  // case as it may not be needed again.
  bool IsPausedAndDecoderWaiting();

  // Returns true if the video decoding is suspended because the element is not
  // visible
  bool IsVideoDecodeSuspended() const;

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
  void OnMediaSinkAudioError();
  void OnMediaSinkVideoError();

  // Return true if the video decoder's decode speed can not catch up the
  // play time.
  bool NeedToSkipToNextKeyframe();

  void* const mDecoderID;
  const RefPtr<FrameStatistics> mFrameStats;
  const RefPtr<VideoFrameContainer> mVideoFrameContainer;
  const dom::AudioChannel mAudioChannel;

  // Task queue for running the state machine.
  RefPtr<TaskQueue> mTaskQueue;

  // State-watching manager.
  WatchManager<MediaDecoderStateMachine> mWatchManager;

  // True is we are decoding a realtime stream, like a camera stream.
  const bool mRealTime;

  // True if we've dispatched a task to run the state machine but the task has
  // yet to run.
  bool mDispatchedStateMachine;

  // Used to dispatch another round schedule with specific target time.
  DelayedScheduler mDelayedScheduler;

  // Queue of audio frames. This queue is threadsafe, and is accessed from
  // the audio, decoder, state machine, and main threads.
  MediaQueue<MediaData> mAudioQueue;
  // Queue of video frames. This queue is threadsafe, and is accessed from
  // the decoder, state machine, and main threads.
  MediaQueue<MediaData> mVideoQueue;

  // The decoder monitor must be obtained before modifying this state.
  // Accessed on state machine, audio, main, and AV thread.
  Watchable<State> mState;

  // Time that buffering started. Used for buffering timeout and only
  // accessed on the state machine thread. This is null while we're not
  // buffering.
  TimeStamp mBufferingStart;

  media::TimeUnit Duration() const { MOZ_ASSERT(OnTaskQueue()); return mDuration.Ref().ref(); }

  // Recomputes the canonical duration from various sources.
  void RecomputeDuration();


  // FrameID which increments every time a frame is pushed to our queue.
  FrameID mCurrentFrameID;

  // The highest timestamp that our position has reached. Monotonically
  // increasing.
  Watchable<media::TimeUnit> mObservedDuration;

  // Returns true if we're logically playing, that is, if the Play() has
  // been called and Pause() has not or we have not yet reached the end
  // of media. This is irrespective of the seeking state; if the owner
  // calls Play() and then Seek(), we still count as logically playing.
  // The decoder monitor must be held.
  bool IsLogicallyPlaying()
  {
    MOZ_ASSERT(OnTaskQueue());
    return mPlayState == MediaDecoder::PLAY_STATE_PLAYING ||
           mNextPlayState == MediaDecoder::PLAY_STATE_PLAYING;
  }

  // Queued seek - moves to mCurrentSeek when DecodeFirstFrame completes.
  SeekJob mQueuedSeek;

  // mSeekTask is responsible for executing the current seek request.
  RefPtr<SeekTask> mSeekTask;
  MozPromiseRequestHolder<SeekTask::SeekTaskPromise> mSeekTaskRequest;

  void OnSeekTaskResolved(SeekTaskResolveValue aValue);
  void OnSeekTaskRejected(SeekTaskRejectValue aValue);

  // This method discards the seek task and then get the ownership of
  // MedaiDecoderReaderWarpper back via registering MDSM's callback into it.
  void DiscardSeekTaskIfExist();

  // Media Fragment end time in microseconds. Access controlled by decoder monitor.
  int64_t mFragmentEndTime;

  // The media sink resource.  Used on the state machine thread.
  RefPtr<media::MediaSink> mMediaSink;

  const RefPtr<MediaDecoderReaderWrapper> mReader;

  // The end time of the last audio frame that's been pushed onto the media sink
  // in microseconds. This will approximately be the end time
  // of the audio stream, unless another frame is pushed to the hardware.
  int64_t AudioEndTime() const;

  // The end time of the last rendered video frame that's been sent to
  // compositor.
  int64_t VideoEndTime() const;

  // The end time of the last decoded audio frame. This signifies the end of
  // decoded audio data. Used to check if we are low in decoded data.
  int64_t mDecodedAudioEndTime;

  // The end time of the last decoded video frame. Used to check if we are low
  // on decoded video data.
  int64_t mDecodedVideoEndTime;

  // Playback rate. 1.0 : normal speed, 0.5 : two times slower.
  double mPlaybackRate;

  // Time at which we started decoding. Synchronised via decoder monitor.
  TimeStamp mDecodeStartTime;

  // The maximum number of second we spend buffering when we are short on
  // unbuffered data.
  uint32_t mBufferingWait;
  int64_t  mLowDataThresholdUsecs;

  // If we've got more than this number of decoded video frames waiting in
  // the video queue, we will not decode any more video frames until some have
  // been consumed by the play state machine thread.
  // Must hold monitor.
  uint32_t GetAmpleVideoFrames() const;

  // Low audio threshold. If we've decoded less than this much audio we
  // consider our audio decode "behind", and we may skip video decoding
  // in order to allow our audio decoding to catch up. We favour audio
  // decoding over video. We increase this threshold if we're slow to
  // decode video frames, in order to reduce the chance of audio underruns.
  // Note that we don't ever reset this threshold, it only ever grows as
  // we detect that the decode can't keep up with rendering.
  int64_t mLowAudioThresholdUsecs;

  // Our "ample" audio threshold. Once we've this much audio decoded, we
  // pause decoding. If we increase mLowAudioThresholdUsecs, we'll also
  // increase this too appropriately (we don't want mLowAudioThresholdUsecs
  // to be greater than ampleAudioThreshold, else we'd stop decoding!).
  // Note that we don't ever reset this threshold, it only ever grows as
  // we detect that the decode can't keep up with rendering.
  int64_t mAmpleAudioThresholdUsecs;

  // If we're quick buffering, we'll remain in buffering mode while we have less than
  // QUICK_BUFFERING_LOW_DATA_USECS of decoded data available.
  int64_t mQuickBufferingLowDataThresholdUsecs;

  // At the start of decoding we want to "preroll" the decode until we've
  // got a few frames decoded before we consider whether decode is falling
  // behind. Otherwise our "we're falling behind" logic will trigger
  // unnecessarily if we start playing as soon as the first sample is
  // decoded. These two fields store how many video frames and audio
  // samples we must consume before are considered to be finished prerolling.
  uint32_t AudioPrerollUsecs() const
  {
    MOZ_ASSERT(OnTaskQueue());
    return IsRealTime() ? 0 : mAmpleAudioThresholdUsecs / 2;
  }

  uint32_t VideoPrerollFrames() const
  {
    MOZ_ASSERT(OnTaskQueue());
    return IsRealTime() ? 0 : GetAmpleVideoFrames() / 2;
  }

  bool DonePrerollingAudio()
  {
    MOZ_ASSERT(OnTaskQueue());
    return !IsAudioDecoding() ||
        GetDecodedAudioDuration() >= AudioPrerollUsecs() * mPlaybackRate;
  }

  bool DonePrerollingVideo()
  {
    MOZ_ASSERT(OnTaskQueue());
    return !mIsVisible ||
        !IsVideoDecoding() ||
        static_cast<uint32_t>(VideoQueue().GetSize()) >=
            VideoPrerollFrames() * mPlaybackRate + 1;
  }

  void StopPrerollingAudio()
  {
    MOZ_ASSERT(OnTaskQueue());
    if (mIsAudioPrerolling) {
      mIsAudioPrerolling = false;
      ScheduleStateMachine();
    }
  }

  void StopPrerollingVideo()
  {
    MOZ_ASSERT(OnTaskQueue());
    if (mIsVideoPrerolling) {
      mIsVideoPrerolling = false;
      ScheduleStateMachine();
    }
  }

  // When we start decoding (either for the first time, or after a pause)
  // we may be low on decoded data. We don't want our "low data" logic to
  // kick in and decide that we're low on decoded data because the download
  // can't keep up with the decode, and cause us to pause playback. So we
  // have a "preroll" stage, where we ignore the results of our "low data"
  // logic during the first few frames of our decode. This occurs during
  // playback. The flags below are true when the corresponding stream is
  // being "prerolled".
  bool mIsAudioPrerolling;
  bool mIsVideoPrerolling;

  // Only one of a given pair of ({Audio,Video}DataPromise, WaitForDataPromise)
  // should exist at any given moment.

  CallbackID mAudioCallbackID;
  MozPromiseRequestHolder<MediaDecoderReader::WaitForDataPromise> mAudioWaitRequest;
  const char* AudioRequestStatus() const;

  CallbackID mVideoCallbackID;
  MozPromiseRequestHolder<MediaDecoderReader::WaitForDataPromise> mVideoWaitRequest;
  const char* VideoRequestStatus() const;

  MozPromiseRequestHolder<MediaDecoderReader::WaitForDataPromise>& WaitRequestRef(MediaData::Type aType)
  {
    MOZ_ASSERT(OnTaskQueue());
    return aType == MediaData::AUDIO_DATA ? mAudioWaitRequest : mVideoWaitRequest;
  }

  // True if we shouldn't play our audio (but still write it to any capturing
  // streams). When this is true, the audio thread will never start again after
  // it has stopped.
  bool mAudioCaptured;

  // True if the audio playback thread has finished. It is finished
  // when either all the audio frames have completed playing, or we've moved
  // into shutdown state, and the threads are to be
  // destroyed. Written by the audio playback thread and read and written by
  // the state machine thread. Synchronised via decoder monitor.
  // When data is being sent to a MediaStream, this is true when all data has
  // been written to the MediaStream.
  Watchable<bool> mAudioCompleted;

  // True if all video frames are already rendered.
  Watchable<bool> mVideoCompleted;

  // Set if MDSM receives dormant request during reading metadata.
  Maybe<bool> mPendingDormant;

  // Flag whether we notify metadata before decoding the first frame or after.
  //
  // Note that the odd semantics here are designed to replicate the current
  // behavior where we notify the decoder each time we come out of dormant, but
  // send suppressed event visibility for those cases. This code can probably be
  // simplified.
  bool mNotifyMetadataBeforeFirstFrame;

  // True if we've dispatched an event to the decode task queue to call
  // DecodeThreadRun(). We use this flag to prevent us from dispatching
  // unnecessary runnables, since the decode thread runs in a loop.
  bool mDispatchedEventToDecode;

  // If this is true while we're in buffering mode, we can exit early,
  // as it's likely we may be able to playback. This happens when we enter
  // buffering mode soon after the decode starts, because the decode-ahead
  // ran fast enough to exhaust all data while the download is starting up.
  // Synchronised via decoder monitor.
  bool mQuickBuffering;

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

  // True if the decode thread has gone filled its buffers and is now
  // waiting to be awakened before it continues decoding. Synchronized
  // by the decoder monitor.
  bool mDecodeThreadWaiting;

  // Track our request for metadata from the reader.
  MozPromiseRequestHolder<MediaDecoderReader::MetadataPromise> mMetadataRequest;

  // Stores presentation info required for playback. The decoder monitor
  // must be held when accessing this.
  MediaInfo mInfo;

  nsAutoPtr<MetadataTags> mMetadataTags;

  mozilla::MediaMetadataManager mMetadataManager;

  // Track our request to update the buffered ranges
  MozPromiseRequestHolder<MediaDecoderReader::BufferedUpdatePromise> mBufferedUpdateRequest;

  // True if we need to call FinishDecodeFirstFrame() upon frame decoding
  // succeeding.
  bool mDecodingFirstFrame;

  // True if we are back from DECODER_STATE_DORMANT state and
  // LoadedMetadataEvent was already sent.
  bool mSentLoadedMetadataEvent;
  // True if we are back from DECODER_STATE_DORMANT state and
  // FirstFrameLoadedEvent was already sent, then we can skip
  // SetStartTime because the mStartTime already set before. Also we don't need
  // to decode any audio/video since the MediaDecoder will trigger a seek
  // operation soon.
  bool mSentFirstFrameLoadedEvent;

  bool mSentPlaybackEndedEvent;

  // Data about MediaStreams that are being fed by the decoder.
  const RefPtr<OutputStreamManager> mOutputStreamManager;

  // Media data resource from the decoder.
  RefPtr<MediaResource> mResource;

  // Track the complete & error for audio/video separately
  MozPromiseRequestHolder<GenericPromise> mMediaSinkAudioPromise;
  MozPromiseRequestHolder<GenericPromise> mMediaSinkVideoPromise;

  MediaEventListener mAudioQueueListener;
  MediaEventListener mVideoQueueListener;
  MediaEventListener mAudibleListener;

  MediaEventProducerExc<nsAutoPtr<MediaInfo>,
                        nsAutoPtr<MetadataTags>,
                        MediaDecoderEventVisibility> mMetadataLoadedEvent;
  MediaEventProducerExc<nsAutoPtr<MediaInfo>,
                        MediaDecoderEventVisibility> mFirstFrameLoadedEvent;

  MediaEventProducer<MediaEventType> mOnPlaybackEvent;
  MediaEventProducer<MediaDecoderEventVisibility> mOnSeekingStart;

  // True if audio is offloading.
  // Playback will not start when audio is offloading.
  bool mAudioOffloading;

#ifdef MOZ_EME
  void OnCDMProxyReady(RefPtr<CDMProxy> aProxy);
  void OnCDMProxyNotReady();
  RefPtr<CDMProxy> mCDMProxy;
  MozPromiseRequestHolder<MediaDecoder::CDMProxyPromise> mCDMProxyPromise;
#endif

private:
  // The buffered range. Mirrored from the decoder thread.
  Mirror<media::TimeIntervals> mBuffered;

  // The duration according to the demuxer's current estimate, mirrored from the main thread.
  Mirror<media::NullableTimeUnit> mEstimatedDuration;

  // The duration explicitly set by JS, mirrored from the main thread.
  Mirror<Maybe<double>> mExplicitDuration;

  // The current play state and next play state, mirrored from the main thread.
  Mirror<MediaDecoder::PlayState> mPlayState;
  Mirror<MediaDecoder::PlayState> mNextPlayState;

  // Volume of playback. 0.0 = muted. 1.0 = full volume.
  Mirror<double> mVolume;

  // TODO: The separation between mPlaybackRate and mLogicalPlaybackRate is a
  // kludge to preserve existing fragile logic while converting this setup to
  // state-mirroring. Some hero should clean this up.
  Mirror<double> mLogicalPlaybackRate;

  // Pitch preservation for the playback rate.
  Mirror<bool> mPreservesPitch;

  // True if the media is same-origin with the element. Data can only be
  // passed to MediaStreams when this is true.
  Mirror<bool> mSameOriginMedia;

  // An identifier for the principal of the media. Used to track when
  // main-thread induced principal changes get reflected on MSG thread.
  Mirror<PrincipalHandle> mMediaPrincipalHandle;

  // Estimate of the current playback rate (bytes/second).
  Mirror<double> mPlaybackBytesPerSecond;

  // True if mPlaybackBytesPerSecond is a reliable estimate.
  Mirror<bool> mPlaybackRateReliable;

  // Current decoding position in the stream.
  Mirror<int64_t> mDecoderPosition;

  // True if the media is seekable (i.e. supports random access).
  Mirror<bool> mMediaSeekable;

  // True if the media is seekable only in buffered ranges.
  Mirror<bool> mMediaSeekableOnlyInBufferedRanges;

  // IsVisible, mirrored from the media decoder.
  Mirror<bool> mIsVisible;

  // Duration of the media. This is guaranteed to be non-null after we finish
  // decoding the first frame.
  Canonical<media::NullableTimeUnit> mDuration;

  // Whether we're currently in or transitioning to shutdown state.
  Canonical<bool> mIsShutdown;

  // The status of our next frame. Mirrored on the main thread and used to
  // compute ready state.
  Canonical<NextFrameStatus> mNextFrameStatus;

  // The time of the current frame in microseconds, corresponding to the "current
  // playback position" in HTML5. This is referenced from 0, which is the initial
  // playback position.
  Canonical<int64_t> mCurrentPosition;

  // Current playback position in the stream in bytes.
  Canonical<int64_t> mPlaybackOffset;

  // Used to distinguish whether the audio is producing sound.
  Canonical<bool> mIsAudioDataAudible;

public:
  AbstractCanonical<media::TimeIntervals>* CanonicalBuffered() const;

  AbstractCanonical<media::NullableTimeUnit>* CanonicalDuration() {
    return &mDuration;
  }
  AbstractCanonical<bool>* CanonicalIsShutdown() {
    return &mIsShutdown;
  }
  AbstractCanonical<NextFrameStatus>* CanonicalNextFrameStatus() {
    return &mNextFrameStatus;
  }
  AbstractCanonical<int64_t>* CanonicalCurrentPosition() {
    return &mCurrentPosition;
  }
  AbstractCanonical<int64_t>* CanonicalPlaybackOffset() {
    return &mPlaybackOffset;
  }
  AbstractCanonical<bool>* CanonicalIsAudioDataAudible() {
    return &mIsAudioDataAudible;
  }
};

} // namespace mozilla

#endif
