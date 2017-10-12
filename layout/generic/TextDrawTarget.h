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

// This is used by all Advanced Layers users, so we use plain gfx types
struct TextRunFragment {
  ScaledFont* font;
  wr::ColorF color;
  nsTArray<wr::GlyphInstance> glyphs;
};

// Only webrender handles this, so we use webrender types
struct SelectionFragment {
  wr::ColorF color;
  wr::LayoutRect rect;
};

// Selections are used in nsTextFrame to hack in sub-frame style changes.
// Most notably text-shadows can be changed by selections, and so we need to
// group all the glyphs and decorations attached to a shadow. We do this by
// having shadows apply to an entire SelectedTextRunFragment, and creating
// one for each "piece" of selection.
//
// For instance, this text:
//
// Hello [there] my name [is Mega]man
//          ^                ^
//  normal selection      Ctrl+F highlight selection (yeah it's very overloaded)
//
// Would be broken up into 5 SelectedTextRunFragments
//
// ["Hello ", "there", " my name ", "is Mega", "man"]
//
// For almost all nsTextFrames, there will be only one SelectedTextRunFragment.
struct SelectedTextRunFragment {
  Maybe<SelectionFragment> selection;
  AutoTArray<wr::Shadow, 1> shadows;
  AutoTArray<TextRunFragment, 1> text;
  AutoTArray<wr::Line, 1> beforeDecorations;
  AutoTArray<wr::Line, 1> afterDecorations;
};

}
}

// AutoTArray is bad
template<>
struct nsTArray_CopyChooser<mozilla::layout::SelectedTextRunFragment>
{
  typedef nsTArray_CopyWithConstructors<mozilla::layout::SelectedTextRunFragment> Type;
};

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
// This does not currently support SVG text effects.
class TextDrawTarget : public DrawTarget
{
public:
  // The different phases of drawing the text we're in
  // Each should only happen once, and in the given order.
  enum class Phase : uint8_t {
    eSelection, eUnderline, eOverline, eGlyphs, eEmphasisMarks, eLineThrough
  };

  explicit TextDrawTarget(const layers::StackingContextHelper& aSc)
  : mCurrentlyDrawing(Phase::eSelection),
    mHasUnsupportedFeatures(false),
    mSc(aSc)
  {
    SetSelectionIndex(0);
  }

  // Prevent this from being copied
  TextDrawTarget(const TextDrawTarget& src) = delete;
  TextDrawTarget& operator=(const TextDrawTarget&) = delete;

  // Change the phase of text we're drawing.
  void StartDrawing(Phase aPhase) { mCurrentlyDrawing = aPhase; }
  void FoundUnsupportedFeature() { mHasUnsupportedFeatures = true; }

  void SetSelectionIndex(size_t i) {
    // i should only be accessed if i-1 has already been
    MOZ_ASSERT(i <= mParts.Length());

    if (mParts.Length() == i){
      mParts.AppendElement();
    }

    mCurrentPart = &mParts[i];
  }

  // This overload just stores the glyphs/font/color.
  void
  FillGlyphs(ScaledFont* aFont,
             const GlyphBuffer& aBuffer,
             const Pattern& aPattern,
             const DrawOptions& aOptions,
             const GlyphRenderingOptions* aRenderingOptions) override
  {
    // FIXME: figure out which of these asserts are real
    MOZ_RELEASE_ASSERT(aOptions.mCompositionOp == CompositionOp::OP_OVER);
    MOZ_RELEASE_ASSERT(aOptions.mAlpha == 1.0f);

    // Make sure we're only given color patterns
    MOZ_RELEASE_ASSERT(aPattern.GetType() == PatternType::COLOR);
    const ColorPattern* colorPat = static_cast<const ColorPattern*>(&aPattern);

    // Make sure the font exists
    MOZ_RELEASE_ASSERT(aFont);

    // FIXME(?): Deal with AA on the DrawOptions, and the GlyphRenderingOptions

    if (mCurrentlyDrawing != Phase::eGlyphs &&
        mCurrentlyDrawing != Phase::eEmphasisMarks) {
      MOZ_CRASH("TextDrawTarget received glyphs in wrong phase");
    }

    // We need to push a new TextRunFragment whenever the font/color changes
    // (usually this implies some font fallback from mixing languages/emoji)
    TextRunFragment* fragment;
    if (mCurrentPart->text.IsEmpty() ||
        mCurrentPart->text.LastElement().font != aFont ||
        !(mCurrentPart->text.LastElement().color ==  wr::ToColorF(colorPat->mColor))) {
      fragment = mCurrentPart->text.AppendElement();
      fragment->font = aFont;
      fragment->color = wr::ToColorF(colorPat->mColor);
    } else {
      fragment = &mCurrentPart->text.LastElement();
    }

    nsTArray<wr::GlyphInstance>& glyphs = fragment->glyphs;

    size_t oldLength = glyphs.Length();
    glyphs.SetLength(oldLength + aBuffer.mNumGlyphs);

    for (size_t i = 0; i < aBuffer.mNumGlyphs; i++) {
      wr::GlyphInstance& targetGlyph = glyphs[oldLength + i];
      const gfx::Glyph& sourceGlyph = aBuffer.mGlyphs[i];
      targetGlyph.index = sourceGlyph.mIndex;
      targetGlyph.point = mSc.ToRelativeLayoutPoint(
              LayerPoint::FromUnknownPoint(sourceGlyph.mPosition));
    }
  }

  void
  AppendShadow(const wr::Shadow& aShadow) {
    mCurrentPart->shadows.AppendElement(aShadow);
  }

