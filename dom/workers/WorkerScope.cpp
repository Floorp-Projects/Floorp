/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WorkerScope.h"

#include "jsapi.h"
#include "mozilla/EventListenerManager.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/Console.h"
#include "mozilla/dom/DedicatedWorkerGlobalScopeBinding.h"
#include "mozilla/dom/Fetch.h"
#include "mozilla/dom/FunctionBinding.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/ServiceWorkerGlobalScopeBinding.h"
#include "mozilla/dom/SharedWorkerGlobalScopeBinding.h"
#include "mozilla/dom/WorkerDebuggerGlobalScopeBinding.h"
#include "mozilla/dom/WorkerGlobalScopeBinding.h"
#include "mozilla/dom/cache/CacheStorage.h"
#include "mozilla/dom/indexedDB/IDBFactory.h"
#include "mozilla/Services.h"
#include "nsServiceManagerUtils.h"

#include "nsIDocument.h"
#include "nsIServiceWorkerManager.h"

#ifdef ANDROID
#include <android/log.h>
#endif

#include "Location.h"
#include "Navigator.h"
#include "Principal.h"
#include "RuntimeService.h"
#include "ScriptLoader.h"
#include "WorkerPrivate.h"
#include "WorkerRunnable.h"
#include "Performance.h"
#include "ServiceWorkerClients.h"
#include "ServiceWorkerRegistration.h"

using namespace mozilla;
using namespace mozilla::dom;
USING_WORKERS_NAMESPACE

using mozilla::dom::cache::CacheStorage;
using mozilla::dom::indexedDB::IDBFactory;
using mozilla::ipc::PrincipalInfo;

BEGIN_WORKERS_NAMESPACE

WorkerGlobalScope::WorkerGlobalScope(WorkerPrivate* aWorkerPrivate)
: mWorkerPrivate(aWorkerPrivate)
{
  mWorkerPrivate->AssertIsOnWorkerThread();
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
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPerformance)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mLocation)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mNavigator)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mIndexedDB)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mCacheStorage)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(WorkerGlobalScope,
                                                DOMEventTargetHelper)
  tmp->mWorkerPrivate->AssertIsOnWorkerThread();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mConsole)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mPerformance)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mLocation)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mNavigator)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mIndexedDB)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mCacheStorage)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(WorkerGlobalScope,
                                               DOMEventTargetHelper)
  tmp->mWorkerPrivate->AssertIsOnWorkerThread();

  tmp->mWorkerPrivate->TraceTimeouts(aCallbacks, aClosure);
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

Console*
WorkerGlobalScope::GetConsole()
{
  mWorkerPrivate->AssertIsOnWorkerThread();

  if (!mConsole) {
    mConsole = new Console(nullptr);
  }

  return mConsole;
}

already_AddRefed<CacheStorage>
WorkerGlobalScope::GetCaches(ErrorResult& aRv)
{
  if (!mCacheStorage) {
    MOZ_ASSERT(mWorkerPrivate);
    mCacheStorage = CacheStorage::CreateOnWorker(cache::DEFAULT_NAMESPACE, this,
                                                 mWorkerPrivate, aRv);
  }

  nsRefPtr<CacheStorage> ref = mCacheStorage;
  return ref.forget();
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

  nsRefPtr<WorkerLocation> location = mLocation;
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

  nsRefPtr<WorkerNavigator> navigator = mNavigator;
  return navigator.forget();
}

already_AddRefed<WorkerNavigator>
WorkerGlobalScope::GetExistingNavigator() const
{
  mWorkerPrivate->AssertIsOnWorkerThread();

  nsRefPtr<WorkerNavigator> navigator = mNavigator;
  return navigator.forget();
}

