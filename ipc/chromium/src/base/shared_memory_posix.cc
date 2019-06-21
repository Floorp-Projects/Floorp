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
#  include <linux/ashmem.h>
#endif

#include "base/eintr_wrapper.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "mozilla/Atomics.h"
#include "mozilla/UniquePtrExtensions.h"
#include "prenv.h"

namespace base {

SharedMemory::SharedMemory()
    : mapped_file_(-1),
      frozen_file_(-1),
      mapped_size_(0),
      memory_(nullptr),
      read_only_(false),
      freezeable_(false),
      max_size_(0) {}

SharedMemory::SharedMemory(SharedMemory&& other) {
  if (this == &other) {
    return;
  }

  mapped_file_ = other.mapped_file_;
  mapped_size_ = other.mapped_size_;
  frozen_file_ = other.frozen_file_;
  memory_ = other.memory_;
  read_only_ = other.read_only_;
  freezeable_ = other.freezeable_;
  max_size_ = other.max_size_;

  other.mapped_file_ = -1;
  other.mapped_size_ = 0;
  other.frozen_file_ = -1;
  other.memory_ = nullptr;
}

SharedMemory::~SharedMemory() { Close(); }

bool SharedMemory::SetHandle(SharedMemoryHandle handle, bool read_only) {
  DCHECK(mapped_file_ == -1);
  DCHECK(frozen_file_ == -1);

  freezeable_ = false;
  mapped_file_ = handle.fd;
  read_only_ = read_only;
  return true;
}

// static
bool SharedMemory::IsHandleValid(const SharedMemoryHandle& handle) {
  return handle.fd >= 0;
}

bool SharedMemory::IsValid() const { return mapped_file_ >= 0; }

// static
SharedMemoryHandle SharedMemory::NULLHandle() { return SharedMemoryHandle(); }

// Workaround for CVE-2018-4435 (https://crbug.com/project-zero/1671);
// can be removed when minimum OS version is at least 10.12.
#ifdef OS_MACOSX
static const char* GetTmpDir() {
  static const char* const kTmpDir = [] {
    const char* tmpdir = PR_GetEnv("TMPDIR");
    if (tmpdir) {
      return tmpdir;
    }
    return "/tmp";
  }();
  return kTmpDir;
}

static int FakeShmOpen(const char* name, int oflag, int mode) {
  CHECK(name[0] == '/');
  std::string path(GetTmpDir());
  path += name;
  return open(path.c_str(), oflag | O_CLOEXEC | O_NOCTTY, mode);
}

static int FakeShmUnlink(const char* name) {
  CHECK(name[0] == '/');
  std::string path(GetTmpDir());
  path += name;
  return unlink(path.c_str());
}

static bool IsShmOpenSecure() {
  static const bool kIsSecure = [] {
    mozilla::UniqueFileHandle rwfd, rofd;
    std::string name;
    CHECK(SharedMemory::AppendPosixShmPrefix(&name, getpid()));
    name += "sectest";
    // The prefix includes the pid and this will be called at most
    // once per process, so no need for a counter.
    rwfd.reset(
        HANDLE_EINTR(shm_open(name.c_str(), O_RDWR | O_CREAT | O_EXCL, 0600)));
    // An adversary could steal the name.  Handle this semi-gracefully.
    DCHECK(rwfd);
    if (!rwfd) {
      return false;
    }
    rofd.reset(shm_open(name.c_str(), O_RDONLY, 0400));
    CHECK(rofd);
    CHECK(shm_unlink(name.c_str()) == 0);
    CHECK(ftruncate(rwfd.get(), 1) == 0);
    rwfd = nullptr;
    void* map = mmap(nullptr, 1, PROT_READ, MAP_SHARED, rofd.get(), 0);
    CHECK(map != MAP_FAILED);
    bool ok = mprotect(map, 1, PROT_READ | PROT_WRITE) != 0;
    munmap(map, 1);
    return ok;
  }();
  return kIsSecure;
}

static int SafeShmOpen(bool freezeable, const char* name, int oflag, int mode) {
  if (!freezeable || IsShmOpenSecure()) {
    return shm_open(name, oflag, mode);
  } else {
    return FakeShmOpen(name, oflag, mode);
  }
}

static int SafeShmUnlink(bool freezeable, const char* name) {
  if (!freezeable || IsShmOpenSecure()) {
    return shm_unlink(name);
  } else {
    return FakeShmUnlink(name);
  }
}

#elif !defined(ANDROID)
static int SafeShmOpen(bool freezeable, const char* name, int oflag, int mode) {
  return shm_open(name, oflag, mode);
}

static int SafeShmUnlink(bool freezeable, const char* name) {
  return shm_unlink(name);
}
#endif

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
  DCHECK(mapped_file_ == -1);
  DCHECK(frozen_file_ == -1);

