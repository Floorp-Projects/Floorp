/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GRAPHDRIVER_H_
#define GRAPHDRIVER_H_

#include "nsAutoRef.h"
#include "AudioBufferUtils.h"
#include "AudioMixer.h"
#include "AudioSegment.h"
#include "SelfRef.h"
#include "mozilla/Atomics.h"
#include "mozilla/dom/AudioContext.h"
#include "mozilla/SharedThreadPool.h"
#include "mozilla/StaticPtr.h"

#include <thread>

#if defined(XP_WIN)
#  include "mozilla/audio/AudioNotificationReceiver.h"
#endif

struct cubeb_stream;

template <>
class nsAutoRefTraits<cubeb_stream> : public nsPointerRefTraits<cubeb_stream> {
 public:
  static void Release(cubeb_stream* aStream) { cubeb_stream_destroy(aStream); }
};

namespace mozilla {

/**
 * Assume we can run an iteration of the MediaTrackGraph loop in this much time
 * or less.
 * We try to run the control loop at this rate.
 */
static const int MEDIA_GRAPH_TARGET_PERIOD_MS = 10;

/**
 * Assume that we might miss our scheduled wakeup of the MediaTrackGraph by
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
static const int AUDIO_TARGET_MS =
    2 * MEDIA_GRAPH_TARGET_PERIOD_MS + SCHEDULE_SAFETY_MARGIN_MS;

class MediaTrack;
class MediaTrackGraphImpl;

class AudioCallbackDriver;
class OfflineClockDriver;
class SystemClockDriver;

namespace dom {
enum class AudioContextOperation;
}

/**
 * XXX
 * Dependencies on mGraphImpl:
 * OK
 * - NotifyInputData
 * - OneIteration
 * - NotifyOutputData
 * - DeviceChanged
 * TRY TO REMOVE
 * - RunByGraphThread (asserts)
 * - SetCurrentDriver
 * - MillisecondsToMediaTime (make static)
 * - SecondsToMediaTime (make static)
 * - SignalMainThreadCleanup (unclear)
 * - mOutputDeviceID, mInputDeviceID (const ctor?)
 * - AudioContextOperationCompleted
 */

/**
 * A driver is responsible for the scheduling of the processing, the thread
 * management, and give the different clocks to a MediaTrackGraph. This is an
 * abstract base class. A MediaTrackGraph can be driven by an
 * OfflineClockDriver, if the graph is offline, or a SystemClockDriver, if the
 * graph is real time.
 * A MediaTrackGraph holds an owning reference to its driver.
 *
 * The lifetime of drivers is a complicated affair. Here are the different
 * scenarii that can happen:
 *
 * Starting a MediaTrackGraph with an AudioCallbackDriver
 * - A new thread T is created, from the main thread.
 * - On this thread T, cubeb is initialized if needed, and a cubeb_stream is
 *   created and started
 * - The thread T posts a message to the main thread to terminate itself.
 * - The graph runs off the audio thread
 *
 * Starting a MediaTrackGraph with a SystemClockDriver:
 * - A new thread T is created from the main thread.
 * - The graph runs off this thread.
 *
 * Switching from a SystemClockDriver to an AudioCallbackDriver:
 * - A new AudioCallabackDriver is created and initialized on the graph thread
 * - At the end of the MTG iteration, the SystemClockDriver transfers its timing
 *   info and a reference to itself to the AudioCallbackDriver. It then starts
 *   the AudioCallbackDriver.
 * - When the AudioCallbackDriver starts, it checks if it has been switched from
 *   a SystemClockDriver, and if that is the case, sends a message to the main
 *   thread to shut the SystemClockDriver thread down.
 * - The graph now runs off an audio callback
 *
 * Switching from an AudioCallbackDriver to a SystemClockDriver:
 * - A new SystemClockDriver is created, and set as mNextDriver.
 * - At the end of the MTG iteration, the AudioCallbackDriver transfers its
 *   timing info and a reference to itself to the SystemClockDriver. A new
 *   SystemClockDriver is started from the current audio thread.
 * - When starting, the SystemClockDriver checks if it has been switched from an
 *   AudioCallbackDriver. If yes, it creates a new temporary thread to release
 *   the cubeb_streams. This temporary thread closes the cubeb_stream, and
 *   then dispatches a message to the main thread to be terminated.
 * - The graph now runs off a normal thread.
 *
 * Two drivers cannot run at the same time for the same graph. The thread safety
 * of the different attributes of drivers, and they access pattern is documented
 * next to the members themselves.
 *
 */
class GraphDriver {
 public:
  GraphDriver(MediaTrackGraphImpl* aGraphImpl, uint32_t aSampleRate);

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(GraphDriver);
  /* Start the graph, init the driver, start the thread.
   * A driver cannot be started twice, it must be shutdown
   * before being started again. */
  virtual void Start() = 0;
  /* Shutdown GraphDriver (synchronously) */
  MOZ_CAN_RUN_SCRIPT virtual void Shutdown() = 0;
  /* Rate at which the GraphDriver runs, in ms. This can either be user
   * controlled (because we are using a {System,Offline}ClockDriver, and decide
   * how often we want to wakeup/how much we want to process per iteration), or
   * it can be indirectly set by the latency of the audio backend, and the
   * number of buffers of this audio backend: say we have four buffers, and 40ms
   * latency, we will get a callback approximately every 10ms. */
  virtual uint32_t IterationDuration() = 0;
  /*
   * Signaled by the graph when it needs another iteration. Goes unhandled for
   * GraphDrivers that are not able to sleep indefinitely (i.e., all drivers but
   * ThreadedDriver). Can be called on any thread.
   */
  virtual void EnsureNextIteration() {}

