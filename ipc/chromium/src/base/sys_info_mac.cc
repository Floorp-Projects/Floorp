// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/sys_info.h"

#include <CoreServices/CoreServices.h>

namespace base {

// static
void SysInfo::OperatingSystemVersionNumbers(int32_t *major_version,
                                            int32_t *minor_version,
                                            int32_t *bugfix_version) {
  static bool is_initialized = false;
  static int32_t major_version_cached = 0;
  static int32_t minor_version_cached = 0;
  static int32_t bugfix_version_cached = 0;

  if (!is_initialized) {
    // Gestalt can't be called in the sandbox, so we cache its return value.
    Gestalt(gestaltSystemVersionMajor,
        reinterpret_cast<SInt32*>(&major_version_cached));
    Gestalt(gestaltSystemVersionMinor,
        reinterpret_cast<SInt32*>(&minor_version_cached));
    Gestalt(gestaltSystemVersionBugFix,
        reinterpret_cast<SInt32*>(&bugfix_version_cached));
    is_initialized = true;
  }

  *major_version = major_version_cached;
  *minor_version = minor_version_cached;
  *bugfix_version = bugfix_version_cached;
}

// static
void SysInfo::CacheSysInfo() {
  // Due to startup time concerns [premature optimization?] we only cache values
  // from functions we know to be called in the renderer & fail when the sandbox
  // is enabled.
  NumberOfProcessors();
  int32_t dummy;
  OperatingSystemVersionNumbers(&dummy, &dummy, &dummy);
}

}  // namespace base
