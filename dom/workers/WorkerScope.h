/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workerscope_h__
#define mozilla_dom_workerscope_h__

#include "js/TypeDecls.h"
#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/Maybe.h"
#include "mozilla/NotNull.h"
#include "mozilla/RefPtr.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/dom/ImageBitmapSource.h"
#include "mozilla/dom/SafeRefPtr.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIGlobalObject.h"
#include "nsISupports.h"
#include "nsWeakReference.h"

#ifdef XP_WIN
#  undef PostMessage
#endif

class nsAtom;
class nsISerialEventTarget;

namespace mozilla {
class ErrorResult;

namespace extensions {

class ExtensionBrowser;

}  // namespace extensions

namespace dom {

class AnyCallback;
enum class CallerType : uint32_t;
class ClientInfo;
class ClientSource;
class Clients;
class Console;
class Crypto;
class DOMString;
class DebuggerNotificationManager;
enum class EventCallbackDebuggerNotificationType : uint8_t;
class EventHandlerNonNull;
class Function;
class IDBFactory;
class OnErrorEventHandlerNonNull;
template <typename T>
class Optional;
class Performance;
struct PostMessageOptions;
class Promise;
class RequestOrUSVString;
template <typename T>
class Sequence;
class ServiceWorkerDescriptor;
class ServiceWorkerRegistration;
class ServiceWorkerRegistrationDescriptor;
class WorkerLocation;
class WorkerNavigator;
class WorkerPrivate;
struct RequestInit;

namespace cache {

class CacheStorage;

}  // namespace cache

class WorkerGlobalScopeBase : public DOMEventTargetHelper,
                              public nsIGlobalObject {
  friend class WorkerPrivate;

 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(WorkerGlobalScopeBase,
                                                         DOMEventTargetHelper)

  WorkerGlobalScopeBase(NotNull<WorkerPrivate*> aWorkerPrivate,
                        UniquePtr<ClientSource> aClientSource);

  virtual bool WrapGlobalObject(JSContext* aCx,
                                JS::MutableHandle<JSObject*> aReflector) = 0;

  // EventTarget implementation
  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) final {
    MOZ_CRASH("WrapObject not supported; use WrapGlobalObject.");
  }

  // nsIGlobalObject implementation
  JSObject* GetGlobalJSObject() final;

  JSObject* GetGlobalJSObjectPreserveColor() const final;

  bool IsSharedMemoryAllowed() const final;

  Maybe<ClientInfo> GetClientInfo() const final;

  Maybe<ServiceWorkerDescriptor> GetController() const final;

  virtual void Control(const ServiceWorkerDescriptor& aServiceWorker);

  // DispatcherTrait implementation
  nsresult Dispatch(TaskCategory aCategory,
                    already_AddRefed<nsIRunnable>&& aRunnable) final;

  nsISerialEventTarget* EventTargetFor(TaskCategory) const final;

  AbstractThread* AbstractMainThreadFor(TaskCategory) final {
    MOZ_CRASH("AbstractMainThreadFor not supported for workers.");
  }

  MOZ_CAN_RUN_SCRIPT
  void ReportError(JSContext* aCx, JS::Handle<JS::Value> aError,
                   CallerType aCallerType, ErrorResult& aRv);

  // atob, btoa, and dump are declared (separately) by both WorkerGlobalScope
  // and WorkerDebuggerGlobalScope WebIDL interfaces
  void Atob(const nsAString& aAtob, nsAString& aOut, ErrorResult& aRv) const;

  void Btoa(const nsAString& aBtoa, nsAString& aOut, ErrorResult& aRv) const;

  already_AddRefed<Console> GetConsole(ErrorResult& aRv);

  Console* GetConsoleIfExists() const { return mConsole; }

  uint64_t WindowID() const;

  void NoteTerminating() { StartDying(); }

  ClientSource& MutableClientSourceRef() const { return *mClientSource; }

