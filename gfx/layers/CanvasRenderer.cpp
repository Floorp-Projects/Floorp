/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CanvasRenderer.h"

#include "AsyncCanvasRenderer.h"
#include "GLContext.h"
#include "OOPCanvasRenderer.h"
#include "PersistentBufferProvider.h"

namespace mozilla {
namespace layers {

CanvasInitializeData::CanvasInitializeData() = default;
CanvasInitializeData::~CanvasInitializeData() = default;

CanvasRenderer::CanvasRenderer()
    : mPreTransCallback(nullptr),
      mPreTransCallbackData(nullptr),
      mDidTransCallback(nullptr),
      mDidTransCallbackData(nullptr),
      mDirty(false) {
  MOZ_COUNT_CTOR(CanvasRenderer);
}

CanvasRenderer::~CanvasRenderer() {
  Destroy();
  MOZ_COUNT_DTOR(CanvasRenderer);
}

void CanvasRenderer::Initialize(const CanvasInitializeData& aData) {
  mPreTransCallback = aData.mPreTransCallback;
  mPreTransCallbackData = aData.mPreTransCallbackData;
  mDidTransCallback = aData.mDidTransCallback;
  mDidTransCallbackData = aData.mDidTransCallbackData;

  mSize = aData.mSize;
}

}  // namespace layers
}  // namespace mozilla
