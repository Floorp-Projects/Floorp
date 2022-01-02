/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_DIR_READER_LINUX_H_
#define BASE_DIR_READER_LINUX_H_
#pragma once

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <sys/syscall.h>
#include <unistd.h>

#include "base/logging.h"
#include "base/eintr_wrapper.h"

// See the comments in dir_reader_posix.h about this.

namespace base {

struct linux_dirent {
  uint64_t d_ino;
  int64_t d_off;
  unsigned short d_reclen;
  unsigned char d_type;
  char d_name[0];
};

class DirReaderLinux {
 public:
  explicit DirReaderLinux(const char* directory_path)
      : fd_(open(directory_path, O_RDONLY | O_DIRECTORY)),
        offset_(0),
        size_(0) {
    memset(buf_, 0, sizeof(buf_));
  }

  ~DirReaderLinux() {
    if (fd_ >= 0) {
      if (IGNORE_EINTR(close(fd_)))
        DLOG(ERROR) << "Failed to close directory handle";
    }
  }

  bool IsValid() const { return fd_ >= 0; }

  // Move to the next entry returning false if the iteration is complete.
  bool Next() {
    if (size_) {
      linux_dirent* dirent = reinterpret_cast<linux_dirent*>(&buf_[offset_]);
      offset_ += dirent->d_reclen;
    }

    if (offset_ != size_) return true;

    const int r = syscall(__NR_getdents64, fd_, buf_, sizeof(buf_));
    if (r == 0) return false;
    if (r == -1) {
      DLOG(ERROR) << "getdents64 returned an error: " << errno;
      return false;
    }
    size_ = r;
    offset_ = 0;
    return true;
  }

  const char* name() const {
    if (!size_) return NULL;

    const linux_dirent* dirent =
        reinterpret_cast<const linux_dirent*>(&buf_[offset_]);
    return dirent->d_name;
  }

  int fd() const { return fd_; }

  static bool IsFallback() { return false; }

 private:
  const int fd_;
  union {
    linux_dirent dirent_;
    unsigned char buf_[512];
  };
  size_t offset_, size_;

  DISALLOW_COPY_AND_ASSIGN(DirReaderLinux);
};

}  // namespace base

#endif  // BASE_DIR_READER_LINUX_H_
