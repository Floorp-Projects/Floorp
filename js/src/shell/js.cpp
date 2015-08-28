/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* JS shell. */

#include "mozilla/ArrayUtils.h"
#include "mozilla/Atomics.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/GuardObjects.h"
#include "mozilla/mozalloc.h"
#include "mozilla/PodOperations.h"

#ifdef XP_WIN
# include <direct.h>
# include <process.h>
#endif
#include <errno.h>
#include <fcntl.h>
#if defined(XP_WIN)
# include <io.h>     /* for isatty() */
#endif
#include <locale.h>
#if defined(MALLOC_H)
# include MALLOC_H    /* for malloc_usable_size, malloc_size, _msize */
#endif
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
#include "jslock.h"
#include "jsobj.h"
#include "jsprf.h"
#include "jsscript.h"
#include "jstypes.h"
#include "jsutil.h"
#ifdef XP_WIN
# include "jswin.h"
#endif
#include "jswrapper.h"

#include "builtin/ModuleObject.h"
#include "builtin/TestingFunctions.h"
#include "frontend/Parser.h"
#include "gc/GCInternals.h"
#include "jit/arm/Simulator-arm.h"
#include "jit/Ion.h"
#include "jit/JitcodeMap.h"
#include "jit/OptimizationTracking.h"
#include "js/Debug.h"
#include "js/GCAPI.h"
#include "js/StructuredClone.h"
#include "js/TrackedOptimizationInfo.h"
#include "perf/jsperf.h"
#include "shell/jsoptparse.h"
#include "shell/OSObject.h"
#include "vm/ArgumentsObject.h"
#include "vm/Debugger.h"
#include "vm/HelperThreads.h"
#include "vm/Monitor.h"
#include "vm/Shape.h"
#include "vm/SharedArrayObject.h"
#include "vm/Time.h"
#include "vm/TypedArrayObject.h"
#include "vm/WrapperObject.h"

#include "jscompartmentinlines.h"
#include "jsobjinlines.h"

#include "vm/Interpreter-inl.h"
#include "vm/Stack-inl.h"

using namespace js;
using namespace js::cli;
using namespace js::shell;

using mozilla::ArrayLength;
using mozilla::Atomic;
using mozilla::MakeUnique;
using mozilla::Maybe;
using mozilla::NumberEqualsInt32;
using mozilla::PodCopy;
using mozilla::PodEqual;
using mozilla::UniquePtr;

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
static Atomic<bool> gServiceInterrupt;
static JS::PersistentRootedValue gInterruptFunc;

static bool gLastWarningEnabled = false;
static JS::PersistentRootedValue gLastWarning;

static bool enableDisassemblyDumps = false;
static bool offthreadCompilation = false;
static bool enableBaseline = false;
static bool enableIon = false;
static bool enableAsmJS = false;
static bool enableNativeRegExp = false;
static bool enableUnboxedArrays = false;
#ifdef JS_GC_ZEAL
static char gZealStr[128];
#endif

static bool printTiming = false;
static const char* jsCacheDir = nullptr;
static const char* jsCacheAsmJSPath = nullptr;
static bool jsCachingEnabled = false;
mozilla::Atomic<bool> jsCacheOpened(false);

static bool
SetTimeoutValue(JSContext* cx, double t);

static bool
InitWatchdog(JSRuntime* rt);

static void
KillWatchdog();

static bool
ScheduleWatchdog(JSRuntime* rt, double t);

static void
CancelExecution(JSRuntime* rt);

/*
 * Watchdog thread state.
 */
static PRLock* gWatchdogLock = nullptr;
static PRCondVar* gWatchdogWakeup = nullptr;
static PRThread* gWatchdogThread = nullptr;
static bool gWatchdogHasTimeout = false;
static int64_t gWatchdogTimeout = 0;

static PRCondVar* gSleepWakeup = nullptr;

static int gExitCode = 0;
static bool gQuitting = false;
static bool gGotError = false;
static FILE* gErrFile = nullptr;
static FILE* gOutFile = nullptr;

static bool reportWarnings = true;
static bool compileOnly = false;
static bool fuzzingSafe = false;

#ifdef DEBUG
static bool dumpEntrainedVariables = false;
static bool OOM_printAllocationCount = false;
#endif

static JSContext*
NewContext(JSRuntime* rt);

static void
DestroyContext(JSContext* cx, bool withGC);

static JSObject*
NewGlobalObject(JSContext* cx, JS::CompartmentOptions& options,
                JSPrincipals* principals);

/*
 * A toy principals type for the shell.
 *
 * In the shell, a principal is simply a 32-bit mask: P subsumes Q if the
 * set bits in P are a superset of those in Q. Thus, the principal 0 is
 * subsumed by everything, and the principal ~0 subsumes everything.
 *
 * As a special case, a null pointer as a principal is treated like 0xffff.
 *
 * The 'newGlobal' function takes an option indicating which principal the
 * new global should have; 'evaluate' does for the new code.
 */
class ShellPrincipals: public JSPrincipals {
    uint32_t bits;

    static uint32_t getBits(JSPrincipals* p) {
        if (!p)
            return 0xffff;
        return static_cast<ShellPrincipals*>(p)->bits;
    }

  public:
    explicit ShellPrincipals(uint32_t bits, int32_t refcount = 0) : bits(bits) {
        this->refcount = refcount;
    }

    static void destroy(JSPrincipals* principals) {
        MOZ_ASSERT(principals != &fullyTrusted);
        MOZ_ASSERT(principals->refcount == 0);
        js_delete(static_cast<const ShellPrincipals*>(principals));
    }

    static bool subsumes(JSPrincipals* first, JSPrincipals* second) {
        uint32_t firstBits  = getBits(first);
        uint32_t secondBits = getBits(second);
        return (firstBits | secondBits) == firstBits;
    }

    static JSSecurityCallbacks securityCallbacks;

    // Fully-trusted principals singleton.
    static ShellPrincipals fullyTrusted;
};

JSSecurityCallbacks ShellPrincipals::securityCallbacks = {
    nullptr, // contentSecurityPolicyAllows
    subsumes
};

// The fully-trusted principal subsumes all other principals.
ShellPrincipals ShellPrincipals::fullyTrusted(-1, 1);

#ifdef EDITLINE
extern "C" {
extern JS_EXPORT_API(char*) readline(const char* prompt);
extern JS_EXPORT_API(void)   add_history(char* line);
} // extern "C"
#endif

