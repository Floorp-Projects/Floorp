/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* JS shell. */

#include "mozilla/DebugOnly.h"
#include "mozilla/GuardObjects.h"
#include "mozilla/Util.h"

#ifdef XP_WIN
# include <direct.h>
#endif
#include <errno.h>
#if defined(XP_OS2) || defined(XP_WIN)
# include <io.h>     /* for isatty() */
#endif
#include <locale.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef XP_UNIX
# include <sys/wait.h>
# include <sys/types.h>
# include <unistd.h>
#endif

#include "jsapi.h"
#include "jsarray.h"
#include "jsatom.h"
#include "jscntxt.h"
#include "jsdate.h"
#include "jsdbgapi.h"
#include "jsfun.h"
#include "jsgc.h"
#include "jsiter.h"
#include "jslock.h"
#include "jsnum.h"
#include "jsobj.h"
#include "json.h"
#include "jsprf.h"
#include "jsreflect.h"
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
#include "frontend/BytecodeEmitter.h"
#include "frontend/Parser.h"
#include "ion/Ion.h"
#include "perf/jsperf.h"
#include "shell/jsheaptools.h"
#include "shell/jsoptparse.h"
#include "vm/Shape.h"
#include "vm/TypedArrayObject.h"
#include "vm/WrapperObject.h"

#include "jsinferinlines.h"
#include "jsscriptinlines.h"

#include "vm/Interpreter-inl.h"

#ifdef XP_WIN
# define PATH_MAX (MAX_PATH > _MAX_DIR ? MAX_PATH : _MAX_DIR)
#else
# include <libgen.h>
#endif

using namespace js;
using namespace js::cli;

using mozilla::ArrayLength;
using mozilla::Maybe;

typedef enum JSShellExitCode {
    EXITCODE_RUNTIME_ERROR      = 3,
    EXITCODE_FILE_NOT_FOUND     = 4,
    EXITCODE_OUT_OF_MEMORY      = 5,
    EXITCODE_TIMEOUT            = 6
} JSShellExitCode;

size_t gStackChunkSize = 8192;

/*
 * Note: This limit should match the stack limit set by the browser in
 *       js/xpconnect/src/XPCJSRuntime.cpp
 */
#if defined(MOZ_ASAN) || (defined(DEBUG) && !defined(XP_WIN))
size_t gMaxStackSize = 2 * 128 * sizeof(size_t) * 1024;
#else
size_t gMaxStackSize = 128 * sizeof(size_t) * 1024;
#endif

#ifdef JS_THREADSAFE
static unsigned gStackBaseThreadIndex;
#else
static uintptr_t gStackBase;
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

static JSBool
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

static PRLock *gWatchdogLock = NULL;
static PRCondVar *gWatchdogWakeup = NULL;
static PRThread *gWatchdogThread = NULL;
static bool gWatchdogHasTimeout = false;
static int64_t gWatchdogTimeout = 0;

static PRCondVar *gSleepWakeup = NULL;

#else

static JSRuntime *gRuntime = NULL;

#endif

int gExitCode = 0;
bool gQuitting = false;
bool gGotError = false;
FILE *gErrFile = NULL;
FILE *gOutFile = NULL;

static bool reportWarnings = true;
static bool compileOnly = false;
static bool fuzzingSafe = false;

#ifdef DEBUG
static bool dumpEntrainedVariables = false;
static bool OOM_printAllocationCount = false;
#endif

typedef enum JSShellErrNum {
#define MSG_DEF(name, number, count, exception, format) \
    name = number,
#include "jsshell.msg"
#undef MSG_DEF
    JSShellErr_Limit
} JSShellErrNum;

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

static void
ReportException(JSContext *cx)
{
    if (JS_IsExceptionPending(cx)) {
        if (!JS_ReportPendingException(cx))
            JS_ClearPendingException(cx);
    }
}

class ToStringHelper
{
  public:
    ToStringHelper(JSContext *aCx, HandleValue v, bool aThrow = false)
      : cx(aCx), mStr(cx, JS_ValueToString(cx, v))
    {
        if (!aThrow && !mStr)
            ReportException(cx);
    }
    ToStringHelper(JSContext *aCx, HandleId id, bool aThrow = false)
      : cx(aCx), mStr(cx, JS_ValueToString(cx, IdToValue(id)))
    {
        if (!aThrow && !mStr)
            ReportException(cx);
    }
    bool threw() { return !mStr; }
    jsval getJSVal() { return STRING_TO_JSVAL(mStr); }
    const char *getBytes() {
        if (mStr && (mBytes.ptr() || mBytes.encodeLatin1(cx, mStr)))
            return mBytes.ptr();
        return "(error converting value)";
    }
  private:
    JSContext *cx;
    RootedString mStr;  // Objects of this class are always stack-allocated.
    JSAutoByteString mBytes;
};

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
            return NULL;
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
        return NULL;
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
            char *tmp = (char *) realloc(buffer, size);
            if (!tmp) {
                free(buffer);
                return NULL;
            }
            buffer = tmp;
        }
        current = buffer + len;
    }
    if (len && !ferror(file))
        return buffer;
    free(buffer);
    return NULL;
}

