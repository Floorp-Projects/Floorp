/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WorkerScope.h"

#include "jsapi.h"
#include "jsfriendapi.h"
#include "mozilla/EventListenerManager.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/Clients.h"
#include "mozilla/dom/ClientState.h"
#include "mozilla/dom/Console.h"
#include "mozilla/dom/CSPEvalChecker.h"
#include "mozilla/dom/DOMMozPromiseRequestHolder.h"
#include "mozilla/dom/DebuggerNotification.h"
#include "mozilla/dom/DedicatedWorkerGlobalScopeBinding.h"
#include "mozilla/dom/Fetch.h"
#include "mozilla/dom/FunctionBinding.h"
#include "mozilla/dom/IDBFactory.h"
#include "mozilla/dom/ImageBitmap.h"
#include "mozilla/dom/ImageBitmapBinding.h"
#include "mozilla/dom/Performance.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/PromiseWorkerProxy.h"
#include "mozilla/dom/ServiceWorkerGlobalScopeBinding.h"
#include "mozilla/dom/ServiceWorkerUtils.h"
#include "mozilla/dom/SharedWorkerGlobalScopeBinding.h"
#include "mozilla/dom/SimpleGlobalObject.h"
#include "mozilla/dom/TimeoutHandler.h"
#include "mozilla/dom/WorkerDebuggerGlobalScopeBinding.h"
#include "mozilla/dom/JSExecutionManager.h"
#include "mozilla/dom/WorkerGlobalScopeBinding.h"
#include "mozilla/dom/WorkerLocation.h"
#include "mozilla/dom/WorkerNavigator.h"
#include "mozilla/dom/cache/CacheStorage.h"
#include "mozilla/StorageAccess.h"
#include "nsContentUtils.h"
#include "nsJSUtils.h"
#include "nsServiceManagerUtils.h"

#include "mozilla/dom/Document.h"
#include "nsIScriptError.h"

#ifdef ANDROID
#  include <android/log.h>
#endif

#include "Crypto.h"
#include "Principal.h"
#include "RuntimeService.h"
#include "ScriptLoader.h"
#include "WorkerPrivate.h"
#include "WorkerRunnable.h"
#include "mozilla/dom/ServiceWorkerManager.h"
#include "mozilla/dom/ServiceWorkerRegistration.h"

#ifdef XP_WIN
#  undef PostMessage
#endif

