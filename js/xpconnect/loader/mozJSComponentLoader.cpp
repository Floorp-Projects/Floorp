/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Attributes.h"

#include <cstdarg>

#include "mozilla/Logging.h"
#ifdef ANDROID
#include <android/log.h>
#endif
#ifdef XP_WIN
#include <windows.h>
#endif

#include "jsapi.h"
#include "js/Printf.h"
#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "nsIComponentManager.h"
#include "mozilla/Module.h"
#include "nsIFile.h"
#include "mozJSComponentLoader.h"
#include "mozJSLoaderUtils.h"
#include "nsIXPConnect.h"
#include "nsIObserverService.h"
#include "nsIScriptSecurityManager.h"
#include "nsIFileURL.h"
#include "nsIJARURI.h"
#include "nsNetUtil.h"
#include "nsJSPrincipals.h"
#include "nsJSUtils.h"
#include "xpcprivate.h"
#include "xpcpublic.h"
#include "nsContentUtils.h"
#include "nsReadableUtils.h"
#include "nsXULAppAPI.h"
#include "GeckoProfiler.h"
#include "WrapperFactory.h"

#include "AutoMemMap.h"
#include "ScriptPreloader-inl.h"

#include "mozilla/scache/StartupCache.h"
#include "mozilla/scache/StartupCacheUtils.h"
#include "mozilla/MacroForEach.h"
#include "mozilla/Preferences.h"
#include "mozilla/ResultExtensions.h"
#include "mozilla/ScriptPreloader.h"
#include "mozilla/dom/DOMPrefs.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/ResultExtensions.h"
#include "mozilla/UniquePtrExtensions.h"
#include "mozilla/Unused.h"

#ifdef MOZ_CRASHREPORTER
#include "mozilla/ipc/CrashReporterClient.h"
#endif

using namespace mozilla;
using namespace mozilla::scache;
using namespace mozilla::loader;
using namespace xpc;
using namespace JS;

static const char kObserverServiceContractID[] = "@mozilla.org/observer-service;1";

#define JS_CACHE_PREFIX(aType) "jsloader/" aType

/**
 * Buffer sizes for serialization and deserialization of scripts.
 * FIXME: bug #411579 (tune this macro!) Last updated: Jan 2008
 */
#define XPC_SERIALIZATION_BUFFER_SIZE   (64 * 1024)
#define XPC_DESERIALIZATION_BUFFER_SIZE (12 * 8192)

// MOZ_LOG=JSComponentLoader:5
static LazyLogModule gJSCLLog("JSComponentLoader");

#define LOG(args) MOZ_LOG(gJSCLLog, mozilla::LogLevel::Debug, args)

// Components.utils.import error messages
#define ERROR_SCOPE_OBJ "%s - Second argument must be an object."
#define ERROR_NOT_PRESENT "%s - EXPORTED_SYMBOLS is not present."
#define ERROR_NOT_AN_ARRAY "%s - EXPORTED_SYMBOLS is not an array."
#define ERROR_GETTING_ARRAY_LENGTH "%s - Error getting array length of EXPORTED_SYMBOLS."
#define ERROR_ARRAY_ELEMENT "%s - EXPORTED_SYMBOLS[%d] is not a string."
#define ERROR_GETTING_SYMBOL "%s - Could not get symbol '%s'."
#define ERROR_SETTING_SYMBOL "%s - Could not set symbol '%s' on target object."

static bool
Dump(JSContext* cx, unsigned argc, Value* vp)
{
    if (!mozilla::dom::DOMPrefs::DumpEnabled()) {
        return true;
    }

    CallArgs args = CallArgsFromVp(argc, vp);

    if (args.length() == 0)
        return true;

    RootedString str(cx, JS::ToString(cx, args[0]));
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
    fputs(utf8str.ptr(), stdout);
    fflush(stdout);
    return true;
}

static bool
Debug(JSContext* cx, unsigned argc, Value* vp)
{
#ifdef DEBUG
    return Dump(cx, argc, vp);
#else
    return true;
#endif
}

static const JSFunctionSpec gGlobalFun[] = {
    JS_FN("dump",    Dump,   1,0),
    JS_FN("debug",   Debug,  1,0),
    JS_FN("atob",    Atob,   1,0),
    JS_FN("btoa",    Btoa,   1,0),
    JS_FS_END
};

class MOZ_STACK_CLASS JSCLContextHelper
{
public:
    explicit JSCLContextHelper(JSContext* aCx);
    ~JSCLContextHelper();

    void reportErrorAfterPop(UniqueChars&& buf);

private:
    JSContext* mContext;
    UniqueChars mBuf;

    // prevent copying and assignment
    JSCLContextHelper(const JSCLContextHelper&) = delete;
    const JSCLContextHelper& operator=(const JSCLContextHelper&) = delete;
};

static nsresult
MOZ_FORMAT_PRINTF(2, 3)
ReportOnCallerUTF8(JSContext* callerContext,
                   const char* format, ...) {
    if (!callerContext) {
        return NS_ERROR_FAILURE;
    }

    va_list ap;
    va_start(ap, format);

    UniqueChars buf = JS_vsmprintf(format, ap);
    if (!buf) {
        va_end(ap);
        return NS_ERROR_OUT_OF_MEMORY;
    }

    JS_ReportErrorUTF8(callerContext, "%s", buf.get());

    va_end(ap);
    return NS_OK;
}

mozJSComponentLoader::mozJSComponentLoader()
    : mModules(16),
      mImports(16),
      mInProgressImports(16),
      mLocations(16),
      mInitialized(false),
      mShareLoaderGlobal(false),
      mLoaderGlobal(dom::RootingCx())
{
    MOZ_ASSERT(!sSelf, "mozJSComponentLoader should be a singleton");
}

// static
already_AddRefed<mozJSComponentLoader>
mozJSComponentLoader::GetOrCreate()
{
    if (!sSelf) {
        sSelf = new mozJSComponentLoader();
    }
    return do_AddRef(sSelf);
}

#define ENSURE_DEP(name) { nsresult rv = Ensure##name(); NS_ENSURE_SUCCESS(rv, rv); }
#define ENSURE_DEPS(...) MOZ_FOR_EACH(ENSURE_DEP, (), (__VA_ARGS__));
#define BEGIN_ENSURE(self, ...) { \
    if (m##self) \
        return NS_OK; \
    ENSURE_DEPS(__VA_ARGS__); \
}

