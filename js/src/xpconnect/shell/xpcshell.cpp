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

/* XPConnect JavaScript interactive shell. */

#include <stdio.h>
#include "nsIXPConnect.h"
#include "nsIXPCScriptable.h"
#include "nsIInterfaceInfo.h"
#include "nsIInterfaceInfoManager.h"
#include "nsIXPCScriptable.h"
#include "nsIServiceManager.h"
#include "nsIComponentManager.h"
#include "jsapi.h"
#include "xpclog.h"
#include "nscore.h"
#include "nsIAllocator.h"
#include "nsIGenericFactory.h"

// all this crap is needed to do the interactive shell stuff
#include <stdlib.h>
#include <errno.h>
#ifdef XP_PC
#include <io.h>     /* for isatty() */
#else
#ifdef XP_UNIX
#include <unistd.h>     /* for isatty() */
#endif
#endif
#include "jsparse.h"
#include "jsscan.h"
#include "jsemit.h"
#include "jsscript.h"
#include "jsarena.h"
#include "jscntxt.h"

#include "nsSpecialSystemDirectory.h"	// For exe dir


/***************************************************************************/
static NS_DEFINE_CID(kGenericFactoryCID, NS_GENERICFACTORY_CID);
static NS_DEFINE_IID(kIAllocatorIID, NS_IALLOCATOR_IID);
static NS_DEFINE_IID(kAllocatorCID, NS_ALLOCATOR_CID);

#ifdef XP_PC
#define XPCOM_DLL  "xpcom32.dll"
#else
#ifdef XP_MAC
#define XPCOM_DLL  "XPCOM_DLL"
#else
#define XPCOM_DLL  "libxpcom.so"
#endif
#endif

static void SetupRegistry()
{
  // Autoregistration happens here. The rest of RegisterComponent() calls should happen
  // only for dlls not in the components directory.

  // Create exeDir/"components"
  nsSpecialSystemDirectory sysdir(nsSpecialSystemDirectory::OS_CurrentProcessDirectory);
  sysdir += "components";
  const char *componentsDir = sysdir.GetCString(); // native path
  if (componentsDir != NULL)
  {
#ifdef XP_PC
      /* The PC version of the directory from filePath is of the form
       *	/y|/moz/mozilla/dist/bin/components
       * We need to remove the initial / and change the | to :
       * for all this to work with NSPR.	  
       */
#endif /* XP_PC */
      printf("nsComponentManager: Using components dir: %s\n", componentsDir);

#ifdef XP_MAC
      nsComponentManager::AutoRegister(nsIComponentManager::NS_Startup, nsnull);
#else
      nsComponentManager::AutoRegister(nsIComponentManager::NS_Startup, componentsDir);
#endif	/* XP_MAC */
      // XXX Look for user specific components
      // XXX UNIX: ~/.mozilla/components
  }

//    nsComponentManager::RegisterComponent(kAllocatorCID, NULL, NULL,
//                                          XPCOM_DLL, PR_FALSE, PR_FALSE);

    nsComponentManager::RegisterComponent(kAllocatorCID, NULL, "allocator", 
                                          XPCOM_DLL, PR_TRUE, PR_TRUE);

    nsComponentManager::RegisterComponent(kGenericFactoryCID, NULL, NULL, 
                                          XPCOM_DLL, PR_FALSE, PR_FALSE);
}
/***************************************************************************/

FILE *gOutFile = NULL;
FILE *gErrFile = NULL;

