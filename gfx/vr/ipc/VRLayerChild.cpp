/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VRLayerChild.h"

#include "gfxPlatform.h"
#include "../../../dom/canvas/ClientWebGLContext.h"
#include "mozilla/dom/HTMLCanvasElement.h"
#include "mozilla/dom/WebGLChild.h"

namespace mozilla {
namespace gfx {

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

void VRLayerChild::SubmitFrame(const VRDisplayInfo& aDisplayInfo) {
  uint64_t frameId = aDisplayInfo.GetFrameId();

  // aFrameId will not increment unless the previuosly submitted
  // frame was received by the VR thread and submitted to the VR
  // compositor.  We early-exit here in the event that SubmitFrame
  // was called twice for the same aFrameId.
  if (!mCanvasElement || frameId == mLastSubmittedFrameId) {
    return;
  }

  mLastSubmittedFrameId = frameId;

  const auto webgl = mCanvasElement->GetWebGLContext();
  if (!webgl) {
    return;
  }
  const auto webglChild = webgl->GetChild();
  if (!webglChild) {
    return;
  }

  SendSubmitFrame(webglChild, frameId,
                  aDisplayInfo.mDisplayState.lastSubmittedFrameId, mLeftEyeRect,
                  mRightEyeRect);
}

bool VRLayerChild::IsIPCOpen() { return mIPCOpen; }

void VRLayerChild::ClearSurfaces() {
  const auto webgl = mCanvasElement->GetWebGLContext();
  if (webgl) {
    webgl->ClearVRFrame();
  }
  SendClearSurfaces();
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

}  // namespace gfx
}  // namespace mozilla
