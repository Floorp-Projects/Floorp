/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GeckoProfiler.h"
#include "LoadedScript.h"
#include "ModuleLoadRequest.h"
#include "ScriptLoadRequest.h"
#include "mozilla/dom/ScriptTrace.h"

#include "js/Array.h"  // JS::GetArrayLength
#include "js/CompilationAndEvaluation.h"
#include "js/ContextOptions.h"        // JS::ContextOptionsRef
#include "js/friend/ErrorMessages.h"  // js::GetErrorMessage, JSMSG_*
#include "js/Modules.h"  // JS::FinishDynamicModuleImport, JS::{G,S}etModuleResolveHook, JS::Get{ModulePrivate,ModuleScript,RequestedModule{s,Specifier,SourcePos}}, JS::SetModule{DynamicImport,Metadata}Hook
#include "js/OffThreadScriptCompilation.h"
#include "js/PropertyAndElement.h"  // JS_DefineProperty, JS_GetElement
#include "js/SourceText.h"
#include "mozilla/dom/AutoEntryScript.h"
#include "mozilla/CycleCollectedJSContext.h"  // nsAutoMicroTask
#include "nsContentUtils.h"
#include "nsICacheInfoChannel.h"  //nsICacheInfoChannel
#include "nsNetUtil.h"            // NS_NewURI

using JS::SourceText;

namespace JS::loader {

mozilla::LazyLogModule ModuleLoaderBase::gCspPRLog("CSP");
mozilla::LazyLogModule ModuleLoaderBase::gModuleLoaderBaseLog(
    "ModuleLoaderBase");

#undef LOG
#define LOG(args)                                                           \
  MOZ_LOG(ModuleLoaderBase::gModuleLoaderBaseLog, mozilla::LogLevel::Debug, \
          args)

#define LOG_ENABLED() \
  MOZ_LOG_TEST(ModuleLoaderBase::gModuleLoaderBaseLog, mozilla::LogLevel::Debug)

//////////////////////////////////////////////////////////////
// ModuleLoaderBase::mFetchingModules / ModuleLoaderBase::mFetchingModules
//////////////////////////////////////////////////////////////

inline void ImplCycleCollectionUnlink(
    nsRefPtrHashtable<ModuleMapKey,
                      mozilla::GenericNonExclusivePromise::Private>& aField) {
  for (auto iter = aField.Iter(); !iter.Done(); iter.Next()) {
    ImplCycleCollectionUnlink(iter.Key());

    RefPtr<mozilla::GenericNonExclusivePromise::Private> promise =
        iter.UserData();
    if (promise) {
      promise->Reject(NS_ERROR_ABORT, __func__);
    }
  }

  aField.Clear();
}

inline void ImplCycleCollectionTraverse(
    nsCycleCollectionTraversalCallback& aCallback,
    nsRefPtrHashtable<ModuleMapKey,
                      mozilla::GenericNonExclusivePromise::Private>& aField,
    const char* aName, uint32_t aFlags = 0) {
  for (auto iter = aField.Iter(); !iter.Done(); iter.Next()) {
    ImplCycleCollectionTraverse(aCallback, iter.Key(), "mFetchingModules key",
                                aFlags);
  }
}

inline void ImplCycleCollectionUnlink(
    nsRefPtrHashtable<ModuleMapKey, ModuleScript>& aField) {
  for (auto iter = aField.Iter(); !iter.Done(); iter.Next()) {
    ImplCycleCollectionUnlink(iter.Key());
  }

  aField.Clear();
}

inline void ImplCycleCollectionTraverse(
    nsCycleCollectionTraversalCallback& aCallback,
    nsRefPtrHashtable<ModuleMapKey, ModuleScript>& aField, const char* aName,
    uint32_t aFlags = 0) {
  for (auto iter = aField.Iter(); !iter.Done(); iter.Next()) {
    ImplCycleCollectionTraverse(aCallback, iter.Key(), "mFetchedModules key",
                                aFlags);
    CycleCollectionNoteChild(aCallback, iter.UserData(), "mFetchedModules data",
                             aFlags);
  }
}

//////////////////////////////////////////////////////////////
// ModuleLoaderBase
//////////////////////////////////////////////////////////////

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ModuleLoaderBase)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION(ModuleLoaderBase, mFetchingModules, mFetchedModules,
                         mDynamicImportRequests, mLoader)

