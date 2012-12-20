/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
Each video element for a media file has two threads:

  1) The Audio thread writes the decoded audio data to the audio
     hardware. This is done in a separate thread to ensure that the
     audio hardware gets a constant stream of data without
     interruption due to decoding or display. At some point
     libsydneyaudio will be refactored to have a callback interface
     where it asks for data and an extra thread will no longer be
     needed.

  2) The decode thread. This thread reads from the media stream and
     decodes the Theora and Vorbis data. It places the decoded data into
     queues for the other threads to pull from.

All file reads, seeks, and all decoding must occur on the decode thread.
Synchronisation of state between the thread is done via a monitor owned
by MediaDecoder.

The lifetime of the decode and audio threads is controlled by the state
machine when it runs on the shared state machine thread. When playback
needs to occur they are created and events dispatched to them to run
them. These events exit when decoding/audio playback is completed or
no longer required.

A/V synchronisation is handled by the state machine. It examines the audio
playback time and compares this to the next frame in the queue of video
frames. If it is time to play the video frame it is then displayed, otherwise
it schedules the state machine to run again at the time of the next frame.

Frame skipping is done in the following ways:

  1) The state machine will skip all frames in the video queue whose
     display time is less than the current audio time. This ensures
     the correct frame for the current time is always displayed.

  2) The decode thread will stop decoding interframes and read to the
     next keyframe if it determines that decoding the remaining
     interframes will cause playback issues. It detects this by:
       a) If the amount of audio data in the audio queue drops
          below a threshold whereby audio may start to skip.
       b) If the video queue drops below a threshold where it
          will be decoding video data that won't be displayed due
          to the decode thread dropping the frame immediately.

When hardware accelerated graphics is not available, YCbCr conversion
is done on the decode thread when video frames are decoded.

The decode thread pushes decoded audio and videos frames into two
separate queues - one for audio and one for video. These are kept
separate to make it easy to constantly feed audio data to the audio
hardware while allowing frame skipping of video data. These queues are
threadsafe, and neither the decode, audio, or state machine should
be able to monopolize them, and cause starvation of the other threads.

Both queues are bounded by a maximum size. When this size is reached
the decode thread will no longer decode video or audio depending on the
queue that has reached the threshold. If both queues are full, the decode
thread will wait on the decoder monitor.

