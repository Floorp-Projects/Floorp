/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

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
#include "jsscan.h"
#include "jsscope.h"
#include "jsscript.h"

#ifdef PERLCONNECT
#include "jsperl.h"
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

#ifdef XP_MAC
#define isatty(f) 1

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
	setvbuf(stderr, malloc(BUFSIZ), _IOLBF, BUFSIZ);

	*argc = 1;
	*argv = mac_argv;
}

#endif

#ifndef JSFILE
# error "JSFILE must be defined for this module to work."
#endif

#ifdef JSDEBUGGER
static JSDContext *_jsdc;
#ifdef JSDEBUGGER_JAVA_UI
static JSDJContext *_jsdjc;
#endif /* JSDEBUGGER_JAVA_UI */
#endif /* JSDEBUGGER */

static int reportWarnings;

typedef enum JSShellErrNum {
#define MSG_DEF(name, number, count, exception, format) \
    name = number,
#include "jsshell.msg"
#undef MSG_DEF
    JSShellErr_Limit
#undef MSGDEF
} JSShellErrNum;

static void
Process(JSContext *cx, JSObject *obj, char *filename)
{
    JSTokenStream *ts;
    JSCodeGenerator cg;
    JSBool ok;
    JSScript *script;
    jsval result;
    JSString *str;

    ts = js_NewFileTokenStream(cx, filename, stdin);
    if (!ts)
	goto out;
#ifdef JSDEBUGGER
    if (!filename)
	ts->filename = "typein";
#endif
    if (isatty(fileno(ts->file))) {
	ts->flags |= TSF_INTERACTIVE;
    } else {
	/* Support the UNIX #! shell hack; gobble the first line if it starts
	 * with '#'.  TODO - this isn't quite compatible with sharp variables,
	 * as a legal js program (using sharp variables) might start with '#'.
	 * But that would require multi-character lookahead.
	 */
	char ch = fgetc(ts->file);
	if (ch == '#') {
	    while((ch = fgetc(ts->file)) != EOF) {
		if(ch == '\n' || ch == '\r')
		    break;
	    }
	}
	ungetc(ch, ts->file);
    }

    do {
	js_InitCodeGenerator(cx, &cg, ts->filename, ts->lineno, ts->principals);
	do {
	    if (ts->flags & TSF_INTERACTIVE)
		printf("js> ");
	    ok = js_CompileTokenStream(cx, obj, ts, &cg);
	    if (ts->flags & TSF_ERROR) {
		ts->flags &= ~TSF_ERROR;
		CLEAR_PUSHBACK(ts);
		ok = JS_TRUE;
	    }
	} while (ok && !(ts->flags & TSF_EOF) && CG_OFFSET(&cg) == 0);
	if (ok) {
	    /* Clear any pending exception from previous failed compiles.  */
	    JS_ClearPendingException(cx);

	    script = js_NewScriptFromCG(cx, &cg, NULL);
	    if (script) {
                JSErrorReporter older;
                
		if (JS_ExecuteScript(cx, obj, script, &result) &&
		    (ts->flags & TSF_INTERACTIVE) &&
		    result != JSVAL_VOID) {
                    /*
                     * If JS_ValueToString generates an error, suppress
                     * the report and print the exception below.
                     */
                    older = JS_SetErrorReporter(cx, NULL);
		    str = JS_ValueToString(cx, result);
                    JS_SetErrorReporter(cx, older);

		    if (str)
			printf("%s\n", JS_GetStringBytes(str));
		}

		if (JS_IsExceptionPending(cx) &&
		    JS_GetPendingException(cx, &result))
		{
		    /*
		     * Calling JS_ValueToString could cause another error (and
		     * throw an associated exception) - so we disable the error
		     * reporter so nothing gets reported, and we always clear
		     * the pending exception.
		     */
                    JS_ClearPendingException(cx);
		    older = JS_SetErrorReporter(cx, NULL);
		    str = JS_ValueToString(cx, result);
		    JS_SetErrorReporter(cx, older);

		    /* XXX non-i18nized strings... */
		    if (str) {
			fprintf(stderr, "Uncaught JavaScript exception:\n%s\n",
			       JS_GetStringBytes(str));
		    } else {
			fprintf(stderr, "Uncaught JavaScript exception\n");
		    }
		}
		JS_DestroyScript(cx, script);
	    }
	}
	cg.firstLine = ts->lineno;
	js_FinishCodeGenerator(cx, &cg);
	RESET_TOKENBUF(ts);
    } while (!(ts->flags & TSF_EOF));

out:
    if (ts)
	(void) js_CloseTokenStream(cx, ts);
    JS_FreeArenaPool(&cx->codePool);
    JS_FreeArenaPool(&cx->tempPool);
}