NS_IMPL_CYCLE_COLLECTING_ADDREF(ModuleLoaderBase)
NS_IMPL_CYCLE_COLLECTING_RELEASE(ModuleLoaderBase)

bool ModuleLoaderBase::ModuleMapContainsURL(nsIURI* aURL,
                                            nsIGlobalObject* aGlobal) const {
  // Returns whether we have fetched, or are currently fetching, a module script
  // for a URL.
  ModuleMapKey key(aURL, aGlobal);
  return mFetchingModules.Contains(key) || mFetchedModules.Contains(key);
}

void ModuleLoaderBase::SetModuleFetchStarted(ModuleLoadRequest* aRequest) {
  // Update the module map to indicate that a module is currently being fetched.

  MOZ_ASSERT(aRequest->IsLoading());
  MOZ_ASSERT(!ModuleMapContainsURL(aRequest->mURI,
                                   aRequest->mLoadContext->GetWebExtGlobal()));
  ModuleMapKey key(aRequest->mURI, aRequest->mLoadContext->GetWebExtGlobal());
  mFetchingModules.InsertOrUpdate(
      key, RefPtr<mozilla::GenericNonExclusivePromise::Private>{});
}

void ModuleLoaderBase::SetModuleFetchFinishedAndResumeWaitingRequests(
    ModuleLoadRequest* aRequest, nsresult aResult) {
  // Update module map with the result of fetching a single module script.
  //
  // If any requests for the same URL are waiting on this one to complete, they
  // will have ModuleLoaded or LoadFailed on them when the promise is
  // resolved/rejected. This is set up in StartLoad.

  LOG(
      ("ScriptLoadRequest (%p): Module fetch finished (script == %p, result == "
       "%u)",
       aRequest, aRequest->mModuleScript.get(), unsigned(aResult)));

  ModuleMapKey key(aRequest->mURI, aRequest->mLoadContext->GetWebExtGlobal());
  RefPtr<mozilla::GenericNonExclusivePromise::Private> promise;
  if (!mFetchingModules.Remove(key, getter_AddRefs(promise))) {
    LOG(
        ("ScriptLoadRequest (%p): Key not found in mFetchingModules, "
         "assuming we have an inline module or have finished fetching already",
         aRequest));
    return;
  }

  RefPtr<ModuleScript> moduleScript(aRequest->mModuleScript);
  MOZ_ASSERT(NS_FAILED(aResult) == !moduleScript);

  mFetchedModules.InsertOrUpdate(key, RefPtr{moduleScript});

  if (promise) {
    if (moduleScript) {
      LOG(("ScriptLoadRequest (%p):   resolving %p", aRequest, promise.get()));
      promise->Resolve(true, __func__);
    } else {
      LOG(("ScriptLoadRequest (%p):   rejecting %p", aRequest, promise.get()));
      promise->Reject(aResult, __func__);
    }
  }
}

RefPtr<mozilla::GenericNonExclusivePromise>
ModuleLoaderBase::WaitForModuleFetch(nsIURI* aURL, nsIGlobalObject* aGlobal) {
  MOZ_ASSERT(ModuleMapContainsURL(aURL, aGlobal));

  ModuleMapKey key(aURL, aGlobal);
  if (auto entry = mFetchingModules.Lookup(key)) {
    if (!entry.Data()) {
      entry.Data() = new mozilla::GenericNonExclusivePromise::Private(__func__);
    }
    return entry.Data();
  }

  RefPtr<ModuleScript> ms;
  MOZ_ALWAYS_TRUE(mFetchedModules.Get(key, getter_AddRefs(ms)));
  if (!ms) {
    return mozilla::GenericNonExclusivePromise::CreateAndReject(
        NS_ERROR_FAILURE, __func__);
  }

  return mozilla::GenericNonExclusivePromise::CreateAndResolve(true, __func__);
}

