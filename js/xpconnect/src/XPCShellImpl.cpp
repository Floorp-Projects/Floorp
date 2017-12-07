/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsXULAppAPI.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "jsprf.h"
#include "mozilla/ChaosMode.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/Preferences.h"
#include "nsServiceManagerUtils.h"
#include "nsComponentManagerUtils.h"
#include "nsExceptionHandler.h"
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
#include "nsCOMPtr.h"
#include "nsJSPrincipals.h"
#include "xpcpublic.h"
#include "xpcprivate.h"
#include "BackstagePass.h"
#include "nsIScriptSecurityManager.h"
#include "nsIPrincipal.h"
#include "nsJSUtils.h"
#include "gfxPrefs.h"
#include "nsIXULRuntime.h"
#include "GeckoProfiler.h"

#ifdef ANDROID
#include <android/log.h>
#endif

#ifdef XP_WIN
#include "mozilla/widget/AudioSession.h"
#include <windows.h>
#if defined(MOZ_SANDBOX)
#include "SandboxBroker.h"
#endif
#endif

#ifdef MOZ_CODE_COVERAGE
#include "mozilla/CodeCoverageHandler.h"
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

#ifdef ENABLE_TESTS
#include "xpctest_private.h"
#endif

using namespace mozilla;
using namespace JS;
using mozilla::dom::AutoJSAPI;
using mozilla::dom::AutoEntryScript;

class XPCShellDirProvider : public nsIDirectoryServiceProvider2
{
public:
    NS_DECL_ISUPPORTS_INHERITED
    NS_DECL_NSIDIRECTORYSERVICEPROVIDER
    NS_DECL_NSIDIRECTORYSERVICEPROVIDER2

    XPCShellDirProvider() { }
    ~XPCShellDirProvider() { }

    // The platform resource folder
    void SetGREDirs(nsIFile* greDir);
    void ClearGREDirs() { mGREDir = nullptr;
                          mGREBinDir = nullptr; }
    // The application resource folder
    void SetAppDir(nsIFile* appFile);
    void ClearAppDir() { mAppDir = nullptr; }
    // The app executable
    void SetAppFile(nsIFile* appFile);
    void ClearAppFile() { mAppFile = nullptr; }
    // An additional custom plugin dir if specified
    void SetPluginDir(nsIFile* pluginDir);
    void ClearPluginDir() { mPluginDir = nullptr; }

private:
    nsCOMPtr<nsIFile> mGREDir;
    nsCOMPtr<nsIFile> mGREBinDir;
    nsCOMPtr<nsIFile> mAppDir;
    nsCOMPtr<nsIFile> mPluginDir;
    nsCOMPtr<nsIFile> mAppFile;
};

#ifdef XP_WIN
class MOZ_STACK_CLASS AutoAudioSession
{
public:
    AutoAudioSession() {
        widget::StartAudioSession();
    }

    ~AutoAudioSession() {
        widget::StopAudioSession();
    }
};
#endif

#define EXITCODE_RUNTIME_ERROR 3
#define EXITCODE_FILE_NOT_FOUND 4

static FILE* gOutFile = nullptr;
static FILE* gErrFile = nullptr;
static FILE* gInFile = nullptr;

static int gExitCode = 0;
static bool gQuitting = false;
static bool reportWarnings = true;
static bool compileOnly = false;

static JSPrincipals* gJSPrincipals = nullptr;
static nsAutoString* gWorkingDirectory = nullptr;

static bool
GetLocationProperty(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    if (!args.thisv().isObject()) {
        JS_ReportErrorASCII(cx, "Unexpected this value for GetLocationProperty");
        return false;
    }
#if !defined(XP_WIN) && !defined(XP_UNIX)
    //XXX: your platform should really implement this
    return false;
#else
    JS::AutoFilename filename;
    if (JS::DescribeScriptedCaller(cx, &filename) && filename.get()) {
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
        nsresult rv = NS_NewLocalFile(filenameString,
                                      false, getter_AddRefs(location));

        if (!location && gWorkingDirectory) {
            // could be a relative path, try appending it to the cwd
            // and then normalize
            nsAutoString absolutePath(*gWorkingDirectory);
            absolutePath.Append(filenameString);

            rv = NS_NewLocalFile(absolutePath,
                                 false, getter_AddRefs(location));
        }

        if (location) {
            bool symlink;
            // don't normalize symlinks, because that's kind of confusing
            if (NS_SUCCEEDED(location->IsSymlink(&symlink)) &&
                !symlink)
                location->Normalize();
            RootedObject locationObj(cx);
            rv = nsXPConnect::XPConnect()->WrapNative(cx, &args.thisv().toObject(),
                                                      location,
                                                      NS_GET_IID(nsIFile),
                                                      locationObj.address());
            if (NS_SUCCEEDED(rv) && locationObj) {
                args.rval().setObject(*locationObj);
            }
        }
    }

    return true;
#endif
}

