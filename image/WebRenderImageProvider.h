/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_IMAGE_WEBRENDERIMAGEPROVIDER_H_
#define MOZILLA_IMAGE_WEBRENDERIMAGEPROVIDER_H_

#include "nsISupportsImpl.h"

namespace mozilla {
namespace layers {
class RenderRootStateManager;
}

namespace wr {
class IpcResourceUpdateQueue;
struct ExternalImageId;
struct ImageKey;
}  // namespace wr

namespace image {

class ImageResource;
using ImageProviderId = uint32_t;

class WebRenderImageProvider {
 public:
  // Subclasses may or may not be XPCOM classes, so we just require that they
  // implement AddRef and Release.
  NS_INLINE_DECL_PURE_VIRTUAL_REFCOUNTING

  ImageProviderId GetProviderId() const { return mProviderId; }

  static ImageProviderId AllocateProviderId();

  /**
   * Generate an ImageKey for the given frame.
   * @param aSurface  The current frame. This should match what was cached via
   *                  SetCurrentFrame, but if it does not, it will need to
   *                  regenerate the cached ImageKey.
   */
  virtual nsresult UpdateKey(layers::RenderRootStateManager* aManager,
                             wr::IpcResourceUpdateQueue& aResources,
                             wr::ImageKey& aKey) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  /**
   * Invalidate if a blob recording or simple surface provider (both are only
   * used by vector images), requiring it to be regenerated.
   */
  virtual void InvalidateSurface() {}

 protected:
  WebRenderImageProvider(const ImageResource* aImage);

 private:
  ImageProviderId mProviderId;
};

}  // namespace image
}  // namespace mozilla

#endif /* MOZILLA_IMAGE_WEBRENDERIMAGEPROVIDER_H_ */
