/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/ipc_message.h"

#include "base/logging.h"
#include "build/build_config.h"

#if defined(OS_POSIX)
#include "chrome/common/file_descriptor_set_posix.h"
#endif
#ifdef MOZ_TASK_TRACER
#include "GeckoTaskTracerImpl.h"
#endif

#include "mozilla/Move.h"

#ifdef MOZ_TASK_TRACER
using namespace mozilla::tasktracer;

#define MSG_HEADER_SZ (GetOrCreateTraceInfo() == nullptr ?              \
                       sizeof(Header) : sizeof(HeaderTaskTracer))
#else
#define MSG_HEADER_SZ sizeof(Header)
#endif

namespace IPC {

//------------------------------------------------------------------------------

Message::~Message() {
  MOZ_COUNT_DTOR(IPC::Message);
}

Message::Message()
    : Pickle(MSG_HEADER_SZ) {
  MOZ_COUNT_CTOR(IPC::Message);
  header()->routing = header()->type = header()->flags = 0;
#if defined(OS_POSIX)
  header()->num_fds = 0;
#endif
#ifdef MOZ_TASK_TRACER
  if (UseTaskTracerHeader()) {
    header()->flags |= TASKTRACER_BIT;
    HeaderTaskTracer* _header = static_cast<HeaderTaskTracer*>(header());
    GetCurTraceInfo(&_header->source_event_id,
                    &_header->parent_task_id,
                    &_header->source_event_type);
  }
#endif
  InitLoggingVariables();
}

Message::Message(int32_t routing_id, msgid_t type, NestedLevel nestedLevel, PriorityValue priority,
                 MessageCompression compression, const char* const aName)
    : Pickle(MSG_HEADER_SZ) {
  MOZ_COUNT_CTOR(IPC::Message);
  header()->routing = routing_id;
  header()->type = type;
  header()->flags = nestedLevel;
  if (priority == HIGH_PRIORITY)
    header()->flags |= PRIO_BIT;
  if (compression == COMPRESSION_ENABLED)
    header()->flags |= COMPRESS_BIT;
  else if (compression == COMPRESSION_ALL)
    header()->flags |= COMPRESSALL_BIT;
#if defined(OS_POSIX)
  header()->num_fds = 0;
#endif
  header()->interrupt_remote_stack_depth_guess = static_cast<uint32_t>(-1);
  header()->interrupt_local_stack_depth = static_cast<uint32_t>(-1);
  header()->seqno = 0;
#if defined(OS_MACOSX)
  header()->cookie = 0;
#endif
#ifdef MOZ_TASK_TRACER
  if (UseTaskTracerHeader()) {
    header()->flags |= TASKTRACER_BIT;
    HeaderTaskTracer* _header = static_cast<HeaderTaskTracer*>(header());
    GetCurTraceInfo(&_header->source_event_id,
                    &_header->parent_task_id,
                    &_header->source_event_type);
  }
#endif
  InitLoggingVariables(aName);
}

#ifndef MOZ_TASK_TRACER
#define MSG_HEADER_SZ_DATA sizeof(Header)
#else
#define MSG_HEADER_SZ_DATA                                            \
  (reinterpret_cast<const Header*>(data)->flags & TASKTRACER_BIT ?  \
   sizeof(HeaderTaskTracer) : sizeof(Header))
#endif

Message::Message(const char* data, int data_len)
  : Pickle(MSG_HEADER_SZ_DATA, data, data_len)
{
  MOZ_COUNT_CTOR(IPC::Message);
  InitLoggingVariables();
}

Message::Message(Message&& other) : Pickle(mozilla::Move(other)) {
  MOZ_COUNT_CTOR(IPC::Message);
  InitLoggingVariables(other.name_);
#if defined(OS_POSIX)
  file_descriptor_set_ = other.file_descriptor_set_.forget();
#endif
}

void Message::InitLoggingVariables(const char* const aName) {
  name_ = aName;
}

Message& Message::operator=(Message&& other) {
  *static_cast<Pickle*>(this) = mozilla::Move(other);
  InitLoggingVariables(other.name_);
#if defined(OS_POSIX)
  file_descriptor_set_.swap(other.file_descriptor_set_);
#endif
  return *this;
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

uint32_t Message::num_fds() const {
  return file_descriptor_set() ? file_descriptor_set()->size() : 0;
}

#endif

#ifdef MOZ_TASK_TRACER
void *MessageTask() {
  return reinterpret_cast<void*>(&MessageTask);
}

void
Message::TaskTracerDispatch() {
  if (header()->flags & TASKTRACER_BIT) {
    HeaderTaskTracer* _header = static_cast<HeaderTaskTracer*>(header());
    _header->task_id = GenNewUniqueTaskId();
    uintptr_t* vtab = reinterpret_cast<uintptr_t*>(&MessageTask);
    LogVirtualTablePtr(_header->task_id,
                       _header->source_event_id,
                       vtab);
    LogDispatch(_header->task_id,
                _header->parent_task_id,
                _header->source_event_id,
                _header->source_event_type);
  }
}

Message::AutoTaskTracerRun::AutoTaskTracerRun(Message& aMsg)
  : mMsg(aMsg)
  , mTaskId(0)
  , mSourceEventId(0) {
  if (mMsg.header()->flags & TASKTRACER_BIT) {
    const HeaderTaskTracer* _header =
      static_cast<HeaderTaskTracer*>(mMsg.header());
    LogBegin(_header->task_id,
             _header->source_event_id);
    SetCurTraceInfo(_header->source_event_id,
                    _header->task_id,
                    _header->source_event_type);
    mTaskId = _header->task_id;
    mSourceEventId = _header->source_event_id;
  } else {
    SetCurTraceInfo(0, 0, SourceEventType::Unknown);
  }
}

Message::AutoTaskTracerRun::~AutoTaskTracerRun() {
  if (mTaskId) {
    AddLabel("IPC Message %s", mMsg.name());
    LogEnd(mTaskId, mSourceEventId);
  }
}
#endif

}  // namespace IPC
