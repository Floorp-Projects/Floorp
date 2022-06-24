/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CanvasRenderer.h"

#include "BuildConstants.h"
#include "ipc/KnowsCompositor.h"
#include "mozilla/gfx/gfxVars.h"
#include "mozilla/widget/DMABufSurface.h"
#include "mozilla/StaticPrefs_webgl.h"
#include "nsICanvasRenderingContextInternal.h"
#include "PersistentBufferProvider.h"
#include "WebGLTypes.h"

namespace mozilla {
namespace layers {

CanvasRendererData::CanvasRendererData() = default;
CanvasRendererData::~CanvasRendererData() = default;

// -

BorrowedSourceSurface::BorrowedSourceSurface(
    PersistentBufferProvider* const returnTo,
    const RefPtr<gfx::SourceSurface> surf)
    : mReturnTo(returnTo), mSurf(surf) {}

BorrowedSourceSurface::~BorrowedSourceSurface() {
  if (mReturnTo) {
    auto forgettable = mSurf;
    mReturnTo->ReturnSnapshot(forgettable.forget());
  }
}

// -

CanvasRenderer::CanvasRenderer() { MOZ_COUNT_CTOR(CanvasRenderer); }

CanvasRenderer::~CanvasRenderer() { MOZ_COUNT_DTOR(CanvasRenderer); }

void CanvasRenderer::Initialize(const CanvasRendererData& aData) {
  mData = aData;
}

bool CanvasRenderer::IsDataValid(const CanvasRendererData& aData) const {
  return mData.GetContext() == aData.GetContext();
}

std::shared_ptr<BorrowedSourceSurface> CanvasRenderer::BorrowSnapshot(
    const bool requireAlphaPremult) const {
  const auto context = mData.GetContext();
  if (!context) return nullptr;
  RefPtr<PersistentBufferProvider> provider = context->GetBufferProvider();

  RefPtr<gfx::SourceSurface> ss;

  if (provider) {
    ss = provider->BorrowSnapshot();
  }
  if (!ss) {
    provider = nullptr;
    ss = context->GetFrontBufferSnapshot(requireAlphaPremult);
  }
  if (!ss) return nullptr;

  return std::make_shared<BorrowedSourceSurface>(provider, ss);
}

bool CanvasRenderer::CopySnapshotTo(gfx::DrawTarget* aDT,
                                    bool aRequireAlphaPremult) {
  auto* const context = mData.GetContext();
  if (!context) return false;

  if (RefPtr<PersistentBufferProvider> provider =
          context->GetBufferProvider()) {
    // If we can copy the snapshot directly to the DT, try that first.
    if (provider->CopySnapshotTo(aDT)) {
      return true;
    }
  }

  // Otherwise, we have to borrow a snapshot before we can copy it to the DT.
  auto borrowed = BorrowSnapshot(aRequireAlphaPremult);
  if (!borrowed) {
    return false;
  }
  aDT->CopySurface(borrowed->mSurf, borrowed->mSurf->GetRect(), {0, 0});
  return true;
}

void CanvasRenderer::FirePreTransactionCallback() const {
  if (!mData.mDoPaintCallbacks) return;
  const auto context = mData.GetContext();
  if (!context) return;
  context->OnBeforePaintTransaction();
}

void CanvasRenderer::FireDidTransactionCallback() const {
  if (!mData.mDoPaintCallbacks) return;
  const auto context = mData.GetContext();
  if (!context) return;
  context->OnDidPaintTransaction();
}

TextureType TexTypeForWebgl(KnowsCompositor* const knowsCompositor) {
  if (!knowsCompositor) return TextureType::Unknown;
  const auto layersBackend = knowsCompositor->GetCompositorBackendType();

  switch (layersBackend) {
    case LayersBackend::LAYERS_LAST:
      MOZ_CRASH("Unexpected LayersBackend::LAYERS_LAST");

    case LayersBackend::LAYERS_NONE:
      return TextureType::Unknown;

    case LayersBackend::LAYERS_WR:
      break;
  }

  if (kIsWindows) {
    if (knowsCompositor->SupportsD3D11()) {
      return TextureType::D3D11;
    }
  }
  if (kIsMacOS) {
    return TextureType::MacIOSurface;
  }
  if (kIsWayland) {
    if (!knowsCompositor->UsingSoftwareWebRender() &&
        widget::GetDMABufDevice()->IsDMABufWebGLEnabled()) {
      return TextureType::DMABUF;
    }
  }

  if (kIsAndroid) {
    if (gfx::gfxVars::UseAHardwareBufferSharedSurface()) {
      return TextureType::AndroidHardwareBuffer;
    }
    if (StaticPrefs::webgl_enable_surface_texture()) {
      return TextureType::AndroidNativeWindow;
    }
  }

  return TextureType::Unknown;
}

}  // namespace layers
}  // namespace mozilla
