/* vim: set ts=8 sts=4 et sw=4 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdlib.h>
#include <errno.h>
#ifdef HAVE_IO_H
#include <io.h>     /* for isatty() */
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>     /* for isatty() */
#endif

#include "base/basictypes.h"

#include "jsapi.h"
#include "jsdbgapi.h"
#include "jsprf.h"

#include "xpcpublic.h"

#include "XPCShellEnvironment.h"

#include "mozilla/XPCOM.h"

#include "nsIChannel.h"
#include "nsIClassInfo.h"
#include "nsIDirectoryService.h"
#include "nsIJSRuntimeService.h"
#include "nsIPrincipal.h"
#include "nsIScriptSecurityManager.h"
#include "nsIURI.h"
#include "nsIXPConnect.h"
#include "nsIXPCScriptable.h"

#include "nsContentUtils.h"
#include "nsCxPusher.h"
#include "nsJSUtils.h"
#include "nsJSPrincipals.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"

#include "BackstagePass.h"

#include "TestShellChild.h"
#include "TestShellParent.h"

#define EXITCODE_RUNTIME_ERROR 3
#define EXITCODE_FILE_NOT_FOUND 4

using mozilla::ipc::XPCShellEnvironment;
using mozilla::ipc::TestShellChild;
using mozilla::ipc::TestShellParent;

namespace {

static const char kDefaultRuntimeScriptFilename[] = "xpcshell.js";

class XPCShellDirProvider : public nsIDirectoryServiceProvider
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIDIRECTORYSERVICEPROVIDER

    XPCShellDirProvider() { }
    ~XPCShellDirProvider() { }

    bool SetGREDir(const char *dir);
    void ClearGREDir() { mGREDir = nullptr; }

private:
    nsCOMPtr<nsIFile> mGREDir;
};

inline XPCShellEnvironment*
Environment(JSContext* cx)
{
    XPCShellEnvironment* env = 
        static_cast<XPCShellEnvironment*>(JS_GetContextPrivate(cx));
    NS_ASSERTION(env, "Should never be null!");
    return env;
}

static void
ScriptErrorReporter(JSContext *cx,
                    const char *message,
                    JSErrorReport *report)
{
    int i, j, k, n;
    char *prefix = NULL, *tmp;
    const char *ctmp;
    nsCOMPtr<nsIXPConnect> xpc;

    // Don't report an exception from inner JS frames as the callers may intend
    // to handle it.
    if (JS_DescribeScriptedCaller(cx, nullptr, nullptr)) {
        return;
    }

    // In some cases cx->fp is null here so use XPConnect to tell us about inner
    // frames.
    if ((xpc = do_GetService(nsIXPConnect::GetCID()))) {
        nsAXPCNativeCallContext *cc = nullptr;
        xpc->GetCurrentNativeCallContext(&cc);
        if (cc) {
            nsAXPCNativeCallContext *prev = cc;
            while (NS_SUCCEEDED(prev->GetPreviousCallContext(&prev)) && prev) {
                uint16_t lang;
                if (NS_SUCCEEDED(prev->GetLanguage(&lang)) &&
                    lang == nsAXPCNativeCallContext::LANG_JS) {
                    return;
                }
            }
        }
    }

    if (!report) {
        fprintf(stderr, "%s\n", message);
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
        prefix = JS_smprintf("%s%swarning: ",
                             tmp ? tmp : "",
                             JSREPORT_IS_STRICT(report->flags) ? "strict " : "");
        JS_free(cx, tmp);
    }

    /* embedded newlines -- argh! */
    while ((ctmp = strchr(message, '\n')) != 0) {
        ctmp++;
        if (prefix) fputs(prefix, stderr);
        fwrite(message, 1, ctmp - message, stderr);
        message = ctmp;
    }
    /* If there were no filename or lineno, the prefix might be empty */
    if (prefix)
        fputs(prefix, stderr);
    fputs(message, stderr);

    if (!report->linebuf) {
        fputc('\n', stderr);
        goto out;
    }

    fprintf(stderr, ":\n%s%s\n%s", prefix, report->linebuf, prefix);
    n = report->tokenptr - report->linebuf;
    for (i = j = 0; i < n; i++) {
        if (report->linebuf[i] == '\t') {
            for (k = (j + 8) & ~7; j < k; j++) {
                fputc('.', stderr);
            }
            continue;
        }
        fputc('.', stderr);
        j++;
    }
    fputs("^\n", stderr);
 out:
    if (!JSREPORT_IS_WARNING(report->flags)) {
        Environment(cx)->SetExitCode(EXITCODE_RUNTIME_ERROR);
    }
    JS_free(cx, prefix);
}

