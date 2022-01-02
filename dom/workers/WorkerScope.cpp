/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/WorkerScope.h"

#include <stdio.h>
#include <new>
#include <utility>
#include "Crypto.h"
#include "GeckoProfiler.h"
#include "MainThreadUtils.h"
#include "Principal.h"
#include "ScriptLoader.h"
#include "js/CompilationAndEvaluation.h"
#include "js/CompileOptions.h"
#include "js/RealmOptions.h"
#include "js/RootingAPI.h"
#include "js/SourceText.h"
#include "js/Value.h"
#include "js/Wrapper.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/BaseProfilerMarkersPrerequisites.h"
#include "mozilla/CycleCollectedJSContext.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/EventListenerManager.h"
#include "mozilla/Logging.h"
#include "mozilla/Maybe.h"
#include "mozilla/MozPromise.h"
#include "mozilla/Mutex.h"
#include "mozilla/NotNull.h"
#include "mozilla/RefPtr.h"
#include "mozilla/Result.h"
#include "mozilla/StaticAnalysisFunctions.h"
#include "mozilla/StorageAccess.h"
#include "mozilla/TaskCategory.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Unused.h"
#include "mozilla/dom/AutoEntryScript.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/BlobURLProtocolHandler.h"
#include "mozilla/dom/CSPEvalChecker.h"
#include "mozilla/dom/ClientSource.h"
#include "mozilla/dom/Clients.h"
#include "mozilla/dom/Console.h"
#include "mozilla/dom/DOMMozPromiseRequestHolder.h"
#include "mozilla/dom/DebuggerNotification.h"
#include "mozilla/dom/DebuggerNotificationBinding.h"
#include "mozilla/dom/DebuggerNotificationManager.h"
#include "mozilla/dom/DedicatedWorkerGlobalScopeBinding.h"
#include "mozilla/dom/DOMString.h"
#include "mozilla/dom/Fetch.h"
#include "mozilla/dom/IDBFactory.h"
#include "mozilla/dom/ImageBitmap.h"
#include "mozilla/dom/ImageBitmapSource.h"
#include "mozilla/dom/MessagePortBinding.h"
#include "mozilla/dom/Performance.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/PromiseWorkerProxy.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/SerializedStackHolder.h"
#include "mozilla/dom/ServiceWorkerDescriptor.h"
#include "mozilla/dom/ServiceWorkerGlobalScopeBinding.h"
#include "mozilla/dom/ServiceWorkerManager.h"
#include "mozilla/dom/ServiceWorkerRegistration.h"
#include "mozilla/dom/ServiceWorkerRegistrationDescriptor.h"
#include "mozilla/dom/ServiceWorkerUtils.h"
#include "mozilla/dom/SharedWorkerGlobalScopeBinding.h"
#include "mozilla/dom/SimpleGlobalObject.h"
#include "mozilla/dom/TimeoutHandler.h"
#include "mozilla/dom/WorkerCommon.h"
#include "mozilla/dom/WorkerDebuggerGlobalScopeBinding.h"
#include "mozilla/dom/WorkerGlobalScopeBinding.h"
#include "mozilla/dom/WorkerLocation.h"
#include "mozilla/dom/WorkerNavigator.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/dom/WorkerRunnable.h"
#include "mozilla/dom/cache/CacheStorage.h"
#include "mozilla/dom/cache/Types.h"
#include "mozilla/extensions/ExtensionBrowser.h"
#include "mozilla/fallible.h"
#include "mozilla/gfx/Rect.h"
#include "nsAtom.h"
#include "nsCOMPtr.h"
#include "nsContentUtils.h"
#include "nsDebug.h"
#include "nsGkAtoms.h"
#include "nsIEventTarget.h"
#include "nsIGlobalObject.h"
#include "nsIScriptError.h"
#include "nsISerialEventTarget.h"
#include "nsIWeakReference.h"
#include "nsJSUtils.h"
#include "nsLiteralString.h"
#include "nsQueryObject.h"
#include "nsReadableUtils.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsTLiteralString.h"
#include "nsThreadUtils.h"
#include "nsWeakReference.h"
#include "nsWrapperCacheInlines.h"
#include "nscore.h"
#include "xpcpublic.h"

#ifdef ANDROID
#  include <android/log.h>
#endif

#ifdef XP_WIN
#  undef PostMessage
#endif

namespace mozilla {
namespace dom {

using mozilla::dom::cache::CacheStorage;
using mozilla::dom::workerinternals::NamedWorkerGlobalScopeMixin;
using mozilla::ipc::PrincipalInfo;

class WorkerScriptTimeoutHandler final : public ScriptTimeoutHandler {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(WorkerScriptTimeoutHandler,
                                           ScriptTimeoutHandler)

  WorkerScriptTimeoutHandler(JSContext* aCx, nsIGlobalObject* aGlobal,
                             const nsAString& aExpression)
      : ScriptTimeoutHandler(aCx, aGlobal, aExpression) {}

  MOZ_CAN_RUN_SCRIPT virtual bool Call(const char* aExecutionReason) override;

