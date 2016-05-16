/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains utility functions for dealing with the local
// filesystem.

#ifndef BASE_FILE_UTIL_H_
#define BASE_FILE_UTIL_H_

#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>
#elif defined(ANDROID)
#include <sys/stat.h>
#elif defined(OS_POSIX) 
#include <sys/types.h>
#include <sys/stat.h>
#endif

#include <stdio.h>

#include <stack>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/file_path.h"

namespace file_util {

//-----------------------------------------------------------------------------
// Functions that operate purely on a path string w/o touching the filesystem:

// Returns true if the given path ends with a path separator character.
bool EndsWithSeparator(const FilePath& path);
// These two versions are both deprecated. TODO(estade): remove them.
bool EndsWithSeparator(std::wstring* path);
bool EndsWithSeparator(const std::wstring& path);

// Modifies a string by trimming all trailing separators from the end.
// Deprecated. FilePath does this automatically, and if it's constructed from a
// path with a trailing separator, StripTrailingSeparators() may be used.
void TrimTrailingSeparator(std::wstring* dir);

// Strips the topmost directory from the end of 'dir'.  Assumes 'dir' does not
// refer to a file.
// If 'dir' is a root directory, return without change.
// Deprecated. Use FilePath::DirName instead.
void UpOneDirectory(std::wstring* dir);

// Returns the filename portion of 'path', without any leading \'s or /'s.
// Deprecated. Use FilePath::BaseName instead.
std::wstring GetFilenameFromPath(const std::wstring& path);

// Deprecated compatibility function.  Use FilePath::Extension.
FilePath::StringType GetFileExtensionFromPath(const FilePath& path);
// Deprecated temporary compatibility function.
std::wstring GetFileExtensionFromPath(const std::wstring& path);

// Appends new_ending to path, adding a separator between the two if necessary.
void AppendToPath(std::wstring* path, const std::wstring& new_ending);

// Convert provided relative path into an absolute path.  Returns false on
// error. On POSIX, this function fails if the path does not exist.
bool AbsolutePath(FilePath* path);
// Deprecated temporary compatibility function.
bool AbsolutePath(std::wstring* path);

// Deprecated compatibility function.  Use FilePath::InsertBeforeExtension.
void InsertBeforeExtension(FilePath* path, const FilePath::StringType& suffix);

// Deprecated compatibility function.  Use FilePath::ReplaceExtension.
void ReplaceExtension(FilePath* file_name,
                      const FilePath::StringType& extension);

#if defined(OS_WIN)
// Deprecated temporary compatibility functions.
void InsertBeforeExtension(std::wstring* path, const std::wstring& suffix);
void ReplaceExtension(std::wstring* file_name, const std::wstring& extension);
#endif

//-----------------------------------------------------------------------------
// Functions that involve filesystem access or modification:

// Deletes the given path, whether it's a file or a directory.
// If it's a directory, it's perfectly happy to delete all of the
// directory's contents.
// Returns true if successful, false otherwise.
bool Delete(const FilePath& path);
// Deprecated temporary compatibility function.
bool Delete(const std::wstring& path);

// Copies a single file. Use CopyDirectory to copy directories.
bool CopyFile(const FilePath& from_path, const FilePath& to_path);
// Deprecated temporary compatibility function.
bool CopyFile(const std::wstring& from_path, const std::wstring& to_path);

// Returns true if the given path exists on the local filesystem,
// false otherwise.
bool PathExists(const FilePath& path);
// Deprecated temporary compatibility function.
bool PathExists(const std::wstring& path);

// Returns true if the given path is writable by the user, false otherwise.
bool PathIsWritable(const FilePath& path);
// Deprecated temporary compatibility function.
bool PathIsWritable(const std::wstring& path);

// Returns true if the given path exists and is a directory, false otherwise.
bool DirectoryExists(const FilePath& path);
// Deprecated temporary compatibility function.
bool DirectoryExists(const std::wstring& path);

#if defined(OS_POSIX)
// Read exactly |bytes| bytes from file descriptor |fd|, storing the result
// in |buffer|. This function is protected against EINTR and partial reads.
// Returns true iff |bytes| bytes have been successfuly read from |fd|.
bool ReadFromFD(int fd, char* buffer, size_t bytes);
#endif  // defined(OS_POSIX)

// Get the temporary directory provided by the system.
bool GetTempDir(FilePath* path);
// Deprecated temporary compatibility function.
bool GetTempDir(std::wstring* path);
// Get a temporary directory for shared memory files.
// Only useful on POSIX; redirects to GetTempDir() on Windows.
bool GetShmemTempDir(FilePath* path);

// Creates a temporary file. The full path is placed in |path|, and the
// function returns true if was successful in creating the file. The file will
// be empty and all handles closed after this function returns.
// TODO(erikkay): rename this function and track down all of the callers.
// (Clarification of erik's comment: the intent is to rename the BlahFileName()
//  calls into BlahFile(), since they create temp files (not temp filenames).)
bool CreateTemporaryFileName(FilePath* path);
// Deprecated temporary compatibility function.
bool CreateTemporaryFileName(std::wstring* temp_file);

// Create and open a temporary file.  File is opened for read/write.
// The full path is placed in |path|, and the function returns true if
// was successful in creating and opening the file.
FILE* CreateAndOpenTemporaryFile(FilePath* path);
// Like above but for shmem files.  Only useful for POSIX.
FILE* CreateAndOpenTemporaryShmemFile(FilePath* path);

// Similar to CreateAndOpenTemporaryFile, but the file is created in |dir|.
FILE* CreateAndOpenTemporaryFileInDir(const FilePath& dir, FilePath* path);

// Same as CreateTemporaryFileName but the file is created in |dir|.
bool CreateTemporaryFileNameInDir(const std::wstring& dir,
                                  std::wstring* temp_file);

// Create a new directory under TempPath. If prefix is provided, the new
// directory name is in the format of prefixyyyy.
// NOTE: prefix is ignored in the POSIX implementation.
// TODO(erikkay): is this OK?
// If success, return true and output the full path of the directory created.
bool CreateNewTempDirectory(const FilePath::StringType& prefix,
                            FilePath* new_temp_path);
// Deprecated temporary compatibility function.
bool CreateNewTempDirectory(const std::wstring& prefix,
                            std::wstring* new_temp_path);

// Creates a directory, as well as creating any parent directories, if they
// don't exist. Returns 'true' on successful creation, or if the directory
// already exists.
bool CreateDirectory(const FilePath& full_path);
// Deprecated temporary compatibility function.
bool CreateDirectory(const std::wstring& full_path);

// Returns the file size. Returns true on success.
bool GetFileSize(const FilePath& file_path, int64_t* file_size);
// Deprecated temporary compatibility function.
bool GetFileSize(const std::wstring& file_path, int64_t* file_size);

// Used to hold information about a given file path.  See GetFileInfo below.
struct FileInfo {
  // The size of the file in bytes.  Undefined when is_directory is true.
  int64_t size;

