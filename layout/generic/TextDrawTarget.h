/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TextDrawTarget_h
#define TextDrawTarget_h

#include "mozilla/gfx/2D.h"
#include "mozilla/layers/WebRenderLayerManager.h"
#include "mozilla/layers/WebRenderBridgeChild.h"
#include "mozilla/webrender/WebRenderAPI.h"
#include "mozilla/layers/StackingContextHelper.h"

namespace mozilla {
namespace layout {

using namespace gfx;

// This class is fake DrawTarget, used to intercept text draw calls, while
// also collecting up the other aspects of text natively.
//
// When using advanced-layers in nsDisplayText's constructor, we construct this
// and run the full painting algorithm with this as the DrawTarget. This is
// done to avoid having to massively refactor gecko's text painting code (which
// has lots of components shared between other rendering algorithms).
//
// In some phases of the painting algorithm, we can grab the relevant values
// and feed them directly into TextDrawTarget. For instance, selections,
// decorations, and shadows are handled in this manner. In those cases we can
// also short-circuit the painting algorithm to save work.
//
// In other phases, the computed values are sufficiently buried in complex
// code that it's best for us to just intercept the final draw calls. This
// is how we handle computing the glyphs of the main text and text-emphasis
// (see our overloaded FillGlyphs implementation).
//
// To be clear: this is a big hack. With time we hope to refactor the codebase
// so that all the elements of text are handled directly by TextDrawTarget,
// which is to say everything is done like we do selections and shadows now.
// This design is a good step for doing this work incrementally.
//
// This is also likely to be a bit buggy (missing or misinterpreted info)
// while we further develop the design.
//
// TextDrawTarget doesn't yet support all features. See mHasUnsupportedFeatures
// for details.
class TextDrawTarget : public DrawTarget
{
public:
  explicit TextDrawTarget(wr::DisplayListBuilder& aBuilder,
                          const layers::StackingContextHelper& aSc,
                          layers::WebRenderLayerManager* aManager,
                          nsDisplayItem* aItem,
                          nsRect& aBounds)
    : mBuilder(aBuilder), mSc(aSc), mManager(aManager)
  {

    // Compute clip/bounds
    auto appUnitsPerDevPixel = aItem->Frame()->PresContext()->AppUnitsPerDevPixel();
    LayoutDeviceRect layoutBoundsRect = LayoutDeviceRect::FromAppUnits(
        aBounds, appUnitsPerDevPixel);
    LayoutDeviceRect layoutClipRect = layoutBoundsRect;

    auto clip = aItem->GetClip();
    if (clip.HasClip()) {
      layoutClipRect = LayoutDeviceRect::FromAppUnits(
                  clip.GetClipRect(), appUnitsPerDevPixel);
    }

    mBoundsRect = aSc.ToRelativeLayoutRect(LayerRect::FromUnknownRect(layoutBoundsRect.ToUnknownRect()));
    mClipRect = aSc.ToRelativeLayoutRect(LayerRect::FromUnknownRect(layoutClipRect.ToUnknownRect()));

    mBackfaceVisible = !aItem->BackfaceIsHidden();

    mBuilder.Save();
  }

  // Prevent this from being copied
  TextDrawTarget(const TextDrawTarget& src) = delete;
  TextDrawTarget& operator=(const TextDrawTarget&) = delete;

  ~TextDrawTarget()
  {
    if (mHasUnsupportedFeatures) {
      mBuilder.Restore();
    } else {
      mBuilder.ClearSave();
    }
  }

  void FoundUnsupportedFeature() { mHasUnsupportedFeatures = true; }
  bool HasUnsupportedFeatures() { return mHasUnsupportedFeatures; }