  /* Return whether we are switching or not. */
  bool Switching();
  /* Implement the switching of the driver and the necessary updates */
  void SwitchToNextDriver();

  // Those are simply or setting the associated pointer, but assert that the
  // lock is held.
  GraphDriver* NextDriver();
  GraphDriver* PreviousDriver();
  void SetPreviousDriver(GraphDriver* aPreviousDriver);

  GraphTime IterationEnd() { return mIterationEnd; }

  virtual AudioCallbackDriver* AsAudioCallbackDriver() { return nullptr; }

  virtual OfflineClockDriver* AsOfflineClockDriver() { return nullptr; }

  virtual SystemClockDriver* AsSystemClockDriver() { return nullptr; }

  /**
   * Tell the driver it has to stop and return the current time of the graph, so
   * another driver can start from the right point in time.
   */
  void SwitchAtNextIteration(GraphDriver* aDriver);

  /**
   * Set the state of the driver so it can start at the right point in time,
   * after switching from another driver.
   */
  void SetState(GraphDriver* aPreviousDriver, GraphTime aIterationStart,
                GraphTime aIterationEnd, GraphTime aStateComputedTime);

  MediaTrackGraphImpl* GraphImpl() const { return mGraphImpl; }

#ifdef DEBUG
  // True if the current thread is driving the MTG.
  bool OnGraphThread();
#endif
  // True if the current thread is the GraphDriver's thread.
  virtual bool OnThread() = 0;
  // GraphDriver's thread has started and the thread is running.
  virtual bool ThreadRunning() = 0;

 protected:
  // Sets the associated pointer, asserting that the lock is held
  void SetNextDriver(GraphDriver* aNextDriver);

  // Time of the start of this graph iteration. This must be accessed while
  // having the monitor.
  GraphTime mIterationStart = 0;
  // Time of the end of this graph iteration. This must be accessed while having
  // the monitor.
  GraphTime mIterationEnd = 0;
  // Time until which the graph has processed data.
  GraphTime mStateComputedTime = 0;
  // The MediaTrackGraphImpl associated with this driver.
  const RefPtr<MediaTrackGraphImpl> mGraphImpl;
  // The sample rate for the graph, and in case of an audio driver, also for the
  // cubeb stream.
  const uint32_t mSampleRate;

  // This is non-null only when this driver has recently switched from an other
  // driver, and has not cleaned it up yet (for example because the audio stream
  // is currently calling the callback during initialization).
  //
  // This is written to when changing driver, from the previous driver's thread,
  // or a thread created for the occasion. This is read each time we need to
  // check whether we're changing driver (in Switching()), from the graph
  // thread.
  // This must be accessed using the {Set,Get}PreviousDriver methods.
  RefPtr<GraphDriver> mPreviousDriver;
  // This is non-null only when this driver is going to switch to an other
  // driver at the end of this iteration.
  // This must be accessed using the {Set,Get}NextDriver methods.
  RefPtr<GraphDriver> mNextDriver;
  virtual ~GraphDriver() = default;
};

class MediaTrackGraphInitThreadRunnable;

/**
 * This class is a driver that manages its own thread.
 */
class ThreadedDriver : public GraphDriver {
  class IterationWaitHelper {
    Monitor mMonitor;
    // The below members are guarded by mMonitor.
    bool mNeedAnotherIteration = false;
    TimeStamp mWakeTime;

