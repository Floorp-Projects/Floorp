// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chrome_paths_internal.h"

#include <glib.h>
#include <stdlib.h>

#include "base/file_path.h"
#include "base/path_service.h"

namespace {

FilePath GetHomeDir() {
  const char *home_dir = getenv("HOME");

  if (home_dir && home_dir[0])
    return FilePath(home_dir);

  home_dir = g_get_home_dir();
  if (home_dir && home_dir[0])
    return FilePath(home_dir);

  FilePath rv;
  if (PathService::Get(base::DIR_TEMP, &rv))
    return rv;

  /* last resort */
  return FilePath("/tmp/");
}

// Wrapper around xdg_user_dir_lookup() from
// src/chrome/third_party/xdg-user-dirs
FilePath GetXDGUserDirectory(const char* env_name, const char* fallback_dir) {
  return GetHomeDir().Append(fallback_dir);
}

// |env_name| is the name of an environment variable that we want to use to get
// a directory path. |fallback_dir| is the directory relative to $HOME that we
// use if |env_name| cannot be found or is empty. |fallback_dir| may be NULL.
FilePath GetXDGDirectory(const char* env_name, const char* fallback_dir) {
  const char* env_value = getenv(env_name);
  if (env_value && env_value[0])
    return FilePath(env_value);
  return GetHomeDir().Append(fallback_dir);
}

}  // namespace

namespace chrome {

// See http://standards.freedesktop.org/basedir-spec/basedir-spec-latest.html
// for a spec on where config files go.  The net effect for most
// systems is we use ~/.config/chromium/ for Chromium and
// ~/.config/google-chrome/ for official builds.
// (This also helps us sidestep issues with other apps grabbing ~/.chromium .)
bool GetDefaultUserDataDirectory(FilePath* result) {
  FilePath config_dir(GetXDGDirectory("XDG_CONFIG_HOME", ".config"));
#if defined(GOOGLE_CHROME_BUILD)
  *result = config_dir.Append("google-chrome");
#else
  *result = config_dir.Append("chromium");
#endif
  return true;
}

bool GetUserDocumentsDirectory(FilePath* result) {
  *result = GetXDGUserDirectory("DOCUMENTS", "Documents");
  return true;
}

// We respect the user's preferred download location, unless it is
// ~ or their desktop directory, in which case we default to ~/Downloads.
bool GetUserDownloadsDirectory(FilePath* result) {
  *result = GetXDGUserDirectory("DOWNLOAD", "Downloads");

  FilePath home = GetHomeDir();
  if (*result == home) {
    *result = home.Append("Downloads");
    return true;
  }

  FilePath desktop;
  GetUserDesktop(&desktop);
  if (*result == desktop) {
    *result = home.Append("Downloads");
  }

  return true;
}

bool GetUserDesktop(FilePath* result) {
  *result = GetXDGUserDirectory("DESKTOP", "Desktop");
  return true;
}

}  // namespace chrome
