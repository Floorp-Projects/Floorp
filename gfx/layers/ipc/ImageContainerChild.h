/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_IMAGECONTAINERCHILD_H
#define MOZILLA_GFX_IMAGECONTAINERCHILD_H

#include "mozilla/layers/PImageContainerChild.h"
#include "mozilla/layers/ImageBridgeChild.h"

namespace mozilla {
class ReentrantMonitor;
namespace layers {

class ImageBridgeCopyAndSendTask;
class ImageContainer;

/**
 * ImageContainerChild is the content-side part of the ImageBridge IPDL protocol
 * acting at the ImageContainer level.
 *
 * There is one ImageContainerChild/Parent
 * pair for each ImageContainer that uses the protocol.
 * ImageContainers that use the ImageBridge protocol forward frames through
 * ImageContainerChild to the compositor without using the main thread whereas
 * ImageContainers that don't use it will forward frames in the main thread
 * within a transaction.
 *
 * See ImageBridgeChild.h for more details about the ImageBridge protocol
 */
class ImageContainerChild : public PImageContainerChild
{
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(ImageContainerChild)

  friend class ImageBridgeChild;
  friend class ImageBridgeCopyAndSendTask;

public:
  ImageContainerChild();
  virtual ~ImageContainerChild();

  /**
   * Sends an image to the compositor without using the main thread.
   *
   * This method should be called by ImageContainer only, and can be called
   * from any thread.
   */
  void SendImageAsync(ImageContainer* aContainer, Image* aImage);

  /**
   * Each ImageContainerChild is associated to an ID. This ID is used in the 
   * compositor side to fetch the images without having to keep direct references
   * between the ShadowImageLayer and the InmageBridgeParent.
   */
  const uint64_t& GetID() const
  {
    return mImageContainerID;
  }

  /**
   * Overriden from PImageContainerChild.
   *
   * This methid is called whenever upon reception of an image that is not used by
   * the compositor anymore. When receiving shared images, the ImageContainer should 
   * store them in a pool to reuse them without reallocating shared memory when 
   * possible, or deallocate them if it can't keep them.
   */
  bool RecvReturnImage(const SharedImage& aImage) MOZ_OVERRIDE;

  /**
   * Puts the ImageContainerChild in a state where it can't send images anymore, to
   * avoid races with the upcomming __delete__ message.
   * Also SendStop to the parent side.
   *
   * Must be called from the ImageBridgeChild's thread.
   */
  void StopChildAndParent();
  
  /**
   * Puts the ImageContainerChild in a state where it can't send images anymore, to
   * avoid races with the upcomming __delete__ message, without calling SendStop.
   * This version does not send stop to the parent side, because it must be called
   * by ImageBridgeChild::StopNow which already send a message that will take care
   * of putting all the ImageContainerParents in the correct state. This is done in
   * order to avoid having many useless synchronous IPC messages when destroying 
   * the ImageBridge protocol entirely
   * 
   * Must be called on the ImageBridgeChild's thread.
   */
  void StopChild();
  
  /**
   * Dispatches StopChildAndParent to the ImageBridgeChild's thread.
   *
   * Can be called on any thread.
   * This is typically called when you want to destroy one ImageContainerChild/Parent
   * pair without destroying the protocol entirely. 
   */
  void DispatchStop();

  /**
   * Dispatches a task to the ImageBridge Thread, that will destroy the 
   * ImageContainerChild and associated imageContainerParent asynchonously.
   */
  void DispatchDestroy();

  /**
   * Flush and deallocate the shared images in the pool.
   */
  void SetIdle();

  /**
   * Can be called from any thread.
   * deallocates or places aImage in a pool.
   * If this method is not called on the ImageBridgeChild thread, a task is
   * is dispatched and the recycling/deallocation happens asynchronously on
   * the ImageBridgeChild thread.
   */
  void RecycleSharedImage(SharedImage* aImage);

  /**
   * Creates a YCbCrImage using shared memory to store its data.
   * Use a uint32_t for formats list to avoid #include horribleness.
   */
  already_AddRefed<Image> CreateImage(const uint32_t *aFormats,
                                      uint32_t aNumFormats);

