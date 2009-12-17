// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test_file_util.h"

#include <windows.h>

#include <vector>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/scoped_handle.h"

namespace file_util {

// We could use GetSystemInfo to get the page size, but this serves
// our purpose fine since 4K is the page size on x86 as well as x64.
static const ptrdiff_t kPageSize = 4096;

bool EvictFileFromSystemCache(const FilePath& file) {
  // Request exclusive access to the file and overwrite it with no buffering.
  ScopedHandle file_handle(
      CreateFile(file.value().c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL,
                 OPEN_EXISTING, FILE_FLAG_NO_BUFFERING, NULL));
  if (!file_handle)
    return false;

  // Get some attributes to restore later.
  BY_HANDLE_FILE_INFORMATION bhi = {0};
  CHECK(::GetFileInformationByHandle(file_handle, &bhi));

  // Execute in chunks. It could be optimized. We want to do few of these since
  // these operations will be slow without the cache.

  // Non-buffered reads and writes need to be sector aligned and since sector
  // sizes typically range from 512-4096 bytes, we just use the page size.
  // The buffer size is twice the size of a page (minus one) since we need to
  // get an aligned pointer into the buffer that we can use.
  char buffer[2 * kPageSize - 1];
  // Get an aligned pointer into buffer.
  char* read_write = reinterpret_cast<char*>(
      reinterpret_cast<ptrdiff_t>(buffer + kPageSize - 1) & ~(kPageSize - 1));
  DCHECK((reinterpret_cast<int>(read_write) % kPageSize) == 0);

  // If the file size isn't a multiple of kPageSize, we'll need special
  // processing.
  bool file_is_page_aligned = true;
  int total_bytes = 0;
  DWORD bytes_read, bytes_written;
  for (;;) {
    bytes_read = 0;
    ReadFile(file_handle, read_write, kPageSize, &bytes_read, NULL);
    if (bytes_read == 0)
      break;

    if (bytes_read < kPageSize) {
      // Zero out the remaining part of the buffer.
      // WriteFile will fail if we provide a buffer size that isn't a
      // sector multiple, so we'll have to write the entire buffer with
      // padded zeros and then use SetEndOfFile to truncate the file.
      ZeroMemory(read_write + bytes_read, kPageSize - bytes_read);
      file_is_page_aligned = false;
    }

    // Move back to the position we just read from.
    // Note that SetFilePointer will also fail if total_bytes isn't sector
    // aligned, but that shouldn't happen here.
    DCHECK((total_bytes % kPageSize) == 0);
    SetFilePointer(file_handle, total_bytes, NULL, FILE_BEGIN);
    if (!WriteFile(file_handle, read_write, kPageSize, &bytes_written, NULL) ||
        bytes_written != kPageSize) {
      DCHECK(false);
      return false;
    }

    total_bytes += bytes_read;

    // If this is false, then we just processed the last portion of the file.
    if (!file_is_page_aligned)
      break;
  }

  if (!file_is_page_aligned) {
    // The size of the file isn't a multiple of the page size, so we'll have
    // to open the file again, this time without the FILE_FLAG_NO_BUFFERING
    // flag and use SetEndOfFile to mark EOF.
    file_handle.Set(NULL);
    file_handle.Set(CreateFile(file.value().c_str(), GENERIC_WRITE, 0, NULL,
                               OPEN_EXISTING, 0, NULL));
    CHECK(SetFilePointer(file_handle, total_bytes, NULL, FILE_BEGIN) !=
          INVALID_SET_FILE_POINTER);
    CHECK(::SetEndOfFile(file_handle));
  }

  // Restore the file attributes.
  CHECK(::SetFileTime(file_handle, &bhi.ftCreationTime, &bhi.ftLastAccessTime,
                      &bhi.ftLastWriteTime));

  return true;
}

// Like CopyFileNoCache but recursively copies all files and subdirectories
// in the given input directory to the output directory.
bool CopyRecursiveDirNoCache(const std::wstring& source_dir,
                             const std::wstring& dest_dir) {
  // Try to create the directory if it doesn't already exist.
  if (!CreateDirectory(dest_dir)) {
    if (GetLastError() != ERROR_ALREADY_EXISTS)
      return false;
  }

  std::vector<std::wstring> files_copied;

  std::wstring src(source_dir);
  file_util::AppendToPath(&src, L"*");

  WIN32_FIND_DATA fd;
  HANDLE fh = FindFirstFile(src.c_str(), &fd);
  if (fh == INVALID_HANDLE_VALUE)
    return false;

  do {
    std::wstring cur_file(fd.cFileName);
    if (cur_file == L"." || cur_file == L"..")
      continue;  // Skip these special entries.

    std::wstring cur_source_path(source_dir);
    file_util::AppendToPath(&cur_source_path, cur_file);

    std::wstring cur_dest_path(dest_dir);
    file_util::AppendToPath(&cur_dest_path, cur_file);

    if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
      // Recursively copy a subdirectory. We stripped "." and ".." already.
      if (!CopyRecursiveDirNoCache(cur_source_path, cur_dest_path)) {
        FindClose(fh);
        return false;
      }
    } else {
      // Copy the file.
      if (!::CopyFile(cur_source_path.c_str(), cur_dest_path.c_str(), false)) {
        FindClose(fh);
        return false;
      }

      // We don't check for errors from this function, often, we are copying
      // files that are in the repository, and they will have read-only set.
      // This will prevent us from evicting from the cache, but these don't
      // matter anyway.
      EvictFileFromSystemCache(FilePath::FromWStringHack(cur_dest_path));
    }
  } while (FindNextFile(fh, &fd));

  FindClose(fh);
  return true;
}

}  // namespace file_util
