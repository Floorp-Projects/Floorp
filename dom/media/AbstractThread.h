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

#include "mozilla/ThreadLocal.h"

namespace mozilla {

class MediaTaskQueue;
class TaskDispatcher;

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
  // Returns the AbstractThread that the caller is currently running in, or null
  // if the caller is not running in an AbstractThread.
  static AbstractThread* GetCurrent() { return sCurrentThreadTLS.get(); }

  AbstractThread(bool aSupportsTailDispatch) : mSupportsTailDispatch(aSupportsTailDispatch) {}

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(AbstractThread);

  enum DispatchFailureHandling { AssertDispatchSuccess, DontAssertDispatchSuccess };
  enum DispatchReason { NormalDispatch, TailDispatch };
  virtual void Dispatch(already_AddRefed<nsIRunnable> aRunnable,
                        DispatchFailureHandling aHandling = AssertDispatchSuccess,
                        DispatchReason aReason = NormalDispatch) = 0;

  virtual bool IsCurrentThreadIn() = 0;

  // Returns true if dispatch is generally reliable. This is used to guard
  // against FlushableMediaTaskQueues, which should go away.
  virtual bool IsDispatchReliable() { return true; }

  // Returns a TaskDispatcher that will dispatch its tasks when the currently-
  // running tasks pops off the stack.
  //
  // May only be called when running within the it is invoked up, and only on
  // threads which support it.
  virtual TaskDispatcher& TailDispatcher() = 0;

  // Returns true if this supports the tail dispatcher.
  bool SupportsTailDispatch() const { return mSupportsTailDispatch; }

  // Returns true if this thread requires all dispatches originating from
  // aThread go through the tail dispatcher.
  bool RequiresTailDispatch(AbstractThread* aThread) const;

  virtual MediaTaskQueue* AsTaskQueue() { MOZ_CRASH("Not a task queue!"); }
  virtual nsIThread* AsXPCOMThread() { MOZ_CRASH("Not an XPCOM thread!"); }

  // Convenience method for getting an AbstractThread for the main thread.
  static AbstractThread* MainThread();

  // Must be called exactly once during startup.
  static void InitStatics();

protected:
  virtual ~AbstractThread() {}
  static ThreadLocal<AbstractThread*> sCurrentThreadTLS;

  // True if we want to require that every task dispatched from tasks running in
  // this queue go through our queue's tail dispatcher.
  const bool mSupportsTailDispatch;
};

} // namespace mozilla

#endif
