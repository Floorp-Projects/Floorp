/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "chrome/common/ipc_message_utils.h"
#include "mozilla/ipc/SharedMemory.h"
#include "mozilla/ipc/SharedMemoryBasic.h"

namespace IPC {

static uint32_t kShmemThreshold = 64 * 1024;

MessageBufferWriter::MessageBufferWriter(MessageWriter* writer,
                                         uint32_t full_len)
    : writer_(writer) {
  if (full_len > kShmemThreshold) {
    shmem_ = new mozilla::ipc::SharedMemoryBasic();
    if (!shmem_->Create(full_len)) {
      writer->FatalError("SharedMemory::Create failed!");
      return;
    }
    if (!shmem_->Map(full_len)) {
      writer->FatalError("SharedMemory::Map failed");
      return;
    }
    if (!shmem_->WriteHandle(writer)) {
      writer->FatalError("SharedMemory::WriterHandle failed");
      return;
    }
    buffer_ = reinterpret_cast<char*>(shmem_->memory());
  }
  remaining_ = full_len;
}

MessageBufferWriter::~MessageBufferWriter() {
  if (remaining_ != 0) {
    writer_->FatalError("didn't fully write message buffer");
  }
}

bool MessageBufferWriter::WriteBytes(const void* data, uint32_t len) {
  MOZ_RELEASE_ASSERT(len == remaining_ || (len % 4) == 0,
                     "all writes except for the final write must be a multiple "
                     "of 4 bytes in length due to padding");
  if (len > remaining_) {
    writer_->FatalError("MessageBufferWriter overrun");
    return false;
  }
  remaining_ -= len;
  // If we're serializing using a shared memory region, `buffer_` will be
  // initialized to point into that region.
  if (buffer_) {
    memcpy(buffer_, data, len);
    buffer_ += len;
    return true;
  }
  return writer_->WriteBytes(data, len);
}

MessageBufferReader::MessageBufferReader(MessageReader* reader,
                                         uint32_t full_len)
    : reader_(reader) {
  if (full_len > kShmemThreshold) {
    shmem_ = new mozilla::ipc::SharedMemoryBasic();
    if (!shmem_->ReadHandle(reader)) {
      reader->FatalError("SharedMemory::ReadHandle failed!");
      return;
    }
    if (!shmem_->Map(full_len)) {
      reader->FatalError("SharedMemory::Map failed");
      return;
    }
    buffer_ = reinterpret_cast<const char*>(shmem_->memory());
  }
  remaining_ = full_len;
}

MessageBufferReader::~MessageBufferReader() {
  if (remaining_ != 0) {
    reader_->FatalError("didn't fully write message buffer");
  }
}

bool MessageBufferReader::ReadBytesInto(void* data, uint32_t len) {
  MOZ_RELEASE_ASSERT(len == remaining_ || (len % 4) == 0,
                     "all reads except for the final read must be a multiple "
                     "of 4 bytes in length due to padding");
  if (len > remaining_) {
    reader_->FatalError("MessageBufferReader overrun");
    return false;
  }
  remaining_ -= len;
  // If we're serializing using a shared memory region, `buffer_` will be
  // initialized to point into that region.
  if (buffer_) {
    memcpy(data, buffer_, len);
    buffer_ += len;
    return true;
  }
  return reader_->ReadBytesInto(data, len);
}

}  // namespace IPC