 private:
  virtual ~WorkerScriptTimeoutHandler() = default;
};

NS_IMPL_CYCLE_COLLECTION_INHERITED(WorkerScriptTimeoutHandler,
                                   ScriptTimeoutHandler)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(WorkerScriptTimeoutHandler)
NS_INTERFACE_MAP_END_INHERITING(ScriptTimeoutHandler)

NS_IMPL_ADDREF_INHERITED(WorkerScriptTimeoutHandler, ScriptTimeoutHandler)
NS_IMPL_RELEASE_INHERITED(WorkerScriptTimeoutHandler, ScriptTimeoutHandler)

bool WorkerScriptTimeoutHandler::Call(const char* aExecutionReason) {
  nsAutoMicroTask mt;
  AutoEntryScript aes(mGlobal, aExecutionReason, false);

  JSContext* cx = aes.cx();
  JS::CompileOptions options(cx);
  options.setFileAndLine(mFileName.get(), mLineNo).setNoScriptRval(true);
  options.setIntroductionType("domTimer");

  JS::Rooted<JS::Value> unused(cx);
  JS::SourceText<char16_t> srcBuf;
  if (!srcBuf.init(cx, mExpr.BeginReading(), mExpr.Length(),
                   JS::SourceOwnership::Borrowed) ||
      !JS::Evaluate(cx, options, srcBuf, &unused)) {
    if (!JS_IsExceptionPending(cx)) {
      return false;
    }
  }

  return true;
};

namespace workerinternals {
void NamedWorkerGlobalScopeMixin::GetName(DOMString& aName) const {
  aName.AsAString() = mName;
}
}  // namespace workerinternals

NS_IMPL_CYCLE_COLLECTION_CLASS(WorkerGlobalScopeBase)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(WorkerGlobalScopeBase,
                                                  DOMEventTargetHelper)
  tmp->mWorkerPrivate->AssertIsOnWorkerThread();
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mConsole)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mSerialEventTarget)
  tmp->TraverseObjectsInGlobal(cb);
  tmp->mWorkerPrivate->TraverseTimeouts(cb);
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(WorkerGlobalScopeBase,
                                                DOMEventTargetHelper)
  tmp->mWorkerPrivate->AssertIsOnWorkerThread();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mConsole)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mSerialEventTarget)
  tmp->UnlinkObjectsInGlobal();
  tmp->mWorkerPrivate->UnlinkTimeouts();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(WorkerGlobalScopeBase,
                                               DOMEventTargetHelper)
  tmp->mWorkerPrivate->AssertIsOnWorkerThread();
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_ADDREF_INHERITED(WorkerGlobalScopeBase, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(WorkerGlobalScopeBase, DOMEventTargetHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(WorkerGlobalScopeBase)
  NS_INTERFACE_MAP_ENTRY(nsIGlobalObject)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

WorkerGlobalScopeBase::WorkerGlobalScopeBase(
    NotNull<WorkerPrivate*> aWorkerPrivate,
    UniquePtr<ClientSource> aClientSource)
    : mWorkerPrivate(aWorkerPrivate),
      mClientSource(std::move(aClientSource)),
      mSerialEventTarget(aWorkerPrivate->HybridEventTarget()) {
  mWorkerPrivate->AssertIsOnWorkerThread();
  MOZ_ASSERT(mClientSource);

  MOZ_DIAGNOSTIC_ASSERT(
      mSerialEventTarget,
      "There should be an event target when a worker global is created.");

  // In workers, each DETH must have an owner. Because the global scope doesn't
  // have one, let's set it as owner of itself.
  BindToOwner(static_cast<nsIGlobalObject*>(this));
}

WorkerGlobalScopeBase::~WorkerGlobalScopeBase() = default;

JSObject* WorkerGlobalScopeBase::GetGlobalJSObject() {
  mWorkerPrivate->AssertIsOnWorkerThread();
  return GetWrapper();
}

JSObject* WorkerGlobalScopeBase::GetGlobalJSObjectPreserveColor() const {
  mWorkerPrivate->AssertIsOnWorkerThread();
  return GetWrapperPreserveColor();
}

bool WorkerGlobalScopeBase::IsSharedMemoryAllowed() const {
  mWorkerPrivate->AssertIsOnWorkerThread();
  return mWorkerPrivate->IsSharedMemoryAllowed();
}

bool WorkerGlobalScopeBase::ShouldResistFingerprinting() const {
  mWorkerPrivate->AssertIsOnWorkerThread();
  return mWorkerPrivate->ShouldResistFingerprinting();
}

uint32_t WorkerGlobalScopeBase::GetPrincipalHashValue() const {
  mWorkerPrivate->AssertIsOnWorkerThread();
  return mWorkerPrivate->GetPrincipalHashValue();
}

StorageAccess WorkerGlobalScopeBase::GetStorageAccess() {
  mWorkerPrivate->AssertIsOnWorkerThread();
  return mWorkerPrivate->StorageAccess();
}

Maybe<ClientInfo> WorkerGlobalScopeBase::GetClientInfo() const {
  return Some(mClientSource->Info());
}

Maybe<ServiceWorkerDescriptor> WorkerGlobalScopeBase::GetController() const {
  return mClientSource->GetController();
}

void WorkerGlobalScopeBase::Control(
    const ServiceWorkerDescriptor& aServiceWorker) {
  mWorkerPrivate->AssertIsOnWorkerThread();
  MOZ_DIAGNOSTIC_ASSERT(!mWorkerPrivate->IsChromeWorker());
  MOZ_DIAGNOSTIC_ASSERT(mWorkerPrivate->Kind() != WorkerKindService);

  if (IsBlobURI(mWorkerPrivate->GetBaseURI())) {
    // Blob URL workers can only become controlled by inheriting from
    // their parent.  Make sure to note this properly.
    mClientSource->InheritController(aServiceWorker);
  } else {
    // Otherwise this is a normal interception and we simply record the
    // controller locally.
    mClientSource->SetController(aServiceWorker);
  }
}

nsresult WorkerGlobalScopeBase::Dispatch(
    TaskCategory aCategory, already_AddRefed<nsIRunnable>&& aRunnable) {
  return EventTargetFor(aCategory)->Dispatch(std::move(aRunnable),
                                             NS_DISPATCH_NORMAL);
}

nsISerialEventTarget* WorkerGlobalScopeBase::EventTargetFor(
    TaskCategory) const {
  mWorkerPrivate->AssertIsOnWorkerThread();
  return mSerialEventTarget;
}

// See also AutoJSAPI::ReportException
void WorkerGlobalScopeBase::ReportError(JSContext* aCx,
                                        JS::Handle<JS::Value> aError,
                                        CallerType, ErrorResult& aRv) {
  JS::ErrorReportBuilder jsReport(aCx);
  JS::ExceptionStack exnStack(aCx, aError, nullptr);
  if (!jsReport.init(aCx, exnStack, JS::ErrorReportBuilder::WithSideEffects)) {
    return aRv.NoteJSContextException(aCx);
  }

  // Before invoking ReportError, put the exception back on the context,
  // because it may want to put it in its error events and has no other way
  // to get hold of it.  After we invoke ReportError, clear the exception on
  // cx(), just in case ReportError didn't.
  JS::SetPendingExceptionStack(aCx, exnStack);
  mWorkerPrivate->ReportError(aCx, jsReport.toStringResult(),
                              jsReport.report());
  JS_ClearPendingException(aCx);
}

void WorkerGlobalScopeBase::Atob(const nsAString& aAtob, nsAString& aOut,
                                 ErrorResult& aRv) const {
  mWorkerPrivate->AssertIsOnWorkerThread();
  aRv = nsContentUtils::Atob(aAtob, aOut);
}

void WorkerGlobalScopeBase::Btoa(const nsAString& aBtoa, nsAString& aOut,
                                 ErrorResult& aRv) const {
  mWorkerPrivate->AssertIsOnWorkerThread();
  aRv = nsContentUtils::Btoa(aBtoa, aOut);
}

already_AddRefed<Console> WorkerGlobalScopeBase::GetConsole(ErrorResult& aRv) {
  mWorkerPrivate->AssertIsOnWorkerThread();

  if (!mConsole) {
    mConsole = Console::Create(mWorkerPrivate->GetJSContext(), nullptr, aRv);
    if (NS_WARN_IF(aRv.Failed())) {
      return nullptr;
    }
  }

  RefPtr<Console> console = mConsole;
  return console.forget();
}

uint64_t WorkerGlobalScopeBase::WindowID() const {
  return mWorkerPrivate->WindowID();
}

NS_IMPL_CYCLE_COLLECTION_CLASS(WorkerGlobalScope)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(WorkerGlobalScope,
                                                  WorkerGlobalScopeBase)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mCrypto)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPerformance)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mLocation)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mNavigator)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mIndexedDB)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mCacheStorage)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDebuggerNotificationManager)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(WorkerGlobalScope,
                                                WorkerGlobalScopeBase)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mCrypto)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mPerformance)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mLocation)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mNavigator)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mIndexedDB)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mCacheStorage)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mDebuggerNotificationManager)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_WEAK_REFERENCE
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_ISUPPORTS_CYCLE_COLLECTION_INHERITED(WorkerGlobalScope,
                                             WorkerGlobalScopeBase,
                                             nsISupportsWeakReference)

