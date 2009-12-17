// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This test is POSIX only.

#include <unistd.h>
#include <fcntl.h>

#include "base/basictypes.h"
#include "base/eintr_wrapper.h"
#include "chrome/common/file_descriptor_set_posix.h"
#include "testing/gtest/include/gtest/gtest.h"

// The FileDescriptorSet will try and close some of the descriptor numbers
// which we given it. This is the base descriptor value. It's great enough such
// that no real descriptor will accidently be closed.
static const int kFDBase = 50000;

TEST(FileDescriptorSet, BasicAdd) {
  scoped_refptr<FileDescriptorSet> set = new FileDescriptorSet;

  ASSERT_EQ(set->size(), 0u);
  ASSERT_TRUE(set->empty());
  ASSERT_TRUE(set->Add(kFDBase));
  ASSERT_EQ(set->size(), 1u);
  ASSERT_TRUE(!set->empty());

  // Empties the set and stops a warning about deleting a set with unconsumed
  // descriptors
  set->CommitAll();
}

TEST(FileDescriptorSet, BasicAddAndClose) {
  scoped_refptr<FileDescriptorSet> set = new FileDescriptorSet;

  ASSERT_EQ(set->size(), 0u);
  ASSERT_TRUE(set->empty());
  ASSERT_TRUE(set->AddAndAutoClose(kFDBase));
  ASSERT_EQ(set->size(), 1u);
  ASSERT_TRUE(!set->empty());

  set->CommitAll();
}

TEST(FileDescriptorSet, MaxSize) {
  scoped_refptr<FileDescriptorSet> set = new FileDescriptorSet;

  for (unsigned i = 0;
       i < FileDescriptorSet::MAX_DESCRIPTORS_PER_MESSAGE; ++i) {
    ASSERT_TRUE(set->Add(kFDBase + 1 + i));
  }

  ASSERT_TRUE(!set->Add(kFDBase));

  set->CommitAll();
}

TEST(FileDescriptorSet, SetDescriptors) {
  scoped_refptr<FileDescriptorSet> set = new FileDescriptorSet;

  ASSERT_TRUE(set->empty());
  set->SetDescriptors(NULL, 0);
  ASSERT_TRUE(set->empty());

  static const int fds[] = {kFDBase};
  set->SetDescriptors(fds, 1);
  ASSERT_TRUE(!set->empty());
  ASSERT_EQ(set->size(), 1u);

  set->CommitAll();
}

TEST(FileDescriptorSet, GetDescriptors) {
  scoped_refptr<FileDescriptorSet> set = new FileDescriptorSet;

  set->GetDescriptors(NULL);
  ASSERT_TRUE(set->Add(kFDBase));

  int fds[1];
  fds[0] = 0;
  set->GetDescriptors(fds);
  ASSERT_EQ(fds[0], kFDBase);
  set->CommitAll();
  ASSERT_TRUE(set->empty());
}

TEST(FileDescriptorSet, WalkInOrder) {
  scoped_refptr<FileDescriptorSet> set = new FileDescriptorSet;

  ASSERT_TRUE(set->Add(kFDBase));
  ASSERT_TRUE(set->Add(kFDBase + 1));
  ASSERT_TRUE(set->Add(kFDBase + 2));

  ASSERT_EQ(set->GetDescriptorAt(0), kFDBase);
  ASSERT_EQ(set->GetDescriptorAt(1), kFDBase + 1);
  ASSERT_EQ(set->GetDescriptorAt(2), kFDBase + 2);

  set->CommitAll();
}

TEST(FileDescriptorSet, WalkWrongOrder) {
  scoped_refptr<FileDescriptorSet> set = new FileDescriptorSet;

  ASSERT_TRUE(set->Add(kFDBase));
  ASSERT_TRUE(set->Add(kFDBase + 1));
  ASSERT_TRUE(set->Add(kFDBase + 2));

  ASSERT_EQ(set->GetDescriptorAt(0), kFDBase);
  ASSERT_EQ(set->GetDescriptorAt(2), -1);

  set->CommitAll();
}

TEST(FileDescriptorSet, WalkCycle) {
  scoped_refptr<FileDescriptorSet> set = new FileDescriptorSet;

  ASSERT_TRUE(set->Add(kFDBase));
  ASSERT_TRUE(set->Add(kFDBase + 1));
  ASSERT_TRUE(set->Add(kFDBase + 2));

  ASSERT_EQ(set->GetDescriptorAt(0), kFDBase);
  ASSERT_EQ(set->GetDescriptorAt(1), kFDBase + 1);
  ASSERT_EQ(set->GetDescriptorAt(2), kFDBase + 2);
  ASSERT_EQ(set->GetDescriptorAt(0), kFDBase);
  ASSERT_EQ(set->GetDescriptorAt(1), kFDBase + 1);
  ASSERT_EQ(set->GetDescriptorAt(2), kFDBase + 2);
  ASSERT_EQ(set->GetDescriptorAt(0), kFDBase);
  ASSERT_EQ(set->GetDescriptorAt(1), kFDBase + 1);
  ASSERT_EQ(set->GetDescriptorAt(2), kFDBase + 2);

  set->CommitAll();
}

TEST(FileDescriptorSet, DontClose) {
  scoped_refptr<FileDescriptorSet> set = new FileDescriptorSet;

  const int fd = open("/dev/null", O_RDONLY);
  ASSERT_TRUE(set->Add(fd));
  set->CommitAll();

  const int duped = dup(fd);
  ASSERT_GE(duped, 0);
  HANDLE_EINTR(close(duped));
  HANDLE_EINTR(close(fd));
}

TEST(FileDescriptorSet, DoClose) {
  scoped_refptr<FileDescriptorSet> set = new FileDescriptorSet;

  const int fd = open("/dev/null", O_RDONLY);
  ASSERT_TRUE(set->AddAndAutoClose(fd));
  set->CommitAll();

  const int duped = dup(fd);
  ASSERT_EQ(duped, -1);
  HANDLE_EINTR(close(fd));
}
