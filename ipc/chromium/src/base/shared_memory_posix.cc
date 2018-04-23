/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/shared_memory.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "base/file_util.h"
#include "base/logging.h"
#include "base/platform_thread.h"
#include "base/string_util.h"
#include "mozilla/UniquePtr.h"

namespace base {

SharedMemory::SharedMemory()
    : mapped_file_(-1),
      memory_(NULL),
      read_only_(false),
      max_size_(0) {
}

SharedMemory::~SharedMemory() {
  Close();
}

bool SharedMemory::SetHandle(SharedMemoryHandle handle, bool read_only) {
  DCHECK(mapped_file_ == -1);

  mapped_file_ = handle.fd;
  read_only_ = read_only;
  return true;
}

// static
bool SharedMemory::IsHandleValid(const SharedMemoryHandle& handle) {
  return handle.fd >= 0;
}

// static
SharedMemoryHandle SharedMemory::NULLHandle() {
  return SharedMemoryHandle();
}

namespace {

// A class to handle auto-closing of FILE*'s.
class ScopedFILEClose {
 public:
  inline void operator()(FILE* x) const {
    if (x) {
      fclose(x);
    }
  }
};

typedef mozilla::UniquePtr<FILE, ScopedFILEClose> ScopedFILE;

}

bool SharedMemory::Create(size_t size) {
  read_only_ = false;

  DCHECK(size > 0);
  DCHECK(mapped_file_ == -1);

  ScopedFILE file_closer;
  FILE *fp;

  FilePath path;
  fp = file_util::CreateAndOpenTemporaryShmemFile(&path);

  // Deleting the file prevents anyone else from mapping it in
  // (making it private), and prevents the need for cleanup (once
  // the last fd is closed, it is truly freed).
  file_util::Delete(path);

  if (fp == NULL)
    return false;
  file_closer.reset(fp);  // close when we go out of scope

  // Set the file size.
  //
  // According to POSIX, (f)truncate can be used to extend files;
  // previously this required the XSI option but as of the 2008
  // edition it's required for everything.  (Linux documents that this
  // may fail on non-"native" filesystems like FAT, but /dev/shm
  // should always be tmpfs.)
  if (ftruncate(fileno(fp), size) != 0)
    return false;
  // This probably isn't needed.
  if (fseeko(fp, size, SEEK_SET) != 0)
    return false;

  mapped_file_ = dup(fileno(fp));
  DCHECK(mapped_file_ >= 0);

  max_size_ = size;
  return true;
}

bool SharedMemory::Map(size_t bytes) {
  if (mapped_file_ == -1)
    return false;

  memory_ = mmap(NULL, bytes, PROT_READ | (read_only_ ? 0 : PROT_WRITE),
                 MAP_SHARED, mapped_file_, 0);

  if (memory_)
    max_size_ = bytes;

  bool mmap_succeeded = (memory_ != (void*)-1);
  DCHECK(mmap_succeeded) << "Call to mmap failed, errno=" << errno;
  return mmap_succeeded;
}

bool SharedMemory::Unmap() {
  if (memory_ == NULL)
    return false;

  munmap(memory_, max_size_);
  memory_ = NULL;
  max_size_ = 0;
  return true;
}

bool SharedMemory::ShareToProcessCommon(ProcessId processId,
                                        SharedMemoryHandle *new_handle,
                                        bool close_self) {
  const int new_fd = dup(mapped_file_);
  DCHECK(new_fd >= -1);
  new_handle->fd = new_fd;
  new_handle->auto_close = true;

  if (close_self)
    Close();

  return true;
}


void SharedMemory::Close(bool unmap_view) {
  if (unmap_view) {
    Unmap();
  }

  if (mapped_file_ >= 0) {
    close(mapped_file_);
    mapped_file_ = -1;
  }
}

SharedMemoryHandle SharedMemory::handle() const {
  return FileDescriptor(mapped_file_, false);
}

}  // namespace base
