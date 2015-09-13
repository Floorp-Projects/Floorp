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
#include <direct.h>
#include <process.h>
#include <string.h>
#else
#include <sys/wait.h>
#include <unistd.h>
#endif

#include "jsapi.h"
// For JSFunctionSpecWithHelp
#include "jsfriendapi.h"
#include "jsobj.h"
#ifdef XP_WIN
# include "jswin.h"
#endif
#include "jswrapper.h"

#include "js/Conversions.h"
#include "shell/jsshell.h"
#include "vm/TypedArrayObject.h"

#include "jsobjinlines.h"

#ifdef XP_WIN
# define PATH_MAX (MAX_PATH > _MAX_DIR ? MAX_PATH : _MAX_DIR)
# define getcwd _getcwd
#else
# include <libgen.h>
#endif

using namespace JS;

namespace js {
namespace shell {

/*
 * Resolve a (possibly) relative filename to an absolute path. If
 * |scriptRelative| is true, then the result will be relative to the directory
 * containing the currently-running script, or the current working directory if
 * the currently-running script is "-e" (namely, you're using it from the
 * command line.) Otherwise, it will be relative to the current working
 * directory.
 */
JSString*
ResolvePath(JSContext* cx, HandleString filenameStr, PathResolutionMode resolveMode)
{
    JSAutoByteString filename(cx, filenameStr);
    if (!filename)
        return nullptr;

    const char* pathname = filename.ptr();
    if (pathname[0] == '/')
        return filenameStr;
#ifdef XP_WIN
    // Various forms of absolute paths per http://msdn.microsoft.com/en-us/library/windows/desktop/aa365247%28v=vs.85%29.aspx
    // "\..."
    if (pathname[0] == '\\')
        return filenameStr;
    // "C:\..."
    if (strlen(pathname) > 3 && isalpha(pathname[0]) && pathname[1] == ':' && pathname[2] == '\\')
        return filenameStr;
    // "\\..."
    if (strlen(pathname) > 2 && pathname[1] == '\\' && pathname[2] == '\\')
        return filenameStr;
#endif

    /* Get the currently executing script's name. */
    JS::AutoFilename scriptFilename;
    if (!DescribeScriptedCaller(cx, &scriptFilename))
        return nullptr;

    if (!scriptFilename.get())
        return nullptr;

    if (strcmp(scriptFilename.get(), "-e") == 0 || strcmp(scriptFilename.get(), "typein") == 0)
        resolveMode = RootRelative;

    static char buffer[PATH_MAX+1];
    if (resolveMode == ScriptRelative) {
#ifdef XP_WIN
        // The docs say it can return EINVAL, but the compiler says it's void
        _splitpath(scriptFilename.get(), nullptr, buffer, nullptr, nullptr);
#else
        strncpy(buffer, scriptFilename.get(), PATH_MAX+1);
        if (buffer[PATH_MAX] != '\0')
            return nullptr;

        // dirname(buffer) might return buffer, or it might return a
        // statically-allocated string
        memmove(buffer, dirname(buffer), strlen(buffer) + 1);
#endif
    } else {
        const char* cwd = getcwd(buffer, PATH_MAX);
        if (!cwd)
            return nullptr;
    }

    size_t len = strlen(buffer);
    buffer[len] = '/';
    strncpy(buffer + len + 1, pathname, sizeof(buffer) - (len+1));
    if (buffer[PATH_MAX] != '\0')
        return nullptr;

    return JS_NewStringCopyZ(cx, buffer);
}

static JSObject*
FileAsTypedArray(JSContext* cx, const char* pathname)
{
    FILE* file = fopen(pathname, "rb");
    if (!file) {
        JS_ReportError(cx, "can't open %s: %s", pathname, strerror(errno));
        return nullptr;
    }
    AutoCloseFile autoClose(file);

    RootedObject obj(cx);
    if (fseek(file, 0, SEEK_END) != 0) {
        JS_ReportError(cx, "can't seek end of %s", pathname);
    } else {
        size_t len = ftell(file);
        if (fseek(file, 0, SEEK_SET) != 0) {
            JS_ReportError(cx, "can't seek start of %s", pathname);
        } else {
            obj = JS_NewUint8Array(cx, len);
            if (!obj)
                return nullptr;
            char* buf = (char*) obj->as<js::TypedArrayObject>().viewData();
            size_t cc = fread(buf, 1, len, file);
            if (cc != len) {
                JS_ReportError(cx, "can't read %s: %s", pathname,
                               (ptrdiff_t(cc) < 0) ? strerror(errno) : "short read");
                obj = nullptr;
            }
        }
    }
    return obj;
}

static bool
ReadFile(JSContext* cx, unsigned argc, Value* vp, bool scriptRelative)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (args.length() < 1 || args.length() > 2) {
        JS_ReportErrorNumber(cx, js::shell::my_GetErrorMessage, nullptr,
                             args.length() < 1 ? JSSMSG_NOT_ENOUGH_ARGS : JSSMSG_TOO_MANY_ARGS,
                             "snarf");
        return false;
    }

