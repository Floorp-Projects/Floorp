/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ImageBridgeChild.h"
#include "ImageContainerChild.h"
#include "CompositorParent.h"
#include "ImageBridgeParent.h"
#include "gfxSharedImageSurface.h"
#include "ImageLayers.h"
#include "base/thread.h"
#include "mozilla/ReentrantMonitor.h"
#include "mozilla/layers/ShadowLayers.h"

using namespace base;

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
      CompositorParent::CompositorLoop());
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
PImageContainerChild* ImageBridgeChild::AllocPImageContainer(PRUint64* id)
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
  PRUint64 id = 0;
  SendPImageContainerConstructor(ctnChild, &id);
  ctnChild->SetID(id);
  return ctnChild.forget();
}

} // layers
} // mozilla
