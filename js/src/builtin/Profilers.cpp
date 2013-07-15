/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Profiling-related API */

#include "builtin/Profilers.h"

#include <stdarg.h>

#ifdef MOZ_CALLGRIND
#include <valgrind/callgrind.h>
#endif

#ifdef __APPLE__
#include "devtools/sharkctl.h"
#include "devtools/Instruments.h"
#endif

#include "jscntxtinlines.h"

using namespace js;

using mozilla::ArrayLength;

/* Thread-unsafe error management */

static char gLastError[2000];

static void
#ifdef __GNUC__
__attribute__((unused,format(printf,1,2)))
#endif
UnsafeError(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    (void) vsnprintf(gLastError, sizeof(gLastError), format, args);
    va_end(args);

    gLastError[sizeof(gLastError) - 1] = '\0';
}

JS_PUBLIC_API(const char *)
JS_UnsafeGetLastProfilingError()
{
    return gLastError;
}

#ifdef __APPLE__
static bool
StartOSXProfiling(const char *profileName = NULL)
{
    bool ok = true;
    const char* profiler = NULL;
#ifdef MOZ_SHARK
    ok = Shark::Start();
    profiler = "Shark";
#endif
#ifdef MOZ_INSTRUMENTS
    ok = Instruments::Start();
    profiler = "Instruments";
#endif
    if (!ok) {
        if (profileName)
            UnsafeError("Failed to start %s for %s", profiler, profileName);
        else
            UnsafeError("Failed to start %s", profiler);
        return false;
    }
    return true;
}
#endif

JS_PUBLIC_API(JSBool)
JS_StartProfiling(const char *profileName)
{
    JSBool ok = JS_TRUE;
#ifdef __APPLE__
    ok = StartOSXProfiling(profileName);
#endif
#ifdef __linux__
    if (!js_StartPerf())
        ok = JS_FALSE;
#endif
    return ok;
}

JS_PUBLIC_API(JSBool)
JS_StopProfiling(const char *profileName)
{
    JSBool ok = JS_TRUE;
#ifdef __APPLE__
#ifdef MOZ_SHARK
    Shark::Stop();
#endif
#ifdef MOZ_INSTRUMENTS
    Instruments::Stop(profileName);
#endif
#endif
#ifdef __linux__
    if (!js_StopPerf())
        ok = JS_FALSE;
#endif
    return ok;
}

/*
 * Start or stop whatever platform- and configuration-specific profiling
 * backends are available.
 */
static JSBool
ControlProfilers(bool toState)
{
    JSBool ok = JS_TRUE;

    if (! Probes::ProfilingActive && toState) {
#ifdef __APPLE__
#if defined(MOZ_SHARK) || defined(MOZ_INSTRUMENTS)
        const char* profiler;
#ifdef MOZ_SHARK
        ok = Shark::Start();
        profiler = "Shark";
#endif
#ifdef MOZ_INSTRUMENTS
        ok = Instruments::Resume();
        profiler = "Instruments";
#endif
        if (!ok) {
            UnsafeError("Failed to start %s", profiler);
        }
#endif
#endif
#ifdef MOZ_CALLGRIND
        if (! js_StartCallgrind()) {
            UnsafeError("Failed to start Callgrind");
            ok = JS_FALSE;
        }
#endif
    } else if (Probes::ProfilingActive && ! toState) {
#ifdef __APPLE__
#ifdef MOZ_SHARK
        Shark::Stop();
#endif
#ifdef MOZ_INSTRUMENTS
        Instruments::Pause();
#endif
#endif
#ifdef MOZ_CALLGRIND
        if (! js_StopCallgrind()) {
            UnsafeError("failed to stop Callgrind");
            ok = JS_FALSE;
        }
#endif
    }

    Probes::ProfilingActive = toState;

    return ok;
}

/*
 * Pause/resume whatever profiling mechanism is currently compiled
 * in, if applicable. This will not affect things like dtrace.
 *
 * Do not mix calls to these APIs with calls to the individual
 * profilers' pause/resume functions, because only overall state is
 * tracked, not the state of each profiler.
 */
JS_PUBLIC_API(JSBool)
JS_PauseProfilers(const char *profileName)
{
    return ControlProfilers(false);
}

JS_PUBLIC_API(JSBool)
JS_ResumeProfilers(const char *profileName)
{
    return ControlProfilers(true);
}

JS_PUBLIC_API(JSBool)
JS_DumpProfile(const char *outfile, const char *profileName)
{
    JSBool ok = JS_TRUE;
#ifdef MOZ_CALLGRIND
    js_DumpCallgrind(outfile);
#endif
    return ok;
}

#ifdef MOZ_PROFILING

struct RequiredStringArg {
    JSContext *mCx;
    char *mBytes;
    RequiredStringArg(JSContext *cx, const CallArgs &args, size_t argi, const char *caller)
        : mCx(cx), mBytes(NULL)
    {
        if (args.length() <= argi) {
            JS_ReportError(cx, "%s: not enough arguments", caller);
        } else if (!args[argi].isString()) {
            JS_ReportError(cx, "%s: invalid arguments (string expected)", caller);
        } else {
            mBytes = JS_EncodeString(cx, args[argi].toString());
        }
    }
    operator void*() {
        return (void*) mBytes;
    }
    ~RequiredStringArg() {
        if (mBytes)
            js_free(mBytes);
    }
};

