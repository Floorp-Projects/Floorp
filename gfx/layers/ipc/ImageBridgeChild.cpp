/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/thread.h"

#include "CompositorParent.h"
#include "ImageBridgeChild.h"
#include "ImageBridgeParent.h"
#include "ImageContainerChild.h"
#include "ImageLayers.h"
#include "gfxSharedImageSurface.h"
#include "mozilla/Monitor.h"
#include "mozilla/ReentrantMonitor.h"
#include "mozilla/layers/ShadowLayers.h"
#include "nsXULAppAPI.h"

using namespace base;
using namespace mozilla::ipc;

namespace mozilla {
namespace layers {

// Singleton 
static ImageBridgeChild *sImageBridgeChildSingleton = nullptr;
static Thread *sImageBridgeChildThread = nullptr;

// dispatched function
static void StopImageBridgeSync(ReentrantMonitor *aBarrier, bool *aDone)
{
  ReentrantMonitorAutoEnter autoMon(*aBarrier);

  NS_ABORT_IF_FALSE(InImageBridgeChildThread(),
                    "Should be in ImageBridgeChild thread.");
  if (sImageBridgeChildSingleton) {

    sImageBridgeChildSingleton->SendStop();

    int numChildren =
      sImageBridgeChildSingleton->ManagedPImageContainerChild().Length();
    for (int i = numChildren-1; i >= 0; --i) {
      ImageContainerChild* ctn =
        static_cast<ImageContainerChild*>(
          sImageBridgeChildSingleton->ManagedPImageContainerChild()[i]);
      ctn->StopChild();
    }
  }
  *aDone = true;
  aBarrier->NotifyAll();
}

// dispatched function
static void DeleteImageBridgeSync(ReentrantMonitor *aBarrier, bool *aDone)
{
  ReentrantMonitorAutoEnter autoMon(*aBarrier);

  NS_ABORT_IF_FALSE(InImageBridgeChildThread(),
                    "Should be in ImageBridgeChild thread.");
  delete sImageBridgeChildSingleton;
  sImageBridgeChildSingleton = nullptr;
  *aDone = true;
  aBarrier->NotifyAll();
}

// dispatched function
static void CreateContainerChildSync(nsRefPtr<ImageContainerChild>* result, 
                                     ReentrantMonitor* barrier,
                                     bool *aDone)
{
  ReentrantMonitorAutoEnter autoMon(*barrier);
  *result = sImageBridgeChildSingleton->CreateImageContainerChildNow();
  *aDone = true;
  barrier->NotifyAll();
}

struct GrallocParam {
  gfxIntSize size;
  uint32_t format;
  uint32_t usage;
  SurfaceDescriptor* buffer;

  GrallocParam(const gfxIntSize& aSize,
               const uint32_t& aFormat,
               const uint32_t& aUsage,
               SurfaceDescriptor* aBuffer)
    : size(aSize)
    , format(aFormat)
    , usage(aUsage)
    , buffer(aBuffer)
  {}
};

// dispatched function
static void AllocSurfaceDescriptorGrallocSync(const GrallocParam& aParam,
                                              Monitor* aBarrier,
                                              bool* aDone)
{
  MonitorAutoLock autoMon(*aBarrier);

  sImageBridgeChildSingleton->AllocSurfaceDescriptorGrallocNow(aParam.size,
                                                               aParam.format,
                                                               aParam.usage,
                                                               aParam.buffer);
  *aDone = true;
  aBarrier->NotifyAll();
}

// dispatched function
static void DeallocSurfaceDescriptorGrallocSync(const SurfaceDescriptor& aBuffer,
                                                Monitor* aBarrier,
                                                bool* aDone)
{
  MonitorAutoLock autoMon(*aBarrier);

  sImageBridgeChildSingleton->DeallocSurfaceDescriptorGrallocNow(aBuffer);
  *aDone = true;
  aBarrier->NotifyAll();
}

// dispatched function
static void ConnectImageBridge(ImageBridgeChild * child, ImageBridgeParent * parent)
{
  MessageLoop *parentMsgLoop = parent->GetMessageLoop();
  ipc::AsyncChannel *parentChannel = parent->GetIPCChannel();
  child->Open(parentChannel, parentMsgLoop, mozilla::ipc::AsyncChannel::Child);
}

Thread* ImageBridgeChild::GetThread() const
{
  return sImageBridgeChildThread;
}

ImageBridgeChild* ImageBridgeChild::GetSingleton()
{
  return sImageBridgeChildSingleton;
}

bool ImageBridgeChild::IsCreated()
{
  return GetSingleton() != nullptr;
}

void ImageBridgeChild::StartUp()
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on the main Thread!");
  ImageBridgeChild::StartUpOnThread(new Thread("ImageBridgeChild"));
}

static void
ConnectImageBridgeInChildProcess(Transport* aTransport,
                                 ProcessHandle aOtherProcess)
{
  // Bind the IPC channel to the image bridge thread.
  sImageBridgeChildSingleton->Open(aTransport, aOtherProcess,
                                   XRE_GetIOMessageLoop(),
                                   AsyncChannel::Child);
}

PImageBridgeChild*
ImageBridgeChild::StartUpInChildProcess(Transport* aTransport,
                                        ProcessId aOtherProcess)
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on the main Thread!");

