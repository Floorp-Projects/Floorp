/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GRAPHDRIVER_H_
#define GRAPHDRIVER_H_

#include "nsAutoPtr.h"
#include "nsAutoRef.h"
#include "AudioBufferUtils.h"
#include "AudioMixer.h"
#include "AudioSegment.h"
#include "mozilla/Atomics.h"

struct cubeb_stream;

template <>
class nsAutoRefTraits<cubeb_stream> : public nsPointerRefTraits<cubeb_stream>
{
public:
  static void Release(cubeb_stream* aStream) { cubeb_stream_destroy(aStream); }
};

namespace mozilla {

/**
 * Assume we can run an iteration of the MediaStreamGraph loop in this much time
 * or less.
 * We try to run the control loop at this rate.
 */
static const int MEDIA_GRAPH_TARGET_PERIOD_MS = 10;

/**
 * Assume that we might miss our scheduled wakeup of the MediaStreamGraph by
 * this much.
 */
static const int SCHEDULE_SAFETY_MARGIN_MS = 10;

/**
 * Try have this much audio buffered in streams and queued to the hardware.
 * The maximum delay to the end of the next control loop
 * is 2*MEDIA_GRAPH_TARGET_PERIOD_MS + SCHEDULE_SAFETY_MARGIN_MS.
 * There is no point in buffering more audio than this in a stream at any
 * given time (until we add processing).
 * This is not optimal yet.
 */
static const int AUDIO_TARGET_MS = 2*MEDIA_GRAPH_TARGET_PERIOD_MS +
    SCHEDULE_SAFETY_MARGIN_MS;

/**
 * Try have this much video buffered. Video frames are set
 * near the end of the iteration of the control loop. The maximum delay
 * to the setting of the next video frame is 2*MEDIA_GRAPH_TARGET_PERIOD_MS +
 * SCHEDULE_SAFETY_MARGIN_MS. This is not optimal yet.
 */
static const int VIDEO_TARGET_MS = 2*MEDIA_GRAPH_TARGET_PERIOD_MS +
    SCHEDULE_SAFETY_MARGIN_MS;

class MediaStreamGraphImpl;
class MessageBlock;

/**
 * Microseconds relative to the start of the graph timeline.
 */
typedef int64_t GraphTime;
const GraphTime GRAPH_TIME_MAX = MEDIA_TIME_MAX;

class AudioCallbackDriver;

/**
 * A driver is responsible for the scheduling of the processing, the thread
 * management, and give the different clocks to a MediaStreamGraph. This is an
 * abstract base class. A MediaStreamGraph can be driven by an
 * OfflineClockDriver, if the graph is offline, or a SystemClockDriver, if the
 * graph is real time.
 * A MediaStreamGraph holds an owning reference to its driver.
 */
class GraphDriver
{
public:
  explicit GraphDriver(MediaStreamGraphImpl* aGraphImpl);

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(GraphDriver);
  /* When the graph wakes up to do an iteration, this returns the range of time
   * that will be processed. */
  virtual void GetIntervalForIteration(GraphTime& aFrom,
                                       GraphTime& aTo) = 0;
  /* Returns the current time for this graph. This is the end of the current
   * iteration. */
  virtual GraphTime GetCurrentTime() = 0;
  /* For real-time graphs, this waits until it's time to process more data. For
   * offline graphs, this is a no-op. */
  virtual void WaitForNextIteration() = 0;
  /* Wakes up the graph if it is waiting. */
  virtual void WakeUp() = 0;
  virtual void Destroy() {}
  /* Start the graph, init the driver, start the thread. */
  virtual void Start() = 0;
  /* Stop the graph, shutting down the thread. */
  virtual void Stop() = 0;
  /* Resume after a stop */
  virtual void Resume() = 0;
  /* Revive this driver, as more messages just arrived. */
  virtual void Revive() = 0;
  void Shutdown();
  /* Rate at which the GraphDriver runs, in ms. This can either be user
   * controlled (because we are using a {System,Offline}ClockDriver, and decide
   * how often we want to wakeup/how much we want to process per iteration), or
   * it can be indirectly set by the latency of the audio backend, and the
   * number of buffers of this audio backend: say we have four buffers, and 40ms
   * latency, we will get a callback approximately every 10ms. */
  virtual uint32_t IterationDuration() = 0;

  /* Return whether we are switching or not. */
  bool Switching() {
    return mNextDriver || mPreviousDriver;
  }

  /**
   * If we are running a real time graph, get the current time stamp to schedule
   * video frames. This has to be reimplemented by real time drivers.
   */
  virtual TimeStamp GetCurrentTimeStamp() {
    return mCurrentTimeStamp;
  }

  bool IsWaiting() {
    return mWaitState == WAITSTATE_WAITING_INDEFINITELY ||
           mWaitState == WAITSTATE_WAITING_FOR_NEXT_ITERATION;
  }

