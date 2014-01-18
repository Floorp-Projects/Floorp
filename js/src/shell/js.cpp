/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* JS shell. */

#include "mozilla/ArrayUtils.h"
#include "mozilla/Atomics.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/GuardObjects.h"
#include "mozilla/PodOperations.h"

#ifdef XP_WIN
# include <direct.h>
# include <process.h>
#endif
#include <errno.h>
#include <fcntl.h>
#if defined(XP_OS2) || defined(XP_WIN)
# include <io.h>     /* for isatty() */
#endif
#include <locale.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#ifdef XP_UNIX
# include <sys/mman.h>
# include <sys/stat.h>
# include <sys/wait.h>
# include <unistd.h>
#endif

#include "jsapi.h"
#include "jsarray.h"
#include "jsatom.h"
#include "jscntxt.h"
#include "jsfun.h"
#ifdef JS_THREADSAFE
#include "jslock.h"
#endif
#include "jsobj.h"
#include "jsprf.h"
#include "jsscript.h"
#include "jstypes.h"
#include "jsutil.h"
#ifdef XP_WIN
# include "jswin.h"
#endif
#include "jsworkers.h"
#include "jswrapper.h"
#include "prmjtime.h"
#if JS_TRACE_LOGGING
#include "TraceLogging.h"
#endif

#include "builtin/TestingFunctions.h"
#include "frontend/Parser.h"
#include "jit/Ion.h"
#include "js/OldDebugAPI.h"
#include "js/StructuredClone.h"
#include "perf/jsperf.h"
#include "shell/jsheaptools.h"
#include "shell/jsoptparse.h"
#include "vm/ArgumentsObject.h"
#include "vm/Monitor.h"
#include "vm/Shape.h"
#include "vm/TypedArrayObject.h"
#include "vm/WrapperObject.h"

#include "jscompartmentinlines.h"
#include "jsobjinlines.h"

#ifdef XP_WIN
# define PATH_MAX (MAX_PATH > _MAX_DIR ? MAX_PATH : _MAX_DIR)
#else
# include <libgen.h>
#endif

using namespace js;
using namespace js::cli;

using mozilla::ArrayLength;
using mozilla::DoubleEqualsInt32;
using mozilla::Maybe;
using mozilla::PodCopy;

enum JSShellExitCode {
    EXITCODE_RUNTIME_ERROR      = 3,
    EXITCODE_FILE_NOT_FOUND     = 4,
    EXITCODE_OUT_OF_MEMORY      = 5,
    EXITCODE_TIMEOUT            = 6
};

static size_t gStackChunkSize = 8192;

/*
 * Note: This limit should match the stack limit set by the browser in
 *       js/xpconnect/src/XPCJSRuntime.cpp
 */
#if defined(MOZ_ASAN) || (defined(DEBUG) && !defined(XP_WIN))
static size_t gMaxStackSize = 2 * 128 * sizeof(size_t) * 1024;
#else
static size_t gMaxStackSize = 128 * sizeof(size_t) * 1024;
#endif

/*
 * Limit the timeout to 30 minutes to prevent an overflow on platfoms
 * that represent the time internally in microseconds using 32-bit int.
 */
static double MAX_TIMEOUT_INTERVAL = 1800.0;
static double gTimeoutInterval = -1.0;
static volatile bool gTimedOut = false;
static JS::Value gTimeoutFunc;

static bool enableTypeInference = true;
static bool enableDisassemblyDumps = false;
static bool enableIon = true;
static bool enableBaseline = true;
static bool enableAsmJS = true;

static bool printTiming = false;
static const char *jsCacheDir = nullptr;
static const char *jsCacheAsmJSPath = nullptr;
mozilla::Atomic<int32_t> jsCacheOpened(false);

static bool
SetTimeoutValue(JSContext *cx, double t);

static bool
InitWatchdog(JSRuntime *rt);

static void
KillWatchdog();

static bool
ScheduleWatchdog(JSRuntime *rt, double t);

static void
CancelExecution(JSRuntime *rt);

/*
 * Watchdog thread state.
 */
#ifdef JS_THREADSAFE

static PRLock *gWatchdogLock = nullptr;
static PRCondVar *gWatchdogWakeup = nullptr;
static PRThread *gWatchdogThread = nullptr;
static bool gWatchdogHasTimeout = false;
static int64_t gWatchdogTimeout = 0;

static PRCondVar *gSleepWakeup = nullptr;

#else

static JSRuntime *gRuntime = nullptr;

#endif

static int gExitCode = 0;
static bool gQuitting = false;
static bool gGotError = false;
static FILE *gErrFile = nullptr;
static FILE *gOutFile = nullptr;

static bool reportWarnings = true;
static bool compileOnly = false;
static bool fuzzingSafe = false;

#ifdef DEBUG
static bool dumpEntrainedVariables = false;
static bool OOM_printAllocationCount = false;
#endif

enum JSShellErrNum {
#define MSG_DEF(name, number, count, exception, format) \
    name = number,
#include "jsshell.msg"
#undef MSG_DEF
    JSShellErr_Limit
};

static JSContext *
NewContext(JSRuntime *rt);

static void
DestroyContext(JSContext *cx, bool withGC);

static const JSErrorFormatString *
my_GetErrorMessage(void *userRef, const char *locale, const unsigned errorNumber);

#ifdef EDITLINE
extern "C" {
extern JS_EXPORT_API(char *) readline(const char *prompt);
extern JS_EXPORT_API(void)   add_history(char *line);
} // extern "C"
#endif

static char *
GetLine(FILE *file, const char * prompt)
{
    size_t size;
    char *buffer;
#ifdef EDITLINE
    /*
     * Use readline only if file is stdin, because there's no way to specify
     * another handle.  Are other filehandles interactive?
     */
    if (file == stdin) {
        char *linep = readline(prompt);
        /*
         * We set it to zero to avoid complaining about inappropriate ioctl
         * for device in the case of EOF. Looks like errno == 251 if line is
         * finished with EOF and errno == 25 (EINVAL on Mac) if there is
         * nothing left to read.
         */
        if (errno == 251 || errno == 25 || errno == EINVAL)
            errno = 0;
        if (!linep)
            return nullptr;
        if (linep[0] != '\0')
            add_history(linep);
        return linep;
    }
#endif
    size_t len = 0;
    if (*prompt != '\0') {
        fprintf(gOutFile, "%s", prompt);
        fflush(gOutFile);
    }
    size = 80;
    buffer = (char *) malloc(size);
    if (!buffer)
        return nullptr;
    char *current = buffer;
    while (fgets(current, size - len, file)) {
        len += strlen(current);
        char *t = buffer + len - 1;
        if (*t == '\n') {
            /* Line was read. We remove '\n' and exit. */
            *t = '\0';
            return buffer;
        }
        if (len + 1 == size) {
            size = size * 2;
            char *tmp = (char *) js_realloc(buffer, size);
            if (!tmp) {
                free(buffer);
                return nullptr;
            }
            buffer = tmp;
        }
        current = buffer + len;
    }
    if (len && !ferror(file))
        return buffer;
    free(buffer);
    return nullptr;
}

static char *
JSStringToUTF8(JSContext *cx, JSString *str)
{
    JSLinearString *linear = str->ensureLinear(cx);
    if (!linear)
        return nullptr;

    return TwoByteCharsToNewUTF8CharsZ(cx, linear->range()).c_str();
}

/*
 * State to store as JSContext private.
 *
 * We declare such timestamp as volatile as they are updated in the operation
 * callback without taking any locks. Any possible race can only lead to more
 * frequent callback calls. This is safe as the callback does everything based
 * on timing.
 */
struct JSShellContextData {
    volatile int64_t startTime;
};

static JSShellContextData *
NewContextData()
{
    /* Prevent creation of new contexts after we have been canceled. */
    if (gTimedOut)
        return nullptr;

    JSShellContextData *data = (JSShellContextData *)
                               js_calloc(sizeof(JSShellContextData), 1);
    if (!data)
        return nullptr;
    data->startTime = PRMJ_Now();
    return data;
}

static inline JSShellContextData *
GetContextData(JSContext *cx)
{
    JSShellContextData *data = (JSShellContextData *) JS_GetContextPrivate(cx);

    JS_ASSERT(data);
    return data;
}

static bool
ShellOperationCallback(JSContext *cx)
{
    if (!gTimedOut)
        return true;

    JS_ClearPendingException(cx);

    bool result;
    if (!gTimeoutFunc.isNull()) {
        JSAutoCompartment ac(cx, &gTimeoutFunc.toObject());
        RootedValue returnedValue(cx);
        if (!JS_CallFunctionValue(cx, nullptr, gTimeoutFunc, 0, nullptr, returnedValue.address()))
            return false;
        if (returnedValue.isBoolean())
            result = returnedValue.toBoolean();
        else
            result = false;
    } else {
        result = false;
    }

    if (!result && gExitCode == 0)
        gExitCode = EXITCODE_TIMEOUT;

    return result;
}

/*
 * Some UTF-8 files, notably those written using Notepad, have a Unicode
 * Byte-Order-Mark (BOM) as their first character. This is useless (byte-order
 * is meaningless for UTF-8) but causes a syntax error unless we skip it.
 */
static void
SkipUTF8BOM(FILE* file)
{
    int ch1 = fgetc(file);
    int ch2 = fgetc(file);
    int ch3 = fgetc(file);

    // Skip the BOM
    if (ch1 == 0xEF && ch2 == 0xBB && ch3 == 0xBF)
        return;

    // No BOM - revert
    if (ch3 != EOF)
        ungetc(ch3, file);
    if (ch2 != EOF)
        ungetc(ch2, file);
    if (ch1 != EOF)
        ungetc(ch1, file);
}

static void
RunFile(JSContext *cx, Handle<JSObject*> obj, const char *filename, FILE *file, bool compileOnly)
{
    SkipUTF8BOM(file);

    // To support the UNIX #! shell hack, gobble the first line if it starts
    // with '#'.
    int ch = fgetc(file);
    if (ch == '#') {
        while ((ch = fgetc(file)) != EOF) {
            if (ch == '\n' || ch == '\r')
                break;
        }
    }
    ungetc(ch, file);

    int64_t t1 = PRMJ_Now();
    RootedScript script(cx);

    {
        JS::AutoSaveContextOptions asco(cx);
        JS::ContextOptionsRef(cx).setNoScriptRval(true);

        CompileOptions options(cx);
        options.setUTF8(true)
               .setFileAndLine(filename, 1)
               .setCompileAndGo(true);

        gGotError = false;
        script = JS::Compile(cx, obj, options, file);
        JS_ASSERT_IF(!script, gGotError);
    }

    #ifdef DEBUG
        if (dumpEntrainedVariables)
            AnalyzeEntrainedVariables(cx, script);
    #endif
    if (script && !compileOnly) {
        if (!JS_ExecuteScript(cx, obj, script, nullptr)) {
            if (!gQuitting && !gTimedOut)
                gExitCode = EXITCODE_RUNTIME_ERROR;
        }
        int64_t t2 = PRMJ_Now() - t1;
        if (printTiming)
            printf("runtime = %.3f ms\n", double(t2) / PRMJ_USEC_PER_MSEC);
    }
}

static bool
EvalAndPrint(JSContext *cx, Handle<JSObject*> global, const char *bytes, size_t length,
             int lineno, bool compileOnly, FILE *out)
{
    // Eval.
    JS::CompileOptions options(cx);
    options.setUTF8(true)
           .setCompileAndGo(true)
           .setFileAndLine("typein", lineno);
    RootedScript script(cx);
    script = JS::Compile(cx, global, options, bytes, length);
    if (!script)
        return false;
    if (compileOnly)
        return true;
    RootedValue result(cx);
    if (!JS_ExecuteScript(cx, global, script, result.address()))
        return false;

    if (!result.isUndefined()) {
        // Print.
        RootedString str(cx);
        str = JS_ValueToSource(cx, result);
        if (!str)
            return false;

        char *utf8chars = JSStringToUTF8(cx, str);
        if (!utf8chars)
            return false;
        fprintf(out, "%s\n", utf8chars);
        JS_free(cx, utf8chars);
    }
    return true;
}

static void
ReadEvalPrintLoop(JSContext *cx, Handle<JSObject*> global, FILE *in, FILE *out, bool compileOnly)
{
    int lineno = 1;
    bool hitEOF = false;

    do {
        /*
         * Accumulate lines until we get a 'compilable unit' - one that either
         * generates an error (before running out of source) or that compiles
         * cleanly.  This should be whenever we get a complete statement that
         * coincides with the end of a line.
         */
        int startline = lineno;
        typedef Vector<char, 32, ContextAllocPolicy> CharBuffer;
        CharBuffer buffer(cx);
        do {
            ScheduleWatchdog(cx->runtime(), -1);
            gTimedOut = false;
            errno = 0;

            char *line = GetLine(in, startline == lineno ? "js> " : "");
            if (!line) {
                if (errno) {
                    JS_ReportError(cx, strerror(errno));
                    return;
                }
                hitEOF = true;
                break;
            }

            if (!buffer.append(line, strlen(line)) || !buffer.append('\n'))
                return;

            lineno++;
            if (!ScheduleWatchdog(cx->runtime(), gTimeoutInterval)) {
                hitEOF = true;
                break;
            }
        } while (!JS_BufferIsCompilableUnit(cx, global, buffer.begin(), buffer.length()));

        if (hitEOF && buffer.empty())
            break;

        if (!EvalAndPrint(cx, global, buffer.begin(), buffer.length(), startline, compileOnly,
                          out))
        {
            // Catch the error, report it, and keep going.
            JS_ReportPendingException(cx);
        }
    } while (!hitEOF && !gQuitting);

    fprintf(out, "\n");
}

class AutoCloseInputFile
{
  private:
    FILE *f_;
  public:
    explicit AutoCloseInputFile(FILE *f) : f_(f) {}
    ~AutoCloseInputFile() {
        if (f_ && f_ != stdin)
            fclose(f_);
    }
};

static void
Process(JSContext *cx, JSObject *obj_, const char *filename, bool forceTTY)
{
    RootedObject obj(cx, obj_);

    FILE *file;
    if (forceTTY || !filename || strcmp(filename, "-") == 0) {
        file = stdin;
    } else {
        file = fopen(filename, "r");
        if (!file) {
            JS_ReportErrorNumber(cx, my_GetErrorMessage, nullptr,
                                 JSSMSG_CANT_OPEN, filename, strerror(errno));
            gExitCode = EXITCODE_FILE_NOT_FOUND;
            return;
        }
    }
    AutoCloseInputFile autoClose(file);

    if (!forceTTY && !isatty(fileno(file))) {
        // It's not interactive - just execute it.
        RunFile(cx, obj, filename, file, compileOnly);
    } else {
        // It's an interactive filehandle; drop into read-eval-print loop.
        ReadEvalPrintLoop(cx, obj, file, gOutFile, compileOnly);
    }
}

static bool
Version(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    JSVersion origVersion = JS_GetVersion(cx);
    if (args.length() == 0 || JSVAL_IS_VOID(args[0])) {
        /* Get version. */
        args.rval().setInt32(origVersion);
    } else {
        /* Set version. */
        int32_t v = -1;
        if (args[0].isInt32()) {
            v = args[0].toInt32();
        } else if (args[0].isDouble()) {
            double fv = args[0].toDouble();
            int32_t fvi;
            if (DoubleEqualsInt32(fv, &fvi))
                v = fvi;
        }
        if (v < 0 || v > JSVERSION_LATEST) {
            JS_ReportErrorNumber(cx, my_GetErrorMessage, nullptr, JSSMSG_INVALID_ARGS, "version");
            return false;
        }
        JS_SetVersionForCompartment(js::GetContextCompartment(cx), JSVersion(v));
        args.rval().setInt32(origVersion);
    }
    return true;
}

static JSScript *
GetTopScript(JSContext *cx)
{
    RootedScript script(cx);
    JS_DescribeScriptedCaller(cx, &script, nullptr);
    return script;
}

/*
 * Resolve a (possibly) relative filename to an absolute path. If
 * |scriptRelative| is true, then the result will be relative to the directory
 * containing the currently-running script, or the current working directory if
 * the currently-running script is "-e" (namely, you're using it from the
 * command line.) Otherwise, it will be relative to the current working
 * directory.
 */
static JSString *
ResolvePath(JSContext *cx, HandleString filenameStr, bool scriptRelative)
{
    JSAutoByteString filename(cx, filenameStr);
    if (!filename)
        return nullptr;

    const char *pathname = filename.ptr();
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
    RootedScript script(cx, GetTopScript(cx));
    if (!script->filename())
        return nullptr;
    if (strcmp(script->filename(), "-e") == 0 || strcmp(script->filename(), "typein") == 0)
        scriptRelative = false;

    static char buffer[PATH_MAX+1];
    if (scriptRelative) {
#ifdef XP_WIN
        // The docs say it can return EINVAL, but the compiler says it's void
        _splitpath(script->filename(), nullptr, buffer, nullptr, nullptr);
#else
        strncpy(buffer, script->filename(), PATH_MAX+1);
        if (buffer[PATH_MAX] != '\0')
            return nullptr;

        // dirname(buffer) might return buffer, or it might return a
        // statically-allocated string
        memmove(buffer, dirname(buffer), strlen(buffer) + 1);
#endif
    } else {
        const char *cwd = getcwd(buffer, PATH_MAX);
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

static bool
Options(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    JS::ContextOptions oldOptions = JS::ContextOptionsRef(cx);
    for (unsigned i = 0; i < args.length(); i++) {
        JSString *str = JS::ToString(cx, args[i]);
        if (!str)
            return false;
        args[i].setString(str);

        JSAutoByteString opt(cx, str);
        if (!opt)
            return false;

        if (strcmp(opt.ptr(), "strict") == 0)
            JS::ContextOptionsRef(cx).toggleExtraWarnings();
        else if (strcmp(opt.ptr(), "typeinfer") == 0)
            JS::ContextOptionsRef(cx).toggleTypeInference();
        else if (strcmp(opt.ptr(), "werror") == 0)
            JS::ContextOptionsRef(cx).toggleWerror();
        else if (strcmp(opt.ptr(), "strict_mode") == 0)
            JS::ContextOptionsRef(cx).toggleStrictMode();
        else {
            char* msg = JS_sprintf_append(nullptr,
                                          "unknown option name '%s'."
                                          " The valid names are strict,"
                                          " typeinfer, werror, and strict_mode.",
                                          opt.ptr());
            if (!msg) {
                JS_ReportOutOfMemory(cx);
                return false;
            }

            JS_ReportError(cx, msg);
            free(msg);
            return false;
        }
    }

    char *names = strdup("");
    bool found = false;
    if (!names && oldOptions.extraWarnings()) {
        names = JS_sprintf_append(names, "%s%s", found ? "," : "", "strict");
        found = true;
    }
    if (!names && oldOptions.typeInference()) {
        names = JS_sprintf_append(names, "%s%s", found ? "," : "", "typeinfer");
        found = true;
    }
    if (!names && oldOptions.werror()) {
        names = JS_sprintf_append(names, "%s%s", found ? "," : "", "werror");
        found = true;
    }
    if (!names && oldOptions.strictMode()) {
        names = JS_sprintf_append(names, "%s%s", found ? "," : "", "strict_mode");
        found = true;
    }
    if (!names) {
        JS_ReportOutOfMemory(cx);
        return false;
    }

    JSString *str = JS_NewStringCopyZ(cx, names);
    free(names);
    if (!str)
        return false;
    args.rval().setString(str);
    return true;
}

static bool
LoadScript(JSContext *cx, unsigned argc, jsval *vp, bool scriptRelative)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    RootedObject thisobj(cx, JS_THIS_OBJECT(cx, vp));
    if (!thisobj)
        return false;

    RootedString str(cx);
    for (unsigned i = 0; i < args.length(); i++) {
        str = JS::ToString(cx, args[i]);
        if (!str) {
            JS_ReportErrorNumber(cx, my_GetErrorMessage, nullptr, JSSMSG_INVALID_ARGS, "load");
            return false;
        }
        str = ResolvePath(cx, str, scriptRelative);
        if (!str) {
            JS_ReportError(cx, "unable to resolve path");
            return false;
        }
        JSAutoByteString filename(cx, str);
        if (!filename)
            return false;
        errno = 0;
        CompileOptions opts(cx);
        opts.setUTF8(true).setCompileAndGo(true).setNoScriptRval(true);
        if ((compileOnly && !Compile(cx, thisobj, opts, filename.ptr())) ||
            !Evaluate(cx, thisobj, opts, filename.ptr(), nullptr))
        {
            return false;
        }
    }

    args.rval().setUndefined();
    return true;
}

static bool
Load(JSContext *cx, unsigned argc, jsval *vp)
{
    return LoadScript(cx, argc, vp, false);
}

static bool
LoadScriptRelativeToScript(JSContext *cx, unsigned argc, jsval *vp)
{
    return LoadScript(cx, argc, vp, true);
}

class AutoNewContext
{
  private:
    JSContext *oldcx;
    JSContext *newcx;
    Maybe<JSAutoRequest> newRequest;
    Maybe<AutoCompartment> newCompartment;

    AutoNewContext(const AutoNewContext &) MOZ_DELETE;

  public:
    AutoNewContext() : oldcx(nullptr), newcx(nullptr) {}

    bool enter(JSContext *cx) {
        JS_ASSERT(!JS_IsExceptionPending(cx));
        oldcx = cx;
        newcx = NewContext(JS_GetRuntime(cx));
        if (!newcx)
            return false;
        JS::ContextOptionsRef(newcx).setDontReportUncaught(true);
        js::SetDefaultObjectForContext(newcx, JS::CurrentGlobalOrNull(cx));

        newRequest.construct(newcx);
        newCompartment.construct(newcx, JS::CurrentGlobalOrNull(cx));
        return true;
    }

    JSContext *get() { return newcx; }

    ~AutoNewContext() {
        if (newcx) {
            RootedValue exc(oldcx);
            bool throwing = JS_IsExceptionPending(newcx);
            if (throwing)
                JS_GetPendingException(newcx, &exc);
            newCompartment.destroy();
            newRequest.destroy();
            if (throwing)
                JS_SetPendingException(oldcx, exc);
            DestroyContext(newcx, false);
        }
    }
};

class AutoSaveFrameChain
{
    JSContext *cx_;
    bool saved_;

  public:
    AutoSaveFrameChain(JSContext *cx)
      : cx_(cx),
        saved_(false)
    {}

    bool save() {
        if (!JS_SaveFrameChain(cx_))
            return false;
        saved_ = true;
        return true;
    }

    ~AutoSaveFrameChain() {
        if (saved_)
            JS_RestoreFrameChain(cx_);
    }
};

static bool
Evaluate(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (args.length() < 1 || args.length() > 2) {
        JS_ReportErrorNumber(cx, my_GetErrorMessage, nullptr,
                             args.length() < 1 ? JSSMSG_NOT_ENOUGH_ARGS : JSSMSG_TOO_MANY_ARGS,
                             "evaluate");
        return false;
    }
    if (!args[0].isString() || (args.length() == 2 && args[1].isPrimitive())) {
        JS_ReportErrorNumber(cx, my_GetErrorMessage, nullptr, JSSMSG_INVALID_ARGS, "evaluate");
        return false;
    }

    bool newContext = false;
    bool compileAndGo = true;
    bool noScriptRval = false;
    const char *fileName = "@evaluate";
    RootedObject element(cx);
    RootedString elementProperty(cx);
    JSAutoByteString fileNameBytes;
    RootedString displayURL(cx);
    RootedString sourceMapURL(cx);
    unsigned lineNumber = 1;
    RootedObject global(cx, nullptr);
    bool catchTermination = false;
    bool saveFrameChain = false;
    RootedObject callerGlobal(cx, cx->global());
    CompileOptions::SourcePolicy sourcePolicy = CompileOptions::SAVE_SOURCE;

    global = JS_GetGlobalForObject(cx, &args.callee());
    if (!global)
        return false;

    if (args.length() == 2) {
        RootedObject opts(cx, &args[1].toObject());
        RootedValue v(cx);

        if (!JS_GetProperty(cx, opts, "newContext", &v))
            return false;
        if (!v.isUndefined())
            newContext = ToBoolean(v);

        if (!JS_GetProperty(cx, opts, "compileAndGo", &v))
            return false;
        if (!v.isUndefined())
            compileAndGo = ToBoolean(v);

        if (!JS_GetProperty(cx, opts, "noScriptRval", &v))
            return false;
        if (!v.isUndefined())
            noScriptRval = ToBoolean(v);

        if (!JS_GetProperty(cx, opts, "fileName", &v))
            return false;
        if (v.isNull()) {
            fileName = nullptr;
        } else if (!v.isUndefined()) {
            JSString *s = ToString(cx, v);
            if (!s)
                return false;
            fileName = fileNameBytes.encodeLatin1(cx, s);
            if (!fileName)
                return false;
        }

        if (!JS_GetProperty(cx, opts, "element", &v))
            return false;
        if (v.isObject())
            element = &v.toObject();

        if (!JS_GetProperty(cx, opts, "elementProperty", &v))
            return false;
        if (!v.isUndefined()) {
            elementProperty = ToString(cx, v);
            if (!elementProperty)
                return false;
        }

        if (!JS_GetProperty(cx, opts, "displayURL", &v))
            return false;
        if (!v.isUndefined()) {
            displayURL = ToString(cx, v);
            if (!displayURL)
                return false;
        }

        if (!JS_GetProperty(cx, opts, "sourceMapURL", &v))
            return false;
        if (!v.isUndefined()) {
            sourceMapURL = ToString(cx, v);
            if (!sourceMapURL)
                return false;
        }

        if (!JS_GetProperty(cx, opts, "lineNumber", &v))
            return false;
        if (!v.isUndefined()) {
            uint32_t u;
            if (!ToUint32(cx, v, &u))
                return false;
            lineNumber = u;
        }

        if (!JS_GetProperty(cx, opts, "global", &v))
            return false;
        if (!v.isUndefined()) {
            if (v.isObject()) {
                global = js::UncheckedUnwrap(&v.toObject());
                if (!global)
                    return false;
            }
            if (!global || !(JS_GetClass(global)->flags & JSCLASS_IS_GLOBAL)) {
                JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_UNEXPECTED_TYPE,
                                     "\"global\" passed to evaluate()", "not a global object");
                return false;
            }
        }

        if (!JS_GetProperty(cx, opts, "catchTermination", &v))
            return false;
        if (!v.isUndefined())
            catchTermination = ToBoolean(v);

        if (!JS_GetProperty(cx, opts, "saveFrameChain", &v))
            return false;
        if (!v.isUndefined())
            saveFrameChain = ToBoolean(v);

        if (!JS_GetProperty(cx, opts, "sourcePolicy", &v))
            return false;
        if (!v.isUndefined()) {
            JSString *s = ToString(cx, v);
            if (!s)
                return false;
            char *policy = JS_EncodeStringToUTF8(cx, s);
            if (!policy)
                return false;
            if (strcmp(policy, "NO_SOURCE") == 0) {
                sourcePolicy = CompileOptions::NO_SOURCE;
            } else if (strcmp(policy, "LAZY_SOURCE") == 0) {
                sourcePolicy = CompileOptions::LAZY_SOURCE;
            } else if (strcmp(policy, "SAVE_SOURCE") == 0) {
                sourcePolicy = CompileOptions::SAVE_SOURCE;
            } else {
                JS_ReportError(cx, "bad 'sourcePolicy' option passed to 'evaluate': '%s'",
                               policy);
                return false;
            }
        }
    }

    RootedString code(cx, args[0].toString());

    size_t codeLength;
    const jschar *codeChars = JS_GetStringCharsAndLength(cx, code, &codeLength);
    if (!codeChars)
        return false;

    AutoNewContext ancx;
    if (newContext) {
        if (!ancx.enter(cx))
            return false;
        cx = ancx.get();
    }

    {
        AutoSaveFrameChain asfc(cx);
        if (saveFrameChain && !asfc.save())
            return false;

        JSAutoCompartment ac(cx, global);
        RootedScript script(cx);

        {
            JS::AutoSaveContextOptions asco(cx);
            JS::ContextOptionsRef(cx).setNoScriptRval(noScriptRval);

            CompileOptions options(cx);
            options.setFileAndLine(fileName, lineNumber)
                   .setElement(element)
                   .setElementProperty(elementProperty)
                   .setSourcePolicy(sourcePolicy)
                   .setCompileAndGo(compileAndGo);

            script = JS::Compile(cx, global, options, codeChars, codeLength);
            if (!script)
                return false;
        }

        if (displayURL && !script->scriptSource()->hasDisplayURL()) {
            const jschar *durl = JS_GetStringCharsZ(cx, displayURL);
            if (!durl)
                return false;
            if (!script->scriptSource()->setDisplayURL(cx, durl))
                return false;
        }
        if (sourceMapURL && !script->scriptSource()->hasSourceMapURL()) {
            const jschar *smurl = JS_GetStringCharsZ(cx, sourceMapURL);
            if (!smurl)
                return false;
            if (!script->scriptSource()->setSourceMapURL(cx, smurl))
                return false;
        }
        if (!JS_ExecuteScript(cx, global, script, vp)) {
            if (catchTermination && !JS_IsExceptionPending(cx)) {
                JSAutoCompartment ac1(cx, callerGlobal);
                JSString *str = JS_NewStringCopyZ(cx, "terminated");
                if (!str)
                    return false;
                args.rval().setString(str);
                return true;
            }
            return false;
        }
    }

    return JS_WrapValue(cx, args.rval());
}

