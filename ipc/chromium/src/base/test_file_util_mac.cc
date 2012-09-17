// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test_file_util.h"

#include <sys/mman.h>
#include <errno.h>
#include "base/logging.h"
#include "base/file_util.h"

namespace file_util {

bool EvictFileFromSystemCache(const FilePath& file) {
  // There aren't any really direct ways to purge a file from the UBC.  From
  // talking with Amit Singh, the safest is to mmap the file with MAP_FILE (the
  // default) + MAP_SHARED, then do an msync to invalidate the memory.  The next
  // open should then have to load the file from disk.

  file_util::MemoryMappedFile mapped_file;
  if (!mapped_file.Initialize(file)) {
    DLOG(WARNING) << "failed to memory map " << file.value();
    return false;
  }
  
  if (msync(const_cast<uint8_t*>(mapped_file.data()), mapped_file.length(),
            MS_INVALIDATE) != 0) {
    DLOG(WARNING) << "failed to invalidate memory map of " << file.value() 
        << ", errno: " << errno;
    return false;
  }
  
  return true;
}

}  // namespace file_util