WorkerGlobalScope::~WorkerGlobalScope() = default;

Crypto* WorkerGlobalScope::GetCrypto(ErrorResult& aError) {
  mWorkerPrivate->AssertIsOnWorkerThread();

  if (!mCrypto) {
    mCrypto = new Crypto(this);
  }

  return mCrypto;
}

already_AddRefed<CacheStorage> WorkerGlobalScope::GetCaches(ErrorResult& aRv) {
  if (!mCacheStorage) {
    mCacheStorage = CacheStorage::CreateOnWorker(cache::DEFAULT_NAMESPACE, this,
                                                 mWorkerPrivate, aRv);
  }

  RefPtr<CacheStorage> ref = mCacheStorage;
  return ref.forget();
}

bool WorkerGlobalScope::IsSecureContext() const {
  bool globalSecure = JS::GetIsSecureContext(
      js::GetNonCCWObjectRealm(GetWrapperPreserveColor()));
  MOZ_ASSERT(globalSecure == mWorkerPrivate->IsSecureContext());
  return globalSecure;
}

already_AddRefed<WorkerLocation> WorkerGlobalScope::Location() {
  mWorkerPrivate->AssertIsOnWorkerThread();

  if (!mLocation) {
    mLocation = WorkerLocation::Create(mWorkerPrivate->GetLocationInfo());
    MOZ_ASSERT(mLocation);
  }

  RefPtr<WorkerLocation> location = mLocation;
  return location.forget();
}

already_AddRefed<WorkerNavigator> WorkerGlobalScope::Navigator() {
  mWorkerPrivate->AssertIsOnWorkerThread();

  if (!mNavigator) {
    mNavigator = WorkerNavigator::Create(mWorkerPrivate->OnLine());
    MOZ_ASSERT(mNavigator);
  }

  RefPtr<WorkerNavigator> navigator = mNavigator;
  return navigator.forget();
}

already_AddRefed<WorkerNavigator> WorkerGlobalScope::GetExistingNavigator()
    const {
  mWorkerPrivate->AssertIsOnWorkerThread();

  RefPtr<WorkerNavigator> navigator = mNavigator;
  return navigator.forget();
}

OnErrorEventHandlerNonNull* WorkerGlobalScope::GetOnerror() {
  mWorkerPrivate->AssertIsOnWorkerThread();

  EventListenerManager* elm = GetExistingListenerManager();
  return elm ? elm->GetOnErrorEventHandler() : nullptr;
}

