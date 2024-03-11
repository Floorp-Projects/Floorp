/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <gtest/gtest.h>

#include "MediaData.h"
#include "MediaQueue.h"
#include "nsISupportsImpl.h"

using namespace mozilla;
using mozilla::media::TimeUnit;

MediaData* CreateDataRawPtr(
    int64_t aStartTime, int64_t aEndTime,
    MediaData::Type aType = MediaData::Type::NULL_DATA) {
  const TimeUnit startTime = TimeUnit::FromMicroseconds(aStartTime);
  const TimeUnit endTime = TimeUnit::FromMicroseconds(aEndTime);
  MediaData* data;
  if (aType == MediaData::Type::AUDIO_DATA) {
    AlignedAudioBuffer samples;
    data = new AudioData(0, startTime, std::move(samples), 2, 44100);
    data->mDuration = endTime - startTime;
  } else if (aType == MediaData::Type::VIDEO_DATA) {
    data = new VideoData(0, startTime, endTime - startTime, true, startTime,
                         gfx::IntSize(), 0);
  } else {
    data = new NullData(0, startTime, endTime - startTime);
  }
  return data;
}

already_AddRefed<MediaData> CreateData(int64_t aStartTime, int64_t aEndTime) {
  RefPtr<MediaData> data = CreateDataRawPtr(aStartTime, aEndTime);
  return data.forget();
}

// Used to avoid the compile error `comparison of integers of different signs`
// when comparing 'const unsigned long' and 'const int'.
#define EXPECT_EQUAL_SIZE_T(lhs, rhs) EXPECT_EQ(size_t(lhs), size_t(rhs))

TEST(MediaQueue, BasicPopOperations)
{
  MediaQueue<MediaData> queue;
  EXPECT_EQUAL_SIZE_T(queue.GetSize(), 0);

  // Test only one element
  const RefPtr<MediaData> data = CreateDataRawPtr(0, 10);
  queue.Push(data.get());
  EXPECT_EQUAL_SIZE_T(queue.GetSize(), 1);

  RefPtr<MediaData> rv = queue.PopFront();
  EXPECT_EQ(rv, data);

  queue.Push(data.get());
  EXPECT_EQUAL_SIZE_T(queue.GetSize(), 1);

  rv = queue.PopBack();
  EXPECT_EQ(rv, data);
  EXPECT_EQUAL_SIZE_T(queue.GetSize(), 0);

  // Test multiple elements
  const RefPtr<MediaData> data1 = CreateDataRawPtr(0, 10);
  const RefPtr<MediaData> data2 = CreateDataRawPtr(11, 20);
  const RefPtr<MediaData> data3 = CreateDataRawPtr(21, 30);
  queue.Push(data1.get());
  queue.Push(data2.get());
  queue.Push(data3.get());
  EXPECT_EQUAL_SIZE_T(queue.GetSize(), 3);

  rv = queue.PopFront();
  EXPECT_EQUAL_SIZE_T(queue.GetSize(), 2);
  EXPECT_EQ(rv, data1);

  rv = queue.PopBack();
  EXPECT_EQUAL_SIZE_T(queue.GetSize(), 1);
  EXPECT_EQ(rv, data3);

  rv = queue.PopBack();
  EXPECT_EQUAL_SIZE_T(queue.GetSize(), 0);
  EXPECT_EQ(rv, data2);
}

TEST(MediaQueue, BasicPeekOperations)
{
  MediaQueue<MediaData> queue;
  EXPECT_EQUAL_SIZE_T(queue.GetSize(), 0);

  // Test only one element
  const RefPtr<MediaData> data1 = CreateDataRawPtr(0, 10);
  queue.Push(data1.get());
  EXPECT_EQUAL_SIZE_T(queue.GetSize(), 1);

  RefPtr<MediaData> rv = queue.PeekFront();
  EXPECT_EQUAL_SIZE_T(queue.GetSize(), 1);
  EXPECT_EQ(rv, data1);

  rv = queue.PeekBack();
  EXPECT_EQUAL_SIZE_T(queue.GetSize(), 1);
  EXPECT_EQ(rv, data1);

  // Test multiple elements
  const RefPtr<MediaData> data2 = CreateDataRawPtr(11, 20);
  const RefPtr<MediaData> data3 = CreateDataRawPtr(21, 30);
  queue.Push(data2.get());
  queue.Push(data3.get());
  EXPECT_EQUAL_SIZE_T(queue.GetSize(), 3);

  rv = queue.PeekFront();
  EXPECT_EQUAL_SIZE_T(queue.GetSize(), 3);
  EXPECT_EQ(rv, data1);

  rv = queue.PeekBack();
  EXPECT_EQUAL_SIZE_T(queue.GetSize(), 3);
  EXPECT_EQ(rv, data3);
}

