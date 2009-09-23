// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test of classes in the tracked_objects.h classes.

#include "base/tracked_objects.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tracked_objects {

class TrackedObjectsTest : public testing::Test {
 public:
  MessageLoop message_loop_;
};

TEST_F(TrackedObjectsTest, MinimalStartupShutdown) {
  // Minimal test doesn't even create any tasks.
  if (!ThreadData::StartTracking(true))
    return;

  EXPECT_FALSE(ThreadData::first());  // No activity even on this thread.
  ThreadData* data = ThreadData::current();
  EXPECT_TRUE(ThreadData::first());  // Now class was constructed.
  EXPECT_TRUE(data);
  EXPECT_TRUE(!data->next());
  EXPECT_EQ(data, ThreadData::current());
  ThreadData::BirthMap birth_map;
  data->SnapshotBirthMap(&birth_map);
  EXPECT_EQ(0u, birth_map.size());
  ThreadData::DeathMap death_map;
  data->SnapshotDeathMap(&death_map);
  EXPECT_EQ(0u, death_map.size());
  ThreadData::ShutdownSingleThreadedCleanup();

  // Do it again, just to be sure we reset state completely.
  ThreadData::StartTracking(true);
  EXPECT_FALSE(ThreadData::first());  // No activity even on this thread.
  data = ThreadData::current();
  EXPECT_TRUE(ThreadData::first());  // Now class was constructed.
  EXPECT_TRUE(data);
  EXPECT_TRUE(!data->next());
  EXPECT_EQ(data, ThreadData::current());
  birth_map.clear();
  data->SnapshotBirthMap(&birth_map);
  EXPECT_EQ(0u, birth_map.size());
  death_map.clear();
  data->SnapshotDeathMap(&death_map);
  EXPECT_EQ(0u, death_map.size());
  ThreadData::ShutdownSingleThreadedCleanup();
}

class NoopTracked : public tracked_objects::Tracked {
};

TEST_F(TrackedObjectsTest, TinyStartupShutdown) {
  if (!ThreadData::StartTracking(true))
    return;

  // Instigate tracking on a single tracked object, or our thread.
  NoopTracked tracked;

  const ThreadData* data = ThreadData::first();
  EXPECT_TRUE(data);
  EXPECT_TRUE(!data->next());
  EXPECT_EQ(data, ThreadData::current());
  ThreadData::BirthMap birth_map;
  data->SnapshotBirthMap(&birth_map);
  EXPECT_EQ(1u, birth_map.size());                         // 1 birth location.
  EXPECT_EQ(1, birth_map.begin()->second->birth_count());  // 1 birth.
  ThreadData::DeathMap death_map;
  data->SnapshotDeathMap(&death_map);
  EXPECT_EQ(0u, death_map.size());                         // No deaths.


  // Now instigate a birth, and a death.
  delete new NoopTracked;

  birth_map.clear();
  data->SnapshotBirthMap(&birth_map);
  EXPECT_EQ(1u, birth_map.size());                         // 1 birth location.
  EXPECT_EQ(2, birth_map.begin()->second->birth_count());  // 2 births.
  death_map.clear();
  data->SnapshotDeathMap(&death_map);
  EXPECT_EQ(1u, death_map.size());                         // 1 location.
  EXPECT_EQ(1, death_map.begin()->second.count());         // 1 death.

  // The births were at the same location as the one known death.
  EXPECT_EQ(birth_map.begin()->second, death_map.begin()->first);

  ThreadData::ShutdownSingleThreadedCleanup();
}

}  // namespace tracked_objects
