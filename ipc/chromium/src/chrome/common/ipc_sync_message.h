// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_IPC_SYNC_MESSAGE_H__
#define CHROME_COMMON_IPC_SYNC_MESSAGE_H__

#if defined(OS_WIN)
#include <windows.h>
#endif
#include <string>
#include "base/basictypes.h"
#include "chrome/common/ipc_message.h"

namespace base {
class WaitableEvent;
}

namespace IPC {

class MessageReplyDeserializer;

class SyncMessage : public Message {
 public:
  SyncMessage(int32_t routing_id, uint16_t type, PriorityValue priority,
              MessageReplyDeserializer* deserializer);

  // Call this to get a deserializer for the output parameters.
  // Note that this can only be called once, and the caller is responsible
  // for deleting the deserializer when they're done.
  MessageReplyDeserializer* GetReplyDeserializer();

  // If this message can cause the receiver to block while waiting for user
  // input (i.e. by calling MessageBox), then the caller needs to pump window
  // messages and dispatch asynchronous messages while waiting for the reply.
  // If this event is passed in, then window messages will start being pumped
  // when it's set.  Note that this behavior will continue even if the event is
  // later reset.  The event must be valid until after the Send call returns.
  void set_pump_messages_event(base::WaitableEvent* event) {
    pump_messages_event_ = event;
    if (event) {
      header()->flags |= PUMPING_MSGS_BIT;
    } else {
      header()->flags &= ~PUMPING_MSGS_BIT;
    }
  }

  // Call this if you always want to pump messages.  You can call this method
  // or set_pump_messages_event but not both.
  void EnableMessagePumping();

  base::WaitableEvent* pump_messages_event() const {
    return pump_messages_event_;
  }

  // Returns true if the message is a reply to the given request id.
  static bool IsMessageReplyTo(const Message& msg, int request_id);

  // Given a reply message, returns an iterator to the beginning of the data
  // (i.e. skips over the synchronous specific data).
  static void* GetDataIterator(const Message* msg);

  // Given a synchronous message (or its reply), returns its id.
  static int GetMessageId(const Message& msg);

  // Generates a reply message to the given message.
  static Message* GenerateReply(const Message* msg);

 private:
  struct SyncHeader {
    // unique ID (unique per sender)
    int message_id;
  };

  static bool ReadSyncHeader(const Message& msg, SyncHeader* header);
  static bool WriteSyncHeader(Message* msg, const SyncHeader& header);

  MessageReplyDeserializer* deserializer_;
  base::WaitableEvent* pump_messages_event_;

  static uint32_t next_id_;  // for generation of unique ids
};

// Used to deserialize parameters from a reply to a synchronous message
class MessageReplyDeserializer {
 public:
  bool SerializeOutputParameters(const Message& msg);
  virtual ~MessageReplyDeserializer() {}
 private:
  // Derived classes need to implement this, using the given iterator (which
  // is skipped past the header for synchronous messages).
  virtual bool SerializeOutputParameters(const Message& msg, void* iter) = 0;
};

}  // namespace IPC

#endif  // CHROME_COMMON_IPC_SYNC_MESSAGE_H__