void
WorkerGlobalScope::Close(JSContext* aCx, ErrorResult& aRv)
{
  mWorkerPrivate->AssertIsOnWorkerThread();

  if (mWorkerPrivate->IsServiceWorker()) {
    aRv.Throw(NS_ERROR_DOM_INVALID_ACCESS_ERR);
  } else {
    mWorkerPrivate->CloseInternal(aCx);
  }
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
WorkerGlobalScope::ImportScripts(JSContext* aCx,
                                 const Sequence<nsString>& aScriptURLs,
                                 ErrorResult& aRv)
{
  mWorkerPrivate->AssertIsOnWorkerThread();
  scriptloader::Load(aCx, mWorkerPrivate, aScriptURLs, WorkerScript, aRv);
}

int32_t
WorkerGlobalScope::SetTimeout(JSContext* aCx,
                              Function& aHandler,
                              const int32_t aTimeout,
                              const Sequence<JS::Value>& aArguments,
                              ErrorResult& aRv)
{
  mWorkerPrivate->AssertIsOnWorkerThread();
  return mWorkerPrivate->SetTimeout(aCx, &aHandler, EmptyString(), aTimeout,
                                    aArguments, false, aRv);
}

int32_t
WorkerGlobalScope::SetTimeout(JSContext* /* unused */,
                              const nsAString& aHandler,
                              const int32_t aTimeout,
                              const Sequence<JS::Value>& /* unused */,
                              ErrorResult& aRv)
{
  mWorkerPrivate->AssertIsOnWorkerThread();
  Sequence<JS::Value> dummy;
  return mWorkerPrivate->SetTimeout(GetCurrentThreadJSContext(), nullptr,
                                    aHandler, aTimeout, dummy, false, aRv);
}

void
WorkerGlobalScope::ClearTimeout(int32_t aHandle, ErrorResult& aRv)
{
  mWorkerPrivate->AssertIsOnWorkerThread();
  mWorkerPrivate->ClearTimeout(aHandle);
}

int32_t
WorkerGlobalScope::SetInterval(JSContext* aCx,
                               Function& aHandler,
                               const Optional<int32_t>& aTimeout,
                               const Sequence<JS::Value>& aArguments,
                               ErrorResult& aRv)
{
  mWorkerPrivate->AssertIsOnWorkerThread();

  bool isInterval = aTimeout.WasPassed();
  int32_t timeout = aTimeout.WasPassed() ? aTimeout.Value() : 0;

  return mWorkerPrivate->SetTimeout(aCx, &aHandler, EmptyString(), timeout,
                                    aArguments, isInterval, aRv);
}

int32_t
WorkerGlobalScope::SetInterval(JSContext* /* unused */,
                               const nsAString& aHandler,
                               const Optional<int32_t>& aTimeout,
                               const Sequence<JS::Value>& /* unused */,
                               ErrorResult& aRv)
{
  mWorkerPrivate->AssertIsOnWorkerThread();

  Sequence<JS::Value> dummy;

  bool isInterval = aTimeout.WasPassed();
  int32_t timeout = aTimeout.WasPassed() ? aTimeout.Value() : 0;

  return mWorkerPrivate->SetTimeout(GetCurrentThreadJSContext(), nullptr,
                                    aHandler, timeout, dummy, isInterval, aRv);
}

void
WorkerGlobalScope::ClearInterval(int32_t aHandle, ErrorResult& aRv)
{
  mWorkerPrivate->AssertIsOnWorkerThread();
  mWorkerPrivate->ClearTimeout(aHandle);
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

  if (!mWorkerPrivate->DumpEnabled()) {
    return;
  }

  NS_ConvertUTF16toUTF8 str(aString.Value());

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
    mPerformance = new Performance(mWorkerPrivate);
  }

  return mPerformance;
}

already_AddRefed<Promise>
WorkerGlobalScope::Fetch(const RequestOrUSVString& aInput,
                         const RequestInit& aInit, ErrorResult& aRv)
{
  return FetchRequest(this, aInput, aInit, aRv);
}

