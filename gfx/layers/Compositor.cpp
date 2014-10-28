/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/Compositor.h"
#include "base/message_loop.h"          // for MessageLoop
#include "mozilla/layers/CompositorParent.h"  // for CompositorParent
#include "mozilla/layers/Effects.h"     // for Effect, EffectChain, etc
#include "mozilla/mozalloc.h"           // for operator delete, etc
#include "gfx2DGlue.h"

namespace mozilla {
namespace gfx {
class Matrix4x4;
}

namespace layers {

/* static */ LayersBackend Compositor::sBackend = LayersBackend::LAYERS_NONE;
/* static */ LayersBackend
Compositor::GetBackend()
{
  AssertOnCompositorThread();
  return sBackend;
}

/* static */ void
Compositor::SetBackend(LayersBackend backend)
{
  if (sBackend != LayersBackend::LAYERS_NONE && sBackend != backend) {
    // Assert this once we figure out bug 972891.
    //MOZ_CRASH("Trying to use more than one OMTC compositor.");

#ifdef XP_MACOSX
    printf("ERROR: Changing compositor from %u to %u.\n",
           unsigned(sBackend), unsigned(backend));
#endif
  }
  sBackend = backend;
}

/* static */ void
Compositor::AssertOnCompositorThread()
{
  MOZ_ASSERT(CompositorParent::CompositorLoop() ==
             MessageLoop::current(),
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
                            const gfx::Rect& aClipRect,
                            const gfx::Matrix4x4& aTransform,
                            uint32_t aFlashCounter)
{
  if (!ShouldDrawDiagnostics(aFlags)) {
    return;
  }

  if (aVisibleRegion.GetNumRects() > 1) {
    nsIntRegionRectIterator screenIter(aVisibleRegion);

    while (const nsIntRect* rect = screenIter.Next())
    {
      DrawDiagnostics(aFlags | DiagnosticFlags::REGION_RECT,
                      ToRect(*rect), aClipRect, aTransform, aFlashCounter);
    }
  }

  DrawDiagnostics(aFlags, ToRect(aVisibleRegion.GetBounds()),
                  aClipRect, aTransform, aFlashCounter);
}

void
Compositor::DrawDiagnostics(DiagnosticFlags aFlags,
                            const gfx::Rect& aVisibleRect,
                            const gfx::Rect& aClipRect,
                            const gfx::Matrix4x4& aTransform,
                            uint32_t aFlashCounter)
{
  if (!ShouldDrawDiagnostics(aFlags)) {
    return;
  }

  DrawDiagnosticsInternal(aFlags, aVisibleRect, aClipRect, aTransform,
                          aFlashCounter);
}

RenderTargetRect
Compositor::ClipRectInLayersCoordinates(Layer* aLayer, RenderTargetIntRect aClip) const {
  ContainerLayer* parent = aLayer->AsContainerLayer() ? aLayer->AsContainerLayer() : aLayer->GetParent();
  while (!parent->UseIntermediateSurface() && parent->GetParent()) {
    parent = parent->GetParent();
  }

  RenderTargetIntPoint renderTargetOffset = RenderTargetIntRect::FromUntyped(
    parent->GetEffectiveVisibleRegion().GetBounds()).TopLeft();

  RenderTargetRect result;
  aClip = aClip + renderTargetOffset;
  result = RenderTargetRect(aClip.x, aClip.y, aClip.width, aClip.height);
  return result;
}

void
Compositor::DrawDiagnosticsInternal(DiagnosticFlags aFlags,
                                    const gfx::Rect& aVisibleRect,
                                    const gfx::Rect& aClipRect,
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
  float opacity = 0.7f;

  gfx::Color color;
  if (aFlags & DiagnosticFlags::CONTENT) {
    color = gfx::Color(0.0f, 1.0f, 0.0f, 1.0f); // green
    if (aFlags & DiagnosticFlags::COMPONENT_ALPHA) {
      color = gfx::Color(0.0f, 1.0f, 1.0f, 1.0f); // greenish blue
    }
  } else if (aFlags & DiagnosticFlags::IMAGE) {
    color = gfx::Color(1.0f, 0.0f, 0.0f, 1.0f); // red
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
    opacity = 0.5f;
    color.r *= 0.7f;
    color.g *= 0.7f;
    color.b *= 0.7f;
  }

  if (mDiagnosticTypes & DiagnosticTypes::FLASH_BORDERS) {
    float flash = (float)aFlashCounter / (float)DIAGNOSTIC_FLASH_COUNTER_MAX;
    color.r *= flash;
    color.g *= flash;
    color.b *= flash;
  }

  EffectChain effects;

  effects.mPrimaryEffect = new EffectSolidColor(color);
  // left
  this->DrawQuad(gfx::Rect(aVisibleRect.x, aVisibleRect.y,
                           lWidth, aVisibleRect.height),
                 aClipRect, effects, opacity,
                 aTransform);
  // top
  this->DrawQuad(gfx::Rect(aVisibleRect.x + lWidth, aVisibleRect.y,
                           aVisibleRect.width - 2 * lWidth, lWidth),
                 aClipRect, effects, opacity,
                 aTransform);
  // right
  this->DrawQuad(gfx::Rect(aVisibleRect.x + aVisibleRect.width - lWidth, aVisibleRect.y,
                           lWidth, aVisibleRect.height),
                 aClipRect, effects, opacity,
                 aTransform);
  // bottom
  this->DrawQuad(gfx::Rect(aVisibleRect.x + lWidth, aVisibleRect.y + aVisibleRect.height-lWidth,
                           aVisibleRect.width - 2 * lWidth, lWidth),
                 aClipRect, effects, opacity,
                 aTransform);
}

static float
WrapTexCoord(float v)
{
    // fmodf gives negative results for negative numbers;
    // that is, fmodf(0.75, 1.0) == 0.75, but
    // fmodf(-0.75, 1.0) == -0.75.  For the negative case,
    // the result we need is 0.25, so we add 1.0f.
    if (v < 0.0f) {
        return 1.0f + fmodf(v, 1.0f);
    }

    return fmodf(v, 1.0f);
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

  NS_ASSERTION(!xwrap ||
               (xmid > aRect.x &&
                xmid < aRect.XMost() &&
                FuzzyEqual((xmid - aRect.x) + (aRect.XMost() - xmid), aRect.width)),
               "xmid should be within [x,XMost()] and the wrapped rect should have the same width");
  NS_ASSERTION(!ywrap ||
               (ymid > aRect.y &&
                ymid < aRect.YMost() &&
                FuzzyEqual((ymid - aRect.y) + (aRect.YMost() - ymid), aRect.height)),
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

} // namespace layers
} // namespace mozilla
