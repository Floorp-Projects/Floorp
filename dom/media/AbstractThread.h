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
 * Use AbstractThread::Create() to instantiate an AbstractThread. Note that
 * if you use different types than the ones currently supported (MediaTaskQueue
 * and nsIThread), you'll need to implement the relevant guts in
 * AbstractThread.cpp to avoid linkage errors.
 */
class AbstractThread
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(AbstractThread);
  virtual nsresult Dispatch(already_AddRefed<nsIRunnable> aRunnable) = 0;
  virtual bool IsCurrentThreadIn() = 0;

  template<typename TargetType> static AbstractThread* Create(TargetType* aTarget);

  // Convenience method for getting an AbstractThread for the main thread.
  //
  // EnsureMainThreadSingleton must be called on the main thread before any
  // other threads that might use MainThread() are spawned.
  static AbstractThread* MainThread();
  static void EnsureMainThreadSingleton();

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

template<typename TargetType>
AbstractThread*
AbstractThread::Create(TargetType* aTarget)
{
  return new AbstractThreadImpl<TargetType>(aTarget);
};

} // namespace mozilla

#endif