static bool
GetLine(JSContext* cx, char* bufp, FILE* file, const char* prompt)
{
    fputs(prompt, gOutFile);
    fflush(gOutFile);

    char line[4096] = { '\0' };
    while (true) {
        if (fgets(line, sizeof line, file)) {
            strcpy(bufp, line);
            return true;
        }
        if (errno != EINTR) {
            return false;
        }
    }
}

static bool
ReadLine(JSContext* cx, unsigned argc, Value* vp)
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
        str = JS_GetEmptyString(cx);
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
Print(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    args.rval().setUndefined();

    RootedString str(cx);
    nsAutoCString utf8output;

    for (unsigned i = 0; i < args.length(); i++) {
        str = ToString(cx, args[i]);
        if (!str)
            return false;

        JSAutoByteString utf8str;
        if (!utf8str.encodeUtf8(cx, str))
            return false;

        if (i)
            utf8output.Append(' ');
        utf8output.Append(utf8str.ptr(), utf8str.length());
    }
    utf8output.Append('\n');
    fputs(utf8output.get(), gOutFile);
    fflush(gOutFile);
    return true;
}

static bool
Dump(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    args.rval().setUndefined();

    if (!args.length())
         return true;

    RootedString str(cx, ToString(cx, args[0]));
    if (!str)
        return false;

    JSAutoByteString utf8str;
    if (!utf8str.encodeUtf8(cx, str))
        return false;

#ifdef ANDROID
    __android_log_print(ANDROID_LOG_INFO, "Gecko", "%s", utf8str.ptr());
#endif
#ifdef XP_WIN
    if (IsDebuggerPresent()) {
        nsAutoJSString wstr;
        if (!wstr.init(cx, str))
            return false;
        OutputDebugStringW(wstr.get());
    }
#endif
    fputs(utf8str.ptr(), gOutFile);
    fflush(gOutFile);
    return true;
}

static bool
Load(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    JS::Rooted<JSObject*> obj(cx, JS_THIS_OBJECT(cx, vp));
    if (!obj)
        return false;

    if (!JS_IsGlobalObject(obj)) {
        JS_ReportErrorASCII(cx, "Trying to load() into a non-global object");
        return false;
    }

    RootedString str(cx);
    for (unsigned i = 0; i < args.length(); i++) {
        str = ToString(cx, args[i]);
        if (!str)
            return false;
        JSAutoByteString filename(cx, str);
        if (!filename)
            return false;
        FILE* file = fopen(filename.ptr(), "r");
        if (!file) {
            filename.clear();
            if (!filename.encodeUtf8(cx, str))
                return false;
            JS_ReportErrorUTF8(cx, "cannot open file '%s' for reading",
                               filename.ptr());
            return false;
        }
        JS::CompileOptions options(cx);
        options.setUTF8(true)
               .setFileAndLine(filename.ptr(), 1)
               .setIsRunOnce(true);
        JS::Rooted<JSScript*> script(cx);
        JS::Rooted<JSObject*> global(cx, JS::CurrentGlobalOrNull(cx));
        JS::Compile(cx, options, file, &script);
        fclose(file);
        if (!script)
            return false;

        if (!compileOnly) {
            if (!JS_ExecuteScript(cx, script)) {
                return false;
            }
        }
    }
    args.rval().setUndefined();
    return true;
}

static bool
Quit(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    gExitCode = 0;
    if (!ToInt32(cx, args.get(0), &gExitCode))
        return false;

    gQuitting = true;
//    exit(0);
    return false;
}

static bool
DumpXPC(JSContext* cx, unsigned argc, Value* vp)
{
    JS::CallArgs args = CallArgsFromVp(argc, vp);

    uint16_t depth = 2;
    if (args.length() > 0) {
        if (!JS::ToUint16(cx, args[0], &depth))
            return false;
    }

    nsXPConnect::XPConnect()->DebugDump(int16_t(depth));
    args.rval().setUndefined();
    return true;
}

