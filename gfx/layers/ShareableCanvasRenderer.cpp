/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ShareableCanvasRenderer.h"

#include "GLContext.h"                  // for GLContext
#include "GLScreenBuffer.h"             // for GLScreenBuffer
#include "SharedSurfaceGL.h"            // for SurfaceFactory_GLTexture, etc
#include "mozilla/layers/AsyncCanvasRenderer.h"
#include "mozilla/layers/TextureClientSharedSurface.h"

namespace mozilla {
namespace layers {

ShareableCanvasRenderer::ShareableCanvasRenderer()
  : mCanvasClient(nullptr)
  , mFactory(nullptr)
  , mFlags(TextureFlags::NO_FLAGS)
{
  MOZ_COUNT_CTOR(ShareableCanvasRenderer);
}

ShareableCanvasRenderer::~ShareableCanvasRenderer()
{
  MOZ_COUNT_DTOR(ShareableCanvasRenderer);

  Destroy();
}

void
ShareableCanvasRenderer::Initialize(const CanvasInitializeData& aData)
{
  CopyableCanvasRenderer::Initialize(aData);

  mCanvasClient = nullptr;

  if (!mGLContext)
    return;

  gl::GLScreenBuffer* screen = mGLContext->Screen();

  gl::SurfaceCaps caps;
  if (mGLFrontbuffer) {
    // The screen caps are irrelevant if we're using a separate frontbuffer.
    caps = mGLFrontbuffer->mHasAlpha ? gl::SurfaceCaps::ForRGBA()
                                     : gl::SurfaceCaps::ForRGB();
  } else {
    MOZ_ASSERT(screen);
    caps = screen->mCaps;
  }

  auto forwarder = GetForwarder();

  mFlags = TextureFlags::ORIGIN_BOTTOM_LEFT;
  if (!aData.mIsGLAlphaPremult) {
    mFlags |= TextureFlags::NON_PREMULTIPLIED;
  }

  UniquePtr<gl::SurfaceFactory> factory =
    gl::GLScreenBuffer::CreateFactory(mGLContext, caps, forwarder, mFlags);

  if (mGLFrontbuffer) {
    // We're using a source other than the one in the default screen.
    // (SkiaGL)
    mFactory = std::move(factory);
    if (!mFactory) {
      // Absolutely must have a factory here, so create a basic one
      mFactory = MakeUnique<gl::SurfaceFactory_Basic>(mGLContext, caps, mFlags);
    }
  } else {
    if (factory)
      screen->Morph(std::move(factory));
  }
}

void
ShareableCanvasRenderer::ClearCachedResources()
{
  CopyableCanvasRenderer::ClearCachedResources();

  if (mCanvasClient) {
    mCanvasClient->Clear();
  }
}

void
ShareableCanvasRenderer::Destroy()
{
  CopyableCanvasRenderer::Destroy();

  if (mCanvasClient) {
    mCanvasClient->OnDetach();
    mCanvasClient = nullptr;
  }
}

bool
ShareableCanvasRenderer::UpdateTarget(DrawTarget* aDestTarget)
{
  MOZ_ASSERT(aDestTarget);
  if (!aDestTarget) {
    return false;
  }

  RefPtr<SourceSurface> surface;

  if (!mGLContext) {
    AutoReturnSnapshot autoReturn;

    if (mAsyncRenderer) {
      surface = mAsyncRenderer->GetSurface();
    } else if (mBufferProvider) {
      surface = mBufferProvider->BorrowSnapshot();
      autoReturn.mSnapshot = &surface;
      autoReturn.mBufferProvider = mBufferProvider;
    }

    MOZ_ASSERT(surface);
    if (!surface) {
      return false;
    }

    aDestTarget->CopySurface(surface,
                             IntRect(0, 0, mSize.width, mSize.height),
                             IntPoint(0, 0));
    return true;
  }

  gl::SharedSurface* frontbuffer = nullptr;
  if (mGLFrontbuffer) {
    frontbuffer = mGLFrontbuffer.get();
  } else {
    gl::GLScreenBuffer* screen = mGLContext->Screen();
    const auto& front = screen->Front();
    if (front) {
      frontbuffer = front->Surf();
    }
  }

  if (!frontbuffer) {
    NS_WARNING("Null frame received.");
    return false;
  }

  IntSize readSize(frontbuffer->mSize);
  SurfaceFormat format =
    mOpaque ? SurfaceFormat::B8G8R8X8 : SurfaceFormat::B8G8R8A8;
  bool needsPremult = frontbuffer->mHasAlpha && !mIsAlphaPremultiplied;

  // Try to read back directly into aDestTarget's output buffer
  uint8_t* destData;
  IntSize destSize;
  int32_t destStride;
  SurfaceFormat destFormat;
  if (aDestTarget->LockBits(&destData, &destSize, &destStride, &destFormat)) {
    if (destSize == readSize && destFormat == format) {
      RefPtr<DataSourceSurface> data =
        Factory::CreateWrappingDataSourceSurface(destData, destStride, destSize, destFormat);
      if (!mGLContext->Readback(frontbuffer, data)) {
        aDestTarget->ReleaseBits(destData);
        return false;
      }
      if (needsPremult) {
        gfxUtils::PremultiplyDataSurface(data, data);
      }
      aDestTarget->ReleaseBits(destData);
      return true;
    }
    aDestTarget->ReleaseBits(destData);
  }

  RefPtr<DataSourceSurface> resultSurf = GetTempSurface(readSize, format);
  // There will already be a warning from inside of GetTempSurface, but
  // it doesn't hurt to complain:
  if (NS_WARN_IF(!resultSurf)) {
    return false;
  }

  // Readback handles Flush/MarkDirty.
  if (!mGLContext->Readback(frontbuffer, resultSurf)) {
    return false;
  }
  if (needsPremult) {
    gfxUtils::PremultiplyDataSurface(resultSurf, resultSurf);
  }

  aDestTarget->CopySurface(resultSurf,
                           IntRect(0, 0, readSize.width, readSize.height),
                           IntPoint(0, 0));

  return true;
}

CanvasClient::CanvasClientType
ShareableCanvasRenderer::GetCanvasClientType()
{
  if (mAsyncRenderer) {
    return CanvasClient::CanvasClientAsync;
  }

  if (mGLContext) {
    return CanvasClient::CanvasClientTypeShSurf;
  }
  return CanvasClient::CanvasClientSurface;
}

void
ShareableCanvasRenderer::UpdateCompositableClient()
{
  if (!CreateCompositable()) {
    return;
  }

  if (mCanvasClient && mAsyncRenderer) {
    mCanvasClient->UpdateAsync(mAsyncRenderer);
  }

  if (!IsDirty()) {
    return;
  }
  ResetDirty();

  FirePreTransactionCallback();
  if (mBufferProvider && mBufferProvider->GetTextureClient()) {
    if (!mBufferProvider->SetKnowsCompositor(GetForwarder())) {
      gfxCriticalNote << "BufferProvider::SetForwarder failed";
      return;
    }
    mCanvasClient->UpdateFromTexture(mBufferProvider->GetTextureClient());
  } else {
    mCanvasClient->Update(gfx::IntSize(mSize.width, mSize.height), this);
  }

  FireDidTransactionCallback();

  mCanvasClient->Updated();
}

} // namespace layers
} // namespace mozilla
