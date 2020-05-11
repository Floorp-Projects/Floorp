/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxFontMissingGlyphs.h"

#include "gfxUtils.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/Helpers.h"
#include "mozilla/gfx/PathHelpers.h"
#include "mozilla/LinkedList.h"
#include "mozilla/RefPtr.h"
#include "nsDeviceContext.h"
#include "nsLayoutUtils.h"
#include "TextDrawTarget.h"

using namespace mozilla;
using namespace mozilla::gfx;

#define X 255
static const uint8_t gMiniFontData[] = {
    0, X, 0, 0, X, 0, X, X, X, X, X, X, X, 0, X, X, X, X, X, X, X, X, X, X,
    X, X, X, X, X, X, X, X, X, X, X, 0, 0, X, X, X, X, 0, X, X, X, X, X, X,
    X, 0, X, 0, X, 0, 0, 0, X, 0, 0, X, X, 0, X, X, 0, 0, X, 0, 0, 0, 0, X,
    X, 0, X, X, 0, X, X, 0, X, X, 0, X, X, 0, 0, X, 0, X, X, 0, 0, X, 0, 0,
    X, 0, X, 0, X, 0, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, 0, 0, X,
    X, X, X, X, X, X, X, X, X, X, X, 0, X, 0, 0, X, 0, X, X, X, X, X, X, X,
    X, 0, X, 0, X, 0, X, 0, 0, 0, 0, X, 0, 0, X, 0, 0, X, X, 0, X, 0, 0, X,
    X, 0, X, 0, 0, X, X, 0, X, X, 0, X, X, 0, 0, X, 0, X, X, 0, 0, X, 0, 0,
    0, X, 0, 0, X, 0, X, X, X, X, X, X, 0, 0, X, X, X, X, X, X, X, 0, 0, X,
    X, X, X, 0, 0, X, X, 0, X, X, X, 0, 0, X, X, X, X, 0, X, X, X, X, 0, 0,
};
#undef X

/* Parameters that control the rendering of hexboxes. They look like this:

        BMP codepoints           non-BMP codepoints
      (U+0000 - U+FFFF)         (U+10000 - U+10FFFF)

         +---------+              +-------------+
         |         |              |             |
         | HHH HHH |              | HHH HHH HHH |
         | HHH HHH |              | HHH HHH HHH |
         | HHH HHH |              | HHH HHH HHH |
         | HHH HHH |              | HHH HHH HHH |
         | HHH HHH |              | HHH HHH HHH |
         |         |              |             |
         | HHH HHH |              | HHH HHH HHH |
         | HHH HHH |              | HHH HHH HHH |
         | HHH HHH |              | HHH HHH HHH |
         | HHH HHH |              | HHH HHH HHH |
         | HHH HHH |              | HHH HHH HHH |
         |         |              |             |
         +---------+              +-------------+
*/

/** Width of a minifont glyph (see above) */
static const int MINIFONT_WIDTH = 3;
/** Height of a minifont glyph (see above) */
static const int MINIFONT_HEIGHT = 5;
/**
 * Gap between minifont glyphs (both horizontal and vertical) and also
 * the minimum desired gap between the box border and the glyphs
 */
static const int HEX_CHAR_GAP = 1;
/**
 * The amount of space between the vertical edge of the glyphbox and the
 * box border. We make this nonzero so that when multiple missing glyphs
 * occur consecutively there's a gap between their rendered boxes.
 */
static const int BOX_HORIZONTAL_INSET = 1;
/** The width of the border */
static const int BOX_BORDER_WIDTH = 1;
/**
 * The scaling factor for the border opacity; this is multiplied by the current
 * opacity being used to draw the text.
 */
static const Float BOX_BORDER_OPACITY = 0.5;

#ifndef MOZ_GFX_OPTIMIZE_MOBILE

static RefPtr<DrawTarget> gGlyphDrawTarget;
static RefPtr<SourceSurface> gGlyphMask;
static RefPtr<SourceSurface> gGlyphAtlas;
static DeviceColor gGlyphColor;

/**
 * Generates a new colored mini-font atlas from the mini-font mask.
 */
