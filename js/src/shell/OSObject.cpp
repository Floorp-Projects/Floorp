/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// OSObject.h - os object for exposing posix system calls in the JS shell

#include "shell/OSObject.h"

#include <errno.h>
#include <stdlib.h>
#ifdef XP_WIN
#include <process.h>
#include <string.h>
#else
#include <sys/wait.h>
#include <unistd.h>
#endif

// For JSFunctionSpecWithHelp
#include "jsfriendapi.h"

#include "js/Conversions.h"

using namespace JS;

static bool
os_getenv(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    if (args.length() < 1) {
        JS_ReportError(cx, "os.getenv requires 1 argument");
        return false;
    }
    RootedString key(cx, ToString(cx, args[0]));
    if (!key)
        return false;
    JSAutoByteString keyBytes;
    if (!keyBytes.encodeUtf8(cx, key))
        return false;

    if (const char *valueBytes = getenv(keyBytes.ptr())) {
        RootedString value(cx, JS_NewStringCopyZ(cx, valueBytes));
        if (!value)
            return false;
        args.rval().setString(value);
    } else {
        args.rval().setUndefined();
    }
    return true;
}

static bool
os_getpid(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    if (args.length() != 0) {
        JS_ReportError(cx, "os.getpid takes no arguments");
        return false;
    }
    args.rval().setInt32(getpid());
    return true;
}

#if !defined(XP_WIN)

// There are two possible definitions of strerror_r floating around. The GNU
// one returns a char* which may or may not be the buffer you passed in. The
// other one returns an integer status code, and always writes the result into
// the provided buffer.

inline char *
strerror_message(int result, char *buffer)
{
    return result == 0 ? buffer : nullptr;
}

inline char *
strerror_message(char *result, char *buffer)
{
    return result;
}

#endif

static void
ReportSysError(JSContext *cx, const char *prefix)
{
    char buffer[200];

#if defined(XP_WIN)
    strerror_s(buffer, sizeof(buffer), errno);
    const char *errstr = buffer;
#else
    const char *errstr = strerror_message(strerror_r(errno, buffer, sizeof(buffer)), buffer);
#endif

    if (!errstr)
        errstr = "unknown error";

    size_t nbytes = strlen(prefix) + strlen(errstr) + 3;
    char *final = (char*) js_malloc(nbytes);
    if (!final) {
        JS_ReportOutOfMemory(cx);
        return;
    }

#ifdef XP_WIN
    _snprintf(final, nbytes, "%s: %s", prefix, errstr);
#else
    snprintf(final, nbytes, "%s: %s", prefix, errstr);
#endif

    JS_ReportError(cx, final);
    js_free(final);
}

static bool
os_system(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (args.length() == 0) {
        JS_ReportError(cx, "os.system requires 1 argument");
        return false;
    }

    JSString *str = JS::ToString(cx, args[0]);
    if (!str)
        return false;

    JSAutoByteString command(cx, str);
    if (!command)
        return false;

    int result = system(command.ptr());
    if (result == -1) {
        ReportSysError(cx, "system call failed");
        return false;
    }

    args.rval().setInt32(result);
    return true;
}

#ifndef XP_WIN
static bool
os_spawn(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (args.length() == 0) {
        JS_ReportError(cx, "os.spawn requires 1 argument");
        return false;
    }

    JSString *str = JS::ToString(cx, args[0]);
    if (!str)
        return false;

    JSAutoByteString command(cx, str);
    if (!command)
        return false;

    int32_t childPid = fork();
    if (childPid) {
        args.rval().setInt32(childPid);
        return true;
    }

    if (childPid == -1) {
        ReportSysError(cx, "fork failed");
        return false;
    }

    // We are in the child

    const char *cmd[] = {"sh", "-c", nullptr, nullptr};
    cmd[2] = command.ptr();

    execvp("sh", (char * const*)cmd);
    exit(1);
}

static bool
os_kill(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    int32_t pid;
    if (args.length() < 1) {
        JS_ReportError(cx, "os.kill requires 1 argument");
        return false;
    }
    if (!JS::ToInt32(cx, args[0], &pid))
        return false;

    // It is too easy to kill yourself accidentally with os.kill("goose").
    if (pid == 0 && !args[0].isInt32()) {
        JS_ReportError(cx, "os.kill requires numeric pid");
        return false;
    }

    int signal = SIGINT;
    if (args.length() > 1) {
        if (!JS::ToInt32(cx, args[1], &signal))
            return false;
    }

    int status = kill(pid, signal);
    if (status == -1)
        ReportSysError(cx, "kill failed");
    args.rval().setUndefined();
    return true;
}

static bool
os_waitpid(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    int32_t pid;
    if (args.length() == 0) {
        pid = -1;
    } else {
        if (!JS::ToInt32(cx, args[0], &pid))
            return false;
    }

    bool nohang = false;
    if (args.length() >= 2)
        nohang = JS::ToBoolean(args[1]);

    int status;
    pid_t result = waitpid(pid, &status, nohang ? WNOHANG : 0);
    if (result == -1) {
        ReportSysError(cx, "os.waitpid failed");
        return false;
    }

    RootedObject info(cx, JS_NewPlainObject(cx));
    if (!info)
        return false;

    RootedValue v(cx);
    if (result != 0) {
        v.setInt32(result);
        if (!JS_DefineProperty(cx, info, "pid", v, JSPROP_ENUMERATE))
            return false;
    }
    if (WIFEXITED(status)) {
        v.setInt32(WEXITSTATUS(status));
        if (!JS_DefineProperty(cx, info, "exitStatus", v, JSPROP_ENUMERATE))
            return false;
    }

    args.rval().setObject(*info);
    return true;
}
#endif

static const JSFunctionSpecWithHelp os_functions[] = {
    JS_FN_HELP("getenv", os_getenv, 1, 0,
"getenv(variable)",
"  Get the value of an environment variable."),

    JS_FN_HELP("getpid", os_getpid, 0, 0,
"getpid()",
"  Return the current process id."),

    JS_FN_HELP("system", os_system, 1, 0,
"system(command)",
"  Execute command on the current host, returning result code or throwing an\n"
"  exception on failure."),

#ifndef XP_WIN
    JS_FN_HELP("spawn", os_spawn, 1, 0,
"spawn(command)",
"  Start up a separate process running the given command. Returns the pid."),

    JS_FN_HELP("kill", os_kill, 1, 0,
"kill(pid[, signal])",
"  Send a signal to the given pid. The default signal is SIGINT. The signal\n"
"  passed in must be numeric, if given."),

    JS_FN_HELP("waitpid", os_waitpid, 1, 0,
"waitpid(pid[, nohang])",
"  Calls waitpid(). 'nohang' is a boolean indicating whether to pass WNOHANG.\n"
"  The return value is an object containing a 'pid' field, if a process was waitable\n"
"  and an 'exitStatus' field if a pid exited."),
#endif
    JS_FS_HELP_END
};

bool
js::DefineOS(JSContext *cx, HandleObject global)
{
    RootedObject obj(cx, JS_NewPlainObject(cx));
    return obj &&
           JS_DefineFunctionsWithHelp(cx, obj, os_functions) &&
           JS_DefineProperty(cx, global, "os", obj, 0);
}