   public:
    IterationWaitHelper() : mMonitor("IterationWaitHelper::mMonitor") {}

    /**
     * If another iteration is needed we wait for aDuration, otherwise we wait
     * for a wake-up. If a wake-up occurs before aDuration time has passed, we
     * wait for aDuration nonetheless.
     */
    void WaitForNextIterationAtLeast(TimeDuration aDuration) {
      MonitorAutoLock lock(mMonitor);
      TimeStamp now = TimeStamp::Now();
      mWakeTime = now + aDuration;
      while (true) {
        if (mNeedAnotherIteration && now >= mWakeTime) {
          break;
        }
        if (mNeedAnotherIteration) {
          lock.Wait(mWakeTime - now);
        } else {
          lock.Wait(TimeDuration::Forever());
        }
        now = TimeStamp::Now();
      }
      mWakeTime = TimeStamp();
      mNeedAnotherIteration = false;
    }

    /**
     * Sets mNeedAnotherIteration to true and notifies the monitor, in case a
     * driver is currently waiting.
     */
    void EnsureNextIteration() {
      MonitorAutoLock lock(mMonitor);
      mNeedAnotherIteration = true;
      lock.Notify();
    }
  };

 public:
  ThreadedDriver(MediaTrackGraphImpl* aGraphImpl, uint32_t aSampleRate);
  virtual ~ThreadedDriver();

  void EnsureNextIteration() override;
  void Start() override;
  MOZ_CAN_RUN_SCRIPT void Shutdown() override;
  /**
   * Runs main control loop on the graph thread. Normally a single invocation
   * of this runs for the entire lifetime of the graph thread.
   */
  void RunThread();
  friend class MediaTrackGraphInitThreadRunnable;
  uint32_t IterationDuration() override { return MEDIA_GRAPH_TARGET_PERIOD_MS; }

  nsIThread* Thread() { return mThread; }

  bool OnThread() override {
    return !mThread || mThread->EventTarget()->IsOnCurrentThread();
  }

  bool ThreadRunning() override { return mThreadRunning; }

 protected:
  /* Waits until it's time to process more data. */
  void WaitForNextIteration();
  /* Implementation dependent time the ThreadedDriver should wait between
   * iterations. */
  virtual TimeDuration WaitInterval() = 0;
  /* When the graph wakes up to do an iteration, implementations return the
   * range of time that will be processed.  This is called only once per
   * iteration; it may determine the interval from state in a previous
   * call. */
  virtual MediaTime GetIntervalForIteration() = 0;

  nsCOMPtr<nsIThread> mThread;

 private:
  // This is true if the thread is running. It is false
  // before starting the thread and after stopping it.
  Atomic<bool> mThreadRunning;

  // Any thread.
  IterationWaitHelper mWaitHelper;
};

/**
 * A SystemClockDriver drives a MediaTrackGraph using a system clock, and waits
 * using a monitor, between each iteration.
 */
enum class FallbackMode { Regular, Fallback };
class SystemClockDriver : public ThreadedDriver {
 public:
  SystemClockDriver(MediaTrackGraphImpl* aGraphImpl, uint32_t aSampleRate,
                    FallbackMode aFallback = FallbackMode::Regular);
  virtual ~SystemClockDriver();
  bool IsFallback();
  SystemClockDriver* AsSystemClockDriver() override { return this; }

 protected:
  /* Return the TimeDuration to wait before the next rendering iteration. */
  TimeDuration WaitInterval() override;
  MediaTime GetIntervalForIteration() override;

 private:
  // Those are only modified (after initialization) on the graph thread. The
  // graph thread does not run during the initialization.
  TimeStamp mInitialTimeStamp;
  TimeStamp mCurrentTimeStamp;
  TimeStamp mLastTimeStamp;

