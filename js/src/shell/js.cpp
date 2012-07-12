/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * JS shell.
 */
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <locale.h>

#include "mozilla/Util.h"

#include "jstypes.h"
#include "jsutil.h"
#include "jsprf.h"
#include "jswrapper.h"
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
#include "jsreflect.h"
#include "jsscope.h"
#include "jsscript.h"
#include "jstypedarray.h"
#include "jstypedarrayinlines.h"
#include "jsxml.h"
#include "jsperf.h"

#include "builtin/TestingFunctions.h"
#include "frontend/BytecodeEmitter.h"
#include "frontend/Parser.h"
#include "methodjit/MethodJIT.h"

#include "prmjtime.h"

#include "jsoptparse.h"
#include "jsheaptools.h"

#include "jsinferinlines.h"
#include "jsinterpinlines.h"
#include "jsobjinlines.h"
#include "jsscriptinlines.h"

#ifdef XP_UNIX
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#endif

#if defined(XP_WIN) || defined(XP_OS2)
#include <io.h>     /* for isatty() */
#endif

#ifdef XP_WIN
#include "jswin.h"
#endif

using namespace mozilla;
using namespace js;
using namespace js::cli;

typedef enum JSShellExitCode {
    EXITCODE_RUNTIME_ERROR      = 3,
    EXITCODE_FILE_NOT_FOUND     = 4,
    EXITCODE_OUT_OF_MEMORY      = 5,
    EXITCODE_TIMEOUT            = 6
} JSShellExitCode;

size_t gStackChunkSize = 8192;

/* Assume that we can not use more than 5e5 bytes of C stack by default. */
#if (defined(DEBUG) && defined(__SUNPRO_CC))  || defined(JS_CPU_SPARC)
/* Sun compiler uses larger stack space for js_Interpret() with debug
   Use a bigger gMaxStackSize to make "make check" happy. */
#define DEFAULT_MAX_STACK_SIZE 5000000
#else
#define DEFAULT_MAX_STACK_SIZE 500000
#endif

size_t gMaxStackSize = DEFAULT_MAX_STACK_SIZE;


#ifdef JS_THREADSAFE
static PRUintn gStackBaseThreadIndex;
#else
static uintptr_t gStackBase;
#endif

/*
 * Limit the timeout to 30 minutes to prevent an overflow on platfoms
 * that represent the time internally in microseconds using 32-bit int.
 */
static double MAX_TIMEOUT_INTERVAL = 1800.0;
static double gTimeoutInterval = -1.0;
static volatile bool gCanceled = false;

static bool enableMethodJit = false;
static bool enableTypeInference = false;
static bool enableDisassemblyDumps = false;

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

#ifdef DEBUG
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
JS_BEGIN_EXTERN_C
extern JS_EXPORT_API(char *) readline(const char *prompt);
extern JS_EXPORT_API(void)   add_history(char *line);
JS_END_EXTERN_C
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
    ToStringHelper(JSContext *aCx, jsval v, bool aThrow = false)
      : cx(aCx)
    {
        mStr = JS_ValueToString(cx, v);
        if (!aThrow && !mStr)
            ReportException(cx);
        JS_AddNamedStringRoot(cx, &mStr, "Value ToString helper");
    }
    ~ToStringHelper() {
        JS_RemoveStringRoot(cx, &mStr);
    }
    bool threw() { return !mStr; }
    jsval getJSVal() { return STRING_TO_JSVAL(mStr); }
    const char *getBytes() {
        if (mStr && (mBytes.ptr() || mBytes.encode(cx, mStr)))
            return mBytes.ptr();
        return "(error converting value)";
    }
  private:
    JSContext *cx;
    JSString *mStr;
    JSAutoByteString mBytes;
};

class IdStringifier : public ToStringHelper {
public:
    IdStringifier(JSContext *cx, jsid id, bool aThrow = false)
    : ToStringHelper(cx, IdToJsval(id), aThrow)
    { }
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
    if (gCanceled)
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
    if (!gCanceled)
        return true;

    JS_ClearPendingException(cx);
    return false;
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
    if (!js_CStringsAreUTF8)
        return;

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
Process(JSContext *cx, JSObject *obj_, const char *filename, bool forceTTY)
{
    bool ok, hitEOF;
    JSScript *script;
    jsval result;
    JSString *str;
    char *buffer;
    size_t size;
    jschar *uc_buffer;
    size_t uc_len;
    int lineno;
    int startline;
    FILE *file;
    uint32_t oldopts;

    RootedObject obj(cx, obj_);

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

    SetContextOptions(cx);

    if (!forceTTY && !isatty(fileno(file)))
    {
        SkipUTF8BOM(file);

        /*
         * It's not interactive - just execute it.  Support the UNIX #! shell
         * hack, and gobble the first line if it starts with '#'.
         */
        int ch = fgetc(file);
        if (ch == '#') {
            while((ch = fgetc(file)) != EOF) {
                if (ch == '\n' || ch == '\r')
                    break;
            }
        }
        ungetc(ch, file);

        int64_t t1 = PRMJ_Now();
        oldopts = JS_GetOptions(cx);
        gGotError = false;
        JS_SetOptions(cx, oldopts | JSOPTION_COMPILE_N_GO | JSOPTION_NO_SCRIPT_RVAL);
        script = JS_CompileUTF8FileHandle(cx, obj, filename, file);
        JS_SetOptions(cx, oldopts);
        JS_ASSERT_IF(!script, gGotError);
        if (script && !compileOnly) {
            if (!JS_ExecuteScript(cx, obj, script, NULL)) {
                if (!gQuitting && !gCanceled)
                    gExitCode = EXITCODE_RUNTIME_ERROR;
            }
            int64_t t2 = PRMJ_Now() - t1;
            if (printTiming)
                printf("runtime = %.3f ms\n", double(t2) / PRMJ_USEC_PER_MSEC);
        }

        goto cleanup;
    }

    /* It's an interactive filehandle; drop into read-eval-print loop. */
    lineno = 1;
    hitEOF = false;
    buffer = NULL;
    size = 0;           /* assign here to avoid warnings */
    do {
        /*
         * Accumulate lines until we get a 'compilable unit' - one that either
         * generates an error (before running out of source) or that compiles
         * cleanly.  This should be whenever we get a complete statement that
         * coincides with the end of a line.
         */
        startline = lineno;
        size_t len = 0; /* initialize to avoid warnings */
        do {
            ScheduleWatchdog(cx->runtime, -1);
            gCanceled = false;
            errno = 0;

            char *line;
            {
                JSAutoSuspendRequest suspended(cx);
                line = GetLine(file, startline == lineno ? "js> " : "");
            }
            if (!line) {
                if (errno) {
                    JS_ReportError(cx, strerror(errno));
                    free(buffer);
                    goto cleanup;
                }
                hitEOF = true;
                break;
            }
            if (!buffer) {
                buffer = line;
                len = strlen(buffer);
                size = len + 1;
            } else {
                /*
                 * len + 1 is required to store '\n' in the end of line.
                 */
                size_t newlen = strlen(line) + (len ? len + 1 : 0);
                if (newlen + 1 > size) {
                    size = newlen + 1 > size * 2 ? newlen + 1 : size * 2;
                    char *newBuf = (char *) realloc(buffer, size);
                    if (!newBuf) {
                        free(buffer);
                        free(line);
                        JS_ReportOutOfMemory(cx);
                        goto cleanup;
                    }
                    buffer = newBuf;
                }
                char *current = buffer + len;
                if (startline != lineno)
                    *current++ = '\n';
                strcpy(current, line);
                len = newlen;
                free(line);
            }
            lineno++;
            if (!ScheduleWatchdog(cx->runtime, gTimeoutInterval)) {
                hitEOF = true;
                break;
            }
        } while (!JS_BufferIsCompilableUnit(cx, true, obj, buffer, len));

        if (hitEOF && !buffer)
            break;

        if (!JS_DecodeUTF8(cx, buffer, len, NULL, &uc_len)) {
            JS_ReportError(cx, "Invalid UTF-8 in input");
            gExitCode = EXITCODE_RUNTIME_ERROR;
            return;
        }

        uc_buffer = (jschar*)malloc(uc_len * sizeof(jschar));
        JS_DecodeUTF8(cx, buffer, len, uc_buffer, &uc_len);

        /* Clear any pending exception from previous failed compiles. */
        JS_ClearPendingException(cx);

        /* Even though we're interactive, we have a compile-n-go opportunity. */
        oldopts = JS_GetOptions(cx);
        gGotError = false;
        if (!compileOnly)
            JS_SetOptions(cx, oldopts | JSOPTION_COMPILE_N_GO);
        script = JS_CompileUCScript(cx, obj, uc_buffer, uc_len, "typein", startline);
        if (!compileOnly)
            JS_SetOptions(cx, oldopts);

        JS_ASSERT_IF(!script, gGotError);

        if (script && !compileOnly) {
            ok = JS_ExecuteScript(cx, obj, script, &result);
            if (ok && !JSVAL_IS_VOID(result)) {
                str = JS_ValueToSource(cx, result);
                ok = !!str;
                if (ok) {
                    JSAutoByteString bytes(cx, str);
                    ok = !!bytes;
                    if (ok)
                        fprintf(gOutFile, "%s\n", bytes.ptr());
                }
            }
        }
        *buffer = '\0';
        free(uc_buffer);
    } while (!hitEOF && !gQuitting);

    free(buffer);
    fprintf(gOutFile, "\n");
cleanup:
    if (file != stdin)
        fclose(file);
    return;
}

/*
 * JSContext option name to flag map. The option names are in alphabetical
 * order for better reporting.
 */
static const struct JSOption {
    const char  *name;
    uint32_t    flag;
} js_options[] = {
    {"atline",          JSOPTION_ATLINE},
    {"methodjit",       JSOPTION_METHODJIT},
    {"methodjit_always",JSOPTION_METHODJIT_ALWAYS},
    {"relimit",         JSOPTION_RELIMIT},
    {"strict",          JSOPTION_STRICT},
    {"typeinfer",       JSOPTION_TYPE_INFERENCE},
    {"werror",          JSOPTION_WERROR},
    {"allow_xml",       JSOPTION_ALLOW_XML},
    {"moar_xml",        JSOPTION_MOAR_XML},
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
    jsval *argv = JS_ARGV(cx, vp);
    if (argc == 0 || JSVAL_IS_VOID(argv[0])) {
        /* Get version. */
        *vp = INT_TO_JSVAL(JS_GetVersion(cx));
    } else {
        /* Set version. */
        int32_t v = -1;
        if (JSVAL_IS_INT(argv[0])) {
            v = JSVAL_TO_INT(argv[0]);
        } else if (JSVAL_IS_DOUBLE(argv[0])) {
            double fv = JSVAL_TO_DOUBLE(argv[0]);
            if (int32_t(fv) == fv)
                v = int32_t(fv);
        }
        if (v < 0 || v > JSVERSION_LATEST) {
            JS_ReportErrorNumber(cx, my_GetErrorMessage, NULL, JSSMSG_INVALID_ARGS, "version");
            return false;
        }
        *vp = INT_TO_JSVAL(JS_SetVersion(cx, JSVersion(v)));
    }
    return true;
}

static JSBool
RevertVersion(JSContext *cx, unsigned argc, jsval *vp)
{
    js_RevertVersion(cx);
    JS_SET_RVAL(cx, vp, JSVAL_VOID);
    return true;
}

static JSBool
Options(JSContext *cx, unsigned argc, jsval *vp)
{
    uint32_t optset, flag;
    JSString *str;
    char *names;
    bool found;

    optset = 0;
    jsval *argv = JS_ARGV(cx, vp);
    for (unsigned i = 0; i < argc; i++) {
        str = JS_ValueToString(cx, argv[i]);
        if (!str)
            return false;
        argv[i] = STRING_TO_JSVAL(str);
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
    *vp = STRING_TO_JSVAL(str);
    return true;
}

static JSBool
Load(JSContext *cx, unsigned argc, jsval *vp)
{
    RootedObject thisobj(cx, JS_THIS_OBJECT(cx, vp));
    if (!thisobj)
        return false;

    jsval *argv = JS_ARGV(cx, vp);
    for (unsigned i = 0; i < argc; i++) {
        JSString *str = JS_ValueToString(cx, argv[i]);
        if (!str)
            return false;
        argv[i] = STRING_TO_JSVAL(str);
        JSAutoByteString filename(cx, str);
        if (!filename)
            return false;
        errno = 0;
        uint32_t oldopts = JS_GetOptions(cx);
        JS_SetOptions(cx, oldopts | JSOPTION_COMPILE_N_GO | JSOPTION_NO_SCRIPT_RVAL);
        JSScript *script = JS_CompileUTF8File(cx, thisobj, filename.ptr());
        JS_SetOptions(cx, oldopts);
        if (!script)
            return false;

        if (!compileOnly && !JS_ExecuteScript(cx, thisobj, script, NULL))
            return false;
    }

    JS_SET_RVAL(cx, vp, JSVAL_VOID);
    return true;
}

class AutoNewContext
{
  private:
    JSContext *oldcx;
    JSContext *newcx;
    Maybe<JSAutoSuspendRequest> suspension;
    Maybe<JSAutoRequest> newRequest;

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

        suspension.construct(cx);
        newRequest.construct(newcx);
        return true;
    }

    JSContext *get() { return newcx; }

    ~AutoNewContext() {
        if (newcx) {
            RootedValue exc(oldcx);
            bool throwing = JS_IsExceptionPending(newcx);
            if (throwing)
                JS_GetPendingException(newcx, exc.address());
            newRequest.destroy();
            suspension.destroy();
            if (throwing)
                JS_SetPendingException(oldcx, exc);
            JS_DestroyContextNoGC(newcx);
        }
    }
};

static JSBool
Evaluate(JSContext *cx, unsigned argc, jsval *vp)
{
    jsval *argv = JS_ARGV(cx, vp);

    if (argc < 1 || argc > 2) {
        JS_ReportErrorNumber(cx, my_GetErrorMessage, NULL,
                             argc < 1 ? JSSMSG_NOT_ENOUGH_ARGS : JSSMSG_TOO_MANY_ARGS,
                             "evaluate");
        return false;
    }
    if (!JSVAL_IS_STRING(argv[0]) || (argc == 2 && JSVAL_IS_PRIMITIVE(argv[1]))) {
        JS_ReportErrorNumber(cx, my_GetErrorMessage, NULL, JSSMSG_INVALID_ARGS, "evaluate");
        return false;
    }

    bool newContext = false;
    bool compileAndGo = true;
    bool noScriptRval = false;
    const char *fileName = "@evaluate";
    JSAutoByteString fileNameBytes;
    unsigned lineNumber = 1;
    RootedObject global(cx, NULL);

    global = JS_GetGlobalForObject(cx, JSVAL_TO_OBJECT(JS_CALLEE(cx, vp)));
    if (!global)
        return false;

    if (argc == 2) {
        RootedObject options(cx, JSVAL_TO_OBJECT(argv[1]));
        jsval v;

        if (!JS_GetProperty(cx, options, "newContext", &v))
            return false;
        if (!JSVAL_IS_VOID(v)) {
            JSBool b;
            if (!JS_ValueToBoolean(cx, v, &b))
                return false;
            newContext = b;
        }

        if (!JS_GetProperty(cx, options, "compileAndGo", &v))
            return false;
        if (!JSVAL_IS_VOID(v)) {
            JSBool b;
            if (!JS_ValueToBoolean(cx, v, &b))
                return false;
            compileAndGo = b;
        }

        if (!JS_GetProperty(cx, options, "noScriptRval", &v))
            return false;
        if (!JSVAL_IS_VOID(v)) {
            JSBool b;
            if (!JS_ValueToBoolean(cx, v, &b))
                return false;
            noScriptRval = b;
        }

        if (!JS_GetProperty(cx, options, "fileName", &v))
            return false;
        if (JSVAL_IS_NULL(v)) {
            fileName = NULL;
        } else if (!JSVAL_IS_VOID(v)) {
            JSString *s = JS_ValueToString(cx, v);
            if (!s)
                return false;
            fileName = fileNameBytes.encode(cx, s);
            if (!fileName)
                return false;
        }

        if (!JS_GetProperty(cx, options, "lineNumber", &v))
            return false;
        if (!JSVAL_IS_VOID(v)) {
            uint32_t u;
            if (!JS_ValueToECMAUint32(cx, v, &u))
                return false;
            lineNumber = u;
        }

        if (!JS_GetProperty(cx, options, "global", &v))
            return false;
        if (!JSVAL_IS_VOID(v)) {
            global = JSVAL_IS_PRIMITIVE(v) ? NULL : JSVAL_TO_OBJECT(v);
            if (global) {
                global = JS_UnwrapObject(global);
                if (!global)
                    return false;
            }
            if (!global || !(JS_GetClass(global)->flags & JSCLASS_IS_GLOBAL)) {
                JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_UNEXPECTED_TYPE,
                                     "\"global\" passed to evaluate()", "not a global object");
                return false;
            }
        }
    }

