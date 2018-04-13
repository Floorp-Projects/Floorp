/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <libgen.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/mman.h>
#define _DARWIN_USE_64_BIT_INODE // Use 64-bit inode data structures
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <fstream>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/eintr_wrapper.h"
#include "base/file_path.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "base/time.h"

namespace file_util {

#if defined(GOOGLE_CHROME_BUILD)
static const char* kTempFileName = "com.google.chrome.XXXXXX";
#else
static const char* kTempFileName = "org.chromium.XXXXXX";
#endif

bool AbsolutePath(FilePath* path) {
  char full_path[PATH_MAX];
  if (realpath(path->value().c_str(), full_path) == NULL)
    return false;
  *path = FilePath(full_path);
  return true;
}

// TODO(erikkay): The Windows version of this accepts paths like "foo/bar/*"
// which works both with and without the recursive flag.  I'm not sure we need
// that functionality. If not, remove from file_util_win.cc, otherwise add it
// here.
bool Delete(const FilePath& path) {
  const char* path_str = path.value().c_str();
  struct stat file_info;
  int test = stat(path_str, &file_info);
  if (test != 0) {
    // The Windows version defines this condition as success.
    bool ret = (errno == ENOENT || errno == ENOTDIR);
    return ret;
  }
  if (!S_ISDIR(file_info.st_mode))
    return (unlink(path_str) == 0);

  return (rmdir(path_str) == 0);
}

bool PathExists(const FilePath& path) {
  struct stat file_info;
  return (stat(path.value().c_str(), &file_info) == 0);
}

bool PathIsWritable(const FilePath& path) {
  FilePath test_path(path);
  struct stat file_info;
  if (stat(test_path.value().c_str(), &file_info) != 0) {
    // If the path doesn't exist, test the parent dir.
    test_path = test_path.DirName();
    // If the parent dir doesn't exist, then return false (the path is not
    // directly writable).
    if (stat(test_path.value().c_str(), &file_info) != 0)
      return false;
  }
  if (S_IWOTH & file_info.st_mode)
    return true;
  if (getegid() == file_info.st_gid && (S_IWGRP & file_info.st_mode))
    return true;
  if (geteuid() == file_info.st_uid && (S_IWUSR & file_info.st_mode))
    return true;
  return false;
}

bool DirectoryExists(const FilePath& path) {
  struct stat file_info;
  if (stat(path.value().c_str(), &file_info) == 0)
    return S_ISDIR(file_info.st_mode);
  return false;
}

bool ReadFromFD(int fd, char* buffer, size_t bytes) {
  size_t total_read = 0;
  while (total_read < bytes) {
    ssize_t bytes_read =
        HANDLE_EINTR(read(fd, buffer + total_read, bytes - total_read));
    if (bytes_read <= 0)
      break;
    total_read += bytes_read;
  }
  return total_read == bytes;
}

// Creates and opens a temporary file in |directory|, returning the
// file descriptor.  |path| is set to the temporary file path.
// Note TODO(erikkay) comment in header for BlahFileName() calls; the
// intent is to rename these files BlahFile() (since they create
// files, not filenames).  This function does NOT unlink() the file.
int CreateAndOpenFdForTemporaryFile(FilePath directory, FilePath* path) {
  *path = directory.Append(kTempFileName);
  const std::string& tmpdir_string = path->value();
  // this should be OK since mkstemp just replaces characters in place
  char* buffer = const_cast<char*>(tmpdir_string.c_str());

  return mkstemp(buffer);
}

bool CreateTemporaryFileName(FilePath* path) {
  FilePath directory;
  if (!GetTempDir(&directory))
    return false;
  int fd = CreateAndOpenFdForTemporaryFile(directory, path);
  if (fd < 0)
    return false;
  close(fd);
  return true;
}

FILE* CreateAndOpenTemporaryShmemFile(FilePath* path) {
  FilePath directory;
  if (!GetShmemTempDir(&directory))
    return NULL;

  return CreateAndOpenTemporaryFileInDir(directory, path);
}

FILE* CreateAndOpenTemporaryFileInDir(const FilePath& dir, FilePath* path) {
  int fd = CreateAndOpenFdForTemporaryFile(dir, path);
  if (fd < 0)
    return NULL;

  return fdopen(fd, "a+");
}

bool CreateTemporaryFileNameInDir(const std::wstring& dir,
                                  std::wstring* temp_file) {
  // Not implemented yet.
  NOTREACHED();
  return false;
}

bool CreateNewTempDirectory(const FilePath::StringType& prefix,
                            FilePath* new_temp_path) {
  FilePath tmpdir;
  if (!GetTempDir(&tmpdir))
    return false;
  tmpdir = tmpdir.Append(kTempFileName);
  std::string tmpdir_string = tmpdir.value();
#ifdef ANDROID
  char* dtemp = NULL;
#else
  // this should be OK since mkdtemp just replaces characters in place
  char* buffer = const_cast<char*>(tmpdir_string.c_str());
  char* dtemp = mkdtemp(buffer);
#endif
  if (!dtemp)
    return false;
  *new_temp_path = FilePath(dtemp);
  return true;
}

bool CreateDirectory(const FilePath& full_path) {
  std::vector<FilePath> subpaths;

  // Collect a list of all parent directories.
  FilePath last_path = full_path;
  subpaths.push_back(full_path);
  for (FilePath path = full_path.DirName();
       path.value() != last_path.value(); path = path.DirName()) {
    subpaths.push_back(path);
    last_path = path;
  }

  // Iterate through the parents and create the missing ones.
  for (std::vector<FilePath>::reverse_iterator i = subpaths.rbegin();
       i != subpaths.rend(); ++i) {
    if (!DirectoryExists(*i)) {
      if (mkdir(i->value().c_str(), 0777) != 0)
        return false;
    }
  }
  return true;
}

bool GetFileInfo(const FilePath& file_path, FileInfo* results) {
  struct stat file_info;
  if (stat(file_path.value().c_str(), &file_info) != 0)
    return false;
  results->is_directory = S_ISDIR(file_info.st_mode);
  results->size = file_info.st_size;
  return true;
}

FILE* OpenFile(const std::string& filename, const char* mode) {
  return OpenFile(FilePath(filename), mode);
}

FILE* OpenFile(const FilePath& filename, const char* mode) {
  return fopen(filename.value().c_str(), mode);
}

int ReadFile(const FilePath& filename, char* data, int size) {
  int fd = open(filename.value().c_str(), O_RDONLY);
  if (fd < 0)
    return -1;

  int ret_value = HANDLE_EINTR(read(fd, data, size));
  IGNORE_EINTR(close(fd));
  return ret_value;
}

int WriteFile(const FilePath& filename, const char* data, int size) {
  int fd = creat(filename.value().c_str(), 0666);
  if (fd < 0)
    return -1;

  // Allow for partial writes
  ssize_t bytes_written_total = 0;
  do {
    ssize_t bytes_written_partial =
      HANDLE_EINTR(write(fd, data + bytes_written_total,
                         size - bytes_written_total));
    if (bytes_written_partial < 0) {
      IGNORE_EINTR(close(fd));
      return -1;
    }
    bytes_written_total += bytes_written_partial;
  } while (bytes_written_total < size);

  IGNORE_EINTR(close(fd));
  return bytes_written_total;
}

// Gets the current working directory for the process.
bool GetCurrentDirectory(FilePath* dir) {
  char system_buffer[PATH_MAX] = "";
  if (!getcwd(system_buffer, sizeof(system_buffer))) {
    NOTREACHED();
    return false;
  }
  *dir = FilePath(system_buffer);
  return true;
}

// Sets the current working directory for the process.
bool SetCurrentDirectory(const FilePath& path) {
  int ret = chdir(path.value().c_str());
  return !ret;
}

#if !defined(OS_MACOSX)
bool GetTempDir(FilePath* path) {
  const char* tmp = getenv("TMPDIR");
  if (tmp)
    *path = FilePath(tmp);
  else
    *path = FilePath("/tmp");
  return true;
}

bool GetShmemTempDir(FilePath* path) {
#if defined(OS_LINUX) && !defined(ANDROID)
  *path = FilePath("/dev/shm");
  return true;
#else
  return GetTempDir(path);
#endif
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

  if (IGNORE_EINTR(close(infile)) < 0)
    result = false;
  if (IGNORE_EINTR(close(outfile)) < 0)
    result = false;

  return result;
}
#endif // !defined(OS_MACOSX)

} // namespace file_util
