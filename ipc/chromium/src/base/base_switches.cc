// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_switches.h"

namespace switches {

// If the program includes chrome/common/debug_on_start.h, the process will
// start the JIT system-registered debugger on itself and will wait for 60
// seconds for the debugger to attach to itself. Then a break point will be hit.
const wchar_t kDebugOnStart[]                  = L"debug-on-start";

// Will wait for 60 seconds for a debugger to come to attach to the process.
const wchar_t kWaitForDebugger[]               = L"wait-for-debugger";

// Suppresses all error dialogs when present.
const wchar_t kNoErrorDialogs[]                = L"noerrdialogs";

// Disables the crash reporting.
const wchar_t kDisableBreakpad[]               = L"disable-breakpad";

// Generates full memory crash dump.
const wchar_t kFullMemoryCrashReport[]         = L"full-memory-crash-report";

// The value of this switch determines whether the process is started as a
// renderer or plugin host.  If it's empty, it's the browser.
const wchar_t kProcessType[]                   = L"type";

// Enable DCHECKs in release mode.
const wchar_t kEnableDCHECK[]                  = L"enable-dcheck";

// Refuse to make HTTP connections and refuse to accept certificate errors.
// For more information about the design of this feature, please see
//
//   ForceHTTPS: Protecting High-Security Web Sites from Network Attacks
//   Collin Jackson and Adam Barth
//   In Proc. of the 17th International World Wide Web Conference (WWW 2008)
//
// Available at http://www.adambarth.com/papers/2008/jackson-barth.pdf
const wchar_t kForceHTTPS[]                    = L"force-https";

}  // namespace switches