static int
usage(void)
{
    fprintf(stderr, "%s\n", JS_GetImplementationVersion());
    fprintf(stderr, "usage: js [-w] [-v version] [-f scriptfile] [scriptfile] [scriptarg...]\n");
    return 2;
}

static int
ProcessArgs(JSContext *cx, JSObject *obj, char **argv, int argc)
{
    int i;
    char *filename = NULL;
    jsint length;
    jsval *vector;
    jsval *p;
    JSObject *argsObj;

    for (i=0; i < argc; i++) {
	if (argv[i][0] == '-') {
	    switch (argv[i][1]) {
	    case 'v':
		if (i+1 == argc) {
		    return usage();
		}
		JS_SetVersion(cx, atoi(argv[i+1]));
		i++;
		break;
	    case 'w':
		reportWarnings++;
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
		i++;
		break;
	    default:
		return usage();
	    }
	} else {
	    filename = argv[i++];
	    break;
	}
    }

    length = argc - i;
    vector = JS_malloc(cx, length * sizeof(jsval));
    p = vector;

    if (vector == NULL)
	return 1;

    while (i < argc) {
	JSString *str = JS_NewStringCopyZ(cx, argv[i]);
	if (str == NULL)
	    return 1;
	*p++ = STRING_TO_JSVAL(str);
	i++;
    }
    argsObj = JS_NewArrayObject(cx, length, vector);
    JS_free(cx, vector);
    if (argsObj == NULL)
	return 1;

    if (!JS_DefineProperty(cx, obj, "arguments",
			   OBJECT_TO_JSVAL(argsObj), NULL, NULL, 0))
	return 1;

    Process(cx, obj, filename);
    return 0;
}


static JSBool
Version(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    if (argc > 0 && JSVAL_IS_INT(argv[0]))
	*rval = INT_TO_JSVAL(JS_SetVersion(cx, JSVAL_TO_INT(argv[0])));
    else
	*rval = INT_TO_JSVAL(JS_GetVersion(cx));
    return JS_TRUE;
}

static void
my_ErrorReporter(JSContext *cx, const char *message, JSErrorReport *report);

static const JSErrorFormatString *
my_GetErrorMessage(void *userRef, const char *locale, const uintN errorNumber);

static JSBool
Load(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    uintN i;
    JSString *str;
    const char *filename;
    JSScript *script;
    JSBool ok;
    jsval result;

    for (i = 0; i < argc; i++) {
	str = JS_ValueToString(cx, argv[i]);
	if (!str)
	    return JS_FALSE;
	argv[i] = STRING_TO_JSVAL(str);
	filename = JS_GetStringBytes(str);
	errno = 0;
	script = JS_CompileFile(cx, obj, filename);
	if (!script)
            ok = JS_FALSE;
        else {
            ok = JS_ExecuteScript(cx, obj, script, &result);
	    JS_DestroyScript(cx, script);
        }
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
	printf("%s%s", i ? " " : "", JS_GetStringBytes(str));
	n++;
    }
    if (n)
	putchar('\n');
    return JS_TRUE;
}

static JSBool
Help(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);