  // This is true if this SystemClockDriver runs the graph because we could
  // not open an audio stream.
  const bool mIsFallback;
};

/**
 * An OfflineClockDriver runs the graph as fast as possible, without waiting
 * between iteration.
 */
class OfflineClockDriver : public ThreadedDriver {
 public:
  OfflineClockDriver(MediaTrackGraphImpl* aGraphImpl, uint32_t aSampleRate,
                     GraphTime aSlice);
  virtual ~OfflineClockDriver();
  OfflineClockDriver* AsOfflineClockDriver() override { return this; }

 protected:
  TimeDuration WaitInterval() override { return 0; }
  MediaTime GetIntervalForIteration() override;

 private:
  // Time, in GraphTime, for each iteration
  GraphTime mSlice;
};

struct TrackAndPromiseForOperation {
  TrackAndPromiseForOperation(MediaTrack* aTrack, void* aPromise,
                              dom::AudioContextOperation aOperation,
                              dom::AudioContextOperationFlags aFlags);
  RefPtr<MediaTrack> mTrack;
  void* mPromise;
  dom::AudioContextOperation mOperation;
  dom::AudioContextOperationFlags mFlags;
};

enum class AsyncCubebOperation { INIT, SHUTDOWN };
enum class AudioInputType { Unknown, Voice };

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
#if defined(XP_WIN)
    ,
                            public audio::DeviceChangeListener
#endif
{
 public:
  /** If aInputChannelCount is zero, then this driver is output-only. */
  AudioCallbackDriver(MediaTrackGraphImpl* aGraphImpl, uint32_t aSampleRate,
                      uint32_t aOutputChannelCount, uint32_t aInputChannelCount,
                      AudioInputType aAudioInputType);
  virtual ~AudioCallbackDriver();

  void Start() override;
  MOZ_CAN_RUN_SCRIPT void Shutdown() override;
#if defined(XP_WIN)
  void ResetDefaultDevice() override;
#endif

  /* Static wrapper function cubeb calls back. */
  static long DataCallback_s(cubeb_stream* aStream, void* aUser,
                             const void* aInputBuffer, void* aOutputBuffer,
                             long aFrames);
  static void StateCallback_s(cubeb_stream* aStream, void* aUser,
                              cubeb_state aState);
  static void DeviceChangedCallback_s(void* aUser);
  /* This function is called by the underlying audio backend when a refill is
   * needed. This is what drives the whole graph when it is used to output
   * audio. If the return value is exactly aFrames, this function will get
   * called again. If it is less than aFrames, the stream will go in draining
   * mode, and this function will not be called again. */
  long DataCallback(const AudioDataValue* aInputBuffer,
                    AudioDataValue* aOutputBuffer, long aFrames);
  /* This function is called by the underlying audio backend, but is only used
   * for informational purposes at the moment. */
  void StateCallback(cubeb_state aState);
  /* This is an approximation of the number of millisecond there are between two
   * iterations of the graph. */
  uint32_t IterationDuration() override;

  /* This function gets called when the graph has produced the audio frames for
   * this iteration. */
  void MixerCallback(AudioDataValue* aMixedBuffer, AudioSampleFormat aFormat,
                     uint32_t aChannels, uint32_t aFrames,
                     uint32_t aSampleRate) override;

  AudioCallbackDriver* AsAudioCallbackDriver() override { return this; }

  uint32_t OutputChannelCount() {
    MOZ_ASSERT(mOutputChannels != 0 && mOutputChannels <= 8);
    return mOutputChannels;
  }

  uint32_t InputChannelCount() { return mInputChannelCount; }

  AudioInputType InputDevicePreference() {
    if (mInputDevicePreference == CUBEB_DEVICE_PREF_VOICE) {
      return AudioInputType::Voice;
    }
    return AudioInputType::Unknown;
  }

  /* Enqueue a promise that is going to be resolved when a specific operation
   * occurs on the cubeb stream. */
  void EnqueueTrackAndPromiseForOperation(
      MediaTrack* aTrack, void* aPromise, dom::AudioContextOperation aOperation,
      dom::AudioContextOperationFlags aFlags);

  std::thread::id ThreadId() { return mAudioThreadId.load(); }

  bool OnThread() override {
    return mAudioThreadId.load() == std::this_thread::get_id();
  }

  bool ThreadRunning() override { return mAudioThreadRunning; }

  /* Whether the underlying cubeb stream has been started. See comment for
   * mStarted for details. */
  bool IsStarted();

  void CompleteAudioContextOperations(AsyncCubebOperation aOperation);

  // Returns the output latency for the current audio output stream.
  TimeDuration AudioOutputLatency();

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
  bool StartStream();
  friend class AsyncCubebTask;
  bool Init();
  void Stop();
  /**
   *  Fall back to a SystemClockDriver using a normal thread. If needed,
   *  the graph will try to re-open an audio stream later. */
  void FallbackToSystemClockDriver();

  /* This is true when the method is executed on CubebOperation thread pool. */
  bool OnCubebOperationThread() {
    return mInitShutdownThread->IsOnCurrentThreadInfallible();
  }

  /* MediaTrackGraphs are always down/up mixed to output channels. */
  const uint32_t mOutputChannels;
  /* The size of this buffer comes from the fact that some audio backends can
   * call back with a number of frames lower than one block (128 frames), so we
   * need to keep at most two block in the SpillBuffer, because we always round
   * up to block boundaries during an iteration.
   * This is only ever accessed on the audio callback thread. */
  SpillBuffer<AudioDataValue, WEBAUDIO_BLOCK_SIZE * 2> mScratchBuffer;
  /* Wrapper to ensure we write exactly the number of frames we need in the
   * audio buffer cubeb passes us. This is only ever accessed on the audio
   * callback thread. */
  AudioCallbackBufferWrapper<AudioDataValue> mBuffer;
  /* cubeb stream for this graph. This is guaranteed to be non-null after Init()
   * has been called, and is synchronized internaly. */
  nsAutoRef<cubeb_stream> mAudioStream;
  /* The number of input channels from cubeb. Set before opening cubeb. If it is
   * zero then the driver is output-only. */
  const uint32_t mInputChannelCount;
  /* Approximation of the time between two callbacks. This is used to schedule
   * video frames. This is in milliseconds. Only even used (after
   * inizatialization) on the audio callback thread. */
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
  Atomic<bool> mStarted;

  struct AutoInCallback {
    explicit AutoInCallback(AudioCallbackDriver* aDriver);
    ~AutoInCallback();
    AudioCallbackDriver* mDriver;
  };

  /* Shared thread pool with up to one thread for off-main-thread
   * initialization and shutdown of the audio stream via AsyncCubebTask. */
  const RefPtr<SharedThreadPool> mInitShutdownThread;
  /* This must be accessed with the graph monitor held. */
  AutoTArray<TrackAndPromiseForOperation, 1> mPromisesForOperation;
  cubeb_device_pref mInputDevicePreference;
  /* The mixer that the graph mixes into during an iteration. Audio thread only.
   */
  AudioMixer mMixer;
  /* Contains the id of the audio thread for as long as the callback
   * is taking place, after that it is reseted to an invalid value. */
  std::atomic<std::thread::id> mAudioThreadId;
  /* True when audio thread is running. False before
   * starting and after stopping it the audio thread. */
  Atomic<bool> mAudioThreadRunning;
  /* Indication of whether a fallback SystemClockDriver should be started if
   * StateCallback() receives an error.  No mutex need be held during access.
   * The transition to true happens before cubeb_stream_start() is called.
   * After transitioning to false on the last DataCallback(), the stream is
   * not accessed from another thread until the graph thread either signals
   * main thread cleanup or dispatches an event to switch to another
   * driver. */
  bool mShouldFallbackIfError;
  /* True if this driver was created from a driver created because of a previous
   * AudioCallbackDriver failure. */
  bool mFromFallback;
#ifdef XP_MACOSX
  /* When using the built-in speakers on macbook pro (13 and 15, all models),
   * it's best to hard pan the audio on the right, to avoid feedback into the
   * microphone that is located next to the left speaker.  */
  Atomic<bool> mNeedsPanning;
#endif
};

class AsyncCubebTask : public Runnable {
 public:
  AsyncCubebTask(AudioCallbackDriver* aDriver, AsyncCubebOperation aOperation);

  nsresult Dispatch(uint32_t aFlags = NS_DISPATCH_NORMAL) {
    return mDriver->mInitShutdownThread->Dispatch(this, aFlags);
  }

 protected:
  virtual ~AsyncCubebTask();

 private:
  NS_IMETHOD Run() final;

  RefPtr<AudioCallbackDriver> mDriver;
  AsyncCubebOperation mOperation;
  RefPtr<MediaTrackGraphImpl> mShutdownGrip;
};

}  // namespace mozilla

#endif  // GRAPHDRIVER_H_