  void
  SetSelectionRect(const LayoutDeviceRect& aRect, const Color& aColor)
  {
    SelectionFragment frag;
    frag.rect = wr::ToLayoutRect(aRect);
    frag.color = wr::ToColorF(aColor);
    mCurrentPart->selection = Some(frag);
  }

  void
  AppendDecoration(const Point& aStart,
                   const Point& aEnd,
                   const float aThickness,
                   const bool aVertical,
                   const Color& aColor,
                   const uint8_t aStyle)
  {
    wr::Line* decoration;

    switch (mCurrentlyDrawing) {
      case Phase::eUnderline:
      case Phase::eOverline:
        decoration = mCurrentPart->beforeDecorations.AppendElement();
        break;
      case Phase::eLineThrough:
        decoration = mCurrentPart->afterDecorations.AppendElement();
        break;
      default:
        MOZ_CRASH("TextDrawTarget received Decoration in wrong phase");
    }

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
    decoration->baseline = (aVertical ? aStart.x : aStart.y) - aThickness / 2;
    decoration->start = aVertical ? aStart.y : aStart.x;
    decoration->end = aVertical ? aEnd.y : aEnd.x;
    decoration->width = aThickness;
    decoration->color = wr::ToColorF(aColor);
    decoration->orientation = aVertical
      ? wr::LineOrientation::Vertical
      : wr::LineOrientation::Horizontal;

    switch (aStyle) {
      case NS_STYLE_TEXT_DECORATION_STYLE_SOLID:
        decoration->style = wr::LineStyle::Solid;
        break;
      case NS_STYLE_TEXT_DECORATION_STYLE_DOTTED:
        decoration->style = wr::LineStyle::Dotted;
        break;
      case NS_STYLE_TEXT_DECORATION_STYLE_DASHED:
        decoration->style = wr::LineStyle::Dashed;
        break;
      case NS_STYLE_TEXT_DECORATION_STYLE_WAVY:
        decoration->style = wr::LineStyle::Wavy;
        break;
      // Double lines should be lowered to two solid lines
      case NS_STYLE_TEXT_DECORATION_STYLE_DOUBLE:
      default:
        MOZ_CRASH("TextDrawTarget received unsupported line style");
    }


  }

  const nsTArray<SelectedTextRunFragment>& GetParts() { return mParts; }

  bool
  CanSerializeFonts()
  {
    if (mHasUnsupportedFeatures) {
      return false;
    }

    for (const SelectedTextRunFragment& part : GetParts()) {
      for (const TextRunFragment& frag : part.text) {
        if (!frag.font->CanSerialize()) {
          return false;
        }
      }
    }
    return true;
  }

  bool
  CreateWebRenderCommands(mozilla::wr::DisplayListBuilder& aBuilder,
                          const layers::StackingContextHelper& aSc,
                          layers::WebRenderLayerManager* aManager,
                          nsDisplayItem* aItem,
                          nsRect& aBounds) {

  if (!CanSerializeFonts()) {
    return false;
  }

  // Drawing order: selections,
  //                shadows,
  //                underline, overline, [grouped in one array]
  //                text, emphasisText,  [grouped in one array]
  //                lineThrough

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

  LayerRect boundsRect = LayerRect::FromUnknownRect(layoutBoundsRect.ToUnknownRect());
  LayerRect clipRect = LayerRect::FromUnknownRect(layoutClipRect.ToUnknownRect());

  bool backfaceVisible = !aItem->BackfaceIsHidden();

  wr::LayoutRect wrBoundsRect = aSc.ToRelativeLayoutRect(boundsRect);
  wr::LayoutRect wrClipRect = aSc.ToRelativeLayoutRect(clipRect);


  // Create commands
  for (auto& part : GetParts()) {
    if (part.selection) {
      auto selection = part.selection.value();
      aBuilder.PushRect(selection.rect, wrClipRect, backfaceVisible, selection.color);
    }
  }

  for (auto& part : GetParts()) {
    // WR takes the shadows in CSS-order (reverse of rendering order),
    // because the drawing of a shadow actually occurs when it's popped.
    for (const wr::Shadow& shadow : part.shadows) {
      aBuilder.PushShadow(wrBoundsRect, wrClipRect, backfaceVisible, shadow);
    }

    for (const wr::Line& decoration : part.beforeDecorations) {
      aBuilder.PushLine(wrClipRect, backfaceVisible, decoration);
    }

    for (const mozilla::layout::TextRunFragment& text : part.text) {
      aManager->WrBridge()->PushGlyphs(aBuilder, text.glyphs, text.font,
                                       text.color, aSc, wrBoundsRect, wrClipRect,
                                       backfaceVisible);
    }

    for (const wr::Line& decoration : part.afterDecorations) {
      aBuilder.PushLine(wrClipRect, backfaceVisible, decoration);
    }

    for (size_t i = 0; i < part.shadows.Length(); ++i) {
      aBuilder.PopShadow();
    }
  }

  return true;
}

private:
  // The part of the text we're currently drawing (glyphs, underlines, etc.)
  Phase mCurrentlyDrawing;

  // Which chunk of mParts is actively being populated
  SelectedTextRunFragment* mCurrentPart;

  // Chunks of the text, grouped by selection
  AutoTArray<SelectedTextRunFragment, 1> mParts;

  // Whether Tofu or SVG fonts were encountered
  bool mHasUnsupportedFeatures;

  // Needs to be saved so FillGlyphs can use this to offset glyphs to
  // relative space. Shouldn't be used otherwise (may dangle if we move
  // to retaining TextDrawTargets)
  const layers::StackingContextHelper& mSc;

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
