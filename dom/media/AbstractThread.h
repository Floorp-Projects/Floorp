/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(AbstractThread_h_)
#define AbstractThread_h_

#include "nscore.h"
#include "nsIRunnable.h"
#include "nsISupportsImpl.h"
#include "nsIThread.h"
#include "nsRefPtr.h"

namespace mozilla {

/*
 * We often want to run tasks on a target that guarantees that events will never
 * run in parallel. There are various target types that achieve this - namely
 * nsIThread and MediaTaskQueue. Note that nsIThreadPool (which implements
 * nsIEventTarget) does not have this property, so we do not want to use
 * nsIEventTarget for this purpose. This class encapsulates the specifics of
 * the structures we might use here and provides a consistent interface.
 *
 * At present, the supported AbstractThread implementations are MediaTaskQueue
 * and AbstractThread::MainThread. If you add support for another thread that is
 * not the MainThread, you'll need to figure out how to make it unique such that
 * comparing AbstractThread pointers is equivalent to comparing nsIThread pointers.
 */
class AbstractThread
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(AbstractThread);
  virtual nsresult Dispatch(already_AddRefed<nsIRunnable> aRunnable) = 0;
  virtual bool IsCurrentThreadIn() = 0;

  // Convenience method for dispatching a runnable when we may be running on
  // a thread that requires runnables to be dispatched with tail dispatch.
  void MaybeTailDispatch(already_AddRefed<nsIRunnable> aRunnable,
                         bool aAssertDispatchSuccess = true);

  // Convenience method for getting an AbstractThread for the main thread.
  static AbstractThread* MainThread();

  // Must be called exactly once during startup.
  static void InitStatics();

protected:
  virtual ~AbstractThread() {}
};

template<typename TargetType>
class AbstractThreadImpl : public AbstractThread
{
public:
  explicit AbstractThreadImpl(TargetType* aTarget) : mTarget(aTarget) {}
  virtual nsresult Dispatch(already_AddRefed<nsIRunnable> aRunnable);
  virtual bool IsCurrentThreadIn();
private:
  nsRefPtr<TargetType> mTarget;
};

} // namespace mozilla

#endif
