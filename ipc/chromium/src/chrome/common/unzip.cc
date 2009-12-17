// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/unzip.h"

#include "base/file_util.h"
#include "base/string_util.h"
#include "net/base/file_stream.h"
#include "third_party/zlib/contrib/minizip/unzip.h"
#if defined(OS_WIN)
#include "third_party/zlib/contrib/minizip/iowin32.h"
#endif

static const int kZipMaxPath = 256;
static const int kUnzipBufSize = 8192;

// Extract the 'current' selected file from the zip into dest_dir.
// Output filename is stored in out_file.  Returns true on success.
static bool ExtractCurrentFile(unzFile zip_file,
                               const FilePath& dest_dir,
                               FilePath* out_file) {
  char filename_inzip[kZipMaxPath] = {0};
  unz_file_info file_info;
  int err = unzGetCurrentFileInfo(zip_file, &file_info, filename_inzip,
                                  sizeof(filename_inzip), NULL, 0, NULL, 0);
  if (err != UNZ_OK)
    return false;
  if (filename_inzip[0] == '\0')
    return false;

  err = unzOpenCurrentFile(zip_file);
  if (err != UNZ_OK)
    return false;

  FilePath::StringType filename;
  std::vector<FilePath::StringType> filename_parts;
#if defined(OS_WIN)
  filename = UTF8ToWide(filename_inzip);
#elif defined(OS_POSIX)
  filename = filename_inzip;
#endif
  SplitString(filename, '/', &filename_parts);

  FilePath dest_file(dest_dir);
  std::vector<FilePath::StringType>::iterator iter;
  for (iter = filename_parts.begin(); iter != filename_parts.end(); ++iter)
    dest_file = dest_file.Append(*iter);
  if (out_file)
    *out_file = dest_file;
  // If this is a directory, just create it and return.
  if (filename_inzip[strlen(filename_inzip) - 1] == '/') {
    if (!file_util::CreateDirectory(dest_file))
      return false;
    return true;
  }
  // TODO(erikkay): Can we always count on the directory entry coming before a
  // file in that directory?  If so, then these three lines can be removed.
  FilePath dir = dest_file.DirName();
  if (!file_util::CreateDirectory(dir))
    return false;

  net::FileStream stream;
  int flags = base::PLATFORM_FILE_CREATE_ALWAYS | base::PLATFORM_FILE_WRITE;
  if (stream.Open(dest_file, flags) != 0)
    return false;

  bool ret = true;
  int num_bytes = 0;
  char buf[kUnzipBufSize];
  do {
    num_bytes = unzReadCurrentFile(zip_file, buf, kUnzipBufSize);
    if (num_bytes < 0) {
      // If num_bytes < 0, then it's a specific UNZ_* error code.
      // While we're not currently handling these codes specifically, save
      // it away in case we want to in the future.
      err = num_bytes;
      break;
    }
    if (num_bytes > 0) {
      if (num_bytes != stream.Write(buf, num_bytes, NULL)) {
        ret = false;
        break;
      }
    }
  } while (num_bytes > 0);

  stream.Close();
  if (err == UNZ_OK)
    err = unzCloseCurrentFile(zip_file);
  else
    unzCloseCurrentFile(zip_file);  // Don't lose the original error code.
  if (err != UNZ_OK)
    ret = false;
  return ret;
}

#if defined(OS_WIN)
typedef struct {
  HANDLE hf;
  int error;
} WIN32FILE_IOWIN;

// This function is derived from third_party/minizip/iowin32.c.
// Its only difference is that it treats the char* as UTF8 and
// uses the Unicode version of CreateFile.
static void* UnzipOpenFunc(void *opaque, const char* filename, int mode) {
  DWORD desired_access, creation_disposition;
  DWORD share_mode, flags_and_attributes;
  HANDLE file = 0;
  void* ret = NULL;

  desired_access = share_mode = flags_and_attributes = 0;

  if ((mode & ZLIB_FILEFUNC_MODE_READWRITEFILTER) == ZLIB_FILEFUNC_MODE_READ) {
    desired_access = GENERIC_READ;
    creation_disposition = OPEN_EXISTING;
    share_mode = FILE_SHARE_READ;
  } else if (mode & ZLIB_FILEFUNC_MODE_EXISTING) {
    desired_access = GENERIC_WRITE | GENERIC_READ;
    creation_disposition = OPEN_EXISTING;
  } else if (mode & ZLIB_FILEFUNC_MODE_CREATE) {
    desired_access = GENERIC_WRITE | GENERIC_READ;
    creation_disposition = CREATE_ALWAYS;
  }

  std::wstring filename_wstr = UTF8ToWide(filename);
  if ((filename != NULL) && (desired_access != 0)) {
    file = CreateFile(filename_wstr.c_str(), desired_access, share_mode,
        NULL, creation_disposition, flags_and_attributes, NULL);
  }

  if (file == INVALID_HANDLE_VALUE)
    file = NULL;

  if (file != NULL) {
    WIN32FILE_IOWIN file_ret;
    file_ret.hf = file;
    file_ret.error = 0;
    ret = malloc(sizeof(WIN32FILE_IOWIN));
    if (ret == NULL)
      CloseHandle(file);
    else
      *(static_cast<WIN32FILE_IOWIN*>(ret)) = file_ret;
  }
  return ret;
}
#endif

// TODO(erikkay): Make this function asynchronous so that a large zip file
// won't starve the thread it's running on.  This won't be entirely possible
// since reads need to be synchronous, but we can at least make writes async.
bool Unzip(const FilePath& zip_path, const FilePath& dest_dir,
           std::vector<FilePath>* files) {
#if defined(OS_WIN)
  zlib_filefunc_def unzip_funcs;
  fill_win32_filefunc(&unzip_funcs);
  unzip_funcs.zopen_file = UnzipOpenFunc;
#endif

#if defined(OS_POSIX)
  std::string zip_file_str = zip_path.value();
  unzFile zip_file = unzOpen(zip_file_str.c_str());
#elif defined(OS_WIN)
  std::string zip_file_str = WideToUTF8(zip_path.value());
  unzFile zip_file = unzOpen2(zip_file_str.c_str(), &unzip_funcs);
#endif
  if (!zip_file) {
    LOG(WARNING) << "couldn't open extension file " << zip_file_str;
    return false;
  }
  unz_global_info zip_info;
  int err;
  err = unzGetGlobalInfo(zip_file, &zip_info);
  if (err != UNZ_OK) {
    LOG(WARNING) << "couldn't open extension file " << zip_file_str;
    return false;
  }
  bool ret = true;
  for (unsigned int i = 0; i < zip_info.number_entry; ++i) {
    FilePath dest_file;
    if (!ExtractCurrentFile(zip_file, dest_dir, &dest_file)) {
      ret = false;
      break;
    }
    if (files)
      files->push_back(dest_file);

    if (i + 1 < zip_info.number_entry) {
      err = unzGoToNextFile(zip_file);
      if (err != UNZ_OK) {
        LOG(WARNING) << "error %d in unzGoToNextFile";
        ret = false;
        break;
      }
    }
  }
  unzClose(zip_file);
  return ret;
}
