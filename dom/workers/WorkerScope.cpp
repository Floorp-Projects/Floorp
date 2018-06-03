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
#include "mozilla/dom/DedicatedWorkerGlobalScopeBinding.h"
#include "mozilla/dom/DOMPrefs.h"
#include "mozilla/dom/Fetch.h"
#include "mozilla/dom/FunctionBinding.h"
#include "mozilla/dom/IDBFactory.h"
#include "mozilla/dom/ImageBitmap.h"
#include "mozilla/dom/ImageBitmapBinding.h"
#include "mozilla/dom/Performance.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/PromiseWorkerProxy.h"
#include "mozilla/dom/ServiceWorkerGlobalScopeBinding.h"
#include "mozilla/dom/SharedWorkerGlobalScopeBinding.h"
#include "mozilla/dom/SimpleGlobalObject.h"
#include "mozilla/dom/WorkerDebuggerGlobalScopeBinding.h"
#include "mozilla/dom/WorkerGlobalScopeBinding.h"
#include "mozilla/dom/WorkerLocation.h"
#include "mozilla/dom/WorkerNavigator.h"
#include "mozilla/dom/cache/CacheStorage.h"
#include "mozilla/Services.h"
#include "nsServiceManagerUtils.h"

#include "nsIDocument.h"
#include "nsIServiceWorkerManager.h"
#include "nsIScriptError.h"
#include "nsIScriptTimeoutHandler.h"

#ifdef ANDROID
#include <android/log.h>
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
#undef PostMessage
#endif

extern already_AddRefed<nsIScriptTimeoutHandler>
NS_CreateJSTimeoutHandler(JSContext* aCx,
                          mozilla::dom::WorkerPrivate* aWorkerPrivate,
                          mozilla::dom::Function& aFunction,
                          const mozilla::dom::Sequence<JS::Value>& aArguments,
                          mozilla::ErrorResult& aError);

extern already_AddRefed<nsIScriptTimeoutHandler>
NS_CreateJSTimeoutHandler(JSContext* aCx,
                          mozilla::dom::WorkerPrivate* aWorkerPrivate,
                          const nsAString& aExpression);