    RootedString code(cx, JSVAL_TO_STRING(JS_ARGV(cx, vp)[0]));

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
        JSAutoEnterCompartment aec;
        if (!aec.enter(cx, global))
            return false;

        uint32_t saved = JS_GetOptions(cx);
        uint32_t options = saved & ~(JSOPTION_COMPILE_N_GO | JSOPTION_NO_SCRIPT_RVAL);
        if (compileAndGo)
            options |= JSOPTION_COMPILE_N_GO;
        if (noScriptRval)
            options |= JSOPTION_NO_SCRIPT_RVAL;
        JS_SetOptions(cx, options);

        JSScript *script = JS_CompileUCScript(cx, global, codeChars, codeLength, fileName, lineNumber);
        JS_SetOptions(cx, saved);
        if (!script || !JS_ExecuteScript(cx, global, script, vp))
            return false;
    }

    return JS_WrapValue(cx, vp);
}

static JSString *
FileAsString(JSContext *cx, const char *pathname)
{
    FILE *file;
    JSString *str = NULL;
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
                    jschar *ucbuf;
                    size_t uclen;

                    len = (size_t)cc;

                    if (!JS_DecodeUTF8(cx, buf, len, NULL, &uclen)) {
                        JS_ReportError(cx, "Invalid UTF-8 in file '%s'", pathname);
                        gExitCode = EXITCODE_RUNTIME_ERROR;
                        return NULL;
                    }

                    ucbuf = (jschar*)malloc(uclen * sizeof(jschar));
                    JS_DecodeUTF8(cx, buf, len, ucbuf, &uclen);
                    str = JS_NewUCStringCopyN(cx, ucbuf, uclen);
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

    JSObject *obj = NULL;
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
            char *buf = (char *) TypedArray::viewData(obj);
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
    if (argc != 1) {
        JS_ReportErrorNumber(cx, my_GetErrorMessage, NULL, JSSMSG_INVALID_ARGS, "run");
        return false;
    }

    JSObject *thisobj = JS_THIS_OBJECT(cx, vp);
    if (!thisobj)
        return false;

    jsval *argv = JS_ARGV(cx, vp);
    JSString *str = JS_ValueToString(cx, argv[0]);
    if (!str)
        return false;
    argv[0] = STRING_TO_JSVAL(str);
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
    JSScript *script = JS_CompileUCScript(cx, thisobj, ucbuf, buflen, filename.ptr(), 1);
    JS_SetOptions(cx, oldopts);
    if (!script || !JS_ExecuteScript(cx, thisobj, script, NULL))
        return false;

    int64_t endClock = PRMJ_Now();
    JS_SET_RVAL(cx, vp, DOUBLE_TO_JSVAL((endClock - startClock) / double(PRMJ_USEC_PER_MSEC)));
    return true;
}

/*
 * function readline()
 * Provides a hook for scripts to read a line from stdin.
 */
static JSBool
ReadLine(JSContext *cx, unsigned argc, jsval *vp)
{
#define BUFSIZE 256
    FILE *from;
    char *buf, *tmp;
    size_t bufsize, buflength, gotlength;
    bool sawNewline;
    JSString *str;

    from = stdin;
    buflength = 0;
    bufsize = BUFSIZE;
    buf = (char *) JS_malloc(cx, bufsize);
    if (!buf)
        return false;

    sawNewline = false;
    while ((gotlength =
            js_fgets(buf + buflength, bufsize - buflength, from)) > 0) {
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
        *vp = feof(from) ? JSVAL_NULL : JS_GetEmptyStringValue(cx);
        JS_free(cx, buf);
        return true;
    }

    /* Shrink the buffer to the real size. */
    tmp = (char *) JS_realloc(cx, buf, buflength);
    if (!tmp) {
        JS_free(cx, buf);
        return false;
    }

    buf = tmp;

    /*
     * Turn buf into a JSString. Note that buflength includes the trailing null
     * character.
     */
    str = JS_NewStringCopyN(cx, buf, sawNewline ? buflength - 1 : buflength);
    JS_free(cx, buf);
    if (!str)
        return false;

    *vp = STRING_TO_JSVAL(str);
    return true;
}

static JSBool
PutStr(JSContext *cx, unsigned argc, jsval *vp)
{
    jsval *argv;
    JSString *str;
    char *bytes;

    if (argc != 0) {
        argv = JS_ARGV(cx, vp);
        str = JS_ValueToString(cx, argv[0]);
        if (!str)
            return false;
        bytes = JS_EncodeString(cx, str);
        if (!bytes)
            return false;
        fputs(bytes, gOutFile);
        JS_free(cx, bytes);
        fflush(gOutFile);
    }

    JS_SET_RVAL(cx, vp, JSVAL_VOID);
    return true;
}

static JSBool
Now(JSContext *cx, unsigned argc, jsval *vp)
{
    double now = PRMJ_Now() / double(PRMJ_USEC_PER_MSEC);
    JS_SET_RVAL(cx, vp, DOUBLE_TO_JSVAL(now));
    return true;
}

static JSBool
PrintInternal(JSContext *cx, unsigned argc, jsval *vp, FILE *file)
{
    jsval *argv;
    unsigned i;
    JSString *str;
    char *bytes;

    argv = JS_ARGV(cx, vp);
    for (i = 0; i < argc; i++) {
        str = JS_ValueToString(cx, argv[i]);
        if (!str)
            return false;
        bytes = JS_EncodeString(cx, str);
        if (!bytes)
            return false;
        fprintf(file, "%s%s", i ? " " : "", bytes);
        JS_free(cx, bytes);
    }

    fputc('\n', file);
    fflush(file);

    JS_SET_RVAL(cx, vp, JSVAL_VOID);
    return true;
}

static JSBool
Print(JSContext *cx, unsigned argc, jsval *vp)
{
    return PrintInternal(cx, argc, vp, gOutFile);
}

static JSBool
PrintErr(JSContext *cx, unsigned argc, jsval *vp)
{
    return PrintInternal(cx, argc, vp, gErrFile);
}

static JSBool
Help(JSContext *cx, unsigned argc, jsval *vp);

static JSBool
Quit(JSContext *cx, unsigned argc, jsval *vp)
{
    JS_ConvertArguments(cx, argc, JS_ARGV(cx, vp), "/ i", &gExitCode);

    gQuitting = true;
    return false;
}

static const char *
ToSource(JSContext *cx, jsval *vp, JSAutoByteString *bytes)
{
    JSString *str = JS_ValueToSource(cx, *vp);
    if (str) {
        *vp = STRING_TO_JSVAL(str);
        if (bytes->encode(cx, str))
            return bytes->ptr();
    }
    JS_ClearPendingException(cx);
    return "<<error converting value to string>>";
}

static JSBool
AssertEq(JSContext *cx, unsigned argc, jsval *vp)
{
    if (!(argc == 2 || (argc == 3 && JSVAL_IS_STRING(JS_ARGV(cx, vp)[2])))) {
        JS_ReportErrorNumber(cx, my_GetErrorMessage, NULL,
                             (argc < 2)
                             ? JSSMSG_NOT_ENOUGH_ARGS
                             : (argc == 3)
                             ? JSSMSG_INVALID_ARGS
                             : JSSMSG_TOO_MANY_ARGS,
                             "assertEq");
        return false;
    }

    jsval *argv = JS_ARGV(cx, vp);
    JSBool same;
    if (!JS_SameValue(cx, argv[0], argv[1], &same))
        return false;
    if (!same) {
        JSAutoByteString bytes0, bytes1;
        const char *actual = ToSource(cx, &argv[0], &bytes0);
        const char *expected = ToSource(cx, &argv[1], &bytes1);
        if (argc == 2) {
            JS_ReportErrorNumber(cx, my_GetErrorMessage, NULL, JSSMSG_ASSERT_EQ_FAILED,
                                 actual, expected);
        } else {
            JSAutoByteString bytes2(cx, JSVAL_TO_STRING(argv[2]));
            if (!bytes2)
                return false;
            JS_ReportErrorNumber(cx, my_GetErrorMessage, NULL, JSSMSG_ASSERT_EQ_FAILED_MSG,
                                 actual, expected, bytes2.ptr());
        }
        return false;
    }
    JS_SET_RVAL(cx, vp, JSVAL_VOID);
    return true;
}

