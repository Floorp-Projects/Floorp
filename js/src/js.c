/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is .
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
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
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

/*
 * JS shell.
 */
#include "jsstddef.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "jstypes.h"
#include "jsarena.h" /* Added by JSIFY */
/* Removed by JSIFY: #include "prlog.h" */
#include "jsutil.h" /* Added by JSIFY */
#include "jsprf.h"
#include "jsapi.h"
#include "jsatom.h"
#include "jscntxt.h"
#include "jsdbgapi.h"
#include "jsemit.h"
#include "jsfun.h"
#include "jsgc.h"
#include "jslock.h"
#include "jsobj.h"
#include "jsparse.h"
#include "jsscope.h"
#include "jsscript.h"

#ifdef PERLCONNECT
#include "perlconnect/jsperl.h"
#endif

#ifdef LIVECONNECT
#include "jsjava.h"
#endif

#ifdef JSDEBUGGER
#include "jsdebug.h"
#ifdef JSDEBUGGER_JAVA_UI
#include "jsdjava.h"
#endif /* JSDEBUGGER_JAVA_UI */
#ifdef JSDEBUGGER_C_UI
#include "jsdb.h"
#endif /* JSDEBUGGER_C_UI */
#endif /* JSDEBUGGER */

#ifdef XP_UNIX
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#endif

#ifdef XP_PC
#include <io.h>     /* for isatty() */
#endif

#define EXITCODE_RUNTIME_ERROR 3
#define EXITCODE_FILE_NOT_FOUND 4


size_t gStackChunkSize = 8192;
int gExitCode = 0;
FILE *gErrFile = NULL;
FILE *gOutFile = NULL;

#ifdef XP_MAC
#ifdef MAC_TEST_HACK
/* this is the data file that all Print strings will be echoed into */
FILE *gTestResultFile = NULL;
#define isatty(f) 0
#else
#define isatty(f) 1
#endif

char *strdup(const char *str)
{
    char *copy = (char *) malloc(strlen(str)+1);
    if (copy)
        strcpy(copy, str);
    return copy;
}

#ifdef XP_MAC_MPW
/* Macintosh MPW replacements for the ANSI routines.  These translate LF's to CR's because
   the MPW libraries supplied by Metrowerks don't do that for some reason.  */
static void translateLFtoCR(char *str, int length)
{
    char *limit = str + length;
    while (str != limit) {
        if (*str == '\n')
            *str = '\r';
        str++;
    }
}

int fputc(int c, FILE *file)
{
    char buffer = c;
    if (buffer == '\n')
        buffer = '\r';
    return fwrite(&buffer, 1, 1, file);
}

int fputs(const char *s, FILE *file)
{
    char buffer[4096];
    int n = strlen(s);
    int extra = 0;

    while (n > sizeof buffer) {
        memcpy(buffer, s, sizeof buffer);
        translateLFtoCR(buffer, sizeof buffer);
        extra += fwrite(buffer, 1, sizeof buffer, file);
        n -= sizeof buffer;
        s += sizeof buffer;
    }
    memcpy(buffer, s, n);
    translateLFtoCR(buffer, n);
    return extra + fwrite(buffer, 1, n, file);
}

int fprintf(FILE* file, const char *format, ...)
{
    va_list args;
    char smallBuffer[4096];
    int n;
    int bufferSize = sizeof smallBuffer;
    char *buffer = smallBuffer;
    int result;

    va_start(args, format);
    n = vsnprintf(buffer, bufferSize, format, args);
    va_end(args);
    while (n < 0) {
        if (buffer != smallBuffer)
            free(buffer);
        bufferSize <<= 1;
        buffer = malloc(bufferSize);
        if (!buffer) {
            JS_ASSERT(JS_FALSE);
            return 0;
        }
        va_start(args, format);
        n = vsnprintf(buffer, bufferSize, format, args);
        va_end(args);
    }
    translateLFtoCR(buffer, n);
    result = fwrite(buffer, 1, n, file);
    if (buffer != smallBuffer)
        free(buffer);
    return result;
}


#else
#include <SIOUX.h>
#include <MacTypes.h>

static char* mac_argv[] = { "js", NULL };

static void initConsole(StringPtr consoleName, const char* startupMessage, int *argc, char** *argv)
{
    SIOUXSettings.autocloseonquit = true;
    SIOUXSettings.asktosaveonclose = false;
    /* SIOUXSettings.initializeTB = false;
     SIOUXSettings.showstatusline = true;*/
    puts(startupMessage);
    SIOUXSetTitle(consoleName);

    /* set up a buffer for stderr (otherwise it's a pig). */
    setvbuf(stderr, (char *) malloc(BUFSIZ), _IOLBF, BUFSIZ);

    *argc = 1;
    *argv = mac_argv;
}

#ifdef LIVECONNECT
/* Little hack to provide a default CLASSPATH on the Mac. */
#define getenv(var) mac_getenv(var)
static char* mac_getenv(const char* var)
{
    if (strcmp(var, "CLASSPATH") == 0) {
        static char class_path[] = "liveconnect.jar";
        return class_path;
    }
    return NULL;
}
#endif /* LIVECONNECT */

#endif
#endif

#ifdef JSDEBUGGER
static JSDContext *_jsdc;
#ifdef JSDEBUGGER_JAVA_UI
static JSDJContext *_jsdjc;
#endif /* JSDEBUGGER_JAVA_UI */
#endif /* JSDEBUGGER */

static JSBool reportWarnings = JS_TRUE;

typedef enum JSShellErrNum {
#define MSG_DEF(name, number, count, exception, format) \
    name = number,
#include "jsshell.msg"
#undef MSG_DEF
    JSShellErr_Limit
#undef MSGDEF
} JSShellErrNum;

static const JSErrorFormatString *
my_GetErrorMessage(void *userRef, const char *locale, const uintN errorNumber);

#ifdef EDITLINE
extern char     *readline(const char *prompt);
extern void     add_history(char *line);
#endif

static JSBool
GetLine(JSContext *cx, char *bufp, FILE *file, const char *prompt) {
#ifdef EDITLINE
    /*
     * Use readline only if file is stdin, because there's no way to specify
     * another handle.  Are other filehandles interactive?
     */
    if (file == stdin) {
        char *linep;
        if ((linep = readline(prompt)) == NULL)
            return JS_FALSE;
        if (strlen(linep) > 0)
            add_history(linep);
        strcpy(bufp, linep);
        JS_free(cx, linep);
        bufp += strlen(bufp);
        *bufp++ = '\n';
        *bufp = '\0';
    } else
#endif
    {
        char line[256];
        fprintf(gOutFile, prompt);
#ifdef XP_MAC_MPW
        /* Print a CR after the prompt because MPW grabs the entire line when entering an interactive command */
        fputc('\n', gOutFile);
#endif
        if (fgets(line, sizeof line, file) == NULL)
            return JS_FALSE;
        strcpy(bufp, line);
    }
    return JS_TRUE;
}

