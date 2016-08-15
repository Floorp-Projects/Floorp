/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VRLayerChild.h"
#include "GLScreenBuffer.h"
#include "mozilla/layers/TextureClientSharedSurface.h"
#include "SharedSurface.h"                // for SharedSurface
#include "SharedSurfaceGL.h"              // for SharedSurface
#include "mozilla/layers/LayersMessages.h" // for TimedTexture
#include "nsICanvasRenderingContextInternal.h"
#include "mozilla/dom/HTMLCanvasElement.h"

namespace mozilla {
namespace gfx {

VRLayerChild::VRLayerChild(uint32_t aVRDisplayID, VRManagerChild* aVRManagerChild)
  : mVRDisplayID(aVRDisplayID)
  , mCanvasElement(nullptr)
  , mShSurfClient(nullptr)
  , mFront(nullptr)
{
  MOZ_COUNT_CTOR(VRLayerChild);
}

VRLayerChild::~VRLayerChild()
{
  if (mCanvasElement) {
    mCanvasElement->StopVRPresentation();
  }

  ClearSurfaces();

  MOZ_COUNT_DTOR(VRLayerChild);
}

void
VRLayerChild::Initialize(dom::HTMLCanvasElement* aCanvasElement)
{
  MOZ_ASSERT(aCanvasElement);
  mCanvasElement = aCanvasElement;
  mCanvasElement->StartVRPresentation();

  VRManagerChild *vrmc = VRManagerChild::Get();
  vrmc->RunFrameRequestCallbacks();
}

void
VRLayerChild::SubmitFrame(int32_t aInputFrameID)
{
  if (!mCanvasElement) {
    return;
  }

  mShSurfClient = mCanvasElement->GetVRFrame();
  if (!mShSurfClient) {
    return;
  }

  gl::SharedSurface* surf = mShSurfClient->Surf();
  if (surf->mType == gl::SharedSurfaceType::Basic) {
    gfxCriticalError() << "SharedSurfaceType::Basic not supported for WebVR";
    return;
  }

  mFront = mShSurfClient;
  mShSurfClient = nullptr;

  mFront->SetAddedToCompositableClient();
  VRManagerChild* vrmc = VRManagerChild::Get();
  mFront->SyncWithObject(vrmc->GetSyncObject());
  MOZ_ALWAYS_TRUE(mFront->InitIPDLActor(vrmc, vrmc->GetBackendType()));

  SendSubmitFrame(aInputFrameID, mFront->GetIPDLActor());
}

void
VRLayerChild::ClearSurfaces()
{
  mFront = nullptr;
  mShSurfClient = nullptr;
}

} // namespace gfx
} // namespace mozilla
