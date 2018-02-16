/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef FileOperations_h
#define FileOperations_h

#include <stdio.h>
#include <string>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#define PATHSEP_CHAR '\\'
#define PATHSEP_STRING "\\"
#else
#define PATHSEP_CHAR '/'
#define PATHSEP_STRING "/"
#endif

// Make sure that all directories on path exist, excluding the final element of
// the path.
void ensurePath(std::string Path);

std::string getAbsolutePath(const std::string &Filename);

// Lock the given filename so that it cannot be opened by anyone else until this
// object goes out of scope. On Windows, we use a named mutex. On POSIX
// platforms, we use flock.
struct AutoLockFile {
  int FileDescriptor = -1;

#if defined(_WIN32) || defined(_WIN64)
  HANDLE Handle = NULL;
#endif

  AutoLockFile(const std::string &Filename);
  ~AutoLockFile();

  bool success();

  FILE *openFile(const char *Mode);
  bool truncateFile(size_t Length);
};

#endif
