// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines all the "base" command-line switches.

#ifndef BASE_BASE_SWITCHES_H_
#define BASE_BASE_SWITCHES_H_

#if defined(COMPILER_MSVC)
#include <string.h>
#endif

namespace switches {

extern const wchar_t kDebugOnStart[];
extern const wchar_t kWaitForDebugger[];
extern const wchar_t kDisableBreakpad[];
extern const wchar_t kFullMemoryCrashReport[];
extern const wchar_t kNoErrorDialogs[];
extern const wchar_t kProcessType[];
extern const wchar_t kEnableDCHECK[];
extern const wchar_t kForceHTTPS[];

}  // namespace switches

#endif  // BASE_BASE_SWITCHES_H_
