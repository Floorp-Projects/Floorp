/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/task.h"                  // for NewRunnableFunction, etc
#include "base/thread.h"                // for Thread
#include "base/tracked.h"               // for FROM_HERE
#include "mozilla/layers/SharedBufferManagerChild.h"
#include "mozilla/layers/SharedBufferManagerParent.h"
#include "mozilla/StaticPtr.h"          // for StaticRefPtr
#include "mozilla/ReentrantMonitor.h"   // for ReentrantMonitor, etc
#include "nsThreadUtils.h"              // fo NS_IsMainThread

#ifdef MOZ_NUWA_PROCESS
#include "ipc/Nuwa.h"
#endif

#ifdef MOZ_WIDGET_GONK
#define LOG(args...) __android_log_print(ANDROID_LOG_INFO, "SBMChild", ## args)
#endif

namespace mozilla {
namespace layers {

using namespace mozilla::gfx;

// Singleton
SharedBufferManagerChild* SharedBufferManagerChild::sSharedBufferManagerChildSingleton = nullptr;
SharedBufferManagerParent* SharedBufferManagerChild::sSharedBufferManagerParentSingleton = nullptr;
base::Thread* SharedBufferManagerChild::sSharedBufferManagerChildThread = nullptr;

SharedBufferManagerChild::SharedBufferManagerChild()
#ifdef MOZ_HAVE_SURFACEDESCRIPTORGRALLOC
  : mBufferMutex("BufferMonitor")
#endif
{
}

static bool
InSharedBufferManagerChildThread()
{
  return SharedBufferManagerChild::sSharedBufferManagerChildThread->thread_id() == PlatformThread::CurrentId();
}

// dispatched function
static void
DeleteSharedBufferManagerSync(ReentrantMonitor *aBarrier, bool *aDone)
{
  ReentrantMonitorAutoEnter autoMon(*aBarrier);

  NS_ABORT_IF_FALSE(InSharedBufferManagerChildThread(),
                    "Should be in SharedBufferManagerChild thread.");
  SharedBufferManagerChild::sSharedBufferManagerChildSingleton = nullptr;
  SharedBufferManagerChild::sSharedBufferManagerParentSingleton = nullptr;
  *aDone = true;
  aBarrier->NotifyAll();
}

// dispatched function
static void
ConnectSharedBufferManager(SharedBufferManagerChild *child, SharedBufferManagerParent *parent)
{
  MessageLoop *parentMsgLoop = parent->GetMessageLoop();
  ipc::MessageChannel *parentChannel = parent->GetIPCChannel();
  child->Open(parentChannel, parentMsgLoop, mozilla::ipc::ChildSide);
}

base::Thread*
SharedBufferManagerChild::GetThread() const
{
  return sSharedBufferManagerChildThread;
}

SharedBufferManagerChild*
SharedBufferManagerChild::GetSingleton()
{
  return sSharedBufferManagerChildSingleton;
}

bool
SharedBufferManagerChild::IsCreated()
{
  return GetSingleton() != nullptr;
}

void
SharedBufferManagerChild::StartUp()
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on the main Thread!");
  SharedBufferManagerChild::StartUpOnThread(new base::Thread("BufferMgrChild"));
}

static void
ConnectSharedBufferManagerInChildProcess(mozilla::ipc::Transport* aTransport,
                                         base::ProcessHandle aOtherProcess)
{
  // Bind the IPC channel to the shared buffer manager thread.
  SharedBufferManagerChild::sSharedBufferManagerChildSingleton->Open(aTransport, aOtherProcess,
                                                                     XRE_GetIOMessageLoop(),
                                                                     ipc::ChildSide);

#ifdef MOZ_NUWA_PROCESS
  if (IsNuwaProcess()) {
    SharedBufferManagerChild::sSharedBufferManagerChildThread
      ->message_loop()->PostTask(FROM_HERE,
                                 NewRunnableFunction(NuwaMarkCurrentThread,
                                                     (void (*)(void *))nullptr,
                                                     (void *)nullptr));
  }
#endif
}

PSharedBufferManagerChild*
SharedBufferManagerChild::StartUpInChildProcess(Transport* aTransport,
                                                base::ProcessId aOtherProcess)
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on the main Thread!");

  ProcessHandle processHandle;
  if (!base::OpenProcessHandle(aOtherProcess, &processHandle)) {
    return nullptr;
  }

  sSharedBufferManagerChildThread = new base::Thread("BufferMgrChild");
  if (!sSharedBufferManagerChildThread->Start()) {
    return nullptr;
  }