  // This overload just stores the glyphs/font/color.
  void
  FillGlyphs(ScaledFont* aFont,
             const GlyphBuffer& aBuffer,
             const Pattern& aPattern,
             const DrawOptions& aOptions,
             const GlyphRenderingOptions* aRenderingOptions) override
  {
    // FIXME(?): Deal with AA on the DrawOptions, and the GlyphRenderingOptions

    // Make sure we're only given boring color patterns
    MOZ_RELEASE_ASSERT(aOptions.mCompositionOp == CompositionOp::OP_OVER);
    MOZ_RELEASE_ASSERT(aOptions.mAlpha == 1.0f);
    MOZ_RELEASE_ASSERT(aPattern.GetType() == PatternType::COLOR);
    auto* colorPat = static_cast<const ColorPattern*>(&aPattern);
    auto color = wr::ToColorF(colorPat->mColor);

    // Make sure the font exists, and can be serialized
    MOZ_RELEASE_ASSERT(aFont);
    if (!aFont->CanSerialize()) {
      FoundUnsupportedFeature();
      return;
    }

    // 170 is the maximum size gfxFont is expected to hand us
    AutoTArray<wr::GlyphInstance, 170> glyphs;
    glyphs.SetLength(aBuffer.mNumGlyphs);

    for (size_t i = 0; i < aBuffer.mNumGlyphs; i++) {
      wr::GlyphInstance& targetGlyph = glyphs[i];
      const gfx::Glyph& sourceGlyph = aBuffer.mGlyphs[i];
      targetGlyph.index = sourceGlyph.mIndex;
      targetGlyph.point = mSc.ToRelativeLayoutPoint(
              LayerPoint::FromUnknownPoint(sourceGlyph.mPosition));
    }

    mManager->WrBridge()->PushGlyphs(mBuilder, glyphs, aFont,
                                     color, mSc, mBoundsRect, mClipRect,
                                     mBackfaceVisible);
  }

  void
  AppendShadow(const wr::Shadow& aShadow)
  {
    mBuilder.PushShadow(mBoundsRect, mClipRect, mBackfaceVisible, aShadow);
    mHasShadows = true;
  }

  void
  TerminateShadows()
  {
    if (mHasShadows) {
      mBuilder.PopAllShadows();
      mHasShadows = false;
    }
  }

  void
  AppendSelectionRect(const LayoutDeviceRect& aRect, const Color& aColor)
  {
    auto rect = wr::ToLayoutRect(aRect);
    auto color = wr::ToColorF(aColor);
    mBuilder.PushRect(rect, mClipRect, mBackfaceVisible, color);
  }

  void
  AppendDecoration(const Point& aStart,
                   const Point& aEnd,
                   const float aThickness,
                   const bool aVertical,
                   const Color& aColor,
                   const uint8_t aStyle)
  {
    wr::Line decoration;

    // This function is basically designed to slide into the decoration drawing
    // code of nsCSSRendering with minimum disruption, to minimize the
    // chances of implementation drift. As such, it mostly looks like a call
    // to a skia-style StrokeLine method: two end-points, with a thickness
    // and style. Notably the end-points are *centered* in the block direction,
    // even though webrender wants a rect-like representation, where the points
    // are on corners.
    //
    // So we mangle the format here in a single centralized place, where neither
    // webrender nor nsCSSRendering has to care about this mismatch.
    decoration.baseline = (aVertical ? aStart.x : aStart.y) - aThickness / 2;
    decoration.start = aVertical ? aStart.y : aStart.x;
    decoration.end = aVertical ? aEnd.y : aEnd.x;
    decoration.width = aThickness;
    decoration.color = wr::ToColorF(aColor);
    decoration.orientation = aVertical
      ? wr::LineOrientation::Vertical
      : wr::LineOrientation::Horizontal;

    switch (aStyle) {
      case NS_STYLE_TEXT_DECORATION_STYLE_SOLID:
        decoration.style = wr::LineStyle::Solid;
        break;
      case NS_STYLE_TEXT_DECORATION_STYLE_DOTTED:
        decoration.style = wr::LineStyle::Dotted;
        break;
      case NS_STYLE_TEXT_DECORATION_STYLE_DASHED:
        decoration.style = wr::LineStyle::Dashed;
        break;
      case NS_STYLE_TEXT_DECORATION_STYLE_WAVY:
        decoration.style = wr::LineStyle::Wavy;
        break;
      // Double lines should be lowered to two solid lines
      case NS_STYLE_TEXT_DECORATION_STYLE_DOUBLE:
      default:
        MOZ_CRASH("TextDrawTarget received unsupported line style");
    }

    mBuilder.PushLine(mClipRect, mBackfaceVisible, decoration);
  }

private:
  // Whether anything unsupported was encountered. Currently:
  //
  // * Synthetic bold/italics
  // * SVG fonts
  // * Unserializable fonts
  // * Tofu glyphs
  // * Pratial ligatures
  // * Text writing-mode
  // * Text stroke
  bool mHasUnsupportedFeatures = false;