JSContextCallback gOldContextCallback = NULL;

static JSBool
ContextCallback(JSContext *cx,
                unsigned contextOp)
{
    if (gOldContextCallback && !gOldContextCallback(cx, contextOp))
        return JS_FALSE;

    if (contextOp == JSCONTEXT_NEW) {
        JS_SetErrorReporter(cx, ScriptErrorReporter);
    }
    return JS_TRUE;
}

static JSBool
Print(JSContext *cx,
      unsigned argc,
      JS::Value *vp)
{
    unsigned i, n;
    JSString *str;

    JS::Value *argv = JS_ARGV(cx, vp);
    for (i = n = 0; i < argc; i++) {
        str = JS_ValueToString(cx, argv[i]);
        if (!str)
            return JS_FALSE;
        JSAutoByteString bytes(cx, str);
        if (!bytes)
            return JS_FALSE;
        fprintf(stdout, "%s%s", i ? " " : "", bytes.ptr());
        fflush(stdout);
    }
    n++;
    if (n)
        fputc('\n', stdout);
    JS_SET_RVAL(cx, vp, JSVAL_VOID);
    return JS_TRUE;
}

static JSBool
GetLine(char *bufp,
        FILE *file,
        const char *prompt)
{
    char line[256];
    fputs(prompt, stdout);
    fflush(stdout);
    if (!fgets(line, sizeof line, file))
        return JS_FALSE;
    strcpy(bufp, line);
    return JS_TRUE;
}

static JSBool
Dump(JSContext *cx,
     unsigned argc,
     JS::Value *vp)
{
    JS_SET_RVAL(cx, vp, JSVAL_VOID);

    JSString *str;
    if (!argc)
        return JS_TRUE;

    str = JS_ValueToString(cx, JS_ARGV(cx, vp)[0]);
    if (!str)
        return JS_FALSE;
    JSAutoByteString bytes(cx, str);
    if (!bytes)
      return JS_FALSE;

    fputs(bytes.ptr(), stdout);
    fflush(stdout);
    return JS_TRUE;
}

static JSBool
Load(JSContext *cx,
     unsigned argc,
     JS::Value *vp)
{
    JS::Rooted<JS::Value> result(cx);

    JS::Rooted<JSObject*> obj(cx, JS_THIS_OBJECT(cx, vp));
    if (!obj)
        return JS_FALSE;

    JS::Value *argv = JS_ARGV(cx, vp);
    for (unsigned i = 0; i < argc; i++) {
        JSString *str = JS_ValueToString(cx, argv[i]);
        if (!str)
            return JS_FALSE;
        argv[i] = STRING_TO_JSVAL(str);
        JSAutoByteString filename(cx, str);
        if (!filename)
            return JS_FALSE;
        FILE *file = fopen(filename.ptr(), "r");
        if (!file) {
            JS_ReportError(cx, "cannot open file '%s' for reading", filename.ptr());
            return JS_FALSE;
        }
        JS::CompileOptions options(cx);
        options.setUTF8(true)
               .setFileAndLine(filename.ptr(), 1)
               .setPrincipals(Environment(cx)->GetPrincipal());
        JS::RootedObject rootedObj(cx, obj);
        JSScript *script = JS::Compile(cx, rootedObj, options, file);
        fclose(file);
        if (!script)
            return JS_FALSE;

        if (!JS_ExecuteScript(cx, obj, script, result.address())) {
            return JS_FALSE;
        }
    }
    JS_SET_RVAL(cx, vp, JSVAL_VOID);
    return JS_TRUE;
}

static JSBool
Version(JSContext *cx,
        unsigned argc,
        JS::Value *vp)
{
    JS::Value *argv = JS_ARGV(cx, vp);
    JS_SET_RVAL(cx, vp, INT_TO_JSVAL(JS_GetVersion(cx)));
    if (argc > 0 && JSVAL_IS_INT(argv[0]))
        JS_SetVersionForCompartment(js::GetContextCompartment(cx),
                                    JSVersion(JSVAL_TO_INT(argv[0])));
    return JS_TRUE;
}