static bool
GC(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    JS_GC(cx);

    args.rval().setUndefined();
    return true;
}

#ifdef JS_GC_ZEAL
static bool
GCZeal(JSContext* cx, unsigned argc, Value* vp)
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
SendCommand(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (args.length() == 0) {
        JS_ReportErrorASCII(cx, "Function takes at least one argument!");
        return false;
    }

    RootedString str(cx, ToString(cx, args[0]));
    if (!str) {
        JS_ReportErrorASCII(cx, "Could not convert argument 1 to string!");
        return false;
    }

    if (args.length() > 1 && JS_TypeOfValue(cx, args[1]) != JSTYPE_FUNCTION) {
        JS_ReportErrorASCII(cx, "Could not convert argument 2 to function!");
        return false;
    }

    if (!XRE_SendTestShellCommand(cx, str, args.length() > 1 ? args[1].address() : nullptr)) {
        JS_ReportErrorASCII(cx, "Couldn't send command!");
        return false;
    }

    args.rval().setUndefined();
    return true;
}

static bool
Options(JSContext* cx, unsigned argc, Value* vp)
{
    JS::CallArgs args = CallArgsFromVp(argc, vp);
    ContextOptions oldContextOptions = ContextOptionsRef(cx);

    RootedString str(cx);
    JSAutoByteString opt;
    for (unsigned i = 0; i < args.length(); ++i) {
        str = ToString(cx, args[i]);
        if (!str)
            return false;

        opt.clear();
        if (!opt.encodeUtf8(cx, str))
            return false;

        if (strcmp(opt.ptr(), "strict") == 0)
            ContextOptionsRef(cx).toggleExtraWarnings();
        else if (strcmp(opt.ptr(), "werror") == 0)
            ContextOptionsRef(cx).toggleWerror();
        else if (strcmp(opt.ptr(), "strict_mode") == 0)
            ContextOptionsRef(cx).toggleStrictMode();
        else {
            JS_ReportErrorUTF8(cx, "unknown option name '%s'. The valid names are "
                               "strict, werror, and strict_mode.", opt.ptr());
            return false;
        }
    }

    UniqueChars names;
    if (oldContextOptions.extraWarnings()) {
        names = JS_sprintf_append(Move(names), "%s", "strict");
        if (!names) {
            JS_ReportOutOfMemory(cx);
            return false;
        }
    }
    if (oldContextOptions.werror()) {
        names = JS_sprintf_append(Move(names), "%s%s", names ? "," : "", "werror");
        if (!names) {
            JS_ReportOutOfMemory(cx);
            return false;
        }
    }
    if (names && oldContextOptions.strictMode()) {
        names = JS_sprintf_append(Move(names), "%s%s", names ? "," : "", "strict_mode");
        if (!names) {
            JS_ReportOutOfMemory(cx);
            return false;
        }
    }

    str = JS_NewStringCopyZ(cx, names.get());
    if (!str)
        return false;

    args.rval().setString(str);
    return true;
}

static PersistentRootedValue *sScriptedInterruptCallback = nullptr;

static bool
XPCShellInterruptCallback(JSContext* cx)
{
    MOZ_ASSERT(sScriptedInterruptCallback->initialized());
    RootedValue callback(cx, *sScriptedInterruptCallback);

    // If no interrupt callback was set by script, no-op.
    if (callback.isUndefined())
        return true;

    JSAutoCompartment ac(cx, &callback.toObject());
    RootedValue rv(cx);
    if (!JS_CallFunctionValue(cx, nullptr, callback, JS::HandleValueArray::empty(), &rv) ||
        !rv.isBoolean())
    {
        NS_WARNING("Scripted interrupt callback failed! Terminating script.");
        JS_ClearPendingException(cx);
        return false;
    }

    return rv.toBoolean();
}

static bool
SetInterruptCallback(JSContext* cx, unsigned argc, Value* vp)
{
    MOZ_ASSERT(sScriptedInterruptCallback->initialized());

    // Sanity-check args.
    JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
    if (args.length() != 1) {
        JS_ReportErrorASCII(cx, "Wrong number of arguments");
        return false;
    }

    // Allow callers to remove the interrupt callback by passing undefined.
    if (args[0].isUndefined()) {
        *sScriptedInterruptCallback = UndefinedValue();
        return true;
    }

    // Otherwise, we should have a callable object.
    if (!args[0].isObject() || !JS::IsCallable(&args[0].toObject())) {
        JS_ReportErrorASCII(cx, "Argument must be callable");
        return false;
    }

    *sScriptedInterruptCallback = args[0];

    return true;
}

