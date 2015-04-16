/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(TaskDispatcher_h_)
#define TaskDispatcher_h_

#include "AbstractThread.h"

#include "mozilla/UniquePtr.h"
#include "mozilla/unused.h"

#include "nsISupportsImpl.h"
#include "nsTArray.h"
#include "nsThreadUtils.h"

namespace mozilla {

/*
 * A classic approach to cross-thread communication is to dispatch asynchronous
 * runnables to perform updates on other threads. This generally works well, but
 * there are sometimes reasons why we might want to delay the actual dispatch of
 * these tasks until a specified moment. At present, this is primarily useful to
 * ensure that mirrored state gets updated atomically - but there may be other
 * applications as well.
 *
 * TaskDispatcher is a general abstract class that accepts tasks and dispatches
 * them at some later point. These groups of tasks are per-target-thread, and
 * contain separate queues for two kinds of tasks - "state change tasks" (which
 * run first, and are intended to be used to update the value held by mirrors),
 * and regular tasks, which are other arbitrary operations that the are gated
 * to run after all the state changes have completed.
 */
class TaskDispatcher
{
public:
  TaskDispatcher() {}
  virtual ~TaskDispatcher() {}

  virtual void AddStateChangeTask(AbstractThread* aThread,
                                  already_AddRefed<nsIRunnable> aRunnable) = 0;
  virtual void AddTask(AbstractThread* aThread,
                       already_AddRefed<nsIRunnable> aRunnable,
                       AbstractThread::DispatchFailureHandling aFailureHandling = AbstractThread::AssertDispatchSuccess) = 0;

#ifdef DEBUG
  void AssertIsTailDispatcherIfRequired();
#else
  void AssertIsTailDispatcherIfRequired() {}
#endif
};

/*
 * AutoTaskDispatcher is a stack-scoped TaskDispatcher implementation that fires
 * its queued tasks when it is popped off the stack.
 */
class MOZ_STACK_CLASS AutoTaskDispatcher : public TaskDispatcher
{
public:
  AutoTaskDispatcher() {}
  ~AutoTaskDispatcher()
  {
    for (size_t i = 0; i < mTaskGroups.Length(); ++i) {
      UniquePtr<PerThreadTaskGroup> group(Move(mTaskGroups[i]));
      nsRefPtr<AbstractThread> thread = group->mThread;

      AbstractThread::DispatchFailureHandling failureHandling = group->mFailureHandling;
      nsCOMPtr<nsIRunnable> r = new TaskGroupRunnable(Move(group));
      thread->Dispatch(r.forget(), failureHandling);
    }
  }

  void AddStateChangeTask(AbstractThread* aThread,
                          already_AddRefed<nsIRunnable> aRunnable) override
  {
    EnsureTaskGroup(aThread).mStateChangeTasks.AppendElement(aRunnable);
  }

  void AddTask(AbstractThread* aThread,
               already_AddRefed<nsIRunnable> aRunnable,
               AbstractThread::DispatchFailureHandling aFailureHandling) override
  {
    PerThreadTaskGroup& group = EnsureTaskGroup(aThread);
    group.mRegularTasks.AppendElement(aRunnable);

    // The task group needs to assert dispatch success if any of the runnables
    // it's dispatching want to assert it.
    if (aFailureHandling == AbstractThread::AssertDispatchSuccess) {
      group.mFailureHandling = AbstractThread::AssertDispatchSuccess;
    }
  }

private:

  struct PerThreadTaskGroup
  {
  public:
    explicit PerThreadTaskGroup(AbstractThread* aThread)
      : mThread(aThread), mFailureHandling(AbstractThread::DontAssertDispatchSuccess)
    {
      MOZ_COUNT_CTOR(PerThreadTaskGroup);
    }

    ~PerThreadTaskGroup() { MOZ_COUNT_DTOR(PerThreadTaskGroup); }

    nsRefPtr<AbstractThread> mThread;
    nsTArray<nsCOMPtr<nsIRunnable>> mStateChangeTasks;
    nsTArray<nsCOMPtr<nsIRunnable>> mRegularTasks;
    AbstractThread::DispatchFailureHandling mFailureHandling;
  };

  class TaskGroupRunnable : public nsRunnable
  {
    public:
      explicit TaskGroupRunnable(UniquePtr<PerThreadTaskGroup>&& aTasks) : mTasks(Move(aTasks)) {}

      NS_IMETHODIMP Run()
      {
        for (size_t i = 0; i < mTasks->mStateChangeTasks.Length(); ++i) {
          mTasks->mStateChangeTasks[i]->Run();
        }

        for (size_t i = 0; i < mTasks->mRegularTasks.Length(); ++i) {
          mTasks->mRegularTasks[i]->Run();
        }

        return NS_OK;
      }

    private:
      UniquePtr<PerThreadTaskGroup> mTasks;
  };

  PerThreadTaskGroup& EnsureTaskGroup(AbstractThread* aThread)
  {
    for (size_t i = 0; i < mTaskGroups.Length(); ++i) {
      if (mTaskGroups[i]->mThread == aThread) {
        return *mTaskGroups[i];
      }
    }

    mTaskGroups.AppendElement(new PerThreadTaskGroup(aThread));
    return *mTaskGroups.LastElement();
  }

  // Task groups, organized by thread.
  nsTArray<UniquePtr<PerThreadTaskGroup>> mTaskGroups;
};

// Little utility class to allow declaring AutoTaskDispatcher as a default
// parameter for methods that take a TaskDispatcher&.
template<typename T>
class PassByRef
{
public:
  PassByRef() {}
  operator T&() { return mVal; }
private:
  T mVal;
};

} // namespace mozilla

#endif