ModuleScript* ModuleLoaderBase::GetFetchedModule(
    nsIURI* aURL, nsIGlobalObject* aGlobal) const {
  if (LOG_ENABLED()) {
    nsAutoCString url;
    aURL->GetAsciiSpec(url);
    LOG(("GetFetchedModule %s %p", url.get(), aGlobal));
  }

  bool found;
  ModuleMapKey key(aURL, aGlobal);
  ModuleScript* ms = mFetchedModules.GetWeak(key, &found);
  MOZ_ASSERT(found);
  return ms;
}

nsresult ModuleLoaderBase::ProcessFetchedModuleSource(
    ModuleLoadRequest* aRequest) {
  MOZ_ASSERT(!aRequest->mModuleScript);

  nsresult rv = CreateModuleScript(aRequest);
  MOZ_ASSERT(NS_FAILED(rv) == !aRequest->mModuleScript);

  aRequest->ClearScriptSource();

  if (NS_FAILED(rv)) {
    aRequest->LoadFailed();
    return rv;
  }

  SetModuleFetchFinishedAndResumeWaitingRequests(aRequest, rv);

  if (!aRequest->mModuleScript->HasParseError()) {
    StartFetchingModuleDependencies(aRequest);
  }

  return NS_OK;
}

nsresult ModuleLoaderBase::CreateModuleScript(ModuleLoadRequest* aRequest) {
  MOZ_ASSERT(!aRequest->mModuleScript);
  MOZ_ASSERT(aRequest->mBaseURL);

  LOG(("ScriptLoadRequest (%p): Create module script", aRequest));

  nsCOMPtr<nsIGlobalObject> globalObject =
      mLoader->GetGlobalForRequest(aRequest);
  if (!globalObject) {
    return NS_ERROR_FAILURE;
  }

  mozilla::nsAutoMicroTask mt;

  mozilla::AutoAllowLegacyScriptExecution exemption;

  mozilla::dom::AutoEntryScript aes(globalObject, "CompileModule", true);

  nsresult rv;
  {
    JSContext* cx = aes.cx();
    JS::Rooted<JSObject*> module(cx);
    JS::Rooted<JSObject*> global(cx, globalObject->GetGlobalJSObject());

    JS::CompileOptions options(cx);
    JS::RootedScript introductionScript(cx);
    rv = mLoader->FillCompileOptionsForRequest(cx, aRequest, global, &options,
                                               &introductionScript);

    if (NS_SUCCEEDED(rv)) {
      rv = CompileOrFinishModuleScript(cx, global, options, aRequest, &module);
    }

    MOZ_ASSERT(NS_SUCCEEDED(rv) == (module != nullptr));

    if (module) {
      JS::RootedValue privateValue(cx);
      JS::RootedScript moduleScript(cx, JS::GetModuleScript(module));
      JS::InstantiateOptions instantiateOptions(options);
      if (!JS::UpdateDebugMetadata(cx, moduleScript, instantiateOptions,
                                   privateValue, nullptr, introductionScript,
                                   nullptr)) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
    }

    RefPtr<ModuleScript> moduleScript =
        new ModuleScript(aRequest->mFetchOptions, aRequest->mBaseURL,
                         aRequest->mLoadContext->mElement);
    aRequest->mModuleScript = moduleScript;

    if (!module) {
      LOG(("ScriptLoadRequest (%p):   compilation failed (%d)", aRequest,
           unsigned(rv)));

      MOZ_ASSERT(aes.HasException());
      JS::Rooted<JS::Value> error(cx);
      if (!aes.StealException(&error)) {
        aRequest->mModuleScript = nullptr;
        return NS_ERROR_FAILURE;
      }

      moduleScript->SetParseError(error);
      aRequest->ModuleErrored();
      return NS_OK;
    }

    moduleScript->SetModuleRecord(module);

    // Validate requested modules and treat failure to resolve module specifiers
    // the same as a parse error.
    rv = ResolveRequestedModules(aRequest, nullptr);
    if (NS_FAILED(rv)) {
      aRequest->ModuleErrored();
      return NS_OK;
    }
  }

  LOG(("ScriptLoadRequest (%p):   module script == %p", aRequest,
       aRequest->mModuleScript.get()));

  return rv;
}