static JSBool
BuildDate(JSContext *cx, unsigned argc, JS::Value *vp)
{
    fprintf(stdout, "built on %s at %s\n", __DATE__, __TIME__);
    return JS_TRUE;
}

static JSBool
Quit(JSContext *cx,
     unsigned argc,
     JS::Value *vp)
{
    int exitCode = 0;
    JS_ConvertArguments(cx, argc, JS_ARGV(cx, vp), "/ i", &exitCode);

    XPCShellEnvironment* env = Environment(cx);
    env->SetExitCode(exitCode);
    env->SetIsQuitting();

    return JS_FALSE;
}

static JSBool
DumpXPC(JSContext *cx,
        unsigned argc,
        JS::Value *vp)
{
    int32_t depth = 2;

    if (argc > 0) {
        if (!JS_ValueToInt32(cx, JS_ARGV(cx, vp)[0], &depth))
            return JS_FALSE;
    }

    nsCOMPtr<nsIXPConnect> xpc = do_GetService(nsIXPConnect::GetCID());
    if(xpc)
        xpc->DebugDump(int16_t(depth));
    JS_SET_RVAL(cx, vp, JSVAL_VOID);
    return JS_TRUE;
}

static JSBool
GC(JSContext *cx,
   unsigned argc,
   JS::Value *vp)
{
    JSRuntime *rt = JS_GetRuntime(cx);
    JS_GC(rt);
#ifdef JS_GCMETER
    js_DumpGCStats(rt, stdout);
#endif
    JS_SET_RVAL(cx, vp, JSVAL_VOID);
    return JS_TRUE;
}

#ifdef JS_GC_ZEAL
static JSBool
GCZeal(JSContext *cx, 
       unsigned argc,
       JS::Value *vp)
{
  JS::Value* argv = JS_ARGV(cx, vp);

  uint32_t zeal;
  if (!JS_ValueToECMAUint32(cx, argv[0], &zeal))
    return JS_FALSE;

  JS_SetGCZeal(cx, uint8_t(zeal), JS_DEFAULT_ZEAL_FREQ);
  return JS_TRUE;
}
#endif

#ifdef DEBUG

static JSBool
DumpHeap(JSContext *cx,
         unsigned argc,
         JS::Value *vp)
{
    JSAutoByteString fileName;
    void* startThing = NULL;
    JSGCTraceKind startTraceKind = JSTRACE_OBJECT;
    void *thingToFind = NULL;
    size_t maxDepth = (size_t)-1;
    void *thingToIgnore = NULL;
    FILE *dumpFile;
    JSBool ok;

    JS::Value *argv = JS_ARGV(cx, vp);
    JS_SET_RVAL(cx, vp, JSVAL_VOID);

    vp = argv + 0;
    if (argc > 0 && *vp != JSVAL_NULL && *vp != JSVAL_VOID) {
        JSString *str;

        str = JS_ValueToString(cx, *vp);
        if (!str)
            return JS_FALSE;
        *vp = STRING_TO_JSVAL(str);
        if (!fileName.encodeLatin1(cx, str))
            return JS_FALSE;
    }

    vp = argv + 1;
    if (argc > 1 && *vp != JSVAL_NULL && *vp != JSVAL_VOID) {
        if (!JSVAL_IS_TRACEABLE(*vp))
            goto not_traceable_arg;
        startThing = JSVAL_TO_TRACEABLE(*vp);
        startTraceKind = JSVAL_TRACE_KIND(*vp);
    }

    vp = argv + 2;
    if (argc > 2 && *vp != JSVAL_NULL && *vp != JSVAL_VOID) {
        if (!JSVAL_IS_TRACEABLE(*vp))
            goto not_traceable_arg;
        thingToFind = JSVAL_TO_TRACEABLE(*vp);
    }

    vp = argv + 3;
    if (argc > 3 && *vp != JSVAL_NULL && *vp != JSVAL_VOID) {
        uint32_t depth;

        if (!JS_ValueToECMAUint32(cx, *vp, &depth))
            return JS_FALSE;
        maxDepth = depth;
    }

    vp = argv + 4;
    if (argc > 4 && *vp != JSVAL_NULL && *vp != JSVAL_VOID) {
        if (!JSVAL_IS_TRACEABLE(*vp))
            goto not_traceable_arg;
        thingToIgnore = JSVAL_TO_TRACEABLE(*vp);
    }

    if (!fileName) {
        dumpFile = stdout;
    } else {
        dumpFile = fopen(fileName.ptr(), "w");
        if (!dumpFile) {
            fprintf(stderr, "dumpHeap: can't open %s: %s\n",
                    fileName.ptr(), strerror(errno));
            return JS_FALSE;
        }
    }

    ok = JS_DumpHeap(JS_GetRuntime(cx), dumpFile, startThing, startTraceKind, thingToFind,
                     maxDepth, thingToIgnore);
    if (dumpFile != stdout)
        fclose(dumpFile);
    if (!ok)
        JS_ReportOutOfMemory(cx);
    return ok;

  not_traceable_arg:
    fprintf(stderr,
            "dumpHeap: argument %u is not null or a heap-allocated thing\n",
            (unsigned)(vp - argv));
    return JS_FALSE;
}

