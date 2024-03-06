/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <tuple>

#include "CubebUtils.h"
#include "GraphDriver.h"

#include "gmock/gmock.h"
#include "gtest/gtest-printers.h"
#include "gtest/gtest.h"

#include "MediaTrackGraphImpl.h"
#include "mozilla/gtest/WaitFor.h"
#include "mozilla/Attributes.h"
#include "mozilla/SyncRunnable.h"
#include "mozilla/UniquePtr.h"
#include "nsTArray.h"

#include "MockCubeb.h"

using namespace mozilla;
using IterationResult = GraphInterface::IterationResult;
using ::testing::_;
using ::testing::AnyNumber;
using ::testing::NiceMock;

class MockGraphInterface : public GraphInterface {
  NS_DECL_THREADSAFE_ISUPPORTS
  explicit MockGraphInterface(TrackRate aSampleRate)
      : mSampleRate(aSampleRate) {}
  MOCK_METHOD0(NotifyInputStopped, void());
  MOCK_METHOD5(NotifyInputData, void(const AudioDataValue*, size_t, TrackRate,
                                     uint32_t, uint32_t));
  MOCK_METHOD0(DeviceChanged, void());
#ifdef DEBUG
  MOCK_CONST_METHOD1(InDriverIteration, bool(const GraphDriver*));
#endif
  /* OneIteration cannot be mocked because IterationResult is non-memmovable and
   * cannot be passed as a parameter, which GMock does internally. */
  IterationResult OneIteration(GraphTime aStateComputedTime, GraphTime,
                               MixerCallbackReceiver* aMixerReceiver) {
    GraphDriver* driver = mCurrentDriver;
    if (aMixerReceiver) {
      mMixer.StartMixing();
      mMixer.Mix(nullptr, driver->AsAudioCallbackDriver()->OutputChannelCount(),
                 aStateComputedTime - mStateComputedTime, mSampleRate);
      aMixerReceiver->MixerCallback(mMixer.MixedChunk(), mSampleRate);
    }
    if (aStateComputedTime != mStateComputedTime) {
      mFramesIteratedEvent.Notify(aStateComputedTime - mStateComputedTime);
      ++mIterationCount;
    }
    mStateComputedTime = aStateComputedTime;
    if (!mKeepProcessing) {
      return IterationResult::CreateStop(
          NS_NewRunnableFunction(__func__, [] {}));
    }
    if (auto guard = mNextDriver.Lock(); guard->isSome()) {
      auto tup = guard->extract();
      const auto& [driver, switchedRunnable] = tup;
      return IterationResult::CreateSwitchDriver(driver, switchedRunnable);
    }
    if (mEnsureNextIteration) {
      driver->EnsureNextIteration();
    }
    return IterationResult::CreateStillProcessing();
  }
  void SetEnsureNextIteration(bool aEnsure) { mEnsureNextIteration = aEnsure; }

  size_t IterationCount() const { return mIterationCount; }

  GraphTime StateComputedTime() const { return mStateComputedTime; }
  void SetCurrentDriver(GraphDriver* aDriver) { mCurrentDriver = aDriver; }

  void StopIterating() { mKeepProcessing = false; }

  void SwitchTo(RefPtr<GraphDriver> aDriver,
                RefPtr<Runnable> aSwitchedRunnable = NS_NewRunnableFunction(
                    "DefaultNoopSwitchedRunnable", [] {})) {
    auto guard = mNextDriver.Lock();
    MOZ_ASSERT(guard->isNothing());
    *guard =
        Some(std::make_tuple(std::move(aDriver), std::move(aSwitchedRunnable)));
  }
  const TrackRate mSampleRate;

  MediaEventSource<uint32_t>& FramesIteratedEvent() {
    return mFramesIteratedEvent;
  }

 protected:
  Atomic<size_t> mIterationCount{0};
  Atomic<GraphTime> mStateComputedTime{0};
  Atomic<GraphDriver*> mCurrentDriver{nullptr};
  Atomic<bool> mEnsureNextIteration{false};
  Atomic<bool> mKeepProcessing{true};
  DataMutex<Maybe<std::tuple<RefPtr<GraphDriver>, RefPtr<Runnable>>>>
      mNextDriver{"MockGraphInterface::mNextDriver"};
  RefPtr<Runnable> mNextDriverSwitchedRunnable;
  MediaEventProducer<uint32_t> mFramesIteratedEvent;
  AudioMixer mMixer;
  virtual ~MockGraphInterface() = default;
};