nsresult ModuleLoaderBase::HandleResolveFailure(
    JSContext* aCx, LoadedScript* aScript, const nsAString& aSpecifier,
    uint32_t aLineNumber, uint32_t aColumnNumber,
    JS::MutableHandle<JS::Value> errorOut) {
  JS::Rooted<JSString*> filename(aCx);
  if (aScript) {
    nsAutoCString url;
    aScript->BaseURL()->GetAsciiSpec(url);
    filename = JS_NewStringCopyZ(aCx, url.get());
  } else {
    filename = JS_NewStringCopyZ(aCx, "(unknown)");
  }

  if (!filename) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  AutoTArray<nsString, 1> errorParams;
  errorParams.AppendElement(aSpecifier);

  nsAutoString errorText;
  nsresult rv = nsContentUtils::FormatLocalizedString(
      nsContentUtils::eDOM_PROPERTIES, "ModuleResolveFailure", errorParams,
      errorText);
  NS_ENSURE_SUCCESS(rv, rv);

  JS::Rooted<JSString*> string(aCx, JS_NewUCStringCopyZ(aCx, errorText.get()));
  if (!string) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  if (!JS::CreateError(aCx, JSEXN_TYPEERR, nullptr, filename, aLineNumber,
                       aColumnNumber, nullptr, string, JS::NothingHandleValue,
                       errorOut)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_OK;
}

already_AddRefed<nsIURI> ModuleLoaderBase::ResolveModuleSpecifier(
    ScriptLoaderInterface* aLoader, LoadedScript* aScript,
    const nsAString& aSpecifier) {
  // The following module specifiers are allowed by the spec:
  //  - a valid absolute URL
  //  - a valid relative URL that starts with "/", "./" or "../"
  //
  // Bareword module specifiers are currently disallowed as these may be given
  // special meanings in the future.

  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), aSpecifier);
  if (NS_SUCCEEDED(rv)) {
    return uri.forget();
  }

  if (rv != NS_ERROR_MALFORMED_URI) {
    return nullptr;
  }

  if (!StringBeginsWith(aSpecifier, u"/"_ns) &&
      !StringBeginsWith(aSpecifier, u"./"_ns) &&
      !StringBeginsWith(aSpecifier, u"../"_ns)) {
    return nullptr;
  }

  // Get the document's base URL if we don't have a referencing script here.
  nsCOMPtr<nsIURI> baseURL;
  if (aScript) {
    baseURL = aScript->BaseURL();
  } else {
    baseURL = aLoader->GetBaseURI();
  }

  rv = NS_NewURI(getter_AddRefs(uri), aSpecifier, nullptr, baseURL);
  if (NS_SUCCEEDED(rv)) {
    return uri.forget();
  }

  return nullptr;
}

nsresult ModuleLoaderBase::ResolveRequestedModules(
    ModuleLoadRequest* aRequest, nsCOMArray<nsIURI>* aUrlsOut) {
  ModuleScript* ms = aRequest->mModuleScript;

  mozilla::dom::AutoJSAPI jsapi;
  if (!jsapi.Init(ms->ModuleRecord())) {
    return NS_ERROR_FAILURE;
  }

  JSContext* cx = jsapi.cx();
  JS::Rooted<JSObject*> moduleRecord(cx, ms->ModuleRecord());
  JS::Rooted<JSObject*> requestedModules(cx);
  requestedModules = JS::GetRequestedModules(cx, moduleRecord);
  MOZ_ASSERT(requestedModules);

  uint32_t length;
  if (!JS::GetArrayLength(cx, requestedModules, &length)) {
    return NS_ERROR_FAILURE;
  }

  JS::Rooted<JS::Value> element(cx);
  for (uint32_t i = 0; i < length; i++) {
    if (!JS_GetElement(cx, requestedModules, i, &element)) {
      return NS_ERROR_FAILURE;
    }

    JS::Rooted<JSString*> str(cx, JS::GetRequestedModuleSpecifier(cx, element));
    MOZ_ASSERT(str);

    nsAutoJSString specifier;
    if (!specifier.init(cx, str)) {
      return NS_ERROR_FAILURE;
    }

    // Let url be the result of resolving a module specifier given module script
    // and requested.
    ModuleLoaderBase* requestModuleLoader = aRequest->mLoader;
    nsCOMPtr<nsIURI> uri =
        ResolveModuleSpecifier(requestModuleLoader->mLoader, ms, specifier);
    if (!uri) {
      uint32_t lineNumber = 0;
      uint32_t columnNumber = 0;
      JS::GetRequestedModuleSourcePos(cx, element, &lineNumber, &columnNumber);

      JS::Rooted<JS::Value> error(cx);
      nsresult rv = HandleResolveFailure(cx, ms, specifier, lineNumber,
                                         columnNumber, &error);
      NS_ENSURE_SUCCESS(rv, rv);

      ms->SetParseError(error);
      return NS_ERROR_FAILURE;
    }

    if (aUrlsOut) {
      aUrlsOut->AppendElement(uri.forget());
    }
  }

  return NS_OK;
}

