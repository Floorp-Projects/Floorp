/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_SHARED_MEMORY_H_
#define BASE_SHARED_MEMORY_H_

#include "build/build_config.h"

#if defined(OS_POSIX)
#include <sys/types.h>
#include <semaphore.h>
#include "base/file_descriptor_posix.h"
#endif
#include <string>

#include "base/basictypes.h"
#include "base/process.h"

namespace base {

// SharedMemoryHandle is a platform specific type which represents
// the underlying OS handle to a shared memory segment.
#if defined(OS_WIN)
typedef HANDLE SharedMemoryHandle;
#elif defined(OS_POSIX)
typedef FileDescriptor SharedMemoryHandle;
#endif

// Platform abstraction for shared memory.  Provides a C++ wrapper
// around the OS primitive for a memory mapped file.
class SharedMemory {
 public:
  // Create a new SharedMemory object.
  SharedMemory();

  // Create a new SharedMemory object from an existing, open
  // shared memory file.
  SharedMemory(SharedMemoryHandle init_handle, bool read_only)
    : SharedMemory() {
    SetHandle(init_handle, read_only);
  }

  // Destructor.  Will close any open files.
  ~SharedMemory();

  // Initialize a new SharedMemory object from an existing, open
  // shared memory file.
  bool SetHandle(SharedMemoryHandle handle, bool read_only);

  // Return true iff the given handle is valid (i.e. not the distingished
  // invalid value; NULL for a HANDLE and -1 for a file descriptor)
  static bool IsHandleValid(const SharedMemoryHandle& handle);

  // Return invalid handle (see comment above for exact definition).
  static SharedMemoryHandle NULLHandle();

  // Creates a shared memory segment.
  // Returns true on success, false on failure.
  bool Create(size_t size);

  // Maps the shared memory into the caller's address space.
  // Returns true on success, false otherwise.  The memory address
  // is accessed via the memory() accessor.
  bool Map(size_t bytes);

  // Unmaps the shared memory from the caller's address space.
  // Returns true if successful; returns false on error or if the
  // memory is not mapped.
  bool Unmap();

  // Get the size of the opened shared memory backing file.
  // Note:  This size is only available to the creator of the
  // shared memory, and not to those that opened shared memory
  // created externally.
  // Returns 0 if not opened or unknown.
  size_t max_size() const { return max_size_; }

  // Gets a pointer to the opened memory space if it has been
  // Mapped via Map().  Returns NULL if it is not mapped.
  void *memory() const { return memory_; }

  // Get access to the underlying OS handle for this segment.
  // Use of this handle for anything other than an opaque
  // identifier is not portable.
  SharedMemoryHandle handle() const;

  // Closes the open shared memory segment.
  // It is safe to call Close repeatedly.
  void Close(bool unmap_view = true);

  // Share the shared memory to another process.  Attempts
  // to create a platform-specific new_handle which can be
  // used in a remote process to access the shared memory
  // file.  new_handle is an ouput parameter to receive
  // the handle for use in the remote process.
  // Returns true on success, false otherwise.
  bool ShareToProcess(base::ProcessId target_pid,
                      SharedMemoryHandle* new_handle) {
    return ShareToProcessCommon(target_pid, new_handle, false);
  }

  // Logically equivalent to:
  //   bool ok = ShareToProcess(process, new_handle);
  //   Close();
  //   return ok;
  // Note that the memory is unmapped by calling this method, regardless of the
  // return value.
  bool GiveToProcess(ProcessId target_pid,
                     SharedMemoryHandle* new_handle) {
    return ShareToProcessCommon(target_pid, new_handle, true);
  }

#ifdef OS_POSIX
  // If named POSIX shm is being used, append the prefix (including
  // the leading '/') that would be used by a process with the given
  // pid to the given string and return true.  If not, return false.
  // (This is public so that the Linux sandboxing code can use it.)
  static bool AppendPosixShmPrefix(std::string* str, pid_t pid);
#endif

 private:
  bool ShareToProcessCommon(ProcessId target_pid,
                            SharedMemoryHandle* new_handle,
                            bool close_self);

#if defined(OS_WIN)
  HANDLE             mapped_file_;
#elif defined(OS_POSIX)
  int                mapped_file_;
#endif
  void*              memory_;
  bool               read_only_;
  size_t             max_size_;

  DISALLOW_EVIL_CONSTRUCTORS(SharedMemory);
};

}  // namespace base

#endif  // BASE_SHARED_MEMORY_H_