  bool IsWaitingIndefinitly() {
    return mWaitState == WAITSTATE_WAITING_INDEFINITELY;
  }

  GraphTime IterationStart() {
    return mIterationStart;
  }

  GraphTime IterationEnd() {
    return mIterationEnd;
  }

  GraphTime StateComputedTime() {
    return mStateComputedTime;
  }

  virtual void GetAudioBuffer(float** aBuffer, long& aFrames) {
    MOZ_CRASH("This is not an Audio GraphDriver!");
  }

  virtual AudioCallbackDriver* AsAudioCallbackDriver() {
    return nullptr;
  }

  /**
   * Tell the driver it has to stop and return the current time of the graph, so
   * another driver can start from the right point in time.
   */
  virtual void SwitchAtNextIteration(GraphDriver* aDriver);

  /**
   * Set the time for a graph, on a driver. This is used so a new driver just
   * created can start at the right point in time.
   */
  void SetGraphTime(GraphDriver* aPreviousDriver,
                    GraphTime aLastSwitchNextIterationStart,
                    GraphTime aLastSwitchNextIterationEnd,
                    GraphTime aLastSwitchNextStateComputedTime,
                    GraphTime aLastSwitchStateComputedTime);

  /**
   * Whenever the graph has computed the time until it has all state
   * (mStateComputedState), it calls this to indicate the new time until which
   * we have computed state.
   */
  void UpdateStateComputedTime(GraphTime aStateComputedTime);

  /**
   * Call this to indicate that another iteration of the control loop is
   * required immediately. The monitor must already be held.
   */
  void EnsureImmediateWakeUpLocked();

  /**
   * Call this to indicate that another iteration of the control loop is
   * required on its regular schedule. The monitor must not be held.
   * This function has to be idempotent.
   */
  void EnsureNextIteration();

  /**
   * Same thing, but not locked.
   */
  void EnsureNextIterationLocked();

  MediaStreamGraphImpl* GraphImpl() {
    return mGraphImpl;
  }

  virtual bool OnThread() = 0;

protected:
  // Time of the start of this graph iteration.
  GraphTime mIterationStart;
  // Time of the end of this graph iteration.
  GraphTime mIterationEnd;
  // Time, in the future, for which blocking has been computed.
  GraphTime mStateComputedTime;
  GraphTime mNextStateComputedTime;
  // The MediaStreamGraphImpl that owns this driver. This has a lifetime longer
  // than the driver, and will never be null.
  MediaStreamGraphImpl* mGraphImpl;

  // This enum specifies the wait state of the driver.
  enum WaitState {
    // RunThread() is running normally
    WAITSTATE_RUNNING,
    // RunThread() is paused waiting for its next iteration, which will
    // happen soon
    WAITSTATE_WAITING_FOR_NEXT_ITERATION,
    // RunThread() is paused indefinitely waiting for something to change
    WAITSTATE_WAITING_INDEFINITELY,
    // Something has signaled RunThread() to wake up immediately,
    // but it hasn't done so yet
    WAITSTATE_WAKING_UP
  };
  WaitState mWaitState;

  // True if the graph needs another iteration after the current iteration.
  bool mNeedAnotherIteration;
  TimeStamp mCurrentTimeStamp;
  // This is non-null only when this driver has recently switched from an other
  // driver, and has not cleaned it up yet (for example because the audio stream
  // is currently calling the callback during initialization).
  nsRefPtr<GraphDriver> mPreviousDriver;
  // This is non-null only when this driver is going to switch to an other
  // driver at the end of this iteration.
  nsRefPtr<GraphDriver> mNextDriver;
  virtual ~GraphDriver()
  { }
};

class MediaStreamGraphInitThreadRunnable;

/**
 * This class is a driver that manages its own thread.
 */
class ThreadedDriver : public GraphDriver
{
public:
  explicit ThreadedDriver(MediaStreamGraphImpl* aGraphImpl);
  virtual ~ThreadedDriver();
  virtual void Start() MOZ_OVERRIDE;
  virtual void Stop() MOZ_OVERRIDE;
  virtual void Resume() MOZ_OVERRIDE;
  virtual void Revive() MOZ_OVERRIDE;
  /**
   * Runs main control loop on the graph thread. Normally a single invocation
   * of this runs for the entire lifetime of the graph thread.
   */
  void RunThread();
  friend class MediaStreamGraphInitThreadRunnable;
  uint32_t IterationDuration() {
    return MEDIA_GRAPH_TARGET_PERIOD_MS;
  }