void ModuleLoaderBase::StartFetchingModuleDependencies(
    ModuleLoadRequest* aRequest) {
  LOG(("ScriptLoadRequest (%p): Start fetching module dependencies", aRequest));

  if (aRequest->IsCanceled()) {
    return;
  }

  MOZ_ASSERT(aRequest->mModuleScript);
  MOZ_ASSERT(!aRequest->mModuleScript->HasParseError());
  MOZ_ASSERT(!aRequest->IsReadyToRun());

  auto visitedSet = aRequest->mVisitedSet;
  MOZ_ASSERT(visitedSet->Contains(aRequest->mURI));

  aRequest->mProgress = ModuleLoadRequest::Progress::eFetchingImports;

  nsCOMArray<nsIURI> urls;
  nsresult rv = ResolveRequestedModules(aRequest, &urls);
  if (NS_FAILED(rv)) {
    aRequest->mModuleScript = nullptr;
    aRequest->ModuleErrored();
    return;
  }

  // Remove already visited URLs from the list. Put unvisited URLs into the
  // visited set.
  int32_t i = 0;
  while (i < urls.Count()) {
    nsIURI* url = urls[i];
    if (visitedSet->Contains(url)) {
      urls.RemoveObjectAt(i);
    } else {
      visitedSet->PutEntry(url);
      i++;
    }
  }

  if (urls.Count() == 0) {
    // There are no descendants to load so this request is ready.
    aRequest->DependenciesLoaded();
    return;
  }

  // For each url in urls, fetch a module script tree given url, module script's
  // CORS setting, and module script's settings object.
  nsTArray<RefPtr<mozilla::GenericPromise>> importsReady;
  for (auto* url : urls) {
    RefPtr<mozilla::GenericPromise> childReady =
        StartFetchingModuleAndDependencies(aRequest, url);
    importsReady.AppendElement(childReady);
  }

  // Wait for all imports to become ready.
  RefPtr<mozilla::GenericPromise::AllPromiseType> allReady =
      mozilla::GenericPromise::All(mozilla::GetMainThreadSerialEventTarget(),
                                   importsReady);
  allReady->Then(mozilla::GetMainThreadSerialEventTarget(), __func__, aRequest,
                 &ModuleLoadRequest::DependenciesLoaded,
                 &ModuleLoadRequest::ModuleErrored);
}

RefPtr<mozilla::GenericPromise>
ModuleLoaderBase::StartFetchingModuleAndDependencies(ModuleLoadRequest* aParent,
                                                     nsIURI* aURI) {
  MOZ_ASSERT(aURI);

  RefPtr<ModuleLoadRequest> childRequest = CreateStaticImport(aURI, aParent);

  aParent->mImports.AppendElement(childRequest);

  if (LOG_ENABLED()) {
    nsAutoCString url1;
    aParent->mURI->GetAsciiSpec(url1);

    nsAutoCString url2;
    aURI->GetAsciiSpec(url2);

    LOG(("ScriptLoadRequest (%p): Start fetching dependency %p", aParent,
         childRequest.get()));
    LOG(("StartFetchingModuleAndDependencies \"%s\" -> \"%s\"", url1.get(),
         url2.get()));
  }

  RefPtr<mozilla::GenericPromise> ready = childRequest->mReady.Ensure(__func__);

  nsresult rv = StartModuleLoad(childRequest);
  if (NS_FAILED(rv)) {
    MOZ_ASSERT(!childRequest->mModuleScript);
    LOG(("ScriptLoadRequest (%p):   rejecting %p", aParent,
         &childRequest->mReady));

    mLoader->ReportErrorToConsole(childRequest, rv);
    childRequest->mReady.Reject(rv, __func__);
    return ready;
  }

  return ready;
}

