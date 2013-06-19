/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Functions for controlling profilers from within JS: Valgrind, Perf,
 * Shark, etc.
 */
#ifndef Profilers_h___
#define Profilers_h___

#include "jstypes.h"

/**
 * Start any profilers that are available and have been configured on for this
 * platform. This is NOT thread safe.
 *
 * The profileName is used by some profilers to describe the current profiling
 * run. It may be used for part of the filename of the output, but the
 * specifics depend on the profiler. Many profilers will ignore it. Passing in
 * NULL is legal; some profilers may use it to output to stdout or similar.
 *
 * Returns true if no profilers fail to start.
 */
extern JS_PUBLIC_API(JSBool)
JS_StartProfiling(const char *profileName);

/**
 * Stop any profilers that were previously started with JS_StartProfiling.
 * Returns true if no profilers fail to stop.
 */
extern JS_PUBLIC_API(JSBool)
JS_StopProfiling(const char *profileName);

/**
 * Write the current profile data to the given file, if applicable to whatever
 * profiler is being used.
 */
extern JS_PUBLIC_API(JSBool)
JS_DumpProfile(const char *outfile, const char *profileName);

/**
 * Pause currently active profilers (only supported by some profilers). Returns
 * whether any profilers failed to pause. (Profilers that do not support
 * pause/resume do not count.)
 */
extern JS_PUBLIC_API(JSBool)
JS_PauseProfilers(const char *profileName);

/**
 * Resume suspended profilers
 */
extern JS_PUBLIC_API(JSBool)
JS_ResumeProfilers(const char *profileName);

/**
 * The profiling API calls are not able to report errors, so they use a
 * thread-unsafe global memory buffer to hold the last error encountered. This
 * should only be called after something returns false.
 */
JS_PUBLIC_API(const char *)
JS_UnsafeGetLastProfilingError();

#ifdef MOZ_CALLGRIND

extern JS_FRIEND_API(JSBool)
js_StopCallgrind();

extern JS_FRIEND_API(JSBool)
js_StartCallgrind();

extern JS_FRIEND_API(JSBool)
js_DumpCallgrind(const char *outfile);

#endif /* MOZ_CALLGRIND */

#ifdef __linux__

extern JS_FRIEND_API(JSBool)
js_StartPerf();

extern JS_FRIEND_API(JSBool)
js_StopPerf();

#endif /* __linux__ */

#endif /* Profilers_h___ */
