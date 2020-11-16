/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_mscom_AsyncInvoker_h
#define mozilla_mscom_AsyncInvoker_h

#include <objidl.h>
#include <windows.h>

#include <utility>

#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/Maybe.h"
#include "mozilla/Mutex.h"
#include "mozilla/mscom/Aggregation.h"
#include "mozilla/mscom/Utils.h"
#include "nsISerialEventTarget.h"
#include "nsISupportsImpl.h"
#include "nsThreadUtils.h"

namespace mozilla {
namespace mscom {
namespace detail {

template <typename AsyncInterface>
class ForgettableAsyncCall : public ISynchronize {
 public:
  explicit ForgettableAsyncCall(ICallFactory* aCallFactory)
      : mRefCnt(0), mAsyncCall(nullptr) {
    StabilizedRefCount<Atomic<ULONG>> stabilizer(mRefCnt);

    HRESULT hr =
        aCallFactory->CreateCall(__uuidof(AsyncInterface), this, IID_IUnknown,
                                 getter_AddRefs(mInnerUnk));
    if (FAILED(hr)) {
      return;
    }

    hr = mInnerUnk->QueryInterface(__uuidof(AsyncInterface),
                                   reinterpret_cast<void**>(&mAsyncCall));
    if (SUCCEEDED(hr)) {
      // Don't hang onto a ref. Because mAsyncCall is aggregated, its refcount
      // is this->mRefCnt, so we'd create a cycle!
      mAsyncCall->Release();
    }
  }

  AsyncInterface* GetInterface() const { return mAsyncCall; }

  // IUnknown
  STDMETHODIMP QueryInterface(REFIID aIid, void** aOutInterface) final {
    if (aIid == IID_ISynchronize || aIid == IID_IUnknown) {
      RefPtr<ISynchronize> ptr(this);
      ptr.forget(aOutInterface);
      return S_OK;
    }

    return mInnerUnk->QueryInterface(aIid, aOutInterface);
  }

  STDMETHODIMP_(ULONG) AddRef() final {
    ULONG result = ++mRefCnt;
    NS_LOG_ADDREF(this, result, "ForgettableAsyncCall", sizeof(*this));
    return result;
  }

  STDMETHODIMP_(ULONG) Release() final {
    ULONG result = --mRefCnt;
    NS_LOG_RELEASE(this, result, "ForgettableAsyncCall");
    if (!result) {
      delete this;
    }
    return result;
  }

  // ISynchronize
  STDMETHODIMP Wait(DWORD aFlags, DWORD aTimeoutMilliseconds) override {
    return E_NOTIMPL;
  }

  STDMETHODIMP Signal() override {
    // Even though this function is a no-op, we must return S_OK as opposed to
    // E_NOTIMPL or else COM will consider the async call to have failed.
    return S_OK;
  }

  STDMETHODIMP Reset() override {
    // Even though this function is a no-op, we must return S_OK as opposed to
    // E_NOTIMPL or else COM will consider the async call to have failed.
    return S_OK;
  }

 protected:
  virtual ~ForgettableAsyncCall() = default;

 private:
  Atomic<ULONG> mRefCnt;
  RefPtr<IUnknown> mInnerUnk;
  AsyncInterface* mAsyncCall;  // weak reference
};

template <typename AsyncInterface>
class WaitableAsyncCall : public ForgettableAsyncCall<AsyncInterface> {
 public:
  explicit WaitableAsyncCall(ICallFactory* aCallFactory)
      : ForgettableAsyncCall<AsyncInterface>(aCallFactory),
        mEvent(::CreateEventW(nullptr, FALSE, FALSE, nullptr)) {}

  STDMETHODIMP Wait(DWORD aFlags, DWORD aTimeoutMilliseconds) override {
    const DWORD waitStart =
        aTimeoutMilliseconds == INFINITE ? 0 : ::GetTickCount();
    DWORD flags = aFlags;
    if (XRE_IsContentProcess() && NS_IsMainThread()) {
      flags |= COWAIT_ALERTABLE;
    }

    HRESULT hr;
    DWORD signaledIdx;

    DWORD elapsed = 0;

    while (true) {
      if (aTimeoutMilliseconds != INFINITE) {
        elapsed = ::GetTickCount() - waitStart;
      }
      if (elapsed >= aTimeoutMilliseconds) {
        return RPC_S_CALLPENDING;
      }

      ::SetLastError(ERROR_SUCCESS);

      hr = ::CoWaitForMultipleHandles(flags, aTimeoutMilliseconds - elapsed, 1,
                                      &mEvent, &signaledIdx);
      if (hr == RPC_S_CALLPENDING || FAILED(hr)) {
        return hr;
      }

      if (hr == S_OK && signaledIdx == 0) {
        return hr;
      }
    }
  }

