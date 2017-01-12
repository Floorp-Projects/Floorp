/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workerscope_h__
#define mozilla_dom_workerscope_h__

#include "Workers.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/dom/Headers.h"
#include "mozilla/dom/RequestBinding.h"
#include "nsWeakReference.h"
#include "mozilla/dom/ImageBitmapSource.h"

namespace mozilla {
namespace dom {

class AnyCallback;
struct ChannelPixelLayout;
class Console;
class Crypto;
class Function;
class IDBFactory;
enum class ImageBitmapFormat : uint8_t;
class Performance;
class Promise;
class RequestOrUSVString;
class ServiceWorkerRegistration;
class WorkerLocation;
class WorkerNavigator;

namespace cache {

class CacheStorage;

} // namespace cache

namespace workers {

class ServiceWorkerClients;
class WorkerPrivate;

} // namespace workers

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

  uint32_t mWindowInteractionsAllowed;

protected:
  typedef mozilla::dom::workers::WorkerPrivate WorkerPrivate;
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

  Console*
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

  void
  Close(JSContext* aCx, ErrorResult& aRv);

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
  Atob(const nsAString& aAtob, nsAString& aOutput, ErrorResult& aRv) const;
  void
  Btoa(const nsAString& aBtoa, nsAString& aOutput, ErrorResult& aRv) const;

  IMPL_EVENT_HANDLER(online)
  IMPL_EVENT_HANDLER(offline)

  void
  Dump(const Optional<nsAString>& aString) const;

  Performance* GetPerformance();

  already_AddRefed<Promise>
  Fetch(const RequestOrUSVString& aInput, const RequestInit& aInit, ErrorResult& aRv);

  already_AddRefed<IDBFactory>
  GetIndexedDB(ErrorResult& aErrorResult);

  already_AddRefed<cache::CacheStorage>
  GetCaches(ErrorResult& aRv);

  bool IsSecureContext() const;

  already_AddRefed<Promise>
  CreateImageBitmap(const ImageBitmapSource& aImage, ErrorResult& aRv);

  already_AddRefed<Promise>
  CreateImageBitmap(const ImageBitmapSource& aImage,
                    int32_t aSx, int32_t aSy, int32_t aSw, int32_t aSh,
                    ErrorResult& aRv);

  already_AddRefed<mozilla::dom::Promise>
  CreateImageBitmap(const ImageBitmapSource& aImage,
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
};

class DedicatedWorkerGlobalScope final : public WorkerGlobalScope
{
  ~DedicatedWorkerGlobalScope() { }

public:
  explicit DedicatedWorkerGlobalScope(WorkerPrivate* aWorkerPrivate);

  virtual bool
  WrapGlobalObject(JSContext* aCx,
                   JS::MutableHandle<JSObject*> aReflector) override;

  void
  PostMessage(JSContext* aCx, JS::Handle<JS::Value> aMessage,
              const Optional<Sequence<JS::Value>>& aTransferable,
              ErrorResult& aRv);

  IMPL_EVENT_HANDLER(message)
};

class SharedWorkerGlobalScope final : public WorkerGlobalScope
{
  const nsCString mName;

  ~SharedWorkerGlobalScope() { }

public:
  SharedWorkerGlobalScope(WorkerPrivate* aWorkerPrivate,
                          const nsCString& aName);

  virtual bool
  WrapGlobalObject(JSContext* aCx,
                   JS::MutableHandle<JSObject*> aReflector) override;

  void GetName(DOMString& aName) const
  {
    aName.AsAString() = NS_ConvertUTF8toUTF16(mName);
  }

  IMPL_EVENT_HANDLER(connect)
};

class ServiceWorkerGlobalScope final : public WorkerGlobalScope
{
  const nsString mScope;
  RefPtr<workers::ServiceWorkerClients> mClients;
  RefPtr<ServiceWorkerRegistration> mRegistration;

  ~ServiceWorkerGlobalScope();

public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(ServiceWorkerGlobalScope,
                                           WorkerGlobalScope)
  IMPL_EVENT_HANDLER(notificationclick)
  IMPL_EVENT_HANDLER(notificationclose)

  ServiceWorkerGlobalScope(WorkerPrivate* aWorkerPrivate, const nsACString& aScope);

  virtual bool
  WrapGlobalObject(JSContext* aCx,
                   JS::MutableHandle<JSObject*> aReflector) override;

  static bool
  OpenWindowEnabled(JSContext* aCx, JSObject* aObj);

  void
  GetScope(nsString& aScope) const
  {
    aScope = mScope;
  }

  workers::ServiceWorkerClients*
  Clients();

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

  using DOMEventTargetHelper::AddEventListener;
  virtual void
  AddEventListener(const nsAString& aType,
                   dom::EventListener* aListener,
                   const dom::AddEventListenerOptionsOrBoolean& aOptions,
                   const dom::Nullable<bool>& aWantsUntrusted,
                   ErrorResult& aRv) override;
};

class WorkerDebuggerGlobalScope final : public DOMEventTargetHelper,
                                        public nsIGlobalObject
{
  typedef mozilla::dom::workers::WorkerPrivate WorkerPrivate;

  WorkerPrivate* mWorkerPrivate;
  RefPtr<Console> mConsole;

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

  Console*
  GetConsole(ErrorResult& aRv);

  Console*
  GetConsoleIfExists() const
  {
    return mConsole;
  }

  void
  Dump(JSContext* aCx, const Optional<nsAString>& aString) const;

private:
  virtual ~WorkerDebuggerGlobalScope();
};

} // namespace dom
} // namespace mozilla

inline nsISupports*
ToSupports(mozilla::dom::WorkerGlobalScope* aScope)
{
  return static_cast<nsIDOMEventTarget*>(aScope);
}

#endif /* mozilla_dom_workerscope_h__ */
