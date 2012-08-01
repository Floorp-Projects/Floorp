/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_IMAGECONTAINERCHILD_H
#define MOZILLA_GFX_IMAGECONTAINERCHILD_H

#include "mozilla/layers/PImageContainerChild.h"
#include "mozilla/layers/ImageBridgeChild.h"

namespace mozilla {
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
  const PRUint64& GetID() const
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
   * Dispatches a task on the ImageBridgeChild's thread that will call SendFlush
   * and deallocate the shared images in the pool.
   * Can be called on any thread.
   */
  void DispatchSetIdle();

  /**
   * Must be called on the ImageBridgeChild's thread.
   */
  void SetIdleNow();
  
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
   * Must be called on the ImageBridgeCHild's thread.
   */
  void DestroyNow();

  inline void SetID(PRUint64 id)
  {
    mImageContainerID = id;
  }

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
   * Removes a shared image from the pool and returns it.
   * Returns nullptr if the pool is empty.
   */
  SharedImage* PopSharedImageFromPool();
  /**
   * Seallocates all the shared images from the pool and clears the pool.
   */

  void ClearSharedImagePool();
  /**
   * Returns a shared image containing the same data as the image passed in 
   * parameter.
   * - If the image's data is already in shared memory, this should just acquire
   * the data rather tahn copying it (TODO)
   * - If There is an available shared image of the same size in the pool, reuse
   * it rather than allocating shared memory.
   * - worst case, allocate shared memory and copy the image's data into it. 
   */
  SharedImage* ImageToSharedImage(Image* toCopy);

  /**
   * Called by ImageToSharedImage.
   */
  bool CopyDataIntoSharedImage(Image* src, SharedImage* dest);

  /**
   * Allocates a SharedImage and copy aImage's data into it.
   * Called by ImageToSharedImage.
   */
  SharedImage * CreateSharedImageFromData(Image* aImage);

private:

  PRUint64 mImageContainerID;
  nsIntSize mSize;
  nsTArray<SharedImage*> mSharedImagePool;
  int mActiveImageCount;
  bool mStop;
  bool mDispatchedDestroy;
};


} // namespace
} // namespace

#endif