  STDMETHODIMP Signal() override {
    if (!::SetEvent(mEvent)) {
      return HRESULT_FROM_WIN32(::GetLastError());
    }
    return S_OK;
  }

 protected:
  ~WaitableAsyncCall() {
    if (mEvent) {
      ::CloseHandle(mEvent);
    }
  }

 private:
  HANDLE mEvent;
};

template <typename AsyncInterface>
class EventDrivenAsyncCall : public ForgettableAsyncCall<AsyncInterface> {
 public:
  explicit EventDrivenAsyncCall(ICallFactory* aCallFactory)
      : ForgettableAsyncCall<AsyncInterface>(aCallFactory) {}

  bool HasCompletionRunnable() const { return !!mCompletionRunnable; }

  void ClearCompletionRunnable() { mCompletionRunnable = nullptr; }

  void SetCompletionRunnable(already_AddRefed<nsIRunnable> aRunnable) {
    nsCOMPtr<nsIRunnable> innerRunnable(aRunnable);
    MOZ_ASSERT(!!innerRunnable);
    if (!innerRunnable) {
      return;
    }

    // We need to retain a ref to ourselves to outlive the AsyncInvoker
    // such that our callback can execute.
    RefPtr<EventDrivenAsyncCall<AsyncInterface>> self(this);

    mCompletionRunnable = NS_NewRunnableFunction(
        "EventDrivenAsyncCall outer completion Runnable",
        [innerRunnable = std::move(innerRunnable), self = std::move(self)]() {
          innerRunnable->Run();
        });
  }

  void SetEventTarget(nsISerialEventTarget* aTarget) { mEventTarget = aTarget; }

  STDMETHODIMP Signal() override {
    MOZ_ASSERT(!!mCompletionRunnable);
    if (!mCompletionRunnable) {
      return S_OK;
    }

    nsCOMPtr<nsISerialEventTarget> eventTarget(mEventTarget.forget());
    if (!eventTarget) {
      eventTarget = GetMainThreadSerialEventTarget();
    }

    DebugOnly<nsresult> rv =
        eventTarget->Dispatch(mCompletionRunnable.forget(), NS_DISPATCH_NORMAL);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
    return S_OK;
  }

 private:
  nsCOMPtr<nsIRunnable> mCompletionRunnable;
  nsCOMPtr<nsISerialEventTarget> mEventTarget;
};

template <typename AsyncInterface>
class FireAndForgetInvoker {
 protected:
  void OnBeginInvoke() {}
  void OnSyncInvoke(HRESULT aHr) {}
  void OnAsyncInvokeFailed() {}

  typedef ForgettableAsyncCall<AsyncInterface> AsyncCallType;

  RefPtr<ForgettableAsyncCall<AsyncInterface>> mAsyncCall;
};

template <typename AsyncInterface>
class WaitableInvoker {
 public:
  HRESULT Wait(DWORD aTimeout = INFINITE) const {
    if (!mAsyncCall) {
      // Nothing to wait for
      return S_OK;
    }

    return mAsyncCall->Wait(0, aTimeout);
  }

 protected:
  void OnBeginInvoke() {}
  void OnSyncInvoke(HRESULT aHr) {}
  void OnAsyncInvokeFailed() {}

  typedef WaitableAsyncCall<AsyncInterface> AsyncCallType;

  RefPtr<WaitableAsyncCall<AsyncInterface>> mAsyncCall;
};

template <typename AsyncInterface>
class EventDrivenInvoker {
 public:
  void SetCompletionRunnable(already_AddRefed<nsIRunnable> aRunnable) {
    if (mAsyncCall) {
      mAsyncCall->SetCompletionRunnable(std::move(aRunnable));
      return;
    }

    mCompletionRunnable = aRunnable;
  }

