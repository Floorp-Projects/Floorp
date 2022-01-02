/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <type_traits>

#include "sdp/SdpLog.h"
#include "common/browser_logging/CSFLog.h"

namespace mozilla {
LazyLogModule SdpLog("sdp");
}  // namespace mozilla

// For compile time enum comparison
template <typename E, typename F>
constexpr bool compareEnum(E e, F f) {
  return static_cast<typename std::underlying_type<E>::type>(e) ==
         static_cast<typename std::underlying_type<F>::type>(f);
}

CSFLogLevel SDPToCSFLogLevel(const SDPLogLevel priority) {
  static_assert(compareEnum(SDP_LOG_ERROR, CSF_LOG_ERROR));
  static_assert(compareEnum(SDP_LOG_WARNING, CSF_LOG_WARNING));
  static_assert(compareEnum(SDP_LOG_INFO, CSF_LOG_INFO));
  static_assert(compareEnum(SDP_LOG_DEBUG, CSF_LOG_DEBUG));
  static_assert(compareEnum(SDP_LOG_VERBOSE, CSF_LOG_VERBOSE));

  // Check that all SDP_LOG_* cases are covered. It compiles to nothing.
  switch (priority) {
    case SDP_LOG_ERROR:
    case SDP_LOG_WARNING:
    case SDP_LOG_INFO:
    case SDP_LOG_DEBUG:
    case SDP_LOG_VERBOSE:
      break;
  }

  // Ditto for CSF_LOG_*
  switch (static_cast<CSFLogLevel>(priority)) {
    case CSF_LOG_ERROR:
    case CSF_LOG_WARNING:
    case CSF_LOG_INFO:
    case CSF_LOG_DEBUG:
    case CSF_LOG_VERBOSE:
      break;
  }

  return static_cast<CSFLogLevel>(priority);
}

void SDPLog(SDPLogLevel priority, const char* sourceFile, int sourceLine,
            const char* tag, const char* format, ...) {
  va_list ap;
  va_start(ap, format);
  CSFLogV(SDPToCSFLogLevel(priority), sourceFile, sourceLine, tag, format, ap);
  va_end(ap);
}

void SDPLogV(SDPLogLevel priority, const char* sourceFile, int sourceLine,
             const char* tag, const char* format, va_list args) {
  CSFLogV(SDPToCSFLogLevel(priority), sourceFile, sourceLine, tag, format,
          args);
}

int SDPLogTestLevel(SDPLogLevel priority) {
  return CSFLogTestLevel(SDPToCSFLogLevel(priority));
}
