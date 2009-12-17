// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include "base/debug_on_start.h"

#include "base/base_switches.h"
#include "base/basictypes.h"
#include "base/debug_util.h"

// Minimalist implementation to try to find a command line argument. We can use
// kernel32 exported functions but not the CRT functions because we're too early
// in the process startup.
// The code is not that bright and will find things like ---argument or
// /-/argument.
// Note: command_line is non-destructively modified.
bool DebugOnStart::FindArgument(wchar_t* command_line, const wchar_t* argument)
{
  int argument_len = lstrlen(argument);
  int command_line_len = lstrlen(command_line);
  while (command_line_len > argument_len) {
    wchar_t first_char = command_line[0];
    wchar_t last_char = command_line[argument_len+1];
    // Try to find an argument.
    if ((first_char == L'-' || first_char == L'/') &&
        (last_char == L' ' || last_char == 0 || last_char == L'=')) {
      command_line[argument_len+1] = 0;
      // Skip the - or /
      if (lstrcmpi(command_line+1, argument) == 0) {
        // Found it.
        command_line[argument_len+1] = last_char;
        return true;
      }
      // Fix back.
      command_line[argument_len+1] = last_char;
    }
    // Continue searching.
    ++command_line;
    --command_line_len;
  }
  return false;
}

// static
int __cdecl DebugOnStart::Init() {
  // Try to find the argument.
  if (FindArgument(GetCommandLine(), switches::kDebugOnStart)) {
    // We can do 2 things here:
    // - Ask for a debugger to attach to us. This involve reading the registry
    //   key and creating the process.
    // - Do a int3.

    // It will fails if we run in a sandbox. That is expected.
    DebugUtil::SpawnDebuggerOnProcess(GetCurrentProcessId());

    // Wait for a debugger to come take us.
    DebugUtil::WaitForDebugger(60, false);
  } else if (FindArgument(GetCommandLine(), switches::kWaitForDebugger)) {
    // Wait for a debugger to come take us.
    DebugUtil::WaitForDebugger(60, true);
  }
  return 0;
}
