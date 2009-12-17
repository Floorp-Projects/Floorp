// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_CHROME_PATHS_H__
#define CHROME_COMMON_CHROME_PATHS_H__

// This file declares path keys for the chrome module.  These can be used with
// the PathService to access various special directories and files.

namespace chrome {

enum {
  PATH_START = 1000,

  DIR_APP = PATH_START,  // directory where dlls and data reside
  DIR_LOGS,              // directory where logs should be written
  DIR_USER_DATA,         // directory where user data can be written
  DIR_CRASH_DUMPS,       // directory where crash dumps are written
  DIR_USER_DESKTOP,      // directory that correspond to the desktop
  DIR_RESOURCES,         // directory where application resources are stored
  DIR_INSPECTOR,         // directory where web inspector is located
  DIR_THEMES,            // directory where theme dll files are stored
  DIR_LOCALES,           // directory where locale resources are stored
  DIR_APP_DICTIONARIES,  // directory where the global dictionaries are
  DIR_USER_DOCUMENTS,    // directory for a user's "My Documents"
  DIR_DEFAULT_DOWNLOADS, // directory for a user's "My Documents/Downloads"
  FILE_RESOURCE_MODULE,  // full path and filename of the module that contains
                         // embedded resources (version, strings, images, etc.)
  FILE_LOCAL_STATE,      // path and filename to the file in which machine/
                         // installation-specific state is saved
  FILE_RECORDED_SCRIPT,  // full path to the script.log file that contains
                         // recorded browser events for playback.
  FILE_GEARS_PLUGIN,     // full path to the gears.dll plugin file.
  FILE_LIBAVCODEC,       // full path to libavcodec media decoding library.
  FILE_LIBAVFORMAT,      // full path to libavformat media parsing library.
  FILE_LIBAVUTIL,        // full path to libavutil media utility library.

  // Valid only in development environment; TODO(darin): move these
  DIR_TEST_DATA,         // directory where unit test data resides
  DIR_TEST_TOOLS,        // directory where unit test tools reside
  FILE_TEST_SERVER,      // simple HTTP server for testing the network stack
  FILE_PYTHON_RUNTIME,   // Python runtime, used for running the test server

  PATH_END
};

// Call once to register the provider for the path keys defined above.
void RegisterPathProvider();

}  // namespace chrome

#endif  // CHROME_COMMON_CHROME_PATHS_H__
