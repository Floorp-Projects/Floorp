/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScriptLoadRequest.h"
#include "mozilla/Attributes.h"
#include "mozilla/ArrayUtils.h"  // mozilla::ArrayLength
#include "mozilla/Utf8.h"        // mozilla::Utf8Unit

#include <cstdarg>

#include "mozilla/Logging.h"
#include "mozilla/dom/RequestBinding.h"
#ifdef ANDROID
#  include <android/log.h>
#endif
#ifdef XP_WIN
#  include <windows.h>
#endif

#include "jsapi.h"
#include "js/Array.h"  // JS::GetArrayLength, JS::IsArrayObject
#include "js/CharacterEncoding.h"
#include "js/CompilationAndEvaluation.h"
#include "js/CompileOptions.h"         // JS::CompileOptions
#include "js/ErrorReport.h"            // JS_ReportErrorUTF8, JSErrorReport
#include "js/Exception.h"              // JS_ErrorFromException
#include "js/friend/JSMEnvironment.h"  // JS::ExecuteInJSMEnvironment, JS::GetJSMEnvironmentOfScriptedCaller, JS::NewJSMEnvironment
#include "js/friend/ErrorMessages.h"   // JSMSG_*
#include "js/loader/ModuleLoadRequest.h"
#include "js/Object.h"  // JS::GetCompartment
#include "js/Printf.h"
#include "js/PropertyAndElement.h"  // JS_DefineFunctions, JS_DefineProperty, JS_Enumerate, JS_GetElement, JS_GetProperty, JS_GetPropertyById, JS_HasOwnProperty, JS_HasOwnPropertyById, JS_SetProperty, JS_SetPropertyById
#include "js/PropertySpec.h"
#include "js/SourceText.h"  // JS::SourceText
#include "nsCOMPtr.h"
#include "nsDirectoryServiceDefs.h"
#include "nsDirectoryServiceUtils.h"
#include "nsIFile.h"
#include "mozJSModuleLoader.h"
#include "mozJSLoaderUtils.h"
#include "nsIFileURL.h"
#include "nsIJARURI.h"
#include "nsIChannel.h"
#include "nsNetUtil.h"
#include "nsJSUtils.h"
#include "xpcprivate.h"
#include "xpcpublic.h"
#include "nsContentUtils.h"
#include "nsXULAppAPI.h"
#include "WrapperFactory.h"
#include "JSMEnvironmentProxy.h"
#include "ModuleEnvironmentProxy.h"
#include "JSServices.h"

#include "mozilla/scache/StartupCache.h"
#include "mozilla/scache/StartupCacheUtils.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/MacroForEach.h"
#include "mozilla/Preferences.h"
#include "mozilla/ProfilerLabels.h"
#include "mozilla/ProfilerMarkers.h"
#include "mozilla/ResultExtensions.h"
#include "mozilla/ScriptPreloader.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/Try.h"
#include "mozilla/dom/AutoEntryScript.h"
#include "mozilla/dom/ReferrerPolicyBinding.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/Unused.h"

using namespace mozilla;
using namespace mozilla::scache;
using namespace mozilla::loader;
using namespace xpc;
using namespace JS;

#define JS_CACHE_PREFIX(aScopeType, aCompilationTarget) \
  "jsloader/" aScopeType "/" aCompilationTarget

/**
 * Buffer sizes for serialization and deserialization of scripts.
 * FIXME: bug #411579 (tune this macro!) Last updated: Jan 2008
 */
#define XPC_SERIALIZATION_BUFFER_SIZE (64 * 1024)
#define XPC_DESERIALIZATION_BUFFER_SIZE (12 * 8192)

// MOZ_LOG=JSModuleLoader:5
static LazyLogModule gJSCLLog("JSModuleLoader");

#define LOG(args) MOZ_LOG(gJSCLLog, mozilla::LogLevel::Debug, args)

// Components.utils.import error messages
#define ERROR_SCOPE_OBJ "%s - Second argument must be an object."
#define ERROR_NO_TARGET_OBJECT "%s - Couldn't find target object for import."
#define ERROR_NOT_PRESENT "%s - EXPORTED_SYMBOLS is not present."
#define ERROR_NOT_AN_ARRAY "%s - EXPORTED_SYMBOLS is not an array."
#define ERROR_GETTING_ARRAY_LENGTH \
  "%s - Error getting array length of EXPORTED_SYMBOLS."
#define ERROR_ARRAY_ELEMENT "%s - EXPORTED_SYMBOLS[%d] is not a string."
#define ERROR_GETTING_SYMBOL "%s - Could not get symbol '%s'."
#define ERROR_SETTING_SYMBOL "%s - Could not set symbol '%s' on target object."
#define ERROR_UNINITIALIZED_SYMBOL \
  "%s - Symbol '%s' accessed before initialization. Cyclic import?"

static constexpr char JSM_Suffix[] = ".jsm";
static constexpr size_t JSM_SuffixLength = mozilla::ArrayLength(JSM_Suffix) - 1;
static constexpr char JSM_JS_Suffix[] = ".jsm.js";
static constexpr size_t JSM_JS_SuffixLength =
    mozilla::ArrayLength(JSM_JS_Suffix) - 1;
static constexpr char JS_Suffix[] = ".js";
static constexpr size_t JS_SuffixLength = mozilla::ArrayLength(JS_Suffix) - 1;
static constexpr char MJS_Suffix[] = ".sys.mjs";
static constexpr size_t MJS_SuffixLength = mozilla::ArrayLength(MJS_Suffix) - 1;

static bool IsJSM(const nsACString& aLocation) {
  if (aLocation.Length() < JSM_SuffixLength) {
    return false;
  }
  const auto ext = Substring(aLocation, aLocation.Length() - JSM_SuffixLength);
  return ext == JSM_Suffix;
}

static bool IsJS(const nsACString& aLocation) {
  if (aLocation.Length() < JS_SuffixLength) {
    return false;
  }
  const auto ext = Substring(aLocation, aLocation.Length() - JS_SuffixLength);
  return ext == JS_Suffix;
}

static bool IsJSM_JS(const nsACString& aLocation) {
  if (aLocation.Length() < JSM_JS_SuffixLength) {
    return false;
  }
  const auto ext =
      Substring(aLocation, aLocation.Length() - JSM_JS_SuffixLength);
  return ext == JSM_JS_Suffix;
}

static bool IsMJS(const nsACString& aLocation) {
  if (aLocation.Length() < MJS_SuffixLength) {
    return false;
  }
  const auto ext = Substring(aLocation, aLocation.Length() - MJS_SuffixLength);
  return ext == MJS_Suffix;
}

static void MJSToJSM(const nsACString& aLocation, nsAutoCString& aOut) {
  MOZ_ASSERT(IsMJS(aLocation));
  aOut = Substring(aLocation, 0, aLocation.Length() - MJS_SuffixLength);
  aOut += JSM_Suffix;
}

static bool TryToMJS(const nsACString& aLocation, nsAutoCString& aOut) {
  if (IsJSM(aLocation)) {
    aOut = Substring(aLocation, 0, aLocation.Length() - JSM_SuffixLength);
    aOut += MJS_Suffix;
    return true;
  }

  if (IsJSM_JS(aLocation)) {
    aOut = Substring(aLocation, 0, aLocation.Length() - JSM_JS_SuffixLength);
    aOut += MJS_Suffix;
    return true;
  }

  if (IsJS(aLocation)) {
    aOut = Substring(aLocation, 0, aLocation.Length() - JS_SuffixLength);
    aOut += MJS_Suffix;
    return true;
  }

  return false;
}

static bool Dump(JSContext* cx, unsigned argc, Value* vp) {
  if (!nsJSUtils::DumpEnabled()) {
    return true;
  }

  CallArgs args = CallArgsFromVp(argc, vp);

  if (args.length() == 0) {
    return true;
  }

  RootedString str(cx, JS::ToString(cx, args[0]));
  if (!str) {
    return false;
  }

  JS::UniqueChars utf8str = JS_EncodeStringToUTF8(cx, str);
  if (!utf8str) {
    return false;
  }

  MOZ_LOG(nsContentUtils::DOMDumpLog(), mozilla::LogLevel::Debug,
          ("[Backstage.Dump] %s", utf8str.get()));
#ifdef ANDROID
  __android_log_print(ANDROID_LOG_INFO, "Gecko", "%s", utf8str.get());
#endif
#ifdef XP_WIN
  if (IsDebuggerPresent()) {
    nsAutoJSString wstr;
    if (!wstr.init(cx, str)) {
      return false;
    }
    OutputDebugStringW(wstr.get());
  }
#endif
  fputs(utf8str.get(), stdout);
  fflush(stdout);
  return true;
}