void WorkerGlobalScope::SetOnerror(OnErrorEventHandlerNonNull* aHandler) {
  mWorkerPrivate->AssertIsOnWorkerThread();

  EventListenerManager* elm = GetOrCreateListenerManager();
  if (elm) {
    elm->SetEventHandler(aHandler);
  }
}

void WorkerGlobalScope::ImportScripts(JSContext* aCx,
                                      const Sequence<nsString>& aScriptURLs,
                                      ErrorResult& aRv) {
  mWorkerPrivate->AssertIsOnWorkerThread();

  UniquePtr<SerializedStackHolder> stack;
  if (mWorkerPrivate->IsWatchedByDevTools()) {
    stack = GetCurrentStackForNetMonitor(aCx);
  }

  {
    AUTO_PROFILER_MARKER_TEXT(
        "ImportScripts", JS, MarkerStack::Capture(),
        profiler_thread_is_being_profiled_for_markers()
            ? StringJoin(","_ns, aScriptURLs,
                         [](nsACString& dest, const auto& scriptUrl) {
                           AppendUTF16toUTF8(scriptUrl, dest);
                         })
            : nsAutoCString{});
    workerinternals::Load(mWorkerPrivate, std::move(stack), aScriptURLs,
                          WorkerScript, aRv);
  }
}

int32_t WorkerGlobalScope::SetTimeout(JSContext* aCx, Function& aHandler,
                                      const int32_t aTimeout,
                                      const Sequence<JS::Value>& aArguments,
                                      ErrorResult& aRv) {
  return SetTimeoutOrInterval(aCx, aHandler, aTimeout, aArguments, false, aRv);
}

int32_t WorkerGlobalScope::SetTimeout(JSContext* aCx, const nsAString& aHandler,
                                      const int32_t aTimeout,
                                      const Sequence<JS::Value>& /* unused */,
                                      ErrorResult& aRv) {
  return SetTimeoutOrInterval(aCx, aHandler, aTimeout, false, aRv);
}

void WorkerGlobalScope::ClearTimeout(int32_t aHandle) {
  mWorkerPrivate->AssertIsOnWorkerThread();

  DebuggerNotificationDispatch(this, DebuggerNotificationType::ClearTimeout);

  mWorkerPrivate->ClearTimeout(aHandle);
}

int32_t WorkerGlobalScope::SetInterval(JSContext* aCx, Function& aHandler,
                                       const int32_t aTimeout,
                                       const Sequence<JS::Value>& aArguments,
                                       ErrorResult& aRv) {
  return SetTimeoutOrInterval(aCx, aHandler, aTimeout, aArguments, true, aRv);
}

int32_t WorkerGlobalScope::SetInterval(JSContext* aCx,
                                       const nsAString& aHandler,
                                       const int32_t aTimeout,
                                       const Sequence<JS::Value>& /* unused */,
                                       ErrorResult& aRv) {
  return SetTimeoutOrInterval(aCx, aHandler, aTimeout, true, aRv);
}

void WorkerGlobalScope::ClearInterval(int32_t aHandle) {
  mWorkerPrivate->AssertIsOnWorkerThread();

  DebuggerNotificationDispatch(this, DebuggerNotificationType::ClearInterval);

  mWorkerPrivate->ClearTimeout(aHandle);
}

int32_t WorkerGlobalScope::SetTimeoutOrInterval(
    JSContext* aCx, Function& aHandler, const int32_t aTimeout,
    const Sequence<JS::Value>& aArguments, bool aIsInterval, ErrorResult& aRv) {
  mWorkerPrivate->AssertIsOnWorkerThread();

  DebuggerNotificationDispatch(
      this, aIsInterval ? DebuggerNotificationType::SetInterval
                        : DebuggerNotificationType::SetTimeout);

  nsTArray<JS::Heap<JS::Value>> args;
  if (!args.AppendElements(aArguments, fallible)) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return 0;
  }

  RefPtr<TimeoutHandler> handler =
      new CallbackTimeoutHandler(aCx, this, &aHandler, std::move(args));

  return mWorkerPrivate->SetTimeout(aCx, handler, aTimeout, aIsInterval, aRv);
}

int32_t WorkerGlobalScope::SetTimeoutOrInterval(JSContext* aCx,
                                                const nsAString& aHandler,
                                                const int32_t aTimeout,
                                                bool aIsInterval,
                                                ErrorResult& aRv) {
  mWorkerPrivate->AssertIsOnWorkerThread();

  DebuggerNotificationDispatch(
      this, aIsInterval ? DebuggerNotificationType::SetInterval
                        : DebuggerNotificationType::SetTimeout);

  bool allowEval = false;
  aRv =
      CSPEvalChecker::CheckForWorker(aCx, mWorkerPrivate, aHandler, &allowEval);
  if (NS_WARN_IF(aRv.Failed()) || !allowEval) {
    return 0;
  }

  RefPtr<TimeoutHandler> handler =
      new WorkerScriptTimeoutHandler(aCx, this, aHandler);

  return mWorkerPrivate->SetTimeout(aCx, handler, aTimeout, aIsInterval, aRv);
}

void WorkerGlobalScope::GetOrigin(nsAString& aOrigin) const {
  mWorkerPrivate->AssertIsOnWorkerThread();
  aOrigin = mWorkerPrivate->OriginNoSuffix();
}

bool WorkerGlobalScope::CrossOriginIsolated() const {
  return mWorkerPrivate->CrossOriginIsolated();
}

