// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chrome_paths_internal.h"

#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>

#include "base/file_path.h"
#include "base/path_service.h"
#include "chrome/common/chrome_constants.h"

namespace chrome {

bool GetDefaultUserDataDirectory(FilePath* result) {
  if (!PathService::Get(base::DIR_LOCAL_APP_DATA, result))
    return false;
#if defined(GOOGLE_CHROME_BUILD)
  *result = result->Append(FILE_PATH_LITERAL("Google"));
#endif
  *result = result->Append(chrome::kBrowserAppName);
  *result = result->Append(chrome::kUserDataDirname);
  return true;
}

bool GetUserDocumentsDirectory(FilePath* result) {
  wchar_t path_buf[MAX_PATH];
  if (FAILED(SHGetFolderPath(NULL, CSIDL_MYDOCUMENTS, NULL,
                             SHGFP_TYPE_CURRENT, path_buf)))
    return false;
  *result = FilePath(path_buf);
  return true;
}

// On Vista, we can get the download path using a Win API
// (http://msdn.microsoft.com/en-us/library/bb762584(VS.85).aspx),
// but it can be set to Desktop, which is dangerous. Instead,
// we just use 'Downloads' under DIR_USER_DOCUMENTS. Localizing
// 'downloads' is not a good idea because Chrome's UI language
// can be changed.
bool GetUserDownloadsDirectory(FilePath* result) {
  if (!GetUserDocumentsDirectory(result))
    return false;

  *result = result->Append(L"Downloads");
  return true;
}

bool GetUserDesktop(FilePath* result) {
  // We need to go compute the value. It would be nice to support paths
  // with names longer than MAX_PATH, but the system functions don't seem
  // to be designed for it either, with the exception of GetTempPath
  // (but other things will surely break if the temp path is too long,
  // so we don't bother handling it.
  wchar_t system_buffer[MAX_PATH];
  system_buffer[0] = 0;
  if (FAILED(SHGetFolderPath(NULL, CSIDL_DESKTOPDIRECTORY, NULL,
                             SHGFP_TYPE_CURRENT, system_buffer)))
    return false;
  *result = FilePath(system_buffer);
  return true;
}

}  // namespace chrome
