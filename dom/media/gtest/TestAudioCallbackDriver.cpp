/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CubebUtils.h"
#include "GraphDriver.h"

#include "gmock/gmock.h"
#include "gtest/gtest-printers.h"
#include "gtest/gtest.h"

#include "mozilla/Attributes.h"
#include "mozilla/UniquePtr.h"
#include "nsTArray.h"

#include "MockCubeb.h"

using namespace mozilla;
using IterationResult = GraphInterface::IterationResult;
using ::testing::NiceMock;
using ::testing::Return;

class MockGraphInterface : public GraphInterface {
  NS_DECL_THREADSAFE_ISUPPORTS
  MOCK_METHOD4(NotifyOutputData,
               void(AudioDataValue*, size_t, TrackRate, uint32_t));
  MOCK_METHOD0(NotifyInputStopped, void());
  MOCK_METHOD5(NotifyInputData, void(const AudioDataValue*, size_t, TrackRate,
                                     uint32_t, uint32_t));
  MOCK_METHOD0(DeviceChanged, void());
  /* OneIteration cannot be mocked because IterationResult is non-memmovable and
   * cannot be passed as a parameter, which GMock does internally. */
  IterationResult OneIteration(GraphTime, GraphTime, AudioMixer*) {
    if (!mKeepProcessing) {
      return IterationResult::CreateStop(
          NS_NewRunnableFunction(__func__, [] {}));
    }
    GraphDriver* next = mNextDriver.exchange(nullptr);
    if (next) {
      return IterationResult::CreateSwitchDriver(
          next, NS_NewRunnableFunction(__func__, [] {}));
    }
    return IterationResult::CreateStillProcessing();
  }
#ifdef DEBUG
  bool InDriverIteration(GraphDriver* aDriver) override {
    return aDriver->OnThread();
  }
#endif

  void StopIterating() { mKeepProcessing = false; }

  void SwitchTo(GraphDriver* aDriver) { mNextDriver = aDriver; }

 protected:
  Atomic<bool> mKeepProcessing{true};
  Atomic<GraphDriver*> mNextDriver{nullptr};
  virtual ~MockGraphInterface() = default;
};

NS_IMPL_ISUPPORTS0(MockGraphInterface)

TEST(TestAudioCallbackDriver, StartStop)
MOZ_CAN_RUN_SCRIPT_FOR_DEFINITION {
  MockCubeb* cubeb = new MockCubeb();
  CubebUtils::ForceSetCubebContext(cubeb->AsCubebContext());

  RefPtr<AudioCallbackDriver> driver;
  auto graph = MakeRefPtr<NiceMock<MockGraphInterface>>();
  EXPECT_CALL(*graph, NotifyInputStopped).Times(0);
  ON_CALL(*graph, NotifyOutputData)
      .WillByDefault([&](AudioDataValue*, size_t, TrackRate, uint32_t) {});

  driver = MakeRefPtr<AudioCallbackDriver>(graph, nullptr, 44100, 2, 0, nullptr,
                                           nullptr, AudioInputType::Unknown);
  EXPECT_FALSE(driver->ThreadRunning()) << "Verify thread is not running";
  EXPECT_FALSE(driver->IsStarted()) << "Verify thread is not started";

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
