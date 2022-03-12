/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VRLayerChild.h"

#include "mozilla/dom/HTMLCanvasElement.h"
#include "mozilla/layers/ImageBridgeChild.h"
#include "mozilla/layers/LayersMessages.h"  // for TimedTexture
#include "mozilla/layers/SyncObject.h"      // for SyncObjectClient
#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/StaticPrefs_webgl.h"

#include "ClientWebGLContext.h"
#include "gfxPlatform.h"
#include "GLContext.h"
#include "GLScreenBuffer.h"
#include "SharedSurface.h"    // for SharedSurface
#include "SharedSurfaceGL.h"  // for SharedSurface

namespace mozilla::gfx {

VRLayerChild::VRLayerChild() { MOZ_COUNT_CTOR(VRLayerChild); }

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

static constexpr bool kIsAndroid =
#if defined(MOZ_WIDGET_ANDROID)
    true;
#else
    false;
#endif

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
  mLastFrameTextureDesc = mThisFrameTextureDesc;

  bool getNewFrame = true;
  if (kIsAndroid) {
    /**
     * Do not blit WebGL to a SurfaceTexture until the last submitted frame is
     * already processed and the new frame poses are ready. SurfaceTextures need
     * to be released in the VR render thread in order to allow to be used again
     * in the WebGLContext GLScreenBuffer producer. Not doing so causes some
     * freezes, crashes or other undefined behaviour.
     */
    getNewFrame = (!mThisFrameTextureDesc ||
                   aDisplayInfo.mDisplayState.lastSubmittedFrameId ==
                       mLastSubmittedFrameId);
  }
  if (getNewFrame) {
    const RefPtr<layers::ImageBridgeChild> imageBridge =
        layers::ImageBridgeChild::GetSingleton();

    auto texType = layers::TextureType::Unknown;
    if (imageBridge) {
      texType = layers::PreferredCanvasTextureType(imageBridge);
    }
    if (kIsAndroid && StaticPrefs::webgl_enable_surface_texture()) {
      texType = layers::TextureType::AndroidNativeWindow;
    }

    webgl->Present(mFramebuffer, texType, true);
    mThisFrameTextureDesc = webgl->GetFrontBuffer(mFramebuffer, true);
  }

  mLastSubmittedFrameId = frameId;

  if (!mThisFrameTextureDesc) {
    gfxCriticalError() << "ToSurfaceDescriptor failed in "
                          "VRLayerChild::SubmitFrame";
    return;
  }

  SendSubmitFrame(*mThisFrameTextureDesc, frameId, mLeftEyeRect, mRightEyeRect);
}

bool VRLayerChild::IsIPCOpen() { return mIPCOpen; }

void VRLayerChild::ClearSurfaces() {
  mThisFrameTextureDesc = Nothing();
  mLastFrameTextureDesc = Nothing();
  const auto& webgl = mCanvasElement->GetWebGLContext();
  if (!mFramebuffer && webgl) {
    webgl->ClearVRSwapChain();
  }
}

void VRLayerChild::ActorDestroy(ActorDestroyReason aWhy) { mIPCOpen = false; }

// static
PVRLayerChild* VRLayerChild::CreateIPDLActor() {
  if (!StaticPrefs::dom_vr_enabled() && !StaticPrefs::dom_vr_webxr_enabled()) {
    return nullptr;
  }

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