static JSString *
FileAsString(JSContext *cx, const char *pathname)
{
    FILE *file;
    RootedString str(cx);
    size_t len, cc;
    char *buf;

    file = fopen(pathname, "rb");
    if (!file) {
        JS_ReportError(cx, "can't open %s: %s", pathname, strerror(errno));
        return nullptr;
    }
    AutoCloseInputFile autoClose(file);

    if (fseek(file, 0, SEEK_END) != 0) {
        JS_ReportError(cx, "can't seek end of %s", pathname);
    } else {
        len = ftell(file);
        if (fseek(file, 0, SEEK_SET) != 0) {
            JS_ReportError(cx, "can't seek start of %s", pathname);
        } else {
            buf = (char*) JS_malloc(cx, len + 1);
            if (buf) {
                cc = fread(buf, 1, len, file);
                if (cc != len) {
                    JS_ReportError(cx, "can't read %s: %s", pathname,
                                   (ptrdiff_t(cc) < 0) ? strerror(errno) : "short read");
                } else {
                    jschar *ucbuf =
                        JS::UTF8CharsToNewTwoByteCharsZ(cx, JS::UTF8Chars(buf, len), &len).get();
                    if (!ucbuf) {
                        JS_ReportError(cx, "Invalid UTF-8 in file '%s'", pathname);
                        gExitCode = EXITCODE_RUNTIME_ERROR;
                        return nullptr;
                    }
                    str = JS_NewUCStringCopyN(cx, ucbuf, len);
                    free(ucbuf);
                }
                JS_free(cx, buf);
            }
        }
    }

    return str;
}

static JSObject *
FileAsTypedArray(JSContext *cx, const char *pathname)
{
    FILE *file = fopen(pathname, "rb");
    if (!file) {
        JS_ReportError(cx, "can't open %s: %s", pathname, strerror(errno));
        return nullptr;
    }
    AutoCloseInputFile autoClose(file);

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
            char *buf = (char *) obj->as<TypedArrayObject>().viewData();
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

/*
 * Function to run scripts and return compilation + execution time. Semantics
 * are closely modelled after the equivalent function in WebKit, as this is used
 * to produce benchmark timings by SunSpider.
 */
static bool
Run(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    if (args.length() != 1) {
        JS_ReportErrorNumber(cx, my_GetErrorMessage, nullptr, JSSMSG_INVALID_ARGS, "run");
        return false;
    }

    RootedObject thisobj(cx, JS_THIS_OBJECT(cx, vp));
    if (!thisobj)
        return false;

    JSString *str = JS::ToString(cx, args[0]);
    if (!str)
        return false;
    args[0].setString(str);
    JSAutoByteString filename(cx, str);
    if (!filename)
        return false;

    const jschar *ucbuf = nullptr;
    size_t buflen;
    str = FileAsString(cx, filename.ptr());
    if (str)
        ucbuf = JS_GetStringCharsAndLength(cx, str, &buflen);
    if (!ucbuf)
        return false;

    JS::Anchor<JSString *> a_str(str);

    RootedScript script(cx);
    int64_t startClock = PRMJ_Now();
    {
        JS::AutoSaveContextOptions asco(cx);
        JS::ContextOptionsRef(cx).setNoScriptRval(true);

        JS::CompileOptions options(cx);
        options.setFileAndLine(filename.ptr(), 1)
               .setCompileAndGo(true);
        script = JS_CompileUCScript(cx, thisobj, ucbuf, buflen, options);
        if (!script)
            return false;
    }

    if (!JS_ExecuteScript(cx, thisobj, script, nullptr))
        return false;

    int64_t endClock = PRMJ_Now();

    args.rval().setDouble((endClock - startClock) / double(PRMJ_USEC_PER_MSEC));
    return true;
}

/*
 * function readline()
 * Provides a hook for scripts to read a line from stdin.
 */
static bool
ReadLine(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

#define BUFSIZE 256
    FILE *from = stdin;
    size_t buflength = 0;
    size_t bufsize = BUFSIZE;
    char *buf = (char *) JS_malloc(cx, bufsize);
    if (!buf)
        return false;

    bool sawNewline = false;
    size_t gotlength;
    while ((gotlength = js_fgets(buf + buflength, bufsize - buflength, from)) > 0) {
        buflength += gotlength;

        /* Are we done? */
        if (buf[buflength - 1] == '\n') {
            buf[buflength - 1] = '\0';
            sawNewline = true;
            break;
        } else if (buflength < bufsize - 1) {
            break;
        }

        /* Else, grow our buffer for another pass. */
        char *tmp;
        bufsize *= 2;
        if (bufsize > buflength) {
            tmp = (char *) JS_realloc(cx, buf, bufsize);
        } else {
            JS_ReportOutOfMemory(cx);
            tmp = nullptr;
        }

        if (!tmp) {
            JS_free(cx, buf);
            return false;
        }

        buf = tmp;
    }

    /* Treat the empty string specially. */
    if (buflength == 0) {
        args.rval().set(feof(from) ? NullValue() : JS_GetEmptyStringValue(cx));
        JS_free(cx, buf);
        return true;
    }

    /* Shrink the buffer to the real size. */
    char *tmp = static_cast<char*>(JS_realloc(cx, buf, buflength));
    if (!tmp) {
        JS_free(cx, buf);
        return false;
    }

    buf = tmp;

    /*
     * Turn buf into a JSString. Note that buflength includes the trailing null
     * character.
     */
    JSString *str = JS_NewStringCopyN(cx, buf, sawNewline ? buflength - 1 : buflength);
    JS_free(cx, buf);
    if (!str)
        return false;

    args.rval().setString(str);
    return true;
}

static bool
PutStr(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (args.length() != 0) {
        JSString *str = JS::ToString(cx, args[0]);
        if (!str)
            return false;
        char *bytes = JSStringToUTF8(cx, str);
        if (!bytes)
            return false;
        fputs(bytes, gOutFile);
        JS_free(cx, bytes);
        fflush(gOutFile);
    }

    args.rval().setUndefined();
    return true;
}

static bool
Now(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    double now = PRMJ_Now() / double(PRMJ_USEC_PER_MSEC);
    args.rval().setDouble(now);
    return true;
}

static bool
PrintInternal(JSContext *cx, const CallArgs &args, FILE *file)
{
    for (unsigned i = 0; i < args.length(); i++) {
        JSString *str = JS::ToString(cx, args[i]);
        if (!str)
            return false;
        char *bytes = JSStringToUTF8(cx, str);
        if (!bytes)
            return false;
        fprintf(file, "%s%s", i ? " " : "", bytes);
#if JS_TRACE_LOGGING
        TraceLog(TraceLogging::defaultLogger(), bytes);
#endif
        JS_free(cx, bytes);
    }

    fputc('\n', file);
    fflush(file);

    args.rval().setUndefined();
    return true;
}

static bool
Print(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return PrintInternal(cx, args, gOutFile);
}

static bool
PrintErr(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return PrintInternal(cx, args, gErrFile);
}

static bool
Help(JSContext *cx, unsigned argc, jsval *vp);

static bool
Quit(JSContext *cx, unsigned argc, jsval *vp)
{
#ifdef JS_MORE_DETERMINISTIC
    // Print a message to stderr in more-deterministic builds to help jsfunfuzz
    // find uncatchable-exception bugs.
    fprintf(stderr, "quit called\n");
#endif

    CallArgs args = CallArgsFromVp(argc, vp);
    JS_ConvertArguments(cx, args.length(), args.array(), "/ i", &gExitCode);

    gQuitting = true;
    return false;
}

static const char *
ToSource(JSContext *cx, MutableHandleValue vp, JSAutoByteString *bytes)
{
    JSString *str = JS_ValueToSource(cx, vp);
    if (str) {
        vp.setString(str);
        if (bytes->encodeLatin1(cx, str))
            return bytes->ptr();
    }
    JS_ClearPendingException(cx);
    return "<<error converting value to string>>";
}

static bool
AssertEq(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    if (!(args.length() == 2 || (args.length() == 3 && args[2].isString()))) {
        JS_ReportErrorNumber(cx, my_GetErrorMessage, nullptr,
                             (args.length() < 2)
                             ? JSSMSG_NOT_ENOUGH_ARGS
                             : (args.length() == 3)
                             ? JSSMSG_INVALID_ARGS
                             : JSSMSG_TOO_MANY_ARGS,
                             "assertEq");
        return false;
    }

    bool same;
    if (!JS_SameValue(cx, args[0], args[1], &same))
        return false;
    if (!same) {
        JSAutoByteString bytes0, bytes1;
        const char *actual = ToSource(cx, args[0], &bytes0);
        const char *expected = ToSource(cx, args[1], &bytes1);
        if (args.length() == 2) {
            JS_ReportErrorNumber(cx, my_GetErrorMessage, nullptr, JSSMSG_ASSERT_EQ_FAILED,
                                 actual, expected);
        } else {
            JSAutoByteString bytes2(cx, args[2].toString());
            if (!bytes2)
                return false;
            JS_ReportErrorNumber(cx, my_GetErrorMessage, nullptr, JSSMSG_ASSERT_EQ_FAILED_MSG,
                                 actual, expected, bytes2.ptr());
        }
        return false;
    }
    args.rval().setUndefined();
    return true;
}

static JSScript *
ValueToScript(JSContext *cx, jsval vArg, JSFunction **funp = nullptr)
{
    RootedValue v(cx, vArg);
    RootedFunction fun(cx, JS_ValueToFunction(cx, v));
    if (!fun)
        return nullptr;

    // Unwrap bound functions.
    while (fun->isBoundFunction()) {
        JSObject *target = fun->getBoundFunctionTarget();
        if (target && target->is<JSFunction>())
            fun = &target->as<JSFunction>();
        else
            break;
    }

    if (!fun->isInterpreted()) {
        JS_ReportErrorNumber(cx, my_GetErrorMessage, nullptr, JSSMSG_SCRIPTS_ONLY);
        return nullptr;
    }

    JSScript *script = fun->getOrCreateScript(cx);
    if (!script)
        return nullptr;

    if (fun && funp)
        *funp = fun;

    return script;
}

static bool
SetDebug(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    if (args.length() == 0 || !args[0].isBoolean()) {
        JS_ReportErrorNumber(cx, my_GetErrorMessage, nullptr,
                             JSSMSG_NOT_ENOUGH_ARGS, "setDebug");
        return false;
    }

    /*
     * Debug mode can only be set when there is no JS code executing on the
     * stack. Unfortunately, that currently means that this call will fail
     * unless debug mode is already set to what you're trying to set it to.
     * In the future, this restriction may be lifted.
     */

    bool ok = !!JS_SetDebugMode(cx, args[0].toBoolean());
    if (ok)
        args.rval().setBoolean(true);
    return ok;
}

static bool
GetScriptAndPCArgs(JSContext *cx, unsigned argc, jsval *argv, MutableHandleScript scriptp,
                   int32_t *ip)
{
    RootedScript script(cx, GetTopScript(cx));
    *ip = 0;
    if (argc != 0) {
        jsval v = argv[0];
        unsigned intarg = 0;
        if (!JSVAL_IS_PRIMITIVE(v) &&
            JS_GetClass(&v.toObject()) == Jsvalify(&JSFunction::class_)) {
            script = ValueToScript(cx, v);
            if (!script)
                return false;
            intarg++;
        }
        if (argc > intarg) {
            if (!JS::ToInt32(cx, HandleValue::fromMarkedLocation(&argv[intarg]), ip))
                return false;
            if ((uint32_t)*ip >= script->length()) {
                JS_ReportError(cx, "Invalid PC");
                return false;
            }
        }
    }

    scriptp.set(script);

    return true;
}

static JSTrapStatus
TrapHandler(JSContext *cx, JSScript *, jsbytecode *pc, jsval *rvalArg,
            jsval closure)
{
    RootedString str(cx, JSVAL_TO_STRING(closure));
    RootedValue rval(cx, *rvalArg);

    ScriptFrameIter iter(cx);
    JS_ASSERT(!iter.done());

    /* Debug-mode currently disables Ion compilation. */
    JSAbstractFramePtr frame(iter.abstractFramePtr().raw(), iter.pc());
    RootedScript script(cx, iter.script());

    size_t length;
    const jschar *chars = JS_GetStringCharsAndLength(cx, str, &length);
    if (!chars)
        return JSTRAP_ERROR;

    if (!frame.evaluateUCInStackFrame(cx, chars, length,
                                      script->filename(),
                                      script->lineno(),
                                      &rval))
    {
        *rvalArg = rval;
        return JSTRAP_ERROR;
    }
    *rvalArg = rval;
    if (!rval.isUndefined())
        return JSTRAP_RETURN;
    return JSTRAP_CONTINUE;
}

static bool
Trap(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    RootedScript script(cx);
    int32_t i;

    if (args.length() == 0) {
        JS_ReportErrorNumber(cx, my_GetErrorMessage, nullptr, JSSMSG_TRAP_USAGE);
        return false;
    }
    argc = args.length() - 1;
    RootedString str(cx, JS::ToString(cx, args[argc]));
    if (!str)
        return false;
    args[argc].setString(str);
    if (!GetScriptAndPCArgs(cx, argc, args.array(), &script, &i))
        return false;
    args.rval().setUndefined();
    return JS_SetTrap(cx, script, script->offsetToPC(i), TrapHandler, STRING_TO_JSVAL(str));
}

static bool
Untrap(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    RootedScript script(cx);
    int32_t i;

    if (!GetScriptAndPCArgs(cx, args.length(), args.array(), &script, &i))
        return false;
    JS_ClearTrap(cx, script, script->offsetToPC(i), nullptr, nullptr);
    args.rval().setUndefined();
    return true;
}

static JSTrapStatus
DebuggerAndThrowHandler(JSContext *cx, JSScript *script, jsbytecode *pc, jsval *rval,
                        void *closure)
{
    return TrapHandler(cx, script, pc, rval, STRING_TO_JSVAL((JSString *)closure));
}

static bool
SetDebuggerHandler(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    if (args.length() == 0) {
        JS_ReportErrorNumber(cx, my_GetErrorMessage, nullptr,
                             JSSMSG_NOT_ENOUGH_ARGS, "setDebuggerHandler");
        return false;
    }

    JSString *str = JS::ToString(cx, args[0]);
    if (!str)
        return false;

    JS_SetDebuggerHandler(cx->runtime(), DebuggerAndThrowHandler, str);
    args.rval().setUndefined();
    return true;
}

static bool
SetThrowHook(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    JSString *str;
    if (args.length() == 0) {
        JS_ReportErrorNumber(cx, my_GetErrorMessage, nullptr,
                             JSSMSG_NOT_ENOUGH_ARGS, "setThrowHook");
        return false;
    }

    str = JS::ToString(cx, args[0]);
    if (!str)
        return false;

    JS_SetThrowHook(cx->runtime(), DebuggerAndThrowHandler, str);
    args.rval().setUndefined();
    return true;
}

static bool
LineToPC(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (args.length() == 0) {
        JS_ReportErrorNumber(cx, my_GetErrorMessage, nullptr, JSSMSG_LINE2PC_USAGE);
        return false;
    }

    RootedScript script(cx, GetTopScript(cx));
    int32_t lineArg = 0;
    if (args[0].isObject() && args[0].toObject().is<JSFunction>()) {
        script = ValueToScript(cx, args[0]);
        if (!script)
            return false;
        lineArg++;
    }

    uint32_t lineno;
    if (!ToUint32(cx, args.get(lineArg), &lineno))
         return false;

    jsbytecode *pc = JS_LineNumberToPC(cx, script, lineno);
    if (!pc)
        return false;
    args.rval().setInt32(script->pcToOffset(pc));
    return true;
}

static bool
PCToLine(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    RootedScript script(cx);
    int32_t i;
    unsigned lineno;

    if (!GetScriptAndPCArgs(cx, args.length(), args.array(), &script, &i))
        return false;
    lineno = JS_PCToLineNumber(cx, script, script->offsetToPC(i));
    if (!lineno)
        return false;
    args.rval().setInt32(lineno);
    return true;
}

#ifdef DEBUG

static void
UpdateSwitchTableBounds(JSContext *cx, HandleScript script, unsigned offset,
                        unsigned *start, unsigned *end)
{
    jsbytecode *pc;
    JSOp op;
    ptrdiff_t jmplen;
    int32_t low, high, n;

    pc = script->offsetToPC(offset);
    op = JSOp(*pc);
    switch (op) {
      case JSOP_TABLESWITCH:
        jmplen = JUMP_OFFSET_LEN;
        pc += jmplen;
        low = GET_JUMP_OFFSET(pc);
        pc += JUMP_OFFSET_LEN;
        high = GET_JUMP_OFFSET(pc);
        pc += JUMP_OFFSET_LEN;
        n = high - low + 1;
        break;

      default:
        /* [condswitch] switch does not have any jump or lookup tables. */
        JS_ASSERT(op == JSOP_CONDSWITCH);
        return;
    }

    *start = script->pcToOffset(pc);
    *end = *start + (unsigned)(n * jmplen);
}

static void
SrcNotes(JSContext *cx, HandleScript script, Sprinter *sp)
{
    Sprint(sp, "\nSource notes:\n");
    Sprint(sp, "%4s %4s %5s %6s %-8s %s\n",
           "ofs", "line", "pc", "delta", "desc", "args");
    Sprint(sp, "---- ---- ----- ------ -------- ------\n");
    unsigned offset = 0;
    unsigned colspan = 0;
    unsigned lineno = script->lineno();
    jssrcnote *notes = script->notes();
    unsigned switchTableEnd = 0, switchTableStart = 0;
    for (jssrcnote *sn = notes; !SN_IS_TERMINATOR(sn); sn = SN_NEXT(sn)) {
        unsigned delta = SN_DELTA(sn);
        offset += delta;
        SrcNoteType type = (SrcNoteType) SN_TYPE(sn);
        const char *name = js_SrcNoteSpec[type].name;
        Sprint(sp, "%3u: %4u %5u [%4u] %-8s", unsigned(sn - notes), lineno, offset, delta, name);
        switch (type) {
          case SRC_NULL:
          case SRC_IF:
          case SRC_CONTINUE:
          case SRC_BREAK:
          case SRC_BREAK2LABEL:
          case SRC_SWITCHBREAK:
          case SRC_ASSIGNOP:
          case SRC_XDELTA:
            break;

          case SRC_COLSPAN:
            colspan = js_GetSrcNoteOffset(sn, 0);
            if (colspan >= SN_COLSPAN_DOMAIN / 2)
                colspan -= SN_COLSPAN_DOMAIN;
            Sprint(sp, "%d", colspan);
            break;

          case SRC_SETLINE:
            lineno = js_GetSrcNoteOffset(sn, 0);
            Sprint(sp, " lineno %u", lineno);
            break;

          case SRC_NEWLINE:
            ++lineno;
            break;

          case SRC_FOR:
            Sprint(sp, " cond %u update %u tail %u",
                   unsigned(js_GetSrcNoteOffset(sn, 0)),
                   unsigned(js_GetSrcNoteOffset(sn, 1)),
                   unsigned(js_GetSrcNoteOffset(sn, 2)));
            break;

          case SRC_IF_ELSE:
            Sprint(sp, " else %u", unsigned(js_GetSrcNoteOffset(sn, 0)));
            break;

          case SRC_FOR_IN:
          case SRC_FOR_OF:
            Sprint(sp, " closingjump %u", unsigned(js_GetSrcNoteOffset(sn, 0)));
            break;

          case SRC_COND:
          case SRC_WHILE:
          case SRC_NEXTCASE:
            Sprint(sp, " offset %u", unsigned(js_GetSrcNoteOffset(sn, 0)));
            break;

          case SRC_TABLESWITCH: {
            JSOp op = JSOp(script->code()[offset]);
            JS_ASSERT(op == JSOP_TABLESWITCH);
            Sprint(sp, " length %u", unsigned(js_GetSrcNoteOffset(sn, 0)));
            UpdateSwitchTableBounds(cx, script, offset,
                                    &switchTableStart, &switchTableEnd);
            break;
          }
          case SRC_CONDSWITCH: {
            JSOp op = JSOp(script->code()[offset]);
            JS_ASSERT(op == JSOP_CONDSWITCH);
            Sprint(sp, " length %u", unsigned(js_GetSrcNoteOffset(sn, 0)));
            unsigned caseOff = (unsigned) js_GetSrcNoteOffset(sn, 1);
            if (caseOff)
                Sprint(sp, " first case offset %u", caseOff);
            UpdateSwitchTableBounds(cx, script, offset,
                                    &switchTableStart, &switchTableEnd);
            break;
          }

          case SRC_TRY:
            JS_ASSERT(JSOp(script->code()[offset]) == JSOP_TRY);
            Sprint(sp, " offset to jump %u", unsigned(js_GetSrcNoteOffset(sn, 0)));
            break;

          default:
            JS_ASSERT(0);
            break;
        }
        Sprint(sp, "\n");
    }
}

static bool
Notes(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    Sprinter sprinter(cx);
    if (!sprinter.init())
        return false;

    for (unsigned i = 0; i < args.length(); i++) {
        RootedScript script (cx, ValueToScript(cx, args[i]));
        if (!script)
            return false;

        SrcNotes(cx, script, &sprinter);
    }

    JSString *str = JS_NewStringCopyZ(cx, sprinter.string());
    if (!str)
        return false;
    args.rval().setString(str);
    return true;
}

JS_STATIC_ASSERT(JSTRY_CATCH == 0);
JS_STATIC_ASSERT(JSTRY_FINALLY == 1);
JS_STATIC_ASSERT(JSTRY_ITER == 2);

static const char* const TryNoteNames[] = { "catch", "finally", "iter", "loop" };

static bool
TryNotes(JSContext *cx, HandleScript script, Sprinter *sp)
{
    JSTryNote *tn, *tnlimit;

    if (!script->hasTrynotes())
        return true;

    tn = script->trynotes()->vector;
    tnlimit = tn + script->trynotes()->length;
    Sprint(sp, "\nException table:\nkind      stack    start      end\n");
    do {
        JS_ASSERT(tn->kind < ArrayLength(TryNoteNames));
        Sprint(sp, " %-7s %6u %8u %8u\n",
               TryNoteNames[tn->kind], tn->stackDepth,
               tn->start, tn->start + tn->length);
    } while (++tn != tnlimit);
    return true;
}

static bool
DisassembleScript(JSContext *cx, HandleScript script, HandleFunction fun, bool lines,
                  bool recursive, Sprinter *sp)
{
    if (fun) {
        Sprint(sp, "flags:");
        if (fun->isLambda())
            Sprint(sp, " LAMBDA");
        if (fun->isHeavyweight())
            Sprint(sp, " HEAVYWEIGHT");
        if (fun->isExprClosure())
            Sprint(sp, " EXPRESSION_CLOSURE");
        if (fun->isFunctionPrototype())
            Sprint(sp, " Function.prototype");
        if (fun->isSelfHostedBuiltin())
            Sprint(sp, " SELF_HOSTED");
        if (fun->isSelfHostedConstructor())
            Sprint(sp, " SELF_HOSTED_CTOR");
        if (fun->isArrow())
            Sprint(sp, " ARROW");
        Sprint(sp, "\n");
    }

    if (!js_Disassemble(cx, script, lines, sp))
        return false;
    SrcNotes(cx, script, sp);
    TryNotes(cx, script, sp);

    if (recursive && script->hasObjects()) {
        ObjectArray *objects = script->objects();
        for (unsigned i = 0; i != objects->length; ++i) {
            JSObject *obj = objects->vector[i];
            if (obj->is<JSFunction>()) {
                Sprint(sp, "\n");
                RootedFunction fun(cx, &obj->as<JSFunction>());
                if (fun->isInterpreted()) {
                    RootedScript script(cx, fun->getOrCreateScript(cx));
                    if (!script || !DisassembleScript(cx, script, fun, lines, recursive, sp))
                        return false;
                } else {
                    Sprint(sp, "[native code]\n");
                }
            }
        }
    }
    return true;
}

namespace {

struct DisassembleOptionParser {
    unsigned   argc;
    jsval   *argv;
    bool    lines;
    bool    recursive;

    DisassembleOptionParser(unsigned argc, jsval *argv)
      : argc(argc), argv(argv), lines(false), recursive(false) {}

    bool parse(JSContext *cx) {
        /* Read options off early arguments */
        while (argc > 0 && argv[0].isString()) {
            JSString *str = argv[0].toString();
            JSFlatString *flatStr = JS_FlattenString(cx, str);
            if (!flatStr)
                return false;
            if (JS_FlatStringEqualsAscii(flatStr, "-l"))
                lines = true;
            else if (JS_FlatStringEqualsAscii(flatStr, "-r"))
                recursive = true;
            else
                break;
            argv++, argc--;
        }
        return true;
    }
};

} /* anonymous namespace */

static bool
DisassembleToSprinter(JSContext *cx, unsigned argc, jsval *vp, Sprinter *sprinter)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    DisassembleOptionParser p(args.length(), args.array());
    if (!p.parse(cx))
        return false;

    if (p.argc == 0) {
        /* Without arguments, disassemble the current script. */
        RootedScript script(cx, GetTopScript(cx));
        if (script) {
            JSAutoCompartment ac(cx, script);
            if (!js_Disassemble(cx, script, p.lines, sprinter))
                return false;
            SrcNotes(cx, script, sprinter);
            TryNotes(cx, script, sprinter);
        }
    } else {
        for (unsigned i = 0; i < p.argc; i++) {
            RootedFunction fun(cx);
            RootedScript script (cx, ValueToScript(cx, p.argv[i], fun.address()));
            if (!script)
                return false;
            if (!DisassembleScript(cx, script, fun, p.lines, p.recursive, sprinter))
                return false;
        }
    }
    return true;
}

