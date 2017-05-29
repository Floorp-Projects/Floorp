/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScriptLoader.h"
#include "ScriptLoadHandler.h"
#include "ScriptLoadRequest.h"
#include "ScriptTrace.h"
#include "ModuleLoadRequest.h"
#include "ModuleScript.h"

#include "prsystem.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "js/Utility.h"
#include "xpcpublic.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIContent.h"
#include "nsJSUtils.h"
#include "mozilla/dom/DocGroup.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/SRILogHelper.h"
#include "nsGkAtoms.h"
#include "nsNetUtil.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptContext.h"
#include "nsIScriptSecurityManager.h"
#include "nsIPrincipal.h"
#include "nsJSPrincipals.h"
#include "nsContentPolicyUtils.h"
#include "nsIHttpChannel.h"
#include "nsIHttpChannelInternal.h"
#include "nsIClassOfService.h"
#include "nsICacheInfoChannel.h"
#include "nsITimedChannel.h"
#include "nsIScriptElement.h"
#include "nsIDOMHTMLScriptElement.h"
#include "nsIDocShell.h"
#include "nsContentUtils.h"
#include "nsUnicharUtils.h"
#include "nsAutoPtr.h"
#include "nsIXPConnect.h"
#include "nsError.h"
#include "nsThreadUtils.h"
#include "nsDocShellCID.h"
#include "nsIContentSecurityPolicy.h"
#include "mozilla/Logging.h"
#include "nsCRT.h"
#include "nsContentCreatorFunctions.h"
#include "nsProxyRelease.h"
#include "nsSandboxFlags.h"
#include "nsContentTypeParser.h"
#include "nsINetworkPredictor.h"
#include "ImportManager.h"
#include "mozilla/dom/EncodingUtils.h"
#include "mozilla/ConsoleReportCollector.h"

#include "mozilla/AsyncEventDispatcher.h"
#include "mozilla/Attributes.h"
#include "mozilla/Telemetry.h"
#include "mozilla/Unused.h"
#include "nsIScriptError.h"
#include "nsIOutputStream.h"

using JS::SourceBufferHolder;

