/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workerscope_h__
#define mozilla_dom_workerscope_h__

#include "mozilla/dom/WorkerCommon.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/dom/DOMPrefs.h"
#include "mozilla/dom/Headers.h"
#include "mozilla/dom/RequestBinding.h"
#include "nsWeakReference.h"
#include "mozilla/dom/ImageBitmapSource.h"

#ifdef XP_WIN
#undef PostMessage
#endif

namespace mozilla {
namespace dom {

class AnyCallback;
struct ChannelPixelLayout;
class ClientInfo;
class Clients;
class ClientState;
class Console;
class Crypto;
class Function;
class IDBFactory;
enum class ImageBitmapFormat : uint8_t;
class Performance;
class Promise;
class RequestOrUSVString;
class WorkerLocation;
class WorkerNavigator;
class WorkerPrivate;
enum class CallerType : uint32_t;

namespace cache {

class CacheStorage;

} // namespace cache

class WorkerGlobalScope : public DOMEventTargetHelper,
                          public nsIGlobalObject,
                          public nsSupportsWeakReference
{
  typedef mozilla::dom::IDBFactory IDBFactory;

  RefPtr<Console> mConsole;
  RefPtr<Crypto> mCrypto;
  RefPtr<WorkerLocation> mLocation;
  RefPtr<WorkerNavigator> mNavigator;
  RefPtr<Performance> mPerformance;
  RefPtr<IDBFactory> mIndexedDB;
  RefPtr<cache::CacheStorage> mCacheStorage;
  nsCOMPtr<nsISerialEventTarget> mSerialEventTarget;

  uint32_t mWindowInteractionsAllowed;

protected:
  WorkerPrivate* mWorkerPrivate;

  explicit WorkerGlobalScope(WorkerPrivate* aWorkerPrivate);
  virtual ~WorkerGlobalScope();

public:
  virtual JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  virtual bool
  WrapGlobalObject(JSContext* aCx, JS::MutableHandle<JSObject*> aReflector) = 0;

  virtual JSObject*
  GetGlobalJSObject(void) override
  {
    return GetWrapper();
  }

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(WorkerGlobalScope,
                                                         DOMEventTargetHelper)

  WorkerGlobalScope*
  Self()
  {
    return this;
  }

  void
  NoteTerminating();

  already_AddRefed<Console>
  GetConsole(ErrorResult& aRv);

  Console*
  GetConsoleIfExists() const
  {
    return mConsole;
  }

  Crypto*
  GetCrypto(ErrorResult& aError);

  already_AddRefed<WorkerLocation>
  Location();

  already_AddRefed<WorkerNavigator>
  Navigator();

  already_AddRefed<WorkerNavigator>
  GetExistingNavigator() const;

  OnErrorEventHandlerNonNull*
  GetOnerror();
  void
  SetOnerror(OnErrorEventHandlerNonNull* aHandler);

  void
  ImportScripts(const Sequence<nsString>& aScriptURLs, ErrorResult& aRv);

  int32_t
  SetTimeout(JSContext* aCx, Function& aHandler, const int32_t aTimeout,
             const Sequence<JS::Value>& aArguments, ErrorResult& aRv);
  int32_t
  SetTimeout(JSContext* aCx, const nsAString& aHandler, const int32_t aTimeout,
             const Sequence<JS::Value>& /* unused */, ErrorResult& aRv);
  void
  ClearTimeout(int32_t aHandle);
  int32_t
  SetInterval(JSContext* aCx, Function& aHandler,
              const Optional<int32_t>& aTimeout,
              const Sequence<JS::Value>& aArguments, ErrorResult& aRv);
  int32_t
  SetInterval(JSContext* aCx, const nsAString& aHandler,
              const Optional<int32_t>& aTimeout,
              const Sequence<JS::Value>& /* unused */, ErrorResult& aRv);
  void
  ClearInterval(int32_t aHandle);

  void
  GetOrigin(nsAString& aOrigin) const;

  void
  Atob(const nsAString& aAtob, nsAString& aOutput, ErrorResult& aRv) const;
  void
  Btoa(const nsAString& aBtoa, nsAString& aOutput, ErrorResult& aRv) const;

  IMPL_EVENT_HANDLER(online)
  IMPL_EVENT_HANDLER(offline)

  void
  Dump(const Optional<nsAString>& aString) const;

  Performance* GetPerformance();

  Performance* GetPerformanceIfExists() const
  {
    return mPerformance;
  }

  static bool IsInAutomation(JSContext* aCx, JSObject* /* unused */);
  void GetJSTestingFunctions(JSContext* aCx,
                             JS::MutableHandle<JSObject*> aFunctions,
                             ErrorResult& aRv);

  already_AddRefed<Promise>
  Fetch(const RequestOrUSVString& aInput, const RequestInit& aInit,
        CallerType aCallerType, ErrorResult& aRv);

  already_AddRefed<IDBFactory>
  GetIndexedDB(ErrorResult& aErrorResult);

  already_AddRefed<cache::CacheStorage>
  GetCaches(ErrorResult& aRv);

  bool IsSecureContext() const;

  already_AddRefed<Promise>
  CreateImageBitmap(JSContext* aCx,
                    const ImageBitmapSource& aImage, ErrorResult& aRv);

  already_AddRefed<Promise>
  CreateImageBitmap(JSContext* aCx,
                    const ImageBitmapSource& aImage,
                    int32_t aSx, int32_t aSy, int32_t aSw, int32_t aSh,
                    ErrorResult& aRv);

  already_AddRefed<mozilla::dom::Promise>
  CreateImageBitmap(JSContext* aCx,
                    const ImageBitmapSource& aImage,
                    int32_t aOffset, int32_t aLength,
                    mozilla::dom::ImageBitmapFormat aFormat,
                    const mozilla::dom::Sequence<mozilla::dom::ChannelPixelLayout>& aLayout,
                    mozilla::ErrorResult& aRv);

  bool
  WindowInteractionAllowed() const
  {
    return mWindowInteractionsAllowed > 0;
  }

  void
  AllowWindowInteraction()
  {
    mWindowInteractionsAllowed++;
  }

  void
  ConsumeWindowInteraction()
  {
    MOZ_ASSERT(mWindowInteractionsAllowed > 0);
    mWindowInteractionsAllowed--;
  }

  // Override DispatchTrait API to target the worker thread.  Dispatch may
  // return failure if the worker thread is not alive.
  nsresult
  Dispatch(TaskCategory aCategory,
           already_AddRefed<nsIRunnable>&& aRunnable) override;

  nsISerialEventTarget*
  EventTargetFor(TaskCategory aCategory) const override;

  AbstractThread*
  AbstractMainThreadFor(TaskCategory aCategory) override;

  Maybe<ClientInfo>
  GetClientInfo() const override;

  Maybe<ClientState>
  GetClientState() const;

  Maybe<ServiceWorkerDescriptor>
  GetController() const override;

  RefPtr<mozilla::dom::ServiceWorkerRegistration>
  GetOrCreateServiceWorkerRegistration(const ServiceWorkerRegistrationDescriptor& aDescriptor) override;
};

class DedicatedWorkerGlobalScope final : public WorkerGlobalScope
{
  const nsString mName;