static void
Process(JSContext *cx, JSObject *obj, char *filename)
{
    JSBool ok, hitEOF;
    JSScript *script;
    jsval result;
    JSString *str;
    char buffer[4096];
    char *bufp;
    int lineno;
    int startline;
    FILE *file;

    if (filename != NULL && strcmp(filename, "-") != 0) {
        file = fopen(filename, "r");
        if (!file) {
            JS_ReportErrorNumber(cx, my_GetErrorMessage, NULL,
                                 JSSMSG_CANT_OPEN, filename, strerror(errno));
            gExitCode = EXITCODE_FILE_NOT_FOUND;
            return;
        }
    } else {
        file = stdin;
    }

    if (!isatty(fileno(file))) {
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
        script = JS_CompileFileHandle(cx, obj, filename, file);
        if (script) {
            (void)JS_ExecuteScript(cx, obj, script, &result);
            JS_DestroyScript(cx, script);
        }
        return;
    }

    /* It's an interactive filehandle; drop into read-eval-print loop. */
    lineno = 1;
    hitEOF = JS_FALSE;
    do {
        bufp = buffer;
        *bufp = '\0';

        /*
         * Accumulate lines until we get a 'compilable unit' - one that either
         * generates an error (before running out of source) or that compiles
         * cleanly.  This should be whenever we get a complete statement that
         * coincides with the end of a line.
         */
        startline = lineno;
        do {
            if (!GetLine(cx, bufp, file, startline == lineno ? "js> " : "")) {
                hitEOF = JS_TRUE;
                break;
            }
            bufp += strlen(bufp);
            lineno++;
        } while (!JS_BufferIsCompilableUnit(cx, obj, buffer, strlen(buffer)));

        /* Clear any pending exception from previous failed compiles.  */
        JS_ClearPendingException(cx);
        script = JS_CompileScript(cx, obj, buffer, strlen(buffer),
#ifdef JSDEBUGGER
                                  "typein",
#else
                                  NULL,
#endif
                                  startline);
        if (script) {
            JSErrorReporter older;

            ok = JS_ExecuteScript(cx, obj, script, &result);
            if (ok && result != JSVAL_VOID) {
                /* Suppress error reports from JS_ValueToString(). */
                older = JS_SetErrorReporter(cx, NULL);
                str = JS_ValueToString(cx, result);
                JS_SetErrorReporter(cx, older);

                if (str)
                    fprintf(gOutFile, "%s\n", JS_GetStringBytes(str));
                else
                    ok = JS_FALSE;
            }
#if 0
#if JS_HAS_ERROR_EXCEPTIONS
            /*
             * Require that any time we return failure, an exception has
             * been set.
             */
            JS_ASSERT(ok || JS_IsExceptionPending(cx));

            /*
             * Also that any time an exception has been set, we've
             * returned failure.
             */
            JS_ASSERT(!JS_IsExceptionPending(cx) || !ok);
#endif /* JS_HAS_ERROR_EXCEPTIONS */
#endif
            JS_DestroyScript(cx, script);
        }
    } while (!hitEOF);
    fprintf(gOutFile, "\n");
    return;
}

static int
usage(void)
{
    fprintf(gErrFile, "%s\n", JS_GetImplementationVersion());
    fprintf(gErrFile, "usage: js [-s] [-w] [-W] [-b branchlimit] [-c stackchunksize] [-v version] [-f scriptfile] [scriptfile] [scriptarg...]\n");
    return 2;
}

static uint32 gBranchCount;
static uint32 gBranchLimit;

static JSBool
my_BranchCallback(JSContext *cx, JSScript *script)
{
    if (++gBranchCount == gBranchLimit) {
        if (script->filename)
            fprintf(gErrFile, "%s:", script->filename);
        fprintf(gErrFile, "%u: script branches too much (%u callbacks)\n",
                script->lineno, gBranchLimit);
        gBranchCount = 0;
        return JS_FALSE;
    }
    return JS_TRUE;
}

static int
ProcessArgs(JSContext *cx, JSObject *obj, char **argv, int argc)
{
    int i, j;
    char *filename = NULL;
    jsint length;
    jsval *vector;
    JSObject *argsObj;
    JSBool isInteractive = JS_TRUE;

    for (i=0; i < argc; i++) {
        if (argv[i][0] == '-') {
            switch (argv[i][1]) {
            case 'v':
                if (i+1 == argc) {
                    return usage();
                }
                JS_SetVersion(cx, (JSVersion) atoi(argv[i+1]));
                i++;
                break;

            case 'w':
                reportWarnings = JS_TRUE;
                break;

            case 'W':
                reportWarnings = JS_FALSE;
                break;

            case 's':
                JS_ToggleOptions(cx, JSOPTION_STRICT);
                break;

            case 'b':
                gBranchLimit = atoi(argv[++i]);
                JS_SetBranchCallback(cx, my_BranchCallback);
                break;

            case 'c':
                /* set stack chunk size */
                gStackChunkSize = atoi(argv[++i]);
                break;

            case 'f':
                if (i+1 == argc) {
                    return usage();
                }
                filename = argv[i+1];
                /* "-f -" means read from stdin */
                if (filename[0] == '-' && filename[1] == '\0')
                    filename = NULL;
                Process(cx, obj, filename);
                filename = NULL;
                /* XXX: js -f foo.js should interpret foo.js and then
                 * drop into interactive mode, but that breaks test
                 * harness. Just execute foo.js for now.
                 */
                isInteractive = JS_FALSE;
                i++;
                break;

            default:
                return usage();
            }
        } else {
            filename = argv[i++];
            isInteractive = JS_FALSE;
            break;
        }
    }

    length = argc - i;
    if (length == 0) {
        vector = NULL;
    } else {
        vector = (jsval *) JS_malloc(cx, length * sizeof(jsval));
        if (vector == NULL)
            return 1;

        for (j = 0; j < length; j++) {
            JSString *str = JS_NewStringCopyZ(cx, argv[i++]);
            if (str == NULL)
                return 1;
            vector[j] = STRING_TO_JSVAL(str);
        }
    }

    argsObj = JS_NewArrayObject(cx, length, vector);
    if (vector)
        JS_free(cx, vector);
    if (argsObj == NULL)
        return 1;

    if (!JS_DefineProperty(cx, obj, "arguments", OBJECT_TO_JSVAL(argsObj),
                           NULL, NULL, 0)) {
        return 1;
    }

    if (filename || isInteractive)
        Process(cx, obj, filename);
    return gExitCode;
}


static JSBool
Version(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    if (argc > 0 && JSVAL_IS_INT(argv[0]))
        *rval = INT_TO_JSVAL(JS_SetVersion(cx, (JSVersion) JSVAL_TO_INT(argv[0])));
    else
        *rval = INT_TO_JSVAL(JS_GetVersion(cx));
    return JS_TRUE;
}

