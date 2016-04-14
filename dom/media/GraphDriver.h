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
#include "SelfRef.h"
#include "mozilla/Atomics.h"
#include "mozilla/SharedThreadPool.h"
#include "mozilla/StaticPtr.h"

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

class MediaStreamGraphImpl;

class AudioCallbackDriver;
class OfflineClockDriver;

/**
 * A driver is responsible for the scheduling of the processing, the thread
 * management, and give the different clocks to a MediaStreamGraph. This is an
 * abstract base class. A MediaStreamGraph can be driven by an
 * OfflineClockDriver, if the graph is offline, or a SystemClockDriver, if the
 * graph is real time.
 * A MediaStreamGraph holds an owning reference to its driver.
 *
 * The lifetime of drivers is a complicated affair. Here are the different
 * scenarii that can happen:
 *
 * Starting a MediaStreamGraph with an AudioCallbackDriver
 * - A new thread T is created, from the main thread.
 * - On this thread T, cubeb is initialized if needed, and a cubeb_stream is
 *   created and started
 * - The thread T posts a message to the main thread to terminate itself.
 * - The graph runs off the audio thread
 *
 * Starting a MediaStreamGraph with a SystemClockDriver:
 * - A new thread T is created from the main thread.
 * - The graph runs off this thread.
 *
 * Switching from a SystemClockDriver to an AudioCallbackDriver:
 * - A new AudioCallabackDriver is created and initialized on the graph thread
 * - At the end of the MSG iteration, the SystemClockDriver transfers its timing
 *   info and a reference to itself to the AudioCallbackDriver. It then starts
 *   the AudioCallbackDriver.
 * - When the AudioCallbackDriver starts, it checks if it has been switched from
 *   a SystemClockDriver, and if that is the case, sends a message to the main
 *   thread to shut the SystemClockDriver thread down.
 * - The graph now runs off an audio callback
 *
 * Switching from an AudioCallbackDriver to a SystemClockDriver:
 * - A new SystemClockDriver is created, and set as mNextDriver.
 * - At the end of the MSG iteration, the AudioCallbackDriver transfers its
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
class GraphDriver
{
public:
  explicit GraphDriver(MediaStreamGraphImpl* aGraphImpl);

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(GraphDriver);
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
  /* Remove Mixer callbacks when switching */
  virtual void RemoveCallback() = 0;
  /* Shutdown GraphDriver (synchronously) */
  void Shutdown();
  /* Rate at which the GraphDriver runs, in ms. This can either be user
   * controlled (because we are using a {System,Offline}ClockDriver, and decide
   * how often we want to wakeup/how much we want to process per iteration), or
   * it can be indirectly set by the latency of the audio backend, and the
   * number of buffers of this audio backend: say we have four buffers, and 40ms
   * latency, we will get a callback approximately every 10ms. */
  virtual uint32_t IterationDuration() = 0;

  /* Return whether we are switching or not. */
  bool Switching();

  // Those are simply or setting the associated pointer, but assert that the
  // lock is held.
  GraphDriver* NextDriver();
  GraphDriver* PreviousDriver();
  void SetNextDriver(GraphDriver* aNextDriver);
  void SetPreviousDriver(GraphDriver* aPreviousDriver);

  /**
   * If we are running a real time graph, get the current time stamp to schedule
   * video frames. This has to be reimplemented by real time drivers.
   */
  virtual TimeStamp GetCurrentTimeStamp() {
    return mCurrentTimeStamp;
  }

  GraphTime IterationEnd() {
    return mIterationEnd;
  }

  virtual AudioCallbackDriver* AsAudioCallbackDriver() {
    return nullptr;
  }

  virtual OfflineClockDriver* AsOfflineClockDriver() {
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
                    GraphTime aLastSwitchNextIterationEnd);
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
  GraphTime StateComputedTime() const;

  // Time of the start of this graph iteration. This must be accessed while
  // having the monitor.
  GraphTime mIterationStart;
  // Time of the end of this graph iteration. This must be accessed while having
  // the monitor.
  GraphTime mIterationEnd;
  // The MediaStreamGraphImpl that owns this driver. This has a lifetime longer
  // than the driver, and will never be null. Hence, it can be accesed without
  // monitor.
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
  // This must be access with the monitor.
  WaitState mWaitState;

  // This is used on the main thread (during initialization), and the graph
  // thread. No monitor needed because we know the graph thread does not run
  // during the initialization.
  TimeStamp mCurrentTimeStamp;
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
  void Start() override;
  void Stop() override;
  void Resume() override;
  void Revive() override;
  void RemoveCallback() override;
  /**
   * Runs main control loop on the graph thread. Normally a single invocation
   * of this runs for the entire lifetime of the graph thread.
   */
  void RunThread();
  friend class MediaStreamGraphInitThreadRunnable;
  uint32_t IterationDuration() override {
    return MEDIA_GRAPH_TARGET_PERIOD_MS;
  }

  bool OnThread() override { return !mThread || NS_GetCurrentThread() == mThread; }

  /* When the graph wakes up to do an iteration, implementations return the
   * range of time that will be processed.  This is called only once per
   * iteration; it may determine the interval from state in a previous
   * call. */
  virtual MediaTime GetIntervalForIteration() = 0;
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
  MediaTime GetIntervalForIteration() override;
  void WaitForNextIteration() override;
  void WakeUp() override;


