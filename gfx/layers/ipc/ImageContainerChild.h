/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_gfx_layers_ImageContainerChild_h
#define mozilla_gfx_layers_ImageContainerChild_h

#include "mozilla/Mutex.h"
#include "mozilla/layers/PImageContainerChild.h"

namespace mozilla {
namespace layers {

class ImageContainer;
class ImageCompositeNotification;

/**
 * The child side of PImageContainer. It's best to avoid ImageContainer filling
 * this role since IPDL objects should be associated with a single thread and
 * ImageContainer definitely isn't. This object belongs to (and is always
 * destroyed on) the ImageBridge thread, except when we need to destroy it
 * during shutdown.
 * An ImageContainer owns one of these; we have a weak reference to our
 * ImageContainer.
 */
class ImageContainerChild final : public PImageContainerChild
{
public:
  explicit ImageContainerChild(ImageContainer* aImageContainer);

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(ImageContainerChild)

  void RegisterWithIPDL();
  void UnregisterFromIPDL();
  void SendAsyncDelete();

  void NotifyComposite(const ImageCompositeNotification& aNotification);
  void ForgetImageContainer();

private:
  ~ImageContainerChild()
  {}

private:
  Mutex mLock;
  ImageContainer* mImageContainer;

  // If mIPCOpen is false, it means the IPDL code tried to deallocate the actor
  // before the ImageContainer released it. When this happens we don't actually
  // delete the actor right away because the ImageContainer has a reference to
  // it. In this case the actor will be deleted when the ImageContainer lets go
  // of it.
  // mIPCOpen must not be accessed off the ImageBridgeChild thread.
  bool mIPCOpen;
};

} // namespace layers
} // namespace mozilla

#endif // mozilla_gfx_layers_ImageContainerChild_h
