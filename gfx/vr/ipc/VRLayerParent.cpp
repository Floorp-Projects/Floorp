/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VRLayerParent.h"

#include "mozilla/dom/WebGLParent.h"
#include "mozilla/layers/CompositorThread.h"
#include "mozilla/Unused.h"
#include "SharedSurface.h"
#include "SurfaceTypes.h"

namespace mozilla {
using namespace layers;
namespace gfx {

VRLayerParent::VRLayerParent(uint32_t aVRDisplayID, const uint32_t aGroup)
    : mIPCOpen(true), mVRDisplayID(aVRDisplayID), mGroup(aGroup) {}

VRLayerParent::~VRLayerParent() { MOZ_COUNT_DTOR(VRLayerParent); }

mozilla::ipc::IPCResult VRLayerParent::RecvDestroy() {
  Destroy();
  return IPC_OK();
}

void VRLayerParent::ActorDestroy(ActorDestroyReason aWhy) { mIPCOpen = false; }

void VRLayerParent::Destroy() {
  if (mVRDisplayID) {
    VRManager* vm = VRManager::Get();
    vm->RemoveLayer(this);
    // 0 will never be a valid VRDisplayID; we can use it to indicate that
    // we are destroyed and no longer associated with a display.
    mVRDisplayID = 0;
  }

  if (mIPCOpen) {
    Unused << PVRLayerParent::Send__delete__(this);
  }
}

mozilla::ipc::IPCResult VRLayerParent::RecvSubmitFrame(
    mozilla::dom::PWebGLParent* aPWebGLParent, const uint64_t& aFrameId,
    const uint64_t& aLastFrameId, const gfx::Rect& aLeftEyeRect,
    const gfx::Rect& aRightEyeRect) {
  // Keep the SharedSurfaceTextureClient alive long enough for
  // 1 extra frame, accomodating overlapped asynchronous rendering.
  mLastFrameTexture = mThisFrameTexture;

  auto webGLParent = static_cast<mozilla::dom::WebGLParent*>(aPWebGLParent);
  if (!webGLParent) {
    return IPC_OK();
  }

#if defined(MOZ_WIDGET_ANDROID)
  /**
   * Do not blit WebGL to a SurfaceTexture until the last submitted frame is
   * already processed and the new frame poses are ready. SurfaceTextures need
   * to be released in the VR render thread in order to allow to be used again
   * in the WebGLContext GLScreenBuffer producer. Not doing so causes some
   * freezes, crashes or other undefined behaviour.
   * DLP: TODO: Umm... not even the same process as before so...
   */
  if (!mThisFrameTexture || aLastFrameId == mLastSubmittedFrameId) {
    mThisFrameTexture = webGLParent->GetVRFrame();
  }
#else
  mThisFrameTexture = webGLParent->GetVRFrame();
#endif  // defined(MOZ_WIDGET_ANDROID)

  if (!mThisFrameTexture) {
    return IPC_OK();
  }

  // TODO: I killed all of the syncObject stuff rather than move it here
  // cause my current understanding is that it isn't needed here.

  gl::SharedSurface* surf = mThisFrameTexture->Surf();
  if (surf->mType == gl::SharedSurfaceType::Basic) {
    gfxCriticalError() << "SharedSurfaceType::Basic not supported for WebVR";
    return IPC_OK();
  }

  layers::SurfaceDescriptor desc;
  if (!surf->ToSurfaceDescriptor(&desc)) {
    gfxCriticalError() << "SharedSurface::ToSurfaceDescriptor failed in "
                          "VRLayerParent::SubmitFrame";
    return IPC_OK();
  }

  if (mVRDisplayID) {
    VRManager* vm = VRManager::Get();
    vm->SubmitFrame(this, desc, aFrameId, aLeftEyeRect, aRightEyeRect);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult VRLayerParent::RecvClearSurfaces() {
  mThisFrameTexture = nullptr;
  mLastFrameTexture = nullptr;
  return IPC_OK();
}

}  // namespace gfx
}  // namespace mozilla