already_AddRefed<IDBFactory>
WorkerGlobalScope::GetIndexedDB(ErrorResult& aErrorResult)
{
  mWorkerPrivate->AssertIsOnWorkerThread();

  nsRefPtr<IDBFactory> indexedDB = mIndexedDB;

  if (!indexedDB) {
    if (!mWorkerPrivate->IsIndexedDBAllowed()) {
      NS_WARNING("IndexedDB is not allowed in this worker!");
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

DedicatedWorkerGlobalScope::DedicatedWorkerGlobalScope(WorkerPrivate* aWorkerPrivate)
: WorkerGlobalScope(aWorkerPrivate)
{
}

bool
DedicatedWorkerGlobalScope::WrapGlobalObject(JSContext* aCx,
                                             JS::MutableHandle<JSObject*> aReflector)
{
  mWorkerPrivate->AssertIsOnWorkerThread();
  MOZ_ASSERT(!mWorkerPrivate->IsSharedWorker());

  JS::CompartmentOptions options;
  mWorkerPrivate->CopyJSCompartmentOptions(options);

  return DedicatedWorkerGlobalScopeBinding_workers::Wrap(aCx, this, this,
                                                         options,
                                                         GetWorkerPrincipal(),
                                                         true, aReflector);
}

void
DedicatedWorkerGlobalScope::PostMessage(JSContext* aCx,
                                        JS::Handle<JS::Value> aMessage,
                                        const Optional<Sequence<JS::Value>>& aTransferable,
                                        ErrorResult& aRv)
{
  mWorkerPrivate->AssertIsOnWorkerThread();
  mWorkerPrivate->PostMessageToParent(aCx, aMessage, aTransferable, aRv);
}

SharedWorkerGlobalScope::SharedWorkerGlobalScope(WorkerPrivate* aWorkerPrivate,
                                                 const nsCString& aName)
: WorkerGlobalScope(aWorkerPrivate), mName(aName)
{
}

bool
SharedWorkerGlobalScope::WrapGlobalObject(JSContext* aCx,
                                          JS::MutableHandle<JSObject*> aReflector)
{
  mWorkerPrivate->AssertIsOnWorkerThread();
  MOZ_ASSERT(mWorkerPrivate->IsSharedWorker());

  JS::CompartmentOptions options;
  mWorkerPrivate->CopyJSCompartmentOptions(options);

  return SharedWorkerGlobalScopeBinding_workers::Wrap(aCx, this, this, options,
                                                      GetWorkerPrincipal(),
                                                      true, aReflector);
}

NS_IMPL_CYCLE_COLLECTION_INHERITED(ServiceWorkerGlobalScope, WorkerGlobalScope,
                                   mClients, mRegistration)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(ServiceWorkerGlobalScope)
NS_INTERFACE_MAP_END_INHERITING(WorkerGlobalScope)

NS_IMPL_ADDREF_INHERITED(ServiceWorkerGlobalScope, WorkerGlobalScope)
NS_IMPL_RELEASE_INHERITED(ServiceWorkerGlobalScope, WorkerGlobalScope)

ServiceWorkerGlobalScope::ServiceWorkerGlobalScope(WorkerPrivate* aWorkerPrivate,
                                                   const nsACString& aScope)
  : WorkerGlobalScope(aWorkerPrivate),
    mScope(NS_ConvertUTF8toUTF16(aScope))
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

  JS::CompartmentOptions options;
  mWorkerPrivate->CopyJSCompartmentOptions(options);

  return ServiceWorkerGlobalScopeBinding_workers::Wrap(aCx, this, this, options,
                                                       GetWorkerPrincipal(),
                                                       true, aReflector);
}

ServiceWorkerClients*
ServiceWorkerGlobalScope::Clients()
{
  if (!mClients) {
    mClients = new ServiceWorkerClients(this);
  }

  return mClients;
}

ServiceWorkerRegistrationWorkerThread*
ServiceWorkerGlobalScope::Registration()
{
  if (!mRegistration) {
    mRegistration =
      new ServiceWorkerRegistrationWorkerThread(mWorkerPrivate, mScope);
  }

  return mRegistration;
}

WorkerDebuggerGlobalScope::WorkerDebuggerGlobalScope(
                                                  WorkerPrivate* aWorkerPrivate)
: mWorkerPrivate(aWorkerPrivate)
{
  mWorkerPrivate->AssertIsOnWorkerThread();
}

WorkerDebuggerGlobalScope::~WorkerDebuggerGlobalScope()
{
  mWorkerPrivate->AssertIsOnWorkerThread();
}

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

  JS::CompartmentOptions options;
  mWorkerPrivate->CopyJSCompartmentOptions(options);

  return WorkerDebuggerGlobalScopeBinding::Wrap(aCx, this, this, options,
                                                GetWorkerPrincipal(), true,
                                                aReflector);
}

void
WorkerDebuggerGlobalScope::GetGlobal(JSContext* aCx,
                                     JS::MutableHandle<JSObject*> aGlobal)
{
  aGlobal.set(mWorkerPrivate->GetOrCreateGlobalScope(aCx)->GetWrapper());
}

class WorkerDebuggerSandboxPrivate : public nsIGlobalObject,
                                     public nsWrapperCache
{
public:
  explicit WorkerDebuggerSandboxPrivate(JSObject *global)
  {
    SetWrapper(global);
  }

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_AMBIGUOUS(WorkerDebuggerSandboxPrivate,
                                                         nsIGlobalObject)

  virtual JSObject *GetGlobalJSObject() override
  {
    return GetWrapper();
  }

  virtual JSObject* WrapObject(JSContext* cx,
                               JS::Handle<JSObject*> aGivenProto) override
  {
    MOZ_CRASH("WorkerDebuggerSandboxPrivate doesn't use DOM bindings!");
  }

private:
  virtual ~WorkerDebuggerSandboxPrivate()
  {
    ClearWrapper();
  }
};

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_0(WorkerDebuggerSandboxPrivate)
NS_IMPL_CYCLE_COLLECTING_ADDREF(WorkerDebuggerSandboxPrivate)
NS_IMPL_CYCLE_COLLECTING_RELEASE(WorkerDebuggerSandboxPrivate)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(WorkerDebuggerSandboxPrivate)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsIGlobalObject)
NS_INTERFACE_MAP_END

