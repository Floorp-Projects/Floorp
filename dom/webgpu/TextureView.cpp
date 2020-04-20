/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TextureView.h"

#include "Device.h"

namespace mozilla {
namespace webgpu {

GPU_IMPL_CYCLE_COLLECTION(TextureView, mParent)
GPU_IMPL_JS_WRAP(TextureView)

TextureView::TextureView(Texture* const aParent, RawId aId)
    : ChildOf(aParent), mId(aId) {}

TextureView::~TextureView() { Cleanup(); }

dom::HTMLCanvasElement* TextureView::GetTargetCanvasElement() const {
  return mParent->mTargetCanvasElement;
}  // namespace webgpu

void TextureView::Cleanup() {
  if (mValid && mParent && mParent->GetParentDevice()) {
    mValid = false;
    auto bridge = mParent->GetParentDevice()->GetBridge();
    if (bridge && bridge->IsOpen()) {
      bridge->SendTextureViewDestroy(mId);
    }
  }
}

}  // namespace webgpu
}  // namespace mozilla