  mozilla::UniqueFileHandle fd;
  mozilla::UniqueFileHandle frozen_fd;
  bool needs_truncate = true;

#ifdef ANDROID
  // Android has its own shared memory facility:
  fd.reset(open("/" ASHMEM_NAME_DEF, O_RDWR, 0600));
  if (!fd) {
    CHROMIUM_LOG(WARNING) << "failed to open shm: " << strerror(errno);
    return false;
  }
  if (ioctl(fd.get(), ASHMEM_SET_SIZE, size) != 0) {
    CHROMIUM_LOG(WARNING) << "failed to set shm size: " << strerror(errno);
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
    fd.reset(HANDLE_EINTR(SafeShmOpen(freezeable, name.c_str(),
                                      O_RDWR | O_CREAT | O_EXCL, 0600)));
    if (fd) {
      if (freezeable) {
        frozen_fd.reset(HANDLE_EINTR(
            SafeShmOpen(freezeable, name.c_str(), O_RDONLY, 0400)));
        if (!frozen_fd) {
          int open_err = errno;
          SafeShmUnlink(freezeable, name.c_str());
          DLOG(FATAL) << "failed to re-open freezeable shm: "
                      << strerror(open_err);
          return false;
        }
      }
      if (SafeShmUnlink(freezeable, name.c_str()) != 0) {
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
    if (HANDLE_EINTR(ftruncate(fd.get(), static_cast<off_t>(size))) != 0) {
      CHROMIUM_LOG(WARNING) << "failed to set shm size: " << strerror(errno);
      return false;
    }
  }

  mapped_file_ = fd.release();
  frozen_file_ = frozen_fd.release();
  max_size_ = size;
  freezeable_ = freezeable;
  return true;
}

bool SharedMemory::Freeze() {
  DCHECK(!read_only_);
  CHECK(freezeable_);
  Unmap();

#ifdef ANDROID
  if (ioctl(mapped_file_, ASHMEM_SET_PROT_MASK, PROT_READ) != 0) {
    CHROMIUM_LOG(WARNING) << "failed to freeze shm: " << strerror(errno);
    return false;
  }
#else
  DCHECK(frozen_file_ >= 0);
  DCHECK(mapped_file_ >= 0);
  close(mapped_file_);
  mapped_file_ = frozen_file_;
  frozen_file_ = -1;
#endif

  read_only_ = true;
  freezeable_ = false;
  return true;
}

bool SharedMemory::Map(size_t bytes, void* fixed_address) {
  if (mapped_file_ == -1) return false;
  DCHECK(!memory_);

  // Don't use MAP_FIXED when a fixed_address was specified, since that can
  // replace pages that are alread mapped at that address.
  void* mem =
      mmap(fixed_address, bytes, PROT_READ | (read_only_ ? 0 : PROT_WRITE),
           MAP_SHARED, mapped_file_, 0);

  if (mem == MAP_FAILED) {
    CHROMIUM_LOG(WARNING) << "Call to mmap failed: " << strerror(errno);
    return false;
  }

  if (fixed_address && mem != fixed_address) {
    bool munmap_succeeded = munmap(mem, bytes) == 0;
    DCHECK(munmap_succeeded) << "Call to munmap failed, errno=" << errno;
    return false;
  }

  memory_ = mem;
  mapped_size_ = bytes;
  return true;
}

bool SharedMemory::Unmap() {
  if (memory_ == NULL) return false;

  munmap(memory_, mapped_size_);
  memory_ = NULL;
  mapped_size_ = 0;
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
  const int new_fd = dup(mapped_file_);
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

  if (mapped_file_ >= 0) {
    close(mapped_file_);
    mapped_file_ = -1;
  }
  if (frozen_file_ >= 0) {
    CHROMIUM_LOG(WARNING) << "freezeable shared memory was never frozen";
    close(frozen_file_);
    frozen_file_ = -1;
  }
}

mozilla::UniqueFileHandle SharedMemory::TakeHandle() {
  mozilla::UniqueFileHandle fh(mapped_file_);
  mapped_file_ = -1;
  // Now that the main fd is removed, reset everything else: close the
  // frozen fd if present and unmap the memory if mapped.
  Close();
  return fh;
}

}  // namespace base