static bool
workerdebuggersandbox_enumerate(JSContext *cx, JS::Handle<JSObject *> obj)
{
  return JS_EnumerateStandardClasses(cx, obj);
}

static bool
workerdebuggersandbox_resolve(JSContext *cx, JS::Handle<JSObject *> obj,
                              JS::Handle<jsid> id, bool *resolvedp)
{
  return JS_ResolveStandardClass(cx, obj, id, resolvedp);
}

static bool
workerdebuggersandbox_convert(JSContext *cx, JS::Handle<JSObject *> obj,
                              JSType type, JS::MutableHandle<JS::Value> vp)
{
  if (type == JSTYPE_OBJECT) {
    vp.set(OBJECT_TO_JSVAL(obj));
    return true;
  }

  return JS::OrdinaryToPrimitive(cx, obj, type, vp);
}

static void
workerdebuggersandbox_finalize(js::FreeOp *fop, JSObject *obj)
{
  nsIGlobalObject *globalObject =
    static_cast<nsIGlobalObject *>(JS_GetPrivate(obj));
  NS_RELEASE(globalObject);
}

static void
workerdebuggersandbox_moved(JSObject *obj, const JSObject *old)
{
}

const js::Class workerdebuggersandbox_class = {
    "workerdebuggersandbox",
    JSCLASS_GLOBAL_FLAGS | JSCLASS_HAS_PRIVATE | JSCLASS_PRIVATE_IS_NSISUPPORTS,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    workerdebuggersandbox_enumerate,
    workerdebuggersandbox_resolve,
    nullptr, /* mayResolve */
    workerdebuggersandbox_convert,
    workerdebuggersandbox_finalize,
    nullptr,
    nullptr,
    nullptr,
    JS_GlobalObjectTraceHook,
    JS_NULL_CLASS_SPEC, {
      nullptr,
      nullptr,
      false,
      nullptr,
      workerdebuggersandbox_moved
    }, JS_NULL_OBJECT_OPS
};

