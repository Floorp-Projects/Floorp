/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/ipc_message.h"

#include "base/logging.h"
#include "mojo/core/ports/event.h"

#include <utility>

#include "nsISupportsImpl.h"

namespace IPC {

//------------------------------------------------------------------------------

const mojo::core::ports::UserMessage::TypeInfo Message::kUserMessageTypeInfo{};

Message::~Message() { MOZ_COUNT_DTOR(IPC::Message); }

Message::Message(int32_t routing_id, msgid_t type, uint32_t segment_capacity,
                 HeaderFlags flags)
    : UserMessage(&kUserMessageTypeInfo),
      Pickle(sizeof(Header), segment_capacity) {
  MOZ_COUNT_CTOR(IPC::Message);
  header()->routing = routing_id;
  header()->type = type;
  header()->flags = flags;
  header()->num_handles = 0;
  header()->txid = -1;
  header()->seqno = 0;
#if defined(XP_DARWIN)
  header()->cookie = 0;
  header()->num_send_rights = 0;
#endif
  header()->event_footer_size = 0;
}

Message::Message(const char* data, int data_len)
    : UserMessage(&kUserMessageTypeInfo),
      Pickle(sizeof(Header), data, data_len) {
  MOZ_COUNT_CTOR(IPC::Message);
}

/*static*/ mozilla::UniquePtr<Message> Message::IPDLMessage(
    int32_t routing_id, msgid_t type, uint32_t segment_capacity,
    HeaderFlags flags) {
  return mozilla::MakeUnique<Message>(routing_id, type, segment_capacity,
                                      flags);
}

/*static*/ mozilla::UniquePtr<Message> Message::ForSyncDispatchError(
    NestedLevel level) {
  auto m = mozilla::MakeUnique<Message>(0, 0, 0, HeaderFlags(level));
  auto& flags = m->header()->flags;
  flags.SetSync();
  flags.SetReply();
  flags.SetReplyError();
  return m;
}

void Message::WriteFooter(const void* data, uint32_t data_len) {
  if (data_len == 0) {
    return;
  }

  WriteBytes(data, data_len);
}

bool Message::ReadFooter(void* buffer, uint32_t buffer_len, bool truncate) {
  if (buffer_len == 0) {
    return true;
  }

  if (NS_WARN_IF(AlignInt(header()->payload_size) != header()->payload_size) ||
      NS_WARN_IF(AlignInt(buffer_len) > header()->payload_size)) {
    return false;
  }

  // Seek to the start of the footer, and read it in. We read in with a
  // duplicate of the iterator so we can use it to truncate later.
  uint32_t offset = header()->payload_size - AlignInt(buffer_len);
  PickleIterator footer_iter(*this);
  if (NS_WARN_IF(!IgnoreBytes(&footer_iter, offset))) {
    return false;
  }

  PickleIterator read_iter(footer_iter);
  bool ok = ReadBytesInto(&read_iter, buffer, buffer_len);

  // If requested, truncate the buffer to the start of the footer.
  if (truncate) {
    Truncate(&footer_iter);
  }
  return ok;
}

bool Message::WriteFileHandle(mozilla::UniqueFileHandle handle) {
  uint32_t handle_index = attached_handles_.Length();
  WriteUInt32(handle_index);
  if (handle_index == MAX_DESCRIPTORS_PER_MESSAGE) {
    return false;
  }
  attached_handles_.AppendElement(std::move(handle));
  return true;
}

bool Message::ConsumeFileHandle(PickleIterator* iter,
                                mozilla::UniqueFileHandle* handle) const {
  uint32_t handle_index;
  if (!ReadUInt32(iter, &handle_index)) {
    return false;
  }
  if (handle_index >= attached_handles_.Length()) {
    return false;
  }
  // NOTE: This mutates the underlying array, replacing the handle with an
  // invalid handle.
  *handle = std::exchange(attached_handles_[handle_index], nullptr);
  return true;
}

void Message::SetAttachedFileHandles(
    nsTArray<mozilla::UniqueFileHandle> handles) {
  MOZ_DIAGNOSTIC_ASSERT(attached_handles_.IsEmpty());
  attached_handles_ = std::move(handles);
}

uint32_t Message::num_handles() const { return attached_handles_.Length(); }

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

#if defined(XP_DARWIN)
bool Message::WriteMachSendRight(mozilla::UniqueMachSendRight port) {
  uint32_t index = attached_send_rights_.Length();
  WriteUInt32(index);
  if (index == MAX_DESCRIPTORS_PER_MESSAGE) {
    return false;
  }
  attached_send_rights_.AppendElement(std::move(port));
  return true;
}

bool Message::ConsumeMachSendRight(PickleIterator* iter,
                                   mozilla::UniqueMachSendRight* port) const {
  uint32_t index;
  if (!ReadUInt32(iter, &index)) {
    return false;
  }
  if (index >= attached_send_rights_.Length()) {
    return false;
  }
  // NOTE: This mutates the underlying array, replacing the send right with a
  // null right.
  *port = std::exchange(attached_send_rights_[index], nullptr);
  return true;
}

uint32_t Message::num_send_rights() const {
  return attached_send_rights_.Length();
}
#endif

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