TEST(MediaQueue, FinishQueue)
{
  MediaQueue<MediaData> queue;
  EXPECT_FALSE(queue.IsFinished());

  queue.Finish();
  EXPECT_TRUE(queue.IsFinished());
}

TEST(MediaQueue, EndOfStream)
{
  MediaQueue<MediaData> queue;
  EXPECT_FALSE(queue.IsFinished());

  queue.Push(CreateData(0, 10));
  EXPECT_EQUAL_SIZE_T(queue.GetSize(), 1);

  queue.Finish();
  EXPECT_EQUAL_SIZE_T(queue.GetSize(), 1);
  EXPECT_TRUE(queue.IsFinished());
  EXPECT_FALSE(queue.AtEndOfStream());

  RefPtr<MediaData> rv = queue.PopFront();
  EXPECT_EQUAL_SIZE_T(queue.GetSize(), 0);
  EXPECT_TRUE(queue.IsFinished());
  EXPECT_TRUE(queue.AtEndOfStream());
}

TEST(MediaQueue, QueueDuration)
{
  MediaQueue<MediaData> queue;
  EXPECT_EQUAL_SIZE_T(queue.GetSize(), 0);

  queue.Push(CreateData(0, 10));
  queue.Push(CreateData(11, 20));
  queue.Push(CreateData(21, 30));
  EXPECT_EQUAL_SIZE_T(queue.GetSize(), 3);

  const int64_t rv = queue.Duration();
  EXPECT_EQ(rv, 30);
}

TEST(MediaQueue, CallGetElementAfterOnSingleElement)
{
  MediaQueue<MediaData> queue;
  queue.Push(CreateData(0, 10));

  // Target time is earlier than data's end time
  TimeUnit targetTime = TimeUnit::FromMicroseconds(5);
  nsTArray<RefPtr<MediaData>> foundResult;
  queue.GetElementsAfter(targetTime, &foundResult);
  EXPECT_EQUAL_SIZE_T(foundResult.Length(), 1);
  EXPECT_TRUE(foundResult[0]->GetEndTime() > targetTime);

  // Target time is later than data's end time
  targetTime = TimeUnit::FromMicroseconds(15);
  nsTArray<RefPtr<MediaData>> emptyResult;
  queue.GetElementsAfter(targetTime, &emptyResult);
  EXPECT_TRUE(emptyResult.IsEmpty());
}

TEST(MediaQueue, CallGetElementAfterOnMultipleElements)
{
  MediaQueue<MediaData> queue;
  queue.Push(CreateData(0, 10));
  queue.Push(CreateData(11, 20));
  queue.Push(CreateData(21, 30));
  queue.Push(CreateData(31, 40));
  queue.Push(CreateData(41, 50));

  // Should find [21,30], [31,40] and [41,50]
  TimeUnit targetTime = TimeUnit::FromMicroseconds(25);
  nsTArray<RefPtr<MediaData>> foundResult;
  queue.GetElementsAfter(targetTime, &foundResult);
  EXPECT_EQUAL_SIZE_T(foundResult.Length(), 3);
  for (const auto& data : foundResult) {
    EXPECT_TRUE(data->GetEndTime() > targetTime);
  }

  // Should find [31,40] and [41,50]
  targetTime = TimeUnit::FromMicroseconds(30);
  foundResult.Clear();
  queue.GetElementsAfter(targetTime, &foundResult);
  EXPECT_EQUAL_SIZE_T(foundResult.Length(), 2);
  for (const auto& data : foundResult) {
    EXPECT_TRUE(data->GetEndTime() > targetTime);
  }

  // Should find no data.
  targetTime = TimeUnit::FromMicroseconds(60);
  nsTArray<RefPtr<MediaData>> emptyResult;
  queue.GetElementsAfter(targetTime, &emptyResult);
  EXPECT_TRUE(emptyResult.IsEmpty());
}