private:
  // Those are only modified (after initialization) on the graph thread. The
  // graph thread does not run during the initialization.
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
  MediaTime GetIntervalForIteration() override;
  void WaitForNextIteration() override;
  void WakeUp() override;
  TimeStamp GetCurrentTimeStamp() override;
  OfflineClockDriver* AsOfflineClockDriver() override {
    return this;
  }

private:
  // Time, in GraphTime, for each iteration
  GraphTime mSlice;
};

struct StreamAndPromiseForOperation
{
  StreamAndPromiseForOperation(MediaStream* aStream,
                               void* aPromise,
                               dom::AudioContextOperation aOperation);
  RefPtr<MediaStream> mStream;
  void* mPromise;
  dom::AudioContextOperation mOperation;
};

enum AsyncCubebOperation {
  INIT,
  SHUTDOWN
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
  explicit AudioCallbackDriver(MediaStreamGraphImpl* aGraphImpl);
  virtual ~AudioCallbackDriver();

  void Destroy() override;
  void Start() override;
  void Stop() override;
  void Resume() override;
  void Revive() override;
  void RemoveCallback() override;
  void WaitForNextIteration() override;
  void WakeUp() override;

  /* Static wrapper function cubeb calls back. */
  static long DataCallback_s(cubeb_stream * aStream,
                             void * aUser,
                             const void * aInputBuffer,
                             void * aOutputBuffer,
                             long aFrames);
  static void StateCallback_s(cubeb_stream* aStream, void * aUser,
                              cubeb_state aState);
  static void DeviceChangedCallback_s(void * aUser);
  /* This function is called by the underlying audio backend when a refill is
   * needed. This is what drives the whole graph when it is used to output
   * audio. If the return value is exactly aFrames, this function will get
   * called again. If it is less than aFrames, the stream will go in draining
   * mode, and this function will not be called again. */
  long DataCallback(const AudioDataValue* aInputBuffer, AudioDataValue* aOutputBuffer, long aFrames);
  /* This function is called by the underlying audio backend, but is only used
   * for informational purposes at the moment. */
  void StateCallback(cubeb_state aState);
  /* This is an approximation of the number of millisecond there are between two
   * iterations of the graph. */
  uint32_t IterationDuration() override;

  /* This function gets called when the graph has produced the audio frames for
   * this iteration. */
  void MixerCallback(AudioDataValue* aMixedBuffer,
                     AudioSampleFormat aFormat,
                     uint32_t aChannels,
                     uint32_t aFrames,
                     uint32_t aSampleRate) override;

  // These are invoked on the MSG thread (we don't call this if not LIFECYCLE_RUNNING)
  virtual void SetInputListener(AudioDataListener *aListener) {
    MOZ_ASSERT(OnThread());
    mAudioInput = aListener;
  }
  // XXX do we need the param?  probably no
  virtual void RemoveInputListener(AudioDataListener *aListener) {
    MOZ_ASSERT(OnThread());
    mAudioInput = nullptr;
  }

  AudioCallbackDriver* AsAudioCallbackDriver() override {
    return this;
  }

