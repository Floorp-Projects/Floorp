/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TextDrawTarget_h
#define TextDrawTarget_h

#include "mozilla/gfx/2D.h"
#include "mozilla/layers/RenderRootStateManager.h"
#include "mozilla/layers/WebRenderLayerManager.h"
#include "mozilla/layers/WebRenderBridgeChild.h"
#include "mozilla/webrender/WebRenderAPI.h"
#include "mozilla/layers/StackingContextHelper.h"
#include "mozilla/layers/IpcResourceUpdateQueue.h"

namespace mozilla {
namespace layout {

using namespace gfx;

// This class is a fake DrawTarget, used to intercept text draw calls, while
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
class TextDrawTarget : public DrawTarget {
 public:
  explicit TextDrawTarget(wr::DisplayListBuilder& aBuilder,
                          wr::IpcResourceUpdateQueue& aResources,
                          const layers::StackingContextHelper& aSc,
                          layers::RenderRootStateManager* aManager,
                          nsDisplayItem* aItem, nsRect& aBounds,
                          bool aCallerDoesSaveRestore = false)
      : mCallerDoesSaveRestore(aCallerDoesSaveRestore), mBuilder(aBuilder) {
    Reinitialize(aResources, aSc, aManager, aItem, aBounds);
  }

  // Prevent this from being copied
  TextDrawTarget(const TextDrawTarget& src) = delete;
  TextDrawTarget& operator=(const TextDrawTarget&) = delete;

  ~TextDrawTarget() { MOZ_ASSERT(mFinished); }

  void Reinitialize(wr::IpcResourceUpdateQueue& aResources,
                    const layers::StackingContextHelper& aSc,
                    layers::RenderRootStateManager* aManager,
                    nsDisplayItem* aItem, nsRect& aBounds) {
    mResources = &aResources;
    mSc = &aSc;
    mManager = aManager;
    mHasUnsupportedFeatures = false;
    mHasShadows = false;

    SetPermitSubpixelAA(true);

    // Compute clip/bounds
    auto appUnitsPerDevPixel =
        aItem->Frame()->PresContext()->AppUnitsPerDevPixel();
    LayoutDeviceRect layoutBoundsRect =
        LayoutDeviceRect::FromAppUnits(aBounds, appUnitsPerDevPixel);
    LayoutDeviceRect layoutClipRect = layoutBoundsRect;
    mBoundsRect = wr::ToLayoutRect(layoutBoundsRect);

    // Add 1 pixel of dirty area around clip rect to allow us to paint
    // antialiased pixels beyond the measured text extents.
    layoutClipRect.Inflate(1);
    mSize = IntSize::Ceil(layoutClipRect.Width(), layoutClipRect.Height());
    mClipStack.ClearAndRetainStorage();
    mClipStack.AppendElement(layoutClipRect);

    mBackfaceVisible = !aItem->BackfaceIsHidden();

    if (!mCallerDoesSaveRestore) {
      mBuilder.Save();
    }
  }

  void FoundUnsupportedFeature() { mHasUnsupportedFeatures = true; }
  bool CheckHasUnsupportedFeatures() {
    MOZ_ASSERT(mCallerDoesSaveRestore);
#ifdef DEBUG
    MOZ_ASSERT(!mFinished);
    mFinished = true;
#endif
    return mHasUnsupportedFeatures;
  }

  bool Finish() {
    MOZ_ASSERT(!mCallerDoesSaveRestore);
#ifdef DEBUG
    mFinished = true;
#endif
    if (mHasUnsupportedFeatures) {
      mBuilder.Restore();
      return false;
    }
    mBuilder.ClearSave();
    return true;
  }

  wr::FontInstanceFlags GetWRGlyphFlags() const { return mWRGlyphFlags; }
  void SetWRGlyphFlags(wr::FontInstanceFlags aFlags) { mWRGlyphFlags = aFlags; }

  class AutoRestoreWRGlyphFlags {
   public:
    ~AutoRestoreWRGlyphFlags() {
      if (mTarget) {
        mTarget->SetWRGlyphFlags(mFlags);
      }
    }