void ModuleLoaderBase::StartDynamicImport(ModuleLoadRequest* aRequest) {
  LOG(("ScriptLoadRequest (%p): Start dynamic import", aRequest));

  mDynamicImportRequests.AppendElement(aRequest);

  nsresult rv = StartModuleLoad(aRequest);
  if (NS_FAILED(rv)) {
    mLoader->ReportErrorToConsole(aRequest, rv);
    FinishDynamicImportAndReject(aRequest, rv);
  }
}

void ModuleLoaderBase::FinishDynamicImportAndReject(ModuleLoadRequest* aRequest,
                                                    nsresult aResult) {
  mozilla::dom::AutoJSAPI jsapi;
  MOZ_ASSERT(NS_FAILED(aResult));
  MOZ_ALWAYS_TRUE(jsapi.Init(aRequest->mDynamicPromise));
  FinishDynamicImport(jsapi.cx(), aRequest, aResult, nullptr);
}

void ModuleLoaderBase::FinishDynamicImport(
    JSContext* aCx, ModuleLoadRequest* aRequest, nsresult aResult,
    JS::Handle<JSObject*> aEvaluationPromise) {
  // If aResult is a failed result, we don't have an EvaluationPromise. If it
  // succeeded, evaluationPromise may still be null, but in this case it will
  // be handled by rejecting the dynamic module import promise in the JSAPI.
  MOZ_ASSERT_IF(NS_FAILED(aResult), !aEvaluationPromise);
  LOG(("ScriptLoadRequest (%p): Finish dynamic import %x %d", aRequest,
       unsigned(aResult), JS_IsExceptionPending(aCx)));

  // Complete the dynamic import, report failures indicated by aResult or as a
  // pending exception on the context.

  if (NS_FAILED(aResult) &&
      aResult != NS_SUCCESS_DOM_SCRIPT_EVALUATION_THREW_UNCATCHABLE) {
    MOZ_ASSERT(!JS_IsExceptionPending(aCx));
    JS_ReportErrorNumberUC(aCx, js::GetErrorMessage, nullptr,
                           JSMSG_DYNAMIC_IMPORT_FAILED);
  }

  JS::Rooted<JS::Value> referencingScript(aCx,
                                          aRequest->mDynamicReferencingPrivate);
  JS::Rooted<JSString*> specifier(aCx, aRequest->mDynamicSpecifier);
  JS::Rooted<JSObject*> promise(aCx, aRequest->mDynamicPromise);

  JS::Rooted<JSObject*> moduleRequest(aCx,
                                      JS::CreateModuleRequest(aCx, specifier));

  JS::FinishDynamicModuleImport(aCx, aEvaluationPromise, referencingScript,
                                moduleRequest, promise);

  // FinishDynamicModuleImport clears any pending exception.
  MOZ_ASSERT(!JS_IsExceptionPending(aCx));

  aRequest->ClearDynamicImport();
}

ModuleLoaderBase::ModuleLoaderBase(ScriptLoaderInterface* aLoader)
    : mLoader(aLoader) {}

ModuleLoaderBase::~ModuleLoaderBase() {
  mDynamicImportRequests.CancelRequestsAndClear();

  LOG(("ModuleLoaderBase::~ModuleLoaderBase %p", this));
}

JS::Value ModuleLoaderBase::FindFirstParseError(ModuleLoadRequest* aRequest) {
  MOZ_ASSERT(aRequest);

  ModuleScript* moduleScript = aRequest->mModuleScript;
  MOZ_ASSERT(moduleScript);

  if (moduleScript->HasParseError()) {
    return moduleScript->ParseError();
  }

  for (ModuleLoadRequest* childRequest : aRequest->mImports) {
    JS::Value error = FindFirstParseError(childRequest);
    if (!error.isUndefined()) {
      return error;
    }
  }

  return JS::UndefinedValue();
}