namespace mozilla {
namespace dom {

LazyLogModule ScriptLoader::gCspPRLog("CSP");
LazyLogModule ScriptLoader::gScriptLoaderLog("ScriptLoader");

#define LOG(args) \
  MOZ_LOG(gScriptLoaderLog, mozilla::LogLevel::Debug, args)

// These are the Alternate Data MIME type used by the ScriptLoader to
// register and read bytecode out of the nsCacheInfoChannel.
static NS_NAMED_LITERAL_CSTRING(
  kBytecodeMimeType, "javascript/moz-bytecode-" NS_STRINGIFY(MOZ_BUILDID));
static NS_NAMED_LITERAL_CSTRING(kNullMimeType, "javascript/null");

//////////////////////////////////////////////////////////////
// ScriptLoader::PreloadInfo
//////////////////////////////////////////////////////////////

inline void
ImplCycleCollectionUnlink(ScriptLoader::PreloadInfo& aField)
{
  ImplCycleCollectionUnlink(aField.mRequest);
}

inline void
ImplCycleCollectionTraverse(nsCycleCollectionTraversalCallback& aCallback,
                            ScriptLoader::PreloadInfo& aField,
                            const char* aName,
                            uint32_t aFlags = 0)
{
  ImplCycleCollectionTraverse(aCallback, aField.mRequest, aName, aFlags);
}

//////////////////////////////////////////////////////////////
// ScriptLoader
//////////////////////////////////////////////////////////////

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ScriptLoader)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION(ScriptLoader,
                         mNonAsyncExternalScriptInsertedRequests,
                         mLoadingAsyncRequests,
                         mLoadedAsyncRequests,
                         mDeferRequests,
                         mXSLTRequests,
                         mParserBlockingRequest,
                         mPreloads,
                         mPendingChildLoaders,
                         mFetchedModules)

NS_IMPL_CYCLE_COLLECTING_ADDREF(ScriptLoader)
NS_IMPL_CYCLE_COLLECTING_RELEASE(ScriptLoader)

ScriptLoader::ScriptLoader(nsIDocument *aDocument)
  : mDocument(aDocument),
    mParserBlockingBlockerCount(0),
    mBlockerCount(0),
    mNumberOfProcessors(0),
    mEnabled(true),
    mDeferEnabled(false),
    mDocumentParsingDone(false),
    mBlockingDOMContentLoaded(false),
    mLoadEventFired(false),
    mReporter(new ConsoleReportCollector())
{
}

ScriptLoader::~ScriptLoader()
{
  mObservers.Clear();

  if (mParserBlockingRequest) {
    mParserBlockingRequest->FireScriptAvailable(NS_ERROR_ABORT);
  }

  for (ScriptLoadRequest* req = mXSLTRequests.getFirst(); req;
       req = req->getNext()) {
    req->FireScriptAvailable(NS_ERROR_ABORT);
  }

  for (ScriptLoadRequest* req = mDeferRequests.getFirst(); req;
       req = req->getNext()) {
    req->FireScriptAvailable(NS_ERROR_ABORT);
  }

  for (ScriptLoadRequest* req = mLoadingAsyncRequests.getFirst(); req;
       req = req->getNext()) {
    req->FireScriptAvailable(NS_ERROR_ABORT);
  }

  for (ScriptLoadRequest* req = mLoadedAsyncRequests.getFirst(); req;
       req = req->getNext()) {
    req->FireScriptAvailable(NS_ERROR_ABORT);
  }

  for(ScriptLoadRequest* req = mNonAsyncExternalScriptInsertedRequests.getFirst();
      req;
      req = req->getNext()) {
    req->FireScriptAvailable(NS_ERROR_ABORT);
  }

  // Unblock the kids, in case any of them moved to a different document
  // subtree in the meantime and therefore aren't actually going away.
  for (uint32_t j = 0; j < mPendingChildLoaders.Length(); ++j) {
    mPendingChildLoaders[j]->RemoveParserBlockingScriptExecutionBlocker();
  }
}

// Helper method for checking if the script element is an event-handler
// This means that it has both a for-attribute and a event-attribute.
// Also, if the for-attribute has a value that matches "\s*window\s*",
// and the event-attribute matches "\s*onload([ \(].*)?" then it isn't an
// eventhandler. (both matches are case insensitive).
// This is how IE seems to filter out a window's onload handler from a
// <script for=... event=...> element.

static bool
IsScriptEventHandler(nsIContent* aScriptElement)
{
  if (!aScriptElement->IsHTMLElement()) {
    return false;
  }

  nsAutoString forAttr, eventAttr;
  if (!aScriptElement->GetAttr(kNameSpaceID_None, nsGkAtoms::_for, forAttr) ||
      !aScriptElement->GetAttr(kNameSpaceID_None, nsGkAtoms::event, eventAttr)) {
    return false;
  }

  const nsAString& for_str =
    nsContentUtils::TrimWhitespace<nsCRT::IsAsciiSpace>(forAttr);
  if (!for_str.LowerCaseEqualsLiteral("window")) {
    return true;
  }

  // We found for="window", now check for event="onload".
  const nsAString& event_str =
    nsContentUtils::TrimWhitespace<nsCRT::IsAsciiSpace>(eventAttr, false);
  if (!StringBeginsWith(event_str, NS_LITERAL_STRING("onload"),
                        nsCaseInsensitiveStringComparator())) {
    // It ain't "onload.*".

    return true;
  }

  nsAutoString::const_iterator start, end;
  event_str.BeginReading(start);
  event_str.EndReading(end);

  start.advance(6); // advance past "onload"

  if (start != end && *start != '(' && *start != ' ') {
    // We got onload followed by something other than space or
    // '('. Not good enough.

    return true;
  }

  return false;
}

nsresult
ScriptLoader::CheckContentPolicy(nsIDocument* aDocument,
                                 nsISupports* aContext,
                                 nsIURI* aURI,
                                 const nsAString& aType,
                                 bool aIsPreLoad)
{
  nsContentPolicyType contentPolicyType = aIsPreLoad
                                          ? nsIContentPolicy::TYPE_INTERNAL_SCRIPT_PRELOAD
                                          : nsIContentPolicy::TYPE_INTERNAL_SCRIPT;

  int16_t shouldLoad = nsIContentPolicy::ACCEPT;
  nsresult rv = NS_CheckContentLoadPolicy(contentPolicyType,
                                          aURI,
                                          aDocument->NodePrincipal(),
                                          aContext,
                                          NS_LossyConvertUTF16toASCII(aType),
                                          nullptr,    //extra
                                          &shouldLoad,
                                          nsContentUtils::GetContentPolicy(),
                                          nsContentUtils::GetSecurityManager());
  if (NS_FAILED(rv) || NS_CP_REJECTED(shouldLoad)) {
    if (NS_FAILED(rv) || shouldLoad != nsIContentPolicy::REJECT_TYPE) {
      return NS_ERROR_CONTENT_BLOCKED;
    }
    return NS_ERROR_CONTENT_BLOCKED_SHOW_ALT;
  }

  return NS_OK;
}

bool
ScriptLoader::ModuleScriptsEnabled()
{
  static bool sEnabledForContent = false;
  static bool sCachedPref = false;
  if (!sCachedPref) {
    sCachedPref = true;
    Preferences::AddBoolVarCache(&sEnabledForContent, "dom.moduleScripts.enabled", false);
  }

  return nsContentUtils::IsChromeDoc(mDocument) || sEnabledForContent;
}

bool
ScriptLoader::ModuleMapContainsModule(ModuleLoadRequest* aRequest) const
{
  // Returns whether we have fetched, or are currently fetching, a module script
  // for the request's URL.
  return mFetchingModules.Contains(aRequest->mURI) ||
         mFetchedModules.Contains(aRequest->mURI);
}

bool
ScriptLoader::IsFetchingModule(ModuleLoadRequest* aRequest) const
{
  bool fetching = mFetchingModules.Contains(aRequest->mURI);
  MOZ_ASSERT_IF(fetching, !mFetchedModules.Contains(aRequest->mURI));
  return fetching;
}

void
ScriptLoader::SetModuleFetchStarted(ModuleLoadRequest* aRequest)
{
  // Update the module map to indicate that a module is currently being fetched.

  MOZ_ASSERT(aRequest->IsLoading());
  MOZ_ASSERT(!ModuleMapContainsModule(aRequest));
  mFetchingModules.Put(aRequest->mURI, nullptr);
}

void
ScriptLoader::SetModuleFetchFinishedAndResumeWaitingRequests(ModuleLoadRequest* aRequest,
                                                             nsresult aResult)
{
  // Update module map with the result of fetching a single module script.  The
  // module script pointer is nullptr on error.

  MOZ_ASSERT(!aRequest->IsReadyToRun());

  RefPtr<GenericPromise::Private> promise;
  MOZ_ALWAYS_TRUE(mFetchingModules.Get(aRequest->mURI, getter_AddRefs(promise)));
  mFetchingModules.Remove(aRequest->mURI);

  RefPtr<ModuleScript> ms(aRequest->mModuleScript);
  MOZ_ASSERT(NS_SUCCEEDED(aResult) == (ms != nullptr));
  mFetchedModules.Put(aRequest->mURI, ms);

  if (promise) {
    if (ms) {
      promise->Resolve(true, __func__);
    } else {
      promise->Reject(aResult, __func__);
    }
  }
}

RefPtr<GenericPromise>
ScriptLoader::WaitForModuleFetch(ModuleLoadRequest* aRequest)
{
  MOZ_ASSERT(ModuleMapContainsModule(aRequest));

  RefPtr<GenericPromise::Private> promise;
  if (mFetchingModules.Get(aRequest->mURI, getter_AddRefs(promise))) {
    if (!promise) {
      promise = new GenericPromise::Private(__func__);
      mFetchingModules.Put(aRequest->mURI, promise);
    }
    return promise;
  }

  RefPtr<ModuleScript> ms;
  MOZ_ALWAYS_TRUE(mFetchedModules.Get(aRequest->mURI, getter_AddRefs(ms)));
  if (!ms || ms->InstantiationFailed()) {
    return GenericPromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
  }

  return GenericPromise::CreateAndResolve(true, __func__);
}

ModuleScript*
ScriptLoader::GetFetchedModule(nsIURI* aURL) const
{
  bool found;
  ModuleScript* ms = mFetchedModules.GetWeak(aURL, &found);
  MOZ_ASSERT(found);
  return ms;
}

nsresult
ScriptLoader::ProcessFetchedModuleSource(ModuleLoadRequest* aRequest)
{
  MOZ_ASSERT(!aRequest->mModuleScript);

  nsresult rv = CreateModuleScript(aRequest);
  SetModuleFetchFinishedAndResumeWaitingRequests(aRequest, rv);

  aRequest->mScriptText.clearAndFree();

  if (NS_SUCCEEDED(rv)) {
    StartFetchingModuleDependencies(aRequest);
  }

  return rv;
}

nsresult
ScriptLoader::CreateModuleScript(ModuleLoadRequest* aRequest)
{
  MOZ_ASSERT(!aRequest->mModuleScript);
  MOZ_ASSERT(aRequest->mBaseURL);

  nsCOMPtr<nsIScriptGlobalObject> globalObject = GetScriptGlobalObject();
  if (!globalObject) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIScriptContext> context = globalObject->GetScriptContext();
  if (!context) {
    return NS_ERROR_FAILURE;
  }

  nsAutoMicroTask mt;
  AutoEntryScript aes(globalObject, "CompileModule", true);

  bool oldProcessingScriptTag = context->GetProcessingScriptTag();
  context->SetProcessingScriptTag(true);

  nsresult rv;
  {
    // Update our current script.
    AutoCurrentScriptUpdater scriptUpdater(this, aRequest->mElement);
    Maybe<AutoCurrentScriptUpdater> masterScriptUpdater;
    nsCOMPtr<nsIDocument> master = mDocument->MasterDocument();
    if (master != mDocument) {
      masterScriptUpdater.emplace(master->ScriptLoader(),
                                  aRequest->mElement);
    }

    JSContext* cx = aes.cx();
    JS::Rooted<JSObject*> module(cx);

    if (aRequest->mWasCompiledOMT) {
      module = JS::FinishOffThreadModule(cx, aRequest->mOffThreadToken);
      aRequest->mOffThreadToken = nullptr;
      rv = module ? NS_OK : NS_ERROR_FAILURE;
    } else {
      JS::Rooted<JSObject*> global(cx, globalObject->GetGlobalJSObject());

      JS::CompileOptions options(cx);
      rv = FillCompileOptionsForRequest(aes, aRequest, global, &options);

      if (NS_SUCCEEDED(rv)) {
        nsAutoString inlineData;
        SourceBufferHolder srcBuf = GetScriptSource(aRequest, inlineData);
        rv = nsJSUtils::CompileModule(cx, srcBuf, global, options, &module);
      }
    }
    MOZ_ASSERT(NS_SUCCEEDED(rv) == (module != nullptr));
    if (module) {
      aRequest->mModuleScript =
        new ModuleScript(this, aRequest->mBaseURL, module);
    }
  }

  context->SetProcessingScriptTag(oldProcessingScriptTag);

  return rv;
}

static bool
ThrowTypeError(JSContext* aCx, ModuleScript* aScript,
               const nsString& aMessage)
{
  JS::Rooted<JSObject*> module(aCx, aScript->ModuleRecord());
  JS::Rooted<JSScript*> script(aCx, JS::GetModuleScript(aCx, module));
  JS::Rooted<JSString*> filename(aCx);
  filename = JS_NewStringCopyZ(aCx, JS_GetScriptFilename(script));
  if (!filename) {
    return false;
  }

  JS::Rooted<JSString*> message(aCx, JS_NewUCStringCopyZ(aCx, aMessage.get()));
  if (!message) {
    return false;
  }

  JS::Rooted<JS::Value> error(aCx);
  if (!JS::CreateError(aCx, JSEXN_TYPEERR, nullptr, filename, 0, 0, nullptr,
                       message, &error)) {
    return false;
  }

  JS_SetPendingException(aCx, error);
  return false;
}

static bool
HandleResolveFailure(JSContext* aCx, ModuleScript* aScript,
                     const nsAString& aSpecifier)
{
  // TODO: How can we get the line number of the failed import?

  nsAutoString message(NS_LITERAL_STRING("Error resolving module specifier: "));
  message.Append(aSpecifier);

  return ThrowTypeError(aCx, aScript, message);
}

static bool
HandleModuleNotFound(JSContext* aCx, ModuleScript* aScript,
                     const nsAString& aSpecifier)
{
  // TODO: How can we get the line number of the failed import?

  nsAutoString message(NS_LITERAL_STRING("Resolved module not found in map: "));
  message.Append(aSpecifier);

  return ThrowTypeError(aCx, aScript, message);
}

static already_AddRefed<nsIURI>
ResolveModuleSpecifier(ModuleScript* aScript,
                       const nsAString& aSpecifier)
{
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

  if (!StringBeginsWith(aSpecifier, NS_LITERAL_STRING("/")) &&
      !StringBeginsWith(aSpecifier, NS_LITERAL_STRING("./")) &&
      !StringBeginsWith(aSpecifier, NS_LITERAL_STRING("../"))) {
    return nullptr;
  }

  rv = NS_NewURI(getter_AddRefs(uri), aSpecifier, nullptr, aScript->BaseURL());
  if (NS_SUCCEEDED(rv)) {
    return uri.forget();
  }

  return nullptr;
}

static nsresult
RequestedModuleIsInAncestorList(ModuleLoadRequest* aRequest, nsIURI* aURL, bool* aResult)
{
  const size_t ImportDepthLimit = 100;

  *aResult = false;
  size_t depth = 0;
  while (aRequest) {
    if (depth++ == ImportDepthLimit) {
      return NS_ERROR_FAILURE;
    }

    bool equal;
    nsresult rv = aURL->Equals(aRequest->mURI, &equal);
    NS_ENSURE_SUCCESS(rv, rv);
    if (equal) {
      *aResult = true;
      return NS_OK;
    }

    aRequest = aRequest->mParent;
  }

  return NS_OK;
}

static nsresult
ResolveRequestedModules(ModuleLoadRequest* aRequest, nsCOMArray<nsIURI>& aUrls)
{
  ModuleScript* ms = aRequest->mModuleScript;

  AutoJSAPI jsapi;
  if (!jsapi.Init(ms->ModuleRecord())) {
    return NS_ERROR_FAILURE;
  }

  JSContext* cx = jsapi.cx();
  JS::Rooted<JSObject*> moduleRecord(cx, ms->ModuleRecord());
  JS::Rooted<JSObject*> specifiers(cx, JS::GetRequestedModules(cx, moduleRecord));

  uint32_t length;
  if (!JS_GetArrayLength(cx, specifiers, &length)) {
    return NS_ERROR_FAILURE;
  }

  JS::Rooted<JS::Value> val(cx);
  for (uint32_t i = 0; i < length; i++) {
    if (!JS_GetElement(cx, specifiers, i, &val)) {
      return NS_ERROR_FAILURE;
    }

    nsAutoJSString specifier;
    if (!specifier.init(cx, val)) {
      return NS_ERROR_FAILURE;
    }

    // Let url be the result of resolving a module specifier given module script and requested.
    ModuleScript* ms = aRequest->mModuleScript;
    nsCOMPtr<nsIURI> uri = ResolveModuleSpecifier(ms, specifier);
    if (!uri) {
      HandleResolveFailure(cx, ms, specifier);
      return NS_ERROR_FAILURE;
    }

    bool isAncestor;
    nsresult rv = RequestedModuleIsInAncestorList(aRequest, uri, &isAncestor);
    NS_ENSURE_SUCCESS(rv, rv);
    if (!isAncestor) {
      aUrls.AppendElement(uri.forget());
    }
  }

  return NS_OK;
}

void
ScriptLoader::StartFetchingModuleDependencies(ModuleLoadRequest* aRequest)
{
  MOZ_ASSERT(aRequest->mModuleScript);
  MOZ_ASSERT(!aRequest->mModuleScript->InstantiationFailed());
  MOZ_ASSERT(!aRequest->IsReadyToRun());
  aRequest->mProgress = ModuleLoadRequest::Progress::FetchingImports;

  nsCOMArray<nsIURI> urls;
  nsresult rv = ResolveRequestedModules(aRequest, urls);
  if (NS_FAILED(rv)) {
    aRequest->LoadFailed();
    return;
  }

  if (urls.Length() == 0) {
    // There are no descendents to load so this request is ready.
    aRequest->DependenciesLoaded();
    return;
  }

  // For each url in urls, fetch a module script tree given url, module script's
  // CORS setting, and module script's settings object.
  nsTArray<RefPtr<GenericPromise>> importsReady;
  for (size_t i = 0; i < urls.Length(); i++) {
    RefPtr<GenericPromise> childReady =
      StartFetchingModuleAndDependencies(aRequest, urls[i]);
    importsReady.AppendElement(childReady);
  }

  // Wait for all imports to become ready.
  RefPtr<GenericPromise::AllPromiseType> allReady =
    GenericPromise::All(AbstractThread::MainThread(), importsReady);
  allReady->Then(AbstractThread::MainThread(), __func__, aRequest,
                 &ModuleLoadRequest::DependenciesLoaded,
                 &ModuleLoadRequest::LoadFailed);
}

RefPtr<GenericPromise>
ScriptLoader::StartFetchingModuleAndDependencies(ModuleLoadRequest* aRequest,
                                                 nsIURI* aURI)
{
  MOZ_ASSERT(aURI);

  RefPtr<ModuleLoadRequest> childRequest =
    new ModuleLoadRequest(aRequest->mElement, aRequest->mJSVersion,
                            aRequest->mCORSMode, aRequest->mIntegrity, this);

  childRequest->mIsTopLevel = false;
  childRequest->mURI = aURI;
  childRequest->mIsInline = false;
  childRequest->mReferrerPolicy = aRequest->mReferrerPolicy;
  childRequest->mParent = aRequest;

  RefPtr<GenericPromise> ready = childRequest->mReady.Ensure(__func__);

  nsresult rv = StartLoad(childRequest);
  if (NS_FAILED(rv)) {
    childRequest->mReady.Reject(rv, __func__);
    return ready;
  }

  aRequest->mImports.AppendElement(childRequest);
  return ready;
}

bool
HostResolveImportedModule(JSContext* aCx, unsigned argc, JS::Value* vp)
{
  MOZ_ASSERT(argc == 2);
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  JS::Rooted<JSObject*> module(aCx, &args[0].toObject());
  JS::Rooted<JSString*> specifier(aCx, args[1].toString());

  // Let referencing module script be referencingModule.[[HostDefined]].
  JS::Value value = JS::GetModuleHostDefinedField(module);
  auto script = static_cast<ModuleScript*>(value.toPrivate());
  MOZ_ASSERT(script->ModuleRecord() == module);

  // Let url be the result of resolving a module specifier given referencing
  // module script and specifier. If the result is failure, throw a TypeError
  // exception and abort these steps.
  nsAutoJSString string;
  if (!string.init(aCx, specifier)) {
    return false;
  }

  nsCOMPtr<nsIURI> uri = ResolveModuleSpecifier(script, string);
  if (!uri) {
    return HandleResolveFailure(aCx, script, string);
  }

  // Let resolved module script be the value of the entry in module map whose
  // key is url. If no such entry exists, throw a TypeError exception and abort
  // these steps.
  ModuleScript* ms = script->Loader()->GetFetchedModule(uri);
  if (!ms) {
    return HandleModuleNotFound(aCx, script, string);
  }

  if (ms->InstantiationFailed()) {
    JS::Rooted<JS::Value> exception(aCx, ms->Exception());
    JS_SetPendingException(aCx, exception);
    return false;
  }

  *vp = JS::ObjectValue(*ms->ModuleRecord());
  return true;
}

static nsresult
EnsureModuleResolveHook(JSContext* aCx)
{
  if (JS::GetModuleResolveHook(aCx)) {
    return NS_OK;
  }

  JS::Rooted<JSFunction*> func(aCx);
  func = JS_NewFunction(aCx, HostResolveImportedModule, 2, 0,
                        "HostResolveImportedModule");
  if (!func) {
    return NS_ERROR_FAILURE;
  }

  JS::SetModuleResolveHook(aCx, func);
  return NS_OK;
}

void
ScriptLoader::ProcessLoadedModuleTree(ModuleLoadRequest* aRequest)
{
  if (aRequest->IsTopLevel()) {
    MaybeMoveToLoadedList(aRequest);
    ProcessPendingRequests();
  }

  if (aRequest->mWasCompiledOMT) {
    mDocument->UnblockOnload(false);
  }
}

bool
ScriptLoader::InstantiateModuleTree(ModuleLoadRequest* aRequest)
{
  // Perform eager instantiation of the loaded module tree.

  MOZ_ASSERT(aRequest);

  ModuleScript* ms = aRequest->mModuleScript;
  MOZ_ASSERT(ms);
  if (!ms->ModuleRecord()) {
    return false;
  }

  AutoJSAPI jsapi;
  if (NS_WARN_IF(!jsapi.Init(ms->ModuleRecord()))) {
    return false;
  }

  nsresult rv = EnsureModuleResolveHook(jsapi.cx());
  NS_ENSURE_SUCCESS(rv, false);

  JS::Rooted<JSObject*> module(jsapi.cx(), ms->ModuleRecord());
  bool ok = NS_SUCCEEDED(nsJSUtils::ModuleDeclarationInstantiation(jsapi.cx(), module));

  JS::RootedValue exception(jsapi.cx());
  if (!ok) {
    MOZ_ASSERT(jsapi.HasException());
    if (!jsapi.StealException(&exception)) {
      return false;
    }
    MOZ_ASSERT(!exception.isUndefined());
  }

  // Mark this module and any uninstantiated dependencies found via depth-first
  // search as instantiated and record any error.

  mozilla::Vector<ModuleLoadRequest*, 1> requests;
  if (!requests.append(aRequest)) {
    return false;
  }

  while (!requests.empty()) {
    ModuleLoadRequest* request = requests.popCopy();
    ModuleScript* ms = request->mModuleScript;
    if (!ms->IsUninstantiated()) {
      continue;
    }

    ms->SetInstantiationResult(exception);

    for (auto import : request->mImports) {
      if (import->mModuleScript->IsUninstantiated() &&
          !requests.append(import))
      {
        return false;
      }
    }
  }

  return true;
}

/* static */ bool
ScriptLoader::IsBytecodeCacheEnabled()
{
  static bool sExposeTestInterfaceEnabled = false;
  static bool sExposeTestInterfacePrefCached = false;
  if (!sExposeTestInterfacePrefCached) {
    sExposeTestInterfacePrefCached = true;
    Preferences::AddBoolVarCache(&sExposeTestInterfaceEnabled,
                                 "dom.script_loader.bytecode_cache.enabled",
                                 false);
  }
  return sExposeTestInterfaceEnabled;
}

/* static */ bool
ScriptLoader::IsEagerBytecodeCache()
{
  // When testing, we want to force use of the bytecode cache.
  static bool sEagerBytecodeCache = false;
  static bool sForceBytecodeCachePrefCached = false;
  if (!sForceBytecodeCachePrefCached) {
    sForceBytecodeCachePrefCached = true;
    Preferences::AddBoolVarCache(&sEagerBytecodeCache,
                                 "dom.script_loader.bytecode_cache.eager",
                                 false);
  }
  return sEagerBytecodeCache;
}

nsresult
ScriptLoader::RestartLoad(ScriptLoadRequest* aRequest)
{
  MOZ_ASSERT(aRequest->IsBytecode());
  aRequest->mScriptBytecode.clearAndFree();
  TRACE_FOR_TEST(aRequest->mElement, "scriptloader_fallback");

  // Start a new channel from which we explicitly request to stream the source
  // instead of the bytecode.
  aRequest->mProgress = ScriptLoadRequest::Progress::Loading_Source;
  nsresult rv = StartLoad(aRequest);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Close the current channel and this ScriptLoadHandler as we created a new
  // one for the same request.
  return NS_BINDING_RETARGETED;
}

nsresult
ScriptLoader::StartLoad(ScriptLoadRequest* aRequest)
{
  MOZ_ASSERT(aRequest->IsLoading());
  NS_ENSURE_TRUE(mDocument, NS_ERROR_NULL_POINTER);
  aRequest->mDataType = ScriptLoadRequest::DataType::Unknown;

  // If this document is sandboxed without 'allow-scripts', abort.
  if (mDocument->HasScriptsBlockedBySandbox()) {
    return NS_OK;
  }

  if (aRequest->IsModuleRequest()) {
    // Check whether the module has been fetched or is currently being fetched,
    // and if so wait for it.
    ModuleLoadRequest* request = aRequest->AsModuleRequest();
    if (ModuleMapContainsModule(request)) {
      WaitForModuleFetch(request)
        ->Then(AbstractThread::MainThread(), __func__, request,
               &ModuleLoadRequest::ModuleLoaded,
               &ModuleLoadRequest::LoadFailed);
      return NS_OK;
    }

    // Otherwise put the URL in the module map and mark it as fetching.
    SetModuleFetchStarted(request);
  }

  nsContentPolicyType contentPolicyType = aRequest->IsPreload()
                                          ? nsIContentPolicy::TYPE_INTERNAL_SCRIPT_PRELOAD
                                          : nsIContentPolicy::TYPE_INTERNAL_SCRIPT;
  nsCOMPtr<nsINode> context;
  if (aRequest->mElement) {
    context = do_QueryInterface(aRequest->mElement);
  }
  else {
    context = mDocument;
  }

  nsCOMPtr<nsILoadGroup> loadGroup = mDocument->GetDocumentLoadGroup();
  nsCOMPtr<nsPIDOMWindowOuter> window = mDocument->MasterDocument()->GetWindow();
  NS_ENSURE_TRUE(window, NS_ERROR_NULL_POINTER);
  nsIDocShell* docshell = window->GetDocShell();
  nsCOMPtr<nsIInterfaceRequestor> prompter(do_QueryInterface(docshell));

  nsSecurityFlags securityFlags;
  if (aRequest->IsModuleRequest()) {
    // According to the spec, module scripts have different behaviour to classic
    // scripts and always use CORS.
    securityFlags = nsILoadInfo::SEC_REQUIRE_CORS_DATA_INHERITS;
    if (aRequest->mCORSMode == CORS_NONE) {
      securityFlags |= nsILoadInfo::SEC_COOKIES_OMIT;
    } else if (aRequest->mCORSMode == CORS_ANONYMOUS) {
      securityFlags |= nsILoadInfo::SEC_COOKIES_SAME_ORIGIN;
    } else {
      MOZ_ASSERT(aRequest->mCORSMode == CORS_USE_CREDENTIALS);
      securityFlags |= nsILoadInfo::SEC_COOKIES_INCLUDE;
    }
  } else {
    securityFlags = aRequest->mCORSMode == CORS_NONE
      ? nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL
      : nsILoadInfo::SEC_REQUIRE_CORS_DATA_INHERITS;
    if (aRequest->mCORSMode == CORS_ANONYMOUS) {
      securityFlags |= nsILoadInfo::SEC_COOKIES_SAME_ORIGIN;
    } else if (aRequest->mCORSMode == CORS_USE_CREDENTIALS) {
      securityFlags |= nsILoadInfo::SEC_COOKIES_INCLUDE;
    }
  }
  securityFlags |= nsILoadInfo::SEC_ALLOW_CHROME;

  nsCOMPtr<nsIChannel> channel;
  nsresult rv = NS_NewChannel(getter_AddRefs(channel),
                              aRequest->mURI,
                              context,
                              securityFlags,
                              contentPolicyType,
                              loadGroup,
                              prompter,
                              nsIRequest::LOAD_NORMAL |
                              nsIChannel::LOAD_CLASSIFY_URI);

  NS_ENSURE_SUCCESS(rv, rv);

  // To avoid decoding issues, the JSVersion is explicitly guarded here, and the
  // build-id is part of the kBytecodeMimeType constant.
  aRequest->mCacheInfo = nullptr;
  nsCOMPtr<nsICacheInfoChannel> cic(do_QueryInterface(channel));
  if (cic && IsBytecodeCacheEnabled() && aRequest->mJSVersion == JSVERSION_DEFAULT) {
    if (!aRequest->IsLoadingSource()) {
      // Inform the HTTP cache that we prefer to have information coming from the
      // bytecode cache instead of the sources, if such entry is already registered.
      LOG(("ScriptLoadRequest (%p): Maybe request bytecode", aRequest));
      cic->PreferAlternativeDataType(kBytecodeMimeType);
    } else {
      // If we are explicitly loading from the sources, such as after a
      // restarted request, we might still want to save the bytecode after.
      //
      // The following tell the cache to look for an alternative data type which
      // does not exist, such that we can later save the bytecode with a
      // different alternative data type.
      LOG(("ScriptLoadRequest (%p): Request saving bytecode later", aRequest));
      cic->PreferAlternativeDataType(kNullMimeType);
    }
  }

  nsIScriptElement* script = aRequest->mElement;
  nsCOMPtr<nsIClassOfService> cos(do_QueryInterface(channel));

  if (cos) {
    if (aRequest->mScriptFromHead &&
        !(script && (script->GetScriptAsync() || script->GetScriptDeferred()))) {
      // synchronous head scripts block loading of most other non js/css
      // content such as images
      cos->AddClassFlags(nsIClassOfService::Leader);
    } else if (!(script && script->GetScriptDeferred())) {
      // other scripts are neither blocked nor prioritized unless marked deferred
      cos->AddClassFlags(nsIClassOfService::Unblocked);
    }
  }

  nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(channel));
  if (httpChannel) {
    // HTTP content negotation has little value in this context.
    rv = httpChannel->SetRequestHeader(NS_LITERAL_CSTRING("Accept"),
                                       NS_LITERAL_CSTRING("*/*"),
                                       false);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
    rv = httpChannel->SetReferrerWithPolicy(mDocument->GetDocumentURI(),
                                            aRequest->mReferrerPolicy);
    MOZ_ASSERT(NS_SUCCEEDED(rv));

    nsCOMPtr<nsIHttpChannelInternal> internalChannel(do_QueryInterface(httpChannel));
    if (internalChannel) {
      rv = internalChannel->SetIntegrityMetadata(aRequest->mIntegrity.GetIntegrityString());
      MOZ_ASSERT(NS_SUCCEEDED(rv));
    }
  }

