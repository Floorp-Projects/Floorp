/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GMPPlatform.h"
#include "GMPStorageChild.h"
#include "GMPTimerChild.h"
#include "mozilla/Monitor.h"
#include "GMPChild.h"
#include "mozilla/Mutex.h"
#include "base/thread.h"
#include "base/time.h"
#include "mozilla/ReentrantMonitor.h"

#include <ctime>

namespace mozilla::gmp {

static MessageLoop* sMainLoop = nullptr;
static GMPChild* sChild = nullptr;

static bool IsOnChildMainThread() {
  return sMainLoop && sMainLoop == MessageLoop::current();
}

// We just need a refcounted wrapper for GMPTask objects.
class GMPRunnable final {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(GMPRunnable)

  explicit GMPRunnable(GMPTask* aTask) : mTask(aTask) { MOZ_ASSERT(mTask); }

  void Run() {
    mTask->Run();
    mTask->Destroy();
    mTask = nullptr;
  }

 private:
  ~GMPRunnable() = default;

  GMPTask* mTask;
};

class GMPSyncRunnable final {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(GMPSyncRunnable)

  GMPSyncRunnable(GMPTask* aTask, MessageLoop* aMessageLoop)
      : mDone(false),
        mTask(aTask),
        mMessageLoop(aMessageLoop),
        mMonitor("GMPSyncRunnable") {
    MOZ_ASSERT(mTask);
    MOZ_ASSERT(mMessageLoop);
  }

  void Post() {
    // We assert here for two reasons.
    // 1) Nobody should be blocking the main thread.
    // 2) This prevents deadlocks when doing sync calls to main which if the
    //    main thread tries to do a sync call back to the calling thread.
    MOZ_ASSERT(!IsOnChildMainThread());

    mMessageLoop->PostTask(NewRunnableMethod("gmp::GMPSyncRunnable::Run", this,
                                             &GMPSyncRunnable::Run));
    MonitorAutoLock lock(mMonitor);
    while (!mDone) {
      lock.Wait();
    }
  }

  void Run() {
    mTask->Run();
    mTask->Destroy();
    mTask = nullptr;
    MonitorAutoLock lock(mMonitor);
    mDone = true;
    lock.Notify();
  }

 private:
  ~GMPSyncRunnable() = default;

  bool mDone;
  GMPTask* mTask;
  MessageLoop* mMessageLoop;
  Monitor mMonitor MOZ_UNANNOTATED;
};

class GMPThreadImpl : public GMPThread {
 public:
  GMPThreadImpl();
  virtual ~GMPThreadImpl();

  // GMPThread
  void Post(GMPTask* aTask) override;
  void Join() override;

 private:
  Mutex mMutex MOZ_UNANNOTATED;
  base::Thread mThread;
};

GMPErr CreateThread(GMPThread** aThread) {
  if (!aThread) {
    return GMPGenericErr;
  }

  *aThread = new GMPThreadImpl();

  return GMPNoErr;
}

GMPErr RunOnMainThread(GMPTask* aTask) {
  if (!aTask || !sMainLoop) {
    return GMPGenericErr;
  }

  RefPtr<GMPRunnable> r = new GMPRunnable(aTask);
  sMainLoop->PostTask(
      NewRunnableMethod("gmp::GMPRunnable::Run", r, &GMPRunnable::Run));

  return GMPNoErr;
}

GMPErr SyncRunOnMainThread(GMPTask* aTask) {
  if (!aTask || !sMainLoop || IsOnChildMainThread()) {
    return GMPGenericErr;
  }

  RefPtr<GMPSyncRunnable> r = new GMPSyncRunnable(aTask, sMainLoop);

  r->Post();

  return GMPNoErr;
}

class GMPMutexImpl : public GMPMutex {
 public:
  GMPMutexImpl();
  virtual ~GMPMutexImpl();

  // GMPMutex
  void Acquire() override;
  void Release() override;
  void Destroy() override;

