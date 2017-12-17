/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_MediaUtils_h
#define mozilla_MediaUtils_h

#include "AutoTaskQueue.h"
#include "mozilla/Assertions.h"
#include "mozilla/Monitor.h"
#include "mozilla/MozPromise.h"
#include "mozilla/RefPtr.h"
#include "mozilla/SharedThreadPool.h"
#include "mozilla/UniquePtr.h"
#include "nsCOMPtr.h"
#include "nsIAsyncShutdown.h"
#include "nsISupportsImpl.h"
#include "nsThreadUtils.h"

class nsIEventTarget;

namespace mozilla {
namespace media {

/*
 * media::Pledge - A promise-like pattern for c++ that takes lambda functions.
 *
 * Asynchronous APIs that proxy to another thread or to the chrome process and
 * back may find it useful to return a pledge to callers who then use
 * pledge.Then(func) to specify a lambda function to be invoked with the result
 * later back on this same thread.
 *
 * Callers will enjoy that lambdas allow "capturing" of local variables, much
 * like closures in JavaScript (safely by-copy by default).
 *
 * Callers will also enjoy that they do not need to be thread-safe (their code
 * runs on the same thread after all).
 *
 * Advantageously, pledges are non-threadsafe by design (because locking and
 * event queues are redundant). This means none of the lambdas you pass in,
 * or variables you lambda-capture into them, need be threasafe or support
 * threadsafe refcounting. After all, they'll run later on the same thread.
 *
 *   RefPtr<media::Pledge<Foo>> p = GetFooAsynchronously(); // returns a pledge
 *   p->Then([](const Foo& foo) {
 *     // use foo here (same thread. Need not be thread-safe!)
 *   });
 *
 * See media::CoatCheck below for an example of GetFooAsynchronously().
 */

class PledgeBase
{
public:
  NS_INLINE_DECL_REFCOUNTING(PledgeBase);
protected:
  virtual ~PledgeBase() {};
};

template<typename ValueType, typename ErrorType = nsresult>
class Pledge : public PledgeBase
{
  // TODO: Remove workaround once mozilla allows std::function from <functional>
  // wo/std::function support, do template + virtual trick to accept lambdas
  class FunctorsBase
  {
  public:
    FunctorsBase() {}
    virtual void Succeed(ValueType& result) = 0;
    virtual void Fail(ErrorType& error) = 0;
    virtual ~FunctorsBase() {};
  };

public:
  explicit Pledge() : mDone(false), mRejected(false) {}
  Pledge(const Pledge& aOther) = delete;
  Pledge& operator = (const Pledge&) = delete;

  template<typename OnSuccessType>
  void Then(OnSuccessType&& aOnSuccess)
  {
    Then(Forward<OnSuccessType>(aOnSuccess), [](ErrorType&){});
  }

  template<typename OnSuccessType, typename OnFailureType>
  void Then(OnSuccessType&& aOnSuccess, OnFailureType&& aOnFailure)
  {
    class Functors : public FunctorsBase
    {
    public:
      Functors(OnSuccessType&& aOnSuccessRef, OnFailureType&& aOnFailureRef)
        : mOnSuccess(Move(aOnSuccessRef)), mOnFailure(Move(aOnFailureRef)) {}

      void Succeed(ValueType& result)
      {
        mOnSuccess(result);
      }
      void Fail(ErrorType& error)
      {
        mOnFailure(error);
      };

      OnSuccessType mOnSuccess;
      OnFailureType mOnFailure;
    };
    mFunctors = MakeUnique<Functors>(Forward<OnSuccessType>(aOnSuccess),
                                     Forward<OnFailureType>(aOnFailure));
    if (mDone) {
      if (!mRejected) {
        mFunctors->Succeed(mValue);
      } else {
        mFunctors->Fail(mError);
      }
    }
  }

  void Resolve(const ValueType& aValue)
  {
    mValue = aValue;
    Resolve();
  }

  void Reject(ErrorType rv)
  {
    if (!mDone) {
      mDone = mRejected = true;
      mError = rv;
      if (mFunctors) {
        mFunctors->Fail(mError);
      }
    }
  }

protected:
  void Resolve()
  {
    if (!mDone) {
      mDone = true;
      MOZ_ASSERT(!mRejected);
      if (mFunctors) {
        mFunctors->Succeed(mValue);
      }
    }
  }