static bool
DisassembleToString(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    Sprinter sprinter(cx);
    if (!sprinter.init())
        return false;
    if (!DisassembleToSprinter(cx, args.length(), vp, &sprinter))
        return false;

    JSString *str = JS_NewStringCopyZ(cx, sprinter.string());
    if (!str)
        return false;
    args.rval().setString(str);
    return true;
}

static bool
Disassemble(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    Sprinter sprinter(cx);
    if (!sprinter.init())
        return false;
    if (!DisassembleToSprinter(cx, args.length(), vp, &sprinter))
        return false;

    fprintf(stdout, "%s\n", sprinter.string());
    args.rval().setUndefined();
    return true;
}

static bool
DisassFile(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    /* Support extra options at the start, just like Disassemble. */
    DisassembleOptionParser p(args.length(), args.array());
    if (!p.parse(cx))
        return false;

    if (!p.argc) {
        args.rval().setUndefined();
        return true;
    }

    RootedObject thisobj(cx, JS_THIS_OBJECT(cx, vp));
    if (!thisobj)
        return false;

    // We should change DisassembleOptionParser to store CallArgs.
    JSString *str = JS::ToString(cx, HandleValue::fromMarkedLocation(&p.argv[0]));
    if (!str)
        return false;
    JSAutoByteString filename(cx, str);
    if (!filename)
        return false;
    RootedScript script(cx);

    {
        JS::AutoSaveContextOptions asco(cx);
        JS::ContextOptionsRef(cx).setNoScriptRval(true);

        CompileOptions options(cx);
        options.setUTF8(true)
               .setFileAndLine(filename.ptr(), 1)
               .setCompileAndGo(true);

        script = JS::Compile(cx, thisobj, options, filename.ptr());
        if (!script)
            return false;
    }

    Sprinter sprinter(cx);
    if (!sprinter.init())
        return false;
    bool ok = DisassembleScript(cx, script, NullPtr(), p.lines, p.recursive, &sprinter);
    if (ok)
        fprintf(stdout, "%s\n", sprinter.string());
    if (!ok)
        return false;

    args.rval().setUndefined();
    return true;
}

static bool
DisassWithSrc(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

#define LINE_BUF_LEN 512
    unsigned len, line1, line2, bupline;
    FILE *file;
    char linebuf[LINE_BUF_LEN];
    jsbytecode *pc, *end;
    static const char sep[] = ";-------------------------";

    bool ok = true;
    RootedScript script(cx);
    for (unsigned i = 0; ok && i < args.length(); i++) {
        script = ValueToScript(cx, args[i]);
        if (!script)
           return false;

        if (!script->filename()) {
            JS_ReportErrorNumber(cx, my_GetErrorMessage, nullptr,
                                 JSSMSG_FILE_SCRIPTS_ONLY);
            return false;
        }

        file = fopen(script->filename(), "r");
        if (!file) {
            JS_ReportErrorNumber(cx, my_GetErrorMessage, nullptr,
                                 JSSMSG_CANT_OPEN, script->filename(),
                                 strerror(errno));
            return false;
        }

        pc = script->code();
        end = script->codeEnd();

        Sprinter sprinter(cx);
        if (!sprinter.init()) {
            ok = false;
            goto bail;
        }

        /* burn the leading lines */
        line2 = JS_PCToLineNumber(cx, script, pc);
        for (line1 = 0; line1 < line2 - 1; line1++) {
            char *tmp = fgets(linebuf, LINE_BUF_LEN, file);
            if (!tmp) {
                JS_ReportError(cx, "failed to read %s fully", script->filename());
                ok = false;
                goto bail;
            }
        }

        bupline = 0;
        while (pc < end) {
            line2 = JS_PCToLineNumber(cx, script, pc);

            if (line2 < line1) {
                if (bupline != line2) {
                    bupline = line2;
                    Sprint(&sprinter, "%s %3u: BACKUP\n", sep, line2);
                }
            } else {
                if (bupline && line1 == line2)
                    Sprint(&sprinter, "%s %3u: RESTORE\n", sep, line2);
                bupline = 0;
                while (line1 < line2) {
                    if (!fgets(linebuf, LINE_BUF_LEN, file)) {
                        JS_ReportErrorNumber(cx, my_GetErrorMessage, nullptr,
                                             JSSMSG_UNEXPECTED_EOF,
                                             script->filename());
                        ok = false;
                        goto bail;
                    }
                    line1++;
                    Sprint(&sprinter, "%s %3u: %s", sep, line1, linebuf);
                }
            }

            len = js_Disassemble1(cx, script, pc, script->pcToOffset(pc), true, &sprinter);
            if (!len) {
                ok = false;
                goto bail;
            }
            pc += len;
        }

      bail:
        fclose(file);
    }
    args.rval().setUndefined();
    return ok;
#undef LINE_BUF_LEN
}

static bool
DumpHeap(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    JSAutoByteString fileName;
    if (args.hasDefined(0)) {
        RootedString str(cx, JS::ToString(cx, args[0]));
        if (!str)
            return false;

        if (!fileName.encodeLatin1(cx, str))
            return false;
    }

    RootedValue startThing(cx);
    if (args.hasDefined(1)) {
        if (!args[1].isGCThing()) {
            JS_ReportError(cx, "dumpHeap: Second argument not a GC thing!");
            return false;
        }
        startThing = args[1];
    }

    RootedValue thingToFind(cx);
    if (args.hasDefined(2)) {
        if (!args[2].isGCThing()) {
            JS_ReportError(cx, "dumpHeap: Third argument not a GC thing!");
            return false;
        }
        thingToFind = args[2];
    }

    size_t maxDepth = size_t(-1);
    if (args.hasDefined(3)) {
        uint32_t depth;
        if (!ToUint32(cx, args[3], &depth))
            return false;
        maxDepth = depth;
    }

    RootedValue thingToIgnore(cx);
    if (args.hasDefined(4)) {
        if (!args[2].isGCThing()) {
            JS_ReportError(cx, "dumpHeap: Fifth argument not a GC thing!");
            return false;
        }
        thingToIgnore = args[4];
    }


    FILE *dumpFile = stdout;
    if (fileName.length()) {
        dumpFile = fopen(fileName.ptr(), "w");
        if (!dumpFile) {
            JS_ReportError(cx, "dumpHeap: can't open %s: %s\n",
                          fileName.ptr(), strerror(errno));
            return false;
        }
    }

    bool ok = JS_DumpHeap(JS_GetRuntime(cx), dumpFile,
                          startThing.isUndefined() ? nullptr : startThing.toGCThing(),
                          startThing.isUndefined() ? JSTRACE_OBJECT : startThing.get().gcKind(),
                          thingToFind.isUndefined() ? nullptr : thingToFind.toGCThing(),
                          maxDepth,
                          thingToIgnore.isUndefined() ? nullptr : thingToIgnore.toGCThing());

    if (dumpFile != stdout)
        fclose(dumpFile);

    if (!ok)
        JS_ReportOutOfMemory(cx);

    return ok;
}

static bool
DumpObject(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    RootedObject arg0(cx);
    if (!JS_ConvertArguments(cx, args.length(), args.array(), "o", arg0.address()))
        return false;

    js_DumpObject(arg0);

    args.rval().setUndefined();
    return true;
}

#endif /* DEBUG */

static bool
BuildDate(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    fprintf(gOutFile, "built on %s at %s\n", __DATE__, __TIME__);
    args.rval().setUndefined();
    return true;
}

static bool
Intern(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    JSString *str = JS::ToString(cx, args.get(0));
    if (!str)
        return false;

    size_t length;
    const jschar *chars = JS_GetStringCharsAndLength(cx, str, &length);
    if (!chars)
        return false;

    if (!JS_InternUCStringN(cx, chars, length))
        return false;

    args.rval().setUndefined();
    return true;
}

static bool
Clone(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    RootedObject parent(cx);
    RootedObject funobj(cx);

    if (!args.length()) {
        JS_ReportError(cx, "Invalid arguments to clone");
        return false;
    }

    {
        Maybe<JSAutoCompartment> ac;
        RootedObject obj(cx, JSVAL_IS_PRIMITIVE(args[0]) ? nullptr : &args[0].toObject());

        if (obj && obj->is<CrossCompartmentWrapperObject>()) {
            obj = UncheckedUnwrap(obj);
            ac.construct(cx, obj);
            args[0].setObject(*obj);
        }
        if (obj && obj->is<JSFunction>()) {
            funobj = obj;
        } else {
            JSFunction *fun = JS_ValueToFunction(cx, args[0]);
            if (!fun)
                return false;
            funobj = JS_GetFunctionObject(fun);
        }
    }
    if (funobj->compartment() != cx->compartment()) {
        JSFunction *fun = &funobj->as<JSFunction>();
        if (fun->hasScript() && fun->nonLazyScript()->compileAndGo()) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_UNEXPECTED_TYPE,
                                 "function", "compile-and-go");
            return false;
        }
    }

    if (argc > 1) {
        if (!JS_ValueToObject(cx, args[1], &parent))
            return false;
    } else {
        parent = JS_GetParent(JSVAL_TO_OBJECT(JS_CALLEE(cx, vp)));
    }

    JSObject *clone = JS_CloneFunctionObject(cx, funobj, parent);
    if (!clone)
        return false;
    JS_SET_RVAL(cx, vp, OBJECT_TO_JSVAL(clone));
    return true;
}

static bool
GetPDA(JSContext *cx, unsigned argc, jsval *vp)
{
    RootedObject vobj(cx);
    bool ok;
    JSPropertyDescArray pda;
    JSPropertyDesc *pd;

    CallArgs args = CallArgsFromVp(argc, vp);
    if (!JS_ValueToObject(cx, args.get(0), &vobj))
        return false;
    if (!vobj) {
        args.rval().setUndefined();
        return true;
    }

    RootedObject aobj(cx, JS_NewArrayObject(cx, 0, nullptr));
    if (!aobj)
        return false;
    args.rval().setObject(*aobj);

    ok = !!JS_GetPropertyDescArray(cx, vobj, &pda);
    if (!ok)
        return false;
    pd = pda.array;

    RootedObject pdobj(cx);
    RootedValue id(cx);
    RootedValue value(cx);
    RootedValue flags(cx);
    RootedValue alias(cx);

    for (uint32_t i = 0; i < pda.length; i++, pd++) {
        pdobj = JS_NewObject(cx, nullptr, JS::NullPtr(), JS::NullPtr());
        if (!pdobj) {
            ok = false;
            break;
        }

        /* Protect pdobj from GC by setting it as an element of aobj now */
        RootedValue v(cx);
        v.setObject(*pdobj);
        ok = !!JS_SetElement(cx, aobj, i, &v);
        if (!ok)
            break;

        id = pd->id;
        value = pd->value;
        flags.setInt32(pd->flags);
        alias = pd->alias;
        ok = JS_SetProperty(cx, pdobj, "id", id) &&
             JS_SetProperty(cx, pdobj, "value", value) &&
             JS_SetProperty(cx, pdobj, "flags", flags) &&
             JS_SetProperty(cx, pdobj, "alias", alias);
        if (!ok)
            break;
    }
    JS_PutPropertyDescArray(cx, &pda);
    return ok;
}

static bool
GetSLX(JSContext *cx, unsigned argc, jsval *vp)
{
    RootedScript script(cx);

    script = ValueToScript(cx, argc == 0 ? UndefinedValue() : vp[2]);
    if (!script)
        return false;
    JS_SET_RVAL(cx, vp, INT_TO_JSVAL(js_GetScriptLineExtent(script)));
    return true;
}

static bool
ThrowError(JSContext *cx, unsigned argc, jsval *vp)
{
    JS_ReportError(cx, "This is an error");
    return false;
}

#define LAZY_STANDARD_CLASSES

/* A class for easily testing the inner/outer object callbacks. */
typedef struct ComplexObject {
    bool isInner;
    bool frozen;
    JSObject *inner;
    JSObject *outer;
} ComplexObject;

static bool
sandbox_enumerate(JSContext *cx, HandleObject obj)
{
    RootedValue v(cx);

    if (!JS_GetProperty(cx, obj, "lazy", &v))
        return false;

    if (!ToBoolean(v))
        return true;

    return JS_EnumerateStandardClasses(cx, obj);
}

static bool
sandbox_resolve(JSContext *cx, HandleObject obj, HandleId id, unsigned flags,
                MutableHandleObject objp)
{
    RootedValue v(cx);
    if (!JS_GetProperty(cx, obj, "lazy", &v))
        return false;

    if (ToBoolean(v)) {
        bool resolved;
        if (!JS_ResolveStandardClass(cx, obj, id, &resolved))
            return false;
        if (resolved) {
            objp.set(obj);
            return true;
        }
    }
    objp.set(nullptr);
    return true;
}

static const JSClass sandbox_class = {
    "sandbox",
    JSCLASS_NEW_RESOLVE | JSCLASS_GLOBAL_FLAGS,
    JS_PropertyStub,   JS_DeletePropertyStub,
    JS_PropertyStub,   JS_StrictPropertyStub,
    sandbox_enumerate, (JSResolveOp)sandbox_resolve,
    JS_ConvertStub
};

static JSObject *
NewSandbox(JSContext *cx, bool lazy)
{
    RootedObject obj(cx, JS_NewGlobalObject(cx, &sandbox_class, nullptr,
                                            JS::DontFireOnNewGlobalHook));
    if (!obj)
        return nullptr;

    {
        JSAutoCompartment ac(cx, obj);
        if (!lazy && !JS_InitStandardClasses(cx, obj))
            return nullptr;

        RootedValue value(cx, BooleanValue(lazy));
        if (!JS_SetProperty(cx, obj, "lazy", value))
            return nullptr;
    }

    JS_FireOnNewGlobalObject(cx, obj);

    if (!cx->compartment()->wrap(cx, &obj))
        return nullptr;
    return obj;
}

