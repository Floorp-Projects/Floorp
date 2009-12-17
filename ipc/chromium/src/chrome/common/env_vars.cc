// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/env_vars.h"

namespace env_vars {

// We call running in unattended mode (for automated testing) "headless".
// This mode can be enabled using this variable or by the kNoErrorDialogs
// switch.
const wchar_t kHeadless[]        = L"CHROME_HEADLESS";

// The name of the log file.
const wchar_t kLogFileName[]     = L"CHROME_LOG_FILE";

// CHROME_CRASHED exists if a previous instance of chrome has crashed. This
// triggers the 'restart chrome' dialog. CHROME_RESTART contains the strings
// that are needed to show the dialog.
const wchar_t kShowRestart[] = L"CHROME_CRASHED";
const wchar_t kRestartInfo[] = L"CHROME_RESTART";

// The strings RIGHT_TO_LEFT and LEFT_TO_RIGHT indicate the locale direction.
// For example, for Hebrew and Arabic locales, we use RIGHT_TO_LEFT so that the
// dialog is displayed using the right orientation.
const wchar_t kRtlLocale[] = L"RIGHT_TO_LEFT";
const wchar_t kLtrLocale[] = L"LEFT_TO_RIGHT";

// If the out-of-process breakpad could not be installed, we set this variable
// according to the process.
const wchar_t kNoOOBreakpad[] = L"NO_OO_BREAKPAD";

}  // namespace env_vars