NS_IMPL_ISUPPORTS0(MockGraphInterface)

TEST(TestAudioCallbackDriver, StartStop)
MOZ_CAN_RUN_SCRIPT_FOR_DEFINITION {
  const TrackRate rate = 44100;
  MockCubeb* cubeb = new MockCubeb();
  CubebUtils::ForceSetCubebContext(cubeb->AsCubebContext());

  RefPtr<AudioCallbackDriver> driver;
  auto graph = MakeRefPtr<NiceMock<MockGraphInterface>>(rate);
  EXPECT_CALL(*graph, NotifyInputStopped).Times(0);

  driver = MakeRefPtr<AudioCallbackDriver>(graph, nullptr, rate, 2, 0, nullptr,
                                           nullptr, AudioInputType::Unknown);
  EXPECT_FALSE(driver->ThreadRunning()) << "Verify thread is not running";
  EXPECT_FALSE(driver->IsStarted()) << "Verify thread is not started";

  graph->SetCurrentDriver(driver);
  driver->Start();
  // Allow some time to "play" audio.
  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  EXPECT_TRUE(driver->ThreadRunning()) << "Verify thread is running";
  EXPECT_TRUE(driver->IsStarted()) << "Verify thread is started";

  // This will block untill all events have been executed.
  MOZ_KnownLive(driver)->Shutdown();
  EXPECT_FALSE(driver->ThreadRunning()) << "Verify thread is not running";
  EXPECT_FALSE(driver->IsStarted()) << "Verify thread is not started";
}

void TestSlowStart(const TrackRate aRate) MOZ_CAN_RUN_SCRIPT_FOR_DEFINITION {
  std::cerr << "TestSlowStart with rate " << aRate << std::endl;

  MockCubeb* cubeb = new MockCubeb();
  cubeb->SetStreamStartFreezeEnabled(true);
  auto unforcer = WaitFor(cubeb->ForceAudioThread()).unwrap();
  Unused << unforcer;
  CubebUtils::ForceSetCubebContext(cubeb->AsCubebContext());

  RefPtr<AudioCallbackDriver> driver;
  auto graph = MakeRefPtr<NiceMock<MockGraphInterface>>(aRate);
  EXPECT_CALL(*graph, NotifyInputStopped).Times(0);

  nsIThread* mainThread = NS_GetCurrentThread();
  Maybe<int64_t> audioStart;
  Maybe<uint32_t> alreadyBuffered;
  int64_t inputFrameCount = 0;
  int64_t processedFrameCount = -1;
  ON_CALL(*graph, NotifyInputData)
      .WillByDefault([&](const AudioDataValue*, size_t aFrames, TrackRate,
                         uint32_t, uint32_t aAlreadyBuffered) {
        if (!audioStart) {
          audioStart = Some(graph->StateComputedTime());
          alreadyBuffered = Some(aAlreadyBuffered);
          mainThread->Dispatch(NS_NewRunnableFunction(__func__, [&] {
            // Start processedFrameCount now, ignoring frames processed while
            // waiting for the fallback driver to stop.
            processedFrameCount = 0;
          }));
        }
        EXPECT_NEAR(inputFrameCount,
                    static_cast<int64_t>(graph->StateComputedTime() -
                                         *audioStart + *alreadyBuffered),
                    WEBAUDIO_BLOCK_SIZE)
            << "Input should be behind state time, due to the delayed start. "
               "stateComputedTime="
            << graph->StateComputedTime() << ", audioStartTime=" << *audioStart
            << ", alreadyBuffered=" << *alreadyBuffered;
        inputFrameCount += aFrames;
      });

  driver = MakeRefPtr<AudioCallbackDriver>(graph, nullptr, aRate, 2, 2, nullptr,
                                           (void*)1, AudioInputType::Voice);
  EXPECT_FALSE(driver->ThreadRunning()) << "Verify thread is not running";
  EXPECT_FALSE(driver->IsStarted()) << "Verify thread is not started";

  graph->SetCurrentDriver(driver);
  graph->SetEnsureNextIteration(true);

  driver->Start();
  RefPtr<SmartMockCubebStream> stream = WaitFor(cubeb->StreamInitEvent());
  cubeb->SetStreamStartFreezeEnabled(false);

  const size_t fallbackIterations = 3;
  WaitUntil(graph->FramesIteratedEvent(), [&](uint32_t aFrames) {
    const GraphTime tenMillis = aRate / 100;
    // An iteration is always rounded upwards to the next full block.
    const GraphTime tenMillisIteration =
        MediaTrackGraphImpl::RoundUpToEndOfAudioBlock(tenMillis);
    // The iteration may be smaller because up to an extra block may have been
    // processed and buffered.
    const GraphTime tenMillisMinIteration =
        tenMillisIteration - WEBAUDIO_BLOCK_SIZE;
    // An iteration must be at least one audio block.
    const GraphTime minIteration =
        std::max<GraphTime>(WEBAUDIO_BLOCK_SIZE, tenMillisMinIteration);
    EXPECT_GE(aFrames, minIteration)
        << "Fallback driver iteration >= 10ms, modulo an audio block";
    EXPECT_LT(aFrames, static_cast<size_t>(aRate))
        << "Fallback driver iteration <1s (sanity)";
    return graph->IterationCount() >= fallbackIterations;
  });

  MediaEventListener processedListener =
      stream->FramesProcessedEvent().Connect(mainThread, [&](uint32_t aFrames) {
        if (processedFrameCount >= 0) {
          processedFrameCount += aFrames;
        }
      });
  stream->Thaw();

  SpinEventLoopUntil(
      "processed at least 100ms of audio data from stream callback"_ns,
      [&] { return processedFrameCount >= aRate / 10; });

  // This will block until all events have been queued.
  MOZ_KnownLive(driver)->Shutdown();
  // Process processListener events.
  NS_ProcessPendingEvents(mainThread);
  processedListener.Disconnect();

  EXPECT_EQ(inputFrameCount, processedFrameCount);
  EXPECT_NEAR(graph->StateComputedTime() - *audioStart,
              inputFrameCount + *alreadyBuffered, WEBAUDIO_BLOCK_SIZE)
      << "Graph progresses while audio driver runs. stateComputedTime="
      << graph->StateComputedTime() << ", inputFrameCount=" << inputFrameCount;
}