static bool
EvalInContext(JSContext *cx, unsigned argc, jsval *vp)
{
    RootedString str(cx);
    RootedObject sobj(cx);
    if (!JS_ConvertArguments(cx, argc, JS_ARGV(cx, vp), "S / o", str.address(), sobj.address()))
        return false;

    size_t srclen;
    const jschar *src = JS_GetStringCharsAndLength(cx, str, &srclen);
    if (!src)
        return false;

    SkipRoot skip(cx, &src);

    bool lazy = false;
    if (srclen == 4) {
        if (src[0] == 'l' && src[1] == 'a' && src[2] == 'z' && src[3] == 'y') {
            lazy = true;
            srclen = 0;
        }
    }

    if (!sobj) {
        sobj = NewSandbox(cx, lazy);
        if (!sobj)
            return false;
    }

    if (srclen == 0) {
        JS_SET_RVAL(cx, vp, OBJECT_TO_JSVAL(sobj));
        return true;
    }

    RootedScript script(cx);
    unsigned lineno;

    JS_DescribeScriptedCaller(cx, &script, &lineno);
    RootedValue rval(cx);
    {
        Maybe<JSAutoCompartment> ac;
        unsigned flags;
        JSObject *unwrapped = UncheckedUnwrap(sobj, true, &flags);
        if (flags & Wrapper::CROSS_COMPARTMENT) {
            sobj = unwrapped;
            ac.construct(cx, sobj);
        }

        sobj = GetInnerObject(cx, sobj);
        if (!sobj)
            return false;
        if (!(sobj->getClass()->flags & JSCLASS_IS_GLOBAL)) {
            JS_ReportError(cx, "Invalid scope argument to evalcx");
            return false;
        }
        if (!JS_EvaluateUCScript(cx, sobj, src, srclen,
                                 script->filename(),
                                 lineno,
                                 rval.address())) {
            return false;
        }
    }

    if (!cx->compartment()->wrap(cx, &rval))
        return false;

    JS_SET_RVAL(cx, vp, rval);
    return true;
}

static bool
EvalInFrame(JSContext *cx, unsigned argc, jsval *vp)
{
    jsval *argv = JS_ARGV(cx, vp);
    if (argc < 2 ||
        !JSVAL_IS_INT(argv[0]) ||
        !JSVAL_IS_STRING(argv[1])) {
        JS_ReportError(cx, "Invalid arguments to evalInFrame");
        return false;
    }

    uint32_t upCount = JSVAL_TO_INT(argv[0]);
    RootedString str(cx, JSVAL_TO_STRING(argv[1]));

    bool saveCurrent = (argc >= 3 && JSVAL_IS_BOOLEAN(argv[2]))
                        ? !!(JSVAL_TO_BOOLEAN(argv[2]))
                        : false;

    /* This is a copy of CheckDebugMode. */
    if (!JS_GetDebugMode(cx)) {
        JS_ReportErrorFlagsAndNumber(cx, JSREPORT_ERROR, js_GetErrorMessage,
                                     nullptr, JSMSG_NEED_DEBUG_MODE);
        return false;
    }

    /* Debug-mode currently disables Ion compilation. */
    ScriptFrameIter fi(cx);
    for (uint32_t i = 0; i < upCount; ++i, ++fi) {
        ScriptFrameIter next(fi);
        ++next;
        if (next.done())
            break;
    }

    AutoSaveFrameChain sfc(cx);
    mozilla::Maybe<AutoCompartment> ac;
    if (saveCurrent) {
        if (!sfc.save())
            return false;
        ac.construct(cx, DefaultObjectForContextOrNull(cx));
    }

    size_t length;
    const jschar *chars = JS_GetStringCharsAndLength(cx, str, &length);
    if (!chars)
        return false;

    JSAbstractFramePtr frame(fi.abstractFramePtr().raw(), fi.pc());
    RootedScript fpscript(cx, frame.script());
    bool ok = !!frame.evaluateUCInStackFrame(cx, chars, length,
                                             fpscript->filename(),
                                             JS_PCToLineNumber(cx, fpscript,
                                                               fi.pc()),
                                             MutableHandleValue::fromMarkedLocation(vp));
    return ok;
}

static bool
ShapeOf(JSContext *cx, unsigned argc, JS::Value *vp)
{
    JS::Value v;
    if (argc < 1 || !((v = JS_ARGV(cx, vp)[0]).isObject())) {
        JS_ReportError(cx, "shapeOf: object expected");
        return false;
    }
    JSObject *obj = &v.toObject();
    JS_SET_RVAL(cx, vp, JS_NumberValue((double) ((uintptr_t)obj->lastProperty() >> 3)));
    return true;
}

/*
 * If referent has an own property named id, copy that property to obj[id].
 * Since obj is native, this isn't totally transparent; properties of a
 * non-native referent may be simplified to data properties.
 */
static bool
CopyProperty(JSContext *cx, HandleObject obj, HandleObject referent, HandleId id,
             unsigned lookupFlags, MutableHandleObject objp)
{
    RootedShape shape(cx);
    Rooted<PropertyDescriptor> desc(cx);
    unsigned propFlags = 0;
    RootedObject obj2(cx);

    objp.set(nullptr);
    if (referent->isNative()) {
        if (!LookupPropertyWithFlags(cx, referent, id, lookupFlags, &obj2, &shape))
            return false;
        if (obj2 != referent)
            return true;

        if (shape->hasSlot()) {
            desc.value().set(referent->nativeGetSlot(shape->slot()));
        } else {
            desc.value().setUndefined();
        }

        desc.setAttributes(shape->attributes());
        desc.setGetter(shape->getter());
        if (!desc.getter() && !desc.hasGetterObject())
            desc.setGetter(JS_PropertyStub);
        desc.setSetter(shape->setter());
        if (!desc.setter() && !desc.hasSetterObject())
            desc.setSetter(JS_StrictPropertyStub);
        desc.setShortId(shape->shortid());
        propFlags = shape->getFlags();
    } else if (referent->is<ProxyObject>()) {
        if (!Proxy::getOwnPropertyDescriptor(cx, referent, id, &desc, 0))
            return false;
        if (!desc.object())
            return true;
    } else {
        if (!JSObject::lookupGeneric(cx, referent, id, objp, &shape))
            return false;
        if (objp != referent)
            return true;
        RootedValue value(cx);
        if (!JSObject::getGeneric(cx, referent, referent, id, &value) ||
            !JSObject::getGenericAttributes(cx, referent, id, &desc.attributesRef()))
        {
            return false;
        }
        desc.value().set(value);
        desc.attributesRef() &= JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT;
        desc.setGetter(JS_PropertyStub);
        desc.setSetter(JS_StrictPropertyStub);
        desc.setShortId(0);
    }

    objp.set(obj);
    return DefineNativeProperty(cx, obj, id, desc.value(), desc.getter(), desc.setter(),
                                desc.attributes(), propFlags, desc.shortid());
}

static bool
resolver_resolve(JSContext *cx, HandleObject obj, HandleId id, unsigned flags,
                 MutableHandleObject objp)
{
    jsval v = JS_GetReservedSlot(obj, 0);
    Rooted<JSObject*> vobj(cx, &v.toObject());
    return CopyProperty(cx, obj, vobj, id, flags, objp);
}

static bool
resolver_enumerate(JSContext *cx, HandleObject obj)
{
    jsval v = JS_GetReservedSlot(obj, 0);
    RootedObject referent(cx, JSVAL_TO_OBJECT(v));

    AutoIdArray ida(cx, JS_Enumerate(cx, referent));
    bool ok = !!ida;
    RootedObject ignore(cx);
    for (size_t i = 0; ok && i < ida.length(); i++) {
        Rooted<jsid> id(cx, ida[i]);
        ok = CopyProperty(cx, obj, referent, id, 0, &ignore);
    }
    return ok;
}

static const JSClass resolver_class = {
    "resolver",
    JSCLASS_NEW_RESOLVE | JSCLASS_HAS_RESERVED_SLOTS(1),
    JS_PropertyStub,   JS_DeletePropertyStub,
    JS_PropertyStub,   JS_StrictPropertyStub,
    resolver_enumerate, (JSResolveOp)resolver_resolve,
    JS_ConvertStub
};


static bool
Resolver(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    RootedObject referent(cx);
    if (!JS_ValueToObject(cx, args.get(0), &referent))
        return false;
    if (!referent) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_CANT_CONVERT_TO,
                             args.get(0).isNull() ? "null" : "undefined", "object");
        return false;
    }

    RootedObject proto(cx, nullptr);
    if (!args.get(1).isNullOrUndefined()) {
        if (!JS_ValueToObject(cx, args.get(1), &proto))
            return false;
    }

    RootedObject parent(cx, JS_GetParent(referent));
    JSObject *result = (argc > 1
                        ? JS_NewObjectWithGivenProto
                        : JS_NewObject)(cx, &resolver_class, proto, parent);
    if (!result)
        return false;

    JS_SetReservedSlot(result, 0, ObjectValue(*referent));
    args.rval().setObject(*result);
    return true;
}

#ifdef JS_THREADSAFE

/*
 * Check that t1 comes strictly before t2. The function correctly deals with
 * wrap-around between t2 and t1 assuming that t2 and t1 stays within INT32_MAX
 * from each other. We use MAX_TIMEOUT_INTERVAL to enforce this restriction.
 */
static bool
IsBefore(int64_t t1, int64_t t2)
{
    return int32_t(t1 - t2) < 0;
}

static bool
Sleep_fn(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    int64_t t_ticks;

    if (args.length() == 0) {
        t_ticks = 0;
    } else {
        double t_secs;

        if (!ToNumber(cx, args[0], &t_secs))
            return false;

        /* NB: The next condition also filter out NaNs. */
        if (!(t_secs <= MAX_TIMEOUT_INTERVAL)) {
            JS_ReportError(cx, "Excessive sleep interval");
            return false;
        }
        t_ticks = (t_secs <= 0.0)
                  ? 0
                  : int64_t(PRMJ_USEC_PER_SEC * t_secs);
    }
    PR_Lock(gWatchdogLock);
    int64_t to_wakeup = PRMJ_Now() + t_ticks;
    for (;;) {
        PR_WaitCondVar(gSleepWakeup, t_ticks);
        if (gTimedOut)
            break;
        int64_t now = PRMJ_Now();
        if (!IsBefore(now, to_wakeup))
            break;
        t_ticks = to_wakeup - now;
    }
    PR_Unlock(gWatchdogLock);
    return !gTimedOut;
}

static bool
InitWatchdog(JSRuntime *rt)
{
    JS_ASSERT(!gWatchdogThread);
    gWatchdogLock = PR_NewLock();
    if (gWatchdogLock) {
        gWatchdogWakeup = PR_NewCondVar(gWatchdogLock);
        if (gWatchdogWakeup) {
            gSleepWakeup = PR_NewCondVar(gWatchdogLock);
            if (gSleepWakeup)
                return true;
            PR_DestroyCondVar(gWatchdogWakeup);
        }
        PR_DestroyLock(gWatchdogLock);
    }
    return false;
}

static void
KillWatchdog()
{
    PRThread *thread;

    PR_Lock(gWatchdogLock);
    thread = gWatchdogThread;
    if (thread) {
        /*
         * The watchdog thread is running, tell it to terminate waking it up
         * if necessary.
         */
        gWatchdogThread = nullptr;
        PR_NotifyCondVar(gWatchdogWakeup);
    }
    PR_Unlock(gWatchdogLock);
    if (thread)
        PR_JoinThread(thread);
    PR_DestroyCondVar(gSleepWakeup);
    PR_DestroyCondVar(gWatchdogWakeup);
    PR_DestroyLock(gWatchdogLock);
}

static void
WatchdogMain(void *arg)
{
    PR_SetCurrentThreadName("JS Watchdog");

    JSRuntime *rt = (JSRuntime *) arg;

    PR_Lock(gWatchdogLock);
    while (gWatchdogThread) {
        int64_t now = PRMJ_Now();
        if (gWatchdogHasTimeout && !IsBefore(now, gWatchdogTimeout)) {
            /*
             * The timeout has just expired. Trigger the operation callback
             * outside the lock.
             */
            gWatchdogHasTimeout = false;
            PR_Unlock(gWatchdogLock);
            CancelExecution(rt);
            PR_Lock(gWatchdogLock);

            /* Wake up any threads doing sleep. */
            PR_NotifyAllCondVar(gSleepWakeup);
        } else {
            if (gWatchdogHasTimeout) {
                /*
                 * Time hasn't expired yet. Simulate an operation callback
                 * which doesn't abort execution.
                 */
                JS_TriggerOperationCallback(rt);
            }

            uint64_t sleepDuration = PR_INTERVAL_NO_TIMEOUT;
            if (gWatchdogHasTimeout)
                sleepDuration = PR_TicksPerSecond() / 10;
            mozilla::DebugOnly<PRStatus> status =
              PR_WaitCondVar(gWatchdogWakeup, sleepDuration);
            JS_ASSERT(status == PR_SUCCESS);
        }
    }
    PR_Unlock(gWatchdogLock);
}

static bool
ScheduleWatchdog(JSRuntime *rt, double t)
{
    if (t <= 0) {
        PR_Lock(gWatchdogLock);
        gWatchdogHasTimeout = false;
        PR_Unlock(gWatchdogLock);
        return true;
    }

    int64_t interval = int64_t(ceil(t * PRMJ_USEC_PER_SEC));
    int64_t timeout = PRMJ_Now() + interval;
    PR_Lock(gWatchdogLock);
    if (!gWatchdogThread) {
        JS_ASSERT(!gWatchdogHasTimeout);
        gWatchdogThread = PR_CreateThread(PR_USER_THREAD,
                                          WatchdogMain,
                                          rt,
                                          PR_PRIORITY_NORMAL,
                                          PR_LOCAL_THREAD,
                                          PR_JOINABLE_THREAD,
                                          0);
        if (!gWatchdogThread) {
            PR_Unlock(gWatchdogLock);
            return false;
        }
    } else if (!gWatchdogHasTimeout || IsBefore(timeout, gWatchdogTimeout)) {
         PR_NotifyCondVar(gWatchdogWakeup);
    }
    gWatchdogHasTimeout = true;
    gWatchdogTimeout = timeout;
    PR_Unlock(gWatchdogLock);
    return true;
}

#else /* !JS_THREADSAFE */

#ifdef XP_WIN
static HANDLE gTimerHandle = 0;

VOID CALLBACK
TimerCallback(PVOID lpParameter, BOOLEAN TimerOrWaitFired)
{
    CancelExecution((JSRuntime *) lpParameter);
}

#else

static void
AlarmHandler(int sig)
{
    CancelExecution(gRuntime);
}

#endif

static bool
InitWatchdog(JSRuntime *rt)
{
    gRuntime = rt;
    return true;
}

static void
KillWatchdog()
{
    ScheduleWatchdog(gRuntime, -1);
}

static bool
ScheduleWatchdog(JSRuntime *rt, double t)
{
#ifdef XP_WIN
    if (gTimerHandle) {
        DeleteTimerQueueTimer(nullptr, gTimerHandle, nullptr);
        gTimerHandle = 0;
    }
    if (t > 0 &&
        !CreateTimerQueueTimer(&gTimerHandle,
                               nullptr,
                               (WAITORTIMERCALLBACK)TimerCallback,
                               rt,
                               DWORD(ceil(t * 1000.0)),
                               0,
                               WT_EXECUTEINTIMERTHREAD | WT_EXECUTEONLYONCE)) {
        gTimerHandle = 0;
        return false;
    }
#else
    /* FIXME: use setitimer when available for sub-second resolution. */
    if (t <= 0) {
        alarm(0);
        signal(SIGALRM, nullptr);
    } else {
        signal(SIGALRM, AlarmHandler); /* set the Alarm signal capture */
        alarm(ceil(t));
    }
#endif
    return true;
}

#endif /* !JS_THREADSAFE */

static void
CancelExecution(JSRuntime *rt)
{
    gTimedOut = true;
    JS_TriggerOperationCallback(rt);

    if (!gTimeoutFunc.isNull()) {
        static const char msg[] = "Script runs for too long, terminating.\n";
#if defined(XP_UNIX) && !defined(JS_THREADSAFE)
        /* It is not safe to call fputs from signals. */
        /* Dummy assignment avoids GCC warning on "attribute warn_unused_result" */
        ssize_t dummy = write(2, msg, sizeof(msg) - 1);
        (void)dummy;
#else
        fputs(msg, stderr);
#endif
    }
}

static bool
SetTimeoutValue(JSContext *cx, double t)
{
    /* NB: The next condition also filter out NaNs. */
    if (!(t <= MAX_TIMEOUT_INTERVAL)) {
        JS_ReportError(cx, "Excessive timeout value");
        return false;
    }
    gTimeoutInterval = t;
    if (!ScheduleWatchdog(cx->runtime(), t)) {
        JS_ReportError(cx, "Failed to create the watchdog");
        return false;
    }
    return true;
}

static bool
Timeout(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (args.length() == 0) {
        args.rval().setNumber(gTimeoutInterval);
        return true;
    }

    if (args.length() > 2) {
        JS_ReportError(cx, "Wrong number of arguments");
        return false;
    }

    double t;
    if (!ToNumber(cx, args[0], &t))
        return false;

    if (args.length() > 1) {
        RootedValue value(cx, args[1]);
        if (!value.isObject() || !value.toObject().is<JSFunction>()) {
            JS_ReportError(cx, "Second argument must be a timeout function");
            return false;
        }
        gTimeoutFunc = value;
    }

    args.rval().setUndefined();
    return SetTimeoutValue(cx, t);
}

static bool
Elapsed(JSContext *cx, unsigned argc, jsval *vp)
{
    if (argc == 0) {
        double d = 0.0;
        JSShellContextData *data = GetContextData(cx);
        if (data)
            d = PRMJ_Now() - data->startTime;
        JS_SET_RVAL(cx, vp, JS_NumberValue(d));
        return true;
    }
    JS_ReportError(cx, "Wrong number of arguments");
    return false;
}

static bool
Parent(JSContext *cx, unsigned argc, jsval *vp)
{
    if (argc != 1) {
        JS_ReportError(cx, "Wrong number of arguments");
        return false;
    }

    jsval v = JS_ARGV(cx, vp)[0];
    if (JSVAL_IS_PRIMITIVE(v)) {
        JS_ReportError(cx, "Only objects have parents!");
        return false;
    }

    Rooted<JSObject*> parent(cx, JS_GetParent(&v.toObject()));
    JS_SET_RVAL(cx, vp, OBJECT_TO_JSVAL(parent));

    /* Outerize if necessary.  Embrace the ugliness! */
    if (parent) {
        if (JSObjectOp op = parent->getClass()->ext.outerObject)
            JS_SET_RVAL(cx, vp, OBJECT_TO_JSVAL(op(cx, parent)));
    }

    return true;
}

static bool
Compile(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    if (args.length() < 1) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_MORE_ARGS_NEEDED,
                             "compile", "0", "s");
        return false;
    }
    if (!args[0].isString()) {
        const char *typeName = JS_GetTypeName(cx, JS_TypeOfValue(cx, args[0]));
        JS_ReportError(cx, "expected string to compile, got %s", typeName);
        return false;
    }

    RootedObject global(cx, JS::CurrentGlobalOrNull(cx));
    JSString *scriptContents = args[0].toString();
    JS::AutoSaveContextOptions asco(cx);
    JS::ContextOptionsRef(cx).setNoScriptRval(true);
    JS::CompileOptions options(cx);
    options.setFileAndLine("<string>", 1)
           .setCompileAndGo(true);
    bool ok = JS_CompileUCScript(cx, global, JS_GetStringCharsZ(cx, scriptContents),
                                 JS_GetStringLength(scriptContents), options);
    args.rval().setUndefined();
    return ok;
}

static bool
Parse(JSContext *cx, unsigned argc, jsval *vp)
{
    using namespace js::frontend;

    CallArgs args = CallArgsFromVp(argc, vp);

    if (args.length() < 1) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_MORE_ARGS_NEEDED,
                             "parse", "0", "s");
        return false;
    }
    if (!args[0].isString()) {
        const char *typeName = JS_GetTypeName(cx, JS_TypeOfValue(cx, args[0]));
        JS_ReportError(cx, "expected string to parse, got %s", typeName);
        return false;
    }

    JSString *scriptContents = args[0].toString();
    CompileOptions options(cx);
    options.setFileAndLine("<string>", 1)
           .setCompileAndGo(false);
    Parser<FullParseHandler> parser(cx, &cx->tempLifoAlloc(), options,
                                    JS_GetStringCharsZ(cx, scriptContents),
                                    JS_GetStringLength(scriptContents),
                                    /* foldConstants = */ true, nullptr, nullptr);

    ParseNode *pn = parser.parse(nullptr);
    if (!pn)
        return false;
#ifdef DEBUG
    DumpParseTree(pn);
    fputc('\n', stderr);
#endif
    args.rval().setUndefined();
    return true;
}

static bool
SyntaxParse(JSContext *cx, unsigned argc, jsval *vp)
{
    using namespace js::frontend;

    CallArgs args = CallArgsFromVp(argc, vp);

    if (args.length() < 1) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_MORE_ARGS_NEEDED,
                             "parse", "0", "s");
        return false;
    }
    if (!args[0].isString()) {
        const char *typeName = JS_GetTypeName(cx, JS_TypeOfValue(cx, args[0]));
        JS_ReportError(cx, "expected string to parse, got %s", typeName);
        return false;
    }

    JSString *scriptContents = args[0].toString();
    CompileOptions options(cx);
    options.setFileAndLine("<string>", 1)
           .setCompileAndGo(false);

    const jschar *chars = JS_GetStringCharsZ(cx, scriptContents);
    size_t length = JS_GetStringLength(scriptContents);
    Parser<frontend::SyntaxParseHandler> parser(cx, &cx->tempLifoAlloc(),
                                                options, chars, length, false, nullptr, nullptr);

    bool succeeded = parser.parse(nullptr);
    if (cx->isExceptionPending())
        return false;

    if (!succeeded && !parser.hadAbortedSyntaxParse()) {
        // If no exception is posted, either there was an OOM or a language
        // feature unhandled by the syntax parser was encountered.
        JS_ASSERT(cx->runtime()->hadOutOfMemory);
        return false;
    }

    args.rval().setBoolean(succeeded);
    return true;
}

#ifdef JS_THREADSAFE

class OffThreadState {
  public:
    enum State {
        IDLE,           /* ready to work; no token, no source */
        COMPILING,      /* working; no token, have source */
        DONE            /* compilation done: have token and source */
    };

    OffThreadState() : monitor(), state(IDLE), token() { }
    bool init() { return monitor.init(); }