  mozilla::net::PredictorLearn(aRequest->mURI, mDocument->GetDocumentURI(),
                               nsINetworkPredictor::LEARN_LOAD_SUBRESOURCE,
                               mDocument->NodePrincipal()->OriginAttributesRef());

  // Set the initiator type
  nsCOMPtr<nsITimedChannel> timedChannel(do_QueryInterface(httpChannel));
  if (timedChannel) {
    timedChannel->SetInitiatorType(NS_LITERAL_STRING("script"));
  }

  nsAutoPtr<mozilla::dom::SRICheckDataVerifier> sriDataVerifier;
  if (!aRequest->mIntegrity.IsEmpty()) {
    nsAutoCString sourceUri;
    if (mDocument->GetDocumentURI()) {
      mDocument->GetDocumentURI()->GetAsciiSpec(sourceUri);
    }
    sriDataVerifier = new SRICheckDataVerifier(aRequest->mIntegrity, sourceUri,
                                               mReporter);
  }

  RefPtr<ScriptLoadHandler> handler =
      new ScriptLoadHandler(this, aRequest, sriDataVerifier.forget());

  nsCOMPtr<nsIIncrementalStreamLoader> loader;
  rv = NS_NewIncrementalStreamLoader(getter_AddRefs(loader), handler);
  NS_ENSURE_SUCCESS(rv, rv);

  return channel->AsyncOpen2(loader);
}

bool
ScriptLoader::PreloadURIComparator::Equals(const PreloadInfo& aPi,
                                           nsIURI* const& aURI) const
{
  bool same;
  return NS_SUCCEEDED(aPi.mRequest->mURI->Equals(aURI, &same)) &&
         same;
}

class ScriptRequestProcessor : public Runnable
{
private:
  RefPtr<ScriptLoader> mLoader;
  RefPtr<ScriptLoadRequest> mRequest;
public:
  ScriptRequestProcessor(ScriptLoader* aLoader,
                         ScriptLoadRequest* aRequest)
    : mLoader(aLoader)
    , mRequest(aRequest)
  {}
  NS_IMETHOD Run() override
  {
    return mLoader->ProcessRequest(mRequest);
  }
};

static inline bool
ParseTypeAttribute(const nsAString& aType, JSVersion* aVersion)
{
  MOZ_ASSERT(!aType.IsEmpty());
  MOZ_ASSERT(aVersion);
  MOZ_ASSERT(*aVersion == JSVERSION_DEFAULT);

  nsContentTypeParser parser(aType);

  nsAutoString mimeType;
  nsresult rv = parser.GetType(mimeType);
  NS_ENSURE_SUCCESS(rv, false);

  if (!nsContentUtils::IsJavascriptMIMEType(mimeType)) {
    return false;
  }

  // Get the version string, and ensure the language supports it.
  nsAutoString versionName;
  rv = parser.GetParameter("version", versionName);

  if (NS_SUCCEEDED(rv)) {
    *aVersion = nsContentUtils::ParseJavascriptVersion(versionName);
  } else if (rv != NS_ERROR_INVALID_ARG) {
    return false;
  }

  return true;
}

static bool
CSPAllowsInlineScript(nsIScriptElement* aElement, nsIDocument* aDocument)
{
  nsCOMPtr<nsIContentSecurityPolicy> csp;
  // Note: For imports NodePrincipal and the principal of the master are
  // the same.
  nsresult rv = aDocument->NodePrincipal()->GetCsp(getter_AddRefs(csp));
  NS_ENSURE_SUCCESS(rv, false);

  if (!csp) {
    // no CSP --> allow
    return true;
  }

  // query the nonce
  nsCOMPtr<nsIContent> scriptContent = do_QueryInterface(aElement);
  nsAutoString nonce;
  scriptContent->GetAttr(kNameSpaceID_None, nsGkAtoms::nonce, nonce);
  bool parserCreated = aElement->GetParserCreated() != mozilla::dom::NOT_FROM_PARSER;

  // query the scripttext
  nsAutoString scriptText;
  aElement->GetScriptText(scriptText);

  bool allowInlineScript = false;
  rv = csp->GetAllowsInline(nsIContentPolicy::TYPE_SCRIPT,
                            nonce, parserCreated, scriptText,
                            aElement->GetScriptLineNumber(),
                            &allowInlineScript);
  return allowInlineScript;
}

ScriptLoadRequest*
ScriptLoader::CreateLoadRequest(ScriptKind aKind,
                                nsIScriptElement* aElement,
                                uint32_t aVersion, CORSMode aCORSMode,
                                const SRIMetadata& aIntegrity)
{
  if (aKind == ScriptKind::Classic) {
    return new ScriptLoadRequest(aKind, aElement, aVersion, aCORSMode,
                                 aIntegrity);
  }

  MOZ_ASSERT(aKind == ScriptKind::Module);
  return new ModuleLoadRequest(aElement, aVersion, aCORSMode, aIntegrity, this);
}

