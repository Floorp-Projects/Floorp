/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * A class that handles loading and evaluation of <script> elements.
 */

#include "nsScriptLoader.h"

#include "prsystem.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "xpcpublic.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIContent.h"
#include "nsJSUtils.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/Element.h"
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

#include "mozilla/Attributes.h"
#include "mozilla/Unused.h"
#include "nsIScriptError.h"

using namespace mozilla;
using namespace mozilla::dom;

using JS::SourceBufferHolder;

static LazyLogModule gCspPRLog("CSP");

void
ImplCycleCollectionUnlink(nsScriptLoadRequestList& aField);

void
ImplCycleCollectionTraverse(nsCycleCollectionTraversalCallback& aCallback,
                            nsScriptLoadRequestList& aField,
                            const char* aName,
                            uint32_t aFlags = 0);

//////////////////////////////////////////////////////////////
// nsScriptLoadRequest
//////////////////////////////////////////////////////////////

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsScriptLoadRequest)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_0(nsScriptLoadRequest)

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsScriptLoadRequest)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsScriptLoadRequest)

nsScriptLoadRequest::~nsScriptLoadRequest()
{
  js_free(mScriptTextBuf);

  // We should always clean up any off-thread script parsing resources.
  MOZ_ASSERT(!mOffThreadToken);

  // But play it safe in release builds and try to clean them up here
  // as a fail safe.
  MaybeCancelOffThreadScript();
}

void
nsScriptLoadRequest::SetReady()
{
  MOZ_ASSERT(mProgress != Progress::Ready);
  mProgress = Progress::Ready;
}

void
nsScriptLoadRequest::Cancel()
{
  MaybeCancelOffThreadScript();
  mIsCanceled = true;
}

void
nsScriptLoadRequest::MaybeCancelOffThreadScript()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!mOffThreadToken) {
    return;
  }

  JSContext* cx = danger::GetJSContext();
  JS::CancelOffThreadScript(cx, mOffThreadToken);
  mOffThreadToken = nullptr;
}

//////////////////////////////////////////////////////////////
// nsModuleLoadRequest
//////////////////////////////////////////////////////////////

// A load request for a module, created for every top level module script and
// every module import.  Load request can share an nsModuleScript if there are
// multiple imports of the same module.

class nsModuleLoadRequest final : public nsScriptLoadRequest
{
  ~nsModuleLoadRequest() {}

  nsModuleLoadRequest(const nsModuleLoadRequest& aOther) = delete;
  nsModuleLoadRequest(nsModuleLoadRequest&& aOther) = delete;

public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(nsModuleLoadRequest, nsScriptLoadRequest)

  nsModuleLoadRequest(nsIScriptElement* aElement,
                      uint32_t aVersion,
                      CORSMode aCORSMode,
                      const SRIMetadata& aIntegrity,
                      nsScriptLoader* aLoader);

  bool IsTopLevel() const {
    return mIsTopLevel;
  }

  void SetReady() override;
  void Cancel() override;

  void ModuleLoaded();
  void DependenciesLoaded();
  void LoadFailed();

  // Is this a request for a top level module script or an import?
  bool mIsTopLevel;

  // The base URL used for resolving relative module imports.
  nsCOMPtr<nsIURI> mBaseURL;

  // Pointer to the script loader, used to trigger actions when the module load
  // finishes.
  RefPtr<nsScriptLoader> mLoader;

  // The importing module, or nullptr for top level module scripts.  Used to
  // implement the ancestor list checked when fetching module dependencies.
  RefPtr<nsModuleLoadRequest> mParent;

  // Set to a module script object after a successful load or nullptr on
  // failure.
  RefPtr<nsModuleScript> mModuleScript;

  // A promise that is completed on successful load of this module and all of
  // its dependencies, indicating that the module is ready for instantiation and
  // evaluation.
  MozPromiseHolder<GenericPromise> mReady;

  // Array of imported modules.
  nsTArray<RefPtr<nsModuleLoadRequest>> mImports;
};

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(nsModuleLoadRequest)
NS_INTERFACE_MAP_END_INHERITING(nsScriptLoadRequest)

NS_IMPL_CYCLE_COLLECTION_INHERITED(nsModuleLoadRequest, nsScriptLoadRequest,
                                   mBaseURL,
                                   mLoader,
                                   mParent,
                                   mModuleScript,
                                   mImports)

NS_IMPL_ADDREF_INHERITED(nsModuleLoadRequest, nsScriptLoadRequest)
NS_IMPL_RELEASE_INHERITED(nsModuleLoadRequest, nsScriptLoadRequest)

nsModuleLoadRequest::nsModuleLoadRequest(nsIScriptElement* aElement,
                                         uint32_t aVersion,
                                         CORSMode aCORSMode,
                                         const SRIMetadata &aIntegrity,
                                         nsScriptLoader* aLoader)
  : nsScriptLoadRequest(nsScriptKind::Module,
                        aElement,
                        aVersion,
                        aCORSMode,
                        aIntegrity),
    mIsTopLevel(true),
    mLoader(aLoader)
{}

inline nsModuleLoadRequest*
nsScriptLoadRequest::AsModuleRequest()
{
  MOZ_ASSERT(IsModuleRequest());
  return static_cast<nsModuleLoadRequest*>(this);
}

void nsModuleLoadRequest::Cancel()
{
  nsScriptLoadRequest::Cancel();
  mModuleScript = nullptr;
  mProgress = nsScriptLoadRequest::Progress::Ready;
  for (size_t i = 0; i < mImports.Length(); i++) {
    mImports[i]->Cancel();
  }
  mReady.RejectIfExists(NS_ERROR_FAILURE, __func__);
}

void
nsModuleLoadRequest::SetReady()
{
#ifdef DEBUG
  for (size_t i = 0; i < mImports.Length(); i++) {
    MOZ_ASSERT(mImports[i]->IsReadyToRun());
  }
#endif

  nsScriptLoadRequest::SetReady();
  mReady.ResolveIfExists(true, __func__);
}

void
nsModuleLoadRequest::ModuleLoaded()
{
  // A module that was found to be marked as fetching in the module map has now
  // been loaded.

  mModuleScript = mLoader->GetFetchedModule(mURI);
  mLoader->StartFetchingModuleDependencies(this);
}

void
nsModuleLoadRequest::DependenciesLoaded()
{
  // The module and all of its dependencies have been successfully fetched and
  // compiled.

  if (!mLoader->InstantiateModuleTree(this)) {
    LoadFailed();
    return;
  }

  SetReady();
  mLoader->ProcessLoadedModuleTree(this);
  mLoader = nullptr;
  mParent = nullptr;
}

