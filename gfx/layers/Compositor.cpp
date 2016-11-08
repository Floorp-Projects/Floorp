/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/Compositor.h"
#include "base/message_loop.h"          // for MessageLoop
#include "mozilla/layers/CompositorBridgeParent.h"  // for CompositorBridgeParent
#include "mozilla/layers/Effects.h"     // for Effect, EffectChain, etc
#include "mozilla/layers/TextureClient.h"
#include "mozilla/layers/TextureHost.h"
#include "mozilla/layers/CompositorThread.h"
#include "mozilla/mozalloc.h"           // for operator delete, etc
#include "gfx2DGlue.h"
#include "nsAppRunner.h"

namespace mozilla {

namespace layers {

Compositor::Compositor(widget::CompositorWidget* aWidget,
                      CompositorBridgeParent* aParent)
  : mCompositorID(0)
  , mDiagnosticTypes(DiagnosticTypes::NO_DIAGNOSTIC)
  , mParent(aParent)
  , mPixelsPerFrame(0)
  , mPixelsFilled(0)
  , mScreenRotation(ROTATION_0)
  , mWidget(aWidget)
  , mIsDestroyed(false)
#if defined(MOZ_WIDGET_ANDROID)
  // If the default color isn't white for Fennec, there is a black
  // flash before the first page of a tab is loaded.
  , mClearColor(1.0, 1.0, 1.0, 1.0)
  , mDefaultClearColor(1.0, 1.0, 1.0, 1.0)
#else
  , mClearColor(0.0, 0.0, 0.0, 0.0)
  , mDefaultClearColor(0.0, 0.0, 0.0, 0.0)
#endif
{
}

Compositor::~Compositor()
{
  ReadUnlockTextures();
}

void
Compositor::Destroy()
{
  ReadUnlockTextures();
  FlushPendingNotifyNotUsed();
  mIsDestroyed = true;
}

void
Compositor::EndFrame()
{
  ReadUnlockTextures();
  mLastCompositionEndTime = TimeStamp::Now();
}

void
Compositor::ReadUnlockTextures()
{
  for (auto& texture : mUnlockAfterComposition) {
    texture->ReadUnlock();
  }
  mUnlockAfterComposition.Clear();
}

void
Compositor::UnlockAfterComposition(TextureHost* aTexture)
{
  mUnlockAfterComposition.AppendElement(aTexture);
}

void
Compositor::NotifyNotUsedAfterComposition(TextureHost* aTextureHost)
{
  MOZ_ASSERT(!mIsDestroyed);

  mNotifyNotUsedAfterComposition.AppendElement(aTextureHost);

  // If Compositor holds many TextureHosts without compositing,
  // the TextureHosts should be flushed to reduce memory consumption.
  const int thresholdCount = 5;
  const double thresholdSec = 2.0f;
  if (mNotifyNotUsedAfterComposition.Length() > thresholdCount) {
    TimeDuration duration = mLastCompositionEndTime ? TimeStamp::Now() - mLastCompositionEndTime : TimeDuration();
    // Check if we could flush
    if (duration.ToSeconds() > thresholdSec) {
      FlushPendingNotifyNotUsed();
    }
  }
}

void
Compositor::FlushPendingNotifyNotUsed()
{
  for (auto& textureHost : mNotifyNotUsedAfterComposition) {
    textureHost->CallNotifyNotUsed();
  }
  mNotifyNotUsedAfterComposition.Clear();
}

/* static */ void
Compositor::AssertOnCompositorThread()
{
  MOZ_ASSERT(!CompositorThreadHolder::Loop() ||
             CompositorThreadHolder::Loop() == MessageLoop::current(),
             "Can only call this from the compositor thread!");
}

bool
Compositor::ShouldDrawDiagnostics(DiagnosticFlags aFlags)
{
  if ((aFlags & DiagnosticFlags::TILE) && !(mDiagnosticTypes & DiagnosticTypes::TILE_BORDERS)) {
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

void
Compositor::DrawDiagnostics(DiagnosticFlags aFlags,
                            const nsIntRegion& aVisibleRegion,
                            const gfx::IntRect& aClipRect,
                            const gfx::Matrix4x4& aTransform,
                            uint32_t aFlashCounter)
{
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

  DrawDiagnostics(aFlags, IntRectToRect(aVisibleRegion.GetBounds()),
                  aClipRect, aTransform, aFlashCounter);
}

void
Compositor::DrawDiagnostics(DiagnosticFlags aFlags,
                            const gfx::Rect& aVisibleRect,
                            const gfx::IntRect& aClipRect,
                            const gfx::Matrix4x4& aTransform,
                            uint32_t aFlashCounter)
{
  if (!ShouldDrawDiagnostics(aFlags)) {
    return;
  }

  DrawDiagnosticsInternal(aFlags, aVisibleRect, aClipRect, aTransform,
                          aFlashCounter);
}

void
Compositor::DrawDiagnosticsInternal(DiagnosticFlags aFlags,
                                    const gfx::Rect& aVisibleRect,
                                    const gfx::IntRect& aClipRect,
                                    const gfx::Matrix4x4& aTransform,
                                    uint32_t aFlashCounter)
{
#ifdef MOZ_B2G
  int lWidth = 4;
#elif defined(ANDROID)
  int lWidth = 10;
#else
  int lWidth = 2;
#endif

  gfx::Color color;
  if (aFlags & DiagnosticFlags::CONTENT) {
    color = gfx::Color(0.0f, 1.0f, 0.0f, 1.0f); // green
    if (aFlags & DiagnosticFlags::COMPONENT_ALPHA) {
      color = gfx::Color(0.0f, 1.0f, 1.0f, 1.0f); // greenish blue
    }
  } else if (aFlags & DiagnosticFlags::IMAGE) {
    if (aFlags & DiagnosticFlags::NV12) {
      color = gfx::Color(1.0f, 1.0f, 0.0f, 1.0f); // yellow
    } else if (aFlags & DiagnosticFlags::YCBCR) {
      color = gfx::Color(1.0f, 0.55f, 0.0f, 1.0f); // orange
    } else {
      color = gfx::Color(1.0f, 0.0f, 0.0f, 1.0f); // red
    }
  } else if (aFlags & DiagnosticFlags::COLOR) {
    color = gfx::Color(0.0f, 0.0f, 1.0f, 1.0f); // blue
  } else if (aFlags & DiagnosticFlags::CONTAINER) {
    color = gfx::Color(0.8f, 0.0f, 0.8f, 1.0f); // purple
  }

  // make tile borders a bit more transparent to keep layer borders readable.
  if (aFlags & DiagnosticFlags::TILE ||
      aFlags & DiagnosticFlags::BIGIMAGE ||
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

static void
UpdateTextureCoordinates(gfx::TexturedTriangle& aTriangle,
                         const gfx::Rect& aRect,
                         const gfx::Rect& aIntersection,
                         gfx::Rect aTextureCoords)
{
  // Calculate the relative offset of the intersection within the layer.
  float dx = (aIntersection.x - aRect.x) / aRect.width;
  float dy = (aIntersection.y - aRect.y) / aRect.height;

  // Update the texture offset.
  float x = aTextureCoords.x + dx * aTextureCoords.width;
  float y = aTextureCoords.y + dy * aTextureCoords.height;

  // Scale the texture width and height.
  float w = aTextureCoords.width * aIntersection.width / aRect.width;
  float h = aTextureCoords.height * aIntersection.height / aRect.height;

  static const auto ValidateAndClamp = [](float& f) {
    // Allow some numerical inaccuracy.
    MOZ_ASSERT(f >= -0.0001f && f <= 1.0001f);

    if (f >= 1.0f) f = 1.0f;
    if (f <= 0.0f) f = 0.0f;
  };

  auto UpdatePoint = [&](const gfx::Point& p, gfx::Point& t)
  {
    t.x = x + (p.x - aIntersection.x) / aIntersection.width * w;
    t.y = y + (p.y - aIntersection.y) / aIntersection.height * h;

    ValidateAndClamp(t.x);
    ValidateAndClamp(t.y);
  };

  UpdatePoint(aTriangle.p1, aTriangle.textureCoords.p1);
  UpdatePoint(aTriangle.p2, aTriangle.textureCoords.p2);
  UpdatePoint(aTriangle.p3, aTriangle.textureCoords.p3);
}

void
Compositor::DrawGeometry(const gfx::Rect& aRect,
                         const gfx::IntRect& aClipRect,
                         const EffectChain& aEffectChain,
                         gfx::Float aOpacity,
                         const gfx::Matrix4x4& aTransform,
                         const gfx::Rect& aVisibleRect,
                         const Maybe<gfx::Polygon3D>& aGeometry)
{
  if (!aGeometry) {
    DrawQuad(aRect, aClipRect, aEffectChain,
             aOpacity, aTransform, aVisibleRect);
    return;
  }

  // Cull invisible polygons.
  if (aRect.Intersect(aGeometry->BoundingBox()).IsEmpty()) {
    return;
  }

  gfx::Polygon3D clipped = aGeometry->ClipPolygon(aRect);
  nsTArray<gfx::Triangle> triangles = clipped.ToTriangles();

  for (gfx::Triangle& geometry : triangles) {
    const gfx::Rect intersection = aRect.Intersect(geometry.BoundingBox());

    // Cull invisible triangles.
    if (intersection.IsEmpty()) {
      continue;
    }

    MOZ_ASSERT(aRect.width > 0.0f && aRect.height > 0.0f);
    MOZ_ASSERT(intersection.width > 0.0f && intersection.height > 0.0f);

    gfx::TexturedTriangle triangle(Move(geometry));
    triangle.width = aRect.width;
    triangle.height = aRect.height;

    // Since the texture was created for non-split geometry, we need to
    // update the texture coordinates to account for the split.
    if (aEffectChain.mPrimaryEffect->mType == EffectTypes::RGB) {
      TexturedEffect* texturedEffect =
        static_cast<TexturedEffect*>(aEffectChain.mPrimaryEffect.get());

      UpdateTextureCoordinates(triangle, aRect, intersection,
                               texturedEffect->mTextureCoords);
    }

    DrawTriangle(triangle, aClipRect, aEffectChain,
                 aOpacity, aTransform, aVisibleRect);
  }
}

void
Compositor::SlowDrawRect(const gfx::Rect& aRect, const gfx::Color& aColor,
                     const gfx::IntRect& aClipRect,
                     const gfx::Matrix4x4& aTransform, int aStrokeWidth)
{
  // TODO This should draw a rect using a single draw call but since
  // this is only used for debugging overlays it's not worth optimizing ATM.
  float opacity = 1.0f;
  EffectChain effects;

  effects.mPrimaryEffect = new EffectSolidColor(aColor);
  // left
  this->DrawQuad(gfx::Rect(aRect.x, aRect.y,
                           aStrokeWidth, aRect.height),
                 aClipRect, effects, opacity,
                 aTransform);
  // top
  this->DrawQuad(gfx::Rect(aRect.x + aStrokeWidth, aRect.y,
                           aRect.width - 2 * aStrokeWidth, aStrokeWidth),
                 aClipRect, effects, opacity,
                 aTransform);
  // right
  this->DrawQuad(gfx::Rect(aRect.x + aRect.width - aStrokeWidth, aRect.y,
                           aStrokeWidth, aRect.height),
                 aClipRect, effects, opacity,
                 aTransform);
  // bottom
  this->DrawQuad(gfx::Rect(aRect.x + aStrokeWidth, aRect.y + aRect.height - aStrokeWidth,
                           aRect.width - 2 * aStrokeWidth, aStrokeWidth),
                 aClipRect, effects, opacity,
                 aTransform);
}

void
Compositor::FillRect(const gfx::Rect& aRect, const gfx::Color& aColor,
                     const gfx::IntRect& aClipRect,
                     const gfx::Matrix4x4& aTransform)
{
  float opacity = 1.0f;
  EffectChain effects;

  effects.mPrimaryEffect = new EffectSolidColor(aColor);
  this->DrawQuad(aRect,
                 aClipRect, effects, opacity,
                 aTransform);
}


static float
WrapTexCoord(float v)
{
    // This should return values in range [0, 1.0)
    return v - floorf(v);
}

static void
SetRects(size_t n,
         decomposedRectArrayT* aLayerRects,
         decomposedRectArrayT* aTextureRects,
         float x0, float y0, float x1, float y1,
         float tx0, float ty0, float tx1, float ty1,
         bool flip_y)
{
  if (flip_y) {
    std::swap(ty0, ty1);
  }
  (*aLayerRects)[n] = gfx::Rect(x0, y0, x1 - x0, y1 - y0);
  (*aTextureRects)[n] = gfx::Rect(tx0, ty0, tx1 - tx0, ty1 - ty0);
}

#ifdef DEBUG
static inline bool
FuzzyEqual(float a, float b)
{
	return fabs(a - b) < 0.0001f;
}
static inline bool
FuzzyLTE(float a, float b)
{
	return a <= b + 0.0001f;
}
#endif

size_t
DecomposeIntoNoRepeatRects(const gfx::Rect& aRect,
                           const gfx::Rect& aTexCoordRect,
                           decomposedRectArrayT* aLayerRects,
                           decomposedRectArrayT* aTextureRects)
{
  gfx::Rect texCoordRect = aTexCoordRect;

  // If the texture should be flipped, it will have negative height. Detect that
  // here and compensate for it. We will flip each rect as we emit it.
  bool flipped = false;
  if (texCoordRect.height < 0) {
    flipped = true;
    texCoordRect.y += texCoordRect.height;
    texCoordRect.height = -texCoordRect.height;
  }

  // Wrap the texture coordinates so they are within [0,1] and cap width/height
  // at 1. We rely on this below.
  texCoordRect = gfx::Rect(gfx::Point(WrapTexCoord(texCoordRect.x),
                                      WrapTexCoord(texCoordRect.y)),
                           gfx::Size(std::min(texCoordRect.width, 1.0f),
                                     std::min(texCoordRect.height, 1.0f)));

  NS_ASSERTION(texCoordRect.x >= 0.0f && texCoordRect.x <= 1.0f &&
               texCoordRect.y >= 0.0f && texCoordRect.y <= 1.0f &&
               texCoordRect.width >= 0.0f && texCoordRect.width <= 1.0f &&
               texCoordRect.height >= 0.0f && texCoordRect.height <= 1.0f &&
               texCoordRect.XMost() >= 0.0f && texCoordRect.XMost() <= 2.0f &&
               texCoordRect.YMost() >= 0.0f && texCoordRect.YMost() <= 2.0f,
               "We just wrapped the texture coordinates, didn't we?");

  // Get the top left and bottom right points of the rectangle. Note that
  // tl.x/tl.y are within [0,1] but br.x/br.y are within [0,2].
  gfx::Point tl = texCoordRect.TopLeft();
  gfx::Point br = texCoordRect.BottomRight();

  NS_ASSERTION(tl.x >= 0.0f && tl.x <= 1.0f &&
               tl.y >= 0.0f && tl.y <= 1.0f &&
               br.x >= tl.x && br.x <= 2.0f &&
               br.y >= tl.y && br.y <= 2.0f &&
               FuzzyLTE(br.x - tl.x, 1.0f) &&
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
    SetRects(0, aLayerRects, aTextureRects,
             aRect.x, aRect.y, aRect.XMost(), aRect.YMost(),
             tl.x, tl.y, br.x, br.y,
             flipped);
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
  GLfloat xmid = aRect.x + (1.0f - tl.x) / texCoordRect.width * aRect.width;
  GLfloat ymid = aRect.y + (1.0f - tl.y) / texCoordRect.height * aRect.height;

  // Due to floating-point inaccuracy, we have to use XMost()-x and YMost()-y
  // to calculate width and height, respectively, to ensure that size will
  // remain consistent going from absolute to relative and back again.
  NS_ASSERTION(!xwrap ||
               (xmid >= aRect.x &&
                xmid <= aRect.XMost() &&
                FuzzyEqual((xmid - aRect.x) + (aRect.XMost() - xmid), aRect.XMost() - aRect.x)),
               "xmid should be within [x,XMost()] and the wrapped rect should have the same width");
  NS_ASSERTION(!ywrap ||
               (ymid >= aRect.y &&
                ymid <= aRect.YMost() &&
                FuzzyEqual((ymid - aRect.y) + (aRect.YMost() - ymid), aRect.YMost() - aRect.y)),
               "ymid should be within [y,YMost()] and the wrapped rect should have the same height");

  if (!xwrap && ywrap) {
    SetRects(0, aLayerRects, aTextureRects,
             aRect.x, aRect.y, aRect.XMost(), ymid,
             tl.x, tl.y, br.x, 1.0f,
             flipped);
    SetRects(1, aLayerRects, aTextureRects,
             aRect.x, ymid, aRect.XMost(), aRect.YMost(),
             tl.x, 0.0f, br.x, br.y,
             flipped);
    return 2;
  }

  if (xwrap && !ywrap) {
    SetRects(0, aLayerRects, aTextureRects,
             aRect.x, aRect.y, xmid, aRect.YMost(),
             tl.x, tl.y, 1.0f, br.y,
             flipped);
    SetRects(1, aLayerRects, aTextureRects,
             xmid, aRect.y, aRect.XMost(), aRect.YMost(),
             0.0f, tl.y, br.x, br.y,
             flipped);
    return 2;
  }

  SetRects(0, aLayerRects, aTextureRects,
           aRect.x, aRect.y, xmid, ymid,
           tl.x, tl.y, 1.0f, 1.0f,
           flipped);
  SetRects(1, aLayerRects, aTextureRects,
           xmid, aRect.y, aRect.XMost(), ymid,
           0.0f, tl.y, br.x, 1.0f,
           flipped);
  SetRects(2, aLayerRects, aTextureRects,
           aRect.x, ymid, xmid, aRect.YMost(),
           tl.x, 0.0f, 1.0f, br.y,
           flipped);
  SetRects(3, aLayerRects, aTextureRects,
           xmid, ymid, aRect.XMost(), aRect.YMost(),
           0.0f, 0.0f, br.x, br.y,
           flipped);
  return 4;
}

gfx::IntRect
Compositor::ComputeBackdropCopyRect(const gfx::Rect& aRect,
                                    const gfx::IntRect& aClipRect,
                                    const gfx::Matrix4x4& aTransform,
                                    gfx::Matrix4x4* aOutTransform,
                                    gfx::Rect* aOutLayerQuad)
{
  // Compute the clip.
  gfx::IntPoint rtOffset = GetCurrentRenderTarget()->GetOrigin();
  gfx::IntSize rtSize = GetCurrentRenderTarget()->GetSize();

  gfx::IntRect renderBounds(0, 0, rtSize.width, rtSize.height);
  renderBounds.IntersectRect(renderBounds, aClipRect);
  renderBounds.MoveBy(rtOffset);

  // Apply the layer transform.
  gfx::RectDouble dest = aTransform.TransformAndClipBounds(
    gfx::RectDouble(aRect.x, aRect.y, aRect.width, aRect.height),
    gfx::RectDouble(renderBounds.x, renderBounds.y, renderBounds.width, renderBounds.height));
  dest -= rtOffset;

  // Ensure we don't round out to -1, which trips up Direct3D.
  dest.IntersectRect(dest, gfx::RectDouble(0, 0, rtSize.width, rtSize.height));

  if (aOutLayerQuad) {
    *aOutLayerQuad = gfx::Rect(dest.x, dest.y, dest.width, dest.height);
  }

  // Round out to integer.
  gfx::IntRect result;
  dest.RoundOut();
  dest.ToIntRect(&result);

  // Create a transform from adjusted clip space to render target space,
  // translate it for the backdrop rect, then transform it into the backdrop's
  // uv-space.
  gfx::Matrix4x4 transform;
  transform.PostScale(rtSize.width, rtSize.height, 1.0);
  transform.PostTranslate(-result.x, -result.y, 0.0);
  transform.PostScale(1 / float(result.width), 1 / float(result.height), 1.0);
  *aOutTransform = transform;
  return result;
}

gfx::IntRect
Compositor::ComputeBackdropCopyRect(const gfx::Triangle& aTriangle,
                                    const gfx::IntRect& aClipRect,
                                    const gfx::Matrix4x4& aTransform,
                                    gfx::Matrix4x4* aOutTransform,
                                    gfx::Rect* aOutLayerQuad)
{
  gfx::Rect boundingBox = aTriangle.BoundingBox();
  return ComputeBackdropCopyRect(boundingBox, aClipRect, aTransform,
                                 aOutTransform, aOutLayerQuad);
}

void
Compositor::SetInvalid()
{
  mParent = nullptr;
}

bool
Compositor::IsValid() const
{
  return !!mParent;
}

void
Compositor::SetDispAcquireFence(Layer* aLayer)
{
}

} // namespace layers
} // namespace mozilla