  // WorkerPrivate wants to be able to forbid script when its state machine
  // demands it.
  void WorkerPrivateSaysForbidScript() { StartForbiddingScript(); }
  void WorkerPrivateSaysAllowScript() { StopForbiddingScript(); }

 protected:
  ~WorkerGlobalScopeBase();

  const NotNull<WorkerPrivate*> mWorkerPrivate;

 private:
  RefPtr<Console> mConsole;
  const UniquePtr<ClientSource> mClientSource;
  nsCOMPtr<nsISerialEventTarget> mSerialEventTarget;
};

namespace workerinternals {

class NamedWorkerGlobalScopeMixin {
 public:
  explicit NamedWorkerGlobalScopeMixin(const nsAString& aName) : mName(aName) {}

  void GetName(DOMString& aName) const;

 protected:
  ~NamedWorkerGlobalScopeMixin() = default;

 private:
  const nsString mName;
};

}  // namespace workerinternals

class WorkerGlobalScope : public WorkerGlobalScopeBase,
                          public nsSupportsWeakReference {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(WorkerGlobalScope,
                                           WorkerGlobalScopeBase)

  using WorkerGlobalScopeBase::WorkerGlobalScopeBase;

  // nsIGlobalObject implementation
  RefPtr<ServiceWorkerRegistration> GetServiceWorkerRegistration(
      const ServiceWorkerRegistrationDescriptor& aDescriptor) const final;

  RefPtr<ServiceWorkerRegistration> GetOrCreateServiceWorkerRegistration(
      const ServiceWorkerRegistrationDescriptor& aDescriptor) final;

  DebuggerNotificationManager* GetOrCreateDebuggerNotificationManager() final;

  DebuggerNotificationManager* GetExistingDebuggerNotificationManager() final;

  Maybe<EventCallbackDebuggerNotificationType> GetDebuggerNotificationType()
      const final;

  // WorkerGlobalScope WebIDL implementation
  WorkerGlobalScope* Self() { return this; }

  already_AddRefed<WorkerLocation> Location();

  already_AddRefed<WorkerNavigator> Navigator();

  already_AddRefed<WorkerNavigator> GetExistingNavigator() const;

  void ImportScripts(JSContext* aCx, const Sequence<nsString>& aScriptURLs,
                     ErrorResult& aRv);

  OnErrorEventHandlerNonNull* GetOnerror();

  void SetOnerror(OnErrorEventHandlerNonNull* aHandler);

  IMPL_EVENT_HANDLER(languagechange)
  IMPL_EVENT_HANDLER(offline)
  IMPL_EVENT_HANDLER(online)
  IMPL_EVENT_HANDLER(rejectionhandled)
  IMPL_EVENT_HANDLER(unhandledrejection)

  void Dump(const Optional<nsAString>& aString) const;

  Performance* GetPerformance();

  Performance* GetPerformanceIfExists() const { return mPerformance; }

  static bool IsInAutomation(JSContext* aCx, JSObject*);

  void GetJSTestingFunctions(JSContext* aCx,
                             JS::MutableHandle<JSObject*> aFunctions,
                             ErrorResult& aRv);

  // GlobalCrypto WebIDL implementation
  Crypto* GetCrypto(ErrorResult& aError);

  // WindowOrWorkerGlobalScope WebIDL implementation
  void GetOrigin(nsAString& aOrigin) const;

  bool CrossOriginIsolated() const final;

  MOZ_CAN_RUN_SCRIPT
  int32_t SetTimeout(JSContext* aCx, Function& aHandler, int32_t aTimeout,
                     const Sequence<JS::Value>& aArguments, ErrorResult& aRv);

  MOZ_CAN_RUN_SCRIPT
  int32_t SetTimeout(JSContext* aCx, const nsAString& aHandler,
                     int32_t aTimeout, const Sequence<JS::Value>&,
                     ErrorResult& aRv);

  MOZ_CAN_RUN_SCRIPT
  void ClearTimeout(int32_t aHandle);

  MOZ_CAN_RUN_SCRIPT
  int32_t SetInterval(JSContext* aCx, Function& aHandler, int32_t aTimeout,
                      const Sequence<JS::Value>& aArguments, ErrorResult& aRv);
  MOZ_CAN_RUN_SCRIPT
  int32_t SetInterval(JSContext* aCx, const nsAString& aHandler,
                      int32_t aTimeout, const Sequence<JS::Value>&,
                      ErrorResult& aRv);

  MOZ_CAN_RUN_SCRIPT
  void ClearInterval(int32_t aHandle);

  already_AddRefed<Promise> CreateImageBitmap(const ImageBitmapSource& aImage,
                                              ErrorResult& aRv);

  already_AddRefed<Promise> CreateImageBitmap(const ImageBitmapSource& aImage,
                                              int32_t aSx, int32_t aSy,
                                              int32_t aSw, int32_t aSh,
                                              ErrorResult& aRv);

  already_AddRefed<Promise> Fetch(const RequestOrUSVString& aInput,
                                  const RequestInit& aInit,
                                  CallerType aCallerType, ErrorResult& aRv);

  bool IsSecureContext() const;

  already_AddRefed<IDBFactory> GetIndexedDB(ErrorResult& aErrorResult);

  already_AddRefed<cache::CacheStorage> GetCaches(ErrorResult& aRv);

  bool WindowInteractionAllowed() const;

  void AllowWindowInteraction();

  void ConsumeWindowInteraction();

  void StorageAccessPermissionGranted();

 protected:
  ~WorkerGlobalScope();

 private:
  MOZ_CAN_RUN_SCRIPT
  int32_t SetTimeoutOrInterval(JSContext* aCx, Function& aHandler,
                               int32_t aTimeout,
                               const Sequence<JS::Value>& aArguments,
                               bool aIsInterval, ErrorResult& aRv);

  MOZ_CAN_RUN_SCRIPT
  int32_t SetTimeoutOrInterval(JSContext* aCx, const nsAString& aHandler,
                               int32_t aTimeout, bool aIsInterval,
                               ErrorResult& aRv);

  RefPtr<Crypto> mCrypto;
  RefPtr<WorkerLocation> mLocation;
  RefPtr<WorkerNavigator> mNavigator;
  RefPtr<Performance> mPerformance;
  RefPtr<IDBFactory> mIndexedDB;
  RefPtr<cache::CacheStorage> mCacheStorage;
  RefPtr<DebuggerNotificationManager> mDebuggerNotificationManager;
  uint32_t mWindowInteractionsAllowed = 0;
};

class DedicatedWorkerGlobalScope final
    : public WorkerGlobalScope,
      public workerinternals::NamedWorkerGlobalScopeMixin {
 public:
  DedicatedWorkerGlobalScope(NotNull<WorkerPrivate*> aWorkerPrivate,
                             UniquePtr<ClientSource> aClientSource,
                             const nsString& aName);

  bool WrapGlobalObject(JSContext* aCx,
                        JS::MutableHandle<JSObject*> aReflector) override;

  void PostMessage(JSContext* aCx, JS::Handle<JS::Value> aMessage,
                   const Sequence<JSObject*>& aTransferable, ErrorResult& aRv);

  void PostMessage(JSContext* aCx, JS::Handle<JS::Value> aMessage,
                   const PostMessageOptions& aOptions, ErrorResult& aRv);

  void Close();

  IMPL_EVENT_HANDLER(message)
  IMPL_EVENT_HANDLER(messageerror)

 private:
  ~DedicatedWorkerGlobalScope() = default;
};

class SharedWorkerGlobalScope final
    : public WorkerGlobalScope,
      public workerinternals::NamedWorkerGlobalScopeMixin {
 public:
  SharedWorkerGlobalScope(NotNull<WorkerPrivate*> aWorkerPrivate,
                          UniquePtr<ClientSource> aClientSource,
                          const nsString& aName);

  bool WrapGlobalObject(JSContext* aCx,
                        JS::MutableHandle<JSObject*> aReflector) override;

  void Close();

  IMPL_EVENT_HANDLER(connect)

 private:
  ~SharedWorkerGlobalScope() = default;
};

class ServiceWorkerGlobalScope final : public WorkerGlobalScope {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(ServiceWorkerGlobalScope,
                                           WorkerGlobalScope)

