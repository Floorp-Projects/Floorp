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
#include "js/OldDebugAPI.h"

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

#include "nsCxPusher.h"
#include "nsJSUtils.h"
#include "nsJSPrincipals.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"

#include "BackstagePass.h"

#include "TestShellChild.h"
#include "TestShellParent.h"

using mozilla::ipc::XPCShellEnvironment;
using mozilla::ipc::TestShellChild;
using mozilla::ipc::TestShellParent;
using mozilla::AutoSafeJSContext;
using namespace JS;

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
Environment(Handle<JSObject*> global)
{
    AutoSafeJSContext cx;
    JSAutoCompartment ac(cx, global);
    Rooted<Value> v(cx);
    if (!JS_GetProperty(cx, global, "__XPCShellEnvironment", &v) ||
        !v.get().isDouble())
    {
        return nullptr;
    }
    return static_cast<XPCShellEnvironment*>(v.get().toPrivate());
}

static bool
Print(JSContext *cx, unsigned argc, JS::Value *vp)
{
    JS::CallArgs args = JS::CallArgsFromVp(argc, vp);

    for (unsigned i = 0; i < args.length(); i++) {
        JSString *str = JS::ToString(cx, args[i]);
        if (!str)
            return false;
        JSAutoByteString bytes(cx, str);
        if (!bytes)
            return false;
        fprintf(stdout, "%s%s", i ? " " : "", bytes.ptr());
        fflush(stdout);
    }
    fputc('\n', stdout);
    args.rval().setUndefined();
    return true;
}

static bool
GetLine(char *bufp,
        FILE *file,
        const char *prompt)
{
    char line[256];
    fputs(prompt, stdout);
    fflush(stdout);
    if (!fgets(line, sizeof line, file))
        return false;
    strcpy(bufp, line);
    return true;
}

static bool
Dump(JSContext *cx, unsigned argc, JS::Value *vp)
{
    JS::CallArgs args = JS::CallArgsFromVp(argc, vp);

    if (!args.length())
        return true;

    JSString *str = JS::ToString(cx, args[0]);
    if (!str)
        return false;
    JSAutoByteString bytes(cx, str);
    if (!bytes)
      return false;

    fputs(bytes.ptr(), stdout);
    fflush(stdout);
    return true;
}

static bool
Load(JSContext *cx,
     unsigned argc,
     JS::Value *vp)
{
    JS::CallArgs args = JS::CallArgsFromVp(argc, vp);

    JS::Rooted<JSObject*> obj(cx, JS_THIS_OBJECT(cx, vp));
    if (!obj)
        return false;

    for (unsigned i = 0; i < args.length(); i++) {
        JS::Rooted<JSString*> str(cx, JS::ToString(cx, args[i]));
        if (!str)
            return false;
        JSAutoByteString filename(cx, str);
        if (!filename)
            return false;
        FILE *file = fopen(filename.ptr(), "r");
        if (!file) {
            JS_ReportError(cx, "cannot open file '%s' for reading", filename.ptr());
            return false;
        }
        Rooted<JSObject*> global(cx, JS::CurrentGlobalOrNull(cx));
        JS::CompileOptions options(cx);
        options.setUTF8(true)
               .setFileAndLine(filename.ptr(), 1);
        JS::Rooted<JSScript*> script(cx, JS::Compile(cx, obj, options, file));
        fclose(file);
        if (!script)
            return false;

        JS::Rooted<JS::Value> result(cx);
        if (!JS_ExecuteScript(cx, obj, script, result.address())) {
            return false;
        }
    }
    args.rval().setUndefined();
    return true;
}

static bool
Version(JSContext *cx,
        unsigned argc,
        JS::Value *vp)
{
    JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
    args.rval().setInt32(JS_GetVersion(cx));
    if (args.get(0).isInt32())
        JS_SetVersionForCompartment(js::GetContextCompartment(cx),
                                    JSVersion(args[0].toInt32()));
    return true;
}

static bool
BuildDate(JSContext *cx, unsigned argc, JS::Value *vp)
{
    JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
    fprintf(stdout, "built on %s at %s\n", __DATE__, __TIME__);
    args.rval().setUndefined();
    return true;
}

static bool
Quit(JSContext *cx,
     unsigned argc,
     JS::Value *vp)
{
    Rooted<JSObject*> global(cx, JS::CurrentGlobalOrNull(cx));
    XPCShellEnvironment* env = Environment(global);
    env->SetIsQuitting();

    return false;
}

static bool
DumpXPC(JSContext *cx,
        unsigned argc,
        JS::Value *vp)
{
    JS::CallArgs args = CallArgsFromVp(argc, vp);

    uint16_t depth = 2;
    if (args.length() > 0) {
        if (!JS::ToUint16(cx, args[0], &depth))
            return false;
    }

    nsCOMPtr<nsIXPConnect> xpc = do_GetService(nsIXPConnect::GetCID());
    if (xpc)
        xpc->DebugDump(int16_t(depth));
    args.rval().setUndefined();
    return true;
}

static bool
GC(JSContext *cx,
   unsigned argc,
   JS::Value *vp)
{
    JS::CallArgs args = JS::CallArgsFromVp(argc, vp);

    JSRuntime *rt = JS_GetRuntime(cx);
    JS_GC(rt);
#ifdef JS_GCMETER
    js_DumpGCStats(rt, stdout);
#endif
    args.rval().setUndefined();
    return true;
}

