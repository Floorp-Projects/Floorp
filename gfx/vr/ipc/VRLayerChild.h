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
  static PVRLayerChild* CreateIPDLActor();
  static bool DestroyIPDLActor(PVRLayerChild* actor);

  void Initialize(dom::HTMLCanvasElement* aCanvasElement);
  void SubmitFrame();
  bool IsIPCOpen();

private:
  VRLayerChild();
  virtual ~VRLayerChild();
  void ClearSurfaces();
  virtual void ActorDestroy(ActorDestroyReason aWhy) override;

  RefPtr<dom::HTMLCanvasElement> mCanvasElement;
  RefPtr<layers::SharedSurfaceTextureClient> mShSurfClient;
  RefPtr<layers::TextureClient> mFront;
  bool mIPCOpen;

  // AddIPDLReference and ReleaseIPDLReference are only to be called by CreateIPDLActor
  // and DestroyIPDLActor, respectively. We intentionally make them private to prevent misuse.
  // The purpose of these methods is to be aware of when the IPC system around this
  // actor goes down: mIPCOpen is then set to false.
  void AddIPDLReference();
  void ReleaseIPDLReference();
};

} // namespace gfx
} // namespace mozilla

#endif