    bool startIfIdle(JSContext *cx, JSString *newSource) {
        AutoLockMonitor alm(monitor);
        if (state != IDLE)
            return false;

        JS_ASSERT(!token);
        JS_ASSERT(!source);

        source = newSource;
        if (!JS_AddStringRoot(cx, &source))
            return false;

        state = COMPILING;
        return true;
    }

    void abandon(JSContext *cx) {
        AutoLockMonitor alm(monitor);
        JS_ASSERT(state == COMPILING);
        JS_ASSERT(!token);
        JS_ASSERT(source);

        JS_RemoveStringRoot(cx, &source);
        source = nullptr;

        state = IDLE;
    }

    void markDone(void *newToken) {
        AutoLockMonitor alm(monitor);
        JS_ASSERT(state == COMPILING);
        JS_ASSERT(!token);
        JS_ASSERT(source);
        JS_ASSERT(newToken);

        token = newToken;
        state = DONE;
        alm.notify();
    }

    void *waitUntilDone(JSContext *cx) {
        AutoLockMonitor alm(monitor);
        if (state == IDLE)
            return nullptr;

        if (state == COMPILING) {
            while (state != DONE)
                alm.wait();
        }

        JS_ASSERT(source);
        JS_RemoveStringRoot(cx, &source);
        source = nullptr;

        JS_ASSERT(token);
        void *holdToken = token;
        token = nullptr;
        state = IDLE;
        return holdToken;
    }

  private:
    Monitor monitor;
    State state;
    void *token;
    JSString *source;
};

static OffThreadState offThreadState;

static void
OffThreadCompileScriptCallback(void *token, void *callbackData)
{
    offThreadState.markDone(token);
}

static bool
OffThreadCompileScript(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (args.length() < 1) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_MORE_ARGS_NEEDED,
                             "offThreadCompileScript", "0", "s");
        return false;
    }
    if (!args[0].isString()) {
        const char *typeName = JS_GetTypeName(cx, JS_TypeOfValue(cx, args[0]));
        JS_ReportError(cx, "expected string to parse, got %s", typeName);
        return false;
    }

    JSString *scriptContents = args[0].toString();
    CompileOptions options(cx);
    options.setFileAndLine("<string>", 1)
           .setCompileAndGo(true)
           .setSourcePolicy(CompileOptions::SAVE_SOURCE);

    if (!JS::CanCompileOffThread(cx, options)) {
        JS_ReportError(cx, "cannot compile code on worker thread");
        return false;
    }

    const jschar *chars = JS_GetStringCharsZ(cx, scriptContents);
    if (!chars)
        return false;
    size_t length = JS_GetStringLength(scriptContents);

    if (!offThreadState.startIfIdle(cx, scriptContents)) {
        JS_ReportError(cx, "called offThreadCompileScript without calling runOffThreadScript"
                       " to receive prior off-thread compilation");
        return false;
    }

    if (!JS::CompileOffThread(cx, cx->global(), options, chars, length,
                              OffThreadCompileScriptCallback, nullptr))
    {
        offThreadState.abandon(cx);
        return false;
    }

    args.rval().setUndefined();
    return true;
}

static bool
runOffThreadScript(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    void *token = offThreadState.waitUntilDone(cx);
    if (!token) {
        JS_ReportError(cx, "called runOffThreadScript when no compilation is pending");
        return false;
    }

    RootedScript script(cx, JS::FinishOffThreadScript(cx, cx->runtime(), token));
    if (!script)
        return false;

    return JS_ExecuteScript(cx, cx->global(), script, args.rval().address());
}

#endif // JS_THREADSAFE

struct FreeOnReturn
{
    JSContext *cx;
    const char *ptr;
    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER

    FreeOnReturn(JSContext *cx, const char *ptr = nullptr
                 MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
      : cx(cx), ptr(ptr)
    {
        MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    }

    void init(const char *ptr) {
        JS_ASSERT(!this->ptr);
        this->ptr = ptr;
    }

    ~FreeOnReturn() {
        JS_free(cx, (void*)ptr);
    }
};

static bool
ReadFile(JSContext *cx, unsigned argc, jsval *vp, bool scriptRelative)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (args.length() < 1 || args.length() > 2) {
        JS_ReportErrorNumber(cx, my_GetErrorMessage, nullptr,
                             args.length() < 1 ? JSSMSG_NOT_ENOUGH_ARGS : JSSMSG_TOO_MANY_ARGS,
                             "snarf");
        return false;
    }

    if (!args[0].isString() || (args.length() == 2 && !args[1].isString())) {
        JS_ReportErrorNumber(cx, my_GetErrorMessage, nullptr, JSSMSG_INVALID_ARGS, "snarf");
        return false;
    }

    RootedString givenPath(cx, args[0].toString());
    RootedString str(cx, ResolvePath(cx, givenPath, scriptRelative));
    if (!str)
        return false;

    JSAutoByteString filename(cx, str);
    if (!filename)
        return false;

    if (args.length() > 1) {
        JSString *opt = JS::ToString(cx, args[1]);
        if (!opt)
            return false;
        bool match;
        if (!JS_StringEqualsAscii(cx, opt, "binary", &match))
            return false;
        if (match) {
            JSObject *obj;
            if (!(obj = FileAsTypedArray(cx, filename.ptr())))
                return false;
            JS_SET_RVAL(cx, vp, OBJECT_TO_JSVAL(obj));
            return true;
        }
    }

    if (!(str = FileAsString(cx, filename.ptr())))
        return false;
    JS_SET_RVAL(cx, vp, STRING_TO_JSVAL(str));
    return true;
}

static bool
Snarf(JSContext *cx, unsigned argc, jsval *vp)
{
    return ReadFile(cx, argc, vp, false);
}

static bool
ReadRelativeToScript(JSContext *cx, unsigned argc, jsval *vp)
{
    return ReadFile(cx, argc, vp, true);
}

static bool
redirect(JSContext *cx, FILE* fp, HandleString relFilename)
{
    RootedString filename(cx, ResolvePath(cx, relFilename, false));
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
RedirectOutput(JSContext *cx, unsigned argc, jsval *vp)
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
        if (!redirect(cx, stdout, stdoutPath))
            return false;
    }

    if (args.length() > 1 && args[1].isString()) {
        RootedString stderrPath(cx, args[1].toString());
        if (!stderrPath)
            return false;
        if (!redirect(cx, stderr, stderrPath))
            return false;
    }

    args.rval().setUndefined();
    return true;
}

static bool
System(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (args.length() == 0) {
        JS_ReportErrorNumber(cx, my_GetErrorMessage, nullptr, JSSMSG_INVALID_ARGS,
                             "system");
        return false;
    }

    JSString *str = JS::ToString(cx, args[0]);
    if (!str)
        return false;

    JSAutoByteString command(cx, str);
    if (!command)
        return false;

    int result = system(command.ptr());
    args.rval().setInt32(result);
    return true;
}

static int sArgc;
static char **sArgv;

class AutoCStringVector
{
    Vector<char *> argv_;
  public:
    AutoCStringVector(JSContext *cx) : argv_(cx) {}
    ~AutoCStringVector() {
        for (size_t i = 0; i < argv_.length(); i++)
            js_free(argv_[i]);
    }
    bool append(char *arg) {
        if (!argv_.append(arg)) {
            js_free(arg);
            return false;
        }
        return true;
    }
    char* const* get() const {
        return argv_.begin();
    }
    size_t length() const {
        return argv_.length();
    }
    char *operator[](size_t i) const {
        return argv_[i];
    }
    void replace(size_t i, char *arg) {
        js_free(argv_[i]);
        argv_[i] = arg;
    }
    char *back() const {
        return argv_.back();
    }
    void replaceBack(char *arg) {
        js_free(argv_.back());
        argv_.back() = arg;
    }
};

#if defined(XP_WIN)
static bool
EscapeForShell(AutoCStringVector &argv)
{
    // Windows will break arguments in argv by various spaces, so we wrap each
    // argument in quotes and escape quotes within. Even with quotes, \ will be
    // treated like an escape character, so inflate each \ to \\.

    for (size_t i = 0; i < argv.length(); i++) {
        if (!argv[i])
            continue;

        size_t newLen = 3;  // quotes before and after and null-terminator
        for (char *p = argv[i]; *p; p++) {
            newLen++;
            if (*p == '\"' || *p == '\\')
                newLen++;
        }

        char *escaped = (char *)js_malloc(newLen);
        if (!escaped)
            return false;

        char *src = argv[i];
        char *dst = escaped;
        *dst++ = '\"';
        while (*src) {
            if (*src == '\"' || *src == '\\')
                *dst++ = '\\';
            *dst++ = *src++;
        }
        *dst++ = '\"';
        *dst++ = '\0';
        JS_ASSERT(escaped + newLen == dst);

        argv.replace(i, escaped);
    }
    return true;
}
#endif

static Vector<const char*, 4, js::SystemAllocPolicy> sPropagatedFlags;

#ifdef DEBUG
#if (defined(JS_CPU_X86) || defined(JS_CPU_X64)) && defined(JS_ION)
static bool
PropagateFlagToNestedShells(const char *flag)
{
    return sPropagatedFlags.append(flag);
}
#endif
#endif

static bool
NestedShell(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    AutoCStringVector argv(cx);

    // The first argument to the shell is its path, which we assume is our own
    // argv[0].
    if (sArgc < 1) {
        JS_ReportErrorNumber(cx, my_GetErrorMessage, nullptr, JSSMSG_NESTED_FAIL);
        return false;
    }
    if (!argv.append(strdup(sArgv[0])))
        return false;

    // Propagate selected flags from the current shell
    for (unsigned i = 0; i < sPropagatedFlags.length(); i++) {
        char *cstr = strdup(sPropagatedFlags[i]);
        if (!cstr || !argv.append(cstr))
            return false;
    }

    // The arguments to nestedShell are stringified and append to argv.
    RootedString str(cx);
    for (unsigned i = 0; i < args.length(); i++) {
        str = ToString(cx, args[i]);
        if (!str || !argv.append(JS_EncodeString(cx, str)))
            return false;

        // As a special case, if the caller passes "--js-cache", replace that
        // with "--js-cache=$(jsCacheDir)"
        if (!strcmp(argv.back(), "--js-cache") && jsCacheDir) {
            char *newArg = JS_smprintf("--js-cache=%s", jsCacheDir);
            if (!newArg)
                return false;
            argv.replaceBack(newArg);
        }
    }

    // execv assumes argv is null-terminated
    if (!argv.append(nullptr))
        return false;

    int status = 0;
#if defined(XP_WIN)
    if (!EscapeForShell(argv))
        return false;
    status = _spawnv(_P_WAIT, sArgv[0], argv.get());
#else
    pid_t pid = fork();
    switch (pid) {
      case -1:
        JS_ReportErrorNumber(cx, my_GetErrorMessage, nullptr, JSSMSG_NESTED_FAIL);
        return false;
      case 0:
        (void)execv(sArgv[0], argv.get());
        exit(-1);
      default: {
        while (waitpid(pid, &status, 0) < 0 && errno == EINTR)
            continue;
        break;
      }
    }
#endif

    if (status != 0) {
        JS_ReportErrorNumber(cx, my_GetErrorMessage, nullptr, JSSMSG_NESTED_FAIL);
        return false;
    }

    args.rval().setUndefined();
    return true;
}

static bool
DecompileFunctionSomehow(JSContext *cx, unsigned argc, Value *vp,
                         JSString *(*decompiler)(JSContext *, HandleFunction, unsigned))
{
    CallArgs args = CallArgsFromVp(argc, vp);
    if (args.length() < 1 || !args[0].isObject() || !args[0].toObject().is<JSFunction>()) {
        args.rval().setUndefined();
        return true;
    }
    RootedFunction fun(cx, &args[0].toObject().as<JSFunction>());
    JSString *result = decompiler(cx, fun, 0);
    if (!result)
        return false;
    args.rval().setString(result);
    return true;
}

static bool
DecompileBody(JSContext *cx, unsigned argc, Value *vp)
{
    return DecompileFunctionSomehow(cx, argc, vp, JS_DecompileFunctionBody);
}

static bool
DecompileFunction(JSContext *cx, unsigned argc, Value *vp)
{
    return DecompileFunctionSomehow(cx, argc, vp, JS_DecompileFunction);
}

static bool
DecompileThisScript(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    RootedScript script (cx);
    if (!JS_DescribeScriptedCaller(cx, &script, nullptr)) {
        args.rval().setString(cx->runtime()->emptyString);
        return true;
    }

    {
        JSAutoCompartment ac(cx, script);
        JSString *result = JS_DecompileScript(cx, script, "test", 0);
        if (!result)
            return false;
        args.rval().setString(result);
    }

    return JS_WrapValue(cx, args.rval());
}

static bool
ThisFilename(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    RootedScript script (cx);
    if (!JS_DescribeScriptedCaller(cx, &script, nullptr) || !script->filename()) {
        args.rval().setString(cx->runtime()->emptyString);
        return true;
    }
    JSString *filename = JS_NewStringCopyZ(cx, script->filename());
    if (!filename)
        return false;
    args.rval().setString(filename);
    return true;
}

/*
 * Internal class for testing hasPrototype easily.
 * Uses passed in prototype instead of target's.
 */
class WrapperWithProto : public Wrapper
{
  public:
    explicit WrapperWithProto(unsigned flags)
      : Wrapper(flags, true)
    { }

    static JSObject *New(JSContext *cx, JSObject *obj, JSObject *proto, JSObject *parent,
                         Wrapper *handler);
};

/* static */ JSObject *
WrapperWithProto::New(JSContext *cx, JSObject *obj, JSObject *proto, JSObject *parent,
                      Wrapper *handler)
{
    JS_ASSERT(parent);
    AutoMarkInDeadZone amd(cx->zone());

    RootedValue priv(cx, ObjectValue(*obj));
    ProxyOptions options;
    options.setCallable(obj->isCallable());
    return NewProxyObject(cx, handler, priv, proto, parent, options);
}

static bool
Wrap(JSContext *cx, unsigned argc, jsval *vp)
{
    jsval v = argc > 0 ? JS_ARGV(cx, vp)[0] : UndefinedValue();
    if (JSVAL_IS_PRIMITIVE(v)) {
        JS_SET_RVAL(cx, vp, v);
        return true;
    }

    RootedObject obj(cx, JSVAL_TO_OBJECT(v));
    JSObject *wrapped = Wrapper::New(cx, obj, &obj->global(),
                                     &Wrapper::singleton);
    if (!wrapped)
        return false;

    JS_SET_RVAL(cx, vp, OBJECT_TO_JSVAL(wrapped));
    return true;
}

static bool
WrapWithProto(JSContext *cx, unsigned argc, jsval *vp)
{
    Value obj = UndefinedValue(), proto = UndefinedValue();
    if (argc == 2) {
        obj = JS_ARGV(cx, vp)[0];
        proto = JS_ARGV(cx, vp)[1];
    }
    if (!obj.isObject() || !proto.isObjectOrNull()) {
        JS_ReportErrorNumber(cx, my_GetErrorMessage, nullptr, JSSMSG_INVALID_ARGS,
                             "wrapWithProto");
        return false;
    }

    JSObject *wrapped = WrapperWithProto::New(cx, &obj.toObject(), proto.toObjectOrNull(),
                                              &obj.toObject().global(),
                                              &Wrapper::singletonWithPrototype);
    if (!wrapped)
        return false;

    JS_SET_RVAL(cx, vp, OBJECT_TO_JSVAL(wrapped));
    return true;
}

static JSObject *
NewGlobalObject(JSContext *cx, JS::CompartmentOptions &options);

static bool
NewGlobal(JSContext *cx, unsigned argc, jsval *vp)
{
    JS::CompartmentOptions options;
    options.setVersion(JSVERSION_LATEST);

    CallArgs args = CallArgsFromVp(argc, vp);
    if (args.length() == 1 && args[0].isObject()) {
        RootedObject opts(cx, &args[0].toObject());
        RootedValue v(cx);

        if (!JS_GetProperty(cx, opts, "sameZoneAs", &v))
            return false;
        if (v.isObject())
            options.setSameZoneAs(UncheckedUnwrap(&v.toObject()));

        if (!JS_GetProperty(cx, opts, "invisibleToDebugger", &v))
            return false;
        if (v.isBoolean())
            options.setInvisibleToDebugger(v.toBoolean());
    }

    RootedObject global(cx, NewGlobalObject(cx, options));
    if (!global)
        return false;

    if (!JS_WrapObject(cx, &global))
        return false;

    args.rval().setObject(*global);
    return true;
}

static bool
EnableStackWalkingAssertion(JSContext *cx, unsigned argc, jsval *vp)
{
    if (argc == 0 || !JSVAL_IS_BOOLEAN(JS_ARGV(cx, vp)[0])) {
        JS_ReportErrorNumber(cx, my_GetErrorMessage, nullptr, JSSMSG_INVALID_ARGS,
                             "enableStackWalkingAssertion");
        return false;
    }

#ifdef DEBUG
    cx->stackIterAssertionEnabled = JSVAL_TO_BOOLEAN(JS_ARGV(cx, vp)[0]);
#endif

    JS_SET_RVAL(cx, vp, UndefinedValue());
    return true;
}

static bool
GetMaxArgs(JSContext *cx, unsigned arg, jsval *vp)
{
    JS_SET_RVAL(cx, vp, INT_TO_JSVAL(ARGS_LENGTH_MAX));
    return true;
}

static bool
ObjectEmulatingUndefined(JSContext *cx, unsigned argc, jsval *vp)
{
    static const JSClass cls = {
        "ObjectEmulatingUndefined",
        JSCLASS_EMULATES_UNDEFINED,
        JS_PropertyStub,
        JS_DeletePropertyStub,
        JS_PropertyStub,
        JS_StrictPropertyStub,
        JS_EnumerateStub,
        JS_ResolveStub,
        JS_ConvertStub
    };

    RootedObject obj(cx, JS_NewObject(cx, &cls, JS::NullPtr(), JS::NullPtr()));
    if (!obj)
        return false;
    JS_SET_RVAL(cx, vp, ObjectValue(*obj));
    return true;
}

static bool
GetSelfHostedValue(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (argc != 1 || !args[0].isString()) {
        JS_ReportErrorNumber(cx, my_GetErrorMessage, nullptr, JSSMSG_INVALID_ARGS,
                             "getSelfHostedValue");
        return false;
    }
    RootedAtom srcAtom(cx, ToAtom<CanGC>(cx, args[0]));
    if (!srcAtom)
        return false;
    RootedPropertyName srcName(cx, srcAtom->asPropertyName());
    return cx->runtime()->cloneSelfHostedValue(cx, srcName, args.rval());
}

class ShellSourceHook: public SourceHook {
    // The runtime to which we attached a source hook.
    JSRuntime *rt;

    // The function we should call to lazily retrieve source code.
    // The constructor and destructor take care of rooting this with the
    // runtime.
    JSObject *fun;

  public:
    ShellSourceHook() : rt(nullptr), fun(nullptr) { }
    bool init(JSContext *cx, JSFunction &fun) {
        JS_ASSERT(!this->rt);
        JS_ASSERT(!this->fun);
        this->rt = cx->runtime();
        this->fun = &fun;
        return JS_AddNamedObjectRoot(cx, &this->fun,
                                     "lazy source callback, set with withSourceHook");
    }

    ~ShellSourceHook() {
        if (fun)
            JS_RemoveObjectRootRT(rt, &fun);
    }

    bool load(JSContext *cx, const char *filename, jschar **src, size_t *length) {
        JS_ASSERT(fun);

        RootedString str(cx, JS_NewStringCopyZ(cx, filename));
        if (!str)
            return false;
        RootedValue filenameValue(cx, StringValue(str));

        RootedValue result(cx);
        if (!Call(cx, UndefinedValue(), &fun->as<JSFunction>(),
                  1, filenameValue.address(), &result))
            return false;

        str = JS::ToString(cx, result);
        if (!str)
            return false;

        *length = JS_GetStringLength(str);
        *src = cx->pod_malloc<jschar>(*length);
        if (!*src)
            return false;

        const jschar *chars = JS_GetStringCharsZ(cx, str);
        if (!chars)
            return false;

        PodCopy(*src, chars, *length);
        return true;
    }
};

static bool
WithSourceHook(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    RootedObject callee(cx, &args.callee());

    if (args.length() != 2) {
        ReportUsageError(cx, callee, "Wrong number of arguments.");
        return false;
    }

    if (!args[0].isObject() || !args[0].toObject().is<JSFunction>()
        || !args[1].isObject() || !args[1].toObject().is<JSFunction>()) {
        ReportUsageError(cx, callee, "First and second arguments must be functions.");
        return false;
    }

    ShellSourceHook *hook = new ShellSourceHook();
    if (!hook->init(cx, args[0].toObject().as<JSFunction>())) {
        delete hook;
        return false;
    }

    SourceHook *savedHook = js::ForgetSourceHook(cx->runtime());
    js::SetSourceHook(cx->runtime(), hook);
    bool result = Call(cx, UndefinedValue(), &args[1].toObject(), 0, nullptr, args.rval());
    js::SetSourceHook(cx->runtime(), savedHook);
    return result;
}

static bool
IsCachingEnabled(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    args.rval().setBoolean(jsCacheAsmJSPath != nullptr);
    return true;
}