  sSharedBufferManagerChildSingleton = new SharedBufferManagerChild();
  sSharedBufferManagerChildSingleton->GetMessageLoop()->PostTask(
    FROM_HERE,
    NewRunnableFunction(ConnectSharedBufferManagerInChildProcess,
                        aTransport, processHandle));

  return sSharedBufferManagerChildSingleton;
}

void
SharedBufferManagerChild::ShutDown()
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on the main Thread!");
  if (IsCreated()) {
    SharedBufferManagerChild::DestroyManager();
    delete sSharedBufferManagerChildThread;
    sSharedBufferManagerChildThread = nullptr;
  }
}

bool
SharedBufferManagerChild::StartUpOnThread(base::Thread* aThread)
{
  NS_ABORT_IF_FALSE(aThread, "SharedBufferManager needs a thread.");
  if (sSharedBufferManagerChildSingleton != nullptr) {
    return false;
  }

  sSharedBufferManagerChildThread = aThread;
  if (!aThread->IsRunning()) {
    aThread->Start();
  }
  sSharedBufferManagerChildSingleton = new SharedBufferManagerChild();
  char thrname[128];
  base::snprintf(thrname, 128, "BufMgrParent#%d", base::Process::Current().pid());
  sSharedBufferManagerParentSingleton = new SharedBufferManagerParent(
    nullptr, base::Process::Current().pid(), new base::Thread(thrname));
  sSharedBufferManagerChildSingleton->ConnectAsync(sSharedBufferManagerParentSingleton);
  return true;
}

void
SharedBufferManagerChild::DestroyManager()
{
  NS_ABORT_IF_FALSE(!InSharedBufferManagerChildThread(),
                    "This method must not be called in this thread.");
  // ...because we are about to dispatch synchronous messages to the
  // BufferManagerChild thread.

  if (!IsCreated()) {
    return;
  }

  ReentrantMonitor barrier("BufferManagerDestroyTask lock");
  ReentrantMonitorAutoEnter autoMon(barrier);

  bool done = false;
  sSharedBufferManagerChildSingleton->GetMessageLoop()->PostTask(FROM_HERE,
    NewRunnableFunction(&DeleteSharedBufferManagerSync, &barrier, &done));
  while (!done) {
    barrier.Wait();
  }

}

MessageLoop *
SharedBufferManagerChild::GetMessageLoop() const
{
  return sSharedBufferManagerChildThread != nullptr ?
      sSharedBufferManagerChildThread->message_loop() :
      nullptr;
}

void
SharedBufferManagerChild::ConnectAsync(SharedBufferManagerParent* aParent)
{
  GetMessageLoop()->PostTask(FROM_HERE, NewRunnableFunction(&ConnectSharedBufferManager,
                                                            this, aParent));
}

// dispatched function
void
SharedBufferManagerChild::AllocGrallocBufferSync(const GrallocParam& aParam,
                                                 Monitor* aBarrier,
                                                 bool* aDone)
{
  MonitorAutoLock autoMon(*aBarrier);

  sSharedBufferManagerChildSingleton->AllocGrallocBufferNow(aParam.size,
                                                            aParam.format,
                                                            aParam.usage,
                                                            aParam.buffer);
  *aDone = true;
  aBarrier->NotifyAll();
}

// dispatched function
void
SharedBufferManagerChild::DeallocGrallocBufferSync(const mozilla::layers::MaybeMagicGrallocBufferHandle& aBuffer)
{
  SharedBufferManagerChild::sSharedBufferManagerChildSingleton->
    DeallocGrallocBufferNow(aBuffer);
}

bool
SharedBufferManagerChild::AllocGrallocBuffer(const gfx::IntSize& aSize,
                                             const uint32_t& aFormat,
                                             const uint32_t& aUsage,
                                             mozilla::layers::MaybeMagicGrallocBufferHandle* aBuffer)
{
  if (InSharedBufferManagerChildThread()) {
    return SharedBufferManagerChild::AllocGrallocBufferNow(aSize, aFormat, aUsage, aBuffer);
  }

  Monitor barrier("AllocSurfaceDescriptorGralloc Lock");
  MonitorAutoLock autoMon(barrier);
  bool done = false;

  GetMessageLoop()->PostTask(
    FROM_HERE,
    NewRunnableFunction(&AllocGrallocBufferSync,
                        GrallocParam(aSize, aFormat, aUsage, aBuffer), &barrier, &done));

  while (!done) {
    barrier.Wait();
  }
  return true;
}