#endif /* DEBUG */

const JSFunctionSpec gGlobalFunctions[] =
{
    JS_FS("print",           Print,          0,0),
    JS_FS("load",            Load,           1,0),
    JS_FS("quit",            Quit,           0,0),
    JS_FS("version",         Version,        1,0),
    JS_FS("build",           BuildDate,      0,0),
    JS_FS("dumpXPC",         DumpXPC,        1,0),
    JS_FS("dump",            Dump,           1,0),
    JS_FS("gc",              GC,             0,0),
 #ifdef JS_GC_ZEAL
    JS_FS("gczeal",          GCZeal,         1,0),
 #endif
 #ifdef DEBUG
    JS_FS("dumpHeap",        DumpHeap,       5,0),
 #endif
    JS_FS_END
};

typedef enum JSShellErrNum
{
#define MSG_DEF(name, number, count, exception, format) \
    name = number,
#include "jsshell.msg"
#undef MSG_DEF
    JSShellErr_Limit
#undef MSGDEF
} JSShellErrNum;

static void
ProcessFile(JSContext *cx,
            JS::Handle<JSObject*> obj,
            const char *filename,
            FILE *file,
            JSBool forceTTY)
{
    XPCShellEnvironment* env = Environment(cx);
    nsCxPusher pusher;
    pusher.Push(env->GetContext());

    JSScript *script;
    JS::Rooted<JS::Value> result(cx);
    int lineno, startline;
    JSBool ok, hitEOF;
    char *bufp, buffer[4096];
    JSString *str;

    if (forceTTY) {
        file = stdin;
    }
    else
#ifdef HAVE_ISATTY
    if (!isatty(fileno(file)))
#endif
    {
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
                if(ch == '\n' || ch == '\r')
                    break;
            }
        }
        ungetc(ch, file);

        JSAutoRequest ar(cx);
        JSAutoCompartment ac(cx, obj);

        JS::CompileOptions options(cx);
        options.setUTF8(true)
               .setFileAndLine(filename, 1)
               .setPrincipals(env->GetPrincipal());
        JSScript* script = JS::Compile(cx, obj, options, file);
        if (script)
            (void)JS_ExecuteScript(cx, obj, script, result.address());

        return;
    }

    /* It's an interactive filehandle; drop into read-eval-print loop. */
    lineno = 1;
    hitEOF = JS_FALSE;
    do {
        bufp = buffer;
        *bufp = '\0';

        JSAutoRequest ar(cx);
        JSAutoCompartment ac(cx, obj);

        /*
         * Accumulate lines until we get a 'compilable unit' - one that either
         * generates an error (before running out of source) or that compiles
         * cleanly.  This should be whenever we get a complete statement that
         * coincides with the end of a line.
         */
        startline = lineno;
        do {
            if (!GetLine(bufp, file, startline == lineno ? "js> " : "")) {
                hitEOF = JS_TRUE;
                break;
            }
            bufp += strlen(bufp);
            lineno++;
        } while (!JS_BufferIsCompilableUnit(cx, obj, buffer, strlen(buffer)));

        /* Clear any pending exception from previous failed compiles.  */
        JS_ClearPendingException(cx);
        script =
            JS_CompileScriptForPrincipals(cx, obj, env->GetPrincipal(), buffer,
                                          strlen(buffer), "typein", startline);
        if (script) {
            JSErrorReporter older;

            ok = JS_ExecuteScript(cx, obj, script, result.address());
            if (ok && result != JSVAL_VOID) {
                /* Suppress error reports from JS_ValueToString(). */
                older = JS_SetErrorReporter(cx, NULL);
                str = JS_ValueToString(cx, result);
                JSAutoByteString bytes;
                if (str)
                    bytes.encodeLatin1(cx, str);
                JS_SetErrorReporter(cx, older);

                if (!!bytes)
                    fprintf(stdout, "%s\n", bytes.ptr());
                else
                    ok = JS_FALSE;
            }
        }
    } while (!hitEOF && !env->IsQuitting());

    fprintf(stdout, "\n");
}

} /* anonymous namespace */

