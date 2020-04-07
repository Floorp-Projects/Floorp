/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VRLayerChild.h"
#include "gfxPlatform.h"
#include "GLScreenBuffer.h"
#include "../../../dom/canvas/ClientWebGLContext.h"
#include "mozilla/layers/TextureClientSharedSurface.h"
#include "SharedSurface.h"                  // for SharedSurface
#include "SharedSurfaceGL.h"                // for SharedSurface
#include "mozilla/layers/LayersMessages.h"  // for TimedTexture
#include "nsICanvasRenderingContextInternal.h"
#include "mozilla/dom/HTMLCanvasElement.h"
#include "mozilla/layers/SyncObject.h"  // for SyncObjectClient

namespace mozilla::gfx {

VRLayerChild::VRLayerChild()
    : mCanvasElement(nullptr), mIPCOpen(false), mLastSubmittedFrameId(0) {
  MOZ_COUNT_CTOR(VRLayerChild);
}

VRLayerChild::~VRLayerChild() {
  ClearSurfaces();

  MOZ_COUNT_DTOR(VRLayerChild);
}

void VRLayerChild::Initialize(dom::HTMLCanvasElement* aCanvasElement,
                              const gfx::Rect& aLeftEyeRect,
                              const gfx::Rect& aRightEyeRect) {
  MOZ_ASSERT(aCanvasElement);
  mLeftEyeRect = aLeftEyeRect;
  mRightEyeRect = aRightEyeRect;
  mCanvasElement = aCanvasElement;
}

void VRLayerChild::SetXRFramebuffer(WebGLFramebufferJS* fb) {
  mFramebuffer = fb;
}

void VRLayerChild::SubmitFrame(const VRDisplayInfo& aDisplayInfo) {
  uint64_t frameId = aDisplayInfo.GetFrameId();

  // aFrameId will not increment unless the previuosly submitted
  // frame was received by the VR thread and submitted to the VR
  // compositor.  We early-exit here in the event that SubmitFrame
  // was called twice for the same aFrameId.
  if (!mCanvasElement || frameId == mLastSubmittedFrameId) {
    return;
  }

  const auto& webgl = mCanvasElement->GetWebGLContext();
  if (!webgl) return;

  // Keep the SharedSurfaceTextureClient alive long enough for
  // 1 extra frame, accomodating overlapped asynchronous rendering.
  mLastFrameTexture = mThisFrameTexture;

#if defined(MOZ_WIDGET_ANDROID)
  /**
   * Do not blit WebGL to a SurfaceTexture until the last submitted frame is
   * already processed and the new frame poses are ready. SurfaceTextures need
   * to be released in the VR render thread in order to allow to be used again
   * in the WebGLContext GLScreenBuffer producer. Not doing so causes some
   * freezes, crashes or other undefined behaviour.
   */
  if (!mThisFrameTexture || aDisplayInfo.mDisplayState.lastSubmittedFrameId ==
                                mLastSubmittedFrameId) {
    mThisFrameTexture = webgl->GetVRFrame(mFramebuffer.get());
  }
#else
  mThisFrameTexture = webgl->GetVRFrame(mFramebuffer.get());
#endif  // defined(MOZ_WIDGET_ANDROID)

  mLastSubmittedFrameId = frameId;

  if (!mThisFrameTexture) {
    return;
  }
  VRManagerChild* vrmc = VRManagerChild::Get();
  layers::SyncObjectClient* syncObject = vrmc->GetSyncObject();
  mThisFrameTexture->SyncWithObject(syncObject);
  if (!gfxPlatform::GetPlatform()->DidRenderingDeviceReset()) {
    if (syncObject && syncObject->IsSyncObjectValid()) {
      syncObject->Synchronize();
    }
  }

  gl::SharedSurface* surf = mThisFrameTexture->Surf();
  if (surf->mType == gl::SharedSurfaceType::Basic) {
    gfxCriticalError() << "SharedSurfaceType::Basic not supported for WebVR";
    return;
  }

  layers::SurfaceDescriptor desc;
  if (!surf->ToSurfaceDescriptor(&desc)) {
    gfxCriticalError() << "SharedSurface::ToSurfaceDescriptor failed in "
                          "VRLayerChild::SubmitFrame";
    return;
  }

  SendSubmitFrame(desc, frameId, mLeftEyeRect, mRightEyeRect);
}

bool VRLayerChild::IsIPCOpen() { return mIPCOpen; }

void VRLayerChild::ClearSurfaces() {
  mThisFrameTexture = nullptr;
  mLastFrameTexture = nullptr;

  const auto& webgl = mCanvasElement->GetWebGLContext();
  if (webgl) {
    webgl->ClearVRFrame();
  }
}

void VRLayerChild::ActorDestroy(ActorDestroyReason aWhy) { mIPCOpen = false; }

// static
PVRLayerChild* VRLayerChild::CreateIPDLActor() {
  VRLayerChild* c = new VRLayerChild();
  c->AddIPDLReference();
  return c;
}

// static
bool VRLayerChild::DestroyIPDLActor(PVRLayerChild* actor) {
  static_cast<VRLayerChild*>(actor)->ReleaseIPDLReference();
  return true;
}

void VRLayerChild::AddIPDLReference() {
  MOZ_ASSERT(mIPCOpen == false);
  mIPCOpen = true;
  AddRef();
}
void VRLayerChild::ReleaseIPDLReference() {
  MOZ_ASSERT(mIPCOpen == false);
  Release();
}

}  // namespace mozilla::gfx