static bool MakeGlyphAtlas(const DeviceColor& aColor) {
  gGlyphAtlas = nullptr;
  if (!gGlyphDrawTarget) {
    gGlyphDrawTarget =
        gfxPlatform::GetPlatform()->CreateOffscreenContentDrawTarget(
            IntSize(MINIFONT_WIDTH * 16, MINIFONT_HEIGHT),
            SurfaceFormat::B8G8R8A8);
    if (!gGlyphDrawTarget) {
      return false;
    }
  }
  if (!gGlyphMask) {
    gGlyphMask = gGlyphDrawTarget->CreateSourceSurfaceFromData(
        const_cast<uint8_t*>(gMiniFontData),
        IntSize(MINIFONT_WIDTH * 16, MINIFONT_HEIGHT), MINIFONT_WIDTH * 16,
        SurfaceFormat::A8);
    if (!gGlyphMask) {
      return false;
    }
  }
  gGlyphDrawTarget->MaskSurface(ColorPattern(aColor), gGlyphMask, Point(0, 0),
                                DrawOptions(1.0f, CompositionOp::OP_SOURCE));
  gGlyphAtlas = gGlyphDrawTarget->Snapshot();
  if (!gGlyphAtlas) {
    return false;
  }
  gGlyphColor = aColor;
  return true;
}

/**
 * Reuse the current mini-font atlas if the color matches, otherwise regenerate
 * it.
 */
static inline already_AddRefed<SourceSurface> GetGlyphAtlas(
    const DeviceColor& aColor) {
  // Get the opaque color, ignoring any transparency which will be handled
  // later.
  DeviceColor color(aColor.r, aColor.g, aColor.b);
  if ((gGlyphAtlas && gGlyphColor == color) || MakeGlyphAtlas(color)) {
    return do_AddRef(gGlyphAtlas);
  }
  return nullptr;
}

/**
 * Clear any cached glyph atlas resources.
 */
static void PurgeGlyphAtlas() {
  gGlyphAtlas = nullptr;
  gGlyphDrawTarget = nullptr;
  gGlyphMask = nullptr;
}

// WebRender layer manager user data that will get signaled when the layer
// manager is destroyed.
class WRUserData : public layers::LayerUserData,
                   public LinkedListElement<WRUserData> {
 public:
  explicit WRUserData(layers::WebRenderLayerManager* aManager);

  ~WRUserData();

  static void Assign(layers::WebRenderLayerManager* aManager) {
    if (!aManager->HasUserData(&sWRUserDataKey)) {
      aManager->SetUserData(&sWRUserDataKey, new WRUserData(aManager));
    }
  }

  void Remove() { mManager->RemoveUserData(&sWRUserDataKey); }

  layers::WebRenderLayerManager* mManager;

  static UserDataKey sWRUserDataKey;
};

static RefPtr<SourceSurface> gWRGlyphAtlas[8];
static LinkedList<WRUserData> gWRUsers;
UserDataKey WRUserData::sWRUserDataKey;

/**
 * Generates a transformed WebRender mini-font atlas for a given orientation.
 */
static already_AddRefed<SourceSurface> MakeWRGlyphAtlas(const Matrix* aMat) {
  IntSize size(MINIFONT_WIDTH * 16, MINIFONT_HEIGHT);
  // If the orientation is transposed, width/height are swapped.
  if (aMat && aMat->_11 == 0) {
    std::swap(size.width, size.height);
  }
  RefPtr<DrawTarget> ref =
      gfxPlatform::GetPlatform()->ScreenReferenceDrawTarget();
  RefPtr<DrawTarget> dt =
      gfxPlatform::GetPlatform()->CreateSimilarSoftwareDrawTarget(
          ref, size, SurfaceFormat::B8G8R8A8);
  if (!dt) {
    return nullptr;
  }
  if (aMat) {
    // Select appropriate transform matrix based on whether the
    // orientation is transposed.
    dt->SetTransform(aMat->_11 == 0
                         ? Matrix(0.0f, copysign(1.0f, aMat->_12),
                                  copysign(1.0f, aMat->_21), 0.0f,
                                  aMat->_21 < 0 ? MINIFONT_HEIGHT : 0.0f,
                                  aMat->_12 < 0 ? MINIFONT_WIDTH * 16 : 0.0f)
                         : Matrix(copysign(1.0f, aMat->_11), 0.0f, 0.0f,
                                  copysign(1.0f, aMat->_22),
                                  aMat->_11 < 0 ? MINIFONT_WIDTH * 16 : 0.0f,
                                  aMat->_22 < 0 ? MINIFONT_HEIGHT : 0.0f));
  }
  RefPtr<SourceSurface> mask = dt->CreateSourceSurfaceFromData(
      const_cast<uint8_t*>(gMiniFontData),
      IntSize(MINIFONT_WIDTH * 16, MINIFONT_HEIGHT), MINIFONT_WIDTH * 16,
      SurfaceFormat::A8);
  if (!mask) {
    return nullptr;
  }
  dt->MaskSurface(ColorPattern(DeviceColor::MaskOpaqueWhite()), mask,
                  Point(0, 0));
  return dt->Snapshot();
}

