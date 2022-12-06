/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ShareableCanvasRenderer.h"

#include "mozilla/dom/WebGLTypes.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/layers/TextureClientSharedSurface.h"
#include "mozilla/layers/CompositableForwarder.h"
#include "mozilla/layers/TextureForwarder.h"

#include "ClientWebGLContext.h"
#include "gfxUtils.h"
#include "GLScreenBuffer.h"
#include "nsICanvasRenderingContextInternal.h"
#include "SharedSurfaceGL.h"

#ifdef MOZ_WIDGET_ANDROID
#  include "mozilla/layers/AndroidHardwareBuffer.h"
#endif

using namespace mozilla::gfx;

namespace mozilla {
namespace layers {

ShareableCanvasRenderer::ShareableCanvasRenderer() {
  MOZ_COUNT_CTOR(ShareableCanvasRenderer);
}

ShareableCanvasRenderer::~ShareableCanvasRenderer() {
  MOZ_COUNT_DTOR(ShareableCanvasRenderer);

  mFrontBufferFromDesc = nullptr;
  DisconnectClient();
}

void ShareableCanvasRenderer::Initialize(const CanvasRendererData& aData) {
  CanvasRenderer::Initialize(aData);
  mCanvasClient = nullptr;
}

void ShareableCanvasRenderer::ClearCachedResources() {
  CanvasRenderer::ClearCachedResources();

  if (mCanvasClient) {
    mCanvasClient->Clear();
  }
}

void ShareableCanvasRenderer::DisconnectClient() {
  if (mCanvasClient) {
    mCanvasClient->OnDetach();
    mCanvasClient = nullptr;
  }
}

RefPtr<layers::TextureClient> ShareableCanvasRenderer::GetFrontBufferFromDesc(
    const layers::SurfaceDescriptor& desc, TextureFlags flags) {
  if (mFrontBufferFromDesc && mFrontBufferDesc == desc)
    return mFrontBufferFromDesc;
  mFrontBufferFromDesc = nullptr;

  // Test the validity of aAllocator
  const auto& compositableForwarder = GetForwarder();
  if (!compositableForwarder) {
    return nullptr;
  }
  const auto& textureForwarder = compositableForwarder->GetTextureForwarder();

  auto format = gfx::SurfaceFormat::R8G8B8X8;
  if (!mData.mIsOpaque) {
    format = gfx::SurfaceFormat::R8G8B8A8;

    if (!mData.mIsAlphaPremult) {
      flags |= TextureFlags::NON_PREMULTIPLIED;
    }
  }

  if (desc.type() !=
      SurfaceDescriptor::TSurfaceDescriptorAndroidHardwareBuffer) {
    mFrontBufferFromDesc = SharedSurfaceTextureData::CreateTextureClient(
        desc, format, mData.mSize, flags, textureForwarder);
  } else {
#ifdef MOZ_WIDGET_ANDROID
    const SurfaceDescriptorAndroidHardwareBuffer& bufferDesc =
        desc.get_SurfaceDescriptorAndroidHardwareBuffer();
    RefPtr<AndroidHardwareBuffer> buffer =
        AndroidHardwareBufferManager::Get()->GetBuffer(bufferDesc.bufferId());
    if (!buffer) {
      return nullptr;
    }
    // TextureClient is created only when AndroidHardwareBuffer does not own it.
    mFrontBufferFromDesc = buffer->GetTextureClientOfSharedSurfaceTextureData(
        desc, format, mData.mSize, flags, textureForwarder);
#else
    MOZ_ASSERT_UNREACHABLE("unexpected to be called");
#endif
  }
  mFrontBufferDesc = desc;
  return mFrontBufferFromDesc;
}

void ShareableCanvasRenderer::UpdateCompositableClient() {
  if (!CreateCompositable()) {
    return;
  }

  if (!IsDirty()) {
    return;
  }
  ResetDirty();

  const auto context = mData.GetContext();
  if (!context) return;
  const auto& provider = context->GetBufferProvider();
  const auto& forwarder = GetForwarder();

  // -

  auto flags = TextureFlags::IMMUTABLE;
  if (!YIsDown()) {
    flags |= TextureFlags::ORIGIN_BOTTOM_LEFT;
  }
  if (IsOpaque()) {
    flags |= TextureFlags::IS_OPAQUE;
  }

  // -

  const auto fnGetExistingTc =
      [&](const Maybe<SurfaceDescriptor>& aDesc) -> RefPtr<TextureClient> {
    if (aDesc) {
      return GetFrontBufferFromDesc(*aDesc, flags);
    }
    if (provider) {
      if (!provider->SetKnowsCompositor(forwarder)) {
        gfxCriticalNote << "BufferProvider::SetForwarder failed";
        return nullptr;
      }

      return provider->GetTextureClient();
    }
    return nullptr;
  };

  // -

  const auto fnMakeTcFromSnapshot = [&]() -> RefPtr<TextureClient> {
    const auto& size = mData.mSize;

    auto contentType = gfxContentType::COLOR;
    if (!mData.mIsOpaque) {
      contentType = gfxContentType::COLOR_ALPHA;
    }
    const auto surfaceFormat =
        gfxPlatform::GetPlatform()->Optimal2DFormatForContent(contentType);

    const auto tc =
        mCanvasClient->CreateTextureClientForCanvas(surfaceFormat, size, flags);
    if (!tc) {
      return nullptr;
    }

    {
      TextureClientAutoLock tcLock(tc, OpenMode::OPEN_WRITE_ONLY);
      if (!tcLock.Succeeded()) {
        return nullptr;
      }

      const RefPtr<DrawTarget> dt = tc->BorrowDrawTarget();

      const bool requireAlphaPremult = false;
      auto borrowed = BorrowSnapshot(requireAlphaPremult);
      if (!borrowed) {
        return nullptr;
      }
      dt->CopySurface(borrowed->mSurf, borrowed->mSurf->GetRect(), {0, 0});
    }

    return tc;
  };

  // -

  {
    FirePreTransactionCallback();

    const auto desc = context->GetFrontBuffer(nullptr);
    if (desc &&
        desc->type() == SurfaceDescriptor::TSurfaceDescriptorRemoteTexture) {
      const auto& forwarder = GetForwarder();
      const auto& textureDesc = desc->get_SurfaceDescriptorRemoteTexture();
      if (!mData.mIsAlphaPremult) {
        flags |= TextureFlags::NON_PREMULTIPLIED;
      }
      EnsurePipeline(/* aIsAsync */ true);
      forwarder->UseRemoteTexture(mCanvasClient, textureDesc.textureId(),
                                  textureDesc.ownerId(), mData.mSize, flags);

      FireDidTransactionCallback();
      return;
    }

    EnsurePipeline(/* aIsAsync */ false);

    // Let's see if we can get a no-copy TextureClient from the canvas.
    auto tc = fnGetExistingTc(desc);
    if (!tc) {
      // Otherwise, snapshot the surface and copy into a TexClient.
      tc = fnMakeTcFromSnapshot();
    }
    if (tc != mFrontBufferFromDesc) {
      mFrontBufferFromDesc = nullptr;
    }

    if (!tc) {
      NS_WARNING("Couldn't make TextureClient for CanvasRenderer.");
      return;
    }

    mCanvasClient->UseTexture(tc);

    FireDidTransactionCallback();
  }
}

}  // namespace layers
}  // namespace mozilla
