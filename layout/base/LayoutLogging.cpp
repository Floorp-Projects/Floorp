/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Chromium headers must come before Mozilla headers.
#include "base/process_util.h"

#include "LayoutLogging.h"

namespace mozilla {
namespace detail {

void LayoutLogWarning(const char* aStr, const char* aExpr,
                      const char* aFile, int32_t aLine)
{
  if (aExpr) {
    MOZ_LOG(sLayoutLog,
            mozilla::LogLevel::Warning,
            ("[%d] WARNING: %s: '%s', file %s, line %d",
             base::GetCurrentProcId(),
             aStr, aExpr, aFile, aLine));
  } else {
    MOZ_LOG(sLayoutLog,
            mozilla::LogLevel::Warning,
            ("[%d] WARNING: %s: file %s, line %d",
             base::GetCurrentProcId(),
             aStr, aFile, aLine));
  }
}

} // namespace detail
} // namespace mozilla
