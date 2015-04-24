/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(StateMirroring_h_)
#define StateMirroring_h_

#include "MediaPromise.h"

#include "StateWatching.h"
#include "TaskDispatcher.h"

#include "mozilla/Maybe.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/unused.h"

#include "prlog.h"
#include "nsISupportsImpl.h"

/*
 * The state-mirroring machinery allows pieces of interesting state to be
 * observed on multiple thread without locking. The basic strategy is to track
 * changes in a canonical value and post updates to other threads that hold
 * mirrors for that value.
 *
 * One problem with the naive implementation of such a system is that some pieces
 * of state need to be updated atomically, and certain other operations need to
 * wait for these atomic updates to complete before executing. The state-mirroring
 * machinery solves this problem by requiring that its owner thread uses tail
 * dispatch, and posting state update events (which should always be run first by
 * TaskDispatcher implementations) to that tail dispatcher. This ensures that
 * state changes are always atomic from the perspective of observing threads.
 *
 * Given that semantics may change and comments tend to go out of date, we
 * deliberately don't provide usage examples here. Grep around to find them.
 */

namespace mozilla {

// Mirror<T> and Canonical<T> inherit WatchTarget, so we piggy-back on the
// logging that WatchTarget already does. Given that, it makes sense to share
// the same log module.
#define MIRROR_LOG(x, ...) \
  MOZ_ASSERT(gStateWatchingLog); \
  PR_LOG(gStateWatchingLog, PR_LOG_DEBUG, (x, ##__VA_ARGS__))

template<typename T> class AbstractMirror;

/*
 * AbstractCanonical is a superclass from which all Canonical values must
 * inherit. It serves as the interface of operations which may be performed (via
 * asynchronous dispatch) by other threads, in particular by the corresponding
 * Mirror value.
 */
template<typename T>
class AbstractCanonical
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(AbstractCanonical)
  AbstractCanonical(AbstractThread* aThread) : mOwnerThread(aThread) {}
  virtual void AddMirror(AbstractMirror<T>* aMirror) = 0;
  virtual void RemoveMirror(AbstractMirror<T>* aMirror) = 0;

  AbstractThread* OwnerThread() const { return mOwnerThread; }
protected:
  virtual ~AbstractCanonical() {}
  nsRefPtr<AbstractThread> mOwnerThread;
};

/*
 * AbstractMirror is a superclass from which all Mirror values must
 * inherit. It serves as the interface of operations which may be performed (via
 * asynchronous dispatch) by other threads, in particular by the corresponding
 * Canonical value.
 */
template<typename T>
class AbstractMirror
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(AbstractMirror)
  AbstractMirror(AbstractThread* aThread) : mOwnerThread(aThread) {}
  virtual void UpdateValue(const T& aNewValue) = 0;
  virtual void NotifyDisconnected() = 0;

  AbstractThread* OwnerThread() const { return mOwnerThread; }
protected:
  virtual ~AbstractMirror() {}
  nsRefPtr<AbstractThread> mOwnerThread;
};

/*
 * Canonical<T> is a wrapper class that allows a given value to be mirrored by other
 * threads. It maintains a list of active mirrors, and queues updates for them
 * when the internal value changes. When changing the value, the caller needs to
 * pass a TaskDispatcher object, which fires the updates at the appropriate time.
 * Canonical<T> is also a WatchTarget, and may be set up to trigger other routines
 * (on the same thread) when the canonical value changes.
 *
 * Do not instantiate a Canonical<T> directly as a member. Instead, instantiate a
 * Canonical<T>::Holder, which handles lifetime issues and may eventually be
 * extended to do other things as well.
 */
template<typename T>
class Canonical : public AbstractCanonical<T>, public WatchTarget
{
public:
  using AbstractCanonical<T>::OwnerThread;

  Canonical(AbstractThread* aThread, const T& aInitialValue, const char* aName)
    : AbstractCanonical<T>(aThread), WatchTarget(aName), mValue(aInitialValue)
  {
    MIRROR_LOG("%s [%p] initialized", mName, this);
    MOZ_ASSERT(aThread->RequiresTailDispatch(), "Can't get coherency without tail dispatch");
  }

  void AddMirror(AbstractMirror<T>* aMirror) override
  {
    MIRROR_LOG("%s [%p] adding mirror %p", mName, this, aMirror);
    MOZ_ASSERT(OwnerThread()->IsCurrentThreadIn());
    MOZ_ASSERT(!mMirrors.Contains(aMirror));
    mMirrors.AppendElement(aMirror);
    aMirror->OwnerThread()->Dispatch(MakeNotifier(aMirror));
  }

