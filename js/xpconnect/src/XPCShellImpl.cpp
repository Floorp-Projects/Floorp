/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsXULAppAPI.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "jsprf.h"
#include "js/OldDebugAPI.h"
#include "nsServiceManagerUtils.h"
#include "nsComponentManagerUtils.h"
#include "nsIXPConnect.h"
#include "nsIJSNativeInitializer.h"
#include "nsIServiceManager.h"
#include "nsIFile.h"
#include "nsString.h"
#include "nsIDirectoryService.h"
#include "nsDirectoryServiceDefs.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nscore.h"
#include "nsArrayEnumerator.h"
#include "nsCOMArray.h"
#include "nsDirectoryServiceUtils.h"
#include "nsIJSRuntimeService.h"
#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "nsJSPrincipals.h"
#include "xpcpublic.h"
#include "BackstagePass.h"
#include "nsCxPusher.h"
#include "nsIScriptSecurityManager.h"
#include "nsIPrincipal.h"

#ifdef ANDROID
#include <android/log.h>
#endif

#ifdef XP_WIN
#include <windows.h>
#endif

// all this crap is needed to do the interactive shell stuff
#include <stdlib.h>
#include <errno.h>
#ifdef HAVE_IO_H
#include <io.h>     /* for isatty() */
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>     /* for isatty() */
#endif

#ifdef MOZ_CRASHREPORTER
#include "nsExceptionHandler.h"
#include "nsICrashReporter.h"
#endif

using namespace mozilla;
using namespace JS;

class XPCShellDirProvider : public nsIDirectoryServiceProvider2
{
public:
    NS_DECL_ISUPPORTS_INHERITED
    NS_DECL_NSIDIRECTORYSERVICEPROVIDER
    NS_DECL_NSIDIRECTORYSERVICEPROVIDER2

    XPCShellDirProvider() { }
    ~XPCShellDirProvider() { }

    // The platform resource folder
    void SetGREDir(nsIFile *greDir);
    void ClearGREDir() { mGREDir = nullptr; }
    // The application resource folder
    void SetAppDir(nsIFile *appFile);
    void ClearAppDir() { mAppDir = nullptr; }
    // The app executable
    void SetAppFile(nsIFile *appFile);
    void ClearAppFile() { mAppFile = nullptr; }
    // An additional custom plugin dir if specified
    void SetPluginDir(nsIFile* pluginDir);
    void ClearPluginDir() { mPluginDir = nullptr; }

private:
    nsCOMPtr<nsIFile> mGREDir;
    nsCOMPtr<nsIFile> mAppDir;
    nsCOMPtr<nsIFile> mPluginDir;
    nsCOMPtr<nsIFile> mAppFile;
};

#ifdef JS_THREADSAFE
#define DoBeginRequest(cx) JS_BeginRequest((cx))
#define DoEndRequest(cx)   JS_EndRequest((cx))
#else
#define DoBeginRequest(cx) ((void)0)
#define DoEndRequest(cx)   ((void)0)
#endif

static const char kXPConnectServiceContractID[] = "@mozilla.org/js/xpc/XPConnect;1";

#define EXITCODE_RUNTIME_ERROR 3
#define EXITCODE_FILE_NOT_FOUND 4

static FILE *gOutFile = nullptr;
static FILE *gErrFile = nullptr;
static FILE *gInFile = nullptr;

static int gExitCode = 0;
static bool gIgnoreReportedErrors = false;
static bool gQuitting = false;
static bool reportWarnings = true;
static bool compileOnly = false;

static JSPrincipals *gJSPrincipals = nullptr;
static nsAutoString *gWorkingDirectory = nullptr;

static bool
GetLocationProperty(JSContext *cx, HandleObject obj, HandleId id, MutableHandleValue vp)
{
#if !defined(XP_WIN) && !defined(XP_UNIX)
    //XXX: your platform should really implement this
    return false;
#else
    JS::AutoFilename filename;
    if (JS::DescribeScriptedCaller(cx, &filename) && filename.get()) {
        nsresult rv;
        nsCOMPtr<nsIXPConnect> xpc =
            do_GetService(kXPConnectServiceContractID, &rv);

#if defined(XP_WIN)
        // convert from the system codepage to UTF-16
        int bufferSize = MultiByteToWideChar(CP_ACP, 0, filename.get(),
                                             -1, nullptr, 0);
        nsAutoString filenameString;
        filenameString.SetLength(bufferSize);
        MultiByteToWideChar(CP_ACP, 0, filename.get(),
                            -1, (LPWSTR)filenameString.BeginWriting(),
                            filenameString.Length());
        // remove the null terminator
        filenameString.SetLength(bufferSize - 1);

        // replace forward slashes with backslashes,
        // since nsLocalFileWin chokes on them
        char16_t* start = filenameString.BeginWriting();
        char16_t* end = filenameString.EndWriting();

        while (start != end) {
            if (*start == L'/')
                *start = L'\\';
            start++;
        }
#elif defined(XP_UNIX)
        NS_ConvertUTF8toUTF16 filenameString(filename.get());
#endif

        nsCOMPtr<nsIFile> location;
        if (NS_SUCCEEDED(rv)) {
            rv = NS_NewLocalFile(filenameString,
                                 false, getter_AddRefs(location));
        }

        if (!location && gWorkingDirectory) {
            // could be a relative path, try appending it to the cwd
            // and then normalize
            nsAutoString absolutePath(*gWorkingDirectory);
            absolutePath.Append(filenameString);

            rv = NS_NewLocalFile(absolutePath,
                                 false, getter_AddRefs(location));
        }

        if (location) {
            nsCOMPtr<nsIXPConnectJSObjectHolder> locationHolder;

            bool symlink;
            // don't normalize symlinks, because that's kind of confusing
            if (NS_SUCCEEDED(location->IsSymlink(&symlink)) &&
                !symlink)
                location->Normalize();
            rv = xpc->WrapNative(cx, obj, location,
                                 NS_GET_IID(nsIFile),
                                 getter_AddRefs(locationHolder));

            if (NS_SUCCEEDED(rv) &&
                locationHolder->GetJSObject()) {
                vp.set(OBJECT_TO_JSVAL(locationHolder->GetJSObject()));
            }
        }
    }

    return true;
#endif
}

static bool
GetLine(JSContext *cx, char *bufp, FILE *file, const char *prompt) {
    {
        char line[4096] = { '\0' };
        fputs(prompt, gOutFile);
        fflush(gOutFile);
        if ((!fgets(line, sizeof line, file) && errno != EINTR) || feof(file))
            return false;
        strcpy(bufp, line);
    }
    return true;
}