  ValueType mValue;
private:
  ~Pledge() {};
  bool mDone;
  bool mRejected;
  ErrorType mError;
  UniquePtr<FunctorsBase> mFunctors;
};

/* media::NewRunnableFrom() - Create a Runnable from a lambda.
 *
 * Passing variables (closures) to an async function is clunky with Runnable:
 *
 *   void Foo()
 *   {
 *     class FooRunnable : public Runnable
 *     {
 *     public:
 *       FooRunnable(const Bar &aBar) : mBar(aBar) {}
 *       NS_IMETHOD Run() override
 *       {
 *         // Use mBar
 *       }
 *     private:
 *       RefPtr<Bar> mBar;
 *     };
 *
 *     RefPtr<Bar> bar = new Bar();
 *     NS_DispatchToMainThread(new FooRunnable(bar);
 *   }
 *
 * It's worse with more variables. Lambdas have a leg up with variable capture:
 *
 *   void Foo()
 *   {
 *     RefPtr<Bar> bar = new Bar();
 *     NS_DispatchToMainThread(media::NewRunnableFrom([bar]() mutable {
 *       // use bar
 *     }));
 *   }
 *
 * Capture is by-copy by default, so the nsRefPtr 'bar' is safely copied for
 * access on the other thread (threadsafe refcounting in bar is assumed).
 *
 * The 'mutable' keyword is only needed for non-const access to bar.
 */

template<typename OnRunType>
class LambdaRunnable : public Runnable
{
public:
  explicit LambdaRunnable(OnRunType&& aOnRun)
    : Runnable("media::LambdaRunnable")
    , mOnRun(Move(aOnRun))
  {
  }

private:
  NS_IMETHODIMP
  Run() override
  {
    return mOnRun();
  }
  OnRunType mOnRun;
};

template<typename OnRunType>
already_AddRefed<LambdaRunnable<OnRunType>>
NewRunnableFrom(OnRunType&& aOnRun)
{
  typedef LambdaRunnable<OnRunType> LambdaType;
  RefPtr<LambdaType> lambda = new LambdaType(Forward<OnRunType>(aOnRun));
  return lambda.forget();
}

/* media::CoatCheck - There and back again. Park an object in exchange for an id.
 *
 * A common problem with calling asynchronous functions that do work on other
 * threads or processes is how to pass in a heap object for use once the
 * function completes, without requiring that object to have threadsafe
 * refcounting, contain mutexes, be marshaled, or leak if things fail
 * (or worse, intermittent use-after-free because of lifetime issues).
 *
 * One solution is to set up a coat-check on the caller side, park your object
 * in exchange for an id, and send the id. Common in IPC, but equally useful
 * for same-process thread-hops, because by never leaving the thread there's
 * no need for objects to be threadsafe or use threadsafe refcounting. E.g.
 *
 *   class FooDoer
 *   {
 *     CoatCheck<Foo> mOutstandingFoos;
 *
 *   public:
 *     void DoFoo()
 *     {
 *       RefPtr<Foo> foo = new Foo();
 *       uint32_t requestId = mOutstandingFoos.Append(*foo);
 *       sChild->SendFoo(requestId);
 *     }
 *
 *     void RecvFooResponse(uint32_t requestId)
 *     {
 *       RefPtr<Foo> foo = mOutstandingFoos.Remove(requestId);
 *       if (foo) {
 *         // use foo
 *       }
 *     }
 *   };
 *
 * If you read media::Pledge earlier, here's how this is useful for pledges:
 *
 *   class FooGetter
 *   {
 *     CoatCheck<Pledge<Foo>> mOutstandingPledges;
 *
 *   public:
 *     already_addRefed<Pledge<Foo>> GetFooAsynchronously()
 *     {
 *       RefPtr<Pledge<Foo>> p = new Pledge<Foo>();
 *       uint32_t requestId = mOutstandingPledges.Append(*p);
 *       sChild->SendFoo(requestId);
 *       return p.forget();
 *     }
 *
 *     void RecvFooResponse(uint32_t requestId, const Foo& fooResult)
 *     {
 *       RefPtr<Foo> p = mOutstandingPledges.Remove(requestId);
 *       if (p) {
 *         p->Resolve(fooResult);
 *       }
 *     }
 *   };
 *
 * This helper is currently optimized for very small sets (i.e. not optimized).
 * It is also not thread-safe as the whole point is to stay on the same thread.
 */

template<class T>
class CoatCheck
{
public:
  typedef std::pair<uint32_t, RefPtr<T>> Element;