  /**
   * Allocates an unsafe shmem.
   * Unlike AllocUnsafeShmem, this method can be called from any thread.
   * If the calling thread is not the ImageBridgeChild thread, this method will
   * dispatch a synchronous call to AllocUnsafeShmem on the ImageBridgeChild
   * thread meaning that the calling thread will be blocked until
   * AllocUnsafeShmem returns on the ImageBridgeChild thread.
   */
  bool AllocUnsafeShmemSync(size_t aBufSize,
                            SharedMemory::SharedMemoryType aType,
                            ipc::Shmem* aShmem);

  /**
   * Dispatches a task on the ImageBridgeChild thread to deallocate a shmem.
   */
  void DeallocShmemAsync(ipc::Shmem& aShmem);

protected:

  virtual PGrallocBufferChild*
  AllocPGrallocBuffer(const gfxIntSize&, const gfxContentType&,
                      MaybeMagicGrallocBufferHandle*) MOZ_OVERRIDE

  { return nullptr; }

  virtual bool
  DeallocPGrallocBuffer(PGrallocBufferChild* actor) MOZ_OVERRIDE
  { return false; }

  inline MessageLoop* GetMessageLoop() const 
  {
    return ImageBridgeChild::GetSingleton()->GetMessageLoop();
  }

  /**
   * Must be called on the ImageBridgeChild's thread.
   */
  void DestroyNow();
  
  /**
   * Dispatches a task on the ImageBridgeChild's thread that will call SendFlush
   * and deallocate the shared images in the pool.
   * Can be called on any thread.
   */
  void SetIdleSync(Monitor* aBarrier, bool* aDone);

  /**
   * Must be called on the ImageBridgeChild's thread.
   */
  void SetIdleNow();

  inline void SetID(uint64_t id)
  {
    mImageContainerID = id;
  }

  /**
   * Must be called on the ImageBirdgeChild thread. (See RecycleSharedImage)
   */
  void RecycleSharedImageNow(SharedImage* aImage);

  /**
   * Deallocates a shared image's shared memory.
   * It is the caller's responsibility to delete the SharedImage itself.
   */
  void DestroySharedImage(const SharedImage& aSurface);

  /**
   * Each ImageContainerChild keeps a pool of SharedImages to avoid 
   * deallocationg/reallocating too often.
   * This method is typically called when the a shared image is received from
   * the compositor.
   * 
   */
  bool AddSharedImageToPool(SharedImage* img);

  /**
   * Must be called on the ImageBridgeChild's thread.
   *
   * creates and sends a shared image containing the same data as the image passed
   * in parameter.
   * - If the image's data is already in shared memory, this should just acquire
   * the data rather tahn copying it
   * - If There is an available shared image of the same size in the pool, reuse
   * it rather than allocating shared memory.
   * - worst case, allocate shared memory and copy the image's data into it. 
   */
  void SendImageNow(Image* aImage);


  /**
   * Returns a SharedImage if the image passed in parameter can be shared
   * directly without a copy, returns nullptr otherwise.
   */
  SharedImage* AsSharedImage(Image* aImage);

  /**
   * Removes a shared image from the pool and returns it.
   * Returns nullptr if the pool is empty.
   * The returned image does not contain the image data, a copy still needs to
   * be done afterward (see CopyDataIntoSharedImage).
   */
  SharedImage* GetSharedImageFor(Image* aImage);

  /**
   * Allocates a SharedImage.
   * Returns nullptr in case of failure.
   * The returned image does not contain the image data, a copy still needs to
   * be done afterward (see CopyDataIntoSharedImage).
   * Must be called on the ImageBridgeChild thread.
   */
  SharedImage* AllocateSharedImageFor(Image* aImage);

  /**
   * Called by ImageToSharedImage.
   */
  bool CopyDataIntoSharedImage(Image* src, SharedImage* dest);


  /**
   * Deallocates all the shared images from the pool and clears the pool.
   */
  void ClearSharedImagePool();

private:
  uint64_t mImageContainerID;
  nsTArray<SharedImage*> mSharedImagePool;

  /**
   * Save a reference to the outgoing images and remove the reference
   * once the image is returned from the compositor.
   * GonkIOSurfaceImage needs to know when to return the buffer to the
   * producing thread. The buffer is returned when GonkIOSurfaceImage
   * destructs.
   */
  nsTArray<nsRefPtr<Image> > mImageQueue;

  int mActiveImageCount;
  bool mStop;
  bool mDispatchedDestroy;
};


} // namespace
} // namespace

#endif