NS_IMETHODIMP_(nsrefcnt)
XPCShellDirProvider::AddRef()
{
    return 2;
}

NS_IMETHODIMP_(nsrefcnt)
XPCShellDirProvider::Release()
{
    return 1;
}

NS_IMPL_QUERY_INTERFACE1(XPCShellDirProvider, nsIDirectoryServiceProvider)

bool
XPCShellDirProvider::SetGREDir(const char *dir)
{
    nsresult rv = XRE_GetFileFromPath(dir, getter_AddRefs(mGREDir));
    return NS_SUCCEEDED(rv);
}

NS_IMETHODIMP
XPCShellDirProvider::GetFile(const char *prop,
                             bool *persistent,
                             nsIFile* *result)
{
    if (mGREDir && !strcmp(prop, NS_GRE_DIR)) {
        *persistent = true;
        NS_ADDREF(*result = mGREDir);
        return NS_OK;
    }

    return NS_ERROR_FAILURE;
}

// static
XPCShellEnvironment*
XPCShellEnvironment::CreateEnvironment()
{
    XPCShellEnvironment* env = new XPCShellEnvironment();
    if (env && !env->Init()) {
        delete env;
        env = nullptr;
    }
    return env;
}

XPCShellEnvironment::XPCShellEnvironment()
:   mCx(NULL),
    mJSPrincipals(NULL),
    mExitCode(0),
    mQuitting(JS_FALSE)
{
}

XPCShellEnvironment::~XPCShellEnvironment()
{
    if (mCx) {
        JS_BeginRequest(mCx);

        JSObject* global = GetGlobalObject();
        if (global) {
            JS_SetAllNonReservedSlotsToUndefined(mCx, global);
        }
        mGlobalHolder.Release();

        JSRuntime *rt = JS_GetRuntime(mCx);
        JS_GC(rt);

        if (mJSPrincipals) {
            JS_DropPrincipals(rt, mJSPrincipals);
        }

        JS_EndRequest(mCx);
        JS_DestroyContext(mCx);

        if (gOldContextCallback) {
            NS_ASSERTION(rt, "Should never be null!");
            JS_SetContextCallback(rt, gOldContextCallback);
            gOldContextCallback = NULL;
        }
    }
}