  uint32_t Append(T& t)
  {
    uint32_t id = GetNextId();
    mElements.AppendElement(Element(id, RefPtr<T>(&t)));
    return id;
  }

  already_AddRefed<T> Remove(uint32_t aId)
  {
    for (auto& element : mElements) {
      if (element.first == aId) {
        RefPtr<T> ref;
        ref.swap(element.second);
        mElements.RemoveElement(element);
        return ref.forget();
      }
    }
    MOZ_ASSERT_UNREACHABLE("Received id with no matching parked object!");
    return nullptr;
  }

private:
  static uint32_t GetNextId()
  {
    static uint32_t counter = 0;
    return ++counter;
  };
  AutoTArray<Element, 3> mElements;
};

/* media::Refcountable - Add threadsafe ref-counting to something that isn't.
 *
 * Often, reference counting is the most practical way to share an object with
 * another thread without imposing lifetime restrictions, even if there's
 * otherwise no concurrent access happening on the object.  For instance, an
 * algorithm on another thread may find it more expedient to modify a passed-in
 * object, rather than pass expensive copies back and forth.
 *
 * Lists in particular often aren't ref-countable, yet are expensive to copy,
 * e.g. nsTArray<RefPtr<Foo>>. Refcountable can be used to make such objects
 * (or owning smart-pointers to such objects) refcountable.
 *
 * Technical limitation: A template specialization is needed for types that take
 * a constructor. Please add below (UniquePtr covers a lot of ground though).
 */

class RefcountableBase
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(RefcountableBase)
protected:
  virtual ~RefcountableBase() {}
};

template<typename T>
class Refcountable : public T, public RefcountableBase
{
public:
  NS_METHOD_(MozExternalRefCountType) AddRef()
  {
    return RefcountableBase::AddRef();
  }

  NS_METHOD_(MozExternalRefCountType) Release()
  {
    return RefcountableBase::Release();
  }

private:
  ~Refcountable<T>() {}
};

template<typename T>
class Refcountable<UniquePtr<T>> : public UniquePtr<T>
{
public:
  explicit Refcountable<UniquePtr<T>>(T* aPtr) : UniquePtr<T>(aPtr) {}
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(Refcountable<T>)
private:
  ~Refcountable<UniquePtr<T>>() {}
};

/* Async shutdown helpers
 */

already_AddRefed<nsIAsyncShutdownClient>
GetShutdownBarrier();

class ShutdownBlocker : public nsIAsyncShutdownBlocker
{
public:
  ShutdownBlocker(const nsString& aName) : mName(aName) {}

  NS_IMETHOD
  BlockShutdown(nsIAsyncShutdownClient* aProfileBeforeChange) override = 0;

  NS_IMETHOD GetName(nsAString& aName) override
  {
    aName = mName;
    return NS_OK;
  }

  NS_IMETHOD GetState(nsIPropertyBag**) override
  {
    return NS_OK;
  }

  NS_DECL_ISUPPORTS
protected:
  virtual ~ShutdownBlocker() {}
private:
  const nsString mName;
};

class ShutdownTicket final
{
public:
  explicit ShutdownTicket(nsIAsyncShutdownBlocker* aBlocker) : mBlocker(aBlocker) {}
  NS_INLINE_DECL_REFCOUNTING(ShutdownTicket)
private:
  ~ShutdownTicket()
  {
    nsCOMPtr<nsIAsyncShutdownClient> barrier = GetShutdownBarrier();
    barrier->RemoveBlocker(mBlocker);
  }

