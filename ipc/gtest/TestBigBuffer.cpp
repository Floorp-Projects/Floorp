/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "chrome/common/ipc_message.h"
#include "chrome/common/ipc_message_utils.h"
#include "gtest/gtest.h"

#include "mozilla/RandomNum.h"
#include "mozilla/ipc/BigBuffer.h"

namespace mozilla::ipc {

static bool SerializeAndDeserialize(BigBuffer&& aIn, BigBuffer* aOut) {
  IPC::Message msg(MSG_ROUTING_NONE, 0);
  {
    IPC::MessageWriter writer(msg);
    IPC::WriteParam(&writer, std::move(aIn));
  }
  EXPECT_EQ(aIn.Size(), 0u);

  IPC::MessageReader reader(msg);
  return IPC::ReadParam(&reader, aOut);
}

TEST(BigBuffer, Empty)
{
  BigBuffer in(0);
  EXPECT_EQ(in.GetSharedMemory(), nullptr);
  EXPECT_EQ(in.Size(), 0u);

  BigBuffer out;
  ASSERT_TRUE(SerializeAndDeserialize(std::move(in), &out));

  EXPECT_EQ(in.GetSharedMemory(), nullptr);
  EXPECT_EQ(in.Size(), 0u);
  EXPECT_EQ(out.GetSharedMemory(), nullptr);
  EXPECT_EQ(out.Size(), 0u);
}

TEST(BigBuffer, SmallSize)
{
  uint8_t data[]{1, 2, 3};
  BigBuffer in{Span(data)};
  EXPECT_EQ(in.GetSharedMemory(), nullptr);
  EXPECT_EQ(in.Size(), 3u);

  BigBuffer out;
  ASSERT_TRUE(SerializeAndDeserialize(std::move(in), &out));

  EXPECT_EQ(in.GetSharedMemory(), nullptr);
  EXPECT_EQ(in.Size(), 0u);
  EXPECT_EQ(out.GetSharedMemory(), nullptr);
  EXPECT_EQ(out.Size(), 3u);

  EXPECT_TRUE(out.AsSpan() == Span(data));
}

TEST(BigBuffer, BigSize)
{
  size_t size = BigBuffer::kShmemThreshold * 2;
  // Generate a large block of random data which will be serialized in a shmem.
  nsTArray<uint8_t> data(size);
  for (size_t i = 0; i < size; ++i) {
    data.AppendElement(mozilla::RandomUint64OrDie());
  }
  BigBuffer in{Span(data)};
  EXPECT_NE(in.GetSharedMemory(), nullptr);
  EXPECT_EQ(in.Size(), size);
  EXPECT_EQ(in.GetSharedMemory()->memory(), in.Data());

  BigBuffer out;
  ASSERT_TRUE(SerializeAndDeserialize(std::move(in), &out));

  EXPECT_EQ(in.GetSharedMemory(), nullptr);
  EXPECT_EQ(in.Size(), 0u);
  EXPECT_NE(out.GetSharedMemory(), nullptr);
  EXPECT_EQ(out.Size(), size);
  EXPECT_EQ(out.GetSharedMemory()->memory(), out.Data());

  EXPECT_TRUE(out.AsSpan() == Span(data));
}

}  // namespace mozilla::ipc