bool
ScriptLoader::ProcessScriptElement(nsIScriptElement* aElement)
{
  // We need a document to evaluate scripts.
  NS_ENSURE_TRUE(mDocument, false);

  // Check to see if scripts has been turned off.
  if (!mEnabled || !mDocument->IsScriptEnabled()) {
    return false;
  }

  NS_ASSERTION(!aElement->IsMalformed(), "Executing malformed script");

  nsCOMPtr<nsIContent> scriptContent = do_QueryInterface(aElement);

  // Step 13. Check that the script is not an eventhandler
  if (IsScriptEventHandler(scriptContent)) {
    return false;
  }

  JSVersion version = JSVERSION_DEFAULT;

  // Check the type attribute to determine language and version.
  // If type exists, it trumps the deprecated 'language='
  nsAutoString type;
  bool hasType = aElement->GetScriptType(type);

  ScriptKind scriptKind = ScriptKind::Classic;
  if (!type.IsEmpty()) {
    if (ModuleScriptsEnabled() && type.LowerCaseEqualsASCII("module")) {
      scriptKind = ScriptKind::Module;
    } else {
      NS_ENSURE_TRUE(ParseTypeAttribute(type, &version), false);
    }
  } else if (!hasType) {
    // no 'type=' element
    // "language" is a deprecated attribute of HTML, so we check it only for
    // HTML script elements.
    if (scriptContent->IsHTMLElement()) {
      nsAutoString language;
      scriptContent->GetAttr(kNameSpaceID_None, nsGkAtoms::language, language);
      if (!language.IsEmpty()) {
        if (!nsContentUtils::IsJavaScriptLanguage(language)) {
          return false;
        }
      }
    }
  }

  // "In modern user agents that support module scripts, the script element with
  // the nomodule attribute will be ignored".
  // "The nomodule attribute must not be specified on module scripts (and will
  // be ignored if it is)."
  if (ModuleScriptsEnabled() &&
      scriptKind == ScriptKind::Classic &&
      scriptContent->IsHTMLElement() &&
      scriptContent->HasAttr(kNameSpaceID_None, nsGkAtoms::nomodule)) {
    return false;
  }

  // Step 15. and later in the HTML5 spec
  nsresult rv = NS_OK;
  RefPtr<ScriptLoadRequest> request;
  if (aElement->GetScriptExternal()) {
    // external script
    nsCOMPtr<nsIURI> scriptURI = aElement->GetScriptURI();
    if (!scriptURI) {
      // Asynchronously report the failure to create a URI object
      NS_DispatchToCurrentThread(
        NewRunnableMethod(aElement,
                          &nsIScriptElement::FireErrorEvent));
      return false;
    }

    // Double-check that the preload matches what we're asked to load now.
    mozilla::net::ReferrerPolicy ourRefPolicy = mDocument->GetReferrerPolicy();
    CORSMode ourCORSMode = aElement->GetCORSMode();
    nsTArray<PreloadInfo>::index_type i =
      mPreloads.IndexOf(scriptURI.get(), 0, PreloadURIComparator());
    if (i != nsTArray<PreloadInfo>::NoIndex) {
      // preloaded
      // note that a script-inserted script can steal a preload!
      request = mPreloads[i].mRequest;
      request->mElement = aElement;
      nsString preloadCharset(mPreloads[i].mCharset);
      mPreloads.RemoveElementAt(i);

      // Double-check that the charset the preload used is the same as
      // the charset we have now.
      nsAutoString elementCharset;
      aElement->GetScriptCharset(elementCharset);
      if (elementCharset.Equals(preloadCharset) &&
          ourCORSMode == request->mCORSMode &&
          ourRefPolicy == request->mReferrerPolicy &&
          scriptKind == request->mKind) {
        rv = CheckContentPolicy(mDocument, aElement, request->mURI, type, false);
        if (NS_FAILED(rv)) {
          // probably plans have changed; even though the preload was allowed seems
          // like the actual load is not; let's cancel the preload request.
          request->Cancel();
          return false;
        }
      } else {
        // Drop the preload
        request = nullptr;
      }
    }

    if (!request) {
      // no usable preload

      SRIMetadata sriMetadata;
      {
        nsAutoString integrity;
        scriptContent->GetAttr(kNameSpaceID_None, nsGkAtoms::integrity,
                               integrity);
        if (!integrity.IsEmpty()) {
          MOZ_LOG(SRILogHelper::GetSriLog(), mozilla::LogLevel::Debug,
                  ("ScriptLoader::ProcessScriptElement, integrity=%s",
                   NS_ConvertUTF16toUTF8(integrity).get()));
          nsAutoCString sourceUri;
          if (mDocument->GetDocumentURI()) {
            mDocument->GetDocumentURI()->GetAsciiSpec(sourceUri);
          }
          SRICheck::IntegrityMetadata(integrity, sourceUri, mReporter,
                                      &sriMetadata);
        }
      }

      request = CreateLoadRequest(scriptKind, aElement, version, ourCORSMode,
                                  sriMetadata);
      request->mURI = scriptURI;
      request->mIsInline = false;
      request->mReferrerPolicy = ourRefPolicy;
      // keep request->mScriptFromHead to false so we don't treat non preloaded
      // scripts as blockers for full page load. See bug 792438.

      rv = StartLoad(request);
      if (NS_FAILED(rv)) {
        const char* message = "ScriptSourceLoadFailed";

        if (rv == NS_ERROR_MALFORMED_URI) {
            message = "ScriptSourceMalformed";
        }
        else if (rv == NS_ERROR_DOM_BAD_URI) {
            message = "ScriptSourceNotAllowed";
        }

        NS_ConvertUTF8toUTF16 url(scriptURI->GetSpecOrDefault());
        const char16_t* params[] = { url.get() };

        nsContentUtils::ReportToConsole(nsIScriptError::warningFlag,
            NS_LITERAL_CSTRING("Script Loader"), mDocument,
            nsContentUtils::eDOM_PROPERTIES, message,
            params, ArrayLength(params), nullptr,
            EmptyString(), aElement->GetScriptLineNumber());

        // Asynchronously report the load failure
        NS_DispatchToCurrentThread(
          NewRunnableMethod(aElement,
                            &nsIScriptElement::FireErrorEvent));
        return false;
      }
    }

    // Should still be in loading stage of script.
    NS_ASSERTION(!request->InCompilingStage(),
                 "Request should not yet be in compiling stage.");

    request->mJSVersion = version;

    if (aElement->GetScriptAsync()) {
      request->mIsAsync = true;
      if (request->IsReadyToRun()) {
        mLoadedAsyncRequests.AppendElement(request);
        // The script is available already. Run it ASAP when the event
        // loop gets a chance to spin.

        // KVKV TODO: Instead of processing immediately, try off-thread-parsing
        // it and only schedule a pending ProcessRequest if that fails.
        ProcessPendingRequestsAsync();
      } else {
        mLoadingAsyncRequests.AppendElement(request);
      }
      return false;
    }
    if (!aElement->GetParserCreated()) {
      // Violate the HTML5 spec in order to make LABjs and the "order" plug-in
      // for RequireJS work with their Gecko-sniffed code path. See
      // http://lists.w3.org/Archives/Public/public-html/2010Oct/0088.html
      request->mIsNonAsyncScriptInserted = true;
      mNonAsyncExternalScriptInsertedRequests.AppendElement(request);
      if (request->IsReadyToRun()) {
        // The script is available already. Run it ASAP when the event
        // loop gets a chance to spin.
        ProcessPendingRequestsAsync();
      }
      return false;
    }
    // we now have a parser-inserted request that may or may not be still
    // loading
    if (aElement->GetScriptDeferred() || request->IsModuleRequest()) {
      // We don't want to run this yet.
      // If we come here, the script is a parser-created script and it has
      // the defer attribute but not the async attribute. Since a
      // a parser-inserted script is being run, we came here by the parser
      // running the script, which means the parser is still alive and the
      // parse is ongoing.
      NS_ASSERTION(mDocument->GetCurrentContentSink() ||
                   aElement->GetParserCreated() == FROM_PARSER_XSLT,
          "Non-XSLT Defer script on a document without an active parser; bug 592366.");
      AddDeferRequest(request);
      return false;
    }

    if (aElement->GetParserCreated() == FROM_PARSER_XSLT) {
      // Need to maintain order for XSLT-inserted scripts
      NS_ASSERTION(!mParserBlockingRequest,
          "Parser-blocking scripts and XSLT scripts in the same doc!");
      request->mIsXSLT = true;
      mXSLTRequests.AppendElement(request);
      if (request->IsReadyToRun()) {
        // The script is available already. Run it ASAP when the event
        // loop gets a chance to spin.
        ProcessPendingRequestsAsync();
      }
      return true;
    }

    if (request->IsReadyToRun() && ReadyToExecuteParserBlockingScripts()) {
      // The request has already been loaded and there are no pending style
      // sheets. If the script comes from the network stream, cheat for
      // performance reasons and avoid a trip through the event loop.
      if (aElement->GetParserCreated() == FROM_PARSER_NETWORK) {
        return ProcessRequest(request) == NS_ERROR_HTMLPARSER_BLOCK;
      }
      // Otherwise, we've got a document.written script, make a trip through
      // the event loop to hide the preload effects from the scripts on the
      // Web page.
      NS_ASSERTION(!mParserBlockingRequest,
          "There can be only one parser-blocking script at a time");
      NS_ASSERTION(mXSLTRequests.isEmpty(),
          "Parser-blocking scripts and XSLT scripts in the same doc!");
      mParserBlockingRequest = request;
      ProcessPendingRequestsAsync();
      return true;
    }

    // The script hasn't loaded yet or there's a style sheet blocking it.
    // The script will be run when it loads or the style sheet loads.
    NS_ASSERTION(!mParserBlockingRequest,
        "There can be only one parser-blocking script at a time");
    NS_ASSERTION(mXSLTRequests.isEmpty(),
        "Parser-blocking scripts and XSLT scripts in the same doc!");
    mParserBlockingRequest = request;
    return true;
  }

  // inline script
  // Is this document sandboxed without 'allow-scripts'?
  if (mDocument->HasScriptsBlockedBySandbox()) {
    return false;
  }

  // Does CSP allow this inline script to run?
  if (!CSPAllowsInlineScript(aElement, mDocument)) {
    return false;
  }

  // Inline scripts ignore ther CORS mode and are always CORS_NONE
  request = CreateLoadRequest(scriptKind, aElement, version, CORS_NONE,
                              SRIMetadata()); // SRI doesn't apply
  request->mJSVersion = version;
  request->mIsInline = true;
  request->mURI = mDocument->GetDocumentURI();
  request->mLineNo = aElement->GetScriptLineNumber();
  request->mProgress = ScriptLoadRequest::Progress::Loading_Source;
  request->mDataType = ScriptLoadRequest::DataType::Source;
  TRACE_FOR_TEST_BOOL(request->mElement, "scriptloader_load_source");

  if (request->IsModuleRequest()) {
    ModuleLoadRequest* modReq = request->AsModuleRequest();
    modReq->mBaseURL = mDocument->GetDocBaseURI();
    rv = CreateModuleScript(modReq);
    NS_ENSURE_SUCCESS(rv, false);
    StartFetchingModuleDependencies(modReq);
    if (aElement->GetScriptAsync()) {
      mLoadingAsyncRequests.AppendElement(request);
    } else {
      AddDeferRequest(request);
    }
    return false;
  }
  request->mProgress = ScriptLoadRequest::Progress::Ready;
  if (aElement->GetParserCreated() == FROM_PARSER_XSLT &&
      (!ReadyToExecuteParserBlockingScripts() || !mXSLTRequests.isEmpty())) {
    // Need to maintain order for XSLT-inserted scripts
    NS_ASSERTION(!mParserBlockingRequest,
        "Parser-blocking scripts and XSLT scripts in the same doc!");
    mXSLTRequests.AppendElement(request);
    return true;
  }
  if (aElement->GetParserCreated() == NOT_FROM_PARSER) {
    NS_ASSERTION(!nsContentUtils::IsSafeToRunScript(),
        "A script-inserted script is inserted without an update batch?");
    nsContentUtils::AddScriptRunner(new ScriptRequestProcessor(this,
                                                                 request));
    return false;
  }
  if (aElement->GetParserCreated() == FROM_PARSER_NETWORK &&
      !ReadyToExecuteParserBlockingScripts()) {
    NS_ASSERTION(!mParserBlockingRequest,
        "There can be only one parser-blocking script at a time");
    mParserBlockingRequest = request;
    NS_ASSERTION(mXSLTRequests.isEmpty(),
        "Parser-blocking scripts and XSLT scripts in the same doc!");
    return true;
  }
  // We now have a document.written inline script or we have an inline script
  // from the network but there is no style sheet that is blocking scripts.
  // Don't check for style sheets blocking scripts in the document.write
  // case to avoid style sheet network activity affecting when
  // document.write returns. It's not really necessary to do this if
  // there's no document.write currently on the call stack. However,
  // this way matches IE more closely than checking if document.write
  // is on the call stack.
  NS_ASSERTION(nsContentUtils::IsSafeToRunScript(),
      "Not safe to run a parser-inserted script?");
  return ProcessRequest(request) == NS_ERROR_HTMLPARSER_BLOCK;
}

namespace {

class NotifyOffThreadScriptLoadCompletedRunnable : public Runnable
{
  RefPtr<ScriptLoadRequest> mRequest;
  RefPtr<ScriptLoader> mLoader;
  RefPtr<DocGroup> mDocGroup;
  void* mToken;

public:
  NotifyOffThreadScriptLoadCompletedRunnable(ScriptLoadRequest* aRequest,
                                             ScriptLoader* aLoader)
    : mRequest(aRequest)
    , mLoader(aLoader)
    , mDocGroup(aLoader->GetDocGroup())
    , mToken(nullptr)
  {
    MOZ_ASSERT(NS_IsMainThread());
  }

  virtual ~NotifyOffThreadScriptLoadCompletedRunnable();

  void SetToken(void* aToken) {
    MOZ_ASSERT(aToken && !mToken);
    mToken = aToken;
  }

  static void Dispatch(already_AddRefed<NotifyOffThreadScriptLoadCompletedRunnable>&& aSelf) {
    RefPtr<NotifyOffThreadScriptLoadCompletedRunnable> self = aSelf;
    RefPtr<DocGroup> docGroup = self->mDocGroup;
    docGroup->Dispatch("NotifyOffThreadScriptLoadCompletedRunnable",
                       TaskCategory::Other, self.forget());
  }

  NS_DECL_NSIRUNNABLE
};

} /* anonymous namespace */

