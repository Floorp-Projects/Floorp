/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/Compositor.h"
#include "mozilla/layers/CompositionRecorder.h"
#include "base/message_loop.h"  // for MessageLoop
#include "mozilla/gfx/Types.h"
#include "mozilla/layers/CompositorBridgeParent.h"  // for CompositorBridgeParent
#include "mozilla/layers/Diagnostics.h"
#include "mozilla/layers/Effects.h"  // for Effect, EffectChain, etc
#include "mozilla/layers/TextureClient.h"
#include "mozilla/layers/TextureHost.h"
#include "mozilla/layers/CompositorThread.h"
#include "mozilla/mozalloc.h"  // for operator delete, etc
#include "GeckoProfiler.h"
#include "gfx2DGlue.h"
#include "gfxUtils.h"
#include "nsAppRunner.h"

namespace mozilla::layers {

class CompositorRecordedFrame final : public RecordedFrame {
 public:
  CompositorRecordedFrame(const TimeStamp& aTimeStamp,
                          RefPtr<AsyncReadbackBuffer>&& aBuffer)
      : RecordedFrame(aTimeStamp), mBuffer(aBuffer) {}

  virtual already_AddRefed<gfx::DataSourceSurface> GetSourceSurface() override {
    if (mSurface) {
      return do_AddRef(mSurface);
    }

    gfx::IntSize size = mBuffer->GetSize();

    mSurface = gfx::Factory::CreateDataSourceSurface(
        size, gfx::SurfaceFormat::B8G8R8A8,
        /* aZero = */ false);

    if (!mBuffer->MapAndCopyInto(mSurface, size)) {
      mSurface = nullptr;
      return nullptr;
    }

    return do_AddRef(mSurface);
  }

