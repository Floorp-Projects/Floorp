// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/debug_flags.h"

#include "base/base_switches.h"
#include "base/command_line.h"
#include "chrome/common/chrome_switches.h"

bool DebugFlags::ProcessDebugFlags(CommandLine* command_line,
                                   ChildProcessInfo::ProcessType type,
                                   bool is_in_sandbox) {
  bool should_help_child = false;
  const CommandLine& current_cmd_line = *CommandLine::ForCurrentProcess();
  if (current_cmd_line.HasSwitch(switches::kDebugChildren)) {
    // Look to pass-on the kDebugOnStart flag.
    std::wstring value;
    value = current_cmd_line.GetSwitchValue(switches::kDebugChildren);
    if (value.empty() ||
        (type == ChildProcessInfo::RENDER_PROCESS &&
         value == switches::kRendererProcess) ||
        (type == ChildProcessInfo::PLUGIN_PROCESS &&
         value == switches::kPluginProcess)) {
      command_line->AppendSwitch(switches::kDebugOnStart);
      should_help_child = true;
    }
    command_line->AppendSwitchWithValue(switches::kDebugChildren, value);
  } else if (current_cmd_line.HasSwitch(switches::kWaitForDebuggerChildren)) {
    // Look to pass-on the kWaitForDebugger flag.
    std::wstring value;
    value = current_cmd_line.GetSwitchValue(switches::kWaitForDebuggerChildren);
    if (value.empty() ||
        (type == ChildProcessInfo::RENDER_PROCESS &&
         value == switches::kRendererProcess) ||
        (type == ChildProcessInfo::PLUGIN_PROCESS &&
         value == switches::kPluginProcess)) {
      command_line->AppendSwitch(switches::kWaitForDebugger);
    }
    command_line->AppendSwitchWithValue(switches::kWaitForDebuggerChildren,
                                        value);
  }
  return should_help_child;
}