static bool
SimulateActivityCallback(JSContext* cx, unsigned argc, Value* vp)
{
    // Sanity-check args.
    JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
    if (args.length() != 1 || !args[0].isBoolean()) {
        JS_ReportErrorASCII(cx, "Wrong number of arguments");
        return false;
    }
    xpc::SimulateActivityCallback(args[0].toBoolean());
    return true;
}

static bool
RegisterAppManifest(JSContext* cx, unsigned argc, Value* vp)
{
    JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
    if (args.length() != 1) {
        JS_ReportErrorASCII(cx, "Wrong number of arguments");
        return false;
    }
    if (!args[0].isObject()) {
        JS_ReportErrorASCII(cx, "Expected object as argument 1 to registerAppManifest");
        return false;
    }

    Rooted<JSObject*> arg1(cx, &args[0].toObject());
    nsCOMPtr<nsIFile> file;
    nsresult rv = nsXPConnect::XPConnect()->
        WrapJS(cx, arg1, NS_GET_IID(nsIFile), getter_AddRefs(file));
    if (NS_FAILED(rv)) {
        XPCThrower::Throw(rv, cx);
        return false;
    }
    rv = XRE_AddManifestLocation(NS_APP_LOCATION, file);
    if (NS_FAILED(rv)) {
        XPCThrower::Throw(rv, cx);
        return false;
    }
    return true;
}

#ifdef ENABLE_TESTS
static bool
RegisterXPCTestComponents(JSContext* cx, unsigned argc, Value* vp)
{
    JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
    if (args.length() != 0) {
        JS_ReportErrorASCII(cx, "Wrong number of arguments");
        return false;
    }
    nsresult rv = XRE_AddStaticComponent(&kXPCTestModule);
    if (NS_FAILED(rv)) {
        XPCThrower::Throw(rv, cx);
        return false;
    }
    return true;
}
#endif

static const JSFunctionSpec glob_functions[] = {
    JS_FN("print",           Print,          0,0),
    JS_FN("readline",        ReadLine,       1,0),
    JS_FN("load",            Load,           1,0),
    JS_FN("quit",            Quit,           0,0),
    JS_FN("dumpXPC",         DumpXPC,        1,0),
    JS_FN("dump",            Dump,           1,0),
    JS_FN("gc",              GC,             0,0),
#ifdef JS_GC_ZEAL
    JS_FN("gczeal",          GCZeal,         1,0),
#endif
    JS_FN("options",         Options,        0,0),
    JS_FN("sendCommand",     SendCommand,    1,0),
    JS_FN("atob",            xpc::Atob,      1,0),
    JS_FN("btoa",            xpc::Btoa,      1,0),
    JS_FN("setInterruptCallback", SetInterruptCallback, 1,0),
    JS_FN("simulateActivityCallback", SimulateActivityCallback, 1,0),
    JS_FN("registerAppManifest", RegisterAppManifest, 1, 0),
#ifdef ENABLE_TESTS
    JS_FN("registerXPCTestComponents", RegisterXPCTestComponents, 0, 0),
#endif
    JS_FS_END
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
    { #name, format, count } ,
#include "jsshell.msg"
#undef MSG_DEF
};

static const JSErrorFormatString*
my_GetErrorMessage(void* userRef, const unsigned errorNumber)
{
    if (errorNumber == 0 || errorNumber >= JSShellErr_Limit)
        return nullptr;

    return &jsShell_ErrorFormatString[errorNumber];
}

static bool
ProcessLine(AutoJSAPI& jsapi, const char* buffer, int startline)
{
    JSContext* cx = jsapi.cx();
    JS::RootedScript script(cx);
    JS::RootedValue result(cx);
    JS::CompileOptions options(cx);
    options.setFileAndLine("typein", startline)
           .setIsRunOnce(true);
    if (!JS_CompileScript(cx, buffer, strlen(buffer), options, &script))
        return false;
    if (compileOnly)
        return true;
    if (!JS_ExecuteScript(cx, script, &result))
        return false;

    if (result.isUndefined())
        return true;
    RootedString str(cx);
    if (!(str = ToString(cx, result)))
        return false;
    JSAutoByteString bytes;
    if (!bytes.encodeLatin1(cx, str))
        return false;

    fprintf(gOutFile, "%s\n", bytes.ptr());
    return true;
}

static bool
ProcessFile(AutoJSAPI& jsapi, const char* filename, FILE* file, bool forceTTY)
{
    JSContext* cx = jsapi.cx();
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
                if (ch == '\n' || ch == '\r')
                    break;
            }
        }
        ungetc(ch, file);

        JS::RootedScript script(cx);
        JS::RootedValue unused(cx);
        JS::CompileOptions options(cx);
        options.setUTF8(true)
               .setFileAndLine(filename, 1)
               .setIsRunOnce(true)
               .setNoScriptRval(true);
        if (!JS::Compile(cx, options, file, &script))
            return false;
        return compileOnly || JS_ExecuteScript(cx, script, &unused);
    }

    /* It's an interactive filehandle; drop into read-eval-print loop. */
    int lineno = 1;
    bool hitEOF = false;
    do {
        char buffer[4096];
        char* bufp = buffer;
        *bufp = '\0';

        /*
         * Accumulate lines until we get a 'compilable unit' - one that either
         * generates an error (before running out of source) or that compiles
         * cleanly.  This should be whenever we get a complete statement that
         * coincides with the end of a line.
         */
        int startline = lineno;
        do {
            if (!GetLine(cx, bufp, file, startline == lineno ? "js> " : "")) {
                hitEOF = true;
                break;
            }
            bufp += strlen(bufp);
            lineno++;
        } while (!JS_BufferIsCompilableUnit(cx, global, buffer, strlen(buffer)));

        if (!ProcessLine(jsapi, buffer, startline))
            jsapi.ReportException();
    } while (!hitEOF && !gQuitting);

    fprintf(gOutFile, "\n");
    return true;
}