static const JSFunctionSpecWithHelp shell_functions[] = {
    JS_FN_HELP("version", Version, 0, 0,
"version([number])",
"  Get or force a script compilation version number."),

    JS_FN_HELP("options", Options, 0, 0,
"options([option ...])",
"  Get or toggle JavaScript options."),

    JS_FN_HELP("load", Load, 1, 0,
"load(['foo.js' ...])",
"  Load files named by string arguments. Filename is relative to the\n"
"      current working directory."),

    JS_FN_HELP("loadRelativeToScript", LoadScriptRelativeToScript, 1, 0,
"loadRelativeToScript(['foo.js' ...])",
"  Load files named by string arguments. Filename is relative to the\n"
"      calling script."),

    JS_FN_HELP("evaluate", Evaluate, 2, 0,
"evaluate(code[, options])",
"  Evaluate code as though it were the contents of a file.\n"
"  options is an optional object that may have these properties:\n"
"      compileAndGo: use the compile-and-go compiler option (default: true)\n"
"      noScriptRval: use the no-script-rval compiler option (default: false)\n"
"      fileName: filename for error messages and debug info\n"
"      lineNumber: starting line number for error messages and debug info\n"
"      global: global in which to execute the code\n"
"      newContext: if true, create and use a new cx (default: false)\n"
"      saveFrameChain: if true, save the frame chain before evaluating code\n"
"         and restore it afterwards\n"
"      catchTermination: if true, catch termination (failure without\n"
"         an exception value, as for slow scripts or out-of-memory)\n"
"         and return 'terminated'\n"
"      element: if present with value |v|, convert |v| to an object |o| mark\n"
"         the source as being attached to the DOM element |o|. If the\n"
"         property is omitted or |v| is null, don't attribute the source to\n"
"         any DOM element.\n"
"      elementProperty: if present and not undefined, the name of property\n"
"         of 'element' that holds this code. This is what Debugger.Source\n"
"         .prototype.elementProperty returns.\n"
"      sourceMapURL: if present with value |v|, convert |v| to a string, and\n"
"         provide that as the code's source map URL. If omitted, attach no\n"
"         source map URL to the code (although the code may provide one itself,\n"
"         via a //#sourceMappingURL comment).\n"
"      sourcePolicy: if present, the value converted to a string must be either\n"
"         'NO_SOURCE', 'LAZY_SOURCE', or 'SAVE_SOURCE'; use the given source\n"
"         retention policy for this compilation.\n"),

    JS_FN_HELP("run", Run, 1, 0,
"run('foo.js')",
"  Run the file named by the first argument, returning the number of\n"
"  of milliseconds spent compiling and executing it."),

    JS_FN_HELP("readline", ReadLine, 0, 0,
"readline()",
"  Read a single line from stdin."),

    JS_FN_HELP("print", Print, 0, 0,
"print([exp ...])",
"  Evaluate and print expressions to stdout."),

    JS_FN_HELP("printErr", PrintErr, 0, 0,
"printErr([exp ...])",
"  Evaluate and print expressions to stderr."),

    JS_FN_HELP("putstr", PutStr, 0, 0,
"putstr([exp])",
"  Evaluate and print expression without newline."),

    JS_FN_HELP("dateNow", Now, 0, 0,
"dateNow()",
"  Return the current time with sub-ms precision."),

    JS_FN_HELP("help", Help, 0, 0,
"help([name ...])",
"  Display usage and help messages."),

    JS_FN_HELP("quit", Quit, 0, 0,
"quit()",
"  Quit the shell."),

    JS_FN_HELP("assertEq", AssertEq, 2, 0,
"assertEq(actual, expected[, msg])",
"  Throw if the first two arguments are not the same (both +0 or both -0,\n"
"  both NaN, or non-zero and ===)."),

    JS_FN_HELP("setDebug", SetDebug, 1, 0,
"setDebug(debug)",
"  Set debug mode."),

    JS_FN_HELP("setDebuggerHandler", SetDebuggerHandler, 1, 0,
"setDebuggerHandler(f)",
"  Set handler for debugger keyword to f."),

    JS_FN_HELP("throwError", ThrowError, 0, 0,
"throwError()",
"  Throw an error from JS_ReportError."),

#ifdef DEBUG
    JS_FN_HELP("disassemble", DisassembleToString, 1, 0,
"disassemble([fun])",
"  Return the disassembly for the given function."),

    JS_FN_HELP("dis", Disassemble, 1, 0,
"dis([fun])",
"  Disassemble functions into bytecodes."),

    JS_FN_HELP("disfile", DisassFile, 1, 0,
"disfile('foo.js')",
"  Disassemble script file into bytecodes.\n"
"  dis and disfile take these options as preceeding string arguments:\n"
"    \"-r\" (disassemble recursively)\n"
"    \"-l\" (show line numbers)"),

    JS_FN_HELP("dissrc", DisassWithSrc, 1, 0,
"dissrc([fun])",
"  Disassemble functions with source lines."),

    JS_FN_HELP("dumpObject", DumpObject, 1, 0,
"dumpObject()",
"  Dump an internal representation of an object."),

    JS_FN_HELP("notes", Notes, 1, 0,
"notes([fun])",
"  Show source notes for functions."),

    JS_FN_HELP("findReferences", FindReferences, 1, 0,
"findReferences(target)",
"  Walk the entire heap, looking for references to |target|, and return a\n"
"  \"references object\" describing what we found.\n"
"\n"
"  Each property of the references object describes one kind of reference. The\n"
"  property's name is the label supplied to MarkObject, JS_CALL_TRACER, or what\n"
"  have you, prefixed with \"edge: \" to avoid collisions with system properties\n"
"  (like \"toString\" and \"__proto__\"). The property's value is an array of things\n"
"  that refer to |thing| via that kind of reference. Ordinary references from\n"
"  one object to another are named after the property name (with the \"edge: \"\n"
"  prefix).\n"
"\n"
"  Garbage collection roots appear as references from 'null'. We use the name\n"
"  given to the root (with the \"edge: \" prefix) as the name of the reference.\n"
"\n"
"  Note that the references object does record references from objects that are\n"
"  only reachable via |thing| itself, not just the references reachable\n"
"  themselves from roots that keep |thing| from being collected. (We could make\n"
"  this distinction if it is useful.)\n"
"\n"
"  If any references are found by the conservative scanner, the references\n"
"  object will have a property named \"edge: machine stack\"; the referrers will\n"
"  be 'null', because they are roots."),

#endif
    JS_FN_HELP("build", BuildDate, 0, 0,
"build()",
"  Show build date and time."),

    JS_FN_HELP("intern", Intern, 1, 0,
"intern(str)",
"  Internalize str in the atom table."),

    JS_FN_HELP("getpda", GetPDA, 1, 0,
"getpda(obj)",
"  Get the property descriptors for obj."),

    JS_FN_HELP("getslx", GetSLX, 1, 0,
"getslx(obj)",
"  Get script line extent."),

    JS_FN_HELP("evalcx", EvalInContext, 1, 0,
"evalcx(s[, o])",
"  Evaluate s in optional sandbox object o.\n"
"  if (s == '' && !o) return new o with eager standard classes\n"
"  if (s == 'lazy' && !o) return new o with lazy standard classes"),

    JS_FN_HELP("evalInFrame", EvalInFrame, 2, 0,
"evalInFrame(n,str,save)",
"  Evaluate 'str' in the nth up frame.\n"
"  If 'save' (default false), save the frame chain."),

    JS_FN_HELP("shapeOf", ShapeOf, 1, 0,
"shapeOf(obj)",
"  Get the shape of obj (an implementation detail)."),

    JS_FN_HELP("resolver", Resolver, 1, 0,
"resolver(src[, proto])",
"  Create object with resolve hook that copies properties\n"
"  from src. If proto is omitted, use Object.prototype."),

#ifdef DEBUG
    JS_FN_HELP("arrayInfo", js_ArrayInfo, 1, 0,
"arrayInfo(a1, a2, ...)",
"  Report statistics about arrays."),

#endif
#ifdef JS_THREADSAFE
    JS_FN_HELP("sleep", Sleep_fn, 1, 0,
"sleep(dt)",
"  Sleep for dt seconds."),

#endif
    JS_FN_HELP("snarf", Snarf, 1, 0,
"snarf(filename, [\"binary\"])",
"  Read filename into returned string. Filename is relative to the current\n"
               "  working directory."),

    JS_FN_HELP("read", Snarf, 1, 0,
"read(filename, [\"binary\"])",
"  Synonym for snarf."),

    JS_FN_HELP("readRelativeToScript", ReadRelativeToScript, 1, 0,
"readRelativeToScript(filename, [\"binary\"])",
"  Read filename into returned string. Filename is relative to the directory\n"
"  containing the current script."),

    JS_FN_HELP("compile", Compile, 1, 0,
"compile(code)",
"  Compiles a string to bytecode, potentially throwing."),

    JS_FN_HELP("parse", Parse, 1, 0,
"parse(code)",
"  Parses a string, potentially throwing."),

    JS_FN_HELP("syntaxParse", SyntaxParse, 1, 0,
"syntaxParse(code)",
"  Check the syntax of a string, returning success value"),

#ifdef JS_THREADSAFE
    JS_FN_HELP("offThreadCompileScript", OffThreadCompileScript, 1, 0,
"offThreadCompileScript(code)",
"  Trigger an off thread parse/emit for the input string"),

    JS_FN_HELP("runOffThreadScript", runOffThreadScript, 0, 0,
"runOffThreadScript()",
"  Wait for off-thread compilation to complete. If an error occurred,\n"
"  throw the appropriate exception; otherwise, run the script and return\n"
               "  its value."),

#endif

    JS_FN_HELP("timeout", Timeout, 1, 0,
"timeout([seconds], [func])",
"  Get/Set the limit in seconds for the execution time for the current context.\n"
"  A negative value (default) means that the execution time is unlimited.\n"
"  If a second argument is provided, it will be invoked when the timer elapses.\n"),

    JS_FN_HELP("elapsed", Elapsed, 0, 0,
"elapsed()",
"  Execution time elapsed for the current context."),

    JS_FN_HELP("decompileFunction", DecompileFunction, 1, 0,
"decompileFunction(func)",
"  Decompile a function."),

    JS_FN_HELP("decompileBody", DecompileBody, 1, 0,
"decompileBody(func)",
"  Decompile a function's body."),

    JS_FN_HELP("decompileThis", DecompileThisScript, 0, 0,
"decompileThis()",
"  Decompile the currently executing script."),

    JS_FN_HELP("thisFilename", ThisFilename, 0, 0,
"thisFilename()",
"  Return the filename of the current script"),

    JS_FN_HELP("wrap", Wrap, 1, 0,
"wrap(obj)",
"  Wrap an object into a noop wrapper."),

    JS_FN_HELP("wrapWithProto", WrapWithProto, 2, 0,
"wrapWithProto(obj)",
"  Wrap an object into a noop wrapper with prototype semantics."),

    JS_FN_HELP("newGlobal", NewGlobal, 1, 0,
"newGlobal([options])",
"  Return a new global object in a new compartment. If options\n"
"  is given, it may have any of the following properties:\n"
"      sameZoneAs: the compartment will be in the same zone as the given object (defaults to a new zone)\n"
"      invisibleToDebugger: the global will be invisible to the debugger (default false)"),

    JS_FN_HELP("enableStackWalkingAssertion", EnableStackWalkingAssertion, 1, 0,
"enableStackWalkingAssertion(enabled)",
"  Enables or disables a particularly expensive assertion in stack-walking\n"
"  code.  If your test isn't ridiculously thorough, such that performing this\n"
"  assertion increases test duration by an order of magnitude, you shouldn't\n"
"  use this."),

    JS_FN_HELP("getMaxArgs", GetMaxArgs, 0, 0,
"getMaxArgs()",
"  Return the maximum number of supported args for a call."),

    JS_FN_HELP("objectEmulatingUndefined", ObjectEmulatingUndefined, 0, 0,
"objectEmulatingUndefined()",
"  Return a new object obj for which typeof obj === \"undefined\", obj == null\n"
"  and obj == undefined (and vice versa for !=), and ToBoolean(obj) === false.\n"),

    JS_FN_HELP("isCachingEnabled", IsCachingEnabled, 0, 0,
"isCachingEnabled()",
"  Return whether --js-cache was set."),

    JS_FS_HELP_END
};

static const JSFunctionSpecWithHelp fuzzing_unsafe_functions[] = {
    JS_FN_HELP("clone", Clone, 1, 0,
"clone(fun[, scope])",
"  Clone function object."),

    JS_FN_HELP("getSelfHostedValue", GetSelfHostedValue, 1, 0,
"getSelfHostedValue()",
"  Get a self-hosted value by its name. Note that these values don't get \n"
"  cached, so repeatedly getting the same value creates multiple distinct clones."),

#ifdef DEBUG
    JS_FN_HELP("dumpHeap", DumpHeap, 0, 0,
"dumpHeap([fileName[, start[, toFind[, maxDepth[, toIgnore]]]]])",
"  Interface to JS_DumpHeap with output sent to file."),
#endif

    JS_FN_HELP("parent", Parent, 1, 0,
"parent(obj)",
"  Returns the parent of obj."),

    JS_FN_HELP("line2pc", LineToPC, 0, 0,
"line2pc([fun,] line)",
"  Map line number to PC."),

    JS_FN_HELP("pc2line", PCToLine, 0, 0,
"pc2line(fun[, pc])",
"  Map PC to line number."),

    JS_FN_HELP("redirect", RedirectOutput, 2, 0,
"redirect(stdoutFilename[, stderrFilename])",
"  Redirect stdout and/or stderr to the named file. Pass undefined to avoid\n"
"   redirecting. Filenames are relative to the current working directory."),

    JS_FN_HELP("setThrowHook", SetThrowHook, 1, 0,
"setThrowHook(f)",
"  Set throw hook to f."),

    JS_FN_HELP("system", System, 1, 0,
"system(command)",
"  Execute command on the current host, returning result code."),

    JS_FN_HELP("nestedShell", NestedShell, 0, 0,
"nestedShell(shellArgs...)",
"  Execute the given code in a new JS shell process, passing this nested shell\n"
"  the arguments passed to nestedShell. argv[0] of the nested shell will be argv[0]\n"
"  of the current shell (which is assumed to be the actual path to the shell.\n"
"  arguments[0] (of the call to nestedShell) will be argv[1], arguments[1] will\n"
"  be argv[2], etc."),

    JS_FN_HELP("trap", Trap, 3, 0,
"trap([fun, [pc,]] exp)",
"  Trap bytecode execution."),

    JS_FN_HELP("assertFloat32", testingFunc_assertFloat32, 2, 0,
"assertFloat32(value, isFloat32)",
"  In IonMonkey only, asserts that value has (resp. hasn't) the MIRType_Float32 if isFloat32 is true (resp. false)."),

    JS_FN_HELP("untrap", Untrap, 2, 0,
"untrap(fun[, pc])",
"  Remove a trap."),

    JS_FN_HELP("withSourceHook", WithSourceHook, 1, 0,
"withSourceHook(hook, fun)",
"  Set this JS runtime's lazy source retrieval hook (that is, the hook\n"
"  used to find sources compiled with |CompileOptions::LAZY_SOURCE|) to\n"
"  |hook|; call |fun| with no arguments; and then restore the runtime's\n"
"  original hook. Return or throw whatever |fun| did. |hook| gets\n"
"  passed the requested code's URL, and should return a string.\n"
"\n"
"  Notes:\n"
"\n"
"  1) SpiderMonkey may assert if the returned code isn't close enough\n"
"  to the script's real code, so this function is not fuzzer-safe.\n"
"\n"
"  2) The runtime can have only one source retrieval hook active at a\n"
"  time. If |fun| is not careful, |hook| could be asked to retrieve the\n"
"  source code for compilations that occurred long before it was set,\n"
"  and that it knows nothing about. The reverse applies as well: the\n"
"  original hook, that we reinstate after the call to |fun| completes,\n"
"  might be asked for the source code of compilations that |fun|\n"
"  performed, and which, presumably, only |hook| knows how to find.\n"),

    JS_FS_HELP_END
};

#ifdef MOZ_PROFILING
# define PROFILING_FUNCTION_COUNT 5
# ifdef MOZ_CALLGRIND
#  define CALLGRIND_FUNCTION_COUNT 3
# else
#  define CALLGRIND_FUNCTION_COUNT 0
# endif
# ifdef MOZ_VTUNE
#  define VTUNE_FUNCTION_COUNT 4
# else
#  define VTUNE_FUNCTION_COUNT 0
# endif
# define EXTERNAL_FUNCTION_COUNT (PROFILING_FUNCTION_COUNT + CALLGRIND_FUNCTION_COUNT + VTUNE_FUNCTION_COUNT)
#else
# define EXTERNAL_FUNCTION_COUNT 0
#endif

#undef PROFILING_FUNCTION_COUNT
#undef CALLGRIND_FUNCTION_COUNT
#undef VTUNE_FUNCTION_COUNT
#undef EXTERNAL_FUNCTION_COUNT

static bool
PrintHelpString(JSContext *cx, jsval v)
{
    JSString *str = JSVAL_TO_STRING(v);
    JS::Anchor<JSString *> a_str(str);
    const jschar *chars = JS_GetStringCharsZ(cx, str);
    if (!chars)
        return false;

    for (const jschar *p = chars; *p; p++)
        fprintf(gOutFile, "%c", char(*p));

    fprintf(gOutFile, "\n");

    return true;
}

static bool
PrintHelp(JSContext *cx, HandleObject obj)
{
    RootedValue usage(cx);
    if (!JS_LookupProperty(cx, obj, "usage", &usage))
        return false;
    RootedValue help(cx);
    if (!JS_LookupProperty(cx, obj, "help", &help))
        return false;

    if (JSVAL_IS_VOID(usage) || JSVAL_IS_VOID(help))
        return true;

    return PrintHelpString(cx, usage) && PrintHelpString(cx, help);
}

static bool
Help(JSContext *cx, unsigned argc, jsval *vp)
{
    fprintf(gOutFile, "%s\n", JS_GetImplementationVersion());

    RootedObject obj(cx);
    if (argc == 0) {
        RootedObject global(cx, JS::CurrentGlobalOrNull(cx));
        AutoIdArray ida(cx, JS_Enumerate(cx, global));
        if (!ida)
            return false;

        for (size_t i = 0; i < ida.length(); i++) {
            RootedValue v(cx);
            if (!JS_LookupPropertyById(cx, global, ida[i], &v))
                return false;
            if (JSVAL_IS_PRIMITIVE(v)) {
                JS_ReportError(cx, "primitive arg");
                return false;
            }
            obj = JSVAL_TO_OBJECT(v);
            if (!PrintHelp(cx, obj))
                return false;
        }
    } else {
        jsval *argv = JS_ARGV(cx, vp);
        for (unsigned i = 0; i < argc; i++) {
            if (JSVAL_IS_PRIMITIVE(argv[i])) {
                JS_ReportError(cx, "primitive arg");
                return false;
            }
            obj = JSVAL_TO_OBJECT(argv[i]);
            if (!PrintHelp(cx, obj))
                return false;
        }
    }

    JS_SET_RVAL(cx, vp, UndefinedValue());
    return true;
}

static const JSErrorFormatString jsShell_ErrorFormatString[JSShellErr_Limit] = {
#define MSG_DEF(name, number, count, exception, format) \
    { format, count, JSEXN_ERR } ,
#include "jsshell.msg"
#undef MSG_DEF
};

static const JSErrorFormatString *
my_GetErrorMessage(void *userRef, const char *locale, const unsigned errorNumber)
{
    if (errorNumber == 0 || errorNumber >= JSShellErr_Limit)
        return nullptr;

    return &jsShell_ErrorFormatString[errorNumber];
}

static void
my_ErrorReporter(JSContext *cx, const char *message, JSErrorReport *report)
{
    gGotError = PrintError(cx, gErrFile, message, report, reportWarnings);
    if (!JSREPORT_IS_WARNING(report->flags)) {
        if (report->errorNumber == JSMSG_OUT_OF_MEMORY) {
            gExitCode = EXITCODE_OUT_OF_MEMORY;
        } else {
            gExitCode = EXITCODE_RUNTIME_ERROR;
        }
    }
}

#if defined(SHELL_HACK) && defined(DEBUG) && defined(XP_UNIX)
static bool
Exec(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    JSFunction *fun;
    const char *name, **nargv;
    unsigned i, nargc;
    JSString *str;
    bool ok;
    pid_t pid;
    int status;

    JS_SET_RVAL(cx, vp, UndefinedValue());

    RootedValue arg(cx, vp[0]);
    fun = JS_ValueToFunction(cx, arg);
    if (!fun)
        return false;
    if (!fun->atom)
        return true;

    nargc = 1 + argc;

    /* nargc + 1 accounts for the terminating nullptr. */
    nargv = new (char *)[nargc + 1];
    if (!nargv)
        return false;
    memset(nargv, 0, sizeof(nargv[0]) * (nargc + 1));
    nargv[0] = name;
    jsval *argv = JS_ARGV(cx, vp);
    for (i = 0; i < nargc; i++) {
        str = (i == 0) ? fun->atom : JS::ToString(cx, args[i-1]);
        if (!str) {
            ok = false;
            goto done;
        }
        nargv[i] = JSStringToUTF8(cx, str);
        if (!nargv[i]) {
            ok = false;
            goto done;
        }
    }
    pid = fork();
    switch (pid) {
      case -1:
        perror("js");
        break;
      case 0:
        (void) execvp(name, (char **)nargv);
        perror("js");
        exit(127);
      default:
        while (waitpid(pid, &status, 0) < 0 && errno == EINTR)
            continue;
        break;
    }
    ok = true;

  done:
    for (i = 0; i < nargc; i++)
        JS_free(cx, nargv[i]);
    delete[] nargv;
    return ok;
}
#endif

static bool
global_enumerate(JSContext *cx, HandleObject obj)
{
#ifdef LAZY_STANDARD_CLASSES
    return JS_EnumerateStandardClasses(cx, obj);
#else
    return true;
#endif
}

static bool
global_resolve(JSContext *cx, HandleObject obj, HandleId id, unsigned flags,
               MutableHandleObject objp)
{
#ifdef LAZY_STANDARD_CLASSES
    bool resolved;

    if (!JS_ResolveStandardClass(cx, obj, id, &resolved))
        return false;
    if (resolved) {
        objp.set(obj);
        return true;
    }
#endif

#if defined(SHELL_HACK) && defined(DEBUG) && defined(XP_UNIX)
    /*
     * Do this expensive hack only for unoptimized Unix builds, which are
     * not used for benchmarking.
     */
    char *path, *comp, *full;
    const char *name;
    bool ok, found;
    JSFunction *fun;

    if (!JSVAL_IS_STRING(id))
        return true;
    path = getenv("PATH");
    if (!path)
        return true;
    path = JS_strdup(cx, path);
    if (!path)
        return false;
    JSAutoByteString name(cx, JSVAL_TO_STRING(id));
    if (!name)
        return false;
    ok = true;
    for (comp = strtok(path, ":"); comp; comp = strtok(nullptr, ":")) {
        if (*comp != '\0') {
            full = JS_smprintf("%s/%s", comp, name.ptr());
            if (!full) {
                JS_ReportOutOfMemory(cx);
                ok = false;
                break;
            }
        } else {
            full = (char *)name;
        }
        found = (access(full, X_OK) == 0);
        if (*comp != '\0')
            free(full);
        if (found) {
            fun = JS_DefineFunction(cx, obj, name, Exec, 0,
                                    JSPROP_ENUMERATE);
            ok = (fun != nullptr);
            if (ok)
                objp.set(obj);
            break;
        }
    }
    JS_free(cx, path);
    return ok;
#else
    return true;
#endif
}

static const JSClass global_class = {
    "global", JSCLASS_NEW_RESOLVE | JSCLASS_GLOBAL_FLAGS,
    JS_PropertyStub,  JS_DeletePropertyStub,
    JS_PropertyStub,  JS_StrictPropertyStub,
    global_enumerate, (JSResolveOp) global_resolve,
    JS_ConvertStub,   nullptr
};

