/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CanvasClient.h"

#include "gfx2DGlue.h"             // for ImageFormatToSurfaceFormat
#include "gfxPlatform.h"           // for gfxPlatform
#include "mozilla/gfx/BaseSize.h"  // for BaseSize
#include "mozilla/gfx/gfxVars.h"
#include "mozilla/layers/BufferTexture.h"
#include "mozilla/layers/CompositableForwarder.h"
#include "mozilla/layers/CompositorBridgeChild.h"  // for CompositorBridgeChild
#include "mozilla/layers/LayersTypes.h"
#include "mozilla/layers/OOPCanvasRenderer.h"
#include "mozilla/layers/TextureClient.h"  // for TextureClient, etc
#include "mozilla/layers/TextureClientOGL.h"
#include "mozilla/layers/TextureClientRecycleAllocator.h"
#include "nsDebug.h"      // for printf_stderr, NS_ASSERTION
#include "nsXULAppAPI.h"  // for XRE_GetProcessType, etc

using namespace mozilla::gfx;
using namespace mozilla::gl;

namespace mozilla {
namespace layers {

void CanvasClient::UseTexture(TextureClient* const aTexture) {
  MOZ_ASSERT(aTexture);

  const auto isClientNonPremult =
      bool(mTextureFlags & TextureFlags::NON_PREMULTIPLIED);
  const auto isTextureNonPremult =
      bool(aTexture->GetFlags() & TextureFlags::NON_PREMULTIPLIED);
  MOZ_ALWAYS_TRUE(isTextureNonPremult == isClientNonPremult);

  bool changed = false;

  if (aTexture != mFrontBuffer) {
    if (!aTexture->IsSharedWithCompositor()) {
      if (!AddTextureClient(aTexture)) {
        return;
      }
    }
    changed = true;
    mFrontBuffer = aTexture;
  }

  AutoTArray<CompositableForwarder::TimedTextureClient, 1> textures;
  CompositableForwarder::TimedTextureClient* t = textures.AppendElement();
  t->mTextureClient = aTexture;
  t->mPictureRect = nsIntRect(nsIntPoint(0, 0), aTexture->GetSize());
  t->mFrameID = mFrameID;

  GetForwarder()->UseTextures(this, textures);
  if (changed) {
    aTexture->SyncWithObject(GetForwarder()->GetSyncObject());
  }
}

static constexpr bool kIsWindows =
#ifdef XP_WIN
    true;
#else
    false;
#endif

RefPtr<TextureClient> CanvasClient::CreateTextureClientForCanvas(
    const gfx::SurfaceFormat aFormat, const gfx::IntSize aSize,
    const TextureFlags aFlags) {
  if (kIsWindows) {
    // With WebRender, host side uses data of TextureClient longer.
    // Then back buffer reuse in CanvasClient2D::Update() does not work. It
    // causes a lot of TextureClient allocations. For reducing the allocations,
    // TextureClientRecycler is used.
    if (GetForwarder() && GetForwarder()->GetCompositorBackendType() ==
                              LayersBackend::LAYERS_WR) {
      return GetTextureClientRecycler()->CreateOrRecycle(
          aFormat, aSize, BackendSelector::Canvas, mTextureFlags | aFlags);
    }
    return CreateTextureClientForDrawing(
        aFormat, aSize, BackendSelector::Canvas, mTextureFlags | aFlags);
  }

  // XXX - We should use CreateTextureClientForDrawing, but we first need
  // to use double buffering.
  gfx::BackendType backend =
      gfxPlatform::GetPlatform()->GetPreferredCanvasBackend();
  return TextureClient::CreateForRawBufferAccess(
      GetForwarder(), aFormat, aSize, backend, mTextureFlags | aFlags);
}

}  // namespace layers
}  // namespace mozilla
