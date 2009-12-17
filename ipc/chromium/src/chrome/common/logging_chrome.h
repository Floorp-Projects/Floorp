// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_LOGGING_CHROME_H__
#define CHROME_COMMON_LOGGING_CHROME_H__

#include <string>
#include <vector>

#include "base/logging.h"

class CommandLine;

namespace logging {

// Call to initialize logging for Chrome. This sets up the chrome-specific
// logfile naming scheme and might do other things like log modules and
// setting levels in the future.
//
// The main process might want to delete any old log files on startup by
// setting delete_old_log_file, but the renderer processes should not, or
// they will delete each others' logs.
//
// XXX
// Setting suppress_error_dialogs to true disables any dialogs that would
// normally appear for assertions and crashes, and makes any catchable
// errors (namely assertions) available via GetSilencedErrorCount()
// and GetSilencedError().
void InitChromeLogging(const CommandLine& command_line,
                       OldFileDeletionState delete_old_log_file);

// Call when done using logging for Chrome.
void CleanupChromeLogging();

// Returns the fully-qualified name of the log file.
std::wstring GetLogFileName();

// Returns true when error/assertion dialogs are to be shown,
// false otherwise.
bool DialogsAreSuppressed();

typedef std::vector<std::wstring> AssertionList;

// Gets the list of fatal assertions in the current log file, and
// returns the number of fatal assertions.  (If you don't care
// about the actual list of assertions, you can pass in NULL.)
// NOTE: Since this reads the log file to determine the assertions,
// this operation is O(n) over the length of the log.
// NOTE: This can fail if the file is locked for writing.  However,
// this is unlikely as this function is most useful after
// the program writing the log has terminated.
size_t GetFatalAssertions(AssertionList* assertions);

} // namespace logging

#endif
