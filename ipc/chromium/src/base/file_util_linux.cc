// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"

#include <fcntl.h>

#include <string>
#include <vector>

#include "base/eintr_wrapper.h"
#include "base/file_path.h"
#include "base/string_util.h"

namespace file_util {

bool GetTempDir(FilePath* path) {
  const char* tmp = getenv("TMPDIR");
  if (tmp)
    *path = FilePath(tmp);
  else
    *path = FilePath("/tmp");
  return true;
}

bool GetShmemTempDir(FilePath* path) {
  *path = FilePath("/dev/shm");
  return true;
}

bool CopyFile(const FilePath& from_path, const FilePath& to_path) {
  int infile = open(from_path.value().c_str(), O_RDONLY);
  if (infile < 0)
    return false;

  int outfile = creat(to_path.value().c_str(), 0666);
  if (outfile < 0) {
    close(infile);
    return false;
  }

  const size_t kBufferSize = 32768;
  std::vector<char> buffer(kBufferSize);
  bool result = true;

  while (result) {
    ssize_t bytes_read = HANDLE_EINTR(read(infile, &buffer[0], buffer.size()));
    if (bytes_read < 0) {
      result = false;
      break;
    }
    if (bytes_read == 0)
      break;
    // Allow for partial writes
    ssize_t bytes_written_per_read = 0;
    do {
      ssize_t bytes_written_partial = HANDLE_EINTR(write(
          outfile,
          &buffer[bytes_written_per_read],
          bytes_read - bytes_written_per_read));
      if (bytes_written_partial < 0) {
        result = false;
        break;
      }
      bytes_written_per_read += bytes_written_partial;
    } while (bytes_written_per_read < bytes_read);
  }

  if (HANDLE_EINTR(close(infile)) < 0)
    result = false;
  if (HANDLE_EINTR(close(outfile)) < 0)
    result = false;

  return result;
}

}  // namespace file_util
