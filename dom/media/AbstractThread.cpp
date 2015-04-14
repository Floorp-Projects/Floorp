/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AbstractThread.h"

#include "MediaTaskQueue.h"
#include "nsThreadUtils.h"
#include "TaskDispatcher.h"

#include "nsIAppShell.h"
#include "nsWidgetsCID.h"
#include "nsServiceManagerUtils.h"

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Maybe.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/unused.h"

namespace mozilla {

StaticRefPtr<AbstractThread> sMainThread;
ThreadLocal<AbstractThread*> AbstractThread::sCurrentThreadTLS;

static NS_DEFINE_CID(kAppShellCID, NS_APPSHELL_CID);

class XPCOMThreadWrapper : public AbstractThread
{
public:
  explicit XPCOMThreadWrapper(nsIThread* aTarget, bool aRequireTailDispatch)
    : AbstractThread(aRequireTailDispatch)
    , mTarget(aTarget)
  {
    // Our current mechanism of implementing tail dispatch is appshell-specific.
    // This is because a very similar mechanism already exists on the main
    // thread, and we want to avoid making event dispatch on the main thread
    // more complicated than it already is.
    //
    // If you need to use tail dispatch on other XPCOM threads, you'll need to
    // implement an nsIThreadObserver to fire the tail dispatcher at the
    // appropriate times.
    MOZ_ASSERT_IF(aRequireTailDispatch,
                  NS_IsMainThread() && NS_GetCurrentThread() == aTarget);
  }

  virtual void Dispatch(already_AddRefed<nsIRunnable> aRunnable,
                        DispatchFailureHandling aFailureHandling = AssertDispatchSuccess,
                        DispatchReason aReason = NormalDispatch) override
  {
    nsCOMPtr<nsIRunnable> r = aRunnable;
    AbstractThread* currentThread;
    if (aReason != TailDispatch && (currentThread = GetCurrent()) && currentThread->RequiresTailDispatch()) {
      currentThread->TailDispatcher().AddTask(this, r.forget(), aFailureHandling);
      return;
    }

    nsresult rv = mTarget->Dispatch(r, NS_DISPATCH_NORMAL);
    MOZ_DIAGNOSTIC_ASSERT(aFailureHandling == DontAssertDispatchSuccess || NS_SUCCEEDED(rv));
    unused << rv;
  }

  virtual bool IsCurrentThreadIn() override
  {
    bool in = NS_GetCurrentThread() == mTarget;
    MOZ_ASSERT(in == (GetCurrent() == this));
    return in;
  }

  void FireTailDispatcher() { MOZ_ASSERT(mTailDispatcher.isSome()); mTailDispatcher.reset(); }

  virtual TaskDispatcher& TailDispatcher() override
  {
    MOZ_ASSERT(this == sMainThread); // See the comment in the constructor.
    MOZ_ASSERT(IsCurrentThreadIn());
    if (!mTailDispatcher.isSome()) {
      mTailDispatcher.emplace(/* aIsTailDispatcher = */ true);

      nsCOMPtr<nsIRunnable> event = NS_NewRunnableMethod(this, &XPCOMThreadWrapper::FireTailDispatcher);
      nsCOMPtr<nsIAppShell> appShell = do_GetService(kAppShellCID);
      appShell->RunInStableState(event);
    }

    return mTailDispatcher.ref();
  }

private:
  nsRefPtr<nsIThread> mTarget;
  Maybe<AutoTaskDispatcher> mTailDispatcher;
};

AbstractThread*
AbstractThread::MainThread()
{
  MOZ_ASSERT(sMainThread);
  return sMainThread;
}

void
AbstractThread::InitStatics()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!sMainThread);
  nsCOMPtr<nsIThread> mainThread;
  NS_GetMainThread(getter_AddRefs(mainThread));
  MOZ_DIAGNOSTIC_ASSERT(mainThread);
  sMainThread = new XPCOMThreadWrapper(mainThread.get(), /* aRequireTailDispatch = */ true);
  ClearOnShutdown(&sMainThread);

  if (!sCurrentThreadTLS.init()) {
    MOZ_CRASH();
  }
  sCurrentThreadTLS.set(sMainThread);
}

} // namespace mozilla