static JSBool
StartProfiling(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    if (args.length() == 0) {
        args.rval().setBoolean(JS_StartProfiling(NULL));
        return JS_TRUE;
    }

    RequiredStringArg profileName(cx, args, 0, "startProfiling");
    if (!profileName)
        return JS_FALSE;
    args.rval().setBoolean(JS_StartProfiling(profileName.mBytes));
    return JS_TRUE;
}

static JSBool
StopProfiling(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    if (args.length() == 0) {
        args.rval().setBoolean(JS_StopProfiling(NULL));
        return JS_TRUE;
    }

    RequiredStringArg profileName(cx, args, 0, "stopProfiling");
    if (!profileName)
        return JS_FALSE;
    args.rval().setBoolean(JS_StopProfiling(profileName.mBytes));
    return JS_TRUE;
}

static JSBool
PauseProfilers(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    if (args.length() == 0) {
        args.rval().setBoolean(JS_PauseProfilers(NULL));
        return JS_TRUE;
    }

    RequiredStringArg profileName(cx, args, 0, "pauseProfiling");
    if (!profileName)
        return JS_FALSE;
    args.rval().setBoolean(JS_PauseProfilers(profileName.mBytes));
    return JS_TRUE;
}

static JSBool
ResumeProfilers(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    if (args.length() == 0) {
        args.rval().setBoolean(JS_ResumeProfilers(NULL));
        return JS_TRUE;
    }

    RequiredStringArg profileName(cx, args, 0, "resumeProfiling");
    if (!profileName)
        return JS_FALSE;
    args.rval().setBoolean(JS_ResumeProfilers(profileName.mBytes));
    return JS_TRUE;
}

/* Usage: DumpProfile([filename[, profileName]]) */
static JSBool
DumpProfile(JSContext *cx, unsigned argc, jsval *vp)
{
    bool ret;
    CallArgs args = CallArgsFromVp(argc, vp);
    if (args.length() == 0) {
        ret = JS_DumpProfile(NULL, NULL);
    } else {
        RequiredStringArg filename(cx, args, 0, "dumpProfile");
        if (!filename)
            return JS_FALSE;

        if (args.length() == 1) {
            ret = JS_DumpProfile(filename.mBytes, NULL);
        } else {
            RequiredStringArg profileName(cx, args, 1, "dumpProfile");
            if (!profileName)
                return JS_FALSE;

            ret = JS_DumpProfile(filename.mBytes, profileName.mBytes);
        }
    }

    args.rval().setBoolean(ret);
    return true;
}

#if defined(MOZ_SHARK) || defined(MOZ_INSTRUMENTS)

static JSBool
IgnoreAndReturnTrue(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    args.rval().setBoolean(true);
    return true;
}

#endif

#ifdef MOZ_CALLGRIND
static JSBool
StartCallgrind(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    args.rval().setBoolean(js_StartCallgrind());
    return JS_TRUE;
}

static JSBool
StopCallgrind(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    args.rval().setBoolean(js_StopCallgrind());
    return JS_TRUE;
}

static JSBool
DumpCallgrind(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    if (args.length() == 0) {
        args.rval().setBoolean(js_DumpCallgrind(NULL));
        return JS_TRUE;
    }

    RequiredStringArg outFile(cx, args, 0, "dumpCallgrind");
    if (!outFile)
        return JS_FALSE;

    args.rval().setBoolean(js_DumpCallgrind(outFile.mBytes));
    return JS_TRUE;
}
#endif

static const JSFunctionSpec profiling_functions[] = {
    JS_FN("startProfiling",  StartProfiling,      1,0),
    JS_FN("stopProfiling",   StopProfiling,       1,0),
    JS_FN("pauseProfilers",  PauseProfilers,      1,0),
    JS_FN("resumeProfilers", ResumeProfilers,     1,0),
    JS_FN("dumpProfile",     DumpProfile,         2,0),
#if defined(MOZ_SHARK) || defined(MOZ_INSTRUMENTS)
    /* Keep users of the old shark API happy. */
    JS_FN("connectShark",    IgnoreAndReturnTrue, 0,0),
    JS_FN("disconnectShark", IgnoreAndReturnTrue, 0,0),
    JS_FN("startShark",      StartProfiling,      0,0),
    JS_FN("stopShark",       StopProfiling,       0,0),
#endif
#ifdef MOZ_CALLGRIND
    JS_FN("startCallgrind", StartCallgrind,       0,0),
    JS_FN("stopCallgrind",  StopCallgrind,        0,0),
    JS_FN("dumpCallgrind",  DumpCallgrind,        1,0),
#endif
    JS_FS_END
};

#endif

JS_PUBLIC_API(JSBool)
JS_DefineProfilingFunctions(JSContext *cx, JSObject *objArg)
{
    RootedObject obj(cx, objArg);

    assertSameCompartment(cx, obj);
#ifdef MOZ_PROFILING
    return JS_DefineFunctions(cx, obj, profiling_functions);
#else
    return true;
#endif
}

