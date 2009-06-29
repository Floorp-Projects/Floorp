// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_BASE_PATHS_LINUX_H_
#define BASE_BASE_PATHS_LINUX_H_

// This file declares Linux-specific path keys for the base module.
// These can be used with the PathService to access various special
// directories and files.

namespace base {

enum {
  PATH_LINUX_START = 200,

  FILE_EXE,     // Path and filename of the current executable.
  FILE_MODULE,  // Path and filename of the module containing the code for the
                // PathService (which could differ from FILE_EXE if the
                // PathService were compiled into a shared object, for example).
  DIR_SOURCE_ROOT,  // Returns the root of the source tree.  This key is useful
                    // for tests that need to locate various resources.  It
                    // should not be used outside of test code.

  PATH_LINUX_END
};

}  // namespace base

#endif  // BASE_BASE_PATHS_LINUX_H_