static JSBool
Quit(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
#ifdef LIVECONNECT
    JSJ_SimpleShutdown();
#endif
    exit(0);
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
	    fprintf(stderr, "gc: can't open %s: %s\n", strerror(errno));
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
    printf("before %lu, after %lu, break %08lx\n",
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

static JSBool
GetTrapArgs(JSContext *cx, uintN argc, jsval *argv, JSScript **scriptp,
	    int32 *ip)
{
    uintN intarg;
    JSFunction *fun;

    *scriptp = cx->fp->down->script;
    *ip = 0;
    if (argc != 0) {
	intarg = 0;
	if (JS_TypeOfValue(cx, argv[0]) == JSTYPE_FUNCTION) {
	    fun = JS_ValueToFunction(cx, argv[0]);
	    if (!fun)
		return JS_FALSE;
	    *scriptp = fun->script;
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

    str = closure;
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
    *rval = INT_TO_JSVAL(pc - script->code);
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
SrcNotes(JSContext *cx, JSFunction *fun )
{
    uintN offset, delta, caseOff;
    jssrcnote *notes, *sn;
    JSSrcNoteType type;
    jsatomid atomIndex;
    JSAtom *atom;

    notes = fun->script->notes;
    if (notes) {
	printf("\nSource notes:\n");
	offset = 0;
	for (sn = notes; !SN_IS_TERMINATOR(sn); sn = SN_NEXT(sn)) {
	    delta = SN_DELTA(sn);
	    offset += delta;
	    printf("%3u: %5u [%4u] %-8s",
		   sn - notes, offset, delta, js_SrcNoteName[SN_TYPE(sn)]);
	    type = SN_TYPE(sn);
	    switch (type) {
	      case SRC_SETLINE:
		printf(" lineno %u", (uintN) js_GetSrcNoteOffset(sn, 0));
		break;
	      case SRC_FOR:
		printf(" cond %u update %u tail %u",
		       (uintN) js_GetSrcNoteOffset(sn, 0),
		       (uintN) js_GetSrcNoteOffset(sn, 1),
		       (uintN) js_GetSrcNoteOffset(sn, 2));
		break;
	      case SRC_PCBASE:
	      case SRC_PCDELTA:
		printf(" offset %u", (uintN) js_GetSrcNoteOffset(sn, 0));
		break;
	      case SRC_LABEL:
	      case SRC_LABELBRACE:
	      case SRC_BREAK2LABEL:
	      case SRC_CONT2LABEL:
	      case SRC_FUNCDEF:
		atomIndex = (jsatomid) js_GetSrcNoteOffset(sn, 0);
		atom = js_GetAtom(cx, &fun->script->atomMap, atomIndex);
		printf(" atom %u (%s)", (uintN)atomIndex, ATOM_BYTES(atom));
		break;
	      case SRC_SWITCH:
		printf(" length %u", (uintN) js_GetSrcNoteOffset(sn, 0));
		caseOff = (uintN) js_GetSrcNoteOffset(sn, 1);
		if (caseOff)
		    printf(" first case offset %u", caseOff);
		break;
	      case SRC_CATCH:
		delta = (uintN) js_GetSrcNoteOffset(sn, 0);
		if (delta)
		    printf(" guard size %u", delta);
		break;
	      default:;
	    }
	    putchar('\n');
	}
    }
}

static JSBool
Notes(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    uintN i;
    JSFunction *fun;

    for (i = 0; i < argc; i++) {
	fun = JS_ValueToFunction(cx, argv[i]);
	if (!fun)
	    return JS_FALSE;

	SrcNotes(cx, fun);
    }
    return JS_TRUE;
}

static JSBool
TryNotes(JSContext *cx, JSFunction *fun)
{
    JSTryNote *tn = fun->script->trynotes;

    if (!tn)
	return JS_TRUE;
    printf("\nException table:\nstart\tend\tcatch\n");
    while (tn->start && tn->catchStart) {
	printf("  %d\t%d\t%d\n",
	       tn->start, tn->length, tn->catchStart);
	tn++;
    }
    return JS_TRUE;
}

static JSBool
Disassemble(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSBool lines;
    uintN i;
    JSFunction *fun;

    if (argc > 0 &&
	JSVAL_IS_STRING(argv[0]) &&
	!strcmp(JS_GetStringBytes(JSVAL_TO_STRING(argv[0])), "-l")) {
	lines = JS_TRUE;
	argv++, argc--;
    } else {
	lines = JS_FALSE;
    }
    for (i = 0; i < argc; i++) {
	fun = JS_ValueToFunction(cx, argv[i]);
	if (!fun)
	    return JS_FALSE;

	js_Disassemble(cx, fun->script, lines, stdout);
	SrcNotes(cx, fun);
	TryNotes(cx, fun);
    }
    return JS_TRUE;
}

static JSBool
DisassWithSrc(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
#define LINE_BUF_LEN 512
    uintN i, len, line1, line2, bupline;
    JSFunction *fun;
    FILE *file;
    char linebuf[LINE_BUF_LEN];
    jsbytecode *pc, *end;
    static char sep[] = ";-------------------------";

    for (i = 0; i < argc; i++) {
	fun = JS_ValueToFunction(cx, argv[i]);
	if (!fun)
	    return JS_FALSE;

	if (!fun->script || !fun->script->filename) {
	    JS_ReportErrorNumber(cx, my_GetErrorMessage, NULL,
					    JSSMSG_FILE_SCRIPTS_ONLY);
	    return JS_FALSE;
	}

	file = fopen(fun->script->filename, "r");
	if (!file) {
	    JS_ReportErrorNumber(cx, my_GetErrorMessage, NULL,
			    JSSMSG_CANT_OPEN,
			    fun->script->filename, strerror(errno));
	    return JS_FALSE;
	}

	pc = fun->script->code;
	end = pc + fun->script->length;

	/* burn the leading lines */
	line2 = JS_PCToLineNumber(cx, fun->script, pc);
	for (line1 = 0; line1 < line2 - 1; line1++)
	    fgets(linebuf, LINE_BUF_LEN, file);

	bupline = 0;
	while (pc < end) {
	    line2 = JS_PCToLineNumber(cx, fun->script, pc);

	    if (line2 < line1) {
		if (bupline != line2) {
		    bupline = line2;
		    printf("%s %3u: BACKUP\n", sep, line2);
		}
	    } else {
		if (bupline && line1 == line2)
		    printf("%s %3u: RESTORE\n", sep, line2);
		bupline = 0;
		while (line1 < line2) {
		    if (!fgets(linebuf, LINE_BUF_LEN, file)) {
			JS_ReportErrorNumber(cx, my_GetErrorMessage, NULL,
				       JSSMSG_UNEXPECTED_EOF,
				       fun->script->filename);
			goto bail;
		    }
		    line1++;
		    printf("%s %3u: %s", sep, line1, linebuf);
		}
	    }

	    len = js_Disassemble1(cx, fun->script, pc, pc - fun->script->code,
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
	fprintf(stderr, "tracing: illegal argument %s\n",
		JS_GetStringBytes(str));
	return JS_TRUE;
    }
    cx->tracefp = bval ? stdout : NULL;
    return JS_TRUE;
}

int
DumpAtom(JSHashEntry *he, int i, void *arg)
{
    FILE *fp = arg;
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

int
DumpSymbol(JSHashEntry *he, int i, void *arg)
{
    FILE *fp = arg;
    JSSymbol *sym = (JSSymbol *)he;

    fprintf(fp, "%3d %08x", i, (uintN)he->keyHash);
    if (JSVAL_IS_INT(sym_id(sym)))
	fprintf(fp, " [%ld]\n", (long)JSVAL_TO_INT(sym_id(sym)));
    else
	fprintf(fp, " \"%s\"\n", ATOM_BYTES(sym_atom(sym)));
    return HT_ENUMERATE_NEXT;
}

extern JS_FRIEND_DATA(JSScopeOps) js_list_scope_ops;

void
DumpScope(JSContext *cx, JSObject *obj, JSHashEnumerator dump, FILE *fp)
{
    JSScope *scope;
    JSSymbol *sym;
    int i;

    fprintf(fp, "\n%s scope contents:\n", OBJ_GET_CLASS(cx, obj)->name);
    scope = (JSScope *)obj->map;
    if (!MAP_IS_NATIVE(&scope->map))
	return;
    if (scope->ops == &js_list_scope_ops) {
	for (sym = (JSSymbol *)scope->data, i = 0; sym;
	     sym = (JSSymbol *)sym->entry.next, i++) {
	    DumpSymbol(&sym->entry, i, fp);
	}
    } else {
	JS_HashTableDump(scope->data, dump, fp);
    }
}

/* These are callable from gdb. */
void Dsym(JSSymbol *sym) { if (sym) DumpSymbol(&sym->entry, 0, stderr); }
void Datom(JSAtom *atom) { if (atom) DumpAtom(&atom->entry, 0, stderr); }

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
#ifdef ARENAMETER
	    JS_DumpArenaStats(stdout);
#endif
	} else if (strcmp(bytes, "atom") == 0) {
	    printf("\natom table contents:\n");
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
		fprintf(stderr, "js: invalid stats argument %s\n",
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
static JSBool
ConvertArgs(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSBool b;
    jschar c;
    int32 i, j;
    uint32 u;
    jsdouble d, I;
    char *s;
    JSString *str;
    JSObject *obj;
    JSFunction *fun;
    jsval v;

    if (!JS_ConvertArguments(cx, argc, argv, "b/ciujdIsSofv*",
			     &b, &c, &i, &u, &j, &d, &I, &s, &str, &obj, &fun,
			     &v)) {
	return JS_FALSE;
    }
    printf("b %u, c %x (%c), i %ld, u %lu, j %ld\n", b, c, (char)c, i, u, j);
    printf("d %g, I %g, s %s, S %s, obj %s, fun %s, v %s\n",
	   d, I, s, JS_GetStringBytes(str),
	   JS_GetStringBytes(JS_ValueToString(cx, OBJECT_TO_JSVAL(obj))),
	   JS_GetStringBytes(JS_DecompileFunction(cx, fun, 4)),
	   JS_GetStringBytes(JS_ValueToString(cx, v)));
    return JS_TRUE;
}
#endif

static JSBool
BuildDate(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    printf("built on %s at %s\n", __DATE__, __TIME__);
    return JS_TRUE;
}

static JSFunctionSpec shell_functions[] = {
    {"version",         Version,        0},
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
    {0}
};

/* NOTE: These must be kept in sync with the above. */

static char *shell_help_messages[] = {
    "version [number]       Get or set JavaScript version number",
    "load ['foo.js' ...]    Load files named by string arguments",
    "print [expr ...]       Evaluate and print expressions",
    "help [name ...]        Display usage and help messages",
    "quit                   Quit mocha",
    "gc                     Run the garbage collector",
    "trap [fun] [pc] expr   Trap bytecode execution",
    "untrap [fun] [pc]      Remove a trap",
    "line2pc [fun] line     Map line number to PC",
    "pc2line [fun] [pc]     Map PC to line number",
#ifdef DEBUG
    "dis [fun]              Disassemble functions into bytecodes",
    "dissrc [fun]           Disassemble functions with source lines",
    "notes [fun]            Show source notes for functions",
    "tracing [toggle]       Turn tracing on or off",
    "stats [string ...]     Dump 'arena', 'atom', 'global' stats",
#endif
#ifdef TEST_EXPORT
    "doexp obj id           Export identified property from object",
#endif
#ifdef TEST_CVTARGS
    "cvtargs b c ...        Test JS_ConvertArguments",
#endif
    "build                  Show build date and time",
    0
};

static void
ShowHelpHeader(void)
{
    printf("%-9s %-22s %s\n", "Command", "Usage", "Description");
    printf("%-9s %-22s %s\n", "=======", "=====", "===========");
}

static void
ShowHelpForCommand(uintN n)
{
    printf("%-9.9s %s\n", shell_functions[n].name, shell_help_messages[n]);
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

    printf("%s\n", JS_GetImplementationVersion());
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
		fprintf(stderr, "Sorry, no help for %s\n",
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
	printf("adding its property %s,",
	       JS_GetStringBytes(JS_ValueToString(cx, id)));
	printf(" initial value %s\n",
	       JS_GetStringBytes(JS_ValueToString(cx, *vp)));
    }
    return JS_TRUE;
}

static JSBool
its_delProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    if (its_noisy) {
	printf("deleting its property %s,",
	       JS_GetStringBytes(JS_ValueToString(cx, id)));
	printf(" current value %s\n",
	       JS_GetStringBytes(JS_ValueToString(cx, *vp)));
    }
    return JS_TRUE;
}

static JSBool
its_getProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    if (its_noisy) {
	printf("getting its property %s,",
	       JS_GetStringBytes(JS_ValueToString(cx, id)));
	printf(" current value %s\n",
	       JS_GetStringBytes(JS_ValueToString(cx, *vp)));
    }
    return JS_TRUE;
}

static JSBool
its_setProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    if (its_noisy) {
	printf("setting its property %s,",
	       JS_GetStringBytes(JS_ValueToString(cx, id)));
	printf(" new value %s\n",
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
	printf("enumerate its properties\n");
    return JS_TRUE;
}

static JSBool
its_resolve(JSContext *cx, JSObject *obj, jsval id)
{
    if (its_noisy) {
	printf("resolving its property %s\n",
	       JS_GetStringBytes(JS_ValueToString(cx, id)));
    }
    return JS_TRUE;
}

static JSBool
its_convert(JSContext *cx, JSObject *obj, JSType type, jsval *vp)
{
    if (its_noisy)
	printf("converting it to %s type\n", JS_GetTypeName(cx, type));
    return JS_TRUE;
}

static void
its_finalize(JSContext *cx, JSObject *obj)
{
    if (its_noisy)
	printf("finalizing it\n");
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
	else
	    return NULL;
}

static void
my_ErrorReporter(JSContext *cx, const char *message, JSErrorReport *report)
{
    int i, j, k, n;
    char *prefix = NULL, *tmp;

    if (!report) {
	fprintf(stderr, "%s\n", message);
	return;
    }

    /*
     * Conditionally ignore reported warnings, and ignore error reports for
     * which a JavaScript exception has been thrown, except when we're in
     * a toplevel compile.
     */
    if ((JSREPORT_IS_WARNING(report->flags) && !reportWarnings) ||
	(JSREPORT_IS_EXCEPTION(report->flags))) {
	return;
    }

    if (report->filename)
	prefix = JS_smprintf("%s:", report->filename);
    if (report->lineno) {
	tmp = prefix;
	prefix = JS_smprintf("%s%u: ", tmp ? tmp : "", report->lineno);
	JS_free(cx, tmp);
    }
    if (JSREPORT_IS_WARNING(report->flags)) {
	tmp = prefix;
	prefix = JS_smprintf("%swarning: ", tmp ? tmp : "");
	JS_free(cx, tmp);
    }

    /* embedded newlines -- argh! */
    while ((tmp = strchr(message, '\n')) != 0) {
	tmp++;
	if (prefix) fputs(prefix, stderr);
	fwrite(message, 1, tmp - message, stderr);
	message = tmp;
    }
    /* If there were no filename or lineno, the prefix might be empty */
    if (prefix) fputs(prefix, stderr);
    fputs(message, stderr);

    if (!report->linebuf) {
	putc('\n', stderr);
	goto out;
    }

    fprintf(stderr, ":\n%s%s\n%s", prefix, report->linebuf, prefix);
    n = report->tokenptr - report->linebuf;
    for (i = j = 0; i < n; i++) {
	if (report->linebuf[i] == '\t') {
	    for (k = (j + 8) & ~7; j < k; j++)
		putc('.', stderr);
	    continue;
	}
	putc('.', stderr);
	j++;
    }
    fputs("^\n", stderr);
 out:
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

static JSBool
global_resolve(JSContext *cx, JSObject *obj, jsval id)
{
#if defined(SHELL_HACK) && defined(DEBUG) && defined(XP_UNIX)
    /*
     * Do this expensive hack only for unoptimized Unix builds, which are not
     * used for benchmarking.
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
	    fun = JS_DefineFunction(cx, obj, name, Exec, 0, JSPROP_ENUMERATE);
	    ok = (fun != NULL);
	    break;
	}
    }
    JS_free(cx, path);
    return ok;
#else
    return JS_TRUE;
#endif
}

static JSClass global_class = {
    "global", 0,
    JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,
    JS_EnumerateStub, global_resolve,   JS_ConvertStub,   JS_FinalizeStub
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

#ifdef XP_MAC
	initConsole("\pJavaScript Shell", "Welcome to js shell.", &argc, &argv);
#endif

    version = JSVERSION_DEFAULT;

    argc--;
    argv++;

    rt = JS_NewRuntime(8L * 1024L * 1024L);
    if (!rt)
	return 1;
    cx = JS_NewContext(rt, 8192);
    if (!cx)
	return 1;
    JS_SetErrorReporter(cx, my_ErrorReporter);

    glob = JS_NewObject(cx, &global_class, NULL, NULL);
    if (!glob)
	return 1;
    if (!JS_InitStandardClasses(cx, glob))
	return 1;
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

    JS_DestroyContext(cx);
    JS_DestroyRuntime(rt);
    JS_ShutDown();
    return result;
}
