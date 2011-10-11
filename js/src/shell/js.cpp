/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99:
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
#include "jsstdint.h"
#include "jsutil.h"
#include "jsprf.h"
#include "jswrapper.h"
#include "jsapi.h"
#include "jsarray.h"
#include "jsatom.h"
#include "jsbuiltins.h"
#include "jscntxt.h"
#include "jsdate.h"
#include "jsdbgapi.h"
#include "jsemit.h"
#include "jsfun.h"
#include "jsgc.h"
#include "jsiter.h"
#include "jslock.h"
#include "jsnum.h"
#include "jsobj.h"
#include "json.h"
#include "jsparse.h"
#include "jsreflect.h"
#include "jsscope.h"
#include "jsscript.h"
#include "jstracer.h"
#include "jstypedarray.h"
#include "jstypedarrayinlines.h"
#include "jsxml.h"
#include "jsperf.h"

#include "prmjtime.h"

#ifdef JSDEBUGGER
#include "jsdebug.h"
#ifdef JSDEBUGGER_JAVA_UI
#include "jsdjava.h"
#endif /* JSDEBUGGER_JAVA_UI */
#ifdef JSDEBUGGER_C_UI
#include "jsdb.h"
#endif /* JSDEBUGGER_C_UI */
#endif /* JSDEBUGGER */

#include "jsoptparse.h"
#include "jsworkers.h"
#include "jsheaptools.h"

#include "jsinferinlines.h"
#include "jsinterpinlines.h"
#include "jsobjinlines.h"
#include "jsscriptinlines.h"
#include "methodjit/MethodJIT.h"

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
static jsuword gStackBase;
#endif

/*
 * Limit the timeout to 30 minutes to prevent an overflow on platfoms
 * that represent the time internally in microseconds using 32-bit int.
 */
static jsdouble MAX_TIMEOUT_INTERVAL = 1800.0;
static jsdouble gTimeoutInterval = -1.0;
static volatile bool gCanceled = false;

static bool enableTraceJit = false;
static bool enableMethodJit = false;
static bool enableProfiling = false;
static bool enableTypeInference = false;
static bool enableDisassemblyDumps = false;

static bool printTiming = false;

static JSBool
SetTimeoutValue(JSContext *cx, jsdouble t);

static bool
InitWatchdog(JSRuntime *rt);

static void
KillWatchdog();

static bool
ScheduleWatchdog(JSRuntime *rt, jsdouble t);

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
static PRIntervalTime gWatchdogTimeout = 0;

static PRCondVar *gSleepWakeup = NULL;

#else

static JSRuntime *gRuntime = NULL;

#endif

int gExitCode = 0;
JSBool gQuitting = JS_FALSE;
FILE *gErrFile = NULL;
FILE *gOutFile = NULL;
#ifdef JS_THREADSAFE
JSObject *gWorkers = NULL;
js::workers::ThreadPool *gWorkerThreadPool = NULL;
#endif

static JSBool reportWarnings = JS_TRUE;
static JSBool compileOnly = JS_FALSE;

#ifdef DEBUG
static JSBool OOM_printAllocationCount = JS_FALSE;
#endif

typedef enum JSShellErrNum {
#define MSG_DEF(name, number, count, exception, format) \
    name = number,
#include "jsshell.msg"
#undef MSG_DEF
    JSShellErr_Limit
#undef MSGDEF
} JSShellErrNum;

static JSContext *
NewContext(JSRuntime *rt);

static void
DestroyContext(JSContext *cx, bool withGC);

static const JSErrorFormatString *
my_GetErrorMessage(void *userRef, const char *locale, const uintN errorNumber);

#ifdef EDITLINE
JS_BEGIN_EXTERN_C
JS_EXTERN_API(char)    *readline(const char *prompt);
JS_EXTERN_API(void)     add_history(char *line);
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

class ToString {
  public:
    ToString(JSContext *aCx, jsval v, JSBool aThrow = JS_FALSE)
      : cx(aCx), mThrow(aThrow)
    {
        mStr = JS_ValueToString(cx, v);
        if (!aThrow && !mStr)
            ReportException(cx);
        JS_AddNamedStringRoot(cx, &mStr, "Value ToString helper");
    }
    ~ToString() {
        JS_RemoveStringRoot(cx, &mStr);
    }
    JSBool threw() { return !mStr; }
    jsval getJSVal() { return STRING_TO_JSVAL(mStr); }
    const char *getBytes() {
        if (mStr && (mBytes.ptr() || mBytes.encode(cx, mStr)))
            return mBytes.ptr();
        return "(error converting value)";
    }
  private:
    JSContext *cx;
    JSString *mStr;
    JSBool mThrow;
    JSAutoByteString mBytes;
};

class IdStringifier : public ToString {
public:
    IdStringifier(JSContext *cx, jsid id, JSBool aThrow = JS_FALSE)
    : ToString(cx, IdToJsval(id), aThrow)
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
    volatile JSIntervalTime startTime;
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
    data->startTime = js_IntervalNow();
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
        return JS_TRUE;

    JS_ClearPendingException(cx);
    return JS_FALSE;
}

static void
SetContextOptions(JSContext *cx)
{
    JS_SetNativeStackQuota(cx, gMaxStackSize);
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
Process(JSContext *cx, JSObject *obj, const char *filename, bool forceTTY)
{
    JSBool ok, hitEOF;
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
    uint32 oldopts;

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
         * It's not interactive - just execute it.
         *
         * Support the UNIX #! shell hack; gobble the first line if it starts
         * with '#'.  TODO - this isn't quite compatible with sharp variables,
         * as a legal js program (using sharp variables) might start with '#'.
         * But that would require multi-character lookahead.
         */
        int ch = fgetc(file);
        if (ch == '#') {
            while((ch = fgetc(file)) != EOF) {
                if (ch == '\n' || ch == '\r')
                    break;
            }
        }
        ungetc(ch, file);

        int64 t1 = PRMJ_Now();
        oldopts = JS_GetOptions(cx);
        JS_SetOptions(cx, oldopts | JSOPTION_COMPILE_N_GO | JSOPTION_NO_SCRIPT_RVAL);
        script = JS_CompileFileHandle(cx, obj, filename, file);
        JS_SetOptions(cx, oldopts);
        if (script && !compileOnly) {
            (void) JS_ExecuteScript(cx, obj, script, NULL);
            int64 t2 = PRMJ_Now() - t1;
            if (printTiming)
                printf("runtime = %.3f ms\n", double(t2) / PRMJ_USEC_PER_MSEC);
        }

        goto cleanup;
    }

    /* It's an interactive filehandle; drop into read-eval-print loop. */
    lineno = 1;
    hitEOF = JS_FALSE;
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
                hitEOF = JS_TRUE;
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
                hitEOF = JS_TRUE;
                break;
            }
        } while (!JS_BufferIsCompilableUnit(cx, JS_TRUE, obj, buffer, len));

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
        if (!compileOnly)
            JS_SetOptions(cx, oldopts | JSOPTION_COMPILE_N_GO);
        script = JS_CompileUCScript(cx, obj, uc_buffer, uc_len, "typein", startline);
        if (!compileOnly)
            JS_SetOptions(cx, oldopts);

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
    uint32      flag;
} js_options[] = {
    {"atline",          JSOPTION_ATLINE},
    {"jitprofiling",    JSOPTION_PROFILING},
    {"tracejit",        JSOPTION_JIT},
    {"methodjit",       JSOPTION_METHODJIT},
    {"methodjit_always",JSOPTION_METHODJIT_ALWAYS},
    {"relimit",         JSOPTION_RELIMIT},
    {"strict",          JSOPTION_STRICT},
    {"typeinfer",       JSOPTION_TYPE_INFERENCE},
    {"werror",          JSOPTION_WERROR},
    {"xml",             JSOPTION_XML},
};

static uint32
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

#if defined(JS_TRACER) && defined(DEBUG)
namespace js {
    extern struct JSClass jitstats_class;
    void InitJITStatsClass(JSContext *cx, JSObject *glob);
}
#endif

#ifdef JS_GC_ZEAL
static void
ParseZealArg(JSContext *cx, const char *arg)
{
    int zeal, freq = 1, compartment = 0;
    const char *p = strchr(arg, ',');

    zeal = atoi(arg);
    if (p) {
        freq = atoi(p + 1);
        p = strchr(p + 1, ',');
        if (p)
            compartment = atoi(p + 1);
    }

    JS_SetGCZeal(cx, (uint8)zeal, freq, !!compartment);
}
#endif

static JSBool
Version(JSContext *cx, uintN argc, jsval *vp)
{
    jsval *argv = JS_ARGV(cx, vp);
    if (argc == 0 || JSVAL_IS_VOID(argv[0])) {
        /* Get version. */
        *vp = INT_TO_JSVAL(JS_GetVersion(cx));
    } else {
        /* Set version. */
        int32 v = -1;
        if (JSVAL_IS_INT(argv[0])) {
            v = JSVAL_TO_INT(argv[0]);
        } else if (JSVAL_IS_DOUBLE(argv[0])) {
            jsdouble fv = JSVAL_TO_DOUBLE(argv[0]);
            if (int32(fv) == fv)
                v = int32(fv);
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
RevertVersion(JSContext *cx, uintN argc, jsval *vp)
{
    js_RevertVersion(cx);
    JS_SET_RVAL(cx, vp, JSVAL_VOID);
    return JS_TRUE;
}

static JSBool
Options(JSContext *cx, uintN argc, jsval *vp)
{
    uint32 optset, flag;
    JSString *str;
    char *names;
    JSBool found;

    optset = 0;
    jsval *argv = JS_ARGV(cx, vp);
    for (uintN i = 0; i < argc; i++) {
        str = JS_ValueToString(cx, argv[i]);
        if (!str)
            return JS_FALSE;
        argv[i] = STRING_TO_JSVAL(str);
        JSAutoByteString opt(cx, str);
        if (!opt)
            return JS_FALSE;
        flag = MapContextOptionNameToFlag(cx, opt.ptr());
        if (!flag)
            return JS_FALSE;
        optset |= flag;
    }
    optset = JS_ToggleOptions(cx, optset);

    names = NULL;
    found = JS_FALSE;
    for (size_t i = 0; i < ArrayLength(js_options); i++) {
        if (js_options[i].flag & optset) {
            found = JS_TRUE;
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
        return JS_FALSE;
    }
    str = JS_NewStringCopyZ(cx, names);
    free(names);
    if (!str)
        return JS_FALSE;
    *vp = STRING_TO_JSVAL(str);
    return JS_TRUE;
}

static JSBool
Load(JSContext *cx, uintN argc, jsval *vp)
{
    JSObject *thisobj = JS_THIS_OBJECT(cx, vp);
    if (!thisobj)
        return JS_FALSE;

    jsval *argv = JS_ARGV(cx, vp);
    for (uintN i = 0; i < argc; i++) {
        JSString *str = JS_ValueToString(cx, argv[i]);
        if (!str)
            return false;
        argv[i] = STRING_TO_JSVAL(str);
        JSAutoByteString filename(cx, str);
        if (!filename)
            return JS_FALSE;
        errno = 0;
        uint32 oldopts = JS_GetOptions(cx);
        JS_SetOptions(cx, oldopts | JSOPTION_COMPILE_N_GO | JSOPTION_NO_SCRIPT_RVAL);
        JSScript *script = JS_CompileFile(cx, thisobj, filename.ptr());
        JS_SetOptions(cx, oldopts);
        if (!script)
            return false;

        if (!compileOnly && !JS_ExecuteScript(cx, thisobj, script, NULL))
            return false;
    }

    JS_SET_RVAL(cx, vp, JSVAL_VOID);
    return true;
}

static JSBool
Evaluate(JSContext *cx, uintN argc, jsval *vp)
{
    if (argc != 1 || !JSVAL_IS_STRING(JS_ARGV(cx, vp)[0])) {
        JS_ReportErrorNumber(cx, my_GetErrorMessage, NULL,
                             (argc != 1) ? JSSMSG_NOT_ENOUGH_ARGS : JSSMSG_INVALID_ARGS,
                             "evaluate");
        return false;
    }

    JSString *code = JSVAL_TO_STRING(JS_ARGV(cx, vp)[0]);

    size_t codeLength;
    const jschar *codeChars = JS_GetStringCharsAndLength(cx, code, &codeLength);
    if (!codeChars)
        return false;

    JSObject *thisobj = JS_THIS_OBJECT(cx, vp);
    if (!thisobj)
        return false;

    if ((JS_GET_CLASS(cx, thisobj)->flags & JSCLASS_IS_GLOBAL) != JSCLASS_IS_GLOBAL) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_UNEXPECTED_TYPE,
                             "this-value passed to evaluate()", "not a global object");
        return false;
    }

    JS_SET_RVAL(cx, vp, JSVAL_VOID);
    return JS_EvaluateUCScript(cx, thisobj, codeChars, codeLength, "@evaluate", 0, NULL);
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
            obj = js_CreateTypedArray(cx, TypedArray::TYPE_UINT8, len);
            if (!obj)
                return NULL;
            char *buf = (char *) TypedArray::getDataOffset(TypedArray::getTypedArray(obj));
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
Run(JSContext *cx, uintN argc, jsval *vp)
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
    uint32 oldopts = JS_GetOptions(cx);
    JS_SetOptions(cx, oldopts | JSOPTION_COMPILE_N_GO | JSOPTION_NO_SCRIPT_RVAL);

    int64 startClock = PRMJ_Now();
    JSScript *script = JS_CompileUCScript(cx, thisobj, ucbuf, buflen, filename.ptr(), 1);
    JS_SetOptions(cx, oldopts);
    if (!script || !JS_ExecuteScript(cx, thisobj, script, NULL))
        return false;

    int64 endClock = PRMJ_Now();
    JS_SET_RVAL(cx, vp, DOUBLE_TO_JSVAL((endClock - startClock) / double(PRMJ_USEC_PER_MSEC)));
    return true;
}

/*
 * function readline()
 * Provides a hook for scripts to read a line from stdin.
 */
static JSBool
ReadLine(JSContext *cx, uintN argc, jsval *vp)
{
#define BUFSIZE 256
    FILE *from;
    char *buf, *tmp;
    size_t bufsize, buflength, gotlength;
    JSBool sawNewline;
    JSString *str;

    from = stdin;
    buflength = 0;
    bufsize = BUFSIZE;
    buf = (char *) JS_malloc(cx, bufsize);
    if (!buf)
        return JS_FALSE;

    sawNewline = JS_FALSE;
    while ((gotlength =
            js_fgets(buf + buflength, bufsize - buflength, from)) > 0) {
        buflength += gotlength;

        /* Are we done? */
        if (buf[buflength - 1] == '\n') {
            buf[buflength - 1] = '\0';
            sawNewline = JS_TRUE;
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
            return JS_FALSE;
        }

        buf = tmp;
    }

    /* Treat the empty string specially. */
    if (buflength == 0) {
        *vp = feof(from) ? JSVAL_NULL : JS_GetEmptyStringValue(cx);
        JS_free(cx, buf);
        return JS_TRUE;
    }

    /* Shrink the buffer to the real size. */
    tmp = (char *) JS_realloc(cx, buf, buflength);
    if (!tmp) {
        JS_free(cx, buf);
        return JS_FALSE;
    }

    buf = tmp;

    /*
     * Turn buf into a JSString. Note that buflength includes the trailing null
     * character.
     */
    str = JS_NewStringCopyN(cx, buf, sawNewline ? buflength - 1 : buflength);
    JS_free(cx, buf);
    if (!str)
        return JS_FALSE;

    *vp = STRING_TO_JSVAL(str);
    return JS_TRUE;
}

static JSBool
PutStr(JSContext *cx, uintN argc, jsval *vp)
{
    jsval *argv;
    JSString *str;
    char *bytes;

    if (argc != 0) {
        argv = JS_ARGV(cx, vp);
        str = JS_ValueToString(cx, argv[0]);
        if (!str)
            return JS_FALSE;
        bytes = JS_EncodeString(cx, str);
        if (!bytes)
            return JS_FALSE;
        fputs(bytes, gOutFile);
        JS_free(cx, bytes);
        fflush(gOutFile);
    }

    JS_SET_RVAL(cx, vp, JSVAL_VOID);
    return JS_TRUE;
}

static JSBool
Now(JSContext *cx, uintN argc, jsval *vp)
{
    jsdouble now = PRMJ_Now() / double(PRMJ_USEC_PER_MSEC);
    JS_SET_RVAL(cx, vp, DOUBLE_TO_JSVAL(now));
    return true;
}

static JSBool
Print(JSContext *cx, uintN argc, jsval *vp)
{
    jsval *argv;
    uintN i;
    JSString *str;
    char *bytes;

    argv = JS_ARGV(cx, vp);
    for (i = 0; i < argc; i++) {
        str = JS_ValueToString(cx, argv[i]);
        if (!str)
            return JS_FALSE;
        bytes = JS_EncodeString(cx, str);
        if (!bytes)
            return JS_FALSE;
        fprintf(gOutFile, "%s%s", i ? " " : "", bytes);
        JS_free(cx, bytes);
    }

    fputc('\n', gOutFile);
    fflush(gOutFile);

    JS_SET_RVAL(cx, vp, JSVAL_VOID);
    return JS_TRUE;
}

static JSBool
Help(JSContext *cx, uintN argc, jsval *vp);

static JSBool
Quit(JSContext *cx, uintN argc, jsval *vp)
{
    JS_ConvertArguments(cx, argc, JS_ARGV(cx, vp), "/ i", &gExitCode);

    gQuitting = JS_TRUE;
#ifdef JS_THREADSAFE
    if (gWorkerThreadPool)
        js::workers::terminateAll(JS_GetRuntime(cx), gWorkerThreadPool);
#endif
    return JS_FALSE;
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
AssertEq(JSContext *cx, uintN argc, jsval *vp)
{
    if (!(argc == 2 || (argc == 3 && JSVAL_IS_STRING(JS_ARGV(cx, vp)[2])))) {
        JS_ReportErrorNumber(cx, my_GetErrorMessage, NULL,
                             (argc < 2)
                             ? JSSMSG_NOT_ENOUGH_ARGS
                             : (argc == 3)
                             ? JSSMSG_INVALID_ARGS
                             : JSSMSG_TOO_MANY_ARGS,
                             "assertEq");
        return JS_FALSE;
    }

    jsval *argv = JS_ARGV(cx, vp);
    JSBool same;
    if (!JS_SameValue(cx, argv[0], argv[1], &same))
        return JS_FALSE;
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
                return JS_FALSE;
            JS_ReportErrorNumber(cx, my_GetErrorMessage, NULL, JSSMSG_ASSERT_EQ_FAILED_MSG,
                                 actual, expected, bytes2.ptr());
        }
        return JS_FALSE;
    }
    JS_SET_RVAL(cx, vp, JSVAL_VOID);
    return JS_TRUE;
}

static JSBool
AssertJit(JSContext *cx, uintN argc, jsval *vp)
{
#ifdef JS_METHODJIT
    if (JS_GetOptions(cx) & JSOPTION_METHODJIT) {
        /*
         * :XXX: Ignore calls to this native when inference is enabled,
         * with METHODJIT_ALWAYS recompilation can happen and discard the
         * script's jitcode.
         */
        if (!cx->typeInferenceEnabled() &&
            !cx->fp()->script()->getJIT(cx->fp()->isConstructing())) {
            JS_ReportErrorNumber(cx, my_GetErrorMessage, NULL, JSSMSG_ASSERT_JIT_FAILED);
            return JS_FALSE;
        }
    }
#endif

    JS_SET_RVAL(cx, vp, JSVAL_VOID);
    return JS_TRUE;
}

static JSBool
GC(JSContext *cx, uintN argc, jsval *vp)
{
    JSCompartment *comp = NULL;
    if (argc == 1) {
        Value arg = vp[2];
        if (arg.isObject())
            comp = UnwrapObject(&arg.toObject())->compartment();
    }

    size_t preBytes = cx->runtime->gcBytes;
    JS_CompartmentGC(cx, comp);

    char buf[256];
    JS_snprintf(buf, sizeof(buf), "before %lu, after %lu, break %08lx\n",
                (unsigned long)preBytes, (unsigned long)cx->runtime->gcBytes,
#ifdef HAVE_SBRK
                (unsigned long)sbrk(0)
#else
                0
#endif
                );
    *vp = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, buf));
    return true;
}

