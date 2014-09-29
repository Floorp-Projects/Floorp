/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Macros used to log restyle events.
 */

#ifndef mozilla_RestyleLogging_h
#define mozilla_RestyleLogging_h

#include "mozilla/AutoRestore.h"

#ifdef DEBUG
#define RESTYLE_LOGGING
#endif

#ifdef RESTYLE_LOGGING
#define LOG_RESTYLE_VAR2(prefix_, suffix_) prefix_##suffix_
#define LOG_RESTYLE_VAR(prefix_, suffix_) LOG_RESTYLE_VAR2(prefix_, suffix_)
#define LOG_RESTYLE_DEPTH LOG_RESTYLE_VAR(restyle_depth_, __LINE__)
#define LOG_RESTYLE_IF(object_, cond_, message_, ...)                         \
  PR_BEGIN_MACRO                                                              \
    if (object_->ShouldLogRestyle() && (cond_)) {                             \
      nsCString line;                                                         \
      for (int32_t LOG_RESTYLE_VAR(rs_depth_, __LINE__) = 0;                  \
           LOG_RESTYLE_VAR(rs_depth_, __LINE__) < object_->LoggingDepth();    \
           LOG_RESTYLE_VAR(rs_depth_, __LINE__)++) {                          \
        line.AppendLiteral("  ");                                             \
      }                                                                       \
      line.AppendPrintf(message_, ##__VA_ARGS__);                             \
      printf_stderr("%s\n", line.get());                                      \
    }                                                                         \
  PR_END_MACRO
#define LOG_RESTYLE(message_, ...)                                            \
  LOG_RESTYLE_IF(this, true, message_, ##__VA_ARGS__)
// Beware that LOG_RESTYLE_INDENT is two statements not wrapped in a block.
#define LOG_RESTYLE_INDENT()                                                  \
  AutoRestore<int32_t> LOG_RESTYLE_VAR(ar_depth_, __LINE__)(LoggingDepth());  \
  ++LoggingDepth();
#else
#define LOG_RESTYLE_IF(cond_, message_, ...) /* nothing */
#define LOG_RESTYLE(message_, ...) /* nothing */
#define LOG_RESTYLE_INDENT() /* nothing */
#endif

#endif /* mozilla_RestyleLogging_h */
