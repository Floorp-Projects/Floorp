/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CanvasRenderingContext2D.h"

#include "mozilla/gfx/Helpers.h"
#include "nsXULElement.h"

#include "nsMathUtils.h"
#include "SVGImageContext.h"

#include "nsContentUtils.h"

#include "mozilla/PresShell.h"
#include "mozilla/PresShellInlines.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/HTMLCanvasElement.h"
#include "SVGObserverUtils.h"
#include "nsPresContext.h"

#include "nsIInterfaceRequestorUtils.h"
#include "nsIFrame.h"
#include "nsError.h"

#include "nsCSSPseudoElements.h"
#include "nsComputedDOMStyle.h"

#include "nsPrintfCString.h"

#include "nsReadableUtils.h"

#include "nsColor.h"
#include "nsGfxCIID.h"
#include "nsIDocShell.h"
#include "nsPIDOMWindow.h"
#include "nsDisplayList.h"
#include "nsFocusManager.h"
#include "nsContentUtils.h"

#include "nsTArray.h"

#include "ImageEncoder.h"
#include "ImageRegion.h"

#include "gfxContext.h"
#include "gfxPlatform.h"
#include "gfxFont.h"
#include "gfxBlur.h"
#include "gfxTextRun.h"
#include "gfxUtils.h"

#include "nsFrameLoader.h"
#include "nsBidiPresUtils.h"
#include "Layers.h"
#include "LayerUserData.h"
#include "CanvasUtils.h"
#include "nsIMemoryReporter.h"
#include "nsStyleUtil.h"
#include "CanvasImageCache.h"

#include <algorithm>

#include "jsapi.h"
#include "jsfriendapi.h"
#include "js/Array.h"  // JS::GetArrayLength
#include "js/Conversions.h"
#include "js/HeapAPI.h"
#include "js/Warnings.h"  // JS::WarnASCII

#include "mozilla/Alignment.h"
#include "mozilla/Assertions.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/dom/DOMMatrix.h"
#include "mozilla/dom/ImageBitmap.h"
#include "mozilla/dom/ImageData.h"
#include "mozilla/dom/PBrowserParent.h"
#include "mozilla/dom/ToJSValue.h"
#include "mozilla/dom/TypedArray.h"
#include "mozilla/EndianUtils.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/Helpers.h"
#include "mozilla/gfx/Tools.h"
#include "mozilla/gfx/PathHelpers.h"
#include "mozilla/gfx/DataSurfaceHelpers.h"
#include "mozilla/gfx/PatternHelpers.h"
#include "mozilla/gfx/Swizzle.h"
#include "mozilla/layers/PersistentBufferProvider.h"
#include "mozilla/MathAlgorithms.h"
#include "mozilla/Preferences.h"
#include "mozilla/ServoBindings.h"
#include "mozilla/StaticPrefs_gfx.h"
#include "mozilla/Telemetry.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Unused.h"
#include "nsCCUncollectableMarker.h"
#include "nsWrapperCacheInlines.h"
#include "mozilla/dom/CanvasRenderingContext2DBinding.h"
#include "mozilla/dom/CanvasPath.h"
#include "mozilla/dom/HTMLImageElement.h"
#include "mozilla/dom/HTMLVideoElement.h"
#include "mozilla/dom/SVGImageElement.h"
#include "mozilla/dom/SVGMatrix.h"
#include "mozilla/dom/TextMetrics.h"
#include "mozilla/dom/SVGMatrix.h"
#include "mozilla/FloatingPoint.h"
#include "nsGlobalWindow.h"
#include "nsFilterInstance.h"
#include "nsDeviceContext.h"
#include "nsFontMetrics.h"
#include "Units.h"
#include "CanvasUtils.h"
#include "mozilla/CycleCollectedJSRuntime.h"
#include "mozilla/ServoCSSParser.h"
#include "mozilla/ServoStyleSet.h"
#include "mozilla/SVGContentUtils.h"
#include "mozilla/layers/CanvasClient.h"
#include "mozilla/layers/WebRenderUserData.h"
#include "mozilla/layers/WebRenderCanvasRenderer.h"

#undef free  // apparently defined by some windows header, clashing with a
             // free() method in SkTypes.h

#ifdef XP_WIN
#  include "gfxWindowsPlatform.h"
#endif

// windows.h (included by chromium code) defines this, in its infinite wisdom
#undef DrawText

using namespace mozilla;
using namespace mozilla::CanvasUtils;
using namespace mozilla::css;
using namespace mozilla::gfx;
using namespace mozilla::image;
using namespace mozilla::ipc;
using namespace mozilla::layers;

namespace mozilla {
namespace dom {

// Cap sigma to avoid overly large temp surfaces.
const Float SIGMA_MAX = 100;

const size_t MAX_STYLE_STACK_SIZE = 1024;

/* Memory reporter stuff */
static int64_t gCanvasAzureMemoryUsed = 0;

// Adds Save() / Restore() calls to the scope.
class MOZ_RAII AutoSaveRestore {
 public:
  explicit AutoSaveRestore(
      CanvasRenderingContext2D* aCtx MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
      : mCtx(aCtx) {
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    mCtx->Save();
  }
  ~AutoSaveRestore() { mCtx->Restore(); }

 private:
  RefPtr<CanvasRenderingContext2D> mCtx;
  MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

// This is KIND_OTHER because it's not always clear where in memory the pixels
// of a canvas are stored.  Furthermore, this memory will be tracked by the
// underlying surface implementations.  See bug 655638 for details.
class Canvas2dPixelsReporter final : public nsIMemoryReporter {
  ~Canvas2dPixelsReporter() = default;

 public:
  NS_DECL_ISUPPORTS

  NS_IMETHOD CollectReports(nsIHandleReportCallback* aHandleReport,
                            nsISupports* aData, bool aAnonymize) override {
    MOZ_COLLECT_REPORT("canvas-2d-pixels", KIND_OTHER, UNITS_BYTES,
                       gCanvasAzureMemoryUsed,
                       "Memory used by 2D canvases. Each canvas requires "
                       "(width * height * 4) bytes.");

    return NS_OK;
  }
};

NS_IMPL_ISUPPORTS(Canvas2dPixelsReporter, nsIMemoryReporter)

class CanvasRadialGradient : public CanvasGradient {
 public:
  CanvasRadialGradient(CanvasRenderingContext2D* aContext,
                       const Point& aBeginOrigin, Float aBeginRadius,
                       const Point& aEndOrigin, Float aEndRadius)
      : CanvasGradient(aContext, Type::RADIAL),
        mCenter1(aBeginOrigin),
        mCenter2(aEndOrigin),
        mRadius1(aBeginRadius),
        mRadius2(aEndRadius) {}

  Point mCenter1;
  Point mCenter2;
  Float mRadius1;
  Float mRadius2;
};

class CanvasLinearGradient : public CanvasGradient {
 public:
  CanvasLinearGradient(CanvasRenderingContext2D* aContext, const Point& aBegin,
                       const Point& aEnd)
      : CanvasGradient(aContext, Type::LINEAR), mBegin(aBegin), mEnd(aEnd) {}

 protected:
  friend struct CanvasBidiProcessor;
  friend class CanvasGeneralPattern;

  // Beginning of linear gradient.
  Point mBegin;
  // End of linear gradient.
  Point mEnd;
};

bool CanvasRenderingContext2D::PatternIsOpaque(
    CanvasRenderingContext2D::Style aStyle, bool* aIsColor) const {
  const ContextState& state = CurrentState();
  bool opaque = false;
  bool color = false;
  if (state.globalAlpha >= 1.0) {
    if (state.patternStyles[aStyle] && state.patternStyles[aStyle]->mSurface) {
      opaque = IsOpaque(state.patternStyles[aStyle]->mSurface->GetFormat());
    } else if (!state.gradientStyles[aStyle]) {
      // TODO: for gradient patterns we could check that all stops are opaque
      // colors.
      // it's a color pattern.
      opaque = sRGBColor::FromABGR(state.colorStyles[aStyle]).a >= 1.0;
      color = true;
    }
  }
  if (aIsColor) {
    *aIsColor = color;
  }
  return opaque;
}

// This class is named 'GeneralCanvasPattern' instead of just
// 'GeneralPattern' to keep Windows PGO builds from confusing the
// GeneralPattern class in gfxContext.cpp with this one.
class CanvasGeneralPattern {
 public:
  typedef CanvasRenderingContext2D::Style Style;
  typedef CanvasRenderingContext2D::ContextState ContextState;

  Pattern& ForStyle(CanvasRenderingContext2D* aCtx, Style aStyle,
                    DrawTarget* aRT) {
    // This should only be called once or the mPattern destructor will
    // not be executed.
    NS_ASSERTION(
        !mPattern.GetPattern(),
        "ForStyle() should only be called once on CanvasGeneralPattern!");

    const ContextState& state = aCtx->CurrentState();

    if (state.StyleIsColor(aStyle)) {
      mPattern.InitColorPattern(ToDeviceColor(state.colorStyles[aStyle]));
    } else if (state.gradientStyles[aStyle] &&
               state.gradientStyles[aStyle]->GetType() ==
                   CanvasGradient::Type::LINEAR) {
      auto gradient = static_cast<CanvasLinearGradient*>(
          state.gradientStyles[aStyle].get());

      mPattern.InitLinearGradientPattern(
          gradient->mBegin, gradient->mEnd,
          gradient->GetGradientStopsForTarget(aRT));
    } else if (state.gradientStyles[aStyle] &&
               state.gradientStyles[aStyle]->GetType() ==
                   CanvasGradient::Type::RADIAL) {
      auto gradient = static_cast<CanvasRadialGradient*>(
          state.gradientStyles[aStyle].get());

      mPattern.InitRadialGradientPattern(
          gradient->mCenter1, gradient->mCenter2, gradient->mRadius1,
          gradient->mRadius2, gradient->GetGradientStopsForTarget(aRT));
    } else if (state.patternStyles[aStyle]) {
      if (aCtx->mCanvasElement) {
        CanvasUtils::DoDrawImageSecurityCheck(
            aCtx->mCanvasElement, state.patternStyles[aStyle]->mPrincipal,
            state.patternStyles[aStyle]->mForceWriteOnly,
            state.patternStyles[aStyle]->mCORSUsed);
      }

      ExtendMode mode;
      if (state.patternStyles[aStyle]->mRepeat ==
          CanvasPattern::RepeatMode::NOREPEAT) {
        mode = ExtendMode::CLAMP;
      } else {
        mode = ExtendMode::REPEAT;
      }

      SamplingFilter samplingFilter;
      if (state.imageSmoothingEnabled) {
        samplingFilter = SamplingFilter::GOOD;
      } else {
        samplingFilter = SamplingFilter::POINT;
      }

      mPattern.InitSurfacePattern(state.patternStyles[aStyle]->mSurface, mode,
                                  state.patternStyles[aStyle]->mTransform,
                                  samplingFilter);
    }

    return *mPattern.GetPattern();
  }

  GeneralPattern mPattern;
};

/* This is an RAII based class that can be used as a drawtarget for
 * operations that need to have a filter applied to their results.
 * All coordinates passed to the constructor are in device space.
 */
class AdjustedTargetForFilter {
 public:
  typedef CanvasRenderingContext2D::ContextState ContextState;

  AdjustedTargetForFilter(CanvasRenderingContext2D* aCtx,
                          DrawTarget* aFinalTarget,
                          const gfx::IntPoint& aFilterSpaceToTargetOffset,
                          const gfx::IntRect& aPreFilterBounds,
                          const gfx::IntRect& aPostFilterBounds,
                          gfx::CompositionOp aCompositionOp)
      : mFinalTarget(aFinalTarget),
        mCtx(aCtx),
        mPostFilterBounds(aPostFilterBounds),
        mOffset(aFilterSpaceToTargetOffset),
        mCompositionOp(aCompositionOp) {
    nsIntRegion sourceGraphicNeededRegion;
    nsIntRegion fillPaintNeededRegion;
    nsIntRegion strokePaintNeededRegion;

    FilterSupport::ComputeSourceNeededRegions(
        aCtx->CurrentState().filter, mPostFilterBounds,
        sourceGraphicNeededRegion, fillPaintNeededRegion,
        strokePaintNeededRegion);

    mSourceGraphicRect = sourceGraphicNeededRegion.GetBounds();
    mFillPaintRect = fillPaintNeededRegion.GetBounds();
    mStrokePaintRect = strokePaintNeededRegion.GetBounds();

    mSourceGraphicRect = mSourceGraphicRect.Intersect(aPreFilterBounds);

    if (mSourceGraphicRect.IsEmpty()) {
      // The filter might not make any use of the source graphic. We need to
      // create a DrawTarget that we can return from DT() anyway, so we'll
      // just use a 1x1-sized one.
      mSourceGraphicRect.SizeTo(1, 1);
    }

    if (!mFinalTarget->CanCreateSimilarDrawTarget(mSourceGraphicRect.Size(),
                                                  SurfaceFormat::B8G8R8A8)) {
      mTarget = mFinalTarget;
      mCtx = nullptr;
      mFinalTarget = nullptr;
      return;
    }

    mTarget = mFinalTarget->CreateSimilarDrawTarget(mSourceGraphicRect.Size(),
                                                    SurfaceFormat::B8G8R8A8);

    if (mTarget) {
      // See bug 1524554.
      mTarget->ClearRect(gfx::Rect());
    }

    if (!mTarget || !mTarget->IsValid()) {
      // XXX - Deal with the situation where our temp size is too big to
      // fit in a texture (bug 1066622).
      mTarget = mFinalTarget;
      mCtx = nullptr;
      mFinalTarget = nullptr;
      return;
    }

    mTarget->SetTransform(mFinalTarget->GetTransform().PostTranslate(
        -mSourceGraphicRect.TopLeft() + mOffset));
  }

  // Return a SourceSurface that contains the FillPaint or StrokePaint source.
  already_AddRefed<SourceSurface> DoSourcePaint(
      gfx::IntRect& aRect, CanvasRenderingContext2D::Style aStyle) {
    if (aRect.IsEmpty()) {
      return nullptr;
    }

    RefPtr<DrawTarget> dt = mFinalTarget->CreateSimilarDrawTarget(
        aRect.Size(), SurfaceFormat::B8G8R8A8);

    if (dt) {
      // See bug 1524554.
      dt->ClearRect(gfx::Rect());
    }

    if (!dt || !dt->IsValid()) {
      aRect.SetEmpty();
      return nullptr;
    }

    Matrix transform =
        mFinalTarget->GetTransform().PostTranslate(-aRect.TopLeft() + mOffset);

    dt->SetTransform(transform);

    if (transform.Invert()) {
      gfx::Rect dtBounds(0, 0, aRect.width, aRect.height);
      gfx::Rect fillRect = transform.TransformBounds(dtBounds);
      dt->FillRect(fillRect, CanvasGeneralPattern().ForStyle(mCtx, aStyle, dt));
    }
    return dt->Snapshot();
  }

  ~AdjustedTargetForFilter() {
    if (!mCtx) {
      return;
    }

    RefPtr<SourceSurface> snapshot = mTarget->Snapshot();

    RefPtr<SourceSurface> fillPaint =
        DoSourcePaint(mFillPaintRect, CanvasRenderingContext2D::Style::FILL);
    RefPtr<SourceSurface> strokePaint = DoSourcePaint(
        mStrokePaintRect, CanvasRenderingContext2D::Style::STROKE);

    AutoRestoreTransform autoRestoreTransform(mFinalTarget);
    mFinalTarget->SetTransform(Matrix());

    MOZ_RELEASE_ASSERT(!mCtx->CurrentState().filter.mPrimitives.IsEmpty());
    gfx::FilterSupport::RenderFilterDescription(
        mFinalTarget, mCtx->CurrentState().filter, gfx::Rect(mPostFilterBounds),
        snapshot, mSourceGraphicRect, fillPaint, mFillPaintRect, strokePaint,
        mStrokePaintRect, mCtx->CurrentState().filterAdditionalImages,
        mPostFilterBounds.TopLeft() - mOffset,
        DrawOptions(1.0f, mCompositionOp));

    const gfx::FilterDescription& filter = mCtx->CurrentState().filter;
    MOZ_RELEASE_ASSERT(!filter.mPrimitives.IsEmpty());
    if (filter.mPrimitives.LastElement().IsTainted() && mCtx->mCanvasElement) {
      mCtx->mCanvasElement->SetWriteOnly();
    }
  }

  DrawTarget* DT() { return mTarget; }

 private:
  RefPtr<DrawTarget> mTarget;
  RefPtr<DrawTarget> mFinalTarget;
  CanvasRenderingContext2D* mCtx;
  gfx::IntRect mSourceGraphicRect;
  gfx::IntRect mFillPaintRect;
  gfx::IntRect mStrokePaintRect;
  gfx::IntRect mPostFilterBounds;
  gfx::IntPoint mOffset;
  gfx::CompositionOp mCompositionOp;
};

/* This is an RAII based class that can be used as a drawtarget for
 * operations that need to have a shadow applied to their results.
 * All coordinates passed to the constructor are in device space.
 */
class AdjustedTargetForShadow {
 public:
  typedef CanvasRenderingContext2D::ContextState ContextState;

  AdjustedTargetForShadow(CanvasRenderingContext2D* aCtx,
                          DrawTarget* aFinalTarget, const gfx::Rect& aBounds,
                          gfx::CompositionOp aCompositionOp)
      : mFinalTarget(aFinalTarget), mCtx(aCtx), mCompositionOp(aCompositionOp) {
    const ContextState& state = mCtx->CurrentState();
    mSigma = state.ShadowBlurSigma();

    // We actually include the bounds of the shadow blur, this makes it
    // easier to execute the actual blur on hardware, and shouldn't affect
    // the amount of pixels that need to be touched.
    gfx::Rect bounds = aBounds;
    int32_t blurRadius = state.ShadowBlurRadius();
    bounds.Inflate(blurRadius);
    bounds.RoundOut();
    bounds.ToIntRect(&mTempRect);

    if (!mFinalTarget->CanCreateSimilarDrawTarget(mTempRect.Size(),
                                                  SurfaceFormat::B8G8R8A8)) {
      mTarget = mFinalTarget;
      mCtx = nullptr;
      mFinalTarget = nullptr;
      return;
    }

    mTarget = mFinalTarget->CreateShadowDrawTarget(
        mTempRect.Size(), SurfaceFormat::B8G8R8A8, mSigma);

    if (mTarget) {
      // See bug 1524554.
      mTarget->ClearRect(gfx::Rect());
    }

    if (!mTarget || !mTarget->IsValid()) {
      // XXX - Deal with the situation where our temp size is too big to
      // fit in a texture (bug 1066622).
      mTarget = mFinalTarget;
      mCtx = nullptr;
      mFinalTarget = nullptr;
    } else {
      mTarget->SetTransform(
          mFinalTarget->GetTransform().PostTranslate(-mTempRect.TopLeft()));
    }
  }

  ~AdjustedTargetForShadow() {
    if (!mCtx) {
      return;
    }

    RefPtr<SourceSurface> snapshot = mTarget->Snapshot();

    mFinalTarget->DrawSurfaceWithShadow(
        snapshot, mTempRect.TopLeft(),
        ToDeviceColor(mCtx->CurrentState().shadowColor),
        mCtx->CurrentState().shadowOffset, mSigma, mCompositionOp);
  }

  DrawTarget* DT() { return mTarget; }

  gfx::IntPoint OffsetToFinalDT() { return mTempRect.TopLeft(); }

 private:
  RefPtr<DrawTarget> mTarget;
  RefPtr<DrawTarget> mFinalTarget;
  CanvasRenderingContext2D* mCtx;
  Float mSigma;
  gfx::IntRect mTempRect;
  gfx::CompositionOp mCompositionOp;
};

/*
 * This is an RAII based class that can be used as a drawtarget for
 * operations that need a shadow or a filter drawn. It will automatically
 * provide a temporary target when needed, and if so blend it back with a
 * shadow, filter, or both.
 * If both a shadow and a filter are needed, the filter is applied first,
 * and the shadow is applied to the filtered results.
 *
 * aBounds specifies the bounds of the drawing operation that will be
 * drawn to the target, it is given in device space! If this is nullptr the
 * drawing operation will be assumed to cover the whole canvas.
 */
class AdjustedTarget {
 public:
  typedef CanvasRenderingContext2D::ContextState ContextState;

  explicit AdjustedTarget(CanvasRenderingContext2D* aCtx,
                          const gfx::Rect* aBounds = nullptr) {
    // All rects in this function are in the device space of ctx->mTarget.

    // In order to keep our temporary surfaces as small as possible, we first
    // calculate what their maximum required bounds would need to be if we
    // were to fill the whole canvas. Everything outside those bounds we don't
    // need to render.
    gfx::Rect r(0, 0, aCtx->mWidth, aCtx->mHeight);
    gfx::Rect maxSourceNeededBoundsForShadow =
        MaxSourceNeededBoundsForShadow(r, aCtx);
    gfx::Rect maxSourceNeededBoundsForFilter =
        MaxSourceNeededBoundsForFilter(maxSourceNeededBoundsForShadow, aCtx);
    if (!aCtx->IsTargetValid()) {
      return;
    }

    gfx::Rect bounds = maxSourceNeededBoundsForFilter;
    if (aBounds) {
      bounds = bounds.Intersect(*aBounds);
    }
    gfx::Rect boundsAfterFilter = BoundsAfterFilter(bounds, aCtx);
    if (!aCtx->IsTargetValid()) {
      return;
    }

    mozilla::gfx::CompositionOp op = aCtx->CurrentState().op;

    gfx::IntPoint offsetToFinalDT;

    // First set up the shadow draw target, because the shadow goes outside.
    // It applies to the post-filter results, if both a filter and a shadow
    // are used.
    if (aCtx->NeedToDrawShadow()) {
      mShadowTarget = MakeUnique<AdjustedTargetForShadow>(
          aCtx, aCtx->mTarget, boundsAfterFilter, op);
      mTarget = mShadowTarget->DT();
      offsetToFinalDT = mShadowTarget->OffsetToFinalDT();

      // If we also have a filter, the filter needs to be drawn with OP_OVER
      // because shadow drawing already applies op on the result.
      op = gfx::CompositionOp::OP_OVER;
    }

    // Now set up the filter draw target.
    const bool applyFilter = aCtx->NeedToApplyFilter();
    if (!aCtx->IsTargetValid()) {
      return;
    }
    if (applyFilter) {
      bounds.RoundOut();

      if (!mTarget) {
        mTarget = aCtx->mTarget;
      }
      gfx::IntRect intBounds;
      if (!bounds.ToIntRect(&intBounds)) {
        return;
      }
      mFilterTarget = MakeUnique<AdjustedTargetForFilter>(
          aCtx, mTarget, offsetToFinalDT, intBounds,
          gfx::RoundedToInt(boundsAfterFilter), op);
      mTarget = mFilterTarget->DT();
    }
    if (!mTarget) {
      mTarget = aCtx->mTarget;
    }
  }

  ~AdjustedTarget() {
    // The order in which the targets are finalized is important.
    // Filters are inside, any shadow applies to the post-filter results.
    mFilterTarget.reset();
    mShadowTarget.reset();
  }

  operator DrawTarget*() { return mTarget; }

  DrawTarget* operator->() MOZ_NO_ADDREF_RELEASE_ON_RETURN { return mTarget; }

 private:
  gfx::Rect MaxSourceNeededBoundsForFilter(const gfx::Rect& aDestBounds,
                                           CanvasRenderingContext2D* aCtx) {
    const bool applyFilter = aCtx->NeedToApplyFilter();
    if (!aCtx->IsTargetValid()) {
      return aDestBounds;
    }
    if (!applyFilter) {
      return aDestBounds;
    }

    nsIntRegion sourceGraphicNeededRegion;
    nsIntRegion fillPaintNeededRegion;
    nsIntRegion strokePaintNeededRegion;

    FilterSupport::ComputeSourceNeededRegions(
        aCtx->CurrentState().filter, gfx::RoundedToInt(aDestBounds),
        sourceGraphicNeededRegion, fillPaintNeededRegion,
        strokePaintNeededRegion);

    return gfx::Rect(sourceGraphicNeededRegion.GetBounds());
  }

  gfx::Rect MaxSourceNeededBoundsForShadow(const gfx::Rect& aDestBounds,
                                           CanvasRenderingContext2D* aCtx) {
    if (!aCtx->NeedToDrawShadow()) {
      return aDestBounds;
    }

    const ContextState& state = aCtx->CurrentState();
    gfx::Rect sourceBounds = aDestBounds - state.shadowOffset;
    sourceBounds.Inflate(state.ShadowBlurRadius());

    // Union the shadow source with the original rect because we're going to
    // draw both.
    return sourceBounds.Union(aDestBounds);
  }

  gfx::Rect BoundsAfterFilter(const gfx::Rect& aBounds,
                              CanvasRenderingContext2D* aCtx) {
    const bool applyFilter = aCtx->NeedToApplyFilter();
    if (!aCtx->IsTargetValid()) {
      return aBounds;
    }
    if (!applyFilter) {
      return aBounds;
    }

    gfx::Rect bounds(aBounds);
    bounds.RoundOut();

    gfx::IntRect intBounds;
    if (!bounds.ToIntRect(&intBounds)) {
      return gfx::Rect();
    }

    nsIntRegion extents = gfx::FilterSupport::ComputePostFilterExtents(
        aCtx->CurrentState().filter, intBounds);
    return gfx::Rect(extents.GetBounds());
  }

  RefPtr<DrawTarget> mTarget;
  UniquePtr<AdjustedTargetForShadow> mShadowTarget;
  UniquePtr<AdjustedTargetForFilter> mFilterTarget;
};

void CanvasPattern::SetTransform(SVGMatrix& aMatrix) {
  mTransform = ToMatrix(aMatrix.GetMatrix());
}

void CanvasGradient::AddColorStop(float aOffset, const nsACString& aColorstr,
                                  ErrorResult& aRv) {
  if (aOffset < 0.0 || aOffset > 1.0) {
    aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return;
  }

  PresShell* presShell = mContext ? mContext->GetPresShell() : nullptr;
  ServoStyleSet* styleSet = presShell ? presShell->StyleSet() : nullptr;

  nscolor color;
  bool ok = ServoCSSParser::ComputeColor(styleSet, NS_RGB(0, 0, 0), aColorstr,
                                         &color);
  if (!ok) {
    aRv.Throw(NS_ERROR_DOM_SYNTAX_ERR);
    return;
  }

  mStops = nullptr;

  GradientStop newStop;

  newStop.offset = aOffset;
  newStop.color = ToDeviceColor(color);

  mRawStops.AppendElement(newStop);
}

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(CanvasGradient, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(CanvasGradient, Release)

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(CanvasGradient, mContext)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(CanvasPattern, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(CanvasPattern, Release)

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(CanvasPattern, mContext)

class CanvasShutdownObserver final : public nsIObserver {
 public:
  explicit CanvasShutdownObserver(CanvasRenderingContext2D* aCanvas)
      : mCanvas(aCanvas) {}

  void OnShutdown() {
    if (!mCanvas) {
      return;
    }

    mCanvas = nullptr;
    nsContentUtils::UnregisterShutdownObserver(this);
  }

  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER
 private:
  ~CanvasShutdownObserver() = default;

  CanvasRenderingContext2D* mCanvas;
};

NS_IMPL_ISUPPORTS(CanvasShutdownObserver, nsIObserver)

NS_IMETHODIMP
CanvasShutdownObserver::Observe(nsISupports* aSubject, const char* aTopic,
                                const char16_t* aData) {
  if (mCanvas && strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID) == 0) {
    mCanvas->OnShutdown();
    OnShutdown();
  }

  return NS_OK;
}

class CanvasRenderingContext2DUserData : public LayerUserData {
 public:
  explicit CanvasRenderingContext2DUserData(CanvasRenderingContext2D* aContext)
      : mContext(aContext) {
    aContext->mUserDatas.AppendElement(this);
  }
  ~CanvasRenderingContext2DUserData() {
    if (mContext) {
      mContext->mUserDatas.RemoveElement(this);
    }
  }

  static void PreTransactionCallback(void* aData) {
    CanvasRenderingContext2D* context =
        static_cast<CanvasRenderingContext2D*>(aData);
    if (!context || !context->mTarget) return;

    context->OnStableState();
  }

  static void DidTransactionCallback(void* aData) {
    CanvasRenderingContext2D* context =
        static_cast<CanvasRenderingContext2D*>(aData);
    if (context) {
      context->MarkContextClean();
    }
  }
  bool IsForContext(CanvasRenderingContext2D* aContext) {
    return mContext == aContext;
  }
  void Forget() { mContext = nullptr; }

 private:
  CanvasRenderingContext2D* mContext;
};

NS_IMPL_CYCLE_COLLECTING_ADDREF(CanvasRenderingContext2D)
NS_IMPL_CYCLE_COLLECTING_RELEASE(CanvasRenderingContext2D)

NS_IMPL_CYCLE_COLLECTION_CLASS(CanvasRenderingContext2D)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(CanvasRenderingContext2D)
  // Make sure we remove ourselves from the list of demotable contexts (raw
  // pointers), since we're logically destructed at this point.
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mCanvasElement)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mDocShell)
  for (uint32_t i = 0; i < tmp->mStyleStack.Length(); i++) {
    ImplCycleCollectionUnlink(tmp->mStyleStack[i].patternStyles[Style::STROKE]);
    ImplCycleCollectionUnlink(tmp->mStyleStack[i].patternStyles[Style::FILL]);
    ImplCycleCollectionUnlink(
        tmp->mStyleStack[i].gradientStyles[Style::STROKE]);
    ImplCycleCollectionUnlink(tmp->mStyleStack[i].gradientStyles[Style::FILL]);
    auto autoSVGFiltersObserver =
        tmp->mStyleStack[i].autoSVGFiltersObserver.get();
    if (autoSVGFiltersObserver) {
      // XXXjwatt: I don't think this call achieves anything.  See the comment
      // that documents this function.
      SVGObserverUtils::DetachFromCanvasContext(autoSVGFiltersObserver);
    }
    ImplCycleCollectionUnlink(tmp->mStyleStack[i].autoSVGFiltersObserver);
  }
  for (size_t x = 0; x < tmp->mHitRegionsOptions.Length(); x++) {
    RegionInfo& info = tmp->mHitRegionsOptions[x];
    if (info.mElement) {
      ImplCycleCollectionUnlink(info.mElement);
    }
  }
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(CanvasRenderingContext2D)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mCanvasElement)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDocShell)
  for (uint32_t i = 0; i < tmp->mStyleStack.Length(); i++) {
    ImplCycleCollectionTraverse(
        cb, tmp->mStyleStack[i].patternStyles[Style::STROKE],
        "Stroke CanvasPattern");
    ImplCycleCollectionTraverse(cb,
                                tmp->mStyleStack[i].patternStyles[Style::FILL],
                                "Fill CanvasPattern");
    ImplCycleCollectionTraverse(
        cb, tmp->mStyleStack[i].gradientStyles[Style::STROKE],
        "Stroke CanvasGradient");
    ImplCycleCollectionTraverse(cb,
                                tmp->mStyleStack[i].gradientStyles[Style::FILL],
                                "Fill CanvasGradient");
    ImplCycleCollectionTraverse(cb, tmp->mStyleStack[i].autoSVGFiltersObserver,
                                "RAII SVG Filters Observer");
  }
  for (size_t x = 0; x < tmp->mHitRegionsOptions.Length(); x++) {
    RegionInfo& info = tmp->mHitRegionsOptions[x];
    if (info.mElement) {
      ImplCycleCollectionTraverse(cb, info.mElement,
                                  "Hit region fallback element");
    }
  }
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_WRAPPERCACHE(CanvasRenderingContext2D)

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_BEGIN(CanvasRenderingContext2D)
  if (nsCCUncollectableMarker::sGeneration && tmp->HasKnownLiveWrapper()) {
    dom::Element* canvasElement = tmp->mCanvasElement;
    if (canvasElement) {
      if (canvasElement->IsPurple()) {
        canvasElement->RemovePurple();
      }
      dom::Element::MarkNodeChildren(canvasElement);
    }
    return true;
  }
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_END

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_IN_CC_BEGIN(CanvasRenderingContext2D)
  return nsCCUncollectableMarker::sGeneration && tmp->HasKnownLiveWrapper();
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_IN_CC_END

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_THIS_BEGIN(CanvasRenderingContext2D)
  return nsCCUncollectableMarker::sGeneration && tmp->HasKnownLiveWrapper();
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_THIS_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(CanvasRenderingContext2D)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsICanvasRenderingContextInternal)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

