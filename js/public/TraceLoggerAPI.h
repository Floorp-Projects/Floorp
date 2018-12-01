/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* SpiderMonkey TraceLogger APIs. */

#ifndef js_TraceLoggerAPI_h
#define js_TraceLoggerAPI_h

#include "jstypes.h"

namespace JS {
#ifdef JS_TRACE_LOGGING

// Begin trace logging events.  This will activate some of the
// textId's for various events and set the global option
// JSJITCOMPILER_ENABLE_TRACELOGGER to true.
// This does nothing except return if the trace logger is already active.
extern JS_PUBLIC_API void StartTraceLogger(JSContext *cx,
                                           mozilla::TimeStamp profilerStart);

// Stop trace logging events.  All textId's will be set to false, and the
// global JSJITCOMPILER_ENABLE_TRACELOGGER will be set to false.
// This does nothing except return if the trace logger is not active.
extern JS_PUBLIC_API void StopTraceLogger(JSContext *cx);

// Clear and free any event data that was recorded by the trace logger.
extern JS_PUBLIC_API void ResetTraceLogger(void);

#else
// Define empty inline functions for when trace logging compilation is not
// enabled.  TraceLogging.cpp will not be built in that case so we need to
// provide something for any routines that reference these.
inline void StartTraceLogger(JSContext *cx, mozilla::TimeStamp profilerStart) {}
inline void StopTraceLogger(JSContext *cx) {}
inline void ResetTraceLogger(void) {}
#endif
};  // namespace JS

#endif /* js_TraceLoggerAPI_h */