static bool
env_setProperty(JSContext *cx, HandleObject obj, HandleId id, bool strict, MutableHandleValue vp)
{
/* XXX porting may be easy, but these don't seem to supply setenv by default */
#if !defined XP_OS2 && !defined SOLARIS
    int rv;

    RootedValue idvalue(cx, IdToValue(id));
    RootedString idstring(cx, ToString(cx, idvalue));
    JSAutoByteString idstr;
    if (!idstr.encodeLatin1(cx, idstring))
        return false;

    RootedString value(cx, ToString(cx, vp));
    if (!value)
        return false;
    JSAutoByteString valstr;
    if (!valstr.encodeLatin1(cx, value))
        return false;

#if defined XP_WIN || defined HPUX || defined OSF1
    {
        char *waste = JS_smprintf("%s=%s", idstr.ptr(), valstr.ptr());
        if (!waste) {
            JS_ReportOutOfMemory(cx);
            return false;
        }
        rv = putenv(waste);
#ifdef XP_WIN
        /*
         * HPUX9 at least still has the bad old non-copying putenv.
         *
         * Per mail from <s.shanmuganathan@digital.com>, OSF1 also has a putenv
         * that will crash if you pass it an auto char array (so it must place
         * its argument directly in the char *environ[] array).
         */
        JS_smprintf_free(waste);
#endif
    }
#else
    rv = setenv(idstr.ptr(), valstr.ptr(), 1);
#endif
    if (rv < 0) {
        JS_ReportError(cx, "can't set env variable %s to %s", idstr.ptr(), valstr.ptr());
        return false;
    }
    vp.set(StringValue(value));
#endif /* !defined XP_OS2 && !defined SOLARIS */
    return true;
}

static bool
env_enumerate(JSContext *cx, HandleObject obj)
{
    static bool reflected;
    char **evp, *name, *value;
    JSString *valstr;
    bool ok;

    if (reflected)
        return true;

    for (evp = (char **)JS_GetPrivate(obj); (name = *evp) != nullptr; evp++) {
        value = strchr(name, '=');
        if (!value)
            continue;
        *value++ = '\0';
        valstr = JS_NewStringCopyZ(cx, value);
        ok = valstr && JS_DefineProperty(cx, obj, name, STRING_TO_JSVAL(valstr),
                                         nullptr, nullptr, JSPROP_ENUMERATE);
        value[-1] = '=';
        if (!ok)
            return false;
    }

    reflected = true;
    return true;
}

static bool
env_resolve(JSContext *cx, HandleObject obj, HandleId id, unsigned flags,
            MutableHandleObject objp)
{
    RootedValue idvalue(cx, IdToValue(id));
    RootedString idstring(cx, ToString(cx, idvalue));
    JSAutoByteString idstr;
    if (!idstr.encodeLatin1(cx, idstring))
        return false;

    const char *name = idstr.ptr();
    const char *value = getenv(name);
    if (value) {
        RootedString valstr(cx, JS_NewStringCopyZ(cx, value));
        if (!valstr)
            return false;
        if (!JS_DefineProperty(cx, obj, name, STRING_TO_JSVAL(valstr),
                               nullptr, nullptr, JSPROP_ENUMERATE)) {
            return false;
        }
        objp.set(obj);
    }
    return true;
}

static const JSClass env_class = {
    "environment", JSCLASS_HAS_PRIVATE | JSCLASS_NEW_RESOLVE,
    JS_PropertyStub,  JS_DeletePropertyStub,
    JS_PropertyStub,  env_setProperty,
    env_enumerate, (JSResolveOp) env_resolve,
    JS_ConvertStub
};

/*
 * Define a FakeDOMObject constructor. It returns an object with a getter,
 * setter and method with attached JitInfo. This object can be used to test
 * IonMonkey DOM optimizations in the shell.
 */
static uint32_t DOM_OBJECT_SLOT = 0;

static bool
dom_genericGetter(JSContext* cx, unsigned argc, JS::Value *vp);

static bool
dom_genericSetter(JSContext* cx, unsigned argc, JS::Value *vp);

static bool
dom_genericMethod(JSContext *cx, unsigned argc, JS::Value *vp);

#ifdef DEBUG
static const JSClass *GetDomClass();
#endif

static bool
dom_get_x(JSContext* cx, HandleObject obj, void *self, JSJitGetterCallArgs args)
{
    JS_ASSERT(JS_GetClass(obj) == GetDomClass());
    JS_ASSERT(self == (void *)0x1234);
    args.rval().set(JS_NumberValue(double(3.14)));
    return true;
}

static bool
dom_set_x(JSContext* cx, HandleObject obj, void *self, JSJitSetterCallArgs args)
{
    JS_ASSERT(JS_GetClass(obj) == GetDomClass());
    JS_ASSERT(self == (void *)0x1234);
    return true;
}

static bool
dom_doFoo(JSContext* cx, HandleObject obj, void *self, const JSJitMethodCallArgs& args)
{
    JS_ASSERT(JS_GetClass(obj) == GetDomClass());
    JS_ASSERT(self == (void *)0x1234);

    /* Just return args.length(). */
    args.rval().setInt32(args.length());
    return true;
}

static const JSJitInfo dom_x_getterinfo = {
    { (JSJitGetterOp)dom_get_x },
    0,        /* protoID */
    0,        /* depth */
    JSJitInfo::AliasNone, /* aliasSet */
    JSJitInfo::Getter,
    JSVAL_TYPE_UNKNOWN, /* returnType */
    true,     /* isInfallible. False in setters. */
    true,     /* isMovable */
    false,    /* isInSlot */
    false,    /* isTypedMethod */
    0         /* slotIndex */
};

static const JSJitInfo dom_x_setterinfo = {
    { (JSJitGetterOp)dom_set_x },
    0,        /* protoID */
    0,        /* depth */
    JSJitInfo::Setter,
    JSJitInfo::AliasEverything, /* aliasSet */
    JSVAL_TYPE_UNKNOWN, /* returnType */
    false,    /* isInfallible. False in setters. */
    false,    /* isMovable. */
    false,    /* isInSlot */
    false,    /* isTypedMethod */
    0         /* slotIndex */
};

static const JSJitInfo doFoo_methodinfo = {
    { (JSJitGetterOp)dom_doFoo },
    0,        /* protoID */
    0,        /* depth */
    JSJitInfo::Method,
    JSJitInfo::AliasEverything, /* aliasSet */
    JSVAL_TYPE_UNKNOWN, /* returnType */
    false,    /* isInfallible. False in setters. */
    false,    /* isMovable */
    false,    /* isInSlot */
    false,    /* isTypedMethod */
    0         /* slotIndex */
};

static const JSPropertySpec dom_props[] = {
    {"x", 0,
     JSPROP_SHARED | JSPROP_ENUMERATE | JSPROP_NATIVE_ACCESSORS,
     { (JSPropertyOp)dom_genericGetter, &dom_x_getterinfo },
     { (JSStrictPropertyOp)dom_genericSetter, &dom_x_setterinfo }
    },
    {nullptr,0,0,JSOP_NULLWRAPPER, JSOP_NULLWRAPPER}
};

static const JSFunctionSpec dom_methods[] = {
    JS_FNINFO("doFoo", dom_genericMethod, &doFoo_methodinfo, 3, JSPROP_ENUMERATE),
    JS_FS_END
};

static const JSClass dom_class = {
    "FakeDOMObject", JSCLASS_IS_DOMJSCLASS | JSCLASS_HAS_RESERVED_SLOTS(2),
    JS_PropertyStub,       /* addProperty */
    JS_DeletePropertyStub, /* delProperty */
    JS_PropertyStub,       /* getProperty */
    JS_StrictPropertyStub, /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub,
    nullptr,               /* finalize */
    nullptr,               /* checkAccess */
    nullptr,               /* call */
    nullptr,               /* hasInstance */
    nullptr,               /* construct */
    nullptr,               /* trace */
    JSCLASS_NO_INTERNAL_MEMBERS
};

#ifdef DEBUG
static const JSClass *GetDomClass() {
    return &dom_class;
}
#endif

static bool
dom_genericGetter(JSContext *cx, unsigned argc, JS::Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    RootedObject obj(cx, JS_THIS_OBJECT(cx, vp));
    if (!obj)
        return false;

    if (JS_GetClass(obj) != &dom_class) {
        args.rval().set(UndefinedValue());
        return true;
    }

    JS::Value val = js::GetReservedSlot(obj, DOM_OBJECT_SLOT);

    const JSJitInfo *info = FUNCTION_VALUE_TO_JITINFO(JS_CALLEE(cx, vp));
    MOZ_ASSERT(info->type() == JSJitInfo::Getter);
    JSJitGetterOp getter = info->getter;
    return getter(cx, obj, val.toPrivate(), JSJitGetterCallArgs(args));
}

static bool
dom_genericSetter(JSContext* cx, unsigned argc, JS::Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    RootedObject obj(cx, JS_THIS_OBJECT(cx, vp));
    if (!obj)
        return false;

    JS_ASSERT(argc == 1);

    if (JS_GetClass(obj) != &dom_class) {
        args.rval().set(UndefinedValue());
        return true;
    }

    JS::Value val = js::GetReservedSlot(obj, DOM_OBJECT_SLOT);

    const JSJitInfo *info = FUNCTION_VALUE_TO_JITINFO(JS_CALLEE(cx, vp));
    MOZ_ASSERT(info->type() == JSJitInfo::Setter);
    JSJitSetterOp setter = info->setter;
    if (!setter(cx, obj, val.toPrivate(), JSJitSetterCallArgs(args)))
        return false;
    args.rval().set(UndefinedValue());
    return true;
}

static bool
dom_genericMethod(JSContext* cx, unsigned argc, JS::Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    RootedObject obj(cx, JS_THIS_OBJECT(cx, vp));
    if (!obj)
        return false;

    if (JS_GetClass(obj) != &dom_class) {
        args.rval().set(UndefinedValue());
        return true;
    }

    JS::Value val = js::GetReservedSlot(obj, DOM_OBJECT_SLOT);

    const JSJitInfo *info = FUNCTION_VALUE_TO_JITINFO(JS_CALLEE(cx, vp));
    MOZ_ASSERT(info->type() == JSJitInfo::Method);
    JSJitMethodOp method = info->method;
    return method(cx, obj, val.toPrivate(), JSJitMethodCallArgs(args));
}

static void
InitDOMObject(HandleObject obj)
{
    /* Fow now just initialize to a constant we can check. */
    SetReservedSlot(obj, DOM_OBJECT_SLOT, PRIVATE_TO_JSVAL((void *)0x1234));
}

static bool
dom_constructor(JSContext* cx, unsigned argc, JS::Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    RootedObject callee(cx, &args.callee());
    RootedValue protov(cx);
    if (!JSObject::getProperty(cx, callee, callee, cx->names().prototype, &protov))
        return false;

    if (!protov.isObject()) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_BAD_PROTOTYPE, "FakeDOMObject");
        return false;
    }

    RootedObject proto(cx, &protov.toObject());
    RootedObject domObj(cx, JS_NewObject(cx, &dom_class, proto, JS::NullPtr()));
    if (!domObj)
        return false;

    InitDOMObject(domObj);

    args.rval().setObject(*domObj);
    return true;
}

static bool
InstanceClassHasProtoAtDepth(JSObject *protoObject, uint32_t protoID, uint32_t depth)
{
    /* There's only a single (fake) DOM object in the shell, so just return true. */
    return true;
}

class ScopedFileDesc
{
    intptr_t fd_;
  public:
    enum LockType { READ_LOCK, WRITE_LOCK };
    ScopedFileDesc(int fd, LockType lockType)
      : fd_(fd)
    {
        if (fd == -1)
            return;
        if (!jsCacheOpened.compareExchange(false, true)) {
            close(fd_);
            fd_ = -1;
            return;
        }
    }
    ~ScopedFileDesc() {
        if (fd_ == -1)
            return;
        JS_ASSERT(jsCacheOpened == true);
        jsCacheOpened = false;
        close(fd_);
    }
    operator intptr_t() const {
        return fd_;
    }
    intptr_t forget() {
        intptr_t ret = fd_;
        fd_ = -1;
        return ret;
    }
};

// To guard against corrupted cache files generated by previous crashes, write
// asmJSCacheCookie to the first uint32_t of the file only after the file is
// fully serialized and flushed to disk.
static const uint32_t asmJSCacheCookie = 0xabbadaba;

static bool
ShellOpenAsmJSCacheEntryForRead(HandleObject global, const jschar *begin, const jschar *limit,
                                size_t *serializedSizeOut, const uint8_t **memoryOut,
                                intptr_t *handleOut)
{
    if (!jsCacheAsmJSPath)
        return false;

    ScopedFileDesc fd(open(jsCacheAsmJSPath, O_RDWR), ScopedFileDesc::READ_LOCK);
    if (fd == -1)
        return false;

    // Get the size and make sure we can dereference at least one uint32_t.
    off_t off = lseek(fd, 0, SEEK_END);
    if (off == -1 || off < (off_t)sizeof(uint32_t))
        return false;

    // Map the file into memory.
    void *memory;
#ifdef XP_WIN
    HANDLE fdOsHandle = (HANDLE)_get_osfhandle(fd);
    HANDLE fileMapping = CreateFileMapping(fdOsHandle, nullptr, PAGE_READWRITE, 0, 0, nullptr);
    if (!fileMapping)
        return false;

    memory = MapViewOfFile(fileMapping, FILE_MAP_READ, 0, 0, 0);
    CloseHandle(fileMapping);
    if (!memory)
        return false;
#else
    memory = mmap(nullptr, off, PROT_READ, MAP_SHARED, fd, 0);
    if (memory == MAP_FAILED)
        return false;
#endif

    // Perform check described by asmJSCacheCookie comment.
    if (*(uint32_t *)memory != asmJSCacheCookie) {
#ifdef XP_WIN
        UnmapViewOfFile(memory);
#else
        munmap(memory, off);
#endif
        return false;
    }

    // The embedding added the cookie so strip it off of the buffer returned to
    // the JS engine.
    *serializedSizeOut = off - sizeof(uint32_t);
    *memoryOut = (uint8_t *)memory + sizeof(uint32_t);
    *handleOut = fd.forget();
    return true;
}

static void
ShellCloseAsmJSCacheEntryForRead(HandleObject global, size_t serializedSize, const uint8_t *memory,
                                 intptr_t handle)
{
    // Undo the cookie adjustment done when opening the file.
    memory -= sizeof(uint32_t);
    serializedSize += sizeof(uint32_t);

    // Release the memory mapping and file.
#ifdef XP_WIN
    UnmapViewOfFile(const_cast<uint8_t*>(memory));
#else
    munmap(const_cast<uint8_t*>(memory), serializedSize);
#endif

    JS_ASSERT(jsCacheOpened == true);
    jsCacheOpened = false;
    close(handle);
}

static bool
ShellOpenAsmJSCacheEntryForWrite(HandleObject global, const jschar *begin, const jschar *end,
                                 size_t serializedSize, uint8_t **memoryOut, intptr_t *handleOut)
{
    if (!jsCacheAsmJSPath)
        return false;

    // Create the cache directory if it doesn't already exist.
    struct stat dirStat;
    if (stat(jsCacheDir, &dirStat) == 0) {
        if (!(dirStat.st_mode & S_IFDIR))
            return false;
    } else {
#ifdef XP_WIN
        if (mkdir(jsCacheDir) != 0)
            return false;
#else
        if (mkdir(jsCacheDir, 0777) != 0)
            return false;
#endif
    }

    ScopedFileDesc fd(open(jsCacheAsmJSPath, O_CREAT|O_RDWR, 0660), ScopedFileDesc::WRITE_LOCK);
    if (fd == -1)
        return false;

    // Include extra space for the asmJSCacheCookie.
    serializedSize += sizeof(uint32_t);

    // Resize the file to the appropriate size after zeroing their contents.
#ifdef XP_WIN
    if (chsize(fd, 0))
        return false;
    if (chsize(fd, serializedSize))
        return false;
#else
    if (ftruncate(fd, 0))
        return false;
    if (ftruncate(fd, serializedSize))
        return false;
#endif

    // Map the file into memory.
    void *memory;
#ifdef XP_WIN
    HANDLE fdOsHandle = (HANDLE)_get_osfhandle(fd);
    HANDLE fileMapping = CreateFileMapping(fdOsHandle, nullptr, PAGE_READWRITE, 0, 0, nullptr);
    if (!fileMapping)
        return false;

    memory = MapViewOfFile(fileMapping, FILE_MAP_WRITE, 0, 0, 0);
    CloseHandle(fileMapping);
    if (!memory)
        return false;
#else
    memory = mmap(nullptr, serializedSize, PROT_WRITE, MAP_SHARED, fd, 0);
    if (memory == MAP_FAILED)
        return false;
#endif

    // The embedding added the cookie so strip it off of the buffer returned to
    // the JS engine. The asmJSCacheCookie will be written on close, below.
    JS_ASSERT(*(uint32_t *)memory == 0);
    *memoryOut = (uint8_t *)memory + sizeof(uint32_t);
    *handleOut = fd.forget();
    return true;
}

static void
ShellCloseAsmJSCacheEntryForWrite(HandleObject global, size_t serializedSize, uint8_t *memory,
                                  intptr_t handle)
{
    // Undo the cookie adjustment done when opening the file.
    memory -= sizeof(uint32_t);
    serializedSize += sizeof(uint32_t);

    // Write the magic cookie value after flushing the entire cache entry.
#ifdef XP_WIN
    FlushViewOfFile(memory, serializedSize);
    FlushFileBuffers(HANDLE(_get_osfhandle(handle)));
#else
    msync(memory, serializedSize, MS_SYNC);
#endif

    JS_ASSERT(*(uint32_t *)memory == 0);
    *(uint32_t *)memory = asmJSCacheCookie;

    // Free the memory mapping and file.
#ifdef XP_WIN
    UnmapViewOfFile(const_cast<uint8_t*>(memory));
#else
    munmap(memory, serializedSize);
#endif

    JS_ASSERT(jsCacheOpened == true);
    jsCacheOpened = false;
    close(handle);
}

static bool
ShellBuildId(JS::BuildIdCharVector *buildId)
{
    // The browser embeds the date into the buildid and the buildid is embedded
    // in the binary, so every 'make' necessarily builds a new firefox binary.
    // Fortunately, the actual firefox executable is tiny -- all the code is in
    // libxul.so and other shared modules -- so this isn't a big deal. Not so
    // for the statically-linked JS shell. To avoid recompmiling js.cpp and
    // re-linking 'js' on every 'make', we use a constant buildid and rely on
    // the shell user to manually clear the cache (deleting the dir passed to
    // --js-cache) between cache-breaking updates. Note: jit_tests.py does this
    // on every run).
    const char buildid[] = "JS-shell";
    return buildId->append(buildid, sizeof(buildid));
}

static JS::AsmJSCacheOps asmJSCacheOps = {
    ShellOpenAsmJSCacheEntryForRead,
    ShellCloseAsmJSCacheEntryForRead,
    ShellOpenAsmJSCacheEntryForWrite,
    ShellCloseAsmJSCacheEntryForWrite,
    ShellBuildId
};

/*
 * Avoid a reentrancy hazard.
 *
 * The non-JS_THREADSAFE shell uses a signal handler to implement timeout().
 * The JS engine is not really reentrant, but JS_TriggerAllOperationCallbacks
 * is mostly safe--the only danger is that we might interrupt JS_NewContext or
 * JS_DestroyContext while the context list is being modified. Therefore we
 * disable the signal handler around calls to those functions.
 */
#ifdef JS_THREADSAFE
# define WITH_SIGNALS_DISABLED(x)  x
#else
# define WITH_SIGNALS_DISABLED(x)                                               \
    JS_BEGIN_MACRO                                                              \
        ScheduleWatchdog(gRuntime, -1);                                         \
        x;                                                                      \
        ScheduleWatchdog(gRuntime, gTimeoutInterval);                           \
    JS_END_MACRO
#endif

static JSContext *
NewContext(JSRuntime *rt)
{
    JSContext *cx;
    WITH_SIGNALS_DISABLED(cx = JS_NewContext(rt, gStackChunkSize));
    if (!cx)
        return nullptr;

    JSShellContextData *data = NewContextData();
    if (!data) {
        DestroyContext(cx, false);
        return nullptr;
    }

    JS_SetContextPrivate(cx, data);
    JS_SetErrorReporter(cx, my_ErrorReporter);
    if (enableTypeInference)
        JS::ContextOptionsRef(cx).toggleTypeInference();
    if (enableIon)
        JS::ContextOptionsRef(cx).toggleIon();
    if (enableBaseline)
        JS::ContextOptionsRef(cx).toggleBaseline();
    if (enableAsmJS)
        JS::ContextOptionsRef(cx).toggleAsmJS();
    return cx;
}

static void
DestroyContext(JSContext *cx, bool withGC)
{
    JSShellContextData *data = GetContextData(cx);
    JS_SetContextPrivate(cx, nullptr);
    free(data);
    WITH_SIGNALS_DISABLED(withGC ? JS_DestroyContext(cx) : JS_DestroyContextNoGC(cx));
}

static JSObject *
NewGlobalObject(JSContext *cx, JS::CompartmentOptions &options)
{
    RootedObject glob(cx, JS_NewGlobalObject(cx, &global_class, nullptr,
                                             JS::DontFireOnNewGlobalHook, options));
    if (!glob)
        return nullptr;

    {
        JSAutoCompartment ac(cx, glob);

#ifndef LAZY_STANDARD_CLASSES
        if (!JS_InitStandardClasses(cx, glob))
            return nullptr;
#endif

#ifdef JS_HAS_CTYPES
        if (!JS_InitCTypesClass(cx, glob))
            return nullptr;
#endif
        if (!JS_InitReflect(cx, glob))
            return nullptr;
        if (!JS_DefineDebuggerObject(cx, glob))
            return nullptr;
        if (!JS::RegisterPerfMeasurement(cx, glob))
            return nullptr;
        if (!JS_DefineFunctionsWithHelp(cx, glob, shell_functions) ||
            !JS_DefineProfilingFunctions(cx, glob))
        {
            return nullptr;
        }
        if (!js::DefineTestingFunctions(cx, glob, fuzzingSafe))
            return nullptr;

        if (!fuzzingSafe && !JS_DefineFunctionsWithHelp(cx, glob, fuzzing_unsafe_functions))
            return nullptr;

        /* Initialize FakeDOMObject. */
        static const js::DOMCallbacks DOMcallbacks = {
            InstanceClassHasProtoAtDepth
        };
        SetDOMCallbacks(cx->runtime(), &DOMcallbacks);

        RootedObject domProto(cx, JS_InitClass(cx, glob, nullptr, &dom_class, dom_constructor, 0,
                                               dom_props, dom_methods, nullptr, nullptr));
        if (!domProto)
            return nullptr;

        /* Initialize FakeDOMObject.prototype */
        InitDOMObject(domProto);
    }

    JS_FireOnNewGlobalObject(cx, glob);

    return glob;
}

static bool
BindScriptArgs(JSContext *cx, JSObject *obj_, OptionParser *op)
{
    RootedObject obj(cx, obj_);

    MultiStringRange msr = op->getMultiStringArg("scriptArgs");
    RootedObject scriptArgs(cx);
    scriptArgs = JS_NewArrayObject(cx, 0, nullptr);
    if (!scriptArgs)
        return false;

    if (!JS_DefineProperty(cx, obj, "scriptArgs", OBJECT_TO_JSVAL(scriptArgs),
                           nullptr, nullptr, 0))
        return false;

    for (size_t i = 0; !msr.empty(); msr.popFront(), ++i) {
        const char *scriptArg = msr.front();
        JSString *str = JS_NewStringCopyZ(cx, scriptArg);
        if (!str ||
            !JS_DefineElement(cx, scriptArgs, i, STRING_TO_JSVAL(str), nullptr, nullptr,
                              JSPROP_ENUMERATE)) {
            return false;
        }
    }

    return true;
}