    if (!args[0].isString() || (args.length() == 2 && !args[1].isString())) {
        JS_ReportErrorNumber(cx, js::shell::my_GetErrorMessage, nullptr, JSSMSG_INVALID_ARGS, "snarf");
        return false;
    }

    RootedString givenPath(cx, args[0].toString());
    RootedString str(cx, js::shell::ResolvePath(cx, givenPath, scriptRelative ? ScriptRelative : RootRelative));
    if (!str)
        return false;

    JSAutoByteString filename(cx, str);
    if (!filename)
        return false;

    if (args.length() > 1) {
        JSString* opt = JS::ToString(cx, args[1]);
        if (!opt)
            return false;
        bool match;
        if (!JS_StringEqualsAscii(cx, opt, "binary", &match))
            return false;
        if (match) {
            JSObject* obj;
            if (!(obj = FileAsTypedArray(cx, filename.ptr())))
                return false;
            args.rval().setObject(*obj);
            return true;
        }
    }

    if (!(str = FileAsString(cx, filename.ptr())))
        return false;
    args.rval().setString(str);
    return true;
}

static bool
osfile_readFile(JSContext* cx, unsigned argc, Value* vp)
{
    return ReadFile(cx, argc, vp, false);
}

static bool
osfile_readRelativeToScript(JSContext* cx, unsigned argc, Value* vp)
{
    return ReadFile(cx, argc, vp, true);
}

static bool
osfile_writeTypedArrayToFile(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (args.length() != 2 ||
        !args[0].isString() ||
        !args[1].isObject() ||
        !args[1].toObject().is<TypedArrayObject>())
    {
        JS_ReportErrorNumber(cx, my_GetErrorMessage, nullptr,
                             JSSMSG_INVALID_ARGS, "writeTypedArrayToFile");
        return false;
    }

    RootedString givenPath(cx, args[0].toString());
    RootedString str(cx, ResolvePath(cx, givenPath, RootRelative));
    if (!str)
        return false;

    JSAutoByteString filename(cx, str);
    if (!filename)
        return false;

    FILE* file = fopen(filename.ptr(), "wb");
    if (!file) {
        JS_ReportError(cx, "can't open %s: %s", filename.ptr(), strerror(errno));
        return false;
    }
    AutoCloseFile autoClose(file);

    TypedArrayObject* obj = &args[1].toObject().as<TypedArrayObject>();

    if (fwrite(obj->viewData(), obj->bytesPerElement(), obj->length(), file) != obj->length() ||
        !autoClose.release())
    {
        JS_ReportError(cx, "can't write %s", filename.ptr());
        return false;
    }

    args.rval().setUndefined();
    return true;
}

static bool
Redirect(JSContext* cx, FILE* fp, HandleString relFilename)
{
    RootedString filename(cx, ResolvePath(cx, relFilename, RootRelative));
    if (!filename)
        return false;
    JSAutoByteString filenameABS(cx, filename);
    if (!filenameABS)
        return false;
    if (freopen(filenameABS.ptr(), "wb", fp) == nullptr) {
        JS_ReportError(cx, "cannot redirect to %s: %s", filenameABS.ptr(), strerror(errno));
        return false;
    }
    return true;
}

static bool
osfile_redirect(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (args.length() < 1 || args.length() > 2) {
        JS_ReportErrorNumber(cx, my_GetErrorMessage, nullptr, JSSMSG_INVALID_ARGS, "redirect");
        return false;
    }

    if (args[0].isString()) {
        RootedString stdoutPath(cx, args[0].toString());
        if (!stdoutPath)
            return false;
        if (!Redirect(cx, stdout, stdoutPath))
            return false;
    }

    if (args.length() > 1 && args[1].isString()) {
        RootedString stderrPath(cx, args[1].toString());
        if (!stderrPath)
            return false;
        if (!Redirect(cx, stderr, stderrPath))
            return false;
    }

    args.rval().setUndefined();
    return true;
}

static const JSFunctionSpecWithHelp osfile_functions[] = {
    JS_FN_HELP("readFile", osfile_readFile, 1, 0,
"readFile(filename, [\"binary\"])",
"  Read filename into returned string. Filename is relative to the current\n"
               "  working directory."),

    JS_FN_HELP("readRelativeToScript", osfile_readRelativeToScript, 1, 0,
"readRelativeToScript(filename, [\"binary\"])",
"  Read filename into returned string. Filename is relative to the directory\n"
"  containing the current script."),

    JS_FS_HELP_END
};

static const JSFunctionSpecWithHelp osfile_unsafe_functions[] = {
    JS_FN_HELP("writeTypedArrayToFile", osfile_writeTypedArrayToFile, 2, 0,
"writeTypedArrayToFile(filename, data)",
"  Write the contents of a typed array to the named file."),

    JS_FN_HELP("redirect", osfile_redirect, 2, 0,
"redirect(stdoutFilename[, stderrFilename])",
"  Redirect stdout and/or stderr to the named file. Pass undefined to avoid\n"
"   redirecting. Filenames are relative to the current working directory."),

    JS_FS_HELP_END
};

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