static char*
GetLine(FILE* file, const char * prompt)
{
#ifdef EDITLINE
    /*
     * Use readline only if file is stdin, because there's no way to specify
     * another handle.  Are other filehandles interactive?
     */
    if (file == stdin) {
        char* linep = readline(prompt);
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

    size_t size = 80;
    char* buffer = static_cast<char*>(malloc(size));
    if (!buffer)
        return nullptr;

    char* current = buffer;
    while (true) {
        while (true) {
            if (fgets(current, size - len, file))
                break;
            if (errno != EINTR) {
                free(buffer);
                return nullptr;
            }
        }

        len += strlen(current);
        char* t = buffer + len - 1;
        if (*t == '\n') {
            /* Line was read. We remove '\n' and exit. */
            *t = '\0';
            return buffer;
        }

        if (len + 1 == size) {
            size = size * 2;
            char* tmp = static_cast<char*>(realloc(buffer, size));
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

/* State to store as JSContext private. */
struct JSShellContextData {
    /* Creation timestamp, used by the elapsed() shell builtin. */
    int64_t startTime;
};

static JSShellContextData*
NewContextData()
{
    JSShellContextData* data = (JSShellContextData*)
                               js_calloc(sizeof(JSShellContextData), 1);
    if (!data)
        return nullptr;
    data->startTime = PRMJ_Now();
    return data;
}

static inline JSShellContextData*
GetContextData(JSContext* cx)
{
    JSShellContextData* data = (JSShellContextData*) JS_GetContextPrivate(cx);

    MOZ_ASSERT(data);
    return data;
}

static bool
ShellInterruptCallback(JSContext* cx)
{
    if (!gServiceInterrupt)
        return true;

    // Reset gServiceInterrupt. CancelExecution or InterruptIf will set it to
    // true to distinguish watchdog or user triggered interrupts.
    // Do this first to prevent other interrupts that may occur while the
    // user-supplied callback is executing from re-entering the handler.
    gServiceInterrupt = false;

    bool result;
    RootedValue interruptFunc(cx, gInterruptFunc);
    if (!interruptFunc.isNull()) {
        JS::AutoSaveExceptionState savedExc(cx);
        JSAutoCompartment ac(cx, &interruptFunc.toObject());
        RootedValue rval(cx);
        if (!JS_CallFunctionValue(cx, nullptr, interruptFunc,
                                  JS::HandleValueArray::empty(), &rval))
        {
            return false;
        }
        if (rval.isBoolean())
            result = rval.toBoolean();
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
RunFile(JSContext* cx, const char* filename, FILE* file, bool compileOnly)
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
        CompileOptions options(cx);
        options.setIntroductionType("js shell file")
               .setUTF8(true)
               .setFileAndLine(filename, 1)
               .setIsRunOnce(true)
               .setNoScriptRval(true);

        gGotError = false;
        (void) JS::Compile(cx, options, file, &script);
        MOZ_ASSERT_IF(!script, gGotError);
    }

    #ifdef DEBUG
        if (dumpEntrainedVariables)
            AnalyzeEntrainedVariables(cx, script);
    #endif
    if (script && !compileOnly) {
        if (!JS_ExecuteScript(cx, script)) {
            if (!gQuitting && gExitCode != EXITCODE_TIMEOUT)
                gExitCode = EXITCODE_RUNTIME_ERROR;
        }
        int64_t t2 = PRMJ_Now() - t1;
        if (printTiming)
            printf("runtime = %.3f ms\n", double(t2) / PRMJ_USEC_PER_MSEC);
    }
}

static bool
EvalAndPrint(JSContext* cx, const char* bytes, size_t length,
             int lineno, bool compileOnly, FILE* out)
{
    // Eval.
    JS::CompileOptions options(cx);
    options.setIntroductionType("js shell interactive")
           .setUTF8(true)
           .setIsRunOnce(true)
           .setFileAndLine("typein", lineno);
    RootedScript script(cx);
    if (!JS::Compile(cx, options, bytes, length, &script))
        return false;
    if (compileOnly)
        return true;
    RootedValue result(cx);
    if (!JS_ExecuteScript(cx, script, &result))
        return false;

    if (!result.isUndefined()) {
        // Print.
        RootedString str(cx);
        str = JS_ValueToSource(cx, result);
        if (!str)
            return false;

        char* utf8chars = JS_EncodeStringToUTF8(cx, str);
        if (!utf8chars)
            return false;
        fprintf(out, "%s\n", utf8chars);
        JS_free(cx, utf8chars);
    }
    return true;
}

static void
ReadEvalPrintLoop(JSContext* cx, FILE* in, FILE* out, bool compileOnly)
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
        typedef Vector<char, 32> CharBuffer;
        CharBuffer buffer(cx);
        do {
            ScheduleWatchdog(cx->runtime(), -1);
            gServiceInterrupt = false;
            errno = 0;

            char* line = GetLine(in, startline == lineno ? "js> " : "");
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
        } while (!JS_BufferIsCompilableUnit(cx, cx->global(), buffer.begin(), buffer.length()));

        if (hitEOF && buffer.empty())
            break;

        if (!EvalAndPrint(cx, buffer.begin(), buffer.length(), startline, compileOnly,
                          out))
        {
            // Catch the error, report it, and keep going.
            JS_ReportPendingException(cx);
        }
    } while (!hitEOF && !gQuitting);

    fprintf(out, "\n");
}

static void
Process(JSContext* cx, const char* filename, bool forceTTY)
{
    FILE* file;
    if (forceTTY || !filename || strcmp(filename, "-") == 0) {
        file = stdin;
    } else {
        file = fopen(filename, "r");
        if (!file) {
            JS_ReportErrorNumber(cx, my_GetErrorMessage, nullptr,
                                 JSSMSG_CANT_OPEN, filename, strerror(errno));
            return;
        }
    }
    AutoCloseInputFile autoClose(file);

    if (!forceTTY && !isatty(fileno(file))) {
        // It's not interactive - just execute it.
        RunFile(cx, filename, file, compileOnly);
    } else {
        // It's an interactive filehandle; drop into read-eval-print loop.
        ReadEvalPrintLoop(cx, file, gOutFile, compileOnly);
    }
}

static bool
Version(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    JSVersion origVersion = JS_GetVersion(cx);
    if (args.length() == 0 || args[0].isUndefined()) {
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
            if (NumberEqualsInt32(fv, &fvi))
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

static bool
CreateMappedArrayBuffer(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (args.length() < 1 || args.length() > 3) {
        JS_ReportErrorNumber(cx, my_GetErrorMessage, nullptr,
                             args.length() < 1 ? JSSMSG_NOT_ENOUGH_ARGS : JSSMSG_TOO_MANY_ARGS,
                             "createMappedArrayBuffer");
        return false;
    }

    RootedString rawFilenameStr(cx, JS::ToString(cx, args[0]));
    if (!rawFilenameStr)
        return false;
    // It's a little bizarre to resolve relative to the script, but for testing
    // I need a file at a known location, and the only good way I know of to do
    // that right now is to include it in the repo alongside the test script.
    // Bug 944164 would introduce an alternative.
    JSString* filenameStr = ResolvePath(cx, rawFilenameStr, ScriptRelative);
    if (!filenameStr)
        return false;
    JSAutoByteString filename(cx, filenameStr);
    if (!filename)
        return false;

    uint32_t offset = 0;
    if (args.length() >= 2) {
        if (!JS::ToUint32(cx, args[1], &offset))
            return false;
    }

    bool sizeGiven = false;
    uint32_t size;
    if (args.length() >= 3) {
        if (!JS::ToUint32(cx, args[2], &size))
            return false;
        sizeGiven = true;
        if (offset > size) {
            JS_ReportErrorNumber(cx, GetErrorMessage, nullptr,
                                 JSMSG_ARG_INDEX_OUT_OF_RANGE, "2");
            return false;
        }
    }

    FILE* file = fopen(filename.ptr(), "r");
    if (!file) {
        JS_ReportErrorNumber(cx, my_GetErrorMessage, nullptr,
                             JSSMSG_CANT_OPEN, filename.ptr(), strerror(errno));
        return false;
    }
    AutoCloseInputFile autoClose(file);

    if (!sizeGiven) {
        struct stat st;
        if (fstat(fileno(file), &st) < 0) {
            JS_ReportError(cx, "Unable to stat file");
            return false;
        }
        if (st.st_size < off_t(offset)) {
            JS_ReportErrorNumber(cx, GetErrorMessage, nullptr,
                                 JSMSG_ARG_INDEX_OUT_OF_RANGE, "2");
            return false;
        }
        size = st.st_size - offset;
    }

    void* contents = JS_CreateMappedArrayBufferContents(fileno(file), offset, size);
    if (!contents) {
        JS_ReportError(cx, "failed to allocate mapped array buffer contents (possibly due to bad alignment)");
        return false;
    }

    RootedObject obj(cx, JS_NewMappedArrayBufferWithContents(cx, size, contents));
    if (!obj)
        return false;

    args.rval().setObject(*obj);
    return true;
}

static bool
Options(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    JS::RuntimeOptions oldRuntimeOptions = JS::RuntimeOptionsRef(cx);
    for (unsigned i = 0; i < args.length(); i++) {
        JSString* str = JS::ToString(cx, args[i]);
        if (!str)
            return false;
        args[i].setString(str);

        JSAutoByteString opt(cx, str);
        if (!opt)
            return false;

        if (strcmp(opt.ptr(), "strict") == 0)
            JS::RuntimeOptionsRef(cx).toggleExtraWarnings();
        else if (strcmp(opt.ptr(), "werror") == 0)
            JS::RuntimeOptionsRef(cx).toggleWerror();
        else if (strcmp(opt.ptr(), "strict_mode") == 0)
            JS::RuntimeOptionsRef(cx).toggleStrictMode();
        else {
            JS_ReportError(cx,
                           "unknown option name '%s'."
                           " The valid names are strict,"
                           " werror, and strict_mode.",
                           opt.ptr());
            return false;
        }
    }

    char* names = strdup("");
    bool found = false;
    if (names && oldRuntimeOptions.extraWarnings()) {
        names = JS_sprintf_append(names, "%s%s", found ? "," : "", "strict");
        found = true;
    }
    if (names && oldRuntimeOptions.werror()) {
        names = JS_sprintf_append(names, "%s%s", found ? "," : "", "werror");
        found = true;
    }
    if (names && oldRuntimeOptions.strictMode()) {
        names = JS_sprintf_append(names, "%s%s", found ? "," : "", "strict_mode");
        found = true;
    }
    if (!names) {
        JS_ReportOutOfMemory(cx);
        return false;
    }

    JSString* str = JS_NewStringCopyZ(cx, names);
    free(names);
    if (!str)
        return false;
    args.rval().setString(str);
    return true;
}

static bool
LoadScript(JSContext* cx, unsigned argc, Value* vp, bool scriptRelative)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    RootedString str(cx);
    for (unsigned i = 0; i < args.length(); i++) {
        str = JS::ToString(cx, args[i]);
        if (!str) {
            JS_ReportErrorNumber(cx, my_GetErrorMessage, nullptr, JSSMSG_INVALID_ARGS, "load");
            return false;
        }
        str = ResolvePath(cx, str, scriptRelative ? ScriptRelative : RootRelative);
        if (!str) {
            JS_ReportError(cx, "unable to resolve path");
            return false;
        }
        JSAutoByteString filename(cx, str);
        if (!filename)
            return false;
        errno = 0;
        CompileOptions opts(cx);
        opts.setIntroductionType("js shell load")
            .setUTF8(true)
            .setIsRunOnce(true)
            .setNoScriptRval(true);
        RootedScript script(cx);
        RootedValue unused(cx);
        if ((compileOnly && !Compile(cx, opts, filename.ptr(), &script)) ||
            !Evaluate(cx, opts, filename.ptr(), &unused))
        {
            return false;
        }
    }

    args.rval().setUndefined();
    return true;
}

static bool
Load(JSContext* cx, unsigned argc, Value* vp)
{
    return LoadScript(cx, argc, vp, false);
}

static bool
LoadScriptRelativeToScript(JSContext* cx, unsigned argc, Value* vp)
{
    return LoadScript(cx, argc, vp, true);
}

// Populate |options| with the options given by |opts|'s properties. If we
// need to convert a filename to a C string, let fileNameBytes own the
// bytes.
static bool
ParseCompileOptions(JSContext* cx, CompileOptions& options, HandleObject opts,
                    JSAutoByteString& fileNameBytes)
{
    RootedValue v(cx);
    RootedString s(cx);

    if (!JS_GetProperty(cx, opts, "isRunOnce", &v))
        return false;
    if (!v.isUndefined())
        options.setIsRunOnce(ToBoolean(v));

    if (!JS_GetProperty(cx, opts, "noScriptRval", &v))
        return false;
    if (!v.isUndefined())
        options.setNoScriptRval(ToBoolean(v));

    if (!JS_GetProperty(cx, opts, "fileName", &v))
        return false;
    if (v.isNull()) {
        options.setFile(nullptr);
    } else if (!v.isUndefined()) {
        s = ToString(cx, v);
        if (!s)
            return false;
        char* fileName = fileNameBytes.encodeLatin1(cx, s);
        if (!fileName)
            return false;
        options.setFile(fileName);
    }

    if (!JS_GetProperty(cx, opts, "element", &v))
        return false;
    if (v.isObject())
        options.setElement(&v.toObject());

    if (!JS_GetProperty(cx, opts, "elementAttributeName", &v))
        return false;
    if (!v.isUndefined()) {
        s = ToString(cx, v);
        if (!s)
            return false;
        options.setElementAttributeName(s);
    }

    if (!JS_GetProperty(cx, opts, "lineNumber", &v))
        return false;
    if (!v.isUndefined()) {
        uint32_t u;
        if (!ToUint32(cx, v, &u))
            return false;
        options.setLine(u);
    }

    if (!JS_GetProperty(cx, opts, "columnNumber", &v))
        return false;
    if (!v.isUndefined()) {
        int32_t c;
        if (!ToInt32(cx, v, &c))
            return false;
        options.setColumn(c);
    }

    if (!JS_GetProperty(cx, opts, "sourceIsLazy", &v))
        return false;
    if (v.isBoolean())
        options.setSourceIsLazy(v.toBoolean());

    return true;
}

class AutoNewContext
{
  private:
    JSContext* oldcx;
    JSContext* newcx;
    Maybe<JSAutoRequest> newRequest;
    Maybe<AutoCompartment> newCompartment;

    AutoNewContext(const AutoNewContext&) = delete;

  public:
    AutoNewContext() : oldcx(nullptr), newcx(nullptr) {}

    bool enter(JSContext* cx) {
        MOZ_ASSERT(!JS_IsExceptionPending(cx));
        oldcx = cx;
        newcx = NewContext(JS_GetRuntime(cx));
        if (!newcx)
            return false;
        JS::ContextOptionsRef(newcx).setDontReportUncaught(true);

        newRequest.emplace(newcx);
        newCompartment.emplace(newcx, JS::CurrentGlobalOrNull(cx));
        return true;
    }

    JSContext* get() { return newcx; }

    ~AutoNewContext() {
        if (newcx) {
            RootedValue exc(oldcx);
            bool throwing = JS_IsExceptionPending(newcx);
            if (throwing)
                JS_GetPendingException(newcx, &exc);
            newCompartment.reset();
            newRequest.reset();
            if (throwing && JS_WrapValue(oldcx, &exc))
                JS_SetPendingException(oldcx, exc);
            DestroyContext(newcx, false);
        }
    }
};

static void
my_LargeAllocFailCallback(void* data)
{
    JSContext* cx = (JSContext*)data;
    JSRuntime* rt = cx->runtime();

    if (!cx->isJSContext())
        return;

    MOZ_ASSERT(!rt->isHeapBusy());
    MOZ_ASSERT(!rt->currentThreadHasExclusiveAccess());

    JS::PrepareForFullGC(rt);
    AutoKeepAtoms keepAtoms(cx->perThreadData);
    rt->gc.gc(GC_NORMAL, JS::gcreason::SHARED_MEMORY_LIMIT);
}

static const uint32_t CacheEntry_SOURCE = 0;
static const uint32_t CacheEntry_BYTECODE = 1;

static const JSClass CacheEntry_class = {
    "CacheEntryObject", JSCLASS_HAS_RESERVED_SLOTS(2)
};

static bool
CacheEntry(JSContext* cx, unsigned argc, JS::Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (args.length() != 1 || !args[0].isString()) {
        JS_ReportErrorNumber(cx, my_GetErrorMessage, nullptr, JSSMSG_INVALID_ARGS, "CacheEntry");
        return false;
    }

    RootedObject obj(cx, JS_NewObject(cx, &CacheEntry_class));
    if (!obj)
        return false;

    SetReservedSlot(obj, CacheEntry_SOURCE, args[0]);
    SetReservedSlot(obj, CacheEntry_BYTECODE, UndefinedValue());
    args.rval().setObject(*obj);
    return true;
}

static bool
CacheEntry_isCacheEntry(JSObject* cache)
{
    return JS_GetClass(cache) == &CacheEntry_class;
}

static JSString*
CacheEntry_getSource(HandleObject cache)
{
    MOZ_ASSERT(CacheEntry_isCacheEntry(cache));
    Value v = JS_GetReservedSlot(cache, CacheEntry_SOURCE);
    if (!v.isString())
        return nullptr;

    return v.toString();
}

static uint8_t*
CacheEntry_getBytecode(HandleObject cache, uint32_t* length)
{
    MOZ_ASSERT(CacheEntry_isCacheEntry(cache));
    Value v = JS_GetReservedSlot(cache, CacheEntry_BYTECODE);
    if (!v.isObject() || !v.toObject().is<ArrayBufferObject>())
        return nullptr;

    ArrayBufferObject* arrayBuffer = &v.toObject().as<ArrayBufferObject>();
    *length = arrayBuffer->byteLength();
    return arrayBuffer->dataPointer();
}

static bool
CacheEntry_setBytecode(JSContext* cx, HandleObject cache, uint8_t* buffer, uint32_t length)
{
    MOZ_ASSERT(CacheEntry_isCacheEntry(cache));

    ArrayBufferObject::BufferContents contents =
        ArrayBufferObject::BufferContents::create<ArrayBufferObject::PLAIN>(buffer);
    Rooted<ArrayBufferObject*> arrayBuffer(cx, ArrayBufferObject::create(cx, length, contents));
    if (!arrayBuffer)
        return false;

    SetReservedSlot(cache, CacheEntry_BYTECODE, ObjectValue(*arrayBuffer));
    return true;
}

class AutoSaveFrameChain
{
    JSContext* cx_;
    bool saved_;

  public:
    explicit AutoSaveFrameChain(JSContext* cx)
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
Evaluate(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (args.length() < 1 || args.length() > 2) {
        JS_ReportErrorNumber(cx, my_GetErrorMessage, nullptr,
                             args.length() < 1 ? JSSMSG_NOT_ENOUGH_ARGS : JSSMSG_TOO_MANY_ARGS,
                             "evaluate");
        return false;
    }

    RootedString code(cx, nullptr);
    RootedObject cacheEntry(cx, nullptr);
    if (args[0].isString()) {
        code = args[0].toString();
    } else if (args[0].isObject() && CacheEntry_isCacheEntry(&args[0].toObject())) {
        cacheEntry = &args[0].toObject();
        code = CacheEntry_getSource(cacheEntry);
    }

    if (!code || (args.length() == 2 && args[1].isPrimitive())) {
        JS_ReportErrorNumber(cx, my_GetErrorMessage, nullptr, JSSMSG_INVALID_ARGS, "evaluate");
        return false;
    }

    CompileOptions options(cx);
    JSAutoByteString fileNameBytes;
    bool newContext = false;
    RootedString displayURL(cx);
    RootedString sourceMapURL(cx);
    RootedObject global(cx, nullptr);
    bool catchTermination = false;
    bool saveFrameChain = false;
    bool loadBytecode = false;
    bool saveBytecode = false;
    bool assertEqBytecode = false;
    RootedObject callerGlobal(cx, cx->global());

    options.setIntroductionType("js shell evaluate")
           .setFileAndLine("@evaluate", 1);

    global = JS_GetGlobalForObject(cx, &args.callee());
    if (!global)
        return false;

    if (args.length() == 2) {
        RootedObject opts(cx, &args[1].toObject());
        RootedValue v(cx);

        if (!ParseCompileOptions(cx, options, opts, fileNameBytes))
            return false;

        if (!JS_GetProperty(cx, opts, "newContext", &v))
            return false;
        if (!v.isUndefined())
            newContext = ToBoolean(v);

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

        if (!JS_GetProperty(cx, opts, "global", &v))
            return false;
        if (!v.isUndefined()) {
            if (v.isObject()) {
                global = js::UncheckedUnwrap(&v.toObject());
                if (!global)
                    return false;
            }
            if (!global || !(JS_GetClass(global)->flags & JSCLASS_IS_GLOBAL)) {
                JS_ReportErrorNumber(cx, GetErrorMessage, nullptr, JSMSG_UNEXPECTED_TYPE,
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

        if (!JS_GetProperty(cx, opts, "loadBytecode", &v))
            return false;
        if (!v.isUndefined())
            loadBytecode = ToBoolean(v);

        if (!JS_GetProperty(cx, opts, "saveBytecode", &v))
            return false;
        if (!v.isUndefined())
            saveBytecode = ToBoolean(v);

        if (!JS_GetProperty(cx, opts, "assertEqBytecode", &v))
            return false;
        if (!v.isUndefined())
            assertEqBytecode = ToBoolean(v);

        // We cannot load or save the bytecode if we have no object where the
        // bytecode cache is stored.
        if (loadBytecode || saveBytecode) {
            if (!cacheEntry) {
                JS_ReportErrorNumber(cx, my_GetErrorMessage, nullptr, JSSMSG_INVALID_ARGS,
                                     "evaluate");
                return false;
            }
        }
    }

    AutoStableStringChars codeChars(cx);
    if (!codeChars.initTwoByte(cx, code))
        return false;

    AutoNewContext ancx;
    if (newContext) {
        if (!ancx.enter(cx))
            return false;
        cx = ancx.get();
    }

    uint32_t loadLength = 0;
    uint8_t* loadBuffer = nullptr;
    uint32_t saveLength = 0;
    ScopedJSFreePtr<uint8_t> saveBuffer;

    if (loadBytecode) {
        loadBuffer = CacheEntry_getBytecode(cacheEntry, &loadLength);
        if (!loadBuffer)
            return false;
    }

    {
        AutoSaveFrameChain asfc(cx);
        if (saveFrameChain && !asfc.save())
            return false;

        JSAutoCompartment ac(cx, global);
        RootedScript script(cx);

        {
            if (saveBytecode) {
                if (!JS::CompartmentOptionsRef(cx).getSingletonsAsTemplates()) {
                    JS_ReportErrorNumber(cx, my_GetErrorMessage, nullptr,
                                         JSSMSG_CACHE_SINGLETON_FAILED);
                    return false;
                }
                JS::CompartmentOptionsRef(cx).setCloneSingletons(true);
            }

            if (loadBytecode) {
                script = JS_DecodeScript(cx, loadBuffer, loadLength);
            } else {
                mozilla::Range<const char16_t> chars = codeChars.twoByteRange();
                (void) JS::Compile(cx, options, chars.start().get(), chars.length(), &script);
            }

            if (!script)
                return false;
        }

        if (displayURL && !script->scriptSource()->hasDisplayURL()) {
            JSFlatString* flat = displayURL->ensureFlat(cx);
            if (!flat)
                return false;

            AutoStableStringChars chars(cx);
            if (!chars.initTwoByte(cx, flat))
                return false;

            const char16_t* durl = chars.twoByteRange().start().get();
            if (!script->scriptSource()->setDisplayURL(cx, durl))
                return false;
        }
        if (sourceMapURL && !script->scriptSource()->hasSourceMapURL()) {
            JSFlatString* flat = sourceMapURL->ensureFlat(cx);
            if (!flat)
                return false;

            AutoStableStringChars chars(cx);
            if (!chars.initTwoByte(cx, flat))
                return false;

            const char16_t* smurl = chars.twoByteRange().start().get();
            if (!script->scriptSource()->setSourceMapURL(cx, smurl))
                return false;
        }
        if (!JS_ExecuteScript(cx, script, args.rval())) {
            if (catchTermination && !JS_IsExceptionPending(cx)) {
                JSAutoCompartment ac1(cx, callerGlobal);
                JSString* str = JS_NewStringCopyZ(cx, "terminated");
                if (!str)
                    return false;
                args.rval().setString(str);
                return true;
            }
            return false;
        }

        if (saveBytecode) {
            saveBuffer = reinterpret_cast<uint8_t*>(JS_EncodeScript(cx, script, &saveLength));
            if (!saveBuffer)
                return false;
        }
    }

    if (saveBytecode) {
        // If we are both loading and saving, we assert that we are going to
        // replace the current bytecode by the same stream of bytes.
        if (loadBytecode && assertEqBytecode) {
            if (saveLength != loadLength) {
                JS_ReportErrorNumber(cx, my_GetErrorMessage, nullptr, JSSMSG_CACHE_EQ_SIZE_FAILED,
                                     loadLength, saveLength);
                return false;
            }

            if (!PodEqual(loadBuffer, saveBuffer.get(), loadLength)) {
                JS_ReportErrorNumber(cx, my_GetErrorMessage, nullptr,
                                     JSSMSG_CACHE_EQ_CONTENT_FAILED);
                return false;
            }
        }

        if (!CacheEntry_setBytecode(cx, cacheEntry, saveBuffer, saveLength))
            return false;

        saveBuffer.forget();
    }

    return JS_WrapValue(cx, args.rval());
}

JSString*
js::shell::FileAsString(JSContext* cx, const char* pathname)
{
    FILE* file;
    RootedString str(cx);
    size_t len, cc;
    char* buf;

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
                    char16_t* ucbuf =
                        JS::UTF8CharsToNewTwoByteCharsZ(cx, JS::UTF8Chars(buf, len), &len).get();
                    if (!ucbuf) {
                        JS_ReportError(cx, "Invalid UTF-8 in file '%s'", pathname);
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

/*
 * Function to run scripts and return compilation + execution time. Semantics
 * are closely modelled after the equivalent function in WebKit, as this is used
 * to produce benchmark timings by SunSpider.
 */
static bool
Run(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    if (args.length() != 1) {
        JS_ReportErrorNumber(cx, my_GetErrorMessage, nullptr, JSSMSG_INVALID_ARGS, "run");
        return false;
    }

    RootedString str(cx, JS::ToString(cx, args[0]));
    if (!str)
        return false;
    args[0].setString(str);
    JSAutoByteString filename(cx, str);
    if (!filename)
        return false;

    str = FileAsString(cx, filename.ptr());
    if (!str)
        return false;

    AutoStableStringChars chars(cx);
    if (!chars.initTwoByte(cx, str))
        return false;

    const char16_t* ucbuf = chars.twoByteRange().start().get();
    size_t buflen = str->length();

    RootedScript script(cx);
    int64_t startClock = PRMJ_Now();
    {
        JS::CompileOptions options(cx);
        options.setIntroductionType("js shell run")
               .setFileAndLine(filename.ptr(), 1)
               .setIsRunOnce(true)
               .setNoScriptRval(true);
        if (!JS_CompileUCScript(cx, ucbuf, buflen, options, &script))
            return false;
    }

    if (!JS_ExecuteScript(cx, script))
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
ReadLine(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

#define BUFSIZE 256
    FILE* from = stdin;
    size_t buflength = 0;
    size_t bufsize = BUFSIZE;
    char* buf = (char*) JS_malloc(cx, bufsize);
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
        char* tmp;
        bufsize *= 2;
        if (bufsize > buflength) {
            tmp = static_cast<char*>(JS_realloc(cx, buf, bufsize / 2, bufsize));
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
    char* tmp = static_cast<char*>(JS_realloc(cx, buf, bufsize, buflength));
    if (!tmp) {
        JS_free(cx, buf);
        return false;
    }

    buf = tmp;

    /*
     * Turn buf into a JSString. Note that buflength includes the trailing null
     * character.
     */
    JSString* str = JS_NewStringCopyN(cx, buf, sawNewline ? buflength - 1 : buflength);
    JS_free(cx, buf);
    if (!str)
        return false;

    args.rval().setString(str);
    return true;
}

static bool
PutStr(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (args.length() != 0) {
        RootedString str(cx, JS::ToString(cx, args[0]));
        if (!str)
            return false;
        char* bytes = JS_EncodeStringToUTF8(cx, str);
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
Now(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    double now = PRMJ_Now() / double(PRMJ_USEC_PER_MSEC);
    args.rval().setDouble(now);
    return true;
}

static bool
PrintInternal(JSContext* cx, const CallArgs& args, FILE* file)
{
    for (unsigned i = 0; i < args.length(); i++) {
        RootedString str(cx, JS::ToString(cx, args[i]));
        if (!str)
            return false;
        char* bytes = JS_EncodeStringToUTF8(cx, str);
        if (!bytes)
            return false;
        fprintf(file, "%s%s", i ? " " : "", bytes);
        JS_free(cx, bytes);
    }

    fputc('\n', file);
    fflush(file);

    args.rval().setUndefined();
    return true;
}

static bool
Print(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return PrintInternal(cx, args, gOutFile);
}

static bool
PrintErr(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return PrintInternal(cx, args, gErrFile);
}

static bool
Help(JSContext* cx, unsigned argc, Value* vp);

static bool
Quit(JSContext* cx, unsigned argc, Value* vp)
{
#ifdef JS_MORE_DETERMINISTIC
    // Print a message to stderr in more-deterministic builds to help jsfunfuzz
    // find uncatchable-exception bugs.
    fprintf(stderr, "quit called\n");
#endif

    CallArgs args = CallArgsFromVp(argc, vp);
    int32_t code;
    if (!ToInt32(cx, args.get(0), &code))
        return false;

    // The fuzzers check the shell's exit code and assume a value >= 128 means
    // the process crashed (for instance, SIGSEGV will result in code 139). On
    // POSIX platforms, the exit code is 8-bit and negative values can also
    // result in an exit code >= 128. We restrict the value to range [0, 127] to
    // avoid false positives.
    if (code < 0 || code >= 128) {
        JS_ReportError(cx, "quit exit code should be in range 0-127");
        return false;
    }

    gExitCode = code;
    gQuitting = true;
    return false;
}

static bool
StartTimingMutator(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    if (args.length() > 0) {
        JS_ReportErrorNumber(cx, my_GetErrorMessage, nullptr,
                             JSSMSG_TOO_MANY_ARGS, "startTimingMutator");
        return false;
    }

    cx->runtime()->gc.stats.startTimingMutator();
    args.rval().setUndefined();
    return true;
}

static bool
StopTimingMutator(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    if (args.length() > 0) {
        JS_ReportErrorNumber(cx, my_GetErrorMessage, nullptr,
                             JSSMSG_TOO_MANY_ARGS, "stopTimingMutator");
        return false;
    }

    double mutator_ms, gc_ms;
    if (!cx->runtime()->gc.stats.stopTimingMutator(mutator_ms, gc_ms)) {
        JS_ReportError(cx, "stopTimingMutator called when not timing the mutator");
        return false;
    }
    double total_ms = mutator_ms + gc_ms;
    if (total_ms > 0) {
        fprintf(gOutFile, "Mutator: %.3fms (%.1f%%), GC: %.3fms (%.1f%%)\n",
                mutator_ms, mutator_ms / total_ms * 100.0, gc_ms, gc_ms / total_ms * 100.0);
    }

    args.rval().setUndefined();
    return true;
}

static const char*
ToSource(JSContext* cx, MutableHandleValue vp, JSAutoByteString* bytes)
{
    JSString* str = JS_ValueToSource(cx, vp);
    if (str) {
        vp.setString(str);
        if (bytes->encodeLatin1(cx, str))
            return bytes->ptr();
    }
    JS_ClearPendingException(cx);
    return "<<error converting value to string>>";
}

static bool
AssertEq(JSContext* cx, unsigned argc, Value* vp)
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
        const char* actual = ToSource(cx, args[0], &bytes0);
        const char* expected = ToSource(cx, args[1], &bytes1);
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

static JSScript*
ValueToScript(JSContext* cx, Value vArg, JSFunction** funp = nullptr)
{
    RootedValue v(cx, vArg);
    RootedFunction fun(cx, JS_ValueToFunction(cx, v));
    if (!fun)
        return nullptr;

    // Unwrap bound functions.
    while (fun->isBoundFunction()) {
        JSObject* target = fun->getBoundFunctionTarget();
        if (target && target->is<JSFunction>())
            fun = &target->as<JSFunction>();
        else
            break;
    }

    if (!fun->isInterpreted()) {
        JS_ReportErrorNumber(cx, my_GetErrorMessage, nullptr, JSSMSG_SCRIPTS_ONLY);
        return nullptr;
    }

    JSScript* script = fun->getOrCreateScript(cx);
    if (!script)
        return nullptr;

    if (fun && funp)
        *funp = fun;

    return script;
}

static JSScript*
GetTopScript(JSContext* cx)
{
    NonBuiltinScriptFrameIter iter(cx);
    return iter.done() ? nullptr : iter.script();
}

static bool
GetScriptAndPCArgs(JSContext* cx, unsigned argc, Value* argv, MutableHandleScript scriptp,
                   int32_t* ip)
{
    RootedScript script(cx, GetTopScript(cx));
    *ip = 0;
    if (argc != 0) {
        Value v = argv[0];
        unsigned intarg = 0;
        if (v.isObject() &&
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

static bool
LineToPC(JSContext* cx, unsigned argc, Value* vp)
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

    jsbytecode* pc = LineNumberToPC(script, lineno);
    if (!pc)
        return false;
    args.rval().setInt32(script->pcToOffset(pc));
    return true;
}

static bool
PCToLine(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    RootedScript script(cx);
    int32_t i;
    unsigned lineno;

    if (!GetScriptAndPCArgs(cx, args.length(), args.array(), &script, &i))
        return false;
    lineno = PCToLineNumber(script, script->offsetToPC(i));
    if (!lineno)
        return false;
    args.rval().setInt32(lineno);
    return true;
}

#ifdef DEBUG

static void
UpdateSwitchTableBounds(JSContext* cx, HandleScript script, unsigned offset,
                        unsigned* start, unsigned* end)
{
    jsbytecode* pc;
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
        MOZ_ASSERT(op == JSOP_CONDSWITCH);
        return;
    }

    *start = script->pcToOffset(pc);
    *end = *start + (unsigned)(n * jmplen);
}

static void
SrcNotes(JSContext* cx, HandleScript script, Sprinter* sp)
{
    Sprint(sp, "\nSource notes:\n");
    Sprint(sp, "%4s %4s %5s %6s %-8s %s\n",
           "ofs", "line", "pc", "delta", "desc", "args");
    Sprint(sp, "---- ---- ----- ------ -------- ------\n");
    unsigned offset = 0;
    unsigned colspan = 0;
    unsigned lineno = script->lineno();
    jssrcnote* notes = script->notes();
    unsigned switchTableEnd = 0, switchTableStart = 0;
    for (jssrcnote* sn = notes; !SN_IS_TERMINATOR(sn); sn = SN_NEXT(sn)) {
        unsigned delta = SN_DELTA(sn);
        offset += delta;
        SrcNoteType type = (SrcNoteType) SN_TYPE(sn);
        const char* name = js_SrcNoteSpec[type].name;
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
            colspan = SN_OFFSET_TO_COLSPAN(GetSrcNoteOffset(sn, 0));
            Sprint(sp, "%d", colspan);
            break;

          case SRC_SETLINE:
            lineno = GetSrcNoteOffset(sn, 0);
            Sprint(sp, " lineno %u", lineno);
            break;

          case SRC_NEWLINE:
            ++lineno;
            break;

          case SRC_FOR:
            Sprint(sp, " cond %u update %u tail %u",
                   unsigned(GetSrcNoteOffset(sn, 0)),
                   unsigned(GetSrcNoteOffset(sn, 1)),
                   unsigned(GetSrcNoteOffset(sn, 2)));
            break;

          case SRC_IF_ELSE:
            Sprint(sp, " else %u", unsigned(GetSrcNoteOffset(sn, 0)));
            break;

          case SRC_FOR_IN:
          case SRC_FOR_OF:
            Sprint(sp, " closingjump %u", unsigned(GetSrcNoteOffset(sn, 0)));
            break;

          case SRC_COND:
          case SRC_WHILE:
          case SRC_NEXTCASE:
            Sprint(sp, " offset %u", unsigned(GetSrcNoteOffset(sn, 0)));
            break;

          case SRC_TABLESWITCH: {
            JSOp op = JSOp(script->code()[offset]);
            MOZ_ASSERT(op == JSOP_TABLESWITCH);
            Sprint(sp, " length %u", unsigned(GetSrcNoteOffset(sn, 0)));
            UpdateSwitchTableBounds(cx, script, offset,
                                    &switchTableStart, &switchTableEnd);
            break;
          }
          case SRC_CONDSWITCH: {
            JSOp op = JSOp(script->code()[offset]);
            MOZ_ASSERT(op == JSOP_CONDSWITCH);
            Sprint(sp, " length %u", unsigned(GetSrcNoteOffset(sn, 0)));
            unsigned caseOff = (unsigned) GetSrcNoteOffset(sn, 1);
            if (caseOff)
                Sprint(sp, " first case offset %u", caseOff);
            UpdateSwitchTableBounds(cx, script, offset,
                                    &switchTableStart, &switchTableEnd);
            break;
          }

          case SRC_TRY:
            MOZ_ASSERT(JSOp(script->code()[offset]) == JSOP_TRY);
            Sprint(sp, " offset to jump %u", unsigned(GetSrcNoteOffset(sn, 0)));
            break;

          default:
            MOZ_ASSERT(0);
            break;
        }
        Sprint(sp, "\n");
    }
}

static bool
Notes(JSContext* cx, unsigned argc, Value* vp)
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

    JSString* str = JS_NewStringCopyZ(cx, sprinter.string());
    if (!str)
        return false;
    args.rval().setString(str);
    return true;
}

JS_STATIC_ASSERT(JSTRY_CATCH == 0);
JS_STATIC_ASSERT(JSTRY_FINALLY == 1);
JS_STATIC_ASSERT(JSTRY_FOR_IN == 2);

static const char* const TryNoteNames[] = { "catch", "finally", "for-in", "for-of", "loop" };

static bool
TryNotes(JSContext* cx, HandleScript script, Sprinter* sp)
{
    if (!script->hasTrynotes())
        return true;

    JSTryNote* tn = script->trynotes()->vector;
    JSTryNote* tnlimit = tn + script->trynotes()->length;
    Sprint(sp, "\nException table:\nkind      stack    start      end\n");
    do {
        MOZ_ASSERT(tn->kind < ArrayLength(TryNoteNames));
        Sprint(sp, " %-7s %6u %8u %8u\n",
               TryNoteNames[tn->kind], tn->stackDepth,
               tn->start, tn->start + tn->length);
    } while (++tn != tnlimit);
    return true;
}

static bool
BlockNotes(JSContext* cx, HandleScript script, Sprinter* sp)
{
    if (!script->hasBlockScopes())
        return true;

    Sprint(sp, "\nBlock table:\n   index   parent    start      end\n");

    BlockScopeArray* scopes = script->blockScopes();
    for (uint32_t i = 0; i < scopes->length; i++) {
        const BlockScopeNote* note = &scopes->vector[i];
        if (note->index == BlockScopeNote::NoBlockScopeIndex)
            Sprint(sp, "%8s ", "(none)");
        else
            Sprint(sp, "%8u ", note->index);
        if (note->parent == BlockScopeNote::NoBlockScopeIndex)
            Sprint(sp, "%8s ", "(none)");
        else
            Sprint(sp, "%8u ", note->parent);
        Sprint(sp, "%8u %8u\n", note->start, note->start + note->length);
    }
    return true;
}

static bool
DisassembleScript(JSContext* cx, HandleScript script, HandleFunction fun, bool lines,
                  bool recursive, Sprinter* sp)
{
    if (fun) {
        Sprint(sp, "flags:");
        if (fun->isLambda())
            Sprint(sp, " LAMBDA");
        if (fun->isHeavyweight())
            Sprint(sp, " HEAVYWEIGHT");
        if (fun->isConstructor())
            Sprint(sp, " CONSTRUCTOR");
        if (fun->isExprBody())
            Sprint(sp, " EXPRESSION_CLOSURE");
        if (fun->isFunctionPrototype())
            Sprint(sp, " Function.prototype");
        if (fun->isSelfHostedBuiltin())
            Sprint(sp, " SELF_HOSTED");
        if (fun->isArrow())
            Sprint(sp, " ARROW");
        Sprint(sp, "\n");
    }

    if (!Disassemble(cx, script, lines, sp))
        return false;
    SrcNotes(cx, script, sp);
    TryNotes(cx, script, sp);
    BlockNotes(cx, script, sp);

    if (recursive && script->hasObjects()) {
        ObjectArray* objects = script->objects();
        for (unsigned i = 0; i != objects->length; ++i) {
            JSObject* obj = objects->vector[i];
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
    Value*  argv;
    bool    lines;
    bool    recursive;

    DisassembleOptionParser(unsigned argc, Value* argv)
      : argc(argc), argv(argv), lines(false), recursive(false) {}

    bool parse(JSContext* cx) {
        /* Read options off early arguments */
        while (argc > 0 && argv[0].isString()) {
            JSString* str = argv[0].toString();
            JSFlatString* flatStr = JS_FlattenString(cx, str);
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
DisassembleToSprinter(JSContext* cx, unsigned argc, Value* vp, Sprinter* sprinter)
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
            if (!Disassemble(cx, script, p.lines, sprinter))
                return false;
            SrcNotes(cx, script, sprinter);
            TryNotes(cx, script, sprinter);
            BlockNotes(cx, script, sprinter);
        }
    } else {
        for (unsigned i = 0; i < p.argc; i++) {
            RootedFunction fun(cx);
            RootedScript script(cx);
            RootedValue value(cx, p.argv[i]);
            if (value.isObject() && value.toObject().is<ModuleObject>())
                script = value.toObject().as<ModuleObject>().script();
            else
                script = ValueToScript(cx, value, fun.address());
            if (!script)
                return false;
            if (!DisassembleScript(cx, script, fun, p.lines, p.recursive, sprinter))
                return false;
        }
    }
    return true;
}

static bool
DisassembleToString(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    Sprinter sprinter(cx);
    if (!sprinter.init())
        return false;
    if (!DisassembleToSprinter(cx, args.length(), vp, &sprinter))
        return false;

    JSString* str = JS_NewStringCopyZ(cx, sprinter.string());
    if (!str)
        return false;
    args.rval().setString(str);
    return true;
}

static bool
Disassemble(JSContext* cx, unsigned argc, Value* vp)
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
DisassFile(JSContext* cx, unsigned argc, Value* vp)
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

    // We should change DisassembleOptionParser to store CallArgs.
    JSString* str = JS::ToString(cx, HandleValue::fromMarkedLocation(&p.argv[0]));
    if (!str)
        return false;
    JSAutoByteString filename(cx, str);
    if (!filename)
        return false;
    RootedScript script(cx);

    {
        CompileOptions options(cx);
        options.setIntroductionType("js shell disFile")
               .setUTF8(true)
               .setFileAndLine(filename.ptr(), 1)
               .setIsRunOnce(true)
               .setNoScriptRval(true);

        if (!JS::Compile(cx, options, filename.ptr(), &script))
            return false;
    }

    Sprinter sprinter(cx);
    if (!sprinter.init())
        return false;
    bool ok = DisassembleScript(cx, script, nullptr, p.lines, p.recursive, &sprinter);
    if (ok)
        fprintf(stdout, "%s\n", sprinter.string());
    if (!ok)
        return false;

    args.rval().setUndefined();
    return true;
}

static bool
DisassWithSrc(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

#define LINE_BUF_LEN 512
    unsigned len, line1, line2, bupline;
    FILE* file;
    char linebuf[LINE_BUF_LEN];
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

        jsbytecode* pc = script->code();
        jsbytecode* end = script->codeEnd();

        Sprinter sprinter(cx);
        if (!sprinter.init()) {
            ok = false;
            goto bail;
        }

        /* burn the leading lines */
        line2 = PCToLineNumber(script, pc);
        for (line1 = 0; line1 < line2 - 1; line1++) {
            char* tmp = fgets(linebuf, LINE_BUF_LEN, file);
            if (!tmp) {
                JS_ReportError(cx, "failed to read %s fully", script->filename());
                ok = false;
                goto bail;
            }
        }

        bupline = 0;
        while (pc < end) {
            line2 = PCToLineNumber(script, pc);

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

            len = Disassemble1(cx, script, pc, script->pcToOffset(pc), true, &sprinter);
            if (!len) {
                ok = false;
                goto bail;
            }
            pc += len;
        }

        fprintf(stdout, "%s\n", sprinter.string());

      bail:
        fclose(file);
    }
    args.rval().setUndefined();
    return ok;
#undef LINE_BUF_LEN
}

#endif /* DEBUG */

static bool
Intern(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    JSString* str = JS::ToString(cx, args.get(0));
    if (!str)
        return false;

    AutoStableStringChars strChars(cx);
    if (!strChars.initTwoByte(cx, str))
        return false;

    mozilla::Range<const char16_t> chars = strChars.twoByteRange();

    if (!JS_AtomizeAndPinUCStringN(cx, chars.start().get(), chars.length()))
        return false;

    args.rval().setUndefined();
    return true;
}

static bool
Clone(JSContext* cx, unsigned argc, Value* vp)
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
        RootedObject obj(cx, args[0].isPrimitive() ? nullptr : &args[0].toObject());

        if (obj && obj->is<CrossCompartmentWrapperObject>()) {
            obj = UncheckedUnwrap(obj);
            ac.emplace(cx, obj);
            args[0].setObject(*obj);
        }
        if (obj && obj->is<JSFunction>()) {
            funobj = obj;
        } else {
            JSFunction* fun = JS_ValueToFunction(cx, args[0]);
            if (!fun)
                return false;
            funobj = JS_GetFunctionObject(fun);
        }
    }

    if (args.length() > 1) {
        if (!JS_ValueToObject(cx, args[1], &parent))
            return false;
    } else {
        parent = js::GetGlobalForObjectCrossCompartment(&args.callee());
    }

    // Should it worry us that we might be getting with wrappers
    // around with wrappers here?
    JS::AutoObjectVector scopeChain(cx);
    if (!parent->is<GlobalObject>() && !scopeChain.append(parent))
        return false;
    JSObject* clone = JS::CloneFunctionObject(cx, funobj, scopeChain);
    if (!clone)
        return false;
    args.rval().setObject(*clone);
    return true;
}

static bool
GetSLX(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    RootedScript script(cx);

    script = ValueToScript(cx, args.get(0));
    if (!script)
        return false;
    args.rval().setInt32(GetScriptLineExtent(script));
    return true;
}

static bool
ThrowError(JSContext* cx, unsigned argc, Value* vp)
{
    JS_ReportError(cx, "This is an error");
    return false;
}

#define LAZY_STANDARD_CLASSES

/* A class for easily testing the inner/outer object callbacks. */
typedef struct ComplexObject {
    bool isInner;
    bool frozen;
    JSObject* inner;
    JSObject* outer;
} ComplexObject;

static bool
sandbox_enumerate(JSContext* cx, HandleObject obj)
{
    RootedValue v(cx);

    if (!JS_GetProperty(cx, obj, "lazy", &v))
        return false;

    if (!ToBoolean(v))
        return true;

    return JS_EnumerateStandardClasses(cx, obj);
}

static bool
sandbox_resolve(JSContext* cx, HandleObject obj, HandleId id, bool* resolvedp)
{
    RootedValue v(cx);
    if (!JS_GetProperty(cx, obj, "lazy", &v))
        return false;

    if (ToBoolean(v))
        return JS_ResolveStandardClass(cx, obj, id, resolvedp);
    return true;
}

static const JSClass sandbox_class = {
    "sandbox",
    JSCLASS_GLOBAL_FLAGS,
    nullptr, nullptr, nullptr, nullptr,
    sandbox_enumerate, sandbox_resolve,
    nullptr, nullptr, nullptr,
    nullptr, nullptr, nullptr,
    JS_GlobalObjectTraceHook
};

static JSObject*
NewSandbox(JSContext* cx, bool lazy)
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
EvalInContext(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    if (!args.requireAtLeast(cx, "evalcx", 1))
        return false;

    RootedString str(cx, ToString(cx, args[0]));
    if (!str)
        return false;

    RootedObject sobj(cx);
    if (args.hasDefined(1)) {
        sobj = ToObject(cx, args[1]);
        if (!sobj)
            return false;
    }

    AutoStableStringChars strChars(cx);
    if (!strChars.initTwoByte(cx, str))
        return false;

    mozilla::Range<const char16_t> chars = strChars.twoByteRange();
    size_t srclen = chars.length();
    const char16_t* src = chars.start().get();

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
        args.rval().setObject(*sobj);
        return true;
    }

    JS::AutoFilename filename;
    unsigned lineno;

    DescribeScriptedCaller(cx, &filename, &lineno);
    {
        Maybe<JSAutoCompartment> ac;
        unsigned flags;
        JSObject* unwrapped = UncheckedUnwrap(sobj, true, &flags);
        if (flags & Wrapper::CROSS_COMPARTMENT) {
            sobj = unwrapped;
            ac.emplace(cx, sobj);
        }

        sobj = GetInnerObject(sobj);
        if (!sobj)
            return false;
        if (!(sobj->getClass()->flags & JSCLASS_IS_GLOBAL)) {
            JS_ReportError(cx, "Invalid scope argument to evalcx");
            return false;
        }
        JS::CompileOptions opts(cx);
        opts.setFileAndLine(filename.get(), lineno);
        if (!JS::Evaluate(cx, opts, src, srclen, args.rval())) {
            return false;
        }
    }

    if (!cx->compartment()->wrap(cx, args.rval()))
        return false;

    return true;
}

struct WorkerInput
{
    JSRuntime* runtime;
    char16_t* chars;
    size_t length;

    WorkerInput(JSRuntime* runtime, char16_t* chars, size_t length)
      : runtime(runtime), chars(chars), length(length)
    {}

    ~WorkerInput() {
        js_free(chars);
    }
};

static void SetWorkerRuntimeOptions(JSRuntime* rt);

static void
WorkerMain(void* arg)
{
    WorkerInput* input = (WorkerInput*) arg;

    JSRuntime* rt = JS_NewRuntime(8L * 1024L * 1024L, 2L * 1024L * 1024L, input->runtime);
    if (!rt) {
        js_delete(input);
        return;
    }
    JS_SetErrorReporter(rt, my_ErrorReporter);
    SetWorkerRuntimeOptions(rt);

    JSContext* cx = NewContext(rt);
    if (!cx) {
        JS_DestroyRuntime(rt);
        js_delete(input);
        return;
    }

    JS::SetLargeAllocationFailureCallback(rt, my_LargeAllocFailCallback, (void*)cx);

    do {
        JSAutoRequest ar(cx);

        JS::CompartmentOptions compartmentOptions;
        compartmentOptions.setVersion(JSVERSION_LATEST);
        RootedObject global(cx, NewGlobalObject(cx, compartmentOptions, nullptr));
        if (!global)
            break;

        JSAutoCompartment ac(cx, global);

        JS::CompileOptions options(cx);
        options.setFileAndLine("<string>", 1)
               .setIsRunOnce(true);

        RootedScript script(cx);
        if (!JS::Compile(cx, options, input->chars, input->length, &script))
            break;
        RootedValue result(cx);
        JS_ExecuteScript(cx, script, &result);
    } while (0);

    JS::SetLargeAllocationFailureCallback(rt, nullptr, nullptr);

    DestroyContext(cx, false);
    JS_DestroyRuntime(rt);

    js_delete(input);
}

Vector<PRThread*, 0, SystemAllocPolicy> workerThreads;

static bool
EvalInWorker(JSContext* cx, unsigned argc, Value* vp)
{
    if (!CanUseExtraThreads()) {
        JS_ReportError(cx, "Can't create worker threads with --no-threads");
        return false;
    }

    CallArgs args = CallArgsFromVp(argc, vp);
    if (!args.get(0).isString()) {
        JS_ReportError(cx, "Invalid arguments to evalInWorker");
        return false;
    }

    if (!args[0].toString()->ensureLinear(cx))
        return false;

    JSLinearString* str = &args[0].toString()->asLinear();

    char16_t* chars = (char16_t*) js_malloc(str->length() * sizeof(char16_t));
    if (!chars)
        return false;
    CopyChars(chars, *str);

    WorkerInput* input = js_new<WorkerInput>(cx->runtime(), chars, str->length());
    if (!input)
        return false;

    PRThread* thread = PR_CreateThread(PR_USER_THREAD, WorkerMain, input,
                                       PR_PRIORITY_NORMAL, PR_GLOBAL_THREAD, PR_JOINABLE_THREAD, 0);
    if (!thread || !workerThreads.append(thread))
        return false;

    return true;
}

static bool
ShapeOf(JSContext* cx, unsigned argc, JS::Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    if (!args.get(0).isObject()) {
        JS_ReportError(cx, "shapeOf: object expected");
        return false;
    }
    JSObject* obj = &args[0].toObject();
    args.rval().set(JS_NumberValue(double(uintptr_t(obj->maybeShape()) >> 3)));
    return true;
}

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
Sleep_fn(JSContext* cx, unsigned argc, Value* vp)
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
        PR_WaitCondVar(gSleepWakeup, PR_MillisecondsToInterval(t_ticks / 1000));
        if (gServiceInterrupt)
            break;
        int64_t now = PRMJ_Now();
        if (!IsBefore(now, to_wakeup))
            break;
        t_ticks = to_wakeup - now;
    }
    PR_Unlock(gWatchdogLock);
    args.rval().setUndefined();
    return !gServiceInterrupt;
}

static bool
InitWatchdog(JSRuntime* rt)
{
    MOZ_ASSERT(!gWatchdogThread);
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
    PRThread* thread;

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
WatchdogMain(void* arg)
{
    PR_SetCurrentThreadName("JS Watchdog");

    JSRuntime* rt = (JSRuntime*) arg;

    PR_Lock(gWatchdogLock);
    while (gWatchdogThread) {
        int64_t now = PRMJ_Now();
        if (gWatchdogHasTimeout && !IsBefore(now, gWatchdogTimeout)) {
            /*
             * The timeout has just expired. Request an interrupt callback
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
                 * Time hasn't expired yet. Simulate an interrupt callback
                 * which doesn't abort execution.
                 */
                JS_RequestInterruptCallback(rt);
            }

            uint64_t sleepDuration = PR_INTERVAL_NO_TIMEOUT;
            if (gWatchdogHasTimeout)
                sleepDuration = PR_TicksPerSecond() / 10;
            mozilla::DebugOnly<PRStatus> status =
              PR_WaitCondVar(gWatchdogWakeup, sleepDuration);
            MOZ_ASSERT(status == PR_SUCCESS);
        }
    }
    PR_Unlock(gWatchdogLock);
}

static bool
ScheduleWatchdog(JSRuntime* rt, double t)
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
        MOZ_ASSERT(!gWatchdogHasTimeout);
        gWatchdogThread = PR_CreateThread(PR_USER_THREAD,
                                          WatchdogMain,
                                          rt,
                                          PR_PRIORITY_NORMAL,
                                          PR_GLOBAL_THREAD,
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

static void
CancelExecution(JSRuntime* rt)
{
    gServiceInterrupt = true;
    JS_RequestInterruptCallback(rt);

    if (!gInterruptFunc.isNull()) {
        static const char msg[] = "Script runs for too long, terminating.\n";
        fputs(msg, stderr);
    }
}

static bool
SetTimeoutValue(JSContext* cx, double t)
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
Timeout(JSContext* cx, unsigned argc, Value* vp)
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
        gInterruptFunc = value;
    }

    args.rval().setUndefined();
    return SetTimeoutValue(cx, t);
}

static bool
InterruptIf(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (args.length() != 1) {
        JS_ReportError(cx, "Wrong number of arguments");
        return false;
    }

    if (ToBoolean(args[0])) {
        gServiceInterrupt = true;
        JS_RequestInterruptCallback(cx->runtime());
    }

    args.rval().setUndefined();
    return true;
}

static bool
InvokeInterruptCallbackWrapper(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    if (args.length() != 1) {
        JS_ReportError(cx, "Wrong number of arguments");
        return false;
    }

    gServiceInterrupt = true;
    JS_RequestInterruptCallback(cx->runtime());
    bool interruptRv = CheckForInterrupt(cx);

    // The interrupt handler could have set a pending exception. Since we call
    // back into JS, don't have it see the pending exception. If we have an
    // uncatchable exception that's not propagating a debug mode forced
    // return, return.
    if (!interruptRv && !cx->isExceptionPending() && !cx->isPropagatingForcedReturn())
        return false;

    JS::AutoSaveExceptionState savedExc(cx);
    Value argv[1] = { BooleanValue(interruptRv) };
    RootedValue rv(cx);
    if (!Invoke(cx, UndefinedValue(), args[0], 1, argv, &rv))
        return false;

    args.rval().setUndefined();
    return interruptRv;
}

static bool
SetInterruptCallback(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (args.length() != 1) {
        JS_ReportError(cx, "Wrong number of arguments");
        return false;
    }

    RootedValue value(cx, args[0]);
    if (!value.isObject() || !value.toObject().is<JSFunction>()) {
        JS_ReportError(cx, "Argument must be a function");
        return false;
    }
    gInterruptFunc = value;

    args.rval().setUndefined();
    return true;
}

static bool
EnableLastWarning(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    gLastWarningEnabled = true;
    gLastWarning.setNull();

    args.rval().setUndefined();
    return true;
}

static bool
DisableLastWarning(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    gLastWarningEnabled = false;
    gLastWarning.setNull();

    args.rval().setUndefined();
    return true;
}

static bool
GetLastWarning(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (!gLastWarningEnabled) {
        JS_ReportError(cx, "Call enableLastWarning first.");
        return false;
    }

    if (!JS_WrapValue(cx, &gLastWarning))
        return false;

    args.rval().set(gLastWarning);
    return true;
}

static bool
ClearLastWarning(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (!gLastWarningEnabled) {
        JS_ReportError(cx, "Call enableLastWarning first.");
        return false;
    }

    gLastWarning.setNull();

    args.rval().setUndefined();
    return true;
}

#ifdef DEBUG
static bool
StackDump(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    bool showArgs = ToBoolean(args.get(0));
    bool showLocals = ToBoolean(args.get(1));
    bool showThisProps = ToBoolean(args.get(2));

    char* buf = JS::FormatStackDump(cx, nullptr, showArgs, showLocals, showThisProps);
    if (!buf) {
        fputs("Failed to format JavaScript stack for dump\n", gOutFile);
    } else {
        fputs(buf, gOutFile);
        JS_smprintf_free(buf);
    }

    args.rval().setUndefined();
    return true;
}
#endif

static bool
StackPointerInfo(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    // Copy the truncated stack pointer to the result.  This value is not used
    // as a pointer but as a way to measure frame-size from JS.
    args.rval().setInt32(int32_t(reinterpret_cast<size_t>(&args) & 0xfffffff));
    return true;
}


static bool
Elapsed(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    if (args.length() == 0) {
        double d = 0.0;
        JSShellContextData* data = GetContextData(cx);
        if (data)
            d = PRMJ_Now() - data->startTime;
        args.rval().setDouble(d);
        return true;
    }
    JS_ReportError(cx, "Wrong number of arguments");
    return false;
}

static bool
Compile(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    if (args.length() < 1) {
        JS_ReportErrorNumber(cx, GetErrorMessage, nullptr, JSMSG_MORE_ARGS_NEEDED,
                             "compile", "0", "s");
        return false;
    }
    if (!args[0].isString()) {
        const char* typeName = InformalValueTypeName(args[0]);
        JS_ReportError(cx, "expected string to compile, got %s", typeName);
        return false;
    }

    RootedObject global(cx, JS::CurrentGlobalOrNull(cx));
    JSFlatString* scriptContents = args[0].toString()->ensureFlat(cx);
    if (!scriptContents)
        return false;

    AutoStableStringChars stableChars(cx);
    if (!stableChars.initTwoByte(cx, scriptContents))
        return false;

    JS::CompileOptions options(cx);
    options.setIntroductionType("js shell compile")
           .setFileAndLine("<string>", 1)
           .setIsRunOnce(true)
           .setNoScriptRval(true);
    RootedScript script(cx);
    const char16_t* chars = stableChars.twoByteRange().start().get();
    bool ok = JS_CompileUCScript(cx, chars, scriptContents->length(), options, &script);
    args.rval().setUndefined();
    return ok;
}

static bool
ParseModule(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    if (args.length() < 1) {
        JS_ReportErrorNumber(cx, GetErrorMessage, nullptr,
                             JSMSG_MORE_ARGS_NEEDED, "parseModule", "0", "s");
        return false;
    }
    if (!args[0].isString()) {
        const char* typeName = InformalValueTypeName(args[0]);
        JS_ReportError(cx, "expected string to compile, got %s", typeName);
        return false;
    }

    RootedObject global(cx, JS::CurrentGlobalOrNull(cx));
    JSFlatString* scriptContents = args[0].toString()->ensureFlat(cx);
    if (!scriptContents)
        return false;

    CompileOptions options(cx);
    options.setFileAndLine("<string>", 1);

    AutoStableStringChars stableChars(cx);
    if (!stableChars.initTwoByte(cx, scriptContents))
        return false;

    const char16_t* chars = stableChars.twoByteRange().start().get();
    SourceBufferHolder srcBuf(chars, scriptContents->length(),
                              SourceBufferHolder::NoOwnership);

    RootedObject module(cx, frontend::CompileModule(cx, global, options, srcBuf));
    if (!module)
        return false;

    args.rval().setObject(*module);
    return true;
}

static bool
Parse(JSContext* cx, unsigned argc, Value* vp)
{
    using namespace js::frontend;

    CallArgs args = CallArgsFromVp(argc, vp);

    if (args.length() < 1) {
        JS_ReportErrorNumber(cx, GetErrorMessage, nullptr, JSMSG_MORE_ARGS_NEEDED,
                             "parse", "0", "s");
        return false;
    }
    if (!args[0].isString()) {
        const char* typeName = InformalValueTypeName(args[0]);
        JS_ReportError(cx, "expected string to parse, got %s", typeName);
        return false;
    }

    JSFlatString* scriptContents = args[0].toString()->ensureFlat(cx);
    if (!scriptContents)
        return false;

    AutoStableStringChars stableChars(cx);
    if (!stableChars.initTwoByte(cx, scriptContents))
        return false;

    size_t length = scriptContents->length();
    const char16_t* chars = stableChars.twoByteRange().start().get();

    CompileOptions options(cx);
    options.setIntroductionType("js shell parse")
           .setFileAndLine("<string>", 1);
    Parser<FullParseHandler> parser(cx, &cx->tempLifoAlloc(), options, chars, length,
                                    /* foldConstants = */ true, nullptr, nullptr);
    if (!parser.checkOptions())
        return false;

    ParseNode* pn = parser.parse();
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
SyntaxParse(JSContext* cx, unsigned argc, Value* vp)
{
    using namespace js::frontend;

    CallArgs args = CallArgsFromVp(argc, vp);

    if (args.length() < 1) {
        JS_ReportErrorNumber(cx, GetErrorMessage, nullptr, JSMSG_MORE_ARGS_NEEDED,
                             "parse", "0", "s");
        return false;
    }
    if (!args[0].isString()) {
        const char* typeName = InformalValueTypeName(args[0]);
        JS_ReportError(cx, "expected string to parse, got %s", typeName);
        return false;
    }

    JSFlatString* scriptContents = args[0].toString()->ensureFlat(cx);
    if (!scriptContents)
        return false;
    CompileOptions options(cx);
    options.setIntroductionType("js shell syntaxParse")
           .setFileAndLine("<string>", 1);

    AutoStableStringChars stableChars(cx);
    if (!stableChars.initTwoByte(cx, scriptContents))
        return false;

    const char16_t* chars = stableChars.twoByteRange().start().get();
    size_t length = scriptContents->length();
    Parser<frontend::SyntaxParseHandler> parser(cx, &cx->tempLifoAlloc(),
                                                options, chars, length, false, nullptr, nullptr);
    if (!parser.checkOptions())
        return false;

    bool succeeded = parser.parse();
    if (cx->isExceptionPending())
        return false;

    if (!succeeded && !parser.hadAbortedSyntaxParse()) {
        // If no exception is posted, either there was an OOM or a language
        // feature unhandled by the syntax parser was encountered.
        MOZ_ASSERT(cx->runtime()->hadOutOfMemory);
        return false;
    }

    args.rval().setBoolean(succeeded);
    return true;
}

class OffThreadState {
  public:
    enum State {
        IDLE,           /* ready to work; no token, no source */
        COMPILING,      /* working; no token, have source */
        DONE            /* compilation done: have token and source */
    };

    OffThreadState() : monitor(), state(IDLE), token(), source(nullptr) { }
    bool init() { return monitor.init(); }

    bool startIfIdle(JSContext* cx, ScopedJSFreePtr<char16_t>& newSource) {
        AutoLockMonitor alm(monitor);
        if (state != IDLE)
            return false;

        MOZ_ASSERT(!token);

        source = newSource.forget();

        state = COMPILING;
        return true;
    }

    void abandon(JSContext* cx) {
        AutoLockMonitor alm(monitor);
        MOZ_ASSERT(state == COMPILING);
        MOZ_ASSERT(!token);
        MOZ_ASSERT(source);

        js_free(source);
        source = nullptr;

        state = IDLE;
    }

    void markDone(void* newToken) {
        AutoLockMonitor alm(monitor);
        MOZ_ASSERT(state == COMPILING);
        MOZ_ASSERT(!token);
        MOZ_ASSERT(source);
        MOZ_ASSERT(newToken);

        token = newToken;
        state = DONE;
        alm.notify();
    }

    void* waitUntilDone(JSContext* cx) {
        AutoLockMonitor alm(monitor);
        if (state == IDLE)
            return nullptr;

        if (state == COMPILING) {
            while (state != DONE)
                alm.wait();
        }

        MOZ_ASSERT(source);
        js_free(source);
        source = nullptr;

        MOZ_ASSERT(token);
        void* holdToken = token;
        token = nullptr;
        state = IDLE;
        return holdToken;
    }

  private:
    Monitor monitor;
    State state;
    void* token;
    char16_t* source;
};

static OffThreadState offThreadState;

static void
OffThreadCompileScriptCallback(void* token, void* callbackData)
{
    offThreadState.markDone(token);
}

static bool
OffThreadCompileScript(JSContext* cx, unsigned argc, Value* vp)
{
    if (!CanUseExtraThreads()) {
        JS_ReportError(cx, "Can't use offThreadCompileScript with --no-threads");
        return false;
    }

    CallArgs args = CallArgsFromVp(argc, vp);

    if (args.length() < 1) {
        JS_ReportErrorNumber(cx, GetErrorMessage, nullptr, JSMSG_MORE_ARGS_NEEDED,
                             "offThreadCompileScript", "0", "s");
        return false;
    }
    if (!args[0].isString()) {
        const char* typeName = InformalValueTypeName(args[0]);
        JS_ReportError(cx, "expected string to parse, got %s", typeName);
        return false;
    }

    JSAutoByteString fileNameBytes;
    CompileOptions options(cx);
    options.setIntroductionType("js shell offThreadCompileScript")
           .setFileAndLine("<string>", 1);

    if (args.length() >= 2) {
        if (args[1].isPrimitive()) {
            JS_ReportErrorNumber(cx, my_GetErrorMessage, nullptr, JSSMSG_INVALID_ARGS, "evaluate");
            return false;
        }

        RootedObject opts(cx, &args[1].toObject());
        if (!ParseCompileOptions(cx, options, opts, fileNameBytes))
            return false;
    }

    // These option settings must override whatever the caller requested.
    options.setIsRunOnce(true)
           .setSourceIsLazy(false);

    // We assume the caller wants caching if at all possible, ignoring
    // heuristics that make sense for a real browser.
    options.forceAsync = true;

    JSString* scriptContents = args[0].toString();
    AutoStableStringChars stableChars(cx);
    if (!stableChars.initTwoByte(cx, scriptContents))
        return false;

    size_t length = scriptContents->length();
    const char16_t* chars = stableChars.twoByteRange().start().get();

    // Make sure we own the string's chars, so that they are not freed before
    // the compilation is finished.
    ScopedJSFreePtr<char16_t> ownedChars;
    if (stableChars.maybeGiveOwnershipToCaller()) {
        ownedChars = const_cast<char16_t*>(chars);
    } else {
        char16_t* copy = cx->pod_malloc<char16_t>(length);
        if (!copy)
            return false;

        mozilla::PodCopy(copy, chars, length);
        ownedChars = copy;
        chars = copy;
    }

    if (!JS::CanCompileOffThread(cx, options, length)) {
        JS_ReportError(cx, "cannot compile code on worker thread");
        return false;
    }

    if (!offThreadState.startIfIdle(cx, ownedChars)) {
        JS_ReportError(cx, "called offThreadCompileScript without calling runOffThreadScript"
                       " to receive prior off-thread compilation");
        return false;
    }

    if (!JS::CompileOffThread(cx, options, chars, length,
                              OffThreadCompileScriptCallback, nullptr))
    {
        offThreadState.abandon(cx);
        return false;
    }

    args.rval().setUndefined();
    return true;
}

static bool
runOffThreadScript(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    JSRuntime* rt = cx->runtime();
    if (OffThreadParsingMustWaitForGC(rt))
        gc::AutoFinishGC finishgc(rt);

    void* token = offThreadState.waitUntilDone(cx);
    if (!token) {
        JS_ReportError(cx, "called runOffThreadScript when no compilation is pending");
        return false;
    }

    RootedScript script(cx, JS::FinishOffThreadScript(cx, rt, token));
    if (!script)
        return false;

    return JS_ExecuteScript(cx, script, args.rval());
}

struct FreeOnReturn
{
    JSContext* cx;
    const char* ptr;
    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER

    explicit FreeOnReturn(JSContext* cx, const char* ptr = nullptr
                 MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
      : cx(cx), ptr(ptr)
    {
        MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    }

    void init(const char* ptr) {
        MOZ_ASSERT(!this->ptr);
        this->ptr = ptr;
    }

    ~FreeOnReturn() {
        JS_free(cx, (void*)ptr);
    }
};

static int sArgc;
static char** sArgv;

class AutoCStringVector
{
    Vector<char*> argv_;
  public:
    explicit AutoCStringVector(JSContext* cx) : argv_(cx) {}
    ~AutoCStringVector() {
        for (size_t i = 0; i < argv_.length(); i++)
            js_free(argv_[i]);
    }
    bool append(char* arg) {
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
    char* operator[](size_t i) const {
        return argv_[i];
    }
    void replace(size_t i, char* arg) {
        js_free(argv_[i]);
        argv_[i] = arg;
    }
    char* back() const {
        return argv_.back();
    }
    void replaceBack(char* arg) {
        js_free(argv_.back());
        argv_.back() = arg;
    }
};

#if defined(XP_WIN)
static bool
EscapeForShell(AutoCStringVector& argv)
{
    // Windows will break arguments in argv by various spaces, so we wrap each
    // argument in quotes and escape quotes within. Even with quotes, \ will be
    // treated like an escape character, so inflate each \ to \\.

    for (size_t i = 0; i < argv.length(); i++) {
        if (!argv[i])
            continue;

        size_t newLen = 3;  // quotes before and after and null-terminator
        for (char* p = argv[i]; *p; p++) {
            newLen++;
            if (*p == '\"' || *p == '\\')
                newLen++;
        }

        char* escaped = (char*)js_malloc(newLen);
        if (!escaped)
            return false;

        char* src = argv[i];
        char* dst = escaped;
        *dst++ = '\"';
        while (*src) {
            if (*src == '\"' || *src == '\\')
                *dst++ = '\\';
            *dst++ = *src++;
        }
        *dst++ = '\"';
        *dst++ = '\0';
        MOZ_ASSERT(escaped + newLen == dst);

        argv.replace(i, escaped);
    }
    return true;
}
#endif

static Vector<const char*, 4, js::SystemAllocPolicy> sPropagatedFlags;

static bool
PropagateFlagToNestedShells(const char* flag)
{
    return sPropagatedFlags.append(flag);
}

static bool
NestedShell(JSContext* cx, unsigned argc, Value* vp)
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
        char* cstr = strdup(sPropagatedFlags[i]);
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
            char* newArg = JS_smprintf("--js-cache=%s", jsCacheDir);
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
DecompileFunctionSomehow(JSContext* cx, unsigned argc, Value* vp,
                         JSString* (*decompiler)(JSContext*, HandleFunction, unsigned))
{
    CallArgs args = CallArgsFromVp(argc, vp);
    if (args.length() < 1 || !args[0].isObject() || !args[0].toObject().is<JSFunction>()) {
        args.rval().setUndefined();
        return true;
    }
    RootedFunction fun(cx, &args[0].toObject().as<JSFunction>());
    JSString* result = decompiler(cx, fun, 0);
    if (!result)
        return false;
    args.rval().setString(result);
    return true;
}

static bool
DecompileBody(JSContext* cx, unsigned argc, Value* vp)
{
    return DecompileFunctionSomehow(cx, argc, vp, JS_DecompileFunctionBody);
}

static bool
DecompileFunction(JSContext* cx, unsigned argc, Value* vp)
{
    return DecompileFunctionSomehow(cx, argc, vp, JS_DecompileFunction);
}

static bool
DecompileThisScript(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    NonBuiltinScriptFrameIter iter(cx);
    if (iter.done()) {
        args.rval().setString(cx->runtime()->emptyString);
        return true;
    }

    {
        JSAutoCompartment ac(cx, iter.script());

        RootedScript script(cx, iter.script());
        JSString* result = JS_DecompileScript(cx, script, "test", 0);
        if (!result)
            return false;

        args.rval().setString(result);
    }

    return JS_WrapValue(cx, args.rval());
}

static bool
ThisFilename(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    JS::AutoFilename filename;
    if (!DescribeScriptedCaller(cx, &filename) || !filename.get()) {
        args.rval().setString(cx->runtime()->emptyString);
        return true;
    }

    JSString* str = JS_NewStringCopyZ(cx, filename.get());
    if (!str)
        return false;

    args.rval().setString(str);
    return true;
}

static bool
WrapWithProto(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    Value obj = UndefinedValue(), proto = UndefinedValue();
    if (args.length() == 2) {
        obj = args[0];
        proto = args[1];
    }
    if (!obj.isObject() || !proto.isObjectOrNull()) {
        JS_ReportErrorNumber(cx, my_GetErrorMessage, nullptr, JSSMSG_INVALID_ARGS,
                             "wrapWithProto");
        return false;
    }

    WrapperOptions options(cx);
    options.setProto(proto.toObjectOrNull());
    JSObject* wrapped = Wrapper::New(cx, &obj.toObject(),
                                     &Wrapper::singletonWithPrototype, options);
    if (!wrapped)
        return false;

    args.rval().setObject(*wrapped);
    return true;
}

static bool
NewGlobal(JSContext* cx, unsigned argc, Value* vp)
{
    JSPrincipals* principals = nullptr;
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

        if (!JS_GetProperty(cx, opts, "principal", &v))
            return false;
        if (!v.isUndefined()) {
            uint32_t bits;
            if (!ToUint32(cx, v, &bits))
                return false;
            principals = cx->new_<ShellPrincipals>(bits);
            if (!principals)
                return false;
            JS_HoldPrincipals(principals);
        }
    }

    RootedObject global(cx, NewGlobalObject(cx, options, principals));
    if (principals)
        JS_DropPrincipals(cx->runtime(), principals);
    if (!global)
        return false;

    if (!JS_WrapObject(cx, &global))
        return false;

    args.rval().setObject(*global);
    return true;
}

static bool
GetMaxArgs(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    args.rval().setInt32(ARGS_LENGTH_MAX);
    return true;
}

static bool
ObjectEmulatingUndefined(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    static const JSClass cls = {
        "ObjectEmulatingUndefined",
        JSCLASS_EMULATES_UNDEFINED
    };

    RootedObject obj(cx, JS_NewObject(cx, &cls));
    if (!obj)
        return false;
    args.rval().setObject(*obj);
    return true;
}

static bool
GetSelfHostedValue(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (args.length() != 1 || !args[0].isString()) {
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
    // The function we should call to lazily retrieve source code.
    PersistentRootedFunction fun;

  public:
    ShellSourceHook(JSContext* cx, JSFunction& fun) : fun(cx, &fun) {}

    bool load(JSContext* cx, const char* filename, char16_t** src, size_t* length) {
        RootedString str(cx, JS_NewStringCopyZ(cx, filename));
        if (!str)
            return false;
        RootedValue filenameValue(cx, StringValue(str));

        RootedValue result(cx);
        if (!Call(cx, UndefinedHandleValue, fun, HandleValueArray(filenameValue), &result))
            return false;

        str = JS::ToString(cx, result);
        if (!str)
            return false;

        *length = JS_GetStringLength(str);
        *src = cx->pod_malloc<char16_t>(*length);
        if (!*src)
            return false;

        JSLinearString* linear = str->ensureLinear(cx);
        if (!linear)
            return false;

        CopyChars(*src, *linear);
        return true;
    }
};

static bool
WithSourceHook(JSContext* cx, unsigned argc, Value* vp)
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

    UniquePtr<ShellSourceHook> hook =
        MakeUnique<ShellSourceHook>(cx, args[0].toObject().as<JSFunction>());
    if (!hook)
        return false;

    UniquePtr<SourceHook> savedHook = js::ForgetSourceHook(cx->runtime());
    js::SetSourceHook(cx->runtime(), Move(hook));

    RootedObject fun(cx, &args[1].toObject());
    bool result = Call(cx, UndefinedHandleValue, fun, JS::HandleValueArray::empty(), args.rval());
    js::SetSourceHook(cx->runtime(), Move(savedHook));
    return result;
}

static bool
IsCachingEnabled(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    args.rval().setBoolean(jsCachingEnabled && jsCacheAsmJSPath != nullptr);
    return true;
}

static bool
SetCachingEnabled(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    jsCachingEnabled = ToBoolean(args.get(0));
    args.rval().setUndefined();
    return true;
}

static void
PrintProfilerEvents_Callback(const char* msg)
{
    fprintf(stderr, "PROFILER EVENT: %s\n", msg);
}

static bool
PrintProfilerEvents(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    if (cx->runtime()->spsProfiler.enabled())
        js::RegisterRuntimeProfilingEventMarker(cx->runtime(), &PrintProfilerEvents_Callback);
    args.rval().setUndefined();
    return true;
}

#if defined(JS_SIMULATOR_ARM)
typedef Vector<char16_t, 0, SystemAllocPolicy> StackChars;
Vector<StackChars, 0, SystemAllocPolicy> stacks;

static void
SingleStepCallback(void* arg, jit::Simulator* sim, void* pc)
{
    JSRuntime* rt = reinterpret_cast<JSRuntime*>(arg);

    // If profiling is not enabled, don't do anything.
    if (!rt->spsProfiler.enabled())
        return;

    JS::ProfilingFrameIterator::RegisterState state;
    state.pc = pc;
    state.sp = (void*)sim->get_register(jit::Simulator::sp);
    state.lr = (void*)sim->get_register(jit::Simulator::lr);

    DebugOnly<void*> lastStackAddress = nullptr;
    StackChars stack;
    uint32_t frameNo = 0;
    for (JS::ProfilingFrameIterator i(rt, state); !i.done(); ++i) {
        MOZ_ASSERT(i.stackAddress() != nullptr);
        MOZ_ASSERT(lastStackAddress <= i.stackAddress());
        lastStackAddress = i.stackAddress();
        JS::ProfilingFrameIterator::Frame frames[16];
        uint32_t nframes = i.extractStack(frames, 0, 16);
        for (uint32_t i = 0; i < nframes; i++) {
            if (frameNo > 0)
                stack.append(",", 1);
            stack.append(frames[i].label, strlen(frames[i].label));
            frameNo++;
        }
    }

    // Only append the stack if it differs from the last stack.
    if (stacks.empty() ||
        stacks.back().length() != stack.length() ||
        !PodEqual(stacks.back().begin(), stack.begin(), stack.length()))
    {
        stacks.append(Move(stack));
    }
}
#endif

static bool
EnableSingleStepProfiling(JSContext* cx, unsigned argc, Value* vp)
{
#if defined(JS_SIMULATOR_ARM)
    CallArgs args = CallArgsFromVp(argc, vp);

    jit::Simulator* sim = cx->runtime()->simulator();
    sim->enable_single_stepping(SingleStepCallback, cx->runtime());

    args.rval().setUndefined();
    return true;
#else
    JS_ReportError(cx, "single-step profiling not enabled on this platform");
    return false;
#endif
}

static bool
DisableSingleStepProfiling(JSContext* cx, unsigned argc, Value* vp)
{
#if defined(JS_SIMULATOR_ARM)
    CallArgs args = CallArgsFromVp(argc, vp);

    jit::Simulator* sim = cx->runtime()->simulator();
    sim->disable_single_stepping();

    AutoValueVector elems(cx);
    for (size_t i = 0; i < stacks.length(); i++) {
        JSString* stack = JS_NewUCStringCopyN(cx, stacks[i].begin(), stacks[i].length());
        if (!stack)
            return false;
        if (!elems.append(StringValue(stack)))
            return false;
    }

    JSObject* array = JS_NewArrayObject(cx, elems);
    if (!array)
        return false;

    stacks.clear();
    args.rval().setObject(*array);
    return true;
#else
    JS_ReportError(cx, "single-step profiling not enabled on this platform");
    return false;
#endif
}

static bool
IsLatin1(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    bool isLatin1 = args.get(0).isString() && args[0].toString()->hasLatin1Chars();
    args.rval().setBoolean(isLatin1);
    return true;
}

// Global mailbox that is used to communicate a SharedArrayBuffer
// value from one worker to another.
//
// For simplicity we store only the SharedArrayRawBuffer; retaining
// the SAB object would require per-runtime storage, and would have no
// real benefits.
//
// Invariant: when a SARB is in the mailbox its reference count is at
// least 1, accounting for the reference from the mailbox.
//
// The lock guards the mailbox variable and prevents a race where two
// workers try to set the mailbox at the same time to replace a SARB
// that is only referenced from the mailbox: the workers will both
// decrement the reference count on the old SARB, and one of those
// decrements will be on a garbage object.  We could implement this
// with atomics and a CAS loop but it's not worth the bother.

static PRLock* sharedArrayBufferMailboxLock;
static SharedArrayRawBuffer* sharedArrayBufferMailbox;

static bool
InitSharedArrayBufferMailbox()
{
    sharedArrayBufferMailboxLock = PR_NewLock();
    return sharedArrayBufferMailboxLock != nullptr;
}

static void
DestructSharedArrayBufferMailbox()
{
    // All workers need to have terminated at this point.
    if (sharedArrayBufferMailbox)
        sharedArrayBufferMailbox->dropReference();
    PR_DestroyLock(sharedArrayBufferMailboxLock);
}

static bool
GetSharedArrayBuffer(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    JSObject* newObj = nullptr;
    bool rval = true;

    PR_Lock(sharedArrayBufferMailboxLock);
    SharedArrayRawBuffer* buf = sharedArrayBufferMailbox;
    if (buf) {
        buf->addReference();
        newObj = SharedArrayBufferObject::New(cx, buf);
        if (!newObj) {
            buf->dropReference();
            rval = false;
        }
    }
    PR_Unlock(sharedArrayBufferMailboxLock);

    args.rval().setObjectOrNull(newObj);
    return rval;
}

static bool
SetSharedArrayBuffer(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    SharedArrayRawBuffer* newBuffer = nullptr;

    if (argc == 0 || args.get(0).isNullOrUndefined()) {
        // Clear out the mailbox
    }
    else if (args.get(0).isObject() && args[0].toObject().is<SharedArrayBufferObject>()) {
        newBuffer = args[0].toObject().as<SharedArrayBufferObject>().rawBufferObject();
        newBuffer->addReference();
    } else {
        JS_ReportError(cx, "Only a SharedArrayBuffer can be installed in the global mailbox");
        return false;
    }

    PR_Lock(sharedArrayBufferMailboxLock);
    SharedArrayRawBuffer* oldBuffer = sharedArrayBufferMailbox;
    if (oldBuffer)
        oldBuffer->dropReference();
    sharedArrayBufferMailbox = newBuffer;
    PR_Unlock(sharedArrayBufferMailboxLock);

    args.rval().setUndefined();
    return true;
}

class SprintOptimizationTypeInfoOp : public JS::ForEachTrackedOptimizationTypeInfoOp
{
    Sprinter* sp;
    bool startedTypes_;

  public:
    explicit SprintOptimizationTypeInfoOp(Sprinter* sp)
      : sp(sp),
        startedTypes_(false)
    { }

    void readType(const char* keyedBy, const char* name,
                  const char* location, Maybe<unsigned> lineno) override
    {
        if (!startedTypes_) {
            startedTypes_ = true;
            Sprint(sp, "{\"typeset\": [");
        }
        Sprint(sp, "{\"keyedBy\":\"%s\"", keyedBy);
        if (name)
            Sprint(sp, ",\"name\":\"%s\"", name);
        if (location) {
            char buf[512];
            PutEscapedString(buf, mozilla::ArrayLength(buf), location, strlen(location), '"');
            Sprint(sp, ",\"location\":%s", buf);
        }
        if (lineno.isSome())
            Sprint(sp, ",\"line\":%u", *lineno);
        Sprint(sp, "},");
    }

    void operator()(JS::TrackedTypeSite site, const char* mirType) override {
        if (startedTypes_) {
            // Clear trailing ,
            if ((*sp)[sp->getOffset() - 1] == ',')
                (*sp)[sp->getOffset() - 1] = ' ';
            Sprint(sp, "],");
            startedTypes_ = false;
        } else {
            Sprint(sp, "{");
        }

        Sprint(sp, "\"site\":\"%s\",\"mirType\":\"%s\"},",
               TrackedTypeSiteString(site), mirType);
    }
};

class SprintOptimizationAttemptsOp : public JS::ForEachTrackedOptimizationAttemptOp
{
    Sprinter* sp;

  public:
    explicit SprintOptimizationAttemptsOp(Sprinter* sp)
      : sp(sp)
    { }

    void operator()(JS::TrackedStrategy strategy, JS::TrackedOutcome outcome) override {
        Sprint(sp, "{\"strategy\":\"%s\",\"outcome\":\"%s\"},",
               TrackedStrategyString(strategy), TrackedOutcomeString(outcome));
    }
};

static bool
ReflectTrackedOptimizations(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    RootedObject callee(cx, &args.callee());
    JSRuntime* rt = cx->runtime();

    if (!rt->hasJitRuntime() || !rt->jitRuntime()->isOptimizationTrackingEnabled(rt)) {
        JS_ReportError(cx, "Optimization tracking is off.");
        return false;
    }

    if (args.length() != 1) {
        ReportUsageError(cx, callee, "Wrong number of arguments");
        return false;
    }

    if (!args[0].isObject() || !args[0].toObject().is<JSFunction>()) {
        ReportUsageError(cx, callee, "Argument must be a function");
        return false;
    }

    RootedFunction fun(cx, &args[0].toObject().as<JSFunction>());
    if (!fun->hasScript() || !fun->nonLazyScript()->hasIonScript()) {
        args.rval().setNull();
        return true;
    }

    // Suppress GC for the unrooted JitcodeGlobalEntry below.
    gc::AutoSuppressGC suppress(cx);

    jit::JitcodeGlobalTable* table = rt->jitRuntime()->getJitcodeGlobalTable();
    jit::JitcodeGlobalEntry entry;
    jit::IonScript* ion = fun->nonLazyScript()->ionScript();
    table->lookupInfallible(ion->method()->raw(), &entry, rt);

    if (!entry.hasTrackedOptimizations()) {
        JSObject* obj = JS_NewPlainObject(cx);
        if (!obj)
            return false;
        args.rval().setObject(*obj);
        return true;
    }

    Sprinter sp(cx);
    if (!sp.init())
        return false;

    const jit::IonTrackedOptimizationsRegionTable* regions =
        entry.ionEntry().trackedOptimizationsRegionTable();

    Sprint(&sp, "{\"regions\": [");
    for (uint32_t i = 0; i < regions->numEntries(); i++) {
        jit::IonTrackedOptimizationsRegion region = regions->entry(i);
        jit::IonTrackedOptimizationsRegion::RangeIterator iter = region.ranges();
        while (iter.more()) {
            uint32_t startOffset, endOffset;
            uint8_t index;
            iter.readNext(&startOffset, &endOffset, &index);
            JSScript* script;
            jsbytecode* pc;
            // Use endOffset, as startOffset may be associated with a
            // previous, adjacent region ending exactly at startOffset. That
            // is, suppose we have two regions [0, startOffset], [startOffset,
            // endOffset]. Since we are not querying a return address, we want
            // the second region and not the first.
            uint8_t* addr = ion->method()->raw() + endOffset;
            entry.youngestFrameLocationAtAddr(rt, addr, &script, &pc);
            Sprint(&sp, "{\"location\":\"%s:%u\",\"offset\":%u,\"index\":%u}%s",
                   script->filename(), script->lineno(), script->pcToOffset(pc), index,
                   iter.more() ? "," : "");
        }
    }
    Sprint(&sp, "],");

    Sprint(&sp, "\"opts\": [");
    for (uint8_t i = 0; i < entry.ionEntry().numOptimizationAttempts(); i++) {
        Sprint(&sp, "%s{\"typeinfo\":[", i == 0 ? "" : ",");
        SprintOptimizationTypeInfoOp top(&sp);
        jit::IonTrackedOptimizationsTypeInfo::ForEachOpAdapter adapter(top);
        entry.trackedOptimizationTypeInfo(i).forEach(adapter, entry.allTrackedTypes());
        // Clear the trailing ,
        if (sp[sp.getOffset() - 1] == ',')
            sp[sp.getOffset() - 1] = ' ';
        Sprint(&sp, "],\"attempts\":[");
        SprintOptimizationAttemptsOp aop(&sp);
        entry.trackedOptimizationAttempts(i).forEach(aop);
        // Clear the trailing ,
        if (sp[sp.getOffset() - 1] == ',')
            sp[sp.getOffset() - 1] = ' ';
        Sprint(&sp, "]}");
    }
    Sprint(&sp, "]}");

    if (sp.hadOutOfMemory())
        return false;

    RootedString str(cx, JS_NewStringCopyZ(cx, sp.string()));
    if (!str)
        return false;
    RootedValue jsonVal(cx);
    if (!JS_ParseJSON(cx, str, &jsonVal))
        return false;

    args.rval().set(jsonVal);
    return true;
}

#ifdef DEBUG
static bool
DumpStaticScopeChain(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    RootedObject callee(cx, &args.callee());

    if (args.length() != 1) {
        ReportUsageError(cx, callee, "Wrong number of arguments");
        return false;
    }

    if (!args[0].isObject() ||
        !(args[0].toObject().is<JSFunction>() || args[0].toObject().is<ModuleObject>()))
    {
        ReportUsageError(cx, callee, "Argument must be an interpreted function or a module");
        return false;
    }

    RootedObject obj(cx, &args[0].toObject());
    RootedScript script(cx);

    if (obj->is<JSFunction>()) {
        RootedFunction fun(cx, &obj->as<JSFunction>());
        if (!fun->isInterpreted()) {
            ReportUsageError(cx, callee, "Argument must be an interpreted function");
            return false;
        }
        script = fun->getOrCreateScript(cx);
    } else {
        script = obj->as<ModuleObject>().script();
    }

    js::DumpStaticScopeChain(script);

    args.rval().setUndefined();
    return true;
}
#endif

namespace js {
namespace shell {

class ShellAutoEntryMonitor : JS::dbg::AutoEntryMonitor {
    Vector<UniqueChars, 1, js::SystemAllocPolicy> log;
    bool oom;
    bool enteredWithoutExit;

  public:
    explicit ShellAutoEntryMonitor(JSContext *cx)
      : AutoEntryMonitor(cx),
        oom(false),
        enteredWithoutExit(false)
    { }

    ~ShellAutoEntryMonitor() {
        MOZ_ASSERT(!enteredWithoutExit);
    }

    void Entry(JSContext* cx, JSFunction* function) override {
        MOZ_ASSERT(!enteredWithoutExit);
        enteredWithoutExit = true;

        RootedString displayId(cx, JS_GetFunctionDisplayId(function));
        if (displayId) {
            UniqueChars displayIdStr(JS_EncodeStringToUTF8(cx, displayId));
            oom = !displayIdStr || !log.append(mozilla::Move(displayIdStr));
            return;
        }

        oom = !log.append(make_string_copy("anonymous"));
    }

    void Entry(JSContext* cx, JSScript* script) override {
        MOZ_ASSERT(!enteredWithoutExit);
        enteredWithoutExit = true;

        UniqueChars label(JS_smprintf("eval:%s", JS_GetScriptFilename(script)));
        oom = !label || !log.append(mozilla::Move(label));
    }

    void Exit(JSContext* cx) override {
        MOZ_ASSERT(enteredWithoutExit);
        enteredWithoutExit = false;
    }

    bool buildResult(JSContext *cx, MutableHandleValue resultValue) {
        if (oom) {
            JS_ReportOutOfMemory(cx);
            return false;
        }

        RootedObject result(cx, JS_NewArrayObject(cx, log.length()));
        if (!result)
            return false;

        for (size_t i = 0; i < log.length(); i++) {
            char *name = log[i].get();
            RootedString string(cx, Atomize(cx, name, strlen(name)));
            if (!string)
                return false;
            RootedValue value(cx, StringValue(string));
            if (!JS_SetElement(cx, result, i, value))
                return false;
        }

        resultValue.setObject(*result.get());
        return true;
    }
};

} // namespace shell
} // namespace js

static bool
EntryPoints(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (args.length() != 1) {
        JS_ReportError(cx, "Wrong number of arguments");
        return false;
    }

    RootedObject opts(cx, ToObject(cx, args[0]));
    if (!opts)
        return false;

    // { function: f } --- Call f.
    {
        RootedValue fun(cx), dummy(cx);

        if (!JS_GetProperty(cx, opts, "function", &fun))
            return false;
        if (!fun.isUndefined()) {
            js::shell::ShellAutoEntryMonitor sarep(cx);
            if (!Call(cx, UndefinedHandleValue, fun, JS::HandleValueArray::empty(), &dummy))
                return false;
            return sarep.buildResult(cx, args.rval());
        }
    }

    // { object: o, property: p, value: v } --- Fetch o[p], or if
    // v is present, assign o[p] = v.
    {
        RootedValue objectv(cx), propv(cx), valuev(cx);

        if (!JS_GetProperty(cx, opts, "object", &objectv) ||
            !JS_GetProperty(cx, opts, "property", &propv))
            return false;
        if (!objectv.isUndefined() && !propv.isUndefined()) {
            RootedObject object(cx, ToObject(cx, objectv));
            if (!object)
                return false;

            RootedString string(cx, ToString(cx, propv));
            if (!string)
                return false;
            RootedId id(cx);
            if (!JS_StringToId(cx, string, &id))
                return false;

            if (!JS_GetProperty(cx, opts, "value", &valuev))
                return false;

            js::shell::ShellAutoEntryMonitor sarep(cx);

            if (!valuev.isUndefined()) {
                if (!JS_SetPropertyById(cx, object, id, valuev))
                    return false;
            } else {
                if (!JS_GetPropertyById(cx, object, id, &valuev))
                    return false;
            }

            return sarep.buildResult(cx, args.rval());
        }
    }

    // { ToString: v } --- Apply JS::ToString to v.
    {
        RootedValue v(cx);

        if (!JS_GetProperty(cx, opts, "ToString", &v))
            return false;
        if (!v.isUndefined()) {
            js::shell::ShellAutoEntryMonitor sarep(cx);
            if (!JS::ToString(cx, v))
                return false;
            return sarep.buildResult(cx, args.rval());
        }
    }

    // { ToNumber: v } --- Apply JS::ToNumber to v.
    {
        RootedValue v(cx);
        double dummy;

        if (!JS_GetProperty(cx, opts, "ToNumber", &v))
            return false;
        if (!v.isUndefined()) {
            js::shell::ShellAutoEntryMonitor sarep(cx);
            if (!JS::ToNumber(cx, v, &dummy))
                return false;
            return sarep.buildResult(cx, args.rval());
        }
    }

    // { eval: code } --- Apply ToString and then Evaluate to code.
    {
        RootedValue code(cx), dummy(cx);

        if (!JS_GetProperty(cx, opts, "eval", &code))
            return false;
        if (!code.isUndefined()) {
            RootedString codeString(cx, ToString(cx, code));
            if (!codeString || !codeString->ensureFlat(cx))
                return false;

            AutoStableStringChars stableChars(cx);
            if (!stableChars.initTwoByte(cx, codeString))
                return false;
            const char16_t* chars = stableChars.twoByteRange().start().get();
            size_t length = codeString->length();

            CompileOptions options(cx);
            options.setIntroductionType("entryPoint eval")
                   .setFileAndLine("entryPoint eval", 1);

            js::shell::ShellAutoEntryMonitor sarep(cx);
            if (!JS::Evaluate(cx, options, chars, length, &dummy))
                return false;
            return sarep.buildResult(cx, args.rval());
        }
    }

    JS_ReportError(cx, "bad 'params' object");
    return false;
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
"      isRunOnce: use the isRunOnce compiler option (default: false)\n"
"      noScriptRval: use the no-script-rval compiler option (default: false)\n"
"      fileName: filename for error messages and debug info\n"
"      lineNumber: starting line number for error messages and debug info\n"
"      columnNumber: starting column number for error messages and debug info\n"
"      global: global in which to execute the code\n"
"      newContext: if true, create and use a new cx (default: false)\n"
"      saveFrameChain: if true, save the frame chain before evaluating code\n"
"         and restore it afterwards\n"
"      catchTermination: if true, catch termination (failure without\n"
"         an exception value, as for slow scripts or out-of-memory)\n"
"         and return 'terminated'\n"
"      element: if present with value |v|, convert |v| to an object |o| and\n"
"         mark the source as being attached to the DOM element |o|. If the\n"
"         property is omitted or |v| is null, don't attribute the source to\n"
"         any DOM element.\n"
"      elementAttributeName: if present and not undefined, the name of\n"
"         property of 'element' that holds this code. This is what\n"
"         Debugger.Source.prototype.elementAttributeName returns.\n"
"      sourceMapURL: if present with value |v|, convert |v| to a string, and\n"
"         provide that as the code's source map URL. If omitted, attach no\n"
"         source map URL to the code (although the code may provide one itself,\n"
"         via a //#sourceMappingURL comment).\n"
"      sourceIsLazy: if present and true, indicates that, after compilation, \n"
          "script source should not be cached by the JS engine and should be \n"
          "lazily loaded from the embedding as-needed.\n"
"      loadBytecode: if true, and if the source is a CacheEntryObject,\n"
"         the bytecode would be loaded and decoded from the cache entry instead\n"
"         of being parsed, then it would be executed as usual.\n"
"      saveBytecode: if true, and if the source is a CacheEntryObject,\n"
"         the bytecode would be encoded and saved into the cache entry after\n"
"         the script execution.\n"
"      assertEqBytecode: if true, and if both loadBytecode and saveBytecode are \n"
"         true, then the loaded bytecode and the encoded bytecode are compared.\n"
"         and an assertion is raised if they differ.\n"
),

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

    JS_FN_HELP("startTimingMutator", StartTimingMutator, 0, 0,
"startTimingMutator()",
"  Start accounting time to mutator vs GC."),

    JS_FN_HELP("stopTimingMutator", StopTimingMutator, 0, 0,
"stopTimingMutator()",
"  Stop accounting time to mutator vs GC and dump the results."),

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

    JS_FN_HELP("notes", Notes, 1, 0,
"notes([fun])",
"  Show source notes for functions."),

    JS_FN_HELP("stackDump", StackDump, 3, 0,
"stackDump(showArgs, showLocals, showThisProps)",
"  Tries to print a lot of information about the current stack. \n"
"  Similar to the DumpJSStack() function in the browser."),

#endif
    JS_FN_HELP("intern", Intern, 1, 0,
"intern(str)",
"  Internalize str in the atom table."),

    JS_FN_HELP("getslx", GetSLX, 1, 0,
"getslx(obj)",
"  Get script line extent."),

    JS_FN_HELP("evalcx", EvalInContext, 1, 0,
"evalcx(s[, o])",
"  Evaluate s in optional sandbox object o.\n"
"  if (s == '' && !o) return new o with eager standard classes\n"
"  if (s == 'lazy' && !o) return new o with lazy standard classes"),

    JS_FN_HELP("evalInWorker", EvalInWorker, 1, 0,
"evalInWorker(str)",
"  Evaluate 'str' in a separate thread with its own runtime.\n"),

    JS_FN_HELP("getSharedArrayBuffer", GetSharedArrayBuffer, 0, 0,
"getSharedArrayBuffer()",
"  Retrieve the SharedArrayBuffer object from the cross-worker mailbox.\n"
"  The object retrieved may not be identical to the object that was\n"
"  installed, but it references the same shared memory.\n"
"  getSharedArrayBuffer performs an ordering memory barrier.\n"),

    JS_FN_HELP("setSharedArrayBuffer", SetSharedArrayBuffer, 0, 0,
"setSharedArrayBuffer()",
"  Install the SharedArrayBuffer object in the cross-worker mailbox.\n"
"  setSharedArrayBuffer performs an ordering memory barrier.\n"),

    JS_FN_HELP("shapeOf", ShapeOf, 1, 0,
"shapeOf(obj)",
"  Get the shape of obj (an implementation detail)."),

#ifdef DEBUG
    JS_FN_HELP("arrayInfo", ArrayInfo, 1, 0,
"arrayInfo(a1, a2, ...)",
"  Report statistics about arrays."),
#endif

    JS_FN_HELP("sleep", Sleep_fn, 1, 0,
"sleep(dt)",
"  Sleep for dt seconds."),

    JS_FN_HELP("compile", Compile, 1, 0,
"compile(code)",
"  Compiles a string to bytecode, potentially throwing."),

    JS_FN_HELP("parseModule", ParseModule, 1, 0,
"parseModule(code)",
"  Parses source text as a module and returns a Module object."),

    JS_FN_HELP("parse", Parse, 1, 0,
"parse(code)",
"  Parses a string, potentially throwing."),

    JS_FN_HELP("syntaxParse", SyntaxParse, 1, 0,
"syntaxParse(code)",
"  Check the syntax of a string, returning success value"),

    JS_FN_HELP("offThreadCompileScript", OffThreadCompileScript, 1, 0,
"offThreadCompileScript(code[, options])",
"  Compile |code| on a helper thread. To wait for the compilation to finish\n"
"  and run the code, call |runOffThreadScript|. If present, |options| may\n"
"  have properties saying how the code should be compiled:\n"
"      noScriptRval: use the no-script-rval compiler option (default: false)\n"
"      fileName: filename for error messages and debug info\n"
"      lineNumber: starting line number for error messages and debug info\n"
"      columnNumber: starting column number for error messages and debug info\n"
"      element: if present with value |v|, convert |v| to an object |o| and\n"
"         mark the source as being attached to the DOM element |o|. If the\n"
"         property is omitted or |v| is null, don't attribute the source to\n"
"         any DOM element.\n"
"      elementAttributeName: if present and not undefined, the name of\n"
"         property of 'element' that holds this code. This is what\n"
"         Debugger.Source.prototype.elementAttributeName returns.\n"),

    JS_FN_HELP("runOffThreadScript", runOffThreadScript, 0, 0,
"runOffThreadScript()",
"  Wait for off-thread compilation to complete. If an error occurred,\n"
"  throw the appropriate exception; otherwise, run the script and return\n"
"  its value."),

    JS_FN_HELP("timeout", Timeout, 1, 0,
"timeout([seconds], [func])",
"  Get/Set the limit in seconds for the execution time for the current context.\n"
"  A negative value (default) means that the execution time is unlimited.\n"
"  If a second argument is provided, it will be invoked when the timer elapses.\n"
"  Calling this function will replace any callback set by |setInterruptCallback|.\n"),

    JS_FN_HELP("interruptIf", InterruptIf, 1, 0,
"interruptIf(cond)",
"  Requests interrupt callback if cond is true. If a callback function is set via\n"
"  |timeout| or |setInterruptCallback|, it will be called. No-op otherwise."),

    JS_FN_HELP("invokeInterruptCallback", InvokeInterruptCallbackWrapper, 0, 0,
"invokeInterruptCallback(fun)",
"  Forcefully set the interrupt flag and invoke the interrupt handler. If a\n"
"  callback function is set via |timeout| or |setInterruptCallback|, it will\n"
"  be called. Before returning, fun is called with the return value of the\n"
"  interrupt handler."),

    JS_FN_HELP("setInterruptCallback", SetInterruptCallback, 1, 0,
"setInterruptCallback(func)",
"  Sets func as the interrupt callback function.\n"
"  Calling this function will replace any callback set by |timeout|.\n"),

    JS_FN_HELP("enableLastWarning", EnableLastWarning, 0, 0,
"enableLastWarning()",
"  Enable storing the last warning."),

    JS_FN_HELP("disableLastWarning", DisableLastWarning, 0, 0,
"disableLastWarning()",
"  Disable storing the last warning."),

    JS_FN_HELP("getLastWarning", GetLastWarning, 0, 0,
"getLastWarning()",
"  Returns an object that represents the last warning."),

    JS_FN_HELP("clearLastWarning", ClearLastWarning, 0, 0,
"clearLastWarning()",
"  Clear the last warning."),

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

    JS_FN_HELP("newGlobal", NewGlobal, 1, 0,
"newGlobal([options])",
"  Return a new global object in a new compartment. If options\n"
"  is given, it may have any of the following properties:\n"
"      sameZoneAs: the compartment will be in the same zone as the given object (defaults to a new zone)\n"
"      invisibleToDebugger: the global will be invisible to the debugger (default false)\n"
"      principal: if present, its value converted to a number must be an\n"
"         integer that fits in 32 bits; use that as the new compartment's\n"
"         principal. Shell principals are toys, meant only for testing; one\n"
"         shell principal subsumes another if its set bits are a superset of\n"
"         the other's. Thus, a principal of 0 subsumes nothing, while a\n"
"         principals of ~0 subsumes all other principals. The absence of a\n"
"         principal is treated as if its bits were 0xffff, for subsumption\n"
"         purposes. If this property is omitted, supply no principal."),

    JS_FN_HELP("createMappedArrayBuffer", CreateMappedArrayBuffer, 1, 0,
"createMappedArrayBuffer(filename, [offset, [size]])",
"  Create an array buffer that mmaps the given file."),

    JS_FN_HELP("getMaxArgs", GetMaxArgs, 0, 0,
"getMaxArgs()",
"  Return the maximum number of supported args for a call."),

    JS_FN_HELP("objectEmulatingUndefined", ObjectEmulatingUndefined, 0, 0,
"objectEmulatingUndefined()",
"  Return a new object obj for which typeof obj === \"undefined\", obj == null\n"
"  and obj == undefined (and vice versa for !=), and ToBoolean(obj) === false.\n"),

    JS_FN_HELP("isCachingEnabled", IsCachingEnabled, 0, 0,
"isCachingEnabled()",
"  Return whether JS caching is enabled."),

    JS_FN_HELP("setCachingEnabled", SetCachingEnabled, 1, 0,
"setCachingEnabled(b)",
"  Enable or disable JS caching."),

    JS_FN_HELP("cacheEntry", CacheEntry, 1, 0,
"cacheEntry(code)",
"  Return a new opaque object which emulates a cache entry of a script.  This\n"
"  object encapsulates the code and its cached content. The cache entry is filled\n"
"  and read by the \"evaluate\" function by using it in place of the source, and\n"
"  by setting \"saveBytecode\" and \"loadBytecode\" options."),

    JS_FN_HELP("printProfilerEvents", PrintProfilerEvents, 0, 0,
"printProfilerEvents()",
"  Register a callback with the profiler that prints javascript profiler events\n"
"  to stderr.  Callback is only registered if profiling is enabled."),

    JS_FN_HELP("enableSingleStepProfiling", EnableSingleStepProfiling, 0, 0,
"enableSingleStepProfiling()",
"  This function will fail on platforms that don't support single-step profiling\n"
"  (currently everything but ARM-simulator). When enabled, at every instruction a\n"
"  backtrace will be recorded and stored in an array. Adjacent duplicate backtraces\n"
"  are discarded."),

    JS_FN_HELP("disableSingleStepProfiling", DisableSingleStepProfiling, 0, 0,
"disableSingleStepProfiling()",
"  Return the array of backtraces recorded by enableSingleStepProfiling."),

    JS_FN_HELP("isLatin1", IsLatin1, 1, 0,
"isLatin1(s)",
"  Return true iff the string's characters are stored as Latin1."),

    JS_FN_HELP("stackPointerInfo", StackPointerInfo, 0, 0,
"stackPointerInfo()",
"  Return an int32 value which corresponds to the offset of the latest stack\n"
"  pointer, such that one can take the differences of 2 to estimate a frame-size."),

    JS_FN_HELP("entryPoints", EntryPoints, 1, 0,
"entryPoints(params)",
"Carry out some JSAPI operation as directed by |params|, and return an array of\n"
"objects describing which JavaScript entry points were invoked as a result.\n"
"|params| is an object whose properties indicate what operation to perform. Here\n"
"are the recognized groups of properties:\n"
"\n"
"{ function }: Call the object |params.function| with no arguments.\n"
"\n"
"{ object, property }: Fetch the property named |params.property| of\n"
"|params.object|.\n"
"\n"
"{ ToString }: Apply JS::ToString to |params.toString|.\n"
"\n"
"{ ToNumber }: Apply JS::ToNumber to |params.toNumber|.\n"
"\n"
"{ eval }: Apply JS::Evaluate to |params.eval|.\n"
"\n"
"The return value is an array of strings, with one element for each\n"
"JavaScript invocation that occurred as a result of the given\n"
"operation. Each element is the name of the function invoked, or the\n"
"string 'eval:FILENAME' if the code was invoked by 'eval' or something\n"
"similar.\n"),

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

    JS_FN_HELP("line2pc", LineToPC, 0, 0,
"line2pc([fun,] line)",
"  Map line number to PC."),

    JS_FN_HELP("pc2line", PCToLine, 0, 0,
"pc2line(fun[, pc])",
"  Map PC to line number."),

    JS_FN_HELP("nestedShell", NestedShell, 0, 0,
"nestedShell(shellArgs...)",
"  Execute the given code in a new JS shell process, passing this nested shell\n"
"  the arguments passed to nestedShell. argv[0] of the nested shell will be argv[0]\n"
"  of the current shell (which is assumed to be the actual path to the shell.\n"
"  arguments[0] (of the call to nestedShell) will be argv[1], arguments[1] will\n"
"  be argv[2], etc."),

    JS_FN_HELP("assertFloat32", testingFunc_assertFloat32, 2, 0,
"assertFloat32(value, isFloat32)",
"  In IonMonkey only, asserts that value has (resp. hasn't) the MIRType_Float32 if isFloat32 is true (resp. false)."),

    JS_FN_HELP("assertRecoveredOnBailout", testingFunc_assertRecoveredOnBailout, 2, 0,
"assertRecoveredOnBailout(var)",
"  In IonMonkey only, asserts that variable has RecoveredOnBailout flag."),

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

    JS_FN_HELP("wrapWithProto", WrapWithProto, 2, 0,
"wrapWithProto(obj)",
"  Wrap an object into a noop wrapper with prototype semantics.\n"
"  Note: This is not fuzzing safe because it can be used to construct\n"
"        deeply nested wrapper chains that cannot exist in the wild."),

    JS_FN_HELP("trackedOpts", ReflectTrackedOptimizations, 1, 0,
"trackedOpts(fun)",
"  Returns an object describing the tracked optimizations of |fun|, if\n"
"  any. If |fun| is not a scripted function or has not been compiled by\n"
"  Ion, null is returned."),

#ifdef DEBUG
    JS_FN_HELP("dumpStaticScopeChain", DumpStaticScopeChain, 1, 0,
"dumpStaticScopeChain(obj)",
"  Prints the static scope chain of an interpreted function or a module."),
#endif

    JS_FS_HELP_END
};

static const JSFunctionSpecWithHelp console_functions[] = {
    JS_FN_HELP("log", Print, 0, 0,
"log([exp ...])",
"  Evaluate and print expressions to stdout.\n"
"  This function is an alias of the print() function."),
    JS_FS_HELP_END
};

bool
DefineConsole(JSContext* cx, HandleObject global)
{
    RootedObject obj(cx, JS_NewPlainObject(cx));
    return obj &&
           JS_DefineFunctionsWithHelp(cx, obj, console_functions) &&
           JS_DefineProperty(cx, global, "console", obj, 0);
}

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
PrintHelpString(JSContext* cx, Value v)
{
    JSString* str = v.toString();

    JSLinearString* linear = str->ensureLinear(cx);
    if (!linear)
        return false;

    JS::AutoCheckCannotGC nogc;
    if (linear->hasLatin1Chars()) {
        for (const Latin1Char* p = linear->latin1Chars(nogc); *p; p++)
            fprintf(gOutFile, "%c", char(*p));
    } else {
        for (const char16_t* p = linear->twoByteChars(nogc); *p; p++)
            fprintf(gOutFile, "%c", char(*p));
    }
    fprintf(gOutFile, "\n");

    return true;
}

static bool
PrintHelp(JSContext* cx, HandleObject obj)
{
    RootedValue usage(cx);
    if (!JS_GetProperty(cx, obj, "usage", &usage))
        return false;
    RootedValue help(cx);
    if (!JS_GetProperty(cx, obj, "help", &help))
        return false;

    if (usage.isUndefined() || help.isUndefined())
        return true;

    return PrintHelpString(cx, usage) && PrintHelpString(cx, help);
}

static bool
Help(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    fprintf(gOutFile, "%s\n", JS_GetImplementationVersion());

    RootedObject obj(cx);
    if (args.length() == 0) {
        RootedObject global(cx, JS::CurrentGlobalOrNull(cx));
        Rooted<IdVector> ida(cx, IdVector(cx));
        if (!JS_Enumerate(cx, global, &ida))
            return false;

        for (size_t i = 0; i < ida.length(); i++) {
            RootedValue v(cx);
            RootedId id(cx, ida[i]);
            if (!JS_GetPropertyById(cx, global, id, &v))
                return false;
            if (v.isPrimitive()) {
                JS_ReportError(cx, "primitive arg");
                return false;
            }
            obj = v.toObjectOrNull();
            if (!PrintHelp(cx, obj))
                return false;
        }
    } else {
        for (unsigned i = 0; i < args.length(); i++) {
            if (args[i].isPrimitive()) {
                JS_ReportError(cx, "primitive arg");
                return false;
            }
            obj = args[i].toObjectOrNull();
            if (!PrintHelp(cx, obj))
                return false;
        }
    }

    args.rval().setUndefined();
    return true;
}

static const JSErrorFormatString jsShell_ErrorFormatString[JSShellErr_Limit] = {
#define MSG_DEF(name, count, exception, format) \
    { format, count, JSEXN_ERR } ,
#include "jsshell.msg"
#undef MSG_DEF
};

const JSErrorFormatString*
js::shell::my_GetErrorMessage(void* userRef, const unsigned errorNumber)
{
    if (errorNumber == 0 || errorNumber >= JSShellErr_Limit)
        return nullptr;

    return &jsShell_ErrorFormatString[errorNumber];
}

static bool
CreateLastWarningObject(JSContext* cx, JSErrorReport* report)
{
    RootedObject warningObj(cx, JS_NewObject(cx, nullptr));
    if (!warningObj)
        return false;

    RootedString nameStr(cx);
    if (report->exnType == JSEXN_NONE)
        nameStr = JS_NewStringCopyZ(cx, "None");
    else
        nameStr = GetErrorTypeName(cx->runtime(), report->exnType);
    if (!nameStr)
        return false;
    RootedValue nameVal(cx, StringValue(nameStr));
    if (!DefineProperty(cx, warningObj, cx->names().name, nameVal))
        return false;

    RootedString messageStr(cx, JS_NewUCStringCopyZ(cx, report->ucmessage));
    if (!messageStr)
        return false;
    RootedValue messageVal(cx, StringValue(messageStr));
    if (!DefineProperty(cx, warningObj, cx->names().message, messageVal))
        return false;

    RootedValue linenoVal(cx, Int32Value(report->lineno));
    if (!DefineProperty(cx, warningObj, cx->names().lineNumber, linenoVal))
        return false;

    RootedValue columnVal(cx, Int32Value(report->column));
    if (!DefineProperty(cx, warningObj, cx->names().columnNumber, columnVal))
        return false;

    gLastWarning.setObject(*warningObj);
    return true;
}

void
js::shell::my_ErrorReporter(JSContext* cx, const char* message, JSErrorReport* report)
{
    if (report && JSREPORT_IS_WARNING(report->flags) && gLastWarningEnabled) {
        JS::AutoSaveExceptionState savedExc(cx);
        if (!CreateLastWarningObject(cx, report)) {
            fputs("Unhandled error happened while creating last warning object.\n", gOutFile);
            fflush(gOutFile);
        }
        savedExc.restore();
    }

    gGotError = PrintError(cx, gErrFile, message, report, reportWarnings);
    if (report->exnType != JSEXN_NONE && !JSREPORT_IS_WARNING(report->flags)) {
        if (report->errorNumber == JSMSG_OUT_OF_MEMORY) {
            gExitCode = EXITCODE_OUT_OF_MEMORY;
        } else {
            gExitCode = EXITCODE_RUNTIME_ERROR;
        }
    }
}

static void
my_OOMCallback(JSContext* cx, void* data)
{
    // If a script is running, the engine is about to throw the string "out of
    // memory", which may or may not be caught. Otherwise the engine will just
    // unwind and return null/false, with no exception set.
    if (!JS_IsRunning(cx))
        gGotError = true;
}

static bool
global_enumerate(JSContext* cx, HandleObject obj)
{
#ifdef LAZY_STANDARD_CLASSES
    return JS_EnumerateStandardClasses(cx, obj);
#else
    return true;
#endif
}

static bool
global_resolve(JSContext* cx, HandleObject obj, HandleId id, bool* resolvedp)
{
#ifdef LAZY_STANDARD_CLASSES
    if (!JS_ResolveStandardClass(cx, obj, id, resolvedp))
        return false;
#endif
    return true;
}

static bool
global_mayResolve(const JSAtomState& names, jsid id, JSObject* maybeObj)
{
    return JS_MayResolveStandardClass(names, id, maybeObj);
}

static const JSClass global_class = {
    "global", JSCLASS_GLOBAL_FLAGS,
    nullptr, nullptr, nullptr, nullptr,
    global_enumerate, global_resolve, global_mayResolve,
    nullptr, nullptr,
    nullptr, nullptr, nullptr,
    JS_GlobalObjectTraceHook
};

/*
 * Define a FakeDOMObject constructor. It returns an object with a getter,
 * setter and method with attached JitInfo. This object can be used to test
 * IonMonkey DOM optimizations in the shell.
 */
static const uint32_t DOM_OBJECT_SLOT = 0;

static bool
dom_genericGetter(JSContext* cx, unsigned argc, JS::Value* vp);

static bool
dom_genericSetter(JSContext* cx, unsigned argc, JS::Value* vp);

static bool
dom_genericMethod(JSContext* cx, unsigned argc, JS::Value* vp);

#ifdef DEBUG
static const JSClass* GetDomClass();
#endif

static bool
dom_get_x(JSContext* cx, HandleObject obj, void* self, JSJitGetterCallArgs args)
{
    MOZ_ASSERT(JS_GetClass(obj) == GetDomClass());
    MOZ_ASSERT(self == (void*)0x1234);
    args.rval().set(JS_NumberValue(double(3.14)));
    return true;
}

static bool
dom_set_x(JSContext* cx, HandleObject obj, void* self, JSJitSetterCallArgs args)
{
    MOZ_ASSERT(JS_GetClass(obj) == GetDomClass());
    MOZ_ASSERT(self == (void*)0x1234);
    return true;
}

static bool
dom_doFoo(JSContext* cx, HandleObject obj, void* self, const JSJitMethodCallArgs& args)
{
    MOZ_ASSERT(JS_GetClass(obj) == GetDomClass());
    MOZ_ASSERT(self == (void*)0x1234);

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
    true,     /* isEliminatable */
    false,    /* isAlwaysInSlot */
    false,    /* isLazilyCachedInSlot */
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
    false,    /* isEliminatable. */
    false,    /* isAlwaysInSlot */
    false,    /* isLazilyCachedInSlot */
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
    false,    /* isEliminatable */
    false,    /* isAlwaysInSlot */
    false,    /* isLazilyCachedInSlot */
    false,    /* isTypedMethod */
    0         /* slotIndex */
};

static const JSPropertySpec dom_props[] = {
    {"x",
     JSPROP_SHARED | JSPROP_ENUMERATE,
     { { dom_genericGetter, &dom_x_getterinfo } },
     { { dom_genericSetter, &dom_x_setterinfo } }
    },
    JS_PS_END
};

static const JSFunctionSpec dom_methods[] = {
    JS_FNINFO("doFoo", dom_genericMethod, &doFoo_methodinfo, 3, JSPROP_ENUMERATE),
    JS_FS_END
};

static const JSClass dom_class = {
    "FakeDOMObject", JSCLASS_IS_DOMJSCLASS | JSCLASS_HAS_RESERVED_SLOTS(2)
};

#ifdef DEBUG
static const JSClass* GetDomClass() {
    return &dom_class;
}
#endif

static bool
dom_genericGetter(JSContext* cx, unsigned argc, JS::Value* vp)
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

    const JSJitInfo* info = FUNCTION_VALUE_TO_JITINFO(args.calleev());
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

    MOZ_ASSERT(args.length() == 1);

    if (JS_GetClass(obj) != &dom_class) {
        args.rval().set(UndefinedValue());
        return true;
    }

    JS::Value val = js::GetReservedSlot(obj, DOM_OBJECT_SLOT);

    const JSJitInfo* info = FUNCTION_VALUE_TO_JITINFO(args.calleev());
    MOZ_ASSERT(info->type() == JSJitInfo::Setter);
    JSJitSetterOp setter = info->setter;
    if (!setter(cx, obj, val.toPrivate(), JSJitSetterCallArgs(args)))
        return false;
    args.rval().set(UndefinedValue());
    return true;
}

static bool
dom_genericMethod(JSContext* cx, unsigned argc, JS::Value* vp)
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

    const JSJitInfo* info = FUNCTION_VALUE_TO_JITINFO(args.calleev());
    MOZ_ASSERT(info->type() == JSJitInfo::Method);
    JSJitMethodOp method = info->method;
    return method(cx, obj, val.toPrivate(), JSJitMethodCallArgs(args));
}

static void
InitDOMObject(HandleObject obj)
{
    /* Fow now just initialize to a constant we can check. */
    SetReservedSlot(obj, DOM_OBJECT_SLOT, PrivateValue((void*)0x1234));
}

static bool
dom_constructor(JSContext* cx, unsigned argc, JS::Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    RootedObject callee(cx, &args.callee());
    RootedValue protov(cx);
    if (!GetProperty(cx, callee, callee, cx->names().prototype, &protov))
        return false;

    if (!protov.isObject()) {
        JS_ReportErrorNumber(cx, GetErrorMessage, nullptr, JSMSG_BAD_PROTOTYPE, "FakeDOMObject");
        return false;
    }

    RootedObject proto(cx, &protov.toObject());
    RootedObject domObj(cx, JS_NewObjectWithGivenProto(cx, &dom_class, proto));
    if (!domObj)
        return false;

    InitDOMObject(domObj);

    args.rval().setObject(*domObj);
    return true;
}

static bool
InstanceClassHasProtoAtDepth(const Class* clasp, uint32_t protoID, uint32_t depth)
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
        MOZ_ASSERT(jsCacheOpened == true);
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
ShellOpenAsmJSCacheEntryForRead(HandleObject global, const char16_t* begin, const char16_t* limit,
                                size_t* serializedSizeOut, const uint8_t** memoryOut,
                                intptr_t* handleOut)
{
    if (!jsCachingEnabled || !jsCacheAsmJSPath)
        return false;

    ScopedFileDesc fd(open(jsCacheAsmJSPath, O_RDWR), ScopedFileDesc::READ_LOCK);
    if (fd == -1)
        return false;

    // Get the size and make sure we can dereference at least one uint32_t.
    off_t off = lseek(fd, 0, SEEK_END);
    if (off == -1 || off < (off_t)sizeof(uint32_t))
        return false;

    // Map the file into memory.
    void* memory;
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
    if (*(uint32_t*)memory != asmJSCacheCookie) {
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
    *memoryOut = (uint8_t*)memory + sizeof(uint32_t);
    *handleOut = fd.forget();
    return true;
}

static void
ShellCloseAsmJSCacheEntryForRead(size_t serializedSize, const uint8_t* memory, intptr_t handle)
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

    MOZ_ASSERT(jsCacheOpened == true);
    jsCacheOpened = false;
    close(handle);
}

static JS::AsmJSCacheResult
ShellOpenAsmJSCacheEntryForWrite(HandleObject global, bool installed,
                                 const char16_t* begin, const char16_t* end,
                                 size_t serializedSize, uint8_t** memoryOut, intptr_t* handleOut)
{
    if (!jsCachingEnabled || !jsCacheAsmJSPath)
        return JS::AsmJSCache_Disabled_ShellFlags;

    // Create the cache directory if it doesn't already exist.
    struct stat dirStat;
    if (stat(jsCacheDir, &dirStat) == 0) {
        if (!(dirStat.st_mode & S_IFDIR))
            return JS::AsmJSCache_InternalError;
    } else {
#ifdef XP_WIN
        if (mkdir(jsCacheDir) != 0)
            return JS::AsmJSCache_InternalError;
#else
        if (mkdir(jsCacheDir, 0777) != 0)
            return JS::AsmJSCache_InternalError;
#endif
    }

    ScopedFileDesc fd(open(jsCacheAsmJSPath, O_CREAT|O_RDWR, 0660), ScopedFileDesc::WRITE_LOCK);
    if (fd == -1)
        return JS::AsmJSCache_InternalError;

    // Include extra space for the asmJSCacheCookie.
    serializedSize += sizeof(uint32_t);

    // Resize the file to the appropriate size after zeroing their contents.
#ifdef XP_WIN
    if (chsize(fd, 0))
        return JS::AsmJSCache_InternalError;
    if (chsize(fd, serializedSize))
        return JS::AsmJSCache_InternalError;
#else
    if (ftruncate(fd, 0))
        return JS::AsmJSCache_InternalError;
    if (ftruncate(fd, serializedSize))
        return JS::AsmJSCache_InternalError;
#endif

    // Map the file into memory.
    void* memory;
#ifdef XP_WIN
    HANDLE fdOsHandle = (HANDLE)_get_osfhandle(fd);
    HANDLE fileMapping = CreateFileMapping(fdOsHandle, nullptr, PAGE_READWRITE, 0, 0, nullptr);
    if (!fileMapping)
        return JS::AsmJSCache_InternalError;

    memory = MapViewOfFile(fileMapping, FILE_MAP_WRITE, 0, 0, 0);
    CloseHandle(fileMapping);
    if (!memory)
        return JS::AsmJSCache_InternalError;
#else
    memory = mmap(nullptr, serializedSize, PROT_WRITE, MAP_SHARED, fd, 0);
    if (memory == MAP_FAILED)
        return JS::AsmJSCache_InternalError;
#endif

    // The embedding added the cookie so strip it off of the buffer returned to
    // the JS engine. The asmJSCacheCookie will be written on close, below.
    MOZ_ASSERT(*(uint32_t*)memory == 0);
    *memoryOut = (uint8_t*)memory + sizeof(uint32_t);
    *handleOut = fd.forget();
    return JS::AsmJSCache_Success;
}

static void
ShellCloseAsmJSCacheEntryForWrite(size_t serializedSize, uint8_t* memory, intptr_t handle)
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

    MOZ_ASSERT(*(uint32_t*)memory == 0);
    *(uint32_t*)memory = asmJSCacheCookie;

    // Free the memory mapping and file.
#ifdef XP_WIN
    UnmapViewOfFile(const_cast<uint8_t*>(memory));
#else
    munmap(memory, serializedSize);
#endif

    MOZ_ASSERT(jsCacheOpened == true);
    jsCacheOpened = false;
    close(handle);
}

static bool
ShellBuildId(JS::BuildIdCharVector* buildId)
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

static const JS::AsmJSCacheOps asmJSCacheOps = {
    ShellOpenAsmJSCacheEntryForRead,
    ShellCloseAsmJSCacheEntryForRead,
    ShellOpenAsmJSCacheEntryForWrite,
    ShellCloseAsmJSCacheEntryForWrite,
    ShellBuildId
};

static JSContext*
NewContext(JSRuntime* rt)
{
    JSContext* cx = JS_NewContext(rt, gStackChunkSize);
    if (!cx)
        return nullptr;

    JSShellContextData* data = NewContextData();
    if (!data) {
        DestroyContext(cx, false);
        return nullptr;
    }

    JS_SetContextPrivate(cx, data);
    return cx;
}

static void
DestroyContext(JSContext* cx, bool withGC)
{
    // Don't use GetContextData as |data| could be a nullptr in the case of
    // destroying a context precisely because we couldn't create its private
    // data.
    JSShellContextData* data = (JSShellContextData*) JS_GetContextPrivate(cx);
    JS_SetContextPrivate(cx, nullptr);
    js_free(data);
    withGC ? JS_DestroyContext(cx) : JS_DestroyContextNoGC(cx);
}

static JSObject*
NewGlobalObject(JSContext* cx, JS::CompartmentOptions& options,
                JSPrincipals* principals)
{
    RootedObject glob(cx, JS_NewGlobalObject(cx, &global_class, principals,
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
        if (!JS_InitReflectParse(cx, glob))
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

        if (!fuzzingSafe) {
            if (!JS_DefineFunctionsWithHelp(cx, glob, fuzzing_unsafe_functions))
                return nullptr;
            if (!DefineConsole(cx, glob))
                return nullptr;
        }

        if (!DefineOS(cx, glob, fuzzingSafe))
            return nullptr;

        RootedObject performanceObj(cx, JS_NewObject(cx, nullptr));
        if (!performanceObj)
            return nullptr;
        RootedObject mozMemoryObj(cx, JS_NewObject(cx, nullptr));
        if (!mozMemoryObj)
            return nullptr;
        RootedObject gcObj(cx, gc::NewMemoryInfoObject(cx));
        if (!gcObj)
            return nullptr;
        if (!JS_DefineProperty(cx, glob, "performance", performanceObj, JSPROP_ENUMERATE))
            return nullptr;
        if (!JS_DefineProperty(cx, performanceObj, "mozMemory", mozMemoryObj, JSPROP_ENUMERATE))
            return nullptr;
        if (!JS_DefineProperty(cx, mozMemoryObj, "gc", gcObj, JSPROP_ENUMERATE))
            return nullptr;

        /* Initialize FakeDOMObject. */
        static const js::DOMCallbacks DOMcallbacks = {
            InstanceClassHasProtoAtDepth
        };
        SetDOMCallbacks(cx->runtime(), &DOMcallbacks);

        RootedObject domProto(cx, JS_InitClass(cx, glob, nullptr, &dom_class, dom_constructor,
                                               0, dom_props, dom_methods, nullptr, nullptr));
        if (!domProto)
            return nullptr;

        /* Initialize FakeDOMObject.prototype */
        InitDOMObject(domProto);
    }

    JS_FireOnNewGlobalObject(cx, glob);

    return glob;
}

static bool
BindScriptArgs(JSContext* cx, OptionParser* op)
{
    MultiStringRange msr = op->getMultiStringArg("scriptArgs");
    RootedObject scriptArgs(cx);
    scriptArgs = JS_NewArrayObject(cx, 0);
    if (!scriptArgs)
        return false;

    if (!JS_DefineProperty(cx, cx->global(), "scriptArgs", scriptArgs, 0))
        return false;

    for (size_t i = 0; !msr.empty(); msr.popFront(), ++i) {
        const char* scriptArg = msr.front();
        JS::RootedString str(cx, JS_NewStringCopyZ(cx, scriptArg));
        if (!str ||
            !JS_DefineElement(cx, scriptArgs, i, str, JSPROP_ENUMERATE))
        {
            return false;
        }
    }

    const char* scriptPath = op->getStringArg("script");
    RootedValue scriptPathValue(cx);
    if (scriptPath) {
        RootedString scriptPathString(cx, JS_NewStringCopyZ(cx, scriptPath));
        if (!scriptPathString)
            return false;
        scriptPathValue = StringValue(scriptPathString);
    } else {
        scriptPathValue = UndefinedValue();
    }

    if (!JS_DefineProperty(cx, cx->global(), "scriptPath", scriptPathValue, 0))
        return false;

    return true;
}

static bool
OptionFailure(const char* option, const char* str)
{
    fprintf(stderr, "Unrecognized option for %s: %s\n", option, str);
    return false;
}

static int
ProcessArgs(JSContext* cx, OptionParser* op)
{
    if (op->getBoolOption('s'))
        JS::RuntimeOptionsRef(cx).toggleExtraWarnings();

    /* |scriptArgs| gets bound on the global before any code is run. */
    if (!BindScriptArgs(cx, op))
        return EXIT_FAILURE;

    MultiStringRange filePaths = op->getMultiStringOption('f');
    MultiStringRange codeChunks = op->getMultiStringOption('e');

    if (filePaths.empty() && codeChunks.empty() && !op->getStringArg("script")) {
        Process(cx, nullptr, true); /* Interactive. */
        return gExitCode;
    }

    while (!filePaths.empty() || !codeChunks.empty()) {
        size_t fpArgno = filePaths.empty() ? -1 : filePaths.argno();
        size_t ccArgno = codeChunks.empty() ? -1 : codeChunks.argno();
        if (fpArgno < ccArgno) {
            char* path = filePaths.front();
            Process(cx, path, false);
            if (gExitCode)
                return gExitCode;
            filePaths.popFront();
        } else {
            const char* code = codeChunks.front();
            RootedValue rval(cx);
            JS::CompileOptions opts(cx);
            opts.setFileAndLine("-e", 1);
            if (!JS::Evaluate(cx, opts, code, strlen(code), &rval))
                return gExitCode ? gExitCode : EXITCODE_RUNTIME_ERROR;
            codeChunks.popFront();
            if (gQuitting)
                break;
        }
    }

    if (gQuitting)
        return gExitCode ? gExitCode : EXIT_SUCCESS;

    /* The |script| argument is processed after all options. */
    if (const char* path = op->getStringArg("script")) {
        Process(cx, path, false);
        if (gExitCode)
            return gExitCode;
    }

    if (op->getBoolOption('i'))
        Process(cx, nullptr, true);

    return gExitCode ? gExitCode : EXIT_SUCCESS;
}

static bool
SetRuntimeOptions(JSRuntime* rt, const OptionParser& op)
{
    enableBaseline = !op.getBoolOption("no-baseline");
    enableIon = !op.getBoolOption("no-ion");
    enableAsmJS = !op.getBoolOption("no-asmjs");
    enableNativeRegExp = !op.getBoolOption("no-native-regexp");
    enableUnboxedArrays = op.getBoolOption("unboxed-arrays");

    JS::RuntimeOptionsRef(rt).setBaseline(enableBaseline)
                             .setIon(enableIon)
                             .setAsmJS(enableAsmJS)
                             .setNativeRegExp(enableNativeRegExp)
                             .setUnboxedArrays(enableUnboxedArrays);

    if (op.getBoolOption("no-unboxed-objects"))
        jit::js_JitOptions.disableUnboxedObjects = true;

    if (const char* str = op.getStringOption("ion-scalar-replacement")) {
        if (strcmp(str, "on") == 0)
            jit::js_JitOptions.disableScalarReplacement = false;
        else if (strcmp(str, "off") == 0)
            jit::js_JitOptions.disableScalarReplacement = true;
        else
            return OptionFailure("ion-scalar-replacement", str);
    }

    if (const char* str = op.getStringOption("ion-shared-stubs")) {
        if (strcmp(str, "on") == 0)
            jit::js_JitOptions.disableSharedStubs = false;
        else if (strcmp(str, "off") == 0)
            jit::js_JitOptions.disableSharedStubs = true;
        else
            return OptionFailure("ion-shared-stubs", str);
    }

    if (const char* str = op.getStringOption("ion-gvn")) {
        if (strcmp(str, "off") == 0) {
            jit::js_JitOptions.disableGvn = true;
        } else if (strcmp(str, "on") != 0 &&
                   strcmp(str, "optimistic") != 0 &&
                   strcmp(str, "pessimistic") != 0)
        {
            // We accept "pessimistic" and "optimistic" as synonyms for "on"
            // for backwards compatibility.
            return OptionFailure("ion-gvn", str);
        }
    }

    if (const char* str = op.getStringOption("ion-licm")) {
        if (strcmp(str, "on") == 0)
            jit::js_JitOptions.disableLicm = false;
        else if (strcmp(str, "off") == 0)
            jit::js_JitOptions.disableLicm = true;
        else
            return OptionFailure("ion-licm", str);
    }

    if (const char* str = op.getStringOption("ion-edgecase-analysis")) {
        if (strcmp(str, "on") == 0)
            jit::js_JitOptions.disableEdgeCaseAnalysis = false;
        else if (strcmp(str, "off") == 0)
            jit::js_JitOptions.disableEdgeCaseAnalysis = true;
        else
            return OptionFailure("ion-edgecase-analysis", str);
    }

    if (const char* str = op.getStringOption("ion-range-analysis")) {
        if (strcmp(str, "on") == 0)
            jit::js_JitOptions.disableRangeAnalysis = false;
        else if (strcmp(str, "off") == 0)
            jit::js_JitOptions.disableRangeAnalysis = true;
        else
            return OptionFailure("ion-range-analysis", str);
    }

    if (const char* str = op.getStringOption("ion-sink")) {
        if (strcmp(str, "on") == 0)
            jit::js_JitOptions.disableSink = false;
        else if (strcmp(str, "off") == 0)
            jit::js_JitOptions.disableSink = true;
        else
            return OptionFailure("ion-sink", str);
    }

    if (const char* str = op.getStringOption("ion-loop-unrolling")) {
        if (strcmp(str, "on") == 0)
            jit::js_JitOptions.disableLoopUnrolling = false;
        else if (strcmp(str, "off") == 0)
            jit::js_JitOptions.disableLoopUnrolling = true;
        else
            return OptionFailure("ion-loop-unrolling", str);
    }

    if (const char* str = op.getStringOption("ion-instruction-reordering")) {
        if (strcmp(str, "on") == 0)
            jit::js_JitOptions.disableInstructionReordering = false;
        else if (strcmp(str, "off") == 0)
            jit::js_JitOptions.disableInstructionReordering = true;
        else
            return OptionFailure("ion-instruction-reordering", str);
    }

    if (op.getBoolOption("ion-check-range-analysis"))
        jit::js_JitOptions.checkRangeAnalysis = true;

    if (op.getBoolOption("ion-extra-checks"))
        jit::js_JitOptions.runExtraChecks = true;

    if (const char* str = op.getStringOption("ion-inlining")) {
        if (strcmp(str, "on") == 0)
            jit::js_JitOptions.disableInlining = false;
        else if (strcmp(str, "off") == 0)
            jit::js_JitOptions.disableInlining = true;
        else
            return OptionFailure("ion-inlining", str);
    }

    if (const char* str = op.getStringOption("ion-osr")) {
        if (strcmp(str, "on") == 0)
            jit::js_JitOptions.osr = true;
        else if (strcmp(str, "off") == 0)
            jit::js_JitOptions.osr = false;
        else
            return OptionFailure("ion-osr", str);
    }

    if (const char* str = op.getStringOption("ion-limit-script-size")) {
        if (strcmp(str, "on") == 0)
            jit::js_JitOptions.limitScriptSize = true;
        else if (strcmp(str, "off") == 0)
            jit::js_JitOptions.limitScriptSize = false;
        else
            return OptionFailure("ion-limit-script-size", str);
    }

    int32_t warmUpThreshold = op.getIntOption("ion-warmup-threshold");
    if (warmUpThreshold >= 0)
        jit::js_JitOptions.setCompilerWarmUpThreshold(warmUpThreshold);

    warmUpThreshold = op.getIntOption("baseline-warmup-threshold");
    if (warmUpThreshold >= 0)
        jit::js_JitOptions.baselineWarmUpThreshold = warmUpThreshold;

    if (op.getBoolOption("baseline-eager"))
        jit::js_JitOptions.baselineWarmUpThreshold = 0;

    if (const char* str = op.getStringOption("ion-regalloc")) {
        jit::js_JitOptions.forcedRegisterAllocator = jit::LookupRegisterAllocator(str);
        if (!jit::js_JitOptions.forcedRegisterAllocator.isSome())
            return OptionFailure("ion-regalloc", str);
    }

    if (op.getBoolOption("ion-eager"))
        jit::js_JitOptions.setEagerCompilation();

    offthreadCompilation = true;
    if (const char* str = op.getStringOption("ion-offthread-compile")) {
        if (strcmp(str, "off") == 0)
            offthreadCompilation = false;
        else if (strcmp(str, "on") != 0)
            return OptionFailure("ion-offthread-compile", str);
    }
    rt->setOffthreadIonCompilationEnabled(offthreadCompilation);

    if (op.getStringOption("ion-parallel-compile")) {
        fprintf(stderr, "--ion-parallel-compile is deprecated. Please use --ion-offthread-compile instead.\n");
        return false;
    }

#if defined(JS_CODEGEN_ARM)
    if (const char* str = op.getStringOption("arm-hwcap"))
        jit::ParseARMHwCapFlags(str);

    int32_t fill = op.getIntOption("arm-asm-nop-fill");
    if (fill >= 0)
        jit::Assembler::NopFill = fill;

    int32_t poolMaxOffset = op.getIntOption("asm-pool-max-offset");
    if (poolMaxOffset >= 5 && poolMaxOffset <= 1024)
        jit::Assembler::AsmPoolMaxOffset = poolMaxOffset;
#endif

#if defined(JS_SIMULATOR_ARM)
    if (op.getBoolOption("arm-sim-icache-checks"))
        jit::Simulator::ICacheCheckingEnabled = true;

    int32_t stopAt = op.getIntOption("arm-sim-stop-at");
    if (stopAt >= 0)
        jit::Simulator::StopSimAt = stopAt;
#elif defined(JS_SIMULATOR_MIPS32)
    if (op.getBoolOption("mips-sim-icache-checks"))
        jit::Simulator::ICacheCheckingEnabled = true;

    int32_t stopAt = op.getIntOption("mips-sim-stop-at");
    if (stopAt >= 0)
        jit::Simulator::StopSimAt = stopAt;
#endif

    reportWarnings = op.getBoolOption('w');
    compileOnly = op.getBoolOption('c');
    printTiming = op.getBoolOption('b');
    rt->profilingScripts = enableDisassemblyDumps = op.getBoolOption('D');

    jsCacheDir = op.getStringOption("js-cache");
    if (jsCacheDir) {
        if (!op.getBoolOption("no-js-cache-per-process"))
            jsCacheDir = JS_smprintf("%s/%u", jsCacheDir, (unsigned)getpid());
        else
            jsCacheDir = JS_strdup(rt, jsCacheDir);
        jsCacheAsmJSPath = JS_smprintf("%s/asmjs.cache", jsCacheDir);
    }

#ifdef DEBUG
    dumpEntrainedVariables = op.getBoolOption("dump-entrained-variables");
#endif

#ifdef JS_GC_ZEAL
    const char* zealStr = op.getStringOption("gc-zeal");
    gZealStr[0] = 0;
    if (zealStr) {
        if (!rt->gc.parseAndSetZeal(zealStr))
            return false;
        strncpy(gZealStr, zealStr, sizeof(gZealStr));
        gZealStr[sizeof(gZealStr)-1] = 0;
    }
#endif

    return true;
}

static void
SetWorkerRuntimeOptions(JSRuntime* rt)
{
    // Copy option values from the main thread.
    JS::RuntimeOptionsRef(rt).setBaseline(enableBaseline)
                             .setIon(enableIon)
                             .setAsmJS(enableAsmJS)
                             .setNativeRegExp(enableNativeRegExp)
                             .setUnboxedArrays(enableUnboxedArrays);
    rt->setOffthreadIonCompilationEnabled(offthreadCompilation);
    rt->profilingScripts = enableDisassemblyDumps;

#ifdef JS_GC_ZEAL
    if (*gZealStr)
        rt->gc.parseAndSetZeal(gZealStr);
#endif
}

static int
Shell(JSContext* cx, OptionParser* op, char** envp)
{
    Maybe<JS::AutoDisableGenerationalGC> noggc;
    if (op->getBoolOption("no-ggc"))
        noggc.emplace(cx->runtime());

    Maybe<AutoDisableCompactingGC> nocgc;
    if (op->getBoolOption("no-cgc"))
        nocgc.emplace(cx->runtime());

    JSAutoRequest ar(cx);

    if (op->getBoolOption("fuzzing-safe"))
        fuzzingSafe = true;
    else
        fuzzingSafe = (getenv("MOZ_FUZZING_SAFE") && getenv("MOZ_FUZZING_SAFE")[0] != '0');

    RootedObject glob(cx);
    JS::CompartmentOptions options;
    options.setVersion(JSVERSION_LATEST);
    glob = NewGlobalObject(cx, options, nullptr);
    if (!glob)
        return 1;

    JSAutoCompartment ac(cx, glob);

    int result = ProcessArgs(cx, op);

    if (enableDisassemblyDumps)
        js::DumpCompartmentPCCounts(cx);

    if (!op->getBoolOption("no-js-cache-per-process")) {
        if (jsCacheAsmJSPath) {
            unlink(jsCacheAsmJSPath);
            JS_free(cx, const_cast<char*>(jsCacheAsmJSPath));
        }
        if (jsCacheDir) {
            rmdir(jsCacheDir);
            JS_free(cx, const_cast<char*>(jsCacheDir));
        }
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

/* Pretend we can always preserve wrappers for dummy DOM objects. */
static bool
DummyPreserveWrapperCallback(JSContext* cx, JSObject* obj)
{
    return true;
}

static void
PreInit()
{
#ifdef XP_WIN
    // Disable the segfault dialog. We want to fail the tests immediately
    // instead of hanging automation.
    UINT prevMode = SetErrorMode(0);
    UINT newMode = SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX;
    SetErrorMode(prevMode | newMode);
#endif
}

int
main(int argc, char** argv, char** envp)
{
    PreInit();

    sArgc = argc;
    sArgv = argv;

    JSRuntime* rt;
    JSContext* cx;
    int result;
#ifdef XP_WIN
    {
        const char* crash_option = getenv("XRE_NO_WINDOWS_CRASH_DIALOG");
        if (crash_option && strncmp(crash_option, "1", 1)) {
            DWORD oldmode = SetErrorMode(SEM_NOGPFAULTERRORBOX);
            SetErrorMode(oldmode | SEM_NOGPFAULTERRORBOX);
        }
    }
#endif

#ifdef HAVE_SETLOCALE
    setlocale(LC_ALL, "");
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
        || !op.addBoolOption('c', "compileonly", "Only compile, don't run (syntax checking mode)")
        || !op.addBoolOption('w', "warnings", "Emit warnings")
        || !op.addBoolOption('W', "nowarnings", "Don't emit warnings")
        || !op.addBoolOption('s', "strict", "Check strictness")
        || !op.addBoolOption('D', "dump-bytecode", "Dump bytecode with exec count for all scripts")
        || !op.addBoolOption('b', "print-timing", "Print sub-ms runtime for each file that's run")
        || !op.addStringOption('\0', "js-cache", "[path]",
                               "Enable the JS cache by specifying the path of the directory to use "
                               "to hold cache files")
        || !op.addBoolOption('\0', "no-js-cache-per-process",
                               "Deactivates cache per process. Otherwise, generate a separate cache"
                               "sub-directory for this process inside the cache directory"
                               "specified by --js-cache. This cache directory will be removed"
                               "when the js shell exits. This is useful for running tests in"
                               "parallel.")
#ifdef DEBUG
        || !op.addBoolOption('O', "print-alloc", "Print the number of allocations at exit")
#endif
        || !op.addOptionalStringArg("script", "A script to execute (after all options)")
        || !op.addOptionalMultiStringArg("scriptArgs",
                                         "String arguments to bind as |scriptArgs| in the "
                                         "shell's global")
        || !op.addIntOption('\0', "thread-count", "COUNT", "Use COUNT auxiliary threads "
                            "(default: # of cores - 1)", -1)
        || !op.addBoolOption('\0', "ion", "Enable IonMonkey (default)")
        || !op.addBoolOption('\0', "no-ion", "Disable IonMonkey")
        || !op.addBoolOption('\0', "no-asmjs", "Disable asm.js compilation")
        || !op.addBoolOption('\0', "no-native-regexp", "Disable native regexp compilation")
        || !op.addBoolOption('\0', "no-unboxed-objects", "Disable creating unboxed plain objects")
        || !op.addBoolOption('\0', "unboxed-arrays", "Allow creating unboxed arrays")
        || !op.addStringOption('\0', "ion-shared-stubs", "on/off",
                               "Use shared stubs (default: off, on to enable)")
        || !op.addStringOption('\0', "ion-scalar-replacement", "on/off",
                               "Scalar Replacement (default: on, off to disable)")
        || !op.addStringOption('\0', "ion-gvn", "[mode]",
                               "Specify Ion global value numbering:\n"
                               "  off: disable GVN\n"
                               "  on:  enable GVN (default)\n")
        || !op.addStringOption('\0', "ion-licm", "on/off",
                               "Loop invariant code motion (default: on, off to disable)")
        || !op.addStringOption('\0', "ion-edgecase-analysis", "on/off",
                               "Find edge cases where Ion can avoid bailouts (default: on, off to disable)")
        || !op.addStringOption('\0', "ion-range-analysis", "on/off",
                               "Range analysis (default: on, off to disable)")
        || !op.addStringOption('\0', "ion-sink", "on/off",
                               "Sink code motion (default: off, on to enable)")
        || !op.addStringOption('\0', "ion-loop-unrolling", "on/off",
                               "Loop unrolling (default: off, on to enable)")
        || !op.addStringOption('\0', "ion-instruction-reordering", "on/off",
                               "Instruction reordering (default: off, on to enable)")
        || !op.addBoolOption('\0', "ion-check-range-analysis",
                               "Range analysis checking")
        || !op.addBoolOption('\0', "ion-extra-checks",
                               "Perform extra dynamic validation checks")
        || !op.addStringOption('\0', "ion-inlining", "on/off",
                               "Inline methods where possible (default: on, off to disable)")
        || !op.addStringOption('\0', "ion-osr", "on/off",
                               "On-Stack Replacement (default: on, off to disable)")
        || !op.addStringOption('\0', "ion-limit-script-size", "on/off",
                               "Don't compile very large scripts (default: on, off to disable)")
        || !op.addIntOption('\0', "ion-warmup-threshold", "COUNT",
                            "Wait for COUNT calls or iterations before compiling "
                            "(default: 1000)", -1)
        || !op.addStringOption('\0', "ion-regalloc", "[mode]",
                               "Specify Ion register allocation:\n"
                               "  backtracking: Priority based backtracking register allocation (default)\n"
                               "  testbed: Backtracking allocator with experimental features\n"
                               "  stupid: Simple block local register allocation")
        || !op.addBoolOption('\0', "ion-eager", "Always ion-compile methods (implies --baseline-eager)")
        || !op.addStringOption('\0', "ion-offthread-compile", "on/off",
                               "Compile scripts off thread (default: on)")
        || !op.addStringOption('\0', "ion-parallel-compile", "on/off",
                               "--ion-parallel compile is deprecated. Use --ion-offthread-compile.")
        || !op.addBoolOption('\0', "baseline", "Enable baseline compiler (default)")
        || !op.addBoolOption('\0', "no-baseline", "Disable baseline compiler")
        || !op.addBoolOption('\0', "baseline-eager", "Always baseline-compile methods")
        || !op.addIntOption('\0', "baseline-warmup-threshold", "COUNT",
                            "Wait for COUNT calls or iterations before baseline-compiling "
                            "(default: 10)", -1)
        || !op.addBoolOption('\0', "non-writable-jitcode", "Allocate JIT code as non-writable memory.")
        || !op.addBoolOption('\0', "no-fpu", "Pretend CPU does not support floating-point operations "
                             "to test JIT codegen (no-op on platforms other than x86).")
        || !op.addBoolOption('\0', "no-sse3", "Pretend CPU does not support SSE3 instructions and above "
                             "to test JIT codegen (no-op on platforms other than x86 and x64).")
        || !op.addBoolOption('\0', "no-sse4", "Pretend CPU does not support SSE4 instructions"
                             "to test JIT codegen (no-op on platforms other than x86 and x64).")
        || !op.addBoolOption('\0', "enable-avx", "AVX is disabled by default. Enable AVX. "
                             "(no-op on platforms other than x86 and x64).")
        || !op.addBoolOption('\0', "no-avx", "No-op. AVX is currently disabled by default.")
        || !op.addBoolOption('\0', "fuzzing-safe", "Don't expose functions that aren't safe for "
                             "fuzzers to call")
        || !op.addBoolOption('\0', "no-threads", "Disable helper threads")
#ifdef DEBUG
        || !op.addBoolOption('\0', "dump-entrained-variables", "Print variables which are "
                             "unnecessarily entrained by inner functions")
#endif
        || !op.addBoolOption('\0', "no-ggc", "Disable Generational GC")
        || !op.addBoolOption('\0', "no-cgc", "Disable Compacting GC")
        || !op.addBoolOption('\0', "no-incremental-gc", "Disable Incremental GC")
        || !op.addIntOption('\0', "available-memory", "SIZE",
                            "Select GC settings based on available memory (MB)", 0)
#if defined(JS_CODEGEN_ARM)
        || !op.addStringOption('\0', "arm-hwcap", "[features]",
                               "Specify ARM code generation features, or 'help' to list all features.")
        || !op.addIntOption('\0', "arm-asm-nop-fill", "SIZE",
                            "Insert the given number of NOP instructions at all possible pool locations.", 0)
        || !op.addIntOption('\0', "asm-pool-max-offset", "OFFSET",
                            "The maximum pc relative OFFSET permitted in pool reference instructions.", 1024)
#endif
#if defined(JS_SIMULATOR_ARM)
        || !op.addBoolOption('\0', "arm-sim-icache-checks", "Enable icache flush checks in the ARM "
                             "simulator.")
        || !op.addIntOption('\0', "arm-sim-stop-at", "NUMBER", "Stop the ARM simulator after the given "
                            "NUMBER of instructions.", -1)
#elif defined(JS_SIMULATOR_MIPS32)
	|| !op.addBoolOption('\0', "mips-sim-icache-checks", "Enable icache flush checks in the MIPS "
                             "simulator.")
        || !op.addIntOption('\0', "mips-sim-stop-at", "NUMBER", "Stop the MIPS simulator after the given "
                            "NUMBER of instructions.", -1)
#endif
        || !op.addIntOption('\0', "nursery-size", "SIZE-MB", "Set the maximum nursery size in MB", 16)
#ifdef JS_GC_ZEAL
        || !op.addStringOption('z', "gc-zeal", "LEVEL[,N]", gc::ZealModeHelpText)
#endif
    )
    {
        return EXIT_FAILURE;
    }

    op.setArgTerminatesOptions("script", true);
    op.setArgCapturesRest("scriptArgs");

    switch (op.parseArgs(argc, argv)) {
      case OptionParser::EarlyExit:
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
    OOM_printAllocationCount = op.getBoolOption('O');
#endif

    if (op.getBoolOption("non-writable-jitcode")) {
        js::jit::ExecutableAllocator::nonWritableJitCode = true;
        PropagateFlagToNestedShells("--non-writable-jitcode");
    }

#ifdef JS_CODEGEN_X86
    if (op.getBoolOption("no-fpu"))
        js::jit::CPUInfo::SetFloatingPointDisabled();
#endif

#if defined(JS_CODEGEN_X86) || defined(JS_CODEGEN_X64)
    if (op.getBoolOption("no-sse3")) {
        js::jit::CPUInfo::SetSSE3Disabled();
        PropagateFlagToNestedShells("--no-sse3");
    }
    if (op.getBoolOption("no-sse4")) {
        js::jit::CPUInfo::SetSSE4Disabled();
        PropagateFlagToNestedShells("--no-sse4");
    }
    if (op.getBoolOption("enable-avx")) {
        js::jit::CPUInfo::SetAVXEnabled();
        PropagateFlagToNestedShells("--enable-avx");
    }
#endif

    if (op.getBoolOption("no-threads"))
        js::DisableExtraThreads();

    // Start the engine.
    if (!JS_Init())
        return 1;

    if (!InitSharedArrayBufferMailbox())
        return 1;

    // The fake thread count must be set before initializing the Runtime,
    // which spins up the thread pool.
    int32_t threadCount = op.getIntOption("thread-count");
    if (threadCount >= 0)
        SetFakeCPUCount(threadCount);

    size_t nurseryBytes = JS::DefaultNurseryBytes;
    nurseryBytes = op.getIntOption("nursery-size") * 1024L * 1024L;

    /* Use the same parameters as the browser in xpcjsruntime.cpp. */
    rt = JS_NewRuntime(JS::DefaultHeapMaxBytes, nurseryBytes);
    if (!rt)
        return 1;

    JS_SetErrorReporter(rt, my_ErrorReporter);
    JS::SetOutOfMemoryCallback(rt, my_OOMCallback, nullptr);
    if (!SetRuntimeOptions(rt, op))
        return 1;

    gInterruptFunc.init(rt, NullValue());
    gLastWarning.init(rt, NullValue());

    JS_SetGCParameter(rt, JSGC_MAX_BYTES, 0xffffffff);

    size_t availMem = op.getIntOption("available-memory");
    if (availMem > 0)
        JS_SetGCParametersBasedOnAvailableMemory(rt, availMem);

    JS_SetTrustedPrincipals(rt, &ShellPrincipals::fullyTrusted);
    JS_SetSecurityCallbacks(rt, &ShellPrincipals::securityCallbacks);
    JS_InitDestroyPrincipalsCallback(rt, ShellPrincipals::destroy);

    JS_SetInterruptCallback(rt, ShellInterruptCallback);
    JS::SetAsmJSCacheOps(rt, &asmJSCacheOps);

    JS_SetNativeStackQuota(rt, gMaxStackSize);

    JS::dbg::SetDebuggerMallocSizeOf(rt, moz_malloc_size_of);

    if (!offThreadState.init())
        return 1;

    if (!InitWatchdog(rt))
        return 1;

    cx = NewContext(rt);
    if (!cx)
        return 1;

    JS_SetGCParameter(rt, JSGC_MODE, JSGC_MODE_INCREMENTAL);
    JS_SetGCParameterForThread(cx, JSGC_MAX_CODE_CACHE_BYTES, 16 * 1024 * 1024);

    JS::SetLargeAllocationFailureCallback(rt, my_LargeAllocFailCallback, (void*)cx);

    // Set some parameters to allow incremental GC in low memory conditions,
    // as is done for the browser, except in more-deterministic builds or when
    // disabled by command line options.
#ifndef JS_MORE_DETERMINISTIC
    if (!op.getBoolOption("no-incremental-gc")) {
        JS_SetGCParameter(rt, JSGC_DYNAMIC_HEAP_GROWTH, 1);
        JS_SetGCParameter(rt, JSGC_DYNAMIC_MARK_SLICE, 1);
        JS_SetGCParameter(rt, JSGC_SLICE_TIME_BUDGET, 10);
    }
#endif

    js::SetPreserveWrapperCallback(rt, DummyPreserveWrapperCallback);

    result = Shell(cx, &op, envp);

#ifdef DEBUG
    if (OOM_printAllocationCount)
        printf("OOM max count: %u\n", OOM_counter);
#endif

    JS::SetLargeAllocationFailureCallback(rt, nullptr, nullptr);

    DestroyContext(cx, true);

    KillWatchdog();

    MOZ_ASSERT_IF(!CanUseExtraThreads(), workerThreads.empty());
    for (size_t i = 0; i < workerThreads.length(); i++)
        PR_JoinThread(workerThreads[i]);

    DestructSharedArrayBufferMailbox();

    JS_DestroyRuntime(rt);
    JS_ShutDown();
    return result;
}