bool
SharedBufferManagerChild::AllocGrallocBufferNow(const IntSize& aSize,
                                                const uint32_t& aFormat,
                                                const uint32_t& aUsage,
                                                mozilla::layers::MaybeMagicGrallocBufferHandle* aHandle)
{
#ifdef MOZ_HAVE_SURFACEDESCRIPTORGRALLOC
  mozilla::layers::MaybeMagicGrallocBufferHandle handle;
  SendAllocateGrallocBuffer(aSize, aFormat, aUsage, &handle);
  if (handle.type() != mozilla::layers::MaybeMagicGrallocBufferHandle::TMagicGrallocBufferHandle) {
    return false;
  }
  *aHandle = handle.get_MagicGrallocBufferHandle().mRef;

  {
    MutexAutoLock lock(mBufferMutex);
    mBuffers[handle.get_MagicGrallocBufferHandle().mRef.mKey] = handle.get_MagicGrallocBufferHandle().mGraphicBuffer;
  }
  return true;
#else
  NS_RUNTIMEABORT("No GrallocBuffer for you");
  return true;
#endif
}

void
SharedBufferManagerChild::DeallocGrallocBuffer(const mozilla::layers::MaybeMagicGrallocBufferHandle& aBuffer)
{
#ifdef MOZ_HAVE_SURFACEDESCRIPTORGRALLOC
  NS_ASSERTION(aBuffer.type() != mozilla::layers::MaybeMagicGrallocBufferHandle::TMagicGrallocBufferHandle, "We shouldn't try to do IPC with real buffer");
  if (aBuffer.type() != mozilla::layers::MaybeMagicGrallocBufferHandle::TGrallocBufferRef) {
    return;
  }
#endif

  if (InSharedBufferManagerChildThread()) {
    return SharedBufferManagerChild::DeallocGrallocBufferNow(aBuffer);
  }

  GetMessageLoop()->PostTask(FROM_HERE, NewRunnableFunction(&DeallocGrallocBufferSync,
                                                            aBuffer));
}

void
SharedBufferManagerChild::DeallocGrallocBufferNow(const mozilla::layers::MaybeMagicGrallocBufferHandle& aBuffer)
{
#ifdef MOZ_HAVE_SURFACEDESCRIPTORGRALLOC
  NS_ASSERTION(aBuffer.type() != mozilla::layers::MaybeMagicGrallocBufferHandle::TMagicGrallocBufferHandle, "We shouldn't try to do IPC with real buffer");

  {
    MutexAutoLock lock(mBufferMutex);
    mBuffers.erase(aBuffer.get_GrallocBufferRef().mKey);
  }
  SendDropGrallocBuffer(aBuffer);
#else
  NS_RUNTIMEABORT("No GrallocBuffer for you");
#endif
}

void
SharedBufferManagerChild::DropGrallocBuffer(const mozilla::layers::MaybeMagicGrallocBufferHandle& aHandle)
{
#ifdef MOZ_HAVE_SURFACEDESCRIPTORGRALLOC
  int64_t bufferKey = -1;
  if (aHandle.type() == mozilla::layers::MaybeMagicGrallocBufferHandle::TMagicGrallocBufferHandle) {
    bufferKey = aHandle.get_MagicGrallocBufferHandle().mRef.mKey;
  } else if (aHandle.type() == mozilla::layers::MaybeMagicGrallocBufferHandle::TGrallocBufferRef) {
    bufferKey = aHandle.get_GrallocBufferRef().mKey;
  } else {
    return;
  }

  {
    MutexAutoLock lock(mBufferMutex);
    mBuffers.erase(bufferKey);
  }
#endif
}

bool SharedBufferManagerChild::RecvDropGrallocBuffer(const mozilla::layers::MaybeMagicGrallocBufferHandle& aHandle)
{
  DropGrallocBuffer(aHandle);
  return true;
}

#ifdef MOZ_HAVE_SURFACEDESCRIPTORGRALLOC
android::sp<android::GraphicBuffer>
SharedBufferManagerChild::GetGraphicBuffer(int64_t key)
{
  MutexAutoLock lock(mBufferMutex);
  if (mBuffers.count(key) == 0) {
    printf_stderr("SharedBufferManagerChild::GetGraphicBuffer -- invalid key");
    return nullptr;
  }
  return mBuffers[key];
}
#endif

bool
SharedBufferManagerChild::IsValidKey(int64_t key)
{
#ifdef MOZ_HAVE_SURFACEDESCRIPTORGRALLOC
  if (mBuffers.count(key) != 1) {
    return false;
  }
#endif
  return true;
}

} /* namespace layers */
} /* namespace mozilla */
