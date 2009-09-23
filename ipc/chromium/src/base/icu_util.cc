// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

#include <string>

#include "base/icu_util.h"

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/sys_string_conversions.h"
#include "unicode/putil.h"
#include "unicode/udata.h"

#define ICU_UTIL_DATA_FILE   0
#define ICU_UTIL_DATA_SHARED 1
#define ICU_UTIL_DATA_STATIC 2

#ifndef ICU_UTIL_DATA_IMPL

#if defined(OS_WIN)
#define ICU_UTIL_DATA_IMPL ICU_UTIL_DATA_SHARED
#elif defined(OS_MACOSX)
#define ICU_UTIL_DATA_IMPL ICU_UTIL_DATA_STATIC
#elif defined(OS_LINUX)
#define ICU_UTIL_DATA_IMPL ICU_UTIL_DATA_FILE
#endif

#endif  // ICU_UTIL_DATA_IMPL

#if defined(OS_WIN)
#define ICU_UTIL_DATA_SYMBOL "icudt38_dat"
#define ICU_UTIL_DATA_SHARED_MODULE_NAME L"icudt38.dll"
#endif

namespace icu_util {

bool Initialize() {
#ifndef NDEBUG
  // Assert that we are not called more than once.  Even though calling this
  // function isn't harmful (ICU can handle it), being called twice probably
  // indicates a programming error.
  static bool called_once = false;
  DCHECK(!called_once);
  called_once = true;
#endif

#if (ICU_UTIL_DATA_IMPL == ICU_UTIL_DATA_SHARED)
  // We expect to find the ICU data module alongside the current module.
  std::wstring data_path;
  PathService::Get(base::DIR_MODULE, &data_path);
  file_util::AppendToPath(&data_path, ICU_UTIL_DATA_SHARED_MODULE_NAME);

  HMODULE module = LoadLibrary(data_path.c_str());
  if (!module)
    return false;

  FARPROC addr = GetProcAddress(module, ICU_UTIL_DATA_SYMBOL);
  if (!addr)
    return false;

  UErrorCode err = U_ZERO_ERROR;
  udata_setCommonData(reinterpret_cast<void*>(addr), &err);
  return err == U_ZERO_ERROR;
#elif (ICU_UTIL_DATA_IMPL == ICU_UTIL_DATA_STATIC)
  // Mac bundles the ICU data in.
  return true;
#elif (ICU_UTIL_DATA_IMPL == ICU_UTIL_DATA_FILE)
  // For now, expect the data file to be alongside the executable.
  // This is sufficient while we work on unit tests, but will eventually
  // likely live in a data directory.
  FilePath data_path;
  bool path_ok = PathService::Get(base::DIR_EXE, &data_path);
  DCHECK(path_ok);
  u_setDataDirectory(data_path.value().c_str());
  // Only look for the packaged data file;
  // the default behavior is to look for individual files.
  UErrorCode err = U_ZERO_ERROR;
  udata_setFileAccess(UDATA_ONLY_PACKAGES, &err);
  return err == U_ZERO_ERROR;
#endif
}

}  // namespace icu_util
