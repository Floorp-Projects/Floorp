// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_IPC_TEST_SINK_H_
#define CHROME_COMMON_IPC_TEST_SINK_H_

#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "chrome/common/ipc_message.h"

namespace IPC {

// This test sink provides a "sink" for IPC messages that are sent. It allows
// the caller to query messages received in various different ways.  It is
// designed for tests for objects that use the IPC system.
//
// Typical usage:
//
//   test_sink.ClearMessages();
//   do_something();
//
//   // We should have gotten exactly one update state message.
//   EXPECT_TRUE(test_sink.GetUniqeMessageMatching(ViewHostMsg_Update::ID));
//   // ...and no start load messages.
//   EXPECT_FALSE(test_sink.GetFirstMessageMatching(ViewHostMsg_Start::ID));
//
//   // Now inspect a message. This assumes a message that was declared like
//   // this: IPC_MESSAGE_ROUTED2(ViewMsg_Foo, bool, int)
//   IPC::Message* msg = test_sink.GetFirstMessageMatching(ViewMsg_Foo::ID));
//   ASSERT_TRUE(msg);
//   bool first_param;
//   int second_param;
//   ViewMsg_Foo::Read(msg, &first_param, &second_param);
//
//   // Go on to the next phase of the test.
//   test_sink.ClearMessages();
//
// To hook up the sink, all you need to do is call OnMessageReceived when a
// message is recieved.
class TestSink {
 public:
  TestSink();
  ~TestSink();

  // Used by the source of the messages to send the message to the sink. This
  // will make a copy of the message and store it in the list.
  void OnMessageReceived(const Message& msg);

  // Returns the number of messages in the queue.
  size_t message_count() const { return messages_.size(); }

  // Clears the message queue of saved messages.
  void ClearMessages();

  // Returns the message at the given index in the queue. The index may be out
  // of range, in which case the return value is NULL. The returned pointer will
  // only be valid until another message is received or the list is cleared.
  const Message* GetMessageAt(size_t index) const;

  // Returns the first message with the given ID in the queue. If there is no
  // message with the given ID, returns NULL. The returned pointer will only be
  // valid until another message is received or the list is cleared.
  const Message* GetFirstMessageMatching(uint16_t id) const;

  // Returns the message with the given ID in the queue. If there is no such
  // message or there is more than one of that message, this will return NULL
  // (with the expectation that you'll do an ASSERT_TRUE() on the result).
  // The returned pointer will only be valid until another message is received
  // or the list is cleared.
  const Message* GetUniqueMessageMatching(uint16_t id) const;

 private:
  // The actual list of received messages.
  std::vector<Message> messages_;

  DISALLOW_COPY_AND_ASSIGN(TestSink);
};

}  // namespace IPC

#endif  // CHROME_COMMON_IPC_TEST_SINK_H_
