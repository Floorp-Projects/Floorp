/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#define ENABLE_SET_CUBEB_BACKEND 1
#include "GraphDriver.h"
#include "MediaStreamGraphImpl.h"

#include "gmock/gmock.h"
#include "gtest/gtest-printers.h"
#include "gtest/gtest.h"

#include "mozilla/Attributes.h"
#include "mozilla/UniquePtr.h"
#include "nsTArray.h"

#include "MockCubeb.h"

using ::testing::Return;
using namespace mozilla;

RefPtr<MediaStreamGraphImpl> MakeMSGImpl() {
  return MakeRefPtr<MediaStreamGraphImpl>(MediaStreamGraph::AUDIO_THREAD_DRIVER,
                                          MediaStreamGraph::DIRECT_DRIVER,
                                          44100, 2, nullptr);
}

TEST(TestAudioCallbackDriver, Revive)
{
  MockCubeb* mock = new MockCubeb();
  mozilla::CubebUtils::ForceSetCubebContext(mock->AsCubebContext());

  RefPtr<MediaStreamGraphImpl> graph = MakeMSGImpl();
  EXPECT_TRUE(!!graph->mDriver) << "AudioCallbackDriver created.";

  AudioCallbackDriver* driver = graph->mDriver->AsAudioCallbackDriver();
  EXPECT_FALSE(driver->ThreadRunning()) << "Verify thread is not running";
  EXPECT_FALSE(driver->IsStarted()) << "Verify thread is not started";

  driver->Start();
  // Allow some time to "play" audio.
  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  EXPECT_TRUE(driver->ThreadRunning()) << "Verify thread is running";
  EXPECT_TRUE(driver->IsStarted()) << "Verify thread is started";

  driver->Shutdown();
  EXPECT_FALSE(driver->ThreadRunning()) << "Verify thread is not running";
  EXPECT_FALSE(driver->IsStarted()) << "Verify thread is not started";

  driver->Revive();
  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  EXPECT_TRUE(driver->ThreadRunning()) << "Verify thread is running";
  EXPECT_TRUE(driver->IsStarted()) << "Verify thread is started";

  // This will block untill all events has been executed.
  driver->AsAudioCallbackDriver()->Shutdown();
  EXPECT_FALSE(driver->ThreadRunning()) << "Verify thread is not running";
  EXPECT_FALSE(driver->IsStarted()) << "Verify thread is not started";

  // This is required because the MSG and the driver hold references between
  // each other. The driver has a reference to SharedThreadPool which would
  // block for ever if it was not cleared. The same logic exists in
  // MediaStreamGraphShutDownRunnable
  graph->mDriver = nullptr;
}
#undef ENABLE_SET_CUBEB_BACKEND
