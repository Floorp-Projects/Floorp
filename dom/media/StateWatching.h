/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(StateWatching_h_)
#define StateWatching_h_

#include "AbstractThread.h"

#include "mozilla/UniquePtr.h"
#include "mozilla/unused.h"

#include "nsISupportsImpl.h"

/*
 * The state-watching machinery automates the process of responding to changes
 * in various pieces of state.
 *
 * A standard programming pattern is as follows:
 *
 * mFoo = ...;
 * NotifyStuffChanged();
 * ...
 * mBar = ...;
 * NotifyStuffChanged();
 *
 * This pattern is error-prone and difficult to audit because it requires the
 * programmer to manually trigger the update routine. This can be especially
 * problematic when the update routine depends on numerous pieces of state, and
 * when that state is modified across a variety of helper methods. In these
 * cases the responsibility for invoking the routine is often unclear, causing
 * developers to scatter calls to it like pixie dust. This can result in
 * duplicate invocations (which is wasteful) and missing invocations in corner-
 * cases (which is a source of bugs).
 *
 * This file provides a set of primitives that automatically handle updates and
 * allow the programmers to explicitly construct a graph of state dependencies.
 * When used correctly, it eliminates the guess-work and wasted cycles described
 * above.
 *
 * There are two basic pieces:
 *   (1) Objects that can be watched for updates. These inherit WatchTarget.
 *   (2) Objects that receive objects and trigger processing. These inherit
 *       AbstractWatcher. In the current machinery, these exist only internally
 *       within the WatchManager, though that could change.
 *
 * Note that none of this machinery is thread-safe - it must all happen on the
 * same owning thread. To solve multi-threaded use-cases, use state mirroring
 * and watch the mirrored value.
 *
 * Given that semantics may change and comments tend to go out of date, we
 * deliberately don't provide usage examples here. Grep around to find them.
 */