class MOZ_STACK_CLASS ComponentLoaderInfo {
  public:
    explicit ComponentLoaderInfo(const nsACString& aLocation) : mLocation(aLocation) {}

    nsIIOService* IOService() { MOZ_ASSERT(mIOService); return mIOService; }
    nsresult EnsureIOService() {
        if (mIOService)
            return NS_OK;
        nsresult rv;
        mIOService = do_GetIOService(&rv);
        return rv;
    }

    nsIURI*  URI() { MOZ_ASSERT(mURI); return mURI; }
    nsresult EnsureURI() {
        BEGIN_ENSURE(URI, IOService);
        return mIOService->NewURI(mLocation, nullptr, nullptr, getter_AddRefs(mURI));
    }

    nsIChannel* ScriptChannel() { MOZ_ASSERT(mScriptChannel); return mScriptChannel; }
    nsresult EnsureScriptChannel() {
        BEGIN_ENSURE(ScriptChannel, IOService, URI);
        return NS_NewChannel(getter_AddRefs(mScriptChannel),
                             mURI,
                             nsContentUtils::GetSystemPrincipal(),
                             nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL,
                             nsIContentPolicy::TYPE_SCRIPT,
                             nullptr, // aPerformanceStorage
                             nullptr, // aLoadGroup
                             nullptr, // aCallbacks
                             nsIRequest::LOAD_NORMAL,
                             mIOService);
    }

    nsIURI* ResolvedURI() { MOZ_ASSERT(mResolvedURI); return mResolvedURI; }
    nsresult EnsureResolvedURI() {
        BEGIN_ENSURE(ResolvedURI, URI);
        return ResolveURI(mURI, getter_AddRefs(mResolvedURI));
    }

    const nsACString& Key() { return mLocation; }
    nsresult EnsureKey() {
        return NS_OK;
    }

    MOZ_MUST_USE nsresult GetLocation(nsCString& aLocation) {
        nsresult rv = EnsureURI();
        NS_ENSURE_SUCCESS(rv, rv);
        return mURI->GetSpec(aLocation);
    }

  private:
    const nsACString& mLocation;
    nsCOMPtr<nsIIOService> mIOService;
    nsCOMPtr<nsIURI> mURI;
    nsCOMPtr<nsIChannel> mScriptChannel;
    nsCOMPtr<nsIURI> mResolvedURI;
};