// This function is currently only called from "#if defined(JS_ION)" chunks,
// so we're guarding the function definition with an #ifdef, too, to avoid
// build warning for unused function in non-ion-enabled builds:
#if defined(JS_ION)
static int
OptionFailure(const char *option, const char *str)
{
    fprintf(stderr, "Unrecognized option for %s: %s\n", option, str);
    return EXIT_FAILURE;
}
#endif /* JS_ION */

static int
ProcessArgs(JSContext *cx, JSObject *obj_, OptionParser *op)
{
    RootedObject obj(cx, obj_);

    if (op->getBoolOption('c'))
        compileOnly = true;

    if (op->getBoolOption('w'))
        reportWarnings = true;
    else if (op->getBoolOption('W'))
        reportWarnings = false;

    if (op->getBoolOption('s'))
        JS::ContextOptionsRef(cx).toggleExtraWarnings();

    if (op->getBoolOption('d')) {
        JS_SetRuntimeDebugMode(JS_GetRuntime(cx), true);
        JS_SetDebugMode(cx, true);
    }

    jsCacheDir = op->getStringOption("js-cache");
    if (jsCacheDir) {
        if (op->getBoolOption("js-cache-per-process"))
            jsCacheDir = JS_smprintf("%s/%u", jsCacheDir, (unsigned)getpid());
        jsCacheAsmJSPath = JS_smprintf("%s/asmjs.cache", jsCacheDir);
    }

    if (op->getBoolOption('b'))
        printTiming = true;

    if (op->getBoolOption('D')) {
        cx->runtime()->profilingScripts = true;
        enableDisassemblyDumps = true;
    }

#ifdef JS_THREADSAFE
    int32_t threadCount = op->getIntOption("thread-count");
    if (threadCount >= 0)
        cx->runtime()->setFakeCPUCount(threadCount);
#endif /* JS_THREADSAFE */

#if defined(JS_ION)
    if (op->getBoolOption("no-ion")) {
        enableIon = false;
        JS::ContextOptionsRef(cx).toggleIon();
    }
    if (op->getBoolOption("no-asmjs")) {
        enableAsmJS = false;
        JS::ContextOptionsRef(cx).toggleAsmJS();
    }

    if (op->getBoolOption("no-baseline")) {
        enableBaseline = false;
        JS::ContextOptionsRef(cx).toggleBaseline();
    }

    if (const char *str = op->getStringOption("ion-gvn")) {
        if (strcmp(str, "off") == 0) {
            jit::js_JitOptions.disableGvn = true;
        } else if (strcmp(str, "pessimistic") == 0) {
            jit::js_JitOptions.forceGvnKind = true;
            jit::js_JitOptions.forcedGvnKind = jit::GVN_Pessimistic;
        } else if (strcmp(str, "optimistic") == 0) {
            jit::js_JitOptions.forceGvnKind = true;
            jit::js_JitOptions.forcedGvnKind = jit::GVN_Optimistic;
        } else {
            return OptionFailure("ion-gvn", str);
        }
    }

    if (const char *str = op->getStringOption("ion-licm")) {
        if (strcmp(str, "on") == 0)
            jit::js_JitOptions.disableLicm = false;
        else if (strcmp(str, "off") == 0)
            jit::js_JitOptions.disableLicm = true;
        else
            return OptionFailure("ion-licm", str);
    }

    if (const char *str = op->getStringOption("ion-edgecase-analysis")) {
        if (strcmp(str, "on") == 0)
            jit::js_JitOptions.disableEdgeCaseAnalysis = false;
        else if (strcmp(str, "off") == 0)
            jit::js_JitOptions.disableEdgeCaseAnalysis = true;
        else
            return OptionFailure("ion-edgecase-analysis", str);
    }

     if (const char *str = op->getStringOption("ion-range-analysis")) {
         if (strcmp(str, "on") == 0)
             jit::js_JitOptions.disableRangeAnalysis = false;
         else if (strcmp(str, "off") == 0)
             jit::js_JitOptions.disableRangeAnalysis = true;
         else
             return OptionFailure("ion-range-analysis", str);
     }

    if (op->getBoolOption("ion-check-range-analysis"))
        jit::js_JitOptions.checkRangeAnalysis = true;

    if (op->getBoolOption("ion-check-thread-safety"))
        jit::js_JitOptions.checkThreadSafety = true;

    if (const char *str = op->getStringOption("ion-inlining")) {
        if (strcmp(str, "on") == 0)
            jit::js_JitOptions.disableInlining = false;
        else if (strcmp(str, "off") == 0)
            jit::js_JitOptions.disableInlining = true;
        else
            return OptionFailure("ion-inlining", str);
    }

    if (const char *str = op->getStringOption("ion-osr")) {
        if (strcmp(str, "on") == 0)
            jit::js_JitOptions.osr = true;
        else if (strcmp(str, "off") == 0)
            jit::js_JitOptions.osr = false;
        else
            return OptionFailure("ion-osr", str);
    }

    if (const char *str = op->getStringOption("ion-limit-script-size")) {
        if (strcmp(str, "on") == 0)
            jit::js_JitOptions.limitScriptSize = true;
        else if (strcmp(str, "off") == 0)
            jit::js_JitOptions.limitScriptSize = false;
        else
            return OptionFailure("ion-limit-script-size", str);
    }

    int32_t useCount = op->getIntOption("ion-uses-before-compile");
    if (useCount >= 0)
        jit::js_JitOptions.setUsesBeforeCompile(useCount);

    useCount = op->getIntOption("baseline-uses-before-compile");
    if (useCount >= 0)
        jit::js_JitOptions.baselineUsesBeforeCompile = useCount;

    if (op->getBoolOption("baseline-eager"))
        jit::js_JitOptions.baselineUsesBeforeCompile = 0;

    if (const char *str = op->getStringOption("ion-regalloc")) {
        if (strcmp(str, "lsra") == 0) {
            jit::js_JitOptions.forceRegisterAllocator = true;
            jit::js_JitOptions.forcedRegisterAllocator = jit::RegisterAllocator_LSRA;
        } else if (strcmp(str, "backtracking") == 0) {
            jit::js_JitOptions.forceRegisterAllocator = true;
            jit::js_JitOptions.forcedRegisterAllocator = jit::RegisterAllocator_Backtracking;
        } else if (strcmp(str, "stupid") == 0) {
            jit::js_JitOptions.forceRegisterAllocator = true;
            jit::js_JitOptions.forcedRegisterAllocator = jit::RegisterAllocator_Stupid;
        } else {
            return OptionFailure("ion-regalloc", str);
        }
    }

    if (op->getBoolOption("ion-eager"))
        jit::js_JitOptions.setEagerCompilation();

    if (op->getBoolOption("ion-compile-try-catch"))
        jit::js_JitOptions.compileTryCatch = true;

    bool parallelCompilation = true;
    if (const char *str = op->getStringOption("ion-parallel-compile")) {
        if (strcmp(str, "off") == 0)
            parallelCompilation = false;
        else if (strcmp(str, "on") != 0)
            return OptionFailure("ion-parallel-compile", str);
    }
#ifdef JS_THREADSAFE
    cx->runtime()->setParallelIonCompilationEnabled(parallelCompilation);
#endif

#endif /* JS_ION */

#ifdef DEBUG
    if (op->getBoolOption("dump-entrained-variables"))
        dumpEntrainedVariables = true;
#endif

    /* |scriptArgs| gets bound on the global before any code is run. */
    if (!BindScriptArgs(cx, obj, op))
        return EXIT_FAILURE;

    MultiStringRange filePaths = op->getMultiStringOption('f');
    MultiStringRange codeChunks = op->getMultiStringOption('e');

    if (filePaths.empty() && codeChunks.empty() && !op->getStringArg("script")) {
        Process(cx, obj, nullptr, true); /* Interactive. */
        return gExitCode;
    }

    while (!filePaths.empty() || !codeChunks.empty()) {
        size_t fpArgno = filePaths.empty() ? -1 : filePaths.argno();
        size_t ccArgno = codeChunks.empty() ? -1 : codeChunks.argno();
        if (fpArgno < ccArgno) {
            char *path = filePaths.front();
            Process(cx, obj, path, false);
            if (gExitCode)
                return gExitCode;
            filePaths.popFront();
        } else {
            const char *code = codeChunks.front();
            RootedValue rval(cx);
            if (!JS_EvaluateScript(cx, obj, code, strlen(code), "-e", 1, rval.address()))
                return gExitCode ? gExitCode : EXITCODE_RUNTIME_ERROR;
            codeChunks.popFront();
        }
    }

    /* The |script| argument is processed after all options. */
    if (const char *path = op->getStringArg("script")) {
        Process(cx, obj, path, false);
        if (gExitCode)
            return gExitCode;
    }

    if (op->getBoolOption('i'))
        Process(cx, obj, nullptr, true);

    return gExitCode ? gExitCode : EXIT_SUCCESS;
}

static int
Shell(JSContext *cx, OptionParser *op, char **envp)
{
    JSAutoRequest ar(cx);

    /*
     * First check to see if type inference is enabled. These flags
     * must be set on the compartment when it is constructed.
     */
    if (op->getBoolOption("no-ti")) {
        enableTypeInference = false;
        JS::ContextOptionsRef(cx).toggleTypeInference();
    }

    if (op->getBoolOption("fuzzing-safe"))
        fuzzingSafe = true;
    else
        fuzzingSafe = (getenv("MOZ_FUZZING_SAFE") && getenv("MOZ_FUZZING_SAFE")[0] != '0');

    RootedObject glob(cx);
    JS::CompartmentOptions options;
    options.setVersion(JSVERSION_LATEST);
    glob = NewGlobalObject(cx, options);
    if (!glob)
        return 1;

    JSAutoCompartment ac(cx, glob);
    js::SetDefaultObjectForContext(cx, glob);

    JSObject *envobj = JS_DefineObject(cx, glob, "environment", &env_class, nullptr, 0);
    if (!envobj)
        return 1;
    JS_SetPrivate(envobj, envp);

    int result = ProcessArgs(cx, glob, op);

    if (enableDisassemblyDumps)
        JS_DumpCompartmentPCCounts(cx);

    if (op->getBoolOption("js-cache-per-process")) {
        if (jsCacheAsmJSPath)
            unlink(jsCacheAsmJSPath);
        if (jsCacheDir)
            rmdir(jsCacheDir);
    }

    return result;
}

static void
MaybeOverrideOutFileFromEnv(const char* const envVar,
                            FILE* defaultOut,
                            FILE** outFile)
{
    const char* outPath = getenv(envVar);
    if (!outPath || !*outPath || !(*outFile = fopen(outPath, "w"))) {
        *outFile = defaultOut;
    }
}

static bool
CheckObjectAccess(JSContext *cx, HandleObject obj, HandleId id, JSAccessMode mode,
                  MutableHandleValue vp)
{
    return true;
}

static const JSSecurityCallbacks securityCallbacks = {
    CheckObjectAccess,
    nullptr
};

/* Pretend we can always preserve wrappers for dummy DOM objects. */
static bool
DummyPreserveWrapperCallback(JSContext *cx, JSObject *obj)
{
    return true;
}

int
main(int argc, char **argv, char **envp)
{
    sArgc = argc;
    sArgv = argv;

    JSRuntime *rt;
    JSContext *cx;
    int result;
#ifdef XP_WIN
    {
        const char *crash_option = getenv("XRE_NO_WINDOWS_CRASH_DIALOG");
        if (crash_option && strncmp(crash_option, "1", 1)) {
            DWORD oldmode = SetErrorMode(SEM_NOGPFAULTERRORBOX);
            SetErrorMode(oldmode | SEM_NOGPFAULTERRORBOX);
        }
    }
#endif

#ifdef HAVE_SETLOCALE
    setlocale(LC_ALL, "");
#endif

#ifdef XP_OS2
   /* these streams are normally line buffered on OS/2 and need a \n, *
    * so we need to unbuffer then to get a reasonable prompt          */
    setbuf(stdout,0);
    setbuf(stderr,0);
#endif

    MaybeOverrideOutFileFromEnv("JS_STDERR", stderr, &gErrFile);
    MaybeOverrideOutFileFromEnv("JS_STDOUT", stdout, &gOutFile);

    OptionParser op("Usage: {progname} [options] [[script] scriptArgs*]");

    op.setDescription("The SpiderMonkey shell provides a command line interface to the "
        "JavaScript engine. Code and file options provided via the command line are "
        "run left to right. If provided, the optional script argument is run after "
        "all options have been processed. Just-In-Time compilation modes may be enabled via "
        "command line options.");
    op.setDescriptionWidth(72);
    op.setHelpWidth(80);
    op.setVersion(JS_GetImplementationVersion());

    if (!op.addMultiStringOption('f', "file", "PATH", "File path to run")
        || !op.addMultiStringOption('e', "execute", "CODE", "Inline code to run")
        || !op.addBoolOption('i', "shell", "Enter prompt after running code")
        || !op.addBoolOption('m', "jm", "No-op (still used by fuzzers)")
        || !op.addBoolOption('\0', "no-jm", "No-op (still used by fuzzers)")
        || !op.addBoolOption('n', "ti", "Enable type inference (default)")
        || !op.addBoolOption('\0', "no-ti", "Disable type inference")
        || !op.addBoolOption('c', "compileonly", "Only compile, don't run (syntax checking mode)")
        || !op.addBoolOption('w', "warnings", "Emit warnings")
        || !op.addBoolOption('W', "nowarnings", "Don't emit warnings")
        || !op.addBoolOption('s', "strict", "Check strictness")
        || !op.addBoolOption('d', "debugjit", "Enable runtime debug mode for method JIT code")
        || !op.addBoolOption('a', "always-mjit", "No-op (still used by fuzzers)")
        || !op.addBoolOption('D', "dump-bytecode", "Dump bytecode with exec count for all scripts")
        || !op.addBoolOption('b', "print-timing", "Print sub-ms runtime for each file that's run")
        || !op.addStringOption('\0', "js-cache", "[path]",
                               "Enable the JS cache by specifying the path of the directory to use "
                               "to hold cache files")
        || !op.addBoolOption('\0', "js-cache-per-process",
                               "Generate a separate cache sub-directory for this process inside "
                               "the cache directory specified by --js-cache. This cache directory "
                               "will be removed when the js shell exits. This is useful for running "
                               "tests in parallel.")
#ifdef DEBUG
        || !op.addBoolOption('O', "print-alloc", "Print the number of allocations at exit")
#endif
        || !op.addOptionalStringArg("script", "A script to execute (after all options)")
        || !op.addOptionalMultiStringArg("scriptArgs",
                                         "String arguments to bind as |scriptArgs| in the "
                                         "shell's global")
#ifdef JS_THREADSAFE
        || !op.addIntOption('\0', "thread-count", "COUNT", "Use COUNT auxiliary threads "
                            "(default: # of cores - 1)", -1)
#endif
        || !op.addBoolOption('\0', "ion", "Enable IonMonkey (default)")
        || !op.addBoolOption('\0', "no-ion", "Disable IonMonkey")
        || !op.addBoolOption('\0', "no-asmjs", "Disable asm.js compilation")
        || !op.addStringOption('\0', "ion-gvn", "[mode]",
                               "Specify Ion global value numbering:\n"
                               "  off: disable GVN\n"
                               "  pessimistic: use pessimistic GVN\n"
                               "  optimistic: (default) use optimistic GVN")
        || !op.addStringOption('\0', "ion-licm", "on/off",
                               "Loop invariant code motion (default: on, off to disable)")
        || !op.addStringOption('\0', "ion-edgecase-analysis", "on/off",
                               "Find edge cases where Ion can avoid bailouts (default: on, off to disable)")
        || !op.addStringOption('\0', "ion-range-analysis", "on/off",
                               "Range analysis (default: on, off to disable)")
        || !op.addBoolOption('\0', "ion-check-range-analysis",
                               "Range analysis checking")
        || !op.addBoolOption('\0', "ion-check-thread-safety",
                             "IonBuilder thread safety checking")
        || !op.addStringOption('\0', "ion-inlining", "on/off",
                               "Inline methods where possible (default: on, off to disable)")
        || !op.addStringOption('\0', "ion-osr", "on/off",
                               "On-Stack Replacement (default: on, off to disable)")
        || !op.addStringOption('\0', "ion-limit-script-size", "on/off",
                               "Don't compile very large scripts (default: on, off to disable)")
        || !op.addIntOption('\0', "ion-uses-before-compile", "COUNT",
                            "Wait for COUNT calls or iterations before compiling "
                            "(default: 1000)", -1)
        || !op.addStringOption('\0', "ion-regalloc", "[mode]",
                               "Specify Ion register allocation:\n"
                               "  lsra: Linear Scan register allocation (default)\n"
                               "  backtracking: Priority based backtracking register allocation\n"
                               "  stupid: Simple block local register allocation")
        || !op.addBoolOption('\0', "ion-eager", "Always ion-compile methods (implies --baseline-eager)")
        || !op.addBoolOption('\0', "ion-compile-try-catch", "Ion-compile try-catch statements")
        || !op.addStringOption('\0', "ion-parallel-compile", "on/off",
                               "Compile scripts off thread (default: off)")
        || !op.addBoolOption('\0', "baseline", "Enable baseline compiler (default)")
        || !op.addBoolOption('\0', "no-baseline", "Disable baseline compiler")
        || !op.addBoolOption('\0', "baseline-eager", "Always baseline-compile methods")
        || !op.addIntOption('\0', "baseline-uses-before-compile", "COUNT",
                            "Wait for COUNT calls or iterations before baseline-compiling "
                            "(default: 10)", -1)
        || !op.addBoolOption('\0', "no-fpu", "Pretend CPU does not support floating-point operations "
                             "to test JIT codegen (no-op on platforms other than x86).")
        || !op.addBoolOption('\0', "no-sse3", "Pretend CPU does not support SSE3 instructions and above "
                             "to test JIT codegen (no-op on platforms other than x86 and x64).")
        || !op.addBoolOption('\0', "no-sse4", "Pretend CPU does not support SSE4 instructions"
                             "to test JIT codegen (no-op on platforms other than x86 and x64).")
        || !op.addBoolOption('\0', "fuzzing-safe", "Don't expose functions that aren't safe for "
                             "fuzzers to call")
#ifdef DEBUG
        || !op.addBoolOption('\0', "dump-entrained-variables", "Print variables which are "
                             "unnecessarily entrained by inner functions")
#endif
#ifdef JSGC_GENERATIONAL
        || !op.addBoolOption('\0', "no-ggc", "Disable Generational GC")
#endif
        || !op.addIntOption('\0', "available-memory", "SIZE",
                            "Select GC settings based on available memory (MB)", 0)
    )
    {
        return EXIT_FAILURE;
    }

    op.setArgTerminatesOptions("script", true);
    op.setArgCapturesRest("scriptArgs");

    switch (op.parseArgs(argc, argv)) {
      case OptionParser::ParseHelp:
        return EXIT_SUCCESS;
      case OptionParser::ParseError:
        op.printHelp(argv[0]);
        return EXIT_FAILURE;
      case OptionParser::Fail:
        return EXIT_FAILURE;
      case OptionParser::Okay:
        break;
    }

    if (op.getHelpOption())
        return EXIT_SUCCESS;

#ifdef DEBUG
    /*
     * Process OOM options as early as possible so that we can observe as many
     * allocations as possible.
     */
    if (op.getBoolOption('O'))
        OOM_printAllocationCount = true;

#if defined(JS_CPU_X86) && defined(JS_ION)
    if (op.getBoolOption("no-fpu"))
        JSC::MacroAssembler::SetFloatingPointDisabled();
#endif

#if (defined(JS_CPU_X86) || defined(JS_CPU_X64)) && defined(JS_ION)
    if (op.getBoolOption("no-sse3")) {
        JSC::MacroAssembler::SetSSE3Disabled();
        PropagateFlagToNestedShells("--no-sse3");
    }
    if (op.getBoolOption("no-sse4")) {
        JSC::MacroAssembler::SetSSE4Disabled();
        PropagateFlagToNestedShells("--no-sse4");
    }
#endif
#endif

    // Start the engine.
    if (!JS_Init())
        return 1;

    // When doing thread safety checks for VM accesses made during Ion compilation,
    // we rely on protected memory and only the main thread should be active.
    JSUseHelperThreads useHelperThreads =
        op.getBoolOption("ion-check-thread-safety")
        ? JS_NO_HELPER_THREADS
        : JS_USE_HELPER_THREADS;

    /* Use the same parameters as the browser in xpcjsruntime.cpp. */
    rt = JS_NewRuntime(32L * 1024L * 1024L, useHelperThreads);
    if (!rt)
        return 1;
    gTimeoutFunc = NullValue();
    if (!JS_AddNamedValueRootRT(rt, &gTimeoutFunc, "gTimeoutFunc"))
        return 1;

    JS_SetGCParameter(rt, JSGC_MAX_BYTES, 0xffffffff);
#ifdef JSGC_GENERATIONAL
    if (op.getBoolOption("no-ggc"))
        JS::DisableGenerationalGC(rt);
#endif

    size_t availMem = op.getIntOption("available-memory");
    if (availMem > 0)
        JS_SetGCParametersBasedOnAvailableMemory(rt, availMem);

    /* Set the initial counter to 1 so the principal will never be destroyed. */
    JSPrincipals shellTrustedPrincipals;
    shellTrustedPrincipals.refcount = 1;

    JS_SetTrustedPrincipals(rt, &shellTrustedPrincipals);
    JS_SetSecurityCallbacks(rt, &securityCallbacks);
    JS_SetOperationCallback(rt, ShellOperationCallback);
    JS::SetAsmJSCacheOps(rt, &asmJSCacheOps);

    JS_SetNativeStackQuota(rt, gMaxStackSize);

#ifdef JS_THREADSAFE
    if (!offThreadState.init())
        return 1;
#endif

    if (!InitWatchdog(rt))
        return 1;

    cx = NewContext(rt);
    if (!cx)
        return 1;

    JS_SetGCParameter(rt, JSGC_MODE, JSGC_MODE_INCREMENTAL);
    JS_SetGCParameterForThread(cx, JSGC_MAX_CODE_CACHE_BYTES, 16 * 1024 * 1024);

    js::SetPreserveWrapperCallback(rt, DummyPreserveWrapperCallback);

    result = Shell(cx, &op, envp);

#ifdef DEBUG
    if (OOM_printAllocationCount)
        printf("OOM max count: %u\n", OOM_counter);
#endif

    gTimeoutFunc = NullValue();
    JS_RemoveValueRootRT(rt, &gTimeoutFunc);

    DestroyContext(cx, true);

    KillWatchdog();

    JS_DestroyRuntime(rt);
    JS_ShutDown();
    return result;
}
