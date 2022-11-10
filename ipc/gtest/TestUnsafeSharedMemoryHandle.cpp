/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "chrome/common/ipc_message.h"
#include "chrome/common/ipc_message_utils.h"
#include "gtest/gtest.h"

#include "mozilla/RandomNum.h"
#include "mozilla/ipc/RawShmem.h"

namespace mozilla::ipc {

static bool SerializeAndDeserialize(UnsafeSharedMemoryHandle&& aIn,
                                    UnsafeSharedMemoryHandle* aOut) {
  IPC::Message msg(MSG_ROUTING_NONE, 0);
  {
    IPC::MessageWriter writer(msg);
    IPC::WriteParam(&writer, std::move(aIn));
  }

  IPC::MessageReader reader(msg);
  return IPC::ReadParam(&reader, aOut);
}

void TestUnsafeSharedMemoryHandle(size_t aSize) {
  auto maybeHandle = UnsafeSharedMemoryHandle::CreateAndMap(aSize);
  ASSERT_TRUE(maybeHandle);
  auto [handle, mapping] = maybeHandle.extract();

  EXPECT_EQ(mapping.Size(), aSize);

  auto inBytes = mapping.Bytes();
  for (size_t i = 0; i < aSize; ++i) {
    inBytes[i] = static_cast<uint8_t>(i % 255);
  }

  UnsafeSharedMemoryHandle out;
  ASSERT_TRUE(SerializeAndDeserialize(std::move(handle), &out));

  auto mapping2 = WritableSharedMemoryMapping::Open(std::move(out)).value();

  EXPECT_EQ(mapping2.Size(), aSize);
  auto outBytes = mapping2.Bytes();
  for (size_t i = 0; i < aSize; ++i) {
    EXPECT_EQ(outBytes[i], static_cast<uint8_t>(i % 255));
  }
}

TEST(UnsafeSharedMemoryHandle, Empty)
{ TestUnsafeSharedMemoryHandle(0); }

TEST(UnsafeSharedMemoryHandle, SmallSize)
{ TestUnsafeSharedMemoryHandle(2048); }

}  // namespace mozilla::ipc