void WorkerGlobalScope::Dump(const Optional<nsAString>& aString) const {
  mWorkerPrivate->AssertIsOnWorkerThread();

  if (!aString.WasPassed()) {
    return;
  }

  if (!nsJSUtils::DumpEnabled()) {
    return;
  }

  NS_ConvertUTF16toUTF8 str(aString.Value());

  MOZ_LOG(nsContentUtils::DOMDumpLog(), LogLevel::Debug,
          ("[Worker.Dump] %s", str.get()));
#ifdef ANDROID
  __android_log_print(ANDROID_LOG_INFO, "Gecko", "%s", str.get());
#endif
  fputs(str.get(), stdout);
  fflush(stdout);
}

Performance* WorkerGlobalScope::GetPerformance() {
  mWorkerPrivate->AssertIsOnWorkerThread();

  if (!mPerformance) {
    mPerformance = Performance::CreateForWorker(mWorkerPrivate);
  }

  return mPerformance;
}

bool WorkerGlobalScope::IsInAutomation(JSContext* aCx, JSObject* /* unused */) {
  return GetWorkerPrivateFromContext(aCx)->IsInAutomation();
}

void WorkerGlobalScope::GetJSTestingFunctions(
    JSContext* aCx, JS::MutableHandle<JSObject*> aFunctions, ErrorResult& aRv) {
  JSObject* obj = js::GetTestingFunctions(aCx);
  if (!obj) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return;
  }

  aFunctions.set(obj);
}

already_AddRefed<Promise> WorkerGlobalScope::Fetch(
    const RequestOrUSVString& aInput, const RequestInit& aInit,
    CallerType aCallerType, ErrorResult& aRv) {
  return FetchRequest(this, aInput, aInit, aCallerType, aRv);
}

already_AddRefed<IDBFactory> WorkerGlobalScope::GetIndexedDB(
    ErrorResult& aErrorResult) {
  mWorkerPrivate->AssertIsOnWorkerThread();

  RefPtr<IDBFactory> indexedDB = mIndexedDB;

  if (!indexedDB) {
    StorageAccess access = mWorkerPrivate->StorageAccess();

    if (access == StorageAccess::eDeny) {
      NS_WARNING("IndexedDB is not allowed in this worker!");
      aErrorResult = NS_ERROR_DOM_SECURITY_ERR;
      return nullptr;
    }

    if (ShouldPartitionStorage(access) &&
        !StoragePartitioningEnabled(access,
                                    mWorkerPrivate->CookieJarSettings())) {
      NS_WARNING("IndexedDB is not allowed in this worker!");
      aErrorResult = NS_ERROR_DOM_SECURITY_ERR;
      return nullptr;
    }

    const PrincipalInfo& principalInfo =
        mWorkerPrivate->GetEffectiveStoragePrincipalInfo();

    auto res = IDBFactory::CreateForWorker(this, principalInfo,
                                           mWorkerPrivate->WindowID());
    if (NS_WARN_IF(res.isErr())) {
      aErrorResult = res.unwrapErr();
      return nullptr;
    }

    indexedDB = res.unwrap();
    mIndexedDB = indexedDB;
  }

  return indexedDB.forget();
}

already_AddRefed<Promise> WorkerGlobalScope::CreateImageBitmap(
    const ImageBitmapSource& aImage, const ImageBitmapOptions& aOptions,
    ErrorResult& aRv) {
  return ImageBitmap::Create(this, aImage, Nothing(), aOptions, aRv);
}

already_AddRefed<Promise> WorkerGlobalScope::CreateImageBitmap(
    const ImageBitmapSource& aImage, int32_t aSx, int32_t aSy, int32_t aSw,
    int32_t aSh, const ImageBitmapOptions& aOptions, ErrorResult& aRv) {
  return ImageBitmap::Create(
      this, aImage, Some(gfx::IntRect(aSx, aSy, aSw, aSh)), aOptions, aRv);
}

// https://html.spec.whatwg.org/#structured-cloning
void WorkerGlobalScope::StructuredClone(
    JSContext* aCx, JS::Handle<JS::Value> aValue,
    const StructuredSerializeOptions& aOptions,
    JS::MutableHandle<JS::Value> aRetval, ErrorResult& aError) {
  nsContentUtils::StructuredClone(aCx, this, aValue, aOptions, aRetval, aError);
}

mozilla::dom::DebuggerNotificationManager*
WorkerGlobalScope::GetOrCreateDebuggerNotificationManager() {
  if (!mDebuggerNotificationManager) {
    mDebuggerNotificationManager = new DebuggerNotificationManager(this);
  }

  return mDebuggerNotificationManager;
}

mozilla::dom::DebuggerNotificationManager*
WorkerGlobalScope::GetExistingDebuggerNotificationManager() {
  return mDebuggerNotificationManager;
}

Maybe<EventCallbackDebuggerNotificationType>
WorkerGlobalScope::GetDebuggerNotificationType() const {
  return Some(EventCallbackDebuggerNotificationType::Global);
}

RefPtr<ServiceWorkerRegistration>
WorkerGlobalScope::GetServiceWorkerRegistration(
    const ServiceWorkerRegistrationDescriptor& aDescriptor) const {
  mWorkerPrivate->AssertIsOnWorkerThread();
  RefPtr<ServiceWorkerRegistration> ref;
  ForEachEventTargetObject([&](DOMEventTargetHelper* aTarget, bool* aDoneOut) {
    RefPtr<ServiceWorkerRegistration> swr = do_QueryObject(aTarget);
    if (!swr || !swr->MatchesDescriptor(aDescriptor)) {
      return;
    }

    ref = std::move(swr);
    *aDoneOut = true;
  });
  return ref;
}

