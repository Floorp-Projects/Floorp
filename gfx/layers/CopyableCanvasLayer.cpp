/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CopyableCanvasLayer.h"

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

CopyableCanvasLayer::CopyableCanvasLayer(LayerManager* aLayerManager, void *aImplData) :
  CanvasLayer(aLayerManager, aImplData)
  , mGLFrontbuffer(nullptr)
  , mIsAlphaPremultiplied(true)
  , mOriginPos(gl::OriginPos::TopLeft)
  , mIsMirror(false)
{
  MOZ_COUNT_CTOR(CopyableCanvasLayer);
}

CopyableCanvasLayer::~CopyableCanvasLayer()
{
  MOZ_COUNT_DTOR(CopyableCanvasLayer);
}

void
CopyableCanvasLayer::Initialize(const Data& aData)
{
  if (aData.mGLContext) {
    mGLContext = aData.mGLContext;
    mIsAlphaPremultiplied = aData.mIsGLAlphaPremult;
    mOriginPos = gl::OriginPos::BottomLeft;
    mIsMirror = aData.mIsMirror;

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
    MOZ_CRASH("GFX: CanvasLayer created without BufferProvider, DrawTarget or GLContext?");
  }

  mBounds.SetRect(0, 0, aData.mSize.width, aData.mSize.height);
}

bool
CopyableCanvasLayer::IsDataValid(const Data& aData)
{
  return mGLContext == aData.mGLContext;
}

void
CopyableCanvasLayer::UpdateTarget(DrawTarget* aDestTarget)
{
  MOZ_ASSERT(aDestTarget);
  if (!aDestTarget) {
    return;
  }

  RefPtr<SourceSurface> surface;

  if (!mGLContext) {
    AutoReturnSnapshot autoReturn;

    if (mAsyncRenderer) {
      surface = mAsyncRenderer->GetSurface();
    } else if (mBufferProvider && !mGLContext) {
      surface = mBufferProvider->BorrowSnapshot();
      autoReturn.mSnapshot = &surface;
      autoReturn.mBufferProvider = mBufferProvider;
    }

    NS_ASSERTION(surface, "Must have surface to draw!");
    if (surface) {
      aDestTarget->CopySurface(surface,
                               IntRect(0, 0, mBounds.width, mBounds.height),
                               IntPoint(0, 0));
    }

    return;
  }

  MOZ_ASSERT(!mBufferProvider);
  MOZ_ASSERT(!mAsyncRenderer);

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
    return;
  }

  IntSize readSize(frontbuffer->mSize);
  SurfaceFormat format = (GetContentFlags() & CONTENT_OPAQUE)
                          ? SurfaceFormat::B8G8R8X8
                          : SurfaceFormat::B8G8R8A8;
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
      mGLContext->Readback(frontbuffer, data);
      if (needsPremult) {
        gfxUtils::PremultiplyDataSurface(data, data);
      }
      aDestTarget->ReleaseBits(destData);
      return;
    }
    aDestTarget->ReleaseBits(destData);
  }

  RefPtr<DataSourceSurface> resultSurf = GetTempSurface(readSize, format);
  // There will already be a warning from inside of GetTempSurface, but
  // it doesn't hurt to complain:
  if (NS_WARN_IF(!resultSurf)) {
    return;
  }

  // Readback handles Flush/MarkDirty.
  mGLContext->Readback(frontbuffer, resultSurf);
  if (needsPremult) {
    gfxUtils::PremultiplyDataSurface(resultSurf, resultSurf);
  }

  aDestTarget->CopySurface(resultSurf,
                           IntRect(0, 0, readSize.width, readSize.height),
                           IntPoint(0, 0));
}

DataSourceSurface*
CopyableCanvasLayer::GetTempSurface(const IntSize& aSize,
                                    const SurfaceFormat aFormat)
{
  if (!mCachedTempSurface ||
      aSize != mCachedTempSurface->GetSize() ||
      aFormat != mCachedTempSurface->GetFormat())
  {
    // Create a surface aligned to 8 bytes since that's the highest alignment WebGL can handle.
    uint32_t stride = GetAlignedStride<8>(aSize.width * BytesPerPixel(aFormat));
    mCachedTempSurface = Factory::CreateDataSourceSurfaceWithStride(aSize, aFormat, stride);
  }

  return mCachedTempSurface;
}

} // namespace layers
} // namespace mozilla