CanvasRenderingContext2D::ContextState::ContextState() = default;

CanvasRenderingContext2D::ContextState::ContextState(const ContextState& aOther)
    : fontGroup(aOther.fontGroup),
      fontLanguage(aOther.fontLanguage),
      fontFont(aOther.fontFont),
      gradientStyles(aOther.gradientStyles),
      patternStyles(aOther.patternStyles),
      colorStyles(aOther.colorStyles),
      font(aOther.font),
      textAlign(aOther.textAlign),
      textBaseline(aOther.textBaseline),
      shadowColor(aOther.shadowColor),
      transform(aOther.transform),
      shadowOffset(aOther.shadowOffset),
      lineWidth(aOther.lineWidth),
      miterLimit(aOther.miterLimit),
      globalAlpha(aOther.globalAlpha),
      shadowBlur(aOther.shadowBlur),
      dash(aOther.dash.Clone()),
      dashOffset(aOther.dashOffset),
      op(aOther.op),
      fillRule(aOther.fillRule),
      lineCap(aOther.lineCap),
      lineJoin(aOther.lineJoin),
      filterString(aOther.filterString),
      filterChain(aOther.filterChain),
      autoSVGFiltersObserver(aOther.autoSVGFiltersObserver),
      filter(aOther.filter),
      filterAdditionalImages(aOther.filterAdditionalImages.Clone()),
      filterSourceGraphicTainted(aOther.filterSourceGraphicTainted),
      imageSmoothingEnabled(aOther.imageSmoothingEnabled),
      fontExplicitLanguage(aOther.fontExplicitLanguage) {}

CanvasRenderingContext2D::ContextState::~ContextState() = default;

/**
 ** CanvasRenderingContext2D impl
 **/

// Initialize our static variables.
uintptr_t CanvasRenderingContext2D::sNumLivingContexts = 0;
DrawTarget* CanvasRenderingContext2D::sErrorTarget = nullptr;

CanvasRenderingContext2D::CanvasRenderingContext2D(
    layers::LayersBackend aCompositorBackend)
    :  // these are the default values from the Canvas spec
      mWidth(0),
      mHeight(0),
      mZero(false),
      mOpaqueAttrValue(false),
      mContextAttributesHasAlpha(true),
      mOpaque(false),
      mResetLayer(true),
      mIPC(false),
      mHasPendingStableStateCallback(false),
      mIsEntireFrameInvalid(false),
      mPredictManyRedrawCalls(false),
      mIsCapturedFrameInvalid(false),
      mPathTransformWillUpdate(false),
      mInvalidateCount(0),
      mWriteOnly(false) {
  sNumLivingContexts++;

  mShutdownObserver = new CanvasShutdownObserver(this);
  nsContentUtils::RegisterShutdownObserver(mShutdownObserver);
}

CanvasRenderingContext2D::~CanvasRenderingContext2D() {
  RemovePostRefreshObserver();
  RemoveShutdownObserver();
  Reset();
  // Drop references from all CanvasRenderingContext2DUserData to this context
  for (uint32_t i = 0; i < mUserDatas.Length(); ++i) {
    mUserDatas[i]->Forget();
  }
  sNumLivingContexts--;
  if (!sNumLivingContexts) {
    NS_IF_RELEASE(sErrorTarget);
  }
}

JSObject* CanvasRenderingContext2D::WrapObject(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return CanvasRenderingContext2D_Binding::Wrap(aCx, this, aGivenProto);
}

bool CanvasRenderingContext2D::ParseColor(const nsACString& aString,
                                          nscolor* aColor) {
  Document* document = mCanvasElement ? mCanvasElement->OwnerDoc() : nullptr;
  css::Loader* loader = document ? document->CSSLoader() : nullptr;

  PresShell* presShell = GetPresShell();
  ServoStyleSet* set = presShell ? presShell->StyleSet() : nullptr;

  // First, try computing the color without handling currentcolor.
  bool wasCurrentColor = false;
  if (!ServoCSSParser::ComputeColor(set, NS_RGB(0, 0, 0), aString, aColor,
                                    &wasCurrentColor, loader)) {
    return false;
  }

  if (wasCurrentColor && mCanvasElement) {
    // Otherwise, get the value of the color property, flushing style
    // if necessary.
    RefPtr<ComputedStyle> canvasStyle =
        nsComputedDOMStyle::GetComputedStyle(mCanvasElement, nullptr);
    if (canvasStyle) {
      *aColor = canvasStyle->StyleText()->mColor.ToColor();
    }
    // Beware that the presShell could be gone here.
  }
  return true;
}

nsresult CanvasRenderingContext2D::Reset() {
  if (mCanvasElement) {
    mCanvasElement->InvalidateCanvas();
  }

  // only do this for non-docshell created contexts,
  // since those are the ones that we created a surface for
  if (mTarget && IsTargetValid() && !mDocShell) {
    gCanvasAzureMemoryUsed -= mWidth * mHeight * 4;
  }

  bool forceReset = true;
  ReturnTarget(forceReset);
  mTarget = nullptr;
  mBufferProvider = nullptr;

  // reset hit regions
  mHitRegionsOptions.ClearAndRetainStorage();

  // Since the target changes the backing texture will change, and this will
  // no longer be valid.
  mIsEntireFrameInvalid = false;
  mPredictManyRedrawCalls = false;
  mIsCapturedFrameInvalid = false;

  return NS_OK;
}

void CanvasRenderingContext2D::OnShutdown() {
  mShutdownObserver = nullptr;

  RefPtr<PersistentBufferProvider> provider = mBufferProvider;

  Reset();

  if (provider) {
    provider->OnShutdown();
  }
}

void CanvasRenderingContext2D::RemoveShutdownObserver() {
  if (mShutdownObserver) {
    mShutdownObserver->OnShutdown();
    mShutdownObserver = nullptr;
  }
}

void CanvasRenderingContext2D::SetStyleFromString(const nsAString& aStr,
                                                  Style aWhichStyle) {
  MOZ_ASSERT(!aStr.IsVoid());

  nscolor color;
  if (!ParseColor(NS_ConvertUTF16toUTF8(aStr), &color)) {
    return;
  }

  CurrentState().SetColorStyle(aWhichStyle, color);
}

void CanvasRenderingContext2D::GetStyleAsUnion(
    OwningStringOrCanvasGradientOrCanvasPattern& aValue, Style aWhichStyle) {
  const ContextState& state = CurrentState();
  if (state.patternStyles[aWhichStyle]) {
    aValue.SetAsCanvasPattern() = state.patternStyles[aWhichStyle];
  } else if (state.gradientStyles[aWhichStyle]) {
    aValue.SetAsCanvasGradient() = state.gradientStyles[aWhichStyle];
  } else {
    StyleColorToString(state.colorStyles[aWhichStyle], aValue.SetAsString());
  }
}

// static
void CanvasRenderingContext2D::StyleColorToString(const nscolor& aColor,
                                                  nsAString& aStr) {
  // We can't reuse the normal CSS color stringification code,
  // because the spec calls for a different algorithm for canvas.
  if (NS_GET_A(aColor) == 255) {
    CopyUTF8toUTF16(nsPrintfCString("#%02x%02x%02x", NS_GET_R(aColor),
                                    NS_GET_G(aColor), NS_GET_B(aColor)),
                    aStr);
  } else {
    CopyUTF8toUTF16(nsPrintfCString("rgba(%d, %d, %d, ", NS_GET_R(aColor),
                                    NS_GET_G(aColor), NS_GET_B(aColor)),
                    aStr);
    aStr.AppendFloat(nsStyleUtil::ColorComponentToFloat(NS_GET_A(aColor)));
    aStr.Append(')');
  }
}

nsresult CanvasRenderingContext2D::Redraw() {
  mIsCapturedFrameInvalid = true;

  if (mIsEntireFrameInvalid) {
    return NS_OK;
  }

  mIsEntireFrameInvalid = true;

  if (!mCanvasElement) {
    NS_ASSERTION(mDocShell, "Redraw with no canvas element or docshell!");
    return NS_OK;
  }

  SVGObserverUtils::InvalidateDirectRenderingObservers(mCanvasElement);

  mCanvasElement->InvalidateCanvasContent(nullptr);

  return NS_OK;
}

void CanvasRenderingContext2D::Redraw(const gfx::Rect& aR) {
  mIsCapturedFrameInvalid = true;

  ++mInvalidateCount;

  if (mIsEntireFrameInvalid) {
    return;
  }

  if (mPredictManyRedrawCalls || mInvalidateCount > kCanvasMaxInvalidateCount) {
    Redraw();
    return;
  }

  if (!mCanvasElement) {
    NS_ASSERTION(mDocShell, "Redraw with no canvas element or docshell!");
    return;
  }

  SVGObserverUtils::InvalidateDirectRenderingObservers(mCanvasElement);

  mCanvasElement->InvalidateCanvasContent(&aR);
}

void CanvasRenderingContext2D::DidRefresh() {}

void CanvasRenderingContext2D::RedrawUser(const gfxRect& aR) {
  mIsCapturedFrameInvalid = true;

  if (mIsEntireFrameInvalid) {
    ++mInvalidateCount;
    return;
  }

  gfx::Rect newr = mTarget->GetTransform().TransformBounds(ToRect(aR));
  Redraw(newr);
}

bool CanvasRenderingContext2D::CopyBufferProvider(
    PersistentBufferProvider& aOld, DrawTarget& aTarget, IntRect aCopyRect) {
  // Borrowing the snapshot must be done after ReturnTarget.
  RefPtr<SourceSurface> snapshot = aOld.BorrowSnapshot();

  if (!snapshot) {
    return false;
  }

  aTarget.CopySurface(snapshot, aCopyRect, IntPoint());
  aOld.ReturnSnapshot(snapshot.forget());
  return true;
}

void CanvasRenderingContext2D::Demote() {}

void CanvasRenderingContext2D::ScheduleStableStateCallback() {
  if (mHasPendingStableStateCallback) {
    return;
  }
  mHasPendingStableStateCallback = true;

  nsContentUtils::RunInStableState(
      NewRunnableMethod("dom::CanvasRenderingContext2D::OnStableState", this,
                        &CanvasRenderingContext2D::OnStableState));
}

void CanvasRenderingContext2D::OnStableState() {
  if (!mHasPendingStableStateCallback) {
    return;
  }

  ReturnTarget();

  mHasPendingStableStateCallback = false;
}

void CanvasRenderingContext2D::RestoreClipsAndTransformToTarget() {
  // Restore clips and transform.
  mTarget->SetTransform(Matrix());

  if (mTarget->GetBackendType() == gfx::BackendType::CAIRO) {
    // Cairo doesn't play well with huge clips. When given a very big clip it
    // will try to allocate big mask surface without taking the target
    // size into account which can cause OOM. See bug 1034593.
    // This limits the clip extents to the size of the canvas.
    // A fix in Cairo would probably be preferable, but requires somewhat
    // invasive changes.
    mTarget->PushClipRect(gfx::Rect(0, 0, mWidth, mHeight));
  }

  for (auto& style : mStyleStack) {
    for (auto clipOrTransform = style.clipsAndTransforms.begin();
         clipOrTransform != style.clipsAndTransforms.end(); clipOrTransform++) {
      if (clipOrTransform->IsClip()) {
        if (mClipsNeedConverting) {
          // We have possibly changed backends, so we need to convert the clips
          // in case they are no longer compatible with mTarget.
          RefPtr<PathBuilder> pathBuilder = mTarget->CreatePathBuilder();
          clipOrTransform->clip->StreamToSink(pathBuilder);
          clipOrTransform->clip = pathBuilder->Finish();
        }
        mTarget->PushClip(clipOrTransform->clip);
      } else {
        mTarget->SetTransform(clipOrTransform->transform);
      }
    }
  }

  mClipsNeedConverting = false;
}

bool CanvasRenderingContext2D::EnsureTarget(const gfx::Rect* aCoveredRect,
                                            bool aWillClear) {
  if (AlreadyShutDown()) {
    gfxCriticalError() << "Attempt to render into a Canvas2d after shutdown.";
    SetErrorState();
    return false;
  }

  if (mTarget) {
    return mTarget != sErrorTarget;
  }

  // Check that the dimensions are sane
  if (mWidth > StaticPrefs::gfx_canvas_max_size() ||
      mHeight > StaticPrefs::gfx_canvas_max_size() || mWidth < 0 ||
      mHeight < 0) {
    SetErrorState();
    return false;
  }

  // If the next drawing command covers the entire canvas, we can skip copying
  // from the previous frame and/or clearing the canvas.
  gfx::Rect canvasRect(0, 0, mWidth, mHeight);
  bool canDiscardContent =
      aCoveredRect && CurrentState()
                          .transform.TransformBounds(*aCoveredRect)
                          .Contains(canvasRect);

  // If a clip is active we don't know for sure that the next drawing command
  // will really cover the entire canvas.
  for (const auto& style : mStyleStack) {
    if (!canDiscardContent) {
      break;
    }
    for (const auto& clipOrTransform : style.clipsAndTransforms) {
      if (clipOrTransform.IsClip()) {
        canDiscardContent = false;
        break;
      }
    }
  }

  ScheduleStableStateCallback();

  IntRect persistedRect =
      canDiscardContent ? IntRect() : IntRect(0, 0, mWidth, mHeight);

  if (mBufferProvider) {
    mTarget = mBufferProvider->BorrowDrawTarget(persistedRect);

    if (mTarget && !mBufferProvider->PreservesDrawingState()) {
      RestoreClipsAndTransformToTarget();
    }

    if (mTarget && mTarget->IsValid()) {
      return true;
    }
  }

  RefPtr<DrawTarget> newTarget;
  RefPtr<PersistentBufferProvider> newProvider;

  if (!TrySharedTarget(newTarget, newProvider) &&
      !TryBasicTarget(newTarget, newProvider)) {
    gfxCriticalError(
        CriticalLog::DefaultOptions(Factory::ReasonableSurfaceSize(GetSize())))
        << "Failed borrow shared and basic targets.";

    SetErrorState();
    return false;
  }

  MOZ_ASSERT(newTarget);
  MOZ_ASSERT(newProvider);

  bool needsClear = !canDiscardContent;
  if (newTarget->GetBackendType() == gfx::BackendType::SKIA &&
      (needsClear || !aWillClear)) {
    // Skia expects the unused X channel to contains 0xFF even for opaque
    // operations so we can't skip clearing in that case, even if we are going
    // to cover the entire canvas in the next drawing operation.
    newTarget->ClearRect(canvasRect);
    needsClear = false;
  }

  // Try to copy data from the previous buffer provider if there is one.
  if (!canDiscardContent && mBufferProvider &&
      CopyBufferProvider(*mBufferProvider, *newTarget, persistedRect)) {
    needsClear = false;
  }

  if (needsClear) {
    newTarget->ClearRect(canvasRect);
  }

  mTarget = std::move(newTarget);
  mBufferProvider = std::move(newProvider);

  RegisterAllocation();

  RestoreClipsAndTransformToTarget();

  // Force a full layer transaction since we didn't have a layer before
  // and now we might need one.
  if (mCanvasElement) {
    mCanvasElement->InvalidateCanvas();
  }
  // Calling Redraw() tells our invalidation machinery that the entire
  // canvas is already invalid, which can speed up future drawing.
  Redraw();

  return true;
}

void CanvasRenderingContext2D::SetInitialState() {
  // Set up the initial canvas defaults
  mPathBuilder = nullptr;
  mPath = nullptr;
  mDSPathBuilder = nullptr;
  mPathTransformWillUpdate = false;

  mStyleStack.Clear();
  ContextState* state = mStyleStack.AppendElement();
  state->globalAlpha = 1.0;

  state->colorStyles[Style::FILL] = NS_RGB(0, 0, 0);
  state->colorStyles[Style::STROKE] = NS_RGB(0, 0, 0);
  state->shadowColor = NS_RGBA(0, 0, 0, 0);
}

void CanvasRenderingContext2D::SetErrorState() {
  EnsureErrorTarget();

  if (mTarget && mTarget != sErrorTarget) {
    gCanvasAzureMemoryUsed -= mWidth * mHeight * 4;
  }

  mTarget = sErrorTarget;
  mBufferProvider = nullptr;

  // clear transforms, clips, etc.
  SetInitialState();
}

void CanvasRenderingContext2D::RegisterAllocation() {
  // XXX - It would make more sense to track the allocation in
  // PeristentBufferProvider, rather than here.
  static bool registered = false;
  // FIXME: Disable the reporter for now, see bug 1241865
  if (!registered && false) {
    registered = true;
    RegisterStrongMemoryReporter(new Canvas2dPixelsReporter());
  }

  JSObject* wrapper = GetWrapperPreserveColor();
  if (wrapper) {
    CycleCollectedJSRuntime::Get()->AddZoneWaitingForGC(
        JS::GetObjectZone(wrapper));
  }
}

static already_AddRefed<LayerManager> LayerManagerFromCanvasElement(
    nsINode* aCanvasElement) {
  if (!aCanvasElement) {
    return nullptr;
  }

  return nsContentUtils::PersistentLayerManagerForDocument(
      aCanvasElement->OwnerDoc());
}

bool CanvasRenderingContext2D::TrySharedTarget(
    RefPtr<gfx::DrawTarget>& aOutDT,
    RefPtr<layers::PersistentBufferProvider>& aOutProvider) {
  aOutDT = nullptr;
  aOutProvider = nullptr;

  if (!mCanvasElement) {
    return false;
  }

  if (mBufferProvider &&
      (mBufferProvider->GetType() == LayersBackend::LAYERS_CLIENT ||
       mBufferProvider->GetType() == LayersBackend::LAYERS_WR)) {
    // we are already using a shared buffer provider, we are allocating a new
    // one because the current one failed so let's just fall back to the basic
    // provider.
    mClipsNeedConverting = true;
    return false;
  }

  RefPtr<LayerManager> layerManager =
      LayerManagerFromCanvasElement(mCanvasElement);

  if (!layerManager) {
    return false;
  }

  aOutProvider = layerManager->CreatePersistentBufferProvider(
      GetSize(), GetSurfaceFormat());

  if (!aOutProvider) {
    return false;
  }

  // We can pass an empty persisted rect since we just created the buffer
  // provider (nothing to restore).
  aOutDT = aOutProvider->BorrowDrawTarget(IntRect());
  MOZ_ASSERT(aOutDT);

  return !!aOutDT;
}

bool CanvasRenderingContext2D::TryBasicTarget(
    RefPtr<gfx::DrawTarget>& aOutDT,
    RefPtr<layers::PersistentBufferProvider>& aOutProvider) {
  aOutDT = gfxPlatform::GetPlatform()->CreateOffscreenCanvasDrawTarget(
      GetSize(), GetSurfaceFormat());
  if (!aOutDT) {
    return false;
  }

  // See Bug 1524554 - this forces DT initialization.
  aOutDT->ClearRect(gfx::Rect());

  if (!aOutDT->IsValid()) {
    aOutDT = nullptr;
    return false;
  }

  aOutProvider = new PersistentBufferProviderBasic(aOutDT);
  return true;
}

NS_IMETHODIMP
CanvasRenderingContext2D::SetDimensions(int32_t aWidth, int32_t aHeight) {
  // Zero sized surfaces can cause problems.
  mZero = false;
  if (aHeight == 0) {
    aHeight = 1;
    mZero = true;
  }
  if (aWidth == 0) {
    aWidth = 1;
    mZero = true;
  }

  ClearTarget(aWidth, aHeight);

  return NS_OK;
}

