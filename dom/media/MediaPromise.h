/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(MediaPromise_h_)
#define MediaPromise_h_

#include "prlog.h"

#include "nsTArray.h"
#include "nsThreadUtils.h"

#include "mozilla/DebugOnly.h"
#include "mozilla/Maybe.h"
#include "mozilla/Mutex.h"
#include "mozilla/Monitor.h"

/* Polyfill __func__ on MSVC for consumers to pass to the MediaPromise API. */
#ifdef _MSC_VER
#define __func__ __FUNCTION__
#endif

class nsIEventTarget;
namespace mozilla {

extern PRLogModuleInfo* gMediaPromiseLog;

#define PROMISE_LOG(x, ...) \
  MOZ_ASSERT(gMediaPromiseLog); \
  PR_LOG(gMediaPromiseLog, PR_LOG_DEBUG, (x, ##__VA_ARGS__))

class MediaTaskQueue;
namespace detail {

nsresult DispatchMediaPromiseRunnable(MediaTaskQueue* aQueue, nsIRunnable* aRunnable);
nsresult DispatchMediaPromiseRunnable(nsIEventTarget* aTarget, nsIRunnable* aRunnable);

} // namespace detail

/*
 * A promise manages an asynchronous request that may or may not be able to be
 * fulfilled immediately. When an API returns a promise, the consumer may attach
 * callbacks to be invoked (asynchronously, on a specified thread) when the
 * request is either completed (resolved) or cannot be completed (rejected).
 *
 * When IsExclusive is true, the MediaPromise does a release-mode assertion that
 * there is at most one call to either Then(...) or ChainTo(...).
 */
template<typename T> class MediaPromiseHolder;
template<typename ResolveValueT, typename RejectValueT, bool IsExclusive>
class MediaPromise
{
public:
  typedef ResolveValueT ResolveValueType;
  typedef RejectValueT RejectValueType;

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MediaPromise)
  explicit MediaPromise(const char* aCreationSite)
    : mCreationSite(aCreationSite)
    , mMutex("MediaPromise Mutex")
    , mHaveConsumer(false)
  {
    PROMISE_LOG("%s creating MediaPromise (%p)", mCreationSite, this);
  }

  static nsRefPtr<MediaPromise>
  CreateAndResolve(ResolveValueType aResolveValue, const char* aResolveSite)
  {
    nsRefPtr<MediaPromise> p = new MediaPromise(aResolveSite);
    p->Resolve(aResolveValue, aResolveSite);
    return p;
  }

  static nsRefPtr<MediaPromise>
  CreateAndReject(RejectValueType aRejectValue, const char* aRejectSite)
  {
    nsRefPtr<MediaPromise> p = new MediaPromise(aRejectSite);
    p->Reject(aRejectValue, aRejectSite);
    return p;
  }

protected:

  /*
   * A ThenValue tracks a single consumer waiting on the promise. When a consumer
   * invokes promise->Then(...), a ThenValue is created. Once the Promise is
   * resolved or rejected, a {Resolve,Reject}Runnable is dispatched, which
   * invokes the resolve/reject method and then deletes the ThenValue.
   */
  class ThenValueBase
  {
  public:
    class ResolveRunnable : public nsRunnable
    {
    public:
      ResolveRunnable(ThenValueBase* aThenValue, ResolveValueType aResolveValue)
        : mThenValue(aThenValue)
        , mResolveValue(aResolveValue) {}

      ~ResolveRunnable()
      {
        MOZ_ASSERT(!mThenValue);
      }

      NS_IMETHODIMP Run()
      {
        PROMISE_LOG("ResolveRunnable::Run() [this=%p]", this);
        mThenValue->DoResolve(mResolveValue);

        delete mThenValue;
        mThenValue = nullptr;
        return NS_OK;
      }

    private:
      ThenValueBase* mThenValue;
      ResolveValueType mResolveValue;
    };

    class RejectRunnable : public nsRunnable
    {
    public:
      RejectRunnable(ThenValueBase* aThenValue, RejectValueType aRejectValue)
        : mThenValue(aThenValue)
        , mRejectValue(aRejectValue) {}

      ~RejectRunnable()
      {
        MOZ_ASSERT(!mThenValue);
      }

      NS_IMETHODIMP Run()
      {
        PROMISE_LOG("RejectRunnable::Run() [this=%p]", this);
        mThenValue->DoReject(mRejectValue);

        delete mThenValue;
        mThenValue = nullptr;
        return NS_OK;
      }

    private:
      ThenValueBase* mThenValue;
      RejectValueType mRejectValue;
    };

    explicit ThenValueBase(const char* aCallSite) : mCallSite(aCallSite)
    {
      MOZ_COUNT_CTOR(ThenValueBase);
    }

    virtual void Dispatch(MediaPromise *aPromise) = 0;

  protected:
    // This may only be deleted by {Resolve,Reject}Runnable::Run.
    virtual ~ThenValueBase() { MOZ_COUNT_DTOR(ThenValueBase); }

    virtual void DoResolve(ResolveValueType aResolveValue) = 0;
    virtual void DoReject(RejectValueType aRejectValue) = 0;

    const char* mCallSite;
  };

  /*
   * We create two overloads for invoking Resolve/Reject Methods so as to
   * make the resolve/reject value argument "optional".
   */

  // Avoid confusing the compiler when the callback accepts T* but the ValueType
  // is nsRefPtr<T>. See bug 1109954 comment 6.
  template <typename T>
  struct NonDeduced
  {
    typedef T type;
  };

  template<typename ThisType, typename ValueType>
  static void InvokeCallbackMethod(ThisType* aThisVal, void(ThisType::*aMethod)(ValueType),
                                   typename NonDeduced<ValueType>::type aValue)
  {
      ((*aThisVal).*aMethod)(aValue);
  }

  template<typename ThisType, typename ValueType>
  static void InvokeCallbackMethod(ThisType* aThisVal, void(ThisType::*aMethod)(), ValueType aValue)
  {
      ((*aThisVal).*aMethod)();
  }

  template<typename TargetType, typename ThisType,
           typename ResolveMethodType, typename RejectMethodType>
  class ThenValue : public ThenValueBase
  {
  public:
    ThenValue(TargetType* aResponseTarget, ThisType* aThisVal,
              ResolveMethodType aResolveMethod, RejectMethodType aRejectMethod,
              const char* aCallSite)
      : ThenValueBase(aCallSite)
      , mResponseTarget(aResponseTarget)
      , mThisVal(aThisVal)
      , mResolveMethod(aResolveMethod)
      , mRejectMethod(aRejectMethod) {}

    void Dispatch(MediaPromise *aPromise) MOZ_OVERRIDE
    {
      aPromise->mMutex.AssertCurrentThreadOwns();
      MOZ_ASSERT(!aPromise->IsPending());
      bool resolved = aPromise->mResolveValue.isSome();
      nsRefPtr<nsRunnable> runnable =
        resolved ? static_cast<nsRunnable*>(new (typename ThenValueBase::ResolveRunnable)(this, aPromise->mResolveValue.ref()))
                 : static_cast<nsRunnable*>(new (typename ThenValueBase::RejectRunnable)(this, aPromise->mRejectValue.ref()));
      PROMISE_LOG("%s Then() call made from %s [Runnable=%p, Promise=%p, ThenValue=%p]",
                  resolved ? "Resolving" : "Rejecting", ThenValueBase::mCallSite,
                  runnable.get(), aPromise, this);
      DebugOnly<nsresult> rv = detail::DispatchMediaPromiseRunnable(mResponseTarget, runnable);
      MOZ_ASSERT(NS_SUCCEEDED(rv));
    }

  protected:
    virtual void DoResolve(ResolveValueType aResolveValue) MOZ_OVERRIDE
    {
      InvokeCallbackMethod(mThisVal.get(), mResolveMethod, aResolveValue);
    }

    virtual void DoReject(RejectValueType aRejectValue) MOZ_OVERRIDE
    {
      InvokeCallbackMethod(mThisVal.get(), mRejectMethod, aRejectValue);
    }

    virtual ~ThenValue() {}

  private:
    nsRefPtr<TargetType> mResponseTarget;
    nsRefPtr<ThisType> mThisVal;
    ResolveMethodType mResolveMethod;
    RejectMethodType mRejectMethod;
  };
public:

  template<typename TargetType, typename ThisType,
           typename ResolveMethodType, typename RejectMethodType>
  void Then(TargetType* aResponseTarget, const char* aCallSite, ThisType* aThisVal,
            ResolveMethodType aResolveMethod, RejectMethodType aRejectMethod)
  {
    MutexAutoLock lock(mMutex);
    MOZ_RELEASE_ASSERT(!IsExclusive || !mHaveConsumer);
    mHaveConsumer = true;
    ThenValueBase* thenValue = new ThenValue<TargetType, ThisType, ResolveMethodType,
                                             RejectMethodType>(aResponseTarget, aThisVal,
                                                               aResolveMethod, aRejectMethod,
                                                               aCallSite);
    PROMISE_LOG("%s invoking Then() [this=%p, thenValue=%p, aThisVal=%p, isPending=%d]",
                aCallSite, this, thenValue, aThisVal, (int) IsPending());
    if (!IsPending()) {
      thenValue->Dispatch(this);
    } else {
      mThenValues.AppendElement(thenValue);
    }
  }

  void ChainTo(already_AddRefed<MediaPromise> aChainedPromise, const char* aCallSite)
  {
    MutexAutoLock lock(mMutex);
    MOZ_RELEASE_ASSERT(!IsExclusive || !mHaveConsumer);
    mHaveConsumer = true;
    nsRefPtr<MediaPromise> chainedPromise = aChainedPromise;
    PROMISE_LOG("%s invoking Chain() [this=%p, chainedPromise=%p, isPending=%d]",
                aCallSite, this, chainedPromise.get(), (int) IsPending());
    if (!IsPending()) {
      ForwardTo(chainedPromise);
    } else {
      mChainedPromises.AppendElement(chainedPromise);
    }
  }

  void Resolve(ResolveValueType aResolveValue, const char* aResolveSite)
  {
    MutexAutoLock lock(mMutex);
    MOZ_ASSERT(IsPending());
    PROMISE_LOG("%s resolving MediaPromise (%p created at %s)", aResolveSite, this, mCreationSite);
    mResolveValue.emplace(aResolveValue);
    DispatchAll();
  }

  void Reject(RejectValueType aRejectValue, const char* aRejectSite)
  {
    MutexAutoLock lock(mMutex);
    MOZ_ASSERT(IsPending());
    PROMISE_LOG("%s rejecting MediaPromise (%p created at %s)", aRejectSite, this, mCreationSite);
    mRejectValue.emplace(aRejectValue);
    DispatchAll();
  }

protected:
  bool IsPending() { return mResolveValue.isNothing() && mRejectValue.isNothing(); }
  void DispatchAll()
  {
    mMutex.AssertCurrentThreadOwns();
    for (size_t i = 0; i < mThenValues.Length(); ++i) {
      mThenValues[i]->Dispatch(this);
    }
    mThenValues.Clear();

    for (size_t i = 0; i < mChainedPromises.Length(); ++i) {
      ForwardTo(mChainedPromises[i]);
    }
    mChainedPromises.Clear();
  }

  void ForwardTo(MediaPromise* aOther)
  {
    MOZ_ASSERT(!IsPending());
    if (mResolveValue.isSome()) {
      aOther->Resolve(mResolveValue.ref(), "<chained promise>");
    } else {
      aOther->Reject(mRejectValue.ref(), "<chained promise>");
    }
  }

  ~MediaPromise()
  {
    PROMISE_LOG("MediaPromise::~MediaPromise [this=%p]", this);
    MOZ_ASSERT(!IsPending());
    MOZ_ASSERT(mThenValues.IsEmpty());
    MOZ_ASSERT(mChainedPromises.IsEmpty());
  };

  const char* mCreationSite; // For logging
  Mutex mMutex;
  Maybe<ResolveValueType> mResolveValue;
  Maybe<RejectValueType> mRejectValue;
  nsTArray<ThenValueBase*> mThenValues;
  nsTArray<nsRefPtr<MediaPromise>> mChainedPromises;
  bool mHaveConsumer;
};

/*
 * Class to encapsulate a promise for a particular role. Use this as the member
 * variable for a class whose method returns a promise.
 */
template<typename PromiseType>
class MediaPromiseHolder
{
public:
  MediaPromiseHolder()
    : mMonitor(nullptr) {}

  ~MediaPromiseHolder() { MOZ_ASSERT(!mPromise); }

  already_AddRefed<PromiseType> Ensure(const char* aMethodName) {
    if (mMonitor) {
      mMonitor->AssertCurrentThreadOwns();
    }
    if (!mPromise) {
      mPromise = new PromiseType(aMethodName);
    }
    nsRefPtr<PromiseType> p = mPromise;
    return p.forget();
  }

  // Provide a Monitor that should always be held when accessing this instance.
  void SetMonitor(Monitor* aMonitor) { mMonitor = aMonitor; }

  bool IsEmpty()
  {
    if (mMonitor) {
      mMonitor->AssertCurrentThreadOwns();
    }
    return !mPromise;
  }

  already_AddRefed<PromiseType> Steal()
  {
    if (mMonitor) {
      mMonitor->AssertCurrentThreadOwns();
    }

    nsRefPtr<PromiseType> p = mPromise;
    mPromise = nullptr;
    return p.forget();
  }

  void Resolve(typename PromiseType::ResolveValueType aResolveValue,
               const char* aMethodName)
  {
    if (mMonitor) {
      mMonitor->AssertCurrentThreadOwns();
    }
    MOZ_ASSERT(mPromise);
    mPromise->Resolve(aResolveValue, aMethodName);
    mPromise = nullptr;
  }

  void ResolveIfExists(typename PromiseType::ResolveValueType aResolveValue,
                       const char* aMethodName)
  {
    if (!IsEmpty()) {
      Resolve(aResolveValue, aMethodName);
    }
  }

  void Reject(typename PromiseType::RejectValueType aRejectValue,
              const char* aMethodName)
  {
    if (mMonitor) {
      mMonitor->AssertCurrentThreadOwns();
    }
    MOZ_ASSERT(mPromise);
    mPromise->Reject(aRejectValue, aMethodName);
    mPromise = nullptr;
  }

  void RejectIfExists(typename PromiseType::RejectValueType aRejectValue,
                      const char* aMethodName)
  {
    if (!IsEmpty()) {
      Reject(aRejectValue, aMethodName);
    }
  }

private:
  Monitor* mMonitor;
  nsRefPtr<PromiseType> mPromise;
};

#undef PROMISE_LOG

} // namespace mozilla

#endif