namespace mozilla {
namespace dom {

using mozilla::dom::cache::CacheStorage;
using mozilla::ipc::PrincipalInfo;

WorkerGlobalScope::WorkerGlobalScope(WorkerPrivate* aWorkerPrivate)
: mSerialEventTarget(aWorkerPrivate->HybridEventTarget())
, mWindowInteractionsAllowed(0)
, mWorkerPrivate(aWorkerPrivate)
{
  mWorkerPrivate->AssertIsOnWorkerThread();

  // We should always have an event target when the global is created.
  MOZ_DIAGNOSTIC_ASSERT(mSerialEventTarget);
}

WorkerGlobalScope::~WorkerGlobalScope()
{
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
  tmp->UnlinkHostObjectURIs();
  tmp->mWorkerPrivate->UnlinkTimeouts();
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

JSObject*
WorkerGlobalScope::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  MOZ_CRASH("We should never get here!");
}

void
WorkerGlobalScope::NoteTerminating()
{
  DisconnectEventTargetObjects();
}

already_AddRefed<Console>
WorkerGlobalScope::GetConsole(ErrorResult& aRv)
{
  mWorkerPrivate->AssertIsOnWorkerThread();

  if (!mConsole) {
    mConsole = Console::Create(mWorkerPrivate->GetJSContext(),
                               nullptr, aRv);
    if (NS_WARN_IF(aRv.Failed())) {
      return nullptr;
    }
  }

  RefPtr<Console> console = mConsole;
  return console.forget();
}

Crypto*
WorkerGlobalScope::GetCrypto(ErrorResult& aError)
{
  mWorkerPrivate->AssertIsOnWorkerThread();

  if (!mCrypto) {
    mCrypto = new Crypto(this);
  }

  return mCrypto;
}

already_AddRefed<CacheStorage>
WorkerGlobalScope::GetCaches(ErrorResult& aRv)
{
  if (!mCacheStorage) {
    MOZ_ASSERT(mWorkerPrivate);
    mCacheStorage = CacheStorage::CreateOnWorker(cache::DEFAULT_NAMESPACE, this,
                                                 mWorkerPrivate, aRv);
  }

  RefPtr<CacheStorage> ref = mCacheStorage;
  return ref.forget();
}

bool
WorkerGlobalScope::IsSecureContext() const
{
  bool globalSecure =
    JS::GetIsSecureContext(js::GetNonCCWObjectRealm(GetWrapperPreserveColor()));
  MOZ_ASSERT(globalSecure == mWorkerPrivate->IsSecureContext());
  return globalSecure;
}

already_AddRefed<WorkerLocation>
WorkerGlobalScope::Location()
{
  mWorkerPrivate->AssertIsOnWorkerThread();

  if (!mLocation) {
    WorkerPrivate::LocationInfo& info = mWorkerPrivate->GetLocationInfo();

    mLocation = WorkerLocation::Create(info);
    MOZ_ASSERT(mLocation);
  }

  RefPtr<WorkerLocation> location = mLocation;
  return location.forget();
}

already_AddRefed<WorkerNavigator>
WorkerGlobalScope::Navigator()
{
  mWorkerPrivate->AssertIsOnWorkerThread();

  if (!mNavigator) {
    mNavigator = WorkerNavigator::Create(mWorkerPrivate->OnLine());
    MOZ_ASSERT(mNavigator);
  }

  RefPtr<WorkerNavigator> navigator = mNavigator;
  return navigator.forget();
}

already_AddRefed<WorkerNavigator>
WorkerGlobalScope::GetExistingNavigator() const
{
  mWorkerPrivate->AssertIsOnWorkerThread();

  RefPtr<WorkerNavigator> navigator = mNavigator;
  return navigator.forget();
}

OnErrorEventHandlerNonNull*
WorkerGlobalScope::GetOnerror()
{
  mWorkerPrivate->AssertIsOnWorkerThread();

  EventListenerManager* elm = GetExistingListenerManager();
  return elm ? elm->GetOnErrorEventHandler() : nullptr;
}

void
WorkerGlobalScope::SetOnerror(OnErrorEventHandlerNonNull* aHandler)
{
  mWorkerPrivate->AssertIsOnWorkerThread();

  EventListenerManager* elm = GetOrCreateListenerManager();
  if (elm) {
    elm->SetEventHandler(aHandler);
  }
}

void
WorkerGlobalScope::ImportScripts(const Sequence<nsString>& aScriptURLs,
                                 ErrorResult& aRv)
{
  mWorkerPrivate->AssertIsOnWorkerThread();
  workerinternals::Load(mWorkerPrivate, aScriptURLs, WorkerScript, aRv);
}

int32_t
WorkerGlobalScope::SetTimeout(JSContext* aCx,
                              Function& aHandler,
                              const int32_t aTimeout,
                              const Sequence<JS::Value>& aArguments,
                              ErrorResult& aRv)
{
  mWorkerPrivate->AssertIsOnWorkerThread();

  nsCOMPtr<nsIScriptTimeoutHandler> handler =
    NS_CreateJSTimeoutHandler(aCx, mWorkerPrivate, aHandler, aArguments, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return 0;
  }

  return mWorkerPrivate->SetTimeout(aCx, handler, aTimeout, false, aRv);
}

int32_t
WorkerGlobalScope::SetTimeout(JSContext* aCx,
                              const nsAString& aHandler,
                              const int32_t aTimeout,
                              const Sequence<JS::Value>& /* unused */,
                              ErrorResult& aRv)
{
  mWorkerPrivate->AssertIsOnWorkerThread();

  nsCOMPtr<nsIScriptTimeoutHandler> handler =
    NS_CreateJSTimeoutHandler(aCx, mWorkerPrivate, aHandler);
  return mWorkerPrivate->SetTimeout(aCx, handler, aTimeout, false, aRv);
}

void
WorkerGlobalScope::ClearTimeout(int32_t aHandle)
{
  mWorkerPrivate->AssertIsOnWorkerThread();
  mWorkerPrivate->ClearTimeout(aHandle);
}

int32_t
WorkerGlobalScope::SetInterval(JSContext* aCx,
                               Function& aHandler,
                               const int32_t aTimeout,
                               const Sequence<JS::Value>& aArguments,
                               ErrorResult& aRv)
{
  mWorkerPrivate->AssertIsOnWorkerThread();

  nsCOMPtr<nsIScriptTimeoutHandler> handler =
    NS_CreateJSTimeoutHandler(aCx, mWorkerPrivate, aHandler, aArguments, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return 0;
  }

  return mWorkerPrivate->SetTimeout(aCx, handler,  aTimeout, true, aRv);
}

int32_t
WorkerGlobalScope::SetInterval(JSContext* aCx,
                               const nsAString& aHandler,
                               const int32_t aTimeout,
                               const Sequence<JS::Value>& /* unused */,
                               ErrorResult& aRv)
{
  mWorkerPrivate->AssertIsOnWorkerThread();

  Sequence<JS::Value> dummy;

  nsCOMPtr<nsIScriptTimeoutHandler> handler =
    NS_CreateJSTimeoutHandler(aCx, mWorkerPrivate, aHandler);
  return mWorkerPrivate->SetTimeout(aCx, handler, aTimeout, true, aRv);
}

void
WorkerGlobalScope::ClearInterval(int32_t aHandle)
{
  mWorkerPrivate->AssertIsOnWorkerThread();
  mWorkerPrivate->ClearTimeout(aHandle);
}

void
WorkerGlobalScope::GetOrigin(nsAString& aOrigin) const
{
  mWorkerPrivate->AssertIsOnWorkerThread();
  aOrigin = mWorkerPrivate->Origin();
}

void
WorkerGlobalScope::Atob(const nsAString& aAtob, nsAString& aOutput, ErrorResult& aRv) const
{
  mWorkerPrivate->AssertIsOnWorkerThread();
  aRv = nsContentUtils::Atob(aAtob, aOutput);
}

void
WorkerGlobalScope::Btoa(const nsAString& aBtoa, nsAString& aOutput, ErrorResult& aRv) const
{
  mWorkerPrivate->AssertIsOnWorkerThread();
  aRv = nsContentUtils::Btoa(aBtoa, aOutput);
}

void
WorkerGlobalScope::Dump(const Optional<nsAString>& aString) const
{
  mWorkerPrivate->AssertIsOnWorkerThread();

  if (!aString.WasPassed()) {
    return;
  }

#if !(defined(DEBUG) || defined(MOZ_ENABLE_JS_DUMP))
  if (!DOMPrefs::DumpEnabled()) {
    return;
  }
#endif

  NS_ConvertUTF16toUTF8 str(aString.Value());

  MOZ_LOG(nsContentUtils::DOMDumpLog(), LogLevel::Debug, ("[Worker.Dump] %s", str.get()));
#ifdef ANDROID
  __android_log_print(ANDROID_LOG_INFO, "Gecko", "%s", str.get());
#endif
  fputs(str.get(), stdout);
  fflush(stdout);
}

Performance*
WorkerGlobalScope::GetPerformance()
{
  mWorkerPrivate->AssertIsOnWorkerThread();

  if (!mPerformance) {
    mPerformance = Performance::CreateForWorker(mWorkerPrivate);
  }

  return mPerformance;
}

bool
WorkerGlobalScope::IsInAutomation(JSContext* aCx, JSObject* /* unused */)
{
  return GetWorkerPrivateFromContext(aCx)->IsInAutomation();
}

void
WorkerGlobalScope::GetJSTestingFunctions(JSContext* aCx,
                                         JS::MutableHandle<JSObject*> aFunctions,
                                         ErrorResult& aRv)
{
  JSObject* obj = js::GetTestingFunctions(aCx);
  if (!obj) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return;
  }

