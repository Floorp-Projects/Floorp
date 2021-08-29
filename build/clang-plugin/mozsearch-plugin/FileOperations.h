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

// Used to synchronize access when writing to an analysis file, so that
// concurrently running clang instances don't clobber each other's data.
// On Windows, we use a named mutex. On POSIX platforms, we use flock on the
// source files. flock is advisory locking, and doesn't interfere with clang's
// own opening of the source files (i.e. to interfere, clang would have to be
// using flock itself, which it does not).
struct AutoLockFile {
  // Absolute path to the analysis file
  std::string Filename;

#if defined(_WIN32) || defined(_WIN64)
  // Handle for the named Mutex
  HANDLE Handle = NULL;
#else
  // fd for the *source* file that corresponds to the analysis file. We use
  // the source file because it doesn't change while the analysis file gets
  // repeatedly replaced by a new version written to a separate tmp file.
  // This fd is used when using flock to synchronize access.
  int FileDescriptor = -1;
#endif

  // SrcFile should be the absolute path to the source code file, and DstFile
  // the absolute path to the corresponding analysis file. This constructor
  // will block until exclusive access has been obtained.
  AutoLockFile(const std::string &SrcFile, const std::string &DstFile);
  ~AutoLockFile();

  // Check after constructing to ensure the mutex was properly set up.
  bool success();

  // There used to be an `openFile` method here but we switched to directly
  // using a std::ifstream for the input file in able to take advantage of its
  // support for variable length lines (as opposed to fgets which takes a fixed
  // size buffer).

  // Open a new tmp file for writing the new analysis data to. Caller is
  // responsible for fclose'ing it.
  FILE *openTmp();
  // Replace the existing analysis file with the new "tmp" one that has the new
  // data. Returns false on error.
  bool moveTmp();
};

#endif
