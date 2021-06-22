/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/ipc_message.h"

#include "base/logging.h"
#include "build/build_config.h"
#include "mojo/core/ports/event.h"

#if defined(OS_POSIX)
#  include "chrome/common/file_descriptor_set_posix.h"
#endif

#include <utility>

#include "nsISupportsImpl.h"

namespace IPC {

//------------------------------------------------------------------------------

const mojo::core::ports::UserMessage::TypeInfo Message::kUserMessageTypeInfo{};

Message::~Message() { MOZ_COUNT_DTOR(IPC::Message); }

Message::Message()
    : UserMessage(&kUserMessageTypeInfo), Pickle(sizeof(Header)) {
  MOZ_COUNT_CTOR(IPC::Message);
  header()->routing = header()->type = 0;
#if defined(OS_POSIX)
  header()->num_fds = 0;
#endif
}

Message::Message(int32_t routing_id, msgid_t type, uint32_t segment_capacity,
                 HeaderFlags flags, bool recordWriteLatency)
    : UserMessage(&kUserMessageTypeInfo),
      Pickle(sizeof(Header), segment_capacity) {
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
  header()->footer_offset = -1;
  if (recordWriteLatency) {
    create_time_ = mozilla::TimeStamp::Now();
  }
}

Message::Message(const char* data, int data_len)
    : UserMessage(&kUserMessageTypeInfo),
      Pickle(sizeof(Header), data, data_len) {
  MOZ_COUNT_CTOR(IPC::Message);
}

Message::Message(Message&& other)
    : UserMessage(&kUserMessageTypeInfo),
      Pickle(std::move(other)),
      attached_ports_(std::move(other.attached_ports_)) {
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
  attached_ports_ = std::move(other.attached_ports_);
#if defined(OS_POSIX)
  file_descriptor_set_.swap(other.file_descriptor_set_);
#endif
  return *this;
}

void Message::WriteFooter(const void* data, uint32_t data_len) {
  MOZ_ASSERT(header()->footer_offset < 0, "Already wrote a footer!");
  if (data_len == 0) {
    return;
  }

  // Record the start of the footer.
  header()->footer_offset = header()->payload_size;

  WriteBytes(data, data_len);
}

bool Message::ReadFooter(void* buffer, uint32_t buffer_len, bool truncate) {
  MOZ_ASSERT(buffer_len == FooterSize());
  MOZ_ASSERT(header()->footer_offset <= int64_t(header()->payload_size));
  if (buffer_len == 0) {
    return true;
  }

  // FIXME: This is a really inefficient way to seek to the end of the message
  // for sufficiently large messages.
  PickleIterator footer_iter(*this);
  if (NS_WARN_IF(!IgnoreBytes(&footer_iter, header()->footer_offset))) {
    return false;
  }

  // Use a copy of the footer iterator for reading bytes so that we can use the
  // previous iterator to truncate the message if requested.
  PickleIterator read_iter(footer_iter);
  bool ok = ReadBytesInto(&read_iter, buffer, buffer_len);

  // If requested, truncate the buffer to the start of the footer, and clear our
  // footer offset back to `-1`.
  if (truncate) {
    header()->footer_offset = -1;
    Truncate(&footer_iter);
  }

  return ok;
}

uint32_t Message::FooterSize() const {
  if (header()->footer_offset >= 0 &&
      uint32_t(header()->footer_offset) < header()->payload_size) {
    return header()->payload_size - header()->footer_offset;
  }
  return 0;
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

void Message::WritePort(mozilla::ipc::ScopedPort port) {
  uint32_t port_index = attached_ports_.Length();
  WriteUInt32(port_index);
  attached_ports_.AppendElement(std::move(port));
}

bool Message::ConsumePort(PickleIterator* iter,
                          mozilla::ipc::ScopedPort* port) const {
  uint32_t port_index;
  if (!ReadUInt32(iter, &port_index)) {
    return false;
  }
  if (port_index >= attached_ports_.Length()) {
    return false;
  }
  // NOTE: This mutates the underlying array, replacing the port with a consumed
  // port.
  *port = std::exchange(attached_ports_[port_index], {});
  return true;
}

void Message::SetAttachedPorts(nsTArray<mozilla::ipc::ScopedPort> ports) {
  MOZ_DIAGNOSTIC_ASSERT(attached_ports_.IsEmpty());
  attached_ports_ = std::move(ports);
}

bool Message::WillBeRoutedExternally(
    mojo::core::ports::UserMessageEvent& event) {
  if (!attached_ports_.IsEmpty()) {
    // Explicitly attach any ports which were attached to this Message to this
    // UserMessageEvent before we route it externally so that they can be
    // transferred correctly. These ports will be recovered if needed in
    // `GetMessage`.
    MOZ_DIAGNOSTIC_ASSERT(
        event.num_ports() == 0,
        "Must not have previously attached ports to the UserMessageEvent");
    event.ReservePorts(attached_ports_.Length());
    for (size_t i = 0; i < event.num_ports(); ++i) {
      event.ports()[i] = attached_ports_[i].Release().name();
    }
    attached_ports_.Clear();
  }
  return true;
}

void Message::AssertAsLargeAsHeader() const {
  MOZ_DIAGNOSTIC_ASSERT(size() >= sizeof(Header));
  MOZ_DIAGNOSTIC_ASSERT(CurrentSize() >= sizeof(Header));
  // Our buffers should agree with what our header specifies.
  MOZ_DIAGNOSTIC_ASSERT(size() == CurrentSize());
}

}  // namespace IPC