template <typename ...Args>
static nsresult
ReportOnCallerUTF8(JSCLContextHelper& helper,
                   const char* format,
                   ComponentLoaderInfo& info,
                   Args... args)
{
    nsCString location;
    MOZ_TRY(info.GetLocation(location));

    UniqueChars buf = JS_smprintf(format, location.get(), args...);
    if (!buf) {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    helper.reportErrorAfterPop(std::move(buf));
    return NS_ERROR_FAILURE;
}

#undef BEGIN_ENSURE
#undef ENSURE_DEPS
#undef ENSURE_DEP

mozJSComponentLoader::~mozJSComponentLoader()
{
    if (mInitialized) {
        NS_ERROR("'xpcom-shutdown-loaders' was not fired before cleaning up mozJSComponentLoader");
        UnloadModules();
    }

    sSelf = nullptr;
}

mozJSComponentLoader*
mozJSComponentLoader::sSelf;

NS_IMPL_ISUPPORTS(mozJSComponentLoader,
                  mozilla::ModuleLoader,
                  xpcIJSModuleLoader,
                  nsIObserver)

nsresult
mozJSComponentLoader::ReallyInit()
{
    MOZ_ASSERT(!mInitialized);

    const char* shareGlobal = PR_GetEnv("MOZ_LOADER_SHARE_GLOBAL");
    if (shareGlobal && *shareGlobal) {
        nsDependentCString val(shareGlobal);
        mShareLoaderGlobal = !(val.EqualsLiteral("0") ||
                               val.LowerCaseEqualsLiteral("no") ||
                               val.LowerCaseEqualsLiteral("false") ||
                               val.LowerCaseEqualsLiteral("off"));
    } else {
        mShareLoaderGlobal = Preferences::GetBool("jsloader.shareGlobal");
    }

    nsresult rv;
    nsCOMPtr<nsIObserverService> obsSvc =
        do_GetService(kObserverServiceContractID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = obsSvc->AddObserver(this, "xpcom-shutdown-loaders", false);
    NS_ENSURE_SUCCESS(rv, rv);

    mInitialized = true;

    return NS_OK;
}

// For terrible compatibility reasons, we need to consider both the global
// lexical environment and the global of modules when searching for exported
// symbols.
static JSObject*
ResolveModuleObjectProperty(JSContext* aCx, HandleObject aModObj, const char* name)
{
    if (JS_HasExtensibleLexicalEnvironment(aModObj)) {
        RootedObject lexical(aCx, JS_ExtensibleLexicalEnvironment(aModObj));
        bool found;
        if (!JS_HasOwnProperty(aCx, lexical, name, &found)) {
            return nullptr;
        }
        if (found) {
            return lexical;
        }
    }
    return aModObj;
}

#ifdef MOZ_CRASHREPORTER
static mozilla::Result<nsCString, nsresult> ReadScript(ComponentLoaderInfo& aInfo);

static nsresult
AnnotateScriptContents(const nsACString& aName, const nsACString& aURI)
{
    ComponentLoaderInfo info(aURI);

    nsCString str;
    MOZ_TRY_VAR(str, ReadScript(info));

    // The crash reporter won't accept any strings with embedded nuls. We
    // shouldn't have any here, but if we do because of data corruption, we
    // still want the annotation. So replace any embedded nuls before
    // annotating.
    str.ReplaceSubstring(NS_LITERAL_CSTRING("\0"), NS_LITERAL_CSTRING("\\0"));

    CrashReporter::AnnotateCrashReport(aName, str);

    return NS_OK;
}
#endif // defined MOZ_CRASHREPORTER

nsresult
mozJSComponentLoader::AnnotateCrashReport()
{
#ifdef MOZ_CRASHREPORTER
    Unused << AnnotateScriptContents(
        NS_LITERAL_CSTRING("nsAsyncShutdownComponent"),
        NS_LITERAL_CSTRING("resource://gre/components/nsAsyncShutdown.js"));

    Unused << AnnotateScriptContents(
        NS_LITERAL_CSTRING("AsyncShutdownModule"),
        NS_LITERAL_CSTRING("resource://gre/modules/AsyncShutdown.jsm"));
#endif // defined MOZ_CRASHREPORTER

    return NS_OK;
}

const mozilla::Module*
mozJSComponentLoader::LoadModule(FileLocation& aFile)
{
    if (!NS_IsMainThread()) {
        MOZ_ASSERT(false, "Don't use JS components off the main thread");
        return nullptr;
    }

    nsCOMPtr<nsIFile> file = aFile.GetBaseFile();

    nsCString spec;
    aFile.GetURIString(spec);
    ComponentLoaderInfo info(spec);
    nsresult rv = info.EnsureURI();
    NS_ENSURE_SUCCESS(rv, nullptr);

    if (!mInitialized) {
        rv = ReallyInit();
        if (NS_FAILED(rv))
            return nullptr;
    }

    AUTO_PROFILER_LABEL_DYNAMIC_NSCSTRING(
      "mozJSComponentLoader::LoadModule", OTHER, spec);

    ModuleEntry* mod;
    if (mModules.Get(spec, &mod))
        return mod;

    dom::AutoJSAPI jsapi;
    jsapi.Init();
    JSContext* cx = jsapi.cx();

    bool isCriticalModule = StringEndsWith(spec, NS_LITERAL_CSTRING("/nsAsyncShutdown.js"));

    nsAutoPtr<ModuleEntry> entry(new ModuleEntry(RootingContext::get(cx)));
    RootedValue exn(cx);
    rv = ObjectForLocation(info, file, &entry->obj, &entry->thisObjectKey,
                           &entry->location, isCriticalModule, &exn);
    if (NS_FAILED(rv)) {
        // Temporary debugging assertion for bug 1403348:
        if (isCriticalModule && !exn.isUndefined()) {
            AnnotateCrashReport();

            JSAutoRealm ar(cx, xpc::PrivilegedJunkScope());
            JS_WrapValue(cx, &exn);

            nsAutoCString file;
            uint32_t line;
            uint32_t column;
            nsAutoString msg;
            nsContentUtils::ExtractErrorValues(cx, exn, file, &line, &column, msg);

            NS_ConvertUTF16toUTF8 cMsg(msg);
            MOZ_CRASH_UNSAFE_PRINTF("Failed to load module \"%s\": "
                                    "[\"%s\" {file: \"%s\", line: %u}]",
                                    spec.get(), cMsg.get(), file.get(), line);
        }
        return nullptr;
    }

    nsCOMPtr<nsIComponentManager> cm;
    rv = NS_GetComponentManager(getter_AddRefs(cm));
    if (NS_FAILED(rv))
        return nullptr;

    JSAutoRealm ar(cx, entry->obj);
    RootedObject entryObj(cx, entry->obj);

    RootedObject NSGetFactoryHolder(cx, ResolveModuleObjectProperty(cx, entryObj, "NSGetFactory"));
    RootedValue NSGetFactory_val(cx);
    if (!NSGetFactoryHolder ||
        !JS_GetProperty(cx, NSGetFactoryHolder, "NSGetFactory", &NSGetFactory_val) ||
        NSGetFactory_val.isUndefined())
    {
        return nullptr;
    }

    if (JS_TypeOfValue(cx, NSGetFactory_val) != JSTYPE_FUNCTION) {
        /*
         * spec's encoding is ASCII unless it's zip file, otherwise it's
         * random encoding.  Latin1 variant is safe for random encoding.
         */
        JS_ReportErrorLatin1(cx, "%s has NSGetFactory property that is not a function",
                             spec.get());
        return nullptr;
    }

    RootedObject jsGetFactoryObj(cx);
    if (!JS_ValueToObject(cx, NSGetFactory_val, &jsGetFactoryObj) ||
        !jsGetFactoryObj) {
        /* XXX report error properly */
        return nullptr;
    }

    rv = nsXPConnect::XPConnect()->WrapJS(cx, jsGetFactoryObj,
                                          NS_GET_IID(xpcIJSGetFactory),
                                          getter_AddRefs(entry->getfactoryobj));
    if (NS_FAILED(rv)) {
        /* XXX report error properly */
#ifdef DEBUG
        fprintf(stderr, "mJCL: couldn't get nsIModule from jsval\n");
#endif
        return nullptr;
    }

#if defined(NIGHTLY_BUILD) || defined(DEBUG)
    if (Preferences::GetBool("browser.startup.record", false)) {
        entry->importStack = xpc_PrintJSStack(cx, false, false, false).get();
    }
#endif

    // Cache this module for later
    mModules.Put(spec, entry);

    // The hash owns the ModuleEntry now, forget about it
    return entry.forget();
}

void
mozJSComponentLoader::FindTargetObject(JSContext* aCx,
                                       MutableHandleObject aTargetObject)
{
    aTargetObject.set(js::GetJSMEnvironmentOfScriptedCaller(aCx));

    // The above could fail if the scripted caller is not a component/JSM (it
    // could be a DOM scope, for instance).
    //
    // If the target object was not in the JSM shared global, return the global
    // instead. This is needed when calling the subscript loader within a frame
    // script, since it the FrameScript NSVO will have been found.
    if (!aTargetObject ||
        !IsLoaderGlobal(JS::GetNonCCWObjectGlobal(aTargetObject))) {
        aTargetObject.set(CurrentGlobalOrNull(aCx));
    }
}

// This requires that the keys be strings and the values be pointers.
template <class Key, class Data, class UserData>
static size_t
SizeOfTableExcludingThis(const nsBaseHashtable<Key, Data, UserData>& aTable,
                         MallocSizeOf aMallocSizeOf)
{
    size_t n = aTable.ShallowSizeOfExcludingThis(aMallocSizeOf);
    for (auto iter = aTable.ConstIter(); !iter.Done(); iter.Next()) {
        n += iter.Key().SizeOfExcludingThisIfUnshared(aMallocSizeOf);
        n += iter.Data()->SizeOfIncludingThis(aMallocSizeOf);
    }
    return n;
}

size_t
mozJSComponentLoader::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf)
{
    size_t n = aMallocSizeOf(this);
    n += SizeOfTableExcludingThis(mModules, aMallocSizeOf);
    n += SizeOfTableExcludingThis(mImports, aMallocSizeOf);
    n += mLocations.ShallowSizeOfExcludingThis(aMallocSizeOf);
    n += SizeOfTableExcludingThis(mInProgressImports, aMallocSizeOf);
    return n;
}

void
mozJSComponentLoader::CreateLoaderGlobal(JSContext* aCx,
                                         const nsACString& aLocation,
                                         MutableHandleObject aGlobal)
{
    RefPtr<BackstagePass> backstagePass;
    nsresult rv = NS_NewBackstagePass(getter_AddRefs(backstagePass));
    NS_ENSURE_SUCCESS_VOID(rv);

    RealmOptions options;

    options.creationOptions()
           .setNewCompartmentInSystemZone();

    if (xpc::SharedMemoryEnabled())
        options.creationOptions().setSharedMemoryAndAtomicsEnabled(true);

    // Defer firing OnNewGlobalObject until after the __URI__ property has
    // been defined so the JS debugger can tell what module the global is
    // for
    RootedObject global(aCx);
    rv = xpc::InitClassesWithNewWrappedGlobal(aCx,
                                              static_cast<nsIGlobalObject*>(backstagePass),
                                              nsContentUtils::GetSystemPrincipal(),
                                              xpc::DONT_FIRE_ONNEWGLOBALHOOK,
                                              options,
                                              &global);
    NS_ENSURE_SUCCESS_VOID(rv);

    NS_ENSURE_TRUE_VOID(global);

    backstagePass->SetGlobalObject(global);

    JSAutoRealm ar(aCx, global);
    if (!JS_DefineFunctions(aCx, global, gGlobalFun) ||
        !JS_DefineProfilingFunctions(aCx, global)) {
        return;
    }

    // Set the location information for the new global, so that tools like
    // about:memory may use that information
    xpc::SetLocationForGlobal(global, aLocation);

    aGlobal.set(global);
}

bool
mozJSComponentLoader::ReuseGlobal(nsIURI* aURI)
{
    if (!mShareLoaderGlobal)
        return false;

    nsCString spec;
    NS_ENSURE_SUCCESS(aURI->GetSpec(spec), false);

    // The loader calls Object.freeze on global properties, which
    // causes problems if the global is shared with other code.
    if (spec.EqualsASCII("resource://gre/modules/commonjs/toolkit/loader.js")) {
        return false;
    }

    // Various tests call addDebuggerToGlobal on the result of
    // importing this JSM, which would be annoying to fix.
    if (spec.EqualsASCII("resource://gre/modules/jsdebugger.jsm")) {
        return false;
    }

    // Some SpecialPowers jsms call Cu.forcePermissiveCOWs(),
    // which sets a per-compartment flag that disables certain
    // security wrappers, so don't use the shared global for them
    // to avoid breaking tests.
    if (FindInReadable(NS_LITERAL_CSTRING("resource://specialpowers/"), spec)) {
        return false;
    }

    return true;
}

JSObject*
mozJSComponentLoader::GetSharedGlobal(JSContext* aCx)
{
    if (!mLoaderGlobal) {
        JS::RootedObject globalObj(aCx);
        CreateLoaderGlobal(aCx, NS_LITERAL_CSTRING("shared JSM global"),
                           &globalObj);

        // If we fail to create a module global this early, we're not going to
        // get very far, so just bail out now.
        MOZ_RELEASE_ASSERT(globalObj);
        mLoaderGlobal = globalObj;

        // AutoEntryScript required to invoke debugger hook, which is a
        // Gecko-specific concept at present.
        dom::AutoEntryScript aes(globalObj,
                                 "component loader report global");
        JS_FireOnNewGlobalObject(aes.cx(), globalObj);
    }

    return mLoaderGlobal;
}

JSObject*
mozJSComponentLoader::PrepareObjectForLocation(JSContext* aCx,
                                               nsIFile* aComponentFile,
                                               nsIURI* aURI,
                                               bool* aReuseGlobal,
                                               bool* aRealFile)
{
    nsAutoCString nativePath;
    NS_ENSURE_SUCCESS(aURI->GetSpec(nativePath), nullptr);

    bool reuseGlobal = ReuseGlobal(aURI);

    *aReuseGlobal = reuseGlobal;

    bool createdNewGlobal = false;
    RootedObject globalObj(aCx);
    if (reuseGlobal) {
        globalObj = GetSharedGlobal(aCx);
    } else if (!globalObj) {
        CreateLoaderGlobal(aCx, nativePath, &globalObj);
        createdNewGlobal = true;
    }

    // |thisObj| is the object we set properties on for a particular .jsm.
    RootedObject thisObj(aCx, globalObj);
    NS_ENSURE_TRUE(thisObj, nullptr);

    JSAutoRealm ar(aCx, thisObj);

    if (reuseGlobal) {
        thisObj = js::NewJSMEnvironment(aCx);
        NS_ENSURE_TRUE(thisObj, nullptr);
    }

    *aRealFile = false;

    // need to be extra careful checking for URIs pointing to files
    // EnsureFile may not always get called, especially on resource URIs
    // so we need to call GetFile to make sure this is a valid file
    nsresult rv = NS_OK;
    nsCOMPtr<nsIFileURL> fileURL = do_QueryInterface(aURI, &rv);
    nsCOMPtr<nsIFile> testFile;
    if (NS_SUCCEEDED(rv)) {
        fileURL->GetFile(getter_AddRefs(testFile));
    }

    if (testFile) {
        *aRealFile = true;

        if (XRE_IsParentProcess()) {
            RootedObject locationObj(aCx);

            rv = nsXPConnect::XPConnect()->WrapNative(aCx, thisObj, aComponentFile,
                                                      NS_GET_IID(nsIFile),
                                                      locationObj.address());
            NS_ENSURE_SUCCESS(rv, nullptr);
            NS_ENSURE_TRUE(locationObj, nullptr);

            if (!JS_DefineProperty(aCx, thisObj, "__LOCATION__", locationObj, 0))
                return nullptr;
        }
    }

    // Expose the URI from which the script was imported through a special
    // variable that we insert into the JSM.
    RootedString exposedUri(aCx, JS_NewStringCopyN(aCx, nativePath.get(), nativePath.Length()));
    NS_ENSURE_TRUE(exposedUri, nullptr);

    if (!JS_DefineProperty(aCx, thisObj, "__URI__", exposedUri, 0))
        return nullptr;

    if (createdNewGlobal) {
        // AutoEntryScript required to invoke debugger hook, which is a
        // Gecko-specific concept at present.
        dom::AutoEntryScript aes(globalObj,
                                 "component loader report global");
        JS_FireOnNewGlobalObject(aes.cx(), globalObj);
    }

    return thisObj;
}

static mozilla::Result<nsCString, nsresult>
ReadScript(ComponentLoaderInfo& aInfo)
{
    MOZ_TRY(aInfo.EnsureScriptChannel());

    nsCOMPtr<nsIInputStream> scriptStream;
    MOZ_TRY(NS_MaybeOpenChannelUsingOpen2(aInfo.ScriptChannel(),
                                          getter_AddRefs(scriptStream)));

    uint64_t len64;
    uint32_t bytesRead;

    MOZ_TRY(scriptStream->Available(&len64));
    NS_ENSURE_TRUE(len64 < UINT32_MAX, Err(NS_ERROR_FILE_TOO_BIG));
    NS_ENSURE_TRUE(len64, Err(NS_ERROR_FAILURE));
    uint32_t len = (uint32_t)len64;

    /* malloc an internal buf the size of the file */
    nsCString str;
    if (!str.SetLength(len, fallible))
        return Err(NS_ERROR_OUT_OF_MEMORY);

    /* read the file in one swoop */
    MOZ_TRY(scriptStream->Read(str.BeginWriting(), len, &bytesRead));
    if (bytesRead != len)
        return Err(NS_BASE_STREAM_OSERROR);

    return std::move(str);
}

nsresult
mozJSComponentLoader::ObjectForLocation(ComponentLoaderInfo& aInfo,
                                        nsIFile* aComponentFile,
                                        MutableHandleObject aObject,
                                        MutableHandleScript aTableScript,
                                        char** aLocation,
                                        bool aPropagateExceptions,
                                        MutableHandleValue aException)
{
    MOZ_ASSERT(NS_IsMainThread(), "Must be on main thread.");

    dom::AutoJSAPI jsapi;
    jsapi.Init();
    JSContext* cx = jsapi.cx();

    bool realFile = false;
    nsresult rv = aInfo.EnsureURI();
    NS_ENSURE_SUCCESS(rv, rv);
    bool reuseGlobal = false;
    RootedObject obj(cx, PrepareObjectForLocation(cx, aComponentFile, aInfo.URI(),
                                                  &reuseGlobal, &realFile));
    NS_ENSURE_TRUE(obj, NS_ERROR_FAILURE);
    MOZ_ASSERT(JS_IsGlobalObject(obj) == !reuseGlobal);

    JSAutoRealm ar(cx, obj);

    RootedScript script(cx);

    nsAutoCString nativePath;
    rv = aInfo.URI()->GetSpec(nativePath);
    NS_ENSURE_SUCCESS(rv, rv);

    // Before compiling the script, first check to see if we have it in
    // the startupcache.  Note: as a rule, startupcache errors are not fatal
    // to loading the script, since we can always slow-load.

    bool writeToCache = false;
    StartupCache* cache = StartupCache::GetSingleton();

    aInfo.EnsureResolvedURI();

    nsAutoCString cachePath(reuseGlobal ? JS_CACHE_PREFIX("non-syntactic")
                                        : JS_CACHE_PREFIX("global"));
    rv = PathifyURI(aInfo.ResolvedURI(), cachePath);
    NS_ENSURE_SUCCESS(rv, rv);

    script = ScriptPreloader::GetSingleton().GetCachedScript(cx, cachePath);
    if (!script && cache) {
        ReadCachedScript(cache, cachePath, cx, &script);
    }

    if (script) {
        LOG(("Successfully loaded %s from startupcache\n", nativePath.get()));
    } else if (cache) {
        // This is ok, it just means the script is not yet in the
        // cache. Could mean that the cache was corrupted and got removed,
        // but either way we're going to write this out.
        writeToCache = true;
        // ReadCachedScript may have set a pending exception.
        JS_ClearPendingException(cx);
    }

    if (!script) {
        // The script wasn't in the cache , so compile it now.
        LOG(("Slow loading %s\n", nativePath.get()));

        // Use lazy source if we're using the startup cache. Non-lazy source +
        // startup cache regresses installer size (due to source code stored in
        // XDR encoded modules in omni.ja). Also, XDR decoding is relatively
        // fast. When we're not using the startup cache, we want to use non-lazy
        // source code so that we can use lazy parsing.
        // See bug 1303754.
        CompileOptions options(cx);
        options.setNoScriptRval(true)
               .maybeMakeStrictMode(true)
               .setFileAndLine(nativePath.get(), 1)
               .setSourceIsLazy(cache || ScriptPreloader::GetSingleton().Active());

        if (realFile) {
            AutoMemMap map;
            MOZ_TRY(map.init(aComponentFile));

            // Note: exceptions will get handled further down;
            // don't early return for them here.
            auto buf = map.get<char>();
            if (reuseGlobal)
                CompileForNonSyntacticScope(cx, options, buf.get(), map.size(), &script);
            else
                Compile(cx, options, buf.get(), map.size(), &script);
        } else {
            nsCString str;
            MOZ_TRY_VAR(str, ReadScript(aInfo));

            if (reuseGlobal)
                CompileForNonSyntacticScope(cx, options, str.get(), str.Length(), &script);
            else
                Compile(cx, options, str.get(), str.Length(), &script);
        }
        // Propagate the exception, if one exists. Also, don't leave the stale
        // exception on this context.
        if (!script && aPropagateExceptions && jsapi.HasException()) {
            if (!jsapi.StealException(aException))
                return NS_ERROR_OUT_OF_MEMORY;
        }
    }

    if (!script) {
        return NS_ERROR_FAILURE;
    }

    ScriptPreloader::GetSingleton().NoteScript(nativePath, cachePath, script);

    if (writeToCache) {
        // We successfully compiled the script, so cache it.
        rv = WriteCachedScript(cache, cachePath, cx, script);

        // Don't treat failure to write as fatal, since we might be working
        // with a read-only cache.
        if (NS_SUCCEEDED(rv)) {
            LOG(("Successfully wrote to cache\n"));
        } else {
            LOG(("Failed to write to cache\n"));
        }
    }

    // Assign aObject here so that it's available to recursive imports.
    // See bug 384168.
    aObject.set(obj);

    aTableScript.set(script);


    {   // Scope for AutoEntryScript

        // We're going to run script via JS_ExecuteScript, so we need an
        // AutoEntryScript. This is Gecko-specific and not in any spec.
        dom::AutoEntryScript aes(CurrentGlobalOrNull(cx),
                                 "component loader load module");
        JSContext* aescx = aes.cx();

        bool executeOk = false;
        if (JS_IsGlobalObject(obj)) {
            JS::RootedValue rval(cx);
            executeOk = JS::CloneAndExecuteScript(aescx, script, &rval);
        } else {
            executeOk = js::ExecuteInJSMEnvironment(aescx, script, obj);
        }

        if (!executeOk) {
            if (aPropagateExceptions && aes.HasException()) {
                // Ignore return value because we're returning an error code
                // anyway.
                Unused << aes.StealException(aException);
            }
            aObject.set(nullptr);
            aTableScript.set(nullptr);
            return NS_ERROR_FAILURE;
        }
    }

    /* Freed when we remove from the table. */
    *aLocation = ToNewCString(nativePath);
    if (!*aLocation) {
        aObject.set(nullptr);
        aTableScript.set(nullptr);
        return NS_ERROR_OUT_OF_MEMORY;
    }

    return NS_OK;
}

void
mozJSComponentLoader::UnloadModules()
{
    mInitialized = false;

    if (mLoaderGlobal) {
        dom::AutoJSAPI jsapi;
        jsapi.Init();
        JSContext* cx = jsapi.cx();
        RootedObject global(cx, mLoaderGlobal);
        JSAutoRealm ar(cx, global);
        MOZ_ASSERT(JS_HasExtensibleLexicalEnvironment(global));
        JS_SetAllNonReservedSlotsToUndefined(cx, JS_ExtensibleLexicalEnvironment(global));
        JS_SetAllNonReservedSlotsToUndefined(cx, global);
        mLoaderGlobal = nullptr;
    }

    mInProgressImports.Clear();
    mImports.Clear();
    mLocations.Clear();

    for (auto iter = mModules.Iter(); !iter.Done(); iter.Next()) {
        iter.Data()->Clear();
        iter.Remove();
    }
}

nsresult
mozJSComponentLoader::ImportInto(const nsACString& registryLocation,
                                 HandleValue targetValArg,
                                 JSContext* cx,
                                 uint8_t optionalArgc,
                                 MutableHandleValue retval)
{
    MOZ_ASSERT(nsContentUtils::IsCallerChrome());

    RootedValue targetVal(cx, targetValArg);
    RootedObject targetObject(cx, nullptr);
    if (optionalArgc) {
        // The caller passed in the optional second argument. Get it.
        if (targetVal.isObject()) {
            // If we're passing in something like a content DOM window, chances
            // are the caller expects the properties to end up on the object
            // proper and not on the Xray holder. This is dubious, but can be used
            // during testing. Given that dumb callers can already leak JSMs into
            // content by passing a raw content JS object (where Xrays aren't
            // possible), we aim for consistency here. Waive xray.
            if (WrapperFactory::IsXrayWrapper(&targetVal.toObject()) &&
                !WrapperFactory::WaiveXrayAndWrap(cx, &targetVal))
            {
                return NS_ERROR_FAILURE;
            }
            targetObject = &targetVal.toObject();
        } else if (!targetVal.isNull()) {
            // If targetVal isNull(), we actually want to leave targetObject null.
            // Not doing so breaks |make package|.
            return ReportOnCallerUTF8(cx, ERROR_SCOPE_OBJ,
                                      PromiseFlatCString(registryLocation).get());
        }
    } else {
        FindTargetObject(cx, &targetObject);
    }

    Maybe<JSAutoRealm> ar;
    if (targetObject) {
        ar.emplace(cx, targetObject);
    }

    RootedObject global(cx);
    nsresult rv = ImportInto(registryLocation, targetObject, cx, &global);

    if (global) {
        if (!JS_WrapObject(cx, &global)) {
            NS_ERROR("can't wrap return value");
            return NS_ERROR_FAILURE;
        }

        retval.setObject(*global);
    }
    return rv;
}

nsresult
mozJSComponentLoader::IsModuleLoaded(const nsACString& aLocation,
                                     bool* retval)
{
    MOZ_ASSERT(nsContentUtils::IsCallerChrome());

    nsresult rv;
    if (!mInitialized) {
        rv = ReallyInit();
        NS_ENSURE_SUCCESS(rv, rv);
    }

    ComponentLoaderInfo info(aLocation);
    rv = info.EnsureKey();
    NS_ENSURE_SUCCESS(rv, rv);

    *retval = !!mImports.Get(info.Key());
    return NS_OK;
}

NS_IMETHODIMP mozJSComponentLoader::LoadedModules(uint32_t* length,
                                                  char*** aModules)
{
    char** modules = new char*[mImports.Count()];
    *length = mImports.Count();
    *aModules = modules;

    for (auto iter = mImports.Iter(); !iter.Done(); iter.Next()) {
        *modules = NS_strdup(iter.Data()->location);
        modules++;
    }

    return NS_OK;
}

NS_IMETHODIMP mozJSComponentLoader::LoadedComponents(uint32_t* length,
                                                     char*** aComponents)
{
    char** comp = new char*[mModules.Count()];
    *length = mModules.Count();
    *aComponents = comp;

    for (auto iter = mModules.Iter(); !iter.Done(); iter.Next()) {
        *comp = NS_strdup(iter.Data()->location);
        comp++;
    }

    return NS_OK;
}

NS_IMETHODIMP
mozJSComponentLoader::GetModuleImportStack(const nsACString& aLocation,
                                           nsACString& retval)
{
#ifdef STARTUP_RECORDER_ENABLED
    MOZ_ASSERT(nsContentUtils::IsCallerChrome());
    MOZ_ASSERT(mInitialized);

    ComponentLoaderInfo info(aLocation);
    nsresult rv = info.EnsureKey();
    NS_ENSURE_SUCCESS(rv, rv);

    ModuleEntry* mod;
    if (!mImports.Get(info.Key(), &mod))
        return NS_ERROR_FAILURE;

    retval = mod->importStack;
    return NS_OK;
#else
    return NS_ERROR_NOT_IMPLEMENTED;
#endif
}

NS_IMETHODIMP
mozJSComponentLoader::GetComponentLoadStack(const nsACString& aLocation,
                                            nsACString& retval)
{
#ifdef STARTUP_RECORDER_ENABLED
    MOZ_ASSERT(nsContentUtils::IsCallerChrome());
    MOZ_ASSERT(mInitialized);

    ComponentLoaderInfo info(aLocation);
    nsresult rv = info.EnsureURI();
    NS_ENSURE_SUCCESS(rv, rv);

    ModuleEntry* mod;
    if (!mModules.Get(info.Key(), &mod))
        return NS_ERROR_FAILURE;

    retval = mod->importStack;
    return NS_OK;
#else
    return NS_ERROR_NOT_IMPLEMENTED;
#endif
}

static JSObject*
ResolveModuleObjectPropertyById(JSContext* aCx, HandleObject aModObj, HandleId id)
{
    if (JS_HasExtensibleLexicalEnvironment(aModObj)) {
        RootedObject lexical(aCx, JS_ExtensibleLexicalEnvironment(aModObj));
        bool found;
        if (!JS_HasOwnPropertyById(aCx, lexical, id, &found)) {
            return nullptr;
        }
        if (found) {
            return lexical;
        }
    }
    return aModObj;
}

nsresult
mozJSComponentLoader::ImportInto(const nsACString& aLocation,
                                 HandleObject targetObj,
                                 JSContext* cx,
                                 MutableHandleObject vp)
{
    vp.set(nullptr);

    JS::RootedObject exports(cx);
    MOZ_TRY(Import(cx, aLocation, vp, &exports, !targetObj));

    if (targetObj) {
        JS::Rooted<JS::IdVector> ids(cx, JS::IdVector(cx));
        if (!JS_Enumerate(cx, exports, &ids)) {
            return NS_ERROR_OUT_OF_MEMORY;
        }

        JS::RootedValue value(cx);
        JS::RootedId id(cx);
        for (jsid idVal : ids) {
            id = idVal;
            if (!JS_GetPropertyById(cx, exports, id, &value) ||
                !JS_SetPropertyById(cx, targetObj, id, value)) {
                return NS_ERROR_FAILURE;
            }
        }
    }

    return NS_OK;
}

nsresult
mozJSComponentLoader::ExtractExports(JSContext* aCx, ComponentLoaderInfo& aInfo,
                                     ModuleEntry* aMod,
                                     JS::MutableHandleObject aExports)
{
    // cxhelper must be created before jsapi, so that jsapi is destroyed and
    // pops any context it has pushed before we report to the caller context.
    JSCLContextHelper cxhelper(aCx);

    // Even though we are calling JS_SetPropertyById on targetObj, we want
    // to ensure that we never run script here, so we use an AutoJSAPI and
    // not an AutoEntryScript.
    dom::AutoJSAPI jsapi;
    jsapi.Init();
    JSContext* cx = jsapi.cx();
    JSAutoRealm ar(cx, aMod->obj);

    RootedValue symbols(cx);
    {
        RootedObject obj(cx, ResolveModuleObjectProperty(cx, aMod->obj,
                                                         "EXPORTED_SYMBOLS"));
        if (!obj || !JS_GetProperty(cx, obj, "EXPORTED_SYMBOLS", &symbols)) {
            return ReportOnCallerUTF8(cxhelper, ERROR_NOT_PRESENT, aInfo);
        }
    }

    bool isArray;
    if (!JS_IsArrayObject(cx, symbols, &isArray)) {
        return NS_ERROR_FAILURE;
    }
    if (!isArray) {
        return ReportOnCallerUTF8(cxhelper, ERROR_NOT_AN_ARRAY, aInfo);
    }

    RootedObject symbolsObj(cx, &symbols.toObject());

    // Iterate over symbols array, installing symbols on targetObj:

    uint32_t symbolCount = 0;
    if (!JS_GetArrayLength(cx, symbolsObj, &symbolCount)) {
        return ReportOnCallerUTF8(cxhelper, ERROR_GETTING_ARRAY_LENGTH,
                                  aInfo);
    }

#ifdef DEBUG
    nsAutoCString logBuffer;
#endif

    aExports.set(JS_NewPlainObject(cx));
    if (!aExports) {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    bool missing = false;

    RootedValue value(cx);
    RootedId symbolId(cx);
    RootedObject symbolHolder(cx);
    for (uint32_t i = 0; i < symbolCount; ++i) {
        if (!JS_GetElement(cx, symbolsObj, i, &value) ||
            !value.isString() ||
            !JS_ValueToId(cx, value, &symbolId)) {
            return ReportOnCallerUTF8(cxhelper, ERROR_ARRAY_ELEMENT, aInfo, i);
        }

        symbolHolder = ResolveModuleObjectPropertyById(cx, aMod->obj, symbolId);
        if (!symbolHolder ||
            !JS_GetPropertyById(cx, symbolHolder, symbolId, &value)) {
            JSAutoByteString bytes;
            RootedString symbolStr(cx, JSID_TO_STRING(symbolId));
            if (!bytes.encodeUtf8(cx, symbolStr))
                return NS_ERROR_FAILURE;
            return ReportOnCallerUTF8(cxhelper, ERROR_GETTING_SYMBOL,
                                      aInfo, bytes.ptr());
        }

        if (value.isUndefined()) {
            missing = true;
        }

        if (!JS_SetPropertyById(cx, aExports, symbolId, value)) {
            JSAutoByteString bytes;
            RootedString symbolStr(cx, JSID_TO_STRING(symbolId));
            if (!bytes.encodeUtf8(cx, symbolStr))
                return NS_ERROR_FAILURE;
            return ReportOnCallerUTF8(cxhelper, ERROR_GETTING_SYMBOL,
                                      aInfo, bytes.ptr());
        }
#ifdef DEBUG
        if (i == 0) {
            logBuffer.AssignLiteral("Installing symbols [ ");
        }
        JSAutoByteString bytes(cx, JSID_TO_STRING(symbolId));
        if (!!bytes)
            logBuffer.Append(bytes.ptr());
        logBuffer.Append(' ');
        if (i == symbolCount - 1) {
            nsCString location;
            MOZ_TRY(aInfo.GetLocation(location));
            LOG(("%s] from %s\n", logBuffer.get(), location.get()));
        }
#endif
    }

    // Don't cache the exports object if any of its exported symbols are
    // missing. If the module hasn't finished loading yet, they may be
    // defined the next time we try to import it.
    if (!missing) {
        aMod->exports = aExports;
    }
    return NS_OK;
}

nsresult
mozJSComponentLoader::Import(JSContext* aCx, const nsACString& aLocation,
                             JS::MutableHandleObject aModuleGlobal,
                             JS::MutableHandleObject aModuleExports,
                             bool aIgnoreExports)
{
    nsresult rv;
    if (!mInitialized) {
        rv = ReallyInit();
        NS_ENSURE_SUCCESS(rv, rv);
    }

    ComponentLoaderInfo info(aLocation);

    rv = info.EnsureKey();
    NS_ENSURE_SUCCESS(rv, rv);

    ModuleEntry* mod;
    nsAutoPtr<ModuleEntry> newEntry;
    if (!mImports.Get(info.Key(), &mod) && !mInProgressImports.Get(info.Key(), &mod)) {
        newEntry = new ModuleEntry(RootingContext::get(aCx));
        if (!newEntry)
            return NS_ERROR_OUT_OF_MEMORY;

        // Note: This implies EnsureURI().
        MOZ_TRY(info.EnsureResolvedURI());

        // get the JAR if there is one
        nsCOMPtr<nsIJARURI> jarURI;
        jarURI = do_QueryInterface(info.ResolvedURI(), &rv);
        nsCOMPtr<nsIFileURL> baseFileURL;
        if (NS_SUCCEEDED(rv)) {
            nsCOMPtr<nsIURI> baseURI;
            while (jarURI) {
                jarURI->GetJARFile(getter_AddRefs(baseURI));
                jarURI = do_QueryInterface(baseURI, &rv);
            }
            baseFileURL = do_QueryInterface(baseURI, &rv);
            NS_ENSURE_SUCCESS(rv, rv);
        } else {
            baseFileURL = do_QueryInterface(info.ResolvedURI(), &rv);
            NS_ENSURE_SUCCESS(rv, rv);
        }

        nsCOMPtr<nsIFile> sourceFile;
        rv = baseFileURL->GetFile(getter_AddRefs(sourceFile));
        NS_ENSURE_SUCCESS(rv, rv);

        rv = info.ResolvedURI()->GetSpec(newEntry->resolvedURL);
        NS_ENSURE_SUCCESS(rv, rv);

        nsCString* existingPath;
        if (mLocations.Get(newEntry->resolvedURL, &existingPath) && *existingPath != info.Key()) {
            return NS_ERROR_UNEXPECTED;
        }

        mLocations.Put(newEntry->resolvedURL, new nsCString(info.Key()));

        RootedValue exception(aCx);
        {
            mInProgressImports.Put(info.Key(), newEntry);
            auto cleanup = MakeScopeExit([&] () {
                mInProgressImports.Remove(info.Key());
            });

            rv = ObjectForLocation(info, sourceFile, &newEntry->obj,
                                   &newEntry->thisObjectKey,
                                   &newEntry->location, true, &exception);
        }

        if (NS_FAILED(rv)) {
            if (!exception.isUndefined()) {
                // An exception was thrown during compilation. Propagate it
                // out to our caller so they can report it.
                if (!JS_WrapValue(aCx, &exception))
                    return NS_ERROR_OUT_OF_MEMORY;
                JS_SetPendingException(aCx, exception);
                return NS_ERROR_FAILURE;
            }

            // Something failed, but we don't know what it is, guess.
            return NS_ERROR_FILE_NOT_FOUND;
        }

#ifdef STARTUP_RECORDER_ENABLED
        if (Preferences::GetBool("browser.startup.record", false)) {
            newEntry->importStack =
                xpc_PrintJSStack(aCx, false, false, false).get();
        }
#endif

        mod = newEntry;
    }

    MOZ_ASSERT(mod->obj, "Import table contains entry with no object");
    aModuleGlobal.set(mod->obj);

    JS::RootedObject exports(aCx, mod->exports);
    if (!exports && !aIgnoreExports) {
        MOZ_TRY(ExtractExports(aCx, info, mod, &exports));
    }

    if (exports && !JS_WrapObject(aCx, &exports)) {
        return NS_ERROR_FAILURE;
    }
    aModuleExports.set(exports);

    // Cache this module for later
    if (newEntry) {
        mImports.Put(info.Key(), newEntry);
        newEntry.forget();
    }

    return NS_OK;
}

nsresult
mozJSComponentLoader::Unload(const nsACString & aLocation)
{
    nsresult rv;

    if (!mInitialized) {
        return NS_OK;
    }

    ComponentLoaderInfo info(aLocation);
    rv = info.EnsureKey();
    NS_ENSURE_SUCCESS(rv, rv);
    ModuleEntry* mod;
    if (mImports.Get(info.Key(), &mod)) {
        mLocations.Remove(mod->resolvedURL);
        mImports.Remove(info.Key());
    }

    // If this is the last module to be unloaded, we will leak mLoaderGlobal
    // until UnloadModules is called. So be it.

    return NS_OK;
}

NS_IMETHODIMP
mozJSComponentLoader::Observe(nsISupports* subject, const char* topic,
                              const char16_t* data)
{
    if (!strcmp(topic, "xpcom-shutdown-loaders")) {
        UnloadModules();
    } else {
        NS_ERROR("Unexpected observer topic.");
    }

    return NS_OK;
}

size_t
mozJSComponentLoader::ModuleEntry::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
{
    size_t n = aMallocSizeOf(this);
    n += aMallocSizeOf(location);

    return n;
}

/* static */ already_AddRefed<nsIFactory>
mozJSComponentLoader::ModuleEntry::GetFactory(const mozilla::Module& module,
                                              const mozilla::Module::CIDEntry& entry)
{
    const ModuleEntry& self = static_cast<const ModuleEntry&>(module);
    MOZ_ASSERT(self.getfactoryobj, "Handing out an uninitialized module?");

    nsCOMPtr<nsIFactory> f;
    nsresult rv = self.getfactoryobj->Get(*entry.cid, getter_AddRefs(f));
    if (NS_FAILED(rv))
        return nullptr;

    return f.forget();
}

//----------------------------------------------------------------------

JSCLContextHelper::JSCLContextHelper(JSContext* aCx)
    : mContext(aCx)
    , mBuf(nullptr)
{
}

JSCLContextHelper::~JSCLContextHelper()
{
    if (mBuf) {
        JS_ReportErrorUTF8(mContext, "%s", mBuf.get());
    }
}

void
JSCLContextHelper::reportErrorAfterPop(UniqueChars&& buf)
{
    MOZ_ASSERT(!mBuf, "Already called reportErrorAfterPop");
    mBuf = std::move(buf);
}