When the decode queues are full (they've reaced their maximum size) and
the decoder is not in PLAYING play state, the state machine may opt
to shut down the decode thread in order to conserve resources.

During playback the audio thread will be idle (via a Wait() on the
monitor) if the audio queue is empty. Otherwise it constantly pops
audio data off the queue and plays it with a blocking write to the audio
hardware (via AudioStream and libsydneyaudio).

*/
#if !defined(MediaDecoderStateMachine_h__)
#define MediaDecoderStateMachine_h__

#include "nsThreadUtils.h"
#include "MediaDecoder.h"
#include "AudioAvailableEventManager.h"
#include "mozilla/ReentrantMonitor.h"
#include "nsITimer.h"
#include "AudioSegment.h"
#include "VideoSegment.h"

namespace mozilla {

class MediaDecoderReader;

/*
  The state machine class. This manages the decoding and seeking in the
  MediaDecoderReader on the decode thread, and A/V sync on the shared
  state machine thread, and controls the audio "push" thread.

  All internal state is synchronised via the decoder monitor. State changes
  are either propagated by NotifyAll on the monitor (typically when state
  changes need to be propagated to non-state machine threads) or by scheduling
  the state machine to run another cycle on the shared state machine thread.

  See MediaDecoder.h for more details.
*/
class MediaDecoderStateMachine : public nsRunnable
{
public:
  typedef MediaDecoder::DecodedStreamData DecodedStreamData;
  MediaDecoderStateMachine(MediaDecoder* aDecoder,
                               MediaDecoderReader* aReader,
                               bool aRealTime = false);
  ~MediaDecoderStateMachine();

  // nsDecoderStateMachine interface
  nsresult Init(MediaDecoderStateMachine* aCloneDonor);

  // Enumeration for the valid decoding states
  enum State {
    DECODER_STATE_DECODING_METADATA,
    DECODER_STATE_DECODING,
    DECODER_STATE_SEEKING,
    DECODER_STATE_BUFFERING,
    DECODER_STATE_COMPLETED,
    DECODER_STATE_SHUTDOWN
  };

  State GetState() {
    mDecoder->GetReentrantMonitor().AssertCurrentThreadIn();
    return mState;
  }

  // Set the audio volume. The decoder monitor must be obtained before
  // calling this.
  void SetVolume(double aVolume);
  void SetAudioCaptured(bool aCapture);
  void Shutdown();

  // Called from the main thread to get the duration. The decoder monitor
  // must be obtained before calling this. It is in units of microseconds.
  int64_t GetDuration();

  // Called from the main thread to set the duration of the media resource
  // if it is able to be obtained via HTTP headers. Called from the
  // state machine thread to set the duration if it is obtained from the
  // media metadata. The decoder monitor must be obtained before calling this.
  // aDuration is in microseconds.
  void SetDuration(int64_t aDuration);

  // Called while decoding metadata to set the end time of the media
  // resource. The decoder monitor must be obtained before calling this.
  // aEndTime is in microseconds.
  void SetMediaEndTime(int64_t aEndTime);

  // Functions used by assertions to ensure we're calling things
  // on the appropriate threads.
  bool OnDecodeThread() const {
    return IsCurrentThread(mDecodeThread);
  }
  bool OnStateMachineThread() const;
  bool OnAudioThread() const {
    return IsCurrentThread(mAudioThread);
  }

  MediaDecoderOwner::NextFrameStatus GetNextFrameStatus();

  // Cause state transitions. These methods obtain the decoder monitor
  // to synchronise the change of state, and to notify other threads
  // that the state has changed.
  void Play();

  // Seeks to aTime in seconds.
  void Seek(double aTime);

  // Returns the current playback position in seconds.
  // Called from the main thread to get the current frame time. The decoder
  // monitor must be obtained before calling this.
  double GetCurrentTime() const;

  // Clear the flag indicating that a playback position change event
  // is currently queued. This is called from the main thread and must
  // be called with the decode monitor held.
  void ClearPositionChangeFlag();

  // Called from the main thread or the decoder thread to set whether the media
  // resource can seek into unbuffered ranges. The decoder monitor must be
  // obtained before calling this.
  void SetTransportSeekable(bool aSeekable);

  // Called from the main thread or the decoder thread to set whether the media
  // can seek to random location. This is not true for chained ogg and WebM
  // media without index. The decoder monitor must be obtained before calling
  // this.
  void SetMediaSeekable(bool aSeekable);

  // Update the playback position. This can result in a timeupdate event
  // and an invalidate of the frame being dispatched asynchronously if
  // there is no such event currently queued.
  // Only called on the decoder thread. Must be called with
  // the decode monitor held.
  void UpdatePlaybackPosition(int64_t aTime);

  // Causes the state machine to switch to buffering state, and to
  // immediately stop playback and buffer downloaded data. Must be called
  // with the decode monitor held. Called on the state machine thread and
  // the main thread.
  void StartBuffering();

  // State machine thread run function. Defers to RunStateMachine().
  NS_IMETHOD Run();

  // This is called on the state machine thread and audio thread.
  // The decoder monitor must be obtained before calling this.
  bool HasAudio() const {
    mDecoder->GetReentrantMonitor().AssertCurrentThreadIn();
    return mInfo.mHasAudio;
  }

  // This is called on the state machine thread and audio thread.
  // The decoder monitor must be obtained before calling this.
  bool HasVideo() const {
    mDecoder->GetReentrantMonitor().AssertCurrentThreadIn();
    return mInfo.mHasVideo;
  }

  // Should be called by main thread.
  bool HaveNextFrameData() const;

  // Must be called with the decode monitor held.
  bool IsBuffering() const {
    mDecoder->GetReentrantMonitor().AssertCurrentThreadIn();

    return mState == DECODER_STATE_BUFFERING;
  }

  // Must be called with the decode monitor held.
  bool IsSeeking() const {
    mDecoder->GetReentrantMonitor().AssertCurrentThreadIn();

    return mState == DECODER_STATE_SEEKING;
  }

  nsresult GetBuffered(nsTimeRanges* aBuffered);

  void SetPlaybackRate(double aPlaybackRate);
  void SetPreservesPitch(bool aPreservesPitch);

  int64_t VideoQueueMemoryInUse() {
    if (mReader) {
      return mReader->VideoQueueMemoryInUse();
    }
    return 0;
  }

  int64_t AudioQueueMemoryInUse() {
    if (mReader) {
      return mReader->AudioQueueMemoryInUse();
    }
    return 0;
  }

  void NotifyDataArrived(const char* aBuffer, uint32_t aLength, int64_t aOffset);

  int64_t GetEndMediaTime() const {
    mDecoder->GetReentrantMonitor().AssertCurrentThreadIn();
    return mEndTime;
  }

  bool IsTransportSeekable() {
    mDecoder->GetReentrantMonitor().AssertCurrentThreadIn();
    return mTransportSeekable;
  }

  bool IsMediaSeekable() {
    mDecoder->GetReentrantMonitor().AssertCurrentThreadIn();
    return mMediaSeekable;
  }

  // Sets the current frame buffer length for the MozAudioAvailable event.
  // Accessed on the main and state machine threads.
  void SetFrameBufferLength(uint32_t aLength);

  // Returns the shared state machine thread.
  static nsIThread* GetStateMachineThread();

  // Calls ScheduleStateMachine() after taking the decoder lock. Also
  // notifies the decoder thread in case it's waiting on the decoder lock.
  void ScheduleStateMachineWithLockAndWakeDecoder();

  // Schedules the shared state machine thread to run the state machine
  // in aUsecs microseconds from now, if it's not already scheduled to run
  // earlier, in which case the request is discarded.
  nsresult ScheduleStateMachine(int64_t aUsecs = 0);

  // Creates and starts a new decode thread. Don't call this directly,
  // request a new decode thread by calling
  // StateMachineTracker::RequestCreateDecodeThread().
  // The decoder monitor must not be held. Called on the state machine thread.
  nsresult StartDecodeThread();

  // Timer function to implement ScheduleStateMachine(aUsecs).
  void TimeoutExpired();

  // Set the media fragment end time. aEndTime is in microseconds.
  void SetFragmentEndTime(int64_t aEndTime);

  // Drop reference to decoder.  Only called during shutdown dance.
  void ReleaseDecoder() { mDecoder = nullptr; }

   // Called when a "MozAudioAvailable" event listener is added to the media
   // element. Called on the main thread.
   void NotifyAudioAvailableListener();

  // Copy queued audio/video data in the reader to any output MediaStreams that
  // need it.
  void SendStreamData();
  void FinishStreamData();
  bool HaveEnoughDecodedAudio(int64_t aAmpleAudioUSecs);
  bool HaveEnoughDecodedVideo();

  // Returns true if the state machine has shutdown or is in the process of
  // shutting down. The decoder monitor must be held while calling this.
  bool IsShutdown();

  void QueueMetadata(int64_t aPublishTime, int aChannels, int aRate, bool aHasAudio, MetadataTags* aTags);

protected:
  virtual uint32_t GetAmpleVideoFrames() { return mAmpleVideoFrames; }

private:
  class WakeDecoderRunnable : public nsRunnable {
  public:
    WakeDecoderRunnable(MediaDecoderStateMachine* aSM)
      : mMutex("WakeDecoderRunnable"), mStateMachine(aSM) {}
    NS_IMETHOD Run()
    {
      nsRefPtr<MediaDecoderStateMachine> stateMachine;
      {
        // Don't let Run() (called by media stream graph thread) race with
        // Revoke() (called by decoder state machine thread)
        MutexAutoLock lock(mMutex);
        if (!mStateMachine)
          return NS_OK;
        stateMachine = mStateMachine;
      }
      stateMachine->ScheduleStateMachineWithLockAndWakeDecoder();
      return NS_OK;
    }
    void Revoke()
    {
      MutexAutoLock lock(mMutex);
      mStateMachine = nullptr;
    }

    Mutex mMutex;
    // Protected by mMutex.
    // We don't use an owning pointer here, because keeping mStateMachine alive
    // would mean in some cases we'd have to destroy mStateMachine from this
    // object, which would be problematic since MediaDecoderStateMachine can
    // only be destroyed on the main thread whereas this object can be destroyed
    // on the media stream graph thread.
    MediaDecoderStateMachine* mStateMachine;
  };
  WakeDecoderRunnable* GetWakeDecoderRunnable();

  // Returns true if we've got less than aAudioUsecs microseconds of decoded
  // and playable data. The decoder monitor must be held.
  bool HasLowDecodedData(int64_t aAudioUsecs) const;

  // Returns true if we're running low on data which is not yet decoded.
  // The decoder monitor must be held.
  bool HasLowUndecodedData() const;

  // Returns the number of microseconds of undecoded data available for
  // decoding. The decoder monitor must be held.
  int64_t GetUndecodedData() const;

  // Returns the number of unplayed usecs of audio we've got decoded and/or
  // pushed to the hardware waiting to play. This is how much audio we can
  // play without having to run the audio decoder. The decoder monitor
  // must be held.
  int64_t AudioDecodedUsecs() const;

  // Returns true when there's decoded audio waiting to play.
  // The decoder monitor must be held.
  bool HasFutureAudio() const;

  // Returns true if we recently exited "quick buffering" mode.
  bool JustExitedQuickBuffering();

  // Waits on the decoder ReentrantMonitor for aUsecs microseconds. If the decoder
  // monitor is awoken by a Notify() call, we'll continue waiting, unless
  // we've moved into shutdown state. This enables us to ensure that we
  // wait for a specified time, and that the myriad of Notify()s we do on
  // the decoder monitor don't cause the audio thread to be starved. aUsecs
  // values of less than 1 millisecond are rounded up to 1 millisecond
  // (see bug 651023). The decoder monitor must be held. Called only on the
  // audio thread.
  void Wait(int64_t aUsecs);

  // Dispatches an asynchronous event to update the media element's ready state.
  void UpdateReadyState();

  // Resets playback timing data. Called when we seek, on the decode thread.
  void ResetPlayback();

  // Returns the audio clock, if we have audio, or -1 if we don't.
  // Called on the state machine thread.
  int64_t GetAudioClock();

  // Get the video stream position, taking the |playbackRate| change into
  // account. This is a position in the media, not the duration of the playback
  // so far.
  int64_t GetVideoStreamPosition();

  // Return the current time, either the audio clock if available (if the media
  // has audio, and the playback is possible), or a clock for the video.
  // Called on the state machine thread.
  int64_t GetClock();

  // Returns the presentation time of the first audio or video frame in the
  // media.  If the media has video, it returns the first video frame. The
  // decoder monitor must be held with exactly one lock count. Called on the
  // state machine thread.
  VideoData* FindStartTime();

  // Update only the state machine's current playback position (and duration,
  // if unknown).  Does not update the playback position on the decoder or
  // media element -- use UpdatePlaybackPosition for that.  Called on the state
  // machine thread, caller must hold the decoder lock.
  void UpdatePlaybackPositionInternal(int64_t aTime);

  // Pushes the image down the rendering pipeline. Called on the shared state
  // machine thread. The decoder monitor must *not* be held when calling this.
  void RenderVideoFrame(VideoData* aData, TimeStamp aTarget);

  // If we have video, display a video frame if it's time for display has
  // arrived, otherwise sleep until it's time for the next frame. Update the
  // current frame time as appropriate, and trigger ready state update.  The
  // decoder monitor must be held with exactly one lock count. Called on the
  // state machine thread.
  void AdvanceFrame();

  // Write aFrames of audio frames of silence to the audio hardware. Returns
  // the number of frames actually written. The write size is capped at
  // SILENCE_BYTES_CHUNK (32kB), so must be called in a loop to write the
  // desired number of frames. This ensures that the playback position
  // advances smoothly, and guarantees that we don't try to allocate an
  // impossibly large chunk of memory in order to play back silence. Called
  // on the audio thread.
  uint32_t PlaySilence(uint32_t aFrames,
                       uint32_t aChannels,
                       uint64_t aFrameOffset);

  // Pops an audio chunk from the front of the audio queue, and pushes its
  // audio data to the audio hardware. MozAudioAvailable data is also queued
  // here. Called on the audio thread.
  uint32_t PlayFromAudioQueue(uint64_t aFrameOffset, uint32_t aChannels);

  // Stops the decode thread, and if we have a pending request for a new
  // decode thread it is canceled. The decoder monitor must be held with exactly
  // one lock count. Called on the state machine thread.
  void StopDecodeThread();

  // Stops the audio thread. The decoder monitor must be held with exactly
  // one lock count. Called on the state machine thread.
  void StopAudioThread();

  // Ensures the decode thread is running if it already exists, or requests
  // a new decode thread be started if there currently is no decode thread.
  // The decoder monitor must be held with exactly one lock count. Called on
  // the state machine thread.
  nsresult ScheduleDecodeThread();

  // Starts the audio thread. The decoder monitor must be held with exactly
  // one lock count. Called on the state machine thread.
  nsresult StartAudioThread();

  // The main loop for the audio thread. Sent to the thread as
  // an nsRunnableMethod. This continually does blocking writes to
  // to audio stream to play audio data.
  void AudioLoop();

  // Sets internal state which causes playback of media to pause.
  // The decoder monitor must be held. Called on the state machine,
  // and decode threads.
  void StopPlayback();

  // Sets internal state which causes playback of media to begin or resume.
  // Must be called with the decode monitor held. Called on the state machine
  // and decode threads.
  void StartPlayback();

  // Moves the decoder into decoding state. Called on the state machine
  // thread. The decoder monitor must be held.
  void StartDecoding();

  // Returns true if we're currently playing. The decoder monitor must
  // be held.
  bool IsPlaying();

  // Returns the "media time". This is the absolute time which the media
  // playback has reached. i.e. this returns values in the range
  // [mStartTime, mEndTime], and mStartTime will not be 0 if the media does
  // not start at 0. Note this is different to the value returned
  // by GetCurrentTime(), which is in the range [0,duration].
  int64_t GetMediaTime() const {
    mDecoder->GetReentrantMonitor().AssertCurrentThreadIn();
    return mStartTime + mCurrentFrameTime;
  }

  // Returns an upper bound on the number of microseconds of audio that is
  // decoded and playable. This is the sum of the number of usecs of audio which
  // is decoded and in the reader's audio queue, and the usecs of unplayed audio
  // which has been pushed to the audio hardware for playback. Note that after
  // calling this, the audio hardware may play some of the audio pushed to
  // hardware, so this can only be used as a upper bound. The decoder monitor
  // must be held when calling this. Called on the decode thread.
  int64_t GetDecodedAudioDuration();

  // Load metadata. Called on the decode thread. The decoder monitor
  // must be held with exactly one lock count.
  nsresult DecodeMetadata();

  // Seeks to mSeekTarget. Called on the decode thread. The decoder monitor
  // must be held with exactly one lock count.
  void DecodeSeek();

  // Decode loop, decodes data until EOF or shutdown.
  // Called on the decode thread.
  void DecodeLoop();

  // Decode thread run function. Determines which of the Decode*() functions
  // to call.
  void DecodeThreadRun();

  // Copy audio from an AudioData packet to aOutput. This may require
  // inserting silence depending on the timing of the audio packet.
  void SendStreamAudio(AudioData* aAudio, DecodedStreamData* aStream,
                       AudioSegment* aOutput);

  // State machine thread run function. Defers to RunStateMachine().
  nsresult CallRunStateMachine();

  // Performs one "cycle" of the state machine. Polls the state, and may send
  // a video frame to be displayed, and generally manages the decode. Called
  // periodically via timer to ensure the video stays in sync.
  nsresult RunStateMachine();

  bool IsStateMachineScheduled() const {
    mDecoder->GetReentrantMonitor().AssertCurrentThreadIn();
    return !mTimeout.IsNull() || mRunAgain;
  }

  // Returns true if we're not playing and the decode thread has filled its
  // decode buffers and is waiting. We can shut the decode thread down in this
  // case as it may not be needed again.
  bool IsPausedAndDecoderWaiting();

  // The decoder object that created this state machine. The state machine
  // holds a strong reference to the decoder to ensure that the decoder stays
  // alive once media element has started the decoder shutdown process, and has
  // dropped its reference to the decoder. This enables the state machine to
  // keep using the decoder's monitor until the state machine has finished
  // shutting down, without fear of the monitor being destroyed. After
  // shutting down, the state machine will then release this reference,
  // causing the decoder to be destroyed. This is accessed on the decode,
  // state machine, audio and main threads.
  nsRefPtr<MediaDecoder> mDecoder;

  // The decoder monitor must be obtained before modifying this state.
  // NotifyAll on the monitor must be called when the state is changed so
  // that interested threads can wake up and alter behaviour if appropriate
  // Accessed on state machine, audio, main, and AV thread.
  State mState;

  // Thread for pushing audio onto the audio hardware.
  // The "audio push thread".
  nsCOMPtr<nsIThread> mAudioThread;

  // Thread for decoding video in background. The "decode thread".
  nsCOMPtr<nsIThread> mDecodeThread;

  // Timer to call the state machine Run() method. Used by
  // ScheduleStateMachine(). Access protected by decoder monitor.
  nsCOMPtr<nsITimer> mTimer;

  // Timestamp at which the next state machine Run() method will be called.
  // If this is non-null, a call to Run() is scheduled, either by a timer,
  // or via an event. Access protected by decoder monitor.
  TimeStamp mTimeout;

  // The time that playback started from the system clock. This is used for
  // timing the presentation of video frames when there's no audio.
  // Accessed only via the state machine thread.
  TimeStamp mPlayStartTime;

  // When the playbackRate changes, and there is no audio clock, it is necessary
  // to reset the mPlayStartTime. This is done next time the clock is queried,
  // when this member is true. Access protected by decoder monitor.
  bool mResetPlayStartTime;

  // The amount of time we've spent playing already the media. The current
  // playback position is therefore |Now() - mPlayStartTime +
  // mPlayDuration|, which must be adjusted by mStartTime if used with media
  // timestamps.  Accessed only via the state machine thread.
  int64_t mPlayDuration;

  // Time that buffering started. Used for buffering timeout and only
  // accessed on the state machine thread. This is null while we're not
  // buffering.
  TimeStamp mBufferingStart;

  // Start time of the media, in microseconds. This is the presentation
  // time of the first frame decoded from the media, and is used to calculate
  // duration and as a bounds for seeking. Accessed on state machine, decode,
  // and main threads. Access controlled by decoder monitor.
  int64_t mStartTime;

  // Time of the last frame in the media, in microseconds. This is the
  // end time of the last frame in the media. Accessed on state
  // machine, decode, and main threads. Access controlled by decoder monitor.
  int64_t mEndTime;

  // Position to seek to in microseconds when the seek state transition occurs.
  // The decoder monitor lock must be obtained before reading or writing
  // this value. Accessed on main and decode thread.
  int64_t mSeekTime;

  // Media Fragment end time in microseconds. Access controlled by decoder monitor.
  int64_t mFragmentEndTime;

  // The audio stream resource. Used on the state machine, and audio threads.
  // This is created and destroyed on the audio thread, while holding the
  // decoder monitor, so if this is used off the audio thread, you must
  // first acquire the decoder monitor and check that it is non-null.
  nsAutoPtr<AudioStream> mAudioStream;

  // The reader, don't call its methods with the decoder monitor held.
  // This is created in the play state machine's constructor, and destroyed
  // in the play state machine's destructor.
  nsAutoPtr<MediaDecoderReader> mReader;

  // Accessed only on the state machine thread.
  // Not an nsRevocableEventPtr since we must Revoke() it well before
  // this object is destroyed, anyway.
  // Protected by decoder monitor except during the SHUTDOWN state after the
  // decoder thread has been stopped.
  nsRevocableEventPtr<WakeDecoderRunnable> mPendingWakeDecoder;

  // The time of the current frame in microseconds. This is referenced from
  // 0 which is the initial playback position. Set by the state machine
  // thread, and read-only from the main thread to get the current
  // time value. Synchronised via decoder monitor.
  int64_t mCurrentFrameTime;

  // The presentation time of the first audio frame that was played in
  // microseconds. We can add this to the audio stream position to determine
  // the current audio time. Accessed on audio and state machine thread.
  // Synchronized by decoder monitor.
  int64_t mAudioStartTime;

  // The end time of the last audio frame that's been pushed onto the audio
  // hardware in microseconds. This will approximately be the end time of the
  // audio stream, unless another frame is pushed to the hardware.
  int64_t mAudioEndTime;

  // The presentation end time of the last video frame which has been displayed
  // in microseconds. Accessed from the state machine thread.
  int64_t mVideoFrameEndTime;

  // Volume of playback. 0.0 = muted. 1.0 = full volume. Read/Written
  // from the state machine and main threads. Synchronised via decoder
  // monitor.
  double mVolume;

  // Playback rate. 1.0 : normal speed, 0.5 : two times slower. Synchronized via
  // decoder monitor.
  double mPlaybackRate;

  // Pitch preservation for the playback rate. Synchronized via decoder monitor.
  bool mPreservesPitch;

  // Position at which the last playback rate change occured, used to compute
  // the actual position in the stream when the playback rate changes and there
  // is no audio to be sync-ed to. Synchronized via decoder monitor.
  int64_t mBasePosition;

  // Time at which we started decoding. Synchronised via decoder monitor.
  TimeStamp mDecodeStartTime;

  // The maximum number of second we spend buffering when we are short on
  // unbuffered data.
  uint32_t mBufferingWait;
  int64_t  mLowDataThresholdUsecs;

  // If we've got more than mAmpleVideoFrames decoded video frames waiting in
  // the video queue, we will not decode any more video frames until some have
  // been consumed by the play state machine thread.
  uint32_t mAmpleVideoFrames;
  // True if we shouldn't play our audio (but still write it to any capturing
  // streams). When this is true, mStopAudioThread is always true and
  // the audio thread will never start again after it has stopped.
  bool mAudioCaptured;

  // True if the media resource can be seeked on a transport level. Accessed
  // from the state machine and main threads. Synchronised via decoder monitor.
  bool mTransportSeekable;

  // True if the media can be seeked. Accessed from the state machine and main
  // threads. Synchronised via decoder monitor.
  bool mMediaSeekable;

  // True if an event to notify about a change in the playback
  // position has been queued, but not yet run. It is set to false when
  // the event is run. This allows coalescing of these events as they can be
  // produced many times per second. Synchronised via decoder monitor.
  // Accessed on main and state machine threads.
  bool mPositionChangeQueued;

  // True if the audio playback thread has finished. It is finished
  // when either all the audio frames in the Vorbis bitstream have completed
  // playing, or we've moved into shutdown state, and the threads are to be
  // destroyed. Written by the audio playback thread and read and written by
  // the state machine thread. Synchronised via decoder monitor.
  bool mAudioCompleted;

  // True if mDuration has a value obtained from an HTTP header, or from
  // the media index/metadata. Accessed on the state machine thread.
  bool mGotDurationFromMetaData;

  // False while decode thread should be running. Accessed state machine
  // and decode threads. Syncrhonised by decoder monitor.
  bool mStopDecodeThread;

  // True when the decode thread run function has finished, but the thread
  // has not necessarily been shut down yet. This can happen if we switch
  // from COMPLETED state to SEEKING before the state machine has a chance
  // to run in the COMPLETED state and shutdown the decode thread.
  // Synchronised by the decoder monitor.
  bool mDecodeThreadIdle;

  // False while audio thread should be running. Accessed state machine
  // and audio threads. Syncrhonised by decoder monitor.
  bool mStopAudioThread;

  // If this is true while we're in buffering mode, we can exit early,
  // as it's likely we may be able to playback. This happens when we enter
  // buffering mode soon after the decode starts, because the decode-ahead
  // ran fast enough to exhaust all data while the download is starting up.
  // Synchronised via decoder monitor.
  bool mQuickBuffering;

  // True if the shared state machine thread is currently running this
  // state machine.
  bool mIsRunning;

  // True if we should run the state machine again once the current
  // state machine run has finished.
  bool mRunAgain;

  // True if we've dispatched an event to run the state machine. It's
  // imperative that we don't dispatch multiple events to run the state
  // machine at the same time, as our code assume all events are synchronous.
  // If we dispatch multiple events, the second event can run while the
  // first is shutting down a thread, causing inconsistent state.
  bool mDispatchedRunEvent;

  // True if the decode thread has gone filled its buffers and is now
  // waiting to be awakened before it continues decoding. Synchronized
  // by the decoder monitor.
  bool mDecodeThreadWaiting;

  // True is we are decoding a realtime stream, like a camera stream
  bool mRealTime;

  // Record whether audio and video decoding were throttled during the
  // previous iteration of DecodeLooop. When we transition from
  // throttled to not-throttled we need to pump decoding.
  bool mDidThrottleAudioDecoding;
  bool mDidThrottleVideoDecoding;

  // True if we've requested a new decode thread, but it has not yet been
  // created. Synchronized by the decoder monitor.
  bool mRequestedNewDecodeThread;

  // Manager for queuing and dispatching MozAudioAvailable events.  The
  // event manager is accessed from the state machine and audio threads,
  // and takes care of synchronizing access to its internal queue.
  AudioAvailableEventManager mEventManager;

  // Stores presentation info required for playback. The decoder monitor
  // must be held when accessing this.
  VideoInfo mInfo;

  mozilla::MediaMetadataManager mMetadataManager;

  MediaDecoderOwner::NextFrameStatus mLastFrameStatus;
};

} // namespace mozilla;
#endif