static bool
Process(AutoJSAPI& jsapi, const char* filename, bool forceTTY)
{
    FILE* file;

    if (forceTTY || !filename || strcmp(filename, "-") == 0) {
        file = stdin;
    } else {
        file = fopen(filename, "r");
        if (!file) {
            /*
             * Use Latin1 variant here because the encoding of the return value
             * of strerror function can be non-UTF-8.
             */
            JS_ReportErrorNumberLatin1(jsapi.cx(), my_GetErrorMessage, nullptr,
                                       JSSMSG_CANT_OPEN,
                                       filename, strerror(errno));
            gExitCode = EXITCODE_FILE_NOT_FOUND;
            return false;
        }
    }

    bool ok = ProcessFile(jsapi, filename, file, forceTTY);
    if (file != stdin)
        fclose(file);
    return ok;
}

static int
usage()
{
    fprintf(gErrFile, "%s\n", JS_GetImplementationVersion());
    fprintf(gErrFile, "usage: xpcshell [-g gredir] [-a appdir] [-r manifest]... [-WwxiCSsmIp] [-f scriptfile] [-e script] [scriptfile] [scriptarg...]\n");
    return 2;
}

static bool
printUsageAndSetExitCode()
{
    gExitCode = usage();
    return false;
}

static void
ProcessArgsForCompartment(JSContext* cx, char** argv, int argc)
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
            MOZ_FALLTHROUGH; // because -S implies -s
        case 's':
            ContextOptionsRef(cx).toggleExtraWarnings();
            break;
        case 'I':
            ContextOptionsRef(cx).toggleIon()
                                 .toggleAsmJS()
                                 .toggleWasm();
            break;
        }
    }
}