void
nsModuleLoadRequest::LoadFailed()
{
  Cancel();
  mLoader->ProcessLoadedModuleTree(this);
  mLoader = nullptr;
  mParent = nullptr;
}

//////////////////////////////////////////////////////////////
// nsModuleScript
//////////////////////////////////////////////////////////////

// A single module script.  May be used to satisfy multiple load requests.

class nsModuleScript final : public nsISupports
{
  enum InstantiationState {
    Uninstantiated,
    Instantiated,
    Errored
  };

  RefPtr<nsScriptLoader> mLoader;
  nsCOMPtr<nsIURI> mBaseURL;
  JS::Heap<JSObject*> mModuleRecord;
  JS::Heap<JS::Value> mException;
  InstantiationState mInstantiationState;

  ~nsModuleScript();

public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(nsModuleScript)

  nsModuleScript(nsScriptLoader* aLoader,
                 nsIURI* aBaseURL,
                 JS::Handle<JSObject*> aModuleRecord);

  nsScriptLoader* Loader() const { return mLoader; }
  JSObject* ModuleRecord() const { return mModuleRecord; }
  JS::Value Exception() const { return mException; }
  nsIURI* BaseURL() const { return mBaseURL; }

  void SetInstantiationResult(JS::Handle<JS::Value> aMaybeException);
  bool IsUninstantiated() const {
    return mInstantiationState == Uninstantiated;
  }
  bool IsInstantiated() const {
    return mInstantiationState == Instantiated;
  }
  bool InstantiationFailed() const {
    return mInstantiationState == Errored;
  }

  void UnlinkModuleRecord();
};

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsModuleScript)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_CLASS(nsModuleScript)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsModuleScript)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mLoader)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mBaseURL)
  tmp->UnlinkModuleRecord();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsModuleScript)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mLoader)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(nsModuleScript)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mModuleRecord)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mException)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsModuleScript)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsModuleScript)

nsModuleScript::nsModuleScript(nsScriptLoader *aLoader, nsIURI* aBaseURL,
                               JS::Handle<JSObject*> aModuleRecord)
 : mLoader(aLoader),
   mBaseURL(aBaseURL),
   mModuleRecord(aModuleRecord),
   mInstantiationState(Uninstantiated)
{
  MOZ_ASSERT(mLoader);
  MOZ_ASSERT(mBaseURL);
  MOZ_ASSERT(mModuleRecord);
  MOZ_ASSERT(mException.isUndefined());

  // Make module's host defined field point to this module script object.
  // This is cleared in the UnlinkModuleRecord().
  JS::SetModuleHostDefinedField(mModuleRecord, JS::PrivateValue(this));
  HoldJSObjects(this);
}

void
nsModuleScript::UnlinkModuleRecord()
{
  // Remove module's back reference to this object request if present.
  if (mModuleRecord) {
    MOZ_ASSERT(JS::GetModuleHostDefinedField(mModuleRecord).toPrivate() ==
               this);
    JS::SetModuleHostDefinedField(mModuleRecord, JS::UndefinedValue());
  }
  mModuleRecord = nullptr;
  mException.setUndefined();
}

nsModuleScript::~nsModuleScript()
{
  if (mModuleRecord) {
    // The object may be destroyed without being unlinked first.
    UnlinkModuleRecord();
  }
  DropJSObjects(this);
}

void
nsModuleScript::SetInstantiationResult(JS::Handle<JS::Value> aMaybeException)
{
  MOZ_ASSERT(mInstantiationState == Uninstantiated);
  MOZ_ASSERT(mModuleRecord);
  MOZ_ASSERT(mException.isUndefined());

  if (aMaybeException.isUndefined()) {
    mInstantiationState = Instantiated;
  } else {
    mModuleRecord = nullptr;
    mException = aMaybeException;
    mInstantiationState = Errored;
  }
}

//////////////////////////////////////////////////////////////

// nsScriptLoadRequestList
//////////////////////////////////////////////////////////////

nsScriptLoadRequestList::~nsScriptLoadRequestList()
{
  Clear();
}

void
nsScriptLoadRequestList::Clear()
{
  while (!isEmpty()) {
    RefPtr<nsScriptLoadRequest> first = StealFirst();
    first->Cancel();
    // And just let it go out of scope and die.
  }
}

#ifdef DEBUG
bool
nsScriptLoadRequestList::Contains(nsScriptLoadRequest* aElem) const
{
  for (const nsScriptLoadRequest* req = getFirst();
       req; req = req->getNext()) {
    if (req == aElem) {
      return true;
    }
  }

  return false;
}
#endif // DEBUG

inline void
ImplCycleCollectionUnlink(nsScriptLoadRequestList& aField)
{
  while (!aField.isEmpty()) {
    RefPtr<nsScriptLoadRequest> first = aField.StealFirst();
  }
}

inline void
ImplCycleCollectionTraverse(nsCycleCollectionTraversalCallback& aCallback,
                            nsScriptLoadRequestList& aField,
                            const char* aName,
                            uint32_t aFlags)
{
  for (nsScriptLoadRequest* request = aField.getFirst();
       request; request = request->getNext())
  {
    CycleCollectionNoteChild(aCallback, request, aName, aFlags);
  }
}

//////////////////////////////////////////////////////////////
// nsScriptLoader::PreloadInfo
//////////////////////////////////////////////////////////////

inline void
ImplCycleCollectionUnlink(nsScriptLoader::PreloadInfo& aField)
{
  ImplCycleCollectionUnlink(aField.mRequest);
}

inline void
ImplCycleCollectionTraverse(nsCycleCollectionTraversalCallback& aCallback,
                            nsScriptLoader::PreloadInfo& aField,
                            const char* aName,
                            uint32_t aFlags = 0)
{
  ImplCycleCollectionTraverse(aCallback, aField.mRequest, aName, aFlags);
}

//////////////////////////////////////////////////////////////
// nsScriptLoader
//////////////////////////////////////////////////////////////

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsScriptLoader)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION(nsScriptLoader,
                         mNonAsyncExternalScriptInsertedRequests,
                         mLoadingAsyncRequests,
                         mLoadedAsyncRequests,
                         mDeferRequests,
                         mXSLTRequests,
                         mParserBlockingRequest,
                         mPreloads,
                         mPendingChildLoaders,
                         mFetchedModules)

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsScriptLoader)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsScriptLoader)

nsScriptLoader::nsScriptLoader(nsIDocument *aDocument)
  : mDocument(aDocument),
    mParserBlockingBlockerCount(0),
    mBlockerCount(0),
    mNumberOfProcessors(0),
    mEnabled(true),
    mDeferEnabled(false),
    mDocumentParsingDone(false),
    mBlockingDOMContentLoaded(false),
    mReporter(new ConsoleReportCollector())
{
}