static bool
ReadLine(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    // While 4096 might be quite arbitrary, this is something to be fixed in
    // bug 105707. It is also the same limit as in ProcessFile.
    char buf[4096];
    RootedString str(cx);

    /* If a prompt was specified, construct the string */
    if (args.length() > 0) {
        str = JS::ToString(cx, args[0]);
        if (!str)
            return false;
    } else {
        str = JS_GetEmptyString(JS_GetRuntime(cx));
    }

    /* Get a line from the infile */
    JSAutoByteString strBytes(cx, str);
    if (!strBytes || !GetLine(cx, buf, gInFile, strBytes.ptr()))
        return false;

    /* Strip newline character added by GetLine() */
    unsigned int buflen = strlen(buf);
    if (buflen == 0) {
        if (feof(gInFile)) {
            args.rval().setNull();
            return true;
        }
    } else if (buf[buflen - 1] == '\n') {
        --buflen;
    }

    /* Turn buf into a JSString */
    str = JS_NewStringCopyN(cx, buf, buflen);
    if (!str)
        return false;

    args.rval().setString(str);
    return true;
}

static bool
Print(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    args.rval().setUndefined();

    RootedString str(cx);
    nsAutoCString utf8str;
    size_t length;
    const jschar *chars;

    for (unsigned i = 0; i < args.length(); i++) {
        str = ToString(cx, args[i]);
        if (!str)
            return false;
        chars = JS_GetStringCharsAndLength(cx, str, &length);
        if (!chars)
            return false;

        if (i)
            utf8str.Append(' ');
        AppendUTF16toUTF8(Substring(reinterpret_cast<const char16_t*>(chars),
                                    length),
                          utf8str);
    }
    utf8str.Append('\n');
    fputs(utf8str.get(), gOutFile);
    fflush(gOutFile);
    return true;
}

static bool
Dump(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    args.rval().setUndefined();

    if (!args.length())
         return true;

    RootedString str(cx, ToString(cx, args[0]));
    if (!str)
        return false;

    size_t length;
    const jschar *chars = JS_GetStringCharsAndLength(cx, str, &length);
    if (!chars)
        return false;

    NS_ConvertUTF16toUTF8 utf8str(reinterpret_cast<const char16_t*>(chars),
                                  length);
#ifdef ANDROID
    __android_log_print(ANDROID_LOG_INFO, "Gecko", "%s", utf8str.get());
#endif
#ifdef XP_WIN
    if (IsDebuggerPresent()) {
      OutputDebugStringW(reinterpret_cast<const wchar_t*>(chars));
    }
#endif
    fputs(utf8str.get(), gOutFile);
    fflush(gOutFile);
    return true;
}

static bool
Load(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    JS::Rooted<JSObject*> obj(cx, JS_THIS_OBJECT(cx, vp));
    if (!obj)
        return false;

    RootedString str(cx);
    for (unsigned i = 0; i < args.length(); i++) {
        str = ToString(cx, args[i]);
        if (!str)
            return false;
        JSAutoByteString filename(cx, str);
        if (!filename)
            return false;
        FILE *file = fopen(filename.ptr(), "r");
        if (!file) {
            JS_ReportError(cx, "cannot open file '%s' for reading",
                           filename.ptr());
            return false;
        }
        JS::CompileOptions options(cx);
        options.setUTF8(true)
               .setFileAndLine(filename.ptr(), 1);
        JS::Rooted<JSScript*> script(cx, JS::Compile(cx, obj, options, file));
        fclose(file);
        if (!script)
            return false;

        if (!compileOnly && !JS_ExecuteScript(cx, obj, script))
            return false;
    }
    args.rval().setUndefined();
    return true;
}

static bool
Version(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    args.rval().setInt32(JS_GetVersion(cx));
    if (args.get(0).isInt32())
        JS_SetVersionForCompartment(js::GetContextCompartment(cx),
                                    JSVersion(args[0].toInt32()));
    return true;
}

static bool
BuildDate(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    fprintf(gOutFile, "built on %s at %s\n", __DATE__, __TIME__);
    args.rval().setUndefined();
    return true;
}

static bool
Quit(JSContext *cx, unsigned argc, jsval *vp)
{
    gExitCode = 0;
    JS_ConvertArguments(cx, JS::CallArgsFromVp(argc, vp),"/ i", &gExitCode);

    gQuitting = true;
//    exit(0);
    return false;
}

// Provide script a way to disable the xpcshell error reporter, preventing
// reported errors from being logged to the console and also from affecting the
// exit code returned by the xpcshell binary.
static bool
IgnoreReportedErrors(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    if (args.length() != 1 || !args[0].isBoolean()) {
        JS_ReportError(cx, "Bad arguments");
        return false;
    }
    gIgnoreReportedErrors = args[0].toBoolean();
    return true;
}

