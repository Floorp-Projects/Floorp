/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AbstractThread.h"

#include "MediaTaskQueue.h"
#include "nsThreadUtils.h"

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/unused.h"

namespace mozilla {

StaticRefPtr<AbstractThread> sMainThread;
ThreadLocal<AbstractThread*> AbstractThread::sCurrentThreadTLS;

class XPCOMThreadWrapper : public AbstractThread
{
public:
  explicit XPCOMThreadWrapper(nsIThread* aTarget)
    : AbstractThread(/* aRequireTailDispatch = */ false)
    , mTarget(aTarget)
  {}

  virtual void Dispatch(already_AddRefed<nsIRunnable> aRunnable,
                        DispatchFailureHandling aFailureHandling = AssertDispatchSuccess) override
  {
    AssertInTailDispatchIfNeeded();
    nsCOMPtr<nsIRunnable> r = aRunnable;
    nsresult rv = mTarget->Dispatch(r, NS_DISPATCH_NORMAL);
    MOZ_DIAGNOSTIC_ASSERT(aFailureHandling == DontAssertDispatchSuccess || NS_SUCCEEDED(rv));
    unused << rv;
  }

  virtual bool IsCurrentThreadIn() override
  {
    bool in = NS_GetCurrentThread() == mTarget;
    MOZ_ASSERT_IF(in, GetCurrent() == this);
    return in;
  }

  virtual TaskDispatcher& TailDispatcher() override { MOZ_CRASH("Not implemented!"); }

  virtual bool InTailDispatch() override { MOZ_CRASH("Not implemented!"); }

private:
  nsRefPtr<nsIThread> mTarget;
};

void
AbstractThread::MaybeTailDispatch(already_AddRefed<nsIRunnable> aRunnable,
                                  DispatchFailureHandling aFailureHandling)
{
  AbstractThread* current = GetCurrent();
  if (current && current->RequiresTailDispatch()) {
    current->TailDispatcher().AddTask(this, Move(aRunnable), aFailureHandling);
  } else {
    Dispatch(Move(aRunnable), aFailureHandling);
  }
}


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
  sMainThread = new XPCOMThreadWrapper(mainThread.get());
  ClearOnShutdown(&sMainThread);

  if (!sCurrentThreadTLS.init()) {
    MOZ_CRASH();
  }
  sCurrentThreadTLS.set(sMainThread);
}

#ifdef DEBUG
void
TaskDispatcher::AssertIsTailDispatcherIfRequired()
{
  AbstractThread* current = AbstractThread::GetCurrent();
  MOZ_ASSERT_IF(current && current->RequiresTailDispatch(),
                this == &current->TailDispatcher());
}
#endif

} // namespace mozilla
