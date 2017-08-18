/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TextDrawTarget_h
#define TextDrawTarget_h

#include "mozilla/gfx/2D.h"

namespace mozilla {
namespace layout {

using namespace gfx;

// This is used by all Advanced Layers users, so we use plain gfx types
struct TextRunFragment {
  ScaledFont* font;
  Color color;
  nsTArray<gfx::Glyph> glyphs;
};

// Only webrender handles this, so we use webrender types
struct SelectionFragment {
  wr::ColorF color;
  wr::LayoutRect rect;
};

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

  explicit TextDrawTarget()
  : mCurrentlyDrawing(Phase::eSelection)
  {
    mCurrentTarget = gfx::Factory::CreateDrawTarget(gfx::BackendType::SKIA, IntSize(1, 1), gfx::SurfaceFormat::B8G8R8A8);
  }

  // Prevent this from being copied
  TextDrawTarget(const TextDrawTarget& src) = delete;
  TextDrawTarget& operator=(const TextDrawTarget&) = delete;

  // Change the phase of text we're drawing.
  void StartDrawing(Phase aPhase) { mCurrentlyDrawing = aPhase; }

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
    if (mText.IsEmpty() ||
        mText.LastElement().font != aFont ||
        mText.LastElement().color != colorPat->mColor) {
      fragment = mText.AppendElement();
      fragment->font = aFont;
      fragment->color = colorPat->mColor;
    } else {
      fragment = &mText.LastElement();
    }

    nsTArray<Glyph>& glyphs = fragment->glyphs;

    size_t oldLength = glyphs.Length();
    glyphs.SetLength(oldLength + aBuffer.mNumGlyphs);
    PodCopy(glyphs.Elements() + oldLength, aBuffer.mGlyphs, aBuffer.mNumGlyphs);
  }

  void AppendShadow(const wr::TextShadow& aShadow) { mShadows.AppendElement(aShadow); }