static bool
DumpXPC(JSContext *cx, unsigned argc, jsval *vp)
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
GC(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
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
GCZeal(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    uint32_t zeal;
    if (!ToUint32(cx, args.get(0), &zeal))
        return false;

    JS_SetGCZeal(cx, uint8_t(zeal), JS_DEFAULT_ZEAL_FREQ);
    args.rval().setUndefined();
    return true;
}
#endif

static bool
SendCommand(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (args.length() == 0) {
        JS_ReportError(cx, "Function takes at least one argument!");
        return false;
    }

    JSString* str = ToString(cx, args[0]);
    if (!str) {
        JS_ReportError(cx, "Could not convert argument 1 to string!");
        return false;
    }

    if (args.length() > 1 && JS_TypeOfValue(cx, args[1]) != JSTYPE_FUNCTION) {
        JS_ReportError(cx, "Could not convert argument 2 to function!");
        return false;
    }

    if (!XRE_SendTestShellCommand(cx, str, args.length() > 1 ? args[1].address() : nullptr)) {
        JS_ReportError(cx, "Couldn't send command!");
        return false;
    }

    args.rval().setUndefined();
    return true;
}

static bool
Options(JSContext *cx, unsigned argc, jsval *vp)
{
    JS::CallArgs args = CallArgsFromVp(argc, vp);
    ContextOptions oldOptions = ContextOptionsRef(cx);

    for (unsigned i = 0; i < args.length(); ++i) {
        JSString *str = ToString(cx, args[i]);
        if (!str)
            return false;

        JSAutoByteString opt(cx, str);
        if (!opt)
            return false;

        if (strcmp(opt.ptr(), "strict") == 0)
            ContextOptionsRef(cx).toggleExtraWarnings();
        else if (strcmp(opt.ptr(), "werror") == 0)
            ContextOptionsRef(cx).toggleWerror();
        else if (strcmp(opt.ptr(), "strict_mode") == 0)
            ContextOptionsRef(cx).toggleStrictMode();
        else {
            JS_ReportError(cx, "unknown option name '%s'. The valid names are "
                           "strict, werror, and strict_mode.", opt.ptr());
            return false;
        }
    }

    char *names = nullptr;
    if (oldOptions.extraWarnings()) {
        names = JS_sprintf_append(names, "%s", "strict");
        if (!names) {
            JS_ReportOutOfMemory(cx);
            return false;
        }
    }
    if (oldOptions.werror()) {
        names = JS_sprintf_append(names, "%s%s", names ? "," : "", "werror");
        if (!names) {
            JS_ReportOutOfMemory(cx);
            return false;
        }
    }
    if (names && oldOptions.strictMode()) {
        names = JS_sprintf_append(names, "%s%s", names ? "," : "", "strict_mode");
        if (!names) {
            JS_ReportOutOfMemory(cx);
            return false;
        }
    }

    JSString *str = JS_NewStringCopyZ(cx, names);
    free(names);
    if (!str)
        return false;

    args.rval().setString(str);
    return true;
}

static bool
Parent(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    if (args.length() != 1) {
        JS_ReportError(cx, "Wrong number of arguments");
        return false;
    }

    Value v = args[0];
    if (v.isPrimitive()) {
        JS_ReportError(cx, "Only objects have parents!");
        return false;
    }

    args.rval().setObjectOrNull(JS_GetParent(&v.toObject()));
    return true;
}

static bool
Atob(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    if (!args.length())
        return true;

    return xpc::Base64Decode(cx, args[0], args.rval());
}

static bool
Btoa(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    if (!args.length())
        return true;

  return xpc::Base64Encode(cx, args[0], args.rval());
}

static bool
Blob(JSContext *cx, unsigned argc, Value *vp)
{
  JS::CallArgs args = CallArgsFromVp(argc, vp);

  nsCOMPtr<nsISupports> native =
    do_CreateInstance("@mozilla.org/dom/multipart-blob;1");
  if (!native) {
    JS_ReportError(cx, "Could not create native object!");
    return false;
  }

  nsCOMPtr<nsIJSNativeInitializer> initializer = do_QueryInterface(native);
  MOZ_ASSERT(initializer);

  nsresult rv = initializer->Initialize(nullptr, cx, nullptr, args);
  if (NS_FAILED(rv)) {
    JS_ReportError(cx, "Could not initialize native object!");
    return false;
  }

  nsCOMPtr<nsIXPConnect> xpc = do_GetService(kXPConnectServiceContractID, &rv);
  if (NS_FAILED(rv)) {
    JS_ReportError(cx, "Could not get XPConnent service!");
    return false;
  }

  JSObject *global = JS::CurrentGlobalOrNull(cx);
  rv = xpc->WrapNativeToJSVal(cx, global, native, nullptr,
                              &NS_GET_IID(nsISupports), true,
                              args.rval());
  if (NS_FAILED(rv)) {
    JS_ReportError(cx, "Could not wrap native object!");
    return false;
  }

  return true;
}

static bool
File(JSContext *cx, unsigned argc, Value *vp)
{
  JS::CallArgs args = CallArgsFromVp(argc, vp);

  nsCOMPtr<nsISupports> native =
    do_CreateInstance("@mozilla.org/dom/multipart-file;1");
  if (!native) {
    JS_ReportError(cx, "Could not create native object!");
    return false;
  }

  nsCOMPtr<nsIJSNativeInitializer> initializer = do_QueryInterface(native);
  MOZ_ASSERT(initializer);

  nsresult rv = initializer->Initialize(nullptr, cx, nullptr, args);
  if (NS_FAILED(rv)) {
    JS_ReportError(cx, "Could not initialize native object!");
    return false;
  }

  nsCOMPtr<nsIXPConnect> xpc = do_GetService(kXPConnectServiceContractID, &rv);
  if (NS_FAILED(rv)) {
    JS_ReportError(cx, "Could not get XPConnent service!");
    return false;
  }

  JSObject *global = JS::CurrentGlobalOrNull(cx);
  rv = xpc->WrapNativeToJSVal(cx, global, native, nullptr,
                              &NS_GET_IID(nsISupports), true,
                              args.rval());
  if (NS_FAILED(rv)) {
    JS_ReportError(cx, "Could not wrap native object!");
    return false;
  }

  return true;
}

static Maybe<PersistentRootedValue> sScriptedInterruptCallback;

static bool
XPCShellInterruptCallback(JSContext *cx)
{
    MOZ_ASSERT(!sScriptedInterruptCallback.empty());
    RootedValue callback(cx, sScriptedInterruptCallback.ref());

    // If no interrupt callback was set by script, no-op.
    if (callback.isUndefined())
        return true;

    JSAutoCompartment ac(cx, &callback.toObject());
    RootedValue rv(cx);
    if (!JS_CallFunctionValue(cx, JS::NullPtr(), callback, JS::HandleValueArray::empty(), &rv) ||
        !rv.isBoolean())
    {
        NS_WARNING("Scripted interrupt callback failed! Terminating script.");
        JS_ClearPendingException(cx);
        return false;
    }

    return rv.toBoolean();
}

static bool
SetInterruptCallback(JSContext *cx, unsigned argc, jsval *vp)
{
    MOZ_ASSERT(!sScriptedInterruptCallback.empty());

    // Sanity-check args.
    JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
    if (args.length() != 1) {
        JS_ReportError(cx, "Wrong number of arguments");
        return false;
    }

    // Allow callers to remove the interrupt callback by passing undefined.
    if (args[0].isUndefined()) {
        sScriptedInterruptCallback.ref() = UndefinedValue();
        return true;
    }

    // Otherwise, we should have a callable object.
    if (!args[0].isObject() || !JS_ObjectIsCallable(cx, &args[0].toObject())) {
        JS_ReportError(cx, "Argument must be callable");
        return false;
    }

    sScriptedInterruptCallback.ref() = args[0];

    return true;
}

static bool
SimulateActivityCallback(JSContext *cx, unsigned argc, jsval *vp)
{
    // Sanity-check args.
    JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
    if (args.length() != 1 || !args[0].isBoolean()) {
        JS_ReportError(cx, "Wrong number of arguments");
        return false;
    }
    xpc::SimulateActivityCallback(args[0].toBoolean());
    return true;
}

static const JSFunctionSpec glob_functions[] = {
    JS_FS("print",           Print,          0,0),
    JS_FS("readline",        ReadLine,       1,0),
    JS_FS("load",            Load,           1,0),
    JS_FS("quit",            Quit,           0,0),
    JS_FS("ignoreReportedErrors", IgnoreReportedErrors, 1,0),
    JS_FS("version",         Version,        1,0),
    JS_FS("build",           BuildDate,      0,0),
    JS_FS("dumpXPC",         DumpXPC,        1,0),
    JS_FS("dump",            Dump,           1,0),
    JS_FS("gc",              GC,             0,0),
#ifdef JS_GC_ZEAL
    JS_FS("gczeal",          GCZeal,         1,0),
#endif
    JS_FS("options",         Options,        0,0),
    JS_FN("parent",          Parent,         1,0),
    JS_FS("sendCommand",     SendCommand,    1,0),
    JS_FS("atob",            Atob,           1,0),
    JS_FS("btoa",            Btoa,           1,0),
    JS_FS("Blob",            Blob,           2,JSFUN_CONSTRUCTOR),
    JS_FS("File",            File,           2,JSFUN_CONSTRUCTOR),
    JS_FS("setInterruptCallback", SetInterruptCallback, 1,0),
    JS_FS("simulateActivityCallback", SimulateActivityCallback, 1,0),
    JS_FS_END
};

static bool
env_setProperty(JSContext *cx, HandleObject obj, HandleId id, bool strict, MutableHandleValue vp)
{
/* XXX porting may be easy, but these don't seem to supply setenv by default */
#if !defined SOLARIS
    JSString *valstr;
    JS::Rooted<JSString*> idstr(cx);
    int rv;

    RootedValue idval(cx);
    if (!JS_IdToValue(cx, id, &idval))
        return false;

    idstr = ToString(cx, idval);
    valstr = ToString(cx, vp);
    if (!idstr || !valstr)
        return false;
    JSAutoByteString name(cx, idstr);
    if (!name)
        return false;
    JSAutoByteString value(cx, valstr);
    if (!value)
        return false;
#if defined XP_WIN || defined HPUX || defined OSF1 || defined SCO
    {
        char *waste = JS_smprintf("%s=%s", name.ptr(), value.ptr());
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
        free(waste);
#endif
    }
#else
    rv = setenv(name.ptr(), value.ptr(), 1);
#endif
    if (rv < 0) {
        JS_ReportError(cx, "can't set envariable %s to %s", name.ptr(), value.ptr());
        return false;
    }
    vp.set(STRING_TO_JSVAL(valstr));
#endif /* !defined SOLARIS */
    return true;
}

static bool
env_enumerate(JSContext *cx, HandleObject obj)
{
    static bool reflected;
    char **evp, *name, *value;
    RootedString valstr(cx);
    bool ok;

    if (reflected)
        return true;

    for (evp = (char **)JS_GetPrivate(obj); (name = *evp) != nullptr; evp++) {
        value = strchr(name, '=');
        if (!value)
            continue;
        *value++ = '\0';
        valstr = JS_NewStringCopyZ(cx, value);
        ok = valstr ? JS_DefineProperty(cx, obj, name, valstr, JSPROP_ENUMERATE) : false;
        value[-1] = '=';
        if (!ok)
            return false;
    }

    reflected = true;
    return true;
}

static bool
env_resolve(JSContext *cx, HandleObject obj, HandleId id,
            JS::MutableHandleObject objp)
{
    JSString *idstr;

    RootedValue idval(cx);
    if (!JS_IdToValue(cx, id, &idval))
        return false;

    idstr = ToString(cx, idval);
    if (!idstr)
        return false;
    JSAutoByteString name(cx, idstr);
    if (!name)
        return false;
    const char *value = getenv(name.ptr());
    if (value) {
        RootedString valstr(cx, JS_NewStringCopyZ(cx, value));
        if (!valstr)
            return false;
        if (!JS_DefinePropertyById(cx, obj, id, valstr, JSPROP_ENUMERATE)) {
            return false;
        }
        objp.set(obj);
    }
    return true;
}

static const JSClass env_class = {
    "environment", JSCLASS_HAS_PRIVATE | JSCLASS_NEW_RESOLVE,
    JS_PropertyStub,  JS_DeletePropertyStub,
    JS_PropertyStub,  env_setProperty,
    env_enumerate, (JSResolveOp) env_resolve,
    JS_ConvertStub,   nullptr
};

/***************************************************************************/

typedef enum JSShellErrNum {
#define MSG_DEF(name, number, count, exception, format) \
    name = number,
#include "jsshell.msg"
#undef MSG_DEF
    JSShellErr_Limit
} JSShellErrNum;

static const JSErrorFormatString jsShell_ErrorFormatString[JSShellErr_Limit] = {
#define MSG_DEF(name, number, count, exception, format) \
    { format, count } ,
#include "jsshell.msg"
#undef MSG_DEF
};

static const JSErrorFormatString *
my_GetErrorMessage(void *userRef, const char *locale, const unsigned errorNumber)
{
    if (errorNumber == 0 || errorNumber >= JSShellErr_Limit)
        return nullptr;

    return &jsShell_ErrorFormatString[errorNumber];
}

static void
ProcessFile(JSContext *cx, JS::Handle<JSObject*> obj, const char *filename, FILE *file,
            bool forceTTY)
{
    JS::RootedScript script(cx);
    JS::RootedValue result(cx);
    int lineno, startline;
    bool ok, hitEOF;
    char *bufp, buffer[4096];
    JSString *str;

    if (forceTTY) {
        file = stdin;
    } else if (!isatty(fileno(file)))
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
            while ((ch = fgetc(file)) != EOF) {
                if (ch == '\n' || ch == '\r')
                    break;
            }
        }
        ungetc(ch, file);
        DoBeginRequest(cx);

        JS::CompileOptions options(cx);
        options.setUTF8(true)
               .setFileAndLine(filename, 1);
        script = JS::Compile(cx, obj, options, file);
        if (script && !compileOnly)
            (void)JS_ExecuteScript(cx, obj, script, &result);
        DoEndRequest(cx);

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
            if (!GetLine(cx, bufp, file, startline == lineno ? "js> " : "")) {
                hitEOF = true;
                break;
            }
            bufp += strlen(bufp);
            lineno++;
        } while (!JS_BufferIsCompilableUnit(cx, obj, buffer, strlen(buffer)));

        DoBeginRequest(cx);
        /* Clear any pending exception from previous failed compiles.  */
        JS_ClearPendingException(cx);
        JS::CompileOptions options(cx);
        options.setFileAndLine("typein", startline);
        script = JS_CompileScript(cx, obj, buffer, strlen(buffer), options);
        if (script) {
            JSErrorReporter older;

            if (!compileOnly) {
                ok = JS_ExecuteScript(cx, obj, script, &result);
                if (ok && result != JSVAL_VOID) {
                    /* Suppress error reports from JS::ToString(). */
                    older = JS_SetErrorReporter(cx, nullptr);
                    str = ToString(cx, result);
                    JS_SetErrorReporter(cx, older);
                    JSAutoByteString bytes;
                    if (str && bytes.encodeLatin1(cx, str))
                        fprintf(gOutFile, "%s\n", bytes.ptr());
                    else
                        ok = false;
                }
            }
        }
        DoEndRequest(cx);
    } while (!hitEOF && !gQuitting);

    fprintf(gOutFile, "\n");
}

