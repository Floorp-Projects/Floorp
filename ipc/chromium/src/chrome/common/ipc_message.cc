// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/ipc_message.h"

#include "base/logging.h"
#include "build/build_config.h"

#if defined(OS_POSIX)
#include "chrome/common/file_descriptor_set_posix.h"
#endif

namespace IPC {

//------------------------------------------------------------------------------

Message::~Message() {
}

Message::Message()
    : Pickle(sizeof(Header)) {
  header()->routing = header()->type = header()->flags = 0;
#if defined(OS_POSIX)
  header()->num_fds = 0;
#endif
  InitLoggingVariables();
}

Message::Message(int32 routing_id, msgid_t type, PriorityValue priority,
                 const char* const name)
    : Pickle(sizeof(Header)) {
  header()->routing = routing_id;
  header()->type = type;
  header()->flags = priority;
#if defined(OS_POSIX)
  header()->num_fds = 0;
#endif
  header()->rpc_remote_stack_depth_guess = static_cast<uint32>(-1);
  header()->rpc_local_stack_depth = static_cast<uint32>(-1);
  header()->seqno = 0;
  InitLoggingVariables(name);
}

Message::Message(const char* data, int data_len) : Pickle(data, data_len) {
  InitLoggingVariables();
}

Message::Message(const Message& other) : Pickle(other) {
  InitLoggingVariables(other.name_);
#if defined(OS_POSIX)
  file_descriptor_set_ = other.file_descriptor_set_;
#endif
}

void Message::InitLoggingVariables(const char* const name) {
  name_ = name;
#ifdef IPC_MESSAGE_LOG_ENABLED
  received_time_ = 0;
  dont_log_ = false;
  log_data_ = NULL;
#endif
}

Message& Message::operator=(const Message& other) {
  *static_cast<Pickle*>(this) = other;
  InitLoggingVariables(other.name_);
#if defined(OS_POSIX)
  file_descriptor_set_ = other.file_descriptor_set_;
#endif
  return *this;
}

#ifdef IPC_MESSAGE_LOG_ENABLED
void Message::set_sent_time(int64 time) {
  DCHECK((header()->flags & HAS_SENT_TIME_BIT) == 0);
  header()->flags |= HAS_SENT_TIME_BIT;
  WriteInt64(time);
}

int64 Message::sent_time() const {
  if ((header()->flags & HAS_SENT_TIME_BIT) == 0)
    return 0;

  const char* data = end_of_payload();
  data -= sizeof(int64);
  return *(reinterpret_cast<const int64*>(data));
}

void Message::set_received_time(int64 time) const {
  received_time_ = time;
}
#endif

#if defined(OS_POSIX)
bool Message::WriteFileDescriptor(const base::FileDescriptor& descriptor) {
  // We write the index of the descriptor so that we don't have to
  // keep the current descriptor as extra decoding state when deserialising.
  WriteInt(file_descriptor_set()->size());
  if (descriptor.auto_close) {
    return file_descriptor_set()->AddAndAutoClose(descriptor.fd);
  } else {
    return file_descriptor_set()->Add(descriptor.fd);
  }
}

bool Message::ReadFileDescriptor(void** iter,
                                base::FileDescriptor* descriptor) const {
  int descriptor_index;
  if (!ReadInt(iter, &descriptor_index))
    return false;

  FileDescriptorSet* file_descriptor_set = file_descriptor_set_.get();
  if (!file_descriptor_set)
    return false;

  descriptor->fd = file_descriptor_set->GetDescriptorAt(descriptor_index);
  descriptor->auto_close = false;

  return descriptor->fd >= 0;
}

void Message::EnsureFileDescriptorSet() {
  if (file_descriptor_set_.get() == NULL)
    file_descriptor_set_ = new FileDescriptorSet;
}

#endif

}  // namespace IPC