void
WorkerDebuggerGlobalScope::CreateSandbox(JSContext* aCx, const nsAString& aName,
                                         JS::Handle<JSObject*> aPrototype,
                                         JS::MutableHandle<JSObject*> aResult)
{
  mWorkerPrivate->AssertIsOnWorkerThread();

  JS::CompartmentOptions options;
  options.setInvisibleToDebugger(true);

  JS::Rooted<JSObject*> sandbox(aCx,
    JS_NewGlobalObject(aCx, js::Jsvalify(&workerdebuggersandbox_class), nullptr,
                       JS::DontFireOnNewGlobalHook, options));
  if (!sandbox) {
    JS_ReportError(aCx, "Can't create sandbox!");
    aResult.set(nullptr);
    return;
  }

  {
    JSAutoCompartment ac(aCx, sandbox);

    JS::Rooted<JSObject*> prototype(aCx, aPrototype);
    if (!JS_WrapObject(aCx, &prototype)) {
      JS_ReportError(aCx, "Can't wrap sandbox prototype!");
      aResult.set(nullptr);
      return;
    }

    if (!JS_SetPrototype(aCx, sandbox, prototype)) {
      JS_ReportError(aCx, "Can't set sandbox prototype!");
      aResult.set(nullptr);
      return;
    }

    nsCOMPtr<nsIGlobalObject> globalObject =
      new WorkerDebuggerSandboxPrivate(sandbox);

    // Pass on ownership of globalObject to |sandbox|.
    JS_SetPrivate(sandbox, globalObject.forget().take());
  }

  JS_FireOnNewGlobalObject(aCx, sandbox);

  if (!JS_WrapObject(aCx, &sandbox)) {
    JS_ReportError(aCx, "Can't wrap sandbox!");
    aResult.set(nullptr);
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

  Maybe<JSAutoCompartment> ac;
  if (aSandbox.WasPassed()) {
    JS::Rooted<JSObject*> sandbox(aCx, js::CheckedUnwrap(aSandbox.Value()));
    if (!IsDebuggerSandbox(sandbox)) {
      aRv.Throw(NS_ERROR_INVALID_ARG);
      return;
    }

    ac.emplace(aCx, sandbox);
  }

  nsTArray<nsString> urls;
  urls.AppendElement(aURL);
  scriptloader::Load(aCx, mWorkerPrivate, urls, DebuggerScript, aRv);
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
WorkerDebuggerGlobalScope::SetImmediate(JSContext* aCx, Function& aHandler,
                                        ErrorResult& aRv)
{
  mWorkerPrivate->SetDebuggerImmediate(aCx, aHandler, aRv);
}

void
WorkerDebuggerGlobalScope::ReportError(JSContext* aCx,
                                       const nsAString& aMessage)
{
  JS::AutoFilename afn;
  uint32_t lineno = 0;
  JS::DescribeScriptedCaller(aCx, &afn, &lineno);
  nsString filename(NS_ConvertUTF8toUTF16(afn.get()));
  mWorkerPrivate->ReportErrorToDebugger(filename, lineno, aMessage);
}

void
WorkerDebuggerGlobalScope::Dump(JSContext* aCx,
                                const Optional<nsAString>& aString) const
{
  return mWorkerPrivate->GetOrCreateGlobalScope(aCx)->Dump(aString);
}

nsIGlobalObject*
GetGlobalObjectForGlobal(JSObject* global)
{
  nsIGlobalObject* globalObject = nullptr;
  UNWRAP_WORKER_OBJECT(WorkerGlobalScope, global, globalObject);

  if (!globalObject) {
    UNWRAP_OBJECT(WorkerDebuggerGlobalScope, global, globalObject);

    if (!globalObject) {
      MOZ_ASSERT(IsDebuggerSandbox(global));
      globalObject = static_cast<nsIGlobalObject *>(JS_GetPrivate(global));

      MOZ_ASSERT(globalObject);
    }
  }

  return globalObject;
}

bool
IsWorkerGlobal(JSObject* object)
{
  nsIGlobalObject* globalObject;
  return NS_SUCCEEDED(UNWRAP_WORKER_OBJECT(WorkerGlobalScope, object,
                                           globalObject)) && !!globalObject;
}

bool
IsDebuggerGlobal(JSObject* object)
{
  nsIGlobalObject* globalObject;
  return NS_SUCCEEDED(UNWRAP_OBJECT(WorkerDebuggerGlobalScope, object,
                                    globalObject)) && !!globalObject;
}

bool
IsDebuggerSandbox(JSObject* object)
{
  return js::GetObjectClass(object) == &workerdebuggersandbox_class;
}

bool
GetterOnlyJSNative(JSContext* aCx, unsigned aArgc, JS::Value* aVp)
{
  JS_ReportErrorNumber(aCx, js::GetErrorMessage, nullptr, JSMSG_GETTER_ONLY);
  return false;
}

END_WORKERS_NAMESPACE