static void
Process(JSContext *cx, JS::Handle<JSObject*> obj, const char *filename, bool forceTTY)
{
    FILE *file;

    if (forceTTY || !filename || strcmp(filename, "-") == 0) {
        file = stdin;
    } else {
        file = fopen(filename, "r");
        if (!file) {
            JS_ReportErrorNumber(cx, my_GetErrorMessage, nullptr,
                                 JSSMSG_CANT_OPEN,
                                 filename, strerror(errno));
            gExitCode = EXITCODE_FILE_NOT_FOUND;
            return;
        }
    }

    ProcessFile(cx, obj, filename, file, forceTTY);
    if (file != stdin)
        fclose(file);
}

static int
usage(void)
{
    fprintf(gErrFile, "%s\n", JS_GetImplementationVersion());
    fprintf(gErrFile, "usage: xpcshell [-g gredir] [-a appdir] [-r manifest]... [-PsSwWCijmIn] [-v version] [-f scriptfile] [-e script] [scriptfile] [scriptarg...]\n");
    return 2;
}

static void
ProcessArgsForCompartment(JSContext *cx, char **argv, int argc)
{
    for (int i = 0; i < argc; i++) {
        if (argv[i][0] != '-' || argv[i][1] == '\0')
            break;

        switch (argv[i][1]) {
          case 'v':
          case 'f':
          case 'e':
            if (++i == argc)
                return;
            break;
        case 'S':
            ContextOptionsRef(cx).toggleWerror();
        case 's':
            ContextOptionsRef(cx).toggleExtraWarnings();
            break;
        case 'I':
            RuntimeOptionsRef(cx).toggleIon()
                                 .toggleAsmJS();
            break;
        }
    }
}

