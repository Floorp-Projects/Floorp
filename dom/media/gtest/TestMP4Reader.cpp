/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "MP4Reader.h"
#include "MP4Decoder.h"
#include "SharedThreadPool.h"
#include "MockMediaResource.h"
#include "MockMediaDecoderOwner.h"
#include "mozilla/Preferences.h"
#include "TimeUnits.h"

using namespace mozilla;
using namespace mozilla::dom;

class TestBinding
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(TestBinding);

  nsRefPtr<MP4Decoder> decoder;
  nsRefPtr<MockMediaResource> resource;
  nsRefPtr<MP4Reader> reader;

  explicit TestBinding(const char* aFileName = "gizmo.mp4")
    : decoder(new MP4Decoder())
    , resource(new MockMediaResource(aFileName))
    , reader(new MP4Reader(decoder))
  {
    EXPECT_EQ(NS_OK, Preferences::SetBool(
                       "media.fragmented-mp4.use-blank-decoder", true));

    EXPECT_EQ(NS_OK, resource->Open(nullptr));
    decoder->SetResource(resource);

    reader->Init(nullptr);
    reader->EnsureTaskQueue();
    {
      // This needs to be done before invoking GetBuffered. This is normally
      // done by MediaDecoderStateMachine.
      ReentrantMonitorAutoEnter mon(decoder->GetReentrantMonitor());
      reader->SetStartTime(0);
    }
  }

  void Init() {
    nsCOMPtr<nsIThread> thread;
    nsresult rv = NS_NewThread(getter_AddRefs(thread),
                               NS_NewRunnableMethod(this, &TestBinding::ReadMetadata));
    EXPECT_EQ(NS_OK, rv);
    thread->Shutdown();
  }

private:
  virtual ~TestBinding()
  {
    {
      nsRefPtr<MediaTaskQueue> queue = reader->GetTaskQueue();
      nsCOMPtr<nsIRunnable> task = NS_NewRunnableMethod(reader, &MP4Reader::Shutdown);
      // Hackily bypass the tail dispatcher so that we can AwaitShutdownAndIdle.
      // In production code we'd use BeginShutdown + promises.
      queue->Dispatch(task.forget(), AbstractThread::AssertDispatchSuccess,
                      AbstractThread::TailDispatch);
      queue->AwaitShutdownAndIdle();
    }
    decoder = nullptr;
    resource = nullptr;
    reader = nullptr;
    SharedThreadPool::SpinUntilShutdown();
  }

  void ReadMetadata()
  {
    MediaInfo info;
    MetadataTags* tags;
    EXPECT_EQ(NS_OK, reader->ReadMetadata(&info, &tags));
  }
};

TEST(MP4Reader, BufferedRange)
{
  nsRefPtr<TestBinding> b = new TestBinding();
  b->Init();

  // Video 3-4 sec, audio 2.986666-4.010666 sec
  b->resource->MockAddBufferedRange(248400, 327455);

  media::TimeIntervals ranges = b->reader->GetBuffered();
  EXPECT_EQ(1U, ranges.Length());
  EXPECT_NEAR(270000 / 90000.0, ranges.Start(0).ToSeconds(), 0.000001);
  EXPECT_NEAR(360000 / 90000.0, ranges.End(0).ToSeconds(), 0.000001);
}

TEST(MP4Reader, BufferedRangeMissingLastByte)
{
  nsRefPtr<TestBinding> b = new TestBinding();
  b->Init();

  // Dropping the last byte of the video
  b->resource->MockClearBufferedRanges();
  b->resource->MockAddBufferedRange(248400, 324912);
  b->resource->MockAddBufferedRange(324913, 327455);

  media::TimeIntervals ranges = b->reader->GetBuffered();
  EXPECT_EQ(1U, ranges.Length());
  EXPECT_NEAR(270000.0 / 90000.0, ranges.Start(0).ToSeconds(), 0.000001);
  EXPECT_NEAR(357000 / 90000.0, ranges.End(0).ToSeconds(), 0.000001);
}

TEST(MP4Reader, BufferedRangeSyncFrame)
{
  nsRefPtr<TestBinding> b = new TestBinding();
  b->Init();

  // Check that missing the first byte at 2 seconds skips right through to 3
  // seconds because of a missing sync frame
  b->resource->MockClearBufferedRanges();
  b->resource->MockAddBufferedRange(146336, 327455);

  media::TimeIntervals ranges = b->reader->GetBuffered();
  EXPECT_EQ(1U, ranges.Length());
  EXPECT_NEAR(270000.0 / 90000.0, ranges.Start(0).ToSeconds(), 0.000001);
  EXPECT_NEAR(360000 / 90000.0, ranges.End(0).ToSeconds(), 0.000001);
}