static void
my_ErrorReporter(JSContext *cx, const char *message, JSErrorReport *report)
{
    printf("%s\n", message);
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
Version(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    if (argc > 0 && JSVAL_IS_INT(argv[0]))
        *rval = INT_TO_JSVAL(JS_SetVersion(cx, JSVersion(JSVAL_TO_INT(argv[0]))));
    else
        *rval = INT_TO_JSVAL(JS_GetVersion(cx));
    return JS_TRUE;
}

static JSBool
BuildDate(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    fprintf(gOutFile, "built on %s at %s\n", __DATE__, __TIME__);
    return JS_TRUE;
}

static JSBool
Quit(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
#ifdef LIVECONNECT
    JSJ_SimpleShutdown();
#endif
    exit(0);
    return JS_FALSE;
}

static JSBool
Dump(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    int32 depth = 2;
        
    if (argc > 0) {
        if (!JS_ValueToInt32(cx, argv[0], &depth))
            return JS_FALSE;
    }
    XPC_DUMP(XPC_GetXPConnect(), depth);
    return JS_TRUE;
}

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

static JSFunctionSpec glob_functions[] = {
    {"print",           Print,          0},
    {"load",            Load,           1},
    {"quit",            Quit,           0},
    {"version",         Version,        1},
    {"build",           BuildDate,      0},
    {"dump",            Dump,           1},
    {"gc",              GC,             0},
    {0}
};

static JSClass global_class = {
    "global", 0,
    JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,   JS_FinalizeStub
};

/***************************************************************************/

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
        int ch = fgetc(ts->file);
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
                fprintf(gOutFile, "js> ");
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
                
                ok = JS_ExecuteScript(cx, obj, script, &result);
                if (ok &&
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
                        fprintf(gOutFile, "%s\n", JS_GetStringBytes(str));
                    else
                        ok = JS_FALSE;
                }

#if JS_HAS_ERROR_EXCEPTIONS
#if 0
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
#endif
#endif /* JS_HAS_ERROR_EXCEPTIONS */
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
    fprintf(gErrFile, "%s\n", JS_GetImplementationVersion());
    fprintf(gErrFile, "usage: js [-w] [-v version] [-f scriptfile] [scriptfile] [scriptarg...]\n");
    return 2;
}

static int reportWarnings;

static int
ProcessArgs(JSContext *cx, JSObject *obj, char **argv, int argc)
{
    int i;
    char *filename = NULL;
    jsint length;
    jsval *vector;
    jsval *p;
    JSObject *argsObj;
    JSBool isInteractive = JS_TRUE;

    for (i=0; i < argc; i++) {
        if (argv[i][0] == '-') {
            switch (argv[i][1]) {
            case 'v':
                if (i+1 == argc) {
                    return usage();
                }
                JS_SetVersion(cx, JSVersion(atoi(argv[i+1])));
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
    vector = (jsval*) JS_malloc(cx, length * sizeof(jsval));
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

    if (filename || isInteractive)
        Process(cx, obj, filename);
    return 0;
}

/***************************************************************************/

int
main(int argc, char **argv)
{
    JSRuntime *rt;
    JSContext *jscontext;
    JSObject *glob;
    int result;

    gErrFile = stderr;
    gOutFile = stdout;

    SetupRegistry();

    rt = JS_NewRuntime(8L * 1024L * 1024L);
    if (!rt)
        return 1;
    jscontext = JS_NewContext(rt, 8192);
    if (!jscontext)
        return 1;

    JS_SetErrorReporter(jscontext, my_ErrorReporter);

    nsIXPConnect* xpc = XPC_GetXPConnect();
    if(!xpc)
    {
        printf("XPC_GetXPConnect() returned NULL!\n");
        return 1;
    }

    glob = JS_NewObject(jscontext, &global_class, NULL, NULL);
    if (!glob)
        return 1;

    if (!JS_InitStandardClasses(jscontext, glob))
        return 1;
    if (!JS_DefineFunctions(jscontext, glob, glob_functions))
        return 1;

    xpc->InitJSContext(jscontext, glob, JS_TRUE);

    argc--;
    argv++;

    result = ProcessArgs(jscontext, glob, argv, argc);

    NS_RELEASE(xpc);
    JS_DestroyContext(jscontext);
    JS_DestroyRuntime(rt);
    JS_ShutDown();
    return result;
}

/***************************************************************************/

#include "jsatom.h"
#ifdef DEBUG
int
DumpAtom(JSHashEntry *he, int i, void *arg)
{
    FILE *fp = (FILE *)arg;
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
    FILE *fp = (FILE *)arg;
    JSSymbol *sym = (JSSymbol *)he;

    fprintf(fp, "%3d %08x", i, (uintN)he->keyHash);
    if (JSVAL_IS_INT(sym_id(sym)))
        fprintf(fp, " [%ld]\n", (long)JSVAL_TO_INT(sym_id(sym)));
    else
        fprintf(fp, " \"%s\"\n", ATOM_BYTES(sym_atom(sym)));
    return HT_ENUMERATE_NEXT;
}

/* These are callable from gdb. */
JS_BEGIN_EXTERN_C
void Dsym(JSSymbol *sym) { if (sym) DumpSymbol(&sym->entry, 0, gErrFile); }
void Datom(JSAtom *atom) { if (atom) DumpAtom(&atom->entry, 0, gErrFile); }
void Dobj(nsISupports* p, int depth) {if(p)XPC_DUMP(p,depth);}
void Dxpc(int depth) {Dobj(XPC_GetXPConnect(), depth);}
JS_END_EXTERN_C
#endif