static int
ProcessArgs(JSContext *cx, JS::Handle<JSObject*> obj, char **argv, int argc, XPCShellDirProvider* aDirProvider)
{
    const char rcfilename[] = "xpcshell.js";
    FILE *rcfile;
    int i;
    JS::Rooted<JSObject*> argsObj(cx);
    char *filename = nullptr;
    bool isInteractive = true;
    bool forceTTY = false;

    rcfile = fopen(rcfilename, "r");
    if (rcfile) {
        printf("[loading '%s'...]\n", rcfilename);
        ProcessFile(cx, obj, rcfilename, rcfile, false);
        fclose(rcfile);
    }

    /*
     * Scan past all optional arguments so we can create the arguments object
     * before processing any -f options, which must interleave properly with
     * -v and -w options.  This requires two passes, and without getopt, we'll
     * have to keep the option logic here and in the second for loop in sync.
     */
    for (i = 0; i < argc; i++) {
        if (argv[i][0] != '-' || argv[i][1] == '\0') {
            ++i;
            break;
        }
        switch (argv[i][1]) {
          case 'v':
          case 'f':
          case 'e':
            ++i;
            break;
          default:;
        }
    }

    /*
     * Create arguments early and define it to root it, so it's safe from any
     * GC calls nested below, and so it is available to -f <file> arguments.
     */
    argsObj = JS_NewArrayObject(cx, 0);
    if (!argsObj)
        return 1;
    if (!JS_DefineProperty(cx, obj, "arguments", argsObj, 0))
        return 1;

    for (size_t j = 0, length = argc - i; j < length; j++) {
        RootedString str(cx, JS_NewStringCopyZ(cx, argv[i++]));
        if (!str ||
            !JS_DefineElement(cx, argsObj, j, str, JSPROP_ENUMERATE)) {
            return 1;
        }
    }

    for (i = 0; i < argc; i++) {
        if (argv[i][0] != '-' || argv[i][1] == '\0') {
            filename = argv[i++];
            isInteractive = false;
            break;
        }
        switch (argv[i][1]) {
        case 'v':
            if (++i == argc) {
                return usage();
            }
            JS_SetVersionForCompartment(js::GetContextCompartment(cx),
                                        JSVersion(atoi(argv[i])));
            break;
        case 'W':
            reportWarnings = false;
            break;
        case 'w':
            reportWarnings = true;
            break;
        case 'x':
            break;
        case 'd':
            xpc_ActivateDebugMode();
            break;
        case 'f':
            if (++i == argc) {
                return usage();
            }
            Process(cx, obj, argv[i], false);
            /*
             * XXX: js -f foo.js should interpret foo.js and then
             * drop into interactive mode, but that breaks test
             * harness. Just execute foo.js for now.
             */
            isInteractive = false;
            break;
        case 'i':
            isInteractive = forceTTY = true;
            break;
        case 'e':
        {
            RootedValue rval(cx);

            if (++i == argc) {
                return usage();
            }

            JS_EvaluateScript(cx, obj, argv[i], strlen(argv[i]), "-e", 1, &rval);

            isInteractive = false;
            break;
        }
        case 'C':
            compileOnly = true;
            isInteractive = false;
            break;
        case 'S':
        case 's':
        case 'm':
        case 'I':
            // These options are processed in ProcessArgsForCompartment.
            break;
        case 'p':
        {
          // plugins path
          char *pluginPath = argv[++i];
          nsCOMPtr<nsIFile> pluginsDir;
          if (NS_FAILED(XRE_GetFileFromPath(pluginPath, getter_AddRefs(pluginsDir)))) {
              fprintf(gErrFile, "Couldn't use given plugins dir.\n");
              return usage();
          }
          aDirProvider->SetPluginDir(pluginsDir);
          break;
        }
        default:
            return usage();
        }
    }

    if (filename || isInteractive)
        Process(cx, obj, filename, forceTTY);

    return gExitCode;
}

/***************************************************************************/