  void
  AppendSelection(const LayoutDeviceRect& aRect, const Color& aColor)
  {
    SelectionFragment frag;
    frag.rect = wr::ToLayoutRect(aRect);
    frag.color = wr::ToColorF(aColor);
    mSelections.AppendElement(frag);
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
        decoration = mBeforeDecorations.AppendElement();
        break;
      case Phase::eLineThrough:
        decoration = mAfterDecorations.AppendElement();
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

  const nsTArray<wr::TextShadow>& GetShadows() { return mShadows; }
  const nsTArray<TextRunFragment>& GetText() { return mText; }
  const nsTArray<SelectionFragment>& GetSelections() { return mSelections; }
  const nsTArray<wr::Line>& GetBeforeDecorations() { return mBeforeDecorations; }
  const nsTArray<wr::Line>& GetAfterDecorations() { return mAfterDecorations; }

  bool
  CanSerializeFonts()
  {
    for (const TextRunFragment& frag : GetText()) {
      if (!frag.font->CanSerialize()) {
        return false;
      }
    }
    return true;
  }

  bool
  TryMerge(const TextDrawTarget& other) {
    if (mShadows != other.mShadows) {
      return false;
    }

    mText.AppendElements(other.mText);
    mSelections.AppendElements(other.mSelections);
    mBeforeDecorations.AppendElements(other.mBeforeDecorations);
    mAfterDecorations.AppendElements(other.mAfterDecorations);

    return true;
  }


private:
  // The part of the text we're currently drawing (glyphs, underlines, etc.)
  Phase mCurrentlyDrawing;

  // Properties of the whole text
  nsTArray<wr::TextShadow> mShadows;
  nsTArray<TextRunFragment> mText;
  nsTArray<SelectionFragment> mSelections;
  nsTArray<wr::Line> mBeforeDecorations;
  nsTArray<wr::Line> mAfterDecorations;

  // A dummy to handle parts of the DrawTarget impl we don't care for
  RefPtr<DrawTarget> mCurrentTarget;

  // The rest of this is dummy implementations of DrawTarget's API
public:
  DrawTargetType GetType() const override {
    return mCurrentTarget->GetType();
  }

  BackendType GetBackendType() const override {
    return mCurrentTarget->GetBackendType();
  }

  bool IsRecording() const override { return true; }
  bool IsCaptureDT() const override { return false; }

  already_AddRefed<SourceSurface> Snapshot() override {
    return mCurrentTarget->Snapshot();
  }

  already_AddRefed<SourceSurface> IntoLuminanceSource(LuminanceType aLuminanceType,
                                                      float aOpacity) override {
    return mCurrentTarget->IntoLuminanceSource(aLuminanceType, aOpacity);
  }

  IntSize GetSize() override {
    return mCurrentTarget->GetSize();
  }

  void Flush() override {
    mCurrentTarget->Flush();
  }

  void DrawCapturedDT(DrawTargetCapture *aCaptureDT,
                      const Matrix& aTransform) override {
    mCurrentTarget->DrawCapturedDT(aCaptureDT, aTransform);
  }

  void DrawSurface(SourceSurface *aSurface,
                   const Rect &aDest,
                   const Rect &aSource,
                   const DrawSurfaceOptions &aSurfOptions,
                   const DrawOptions &aOptions) override {
    mCurrentTarget->DrawSurface(aSurface, aDest, aSource, aSurfOptions, aOptions);
  }

  void DrawFilter(FilterNode *aNode,
                          const Rect &aSourceRect,
                          const Point &aDestPoint,
                          const DrawOptions &aOptions) override {
    mCurrentTarget->DrawFilter(aNode, aSourceRect, aDestPoint, aOptions);
  }

  void DrawSurfaceWithShadow(SourceSurface *aSurface,
                             const Point &aDest,
                             const Color &aColor,
                             const Point &aOffset,
                             Float aSigma,
                             CompositionOp aOperator) override {
    mCurrentTarget->DrawSurfaceWithShadow(aSurface, aDest, aColor, aOffset, aSigma, aOperator);
  }

  void ClearRect(const Rect &aRect) override {
    mCurrentTarget->ClearRect(aRect);
  }

  void CopySurface(SourceSurface *aSurface,
                   const IntRect &aSourceRect,
                   const IntPoint &aDestination) override {
    mCurrentTarget->CopySurface(aSurface, aSourceRect, aDestination);
  }

  void FillRect(const Rect &aRect,
                const Pattern &aPattern,
                const DrawOptions &aOptions = DrawOptions()) override {
    mCurrentTarget->FillRect(aRect, aPattern, aOptions);
  }

  void StrokeRect(const Rect &aRect,
                  const Pattern &aPattern,
                  const StrokeOptions &aStrokeOptions,
                  const DrawOptions &aOptions) override {
    mCurrentTarget->StrokeRect(aRect, aPattern, aStrokeOptions, aOptions);
  }

  void StrokeLine(const Point &aStart,
                  const Point &aEnd,
                  const Pattern &aPattern,
                  const StrokeOptions &aStrokeOptions,
                  const DrawOptions &aOptions) override {
    mCurrentTarget->StrokeLine(aStart, aEnd, aPattern, aStrokeOptions, aOptions);
  }


  void Stroke(const Path *aPath,
              const Pattern &aPattern,
              const StrokeOptions &aStrokeOptions,
              const DrawOptions &aOptions) override {
    mCurrentTarget->Stroke(aPath, aPattern, aStrokeOptions, aOptions);
  }

  void Fill(const Path *aPath,
            const Pattern &aPattern,
            const DrawOptions &aOptions) override {
    mCurrentTarget->Fill(aPath, aPattern, aOptions);
  }

  void StrokeGlyphs(ScaledFont* aFont,
                    const GlyphBuffer& aBuffer,
                    const Pattern& aPattern,
                    const StrokeOptions& aStrokeOptions,
                    const DrawOptions& aOptions,
                    const GlyphRenderingOptions* aRenderingOptions) override {
    MOZ_ASSERT(mCurrentlyDrawing == Phase::eGlyphs);
    mCurrentTarget->StrokeGlyphs(aFont, aBuffer, aPattern,
                                 aStrokeOptions, aOptions, aRenderingOptions);
  }

  void Mask(const Pattern &aSource,
                    const Pattern &aMask,
                    const DrawOptions &aOptions) override {
    return mCurrentTarget->Mask(aSource, aMask, aOptions);
  }

  void MaskSurface(const Pattern &aSource,
                   SourceSurface *aMask,
                   Point aOffset,
                   const DrawOptions &aOptions) override {
    return mCurrentTarget->MaskSurface(aSource, aMask, aOffset, aOptions);
  }

  bool Draw3DTransformedSurface(SourceSurface* aSurface,
                                const Matrix4x4& aMatrix) override {
    return mCurrentTarget->Draw3DTransformedSurface(aSurface, aMatrix);
  }

  void PushClip(const Path *aPath) override {
    mCurrentTarget->PushClip(aPath);
  }

  void PushClipRect(const Rect &aRect) override {
    mCurrentTarget->PushClipRect(aRect);
  }

  void PushDeviceSpaceClipRects(const IntRect* aRects, uint32_t aCount) override {
    mCurrentTarget->PushDeviceSpaceClipRects(aRects, aCount);
  }

  void PopClip() override {
    mCurrentTarget->PopClip();
  }

  void PushLayer(bool aOpaque, Float aOpacity,
                         SourceSurface* aMask,
                         const Matrix& aMaskTransform,
                         const IntRect& aBounds,
                         bool aCopyBackground) override {
    mCurrentTarget->PushLayer(aOpaque, aOpacity, aMask, aMaskTransform, aBounds, aCopyBackground);
  }

  void PopLayer() override {
    mCurrentTarget->PopLayer();
  }


  already_AddRefed<SourceSurface> CreateSourceSurfaceFromData(unsigned char *aData,
                                                              const IntSize &aSize,
                                                              int32_t aStride,
                                                              SurfaceFormat aFormat) const override {
    return mCurrentTarget->CreateSourceSurfaceFromData(aData, aSize, aStride, aFormat);
  }

  already_AddRefed<SourceSurface> OptimizeSourceSurface(SourceSurface *aSurface) const override {
      return mCurrentTarget->OptimizeSourceSurface(aSurface);
  }

  already_AddRefed<SourceSurface>
    CreateSourceSurfaceFromNativeSurface(const NativeSurface &aSurface) const override {
      return mCurrentTarget->CreateSourceSurfaceFromNativeSurface(aSurface);
  }

  already_AddRefed<DrawTarget>
    CreateSimilarDrawTarget(const IntSize &aSize, SurfaceFormat aFormat) const override {
      return mCurrentTarget->CreateSimilarDrawTarget(aSize, aFormat);
  }

  already_AddRefed<PathBuilder> CreatePathBuilder(FillRule aFillRule) const override {
    return mCurrentTarget->CreatePathBuilder(aFillRule);
  }

  already_AddRefed<FilterNode> CreateFilter(FilterType aType) override {
    return mCurrentTarget->CreateFilter(aType);
  }

  already_AddRefed<GradientStops>
    CreateGradientStops(GradientStop *aStops,
                        uint32_t aNumStops,
                        ExtendMode aExtendMode) const override {
      return mCurrentTarget->CreateGradientStops(aStops, aNumStops, aExtendMode);
  }

  void SetTransform(const Matrix &aTransform) override {
    mCurrentTarget->SetTransform(aTransform);
    // Need to do this to make inherited GetTransform to work
    DrawTarget::SetTransform(aTransform);
  }

  void* GetNativeSurface(NativeSurfaceType aType) override {
    return mCurrentTarget->GetNativeSurface(aType);
  }

  void DetachAllSnapshots() override { mCurrentTarget->DetachAllSnapshots(); }
};

}
}

#endif