/**
 * Clear any cached WebRender glyph atlas resources.
 */
static void PurgeWRGlyphAtlas() {
  // For each WR layer manager, we need go through each atlas orientation
  // and see if it has a stashed image key. If it does, remove the image
  // from the layer manager.
  for (WRUserData* user : gWRUsers) {
    auto* manager = user->mManager;
    for (size_t i = 0; i < 8; i++) {
      if (gWRGlyphAtlas[i]) {
        uint32_t handle = (uint32_t)(uintptr_t)gWRGlyphAtlas[i]->GetUserData(
            reinterpret_cast<UserDataKey*>(manager));
        if (handle) {
          manager->GetRenderRootStateManager()->AddImageKeyForDiscard(
              wr::ImageKey{manager->WrBridge()->GetNamespace(), handle});
        }
      }
    }
  }
  // Remove the layer managers' destroy notifications only after processing
  // so as not to mess up gWRUsers iteration.
  while (!gWRUsers.isEmpty()) {
    gWRUsers.popFirst()->Remove();
  }
  // Finally, clear out the atlases.
  for (size_t i = 0; i < 8; i++) {
    gWRGlyphAtlas[i] = nullptr;
  }
}

WRUserData::WRUserData(layers::WebRenderLayerManager* aManager)
    : mManager(aManager) {
  gWRUsers.insertFront(this);
}

WRUserData::~WRUserData() {
  // When the layer manager is destroyed, we need go through each
  // atlas and remove any assigned image keys.
  if (isInList()) {
    for (size_t i = 0; i < 8; i++) {
      if (gWRGlyphAtlas[i]) {
        gWRGlyphAtlas[i]->RemoveUserData(
            reinterpret_cast<UserDataKey*>(mManager));
      }
    }
  }
}

static already_AddRefed<SourceSurface> GetWRGlyphAtlas(DrawTarget& aDrawTarget,
                                                       const Matrix* aMat) {
  uint32_t key = 0;
  // Encode orientation in the key.
  if (aMat) {
    if (aMat->_11 == 0) {
      key |= 4 | (aMat->_12 < 0 ? 1 : 0) | (aMat->_21 < 0 ? 2 : 0);
    } else {
      key |= (aMat->_11 < 0 ? 1 : 0) | (aMat->_22 < 0 ? 2 : 0);
    }
  }

  // Check if an atlas was already created, or create one if necessary.
  RefPtr<SourceSurface> atlas = gWRGlyphAtlas[key];
  if (!atlas) {
    atlas = MakeWRGlyphAtlas(aMat);
    gWRGlyphAtlas[key] = atlas;
  }

  // The atlas may exist, but an image key may not be assigned for it to
  // the given layer manager.
  auto* tdt = static_cast<layout::TextDrawTarget*>(&aDrawTarget);
  auto* manager = tdt->WrLayerManager();
  if (!atlas->GetUserData(reinterpret_cast<UserDataKey*>(manager))) {
    // No image key, so we need to map the atlas' data for transfer to WR.
    RefPtr<DataSourceSurface> dataSurface = atlas->GetDataSurface();
    if (!dataSurface) {
      return nullptr;
    }
    DataSourceSurface::ScopedMap map(dataSurface, DataSourceSurface::READ);
    if (!map.IsMapped()) {
      return nullptr;
    }
    // Transfer the data and get an image key for it.
    Maybe<wr::ImageKey> result = tdt->DefineImage(
        atlas->GetSize(), map.GetStride(), atlas->GetFormat(), map.GetData());
    if (!result.isSome()) {
      return nullptr;
    }
    // Assign the image key to the atlas.
    atlas->AddUserData(reinterpret_cast<UserDataKey*>(manager),
                       (void*)(uintptr_t)result.value().mHandle, nullptr);
    // Create a user data notification for when the layer manager is
    // destroyed so we can clean up any assigned image keys.
    WRUserData::Assign(manager);
  }
  return atlas.forget();
}

