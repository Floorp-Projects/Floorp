/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_SHAREDSURFACESCHILD_H
#define MOZILLA_GFX_SHAREDSURFACESCHILD_H

#include <stddef.h>                     // for size_t
#include <stdint.h>                     // for uint32_t, uint64_t
#include "mozilla/Attributes.h"         // for override
#include "mozilla/Maybe.h"              // for Maybe
#include "mozilla/RefPtr.h"             // for already_AddRefed
#include "mozilla/StaticPtr.h"          // for StaticRefPtr
#include "mozilla/gfx/UserData.h"       // for UserDataKey
#include "mozilla/webrender/WebRenderTypes.h" // for wr::ImageKey

namespace mozilla {
namespace gfx {
class SourceSurfaceSharedData;
} // namespace gfx

namespace wr {
class IpcResourceUpdateQueue;
} // namespace wr

namespace layers {

class CompositorManagerChild;
class ImageContainer;
class WebRenderLayerManager;

class SharedSurfacesChild final
{
public:
  /**
   * Request that the surface be mapped into the compositor thread's memory
   * space. This is useful for when the caller itself has no present need for
   * the surface to be mapped, but knows there will be such a need in the
   * future. This may be called from any thread, but it may cause a dispatch to
   * the main thread.
   */
  static void Share(gfx::SourceSurfaceSharedData* aSurface);

  /**
   * Request that the surface be mapped into the compositor thread's memory
   * space, and a valid ExternalImageId be generated for it for use with
   * WebRender. This must be called from the main thread.
   */
  static nsresult Share(gfx::SourceSurface* aSurface,
                        wr::ExternalImageId& aId);

  /**
   * Request that the surface be mapped into the compositor thread's memory
   * space, and a valid ImageKey be generated for it for use with WebRender.
   * This must be called from the main thread.
   */
  static nsresult Share(gfx::SourceSurfaceSharedData* aSurface,
                        WebRenderLayerManager* aManager,
                        wr::IpcResourceUpdateQueue& aResources,
                        wr::ImageKey& aKey);

  /**
   * Request that the first surface in the image container's current images be
   * mapped into the compositor thread's memory space, and a valid ImageKey be
   * generated for it for use with WebRender. If a different method should be
   * used to share the image data for this particular container, it will return
   * NS_ERROR_NOT_IMPLEMENTED. This must be called from the main thread.
   */
  static nsresult Share(ImageContainer* aContainer,
                        WebRenderLayerManager* aManager,
                        wr::IpcResourceUpdateQueue& aResources,
                        wr::ImageKey& aKey);

  /**
   * Get the external ID, if any, bound to the shared surface. Used for memory
   * reporting purposes.
   */
  static Maybe<wr::ExternalImageId>
  GetExternalId(const gfx::SourceSurfaceSharedData* aSurface);

private:
  SharedSurfacesChild() = delete;
  ~SharedSurfacesChild() = delete;

  class ImageKeyData;
  class SharedUserData;

  static nsresult ShareInternal(gfx::SourceSurfaceSharedData* aSurface,
                                SharedUserData** aUserData);

  static void Unshare(const wr::ExternalImageId& aId, nsTArray<ImageKeyData>& aKeys);
  static void DestroySharedUserData(void* aClosure);

  static gfx::UserDataKey sSharedKey;
};

} // namespace layers
} // namespace mozilla

#endif