  ~DedicatedWorkerGlobalScope() { }

public:
  DedicatedWorkerGlobalScope(WorkerPrivate* aWorkerPrivate,
                             const nsString& aName);

  virtual bool
  WrapGlobalObject(JSContext* aCx,
                   JS::MutableHandle<JSObject*> aReflector) override;

  void GetName(DOMString& aName) const
  {
    aName.AsAString() = mName;
  }

  void
  PostMessage(JSContext* aCx, JS::Handle<JS::Value> aMessage,
              const Sequence<JSObject*>& aTransferable,
              ErrorResult& aRv);

  void
  Close();

  IMPL_EVENT_HANDLER(message)
  IMPL_EVENT_HANDLER(messageerror)
};

class SharedWorkerGlobalScope final : public WorkerGlobalScope
{
  const nsString mName;

  ~SharedWorkerGlobalScope() { }

public:
  SharedWorkerGlobalScope(WorkerPrivate* aWorkerPrivate,
                          const nsString& aName);

  virtual bool
  WrapGlobalObject(JSContext* aCx,
                   JS::MutableHandle<JSObject*> aReflector) override;

  void GetName(DOMString& aName) const
  {
    aName.AsAString() = mName;
  }

  void
  Close();

  IMPL_EVENT_HANDLER(connect)
};

class ServiceWorkerGlobalScope final : public WorkerGlobalScope
{
  const nsString mScope;
  RefPtr<Clients> mClients;
  RefPtr<ServiceWorkerRegistration> mRegistration;

  ~ServiceWorkerGlobalScope();

public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(ServiceWorkerGlobalScope,
                                           WorkerGlobalScope)
  IMPL_EVENT_HANDLER(notificationclick)
  IMPL_EVENT_HANDLER(notificationclose)