nsresult
ScriptLoader::ProcessOffThreadRequest(ScriptLoadRequest* aRequest)
{
  MOZ_ASSERT(aRequest->mProgress == ScriptLoadRequest::Progress::Compiling);
  MOZ_ASSERT(!aRequest->mWasCompiledOMT);

  aRequest->mWasCompiledOMT = true;

  if (aRequest->IsModuleRequest()) {
    MOZ_ASSERT(aRequest->mOffThreadToken);
    ModuleLoadRequest* request = aRequest->AsModuleRequest();
    nsresult rv = ProcessFetchedModuleSource(request);
    if (NS_FAILED(rv)) {
      request->LoadFailed();
    }
    return rv;
  }

  aRequest->SetReady();

  if (aRequest == mParserBlockingRequest) {
    if (!ReadyToExecuteParserBlockingScripts()) {
      // If not ready to execute scripts, schedule an async call to
      // ProcessPendingRequests to handle it.
      ProcessPendingRequestsAsync();
      return NS_OK;
    }

    // Same logic as in top of ProcessPendingRequests.
    mParserBlockingRequest = nullptr;
    UnblockParser(aRequest);
    ProcessRequest(aRequest);
    mDocument->UnblockOnload(false);
    ContinueParserAsync(aRequest);
    return NS_OK;
  }

  nsresult rv = ProcessRequest(aRequest);
  mDocument->UnblockOnload(false);
  return rv;
}

NotifyOffThreadScriptLoadCompletedRunnable::~NotifyOffThreadScriptLoadCompletedRunnable()
{
  if (MOZ_UNLIKELY(mRequest || mLoader) && !NS_IsMainThread()) {
    NS_ReleaseOnMainThread(mRequest.forget());
    NS_ReleaseOnMainThread(mLoader.forget());
  }
}

NS_IMETHODIMP
NotifyOffThreadScriptLoadCompletedRunnable::Run()
{
  MOZ_ASSERT(NS_IsMainThread());

  // We want these to be dropped on the main thread, once we return from this
  // function.
  RefPtr<ScriptLoadRequest> request = mRequest.forget();
  RefPtr<ScriptLoader> loader = mLoader.forget();

  request->mOffThreadToken = mToken;
  nsresult rv = loader->ProcessOffThreadRequest(request);

  return rv;
}

static void
OffThreadScriptLoaderCallback(void* aToken, void* aCallbackData)
{
  RefPtr<NotifyOffThreadScriptLoadCompletedRunnable> aRunnable =
    dont_AddRef(static_cast<NotifyOffThreadScriptLoadCompletedRunnable*>(aCallbackData));
  aRunnable->SetToken(aToken);
  NotifyOffThreadScriptLoadCompletedRunnable::Dispatch(aRunnable.forget());
}