  ServiceWorkerGlobalScope(
      NotNull<WorkerPrivate*> aWorkerPrivate,
      UniquePtr<ClientSource> aClientSource,
      const ServiceWorkerRegistrationDescriptor& aRegistrationDescriptor);

  bool WrapGlobalObject(JSContext* aCx,
                        JS::MutableHandle<JSObject*> aReflector) override;

  already_AddRefed<Clients> GetClients();

  ServiceWorkerRegistration* Registration();

  already_AddRefed<Promise> SkipWaiting(ErrorResult& aRv);

  SafeRefPtr<extensions::ExtensionBrowser> AcquireExtensionBrowser();

  IMPL_EVENT_HANDLER(install)
  IMPL_EVENT_HANDLER(activate)

  EventHandlerNonNull* GetOnfetch();

  void SetOnfetch(EventHandlerNonNull* aCallback);

  void EventListenerAdded(nsAtom* aType) override;

  IMPL_EVENT_HANDLER(message)
  IMPL_EVENT_HANDLER(messageerror)

  IMPL_EVENT_HANDLER(notificationclick)
  IMPL_EVENT_HANDLER(notificationclose)

  IMPL_EVENT_HANDLER(push)
  IMPL_EVENT_HANDLER(pushsubscriptionchange)

 private:
  ~ServiceWorkerGlobalScope();