RefPtr<ServiceWorkerRegistration>
WorkerGlobalScope::GetOrCreateServiceWorkerRegistration(
    const ServiceWorkerRegistrationDescriptor& aDescriptor) {
  mWorkerPrivate->AssertIsOnWorkerThread();
  RefPtr<ServiceWorkerRegistration> ref =
      GetServiceWorkerRegistration(aDescriptor);
  if (!ref) {
    ref = ServiceWorkerRegistration::CreateForWorker(mWorkerPrivate, this,
                                                     aDescriptor);
  }
  return ref;
}

void WorkerGlobalScope::StorageAccessPermissionGranted() {
  // Reset the IndexedDB factory.
  mIndexedDB = nullptr;

  // Reset DOM Cache
  mCacheStorage = nullptr;
}

bool WorkerGlobalScope::WindowInteractionAllowed() const {
  mWorkerPrivate->AssertIsOnWorkerThread();
  return mWindowInteractionsAllowed > 0;
}

void WorkerGlobalScope::AllowWindowInteraction() {
  mWorkerPrivate->AssertIsOnWorkerThread();
  mWindowInteractionsAllowed++;
}

void WorkerGlobalScope::ConsumeWindowInteraction() {
  mWorkerPrivate->AssertIsOnWorkerThread();
  MOZ_ASSERT(mWindowInteractionsAllowed);
  mWindowInteractionsAllowed--;
}

DedicatedWorkerGlobalScope::DedicatedWorkerGlobalScope(
    NotNull<WorkerPrivate*> aWorkerPrivate,
    UniquePtr<ClientSource> aClientSource, const nsString& aName)
    : WorkerGlobalScope(aWorkerPrivate, std::move(aClientSource)),
      NamedWorkerGlobalScopeMixin(aName) {}

bool DedicatedWorkerGlobalScope::WrapGlobalObject(
    JSContext* aCx, JS::MutableHandle<JSObject*> aReflector) {
  mWorkerPrivate->AssertIsOnWorkerThread();
  MOZ_ASSERT(!mWorkerPrivate->IsSharedWorker());

  JS::RealmOptions options;
  mWorkerPrivate->CopyJSRealmOptions(options);

  const bool usesSystemPrincipal = mWorkerPrivate->UsesSystemPrincipal();

  // Note that xpc::ShouldDiscardSystemSource() reads a prefs that is cached
  // on the main thread. This is benignly racey.
  const bool discardSource =
      usesSystemPrincipal && xpc::ShouldDiscardSystemSource();

  JS::RealmBehaviors& behaviors = options.behaviors();
  behaviors.setDiscardSource(discardSource);

  xpc::SetPrefableRealmOptions(options);

  return DedicatedWorkerGlobalScope_Binding::Wrap(
      aCx, this, this, options,
      new WorkerPrincipal(usesSystemPrincipal ||
                          mWorkerPrivate->UsesAddonOrExpandedAddonPrincipal()),
      true, aReflector);
}

void DedicatedWorkerGlobalScope::PostMessage(
    JSContext* aCx, JS::Handle<JS::Value> aMessage,
    const Sequence<JSObject*>& aTransferable, ErrorResult& aRv) {
  mWorkerPrivate->AssertIsOnWorkerThread();
  mWorkerPrivate->PostMessageToParent(aCx, aMessage, aTransferable, aRv);
}

void DedicatedWorkerGlobalScope::PostMessage(
    JSContext* aCx, JS::Handle<JS::Value> aMessage,
    const StructuredSerializeOptions& aOptions, ErrorResult& aRv) {
  mWorkerPrivate->AssertIsOnWorkerThread();
  mWorkerPrivate->PostMessageToParent(aCx, aMessage, aOptions.mTransfer, aRv);
}

void DedicatedWorkerGlobalScope::Close() {
  mWorkerPrivate->AssertIsOnWorkerThread();
  mWorkerPrivate->CloseInternal();
}

SharedWorkerGlobalScope::SharedWorkerGlobalScope(
    NotNull<WorkerPrivate*> aWorkerPrivate,
    UniquePtr<ClientSource> aClientSource, const nsString& aName)
    : WorkerGlobalScope(aWorkerPrivate, std::move(aClientSource)),
      NamedWorkerGlobalScopeMixin(aName) {}

bool SharedWorkerGlobalScope::WrapGlobalObject(
    JSContext* aCx, JS::MutableHandle<JSObject*> aReflector) {
  mWorkerPrivate->AssertIsOnWorkerThread();
  MOZ_ASSERT(mWorkerPrivate->IsSharedWorker());

  JS::RealmOptions options;
  mWorkerPrivate->CopyJSRealmOptions(options);

  return SharedWorkerGlobalScope_Binding::Wrap(
      aCx, this, this, options,
      new WorkerPrincipal(mWorkerPrivate->UsesSystemPrincipal() ||
                          mWorkerPrivate->UsesAddonOrExpandedAddonPrincipal()),
      true, aReflector);
}

void SharedWorkerGlobalScope::Close() {
  mWorkerPrivate->AssertIsOnWorkerThread();
  mWorkerPrivate->CloseInternal();
}

NS_IMPL_CYCLE_COLLECTION_INHERITED(ServiceWorkerGlobalScope, WorkerGlobalScope,
                                   mClients, mExtensionBrowser, mRegistration)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ServiceWorkerGlobalScope)
NS_INTERFACE_MAP_END_INHERITING(WorkerGlobalScope)

NS_IMPL_ADDREF_INHERITED(ServiceWorkerGlobalScope, WorkerGlobalScope)
NS_IMPL_RELEASE_INHERITED(ServiceWorkerGlobalScope, WorkerGlobalScope)