nsScriptLoader::~nsScriptLoader()
{
  mObservers.Clear();

  if (mParserBlockingRequest) {
    mParserBlockingRequest->FireScriptAvailable(NS_ERROR_ABORT);
  }

  for (nsScriptLoadRequest* req = mXSLTRequests.getFirst(); req;
       req = req->getNext()) {
    req->FireScriptAvailable(NS_ERROR_ABORT);
  }

  for (nsScriptLoadRequest* req = mDeferRequests.getFirst(); req;
       req = req->getNext()) {
    req->FireScriptAvailable(NS_ERROR_ABORT);
  }

  for (nsScriptLoadRequest* req = mLoadingAsyncRequests.getFirst(); req;
       req = req->getNext()) {
    req->FireScriptAvailable(NS_ERROR_ABORT);
  }

  for (nsScriptLoadRequest* req = mLoadedAsyncRequests.getFirst(); req;
       req = req->getNext()) {
    req->FireScriptAvailable(NS_ERROR_ABORT);
  }

  for(nsScriptLoadRequest* req = mNonAsyncExternalScriptInsertedRequests.getFirst();
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
nsScriptLoader::CheckContentPolicy(nsIDocument* aDocument,
                                   nsISupports *aContext,
                                   nsIURI *aURI,
                                   const nsAString &aType,
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
nsScriptLoader::ModuleMapContainsModule(nsModuleLoadRequest *aRequest) const
{
  // Returns whether we have fetched, or are currently fetching, a module script
  // for the request's URL.
  return mFetchingModules.Contains(aRequest->mURI) ||
         mFetchedModules.Contains(aRequest->mURI);
}

bool
nsScriptLoader::IsFetchingModule(nsModuleLoadRequest *aRequest) const
{
  bool fetching = mFetchingModules.Contains(aRequest->mURI);
  MOZ_ASSERT_IF(fetching, !mFetchedModules.Contains(aRequest->mURI));
  return fetching;
}

void
nsScriptLoader::SetModuleFetchStarted(nsModuleLoadRequest *aRequest)
{
  // Update the module map to indicate that a module is currently being fetched.

  MOZ_ASSERT(aRequest->IsLoading());
  MOZ_ASSERT(!ModuleMapContainsModule(aRequest));
  mFetchingModules.Put(aRequest->mURI, nullptr);
}

void
nsScriptLoader::SetModuleFetchFinishedAndResumeWaitingRequests(nsModuleLoadRequest *aRequest,
                                                               nsresult aResult)
{
  // Update module map with the result of fetching a single module script.  The
  // module script pointer is nullptr on error.

  MOZ_ASSERT(!aRequest->IsReadyToRun());

  RefPtr<GenericPromise::Private> promise;
  MOZ_ALWAYS_TRUE(mFetchingModules.Get(aRequest->mURI, getter_AddRefs(promise)));
  mFetchingModules.Remove(aRequest->mURI);

  RefPtr<nsModuleScript> ms(aRequest->mModuleScript);
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
nsScriptLoader::WaitForModuleFetch(nsModuleLoadRequest *aRequest)
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

  RefPtr<nsModuleScript> ms;
  MOZ_ALWAYS_TRUE(mFetchedModules.Get(aRequest->mURI, getter_AddRefs(ms)));
  if (!ms) {
    return GenericPromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
  }

  return GenericPromise::CreateAndResolve(true, __func__);
}

nsModuleScript*
nsScriptLoader::GetFetchedModule(nsIURI* aURL) const
{
  bool found;
  nsModuleScript* ms = mFetchedModules.GetWeak(aURL, &found);
  MOZ_ASSERT(found);
  return ms;
}

nsresult
nsScriptLoader::ProcessFetchedModuleSource(nsModuleLoadRequest* aRequest)
{
  MOZ_ASSERT(!aRequest->mModuleScript);

  nsresult rv = CreateModuleScript(aRequest);
  SetModuleFetchFinishedAndResumeWaitingRequests(aRequest, rv);

  free(aRequest->mScriptTextBuf);
  aRequest->mScriptTextBuf = nullptr;
  aRequest->mScriptTextLength = 0;

  if (NS_SUCCEEDED(rv)) {
    StartFetchingModuleDependencies(aRequest);
  }

  return rv;
}

nsresult
nsScriptLoader::CreateModuleScript(nsModuleLoadRequest* aRequest)
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
        new nsModuleScript(this, aRequest->mBaseURL, module);
    }
  }

  context->SetProcessingScriptTag(oldProcessingScriptTag);

  return rv;
}