static JSBool
AssertJit(JSContext *cx, unsigned argc, jsval *vp)
{
#ifdef JS_METHODJIT
    if (JS_GetOptions(cx) & JSOPTION_METHODJIT) {
        /*
         * :XXX: Ignore calls to this native when inference is enabled,
         * with METHODJIT_ALWAYS recompilation can happen and discard the
         * script's jitcode.
         */
        if (!cx->typeInferenceEnabled() && !cx->fp()->jit()) {
            JS_ReportErrorNumber(cx, my_GetErrorMessage, NULL, JSSMSG_ASSERT_JIT_FAILED);
            return false;
        }
    }
#endif

    JS_SET_RVAL(cx, vp, JSVAL_VOID);
    return true;
}

static JSScript *
ValueToScript(JSContext *cx, jsval v, JSFunction **funp = NULL)
{
    JSFunction *fun = JS_ValueToFunction(cx, v);
    if (!fun)
        return NULL;

    JSScript *script = fun->maybeScript();
    if (!script)
        JS_ReportErrorNumber(cx, my_GetErrorMessage, NULL, JSSMSG_SCRIPTS_ONLY);

    if (fun && funp)
        *funp = fun;

    return script;
}

static JSBool
SetDebug(JSContext *cx, unsigned argc, jsval *vp)
{
    jsval *argv = JS_ARGV(cx, vp);
    if (argc == 0 || !JSVAL_IS_BOOLEAN(argv[0])) {
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

    bool ok = !!JS_SetDebugMode(cx, JSVAL_TO_BOOLEAN(argv[0]));
    if (ok)
        JS_SET_RVAL(cx, vp, JSVAL_TRUE);
    return ok;
}

static JSScript *
GetTopScript(JSContext *cx)
{
    JSScript *script;
    JS_DescribeScriptedCaller(cx, &script, NULL);
    return script;
}

static JSBool
GetScriptAndPCArgs(JSContext *cx, unsigned argc, jsval *argv, JSScript **scriptp,
                   int32_t *ip)
{
    JSScript *script = GetTopScript(cx);
    *ip = 0;
    if (argc != 0) {
        jsval v = argv[0];
        unsigned intarg = 0;
        if (!JSVAL_IS_PRIMITIVE(v) &&
            JS_GetClass(JSVAL_TO_OBJECT(v)) == Jsvalify(&FunctionClass)) {
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

    *scriptp = script;

    return true;
}

static JSTrapStatus
TrapHandler(JSContext *cx, JSScript *, jsbytecode *pc, jsval *rval,
            jsval closure)
{
    JSString *str = JSVAL_TO_STRING(closure);

    ScriptFrameIter iter(cx);
    JS_ASSERT(!iter.done());

    JSStackFrame *caller = Jsvalify(iter.fp());
    JSScript *script = iter.script();

    size_t length;
    const jschar *chars = JS_GetStringCharsAndLength(cx, str, &length);
    if (!chars)
        return JSTRAP_ERROR;

    if (!JS_EvaluateUCInStackFrame(cx, caller, chars, length,
                                   script->filename,
                                   script->lineno,
                                   rval)) {
        return JSTRAP_ERROR;
    }
    if (!JSVAL_IS_VOID(*rval))
        return JSTRAP_RETURN;
    return JSTRAP_CONTINUE;
}

static JSBool
Trap(JSContext *cx, unsigned argc, jsval *vp)
{
    JSString *str;
    JSScript *script;
    int32_t i;

    jsval *argv = JS_ARGV(cx, vp);
    if (argc == 0) {
        JS_ReportErrorNumber(cx, my_GetErrorMessage, NULL, JSSMSG_TRAP_USAGE);
        return false;
    }
    argc--;
    str = JS_ValueToString(cx, argv[argc]);
    if (!str)
        return false;
    argv[argc] = STRING_TO_JSVAL(str);
    if (!GetScriptAndPCArgs(cx, argc, argv, &script, &i))
        return false;
    if (uint32_t(i) >= script->length) {
        JS_ReportErrorNumber(cx, my_GetErrorMessage, NULL, JSSMSG_TRAP_USAGE);
        return false;
    }
    JS_SET_RVAL(cx, vp, JSVAL_VOID);
    return JS_SetTrap(cx, script, script->code + i, TrapHandler, STRING_TO_JSVAL(str));
}

static JSBool
Untrap(JSContext *cx, unsigned argc, jsval *vp)
{
    JSScript *script;
    int32_t i;

    if (!GetScriptAndPCArgs(cx, argc, JS_ARGV(cx, vp), &script, &i))
        return false;
    JS_ClearTrap(cx, script, script->code + i, NULL, NULL);
    JS_SET_RVAL(cx, vp, JSVAL_VOID);
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
    JSString *str;
    if (argc == 0) {
        JS_ReportErrorNumber(cx, my_GetErrorMessage, NULL,
                             JSSMSG_NOT_ENOUGH_ARGS, "setDebuggerHandler");
        return false;
    }

    str = JS_ValueToString(cx, JS_ARGV(cx, vp)[0]);
    if (!str)
        return false;

    JS_SetDebuggerHandler(cx->runtime, DebuggerAndThrowHandler, str);
    JS_SET_RVAL(cx, vp, JSVAL_VOID);
    return true;
}

static JSBool
SetThrowHook(JSContext *cx, unsigned argc, jsval *vp)
{
    JSString *str;
    if (argc == 0) {
        JS_ReportErrorNumber(cx, my_GetErrorMessage, NULL,
                             JSSMSG_NOT_ENOUGH_ARGS, "setThrowHook");
        return false;
    }

    str = JS_ValueToString(cx, JS_ARGV(cx, vp)[0]);
    if (!str)
        return false;

    JS_SetThrowHook(cx->runtime, DebuggerAndThrowHandler, str);
    JS_SET_RVAL(cx, vp, JSVAL_VOID);
    return true;
}

static JSBool
LineToPC(JSContext *cx, unsigned argc, jsval *vp)
{
    JSScript *script;
    int32_t lineArg = 0;
    uint32_t lineno;
    jsbytecode *pc;

    if (argc == 0) {
        JS_ReportErrorNumber(cx, my_GetErrorMessage, NULL, JSSMSG_LINE2PC_USAGE);
        return false;
    }
    script = GetTopScript(cx);
    jsval v = JS_ARGV(cx, vp)[0];
    if (!JSVAL_IS_PRIMITIVE(v) &&
        JS_GetClass(JSVAL_TO_OBJECT(v)) == Jsvalify(&FunctionClass))
    {
        script = ValueToScript(cx, v);
        if (!script)
            return false;
        lineArg++;
    }
    if (!JS_ValueToECMAUint32(cx, JS_ARGV(cx, vp)[lineArg], &lineno))
        return false;
    pc = JS_LineNumberToPC(cx, script, lineno);
    if (!pc)
        return false;
    *vp = INT_TO_JSVAL(pc - script->code);
    return true;
}

static JSBool
PCToLine(JSContext *cx, unsigned argc, jsval *vp)
{
    JSScript *script;
    int32_t i;
    unsigned lineno;

    if (!GetScriptAndPCArgs(cx, argc, JS_ARGV(cx, vp), &script, &i))
        return false;
    lineno = JS_PCToLineNumber(cx, script, script->code + i);
    if (!lineno)
        return false;
    *vp = INT_TO_JSVAL(lineno);
    return true;
}

#ifdef DEBUG

static void
UpdateSwitchTableBounds(JSContext *cx, JSScript *script, unsigned offset,
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

      case JSOP_LOOKUPSWITCH:
        jmplen = JUMP_OFFSET_LEN;
        pc += jmplen;
        n = GET_UINT16(pc);
        pc += UINT16_LEN;
        jmplen += JUMP_OFFSET_LEN;
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
SrcNotes(JSContext *cx, JSScript *script, Sprinter *sp)
{
    Sprint(sp, "\nSource notes:\n");
    Sprint(sp, "%4s  %4s %5s %6s %-8s %s\n",
           "ofs", "line", "pc", "delta", "desc", "args");
    Sprint(sp, "---- ---- ----- ------ -------- ------\n");
    unsigned offset = 0;
    unsigned lineno = script->lineno;
    jssrcnote *notes = script->notes();
    unsigned switchTableEnd = 0, switchTableStart = 0;
    for (jssrcnote *sn = notes; !SN_IS_TERMINATOR(sn); sn = SN_NEXT(sn)) {
        unsigned delta = SN_DELTA(sn);
        offset += delta;
        SrcNoteType type = (SrcNoteType) SN_TYPE(sn);
        const char *name = js_SrcNoteSpec[type].name;
        if (type == SRC_LABEL) {
            /* Check if the source note is for a switch case. */
            if (switchTableStart <= offset && offset < switchTableEnd)
                name = "case";
        }
        Sprint(sp, "%3u: %4u %5u [%4u] %-8s", unsigned(sn - notes), lineno, offset, delta, name);
        switch (type) {
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
            Sprint(sp, " else %u elseif %u",
                   unsigned(js_GetSrcNoteOffset(sn, 0)),
                   unsigned(js_GetSrcNoteOffset(sn, 1)));
            break;
          case SRC_COND:
          case SRC_WHILE:
          case SRC_PCBASE:
          case SRC_PCDELTA:
          case SRC_DECL:
          case SRC_BRACE:
            Sprint(sp, " offset %u", unsigned(js_GetSrcNoteOffset(sn, 0)));
            break;
          case SRC_LABEL:
          case SRC_LABELBRACE:
          case SRC_BREAK2LABEL:
          case SRC_CONT2LABEL: {
            uint32_t index = js_GetSrcNoteOffset(sn, 0);
            JSAtom *atom = script->getAtom(index);
            Sprint(sp, " atom %u (", index);
            size_t len = PutEscapedString(NULL, 0, atom, '\0');
            if (char *buf = sp->reserve(len)) {
                PutEscapedString(buf, len + 1, atom, 0);
                buf[len] = 0;
            }
            Sprint(sp, ")");
            break;
          }
          case SRC_FUNCDEF: {
            uint32_t index = js_GetSrcNoteOffset(sn, 0);
            JSObject *obj = script->getObject(index);
            JSFunction *fun = obj->toFunction();
            JSString *str = JS_DecompileFunction(cx, fun, JS_DONT_PRETTY_PRINT);
            JSAutoByteString bytes;
            if (!str || !bytes.encode(cx, str))
                ReportException(cx);
            Sprint(sp, " function %u (%s)", index, !!bytes ? bytes.ptr() : "N/A");
            break;
          }
          case SRC_SWITCH: {
            JSOp op = JSOp(script->code[offset]);
            if (op == JSOP_GOTO)
                break;
            Sprint(sp, " length %u", unsigned(js_GetSrcNoteOffset(sn, 0)));
            unsigned caseOff = (unsigned) js_GetSrcNoteOffset(sn, 1);
            if (caseOff)
                Sprint(sp, " first case offset %u", caseOff);
            UpdateSwitchTableBounds(cx, script, offset,
                                    &switchTableStart, &switchTableEnd);
            break;
          }
          case SRC_CATCH:
            delta = (unsigned) js_GetSrcNoteOffset(sn, 0);
            if (delta) {
                if (script->main()[offset] == JSOP_LEAVEBLOCK)
                    Sprint(sp, " stack depth %u", delta);
                else
                    Sprint(sp, " guard delta %u", delta);
            }
            break;
          default:;
        }
        Sprint(sp, "\n");
    }
}

static JSBool
Notes(JSContext *cx, unsigned argc, jsval *vp)
{
    Sprinter sprinter(cx);
    if (!sprinter.init())
        return false;

    jsval *argv = JS_ARGV(cx, vp);
    for (unsigned i = 0; i < argc; i++) {
        JSScript *script = ValueToScript(cx, argv[i]);
        if (!script)
            continue;

        SrcNotes(cx, script, &sprinter);
    }

    JSString *str = JS_NewStringCopyZ(cx, sprinter.string());
    if (!str)
        return false;
    JS_SET_RVAL(cx, vp, STRING_TO_JSVAL(str));
    return true;
}

JS_STATIC_ASSERT(JSTRY_CATCH == 0);
JS_STATIC_ASSERT(JSTRY_FINALLY == 1);
JS_STATIC_ASSERT(JSTRY_ITER == 2);

static const char* const TryNoteNames[] = { "catch", "finally", "iter" };

static JSBool
TryNotes(JSContext *cx, JSScript *script, Sprinter *sp)
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
DisassembleScript(JSContext *cx, JSScript *script_, JSFunction *fun, bool lines, bool recursive,
                  Sprinter *sp)
{
    Rooted<JSScript*> script(cx, script_);

    if (fun && (fun->flags & ~7U)) {
        uint16_t flags = fun->flags;
        Sprint(sp, "flags:");

#define SHOW_FLAG(flag) if (flags & JSFUN_##flag) Sprint(sp, " " #flag);

        SHOW_FLAG(LAMBDA);
        SHOW_FLAG(HEAVYWEIGHT);
        SHOW_FLAG(EXPR_CLOSURE);

#undef SHOW_FLAG

        if (fun->isNullClosure())
            Sprint(sp, " NULL_CLOSURE");

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
            if (obj->isFunction()) {
                Sprint(sp, "\n");
                JSFunction *fun = obj->toFunction();
                JSScript *nested = fun->maybeScript();
                if (!DisassembleScript(cx, nested, fun, lines, recursive, sp))
                    return false;
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
        while (argc > 0 && JSVAL_IS_STRING(argv[0])) {
            JSString *str = JSVAL_TO_STRING(argv[0]);
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

static JSBool
DisassembleToString(JSContext *cx, unsigned argc, jsval *vp)
{
    DisassembleOptionParser p(argc, JS_ARGV(cx, vp));
    if (!p.parse(cx))
        return false;

    Sprinter sprinter(cx);
    if (!sprinter.init())
        return false;

    bool ok = true;
    if (p.argc == 0) {
        /* Without arguments, disassemble the current script. */
        if (JSScript *script = GetTopScript(cx)) {
            if (js_Disassemble(cx, script, p.lines, &sprinter)) {
                SrcNotes(cx, script, &sprinter);
                TryNotes(cx, script, &sprinter);
            } else {
                ok = false;
            }
        }
    } else {
        for (unsigned i = 0; i < p.argc; i++) {
            JSFunction *fun;
            JSScript *script = ValueToScript(cx, p.argv[i], &fun);
            ok = ok && script && DisassembleScript(cx, script, fun, p.lines, p.recursive, &sprinter);
        }
    }

    JSString *str = ok ? JS_NewStringCopyZ(cx, sprinter.string()) : NULL;
    if (!str)
        return false;
    JS_SET_RVAL(cx, vp, STRING_TO_JSVAL(str));
    return true;
}

static JSBool
Disassemble(JSContext *cx, unsigned argc, jsval *vp)
{
    DisassembleOptionParser p(argc, JS_ARGV(cx, vp));
    if (!p.parse(cx))
        return false;

    Sprinter sprinter(cx);
    if (!sprinter.init())
        return false;

    bool ok = true;
    if (p.argc == 0) {
        /* Without arguments, disassemble the current script. */
        if (JSScript *script = GetTopScript(cx)) {
            if (js_Disassemble(cx, script, p.lines, &sprinter)) {
                SrcNotes(cx, script, &sprinter);
                TryNotes(cx, script, &sprinter);
            } else {
                ok = false;
            }
        }
    } else {
        for (unsigned i = 0; i < p.argc; i++) {
            JSFunction *fun;
            JSScript *script = ValueToScript(cx, p.argv[i], &fun);
            ok = ok && script && DisassembleScript(cx, script, fun, p.lines, p.recursive, &sprinter);
        }
    }

    if (ok)
        fprintf(stdout, "%s\n", sprinter.string());
    JS_SET_RVAL(cx, vp, JSVAL_VOID);
    return ok;
}

static JSBool
DisassFile(JSContext *cx, unsigned argc, jsval *vp)
{
    /* Support extra options at the start, just like Dissassemble. */
    DisassembleOptionParser p(argc, JS_ARGV(cx, vp));
    if (!p.parse(cx))
        return false;

    if (!p.argc) {
        JS_SET_RVAL(cx, vp, JSVAL_VOID);
        return true;
    }

    JSObject *thisobj = JS_THIS_OBJECT(cx, vp);
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
    JSScript *script = JS_CompileUTF8File(cx, thisobj, filename.ptr());
    JS_SetOptions(cx, oldopts);
    if (!script)
        return false;

    Sprinter sprinter(cx);
    if (!sprinter.init())
        return false;
    bool ok = DisassembleScript(cx, script, NULL, p.lines, p.recursive, &sprinter);
    if (ok)
        fprintf(stdout, "%s\n", sprinter.string());
    if (!ok)
        return false;

    JS_SET_RVAL(cx, vp, JSVAL_VOID);
    return true;
}

static JSBool
DisassWithSrc(JSContext *cx, unsigned argc, jsval *vp)
{
#define LINE_BUF_LEN 512
    unsigned i, len, line1, line2, bupline;
    JSScript *script;
    FILE *file;
    char linebuf[LINE_BUF_LEN];
    jsbytecode *pc, *end;
    bool ok;
    static char sep[] = ";-------------------------";

    ok = true;
    jsval *argv = JS_ARGV(cx, vp);
    for (i = 0; ok && i < argc; i++) {
        script = ValueToScript(cx, argv[i]);
        if (!script)
           return false;

        if (!script->filename) {
            JS_ReportErrorNumber(cx, my_GetErrorMessage, NULL,
                                 JSSMSG_FILE_SCRIPTS_ONLY);
            return false;
        }

        file = fopen(script->filename, "r");
        if (!file) {
            JS_ReportErrorNumber(cx, my_GetErrorMessage, NULL,
                                 JSSMSG_CANT_OPEN, script->filename,
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
                JS_ReportError(cx, "failed to read %s fully", script->filename);
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
                                             script->filename);
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
    JS_SET_RVAL(cx, vp, JSVAL_VOID);
    return ok;
#undef LINE_BUF_LEN
}

static JSBool
DumpHeap(JSContext *cx, unsigned argc, jsval *vp)
{
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
    if (argc > 0) {
        v = JS_ARGV(cx, vp)[0];
        if (!JSVAL_IS_NULL(v)) {
            JSString *str;

            str = JS_ValueToString(cx, v);
            if (!str)
                return false;
            JS_ARGV(cx, vp)[0] = STRING_TO_JSVAL(str);
            if (!fileNameBytes.encode(cx, str))
                return false;
            fileName = fileNameBytes.ptr();
        }
    }

    startThing = NULL;
    startTraceKind = JSTRACE_OBJECT;
    if (argc > 1) {
        v = JS_ARGV(cx, vp)[1];
        if (JSVAL_IS_TRACEABLE(v)) {
            startThing = JSVAL_TO_TRACEABLE(v);
            startTraceKind = JSVAL_TRACE_KIND(v);
        } else if (!JSVAL_IS_NULL(v)) {
            badTraceArg = "start";
            goto not_traceable_arg;
        }
    }

    thingToFind = NULL;
    if (argc > 2) {
        v = JS_ARGV(cx, vp)[2];
        if (JSVAL_IS_TRACEABLE(v)) {
            thingToFind = JSVAL_TO_TRACEABLE(v);
        } else if (!JSVAL_IS_NULL(v)) {
            badTraceArg = "toFind";
            goto not_traceable_arg;
        }
    }

    maxDepth = (size_t)-1;
    if (argc > 3) {
        v = JS_ARGV(cx, vp)[3];
        if (!JSVAL_IS_NULL(v)) {
            uint32_t depth;

            if (!JS_ValueToECMAUint32(cx, v, &depth))
                return false;
            maxDepth = depth;
        }
    }

    thingToIgnore = NULL;
    if (argc > 4) {
        v = JS_ARGV(cx, vp)[4];
        if (JSVAL_IS_TRACEABLE(v)) {
            thingToIgnore = JSVAL_TO_TRACEABLE(v);
        } else if (!JSVAL_IS_NULL(v)) {
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
    JS_SET_RVAL(cx, vp, JSVAL_VOID);
    return true;

  not_traceable_arg:
    JS_ReportError(cx, "argument '%s' is not null or a heap-allocated thing",
                   badTraceArg);
    return false;
}

static JSBool
DumpObject(JSContext *cx, unsigned argc, jsval *vp)
{
    JSObject *arg0 = NULL;
    if (!JS_ConvertArguments(cx, argc, JS_ARGV(cx, vp), "o", &arg0))
        return false;

    js_DumpObject(arg0);

    JS_SET_RVAL(cx, vp, JSVAL_VOID);
    return true;
}

#endif /* DEBUG */

#ifdef TEST_CVTARGS
#include <ctype.h>

static const char *
EscapeWideString(jschar *w)
{
    static char enuf[80];
    static char hex[] = "0123456789abcdef";
    jschar u;
    unsigned char b, c;
    int i, j;

    if (!w)
        return "";
    for (i = j = 0; i < sizeof enuf - 1; i++, j++) {
        u = w[j];
        if (u == 0)
            break;
        b = (unsigned char)(u >> 8);
        c = (unsigned char)(u);
        if (b) {
            if (i >= sizeof enuf - 6)
                break;
            enuf[i++] = '\\';
            enuf[i++] = 'u';
            enuf[i++] = hex[b >> 4];
            enuf[i++] = hex[b & 15];
            enuf[i++] = hex[c >> 4];
            enuf[i] = hex[c & 15];
        } else if (!isprint(c)) {
            if (i >= sizeof enuf - 4)
                break;
            enuf[i++] = '\\';
            enuf[i++] = 'x';
            enuf[i++] = hex[c >> 4];
            enuf[i] = hex[c & 15];
        } else {
            enuf[i] = (char)c;
        }
    }
    enuf[i] = 0;
    return enuf;
}

#include <stdarg.h>

static JSBool
ZZ_formatter(JSContext *cx, const char *format, bool fromJS, jsval **vpp,
             va_list *app)
{
    jsval *vp;
    va_list ap;
    double re, im;

    printf("entering ZZ_formatter");
    vp = *vpp;
    ap = *app;
    if (fromJS) {
        if (!JS_ValueToNumber(cx, vp[0], &re))
            return false;
        if (!JS_ValueToNumber(cx, vp[1], &im))
            return false;
        *va_arg(ap, double *) = re;
        *va_arg(ap, double *) = im;
    } else {
        re = va_arg(ap, double);
        im = va_arg(ap, double);
        if (!JS_NewNumberValue(cx, re, &vp[0]))
            return false;
        if (!JS_NewNumberValue(cx, im, &vp[1]))
            return false;
    }
    *vpp = vp + 2;
    *app = ap;
    printf("leaving ZZ_formatter");
    return true;
}

static JSBool
ConvertArgs(JSContext *cx, unsigned argc, jsval *vp)
{
    bool b = false;
    jschar c = 0;
    int32_t i = 0, j = 0;
    uint32_t u = 0;
    double d = 0, I = 0, re = 0, im = 0;
    JSString *str = NULL;
    jschar *w = NULL;
    JSObject *obj2 = NULL;
    JSFunction *fun = NULL;
    jsval v = JSVAL_VOID;
    bool ok;

    if (!JS_AddArgumentFormatter(cx, "ZZ", ZZ_formatter))
        return false;
    ok = JS_ConvertArguments(cx, argc, JS_ARGV(cx, vp), "b/ciujdISWofvZZ*",
                             &b, &c, &i, &u, &j, &d, &I, &str, &w, &obj2,
                             &fun, &v, &re, &im);
    JS_RemoveArgumentFormatter(cx, "ZZ");
    if (!ok)
        return false;
    fprintf(gOutFile,
            "b %u, c %x (%c), i %ld, u %lu, j %ld\n",
            b, c, (char)c, i, u, j);
    ToStringHelper obj2string(cx, obj2);
    ToStringHelper valueString(cx, v);
    JSAutoByteString strBytes;
    if (str)
        strBytes.encode(cx, str);
    JSString *tmpstr = JS_DecompileFunction(cx, fun, 4);
    JSAutoByteString func;
    if (!tmpstr || !func.encode(cx, tmpstr))
        ReportException(cx);
    fprintf(gOutFile,
            "d %g, I %g, S %s, W %s, obj %s, fun %s\n"
            "v %s, re %g, im %g\n",
            d, I, !!strBytes ? strBytes.ptr() : "", EscapeWideString(w),
            obj2string.getBytes(),
            fun ? (!!func ? func.ptr() : "error decompiling fun") : "",
            valueString.getBytes(), re, im);
    JS_SET_RVAL(cx, vp, JSVAL_VOID);
    return true;
}
#endif

static JSBool
BuildDate(JSContext *cx, unsigned argc, jsval *vp)
{
    char version[20] = "\n";
#if JS_VERSION < 150
    sprintf(version, " for version %d\n", JS_VERSION);
#endif
    fprintf(gOutFile, "built on %s at %s%s", __DATE__, __TIME__, version);
    *vp = JSVAL_VOID;
    return true;
}

static JSBool
Intern(JSContext *cx, unsigned argc, jsval *vp)
{
    JSString *str = JS_ValueToString(cx, argc == 0 ? JSVAL_VOID : vp[2]);
    if (!str)
        return false;

    size_t length;
    const jschar *chars = JS_GetStringCharsAndLength(cx, str, &length);
    if (!chars)
        return false;

    if (!JS_InternUCStringN(cx, chars, length))
        return false;

    JS_SET_RVAL(cx, vp, JSVAL_VOID);
    return true;
}

static JSBool
Clone(JSContext *cx, unsigned argc, jsval *vp)
{
    JSObject *funobj, *parent, *clone;

    if (!argc) {
        JS_ReportError(cx, "Invalid arguments to clone");
        return false;
    }

    jsval *argv = JS_ARGV(cx, vp);
    {
        JSAutoEnterCompartment ac;
        if (!JSVAL_IS_PRIMITIVE(argv[0]) &&
            IsCrossCompartmentWrapper(JSVAL_TO_OBJECT(argv[0])))
        {
            JSObject *obj = UnwrapObject(JSVAL_TO_OBJECT(argv[0]));
            if (!ac.enter(cx, obj))
                return false;
            argv[0] = OBJECT_TO_JSVAL(obj);
        }
        if (!JSVAL_IS_PRIMITIVE(argv[0]) && JSVAL_TO_OBJECT(argv[0])->isFunction()) {
            funobj = JSVAL_TO_OBJECT(argv[0]);
        } else {
            JSFunction *fun = JS_ValueToFunction(cx, argv[0]);
            if (!fun)
                return false;
            funobj = JS_GetFunctionObject(fun);
        }
    }
    if (funobj->compartment() != cx->compartment) {
        JSFunction *fun = funobj->toFunction();
        if (fun->isInterpreted() && fun->script()->compileAndGo) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_UNEXPECTED_TYPE,
                                 "function", "compile-and-go");
            return false;
        }
    }

    if (argc > 1) {
        if (!JS_ValueToObject(cx, argv[1], &parent))
            return false;
    } else {
        parent = JS_GetParent(JSVAL_TO_OBJECT(JS_CALLEE(cx, vp)));
    }

    clone = JS_CloneFunctionObject(cx, funobj, parent);
    if (!clone)
        return false;
    *vp = OBJECT_TO_JSVAL(clone);
    return true;
}

static JSBool
GetPDA(JSContext *cx, unsigned argc, jsval *vp)
{
    JSObject *vobj, *aobj, *pdobj;
    bool ok;
    JSPropertyDescArray pda;
    JSPropertyDesc *pd;
    jsval v;

    if (!JS_ValueToObject(cx, argc == 0 ? JSVAL_VOID : vp[2], &vobj))
        return false;
    if (!vobj) {
        *vp = JSVAL_VOID;
        return true;
    }

    aobj = JS_NewArrayObject(cx, 0, NULL);
    if (!aobj)
        return false;
    *vp = OBJECT_TO_JSVAL(aobj);

    ok = !!JS_GetPropertyDescArray(cx, vobj, &pda);
    if (!ok)
        return false;
    pd = pda.array;
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

        ok = JS_SetProperty(cx, pdobj, "id", &pd->id) &&
             JS_SetProperty(cx, pdobj, "value", &pd->value) &&
             (v = INT_TO_JSVAL(pd->flags),
              JS_SetProperty(cx, pdobj, "flags", &v)) &&
             JS_SetProperty(cx, pdobj, "alias", &pd->alias);
        if (!ok)
            break;
    }
    JS_PutPropertyDescArray(cx, &pda);
    return ok;
}

static JSBool
GetSLX(JSContext *cx, unsigned argc, jsval *vp)
{
    JSScript *script;

    script = ValueToScript(cx, argc == 0 ? JSVAL_VOID : vp[2]);
    if (!script)
        return false;
    *vp = INT_TO_JSVAL(js_GetScriptLineExtent(script));
    return true;
}

static JSBool
ToInt32(JSContext *cx, unsigned argc, jsval *vp)
{
    int32_t i;

    if (!JS_ValueToInt32(cx, argc == 0 ? JSVAL_VOID : vp[2], &i))
        return false;
    return JS_NewNumberValue(cx, i, vp);
}

static JSBool
StringsAreUTF8(JSContext *cx, unsigned argc, jsval *vp)
{
    *vp = JS_CStringsAreUTF8() ? JSVAL_TRUE : JSVAL_FALSE;
    return true;
}

static const char* badUTF8 = "...\xC0...";
static const char* bigUTF8 = "...\xFB\xBF\xBF\xBF\xBF...";
static const jschar badSurrogate[] = { 'A', 'B', 'C', 0xDEEE, 'D', 'E', 0 };

static JSBool
TestUTF8(JSContext *cx, unsigned argc, jsval *vp)
{
    int32_t mode = 1;
    jschar chars[20];
    size_t charsLength = 5;
    char bytes[20];
    size_t bytesLength = 20;
    if (argc && !JS_ValueToInt32(cx, *JS_ARGV(cx, vp), &mode))
        return false;

    /* The following throw errors if compiled with UTF-8. */
    switch (mode) {
      /* mode 1: malformed UTF-8 string. */
      case 1:
        JS_NewStringCopyZ(cx, badUTF8);
        break;
      /* mode 2: big UTF-8 character. */
      case 2:
        JS_NewStringCopyZ(cx, bigUTF8);
        break;
      /* mode 3: bad surrogate character. */
      case 3:
        JS_EncodeCharacters(cx, badSurrogate, 6, bytes, &bytesLength);
        break;
      /* mode 4: use a too small buffer. */
      case 4:
        JS_DecodeBytes(cx, "1234567890", 10, chars, &charsLength);
        break;
      default:
        JS_ReportError(cx, "invalid mode parameter");
        return false;
    }
    JS_SET_RVAL(cx, vp, JSVAL_VOID);
    return !JS_IsExceptionPending (cx);
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
    jsval v;
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
    jsval v;
    JSBool b, resolved;

    if (!JS_GetProperty(cx, obj, "lazy", &v))
        return false;

    JS_ValueToBoolean(cx, v, &b);
    if (b && (flags & JSRESOLVE_ASSIGNING) == 0) {
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
    JS_PropertyStub,   JS_PropertyStub,
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
        JSAutoEnterCompartment ac;
        if (!ac.enter(cx, obj))
            return NULL;

        if (!lazy && !JS_InitStandardClasses(cx, obj))
            return NULL;

        RootedValue value(cx, BooleanValue(lazy));
        if (!JS_SetProperty(cx, obj, "lazy", value.address()))
            return NULL;
    }

    if (!cx->compartment->wrap(cx, obj.address()))
        return NULL;
    return obj;
}

static JSBool
EvalInContext(JSContext *cx, unsigned argc, jsval *vp)
{
    JSString *str;
    RootedObject sobj(cx);
    if (!JS_ConvertArguments(cx, argc, JS_ARGV(cx, vp), "S / o", &str, sobj.address()))
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

    JSScript *script;
    unsigned lineno;

    JS_DescribeScriptedCaller(cx, &script, &lineno);
    jsval rval;
    {
        JSAutoEnterCompartment ac;
        unsigned flags;
        JSObject *unwrapped = UnwrapObject(sobj, true, &flags);
        if (flags & Wrapper::CROSS_COMPARTMENT) {
            sobj = unwrapped;
            if (!ac.enter(cx, sobj))
                return false;
        }

        sobj = GetInnerObject(cx, sobj);
        if (!sobj)
            return false;
        if (!(sobj->getClass()->flags & JSCLASS_IS_GLOBAL)) {
            JS_ReportError(cx, "Invalid scope argument to evalcx");
            return false;
        }
        if (!JS_EvaluateUCScript(cx, sobj, src, srclen,
                                 script->filename,
                                 lineno,
                                 &rval)) {
            return false;
        }
    }

    if (!cx->compartment->wrap(cx, &rval))
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
    JSString *str = JSVAL_TO_STRING(argv[1]);

    bool saveCurrent = (argc >= 3 && JSVAL_IS_BOOLEAN(argv[2]))
                        ? !!(JSVAL_TO_BOOLEAN(argv[2]))
                        : false;

    JS_ASSERT(cx->hasfp());

    ScriptFrameIter fi(cx);
    for (uint32_t i = 0; i < upCount; ++i, ++fi) {
        if (!fi.fp()->prev())
            break;
    }

    StackFrame *const fp = fi.fp();
    if (!fp->isScriptFrame()) {
        JS_ReportError(cx, "cannot eval in non-script frame");
        return false;
    }

    bool saved = false;
    if (saveCurrent)
        saved = JS_SaveFrameChain(cx);

    size_t length;
    const jschar *chars = JS_GetStringCharsAndLength(cx, str, &length);
    if (!chars)
        return false;

    bool ok = !!JS_EvaluateUCInStackFrame(cx, Jsvalify(fp), chars, length,
                                          fp->script()->filename,
                                          JS_PCToLineNumber(cx, fp->script(),
                                                            fi.pc()),
                                          vp);

    if (saved)
        JS_RestoreFrameChain(cx);

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
    return JS_NewNumberValue(cx, (double) ((uintptr_t)obj->lastProperty() >> 3), vp);
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
    PropertyDescriptor desc;
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
   } else if (IsProxy(referent)) {
        PropertyDescriptor desc;
        if (!Proxy::getOwnPropertyDescriptor(cx, referent, id, false, &desc))
            return false;
        if (!desc.obj)
            return true;
    } else {
        if (!referent->lookupGeneric(cx, id, objp, &shape))
            return false;
        if (objp != referent)
            return true;
        if (!referent->getGeneric(cx, id, &desc.value) ||
            !referent->getGenericAttributes(cx, id, &desc.attrs)) {
            return false;
        }
        desc.attrs &= JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT;
        desc.getter = JS_PropertyStub;
        desc.setter = JS_StrictPropertyStub;
        desc.shortid = 0;
    }

    objp.set(obj);
    return !!DefineNativeProperty(cx, obj, id, desc.value, desc.getter, desc.setter,
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
        ok = CopyProperty(cx, obj, referent, id, JSRESOLVE_QUALIFIED, &ignore);
    }
    return ok;
}

static JSClass resolver_class = {
    "resolver",
    JSCLASS_NEW_RESOLVE | JSCLASS_HAS_RESERVED_SLOTS(1),
    JS_PropertyStub,   JS_PropertyStub,
    JS_PropertyStub,   JS_StrictPropertyStub,
    resolver_enumerate, (JSResolveOp)resolver_resolve,
    JS_ConvertStub
};


static JSBool
Resolver(JSContext *cx, unsigned argc, jsval *vp)
{
    JSObject *referent, *proto = NULL;
    if (!JS_ConvertArguments(cx, argc, JS_ARGV(cx, vp), "o/o", &referent, &proto))
        return false;

    JSObject *result = (argc > 1
                        ? JS_NewObjectWithGivenProto
                        : JS_NewObject)(cx, &resolver_class, proto, JS_GetParent(referent));
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

        if (!JS_ValueToNumber(cx, argc == 0 ? JSVAL_VOID : vp[2], &t_secs))
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
    if (t_ticks == 0) {
        JS_YieldRequest(cx);
    } else {
        JSAutoSuspendRequest suspended(cx);
        PR_Lock(gWatchdogLock);
        int64_t to_wakeup = PRMJ_Now() + t_ticks;
        for (;;) {
            PR_WaitCondVar(gSleepWakeup, t_ticks);
            if (gCanceled)
                break;
            int64_t now = PRMJ_Now();
            if (!IsBefore(now, to_wakeup))
                break;
            t_ticks = to_wakeup - now;
        }
        PR_Unlock(gWatchdogLock);
    }
    return !gCanceled;
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
            int64_t sleepDuration = gWatchdogHasTimeout
                                    ? gWatchdogTimeout - now
                                    : PR_INTERVAL_NO_TIMEOUT;
            DebugOnly<PRStatus> status =
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
    gCanceled = true;
    if (gExitCode == 0)
        gExitCode = EXITCODE_TIMEOUT;
    JS_TriggerOperationCallback(rt);

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

static JSBool
SetTimeoutValue(JSContext *cx, double t)
{
    /* NB: The next condition also filter out NaNs. */
    if (!(t <= MAX_TIMEOUT_INTERVAL)) {
        JS_ReportError(cx, "Excessive timeout value");
        return false;
    }
    gTimeoutInterval = t;
    if (!ScheduleWatchdog(cx->runtime, t)) {
        JS_ReportError(cx, "Failed to create the watchdog");
        return false;
    }
    return true;
}

static JSBool
Timeout(JSContext *cx, unsigned argc, jsval *vp)
{
    if (argc == 0)
        return JS_NewNumberValue(cx, gTimeoutInterval, vp);

    if (argc > 1) {
        JS_ReportError(cx, "Wrong number of arguments");
        return false;
    }

    double t;
    if (!JS_ValueToNumber(cx, JS_ARGV(cx, vp)[0], &t))
        return false;

    *vp = JSVAL_VOID;
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
        return JS_NewNumberValue(cx, d, vp);
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
    *vp = OBJECT_TO_JSVAL(parent);

    /* Outerize if necessary.  Embrace the ugliness! */
    if (parent) {
        if (JSObjectOp op = parent->getClass()->ext.outerObject)
            *vp = OBJECT_TO_JSVAL(op(cx, parent));
    }

    return true;
}

#ifdef XP_UNIX

#include <fcntl.h>
#include <sys/stat.h>

/*
 * Returns a JS_malloc'd string (that the caller needs to JS_free)
 * containing the directory (non-leaf) part of |from| prepended to |leaf|.
 * If |from| is empty or a leaf, MakeAbsolutePathname returns a copy of leaf.
 * Returns NULL to indicate an error.
 */
static char *
MakeAbsolutePathname(JSContext *cx, const char *from, const char *leaf)
{
    size_t dirlen;
    char *dir;
    const char *slash = NULL, *cp;

    if (*leaf == '/') {
        /* We were given an absolute pathname. */
        return JS_strdup(cx, leaf);
    }

    cp = from;
    while (*cp) {
        if (*cp == '/') {
            slash = cp;
        }

        ++cp;
    }

    if (!slash) {
        /* We were given a leaf or |from| was empty. */
        return JS_strdup(cx, leaf);
    }

    /* Else, we were given a real pathname, return that + the leaf. */
    dirlen = slash - from + 1;
    dir = (char*) JS_malloc(cx, dirlen + strlen(leaf) + 1);
    if (!dir)
        return NULL;

    strncpy(dir, from, dirlen);
    strcpy(dir + dirlen, leaf); /* Note: we can't use strcat here. */

    return dir;
}

#endif // XP_UNIX

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

    JSObject *global = JS_GetGlobalForScopeChain(cx);
    JSString *scriptContents = JSVAL_TO_STRING(arg0);
    unsigned oldopts = JS_GetOptions(cx);
    JS_SetOptions(cx, oldopts | JSOPTION_COMPILE_N_GO | JSOPTION_NO_SCRIPT_RVAL);
    bool ok = JS_CompileUCScript(cx, global, JS_GetStringCharsZ(cx, scriptContents),
                                 JS_GetStringLength(scriptContents), "<string>", 1);
    JS_SetOptions(cx, oldopts);

    JS_SET_RVAL(cx, vp, JSVAL_VOID);
    return ok;
}

static JSBool
Parse(JSContext *cx, unsigned argc, jsval *vp)
{
    if (argc < 1) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_MORE_ARGS_NEEDED,
                             "compile", "0", "s");
        return false;
    }
    jsval arg0 = JS_ARGV(cx, vp)[0];
    if (!JSVAL_IS_STRING(arg0)) {
        const char *typeName = JS_GetTypeName(cx, JS_TypeOfValue(cx, arg0));
        JS_ReportError(cx, "expected string to parse, got %s", typeName);
        return false;
    }

    JSString *scriptContents = JSVAL_TO_STRING(arg0);
    js::Parser parser(cx, /* prin = */ NULL, /* originPrin = */ NULL,
                      JS_GetStringCharsZ(cx, scriptContents), JS_GetStringLength(scriptContents),
                      "<string>", /* lineno = */ 1, cx->findVersion(),
                      /* foldConstants = */ true, /* compileAndGo = */ false);
    if (!parser.init())
        return false;

    ParseNode *pn = parser.parse(NULL);
    if (!pn)
        return false;
#ifdef DEBUG
    DumpParseTree(pn);
#endif
    JS_SET_RVAL(cx, vp, JSVAL_VOID);
    return true;
}

struct FreeOnReturn {
    JSContext *cx;
    const char *ptr;
    JS_DECL_USE_GUARD_OBJECT_NOTIFIER

    FreeOnReturn(JSContext *cx, const char *ptr = NULL JS_GUARD_OBJECT_NOTIFIER_PARAM)
      : cx(cx), ptr(ptr) {
        JS_GUARD_OBJECT_NOTIFIER_INIT;
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
Snarf(JSContext *cx, unsigned argc, jsval *vp)
{
    JSString *str;

    if (!argc)
        return false;

    str = JS_ValueToString(cx, JS_ARGV(cx, vp)[0]);
    if (!str)
        return false;
    JSAutoByteString filename(cx, str);
    if (!filename)
        return false;

    /* Get the currently executing script's name. */
    JSScript *script = GetTopScript(cx);
    JS_ASSERT(script->filename);
    const char *pathname = filename.ptr();
#ifdef XP_UNIX
    FreeOnReturn pnGuard(cx);
    if (pathname[0] != '/') {
        pathname = MakeAbsolutePathname(cx, script->filename, pathname);
        if (!pathname)
            return false;
        pnGuard.init(pathname);
    }
#endif

    if (argc > 1) {
        JSString *opt = JS_ValueToString(cx, JS_ARGV(cx, vp)[1]);
        if (!opt)
            return false;
        JSBool match;
        if (!JS_StringEqualsAscii(cx, opt, "binary", &match))
            return false;
        if (match) {
            JSObject *obj;
            if (!(obj = FileAsTypedArray(cx, pathname)))
                return false;
            *vp = OBJECT_TO_JSVAL(obj);
            return true;
        }
    }

    if (!(str = FileAsString(cx, pathname)))
        return false;
    *vp = STRING_TO_JSVAL(str);
    return true;
}

static JSBool
Wrap(JSContext *cx, unsigned argc, jsval *vp)
{
    jsval v = argc > 0 ? JS_ARGV(cx, vp)[0] : JSVAL_VOID;
    if (JSVAL_IS_PRIMITIVE(v)) {
        JS_SET_RVAL(cx, vp, v);
        return true;
    }

    JSObject *obj = JSVAL_TO_OBJECT(v);
    JSObject *wrapped = Wrapper::New(cx, obj, obj->getProto(), &obj->global(),
                                     &DirectWrapper::singleton);
    if (!wrapped)
        return false;

    JS_SET_RVAL(cx, vp, OBJECT_TO_JSVAL(wrapped));
    return true;
}

static JSBool
Serialize(JSContext *cx, unsigned argc, jsval *vp)
{
    jsval v = argc > 0 ? JS_ARGV(cx, vp)[0] : JSVAL_VOID;
    uint64_t *datap;
    size_t nbytes;
    if (!JS_WriteStructuredClone(cx, v, &datap, &nbytes, NULL, NULL))
        return false;

    JSObject *array = JS_NewUint8Array(cx, nbytes);
    if (!array) {
        JS_free(cx, datap);
        return false;
    }
    JS_ASSERT((uintptr_t(TypedArray::viewData(array)) & 7) == 0);
    js_memcpy(TypedArray::viewData(array), datap, nbytes);
    JS_free(cx, datap);
    JS_SET_RVAL(cx, vp, OBJECT_TO_JSVAL(array));
    return true;
}

static JSBool
Deserialize(JSContext *cx, unsigned argc, jsval *vp)
{
    Rooted<jsval> v(cx, argc > 0 ? JS_ARGV(cx, vp)[0] : JSVAL_VOID);
    JSObject *obj;
    if (JSVAL_IS_PRIMITIVE(v) || !(obj = JSVAL_TO_OBJECT(v))->isTypedArray()) {
        JS_ReportErrorNumber(cx, my_GetErrorMessage, NULL, JSSMSG_INVALID_ARGS, "deserialize");
        return false;
    }
    if ((TypedArray::byteLength(obj) & 7) != 0) {
        JS_ReportErrorNumber(cx, my_GetErrorMessage, NULL, JSSMSG_INVALID_ARGS, "deserialize");
        return false;
    }
    if ((uintptr_t(TypedArray::viewData(obj)) & 7) != 0) {
        JS_ReportErrorNumber(cx, my_GetErrorMessage, NULL, JSSMSG_BAD_ALIGNMENT);
        return false;
    }

    if (!JS_ReadStructuredClone(cx, (uint64_t *) TypedArray::viewData(obj), TypedArray::byteLength(obj),
                                JS_STRUCTURED_CLONE_VERSION, v.address(), NULL, NULL)) {
        return false;
    }
    JS_SET_RVAL(cx, vp, v);
    return true;
}

static JSObject *
NewGlobalObject(JSContext *cx);

static JSBool
NewGlobal(JSContext *cx, unsigned argc, jsval *vp)
{
    JSObject *global = NewGlobalObject(cx);
    if (!global)
        return false;

    JS_SET_RVAL(cx, vp, OBJECT_TO_JSVAL(global));
    return true;
}

static JSBool
ParseLegacyJSON(JSContext *cx, unsigned argc, jsval *vp)
{
    if (argc != 1 || !JSVAL_IS_STRING(JS_ARGV(cx, vp)[0])) {
        JS_ReportErrorNumber(cx, my_GetErrorMessage, NULL, JSSMSG_INVALID_ARGS, "parseLegacyJSON");
        return false;
    }

    JSString *str = JSVAL_TO_STRING(JS_ARGV(cx, vp)[0]);

    size_t length;
    const jschar *chars = JS_GetStringCharsAndLength(cx, str, &length);
    if (!chars)
        return false;
    return js::ParseJSONWithReviver(cx, chars, length, js::NullValue(), vp, LEGACY);
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

    JS_SET_RVAL(cx, vp, JSVAL_VOID);
    return true;
}

static JSBool
EnableSPSProfilingAssertions(JSContext *cx, unsigned argc, jsval *vp)
{
    jsval arg = JS_ARGV(cx, vp)[0];
    if (argc == 0 || !JSVAL_IS_BOOLEAN(arg)) {
        JS_ReportErrorNumber(cx, my_GetErrorMessage, NULL, JSSMSG_INVALID_ARGS,
                             "enableSPSProfilingAssertions");
        return false;
    }

    static ProfileEntry stack[1000];
    static uint32_t stack_size = 0;

    if (JSVAL_TO_BOOLEAN(arg))
        SetRuntimeProfilingStack(cx->runtime, stack, &stack_size, 1000);
    else
        SetRuntimeProfilingStack(cx->runtime, NULL, NULL, 0);
    cx->runtime->spsProfiler.enableSlowAssertions(JSVAL_TO_BOOLEAN(arg));

    JS_SET_RVAL(cx, vp, JSVAL_VOID);
    return true;
}

static JSBool
GetMaxArgs(JSContext *cx, unsigned arg, jsval *vp)
{
    JS_SET_RVAL(cx, vp, INT_TO_JSVAL(StackSpace::ARGS_LENGTH_MAX));
    return true;
}

static JSFunctionSpecWithHelp shell_functions[] = {
    JS_FN_HELP("version", Version, 0, 0,
"version([number])",
"  Get or force a script compilation version number."),

    JS_FN_HELP("revertVersion", RevertVersion, 0, 0,
"revertVersion()",
"  Revert previously set version number."),

    JS_FN_HELP("options", Options, 0, 0,
"options([option ...])",
"  Get or toggle JavaScript options."),

    JS_FN_HELP("load", Load, 1, 0,
"load(['foo.js' ...])",
"  Load files named by string arguments."),

    JS_FN_HELP("evaluate", Evaluate, 2, 0,
"evaluate(code[, options])",
"  Evaluate code as though it were the contents of a file.\n"
"  options is an optional object that may have these properties:\n"
"      compileAndGo: use the compile-and-go compiler option (default: true)\n"
"      noScriptRval: use the no-script-rval compiler option (default: false)\n"
"      fileName: filename for error messages and debug info\n"
"      lineNumber: starting line number for error messages and debug info\n"
"      global: global in which to execute the code\n"
"      newContext: if true, create and use a new cx (default: false)\n"),

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

    JS_FN_HELP("assertJit", AssertJit, 0, 0,
"assertJit()",
"  Throw if the calling function failed to JIT."),

    JS_FN_HELP("setDebug", SetDebug, 1, 0,
"setDebug(debug)",
"  Set debug mode."),

    JS_FN_HELP("setDebuggerHandler", SetDebuggerHandler, 1, 0,
"setDebuggerHandler(f)",
"  Set handler for debugger keyword to f."),

    JS_FN_HELP("setThrowHook", SetThrowHook, 1, 0,
"setThrowHook(f)",
"  Set throw hook to f."),

    JS_FN_HELP("trap", Trap, 3, 0,
"trap([fun, [pc,]] exp)",
"  Trap bytecode execution."),

    JS_FN_HELP("untrap", Untrap, 2, 0,
"untrap(fun[, pc])",
"  Remove a trap."),

    JS_FN_HELP("line2pc", LineToPC, 0, 0,
"line2pc([fun,] line)",
"  Map line number to PC."),

    JS_FN_HELP("pc2line", PCToLine, 0, 0,
"pc2line(fun[, pc])",
"  Map PC to line number."),

    JS_FN_HELP("stringsAreUTF8", StringsAreUTF8, 0, 0,
"stringsAreUTF8()",
"  Check if strings are UTF-8 encoded."),

    JS_FN_HELP("testUTF8", TestUTF8, 1, 0,
"testUTF8(mode)",
"  Perform UTF-8 tests (modes are 1 to 4)."),

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
#ifdef TEST_CVTARGS
    JS_FN_HELP("cvtargs", ConvertArgs, 0, 0,
"cvtargs(arg1..., arg12)",
"  Test argument formatter."),

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
    JS_FN_HELP("snarf", Snarf, 0, 0,
"snarf(filename)",
"  Read filename into returned string."),

    JS_FN_HELP("read", Snarf, 0, 0,
"read(filename)",
"  Synonym for snarf."),

    JS_FN_HELP("compile", Compile, 1, 0,
"compile(code)",
"  Compiles a string to bytecode, potentially throwing."),

    JS_FN_HELP("parse", Parse, 1, 0,
"parse(code)",
"  Parses a string, potentially throwing."),

    JS_FN_HELP("timeout", Timeout, 1, 0,
"timeout([seconds])",
"  Get/Set the limit in seconds for the execution time for the current context.\n"
"  A negative value (default) means that the execution time is unlimited."),

    JS_FN_HELP("elapsed", Elapsed, 0, 0,
"elapsed()",
"  Execution time elapsed for the current context."),

    JS_FN_HELP("parent", Parent, 1, 0,
"parent(obj)",
"  Returns the parent of obj."),

    JS_FN_HELP("wrap", Wrap, 1, 0,
"wrap(obj)",
"  Wrap an object into a noop wrapper."),

    JS_FN_HELP("serialize", Serialize, 1, 0,
"serialize(sd)",
"  Serialize sd using JS_WriteStructuredClone. Returns a TypedArray."),

    JS_FN_HELP("deserialize", Deserialize, 1, 0,
"deserialize(a)",
"  Deserialize data generated by serialize."),

    JS_FN_HELP("newGlobal", NewGlobal, 1, 0,
"newGlobal(kind)",
"  Return a new global object, in the current\n"
"  compartment if kind === 'same-compartment' or in a\n"
"  new compartment if kind === 'new-compartment'."),

    JS_FN_HELP("parseLegacyJSON", ParseLegacyJSON, 1, 0,
"parseLegacyJSON(str)",
"  Parse str as legacy JSON, returning the result if the\n"
"  parse succeeded and throwing a SyntaxError if not."),

    JS_FN_HELP("enableStackWalkingAssertion", EnableStackWalkingAssertion, 1, 0,
"enableStackWalkingAssertion(enabled)",
"  Enables or disables a particularly expensive assertion in stack-walking\n"
"  code.  If your test isn't ridiculously thorough, such that performing this\n"
"  assertion increases test duration by an order of magnitude, you shouldn't\n"
"  use this."),

    JS_FN_HELP("getMaxArgs", GetMaxArgs, 0, 0,
"getMaxArgs()",
"  Return the maximum number of supported args for a call."),

    JS_FN_HELP("enableSPSProfilingAssertions", EnableSPSProfilingAssertions, 1, 0,
"enableProfilingAssertions(enabled)",
"  Enables or disables the assertions related to SPS profiling. This is fairly\n"
"  expensive, so it shouldn't be enabled normally."),

    JS_FS_END
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
PrintHelp(JSContext *cx, JSObject *obj)
{
    jsval usage, help;
    if (!JS_LookupProperty(cx, obj, "usage", &usage))
        return false;
    if (!JS_LookupProperty(cx, obj, "help", &help))
        return false;

    if (JSVAL_IS_VOID(usage) || JSVAL_IS_VOID(help))
        return true;

    return PrintHelpString(cx, usage) && PrintHelpString(cx, help);
}

static JSBool
Help(JSContext *cx, unsigned argc, jsval *vp)
{
    fprintf(gOutFile, "%s\n", JS_GetImplementationVersion());

    if (argc == 0) {
        JSObject *global = JS_GetGlobalObject(cx);
        AutoIdArray ida(cx, JS_Enumerate(cx, global));
        if (!ida)
            return false;

        for (size_t i = 0; i < ida.length(); i++) {
            jsval v;
            if (!JS_LookupPropertyById(cx, global, ida[i], &v))
                return false;
            if (!JSVAL_IS_PRIMITIVE(v) && !PrintHelp(cx, JSVAL_TO_OBJECT(v)))
                return false;
        }
    } else {
        jsval *argv = JS_ARGV(cx, vp);
        for (unsigned i = 0; i < argc; i++) {
            if (!JSVAL_IS_PRIMITIVE(argv[i]) && !PrintHelp(cx, JSVAL_TO_OBJECT(argv[i])))
                return false;
        }
    }

    JS_SET_RVAL(cx, vp, JSVAL_VOID);
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
its_getter(JSContext *cx, HandleObject obj, HandleId id, jsval *vp);

static JSBool
its_setter(JSContext *cx, HandleObject obj, HandleId id, JSBool strict, jsval *vp);

static JSBool
its_get_customNative(JSContext *cx, unsigned argc, jsval *vp);

static JSBool
its_set_customNative(JSContext *cx, unsigned argc, jsval *vp);

static JSPropertySpec its_props[] = {
    {"color",           ITS_COLOR,      JSPROP_ENUMERATE,       NULL, NULL},
    {"height",          ITS_HEIGHT,     JSPROP_ENUMERATE,       NULL, NULL},
    {"width",           ITS_WIDTH,      JSPROP_ENUMERATE,       NULL, NULL},
    {"funny",           ITS_FUNNY,      JSPROP_ENUMERATE,       NULL, NULL},
    {"array",           ITS_ARRAY,      JSPROP_ENUMERATE,       NULL, NULL},
    {"rdonly",          ITS_RDONLY,     JSPROP_READONLY,        NULL, NULL},
    {"custom",          ITS_CUSTOM,     JSPROP_ENUMERATE,
                        its_getter,     its_setter},
    {"customRdOnly",    ITS_CUSTOMRDONLY, JSPROP_ENUMERATE | JSPROP_READONLY,
                        its_getter,     its_setter},
    {"customNative",    ITS_CUSTOMNATIVE,
                        JSPROP_ENUMERATE | JSPROP_NATIVE_ACCESSORS,
                        (JSPropertyOp)its_get_customNative,
                        (JSStrictPropertyOp)its_set_customNative },
    {NULL,0,0,NULL,NULL}
};

static JSBool its_noisy;    /* whether to be noisy when finalizing it */
static JSBool its_enum_fail;/* whether to fail when enumerating it */

static JSBool
its_addProperty(JSContext *cx, HandleObject obj, HandleId id, jsval *vp)
{
    if (!its_noisy)
        return true;

    IdStringifier idString(cx, id);
    fprintf(gOutFile, "adding its property %s,", idString.getBytes());
    ToStringHelper valueString(cx, *vp);
    fprintf(gOutFile, " initial value %s\n", valueString.getBytes());
    return true;
}

static JSBool
its_delProperty(JSContext *cx, HandleObject obj, HandleId id, jsval *vp)
{
    if (!its_noisy)
        return true;

    IdStringifier idString(cx, id);
    fprintf(gOutFile, "deleting its property %s,", idString.getBytes());
    ToStringHelper valueString(cx, *vp);
    fprintf(gOutFile, " initial value %s\n", valueString.getBytes());
    return true;
}

static JSBool
its_getProperty(JSContext *cx, HandleObject obj, HandleId id, jsval *vp)
{
    if (!its_noisy)
        return true;

    IdStringifier idString(cx, id);
    fprintf(gOutFile, "getting its property %s,", idString.getBytes());
    ToStringHelper valueString(cx, *vp);
    fprintf(gOutFile, " initial value %s\n", valueString.getBytes());
    return true;
}

static JSBool
its_setProperty(JSContext *cx, HandleObject obj, HandleId id, JSBool strict, jsval *vp)
{
    IdStringifier idString(cx, id);
    if (its_noisy) {
        fprintf(gOutFile, "setting its property %s,", idString.getBytes());
        ToStringHelper valueString(cx, *vp);
        fprintf(gOutFile, " new value %s\n", valueString.getBytes());
    }

    if (!JSID_IS_ATOM(id))
        return true;

    if (!strcmp(idString.getBytes(), "noisy"))
        JS_ValueToBoolean(cx, *vp, &its_noisy);
    else if (!strcmp(idString.getBytes(), "enum_fail"))
        JS_ValueToBoolean(cx, *vp, &its_enum_fail);

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
    JSObject *iterator;

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
        *statep = JSVAL_NULL;
        break;
    }

    return true;
}

static JSBool
its_resolve(JSContext *cx, HandleObject obj, HandleId id, unsigned flags,
            MutableHandleObject objp)
{
    if (its_noisy) {
        IdStringifier idString(cx, id);
        fprintf(gOutFile, "resolving its property %s, flags {%s,%s,%s}\n",
               idString.getBytes(),
               (flags & JSRESOLVE_QUALIFIED) ? "qualified" : "",
               (flags & JSRESOLVE_ASSIGNING) ? "assigning" : "",
               (flags & JSRESOLVE_DETECTING) ? "detecting" : "");
    }
    return true;
}

static JSBool
its_convert(JSContext *cx, HandleObject obj, JSType type, jsval *vp)
{
    if (its_noisy)
        fprintf(gOutFile, "converting it to %s type\n", JS_GetTypeName(cx, type));
    return JS_ConvertStub(cx, obj, type, vp);
}

static void
its_finalize(JSFreeOp *fop, JSObject *obj)
{
    jsval *rootedVal;
    if (its_noisy)
        fprintf(gOutFile, "finalizing it\n");
    rootedVal = (jsval *) JS_GetPrivate(obj);
    if (rootedVal) {
        JS_RemoveValueRootRT(fop->runtime(), rootedVal);
        JS_SetPrivate(obj, NULL);
        delete rootedVal;
    }
}

static JSClass its_class = {
    "It", JSCLASS_NEW_RESOLVE | JSCLASS_NEW_ENUMERATE | JSCLASS_HAS_PRIVATE,
    its_addProperty,  its_delProperty,  its_getProperty,  its_setProperty,
    (JSEnumerateOp)its_enumerate, (JSResolveOp)its_resolve,
    its_convert,      its_finalize
};

static JSBool
its_getter(JSContext *cx, HandleObject obj, HandleId id, jsval *vp)
{
    if (JS_GetClass(obj) == &its_class) {
        jsval *val = (jsval *) JS_GetPrivate(obj);
        *vp = val ? *val : JSVAL_VOID;
    } else {
        *vp = JSVAL_VOID;
    }

    return true;
}

static JSBool
its_setter(JSContext *cx, HandleObject obj, HandleId id, JSBool strict, jsval *vp)
{
    if (JS_GetClass(obj) != &its_class)
        return true;

    jsval *val = (jsval *) JS_GetPrivate(obj);
    if (val) {
        *val = *vp;
        return true;
    }

    val = new jsval;
    if (!val) {
        JS_ReportOutOfMemory(cx);
        return false;
    }

    if (!JS_AddValueRoot(cx, val)) {
        delete val;
        return false;
    }

    JS_SetPrivate(obj, (void*)val);

    *val = *vp;
    return true;
}

static JSBool
its_get_customNative(JSContext *cx, unsigned argc, jsval *vp)
{
    JSObject *obj = JS_THIS_OBJECT(cx, vp);
    if (!obj)
        return false;

    if (JS_GetClass(obj) == &its_class) {
        jsval *val = (jsval *) JS_GetPrivate(obj);
        *vp = val ? *val : JSVAL_VOID;
    } else {
        *vp = JSVAL_VOID;
    }

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

    jsval *argv = JS_ARGV(cx, vp);

    jsval *val = (jsval *) JS_GetPrivate(obj);
    if (val) {
        *val = *argv;
        return true;
    }

    val = new jsval;
    if (!val) {
        JS_ReportOutOfMemory(cx);
        return false;
    }

    if (!JS_AddValueRoot(cx, val)) {
        delete val;
        return false;
    }

    JS_SetPrivate(obj, (void *)val);

    *val = *argv;
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
    int i, j, k, n;
    char *prefix, *tmp;
    const char *ctmp;

    if (!report) {
        fprintf(gErrFile, "%s\n", message);
        fflush(gErrFile);
        return;
    }

    /* Conditionally ignore reported warnings. */
    if (JSREPORT_IS_WARNING(report->flags) && !reportWarnings)
        return;

    gGotError = true;

    prefix = NULL;
    if (report->filename)
        prefix = JS_smprintf("%s:", report->filename);
    if (report->lineno) {
        tmp = prefix;
        prefix = JS_smprintf("%s%u: ", tmp ? tmp : "", report->lineno);
        JS_free(cx, tmp);
    }
    if (JSREPORT_IS_WARNING(report->flags)) {
        tmp = prefix;
        prefix = JS_smprintf("%s%swarning: ",
                             tmp ? tmp : "",
                             JSREPORT_IS_STRICT(report->flags) ? "strict " : "");
        JS_free(cx, tmp);
    }

    /* embedded newlines -- argh! */
    while ((ctmp = strchr(message, '\n')) != 0) {
        ctmp++;
        if (prefix)
            fputs(prefix, gErrFile);
        fwrite(message, 1, ctmp - message, gErrFile);
        message = ctmp;
    }

    /* If there were no filename or lineno, the prefix might be empty */
    if (prefix)
        fputs(prefix, gErrFile);
    fputs(message, gErrFile);

    if (!report->linebuf) {
        fputc('\n', gErrFile);
        goto out;
    }

    /* report->linebuf usually ends with a newline. */
    n = strlen(report->linebuf);
    fprintf(gErrFile, ":\n%s%s%s%s",
            prefix,
            report->linebuf,
            (n > 0 && report->linebuf[n-1] == '\n') ? "" : "\n",
            prefix);
    n = report->tokenptr - report->linebuf;
    for (i = j = 0; i < n; i++) {
        if (report->linebuf[i] == '\t') {
            for (k = (j + 8) & ~7; j < k; j++) {
                fputc('.', gErrFile);
            }
            continue;
        }
        fputc('.', gErrFile);
        j++;
    }
    fputs("^\n", gErrFile);
 out:
    fflush(gErrFile);
    if (!JSREPORT_IS_WARNING(report->flags)) {
        if (report->errorNumber == JSMSG_OUT_OF_MEMORY) {
            gExitCode = EXITCODE_OUT_OF_MEMORY;
        } else {
            gExitCode = EXITCODE_RUNTIME_ERROR;
        }
    }
    JS_free(cx, prefix);
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

    JS_SET_RVAL(cx, vp, JSVAL_VOID);

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
        nargv[i] = JS_EncodeString(cx, str);
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
    if (!(flags & JSRESOLVE_QUALIFIED)) {
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
    }
#else
    return true;
#endif
}

JSClass global_class = {
    "global", JSCLASS_NEW_RESOLVE | JSCLASS_GLOBAL_FLAGS,
    JS_PropertyStub,  JS_PropertyStub,
    JS_PropertyStub,  JS_StrictPropertyStub,
    global_enumerate, (JSResolveOp) global_resolve,
    JS_ConvertStub,   NULL
};

static JSBool
env_setProperty(JSContext *cx, HandleObject obj, HandleId id, JSBool strict, jsval *vp)
{
/* XXX porting may be easy, but these don't seem to supply setenv by default */
#if !defined XP_OS2 && !defined SOLARIS
    int rv;

    IdStringifier idstr(cx, id, true);
    if (idstr.threw())
        return false;
    ToStringHelper valstr(cx, *vp, true);
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
    *vp = valstr.getJSVal();
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

    if (flags & JSRESOLVE_ASSIGNING)
        return true;

    IdStringifier idstr(cx, id, true);
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
    JS_PropertyStub,  JS_PropertyStub,
    JS_PropertyStub,  env_setProperty,
    env_enumerate, (JSResolveOp) env_resolve,
    JS_ConvertStub
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
        return NULL;

    JSShellContextData *data = NewContextData();
    if (!data) {
        DestroyContext(cx, false);
        return NULL;
    }

    JS_SetContextPrivate(cx, data);
    JS_SetErrorReporter(cx, my_ErrorReporter);
    JS_SetVersion(cx, JSVERSION_LATEST);
    SetContextOptions(cx);
    if (enableMethodJit)
        JS_ToggleOptions(cx, JSOPTION_METHODJIT);
    if (enableTypeInference)
        JS_ToggleOptions(cx, JSOPTION_TYPE_INFERENCE);
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
NewGlobalObject(JSContext *cx)
{
    RootedObject glob(cx, JS_NewGlobalObject(cx, &global_class, NULL));
    if (!glob)
        return NULL;

    {
        JSAutoEnterCompartment ac;
        if (!ac.enter(cx, glob))
            return NULL;

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
            !JS_DefineProfilingFunctions(cx, glob)) {
            return NULL;
        }
        if (!js::DefineTestingFunctions(cx, glob))
            return NULL;

        JSObject *it = JS_DefineObject(cx, glob, "it", &its_class, NULL, 0);
        if (!it)
            return NULL;
        if (!JS_DefineProperties(cx, it, its_props))
            return NULL;

        if (!JS_DefineProperty(cx, glob, "custom", JSVAL_VOID, its_getter,
                               its_setter, 0))
            return NULL;
        if (!JS_DefineProperty(cx, glob, "customRdOnly", JSVAL_VOID, its_getter,
                               its_setter, JSPROP_READONLY))
            return NULL;
    }

    if (!JS_WrapObject(cx, glob.address()))
        return NULL;

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

    /*
     * Script arguments are bound as a normal |arguments| property on the
     * global object. It has no special significance, like |arguments| in
     * function scope does -- this identifier is used de-facto across shell
     * implementations, see bug 675269.
     */
    if (!JS_DefineProperty(cx, obj, "arguments", OBJECT_TO_JSVAL(scriptArgs), NULL, NULL, 0))
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

static int
ProcessArgs(JSContext *cx, JSObject *obj_, OptionParser *op)
{
    RootedObject obj(cx, obj_);

    if (op->getBoolOption('a'))
        JS_ToggleOptions(cx, JSOPTION_METHODJIT_ALWAYS);

    if (op->getBoolOption('c'))
        compileOnly = true;

    if (op->getBoolOption('m')) {
        enableMethodJit = true;
        JS_ToggleOptions(cx, JSOPTION_METHODJIT);
    }

    if (op->getBoolOption('d')) {
        JS_SetRuntimeDebugMode(JS_GetRuntime(cx), true);
        JS_SetDebugMode(cx, true);
    }

    if (op->getBoolOption('b'))
        printTiming = true;

    if (op->getBoolOption('D'))
        enableDisassemblyDumps = true;

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
            jsval rval;
            if (!JS_EvaluateScript(cx, obj, code, strlen(code), "-e", 1, &rval))
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
     * First check to see if type inference is enabled. This flag must be set
     * on the compartment when it is constructed.
     */
    if (op->getBoolOption('n')) {
        enableTypeInference = !enableTypeInference;
        JS_ToggleOptions(cx, JSOPTION_TYPE_INFERENCE);
    }

    RootedObject glob(cx);
    glob = NewGlobalObject(cx);
    if (!glob)
        return 1;

    JSAutoEnterCompartment ac;
    if (!ac.enter(cx, glob))
        return 1;

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
CheckObjectAccess(JSContext *cx, HandleObject obj, HandleId id, JSAccessMode mode, jsval *vp)
{
    return true;
}

JSSecurityCallbacks securityCallbacks = {
    CheckObjectAccess,
    NULL,
    NULL,
    NULL
};

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
        || !op.addBoolOption('m', "methodjit", "Enable the JaegerMonkey method JIT")
        || !op.addBoolOption('n', "typeinfer", "Enable type inference")
        || !op.addBoolOption('c', "compileonly", "Only compile, don't run (syntax checking mode)")
        || !op.addBoolOption('d', "debugjit", "Enable runtime debug mode for method JIT code")
        || !op.addBoolOption('a', "always-mjit",
                             "Do not try to run in the interpreter before method jitting.")
        || !op.addBoolOption('D', "dump-bytecode", "Dump bytecode with exec count for all scripts")
        || !op.addBoolOption('b', "print-timing", "Print sub-ms runtime for each file that's run")
#ifdef DEBUG
        || !op.addIntOption('A', "oom-after", "COUNT", "Trigger OOM after COUNT allocations", -1)
        || !op.addBoolOption('O', "print-alloc", "Print the number of allocations at exit")
#endif
        || !op.addBoolOption('U', "utf8", "C strings passed to the JSAPI are UTF-8 encoded")
        || !op.addOptionalStringArg("script", "A script to execute (after all options)")
        || !op.addOptionalMultiStringArg("scriptArgs",
                                         "String arguments to bind as |arguments| in the "
                                         "shell's global")) {
        return EXIT_FAILURE;
    }

    op.setArgTerminatesOptions("script", true);

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
    if (op.getIntOption('A') >= 0)
        OOM_maxAllocations = op.getIntOption('A');
    if (op.getBoolOption('O'))
        OOM_printAllocationCount = true;
#endif

    /* Must be done before we create the JSRuntime. */
    if (op.getBoolOption('U'))
        JS_SetCStringsAreUTF8();

#ifdef XP_WIN
    // Set the timer calibration delay count to 0 so we get high
    // resolution right away, which we need for precise benchmarking.
    extern int CALIBRATION_DELAY_COUNT;
    CALIBRATION_DELAY_COUNT = 0;
#endif

    /* Use the same parameters as the browser in xpcjsruntime.cpp. */
    rt = JS_NewRuntime(32L * 1024L * 1024L);
    if (!rt)
        return 1;

    JS_SetGCParameter(rt, JSGC_MAX_BYTES, 0xffffffff);

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
#ifdef JS_GC_ZEAL
    JS_SetGCZeal(cx, 0, 0);
#endif

    /* Must be done before creating the global object */
    if (op.getBoolOption('D'))
        JS_ToggleOptions(cx, JSOPTION_PCCOUNT);

    result = Shell(cx, &op, envp);

#ifdef DEBUG
    if (OOM_printAllocationCount)
        printf("OOM max count: %u\n", OOM_counter);
#endif

    DestroyContext(cx, true);

    KillWatchdog();

    JS_DestroyRuntime(rt);
    JS_ShutDown();
    return result;
}