static const struct ParamPair {
    const char      *name;
    JSGCParamKey    param;
} paramMap[] = {
    {"maxBytes",            JSGC_MAX_BYTES },
    {"maxMallocBytes",      JSGC_MAX_MALLOC_BYTES},
    {"gcStackpoolLifespan", JSGC_STACKPOOL_LIFESPAN},
    {"gcBytes",             JSGC_BYTES},
    {"gcNumber",            JSGC_NUMBER},
};

static JSBool
GCParameter(JSContext *cx, uintN argc, jsval *vp)
{
    JSString *str;
    if (argc == 0) {
        str = JS_ValueToString(cx, JSVAL_VOID);
        JS_ASSERT(str);
    } else {
        str = JS_ValueToString(cx, vp[2]);
        if (!str)
            return JS_FALSE;
        vp[2] = STRING_TO_JSVAL(str);
    }

    JSFlatString *flatStr = JS_FlattenString(cx, str);
    if (!flatStr)
        return JS_FALSE;

    size_t paramIndex = 0;
    for (;; paramIndex++) {
        if (paramIndex == ArrayLength(paramMap)) {
            JS_ReportError(cx,
                           "the first argument argument must be maxBytes, "
                           "maxMallocBytes, gcStackpoolLifespan, gcBytes or "
                           "gcNumber");
            return JS_FALSE;
        }
        if (JS_FlatStringEqualsAscii(flatStr, paramMap[paramIndex].name))
            break;
    }
    JSGCParamKey param = paramMap[paramIndex].param;

    if (argc == 1) {
        uint32 value = JS_GetGCParameter(cx->runtime, param);
        return JS_NewNumberValue(cx, value, &vp[0]);
    }

    if (param == JSGC_NUMBER ||
        param == JSGC_BYTES) {
        JS_ReportError(cx, "Attempt to change read-only parameter %s",
                       paramMap[paramIndex].name);
        return JS_FALSE;
    }

    uint32 value;
    if (!JS_ValueToECMAUint32(cx, vp[3], &value)) {
        JS_ReportError(cx,
                       "the second argument must be convertable to uint32 "
                       "with non-zero value");
        return JS_FALSE;
    }
    JS_SetGCParameter(cx->runtime, param, value);
    *vp = JSVAL_VOID;
    return JS_TRUE;
}

static JSBool
InternalConst(JSContext *cx, uintN argc, jsval *vp)
{
    if (argc != 1) {
        JS_ReportError(cx, "the function takes exactly one argument");
        return false;
    }

    JSString *str = JS_ValueToString(cx, vp[2]);
    if (!str)
        return false;
    JSFlatString *flat = JS_FlattenString(cx, str);
    if (!flat)
        return false;

    if (JS_FlatStringEqualsAscii(flat, "OBJECT_MARK_STACK_LENGTH")) {
        vp[0] = UINT_TO_JSVAL(js::OBJECT_MARK_STACK_SIZE / sizeof(JSObject *));
    } else {
        JS_ReportError(cx, "unknown const name");
        return false;
    }
    return true;
}

#ifdef JS_GC_ZEAL
static JSBool
GCZeal(JSContext *cx, uintN argc, jsval *vp)
{
    uint32 zeal, frequency = JS_DEFAULT_ZEAL_FREQ;
    JSBool compartment = JS_FALSE;

    if (argc > 3) {
        JS_ReportErrorNumber(cx, my_GetErrorMessage, NULL, JSSMSG_TOO_MANY_ARGS, "gczeal");
        return JS_FALSE;
    }
    if (!JS_ValueToECMAUint32(cx, argc < 1 ? JSVAL_VOID : vp[2], &zeal))
        return JS_FALSE;
    if (argc >= 2)
        if (!JS_ValueToECMAUint32(cx, vp[3], &frequency))
            return JS_FALSE;
    if (argc >= 3)
        compartment = js_ValueToBoolean(vp[3]);

    JS_SetGCZeal(cx, (uint8)zeal, frequency, compartment);
    *vp = JSVAL_VOID;
    return JS_TRUE;
}

static JSBool
ScheduleGC(JSContext *cx, uintN argc, jsval *vp)
{
    uint32 count;
    bool compartment = false;

    if (argc != 1 && argc != 2) {
        JS_ReportErrorNumber(cx, my_GetErrorMessage, NULL,
                             (argc < 1)
                             ? JSSMSG_NOT_ENOUGH_ARGS
                             : JSSMSG_TOO_MANY_ARGS,
                             "schedulegc");
        return JS_FALSE;
    }
    if (!JS_ValueToECMAUint32(cx, vp[2], &count))
        return JS_FALSE;
    if (argc == 2)
        compartment = js_ValueToBoolean(vp[3]);

    JS_ScheduleGC(cx, count, compartment);
    *vp = JSVAL_VOID;
    return JS_TRUE;
}
#endif /* JS_GC_ZEAL */

typedef struct JSCountHeapNode JSCountHeapNode;

struct JSCountHeapNode {
    void                *thing;
    JSGCTraceKind       kind;
    JSCountHeapNode     *next;
};

typedef struct JSCountHeapTracer {
    JSTracer            base;
    JSDHashTable        visited;
    JSBool              ok;
    JSCountHeapNode     *traceList;
    JSCountHeapNode     *recycleList;
} JSCountHeapTracer;

static void
CountHeapNotify(JSTracer *trc, void *thing, JSGCTraceKind kind)
{
    JSCountHeapTracer *countTracer;
    JSDHashEntryStub *entry;
    JSCountHeapNode *node;

    JS_ASSERT(trc->callback == CountHeapNotify);
    countTracer = (JSCountHeapTracer *)trc;
    if (!countTracer->ok)
        return;

    entry = (JSDHashEntryStub *)
            JS_DHashTableOperate(&countTracer->visited, thing, JS_DHASH_ADD);
    if (!entry) {
        JS_ReportOutOfMemory(trc->context);
        countTracer->ok = JS_FALSE;
        return;
    }
    if (entry->key)
        return;
    entry->key = thing;

    node = countTracer->recycleList;
    if (node) {
        countTracer->recycleList = node->next;
    } else {
        node = (JSCountHeapNode *) JS_malloc(trc->context, sizeof *node);
        if (!node) {
            countTracer->ok = JS_FALSE;
            return;
        }
    }
    node->thing = thing;
    node->kind = kind;
    node->next = countTracer->traceList;
    countTracer->traceList = node;
}

static const struct TraceKindPair {
    const char       *name;
    int32             kind;
} traceKindNames[] = {
    { "all",        -1                  },
    { "object",     JSTRACE_OBJECT      },
    { "string",     JSTRACE_STRING      },
#if JS_HAS_XML_SUPPORT
    { "xml",        JSTRACE_XML         },
#endif
};

static JSBool
CountHeap(JSContext *cx, uintN argc, jsval *vp)
{
    void* startThing;
    JSGCTraceKind startTraceKind;
    jsval v;
    int32 traceKind, i;
    JSString *str;
    JSCountHeapTracer countTracer;
    JSCountHeapNode *node;
    size_t counter;

    startThing = NULL;
    startTraceKind = JSTRACE_OBJECT;
    if (argc > 0) {
        v = JS_ARGV(cx, vp)[0];
        if (JSVAL_IS_TRACEABLE(v)) {
            startThing = JSVAL_TO_TRACEABLE(v);
            startTraceKind = JSVAL_TRACE_KIND(v);
        } else if (!JSVAL_IS_NULL(v)) {
            JS_ReportError(cx,
                           "the first argument is not null or a heap-allocated "
                           "thing");
            return JS_FALSE;
        }
    }

    traceKind = -1;
    if (argc > 1) {
        str = JS_ValueToString(cx, JS_ARGV(cx, vp)[1]);
        if (!str)
            return JS_FALSE;
        JSFlatString *flatStr = JS_FlattenString(cx, str);
        if (!flatStr)
            return JS_FALSE;
        for (i = 0; ;) {
            if (JS_FlatStringEqualsAscii(flatStr, traceKindNames[i].name)) {
                traceKind = traceKindNames[i].kind;
                break;
            }
            if (++i == ArrayLength(traceKindNames)) {
                JSAutoByteString bytes(cx, str);
                if (!!bytes)
                    JS_ReportError(cx, "trace kind name '%s' is unknown", bytes.ptr());
                return JS_FALSE;
            }
        }
    }

    JS_TRACER_INIT(&countTracer.base, cx, CountHeapNotify);
    if (!JS_DHashTableInit(&countTracer.visited, JS_DHashGetStubOps(),
                           NULL, sizeof(JSDHashEntryStub),
                           JS_DHASH_DEFAULT_CAPACITY(100))) {
        JS_ReportOutOfMemory(cx);
        return JS_FALSE;
    }
    countTracer.ok = JS_TRUE;
    countTracer.traceList = NULL;
    countTracer.recycleList = NULL;

    if (!startThing) {
        JS_TraceRuntime(&countTracer.base);
    } else {
        JS_SET_TRACING_NAME(&countTracer.base, "root");
        JS_CallTracer(&countTracer.base, startThing, startTraceKind);
    }

    counter = 0;
    while ((node = countTracer.traceList) != NULL) {
        if (traceKind == -1 || node->kind == traceKind)
            counter++;
        countTracer.traceList = node->next;
        node->next = countTracer.recycleList;
        countTracer.recycleList = node;
        JS_TraceChildren(&countTracer.base, node->thing, node->kind);
    }
    while ((node = countTracer.recycleList) != NULL) {
        countTracer.recycleList = node->next;
        JS_free(cx, node);
    }
    JS_DHashTableFinish(&countTracer.visited);

    return countTracer.ok && JS_NewNumberValue(cx, (jsdouble) counter, vp);
}

static jsrefcount finalizeCount = 0;

static void
finalize_counter_finalize(JSContext *cx, JSObject *obj)
{
    JS_ATOMIC_INCREMENT(&finalizeCount);
}

static JSClass FinalizeCounterClass = {
    "FinalizeCounter", JSCLASS_IS_ANONYMOUS,
    JS_PropertyStub,       /* addProperty */
    JS_PropertyStub,       /* delProperty */
    JS_PropertyStub,       /* getProperty */
    JS_StrictPropertyStub, /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub,
    finalize_counter_finalize
};

static JSBool
MakeFinalizeObserver(JSContext *cx, uintN argc, jsval *vp)
{
    JSObject *obj = JS_NewObjectWithGivenProto(cx, &FinalizeCounterClass, NULL,
                                               JS_GetGlobalObject(cx));
    if (!obj)
        return false;
    *vp = OBJECT_TO_JSVAL(obj);
    return true;
}

static JSBool
FinalizeCount(JSContext *cx, uintN argc, jsval *vp)
{
    *vp = INT_TO_JSVAL(finalizeCount);
    return true;
}

static JSScript *
ValueToScript(JSContext *cx, jsval v, JSFunction **funp = NULL)
{
    JSScript *script = NULL;
    JSFunction *fun = NULL;

    if (!JSVAL_IS_PRIMITIVE(v)) {
        JSObject *obj = JSVAL_TO_OBJECT(v);
        JSClass *clasp = JS_GET_CLASS(cx, obj);

        if (clasp == Jsvalify(&ScriptClass)) {
            script = (JSScript *) JS_GetPrivate(cx, obj);
        } else if (clasp == Jsvalify(&GeneratorClass)) {
            JSGenerator *gen = (JSGenerator *) JS_GetPrivate(cx, obj);
            fun = gen->floatingFrame()->fun();
            script = fun->script();
        }
    }

    if (!script) {
        fun = JS_ValueToFunction(cx, v);
        if (!fun)
            return NULL;
        script = fun->maybeScript();
        if (!script) {
            JS_ReportErrorNumber(cx, my_GetErrorMessage, NULL,
                                 JSSMSG_SCRIPTS_ONLY);
        }
    }
    if (fun && funp)
        *funp = fun;

    return script;
}

static JSBool
SetDebug(JSContext *cx, uintN argc, jsval *vp)
{
    jsval *argv = JS_ARGV(cx, vp);
    if (argc == 0 || !JSVAL_IS_BOOLEAN(argv[0])) {
        JS_ReportErrorNumber(cx, my_GetErrorMessage, NULL,
                             JSSMSG_NOT_ENOUGH_ARGS, "setDebug");
        return JS_FALSE;
    }

    /*
     * Debug mode can only be set when there is no JS code executing on the
     * stack. Unfortunately, that currently means that this call will fail
     * unless debug mode is already set to what you're trying to set it to.
     * In the future, this restriction may be lifted.
     */

    JSBool ok = JS_SetDebugMode(cx, JSVAL_TO_BOOLEAN(argv[0]));
    if (ok)
        JS_SET_RVAL(cx, vp, JSVAL_TRUE);
    return ok;
}

static JSBool
GetTrapArgs(JSContext *cx, uintN argc, jsval *argv, JSScript **scriptp,
            int32 *ip)
{
    jsval v;
    uintN intarg;
    JSScript *script;

    *scriptp = JS_GetFrameScript(cx, JS_GetScriptedCaller(cx, NULL));
    *ip = 0;
    if (argc != 0) {
        v = argv[0];
        intarg = 0;
        if (!JSVAL_IS_PRIMITIVE(v) &&
            (JS_GET_CLASS(cx, JSVAL_TO_OBJECT(v)) == Jsvalify(&FunctionClass) ||
             JS_GET_CLASS(cx, JSVAL_TO_OBJECT(v)) == Jsvalify(&ScriptClass))) {
            script = ValueToScript(cx, v);
            if (!script)
                return JS_FALSE;
            *scriptp = script;
            intarg++;
        }
        if (argc > intarg) {
            if (!JS_ValueToInt32(cx, argv[intarg], ip))
                return JS_FALSE;
        }
    }
    return JS_TRUE;
}