namespace mozilla {

extern PRLogModuleInfo* gStateWatchingLog;

void EnsureStateWatchingLog();

#define WATCH_LOG(x, ...) \
  MOZ_ASSERT(gStateWatchingLog); \
  PR_LOG(gStateWatchingLog, PR_LOG_DEBUG, (x, ##__VA_ARGS__))

/*
 * AbstractWatcher is a superclass from which all watchers must inherit.
 */
class AbstractWatcher
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(AbstractWatcher)
  AbstractWatcher() : mDestroyed(false) {}
  bool IsDestroyed() { return mDestroyed; }
  virtual void Notify() = 0;

protected:
  virtual ~AbstractWatcher() { MOZ_ASSERT(mDestroyed); }
  bool mDestroyed;
};

/*
 * WatchTarget is a superclass from which all watchable things must inherit.
 * Unlike AbstractWatcher, it is a fully-implemented Mix-in, and the subclass
 * needs only to invoke NotifyWatchers when something changes.
 *
 * The functionality that this class provides is not threadsafe, and should only
 * be used on the thread that owns that WatchTarget.
 */
class WatchTarget
{
public:
  explicit WatchTarget(const char* aName) : mName(aName) {}

  void AddWatcher(AbstractWatcher* aWatcher)
  {
    MOZ_ASSERT(!mWatchers.Contains(aWatcher));
    mWatchers.AppendElement(aWatcher);
  }

  void RemoveWatcher(AbstractWatcher* aWatcher)
  {
    MOZ_ASSERT(mWatchers.Contains(aWatcher));
    mWatchers.RemoveElement(aWatcher);
  }

protected:
  void NotifyWatchers()
  {
    WATCH_LOG("%s[%p] notifying watchers\n", mName, this);
    PruneWatchers();
    for (size_t i = 0; i < mWatchers.Length(); ++i) {
      mWatchers[i]->Notify();
    }
  }

private:
  // We don't have Watchers explicitly unregister themselves when they die,
  // because then they'd need back-references to all the WatchTargets they're
  // subscribed to, and WatchTargets aren't reference-counted. So instead we
  // just prune dead ones at appropriate times, which works just fine.
  void PruneWatchers()
  {
    for (int i = mWatchers.Length() - 1; i >= 0; --i) {
      if (mWatchers[i]->IsDestroyed()) {
        mWatchers.RemoveElementAt(i);
      }
    }
  }

  nsTArray<nsRefPtr<AbstractWatcher>> mWatchers;

protected:
  const char* mName;
};

/*
 * Watchable is a wrapper class that turns any primitive into a WatchTarget.
 */
template<typename T>
class Watchable : public WatchTarget
{
public:
  Watchable(const T& aInitialValue, const char* aName)
    : WatchTarget(aName), mValue(aInitialValue) {}

  operator const T&() const { return mValue; }
  Watchable& operator=(const T& aNewValue)
  {
    if (aNewValue != mValue) {
      mValue = aNewValue;
      NotifyWatchers();
    }

    return *this;
  }

private:
  Watchable(const Watchable& aOther); // Not implemented
  Watchable& operator=(const Watchable& aOther); // Not implemented

  T mValue;
};

// Manager class for state-watching. Declare one of these in any class for which
// you want to invoke method callbacks.
//
// Internally, WatchManager maintains one AbstractWatcher per callback method.
// Consumers invoke Watch/Unwatch on a particular (WatchTarget, Callback) tuple.
// This causes an AbstractWatcher for |Callback| to be instantiated if it doesn't
// already exist, and registers it with |WatchTarget|.
//
// Using Direct Tasks on the TailDispatcher, WatchManager ensures that we fire
// watch callbacks no more than once per task, once all other operations for that
// task have been completed.
//
// WatchManager<OwnerType> is intended to be declared as a member of |OwnerType|
// objects. Given that, it and its owned objects can't hold permanent strong refs to
// the owner, since that would keep the owner alive indefinitely. Instead, it
// _only_ holds strong refs while waiting for Direct Tasks to fire. This ensures
// that everything is kept alive just long enough.
template <typename OwnerType>
class WatchManager
{
public:
  typedef void(OwnerType::*CallbackMethod)();
  explicit WatchManager(OwnerType* aOwner)
    : mOwner(aOwner) {}

  ~WatchManager()
  {
    for (size_t i = 0; i < mWatchers.Length(); ++i) {
      mWatchers[i]->Destroy();
    }
  }

  void Watch(WatchTarget& aTarget, CallbackMethod aMethod)
  {
    aTarget.AddWatcher(&EnsureWatcher(aMethod));
  }

  void Unwatch(WatchTarget& aTarget, CallbackMethod aMethod)
  {
    PerCallbackWatcher* watcher = GetWatcher(aMethod);
    MOZ_ASSERT(watcher);
    aTarget.RemoveWatcher(watcher);
  }

  void ManualNotify(CallbackMethod aMethod)
  {
    PerCallbackWatcher* watcher = GetWatcher(aMethod);
    MOZ_ASSERT(watcher);
    watcher->Notify();
  }

private:
  class PerCallbackWatcher : public AbstractWatcher
  {
  public:
    PerCallbackWatcher(OwnerType* aOwner, CallbackMethod aMethod)
      : mOwner(aOwner), mCallbackMethod(aMethod) {}

    void Destroy()
    {
      mDestroyed = true;
      mOwner = nullptr;
    }

    void Notify() override
    {
      MOZ_DIAGNOSTIC_ASSERT(mOwner, "mOwner is only null after destruction, "
                                    "at which point we shouldn't be notified");
      if (mStrongRef) {
        // We've already got a notification job in the pipe.
        return;
      }
      mStrongRef = mOwner; // Hold the owner alive while notifying.

      // Queue up our notification jobs to run in a stable state.
      nsCOMPtr<nsIRunnable> r = NS_NewRunnableMethod(this, &PerCallbackWatcher::DoNotify);
      AbstractThread::GetCurrent()->TailDispatcher().AddDirectTask(r.forget());
    }

    bool CallbackMethodIs(CallbackMethod aMethod) const
    {
      return mCallbackMethod == aMethod;
    }

  private:
    ~PerCallbackWatcher() {}

    void DoNotify()
    {
      MOZ_ASSERT(mStrongRef);
      nsRefPtr<OwnerType> ref = mStrongRef.forget();
      ((*ref).*mCallbackMethod)();
    }

    OwnerType* mOwner; // Never null.
    nsRefPtr<OwnerType> mStrongRef; // Only non-null when notifying.
    CallbackMethod mCallbackMethod;
  };

  PerCallbackWatcher* GetWatcher(CallbackMethod aMethod)
  {
    for (size_t i = 0; i < mWatchers.Length(); ++i) {
      if (mWatchers[i]->CallbackMethodIs(aMethod)) {
        return mWatchers[i];
      }
    }
    return nullptr;
  }

  PerCallbackWatcher& EnsureWatcher(CallbackMethod aMethod)
  {
    PerCallbackWatcher* watcher = GetWatcher(aMethod);
    if (watcher) {
      return *watcher;
    }
    watcher = mWatchers.AppendElement(new PerCallbackWatcher(mOwner, aMethod))->get();
    return *watcher;
  }

  nsTArray<nsRefPtr<PerCallbackWatcher>> mWatchers;
  OwnerType* mOwner;
};

#undef WATCH_LOG

} // namespace mozilla

#endif