static bool
ProcessArgs(AutoJSAPI& jsapi, char** argv, int argc, XPCShellDirProvider* aDirProvider)
{
    JSContext* cx = jsapi.cx();
    const char rcfilename[] = "xpcshell.js";
    FILE* rcfile;
    int rootPosition;
    JS::Rooted<JSObject*> argsObj(cx);
    char* filename = nullptr;
    bool isInteractive = true;
    bool forceTTY = false;

    rcfile = fopen(rcfilename, "r");
    if (rcfile) {
        printf("[loading '%s'...]\n", rcfilename);
        bool ok = ProcessFile(jsapi, rcfilename, rcfile, false);
        fclose(rcfile);
        if (!ok) {
            return false;
        }
    }

    JS::Rooted<JSObject*> global(cx, JS::CurrentGlobalOrNull(cx));

    /*
     * Scan past all optional arguments so we can create the arguments object
     * before processing any -f options, which must interleave properly with
     * -v and -w options.  This requires two passes, and without getopt, we'll
     * have to keep the option logic here and in the second for loop in sync.
     * First of all, find out the first argument position which will be passed
     * as a script file to be executed.
     */
    for (rootPosition = 0; rootPosition < argc; rootPosition++) {
        if (argv[rootPosition][0] != '-' || argv[rootPosition][1] == '\0') {
            ++rootPosition;
            break;
        }

        bool isPairedFlag =
            argv[rootPosition][0] != '\0' &&
            (argv[rootPosition][1] == 'v' ||
             argv[rootPosition][1] == 'f' ||
             argv[rootPosition][1] == 'e');
        if (isPairedFlag && rootPosition < argc - 1) {
          ++rootPosition; // Skip over the 'foo' portion of |-v foo|, |-f foo|, or |-e foo|.
        }
    }

    /*
     * Create arguments early and define it to root it, so it's safe from any
     * GC calls nested below, and so it is available to -f <file> arguments.
     */
    argsObj = JS_NewArrayObject(cx, 0);
    if (!argsObj)
        return 1;
    if (!JS_DefineProperty(cx, global, "arguments", argsObj, 0))
        return 1;

    for (int j = 0, length = argc - rootPosition; j < length; j++) {
        RootedString str(cx, JS_NewStringCopyZ(cx, argv[rootPosition++]));
        if (!str ||
            !JS_DefineElement(cx, argsObj, j, str, JSPROP_ENUMERATE)) {
            return 1;
        }
    }

    for (int i = 0; i < argc; i++) {
        if (argv[i][0] != '-' || argv[i][1] == '\0') {
            filename = argv[i++];
            isInteractive = false;
            break;
        }
        switch (argv[i][1]) {
        case 'W':
            reportWarnings = false;
            break;
        case 'w':
            reportWarnings = true;
            break;
        case 'x':
            break;
        case 'd':
            /* This used to try to turn on the debugger. */
            break;
        case 'm':
            break;
        case 'f':
            if (++i == argc) {
                return printUsageAndSetExitCode();
            }
            if (!Process(jsapi, argv[i], false))
                return false;
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
                return printUsageAndSetExitCode();
            }

            JS::CompileOptions opts(cx);
            opts.setFileAndLine("-e", 1);
            JS::Evaluate(cx, opts, argv[i], strlen(argv[i]), &rval);

            isInteractive = false;
            break;
        }
        case 'C':
            compileOnly = true;
            isInteractive = false;
            break;
        case 'S':
        case 's':
        case 'I':
            // These options are processed in ProcessArgsForCompartment.
            break;
        case 'p':
        {
          // plugins path
          char* pluginPath = argv[++i];
          nsCOMPtr<nsIFile> pluginsDir;
          if (NS_FAILED(XRE_GetFileFromPath(pluginPath, getter_AddRefs(pluginsDir)))) {
              fprintf(gErrFile, "Couldn't use given plugins dir.\n");
              return printUsageAndSetExitCode();
          }
          aDirProvider->SetPluginDir(pluginsDir);
          break;
        }
        default:
            return printUsageAndSetExitCode();
        }
    }

    if (filename || isInteractive)
        return Process(jsapi, filename, forceTTY);
    return true;
}

