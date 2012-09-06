// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// derived from dir_reader_linux.h

#ifndef BASE_DIR_READER_BSD_H_
#define BASE_DIR_READER_BSD_H_
#pragma once

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <unistd.h>

#include "base/logging.h"
#include "base/eintr_wrapper.h"

// See the comments in dir_reader_posix.h about this.

namespace base {

class DirReaderBSD {
 public:
  explicit DirReaderBSD(const char* directory_path)
#ifdef O_DIRECTORY
      : fd_(open(directory_path, O_RDONLY | O_DIRECTORY)),
#else
      : fd_(open(directory_path, O_RDONLY)),
#endif
        offset_(0),
        size_(0) {
    memset(buf_, 0, sizeof(buf_));
  }

  ~DirReaderBSD() {
    if (fd_ >= 0) {
      if (HANDLE_EINTR(close(fd_)))
        DLOG(ERROR) << "Failed to close directory handle";
    }
  }

  bool IsValid() const {
    return fd_ >= 0;
  }

  // Move to the next entry returning false if the iteration is complete.
  bool Next() {
    if (size_) {
      struct dirent* dirent = reinterpret_cast<struct dirent*>(&buf_[offset_]);
#ifdef OS_DRAGONFLY
      offset_ += _DIRENT_DIRSIZ(dirent);
#else
      offset_ += dirent->d_reclen;
#endif
    }

    if (offset_ != size_)
      return true;

#ifdef OS_OPENBSD
    const int r = getdirentries(fd_, buf_, sizeof(buf_), basep_);
#else
    const int r = getdents(fd_, buf_, sizeof(buf_));
#endif
    if (r == 0)
      return false;
    if (r == -1) {
#ifdef OS_OPENBSD
      DLOG(ERROR) << "getdirentries returned an error: " << errno;
#else
      DLOG(ERROR) << "getdents returned an error: " << errno;
#endif
      return false;
    }
    size_ = r;
    offset_ = 0;
    return true;
  }

  const char* name() const {
    if (!size_)
      return NULL;

    const struct dirent* dirent =
        reinterpret_cast<const struct dirent*>(&buf_[offset_]);
    return dirent->d_name;
  }

  int fd() const {
    return fd_;
  }

  static bool IsFallback() {
    return false;
  }

 private:
  const int fd_;
  char buf_[512];
#ifdef OS_OPENBSD
  off_t *basep_;
#endif
  size_t offset_, size_;

  DISALLOW_COPY_AND_ASSIGN(DirReaderBSD);
};

}  // namespace base

#endif // BASE_DIR_READER_BSD_H_
