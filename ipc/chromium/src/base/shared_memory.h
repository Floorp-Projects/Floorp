/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_SHARED_MEMORY_H_
#define BASE_SHARED_MEMORY_H_

#include "build/build_config.h"

#if defined(OS_POSIX)
#  include <sys/types.h>
#  include <semaphore.h>
#  include "base/file_descriptor_posix.h"
#endif
#include <string>

#include "base/basictypes.h"
#include "base/process.h"
#include "mozilla/Attributes.h"
#include "mozilla/UniquePtrExtensions.h"

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
  SharedMemory() = default;

  // Create a new SharedMemory object from an existing, open
  // shared memory file.
  SharedMemory(SharedMemoryHandle init_handle, bool read_only)
      : SharedMemory() {
    SetHandle(init_handle, read_only);
  }

  // Move constructor; transfers ownership.
  SharedMemory(SharedMemory&& other) = default;

  // Destructor.  Will close any open files.
  ~SharedMemory();

  // Initialize a new SharedMemory object from an existing, open
  // shared memory file.
  bool SetHandle(SharedMemoryHandle handle, bool read_only);

  // Return true iff the given handle is valid (i.e. not the distingished
  // invalid value; NULL for a HANDLE and -1 for a file descriptor)
  static bool IsHandleValid(const SharedMemoryHandle& handle);

  // IsHandleValid applied to this object's handle.
  bool IsValid() const { return static_cast<bool>(mapped_file_); }

  // Return invalid handle (see comment above for exact definition).
  static SharedMemoryHandle NULLHandle();

  // Creates a shared memory segment.
  // Returns true on success, false on failure.
  bool Create(size_t size) { return CreateInternal(size, false); }

  // Creates a shared memory segment that supports the Freeze()
  // method; see below.  (Warning: creating freezeable shared memory
  // within a sandboxed process isn't possible on some platforms.)
  // Returns true on success, false on failure.
  bool CreateFreezeable(size_t size) { return CreateInternal(size, true); }

  // Maps the shared memory into the caller's address space.
  // Returns true on success, false otherwise.  The memory address
  // is accessed via the memory() accessor.
  //
  // If the specified fixed address is not null, it is the address that the
  // shared memory must be mapped at.  Returns false if the shared memory
  // could not be mapped at that address.
  bool Map(size_t bytes, void* fixed_address = nullptr);

  // Unmaps the shared memory from the caller's address space.
  void Unmap() { memory_ = nullptr; }

  // Get the size of the opened shared memory backing file.
  // Note:  This size is only available to the creator of the
  // shared memory, and not to those that opened shared memory
  // created externally.
  // Returns 0 if not opened or unknown.
  size_t max_size() const { return max_size_; }

  // Gets a pointer to the opened memory space if it has been
  // Mapped via Map().  Returns NULL if it is not mapped.
  void* memory() const { return memory_.get(); }

  // Extracts the underlying file handle; similar to
  // GiveToProcess(GetCurrentProcId(), ...) but returns a RAII type.
  // Like GiveToProcess, this unmaps the memory as a side-effect (and
  // cleans up any OS-specific resources).
  mozilla::UniqueFileHandle TakeHandle() {
    mozilla::UniqueFileHandle handle = std::move(mapped_file_);
    Close();
    return handle;
  }

#ifdef OS_WIN
  // Used only in gfx/ipc/SharedDIBWin.cpp; should be removable once
  // NPAPI goes away.
  HANDLE GetHandle() {
    freezeable_ = false;
    return mapped_file_.get();
  }
#endif

  // Make the shared memory object read-only, such that it cannot be
  // written even if it's sent to an untrusted process.  If it was
  // mapped in this process, it will be unmapped.  The object must
  // have been created with CreateFreezeable(), and must not have
  // already been shared to another process.
  //
  // (See bug 1479960 comment #0 for OS-specific implementation
  // details.)
  [[nodiscard]] bool Freeze() {
    Unmap();
    return ReadOnlyCopy(this);
  }

  // Similar to Freeze(), but assigns the read-only capability to
  // another SharedMemory object and leaves this object's mapping in
  // place and writeable.  This can be used to broadcast data to
  // several untrusted readers without copying.
  //
  // The other constraints of Freeze() still apply: this object closes
  // its handle (as if by `Close(false)`), it cannot have been
  // previously shared, and the read-only handle cannot be used to
  // write the memory even by a malicious process.
  //
  // (The out parameter can currently be the same as `this` if and
  // only if the memory was unmapped, but this is an implementation
  // detail and shouldn't be relied on; for that use case Freeze()
  // should be used instead.)
  [[nodiscard]] bool ReadOnlyCopy(SharedMemory* ro_out);

  // Closes the open shared memory segment.
  // It is safe to call Close repeatedly.
  void Close(bool unmap_view = true);

  // Returns a page-aligned address at which the given number of bytes could
  // probably be mapped.  Returns NULL on error or if there is insufficient
  // contiguous address space to map the required number of pages.
  //
  // Note that there is no guarantee that the given address space will actually
  // be free by the time this function returns, since another thread might map
  // something there in the meantime.
  static void* FindFreeAddressSpace(size_t size);

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
  bool GiveToProcess(ProcessId target_pid, SharedMemoryHandle* new_handle) {
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
                            SharedMemoryHandle* new_handle, bool close_self);

  bool CreateInternal(size_t size, bool freezeable);

  // Unmapping shared memory requires the mapped size on Unix but not
  // Windows; this encapsulates that difference.
  struct MappingDeleter {
#ifdef OS_POSIX
    // A default-constructed deleter must be used only with nullptr
    // (to allow default-constructing UniqueMapping).  A deleter with
    // a size must be used at most once.
    size_t mapped_size_ = 0;
    explicit MappingDeleter(size_t size) : mapped_size_(size) {}
#endif
    MappingDeleter() = default;
    void operator()(void* ptr);
  };
  using UniqueMapping = mozilla::UniquePtr<void, MappingDeleter>;

  UniqueMapping memory_;
  size_t max_size_ = 0;
  mozilla::UniqueFileHandle mapped_file_;
#if defined(OS_WIN)
  // If true indicates this came from an external source so needs extra checks
  // before being mapped.
  bool external_section_ = false;
#elif defined(OS_POSIX)
  mozilla::UniqueFileHandle frozen_file_;
#endif
  bool read_only_ = false;
  bool freezeable_ = false;

  DISALLOW_EVIL_CONSTRUCTORS(SharedMemory);
};

}  // namespace base

#endif  // BASE_SHARED_MEMORY_H_