// #define TEST_InitClassesWithNewWrappedGlobal

#ifdef TEST_InitClassesWithNewWrappedGlobal
// XXX hacky test code...
#include "xpctest.h"

class TestGlobal : public nsIXPCTestNoisy, public nsIXPCScriptable
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIXPCTESTNOISY
    NS_DECL_NSIXPCSCRIPTABLE

    TestGlobal(){}
};

NS_IMPL_ISUPPORTS(TestGlobal, nsIXPCTestNoisy, nsIXPCScriptable)

// The nsIXPCScriptable map declaration that will generate stubs for us...
#define XPC_MAP_CLASSNAME           TestGlobal
#define XPC_MAP_QUOTED_CLASSNAME   "TestGlobal"
#define XPC_MAP_FLAGS               nsIXPCScriptable::USE_JSSTUB_FOR_ADDPROPERTY |\
                                    nsIXPCScriptable::USE_JSSTUB_FOR_DELPROPERTY |\
                                    nsIXPCScriptable::USE_JSSTUB_FOR_SETPROPERTY
#include "xpc_map_end.h" /* This will #undef the above */

NS_IMETHODIMP TestGlobal::Squawk() {return NS_OK;}

#endif

// uncomment to install the test 'this' translator
// #define TEST_TranslateThis

#ifdef TEST_TranslateThis

#include "xpctest.h"

class nsXPCFunctionThisTranslator : public nsIXPCFunctionThisTranslator
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIXPCFUNCTIONTHISTRANSLATOR

  nsXPCFunctionThisTranslator();
  virtual ~nsXPCFunctionThisTranslator();
  /* additional members */
};

/* Implementation file */
NS_IMPL_ISUPPORTS(nsXPCFunctionThisTranslator, nsIXPCFunctionThisTranslator)

nsXPCFunctionThisTranslator::nsXPCFunctionThisTranslator()
{
}

nsXPCFunctionThisTranslator::~nsXPCFunctionThisTranslator()
{
}

/* nsISupports TranslateThis (in nsISupports aInitialThis); */
NS_IMETHODIMP
nsXPCFunctionThisTranslator::TranslateThis(nsISupports *aInitialThis,
                                           nsISupports **_retval)
{
    nsCOMPtr<nsISupports> temp = aInitialThis;
    temp.forget(_retval);
    return NS_OK;
}

#endif

static void
XPCShellErrorReporter(JSContext *cx, const char *message, JSErrorReport *rep)
{
    if (gIgnoreReportedErrors)
        return;

    if (!JSREPORT_IS_WARNING(rep->flags))
        gExitCode = EXITCODE_RUNTIME_ERROR;

    // Delegate to the system error reporter for heavy lifting.
    xpc::SystemErrorReporter(cx, message, rep);
}

static bool
ContextCallback(JSContext *cx, unsigned contextOp)
{
    if (contextOp == JSCONTEXT_NEW)
        JS_SetErrorReporter(cx, XPCShellErrorReporter);
    return true;
}

static bool
GetCurrentWorkingDirectory(nsAString& workingDirectory)
{
#if !defined(XP_WIN) && !defined(XP_UNIX)
    //XXX: your platform should really implement this
    return false;
#elif XP_WIN
    DWORD requiredLength = GetCurrentDirectoryW(0, nullptr);
    workingDirectory.SetLength(requiredLength);
    GetCurrentDirectoryW(workingDirectory.Length(),
                         (LPWSTR)workingDirectory.BeginWriting());
    // we got a trailing null there
    workingDirectory.SetLength(requiredLength);
    workingDirectory.Replace(workingDirectory.Length() - 1, 1, L'\\');
#elif defined(XP_UNIX)
    nsAutoCString cwd;
    // 1024 is just a guess at a sane starting value
    size_t bufsize = 1024;
    char* result = nullptr;
    while (result == nullptr) {
        cwd.SetLength(bufsize);
        result = getcwd(cwd.BeginWriting(), cwd.Length());
        if (!result) {
            if (errno != ERANGE)
                return false;
            // need to make the buffer bigger
            bufsize *= 2;
        }
    }
    // size back down to the actual string length
    cwd.SetLength(strlen(result) + 1);
    cwd.Replace(cwd.Length() - 1, 1, '/');
    workingDirectory = NS_ConvertUTF8toUTF16(cwd);
#endif
    return true;
}

static JSSecurityCallbacks shellSecurityCallbacks;