static JSTrapStatus
TrapHandler(JSContext *cx, JSScript *, jsbytecode *pc, jsval *rval,
            jsval closure)
{
    JSString *str = JSVAL_TO_STRING(closure);
    JSStackFrame *caller = JS_GetScriptedCaller(cx, NULL);
    JSScript *script = JS_GetFrameScript(cx, caller);

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
Trap(JSContext *cx, uintN argc, jsval *vp)
{
    JSString *str;
    JSScript *script;
    int32 i;

    jsval *argv = JS_ARGV(cx, vp);
    if (argc == 0) {
        JS_ReportErrorNumber(cx, my_GetErrorMessage, NULL, JSSMSG_TRAP_USAGE);
        return JS_FALSE;
    }
    argc--;
    str = JS_ValueToString(cx, argv[argc]);
    if (!str)
        return JS_FALSE;
    argv[argc] = STRING_TO_JSVAL(str);
    if (!GetTrapArgs(cx, argc, argv, &script, &i))
        return JS_FALSE;
    if (uint32(i) >= script->length) {
        JS_ReportErrorNumber(cx, my_GetErrorMessage, NULL, JSSMSG_TRAP_USAGE);
        return JS_FALSE;
    }
    JS_SET_RVAL(cx, vp, JSVAL_VOID);
    return JS_SetTrap(cx, script, script->code + i, TrapHandler, STRING_TO_JSVAL(str));
}

static JSBool
Untrap(JSContext *cx, uintN argc, jsval *vp)
{
    JSScript *script;
    int32 i;

    if (!GetTrapArgs(cx, argc, JS_ARGV(cx, vp), &script, &i))
        return JS_FALSE;
    JS_ClearTrap(cx, script, script->code + i, NULL, NULL);
    JS_SET_RVAL(cx, vp, JSVAL_VOID);
    return JS_TRUE;
}

static JSTrapStatus
DebuggerAndThrowHandler(JSContext *cx, JSScript *script, jsbytecode *pc, jsval *rval,
                        void *closure)
{
    return TrapHandler(cx, script, pc, rval, STRING_TO_JSVAL((JSString *)closure));
}

static JSBool
SetDebuggerHandler(JSContext *cx, uintN argc, jsval *vp)
{
    JSString *str;
    if (argc == 0) {
        JS_ReportErrorNumber(cx, my_GetErrorMessage, NULL,
                             JSSMSG_NOT_ENOUGH_ARGS, "setDebuggerHandler");
        return JS_FALSE;
    }

    str = JS_ValueToString(cx, JS_ARGV(cx, vp)[0]);
    if (!str)
        return JS_FALSE;

    JS_SetDebuggerHandler(cx->runtime, DebuggerAndThrowHandler, str);
    JS_SET_RVAL(cx, vp, JSVAL_VOID);
    return JS_TRUE;
}

static JSBool
SetThrowHook(JSContext *cx, uintN argc, jsval *vp)
{
    JSString *str;
    if (argc == 0) {
        JS_ReportErrorNumber(cx, my_GetErrorMessage, NULL,
                             JSSMSG_NOT_ENOUGH_ARGS, "setThrowHook");
        return JS_FALSE;
    }

    str = JS_ValueToString(cx, JS_ARGV(cx, vp)[0]);
    if (!str)
        return JS_FALSE;

    JS_SetThrowHook(cx->runtime, DebuggerAndThrowHandler, str);
    JS_SET_RVAL(cx, vp, JSVAL_VOID);
    return JS_TRUE;
}

static JSBool
LineToPC(JSContext *cx, uintN argc, jsval *vp)
{
    JSScript *script;
    int32 i;
    uintN lineno;
    jsbytecode *pc;

    if (argc == 0) {
        JS_ReportErrorNumber(cx, my_GetErrorMessage, NULL, JSSMSG_LINE2PC_USAGE);
        return JS_FALSE;
    }
    script = JS_GetFrameScript(cx, JS_GetScriptedCaller(cx, NULL));
    if (!GetTrapArgs(cx, argc, JS_ARGV(cx, vp), &script, &i))
        return JS_FALSE;
    lineno = (i == 0) ? script->lineno : (uintN)i;
    pc = JS_LineNumberToPC(cx, script, lineno);
    if (!pc)
        return JS_FALSE;
    *vp = INT_TO_JSVAL(pc - script->code);
    return JS_TRUE;
}

static JSBool
PCToLine(JSContext *cx, uintN argc, jsval *vp)
{
    JSScript *script;
    int32 i;
    uintN lineno;

    if (!GetTrapArgs(cx, argc, JS_ARGV(cx, vp), &script, &i))
        return JS_FALSE;
    lineno = JS_PCToLineNumber(cx, script, script->code + i);
    if (!lineno)
        return JS_FALSE;
    *vp = INT_TO_JSVAL(lineno);
    return JS_TRUE;
}

#ifdef DEBUG

static void
UpdateSwitchTableBounds(JSContext *cx, JSScript *script, uintN offset,
                        uintN *start, uintN *end)
{
    jsbytecode *pc;
    JSOp op;
    ptrdiff_t jmplen;
    jsint low, high, n;

    pc = script->code + offset;
    op = js_GetOpcode(cx, script, pc);
    switch (op) {
      case JSOP_TABLESWITCHX:
        jmplen = JUMPX_OFFSET_LEN;
        goto jump_table;
      case JSOP_TABLESWITCH:
        jmplen = JUMP_OFFSET_LEN;
      jump_table:
        pc += jmplen;
        low = GET_JUMP_OFFSET(pc);
        pc += JUMP_OFFSET_LEN;
        high = GET_JUMP_OFFSET(pc);
        pc += JUMP_OFFSET_LEN;
        n = high - low + 1;
        break;

      case JSOP_LOOKUPSWITCHX:
        jmplen = JUMPX_OFFSET_LEN;
        goto lookup_table;
      case JSOP_LOOKUPSWITCH:
        jmplen = JUMP_OFFSET_LEN;
      lookup_table:
        pc += jmplen;
        n = GET_INDEX(pc);
        pc += INDEX_LEN;
        jmplen += JUMP_OFFSET_LEN;
        break;

      default:
        /* [condswitch] switch does not have any jump or lookup tables. */
        JS_ASSERT(op == JSOP_CONDSWITCH);
        return;
    }

    *start = (uintN)(pc - script->code);
    *end = *start + (uintN)(n * jmplen);
}

static void
SrcNotes(JSContext *cx, JSScript *script, Sprinter *sp)
{
    uintN offset, lineno, delta, caseOff, switchTableStart, switchTableEnd;
    jssrcnote *notes, *sn;
    JSSrcNoteType type;
    const char *name;
    uint32 index;
    JSAtom *atom;
    JSString *str;

    Sprint(sp, "\nSource notes:\n");
    Sprint(sp, "%4s  %4s %5s %6s %-8s %s\n",
           "ofs", "line", "pc", "delta", "desc", "args");
    Sprint(sp, "---- ---- ----- ------ -------- ------\n");
    offset = 0;
    lineno = script->lineno;
    notes = script->notes();
    switchTableEnd = switchTableStart = 0;
    for (sn = notes; !SN_IS_TERMINATOR(sn); sn = SN_NEXT(sn)) {
        delta = SN_DELTA(sn);
        offset += delta;
        type = (JSSrcNoteType) SN_TYPE(sn);
        name = js_SrcNoteSpec[type].name;
        if (type == SRC_LABEL) {
            /* Check if the source note is for a switch case. */
            if (switchTableStart <= offset && offset < switchTableEnd) {
                name = "case";
            } else {
                JS_ASSERT(js_GetOpcode(cx, script, script->code + offset) == JSOP_NOP);
            }
        }
        Sprint(sp, "%3u: %4u %5u [%4u] %-8s", uintN(sn - notes), lineno, offset, delta, name);
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
                   uintN(js_GetSrcNoteOffset(sn, 0)),
                   uintN(js_GetSrcNoteOffset(sn, 1)),
                   uintN(js_GetSrcNoteOffset(sn, 2)));
            break;
          case SRC_IF_ELSE:
            Sprint(sp, " else %u elseif %u",
                   uintN(js_GetSrcNoteOffset(sn, 0)),
                   uintN(js_GetSrcNoteOffset(sn, 1)));
            break;
          case SRC_COND:
          case SRC_WHILE:
          case SRC_PCBASE:
          case SRC_PCDELTA:
          case SRC_DECL:
          case SRC_BRACE:
            Sprint(sp, " offset %u", uintN(js_GetSrcNoteOffset(sn, 0)));
            break;
          case SRC_LABEL:
          case SRC_LABELBRACE:
          case SRC_BREAK2LABEL:
          case SRC_CONT2LABEL:
            index = js_GetSrcNoteOffset(sn, 0);
            atom = script->getAtom(index);
            Sprint(sp, " atom %u (", index);
            {
                size_t len = PutEscapedString(NULL, 0, atom, '\0');
                if (char *buf = SprintReserveAmount(sp, len)) {
                    PutEscapedString(buf, len, atom, 0);
                    buf[len] = '\0';
                }
            }
            Sprint(sp, ")");
            break;
          case SRC_FUNCDEF: {
            index = js_GetSrcNoteOffset(sn, 0);
            JSObject *obj = script->getObject(index);
            JSFunction *fun = (JSFunction *) JS_GetPrivate(cx, obj);
            str = JS_DecompileFunction(cx, fun, JS_DONT_PRETTY_PRINT);
            JSAutoByteString bytes;
            if (!str || !bytes.encode(cx, str))
                ReportException(cx);
            Sprint(sp, " function %u (%s)", index, !!bytes ? bytes.ptr() : "N/A");
            break;
          }
          case SRC_SWITCH: {
            JSOp op = js_GetOpcode(cx, script, script->code + offset);
            if (op == JSOP_GOTO || op == JSOP_GOTOX)
                break;
            Sprint(sp, " length %u", uintN(js_GetSrcNoteOffset(sn, 0)));
            caseOff = (uintN) js_GetSrcNoteOffset(sn, 1);
            if (caseOff)
                Sprint(sp, " first case offset %u", caseOff);
            UpdateSwitchTableBounds(cx, script, offset,
                                    &switchTableStart, &switchTableEnd);
            break;
          }
          case SRC_CATCH:
            delta = (uintN) js_GetSrcNoteOffset(sn, 0);
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
Notes(JSContext *cx, uintN argc, jsval *vp)
{
    LifoAllocScope las(&cx->tempLifoAlloc());
    Sprinter sprinter;
    INIT_SPRINTER(cx, &sprinter, &cx->tempLifoAlloc(), 0);

    jsval *argv = JS_ARGV(cx, vp);
    for (uintN i = 0; i < argc; i++) {
        JSScript *script = ValueToScript(cx, argv[i]);
        if (!script)
            continue;

        SrcNotes(cx, script, &sprinter);
    }

    JSString *str = JS_NewStringCopyZ(cx, sprinter.base);
    if (!str)
        return JS_FALSE;
    JS_SET_RVAL(cx, vp, STRING_TO_JSVAL(str));
    return JS_TRUE;
}

JS_STATIC_ASSERT(JSTRY_CATCH == 0);
JS_STATIC_ASSERT(JSTRY_FINALLY == 1);
JS_STATIC_ASSERT(JSTRY_ITER == 2);

static const char* const TryNoteNames[] = { "catch", "finally", "iter" };

static JSBool
TryNotes(JSContext *cx, JSScript *script, Sprinter *sp)
{
    JSTryNote *tn, *tnlimit;

    if (!JSScript::isValidOffset(script->trynotesOffset))
        return JS_TRUE;

    tn = script->trynotes()->vector;
    tnlimit = tn + script->trynotes()->length;
    Sprint(sp, "\nException table:\nkind      stack    start      end\n");
    do {
        JS_ASSERT(tn->kind < ArrayLength(TryNoteNames));
        Sprint(sp, " %-7s %6u %8u %8u\n",
               TryNoteNames[tn->kind], tn->stackDepth,
               tn->start, tn->start + tn->length);
    } while (++tn != tnlimit);
    return JS_TRUE;
}

static bool
DisassembleScript(JSContext *cx, JSScript *script, JSFunction *fun, bool lines, bool recursive,
                  Sprinter *sp)
{
    if (fun && (fun->flags & ~7U)) {
        uint16 flags = fun->flags;
        Sprint(sp, "flags:");
        
#define SHOW_FLAG(flag) if (flags & JSFUN_##flag) Sprint(sp, " " #flag);
        
        SHOW_FLAG(LAMBDA);
        SHOW_FLAG(HEAVYWEIGHT);
        SHOW_FLAG(EXPR_CLOSURE);
        SHOW_FLAG(TRCINFO);
        
#undef SHOW_FLAG
        
        if (fun->isNullClosure())
            Sprint(sp, " NULL_CLOSURE");
        else if (fun->isFlatClosure())
            Sprint(sp, " FLAT_CLOSURE");
        
        JSScript *script = fun->script();
        if (script->bindings.hasUpvars()) {
            Sprint(sp, "\nupvars: {\n");
            
            Vector<JSAtom *> localNames(cx);
            if (!script->bindings.getLocalNameArray(cx, &localNames))
                return false;
            
            JSUpvarArray *uva = script->upvars();
            uintN upvar_base = script->bindings.countArgsAndVars();
            
            for (uint32 i = 0, n = uva->length; i < n; i++) {
                JSAtom *atom = localNames[upvar_base + i];
                UpvarCookie cookie = uva->vector[i];
                JSAutoByteString printable;
                if (js_AtomToPrintableString(cx, atom, &printable)) {
                    Sprint(sp, "  %s: {skip:%u, slot:%u},\n",
                           printable.ptr(), cookie.level(), cookie.slot());
                }
            }
            
            Sprint(sp, "}");
        }
        Sprint(sp, "\n");
    }

    if (!js_Disassemble(cx, script, lines, sp))
        return false;
    SrcNotes(cx, script, sp);
    TryNotes(cx, script, sp);

    if (recursive && JSScript::isValidOffset(script->objectsOffset)) {
        JSObjectArray *objects = script->objects();
        for (uintN i = 0; i != objects->length; ++i) {
            JSObject *obj = objects->vector[i];
            if (obj->isFunction()) {
                Sprint(sp, "\n");
                JSFunction *fun = obj->getFunctionPrivate();
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
    uintN   argc;
    jsval   *argv;
    bool    lines;
    bool    recursive;

    DisassembleOptionParser(uintN argc, jsval *argv)
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
DisassembleToString(JSContext *cx, uintN argc, jsval *vp)
{
    DisassembleOptionParser p(argc, JS_ARGV(cx, vp));
    if (!p.parse(cx))
        return false;

    LifoAllocScope las(&cx->tempLifoAlloc());
    Sprinter sprinter;
    INIT_SPRINTER(cx, &sprinter, &cx->tempLifoAlloc(), 0);
    Sprinter *sp = &sprinter;

    bool ok = true;
    if (p.argc == 0) {
        /* Without arguments, disassemble the current script. */
        if (JSStackFrame *frame = JS_GetScriptedCaller(cx, NULL)) {
            JSScript *script = JS_GetFrameScript(cx, frame);
            if (js_Disassemble(cx, script, p.lines, sp)) {
                SrcNotes(cx, script, sp);
                TryNotes(cx, script, sp);
            } else {
                ok = false;
            }
        }
    } else {
        for (uintN i = 0; i < p.argc; i++) {
            JSFunction *fun;
            JSScript *script = ValueToScript(cx, p.argv[i], &fun);
            ok = ok && script && DisassembleScript(cx, script, fun, p.lines, p.recursive, sp);
        }
    }

    JSString *str = ok ? JS_NewStringCopyZ(cx, sprinter.base) : NULL;
    if (!str)
        return false;
    JS_SET_RVAL(cx, vp, STRING_TO_JSVAL(str));
    return true;
}

static JSBool
Disassemble(JSContext *cx, uintN argc, jsval *vp)
{
    DisassembleOptionParser p(argc, JS_ARGV(cx, vp));
    if (!p.parse(cx))
        return false;

    LifoAllocScope las(&cx->tempLifoAlloc());
    Sprinter sprinter;
    INIT_SPRINTER(cx, &sprinter, &cx->tempLifoAlloc(), 0);
    Sprinter *sp = &sprinter;

    bool ok = true;
    if (p.argc == 0) {
        /* Without arguments, disassemble the current script. */
        if (JSStackFrame *frame = JS_GetScriptedCaller(cx, NULL)) {
            JSScript *script = JS_GetFrameScript(cx, frame);
            if (js_Disassemble(cx, script, p.lines, sp)) {
                SrcNotes(cx, script, sp);
                TryNotes(cx, script, sp);
            } else {
                ok = false;
            }
        }
    } else {
        for (uintN i = 0; i < p.argc; i++) {
            JSFunction *fun;
            JSScript *script = ValueToScript(cx, p.argv[i], &fun);
            ok = ok && script && DisassembleScript(cx, script, fun, p.lines, p.recursive, sp);
        }
    }

    if (ok)
        fprintf(stdout, "%s\n", sprinter.base);
    JS_SET_RVAL(cx, vp, JSVAL_VOID);
    return ok;
}

static JSBool
DisassFile(JSContext *cx, uintN argc, jsval *vp)
{
    /* Support extra options at the start, just like Dissassemble. */
    DisassembleOptionParser p(argc, JS_ARGV(cx, vp));
    if (!p.parse(cx))
        return false;
    
    if (!p.argc) {
        JS_SET_RVAL(cx, vp, JSVAL_VOID);
        return JS_TRUE;
    }

    JSObject *thisobj = JS_THIS_OBJECT(cx, vp);
    if (!thisobj)
        return JS_FALSE;

    JSString *str = JS_ValueToString(cx, p.argv[0]);
    if (!str)
        return JS_FALSE;
    JSAutoByteString filename(cx, str);
    if (!filename)
        return JS_FALSE;

    uint32 oldopts = JS_GetOptions(cx);
    JS_SetOptions(cx, oldopts | JSOPTION_COMPILE_N_GO | JSOPTION_NO_SCRIPT_RVAL);
    JSScript *script = JS_CompileFile(cx, thisobj, filename.ptr());
    JS_SetOptions(cx, oldopts);
    if (!script)
        return false;

    LifoAllocScope las(&cx->tempLifoAlloc());
    Sprinter sprinter;
    INIT_SPRINTER(cx, &sprinter, &cx->tempLifoAlloc(), 0);
    bool ok = DisassembleScript(cx, script, NULL, p.lines, p.recursive, &sprinter);
    if (ok)
        fprintf(stdout, "%s\n", sprinter.base);
    if (!ok)
        return false;
    
    JS_SET_RVAL(cx, vp, JSVAL_VOID);
    return true;
}

static JSBool
DisassWithSrc(JSContext *cx, uintN argc, jsval *vp)
{
#define LINE_BUF_LEN 512
    uintN i, len, line1, line2, bupline;
    JSScript *script;
    FILE *file;
    char linebuf[LINE_BUF_LEN];
    jsbytecode *pc, *end;
    JSBool ok;
    static char sep[] = ";-------------------------";

    ok = JS_TRUE;
    jsval *argv = JS_ARGV(cx, vp);
    for (i = 0; ok && i < argc; i++) {
        script = ValueToScript(cx, argv[i]);
        if (!script)
           return JS_FALSE;

        if (!script->filename) {
            JS_ReportErrorNumber(cx, my_GetErrorMessage, NULL,
                                 JSSMSG_FILE_SCRIPTS_ONLY);
            return JS_FALSE;
        }

        file = fopen(script->filename, "r");
        if (!file) {
            JS_ReportErrorNumber(cx, my_GetErrorMessage, NULL,
                                 JSSMSG_CANT_OPEN, script->filename,
                                 strerror(errno));
            return JS_FALSE;
        }

        pc = script->code;
        end = pc + script->length;

        LifoAllocScope las(&cx->tempLifoAlloc());
        Sprinter sprinter;
        Sprinter *sp = &sprinter;
        INIT_SPRINTER(cx, sp, &cx->tempLifoAlloc(), 0);

        /* burn the leading lines */
        line2 = JS_PCToLineNumber(cx, script, pc);
        for (line1 = 0; line1 < line2 - 1; line1++) {
            char *tmp = fgets(linebuf, LINE_BUF_LEN, file);
            if (!tmp) {
                JS_ReportError(cx, "failed to read %s fully", script->filename);
                ok = JS_FALSE;
                goto bail;
            }
        }

        bupline = 0;
        while (pc < end) {
            line2 = JS_PCToLineNumber(cx, script, pc);

            if (line2 < line1) {
                if (bupline != line2) {
                    bupline = line2;
                    Sprint(sp, "%s %3u: BACKUP\n", sep, line2);
                }
            } else {
                if (bupline && line1 == line2)
                    Sprint(sp, "%s %3u: RESTORE\n", sep, line2);
                bupline = 0;
                while (line1 < line2) {
                    if (!fgets(linebuf, LINE_BUF_LEN, file)) {
                        JS_ReportErrorNumber(cx, my_GetErrorMessage, NULL,
                                             JSSMSG_UNEXPECTED_EOF,
                                             script->filename);
                        ok = JS_FALSE;
                        goto bail;
                    }
                    line1++;
                    Sprint(sp, "%s %3u: %s", sep, line1, linebuf);
                }
            }

            len = js_Disassemble1(cx, script, pc, pc - script->code, JS_TRUE, sp);
            if (!len) {
                ok = JS_FALSE;
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

static void
DumpScope(JSContext *cx, JSObject *obj, FILE *fp)
{
    uintN i = 0;
    for (JSScopeProperty *sprop = NULL; JS_PropertyIterator(obj, &sprop);) {
        fprintf(fp, "%3u %p ", i++, (void *) sprop);
        ((Shape *) sprop)->dump(cx, fp);
    }
}

static JSBool
DumpStats(JSContext *cx, uintN argc, jsval *vp)
{
    uintN i;
    JSString *str;
    jsid id;
    JSObject *obj2;
    JSProperty *prop;
    Value value;

    jsval *argv = JS_ARGV(cx, vp);
    for (i = 0; i < argc; i++) {
        str = JS_ValueToString(cx, argv[i]);
        if (!str)
            return JS_FALSE;
        argv[i] = STRING_TO_JSVAL(str);
        JSFlatString *flatStr = JS_FlattenString(cx, str);
        if (!flatStr)
            return JS_FALSE;
        if (JS_FlatStringEqualsAscii(flatStr, "atom")) {
            js_DumpAtoms(cx, gOutFile);
        } else if (JS_FlatStringEqualsAscii(flatStr, "global")) {
            DumpScope(cx, cx->globalObject, stdout);
        } else {
            if (!JS_ValueToId(cx, STRING_TO_JSVAL(str), &id))
                return JS_FALSE;
            JSObject *obj;
            if (!js_FindProperty(cx, id, false, &obj, &obj2, &prop))
                return JS_FALSE;
            if (prop) {
                if (!obj->getGeneric(cx, id, &value))
                    return JS_FALSE;
            }
            if (!prop || !value.isObjectOrNull()) {
                fputs("js: invalid stats argument ", gErrFile);
                JS_FileEscapedString(gErrFile, str, 0);
                putc('\n', gErrFile);
                continue;
            }
            obj = value.toObjectOrNull();
            if (obj)
                DumpScope(cx, obj, stdout);
        }
    }
    JS_SET_RVAL(cx, vp, JSVAL_VOID);
    return JS_TRUE;
}

static JSBool
DumpHeap(JSContext *cx, uintN argc, jsval *vp)
{
    jsval v;
    void* startThing;
    JSGCTraceKind startTraceKind;
    const char *badTraceArg;
    void *thingToFind;
    size_t maxDepth;
    void *thingToIgnore;
    FILE *dumpFile;
    JSBool ok;

    const char *fileName = NULL;
    JSAutoByteString fileNameBytes;
    if (argc > 0) {
        v = JS_ARGV(cx, vp)[0];
        if (!JSVAL_IS_NULL(v)) {
            JSString *str;

            str = JS_ValueToString(cx, v);
            if (!str)
                return JS_FALSE;
            JS_ARGV(cx, vp)[0] = STRING_TO_JSVAL(str);
            if (!fileNameBytes.encode(cx, str))
                return JS_FALSE;
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
            uint32 depth;

            if (!JS_ValueToECMAUint32(cx, v, &depth))
                return JS_FALSE;
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
            return JS_FALSE;
        }
    }

    ok = JS_DumpHeap(cx, dumpFile, startThing, startTraceKind, thingToFind,
                     maxDepth, thingToIgnore);
    if (dumpFile != stdout)
        fclose(dumpFile);
    JS_SET_RVAL(cx, vp, JSVAL_VOID);
    return ok;

  not_traceable_arg:
    JS_ReportError(cx, "argument '%s' is not null or a heap-allocated thing",
                   badTraceArg);
    return JS_FALSE;
}

JSBool
DumpObject(JSContext *cx, uintN argc, jsval *vp)
{
    JSObject *arg0 = NULL;
    if (!JS_ConvertArguments(cx, argc, JS_ARGV(cx, vp), "o", &arg0))
        return JS_FALSE;

    js_DumpObject(arg0);

    JS_SET_RVAL(cx, vp, JSVAL_VOID);
    return JS_TRUE;
}

#endif /* DEBUG */

/*
 * This shell function is temporary (used by testStackIter.js) and should be
 * removed once JSD2 lands wholly subsumes the functionality here.
 */
JSBool
DumpStack(JSContext *cx, uintN argc, Value *vp)
{
    JSObject *arr = JS_NewArrayObject(cx, 0, NULL);
    if (!arr)
        return false;

    JSString *evalStr = JS_NewStringCopyZ(cx, "eval-code");
    if (!evalStr)
        return false;

    JSString *globalStr = JS_NewStringCopyZ(cx, "global-code");
    if (!globalStr)
        return false;

    StackIter iter(cx);
    JS_ASSERT(iter.nativeArgs().callee().getFunctionPrivate()->native() == DumpStack);
    ++iter;

    uint32 index = 0;
    for (; !iter.done(); ++index, ++iter) {
        Value v;
        if (iter.isScript()) {
            if (iter.fp()->isNonEvalFunctionFrame()) {
                if (!iter.fp()->getValidCalleeObject(cx, &v))
                    return false;
            } else if (iter.fp()->isEvalFrame()) {
                v = StringValue(evalStr);
            } else {
                v = StringValue(globalStr);
            }
        } else {
            v = iter.nativeArgs().calleev();
        }
        if (!JS_SetElement(cx, arr, index, &v))
            return false;
    }

    JS_SET_RVAL(cx, vp, ObjectValue(*arr));
    return true;
}

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
ZZ_formatter(JSContext *cx, const char *format, JSBool fromJS, jsval **vpp,
             va_list *app)
{
    jsval *vp;
    va_list ap;
    jsdouble re, im;

    printf("entering ZZ_formatter");
    vp = *vpp;
    ap = *app;
    if (fromJS) {
        if (!JS_ValueToNumber(cx, vp[0], &re))
            return JS_FALSE;
        if (!JS_ValueToNumber(cx, vp[1], &im))
            return JS_FALSE;
        *va_arg(ap, jsdouble *) = re;
        *va_arg(ap, jsdouble *) = im;
    } else {
        re = va_arg(ap, jsdouble);
        im = va_arg(ap, jsdouble);
        if (!JS_NewNumberValue(cx, re, &vp[0]))
            return JS_FALSE;
        if (!JS_NewNumberValue(cx, im, &vp[1]))
            return JS_FALSE;
    }
    *vpp = vp + 2;
    *app = ap;
    printf("leaving ZZ_formatter");
    return JS_TRUE;
}

static JSBool
ConvertArgs(JSContext *cx, uintN argc, jsval *vp)
{
    JSBool b = JS_FALSE;
    jschar c = 0;
    int32 i = 0, j = 0;
    uint32 u = 0;
    jsdouble d = 0, I = 0, re = 0, im = 0;
    JSString *str = NULL;
    jschar *w = NULL;
    JSObject *obj2 = NULL;
    JSFunction *fun = NULL;
    jsval v = JSVAL_VOID;
    JSBool ok;

    if (!JS_AddArgumentFormatter(cx, "ZZ", ZZ_formatter))
        return JS_FALSE;
    ok = JS_ConvertArguments(cx, argc, JS_ARGV(cx, vp), "b/ciujdISWofvZZ*",
                             &b, &c, &i, &u, &j, &d, &I, &str, &w, &obj2,
                             &fun, &v, &re, &im);
    JS_RemoveArgumentFormatter(cx, "ZZ");
    if (!ok)
        return JS_FALSE;
    fprintf(gOutFile,
            "b %u, c %x (%c), i %ld, u %lu, j %ld\n",
            b, c, (char)c, i, u, j);
    ToString obj2string(cx, obj2);
    ToString valueString(cx, v);
    JSAutoByteString strBytes;
    if (str)
        strBytes.encode(cx, str);
    JSString *tmpstr = JS_DecompileFunction(cx, fun, 4);
    JSAutoByteString func;
    if (!tmpstr || !func.encode(cx, tmpstr));
        ReportException(cx);
    fprintf(gOutFile,
            "d %g, I %g, S %s, W %s, obj %s, fun %s\n"
            "v %s, re %g, im %g\n",
            d, I, !!strBytes ? strBytes.ptr() : "", EscapeWideString(w),
            obj2string.getBytes(),
            fun ? (!!func ? func.ptr() : "error decompiling fun") : "",
            valueString.getBytes(), re, im);
    JS_SET_RVAL(cx, vp, JSVAL_VOID);
    return JS_TRUE;
}
#endif

static JSBool
BuildDate(JSContext *cx, uintN argc, jsval *vp)
{
    char version[20] = "\n";
#if JS_VERSION < 150
    sprintf(version, " for version %d\n", JS_VERSION);
#endif
    fprintf(gOutFile, "built on %s at %s%s", __DATE__, __TIME__, version);
    *vp = JSVAL_VOID;
    return JS_TRUE;
}

static JSBool
Clear(JSContext *cx, uintN argc, jsval *vp)
{
    JSObject *obj;
    if (argc == 0) {
        obj = JS_GetGlobalForScopeChain(cx);
        if (!obj)
            return false;
    } else if (!JS_ValueToObject(cx, JS_ARGV(cx, vp)[0], &obj)) {
        return false;
    }
    JS_ClearScope(cx, obj);
    JS_SET_RVAL(cx, vp, JSVAL_VOID);
    return true;
}

static JSBool
Intern(JSContext *cx, uintN argc, jsval *vp)
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
Clone(JSContext *cx, uintN argc, jsval *vp)
{
    JSObject *funobj, *parent, *clone;

    if (!argc) {
        JS_ReportError(cx, "Invalid arguments to clone");
        return JS_FALSE;
    }

    jsval *argv = JS_ARGV(cx, vp);
    {
        JSAutoEnterCompartment ac;
        if (!JSVAL_IS_PRIMITIVE(argv[0]) &&
            IsCrossCompartmentWrapper(JSVAL_TO_OBJECT(argv[0])))
        {
            JSObject *obj = UnwrapObject(JSVAL_TO_OBJECT(argv[0]));
            if (!ac.enter(cx, obj))
                return JS_FALSE;
            argv[0] = OBJECT_TO_JSVAL(obj);
        }
        if (!JSVAL_IS_PRIMITIVE(argv[0]) && JSVAL_TO_OBJECT(argv[0])->isFunction()) {
            funobj = JSVAL_TO_OBJECT(argv[0]);
        } else {
            JSFunction *fun = JS_ValueToFunction(cx, argv[0]);
            if (!fun)
                return JS_FALSE;
            funobj = JS_GetFunctionObject(fun);
        }
    }
    if (funobj->compartment() != cx->compartment) {
        JSFunction *fun = funobj->getFunctionPrivate();
        if (fun->isInterpreted() && fun->script()->compileAndGo) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_UNEXPECTED_TYPE,
                                 "function", "compile-and-go");
            return JS_FALSE;
        }
    }

    if (argc > 1) {
        if (!JS_ValueToObject(cx, argv[1], &parent))
            return JS_FALSE;
    } else {
        parent = JS_GetParent(cx, JSVAL_TO_OBJECT(JS_CALLEE(cx, vp)));
    }

    clone = JS_CloneFunctionObject(cx, funobj, parent);
    if (!clone)
        return JS_FALSE;
    *vp = OBJECT_TO_JSVAL(clone);
    return JS_TRUE;
}

static JSBool
GetPDA(JSContext *cx, uintN argc, jsval *vp)
{
    JSObject *vobj, *aobj, *pdobj;
    JSBool ok;
    JSPropertyDescArray pda;
    JSPropertyDesc *pd;
    jsval v;

    if (!JS_ValueToObject(cx, argc == 0 ? JSVAL_VOID : vp[2], &vobj))
        return JS_FALSE;
    if (!vobj) {
        *vp = JSVAL_VOID;
        return JS_TRUE;
    }

    aobj = JS_NewArrayObject(cx, 0, NULL);
    if (!aobj)
        return JS_FALSE;
    *vp = OBJECT_TO_JSVAL(aobj);

    ok = JS_GetPropertyDescArray(cx, vobj, &pda);
    if (!ok)
        return JS_FALSE;
    pd = pda.array;
    for (uint32 i = 0; i < pda.length; i++, pd++) {
        pdobj = JS_NewObject(cx, NULL, NULL, NULL);
        if (!pdobj) {
            ok = JS_FALSE;
            break;
        }

        /* Protect pdobj from GC by setting it as an element of aobj now */
        v = OBJECT_TO_JSVAL(pdobj);
        ok = JS_SetElement(cx, aobj, i, &v);
        if (!ok)
            break;

        ok = JS_SetProperty(cx, pdobj, "id", &pd->id) &&
             JS_SetProperty(cx, pdobj, "value", &pd->value) &&
             (v = INT_TO_JSVAL(pd->flags),
              JS_SetProperty(cx, pdobj, "flags", &v)) &&
             (v = INT_TO_JSVAL(pd->slot),
              JS_SetProperty(cx, pdobj, "slot", &v)) &&
             JS_SetProperty(cx, pdobj, "alias", &pd->alias);
        if (!ok)
            break;
    }
    JS_PutPropertyDescArray(cx, &pda);
    return ok;
}

static JSBool
GetSLX(JSContext *cx, uintN argc, jsval *vp)
{
    JSScript *script;

    script = ValueToScript(cx, argc == 0 ? JSVAL_VOID : vp[2]);
    if (!script)
        return JS_FALSE;
    *vp = INT_TO_JSVAL(js_GetScriptLineExtent(script));
    return JS_TRUE;
}

static JSBool
ToInt32(JSContext *cx, uintN argc, jsval *vp)
{
    int32 i;

    if (!JS_ValueToInt32(cx, argc == 0 ? JSVAL_VOID : vp[2], &i))
        return JS_FALSE;
    return JS_NewNumberValue(cx, i, vp);
}

static JSBool
StringsAreUTF8(JSContext *cx, uintN argc, jsval *vp)
{
    *vp = JS_CStringsAreUTF8() ? JSVAL_TRUE : JSVAL_FALSE;
    return JS_TRUE;
}

static const char* badUTF8 = "...\xC0...";
static const char* bigUTF8 = "...\xFB\xBF\xBF\xBF\xBF...";
static const jschar badSurrogate[] = { 'A', 'B', 'C', 0xDEEE, 'D', 'E', 0 };

static JSBool
TestUTF8(JSContext *cx, uintN argc, jsval *vp)
{
    int32 mode = 1;
    jschar chars[20];
    size_t charsLength = 5;
    char bytes[20];
    size_t bytesLength = 20;
    if (argc && !JS_ValueToInt32(cx, *JS_ARGV(cx, vp), &mode))
        return JS_FALSE;

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
        return JS_FALSE;
    }
    JS_SET_RVAL(cx, vp, JSVAL_VOID);
    return !JS_IsExceptionPending (cx);
}

static JSBool
ThrowError(JSContext *cx, uintN argc, jsval *vp)
{
    JS_ReportError(cx, "This is an error");
    return JS_FALSE;
}

#define LAZY_STANDARD_CLASSES

/* A class for easily testing the inner/outer object callbacks. */
typedef struct ComplexObject {
    JSBool isInner;
    JSBool frozen;
    JSObject *inner;
    JSObject *outer;
} ComplexObject;

static JSBool
sandbox_enumerate(JSContext *cx, JSObject *obj)
{
    jsval v;
    JSBool b;

    if (!JS_GetProperty(cx, obj, "lazy", &v))
        return JS_FALSE;

    JS_ValueToBoolean(cx, v, &b);
    return !b || JS_EnumerateStandardClasses(cx, obj);
}

static JSBool
sandbox_resolve(JSContext *cx, JSObject *obj, jsid id, uintN flags,
                JSObject **objp)
{
    jsval v;
    JSBool b, resolved;

    if (!JS_GetProperty(cx, obj, "lazy", &v))
        return JS_FALSE;

    JS_ValueToBoolean(cx, v, &b);
    if (b && (flags & JSRESOLVE_ASSIGNING) == 0) {
        if (!JS_ResolveStandardClass(cx, obj, id, &resolved))
            return JS_FALSE;
        if (resolved) {
            *objp = obj;
            return JS_TRUE;
        }
    }
    *objp = NULL;
    return JS_TRUE;
}

static JSClass sandbox_class = {
    "sandbox",
    JSCLASS_NEW_RESOLVE | JSCLASS_GLOBAL_FLAGS,
    JS_PropertyStub,   JS_PropertyStub,
    JS_PropertyStub,   JS_StrictPropertyStub,
    sandbox_enumerate, (JSResolveOp)sandbox_resolve,
    JS_ConvertStub,    NULL,
    JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSObject *
NewSandbox(JSContext *cx, bool lazy)
{
    JSObject *obj = JS_NewCompartmentAndGlobalObject(cx, &sandbox_class, NULL);
    if (!obj)
        return NULL;

    {
        JSAutoEnterCompartment ac;
        if (!ac.enter(cx, obj))
            return NULL;

        if (!lazy && !JS_InitStandardClasses(cx, obj))
            return NULL;

        AutoValueRooter root(cx, BooleanValue(lazy));
        if (!JS_SetProperty(cx, obj, "lazy", root.jsval_addr()))
            return NULL;
    }

    AutoObjectRooter objroot(cx, obj);
    if (!cx->compartment->wrap(cx, objroot.addr()))
        return NULL;
    return objroot.object();
}

static JSBool
EvalInContext(JSContext *cx, uintN argc, jsval *vp)
{
    JSString *str;
    JSObject *sobj = NULL;
    if (!JS_ConvertArguments(cx, argc, JS_ARGV(cx, vp), "S / o", &str, &sobj))
        return false;

    size_t srclen;
    const jschar *src = JS_GetStringCharsAndLength(cx, str, &srclen);
    if (!src)
        return false;

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

    JSStackFrame *fp = JS_GetScriptedCaller(cx, NULL);
    JSScript *script = JS_GetFrameScript(cx, fp);
    jsbytecode *pc = JS_GetFramePC(cx, fp);
    jsval rval;
    {
        JSAutoEnterCompartment ac;
        uintN flags;
        JSObject *unwrapped = UnwrapObject(sobj, &flags);
        if (flags & Wrapper::CROSS_COMPARTMENT) {
            sobj = unwrapped;
            if (!ac.enter(cx, sobj))
                return false;
        }

        OBJ_TO_INNER_OBJECT(cx, sobj);
        if (!sobj)
            return false;
        if (!(sobj->getClass()->flags & JSCLASS_IS_GLOBAL)) {
            JS_ReportError(cx, "Invalid scope argument to evalcx");
            return false;
        }
        if (!JS_EvaluateUCScript(cx, sobj, src, srclen,
                                 script->filename,
                                 JS_PCToLineNumber(cx, script, pc),
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
EvalInFrame(JSContext *cx, uintN argc, jsval *vp)
{
    jsval *argv = JS_ARGV(cx, vp);
    if (argc < 2 ||
        !JSVAL_IS_INT(argv[0]) ||
        !JSVAL_IS_STRING(argv[1])) {
        JS_ReportError(cx, "Invalid arguments to evalInFrame");
        return JS_FALSE;
    }

    uint32 upCount = JSVAL_TO_INT(argv[0]);
    JSString *str = JSVAL_TO_STRING(argv[1]);

    bool saveCurrent = (argc >= 3 && JSVAL_IS_BOOLEAN(argv[2]))
                        ? !!(JSVAL_TO_BOOLEAN(argv[2]))
                        : false;

    JS_ASSERT(cx->hasfp());

    FrameRegsIter fi(cx);
    for (uint32 i = 0; i < upCount; ++i, ++fi) {
        if (!fi.fp()->prev())
            break;
    }

    StackFrame *const fp = fi.fp();
    if (!fp->isScriptFrame()) {
        JS_ReportError(cx, "cannot eval in non-script frame");
        return JS_FALSE;
    }

    JSBool saved = JS_FALSE;;
    if (saveCurrent)
        saved = JS_SaveFrameChain(cx);

    size_t length;
    const jschar *chars = JS_GetStringCharsAndLength(cx, str, &length);
    if (!chars)
        return JS_FALSE;

    JSBool ok = JS_EvaluateUCInStackFrame(cx, Jsvalify(fp), chars, length,
                                          fp->script()->filename,
                                          JS_PCToLineNumber(cx, fp->script(),
                                                            fi.pc()),
                                          vp);

    if (saved)
        JS_RestoreFrameChain(cx);

    return ok;
}

static JSBool
ShapeOf(JSContext *cx, uintN argc, jsval *vp)
{
    jsval v;
    if (argc < 1 || !JSVAL_IS_OBJECT(v = JS_ARGV(cx, vp)[0])) {
        JS_ReportError(cx, "shapeOf: object expected");
        return JS_FALSE;
    }
    JSObject *obj = JSVAL_TO_OBJECT(v);
    if (!obj) {
        *vp = JSVAL_ZERO;
        return JS_TRUE;
    }
    if (!obj->isNative()) {
        *vp = INT_TO_JSVAL(-1);
        return JS_TRUE;
    }
    return JS_NewNumberValue(cx, obj->shape(), vp);
}

/*
 * If referent has an own property named id, copy that property to obj[id].
 * Since obj is native, this isn't totally transparent; properties of a
 * non-native referent may be simplified to data properties.
 */
static JSBool
CopyProperty(JSContext *cx, JSObject *obj, JSObject *referent, jsid id,
             uintN lookupFlags, JSObject **objp)
{
    JSProperty *prop;
    PropertyDescriptor desc;
    uintN propFlags = 0;
    JSObject *obj2;

    *objp = NULL;
    if (referent->isNative()) {
        if (!LookupPropertyWithFlags(cx, referent, id, lookupFlags, &obj2, &prop))
            return false;
        if (obj2 != referent)
            return true;

        const Shape *shape = (Shape *) prop;
        if (shape->isMethod()) {
            shape = referent->methodReadBarrier(cx, *shape, &desc.value);
            if (!shape)
                return false;
        } else if (shape->hasSlot()) {
            desc.value = referent->nativeGetSlot(shape->slot);
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
        desc.shortid = shape->shortid;
        propFlags = shape->getFlags();
   } else if (IsProxy(referent)) {
        PropertyDescriptor desc;
        if (!Proxy::getOwnPropertyDescriptor(cx, referent, id, false, &desc))
            return false;
        if (!desc.obj)
            return true;
    } else {
        if (!referent->lookupProperty(cx, id, objp, &prop))
            return false;
        if (*objp != referent)
            return true;
        if (!referent->getGeneric(cx, id, &desc.value) ||
            !referent->getAttributes(cx, id, &desc.attrs)) {
            return false;
        }
        desc.attrs &= JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT;
        desc.getter = JS_PropertyStub;
        desc.setter = JS_StrictPropertyStub;
        desc.shortid = 0;
    }

    *objp = obj;
    return !!DefineNativeProperty(cx, obj, id, desc.value, desc.getter, desc.setter,
                                  desc.attrs, propFlags, desc.shortid);
}

static JSBool
resolver_resolve(JSContext *cx, JSObject *obj, jsid id, uintN flags, JSObject **objp)
{
    jsval v;
    JS_ALWAYS_TRUE(JS_GetReservedSlot(cx, obj, 0, &v));
    return CopyProperty(cx, obj, JSVAL_TO_OBJECT(v), id, flags, objp);
}

static JSBool
resolver_enumerate(JSContext *cx, JSObject *obj)
{
    jsval v;
    JS_ALWAYS_TRUE(JS_GetReservedSlot(cx, obj, 0, &v));
    JSObject *referent = JSVAL_TO_OBJECT(v);

    AutoIdArray ida(cx, JS_Enumerate(cx, referent));
    bool ok = !!ida;
    JSObject *ignore;
    for (size_t i = 0; ok && i < ida.length(); i++)
        ok = CopyProperty(cx, obj, referent, ida[i], JSRESOLVE_QUALIFIED, &ignore);
    return ok;
}

static JSClass resolver_class = {
    "resolver",
    JSCLASS_NEW_RESOLVE | JSCLASS_HAS_RESERVED_SLOTS(1),
    JS_PropertyStub,   JS_PropertyStub,
    JS_PropertyStub,   JS_StrictPropertyStub,
    resolver_enumerate, (JSResolveOp)resolver_resolve,
    JS_ConvertStub,    NULL,
    JSCLASS_NO_OPTIONAL_MEMBERS
};


static JSBool
Resolver(JSContext *cx, uintN argc, jsval *vp)
{
    JSObject *referent, *proto = NULL;
    if (!JS_ConvertArguments(cx, argc, JS_ARGV(cx, vp), "o/o", &referent, &proto))
        return false;

    JSObject *result = (argc > 1
                        ? JS_NewObjectWithGivenProto
                        : JS_NewObject)(cx, &resolver_class, proto, JS_GetParent(cx, referent));
    if (!result)
        return false;

    JS_ALWAYS_TRUE(JS_SetReservedSlot(cx, result, 0, OBJECT_TO_JSVAL(referent)));
    JS_SET_RVAL(cx, vp, OBJECT_TO_JSVAL(result));
    return true;
}

#ifdef JS_THREADSAFE

/*
 * Check that t1 comes strictly before t2. The function correctly deals with
 * PRIntervalTime wrap-around between t2 and t1 assuming that t2 and t1 stays
 * within INT32_MAX from each other. We use MAX_TIMEOUT_INTERVAL to enforce
 * this restriction.
 */
static bool
IsBefore(PRIntervalTime t1, PRIntervalTime t2)
{
    return int32(t1 - t2) < 0;
}

static JSBool
Sleep_fn(JSContext *cx, uintN argc, jsval *vp)
{
    PRIntervalTime t_ticks;

    if (argc == 0) {
        t_ticks = 0;
    } else {
        jsdouble t_secs;

        if (!JS_ValueToNumber(cx, argc == 0 ? JSVAL_VOID : vp[2], &t_secs))
            return JS_FALSE;

        /* NB: The next condition also filter out NaNs. */
        if (!(t_secs <= MAX_TIMEOUT_INTERVAL)) {
            JS_ReportError(cx, "Excessive sleep interval");
            return JS_FALSE;
        }
        t_ticks = (t_secs <= 0.0)
                  ? 0
                  : PRIntervalTime(PR_TicksPerSecond() * t_secs);
    }
    if (t_ticks == 0) {
        JS_YieldRequest(cx);
    } else {
        JSAutoSuspendRequest suspended(cx);
        PR_Lock(gWatchdogLock);
        PRIntervalTime to_wakeup = PR_IntervalNow() + t_ticks;
        for (;;) {
            PR_WaitCondVar(gSleepWakeup, t_ticks);
            if (gCanceled)
                break;
            PRIntervalTime now = PR_IntervalNow();
            if (!IsBefore(now, to_wakeup))
                break;
            t_ticks = to_wakeup - now;
        }
        PR_Unlock(gWatchdogLock);
    }
    return !gCanceled;
}

typedef struct ScatterThreadData ScatterThreadData;
typedef struct ScatterData ScatterData;

typedef enum ScatterStatus {
    SCATTER_WAIT,
    SCATTER_GO,
    SCATTER_CANCEL
} ScatterStatus;

struct ScatterData {
    ScatterThreadData   *threads;
    jsval               *results;
    PRLock              *lock;
    PRCondVar           *cvar;
    ScatterStatus       status;
};

struct ScatterThreadData {
    jsint               index;
    ScatterData         *shared;
    PRThread            *thr;
    JSContext           *cx;
    jsval               fn;
};

static void
DoScatteredWork(JSContext *cx, ScatterThreadData *td)
{
    jsval *rval = &td->shared->results[td->index];

    if (!JS_CallFunctionValue(cx, NULL, td->fn, 0, NULL, rval)) {
        *rval = JSVAL_VOID;
        JS_GetPendingException(cx, rval);
        JS_ClearPendingException(cx);
    }
}

static void
RunScatterThread(void *arg)
{
    int stackDummy;
    ScatterThreadData *td;
    ScatterStatus st;
    JSContext *cx;

    if (PR_FAILURE == PR_SetThreadPrivate(gStackBaseThreadIndex, &stackDummy))
        return;

    td = (ScatterThreadData *)arg;
    cx = td->cx;

    /* Wait for our signal. */
    PR_Lock(td->shared->lock);
    while ((st = td->shared->status) == SCATTER_WAIT)
        PR_WaitCondVar(td->shared->cvar, PR_INTERVAL_NO_TIMEOUT);
    PR_Unlock(td->shared->lock);

    if (st == SCATTER_CANCEL)
        return;

    /* We are good to go. */
    JS_SetContextThread(cx);
    JS_SetNativeStackQuota(cx, gMaxStackSize);
    JS_BeginRequest(cx);
    DoScatteredWork(cx, td);
    JS_EndRequest(cx);
    JS_ClearContextThread(cx);
}

/*
 * scatter(fnArray) - Call each function in `fnArray` without arguments, each
 * in a different thread. When all threads have finished, return an array: the
 * return values. Errors are not propagated; if any of the function calls
 * fails, the corresponding element in the results array gets the exception
 * object, if any, else (undefined).
 */
static JSBool
Scatter(JSContext *cx, uintN argc, jsval *vp)
{
    jsuint i;
    jsuint n;  /* number of threads */
    JSObject *inArr;
    JSObject *arr;
    JSObject *global;
    ScatterData sd;
    JSBool ok;

    sd.lock = NULL;
    sd.cvar = NULL;
    sd.results = NULL;
    sd.threads = NULL;
    sd.status = SCATTER_WAIT;

    if (argc == 0 || JSVAL_IS_PRIMITIVE(JS_ARGV(cx, vp)[0])) {
        JS_ReportError(cx, "the first argument must be an object");
        goto fail;
    }

    inArr = JSVAL_TO_OBJECT(JS_ARGV(cx, vp)[0]);
    ok = JS_GetArrayLength(cx, inArr, &n);
    if (!ok)
        goto out;
    if (n == 0)
        goto success;

    sd.lock = PR_NewLock();
    if (!sd.lock)
        goto fail;

    sd.cvar = PR_NewCondVar(sd.lock);
    if (!sd.cvar)
        goto fail;

    sd.results = (jsval *) malloc(n * sizeof(jsval));
    if (!sd.results)
        goto fail;
    for (i = 0; i < n; i++) {
        sd.results[i] = JSVAL_VOID;
        ok = JS_AddValueRoot(cx, &sd.results[i]);
        if (!ok) {
            while (i-- > 0)
                JS_RemoveValueRoot(cx, &sd.results[i]);
            free(sd.results);
            sd.results = NULL;
            goto fail;
        }
    }

    sd.threads = (ScatterThreadData *) malloc(n * sizeof(ScatterThreadData));
    if (!sd.threads)
        goto fail;
    for (i = 0; i < n; i++) {
        sd.threads[i].index = i;
        sd.threads[i].shared = &sd;
        sd.threads[i].thr = NULL;
        sd.threads[i].cx = NULL;
        sd.threads[i].fn = JSVAL_NULL;

        ok = JS_AddValueRoot(cx, &sd.threads[i].fn);
        if (ok && !JS_GetElement(cx, inArr, i, &sd.threads[i].fn)) {
            JS_RemoveValueRoot(cx, &sd.threads[i].fn);
            ok = JS_FALSE;
        }
        if (!ok) {
            while (i-- > 0)
                JS_RemoveValueRoot(cx, &sd.threads[i].fn);
            free(sd.threads);
            sd.threads = NULL;
            goto fail;
        }
    }

    global = JS_GetGlobalObject(cx);
    for (i = 1; i < n; i++) {
        JSContext *newcx = NewContext(JS_GetRuntime(cx));
        if (!newcx)
            goto fail;

        {
            JSAutoRequest req(newcx);
            JS_SetGlobalObject(newcx, global);
        }
        JS_ClearContextThread(newcx);
        sd.threads[i].cx = newcx;
    }

    for (i = 1; i < n; i++) {
        PRThread *t = PR_CreateThread(PR_USER_THREAD,
                                      RunScatterThread,
                                      &sd.threads[i],
                                      PR_PRIORITY_NORMAL,
                                      PR_GLOBAL_THREAD,
                                      PR_JOINABLE_THREAD,
                                      0);
        if (!t) {
            /* Failed to start thread. */
            PR_Lock(sd.lock);
            sd.status = SCATTER_CANCEL;
            PR_NotifyAllCondVar(sd.cvar);
            PR_Unlock(sd.lock);
            while (i-- > 1)
                PR_JoinThread(sd.threads[i].thr);
            goto fail;
        }

        sd.threads[i].thr = t;
    }
    PR_Lock(sd.lock);
    sd.status = SCATTER_GO;
    PR_NotifyAllCondVar(sd.cvar);
    PR_Unlock(sd.lock);

    DoScatteredWork(cx, &sd.threads[0]);

    {
        JSAutoSuspendRequest suspended(cx);
        for (i = 1; i < n; i++) {
            PR_JoinThread(sd.threads[i].thr);
        }
    }

success:
    arr = JS_NewArrayObject(cx, n, sd.results);
    if (!arr)
        goto fail;
    *vp = OBJECT_TO_JSVAL(arr);
    ok = JS_TRUE;

out:
    if (sd.threads) {
        JSContext *acx;

        for (i = 0; i < n; i++) {
            JS_RemoveValueRoot(cx, &sd.threads[i].fn);
            acx = sd.threads[i].cx;
            if (acx) {
                JS_SetContextThread(acx);
                DestroyContext(acx, true);
            }
        }
        free(sd.threads);
    }
    if (sd.results) {
        for (i = 0; i < n; i++)
            JS_RemoveValueRoot(cx, &sd.results[i]);
        free(sd.results);
    }
    if (sd.cvar)
        PR_DestroyCondVar(sd.cvar);
    if (sd.lock)
        PR_DestroyLock(sd.lock);

    return ok;

fail:
    ok = JS_FALSE;
    goto out;
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
    JSRuntime *rt = (JSRuntime *) arg;

    PR_Lock(gWatchdogLock);
    while (gWatchdogThread) {
        PRIntervalTime now = PR_IntervalNow();
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
            PRIntervalTime sleepDuration = gWatchdogHasTimeout
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
ScheduleWatchdog(JSRuntime *rt, jsdouble t)
{
    if (t <= 0) {
        PR_Lock(gWatchdogLock);
        gWatchdogHasTimeout = false;
        PR_Unlock(gWatchdogLock);
        return true;
    }

    PRIntervalTime interval = PRIntervalTime(ceil(t * PR_TicksPerSecond()));
    PRIntervalTime timeout = PR_IntervalNow() + interval;
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
ScheduleWatchdog(JSRuntime *rt, jsdouble t)
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
#ifdef JS_THREADSAFE
    if (gWorkerThreadPool)
        js::workers::terminateAll(rt, gWorkerThreadPool);
#endif
    JS_TriggerAllOperationCallbacks(rt);

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
SetTimeoutValue(JSContext *cx, jsdouble t)
{
    /* NB: The next condition also filter out NaNs. */
    if (!(t <= MAX_TIMEOUT_INTERVAL)) {
        JS_ReportError(cx, "Excessive timeout value");
        return JS_FALSE;
    }
    gTimeoutInterval = t;
    if (!ScheduleWatchdog(cx->runtime, t)) {
        JS_ReportError(cx, "Failed to create the watchdog");
        return JS_FALSE;
    }
    return JS_TRUE;
}

static JSBool
Timeout(JSContext *cx, uintN argc, jsval *vp)
{
    if (argc == 0)
        return JS_NewNumberValue(cx, gTimeoutInterval, vp);

    if (argc > 1) {
        JS_ReportError(cx, "Wrong number of arguments");
        return JS_FALSE;
    }

    jsdouble t;
    if (!JS_ValueToNumber(cx, JS_ARGV(cx, vp)[0], &t))
        return JS_FALSE;

    *vp = JSVAL_VOID;
    return SetTimeoutValue(cx, t);
}

static JSBool
Elapsed(JSContext *cx, uintN argc, jsval *vp)
{
    if (argc == 0) {
        double d = 0.0;
        JSShellContextData *data = GetContextData(cx);
        if (data)
            d = js_IntervalNow() - data->startTime;
        return JS_NewNumberValue(cx, d, vp);
    }
    JS_ReportError(cx, "Wrong number of arguments");
    return JS_FALSE;
}

static JSBool
Parent(JSContext *cx, uintN argc, jsval *vp)
{
    if (argc != 1) {
        JS_ReportError(cx, "Wrong number of arguments");
        return JS_FALSE;
    }

    jsval v = JS_ARGV(cx, vp)[0];
    if (JSVAL_IS_PRIMITIVE(v)) {
        JS_ReportError(cx, "Only objects have parents!");
        return JS_FALSE;
    }

    JSObject *parent = JS_GetParent(cx, JSVAL_TO_OBJECT(v));
    *vp = OBJECT_TO_JSVAL(parent);

    /* Outerize if necessary.  Embrace the ugliness! */
    if (parent) {
        if (JSObjectOp op = parent->getClass()->ext.outerObject)
            *vp = OBJECT_TO_JSVAL(op(cx, parent));
    }

    return JS_TRUE;
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
Compile(JSContext *cx, uintN argc, jsval *vp)
{
    if (argc < 1) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_MORE_ARGS_NEEDED,
                             "compile", "0", "s");
        return JS_FALSE;
    }
    jsval arg0 = JS_ARGV(cx, vp)[0];
    if (!JSVAL_IS_STRING(arg0)) {
        const char *typeName = JS_GetTypeName(cx, JS_TypeOfValue(cx, arg0));
        JS_ReportError(cx, "expected string to compile, got %s", typeName);
        return JS_FALSE;
    }

    static JSClass dummy_class = {
        "jdummy",
        JSCLASS_GLOBAL_FLAGS,
        JS_PropertyStub,  JS_PropertyStub,
        JS_PropertyStub,  JS_StrictPropertyStub,
        JS_EnumerateStub, JS_ResolveStub,
        JS_ConvertStub,   NULL,
        JSCLASS_NO_OPTIONAL_MEMBERS
    };

    JSObject *fakeGlobal = JS_NewGlobalObject(cx, &dummy_class);
    if (!fakeGlobal)
        return JS_FALSE;

    JSString *scriptContents = JSVAL_TO_STRING(arg0);

    uintN oldopts = JS_GetOptions(cx);
    JS_SetOptions(cx, oldopts | JSOPTION_COMPILE_N_GO | JSOPTION_NO_SCRIPT_RVAL);
    bool ok = JS_CompileUCScript(cx, fakeGlobal, JS_GetStringCharsZ(cx, scriptContents),
                                 JS_GetStringLength(scriptContents), "<string>", 0);
    JS_SetOptions(cx, oldopts);

    JS_SET_RVAL(cx, vp, JSVAL_VOID);
    return ok;
}

static JSBool
Parse(JSContext *cx, uintN argc, jsval *vp)
{
    if (argc < 1) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_MORE_ARGS_NEEDED,
                             "compile", "0", "s");
        return JS_FALSE;
    }
    jsval arg0 = JS_ARGV(cx, vp)[0];
    if (!JSVAL_IS_STRING(arg0)) {
        const char *typeName = JS_GetTypeName(cx, JS_TypeOfValue(cx, arg0));
        JS_ReportError(cx, "expected string to parse, got %s", typeName);
        return JS_FALSE;
    }

    JSString *scriptContents = JSVAL_TO_STRING(arg0);
    js::Parser parser(cx);
    parser.init(JS_GetStringCharsZ(cx, scriptContents), JS_GetStringLength(scriptContents),
                "<string>", 0, cx->findVersion());
    if (!parser.parse(NULL))
        return JS_FALSE;
    JS_SET_RVAL(cx, vp, JSVAL_VOID);
    return JS_TRUE;
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
Snarf(JSContext *cx, uintN argc, jsval *vp)
{
    JSString *str;

    if (!argc)
        return JS_FALSE;

    str = JS_ValueToString(cx, JS_ARGV(cx, vp)[0]);
    if (!str)
        return JS_FALSE;
    JSAutoByteString filename(cx, str);
    if (!filename)
        return JS_FALSE;

    /* Get the currently executing script's name. */
    JSStackFrame *fp = JS_GetScriptedCaller(cx, NULL);
    JSScript *script = JS_GetFrameScript(cx, fp);
    JS_ASSERT(fp && script->filename);
    const char *pathname = filename.ptr();
#ifdef XP_UNIX
    FreeOnReturn pnGuard(cx);
    if (pathname[0] != '/') {
        pathname = MakeAbsolutePathname(cx, script->filename, pathname);
        if (!pathname)
            return JS_FALSE;
        pnGuard.init(pathname);
    }
#endif

    if (argc > 1) {
        JSString *opt = JS_ValueToString(cx, JS_ARGV(cx, vp)[1]);
        if (!opt)
            return JS_FALSE;
        JSBool match;
        if (!JS_StringEqualsAscii(cx, opt, "binary", &match))
            return JS_FALSE;
        if (match) {
            JSObject *obj;
            if (!(obj = FileAsTypedArray(cx, pathname)))
                return JS_FALSE;
            *vp = OBJECT_TO_JSVAL(obj);
            return JS_TRUE;
        }
    }

    if (!(str = FileAsString(cx, pathname)))
        return JS_FALSE;
    *vp = STRING_TO_JSVAL(str);
    return JS_TRUE;
}

JSBool
Wrap(JSContext *cx, uintN argc, jsval *vp)
{
    jsval v = argc > 0 ? JS_ARGV(cx, vp)[0] : JSVAL_VOID;
    if (JSVAL_IS_PRIMITIVE(v)) {
        JS_SET_RVAL(cx, vp, v);
        return true;
    }

    JSObject *obj = JSVAL_TO_OBJECT(v);
    JSObject *wrapped = Wrapper::New(cx, obj, obj->getProto(), obj->getGlobal(),
                                     &Wrapper::singleton);
    if (!wrapped)
        return false;

    JS_SET_RVAL(cx, vp, OBJECT_TO_JSVAL(wrapped));
    return true;
}

JSBool
Serialize(JSContext *cx, uintN argc, jsval *vp)
{
    jsval v = argc > 0 ? JS_ARGV(cx, vp)[0] : JSVAL_VOID;
    uint64 *datap;
    size_t nbytes;
    if (!JS_WriteStructuredClone(cx, v, &datap, &nbytes, NULL, NULL))
        return false;

    JSObject *arrayobj = js_CreateTypedArray(cx, TypedArray::TYPE_UINT8, nbytes);
    if (!arrayobj) {
        JS_free(cx, datap);
        return false;
    }
    JSObject *array = TypedArray::getTypedArray(arrayobj);
    JS_ASSERT((uintptr_t(TypedArray::getDataOffset(array)) & 7) == 0);
    memcpy(TypedArray::getDataOffset(array), datap, nbytes);
    JS_free(cx, datap);
    JS_SET_RVAL(cx, vp, OBJECT_TO_JSVAL(arrayobj));
    return true;
}

JSBool
Deserialize(JSContext *cx, uintN argc, jsval *vp)
{
    jsval v = argc > 0 ? JS_ARGV(cx, vp)[0] : JSVAL_VOID;
    JSObject *obj;
    if (JSVAL_IS_PRIMITIVE(v) || !js_IsTypedArray((obj = JSVAL_TO_OBJECT(v)))) {
        JS_ReportErrorNumber(cx, my_GetErrorMessage, NULL, JSSMSG_INVALID_ARGS, "deserialize");
        return false;
    }
    JSObject *array = TypedArray::getTypedArray(obj);
    if ((TypedArray::getByteLength(array) & 7) != 0) {
        JS_ReportErrorNumber(cx, my_GetErrorMessage, NULL, JSSMSG_INVALID_ARGS, "deserialize");
        return false;
    }
    if ((uintptr_t(TypedArray::getDataOffset(array)) & 7) != 0) {
        JS_ReportErrorNumber(cx, my_GetErrorMessage, NULL, JSSMSG_BAD_ALIGNMENT);
        return false;
    }

    if (!JS_ReadStructuredClone(cx, (uint64 *) TypedArray::getDataOffset(array), TypedArray::getByteLength(array),
                                JS_STRUCTURED_CLONE_VERSION, &v, NULL, NULL)) {
        return false;
    }
    JS_SET_RVAL(cx, vp, v);
    return true;
}

JSBool
MJitCodeStats(JSContext *cx, uintN argc, jsval *vp)
{
#ifdef JS_METHODJIT
    JSRuntime *rt = cx->runtime;
    AutoLockGC lock(rt);
    size_t n = 0, method, regexp, unused;
    for (JSCompartment **c = rt->compartments.begin(); c != rt->compartments.end(); ++c)
    {
        (*c)->getMjitCodeStats(method, regexp, unused);
        n += method + regexp + unused;
    }
    JS_SET_RVAL(cx, vp, INT_TO_JSVAL(n));
#else
    JS_SET_RVAL(cx, vp, JSVAL_VOID);
#endif
    return true;
}

#ifdef JS_METHODJIT

static size_t
zero_usable_size(void *p)
{
    return 0;
}

static void
SumJitDataSizeCallback(JSContext *cx, void *data, void *thing,
                       JSGCTraceKind traceKind, size_t thingSize)
{
    size_t *sump = static_cast<size_t *>(data);
    JS_ASSERT(traceKind == JSTRACE_SCRIPT);
    JSScript *script = static_cast<JSScript *>(thing);
    /*
     * Passing in zero_usable_size causes jitDataSize to fall back to its
     * secondary size computation.
     */
    *sump += script->jitDataSize(zero_usable_size);
}

#endif

JSBool
MJitDataStats(JSContext *cx, uintN argc, jsval *vp)
{
#ifdef JS_METHODJIT
    size_t n = 0;
    IterateCells(cx, NULL, gc::FINALIZE_SCRIPT, &n, SumJitDataSizeCallback);
    JS_SET_RVAL(cx, vp, INT_TO_JSVAL(n));
#else
    JS_SET_RVAL(cx, vp, JSVAL_VOID);
#endif
    return true;
}

JSBool
StringStats(JSContext *cx, uintN argc, jsval *vp)
{
    // XXX: should report something meaningful;  bug 625305 will probably fix
    // this.
    JS_SET_RVAL(cx, vp, INT_TO_JSVAL(0));
    return true;
}

enum CompartmentKind { SAME_COMPARTMENT, NEW_COMPARTMENT };

static JSObject *
NewGlobalObject(JSContext *cx, CompartmentKind compartment);

JSBool
NewGlobal(JSContext *cx, uintN argc, jsval *vp)
{
    if (argc != 1 || !JSVAL_IS_STRING(JS_ARGV(cx, vp)[0])) {
        JS_ReportErrorNumber(cx, my_GetErrorMessage, NULL, JSSMSG_INVALID_ARGS, "newGlobal");
        return false;
    }

    JSString *str = JSVAL_TO_STRING(JS_ARGV(cx, vp)[0]);

    JSBool equalSame = JS_FALSE, equalNew = JS_FALSE;
    if (!JS_StringEqualsAscii(cx, str, "same-compartment", &equalSame) ||
        !JS_StringEqualsAscii(cx, str, "new-compartment", &equalNew)) {
        return false;
    }

    if (!equalSame && !equalNew) {
        JS_ReportErrorNumber(cx, my_GetErrorMessage, NULL, JSSMSG_INVALID_ARGS, "newGlobal");
        return false;
    }

    JSObject *global = NewGlobalObject(cx, equalSame ? SAME_COMPARTMENT : NEW_COMPARTMENT);
    if (!global)
        return false;

    JS_SET_RVAL(cx, vp, OBJECT_TO_JSVAL(global));
    return true;
}

static JSBool
ParseLegacyJSON(JSContext *cx, uintN argc, jsval *vp)
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
EnableStackWalkingAssertion(JSContext *cx, uintN argc, jsval *vp)
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
GetMaxArgs(JSContext *cx, uintN arg, jsval *vp)
{
    JS_SET_RVAL(cx, vp, INT_TO_JSVAL(StackSpace::ARGS_LENGTH_MAX));
    return JS_TRUE;
}

static JSFunctionSpec shell_functions[] = {
    JS_FN("version",        Version,        0,0),
    JS_FN("revertVersion",  RevertVersion,  0,0),
    JS_FN("options",        Options,        0,0),
    JS_FN("load",           Load,           1,0),
    JS_FN("evaluate",       Evaluate,       1,0),
    JS_FN("run",            Run,            1,0),
    JS_FN("readline",       ReadLine,       0,0),
    JS_FN("print",          Print,          0,0),
    JS_FN("putstr",         PutStr,         0,0),
    JS_FN("dateNow",        Now,            0,0),
    JS_FN("help",           Help,           0,0),
    JS_FN("quit",           Quit,           0,0),
    JS_FN("assertEq",       AssertEq,       2,0),
    JS_FN("assertJit",      AssertJit,      0,0),
    JS_FN("gc",             ::GC,           0,0),
    JS_FN("gcparam",        GCParameter,    2,0),
    JS_FN("countHeap",      CountHeap,      0,0),
    JS_FN("makeFinalizeObserver", MakeFinalizeObserver, 0,0),
    JS_FN("finalizeCount",  FinalizeCount,  0,0),
#ifdef JS_GC_ZEAL
    JS_FN("gczeal",         GCZeal,         2,0),
    JS_FN("schedulegc",     ScheduleGC,     1,0),
#endif
    JS_FN("internalConst",  InternalConst,  1,0),
    JS_FN("setDebug",       SetDebug,       1,0),
    JS_FN("setDebuggerHandler", SetDebuggerHandler, 1,0),
    JS_FN("setThrowHook",   SetThrowHook,   1,0),
    JS_FN("trap",           Trap,           3,0),
    JS_FN("untrap",         Untrap,         2,0),
    JS_FN("line2pc",        LineToPC,       0,0),
    JS_FN("pc2line",        PCToLine,       0,0),
    JS_FN("stringsAreUTF8", StringsAreUTF8, 0,0),
    JS_FN("testUTF8",       TestUTF8,       1,0),
    JS_FN("throwError",     ThrowError,     0,0),
#ifdef DEBUG
    JS_FN("disassemble",    DisassembleToString, 1,0),
    JS_FN("dis",            Disassemble,    1,0),
    JS_FN("disfile",        DisassFile,     1,0),
    JS_FN("dissrc",         DisassWithSrc,  1,0),
    JS_FN("dumpHeap",       DumpHeap,       0,0),
    JS_FN("dumpObject",     DumpObject,     1,0),
    JS_FN("notes",          Notes,          1,0),
    JS_FN("stats",          DumpStats,      1,0),
    JS_FN("findReferences", FindReferences, 1,0),
#endif
    JS_FN("dumpStack",      DumpStack,      1,0),
#ifdef TEST_CVTARGS
    JS_FN("cvtargs",        ConvertArgs,    0,0),
#endif
    JS_FN("build",          BuildDate,      0,0),
    JS_FN("clear",          Clear,          0,0),
    JS_FN("intern",         Intern,         1,0),
    JS_FN("clone",          Clone,          1,0),
    JS_FN("getpda",         GetPDA,         1,0),
    JS_FN("getslx",         GetSLX,         1,0),
    JS_FN("toint32",        ToInt32,        1,0),
    JS_FN("evalcx",         EvalInContext,  1,0),
    JS_FN("evalInFrame",    EvalInFrame,    2,0),
    JS_FN("shapeOf",        ShapeOf,        1,0),
    JS_FN("resolver",       Resolver,       1,0),
#ifdef MOZ_TRACEVIS
    JS_FN("startTraceVis",  StartTraceVisNative, 1,0),
    JS_FN("stopTraceVis",   StopTraceVisNative,  0,0),
#endif
#ifdef DEBUG
    JS_FN("arrayInfo",      js_ArrayInfo,   1,0),
#endif
#ifdef JS_THREADSAFE
    JS_FN("sleep",          Sleep_fn,       1,0),
    JS_FN("scatter",        Scatter,        1,0),
#endif
    JS_FN("snarf",          Snarf,          0,0),
    JS_FN("read",           Snarf,          0,0),
    JS_FN("compile",        Compile,        1,0),
    JS_FN("parse",          Parse,          1,0),
    JS_FN("timeout",        Timeout,        1,0),
    JS_FN("elapsed",        Elapsed,        0,0),
    JS_FN("parent",         Parent,         1,0),
    JS_FN("wrap",           Wrap,           1,0),
    JS_FN("serialize",      Serialize,      1,0),
    JS_FN("deserialize",    Deserialize,    1,0),
#ifdef JS_METHODJIT
    JS_FN("mjitcodestats",  MJitCodeStats,  0,0),
    JS_FN("mjitdatastats",  MJitDataStats,  0,0),
#endif
    JS_FN("stringstats",    StringStats,    0,0),
    JS_FN("newGlobal",      NewGlobal,      1,0),
    JS_FN("parseLegacyJSON",ParseLegacyJSON,1,0),
    JS_FN("enableStackWalkingAssertion",EnableStackWalkingAssertion,1,0),
    JS_FN("getMaxArgs",     GetMaxArgs,     0,0),
    JS_FS_END
};

static const char shell_help_header[] =
"Command                  Description\n"
"=======                  ===========\n";

static const char *const shell_help_messages[] = {
"version([number])        Get or force a script compilation version number",
"revertVersion()          Revert previously set version number",
"options([option ...])    Get or toggle JavaScript options",
"load(['foo.js' ...])     Load files named by string arguments",
"evaluate(code)           Evaluate code as though it were the contents of a file",
"run('foo.js')\n"
"  Run the file named by the first argument, returning the number of\n"
"  of milliseconds spent compiling and executing it",
"readline()               Read a single line from stdin",
"print([exp ...])         Evaluate and print expressions",
"putstr([exp])            Evaluate and print expression without newline",
"dateNow()                    Return the current time with sub-ms precision",
"help([name ...])         Display usage and help messages",
"quit()                   Quit the shell",
"assertEq(actual, expected[, msg])\n"
"  Throw if the first two arguments are not the same (both +0 or both -0,\n"
"  both NaN, or non-zero and ===)",
"assertJit()              Throw if the calling function failed to JIT",
"gc([obj])                Run the garbage collector\n"
"                         When obj is given, GC only the compartment it's in",
"gcparam(name, value)\n"
"  Wrapper for JS_SetGCParameter. The name must be either 'maxBytes' or\n"
"  'maxMallocBytes' and the value must be convertable to a positive uint32",
"countHeap([start[, kind]])\n"
"  Count the number of live GC things in the heap or things reachable from\n"
"  start when it is given and is not null. kind is either 'all' (default) to\n"
"  count all things or one of 'object', 'double', 'string', 'function',\n"
"  'qname', 'namespace', 'xml' to count only things of that kind",
"makeFinalizeObserver()\n"
"  get a special object whose finalization increases the counter returned\n"
"  by the finalizeCount function",
"finalizeCount()\n"
"  return the current value of the finalization counter that is incremented\n"
"  each time an object returned by the makeFinalizeObserver is finalized",
#ifdef JS_GC_ZEAL
"gczeal(level, [freq], [compartmentGC?])\n"
"                         How zealous the garbage collector should be",
"schedulegc(num, [compartmentGC?])\n"
"                         Schedule a GC to happen after num allocations",
#endif
"internalConst(name)\n"
"  Query an internal constant for the engine. See InternalConst source for the\n"
"  list of constant names",
"setDebug(debug)          Set debug mode",
"setDebuggerHandler(f)    Set handler for debugger keyword to f",
"setThrowHook(f)          Set throw hook to f",
"trap([fun, [pc,]] exp)   Trap bytecode execution",
"untrap(fun[, pc])        Remove a trap",
"line2pc([fun,] line)     Map line number to PC",
"pc2line(fun[, pc])       Map PC to line number",
"stringsAreUTF8()         Check if strings are UTF-8 encoded",
"testUTF8(mode)           Perform UTF-8 tests (modes are 1 to 4)",
"throwError()             Throw an error from JS_ReportError",
#ifdef DEBUG
"disassemble([fun])       Return the disassembly for the given function",
"dis([fun])               Disassemble functions into bytecodes",
"disfile('foo.js')        Disassemble script file into bytecodes\n"
"  dis and disfile take these options as preceeding string arguments\n"
"    \"-r\" (disassemble recursively)\n"
"    \"-l\" (show line numbers)",
"dissrc([fun])            Disassemble functions with source lines",
"dumpHeap([fileName[, start[, toFind[, maxDepth[, toIgnore]]]]])\n"
"  Interface to JS_DumpHeap with output sent to file",
"dumpObject()             Dump an internal representation of an object",
"notes([fun])             Show source notes for functions",
"stats([string ...])      Dump 'arena', 'atom', 'global' stats",
"findReferences(target)\n"
"  Walk the heap and return an object describing all references to target",
#endif
"dumpStack()              Dump the stack as an array of callees (youngest first)",
#ifdef TEST_CVTARGS
"cvtargs(arg1..., arg12)  Test argument formatter",
#endif
"build()                  Show build date and time",
"clear([obj])             Clear properties of object",
"intern(str)              Internalize str in the atom table",
"clone(fun[, scope])      Clone function object",
"getpda(obj)              Get the property descriptors for obj",
"getslx(obj)              Get script line extent",
"toint32(n)               Testing hook for JS_ValueToInt32",
"evalcx(s[, o])\n"
"  Evaluate s in optional sandbox object o\n"
"  if (s == '' && !o) return new o with eager standard classes\n"
"  if (s == 'lazy' && !o) return new o with lazy standard classes",
"evalInFrame(n,str,save)  Evaluate 'str' in the nth up frame.\n"
"                         If 'save' (default false), save the frame chain",
"shapeOf(obj)             Get the shape of obj (an implementation detail)",
"resolver(src[, proto])   Create object with resolve hook that copies properties\n"
"                         from src. If proto is omitted, use Object.prototype.",
#ifdef MOZ_TRACEVIS
"startTraceVis(filename)  Start TraceVis recording (stops any current recording)",
"stopTraceVis()           Stop TraceVis recording",
#endif
#ifdef DEBUG
"arrayInfo(a1, a2, ...)   Report statistics about arrays",
#endif
#ifdef JS_THREADSAFE
"sleep(dt)                Sleep for dt seconds",
"scatter(fns)             Call functions concurrently (ignoring errors)",
#endif
"snarf(filename)          Read filename into returned string",
"read(filename)           Synonym for snarf",
"compile(code)            Compiles a string to bytecode, potentially throwing",
"parse(code)              Parses a string, potentially throwing",
"timeout([seconds])\n"
"  Get/Set the limit in seconds for the execution time for the current context.\n"
"  A negative value (default) means that the execution time is unlimited.",
"elapsed()                Execution time elapsed for the current context.",
"parent(obj)              Returns the parent of obj.",
"wrap(obj)                Wrap an object into a noop wrapper.",
"serialize(sd)            Serialize sd using JS_WriteStructuredClone. Returns a TypedArray.",
"deserialize(a)           Deserialize data generated by serialize.",
#ifdef JS_METHODJIT
"mjitcodestats()          Return stats on mjit code memory usage.",
"mjitdatastats()          Return stats on mjit data memory usage.",
#endif
"stringstats()            Return stats on string memory usage.",
"newGlobal(kind)          Return a new global object, in the current\n"
"                         compartment if kind === 'same-compartment' or in a\n"
"                         new compartment if kind === 'new-compartment'",
"parseLegacyJSON(str)     Parse str as legacy JSON, returning the result if the\n"
"                         parse succeeded and throwing a SyntaxError if not.",
"enableStackWalkingAssertion(enabled)\n"
"  Enables or disables a particularly expensive assertion in stack-walking\n"
"  code.  If your test isn't ridiculously thorough, such that performing this\n"
"  assertion increases test duration by an order of magnitude, you shouldn't\n"
"  use this.",
"getMaxArgs()             Return the maximum number of supported args for a call.",

/* Keep these last: see the static assertion below. */
#ifdef MOZ_PROFILING
"startProfiling([profileName])\n"
"                         Start a profiling session\n"
"                         Profiler must be running with programatic sampling",
"stopProfiling([profileName])\n"
"                         Stop a running profiling session",
"pauseProfilers([profileName])\n"
"                         Pause a running profiling session",
"resumeProfilers([profileName])\n"
"                         Resume a paused profiling session",
"dumpProfile([outfile[, profileName]])\n"
"                         Dump out current profile info (only valid for callgrind)",
# ifdef MOZ_CALLGRIND
"startCallgrind()         Start Callgrind instrumentation",
"stopCallgrind()          Stop Callgrind instrumentation",
"dumpCallgrind([outfile]) Dump current Callgrind counters to file or stdout",
# endif
# ifdef MOZ_VTUNE
"startVtune()             Start Vtune instrumentation",
"stopVtune()              Stop Vtune instrumentation",
"pauseVtune()             Pause Vtune collection",
"resumeVtune()            Resume Vtune collection",
# endif
#endif
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

/* Help messages must match shell functions. */
JS_STATIC_ASSERT(JS_ARRAY_LENGTH(shell_help_messages) - EXTERNAL_FUNCTION_COUNT ==
                 JS_ARRAY_LENGTH(shell_functions) - 1 /* JS_FS_END */);

#ifdef DEBUG
static void
CheckHelpMessages()
{
    const char *const *m;
    const char *lp;

    /* Messages begin with "function_name(" prefix and don't end with \n. */
    for (m = shell_help_messages; m < ArrayEnd(shell_help_messages) - EXTERNAL_FUNCTION_COUNT; ++m) {
        lp = strchr(*m, '(');
        JS_ASSERT(lp);
        JS_ASSERT(memcmp(shell_functions[m - shell_help_messages].name,
                         *m, lp - *m) == 0);
        JS_ASSERT((*m)[strlen(*m) - 1] != '\n');
    }
}
#else
# define CheckHelpMessages() ((void) 0)
#endif

#undef PROFILING_FUNCTION_COUNT
#undef CALLGRIND_FUNCTION_COUNT
#undef VTUNE_FUNCTION_COUNT
#undef EXTERNAL_FUNCTION_COUNT

static JSBool
Help(JSContext *cx, uintN argc, jsval *vp)
{
    uintN i, j;
    int did_header, did_something;
    JSType type;
    JSFunction *fun;
    JSString *str;

    fprintf(gOutFile, "%s\n", JS_GetImplementationVersion());
    if (argc == 0) {
        fputs(shell_help_header, gOutFile);
        for (i = 0; i < ArrayLength(shell_help_messages); ++i)
            fprintf(gOutFile, "%s\n", shell_help_messages[i]);
    } else {
        did_header = 0;
        jsval *argv = JS_ARGV(cx, vp);
        for (i = 0; i < argc; i++) {
            did_something = 0;
            type = JS_TypeOfValue(cx, argv[i]);
            if (type == JSTYPE_FUNCTION) {
                fun = JS_ValueToFunction(cx, argv[i]);
                str = fun->atom;
            } else if (type == JSTYPE_STRING) {
                str = JSVAL_TO_STRING(argv[i]);
            } else {
                str = NULL;
            }
            if (str) {
                JSAutoByteString funcName(cx, str);
                if (!funcName)
                    return JS_FALSE;
                for (j = 0; j < ArrayLength(shell_help_messages); ++j) {
                    /* Help messages are required to be formatted "functionName(..." */
                    const char *msg = shell_help_messages[j];
                    const char *p = strchr(msg, '(');
                    JS_ASSERT(p);

                    if (size_t(p - msg) != str->length())
                        continue;

                    if (strncmp(funcName.ptr(), msg, p - msg) == 0) {
                        if (!did_header) {
                            did_header = 1;
                            fputs(shell_help_header, gOutFile);
                        }
                        did_something = 1;
                        fprintf(gOutFile, "%s\n", shell_help_messages[j]);
                        break;
                    }
                }
            }
            if (!did_something) {
                str = JS_ValueToString(cx, argv[i]);
                if (!str)
                    return JS_FALSE;
                fputs("Sorry, no help for ", gErrFile);
                JS_FileEscapedString(gErrFile, str, 0);
                putc('\n', gErrFile);
            }
        }
    }
    JS_SET_RVAL(cx, vp, JSVAL_VOID);
    return JS_TRUE;
}

/*
 * Define a JS object called "it".  Give it class operations that printf why
 * they're being called for tutorial purposes.
 */
enum its_tinyid {
    ITS_COLOR, ITS_HEIGHT, ITS_WIDTH, ITS_FUNNY, ITS_ARRAY, ITS_RDONLY,
    ITS_CUSTOM, ITS_CUSTOMRDONLY
};

static JSBool
its_getter(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
  jsval *val = (jsval *) JS_GetPrivate(cx, obj);
  *vp = val ? *val : JSVAL_VOID;
  return JS_TRUE;
}

static JSBool
its_setter(JSContext *cx, JSObject *obj, jsid id, JSBool strict, jsval *vp)
{
  jsval *val = (jsval *) JS_GetPrivate(cx, obj);
  if (val) {
      *val = *vp;
      return JS_TRUE;
  }

  val = new jsval;
  if (!val) {
      JS_ReportOutOfMemory(cx);
      return JS_FALSE;
  }

  if (!JS_AddValueRoot(cx, val)) {
      delete val;
      return JS_FALSE;
  }

  if (!JS_SetPrivate(cx, obj, (void*)val)) {
      JS_RemoveValueRoot(cx, val);
      delete val;
      return JS_FALSE;
  }

  *val = *vp;
  return JS_TRUE;
}

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
    {NULL,0,0,NULL,NULL}
};

#ifdef JSD_LOWLEVEL_SOURCE
/*
 * This facilitates sending source to JSD (the debugger system) in the shell
 * where the source is loaded using the JSFILE hack in jsscan. The function
 * below is used as a callback for the jsdbgapi JS_SetSourceHandler hook.
 * A more normal embedding (e.g. mozilla) loads source itself and can send
 * source directly to JSD without using this hook scheme.
 */
static void
SendSourceToJSDebugger(const char *filename, uintN lineno,
                       jschar *str, size_t length,
                       void **listenerTSData, JSDContext* jsdc)
{
    JSDSourceText *jsdsrc = (JSDSourceText *) *listenerTSData;

    if (!jsdsrc) {
        if (!filename)
            filename = "typein";
        if (1 == lineno) {
            jsdsrc = JSD_NewSourceText(jsdc, filename);
        } else {
            jsdsrc = JSD_FindSourceForURL(jsdc, filename);
            if (jsdsrc && JSD_SOURCE_PARTIAL !=
                JSD_GetSourceStatus(jsdc, jsdsrc)) {
                jsdsrc = NULL;
            }
        }
    }
    if (jsdsrc) {
        jsdsrc = JSD_AppendUCSourceText(jsdc,jsdsrc, str, length,
                                        JSD_SOURCE_PARTIAL);
    }
    *listenerTSData = jsdsrc;
}
#endif /* JSD_LOWLEVEL_SOURCE */

static JSBool its_noisy;    /* whether to be noisy when finalizing it */
static JSBool its_enum_fail;/* whether to fail when enumerating it */

static JSBool
its_addProperty(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
    if (!its_noisy)
        return JS_TRUE;

    IdStringifier idString(cx, id);
    fprintf(gOutFile, "adding its property %s,", idString.getBytes());
    ToString valueString(cx, *vp);
    fprintf(gOutFile, " initial value %s\n", valueString.getBytes());
    return JS_TRUE;
}

static JSBool
its_delProperty(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
    if (!its_noisy)
        return JS_TRUE;

    IdStringifier idString(cx, id);
    fprintf(gOutFile, "deleting its property %s,", idString.getBytes());
    ToString valueString(cx, *vp);
    fprintf(gOutFile, " initial value %s\n", valueString.getBytes());
    return JS_TRUE;
}

static JSBool
its_getProperty(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
    if (!its_noisy)
        return JS_TRUE;

    IdStringifier idString(cx, id);
    fprintf(gOutFile, "getting its property %s,", idString.getBytes());
    ToString valueString(cx, *vp);
    fprintf(gOutFile, " initial value %s\n", valueString.getBytes());
    return JS_TRUE;
}

static JSBool
its_setProperty(JSContext *cx, JSObject *obj, jsid id, JSBool strict, jsval *vp)
{
    IdStringifier idString(cx, id);
    if (its_noisy) {
        fprintf(gOutFile, "setting its property %s,", idString.getBytes());
        ToString valueString(cx, *vp);
        fprintf(gOutFile, " new value %s\n", valueString.getBytes());
    }

    if (!JSID_IS_ATOM(id))
        return JS_TRUE;

    if (!strcmp(idString.getBytes(), "noisy"))
        JS_ValueToBoolean(cx, *vp, &its_noisy);
    else if (!strcmp(idString.getBytes(), "enum_fail"))
        JS_ValueToBoolean(cx, *vp, &its_enum_fail);

    return JS_TRUE;
}

/*
 * Its enumerator, implemented using the "new" enumerate API,
 * see class flags.
 */
static JSBool
its_enumerate(JSContext *cx, JSObject *obj, JSIterateOp enum_op,
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
            return JS_FALSE;

        *statep = OBJECT_TO_JSVAL(iterator);
        if (idp)
            *idp = INT_TO_JSID(0);
        break;

      case JSENUMERATE_NEXT:
        if (its_enum_fail) {
            JS_ReportError(cx, "its enumeration failed");
            return JS_FALSE;
        }

        iterator = (JSObject *) JSVAL_TO_OBJECT(*statep);
        if (!JS_NextProperty(cx, iterator, idp))
            return JS_FALSE;

        if (!JSID_IS_VOID(*idp))
            break;
        /* Fall through. */

      case JSENUMERATE_DESTROY:
        /* Allow our iterator object to be GC'd. */
        *statep = JSVAL_NULL;
        break;
    }

    return JS_TRUE;
}

static JSBool
its_resolve(JSContext *cx, JSObject *obj, jsid id, uintN flags,
            JSObject **objp)
{
    if (its_noisy) {
        IdStringifier idString(cx, id);
        fprintf(gOutFile, "resolving its property %s, flags {%s,%s,%s}\n",
               idString.getBytes(),
               (flags & JSRESOLVE_QUALIFIED) ? "qualified" : "",
               (flags & JSRESOLVE_ASSIGNING) ? "assigning" : "",
               (flags & JSRESOLVE_DETECTING) ? "detecting" : "");
    }
    return JS_TRUE;
}

static JSBool
its_convert(JSContext *cx, JSObject *obj, JSType type, jsval *vp)
{
    if (its_noisy)
        fprintf(gOutFile, "converting it to %s type\n", JS_GetTypeName(cx, type));
    return JS_ConvertStub(cx, obj, type, vp);
}

static void
its_finalize(JSContext *cx, JSObject *obj)
{
    jsval *rootedVal;
    if (its_noisy)
        fprintf(gOutFile, "finalizing it\n");
    rootedVal = (jsval *) JS_GetPrivate(cx, obj);
    if (rootedVal) {
      JS_RemoveValueRoot(cx, rootedVal);
      JS_SetPrivate(cx, obj, NULL);
      delete rootedVal;
    }
}

static JSClass its_class = {
    "It", JSCLASS_NEW_RESOLVE | JSCLASS_NEW_ENUMERATE | JSCLASS_HAS_PRIVATE,
    its_addProperty,  its_delProperty,  its_getProperty,  its_setProperty,
    (JSEnumerateOp)its_enumerate, (JSResolveOp)its_resolve,
    its_convert,      its_finalize,
    JSCLASS_NO_OPTIONAL_MEMBERS
};

JSErrorFormatString jsShell_ErrorFormatString[JSErr_Limit] = {
#define MSG_DEF(name, number, count, exception, format) \
    { format, count, JSEXN_ERR } ,
#include "jsshell.msg"
#undef MSG_DEF
};

static const JSErrorFormatString *
my_GetErrorMessage(void *userRef, const char *locale, const uintN errorNumber)
{
    if ((errorNumber > 0) && (errorNumber < JSShellErr_Limit))
        return &jsShell_ErrorFormatString[errorNumber];
    return NULL;
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
Exec(JSContext *cx, uintN argc, jsval *vp)
{
    JSFunction *fun;
    const char *name, **nargv;
    uintN i, nargc;
    JSString *str;
    bool ok;
    pid_t pid;
    int status;

    JS_SET_RVAL(cx, vp, JSVAL_VOID);

    fun = JS_ValueToFunction(cx, vp[0]);
    if (!fun)
        return JS_FALSE;
    if (!fun->atom)
        return JS_TRUE;

    nargc = 1 + argc;

    /* nargc + 1 accounts for the terminating NULL. */
    nargv = new (char *)[nargc + 1];
    if (!nargv)
        return JS_FALSE;
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
global_enumerate(JSContext *cx, JSObject *obj)
{
#ifdef LAZY_STANDARD_CLASSES
    return JS_EnumerateStandardClasses(cx, obj);
#else
    return JS_TRUE;
#endif
}

static JSBool
global_resolve(JSContext *cx, JSObject *obj, jsid id, uintN flags,
               JSObject **objp)
{
#ifdef LAZY_STANDARD_CLASSES
    JSBool resolved;

    if (!JS_ResolveStandardClass(cx, obj, id, &resolved))
        return JS_FALSE;
    if (resolved) {
        *objp = obj;
        return JS_TRUE;
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
        JSBool ok, found;
        JSFunction *fun;

        if (!JSVAL_IS_STRING(id))
            return JS_TRUE;
        path = getenv("PATH");
        if (!path)
            return JS_TRUE;
        path = JS_strdup(cx, path);
        if (!path)
            return JS_FALSE;
        JSAutoByteString name(cx, JSVAL_TO_STRING(id));
        if (!name)
            return JS_FALSE;
        ok = JS_TRUE;
        for (comp = strtok(path, ":"); comp; comp = strtok(NULL, ":")) {
            if (*comp != '\0') {
                full = JS_smprintf("%s/%s", comp, name.ptr());
                if (!full) {
                    JS_ReportOutOfMemory(cx);
                    ok = JS_FALSE;
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
                    *objp = obj;
                break;
            }
        }
        JS_free(cx, path);
        return ok;
    }
#else
    return JS_TRUE;
#endif
}

JSClass global_class = {
    "global", JSCLASS_NEW_RESOLVE | JSCLASS_GLOBAL_FLAGS | JSCLASS_HAS_PRIVATE,
    JS_PropertyStub,  JS_PropertyStub,
    JS_PropertyStub,  JS_StrictPropertyStub,
    global_enumerate, (JSResolveOp) global_resolve,
    JS_ConvertStub,   its_finalize,
    JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSBool
env_setProperty(JSContext *cx, JSObject *obj, jsid id, JSBool strict, jsval *vp)
{
/* XXX porting may be easy, but these don't seem to supply setenv by default */
#if !defined XP_OS2 && !defined SOLARIS
    int rv;

    IdStringifier idstr(cx, id, JS_TRUE);
    if (idstr.threw())
        return JS_FALSE;
    ToString valstr(cx, *vp, JS_TRUE);
    if (valstr.threw())
        return JS_FALSE;
#if defined XP_WIN || defined HPUX || defined OSF1
    {
        char *waste = JS_smprintf("%s=%s", idstr.getBytes(), valstr.getBytes());
        if (!waste) {
            JS_ReportOutOfMemory(cx);
            return JS_FALSE;
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
        return JS_FALSE;
    }
    *vp = valstr.getJSVal();
#endif /* !defined XP_OS2 && !defined SOLARIS */
    return JS_TRUE;
}

static JSBool
env_enumerate(JSContext *cx, JSObject *obj)
{
    static JSBool reflected;
    char **evp, *name, *value;
    JSString *valstr;
    JSBool ok;

    if (reflected)
        return JS_TRUE;

    for (evp = (char **)JS_GetPrivate(cx, obj); (name = *evp) != NULL; evp++) {
        value = strchr(name, '=');
        if (!value)
            continue;
        *value++ = '\0';
        valstr = JS_NewStringCopyZ(cx, value);
        if (!valstr) {
            ok = JS_FALSE;
        } else {
            ok = JS_DefineProperty(cx, obj, name, STRING_TO_JSVAL(valstr),
                                   NULL, NULL, JSPROP_ENUMERATE);
        }
        value[-1] = '=';
        if (!ok)
            return JS_FALSE;
    }

    reflected = JS_TRUE;
    return JS_TRUE;
}

static JSBool
env_resolve(JSContext *cx, JSObject *obj, jsid id, uintN flags,
            JSObject **objp)
{
    JSString *valstr;
    const char *name, *value;

    if (flags & JSRESOLVE_ASSIGNING)
        return JS_TRUE;

    IdStringifier idstr(cx, id, JS_TRUE);
    if (idstr.threw())
        return JS_FALSE;

    name = idstr.getBytes();
    value = getenv(name);
    if (value) {
        valstr = JS_NewStringCopyZ(cx, value);
        if (!valstr)
            return JS_FALSE;
        if (!JS_DefineProperty(cx, obj, name, STRING_TO_JSVAL(valstr),
                               NULL, NULL, JSPROP_ENUMERATE)) {
            return JS_FALSE;
        }
        *objp = obj;
    }
    return JS_TRUE;
}

static JSClass env_class = {
    "environment", JSCLASS_HAS_PRIVATE | JSCLASS_NEW_RESOLVE,
    JS_PropertyStub,  JS_PropertyStub,
    JS_PropertyStub,  env_setProperty,
    env_enumerate, (JSResolveOp) env_resolve,
    JS_ConvertStub,   NULL,
    JSCLASS_NO_OPTIONAL_MEMBERS
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
    if (enableTraceJit)
        JS_ToggleOptions(cx, JSOPTION_JIT);
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
NewGlobalObject(JSContext *cx, CompartmentKind compartment)
{
    JSObject *glob = (compartment == NEW_COMPARTMENT)
                     ? JS_NewCompartmentAndGlobalObject(cx, &global_class, NULL)
                     : JS_NewGlobalObject(cx, &global_class);
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
        if (!JS_DefineFunctions(cx, glob, shell_functions) ||
            !JS_DefineProfilingFunctions(cx, glob)) {
            return NULL;
        }

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

    if (compartment == NEW_COMPARTMENT && !JS_WrapObject(cx, &glob))
        return NULL;

    return glob;
}

static bool
BindScriptArgs(JSContext *cx, JSObject *obj, OptionParser *op)
{
    MultiStringRange msr = op->getMultiStringArg("scriptArgs");
    JSObject *scriptArgs = JS_NewArrayObject(cx, 0, NULL);
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
ProcessArgs(JSContext *cx, JSObject *obj, OptionParser *op)
{
    if (op->getBoolOption('a'))
        JS_ToggleOptions(cx, JSOPTION_METHODJIT_ALWAYS);

    if (op->getBoolOption('m')) {
        enableMethodJit = true;
        JS_ToggleOptions(cx, JSOPTION_METHODJIT);
    }

#ifdef JS_GC_ZEAL
    if (const char *zeal = op->getStringOption('Z'))
        ParseZealArg(cx, zeal);
#endif

    if (op->getBoolOption('j')) {
        enableTraceJit = true;
        JS_ToggleOptions(cx, JSOPTION_JIT);
#if defined(JS_TRACER) && defined(DEBUG)
        js::InitJITStatsClass(cx, JS_GetGlobalObject(cx));
        JS_DefineObject(cx, JS_GetGlobalObject(cx), "tracemonkey",
                        &js::jitstats_class, NULL, 0);
#endif
    }
    
    if (op->getBoolOption('p')) {
        enableProfiling = true;
        JS_ToggleOptions(cx, JSOPTION_PROFILING);
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
                return EXIT_FAILURE;
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

    JSObject *glob = NewGlobalObject(cx, NEW_COMPARTMENT);
    if (!glob)
        return 1;

    JSAutoEnterCompartment ac;
    if (!ac.enter(cx, glob))
        return 1;

    JS_SetGlobalObject(cx, glob);

    JSObject *envobj = JS_DefineObject(cx, glob, "environment", &env_class, NULL, 0);
    if (!envobj || !JS_SetPrivate(cx, envobj, envp))
        return 1;

#ifdef JSDEBUGGER
    /*
    * XXX A command line option to enable debugging (or not) would be good
    */
    jsdc = JSD_DebuggerOnForUser(rt, NULL, NULL);
    if (!jsdc)
        return 1;
    JSD_JSContextInUse(jsdc, cx);
#ifdef JSD_LOWLEVEL_SOURCE
    JS_SetSourceHandler(rt, SendSourceToJSDebugger, jsdc);
#endif /* JSD_LOWLEVEL_SOURCE */
#ifdef JSDEBUGGER_JAVA_UI
    jsdjc = JSDJ_CreateContext();
    if (! jsdjc)
        return 1;
    JSDJ_SetJSDContext(jsdjc, jsdc);
    java_env = JSDJ_CreateJavaVMAndStartDebugger(jsdjc);
    /*
    * XXX This would be the place to wait for the debugger to start.
    * Waiting would be nice in general, but especially when a js file
    * is passed on the cmd line.
    */
#endif /* JSDEBUGGER_JAVA_UI */
#ifdef JSDEBUGGER_C_UI
    jsdbc = JSDB_InitDebugger(rt, jsdc, 0);
#endif /* JSDEBUGGER_C_UI */
#endif /* JSDEBUGGER */

#ifdef JS_THREADSAFE
    class ShellWorkerHooks : public js::workers::WorkerHooks {
    public:
        JSObject *newGlobalObject(JSContext *cx) {
            return NewGlobalObject(cx, NEW_COMPARTMENT);
        }
    };
    ShellWorkerHooks hooks;
    if (!JS_AddNamedObjectRoot(cx, &gWorkers, "Workers") ||
        (gWorkerThreadPool = js::workers::init(cx, &hooks, glob, &gWorkers)) == NULL) {
        return 1;
    }
#endif

    int result = ProcessArgs(cx, glob, op);

#ifdef JS_THREADSAFE
    js::workers::finish(cx, gWorkerThreadPool);
    JS_RemoveObjectRoot(cx, &gWorkers);
    if (result == 0)
        result = gExitCode;
#endif

#ifdef JSDEBUGGER
    if (jsdc) {
#ifdef JSDEBUGGER_C_UI
        if (jsdbc)
            JSDB_TermDebugger(jsdc);
#endif /* JSDEBUGGER_C_UI */
        JSD_DebuggerOff(jsdc);
    }
#endif  /* JSDEBUGGER */

    if (enableDisassemblyDumps)
        JS_DumpCompartmentBytecode(cx);
 
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

JSBool
ShellPrincipalsSubsume(JSPrincipals *, JSPrincipals *)
{
    return JS_TRUE;
}

JSPrincipals shellTrustedPrincipals = {
    (char *)"[shell trusted principals]",
    NULL,
    NULL,
    1,
    NULL, /* nobody should be destroying this */
    ShellPrincipalsSubsume
};

JSBool
CheckObjectAccess(JSContext *cx, JSObject *obj, jsid id, JSAccessMode mode,
                  jsval *vp)
{
    LeaveTrace(cx);
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
#ifdef JSDEBUGGER
    JSDContext *jsdc;
#ifdef JSDEBUGGER_JAVA_UI
    JNIEnv *java_env;
    JSDJContext *jsdjc;
#endif
#ifdef JSDEBUGGER_C_UI
    JSBool jsdbc;
#endif /* JSDEBUGGER_C_UI */
#endif /* JSDEBUGGER */
#ifdef XP_WIN
    {
        const char *crash_option = getenv("XRE_NO_WINDOWS_CRASH_DIALOG");
        if (crash_option && strncmp(crash_option, "1", 1)) {
            DWORD oldmode = SetErrorMode(SEM_NOGPFAULTERRORBOX);
            SetErrorMode(oldmode | SEM_NOGPFAULTERRORBOX);
        }
    }
#endif

    CheckHelpMessages();
#ifdef HAVE_SETLOCALE
    setlocale(LC_ALL, "");
#endif

#ifdef JS_THREADSAFE
    if (PR_FAILURE == PR_NewThreadPrivateIndex(&gStackBaseThreadIndex, NULL) ||
        PR_FAILURE == PR_SetThreadPrivate(gStackBaseThreadIndex, &stackDummy)) {
        return 1;
    }
#else
    gStackBase = (jsuword) &stackDummy;
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
        || !op.addBoolOption('j', "tracejit", "Enable the JaegerMonkey trace JIT")
        || !op.addBoolOption('p', "profiling", "Enable runtime profiling select JIT mode")
        || !op.addBoolOption('n', "typeinfer", "Enable type inference")
        || !op.addBoolOption('d', "debugjit", "Enable runtime debug mode for method JIT code")
        || !op.addBoolOption('a', "always-mjit",
                             "Do not try to run in the interpreter before "
                             "method jitting. Note that this has no particular effect on the "
                             "tracer; it still kicks in if enabled.")
        || !op.addBoolOption('D', "dump-bytecode", "Dump bytecode with exec count for all scripts")
        || !op.addBoolOption('b', "print-timing", "Print sub-ms runtime for each file that's run")
#ifdef DEBUG
        || !op.addIntOption('A', "oom-after", "COUNT", "Trigger OOM after COUNT allocations", -1)
        || !op.addBoolOption('O', "print-alloc", "Print the number of allocations at exit")
#endif
        || !op.addBoolOption('U', "utf8", "C strings passed to the JSAPI are UTF-8 encoded")
#ifdef JS_GC_ZEAL
        || !op.addStringOption('Z', "gc-zeal", "N[,F[,C]]",
                               "N indicates \"zealousness\":\n"
                               "  0: no additional GCs\n"
                               "  1: additional GCs at common danger points\n"
                               "  2: GC every F allocations (default: 100)\n"
                               "If C is 1, compartmental GCs are performed; otherwise, full")
#endif
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
    JS_SetRuntimeSecurityCallbacks(rt, &securityCallbacks);

    if (!InitWatchdog(rt))
        return 1;

    cx = NewContext(rt);
    if (!cx)
        return 1;

    JS_SetGCParameter(rt, JSGC_MODE, JSGC_MODE_COMPARTMENT);
    JS_SetGCParameterForThread(cx, JSGC_MAX_CODE_CACHE_BYTES, 16 * 1024 * 1024);

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