  void RemoveMirror(AbstractMirror<T>* aMirror) override
  {
    MIRROR_LOG("%s [%p] removing mirror %p", mName, this, aMirror);
    MOZ_ASSERT(OwnerThread()->IsCurrentThreadIn());
    MOZ_ASSERT(mMirrors.Contains(aMirror));
    mMirrors.RemoveElement(aMirror);
  }

  void DisconnectAll()
  {
    MIRROR_LOG("%s [%p] Disconnecting all mirrors", mName, this);
    for (size_t i = 0; i < mMirrors.Length(); ++i) {
      nsCOMPtr<nsIRunnable> r =
        NS_NewRunnableMethod(mMirrors[i], &AbstractMirror<T>::NotifyDisconnected);
      mMirrors[i]->OwnerThread()->Dispatch(r.forget());
    }
    mMirrors.Clear();
  }

  operator const T&()
  {
    MOZ_ASSERT(OwnerThread()->IsCurrentThreadIn());
    return mValue;
  }

  Canonical& operator=(const T& aNewValue)
  {
    MOZ_ASSERT(OwnerThread()->IsCurrentThreadIn());

    if (aNewValue == mValue) {
      return *this;
    }

    // Notify same-thread watchers. The state watching machinery will make sure
    // that notifications run at the right time.
    NotifyWatchers();

    // Check if we've already got a pending update. If so we won't schedule another
    // one.
    bool alreadyNotifying = mInitialValue.isSome();

    // Stash the initial value if needed, then update to the new value.
    if (mInitialValue.isNothing()) {
      mInitialValue.emplace(mValue);
    }
    mValue = aNewValue;

    // We wait until things have stablized before sending state updates so that
    // we can avoid sending multiple updates, and possibly avoid sending any
    // updates at all if the value ends up where it started.
    if (!alreadyNotifying) {
      nsCOMPtr<nsIRunnable> r = NS_NewRunnableMethod(this, &Canonical::DoNotify);
      AbstractThread::GetCurrent()->TailDispatcher().AddDirectTask(r.forget());
    }

    return *this;
  }

  class Holder
  {
  public:
    Holder() {}
    ~Holder() { MOZ_DIAGNOSTIC_ASSERT(mCanonical, "Should have initialized me"); }

    // NB: Because mirror-initiated disconnection can race with canonical-
    // initiated disconnection, a canonical should never be reinitialized.
    void Init(AbstractThread* aThread, const T& aInitialValue, const char* aName)
    {
      mCanonical = new Canonical<T>(aThread, aInitialValue, aName);
    }

    // Forward control operations to the Canonical<T>.
    void DisconnectAll() { return mCanonical->DisconnectAll(); }

    // Access to the Canonical<T>.
    operator Canonical<T>&() { return *mCanonical; }
    Canonical<T>* operator&() { return mCanonical; }

    // Access to the T.
    const T& Ref() { return *mCanonical; }
    operator const T&() { return Ref(); }
    Holder& operator=(const T& aNewValue) { *mCanonical = aNewValue; return *this; }

  private:
    nsRefPtr<Canonical<T>> mCanonical;
  };

protected:
  ~Canonical() { MOZ_DIAGNOSTIC_ASSERT(mMirrors.IsEmpty()); }

private:
  void DoNotify()
  {
    MOZ_ASSERT(OwnerThread()->IsCurrentThreadIn());
    MOZ_ASSERT(mInitialValue.isSome());
    bool same = mInitialValue.ref() == mValue;
    mInitialValue.reset();

    if (same) {
      MIRROR_LOG("%s [%p] unchanged - not sending update", mName, this);
      return;
    }

    for (size_t i = 0; i < mMirrors.Length(); ++i) {
      OwnerThread()->TailDispatcher().AddStateChangeTask(mMirrors[i]->OwnerThread(), MakeNotifier(mMirrors[i]));
    }
  }

  already_AddRefed<nsIRunnable> MakeNotifier(AbstractMirror<T>* aMirror)
  {
    nsCOMPtr<nsIRunnable> r =
      NS_NewRunnableMethodWithArg<T>(aMirror, &AbstractMirror<T>::UpdateValue, mValue);
    return r.forget();
  }

