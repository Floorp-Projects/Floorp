/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef HAVE_IO_H
#  include <io.h> /* for isatty() */
#endif
#ifdef HAVE_UNISTD_H
#  include <unistd.h> /* for isatty() */
#endif

#include "jsapi.h"
#include "js/CharacterEncoding.h"
#include "js/CompilationAndEvaluation.h"  // JS::Compile{,Utf8File}
#include "js/PropertyAndElement.h"  // JS_DefineFunctions, JS_DefineProperty, JS_GetProperty
#include "js/PropertySpec.h"
#include "js/RealmOptions.h"
#include "js/SourceText.h"  // JS::Source{Ownership,Text}

#include "xpcpublic.h"

#include "XPCShellEnvironment.h"

#include "mozilla/Utf8.h"  // mozilla::Utf8Unit
#include "mozilla/dom/AutoEntryScript.h"
#include "mozilla/dom/ScriptSettings.h"

#include "nsIPrincipal.h"
#include "nsIScriptSecurityManager.h"
#include "nsIXPConnect.h"
#include "nsServiceManagerUtils.h"

#include "nsJSUtils.h"

#include "BackstagePass.h"

#include "TestShellChild.h"

using mozilla::AutoSafeJSContext;
using mozilla::dom::AutoEntryScript;
using mozilla::dom::AutoJSAPI;
using mozilla::ipc::XPCShellEnvironment;
using namespace JS;

namespace {

static const char kDefaultRuntimeScriptFilename[] = "xpcshell.js";

inline XPCShellEnvironment* Environment(JS::Handle<JSObject*> global) {
  AutoJSAPI jsapi;
  if (!jsapi.Init(global)) {
    return nullptr;
  }
  JSContext* cx = jsapi.cx();
  Rooted<Value> v(cx);
  if (!JS_GetProperty(cx, global, "__XPCShellEnvironment", &v) ||
      !v.get().isDouble()) {
    return nullptr;
  }
  return static_cast<XPCShellEnvironment*>(v.get().toPrivate());
}

static bool Print(JSContext* cx, unsigned argc, JS::Value* vp) {
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);

  for (unsigned i = 0; i < args.length(); i++) {
    JSString* str = JS::ToString(cx, args[i]);
    if (!str) return false;
    JS::UniqueChars bytes = JS_EncodeStringToLatin1(cx, str);
    if (!bytes) return false;
    fprintf(stdout, "%s%s", i ? " " : "", bytes.get());
    fflush(stdout);
  }
  fputc('\n', stdout);
  args.rval().setUndefined();
  return true;
}

static bool GetLine(char* bufp, FILE* file, const char* prompt) {
  char line[256];
  fputs(prompt, stdout);
  fflush(stdout);
  if (!fgets(line, sizeof line, file)) return false;
  strcpy(bufp, line);
  return true;
}

static bool Dump(JSContext* cx, unsigned argc, JS::Value* vp) {
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);

  if (!args.length()) return true;

  JSString* str = JS::ToString(cx, args[0]);
  if (!str) return false;
  JS::UniqueChars bytes = JS_EncodeStringToLatin1(cx, str);
  if (!bytes) return false;

  fputs(bytes.get(), stdout);
  fflush(stdout);
  return true;
}

static bool Load(JSContext* cx, unsigned argc, JS::Value* vp) {
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);

  JS::RootedObject thisObject(cx);
  if (!args.computeThis(cx, &thisObject)) return false;
  if (!JS_IsGlobalObject(thisObject)) {
    JS_ReportErrorASCII(cx, "Trying to load() into a non-global object");
    return false;
  }

  for (unsigned i = 0; i < args.length(); i++) {
    JS::Rooted<JSString*> str(cx, JS::ToString(cx, args[i]));
    if (!str) return false;
    JS::UniqueChars filename = JS_EncodeStringToLatin1(cx, str);
    if (!filename) return false;
    FILE* file = fopen(filename.get(), "r");
    if (!file) {
      filename = JS_EncodeStringToUTF8(cx, str);
      if (!filename) return false;
      JS_ReportErrorUTF8(cx, "cannot open file '%s' for reading",
                         filename.get());
      return false;
    }

    JS::CompileOptions options(cx);
    options.setFileAndLine(filename.get(), 1);

    JS::Rooted<JSScript*> script(cx, JS::CompileUtf8File(cx, options, file));
    fclose(file);
    if (!script) return false;

    if (!JS_ExecuteScript(cx, script)) {
      return false;
    }
  }
  args.rval().setUndefined();
  return true;
}