#ifdef JS_GC_ZEAL
static bool
GCZeal(JSContext *cx, unsigned argc, JS::Value *vp)
{
  CallArgs args = CallArgsFromVp(argc, vp);

  uint32_t zeal;
  if (!ToUint32(cx, args.get(0), &zeal))
    return false;

  JS_SetGCZeal(cx, uint8_t(zeal), JS_DEFAULT_ZEAL_FREQ);
  return true;
}
#endif

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

} /* anonymous namespace */

void
XPCShellEnvironment::ProcessFile(JSContext *cx,
                                 JS::Handle<JSObject*> obj,
                                 const char *filename,
                                 FILE *file,
                                 bool forceTTY)
{
    XPCShellEnvironment* env = this;

    JS::Rooted<JS::Value> result(cx);
    int lineno, startline;
    bool ok, hitEOF;
    char *bufp, buffer[4096];
    JSString *str;

    if (forceTTY) {
        file = stdin;
    }
    else if (!isatty(fileno(file)))
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
               .setFileAndLine(filename, 1);
        JS::Rooted<JSScript*> script(cx, JS::Compile(cx, obj, options, file));
        if (script)
            (void)JS_ExecuteScript(cx, obj, script, result.address());

        return;
    }

    /* It's an interactive filehandle; drop into read-eval-print loop. */
    lineno = 1;
    hitEOF = false;
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
                hitEOF = true;
                break;
            }
            bufp += strlen(bufp);
            lineno++;
        } while (!JS_BufferIsCompilableUnit(cx, obj, buffer, strlen(buffer)));

        /* Clear any pending exception from previous failed compiles.  */
        JS_ClearPendingException(cx);
        JS::CompileOptions options(cx);
        options.setFileAndLine("typein", startline);
        JS::Rooted<JSScript*> script(cx,
                                     JS_CompileScript(cx, obj, buffer, strlen(buffer), options));
        if (script) {
            JSErrorReporter older;

            ok = JS_ExecuteScript(cx, obj, script, result.address());
            if (ok && result != JSVAL_VOID) {
                /* Suppress error reports from JS::ToString(). */
                older = JS_SetErrorReporter(cx, nullptr);
                str = JS::ToString(cx, result);
                JSAutoByteString bytes;
                if (str)
                    bytes.encodeLatin1(cx, str);
                JS_SetErrorReporter(cx, older);

                if (!!bytes)
                    fprintf(stdout, "%s\n", bytes.ptr());
                else
                    ok = false;
            }
        }
    } while (!hitEOF && !env->IsQuitting());

    fprintf(stdout, "\n");
}

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
:   mQuitting(false)
{
}

XPCShellEnvironment::~XPCShellEnvironment()
{

    AutoSafeJSContext cx;
    Rooted<JSObject*> global(cx, GetGlobalObject());
    if (global) {
        {
            JSAutoCompartment ac(cx, global);
            JS_SetAllNonReservedSlotsToUndefined(cx, global);
        }
        mGlobalHolder.Release();

        JSRuntime *rt = JS_GetRuntime(cx);
        JS_GC(rt);
    }
}

bool
XPCShellEnvironment::Init()
{
    nsresult rv;

    // unbuffer stdout so that output is in the correct order; note that stderr
    // is unbuffered by default
    setbuf(stdout, 0);

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

    AutoSafeJSContext cx;

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
        }
    } else {
        fprintf(stderr, "+++ Failed to get ScriptSecurityManager service, running without principals");
    }

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
    JSAutoCompartment ac(cx, globalObj);

    backstagePass->SetGlobalObject(globalObj);

    if (!JS_DefineProperty(cx, globalObj, "__XPCShellEnvironment",
                           PRIVATE_TO_JSVAL(this), JS_PropertyStub,
                           JS_StrictPropertyStub,
                           JSPROP_READONLY | JSPROP_PERMANENT) ||
        !JS_DefineFunctions(cx, globalObj, gGlobalFunctions) ||
        !JS_DefineProfilingFunctions(cx, globalObj))
    {
        NS_ERROR("JS_DefineFunctions failed!");
        return false;
    }

    mGlobalHolder = globalObj;

    FILE* runtimeScriptFile = fopen(kDefaultRuntimeScriptFilename, "r");
    if (runtimeScriptFile) {
        fprintf(stdout, "[loading '%s'...]\n", kDefaultRuntimeScriptFilename);
        ProcessFile(cx, globalObj, kDefaultRuntimeScriptFilename,
                    runtimeScriptFile, false);
        fclose(runtimeScriptFile);
    }

    return true;
}

bool
XPCShellEnvironment::EvaluateString(const nsString& aString,
                                    nsString* aResult)
{
  AutoSafeJSContext cx;
  JS::Rooted<JSObject*> global(cx, GetGlobalObject());
  JSAutoCompartment ac(cx, global);

  JS::CompileOptions options(cx);
  options.setFileAndLine("typein", 0);
  JS::Rooted<JSScript*> script(cx, JS_CompileUCScript(cx, global, aString.get(),
                                                      aString.Length(), options));
  if (!script) {
     return false;
  }

  if (aResult) {
      aResult->Truncate();
  }

  JS::Rooted<JS::Value> result(cx);
  bool ok = JS_ExecuteScript(cx, global, script, result.address());
  if (ok && result != JSVAL_VOID) {
      JSErrorReporter old = JS_SetErrorReporter(cx, nullptr);
      JSString* str = JS::ToString(cx, result);
      nsDependentJSString depStr;
      if (str)
          depStr.init(cx, str);
      JS_SetErrorReporter(cx, old);

      if (!depStr.IsEmpty() && aResult) {
          aResult->Assign(depStr);
      }
  }

  return true;
}