  ProcessHandle processHandle;
  if (!base::OpenProcessHandle(aOtherProcess, &processHandle)) {
    return nullptr;
  }

  sImageBridgeChildThread = new Thread("ImageBridgeChild");
  if (!sImageBridgeChildThread->Start()) {
    return nullptr;
  }

  sImageBridgeChildSingleton = new ImageBridgeChild();
  sImageBridgeChildSingleton->GetMessageLoop()->PostTask(
    FROM_HERE,
    NewRunnableFunction(ConnectImageBridgeInChildProcess,
                        aTransport, processHandle));
  
  return sImageBridgeChildSingleton;
}

void ImageBridgeChild::ShutDown()
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on the main Thread!");
  if (ImageBridgeChild::IsCreated()) {
    ImageBridgeChild::DestroyBridge();
    delete sImageBridgeChildThread;
    sImageBridgeChildThread = nullptr;
  }
}

bool ImageBridgeChild::StartUpOnThread(Thread* aThread)
{
  NS_ABORT_IF_FALSE(aThread, "ImageBridge needs a thread.");
  if (sImageBridgeChildSingleton == nullptr) {
    sImageBridgeChildThread = aThread;
    if (!aThread->IsRunning()) {
      aThread->Start();
    }
    sImageBridgeChildSingleton = new ImageBridgeChild();
    ImageBridgeParent* imageBridgeParent = new ImageBridgeParent(
      CompositorParent::CompositorLoop(), nullptr);
    sImageBridgeChildSingleton->ConnectAsync(imageBridgeParent);
    return true;
  } else {
    return false;
  }
}

void ImageBridgeChild::DestroyBridge()
{
  NS_ABORT_IF_FALSE(!InImageBridgeChildThread(),
                    "This method must not be called in this thread.");
  // ...because we are about to dispatch synchronous messages to the 
  // ImageBridgeChild thread.

  if (!IsCreated()) {
    return;
  }

  ReentrantMonitor barrier("ImageBridgeDestroyTask lock");
  ReentrantMonitorAutoEnter autoMon(barrier);

  bool done = false;
  sImageBridgeChildSingleton->GetMessageLoop()->PostTask(FROM_HERE, 
                  NewRunnableFunction(&StopImageBridgeSync, &barrier, &done));
  while (!done) {
    barrier.Wait();
  }

  done = false;
  sImageBridgeChildSingleton->GetMessageLoop()->PostTask(FROM_HERE, 
                  NewRunnableFunction(&DeleteImageBridgeSync, &barrier, &done));
  while (!done) {
    barrier.Wait();
  }

}

// Not needed, cf CreateImageContainerChildNow
PImageContainerChild* ImageBridgeChild::AllocPImageContainer(uint64_t* id)
{
  NS_ABORT();
  return nullptr;
}

bool ImageBridgeChild::DeallocPImageContainer(PImageContainerChild* aImgContainerChild)
{
  static_cast<ImageContainerChild*>(aImgContainerChild)->Release();
  return true;
}

bool InImageBridgeChildThread()
{
  return sImageBridgeChildThread->thread_id() == PlatformThread::CurrentId();
}

MessageLoop * ImageBridgeChild::GetMessageLoop() const
{
  return sImageBridgeChildThread->message_loop();
}

void ImageBridgeChild::ConnectAsync(ImageBridgeParent* aParent)
{
  GetMessageLoop()->PostTask(FROM_HERE, NewRunnableFunction(&ConnectImageBridge, 
                                                            this, aParent));
}

already_AddRefed<ImageContainerChild> ImageBridgeChild::CreateImageContainerChild()
{
  if (InImageBridgeChildThread()) {
    return ImageBridgeChild::CreateImageContainerChildNow(); 
  } 

  // ImageContainerChild can only be alocated on the ImageBridgeChild thread, so se
  // dispatch a task to the thread and block the current thread until the task has been
  // executed.
  nsRefPtr<ImageContainerChild> result = nullptr;

  ReentrantMonitor barrier("CreateImageContainerChild Lock");
  ReentrantMonitorAutoEnter autoMon(barrier);
  bool done = false;

  GetMessageLoop()->PostTask(FROM_HERE, NewRunnableFunction(&CreateContainerChildSync,
                                                            &result, &barrier, &done));

  // should stop the thread until the ImageContainerChild has been created on 
  // the other thread
  while (!done) {
    barrier.Wait();
  }
  return result.forget();
}

