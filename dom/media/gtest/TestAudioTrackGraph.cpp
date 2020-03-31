/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaTrackGraphImpl.h"

#include "gmock/gmock.h"
#include "gtest/gtest-printers.h"
#include "gtest/gtest.h"

#include "GMPTestMonitor.h"
#include "MockCubeb.h"

TEST(TestAudioTrackGraph, DifferentDeviceIDs)
{
  MockCubeb* cubeb = new MockCubeb();
  CubebUtils::ForceSetCubebContext(cubeb->AsCubebContext());

  MediaTrackGraph* g1 = MediaTrackGraph::GetInstance(
      MediaTrackGraph::AUDIO_THREAD_DRIVER, /*window*/ nullptr,
      MediaTrackGraph::REQUEST_DEFAULT_SAMPLE_RATE,
      /*OutputDeviceID*/ nullptr);

  MediaTrackGraph* g2 = MediaTrackGraph::GetInstance(
      MediaTrackGraph::AUDIO_THREAD_DRIVER, /*window*/ nullptr,
      MediaTrackGraph::REQUEST_DEFAULT_SAMPLE_RATE,
      /*OutputDeviceID*/ reinterpret_cast<cubeb_devid>(1));

  MediaTrackGraph* g1_2 = MediaTrackGraph::GetInstance(
      MediaTrackGraph::AUDIO_THREAD_DRIVER, /*window*/ nullptr,
      MediaTrackGraph::REQUEST_DEFAULT_SAMPLE_RATE,
      /*OutputDeviceID*/ nullptr);

  MediaTrackGraph* g2_2 = MediaTrackGraph::GetInstance(
      MediaTrackGraph::AUDIO_THREAD_DRIVER, /*window*/ nullptr,
      MediaTrackGraph::REQUEST_DEFAULT_SAMPLE_RATE,
      /*OutputDeviceID*/ reinterpret_cast<cubeb_devid>(1));

  EXPECT_NE(g1, g2) << "Different graphs have due to different device ids";
  EXPECT_EQ(g1, g1_2) << "Same graphs for same device ids";
  EXPECT_EQ(g2, g2_2) << "Same graphs for same device ids";

  // Dummy track to make graph rolling. Add it and remove it to remove the
  // graph from the global hash table and let it shutdown.
  RefPtr<SourceMediaTrack> dummySource1 =
      g1->CreateSourceTrack(MediaSegment::AUDIO);
  RefPtr<SourceMediaTrack> dummySource2 =
      g2->CreateSourceTrack(MediaSegment::AUDIO);
  dummySource1->Destroy();
  dummySource2->Destroy();
}

// Disabled on android due to high failure rate in bug 1622897
#if !defined(ANDROID)
TEST(TestAudioTrackGraph, SetOutputDeviceID)
{
  MockCubeb* cubeb = new MockCubeb();
  CubebUtils::ForceSetCubebContext(cubeb->AsCubebContext());

  // Set the output device id in GetInstance method confirm that it is the one
  // used in cubeb_stream_init.
  MediaTrackGraph* graph = MediaTrackGraph::GetInstance(
      MediaTrackGraph::AUDIO_THREAD_DRIVER, /*window*/ nullptr,
      MediaTrackGraph::REQUEST_DEFAULT_SAMPLE_RATE,
      /*OutputDeviceID*/ reinterpret_cast<cubeb_devid>(1));

  // Cubeb stream has not been init yet, confirm invalid device id `-1`
  EXPECT_EQ(cubeb->GetCurrentOutputDeviceID(),
            reinterpret_cast<cubeb_devid>(-1))
      << "Initial state, invalid output device id";

  // Dummy track to make graph rolling. Add it and remove it to remove the
  // graph from the global hash table and let it shutdown.
  RefPtr<SourceMediaTrack> dummySource =
      graph->CreateSourceTrack(MediaSegment::AUDIO);

  /* Use a ControlMessage to signal that the AudioCallbackDriver has started. */
  class Message : public ControlMessage {
   public:
    explicit Message(MediaTrack* aTrack) : ControlMessage(aTrack) {}
    void Run() override {
      MOZ_ASSERT(mTrack->GraphImpl()->CurrentDriver()->AsAudioCallbackDriver());
      mGraphStartedPromise.Resolve(true, __func__);
    }
    void RunDuringShutdown() override {
      // During shutdown we still want the listener's NotifyRemoved to be
      // called, since not doing that might block shutdown of other modules.
      Run();
    }
    MozPromiseHolder<GenericPromise> mGraphStartedPromise;
  };

  GMPTestMonitor mon;
  UniquePtr<Message> message = MakeUnique<Message>(dummySource);
  RefPtr<GenericPromise> p = message->mGraphStartedPromise.Ensure(__func__);
  p->Then(GetMainThreadSerialEventTarget(), __func__,
          [&mon, cubeb, dummySource]() {
            EXPECT_EQ(cubeb->GetCurrentOutputDeviceID(),
                      reinterpret_cast<cubeb_devid>(1))
                << "After init confirm the expected output device id";
            // Test has finished, destroy the track to shutdown the MTG.
            dummySource->Destroy();
            mon.SetFinished();
          });

  dummySource->GraphImpl()->AppendMessage(std::move(message));

  mon.AwaitFinished();
}
#endif
