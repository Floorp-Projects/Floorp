/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef LayoutLogging_h
#define LayoutLogging_h

#include "mozilla/Logging.h"

/**
 * Retrieves the log module to use for layout logging.
 */
static mozilla::LazyLogModule sLayoutLog("layout");

/**
 * Use the layout log to warn if a given condition is false.
 *
 * This is only enabled in debug builds and the logging is only displayed if
 * the environmental variable NSPR_LOG_MODULES includes "layout:2" (or higher).
 */
#ifdef DEBUG
#define LAYOUT_WARN_IF_FALSE(_cond, _msg)                                  \
  PR_BEGIN_MACRO                                                           \
    if (MOZ_LOG_TEST(sLayoutLog, mozilla::LogLevel::Warning) &&        \
        !(_cond)) {                                                        \
      mozilla::detail::LayoutLogWarning(_msg, #_cond, __FILE__, __LINE__); \
    }                                                                      \
  PR_END_MACRO
#else
#define LAYOUT_WARN_IF_FALSE(_cond, _msg) \
  PR_BEGIN_MACRO                          \
  PR_END_MACRO
#endif

/**
 * Use the layout log to emit a warning with the same format as NS_WARNING.
 *
 * This is only enabled in debug builds and the logging is only displayed if
 * the environmental variable NSPR_LOG_MODULES includes "layout:2" (or higher).
 */
#ifdef DEBUG
#define LAYOUT_WARNING(_msg)                                                \
  PR_BEGIN_MACRO                                                            \
    if (MOZ_LOG_TEST(sLayoutLog, mozilla::LogLevel::Warning)) {         \
      mozilla::detail::LayoutLogWarning(_msg, nullptr, __FILE__, __LINE__); \
    }                                                                       \
  PR_END_MACRO
#else
#define LAYOUT_WARNING(_msg) \
  PR_BEGIN_MACRO             \
  PR_END_MACRO
#endif

namespace mozilla {
namespace detail {

void LayoutLogWarning(const char* aStr, const char* aExpr,
                      const char* aFile, int32_t aLine);

} // namespace detail
} // namespace mozilla

#endif // LayoutLogging_h
