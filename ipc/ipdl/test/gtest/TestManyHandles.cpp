/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "mozilla/_ipdltest/IPDLUnitTest.h"
#include "mozilla/_ipdltest/PTestManyHandlesChild.h"
#include "mozilla/_ipdltest/PTestManyHandlesParent.h"

using namespace mozilla::ipc;

namespace mozilla::_ipdltest {

class TestManyHandlesChild : public PTestManyHandlesChild {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(TestManyHandlesChild, override)

 public:
  IPCResult RecvManyHandles(nsTArray<FileDescriptor>&& aDescrs) override {
    EXPECT_EQ(aDescrs.Length(), 500u);
    for (int i = 0; i < static_cast<int>(aDescrs.Length()); ++i) {
      UniqueFileHandle handle = aDescrs[i].TakePlatformHandle();
      int value;
      const int size = sizeof(value);
#ifdef XP_WIN
      DWORD numberOfBytesRead;
      EXPECT_TRUE(
          ::ReadFile(handle.get(), &value, size, &numberOfBytesRead, nullptr));
      EXPECT_EQ(numberOfBytesRead, (DWORD)size);
#else
      EXPECT_EQ(read(handle.get(), &value, size), size);
#endif
      EXPECT_EQ(value, i);
    }
    Close();
    return IPC_OK();
  }

 private:
  ~TestManyHandlesChild() = default;
};

class TestManyHandlesParent : public PTestManyHandlesParent {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(TestManyHandlesParent, override)

 private:
  ~TestManyHandlesParent() = default;
};

IPDL_TEST(TestManyHandles) {
  nsTArray<FileDescriptor> descrs;
  for (int i = 0; i < 500; ++i) {
    const int size = sizeof(i);
    UniqueFileHandle readPipe;
    UniqueFileHandle writePipe;
#ifdef XP_WIN
    ASSERT_TRUE(::CreatePipe(getter_Transfers(readPipe),
                             getter_Transfers(writePipe), nullptr, size));
    DWORD numberOfBytesWritten;
    ASSERT_TRUE(
        ::WriteFile(writePipe.get(), &i, size, &numberOfBytesWritten, nullptr));
    ASSERT_EQ(numberOfBytesWritten, (DWORD)size);
#else
    int fds[2];
    ASSERT_EQ(pipe(fds), 0);
    readPipe.reset(fds[0]);
    writePipe.reset(fds[1]);
    ASSERT_EQ(write(writePipe.get(), &i, size), size);
#endif
    descrs.AppendElement(FileDescriptor(std::move(readPipe)));
  }
  bool ok = mActor->SendManyHandles(descrs);
  ASSERT_TRUE(ok);
}

}  // namespace mozilla::_ipdltest
