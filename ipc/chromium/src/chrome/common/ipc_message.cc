/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/ipc_message.h"

#include "base/logging.h"
#include "build/build_config.h"

#if defined(OS_POSIX)
#  include "chrome/common/file_descriptor_set_posix.h"
#endif

#include <utility>

#include "nsISupportsImpl.h"

namespace IPC {

//------------------------------------------------------------------------------

Message::~Message() { MOZ_COUNT_DTOR(IPC::Message); }

Message::Message() : Pickle(sizeof(Header)) {
  MOZ_COUNT_CTOR(IPC::Message);
  header()->routing = header()->type = 0;
#if defined(OS_POSIX)
  header()->num_fds = 0;
#endif
}

Message::Message(int32_t routing_id, msgid_t type, uint32_t segment_capacity,
                 HeaderFlags flags, bool recordWriteLatency)
    : Pickle(sizeof(Header), segment_capacity) {
  MOZ_COUNT_CTOR(IPC::Message);
  header()->routing = routing_id;
  header()->type = type;
  header()->flags = flags;
#if defined(OS_POSIX)
  header()->num_fds = 0;
#endif
  header()->interrupt_remote_stack_depth_guess = static_cast<uint32_t>(-1);
  header()->interrupt_local_stack_depth = static_cast<uint32_t>(-1);
  header()->seqno = 0;
#if defined(OS_MACOSX)
  header()->cookie = 0;
#endif
  if (recordWriteLatency) {
    create_time_ = mozilla::TimeStamp::Now();
  }
}

Message::Message(const char* data, int data_len)
    : Pickle(sizeof(Header), data, data_len) {
  MOZ_COUNT_CTOR(IPC::Message);
}

Message::Message(Message&& other) : Pickle(std::move(other)) {
  MOZ_COUNT_CTOR(IPC::Message);
#if defined(OS_POSIX)
  file_descriptor_set_ = std::move(other.file_descriptor_set_);
#endif
}

/*static*/ Message* Message::IPDLMessage(int32_t routing_id, msgid_t type,
                                         HeaderFlags flags) {
  return new Message(routing_id, type, 0, flags, true);
}

/*static*/ Message* Message::ForSyncDispatchError(NestedLevel level) {
  auto* m = new Message(0, 0, 0, HeaderFlags(level));
  auto& flags = m->header()->flags;
  flags.SetSync();
  flags.SetReply();
  flags.SetReplyError();
  return m;
}

/*static*/ Message* Message::ForInterruptDispatchError() {
  auto* m = new Message();
  auto& flags = m->header()->flags;
  flags.SetInterrupt();
  flags.SetReply();
  flags.SetReplyError();
  return m;
}

Message& Message::operator=(Message&& other) {
  *static_cast<Pickle*>(this) = std::move(other);
#if defined(OS_POSIX)
  file_descriptor_set_.swap(other.file_descriptor_set_);
#endif
  return *this;
}

void Message::CopyFrom(const Message& other) {
  Pickle::CopyFrom(other);
#if defined(OS_POSIX)
  MOZ_ASSERT(!file_descriptor_set_);
  if (other.file_descriptor_set_) {
    file_descriptor_set_ = new FileDescriptorSet;
    file_descriptor_set_->CopyFrom(*other.file_descriptor_set_);
  }
#endif
}

#if defined(OS_POSIX)
bool Message::WriteFileDescriptor(const base::FileDescriptor& descriptor) {
  // We write the index of the descriptor so that we don't have to
  // keep the current descriptor as extra decoding state when deserialising.
  // Also, we rely on each file descriptor being accompanied by sizeof(int)
  // bytes of data in the message. See the comment for input_cmsg_buf_.
  WriteInt(file_descriptor_set()->size());
  if (descriptor.auto_close) {
    return file_descriptor_set()->AddAndAutoClose(descriptor.fd);
  } else {
    return file_descriptor_set()->Add(descriptor.fd);
  }
}

bool Message::ReadFileDescriptor(PickleIterator* iter,
                                 base::FileDescriptor* descriptor) const {
  int descriptor_index;
  if (!ReadInt(iter, &descriptor_index)) return false;

  FileDescriptorSet* file_descriptor_set = file_descriptor_set_.get();
  if (!file_descriptor_set) return false;

  descriptor->fd = file_descriptor_set->GetDescriptorAt(descriptor_index);
  descriptor->auto_close = false;

  return descriptor->fd >= 0;
}

void Message::EnsureFileDescriptorSet() {
  if (file_descriptor_set_.get() == NULL)
    file_descriptor_set_ = new FileDescriptorSet;
}

uint32_t Message::num_fds() const {
  return file_descriptor_set() ? file_descriptor_set()->size() : 0;
}

#endif

void Message::AssertAsLargeAsHeader() const {
  MOZ_DIAGNOSTIC_ASSERT(size() >= sizeof(Header));
  MOZ_DIAGNOSTIC_ASSERT(CurrentSize() >= sizeof(Header));
  // Our buffers should agree with what our header specifies.
  MOZ_DIAGNOSTIC_ASSERT(size() == CurrentSize());
}

}  // namespace IPC
