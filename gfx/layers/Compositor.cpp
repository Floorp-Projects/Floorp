/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/Compositor.h"
#include "base/message_loop.h"                      // for MessageLoop
#include "mozilla/layers/CompositorBridgeParent.h"  // for CompositorBridgeParent
#include "mozilla/layers/Diagnostics.h"
#include "mozilla/layers/Effects.h"  // for Effect, EffectChain, etc
#include "mozilla/layers/TextureClient.h"
#include "mozilla/layers/TextureHost.h"
#include "mozilla/layers/CompositorThread.h"
#include "mozilla/mozalloc.h"  // for operator delete, etc
#include "GeckoProfiler.h"
#include "gfx2DGlue.h"
#include "nsAppRunner.h"
#include "LayersHelpers.h"

namespace mozilla {

namespace layers {

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

Compositor::Compositor(widget::CompositorWidget* aWidget,
                       CompositorBridgeParent* aParent)
    : mDiagnosticTypes(DiagnosticTypes::NO_DIAGNOSTIC),
      mParent(aParent),
      mPixelsPerFrame(0),
      mPixelsFilled(0),
      mScreenRotation(ROTATION_0),
      mWidget(aWidget),
      mIsDestroyed(false)
#if defined(MOZ_WIDGET_ANDROID)
      // If the default color isn't white for Fennec, there is a black
      // flash before the first page of a tab is loaded.
      ,
      mClearColor(1.0, 1.0, 1.0, 1.0),
      mDefaultClearColor(1.0, 1.0, 1.0, 1.0)
#else
      ,
      mClearColor(0.0, 0.0, 0.0, 0.0),
      mDefaultClearColor(0.0, 0.0, 0.0, 0.0)
#endif
{
}

Compositor::~Compositor() { ReadUnlockTextures(); }

void Compositor::Destroy() {
  mWidget = nullptr;

  TextureSourceProvider::Destroy();
  mIsDestroyed = true;
}

void Compositor::EndFrame() {
  ReadUnlockTextures();
  mLastCompositionEndTime = TimeStamp::Now();
}

bool Compositor::ShouldDrawDiagnostics(DiagnosticFlags aFlags) {
  if ((aFlags & DiagnosticFlags::TILE) &&
      !(mDiagnosticTypes & DiagnosticTypes::TILE_BORDERS)) {
    return false;
  }
  if ((aFlags & DiagnosticFlags::BIGIMAGE) &&
      !(mDiagnosticTypes & DiagnosticTypes::BIGIMAGE_BORDERS)) {
    return false;
  }
  if (mDiagnosticTypes == DiagnosticTypes::NO_DIAGNOSTIC) {
    return false;
  }
  return true;
}

void Compositor::DrawDiagnostics(DiagnosticFlags aFlags,
                                 const nsIntRegion& aVisibleRegion,
                                 const gfx::IntRect& aClipRect,
                                 const gfx::Matrix4x4& aTransform,
                                 uint32_t aFlashCounter) {
  if (!ShouldDrawDiagnostics(aFlags)) {
    return;
  }

  if (aVisibleRegion.GetNumRects() > 1) {
    for (auto iter = aVisibleRegion.RectIter(); !iter.Done(); iter.Next()) {
      DrawDiagnostics(aFlags | DiagnosticFlags::REGION_RECT,
                      IntRectToRect(iter.Get()), aClipRect, aTransform,
                      aFlashCounter);
    }
  }

  DrawDiagnostics(aFlags, IntRectToRect(aVisibleRegion.GetBounds()), aClipRect,
                  aTransform, aFlashCounter);
}

void Compositor::DrawDiagnostics(DiagnosticFlags aFlags,
                                 const gfx::Rect& aVisibleRect,
                                 const gfx::IntRect& aClipRect,
                                 const gfx::Matrix4x4& aTransform,
                                 uint32_t aFlashCounter) {
  if (!ShouldDrawDiagnostics(aFlags)) {
    return;
  }

  DrawDiagnosticsInternal(aFlags, aVisibleRect, aClipRect, aTransform,
                          aFlashCounter);
}

void Compositor::DrawDiagnosticsInternal(DiagnosticFlags aFlags,
                                         const gfx::Rect& aVisibleRect,
                                         const gfx::IntRect& aClipRect,
                                         const gfx::Matrix4x4& aTransform,
                                         uint32_t aFlashCounter) {
#ifdef ANDROID
  int lWidth = 10;
#else
  int lWidth = 2;
#endif

  gfx::Color color;
  if (aFlags & DiagnosticFlags::CONTENT) {
    color = gfx::Color(0.0f, 1.0f, 0.0f, 1.0f);  // green
    if (aFlags & DiagnosticFlags::COMPONENT_ALPHA) {
      color = gfx::Color(0.0f, 1.0f, 1.0f, 1.0f);  // greenish blue
    }
  } else if (aFlags & DiagnosticFlags::IMAGE) {
    if (aFlags & DiagnosticFlags::NV12) {
      color = gfx::Color(1.0f, 1.0f, 0.0f, 1.0f);  // yellow
    } else if (aFlags & DiagnosticFlags::YCBCR) {
      color = gfx::Color(1.0f, 0.55f, 0.0f, 1.0f);  // orange
    } else {
      color = gfx::Color(1.0f, 0.0f, 0.0f, 1.0f);  // red
    }
  } else if (aFlags & DiagnosticFlags::COLOR) {
    color = gfx::Color(0.0f, 0.0f, 1.0f, 1.0f);  // blue
  } else if (aFlags & DiagnosticFlags::CONTAINER) {
    color = gfx::Color(0.8f, 0.0f, 0.8f, 1.0f);  // purple
  }

  // make tile borders a bit more transparent to keep layer borders readable.
  if (aFlags & DiagnosticFlags::TILE || aFlags & DiagnosticFlags::BIGIMAGE ||
      aFlags & DiagnosticFlags::REGION_RECT) {
    lWidth = 1;
    color.r *= 0.7f;
    color.g *= 0.7f;
    color.b *= 0.7f;
    color.a = color.a * 0.5f;
  } else {
    color.a = color.a * 0.7f;
  }

  if (mDiagnosticTypes & DiagnosticTypes::FLASH_BORDERS) {
    float flash = (float)aFlashCounter / (float)DIAGNOSTIC_FLASH_COUNTER_MAX;
    color.r *= flash;
    color.g *= flash;
    color.b *= flash;
  }

  SlowDrawRect(aVisibleRect, color, aClipRect, aTransform, lWidth);
}

static void UpdateTextureCoordinates(gfx::TexturedTriangle& aTriangle,
                                     const gfx::Rect& aRect,
                                     const gfx::Rect& aIntersection,
                                     const gfx::Rect& aTextureCoords) {
  // Calculate the relative offset of the intersection within the layer.
  float dx = (aIntersection.X() - aRect.X()) / aRect.Width();
  float dy = (aIntersection.Y() - aRect.Y()) / aRect.Height();

  // Update the texture offset.
  float x = aTextureCoords.X() + dx * aTextureCoords.Width();
  float y = aTextureCoords.Y() + dy * aTextureCoords.Height();

  // Scale the texture width and height.
  float w = aTextureCoords.Width() * aIntersection.Width() / aRect.Width();
  float h = aTextureCoords.Height() * aIntersection.Height() / aRect.Height();

  static const auto Clamp = [](float& f) {
    if (f >= 1.0f) f = 1.0f;
    if (f <= 0.0f) f = 0.0f;
  };

  auto UpdatePoint = [&](const gfx::Point& p, gfx::Point& t) {
    t.x = x + (p.x - aIntersection.X()) / aIntersection.Width() * w;
    t.y = y + (p.y - aIntersection.Y()) / aIntersection.Height() * h;

    Clamp(t.x);
    Clamp(t.y);
  };

  UpdatePoint(aTriangle.p1, aTriangle.textureCoords.p1);
  UpdatePoint(aTriangle.p2, aTriangle.textureCoords.p2);
  UpdatePoint(aTriangle.p3, aTriangle.textureCoords.p3);
}

void Compositor::DrawGeometry(const gfx::Rect& aRect,
                              const gfx::IntRect& aClipRect,
                              const EffectChain& aEffectChain,
                              gfx::Float aOpacity,
                              const gfx::Matrix4x4& aTransform,
                              const gfx::Rect& aVisibleRect,
                              const Maybe<gfx::Polygon>& aGeometry) {
  if (aRect.IsEmpty()) {
    return;
  }

  if (!aGeometry || !SupportsLayerGeometry()) {
    DrawQuad(aRect, aClipRect, aEffectChain, aOpacity, aTransform,
             aVisibleRect);
    return;
  }

  // Cull completely invisible polygons.
  if (aRect.Intersect(aGeometry->BoundingBox()).IsEmpty()) {
    return;
  }

  const gfx::Polygon clipped = aGeometry->ClipPolygon(aRect);

  // Cull polygons with no area.
  if (clipped.IsEmpty()) {
    return;
  }

  DrawPolygon(clipped, aRect, aClipRect, aEffectChain, aOpacity, aTransform,
              aVisibleRect);
}

void Compositor::DrawTriangles(
    const nsTArray<gfx::TexturedTriangle>& aTriangles, const gfx::Rect& aRect,
    const gfx::IntRect& aClipRect, const EffectChain& aEffectChain,
    gfx::Float aOpacity, const gfx::Matrix4x4& aTransform,
    const gfx::Rect& aVisibleRect) {
  for (const gfx::TexturedTriangle& triangle : aTriangles) {
    DrawTriangle(triangle, aClipRect, aEffectChain, aOpacity, aTransform,
                 aVisibleRect);
  }
}

nsTArray<gfx::TexturedTriangle> GenerateTexturedTriangles(
    const gfx::Polygon& aPolygon, const gfx::Rect& aRect,
    const gfx::Rect& aTexRect) {
  nsTArray<gfx::TexturedTriangle> texturedTriangles;

  gfx::Rect layerRects[4];
  gfx::Rect textureRects[4];
  size_t rects =
      DecomposeIntoNoRepeatRects(aRect, aTexRect, &layerRects, &textureRects);
  for (size_t i = 0; i < rects; ++i) {
    const gfx::Rect& rect = layerRects[i];
    const gfx::Rect& texRect = textureRects[i];
    const gfx::Polygon clipped = aPolygon.ClipPolygon(rect);

    if (clipped.IsEmpty()) {
      continue;
    }

    for (const gfx::Triangle& triangle : clipped.ToTriangles()) {
      const gfx::Rect intersection = rect.Intersect(triangle.BoundingBox());

      // Cull completely invisible triangles.
      if (intersection.IsEmpty()) {
        continue;
      }

      MOZ_ASSERT(rect.Width() > 0.0f && rect.Height() > 0.0f);
      MOZ_ASSERT(intersection.Width() > 0.0f && intersection.Height() > 0.0f);

      // Since the texture was created for non-split geometry, we need to
      // update the texture coordinates to account for the split.
      gfx::TexturedTriangle t(triangle);
      UpdateTextureCoordinates(t, rect, intersection, texRect);
      texturedTriangles.AppendElement(std::move(t));
    }
  }

  return texturedTriangles;
}

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

void Compositor::DrawPolygon(const gfx::Polygon& aPolygon,
                             const gfx::Rect& aRect,
                             const gfx::IntRect& aClipRect,
                             const EffectChain& aEffectChain,
                             gfx::Float aOpacity,
                             const gfx::Matrix4x4& aTransform,
                             const gfx::Rect& aVisibleRect) {
  nsTArray<gfx::TexturedTriangle> texturedTriangles;

  TexturedEffect* texturedEffect =
      aEffectChain.mPrimaryEffect->AsTexturedEffect();

  if (texturedEffect) {
    texturedTriangles = GenerateTexturedTriangles(
        aPolygon, aRect, texturedEffect->mTextureCoords);
  } else {
    for (const gfx::Triangle& triangle : aPolygon.ToTriangles()) {
      texturedTriangles.AppendElement(gfx::TexturedTriangle(triangle));
    }
  }

  if (texturedTriangles.IsEmpty()) {
    // Nothing to render.
    return;
  }

  DrawTriangles(texturedTriangles, aRect, aClipRect, aEffectChain, aOpacity,
                aTransform, aVisibleRect);
}

void Compositor::SlowDrawRect(const gfx::Rect& aRect, const gfx::Color& aColor,
                              const gfx::IntRect& aClipRect,
                              const gfx::Matrix4x4& aTransform,
                              int aStrokeWidth) {
  // TODO This should draw a rect using a single draw call but since
  // this is only used for debugging overlays it's not worth optimizing ATM.
  float opacity = 1.0f;
  EffectChain effects;

  effects.mPrimaryEffect = new EffectSolidColor(aColor);
  // left
  this->DrawQuad(gfx::Rect(aRect.X(), aRect.Y(), aStrokeWidth, aRect.Height()),
                 aClipRect, effects, opacity, aTransform);
  // top
  this->DrawQuad(gfx::Rect(aRect.X() + aStrokeWidth, aRect.Y(),
                           aRect.Width() - 2 * aStrokeWidth, aStrokeWidth),
                 aClipRect, effects, opacity, aTransform);
  // right
  this->DrawQuad(gfx::Rect(aRect.XMost() - aStrokeWidth, aRect.Y(),
                           aStrokeWidth, aRect.Height()),
                 aClipRect, effects, opacity, aTransform);
  // bottom
  this->DrawQuad(
      gfx::Rect(aRect.X() + aStrokeWidth, aRect.YMost() - aStrokeWidth,
                aRect.Width() - 2 * aStrokeWidth, aStrokeWidth),
      aClipRect, effects, opacity, aTransform);
}

void Compositor::FillRect(const gfx::Rect& aRect, const gfx::Color& aColor,
                          const gfx::IntRect& aClipRect,
                          const gfx::Matrix4x4& aTransform) {
  float opacity = 1.0f;
  EffectChain effects;

  effects.mPrimaryEffect = new EffectSolidColor(aColor);
  this->DrawQuad(aRect, aClipRect, effects, opacity, aTransform);
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
  br = gfx::Point(xwrap ? WrapTexCoord(br.x) : br.x,
                  ywrap ? WrapTexCoord(br.y) : br.y);

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

gfx::IntRect Compositor::ComputeBackdropCopyRect(
    const gfx::Rect& aRect, const gfx::IntRect& aClipRect,
    const gfx::Matrix4x4& aTransform, gfx::Matrix4x4* aOutTransform,
    gfx::Rect* aOutLayerQuad) {
  // Compute the clip.
  RefPtr<CompositingRenderTarget> currentRenderTarget =
      GetCurrentRenderTarget();
  gfx::IntPoint rtOffset = currentRenderTarget->GetOrigin();
  gfx::IntSize rtSize = currentRenderTarget->GetSize();

  return layers::ComputeBackdropCopyRect(aRect, aClipRect, aTransform,
                                         gfx::IntRect(rtOffset, rtSize),
                                         aOutTransform, aOutLayerQuad);
}

gfx::IntRect Compositor::ComputeBackdropCopyRect(
    const gfx::Triangle& aTriangle, const gfx::IntRect& aClipRect,
    const gfx::Matrix4x4& aTransform, gfx::Matrix4x4* aOutTransform,
    gfx::Rect* aOutLayerQuad) {
  gfx::Rect boundingBox = aTriangle.BoundingBox();
  return ComputeBackdropCopyRect(boundingBox, aClipRect, aTransform,
                                 aOutTransform, aOutLayerQuad);
}

void Compositor::SetInvalid() { mParent = nullptr; }

bool Compositor::IsValid() const { return !!mParent; }

void Compositor::UnlockAfterComposition(TextureHost* aTexture) {
  TextureSourceProvider::UnlockAfterComposition(aTexture);

  // If this is being called after we shutdown the compositor, we must finish
  // read unlocking now to prevent a cycle.
  if (IsDestroyed()) {
    ReadUnlockTextures();
  }
}

bool Compositor::NotifyNotUsedAfterComposition(TextureHost* aTextureHost) {
  if (IsDestroyed() || AsBasicCompositor()) {
    return false;
  }
  return TextureSourceProvider::NotifyNotUsedAfterComposition(aTextureHost);
}

void Compositor::GetFrameStats(GPUStats* aStats) {
  aStats->mInvalidPixels = mPixelsPerFrame;
  aStats->mPixelsFilled = mPixelsFilled;
}

already_AddRefed<RecordedFrame> Compositor::RecordFrame(
    const TimeStamp& aTimeStamp) {
  RefPtr<CompositingRenderTarget> renderTarget = GetWindowRenderTarget();
  if (!renderTarget) {
    return nullptr;
  }

  RefPtr<AsyncReadbackBuffer> buffer =
      CreateAsyncReadbackBuffer(renderTarget->GetSize());

  if (!ReadbackRenderTarget(renderTarget, buffer)) {
    return nullptr;
  }

  return MakeAndAddRef<CompositorRecordedFrame>(aTimeStamp, std::move(buffer));
}

bool Compositor::ShouldRecordFrames() const {
#ifdef MOZ_GECKO_PROFILER
  if (profiler_feature_active(ProfilerFeature::Screenshots)) {
    return true;
  }
#endif
  return mRecordFrames;
}

}  // namespace layers
}  // namespace mozilla