bool ModuleLoaderBase::InstantiateModuleTree(ModuleLoadRequest* aRequest) {
  // Instantiate a top-level module and record any error.

  MOZ_ASSERT(aRequest);
  MOZ_ASSERT(aRequest->IsTopLevel());

  LOG(("ScriptLoadRequest (%p): Instantiate module tree", aRequest));

  ModuleScript* moduleScript = aRequest->mModuleScript;
  MOZ_ASSERT(moduleScript);

  JS::Value parseError = FindFirstParseError(aRequest);
  if (!parseError.isUndefined()) {
    moduleScript->SetErrorToRethrow(parseError);
    LOG(("ScriptLoadRequest (%p):   found parse error", aRequest));
    return true;
  }

  MOZ_ASSERT(moduleScript->ModuleRecord());

  mozilla::nsAutoMicroTask mt;
  mozilla::dom::AutoJSAPI jsapi;
  if (NS_WARN_IF(!jsapi.Init(moduleScript->ModuleRecord()))) {
    return false;
  }

  JS::Rooted<JSObject*> module(jsapi.cx(), moduleScript->ModuleRecord());
  bool ok = NS_SUCCEEDED(nsJSUtils::ModuleInstantiate(jsapi.cx(), module));

  if (!ok) {
    LOG(("ScriptLoadRequest (%p): Instantiate failed", aRequest));
    MOZ_ASSERT(jsapi.HasException());
    JS::RootedValue exception(jsapi.cx());
    if (!jsapi.StealException(&exception)) {
      return false;
    }
    MOZ_ASSERT(!exception.isUndefined());
    moduleScript->SetErrorToRethrow(exception);
  }

  return true;
}