/***************************************************************************/

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
XRE_XPCShellMain(int argc, char** argv, char** envp,
                 const XREShellData* aShellData)
{
    MOZ_ASSERT(aShellData);

    JSContext* cx;
    int result = 0;
    nsresult rv;

    gErrFile = stderr;
    gOutFile = stdout;
    gInFile = stdin;

    NS_LogInit();

    mozilla::LogModule::Init();

#ifdef MOZ_GECKO_PROFILER
    char aLocal;
    profiler_init(&aLocal);
#endif

    if (PR_GetEnv("MOZ_CHAOSMODE")) {
        ChaosFeature feature = ChaosFeature::Any;
        long featureInt = strtol(PR_GetEnv("MOZ_CHAOSMODE"), nullptr, 16);
        if (featureInt) {
            // NOTE: MOZ_CHAOSMODE=0 or a non-hex value maps to Any feature.
            feature = static_cast<ChaosFeature>(featureInt);
        }
        ChaosMode::SetChaosFeature(feature);
    }

    if (ChaosMode::isActive(ChaosFeature::Any)) {
        printf_stderr("*** You are running in chaos test mode. See ChaosMode.h. ***\n");
    }

    // The provider needs to outlive the call to shutting down XPCOM.
    XPCShellDirProvider dirprovider;

    { // Start scoping nsCOMPtrs
        nsCOMPtr<nsIFile> appFile;
        rv = XRE_GetBinaryPath(getter_AddRefs(appFile));
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

            dirprovider.SetGREDirs(greDir);

            argc -= 2;
            argv += 2;
        } else {
#ifdef XP_MACOSX
            // On OSX, the GreD needs to point to Contents/Resources in the .app
            // bundle. Libraries will be loaded at a relative path to GreD, i.e.
            // ../MacOS.
            nsCOMPtr<nsIFile> tmpDir;
            XRE_GetFileFromPath(argv[0], getter_AddRefs(greDir));
            greDir->GetParent(getter_AddRefs(tmpDir));
            tmpDir->Clone(getter_AddRefs(greDir));
            tmpDir->SetNativeLeafName(NS_LITERAL_CSTRING("Resources"));
            bool dirExists = false;
            tmpDir->Exists(&dirExists);
            if (dirExists) {
                greDir = tmpDir.forget();
            }
            dirprovider.SetGREDirs(greDir);
#else
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
#endif
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
            XRE_AddManifestLocation(NS_APP_LOCATION, lf);

            argc -= 2;
            argv += 2;
        }

        const char* val = getenv("MOZ_CRASHREPORTER");
        if (val && *val && !CrashReporter::IsDummy()) {
            rv = CrashReporter::SetExceptionHandler(greDir, true);
            if (NS_FAILED(rv)) {
                printf("CrashReporter::SetExceptionHandler failed!\n");
                return 1;
            }
            MOZ_ASSERT(CrashReporter::GetEnabled());
        }

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

        // xpc::ErrorReport::LogToConsoleWithStack needs this to print errors
        // to stderr.
        Preferences::SetBool("browser.dom.window.dump.enabled", true);

        AutoJSAPI jsapi;
        jsapi.Init();
        cx = jsapi.cx();

        // Override the default XPConnect interrupt callback. We could store the
        // old one and restore it before shutting down, but there's not really a
        // reason to bother.
        sScriptedInterruptCallback = new PersistentRootedValue;
        sScriptedInterruptCallback->init(cx, UndefinedValue());

        JS_AddInterruptCallback(cx, XPCShellInterruptCallback);

        argc--;
        argv++;
        ProcessArgsForCompartment(cx, argv, argc);

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

        const JSSecurityCallbacks* scb = JS_GetSecurityCallbacks(cx);
        MOZ_ASSERT(scb, "We are assuming that nsScriptSecurityManager::Init() has been run");
        shellSecurityCallbacks = *scb;
        JS_SetSecurityCallbacks(cx, &shellSecurityCallbacks);

        RefPtr<BackstagePass> backstagePass;
        rv = NS_NewBackstagePass(getter_AddRefs(backstagePass));
        if (NS_FAILED(rv)) {
            fprintf(gErrFile, "+++ Failed to create BackstagePass: %8x\n",
                    static_cast<uint32_t>(rv));
            return 1;
        }

        // Make the default XPCShell global use a fresh zone (rather than the
        // System Zone) to improve cross-zone test coverage.
        JS::CompartmentOptions options;
        options.creationOptions().setNewZoneInSystemZoneGroup();
        if (xpc::SharedMemoryEnabled())
            options.creationOptions().setSharedMemoryAndAtomicsEnabled(true);
        JS::Rooted<JSObject*> glob(cx);
        rv = xpc::InitClassesWithNewWrappedGlobal(cx,
                                                  static_cast<nsIGlobalObject*>(backstagePass),
                                                  systemprincipal,
                                                  0,
                                                  options,
                                                  &glob);
        if (NS_FAILED(rv))
            return 1;

        // Initialize graphics prefs on the main thread, if not already done
        gfxPrefs::GetSingleton();
        // Initialize e10s check on the main thread, if not already done
        BrowserTabsRemoteAutostart();
#ifdef XP_WIN
        // Plugin may require audio session if installed plugin can initialize
        // asynchronized.
        AutoAudioSession audioSession;

#if defined(MOZ_SANDBOX)
        // Required for sandboxed child processes.
        if (aShellData->sandboxBrokerServices) {
          SandboxBroker::Initialize(aShellData->sandboxBrokerServices);
          SandboxBroker::GeckoDependentInitialize();
        } else {
          NS_WARNING("Failed to initialize broker services, sandboxed "
                     "processes will fail to start.");
        }
#endif
#endif

#ifdef MOZ_CODE_COVERAGE
        CodeCoverageHandler::Init();
#endif

        {
            if (!glob) {
                return 1;
            }

            // Even if we're building in a configuration where source is
            // discarded, there's no reason to do that on XPCShell, and doing so
            // might break various automation scripts.
            JS::CompartmentBehaviorsRef(glob).setDiscardSource(false);

            backstagePass->SetGlobalObject(glob);

            JSAutoCompartment ac(cx, glob);

            if (!JS_InitReflectParse(cx, glob)) {
                return 1;
            }

            if (!JS_DefineFunctions(cx, glob, glob_functions) ||
                !JS_DefineProfilingFunctions(cx, glob)) {
                return 1;
            }

            nsAutoString workingDirectory;
            if (GetCurrentWorkingDirectory(workingDirectory))
                gWorkingDirectory = &workingDirectory;

            JS_DefineProperty(cx, glob, "__LOCATION__",
                              GetLocationProperty, nullptr,
                              0);

            {
                // We are almost certainly going to run script here, so we need an
                // AutoEntryScript. This is Gecko-specific and not in any spec.
                AutoEntryScript aes(backstagePass, "xpcshell argument processing");

                // If an exception is thrown, we'll set our return code
                // appropriately, and then let the AutoEntryScript destructor report
                // the error to the console.
                if (!ProcessArgs(aes, argv, argc, &dirprovider)) {
                    if (gExitCode) {
                        result = gExitCode;
                    } else if (gQuitting) {
                        result = 0;
                    } else {
                        result = EXITCODE_RUNTIME_ERROR;
                    }
                }
            }

            JS_DropPrincipals(cx, gJSPrincipals);
            JS_SetAllNonReservedSlotsToUndefined(cx, glob);
            JS_SetAllNonReservedSlotsToUndefined(cx, JS_GlobalLexicalEnvironment(glob));
            JS_GC(cx);
        }
        JS_GC(cx);

        dirprovider.ClearGREDirs();
        dirprovider.ClearAppDir();
        dirprovider.ClearPluginDir();
        dirprovider.ClearAppFile();
    } // this scopes the nsCOMPtrs

    if (!XRE_ShutdownTestShell())
        NS_ERROR("problem shutting down testshell");

    // no nsCOMPtrs are allowed to be alive when you call NS_ShutdownXPCOM
    rv = NS_ShutdownXPCOM( nullptr );
    MOZ_ASSERT(NS_SUCCEEDED(rv), "NS_ShutdownXPCOM failed");

    // Shut down the crashreporter service to prevent leaking some strings it holds.
    if (CrashReporter::GetEnabled()) {
        CrashReporter::UnsetExceptionHandler();
    }

#ifdef MOZ_GECKO_PROFILER
    // This must precede NS_LogTerm(), otherwise xpcshell return non-zero
    // during some tests, which causes failures.
    profiler_shutdown();
#endif

    NS_LogTerm();

    return result;
}

void
XPCShellDirProvider::SetGREDirs(nsIFile* greDir)
{
    mGREDir = greDir;
    mGREDir->Clone(getter_AddRefs(mGREBinDir));

#ifdef XP_MACOSX
    nsAutoCString leafName;
    mGREDir->GetNativeLeafName(leafName);
    if (leafName.EqualsLiteral("Resources")) {
        mGREBinDir->SetNativeLeafName(NS_LITERAL_CSTRING("MacOS"));
    }
#endif
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
XPCShellDirProvider::GetFile(const char* prop, bool* persistent,
                             nsIFile* *result)
{
    if (mGREDir && !strcmp(prop, NS_GRE_DIR)) {
        *persistent = true;
        return mGREDir->Clone(result);
    } else if (mGREBinDir && !strcmp(prop, NS_GRE_BIN_DIR)) {
        *persistent = true;
        return mGREBinDir->Clone(result);
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
XPCShellDirProvider::GetFiles(const char* prop, nsISimpleEnumerator* *result)
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