static bool Quit(JSContext* cx, unsigned argc, JS::Value* vp) {
  Rooted<JSObject*> global(cx, JS::CurrentGlobalOrNull(cx));
  XPCShellEnvironment* env = Environment(global);
  env->SetIsQuitting();

  return false;
}

static bool DumpXPC(JSContext* cx, unsigned argc, JS::Value* vp) {
  JS::CallArgs args = CallArgsFromVp(argc, vp);

  uint16_t depth = 2;
  if (args.length() > 0) {
    if (!JS::ToUint16(cx, args[0], &depth)) return false;
  }

  nsCOMPtr<nsIXPConnect> xpc = nsIXPConnect::XPConnect();
  if (xpc) xpc->DebugDump(int16_t(depth));
  args.rval().setUndefined();
  return true;
}

static bool GC(JSContext* cx, unsigned argc, JS::Value* vp) {
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);

  JS_GC(cx);

  args.rval().setUndefined();
  return true;
}

#ifdef JS_GC_ZEAL
static bool GCZeal(JSContext* cx, unsigned argc, JS::Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  uint32_t zeal;
  if (!ToUint32(cx, args.get(0), &zeal)) return false;

  JS_SetGCZeal(cx, uint8_t(zeal), JS_DEFAULT_ZEAL_FREQ);
  return true;
}
#endif

const JSFunctionSpec gGlobalFunctions[] = {JS_FN("print", Print, 0, 0),
                                           JS_FN("load", Load, 1, 0),
                                           JS_FN("quit", Quit, 0, 0),
                                           JS_FN("dumpXPC", DumpXPC, 1, 0),
                                           JS_FN("dump", Dump, 1, 0),
                                           JS_FN("gc", GC, 0, 0),
#ifdef JS_GC_ZEAL
                                           JS_FN("gczeal", GCZeal, 1, 0),
#endif
                                           JS_FS_END};

typedef enum JSShellErrNum {
#define MSG_DEF(name, number, count, exception, format) name = number,
#include "jsshell.msg"
#undef MSG_DEF
  JSShellErr_Limit
#undef MSGDEF
} JSShellErrNum;

} /* anonymous namespace */

void XPCShellEnvironment::ProcessFile(JSContext* cx, const char* filename,
                                      FILE* file, bool forceTTY) {
  XPCShellEnvironment* env = this;

  JS::Rooted<JS::Value> result(cx);
  int lineno, startline;
  bool ok, hitEOF;
  char *bufp, buffer[4096];
  JSString* str;

  JS::Rooted<JSObject*> global(cx, JS::CurrentGlobalOrNull(cx));
  MOZ_ASSERT(global);

  if (forceTTY) {
    file = stdin;
  } else if (!isatty(fileno(file))) {
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
      while ((ch = fgetc(file)) != EOF) {
        if (ch == '\n' || ch == '\r') break;
      }
    }
    ungetc(ch, file);

    JS::CompileOptions options(cx);
    options.setFileAndLine(filename, 1);

    JS::Rooted<JSScript*> script(cx, JS::CompileUtf8File(cx, options, file));
    if (script) {
      (void)JS_ExecuteScript(cx, script, &result);
    }

    return;
  }

  /* It's an interactive filehandle; drop into read-eval-print loop. */
  lineno = 1;
  hitEOF = false;
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
      if (!GetLine(bufp, file, startline == lineno ? "js> " : "")) {
        hitEOF = true;
        break;
      }
      bufp += strlen(bufp);
      lineno++;
    } while (
        !JS_Utf8BufferIsCompilableUnit(cx, global, buffer, strlen(buffer)));

    /* Clear any pending exception from previous failed compiles.  */
    JS_ClearPendingException(cx);

    JS::CompileOptions options(cx);
    options.setFileAndLine("typein", startline);

    JS::SourceText<mozilla::Utf8Unit> srcBuf;
    JS::Rooted<JSScript*> script(cx);

    if (srcBuf.init(cx, buffer, strlen(buffer),
                    JS::SourceOwnership::Borrowed) &&
        (script = JS::Compile(cx, options, srcBuf))) {
      ok = JS_ExecuteScript(cx, script, &result);
      if (ok && !result.isUndefined()) {
        /* Suppress warnings from JS::ToString(). */
        JS::AutoSuppressWarningReporter suppressWarnings(cx);
        str = JS::ToString(cx, result);
        JS::UniqueChars bytes;
        if (str) bytes = JS_EncodeStringToLatin1(cx, str);

        if (!!bytes)
          fprintf(stdout, "%s\n", bytes.get());
        else
          ok = false;
      }
    }
  } while (!hitEOF && !env->IsQuitting());

  fprintf(stdout, "\n");
}

