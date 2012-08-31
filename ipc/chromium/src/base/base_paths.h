// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_BASE_PATHS_H_
#define BASE_BASE_PATHS_H_

// This file declares path keys for the base module.  These can be used with
// the PathService to access various special directories and files.

#include "base/basictypes.h"
#if defined(OS_WIN)
#include "base/base_paths_win.h"
#elif defined(OS_MACOSX)
#include "base/base_paths_mac.h"
#elif defined(OS_LINUX) || defined(OS_BSD)
#include "base/base_paths_linux.h"
#endif
#include "base/path_service.h"

namespace base {

enum {
  PATH_START = 0,

  DIR_CURRENT,  // current directory
  DIR_EXE,      // directory containing FILE_EXE
  DIR_MODULE,   // directory containing FILE_MODULE
  DIR_TEMP,     // temporary directory
  PATH_END
};

}  // namespace base

#endif  // BASE_BASE_PATHS_H_