namespace mozilla {
namespace dom {

using mozilla::dom::cache::CacheStorage;
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

WorkerGlobalScope::WorkerGlobalScope(WorkerPrivate* aWorkerPrivate)
    : mSerialEventTarget(aWorkerPrivate->HybridEventTarget()),
      mWindowInteractionsAllowed(0),
      mWorkerPrivate(aWorkerPrivate) {
  mWorkerPrivate->AssertIsOnWorkerThread();

  // We should always have an event target when the global is created.
  MOZ_DIAGNOSTIC_ASSERT(mSerialEventTarget);

  // In workers, each DETH must have an owner. Because the global scope doesn't
  // have one, let's set it as owner of itself.
  BindToOwner(static_cast<nsIGlobalObject*>(this));
}

WorkerGlobalScope::~WorkerGlobalScope() {
  mWorkerPrivate->AssertIsOnWorkerThread();
}

NS_IMPL_CYCLE_COLLECTION_CLASS(WorkerGlobalScope)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(WorkerGlobalScope,
                                                  DOMEventTargetHelper)
  tmp->mWorkerPrivate->AssertIsOnWorkerThread();
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mConsole)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mCrypto)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPerformance)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mLocation)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mNavigator)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mIndexedDB)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mCacheStorage)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDebuggerNotificationManager)
  tmp->TraverseHostObjectURIs(cb);
  tmp->mWorkerPrivate->TraverseTimeouts(cb);
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(WorkerGlobalScope,
                                                DOMEventTargetHelper)
  tmp->mWorkerPrivate->AssertIsOnWorkerThread();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mConsole)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mCrypto)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mPerformance)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mLocation)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mNavigator)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mIndexedDB)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mCacheStorage)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mDebuggerNotificationManager)
  tmp->UnlinkHostObjectURIs();
  tmp->mWorkerPrivate->UnlinkTimeouts();
  NS_IMPL_CYCLE_COLLECTION_UNLINK_WEAK_REFERENCE
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(WorkerGlobalScope,
                                               DOMEventTargetHelper)
  tmp->mWorkerPrivate->AssertIsOnWorkerThread();
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_ADDREF_INHERITED(WorkerGlobalScope, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(WorkerGlobalScope, DOMEventTargetHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(WorkerGlobalScope)
  NS_INTERFACE_MAP_ENTRY(nsIGlobalObject)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

JSObject* WorkerGlobalScope::WrapObject(JSContext* aCx,
                                        JS::Handle<JSObject*> aGivenProto) {
  MOZ_CRASH("We should never get here!");
}

void WorkerGlobalScope::NoteTerminating() { StartDying(); }

already_AddRefed<Console> WorkerGlobalScope::GetConsole(ErrorResult& aRv) {
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

Crypto* WorkerGlobalScope::GetCrypto(ErrorResult& aError) {
  mWorkerPrivate->AssertIsOnWorkerThread();

  if (!mCrypto) {
    mCrypto = new Crypto(this);
  }

  return mCrypto;
}

already_AddRefed<CacheStorage> WorkerGlobalScope::GetCaches(ErrorResult& aRv) {
  if (!mCacheStorage) {
    MOZ_ASSERT(mWorkerPrivate);
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
    WorkerPrivate::LocationInfo& info = mWorkerPrivate->GetLocationInfo();

    mLocation = WorkerLocation::Create(info);
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
  if (mWorkerPrivate->IsWatchedByDevtools()) {
    stack = GetCurrentStackForNetMonitor(aCx);
  }

  workerinternals::Load(mWorkerPrivate, std::move(stack), aScriptURLs,
                        WorkerScript, aRv);
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
  aOrigin = mWorkerPrivate->Origin();
}

bool WorkerGlobalScope::CrossOriginIsolated() const {
  return mWorkerPrivate->CrossOriginIsolated();
}

void WorkerGlobalScope::Atob(const nsAString& aAtob, nsAString& aOutput,
                             ErrorResult& aRv) const {
  mWorkerPrivate->AssertIsOnWorkerThread();
  aRv = nsContentUtils::Atob(aAtob, aOutput);
}

void WorkerGlobalScope::Btoa(const nsAString& aBtoa, nsAString& aOutput,
                             ErrorResult& aRv) const {
  mWorkerPrivate->AssertIsOnWorkerThread();
  aRv = nsContentUtils::Btoa(aBtoa, aOutput);
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

    nsresult rv = IDBFactory::CreateForWorker(this, principalInfo,
                                              mWorkerPrivate->WindowID(),
                                              getter_AddRefs(indexedDB));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      aErrorResult = rv;
      return nullptr;
    }

    mIndexedDB = indexedDB;
  }

  return indexedDB.forget();
}

already_AddRefed<Promise> WorkerGlobalScope::CreateImageBitmap(
    JSContext* aCx, const ImageBitmapSource& aImage, ErrorResult& aRv) {
  return ImageBitmap::Create(this, aImage, Nothing(), aRv);
}

already_AddRefed<Promise> WorkerGlobalScope::CreateImageBitmap(
    JSContext* aCx, const ImageBitmapSource& aImage, int32_t aSx, int32_t aSy,
    int32_t aSw, int32_t aSh, ErrorResult& aRv) {
  return ImageBitmap::Create(this, aImage,
                             Some(gfx::IntRect(aSx, aSy, aSw, aSh)), aRv);
}

nsresult WorkerGlobalScope::Dispatch(
    TaskCategory aCategory, already_AddRefed<nsIRunnable>&& aRunnable) {
  return EventTargetFor(aCategory)->Dispatch(std::move(aRunnable),
                                             NS_DISPATCH_NORMAL);
}

nsISerialEventTarget* WorkerGlobalScope::EventTargetFor(
    TaskCategory aCategory) const {
  return mSerialEventTarget;
}

AbstractThread* WorkerGlobalScope::AbstractMainThreadFor(
    TaskCategory aCategory) {
  MOZ_CRASH("AbstractMainThreadFor not supported for workers.");
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

bool WorkerGlobalScope::IsSharedMemoryAllowed() const {
  return mWorkerPrivate->IsSharedMemoryAllowed();
}

Maybe<ClientInfo> WorkerGlobalScope::GetClientInfo() const {
  return mWorkerPrivate->GetClientInfo();
}

Maybe<ClientState> WorkerGlobalScope::GetClientState() const {
  Maybe<ClientState> state;
  state.emplace(mWorkerPrivate->GetClientState());
  return state;
}

Maybe<ServiceWorkerDescriptor> WorkerGlobalScope::GetController() const {
  return mWorkerPrivate->GetController();
}

RefPtr<mozilla::dom::ServiceWorkerRegistration>
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

uint64_t WorkerGlobalScope::WindowID() const {
  return mWorkerPrivate->WindowID();
}

void WorkerGlobalScope::FirstPartyStorageAccessGranted() {
  // Reset the IndexedDB factory.
  mIndexedDB = nullptr;

  // Reset DOM Cache
  mCacheStorage = nullptr;
}

DedicatedWorkerGlobalScope::DedicatedWorkerGlobalScope(
    WorkerPrivate* aWorkerPrivate, const nsString& aName)
    : WorkerGlobalScope(aWorkerPrivate), mName(aName) {}

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

void DedicatedWorkerGlobalScope::PostMessage(JSContext* aCx,
                                             JS::Handle<JS::Value> aMessage,
                                             const PostMessageOptions& aOptions,
                                             ErrorResult& aRv) {
  mWorkerPrivate->AssertIsOnWorkerThread();
  mWorkerPrivate->PostMessageToParent(aCx, aMessage, aOptions.mTransfer, aRv);
}

void DedicatedWorkerGlobalScope::Close() {
  mWorkerPrivate->AssertIsOnWorkerThread();
  mWorkerPrivate->CloseInternal();
}

SharedWorkerGlobalScope::SharedWorkerGlobalScope(WorkerPrivate* aWorkerPrivate,
                                                 const nsString& aName)
    : WorkerGlobalScope(aWorkerPrivate), mName(aName) {}

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
                                   mClients, mRegistration)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ServiceWorkerGlobalScope)
NS_INTERFACE_MAP_END_INHERITING(WorkerGlobalScope)

NS_IMPL_ADDREF_INHERITED(ServiceWorkerGlobalScope, WorkerGlobalScope)
NS_IMPL_RELEASE_INHERITED(ServiceWorkerGlobalScope, WorkerGlobalScope)

ServiceWorkerGlobalScope::ServiceWorkerGlobalScope(
    WorkerPrivate* aWorkerPrivate,
    const ServiceWorkerRegistrationDescriptor& aRegistrationDescriptor)
    : WorkerGlobalScope(aWorkerPrivate),
      mScope(NS_ConvertUTF8toUTF16(aRegistrationDescriptor.Scope()))

      // Eagerly create the registration because we will need to receive updates
      // about the state of the registration.  We can't wait until first access
      // to start receiving these.
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
  MOZ_ASSERT(mWorkerPrivate);
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
        nsIScriptError::warningFlag, mSourceSpec, EmptyString(), mLine,
        mColumn);

    return NS_OK;
  }
};

}  // anonymous namespace