static struct {
    const char  *name;
    uint32      flag;
} js_options[] = {
    {"strict",          JSOPTION_STRICT},
    {"werror",          JSOPTION_WERROR},
    {0,                 0}
};

static JSBool
Options(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    uint32 optset, flag;
    uintN i, j, found;
    JSString *str;
    const char *opt;
    char *names;

    optset = 0;
    for (i = 0; i < argc; i++) {
        str = JS_ValueToString(cx, argv[i]);
        if (!str)
            return JS_FALSE;
        opt = JS_GetStringBytes(str);
        for (j = 0; js_options[j].name; j++) {
            if (strcmp(js_options[j].name, opt) == 0) {
                optset |= js_options[j].flag;
                break;
            }
        }
    }
    optset = JS_ToggleOptions(cx, optset);

    names = NULL;
    found = 0;
    while (optset != 0) {
        flag = optset;
        optset &= optset - 1;
        flag &= ~optset;
        for (j = 0; js_options[j].name; j++) {
            if (js_options[j].flag == flag) {
                names = JS_sprintf_append(names, "%s%s",
                                          names ? "," : "", js_options[j].name);
                found++;
                break;
            }
        }
    }
    if (!found)
        names = strdup("");
    if (!names) {
        JS_ReportOutOfMemory(cx);
        return JS_FALSE;
    }

    str = JS_NewString(cx, names, strlen(names));
    if (!str) {
        free(names);
        return JS_FALSE;
    }
    *rval = STRING_TO_JSVAL(str);
    return JS_TRUE;
}

static void
my_LoadErrorReporter(JSContext *cx, const char *message, JSErrorReport *report);

static void
my_ErrorReporter(JSContext *cx, const char *message, JSErrorReport *report);

static JSBool
Load(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    uintN i;
    JSString *str;
    const char *filename;
    JSScript *script;
    JSBool ok;
    jsval result;
    JSErrorReporter older;

    for (i = 0; i < argc; i++) {
        str = JS_ValueToString(cx, argv[i]);
        if (!str)
            return JS_FALSE;
        argv[i] = STRING_TO_JSVAL(str);
        filename = JS_GetStringBytes(str);
        errno = 0;
        older = JS_SetErrorReporter(cx, my_LoadErrorReporter);
        script = JS_CompileFile(cx, obj, filename);
        if (!script)
            ok = JS_FALSE;
        else {
            ok = JS_ExecuteScript(cx, obj, script, &result);
            JS_DestroyScript(cx, script);
        }
        JS_SetErrorReporter(cx, older);
        if (!ok)
            return JS_FALSE;
    }

    return JS_TRUE;
}

static JSBool
Print(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    uintN i, n;
    JSString *str;

    for (i = n = 0; i < argc; i++) {
        str = JS_ValueToString(cx, argv[i]);
        if (!str)
            return JS_FALSE;
        fprintf(gOutFile, "%s%s", i ? " " : "", JS_GetStringBytes(str));
    }
    n++;
    if (n)
        fputc('\n', gOutFile);
    return JS_TRUE;
}

static JSBool
Help(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);

static JSBool
Quit(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    int r = 0;

#ifdef LIVECONNECT
    JSJ_SimpleShutdown();
#endif

    JS_ConvertArguments(cx, argc, argv,"/ i", &r);

    exit(r);
    return JS_FALSE;
}

#ifdef GC_MARK_DEBUG
extern JS_FRIEND_DATA(FILE *) js_DumpGCHeap;
#endif

static JSBool
GC(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSRuntime *rt;
    uint32 preBytes;

    rt = cx->runtime;
    preBytes = rt->gcBytes;
#ifdef GC_MARK_DEBUG
    if (argc && JSVAL_IS_STRING(argv[0])) {
        char *name = JS_GetStringBytes(JSVAL_TO_STRING(argv[0]));
        FILE *file = fopen(name, "w");
        if (!file) {
            fprintf(gErrFile, "gc: can't open %s: %s\n", strerror(errno));
            return JS_FALSE;
        }
        js_DumpGCHeap = file;
    } else {
        js_DumpGCHeap = stdout;
    }
#endif
    js_ForceGC(cx);
#ifdef GC_MARK_DEBUG
    if (js_DumpGCHeap != stdout)
        fclose(js_DumpGCHeap);
    js_DumpGCHeap = NULL;
#endif
    fprintf(gOutFile, "before %lu, after %lu, break %08lx\n",
            (unsigned long)preBytes, (unsigned long)rt->gcBytes,
#ifdef XP_UNIX
            (unsigned long)sbrk(0)
#else
            0
#endif
            );
#ifdef JS_GCMETER
    js_DumpGCStats(rt, stdout);
#endif
    return JS_TRUE;
}

static JSScript *
ValueToScript(JSContext *cx, jsval v)
{
    JSScript *script;
    JSFunction *fun;

    if (JSVAL_IS_OBJECT(v) &&
        JS_GET_CLASS(cx, JSVAL_TO_OBJECT(v)) == &js_ScriptClass) {
        script = (JSScript *) JS_GetPrivate(cx, JSVAL_TO_OBJECT(v));
    } else {
        fun = JS_ValueToFunction(cx, v);
        if (!fun)
            return NULL;
        script = fun->script;
    }
    return script;
}