static char *
JSStringToUTF8(JSContext *cx, JSString *str)
{
    JSLinearString *linear = str->ensureLinear(cx);
    if (!linear)
        return NULL;

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
        return NULL;

    JSShellContextData *data = (JSShellContextData *)
                               calloc(sizeof(JSShellContextData), 1);
    if (!data)
        return NULL;
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

static JSBool
ShellOperationCallback(JSContext *cx)
{
    if (!gTimedOut)
        return true;

    JS_ClearPendingException(cx);

    bool result;
    if (!gTimeoutFunc.isNull()) {
        RootedValue returnedValue(cx);
        if (!JS_CallFunctionValue(cx, NULL, gTimeoutFunc, 0, NULL, returnedValue.address()))
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

static void
SetContextOptions(JSContext *cx)
{
    JS_SetOperationCallback(cx, ShellOperationCallback);
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
    uint32_t oldopts = JS_GetOptions(cx);
    gGotError = false;
    JS_SetOptions(cx, oldopts | JSOPTION_COMPILE_N_GO | JSOPTION_NO_SCRIPT_RVAL);
    CompileOptions options(cx);
    options.setUTF8(true)
        .setFileAndLine(filename, 1);

    RootedScript script(cx);
    script = JS::Compile(cx, obj, options, file);

#ifdef DEBUG
    if (dumpEntrainedVariables)
        AnalyzeEntrainedVariables(cx, script);
#endif

    JS_SetOptions(cx, oldopts);
    JS_ASSERT_IF(!script, gGotError);
    if (script && !compileOnly) {
        if (!JS_ExecuteScript(cx, obj, script, NULL)) {
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
    options.utf8 = true;
    options.compileAndGo = true;
    options.filename = "typein";
    options.lineno = lineno;
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
            JS_ReportErrorNumber(cx, my_GetErrorMessage, NULL,
                                 JSSMSG_CANT_OPEN, filename, strerror(errno));
            gExitCode = EXITCODE_FILE_NOT_FOUND;
            return;
        }
    }
    AutoCloseInputFile autoClose(file);

    SetContextOptions(cx);

    if (!forceTTY && !isatty(fileno(file))) {
        // It's not interactive - just execute it.
        RunFile(cx, obj, filename, file, compileOnly);
    } else {
        // It's an interactive filehandle; drop into read-eval-print loop.
        ReadEvalPrintLoop(cx, obj, file, gOutFile, compileOnly);
    }
}

/*
 * JSContext option name to flag map. The option names are in alphabetical
 * order for better reporting.
 */
static const struct JSOption {
    const char  *name;
    uint32_t    flag;
} js_options[] = {
    {"strict",          JSOPTION_EXTRA_WARNINGS},
    {"typeinfer",       JSOPTION_TYPE_INFERENCE},
    {"werror",          JSOPTION_WERROR},
    {"strict_mode",     JSOPTION_STRICT_MODE},
};

static uint32_t
MapContextOptionNameToFlag(JSContext* cx, const char* name)
{
    for (size_t i = 0; i < ArrayLength(js_options); ++i) {
        if (strcmp(name, js_options[i].name) == 0)
            return js_options[i].flag;
    }

    char* msg = JS_sprintf_append(NULL,
                                  "unknown option name '%s'."
                                  " The valid names are ", name);
    for (size_t i = 0; i < ArrayLength(js_options); ++i) {
        if (!msg)
            break;
        msg = JS_sprintf_append(msg, "%s%s", js_options[i].name,
                                (i + 2 < ArrayLength(js_options)
                                 ? ", "
                                 : i + 2 == ArrayLength(js_options)
                                 ? " and "
                                 : "."));
    }
    if (!msg) {
        JS_ReportOutOfMemory(cx);
    } else {
        JS_ReportError(cx, msg);
        free(msg);
    }
    return 0;
}

extern JSClass global_class;

static JSBool
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
            if (int32_t(fv) == fv)
                v = int32_t(fv);
        }
        if (v < 0 || v > JSVERSION_LATEST) {
            JS_ReportErrorNumber(cx, my_GetErrorMessage, NULL, JSSMSG_INVALID_ARGS, "version");
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
    JS_DescribeScriptedCaller(cx, script.address(), NULL);
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
        return NULL;

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
        return NULL;
    if (strcmp(script->filename(), "-e") == 0 || strcmp(script->filename(), "typein") == 0)
        scriptRelative = false;

    static char buffer[PATH_MAX+1];
    if (scriptRelative) {
#ifdef XP_WIN
        // The docs say it can return EINVAL, but the compiler says it's void
        _splitpath(script->filename(), NULL, buffer, NULL, NULL);
#else
        strncpy(buffer, script->filename(), PATH_MAX+1);
        if (buffer[PATH_MAX] != '\0')
            return NULL;

        // dirname(buffer) might return buffer, or it might return a
        // statically-allocated string
        memmove(buffer, dirname(buffer), strlen(buffer) + 1);
#endif
    } else {
        const char *cwd = getcwd(buffer, PATH_MAX);
        if (!cwd)
            return NULL;
    }

    size_t len = strlen(buffer);
    buffer[len] = '/';
    strncpy(buffer + len + 1, pathname, sizeof(buffer) - (len+1));
    if (buffer[PATH_MAX] != '\0')
        return NULL;

    return JS_NewStringCopyZ(cx, buffer);
}

static JSBool
Options(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    uint32_t flag;
    JSString *str;
    char *names;
    bool found;

    uint32_t optset = 0;
    for (unsigned i = 0; i < args.length(); i++) {
        str = JS_ValueToString(cx, args[i]);
        if (!str)
            return false;
        args[i].setString(str);
        JSAutoByteString opt(cx, str);
        if (!opt)
            return false;
        flag = MapContextOptionNameToFlag(cx, opt.ptr());
        if (!flag)
            return false;
        optset |= flag;
    }
    optset = JS_ToggleOptions(cx, optset);

    names = NULL;
    found = false;
    for (size_t i = 0; i < ArrayLength(js_options); i++) {
        if (js_options[i].flag & optset) {
            found = true;
            names = JS_sprintf_append(names, "%s%s",
                                      names ? "," : "", js_options[i].name);
            if (!names)
                break;
        }
    }
    if (!found)
        names = strdup("");
    if (!names) {
        JS_ReportOutOfMemory(cx);
        return false;
    }
    str = JS_NewStringCopyZ(cx, names);
    free(names);
    if (!str)
        return false;
    args.rval().setString(str);
    return true;
}

static JSBool
LoadScript(JSContext *cx, unsigned argc, jsval *vp, bool scriptRelative)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    RootedObject thisobj(cx, JS_THIS_OBJECT(cx, vp));
    if (!thisobj)
        return false;

    RootedString str(cx);
    for (unsigned i = 0; i < args.length(); i++) {
        str = JS_ValueToString(cx, args[i]);
        if (!str) {
            JS_ReportErrorNumber(cx, my_GetErrorMessage, NULL, JSSMSG_INVALID_ARGS, "load");
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
            !Evaluate(cx, thisobj, opts, filename.ptr(), NULL))
        {
            return false;
        }
    }

    args.rval().setUndefined();
    return true;
}

static JSBool
Load(JSContext *cx, unsigned argc, jsval *vp)
{
    return LoadScript(cx, argc, vp, false);
}

static JSBool
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
    AutoNewContext() : oldcx(NULL), newcx(NULL) {}

    bool enter(JSContext *cx) {
        JS_ASSERT(!JS_IsExceptionPending(cx));
        oldcx = cx;
        newcx = NewContext(JS_GetRuntime(cx));
        if (!newcx)
            return false;
        JS_SetOptions(newcx, JS_GetOptions(newcx) | JSOPTION_DONT_REPORT_UNCAUGHT);
        JS_SetGlobalObject(newcx, JS_GetGlobalForScopeChain(cx));

        newRequest.construct(newcx);
        newCompartment.construct(newcx, JS_GetGlobalForScopeChain(cx));
        return true;
    }

    JSContext *get() { return newcx; }

    ~AutoNewContext() {
        if (newcx) {
            RootedValue exc(oldcx);
            bool throwing = JS_IsExceptionPending(newcx);
            if (throwing)
                JS_GetPendingException(newcx, exc.address());
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

static JSBool
Evaluate(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (args.length() < 1 || args.length() > 2) {
        JS_ReportErrorNumber(cx, my_GetErrorMessage, NULL,
                             args.length() < 1 ? JSSMSG_NOT_ENOUGH_ARGS : JSSMSG_TOO_MANY_ARGS,
                             "evaluate");
        return false;
    }
    if (!args[0].isString() || (args.length() == 2 && args[1].isPrimitive())) {
        JS_ReportErrorNumber(cx, my_GetErrorMessage, NULL, JSSMSG_INVALID_ARGS, "evaluate");
        return false;
    }

    bool newContext = false;
    bool compileAndGo = true;
    bool noScriptRval = false;
    const char *fileName = "@evaluate";
    RootedObject element(cx);
    JSAutoByteString fileNameBytes;
    RootedString sourceMapURL(cx);
    unsigned lineNumber = 1;
    RootedObject global(cx, NULL);
    bool catchTermination = false;
    bool saveFrameChain = false;
    RootedObject callerGlobal(cx, cx->global());

    global = JS_GetGlobalForObject(cx, &args.callee());
    if (!global)
        return false;

    if (args.length() == 2) {
        RootedObject opts(cx, &args[1].toObject());
        RootedValue v(cx);

        if (!JS_GetProperty(cx, opts, "newContext", &v))
            return false;
        if (!JSVAL_IS_VOID(v)) {
            JSBool b;
            if (!JS_ValueToBoolean(cx, v, &b))
                return false;
            newContext = b;
        }

        if (!JS_GetProperty(cx, opts, "compileAndGo", &v))
            return false;
        if (!JSVAL_IS_VOID(v)) {
            JSBool b;
            if (!JS_ValueToBoolean(cx, v, &b))
                return false;
            compileAndGo = b;
        }

        if (!JS_GetProperty(cx, opts, "noScriptRval", &v))
            return false;
        if (!JSVAL_IS_VOID(v)) {
            JSBool b;
            if (!JS_ValueToBoolean(cx, v, &b))
                return false;
            noScriptRval = b;
        }

        if (!JS_GetProperty(cx, opts, "fileName", &v))
            return false;
        if (JSVAL_IS_NULL(v)) {
            fileName = NULL;
        } else if (!JSVAL_IS_VOID(v)) {
            JSString *s = JS_ValueToString(cx, v);
            if (!s)
                return false;
            fileName = fileNameBytes.encodeLatin1(cx, s);
            if (!fileName)
                return false;
        }

        if (!JS_GetProperty(cx, opts, "element", &v))
            return false;
        if (!JSVAL_IS_PRIMITIVE(v))
            element = JSVAL_TO_OBJECT(v);

        if (!JS_GetProperty(cx, opts, "sourceMapURL", &v))
            return false;
        if (!JSVAL_IS_VOID(v)) {
            sourceMapURL = JS_ValueToString(cx, v);
            if (!sourceMapURL)
                return false;
        }

        if (!JS_GetProperty(cx, opts, "lineNumber", &v))
            return false;
        if (!JSVAL_IS_VOID(v)) {
            uint32_t u;
            if (!JS_ValueToECMAUint32(cx, v, &u))
                return false;
            lineNumber = u;
        }

        if (!JS_GetProperty(cx, opts, "global", &v))
            return false;
        if (!JSVAL_IS_VOID(v)) {
            global = JSVAL_IS_PRIMITIVE(v) ? NULL : JSVAL_TO_OBJECT(v);
            if (global) {
                global = js::UncheckedUnwrap(global);
                if (!global)
                    return false;
            }
            if (!global || !(JS_GetClass(global)->flags & JSCLASS_IS_GLOBAL)) {
                JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_UNEXPECTED_TYPE,
                                     "\"global\" passed to evaluate()", "not a global object");
                return false;
            }
        }

        if (!JS_GetProperty(cx, opts, "catchTermination", &v))
            return false;
        if (!JSVAL_IS_VOID(v)) {
            JSBool b;
            if (!JS_ValueToBoolean(cx, v, &b))
                return false;
            catchTermination = b;
        }

        if (!JS_GetProperty(cx, opts, "saveFrameChain", &v))
            return false;
        if (!JSVAL_IS_VOID(v)) {
            JSBool b;
            if (!JS_ValueToBoolean(cx, v, &b))
                return false;
            saveFrameChain = b;
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
        uint32_t oldopts = JS_GetOptions(cx);
        uint32_t opts = oldopts & ~(JSOPTION_COMPILE_N_GO | JSOPTION_NO_SCRIPT_RVAL);
        if (compileAndGo)
            opts |= JSOPTION_COMPILE_N_GO;
        if (noScriptRval)
            opts |= JSOPTION_NO_SCRIPT_RVAL;

        JS_SetOptions(cx, opts);
        CompileOptions options(cx);
        options.setFileAndLine(fileName, lineNumber);
        options.setElement(element);
        RootedScript script(cx, JS::Compile(cx, global, options, codeChars, codeLength));
        JS_SetOptions(cx, oldopts);
        if (!script)
            return false;

        if (sourceMapURL && !script->scriptSource()->hasSourceMap()) {
            const jschar *smurl = JS_GetStringCharsZ(cx, sourceMapURL);
            if (!smurl)
                return false;
            jschar *smurl_copy = js_strdup(cx, smurl);
            if (!smurl_copy || !script->scriptSource()->setSourceMap(cx, smurl_copy))
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

    return JS_WrapValue(cx, vp);
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
        return NULL;
    }

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
                        return NULL;
                    }
                    str = JS_NewUCStringCopyN(cx, ucbuf, len);
                    free(ucbuf);
                }
                JS_free(cx, buf);
            }
        }
    }
    fclose(file);

    return str;
}

static JSObject *
FileAsTypedArray(JSContext *cx, const char *pathname)
{
    FILE *file = fopen(pathname, "rb");
    if (!file) {
        JS_ReportError(cx, "can't open %s: %s", pathname, strerror(errno));
        return NULL;
    }

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
                return NULL;
            char *buf = (char *) obj->as<TypedArrayObject>().viewData();
            size_t cc = fread(buf, 1, len, file);
            if (cc != len) {
                JS_ReportError(cx, "can't read %s: %s", pathname,
                               (ptrdiff_t(cc) < 0) ? strerror(errno) : "short read");
                obj = NULL;
            }
        }
    }
    fclose(file);
    return obj;
}

/*
 * Function to run scripts and return compilation + execution time. Semantics
 * are closely modelled after the equivalent function in WebKit, as this is used
 * to produce benchmark timings by SunSpider.
 */
static JSBool
Run(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    if (args.length() != 1) {
        JS_ReportErrorNumber(cx, my_GetErrorMessage, NULL, JSSMSG_INVALID_ARGS, "run");
        return false;
    }

    RootedObject thisobj(cx, JS_THIS_OBJECT(cx, vp));
    if (!thisobj)
        return false;

    JSString *str = JS_ValueToString(cx, args[0]);
    if (!str)
        return false;
    args[0].setString(str);
    JSAutoByteString filename(cx, str);
    if (!filename)
        return false;

    const jschar *ucbuf = NULL;
    size_t buflen;
    str = FileAsString(cx, filename.ptr());
    if (str)
        ucbuf = JS_GetStringCharsAndLength(cx, str, &buflen);
    if (!ucbuf)
        return false;

    JS::Anchor<JSString *> a_str(str);
    uint32_t oldopts = JS_GetOptions(cx);
    JS_SetOptions(cx, oldopts | JSOPTION_COMPILE_N_GO | JSOPTION_NO_SCRIPT_RVAL);

    int64_t startClock = PRMJ_Now();
    RootedScript script(cx, JS_CompileUCScript(cx, thisobj, ucbuf, buflen, filename.ptr(), 1));
    JS_SetOptions(cx, oldopts);
    if (!script || !JS_ExecuteScript(cx, thisobj, script, NULL))
        return false;

    int64_t endClock = PRMJ_Now();
    args.rval().setDouble((endClock - startClock) / double(PRMJ_USEC_PER_MSEC));
    return true;
}

/*
 * function readline()
 * Provides a hook for scripts to read a line from stdin.
 */
static JSBool
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
            tmp = NULL;
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