static bool
ThrowTypeError(JSContext* aCx, nsModuleScript* aScript,
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
HandleResolveFailure(JSContext* aCx, nsModuleScript* aScript,
                     const nsAString& aSpecifier)
{
  // TODO: How can we get the line number of the failed import?

  nsAutoString message(NS_LITERAL_STRING("Error resolving module specifier: "));
  message.Append(aSpecifier);

  return ThrowTypeError(aCx, aScript, message);
}

static bool
HandleModuleNotFound(JSContext* aCx, nsModuleScript* aScript,
                     const nsAString& aSpecifier)
{
  // TODO: How can we get the line number of the failed import?

  nsAutoString message(NS_LITERAL_STRING("Resolved module not found in map: "));
  message.Append(aSpecifier);

  return ThrowTypeError(aCx, aScript, message);
}

static already_AddRefed<nsIURI>
ResolveModuleSpecifier(nsModuleScript* aScript,
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
RequestedModuleIsInAncestorList(nsModuleLoadRequest* aRequest, nsIURI* aURL, bool* aResult)
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
ResolveRequestedModules(nsModuleLoadRequest* aRequest, nsCOMArray<nsIURI> &aUrls)
{
  nsModuleScript* ms = aRequest->mModuleScript;

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
    nsModuleScript* ms = aRequest->mModuleScript;
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
nsScriptLoader::StartFetchingModuleDependencies(nsModuleLoadRequest* aRequest)
{
  MOZ_ASSERT(aRequest->mModuleScript);
  MOZ_ASSERT(!aRequest->IsReadyToRun());
  aRequest->mProgress = nsModuleLoadRequest::Progress::FetchingImports;

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
    GenericPromise::All(AbstractThread::GetCurrent(), importsReady);
  allReady->Then(AbstractThread::GetCurrent(), __func__, aRequest,
                 &nsModuleLoadRequest::DependenciesLoaded,
                 &nsModuleLoadRequest::LoadFailed);
}

RefPtr<GenericPromise>
nsScriptLoader::StartFetchingModuleAndDependencies(nsModuleLoadRequest* aRequest,
                                                   nsIURI* aURI)
{
  MOZ_ASSERT(aURI);

  RefPtr<nsModuleLoadRequest> childRequest =
    new nsModuleLoadRequest(aRequest->mElement, aRequest->mJSVersion,
                            aRequest->mCORSMode, aRequest->mIntegrity, this);

  childRequest->mIsTopLevel = false;
  childRequest->mURI = aURI;
  childRequest->mIsInline = false;
  childRequest->mReferrerPolicy = aRequest->mReferrerPolicy;
  childRequest->mParent = aRequest;

  RefPtr<GenericPromise> ready = childRequest->mReady.Ensure(__func__);

  nsresult rv = StartLoad(childRequest, NS_LITERAL_STRING("module"), false);
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
  auto script = static_cast<nsModuleScript*>(value.toPrivate());
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
  nsModuleScript* ms = script->Loader()->GetFetchedModule(uri);
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
nsScriptLoader::ProcessLoadedModuleTree(nsModuleLoadRequest* aRequest)
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
nsScriptLoader::InstantiateModuleTree(nsModuleLoadRequest* aRequest)
{
  // Perform eager instantiation of the loaded module tree.

  MOZ_ASSERT(aRequest);

  nsModuleScript* ms = aRequest->mModuleScript;
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

  mozilla::Vector<nsModuleLoadRequest*, 1> requests;
  if (!requests.append(aRequest)) {
    return false;
  }

  while (!requests.empty()) {
    nsModuleLoadRequest* request = requests.popCopy();
    nsModuleScript* ms = request->mModuleScript;
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

nsresult
nsScriptLoader::StartLoad(nsScriptLoadRequest *aRequest, const nsAString &aType,
                          bool aScriptFromHead)
{
  MOZ_ASSERT(aRequest->IsLoading());
  NS_ENSURE_TRUE(mDocument, NS_ERROR_NULL_POINTER);

  // If this document is sandboxed without 'allow-scripts', abort.
  if (mDocument->HasScriptsBlockedBySandbox()) {
    return NS_OK;
  }

  if (aRequest->IsModuleRequest()) {
    // Check whether the module has been fetched or is currently being fetched,
    // and if so wait for it.
    nsModuleLoadRequest* request = aRequest->AsModuleRequest();
    if (ModuleMapContainsModule(request)) {
      WaitForModuleFetch(request)
        ->Then(AbstractThread::GetCurrent(), __func__, request,
               &nsModuleLoadRequest::ModuleLoaded,
               &nsModuleLoadRequest::LoadFailed);
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
  nsIDocShell *docshell = window->GetDocShell();
  nsCOMPtr<nsIInterfaceRequestor> prompter(do_QueryInterface(docshell));

  nsSecurityFlags securityFlags;
  // TODO: the spec currently gives module scripts different CORS behaviour to
  // classic scripts.
  securityFlags = aRequest->mCORSMode == CORS_NONE
    ? nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL
    : nsILoadInfo::SEC_REQUIRE_CORS_DATA_INHERITS;
  if (aRequest->mCORSMode == CORS_ANONYMOUS) {
    securityFlags |= nsILoadInfo::SEC_COOKIES_SAME_ORIGIN;
  } else if (aRequest->mCORSMode == CORS_USE_CREDENTIALS) {
    securityFlags |= nsILoadInfo::SEC_COOKIES_INCLUDE;
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

  nsIScriptElement *script = aRequest->mElement;
  nsCOMPtr<nsIClassOfService> cos(do_QueryInterface(channel));

  if (cos) {
    if (aScriptFromHead &&
        !(script && (script->GetScriptAsync() || script->GetScriptDeferred()))) {
      // synchronous head scripts block lading of most other non js/css
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
    httpChannel->SetRequestHeader(NS_LITERAL_CSTRING("Accept"),
                                  NS_LITERAL_CSTRING("*/*"),
                                  false);
    httpChannel->SetReferrerWithPolicy(mDocument->GetDocumentURI(),
                                       aRequest->mReferrerPolicy);

    nsCOMPtr<nsIHttpChannelInternal> internalChannel(do_QueryInterface(httpChannel));
    if (internalChannel) {
      internalChannel->SetIntegrityMetadata(aRequest->mIntegrity.GetIntegrityString());
    }
  }

  nsCOMPtr<nsILoadContext> loadContext(do_QueryInterface(docshell));
  mozilla::net::PredictorLearn(aRequest->mURI, mDocument->GetDocumentURI(),
      nsINetworkPredictor::LEARN_LOAD_SUBRESOURCE, loadContext);

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

  RefPtr<nsScriptLoadHandler> handler =
      new nsScriptLoadHandler(this, aRequest, sriDataVerifier.forget());

  nsCOMPtr<nsIIncrementalStreamLoader> loader;
  rv = NS_NewIncrementalStreamLoader(getter_AddRefs(loader), handler);
  NS_ENSURE_SUCCESS(rv, rv);

  return channel->AsyncOpen2(loader);
}

bool
nsScriptLoader::PreloadURIComparator::Equals(const PreloadInfo &aPi,
                                             nsIURI * const &aURI) const
{
  bool same;
  return NS_SUCCEEDED(aPi.mRequest->mURI->Equals(aURI, &same)) &&
         same;
}

class nsScriptRequestProcessor : public Runnable
{
private:
  RefPtr<nsScriptLoader> mLoader;
  RefPtr<nsScriptLoadRequest> mRequest;
public:
  nsScriptRequestProcessor(nsScriptLoader* aLoader,
                           nsScriptLoadRequest* aRequest)
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
CSPAllowsInlineScript(nsIScriptElement *aElement, nsIDocument *aDocument)
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

nsScriptLoadRequest*
nsScriptLoader::CreateLoadRequest(nsScriptKind aKind,
                                  nsIScriptElement* aElement,
                                  uint32_t aVersion, CORSMode aCORSMode,
                                  const SRIMetadata &aIntegrity)
{
  if (aKind == nsScriptKind::Classic) {
    return new nsScriptLoadRequest(aKind, aElement, aVersion, aCORSMode,
                                   aIntegrity);
  }

  MOZ_ASSERT(aKind == nsScriptKind::Module);
  return new nsModuleLoadRequest(aElement, aVersion, aCORSMode, aIntegrity,
                                 this);
}

bool
nsScriptLoader::ProcessScriptElement(nsIScriptElement *aElement)
{
  // We need a document to evaluate scripts.
  NS_ENSURE_TRUE(mDocument, false);

  // Check to see if scripts has been turned off.
  if (!mEnabled || !mDocument->IsScriptEnabled()) {
    return false;
  }

  NS_ASSERTION(!aElement->IsMalformed(), "Executing malformed script");

  nsCOMPtr<nsIContent> scriptContent = do_QueryInterface(aElement);

  // Step 12. Check that the script is not an eventhandler
  if (IsScriptEventHandler(scriptContent)) {
    return false;
  }

  JSVersion version = JSVERSION_DEFAULT;

  // Check the type attribute to determine language and version.
  // If type exists, it trumps the deprecated 'language='
  nsAutoString type;
  bool hasType = aElement->GetScriptType(type);

  nsScriptKind scriptKind = nsScriptKind::Classic;
  if (!type.IsEmpty()) {
    // Support type="module" only for chrome documents.
    if (nsContentUtils::IsChromeDoc(mDocument) && type.LowerCaseEqualsASCII("module")) {
      scriptKind = nsScriptKind::Module;
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

  // Step 14. in the HTML5 spec
  nsresult rv = NS_OK;
  RefPtr<nsScriptLoadRequest> request;
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
                 ("nsScriptLoader::ProcessScriptElement, integrity=%s",
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

      // set aScriptFromHead to false so we don't treat non preloaded scripts as
      // blockers for full page load. See bug 792438.
      rv = StartLoad(request, type, false);
      if (NS_FAILED(rv)) {
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
    if (!aElement->GetParserCreated() && !request->IsModuleRequest()) {
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

  if (request->IsModuleRequest()) {
    nsModuleLoadRequest* modReq = request->AsModuleRequest();
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
  request->mProgress = nsScriptLoadRequest::Progress::Ready;
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
    nsContentUtils::AddScriptRunner(new nsScriptRequestProcessor(this,
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
  RefPtr<nsScriptLoadRequest> mRequest;
  RefPtr<nsScriptLoader> mLoader;
  void *mToken;

public:
  NotifyOffThreadScriptLoadCompletedRunnable(nsScriptLoadRequest* aRequest,
                                             nsScriptLoader* aLoader)
    : mRequest(aRequest), mLoader(aLoader), mToken(nullptr)
  {}

  virtual ~NotifyOffThreadScriptLoadCompletedRunnable();

  void SetToken(void* aToken) {
    MOZ_ASSERT(aToken && !mToken);
    mToken = aToken;
  }

  NS_DECL_NSIRUNNABLE
};

} /* anonymous namespace */

nsresult
nsScriptLoader::ProcessOffThreadRequest(nsScriptLoadRequest* aRequest)
{
  MOZ_ASSERT(aRequest->mProgress == nsScriptLoadRequest::Progress::Compiling);
  MOZ_ASSERT(!aRequest->mWasCompiledOMT);

  aRequest->mWasCompiledOMT = true;

  if (aRequest->IsModuleRequest()) {
    MOZ_ASSERT(aRequest->mOffThreadToken);
    nsModuleLoadRequest* request = aRequest->AsModuleRequest();
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
  RefPtr<nsScriptLoadRequest> request = mRequest.forget();
  RefPtr<nsScriptLoader> loader = mLoader.forget();

  request->mOffThreadToken = mToken;
  nsresult rv = loader->ProcessOffThreadRequest(request);

  return rv;
}

static void
OffThreadScriptLoaderCallback(void *aToken, void *aCallbackData)
{
  RefPtr<NotifyOffThreadScriptLoadCompletedRunnable> aRunnable =
    dont_AddRef(static_cast<NotifyOffThreadScriptLoadCompletedRunnable*>(aCallbackData));
  aRunnable->SetToken(aToken);
  NS_DispatchToMainThread(aRunnable);
}

nsresult
nsScriptLoader::AttemptAsyncScriptCompile(nsScriptLoadRequest* aRequest)
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

  if (!JS::CanCompileOffThread(cx, options, aRequest->mScriptTextLength)) {
    return NS_ERROR_FAILURE;
  }

  RefPtr<NotifyOffThreadScriptLoadCompletedRunnable> runnable =
    new NotifyOffThreadScriptLoadCompletedRunnable(aRequest, this);

  if (aRequest->IsModuleRequest()) {
    if (!JS::CompileOffThreadModule(cx, options,
                                    aRequest->mScriptTextBuf, aRequest->mScriptTextLength,
                                    OffThreadScriptLoaderCallback,
                                    static_cast<void*>(runnable))) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  } else {
    if (!JS::CompileOffThread(cx, options,
                              aRequest->mScriptTextBuf, aRequest->mScriptTextLength,
                              OffThreadScriptLoaderCallback,
                              static_cast<void*>(runnable))) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  mDocument->BlockOnload();
  aRequest->mProgress = nsScriptLoadRequest::Progress::Compiling;

  Unused << runnable.forget();
  return NS_OK;
}

nsresult
nsScriptLoader::CompileOffThreadOrProcessRequest(nsScriptLoadRequest* aRequest)
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
nsScriptLoader::GetScriptSource(nsScriptLoadRequest* aRequest, nsAutoString& inlineData)
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

  return SourceBufferHolder(aRequest->mScriptTextBuf,
                            aRequest->mScriptTextLength,
                            SourceBufferHolder::NoOwnership);
}

nsresult
nsScriptLoader::ProcessRequest(nsScriptLoadRequest* aRequest)
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

  nsPIDOMWindowInner *pwin = master->GetInnerWindow();
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

  // Free any source data.
  free(aRequest->mScriptTextBuf);
  aRequest->mScriptTextBuf = nullptr;
  aRequest->mScriptTextLength = 0;

  return rv;
}

void
nsScriptLoader::FireScriptAvailable(nsresult aResult,
                                    nsScriptLoadRequest* aRequest)
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
nsScriptLoader::FireScriptEvaluated(nsresult aResult,
                                    nsScriptLoadRequest* aRequest)
{
  for (int32_t i = 0; i < mObservers.Count(); i++) {
    nsCOMPtr<nsIScriptLoaderObserver> obs = mObservers[i];
    obs->ScriptEvaluated(aResult, aRequest->mElement,
                         aRequest->mIsInline);
  }

  aRequest->FireScriptEvaluated(aResult);
}

already_AddRefed<nsIScriptGlobalObject>
nsScriptLoader::GetScriptGlobalObject()
{
  nsCOMPtr<nsIDocument> master = mDocument->MasterDocument();
  nsPIDOMWindowInner *pwin = master->GetInnerWindow();
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
nsScriptLoader::FillCompileOptionsForRequest(const AutoJSAPI&jsapi,
                                             nsScriptLoadRequest* aRequest,
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

  bool isScriptElement = !aRequest->IsModuleRequest() ||
                         aRequest->AsModuleRequest()->IsTopLevel();
  aOptions->setIntroductionType(isScriptElement ? "scriptElement"
                                                : "importedModule");
  aOptions->setFileAndLine(aRequest->mURL.get(), aRequest->mLineNo);
  aOptions->setVersion(JSVersion(aRequest->mJSVersion));
  aOptions->setIsRunOnce(true);
  // We only need the setNoScriptRval bit when compiling off-thread here, since
  // otherwise nsJSUtils::EvaluateString will set it up for us.
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
nsScriptLoader::EvaluateScript(nsScriptLoadRequest* aRequest)
{
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
      nsModuleLoadRequest* request = aRequest->AsModuleRequest();
      MOZ_ASSERT(request->mModuleScript);
      MOZ_ASSERT(!request->mOffThreadToken);
      nsModuleScript* ms = request->mModuleScript;
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
    } else {
      JS::CompileOptions options(aes.cx());
      rv = FillCompileOptionsForRequest(aes, aRequest, global, &options);

      if (NS_SUCCEEDED(rv)) {
        nsAutoString inlineData;
        SourceBufferHolder srcBuf = GetScriptSource(aRequest, inlineData);
        rv = nsJSUtils::EvaluateString(aes.cx(), srcBuf, global, options,
                                       aRequest->OffThreadTokenPtr());
      }
    }
  }

  context->SetProcessingScriptTag(oldProcessingScriptTag);
  return rv;
}

void
nsScriptLoader::ProcessPendingRequestsAsync()
{
  if (mParserBlockingRequest ||
      !mXSLTRequests.isEmpty() ||
      !mLoadedAsyncRequests.isEmpty() ||
      !mNonAsyncExternalScriptInsertedRequests.isEmpty() ||
      !mDeferRequests.isEmpty() ||
      !mPendingChildLoaders.IsEmpty()) {
    NS_DispatchToCurrentThread(NewRunnableMethod(this,
                                                 &nsScriptLoader::ProcessPendingRequests));
  }
}

void
nsScriptLoader::ProcessPendingRequests()
{
  RefPtr<nsScriptLoadRequest> request;

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
    RefPtr<nsScriptLoader> child = mPendingChildLoaders[0];
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
nsScriptLoader::ReadyToExecuteParserBlockingScripts()
{
  // Make sure the SelfReadyToExecuteParserBlockingScripts check is first, so
  // that we don't block twice on an ancestor.
  if (!SelfReadyToExecuteParserBlockingScripts()) {
    return false;
  }

  for (nsIDocument* doc = mDocument; doc; doc = doc->GetParentDocument()) {
    nsScriptLoader* ancestor = doc->ScriptLoader();
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
nsScriptLoader::ConvertToUTF16(nsIChannel* aChannel, const uint8_t* aData,
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
    unicodeDecoder = EncodingUtils::DecoderForEncoding("windows-1252");
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
  return rv;
}

nsresult
nsScriptLoader::OnStreamComplete(nsIIncrementalStreamLoader* aLoader,
                                 nsISupports* aContext,
                                 nsresult aChannelStatus,
                                 nsresult aSRIStatus,
                                 mozilla::Vector<char16_t> &aString,
                                 mozilla::dom::SRICheckDataVerifier* aSRIDataVerifier)
{
  nsScriptLoadRequest* request = static_cast<nsScriptLoadRequest*>(aContext);
  NS_ASSERTION(request, "null request in stream complete handler");
  NS_ENSURE_TRUE(request, NS_ERROR_FAILURE);

  nsCOMPtr<nsIRequest> channelRequest;
  aLoader->GetRequest(getter_AddRefs(channelRequest));
  nsCOMPtr<nsIChannel> channel;
  channel = do_QueryInterface(channelRequest);

  nsresult rv = NS_OK;
  if (!request->mIntegrity.IsEmpty() &&
      NS_SUCCEEDED((rv = aSRIStatus))) {
    MOZ_ASSERT(aSRIDataVerifier);
    MOZ_ASSERT(mReporter);

    nsAutoCString sourceUri;
    if (mDocument && mDocument->GetDocumentURI()) {
      mDocument->GetDocumentURI()->GetAsciiSpec(sourceUri);
    }
    rv = aSRIDataVerifier->Verify(request->mIntegrity, channel, sourceUri,
                                  mReporter);
    mReporter->FlushConsoleReports(mDocument);
    if (NS_FAILED(rv)) {
      rv = NS_ERROR_SRI_CORRUPT;
    }
  } else {
    nsCOMPtr<nsILoadInfo> loadInfo = channel->GetLoadInfo();

    if (loadInfo->GetEnforceSRI()) {
      MOZ_LOG(SRILogHelper::GetSriLog(), mozilla::LogLevel::Debug,
             ("nsScriptLoader::OnStreamComplete, required SRI not found"));
      nsCOMPtr<nsIContentSecurityPolicy> csp;
      loadInfo->LoadingPrincipal()->GetCsp(getter_AddRefs(csp));
      nsAutoCString violationURISpec;
      mDocument->GetDocumentURI()->GetAsciiSpec(violationURISpec);
      uint32_t lineNo = request->mElement ? request->mElement->GetScriptLineNumber() : 0;
      csp->LogViolationDetails(
        nsIContentSecurityPolicy::VIOLATION_TYPE_REQUIRE_SRI_FOR_SCRIPT,
        NS_ConvertUTF8toUTF16(violationURISpec),
        EmptyString(), lineNo, EmptyString(), EmptyString());
      rv = NS_ERROR_SRI_CORRUPT;
    }
  }

  if (NS_SUCCEEDED(rv)) {
    rv = PrepareLoadedRequest(request, aLoader, aChannelStatus, aString);
  }

  if (NS_FAILED(rv)) {
    /*
     * Handle script not loading error because source was a tracking URL.
     * We make a note of this script node by including it in a dedicated
     * array of blocked tracking nodes under its parent document.
     */
    if (rv == NS_ERROR_TRACKING_URI) {
      nsCOMPtr<nsIContent> cont = do_QueryInterface(request->mElement);
      mDocument->AddBlockedTrackingNode(cont);
    }

    if (request->mIsDefer) {
      MOZ_ASSERT_IF(request->IsModuleRequest(),
                    request->AsModuleRequest()->IsTopLevel());
      if (request->isInList()) {
        RefPtr<nsScriptLoadRequest> req = mDeferRequests.Steal(request);
        FireScriptAvailable(rv, req);
      }
    } else if (request->mIsAsync) {
      MOZ_ASSERT_IF(request->IsModuleRequest(),
                    request->AsModuleRequest()->IsTopLevel());
      if (request->isInList()) {
        RefPtr<nsScriptLoadRequest> req = mLoadingAsyncRequests.Steal(request);
        FireScriptAvailable(rv, req);
      }
    } else if (request->mIsNonAsyncScriptInserted) {
      if (request->isInList()) {
        RefPtr<nsScriptLoadRequest> req =
          mNonAsyncExternalScriptInsertedRequests.Steal(request);
        FireScriptAvailable(rv, req);
      }
    } else if (request->mIsXSLT) {
      if (request->isInList()) {
        RefPtr<nsScriptLoadRequest> req = mXSLTRequests.Steal(request);
        FireScriptAvailable(rv, req);
      }
    } else if (request->IsModuleRequest()) {
      nsModuleLoadRequest* modReq = request->AsModuleRequest();
      MOZ_ASSERT(!modReq->IsTopLevel());
      MOZ_ASSERT(!modReq->isInList());
      modReq->Cancel();
      FireScriptAvailable(rv, request);
    } else if (mParserBlockingRequest == request) {
      MOZ_ASSERT(!request->isInList());
      mParserBlockingRequest = nullptr;
      UnblockParser(request);

      // Ensure that we treat request->mElement as our current parser-inserted
      // script while firing onerror on it.
      MOZ_ASSERT(request->mElement->GetParserCreated());
      nsCOMPtr<nsIScriptElement> oldParserInsertedScript =
        mCurrentParserInsertedScript;
      mCurrentParserInsertedScript = request->mElement;
      FireScriptAvailable(rv, request);
      ContinueParserAsync(request);
      mCurrentParserInsertedScript = oldParserInsertedScript;
    } else {
      mPreloads.RemoveElement(request, PreloadRequestComparator());
    }
  }

  // Process our request and/or any pending ones
  ProcessPendingRequests();

  return NS_OK;
}

void
nsScriptLoader::UnblockParser(nsScriptLoadRequest* aParserBlockingRequest)
{
  aParserBlockingRequest->mElement->UnblockParser();
}

void
nsScriptLoader::ContinueParserAsync(nsScriptLoadRequest* aParserBlockingRequest)
{
  aParserBlockingRequest->mElement->ContinueParserAsync();
}

uint32_t
nsScriptLoader::NumberOfProcessors()
{
  if (mNumberOfProcessors > 0)
    return mNumberOfProcessors;

  int32_t numProcs = PR_GetNumberOfProcessors();
  if (numProcs > 0)
    mNumberOfProcessors = numProcs;
  return mNumberOfProcessors;
}

void
nsScriptLoader::MaybeMoveToLoadedList(nsScriptLoadRequest* aRequest)
{
  MOZ_ASSERT(aRequest->IsReadyToRun());

  // If it's async, move it to the loaded list.  aRequest->mIsAsync really
  // _should_ be in a list, but the consequences if it's not are bad enough we
  // want to avoid trying to move it if it's not.
  if (aRequest->mIsAsync) {
    MOZ_ASSERT(aRequest->isInList());
    if (aRequest->isInList()) {
      RefPtr<nsScriptLoadRequest> req = mLoadingAsyncRequests.Steal(aRequest);
      mLoadedAsyncRequests.AppendElement(req);
    }
  }
}

nsresult
nsScriptLoader::PrepareLoadedRequest(nsScriptLoadRequest* aRequest,
                                     nsIIncrementalStreamLoader* aLoader,
                                     nsresult aStatus,
                                     mozilla::Vector<char16_t> &aString)
{
  if (NS_FAILED(aStatus)) {
    return aStatus;
  }

  if (aRequest->IsCanceled()) {
    return NS_BINDING_ABORTED;
  }

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
    rv = httpChannel->GetResponseHeader(NS_LITERAL_CSTRING("X-SourceMap"), sourceMapURL);
    if (NS_SUCCEEDED(rv)) {
      aRequest->mHasSourceMapURL = true;
      aRequest->mSourceMapURL = NS_ConvertUTF8toUTF16(sourceMapURL);
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

  if (!aString.empty()) {
    aRequest->mScriptTextLength = aString.length();
    aRequest->mScriptTextBuf = aString.extractOrCopyRawBuffer();
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
    nsModuleLoadRequest* request = aRequest->AsModuleRequest();

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
  if (aRequest == mParserBlockingRequest && (NumberOfProcessors() > 1)) {
    MOZ_ASSERT(!aRequest->IsModuleRequest());
    nsresult rv = AttemptAsyncScriptCompile(aRequest);
    if (rv == NS_OK) {
      MOZ_ASSERT(aRequest->mProgress == nsScriptLoadRequest::Progress::Compiling,
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
nsScriptLoader::ParsingComplete(bool aTerminated)
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
nsScriptLoader::PreloadURI(nsIURI *aURI, const nsAString &aCharset,
                           const nsAString &aType,
                           const nsAString &aCrossOrigin,
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
  if (nsContentUtils::IsChromeDoc(mDocument) && aType.LowerCaseEqualsASCII("module")) {
    return;
  }

  SRIMetadata sriMetadata;
  if (!aIntegrity.IsEmpty()) {
    MOZ_LOG(SRILogHelper::GetSriLog(), mozilla::LogLevel::Debug,
           ("nsScriptLoader::PreloadURI, integrity=%s",
            NS_ConvertUTF16toUTF8(aIntegrity).get()));
    nsAutoCString sourceUri;
    if (mDocument->GetDocumentURI()) {
      mDocument->GetDocumentURI()->GetAsciiSpec(sourceUri);
    }
    SRICheck::IntegrityMetadata(aIntegrity, sourceUri, mReporter, &sriMetadata);
  }

  RefPtr<nsScriptLoadRequest> request =
    CreateLoadRequest(nsScriptKind::Classic, nullptr, 0,
                      Element::StringToCORSMode(aCrossOrigin), sriMetadata);
  request->mURI = aURI;
  request->mIsInline = false;
  request->mReferrerPolicy = aReferrerPolicy;

  nsresult rv = StartLoad(request, aType, aScriptFromHead);
  if (NS_FAILED(rv)) {
    return;
  }

  PreloadInfo *pi = mPreloads.AppendElement();
  pi->mRequest = request;
  pi->mCharset = aCharset;
}

void
nsScriptLoader::AddDeferRequest(nsScriptLoadRequest* aRequest)
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
nsScriptLoader::MaybeRemovedDeferRequests()
{
  if (mDeferRequests.isEmpty() && mDocument &&
      mBlockingDOMContentLoaded) {
    mBlockingDOMContentLoaded = false;
    mDocument->UnblockDOMContentLoaded();
    return true;
  }
  return false;
}

//////////////////////////////////////////////////////////////
// nsScriptLoadHandler
//////////////////////////////////////////////////////////////

nsScriptLoadHandler::nsScriptLoadHandler(nsScriptLoader *aScriptLoader,
                                         nsScriptLoadRequest *aRequest,
                                         mozilla::dom::SRICheckDataVerifier *aSRIDataVerifier)
  : mScriptLoader(aScriptLoader),
    mRequest(aRequest),
    mSRIDataVerifier(aSRIDataVerifier),
    mSRIStatus(NS_OK),
    mDecoder(),
    mBuffer()
{}

nsScriptLoadHandler::~nsScriptLoadHandler()
{}

NS_IMPL_ISUPPORTS(nsScriptLoadHandler, nsIIncrementalStreamLoaderObserver)

NS_IMETHODIMP
nsScriptLoadHandler::OnIncrementalData(nsIIncrementalStreamLoader* aLoader,
                                       nsISupports* aContext,
                                       uint32_t aDataLength,
                                       const uint8_t* aData,
                                       uint32_t *aConsumedLength)
{
  if (mRequest->IsCanceled()) {
    // If request cancelled, ignore any incoming data.
    *aConsumedLength = aDataLength;
    return NS_OK;
  }

  if (!EnsureDecoder(aLoader, aData, aDataLength,
                     /* aEndOfStream = */ false)) {
    return NS_OK;
  }

  // Below we will/shall consume entire data chunk.
  *aConsumedLength = aDataLength;

  // Decoder has already been initialized. -- trying to decode all loaded bytes.
  nsresult rv = TryDecodeRawData(aData, aDataLength,
                                 /* aEndOfStream = */ false);
  NS_ENSURE_SUCCESS(rv, rv);

  // If SRI is required for this load, appending new bytes to the hash.
  if (mSRIDataVerifier && NS_SUCCEEDED(mSRIStatus)) {
    mSRIStatus = mSRIDataVerifier->Update(aDataLength, aData);
  }

  return rv;
}

nsresult
nsScriptLoadHandler::TryDecodeRawData(const uint8_t* aData,
                                      uint32_t aDataLength,
                                      bool aEndOfStream)
{
  int32_t srcLen = aDataLength;
  const char* src = reinterpret_cast<const char *>(aData);
  int32_t dstLen;
  nsresult rv =
    mDecoder->GetMaxLength(src, srcLen, &dstLen);

  NS_ENSURE_SUCCESS(rv, rv);

  uint32_t haveRead = mBuffer.length();

  CheckedInt<uint32_t> capacity = haveRead;
  capacity += dstLen;

  if (!capacity.isValid() || !mBuffer.reserve(capacity.value())) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  rv = mDecoder->Convert(src,
                      &srcLen,
                      mBuffer.begin() + haveRead,
                      &dstLen);

  NS_ENSURE_SUCCESS(rv, rv);

  haveRead += dstLen;
  MOZ_ASSERT(haveRead <= capacity.value(), "mDecoder produced more data than expected");
  MOZ_ALWAYS_TRUE(mBuffer.resizeUninitialized(haveRead));

  return NS_OK;
}

bool
nsScriptLoadHandler::EnsureDecoder(nsIIncrementalStreamLoader *aLoader,
                                   const uint8_t* aData,
                                   uint32_t aDataLength,
                                   bool aEndOfStream)
{
  // Check if decoder has already been created.
  if (mDecoder) {
    return true;
  }

  nsAutoCString charset;

  // JavaScript modules are always UTF-8.
  if (mRequest->IsModuleRequest()) {
    charset = "UTF-8";
    mDecoder = EncodingUtils::DecoderForEncoding(charset);
    return true;
  }

  // Determine if BOM check should be done.  This occurs either
  // if end-of-stream has been reached, or at least 3 bytes have
  // been read from input.
  if (!aEndOfStream && (aDataLength < 3)) {
    return false;
  }

  // Do BOM detection.
  if (nsContentUtils::CheckForBOM(aData, aDataLength, charset)) {
    mDecoder = EncodingUtils::DecoderForEncoding(charset);
    return true;
  }

  // BOM detection failed, check content stream for charset.
  nsCOMPtr<nsIRequest> req;
  nsresult rv = aLoader->GetRequest(getter_AddRefs(req));
  NS_ASSERTION(req, "StreamLoader's request went away prematurely");
  NS_ENSURE_SUCCESS(rv, false);

  nsCOMPtr<nsIChannel> channel = do_QueryInterface(req);

  if (channel &&
      NS_SUCCEEDED(channel->GetContentCharset(charset)) &&
      EncodingUtils::FindEncodingForLabel(charset, charset)) {
    mDecoder = EncodingUtils::DecoderForEncoding(charset);
    return true;
  }

  // Check the hint charset from the script element or preload
  // request.
  nsAutoString hintCharset;
  if (!mRequest->IsPreload()) {
    mRequest->mElement->GetScriptCharset(hintCharset);
  } else {
    nsTArray<nsScriptLoader::PreloadInfo>::index_type i =
      mScriptLoader->mPreloads.IndexOf(mRequest, 0,
            nsScriptLoader::PreloadRequestComparator());

    NS_ASSERTION(i != mScriptLoader->mPreloads.NoIndex,
                 "Incorrect preload bookkeeping");
    hintCharset = mScriptLoader->mPreloads[i].mCharset;
  }

  if (EncodingUtils::FindEncodingForLabel(hintCharset, charset)) {
    mDecoder = EncodingUtils::DecoderForEncoding(charset);
    return true;
  }

  // Get the charset from the charset of the document.
  if (mScriptLoader->mDocument) {
    charset = mScriptLoader->mDocument->GetDocumentCharacterSet();
    mDecoder = EncodingUtils::DecoderForEncoding(charset);
    return true;
  }

  // Curiously, there are various callers that don't pass aDocument. The
  // fallback in the old code was ISO-8859-1, which behaved like
  // windows-1252. Saying windows-1252 for clarity and for compliance
  // with the Encoding Standard.
  charset = "windows-1252";
  mDecoder = EncodingUtils::DecoderForEncoding(charset);
  return true;
}

NS_IMETHODIMP
nsScriptLoadHandler::OnStreamComplete(nsIIncrementalStreamLoader* aLoader,
                                      nsISupports* aContext,
                                      nsresult aStatus,
                                      uint32_t aDataLength,
                                      const uint8_t* aData)
{
  if (!mRequest->IsCanceled()) {
    DebugOnly<bool> encoderSet =
      EnsureDecoder(aLoader, aData, aDataLength, /* aEndOfStream = */ true);
    MOZ_ASSERT(encoderSet);
    DebugOnly<nsresult> rv = TryDecodeRawData(aData, aDataLength,
                                              /* aEndOfStream = */ true);

    // If SRI is required for this load, appending new bytes to the hash.
    if (mSRIDataVerifier && NS_SUCCEEDED(mSRIStatus)) {
      mSRIStatus = mSRIDataVerifier->Update(aDataLength, aData);
    }
  }

  // we have to mediate and use mRequest.
  return mScriptLoader->OnStreamComplete(aLoader, mRequest, aStatus, mSRIStatus,
                                         mBuffer, mSRIDataVerifier);
}
