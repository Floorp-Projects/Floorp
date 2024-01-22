/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CubebUtils.h"
#include "GraphDriver.h"

#include "gmock/gmock.h"
#include "gtest/gtest-printers.h"
#include "gtest/gtest.h"

#include "MediaTrackGraphImpl.h"
#include "mozilla/gtest/WaitFor.h"
#include "mozilla/Attributes.h"
#include "mozilla/UniquePtr.h"
#include "nsTArray.h"

#include "MockCubeb.h"

using namespace mozilla;
using IterationResult = GraphInterface::IterationResult;
using ::testing::NiceMock;

class MockGraphInterface : public GraphInterface {
  NS_DECL_THREADSAFE_ISUPPORTS
  explicit MockGraphInterface(TrackRate aSampleRate)
      : mSampleRate(aSampleRate) {}
  MOCK_METHOD0(NotifyInputStopped, void());
  MOCK_METHOD5(NotifyInputData, void(const AudioDataValue*, size_t, TrackRate,
                                     uint32_t, uint32_t));
  MOCK_METHOD0(DeviceChanged, void());
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
    GraphDriver* next = mNextDriver.exchange(nullptr);
    if (next) {
      return IterationResult::CreateSwitchDriver(
          next, NS_NewRunnableFunction(__func__, [] {}));
    }
    if (mEnsureNextIteration) {
      driver->EnsureNextIteration();
    }
    return IterationResult::CreateStillProcessing();
  }
  void SetEnsureNextIteration(bool aEnsure) { mEnsureNextIteration = aEnsure; }

#ifdef DEBUG
  bool InDriverIteration(const GraphDriver* aDriver) const override {
    return aDriver->OnThread();
  }
#endif

  size_t IterationCount() const { return mIterationCount; }

  GraphTime StateComputedTime() const { return mStateComputedTime; }
  void SetCurrentDriver(GraphDriver* aDriver) { mCurrentDriver = aDriver; }

  void StopIterating() { mKeepProcessing = false; }

  void SwitchTo(GraphDriver* aDriver) { mNextDriver = aDriver; }
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
  Atomic<GraphDriver*> mNextDriver{nullptr};
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

  auto* mainThread = AbstractThread::GetCurrent();
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

  // This will block untill all events have been executed.
  MOZ_KnownLive(driver)->Shutdown();
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