 private:
  ReentrantMonitor mMonitor MOZ_UNANNOTATED;
};

GMPErr CreateMutex(GMPMutex** aMutex) {
  if (!aMutex) {
    return GMPGenericErr;
  }

  *aMutex = new GMPMutexImpl();

  return GMPNoErr;
}

GMPErr CreateRecord(const char* aRecordName, uint32_t aRecordNameSize,
                    GMPRecord** aOutRecord, GMPRecordClient* aClient) {
  if (aRecordNameSize > GMP_MAX_RECORD_NAME_SIZE || aRecordNameSize == 0) {
    NS_WARNING("GMP tried to CreateRecord with too long or 0 record name");
    return GMPGenericErr;
  }
  GMPStorageChild* storage = sChild->GetGMPStorage();
  if (!storage) {
    return GMPGenericErr;
  }
  MOZ_ASSERT(storage);
  return storage->CreateRecord(nsDependentCString(aRecordName, aRecordNameSize),
                               aOutRecord, aClient);
}

GMPErr SetTimerOnMainThread(GMPTask* aTask, int64_t aTimeoutMS) {
  if (!aTask || !sMainLoop || !IsOnChildMainThread()) {
    return GMPGenericErr;
  }
  GMPTimerChild* timers = sChild->GetGMPTimers();
  NS_ENSURE_TRUE(timers, GMPGenericErr);
  return timers->SetTimer(aTask, aTimeoutMS);
}

GMPErr GetClock(GMPTimestamp* aOutTime) {
  if (!aOutTime) {
    return GMPGenericErr;
  }
  *aOutTime = base::Time::Now().ToDoubleT() * 1000.0;
  return GMPNoErr;
}

void InitPlatformAPI(GMPPlatformAPI& aPlatformAPI, GMPChild* aChild) {
  if (!sMainLoop) {
    sMainLoop = MessageLoop::current();
  }
  if (!sChild) {
    sChild = aChild;
  }

  aPlatformAPI.version = 0;
  aPlatformAPI.createthread = &CreateThread;
  aPlatformAPI.runonmainthread = &RunOnMainThread;
  aPlatformAPI.syncrunonmainthread = &SyncRunOnMainThread;
  aPlatformAPI.createmutex = &CreateMutex;
  aPlatformAPI.createrecord = &CreateRecord;
  aPlatformAPI.settimer = &SetTimerOnMainThread;
  aPlatformAPI.getcurrenttime = &GetClock;
}

void SendFOGData(ipc::ByteBuf&& buf) {
  if (sChild) {
    sChild->SendFOGData(std::move(buf));
  }
}

GMPThreadImpl::GMPThreadImpl() : mMutex("GMPThreadImpl"), mThread("GMPThread") {
  MOZ_COUNT_CTOR(GMPThread);
}

GMPThreadImpl::~GMPThreadImpl() { MOZ_COUNT_DTOR(GMPThread); }

void GMPThreadImpl::Post(GMPTask* aTask) {
  MutexAutoLock lock(mMutex);

  if (!mThread.IsRunning()) {
    bool started = mThread.Start();
    if (!started) {
      NS_WARNING("Unable to start GMPThread!");
      return;
    }
  }

  RefPtr<GMPRunnable> r = new GMPRunnable(aTask);
  mThread.message_loop()->PostTask(
      NewRunnableMethod("gmp::GMPRunnable::Run", r.get(), &GMPRunnable::Run));
}

void GMPThreadImpl::Join() {
  {
    MutexAutoLock lock(mMutex);
    if (mThread.IsRunning()) {
      mThread.Stop();
    }
  }
  delete this;
}

GMPMutexImpl::GMPMutexImpl() : mMonitor("gmp-mutex") {
  MOZ_COUNT_CTOR(GMPMutexImpl);
}

GMPMutexImpl::~GMPMutexImpl() { MOZ_COUNT_DTOR(GMPMutexImpl); }

void GMPMutexImpl::Destroy() { delete this; }

MOZ_PUSH_IGNORE_THREAD_SAFETY
void GMPMutexImpl::Acquire() { mMonitor.Enter(); }

void GMPMutexImpl::Release() { mMonitor.Exit(); }
MOZ_POP_THREAD_SAFETY

GMPTask* NewGMPTask(std::function<void()>&& aFunction) {
  class Task : public GMPTask {
   public:
    explicit Task(std::function<void()>&& aFunction)
        : mFunction(std::move(aFunction)) {}
    void Destroy() override { delete this; }
    ~Task() override = default;
    void Run() override { mFunction(); }

   private:
    std::function<void()> mFunction;
  };
  return new Task(std::move(aFunction));
}

}  // namespace mozilla::gmp
