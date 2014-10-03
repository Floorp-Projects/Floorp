/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workerscope_h__
#define mozilla_dom_workerscope_h__

#include "Workers.h"
#include "mozilla/DOMEventTargetHelper.h"

namespace mozilla {
namespace dom {

class Console;
class Function;
class Promise;

} // namespace dom
} // namespace mozilla

BEGIN_WORKERS_NAMESPACE

class WorkerPrivate;
class WorkerLocation;
class WorkerNavigator;
class Performance;

class WorkerGlobalScope : public DOMEventTargetHelper,
                          public nsIGlobalObject
{
  nsRefPtr<Console> mConsole;
  nsRefPtr<WorkerLocation> mLocation;
  nsRefPtr<WorkerNavigator> mNavigator;
  nsRefPtr<Performance> mPerformance;

protected:
  WorkerPrivate* mWorkerPrivate;

  explicit WorkerGlobalScope(WorkerPrivate* aWorkerPrivate);
  virtual ~WorkerGlobalScope();

public:
  virtual JSObject*
  WrapObject(JSContext* aCx) MOZ_OVERRIDE;

  virtual JSObject*
  WrapGlobalObject(JSContext* aCx) = 0;

  virtual JSObject*
  GetGlobalJSObject(void) MOZ_OVERRIDE
  {
    return GetWrapper();
  }

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(WorkerGlobalScope,
                                                         DOMEventTargetHelper)

  already_AddRefed<WorkerGlobalScope>
  Self()
  {
    return nsRefPtr<WorkerGlobalScope>(this).forget();
  }

  Console*
  GetConsole();

  already_AddRefed<WorkerLocation>
  Location();

  already_AddRefed<WorkerNavigator>
  Navigator();

  already_AddRefed<WorkerNavigator>
  GetExistingNavigator() const;

  void
  Close(JSContext* aCx);

  OnErrorEventHandlerNonNull*
  GetOnerror();
  void
  SetOnerror(OnErrorEventHandlerNonNull* aHandler);

  void
  ImportScripts(JSContext* aCx, const Sequence<nsString>& aScriptURLs,
                ErrorResult& aRv);

  int32_t
  SetTimeout(JSContext* aCx, Function& aHandler, const int32_t aTimeout,
             const Sequence<JS::Value>& aArguments, ErrorResult& aRv);
  int32_t
  SetTimeout(JSContext* /* unused */, const nsAString& aHandler,
             const int32_t aTimeout, const Sequence<JS::Value>& /* unused */,
             ErrorResult& aRv);
  void
  ClearTimeout(int32_t aHandle, ErrorResult& aRv);
  int32_t
  SetInterval(JSContext* aCx, Function& aHandler,
              const Optional<int32_t>& aTimeout,
              const Sequence<JS::Value>& aArguments, ErrorResult& aRv);
  int32_t
  SetInterval(JSContext* /* unused */, const nsAString& aHandler,
              const Optional<int32_t>& aTimeout,
              const Sequence<JS::Value>& /* unused */, ErrorResult& aRv);
  void
  ClearInterval(int32_t aHandle, ErrorResult& aRv);

  void
  Atob(const nsAString& aAtob, nsAString& aOutput, ErrorResult& aRv) const;
  void
  Btoa(const nsAString& aBtoa, nsAString& aOutput, ErrorResult& aRv) const;

  IMPL_EVENT_HANDLER(online)
  IMPL_EVENT_HANDLER(offline)
  IMPL_EVENT_HANDLER(close)

  void
  Dump(const Optional<nsAString>& aString) const;

  Performance* GetPerformance();
};

class DedicatedWorkerGlobalScope MOZ_FINAL : public WorkerGlobalScope
{
  ~DedicatedWorkerGlobalScope() { }

public:
  explicit DedicatedWorkerGlobalScope(WorkerPrivate* aWorkerPrivate);

  virtual JSObject*
  WrapGlobalObject(JSContext* aCx) MOZ_OVERRIDE;

  void
  PostMessage(JSContext* aCx, JS::Handle<JS::Value> aMessage,
              const Optional<Sequence<JS::Value>>& aTransferable,
              ErrorResult& aRv);

  IMPL_EVENT_HANDLER(message)
};

class SharedWorkerGlobalScope MOZ_FINAL : public WorkerGlobalScope
{
  const nsCString mName;

  ~SharedWorkerGlobalScope() { }

public:
  SharedWorkerGlobalScope(WorkerPrivate* aWorkerPrivate,
                          const nsCString& aName);

  virtual JSObject*
  WrapGlobalObject(JSContext* aCx) MOZ_OVERRIDE;

  void GetName(DOMString& aName) const
  {
    aName.AsAString() = NS_ConvertUTF8toUTF16(mName);
  }

  IMPL_EVENT_HANDLER(connect)
};

class ServiceWorkerGlobalScope MOZ_FINAL : public WorkerGlobalScope
{
  const nsString mScope;
  ~ServiceWorkerGlobalScope() { }

public:
  ServiceWorkerGlobalScope(WorkerPrivate* aWorkerPrivate, const nsACString& aScope);

  virtual JSObject*
  WrapGlobalObject(JSContext* aCx) MOZ_OVERRIDE;

  void
  GetScope(DOMString& aScope) const
  {
    aScope.AsAString() = mScope;
  }

  void
  Close() const
  {
    // no-op close.
  }

  void
  Update()
  {
    // FIXME(nsm): Bug 982728
  }

  already_AddRefed<Promise>
  Unregister(ErrorResult& aRv);

  IMPL_EVENT_HANDLER(activate)
  IMPL_EVENT_HANDLER(beforeevicted)
  IMPL_EVENT_HANDLER(evicted)
  IMPL_EVENT_HANDLER(fetch)
  IMPL_EVENT_HANDLER(install)
  IMPL_EVENT_HANDLER(message)
};

JSObject*
CreateGlobalScope(JSContext* aCx);

END_WORKERS_NAMESPACE

#endif /* mozilla_dom_workerscope_h__ */