  aFunctions.set(obj);
}

already_AddRefed<Promise>
WorkerGlobalScope::Fetch(const RequestOrUSVString& aInput,
                         const RequestInit& aInit,
                         CallerType aCallerType, ErrorResult& aRv)
{
  return FetchRequest(this, aInput, aInit, aCallerType, aRv);
}

already_AddRefed<IDBFactory>
WorkerGlobalScope::GetIndexedDB(ErrorResult& aErrorResult)
{
  mWorkerPrivate->AssertIsOnWorkerThread();

  RefPtr<IDBFactory> indexedDB = mIndexedDB;

  if (!indexedDB) {
    if (!mWorkerPrivate->IsStorageAllowed()) {
      NS_WARNING("IndexedDB is not allowed in this worker!");
      aErrorResult = NS_ERROR_DOM_SECURITY_ERR;
      return nullptr;
    }

    JSContext* cx = mWorkerPrivate->GetJSContext();
    MOZ_ASSERT(cx);

    JS::Rooted<JSObject*> owningObject(cx, GetGlobalJSObject());
    MOZ_ASSERT(owningObject);

    const PrincipalInfo& principalInfo = mWorkerPrivate->GetPrincipalInfo();

    nsresult rv =
      IDBFactory::CreateForWorker(cx,
                                  owningObject,
                                  principalInfo,
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

already_AddRefed<Promise>
WorkerGlobalScope::CreateImageBitmap(JSContext* aCx,
                                     const ImageBitmapSource& aImage,
                                     ErrorResult& aRv)
{
  if (aImage.IsArrayBuffer() || aImage.IsArrayBufferView()) {
    aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
    return nullptr;
  }

  return ImageBitmap::Create(this, aImage, Nothing(), aRv);
}

already_AddRefed<Promise>
WorkerGlobalScope::CreateImageBitmap(JSContext* aCx,
                                     const ImageBitmapSource& aImage,
                                     int32_t aSx, int32_t aSy, int32_t aSw, int32_t aSh,
                                     ErrorResult& aRv)
{
  if (aImage.IsArrayBuffer() || aImage.IsArrayBufferView()) {
    aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
    return nullptr;
  }

  return ImageBitmap::Create(this, aImage, Some(gfx::IntRect(aSx, aSy, aSw, aSh)), aRv);
}

already_AddRefed<mozilla::dom::Promise>
WorkerGlobalScope::CreateImageBitmap(JSContext* aCx,
                                     const ImageBitmapSource& aImage,
                                     int32_t aOffset, int32_t aLength,
                                     ImageBitmapFormat aFormat,
                                     const Sequence<ChannelPixelLayout>& aLayout,
                                     ErrorResult& aRv)
{
  if (!DOMPrefs::ImageBitmapExtensionsEnabled()) {
    aRv.Throw(NS_ERROR_TYPE_ERR);
    return nullptr;
  }

  if (aImage.IsArrayBuffer() || aImage.IsArrayBufferView()) {
    return ImageBitmap::Create(this, aImage, aOffset, aLength, aFormat, aLayout,
                               aRv);
  } else {
    aRv.Throw(NS_ERROR_TYPE_ERR);
    return nullptr;
  }
}

nsresult
WorkerGlobalScope::Dispatch(TaskCategory aCategory,
                            already_AddRefed<nsIRunnable>&& aRunnable)
{
  return EventTargetFor(aCategory)->Dispatch(std::move(aRunnable),
                                             NS_DISPATCH_NORMAL);
}

nsISerialEventTarget*
WorkerGlobalScope::EventTargetFor(TaskCategory aCategory) const
{
  return mSerialEventTarget;
}

AbstractThread*
WorkerGlobalScope::AbstractMainThreadFor(TaskCategory aCategory)
{
  MOZ_CRASH("AbstractMainThreadFor not supported for workers.");
}

Maybe<ClientInfo>
WorkerGlobalScope::GetClientInfo() const
{
  return mWorkerPrivate->GetClientInfo();
}

Maybe<ClientState>
WorkerGlobalScope::GetClientState() const
{
  Maybe<ClientState> state;
  state.emplace(mWorkerPrivate->GetClientState());
  return std::move(state);
}

Maybe<ServiceWorkerDescriptor>
WorkerGlobalScope::GetController() const
{
  return mWorkerPrivate->GetController();
}

RefPtr<ServiceWorkerRegistration>
WorkerGlobalScope::GetOrCreateServiceWorkerRegistration(const ServiceWorkerRegistrationDescriptor& aDescriptor)
{
  mWorkerPrivate->AssertIsOnWorkerThread();
  RefPtr<ServiceWorkerRegistration> ref;
  ForEachEventTargetObject([&] (DOMEventTargetHelper* aTarget, bool* aDoneOut) {
    RefPtr<ServiceWorkerRegistration> swr = do_QueryObject(aTarget);
    if (!swr || !swr->MatchesDescriptor(aDescriptor)) {
      return;
    }

    ref = swr.forget();
    *aDoneOut = true;
  });

  if (!ref) {
    ref = ServiceWorkerRegistration::CreateForWorker(mWorkerPrivate, this,
                                                     aDescriptor);
  }

  return ref.forget();
}

DedicatedWorkerGlobalScope::DedicatedWorkerGlobalScope(WorkerPrivate* aWorkerPrivate,
                                                       const nsString& aName)
  : WorkerGlobalScope(aWorkerPrivate)
  , mName(aName)
{
}

bool
DedicatedWorkerGlobalScope::WrapGlobalObject(JSContext* aCx,
                                             JS::MutableHandle<JSObject*> aReflector)
{
  mWorkerPrivate->AssertIsOnWorkerThread();
  MOZ_ASSERT(!mWorkerPrivate->IsSharedWorker());

  JS::RealmOptions options;
  mWorkerPrivate->CopyJSRealmOptions(options);

  const bool usesSystemPrincipal = mWorkerPrivate->UsesSystemPrincipal();

  // Note that xpc::ShouldDiscardSystemSource() and
  // xpc::ExtraWarningsForSystemJS() read prefs that are cached on the main
  // thread. This is benignly racey.
  const bool discardSource = usesSystemPrincipal &&
                             xpc::ShouldDiscardSystemSource();
  const bool extraWarnings = usesSystemPrincipal &&
                             xpc::ExtraWarningsForSystemJS();

  JS::RealmBehaviors& behaviors = options.behaviors();
  behaviors.setDiscardSource(discardSource)
           .extraWarningsOverride().set(extraWarnings);

  const bool sharedMemoryEnabled = xpc::SharedMemoryEnabled();

  JS::RealmCreationOptions& creationOptions = options.creationOptions();
  creationOptions.setSharedMemoryAndAtomicsEnabled(sharedMemoryEnabled);

  return DedicatedWorkerGlobalScopeBinding::Wrap(aCx, this, this,
                                                 options,
                                                 GetWorkerPrincipal(),
                                                 true, aReflector);
}

void
DedicatedWorkerGlobalScope::PostMessage(JSContext* aCx,
                                        JS::Handle<JS::Value> aMessage,
                                        const Sequence<JSObject*>& aTransferable,
                                        ErrorResult& aRv)
{
  mWorkerPrivate->AssertIsOnWorkerThread();
  mWorkerPrivate->PostMessageToParent(aCx, aMessage, aTransferable, aRv);
}

void
DedicatedWorkerGlobalScope::Close()
{
  mWorkerPrivate->AssertIsOnWorkerThread();
  mWorkerPrivate->CloseInternal();
}

SharedWorkerGlobalScope::SharedWorkerGlobalScope(WorkerPrivate* aWorkerPrivate,
                                                 const nsString& aName)
: WorkerGlobalScope(aWorkerPrivate), mName(aName)
{
}

bool
SharedWorkerGlobalScope::WrapGlobalObject(JSContext* aCx,
                                          JS::MutableHandle<JSObject*> aReflector)
{
  mWorkerPrivate->AssertIsOnWorkerThread();
  MOZ_ASSERT(mWorkerPrivate->IsSharedWorker());

  JS::RealmOptions options;
  mWorkerPrivate->CopyJSRealmOptions(options);

  return SharedWorkerGlobalScopeBinding::Wrap(aCx, this, this, options,
                                              GetWorkerPrincipal(),
                                              true, aReflector);
}

void
SharedWorkerGlobalScope::Close()
{
  mWorkerPrivate->AssertIsOnWorkerThread();
  mWorkerPrivate->CloseInternal();
}

NS_IMPL_CYCLE_COLLECTION_INHERITED(ServiceWorkerGlobalScope, WorkerGlobalScope,
                                   mClients, mRegistration)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ServiceWorkerGlobalScope)
NS_INTERFACE_MAP_END_INHERITING(WorkerGlobalScope)

NS_IMPL_ADDREF_INHERITED(ServiceWorkerGlobalScope, WorkerGlobalScope)
NS_IMPL_RELEASE_INHERITED(ServiceWorkerGlobalScope, WorkerGlobalScope)

ServiceWorkerGlobalScope::ServiceWorkerGlobalScope(WorkerPrivate* aWorkerPrivate,
                                                   const ServiceWorkerRegistrationDescriptor& aRegistrationDescriptor)
  : WorkerGlobalScope(aWorkerPrivate)
  , mScope(NS_ConvertUTF8toUTF16(aRegistrationDescriptor.Scope()))

  // Eagerly create the registration because we will need to receive updates
  // about the state of the registration.  We can't wait until first access
  // to start receiving these.
  , mRegistration(GetOrCreateServiceWorkerRegistration(aRegistrationDescriptor))
{
}

ServiceWorkerGlobalScope::~ServiceWorkerGlobalScope()
{
}

bool
ServiceWorkerGlobalScope::WrapGlobalObject(JSContext* aCx,
                                           JS::MutableHandle<JSObject*> aReflector)
{
  mWorkerPrivate->AssertIsOnWorkerThread();
  MOZ_ASSERT(mWorkerPrivate->IsServiceWorker());

  JS::RealmOptions options;
  mWorkerPrivate->CopyJSRealmOptions(options);

  return ServiceWorkerGlobalScopeBinding::Wrap(aCx, this, this, options,
                                               GetWorkerPrincipal(),
                                               true, aReflector);
}

already_AddRefed<Clients>
ServiceWorkerGlobalScope::GetClients()
{
  if (!mClients) {
    mClients = new Clients(this);
  }

  RefPtr<Clients> ref = mClients;
  return ref.forget();
}

ServiceWorkerRegistration*
ServiceWorkerGlobalScope::Registration()
{
  return mRegistration;
}

EventHandlerNonNull*
ServiceWorkerGlobalScope::GetOnfetch()
{
  MOZ_ASSERT(mWorkerPrivate);
  mWorkerPrivate->AssertIsOnWorkerThread();

  return GetEventHandler(nullptr, NS_LITERAL_STRING("fetch"));
}

namespace {

class ReportFetchListenerWarningRunnable final : public Runnable
{
  const nsCString mScope;
  nsCString mSourceSpec;
  uint32_t mLine;
  uint32_t mColumn;

public:
  explicit ReportFetchListenerWarningRunnable(const nsString& aScope)
    : mozilla::Runnable("ReportFetchListenerWarningRunnable")
    , mScope(NS_ConvertUTF16toUTF8(aScope))
  {
    WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
    MOZ_ASSERT(workerPrivate);
    JSContext* cx = workerPrivate->GetJSContext();
    MOZ_ASSERT(cx);

    nsJSUtils::GetCallingLocation(cx, mSourceSpec, &mLine, &mColumn);
  }

  NS_IMETHOD
  Run() override
  {
    AssertIsOnMainThread();

    ServiceWorkerManager::LocalizeAndReportToAllClients(mScope, "ServiceWorkerNoFetchHandler",
        nsTArray<nsString>{}, nsIScriptError::warningFlag, NS_ConvertUTF8toUTF16(mSourceSpec),
        EmptyString(), mLine, mColumn);

    return NS_OK;
  }
};

} // anonymous namespace

void
ServiceWorkerGlobalScope::SetOnfetch(mozilla::dom::EventHandlerNonNull* aCallback)
{
  MOZ_ASSERT(mWorkerPrivate);
  mWorkerPrivate->AssertIsOnWorkerThread();

  if (aCallback) {
    if (mWorkerPrivate->WorkerScriptExecutedSuccessfully()) {
      RefPtr<Runnable> r = new ReportFetchListenerWarningRunnable(mScope);
      mWorkerPrivate->DispatchToMainThread(r.forget());
    }
    mWorkerPrivate->SetFetchHandlerWasAdded();
  }
  SetEventHandler(nullptr, NS_LITERAL_STRING("fetch"), aCallback);
}

void
ServiceWorkerGlobalScope::EventListenerAdded(const nsAString& aType)
{
  MOZ_ASSERT(mWorkerPrivate);
  mWorkerPrivate->AssertIsOnWorkerThread();

  if (!aType.EqualsLiteral("fetch")) {
    return;
  }

  if (mWorkerPrivate->WorkerScriptExecutedSuccessfully()) {
    RefPtr<Runnable> r = new ReportFetchListenerWarningRunnable(mScope);
    mWorkerPrivate->DispatchToMainThread(r.forget());
  }

  mWorkerPrivate->SetFetchHandlerWasAdded();
}

namespace {

class SkipWaitingResultRunnable final : public WorkerRunnable
{
  RefPtr<PromiseWorkerProxy> mPromiseProxy;

public:
  SkipWaitingResultRunnable(WorkerPrivate* aWorkerPrivate,
                            PromiseWorkerProxy* aPromiseProxy)
    : WorkerRunnable(aWorkerPrivate)
    , mPromiseProxy(aPromiseProxy)
  {
    AssertIsOnMainThread();
  }

  virtual bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    MOZ_ASSERT(aWorkerPrivate);
    aWorkerPrivate->AssertIsOnWorkerThread();

    RefPtr<Promise> promise = mPromiseProxy->WorkerPromise();
    promise->MaybeResolveWithUndefined();

    // Release the reference on the worker thread.
    mPromiseProxy->CleanUp();

    return true;
  }
};

class WorkerScopeSkipWaitingRunnable final : public Runnable
{
  RefPtr<PromiseWorkerProxy> mPromiseProxy;
  nsCString mScope;

public:
  WorkerScopeSkipWaitingRunnable(PromiseWorkerProxy* aPromiseProxy,
                                 const nsCString& aScope)
    : mozilla::Runnable("WorkerScopeSkipWaitingRunnable")
    , mPromiseProxy(aPromiseProxy)
    , mScope(aScope)
  {
    MOZ_ASSERT(aPromiseProxy);
  }

  NS_IMETHOD
  Run() override
  {
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

} // namespace

already_AddRefed<Promise>
ServiceWorkerGlobalScope::SkipWaiting(ErrorResult& aRv)
{
  mWorkerPrivate->AssertIsOnWorkerThread();
  MOZ_ASSERT(mWorkerPrivate->IsServiceWorker());

  RefPtr<Promise> promise = Promise::Create(this, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
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
: mWorkerPrivate(aWorkerPrivate)
, mSerialEventTarget(aWorkerPrivate->HybridEventTarget())
{
  mWorkerPrivate->AssertIsOnWorkerThread();

  // We should always have an event target when the global is created.
  MOZ_DIAGNOSTIC_ASSERT(mSerialEventTarget);
}

WorkerDebuggerGlobalScope::~WorkerDebuggerGlobalScope()
{
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

bool
WorkerDebuggerGlobalScope::WrapGlobalObject(JSContext* aCx,
                                            JS::MutableHandle<JSObject*> aReflector)
{
  mWorkerPrivate->AssertIsOnWorkerThread();

  JS::RealmOptions options;
  mWorkerPrivate->CopyJSRealmOptions(options);

  return WorkerDebuggerGlobalScopeBinding::Wrap(aCx, this, this, options,
                                                GetWorkerPrincipal(), true,
                                                aReflector);
}

void
WorkerDebuggerGlobalScope::GetGlobal(JSContext* aCx,
                                     JS::MutableHandle<JSObject*> aGlobal,
                                     ErrorResult& aRv)
{
  WorkerGlobalScope* scope = mWorkerPrivate->GetOrCreateGlobalScope(aCx);
  if (!scope) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  aGlobal.set(scope->GetWrapper());
}

void
WorkerDebuggerGlobalScope::CreateSandbox(JSContext* aCx, const nsAString& aName,
                                         JS::Handle<JSObject*> aPrototype,
                                         JS::MutableHandle<JSObject*> aResult,
                                         ErrorResult& aRv)
{
  mWorkerPrivate->AssertIsOnWorkerThread();

  aResult.set(nullptr);

  JS::Rooted<JS::Value> protoVal(aCx);
  protoVal.setObjectOrNull(aPrototype);
  JS::Rooted<JSObject*> sandbox(aCx,
    SimpleGlobalObject::Create(SimpleGlobalObject::GlobalType::WorkerDebuggerSandbox,
                               protoVal));

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

void
WorkerDebuggerGlobalScope::LoadSubScript(JSContext* aCx,
                                         const nsAString& aURL,
                                         const Optional<JS::Handle<JSObject*>>& aSandbox,
                                         ErrorResult& aRv)
{
  mWorkerPrivate->AssertIsOnWorkerThread();

  Maybe<JSAutoRealm> ar;
  if (aSandbox.WasPassed()) {
    JS::Rooted<JSObject*> sandbox(aCx, js::CheckedUnwrap(aSandbox.Value()));
    if (!IsWorkerDebuggerSandbox(sandbox)) {
      aRv.Throw(NS_ERROR_INVALID_ARG);
      return;
    }

    ar.emplace(aCx, sandbox);
  }

  nsTArray<nsString> urls;
  urls.AppendElement(aURL);
  workerinternals::Load(mWorkerPrivate, urls, DebuggerScript, aRv);
}

void
WorkerDebuggerGlobalScope::EnterEventLoop()
{
  mWorkerPrivate->EnterDebuggerEventLoop();
}

void
WorkerDebuggerGlobalScope::LeaveEventLoop()
{
  mWorkerPrivate->LeaveDebuggerEventLoop();
}

void
WorkerDebuggerGlobalScope::PostMessage(const nsAString& aMessage)
{
  mWorkerPrivate->PostMessageToDebugger(aMessage);
}

void
WorkerDebuggerGlobalScope::SetImmediate(Function& aHandler, ErrorResult& aRv)
{
  mWorkerPrivate->SetDebuggerImmediate(aHandler, aRv);
}

void
WorkerDebuggerGlobalScope::ReportError(JSContext* aCx,
                                       const nsAString& aMessage)
{
  JS::AutoFilename chars;
  uint32_t lineno = 0;
  JS::DescribeScriptedCaller(aCx, &chars, &lineno);
  nsString filename(NS_ConvertUTF8toUTF16(chars.get()));
  mWorkerPrivate->ReportErrorToDebugger(filename, lineno, aMessage);
}

void
WorkerDebuggerGlobalScope::RetrieveConsoleEvents(JSContext* aCx,
                                                 nsTArray<JS::Value>& aEvents,
                                                 ErrorResult& aRv)
{
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

void
WorkerDebuggerGlobalScope::SetConsoleEventHandler(JSContext* aCx,
                                                  AnyCallback* aHandler,
                                                  ErrorResult& aRv)
{
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

already_AddRefed<Console>
WorkerDebuggerGlobalScope::GetConsole(ErrorResult& aRv)
{
  mWorkerPrivate->AssertIsOnWorkerThread();

  // Debugger console has its own console object.
  if (!mConsole) {
    mConsole = Console::Create(mWorkerPrivate->GetJSContext(),
                               nullptr, aRv);
    if (NS_WARN_IF(aRv.Failed())) {
      return nullptr;
    }
  }

  RefPtr<Console> console = mConsole;
  return console.forget();
}

void
WorkerDebuggerGlobalScope::Dump(JSContext* aCx,
                                const Optional<nsAString>& aString) const
{
  WorkerGlobalScope* scope = mWorkerPrivate->GetOrCreateGlobalScope(aCx);
  if (scope) {
    scope->Dump(aString);
  }
}

nsresult
WorkerDebuggerGlobalScope::Dispatch(TaskCategory aCategory,
                                    already_AddRefed<nsIRunnable>&& aRunnable)
{
  return EventTargetFor(aCategory)->Dispatch(std::move(aRunnable),
                                             NS_DISPATCH_NORMAL);
}

nsISerialEventTarget*
WorkerDebuggerGlobalScope::EventTargetFor(TaskCategory aCategory) const
{
  return mSerialEventTarget;
}

AbstractThread*
WorkerDebuggerGlobalScope::AbstractMainThreadFor(TaskCategory aCategory)
{
  MOZ_CRASH("AbstractMainThreadFor not supported for workers.");
}

bool
IsWorkerGlobal(JSObject* object)
{
  return IS_INSTANCE_OF(WorkerGlobalScope, object);
}

bool
IsWorkerDebuggerGlobal(JSObject* object)
{
  return IS_INSTANCE_OF(WorkerDebuggerGlobalScope, object);
}

bool
IsWorkerDebuggerSandbox(JSObject* object)
{
  return SimpleGlobalObject::SimpleGlobalType(object) ==
    SimpleGlobalObject::GlobalType::WorkerDebuggerSandbox;
}

} // dom namespace
} // mozilla namespace