void CanvasRenderingContext2D::ClearTarget(int32_t aWidth, int32_t aHeight) {
  Reset();

  mResetLayer = true;

  SetInitialState();

  // Update dimensions only if new (strictly positive) values were passed.
  if (aWidth > 0 && aHeight > 0) {
    // Update the memory size associated with the wrapper object when we change
    // the dimensions. Note that we need to keep updating dying wrappers before
    // they are finalized so that the memory accounting balances out.
    JSObject* wrapper = GetWrapperMaybeDead();
    if (wrapper) {
      JS::RemoveAssociatedMemory(wrapper, BindingJSObjectMallocBytes(this),
                                 JS::MemoryUse::DOMBinding);
    }

    mWidth = aWidth;
    mHeight = aHeight;

    if (wrapper) {
      JS::AddAssociatedMemory(wrapper, BindingJSObjectMallocBytes(this),
                              JS::MemoryUse::DOMBinding);
    }
  }

  if (!mCanvasElement || !mCanvasElement->IsInComposedDoc()) {
    return;
  }

  // For vertical writing-mode, unless text-orientation is sideways,
  // we'll modify the initial value of textBaseline to 'middle'.
  RefPtr<ComputedStyle> canvasStyle =
      nsComputedDOMStyle::GetComputedStyle(mCanvasElement, nullptr);
  if (canvasStyle) {
    WritingMode wm(canvasStyle);
    if (wm.IsVertical() && !wm.IsSideways()) {
      CurrentState().textBaseline = TextBaseline::MIDDLE;
    }
  }
}

void CanvasRenderingContext2D::ReturnTarget(bool aForceReset) {
  if (mTarget && mBufferProvider && mTarget != sErrorTarget) {
    CurrentState().transform = mTarget->GetTransform();
    if (aForceReset || !mBufferProvider->PreservesDrawingState()) {
      for (const auto& style : mStyleStack) {
        for (const auto& clipOrTransform : style.clipsAndTransforms) {
          if (clipOrTransform.IsClip()) {
            mTarget->PopClip();
          }
        }
      }

      if (mTarget->GetBackendType() == gfx::BackendType::CAIRO) {
        // With the cairo backend we pushed an extra clip rect which we have to
        // balance out here. See the comment in
        // RestoreClipsAndTransformToTarget.
        mTarget->PopClip();
      }

      mTarget->SetTransform(Matrix());
    }

    mBufferProvider->ReturnDrawTarget(mTarget.forget());
  }
}

NS_IMETHODIMP
CanvasRenderingContext2D::InitializeWithDrawTarget(
    nsIDocShell* aShell, NotNull<gfx::DrawTarget*> aTarget) {
  RemovePostRefreshObserver();
  mDocShell = aShell;
  AddPostRefreshObserverIfNecessary();

  IntSize size = aTarget->GetSize();
  SetDimensions(size.width, size.height);

  mTarget = aTarget;
  mBufferProvider = new PersistentBufferProviderBasic(aTarget);

  if (mTarget->GetBackendType() == gfx::BackendType::CAIRO) {
    // Cf comment in EnsureTarget
    mTarget->PushClipRect(gfx::Rect(Point(0, 0), Size(mWidth, mHeight)));
  }

  return NS_OK;
}

void CanvasRenderingContext2D::SetOpaqueValueFromOpaqueAttr(
    bool aOpaqueAttrValue) {
  if (aOpaqueAttrValue != mOpaqueAttrValue) {
    mOpaqueAttrValue = aOpaqueAttrValue;
    UpdateIsOpaque();
  }
}

void CanvasRenderingContext2D::UpdateIsOpaque() {
  mOpaque = !mContextAttributesHasAlpha || mOpaqueAttrValue;
  ClearTarget();
}

NS_IMETHODIMP
CanvasRenderingContext2D::SetIsIPC(bool aIsIPC) {
  if (aIsIPC != mIPC) {
    mIPC = aIsIPC;
    ClearTarget();
  }

  return NS_OK;
}

NS_IMETHODIMP
CanvasRenderingContext2D::SetContextOptions(JSContext* aCx,
                                            JS::Handle<JS::Value> aOptions,
                                            ErrorResult& aRvForDictionaryInit) {
  if (aOptions.isNullOrUndefined()) {
    return NS_OK;
  }

  // This shouldn't be called before drawing starts, so there should be no
  // drawtarget yet
  MOZ_ASSERT(!mTarget);

  ContextAttributes2D attributes;
  if (!attributes.Init(aCx, aOptions)) {
    aRvForDictionaryInit.Throw(NS_ERROR_UNEXPECTED);
    return NS_ERROR_UNEXPECTED;
  }

  mContextAttributesHasAlpha = attributes.mAlpha;
  UpdateIsOpaque();

  return NS_OK;
}

UniquePtr<uint8_t[]> CanvasRenderingContext2D::GetImageBuffer(
    int32_t* aFormat) {
  UniquePtr<uint8_t[]> ret;

  *aFormat = 0;

  if (!mBufferProvider) {
    if (!EnsureTarget()) {
      return nullptr;
    }
  }

  RefPtr<SourceSurface> snapshot = mBufferProvider->BorrowSnapshot();
  if (snapshot) {
    RefPtr<DataSourceSurface> data = snapshot->GetDataSurface();
    if (data && data->GetSize() == GetSize()) {
      *aFormat = imgIEncoder::INPUT_FORMAT_HOSTARGB;
      ret = SurfaceToPackedBGRA(data);
    }
  }

  mBufferProvider->ReturnSnapshot(snapshot.forget());

  return ret;
}

nsString CanvasRenderingContext2D::GetHitRegion(
    const mozilla::gfx::Point& aPoint) {
  for (size_t x = 0; x < mHitRegionsOptions.Length(); x++) {
    RegionInfo& info = mHitRegionsOptions[x];
    if (info.mPath->ContainsPoint(aPoint, Matrix())) {
      return info.mId;
    }
  }
  return nsString();
}

NS_IMETHODIMP
CanvasRenderingContext2D::GetInputStream(const char* aMimeType,
                                         const nsAString& aEncoderOptions,
                                         nsIInputStream** aStream) {
  nsCString enccid("@mozilla.org/image/encoder;2?type=");
  enccid += aMimeType;
  nsCOMPtr<imgIEncoder> encoder = do_CreateInstance(enccid.get());
  if (!encoder) {
    return NS_ERROR_FAILURE;
  }

  int32_t format = 0;
  UniquePtr<uint8_t[]> imageBuffer = GetImageBuffer(&format);
  if (!imageBuffer) {
    return NS_ERROR_FAILURE;
  }

  return ImageEncoder::GetInputStream(mWidth, mHeight, imageBuffer.get(),
                                      format, encoder, aEncoderOptions,
                                      aStream);
}

already_AddRefed<mozilla::gfx::SourceSurface>
CanvasRenderingContext2D::GetSurfaceSnapshot(gfxAlphaType* aOutAlphaType) {
  if (aOutAlphaType) {
    *aOutAlphaType = (mOpaque ? gfxAlphaType::Opaque : gfxAlphaType::Premult);
  }

  // For GetSurfaceSnapshot we always call EnsureTarget even if mBufferProvider
  // already exists, otherwise we get performance issues. See bug 1567054.
  if (!EnsureTarget()) {
    MOZ_ASSERT(
        mTarget == sErrorTarget,
        "On EnsureTarget failure mTarget should be set to sErrorTarget.");
    return mTarget->Snapshot();
  }

  // The concept of BorrowSnapshot seems a bit broken here, but the original
  // code in GetSurfaceSnapshot just returned a snapshot from mTarget, which
  // amounts to breaking the concept implicitly.
  RefPtr<SourceSurface> snapshot = mBufferProvider->BorrowSnapshot();
  RefPtr<SourceSurface> retSurface = snapshot;
  mBufferProvider->ReturnSnapshot(snapshot.forget());
  return retSurface.forget();
}

SurfaceFormat CanvasRenderingContext2D::GetSurfaceFormat() const {
  return mOpaque ? SurfaceFormat::B8G8R8X8 : SurfaceFormat::B8G8R8A8;
}

//
// state
//

void CanvasRenderingContext2D::Save() {
  EnsureTarget();
  if (MOZ_UNLIKELY(!mTarget || mStyleStack.IsEmpty())) {
    SetErrorState();
    return;
  }
  mStyleStack[mStyleStack.Length() - 1].transform = mTarget->GetTransform();
  mStyleStack.SetCapacity(mStyleStack.Length() + 1);
  mStyleStack.AppendElement(CurrentState());

  if (mStyleStack.Length() > MAX_STYLE_STACK_SIZE) {
    // This is not fast, but is better than OOMing and shouldn't be hit by
    // reasonable code.
    mStyleStack.RemoveElementAt(0);
  }
}

void CanvasRenderingContext2D::Restore() {
  if (MOZ_UNLIKELY(mStyleStack.Length() < 2)) {
    return;
  }

  TransformWillUpdate();
  if (!IsTargetValid()) {
    return;
  }

  for (const auto& clipOrTransform : CurrentState().clipsAndTransforms) {
    if (clipOrTransform.IsClip()) {
      mTarget->PopClip();
    }
  }

  mStyleStack.RemoveLastElement();

  mTarget->SetTransform(CurrentState().transform);
}

//
// transformations
//

void CanvasRenderingContext2D::Scale(double aX, double aY,
                                     ErrorResult& aError) {
  TransformWillUpdate();
  if (!IsTargetValid()) {
    aError.Throw(NS_ERROR_FAILURE);
    return;
  }

  Matrix newMatrix = mTarget->GetTransform();
  newMatrix.PreScale(aX, aY);

  SetTransformInternal(newMatrix);
}

void CanvasRenderingContext2D::Rotate(double aAngle, ErrorResult& aError) {
  TransformWillUpdate();
  if (!IsTargetValid()) {
    aError.Throw(NS_ERROR_FAILURE);
    return;
  }

  Matrix newMatrix = Matrix::Rotation(aAngle) * mTarget->GetTransform();

  SetTransformInternal(newMatrix);
}

void CanvasRenderingContext2D::Translate(double aX, double aY,
                                         ErrorResult& aError) {
  TransformWillUpdate();
  if (!IsTargetValid()) {
    aError.Throw(NS_ERROR_FAILURE);
    return;
  }

  Matrix newMatrix = mTarget->GetTransform();
  newMatrix.PreTranslate(aX, aY);

  SetTransformInternal(newMatrix);
}

void CanvasRenderingContext2D::Transform(double aM11, double aM12, double aM21,
                                         double aM22, double aDx, double aDy,
                                         ErrorResult& aError) {
  TransformWillUpdate();
  if (!IsTargetValid()) {
    aError.Throw(NS_ERROR_FAILURE);
    return;
  }

  Matrix newMatrix(aM11, aM12, aM21, aM22, aDx, aDy);
  newMatrix *= mTarget->GetTransform();

  SetTransformInternal(newMatrix);
}

already_AddRefed<DOMMatrix> CanvasRenderingContext2D::GetTransform(
    ErrorResult& aError) {
  EnsureTarget();
  if (!IsTargetValid()) {
    aError.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }
  RefPtr<DOMMatrix> matrix =
      new DOMMatrix(GetParentObject(), mTarget->GetTransform());
  return matrix.forget();
}

void CanvasRenderingContext2D::SetTransform(double aM11, double aM12,
                                            double aM21, double aM22,
                                            double aDx, double aDy,
                                            ErrorResult& aError) {
  TransformWillUpdate();
  if (!IsTargetValid()) {
    aError.Throw(NS_ERROR_FAILURE);
    return;
  }

  SetTransformInternal(Matrix(aM11, aM12, aM21, aM22, aDx, aDy));
}

void CanvasRenderingContext2D::SetTransform(const DOMMatrix2DInit& aInit,
                                            ErrorResult& aError) {
  TransformWillUpdate();
  if (!IsTargetValid()) {
    aError.Throw(NS_ERROR_FAILURE);
    return;
  }

  RefPtr<DOMMatrixReadOnly> matrix =
      DOMMatrixReadOnly::FromMatrix(GetParentObject(), aInit, aError);
  if (!aError.Failed()) {
    SetTransformInternal(Matrix(*(matrix->GetInternal2D())));
  }
}

void CanvasRenderingContext2D::SetTransformInternal(const Matrix& aTransform) {
  if (!aTransform.IsFinite()) {
    return;
  }

  // Save the transform in the clip stack to be able to replay clips properly.
  auto& clipsAndTransforms = CurrentState().clipsAndTransforms;
  if (clipsAndTransforms.IsEmpty() ||
      clipsAndTransforms.LastElement().IsClip()) {
    clipsAndTransforms.AppendElement(ClipState(aTransform));
  } else {
    // If the last item is a transform we can replace it instead of appending
    // a new item.
    clipsAndTransforms.LastElement().transform = aTransform;
  }
  mTarget->SetTransform(aTransform);
}

void CanvasRenderingContext2D::ResetTransform(ErrorResult& aError) {
  SetTransform(1.0, 0.0, 0.0, 1.0, 0.0, 0.0, aError);
}

static void MatrixToJSObject(JSContext* aCx, const Matrix& aMatrix,
                             JS::MutableHandle<JSObject*> aResult,
                             ErrorResult& aError) {
  double elts[6] = {aMatrix._11, aMatrix._12, aMatrix._21,
                    aMatrix._22, aMatrix._31, aMatrix._32};

  // XXX Should we enter GetWrapper()'s compartment?
  JS::Rooted<JS::Value> val(aCx);
  if (!ToJSValue(aCx, elts, &val)) {
    aError.Throw(NS_ERROR_OUT_OF_MEMORY);
  } else {
    aResult.set(&val.toObject());
  }
}

static bool ObjectToMatrix(JSContext* aCx, JS::Handle<JSObject*> aObj,
                           Matrix& aMatrix, ErrorResult& aError) {
  uint32_t length;
  if (!JS::GetArrayLength(aCx, aObj, &length) || length != 6) {
    // Not an array-like thing or wrong size
    aError.Throw(NS_ERROR_INVALID_ARG);
    return false;
  }

  Float* elts[] = {&aMatrix._11, &aMatrix._12, &aMatrix._21,
                   &aMatrix._22, &aMatrix._31, &aMatrix._32};
  for (uint32_t i = 0; i < 6; ++i) {
    JS::Rooted<JS::Value> elt(aCx);
    double d;
    if (!JS_GetElement(aCx, aObj, i, &elt)) {
      aError.Throw(NS_ERROR_FAILURE);
      return false;
    }
    if (!CoerceDouble(elt, &d)) {
      aError.Throw(NS_ERROR_INVALID_ARG);
      return false;
    }
    if (!FloatValidate(d)) {
      // This is weird, but it's the behavior of SetTransform()
      return false;
    }
    *elts[i] = Float(d);
  }
  return true;
}

void CanvasRenderingContext2D::SetMozCurrentTransform(
    JSContext* aCx, JS::Handle<JSObject*> aCurrentTransform,
    ErrorResult& aError) {
  EnsureTarget();
  if (!IsTargetValid()) {
    aError.Throw(NS_ERROR_FAILURE);
    return;
  }

  Matrix newCTM;
  if (ObjectToMatrix(aCx, aCurrentTransform, newCTM, aError) &&
      newCTM.IsFinite()) {
    mTarget->SetTransform(newCTM);
  }
}

void CanvasRenderingContext2D::GetMozCurrentTransform(
    JSContext* aCx, JS::MutableHandle<JSObject*> aResult, ErrorResult& aError) {
  EnsureTarget();

  MatrixToJSObject(aCx, mTarget ? mTarget->GetTransform() : Matrix(), aResult,
                   aError);
}

void CanvasRenderingContext2D::SetMozCurrentTransformInverse(
    JSContext* aCx, JS::Handle<JSObject*> aCurrentTransform,
    ErrorResult& aError) {
  EnsureTarget();
  if (!IsTargetValid()) {
    aError.Throw(NS_ERROR_FAILURE);
    return;
  }

  Matrix newCTMInverse;
  if (ObjectToMatrix(aCx, aCurrentTransform, newCTMInverse, aError)) {
    // XXX ERRMSG we need to report an error to developers here! (bug 329026)
    if (newCTMInverse.Invert() && newCTMInverse.IsFinite()) {
      mTarget->SetTransform(newCTMInverse);
    }
  }
}

void CanvasRenderingContext2D::GetMozCurrentTransformInverse(
    JSContext* aCx, JS::MutableHandle<JSObject*> aResult, ErrorResult& aError) {
  EnsureTarget();

  if (!mTarget) {
    MatrixToJSObject(aCx, Matrix(), aResult, aError);
    return;
  }

  Matrix ctm = mTarget->GetTransform();

  if (!ctm.Invert()) {
    double NaN = JS::GenericNaN();
    ctm = Matrix(NaN, NaN, NaN, NaN, NaN, NaN);
  }

  MatrixToJSObject(aCx, ctm, aResult, aError);
}

//
// colors
//

void CanvasRenderingContext2D::SetStyleFromUnion(
    const StringOrCanvasGradientOrCanvasPattern& aValue, Style aWhichStyle) {
  if (aValue.IsString()) {
    SetStyleFromString(aValue.GetAsString(), aWhichStyle);
    return;
  }

  if (aValue.IsCanvasGradient()) {
    SetStyleFromGradient(aValue.GetAsCanvasGradient(), aWhichStyle);
    return;
  }

  if (aValue.IsCanvasPattern()) {
    CanvasPattern& pattern = aValue.GetAsCanvasPattern();
    SetStyleFromPattern(pattern, aWhichStyle);
    if (pattern.mForceWriteOnly) {
      SetWriteOnly();
    }
    return;
  }

  MOZ_ASSERT_UNREACHABLE("Invalid union value");
}

void CanvasRenderingContext2D::SetFillRule(const nsAString& aString) {
  FillRule rule;

  if (aString.EqualsLiteral("evenodd"))
    rule = FillRule::FILL_EVEN_ODD;
  else if (aString.EqualsLiteral("nonzero"))
    rule = FillRule::FILL_WINDING;
  else
    return;

  CurrentState().fillRule = rule;
}

void CanvasRenderingContext2D::GetFillRule(nsAString& aString) {
  switch (CurrentState().fillRule) {
    case FillRule::FILL_WINDING:
      aString.AssignLiteral("nonzero");
      break;
    case FillRule::FILL_EVEN_ODD:
      aString.AssignLiteral("evenodd");
      break;
  }
}
//
// gradients and patterns
//
already_AddRefed<CanvasGradient> CanvasRenderingContext2D::CreateLinearGradient(
    double aX0, double aY0, double aX1, double aY1) {
  RefPtr<CanvasGradient> grad =
      new CanvasLinearGradient(this, Point(aX0, aY0), Point(aX1, aY1));

  return grad.forget();
}

already_AddRefed<CanvasGradient> CanvasRenderingContext2D::CreateRadialGradient(
    double aX0, double aY0, double aR0, double aX1, double aY1, double aR1,
    ErrorResult& aError) {
  if (aR0 < 0.0 || aR1 < 0.0) {
    aError.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return nullptr;
  }

  RefPtr<CanvasGradient> grad = new CanvasRadialGradient(
      this, Point(aX0, aY0), aR0, Point(aX1, aY1), aR1);

  return grad.forget();
}