  ServiceWorkerGlobalScope(WorkerPrivate* aWorkerPrivate,
                           const ServiceWorkerRegistrationDescriptor& aRegistrationDescriptor);

  virtual bool
  WrapGlobalObject(JSContext* aCx,
                   JS::MutableHandle<JSObject*> aReflector) override;

  void
  GetScope(nsString& aScope) const
  {
    aScope = mScope;
  }

  already_AddRefed<Clients>
  GetClients();

  ServiceWorkerRegistration*
  Registration();

  already_AddRefed<Promise>
  SkipWaiting(ErrorResult& aRv);

  IMPL_EVENT_HANDLER(activate)
  IMPL_EVENT_HANDLER(install)
  IMPL_EVENT_HANDLER(message)

  IMPL_EVENT_HANDLER(push)
  IMPL_EVENT_HANDLER(pushsubscriptionchange)

  EventHandlerNonNull*
  GetOnfetch();

  void
  SetOnfetch(mozilla::dom::EventHandlerNonNull* aCallback);

  // We only need to override the string version of EventListenerAdded, because
  // the atom version should never be called on workers.  Until bug 1450167 is
  // fixed, at least.
  using DOMEventTargetHelper::EventListenerAdded;
  void EventListenerAdded(const nsAString& aType) override;
};

class WorkerDebuggerGlobalScope final : public DOMEventTargetHelper,
                                        public nsIGlobalObject
{
  WorkerPrivate* mWorkerPrivate;
  RefPtr<Console> mConsole;
  nsCOMPtr<nsISerialEventTarget> mSerialEventTarget;

public:
  explicit WorkerDebuggerGlobalScope(WorkerPrivate* aWorkerPrivate);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(WorkerDebuggerGlobalScope,
                                                         DOMEventTargetHelper)

  virtual JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override
  {
    MOZ_CRASH("Shouldn't get here!");
  }

  virtual bool
  WrapGlobalObject(JSContext* aCx,
                   JS::MutableHandle<JSObject*> aReflector);

  virtual JSObject*
  GetGlobalJSObject(void) override
  {
    return GetWrapper();
  }

  void
  GetGlobal(JSContext* aCx, JS::MutableHandle<JSObject*> aGlobal,
            ErrorResult& aRv);

  void
  CreateSandbox(JSContext* aCx, const nsAString& aName,
                JS::Handle<JSObject*> aPrototype,
                JS::MutableHandle<JSObject*> aResult,
                ErrorResult& aRv);

  void
  LoadSubScript(JSContext* aCx, const nsAString& aURL,
                const Optional<JS::Handle<JSObject*>>& aSandbox,
                ErrorResult& aRv);

  void
  EnterEventLoop();

  void
  LeaveEventLoop();

  void
  PostMessage(const nsAString& aMessage);

  IMPL_EVENT_HANDLER(message)
  IMPL_EVENT_HANDLER(messageerror)

  void
  SetImmediate(Function& aHandler, ErrorResult& aRv);

  void
  ReportError(JSContext* aCx, const nsAString& aMessage);

  void
  RetrieveConsoleEvents(JSContext* aCx, nsTArray<JS::Value>& aEvents,
                        ErrorResult& aRv);

  void
  SetConsoleEventHandler(JSContext* aCx, AnyCallback* aHandler,
                         ErrorResult& aRv);

  already_AddRefed<Console>
  GetConsole(ErrorResult& aRv);

  Console*
  GetConsoleIfExists() const
  {
    return mConsole;
  }

  void
  Dump(JSContext* aCx, const Optional<nsAString>& aString) const;

  // Override DispatchTrait API to target the worker thread.  Dispatch may
  // return failure if the worker thread is not alive.
  nsresult
  Dispatch(TaskCategory aCategory,
           already_AddRefed<nsIRunnable>&& aRunnable) override;

  nsISerialEventTarget*
  EventTargetFor(TaskCategory aCategory) const override;

  AbstractThread*
  AbstractMainThreadFor(TaskCategory aCategory) override;

private:
  virtual ~WorkerDebuggerGlobalScope();
};

} // namespace dom
} // namespace mozilla

inline nsISupports*
ToSupports(mozilla::dom::WorkerGlobalScope* aScope)
{
  return static_cast<mozilla::dom::EventTarget*>(aScope);
}

#endif /* mozilla_dom_workerscope_h__ */
