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

namespace base {

namespace {
// Paranoia. Semaphores and shared memory segments should live in different
// namespaces, but who knows what's out there.
const char kSemaphoreSuffix[] = "-sem";
}

SharedMemory::SharedMemory()
    : mapped_file_(-1),
      inode_(0),
      memory_(NULL),
      read_only_(false),
      max_size_(0) {
}

SharedMemory::SharedMemory(SharedMemoryHandle handle, bool read_only)
    : mapped_file_(handle.fd),
      inode_(0),
      memory_(NULL),
      read_only_(read_only),
      max_size_(0) {
  struct stat st;
  if (fstat(handle.fd, &st) == 0) {
    // If fstat fails, then the file descriptor is invalid and we'll learn this
    // fact when Map() fails.
    inode_ = st.st_ino;
  }
}

SharedMemory::SharedMemory(SharedMemoryHandle handle, bool read_only,
                           ProcessHandle process)
    : mapped_file_(handle.fd),
      memory_(NULL),
      read_only_(read_only),
      max_size_(0) {
  // We don't handle this case yet (note the ignored parameter); let's die if
  // someone comes calling.
  NOTREACHED();
}

SharedMemory::~SharedMemory() {
  Close();
}

// static
bool SharedMemory::IsHandleValid(const SharedMemoryHandle& handle) {
  return handle.fd >= 0;
}

// static
SharedMemoryHandle SharedMemory::NULLHandle() {
  return SharedMemoryHandle();
}

bool SharedMemory::Create(const std::string &cname, bool read_only,
                          bool open_existing, size_t size) {
  read_only_ = read_only;

  std::wstring name = UTF8ToWide(cname);

  int posix_flags = 0;
  posix_flags |= read_only ? O_RDONLY : O_RDWR;
  if (!open_existing || mapped_file_ <= 0)
    posix_flags |= O_CREAT;

  if (!CreateOrOpen(name, posix_flags, size))
    return false;

  max_size_ = size;
  return true;
}

// Our current implementation of shmem is with mmap()ing of files.
// These files need to be deleted explicitly.
// In practice this call is only needed for unit tests.
bool SharedMemory::Delete(const std::wstring& name) {
  std::wstring mem_filename;
  if (FilenameForMemoryName(name, &mem_filename) == false)
    return false;

  FilePath path(WideToUTF8(mem_filename));
  if (file_util::PathExists(path)) {
    return file_util::Delete(path, false);
  }

  // Doesn't exist, so success.
  return true;
}

bool SharedMemory::Open(const std::wstring &name, bool read_only) {
  read_only_ = read_only;

  int posix_flags = 0;
  posix_flags |= read_only ? O_RDONLY : O_RDWR;

  return CreateOrOpen(name, posix_flags, 0);
}

// For the given shmem named |memname|, return a filename to mmap()
// (and possibly create).  Modifies |filename|.  Return false on
// error, or true of we are happy.
bool SharedMemory::FilenameForMemoryName(const std::wstring &memname,
                                         std::wstring *filename) {
  std::wstring mem_filename;

  // mem_name will be used for a filename; make sure it doesn't
  // contain anything which will confuse us.
  DCHECK(memname.find_first_of(L"/") == std::string::npos);
  DCHECK(memname.find_first_of(L"\0") == std::string::npos);

  FilePath temp_dir;
  if (file_util::GetShmemTempDir(&temp_dir) == false)
    return false;

  mem_filename = UTF8ToWide(temp_dir.value());
  file_util::AppendToPath(&mem_filename, L"com.google.chrome.shmem." + memname);
  *filename = mem_filename;
  return true;
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

typedef scoped_ptr_malloc<FILE, ScopedFILEClose> ScopedFILE;

}

// Chromium mostly only use the unique/private shmem as specified by
// "name == L"". The exception is in the StatsTable.
// TODO(jrg): there is no way to "clean up" all unused named shmem if
// we restart from a crash.  (That isn't a new problem, but it is a problem.)
// In case we want to delete it later, it may be useful to save the value
// of mem_filename after FilenameForMemoryName().
bool SharedMemory::CreateOrOpen(const std::wstring &name,
                                int posix_flags, size_t size) {
  DCHECK(mapped_file_ == -1);

  ScopedFILE file_closer;
  FILE *fp;

  if (name == L"") {
    // It doesn't make sense to have a read-only private piece of shmem
    DCHECK(posix_flags & (O_RDWR | O_WRONLY));

    FilePath path;
    fp = file_util::CreateAndOpenTemporaryShmemFile(&path);

    // Deleting the file prevents anyone else from mapping it in
    // (making it private), and prevents the need for cleanup (once
    // the last fd is closed, it is truly freed).
    file_util::Delete(path, false);
  } else {
    std::wstring mem_filename;
    if (FilenameForMemoryName(name, &mem_filename) == false)
      return false;

    std::string mode;
    switch (posix_flags) {
      case (O_RDWR | O_CREAT):
        // Careful: "w+" will truncate if it already exists.
        mode = "a+";
        break;
      case O_RDWR:
        mode = "r+";
        break;
      case O_RDONLY:
        mode = "r";
        break;
      default:
        NOTIMPLEMENTED();
        break;
    }

    fp = file_util::OpenFile(mem_filename, mode.c_str());
  }

  if (fp == NULL)
    return false;
  file_closer.reset(fp);  // close when we go out of scope

  // Make sure the (new) file is the right size.
  // According to the man page, "Use of truncate() to extend a file is
  // not portable."
  if (size && (posix_flags & (O_RDWR | O_CREAT))) {
    // Get current size.
    struct stat stat;
    if (fstat(fileno(fp), &stat) != 0)
      return false;
    size_t current_size = stat.st_size;
    if (current_size != size) {
      if (ftruncate(fileno(fp), size) != 0)
        return false;
      if (fseeko(fp, size, SEEK_SET) != 0)
        return false;
    }
  }

  mapped_file_ = dup(fileno(fp));
  DCHECK(mapped_file_ >= 0);

  struct stat st;
  if (fstat(mapped_file_, &st))
    NOTREACHED();
  inode_ = st.st_ino;

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

bool SharedMemory::ShareToProcessCommon(ProcessHandle process,
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


void SharedMemory::Close() {
  Unmap();

  if (mapped_file_ > 0) {
    close(mapped_file_);
    mapped_file_ = -1;
  }
}

#ifdef ANDROID
void SharedMemory::LockOrUnlockCommon(int function) {
  DCHECK(mapped_file_ >= 0);
  struct flock lockreq;
  lockreq.l_type = function;
  lockreq.l_whence = SEEK_SET;
  lockreq.l_start = 0;
  lockreq.l_len = 0;
  while (fcntl(mapped_file_, F_SETLKW, &lockreq) < 0) {
    if (errno == EINTR) {
      continue;
    } else if (errno == ENOLCK) {
      // temporary kernel resource exaustion
      PlatformThread::Sleep(500);
      continue;
    } else {
      NOTREACHED() << "lockf() failed."
                   << " function:" << function
                   << " fd:" << mapped_file_
                   << " errno:" << errno
                   << " msg:" << strerror(errno);
    }
  }
}

void SharedMemory::Lock() {
  LockOrUnlockCommon(F_WRLCK);
}

void SharedMemory::Unlock() {
  LockOrUnlockCommon(F_UNLCK);
}
#else
void SharedMemory::LockOrUnlockCommon(int function) {
  DCHECK(mapped_file_ >= 0);
  while (lockf(mapped_file_, function, 0) < 0) {
    if (errno == EINTR) {
      continue;
    } else if (errno == ENOLCK) {
      // temporary kernel resource exaustion
      PlatformThread::Sleep(500);
      continue;
    } else {
      NOTREACHED() << "lockf() failed."
                   << " function:" << function
                   << " fd:" << mapped_file_
                   << " errno:" << errno
                   << " msg:" << strerror(errno);
    }
  }
}

void SharedMemory::Lock() {
  LockOrUnlockCommon(F_LOCK);
}

void SharedMemory::Unlock() {
  LockOrUnlockCommon(F_ULOCK);
}
#endif

SharedMemoryHandle SharedMemory::handle() const {
  return FileDescriptor(mapped_file_, false);
}

}  // namespace base