  void SetAsyncEventTarget(nsISerialEventTarget* aTarget) {
    if (mAsyncCall) {
      mAsyncCall->SetEventTarget(aTarget);
    }
  }

 protected:
  void OnBeginInvoke() {
    MOZ_RELEASE_ASSERT(
        mCompletionRunnable ||
            (mAsyncCall && mAsyncCall->HasCompletionRunnable()),
        "You should have called SetCompletionRunnable before invoking!");
  }

  void OnSyncInvoke(HRESULT aHr) {
    nsCOMPtr<nsIRunnable> completionRunnable(mCompletionRunnable.forget());
    if (FAILED(aHr)) {
      return;
    }

    completionRunnable->Run();
  }

  void OnAsyncInvokeFailed() {
    MOZ_ASSERT(!!mAsyncCall);
    mAsyncCall->ClearCompletionRunnable();
  }

  typedef EventDrivenAsyncCall<AsyncInterface> AsyncCallType;

  RefPtr<EventDrivenAsyncCall<AsyncInterface>> mAsyncCall;
  nsCOMPtr<nsIRunnable> mCompletionRunnable;
};

}  // namespace detail

/**
 * This class is intended for "fire-and-forget" asynchronous invocations of COM
 * interfaces. This requires that an interface be annotated with the
 * |async_uuid| attribute in midl. We also require that there be no outparams
 * in the desired asynchronous interface (otherwise that would break the
 * desired "fire-and-forget" semantics).
 *
 * For example, let us suppose we have some IDL as such:
 * [object, uuid(...), async_uuid(...)]
 * interface IFoo : IUnknown
 * {
 *    HRESULT Bar([in] long baz);
 * }
 *
 * Then, given an IFoo, we may construct an AsyncInvoker<IFoo, AsyncIFoo>:
 *
 * IFoo* foo = ...;
 * AsyncInvoker<IFoo, AsyncIFoo> myInvoker(foo);
 * HRESULT hr = myInvoker.Invoke(&IFoo::Bar, &AsyncIFoo::Begin_Bar, 7);
 *
 * Alternatively you may use the ASYNC_INVOKER_FOR and ASYNC_INVOKE macros,
 * which automatically deduce the name of the asynchronous interface from the
 * name of the synchronous interface:
 *
 * ASYNC_INVOKER_FOR(IFoo) myInvoker(foo);
 * HRESULT hr = ASYNC_INVOKE(myInvoker, Bar, 7);
 *
 * This class may also be used when a synchronous COM call must be made that
 * might reenter the content process. In this case, use the WaitableAsyncInvoker
 * variant, or the WAITABLE_ASYNC_INVOKER_FOR macro:
 *
 * WAITABLE_ASYNC_INVOKER_FOR(Ifoo) myInvoker(foo);
 * HRESULT hr = ASYNC_INVOKE(myInvoker, Bar, 7);
 * if (SUCCEEDED(hr)) {
 *   myInvoker.Wait(); // <-- Wait for the COM call to complete.
 * }
 *
 * In general you should avoid using the waitable version, but in some corner
 * cases it is absolutely necessary in order to preserve correctness while
 * avoiding deadlock.
 *
 * Finally, it is also possible to have the async invoker enqueue a runnable
 * to the main thread when the async operation completes:
 *
 * EVENT_DRIVEN_ASYNC_INVOKER_FOR(Ifoo) myInvoker(foo);
 * // myRunnable will be invoked on the main thread once the async operation
 * // has completed. Note that we set this *before* we do the ASYNC_INVOKE!
 * myInvoker.SetCompletionRunnable(myRunnable.forget());
 * HRESULT hr = ASYNC_INVOKE(myInvoker, Bar, 7);
 * // ...
 */
template <typename SyncInterface, typename AsyncInterface,
          template <typename Iface> class WaitPolicy =
              detail::FireAndForgetInvoker>
class MOZ_RAII AsyncInvoker final : public WaitPolicy<AsyncInterface> {
  using Base = WaitPolicy<AsyncInterface>;

 public:
  typedef SyncInterface SyncInterfaceT;
  typedef AsyncInterface AsyncInterfaceT;