void ServiceWorkerGlobalScope::SetOnfetch(
    mozilla::dom::EventHandlerNonNull* aCallback) {
  MOZ_ASSERT(mWorkerPrivate);
  mWorkerPrivate->AssertIsOnWorkerThread();

  if (aCallback) {
    if (mWorkerPrivate->WorkerScriptExecutedSuccessfully()) {
      RefPtr<Runnable> r = new ReportFetchListenerWarningRunnable(mScope);
      mWorkerPrivate->DispatchToMainThreadForMessaging(r.forget());
    }
    mWorkerPrivate->SetFetchHandlerWasAdded();
  }
  SetEventHandler(nsGkAtoms::onfetch, aCallback);
}

void ServiceWorkerGlobalScope::EventListenerAdded(nsAtom* aType) {
  MOZ_ASSERT(mWorkerPrivate);
  mWorkerPrivate->AssertIsOnWorkerThread();

  if (aType != nsGkAtoms::onfetch) {
    return;
  }

  if (mWorkerPrivate->WorkerScriptExecutedSuccessfully()) {
    RefPtr<Runnable> r = new ReportFetchListenerWarningRunnable(mScope);
    mWorkerPrivate->DispatchToMainThreadForMessaging(r.forget());
  }

  mWorkerPrivate->SetFetchHandlerWasAdded();
}