bool
XPCShellEnvironment::Init()
{
    nsresult rv;

#ifdef HAVE_SETBUF
    // unbuffer stdout so that output is in the correct order; note that stderr
    // is unbuffered by default
    setbuf(stdout, 0);
#endif

    nsCOMPtr<nsIJSRuntimeService> rtsvc =
        do_GetService("@mozilla.org/js/xpc/RuntimeService;1");
    if (!rtsvc) {
        NS_ERROR("failed to get nsJSRuntimeService!");
        return false;
    }

    JSRuntime *rt;
    if (NS_FAILED(rtsvc->GetRuntime(&rt)) || !rt) {
        NS_ERROR("failed to get JSRuntime from nsJSRuntimeService!");
        return false;
    }

    if (!mGlobalHolder.Hold(rt)) {
        NS_ERROR("Can't protect global object!");
        return false;
    }

    gOldContextCallback = JS_SetContextCallback(rt, ContextCallback);

    JSContext *cx = JS_NewContext(rt, 8192);
    if (!cx) {
        NS_ERROR("JS_NewContext failed!");

        JS_SetContextCallback(rt, gOldContextCallback);
        gOldContextCallback = NULL;

        return false;
    }
    mCx = cx;

    JS_SetContextPrivate(cx, this);

    nsCOMPtr<nsIXPConnect> xpc =
      do_GetService(nsIXPConnect::GetCID());
    if (!xpc) {
        NS_ERROR("failed to get nsXPConnect service!");
        return false;
    }

    nsCOMPtr<nsIPrincipal> principal;
    nsCOMPtr<nsIScriptSecurityManager> securityManager =
        do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv) && securityManager) {
        rv = securityManager->GetSystemPrincipal(getter_AddRefs(principal));
        if (NS_FAILED(rv)) {
            fprintf(stderr, "+++ Failed to obtain SystemPrincipal from ScriptSecurityManager service.\n");
        } else {
            // fetch the JS principals and stick in a global
            mJSPrincipals = nsJSPrincipals::get(principal);
            JS_HoldPrincipals(mJSPrincipals);
        }
    } else {
        fprintf(stderr, "+++ Failed to get ScriptSecurityManager service, running without principals");
    }

    nsCxPusher pusher;
    pusher.Push(mCx);

    nsRefPtr<BackstagePass> backstagePass;
    rv = NS_NewBackstagePass(getter_AddRefs(backstagePass));
    if (NS_FAILED(rv)) {
        NS_ERROR("Failed to create backstage pass!");
        return false;
    }

    JS::CompartmentOptions options;
    options.setZone(JS::SystemZone)
           .setVersion(JSVERSION_LATEST);
    nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
    rv = xpc->InitClassesWithNewWrappedGlobal(cx,
                                              static_cast<nsIGlobalObject *>(backstagePass),
                                              principal, 0,
                                              options,
                                              getter_AddRefs(holder));
    if (NS_FAILED(rv)) {
        NS_ERROR("InitClassesWithNewWrappedGlobal failed!");
        return false;
    }

    JS::Rooted<JSObject*> globalObj(cx, holder->GetJSObject());
    if (!globalObj) {
        NS_ERROR("Failed to get global JSObject!");
        return false;
    }

    backstagePass->SetGlobalObject(globalObj);

    {
        JSAutoRequest ar(cx);
        JSAutoCompartment ac(cx, globalObj);

        if (!JS_DefineFunctions(cx, globalObj, gGlobalFunctions) ||
	    !JS_DefineProfilingFunctions(cx, globalObj)) {
            NS_ERROR("JS_DefineFunctions failed!");
            return false;
        }
    }

    mGlobalHolder = globalObj;

    FILE* runtimeScriptFile = fopen(kDefaultRuntimeScriptFilename, "r");
    if (runtimeScriptFile) {
        fprintf(stdout, "[loading '%s'...]\n", kDefaultRuntimeScriptFilename);
        ProcessFile(cx, globalObj, kDefaultRuntimeScriptFilename,
                    runtimeScriptFile, JS_FALSE);
        fclose(runtimeScriptFile);
    }

    return true;
}

bool
XPCShellEnvironment::EvaluateString(const nsString& aString,
                                    nsString* aResult)
{
  XPCShellEnvironment* env = Environment(mCx);
  nsCxPusher pusher;
  pusher.Push(env->GetContext());

  JSAutoRequest ar(mCx);

  JS_ClearPendingException(mCx);

  JS::Rooted<JSObject*> global(mCx, GetGlobalObject());
  JSAutoCompartment ac(mCx, global);

  JSScript* script =
      JS_CompileUCScriptForPrincipals(mCx, global, GetPrincipal(),
                                      aString.get(), aString.Length(),
                                      "typein", 0);
  if (!script) {
     return false;
  }

  if (aResult) {
      aResult->Truncate();
  }

  JS::Rooted<JS::Value> result(mCx);
  JSBool ok = JS_ExecuteScript(mCx, global, script, result.address());
  if (ok && result != JSVAL_VOID) {
      JSErrorReporter old = JS_SetErrorReporter(mCx, NULL);
      JSString* str = JS_ValueToString(mCx, result);
      nsDependentJSString depStr;
      if (str)
          depStr.init(mCx, str);
      JS_SetErrorReporter(mCx, old);

      if (!depStr.IsEmpty() && aResult) {
          aResult->Assign(depStr);
      }
  }

  return true;
}
