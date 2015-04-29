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
 *       AbstractWatcher. Note that some watchers may themselves be watched,
 *       allowing watchers to be composed together.
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
  virtual ~AbstractWatcher() {}
  virtual void CustomDestroy() {}

private:
  // Only the holder is allowed to invoke Destroy().
  friend class WatcherHolder;
  void Destroy()
  {
    mDestroyed = true;
    CustomDestroy();
  }

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

/*
 * Watcher is the concrete AbstractWatcher implementation. It registers itself with
 * various WatchTargets, and accepts any number of callbacks that will be
 * invoked whenever any WatchTarget notifies of a change. It may also be watched
 * by other watchers.
 *
 * When a notification is received, a runnable is passed as "direct" to the
 * thread's tail dispatcher, which run directly (rather than via dispatch) when
 * the tail dispatcher fires. All subsequent notifications are ignored until the
 * runnable executes, triggering the updates and resetting the flags.
 */
class Watcher : public AbstractWatcher, public WatchTarget
{
public:
  explicit Watcher(const char* aName)
    : WatchTarget(aName), mNotifying(false) {}

  void Notify() override
  {
    if (mNotifying) {
      return;
    }
    mNotifying = true;

    // Queue up our notification jobs to run in a stable state.
    nsCOMPtr<nsIRunnable> r = NS_NewRunnableMethod(this, &Watcher::DoNotify);
    AbstractThread::GetCurrent()->TailDispatcher().AddDirectTask(r.forget());
  }

  void Watch(WatchTarget& aTarget) { aTarget.AddWatcher(this); }
  void Unwatch(WatchTarget& aTarget) { aTarget.RemoveWatcher(this); }

  template<typename ThisType>
  void AddWeakCallback(ThisType* aThisVal, void(ThisType::*aMethod)())
  {
    mCallbacks.AppendElement(NS_NewNonOwningRunnableMethod(aThisVal, aMethod));
  }

protected:
  void CustomDestroy() override { mCallbacks.Clear(); }

  void DoNotify()
  {
    MOZ_ASSERT(mNotifying);
    mNotifying = false;

    // Notify dependent watchers.
    NotifyWatchers();

    for (size_t i = 0; i < mCallbacks.Length(); ++i) {
      mCallbacks[i]->Run();
    }
  }

private:
  nsTArray<nsCOMPtr<nsIRunnable>> mCallbacks;

  bool mNotifying;
};

/*
 * WatcherHolder encapsulates a Watcher and handles lifetime issues. Use it to
 * holder watcher members.
 */
class WatcherHolder
{
public:
  explicit WatcherHolder(const char* aName) { mWatcher = new Watcher(aName); }
  operator Watcher*() { return mWatcher; }
  Watcher* operator->() { return mWatcher; }

  ~WatcherHolder()
  {
    mWatcher->Destroy();
    mWatcher = nullptr;
  }

private:
  nsRefPtr<Watcher> mWatcher;
};


#undef WATCH_LOG

} // namespace mozilla

#endif