int
XRE_XPCShellMain(int argc, char **argv, char **envp)
{
    JSRuntime *rt;
    JSContext *cx;
    int result;
    nsresult rv;

    gErrFile = stderr;
    gOutFile = stdout;
    gInFile = stdin;

    NS_LogInit();

    nsCOMPtr<nsIFile> appFile;
    rv = XRE_GetBinaryPath(argv[0], getter_AddRefs(appFile));
    if (NS_FAILED(rv)) {
        printf("Couldn't find application file.\n");
        return 1;
    }
    nsCOMPtr<nsIFile> appDir;
    rv = appFile->GetParent(getter_AddRefs(appDir));
    if (NS_FAILED(rv)) {
        printf("Couldn't get application directory.\n");
        return 1;
    }

    XPCShellDirProvider dirprovider;

    dirprovider.SetAppFile(appFile);

    nsCOMPtr<nsIFile> greDir;
    if (argc > 1 && !strcmp(argv[1], "-g")) {
        if (argc < 3)
            return usage();

        rv = XRE_GetFileFromPath(argv[2], getter_AddRefs(greDir));
        if (NS_FAILED(rv)) {
            printf("Couldn't use given GRE dir.\n");
            return 1;
        }

        dirprovider.SetGREDir(greDir);

        argc -= 2;
        argv += 2;
    } else {
        nsAutoString workingDir;
        if (!GetCurrentWorkingDirectory(workingDir)) {
            printf("GetCurrentWorkingDirectory failed.\n");
            return 1;
        }
        rv = NS_NewLocalFile(workingDir, true, getter_AddRefs(greDir));
        if (NS_FAILED(rv)) {
            printf("NS_NewLocalFile failed.\n");
            return 1;
        }
    }

    if (argc > 1 && !strcmp(argv[1], "-a")) {
        if (argc < 3)
            return usage();

        nsCOMPtr<nsIFile> dir;
        rv = XRE_GetFileFromPath(argv[2], getter_AddRefs(dir));
        if (NS_SUCCEEDED(rv)) {
            appDir = do_QueryInterface(dir, &rv);
            dirprovider.SetAppDir(appDir);
        }
        if (NS_FAILED(rv)) {
            printf("Couldn't use given appdir.\n");
            return 1;
        }
        argc -= 2;
        argv += 2;
    }

    while (argc > 1 && !strcmp(argv[1], "-r")) {
        if (argc < 3)
            return usage();

        nsCOMPtr<nsIFile> lf;
        rv = XRE_GetFileFromPath(argv[2], getter_AddRefs(lf));
        if (NS_FAILED(rv)) {
            printf("Couldn't get manifest file.\n");
            return 1;
        }
        XRE_AddManifestLocation(NS_COMPONENT_LOCATION, lf);

        argc -= 2;
        argv += 2;
    }

#ifdef MOZ_CRASHREPORTER
    const char *val = getenv("MOZ_CRASHREPORTER");
    if (val && *val) {
        rv = CrashReporter::SetExceptionHandler(greDir, true);
        if (NS_FAILED(rv)) {
            printf("CrashReporter::SetExceptionHandler failed!\n");
            return 1;
        }
        MOZ_ASSERT(CrashReporter::GetEnabled());
    }
#endif

    {
        if (argc > 1 && !strcmp(argv[1], "--greomni")) {
            nsCOMPtr<nsIFile> greOmni;
            nsCOMPtr<nsIFile> appOmni;
            XRE_GetFileFromPath(argv[2], getter_AddRefs(greOmni));
            if (argc > 3 && !strcmp(argv[3], "--appomni")) {
                XRE_GetFileFromPath(argv[4], getter_AddRefs(appOmni));
                argc-=2;
                argv+=2;
            } else {
                appOmni = greOmni;
            }

            XRE_InitOmnijar(greOmni, appOmni);
            argc-=2;
            argv+=2;
        }

        nsCOMPtr<nsIServiceManager> servMan;
        rv = NS_InitXPCOM2(getter_AddRefs(servMan), appDir, &dirprovider);
        if (NS_FAILED(rv)) {
            printf("NS_InitXPCOM2 failed!\n");
            return 1;
        }

        nsCOMPtr<nsIJSRuntimeService> rtsvc = do_GetService("@mozilla.org/js/xpc/RuntimeService;1");
        // get the JSRuntime from the runtime svc
        if (!rtsvc) {
            printf("failed to get nsJSRuntimeService!\n");
            return 1;
        }

        if (NS_FAILED(rtsvc->GetRuntime(&rt)) || !rt) {
            printf("failed to get JSRuntime from nsJSRuntimeService!\n");
            return 1;
        }

        rtsvc->RegisterContextCallback(ContextCallback);

        // Override the default XPConnect interrupt callback. We could store the
        // old one and restore it before shutting down, but there's not really a
        // reason to bother.
        sScriptedInterruptCallback.construct(rt, UndefinedValue());
        JS_SetInterruptCallback(rt, XPCShellInterruptCallback);

        cx = JS_NewContext(rt, 8192);
        if (!cx) {
            printf("JS_NewContext failed!\n");
            return 1;
        }

        argc--;
        argv++;
        ProcessArgsForCompartment(cx, argv, argc);

        nsCOMPtr<nsIXPConnect> xpc = do_GetService(nsIXPConnect::GetCID());
        if (!xpc) {
            printf("failed to get nsXPConnect service!\n");
            return 1;
        }

        nsCOMPtr<nsIPrincipal> systemprincipal;
        // Fetch the system principal and store it away in a global, to use for
        // script compilation in Load() and ProcessFile() (including interactive
        // eval loop)
        {

            nsCOMPtr<nsIScriptSecurityManager> securityManager =
                do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);
            if (NS_SUCCEEDED(rv) && securityManager) {
                rv = securityManager->GetSystemPrincipal(getter_AddRefs(systemprincipal));
                if (NS_FAILED(rv)) {
                    fprintf(gErrFile, "+++ Failed to obtain SystemPrincipal from ScriptSecurityManager service.\n");
                } else {
                    // fetch the JS principals and stick in a global
                    gJSPrincipals = nsJSPrincipals::get(systemprincipal);
                    JS_HoldPrincipals(gJSPrincipals);
                }
            } else {
                fprintf(gErrFile, "+++ Failed to get ScriptSecurityManager service, running without principals");
            }
        }

        const JSSecurityCallbacks *scb = JS_GetSecurityCallbacks(rt);
        MOZ_ASSERT(scb, "We are assuming that nsScriptSecurityManager::Init() has been run");
        shellSecurityCallbacks = *scb;
        JS_SetSecurityCallbacks(rt, &shellSecurityCallbacks);

#ifdef TEST_TranslateThis
        nsCOMPtr<nsIXPCFunctionThisTranslator>
            translator(new nsXPCFunctionThisTranslator);
        xpc->SetFunctionThisTranslator(NS_GET_IID(nsITestXPCFunctionCallback), translator);
#endif

        nsCxPusher pusher;
        pusher.Push(cx);

        nsRefPtr<BackstagePass> backstagePass;
        rv = NS_NewBackstagePass(getter_AddRefs(backstagePass));
        if (NS_FAILED(rv)) {
            fprintf(gErrFile, "+++ Failed to create BackstagePass: %8x\n",
                    static_cast<uint32_t>(rv));
            return 1;
        }

        // Make the default XPCShell global use a fresh zone (rather than the
        // System Zone) to improve cross-zone test coverage.
        JS::CompartmentOptions options;
        options.setZone(JS::FreshZone)
               .setVersion(JSVERSION_LATEST);
        nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
        rv = xpc->InitClassesWithNewWrappedGlobal(cx,
                                                  static_cast<nsIGlobalObject *>(backstagePass),
                                                  systemprincipal,
                                                  0,
                                                  options,
                                                  getter_AddRefs(holder));
        if (NS_FAILED(rv))
            return 1;

        {
            JS::Rooted<JSObject*> glob(cx, holder->GetJSObject());
            if (!glob) {
                return 1;
            }

            // Even if we're building in a configuration where source is
            // discarded, there's no reason to do that on XPCShell, and doing so
            // might break various automation scripts.
            JS::CompartmentOptionsRef(glob).setDiscardSource(false);

            backstagePass->SetGlobalObject(glob);

            JSAutoCompartment ac(cx, glob);

            if (!JS_InitReflect(cx, glob)) {
                JS_EndRequest(cx);
                return 1;
            }

            if (!JS_DefineFunctions(cx, glob, glob_functions) ||
                !JS_DefineProfilingFunctions(cx, glob)) {
                JS_EndRequest(cx);
                return 1;
            }

            JS::Rooted<JSObject*> envobj(cx);
            envobj = JS_DefineObject(cx, glob, "environment", &env_class);
            if (!envobj) {
                JS_EndRequest(cx);
                return 1;
            }

            JS_SetPrivate(envobj, envp);

            nsAutoString workingDirectory;
            if (GetCurrentWorkingDirectory(workingDirectory))
                gWorkingDirectory = &workingDirectory;

            JS_DefineProperty(cx, glob, "__LOCATION__", JS::UndefinedHandleValue, 0,
                              GetLocationProperty, nullptr);

            result = ProcessArgs(cx, glob, argv, argc, &dirprovider);

            JS_DropPrincipals(rt, gJSPrincipals);
            JS_SetAllNonReservedSlotsToUndefined(cx, glob);
            JS_GC(rt);
        }
        pusher.Pop();
        JS_GC(rt);
        JS_DestroyContext(cx);
    } // this scopes the nsCOMPtrs

    if (!XRE_ShutdownTestShell())
        NS_ERROR("problem shutting down testshell");

    // no nsCOMPtrs are allowed to be alive when you call NS_ShutdownXPCOM
    rv = NS_ShutdownXPCOM( nullptr );
    MOZ_ASSERT(NS_SUCCEEDED(rv), "NS_ShutdownXPCOM failed");

    sScriptedInterruptCallback.destroyIfConstructed();

