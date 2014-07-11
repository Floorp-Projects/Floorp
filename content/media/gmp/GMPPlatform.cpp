/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GMPPlatform.h"
#include "mozilla/Monitor.h"
#include "nsAutoPtr.h"

namespace mozilla {
namespace gmp {

static MessageLoop* sMainLoop = nullptr;

// We just need a refcounted wrapper for GMPTask objects.
class Runnable MOZ_FINAL
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(Runnable)

  Runnable(GMPTask* aTask)
  : mTask(aTask)
  {
    MOZ_ASSERT(mTask);
  }

  void Run()
  {
    mTask->Run();
    mTask->Destroy();
    mTask = nullptr;
  }

private:
  ~Runnable()
  {
  }

  GMPTask* mTask;
};

class SyncRunnable MOZ_FINAL
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(SyncRunnable)

  SyncRunnable(GMPTask* aTask, MessageLoop* aMessageLoop)
  : mDone(false)
  , mTask(aTask)
  , mMessageLoop(aMessageLoop)
  , mMonitor("GMPSyncRunnable")
  {
    MOZ_ASSERT(mTask);
    MOZ_ASSERT(mMessageLoop);
  }

  void Post()
  {
    // We assert here for two reasons.
    // 1) Nobody should be blocking the main thread.
    // 2) This prevents deadlocks when doing sync calls to main which if the
    //    main thread tries to do a sync call back to the calling thread.
    MOZ_ASSERT(MessageLoop::current() != sMainLoop);

    mMessageLoop->PostTask(FROM_HERE, NewRunnableMethod(this, &SyncRunnable::Run));
    MonitorAutoLock lock(mMonitor);
    while (!mDone) {
      lock.Wait();
    }
  }

  void Run()
  {
    mTask->Run();
    mTask->Destroy();
    mTask = nullptr;
    MonitorAutoLock lock(mMonitor);
    mDone = true;
    lock.Notify();
  }

private:
  ~SyncRunnable()
  {
  }

  bool mDone;
  GMPTask* mTask;
  MessageLoop* mMessageLoop;
  Monitor mMonitor;
};

GMPErr
CreateThread(GMPThread** aThread)
{
  if (!aThread) {
    return GMPGenericErr;
  }

  *aThread = new GMPThreadImpl();

  return GMPNoErr;
}

GMPErr
RunOnMainThread(GMPTask* aTask)
{
  if (!aTask || !sMainLoop) {
    return GMPGenericErr;
  }

  nsRefPtr<Runnable> r = new Runnable(aTask);
  sMainLoop->PostTask(FROM_HERE, NewRunnableMethod(r.get(), &Runnable::Run));

  return GMPNoErr;
}

GMPErr
SyncRunOnMainThread(GMPTask* aTask)
{
  if (!aTask || !sMainLoop || sMainLoop == MessageLoop::current()) {
    return GMPGenericErr;
  }

  nsRefPtr<SyncRunnable> r = new SyncRunnable(aTask, sMainLoop);

  r->Post();

  return GMPNoErr;
}

GMPErr
CreateMutex(GMPMutex** aMutex)
{
  if (!aMutex) {
    return GMPGenericErr;
  }

  *aMutex = new GMPMutexImpl();

  return GMPNoErr;
}

void
InitPlatformAPI(GMPPlatformAPI& aPlatformAPI)
{
  if (!sMainLoop) {
    sMainLoop = MessageLoop::current();
  }

  aPlatformAPI.version = 0;
  aPlatformAPI.createthread = &CreateThread;
  aPlatformAPI.runonmainthread = &RunOnMainThread;
  aPlatformAPI.syncrunonmainthread = &SyncRunOnMainThread;
  aPlatformAPI.createmutex = &CreateMutex;
  aPlatformAPI.createrecord = nullptr;
  aPlatformAPI.settimer = nullptr;
  aPlatformAPI.getcurrenttime = nullptr;
}

GMPThreadImpl::GMPThreadImpl()
: mMutex("GMPThreadImpl"),
  mThread("GMPThread")
{
}

GMPThreadImpl::~GMPThreadImpl()
{
}

void
GMPThreadImpl::Post(GMPTask* aTask)
{
  MutexAutoLock lock(mMutex);

  if (!mThread.IsRunning()) {
    bool started = mThread.Start();
    if (!started) {
      NS_WARNING("Unable to start GMPThread!");
      return;
    }
  }

  nsRefPtr<Runnable> r = new Runnable(aTask);

  mThread.message_loop()->PostTask(FROM_HERE, NewRunnableMethod(r.get(), &Runnable::Run));
}

void
GMPThreadImpl::Join()
{
  MutexAutoLock lock(mMutex);
  if (mThread.IsRunning()) {
    mThread.Stop();
  }
}

GMPMutexImpl::GMPMutexImpl()
: mMutex("gmp-mutex")
{
}

GMPMutexImpl::~GMPMutexImpl()
{
}

void
GMPMutexImpl::Acquire()
{
  mMutex.Lock();
}

void
GMPMutexImpl::Release()
{
  mMutex.Unlock();
}

} // namespace gmp
} // namespace mozilla