static JSBool
GetTrapArgs(JSContext *cx, uintN argc, jsval *argv, JSScript **scriptp,
            int32 *ip)
{
    uintN intarg;
    JSScript *script;

    *scriptp = cx->fp->down->script;
    *ip = 0;
    if (argc != 0) {
        intarg = 0;
        if (JS_TypeOfValue(cx, argv[0]) == JSTYPE_FUNCTION) {
            script = ValueToScript(cx, argv[0]);
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
TrapHandler(JSContext *cx, JSScript *script, jsbytecode *pc, jsval *rval,
            void *closure)
{
    JSString *str;
    JSStackFrame *caller;

    str = (JSString *) closure;
    caller = cx->fp->down;
    if (!JS_EvaluateScript(cx, caller->scopeChain,
                           JS_GetStringBytes(str), JS_GetStringLength(str),
                           caller->script->filename, caller->script->lineno,
                           rval)) {
        return JSTRAP_ERROR;
    }
    if (*rval != JSVAL_VOID)
        return JSTRAP_RETURN;
    return JSTRAP_CONTINUE;
}

static JSBool
Trap(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSString *str;
    JSScript *script;
    int32 i;

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
    return JS_SetTrap(cx, script, script->code + i, TrapHandler, str);
}

static JSBool
Untrap(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSScript *script;
    int32 i;

    if (!GetTrapArgs(cx, argc, argv, &script, &i))
        return JS_FALSE;
    JS_ClearTrap(cx, script, script->code + i, NULL, NULL);
    return JS_TRUE;
}

static JSBool
LineToPC(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSScript *script;
    int32 i;
    uintN lineno;
    jsbytecode *pc;

    if (argc == 0) {
        JS_ReportErrorNumber(cx, my_GetErrorMessage, NULL, JSSMSG_LINE2PC_USAGE);
        return JS_FALSE;
    }
    script = cx->fp->down->script;
    if (!GetTrapArgs(cx, argc, argv, &script, &i))
        return JS_FALSE;
    lineno = (i == 0) ? script->lineno : (uintN)i;
    pc = JS_LineNumberToPC(cx, script, lineno);
    if (!pc)
        return JS_FALSE;
    *rval = INT_TO_JSVAL(PTRDIFF(pc, script->code, jsbytecode));
    return JS_TRUE;
}

static JSBool
PCToLine(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSScript *script;
    int32 i;
    uintN lineno;

    if (!GetTrapArgs(cx, argc, argv, &script, &i))
        return JS_FALSE;
    lineno = JS_PCToLineNumber(cx, script, script->code + i);
    if (!lineno)
        return JS_FALSE;
    *rval = INT_TO_JSVAL(lineno);
    return JS_TRUE;
}

#ifdef DEBUG

static void
SrcNotes(JSContext *cx, JSScript *script)
{
    uintN offset, delta, caseOff;
    jssrcnote *notes, *sn;
    JSSrcNoteType type;
    jsatomid atomIndex;
    JSAtom *atom;

    notes = script->notes;
    if (notes) {
        fprintf(gOutFile, "\nSource notes:\n");
        offset = 0;
        for (sn = notes; !SN_IS_TERMINATOR(sn); sn = SN_NEXT(sn)) {
            delta = SN_DELTA(sn);
            offset += delta;
            fprintf(gOutFile, "%3u: %5u [%4u] %-8s",
                    PTRDIFF(sn, notes, jssrcnote), offset, delta,
                    js_SrcNoteName[SN_TYPE(sn)]);
            type = (JSSrcNoteType) SN_TYPE(sn);
            switch (type) {
              case SRC_SETLINE:
                fprintf(gOutFile, " lineno %u", (uintN) js_GetSrcNoteOffset(sn, 0));
                break;
              case SRC_FOR:
                fprintf(gOutFile, " cond %u update %u tail %u",
                       (uintN) js_GetSrcNoteOffset(sn, 0),
                       (uintN) js_GetSrcNoteOffset(sn, 1),
                       (uintN) js_GetSrcNoteOffset(sn, 2));
                break;
              case SRC_PCBASE:
              case SRC_PCDELTA:
                fprintf(gOutFile, " offset %u", (uintN) js_GetSrcNoteOffset(sn, 0));
                break;
              case SRC_LABEL:
              case SRC_LABELBRACE:
              case SRC_BREAK2LABEL:
              case SRC_CONT2LABEL:
              case SRC_FUNCDEF: {
                const char *bytes;
                JSFunction *fun;
                JSString *str;

                atomIndex = (jsatomid) js_GetSrcNoteOffset(sn, 0);
                atom = js_GetAtom(cx, &script->atomMap, atomIndex);
                if (type != SRC_FUNCDEF) {
                    bytes = ATOM_BYTES(atom);
                } else {
                    fun = (JSFunction *)
                        JS_GetPrivate(cx, ATOM_TO_OBJECT(atom));
                    str = JS_DecompileFunction(cx, fun, JS_DONT_PRETTY_PRINT);
                    bytes = str ? JS_GetStringBytes(str) : "N/A";
                }
                fprintf(gOutFile, " atom %u (%s)", (uintN)atomIndex, bytes);
                break;
              }
              case SRC_SWITCH:
                fprintf(gOutFile, " length %u", (uintN) js_GetSrcNoteOffset(sn, 0));
                caseOff = (uintN) js_GetSrcNoteOffset(sn, 1);
                if (caseOff)
                    fprintf(gOutFile, " first case offset %u", caseOff);
                break;
              case SRC_CATCH:
                delta = (uintN) js_GetSrcNoteOffset(sn, 0);
                if (delta)
                    fprintf(gOutFile, " guard size %u", delta);
                break;
              default:;
            }
            fputc('\n', gOutFile);
        }
    }
}

static JSBool
Notes(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    uintN i;
    JSScript *script;

    for (i = 0; i < argc; i++) {
        script = ValueToScript(cx, argv[i]);
        if (!script)
            continue;

        SrcNotes(cx, script);
    }
    return JS_TRUE;
}

static JSBool
TryNotes(JSContext *cx, JSScript *script)
{
    JSTryNote *tn = script->trynotes;

    if (!tn)
        return JS_TRUE;
    fprintf(gOutFile, "\nException table:\nstart\tend\tcatch\n");
    while (tn->start && tn->catchStart) {
        fprintf(gOutFile, "  %d\t%d\t%d\n",
               tn->start, tn->start + tn->length, tn->catchStart);
        tn++;
    }
    return JS_TRUE;
}

static JSBool
Disassemble(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSBool lines;
    uintN i;
    JSScript *script;

    if (argc > 0 &&
        JSVAL_IS_STRING(argv[0]) &&
        !strcmp(JS_GetStringBytes(JSVAL_TO_STRING(argv[0])), "-l")) {
        lines = JS_TRUE;
        argv++, argc--;
    } else {
        lines = JS_FALSE;
    }
    for (i = 0; i < argc; i++) {
        script = ValueToScript(cx, argv[i]);
        if (!script)
            continue;

        if (JSVAL_IS_FUNCTION(cx, argv[i])) {
            JSFunction *fun = JS_ValueToFunction(cx, argv[i]);
            if (fun && (fun->flags & JSFUN_FLAGS_MASK)) {
                uint8 flags = fun->flags;
                fputs("flags:", stderr);

#define SHOW_FLAG(flag) if (flags & JSFUN_##flag) fputs(" " #flag, stdout);

                SHOW_FLAG(LAMBDA);
                SHOW_FLAG(SETTER);
                SHOW_FLAG(GETTER);
                SHOW_FLAG(BOUND_METHOD);
                SHOW_FLAG(HEAVYWEIGHT);
                
#undef SHOW_FLAG
                putchar('\n');
            }
        }

        js_Disassemble(cx, script, lines, stdout);
        SrcNotes(cx, script);
        TryNotes(cx, script);
    }
    return JS_TRUE;
}

static JSBool
DisassWithSrc(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
              jsval *rval)
{
#define LINE_BUF_LEN 512
    uintN i, len, line1, line2, bupline;
    JSScript *script;
    FILE *file;
    char linebuf[LINE_BUF_LEN];
    jsbytecode *pc, *end;
    static char sep[] = ";-------------------------";

    for (i = 0; i < argc; i++) {
        script = ValueToScript(cx, argv[i]);
        if (!script)
            continue;

        if (!script || !script->filename) {
            JS_ReportErrorNumber(cx, my_GetErrorMessage, NULL,
                                            JSSMSG_FILE_SCRIPTS_ONLY);
            return JS_FALSE;
        }

        file = fopen(script->filename, "r");
        if (!file) {
            JS_ReportErrorNumber(cx, my_GetErrorMessage, NULL,
                            JSSMSG_CANT_OPEN,
                            script->filename, strerror(errno));
            return JS_FALSE;
        }

        pc = script->code;
        end = pc + script->length;

        /* burn the leading lines */
        line2 = JS_PCToLineNumber(cx, script, pc);
        for (line1 = 0; line1 < line2 - 1; line1++)
            fgets(linebuf, LINE_BUF_LEN, file);

        bupline = 0;
        while (pc < end) {
            line2 = JS_PCToLineNumber(cx, script, pc);

            if (line2 < line1) {
                if (bupline != line2) {
                    bupline = line2;
                    fprintf(gOutFile, "%s %3u: BACKUP\n", sep, line2);
                }
            } else {
                if (bupline && line1 == line2)
                    fprintf(gOutFile, "%s %3u: RESTORE\n", sep, line2);
                bupline = 0;
                while (line1 < line2) {
                    if (!fgets(linebuf, LINE_BUF_LEN, file)) {
                        JS_ReportErrorNumber(cx, my_GetErrorMessage, NULL,
                                       JSSMSG_UNEXPECTED_EOF,
                                       script->filename);
                        goto bail;
                    }
                    line1++;
                    fprintf(gOutFile, "%s %3u: %s", sep, line1, linebuf);
                }
            }

            len = js_Disassemble1(cx, script, pc,
                                  PTRDIFF(pc, script->code, jsbytecode),
                                  JS_TRUE, stdout);
            if (!len)
                return JS_FALSE;
            pc += len;
        }

      bail:
        fclose(file);
    }
    return JS_TRUE;
#undef LINE_BUF_LEN
}

static JSBool
Tracing(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSBool bval;
    JSString *str;

    if (argc == 0) {
        *rval = BOOLEAN_TO_JSVAL(cx->tracefp != 0);
        return JS_TRUE;
    }

    switch (JS_TypeOfValue(cx, argv[0])) {
      case JSTYPE_NUMBER:
        bval = JSVAL_IS_INT(argv[0])
               ? JSVAL_TO_INT(argv[0])
               : (jsint) *JSVAL_TO_DOUBLE(argv[0]);
        break;
      case JSTYPE_BOOLEAN:
        bval = JSVAL_TO_BOOLEAN(argv[0]);
        break;
      default:
        str = JS_ValueToString(cx, argv[0]);
        if (!str)
            return JS_FALSE;
        fprintf(gErrFile, "tracing: illegal argument %s\n",
                JS_GetStringBytes(str));
        return JS_TRUE;
    }
    cx->tracefp = bval ? stdout : NULL;
    return JS_TRUE;
}

static int
DumpAtom(JSHashEntry *he, int i, void *arg)
{
    FILE *fp = (FILE *) arg;
    JSAtom *atom = (JSAtom *)he;

    fprintf(fp, "%3d %08x %5lu ",
            i, (uintN)he->keyHash, (unsigned long)atom->number);
    if (ATOM_IS_STRING(atom))
        fprintf(fp, "\"%s\"\n", ATOM_BYTES(atom));
    else if (ATOM_IS_INT(atom))
        fprintf(fp, "%ld\n", (long)ATOM_TO_INT(atom));
    else
        fprintf(fp, "%.16g\n", *ATOM_TO_DOUBLE(atom));
    return HT_ENUMERATE_NEXT;
}

static int
DumpSymbol(JSHashEntry *he, int i, void *arg)
{
    FILE *fp = (FILE *) arg;
    JSSymbol *sym = (JSSymbol *)he;

    fprintf(fp, "%3d %08x", i, (uintN)he->keyHash);
    if (JSVAL_IS_INT(sym_id(sym)))
        fprintf(fp, " [%ld]\n", (long)JSVAL_TO_INT(sym_id(sym)));
    else
        fprintf(fp, " \"%s\"\n", ATOM_BYTES(sym_atom(sym)));
    return HT_ENUMERATE_NEXT;
}

extern JS_FRIEND_DATA(JSScopeOps) js_list_scope_ops;

static void
DumpScope(JSContext *cx, JSObject *obj, JSHashEnumerator dump, FILE *fp)
{
    JSScope *scope;
    JSSymbol *sym;
    int i;

    fprintf(fp, "\n%s scope contents:\n", JS_GET_CLASS(cx, obj)->name);
    scope = OBJ_SCOPE(obj);
    if (!MAP_IS_NATIVE(&scope->map))
        return;
    if (scope->ops == &js_list_scope_ops) {
        for (sym = (JSSymbol *)scope->data, i = 0; sym;
             sym = (JSSymbol *)sym->entry.next, i++) {
            DumpSymbol(&sym->entry, i, fp);
        }
    } else {
        JS_HashTableDump((JSHashTable *) scope->data, dump, fp);
    }
}

/* These are callable from gdb. */
static void Dsym(JSSymbol *sym) { if (sym) DumpSymbol(&sym->entry, 0, gErrFile); }
static void Datom(JSAtom *atom) { if (atom) DumpAtom(&atom->entry, 0, gErrFile); }

static JSBool
DumpStats(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    uintN i;
    JSString *str;
    const char *bytes;
    JSAtom *atom;
    JSObject *obj2;
    JSProperty *prop;
    jsval value;

    for (i = 0; i < argc; i++) {
        str = JS_ValueToString(cx, argv[i]);
        if (!str)
            return JS_FALSE;
        bytes = JS_GetStringBytes(str);
        if (strcmp(bytes, "arena") == 0) {
#ifdef JS_ARENAMETER
            JS_DumpArenaStats(stdout);
#endif
        } else if (strcmp(bytes, "atom") == 0) {
            fprintf(gOutFile, "\natom table contents:\n");
            JS_HashTableDump(cx->runtime->atomState.table, DumpAtom, stdout);
        } else if (strcmp(bytes, "global") == 0) {
            DumpScope(cx, cx->globalObject, DumpSymbol, stdout);
        } else {
            atom = js_Atomize(cx, bytes, JS_GetStringLength(str), 0);
            if (!atom)
                return JS_FALSE;
            if (!js_FindProperty(cx, (jsid)atom, &obj, &obj2, &prop))
                return JS_FALSE;
            if (prop) {
                OBJ_DROP_PROPERTY(cx, obj2, prop);
                if (!OBJ_GET_PROPERTY(cx, obj, (jsid)atom, &value))
                    return JS_FALSE;
            }
            if (!prop || !JSVAL_IS_OBJECT(value)) {
                fprintf(gErrFile, "js: invalid stats argument %s\n",
                        bytes);
                continue;
            }
            obj = JSVAL_TO_OBJECT(value);
            if (obj)
                DumpScope(cx, obj, DumpSymbol, stdout);
        }
    }
    return JS_TRUE;
}

#endif /* DEBUG */

#ifdef TEST_EXPORT
static JSBool
DoExport(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSAtom *atom;
    JSObject *obj2;
    JSProperty *prop;
    JSBool ok;
    uintN attrs;

    if (argc != 2) {
        JS_ReportErrorNumber(cx, my_GetErrorMessage, NULL, JSSMSG_DOEXP_USAGE);
        return JS_FALSE;
    }
    if (!JS_ValueToObject(cx, argv[0], &obj))
        return JS_FALSE;
    argv[0] = OBJECT_TO_JSVAL(obj);
    atom = js_ValueToStringAtom(cx, argv[1]);
    if (!atom)
        return JS_FALSE;
    if (!OBJ_LOOKUP_PROPERTY(cx, obj, (jsid)atom, &obj2, &prop))
        return JS_FALSE;
    if (!prop) {
        ok = OBJ_DEFINE_PROPERTY(cx, obj, id, JSVAL_VOID, NULL, NULL,
                                 JSPROP_EXPORTED, NULL);
    } else {
        ok = OBJ_GET_ATTRIBUTES(cx, obj, (jsid)atom, prop, &attrs);
        if (ok) {
            attrs |= JSPROP_EXPORTED;
            ok = OBJ_SET_ATTRIBUTES(cx, obj, (jsid)atom, prop, &attrs);
        }
        OBJ_DROP_PROPERTY(cx, obj2, prop);
    }
    return ok;
}
#endif

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
ConvertArgs(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSBool b = JS_FALSE;
    jschar c = 0;
    int32 i = 0, j = 0;
    uint32 u = 0;
    jsdouble d = 0, I = 0, re = 0, im = 0;
    char *s = NULL;
    JSString *str = NULL;
    jschar *w = NULL;
    JSObject *obj2 = NULL;
    JSFunction *fun = NULL;
    jsval v = JSVAL_VOID;
    JSBool ok;

    if (!JS_AddArgumentFormatter(cx, "ZZ", ZZ_formatter))
        return JS_FALSE;;
    ok = JS_ConvertArguments(cx, argc, argv, "b/ciujdIsSWofvZZ*",
                             &b, &c, &i, &u, &j, &d, &I, &s, &str, &w, &obj2,
                             &fun, &v, &re, &im);
    JS_RemoveArgumentFormatter(cx, "ZZ");
    if (!ok)
        return JS_FALSE;
    fprintf(gOutFile,
            "b %u, c %x (%c), i %ld, u %lu, j %ld\n",
            b, c, (char)c, i, u, j);
    fprintf(gOutFile,
            "d %g, I %g, s %s, S %s, W %s, obj %s, fun %s\n"
            "v %s, re %g, im %g\n",
            d, I, s, str ? JS_GetStringBytes(str) : "", EscapeWideString(w),
            JS_GetStringBytes(JS_ValueToString(cx, OBJECT_TO_JSVAL(obj2))),
            fun ? JS_GetStringBytes(JS_DecompileFunction(cx, fun, 4)) : "",
            JS_GetStringBytes(JS_ValueToString(cx, v)), re, im);
    return JS_TRUE;
}
#endif

static JSBool
BuildDate(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    fprintf(gOutFile, "built on %s at %s\n", __DATE__, __TIME__);
    return JS_TRUE;
}

static JSBool
Clear(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    if (argc != 0 && !JS_ValueToObject(cx, argv[0], &obj))
        return JS_FALSE;
    JS_ClearScope(cx, obj);
    return JS_TRUE;
}

static JSBool
Intern(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSString *str;

    str = JS_ValueToString(cx, argv[0]);
    if (!str)
        return JS_FALSE;
    if (!JS_InternUCStringN(cx, JS_GetStringChars(str),
                                JS_GetStringLength(str))) {
        return JS_FALSE;
    }
    return JS_TRUE;
}

static JSFunctionSpec shell_functions[] = {
    {"version",         Version,        0},
    {"options",         Options,        0},
    {"load",            Load,           1},
    {"print",           Print,          0},
    {"help",            Help,           0},
    {"quit",            Quit,           0},
    {"gc",              GC,             0},
    {"trap",            Trap,           3},
    {"untrap",          Untrap,         2},
    {"line2pc",         LineToPC,       0},
    {"pc2line",         PCToLine,       0},
#ifdef DEBUG
    {"dis",             Disassemble,    1},
    {"dissrc",          DisassWithSrc,  1},
    {"notes",           Notes,          1},
    {"tracing",         Tracing,        0},
    {"stats",           DumpStats,      1},
#endif
#ifdef TEST_EXPORT
    {"doexp",           DoExport,       2},
#endif
#ifdef TEST_CVTARGS
    {"cvtargs",         ConvertArgs,    0, 0, 12},
#endif
    {"build",           BuildDate,      0},
    {"clear",           Clear,          0},
    {"intern",          Intern,         1},
    {0}
};

/* NOTE: These must be kept in sync with the above. */

static char *shell_help_messages[] = {
    "version([number])      Get or set JavaScript version number",
    "options([option ...])  Get or toggle JavaScript options",
    "load(['foo.js' ...])   Load files named by string arguments",
    "print([expr ...])      Evaluate and print expressions",
    "help([name ...])       Display usage and help messages",
    "quit()                 Quit the shell",
    "gc()                   Run the garbage collector",
    "trap([fun] [pc] expr)  Trap bytecode execution",
    "untrap([fun] [pc])     Remove a trap",
    "line2pc([fun] line)    Map line number to PC",
    "pc2line([fun] [pc])    Map PC to line number",
#ifdef DEBUG
    "dis([fun])             Disassemble functions into bytecodes",
    "dissrc([fun])          Disassemble functions with source lines",
    "notes([fun])           Show source notes for functions",
    "tracing([toggle])      Turn tracing on or off",
    "stats([string ...])    Dump 'arena', 'atom', 'global' stats",
#endif
#ifdef TEST_EXPORT
    "doexp(obj, id)         Export identified property from object",
#endif
#ifdef TEST_CVTARGS
    "cvtargs(b, c, ...)     Test JS_ConvertArguments",
#endif
    "build()                Show build date and time",
    "clear([obj])           Clear properties of object",
    "intern(str)            Internalize str in the atom table",
    0
};

static void
ShowHelpHeader(void)
{
    fprintf(gOutFile, "%-9s %-22s %s\n", "Command", "Usage", "Description");
    fprintf(gOutFile, "%-9s %-22s %s\n", "=======", "=====", "===========");
}

static void
ShowHelpForCommand(uintN n)
{
    fprintf(gOutFile, "%-9.9s %s\n", shell_functions[n].name, shell_help_messages[n]);
}

static JSBool
Help(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    uintN i, j;
    int did_header, did_something;
    JSType type;
    JSFunction *fun;
    JSString *str;
    const char *bytes;

    fprintf(gOutFile, "%s\n", JS_GetImplementationVersion());
    if (argc == 0) {
        ShowHelpHeader();
        for (i = 0; shell_functions[i].name; i++)
            ShowHelpForCommand(i);
    } else {
        did_header = 0;
        for (i = 0; i < argc; i++) {
            did_something = 0;
            type = JS_TypeOfValue(cx, argv[i]);
            if (type == JSTYPE_FUNCTION) {
                fun = JS_ValueToFunction(cx, argv[i]);
                str = fun->atom ? ATOM_TO_STRING(fun->atom) : NULL;
            } else if (type == JSTYPE_STRING) {
                str = JSVAL_TO_STRING(argv[i]);
            } else {
                str = NULL;
            }
            if (str) {
                bytes = JS_GetStringBytes(str);
                for (j = 0; shell_functions[j].name; j++) {
                    if (!strcmp(bytes, shell_functions[j].name)) {
                        if (!did_header) {
                            did_header = 1;
                            ShowHelpHeader();
                        }
                        did_something = 1;
                        ShowHelpForCommand(j);
                        break;
                    }
                }
            }
            if (!did_something) {
                str = JS_ValueToString(cx, argv[i]);
                if (!str)
                    return JS_FALSE;
                fprintf(gErrFile, "Sorry, no help for %s\n",
                        JS_GetStringBytes(str));
            }
        }
    }
    return JS_TRUE;
}

/*
 * Define a JS object called "it".  Give it class operations that printf why
 * they're being called for tutorial purposes.
 */
enum its_tinyid {
    ITS_COLOR, ITS_HEIGHT, ITS_WIDTH, ITS_FUNNY, ITS_ARRAY, ITS_RDONLY
};

static JSPropertySpec its_props[] = {
    {"color",           ITS_COLOR,	JSPROP_ENUMERATE},
    {"height",          ITS_HEIGHT,	JSPROP_ENUMERATE},
    {"width",           ITS_WIDTH,	JSPROP_ENUMERATE},
    {"funny",           ITS_FUNNY,	JSPROP_ENUMERATE},
    {"array",           ITS_ARRAY,	JSPROP_ENUMERATE},
    {"rdonly",		ITS_RDONLY,	JSPROP_READONLY},
    {0}
};

static JSBool
its_item(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    *rval = OBJECT_TO_JSVAL(obj);
    if (argc != 0)
        JS_SetCallReturnValue2(cx, argv[0]);
    return JS_TRUE;
}

static JSFunctionSpec its_methods[] = {
    {"item",            its_item,       0},
    {0}
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

static JSBool
its_addProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    if (its_noisy) {
        fprintf(gOutFile, "adding its property %s,",
               JS_GetStringBytes(JS_ValueToString(cx, id)));
        fprintf(gOutFile, " initial value %s\n",
               JS_GetStringBytes(JS_ValueToString(cx, *vp)));
    }
    return JS_TRUE;
}

static JSBool
its_delProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    if (its_noisy) {
        fprintf(gOutFile, "deleting its property %s,",
               JS_GetStringBytes(JS_ValueToString(cx, id)));
        fprintf(gOutFile, " current value %s\n",
               JS_GetStringBytes(JS_ValueToString(cx, *vp)));
    }
    return JS_TRUE;
}

static JSBool
its_getProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    if (its_noisy) {
        fprintf(gOutFile, "getting its property %s,",
               JS_GetStringBytes(JS_ValueToString(cx, id)));
        fprintf(gOutFile, " current value %s\n",
               JS_GetStringBytes(JS_ValueToString(cx, *vp)));
    }
    return JS_TRUE;
}

static JSBool
its_setProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    if (its_noisy) {
        fprintf(gOutFile, "setting its property %s,",
               JS_GetStringBytes(JS_ValueToString(cx, id)));
        fprintf(gOutFile, " new value %s\n",
               JS_GetStringBytes(JS_ValueToString(cx, *vp)));
    }
    if (JSVAL_IS_STRING(id) &&
        !strcmp(JS_GetStringBytes(JSVAL_TO_STRING(id)), "noisy")) {
        return JS_ValueToBoolean(cx, *vp, &its_noisy);
    }
    return JS_TRUE;
}