#ifdef TEST_CALL_ON_WRAPPED_JS_AFTER_SHUTDOWN
    // test of late call and release (see above)
    JSContext* bogusCX;
    bogus->Peek(&bogusCX);
    bogus = nullptr;
#endif

    appDir = nullptr;
    appFile = nullptr;
    dirprovider.ClearGREDir();
    dirprovider.ClearAppDir();
    dirprovider.ClearPluginDir();
    dirprovider.ClearAppFile();

#ifdef MOZ_CRASHREPORTER
    // Shut down the crashreporter service to prevent leaking some strings it holds.
    if (CrashReporter::GetEnabled())
        CrashReporter::UnsetExceptionHandler();
#endif

    NS_LogTerm();

    return result;
}

void
XPCShellDirProvider::SetGREDir(nsIFile* greDir)
{
    mGREDir = greDir;
}

void
XPCShellDirProvider::SetAppFile(nsIFile* appFile)
{
    mAppFile = appFile;
}

void
XPCShellDirProvider::SetAppDir(nsIFile* appDir)
{
    mAppDir = appDir;
}

void
XPCShellDirProvider::SetPluginDir(nsIFile* pluginDir)
{
    mPluginDir = pluginDir;
}

NS_IMETHODIMP_(MozExternalRefCountType)
XPCShellDirProvider::AddRef()
{
    return 2;
}

NS_IMETHODIMP_(MozExternalRefCountType)
XPCShellDirProvider::Release()
{
    return 1;
}

NS_IMPL_QUERY_INTERFACE(XPCShellDirProvider,
                        nsIDirectoryServiceProvider,
                        nsIDirectoryServiceProvider2)

NS_IMETHODIMP
XPCShellDirProvider::GetFile(const char *prop, bool *persistent,
                             nsIFile* *result)
{
    if (mGREDir && !strcmp(prop, NS_GRE_DIR)) {
        *persistent = true;
        return mGREDir->Clone(result);
    } else if (mAppFile && !strcmp(prop, XRE_EXECUTABLE_FILE)) {
        *persistent = true;
        return mAppFile->Clone(result);
    } else if (mGREDir && !strcmp(prop, NS_APP_PREF_DEFAULTS_50_DIR)) {
        nsCOMPtr<nsIFile> file;
        *persistent = true;
        if (NS_FAILED(mGREDir->Clone(getter_AddRefs(file))) ||
            NS_FAILED(file->AppendNative(NS_LITERAL_CSTRING("defaults"))) ||
            NS_FAILED(file->AppendNative(NS_LITERAL_CSTRING("pref"))))
            return NS_ERROR_FAILURE;
        file.forget(result);
        return NS_OK;
    }

    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
XPCShellDirProvider::GetFiles(const char *prop, nsISimpleEnumerator* *result)
{
    if (mGREDir && !strcmp(prop, "ChromeML")) {
        nsCOMArray<nsIFile> dirs;

        nsCOMPtr<nsIFile> file;
        mGREDir->Clone(getter_AddRefs(file));
        file->AppendNative(NS_LITERAL_CSTRING("chrome"));
        dirs.AppendObject(file);

        nsresult rv = NS_GetSpecialDirectory(NS_APP_CHROME_DIR,
                                             getter_AddRefs(file));
        if (NS_SUCCEEDED(rv))
            dirs.AppendObject(file);

        return NS_NewArrayEnumerator(result, dirs);
    } else if (!strcmp(prop, NS_APP_PREFS_DEFAULTS_DIR_LIST)) {
        nsCOMArray<nsIFile> dirs;
        nsCOMPtr<nsIFile> appDir;
        bool exists;
        if (mAppDir &&
            NS_SUCCEEDED(mAppDir->Clone(getter_AddRefs(appDir))) &&
            NS_SUCCEEDED(appDir->AppendNative(NS_LITERAL_CSTRING("defaults"))) &&
            NS_SUCCEEDED(appDir->AppendNative(NS_LITERAL_CSTRING("preferences"))) &&
            NS_SUCCEEDED(appDir->Exists(&exists)) && exists) {
            dirs.AppendObject(appDir);
            return NS_NewArrayEnumerator(result, dirs);
        }
        return NS_ERROR_FAILURE;
    } else if (!strcmp(prop, NS_APP_PLUGINS_DIR_LIST)) {
        nsCOMArray<nsIFile> dirs;
        // Add the test plugin location passed in by the caller or through
        // runxpcshelltests.
        if (mPluginDir) {
            dirs.AppendObject(mPluginDir);
        // If there was no path specified, default to the one set up by automation
        } else {
            nsCOMPtr<nsIFile> file;
            bool exists;
            // We have to add this path, buildbot copies the test plugin directory
            // to (app)/bin when unpacking test zips.
            if (mGREDir) {
                mGREDir->Clone(getter_AddRefs(file));
                if (NS_SUCCEEDED(mGREDir->Clone(getter_AddRefs(file)))) {
                    file->AppendNative(NS_LITERAL_CSTRING("plugins"));
                    if (NS_SUCCEEDED(file->Exists(&exists)) && exists) {
                        dirs.AppendObject(file);
                    }
                }
            }
        }
        return NS_NewArrayEnumerator(result, dirs);
    }
    return NS_ERROR_FAILURE;
}
