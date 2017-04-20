/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_glue_MessageLoopAbstractThreadWrapper_h
#define mozilla_ipc_glue_MessageLoopAbstractThreadWrapper_h

#include "mozilla/AbstractThread.h"

#include "base/message_loop.h"

#include "nsThreadUtils.h"

namespace mozilla {
namespace ipc {

class MessageLoopAbstractThreadWrapper : public AbstractThread
{
public:
  static already_AddRefed<AbstractThread>
  Create(MessageLoop* aMessageLoop)
  {
    RefPtr<MessageLoopAbstractThreadWrapper> wrapper =
      new MessageLoopAbstractThreadWrapper(aMessageLoop);

    bool onCurrentThread = (aMessageLoop == MessageLoop::current());

    if (onCurrentThread) {
      sCurrentThreadTLS.set(wrapper);
      return wrapper.forget();
    }

    // Set the thread-local sCurrentThreadTLS to point to the wrapper on the
    // target thread. This ensures that sCurrentThreadTLS is as expected by
    // AbstractThread::GetCurrent() on the target thread.
    RefPtr<Runnable> r =
      NS_NewRunnableFunction([wrapper]() { sCurrentThreadTLS.set(wrapper); });
    aMessageLoop->PostTask(r.forget());
    return wrapper.forget();
  }

  virtual void Dispatch(already_AddRefed<nsIRunnable> aRunnable,
                        DispatchFailureHandling aFailureHandling = AssertDispatchSuccess,
                        DispatchReason aReason = NormalDispatch) override
  {
    MOZ_RELEASE_ASSERT(aReason == NormalDispatch, "Only supports NormalDispatch");

    RefPtr<Runnable> runner(new Runner(this, Move(aRunnable)));
    mMessageLoop->PostTask(runner.forget());
  }

  virtual bool IsCurrentThreadIn() override
  {
    MessageLoop* messageLoop = MessageLoop::current();
    bool in = (mMessageLoop == messageLoop);
    return in;
  }

  virtual TaskDispatcher& TailDispatcher() override
  {
    MOZ_CRASH("Not supported!");
    TaskDispatcher* dispatcher = nullptr;
    return *dispatcher;
  }

  virtual bool MightHaveTailTasks() override
  {
    return false;
  }
private:
  explicit MessageLoopAbstractThreadWrapper(MessageLoop* aMessageLoop)
    : AbstractThread(false)
    , mMessageLoop(aMessageLoop)
  {
  }

  MessageLoop* mMessageLoop;

  class Runner : public CancelableRunnable {
    class MOZ_STACK_CLASS AutoTaskGuard final {
    public:
      explicit AutoTaskGuard(MessageLoopAbstractThreadWrapper* aThread)
        : mLastCurrentThread(nullptr)
      {
        MOZ_ASSERT(aThread);
        mLastCurrentThread = sCurrentThreadTLS.get();
        sCurrentThreadTLS.set(aThread);
      }

      ~AutoTaskGuard()
      {
        sCurrentThreadTLS.set(mLastCurrentThread);
      }
    private:
      AbstractThread* mLastCurrentThread;
    };

  public:
    explicit Runner(MessageLoopAbstractThreadWrapper* aThread,
                    already_AddRefed<nsIRunnable> aRunnable)
      : mThread(aThread)
      , mRunnable(aRunnable)
    {
    }

    NS_IMETHOD Run() override
    {
      AutoTaskGuard taskGuard(mThread);

      MOZ_ASSERT(mThread == AbstractThread::GetCurrent());
      MOZ_ASSERT(mThread->IsCurrentThreadIn());
      nsresult rv = mRunnable->Run();

      return rv;
    }

    nsresult Cancel() override
    {
      // Set the TLS during Cancel() just in case it calls Run().
      AutoTaskGuard taskGuard(mThread);

      nsresult rv = NS_OK;

      // Try to cancel the runnable if it implements the right interface.
      // Otherwise just skip the runnable.
      nsCOMPtr<nsICancelableRunnable> cr = do_QueryInterface(mRunnable);
      if (cr) {
        rv = cr->Cancel();
      }

      return rv;
    }

    NS_IMETHOD GetName(nsACString& aName) override
    {
      aName.AssignLiteral("AbstractThread::Runner");
      if (nsCOMPtr<nsINamed> named = do_QueryInterface(mRunnable)) {
        nsAutoCString name;
        named->GetName(name);
        if (!name.IsEmpty()) {
          aName.AppendLiteral(" for ");
          aName.Append(name);
        }
      }
      return NS_OK;
    }

  private:
    RefPtr<MessageLoopAbstractThreadWrapper> mThread;
    RefPtr<nsIRunnable> mRunnable;
  };
};

} // namespace ipc
} // namespace mozilla

#endif // mozilla_ipc_glue_MessageLoopAbstractThreadWrapper_h