ServiceWorkerGlobalScope::ServiceWorkerGlobalScope(
    NotNull<WorkerPrivate*> aWorkerPrivate,
    UniquePtr<ClientSource> aClientSource,
    const ServiceWorkerRegistrationDescriptor& aRegistrationDescriptor)
    : WorkerGlobalScope(aWorkerPrivate, std::move(aClientSource)),
      mScope(NS_ConvertUTF8toUTF16(aRegistrationDescriptor.Scope()))

      // Eagerly create the registration because we will need to receive
      // updates about the state of the registration.  We can't wait until
      // first access to start receiving these.
      ,
      mRegistration(
          GetOrCreateServiceWorkerRegistration(aRegistrationDescriptor)) {}

ServiceWorkerGlobalScope::~ServiceWorkerGlobalScope() = default;

bool ServiceWorkerGlobalScope::WrapGlobalObject(
    JSContext* aCx, JS::MutableHandle<JSObject*> aReflector) {
  mWorkerPrivate->AssertIsOnWorkerThread();
  MOZ_ASSERT(mWorkerPrivate->IsServiceWorker());

  JS::RealmOptions options;
  mWorkerPrivate->CopyJSRealmOptions(options);

  return ServiceWorkerGlobalScope_Binding::Wrap(
      aCx, this, this, options,
      new WorkerPrincipal(mWorkerPrivate->UsesSystemPrincipal() ||
                          mWorkerPrivate->UsesAddonOrExpandedAddonPrincipal()),
      true, aReflector);
}

already_AddRefed<Clients> ServiceWorkerGlobalScope::GetClients() {
  if (!mClients) {
    mClients = new Clients(this);
  }

  RefPtr<Clients> ref = mClients;
  return ref.forget();
}

ServiceWorkerRegistration* ServiceWorkerGlobalScope::Registration() {
  return mRegistration;
}

EventHandlerNonNull* ServiceWorkerGlobalScope::GetOnfetch() {
  mWorkerPrivate->AssertIsOnWorkerThread();

  return GetEventHandler(nsGkAtoms::onfetch);
}

namespace {

class ReportFetchListenerWarningRunnable final : public Runnable {
  const nsCString mScope;
  nsString mSourceSpec;
  uint32_t mLine;
  uint32_t mColumn;

 public:
  explicit ReportFetchListenerWarningRunnable(const nsString& aScope)
      : mozilla::Runnable("ReportFetchListenerWarningRunnable"),
        mScope(NS_ConvertUTF16toUTF8(aScope)) {
    WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
    MOZ_ASSERT(workerPrivate);
    JSContext* cx = workerPrivate->GetJSContext();
    MOZ_ASSERT(cx);

    nsJSUtils::GetCallingLocation(cx, mSourceSpec, &mLine, &mColumn);
  }

  NS_IMETHOD
  Run() override {
    AssertIsOnMainThread();

    ServiceWorkerManager::LocalizeAndReportToAllClients(
        mScope, "ServiceWorkerNoFetchHandler", nsTArray<nsString>{},
        nsIScriptError::warningFlag, mSourceSpec, u""_ns, mLine, mColumn);

    return NS_OK;
  }
};

}  // anonymous namespace

void ServiceWorkerGlobalScope::NoteFetchHandlerWasAdded() const {
  if (mWorkerPrivate->WorkerScriptExecutedSuccessfully()) {
    RefPtr<Runnable> r = new ReportFetchListenerWarningRunnable(mScope);
    mWorkerPrivate->DispatchToMainThreadForMessaging(r.forget());
  }
  mWorkerPrivate->SetFetchHandlerWasAdded();
}

void ServiceWorkerGlobalScope::SetOnfetch(
    mozilla::dom::EventHandlerNonNull* aCallback) {
  mWorkerPrivate->AssertIsOnWorkerThread();

  if (aCallback) {
    NoteFetchHandlerWasAdded();
  }
  SetEventHandler(nsGkAtoms::onfetch, aCallback);
}

void ServiceWorkerGlobalScope::EventListenerAdded(nsAtom* aType) {
  mWorkerPrivate->AssertIsOnWorkerThread();

  if (aType == nsGkAtoms::onfetch) {
    NoteFetchHandlerWasAdded();
  }
}

already_AddRefed<Promise> ServiceWorkerGlobalScope::SkipWaiting(
    ErrorResult& aRv) {
  mWorkerPrivate->AssertIsOnWorkerThread();
  MOZ_ASSERT(mWorkerPrivate->IsServiceWorker());

  RefPtr<Promise> promise = Promise::Create(this, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  using MozPromiseType =
      decltype(mWorkerPrivate->SetServiceWorkerSkipWaitingFlag())::element_type;
  auto holder = MakeRefPtr<DOMMozPromiseRequestHolder<MozPromiseType>>(this);

  mWorkerPrivate->SetServiceWorkerSkipWaitingFlag()
      ->Then(GetCurrentSerialEventTarget(), __func__,
             [holder, promise](const MozPromiseType::ResolveOrRejectValue&) {
               holder->Complete();
               promise->MaybeResolveWithUndefined();
             })
      ->Track(*holder);

  return promise.forget();
}

SafeRefPtr<extensions::ExtensionBrowser>
ServiceWorkerGlobalScope::AcquireExtensionBrowser() {
  if (!mExtensionBrowser) {
    mExtensionBrowser = MakeSafeRefPtr<extensions::ExtensionBrowser>(this);
  }

  return mExtensionBrowser.clonePtr();
}

bool WorkerDebuggerGlobalScope::WrapGlobalObject(
    JSContext* aCx, JS::MutableHandle<JSObject*> aReflector) {
  mWorkerPrivate->AssertIsOnWorkerThread();

  JS::RealmOptions options;
  mWorkerPrivate->CopyJSRealmOptions(options);

  return WorkerDebuggerGlobalScope_Binding::Wrap(
      aCx, this, this, options,
      new WorkerPrincipal(mWorkerPrivate->UsesSystemPrincipal() ||
                          mWorkerPrivate->UsesAddonOrExpandedAddonPrincipal()),
      true, aReflector);
}

void WorkerDebuggerGlobalScope::GetGlobal(JSContext* aCx,
                                          JS::MutableHandle<JSObject*> aGlobal,
                                          ErrorResult& aRv) {
  WorkerGlobalScope* scope = mWorkerPrivate->GetOrCreateGlobalScope(aCx);
  if (!scope) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  aGlobal.set(scope->GetWrapper());
}

void WorkerDebuggerGlobalScope::CreateSandbox(
    JSContext* aCx, const nsAString& aName, JS::Handle<JSObject*> aPrototype,
    JS::MutableHandle<JSObject*> aResult, ErrorResult& aRv) {
  mWorkerPrivate->AssertIsOnWorkerThread();

  aResult.set(nullptr);

  JS::Rooted<JS::Value> protoVal(aCx);
  protoVal.setObjectOrNull(aPrototype);
  JS::Rooted<JSObject*> sandbox(
      aCx,
      SimpleGlobalObject::Create(
          SimpleGlobalObject::GlobalType::WorkerDebuggerSandbox, protoVal));

  if (!sandbox) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return;
  }

  if (!JS_WrapObject(aCx, &sandbox)) {
    aRv.NoteJSContextException(aCx);
    return;
  }

  aResult.set(sandbox);
}