// static
XPCShellEnvironment* XPCShellEnvironment::CreateEnvironment() {
  auto* env = new XPCShellEnvironment();
  if (env && !env->Init()) {
    delete env;
    env = nullptr;
  }
  return env;
}

XPCShellEnvironment::XPCShellEnvironment() : mQuitting(false) {}

XPCShellEnvironment::~XPCShellEnvironment() {
  if (GetGlobalObject()) {
    AutoJSAPI jsapi;
    if (!jsapi.Init(GetGlobalObject())) {
      return;
    }
    JS_SetAllNonReservedSlotsToUndefined(mGlobalHolder);
    mGlobalHolder.reset();

    JS_GC(jsapi.cx());
  }
}

bool XPCShellEnvironment::Init() {
  nsresult rv;

  // unbuffer stdout so that output is in the correct order; note that stderr
  // is unbuffered by default
  setbuf(stdout, 0);

  AutoSafeJSContext cx;

  mGlobalHolder.init(cx);

  nsCOMPtr<nsIPrincipal> principal;
  nsCOMPtr<nsIScriptSecurityManager> securityManager =
      do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);
  if (NS_SUCCEEDED(rv) && securityManager) {
    rv = securityManager->GetSystemPrincipal(getter_AddRefs(principal));
    if (NS_FAILED(rv)) {
      fprintf(stderr,
              "+++ Failed to obtain SystemPrincipal from ScriptSecurityManager "
              "service.\n");
    }
  } else {
    fprintf(stderr,
            "+++ Failed to get ScriptSecurityManager service, running without "
            "principals");
  }

  auto backstagePass = MakeRefPtr<BackstagePass>();

  JS::RealmOptions options;
  options.creationOptions().setNewCompartmentInSystemZone();
  xpc::SetPrefableRealmOptions(options);

  JS::Rooted<JSObject*> globalObj(cx);
  rv = xpc::InitClassesWithNewWrappedGlobal(
      cx, static_cast<nsIGlobalObject*>(backstagePass), principal, 0, options,
      &globalObj);
  if (NS_FAILED(rv)) {
    NS_ERROR("InitClassesWithNewWrappedGlobal failed!");
    return false;
  }

  if (!globalObj) {
    NS_ERROR("Failed to get global JSObject!");
    return false;
  }
  JSAutoRealm ar(cx, globalObj);

  backstagePass->SetGlobalObject(globalObj);

  JS::Rooted<Value> privateVal(cx, PrivateValue(this));
  if (!JS_DefineProperty(cx, globalObj, "__XPCShellEnvironment", privateVal,
                         JSPROP_READONLY | JSPROP_PERMANENT) ||
      !JS_DefineFunctions(cx, globalObj, gGlobalFunctions)) {
    NS_ERROR("JS_DefineFunctions failed!");
    return false;
  }

  mGlobalHolder = globalObj;

  FILE* runtimeScriptFile = fopen(kDefaultRuntimeScriptFilename, "r");
  if (runtimeScriptFile) {
    fprintf(stdout, "[loading '%s'...]\n", kDefaultRuntimeScriptFilename);
    ProcessFile(cx, kDefaultRuntimeScriptFilename, runtimeScriptFile, false);
    fclose(runtimeScriptFile);
  }

  return true;
}

bool XPCShellEnvironment::EvaluateString(const nsString& aString,
                                         nsString* aResult) {
  AutoEntryScript aes(GetGlobalObject(),
                      "ipc XPCShellEnvironment::EvaluateString");
  JSContext* cx = aes.cx();

  JS::CompileOptions options(cx);
  options.setFileAndLine("typein", 0);

  JS::SourceText<char16_t> srcBuf;
  if (!srcBuf.init(cx, aString.get(), aString.Length(),
                   JS::SourceOwnership::Borrowed)) {
    return false;
  }

  JS::Rooted<JSScript*> script(cx, JS::Compile(cx, options, srcBuf));
  if (!script) {
    return false;
  }

  if (aResult) {
    aResult->Truncate();
  }

  JS::Rooted<JS::Value> result(cx);
  bool ok = JS_ExecuteScript(cx, script, &result);
  if (ok && !result.isUndefined()) {
    /* Suppress warnings from JS::ToString(). */
    JS::AutoSuppressWarningReporter suppressWarnings(cx);
    JSString* str = JS::ToString(cx, result);
    nsAutoJSString autoStr;
    if (str) autoStr.init(cx, str);

    if (!autoStr.IsEmpty() && aResult) {
      aResult->Assign(autoStr);
    }
  }

  return true;
}
