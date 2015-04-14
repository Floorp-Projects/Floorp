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

namespace mozilla {

StaticRefPtr<AbstractThread> sMainThread;

template<>
nsresult
AbstractThreadImpl<nsIThread>::Dispatch(already_AddRefed<nsIRunnable> aRunnable)
{
  MediaTaskQueue::AssertInTailDispatchIfNeeded();
  nsCOMPtr<nsIRunnable> r = aRunnable;
  return mTarget->Dispatch(r, NS_DISPATCH_NORMAL);
}

template<>
bool
AbstractThreadImpl<nsIThread>::IsCurrentThreadIn()
{
  bool in = NS_GetCurrentThread() == mTarget;
  MOZ_ASSERT_IF(in, MediaTaskQueue::GetCurrentQueue() == nullptr);
  return in;
}

void
AbstractThread::MaybeTailDispatch(already_AddRefed<nsIRunnable> aRunnable,
                                  bool aAssertDispatchSuccess)
{
  MediaTaskQueue* currentQueue = MediaTaskQueue::GetCurrentQueue();
  if (currentQueue && currentQueue->RequiresTailDispatch()) {
    currentQueue->TailDispatcher().AddTask(this, Move(aRunnable), aAssertDispatchSuccess);
  } else {
    nsresult rv = Dispatch(Move(aRunnable));
    MOZ_DIAGNOSTIC_ASSERT(!aAssertDispatchSuccess || NS_SUCCEEDED(rv));
    unused << rv;
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
  sMainThread = new AbstractThreadImpl<nsIThread>(mainThread.get());
  ClearOnShutdown(&sMainThread);
}

} // namespace mozilla