#ifdef MOZ_CALLGRIND

JS_FRIEND_API(JSBool)
js_StartCallgrind()
{
    JS_SILENCE_UNUSED_VALUE_IN_EXPR(CALLGRIND_START_INSTRUMENTATION);
    JS_SILENCE_UNUSED_VALUE_IN_EXPR(CALLGRIND_ZERO_STATS);
    return true;
}

JS_FRIEND_API(JSBool)
js_StopCallgrind()
{
    JS_SILENCE_UNUSED_VALUE_IN_EXPR(CALLGRIND_STOP_INSTRUMENTATION);
    return true;
}

JS_FRIEND_API(JSBool)
js_DumpCallgrind(const char *outfile)
{
    if (outfile) {
        JS_SILENCE_UNUSED_VALUE_IN_EXPR(CALLGRIND_DUMP_STATS_AT(outfile));
    } else {
        JS_SILENCE_UNUSED_VALUE_IN_EXPR(CALLGRIND_DUMP_STATS);
    }

    return true;
}

#endif /* MOZ_CALLGRIND */

#ifdef __linux__

/*
 * Code for starting and stopping |perf|, the Linux profiler.
 *
 * Output from profiling is written to mozperf.data in your cwd.
 *
 * To enable, set MOZ_PROFILE_WITH_PERF=1 in your environment.
 *
 * To pass additional parameters to |perf record|, provide them in the
 * MOZ_PROFILE_PERF_FLAGS environment variable.  If this variable does not
 * exist, we default it to "--call-graph".  (If you don't want --call-graph but
 * don't want to pass any other args, define MOZ_PROFILE_PERF_FLAGS to the empty
 * string.)
 *
 * If you include --pid or --output in MOZ_PROFILE_PERF_FLAGS, you're just
 * asking for trouble.
 *
 * Our split-on-spaces logic is lame, so don't expect MOZ_PROFILE_PERF_FLAGS to
 * work if you pass an argument which includes a space (e.g.
 * MOZ_PROFILE_PERF_FLAGS="-e 'foo bar'").
 */

#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>

static bool perfInitialized = false;
static pid_t perfPid = 0;

JSBool js_StartPerf()
{
    const char *outfile = "mozperf.data";

    if (perfPid != 0) {
        UnsafeError("js_StartPerf: called while perf was already running!\n");
        return false;
    }

    // Bail if MOZ_PROFILE_WITH_PERF is empty or undefined.
    if (!getenv("MOZ_PROFILE_WITH_PERF") ||
        !strlen(getenv("MOZ_PROFILE_WITH_PERF"))) {
        return true;
    }

    /*
     * Delete mozperf.data the first time through -- we're going to append to it
     * later on, so we want it to be clean when we start out.
     */
    if (!perfInitialized) {
        perfInitialized = true;
        unlink(outfile);
        char cwd[4096];
        printf("Writing perf profiling data to %s/%s\n",
               getcwd(cwd, sizeof(cwd)), outfile);
    }

    pid_t mainPid = getpid();

    pid_t childPid = fork();
    if (childPid == 0) {
        /* perf record --append --pid $mainPID --output=$outfile $MOZ_PROFILE_PERF_FLAGS */

        char mainPidStr[16];
        snprintf(mainPidStr, sizeof(mainPidStr), "%d", mainPid);
        const char *defaultArgs[] = {"perf", "record", "--append",
                                     "--pid", mainPidStr, "--output", outfile};

        Vector<const char*, 0, SystemAllocPolicy> args;
        args.append(defaultArgs, ArrayLength(defaultArgs));

        const char *flags = getenv("MOZ_PROFILE_PERF_FLAGS");
        if (!flags) {
            flags = "--call-graph";
        }

        // Split |flags| on spaces.  (Don't bother to free it -- we're going to
        // exec anyway.)
        char *toksave;
        char *tok = strtok_r(strdup(flags), " ", &toksave);
        while (tok) {
            args.append(tok);
            tok = strtok_r(NULL, " ", &toksave);
        }

        args.append((char*) NULL);

        execvp("perf", const_cast<char**>(args.begin()));

        /* Reached only if execlp fails. */
        fprintf(stderr, "Unable to start perf.\n");
        exit(1);
    }
    else if (childPid > 0) {
        perfPid = childPid;

        /* Give perf a chance to warm up. */
        usleep(500 * 1000);
        return true;
    }
    else {
        UnsafeError("js_StartPerf: fork() failed\n");
        return false;
    }
}

JSBool js_StopPerf()
{
    if (perfPid == 0) {
        UnsafeError("js_StopPerf: perf is not running.\n");
        return true;
    }

    if (kill(perfPid, SIGINT)) {
        UnsafeError("js_StopPerf: kill failed\n");

        // Try to reap the process anyway.
        waitpid(perfPid, NULL, WNOHANG);
    }
    else {
        waitpid(perfPid, NULL, 0);
    }

    perfPid = 0;
    return true;
}

#endif /* __linux__ */
