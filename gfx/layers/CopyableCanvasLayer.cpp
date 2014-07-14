/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BasicLayersImpl.h"            // for FillWithMask, etc
#include "CopyableCanvasLayer.h"
#include "GLContext.h"                  // for GLContext
#include "GLScreenBuffer.h"             // for GLScreenBuffer
#include "SharedSurface.h"              // for SharedSurface
#include "SharedSurfaceGL.h"              // for SharedSurface
#include "SurfaceStream.h"              // for SurfaceStream
#include "gfxMatrix.h"                  // for gfxMatrix
#include "gfxPattern.h"                 // for gfxPattern, etc
#include "gfxPlatform.h"                // for gfxPlatform, gfxImageFormat
#include "gfxRect.h"                    // for gfxRect
#include "gfxUtils.h"                   // for gfxUtils
#include "gfx2DGlue.h"                  // for thebes --> moz2d transition
#include "mozilla/gfx/BaseSize.h"       // for BaseSize
#include "mozilla/gfx/Tools.h"
#include "nsDebug.h"                    // for NS_ASSERTION, NS_WARNING, etc
#include "nsISupportsImpl.h"            // for gfxContext::AddRef, etc
#include "nsRect.h"                     // for nsIntRect
#include "nsSize.h"                     // for nsIntSize
#include "gfxUtils.h"

namespace mozilla {
namespace layers {

using namespace mozilla::gfx;
using namespace mozilla::gl;

CopyableCanvasLayer::CopyableCanvasLayer(LayerManager* aLayerManager, void *aImplData) :
  CanvasLayer(aLayerManager, aImplData)
  , mStream(nullptr)
  , mIsAlphaPremultiplied(true)
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
  NS_ASSERTION(mSurface == nullptr, "BasicCanvasLayer::Initialize called twice!");

  if (aData.mGLContext) {
    mGLContext = aData.mGLContext;
    mStream = aData.mStream;
    mIsAlphaPremultiplied = aData.mIsGLAlphaPremult;
    mNeedsYFlip = true;
    MOZ_ASSERT(mGLContext->IsOffscreen(), "canvas gl context isn't offscreen");

    // [Basic Layers, non-OMTC] WebGL layer init.
    // `GLScreenBuffer::Morph`ing is only needed in BasicShadowableCanvasLayer.
  } else if (aData.mDrawTarget) {
    mDrawTarget = aData.mDrawTarget;
    mSurface = mDrawTarget->Snapshot();
    mNeedsYFlip = false;
  } else {
    NS_ERROR("CanvasLayer created without mSurface, mDrawTarget or mGLContext?");
  }

  mBounds.SetRect(0, 0, aData.mSize.width, aData.mSize.height);
}

bool
CopyableCanvasLayer::IsDataValid(const Data& aData)
{
  return mGLContext == aData.mGLContext && mStream == aData.mStream;
}

void
CopyableCanvasLayer::UpdateTarget(DrawTarget* aDestTarget)
{
  if (!IsDirty())
    return;
  Painted();

  if (mDrawTarget) {
    mDrawTarget->Flush();
    mSurface = mDrawTarget->Snapshot();
  }

  if (!mGLContext && aDestTarget) {
    NS_ASSERTION(mSurface, "Must have surface to draw!");
    if (mSurface) {
      aDestTarget->CopySurface(mSurface,
                               IntRect(0, 0, mBounds.width, mBounds.height),
                               IntPoint(0, 0));
      mSurface = nullptr;
    }
    return;
  }

  if (mGLContext) {
    SharedSurface* sharedSurf = nullptr;
    if (mStream) {
      sharedSurf = mStream->SwapConsumer();
    } else {
      sharedSurf = mGLContext->RequestFrame();
    }

    if (!sharedSurf) {
      NS_WARNING("Null frame received.");
      return;
    }

    IntSize readSize(sharedSurf->mSize);
    SurfaceFormat format = (GetContentFlags() & CONTENT_OPAQUE)
                            ? SurfaceFormat::B8G8R8X8
                            : SurfaceFormat::B8G8R8A8;
    bool needsPremult = sharedSurf->mHasAlpha && !mIsAlphaPremultiplied;

    // Try to read back directly into aDestTarget's output buffer
    if (aDestTarget) {
      uint8_t* destData;
      IntSize destSize;
      int32_t destStride;
      SurfaceFormat destFormat;
      if (aDestTarget->LockBits(&destData, &destSize, &destStride, &destFormat)) {
        if (destSize == readSize && destFormat == format) {
          RefPtr<DataSourceSurface> data =
            Factory::CreateWrappingDataSourceSurface(destData, destStride, destSize, destFormat);
          mGLContext->Screen()->Readback(sharedSurf, data);
          if (needsPremult) {
              gfxUtils::PremultiplyDataSurface(data, data);
          }
          aDestTarget->ReleaseBits(destData);
          return;
        }
        aDestTarget->ReleaseBits(destData);
      }
    }

    RefPtr<SourceSurface> resultSurf;
    if (sharedSurf->mType == SharedSurfaceType::Basic && !needsPremult) {
      SharedSurface_Basic* sharedSurf_Basic = SharedSurface_Basic::Cast(sharedSurf);
      resultSurf = sharedSurf_Basic->GetData();
    } else {
      RefPtr<DataSourceSurface> data = GetTempSurface(readSize, format);
      // Readback handles Flush/MarkDirty.
      mGLContext->Screen()->Readback(sharedSurf, data);
      if (needsPremult) {
        gfxUtils::PremultiplyDataSurface(data, data);
      }
      resultSurf = data;
    }
    MOZ_ASSERT(resultSurf);

    if (aDestTarget) {
      aDestTarget->CopySurface(resultSurf,
                               IntRect(0, 0, readSize.width, readSize.height),
                               IntPoint(0, 0));
    } else {
      // If !aDestSurface then we will end up painting using mSurface, so
      // stick our surface into mSurface, so that the Paint() path is the same.
      mSurface = resultSurf;
    }
  }
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

void
CopyableCanvasLayer::DiscardTempSurface()
{
  mCachedTempSurface = nullptr;
}

}
}