already_AddRefed<ImageContainerChild> ImageBridgeChild::CreateImageContainerChildNow()
{
  nsRefPtr<ImageContainerChild> ctnChild = new ImageContainerChild();
  uint64_t id = 0;
  SendPImageContainerConstructor(ctnChild, &id);
  ctnChild->SetID(id);
  return ctnChild.forget();
}

PGrallocBufferChild*
ImageBridgeChild::AllocPGrallocBuffer(const gfxIntSize&, const uint32_t&, const uint32_t&,
                                      MaybeMagicGrallocBufferHandle*)
{
#ifdef MOZ_HAVE_SURFACEDESCRIPTORGRALLOC
  return GrallocBufferActor::Create();
#else
  NS_RUNTIMEABORT("No gralloc buffers for you");
  return nullptr;
#endif
}

bool
ImageBridgeChild::DeallocPGrallocBuffer(PGrallocBufferChild* actor)
{
#ifdef MOZ_HAVE_SURFACEDESCRIPTORGRALLOC
  delete actor;
  return true;
#else
  NS_RUNTIMEABORT("Um, how did we get here?");
  return false;
#endif
}

bool
ImageBridgeChild::AllocSurfaceDescriptorGralloc(const gfxIntSize& aSize,
                                                const uint32_t& aFormat,
                                                const uint32_t& aUsage,
                                                SurfaceDescriptor* aBuffer)
{
  if (InImageBridgeChildThread()) {
    return ImageBridgeChild::AllocSurfaceDescriptorGrallocNow(aSize, aFormat, aUsage, aBuffer);
  }

  Monitor barrier("AllocSurfaceDescriptorGralloc Lock");
  MonitorAutoLock autoMon(barrier);
  bool done = false;

  GetMessageLoop()->PostTask(
    FROM_HERE,
    NewRunnableFunction(&AllocSurfaceDescriptorGrallocSync,
                        GrallocParam(aSize, aFormat, aUsage, aBuffer), &barrier, &done));

  while (!done) {
    barrier.Wait();
  }
  return true;
}

bool
ImageBridgeChild::AllocSurfaceDescriptorGrallocNow(const gfxIntSize& aSize,
                                                   const uint32_t& aFormat,
                                                   const uint32_t& aUsage,
                                                   SurfaceDescriptor* aBuffer)
{
#ifdef MOZ_HAVE_SURFACEDESCRIPTORGRALLOC
  MaybeMagicGrallocBufferHandle handle;
  PGrallocBufferChild* gc = SendPGrallocBufferConstructor(aSize, aFormat, aUsage, &handle);
  if (handle.Tnull_t == handle.type()) {
    PGrallocBufferChild::Send__delete__(gc);
    return false;
  }

  GrallocBufferActor* gba = static_cast<GrallocBufferActor*>(gc);
  gba->InitFromHandle(handle.get_MagicGrallocBufferHandle());

  *aBuffer = SurfaceDescriptorGralloc(nullptr, gc, aSize, /* external */ false);
  return true;
#else
  NS_RUNTIMEABORT("No gralloc buffers for you");
  return false;
#endif
}

bool
ImageBridgeChild::DeallocSurfaceDescriptorGralloc(const SurfaceDescriptor& aBuffer)
{
  if (InImageBridgeChildThread()) {
    return ImageBridgeChild::DeallocSurfaceDescriptorGrallocNow(aBuffer);
  }

  Monitor barrier("DeallocSurfaceDescriptor Lock");
  MonitorAutoLock autoMon(barrier);
  bool done = false;

  GetMessageLoop()->PostTask(FROM_HERE, NewRunnableFunction(&DeallocSurfaceDescriptorGrallocSync,
                                                            aBuffer, &barrier, &done));

  while (!done) {
    barrier.Wait();
  }

  return true;
}

bool
ImageBridgeChild::DeallocSurfaceDescriptorGrallocNow(const SurfaceDescriptor& aBuffer)
{
#ifdef MOZ_HAVE_SURFACEDESCRIPTORGRALLOC
  PGrallocBufferChild* gbp =
    aBuffer.get_SurfaceDescriptorGralloc().bufferChild();
  PGrallocBufferChild::Send__delete__(gbp);

  return true;
#else
  NS_RUNTIMEABORT("Um, how did we get here?");
  return false;
#endif
}

} // layers
} // mozilla
