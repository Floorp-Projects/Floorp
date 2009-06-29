// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_IPC_TESTS_H__
#define CHROME_COMMON_IPC_TESTS_H__

#include "base/multiprocess_test.h"
#include "base/process.h"

// This unit test uses 3 types of child processes, a regular pipe client,
// a client reflector and a IPC server used for fuzzing tests.
enum ChildType {
  TEST_CLIENT,
  TEST_DESCRIPTOR_CLIENT,
  TEST_DESCRIPTOR_CLIENT_SANDBOXED,
  TEST_REFLECTOR,
  FUZZER_SERVER
};

// The different channel names for the child processes.
extern const wchar_t kTestClientChannel[];
extern const wchar_t kReflectorChannel[];
extern const wchar_t kFuzzerChannel[];

class MessageLoopForIO;
namespace IPC {
class Channel;
}  // namespace IPC

//Base class to facilitate Spawning IPC Client processes.
class IPCChannelTest : public MultiProcessTest {
 protected:

  // Create a new MessageLoopForIO For each test.
  virtual void SetUp();
  virtual void TearDown();

  // Spawns a child process of the specified type
  base::ProcessHandle SpawnChild(ChildType child_type,
                                 IPC::Channel *channel);

  // Created around each test instantiation.
  MessageLoopForIO *message_loop_;
};

#endif  // CHROME_COMMON_IPC_TESTS_H__