nsresult
ScriptLoader::AttemptAsyncScriptCompile(ScriptLoadRequest* aRequest)
{
  MOZ_ASSERT_IF(!aRequest->IsModuleRequest(), aRequest->IsReadyToRun());
  MOZ_ASSERT(!aRequest->mWasCompiledOMT);

  // Don't off-thread compile inline scripts.
  if (aRequest->mIsInline) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIScriptGlobalObject> globalObject = GetScriptGlobalObject();
  if (!globalObject) {
    return NS_ERROR_FAILURE;
  }

  AutoJSAPI jsapi;
  if (!jsapi.Init(globalObject)) {
    return NS_ERROR_FAILURE;
  }

  JSContext* cx = jsapi.cx();
  JS::Rooted<JSObject*> global(cx, globalObject->GetGlobalJSObject());
  JS::CompileOptions options(cx);

  nsresult rv = FillCompileOptionsForRequest(jsapi, aRequest, global, &options);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  size_t len = aRequest->IsSource()
    ? aRequest->mScriptText.length()
    : aRequest->mScriptBytecode.length();
  if (!JS::CanCompileOffThread(cx, options, len)) {
    return NS_ERROR_FAILURE;
  }

  RefPtr<NotifyOffThreadScriptLoadCompletedRunnable> runnable =
    new NotifyOffThreadScriptLoadCompletedRunnable(aRequest, this);

  if (aRequest->IsModuleRequest()) {
    MOZ_ASSERT(aRequest->IsSource());
    if (!JS::CompileOffThreadModule(cx, options,
                                    aRequest->mScriptText.begin(),
                                    aRequest->mScriptText.length(),
                                    OffThreadScriptLoaderCallback,
                                    static_cast<void*>(runnable))) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  } else if (aRequest->IsSource()) {
    if (!JS::CompileOffThread(cx, options,
                              aRequest->mScriptText.begin(),
                              aRequest->mScriptText.length(),
                              OffThreadScriptLoaderCallback,
                              static_cast<void*>(runnable))) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  } else {
    MOZ_ASSERT(aRequest->IsBytecode());
    if (!JS::DecodeOffThreadScript(cx, options,
                                   aRequest->mScriptBytecode,
                                   aRequest->mBytecodeOffset,
                                   OffThreadScriptLoaderCallback,
                                   static_cast<void*>(runnable))) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  mDocument->BlockOnload();

  // Once the compilation is finished, an event would be added to the event loop
  // to call ScriptLoader::ProcessOffThreadRequest with the same request.
  aRequest->mProgress = ScriptLoadRequest::Progress::Compiling;

  Unused << runnable.forget();
  return NS_OK;
}

nsresult
ScriptLoader::CompileOffThreadOrProcessRequest(ScriptLoadRequest* aRequest)
{
  NS_ASSERTION(nsContentUtils::IsSafeToRunScript(),
               "Processing requests when running scripts is unsafe.");
  NS_ASSERTION(!aRequest->mOffThreadToken,
               "Candidate for off-thread compile is already parsed off-thread");
  NS_ASSERTION(!aRequest->InCompilingStage(),
               "Candidate for off-thread compile is already in compiling stage.");

  nsresult rv = AttemptAsyncScriptCompile(aRequest);
  if (NS_SUCCEEDED(rv)) {
    return rv;
  }

  return ProcessRequest(aRequest);
}

SourceBufferHolder
ScriptLoader::GetScriptSource(ScriptLoadRequest* aRequest, nsAutoString& inlineData)
{
  // Return a SourceBufferHolder object holding the script's source text.
  // |inlineData| is used to hold the text for inline objects.

  // If there's no script text, we try to get it from the element
  if (aRequest->mIsInline) {
    // XXX This is inefficient - GetText makes multiple
    // copies.
    aRequest->mElement->GetScriptText(inlineData);
    return SourceBufferHolder(inlineData.get(),
                              inlineData.Length(),
                              SourceBufferHolder::NoOwnership);
  }

  return SourceBufferHolder(aRequest->mScriptText.begin(),
                            aRequest->mScriptText.length(),
                            SourceBufferHolder::NoOwnership);
}

nsresult
ScriptLoader::ProcessRequest(ScriptLoadRequest* aRequest)
{
  NS_ASSERTION(nsContentUtils::IsSafeToRunScript(),
               "Processing requests when running scripts is unsafe.");
  NS_ASSERTION(aRequest->IsReadyToRun(),
               "Processing a request that is not ready to run.");

  NS_ENSURE_ARG(aRequest);

  if (aRequest->IsModuleRequest() &&
      !aRequest->AsModuleRequest()->mModuleScript)
  {
    // There was an error parsing a module script.  Nothing to do here.
    FireScriptAvailable(NS_ERROR_FAILURE, aRequest);
    return NS_OK;
  }

  nsCOMPtr<nsINode> scriptElem = do_QueryInterface(aRequest->mElement);

  nsCOMPtr<nsIDocument> doc;
  if (!aRequest->mIsInline) {
    doc = scriptElem->OwnerDoc();
  }

  nsCOMPtr<nsIScriptElement> oldParserInsertedScript;
  uint32_t parserCreated = aRequest->mElement->GetParserCreated();
  if (parserCreated) {
    oldParserInsertedScript = mCurrentParserInsertedScript;
    mCurrentParserInsertedScript = aRequest->mElement;
  }

  aRequest->mElement->BeginEvaluating();

  FireScriptAvailable(NS_OK, aRequest);

  // The window may have gone away by this point, in which case there's no point
  // in trying to run the script.
  nsCOMPtr<nsIDocument> master = mDocument->MasterDocument();
  {
    // Try to perform a microtask checkpoint
    nsAutoMicroTask mt;
  }

  nsPIDOMWindowInner* pwin = master->GetInnerWindow();
  bool runScript = !!pwin;
  if (runScript) {
    nsContentUtils::DispatchTrustedEvent(scriptElem->OwnerDoc(),
                                         scriptElem,
                                         NS_LITERAL_STRING("beforescriptexecute"),
                                         true, true, &runScript);
  }

  // Inner window could have gone away after firing beforescriptexecute
  pwin = master->GetInnerWindow();
  if (!pwin) {
    runScript = false;
  }

  nsresult rv = NS_OK;
  if (runScript) {
    if (doc) {
      doc->BeginEvaluatingExternalScript();
    }
    rv = EvaluateScript(aRequest);
    if (doc) {
      doc->EndEvaluatingExternalScript();
    }

    nsContentUtils::DispatchTrustedEvent(scriptElem->OwnerDoc(),
                                         scriptElem,
                                         NS_LITERAL_STRING("afterscriptexecute"),
                                         true, false);
  }

  FireScriptEvaluated(rv, aRequest);

  aRequest->mElement->EndEvaluating();

  if (parserCreated) {
    mCurrentParserInsertedScript = oldParserInsertedScript;
  }

  if (aRequest->mOffThreadToken) {
    // The request was parsed off-main-thread, but the result of the off
    // thread parse was not actually needed to process the request
    // (disappearing window, some other error, ...). Finish the
    // request to avoid leaks in the JS engine.
    MOZ_ASSERT(!aRequest->IsModuleRequest());
    aRequest->MaybeCancelOffThreadScript();
  }

  // Free any source data, but keep the bytecode content as we might have to
  // save it later.
  aRequest->mScriptText.clearAndFree();

  return rv;
}

void
ScriptLoader::FireScriptAvailable(nsresult aResult,
                                  ScriptLoadRequest* aRequest)
{
  for (int32_t i = 0; i < mObservers.Count(); i++) {
    nsCOMPtr<nsIScriptLoaderObserver> obs = mObservers[i];
    obs->ScriptAvailable(aResult, aRequest->mElement,
                         aRequest->mIsInline, aRequest->mURI,
                         aRequest->mLineNo);
  }

  aRequest->FireScriptAvailable(aResult);
}

void
ScriptLoader::FireScriptEvaluated(nsresult aResult,
                                  ScriptLoadRequest* aRequest)
{
  for (int32_t i = 0; i < mObservers.Count(); i++) {
    nsCOMPtr<nsIScriptLoaderObserver> obs = mObservers[i];
    obs->ScriptEvaluated(aResult, aRequest->mElement,
                         aRequest->mIsInline);
  }

  aRequest->FireScriptEvaluated(aResult);
}

already_AddRefed<nsIScriptGlobalObject>
ScriptLoader::GetScriptGlobalObject()
{
  nsCOMPtr<nsIDocument> master = mDocument->MasterDocument();
  nsPIDOMWindowInner* pwin = master->GetInnerWindow();
  if (!pwin) {
    return nullptr;
  }

  nsCOMPtr<nsIScriptGlobalObject> globalObject = do_QueryInterface(pwin);
  NS_ASSERTION(globalObject, "windows must be global objects");

  // and make sure we are setup for this type of script.
  nsresult rv = globalObject->EnsureScriptEnvironment();
  if (NS_FAILED(rv)) {
    return nullptr;
  }

  return globalObject.forget();
}

nsresult
ScriptLoader::FillCompileOptionsForRequest(const AutoJSAPI&jsapi,
                                           ScriptLoadRequest* aRequest,
                                           JS::Handle<JSObject*> aScopeChain,
                                           JS::CompileOptions* aOptions)
{
  // It's very important to use aRequest->mURI, not the final URI of the channel
  // aRequest ended up getting script data from, as the script filename.
  nsresult rv;
  nsContentUtils::GetWrapperSafeScriptFilename(mDocument, aRequest->mURI,
                                               aRequest->mURL, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (mDocument) {
    mDocument->NoteScriptTrackingStatus(aRequest->mURL, aRequest->IsTracking());
  }

  bool isScriptElement = !aRequest->IsModuleRequest() ||
                         aRequest->AsModuleRequest()->IsTopLevel();
  aOptions->setIntroductionType(isScriptElement ? "scriptElement"
                                                : "importedModule");
  aOptions->setFileAndLine(aRequest->mURL.get(), aRequest->mLineNo);
  aOptions->setVersion(JSVersion(aRequest->mJSVersion));
  aOptions->setIsRunOnce(true);
  aOptions->setNoScriptRval(true);
  if (aRequest->mHasSourceMapURL) {
    aOptions->setSourceMapURL(aRequest->mSourceMapURL.get());
  }
  if (aRequest->mOriginPrincipal) {
    nsIPrincipal* scriptPrin = nsContentUtils::ObjectPrincipal(aScopeChain);
    bool subsumes = scriptPrin->Subsumes(aRequest->mOriginPrincipal);
    aOptions->setMutedErrors(!subsumes);
  }

  JSContext* cx = jsapi.cx();
  JS::Rooted<JS::Value> elementVal(cx);
  MOZ_ASSERT(aRequest->mElement);
  if (NS_SUCCEEDED(nsContentUtils::WrapNative(cx, aRequest->mElement,
                                              &elementVal,
                                              /* aAllowWrapping = */ true))) {
    MOZ_ASSERT(elementVal.isObject());
    aOptions->setElement(&elementVal.toObject());
  }

  return NS_OK;
}

nsresult
ScriptLoader::EvaluateScript(ScriptLoadRequest* aRequest)
{
  MOZ_ASSERT(aRequest->IsReadyToRun());

  // We need a document to evaluate scripts.
  if (!mDocument) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIContent> scriptContent(do_QueryInterface(aRequest->mElement));
  nsIDocument* ownerDoc = scriptContent->OwnerDoc();
  if (ownerDoc != mDocument) {
    // Willful violation of HTML5 as of 2010-12-01
    return NS_ERROR_FAILURE;
  }

  // Get the script-type to be used by this element.
  NS_ASSERTION(scriptContent, "no content - what is default script-type?");

  nsCOMPtr<nsIScriptGlobalObject> globalObject = GetScriptGlobalObject();
  if (!globalObject) {
    return NS_ERROR_FAILURE;
  }

  // Make sure context is a strong reference since we access it after
  // we've executed a script, which may cause all other references to
  // the context to go away.
  nsCOMPtr<nsIScriptContext> context = globalObject->GetScriptContext();
  if (!context) {
    return NS_ERROR_FAILURE;
  }

  JSVersion version = JSVersion(aRequest->mJSVersion);
  if (version == JSVERSION_UNKNOWN) {
    return NS_OK;
  }

  // New script entry point required, due to the "Create a script" sub-step of
  // http://www.whatwg.org/specs/web-apps/current-work/#execute-the-script-block
  nsAutoMicroTask mt;
  AutoEntryScript aes(globalObject, "<script> element", true);
  JS::Rooted<JSObject*> global(aes.cx(),
                               globalObject->GetGlobalJSObject());

  bool oldProcessingScriptTag = context->GetProcessingScriptTag();
  context->SetProcessingScriptTag(true);
  nsresult rv;
  {
    // Update our current script.
    AutoCurrentScriptUpdater scriptUpdater(this, aRequest->mElement);
    Maybe<AutoCurrentScriptUpdater> masterScriptUpdater;
    nsCOMPtr<nsIDocument> master = mDocument->MasterDocument();
    if (master != mDocument) {
      // If this script belongs to an import document, it will be
      // executed in the context of the master document. During the
      // execution currentScript of the master should refer to this
      // script. So let's update the mCurrentScript of the ScriptLoader
      // of the master document too.
      masterScriptUpdater.emplace(master->ScriptLoader(),
                                  aRequest->mElement);
    }

    if (aRequest->IsModuleRequest()) {
      // When a module is already loaded, it is not feched a second time and the
      // mDataType of the request might remain set to DataType::Unknown.
      MOZ_ASSERT(!aRequest->IsBytecode());
      LOG(("ScriptLoadRequest (%p): Evaluate Module", aRequest));
      ModuleLoadRequest* request = aRequest->AsModuleRequest();
      MOZ_ASSERT(request->mModuleScript);
      MOZ_ASSERT(!request->mOffThreadToken);
      ModuleScript* ms = request->mModuleScript;
      MOZ_ASSERT(!ms->IsUninstantiated());
      if (ms->InstantiationFailed()) {
        JS::Rooted<JS::Value> exception(aes.cx(), ms->Exception());
        JS_SetPendingException(aes.cx(), exception);
        rv = NS_ERROR_FAILURE;
      } else {
        JS::Rooted<JSObject*> module(aes.cx(), ms->ModuleRecord());
        MOZ_ASSERT(module);
        rv = nsJSUtils::ModuleEvaluation(aes.cx(), module);
      }
      aRequest->mCacheInfo = nullptr;
    } else {
      JS::CompileOptions options(aes.cx());
      rv = FillCompileOptionsForRequest(aes, aRequest, global, &options);

      if (NS_SUCCEEDED(rv)) {
        if (aRequest->IsBytecode()) {
          TRACE_FOR_TEST(aRequest->mElement, "scriptloader_execute");
          nsJSUtils::ExecutionContext exec(aes.cx(), global);
          if (aRequest->mOffThreadToken) {
            LOG(("ScriptLoadRequest (%p): Decode Bytecode & Join and Execute", aRequest));
            rv = exec.DecodeJoinAndExec(&aRequest->mOffThreadToken);
          } else {
            LOG(("ScriptLoadRequest (%p): Decode Bytecode and Execute", aRequest));
            rv = exec.DecodeAndExec(options, aRequest->mScriptBytecode,
                                    aRequest->mBytecodeOffset);
          }
          // We do not expect to be saving anything when we already have some
          // bytecode.
          MOZ_ASSERT(!aRequest->mCacheInfo);
        } else {
          MOZ_ASSERT(aRequest->IsSource());
          JS::Rooted<JSScript*> script(aes.cx());

          bool encodeBytecode = false;
          if (aRequest->mCacheInfo) {
            MOZ_ASSERT(aRequest->mBytecodeOffset ==
                       aRequest->mScriptBytecode.length());
            encodeBytecode = IsEagerBytecodeCache(); // Heuristic!
          }

          {
            nsJSUtils::ExecutionContext exec(aes.cx(), global);
            exec.SetEncodeBytecode(encodeBytecode);
            TRACE_FOR_TEST(aRequest->mElement, "scriptloader_execute");
            if (aRequest->mOffThreadToken) {
              // Off-main-thread parsing.
              LOG(("ScriptLoadRequest (%p): Join (off-thread parsing) and Execute",
                   aRequest));
              rv = exec.JoinAndExec(&aRequest->mOffThreadToken, &script);
            } else {
              // Main thread parsing (inline and small scripts)
              LOG(("ScriptLoadRequest (%p): Compile And Exec", aRequest));
              nsAutoString inlineData;
              SourceBufferHolder srcBuf = GetScriptSource(aRequest, inlineData);
              rv = exec.CompileAndExec(options, srcBuf, &script);
            }
          }

          // Queue the current script load request to later save the bytecode.
          if (NS_SUCCEEDED(rv) && encodeBytecode) {
            aRequest->mScript = script;
            HoldJSObjects(aRequest);
            TRACE_FOR_TEST(aRequest->mElement, "scriptloader_encode");
            RegisterForBytecodeEncoding(aRequest);
          } else {
            LOG(("ScriptLoadRequest (%p): Cannot cache anything (rv = %X, script = %p, cacheInfo = %p)",
                 aRequest, unsigned(rv), script.get(), aRequest->mCacheInfo.get()));
            TRACE_FOR_TEST_NONE(aRequest->mElement, "scriptloader_no_encode");
            aRequest->mCacheInfo = nullptr;
          }
        }
      }
    }

    // Even if we are not saving the bytecode of the current script, we have
    // to trigger the encoding of the bytecode, as the current script can
    // call functions of a script for which we are recording the bytecode.
    MaybeTriggerBytecodeEncoding();
  }

  context->SetProcessingScriptTag(oldProcessingScriptTag);
  return rv;
}

void
ScriptLoader::RegisterForBytecodeEncoding(ScriptLoadRequest* aRequest)
{
  MOZ_ASSERT(aRequest->mCacheInfo);
  MOZ_ASSERT(aRequest->mScript);
  mBytecodeEncodingQueue.AppendElement(aRequest);
}

void
ScriptLoader::LoadEventFired()
{
  mLoadEventFired = true;
  MaybeTriggerBytecodeEncoding();
}

void
ScriptLoader::MaybeTriggerBytecodeEncoding()
{
  // We wait for the load event to be fired before saving the bytecode of
  // any script to the cache. It is quite common to have load event
  // listeners trigger more JavaScript execution, that we want to save as
  // part of this start-up bytecode cache.
  if (!mLoadEventFired) {
    return;
  }

  // No need to fire any event if there is no bytecode to be saved.
  if (mBytecodeEncodingQueue.isEmpty()) {
    return;
  }

  // Wait until all scripts are loaded before saving the bytecode, such that
  // we capture most of the intialization of the page.
  if (HasPendingRequests()) {
    return;
  }

  // Create a new runnable dedicated to encoding the content of the bytecode
  // of all enqueued scripts. In case of failure, we give-up on encoding the
  // bytecode.
  nsCOMPtr<nsIRunnable> encoder =
    NewRunnableMethod("ScriptLoader::EncodeBytecode",
                      this, &ScriptLoader::EncodeBytecode);
  if (NS_FAILED(NS_DispatchToCurrentThread(encoder))) {
    GiveUpBytecodeEncoding();
  }
}

void
ScriptLoader::EncodeBytecode()
{
  // If any script got added in the previous loop cycle, wait until all
  // remaining script executions are completed, such that we capture most of
  // the initialization.
  if (HasPendingRequests()) {
    return;
  }

  nsCOMPtr<nsIScriptGlobalObject> globalObject = GetScriptGlobalObject();
  if (!globalObject) {
    GiveUpBytecodeEncoding();
    return;
  }

  nsCOMPtr<nsIScriptContext> context = globalObject->GetScriptContext();
  if (!context) {
    GiveUpBytecodeEncoding();
    return;
  }

  AutoEntryScript aes(globalObject, "encode bytecode", true);
  RefPtr<ScriptLoadRequest> request;
  while (!mBytecodeEncodingQueue.isEmpty()) {
    request = mBytecodeEncodingQueue.StealFirst();
    EncodeRequestBytecode(aes.cx(), request);
    request->mScriptBytecode.clearAndFree();
    request->DropBytecodeCacheReferences();
  }
}

void
ScriptLoader::EncodeRequestBytecode(JSContext* aCx, ScriptLoadRequest* aRequest)
{
  nsresult rv = NS_OK;
  MOZ_ASSERT(aRequest->mCacheInfo);
  auto bytecodeFailed = mozilla::MakeScopeExit([&]() {
    TRACE_FOR_TEST_NONE(aRequest->mElement, "scriptloader_bytecode_failed");
  });

  JS::RootedScript script(aCx, aRequest->mScript);
  if (!JS::FinishIncrementalEncoding(aCx, script, aRequest->mScriptBytecode)) {
    LOG(("ScriptLoadRequest (%p): Cannot serialize bytecode",
         aRequest));
    return;
  }

  if (aRequest->mScriptBytecode.length() >= UINT32_MAX) {
    LOG(("ScriptLoadRequest (%p): Bytecode cache is too large to be decoded correctly.",
         aRequest));
    return;
  }

  // Open the output stream to the cache entry alternate data storage. This
  // might fail if the stream is already open by another request, in which
  // case, we just ignore the current one.
  nsCOMPtr<nsIOutputStream> output;
  rv = aRequest->mCacheInfo->OpenAlternativeOutputStream(kBytecodeMimeType,
                                                         getter_AddRefs(output));
  if (NS_FAILED(rv)) {
    LOG(("ScriptLoadRequest (%p): Cannot open bytecode cache (rv = %X, output = %p)",
         aRequest, unsigned(rv), output.get()));
    return;
  }
  MOZ_ASSERT(output);
  auto closeOutStream = mozilla::MakeScopeExit([&]() {
    nsresult rv = output->Close();
    LOG(("ScriptLoadRequest (%p): Closing (rv = %X)",
         aRequest, unsigned(rv)));
  });

  uint32_t n;
  rv = output->Write(reinterpret_cast<char*>(aRequest->mScriptBytecode.begin()),
                     aRequest->mScriptBytecode.length(), &n);
  LOG(("ScriptLoadRequest (%p): Write bytecode cache (rv = %X, length = %u, written = %u)",
       aRequest, unsigned(rv), unsigned(aRequest->mScriptBytecode.length()), n));
  if (NS_FAILED(rv)) {
    return;
  }

  bytecodeFailed.release();
  TRACE_FOR_TEST_NONE(aRequest->mElement, "scriptloader_bytecode_saved");
}

void
ScriptLoader::GiveUpBytecodeEncoding()
{
  // Ideally we prefer to properly end the incremental encoder, such that we
  // would not keep a large buffer around.  If we cannot, we fallback on the
  // removal of all request from the current list and these large buffers would
  // be removed at the same time as the source object.
  nsCOMPtr<nsIScriptGlobalObject> globalObject = GetScriptGlobalObject();
  if (globalObject) {
    nsCOMPtr<nsIScriptContext> context = globalObject->GetScriptContext();
    if (context) {
      AutoEntryScript aes(globalObject, "give-up bytecode encoding", true);
      JS::RootedScript script(aes.cx());
      while (!mBytecodeEncodingQueue.isEmpty()) {
        RefPtr<ScriptLoadRequest> request = mBytecodeEncodingQueue.StealFirst();
        LOG(("ScriptLoadRequest (%p): Cannot serialize bytecode", request.get()));
        TRACE_FOR_TEST_NONE(request->mElement, "scriptloader_bytecode_failed");
        script.set(request->mScript);
        Unused << JS::FinishIncrementalEncoding(aes.cx(), script,
                                                request->mScriptBytecode);
        request->mScriptBytecode.clearAndFree();
        request->DropBytecodeCacheReferences();
      }
      return;
    }
  }

  while (!mBytecodeEncodingQueue.isEmpty()) {
    RefPtr<ScriptLoadRequest> request = mBytecodeEncodingQueue.StealFirst();
    LOG(("ScriptLoadRequest (%p): Cannot serialize bytecode", request.get()));
    TRACE_FOR_TEST_NONE(request->mElement, "scriptloader_bytecode_failed");
    request->mScriptBytecode.clearAndFree();
    request->DropBytecodeCacheReferences();
  }
}

bool
ScriptLoader::HasPendingRequests()
{
  return mParserBlockingRequest ||
         !mXSLTRequests.isEmpty() ||
         !mLoadedAsyncRequests.isEmpty() ||
         !mNonAsyncExternalScriptInsertedRequests.isEmpty() ||
         !mDeferRequests.isEmpty() ||
         !mPendingChildLoaders.IsEmpty();
}

void
ScriptLoader::ProcessPendingRequestsAsync()
{
  if (HasPendingRequests()) {
    nsCOMPtr<nsIRunnable> task = NewRunnableMethod(this,
                                                   &ScriptLoader::ProcessPendingRequests);
    if (mDocument) {
      mDocument->Dispatch("ScriptLoader", TaskCategory::Other, task.forget());
    } else {
      NS_DispatchToCurrentThread(task.forget());
    }
  }
}

void
ScriptLoader::ProcessPendingRequests()
{
  RefPtr<ScriptLoadRequest> request;

  if (mParserBlockingRequest &&
      mParserBlockingRequest->IsReadyToRun() &&
      ReadyToExecuteParserBlockingScripts()) {
    request.swap(mParserBlockingRequest);
    UnblockParser(request);
    ProcessRequest(request);
    if (request->mWasCompiledOMT) {
      mDocument->UnblockOnload(false);
    }
    ContinueParserAsync(request);
  }

  while (ReadyToExecuteParserBlockingScripts() &&
         !mXSLTRequests.isEmpty() &&
         mXSLTRequests.getFirst()->IsReadyToRun()) {
    request = mXSLTRequests.StealFirst();
    ProcessRequest(request);
  }

  while (ReadyToExecuteScripts() && !mLoadedAsyncRequests.isEmpty()) {
    request = mLoadedAsyncRequests.StealFirst();
    if (request->IsModuleRequest()) {
      ProcessRequest(request);
    } else {
      CompileOffThreadOrProcessRequest(request);
    }
  }

  while (ReadyToExecuteScripts() &&
         !mNonAsyncExternalScriptInsertedRequests.isEmpty() &&
         mNonAsyncExternalScriptInsertedRequests.getFirst()->IsReadyToRun()) {
    // Violate the HTML5 spec and execute these in the insertion order in
    // order to make LABjs and the "order" plug-in for RequireJS work with
    // their Gecko-sniffed code path. See
    // http://lists.w3.org/Archives/Public/public-html/2010Oct/0088.html
    request = mNonAsyncExternalScriptInsertedRequests.StealFirst();
    ProcessRequest(request);
  }

  if (mDocumentParsingDone && mXSLTRequests.isEmpty()) {
    while (ReadyToExecuteScripts() &&
           !mDeferRequests.isEmpty() &&
           mDeferRequests.getFirst()->IsReadyToRun()) {
      request = mDeferRequests.StealFirst();
      ProcessRequest(request);
    }
  }

  while (!mPendingChildLoaders.IsEmpty() &&
         ReadyToExecuteParserBlockingScripts()) {
    RefPtr<ScriptLoader> child = mPendingChildLoaders[0];
    mPendingChildLoaders.RemoveElementAt(0);
    child->RemoveParserBlockingScriptExecutionBlocker();
  }

  if (mDocumentParsingDone && mDocument && !mParserBlockingRequest &&
      mNonAsyncExternalScriptInsertedRequests.isEmpty() &&
      mXSLTRequests.isEmpty() && mDeferRequests.isEmpty() &&
      MaybeRemovedDeferRequests()) {
    return ProcessPendingRequests();
  }

  if (mDocumentParsingDone && mDocument &&
      !mParserBlockingRequest && mLoadingAsyncRequests.isEmpty() &&
      mLoadedAsyncRequests.isEmpty() &&
      mNonAsyncExternalScriptInsertedRequests.isEmpty() &&
      mXSLTRequests.isEmpty() && mDeferRequests.isEmpty()) {
    // No more pending scripts; time to unblock onload.
    // OK to unblock onload synchronously here, since callers must be
    // prepared for the world changing anyway.
    mDocumentParsingDone = false;
    mDocument->UnblockOnload(true);
  }
}

bool
ScriptLoader::ReadyToExecuteParserBlockingScripts()
{
  // Make sure the SelfReadyToExecuteParserBlockingScripts check is first, so
  // that we don't block twice on an ancestor.
  if (!SelfReadyToExecuteParserBlockingScripts()) {
    return false;
  }

  for (nsIDocument* doc = mDocument; doc; doc = doc->GetParentDocument()) {
    ScriptLoader* ancestor = doc->ScriptLoader();
    if (!ancestor->SelfReadyToExecuteParserBlockingScripts() &&
        ancestor->AddPendingChildLoader(this)) {
      AddParserBlockingScriptExecutionBlocker();
      return false;
    }
  }

  if (mDocument && !mDocument->IsMasterDocument()) {
    RefPtr<ImportManager> im = mDocument->ImportManager();
    RefPtr<ImportLoader> loader = im->Find(mDocument);
    MOZ_ASSERT(loader, "How can we have an import document without a loader?");

    // The referring link that counts in the execution order calculation
    // (in spec: flagged as branch)
    nsCOMPtr<nsINode> referrer = loader->GetMainReferrer();
    MOZ_ASSERT(referrer, "There has to be a main referring link for each imports");

    // Import documents are blocked by their import predecessors. We need to
    // wait with script execution until all the predecessors are done.
    // Technically it means we have to wait for the last one to finish,
    // which is the neares one to us in the order.
    RefPtr<ImportLoader> lastPred = im->GetNearestPredecessor(referrer);
    if (!lastPred) {
      // If there is no predecessor we can run.
      return true;
    }

    nsCOMPtr<nsIDocument> doc = lastPred->GetDocument();
    if (lastPred->IsBlocking() || !doc ||
        !doc->ScriptLoader()->SelfReadyToExecuteParserBlockingScripts()) {
      // Document has not been created yet or it was created but not ready.
      // Either case we are blocked by it. The ImportLoader will take care
      // of blocking us, and adding the pending child loader to the blocking
      // ScriptLoader when it's possible (at this point the blocking loader
      // might not have created the document/ScriptLoader)
      lastPred->AddBlockedScriptLoader(this);
      // As more imports are parsed, this can change, let's cache what we
      // blocked, so it can be later updated if needed (see: ImportLoader::Updater).
      loader->SetBlockingPredecessor(lastPred);
      return false;
    }
  }

  return true;
}

/* static */ nsresult
ScriptLoader::ConvertToUTF16(nsIChannel* aChannel, const uint8_t* aData,
                             uint32_t aLength, const nsAString& aHintCharset,
                             nsIDocument* aDocument,
                             char16_t*& aBufOut, size_t& aLengthOut)
{
  if (!aLength) {
    aBufOut = nullptr;
    aLengthOut = 0;
    return NS_OK;
  }

  // The encoding info precedence is as follows from high to low:
  // The BOM
  // HTTP Content-Type (if name recognized)
  // charset attribute (if name recognized)
  // The encoding of the document

  nsAutoCString charset;

  nsCOMPtr<nsIUnicodeDecoder> unicodeDecoder;

  if (nsContentUtils::CheckForBOM(aData, aLength, charset)) {
    // charset is now one of "UTF-16BE", "UTF-16BE" or "UTF-8".  Those decoder
    // will take care of swallowing the BOM.
    unicodeDecoder = EncodingUtils::DecoderForEncoding(charset);
  }

  if (!unicodeDecoder &&
      aChannel &&
      NS_SUCCEEDED(aChannel->GetContentCharset(charset)) &&
      EncodingUtils::FindEncodingForLabel(charset, charset)) {
    unicodeDecoder = EncodingUtils::DecoderForEncoding(charset);
  }

  if (!unicodeDecoder &&
      EncodingUtils::FindEncodingForLabel(aHintCharset, charset)) {
    unicodeDecoder = EncodingUtils::DecoderForEncoding(charset);
  }

  if (!unicodeDecoder && aDocument) {
    charset = aDocument->GetDocumentCharacterSet();
    unicodeDecoder = EncodingUtils::DecoderForEncoding(charset);
  }

  if (!unicodeDecoder) {
    // Curiously, there are various callers that don't pass aDocument. The
    // fallback in the old code was ISO-8859-1, which behaved like
    // windows-1252. Saying windows-1252 for clarity and for compliance
    // with the Encoding Standard.
    charset = "windows-1252";
    unicodeDecoder = EncodingUtils::DecoderForEncoding(charset);
  }

  int32_t unicodeLength = 0;

  nsresult rv =
    unicodeDecoder->GetMaxLength(reinterpret_cast<const char*>(aData),
                                 aLength, &unicodeLength);
  NS_ENSURE_SUCCESS(rv, rv);

  aBufOut = static_cast<char16_t*>(js_malloc(unicodeLength * sizeof(char16_t)));
  if (!aBufOut) {
    aLengthOut = 0;
    return NS_ERROR_OUT_OF_MEMORY;
  }
  aLengthOut = unicodeLength;

  rv = unicodeDecoder->Convert(reinterpret_cast<const char*>(aData),
                               (int32_t *) &aLength, aBufOut,
                               &unicodeLength);
  MOZ_ASSERT(NS_SUCCEEDED(rv));
  aLengthOut = unicodeLength;
  if (NS_FAILED(rv)) {
    js_free(aBufOut);
    aBufOut = nullptr;
    aLengthOut = 0;
  }
  if (charset.Length() == 0) {
    charset = "?";
  }
  mozilla::Telemetry::Accumulate(mozilla::Telemetry::DOM_SCRIPT_SRC_ENCODING,
    charset);
  return rv;
}

nsresult
ScriptLoader::OnStreamComplete(nsIIncrementalStreamLoader* aLoader,
                               ScriptLoadRequest* aRequest,
                               nsresult aChannelStatus,
                               nsresult aSRIStatus,
                               mozilla::dom::SRICheckDataVerifier* aSRIDataVerifier)
{
  NS_ASSERTION(aRequest, "null request in stream complete handler");
  NS_ENSURE_TRUE(aRequest, NS_ERROR_FAILURE);

  nsCOMPtr<nsIRequest> channelRequest;
  aLoader->GetRequest(getter_AddRefs(channelRequest));
  nsCOMPtr<nsIChannel> channel;
  channel = do_QueryInterface(channelRequest);

  nsresult rv = NS_OK;
  if (!aRequest->mIntegrity.IsEmpty() &&
      NS_SUCCEEDED((rv = aSRIStatus))) {
    MOZ_ASSERT(aSRIDataVerifier);
    MOZ_ASSERT(mReporter);

    nsAutoCString sourceUri;
    if (mDocument && mDocument->GetDocumentURI()) {
      mDocument->GetDocumentURI()->GetAsciiSpec(sourceUri);
    }
    rv = aSRIDataVerifier->Verify(aRequest->mIntegrity, channel, sourceUri,
                                  mReporter);
    if (channelRequest) {
      mReporter->FlushReportsToConsole(
        nsContentUtils::GetInnerWindowID(channelRequest));
    } else {
      mReporter->FlushConsoleReports(mDocument);
    }
    if (NS_FAILED(rv)) {
      rv = NS_ERROR_SRI_CORRUPT;
    }
  } else {
    nsCOMPtr<nsILoadInfo> loadInfo = channel->GetLoadInfo();

    if (loadInfo && loadInfo->GetEnforceSRI()) {
      MOZ_LOG(SRILogHelper::GetSriLog(), mozilla::LogLevel::Debug,
              ("ScriptLoader::OnStreamComplete, required SRI not found"));
      nsCOMPtr<nsIContentSecurityPolicy> csp;
      loadInfo->LoadingPrincipal()->GetCsp(getter_AddRefs(csp));
      nsAutoCString violationURISpec;
      mDocument->GetDocumentURI()->GetAsciiSpec(violationURISpec);
      uint32_t lineNo = aRequest->mElement ? aRequest->mElement->GetScriptLineNumber() : 0;
      csp->LogViolationDetails(
        nsIContentSecurityPolicy::VIOLATION_TYPE_REQUIRE_SRI_FOR_SCRIPT,
        NS_ConvertUTF8toUTF16(violationURISpec),
        EmptyString(), lineNo, EmptyString(), EmptyString());
      rv = NS_ERROR_SRI_CORRUPT;
    }
  }

  bool sriOk = NS_SUCCEEDED(rv);

  // If we are loading from source, save the computed SRI hash or a dummy SRI
  // hash in case we are going to save the bytecode of this script in the cache.
  if (sriOk && aRequest->IsSource()) {
    MOZ_ASSERT(aRequest->mScriptBytecode.empty());
    // If the integrity metadata does not correspond to a valid hash function,
    // IsComplete would be false.
    if (!aRequest->mIntegrity.IsEmpty() && aSRIDataVerifier->IsComplete()) {
      // Encode the SRI computed hash.
      uint32_t len = aSRIDataVerifier->DataSummaryLength();
      if (!aRequest->mScriptBytecode.growBy(len)) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
      aRequest->mBytecodeOffset = len;

      DebugOnly<nsresult> res = aSRIDataVerifier->ExportDataSummary(
        aRequest->mScriptBytecode.length(),
        aRequest->mScriptBytecode.begin());
      MOZ_ASSERT(NS_SUCCEEDED(res));
    } else {
      // Encode a dummy SRI hash.
      uint32_t len = SRICheckDataVerifier::EmptyDataSummaryLength();
      if (!aRequest->mScriptBytecode.growBy(len)) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
      aRequest->mBytecodeOffset = len;

      DebugOnly<nsresult> res = SRICheckDataVerifier::ExportEmptyDataSummary(
        aRequest->mScriptBytecode.length(),
        aRequest->mScriptBytecode.begin());
      MOZ_ASSERT(NS_SUCCEEDED(res));
    }

    // Verify that the exported and predicted length correspond.
    mozilla::DebugOnly<uint32_t> srilen;
    MOZ_ASSERT(NS_SUCCEEDED(SRICheckDataVerifier::DataSummaryLength(
                              aRequest->mScriptBytecode.length(),
                              aRequest->mScriptBytecode.begin(),
                              &srilen)));
    MOZ_ASSERT(srilen == aRequest->mBytecodeOffset);
  }

  if (sriOk) {
    rv = PrepareLoadedRequest(aRequest, aLoader, aChannelStatus);
  }

  if (NS_FAILED(rv)) {
    if (sriOk && aRequest->mElement) {

      uint32_t lineNo = aRequest->mElement->GetScriptLineNumber();

      nsAutoString url;
      if (aRequest->mURI) {
        AppendUTF8toUTF16(aRequest->mURI->GetSpecOrDefault(), url);
      }

      const char16_t* params[] = { url.get() };

      nsContentUtils::ReportToConsole(nsIScriptError::warningFlag,
        NS_LITERAL_CSTRING("Script Loader"), mDocument,
        nsContentUtils::eDOM_PROPERTIES, "ScriptSourceLoadFailed",
        params, ArrayLength(params), nullptr,
        EmptyString(), lineNo);
    }

    /*
     * Handle script not loading error because source was a tracking URL.
     * We make a note of this script node by including it in a dedicated
     * array of blocked tracking nodes under its parent document.
     */
    if (rv == NS_ERROR_TRACKING_URI) {
      nsCOMPtr<nsIContent> cont = do_QueryInterface(aRequest->mElement);
      mDocument->AddBlockedTrackingNode(cont);
    }

    if (aChannelStatus == NS_BINDING_RETARGETED) {
      // When loading bytecode, we verify the SRI hash. If it does not match the
      // one from the document we restart the load, forcing us to load the
      // source instead. This test makes sure we do not remove the current
      // request from the following lists when we restart the load.
    } else if (aRequest->mIsDefer) {
      MOZ_ASSERT_IF(aRequest->IsModuleRequest(),
                    aRequest->AsModuleRequest()->IsTopLevel());
      if (aRequest->isInList()) {
        RefPtr<ScriptLoadRequest> req = mDeferRequests.Steal(aRequest);
        FireScriptAvailable(rv, req);
      }
    } else if (aRequest->mIsAsync) {
      MOZ_ASSERT_IF(aRequest->IsModuleRequest(),
                    aRequest->AsModuleRequest()->IsTopLevel());
      if (aRequest->isInList()) {
        RefPtr<ScriptLoadRequest> req = mLoadingAsyncRequests.Steal(aRequest);
        FireScriptAvailable(rv, req);
      }
    } else if (aRequest->mIsNonAsyncScriptInserted) {
      if (aRequest->isInList()) {
        RefPtr<ScriptLoadRequest> req =
          mNonAsyncExternalScriptInsertedRequests.Steal(aRequest);
        FireScriptAvailable(rv, req);
      }
    } else if (aRequest->mIsXSLT) {
      if (aRequest->isInList()) {
        RefPtr<ScriptLoadRequest> req = mXSLTRequests.Steal(aRequest);
        FireScriptAvailable(rv, req);
      }
    } else if (aRequest->IsModuleRequest()) {
      ModuleLoadRequest* modReq = aRequest->AsModuleRequest();
      MOZ_ASSERT(!modReq->IsTopLevel());
      MOZ_ASSERT(!modReq->isInList());
      modReq->Cancel();
      FireScriptAvailable(rv, aRequest);
    } else if (mParserBlockingRequest == aRequest) {
      MOZ_ASSERT(!aRequest->isInList());
      mParserBlockingRequest = nullptr;
      UnblockParser(aRequest);

      // Ensure that we treat aRequest->mElement as our current parser-inserted
      // script while firing onerror on it.
      MOZ_ASSERT(aRequest->mElement->GetParserCreated());
      nsCOMPtr<nsIScriptElement> oldParserInsertedScript =
        mCurrentParserInsertedScript;
      mCurrentParserInsertedScript = aRequest->mElement;
      FireScriptAvailable(rv, aRequest);
      ContinueParserAsync(aRequest);
      mCurrentParserInsertedScript = oldParserInsertedScript;
    } else {
      mPreloads.RemoveElement(aRequest, PreloadRequestComparator());
    }
  }

  // Process our request and/or any pending ones
  ProcessPendingRequests();

  return NS_OK;
}

void
ScriptLoader::UnblockParser(ScriptLoadRequest* aParserBlockingRequest)
{
  aParserBlockingRequest->mElement->UnblockParser();
}

void
ScriptLoader::ContinueParserAsync(ScriptLoadRequest* aParserBlockingRequest)
{
  aParserBlockingRequest->mElement->ContinueParserAsync();
}

uint32_t
ScriptLoader::NumberOfProcessors()
{
  if (mNumberOfProcessors > 0)
    return mNumberOfProcessors;

  int32_t numProcs = PR_GetNumberOfProcessors();
  if (numProcs > 0)
    mNumberOfProcessors = numProcs;
  return mNumberOfProcessors;
}

void
ScriptLoader::MaybeMoveToLoadedList(ScriptLoadRequest* aRequest)
{
  MOZ_ASSERT(aRequest->IsReadyToRun());

  // If it's async, move it to the loaded list.  aRequest->mIsAsync really
  // _should_ be in a list, but the consequences if it's not are bad enough we
  // want to avoid trying to move it if it's not.
  if (aRequest->mIsAsync) {
    MOZ_ASSERT(aRequest->isInList());
    if (aRequest->isInList()) {
      RefPtr<ScriptLoadRequest> req = mLoadingAsyncRequests.Steal(aRequest);
      mLoadedAsyncRequests.AppendElement(req);
    }
  }
}

nsresult
ScriptLoader::PrepareLoadedRequest(ScriptLoadRequest* aRequest,
                                   nsIIncrementalStreamLoader* aLoader,
                                   nsresult aStatus)
{
  if (NS_FAILED(aStatus)) {
    return aStatus;
  }

  if (aRequest->IsCanceled()) {
    return NS_BINDING_ABORTED;
  }
  MOZ_ASSERT(aRequest->IsLoading());

  // If we don't have a document, then we need to abort further
  // evaluation.
  if (!mDocument) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // If the load returned an error page, then we need to abort
  nsCOMPtr<nsIRequest> req;
  nsresult rv = aLoader->GetRequest(getter_AddRefs(req));
  NS_ASSERTION(req, "StreamLoader's request went away prematurely");
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(req);
  if (httpChannel) {
    bool requestSucceeded;
    rv = httpChannel->GetRequestSucceeded(&requestSucceeded);
    if (NS_SUCCEEDED(rv) && !requestSucceeded) {
      return NS_ERROR_NOT_AVAILABLE;
    }

    nsAutoCString sourceMapURL;
    rv = httpChannel->GetResponseHeader(NS_LITERAL_CSTRING("SourceMap"), sourceMapURL);
    if (NS_FAILED(rv)) {
      rv = httpChannel->GetResponseHeader(NS_LITERAL_CSTRING("X-SourceMap"), sourceMapURL);
    }
    if (NS_SUCCEEDED(rv)) {
      aRequest->mHasSourceMapURL = true;
      aRequest->mSourceMapURL = NS_ConvertUTF8toUTF16(sourceMapURL);
    }

    if (httpChannel->GetIsTrackingResource()) {
      aRequest->SetIsTracking();
    }
  }

  nsCOMPtr<nsIChannel> channel = do_QueryInterface(req);
  // If this load was subject to a CORS check; don't flag it with a
  // separate origin principal, so that it will treat our document's
  // principal as the origin principal
  if (aRequest->mCORSMode == CORS_NONE) {
    rv = nsContentUtils::GetSecurityManager()->
      GetChannelResultPrincipal(channel, getter_AddRefs(aRequest->mOriginPrincipal));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // This assertion could fire errorously if we ran out of memory when
  // inserting the request in the array. However it's an unlikely case
  // so if you see this assertion it is likely something else that is
  // wrong, especially if you see it more than once.
  NS_ASSERTION(mDeferRequests.Contains(aRequest) ||
               mLoadingAsyncRequests.Contains(aRequest) ||
               mNonAsyncExternalScriptInsertedRequests.Contains(aRequest) ||
               mXSLTRequests.Contains(aRequest)  ||
               (aRequest->IsModuleRequest() &&
                !aRequest->AsModuleRequest()->IsTopLevel() &&
                !aRequest->isInList()) ||
               mPreloads.Contains(aRequest, PreloadRequestComparator()) ||
               mParserBlockingRequest,
               "aRequest should be pending!");

  if (aRequest->IsModuleRequest()) {
    MOZ_ASSERT(aRequest->IsSource());
    ModuleLoadRequest* request = aRequest->AsModuleRequest();

    // When loading a module, only responses with a JavaScript MIME type are
    // acceptable.
    nsAutoCString mimeType;
    channel->GetContentType(mimeType);
    NS_ConvertUTF8toUTF16 typeString(mimeType);
    if (!nsContentUtils::IsJavascriptMIMEType(typeString)) {
      return NS_ERROR_FAILURE;
    }

    channel->GetURI(getter_AddRefs(request->mBaseURL));

    // Attempt to compile off main thread.
    rv = AttemptAsyncScriptCompile(request);
    if (NS_SUCCEEDED(rv)) {
      return rv;
    }

    // Otherwise compile it right away and start fetching descendents.
    return ProcessFetchedModuleSource(request);
  }

  // The script is now loaded and ready to run.
  aRequest->SetReady();

  // If this is currently blocking the parser, attempt to compile it off-main-thread.
  if (aRequest == mParserBlockingRequest && NumberOfProcessors() > 1) {
    MOZ_ASSERT(!aRequest->IsModuleRequest());
    nsresult rv = AttemptAsyncScriptCompile(aRequest);
    if (rv == NS_OK) {
      MOZ_ASSERT(aRequest->mProgress == ScriptLoadRequest::Progress::Compiling,
                 "Request should be off-thread compiling now.");
      return NS_OK;
    }

    // If off-thread compile errored, return the error.
    if (rv != NS_ERROR_FAILURE) {
      return rv;
    }

    // If off-thread compile was rejected, continue with regular processing.
  }

  MaybeMoveToLoadedList(aRequest);

  return NS_OK;
}

void
ScriptLoader::ParsingComplete(bool aTerminated)
{
  if (mDeferEnabled) {
    // Have to check because we apparently get ParsingComplete
    // without BeginDeferringScripts in some cases
    mDocumentParsingDone = true;
  }
  mDeferEnabled = false;
  if (aTerminated) {
    mDeferRequests.Clear();
    mLoadingAsyncRequests.Clear();
    mLoadedAsyncRequests.Clear();
    mNonAsyncExternalScriptInsertedRequests.Clear();
    mXSLTRequests.Clear();
    if (mParserBlockingRequest) {
      mParserBlockingRequest->Cancel();
      mParserBlockingRequest = nullptr;
    }
  }

  // Have to call this even if aTerminated so we'll correctly unblock
  // onload and all.
  ProcessPendingRequests();
}

void
ScriptLoader::PreloadURI(nsIURI* aURI, const nsAString& aCharset,
                         const nsAString& aType,
                         const nsAString& aCrossOrigin,
                         const nsAString& aIntegrity,
                         bool aScriptFromHead,
                         const mozilla::net::ReferrerPolicy aReferrerPolicy)
{
  NS_ENSURE_TRUE_VOID(mDocument);
  // Check to see if scripts has been turned off.
  if (!mEnabled || !mDocument->IsScriptEnabled()) {
    return;
  }

  // TODO: Preload module scripts.
  if (ModuleScriptsEnabled() && aType.LowerCaseEqualsASCII("module")) {
    return;
  }

  SRIMetadata sriMetadata;
  if (!aIntegrity.IsEmpty()) {
    MOZ_LOG(SRILogHelper::GetSriLog(), mozilla::LogLevel::Debug,
            ("ScriptLoader::PreloadURI, integrity=%s",
             NS_ConvertUTF16toUTF8(aIntegrity).get()));
    nsAutoCString sourceUri;
    if (mDocument->GetDocumentURI()) {
      mDocument->GetDocumentURI()->GetAsciiSpec(sourceUri);
    }
    SRICheck::IntegrityMetadata(aIntegrity, sourceUri, mReporter, &sriMetadata);
  }

  RefPtr<ScriptLoadRequest> request =
    CreateLoadRequest(ScriptKind::Classic, nullptr, 0,
                      Element::StringToCORSMode(aCrossOrigin), sriMetadata);
  request->mURI = aURI;
  request->mIsInline = false;
  request->mReferrerPolicy = aReferrerPolicy;
  request->mScriptFromHead = aScriptFromHead;

  nsresult rv = StartLoad(request);
  if (NS_FAILED(rv)) {
    return;
  }

  PreloadInfo* pi = mPreloads.AppendElement();
  pi->mRequest = request;
  pi->mCharset = aCharset;
}

void
ScriptLoader::AddDeferRequest(ScriptLoadRequest* aRequest)
{
  aRequest->mIsDefer = true;
  mDeferRequests.AppendElement(aRequest);
  if (mDeferEnabled && aRequest == mDeferRequests.getFirst() &&
      mDocument && !mBlockingDOMContentLoaded) {
    MOZ_ASSERT(mDocument->GetReadyStateEnum() == nsIDocument::READYSTATE_LOADING);
    mBlockingDOMContentLoaded = true;
    mDocument->BlockDOMContentLoaded();
  }
}

bool
ScriptLoader::MaybeRemovedDeferRequests()
{
  if (mDeferRequests.isEmpty() && mDocument &&
      mBlockingDOMContentLoaded) {
    mBlockingDOMContentLoaded = false;
    mDocument->UnblockDOMContentLoaded();
    return true;
  }
  return false;
}

#undef TRACE_FOR_TEST
#undef TRACE_FOR_TEST_BOOL
#undef TRACE_FOR_TEST_NONE

#undef LOG

} // dom namespace
} // mozilla namespace
