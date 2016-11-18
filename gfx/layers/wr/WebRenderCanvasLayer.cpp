/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebRenderCanvasLayer.h"

#include "AsyncCanvasRenderer.h"
#include "gfxUtils.h"
#include "GLContext.h"
#include "GLScreenBuffer.h"
#include "LayersLogging.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/layers/TextureClientSharedSurface.h"
#include "mozilla/layers/WebRenderBridgeChild.h"
#include "PersistentBufferProvider.h"
#include "SharedSurface.h"

namespace mozilla {
namespace layers {

WebRenderCanvasLayer::~WebRenderCanvasLayer()
{
  MOZ_COUNT_DTOR(WebRenderCanvasLayer);
}

void
WebRenderCanvasLayer::RenderLayer()
{
  FirePreTransactionCallback();
  RefPtr<gfx::SourceSurface> surface;
  // Get the canvas buffer
  AutoReturnSnapshot autoReturn;
  if (mAsyncRenderer) {
    MOZ_ASSERT(!mBufferProvider);
    MOZ_ASSERT(!mGLContext);
    surface = mAsyncRenderer->GetSurface();
  } else if (mGLContext) {
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
      return;
    }

    gfx::IntSize readSize(frontbuffer->mSize);
    gfx::SurfaceFormat format = (GetContentFlags() & CONTENT_OPAQUE)
                                ? gfx::SurfaceFormat::B8G8R8X8
                                : gfx::SurfaceFormat::B8G8R8A8;
    bool needsPremult = frontbuffer->mHasAlpha && !mIsAlphaPremultiplied;

    RefPtr<gfx::DataSourceSurface> resultSurf = GetTempSurface(readSize, format);
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
    surface = resultSurf;
  } else if (mBufferProvider) {
    surface = mBufferProvider->BorrowSnapshot();
    autoReturn.mSnapshot = &surface;
    autoReturn.mBufferProvider = mBufferProvider;
  }
  FireDidTransactionCallback();

  if (!surface) {
    return;
  }

  WRScrollFrameStackingContextGenerator scrollFrames(this);

  RefPtr<gfx::DataSourceSurface> dataSurface = surface->GetDataSurface();
  gfx::DataSourceSurface::ScopedMap map(dataSurface, gfx::DataSourceSurface::MapType::READ);
  //XXX
  MOZ_RELEASE_ASSERT(surface->GetFormat() == gfx::SurfaceFormat::B8G8R8X8 ||
                     surface->GetFormat() == gfx::SurfaceFormat::B8G8R8A8, "bad format");

  gfx::IntSize size = surface->GetSize();

  WRImageKey key;
  gfx::ByteBuffer buf(size.height * map.GetStride(), map.GetData());
  WRBridge()->SendAddImage(size.width, size.height, map.GetStride(), RGBA8, buf, &key);

  gfx::Matrix4x4 transform;// = GetTransform();
  const bool needsYFlip = (mOriginPos == gl::OriginPos::BottomLeft);
  if (needsYFlip) {
    transform.PreTranslate(0, size.height, 0).PreScale(1, -1, 1);
  }
  gfx::Rect rect(0, 0, size.width, size.height);

  gfx::Rect clip;
  if (GetClipRect().isSome()) {
      clip = RelativeToTransformedVisible(IntRectToRect(GetClipRect().ref().ToUnknownRect()));
  } else {
      clip = rect;
  }
  if (gfxPrefs::LayersDump()) printf_stderr("CanvasLayer %p using rect:%s clip:%s\n", this, Stringify(rect).c_str(), Stringify(clip).c_str());
  WRBridge()->AddWebRenderCommand(OpPushDLBuilder());
  WRBridge()->AddWebRenderCommand(OpDPPushImage(toWrRect(rect), toWrRect(clip), Nothing(), key));
  Manager()->AddImageKeyForDiscard(key);

  gfx::Rect relBounds = TransformedVisibleBoundsRelativeToParent();
  if (gfxPrefs::LayersDump()) printf_stderr("CanvasLayer %p using %s as bounds/overflow, %s for transform\n", this, Stringify(relBounds).c_str(), Stringify(transform).c_str());
  WRBridge()->AddWebRenderCommand(
    OpPopDLBuilder(toWrRect(relBounds), toWrRect(relBounds), transform, FrameMetrics::NULL_SCROLL_ID));
}

} // namespace layers
} // namespace mozilla