    void Save(TextDrawTarget* aTarget) {
      // This allows for recursive saves, in case the flags need to be modified
      // under multiple conditions (i.e. transforms and synthetic italics),
      // since the flags will be restored to the first saved value in the
      // destructor on scope exit.
      if (!mTarget) {
        // Only record the first save with the original flags that will be
        // restored.
        mTarget = aTarget;
        mFlags = aTarget->GetWRGlyphFlags();
      } else {
        // Ensure that this is actually a recursive save to the same target
        MOZ_ASSERT(
            mTarget == aTarget,
            "Recursive save of WR glyph flags to different TextDrawTargets");
      }
    }

   private:
    TextDrawTarget* mTarget = nullptr;
    wr::FontInstanceFlags mFlags = {0};
  };

  // This overload just stores the glyphs/font/color.
  void FillGlyphs(ScaledFont* aFont, const GlyphBuffer& aBuffer,
                  const Pattern& aPattern,
                  const DrawOptions& aOptions) override {
    // Make sure we're only given boring color patterns
    MOZ_RELEASE_ASSERT(aOptions.mCompositionOp == CompositionOp::OP_OVER);
    MOZ_RELEASE_ASSERT(aOptions.mAlpha == 1.0f);
    MOZ_RELEASE_ASSERT(aPattern.GetType() == PatternType::COLOR);

    // Make sure the font exists, and can be serialized
    MOZ_RELEASE_ASSERT(aFont);
    if (!aFont->CanSerialize()) {
      FoundUnsupportedFeature();
      return;
    }

    auto* colorPat = static_cast<const ColorPattern*>(&aPattern);
    auto color = wr::ToColorF(colorPat->mColor);
    MOZ_ASSERT(aBuffer.mNumGlyphs);
    auto glyphs = Range<const wr::GlyphInstance>(
        reinterpret_cast<const wr::GlyphInstance*>(aBuffer.mGlyphs),
        aBuffer.mNumGlyphs);
    // MSVC won't let us use offsetof on the following directly so we give it a
    // name with typedef
    typedef std::remove_reference<decltype(aBuffer.mGlyphs[0])>::type GlyphType;
    // Compare gfx::Glyph and wr::GlyphInstance to make sure that they are
    // structurally equivalent to ensure that our cast above was ok
    static_assert(
        std::is_same<decltype(aBuffer.mGlyphs[0].mIndex),
                     decltype(glyphs[0].index)>() &&
            std::is_same<decltype(aBuffer.mGlyphs[0].mPosition.x.value),
                         decltype(glyphs[0].point.x)>() &&
            std::is_same<decltype(aBuffer.mGlyphs[0].mPosition.y.value),
                         decltype(glyphs[0].point.y)>() &&
            offsetof(GlyphType, mIndex) == offsetof(wr::GlyphInstance, index) &&
            offsetof(GlyphType, mPosition) ==
                offsetof(wr::GlyphInstance, point) &&
            offsetof(decltype(aBuffer.mGlyphs[0].mPosition), x) ==
                offsetof(decltype(glyphs[0].point), x) &&
            offsetof(decltype(aBuffer.mGlyphs[0].mPosition), y) ==
                offsetof(decltype(glyphs[0].point), y) &&
            std::is_standard_layout<
                std::remove_reference<decltype(aBuffer.mGlyphs[0])>>::value &&
            std::is_standard_layout<
                std::remove_reference<decltype(glyphs[0])>>::value &&
            sizeof(aBuffer.mGlyphs[0]) == sizeof(glyphs[0]) &&
            sizeof(aBuffer.mGlyphs[0].mPosition) == sizeof(glyphs[0].point),
        "glyph buf types don't match");

    wr::GlyphOptions glyphOptions;
    glyphOptions.render_mode =
        wr::ToFontRenderMode(aOptions.mAntialiasMode, GetPermitSubpixelAA());
    glyphOptions.flags = mWRGlyphFlags;

    mManager->WrBridge()->PushGlyphs(mBuilder, *mResources, glyphs, aFont,
                                     color, *mSc, mBoundsRect, ClipRect(),
                                     mBackfaceVisible, &glyphOptions);
  }