nsresult ModuleLoaderBase::InitDebuggerDataForModuleTree(
    JSContext* aCx, ModuleLoadRequest* aRequest) {
  // JS scripts can be associated with a DOM element for use by the debugger,
  // but preloading can cause scripts to be compiled before DOM script element
  // nodes have been created. This method ensures that this association takes
  // place before the first time a module script is run.

  MOZ_ASSERT(aRequest);

  ModuleScript* moduleScript = aRequest->mModuleScript;
  if (moduleScript->DebuggerDataInitialized()) {
    return NS_OK;
  }

  for (ModuleLoadRequest* childRequest : aRequest->mImports) {
    nsresult rv = InitDebuggerDataForModuleTree(aCx, childRequest);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  JS::Rooted<JSObject*> module(aCx, moduleScript->ModuleRecord());
  MOZ_ASSERT(module);

  // The script is now ready to be exposed to the debugger.
  JS::Rooted<JSScript*> script(aCx, JS::GetModuleScript(module));
  JS::ExposeScriptToDebugger(aCx, script);

  moduleScript->SetDebuggerDataInitialized();
  return NS_OK;
}

void ModuleLoaderBase::ProcessDynamicImport(ModuleLoadRequest* aRequest) {
  if (aRequest->mModuleScript) {
    if (!InstantiateModuleTree(aRequest)) {
      aRequest->mModuleScript = nullptr;
    }
  }

  nsresult rv = NS_ERROR_FAILURE;
  if (aRequest->mModuleScript) {
    rv = EvaluateModule(aRequest);
  }

  if (NS_FAILED(rv)) {
    FinishDynamicImportAndReject(aRequest, rv);
  }
}

nsresult ModuleLoaderBase::EvaluateModule(nsIGlobalObject* aGlobalObject,
                                          ScriptLoadRequest* aRequest) {
  mozilla::nsAutoMicroTask mt;
  mozilla::dom::AutoEntryScript aes(aGlobalObject, "EvaluateModule", true);
  JSContext* cx = aes.cx();

  nsAutoCString profilerLabelString;
  if (aRequest->HasLoadContext()) {
    aRequest->GetLoadContext()->GetProfilerLabel(profilerLabelString);
  }

  LOG(("ScriptLoadRequest (%p): Evaluate Module", aRequest));
  AUTO_PROFILER_MARKER_TEXT("ModuleEvaluation", JS,
                            MarkerInnerWindowIdFromJSContext(cx),
                            profilerLabelString);

  // When a module is already loaded, it is not feched a second time and the
  // mDataType of the request might remain set to DataType::Unknown.
  MOZ_ASSERT(aRequest->IsTextSource() || aRequest->IsUnknownDataType());

  ModuleLoadRequest* request = aRequest->AsModuleRequest();
  MOZ_ASSERT(request->mModuleScript);
  MOZ_ASSERT_IF(request->HasLoadContext(),
                !request->GetLoadContext()->mOffThreadToken);

  ModuleScript* moduleScript = request->mModuleScript;
  if (moduleScript->HasErrorToRethrow()) {
    LOG(("ScriptLoadRequest (%p):   module has error to rethrow", aRequest));
    JS::Rooted<JS::Value> error(cx, moduleScript->ErrorToRethrow());
    JS_SetPendingException(cx, error);
    // For a dynamic import, the promise is rejected.  Otherwise an error
    // is either reported by AutoEntryScript.
    if (request->IsDynamicImport()) {
      FinishDynamicImport(cx, request, NS_OK, nullptr);
    }
    return NS_OK;
  }

  JS::Rooted<JSObject*> module(cx, moduleScript->ModuleRecord());
  MOZ_ASSERT(module);

  nsresult rv = InitDebuggerDataForModuleTree(cx, request);
  NS_ENSURE_SUCCESS(rv, rv);

  if (request->HasLoadContext()) {
    TRACE_FOR_TEST(aRequest->GetLoadContext()->GetScriptElement(),
                   "scriptloader_evaluate_module");
  }

  JS::Rooted<JS::Value> rval(cx);

  rv = nsJSUtils::ModuleEvaluate(cx, module, &rval);

  if (NS_SUCCEEDED(rv)) {
    // If we have an infinite loop in a module, which is stopped by the
    // user, the module evaluation will fail, but we will not have an
    // AutoEntryScript exception.
    MOZ_ASSERT(!aes.HasException());
  }

  if (NS_FAILED(rv)) {
    LOG(("ScriptLoadRequest (%p):   evaluation failed", aRequest));
    // For a dynamic import, the promise is rejected.  Otherwise an error is
    // either reported by AutoEntryScript.
    rv = NS_OK;
  }

  JS::Rooted<JSObject*> aEvaluationPromise(cx);
  if (rval.isObject()) {
    // If the user cancels the evaluation on an infinite loop, we need
    // to skip this step. In that case, ModuleEvaluate will not return a
    // promise, rval will be undefined. We should treat it as a failed
    // evaluation, and reject appropriately.
    aEvaluationPromise.set(&rval.toObject());
  }
  if (request->IsDynamicImport()) {
    FinishDynamicImport(cx, request, rv, aEvaluationPromise);
  } else {
    // If this is not a dynamic import, and if the promise is rejected,
    // the value is unwrapped from the promise value.
    if (!JS::ThrowOnModuleEvaluationFailure(cx, aEvaluationPromise)) {
      LOG(("ScriptLoadRequest (%p):   evaluation failed on throw", aRequest));
      // For a dynamic import, the promise is rejected.  Otherwise an
      // error is either reported by AutoEntryScript.
      rv = NS_OK;
    }
  }

  if (aRequest->HasLoadContext()) {
    TRACE_FOR_TEST_NONE(aRequest->GetLoadContext()->GetScriptElement(),
                        "scriptloader_no_encode");
  }
  aRequest->mCacheInfo = nullptr;
  return rv;
}

nsresult ModuleLoaderBase::EvaluateModule(ScriptLoadRequest* aRequest) {
  nsCOMPtr<nsIGlobalObject> globalObject =
      mLoader->GetGlobalForRequest(aRequest);
  if (!globalObject) {
    return NS_ERROR_FAILURE;
  }

  return EvaluateModule(globalObject, aRequest);
}

void ModuleLoaderBase::CancelAndClearDynamicImports() {
  for (ScriptLoadRequest* req = mDynamicImportRequests.getFirst(); req;
       req = req->getNext()) {
    req->Cancel();
    // FinishDynamicImport must happen exactly once for each dynamic import
    // request. If the load is aborted we do it when we remove the request
    // from mDynamicImportRequests.
    FinishDynamicImportAndReject(req->AsModuleRequest(), NS_ERROR_ABORT);
  }
  mDynamicImportRequests.CancelRequestsAndClear();
}

#undef LOG
#undef LOG_ENABLED

}  // namespace JS::loader