  // Whether PopAllShadows needs to be called
  bool mHasShadows = false;

  // Things used to push to webrender
  wr::DisplayListBuilder& mBuilder;
  const layers::StackingContextHelper& mSc;
  layers::WebRenderLayerManager* mManager;

  // Computed facts
  wr::LayerRect mBoundsRect;
  wr::LayerRect mClipRect;
  bool mBackfaceVisible;

  // The rest of this is dummy implementations of DrawTarget's API
public:
  DrawTargetType GetType() const override {
    return DrawTargetType::SOFTWARE_RASTER;
  }

  BackendType GetBackendType() const override {
    return BackendType::WEBRENDER_TEXT;
  }

  bool IsRecording() const override { return true; }
  bool IsCaptureDT() const override { return false; }

  already_AddRefed<SourceSurface> Snapshot() override {
    MOZ_CRASH("TextDrawTarget: Method shouldn't be called");
    return nullptr;
  }

  already_AddRefed<SourceSurface> IntoLuminanceSource(LuminanceType aLuminanceType,
                                                      float aOpacity) override {
    MOZ_CRASH("TextDrawTarget: Method shouldn't be called");
    return nullptr;
  }

  IntSize GetSize() override {
    MOZ_CRASH("TextDrawTarget: Method shouldn't be called");
    return IntSize(1, 1);
  }

  void Flush() override {
    MOZ_CRASH("TextDrawTarget: Method shouldn't be called");
  }

  void DrawCapturedDT(DrawTargetCapture *aCaptureDT,
                      const Matrix& aTransform) override {
    MOZ_CRASH("TextDrawTarget: Method shouldn't be called");
  }

  void DrawSurface(SourceSurface *aSurface,
                   const Rect &aDest,
                   const Rect &aSource,
                   const DrawSurfaceOptions &aSurfOptions,
                   const DrawOptions &aOptions) override {
    MOZ_CRASH("TextDrawTarget: Method shouldn't be called");
  }

  void DrawFilter(FilterNode *aNode,
                          const Rect &aSourceRect,
                          const Point &aDestPoint,
                          const DrawOptions &aOptions) override {
    MOZ_CRASH("TextDrawTarget: Method shouldn't be called");
  }

  void DrawSurfaceWithShadow(SourceSurface *aSurface,
                             const Point &aDest,
                             const Color &aColor,
                             const Point &aOffset,
                             Float aSigma,
                             CompositionOp aOperator) override {
    MOZ_CRASH("TextDrawTarget: Method shouldn't be called");
  }

  void ClearRect(const Rect &aRect) override {
    MOZ_CRASH("TextDrawTarget: Method shouldn't be called");
  }

  void CopySurface(SourceSurface *aSurface,
                   const IntRect &aSourceRect,
                   const IntPoint &aDestination) override {
    MOZ_CRASH("TextDrawTarget: Method shouldn't be called");
  }

  void FillRect(const Rect &aRect,
                const Pattern &aPattern,
                const DrawOptions &aOptions = DrawOptions()) override {
    MOZ_CRASH("TextDrawTarget: Method shouldn't be called");
  }

  void StrokeRect(const Rect &aRect,
                  const Pattern &aPattern,
                  const StrokeOptions &aStrokeOptions,
                  const DrawOptions &aOptions) override {
    MOZ_CRASH("TextDrawTarget: Method shouldn't be called");
  }

  void StrokeLine(const Point &aStart,
                  const Point &aEnd,
                  const Pattern &aPattern,
                  const StrokeOptions &aStrokeOptions,
                  const DrawOptions &aOptions) override {
    MOZ_CRASH("TextDrawTarget: Method shouldn't be called");
  }


  void Stroke(const Path *aPath,
              const Pattern &aPattern,
              const StrokeOptions &aStrokeOptions,
              const DrawOptions &aOptions) override {
    MOZ_CRASH("TextDrawTarget: Method shouldn't be called");
  }