  T mValue;
  Maybe<T> mInitialValue;
  nsTArray<nsRefPtr<AbstractMirror<T>>> mMirrors;
};

/*
 * Mirror<T> is a wrapper class that allows a given value to mirror that of a
 * Canonical<T> owned by another thread. It registers itself with a Canonical<T>,
 * and is periodically updated with new values. Mirror<T> is also a WatchTarget,
 * and may be set up to trigger other routines (on the same thread) when the
 * mirrored value changes.
 *
 * Do not instantiate a Mirror<T> directly as a member. Instead, instantiate a
 * Mirror<T>::Holder, which handles lifetime issues and whose destructor
 * initiates an asynchronous teardown of the reference-counted Mirror<T>,
 * breaking the inherent cycle between Mirror<T> and Canonical<T>.
 */
template<typename T>
class Mirror : public AbstractMirror<T>, public WatchTarget
{
public:
  using AbstractMirror<T>::OwnerThread;

  Mirror(AbstractThread* aThread, const T& aInitialValue, const char* aName)
    : AbstractMirror<T>(aThread), WatchTarget(aName), mValue(aInitialValue)
  {
    MIRROR_LOG("%s [%p] initialized", mName, this);
  }

  operator const T&()
  {
    MOZ_ASSERT(OwnerThread()->IsCurrentThreadIn());
    return mValue;
  }

  virtual void UpdateValue(const T& aNewValue) override
  {
    MOZ_ASSERT(OwnerThread()->IsCurrentThreadIn());
    if (mValue != aNewValue) {
      mValue = aNewValue;
      WatchTarget::NotifyWatchers();
    }
  }

  virtual void NotifyDisconnected() override
  {
    MIRROR_LOG("%s [%p] Notifed of disconnection from %p", mName, this, mCanonical.get());
    MOZ_ASSERT(OwnerThread()->IsCurrentThreadIn());
    mCanonical = nullptr;
  }

  bool IsConnected() const { return !!mCanonical; }

  void Connect(AbstractCanonical<T>* aCanonical)
  {
    MIRROR_LOG("%s [%p] Connecting to %p", mName, this, aCanonical);
    MOZ_ASSERT(OwnerThread()->IsCurrentThreadIn());
    MOZ_ASSERT(!IsConnected());

    nsCOMPtr<nsIRunnable> r = NS_NewRunnableMethodWithArg<StorensRefPtrPassByPtr<AbstractMirror<T>>>
                                (aCanonical, &AbstractCanonical<T>::AddMirror, this);
    aCanonical->OwnerThread()->Dispatch(r.forget());
    mCanonical = aCanonical;
  }

  void DisconnectIfConnected()
  {
    MOZ_ASSERT(OwnerThread()->IsCurrentThreadIn());
    if (!IsConnected()) {
      return;
    }

    MIRROR_LOG("%s [%p] Disconnecting from %p", mName, this, mCanonical.get());
    nsCOMPtr<nsIRunnable> r = NS_NewRunnableMethodWithArg<StorensRefPtrPassByPtr<AbstractMirror<T>>>
                                (mCanonical, &AbstractCanonical<T>::RemoveMirror, this);
    mCanonical->OwnerThread()->Dispatch(r.forget());
    mCanonical = nullptr;
  }

  class Holder
  {
  public:
    Holder() {}
    ~Holder()
    {
      MOZ_DIAGNOSTIC_ASSERT(mMirror, "Should have initialized me");
      mMirror->DisconnectIfConnected();
    }

    // NB: Because mirror-initiated disconnection can race with canonical-
    // initiated disconnection, a mirror should never be reinitialized.
    void Init(AbstractThread* aThread, const T& aInitialValue, const char* aName)
    {
      mMirror = new Mirror<T>(aThread, aInitialValue, aName);
    }

    // Forward control operations to the Mirror<T>.
    void Connect(AbstractCanonical<T>* aCanonical) { mMirror->Connect(aCanonical); }
    void DisconnectIfConnected() { mMirror->DisconnectIfConnected(); }

    // Access to the Mirror<T>.
    operator Mirror<T>&() { return *mMirror; }
    Mirror<T>* operator&() { return mMirror; }

    // Access to the T.
    const T& Ref() { return *mMirror; }
    operator const T&() { return Ref(); }

  private:
    nsRefPtr<Mirror<T>> mMirror;
  };

protected:
  ~Mirror() { MOZ_DIAGNOSTIC_ASSERT(!IsConnected()); }

private:
  T mValue;
  nsRefPtr<AbstractCanonical<T>> mCanonical;
};

#undef MIRROR_LOG

} // namespace mozilla

#endif