static JSBool
PutStr(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (args.length() != 0) {
        JSString *str = JS_ValueToString(cx, args[0]);
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

static JSBool
Now(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    double now = PRMJ_Now() / double(PRMJ_USEC_PER_MSEC);
    args.rval().setDouble(now);
    return true;
}

static JSBool
PrintInternal(JSContext *cx, const CallArgs &args, FILE *file)
{
    for (unsigned i = 0; i < args.length(); i++) {
        JSString *str = JS_ValueToString(cx, args[i]);
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

static JSBool
Print(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return PrintInternal(cx, args, gOutFile);
}

static JSBool
PrintErr(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return PrintInternal(cx, args, gErrFile);
}

static JSBool
Help(JSContext *cx, unsigned argc, jsval *vp);

static JSBool
Quit(JSContext *cx, unsigned argc, jsval *vp)
{
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

static JSBool
AssertEq(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    if (!(args.length() == 2 || (args.length() == 3 && args[2].isString()))) {
        JS_ReportErrorNumber(cx, my_GetErrorMessage, NULL,
                             (args.length() < 2)
                             ? JSSMSG_NOT_ENOUGH_ARGS
                             : (args.length() == 3)
                             ? JSSMSG_INVALID_ARGS
                             : JSSMSG_TOO_MANY_ARGS,
                             "assertEq");
        return false;
    }

    JSBool same;
    if (!JS_SameValue(cx, args[0], args[1], &same))
        return false;
    if (!same) {
        JSAutoByteString bytes0, bytes1;
        const char *actual = ToSource(cx, args[0], &bytes0);
        const char *expected = ToSource(cx, args[1], &bytes1);
        if (args.length() == 2) {
            JS_ReportErrorNumber(cx, my_GetErrorMessage, NULL, JSSMSG_ASSERT_EQ_FAILED,
                                 actual, expected);
        } else {
            JSAutoByteString bytes2(cx, args[2].toString());
            if (!bytes2)
                return false;
            JS_ReportErrorNumber(cx, my_GetErrorMessage, NULL, JSSMSG_ASSERT_EQ_FAILED_MSG,
                                 actual, expected, bytes2.ptr());
        }
        return false;
    }
    args.rval().setUndefined();
    return true;
}

static JSScript *
ValueToScript(JSContext *cx, jsval v, JSFunction **funp = NULL)
{
    RootedFunction fun(cx, JS_ValueToFunction(cx, v));
    if (!fun)
        return NULL;

    // Unwrap bound functions.
    while (fun->isBoundFunction()) {
        JSObject *target = fun->getBoundFunctionTarget();
        if (target && target->is<JSFunction>())
            fun = &target->as<JSFunction>();
        else
            break;
    }

    if (!fun->isInterpreted()) {
        JS_ReportErrorNumber(cx, my_GetErrorMessage, NULL, JSSMSG_SCRIPTS_ONLY);
        return NULL;
    }

    JSScript *script = fun->getOrCreateScript(cx);
    if (!script)
        return NULL;

    if (fun && funp)
        *funp = fun;

    return script;
}

static JSBool
SetDebug(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    if (args.length() == 0 || !args[0].isBoolean()) {
        JS_ReportErrorNumber(cx, my_GetErrorMessage, NULL,
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

static JSBool
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
            if (!JS_ValueToInt32(cx, argv[intarg], ip))
                return false;
            if ((uint32_t)*ip >= script->length) {
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
    JSAbstractFramePtr frame(Jsvalify(iter.abstractFramePtr()));
    RootedScript script(cx, iter.script());

    size_t length;
    const jschar *chars = JS_GetStringCharsAndLength(cx, str, &length);
    if (!chars)
        return JSTRAP_ERROR;

    if (!frame.evaluateUCInStackFrame(cx, chars, length,
                                      script->filename(),
                                      script->lineno,
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

static JSBool
Trap(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    RootedScript script(cx);
    int32_t i;

    if (args.length() == 0) {
        JS_ReportErrorNumber(cx, my_GetErrorMessage, NULL, JSSMSG_TRAP_USAGE);
        return false;
    }
    argc = args.length() - 1;
    RootedString str(cx, JS_ValueToString(cx, args[argc]));
    if (!str)
        return false;
    args[argc].setString(str);
    if (!GetScriptAndPCArgs(cx, argc, args.array(), &script, &i))
        return false;
    if (uint32_t(i) >= script->length) {
        JS_ReportErrorNumber(cx, my_GetErrorMessage, NULL, JSSMSG_TRAP_USAGE);
        return false;
    }
    args.rval().setUndefined();
    return JS_SetTrap(cx, script, script->code + i, TrapHandler, STRING_TO_JSVAL(str));
}

static JSBool
Untrap(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    RootedScript script(cx);
    int32_t i;

    if (!GetScriptAndPCArgs(cx, args.length(), args.array(), &script, &i))
        return false;
    JS_ClearTrap(cx, script, script->code + i, NULL, NULL);
    args.rval().setUndefined();
    return true;
}

static JSTrapStatus
DebuggerAndThrowHandler(JSContext *cx, JSScript *script, jsbytecode *pc, jsval *rval,
                        void *closure)
{
    return TrapHandler(cx, script, pc, rval, STRING_TO_JSVAL((JSString *)closure));
}

static JSBool
SetDebuggerHandler(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    if (args.length() == 0) {
        JS_ReportErrorNumber(cx, my_GetErrorMessage, NULL,
                             JSSMSG_NOT_ENOUGH_ARGS, "setDebuggerHandler");
        return false;
    }

    JSString *str = JS_ValueToString(cx, args[0]);
    if (!str)
        return false;

    JS_SetDebuggerHandler(cx->runtime(), DebuggerAndThrowHandler, str);
    args.rval().setUndefined();
    return true;
}

static JSBool
SetThrowHook(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    JSString *str;
    if (args.length() == 0) {
        JS_ReportErrorNumber(cx, my_GetErrorMessage, NULL,
                             JSSMSG_NOT_ENOUGH_ARGS, "setThrowHook");
        return false;
    }

    str = JS_ValueToString(cx, args[0]);
    if (!str)
        return false;

    JS_SetThrowHook(cx->runtime(), DebuggerAndThrowHandler, str);
    args.rval().setUndefined();
    return true;
}

static JSBool
LineToPC(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    RootedScript script(cx);
    int32_t lineArg = 0;
    uint32_t lineno;
    jsbytecode *pc;

    if (args.length() == 0) {
        JS_ReportErrorNumber(cx, my_GetErrorMessage, NULL, JSSMSG_LINE2PC_USAGE);
        return false;
    }
    script = GetTopScript(cx);
    jsval v = args[0];
    if (!JSVAL_IS_PRIMITIVE(v) &&
        JS_GetClass(&v.toObject()) == Jsvalify(&JSFunction::class_))
    {
        script = ValueToScript(cx, v);
        if (!script)
            return false;
        lineArg++;
    }
    if (!JS_ValueToECMAUint32(cx, args[lineArg], &lineno))
        return false;
    pc = JS_LineNumberToPC(cx, script, lineno);
    if (!pc)
        return false;
    args.rval().setInt32(pc - script->code);
    return true;
}

static JSBool
PCToLine(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    RootedScript script(cx);
    int32_t i;
    unsigned lineno;

    if (!GetScriptAndPCArgs(cx, args.length(), args.array(), &script, &i))
        return false;
    lineno = JS_PCToLineNumber(cx, script, script->code + i);
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

    pc = script->code + offset;
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

    *start = (unsigned)(pc - script->code);
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
    unsigned lineno = script->lineno;
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
          case SRC_HIDDEN:
          case SRC_CATCH:
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
            Sprint(sp, " closingjump %u", unsigned(js_GetSrcNoteOffset(sn, 0)));
            break;

          case SRC_COND:
          case SRC_WHILE:
          case SRC_NEXTCASE:
            Sprint(sp, " offset %u", unsigned(js_GetSrcNoteOffset(sn, 0)));
            break;

          case SRC_TABLESWITCH: {
            JSOp op = JSOp(script->code[offset]);
            JS_ASSERT(op == JSOP_TABLESWITCH);
            Sprint(sp, " length %u", unsigned(js_GetSrcNoteOffset(sn, 0)));
            UpdateSwitchTableBounds(cx, script, offset,
                                    &switchTableStart, &switchTableEnd);
            break;
          }
          case SRC_CONDSWITCH: {
            JSOp op = JSOp(script->code[offset]);
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
            JS_ASSERT(JSOp(script->code[offset]) == JSOP_TRY);
            Sprint(sp, " offset to jump %u", unsigned(js_GetSrcNoteOffset(sn, 0)));
            break;

          default:
            JS_ASSERT(0);
            break;
        }
        Sprint(sp, "\n");
    }
}

static JSBool
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

static JSBool
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

static JSBool
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

static JSBool
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

static JSBool
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

    JSString *str = JS_ValueToString(cx, p.argv[0]);
    if (!str)
        return false;
    JSAutoByteString filename(cx, str);
    if (!filename)
        return false;

    uint32_t oldopts = JS_GetOptions(cx);
    JS_SetOptions(cx, oldopts | JSOPTION_COMPILE_N_GO | JSOPTION_NO_SCRIPT_RVAL);
    CompileOptions options(cx);
    options.setUTF8(true)
           .setFileAndLine(filename.ptr(), 1);
    RootedScript script (cx, JS::Compile(cx, thisobj, options, filename.ptr()));
    JS_SetOptions(cx, oldopts);
    if (!script)
        return false;

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

static JSBool
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
            JS_ReportErrorNumber(cx, my_GetErrorMessage, NULL,
                                 JSSMSG_FILE_SCRIPTS_ONLY);
            return false;
        }

        file = fopen(script->filename(), "r");
        if (!file) {
            JS_ReportErrorNumber(cx, my_GetErrorMessage, NULL,
                                 JSSMSG_CANT_OPEN, script->filename(),
                                 strerror(errno));
            return false;
        }

        pc = script->code;
        end = pc + script->length;

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
                        JS_ReportErrorNumber(cx, my_GetErrorMessage, NULL,
                                             JSSMSG_UNEXPECTED_EOF,
                                             script->filename());
                        ok = false;
                        goto bail;
                    }
                    line1++;
                    Sprint(&sprinter, "%s %3u: %s", sep, line1, linebuf);
                }
            }

            len = js_Disassemble1(cx, script, pc, pc - script->code, true, &sprinter);
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

static JSBool
DumpHeap(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    jsval v;
    void* startThing;
    JSGCTraceKind startTraceKind;
    const char *badTraceArg;
    void *thingToFind;
    size_t maxDepth;
    void *thingToIgnore;
    FILE *dumpFile;
    bool ok;

    const char *fileName = NULL;
    JSAutoByteString fileNameBytes;
    if (args.length() > 0) {
        v = args[0];
        if (!v.isNull()) {
            JSString *str;

            str = JS_ValueToString(cx, v);
            if (!str)
                return false;
            args[0].setString(str);
            if (!fileNameBytes.encodeLatin1(cx, str))
                return false;
            fileName = fileNameBytes.ptr();
        }
    }

    // Grab the depth param first, because JS_ValueToECMAUint32 can GC, and
    // there's no easy way to root the traceable void* parameters below.
    maxDepth = (size_t)-1;
    if (args.length() > 3) {
        v = args[3];
        if (!v.isNull()) {
            uint32_t depth;

            if (!JS_ValueToECMAUint32(cx, v, &depth))
                return false;
            maxDepth = depth;
        }
    }

    startThing = NULL;
    startTraceKind = JSTRACE_OBJECT;
    if (args.length() > 1) {
        v = args[1];
        if (v.isMarkable()) {
            startThing = JSVAL_TO_TRACEABLE(v);
            startTraceKind = v.gcKind();
        } else if (!v.isNull()) {
            badTraceArg = "start";
            goto not_traceable_arg;
        }
    }

    thingToFind = NULL;
    if (args.length() > 2) {
        v = args[2];
        if (v.isMarkable()) {
            thingToFind = JSVAL_TO_TRACEABLE(v);
        } else if (!v.isNull()) {
            badTraceArg = "toFind";
            goto not_traceable_arg;
        }
    }

    thingToIgnore = NULL;
    if (args.length() > 4) {
        v = args[4];
        if (v.isMarkable()) {
            thingToIgnore = JSVAL_TO_TRACEABLE(v);
        } else if (!v.isNull()) {
            badTraceArg = "toIgnore";
            goto not_traceable_arg;
        }
    }

    if (!fileName) {
        dumpFile = stdout;
    } else {
        dumpFile = fopen(fileName, "w");
        if (!dumpFile) {
            JS_ReportError(cx, "can't open %s: %s", fileName, strerror(errno));
            return false;
        }
    }

    ok = JS_DumpHeap(JS_GetRuntime(cx), dumpFile, startThing, startTraceKind, thingToFind,
                     maxDepth, thingToIgnore);
    if (dumpFile != stdout)
        fclose(dumpFile);
    if (!ok) {
        JS_ReportOutOfMemory(cx);
        return false;
    }
    args.rval().setUndefined();
    return true;

  not_traceable_arg:
    JS_ReportError(cx, "argument '%s' is not null or a heap-allocated thing",
                   badTraceArg);
    return false;
}

static JSBool
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

static JSBool
BuildDate(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    fprintf(gOutFile, "built on %s at %s\n", __DATE__, __TIME__);
    args.rval().setUndefined();
    return true;
}

static JSBool
Intern(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    JSString *str = JS_ValueToString(cx, args.length() == 0 ? UndefinedValue() : args[0]);
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

static JSBool
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
        RootedObject obj(cx, JSVAL_IS_PRIMITIVE(args[0]) ? NULL : &args[0].toObject());

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
        if (fun->hasScript() && fun->nonLazyScript()->compileAndGo) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_UNEXPECTED_TYPE,
                                 "function", "compile-and-go");
            return false;
        }
    }

    if (argc > 1) {
        if (!JS_ValueToObject(cx, args[1], parent.address()))
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

static JSBool
GetPDA(JSContext *cx, unsigned argc, jsval *vp)
{
    RootedObject vobj(cx);
    bool ok;
    JSPropertyDescArray pda;
    JSPropertyDesc *pd;
    jsval v;

    if (!JS_ValueToObject(cx, argc == 0 ? UndefinedValue() : vp[2], vobj.address()))
        return false;
    if (!vobj) {
        JS_SET_RVAL(cx, vp, UndefinedValue());
        return true;
    }

    RootedObject aobj(cx, JS_NewArrayObject(cx, 0, NULL));
    if (!aobj)
        return false;
    JS_SET_RVAL(cx, vp, OBJECT_TO_JSVAL(aobj));

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
        pdobj = JS_NewObject(cx, NULL, NULL, NULL);
        if (!pdobj) {
            ok = false;
            break;
        }

        /* Protect pdobj from GC by setting it as an element of aobj now */
        v = OBJECT_TO_JSVAL(pdobj);
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

static JSBool
GetSLX(JSContext *cx, unsigned argc, jsval *vp)
{
    RootedScript script(cx);

    script = ValueToScript(cx, argc == 0 ? UndefinedValue() : vp[2]);
    if (!script)
        return false;
    JS_SET_RVAL(cx, vp, INT_TO_JSVAL(js_GetScriptLineExtent(script)));
    return true;
}

static JSBool
ToInt32(JSContext *cx, unsigned argc, jsval *vp)
{
    int32_t i;

    if (!JS_ValueToInt32(cx, argc == 0 ? UndefinedValue() : vp[2], &i))
        return false;
    JS_SET_RVAL(cx, vp, JS_NumberValue(i));
    return true;
}

static JSBool
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

static JSBool
sandbox_enumerate(JSContext *cx, HandleObject obj)
{
    RootedValue v(cx);
    JSBool b;

    if (!JS_GetProperty(cx, obj, "lazy", &v))
        return false;

    JS_ValueToBoolean(cx, v, &b);
    return !b || JS_EnumerateStandardClasses(cx, obj);
}

static JSBool
sandbox_resolve(JSContext *cx, HandleObject obj, HandleId id, unsigned flags,
                MutableHandleObject objp)
{
    RootedValue v(cx);
    JSBool b, resolved;

    if (!JS_GetProperty(cx, obj, "lazy", &v))
        return false;

    JS_ValueToBoolean(cx, v, &b);
    if (b) {
        if (!JS_ResolveStandardClass(cx, obj, id, &resolved))
            return false;
        if (resolved) {
            objp.set(obj);
            return true;
        }
    }
    objp.set(NULL);
    return true;
}

static JSClass sandbox_class = {
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
    RootedObject obj(cx, JS_NewGlobalObject(cx, &sandbox_class, NULL));
    if (!obj)
        return NULL;

    {
        JSAutoCompartment ac(cx, obj);
        if (!lazy && !JS_InitStandardClasses(cx, obj))
            return NULL;

        RootedValue value(cx, BooleanValue(lazy));
        if (!JS_SetProperty(cx, obj, "lazy", value))
            return NULL;
    }

    if (!cx->compartment()->wrap(cx, obj.address()))
        return NULL;
    return obj;
}

static JSBool
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

    JS_DescribeScriptedCaller(cx, script.address(), &lineno);
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

static JSBool
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
                                     NULL, JSMSG_NEED_DEBUG_MODE);
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

    JSAbstractFramePtr frame(Jsvalify(fi.abstractFramePtr()));
    RootedScript fpscript(cx, frame.script());
    bool ok = !!frame.evaluateUCInStackFrame(cx, chars, length,
                                             fpscript->filename(),
                                             JS_PCToLineNumber(cx, fpscript,
                                                               fi.pc()),
                                             MutableHandleValue::fromMarkedLocation(vp));
    return ok;
}

static JSBool
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
static JSBool
CopyProperty(JSContext *cx, HandleObject obj, HandleObject referent, HandleId id,
             unsigned lookupFlags, MutableHandleObject objp)
{
    RootedShape shape(cx);
    AutoPropertyDescriptorRooter desc(cx);
    unsigned propFlags = 0;
    RootedObject obj2(cx);

    objp.set(NULL);
    if (referent->isNative()) {
        if (!LookupPropertyWithFlags(cx, referent, id, lookupFlags, &obj2, &shape))
            return false;
        if (obj2 != referent)
            return true;

        if (shape->hasSlot()) {
            desc.value = referent->nativeGetSlot(shape->slot());
        } else {
            desc.value.setUndefined();
        }

        desc.attrs = shape->attributes();
        desc.getter = shape->getter();
        if (!desc.getter && !(desc.attrs & JSPROP_GETTER))
            desc.getter = JS_PropertyStub;
        desc.setter = shape->setter();
        if (!desc.setter && !(desc.attrs & JSPROP_SETTER))
            desc.setter = JS_StrictPropertyStub;
        desc.shortid = shape->shortid();
        propFlags = shape->getFlags();
    } else if (referent->is<ProxyObject>()) {
        if (!Proxy::getOwnPropertyDescriptor(cx, referent, id, &desc, 0))
            return false;
        if (!desc.obj)
            return true;
    } else {
        if (!JSObject::lookupGeneric(cx, referent, id, objp, &shape))
            return false;
        if (objp != referent)
            return true;
        RootedValue value(cx);
        if (!JSObject::getGeneric(cx, referent, referent, id, &value) ||
            !JSObject::getGenericAttributes(cx, referent, id, &desc.attrs))
        {
            return false;
        }
        desc.value = value;
        desc.attrs &= JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT;
        desc.getter = JS_PropertyStub;
        desc.setter = JS_StrictPropertyStub;
        desc.shortid = 0;
    }

    RootedValue value(cx, desc.value);
    objp.set(obj);
    return DefineNativeProperty(cx, obj, id, value, desc.getter, desc.setter,
                                desc.attrs, propFlags, desc.shortid);
}

static JSBool
resolver_resolve(JSContext *cx, HandleObject obj, HandleId id, unsigned flags,
                 MutableHandleObject objp)
{
    jsval v = JS_GetReservedSlot(obj, 0);
    Rooted<JSObject*> vobj(cx, &v.toObject());
    return CopyProperty(cx, obj, vobj, id, flags, objp);
}

static JSBool
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

static JSClass resolver_class = {
    "resolver",
    JSCLASS_NEW_RESOLVE | JSCLASS_HAS_RESERVED_SLOTS(1),
    JS_PropertyStub,   JS_DeletePropertyStub,
    JS_PropertyStub,   JS_StrictPropertyStub,
    resolver_enumerate, (JSResolveOp)resolver_resolve,
    JS_ConvertStub
};


static JSBool
Resolver(JSContext *cx, unsigned argc, jsval *vp)
{
    RootedObject referent(cx, NULL);
    RootedObject proto(cx, NULL);
    if (!JS_ConvertArguments(cx, argc, JS_ARGV(cx, vp), "o/o", &referent, &proto))
        return false;

    RootedObject parent(cx, JS_GetParent(referent));
    JSObject *result = (argc > 1
                        ? JS_NewObjectWithGivenProto
                        : JS_NewObject)(cx, &resolver_class, proto, parent);
    if (!result)
        return false;

    JS_SetReservedSlot(result, 0, OBJECT_TO_JSVAL(referent));
    JS_SET_RVAL(cx, vp, OBJECT_TO_JSVAL(result));
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

static JSBool
Sleep_fn(JSContext *cx, unsigned argc, jsval *vp)
{
    int64_t t_ticks;

    if (argc == 0) {
        t_ticks = 0;
    } else {
        double t_secs;

        if (!JS_ValueToNumber(cx, argc == 0 ? UndefinedValue() : vp[2], &t_secs))
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
        gWatchdogThread = NULL;
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
        DeleteTimerQueueTimer(NULL, gTimerHandle, NULL);
        gTimerHandle = 0;
    }
    if (t > 0 &&
        !CreateTimerQueueTimer(&gTimerHandle,
                               NULL,
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
        signal(SIGALRM, NULL);
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

static JSBool
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

static JSBool
Timeout(JSContext *cx, unsigned argc, jsval *vp)
{
    if (argc == 0) {
        JS_SET_RVAL(cx, vp, JS_NumberValue(gTimeoutInterval));
        return true;
    }

    if (argc > 2) {
        JS_ReportError(cx, "Wrong number of arguments");
        return false;
    }

    double t;
    if (!JS_ValueToNumber(cx, JS_ARGV(cx, vp)[0], &t))
        return false;

    if (argc > 1) {
        RootedValue value(cx, JS_ARGV(cx, vp)[1]);
        if (!value.isObject() || !value.toObject().is<JSFunction>()) {
            JS_ReportError(cx, "Second argument must be a timeout function");
            return false;
        }
        gTimeoutFunc = value;
    }

    JS_SET_RVAL(cx, vp, UndefinedValue());
    return SetTimeoutValue(cx, t);
}

static JSBool
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

static JSBool
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

static JSBool
Compile(JSContext *cx, unsigned argc, jsval *vp)
{
    if (argc < 1) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_MORE_ARGS_NEEDED,
                             "compile", "0", "s");
        return false;
    }
    jsval arg0 = JS_ARGV(cx, vp)[0];
    if (!JSVAL_IS_STRING(arg0)) {
        const char *typeName = JS_GetTypeName(cx, JS_TypeOfValue(cx, arg0));
        JS_ReportError(cx, "expected string to compile, got %s", typeName);
        return false;
    }

    RootedObject global(cx, JS_GetGlobalForScopeChain(cx));
    JSString *scriptContents = JSVAL_TO_STRING(arg0);
    unsigned oldopts = JS_GetOptions(cx);
    JS_SetOptions(cx, oldopts | JSOPTION_COMPILE_N_GO | JSOPTION_NO_SCRIPT_RVAL);
    bool ok = JS_CompileUCScript(cx, global, JS_GetStringCharsZ(cx, scriptContents),
                                 JS_GetStringLength(scriptContents), "<string>", 1);
    JS_SetOptions(cx, oldopts);

    JS_SET_RVAL(cx, vp, UndefinedValue());
    return ok;
}

static JSBool
Parse(JSContext *cx, unsigned argc, jsval *vp)
{
    using namespace js::frontend;

    CallArgs args = CallArgsFromVp(argc, vp);

    if (args.length() < 1) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_MORE_ARGS_NEEDED,
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
                                    /* foldConstants = */ true, NULL, NULL);

    ParseNode *pn = parser.parse(NULL);
    if (!pn)
        return false;
#ifdef DEBUG
    DumpParseTree(pn);
    fputc('\n', stderr);
#endif
    args.rval().setUndefined();
    return true;
}

static JSBool
SyntaxParse(JSContext *cx, unsigned argc, jsval *vp)
{
    using namespace js::frontend;

    CallArgs args = CallArgsFromVp(argc, vp);

    if (args.length() < 1) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_MORE_ARGS_NEEDED,
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
                                                options, chars, length, false, NULL, NULL);

    bool succeeded = parser.parse(NULL);
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

static JSBool
OffThreadCompileScript(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (args.length() < 1) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_MORE_ARGS_NEEDED,
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
           .setSourcePolicy(CompileOptions::NO_SOURCE);

    const jschar *chars = JS_GetStringCharsZ(cx, scriptContents);
    if (!chars)
        return false;
    size_t length = JS_GetStringLength(scriptContents);

    // Prevent the string contents from ever being GC'ed. This will leak memory
    // but since the compiled script is never consumed there isn't much choice.
    JSString **permanentRoot = cx->new_<JSString *>();
    if (!permanentRoot)
        return false;
    *permanentRoot = scriptContents;
    if (!JS_AddStringRoot(cx, permanentRoot))
        return false;

    if (!StartOffThreadParseScript(cx, options, chars, length))
        return false;

    args.rval().setUndefined();
    return true;
}

#endif // JS_THREADSAFE

struct FreeOnReturn
{
    JSContext *cx;
    const char *ptr;
    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER

    FreeOnReturn(JSContext *cx, const char *ptr = NULL
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

static JSBool
ReadFile(JSContext *cx, unsigned argc, jsval *vp, bool scriptRelative)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (args.length() < 1 || args.length() > 2) {
        JS_ReportErrorNumber(cx, my_GetErrorMessage, NULL,
                             args.length() < 1 ? JSSMSG_NOT_ENOUGH_ARGS : JSSMSG_TOO_MANY_ARGS,
                             "snarf");
        return false;
    }

    if (!args[0].isString() || (args.length() == 2 && !args[1].isString())) {
        JS_ReportErrorNumber(cx, my_GetErrorMessage, NULL, JSSMSG_INVALID_ARGS, "snarf");
        return false;
    }

    RootedString givenPath(cx, args[0].toString());
    RootedString str(cx, ResolvePath(cx, givenPath, scriptRelative));
    JSAutoByteString filename(cx, str);
    if (!filename)
        return false;

    if (args.length() > 1) {
        JSString *opt = JS_ValueToString(cx, args[1]);
        if (!opt)
            return false;
        JSBool match;
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

static JSBool
Snarf(JSContext *cx, unsigned argc, jsval *vp)
{
    return ReadFile(cx, argc, vp, false);
}

static JSBool
ReadRelativeToScript(JSContext *cx, unsigned argc, jsval *vp)
{
    return ReadFile(cx, argc, vp, true);
}

static JSBool
System(JSContext *cx, unsigned argc, jsval *vp)
{
    JSString *str;

    if (argc != 1) {
        JS_ReportErrorNumber(cx, my_GetErrorMessage, NULL, JSSMSG_INVALID_ARGS,
                             "system");
        return false;
    }

    str = JS_ValueToString(cx, JS_ARGV(cx, vp)[0]);
    if (!str)
        return false;
    JSAutoByteString command(cx, str);
    if (!command)
        return false;

    int result = system(command.ptr());

    JS_SET_RVAL(cx, vp, Int32Value(result));
    return true;
}

static bool
DecompileFunctionSomehow(JSContext *cx, unsigned argc, Value *vp,
                         JSString *(*decompiler)(JSContext *, JSFunction *, unsigned))
{
    CallArgs args = CallArgsFromVp(argc, vp);
    if (args.length() < 1 || !args[0].isObject() || !args[0].toObject().is<JSFunction>()) {
        args.rval().setUndefined();
        return true;
    }
    JSString *result = decompiler(cx, &args[0].toObject().as<JSFunction>(), 0);
    if (!result)
        return false;
    args.rval().setString(result);
    return true;
}

static JSBool
DecompileBody(JSContext *cx, unsigned argc, Value *vp)
{
    return DecompileFunctionSomehow(cx, argc, vp, JS_DecompileFunctionBody);
}

static JSBool
DecompileFunction(JSContext *cx, unsigned argc, Value *vp)
{
    return DecompileFunctionSomehow(cx, argc, vp, JS_DecompileFunction);
}

static JSBool
DecompileThisScript(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    RootedScript script (cx);
    if (!JS_DescribeScriptedCaller(cx, script.address(), NULL)) {
        args.rval().setString(cx->runtime()->emptyString);
        return true;
    }
    JSString *result = JS_DecompileScript(cx, script, "test", 0);
    if (!result)
        return false;
    args.rval().setString(result);
    return true;
}

static JSBool
ThisFilename(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    RootedScript script (cx);
    if (!JS_DescribeScriptedCaller(cx, script.address(), NULL) || !script->filename()) {
        args.rval().setString(cx->runtime()->emptyString);
        return true;
    }
    JSString *filename = JS_NewStringCopyZ(cx, script->filename());
    if (!filename)
        return false;
    args.rval().setString(filename);
    return true;
}

static JSBool
Wrap(JSContext *cx, unsigned argc, jsval *vp)
{
    jsval v = argc > 0 ? JS_ARGV(cx, vp)[0] : UndefinedValue();
    if (JSVAL_IS_PRIMITIVE(v)) {
        JS_SET_RVAL(cx, vp, v);
        return true;
    }

    RootedObject obj(cx, JSVAL_TO_OBJECT(v));
    RootedObject proto(cx);
    if (!JSObject::getProto(cx, obj, &proto))
        return false;
    JSObject *wrapped = Wrapper::New(cx, obj, proto, &obj->global(),
                                     &Wrapper::singleton);
    if (!wrapped)
        return false;

    JS_SET_RVAL(cx, vp, OBJECT_TO_JSVAL(wrapped));
    return true;
}

static JSBool
WrapWithProto(JSContext *cx, unsigned argc, jsval *vp)
{
    Value obj = UndefinedValue(), proto = UndefinedValue();
    if (argc == 2) {
        obj = JS_ARGV(cx, vp)[0];
        proto = JS_ARGV(cx, vp)[1];
    }
    if (!obj.isObject() || !proto.isObjectOrNull()) {
        JS_ReportErrorNumber(cx, my_GetErrorMessage, NULL, JSSMSG_INVALID_ARGS,
                             "wrapWithProto");
        return false;
    }

    JSObject *wrapped = Wrapper::New(cx, &obj.toObject(), proto.toObjectOrNull(),
                                     &obj.toObject().global(),
                                     &Wrapper::singletonWithPrototype);
    if (!wrapped)
        return false;

    JS_SET_RVAL(cx, vp, OBJECT_TO_JSVAL(wrapped));
    return true;
}

static JSBool
Serialize(JSContext *cx, unsigned argc, jsval *vp)
{
    jsval v = argc > 0 ? JS_ARGV(cx, vp)[0] : UndefinedValue();
    uint64_t *datap;
    size_t nbytes;
    if (!JS_WriteStructuredClone(cx, v, &datap, &nbytes, NULL, NULL, UndefinedValue()))
        return false;

    JSObject *obj = JS_NewUint8Array(cx, nbytes);
    if (!obj) {
        JS_free(cx, datap);
        return false;
    }
    TypedArrayObject *tarr = &obj->as<TypedArrayObject>();
    JS_ASSERT((uintptr_t(tarr->viewData()) & 7) == 0);
    js_memcpy(tarr->viewData(), datap, nbytes);

    JS_ClearStructuredClone(datap, nbytes);
    JS_SET_RVAL(cx, vp, OBJECT_TO_JSVAL(tarr));
    return true;
}

static JSBool
Deserialize(JSContext *cx, unsigned argc, jsval *vp)
{
    Rooted<jsval> v(cx, argc > 0 ? JS_ARGV(cx, vp)[0] : UndefinedValue());
    JSObject *obj;
    if (JSVAL_IS_PRIMITIVE(v) || !(obj = JSVAL_TO_OBJECT(v))->is<TypedArrayObject>()) {
        JS_ReportErrorNumber(cx, my_GetErrorMessage, NULL, JSSMSG_INVALID_ARGS, "deserialize");
        return false;
    }
    TypedArrayObject *tarr = &obj->as<TypedArrayObject>();
    if ((tarr->byteLength() & 7) != 0) {
        JS_ReportErrorNumber(cx, my_GetErrorMessage, NULL, JSSMSG_INVALID_ARGS, "deserialize");
        return false;
    }
    if ((uintptr_t(tarr->viewData()) & 7) != 0) {
        JS_ReportErrorNumber(cx, my_GetErrorMessage, NULL, JSSMSG_BAD_ALIGNMENT);
        return false;
    }

    if (!JS_ReadStructuredClone(cx, (uint64_t *) tarr->viewData(), tarr->byteLength(),
                                JS_STRUCTURED_CLONE_VERSION, v.address(), NULL, NULL)) {
        return false;
    }
    JS_SET_RVAL(cx, vp, v);
    return true;
}

static JSObject *
NewGlobalObject(JSContext *cx, JSObject *sameZoneAs);

static JSBool
NewGlobal(JSContext *cx, unsigned argc, jsval *vp)
{
    JSObject *sameZoneAs = NULL;
    if (argc == 1 && JS_ARGV(cx, vp)[0].isObject())
        sameZoneAs = UncheckedUnwrap(&JS_ARGV(cx, vp)[0].toObject());

    RootedObject global(cx, NewGlobalObject(cx, sameZoneAs));
    if (!global)
        return false;

    if (!JS_WrapObject(cx, global.address()))
        return false;

    JS_SET_RVAL(cx, vp, OBJECT_TO_JSVAL(global));
    return true;
}

static JSBool
EnableStackWalkingAssertion(JSContext *cx, unsigned argc, jsval *vp)
{
    if (argc == 0 || !JSVAL_IS_BOOLEAN(JS_ARGV(cx, vp)[0])) {
        JS_ReportErrorNumber(cx, my_GetErrorMessage, NULL, JSSMSG_INVALID_ARGS,
                             "enableStackWalkingAssertion");
        return false;
    }

#ifdef DEBUG
    cx->stackIterAssertionEnabled = JSVAL_TO_BOOLEAN(JS_ARGV(cx, vp)[0]);
#endif

    JS_SET_RVAL(cx, vp, UndefinedValue());
    return true;
}

static JSBool
GetMaxArgs(JSContext *cx, unsigned arg, jsval *vp)
{
    JS_SET_RVAL(cx, vp, INT_TO_JSVAL(ARGS_LENGTH_MAX));
    return true;
}

static JSBool
ObjectEmulatingUndefined(JSContext *cx, unsigned argc, jsval *vp)
{
    static JSClass cls = {
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

    RootedObject obj(cx, JS_NewObject(cx, &cls, NULL, NULL));
    if (!obj)
        return false;
    JS_SET_RVAL(cx, vp, ObjectValue(*obj));
    return true;
}

static JSBool
GetSelfHostedValue(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (argc != 1 || !args[0].isString()) {
        JS_ReportErrorNumber(cx, my_GetErrorMessage, NULL, JSSMSG_INVALID_ARGS,
                             "getSelfHostedValue");
        return false;
    }
    RootedAtom srcAtom(cx, ToAtom<CanGC>(cx, args[0]));
    if (!srcAtom)
        return false;
    RootedPropertyName srcName(cx, srcAtom->asPropertyName());
    return cx->runtime()->cloneSelfHostedValue(cx, srcName, args.rval());
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
"          and return 'terminated'\n"),

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

    JS_FN_HELP("dumpHeap", DumpHeap, 0, 0,
"dumpHeap([fileName[, start[, toFind[, maxDepth[, toIgnore]]]]])",
"  Interface to JS_DumpHeap with output sent to file."),

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

    JS_FN_HELP("clone", Clone, 1, 0,
"clone(fun[, scope])",
"  Clone function object."),

    JS_FN_HELP("getpda", GetPDA, 1, 0,
"getpda(obj)",
"  Get the property descriptors for obj."),

    JS_FN_HELP("getslx", GetSLX, 1, 0,
"getslx(obj)",
"  Get script line extent."),

    JS_FN_HELP("toint32", ToInt32, 1, 0,
"toint32(n)",
"  Testing hook for JS_ValueToInt32."),

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

    JS_FN_HELP("serialize", Serialize, 1, 0,
"serialize(sd)",
"  Serialize sd using JS_WriteStructuredClone. Returns a TypedArray."),

    JS_FN_HELP("deserialize", Deserialize, 1, 0,
"deserialize(a)",
"  Deserialize data generated by serialize."),

    JS_FN_HELP("newGlobal", NewGlobal, 1, 0,
"newGlobal([obj])",
"  Return a new global object in a new compartment. If obj\n"
"  is given, the compartment will be in the same zone as obj."),

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

    JS_FS_HELP_END
};

static const JSFunctionSpecWithHelp fuzzing_unsafe_functions[] = {
    JS_FN_HELP("getSelfHostedValue", GetSelfHostedValue, 1, 0,
"getSelfHostedValue()",
"  Get a self-hosted value by its name. Note that these values don't get \n"
"  cached, so repeatedly getting the same value creates multiple distinct clones."),

    JS_FN_HELP("parent", Parent, 1, 0,
"parent(obj)",
"  Returns the parent of obj."),

    JS_FN_HELP("line2pc", LineToPC, 0, 0,
"line2pc([fun,] line)",
"  Map line number to PC."),

    JS_FN_HELP("pc2line", PCToLine, 0, 0,
"pc2line(fun[, pc])",
"  Map PC to line number."),

    JS_FN_HELP("setThrowHook", SetThrowHook, 1, 0,
"setThrowHook(f)",
"  Set throw hook to f."),

    JS_FN_HELP("system", System, 1, 0,
"system(command)",
"  Execute command on the current host, returning result code."),

    JS_FN_HELP("trap", Trap, 3, 0,
"trap([fun, [pc,]] exp)",
"  Trap bytecode execution."),

    JS_FN_HELP("untrap", Untrap, 2, 0,
"untrap(fun[, pc])",
"  Remove a trap."),

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
    if (!JS_LookupProperty(cx, obj, "usage", usage.address()))
        return false;
    RootedValue help(cx);
    if (!JS_LookupProperty(cx, obj, "help", help.address()))
        return false;

    if (JSVAL_IS_VOID(usage) || JSVAL_IS_VOID(help))
        return true;

    return PrintHelpString(cx, usage) && PrintHelpString(cx, help);
}

static JSBool
Help(JSContext *cx, unsigned argc, jsval *vp)
{
    fprintf(gOutFile, "%s\n", JS_GetImplementationVersion());

    RootedObject obj(cx);
    if (argc == 0) {
        RootedObject global(cx, JS_GetGlobalForScopeChain(cx));
        AutoIdArray ida(cx, JS_Enumerate(cx, global));
        if (!ida)
            return false;

        for (size_t i = 0; i < ida.length(); i++) {
            RootedValue v(cx);
            if (!JS_LookupPropertyById(cx, global, ida[i], v.address()))
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

/*
 * Define a JS object called "it".  Give it class operations that printf why
 * they're being called for tutorial purposes.
 */
enum its_tinyid {
    ITS_COLOR, ITS_HEIGHT, ITS_WIDTH, ITS_FUNNY, ITS_ARRAY, ITS_RDONLY,
    ITS_CUSTOM, ITS_CUSTOMRDONLY, ITS_CUSTOMNATIVE
};

static JSBool
its_getter(JSContext *cx, HandleObject obj, HandleId id, MutableHandleValue vp);

static JSBool
its_setter(JSContext *cx, HandleObject obj, HandleId id, JSBool strict, MutableHandleValue vp);

static JSBool
its_get_customNative(JSContext *cx, unsigned argc, jsval *vp);

static JSBool
its_set_customNative(JSContext *cx, unsigned argc, jsval *vp);

static const JSPropertySpec its_props[] = {
    {"color",           ITS_COLOR,      JSPROP_ENUMERATE,       JSOP_NULLWRAPPER, JSOP_NULLWRAPPER},
    {"height",          ITS_HEIGHT,     JSPROP_ENUMERATE,       JSOP_NULLWRAPPER, JSOP_NULLWRAPPER},
    {"width",           ITS_WIDTH,      JSPROP_ENUMERATE,       JSOP_NULLWRAPPER, JSOP_NULLWRAPPER},
    {"funny",           ITS_FUNNY,      JSPROP_ENUMERATE,       JSOP_NULLWRAPPER, JSOP_NULLWRAPPER},
    {"array",           ITS_ARRAY,      JSPROP_ENUMERATE,       JSOP_NULLWRAPPER, JSOP_NULLWRAPPER},
    {"rdonly",          ITS_RDONLY,     JSPROP_READONLY,        JSOP_NULLWRAPPER, JSOP_NULLWRAPPER},
    {"custom",          ITS_CUSTOM,     JSPROP_ENUMERATE,
                        JSOP_WRAPPER(its_getter),     JSOP_WRAPPER(its_setter)},
    {"customRdOnly",    ITS_CUSTOMRDONLY, JSPROP_ENUMERATE | JSPROP_READONLY,
                        JSOP_WRAPPER(its_getter),     JSOP_WRAPPER(its_setter)},
    {"customNative",    ITS_CUSTOMNATIVE,
                        JSPROP_ENUMERATE | JSPROP_NATIVE_ACCESSORS,
                        JSOP_WRAPPER((JSPropertyOp)its_get_customNative),
                        JSOP_WRAPPER((JSStrictPropertyOp)its_set_customNative)},
    {NULL,0,0,JSOP_NULLWRAPPER, JSOP_NULLWRAPPER}
};

static JSBool its_noisy;    /* whether to be noisy when finalizing it */
static JSBool its_enum_fail;/* whether to fail when enumerating it */

static JSBool
its_addProperty(JSContext *cx, HandleObject obj, HandleId id, MutableHandleValue vp)
{
    if (!its_noisy)
        return true;

    ToStringHelper idString(cx, id);
    fprintf(gOutFile, "adding its property %s,", idString.getBytes());
    ToStringHelper valueString(cx, vp);
    fprintf(gOutFile, " initial value %s\n", valueString.getBytes());
    return true;
}

static JSBool
its_delProperty(JSContext *cx, HandleObject obj, HandleId id, JSBool *succeeded)
{
    if (!its_noisy) {
        *succeeded = true;
        return true;
    }

    ToStringHelper idString(cx, id);
    if (idString.threw())
        return false;

    fprintf(gOutFile, "deleting its property %s,", idString.getBytes());

    *succeeded = true;
    return true;
}

static JSBool
its_getProperty(JSContext *cx, HandleObject obj, HandleId id, MutableHandleValue vp)
{
    if (!its_noisy)
        return true;

    ToStringHelper idString(cx, id);
    fprintf(gOutFile, "getting its property %s,", idString.getBytes());
    ToStringHelper valueString(cx, vp);
    fprintf(gOutFile, " initial value %s\n", valueString.getBytes());
    return true;
}

static JSBool
its_setProperty(JSContext *cx, HandleObject obj, HandleId id, JSBool strict, MutableHandleValue vp)
{
    ToStringHelper idString(cx, id);
    if (its_noisy) {
        fprintf(gOutFile, "setting its property %s,", idString.getBytes());
        ToStringHelper valueString(cx, vp);
        fprintf(gOutFile, " new value %s\n", valueString.getBytes());
    }

    if (!JSID_IS_ATOM(id))
        return true;

    if (!strcmp(idString.getBytes(), "noisy"))
        JS_ValueToBoolean(cx, vp, &its_noisy);
    else if (!strcmp(idString.getBytes(), "enum_fail"))
        JS_ValueToBoolean(cx, vp, &its_enum_fail);

    return true;
}

/*
 * Its enumerator, implemented using the "new" enumerate API,
 * see class flags.
 */
static JSBool
its_enumerate(JSContext *cx, HandleObject obj, JSIterateOp enum_op,
              jsval *statep, jsid *idp)
{
    RootedObject iterator(cx);

    switch (enum_op) {
      case JSENUMERATE_INIT:
      case JSENUMERATE_INIT_ALL:
        if (its_noisy)
            fprintf(gOutFile, "enumerate its properties\n");

        iterator = JS_NewPropertyIterator(cx, obj);
        if (!iterator)
            return false;

        *statep = OBJECT_TO_JSVAL(iterator);
        if (idp)
            *idp = INT_TO_JSID(0);
        break;

      case JSENUMERATE_NEXT:
        if (its_enum_fail) {
            JS_ReportError(cx, "its enumeration failed");
            return false;
        }

        iterator = (JSObject *) JSVAL_TO_OBJECT(*statep);
        if (!JS_NextProperty(cx, iterator, idp))
            return false;

        if (!JSID_IS_VOID(*idp))
            break;
        /* Fall through. */

      case JSENUMERATE_DESTROY:
        /* Allow our iterator object to be GC'd. */
        *statep = NullValue();
        break;
    }

    return true;
}

static JSBool
its_resolve(JSContext *cx, HandleObject obj, HandleId id, unsigned flags,
            MutableHandleObject objp)
{
    if (its_noisy) {
        ToStringHelper idString(cx, id);
        fprintf(gOutFile, "resolving its property %s, flags {%s}\n",
               idString.getBytes(),
               (flags & JSRESOLVE_ASSIGNING) ? "assigning" : "");
    }
    return true;
}

static JSBool
its_convert(JSContext *cx, HandleObject obj, JSType type, MutableHandleValue vp)
{
    if (its_noisy)
        fprintf(gOutFile, "converting it to %s type\n", JS_GetTypeName(cx, type));
    return JS_ConvertStub(cx, obj, type, vp);
}

static void
its_finalize(JSFreeOp *fop, JSObject *obj)
{
    if (its_noisy)
        fprintf(gOutFile, "finalizing it\n");
}

static JSClass its_class = {
    "It", JSCLASS_NEW_RESOLVE | JSCLASS_NEW_ENUMERATE | JSCLASS_HAS_RESERVED_SLOTS(1),
    its_addProperty,  its_delProperty,  its_getProperty,  its_setProperty,
    (JSEnumerateOp)its_enumerate, (JSResolveOp)its_resolve,
    its_convert,      its_finalize,
};

static JSBool
its_getter(JSContext *cx, HandleObject obj, HandleId id, MutableHandleValue vp)
{
    if (JS_GetClass(obj) != &its_class) {
        vp.set(UndefinedValue());
        return true;
    }

    vp.set(JS_GetReservedSlot(obj, 0));
    return true;
}

static JSBool
its_setter(JSContext *cx, HandleObject obj, HandleId id, JSBool strict, MutableHandleValue vp)
{
    if (JS_GetClass(obj) != &its_class)
        return true;

    JS_SetReservedSlot(obj, 0, vp);

    vp.set(UndefinedValue());
    return true;
}

static JSBool
its_get_customNative(JSContext *cx, unsigned argc, jsval *vp)
{
    JSObject *obj = JS_THIS_OBJECT(cx, vp);
    if (!obj)
        return false;

    if (JS_GetClass(obj) != &its_class) {
        JS_SET_RVAL(cx, vp, UndefinedValue());
        return true;
    }

    JS_SET_RVAL(cx, vp, JS_GetReservedSlot(obj, 0));
    return true;
}

static JSBool
its_set_customNative(JSContext *cx, unsigned argc, jsval *vp)
{
    JSObject *obj = JS_THIS_OBJECT(cx, vp);
    if (!obj)
        return false;

    if (JS_GetClass(obj) != &its_class)
        return true;

    JS_SetReservedSlot(obj, 0, argc >= 1 ? JS_ARGV(cx, vp)[0] : UndefinedValue());
    JS_SET_RVAL(cx, vp, UndefinedValue());
    return true;
}

JSErrorFormatString jsShell_ErrorFormatString[JSShellErr_Limit] = {
#define MSG_DEF(name, number, count, exception, format) \
    { format, count, JSEXN_ERR } ,
#include "jsshell.msg"
#undef MSG_DEF
};

static const JSErrorFormatString *
my_GetErrorMessage(void *userRef, const char *locale, const unsigned errorNumber)
{
    if (errorNumber == 0 || errorNumber >= JSShellErr_Limit)
        return NULL;

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
static JSBool
Exec(JSContext *cx, unsigned argc, jsval *vp)
{
    JSFunction *fun;
    const char *name, **nargv;
    unsigned i, nargc;
    JSString *str;
    bool ok;
    pid_t pid;
    int status;

    JS_SET_RVAL(cx, vp, UndefinedValue());

    fun = JS_ValueToFunction(cx, vp[0]);
    if (!fun)
        return false;
    if (!fun->atom)
        return true;

    nargc = 1 + argc;

    /* nargc + 1 accounts for the terminating NULL. */
    nargv = new (char *)[nargc + 1];
    if (!nargv)
        return false;
    memset(nargv, 0, sizeof(nargv[0]) * (nargc + 1));
    nargv[0] = name;
    jsval *argv = JS_ARGV(cx, vp);
    for (i = 0; i < nargc; i++) {
        str = (i == 0) ? fun->atom : JS_ValueToString(cx, argv[i-1]);
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

static JSBool
global_enumerate(JSContext *cx, HandleObject obj)
{
#ifdef LAZY_STANDARD_CLASSES
    return JS_EnumerateStandardClasses(cx, obj);
#else
    return true;
#endif
}

static JSBool
global_resolve(JSContext *cx, HandleObject obj, HandleId id, unsigned flags,
               MutableHandleObject objp)
{
#ifdef LAZY_STANDARD_CLASSES
    JSBool resolved;

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
    for (comp = strtok(path, ":"); comp; comp = strtok(NULL, ":")) {
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
            ok = (fun != NULL);
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

JSClass global_class = {
    "global", JSCLASS_NEW_RESOLVE | JSCLASS_GLOBAL_FLAGS,
    JS_PropertyStub,  JS_DeletePropertyStub,
    JS_PropertyStub,  JS_StrictPropertyStub,
    global_enumerate, (JSResolveOp) global_resolve,
    JS_ConvertStub,   NULL
};

static JSBool
env_setProperty(JSContext *cx, HandleObject obj, HandleId id, JSBool strict, MutableHandleValue vp)
{
/* XXX porting may be easy, but these don't seem to supply setenv by default */
#if !defined XP_OS2 && !defined SOLARIS
    int rv;

    ToStringHelper idstr(cx, id, true);
    if (idstr.threw())
        return false;
    ToStringHelper valstr(cx, vp, true);
    if (valstr.threw())
        return false;
#if defined XP_WIN || defined HPUX || defined OSF1
    {
        char *waste = JS_smprintf("%s=%s", idstr.getBytes(), valstr.getBytes());
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
    rv = setenv(idstr.getBytes(), valstr.getBytes(), 1);
#endif
    if (rv < 0) {
        JS_ReportError(cx, "can't set env variable %s to %s", idstr.getBytes(), valstr.getBytes());
        return false;
    }
    vp.set(valstr.getJSVal());
#endif /* !defined XP_OS2 && !defined SOLARIS */
    return true;
}

static JSBool
env_enumerate(JSContext *cx, HandleObject obj)
{
    static bool reflected;
    char **evp, *name, *value;
    JSString *valstr;
    bool ok;

    if (reflected)
        return true;

    for (evp = (char **)JS_GetPrivate(obj); (name = *evp) != NULL; evp++) {
        value = strchr(name, '=');
        if (!value)
            continue;
        *value++ = '\0';
        valstr = JS_NewStringCopyZ(cx, value);
        ok = valstr && JS_DefineProperty(cx, obj, name, STRING_TO_JSVAL(valstr),
                                         NULL, NULL, JSPROP_ENUMERATE);
        value[-1] = '=';
        if (!ok)
            return false;
    }

    reflected = true;
    return true;
}

static JSBool
env_resolve(JSContext *cx, HandleObject obj, HandleId id, unsigned flags,
            MutableHandleObject objp)
{
    JSString *valstr;
    const char *name, *value;

    ToStringHelper idstr(cx, id, true);
    if (idstr.threw())
        return false;

    name = idstr.getBytes();
    value = getenv(name);
    if (value) {
        valstr = JS_NewStringCopyZ(cx, value);
        if (!valstr)
            return false;
        if (!JS_DefineProperty(cx, obj, name, STRING_TO_JSVAL(valstr),
                               NULL, NULL, JSPROP_ENUMERATE)) {
            return false;
        }
        objp.set(obj);
    }
    return true;
}

static JSClass env_class = {
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

static JSBool
dom_genericGetter(JSContext* cx, unsigned argc, JS::Value *vp);

static JSBool
dom_genericSetter(JSContext* cx, unsigned argc, JS::Value *vp);

static JSBool
dom_genericMethod(JSContext *cx, unsigned argc, JS::Value *vp);

#ifdef DEBUG
static JSClass *GetDomClass();
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

const JSJitInfo dom_x_getterinfo = {
    { (JSJitGetterOp)dom_get_x },
    0,        /* protoID */
    0,        /* depth */
    JSJitInfo::Getter,
    true,     /* isInfallible. False in setters. */
    true      /* isConstant. Only relevant for getters. */
};

const JSJitInfo dom_x_setterinfo = {
    { (JSJitGetterOp)dom_set_x },
    0,        /* protoID */
    0,        /* depth */
    JSJitInfo::Setter,
    false,    /* isInfallible. False in setters. */
    false     /* isConstant. Only relevant for getters. */
};

const JSJitInfo doFoo_methodinfo = {
    { (JSJitGetterOp)dom_doFoo },
    0,        /* protoID */
    0,        /* depth */
    JSJitInfo::Method,
    false,    /* isInfallible. False in setters. */
    false     /* isConstant. Only relevant for getters. */
};

static const JSPropertySpec dom_props[] = {
    {"x", 0,
     JSPROP_SHARED | JSPROP_ENUMERATE | JSPROP_NATIVE_ACCESSORS,
     { (JSPropertyOp)dom_genericGetter, &dom_x_getterinfo },
     { (JSStrictPropertyOp)dom_genericSetter, &dom_x_setterinfo }
    },
    {NULL,0,0,JSOP_NULLWRAPPER, JSOP_NULLWRAPPER}
};

static const JSFunctionSpec dom_methods[] = {
    JS_FNINFO("doFoo", dom_genericMethod, &doFoo_methodinfo, 3, JSPROP_ENUMERATE),
    JS_FS_END
};

static JSClass dom_class = {
    "FakeDOMObject", JSCLASS_IS_DOMJSCLASS | JSCLASS_HAS_RESERVED_SLOTS(2),
    JS_PropertyStub,       /* addProperty */
    JS_DeletePropertyStub, /* delProperty */
    JS_PropertyStub,       /* getProperty */
    JS_StrictPropertyStub, /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub,
    NULL,                  /* finalize */
    NULL,                  /* checkAccess */
    NULL,                  /* call */
    NULL,                  /* hasInstance */
    NULL,                  /* construct */
    NULL,                  /* trace */
    JSCLASS_NO_INTERNAL_MEMBERS
};

#ifdef DEBUG
static JSClass *GetDomClass() {
    return &dom_class;
}
#endif

static JSBool
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
    MOZ_ASSERT(info->type == JSJitInfo::Getter);
    JSJitGetterOp getter = info->getter;
    return getter(cx, obj, val.toPrivate(), JSJitGetterCallArgs(args));
}

static JSBool
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
    MOZ_ASSERT(info->type == JSJitInfo::Setter);
    JSJitSetterOp setter = info->setter;
    if (!setter(cx, obj, val.toPrivate(), JSJitSetterCallArgs(args)))
        return false;
    args.rval().set(UndefinedValue());
    return true;
}

static JSBool
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
    MOZ_ASSERT(info->type == JSJitInfo::Method);
    JSJitMethodOp method = info->method;
    return method(cx, obj, val.toPrivate(), JSJitMethodCallArgs(args));
}

static void
InitDOMObject(HandleObject obj)
{
    /* Fow now just initialize to a constant we can check. */
    SetReservedSlot(obj, DOM_OBJECT_SLOT, PRIVATE_TO_JSVAL((void *)0x1234));
}

static JSBool
dom_constructor(JSContext* cx, unsigned argc, JS::Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    RootedObject callee(cx, &args.callee());
    RootedValue protov(cx);
    if (!JSObject::getProperty(cx, callee, callee, cx->names().classPrototype, &protov))
        return false;

    if (!protov.isObject()) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_BAD_PROTOTYPE, "FakeDOMObject");
        return false;
    }

    RootedObject domObj(cx, JS_NewObject(cx, &dom_class, &protov.toObject(), NULL));
    if (!domObj)
        return false;

    InitDOMObject(domObj);

    args.rval().setObject(*domObj);
    return true;
}

static JSBool
InstanceClassHasProtoAtDepth(HandleObject protoObject, uint32_t protoID, uint32_t depth)
{
    /* There's only a single (fake) DOM object in the shell, so just return true. */
    return true;
}

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
        return NULL;

    JSShellContextData *data = NewContextData();
    if (!data) {
        DestroyContext(cx, false);
        return NULL;
    }

    JS_SetContextPrivate(cx, data);
    JS_SetErrorReporter(cx, my_ErrorReporter);
    SetContextOptions(cx);
    if (enableTypeInference)
        JS_ToggleOptions(cx, JSOPTION_TYPE_INFERENCE);
    if (enableIon)
        JS_ToggleOptions(cx, JSOPTION_ION);
    if (enableBaseline)
        JS_ToggleOptions(cx, JSOPTION_BASELINE);
    if (enableAsmJS)
        JS_ToggleOptions(cx, JSOPTION_ASMJS);
    return cx;
}

static void
DestroyContext(JSContext *cx, bool withGC)
{
    JSShellContextData *data = GetContextData(cx);
    JS_SetContextPrivate(cx, NULL);
    free(data);
    WITH_SIGNALS_DISABLED(withGC ? JS_DestroyContext(cx) : JS_DestroyContextNoGC(cx));
}

static JSObject *
NewGlobalObject(JSContext *cx, JSObject *sameZoneAs)
{
    JS::CompartmentOptions options;
    options.setZone(sameZoneAs ? JS::SameZoneAs(sameZoneAs) : JS::FreshZone)
           .setVersion(JSVERSION_LATEST);
    RootedObject glob(cx, JS_NewGlobalObject(cx, &global_class, NULL, options));
    if (!glob)
        return NULL;

    {
        JSAutoCompartment ac(cx, glob);

#ifndef LAZY_STANDARD_CLASSES
        if (!JS_InitStandardClasses(cx, glob))
            return NULL;
#endif

#ifdef JS_HAS_CTYPES
        if (!JS_InitCTypesClass(cx, glob))
            return NULL;
#endif
        if (!JS_InitReflect(cx, glob))
            return NULL;
        if (!JS_DefineDebuggerObject(cx, glob))
            return NULL;
        if (!JS::RegisterPerfMeasurement(cx, glob))
            return NULL;
        if (!JS_DefineFunctionsWithHelp(cx, glob, shell_functions) ||
            !JS_DefineProfilingFunctions(cx, glob))
        {
            return NULL;
        }
        if (!js::DefineTestingFunctions(cx, glob))
            return NULL;

        if (!fuzzingSafe && !JS_DefineFunctionsWithHelp(cx, glob, fuzzing_unsafe_functions))
            return NULL;

        RootedObject it(cx, JS_DefineObject(cx, glob, "it", &its_class, NULL, 0));
        if (!it)
            return NULL;
        if (!JS_DefineProperties(cx, it, its_props))
            return NULL;

        if (!JS_DefineProperty(cx, glob, "custom", UndefinedValue(), its_getter,
                               its_setter, 0))
            return NULL;
        if (!JS_DefineProperty(cx, glob, "customRdOnly", UndefinedValue(), its_getter,
                               its_setter, JSPROP_READONLY))
            return NULL;

        if (!JS_DefineProperty(cx, glob, "customNative", UndefinedValue(),
                               (JSPropertyOp)its_get_customNative,
                               (JSStrictPropertyOp)its_set_customNative,
                               JSPROP_SHARED | JSPROP_NATIVE_ACCESSORS))
        {
            return NULL;
        }

        /* Initialize FakeDOMObject. */
        static js::DOMCallbacks DOMcallbacks = {
            InstanceClassHasProtoAtDepth
        };
        SetDOMCallbacks(cx->runtime(), &DOMcallbacks);

        RootedObject domProto(cx, JS_InitClass(cx, glob, NULL, &dom_class, dom_constructor, 0,
                                               dom_props, dom_methods, NULL, NULL));
        if (!domProto)
            return NULL;

        /* Initialize FakeDOMObject.prototype */
        InitDOMObject(domProto);
    }

    return glob;
}

static bool
BindScriptArgs(JSContext *cx, JSObject *obj_, OptionParser *op)
{
    RootedObject obj(cx, obj_);

    MultiStringRange msr = op->getMultiStringArg("scriptArgs");
    RootedObject scriptArgs(cx);
    scriptArgs = JS_NewArrayObject(cx, 0, NULL);
    if (!scriptArgs)
        return false;

    if (!JS_DefineProperty(cx, obj, "scriptArgs", OBJECT_TO_JSVAL(scriptArgs), NULL, NULL, 0))
        return false;

    for (size_t i = 0; !msr.empty(); msr.popFront(), ++i) {
        const char *scriptArg = msr.front();
        JSString *str = JS_NewStringCopyZ(cx, scriptArg);
        if (!str ||
            !JS_DefineElement(cx, scriptArgs, i, STRING_TO_JSVAL(str), NULL, NULL,
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
        reportWarnings = JS_TRUE;
    else if (op->getBoolOption('W'))
        reportWarnings = JS_FALSE;

    if (op->getBoolOption('s'))
        JS_ToggleOptions(cx, JSOPTION_EXTRA_WARNINGS);

    if (op->getBoolOption('d')) {
        JS_SetRuntimeDebugMode(JS_GetRuntime(cx), true);
        JS_SetDebugMode(cx, true);
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
        cx->runtime()->requestHelperThreadCount(threadCount);
#endif /* JS_THREADSAFE */

#if defined(JS_ION)
    if (op->getBoolOption("no-ion")) {
        enableIon = false;
        JS_ToggleOptions(cx, JSOPTION_ION);
    }
    if (op->getBoolOption("no-asmjs")) {
        enableAsmJS = false;
        JS_ToggleOptions(cx, JSOPTION_ASMJS);
    }

    if (op->getBoolOption("no-baseline")) {
        enableBaseline = false;
        JS_ToggleOptions(cx, JSOPTION_BASELINE);
    }

    if (const char *str = op->getStringOption("ion-gvn")) {
        if (strcmp(str, "off") == 0)
            ion::js_IonOptions.gvn = false;
        else if (strcmp(str, "pessimistic") == 0)
            ion::js_IonOptions.gvnIsOptimistic = false;
        else if (strcmp(str, "optimistic") == 0)
            ion::js_IonOptions.gvnIsOptimistic = true;
        else
            return OptionFailure("ion-gvn", str);
    }

    if (const char *str = op->getStringOption("ion-licm")) {
        if (strcmp(str, "on") == 0)
            ion::js_IonOptions.licm = true;
        else if (strcmp(str, "off") == 0)
            ion::js_IonOptions.licm = false;
        else
            return OptionFailure("ion-licm", str);
    }

    if (const char *str = op->getStringOption("ion-edgecase-analysis")) {
        if (strcmp(str, "on") == 0)
            ion::js_IonOptions.edgeCaseAnalysis = true;
        else if (strcmp(str, "off") == 0)
            ion::js_IonOptions.edgeCaseAnalysis = false;
        else
            return OptionFailure("ion-edgecase-analysis", str);
    }

     if (const char *str = op->getStringOption("ion-range-analysis")) {
         if (strcmp(str, "on") == 0)
             ion::js_IonOptions.rangeAnalysis = true;
         else if (strcmp(str, "off") == 0)
             ion::js_IonOptions.rangeAnalysis = false;
         else
             return OptionFailure("ion-range-analysis", str);
     }

    if (const char *str = op->getStringOption("ion-inlining")) {
        if (strcmp(str, "on") == 0)
            ion::js_IonOptions.inlining = true;
        else if (strcmp(str, "off") == 0)
            ion::js_IonOptions.inlining = false;
        else
            return OptionFailure("ion-inlining", str);
    }

    if (const char *str = op->getStringOption("ion-osr")) {
        if (strcmp(str, "on") == 0)
            ion::js_IonOptions.osr = true;
        else if (strcmp(str, "off") == 0)
            ion::js_IonOptions.osr = false;
        else
            return OptionFailure("ion-osr", str);
    }

    if (const char *str = op->getStringOption("ion-limit-script-size")) {
        if (strcmp(str, "on") == 0)
            ion::js_IonOptions.limitScriptSize = true;
        else if (strcmp(str, "off") == 0)
            ion::js_IonOptions.limitScriptSize = false;
        else
            return OptionFailure("ion-limit-script-size", str);
    }

    int32_t useCount = op->getIntOption("ion-uses-before-compile");
    if (useCount >= 0)
        ion::js_IonOptions.usesBeforeCompile = useCount;

    useCount = op->getIntOption("baseline-uses-before-compile");
    if (useCount >= 0)
        ion::js_IonOptions.baselineUsesBeforeCompile = useCount;

    if (op->getBoolOption("baseline-eager"))
        ion::js_IonOptions.baselineUsesBeforeCompile = 0;

    if (const char *str = op->getStringOption("ion-regalloc")) {
        if (strcmp(str, "lsra") == 0)
            ion::js_IonOptions.registerAllocator = ion::RegisterAllocator_LSRA;
        else if (strcmp(str, "backtracking") == 0)
            ion::js_IonOptions.registerAllocator = ion::RegisterAllocator_Backtracking;
        else if (strcmp(str, "stupid") == 0)
            ion::js_IonOptions.registerAllocator = ion::RegisterAllocator_Stupid;
        else
            return OptionFailure("ion-regalloc", str);
    }

    if (op->getBoolOption("ion-eager"))
        ion::js_IonOptions.setEagerCompilation();

#ifdef JS_THREADSAFE
    if (const char *str = op->getStringOption("ion-parallel-compile")) {
        if (strcmp(str, "on") == 0) {
            if (cx->runtime()->helperThreadCount() == 0) {
                fprintf(stderr, "Parallel compilation not available without helper threads");
                return EXIT_FAILURE;
            }
            ion::js_IonOptions.parallelCompilation = true;
        } else if (strcmp(str, "off") == 0) {
            ion::js_IonOptions.parallelCompilation = false;
        } else {
            return OptionFailure("ion-parallel-compile", str);
        }
    }
#endif /* JS_THREADSAFE */

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
        Process(cx, obj, NULL, true); /* Interactive. */
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
        Process(cx, obj, NULL, true);

    return gExitCode ? gExitCode : EXIT_SUCCESS;
}

int
Shell(JSContext *cx, OptionParser *op, char **envp)
{
    JSAutoRequest ar(cx);

    /*
     * First check to see if type inference is enabled. These flags
     * must be set on the compartment when it is constructed.
     */
    if (op->getBoolOption("no-ti")) {
        enableTypeInference = false;
        JS_ToggleOptions(cx, JSOPTION_TYPE_INFERENCE);
    }

    if (op->getBoolOption("fuzzing-safe"))
        fuzzingSafe = true;

    RootedObject glob(cx);
    glob = NewGlobalObject(cx, NULL);
    if (!glob)
        return 1;

    JSAutoCompartment ac(cx, glob);
    JS_SetGlobalObject(cx, glob);

    JSObject *envobj = JS_DefineObject(cx, glob, "environment", &env_class, NULL, 0);
    if (!envobj)
        return 1;
    JS_SetPrivate(envobj, envp);

    int result = ProcessArgs(cx, glob, op);

    if (enableDisassemblyDumps)
        JS_DumpCompartmentPCCounts(cx);

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

/* Set the initial counter to 1 so the principal will never be destroyed. */
JSPrincipals shellTrustedPrincipals = { 1 };

JSBool
CheckObjectAccess(JSContext *cx, HandleObject obj, HandleId id, JSAccessMode mode,
                  MutableHandleValue vp)
{
    return true;
}

JSSecurityCallbacks securityCallbacks = {
    CheckObjectAccess,
    NULL
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
    int stackDummy;
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

#ifdef JS_THREADSAFE
    if (PR_FAILURE == PR_NewThreadPrivateIndex(&gStackBaseThreadIndex, NULL) ||
        PR_FAILURE == PR_SetThreadPrivate(gStackBaseThreadIndex, &stackDummy)) {
        return 1;
    }
#else
    gStackBase = (uintptr_t) &stackDummy;
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
                               "Range analysis (default: off, on to enable)")
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
#ifdef JS_THREADSAFE
        || !op.addStringOption('\0', "ion-parallel-compile", "on/off",
                               "Compile scripts off thread (default: off)")
#endif
        || !op.addBoolOption('\0', "baseline", "Enable baseline compiler (default)")
        || !op.addBoolOption('\0', "no-baseline", "Disable baseline compiler")
        || !op.addBoolOption('\0', "baseline-eager", "Always baseline-compile methods")
        || !op.addIntOption('\0', "baseline-uses-before-compile", "COUNT",
                            "Wait for COUNT calls or iterations before baseline-compiling "
                            "(default: 10)", -1)
        || !op.addBoolOption('\0', "no-fpu", "Pretend CPU does not support floating-point operations "
                             "to test JIT codegen (no-op on platforms other than x86).")
        || !op.addBoolOption('\0', "fuzzing-safe", "Don't expose functions that aren't safe for "
                             "fuzzers to call")
#ifdef DEBUG
        || !op.addBoolOption('\0', "dump-entrained-variables", "Print variables which are "
                             "unnecessarily entrained by inner functions")
#endif
#ifdef JSGC_GENERATIONAL
        || !op.addBoolOption('\0', "no-ggc", "Disable Generational GC")
#endif
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

#if defined(JS_CPU_X86)
    if (op.getBoolOption("no-fpu"))
        JSC::MacroAssembler::SetFloatingPointDisabled();
#endif
#endif

    // Start the engine.
    if (!JS_Init())
        return 1;

    /* Use the same parameters as the browser in xpcjsruntime.cpp. */
    rt = JS_NewRuntime(32L * 1024L * 1024L, JS_USE_HELPER_THREADS);
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

    JS_SetTrustedPrincipals(rt, &shellTrustedPrincipals);
    JS_SetSecurityCallbacks(rt, &securityCallbacks);

    JS_SetNativeStackQuota(rt, gMaxStackSize);

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