 private:
  RefPtr<AsyncReadbackBuffer> mBuffer;
  RefPtr<gfx::DataSourceSurface> mSurface;
};

Compositor::Compositor(widget::CompositorWidget* aWidget)
    : mWidget(aWidget),
      mIsDestroyed(false),
#if defined(MOZ_WIDGET_ANDROID)
      // If the default color isn't white for Fennec, there is a black
      // flash before the first page of a tab is loaded.
      mClearColor(gfx::ToDeviceColor(gfx::sRGBColor::OpaqueWhite()))
#else
      mClearColor(gfx::DeviceColor())
#endif
{
}

Compositor::~Compositor() {}

void Compositor::Destroy() {
  mWidget = nullptr;

  TextureSourceProvider::Destroy();
  mIsDestroyed = true;
}

void Compositor::EndFrame() { mLastCompositionEndTime = TimeStamp::Now(); }

nsTArray<TexturedVertex> TexturedTrianglesToVertexArray(
    const nsTArray<gfx::TexturedTriangle>& aTriangles) {
  const auto VertexFromPoints = [](const gfx::Point& p, const gfx::Point& t) {
    return TexturedVertex{{p.x, p.y}, {t.x, t.y}};
  };

  nsTArray<TexturedVertex> vertices;

  for (const gfx::TexturedTriangle& t : aTriangles) {
    vertices.AppendElement(VertexFromPoints(t.p1, t.textureCoords.p1));
    vertices.AppendElement(VertexFromPoints(t.p2, t.textureCoords.p2));
    vertices.AppendElement(VertexFromPoints(t.p3, t.textureCoords.p3));
  }

  return vertices;
}

static float WrapTexCoord(float v) {
  // This should return values in range [0, 1.0)
  return v - floorf(v);
}

static void SetRects(size_t n, decomposedRectArrayT* aLayerRects,
                     decomposedRectArrayT* aTextureRects, float x0, float y0,
                     float x1, float y1, float tx0, float ty0, float tx1,
                     float ty1, bool flip_y) {
  if (flip_y) {
    std::swap(ty0, ty1);
  }
  (*aLayerRects)[n] = gfx::Rect(x0, y0, x1 - x0, y1 - y0);
  (*aTextureRects)[n] = gfx::Rect(tx0, ty0, tx1 - tx0, ty1 - ty0);
}

#ifdef DEBUG
static inline bool FuzzyEqual(float a, float b) {
  return fabs(a - b) < 0.0001f;
}
static inline bool FuzzyLTE(float a, float b) { return a <= b + 0.0001f; }
#endif

size_t DecomposeIntoNoRepeatRects(const gfx::Rect& aRect,
                                  const gfx::Rect& aTexCoordRect,
                                  decomposedRectArrayT* aLayerRects,
                                  decomposedRectArrayT* aTextureRects) {
  gfx::Rect texCoordRect = aTexCoordRect;

  // If the texture should be flipped, it will have negative height. Detect that
  // here and compensate for it. We will flip each rect as we emit it.
  bool flipped = false;
  if (texCoordRect.Height() < 0) {
    flipped = true;
    texCoordRect.MoveByY(texCoordRect.Height());
    texCoordRect.SetHeight(-texCoordRect.Height());
  }

  // Wrap the texture coordinates so they are within [0,1] and cap width/height
  // at 1. We rely on this below.
  texCoordRect = gfx::Rect(gfx::Point(WrapTexCoord(texCoordRect.X()),
                                      WrapTexCoord(texCoordRect.Y())),
                           gfx::Size(std::min(texCoordRect.Width(), 1.0f),
                                     std::min(texCoordRect.Height(), 1.0f)));

  NS_ASSERTION(
      texCoordRect.X() >= 0.0f && texCoordRect.X() <= 1.0f &&
          texCoordRect.Y() >= 0.0f && texCoordRect.Y() <= 1.0f &&
          texCoordRect.Width() >= 0.0f && texCoordRect.Width() <= 1.0f &&
          texCoordRect.Height() >= 0.0f && texCoordRect.Height() <= 1.0f &&
          texCoordRect.XMost() >= 0.0f && texCoordRect.XMost() <= 2.0f &&
          texCoordRect.YMost() >= 0.0f && texCoordRect.YMost() <= 2.0f,
      "We just wrapped the texture coordinates, didn't we?");

  // Get the top left and bottom right points of the rectangle. Note that
  // tl.x/tl.y are within [0,1] but br.x/br.y are within [0,2].
  gfx::Point tl = texCoordRect.TopLeft();
  gfx::Point br = texCoordRect.BottomRight();

  NS_ASSERTION(tl.x >= 0.0f && tl.x <= 1.0f && tl.y >= 0.0f && tl.y <= 1.0f &&
                   br.x >= tl.x && br.x <= 2.0f && br.y >= tl.y &&
                   br.y <= 2.0f && FuzzyLTE(br.x - tl.x, 1.0f) &&
                   FuzzyLTE(br.y - tl.y, 1.0f),
               "Somehow generated invalid texture coordinates");

  // Then check if we wrap in either the x or y axis.
  bool xwrap = br.x > 1.0f;
  bool ywrap = br.y > 1.0f;

  // If xwrap is false, the texture will be sampled from tl.x .. br.x.
  // If xwrap is true, then it will be split into tl.x .. 1.0, and
  // 0.0 .. WrapTexCoord(br.x). Same for the Y axis. The destination
  // rectangle is also split appropriately, according to the calculated
  // xmid/ymid values.
  if (!xwrap && !ywrap) {
    SetRects(0, aLayerRects, aTextureRects, aRect.X(), aRect.Y(), aRect.XMost(),
             aRect.YMost(), tl.x, tl.y, br.x, br.y, flipped);
    return 1;
  }

  // If we are dealing with wrapping br.x and br.y are greater than 1.0 so
  // wrap them here as well.
  br = gfx::Point(xwrap ? WrapTexCoord(br.x.value) : br.x.value,
                  ywrap ? WrapTexCoord(br.y.value) : br.y.value);

  // If we wrap around along the x axis, we will draw first from
  // tl.x .. 1.0 and then from 0.0 .. br.x (which we just wrapped above).
  // The same applies for the Y axis. The midpoints we calculate here are
  // only valid if we actually wrap around.
  GLfloat xmid =
      aRect.X() + (1.0f - tl.x) / texCoordRect.Width() * aRect.Width();
  GLfloat ymid =
      aRect.Y() + (1.0f - tl.y) / texCoordRect.Height() * aRect.Height();

  // Due to floating-point inaccuracy, we have to use XMost()-x and YMost()-y
  // to calculate width and height, respectively, to ensure that size will
  // remain consistent going from absolute to relative and back again.
  NS_ASSERTION(
      !xwrap || (xmid >= aRect.X() && xmid <= aRect.XMost() &&
                 FuzzyEqual((xmid - aRect.X()) + (aRect.XMost() - xmid),
                            aRect.XMost() - aRect.X())),
      "xmid should be within [x,XMost()] and the wrapped rect should have the "
      "same width");
  NS_ASSERTION(
      !ywrap || (ymid >= aRect.Y() && ymid <= aRect.YMost() &&
                 FuzzyEqual((ymid - aRect.Y()) + (aRect.YMost() - ymid),
                            aRect.YMost() - aRect.Y())),
      "ymid should be within [y,YMost()] and the wrapped rect should have the "
      "same height");

  if (!xwrap && ywrap) {
    SetRects(0, aLayerRects, aTextureRects, aRect.X(), aRect.Y(), aRect.XMost(),
             ymid, tl.x, tl.y, br.x, 1.0f, flipped);
    SetRects(1, aLayerRects, aTextureRects, aRect.X(), ymid, aRect.XMost(),
             aRect.YMost(), tl.x, 0.0f, br.x, br.y, flipped);
    return 2;
  }

  if (xwrap && !ywrap) {
    SetRects(0, aLayerRects, aTextureRects, aRect.X(), aRect.Y(), xmid,
             aRect.YMost(), tl.x, tl.y, 1.0f, br.y, flipped);
    SetRects(1, aLayerRects, aTextureRects, xmid, aRect.Y(), aRect.XMost(),
             aRect.YMost(), 0.0f, tl.y, br.x, br.y, flipped);
    return 2;
  }

  SetRects(0, aLayerRects, aTextureRects, aRect.X(), aRect.Y(), xmid, ymid,
           tl.x, tl.y, 1.0f, 1.0f, flipped);
  SetRects(1, aLayerRects, aTextureRects, xmid, aRect.Y(), aRect.XMost(), ymid,
           0.0f, tl.y, br.x, 1.0f, flipped);
  SetRects(2, aLayerRects, aTextureRects, aRect.X(), ymid, xmid, aRect.YMost(),
           tl.x, 0.0f, 1.0f, br.y, flipped);
  SetRects(3, aLayerRects, aTextureRects, xmid, ymid, aRect.XMost(),
           aRect.YMost(), 0.0f, 0.0f, br.x, br.y, flipped);
  return 4;
}

bool Compositor::ShouldRecordFrames() const {
  return profiler_feature_active(ProfilerFeature::Screenshots) || mRecordFrames;
}

}  // namespace mozilla::layers