  void Fill(const Path *aPath,
            const Pattern &aPattern,
            const DrawOptions &aOptions) override {
    MOZ_CRASH("TextDrawTarget: Method shouldn't be called");
  }

  void StrokeGlyphs(ScaledFont* aFont,
                    const GlyphBuffer& aBuffer,
                    const Pattern& aPattern,
                    const StrokeOptions& aStrokeOptions,
                    const DrawOptions& aOptions,
                    const GlyphRenderingOptions* aRenderingOptions) override {
    MOZ_CRASH("TextDrawTarget: Method shouldn't be called");
  }

  void Mask(const Pattern &aSource,
                    const Pattern &aMask,
                    const DrawOptions &aOptions) override {
    MOZ_CRASH("TextDrawTarget: Method shouldn't be called");
  }

  void MaskSurface(const Pattern &aSource,
                   SourceSurface *aMask,
                   Point aOffset,
                   const DrawOptions &aOptions) override {
    MOZ_CRASH("TextDrawTarget: Method shouldn't be called");
  }

  bool Draw3DTransformedSurface(SourceSurface* aSurface,
                                const Matrix4x4& aMatrix) override {
    MOZ_CRASH("TextDrawTarget: Method shouldn't be called");
  }

  void PushClip(const Path *aPath) override {
    // Fine to pretend we do this
  }

  void PushClipRect(const Rect &aRect) override {
    // Fine to pretend we do this
  }

  void PushDeviceSpaceClipRects(const IntRect* aRects, uint32_t aCount) override {
    MOZ_CRASH("TextDrawTarget: Method shouldn't be called");
  }

  void PopClip() override {
    // Fine to pretend we do this
  }

  void PushLayer(bool aOpaque, Float aOpacity,
                         SourceSurface* aMask,
                         const Matrix& aMaskTransform,
                         const IntRect& aBounds,
                         bool aCopyBackground) override {
    // Fine to pretend we do this
  }

  void PopLayer() override {
    // Fine to pretend we do this
  }


  already_AddRefed<SourceSurface> CreateSourceSurfaceFromData(unsigned char *aData,
                                                              const IntSize &aSize,
                                                              int32_t aStride,
                                                              SurfaceFormat aFormat) const override {
    MOZ_CRASH("TextDrawTarget: Method shouldn't be called");
    return nullptr;
  }

  already_AddRefed<SourceSurface> OptimizeSourceSurface(SourceSurface *aSurface) const override {
    MOZ_CRASH("TextDrawTarget: Method shouldn't be called");
    return nullptr;
  }

  already_AddRefed<SourceSurface>
  CreateSourceSurfaceFromNativeSurface(const NativeSurface &aSurface) const override {
    MOZ_CRASH("TextDrawTarget: Method shouldn't be called");
    return nullptr;
  }

  already_AddRefed<DrawTarget>
  CreateSimilarDrawTarget(const IntSize &aSize, SurfaceFormat aFormat) const override {
    MOZ_CRASH("TextDrawTarget: Method shouldn't be called");
    return nullptr;
  }

  already_AddRefed<PathBuilder> CreatePathBuilder(FillRule aFillRule) const override {
    MOZ_CRASH("TextDrawTarget: Method shouldn't be called");
    return nullptr;
  }

  already_AddRefed<FilterNode> CreateFilter(FilterType aType) override {
    MOZ_CRASH("TextDrawTarget: Method shouldn't be called");
    return nullptr;
  }

  already_AddRefed<GradientStops>
  CreateGradientStops(GradientStop *aStops,
                      uint32_t aNumStops,
                      ExtendMode aExtendMode) const override {
    MOZ_CRASH("TextDrawTarget: Method shouldn't be called");
    return nullptr;
  }

  void* GetNativeSurface(NativeSurfaceType aType) override {
    MOZ_CRASH("TextDrawTarget: Method shouldn't be called");
    return nullptr;
  }

  void DetachAllSnapshots() override {
    MOZ_CRASH("TextDrawTarget: Method shouldn't be called");
  }
};

}
}

#endif
