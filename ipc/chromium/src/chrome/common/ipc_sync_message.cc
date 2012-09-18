// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>
#endif
#include <stack>

#include "base/logging.h"
#include "base/waitable_event.h"
#include "chrome/common/ipc_sync_message.h"

namespace IPC {

uint32_t SyncMessage::next_id_ = 0;
#define kSyncMessageHeaderSize 4

base::WaitableEvent* dummy_event = new base::WaitableEvent(true, true);

SyncMessage::SyncMessage(
    int32_t routing_id,
    uint16_t type,
    PriorityValue priority,
    MessageReplyDeserializer* deserializer)
    : Message(routing_id, type, priority),
      deserializer_(deserializer),
      pump_messages_event_(NULL)
      {
  set_sync();
  set_unblock(true);

  // Add synchronous message data before the message payload.
  SyncHeader header;
  header.message_id = ++next_id_;
  WriteSyncHeader(this, header);
}

MessageReplyDeserializer* SyncMessage::GetReplyDeserializer() {
  MessageReplyDeserializer* rv = deserializer_;
  DCHECK(rv);
  deserializer_ = NULL;
  return rv;
}

void SyncMessage::EnableMessagePumping() {
  DCHECK(!pump_messages_event_);
  set_pump_messages_event(dummy_event);
}

bool SyncMessage::IsMessageReplyTo(const Message& msg, int request_id) {
  if (!msg.is_reply())
    return false;

  return GetMessageId(msg) == request_id;
}

void* SyncMessage::GetDataIterator(const Message* msg) {
  void* iter = const_cast<char*>(msg->payload());
  UpdateIter(&iter, kSyncMessageHeaderSize);
  return iter;
}

int SyncMessage::GetMessageId(const Message& msg) {
  if (!msg.is_sync() && !msg.is_reply())
    return 0;

  SyncHeader header;
  if (!ReadSyncHeader(msg, &header))
    return 0;

  return header.message_id;
}

Message* SyncMessage::GenerateReply(const Message* msg) {
  DCHECK(msg->is_sync());

  Message* reply = new Message(msg->routing_id(), IPC_REPLY_ID,
                               msg->priority());
  reply->set_reply();

  SyncHeader header;

  // use the same message id, but this time reply bit is set
  header.message_id = GetMessageId(*msg);
  WriteSyncHeader(reply, header);

  return reply;
}

bool SyncMessage::ReadSyncHeader(const Message& msg, SyncHeader* header) {
  DCHECK(msg.is_sync() || msg.is_reply());

  void* iter = NULL;
  bool result = msg.ReadInt(&iter, &header->message_id);
  if (!result) {
    NOTREACHED();
    return false;
  }

  return true;
}

bool SyncMessage::WriteSyncHeader(Message* msg, const SyncHeader& header) {
  DCHECK(msg->is_sync() || msg->is_reply());
  DCHECK(msg->payload_size() == 0);
  bool result = msg->WriteInt(header.message_id);
  if (!result) {
    NOTREACHED();
    return false;
  }

  // Note: if you add anything here, you need to update kSyncMessageHeaderSize.
  DCHECK(kSyncMessageHeaderSize == msg->payload_size());

  return true;
}


bool MessageReplyDeserializer::SerializeOutputParameters(const Message& msg) {
  return SerializeOutputParameters(msg, SyncMessage::GetDataIterator(&msg));
}

}  // namespace IPC