  void NoteFetchHandlerWasAdded() const;

  RefPtr<Clients> mClients;
  const nsString mScope;
  RefPtr<ServiceWorkerRegistration> mRegistration;
  SafeRefPtr<extensions::ExtensionBrowser> mExtensionBrowser;
};

class WorkerDebuggerGlobalScope final : public WorkerGlobalScopeBase {
 public:
  using WorkerGlobalScopeBase::WorkerGlobalScopeBase;

  bool WrapGlobalObject(JSContext* aCx,
                        JS::MutableHandle<JSObject*> aReflector) override;

  void Control(const ServiceWorkerDescriptor& aServiceWorker) override {
    MOZ_CRASH("Can't control debugger workers.");
  }

  void GetGlobal(JSContext* aCx, JS::MutableHandle<JSObject*> aGlobal,
                 ErrorResult& aRv);

  void CreateSandbox(JSContext* aCx, const nsAString& aName,
                     JS::Handle<JSObject*> aPrototype,
                     JS::MutableHandle<JSObject*> aResult, ErrorResult& aRv);

  void LoadSubScript(JSContext* aCx, const nsAString& aUrl,
                     const Optional<JS::Handle<JSObject*>>& aSandbox,
                     ErrorResult& aRv);

  MOZ_CAN_RUN_SCRIPT void EnterEventLoop();

  void LeaveEventLoop();

  void PostMessage(const nsAString& aMessage);

  void SetImmediate(Function& aHandler, ErrorResult& aRv);

  void ReportError(JSContext* aCx, const nsAString& aMessage);

  void RetrieveConsoleEvents(JSContext* aCx, nsTArray<JS::Value>& aEvents,
                             ErrorResult& aRv);

  void SetConsoleEventHandler(JSContext* aCx, AnyCallback* aHandler,
                              ErrorResult& aRv);

  void Dump(JSContext* aCx, const Optional<nsAString>& aString) const;

  IMPL_EVENT_HANDLER(message)
  IMPL_EVENT_HANDLER(messageerror)

 private:
  ~WorkerDebuggerGlobalScope() = default;
};

}  // namespace dom
}  // namespace mozilla

inline nsISupports* ToSupports(mozilla::dom::WorkerGlobalScope* aScope) {
  return static_cast<mozilla::dom::EventTarget*>(aScope);
}

#endif /* mozilla_dom_workerscope_h__ */
