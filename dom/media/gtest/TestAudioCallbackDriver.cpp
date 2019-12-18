/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CubebUtils.h"
#include "GraphDriver.h"
#include "MediaTrackGraphImpl.h"

#include "gmock/gmock.h"
#include "gtest/gtest-printers.h"
#include "gtest/gtest.h"

#include "mozilla/Attributes.h"
#include "mozilla/UniquePtr.h"
#include "nsTArray.h"

#include "MockCubeb.h"

using ::testing::Return;
using namespace mozilla;

RefPtr<MediaTrackGraphImpl> MakeMTGImpl() {
  return MakeRefPtr<MediaTrackGraphImpl>(MediaTrackGraph::AUDIO_THREAD_DRIVER,
                                         MediaTrackGraph::DIRECT_DRIVER, 44100,
                                         2, nullptr);
}

#if 0
TEST(TestAudioCallbackDriver, StartStop)
MOZ_CAN_RUN_SCRIPT_FOR_DEFINITION {
  MockCubeb* mock = new MockCubeb();
  CubebUtils::ForceSetCubebContext(mock->AsCubebContext());

  RefPtr<MediaTrackGraphImpl> graph = MakeMTGImpl();
  EXPECT_TRUE(!!graph->mDriver) << "AudioCallbackDriver created.";

  AudioCallbackDriver* driver = graph->mDriver->AsAudioCallbackDriver();
  EXPECT_FALSE(driver->ThreadRunning()) << "Verify thread is not running";
  EXPECT_FALSE(driver->IsStarted()) << "Verify thread is not started";

  driver->Start();
  // Allow some time to "play" audio.
  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  EXPECT_TRUE(driver->ThreadRunning()) << "Verify thread is running";
  EXPECT_TRUE(driver->IsStarted()) << "Verify thread is started";

  // This will block untill all events have been executed.
  MOZ_KnownLive(driver->AsAudioCallbackDriver())->Shutdown();
  EXPECT_FALSE(driver->ThreadRunning()) << "Verify thread is not running";
  EXPECT_FALSE(driver->IsStarted()) << "Verify thread is not started";

  // This is required because the MTG and the driver hold references between
  // each other. The driver has a reference to SharedThreadPool which will
  // block for ever if it is not cleared. The same logic exists in
  // MediaTrackGraphShutDownRunnable
  graph->mDriver = nullptr;

  graph->RemoveShutdownBlocker();
}
#endif