  void PushClipRect(const Rect& aRect) override {
    LayoutDeviceRect rect = LayoutDeviceRect::FromUnknownRect(aRect);
    rect = rect.Intersect(mClipStack.LastElement());
    mClipStack.AppendElement(rect);
  }

  void PopClip() override { mClipStack.RemoveLastElement(); }

  IntSize GetSize() const override { return mSize; }

  void AppendShadow(const wr::Shadow& aShadow, bool aInflate) {
    mBuilder.PushShadow(mBoundsRect, ClipRect(), mBackfaceVisible, aShadow,
                        aInflate);
    mHasShadows = true;
  }

  void TerminateShadows() {
    if (mHasShadows) {
      mBuilder.PopAllShadows();
      mHasShadows = false;
    }
  }

  void AppendSelectionRect(const LayoutDeviceRect& aRect,
                           const DeviceColor& aColor) {
    auto rect = wr::ToLayoutRect(aRect);
    auto color = wr::ToColorF(aColor);
    mBuilder.PushRect(rect, ClipRect(), mBackfaceVisible, false, false, color);
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
  //
  // NOTE: we assume the points are axis-aligned, and aStart should be used
  // as the top-left corner of the rect.
  void AppendDecoration(const Point& aStart, const Point& aEnd,
                        const float aThickness, const bool aVertical,
                        const DeviceColor& aColor,
                        const StyleTextDecorationStyle aStyle) {
    auto pos = LayoutDevicePoint::FromUnknownPoint(aStart);
    LayoutDeviceSize size;

    if (aVertical) {
      pos.x -= aThickness / 2;  // adjust from center to corner
      size = LayoutDeviceSize(aThickness,
                              ViewAs<LayoutDevicePixel>(aEnd.y - aStart.y));
    } else {
      pos.y -= aThickness / 2;  // adjust from center to corner
      size = LayoutDeviceSize(ViewAs<LayoutDevicePixel>(aEnd.x - aStart.x),
                              aThickness);
    }

    wr::Line decoration;
    decoration.bounds = wr::ToLayoutRect(LayoutDeviceRect(pos, size));
    decoration.wavyLineThickness = 0;  // dummy value, unused
    decoration.color = wr::ToColorF(aColor);
    decoration.orientation = aVertical ? wr::LineOrientation::Vertical
                                       : wr::LineOrientation::Horizontal;

    switch (aStyle) {
      case StyleTextDecorationStyle::Solid:
        decoration.style = wr::LineStyle::Solid;
        break;
      case StyleTextDecorationStyle::Dotted:
        decoration.style = wr::LineStyle::Dotted;
        break;
      case StyleTextDecorationStyle::Dashed:
        decoration.style = wr::LineStyle::Dashed;
        break;
      // Wavy lines should go through AppendWavyDecoration
      case StyleTextDecorationStyle::Wavy:
      // Double lines should be lowered to two solid lines
      case StyleTextDecorationStyle::Double:
      default:
        MOZ_CRASH("TextDrawTarget received unsupported line style");
    }

    mBuilder.PushLine(ClipRect(), mBackfaceVisible, decoration);
  }

  // Seperated out from AppendDecoration because Wavy Lines are completely
  // different, and trying to merge the concept is more of a mess than it's
  // worth.
  void AppendWavyDecoration(const Rect& aBounds, const float aThickness,
                            const bool aVertical, const DeviceColor& aColor) {
    wr::Line decoration;

    decoration.bounds =
        wr::ToLayoutRect(LayoutDeviceRect::FromUnknownRect(aBounds));
    decoration.wavyLineThickness = aThickness;
    decoration.color = wr::ToColorF(aColor);
    decoration.orientation = aVertical ? wr::LineOrientation::Vertical
                                       : wr::LineOrientation::Horizontal;
    decoration.style = wr::LineStyle::Wavy;

    mBuilder.PushLine(ClipRect(), mBackfaceVisible, decoration);
  }

  layers::WebRenderBridgeChild* WrBridge() { return mManager->WrBridge(); }
  layers::WebRenderLayerManager* WrLayerManager() {
    return mManager->LayerManager();
  }

  Maybe<wr::ImageKey> DefineImage(const IntSize& aSize, uint32_t aStride,
                                  SurfaceFormat aFormat, const uint8_t* aData) {
    wr::ImageKey key = mManager->WrBridge()->GetNextImageKey();
    wr::ImageDescriptor desc(aSize, aStride, aFormat);
    Range<uint8_t> bytes(const_cast<uint8_t*>(aData), aStride * aSize.height);
    if (mResources->AddImage(key, desc, bytes)) {
      return Some(key);
    }
    return Nothing();
  }

  void PushImage(wr::ImageKey aKey, const Rect& aBounds, const Rect& aClip,
                 wr::ImageRendering aFilter, const wr::ColorF& aColor) {
    if (!aClip.Intersects(GeckoClipRect().ToUnknownRect())) {
      return;
    }
    mBuilder.PushImage(wr::ToLayoutRect(aBounds), wr::ToLayoutRect(aClip), true,
                       false, aFilter, aKey, true, aColor);
  }

  LayoutDeviceRect GeckoClipRect() { return mClipStack.LastElement(); }

 private:
  wr::LayoutRect ClipRect() {
    return wr::ToLayoutRect(mClipStack.LastElement());
  }
  // Whether anything unsupported was encountered. This will result in this
  // text being emitted as a blob, which means subpixel-AA can't be used and
  // that performance will probably be a bit worse. At this point, we've
  // properly implemented everything that shows up a lot, so you can assume
  // that the remaining things we don't implement are fairly rare. The complete
  // set of things that we don't implement are as follows:
  //
  // * Unserializable Fonts: WR lives across an IPC boundary
  // * Text-Combine-Upright Squishing: no one's really bothered to impl it yet
  // * Text-Stroke: not a real standard (exists for webcompat)
  // * SVG Glyphs: not a real standard (we got overzealous with svg)
  // * Color Glyphs (Emoji) With Transparency: requires us to apply transparency
  //   with a composited layer (a single emoji can be many single-color glyphs)
  //
  // The transparent colored-glyphs issue is probably the most valuable to fix,
  // since ideally it would also result in us fixing transparency for all
  // intersecting glyphs (which currently look bad with or without webrender,
  // so there's no fallback like with emoji). Specifically, transparency
  // looks bad for "cursive" fonts where glyphs overlap at the seams. Since
  // this is more common for non-latin scripts (e.g. मनीष), this amounts to us
  // treating non-latin scripts poorly... unless they're emoji. Yikes!
  bool mHasUnsupportedFeatures = false;

  // The caller promises to call Save/Restore on the builder as needed.
  bool mCallerDoesSaveRestore = false;
#ifdef DEBUG
  bool mFinished = false;
#endif

  // Whether PopAllShadows needs to be called
  bool mHasShadows = false;

  // Things used to push to webrender
  wr::DisplayListBuilder& mBuilder;
  wr::IpcResourceUpdateQueue* mResources;
  const layers::StackingContextHelper* mSc;
  layers::RenderRootStateManager* mManager;

  // Computed facts
  IntSize mSize;
  wr::LayoutRect mBoundsRect;
  AutoTArray<LayoutDeviceRect, 3> mClipStack;
  bool mBackfaceVisible;

  wr::FontInstanceFlags mWRGlyphFlags = {0};

  // The rest of this is dummy implementations of DrawTarget's API
 public:
  DrawTargetType GetType() const override {
    return DrawTargetType::SOFTWARE_RASTER;
  }

  BackendType GetBackendType() const override {
    return BackendType::WEBRENDER_TEXT;
  }

  bool IsRecording() const override { return true; }

  already_AddRefed<SourceSurface> Snapshot() override {
    MOZ_CRASH("TextDrawTarget: Method shouldn't be called");
    return nullptr;
  }

  already_AddRefed<SourceSurface> IntoLuminanceSource(
      LuminanceType aLuminanceType, float aOpacity) override {
    MOZ_CRASH("TextDrawTarget: Method shouldn't be called");
    return nullptr;
  }

  void Flush() override {
    MOZ_CRASH("TextDrawTarget: Method shouldn't be called");
  }

  void DrawSurface(SourceSurface* aSurface, const Rect& aDest,
                   const Rect& aSource, const DrawSurfaceOptions& aSurfOptions,
                   const DrawOptions& aOptions) override {
    MOZ_CRASH("TextDrawTarget: Method shouldn't be called");
  }

  void DrawFilter(FilterNode* aNode, const Rect& aSourceRect,
                  const Point& aDestPoint,
                  const DrawOptions& aOptions) override {
    MOZ_CRASH("TextDrawTarget: Method shouldn't be called");
  }

  void DrawSurfaceWithShadow(SourceSurface* aSurface, const Point& aDest,
                             const ShadowOptions& aShadow,
                             CompositionOp aOperator) override {
    MOZ_CRASH("TextDrawTarget: Method shouldn't be called");
  }

  void ClearRect(const Rect& aRect) override {
    MOZ_CRASH("TextDrawTarget: Method shouldn't be called");
  }

  void CopySurface(SourceSurface* aSurface, const IntRect& aSourceRect,
                   const IntPoint& aDestination) override {
    MOZ_CRASH("TextDrawTarget: Method shouldn't be called");
  }

  void FillRect(const Rect& aRect, const Pattern& aPattern,
                const DrawOptions& aOptions = DrawOptions()) override {
    MOZ_RELEASE_ASSERT(aPattern.GetType() == PatternType::COLOR);

    if (!aRect.Intersects(GeckoClipRect().ToUnknownRect())) {
      return;
    }
    auto rect = wr::ToLayoutRect(LayoutDeviceRect::FromUnknownRect(aRect));
    auto color =
        wr::ToColorF(static_cast<const ColorPattern&>(aPattern).mColor);
    mBuilder.PushRect(rect, ClipRect(), mBackfaceVisible, false, false, color);
  }

  void StrokeRect(const Rect& aRect, const Pattern& aPattern,
                  const StrokeOptions& aStrokeOptions,
                  const DrawOptions& aOptions) override {
    MOZ_RELEASE_ASSERT(aPattern.GetType() == PatternType::COLOR &&
                       aStrokeOptions.mDashLength == 0);

    wr::LayoutSideOffsets widths = {
        aStrokeOptions.mLineWidth, aStrokeOptions.mLineWidth,
        aStrokeOptions.mLineWidth, aStrokeOptions.mLineWidth};
    wr::ColorF color =
        wr::ToColorF(static_cast<const ColorPattern&>(aPattern).mColor);
    wr::BorderSide sides[4] = {{color, wr::BorderStyle::Solid},
                               {color, wr::BorderStyle::Solid},
                               {color, wr::BorderStyle::Solid},
                               {color, wr::BorderStyle::Solid}};
    wr::BorderRadius radius = {{0, 0}, {0, 0}, {0, 0}, {0, 0}};
    LayoutDeviceRect rect = LayoutDeviceRect::FromUnknownRect(aRect);
    rect.Inflate(aStrokeOptions.mLineWidth / 2);
    if (!rect.Intersects(GeckoClipRect())) {
      return;
    }
    wr::LayoutRect bounds = wr::ToLayoutRect(rect);
    mBuilder.PushBorder(bounds, ClipRect(), true, widths,
                        Range<const wr::BorderSide>(sides, 4), radius);
  }

  void StrokeLine(const Point& aStart, const Point& aEnd,
                  const Pattern& aPattern, const StrokeOptions& aStrokeOptions,
                  const DrawOptions& aOptions) override {
    MOZ_CRASH("TextDrawTarget: Method shouldn't be called");
  }

  void Stroke(const Path* aPath, const Pattern& aPattern,
              const StrokeOptions& aStrokeOptions,
              const DrawOptions& aOptions) override {
    MOZ_CRASH("TextDrawTarget: Method shouldn't be called");
  }

  void Fill(const Path* aPath, const Pattern& aPattern,
            const DrawOptions& aOptions) override {
    MOZ_CRASH("TextDrawTarget: Method shouldn't be called");
  }

  void StrokeGlyphs(ScaledFont* aFont, const GlyphBuffer& aBuffer,
                    const Pattern& aPattern,
                    const StrokeOptions& aStrokeOptions,
                    const DrawOptions& aOptions) override {
    MOZ_CRASH("TextDrawTarget: Method shouldn't be called");
  }

  void Mask(const Pattern& aSource, const Pattern& aMask,
            const DrawOptions& aOptions) override {
    MOZ_CRASH("TextDrawTarget: Method shouldn't be called");
  }

  void MaskSurface(const Pattern& aSource, SourceSurface* aMask, Point aOffset,
                   const DrawOptions& aOptions) override {
    MOZ_CRASH("TextDrawTarget: Method shouldn't be called");
  }

  bool Draw3DTransformedSurface(SourceSurface* aSurface,
                                const Matrix4x4& aMatrix) override {
    MOZ_CRASH("TextDrawTarget: Method shouldn't be called");
  }

  void PushClip(const Path* aPath) override {
    MOZ_CRASH("TextDrawTarget: Method shouldn't be called");
  }

  void PushDeviceSpaceClipRects(const IntRect* aRects,
                                uint32_t aCount) override {
    MOZ_CRASH("TextDrawTarget: Method shouldn't be called");
  }

  void PushLayer(bool aOpaque, Float aOpacity, SourceSurface* aMask,
                 const Matrix& aMaskTransform, const IntRect& aBounds,
                 bool aCopyBackground) override {
    // Fine to pretend we do this
  }

  void PopLayer() override {
    // Fine to pretend we do this
  }

  already_AddRefed<SourceSurface> CreateSourceSurfaceFromData(
      unsigned char* aData, const IntSize& aSize, int32_t aStride,
      SurfaceFormat aFormat) const override {
    MOZ_CRASH("TextDrawTarget: Method shouldn't be called");
    return nullptr;
  }

  already_AddRefed<SourceSurface> OptimizeSourceSurface(
      SourceSurface* aSurface) const override {
    MOZ_CRASH("TextDrawTarget: Method shouldn't be called");
    return nullptr;
  }

  already_AddRefed<SourceSurface> CreateSourceSurfaceFromNativeSurface(
      const NativeSurface& aSurface) const override {
    MOZ_CRASH("TextDrawTarget: Method shouldn't be called");
    return nullptr;
  }

  already_AddRefed<DrawTarget> CreateSimilarDrawTarget(
      const IntSize& aSize, SurfaceFormat aFormat) const override {
    MOZ_CRASH("TextDrawTarget: Method shouldn't be called");
    return nullptr;
  }

  bool CanCreateSimilarDrawTarget(const IntSize& aSize,
                                  SurfaceFormat aFormat) const override {
    MOZ_CRASH("TextDrawTarget: Method shouldn't be called");
    return false;
  }

  virtual RefPtr<DrawTarget> CreateClippedDrawTarget(
      const Rect& aBounds, SurfaceFormat aFormat) override {
    MOZ_CRASH("TextDrawTarget: Method shouldn't be called");
    return nullptr;
  }

  already_AddRefed<PathBuilder> CreatePathBuilder(
      FillRule aFillRule) const override {
    MOZ_CRASH("TextDrawTarget: Method shouldn't be called");
    return nullptr;
  }

  already_AddRefed<FilterNode> CreateFilter(FilterType aType) override {
    MOZ_CRASH("TextDrawTarget: Method shouldn't be called");
    return nullptr;
  }

  already_AddRefed<GradientStops> CreateGradientStops(
      GradientStop* aStops, uint32_t aNumStops,
      ExtendMode aExtendMode) const override {
    MOZ_CRASH("TextDrawTarget: Method shouldn't be called");
    return nullptr;
  }

  void DetachAllSnapshots() override {
    MOZ_CRASH("TextDrawTarget: Method shouldn't be called");
  }
};

}  // namespace layout
}  // namespace mozilla

#endif