  /**
   * @param aSyncObj The COM object on which to invoke the asynchronous event.
   *                 If this object is not a proxy to the synchronous variant
   *                 of AsyncInterface, then it will be invoked synchronously
   *                 instead (because it is an in-process virtual method call).
   * @param aIsProxy An optional hint as to whether or not aSyncObj is a proxy.
   *                 If not specified, AsyncInvoker will automatically detect
   *                 whether aSyncObj is a proxy, however there may be a
   *                 performance penalty associated with that.
   */
  explicit AsyncInvoker(SyncInterface* aSyncObj,
                        const Maybe<bool>& aIsProxy = Nothing()) {
    MOZ_ASSERT(aSyncObj);

    RefPtr<ICallFactory> callFactory;
    if ((aIsProxy.isSome() && !aIsProxy.value()) ||
        FAILED(aSyncObj->QueryInterface(IID_ICallFactory,
                                        getter_AddRefs(callFactory)))) {
      mSyncObj = aSyncObj;
      return;
    }

    this->mAsyncCall = new typename Base::AsyncCallType(callFactory);
  }

  /**
   * @brief Invoke a method on the object. Member function pointers are provided
   *        for both the sychronous and asynchronous variants of the interface.
   *        If this invoker's encapsulated COM object is a proxy, then Invoke
   *        will call the asynchronous member function. Otherwise the
   *        synchronous version must be used, as the invocation will simply be a
   *        virtual function call that executes in-process.
   * @param aSyncMethod Pointer to the method that we would like to invoke on
   *        the synchronous interface.
   * @param aAsyncMethod Pointer to the method that we would like to invoke on
   *        the asynchronous interface.
   */
  template <typename SyncMethod, typename AsyncMethod, typename... Args>
  HRESULT Invoke(SyncMethod aSyncMethod, AsyncMethod aAsyncMethod,
                 Args&&... aArgs) {
    this->OnBeginInvoke();
    if (mSyncObj) {
      HRESULT hr = (mSyncObj->*aSyncMethod)(std::forward<Args>(aArgs)...);
      this->OnSyncInvoke(hr);
      return hr;
    }

    MOZ_ASSERT(this->mAsyncCall);
    if (!this->mAsyncCall) {
      this->OnAsyncInvokeFailed();
      return E_POINTER;
    }

    AsyncInterface* asyncInterface = this->mAsyncCall->GetInterface();
    MOZ_ASSERT(asyncInterface);
    if (!asyncInterface) {
      this->OnAsyncInvokeFailed();
      return E_POINTER;
    }

    HRESULT hr = (asyncInterface->*aAsyncMethod)(std::forward<Args>(aArgs)...);
    if (FAILED(hr)) {
      this->OnAsyncInvokeFailed();
    }

    return hr;
  }

  AsyncInvoker(const AsyncInvoker& aOther) = delete;
  AsyncInvoker(AsyncInvoker&& aOther) = delete;
  AsyncInvoker& operator=(const AsyncInvoker& aOther) = delete;
  AsyncInvoker& operator=(AsyncInvoker&& aOther) = delete;

 private:
  RefPtr<SyncInterface> mSyncObj;
};

template <typename SyncInterface, typename AsyncInterface>
using WaitableAsyncInvoker =
    AsyncInvoker<SyncInterface, AsyncInterface, detail::WaitableInvoker>;

template <typename SyncInterface, typename AsyncInterface>
using EventDrivenAsyncInvoker =
    AsyncInvoker<SyncInterface, AsyncInterface, detail::EventDrivenInvoker>;

}  // namespace mscom
}  // namespace mozilla

#define ASYNC_INVOKER_FOR(SyncIface) \
  mozilla::mscom::AsyncInvoker<SyncIface, Async##SyncIface>

#define WAITABLE_ASYNC_INVOKER_FOR(SyncIface) \
  mozilla::mscom::WaitableAsyncInvoker<SyncIface, Async##SyncIface>

#define EVENT_DRIVEN_ASYNC_INVOKER_FOR(SyncIface) \
  mozilla::mscom::EventDrivenAsyncInvoker<SyncIface, Async##SyncIface>

#define ASYNC_INVOKE(InvokerObj, SyncMethodName, ...)                 \
  InvokerObj.Invoke(                                                  \
      &decltype(InvokerObj)::SyncInterfaceT::SyncMethodName,          \
      &decltype(InvokerObj)::AsyncInterfaceT::Begin_##SyncMethodName, \
      ##__VA_ARGS__)

#endif  // mozilla_mscom_AsyncInvoker_h