  nsCOMPtr<nsIAsyncShutdownBlocker> mBlocker;
};

/**
 * Await convenience methods to block until the promise has been resolved or
 * rejected. The Resolve/Reject functions, while called on a different thread,
 * would be running just as on the current thread thanks to the memory barrier
 * provided by the monitor.
 * For now Await can only be used with an exclusive MozPromise if passed a
 * Resolve/Reject function.
 */
template<typename ResolveValueType,
         typename RejectValueType,
         typename ResolveFunction,
         typename RejectFunction>
void
Await(
  already_AddRefed<nsIEventTarget> aPool,
  RefPtr<MozPromise<ResolveValueType, RejectValueType, true>> aPromise,
  ResolveFunction&& aResolveFunction,
  RejectFunction&& aRejectFunction)
{
  Monitor mon(__func__);
  RefPtr<AutoTaskQueue> taskQueue =
    new AutoTaskQueue(Move(aPool), "MozPromiseAwait");
  bool done = false;

  aPromise->Then(taskQueue,
                 __func__,
                 [&](ResolveValueType&& aResolveValue) {
                   MonitorAutoLock lock(mon);
                   aResolveFunction(Forward<ResolveValueType>(aResolveValue));
                   done = true;
                   mon.Notify();
                 },
                 [&](RejectValueType&& aRejectValue) {
                   MonitorAutoLock lock(mon);
                   aRejectFunction(Forward<RejectValueType>(aRejectValue));
                   done = true;
                   mon.Notify();
                 });

  MonitorAutoLock lock(mon);
  while (!done) {
    mon.Wait();
  }
}

template<typename ResolveValueType, typename RejectValueType, bool Excl>
typename MozPromise<ResolveValueType, RejectValueType, Excl>::
  ResolveOrRejectValue
Await(already_AddRefed<nsIEventTarget> aPool,
      RefPtr<MozPromise<ResolveValueType, RejectValueType, Excl>> aPromise)
{
  Monitor mon(__func__);
  RefPtr<AutoTaskQueue> taskQueue =
    new AutoTaskQueue(Move(aPool), "MozPromiseAwait");
  bool done = false;

  typename MozPromise<ResolveValueType, RejectValueType, Excl>::ResolveOrRejectValue val;
  aPromise->Then(taskQueue,
                 __func__,
                 [&](ResolveValueType aResolveValue) {
                   val.SetResolve(Move(aResolveValue));
                   MonitorAutoLock lock(mon);
                   done = true;
                   mon.Notify();
                 },
                 [&](RejectValueType aRejectValue) {
                   val.SetReject(Move(aRejectValue));
                   MonitorAutoLock lock(mon);
                   done = true;
                   mon.Notify();
                 });

  MonitorAutoLock lock(mon);
  while (!done) {
    mon.Wait();
  }

  return val;
}

/**
 * Similar to Await, takes an array of promises of the same type.
 * MozPromise::All is used to handle the resolution/rejection of the promises.
 */
template<typename ResolveValueType,
         typename RejectValueType,
         typename ResolveFunction,
         typename RejectFunction>
void
AwaitAll(already_AddRefed<nsIEventTarget> aPool,
         nsTArray<RefPtr<MozPromise<ResolveValueType, RejectValueType, true>>>&
           aPromises,
         ResolveFunction&& aResolveFunction,
         RejectFunction&& aRejectFunction)
{
  typedef MozPromise<ResolveValueType, RejectValueType, true> Promise;
  RefPtr<nsIEventTarget> pool = aPool;
  RefPtr<AutoTaskQueue> taskQueue =
    new AutoTaskQueue(do_AddRef(pool), "MozPromiseAwaitAll");
  RefPtr<typename Promise::AllPromiseType> p = Promise::All(taskQueue, aPromises);
  Await(pool.forget(), p, Move(aResolveFunction), Move(aRejectFunction));
}

// Note: only works with exclusive MozPromise, as Promise::All would attempt
// to perform copy of nsTArrays which are disallowed.
template<typename ResolveValueType, typename RejectValueType>
typename MozPromise<ResolveValueType,
                    RejectValueType,
                    true>::AllPromiseType::ResolveOrRejectValue
AwaitAll(already_AddRefed<nsIEventTarget> aPool,
         nsTArray<RefPtr<MozPromise<ResolveValueType, RejectValueType, true>>>&
           aPromises)
{
  typedef MozPromise<ResolveValueType, RejectValueType, true> Promise;
  RefPtr<nsIEventTarget> pool = aPool;
  RefPtr<AutoTaskQueue> taskQueue =
    new AutoTaskQueue(do_AddRef(pool), "MozPromiseAwaitAll");
  RefPtr<typename Promise::AllPromiseType> p =
    Promise::All(taskQueue, aPromises);
  return Await(pool.forget(), p);
}

} // namespace media
} // namespace mozilla

#endif // mozilla_MediaUtils_h