static JSBool
its_enumerate(JSContext *cx, JSObject *obj)
{
    if (its_noisy)
        fprintf(gOutFile, "enumerate its properties\n");
    return JS_TRUE;
}

static JSBool
its_resolve(JSContext *cx, JSObject *obj, jsval id)
{
    if (its_noisy) {
        fprintf(gOutFile, "resolving its property %s\n",
               JS_GetStringBytes(JS_ValueToString(cx, id)));
    }
    return JS_TRUE;
}

static JSBool
its_convert(JSContext *cx, JSObject *obj, JSType type, jsval *vp)
{
    if (its_noisy)
        fprintf(gOutFile, "converting it to %s type\n", JS_GetTypeName(cx, type));
    return JS_TRUE;
}

static void
its_finalize(JSContext *cx, JSObject *obj)
{
    if (its_noisy)
        fprintf(gOutFile, "finalizing it\n");
}

static JSClass its_class = {
    "It", 0,
    its_addProperty,  its_delProperty,  its_getProperty,  its_setProperty,
    its_enumerate,    its_resolve,      its_convert,      its_finalize
};

JSErrorFormatString jsShell_ErrorFormatString[JSErr_Limit] = {
#if JS_HAS_DFLT_MSG_STRINGS
#define MSG_DEF(name, number, count, exception, format) \
    { format, count } ,
#else
#define MSG_DEF(name, number, count, exception, format) \
    { NULL, count } ,
#endif
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
my_LoadErrorReporter(JSContext *cx, const char *message, JSErrorReport *report)
{
    if (!report) {
        fprintf(gErrFile, "%s\n", message);
        return;
    }

    /* Ignore any exceptions */
    if (JSREPORT_IS_EXCEPTION(report->flags))
        return;

    /* Otherwise, fall back to the ordinary error reporter. */
    my_ErrorReporter(cx, message, report);
}

static void
my_ErrorReporter(JSContext *cx, const char *message, JSErrorReport *report)
{
    int i, j, k, n;
    char *prefix, *tmp;
    const char *ctmp;

    if (!report) {
        fprintf(gErrFile, "%s\n", message);
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
    n = PTRDIFF(report->tokenptr, report->linebuf, char);
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
    if (!JSREPORT_IS_WARNING(report->flags))
        gExitCode = EXITCODE_RUNTIME_ERROR;
    JS_free(cx, prefix);
}

#if defined(SHELL_HACK) && defined(DEBUG) && defined(XP_UNIX)
static JSBool
Exec(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSFunction *fun;
    const char *name, **nargv;
    uintN i, nargc;
    JSString *str;
    pid_t pid;
    int status;

    fun = JS_ValueToFunction(cx, argv[-2]);
    if (!fun)
        return JS_FALSE;
    if (!fun->atom)
        return JS_TRUE;
    name = ATOM_BYTES(fun->atom);
    nargc = 1 + argc;
    nargv = JS_malloc(cx, (nargc + 1) * sizeof(char *));
    if (!nargv)
        return JS_FALSE;
    nargv[0] = name;
    for (i = 1; i < nargc; i++) {
        str = JS_ValueToString(cx, argv[i-1]);
        if (!str) {
            JS_free(cx, nargv);
            return JS_FALSE;
        }
        nargv[i] = JS_GetStringBytes(str);
    }
    nargv[nargc] = 0;
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
            ;
        break;
    }
    JS_free(cx, nargv);
    return JS_TRUE;
}
#endif

#define LAZY_STANDARD_CLASSES

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
global_resolve(JSContext *cx, JSObject *obj, jsval id, uintN flags,
               JSObject **objp)
{
#ifdef LAZY_STANDARD_CLASSES
    if ((flags & JSRESOLVE_ASSIGNING) == 0) {
        JSBool resolved;

        if (!JS_ResolveStandardClass(cx, obj, id, &resolved))
            return JS_FALSE;
        if (resolved) {
            *objp = obj;
            return JS_TRUE;
        }
    }
#endif

#if defined(SHELL_HACK) && defined(DEBUG) && defined(XP_UNIX)
    if ((flags & (JSRESOLVE_QUALIFIED | JSRESOLVE_ASSIGNING)) == 0) {
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
        name = JS_GetStringBytes(JSVAL_TO_STRING(id));
        ok = JS_TRUE;
        for (comp = strtok(path, ":"); comp; comp = strtok(NULL, ":")) {
            if (*comp != '\0') {
                full = JS_smprintf("%s/%s", comp, name);
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

static JSClass global_class = {
    "global", JSCLASS_NEW_RESOLVE,
    JS_PropertyStub,  JS_PropertyStub,
    JS_PropertyStub,  JS_PropertyStub,
    global_enumerate, (JSResolveOp) global_resolve,
    JS_ConvertStub,   JS_FinalizeStub
};

int
main(int argc, char **argv)
{
    JSVersion version;
    JSRuntime *rt;
    JSContext *cx;
    JSObject *glob, *it;
    int result;
#ifdef LIVECONNECT
    JavaVM *java_vm = NULL;
#endif
#ifdef JSDEBUGGER_JAVA_UI
    JNIEnv *java_env;
#endif

#ifdef XP_OS2
   /* these streams are normally line buffered on OS/2 and need a \n, *
    * so we need to unbuffer then to get a reasonable prompt          */
    setbuf(stdout,0);
    setbuf(stderr,0);
#endif

    gErrFile = stderr;
    gOutFile = stdout;

#ifdef XP_MAC
#ifndef XP_MAC_MPW
        initConsole("\pJavaScript Shell", "Welcome to js shell.", &argc, &argv);
#endif
#endif

#ifdef MAC_TEST_HACK
/*
        Open a file "testArgs.txt" and read each line into argc/argv.
        Re-direct all output to "results.txt"
*/
        {
                char argText[256];
                FILE *f = fopen("testargs.txt", "r");
                if (f != NULL) {
                        int maxArgs = 32; /* arbitrary max !!! */
                        argc = 1;
                        argv = malloc(sizeof(char *) * maxArgs);
                        argv[0] = NULL;
                        while (fgets(argText, 255, f) != NULL) {
                                 /* argText includes '\n' */
                                argv[argc] = malloc(strlen(argText));
                                strncpy(argv[argc], argText,
                                                    strlen(argText) - 1);
                                argv[argc][strlen(argText) - 1] = '\0';
                                argc++;
                                if (argc >= maxArgs) break;
                        }
                        fclose(f);
                }
                gTestResultFile = fopen("results.txt", "w");
        }

        gErrFile = gTestResultFile;
        gOutFile = gTestResultFile;
#endif

    version = JSVERSION_DEFAULT;

    argc--;
    argv++;

    rt = JS_NewRuntime(8L * 1024L * 1024L);
    if (!rt)
        return 1;

    cx = JS_NewContext(rt, gStackChunkSize);
    if (!cx)
        return 1;
    JS_SetErrorReporter(cx, my_ErrorReporter);

    glob = JS_NewObject(cx, &global_class, NULL, NULL);
    if (!glob)
        return 1;
#ifdef LAZY_STANDARD_CLASSES
    JS_SetGlobalObject(cx, glob);
#else
    if (!JS_InitStandardClasses(cx, glob))
        return 1;
#endif
    if (!JS_DefineFunctions(cx, glob, shell_functions))
        return 1;

    /* Set version only after there is a global object. */
    if (version != JSVERSION_DEFAULT)
        JS_SetVersion(cx, version);

    it = JS_DefineObject(cx, glob, "it", &its_class, NULL, 0);
    if (!it)
        return 1;
    if (!JS_DefineProperties(cx, it, its_props))
        return 1;
    if (!JS_DefineFunctions(cx, it, its_methods))
        return 1;

#ifdef PERLCONNECT
    if (!JS_InitPerlClass(cx, glob))
        return 1;
#endif

#ifdef JSDEBUGGER
    /*
    * XXX A command line option to enable debugging (or not) would be good
    */
    _jsdc = JSD_DebuggerOnForUser(rt, NULL, NULL);
    if (!_jsdc)
        return 1;
    JSD_JSContextInUse(_jsdc, cx);
#ifdef JSD_LOWLEVEL_SOURCE
    JS_SetSourceHandler(rt, SendSourceToJSDebugger, _jsdc);
#endif /* JSD_LOWLEVEL_SOURCE */
#ifdef JSDEBUGGER_JAVA_UI
    _jsdjc = JSDJ_CreateContext();
    if (! _jsdjc)
        return 1;
    JSDJ_SetJSDContext(_jsdjc, _jsdc);
    java_env = JSDJ_CreateJavaVMAndStartDebugger(_jsdjc);
#ifdef LIVECONNECT
    if (java_env)
        (*java_env)->GetJavaVM(java_env, &java_vm);
#endif
    /*
    * XXX This would be the place to wait for the debugger to start.
    * Waiting would be nice in general, but especially when a js file
    * is passed on the cmd line.
    */
#endif /* JSDEBUGGER_JAVA_UI */
#ifdef JSDEBUGGER_C_UI
    JSDB_InitDebugger(rt, _jsdc, 0);
#endif /* JSDEBUGGER_C_UI */
#endif /* JSDEBUGGER */

#ifdef LIVECONNECT
        if (!JSJ_SimpleInit(cx, glob, java_vm, getenv("CLASSPATH")))
            return 1;
#endif

    result = ProcessArgs(cx, glob, argv, argc);

#ifdef JSDEBUGGER
    if (_jsdc)
        JSD_DebuggerOff(_jsdc);
#endif  /* JSDEBUGGER */

#ifdef MAC_TEST_HACK
    fclose(gTestResultFile);
#endif

    JS_DestroyContext(cx);
    JS_DestroyRuntime(rt);
    JS_ShutDown();
    return result;
}
