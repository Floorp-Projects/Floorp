/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AccessibleCaretLog_h
#define AccessibleCaretLog_h

#include "mozilla/Logging.h"

namespace mozilla {

static LazyLogModule sAccessibleCaretLog("AccessibleCaret");

#ifndef AC_LOG_BASE
#define AC_LOG_BASE(...) MOZ_LOG(sAccessibleCaretLog, mozilla::LogLevel::Debug, (__VA_ARGS__));
#endif

#ifndef AC_LOGV_BASE
#define AC_LOGV_BASE(...)                                                      \
  MOZ_LOG(sAccessibleCaretLog, LogLevel::Verbose, (__VA_ARGS__));
#endif

} // namespace mozilla

#endif // AccessibleCaretLog_h
