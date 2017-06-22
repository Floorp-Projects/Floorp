/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_VR_LAYERCHILD_H
#define GFX_VR_LAYERCHILD_H

#include "VRManagerChild.h"

#include "mozilla/RefPtr.h"
#include "mozilla/gfx/PVRLayerChild.h"
#include "GLContext.h"
#include "gfxVR.h"

class nsICanvasRenderingContextInternal;

namespace mozilla {
class WebGLContext;
namespace dom {
class HTMLCanvasElement;
}
namespace layers {
class SharedSurfaceTextureClient;
}
namespace gl {
class SurfaceFactory;
}
namespace gfx {

class VRLayerChild : public PVRLayerChild {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(VRLayerChild)

public:
  VRLayerChild(uint32_t aVRDisplayID, VRManagerChild* aVRManagerChild);
  void Initialize(dom::HTMLCanvasElement* aCanvasElement);
  void SubmitFrame();
  bool IsIPCOpen();

protected:
  virtual ~VRLayerChild();
  void ClearSurfaces();
  virtual void ActorDestroy(ActorDestroyReason aWhy) override;

  uint32_t mVRDisplayID;

  RefPtr<dom::HTMLCanvasElement> mCanvasElement;
  RefPtr<layers::SharedSurfaceTextureClient> mShSurfClient;
  RefPtr<layers::TextureClient> mFront;
  bool mIPCOpen;
};

} // namespace gfx
} // namespace mozilla

#endif
