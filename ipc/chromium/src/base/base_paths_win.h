// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_BASE_PATHS_WIN_H__
#define BASE_BASE_PATHS_WIN_H__

// This file declares windows-specific path keys for the base module.
// These can be used with the PathService to access various special
// directories and files.

namespace base {

enum {
  PATH_WIN_START = 100,

  FILE_EXE,     // path and filename of the current executable
  FILE_MODULE,  // path and filename of the module containing the code for the
                // PathService (which could differ from FILE_EXE if the
                // PathService were compiled into a DLL, for example)
  DIR_WINDOWS,  // Windows directory, usually "c:\windows"
  DIR_SYSTEM,   // Usually c:\windows\system32"
  DIR_PROGRAM_FILES, // Usually c:\program files

  DIR_IE_INTERNET_CACHE,  // Temporary Internet Files directory.
  DIR_COMMON_START_MENU,  // Usually "C:\Documents and Settings\All Users\
                          // Start Menu\Programs"
  DIR_START_MENU,         // Usually "C:\Documents and Settings\<user>\
                          // Start Menu\Programs"
  DIR_APP_DATA,  // Application Data directory under the user profile.
  DIR_LOCAL_APP_DATA_LOW,  // Local AppData directory for low integrity level.
  DIR_LOCAL_APP_DATA,  // "Local Settings\Application Data" directory under the
                       // user profile.
  DIR_SOURCE_ROOT,  // Returns the root of the source tree.  This key is useful
                    // for tests that need to locate various resources.  It
                    // should not be used outside of test code.
  PATH_WIN_END
};

}  // namespace base

#endif  // BASE_BASE_PATHS_WIN_H__