void WorkerDebuggerGlobalScope::LoadSubScript(
    JSContext* aCx, const nsAString& aURL,
    const Optional<JS::Handle<JSObject*>>& aSandbox, ErrorResult& aRv) {
  mWorkerPrivate->AssertIsOnWorkerThread();

  Maybe<JSAutoRealm> ar;
  if (aSandbox.WasPassed()) {
    // We only care about worker debugger sandbox objects here, so
    // CheckedUnwrapStatic is fine.
    JS::Rooted<JSObject*> sandbox(aCx,
                                  js::CheckedUnwrapStatic(aSandbox.Value()));
    if (!sandbox || !IsWorkerDebuggerSandbox(sandbox)) {
      aRv.Throw(NS_ERROR_INVALID_ARG);
      return;
    }

    ar.emplace(aCx, sandbox);
  }

  nsTArray<nsString> urls;
  urls.AppendElement(aURL);
  workerinternals::Load(mWorkerPrivate, nullptr, urls, DebuggerScript, aRv);
}

void WorkerDebuggerGlobalScope::EnterEventLoop() {
  // We're on the worker thread here, and WorkerPrivate's refcounting is
  // non-threadsafe: you can only do it on the parent thread.  What that
  // means in practice is that we're relying on it being kept alive while
  // we run.  Hopefully.
  MOZ_KnownLive(mWorkerPrivate)->EnterDebuggerEventLoop();
}

void WorkerDebuggerGlobalScope::LeaveEventLoop() {
  mWorkerPrivate->LeaveDebuggerEventLoop();
}

void WorkerDebuggerGlobalScope::PostMessage(const nsAString& aMessage) {
  mWorkerPrivate->PostMessageToDebugger(aMessage);
}

void WorkerDebuggerGlobalScope::SetImmediate(Function& aHandler,
                                             ErrorResult& aRv) {
  mWorkerPrivate->SetDebuggerImmediate(aHandler, aRv);
}

void WorkerDebuggerGlobalScope::ReportError(JSContext* aCx,
                                            const nsAString& aMessage) {
  JS::AutoFilename chars;
  uint32_t lineno = 0;
  JS::DescribeScriptedCaller(aCx, &chars, &lineno);
  nsString filename(NS_ConvertUTF8toUTF16(chars.get()));
  mWorkerPrivate->ReportErrorToDebugger(filename, lineno, aMessage);
}

void WorkerDebuggerGlobalScope::RetrieveConsoleEvents(
    JSContext* aCx, nsTArray<JS::Value>& aEvents, ErrorResult& aRv) {
  WorkerGlobalScope* scope = mWorkerPrivate->GetOrCreateGlobalScope(aCx);
  if (!scope) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  RefPtr<Console> console = scope->GetConsole(aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  console->RetrieveConsoleEvents(aCx, aEvents, aRv);
}

void WorkerDebuggerGlobalScope::SetConsoleEventHandler(JSContext* aCx,
                                                       AnyCallback* aHandler,
                                                       ErrorResult& aRv) {
  WorkerGlobalScope* scope = mWorkerPrivate->GetOrCreateGlobalScope(aCx);
  if (!scope) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  RefPtr<Console> console = scope->GetConsole(aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  console->SetConsoleEventHandler(aHandler);
}

void WorkerDebuggerGlobalScope::Dump(JSContext* aCx,
                                     const Optional<nsAString>& aString) const {
  WorkerGlobalScope* scope = mWorkerPrivate->GetOrCreateGlobalScope(aCx);
  if (scope) {
    scope->Dump(aString);
  }
}

bool IsWorkerGlobal(JSObject* object) {
  return IS_INSTANCE_OF(WorkerGlobalScope, object);
}

bool IsWorkerDebuggerGlobal(JSObject* object) {
  return IS_INSTANCE_OF(WorkerDebuggerGlobalScope, object);
}

bool IsWorkerDebuggerSandbox(JSObject* object) {
  return SimpleGlobalObject::SimpleGlobalType(object) ==
         SimpleGlobalObject::GlobalType::WorkerDebuggerSandbox;
}

}  // namespace dom
}  // namespace mozilla