TEST(MP4Reader, CompositionOrder)
{
  nsRefPtr<TestBinding> b = new TestBinding("mediasource_test.mp4");
  b->Init();

  // The first 5 video samples of this file are:
  // Video timescale=2500
  // Frame Start  Size  Time  Duration  Sync
  //     1    48  5455   166        83  Yes
  //     2  5503   145   249        83
  //     3  6228   575   581        83
  //     4  7383   235   415        83
  //     5  8779   183   332        83
  //     6  9543   191   498        83
  //
  // Audio timescale=44100
  //     1  5648   580     0      1024  Yes
  //     2  6803   580  1024      1058  Yes
  //     3  7618   581  2082      1014  Yes
  //     4  8199   580  3096      1015  Yes
  //     5  8962   581  4111      1014  Yes
  //     6  9734   580  5125      1014  Yes
  //     7 10314   581  6139      1059  Yes
  //     8 11207   580  7198      1014  Yes
  //     9 12035   581  8212      1014  Yes
  //    10 12616   580  9226      1015  Yes
  //    11 13220   581  10241     1014  Yes

  b->resource->MockClearBufferedRanges();
  // First two frames in decoding + first audio frame
  b->resource->MockAddBufferedRange(48, 5503);   // Video 1
  b->resource->MockAddBufferedRange(5503, 5648); // Video 2
  b->resource->MockAddBufferedRange(6228, 6803); // Video 3

  // Audio - 5 frames; 0 - 139206 us
  b->resource->MockAddBufferedRange(5648, 6228);
  b->resource->MockAddBufferedRange(6803, 7383);
  b->resource->MockAddBufferedRange(7618, 8199);
  b->resource->MockAddBufferedRange(8199, 8779);
  b->resource->MockAddBufferedRange(8962, 9563);
  b->resource->MockAddBufferedRange(9734, 10314);
  b->resource->MockAddBufferedRange(10314, 10895);
  b->resource->MockAddBufferedRange(11207, 11787);
  b->resource->MockAddBufferedRange(12035, 12616);
  b->resource->MockAddBufferedRange(12616, 13196);
  b->resource->MockAddBufferedRange(13220, 13901);

  media::TimeIntervals ranges = b->reader->GetBuffered();
  EXPECT_EQ(2U, ranges.Length());

  EXPECT_NEAR(166.0 / 2500.0, ranges.Start(0).ToSeconds(), 0.000001);
  EXPECT_NEAR(332.0 / 2500.0, ranges.End(0).ToSeconds(), 0.000001);

  EXPECT_NEAR(581.0 / 2500.0, ranges.Start(1).ToSeconds(), 0.000001);
  EXPECT_NEAR(11255.0 / 44100.0, ranges.End(1).ToSeconds(), 0.000001);
}

TEST(MP4Reader, Normalised)
{
  nsRefPtr<TestBinding> b = new TestBinding("mediasource_test.mp4");
  b->Init();

  // The first 5 video samples of this file are:
  // Video timescale=2500
  // Frame Start  Size  Time  Duration  Sync
  //     1    48  5455   166        83  Yes
  //     2  5503   145   249        83
  //     3  6228   575   581        83
  //     4  7383   235   415        83
  //     5  8779   183   332        83
  //     6  9543   191   498        83
  //
  // Audio timescale=44100
  //     1  5648   580     0      1024  Yes
  //     2  6803   580  1024      1058  Yes
  //     3  7618   581  2082      1014  Yes
  //     4  8199   580  3096      1015  Yes
  //     5  8962   581  4111      1014  Yes
  //     6  9734   580  5125      1014  Yes
  //     7 10314   581  6139      1059  Yes
  //     8 11207   580  7198      1014  Yes
  //     9 12035   581  8212      1014  Yes
  //    10 12616   580  9226      1015  Yes
  //    11 13220   581  10241     1014  Yes

  b->resource->MockClearBufferedRanges();
  b->resource->MockAddBufferedRange(48, 13901);

  media::TimeIntervals ranges = b->reader->GetBuffered();
  EXPECT_EQ(1U, ranges.Length());

  EXPECT_NEAR(166.0 / 2500.0, ranges.Start(0).ToSeconds(), 0.000001);
  EXPECT_NEAR(11255.0 / 44100.0, ranges.End(0).ToSeconds(), 0.000001);
}