static void DrawHexChar(uint32_t aDigit, Float aLeft, Float aTop,
                        DrawTarget& aDrawTarget, SourceSurface* aAtlas,
                        const DeviceColor& aColor,
                        const Matrix* aMat = nullptr) {
  Rect dest(aLeft, aTop, MINIFONT_WIDTH, MINIFONT_HEIGHT);
  if (aDrawTarget.GetBackendType() == BackendType::WEBRENDER_TEXT) {
    // For WR, we need to get the image key assigned to the given WR layer
    // manager for referencing the image.
    auto* tdt = static_cast<layout::TextDrawTarget*>(&aDrawTarget);
    auto* manager = tdt->WrLayerManager();
    wr::ImageKey key = {manager->WrBridge()->GetNamespace(),
                        (uint32_t)(uintptr_t)aAtlas->GetUserData(
                            reinterpret_cast<UserDataKey*>(manager))};
    // Transform the bounds of the atlas into the given orientation, and then
    // also transform a small clip rect which will be used to select the given
    // digit from the atlas.
    Rect bounds(aLeft - aDigit * MINIFONT_WIDTH, aTop, MINIFONT_WIDTH * 16,
                MINIFONT_HEIGHT);
    if (aMat) {
      // Width and height may be negative after the transform, so move the rect
      // if necessary and fix size.
      bounds = aMat->TransformRect(bounds);
      bounds.x += std::min(bounds.width, 0.0f);
      bounds.y += std::min(bounds.height, 0.0f);
      bounds.width = fabs(bounds.width);
      bounds.height = fabs(bounds.height);
      dest = aMat->TransformRect(dest);
      dest.x += std::min(dest.width, 0.0f);
      dest.y += std::min(dest.height, 0.0f);
      dest.width = fabs(dest.width);
      dest.height = fabs(dest.height);
    }
    // Finally, push the colored image with point filtering.
    tdt->PushImage(key, bounds, dest, wr::ImageRendering::Pixelated,
                   wr::ToColorF(aColor));
  } else {
    // For the normal case, just draw the given digit from the atlas. Point
    // filtering is used to ensure the mini-font rectangles stay sharp with any
    // scaling. Handle any transparency here as well.
    aDrawTarget.DrawSurface(
        aAtlas, dest,
        Rect(aDigit * MINIFONT_WIDTH, 0, MINIFONT_WIDTH, MINIFONT_HEIGHT),
        DrawSurfaceOptions(SamplingFilter::POINT),
        DrawOptions(aColor.a, CompositionOp::OP_OVER, AntialiasMode::NONE));
  }
}

void gfxFontMissingGlyphs::Purge() {
  PurgeGlyphAtlas();
  PurgeWRGlyphAtlas();
}

#else  // MOZ_GFX_OPTIMIZE_MOBILE

void gfxFontMissingGlyphs::Purge() {}

#endif

void gfxFontMissingGlyphs::Shutdown() { Purge(); }

