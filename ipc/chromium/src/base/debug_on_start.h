// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Define the necessary code and global data to look for kDebugOnStart command
// line argument. When the command line argument is detected, it invokes the
// debugger, if no system-wide debugger is registered, a debug break is done.

#ifndef BASE_DEBUG_ON_START_H_
#define BASE_DEBUG_ON_START_H_

#include "base/basictypes.h"

// This only works on Windows.
#if defined(OS_WIN)

#ifndef DECLSPEC_SELECTANY
#define DECLSPEC_SELECTANY  __declspec(selectany)
#endif

// Debug on start functions and data.
class DebugOnStart {
 public:
  // Expected function type in the .CRT$XI* section.
  // Note: See VC\crt\src\internal.h for reference.
  typedef int  (__cdecl *PIFV)(void);

  // Looks at the command line for kDebugOnStart argument. If found, it invokes
  // the debugger, if this fails, it crashes.
  static int __cdecl Init();

  // Returns true if the 'argument' is present in the 'command_line'. It does
  // not use the CRT, only Kernel32 functions.
  static bool FindArgument(wchar_t* command_line, const wchar_t* argument);
};

// Set the function pointer to our function to look for a crash on start. The
// XIB section is started pretty early in the program initialization so in
// theory it should be called before any user created global variable
// initialization code and CRT initialization code.
// Note: See VC\crt\src\defsects.inc and VC\crt\src\crt0.c for reference.
#ifdef _WIN64

// "Fix" the segment. On x64, the .CRT segment is merged into the .rdata segment
// so it contains const data only.
#pragma const_seg(push, ".CRT$XIB")
// Declare the pointer so the CRT will find it.
extern const DebugOnStart::PIFV debug_on_start;
DECLSPEC_SELECTANY const DebugOnStart::PIFV debug_on_start =
    &DebugOnStart::Init;
// Fix back the segment.
#pragma const_seg(pop)

#else  // _WIN64

// "Fix" the segment. On x86, the .CRT segment is merged into the .data segment
// so it contains non-const data only.
#pragma data_seg(push, ".CRT$XIB")
// Declare the pointer so the CRT will find it.
DECLSPEC_SELECTANY DebugOnStart::PIFV debug_on_start = &DebugOnStart::Init;
// Fix back the segment.
#pragma data_seg(pop)

#endif  // _WIN64
#endif  // defined(OS_WIN)

#endif  // BASE_DEBUG_ON_START_H_
