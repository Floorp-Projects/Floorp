// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chrome_paths.h"

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/sys_info.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/chrome_switches.h"

namespace chrome {

bool GetGearsPluginPathFromCommandLine(FilePath* path) {
#ifndef NDEBUG
  // for debugging, support a cmd line based override
  std::wstring plugin_path = CommandLine::ForCurrentProcess()->GetSwitchValue(
      switches::kGearsPluginPathOverride);
  // TODO(tc): After GetSwitchNativeValue lands, we don't need to use
  // FromWStringHack.
  *path = FilePath::FromWStringHack(plugin_path);
  return !plugin_path.empty();
#else
  return false;
#endif
}

bool PathProvider(int key, FilePath* result) {
#ifndef CHROMIUM_MOZILLA_BUILD
  // Some keys are just aliases...
  switch (key) {
    case chrome::DIR_APP:
      return PathService::Get(base::DIR_MODULE, result);
    case chrome::DIR_LOGS:
#ifndef NDEBUG
      return PathService::Get(chrome::DIR_USER_DATA, result);
#else
      return PathService::Get(base::DIR_EXE, result);
#endif
    case chrome::FILE_RESOURCE_MODULE:
      return PathService::Get(base::FILE_MODULE, result);
  }

  // Assume that we will not need to create the directory if it does not exist.
  // This flag can be set to true for the cases where we want to create it.
  bool create_dir = false;

  FilePath cur;
  switch (key) {
    case chrome::DIR_USER_DATA:
      if (!GetDefaultUserDataDirectory(&cur))
        return false;
      create_dir = true;
      break;
    case chrome::DIR_USER_DOCUMENTS:
      if (!GetUserDocumentsDirectory(&cur))
        return false;
      create_dir = true;
      break;
    case chrome::DIR_DEFAULT_DOWNLOADS:
      if (!GetUserDownloadsDirectory(&cur))
        return false;
      break;
    case chrome::DIR_CRASH_DUMPS:
      // The crash reports are always stored relative to the default user data
      // directory.  This avoids the problem of having to re-initialize the
      // exception handler after parsing command line options, which may
      // override the location of the app's profile directory.
      if (!GetDefaultUserDataDirectory(&cur))
        return false;
      cur = cur.Append(FILE_PATH_LITERAL("Crash Reports"));
      create_dir = true;
      break;
    case chrome::DIR_USER_DESKTOP:
      if (!GetUserDesktop(&cur))
        return false;
      break;
    case chrome::DIR_RESOURCES:
      if (!PathService::Get(chrome::DIR_APP, &cur))
        return false;
      cur = cur.Append(FILE_PATH_LITERAL("resources"));
      create_dir = true;
      break;
    case chrome::DIR_INSPECTOR:
      if (!PathService::Get(chrome::DIR_APP, &cur))
        return false;
      cur = cur.Append(FILE_PATH_LITERAL("resources"));
      cur = cur.Append(FILE_PATH_LITERAL("inspector"));
      break;
    case chrome::DIR_THEMES:
      if (!PathService::Get(chrome::DIR_APP, &cur))
        return false;
      cur = cur.Append(FILE_PATH_LITERAL("themes"));
      create_dir = true;
      break;
    case chrome::DIR_LOCALES:
      if (!PathService::Get(chrome::DIR_APP, &cur))
        return false;
#if defined(OS_MACOSX)
      // On Mac, locale files are in Contents/Resources, a sibling of the
      // App dir.
      cur = cur.DirName();
      cur = cur.Append(FILE_PATH_LITERAL("Resources"));
#else
      cur = cur.Append(FILE_PATH_LITERAL("locales"));
#endif
      create_dir = true;
      break;
    case chrome::DIR_APP_DICTIONARIES:
#if defined(OS_LINUX)
      // We can't write into the EXE dir on Linux, so keep dictionaries
      // alongside the safe browsing database in the user data dir.
      if (!PathService::Get(chrome::DIR_USER_DATA, &cur))
        return false;
#else
      if (!PathService::Get(base::DIR_EXE, &cur))
        return false;
#endif
      cur = cur.Append(FILE_PATH_LITERAL("Dictionaries"));
      create_dir = true;
      break;
    case chrome::FILE_LOCAL_STATE:
      if (!PathService::Get(chrome::DIR_USER_DATA, &cur))
        return false;
#ifdef CHROMIUM_MOZILLA_BUILD
      cur = cur.Append(FILE_PATH_LITERAL("Local State"));
#else
      cur = cur.Append(chrome::kLocalStateFilename);
#endif
      break;
    case chrome::FILE_RECORDED_SCRIPT:
      if (!PathService::Get(chrome::DIR_USER_DATA, &cur))
        return false;
      cur = cur.Append(FILE_PATH_LITERAL("script.log"));
      break;
    case chrome::FILE_GEARS_PLUGIN:
      if (!GetGearsPluginPathFromCommandLine(&cur)) {
#if defined(OS_WIN)
        // Search for gears.dll alongside chrome.dll first.  This new model
        // allows us to package gears.dll with the Chrome installer and update
        // it while Chrome is running.
        if (!PathService::Get(base::DIR_MODULE, &cur))
          return false;
        cur = cur.Append(FILE_PATH_LITERAL("gears.dll"));

        if (!file_util::PathExists(cur)) {
          if (!PathService::Get(base::DIR_EXE, &cur))
            return false;
          cur = cur.Append(FILE_PATH_LITERAL("plugins"));
          cur = cur.Append(FILE_PATH_LITERAL("gears"));
          cur = cur.Append(FILE_PATH_LITERAL("gears.dll"));
        }
#else
        // No gears.dll on non-Windows systems.
        return false;
#endif
      }
      break;
    // The following are only valid in the development environment, and
    // will fail if executed from an installed executable (because the
    // generated path won't exist).
    case chrome::DIR_TEST_DATA:
      if (!PathService::Get(base::DIR_SOURCE_ROOT, &cur))
        return false;
      cur = cur.Append(FILE_PATH_LITERAL("chrome"));
      cur = cur.Append(FILE_PATH_LITERAL("test"));
      cur = cur.Append(FILE_PATH_LITERAL("data"));
      if (!file_util::PathExists(cur))  // we don't want to create this
        return false;
      break;
    case chrome::DIR_TEST_TOOLS:
      if (!PathService::Get(base::DIR_SOURCE_ROOT, &cur))
        return false;
      cur = cur.Append(FILE_PATH_LITERAL("chrome"));
      cur = cur.Append(FILE_PATH_LITERAL("tools"));
      cur = cur.Append(FILE_PATH_LITERAL("test"));
      if (!file_util::PathExists(cur))  // we don't want to create this
        return false;
      break;
    case chrome::FILE_PYTHON_RUNTIME:
      if (!PathService::Get(base::DIR_SOURCE_ROOT, &cur))
        return false;
      cur = cur.Append(FILE_PATH_LITERAL("third_party"));
      cur = cur.Append(FILE_PATH_LITERAL("python_24"));
      cur = cur.Append(FILE_PATH_LITERAL("python.exe"));
      if (!file_util::PathExists(cur))  // we don't want to create this
        return false;
      break;
    case chrome::FILE_TEST_SERVER:
      if (!PathService::Get(base::DIR_SOURCE_ROOT, &cur))
        return false;
      cur = cur.Append(FILE_PATH_LITERAL("net"));
      cur = cur.Append(FILE_PATH_LITERAL("tools"));
      cur = cur.Append(FILE_PATH_LITERAL("test"));
      cur = cur.Append(FILE_PATH_LITERAL("testserver"));
      cur = cur.Append(FILE_PATH_LITERAL("testserver.py"));
      if (!file_util::PathExists(cur))  // we don't want to create this
        return false;
      break;
    default:
      return false;
  }

  if (create_dir && !file_util::PathExists(cur) &&
      !file_util::CreateDirectory(cur))
    return false;

  *result = cur;
#endif
  return true;
}

// This cannot be done as a static initializer sadly since Visual Studio will
// eliminate this object file if there is no direct entry point into it.
void RegisterPathProvider() {
  PathService::RegisterProvider(PathProvider, PATH_START, PATH_END);
}

}  // namespace chrome