already_AddRefed<CanvasPattern> CanvasRenderingContext2D::CreatePattern(
    const CanvasImageSource& aSource, const nsAString& aRepeat,
    ErrorResult& aError) {
  CanvasPattern::RepeatMode repeatMode = CanvasPattern::RepeatMode::NOREPEAT;

  if (aRepeat.IsEmpty() || aRepeat.EqualsLiteral("repeat")) {
    repeatMode = CanvasPattern::RepeatMode::REPEAT;
  } else if (aRepeat.EqualsLiteral("repeat-x")) {
    repeatMode = CanvasPattern::RepeatMode::REPEATX;
  } else if (aRepeat.EqualsLiteral("repeat-y")) {
    repeatMode = CanvasPattern::RepeatMode::REPEATY;
  } else if (aRepeat.EqualsLiteral("no-repeat")) {
    repeatMode = CanvasPattern::RepeatMode::NOREPEAT;
  } else {
    aError.Throw(NS_ERROR_DOM_SYNTAX_ERR);
    return nullptr;
  }

  Element* element;
  if (aSource.IsHTMLCanvasElement()) {
    HTMLCanvasElement* canvas = &aSource.GetAsHTMLCanvasElement();
    element = canvas;

    nsIntSize size = canvas->GetSize();
    if (size.width == 0) {
      aError.ThrowInvalidStateError("Passed-in canvas has width 0");
      return nullptr;
    }

    if (size.height == 0) {
      aError.ThrowInvalidStateError("Passed-in canvas has height 0");
      return nullptr;
    }

    // Special case for Canvas, which could be an Azure canvas!
    nsICanvasRenderingContextInternal* srcCanvas = canvas->GetContextAtIndex(0);
    if (srcCanvas) {
      // This might not be an Azure canvas!
      RefPtr<SourceSurface> srcSurf = srcCanvas->GetSurfaceSnapshot();
      if (!srcSurf) {
        JSContext* context = nsContentUtils::GetCurrentJSContext();
        if (context) {
          JS::WarnASCII(context,
                        "CanvasRenderingContext2D.createPattern() failed to "
                        "snapshot source canvas.");
        }
        aError.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
        return nullptr;
      }

      RefPtr<CanvasPattern> pat =
          new CanvasPattern(this, srcSurf, repeatMode, element->NodePrincipal(),
                            canvas->IsWriteOnly(), false);

      return pat.forget();
    }
  } else if (aSource.IsHTMLImageElement()) {
    HTMLImageElement* img = &aSource.GetAsHTMLImageElement();
    element = img;
  } else if (aSource.IsSVGImageElement()) {
    SVGImageElement* img = &aSource.GetAsSVGImageElement();
    element = img;
  } else if (aSource.IsHTMLVideoElement()) {
    auto& video = aSource.GetAsHTMLVideoElement();
    video.MarkAsContentSource(
        mozilla::dom::HTMLVideoElement::CallerAPI::CREATE_PATTERN);
    element = &video;
  } else {
    // Special case for ImageBitmap
    ImageBitmap& imgBitmap = aSource.GetAsImageBitmap();
    EnsureTarget();
    if (!IsTargetValid()) {
      aError.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
      return nullptr;
    }
    RefPtr<SourceSurface> srcSurf = imgBitmap.PrepareForDrawTarget(mTarget);
    if (!srcSurf) {
      aError.ThrowInvalidStateError(
          "Passed-in ImageBitmap has been transferred");
      return nullptr;
    }

    // An ImageBitmap never taints others so we set principalForSecurityCheck to
    // nullptr and set CORSUsed to true for passing the security check in
    // CanvasUtils::DoDrawImageSecurityCheck().
    RefPtr<CanvasPattern> pat = new CanvasPattern(
        this, srcSurf, repeatMode, nullptr, imgBitmap.IsWriteOnly(), true);

    return pat.forget();
  }

  EnsureTarget();
  if (!IsTargetValid()) {
    aError.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  // The canvas spec says that createPattern should use the first frame
  // of animated images
  nsLayoutUtils::SurfaceFromElementResult res =
      nsLayoutUtils::SurfaceFromElement(
          element, nsLayoutUtils::SFE_WANT_FIRST_FRAME_IF_IMAGE, mTarget);

  // Per spec, we should throw here for the HTMLImageElement and SVGImageElement
  // cases if the image request state is "broken".  In terms of the infromation
  // in "res", the "broken" state corresponds to not having a size and not being
  // still-loading (so there is no size forthcoming).
  if (aSource.IsHTMLImageElement() || aSource.IsSVGImageElement()) {
    if (!res.mIsStillLoading && !res.mHasSize) {
      aError.ThrowInvalidStateError(
          "Passed-in image's current request's state is \"broken\"");
      return nullptr;
    }

    if (res.mSize.width == 0 || res.mSize.height == 0) {
      return nullptr;
    }

    // Is the "fully decodable" check already done in SurfaceFromElement?  It's
    // not clear how to do it from here, exactly.
  }

  RefPtr<SourceSurface> surface = res.GetSourceSurface();
  if (!surface) {
    return nullptr;
  }

  RefPtr<CanvasPattern> pat =
      new CanvasPattern(this, surface, repeatMode, res.mPrincipal,
                        res.mIsWriteOnly, res.mCORSUsed);
  return pat.forget();
}

//
// shadows
//
void CanvasRenderingContext2D::SetShadowColor(const nsAString& aShadowColor) {
  nscolor color;
  if (!ParseColor(NS_ConvertUTF16toUTF8(aShadowColor), &color)) {
    return;
  }

  CurrentState().shadowColor = color;
}

//
// filters
//

static already_AddRefed<RawServoDeclarationBlock> CreateDeclarationForServo(
    nsCSSPropertyID aProperty, const nsAString& aPropertyValue,
    Document* aDocument) {
  nsCOMPtr<nsIReferrerInfo> referrerInfo =
      ReferrerInfo::CreateForInternalCSSResources(aDocument);

  RefPtr<URLExtraData> data = new URLExtraData(
      aDocument->GetDocBaseURI(), referrerInfo, aDocument->NodePrincipal());

  ServoCSSParser::ParsingEnvironment env(
      data, aDocument->GetCompatibilityMode(), aDocument->CSSLoader());
  RefPtr<RawServoDeclarationBlock> servoDeclarations =
      ServoCSSParser::ParseProperty(aProperty, aPropertyValue, env);

  if (!servoDeclarations) {
    // We got a syntax error.  The spec says this value must be ignored.
    return nullptr;
  }

  // From canvas spec, force to set line-height property to 'normal' font
  // property.
  if (aProperty == eCSSProperty_font) {
    const nsCString normalString = NS_LITERAL_CSTRING("normal");
    Servo_DeclarationBlock_SetPropertyById(
        servoDeclarations, eCSSProperty_line_height, &normalString, false, data,
        ParsingMode::Default, aDocument->GetCompatibilityMode(),
        aDocument->CSSLoader(), {});
  }

  return servoDeclarations.forget();
}

static already_AddRefed<RawServoDeclarationBlock> CreateFontDeclarationForServo(
    const nsAString& aFont, Document* aDocument) {
  return CreateDeclarationForServo(eCSSProperty_font, aFont, aDocument);
}

static already_AddRefed<ComputedStyle> GetFontStyleForServo(
    Element* aElement, const nsAString& aFont, PresShell* aPresShell,
    nsAString& aOutUsedFont, ErrorResult& aError) {
  RefPtr<RawServoDeclarationBlock> declarations =
      CreateFontDeclarationForServo(aFont, aPresShell->GetDocument());
  if (!declarations) {
    // We got a syntax error.  The spec says this value must be ignored.
    return nullptr;
  }

  // In addition to unparseable values, the spec says we need to reject
  // 'inherit' and 'initial'. The easiest way to check for this is to look
  // at font-size-adjust, which the font shorthand resets to 'none'.
  if (Servo_DeclarationBlock_HasCSSWideKeyword(declarations,
                                               eCSSProperty_font_size_adjust)) {
    return nullptr;
  }

  ServoStyleSet* styleSet = aPresShell->StyleSet();

  RefPtr<ComputedStyle> parentStyle;
  // have to get a parent ComputedStyle for inherit-like relative
  // values (2em, bolder, etc.)
  if (aElement && aElement->IsInComposedDoc()) {
    parentStyle = nsComputedDOMStyle::GetComputedStyle(aElement, nullptr);
    if (!parentStyle) {
      // The flush killed the shell, so we couldn't get any meaningful style
      // back.
      aError.Throw(NS_ERROR_FAILURE);
      return nullptr;
    }
  } else {
    RefPtr<RawServoDeclarationBlock> declarations =
        CreateFontDeclarationForServo(NS_LITERAL_STRING("10px sans-serif"),
                                      aPresShell->GetDocument());
    MOZ_ASSERT(declarations);

    parentStyle =
        aPresShell->StyleSet()->ResolveForDeclarations(nullptr, declarations);
  }

  MOZ_RELEASE_ASSERT(parentStyle, "Should have a valid parent style");

  MOZ_ASSERT(!aPresShell->IsDestroying(),
             "We should have returned an error above if the presshell is "
             "being destroyed.");

  RefPtr<ComputedStyle> sc =
      styleSet->ResolveForDeclarations(parentStyle, declarations);

  // The font getter is required to be reserialized based on what we
  // parsed (including having line-height removed).  (Older drafts of
  // the spec required font sizes be converted to pixels, but that no
  // longer seems to be required.)
  Servo_SerializeFontValueForCanvas(declarations, &aOutUsedFont);
  return sc.forget();
}

static already_AddRefed<RawServoDeclarationBlock>
CreateFilterDeclarationForServo(const nsAString& aFilter, Document* aDocument) {
  return CreateDeclarationForServo(eCSSProperty_filter, aFilter, aDocument);
}

static already_AddRefed<ComputedStyle> ResolveFilterStyleForServo(
    const nsAString& aFilterString, const ComputedStyle* aParentStyle,
    PresShell* aPresShell, ErrorResult& aError) {
  RefPtr<RawServoDeclarationBlock> declarations =
      CreateFilterDeclarationForServo(aFilterString, aPresShell->GetDocument());
  if (!declarations) {
    // Refuse to accept the filter, but do not throw an error.
    return nullptr;
  }

  // In addition to unparseable values, the spec says we need to reject
  // 'inherit' and 'initial'.
  if (Servo_DeclarationBlock_HasCSSWideKeyword(declarations,
                                               eCSSProperty_filter)) {
    return nullptr;
  }

  ServoStyleSet* styleSet = aPresShell->StyleSet();
  RefPtr<ComputedStyle> computedValues =
      styleSet->ResolveForDeclarations(aParentStyle, declarations);

  return computedValues.forget();
}

bool CanvasRenderingContext2D::ParseFilter(
    const nsAString& aString, StyleOwnedSlice<StyleFilter>& aFilterChain,
    ErrorResult& aError) {
  if (!mCanvasElement && !mDocShell) {
    NS_WARNING(
        "Canvas element must be non-null or a docshell must be provided");
    aError.Throw(NS_ERROR_FAILURE);
    return false;
  }

  RefPtr<PresShell> presShell = GetPresShell();
  if (!presShell) {
    aError.Throw(NS_ERROR_FAILURE);
    return false;
  }

  nsAutoString usedFont;  // unused

  RefPtr<ComputedStyle> parentStyle = GetFontStyleForServo(
      mCanvasElement, GetFont(), presShell, usedFont, aError);
  if (!parentStyle) {
    return false;
  }

  RefPtr<ComputedStyle> style =
      ResolveFilterStyleForServo(aString, parentStyle, presShell, aError);
  if (!style) {
    return false;
  }

  aFilterChain = style->StyleEffects()->mFilters;
  return true;
}

void CanvasRenderingContext2D::SetFilter(const nsAString& aFilter,
                                         ErrorResult& aError) {
  StyleOwnedSlice<StyleFilter> filterChain;
  if (ParseFilter(aFilter, filterChain, aError)) {
    CurrentState().filterString = aFilter;
    CurrentState().filterChain = std::move(filterChain);
    if (mCanvasElement) {
      CurrentState().autoSVGFiltersObserver =
          SVGObserverUtils::ObserveFiltersForCanvasContext(
              this, mCanvasElement, CurrentState().filterChain.AsSpan());
      UpdateFilter();
    }
  }
}

class CanvasUserSpaceMetrics : public UserSpaceMetricsWithSize {
 public:
  CanvasUserSpaceMetrics(const gfx::IntSize& aSize, const nsFont& aFont,
                         nsAtom* aFontLanguage, bool aExplicitLanguage,
                         nsPresContext* aPresContext)
      : mSize(aSize),
        mFont(aFont),
        mFontLanguage(aFontLanguage),
        mExplicitLanguage(aExplicitLanguage),
        mPresContext(aPresContext) {}

  virtual float GetEmLength() const override {
    return NSAppUnitsToFloatPixels(mFont.size, AppUnitsPerCSSPixel());
  }

  virtual float GetExLength() const override {
    nsDeviceContext* dc = mPresContext->DeviceContext();
    nsFontMetrics::Params params;
    params.language = mFontLanguage;
    params.explicitLanguage = mExplicitLanguage;
    params.textPerf = mPresContext->GetTextPerfMetrics();
    params.fontStats = mPresContext->GetFontMatchingStats();
    params.featureValueLookup = mPresContext->GetFontFeatureValuesLookup();
    RefPtr<nsFontMetrics> fontMetrics = dc->GetMetricsFor(mFont, params);
    return NSAppUnitsToFloatPixels(fontMetrics->XHeight(),
                                   AppUnitsPerCSSPixel());
  }

  virtual gfx::Size GetSize() const override { return Size(mSize); }

 private:
  gfx::IntSize mSize;
  const nsFont& mFont;
  nsAtom* mFontLanguage;
  bool mExplicitLanguage;
  nsPresContext* mPresContext;
};

// The filter might reference an SVG filter that is declared inside this
// document. Flush frames so that we'll have an nsSVGFilterFrame to work
// with.
static bool FiltersNeedFrameFlush(Span<const StyleFilter> aFilters) {
  for (const auto& filter : aFilters) {
    if (filter.IsUrl()) {
      return true;
    }
  }
  return false;
}

void CanvasRenderingContext2D::UpdateFilter() {
  RefPtr<PresShell> presShell = GetPresShell();
  if (!presShell || presShell->IsDestroying()) {
    // Ensure we set an empty filter and update the state to
    // reflect the current "taint" status of the canvas
    CurrentState().filter = FilterDescription();
    CurrentState().filterSourceGraphicTainted =
        mCanvasElement && mCanvasElement->IsWriteOnly();
    return;
  }

  if (FiltersNeedFrameFlush(CurrentState().filterChain.AsSpan())) {
    presShell->FlushPendingNotifications(FlushType::Frames);
  }

  MOZ_RELEASE_ASSERT(!mStyleStack.IsEmpty());
  if (MOZ_UNLIKELY(presShell->IsDestroying())) {
    return;
  }

  const bool sourceGraphicIsTainted =
      mCanvasElement && mCanvasElement->IsWriteOnly();

  CurrentState().filter = nsFilterInstance::GetFilterDescription(
      mCanvasElement, CurrentState().filterChain.AsSpan(),
      sourceGraphicIsTainted,
      CanvasUserSpaceMetrics(
          GetSize(), CurrentState().fontFont, CurrentState().fontLanguage,
          CurrentState().fontExplicitLanguage, presShell->GetPresContext()),
      gfxRect(0, 0, mWidth, mHeight), CurrentState().filterAdditionalImages);
  CurrentState().filterSourceGraphicTainted = sourceGraphicIsTainted;
}

//
// rects
//

static bool ValidateRect(double& aX, double& aY, double& aWidth,
                         double& aHeight, bool aIsZeroSizeValid) {
  if (!aIsZeroSizeValid && (aWidth == 0.0 || aHeight == 0.0)) {
    return false;
  }

  // bug 1018527
  // The values of canvas API input are in double precision, but Moz2D APIs are
  // using float precision. Bypass canvas API calls when the input is out of
  // float precision to avoid precision problem
  if (!std::isfinite((float)aX) | !std::isfinite((float)aY) |
      !std::isfinite((float)aWidth) | !std::isfinite((float)aHeight)) {
    return false;
  }

  // bug 1074733
  // The canvas spec does not forbid rects with negative w or h, so given
  // corners (x, y), (x+w, y), (x+w, y+h), and (x, y+h) we must generate
  // the appropriate rect by flipping negative dimensions. This prevents
  // draw targets from receiving "empty" rects later on.
  if (aWidth < 0) {
    aWidth = -aWidth;
    aX -= aWidth;
  }
  if (aHeight < 0) {
    aHeight = -aHeight;
    aY -= aHeight;
  }
  return true;
}

void CanvasRenderingContext2D::ClearRect(double aX, double aY, double aW,
                                         double aH) {
  // Do not allow zeros - it's a no-op at that point per spec.
  if (!ValidateRect(aX, aY, aW, aH, false)) {
    return;
  }

  gfx::Rect clearRect(aX, aY, aW, aH);

  EnsureTarget(&clearRect, true);
  if (!IsTargetValid()) {
    return;
  }

  mTarget->ClearRect(clearRect);

  RedrawUser(gfxRect(aX, aY, aW, aH));
}

void CanvasRenderingContext2D::FillRect(double aX, double aY, double aW,
                                        double aH) {
  if (!ValidateRect(aX, aY, aW, aH, true)) {
    return;
  }

  const ContextState* state = &CurrentState();
  if (state->patternStyles[Style::FILL]) {
    CanvasPattern::RepeatMode repeat =
        state->patternStyles[Style::FILL]->mRepeat;
    // In the FillRect case repeat modes are easy to deal with.
    bool limitx = repeat == CanvasPattern::RepeatMode::NOREPEAT ||
                  repeat == CanvasPattern::RepeatMode::REPEATY;
    bool limity = repeat == CanvasPattern::RepeatMode::NOREPEAT ||
                  repeat == CanvasPattern::RepeatMode::REPEATX;

    IntSize patternSize =
        state->patternStyles[Style::FILL]->mSurface->GetSize();

    // We always need to execute painting for non-over operators, even if
    // we end up with w/h = 0.
    if (limitx) {
      if (aX < 0) {
        aW += aX;
        if (aW < 0) {
          aW = 0;
        }

        aX = 0;
      }
      if (aX + aW > patternSize.width) {
        aW = patternSize.width - aX;
        if (aW < 0) {
          aW = 0;
        }
      }
    }
    if (limity) {
      if (aY < 0) {
        aH += aY;
        if (aH < 0) {
          aH = 0;
        }

        aY = 0;
      }
      if (aY + aH > patternSize.height) {
        aH = patternSize.height - aY;
        if (aH < 0) {
          aH = 0;
        }
      }
    }
  }
  state = nullptr;

  CompositionOp op = UsedOperation();
  bool isColor;
  bool discardContent =
      PatternIsOpaque(Style::FILL, &isColor) &&
      (op == CompositionOp::OP_OVER || op == CompositionOp::OP_SOURCE);
  const gfx::Rect fillRect(aX, aY, aW, aH);
  EnsureTarget(discardContent ? &fillRect : nullptr, discardContent && isColor);
  if (!IsTargetValid()) {
    return;
  }

  gfx::Rect bounds;
  const bool needBounds = NeedToCalculateBounds();
  if (!IsTargetValid()) {
    return;
  }
  if (needBounds) {
    bounds = mTarget->GetTransform().TransformBounds(fillRect);
  }

  AntialiasMode antialiasMode = CurrentState().imageSmoothingEnabled
                                    ? AntialiasMode::DEFAULT
                                    : AntialiasMode::NONE;

  AdjustedTarget target(this, bounds.IsEmpty() ? nullptr : &bounds);
  if (!target) {
    return;
  }
  target->FillRect(gfx::Rect(aX, aY, aW, aH),
                   CanvasGeneralPattern().ForStyle(this, Style::FILL, mTarget),
                   DrawOptions(CurrentState().globalAlpha, op, antialiasMode));

  RedrawUser(gfxRect(aX, aY, aW, aH));
}

void CanvasRenderingContext2D::StrokeRect(double aX, double aY, double aW,
                                          double aH) {
  if (!aW && !aH) {
    return;
  }

  if (!ValidateRect(aX, aY, aW, aH, true)) {
    return;
  }

  EnsureTarget();
  if (!IsTargetValid()) {
    return;
  }

  const bool needBounds = NeedToCalculateBounds();
  if (!IsTargetValid()) {
    return;
  }

  gfx::Rect bounds;
  if (needBounds) {
    const ContextState& state = CurrentState();
    bounds = gfx::Rect(aX - state.lineWidth / 2.0f, aY - state.lineWidth / 2.0f,
                       aW + state.lineWidth, aH + state.lineWidth);
    bounds = mTarget->GetTransform().TransformBounds(bounds);
  }

  auto op = UsedOperation();
  if (!IsTargetValid()) {
    return;
  }

  if (!aH) {
    CapStyle cap = CapStyle::BUTT;
    if (CurrentState().lineJoin == JoinStyle::ROUND) {
      cap = CapStyle::ROUND;
    }
    AdjustedTarget target(this, bounds.IsEmpty() ? nullptr : &bounds);
    if (!target) {
      return;
    }

    const ContextState& state = CurrentState();
    target->StrokeLine(
        Point(aX, aY), Point(aX + aW, aY),
        CanvasGeneralPattern().ForStyle(this, Style::STROKE, mTarget),
        StrokeOptions(state.lineWidth, state.lineJoin, cap, state.miterLimit,
                      state.dash.Length(), state.dash.Elements(),
                      state.dashOffset),
        DrawOptions(state.globalAlpha, op));
    return;
  }

  if (!aW) {
    CapStyle cap = CapStyle::BUTT;
    if (CurrentState().lineJoin == JoinStyle::ROUND) {
      cap = CapStyle::ROUND;
    }
    AdjustedTarget target(this, bounds.IsEmpty() ? nullptr : &bounds);
    if (!target) {
      return;
    }

    const ContextState& state = CurrentState();
    target->StrokeLine(
        Point(aX, aY), Point(aX, aY + aH),
        CanvasGeneralPattern().ForStyle(this, Style::STROKE, mTarget),
        StrokeOptions(state.lineWidth, state.lineJoin, cap, state.miterLimit,
                      state.dash.Length(), state.dash.Elements(),
                      state.dashOffset),
        DrawOptions(state.globalAlpha, op));
    return;
  }

  AdjustedTarget target(this, bounds.IsEmpty() ? nullptr : &bounds);
  if (!target) {
    return;
  }

  const ContextState& state = CurrentState();
  target->StrokeRect(
      gfx::Rect(aX, aY, aW, aH),
      CanvasGeneralPattern().ForStyle(this, Style::STROKE, mTarget),
      StrokeOptions(state.lineWidth, state.lineJoin, state.lineCap,
                    state.miterLimit, state.dash.Length(),
                    state.dash.Elements(), state.dashOffset),
      DrawOptions(state.globalAlpha, op));

  Redraw();
}

//
// path bits
//

void CanvasRenderingContext2D::BeginPath() {
  mPath = nullptr;
  mPathBuilder = nullptr;
  mDSPathBuilder = nullptr;
  mPathTransformWillUpdate = false;
}

void CanvasRenderingContext2D::Fill(const CanvasWindingRule& aWinding) {
  EnsureUserSpacePath(aWinding);

  if (!mPath) {
    return;
  }

  const bool needBounds = NeedToCalculateBounds();
  if (!IsTargetValid()) {
    return;
  }
  gfx::Rect bounds;
  if (needBounds) {
    bounds = mPath->GetBounds(mTarget->GetTransform());
  }

  AdjustedTarget target(this, bounds.IsEmpty() ? nullptr : &bounds);
  if (!target) {
    return;
  }

  auto op = UsedOperation();
  if (!IsTargetValid() || !target) {
    return;
  }
  target->Fill(mPath,
               CanvasGeneralPattern().ForStyle(this, Style::FILL, mTarget),
               DrawOptions(CurrentState().globalAlpha, op));
  Redraw();
}

void CanvasRenderingContext2D::Fill(const CanvasPath& aPath,
                                    const CanvasWindingRule& aWinding) {
  EnsureTarget();
  if (!IsTargetValid()) {
    return;
  }

  RefPtr<gfx::Path> gfxpath = aPath.GetPath(aWinding, mTarget);
  if (!gfxpath) {
    return;
  }

  const bool needBounds = NeedToCalculateBounds();
  if (!IsTargetValid()) {
    return;
  }
  gfx::Rect bounds;
  if (needBounds) {
    bounds = gfxpath->GetBounds(mTarget->GetTransform());
  }

  AdjustedTarget target(this, bounds.IsEmpty() ? nullptr : &bounds);
  if (!target) {
    return;
  }

  auto op = UsedOperation();
  if (!IsTargetValid() || !target) {
    return;
  }
  target->Fill(gfxpath,
               CanvasGeneralPattern().ForStyle(this, Style::FILL, mTarget),
               DrawOptions(CurrentState().globalAlpha, op));
  Redraw();
}

void CanvasRenderingContext2D::Stroke() {
  EnsureUserSpacePath();

  if (!mPath) {
    return;
  }

  const ContextState* state = &CurrentState();
  StrokeOptions strokeOptions(state->lineWidth, state->lineJoin, state->lineCap,
                              state->miterLimit, state->dash.Length(),
                              state->dash.Elements(), state->dashOffset);
  state = nullptr;

  const bool needBounds = NeedToCalculateBounds();
  if (!IsTargetValid()) {
    return;
  }
  gfx::Rect bounds;
  if (needBounds) {
    bounds = mPath->GetStrokedBounds(strokeOptions, mTarget->GetTransform());
  }

  AdjustedTarget target(this, bounds.IsEmpty() ? nullptr : &bounds);
  if (!target) {
    return;
  }

  auto op = UsedOperation();
  if (!IsTargetValid() || !target) {
    return;
  }
  target->Stroke(mPath,
                 CanvasGeneralPattern().ForStyle(this, Style::STROKE, mTarget),
                 strokeOptions, DrawOptions(CurrentState().globalAlpha, op));
  Redraw();
}

void CanvasRenderingContext2D::Stroke(const CanvasPath& aPath) {
  EnsureTarget();
  if (!IsTargetValid()) {
    return;
  }

  RefPtr<gfx::Path> gfxpath =
      aPath.GetPath(CanvasWindingRule::Nonzero, mTarget);

  if (!gfxpath) {
    return;
  }

  const ContextState* state = &CurrentState();
  StrokeOptions strokeOptions(state->lineWidth, state->lineJoin, state->lineCap,
                              state->miterLimit, state->dash.Length(),
                              state->dash.Elements(), state->dashOffset);
  state = nullptr;

  const bool needBounds = NeedToCalculateBounds();
  if (!IsTargetValid()) {
    return;
  }
  gfx::Rect bounds;
  if (needBounds) {
    bounds = gfxpath->GetStrokedBounds(strokeOptions, mTarget->GetTransform());
  }

  AdjustedTarget target(this, bounds.IsEmpty() ? nullptr : &bounds);
  if (!target) {
    return;
  }

  auto op = UsedOperation();
  if (!IsTargetValid() || !target) {
    return;
  }
  target->Stroke(gfxpath,
                 CanvasGeneralPattern().ForStyle(this, Style::STROKE, mTarget),
                 strokeOptions, DrawOptions(CurrentState().globalAlpha, op));
  Redraw();
}

void CanvasRenderingContext2D::DrawFocusIfNeeded(
    mozilla::dom::Element& aElement, ErrorResult& aRv) {
  EnsureUserSpacePath();
  if (!mPath) {
    return;
  }

  if (DrawCustomFocusRing(aElement)) {
    AutoSaveRestore asr(this);

    // set state to conforming focus state
    ContextState* state = &CurrentState();
    state->globalAlpha = 1.0;
    state->shadowBlur = 0;
    state->shadowOffset.x = 0;
    state->shadowOffset.y = 0;
    state->op = mozilla::gfx::CompositionOp::OP_OVER;

    state->lineCap = CapStyle::BUTT;
    state->lineJoin = mozilla::gfx::JoinStyle::MITER_OR_BEVEL;
    state->lineWidth = 1;
    state->dash.Clear();

    // color and style of the rings is the same as for image maps
    // set the background focus color
    state->SetColorStyle(Style::STROKE, NS_RGBA(255, 255, 255, 255));
    state = nullptr;

    // draw the focus ring
    Stroke();
    if (!mPath) {
      return;
    }

    // set dashing for foreground
    nsTArray<mozilla::gfx::Float>& dash = CurrentState().dash;
    for (uint32_t i = 0; i < 2; ++i) {
      if (!dash.AppendElement(1, fallible)) {
        aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
        return;
      }
    }

    // set the foreground focus color
    CurrentState().SetColorStyle(Style::STROKE, NS_RGBA(0, 0, 0, 255));
    // draw the focus ring
    Stroke();
    if (!mPath) {
      return;
    }
  }
}

bool CanvasRenderingContext2D::DrawCustomFocusRing(
    mozilla::dom::Element& aElement) {
  EnsureUserSpacePath();

  HTMLCanvasElement* canvas = GetCanvas();

  if (!canvas || !aElement.IsInclusiveDescendantOf(canvas)) {
    return false;
  }

  if (nsFocusManager* fm = nsFocusManager::GetFocusManager()) {
    // check that the element is focused
    if (&aElement == fm->GetFocusedElement()) {
      if (nsPIDOMWindowOuter* window = aElement.OwnerDoc()->GetWindow()) {
        return window->ShouldShowFocusRing();
      }
    }
  }

  return false;
}

void CanvasRenderingContext2D::Clip(const CanvasWindingRule& aWinding) {
  EnsureUserSpacePath(aWinding);

  if (!mPath) {
    return;
  }

  mTarget->PushClip(mPath);
  CurrentState().clipsAndTransforms.AppendElement(ClipState(mPath));
}

void CanvasRenderingContext2D::Clip(const CanvasPath& aPath,
                                    const CanvasWindingRule& aWinding) {
  EnsureTarget();
  if (!IsTargetValid()) {
    return;
  }

  RefPtr<gfx::Path> gfxpath = aPath.GetPath(aWinding, mTarget);

  if (!gfxpath) {
    return;
  }

  mTarget->PushClip(gfxpath);
  CurrentState().clipsAndTransforms.AppendElement(ClipState(gfxpath));
}

void CanvasRenderingContext2D::ArcTo(double aX1, double aY1, double aX2,
                                     double aY2, double aRadius,
                                     ErrorResult& aError) {
  if (aRadius < 0) {
    aError.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return;
  }

  EnsureWritablePath();

  // Current point in user space!
  Point p0;
  if (mPathBuilder) {
    p0 = mPathBuilder->CurrentPoint();
  } else {
    Matrix invTransform = mTarget->GetTransform();
    if (!invTransform.Invert()) {
      return;
    }

    p0 = invTransform.TransformPoint(mDSPathBuilder->CurrentPoint());
  }

  Point p1(aX1, aY1);
  Point p2(aX2, aY2);

  // Execute these calculations in double precision to avoid cumulative
  // rounding errors.
  double dir, a2, b2, c2, cosx, sinx, d, anx, any, bnx, bny, x3, y3, x4, y4, cx,
      cy, angle0, angle1;
  bool anticlockwise;

  if (p0 == p1 || p1 == p2 || aRadius == 0) {
    LineTo(p1.x, p1.y);
    return;
  }

  // Check for colinearity
  dir = (p2.x - p1.x) * (p0.y - p1.y) + (p2.y - p1.y) * (p1.x - p0.x);
  if (dir == 0) {
    LineTo(p1.x, p1.y);
    return;
  }

  // XXX - Math for this code was already available from the non-azure code
  // and would be well tested. Perhaps converting to bezier directly might
  // be more efficient longer run.
  a2 = (p0.x - aX1) * (p0.x - aX1) + (p0.y - aY1) * (p0.y - aY1);
  b2 = (aX1 - aX2) * (aX1 - aX2) + (aY1 - aY2) * (aY1 - aY2);
  c2 = (p0.x - aX2) * (p0.x - aX2) + (p0.y - aY2) * (p0.y - aY2);
  cosx = (a2 + b2 - c2) / (2 * sqrt(a2 * b2));

  sinx = sqrt(1 - cosx * cosx);
  d = aRadius / ((1 - cosx) / sinx);

  anx = (aX1 - p0.x) / sqrt(a2);
  any = (aY1 - p0.y) / sqrt(a2);
  bnx = (aX1 - aX2) / sqrt(b2);
  bny = (aY1 - aY2) / sqrt(b2);
  x3 = aX1 - anx * d;
  y3 = aY1 - any * d;
  x4 = aX1 - bnx * d;
  y4 = aY1 - bny * d;
  anticlockwise = (dir < 0);
  cx = x3 + any * aRadius * (anticlockwise ? 1 : -1);
  cy = y3 - anx * aRadius * (anticlockwise ? 1 : -1);
  angle0 = atan2((y3 - cy), (x3 - cx));
  angle1 = atan2((y4 - cy), (x4 - cx));

  LineTo(x3, y3);

  Arc(cx, cy, aRadius, angle0, angle1, anticlockwise, aError);
}

void CanvasRenderingContext2D::Arc(double aX, double aY, double aR,
                                   double aStartAngle, double aEndAngle,
                                   bool aAnticlockwise, ErrorResult& aError) {
  if (aR < 0.0) {
    aError.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return;
  }

  EnsureWritablePath();

  ArcToBezier(this, Point(aX, aY), Size(aR, aR), aStartAngle, aEndAngle,
              aAnticlockwise);
}

void CanvasRenderingContext2D::Rect(double aX, double aY, double aW,
                                    double aH) {
  EnsureWritablePath();

  if (mPathBuilder) {
    mPathBuilder->MoveTo(Point(aX, aY));
    mPathBuilder->LineTo(Point(aX + aW, aY));
    mPathBuilder->LineTo(Point(aX + aW, aY + aH));
    mPathBuilder->LineTo(Point(aX, aY + aH));
    mPathBuilder->Close();
  } else {
    mDSPathBuilder->MoveTo(
        mTarget->GetTransform().TransformPoint(Point(aX, aY)));
    mDSPathBuilder->LineTo(
        mTarget->GetTransform().TransformPoint(Point(aX + aW, aY)));
    mDSPathBuilder->LineTo(
        mTarget->GetTransform().TransformPoint(Point(aX + aW, aY + aH)));
    mDSPathBuilder->LineTo(
        mTarget->GetTransform().TransformPoint(Point(aX, aY + aH)));
    mDSPathBuilder->Close();
  }
}

void CanvasRenderingContext2D::Ellipse(double aX, double aY, double aRadiusX,
                                       double aRadiusY, double aRotation,
                                       double aStartAngle, double aEndAngle,
                                       bool aAnticlockwise,
                                       ErrorResult& aError) {
  if (aRadiusX < 0.0 || aRadiusY < 0.0) {
    aError.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return;
  }

  EnsureWritablePath();

  ArcToBezier(this, Point(aX, aY), Size(aRadiusX, aRadiusY), aStartAngle,
              aEndAngle, aAnticlockwise, aRotation);
}

void CanvasRenderingContext2D::EnsureWritablePath() {
  EnsureTarget();
  // NOTE: IsTargetValid() may be false here (mTarget == sErrorTarget) but we
  // go ahead and create a path anyway since callers depend on that.

  if (mDSPathBuilder) {
    return;
  }

  FillRule fillRule = CurrentState().fillRule;

  if (mPathBuilder) {
    if (mPathTransformWillUpdate) {
      mPath = mPathBuilder->Finish();
      mDSPathBuilder = mPath->TransformedCopyToBuilder(mPathToDS, fillRule);
      mPath = nullptr;
      mPathBuilder = nullptr;
      mPathTransformWillUpdate = false;
    }
    return;
  }

  if (!mPath) {
    NS_ASSERTION(
        !mPathTransformWillUpdate,
        "mPathTransformWillUpdate should be false, if all paths are null");
    mPathBuilder = mTarget->CreatePathBuilder(fillRule);
  } else if (!mPathTransformWillUpdate) {
    mPathBuilder = mPath->CopyToBuilder(fillRule);
  } else {
    mDSPathBuilder = mPath->TransformedCopyToBuilder(mPathToDS, fillRule);
    mPathTransformWillUpdate = false;
    mPath = nullptr;
  }
}

void CanvasRenderingContext2D::EnsureUserSpacePath(
    const CanvasWindingRule& aWinding) {
  FillRule fillRule = CurrentState().fillRule;
  if (aWinding == CanvasWindingRule::Evenodd)
    fillRule = FillRule::FILL_EVEN_ODD;

  EnsureTarget();
  if (!IsTargetValid()) {
    return;
  }

  if (!mPath && !mPathBuilder && !mDSPathBuilder) {
    mPathBuilder = mTarget->CreatePathBuilder(fillRule);
  }

  if (mPathBuilder) {
    mPath = mPathBuilder->Finish();
    mPathBuilder = nullptr;
  }

  if (mPath && mPathTransformWillUpdate) {
    mDSPathBuilder = mPath->TransformedCopyToBuilder(mPathToDS, fillRule);
    mPath = nullptr;
    mPathTransformWillUpdate = false;
  }

  if (mDSPathBuilder) {
    RefPtr<Path> dsPath;
    dsPath = mDSPathBuilder->Finish();
    mDSPathBuilder = nullptr;

    Matrix inverse = mTarget->GetTransform();
    if (!inverse.Invert()) {
      NS_WARNING("Could not invert transform");
      return;
    }

    mPathBuilder = dsPath->TransformedCopyToBuilder(inverse, fillRule);
    mPath = mPathBuilder->Finish();
    mPathBuilder = nullptr;
  }

  if (mPath && mPath->GetFillRule() != fillRule) {
    mPathBuilder = mPath->CopyToBuilder(fillRule);
    mPath = mPathBuilder->Finish();
    mPathBuilder = nullptr;
  }

  NS_ASSERTION(mPath, "mPath should exist");
}

void CanvasRenderingContext2D::TransformWillUpdate() {
  EnsureTarget();
  if (!IsTargetValid()) {
    return;
  }

  // Store the matrix that would transform the current path to device
  // space.
  if (mPath || mPathBuilder) {
    if (!mPathTransformWillUpdate) {
      // If the transform has already been updated, but a device space builder
      // has not been created yet mPathToDS contains the right transform to
      // transform the current mPath into device space.
      // We should leave it alone.
      mPathToDS = mTarget->GetTransform();
    }
    mPathTransformWillUpdate = true;
  }
}

//
// text
//

void CanvasRenderingContext2D::SetFont(const nsAString& aFont,
                                       ErrorResult& aError) {
  SetFontInternal(aFont, aError);
}

bool CanvasRenderingContext2D::SetFontInternal(const nsAString& aFont,
                                               ErrorResult& aError) {
  /*
   * If font is defined with relative units (e.g. ems) and the parent
   * ComputedStyle changes in between calls, setting the font to the
   * same value as previous could result in a different computed value,
   * so we cannot have the optimization where we check if the new font
   * string is equal to the old one.
   */

  if (!mCanvasElement && !mDocShell) {
    NS_WARNING(
        "Canvas element must be non-null or a docshell must be provided");
    aError.Throw(NS_ERROR_FAILURE);
    return false;
  }

  RefPtr<PresShell> presShell = GetPresShell();
  if (!presShell) {
    aError.Throw(NS_ERROR_FAILURE);
    return false;
  }

  nsString usedFont;
  RefPtr<ComputedStyle> sc =
      GetFontStyleForServo(mCanvasElement, aFont, presShell, usedFont, aError);
  if (!sc) {
    return false;
  }

  const nsStyleFont* fontStyle = sc->StyleFont();
  nsPresContext* c = presShell->GetPresContext();

  // Purposely ignore the font size that respects the user's minimum
  // font preference (fontStyle->mFont.size) in favor of the computed
  // size (fontStyle->mSize).  See
  // https://bugzilla.mozilla.org/show_bug.cgi?id=698652.
  // FIXME: Nobody initializes mAllowZoom for servo?
  // MOZ_ASSERT(!fontStyle->mAllowZoom,
  //           "expected text zoom to be disabled on this nsStyleFont");
  nsFont resizedFont(fontStyle->mFont);
  // Create a font group working in units of CSS pixels instead of the usual
  // device pixels, to avoid being affected by page zoom. nsFontMetrics will
  // convert nsFont size in app units to device pixels for the font group, so
  // here we first apply to the size the equivalent of a conversion from device
  // pixels to CSS pixels, to adjust for the difference in expectations from
  // other nsFontMetrics clients.
  resizedFont.size =
      (fontStyle->mSize * c->AppUnitsPerDevPixel()) / AppUnitsPerCSSPixel();

  c->Document()->FlushUserFontSet();

  nsFontMetrics::Params params;
  params.language = fontStyle->mLanguage;
  params.explicitLanguage = fontStyle->mExplicitLanguage;
  params.userFontSet = c->GetUserFontSet();
  params.textPerf = c->GetTextPerfMetrics();
  params.fontStats = c->GetFontMatchingStats();
  RefPtr<nsFontMetrics> metrics =
      c->DeviceContext()->GetMetricsFor(resizedFont, params);

  gfxFontGroup* newFontGroup = metrics->GetThebesFontGroup();
  CurrentState().fontGroup = newFontGroup;
  NS_ASSERTION(CurrentState().fontGroup, "Could not get font group");
  CurrentState().font = usedFont;
  CurrentState().fontFont = fontStyle->mFont;
  CurrentState().fontFont.size = fontStyle->mSize;
  CurrentState().fontLanguage = fontStyle->mLanguage;
  CurrentState().fontExplicitLanguage = fontStyle->mExplicitLanguage;

  return true;
}

void CanvasRenderingContext2D::SetTextAlign(const nsAString& aTextAlign) {
  if (aTextAlign.EqualsLiteral("start"))
    CurrentState().textAlign = TextAlign::START;
  else if (aTextAlign.EqualsLiteral("end"))
    CurrentState().textAlign = TextAlign::END;
  else if (aTextAlign.EqualsLiteral("left"))
    CurrentState().textAlign = TextAlign::LEFT;
  else if (aTextAlign.EqualsLiteral("right"))
    CurrentState().textAlign = TextAlign::RIGHT;
  else if (aTextAlign.EqualsLiteral("center"))
    CurrentState().textAlign = TextAlign::CENTER;
}

void CanvasRenderingContext2D::GetTextAlign(nsAString& aTextAlign) {
  switch (CurrentState().textAlign) {
    case TextAlign::START:
      aTextAlign.AssignLiteral("start");
      break;
    case TextAlign::END:
      aTextAlign.AssignLiteral("end");
      break;
    case TextAlign::LEFT:
      aTextAlign.AssignLiteral("left");
      break;
    case TextAlign::RIGHT:
      aTextAlign.AssignLiteral("right");
      break;
    case TextAlign::CENTER:
      aTextAlign.AssignLiteral("center");
      break;
  }
}

void CanvasRenderingContext2D::SetTextBaseline(const nsAString& aTextBaseline) {
  if (aTextBaseline.EqualsLiteral("top"))
    CurrentState().textBaseline = TextBaseline::TOP;
  else if (aTextBaseline.EqualsLiteral("hanging"))
    CurrentState().textBaseline = TextBaseline::HANGING;
  else if (aTextBaseline.EqualsLiteral("middle"))
    CurrentState().textBaseline = TextBaseline::MIDDLE;
  else if (aTextBaseline.EqualsLiteral("alphabetic"))
    CurrentState().textBaseline = TextBaseline::ALPHABETIC;
  else if (aTextBaseline.EqualsLiteral("ideographic"))
    CurrentState().textBaseline = TextBaseline::IDEOGRAPHIC;
  else if (aTextBaseline.EqualsLiteral("bottom"))
    CurrentState().textBaseline = TextBaseline::BOTTOM;
}

void CanvasRenderingContext2D::GetTextBaseline(nsAString& aTextBaseline) {
  switch (CurrentState().textBaseline) {
    case TextBaseline::TOP:
      aTextBaseline.AssignLiteral("top");
      break;
    case TextBaseline::HANGING:
      aTextBaseline.AssignLiteral("hanging");
      break;
    case TextBaseline::MIDDLE:
      aTextBaseline.AssignLiteral("middle");
      break;
    case TextBaseline::ALPHABETIC:
      aTextBaseline.AssignLiteral("alphabetic");
      break;
    case TextBaseline::IDEOGRAPHIC:
      aTextBaseline.AssignLiteral("ideographic");
      break;
    case TextBaseline::BOTTOM:
      aTextBaseline.AssignLiteral("bottom");
      break;
  }
}

/*
 * Helper function that replaces the whitespace characters in a string
 * with U+0020 SPACE. The whitespace characters are defined as U+0020 SPACE,
 * U+0009 CHARACTER TABULATION (tab), U+000A LINE FEED (LF), U+000B LINE
 * TABULATION, U+000C FORM FEED (FF), and U+000D CARRIAGE RETURN (CR).
 * @param str The string whose whitespace characters to replace.
 */
static inline void TextReplaceWhitespaceCharacters(nsAutoString& aStr) {
  aStr.ReplaceChar("\x09\x0A\x0B\x0C\x0D", char16_t(' '));
}

void CanvasRenderingContext2D::FillText(const nsAString& aText, double aX,
                                        double aY,
                                        const Optional<double>& aMaxWidth,
                                        ErrorResult& aError) {
  DebugOnly<TextMetrics*> metrics = DrawOrMeasureText(
      aText, aX, aY, aMaxWidth, TextDrawOperation::FILL, aError);
  MOZ_ASSERT(!metrics);  // drawing operation never returns TextMetrics
}

void CanvasRenderingContext2D::StrokeText(const nsAString& aText, double aX,
                                          double aY,
                                          const Optional<double>& aMaxWidth,
                                          ErrorResult& aError) {
  DebugOnly<TextMetrics*> metrics = DrawOrMeasureText(
      aText, aX, aY, aMaxWidth, TextDrawOperation::STROKE, aError);
  MOZ_ASSERT(!metrics);  // drawing operation never returns TextMetrics
}

TextMetrics* CanvasRenderingContext2D::MeasureText(const nsAString& aRawText,
                                                   ErrorResult& aError) {
  Optional<double> maxWidth;
  return DrawOrMeasureText(aRawText, 0, 0, maxWidth, TextDrawOperation::MEASURE,
                           aError);
}

void CanvasRenderingContext2D::AddHitRegion(const HitRegionOptions& aOptions,
                                            ErrorResult& aError) {
  RefPtr<gfx::Path> path;
  if (aOptions.mPath) {
    EnsureTarget();
    if (!IsTargetValid()) {
      return;
    }
    path = aOptions.mPath->GetPath(CanvasWindingRule::Nonzero, mTarget);
  }

  if (!path) {
    // check if the path is valid
    EnsureUserSpacePath(CanvasWindingRule::Nonzero);
    path = mPath;
  }

  if (!path) {
    aError.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
    return;
  }

  // get the bounds of the current path. They are relative to the canvas
  gfx::Rect bounds(path->GetBounds(mTarget->GetTransform()));
  if ((bounds.width == 0) || (bounds.height == 0) || !bounds.IsFinite()) {
    // The specified region has no pixels.
    aError.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
    return;
  }

  // remove old hit region first
  RemoveHitRegion(aOptions.mId);

  if (aOptions.mControl) {
    // also remove regions with this control
    for (size_t x = 0; x < mHitRegionsOptions.Length(); x++) {
      RegionInfo& info = mHitRegionsOptions[x];
      if (info.mElement == aOptions.mControl) {
        mHitRegionsOptions.RemoveElementAt(x);
        break;
      }
    }
#ifdef ACCESSIBILITY
    aOptions.mControl->SetProperty(nsGkAtoms::hitregion, new bool(true),
                                   nsINode::DeleteProperty<bool>);
#endif
  }

  // finally, add the region to the list
  RegionInfo info;
  info.mId = aOptions.mId;
  info.mElement = aOptions.mControl;
  RefPtr<PathBuilder> pathBuilder =
      path->TransformedCopyToBuilder(mTarget->GetTransform());
  info.mPath = pathBuilder->Finish();

  mHitRegionsOptions.InsertElementAt(0, info);
}

void CanvasRenderingContext2D::RemoveHitRegion(const nsAString& aId) {
  if (aId.Length() == 0) {
    return;
  }

  for (size_t x = 0; x < mHitRegionsOptions.Length(); x++) {
    RegionInfo& info = mHitRegionsOptions[x];
    if (info.mId == aId) {
      mHitRegionsOptions.RemoveElementAt(x);

      return;
    }
  }
}

void CanvasRenderingContext2D::ClearHitRegions() { mHitRegionsOptions.Clear(); }

bool CanvasRenderingContext2D::GetHitRegionRect(Element* aElement,
                                                nsRect& aRect) {
  for (unsigned int x = 0; x < mHitRegionsOptions.Length(); x++) {
    RegionInfo& info = mHitRegionsOptions[x];
    if (info.mElement == aElement) {
      gfx::Rect bounds(info.mPath->GetBounds());
      gfxRect rect(bounds.x, bounds.y, bounds.width, bounds.height);
      aRect = nsLayoutUtils::RoundGfxRectToAppRect(rect, AppUnitsPerCSSPixel());

      return true;
    }
  }

  return false;
}

/**
 * Used for nsBidiPresUtils::ProcessText
 */
struct MOZ_STACK_CLASS CanvasBidiProcessor
    : public nsBidiPresUtils::BidiProcessor {
  typedef CanvasRenderingContext2D::Style Style;

  CanvasBidiProcessor()
      : nsBidiPresUtils::BidiProcessor(),
        mCtx(nullptr),
        mFontgrp(nullptr),
        mAppUnitsPerDevPixel(0),
        mOp(CanvasRenderingContext2D::TextDrawOperation::FILL),
        mTextRunFlags(),
        mDoMeasureBoundingBox(false) {
    if (Preferences::GetBool(GFX_MISSING_FONTS_NOTIFY_PREF)) {
      mMissingFonts = MakeUnique<gfxMissingFontRecorder>();
    }
  }

  ~CanvasBidiProcessor() {
    // notify front-end code if we encountered missing glyphs in any script
    if (mMissingFonts) {
      mMissingFonts->Flush();
    }
  }

  typedef CanvasRenderingContext2D::ContextState ContextState;

  virtual void SetText(const char16_t* aText, int32_t aLength,
                       nsBidiDirection aDirection) override {
    mFontgrp->UpdateUserFonts();  // ensure user font generation is current
    // adjust flags for current direction run
    gfx::ShapedTextFlags flags = mTextRunFlags;
    if (aDirection == NSBIDI_RTL) {
      flags |= gfx::ShapedTextFlags::TEXT_IS_RTL;
    } else {
      flags &= ~gfx::ShapedTextFlags::TEXT_IS_RTL;
    }
    mTextRun = mFontgrp->MakeTextRun(
        aText, aLength, mDrawTarget, mAppUnitsPerDevPixel, flags,
        nsTextFrameUtils::Flags::DontSkipDrawingForPendingUserFonts,
        mMissingFonts.get());
  }

  virtual nscoord GetWidth() override {
    gfxTextRun::Metrics textRunMetrics = mTextRun->MeasureText(
        mDoMeasureBoundingBox ? gfxFont::TIGHT_INK_EXTENTS
                              : gfxFont::LOOSE_INK_EXTENTS,
        mDrawTarget);

    // this only measures the height; the total width is gotten from the
    // the return value of ProcessText.
    if (mDoMeasureBoundingBox) {
      textRunMetrics.mBoundingBox.Scale(1.0 / mAppUnitsPerDevPixel);
      mBoundingBox = mBoundingBox.Union(textRunMetrics.mBoundingBox);
    }

    return NSToCoordRound(textRunMetrics.mAdvanceWidth);
  }

  already_AddRefed<gfxPattern> GetGradientFor(Style aStyle) {
    RefPtr<gfxPattern> pattern;
    CanvasGradient* gradient = mCtx->CurrentState().gradientStyles[aStyle];
    CanvasGradient::Type type = gradient->GetType();

    switch (type) {
      case CanvasGradient::Type::RADIAL: {
        auto radial = static_cast<CanvasRadialGradient*>(gradient);
        pattern = new gfxPattern(radial->mCenter1.x, radial->mCenter1.y,
                                 radial->mRadius1, radial->mCenter2.x,
                                 radial->mCenter2.y, radial->mRadius2);
        break;
      }
      case CanvasGradient::Type::LINEAR: {
        auto linear = static_cast<CanvasLinearGradient*>(gradient);
        pattern = new gfxPattern(linear->mBegin.x, linear->mBegin.y,
                                 linear->mEnd.x, linear->mEnd.y);
        break;
      }
      default:
        MOZ_ASSERT(false, "Should be linear or radial gradient.");
        return nullptr;
    }

    for (auto stop : gradient->mRawStops) {
      pattern->AddColorStop(stop.offset, stop.color);
    }

    return pattern.forget();
  }

  gfx::ExtendMode CvtCanvasRepeatToGfxRepeat(
      CanvasPattern::RepeatMode aRepeatMode) {
    switch (aRepeatMode) {
      case CanvasPattern::RepeatMode::REPEAT:
        return gfx::ExtendMode::REPEAT;
      case CanvasPattern::RepeatMode::REPEATX:
        return gfx::ExtendMode::REPEAT_X;
      case CanvasPattern::RepeatMode::REPEATY:
        return gfx::ExtendMode::REPEAT_Y;
      case CanvasPattern::RepeatMode::NOREPEAT:
        return gfx::ExtendMode::CLAMP;
      default:
        return gfx::ExtendMode::CLAMP;
    }
  }

  already_AddRefed<gfxPattern> GetPatternFor(Style aStyle) {
    const CanvasPattern* pat = mCtx->CurrentState().patternStyles[aStyle];
    RefPtr<gfxPattern> pattern = new gfxPattern(pat->mSurface, pat->mTransform);
    pattern->SetExtend(CvtCanvasRepeatToGfxRepeat(pat->mRepeat));
    return pattern.forget();
  }

  virtual void DrawText(nscoord aXOffset, nscoord aWidth) override {
    gfx::Point point = mPt;
    bool rtl = mTextRun->IsRightToLeft();
    bool verticalRun = mTextRun->IsVertical();
    RefPtr<gfxPattern> pattern;

    float& inlineCoord = verticalRun ? point.y : point.x;
    inlineCoord += aXOffset;

    // offset is given in terms of left side of string
    if (rtl) {
      // Bug 581092 - don't use rounded pixel width to advance to
      // right-hand end of run, because this will cause different
      // glyph positioning for LTR vs RTL drawing of the same
      // glyph string on OS X and DWrite where textrun widths may
      // involve fractional pixels.
      gfxTextRun::Metrics textRunMetrics = mTextRun->MeasureText(
          mDoMeasureBoundingBox ? gfxFont::TIGHT_INK_EXTENTS
                                : gfxFont::LOOSE_INK_EXTENTS,
          mDrawTarget);
      inlineCoord += textRunMetrics.mAdvanceWidth;
      // old code was:
      //   point.x += width * mAppUnitsPerDevPixel;
      // TODO: restore this if/when we move to fractional coords
      // throughout the text layout process
    }

    mCtx->EnsureTarget();
    if (!mCtx->IsTargetValid()) {
      return;
    }

    // Defer the tasks to gfxTextRun which will handle color/svg-in-ot fonts
    // appropriately.
    StrokeOptions strokeOpts;
    DrawOptions drawOpts;
    Style style = (mOp == CanvasRenderingContext2D::TextDrawOperation::FILL)
                      ? Style::FILL
                      : Style::STROKE;

    AdjustedTarget target(mCtx);
    if (!target) {
      return;
    }

    RefPtr<gfxContext> thebes =
        gfxContext::CreatePreservingTransformOrNull(target);
    if (!thebes) {
      // If CreatePreservingTransformOrNull returns null, it will also have
      // issued a gfxCriticalNote already, so here we'll just bail out.
      return;
    }
    gfxTextRun::DrawParams params(thebes);

    const ContextState* state = &mCtx->CurrentState();
    if (state->StyleIsColor(style)) {  // Color
      nscolor fontColor = state->colorStyles[style];
      if (style == Style::FILL) {
        params.context->SetColor(sRGBColor::FromABGR(fontColor));
      } else {
        params.textStrokeColor = fontColor;
      }
    } else {
      if (state->gradientStyles[style]) {  // Gradient
        pattern = GetGradientFor(style);
      } else if (state->patternStyles[style]) {  // Pattern
        pattern = GetPatternFor(style);
      } else {
        MOZ_ASSERT(false, "Should never reach here.");
        return;
      }
      MOZ_ASSERT(pattern, "No valid pattern.");

      if (style == Style::FILL) {
        params.context->SetPattern(pattern);
      } else {
        params.textStrokePattern = pattern;
      }
    }

    drawOpts.mAlpha = state->globalAlpha;
    drawOpts.mCompositionOp = mCtx->UsedOperation();
    if (!mCtx->IsTargetValid()) {
      return;
    }
    state = &mCtx->CurrentState();
    params.drawOpts = &drawOpts;

    if (style == Style::STROKE) {
      strokeOpts.mLineWidth = state->lineWidth;
      strokeOpts.mLineJoin = state->lineJoin;
      strokeOpts.mLineCap = state->lineCap;
      strokeOpts.mMiterLimit = state->miterLimit;
      strokeOpts.mDashLength = state->dash.Length();
      strokeOpts.mDashPattern =
          (strokeOpts.mDashLength > 0) ? state->dash.Elements() : 0;
      strokeOpts.mDashOffset = state->dashOffset;

      params.drawMode = DrawMode::GLYPH_STROKE;
      params.strokeOpts = &strokeOpts;
    }

    mTextRun->Draw(gfxTextRun::Range(mTextRun.get()), point, params);
  }

  // current text run
  RefPtr<gfxTextRun> mTextRun;

  // pointer to a screen reference context used to measure text and such
  RefPtr<DrawTarget> mDrawTarget;

  // Pointer to the draw target we should fill our text to
  CanvasRenderingContext2D* mCtx;

  // position of the left side of the string, alphabetic baseline
  gfx::Point mPt;

  // current font
  gfxFontGroup* mFontgrp;

  // to record any unsupported characters found in the text,
  // and notify front-end if it is interested
  UniquePtr<gfxMissingFontRecorder> mMissingFonts;

  // dev pixel conversion factor
  int32_t mAppUnitsPerDevPixel;

  // operation (fill or stroke)
  CanvasRenderingContext2D::TextDrawOperation mOp;

  // union of bounding boxes of all runs, needed for shadows
  gfxRect mBoundingBox;

  // flags to use when creating textrun, based on CSS style
  gfx::ShapedTextFlags mTextRunFlags;

  // true iff the bounding box should be measured
  bool mDoMeasureBoundingBox;
};

TextMetrics* CanvasRenderingContext2D::DrawOrMeasureText(
    const nsAString& aRawText, float aX, float aY,
    const Optional<double>& aMaxWidth, TextDrawOperation aOp,
    ErrorResult& aError) {
  // Approximated baselines. In an ideal world, we'd read the baseline info
  // directly from the font (where available). Alas we currently lack
  // that functionality. These numbers are best guesses and should
  // suffice for now. Both are fractions of the em ascent/descent from the
  // alphabetic baseline.
  const double kHangingBaselineDefault = 0.8;      // fraction of ascent
  const double kIdeographicBaselineDefault = 0.5;  // fraction of descent

  if (!mCanvasElement && !mDocShell) {
    NS_WARNING(
        "Canvas element must be non-null or a docshell must be provided");
    aError = NS_ERROR_FAILURE;
    return nullptr;
  }

  RefPtr<PresShell> presShell = GetPresShell();
  if (!presShell) {
    aError = NS_ERROR_FAILURE;
    return nullptr;
  }

  Document* document = presShell->GetDocument();

  // replace all the whitespace characters with U+0020 SPACE
  nsAutoString textToDraw(aRawText);
  TextReplaceWhitespaceCharacters(textToDraw);

  // According to spec, the API should return an empty array if maxWidth was
  // provided but is less than or equal to zero or equal to NaN.
  if (aMaxWidth.WasPassed() &&
      (aMaxWidth.Value() <= 0 || IsNaN(aMaxWidth.Value()))) {
    textToDraw.Truncate();
  }

  // for now, default to ltr if not in doc
  bool isRTL = false;

  RefPtr<ComputedStyle> canvasStyle;
  if (mCanvasElement && mCanvasElement->IsInComposedDoc()) {
    // try to find the closest context
    canvasStyle = nsComputedDOMStyle::GetComputedStyle(mCanvasElement, nullptr);
    if (!canvasStyle) {
      aError = NS_ERROR_FAILURE;
      return nullptr;
    }

    isRTL = canvasStyle->StyleVisibility()->mDirection == StyleDirection::Rtl;
  } else {
    isRTL = GET_BIDI_OPTION_DIRECTION(document->GetBidiOptions()) ==
            IBMBIDI_TEXTDIRECTION_RTL;
  }

  // This is only needed to know if we can know the drawing bounding box easily.
  const bool doCalculateBounds = NeedToCalculateBounds();
  if (presShell->IsDestroying()) {
    aError = NS_ERROR_FAILURE;
    return nullptr;
  }

  gfxFontGroup* currentFontStyle = GetCurrentFontStyle();
  if (!currentFontStyle) {
    aError = NS_ERROR_FAILURE;
    return nullptr;
  }

  MOZ_ASSERT(!presShell->IsDestroying(),
             "GetCurrentFontStyle() should have returned null if the presshell "
             "is being destroyed");

  nsPresContext* presContext = presShell->GetPresContext();

  // ensure user font set is up to date
  presContext->Document()->FlushUserFontSet();
  currentFontStyle->SetUserFontSet(presContext->GetUserFontSet());

  if (currentFontStyle->GetStyle()->size == 0.0F) {
    aError = NS_OK;
    if (aOp == TextDrawOperation::MEASURE) {
      return new TextMetrics(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
                             0.0, 0.0);
    }
    return nullptr;
  }

  if (!IsFinite(aX) || !IsFinite(aY)) {
    aError = NS_OK;
    // This may not be correct - what should TextMetrics contain in the case of
    // infinite width or height?
    if (aOp == TextDrawOperation::MEASURE) {
      return new TextMetrics(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
                             0.0, 0.0);
    }
    return nullptr;
  }

  CanvasBidiProcessor processor;

  // If we don't have a ComputedStyle, we can't set up vertical-text flags
  // (for now, at least; perhaps we need new Canvas API to control this).
  processor.mTextRunFlags =
      canvasStyle ? nsLayoutUtils::GetTextRunFlagsForStyle(
                        canvasStyle, presContext, canvasStyle->StyleFont(),
                        canvasStyle->StyleText(), 0)
                  : gfx::ShapedTextFlags();

  GetAppUnitsValues(&processor.mAppUnitsPerDevPixel, nullptr);
  processor.mPt = gfx::Point(aX, aY);
  processor.mDrawTarget =
      gfxPlatform::GetPlatform()->ScreenReferenceDrawTarget();

  // If we don't have a target then we don't have a transform. A target won't
  // be needed in the case where we're measuring the text size. This allows
  // to avoid creating a target if it's only being used to measure text sizes.
  if (mTarget) {
    processor.mDrawTarget->SetTransform(mTarget->GetTransform());
  }
  processor.mCtx = this;
  processor.mOp = aOp;
  processor.mBoundingBox = gfxRect(0, 0, 0, 0);
  processor.mDoMeasureBoundingBox = doCalculateBounds ||
                                    !mIsEntireFrameInvalid ||
                                    aOp == TextDrawOperation::MEASURE;
  processor.mFontgrp = currentFontStyle;

  nscoord totalWidthCoord;

  // calls bidi algo twice since it needs the full text width and the
  // bounding boxes before rendering anything
  aError = nsBidiPresUtils::ProcessText(
      textToDraw.get(), textToDraw.Length(), isRTL ? NSBIDI_RTL : NSBIDI_LTR,
      presShell->GetPresContext(), processor, nsBidiPresUtils::MODE_MEASURE,
      nullptr, 0, &totalWidthCoord, &mBidiEngine);
  if (aError.Failed()) {
    return nullptr;
  }

  float totalWidth = float(totalWidthCoord) / processor.mAppUnitsPerDevPixel;

  // offset pt.x based on text align
  gfxFloat anchorX;

  const ContextState& state = CurrentState();
  if (state.textAlign == TextAlign::CENTER) {
    anchorX = .5;
  } else if (state.textAlign == TextAlign::LEFT ||
             (!isRTL && state.textAlign == TextAlign::START) ||
             (isRTL && state.textAlign == TextAlign::END)) {
    anchorX = 0;
  } else {
    anchorX = 1;
  }

  float offsetX = anchorX * totalWidth;
  processor.mPt.x -= offsetX;

  // offset pt.y (or pt.x, for vertical text) based on text baseline
  processor.mFontgrp
      ->UpdateUserFonts();  // ensure user font generation is current
  const gfxFont::Metrics& fontMetrics =
      processor.mFontgrp->GetFirstValidFont()->GetMetrics(
          nsFontMetrics::eHorizontal);

  gfxFloat baselineAnchor;

  switch (state.textBaseline) {
    case TextBaseline::HANGING:
      baselineAnchor = fontMetrics.emAscent * kHangingBaselineDefault;
      break;
    case TextBaseline::TOP:
      baselineAnchor = fontMetrics.emAscent;
      break;
    case TextBaseline::MIDDLE:
      baselineAnchor = (fontMetrics.emAscent - fontMetrics.emDescent) * .5f;
      break;
    case TextBaseline::ALPHABETIC:
      baselineAnchor = 0;
      break;
    case TextBaseline::IDEOGRAPHIC:
      baselineAnchor = -fontMetrics.emDescent * kIdeographicBaselineDefault;
      break;
    case TextBaseline::BOTTOM:
      baselineAnchor = -fontMetrics.emDescent;
      break;
    default:
      MOZ_CRASH("GFX: unexpected TextBaseline");
  }

  // We can't query the textRun directly, as it may not have been created yet;
  // so instead we check the flags that will be used to initialize it.
  gfx::ShapedTextFlags runOrientation =
      (processor.mTextRunFlags & gfx::ShapedTextFlags::TEXT_ORIENT_MASK);
  if (runOrientation != gfx::ShapedTextFlags::TEXT_ORIENT_HORIZONTAL) {
    if (runOrientation == gfx::ShapedTextFlags::TEXT_ORIENT_VERTICAL_MIXED ||
        runOrientation == gfx::ShapedTextFlags::TEXT_ORIENT_VERTICAL_UPRIGHT) {
      // Adjust to account for mTextRun being shaped using center baseline
      // rather than alphabetic.
      baselineAnchor -= (fontMetrics.emAscent - fontMetrics.emDescent) * .5f;
    }
    processor.mPt.x -= baselineAnchor;
  } else {
    processor.mPt.y += baselineAnchor;
  }

  // if only measuring, don't need to do any more work
  if (aOp == TextDrawOperation::MEASURE) {
    aError = NS_OK;
    double actualBoundingBoxLeft = processor.mBoundingBox.X() - offsetX;
    double actualBoundingBoxRight = processor.mBoundingBox.XMost() - offsetX;
    double actualBoundingBoxAscent =
        -processor.mBoundingBox.Y() - baselineAnchor;
    double actualBoundingBoxDescent =
        processor.mBoundingBox.YMost() + baselineAnchor;
    double hangingBaseline =
        fontMetrics.emAscent * kHangingBaselineDefault - baselineAnchor;
    double ideographicBaseline =
        -fontMetrics.emDescent * kIdeographicBaselineDefault - baselineAnchor;
    return new TextMetrics(
        totalWidth, actualBoundingBoxLeft, actualBoundingBoxRight,
        fontMetrics.maxAscent - baselineAnchor,   // fontBBAscent
        fontMetrics.maxDescent + baselineAnchor,  // fontBBDescent
        actualBoundingBoxAscent, actualBoundingBoxDescent,
        fontMetrics.emAscent - baselineAnchor,    // emHeightAscent
        -fontMetrics.emDescent - baselineAnchor,  // emHeightDescent
        hangingBaseline,
        -baselineAnchor,  // alphabeticBaseline
        ideographicBaseline);
  }

  // If we did not actually calculate bounds, set up a simple bounding box
  // based on the text position and advance.
  if (!doCalculateBounds) {
    processor.mBoundingBox.width = totalWidth;
    processor.mBoundingBox.MoveBy(gfxPoint(processor.mPt.x, processor.mPt.y));
  }

  processor.mPt.x *= processor.mAppUnitsPerDevPixel;
  processor.mPt.y *= processor.mAppUnitsPerDevPixel;

  EnsureTarget();
  if (!IsTargetValid()) {
    aError = NS_ERROR_FAILURE;
    return nullptr;
  }

  Matrix oldTransform = mTarget->GetTransform();
  // if text is over aMaxWidth, then scale the text horizontally such that its
  // width is precisely aMaxWidth
  if (aMaxWidth.WasPassed() && aMaxWidth.Value() > 0 &&
      totalWidth > aMaxWidth.Value()) {
    Matrix newTransform = oldTransform;

    // Translate so that the anchor point is at 0,0, then scale and then
    // translate back.
    newTransform.PreTranslate(aX, 0);
    newTransform.PreScale(aMaxWidth.Value() / totalWidth, 1);
    newTransform.PreTranslate(-aX, 0);
    /* we do this to avoid an ICE in the android compiler */
    Matrix androidCompilerBug = newTransform;
    mTarget->SetTransform(androidCompilerBug);
  }

  // save the previous bounding box
  gfxRect boundingBox = processor.mBoundingBox;

  // don't ever need to measure the bounding box twice
  processor.mDoMeasureBoundingBox = false;

  aError = nsBidiPresUtils::ProcessText(
      textToDraw.get(), textToDraw.Length(), isRTL ? NSBIDI_RTL : NSBIDI_LTR,
      presShell->GetPresContext(), processor, nsBidiPresUtils::MODE_DRAW,
      nullptr, 0, nullptr, &mBidiEngine);

  if (aError.Failed()) {
    return nullptr;
  }

  mTarget->SetTransform(oldTransform);

  if (aOp == CanvasRenderingContext2D::TextDrawOperation::FILL &&
      !doCalculateBounds) {
    RedrawUser(boundingBox);
  } else {
    Redraw();
  }

  aError = NS_OK;
  return nullptr;
}

gfxFontGroup* CanvasRenderingContext2D::GetCurrentFontStyle() {
  // use lazy initilization for the font group since it's rather expensive
  if (!CurrentState().fontGroup) {
    ErrorResult err;
    NS_NAMED_LITERAL_STRING(kDefaultFontStyle, "10px sans-serif");
    static float kDefaultFontSize = 10.0;
    RefPtr<PresShell> presShell = GetPresShell();
    bool fontUpdated = SetFontInternal(kDefaultFontStyle, err);
    if (err.Failed() || !fontUpdated) {
      err.SuppressException();
      gfxFontStyle style;
      style.size = kDefaultFontSize;
      gfxTextPerfMetrics* tp = nullptr;
      FontMatchingStats* fontStats = nullptr;
      if (presShell && !presShell->IsDestroying()) {
        tp = presShell->GetPresContext()->GetTextPerfMetrics();
        fontStats = presShell->GetPresContext()->GetFontMatchingStats();
      }
      int32_t perDevPixel, perCSSPixel;
      GetAppUnitsValues(&perDevPixel, &perCSSPixel);
      gfxFloat devToCssSize = gfxFloat(perDevPixel) / gfxFloat(perCSSPixel);
      CurrentState().fontGroup = gfxPlatform::GetPlatform()->CreateFontGroup(
          FontFamilyList(StyleGenericFontFamily::SansSerif), &style, tp,
          fontStats, nullptr, devToCssSize);
      if (CurrentState().fontGroup) {
        CurrentState().font = kDefaultFontStyle;
      } else {
        NS_ERROR("Default canvas font is invalid");
      }
    }
  } else {
    // The fontgroup needs to check if its cached families/faces are valid.
    CurrentState().fontGroup->CheckForUpdatedPlatformList();
  }

  return CurrentState().fontGroup;
}

//
// line caps/joins
//

void CanvasRenderingContext2D::SetLineCap(const nsAString& aLinecapStyle) {
  CapStyle cap;

  if (aLinecapStyle.EqualsLiteral("butt")) {
    cap = CapStyle::BUTT;
  } else if (aLinecapStyle.EqualsLiteral("round")) {
    cap = CapStyle::ROUND;
  } else if (aLinecapStyle.EqualsLiteral("square")) {
    cap = CapStyle::SQUARE;
  } else {
    // XXX ERRMSG we need to report an error to developers here! (bug 329026)
    return;
  }

  CurrentState().lineCap = cap;
}

void CanvasRenderingContext2D::GetLineCap(nsAString& aLinecapStyle) {
  switch (CurrentState().lineCap) {
    case CapStyle::BUTT:
      aLinecapStyle.AssignLiteral("butt");
      break;
    case CapStyle::ROUND:
      aLinecapStyle.AssignLiteral("round");
      break;
    case CapStyle::SQUARE:
      aLinecapStyle.AssignLiteral("square");
      break;
  }
}

void CanvasRenderingContext2D::SetLineJoin(const nsAString& aLinejoinStyle) {
  JoinStyle j;

  if (aLinejoinStyle.EqualsLiteral("round")) {
    j = JoinStyle::ROUND;
  } else if (aLinejoinStyle.EqualsLiteral("bevel")) {
    j = JoinStyle::BEVEL;
  } else if (aLinejoinStyle.EqualsLiteral("miter")) {
    j = JoinStyle::MITER_OR_BEVEL;
  } else {
    // XXX ERRMSG we need to report an error to developers here! (bug 329026)
    return;
  }

  CurrentState().lineJoin = j;
}

void CanvasRenderingContext2D::GetLineJoin(nsAString& aLinejoinStyle,
                                           ErrorResult& aError) {
  switch (CurrentState().lineJoin) {
    case JoinStyle::ROUND:
      aLinejoinStyle.AssignLiteral("round");
      break;
    case JoinStyle::BEVEL:
      aLinejoinStyle.AssignLiteral("bevel");
      break;
    case JoinStyle::MITER_OR_BEVEL:
      aLinejoinStyle.AssignLiteral("miter");
      break;
    default:
      aError.Throw(NS_ERROR_FAILURE);
  }
}

void CanvasRenderingContext2D::SetLineDash(const Sequence<double>& aSegments,
                                           ErrorResult& aRv) {
  nsTArray<mozilla::gfx::Float> dash;

  for (uint32_t x = 0; x < aSegments.Length(); x++) {
    if (aSegments[x] < 0.0) {
      // Pattern elements must be finite "numbers" >= 0, with "finite"
      // taken care of by WebIDL
      return;
    }

    if (!dash.AppendElement(aSegments[x], fallible)) {
      aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
      return;
    }
  }
  if (aSegments.Length() %
      2) {  // If the number of elements is odd, concatenate again
    for (uint32_t x = 0; x < aSegments.Length(); x++) {
      if (!dash.AppendElement(aSegments[x], fallible)) {
        aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
        return;
      }
    }
  }

  CurrentState().dash = std::move(dash);
}

void CanvasRenderingContext2D::GetLineDash(nsTArray<double>& aSegments) const {
  const nsTArray<mozilla::gfx::Float>& dash = CurrentState().dash;
  aSegments.Clear();

  for (uint32_t x = 0; x < dash.Length(); x++) {
    aSegments.AppendElement(dash[x]);
  }
}

void CanvasRenderingContext2D::SetLineDashOffset(double aOffset) {
  CurrentState().dashOffset = aOffset;
}

double CanvasRenderingContext2D::LineDashOffset() const {
  return CurrentState().dashOffset;
}

bool CanvasRenderingContext2D::IsPointInPath(JSContext* aCx, double aX,
                                             double aY,
                                             const CanvasWindingRule& aWinding,
                                             nsIPrincipal& aSubjectPrincipal) {
  if (!FloatValidate(aX, aY)) {
    return false;
  }

  // Check for site-specific permission and return false if no permission.
  if (mCanvasElement) {
    nsCOMPtr<Document> ownerDoc = mCanvasElement->OwnerDoc();
    if (!CanvasUtils::IsImageExtractionAllowed(ownerDoc, aCx,
                                               aSubjectPrincipal)) {
      return false;
    }
  }

  EnsureUserSpacePath(aWinding);
  if (!mPath) {
    return false;
  }

  if (mPathTransformWillUpdate) {
    return mPath->ContainsPoint(Point(aX, aY), mPathToDS);
  }

  return mPath->ContainsPoint(Point(aX, aY), mTarget->GetTransform());
}

bool CanvasRenderingContext2D::IsPointInPath(JSContext* aCx,
                                             const CanvasPath& aPath, double aX,
                                             double aY,
                                             const CanvasWindingRule& aWinding,
                                             nsIPrincipal&) {
  if (!FloatValidate(aX, aY)) {
    return false;
  }

  EnsureTarget();
  if (!IsTargetValid()) {
    return false;
  }

  RefPtr<gfx::Path> tempPath = aPath.GetPath(aWinding, mTarget);

  return tempPath->ContainsPoint(Point(aX, aY), mTarget->GetTransform());
}

bool CanvasRenderingContext2D::IsPointInStroke(
    JSContext* aCx, double aX, double aY, nsIPrincipal& aSubjectPrincipal) {
  if (!FloatValidate(aX, aY)) {
    return false;
  }

  // Check for site-specific permission and return false if no permission.
  if (mCanvasElement) {
    nsCOMPtr<Document> ownerDoc = mCanvasElement->OwnerDoc();
    if (!CanvasUtils::IsImageExtractionAllowed(ownerDoc, aCx,
                                               aSubjectPrincipal)) {
      return false;
    }
  }

  EnsureUserSpacePath();
  if (!mPath) {
    return false;
  }

  const ContextState& state = CurrentState();

  StrokeOptions strokeOptions(state.lineWidth, state.lineJoin, state.lineCap,
                              state.miterLimit, state.dash.Length(),
                              state.dash.Elements(), state.dashOffset);

  if (mPathTransformWillUpdate) {
    return mPath->StrokeContainsPoint(strokeOptions, Point(aX, aY), mPathToDS);
  }
  return mPath->StrokeContainsPoint(strokeOptions, Point(aX, aY),
                                    mTarget->GetTransform());
}

bool CanvasRenderingContext2D::IsPointInStroke(JSContext* aCx,
                                               const CanvasPath& aPath,
                                               double aX, double aY,
                                               nsIPrincipal&) {
  if (!FloatValidate(aX, aY)) {
    return false;
  }

  EnsureTarget();
  if (!IsTargetValid()) {
    return false;
  }

  RefPtr<gfx::Path> tempPath =
      aPath.GetPath(CanvasWindingRule::Nonzero, mTarget);

  const ContextState& state = CurrentState();

  StrokeOptions strokeOptions(state.lineWidth, state.lineJoin, state.lineCap,
                              state.miterLimit, state.dash.Length(),
                              state.dash.Elements(), state.dashOffset);

  return tempPath->StrokeContainsPoint(strokeOptions, Point(aX, aY),
                                       mTarget->GetTransform());
}

// Returns a surface that contains only the part needed to draw aSourceRect.
// On entry, aSourceRect is relative to aSurface, and on return aSourceRect is
// relative to the returned surface.
static already_AddRefed<SourceSurface> ExtractSubrect(SourceSurface* aSurface,
                                                      gfx::Rect* aSourceRect,
                                                      DrawTarget* aTargetDT) {
  gfx::Rect roundedOutSourceRect = *aSourceRect;
  roundedOutSourceRect.RoundOut();
  gfx::IntRect roundedOutSourceRectInt;
  if (!roundedOutSourceRect.ToIntRect(&roundedOutSourceRectInt)) {
    RefPtr<SourceSurface> surface(aSurface);
    return surface.forget();
  }

  RefPtr<DrawTarget> subrectDT = aTargetDT->CreateSimilarDrawTarget(
      roundedOutSourceRectInt.Size(), SurfaceFormat::B8G8R8A8);

  if (subrectDT) {
    // See bug 1524554.
    subrectDT->ClearRect(gfx::Rect());
  }

  if (!subrectDT || !subrectDT->IsValid()) {
    RefPtr<SourceSurface> surface(aSurface);
    return surface.forget();
  }

  *aSourceRect -= roundedOutSourceRect.TopLeft();

  subrectDT->CopySurface(aSurface, roundedOutSourceRectInt, IntPoint());
  return subrectDT->Snapshot();
}

//
// image
//

static void ClipImageDimension(double& aSourceCoord, double& aSourceSize,
                               int32_t aImageSize, double& aDestCoord,
                               double& aDestSize) {
  double scale = aDestSize / aSourceSize;
  if (aSourceCoord < 0.0) {
    double destEnd = aDestCoord + aDestSize;
    aDestCoord -= aSourceCoord * scale;
    aDestSize = destEnd - aDestCoord;
    aSourceSize += aSourceCoord;
    aSourceCoord = 0.0;
  }
  double delta = aImageSize - (aSourceCoord + aSourceSize);
  if (delta < 0.0) {
    aDestSize += delta * scale;
    aSourceSize = aImageSize - aSourceCoord;
  }
}

// Acts like nsLayoutUtils::SurfaceFromElement, but it'll attempt
// to pull a SourceSurface from our cache. This allows us to avoid
// reoptimizing surfaces if content and canvas backends are different.
nsLayoutUtils::SurfaceFromElementResult
CanvasRenderingContext2D::CachedSurfaceFromElement(Element* aElement) {
  nsLayoutUtils::SurfaceFromElementResult res;
  nsCOMPtr<nsIImageLoadingContent> imageLoader = do_QueryInterface(aElement);
  if (!imageLoader) {
    return res;
  }

  nsCOMPtr<imgIRequest> imgRequest;
  imageLoader->GetRequest(nsIImageLoadingContent::CURRENT_REQUEST,
                          getter_AddRefs(imgRequest));
  if (!imgRequest) {
    return res;
  }

  uint32_t status = 0;
  if (NS_FAILED(imgRequest->GetImageStatus(&status)) ||
      !(status & imgIRequest::STATUS_LOAD_COMPLETE)) {
    return res;
  }

  nsCOMPtr<nsIPrincipal> principal;
  if (NS_FAILED(imgRequest->GetImagePrincipal(getter_AddRefs(principal))) ||
      !principal) {
    return res;
  }

  if (NS_FAILED(imgRequest->GetHadCrossOriginRedirects(
          &res.mHadCrossOriginRedirects))) {
    return res;
  }

  res.mSourceSurface = CanvasImageCache::LookupAllCanvas(aElement);
  if (!res.mSourceSurface) {
    return res;
  }

  int32_t corsmode = imgIRequest::CORS_NONE;
  if (NS_SUCCEEDED(imgRequest->GetCORSMode(&corsmode))) {
    res.mCORSUsed = corsmode != imgIRequest::CORS_NONE;
  }

  res.mSize = res.mIntrinsicSize = res.mSourceSurface->GetSize();
  res.mPrincipal = std::move(principal);
  res.mImageRequest = std::move(imgRequest);
  res.mIsWriteOnly = CheckWriteOnlySecurity(res.mCORSUsed, res.mPrincipal,
                                            res.mHadCrossOriginRedirects);

  return res;
}

// drawImage(in HTMLImageElement image, in float dx, in float dy);
//   -- render image from 0,0 at dx,dy top-left coords
// drawImage(in HTMLImageElement image, in float dx, in float dy, in float dw,
//           in float dh);
//   -- render image from 0,0 at dx,dy top-left coords clipping it to dw,dh
// drawImage(in HTMLImageElement image, in float sx, in float sy, in float sw,
//           in float sh, in float dx, in float dy, in float dw, in float dh);
//   -- render the region defined by (sx,sy,sw,wh) in image-local space into the
//      region (dx,dy,dw,dh) on the canvas

// If only dx and dy are passed in then optional_argc should be 0. If only
// dx, dy, dw and dh are passed in then optional_argc should be 2. The only
// other valid value for optional_argc is 6 if sx, sy, sw, sh, dx, dy, dw and dh
// are all passed in.

void CanvasRenderingContext2D::DrawImage(const CanvasImageSource& aImage,
                                         double aSx, double aSy, double aSw,
                                         double aSh, double aDx, double aDy,
                                         double aDw, double aDh,
                                         uint8_t aOptional_argc,
                                         ErrorResult& aError) {
  MOZ_ASSERT(aOptional_argc == 0 || aOptional_argc == 2 || aOptional_argc == 6);

  if (!ValidateRect(aDx, aDy, aDw, aDh, true)) {
    return;
  }
  if (aOptional_argc == 6) {
    if (!ValidateRect(aSx, aSy, aSw, aSh, true)) {
      return;
    }
  }

  RefPtr<SourceSurface> srcSurf;
  gfx::IntSize imgSize;
  gfx::IntSize intrinsicImgSize;

  Element* element = nullptr;

  EnsureTarget();
  if (!IsTargetValid()) {
    return;
  }

  if (aImage.IsHTMLCanvasElement()) {
    HTMLCanvasElement* canvas = &aImage.GetAsHTMLCanvasElement();
    element = canvas;
    nsIntSize size = canvas->GetSize();
    if (size.width == 0 || size.height == 0) {
      aError.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
      return;
    }

    if (canvas->IsWriteOnly()) {
      SetWriteOnly();
    }
  } else if (aImage.IsImageBitmap()) {
    ImageBitmap& imageBitmap = aImage.GetAsImageBitmap();
    srcSurf = imageBitmap.PrepareForDrawTarget(mTarget);

    if (!srcSurf) {
      return;
    }

    if (imageBitmap.IsWriteOnly()) {
      SetWriteOnly();
    }

    imgSize = intrinsicImgSize =
        gfx::IntSize(imageBitmap.Width(), imageBitmap.Height());
  } else {
    if (aImage.IsHTMLImageElement()) {
      HTMLImageElement* img = &aImage.GetAsHTMLImageElement();
      element = img;
    } else if (aImage.IsSVGImageElement()) {
      SVGImageElement* img = &aImage.GetAsSVGImageElement();
      element = img;
    } else {
      HTMLVideoElement* video = &aImage.GetAsHTMLVideoElement();
      video->MarkAsContentSource(
          mozilla::dom::HTMLVideoElement::CallerAPI::DRAW_IMAGE);
      element = video;
    }

    srcSurf = CanvasImageCache::LookupCanvas(element, mCanvasElement, &imgSize,
                                             &intrinsicImgSize);
  }

  nsLayoutUtils::DirectDrawInfo drawInfo;

  if (!srcSurf) {
    // The canvas spec says that drawImage should draw the first frame
    // of animated images. We also don't want to rasterize vector images.
    uint32_t sfeFlags = nsLayoutUtils::SFE_WANT_FIRST_FRAME_IF_IMAGE |
                        nsLayoutUtils::SFE_NO_RASTERIZING_VECTORS;

    nsLayoutUtils::SurfaceFromElementResult res =
        CanvasRenderingContext2D::CachedSurfaceFromElement(element);

    if (!res.mSourceSurface) {
      res = nsLayoutUtils::SurfaceFromElement(element, sfeFlags, mTarget);
    }

    if (!res.mSourceSurface && !res.mDrawInfo.mImgContainer) {
      // The spec says to silently do nothing in the following cases:
      //   - The element is still loading.
      //   - The image is bad, but it's not in the broken state (i.e., we could
      //     decode the headers and get the size).
      if (!res.mIsStillLoading && !res.mHasSize) {
        aError.Throw(NS_ERROR_NOT_AVAILABLE);
      }
      return;
    }

    imgSize = res.mSize;
    intrinsicImgSize = res.mIntrinsicSize;

    if (mCanvasElement) {
      CanvasUtils::DoDrawImageSecurityCheck(mCanvasElement, res.mPrincipal,
                                            res.mIsWriteOnly, res.mCORSUsed);
    }

    if (res.mSourceSurface) {
      if (res.mImageRequest) {
        CanvasImageCache::NotifyDrawImage(element, mCanvasElement,
                                          res.mSourceSurface, imgSize,
                                          intrinsicImgSize);
      }
      srcSurf = res.mSourceSurface;
    } else {
      drawInfo = res.mDrawInfo;
    }
  }

  if (aOptional_argc == 0) {
    aSx = aSy = 0.0;
    aSw = (double)imgSize.width;
    aSh = (double)imgSize.height;
    aDw = (double)intrinsicImgSize.width;
    aDh = (double)intrinsicImgSize.height;
  } else if (aOptional_argc == 2) {
    aSx = aSy = 0.0;
    aSw = (double)imgSize.width;
    aSh = (double)imgSize.height;
  }

  if (aSw == 0.0 || aSh == 0.0) {
    aError.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return;
  }

  ClipImageDimension(aSx, aSw, imgSize.width, aDx, aDw);
  ClipImageDimension(aSy, aSh, imgSize.height, aDy, aDh);

  if (aSw <= 0.0 || aSh <= 0.0 || aDw <= 0.0 || aDh <= 0.0) {
    // source and/or destination are fully clipped, so nothing is painted
    return;
  }

  // Per spec, the smoothing setting applies only to scaling up a bitmap image.
  // When down-scaling the user agent is free to choose whether or not to smooth
  // the image. Nearest sampling when down-scaling is rarely desirable and
  // smoothing when down-scaling matches chromium's behavior.
  // If any dimension is up-scaled, we consider the image as being up-scaled.
  auto scale = mTarget->GetTransform().ScaleFactors(true);
  bool isDownScale =
      aDw * Abs(scale.width) < aSw && aDh * Abs(scale.height) < aSh;

  SamplingFilter samplingFilter;
  AntialiasMode antialiasMode;

  if (CurrentState().imageSmoothingEnabled || isDownScale) {
    samplingFilter = gfx::SamplingFilter::LINEAR;
    antialiasMode = AntialiasMode::DEFAULT;
  } else {
    samplingFilter = gfx::SamplingFilter::POINT;
    antialiasMode = AntialiasMode::NONE;
  }

  const bool needBounds = NeedToCalculateBounds();
  if (!IsTargetValid()) {
    return;
  }
  gfx::Rect bounds;
  if (needBounds) {
    bounds = gfx::Rect(aDx, aDy, aDw, aDh);
    bounds = mTarget->GetTransform().TransformBounds(bounds);
  }

  if (!IsTargetValid()) {
    aError.Throw(NS_ERROR_FAILURE);
    return;
  }

  if (srcSurf) {
    gfx::Rect sourceRect(aSx, aSy, aSw, aSh);
    if (element == mCanvasElement) {
      // srcSurf is a snapshot of mTarget. If we draw to mTarget now, we'll
      // trigger a COW copy of the whole canvas into srcSurf. That's a huge
      // waste if sourceRect doesn't cover the whole canvas.
      // We avoid copying the whole canvas by manually copying just the part
      // that we need.
      srcSurf = ExtractSubrect(srcSurf, &sourceRect, mTarget);
    }

    AdjustedTarget tempTarget(this, bounds.IsEmpty() ? nullptr : &bounds);
    if (!tempTarget) {
      gfxDevCrash(LogReason::InvalidDrawTarget)
          << "Invalid adjusted target in Canvas2D "
          << gfx::hexa((DrawTarget*)mTarget) << ", " << NeedToDrawShadow()
          << NeedToApplyFilter();
      return;
    }

    auto op = UsedOperation();
    if (!IsTargetValid() || !tempTarget) {
      return;
    }
    tempTarget->DrawSurface(
        srcSurf, gfx::Rect(aDx, aDy, aDw, aDh), sourceRect,
        DrawSurfaceOptions(samplingFilter, SamplingBounds::UNBOUNDED),
        DrawOptions(CurrentState().globalAlpha, op, antialiasMode));
  } else {
    DrawDirectlyToCanvas(drawInfo, &bounds, gfx::Rect(aDx, aDy, aDw, aDh),
                         gfx::Rect(aSx, aSy, aSw, aSh), imgSize);
  }

  RedrawUser(gfxRect(aDx, aDy, aDw, aDh));
}

void CanvasRenderingContext2D::DrawDirectlyToCanvas(
    const nsLayoutUtils::DirectDrawInfo& aImage, gfx::Rect* aBounds,
    gfx::Rect aDest, gfx::Rect aSrc, gfx::IntSize aImgSize) {
  MOZ_ASSERT(aSrc.width > 0 && aSrc.height > 0,
             "Need positive source width and height");

  AdjustedTarget tempTarget(this, aBounds->IsEmpty() ? nullptr : aBounds);
  if (!tempTarget) {
    return;
  }

  // Get any existing transforms on the context, including transformations used
  // for context shadow.
  Matrix matrix = tempTarget->GetTransform();
  gfxMatrix contextMatrix = ThebesMatrix(matrix);
  gfxSize contextScale(contextMatrix.ScaleFactors(true));

  // Scale the dest rect to include the context scale.
  aDest.Scale(contextScale.width, contextScale.height);

  // Scale the image size to the dest rect, and adjust the source rect to match.
  gfxSize scale(aDest.width / aSrc.width, aDest.height / aSrc.height);
  IntSize scaledImageSize = IntSize::Ceil(aImgSize.width * scale.width,
                                          aImgSize.height * scale.height);
  aSrc.Scale(scale.width, scale.height);

  // We're wrapping tempTarget's (our) DrawTarget here, so we need to restore
  // the matrix even though this is a temp gfxContext.
  AutoRestoreTransform autoRestoreTransform(mTarget);

  RefPtr<gfxContext> context = gfxContext::CreateOrNull(tempTarget);
  if (!context) {
    gfxDevCrash(LogReason::InvalidContext) << "Canvas context problem";
    return;
  }
  context->SetMatrixDouble(
      contextMatrix
          .PreScale(1.0 / contextScale.width, 1.0 / contextScale.height)
          .PreTranslate(aDest.x - aSrc.x, aDest.y - aSrc.y));

  context->SetOp(UsedOperation());

  // FLAG_CLAMP is added for increased performance, since we never tile here.
  uint32_t modifiedFlags = aImage.mDrawingFlags | imgIContainer::FLAG_CLAMP;

  CSSIntSize sz(
      scaledImageSize.width,
      scaledImageSize
          .height);  // XXX hmm is scaledImageSize really in CSS pixels?
  SVGImageContext svgContext(Some(sz));

  auto result = aImage.mImgContainer->Draw(
      context, scaledImageSize,
      ImageRegion::Create(gfxRect(aSrc.x, aSrc.y, aSrc.width, aSrc.height)),
      aImage.mWhichFrame, SamplingFilter::GOOD, Some(svgContext), modifiedFlags,
      CurrentState().globalAlpha);

  if (result != ImgDrawResult::SUCCESS) {
    NS_WARNING("imgIContainer::Draw failed");
  }
}

void CanvasRenderingContext2D::SetGlobalCompositeOperation(
    const nsAString& aOp, ErrorResult& aError) {
  CompositionOp comp_op;

#define CANVAS_OP_TO_GFX_OP(cvsop, op2d) \
  if (aOp.EqualsLiteral(cvsop)) comp_op = CompositionOp::OP_##op2d;

  CANVAS_OP_TO_GFX_OP("copy", SOURCE)
  else CANVAS_OP_TO_GFX_OP("source-atop", ATOP)
  else CANVAS_OP_TO_GFX_OP("source-in", IN)
  else CANVAS_OP_TO_GFX_OP("source-out", OUT)
  else CANVAS_OP_TO_GFX_OP("source-over", OVER)
  else CANVAS_OP_TO_GFX_OP("destination-in", DEST_IN)
  else CANVAS_OP_TO_GFX_OP("destination-out", DEST_OUT)
  else CANVAS_OP_TO_GFX_OP("destination-over", DEST_OVER)
  else CANVAS_OP_TO_GFX_OP("destination-atop", DEST_ATOP)
  else CANVAS_OP_TO_GFX_OP("lighter", ADD)
  else CANVAS_OP_TO_GFX_OP("xor", XOR)
  else CANVAS_OP_TO_GFX_OP("multiply", MULTIPLY)
  else CANVAS_OP_TO_GFX_OP("screen", SCREEN)
  else CANVAS_OP_TO_GFX_OP("overlay", OVERLAY)
  else CANVAS_OP_TO_GFX_OP("darken", DARKEN)
  else CANVAS_OP_TO_GFX_OP("lighten", LIGHTEN)
  else CANVAS_OP_TO_GFX_OP("color-dodge", COLOR_DODGE)
  else CANVAS_OP_TO_GFX_OP("color-burn", COLOR_BURN)
  else CANVAS_OP_TO_GFX_OP("hard-light", HARD_LIGHT)
  else CANVAS_OP_TO_GFX_OP("soft-light", SOFT_LIGHT)
  else CANVAS_OP_TO_GFX_OP("difference", DIFFERENCE)
  else CANVAS_OP_TO_GFX_OP("exclusion", EXCLUSION)
  else CANVAS_OP_TO_GFX_OP("hue", HUE)
  else CANVAS_OP_TO_GFX_OP("saturation", SATURATION)
  else CANVAS_OP_TO_GFX_OP("color", COLOR)
  else CANVAS_OP_TO_GFX_OP("luminosity", LUMINOSITY)
  // XXX ERRMSG we need to report an error to developers here! (bug 329026)
  else return;

#undef CANVAS_OP_TO_GFX_OP
  CurrentState().op = comp_op;
}

void CanvasRenderingContext2D::GetGlobalCompositeOperation(
    nsAString& aOp, ErrorResult& aError) {
  CompositionOp comp_op = CurrentState().op;

#define CANVAS_OP_TO_GFX_OP(cvsop, op2d) \
  if (comp_op == CompositionOp::OP_##op2d) aOp.AssignLiteral(cvsop);

  CANVAS_OP_TO_GFX_OP("copy", SOURCE)
  else CANVAS_OP_TO_GFX_OP("destination-atop", DEST_ATOP)
  else CANVAS_OP_TO_GFX_OP("destination-in", DEST_IN)
  else CANVAS_OP_TO_GFX_OP("destination-out", DEST_OUT)
  else CANVAS_OP_TO_GFX_OP("destination-over", DEST_OVER)
  else CANVAS_OP_TO_GFX_OP("lighter", ADD)
  else CANVAS_OP_TO_GFX_OP("source-atop", ATOP)
  else CANVAS_OP_TO_GFX_OP("source-in", IN)
  else CANVAS_OP_TO_GFX_OP("source-out", OUT)
  else CANVAS_OP_TO_GFX_OP("source-over", OVER)
  else CANVAS_OP_TO_GFX_OP("xor", XOR)
  else CANVAS_OP_TO_GFX_OP("multiply", MULTIPLY)
  else CANVAS_OP_TO_GFX_OP("screen", SCREEN)
  else CANVAS_OP_TO_GFX_OP("overlay", OVERLAY)
  else CANVAS_OP_TO_GFX_OP("darken", DARKEN)
  else CANVAS_OP_TO_GFX_OP("lighten", LIGHTEN)
  else CANVAS_OP_TO_GFX_OP("color-dodge", COLOR_DODGE)
  else CANVAS_OP_TO_GFX_OP("color-burn", COLOR_BURN)
  else CANVAS_OP_TO_GFX_OP("hard-light", HARD_LIGHT)
  else CANVAS_OP_TO_GFX_OP("soft-light", SOFT_LIGHT)
  else CANVAS_OP_TO_GFX_OP("difference", DIFFERENCE)
  else CANVAS_OP_TO_GFX_OP("exclusion", EXCLUSION)
  else CANVAS_OP_TO_GFX_OP("hue", HUE)
  else CANVAS_OP_TO_GFX_OP("saturation", SATURATION)
  else CANVAS_OP_TO_GFX_OP("color", COLOR)
  else CANVAS_OP_TO_GFX_OP("luminosity", LUMINOSITY)
  else {
    aError.Throw(NS_ERROR_FAILURE);
  }

#undef CANVAS_OP_TO_GFX_OP
}

void CanvasRenderingContext2D::DrawWindow(nsGlobalWindowInner& aWindow,
                                          double aX, double aY, double aW,
                                          double aH, const nsACString& aBgColor,
                                          uint32_t aFlags,
                                          ErrorResult& aError) {
  if (int32_t(aW) == 0 || int32_t(aH) == 0) {
    return;
  }

  // protect against too-large surfaces that will cause allocation
  // or overflow issues
  if (!Factory::CheckSurfaceSize(IntSize(int32_t(aW), int32_t(aH)), 0xffff)) {
    aError.Throw(NS_ERROR_FAILURE);
    return;
  }

  // Flush layout updates
  if (!(aFlags & CanvasRenderingContext2D_Binding::DRAWWINDOW_DO_NOT_FLUSH)) {
    nsContentUtils::FlushLayoutForTree(aWindow.GetOuterWindow());
  }

  CompositionOp op = UsedOperation();
  bool discardContent =
      GlobalAlpha() == 1.0f &&
      (op == CompositionOp::OP_OVER || op == CompositionOp::OP_SOURCE);
  const gfx::Rect drawRect(aX, aY, aW, aH);
  EnsureTarget(discardContent ? &drawRect : nullptr);
  if (!IsTargetValid()) {
    return;
  }

  RefPtr<nsPresContext> presContext;
  nsIDocShell* docshell = aWindow.GetDocShell();
  if (docshell) {
    presContext = docshell->GetPresContext();
  }
  if (!presContext) {
    aError.Throw(NS_ERROR_FAILURE);
    return;
  }

  nscolor backgroundColor;
  if (!ParseColor(aBgColor, &backgroundColor)) {
    aError.Throw(NS_ERROR_FAILURE);
    return;
  }

  nsRect r(nsPresContext::CSSPixelsToAppUnits((float)aX),
           nsPresContext::CSSPixelsToAppUnits((float)aY),
           nsPresContext::CSSPixelsToAppUnits((float)aW),
           nsPresContext::CSSPixelsToAppUnits((float)aH));
  RenderDocumentFlags renderDocFlags =
      (RenderDocumentFlags::IgnoreViewportScrolling |
       RenderDocumentFlags::DocumentRelative);
  if (aFlags & CanvasRenderingContext2D_Binding::DRAWWINDOW_DRAW_CARET) {
    renderDocFlags |= RenderDocumentFlags::DrawCaret;
  }
  if (aFlags & CanvasRenderingContext2D_Binding::DRAWWINDOW_DRAW_VIEW) {
    renderDocFlags &= ~(RenderDocumentFlags::IgnoreViewportScrolling |
                        RenderDocumentFlags::DocumentRelative);
  }
  if (aFlags & CanvasRenderingContext2D_Binding::DRAWWINDOW_USE_WIDGET_LAYERS) {
    renderDocFlags |= RenderDocumentFlags::UseWidgetLayers;
  }
  if (aFlags &
      CanvasRenderingContext2D_Binding::DRAWWINDOW_ASYNC_DECODE_IMAGES) {
    renderDocFlags |= RenderDocumentFlags::AsyncDecodeImages;
  }
  if (aFlags & CanvasRenderingContext2D_Binding::DRAWWINDOW_DO_NOT_FLUSH) {
    renderDocFlags |= RenderDocumentFlags::DrawWindowNotFlushing;
  }

  // gfxContext-over-Azure may modify the DrawTarget's transform, so
  // save and restore it
  Matrix matrix = mTarget->GetTransform();
  double sw = matrix._11 * aW;
  double sh = matrix._22 * aH;
  if (!sw || !sh) {
    return;
  }

  RefPtr<gfxContext> thebes;
  RefPtr<DrawTarget> drawDT;
  // Rendering directly is faster and can be done if mTarget supports Azure
  // and does not need alpha blending.
  // Since the pre-transaction callback calls ReturnTarget, we can't have a
  // gfxContext wrapped around it when using a shared buffer provider because
  // the DrawTarget's shared buffer may be unmapped in ReturnTarget.
  op = CompositionOp::OP_ADD;
  if (gfxPlatform::GetPlatform()->SupportsAzureContentForDrawTarget(mTarget) &&
      GlobalAlpha() == 1.0f) {
    op = UsedOperation();
    if (!IsTargetValid()) {
      aError.Throw(NS_ERROR_FAILURE);
      return;
    }
  }
  if (op == CompositionOp::OP_OVER &&
      (!mBufferProvider ||
       (mBufferProvider->GetType() != LayersBackend::LAYERS_CLIENT &&
        mBufferProvider->GetType() != LayersBackend::LAYERS_WR))) {
    thebes = gfxContext::CreateOrNull(mTarget);
    MOZ_ASSERT(thebes);  // already checked the draw target above
                         // (in SupportsAzureContentForDrawTarget)
    thebes->SetMatrix(matrix);
  } else {
    IntSize dtSize = IntSize::Ceil(sw, sh);
    if (!Factory::AllowedSurfaceSize(dtSize)) {
      // attempt to limit the DT to what will actually cover the target
      Size limitSize(mTarget->GetSize());
      limitSize.Scale(matrix._11, matrix._22);
      dtSize = Min(dtSize, IntSize::Ceil(limitSize));
      // if the DT is still too big, then error
      if (!Factory::AllowedSurfaceSize(dtSize)) {
        aError.Throw(NS_ERROR_FAILURE);
        return;
      }
    }
    drawDT = gfxPlatform::GetPlatform()->CreateOffscreenContentDrawTarget(
        dtSize, SurfaceFormat::B8G8R8A8);
    if (!drawDT || !drawDT->IsValid()) {
      aError.Throw(NS_ERROR_FAILURE);
      return;
    }

    thebes = gfxContext::CreateOrNull(drawDT);
    MOZ_ASSERT(thebes);  // alrady checked the draw target above
    thebes->SetMatrix(Matrix::Scaling(matrix._11, matrix._22));
  }

  RefPtr<PresShell> presShell = presContext->PresShell();

  Unused << presShell->RenderDocument(r, renderDocFlags, backgroundColor,
                                      thebes);
  // If this canvas was contained in the drawn window, the pre-transaction
  // callback may have returned its DT. If so, we must reacquire it here.
  EnsureTarget(discardContent ? &drawRect : nullptr);

  if (drawDT) {
    RefPtr<SourceSurface> snapshot = drawDT->Snapshot();
    if (NS_WARN_IF(!snapshot)) {
      aError.Throw(NS_ERROR_FAILURE);
      return;
    }
    RefPtr<DataSourceSurface> data = snapshot->GetDataSurface();
    if (!data || !Factory::AllowedSurfaceSize(data->GetSize())) {
      gfxCriticalError() << "Unexpected invalid data source surface "
                         << (data ? data->GetSize() : IntSize(0, 0));
      aError.Throw(NS_ERROR_FAILURE);
      return;
    }

    DataSourceSurface::MappedSurface rawData;
    if (NS_WARN_IF(!data->Map(DataSourceSurface::READ, &rawData))) {
      aError.Throw(NS_ERROR_FAILURE);
      return;
    }
    RefPtr<SourceSurface> source = mTarget->CreateSourceSurfaceFromData(
        rawData.mData, data->GetSize(), rawData.mStride, data->GetFormat());
    data->Unmap();

    if (!source) {
      aError.Throw(NS_ERROR_FAILURE);
      return;
    }

    op = UsedOperation();
    if (!IsTargetValid()) {
      aError.Throw(NS_ERROR_FAILURE);
      return;
    }
    gfx::Rect destRect(0, 0, aW, aH);
    gfx::Rect sourceRect(0, 0, sw, sh);
    mTarget->DrawSurface(source, destRect, sourceRect,
                         DrawSurfaceOptions(gfx::SamplingFilter::POINT),
                         DrawOptions(GlobalAlpha(), op, AntialiasMode::NONE));
  } else {
    mTarget->SetTransform(matrix);
  }

  // note that x and y are coordinates in the document that
  // we're drawing; x and y are drawn to 0,0 in current user
  // space.
  RedrawUser(gfxRect(0, 0, aW, aH));
}

//
// device pixel getting/setting
//

already_AddRefed<ImageData> CanvasRenderingContext2D::GetImageData(
    JSContext* aCx, double aSx, double aSy, double aSw, double aSh,
    nsIPrincipal& aSubjectPrincipal, ErrorResult& aError) {
  if (!mCanvasElement && !mDocShell) {
    NS_ERROR("No canvas element and no docshell in GetImageData!!!");
    aError.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return nullptr;
  }

  // Check only if we have a canvas element; if we were created with a docshell,
  // then it's special internal use.
  if (IsWriteOnly() ||
      (mCanvasElement && !mCanvasElement->CallerCanRead(aCx))) {
    // XXX ERRMSG we need to report an error to developers here! (bug 329026)
    aError.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return nullptr;
  }

  if (!IsFinite(aSx) || !IsFinite(aSy) || !IsFinite(aSw) || !IsFinite(aSh)) {
    aError.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
    return nullptr;
  }

  if (!aSw || !aSh) {
    aError.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return nullptr;
  }

  int32_t x = JS::ToInt32(aSx);
  int32_t y = JS::ToInt32(aSy);
  int32_t wi = JS::ToInt32(aSw);
  int32_t hi = JS::ToInt32(aSh);

  // Handle negative width and height by flipping the rectangle over in the
  // relevant direction.
  uint32_t w, h;
  if (aSw < 0) {
    w = -wi;
    x -= w;
  } else {
    w = wi;
  }
  if (aSh < 0) {
    h = -hi;
    y -= h;
  } else {
    h = hi;
  }

  if (w == 0) {
    w = 1;
  }
  if (h == 0) {
    h = 1;
  }

  JS::Rooted<JSObject*> array(aCx);
  aError =
      GetImageDataArray(aCx, x, y, w, h, aSubjectPrincipal, array.address());
  if (aError.Failed()) {
    return nullptr;
  }
  MOZ_ASSERT(array);

  RefPtr<ImageData> imageData = new ImageData(w, h, *array);
  return imageData.forget();
}

nsresult CanvasRenderingContext2D::GetImageDataArray(
    JSContext* aCx, int32_t aX, int32_t aY, uint32_t aWidth, uint32_t aHeight,
    nsIPrincipal& aSubjectPrincipal, JSObject** aRetval) {
  MOZ_ASSERT(aWidth && aHeight);

  CheckedInt<uint32_t> len = CheckedInt<uint32_t>(aWidth) * aHeight * 4;
  if (!len.isValid()) {
    return NS_ERROR_DOM_INDEX_SIZE_ERR;
  }

  CheckedInt<int32_t> rightMost = CheckedInt<int32_t>(aX) + aWidth;
  CheckedInt<int32_t> bottomMost = CheckedInt<int32_t>(aY) + aHeight;

  if (!rightMost.isValid() || !bottomMost.isValid()) {
    return NS_ERROR_DOM_SYNTAX_ERR;
  }

  JS::Rooted<JSObject*> darray(aCx, JS_NewUint8ClampedArray(aCx, len.value()));
  if (!darray) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  if (mZero) {
    *aRetval = darray;
    return NS_OK;
  }

  IntRect srcRect(0, 0, mWidth, mHeight);
  IntRect destRect(aX, aY, aWidth, aHeight);
  IntRect srcReadRect = srcRect.Intersect(destRect);
  if (srcReadRect.IsEmpty()) {
    *aRetval = darray;
    return NS_OK;
  }

  if (!mBufferProvider) {
    if (!EnsureTarget()) {
      return NS_ERROR_FAILURE;
    }
  }

  RefPtr<SourceSurface> snapshot = mBufferProvider->BorrowSnapshot();
  if (!snapshot) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  RefPtr<DataSourceSurface> readback = snapshot->GetDataSurface();
  mBufferProvider->ReturnSnapshot(snapshot.forget());

  DataSourceSurface::MappedSurface rawData;
  if (!readback || !readback->Map(DataSourceSurface::READ, &rawData)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  IntRect dstWriteRect = srcReadRect;
  dstWriteRect.MoveBy(-aX, -aY);

  // Check for site-specific permission.  This check is not needed if the
  // canvas was created with a docshell (that is only done for special
  // internal uses).
  bool usePlaceholder = false;
  if (mCanvasElement) {
    nsCOMPtr<Document> ownerDoc = mCanvasElement->OwnerDoc();
    usePlaceholder = !CanvasUtils::IsImageExtractionAllowed(ownerDoc, aCx,
                                                            aSubjectPrincipal);
  }

  do {
    JS::AutoCheckCannotGC nogc;
    bool isShared;
    uint8_t* data = JS_GetUint8ClampedArrayData(darray, &isShared, nogc);
    MOZ_ASSERT(!isShared);  // Should not happen, data was created above

    uint32_t srcStride = rawData.mStride;
    uint8_t* src =
        rawData.mData + srcReadRect.y * srcStride + srcReadRect.x * 4;

    // Return all-white, opaque pixel data if no permission.
    if (usePlaceholder) {
      memset(data, 0xFF, len.value());
      break;
    }

    uint8_t* dst = data + dstWriteRect.y * (aWidth * 4) + dstWriteRect.x * 4;

    if (mOpaque) {
      SwizzleData(src, srcStride, SurfaceFormat::X8R8G8B8_UINT32, dst,
                  aWidth * 4, SurfaceFormat::R8G8B8A8, dstWriteRect.Size());
    } else {
      UnpremultiplyData(src, srcStride, SurfaceFormat::A8R8G8B8_UINT32, dst,
                        aWidth * 4, SurfaceFormat::R8G8B8A8,
                        dstWriteRect.Size());
    }
  } while (false);

  readback->Unmap();
  *aRetval = darray;
  return NS_OK;
}

void CanvasRenderingContext2D::EnsureErrorTarget() {
  if (sErrorTarget) {
    return;
  }

  RefPtr<DrawTarget> errorTarget =
      gfxPlatform::GetPlatform()->CreateOffscreenCanvasDrawTarget(
          IntSize(1, 1), SurfaceFormat::B8G8R8A8);
  MOZ_ASSERT(errorTarget, "Failed to allocate the error target!");

  sErrorTarget = errorTarget;
  NS_ADDREF(sErrorTarget);
}

void CanvasRenderingContext2D::FillRuleChanged() {
  if (mPath) {
    mPathBuilder = mPath->CopyToBuilder(CurrentState().fillRule);
    mPath = nullptr;
  }
}

void CanvasRenderingContext2D::PutImageData(ImageData& aImageData, double aDx,
                                            double aDy, ErrorResult& aError) {
  RootedSpiderMonkeyInterface<Uint8ClampedArray> arr(RootingCx());
  DebugOnly<bool> inited = arr.Init(aImageData.GetDataObject());
  MOZ_ASSERT(inited);

  aError = PutImageData_explicit(JS::ToInt32(aDx), JS::ToInt32(aDy),
                                 aImageData.Width(), aImageData.Height(), &arr,
                                 false, 0, 0, 0, 0);
}

void CanvasRenderingContext2D::PutImageData(ImageData& aImageData, double aDx,
                                            double aDy, double aDirtyX,
                                            double aDirtyY, double aDirtyWidth,
                                            double aDirtyHeight,
                                            ErrorResult& aError) {
  RootedSpiderMonkeyInterface<Uint8ClampedArray> arr(RootingCx());
  DebugOnly<bool> inited = arr.Init(aImageData.GetDataObject());
  MOZ_ASSERT(inited);

  aError = PutImageData_explicit(JS::ToInt32(aDx), JS::ToInt32(aDy),
                                 aImageData.Width(), aImageData.Height(), &arr,
                                 true, JS::ToInt32(aDirtyX),
                                 JS::ToInt32(aDirtyY), JS::ToInt32(aDirtyWidth),
                                 JS::ToInt32(aDirtyHeight));
}

nsresult CanvasRenderingContext2D::PutImageData_explicit(
    int32_t aX, int32_t aY, uint32_t aW, uint32_t aH,
    dom::Uint8ClampedArray* aArray, bool aHasDirtyRect, int32_t aDirtyX,
    int32_t aDirtyY, int32_t aDirtyWidth, int32_t aDirtyHeight) {
  if (aW == 0 || aH == 0) {
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }

  IntRect dirtyRect;
  IntRect imageDataRect(0, 0, aW, aH);

  if (aHasDirtyRect) {
    // fix up negative dimensions
    if (aDirtyWidth < 0) {
      NS_ENSURE_TRUE(aDirtyWidth != INT_MIN, NS_ERROR_DOM_INDEX_SIZE_ERR);

      CheckedInt32 checkedDirtyX = CheckedInt32(aDirtyX) + aDirtyWidth;

      if (!checkedDirtyX.isValid()) return NS_ERROR_DOM_INDEX_SIZE_ERR;

      aDirtyX = checkedDirtyX.value();
      aDirtyWidth = -aDirtyWidth;
    }

    if (aDirtyHeight < 0) {
      NS_ENSURE_TRUE(aDirtyHeight != INT_MIN, NS_ERROR_DOM_INDEX_SIZE_ERR);

      CheckedInt32 checkedDirtyY = CheckedInt32(aDirtyY) + aDirtyHeight;

      if (!checkedDirtyY.isValid()) return NS_ERROR_DOM_INDEX_SIZE_ERR;

      aDirtyY = checkedDirtyY.value();
      aDirtyHeight = -aDirtyHeight;
    }

    // bound the dirty rect within the imageData rectangle
    dirtyRect = imageDataRect.Intersect(
        IntRect(aDirtyX, aDirtyY, aDirtyWidth, aDirtyHeight));

    if (dirtyRect.Width() <= 0 || dirtyRect.Height() <= 0) return NS_OK;
  } else {
    dirtyRect = imageDataRect;
  }

  dirtyRect.MoveBy(IntPoint(aX, aY));
  dirtyRect = IntRect(0, 0, mWidth, mHeight).Intersect(dirtyRect);

  if (dirtyRect.Width() <= 0 || dirtyRect.Height() <= 0) {
    return NS_OK;
  }

  aArray->ComputeState();

  uint32_t dataLen = aArray->Length();

  uint32_t len = aW * aH * 4;
  if (dataLen != len) {
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }

  // The canvas spec says that the current path, transformation matrix, shadow
  // attributes, global alpha, the clipping region, and global composition
  // operator must not affect the getImageData() and putImageData() methods.
  const gfx::Rect putRect(dirtyRect);
  EnsureTarget(&putRect);

  if (!IsTargetValid()) {
    return NS_ERROR_FAILURE;
  }

  DataSourceSurface::MappedSurface map;
  RefPtr<DataSourceSurface> sourceSurface;
  uint8_t* lockedBits = nullptr;
  uint8_t* dstData;
  IntSize dstSize;
  int32_t dstStride;
  SurfaceFormat dstFormat;
  if (mTarget->LockBits(&lockedBits, &dstSize, &dstStride, &dstFormat)) {
    dstData = lockedBits + dirtyRect.y * dstStride + dirtyRect.x * 4;
  } else {
    sourceSurface = Factory::CreateDataSourceSurface(
        dirtyRect.Size(), SurfaceFormat::B8G8R8A8, false);

    // In certain scenarios, requesting larger than 8k image fails.  Bug 803568
    // covers the details of how to run into it, but the full detailed
    // investigation hasn't been done to determine the underlying cause.  We
    // will just handle the failure to allocate the surface to avoid a crash.
    if (!sourceSurface) {
      return NS_ERROR_FAILURE;
    }
    if (!sourceSurface->Map(DataSourceSurface::READ_WRITE, &map)) {
      return NS_ERROR_FAILURE;
    }

    dstData = map.mData;
    if (!dstData) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    dstStride = map.mStride;
    dstFormat = sourceSurface->GetFormat();
  }

  IntRect srcRect = dirtyRect - IntPoint(aX, aY);
  uint8_t* srcData = aArray->Data() + srcRect.y * (aW * 4) + srcRect.x * 4;

  PremultiplyData(
      srcData, aW * 4, SurfaceFormat::R8G8B8A8, dstData, dstStride,
      mOpaque ? SurfaceFormat::X8R8G8B8_UINT32 : SurfaceFormat::A8R8G8B8_UINT32,
      dirtyRect.Size());

  if (lockedBits) {
    mTarget->ReleaseBits(lockedBits);
  } else if (sourceSurface) {
    sourceSurface->Unmap();
    mTarget->CopySurface(sourceSurface, dirtyRect - dirtyRect.TopLeft(),
                         dirtyRect.TopLeft());
  }

  Redraw(
      gfx::Rect(dirtyRect.x, dirtyRect.y, dirtyRect.width, dirtyRect.height));

  return NS_OK;
}

static already_AddRefed<ImageData> CreateImageData(
    JSContext* aCx, CanvasRenderingContext2D* aContext, uint32_t aW,
    uint32_t aH, ErrorResult& aError) {
  if (aW == 0) aW = 1;
  if (aH == 0) aH = 1;

  CheckedInt<uint32_t> len = CheckedInt<uint32_t>(aW) * aH * 4;
  if (!len.isValid()) {
    aError.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return nullptr;
  }

  // Create the fast typed array; it's initialized to 0 by default.
  JSObject* darray = Uint8ClampedArray::Create(aCx, aContext, len.value());
  if (!darray) {
    aError.Throw(NS_ERROR_OUT_OF_MEMORY);
    return nullptr;
  }

  RefPtr<mozilla::dom::ImageData> imageData =
      new mozilla::dom::ImageData(aW, aH, *darray);
  return imageData.forget();
}

already_AddRefed<ImageData> CanvasRenderingContext2D::CreateImageData(
    JSContext* aCx, double aSw, double aSh, ErrorResult& aError) {
  if (!aSw || !aSh) {
    aError.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return nullptr;
  }

  int32_t wi = JS::ToInt32(aSw);
  int32_t hi = JS::ToInt32(aSh);

  uint32_t w = Abs(wi);
  uint32_t h = Abs(hi);
  return mozilla::dom::CreateImageData(aCx, this, w, h, aError);
}

already_AddRefed<ImageData> CanvasRenderingContext2D::CreateImageData(
    JSContext* aCx, ImageData& aImagedata, ErrorResult& aError) {
  return mozilla::dom::CreateImageData(aCx, this, aImagedata.Width(),
                                       aImagedata.Height(), aError);
}

static uint8_t g2DContextLayerUserData;

already_AddRefed<Layer> CanvasRenderingContext2D::GetCanvasLayer(
    nsDisplayListBuilder* aBuilder, Layer* aOldLayer, LayerManager* aManager) {
  if (mOpaque) {
    // If we're opaque then make sure we have a surface so we paint black
    // instead of transparent.
    EnsureTarget();
  }

  // Don't call EnsureTarget() ... if there isn't already a surface, then
  // we have nothing to paint and there is no need to create a surface just
  // to paint nothing. Also, EnsureTarget() can cause creation of a persistent
  // layer manager which must NOT happen during a paint.
  if (!mBufferProvider && !IsTargetValid()) {
    // No DidTransactionCallback will be received, so mark the context clean
    // now so future invalidations will be dispatched.
    MarkContextClean();
    return nullptr;
  }

  if (!mResetLayer && aOldLayer) {
    auto userData = static_cast<CanvasRenderingContext2DUserData*>(
        aOldLayer->GetUserData(&g2DContextLayerUserData));

    CanvasInitializeData data;

    data.mBufferProvider = mBufferProvider;

    if (userData && userData->IsForContext(this) &&
        static_cast<CanvasLayer*>(aOldLayer)
            ->CreateOrGetCanvasRenderer()
            ->IsDataValid(data)) {
      RefPtr<Layer> ret = aOldLayer;
      return ret.forget();
    }
  }

  RefPtr<CanvasLayer> canvasLayer = aManager->CreateCanvasLayer();
  if (!canvasLayer) {
    NS_WARNING("CreateCanvasLayer returned null!");
    // No DidTransactionCallback will be received, so mark the context clean
    // now so future invalidations will be dispatched.
    MarkContextClean();
    return nullptr;
  }
  CanvasRenderingContext2DUserData* userData = nullptr;
  // Make the layer tell us whenever a transaction finishes (including
  // the current transaction), so we can clear our invalidation state and
  // start invalidating again. We need to do this for all layers since
  // callers of DrawWindow may be expecting to receive normal invalidation
  // notifications after this paint.

  // The layer will be destroyed when we tear down the presentation
  // (at the latest), at which time this userData will be destroyed,
  // releasing the reference to the element.
  // The userData will receive DidTransactionCallbacks, which flush the
  // the invalidation state to indicate that the canvas is up to date.
  userData = new CanvasRenderingContext2DUserData(this);
  canvasLayer->SetUserData(&g2DContextLayerUserData, userData);

  CanvasRenderer* canvasRenderer = canvasLayer->CreateOrGetCanvasRenderer();
  InitializeCanvasRenderer(aBuilder, canvasRenderer);
  uint32_t flags = mOpaque ? Layer::CONTENT_OPAQUE : 0;
  canvasLayer->SetContentFlags(flags);

  mResetLayer = false;

  return canvasLayer.forget();
}

bool CanvasRenderingContext2D::UpdateWebRenderCanvasData(
    nsDisplayListBuilder* aBuilder, WebRenderCanvasData* aCanvasData) {
  if (mOpaque) {
    // If we're opaque then make sure we have a surface so we paint black
    // instead of transparent.
    EnsureTarget();
  }

  // Don't call EnsureTarget() ... if there isn't already a surface, then
  // we have nothing to paint and there is no need to create a surface just
  // to paint nothing. Also, EnsureTarget() can cause creation of a persistent
  // layer manager which must NOT happen during a paint.
  if (!mBufferProvider && !IsTargetValid()) {
    // No DidTransactionCallback will be received, so mark the context clean
    // now so future invalidations will be dispatched.
    MarkContextClean();
    // Clear CanvasRenderer of WebRenderCanvasData
    aCanvasData->ClearCanvasRenderer();
    return false;
  }

  CanvasRenderer* renderer = aCanvasData->GetCanvasRenderer();

  if (!mResetLayer && renderer) {
    CanvasInitializeData data;

    data.mBufferProvider = mBufferProvider;

    if (renderer->IsDataValid(data)) {
      return true;
    }
  }

  renderer = aCanvasData->CreateCanvasRenderer();
  if (!InitializeCanvasRenderer(aBuilder, renderer)) {
    // Clear CanvasRenderer of WebRenderCanvasData
    aCanvasData->ClearCanvasRenderer();
    return false;
  }

  MOZ_ASSERT(renderer);
  mResetLayer = false;
  return true;
}

bool CanvasRenderingContext2D::InitializeCanvasRenderer(
    nsDisplayListBuilder* aBuilder, CanvasRenderer* aRenderer) {
  CanvasInitializeData data;
  data.mSize = GetSize();
  data.mHasAlpha = !mOpaque;
  data.mPreTransCallback =
      CanvasRenderingContext2DUserData::PreTransactionCallback;
  data.mPreTransCallbackData = this;
  data.mDidTransCallback =
      CanvasRenderingContext2DUserData::DidTransactionCallback;
  data.mDidTransCallbackData = this;

  if (!mBufferProvider) {
    // Force the creation of a buffer provider.
    EnsureTarget();
    ReturnTarget();
    if (!mBufferProvider) {
      MarkContextClean();
      return false;
    }
  }

  data.mBufferProvider = mBufferProvider;

  aRenderer->Initialize(data);
  aRenderer->SetDirty();
  return true;
}

void CanvasRenderingContext2D::MarkContextClean() {
  if (mInvalidateCount > 0) {
    mPredictManyRedrawCalls = mInvalidateCount > kCanvasMaxInvalidateCount;
  }
  mIsEntireFrameInvalid = false;
  mInvalidateCount = 0;
}

void CanvasRenderingContext2D::MarkContextCleanForFrameCapture() {
  mIsCapturedFrameInvalid = false;
}

bool CanvasRenderingContext2D::IsContextCleanForFrameCapture() {
  return !mIsCapturedFrameInvalid;
}

bool CanvasRenderingContext2D::ShouldForceInactiveLayer(
    LayerManager* aManager) {
  return !aManager->CanUseCanvasLayerForSize(GetSize());
}

void CanvasRenderingContext2D::SetWriteOnly() {
  mWriteOnly = true;
  if (mCanvasElement) {
    mCanvasElement->SetWriteOnly();
  }
}

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(CanvasPath, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(CanvasPath, Release)

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(CanvasPath, mParent)

CanvasPath::CanvasPath(nsISupports* aParent) : mParent(aParent) {
  mPathBuilder = gfxPlatform::GetPlatform()
                     ->ScreenReferenceDrawTarget()
                     ->CreatePathBuilder();
}

CanvasPath::CanvasPath(nsISupports* aParent,
                       already_AddRefed<PathBuilder> aPathBuilder)
    : mParent(aParent), mPathBuilder(aPathBuilder) {
  if (!mPathBuilder) {
    mPathBuilder = gfxPlatform::GetPlatform()
                       ->ScreenReferenceDrawTarget()
                       ->CreatePathBuilder();
  }
}

JSObject* CanvasPath::WrapObject(JSContext* aCx,
                                 JS::Handle<JSObject*> aGivenProto) {
  return Path2D_Binding::Wrap(aCx, this, aGivenProto);
}

already_AddRefed<CanvasPath> CanvasPath::Constructor(
    const GlobalObject& aGlobal) {
  RefPtr<CanvasPath> path = new CanvasPath(aGlobal.GetAsSupports());
  return path.forget();
}

already_AddRefed<CanvasPath> CanvasPath::Constructor(
    const GlobalObject& aGlobal, CanvasPath& aCanvasPath) {
  RefPtr<gfx::Path> tempPath = aCanvasPath.GetPath(
      CanvasWindingRule::Nonzero,
      gfxPlatform::GetPlatform()->ScreenReferenceDrawTarget().get());

  RefPtr<CanvasPath> path =
      new CanvasPath(aGlobal.GetAsSupports(), tempPath->CopyToBuilder());
  return path.forget();
}

already_AddRefed<CanvasPath> CanvasPath::Constructor(
    const GlobalObject& aGlobal, const nsAString& aPathString) {
  RefPtr<gfx::Path> tempPath = SVGContentUtils::GetPath(aPathString);
  if (!tempPath) {
    return Constructor(aGlobal);
  }

  RefPtr<CanvasPath> path =
      new CanvasPath(aGlobal.GetAsSupports(), tempPath->CopyToBuilder());
  return path.forget();
}

void CanvasPath::ClosePath() {
  EnsurePathBuilder();

  mPathBuilder->Close();
}

void CanvasPath::MoveTo(double aX, double aY) {
  EnsurePathBuilder();

  mPathBuilder->MoveTo(Point(ToFloat(aX), ToFloat(aY)));
}

void CanvasPath::LineTo(double aX, double aY) {
  EnsurePathBuilder();

  mPathBuilder->LineTo(Point(ToFloat(aX), ToFloat(aY)));
}

void CanvasPath::QuadraticCurveTo(double aCpx, double aCpy, double aX,
                                  double aY) {
  EnsurePathBuilder();

  mPathBuilder->QuadraticBezierTo(gfx::Point(ToFloat(aCpx), ToFloat(aCpy)),
                                  gfx::Point(ToFloat(aX), ToFloat(aY)));
}

void CanvasPath::BezierCurveTo(double aCp1x, double aCp1y, double aCp2x,
                               double aCp2y, double aX, double aY) {
  BezierTo(gfx::Point(ToFloat(aCp1x), ToFloat(aCp1y)),
           gfx::Point(ToFloat(aCp2x), ToFloat(aCp2y)),
           gfx::Point(ToFloat(aX), ToFloat(aY)));
}

void CanvasPath::ArcTo(double aX1, double aY1, double aX2, double aY2,
                       double aRadius, ErrorResult& aError) {
  if (aRadius < 0) {
    aError.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return;
  }

  EnsurePathBuilder();

  // Current point in user space!
  Point p0 = mPathBuilder->CurrentPoint();
  Point p1(aX1, aY1);
  Point p2(aX2, aY2);

  // Execute these calculations in double precision to avoid cumulative
  // rounding errors.
  double dir, a2, b2, c2, cosx, sinx, d, anx, any, bnx, bny, x3, y3, x4, y4, cx,
      cy, angle0, angle1;
  bool anticlockwise;

  if (p0 == p1 || p1 == p2 || aRadius == 0) {
    LineTo(p1.x, p1.y);
    return;
  }

  // Check for colinearity
  dir = (p2.x - p1.x) * (p0.y - p1.y) + (p2.y - p1.y) * (p1.x - p0.x);
  if (dir == 0) {
    LineTo(p1.x, p1.y);
    return;
  }

  // XXX - Math for this code was already available from the non-azure code
  // and would be well tested. Perhaps converting to bezier directly might
  // be more efficient longer run.
  a2 = (p0.x - aX1) * (p0.x - aX1) + (p0.y - aY1) * (p0.y - aY1);
  b2 = (aX1 - aX2) * (aX1 - aX2) + (aY1 - aY2) * (aY1 - aY2);
  c2 = (p0.x - aX2) * (p0.x - aX2) + (p0.y - aY2) * (p0.y - aY2);
  cosx = (a2 + b2 - c2) / (2 * sqrt(a2 * b2));

  sinx = sqrt(1 - cosx * cosx);
  d = aRadius / ((1 - cosx) / sinx);

  anx = (aX1 - p0.x) / sqrt(a2);
  any = (aY1 - p0.y) / sqrt(a2);
  bnx = (aX1 - aX2) / sqrt(b2);
  bny = (aY1 - aY2) / sqrt(b2);
  x3 = aX1 - anx * d;
  y3 = aY1 - any * d;
  x4 = aX1 - bnx * d;
  y4 = aY1 - bny * d;
  anticlockwise = (dir < 0);
  cx = x3 + any * aRadius * (anticlockwise ? 1 : -1);
  cy = y3 - anx * aRadius * (anticlockwise ? 1 : -1);
  angle0 = atan2((y3 - cy), (x3 - cx));
  angle1 = atan2((y4 - cy), (x4 - cx));

  LineTo(x3, y3);

  Arc(cx, cy, aRadius, angle0, angle1, anticlockwise, aError);
}

void CanvasPath::Rect(double aX, double aY, double aW, double aH) {
  MoveTo(aX, aY);
  LineTo(aX + aW, aY);
  LineTo(aX + aW, aY + aH);
  LineTo(aX, aY + aH);
  ClosePath();
}

void CanvasPath::Arc(double aX, double aY, double aRadius, double aStartAngle,
                     double aEndAngle, bool aAnticlockwise,
                     ErrorResult& aError) {
  if (aRadius < 0.0) {
    aError.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return;
  }

  EnsurePathBuilder();

  ArcToBezier(this, Point(aX, aY), Size(aRadius, aRadius), aStartAngle,
              aEndAngle, aAnticlockwise);
}

void CanvasPath::Ellipse(double x, double y, double radiusX, double radiusY,
                         double rotation, double startAngle, double endAngle,
                         bool anticlockwise, ErrorResult& error) {
  if (radiusX < 0.0 || radiusY < 0.0) {
    error.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return;
  }

  EnsurePathBuilder();

  ArcToBezier(this, Point(x, y), Size(radiusX, radiusY), startAngle, endAngle,
              anticlockwise, rotation);
}

void CanvasPath::LineTo(const gfx::Point& aPoint) {
  EnsurePathBuilder();

  mPathBuilder->LineTo(aPoint);
}

void CanvasPath::BezierTo(const gfx::Point& aCP1, const gfx::Point& aCP2,
                          const gfx::Point& aCP3) {
  EnsurePathBuilder();

  mPathBuilder->BezierTo(aCP1, aCP2, aCP3);
}

void CanvasPath::AddPath(CanvasPath& aCanvasPath, const DOMMatrix2DInit& aInit,
                         ErrorResult& aError) {
  RefPtr<gfx::Path> tempPath = aCanvasPath.GetPath(
      CanvasWindingRule::Nonzero,
      gfxPlatform::GetPlatform()->ScreenReferenceDrawTarget().get());

  RefPtr<DOMMatrixReadOnly> matrix =
      DOMMatrixReadOnly::FromMatrix(GetParentObject(), aInit, aError);
  if (aError.Failed()) {
    return;
  }

  Matrix transform(*(matrix->GetInternal2D()));

  if (!transform.IsFinite()) {
    return;
  }

  if (!transform.IsIdentity()) {
    RefPtr<PathBuilder> tempBuilder =
        tempPath->TransformedCopyToBuilder(transform, FillRule::FILL_WINDING);
    tempPath = tempBuilder->Finish();
  }

  EnsurePathBuilder();  // in case a path is added to itself
  tempPath->StreamToSink(mPathBuilder);
}

already_AddRefed<gfx::Path> CanvasPath::GetPath(
    const CanvasWindingRule& aWinding, const DrawTarget* aTarget) const {
  FillRule fillRule = FillRule::FILL_WINDING;
  if (aWinding == CanvasWindingRule::Evenodd) {
    fillRule = FillRule::FILL_EVEN_ODD;
  }

  if (mPath && (mPath->GetBackendType() == aTarget->GetBackendType()) &&
      (mPath->GetFillRule() == fillRule)) {
    RefPtr<gfx::Path> path(mPath);
    return path.forget();
  }

  if (!mPath) {
    // if there is no path, there must be a pathbuilder
    MOZ_ASSERT(mPathBuilder);
    mPath = mPathBuilder->Finish();
    if (!mPath) {
      RefPtr<gfx::Path> path(mPath);
      return path.forget();
    }

    mPathBuilder = nullptr;
  }

  // retarget our backend if we're used with a different backend
  if (mPath->GetBackendType() != aTarget->GetBackendType()) {
    RefPtr<PathBuilder> tmpPathBuilder = aTarget->CreatePathBuilder(fillRule);
    mPath->StreamToSink(tmpPathBuilder);
    mPath = tmpPathBuilder->Finish();
  } else if (mPath->GetFillRule() != fillRule) {
    RefPtr<PathBuilder> tmpPathBuilder = mPath->CopyToBuilder(fillRule);
    mPath = tmpPathBuilder->Finish();
  }

  RefPtr<gfx::Path> path(mPath);
  return path.forget();
}

void CanvasPath::EnsurePathBuilder() const {
  if (mPathBuilder) {
    return;
  }

  // if there is not pathbuilder, there must be a path
  MOZ_ASSERT(mPath);
  mPathBuilder = mPath->CopyToBuilder();
  mPath = nullptr;
}

size_t BindingJSObjectMallocBytes(CanvasRenderingContext2D* aContext) {
  int32_t width = aContext->GetWidth();
  int32_t height = aContext->GetHeight();

  // TODO: Bug 1552137: No memory will be allocated if either dimension is
  // greater than gfxPrefs::gfx_canvas_max_size(). We should check this here
  // too.

  CheckedInt<uint32_t> bytes = CheckedInt<uint32_t>(width) * height * 4;
  if (!bytes.isValid()) {
    return 0;
  }

  return bytes.value();
}

}  // namespace dom
}  // namespace mozilla