  /* Enqueue a promise that is going to be resolved when a specific operation
   * occurs on the cubeb stream. */
  void EnqueueStreamAndPromiseForOperation(MediaStream* aStream,
                                         void* aPromise,
                                         dom::AudioContextOperation aOperation);

  bool IsSwitchingDevice() {
#ifdef XP_MACOSX
    return mSelfReference;
#else
    return false;
#endif
  }

  /**
   * Whether the audio callback is processing. This is for asserting only.
   */
  bool InCallback();

  bool OnThread() override { return !mStarted || InCallback(); }

  /* Whether the underlying cubeb stream has been started. See comment for
   * mStarted for details. */
  bool IsStarted();

  /* Tell the driver whether this process is using a microphone or not. This is
   * thread safe. */
  void SetMicrophoneActive(bool aActive);

  void CompleteAudioContextOperations(AsyncCubebOperation aOperation);
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
   * up to block boundaries during an iteration.
   * This is only ever accessed on the audio callback thread. */
  SpillBuffer<AudioDataValue, WEBAUDIO_BLOCK_SIZE * 2, ChannelCount> mScratchBuffer;
  /* Wrapper to ensure we write exactly the number of frames we need in the
   * audio buffer cubeb passes us. This is only ever accessed on the audio
   * callback thread. */
  AudioCallbackBufferWrapper<AudioDataValue, ChannelCount> mBuffer;
  /* cubeb stream for this graph. This is guaranteed to be non-null after Init()
   * has been called, and is synchronized internaly. */
  nsAutoRef<cubeb_stream> mAudioStream;
  /* The sample rate for the aforementionned cubeb stream. This is set on
   * initialization and can be read safely afterwards. */
  uint32_t mSampleRate;
  /* The number of input channels from cubeb.  Should be set before opening cubeb
   * and then be static. */
  uint32_t mInputChannels;
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
  bool mStarted;
  /* Listener for mic input, if any. */
  RefPtr<AudioDataListener> mAudioInput;

  struct AutoInCallback
  {
    explicit AutoInCallback(AudioCallbackDriver* aDriver);
    ~AutoInCallback();
    AudioCallbackDriver* mDriver;
  };

  /* Thread for off-main-thread initialization and
   * shutdown of the audio stream. */
  nsCOMPtr<nsIThread> mInitShutdownThread;
  /* This must be accessed with the graph monitor held. */
  AutoTArray<StreamAndPromiseForOperation, 1> mPromisesForOperation;
  /* This is set during initialization, and can be read safely afterwards. */
  dom::AudioChannel mAudioChannel;
  /* Used to queue us to add the mixer callback on first run. */
  bool mAddedMixer;

  /* This is atomic and is set by the audio callback thread. It can be read by
   * any thread safely. */
  Atomic<bool> mInCallback;

#ifdef XP_MACOSX
  /**
   * True if microphone is being used by this process. This is synchronized by
   * the graph's monitor. */
  bool mMicrophoneActive;

  /* Implements the workaround for the osx audio stack when changing output
   * devices. See comments in .cpp */
  bool OSXDeviceSwitchingWorkaround();
  /* Self-reference that keep this driver alive when switching output audio
   * device and making the graph running temporarily off a SystemClockDriver.  */
  SelfReference<AudioCallbackDriver> mSelfReference;
  /* While switching devices, we keep track of the number of callbacks received,
   * since OSX seems to still call us _sometimes_. */
  uint32_t mCallbackReceivedWhileSwitching;
#endif
};

class AsyncCubebTask : public nsRunnable
{
public:

  AsyncCubebTask(AudioCallbackDriver* aDriver, AsyncCubebOperation aOperation);

  nsresult Dispatch(uint32_t aFlags = NS_DISPATCH_NORMAL)
  {
    nsresult rv = EnsureThread();
    if (!NS_FAILED(rv)) {
      rv = sThreadPool->Dispatch(this, aFlags);
    }
    return rv;
  }

protected:
  virtual ~AsyncCubebTask();

private:
  static nsresult EnsureThread();

  NS_IMETHOD Run() override final;
  static StaticRefPtr<nsIThreadPool> sThreadPool;
  RefPtr<AudioCallbackDriver> mDriver;
  AsyncCubebOperation mOperation;
  RefPtr<MediaStreamGraphImpl> mShutdownGrip;
};

} // namespace mozilla

#endif // GRAPHDRIVER_H_