  virtual bool OnThread() MOZ_OVERRIDE { return !mThread || NS_GetCurrentThread() == mThread; }

protected:
  nsCOMPtr<nsIThread> mThread;
};

/**
 * A SystemClockDriver drives a MediaStreamGraph using a system clock, and waits
 * using a monitor, between each iteration.
 */
class SystemClockDriver : public ThreadedDriver
{
public:
  explicit SystemClockDriver(MediaStreamGraphImpl* aGraphImpl);
  virtual ~SystemClockDriver();
  virtual void GetIntervalForIteration(GraphTime& aFrom,
                                       GraphTime& aTo) MOZ_OVERRIDE;
  virtual GraphTime GetCurrentTime() MOZ_OVERRIDE;
  virtual void WaitForNextIteration() MOZ_OVERRIDE;
  virtual void WakeUp() MOZ_OVERRIDE;


private:
  TimeStamp mInitialTimeStamp;
  TimeStamp mLastTimeStamp;
};

/**
 * An OfflineClockDriver runs the graph as fast as possible, without waiting
 * between iteration.
 */
class OfflineClockDriver : public ThreadedDriver
{
public:
  OfflineClockDriver(MediaStreamGraphImpl* aGraphImpl, GraphTime aSlice);
  virtual ~OfflineClockDriver();
  virtual void GetIntervalForIteration(GraphTime& aFrom,
                                       GraphTime& aTo) MOZ_OVERRIDE;
  virtual GraphTime GetCurrentTime() MOZ_OVERRIDE;
  virtual void WaitForNextIteration() MOZ_OVERRIDE;
  virtual void WakeUp() MOZ_OVERRIDE;
  virtual TimeStamp GetCurrentTimeStamp() MOZ_OVERRIDE;

private:
  // Time, in GraphTime, for each iteration
  GraphTime mSlice;
};

/**
 * This is a graph driver that is based on callback functions called by the
 * audio api. This ensures minimal audio latency, because it means there is no
 * buffering happening: the audio is generated inside the callback.
 *
 * This design is less flexible than running our own thread:
 * - We have no control over the thread:
 * - It cannot block, and it has to run for a shorter amount of time than the
 *   buffer it is going to fill, or an under-run is going to occur (short burst
 *   of silence in the final audio output).
 * - We can't know for sure when the callback function is going to be called
 *   (although we compute an estimation so we can schedule video frames)
 * - Creating and shutting the thread down is a blocking operation, that can
 *   take _seconds_ in some cases (because IPC has to be set up, and
 *   sometimes hardware components are involved and need to be warmed up)
 * - We have no control on how much audio we generate, we have to return exactly
 *   the number of frames asked for by the callback. Since for the Web Audio
 *   API, we have to do block processing at 128 frames per block, we need to
 *   keep a little spill buffer to store the extra frames.
 */
class AudioCallbackDriver : public GraphDriver,
                            public MixerCallbackReceiver
{
public:
  explicit AudioCallbackDriver(MediaStreamGraphImpl* aGraphImpl,
                               dom::AudioChannel aChannel = dom::AudioChannel::Normal);
  virtual ~AudioCallbackDriver();

  virtual void Destroy() MOZ_OVERRIDE;
  virtual void Start() MOZ_OVERRIDE;
  virtual void Stop() MOZ_OVERRIDE;
  virtual void Resume() MOZ_OVERRIDE;
  virtual void Revive() MOZ_OVERRIDE;
  virtual void GetIntervalForIteration(GraphTime& aFrom,
                                       GraphTime& aTo) MOZ_OVERRIDE;
  virtual GraphTime GetCurrentTime() MOZ_OVERRIDE;
  virtual void WaitForNextIteration() MOZ_OVERRIDE;
  virtual void WakeUp() MOZ_OVERRIDE;

  /* Static wrapper function cubeb calls back. */
  static long DataCallback_s(cubeb_stream * aStream,
                             void * aUser, void * aBuffer,
                             long aFrames);
  static void StateCallback_s(cubeb_stream* aStream, void * aUser,
                              cubeb_state aState);
  static void DeviceChangedCallback_s(void * aUser);
  /* This function is called by the underlying audio backend when a refill is
   * needed. This is what drives the whole graph when it is used to output
   * audio. If the return value is exactly aFrames, this function will get
   * called again. If it is less than aFrames, the stream will go in draining
   * mode, and this function will not be called again. */
  long DataCallback(AudioDataValue* aBuffer, long aFrames);
  /* This function is called by the underlying audio backend, but is only used
   * for informational purposes at the moment. */
  void StateCallback(cubeb_state aState);
  /* This is an approximation of the number of millisecond there are between two
   * iterations of the graph. */
  uint32_t IterationDuration();

  /* This function gets called when the graph has produced the audio frames for
   * this iteration. */
  virtual void MixerCallback(AudioDataValue* aMixedBuffer,
                             AudioSampleFormat aFormat,
                             uint32_t aChannels,
                             uint32_t aFrames,
                             uint32_t aSampleRate) MOZ_OVERRIDE;

  virtual AudioCallbackDriver* AsAudioCallbackDriver() {
    return this;
  }

  /**
   * Whether the audio callback is processing. This is for asserting only.
   */
  bool InCallback();

  virtual bool OnThread() MOZ_OVERRIDE { return !mStarted || InCallback(); }

  /* Whether the underlying cubeb stream has been started. See comment for
   * mStarted for details. */
  bool IsStarted();

  /* Tell the driver whether this process is using a microphone or not. This is
   * thread safe. */
  void SetMicrophoneActive(bool aActive);
private:
  /**
   * On certain MacBookPro, the microphone is located near the left speaker.
   * We need to pan the sound output to the right speaker if we are using the
   * mic and the built-in speaker, or we will have terrible echo.  */
  void PanOutputIfNeeded(bool aMicrophoneActive);
  /**
   * This is called when the output device used by the cubeb stream changes. */
  void DeviceChangedCallback();
  /* Start the cubeb stream */
  void StartStream();
  friend class AsyncCubebTask;
  void Init();
  /* MediaStreamGraphs are always down/up mixed to stereo for now. */
  static const uint32_t ChannelCount = 2;
  /* The size of this buffer comes from the fact that some audio backends can
   * call back with a number of frames lower than one block (128 frames), so we
   * need to keep at most two block in the SpillBuffer, because we always round
   * up to block boundaries during an iteration. */
  SpillBuffer<AudioDataValue, WEBAUDIO_BLOCK_SIZE * 2, ChannelCount> mScratchBuffer;
  /* Wrapper to ensure we write exactly the number of frames we need in the
   * audio buffer cubeb passes us. */
  AudioCallbackBufferWrapper<AudioDataValue, ChannelCount> mBuffer;
  /* cubeb stream for this graph. This is guaranteed to be non-null after Init()
   * has been called. */
  nsAutoRef<cubeb_stream> mAudioStream;
  /* The sample rate for the aforementionned cubeb stream. */
  uint32_t mSampleRate;
  /* Approximation of the time between two callbacks. This is used to schedule
   * video frames. This is in milliseconds. */
  uint32_t mIterationDurationMS;
  /* cubeb_stream_init calls the audio callback to prefill the buffers. The
   * previous driver has to be kept alive until the audio stream has been
   * started, because it is responsible to call cubeb_stream_start, so we delay
   * the cleanup of the previous driver until it has started the audio stream.
   * Otherwise, there is a race where we kill the previous driver thread
   * between cubeb_stream_init and cubeb_stream_start,
   * and callbacks after the prefill never get called.
   * This is written on the previous driver's thread (if switching) or main
   * thread (if this driver is the first one).
   * This is read on previous driver's thread (during callbacks from
   * cubeb_stream_init) and the audio thread (when switching away from this
   * driver back to a SystemClockDriver).
   * This is synchronized by the Graph's monitor.
   * */
  bool mStarted;

  struct AutoInCallback
  {
    explicit AutoInCallback(AudioCallbackDriver* aDriver);
    ~AutoInCallback();
    AudioCallbackDriver* mDriver;
  };

  /* Thread for off-main-thread initialization and
   * shutdown of the audio stream. */
  nsCOMPtr<nsIThread> mInitShutdownThread;
  dom::AudioChannel mAudioChannel;
  Atomic<bool> mInCallback;
  /* A thread has been created to be able to pause and restart the audio thread,
   * but has not done so yet. This indicates that the callback should return
   * early */
  bool mPauseRequested;
  /**
   * True if microphone is being used by this process. This is synchronized by
   * the graph's monitor. */
  bool mMicrophoneActive;
};

class AsyncCubebTask : public nsRunnable
{
public:
  enum AsyncCubebOperation {
    INIT,
    SHUTDOWN,
    SLEEP
  };


  AsyncCubebTask(AudioCallbackDriver* aDriver, AsyncCubebOperation aOperation);

  nsresult Dispatch()
  {
    // Can't add 'this' as the event to run, since mThread may not be set yet
    nsresult rv = NS_NewNamedThread("CubebOperation", getter_AddRefs(mThread));
    if (NS_SUCCEEDED(rv)) {
      // Note: event must not null out mThread!
      rv = mThread->Dispatch(this, NS_DISPATCH_NORMAL);
    }
    return rv;
  }

protected:
  virtual ~AsyncCubebTask();

private:
  NS_IMETHOD Run() MOZ_OVERRIDE MOZ_FINAL;
  nsCOMPtr<nsIThread> mThread;
  nsRefPtr<AudioCallbackDriver> mDriver;
  AsyncCubebOperation mOperation;
  nsRefPtr<MediaStreamGraphImpl> mShutdownGrip;
};

}

#endif // GRAPHDRIVER_H_
