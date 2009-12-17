// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test_file_util.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "base/file_path.h"

namespace file_util {

bool EvictFileFromSystemCache(const FilePath& file) {
  int fd = open(file.value().c_str(), O_RDONLY);
  if (fd < 0)
    return false;
  if (fdatasync(fd) != 0)
    return false;
  if (posix_fadvise(fd, 0, 0, POSIX_FADV_DONTNEED) != 0)
    return false;
  close(fd);
  return true;
}

}  // namespace file_util