namespace {

class SkipWaitingResultRunnable final : public WorkerRunnable {
  RefPtr<PromiseWorkerProxy> mPromiseProxy;

 public:
  SkipWaitingResultRunnable(WorkerPrivate* aWorkerPrivate,
                            PromiseWorkerProxy* aPromiseProxy)
      : WorkerRunnable(aWorkerPrivate), mPromiseProxy(aPromiseProxy) {
    AssertIsOnMainThread();
  }

  virtual bool WorkerRun(JSContext* aCx,
                         WorkerPrivate* aWorkerPrivate) override {
    MOZ_ASSERT(aWorkerPrivate);
    aWorkerPrivate->AssertIsOnWorkerThread();

    RefPtr<Promise> promise = mPromiseProxy->WorkerPromise();
    promise->MaybeResolveWithUndefined();

    // Release the reference on the worker thread.
    mPromiseProxy->CleanUp();

    return true;
  }
};

class WorkerScopeSkipWaitingRunnable final : public Runnable {
  RefPtr<PromiseWorkerProxy> mPromiseProxy;
  nsCString mScope;

 public:
  WorkerScopeSkipWaitingRunnable(PromiseWorkerProxy* aPromiseProxy,
                                 const nsCString& aScope)
      : mozilla::Runnable("WorkerScopeSkipWaitingRunnable"),
        mPromiseProxy(aPromiseProxy),
        mScope(aScope) {
    MOZ_ASSERT(aPromiseProxy);
  }

  NS_IMETHOD
  Run() override {
    AssertIsOnMainThread();

    MutexAutoLock lock(mPromiseProxy->Lock());
    if (mPromiseProxy->CleanedUp()) {
      return NS_OK;
    }

    WorkerPrivate* workerPrivate = mPromiseProxy->GetWorkerPrivate();
    MOZ_DIAGNOSTIC_ASSERT(workerPrivate);

    RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
    if (swm) {
      swm->SetSkipWaitingFlag(workerPrivate->GetPrincipal(), mScope,
                              workerPrivate->ServiceWorkerID());
    }

    RefPtr<SkipWaitingResultRunnable> runnable =
        new SkipWaitingResultRunnable(workerPrivate, mPromiseProxy);

    if (!runnable->Dispatch()) {
      NS_WARNING("Failed to dispatch SkipWaitingResultRunnable to the worker.");
    }
    return NS_OK;
  }
};

}  // namespace

