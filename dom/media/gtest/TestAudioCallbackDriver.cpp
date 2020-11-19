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
#include "mozilla/Attributes.h"
#include "mozilla/UniquePtr.h"
#include "nsTArray.h"

#include "MockCubeb.h"
#include "WaitFor.h"

using namespace mozilla;
using IterationResult = GraphInterface::IterationResult;
using ::testing::NiceMock;

class MockGraphInterface : public GraphInterface {
  NS_DECL_THREADSAFE_ISUPPORTS
  explicit MockGraphInterface(TrackRate aSampleRate)
      : mSampleRate(aSampleRate) {}
  MOCK_METHOD4(NotifyOutputData,
               void(AudioDataValue*, size_t, TrackRate, uint32_t));
  MOCK_METHOD0(NotifyInputStopped, void());
  MOCK_METHOD5(NotifyInputData, void(const AudioDataValue*, size_t, TrackRate,
                                     uint32_t, uint32_t));
  MOCK_METHOD0(DeviceChanged, void());
  /* OneIteration cannot be mocked because IterationResult is non-memmovable and
   * cannot be passed as a parameter, which GMock does internally. */
  IterationResult OneIteration(GraphTime aStateComputedTime, GraphTime,
                               AudioMixer* aMixer) {
    GraphDriver* driver = mCurrentDriver;
    if (aMixer) {
      aMixer->StartMixing();
      aMixer->Mix(nullptr,
                  driver->AsAudioCallbackDriver()->OutputChannelCount(),
                  aStateComputedTime - mStateComputedTime, mSampleRate);
      aMixer->FinishMixing();
    }
    if (aStateComputedTime != mStateComputedTime) {
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

 protected:
  Atomic<size_t> mIterationCount{0};
  Atomic<GraphTime> mStateComputedTime{0};
  Atomic<GraphDriver*> mCurrentDriver{nullptr};
  Atomic<bool> mEnsureNextIteration{false};
  Atomic<bool> mKeepProcessing{true};
  Atomic<GraphDriver*> mNextDriver{nullptr};
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
  ON_CALL(*graph, NotifyOutputData)
      .WillByDefault([&](AudioDataValue*, size_t, TrackRate, uint32_t) {});

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
  graph->StopIterating();
  MOZ_KnownLive(driver)->Shutdown();
  EXPECT_FALSE(driver->ThreadRunning()) << "Verify thread is not running";
  EXPECT_FALSE(driver->IsStarted()) << "Verify thread is not started";
}

void TestSlowStart(const TrackRate aRate) MOZ_CAN_RUN_SCRIPT_FOR_DEFINITION {
  std::cerr << "TestSlowStart with rate " << aRate << std::endl;

  MockCubeb* cubeb = new MockCubeb();
  const TimeDuration startDelay = TimeDuration::FromSeconds(0.3);
  cubeb->SetStreamStartDelay(startDelay);
  auto unforcer = WaitFor(cubeb->ForceAudioThread()).unwrap();
  Unused << unforcer;
  CubebUtils::ForceSetCubebContext(cubeb->AsCubebContext());

  RefPtr<AudioCallbackDriver> driver;
  auto graph = MakeRefPtr<NiceMock<MockGraphInterface>>(aRate);
  EXPECT_CALL(*graph, NotifyInputStopped).Times(0);

  Maybe<size_t> fallbackIterationCount;
  Maybe<int64_t> audioStart;
  Maybe<uint32_t> alreadyBuffered;
  int64_t inputFrameCount = 0;
  int64_t outputFrameCount = 0;
  int64_t processedFrameCount = 0;
  ON_CALL(*graph, NotifyInputData)
      .WillByDefault([&](const AudioDataValue*, size_t aFrames, TrackRate aRate,
                         uint32_t, uint32_t aAlreadyBuffered) {
        if (!audioStart) {
          fallbackIterationCount = Some(graph->IterationCount());
          audioStart = Some(graph->StateComputedTime());
          alreadyBuffered = Some(aAlreadyBuffered);
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
  ON_CALL(*graph, NotifyOutputData)
      .WillByDefault([&](AudioDataValue*, size_t aFrames, TrackRate aRate,
                         uint32_t) { outputFrameCount += aFrames; });

  driver = MakeRefPtr<AudioCallbackDriver>(graph, nullptr, aRate, 2, 2, nullptr,
                                           (void*)1, AudioInputType::Voice);
  EXPECT_FALSE(driver->ThreadRunning()) << "Verify thread is not running";
  EXPECT_FALSE(driver->IsStarted()) << "Verify thread is not started";

  graph->SetCurrentDriver(driver);
  graph->SetEnsureNextIteration(true);

  driver->Start();
  RefPtr<SmartMockCubebStream> stream = WaitFor(cubeb->StreamInitEvent());
  // Wait for at least 100ms of audio data.
  WaitUntil(stream->FramesProcessedEvent(), [&](uint32_t aFrames) {
    processedFrameCount += aFrames;
    return processedFrameCount >= aRate / 10;
  });

  // This will block untill all events have been executed.
  graph->StopIterating();
  MOZ_KnownLive(driver)->Shutdown();

  const uint32_t tenMillis = aRate / 100;
  EXPECT_EQ(inputFrameCount, outputFrameCount);
  // Margin (50ms) is there to cover (fake) audio callbacks that occur while
  // waiting for fallback driver's next iteration to detect that it shall stop.
  EXPECT_GE(*audioStart, MediaTrackGraphImpl::RoundUpToEndOfAudioBlock(
                             startDelay.ToSeconds() * aRate - tenMillis * 5))
      << "Fallback driver runs for at least the duration of the start delay";
  // Margin (150ms) is generous because on some try machines the
  // SystemClockDriver can take upwards of 100ms (should be 10ms) between
  // iterations.
  EXPECT_LE(*audioStart, MediaTrackGraphImpl::RoundUpToEndOfAudioBlock(
                             startDelay.ToSeconds() * aRate + tenMillis * 15))
      << "Fallback driver does not run significantly longer than the duration "
         "of the start delay";
  EXPECT_NEAR(graph->StateComputedTime() - *audioStart,
              inputFrameCount + *alreadyBuffered, WEBAUDIO_BLOCK_SIZE)
      << "Graph progresses while audio driver runs. stateComputedTime="
      << graph->StateComputedTime() << ", inputFrameCount=" << inputFrameCount;
  EXPECT_GE(*fallbackIterationCount, 3U)
      << "Fallback driver runs continuously while starting audio driver";
  EXPECT_LE(*fallbackIterationCount,
            static_cast<float>(*audioStart) / tenMillis)
      << "Fallback driver does not iterate more frequently than every 10ms";
}

TEST(TestAudioCallbackDriver, SlowStart)
MOZ_CAN_RUN_SCRIPT_FOR_DEFINITION {
  TestSlowStart(1000);   // 10ms = 10 <<< 128 samples
  TestSlowStart(8000);   // 10ms = 80  <  128 samples
  TestSlowStart(44100);  // 10ms = 441 >  128 samples
}
