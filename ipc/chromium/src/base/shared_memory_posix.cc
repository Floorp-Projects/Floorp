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

#ifdef ANDROID
#  include "mozilla/Ashmem.h"
#endif

#include "base/eintr_wrapper.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "mozilla/Atomics.h"
#include "mozilla/UniquePtrExtensions.h"
#include "prenv.h"
#include "GeckoProfiler.h"

namespace base {

void SharedMemory::MappingDeleter::operator()(void* ptr) {
  // Check that this isn't a default-constructed deleter.  (`munmap`
  // is specified to fail with `EINVAL` if the length is 0, so this
  // assertion isn't load-bearing.)
  DCHECK(mapped_size_ != 0);
  munmap(ptr, mapped_size_);
  // Guard against multiple calls of the same deleter, which shouldn't
  // happen (but could, if `UniquePtr::reset` were used).  Calling
  // `munmap` with an incorrect non-zero length would be bad.
  mapped_size_ = 0;
}

SharedMemory::~SharedMemory() {
  // This is almost equal to the default destructor, except for the
  // warning message about unfrozen freezable memory.
  Close();
}

bool SharedMemory::SetHandle(SharedMemoryHandle handle, bool read_only) {
  DCHECK(!mapped_file_);
  DCHECK(!frozen_file_);

  freezeable_ = false;
  mapped_file_.reset(handle.fd);
  read_only_ = read_only;
  return true;
}

// static
bool SharedMemory::IsHandleValid(const SharedMemoryHandle& handle) {
  return handle.fd >= 0;
}

// static
SharedMemoryHandle SharedMemory::NULLHandle() { return SharedMemoryHandle(); }

// static
bool SharedMemory::AppendPosixShmPrefix(std::string* str, pid_t pid) {
#if defined(ANDROID)
  return false;
#else
  *str += '/';
#  ifdef OS_LINUX
  // The Snap package environment doesn't provide a private /dev/shm
  // (it's used for communication with services like PulseAudio);
  // instead AppArmor is used to restrict access to it.  Anything with
  // this prefix is allowed:
  static const char* const kSnap = [] {
    auto instanceName = PR_GetEnv("SNAP_INSTANCE_NAME");
    if (instanceName != nullptr) {
      return instanceName;
    }
    // Compatibility for snapd <= 2.35:
    return PR_GetEnv("SNAP_NAME");
  }();

  if (kSnap) {
    StringAppendF(str, "snap.%s.", kSnap);
  }
#  endif  // OS_LINUX
  // Hopefully the "implementation defined" name length limit is long
  // enough for this.
  StringAppendF(str, "org.mozilla.ipc.%d.", static_cast<int>(pid));
  return true;
#endif    // !ANDROID
}

bool SharedMemory::CreateInternal(size_t size, bool freezeable) {
  read_only_ = false;

  DCHECK(size > 0);
  DCHECK(!mapped_file_);
  DCHECK(!frozen_file_);

  mozilla::UniqueFileHandle fd;
  mozilla::UniqueFileHandle frozen_fd;
  bool needs_truncate = true;

#ifdef ANDROID
  // Android has its own shared memory facility:
  fd.reset(mozilla::android::ashmem_create(nullptr, size));
  if (!fd) {
    CHROMIUM_LOG(WARNING) << "failed to open shm: " << strerror(errno);
    return false;
  }
  needs_truncate = false;
#else
  // Generic Unix: shm_open + shm_unlink
  do {
    // The names don't need to be unique, but it saves time if they
    // usually are.
    static mozilla::Atomic<size_t> sNameCounter;
    std::string name;
    CHECK(AppendPosixShmPrefix(&name, getpid()));
    StringAppendF(&name, "%zu", sNameCounter++);
    // O_EXCL means the names being predictable shouldn't be a problem.
    fd.reset(
        HANDLE_EINTR(shm_open(name.c_str(), O_RDWR | O_CREAT | O_EXCL, 0600)));
    if (fd) {
      if (freezeable) {
        frozen_fd.reset(HANDLE_EINTR(shm_open(name.c_str(), O_RDONLY, 0400)));
        if (!frozen_fd) {
          int open_err = errno;
          shm_unlink(name.c_str());
          DLOG(FATAL) << "failed to re-open freezeable shm: "
                      << strerror(open_err);
          return false;
        }
      }
      if (shm_unlink(name.c_str()) != 0) {
        // This shouldn't happen, but if it does: assume the file is
        // in fact leaked, and bail out now while it's still 0-length.
        DLOG(FATAL) << "failed to unlink shm: " << strerror(errno);
        return false;
      }
    }
  } while (!fd && errno == EEXIST);
#endif

  if (!fd) {
    CHROMIUM_LOG(WARNING) << "failed to open shm: " << strerror(errno);
    return false;
  }

  if (needs_truncate) {
#if defined(HAVE_POSIX_FALLOCATE)
    // Using posix_fallocate will ensure that there's actually space for this
    // file. Otherwise we end up with a sparse file that can give SIGBUS if we
    // run out of space while writing to it.
    int rv;
    {
      // Avoid repeated interruptions of posix_fallocate by the profiler's
      // SIGPROF sampling signal. Indicating "thread sleep" here means we'll
      // get up to one interruption but not more. See bug 1658847 for more.
      // This has to be scoped outside the HANDLE_RV_EINTR retry loop.
      AUTO_PROFILER_THREAD_SLEEP;
      rv = HANDLE_RV_EINTR(
          posix_fallocate(fd.get(), 0, static_cast<off_t>(size)));
    }
    if (rv != 0) {
      if (rv == EOPNOTSUPP || rv == EINVAL || rv == ENODEV) {
        // Some filesystems have trouble with posix_fallocate. For now, we must
        // fallback ftruncate and accept the allocation failures like we do
        // without posix_fallocate.
        // See https://bugzilla.mozilla.org/show_bug.cgi?id=1618914
        int fallocate_errno = rv;
        rv = HANDLE_EINTR(ftruncate(fd.get(), static_cast<off_t>(size)));
        if (rv != 0) {
          CHROMIUM_LOG(WARNING) << "fallocate failed to set shm size: "
                                << strerror(fallocate_errno);
          CHROMIUM_LOG(WARNING)
              << "ftruncate failed to set shm size: " << strerror(errno);
          return false;
        }
      } else {
        CHROMIUM_LOG(WARNING)
            << "fallocate failed to set shm size: " << strerror(rv);
        return false;
      }
    }
#else
    int rv = HANDLE_EINTR(ftruncate(fd.get(), static_cast<off_t>(size)));
    if (rv != 0) {
      CHROMIUM_LOG(WARNING)
          << "ftruncate failed to set shm size: " << strerror(errno);
      return false;
    }
#endif
  }

  mapped_file_ = std::move(fd);
  frozen_file_ = std::move(frozen_fd);
  max_size_ = size;
  freezeable_ = freezeable;
  return true;
}

bool SharedMemory::ReadOnlyCopy(SharedMemory* ro_out) {
  DCHECK(mapped_file_);
  DCHECK(!read_only_);
  CHECK(freezeable_);

  if (ro_out == this) {
    DCHECK(!memory_);
  }

  mozilla::UniqueFileHandle ro_file;
#ifdef ANDROID
  ro_file = std::move(mapped_file_);
  if (mozilla::android::ashmem_setProt(ro_file.get(), PROT_READ) != 0) {
    CHROMIUM_LOG(WARNING) << "failed to set ashmem read-only: "
                          << strerror(errno);
    return false;
  }
#else
  DCHECK(frozen_file_);
  mapped_file_ = nullptr;
  ro_file = std::move(frozen_file_);
#endif

  DCHECK(ro_file);
  freezeable_ = false;

  ro_out->Close();
  ro_out->mapped_file_ = std::move(ro_file);
  ro_out->max_size_ = max_size_;
  ro_out->read_only_ = true;
  ro_out->freezeable_ = false;

  return true;
}

bool SharedMemory::Map(size_t bytes, void* fixed_address) {
  if (!mapped_file_) {
    return false;
  }
  DCHECK(!memory_);

  // Don't use MAP_FIXED when a fixed_address was specified, since that can
  // replace pages that are alread mapped at that address.
  void* mem =
      mmap(fixed_address, bytes, PROT_READ | (read_only_ ? 0 : PROT_WRITE),
           MAP_SHARED, mapped_file_.get(), 0);

  if (mem == MAP_FAILED) {
    CHROMIUM_LOG(WARNING) << "Call to mmap failed: " << strerror(errno);
    return false;
  }

  if (fixed_address && mem != fixed_address) {
    bool munmap_succeeded = munmap(mem, bytes) == 0;
    DCHECK(munmap_succeeded) << "Call to munmap failed, errno=" << errno;
    return false;
  }

  memory_ = UniqueMapping(mem, MappingDeleter(bytes));
  return true;
}

void* SharedMemory::FindFreeAddressSpace(size_t size) {
  void* memory =
      mmap(NULL, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  munmap(memory, size);
  return memory != MAP_FAILED ? memory : NULL;
}

bool SharedMemory::ShareToProcessCommon(ProcessId processId,
                                        SharedMemoryHandle* new_handle,
                                        bool close_self) {
  freezeable_ = false;
  const int new_fd = dup(mapped_file_.get());
  DCHECK(new_fd >= -1);
  new_handle->fd = new_fd;
  new_handle->auto_close = true;

  if (close_self) Close();

  return true;
}

void SharedMemory::Close(bool unmap_view) {
  if (unmap_view) {
    Unmap();
  }

  mapped_file_ = nullptr;
  if (frozen_file_) {
    CHROMIUM_LOG(WARNING) << "freezeable shared memory was never frozen";
    frozen_file_ = nullptr;
  }
}

}  // namespace base
