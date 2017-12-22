/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CopyableCanvasRenderer.h"

#include "BasicLayersImpl.h"            // for FillWithMask, etc
#include "GLContext.h"                  // for GLContext
#include "GLScreenBuffer.h"             // for GLScreenBuffer
#include "SharedSurface.h"              // for SharedSurface
#include "SharedSurfaceGL.h"              // for SharedSurface
#include "gfxPattern.h"                 // for gfxPattern, etc
#include "gfxPlatform.h"                // for gfxPlatform, gfxImageFormat
#include "gfxRect.h"                    // for gfxRect
#include "gfxUtils.h"                   // for gfxUtils
#include "gfx2DGlue.h"                  // for thebes --> moz2d transition
#include "mozilla/gfx/BaseSize.h"       // for BaseSize
#include "mozilla/gfx/Tools.h"
#include "mozilla/gfx/Point.h"          // for IntSize
#include "mozilla/layers/AsyncCanvasRenderer.h"
#include "mozilla/layers/PersistentBufferProvider.h"
#include "nsDebug.h"                    // for NS_ASSERTION, NS_WARNING, etc
#include "nsISupportsImpl.h"            // for gfxContext::AddRef, etc
#include "nsRect.h"                     // for mozilla::gfx::IntRect
#include "gfxUtils.h"
#include "client/TextureClientSharedSurface.h"

namespace mozilla {
namespace layers {

using namespace mozilla::gfx;
using namespace mozilla::gl;

CopyableCanvasRenderer::CopyableCanvasRenderer()
  : mGLContext(nullptr)
  , mBufferProvider(nullptr)
  , mGLFrontbuffer(nullptr)
  , mAsyncRenderer(nullptr)
  , mIsAlphaPremultiplied(true)
  , mOriginPos(gl::OriginPos::TopLeft)
  , mOpaque(true)
  , mCachedTempSurface(nullptr)
{
  MOZ_COUNT_CTOR(CopyableCanvasRenderer);
}

CopyableCanvasRenderer::~CopyableCanvasRenderer()
{
  Destroy();
  MOZ_COUNT_DTOR(CopyableCanvasRenderer);
}

void
CopyableCanvasRenderer::Initialize(const CanvasInitializeData& aData)
{
  CanvasRenderer::Initialize(aData);

  if (aData.mGLContext) {
    mGLContext = aData.mGLContext;
    mIsAlphaPremultiplied = aData.mIsGLAlphaPremult;
    mOriginPos = gl::OriginPos::BottomLeft;

    MOZ_ASSERT(mGLContext->IsOffscreen(), "canvas gl context isn't offscreen");

    if (aData.mFrontbufferGLTex) {
      gfx::IntSize size(aData.mSize.width, aData.mSize.height);
      mGLFrontbuffer = SharedSurface_Basic::Wrap(aData.mGLContext, size, aData.mHasAlpha,
                                                 aData.mFrontbufferGLTex);
      mBufferProvider = aData.mBufferProvider;
    }
  } else if (aData.mBufferProvider) {
    mBufferProvider = aData.mBufferProvider;
  } else if (aData.mRenderer) {
    mAsyncRenderer = aData.mRenderer;
    mOriginPos = gl::OriginPos::BottomLeft;
  } else {
    MOZ_CRASH("GFX: CanvasRenderer created without BufferProvider, DrawTarget or GLContext?");
  }

  mOpaque = !aData.mHasAlpha;
}

bool
CopyableCanvasRenderer::IsDataValid(const CanvasInitializeData& aData)
{
  return mGLContext == aData.mGLContext;
}

void
CopyableCanvasRenderer::ClearCachedResources()
{
  if (mBufferProvider) {
    mBufferProvider->ClearCachedResources();
  }

  mCachedTempSurface = nullptr;
}

void
CopyableCanvasRenderer::Destroy()
{
  if (mBufferProvider) {
    mBufferProvider->ClearCachedResources();
  }

  mBufferProvider = nullptr;
  mCachedTempSurface = nullptr;
}

already_AddRefed<SourceSurface>
CopyableCanvasRenderer::ReadbackSurface()
{
  struct ScopedFireTransactionCallback {
    explicit ScopedFireTransactionCallback(CopyableCanvasRenderer* aRenderer)
      : mRenderer(aRenderer)
    {
      mRenderer->FirePreTransactionCallback();
    }

    ~ScopedFireTransactionCallback()
    {
      mRenderer->FireDidTransactionCallback();
    }

    CopyableCanvasRenderer* mRenderer;
  };

  ScopedFireTransactionCallback callback(this);
  if (mAsyncRenderer) {
    MOZ_ASSERT(!mBufferProvider);
    MOZ_ASSERT(!mGLContext);
    return mAsyncRenderer->GetSurface();
  }

  if (!mGLContext) {
    return nullptr;
  }

  SharedSurface* frontbuffer = nullptr;
  if (mGLFrontbuffer) {
    frontbuffer = mGLFrontbuffer.get();
  } else {
    GLScreenBuffer* screen = mGLContext->Screen();
    const auto& front = screen->Front();
    if (front) {
      frontbuffer = front->Surf();
    }
  }

  if (!frontbuffer) {
    NS_WARNING("Null frame received.");
    return nullptr;
  }

  IntSize readSize(frontbuffer->mSize);
  SurfaceFormat format = frontbuffer->mHasAlpha ? SurfaceFormat::B8G8R8A8
                                                : SurfaceFormat::B8G8R8X8;
  bool needsPremult = frontbuffer->mHasAlpha && !mIsAlphaPremultiplied;

  RefPtr<DataSourceSurface> resultSurf = GetTempSurface(readSize, format);
  // There will already be a warning from inside of GetTempSurface, but
  // it doesn't hurt to complain:
  if (NS_WARN_IF(!resultSurf)) {
    return nullptr;
  }

  // Readback handles Flush/MarkDirty.
  if (!mGLContext->Readback(frontbuffer, resultSurf)) {
    NS_WARNING("Failed to read back canvas surface.");
    return nullptr;
  }
  if (needsPremult) {
    gfxUtils::PremultiplyDataSurface(resultSurf, resultSurf);
  }
  MOZ_ASSERT(resultSurf);

  return resultSurf.forget();
}

DataSourceSurface*
CopyableCanvasRenderer::GetTempSurface(const IntSize& aSize,
                                    const SurfaceFormat aFormat)
{
  if (!mCachedTempSurface ||
      aSize != mCachedTempSurface->GetSize() ||
      aFormat != mCachedTempSurface->GetFormat())
  {
    // Create a surface aligned to 8 bytes since that's the highest alignment WebGL can handle.
    uint32_t stride = GetAlignedStride<8>(aSize.width, BytesPerPixel(aFormat));
    mCachedTempSurface = Factory::CreateDataSourceSurfaceWithStride(aSize, aFormat, stride);
  }

  return mCachedTempSurface;
}

} // namespace layers
} // namespace mozilla