void gfxFontMissingGlyphs::DrawMissingGlyph(uint32_t aChar, const Rect& aRect,
                                            DrawTarget& aDrawTarget,
                                            const Pattern& aPattern,
                                            uint32_t aAppUnitsPerDevPixel,
                                            const Matrix* aMat) {
  Rect rect(aRect);
  // If there is an orientation transform, reorient the bounding rect.
  if (aMat) {
    rect.MoveBy(-aRect.BottomLeft());
    rect = aMat->TransformBounds(rect);
    rect.MoveBy(aRect.BottomLeft());
  }

  // If we're currently drawing with some kind of pattern, we just draw the
  // missing-glyph data in black.
  DeviceColor color = aPattern.GetType() == PatternType::COLOR
                          ? static_cast<const ColorPattern&>(aPattern).mColor
                          : ToDeviceColor(sRGBColor::OpaqueBlack());

  // Stroke a rectangle so that the stroke's left edge is inset one pixel
  // from the left edge of the glyph box and the stroke's right edge
  // is inset one pixel from the right edge of the glyph box.
  Float halfBorderWidth = BOX_BORDER_WIDTH / 2.0;
  Float borderLeft = rect.X() + BOX_HORIZONTAL_INSET + halfBorderWidth;
  Float borderRight = rect.XMost() - BOX_HORIZONTAL_INSET - halfBorderWidth;
  Rect borderStrokeRect(borderLeft, rect.Y() + halfBorderWidth,
                        borderRight - borderLeft,
                        rect.Height() - 2.0 * halfBorderWidth);
  if (!borderStrokeRect.IsEmpty()) {
    ColorPattern adjustedColor(color);
    adjustedColor.mColor.a *= BOX_BORDER_OPACITY;
#ifdef MOZ_GFX_OPTIMIZE_MOBILE
    aDrawTarget.FillRect(borderStrokeRect, adjustedColor);
#else
    StrokeOptions strokeOptions(BOX_BORDER_WIDTH);
    aDrawTarget.StrokeRect(borderStrokeRect, adjustedColor, strokeOptions);
#endif
  }

#ifndef MOZ_GFX_OPTIMIZE_MOBILE
  RefPtr<SourceSurface> atlas =
      aDrawTarget.GetBackendType() == BackendType::WEBRENDER_TEXT
          ? GetWRGlyphAtlas(aDrawTarget, aMat)
          : GetGlyphAtlas(color);
  if (!atlas) {
    return;
  }

  Point center = rect.Center();
  Float halfGap = HEX_CHAR_GAP / 2.f;
  Float top = -(MINIFONT_HEIGHT + halfGap);
  // We always want integer scaling, otherwise the "bitmap" glyphs will look
  // even uglier than usual when zoomed
  int32_t devPixelsPerCSSPx =
      std::max<int32_t>(1, AppUnitsPerCSSPixel() / aAppUnitsPerDevPixel);

  Matrix tempMat;
  if (aMat) {
    // If there is an orientation transform, since draw target transforms may
    // not be supported, scale and translate it so that it can be directly used
    // for rendering the mini font without changing the draw target transform.
    tempMat = Matrix(*aMat)
                  .PostScale(devPixelsPerCSSPx, devPixelsPerCSSPx)
                  .PostTranslate(center);
    aMat = &tempMat;
  } else {
    // Otherwise, scale and translate the draw target transform assuming it
    // supports that.
    tempMat = aDrawTarget.GetTransform();
    aDrawTarget.SetTransform(Matrix(tempMat).PreTranslate(center).PreScale(
        devPixelsPerCSSPx, devPixelsPerCSSPx));
  }

  if (aChar < 0x10000) {
    if (rect.Width() >= 2 * (MINIFONT_WIDTH + HEX_CHAR_GAP) &&
        rect.Height() >= 2 * MINIFONT_HEIGHT + HEX_CHAR_GAP) {
      // Draw 4 digits for BMP
      Float left = -(MINIFONT_WIDTH + halfGap);
      DrawHexChar((aChar >> 12) & 0xF, left, top, aDrawTarget, atlas, color,
                  aMat);
      DrawHexChar((aChar >> 8) & 0xF, halfGap, top, aDrawTarget, atlas, color,
                  aMat);
      DrawHexChar((aChar >> 4) & 0xF, left, halfGap, aDrawTarget, atlas, color,
                  aMat);
      DrawHexChar(aChar & 0xF, halfGap, halfGap, aDrawTarget, atlas, color,
                  aMat);
    }
  } else {
    if (rect.Width() >= 3 * (MINIFONT_WIDTH + HEX_CHAR_GAP) &&
        rect.Height() >= 2 * MINIFONT_HEIGHT + HEX_CHAR_GAP) {
      // Draw 6 digits for non-BMP
      Float first = -(MINIFONT_WIDTH * 1.5 + HEX_CHAR_GAP);
      Float second = -(MINIFONT_WIDTH / 2.0);
      Float third = (MINIFONT_WIDTH / 2.0 + HEX_CHAR_GAP);
      DrawHexChar((aChar >> 20) & 0xF, first, top, aDrawTarget, atlas, color,
                  aMat);
      DrawHexChar((aChar >> 16) & 0xF, second, top, aDrawTarget, atlas, color,
                  aMat);
      DrawHexChar((aChar >> 12) & 0xF, third, top, aDrawTarget, atlas, color,
                  aMat);
      DrawHexChar((aChar >> 8) & 0xF, first, halfGap, aDrawTarget, atlas, color,
                  aMat);
      DrawHexChar((aChar >> 4) & 0xF, second, halfGap, aDrawTarget, atlas,
                  color, aMat);
      DrawHexChar(aChar & 0xF, third, halfGap, aDrawTarget, atlas, color, aMat);
    }
  }

  if (!aMat) {
    // The draw target transform was changed, so it must be restored to
    // the original value.
    aDrawTarget.SetTransform(tempMat);
  }
#endif
}

Float gfxFontMissingGlyphs::GetDesiredMinWidth(uint32_t aChar,
                                               uint32_t aAppUnitsPerDevPixel) {
  /**
   * The minimum desired width for a missing-glyph glyph box. I've laid it out
   * like this so you can see what goes where.
   */
  Float width = BOX_HORIZONTAL_INSET + BOX_BORDER_WIDTH + HEX_CHAR_GAP +
                MINIFONT_WIDTH + HEX_CHAR_GAP + MINIFONT_WIDTH +
                ((aChar < 0x10000) ? 0 : HEX_CHAR_GAP + MINIFONT_WIDTH) +
                HEX_CHAR_GAP + BOX_BORDER_WIDTH + BOX_HORIZONTAL_INSET;
  // Note that this will give us floating-point division, so the width will
  // -not- be snapped to integer multiples of its basic pixel value
  width *= Float(AppUnitsPerCSSPixel()) / aAppUnitsPerDevPixel;
  return width;
}
