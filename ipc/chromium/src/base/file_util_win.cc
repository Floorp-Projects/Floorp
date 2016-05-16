/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"

#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <time.h>
#include <string>

#include "base/file_path.h"
#include "base/logging.h"
#include "base/scoped_handle.h"
#include "base/string_util.h"
#include "base/time.h"
#include "base/win_util.h"

namespace file_util {

bool AbsolutePath(FilePath* path) {
  wchar_t file_path_buf[MAX_PATH];
  if (!_wfullpath(file_path_buf, path->value().c_str(), MAX_PATH))
    return false;
  *path = FilePath(file_path_buf);
  return true;
}

bool Delete(const FilePath& path) {
  if (path.value().length() >= MAX_PATH)
    return false;

  // Use DeleteFile; it should be faster. DeleteFile
  // fails if passed a directory though, which is why we fall through on
  // failure to the SHFileOperation.
  if (DeleteFile(path.value().c_str()) != 0)
    return true;

  // SHFILEOPSTRUCT wants the path to be terminated with two NULLs,
  // so we have to use wcscpy because wcscpy_s writes non-NULLs
  // into the rest of the buffer.
  wchar_t double_terminated_path[MAX_PATH + 1] = {0};
#pragma warning(suppress:4996)  // don't complain about wcscpy deprecation
  wcscpy(double_terminated_path, path.value().c_str());

  SHFILEOPSTRUCT file_operation = {0};
  file_operation.wFunc = FO_DELETE;
  file_operation.pFrom = double_terminated_path;
  file_operation.fFlags = FOF_NOERRORUI | FOF_SILENT | FOF_NOCONFIRMATION;
  file_operation.fFlags |= FOF_NORECURSION | FOF_FILESONLY;
  int err = SHFileOperation(&file_operation);
  // Some versions of Windows return ERROR_FILE_NOT_FOUND when
  // deleting an empty directory.
  return (err == 0 || err == ERROR_FILE_NOT_FOUND);
}

bool CopyFile(const FilePath& from_path, const FilePath& to_path) {
  // NOTE: I suspect we could support longer paths, but that would involve
  // analyzing all our usage of files.
  if (from_path.value().length() >= MAX_PATH ||
      to_path.value().length() >= MAX_PATH) {
    return false;
  }
  return (::CopyFile(from_path.value().c_str(), to_path.value().c_str(),
                     false) != 0);
}

bool ShellCopy(const FilePath& from_path, const FilePath& to_path,
               bool recursive) {
  // NOTE: I suspect we could support longer paths, but that would involve
  // analyzing all our usage of files.
  if (from_path.value().length() >= MAX_PATH ||
      to_path.value().length() >= MAX_PATH) {
    return false;
  }

  // SHFILEOPSTRUCT wants the path to be terminated with two NULLs,
  // so we have to use wcscpy because wcscpy_s writes non-NULLs
  // into the rest of the buffer.
  wchar_t double_terminated_path_from[MAX_PATH + 1] = {0};
  wchar_t double_terminated_path_to[MAX_PATH + 1] = {0};
#pragma warning(suppress:4996)  // don't complain about wcscpy deprecation
  wcscpy(double_terminated_path_from, from_path.value().c_str());
#pragma warning(suppress:4996)  // don't complain about wcscpy deprecation
  wcscpy(double_terminated_path_to, to_path.value().c_str());

  SHFILEOPSTRUCT file_operation = {0};
  file_operation.wFunc = FO_COPY;
  file_operation.pFrom = double_terminated_path_from;
  file_operation.pTo = double_terminated_path_to;
  file_operation.fFlags = FOF_NOERRORUI | FOF_SILENT | FOF_NOCONFIRMATION |
                          FOF_NOCONFIRMMKDIR;
  if (!recursive)
    file_operation.fFlags |= FOF_NORECURSION | FOF_FILESONLY;

  return (SHFileOperation(&file_operation) == 0);
}

bool PathExists(const FilePath& path) {
  return (GetFileAttributes(path.value().c_str()) != INVALID_FILE_ATTRIBUTES);
}

bool PathIsWritable(const FilePath& path) {
  HANDLE dir =
      CreateFile(path.value().c_str(), FILE_ADD_FILE,
                 FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                 NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);

  if (dir == INVALID_HANDLE_VALUE)
    return false;

  CloseHandle(dir);
  return true;
}

bool DirectoryExists(const FilePath& path) {
  DWORD fileattr = GetFileAttributes(path.value().c_str());
  if (fileattr != INVALID_FILE_ATTRIBUTES)
    return (fileattr & FILE_ATTRIBUTE_DIRECTORY) != 0;
  return false;
}

bool GetTempDir(FilePath* path) {
  wchar_t temp_path[MAX_PATH + 1];
  DWORD path_len = ::GetTempPath(MAX_PATH, temp_path);
  if (path_len >= MAX_PATH || path_len <= 0)
    return false;
  // TODO(evanm): the old behavior of this function was to always strip the
  // trailing slash.  We duplicate this here, but it shouldn't be necessary
  // when everyone is using the appropriate FilePath APIs.
  std::wstring path_str(temp_path);
  TrimTrailingSeparator(&path_str);
  *path = FilePath(path_str);
  return true;
}

bool GetShmemTempDir(FilePath* path) {
  return GetTempDir(path);
}

bool CreateTemporaryFileName(FilePath* path) {
  std::wstring temp_path, temp_file;

  if (!GetTempDir(&temp_path))
    return false;

  if (CreateTemporaryFileNameInDir(temp_path, &temp_file)) {
    *path = FilePath(temp_file);
    return true;
  }

  return false;
}

FILE* CreateAndOpenTemporaryShmemFile(FilePath* path) {
  return CreateAndOpenTemporaryFile(path);
}

// On POSIX we have semantics to create and open a temporary file
// atomically.
// TODO(jrg): is there equivalent call to use on Windows instead of
// going 2-step?
FILE* CreateAndOpenTemporaryFileInDir(const FilePath& dir, FilePath* path) {
  std::wstring wstring_path;
  if (!CreateTemporaryFileNameInDir(dir.value(), &wstring_path)) {
    return NULL;
  }
  *path = FilePath(wstring_path);
  // Open file in binary mode, to avoid problems with fwrite. On Windows
  // it replaces \n's with \r\n's, which may surprise you.
  // Reference: http://msdn.microsoft.com/en-us/library/h9t88zwz(VS.71).aspx
  return OpenFile(*path, "wb+");
}

bool CreateTemporaryFileNameInDir(const std::wstring& dir,
                                  std::wstring* temp_file) {
  wchar_t temp_name[MAX_PATH + 1];

  if (!GetTempFileName(dir.c_str(), L"", 0, temp_name))
    return false;  // fail!

  DWORD path_len = GetLongPathName(temp_name, temp_name, MAX_PATH);
  if (path_len > MAX_PATH + 1 || path_len == 0)
    return false;  // fail!

  temp_file->assign(temp_name, path_len);
  return true;
}

bool CreateNewTempDirectory(const FilePath::StringType& prefix,
                            FilePath* new_temp_path) {
  FilePath system_temp_dir;
  if (!GetTempDir(&system_temp_dir))
    return false;

  FilePath path_to_create;
  srand(static_cast<uint32_t>(time(NULL)));

  int count = 0;
  while (count < 50) {
    // Try create a new temporary directory with random generated name. If
    // the one exists, keep trying another path name until we reach some limit.
    path_to_create = system_temp_dir;
    std::wstring new_dir_name;
    new_dir_name.assign(prefix);
    new_dir_name.append(IntToWString(rand() % kint16max));
    path_to_create = path_to_create.Append(new_dir_name);

    if (::CreateDirectory(path_to_create.value().c_str(), NULL))
      break;
    count++;
  }

  if (count == 50) {
    return false;
  }

  *new_temp_path = path_to_create;
  return true;
}

bool CreateDirectory(const FilePath& full_path) {
  if (DirectoryExists(full_path))
    return true;
  int err = SHCreateDirectoryEx(NULL, full_path.value().c_str(), NULL);
  return err == ERROR_SUCCESS;
}

bool GetFileInfo(const FilePath& file_path, FileInfo* results) {
  WIN32_FILE_ATTRIBUTE_DATA attr;
  if (!GetFileAttributesEx(file_path.ToWStringHack().c_str(),
                           GetFileExInfoStandard, &attr)) {
    return false;
  }

  ULARGE_INTEGER size;
  size.HighPart = attr.nFileSizeHigh;
  size.LowPart = attr.nFileSizeLow;
  results->size = size.QuadPart;

  results->is_directory =
      (attr.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
  return true;
}

FILE* OpenFile(const FilePath& filename, const char* mode) {
  std::wstring w_mode = ASCIIToWide(std::string(mode));
  FILE* file;
  if (_wfopen_s(&file, filename.value().c_str(), w_mode.c_str()) != 0) {
    return NULL;
  }
  return file;
}

FILE* OpenFile(const std::string& filename, const char* mode) {
  FILE* file;
  if (fopen_s(&file, filename.c_str(), mode) != 0) {
    return NULL;
  }
  return file;
}

int ReadFile(const FilePath& filename, char* data, int size) {
  ScopedHandle file(CreateFile(filename.value().c_str(),
                               GENERIC_READ,
                               FILE_SHARE_READ | FILE_SHARE_WRITE,
                               NULL,
                               OPEN_EXISTING,
                               FILE_FLAG_SEQUENTIAL_SCAN,
                               NULL));
  if (file == INVALID_HANDLE_VALUE)
    return -1;

  int ret_value;
  DWORD read;
  if (::ReadFile(file, data, size, &read, NULL) && read == size) {
    ret_value = static_cast<int>(read);
  } else {
    ret_value = -1;
  }

  return ret_value;
}

int WriteFile(const FilePath& filename, const char* data, int size) {
  ScopedHandle file(CreateFile(filename.value().c_str(),
                               GENERIC_WRITE,
                               0,
                               NULL,
                               CREATE_ALWAYS,
                               0,
                               NULL));
  if (file == INVALID_HANDLE_VALUE) {
    CHROMIUM_LOG(WARNING) << "CreateFile failed for path " << filename.value() <<
        " error code=" << GetLastError() <<
        " error text=" << win_util::FormatLastWin32Error();
    return -1;
  }

  DWORD written;
  BOOL result = ::WriteFile(file, data, size, &written, NULL);
  if (result && written == size)
    return static_cast<int>(written);

  if (!result) {
    // WriteFile failed.
    CHROMIUM_LOG(WARNING) << "writing file " << filename.value() <<
        " failed, error code=" << GetLastError() <<
        " description=" << win_util::FormatLastWin32Error();
  } else {
    // Didn't write all the bytes.
    CHROMIUM_LOG(WARNING) << "wrote" << written << " bytes to " <<
        filename.value() << " expected " << size;
  }
  return -1;
}

// Gets the current working directory for the process.
bool GetCurrentDirectory(FilePath* dir) {
  wchar_t system_buffer[MAX_PATH];
  system_buffer[0] = 0;
  DWORD len = ::GetCurrentDirectory(MAX_PATH, system_buffer);
  if (len == 0 || len > MAX_PATH)
    return false;
  // TODO(evanm): the old behavior of this function was to always strip the
  // trailing slash.  We duplicate this here, but it shouldn't be necessary
  // when everyone is using the appropriate FilePath APIs.
  std::wstring dir_str(system_buffer);
  file_util::TrimTrailingSeparator(&dir_str);
  *dir = FilePath(dir_str);
  return true;
}

// Sets the current working directory for the process.
bool SetCurrentDirectory(const FilePath& directory) {
  BOOL ret = ::SetCurrentDirectory(directory.value().c_str());
  return ret != 0;
}

// Deprecated functions ----------------------------------------------------

void InsertBeforeExtension(std::wstring* path_str,
                           const std::wstring& suffix) {
  FilePath path(*path_str);
  InsertBeforeExtension(&path, suffix);
  path_str->assign(path.value());
}
void ReplaceExtension(std::wstring* file_name, const std::wstring& extension) {
  FilePath path(*file_name);
  ReplaceExtension(&path, extension);
  file_name->assign(path.value());
}
}  // namespace file_util