static bool Debug(JSContext* cx, unsigned argc, Value* vp) {
#ifdef DEBUG
  return Dump(cx, argc, vp);
#else
  return true;
#endif
}

static const JSFunctionSpec gGlobalFun[] = {
    JS_FN("dump", Dump, 1, 0), JS_FN("debug", Debug, 1, 0),
    JS_FN("atob", Atob, 1, 0), JS_FN("btoa", Btoa, 1, 0), JS_FS_END};

class MOZ_STACK_CLASS JSCLContextHelper {
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

static nsresult MOZ_FORMAT_PRINTF(2, 3)
    ReportOnCallerUTF8(JSContext* callerContext, const char* format, ...) {
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

NS_IMPL_ISUPPORTS(mozJSModuleLoader, nsIMemoryReporter)

mozJSModuleLoader::mozJSModuleLoader()
    : mImports(16),
      mInProgressImports(16),
      mFallbackImports(16),
#ifdef STARTUP_RECORDER_ENABLED
      mImportStacks(16),
#endif
      mLocations(16),
      mInitialized(false),
      mLoaderGlobal(dom::RootingCx()),
      mServicesObj(dom::RootingCx()) {
}

#define ENSURE_DEP(name)          \
  {                               \
    nsresult rv = Ensure##name(); \
    NS_ENSURE_SUCCESS(rv, rv);    \
  }
#define ENSURE_DEPS(...) MOZ_FOR_EACH(ENSURE_DEP, (), (__VA_ARGS__));
#define BEGIN_ENSURE(self, ...) \
  {                             \
    if (m##self) return NS_OK;  \
    ENSURE_DEPS(__VA_ARGS__);   \
  }

class MOZ_STACK_CLASS ModuleLoaderInfo {
 public:
  explicit ModuleLoaderInfo(const nsACString& aLocation,
                            SkipCheckForBrokenURLOrZeroSized aSkipCheck =
                                SkipCheckForBrokenURLOrZeroSized::No)
      : mLocation(&aLocation), mIsModule(false), mSkipCheck(aSkipCheck) {}
  explicit ModuleLoaderInfo(JS::loader::ModuleLoadRequest* aRequest)
      : mLocation(nullptr),
        mURI(aRequest->mURI),
        mIsModule(true),
        mSkipCheck(aRequest->GetComponentLoadContext()->mSkipCheck) {}

  SkipCheckForBrokenURLOrZeroSized getSkipCheckForBrokenURLOrZeroSized() const {
    return mSkipCheck;
  }

  void resetChannelWithCheckForBrokenURLOrZeroSized() {
    MOZ_ASSERT(mSkipCheck == SkipCheckForBrokenURLOrZeroSized::Yes);
    mSkipCheck = SkipCheckForBrokenURLOrZeroSized::No;
    mScriptChannel = nullptr;
  }

  nsIIOService* IOService() {
    MOZ_ASSERT(mIOService);
    return mIOService;
  }
  nsresult EnsureIOService() {
    if (mIOService) {
      return NS_OK;
    }
    nsresult rv;
    mIOService = do_GetIOService(&rv);
    return rv;
  }

  nsIURI* URI() {
    MOZ_ASSERT(mURI);
    return mURI;
  }
  nsresult EnsureURI() {
    BEGIN_ENSURE(URI, IOService);
    MOZ_ASSERT(mLocation);
    return mIOService->NewURI(*mLocation, nullptr, nullptr,
                              getter_AddRefs(mURI));
  }

  nsIChannel* ScriptChannel() {
    MOZ_ASSERT(mScriptChannel);
    return mScriptChannel;
  }
  nsresult EnsureScriptChannel() {
    BEGIN_ENSURE(ScriptChannel, IOService, URI);

    // Skip check for missing URL when handling JSM-to-ESM fallback.
    return NS_NewChannel(
        getter_AddRefs(mScriptChannel), mURI,
        nsContentUtils::GetSystemPrincipal(),
        nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_SEC_CONTEXT_IS_NULL,
        nsIContentPolicy::TYPE_SCRIPT,
        /* aCookieJarSettings = */ nullptr,
        /* aPerformanceStorage = */ nullptr,
        /* aLoadGroup = */ nullptr, /* aCallbacks = */ nullptr,
        nsIRequest::LOAD_NORMAL, mIOService, /* aSandboxFlags = */ 0,
        mSkipCheck == SkipCheckForBrokenURLOrZeroSized::Yes);
  }

  nsIURI* ResolvedURI() {
    MOZ_ASSERT(mResolvedURI);
    return mResolvedURI;
  }
  nsresult EnsureResolvedURI() {
    BEGIN_ENSURE(ResolvedURI, URI);
    return ResolveURI(mURI, getter_AddRefs(mResolvedURI));
  }

  const nsACString& Key() {
    MOZ_ASSERT(mLocation);
    return *mLocation;
  }

  [[nodiscard]] nsresult GetLocation(nsCString& aLocation) {
    nsresult rv = EnsureURI();
    NS_ENSURE_SUCCESS(rv, rv);
    return mURI->GetSpec(aLocation);
  }

  bool IsModule() const { return mIsModule; }

 private:
  const nsACString* mLocation;
  nsCOMPtr<nsIIOService> mIOService;
  nsCOMPtr<nsIURI> mURI;
  nsCOMPtr<nsIChannel> mScriptChannel;
  nsCOMPtr<nsIURI> mResolvedURI;
  const bool mIsModule;
  SkipCheckForBrokenURLOrZeroSized mSkipCheck;
};

template <typename... Args>
static nsresult ReportOnCallerUTF8(JSCLContextHelper& helper,
                                   const char* format, ModuleLoaderInfo& info,
                                   Args... args) {
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

mozJSModuleLoader::~mozJSModuleLoader() {
  MOZ_ASSERT(!mInitialized,
             "UnloadModules() was not explicitly called before cleaning up "
             "mozJSModuleLoader");

  if (mInitialized) {
    UnloadModules();
  }
}

StaticRefPtr<mozJSModuleLoader> mozJSModuleLoader::sSelf;
StaticRefPtr<mozJSModuleLoader> mozJSModuleLoader::sDevToolsLoader;

void mozJSModuleLoader::FindTargetObject(JSContext* aCx,
                                         MutableHandleObject aTargetObject) {
  aTargetObject.set(JS::GetJSMEnvironmentOfScriptedCaller(aCx));

  // The above could fail if the scripted caller is not a JSM (it could be a DOM
  // scope, for instance).
  //
  // If the target object was not in the JSM shared global, return the global
  // instead. This is needed when calling the subscript loader within a frame
  // script, since it the FrameScript NSVO will have been found.
  if (!aTargetObject ||
      !IsLoaderGlobal(JS::GetNonCCWObjectGlobal(aTargetObject))) {
    aTargetObject.set(JS::GetScriptedCallerGlobal(aCx));

    // Return nullptr if the scripted caller is in a different compartment.
    if (JS::GetCompartment(aTargetObject) != js::GetContextCompartment(aCx)) {
      aTargetObject.set(nullptr);
    }
  }
}

void mozJSModuleLoader::InitStatics() {
  MOZ_ASSERT(!sSelf);
  sSelf = new mozJSModuleLoader();
  RegisterWeakMemoryReporter(sSelf);
}

void mozJSModuleLoader::UnloadLoaders() {
  if (sSelf) {
    sSelf->Unload();
  }
  if (sDevToolsLoader) {
    sDevToolsLoader->Unload();
  }
}

void mozJSModuleLoader::Unload() {
  UnloadModules();

  if (mModuleLoader) {
    mModuleLoader->Shutdown();
    mModuleLoader = nullptr;
  }
}

void mozJSModuleLoader::ShutdownLoaders() {
  MOZ_ASSERT(sSelf);
  UnregisterWeakMemoryReporter(sSelf);
  sSelf = nullptr;

  if (sDevToolsLoader) {
    UnregisterWeakMemoryReporter(sDevToolsLoader);
    sDevToolsLoader = nullptr;
  }
}

mozJSModuleLoader* mozJSModuleLoader::GetOrCreateDevToolsLoader() {
  if (sDevToolsLoader) {
    return sDevToolsLoader;
  }
  sDevToolsLoader = new mozJSModuleLoader();
  RegisterWeakMemoryReporter(sDevToolsLoader);
  return sDevToolsLoader;
}

// This requires that the keys be strings and the values be pointers.
template <class Key, class Data, class UserData, class Converter>
static size_t SizeOfTableExcludingThis(
    const nsBaseHashtable<Key, Data, UserData, Converter>& aTable,
    MallocSizeOf aMallocSizeOf) {
  size_t n = aTable.ShallowSizeOfExcludingThis(aMallocSizeOf);
  for (const auto& entry : aTable) {
    n += entry.GetKey().SizeOfExcludingThisIfUnshared(aMallocSizeOf);
    n += entry.GetData()->SizeOfIncludingThis(aMallocSizeOf);
  }
  return n;
}

#ifdef STARTUP_RECORDER_ENABLED
template <class Key, class Data, class UserData, class Converter>
static size_t SizeOfStringTableExcludingThis(
    const nsBaseHashtable<Key, Data, UserData, Converter>& aTable,
    MallocSizeOf aMallocSizeOf) {
  size_t n = aTable.ShallowSizeOfExcludingThis(aMallocSizeOf);
  for (const auto& entry : aTable) {
    n += entry.GetKey().SizeOfExcludingThisIfUnshared(aMallocSizeOf);
    n += entry.GetData().SizeOfExcludingThisIfUnshared(aMallocSizeOf);
  }
  return n;
}
#endif

size_t mozJSModuleLoader::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) {
  size_t n = aMallocSizeOf(this);
  n += SizeOfTableExcludingThis(mImports, aMallocSizeOf);
  n += mLocations.ShallowSizeOfExcludingThis(aMallocSizeOf);
  n += SizeOfTableExcludingThis(mInProgressImports, aMallocSizeOf);
  n += SizeOfTableExcludingThis(mFallbackImports, aMallocSizeOf);
#ifdef STARTUP_RECORDER_ENABLED
  n += SizeOfStringTableExcludingThis(mImportStacks, aMallocSizeOf);
#endif
  return n;
}

// Memory report paths are split on '/', with each module displayed as a
// separate layer of a visual tree. Any slashes which are meant to belong to a
// particular path module, rather than be used to build a hierarchy, therefore
// need to be replaced with backslashes, which are displayed as slashes in the
// UI.
//
// If `aAnonymize` is true, this function also attempts to translate any file:
// URLs to replace the path of the GRE directory with a placeholder containing
// no private information, and strips all other file: URIs of everything upto
// their last `/`.
static nsAutoCString MangleURL(const char* aURL, bool aAnonymize) {
  nsAutoCString url(aURL);

  if (aAnonymize) {
    static nsCString greDirURI;
    if (greDirURI.IsEmpty()) {
      nsCOMPtr<nsIFile> file;
      Unused << NS_GetSpecialDirectory(NS_GRE_DIR, getter_AddRefs(file));
      if (file) {
        nsCOMPtr<nsIURI> uri;
        NS_NewFileURI(getter_AddRefs(uri), file);
        if (uri) {
          uri->GetSpec(greDirURI);
          RunOnShutdown([&]() { greDirURI.Truncate(0); });
        }
      }
    }

    url.ReplaceSubstring(greDirURI, "<GREDir>/"_ns);

    if (FindInReadable("file:"_ns, url)) {
      if (StringBeginsWith(url, "jar:file:"_ns)) {
        int32_t idx = url.RFindChar('!');
        url = "jar:file://<anonymized>!"_ns + Substring(url, idx);
      } else {
        int32_t idx = url.RFindChar('/');
        url = "file://<anonymized>/"_ns + Substring(url, idx);
      }
    }
  }

  url.ReplaceChar('/', '\\');
  return url;
}

NS_IMETHODIMP
mozJSModuleLoader::CollectReports(nsIHandleReportCallback* aHandleReport,
                                  nsISupports* aData, bool aAnonymize) {
  for (const auto& entry : mImports.Values()) {
    nsAutoCString path("js-module-loader/modules/");
    path.Append(MangleURL(entry->location, aAnonymize));

    aHandleReport->Callback(""_ns, path, KIND_NONHEAP, UNITS_COUNT, 1,
                            "Loaded JS modules"_ns, aData);
  }

  return NS_OK;
}

void mozJSModuleLoader::CreateLoaderGlobal(JSContext* aCx,
                                           const nsACString& aLocation,
                                           MutableHandleObject aGlobal) {
  auto backstagePass = MakeRefPtr<BackstagePass>();
  RealmOptions options;
  auto& creationOptions = options.creationOptions();

  creationOptions.setFreezeBuiltins(true).setNewCompartmentInSystemZone();
  if (IsDevToolsLoader()) {
    creationOptions.setInvisibleToDebugger(true);
  }
  xpc::SetPrefableRealmOptions(options);

  // Defer firing OnNewGlobalObject until after the __URI__ property has
  // been defined so the JS debugger can tell what module the global is
  // for
  RootedObject global(aCx);

#ifdef DEBUG
  // See mozJSModuleLoader::DefineJSServices.
  mIsInitializingLoaderGlobal = true;
#endif
  nsresult rv = xpc::InitClassesWithNewWrappedGlobal(
      aCx, static_cast<nsIGlobalObject*>(backstagePass),
      nsContentUtils::GetSystemPrincipal(), xpc::DONT_FIRE_ONNEWGLOBALHOOK,
      options, &global);
#ifdef DEBUG
  mIsInitializingLoaderGlobal = false;
#endif
  NS_ENSURE_SUCCESS_VOID(rv);

  NS_ENSURE_TRUE_VOID(global);

  backstagePass->SetGlobalObject(global);

  JSAutoRealm ar(aCx, global);
  if (!JS_DefineFunctions(aCx, global, gGlobalFun)) {
    return;
  }

  if (!CreateJSServices(aCx)) {
    return;
  }

  if (!DefineJSServices(aCx, global)) {
    return;
  }

  // Set the location information for the new global, so that tools like
  // about:memory may use that information
  xpc::SetLocationForGlobal(global, aLocation);

  MOZ_ASSERT(!mModuleLoader);
  RefPtr<ComponentScriptLoader> scriptLoader = new ComponentScriptLoader;
  mModuleLoader = new ComponentModuleLoader(scriptLoader, backstagePass);
  backstagePass->InitModuleLoader(mModuleLoader);

  aGlobal.set(global);
}

JSObject* mozJSModuleLoader::GetSharedGlobal(JSContext* aCx) {
  if (!mLoaderGlobal) {
    JS::RootedObject globalObj(aCx);

    CreateLoaderGlobal(
        aCx, IsDevToolsLoader() ? "DevTools global"_ns : "shared JSM global"_ns,
        &globalObj);

    // If we fail to create a module global this early, we're not going to
    // get very far, so just bail out now.
    MOZ_RELEASE_ASSERT(globalObj);
    mLoaderGlobal = globalObj;

    // AutoEntryScript required to invoke debugger hook, which is a
    // Gecko-specific concept at present.
    dom::AutoEntryScript aes(globalObj, "module loader report global");
    JS_FireOnNewGlobalObject(aes.cx(), globalObj);
  }

  return mLoaderGlobal;
}

/* static */
nsresult mozJSModuleLoader::LoadSingleModuleScript(
    ComponentModuleLoader* aModuleLoader, JSContext* aCx,
    JS::loader::ModuleLoadRequest* aRequest, MutableHandleScript aScriptOut) {
  AUTO_PROFILER_MARKER_TEXT(
      "ChromeUtils.importESModule static import", JS,
      MarkerOptions(MarkerStack::Capture(),
                    MarkerInnerWindowIdFromJSContext(aCx)),
      nsContentUtils::TruncatedURLForDisplay(aRequest->mURI));

  ModuleLoaderInfo info(aRequest);
  nsresult rv = info.EnsureResolvedURI();
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIFile> sourceFile;
  rv = GetSourceFile(info.ResolvedURI(), getter_AddRefs(sourceFile));
  NS_ENSURE_SUCCESS(rv, rv);

  bool realFile = LocationIsRealFile(aRequest->mURI);

  RootedScript script(aCx);
  rv = GetScriptForLocation(aCx, info, sourceFile, realFile, aScriptOut);
  NS_ENSURE_SUCCESS(rv, rv);

#ifdef STARTUP_RECORDER_ENABLED
  if (aModuleLoader == sSelf->mModuleLoader) {
    sSelf->RecordImportStack(aCx, aRequest);
  } else {
    MOZ_ASSERT(sDevToolsLoader);
    MOZ_ASSERT(aModuleLoader == sDevToolsLoader->mModuleLoader);
    sDevToolsLoader->RecordImportStack(aCx, aRequest);
  }
#endif

  return NS_OK;
}

/* static */
nsresult mozJSModuleLoader::GetSourceFile(nsIURI* aResolvedURI,
                                          nsIFile** aSourceFileOut) {
  // Get the JAR if there is one.
  nsCOMPtr<nsIJARURI> jarURI;
  nsresult rv = NS_OK;
  jarURI = do_QueryInterface(aResolvedURI, &rv);
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
    baseFileURL = do_QueryInterface(aResolvedURI, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return baseFileURL->GetFile(aSourceFileOut);
}

/* static */
bool mozJSModuleLoader::LocationIsRealFile(nsIURI* aURI) {
  // We need to be extra careful checking for URIs pointing to files.
  // EnsureFile may not always get called, especially on resource URIs so we
  // need to call GetFile to make sure this is a valid file.
  nsresult rv = NS_OK;
  nsCOMPtr<nsIFileURL> fileURL = do_QueryInterface(aURI, &rv);
  nsCOMPtr<nsIFile> testFile;
  if (NS_SUCCEEDED(rv)) {
    fileURL->GetFile(getter_AddRefs(testFile));
  }

  return bool(testFile);
}

JSObject* mozJSModuleLoader::PrepareObjectForLocation(JSContext* aCx,
                                                      nsIFile* aModuleFile,
                                                      nsIURI* aURI,
                                                      bool aRealFile) {
  RootedObject globalObj(aCx, GetSharedGlobal(aCx));
  NS_ENSURE_TRUE(globalObj, nullptr);
  JSAutoRealm ar(aCx, globalObj);

  // |thisObj| is the object we set properties on for a particular .jsm.
  RootedObject thisObj(aCx, JS::NewJSMEnvironment(aCx));
  NS_ENSURE_TRUE(thisObj, nullptr);

  if (aRealFile) {
    if (XRE_IsParentProcess()) {
      RootedObject locationObj(aCx);

      nsresult rv = nsXPConnect::XPConnect()->WrapNative(
          aCx, thisObj, aModuleFile, NS_GET_IID(nsIFile),
          locationObj.address());
      NS_ENSURE_SUCCESS(rv, nullptr);
      NS_ENSURE_TRUE(locationObj, nullptr);

      if (!JS_DefineProperty(aCx, thisObj, "__LOCATION__", locationObj, 0)) {
        return nullptr;
      }
    }
  }

  // Expose the URI from which the script was imported through a special
  // variable that we insert into the JSM.
  nsAutoCString nativePath;
  NS_ENSURE_SUCCESS(aURI->GetSpec(nativePath), nullptr);

  RootedString exposedUri(
      aCx, JS_NewStringCopyN(aCx, nativePath.get(), nativePath.Length()));
  NS_ENSURE_TRUE(exposedUri, nullptr);

  if (!JS_DefineProperty(aCx, thisObj, "__URI__", exposedUri, 0)) {
    return nullptr;
  }

  return thisObj;
}

static mozilla::Result<nsCString, nsresult> ReadScript(
    ModuleLoaderInfo& aInfo) {
  MOZ_TRY(aInfo.EnsureScriptChannel());

  nsCOMPtr<nsIInputStream> scriptStream;
  MOZ_TRY(aInfo.ScriptChannel()->Open(getter_AddRefs(scriptStream)));

  uint64_t len64;
  uint32_t bytesRead;

  MOZ_TRY(scriptStream->Available(&len64));
  NS_ENSURE_TRUE(len64 < UINT32_MAX, Err(NS_ERROR_FILE_TOO_BIG));
  NS_ENSURE_TRUE(len64, Err(NS_ERROR_FAILURE));
  uint32_t len = (uint32_t)len64;

  /* malloc an internal buf the size of the file */
  nsCString str;
  if (!str.SetLength(len, fallible)) {
    return Err(NS_ERROR_OUT_OF_MEMORY);
  }

  /* read the file in one swoop */
  MOZ_TRY(scriptStream->Read(str.BeginWriting(), len, &bytesRead));
  if (bytesRead != len) {
    return Err(NS_BASE_STREAM_OSERROR);
  }

  return std::move(str);
}

nsresult mozJSModuleLoader::ObjectForLocation(
    ModuleLoaderInfo& aInfo, nsIFile* aModuleFile, MutableHandleObject aObject,
    MutableHandleScript aTableScript, char** aLocation,
    bool aPropagateExceptions, MutableHandleValue aException) {
  MOZ_ASSERT(NS_IsMainThread(), "Must be on main thread.");

  dom::AutoJSAPI jsapi;
  jsapi.Init();
  JSContext* cx = jsapi.cx();

  nsresult rv = aInfo.EnsureURI();
  NS_ENSURE_SUCCESS(rv, rv);

  bool realFile = LocationIsRealFile(aInfo.URI());

  RootedObject obj(
      cx, PrepareObjectForLocation(cx, aModuleFile, aInfo.URI(), realFile));
  NS_ENSURE_TRUE(obj, NS_ERROR_FAILURE);
  MOZ_ASSERT(!JS_IsGlobalObject(obj));

  JSAutoRealm ar(cx, obj);

  RootedScript script(cx);
  rv = GetScriptForLocation(cx, aInfo, aModuleFile, realFile, &script,
                            aLocation);
  if (NS_FAILED(rv)) {
    // Propagate the exception, if one exists. Also, don't leave the stale
    // exception on this context.
    if (aPropagateExceptions && jsapi.HasException()) {
      if (!jsapi.StealException(aException)) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
    }

    return rv;
  }

  // Assign aObject here so that it's available to recursive imports.
  // See bug 384168.
  aObject.set(obj);

  aTableScript.set(script);

  {  // Scope for AutoEntryScript
    AutoAllowLegacyScriptExecution exemption;

    // We're going to run script via JS_ExecuteScript, so we need an
    // AutoEntryScript. This is Gecko-specific and not in any spec.
    dom::AutoEntryScript aes(CurrentGlobalOrNull(cx),
                             "module loader load module");
    JSContext* aescx = aes.cx();

    bool executeOk = false;
    if (JS_IsGlobalObject(obj)) {
      JS::RootedValue rval(cx);
      executeOk = JS_ExecuteScript(aescx, script, &rval);
    } else {
      executeOk = JS::ExecuteInJSMEnvironment(aescx, script, obj);
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

  return rv;
}

/* static */
nsresult mozJSModuleLoader::GetScriptForLocation(
    JSContext* aCx, ModuleLoaderInfo& aInfo, nsIFile* aModuleFile,
    bool aUseMemMap, MutableHandleScript aScriptOut, char** aLocationOut) {
  // JS compilation errors are returned via an exception on the context.
  MOZ_ASSERT(!JS_IsExceptionPending(aCx));

  aScriptOut.set(nullptr);

  nsAutoCString nativePath;
  nsresult rv = aInfo.URI()->GetSpec(nativePath);
  NS_ENSURE_SUCCESS(rv, rv);

  // Before compiling the script, first check to see if we have it in
  // the preloader cache or the startupcache.  Note: as a rule, preloader cache
  // errors and startupcache errors are not fatal to loading the script, since
  // we can always slow-load.

  bool storeIntoStartupCache = false;
  StartupCache* cache = StartupCache::GetSingleton();

  aInfo.EnsureResolvedURI();

  nsAutoCString cachePath;
  if (aInfo.IsModule()) {
    rv = PathifyURI(JS_CACHE_PREFIX("non-syntactic", "module"),
                    aInfo.ResolvedURI(), cachePath);
  } else {
    rv = PathifyURI(JS_CACHE_PREFIX("non-syntactic", "script"),
                    aInfo.ResolvedURI(), cachePath);
  }
  NS_ENSURE_SUCCESS(rv, rv);

  JS::DecodeOptions decodeOptions;
  ScriptPreloader::FillDecodeOptionsForCachedStencil(decodeOptions);

  RefPtr<JS::Stencil> stencil =
      ScriptPreloader::GetSingleton().GetCachedStencil(aCx, decodeOptions,
                                                       cachePath);

  if (!stencil && cache) {
    ReadCachedStencil(cache, cachePath, aCx, decodeOptions,
                      getter_AddRefs(stencil));
    if (!stencil) {
      JS_ClearPendingException(aCx);

      storeIntoStartupCache = true;
    }
  }

  if (stencil) {
    LOG(("Successfully loaded %s from cache\n", nativePath.get()));
  } else {
    // The script wasn't in the cache , so compile it now.
    LOG(("Slow loading %s\n", nativePath.get()));

    CompileOptions options(aCx);
    ScriptPreloader::FillCompileOptionsForCachedStencil(options);
    options.setFileAndLine(nativePath.get(), 1);
    if (aInfo.IsModule()) {
      options.setModule();
      // Top level await is not supported in synchronously loaded modules.
      options.topLevelAwait = false;

      // Make all top-level `vars` available in `ModuleEnvironmentObject`.
      options.deoptimizeModuleGlobalVars = true;
    } else {
      options.setForceStrictMode();
      options.setNonSyntacticScope(true);
    }

    // If we can no longer write to caches, we should stop using lazy sources
    // and instead let normal syntax parsing occur. This can occur in content
    // processes after the ScriptPreloader is flushed where we can read but no
    // longer write.
    if (!storeIntoStartupCache && !ScriptPreloader::GetSingleton().Active()) {
      options.setSourceIsLazy(false);
    }

    if (aUseMemMap) {
      AutoMemMap map;
      MOZ_TRY(map.init(aModuleFile));

      // Note: exceptions will get handled further down;
      // don't early return for them here.
      auto buf = map.get<char>();

      JS::SourceText<mozilla::Utf8Unit> srcBuf;
      if (srcBuf.init(aCx, buf.get(), map.size(),
                      JS::SourceOwnership::Borrowed)) {
        stencil = CompileStencil(aCx, options, srcBuf, aInfo.IsModule());
      }
    } else {
      nsCString str;
      MOZ_TRY_VAR(str, ReadScript(aInfo));

      JS::SourceText<mozilla::Utf8Unit> srcBuf;
      if (srcBuf.init(aCx, str.get(), str.Length(),
                      JS::SourceOwnership::Borrowed)) {
        stencil = CompileStencil(aCx, options, srcBuf, aInfo.IsModule());
      }
    }

#ifdef DEBUG
    // The above shouldn't touch any options for instantiation.
    JS::InstantiateOptions instantiateOptions(options);
    instantiateOptions.assertDefault();
#endif

    if (!stencil) {
      return NS_ERROR_FAILURE;
    }
  }

  aScriptOut.set(InstantiateStencil(aCx, stencil, aInfo.IsModule()));
  if (!aScriptOut) {
    return NS_ERROR_FAILURE;
  }

  // ScriptPreloader::NoteScript needs to be called unconditionally, to
  // reflect the usage into the next session's cache.
  ScriptPreloader::GetSingleton().NoteStencil(nativePath, cachePath, stencil);

  // Write to startup cache only when we didn't have any cache for the script
  // and compiled it.
  if (storeIntoStartupCache) {
    MOZ_ASSERT(stencil);

    // We successfully compiled the script, so cache it.
    rv = WriteCachedStencil(cache, cachePath, aCx, stencil);

    // Don't treat failure to write as fatal, since we might be working
    // with a read-only cache.
    if (NS_SUCCEEDED(rv)) {
      LOG(("Successfully wrote to cache\n"));
    } else {
      LOG(("Failed to write to cache\n"));
    }
  }

  /* Owned by ModuleEntry. Freed when we remove from the table. */
  if (aLocationOut) {
    *aLocationOut = ToNewCString(nativePath, mozilla::fallible);
    if (!*aLocationOut) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  return NS_OK;
}

void mozJSModuleLoader::UnloadModules() {
  mInitialized = false;

  if (mLoaderGlobal) {
    MOZ_ASSERT(JS_HasExtensibleLexicalEnvironment(mLoaderGlobal));
    JS::RootedObject lexicalEnv(dom::RootingCx(),
                                JS_ExtensibleLexicalEnvironment(mLoaderGlobal));
    JS_SetAllNonReservedSlotsToUndefined(lexicalEnv);
    JS_SetAllNonReservedSlotsToUndefined(mLoaderGlobal);
    mLoaderGlobal = nullptr;
  }
  mServicesObj = nullptr;

#ifdef STARTUP_RECORDER_ENABLED
  mImportStacks.Clear();
#endif
  mFallbackImports.Clear();
  mInProgressImports.Clear();
  mImports.Clear();
  mLocations.Clear();
}

/* static */
already_AddRefed<Stencil> mozJSModuleLoader::CompileStencil(
    JSContext* aCx, const JS::CompileOptions& aOptions,
    JS::SourceText<mozilla::Utf8Unit>& aSource, bool aIsModule) {
  if (aIsModule) {
    return CompileModuleScriptToStencil(aCx, aOptions, aSource);
  }

  return CompileGlobalScriptToStencil(aCx, aOptions, aSource);
}

/* static */
JSScript* mozJSModuleLoader::InstantiateStencil(JSContext* aCx,
                                                JS::Stencil* aStencil,
                                                bool aIsModule) {
  JS::InstantiateOptions instantiateOptions;

  if (aIsModule) {
    RootedObject module(aCx);
    module = JS::InstantiateModuleStencil(aCx, instantiateOptions, aStencil);
    if (!module) {
      return nullptr;
    }

    return JS::GetModuleScript(module);
  }

  return JS::InstantiateGlobalStencil(aCx, instantiateOptions, aStencil);
}

nsresult mozJSModuleLoader::ImportInto(const nsACString& registryLocation,
                                       HandleValue targetValArg, JSContext* cx,
                                       uint8_t optionalArgc,
                                       MutableHandleValue retval) {
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
          !WrapperFactory::WaiveXrayAndWrap(cx, &targetVal)) {
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
    if (!targetObject) {
      return ReportOnCallerUTF8(cx, ERROR_NO_TARGET_OBJECT,
                                PromiseFlatCString(registryLocation).get());
    }
  }

  js::AssertSameCompartment(cx, targetObject);

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

nsresult mozJSModuleLoader::IsModuleLoaded(const nsACString& aLocation,
                                           bool* retval) {
  MOZ_ASSERT(nsContentUtils::IsCallerChrome());

  mInitialized = true;
  ModuleLoaderInfo info(aLocation);
  if (mImports.Get(info.Key())) {
    *retval = true;
    return NS_OK;
  }

  if (mModuleLoader) {
    nsAutoCString mjsLocation;
    if (!TryToMJS(aLocation, mjsLocation)) {
      *retval = false;
      return NS_OK;
    }

    ModuleLoaderInfo mjsInfo(mjsLocation);

    nsresult rv = mjsInfo.EnsureURI();
    NS_ENSURE_SUCCESS(rv, rv);

    if (mModuleLoader->IsModuleFetched(mjsInfo.URI())) {
      *retval = true;
      return NS_OK;
    }
  }

  *retval = false;
  return NS_OK;
}

nsresult mozJSModuleLoader::IsJSModuleLoaded(const nsACString& aLocation,
                                             bool* retval) {
  MOZ_ASSERT(nsContentUtils::IsCallerChrome());

  mInitialized = true;
  ModuleLoaderInfo info(aLocation);
  if (mImports.Get(info.Key())) {
    *retval = true;
    return NS_OK;
  }

  *retval = false;
  return NS_OK;
}

nsresult mozJSModuleLoader::IsESModuleLoaded(const nsACString& aLocation,
                                             bool* retval) {
  MOZ_ASSERT(nsContentUtils::IsCallerChrome());

  mInitialized = true;
  ModuleLoaderInfo info(aLocation);

  nsresult rv = info.EnsureURI();
  NS_ENSURE_SUCCESS(rv, rv);

  if (mModuleLoader->IsModuleFetched(info.URI())) {
    *retval = true;
    return NS_OK;
  }

  *retval = false;
  return NS_OK;
}

void mozJSModuleLoader::GetLoadedModules(nsTArray<nsCString>& aLoadedModules) {
  aLoadedModules.SetCapacity(mImports.Count());
  for (const auto& data : mImports.Values()) {
    aLoadedModules.AppendElement(data->location);
  }
}

nsresult mozJSModuleLoader::GetLoadedESModules(
    nsTArray<nsCString>& aLoadedModules) {
  return mModuleLoader->GetFetchedModuleURLs(aLoadedModules);
}

nsresult mozJSModuleLoader::GetLoadedJSAndESModules(
    nsTArray<nsCString>& aLoadedModules) {
  GetLoadedModules(aLoadedModules);

  nsTArray<nsCString> modules;
  nsresult rv = GetLoadedESModules(modules);
  NS_ENSURE_SUCCESS(rv, rv);

  for (const auto& location : modules) {
    if (IsMJS(location)) {
      nsAutoCString jsmLocation;
      // NOTE: Unconditionally convert to *.jsm.  This doesn't cover *.js case
      //       but given `Cu.loadedModules` is rarely used for system modules,
      //       this won't cause much compat issue.
      MJSToJSM(location, jsmLocation);
      aLoadedModules.AppendElement(jsmLocation);
    }
  }

  return NS_OK;
}

#ifdef STARTUP_RECORDER_ENABLED
void mozJSModuleLoader::RecordImportStack(JSContext* aCx,
                                          const nsACString& aLocation) {
  if (!Preferences::GetBool("browser.startup.record", false)) {
    return;
  }

  mImportStacks.InsertOrUpdate(
      aLocation, xpc_PrintJSStack(aCx, false, false, false).get());
}

void mozJSModuleLoader::RecordImportStack(
    JSContext* aCx, JS::loader::ModuleLoadRequest* aRequest) {
  if (!Preferences::GetBool("browser.startup.record", false)) {
    return;
  }

  nsAutoCString location;
  nsresult rv = aRequest->mURI->GetSpec(location);
  if (NS_FAILED(rv)) {
    return;
  }

  auto recordJSStackOnly = [&]() {
    mImportStacks.InsertOrUpdate(
        location, xpc_PrintJSStack(aCx, false, false, false).get());
  };

  if (aRequest->IsTopLevel()) {
    recordJSStackOnly();
    return;
  }

  nsAutoCString importerSpec;
  rv = aRequest->mReferrer->GetSpec(importerSpec);
  if (NS_FAILED(rv)) {
    recordJSStackOnly();
    return;
  }

  ModuleLoaderInfo importerInfo(importerSpec);
  auto importerStack = mImportStacks.Lookup(importerInfo.Key());
  if (!importerStack) {
    // The importer's stack is not collected, possibly due to OOM.
    recordJSStackOnly();
    return;
  }

  nsAutoCString stack;

  stack += "* import [\"";
  stack += importerSpec;
  stack += "\"]\n";
  stack += *importerStack;

  mImportStacks.InsertOrUpdate(location, stack);
}
#endif

nsresult mozJSModuleLoader::GetModuleImportStack(const nsACString& aLocation,
                                                 nsACString& retval) {
#ifdef STARTUP_RECORDER_ENABLED
  MOZ_ASSERT(nsContentUtils::IsCallerChrome());

  // When querying the DevTools loader, it may not be initialized yet
  if (!mInitialized) {
    return NS_ERROR_FAILURE;
  }

  ModuleLoaderInfo info(aLocation);
  auto str = mImportStacks.Lookup(info.Key());
  if (!str) {
    return NS_ERROR_FAILURE;
  }

  retval = *str;
  return NS_OK;
#else
  return NS_ERROR_NOT_IMPLEMENTED;
#endif
}

nsresult mozJSModuleLoader::ImportInto(const nsACString& aLocation,
                                       HandleObject targetObj, JSContext* cx,
                                       MutableHandleObject vp) {
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

nsresult mozJSModuleLoader::ExtractExports(JSContext* aCx,
                                           ModuleLoaderInfo& aInfo,
                                           ModuleEntry* aMod,
                                           JS::MutableHandleObject aExports) {
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
    RootedObject obj(
        cx, ResolveModuleObjectProperty(cx, aMod->obj, "EXPORTED_SYMBOLS"));
    if (!obj || !JS_GetProperty(cx, obj, "EXPORTED_SYMBOLS", &symbols)) {
      return ReportOnCallerUTF8(cxhelper, ERROR_NOT_PRESENT, aInfo);
    }
  }

  bool isArray;
  if (!JS::IsArrayObject(cx, symbols, &isArray)) {
    return NS_ERROR_FAILURE;
  }
  if (!isArray) {
    return ReportOnCallerUTF8(cxhelper, ERROR_NOT_AN_ARRAY, aInfo);
  }

  RootedObject symbolsObj(cx, &symbols.toObject());

  // Iterate over symbols array, installing symbols on targetObj:

  uint32_t symbolCount = 0;
  if (!JS::GetArrayLength(cx, symbolsObj, &symbolCount)) {
    return ReportOnCallerUTF8(cxhelper, ERROR_GETTING_ARRAY_LENGTH, aInfo);
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
    if (!JS_GetElement(cx, symbolsObj, i, &value) || !value.isString() ||
        !JS_ValueToId(cx, value, &symbolId)) {
      return ReportOnCallerUTF8(cxhelper, ERROR_ARRAY_ELEMENT, aInfo, i);
    }

    symbolHolder = ResolveModuleObjectPropertyById(cx, aMod->obj, symbolId);
    if (!symbolHolder ||
        !JS_GetPropertyById(cx, symbolHolder, symbolId, &value)) {
      RootedString symbolStr(cx, symbolId.toString());
      JS::UniqueChars bytes = JS_EncodeStringToUTF8(cx, symbolStr);
      if (!bytes) {
        return NS_ERROR_FAILURE;
      }
      return ReportOnCallerUTF8(cxhelper, ERROR_GETTING_SYMBOL, aInfo,
                                bytes.get());
    }

    // It's possible |value| is the uninitialized lexical MagicValue when
    // there's a cyclic import: const obj = ChromeUtils.import("parent.jsm").
    if (value.isMagic(JS_UNINITIALIZED_LEXICAL)) {
      RootedString symbolStr(cx, symbolId.toString());
      JS::UniqueChars bytes = JS_EncodeStringToUTF8(cx, symbolStr);
      if (!bytes) {
        return NS_ERROR_FAILURE;
      }
      return ReportOnCallerUTF8(cxhelper, ERROR_UNINITIALIZED_SYMBOL, aInfo,
                                bytes.get());
    }

    if (value.isUndefined()) {
      missing = true;
    }

    if (!JS_SetPropertyById(cx, aExports, symbolId, value)) {
      RootedString symbolStr(cx, symbolId.toString());
      JS::UniqueChars bytes = JS_EncodeStringToUTF8(cx, symbolStr);
      if (!bytes) {
        return NS_ERROR_FAILURE;
      }
      return ReportOnCallerUTF8(cxhelper, ERROR_GETTING_SYMBOL, aInfo,
                                bytes.get());
    }
#ifdef DEBUG
    if (i == 0) {
      logBuffer.AssignLiteral("Installing symbols [ ");
    }
    JS::UniqueChars bytes = JS_EncodeStringToLatin1(cx, symbolId.toString());
    if (!!bytes) {
      logBuffer.Append(bytes.get());
    }
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

/* static */
bool mozJSModuleLoader::IsTrustedScheme(nsIURI* aURI) {
  return aURI->SchemeIs("resource") || aURI->SchemeIs("chrome");
}

nsresult mozJSModuleLoader::Import(JSContext* aCx, const nsACString& aLocation,
                                   JS::MutableHandleObject aModuleGlobal,
                                   JS::MutableHandleObject aModuleExports,
                                   bool aIgnoreExports) {
  mInitialized = true;

  AUTO_PROFILER_MARKER_TEXT(
      "ChromeUtils.import", JS,
      MarkerOptions(MarkerStack::Capture(),
                    MarkerInnerWindowIdFromJSContext(aCx)),
      Substring(aLocation, 0, std::min(size_t(128), aLocation.Length())));

  // The JSM may already be ESM-ified, and in that case the load is expected
  // to fail.  Suppress the error message, the crash, and also the telemetry
  // event for the failure.
  //
  // If this load fails, it will be redirected to `.sys.mjs` URL
  // in TryFallbackToImportESModule, and if the redirect also fails,
  // the load is performed again below, with the check enabled.
  ModuleLoaderInfo info(aLocation, SkipCheckForBrokenURLOrZeroSized::Yes);

  nsresult rv;
  ModuleEntry* mod;
  UniquePtr<ModuleEntry> newEntry;
  if (!mImports.Get(info.Key(), &mod) &&
      !mInProgressImports.Get(info.Key(), &mod)) {
    // We're trying to import a new JSM, but we're late in shutdown and this
    // will likely not succeed and might even crash, so fail here.
    if (PastShutdownPhase(ShutdownPhase::XPCOMShutdownFinal)) {
      return NS_ERROR_ILLEGAL_DURING_SHUTDOWN;
    }

    // If we've hit file-not-found and fallback was successful,
    // return the cached data.
    bool aFound;
    rv = TryCachedFallbackToImportESModule(
        aCx, aLocation, aModuleGlobal, aModuleExports, aIgnoreExports, &aFound);
    NS_ENSURE_SUCCESS(rv, rv);
    if (aFound) {
      return NS_OK;
    }

    newEntry = MakeUnique<ModuleEntry>(RootingContext::get(aCx));

    // Note: This implies EnsureURI().
    MOZ_TRY(info.EnsureResolvedURI());

    // Reject imports from untrusted sources.
    if (!IsTrustedScheme(info.URI())) {
      return NS_ERROR_DOM_SECURITY_ERR;
    }

    nsCOMPtr<nsIFile> sourceFile;
    rv = GetSourceFile(info.ResolvedURI(), getter_AddRefs(sourceFile));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = info.ResolvedURI()->GetSpec(newEntry->resolvedURL);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCString* existingPath;
    if (mLocations.Get(newEntry->resolvedURL, &existingPath) &&
        *existingPath != info.Key()) {
      return NS_ERROR_UNEXPECTED;
    }

    mLocations.InsertOrUpdate(newEntry->resolvedURL,
                              MakeUnique<nsCString>(info.Key()));

    RootedValue exception(aCx);
    {
      mInProgressImports.InsertOrUpdate(info.Key(), newEntry.get());
      auto cleanup =
          MakeScopeExit([&]() { mInProgressImports.Remove(info.Key()); });

      rv = ObjectForLocation(info, sourceFile, &newEntry->obj,
                             &newEntry->thisObjectKey, &newEntry->location,
                             true, &exception);
    }

    if (NS_FAILED(rv)) {
      mLocations.Remove(newEntry->resolvedURL);
      if (!exception.isUndefined()) {
        // An exception was thrown during compilation. Propagate it
        // out to our caller so they can report it.
        bool isModuleSyntaxError = false;

        if (exception.isObject()) {
          JS::Rooted<JSObject*> exceptionObj(aCx, &exception.toObject());
          JSAutoRealm ar(aCx, exceptionObj);
          JSErrorReport* report = JS_ErrorFromException(aCx, exceptionObj);
          if (report) {
            switch (report->errorNumber) {
              case JSMSG_IMPORT_DECL_AT_TOP_LEVEL:
              case JSMSG_EXPORT_DECL_AT_TOP_LEVEL:
                // If the exception is related to module syntax, it's most
                // likely because of misuse of API.
                // Provide better error message.
                isModuleSyntaxError = true;

                JS_ReportErrorUTF8(aCx,
                                   "ChromeUtils.import is called against "
                                   "an ES module script (%s).  Please use "
                                   "ChromeUtils.importESModule instead "
                                   "(SyntaxError: %s)",
                                   aLocation.BeginReading(),
                                   report->message().c_str());
                break;
              default:
                break;
            }
          }
        }

        if (!isModuleSyntaxError) {
          if (!JS_WrapValue(aCx, &exception)) {
            return NS_ERROR_OUT_OF_MEMORY;
          }
          JS_SetPendingException(aCx, exception);
        }

        return NS_ERROR_FAILURE;
      }

      if (rv == NS_ERROR_FILE_NOT_FOUND || rv == NS_ERROR_FILE_ACCESS_DENIED) {
        // NS_ERROR_FILE_ACCESS_DENIED happens if the access is blocked by
        // sandbox.
        rv = TryFallbackToImportESModule(aCx, aLocation, aModuleGlobal,
                                         aModuleExports, aIgnoreExports);

        if (rv == NS_ERROR_FILE_NOT_FOUND ||
            rv == NS_ERROR_FILE_ACCESS_DENIED) {
          // Both JSM and ESM are not found, with the check inside necko
          // skipped (See EnsureScriptChannel and mSkipCheck).
          //
          // Perform the load again with the check enabled, so that
          // logging, crash-on-autonation, and telemetry event happen.
          if (NS_SUCCEEDED(info.EnsureURI()) &&
              !LocationIsRealFile(info.URI())) {
            info.resetChannelWithCheckForBrokenURLOrZeroSized();
            (void)ReadScript(info);
          }
        }

        return rv;
      }

      // Something failed, but we don't know what it is, guess.
      return NS_ERROR_FILE_NOT_FOUND;
    }

#ifdef STARTUP_RECORDER_ENABLED
    RecordImportStack(aCx, aLocation);
#endif

    mod = newEntry.get();
  }

  MOZ_ASSERT(mod->obj, "Import table contains entry with no object");
  JS::RootedObject globalProxy(aCx);
  {
    JSAutoRealm ar(aCx, mod->obj);

    globalProxy = CreateJSMEnvironmentProxy(aCx, mod->obj);
    if (!globalProxy) {
      return NS_ERROR_FAILURE;
    }
  }
  if (!JS_WrapObject(aCx, &globalProxy)) {
    return NS_ERROR_FAILURE;
  }
  aModuleGlobal.set(globalProxy);

  JS::RootedObject exports(aCx, mod->exports);
  if (!exports && !aIgnoreExports) {
    MOZ_TRY(ExtractExports(aCx, info, mod, &exports));
  }

  if (exports && !JS_WrapObject(aCx, &exports)) {
    mLocations.Remove(newEntry->resolvedURL);
    return NS_ERROR_FAILURE;
  }
  aModuleExports.set(exports);

  // Cache this module for later
  if (newEntry) {
    mImports.InsertOrUpdate(info.Key(), std::move(newEntry));
  }

  return NS_OK;
}

nsresult mozJSModuleLoader::TryFallbackToImportESModule(
    JSContext* aCx, const nsACString& aLocation,
    JS::MutableHandleObject aModuleGlobal,
    JS::MutableHandleObject aModuleExports, bool aIgnoreExports) {
  nsAutoCString mjsLocation;
  if (!TryToMJS(aLocation, mjsLocation)) {
    return NS_ERROR_FILE_NOT_FOUND;
  }

  JS::RootedObject moduleNamespace(aCx);
  // The fallback can fail if the URL was not for ESMified JSM.  Suppress the
  // error message, the crash, and also the telemetry event for the failure.
  nsresult rv = ImportESModule(aCx, mjsLocation, &moduleNamespace,
                               SkipCheckForBrokenURLOrZeroSized::Yes);
  if (rv == NS_ERROR_FILE_NOT_FOUND || rv == NS_ERROR_FILE_ACCESS_DENIED) {
    // The error for ESModule shouldn't be exposed if the file does not exist,
    // or the access is blocked by sandbox.
    if (JS_IsExceptionPending(aCx)) {
      JS_ClearPendingException(aCx);
    }
  }
  NS_ENSURE_SUCCESS(rv, rv);

  JS::RootedObject globalProxy(aCx);
  {
    JSAutoRealm ar(aCx, moduleNamespace);

    JS::RootedObject moduleObject(
        aCx, JS::GetModuleForNamespace(aCx, moduleNamespace));
    if (!moduleObject) {
      return NS_ERROR_FAILURE;
    }

    globalProxy = CreateModuleEnvironmentProxy(aCx, moduleObject);
    if (!globalProxy) {
      return NS_ERROR_FAILURE;
    }

    // Cache the redirect to use in subsequent imports.
    ModuleLoaderInfo info(aLocation);
    auto newEntry = MakeUnique<FallbackModuleEntry>(RootingContext::get(aCx));
    newEntry->globalProxy = globalProxy;
    newEntry->moduleNamespace = moduleNamespace;
    mFallbackImports.InsertOrUpdate(info.Key(), std::move(newEntry));
  }

  if (!JS_WrapObject(aCx, &globalProxy)) {
    return NS_ERROR_FAILURE;
  }
  aModuleGlobal.set(globalProxy);

  if (!aIgnoreExports) {
    JS::RootedObject exports(aCx, moduleNamespace);
    if (!JS_WrapObject(aCx, &exports)) {
      return NS_ERROR_FAILURE;
    }
    aModuleExports.set(exports);
  }

  return NS_OK;
}

nsresult mozJSModuleLoader::TryCachedFallbackToImportESModule(
    JSContext* aCx, const nsACString& aLocation,
    JS::MutableHandleObject aModuleGlobal,
    JS::MutableHandleObject aModuleExports, bool aIgnoreExports, bool* aFound) {
  ModuleLoaderInfo info(aLocation);
  FallbackModuleEntry* fallbackMod;
  if (!mFallbackImports.Get(info.Key(), &fallbackMod)) {
    *aFound = false;
    return NS_OK;
  }

  JS::RootedObject globalProxy(aCx, fallbackMod->globalProxy);
  if (!JS_WrapObject(aCx, &globalProxy)) {
    return NS_ERROR_FAILURE;
  }
  aModuleGlobal.set(globalProxy);

  if (!aIgnoreExports) {
    JS::RootedObject exports(aCx, fallbackMod->moduleNamespace);
    if (!JS_WrapObject(aCx, &exports)) {
      return NS_ERROR_FAILURE;
    }
    aModuleExports.set(exports);
  }

  *aFound = true;
  return NS_OK;
}

nsresult mozJSModuleLoader::ImportESModule(
    JSContext* aCx, const nsACString& aLocation,
    JS::MutableHandleObject aModuleNamespace,
    SkipCheckForBrokenURLOrZeroSized
        aSkipCheck /* = SkipCheckForBrokenURLOrZeroSized::No */) {
  using namespace JS::loader;

  mInitialized = true;

  // Called from ChromeUtils::ImportESModule.
  nsCString str(aLocation);

  AUTO_PROFILER_MARKER_TEXT(
      "ChromeUtils.importESModule", JS,
      MarkerOptions(MarkerStack::Capture(),
                    MarkerInnerWindowIdFromJSContext(aCx)),
      Substring(aLocation, 0, std::min(size_t(128), aLocation.Length())));

  RootedObject globalObj(aCx, GetSharedGlobal(aCx));
  NS_ENSURE_TRUE(globalObj, NS_ERROR_FAILURE);
  MOZ_ASSERT(xpc::Scriptability::Get(globalObj).Allowed());

  // The module loader should be instantiated when fetching the shared global
  MOZ_ASSERT(mModuleLoader);

  JSAutoRealm ar(aCx, globalObj);

  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), aLocation);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIPrincipal> principal =
      mModuleLoader->GetGlobalObject()->PrincipalOrNull();
  MOZ_ASSERT(principal);

  RefPtr<ScriptFetchOptions> options = new ScriptFetchOptions(
      CORS_NONE, /* aNonce = */ u""_ns, dom::RequestPriority::Auto,
      ParserMetadata::NotParserInserted, principal);

  RefPtr<ComponentLoadContext> context = new ComponentLoadContext();
  context->mSkipCheck = aSkipCheck;

  RefPtr<VisitedURLSet> visitedSet =
      ModuleLoadRequest::NewVisitedSetForTopLevelImport(uri);

  RefPtr<ModuleLoadRequest> request = new ModuleLoadRequest(
      uri, dom::ReferrerPolicy::No_referrer, options, dom::SRIMetadata(),
      /* aReferrer = */ nullptr, context,
      /* aIsTopLevel = */ true,
      /* aIsDynamicImport = */ false, mModuleLoader, visitedSet, nullptr);

  request->NoCacheEntryFound();

  rv = request->StartModuleLoad();
  if (NS_FAILED(rv)) {
    mModuleLoader->MaybeReportLoadError(aCx);
    return rv;
  }

  rv = mModuleLoader->ProcessRequests();
  if (NS_FAILED(rv)) {
    mModuleLoader->MaybeReportLoadError(aCx);
    return rv;
  }

  MOZ_ASSERT(request->IsFinished());
  if (!request->mModuleScript) {
    mModuleLoader->MaybeReportLoadError(aCx);
    return NS_ERROR_FAILURE;
  }

  // All modules are loaded. MaybeReportLoadError isn't necessary from here.

  if (!request->InstantiateModuleGraph()) {
    return NS_ERROR_FAILURE;
  }

  rv = mModuleLoader->EvaluateModuleInContext(aCx, request,
                                              JS::ThrowModuleErrorsSync);
  NS_ENSURE_SUCCESS(rv, rv);
  if (JS_IsExceptionPending(aCx)) {
    return NS_ERROR_FAILURE;
  }

  RefPtr<ModuleScript> moduleScript = request->mModuleScript;
  JS::Rooted<JSObject*> module(aCx, moduleScript->ModuleRecord());
  aModuleNamespace.set(JS::GetModuleNamespace(aCx, module));

  return NS_OK;
}

nsresult mozJSModuleLoader::Unload(const nsACString& aLocation) {
  if (!mInitialized) {
    return NS_OK;
  }

  ModuleLoaderInfo info(aLocation);

  ModuleEntry* mod;
  if (mImports.Get(info.Key(), &mod)) {
    mLocations.Remove(mod->resolvedURL);
    mImports.Remove(info.Key());
  }

  // If this is the last module to be unloaded, we will leak mLoaderGlobal
  // until UnloadModules is called. So be it.

  return NS_OK;
}

bool mozJSModuleLoader::CreateJSServices(JSContext* aCx) {
  JSObject* services = NewJSServices(aCx);
  if (!services) {
    return false;
  }

  mServicesObj = services;
  return true;
}

bool mozJSModuleLoader::DefineJSServices(JSContext* aCx,
                                         JS::Handle<JSObject*> aGlobal) {
  if (!mServicesObj) {
    // This function is called whenever creating a new global that needs
    // `Services`, including the loader's shared global.
    //
    // This function is no-op if it's called during creating the loader's
    // shared global.
    //
    // See also  CreateAndDefineJSServices.
    MOZ_ASSERT(!mLoaderGlobal);
    MOZ_ASSERT(mIsInitializingLoaderGlobal);
    return true;
  }

  JS::Rooted<JS::Value> services(aCx, ObjectValue(*mServicesObj));
  if (!JS_WrapValue(aCx, &services)) {
    return false;
  }

  JS::Rooted<JS::PropertyKey> servicesId(
      aCx, XPCJSContext::Get()->GetStringID(XPCJSContext::IDX_SERVICES));
  return JS_DefinePropertyById(aCx, aGlobal, servicesId, services, 0);
}

size_t mozJSModuleLoader::ModuleEntry::SizeOfIncludingThis(
    MallocSizeOf aMallocSizeOf) const {
  size_t n = aMallocSizeOf(this);
  n += aMallocSizeOf(location);

  return n;
}

//----------------------------------------------------------------------

JSCLContextHelper::JSCLContextHelper(JSContext* aCx)
    : mContext(aCx), mBuf(nullptr) {}

JSCLContextHelper::~JSCLContextHelper() {
  if (mBuf) {
    JS_ReportErrorUTF8(mContext, "%s", mBuf.get());
  }
}

void JSCLContextHelper::reportErrorAfterPop(UniqueChars&& buf) {
  MOZ_ASSERT(!mBuf, "Already called reportErrorAfterPop");
  mBuf = std::move(buf);
}