TEST(TestAudioCallbackDriver, SlowStart)
MOZ_CAN_RUN_SCRIPT_FOR_DEFINITION {
  TestSlowStart(1000);   // 10ms = 10 <<< 128 samples
  TestSlowStart(8000);   // 10ms = 80  <  128 samples
  TestSlowStart(44100);  // 10ms = 441 >  128 samples
}

#ifdef DEBUG
template <typename T>
class MOZ_STACK_CLASS AutoSetter {
  std::atomic<T>& mVal;
  T mNew;
  T mOld;

 public:
  explicit AutoSetter(std::atomic<T>& aVal, T aNew)
      : mVal(aVal), mNew(aNew), mOld(mVal.exchange(aNew)) {}
  ~AutoSetter() {
    DebugOnly<T> oldNew = mVal.exchange(mOld);
    MOZ_ASSERT(oldNew == mNew);
  }
};
#endif

TEST(TestAudioCallbackDriver, SlowDeviceChange)
MOZ_CAN_RUN_SCRIPT_BOUNDARY {
  constexpr TrackRate rate = 48000;
  MockCubeb* cubeb = new MockCubeb(MockCubeb::RunningMode::Manual);
  CubebUtils::ForceSetCubebContext(cubeb->AsCubebContext());

  auto graph = MakeRefPtr<MockGraphInterface>(rate);
  auto driver = MakeRefPtr<AudioCallbackDriver>(
      graph, nullptr, rate, 2, 1, nullptr, (void*)1, AudioInputType::Voice);
  EXPECT_FALSE(driver->ThreadRunning()) << "Verify thread is not running";
  EXPECT_FALSE(driver->IsStarted()) << "Verify thread is not started";

#ifdef DEBUG
  std::atomic<std::thread::id> threadInDriverIteration((std::thread::id()));
  EXPECT_CALL(*graph, InDriverIteration(driver.get())).WillRepeatedly([&] {
    return std::this_thread::get_id() == threadInDriverIteration;
  });
#endif
  constexpr size_t ignoredFrameCount = 1337;
  EXPECT_CALL(*graph, NotifyInputData(_, 0, rate, 1, _)).Times(AnyNumber());
  EXPECT_CALL(*graph, NotifyInputData(_, ignoredFrameCount, _, _, _)).Times(0);
  EXPECT_CALL(*graph, DeviceChanged);

  graph->SetCurrentDriver(driver);
  graph->SetEnsureNextIteration(true);
  // This starts the fallback driver.
  driver->Start();
  RefPtr<SmartMockCubebStream> stream = WaitFor(cubeb->StreamInitEvent());

  // Wait for the audio driver to have started the stream before running data
  // callbacks. driver->Start() does a dispatch to the cubeb operation thread
  // and starts the stream there.
  nsCOMPtr<nsIEventTarget> cubebOpThread = CUBEB_TASK_THREAD;
  MOZ_ALWAYS_SUCCEEDS(SyncRunnable::DispatchToThread(
      cubebOpThread, NS_NewRunnableFunction(__func__, [] {})));

  // This makes the fallback driver stop on its next callback.
  EXPECT_EQ(stream->ManualDataCallback(0),
            MockCubebStream::KeepProcessing::Yes);
  {
#ifdef DEBUG
    AutoSetter as(threadInDriverIteration, std::this_thread::get_id());
#endif
    while (driver->OnFallback()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  }

  const TimeStamp wallClockStart = TimeStamp::Now();
  const GraphTime graphClockStart = graph->StateComputedTime();
  const size_t iterationCountStart = graph->IterationCount();

  // Flag that the stream should force a devicechange event.
  stream->NotifyDeviceChangedNow();

  // The audio driver should now have switched on the fallback driver again.
  {
#ifdef DEBUG
    AutoSetter as(threadInDriverIteration, std::this_thread::get_id());
#endif
    EXPECT_TRUE(driver->OnFallback());
  }

  // Make sure that the audio driver can handle (and ignore) data callbacks for
  // a little while after the devicechange callback. Cubeb does not provide
  // ordering guarantees here.
  auto start = TimeStamp::Now();
  while (start + TimeDuration::FromMilliseconds(5) > TimeStamp::Now()) {
    EXPECT_EQ(stream->ManualDataCallback(ignoredFrameCount),
              MockCubebStream::KeepProcessing::Yes);
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }

  // Let the fallback driver start and spin for one second.
  std::this_thread::sleep_for(std::chrono::seconds(1));

  // Tell the fallback driver to hand over to the audio driver which has
  // finished changing devices.
  EXPECT_EQ(stream->ManualDataCallback(0),
            MockCubebStream::KeepProcessing::Yes);

  // Wait for the fallback to stop.
  {
#ifdef DEBUG
    AutoSetter as(threadInDriverIteration, std::this_thread::get_id());
#endif
    while (driver->OnFallback()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  }

  TimeStamp wallClockEnd = TimeStamp::Now();
  GraphTime graphClockEnd = graph->StateComputedTime();
  size_t iterationCountEnd = graph->IterationCount();

  auto wallClockDuration =
      media::TimeUnit::FromTimeDuration(wallClockEnd - wallClockStart);
  auto graphClockDuration =
      media::TimeUnit(CheckedInt64(graphClockEnd) - graphClockStart, rate);

  // Check that the time while we switched devices was accounted for by the
  // fallback driver.
  EXPECT_NEAR(
      wallClockDuration.ToSeconds(), graphClockDuration.ToSeconds(),
#ifdef XP_MACOSX
      // SystemClockDriver on macOS in CI is underrunning, i.e. the driver
      // thread when waiting for the next iteration waits too long. Therefore
      // the graph clock is unable to keep up with wall clock.
      wallClockDuration.ToSeconds() * 0.8
#else
      0.1
#endif
  );
  // Check that each fallback driver was of reasonable cadence. It's a thread
  // that tries to run a task every 10ms. Check that the average callback
  // interval i falls in 8ms ≤ i ≤ 40ms.
  auto fallbackCadence =
      graphClockDuration /
      static_cast<int64_t>(iterationCountEnd - iterationCountStart);
  EXPECT_LE(8, fallbackCadence.ToMilliseconds());
  EXPECT_LE(fallbackCadence.ToMilliseconds(), 40.0);

  // This will block until all events have been queued.
  MOZ_KnownLive(driver)->Shutdown();
  // Drain the event queue.
  NS_ProcessPendingEvents(nullptr);
}

TEST(TestAudioCallbackDriver, DeviceChangeAfterStop)
MOZ_CAN_RUN_SCRIPT_BOUNDARY {
  constexpr TrackRate rate = 48000;
  MockCubeb* cubeb = new MockCubeb(MockCubeb::RunningMode::Manual);
  CubebUtils::ForceSetCubebContext(cubeb->AsCubebContext());

  auto graph = MakeRefPtr<MockGraphInterface>(rate);
  auto driver = MakeRefPtr<AudioCallbackDriver>(
      graph, nullptr, rate, 2, 1, nullptr, (void*)1, AudioInputType::Voice);
  EXPECT_FALSE(driver->ThreadRunning()) << "Verify thread is not running";
  EXPECT_FALSE(driver->IsStarted()) << "Verify thread is not started";

  auto newDriver = MakeRefPtr<AudioCallbackDriver>(
      graph, nullptr, rate, 2, 1, nullptr, (void*)1, AudioInputType::Voice);
  EXPECT_FALSE(newDriver->ThreadRunning()) << "Verify thread is not running";
  EXPECT_FALSE(newDriver->IsStarted()) << "Verify thread is not started";

#ifdef DEBUG
  std::atomic<std::thread::id> threadInDriverIteration(
      (std::this_thread::get_id()));
  EXPECT_CALL(*graph, InDriverIteration(_)).WillRepeatedly([&] {
    return std::this_thread::get_id() == threadInDriverIteration;
  });
#endif
  EXPECT_CALL(*graph, NotifyInputData(_, 0, rate, 1, _)).Times(AnyNumber());
  EXPECT_CALL(*graph, DeviceChanged);

  graph->SetCurrentDriver(driver);
  graph->SetEnsureNextIteration(true);
  // This starts the fallback driver.
  driver->Start();
  RefPtr<SmartMockCubebStream> stream = WaitFor(cubeb->StreamInitEvent());

  // Wait for the audio driver to have started or the DeviceChanged event will
  // be ignored. driver->Start() does a dispatch to the cubeb operation thread
  // and starts the stream there.
  nsCOMPtr<nsIEventTarget> cubebOpThread = CUBEB_TASK_THREAD;
  MOZ_ALWAYS_SUCCEEDS(SyncRunnable::DispatchToThread(
      cubebOpThread, NS_NewRunnableFunction(__func__, [] {})));

#ifdef DEBUG
  AutoSetter as(threadInDriverIteration, std::this_thread::get_id());
#endif

  // This marks the audio driver as running.
  EXPECT_EQ(stream->ManualDataCallback(0),
            MockCubebStream::KeepProcessing::Yes);

  // If a fallback driver callback happens between the audio callback above, and
  // the SwitchTo below, the audio driver will perform the switch instead of the
  // fallback since the fallback will have stopped. This test may therefore
  // intermittently take different code paths.

  // Stop the fallback driver by switching audio driver in the graph.
  {
    Monitor mon(__func__);
    MonitorAutoLock lock(mon);
    bool switched = false;
    graph->SwitchTo(newDriver, NS_NewRunnableFunction(__func__, [&] {
                      MonitorAutoLock lock(mon);
                      switched = true;
                      lock.Notify();
                    }));
    while (!switched) {
      lock.Wait();
    }
  }

  {
#ifdef DEBUG
    AutoSetter as(threadInDriverIteration, std::thread::id());
#endif
    // After stopping the fallback driver, but before newDriver has stopped the
    // old audio driver, fire a DeviceChanged event to ensure it is handled
    // properly.
    AudioCallbackDriver::DeviceChangedCallback_s(driver);
  }

  graph->StopIterating();
  newDriver->EnsureNextIteration();
  while (newDriver->OnFallback()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }

  // This will block until all events have been queued.
  MOZ_KnownLive(driver)->Shutdown();
  MOZ_KnownLive(newDriver)->Shutdown();
  // Drain the event queue.
  NS_ProcessPendingEvents(nullptr);
}
