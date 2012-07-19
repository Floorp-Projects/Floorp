/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_IMAGEBRIDGECHILD_H
#define MOZILLA_GFX_IMAGEBRIDGECHILD_H

#include "mozilla/layers/PImageBridgeChild.h"
#include "nsAutoPtr.h"

class gfxSharedImageSurface;

namespace base {
class Thread;
}

namespace mozilla {
namespace layers {

class ImageContainerChild;
class ImageBridgeParent;
class SharedImage;
class Image;

/**
 * Returns true if the current thread is the ImageBrdigeChild's thread.
 *
 * Can be called from any thread.
 */
bool InImageBridgeChildThread();

/**
 * The ImageBridge protocol is meant to allow ImageContainers to forward images
 * directly to the compositor thread/process without using the main thread.
 *
 * There are three important phases in the ImageBridge protocol. These three steps
 * can do different things depending if (A) the ImageContainer uses ImageBridge
 * or (B) it does not use ImageBridge:
 *
 * - When an ImageContainer calls its method SetCurrentImage:
 *   - (A) The image is sent directly to the compositor process through the 
 *   ImageBridge IPDL protocol.
 *   On the compositor side the image is stored in a global table that associates 
 *   the image with an ID corresponding to the ImageContainer, and a composition is 
 *   triggered.
 *   - (B) Since it does not have an ImageBridge, the image is not sent yet. 
 *   instead the will be sent to the compositor during the next layer transaction 
 *   (on the main thread).
 *  
 * - During a Layer transaction:
 *   - (A) The ImageContainer uses ImageBridge. The image is already available to the
 *   compositor process because it has been sent with SetCurrentImage. Yet, the 
 *   ShadowImageLayer on the compositor side will needs the ID referring to the 
 *   ImageContainer to access the Image. So during the Swap operation that happens
 *   in the transaction, we swap the container ID rather than the image data.
 *   - (B) Since the ImageContainer does not use ImageBridge, the image data is swaped.
 *  
 * - During composition:
 *   - (A) The ShadowImageLayer has an ImageContainer ID, it looks up the ID in the 
 *   global table to see if there is an image. If there is no image, nothing is rendered.
 *   - (B) The shadowImageLayer has image data rather than an ID (meaning it is not
 *   using ImageBridge), then it just composites the image data normally.
 *
 * This means that there might be a possibility for the ImageBridge to send the first 
 * frame before the first layer transaction that will pass the container ID to the 
 * ShadowImageLayer happens. In this (unlikely) case the layer is not composited 
 * until the layer transaction happens. This means this scenario is not harmful.
 *
 * Since sending an image through imageBridge triggers compsiting, the main thread is
 * not used at all (except for the very first transaction that provides the 
 * ShadowImageLayer with an ImageContainer ID).
 */
class ImageBridgeChild : public PImageBridgeChild
{
  friend class ImageContainer;
public:

  /**
   * Creates the image bridge with a dedicated thread for ImageBridgeChild.
   *
   * We may want to use a specifi thread in the future. In this case, use
   * CreateWithThread instead. 
   */
  static void StartUp();

  /**
   * Destroys the image bridge by calling DestroyBridge, and destroys the 
   * ImageBridge's thread.
   *
   * If you don't want to destroy the thread, call DestroyBridge directly 
   * instead.
   */
  static void ShutDown();

  /**
   * Creates the ImageBridgeChild manager protocol.
   */
  static bool StartUpOnThread(base::Thread* aThread);

  /**
   * Destroys The ImageBridge protcol.
   *
   * The actual destruction happens synchronously on the ImageBridgeChild thread
   * which means that if this function is called from another thread, the current 
   * thread will be paused until the destruction is done.
   */
  static void DestroyBridge();
  
  /**
   * Returns true if the singleton has been created.
   *
   * Can be called from any thread.
   */
  static bool IsCreated();

  /**
   * returns the singleton instance.
   *
   * can be called from any thread.
   */
  static ImageBridgeChild* GetSingleton();


  /**
   * Dispatches a task to the ImageBridgeChild thread to do the connection
   */
  void ConnectAsync(ImageBridgeParent* aParent);
    
  /**
   * Returns the ImageBridgeChild's thread.
   *
   * Can be called from any thread.
   */
  base::Thread * GetThread() const;
  
  /**
   * Returns the ImageBridgeChild's message loop.
   *
   * Can be called from any thread.
   */
  MessageLoop * GetMessageLoop() const;

  // overriden from PImageBridgeChild
  PImageContainerChild* AllocPImageContainer(PRUint64*);
  // overriden from PImageBridgeChild
  bool DeallocPImageContainer(PImageContainerChild* aImgContainerChild);

  /**
   * This must be called by the static function DeleteImageBridgeSync defined
   * in ImageBridgeChild.cpp ONLY.
   */
  ~ImageBridgeChild() {};

  /**
   * Part of the creation of ImageCOntainerChild that is executed on the 
   * ImageBridgeChild thread after invoking CreateImageContainerChild
   *
   * Must be called from the ImageBridgeChild thread.
   */
  already_AddRefed<ImageContainerChild> CreateImageContainerChildNow();
protected:
  
  ImageBridgeChild() {};

  /**
   * Creates an ImageContainerChild and it's associated ImageContainerParent.
   *
   * The creation happens synchronously on the ImageBridgeChild thread, so if 
   * this function is called on another thread, the current thread will be 
   * paused until the creation is done.
   *
   * This method should only be called from ImageContainer's constructor, 
   * because it spawns a task that contains a pointer to the ImageContainer
   * (not a refPtr). So the execution of the task can race with the destruction
   * of the ImageContainer if the refcount is decremented in another thread.
   * This is done like this because you cannot use a refPtr to the object
   * within its own constructor (if you do the object will be deleted along with
   * the refPtr before the constructor returns and the refcount gets a chance to
   * get incremented!). If you really need to call this function outside 
   * ImageContainer's constructor, make sure to increment and decrement 
   * the ImageContainer's reference count respectively before and after the call. 
   */
  already_AddRefed<ImageContainerChild> CreateImageContainerChild();

  
};

} // layers
} // mozilla


#endif