    if (const char* valueBytes = getenv(keyBytes.ptr())) {
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

inline char*
strerror_message(int result, char* buffer)
{
    return result == 0 ? buffer : nullptr;
}

inline char*
strerror_message(char* result, char* buffer)
{
    return result;
}

#endif

static void
ReportSysError(JSContext* cx, const char* prefix)
{
    char buffer[200];

#if defined(XP_WIN)
    strerror_s(buffer, sizeof(buffer), errno);
    const char* errstr = buffer;
#else
    const char* errstr = strerror_message(strerror_r(errno, buffer, sizeof(buffer)), buffer);
#endif

    if (!errstr)
        errstr = "unknown error";

    size_t nbytes = strlen(prefix) + strlen(errstr) + 3;
    char* final = (char*) js_malloc(nbytes);
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
os_system(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (args.length() == 0) {
        JS_ReportError(cx, "os.system requires 1 argument");
        return false;
    }

    JSString* str = JS::ToString(cx, args[0]);
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
os_spawn(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (args.length() == 0) {
        JS_ReportError(cx, "os.spawn requires 1 argument");
        return false;
    }

    JSString* str = JS::ToString(cx, args[0]);
    if (!str)
        return false;

    JSAutoByteString command(cx, str);
    if (!command)
        return false;

    int32_t childPid = fork();
    if (childPid == -1) {
        ReportSysError(cx, "fork failed");
        return false;
    }

    if (childPid) {
        args.rval().setInt32(childPid);
        return true;
    }

    // We are in the child

    const char* cmd[] = {"sh", "-c", nullptr, nullptr};
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
    if (status == -1) {
        ReportSysError(cx, "kill failed");
        return false;
    }

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

    int status = 0;
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
        if (WIFEXITED(status)) {
            v.setInt32(WEXITSTATUS(status));
            if (!JS_DefineProperty(cx, info, "exitStatus", v, JSPROP_ENUMERATE))
                return false;
        }
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
DefineOS(JSContext* cx, HandleObject global, bool fuzzingSafe)
{
    RootedObject obj(cx, JS_NewPlainObject(cx));
    if (!obj || !JS_DefineProperty(cx, global, "os", obj, 0))
        return false;

    if (!fuzzingSafe) {
        if (!JS_DefineFunctionsWithHelp(cx, obj, os_functions))
            return false;
    }

    RootedObject osfile(cx, JS_NewPlainObject(cx));
    if (!osfile ||
        !JS_DefineFunctionsWithHelp(cx, osfile, osfile_functions) ||
        !JS_DefineProperty(cx, obj, "file", osfile, 0))
    {
        return false;
    }

    if (!fuzzingSafe) {
        if (!JS_DefineFunctionsWithHelp(cx, osfile, osfile_unsafe_functions))
            return false;
    }

    // For backwards compatibility, expose various os.file.* functions as
    // direct methods on the global.
    RootedValue val(cx);

    struct {
        const char* src;
        const char* dst;
    } osfile_exports[] = {
        { "readFile", "read" },
        { "readFile", "snarf" },
        { "readRelativeToScript", "readRelativeToScript" },
        { "redirect", "redirect" }
    };

    for (auto pair : osfile_exports) {
        if (!JS_GetProperty(cx, osfile, pair.src, &val))
            return false;
        if (val.isObject()) {
            RootedObject function(cx, &val.toObject());
            if (!JS_DefineProperty(cx, global, pair.dst, function, 0))
                return false;
        }
    }

    return true;
}

} // namespace shell
} // namespace js