TEST(MediaQueue, TimestampAdjustmentForSupportDataType)
{
  const size_t kOffSet = 30;
  {
    MediaQueue<AudioData> audioQueue;
    audioQueue.Push(
        CreateDataRawPtr(0, 10, MediaData::Type::AUDIO_DATA)->As<AudioData>());
    audioQueue.SetOffset(TimeUnit::FromMicroseconds(kOffSet));
    audioQueue.Push(
        CreateDataRawPtr(0, 10, MediaData::Type::AUDIO_DATA)->As<AudioData>());

    // Data stored before setting the offset shouldn't be changed
    RefPtr<AudioData> data = audioQueue.PopFront();
    EXPECT_EQ(data->mTime, TimeUnit::FromMicroseconds(0));
    EXPECT_EQ(data->GetEndTime(), TimeUnit::FromMicroseconds(10));

    // Data stored after setting the offset should be changed
    data = audioQueue.PopFront();
    EXPECT_EQ(data->mTime, TimeUnit::FromMicroseconds(0 + kOffSet));
    EXPECT_EQ(data->GetEndTime(), TimeUnit::FromMicroseconds(10 + kOffSet));

    // Reset will clean the offset.
    audioQueue.Reset();
    audioQueue.Push(
        CreateDataRawPtr(0, 10, MediaData::Type::AUDIO_DATA)->As<AudioData>());
    data = audioQueue.PopFront();
    EXPECT_EQ(data->mTime, TimeUnit::FromMicroseconds(0));
    EXPECT_EQ(data->GetEndTime(), TimeUnit::FromMicroseconds(10));
  }

  // Check another supported type
  MediaQueue<VideoData> videoQueue;
  videoQueue.Push(
      CreateDataRawPtr(0, 10, MediaData::Type::VIDEO_DATA)->As<VideoData>());
  videoQueue.SetOffset(TimeUnit::FromMicroseconds(kOffSet));
  videoQueue.Push(
      CreateDataRawPtr(0, 10, MediaData::Type::VIDEO_DATA)->As<VideoData>());

  // Data stored before setting the offset shouldn't be changed
  RefPtr<VideoData> data = videoQueue.PopFront();
  EXPECT_EQ(data->mTime, TimeUnit::FromMicroseconds(0));
  EXPECT_EQ(data->GetEndTime(), TimeUnit::FromMicroseconds(10));

  // Data stored after setting the offset should be changed
  data = videoQueue.PopFront();
  EXPECT_EQ(data->mTime, TimeUnit::FromMicroseconds(0 + kOffSet));
  EXPECT_EQ(data->GetEndTime(), TimeUnit::FromMicroseconds(10 + kOffSet));

  // Reset will clean the offset.
  videoQueue.Reset();
  videoQueue.Push(
      CreateDataRawPtr(0, 10, MediaData::Type::VIDEO_DATA)->As<VideoData>());
  data = videoQueue.PopFront();
  EXPECT_EQ(data->mTime, TimeUnit::FromMicroseconds(0));
  EXPECT_EQ(data->GetEndTime(), TimeUnit::FromMicroseconds(10));
}

TEST(MediaQueue, TimestampAdjustmentForNotSupportDataType)
{
  const size_t kOffSet = 30;

  MediaQueue<MediaData> queue;
  queue.Push(CreateDataRawPtr(0, 10));
  queue.SetOffset(TimeUnit::FromMicroseconds(kOffSet));
  queue.Push(CreateDataRawPtr(0, 10));

  // Offset won't affect any data at all.
  RefPtr<MediaData> data = queue.PopFront();
  EXPECT_EQ(data->mTime, TimeUnit::FromMicroseconds(0));
  EXPECT_EQ(data->GetEndTime(), TimeUnit::FromMicroseconds(10));

  data = queue.PopFront();
  EXPECT_EQ(data->mTime, TimeUnit::FromMicroseconds(0));
  EXPECT_EQ(data->GetEndTime(), TimeUnit::FromMicroseconds(10));
}

TEST(MediaQueue, PreciseDuration)
{
  MediaQueue<MediaData> queueOff;
  queueOff.Push(CreateDataRawPtr(5, 10));
  queueOff.Push(CreateDataRawPtr(0, 5));
  EXPECT_EQ(queueOff.Duration(), 0);
  EXPECT_EQ(queueOff.PreciseDuration(), -1);

  MediaQueue<MediaData> queueOn(true /* aEnablePreciseDuration */);
  queueOn.Push(CreateDataRawPtr(5, 10));
  queueOn.Push(CreateDataRawPtr(0, 5));
  EXPECT_EQ(queueOn.Duration(), 0);
  EXPECT_EQ(queueOn.PreciseDuration(), 10);
}

#undef EXPECT_EQUAL_SIZE_T