  // True if the file corresponds to a directory.
  bool is_directory;

  // Add additional fields here as needed.
};

// Returns information about the given file path.
bool GetFileInfo(const FilePath& file_path, FileInfo* info);
// Deprecated temporary compatibility function.
bool GetFileInfo(const std::wstring& file_path, FileInfo* info);

// Wrapper for fopen-like calls. Returns non-NULL FILE* on success.
FILE* OpenFile(const FilePath& filename, const char* mode);
// Deprecated temporary compatibility functions.
FILE* OpenFile(const std::string& filename, const char* mode);
FILE* OpenFile(const std::wstring& filename, const char* mode);

// Closes file opened by OpenFile. Returns true on success.
bool CloseFile(FILE* file);

// Reads the given number of bytes from the file into the buffer.  Returns
// the number of read bytes, or -1 on error.
int ReadFile(const FilePath& filename, char* data, int size);
// Deprecated temporary compatibility function.
int ReadFile(const std::wstring& filename, char* data, int size);

// Writes the given buffer into the file, overwriting any data that was
// previously there.  Returns the number of bytes written, or -1 on error.
int WriteFile(const FilePath& filename, const char* data, int size);
// Deprecated temporary compatibility function.
int WriteFile(const std::wstring& filename, const char* data, int size);

// Gets the current working directory for the process.
bool GetCurrentDirectory(FilePath* path);
// Deprecated temporary compatibility function.
bool GetCurrentDirectory(std::wstring* path);

// Sets the current working directory for the process.
bool SetCurrentDirectory(const FilePath& path);
// Deprecated temporary compatibility function.
bool SetCurrentDirectory(const std::wstring& current_directory);

}  // namespace file_util

#endif  // BASE_FILE_UTIL_H_