already_AddRefed<Promise> ServiceWorkerGlobalScope::SkipWaiting(
    ErrorResult& aRv) {
  mWorkerPrivate->AssertIsOnWorkerThread();
  MOZ_ASSERT(mWorkerPrivate->IsServiceWorker());

  RefPtr<Promise> promise = Promise::Create(this, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  if (ServiceWorkerParentInterceptEnabled()) {
    using MozPromiseType = decltype(
        mWorkerPrivate->SetServiceWorkerSkipWaitingFlag())::element_type;
    auto holder = MakeRefPtr<DOMMozPromiseRequestHolder<MozPromiseType>>(this);

    mWorkerPrivate->SetServiceWorkerSkipWaitingFlag()
        ->Then(GetCurrentThreadSerialEventTarget(), __func__,
               [holder, promise](const MozPromiseType::ResolveOrRejectValue&) {
                 holder->Complete();
                 promise->MaybeResolveWithUndefined();
               })
        ->Track(*holder);

    return promise.forget();
  }

  RefPtr<PromiseWorkerProxy> promiseProxy =
      PromiseWorkerProxy::Create(mWorkerPrivate, promise);
  if (!promiseProxy) {
    promise->MaybeResolveWithUndefined();
    return promise.forget();
  }

  RefPtr<WorkerScopeSkipWaitingRunnable> runnable =
      new WorkerScopeSkipWaitingRunnable(promiseProxy,
                                         NS_ConvertUTF16toUTF8(mScope));

  MOZ_ALWAYS_SUCCEEDS(mWorkerPrivate->DispatchToMainThread(runnable.forget()));
  return promise.forget();
}

WorkerDebuggerGlobalScope::WorkerDebuggerGlobalScope(
    WorkerPrivate* aWorkerPrivate)
    : mWorkerPrivate(aWorkerPrivate),
      mSerialEventTarget(aWorkerPrivate->HybridEventTarget()) {
  mWorkerPrivate->AssertIsOnWorkerThread();

  // We should always have an event target when the global is created.
  MOZ_DIAGNOSTIC_ASSERT(mSerialEventTarget);

  // In workers, each DETH must have an owner. Because the global scope doesn't
  // have an owner, let's set it as owner of itself.
  BindToOwner(static_cast<nsIGlobalObject*>(this));
}

WorkerDebuggerGlobalScope::~WorkerDebuggerGlobalScope() {
  mWorkerPrivate->AssertIsOnWorkerThread();
}

NS_IMPL_CYCLE_COLLECTION_CLASS(WorkerDebuggerGlobalScope)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(WorkerDebuggerGlobalScope,
                                                  DOMEventTargetHelper)
  tmp->mWorkerPrivate->AssertIsOnWorkerThread();
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mConsole)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(WorkerDebuggerGlobalScope,
                                                DOMEventTargetHelper)
  tmp->mWorkerPrivate->AssertIsOnWorkerThread();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mConsole)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(WorkerDebuggerGlobalScope,
                                               DOMEventTargetHelper)
  tmp->mWorkerPrivate->AssertIsOnWorkerThread();
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_ADDREF_INHERITED(WorkerDebuggerGlobalScope, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(WorkerDebuggerGlobalScope, DOMEventTargetHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(WorkerDebuggerGlobalScope)
  NS_INTERFACE_MAP_ENTRY(nsIGlobalObject)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

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

already_AddRefed<Console> WorkerDebuggerGlobalScope::GetConsole(
    ErrorResult& aRv) {
  mWorkerPrivate->AssertIsOnWorkerThread();

  // Debugger console has its own console object.
  if (!mConsole) {
    mConsole = Console::Create(mWorkerPrivate->GetJSContext(), nullptr, aRv);
    if (NS_WARN_IF(aRv.Failed())) {
      return nullptr;
    }
  }

  RefPtr<Console> console = mConsole;
  return console.forget();
}

void WorkerDebuggerGlobalScope::Dump(JSContext* aCx,
                                     const Optional<nsAString>& aString) const {
  WorkerGlobalScope* scope = mWorkerPrivate->GetOrCreateGlobalScope(aCx);
  if (scope) {
    scope->Dump(aString);
  }
}

void WorkerDebuggerGlobalScope::Atob(const nsAString& aAtob, nsAString& aOutput,
                                     ErrorResult& aRv) const {
  mWorkerPrivate->AssertIsOnWorkerThread();
  aRv = nsContentUtils::Atob(aAtob, aOutput);
}

void WorkerDebuggerGlobalScope::Btoa(const nsAString& aBtoa, nsAString& aOutput,
                                     ErrorResult& aRv) const {
  mWorkerPrivate->AssertIsOnWorkerThread();
  aRv = nsContentUtils::Btoa(aBtoa, aOutput);
}

nsresult WorkerDebuggerGlobalScope::Dispatch(
    TaskCategory aCategory, already_AddRefed<nsIRunnable>&& aRunnable) {
  return EventTargetFor(aCategory)->Dispatch(std::move(aRunnable),
                                             NS_DISPATCH_NORMAL);
}

nsISerialEventTarget* WorkerDebuggerGlobalScope::EventTargetFor(
    TaskCategory aCategory) const {
  return mSerialEventTarget;
}

AbstractThread* WorkerDebuggerGlobalScope::AbstractMainThreadFor(
    TaskCategory aCategory) {
  MOZ_CRASH("AbstractMainThreadFor not supported for workers.");
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
