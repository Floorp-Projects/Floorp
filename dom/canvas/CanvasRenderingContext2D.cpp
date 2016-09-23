/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CanvasRenderingContext2D.h"

#include "mozilla/gfx/Helpers.h"
#include "nsXULElement.h"

#include "nsAutoPtr.h"
#include "nsIServiceManager.h"
#include "nsMathUtils.h"
#include "SVGImageContext.h"

#include "nsContentUtils.h"

#include "nsIDocument.h"
#include "mozilla/dom/HTMLCanvasElement.h"
#include "nsSVGEffects.h"
#include "nsPresContext.h"
#include "nsIPresShell.h"

#include "nsIInterfaceRequestorUtils.h"
#include "nsIFrame.h"
#include "nsError.h"

#include "nsCSSParser.h"
#include "mozilla/css/StyleRule.h"
#include "mozilla/css/Declaration.h"
#include "nsComputedDOMStyle.h"
#include "nsStyleSet.h"

#include "nsPrintfCString.h"

#include "nsReadableUtils.h"

#include "nsColor.h"
#include "nsGfxCIID.h"
#include "nsIDocShell.h"
#include "nsIDOMWindow.h"
#include "nsPIDOMWindow.h"
#include "nsDisplayList.h"
#include "nsFocusManager.h"
#include "nsContentUtils.h"

#include "nsTArray.h"

#include "ImageEncoder.h"
#include "ImageRegion.h"

#include "gfxContext.h"
#include "gfxImageSurface.h"
#include "gfxPlatform.h"
#include "gfxFont.h"
#include "gfxBlur.h"
#include "gfxPrefs.h"
#include "gfxUtils.h"

#include "nsFrameLoader.h"
#include "nsBidi.h"
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
#include "js/Conversions.h"

#include "mozilla/Alignment.h"
#include "mozilla/Assertions.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/dom/ContentParent.h"
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
#include "mozilla/ipc/DocumentRendererParent.h"
#include "mozilla/ipc/PDocumentRendererParent.h"
#include "mozilla/layers/PersistentBufferProvider.h"
#include "mozilla/MathAlgorithms.h"
#include "mozilla/Preferences.h"
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
#include "mozilla/dom/SVGMatrix.h"
#include "mozilla/dom/TextMetrics.h"
#include "mozilla/dom/SVGMatrix.h"
#include "mozilla/FloatingPoint.h"
#include "nsGlobalWindow.h"
#include "GLContext.h"
#include "GLContextProvider.h"
#include "SVGContentUtils.h"
#include "nsIScreenManager.h"
#include "nsFilterInstance.h"
#include "nsSVGLength2.h"
#include "nsDeviceContext.h"
#include "nsFontMetrics.h"
#include "Units.h"
#include "CanvasUtils.h"
#include "mozilla/StyleSetHandle.h"
#include "mozilla/StyleSetHandleInlines.h"
#include "mozilla/layers/CanvasClient.h"

#undef free // apparently defined by some windows header, clashing with a free()
            // method in SkTypes.h
#include "SkiaGLGlue.h"
#ifdef USE_SKIA
#include "SurfaceTypes.h"
#include "GLBlitHelper.h"
#endif

using mozilla::gl::GLContext;
using mozilla::gl::SkiaGLGlue;
using mozilla::gl::GLContextProvider;

#ifdef XP_WIN
#include "gfxWindowsPlatform.h"
#endif

#ifdef MOZ_WIDGET_GONK
#include "mozilla/layers/ShadowLayers.h"
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

// This is KIND_OTHER because it's not always clear where in memory the pixels
// of a canvas are stored.  Furthermore, this memory will be tracked by the
// underlying surface implementations.  See bug 655638 for details.
class Canvas2dPixelsReporter final : public nsIMemoryReporter
{
  ~Canvas2dPixelsReporter() {}
public:
  NS_DECL_ISUPPORTS

  NS_IMETHOD CollectReports(nsIHandleReportCallback* aHandleReport,
                            nsISupports* aData, bool aAnonymize) override
  {
    MOZ_COLLECT_REPORT(
      "canvas-2d-pixels", KIND_OTHER, UNITS_BYTES, gCanvasAzureMemoryUsed,
      "Memory used by 2D canvases. Each canvas requires "
      "(width * height * 4) bytes.");

    return NS_OK;
  }
};

NS_IMPL_ISUPPORTS(Canvas2dPixelsReporter, nsIMemoryReporter)

class CanvasRadialGradient : public CanvasGradient
{
public:
  CanvasRadialGradient(CanvasRenderingContext2D* aContext,
                       const Point& aBeginOrigin, Float aBeginRadius,
                       const Point& aEndOrigin, Float aEndRadius)
    : CanvasGradient(aContext, Type::RADIAL)
    , mCenter1(aBeginOrigin)
    , mCenter2(aEndOrigin)
    , mRadius1(aBeginRadius)
    , mRadius2(aEndRadius)
  {
  }

  Point mCenter1;
  Point mCenter2;
  Float mRadius1;
  Float mRadius2;
};

class CanvasLinearGradient : public CanvasGradient
{
public:
  CanvasLinearGradient(CanvasRenderingContext2D* aContext,
                       const Point& aBegin, const Point& aEnd)
    : CanvasGradient(aContext, Type::LINEAR)
    , mBegin(aBegin)
    , mEnd(aEnd)
  {
  }

protected:
  friend struct CanvasBidiProcessor;
  friend class CanvasGeneralPattern;

  // Beginning of linear gradient.
  Point mBegin;
  // End of linear gradient.
  Point mEnd;
};

bool
CanvasRenderingContext2D::PatternIsOpaque(CanvasRenderingContext2D::Style aStyle) const
{
  const ContextState& state = CurrentState();
  if (state.globalAlpha < 1.0) {
    return false;
  }

  if (state.patternStyles[aStyle] && state.patternStyles[aStyle]->mSurface) {
    return IsOpaqueFormat(state.patternStyles[aStyle]->mSurface->GetFormat());
  }

  // TODO: for gradient patterns we could check that all stops are opaque
  // colors.

  if (!state.gradientStyles[aStyle]) {
    // it's a color pattern.
    return Color::FromABGR(state.colorStyles[aStyle]).a >= 1.0;
  }

  return false;
}

// This class is named 'GeneralCanvasPattern' instead of just
// 'GeneralPattern' to keep Windows PGO builds from confusing the
// GeneralPattern class in gfxContext.cpp with this one.
class CanvasGeneralPattern
{
public:
  typedef CanvasRenderingContext2D::Style Style;
  typedef CanvasRenderingContext2D::ContextState ContextState;

  Pattern& ForStyle(CanvasRenderingContext2D* aCtx,
                    Style aStyle,
                    DrawTarget* aRT)
  {
    // This should only be called once or the mPattern destructor will
    // not be executed.
    NS_ASSERTION(!mPattern.GetPattern(), "ForStyle() should only be called once on CanvasGeneralPattern!");

    const ContextState &state = aCtx->CurrentState();

    if (state.StyleIsColor(aStyle)) {
      mPattern.InitColorPattern(ToDeviceColor(state.colorStyles[aStyle]));
    } else if (state.gradientStyles[aStyle] &&
               state.gradientStyles[aStyle]->GetType() == CanvasGradient::Type::LINEAR) {
      CanvasLinearGradient *gradient =
        static_cast<CanvasLinearGradient*>(state.gradientStyles[aStyle].get());

      mPattern.InitLinearGradientPattern(gradient->mBegin, gradient->mEnd,
                                         gradient->GetGradientStopsForTarget(aRT));
    } else if (state.gradientStyles[aStyle] &&
               state.gradientStyles[aStyle]->GetType() == CanvasGradient::Type::RADIAL) {
      CanvasRadialGradient *gradient =
        static_cast<CanvasRadialGradient*>(state.gradientStyles[aStyle].get());

      mPattern.InitRadialGradientPattern(gradient->mCenter1, gradient->mCenter2,
                                         gradient->mRadius1, gradient->mRadius2,
                                         gradient->GetGradientStopsForTarget(aRT));
    } else if (state.patternStyles[aStyle]) {
      if (aCtx->mCanvasElement) {
        CanvasUtils::DoDrawImageSecurityCheck(aCtx->mCanvasElement,
                                              state.patternStyles[aStyle]->mPrincipal,
                                              state.patternStyles[aStyle]->mForceWriteOnly,
                                              state.patternStyles[aStyle]->mCORSUsed);
      }

      ExtendMode mode;
      if (state.patternStyles[aStyle]->mRepeat == CanvasPattern::RepeatMode::NOREPEAT) {
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
class AdjustedTargetForFilter
{
public:
  typedef CanvasRenderingContext2D::ContextState ContextState;

  AdjustedTargetForFilter(CanvasRenderingContext2D* aCtx,
                          DrawTarget* aFinalTarget,
                          const gfx::IntPoint& aFilterSpaceToTargetOffset,
                          const gfx::IntRect& aPreFilterBounds,
                          const gfx::IntRect& aPostFilterBounds,
                          gfx::CompositionOp aCompositionOp)
    : mCtx(nullptr)
    , mCompositionOp(aCompositionOp)
  {
    mCtx = aCtx;
    mFinalTarget = aFinalTarget;
    mPostFilterBounds = aPostFilterBounds;
    mOffset = aFilterSpaceToTargetOffset;

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

    mTarget =
      mFinalTarget->CreateSimilarDrawTarget(mSourceGraphicRect.Size(), SurfaceFormat::B8G8R8A8);

    if (!mTarget) {
      // XXX - Deal with the situation where our temp size is too big to
      // fit in a texture (bug 1066622).
      mTarget = mFinalTarget;
      mCtx = nullptr;
      mFinalTarget = nullptr;
      return;
    }

    mTarget->SetTransform(
      mFinalTarget->GetTransform().PostTranslate(-mSourceGraphicRect.TopLeft() + mOffset));
  }

  // Return a SourceSurface that contains the FillPaint or StrokePaint source.
  already_AddRefed<SourceSurface>
  DoSourcePaint(gfx::IntRect& aRect, CanvasRenderingContext2D::Style aStyle)
  {
    if (aRect.IsEmpty()) {
      return nullptr;
    }

    RefPtr<DrawTarget> dt =
      mFinalTarget->CreateSimilarDrawTarget(aRect.Size(), SurfaceFormat::B8G8R8A8);
    if (!dt) {
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

  ~AdjustedTargetForFilter()
  {
    if (!mCtx) {
      return;
    }

    RefPtr<SourceSurface> snapshot = mTarget->Snapshot();

    RefPtr<SourceSurface> fillPaint =
      DoSourcePaint(mFillPaintRect, CanvasRenderingContext2D::Style::FILL);
    RefPtr<SourceSurface> strokePaint =
      DoSourcePaint(mStrokePaintRect, CanvasRenderingContext2D::Style::STROKE);

    AutoRestoreTransform autoRestoreTransform(mFinalTarget);
    mFinalTarget->SetTransform(Matrix());

    MOZ_RELEASE_ASSERT(!mCtx->CurrentState().filter.mPrimitives.IsEmpty());
    gfx::FilterSupport::RenderFilterDescription(
      mFinalTarget, mCtx->CurrentState().filter,
      gfx::Rect(mPostFilterBounds),
      snapshot, mSourceGraphicRect,
      fillPaint, mFillPaintRect,
      strokePaint, mStrokePaintRect,
      mCtx->CurrentState().filterAdditionalImages,
      mPostFilterBounds.TopLeft() - mOffset,
      DrawOptions(1.0f, mCompositionOp));

    const gfx::FilterDescription& filter = mCtx->CurrentState().filter;
    MOZ_RELEASE_ASSERT(!filter.mPrimitives.IsEmpty());
    if (filter.mPrimitives.LastElement().IsTainted() && mCtx->mCanvasElement) {
      mCtx->mCanvasElement->SetWriteOnly();
    }
  }

  DrawTarget* DT()
  {
    return mTarget;
  }

private:
  RefPtr<DrawTarget> mTarget;
  RefPtr<DrawTarget> mFinalTarget;
  CanvasRenderingContext2D *mCtx;
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
class AdjustedTargetForShadow
{
public:
  typedef CanvasRenderingContext2D::ContextState ContextState;

  AdjustedTargetForShadow(CanvasRenderingContext2D* aCtx,
                          DrawTarget* aFinalTarget,
                          const gfx::Rect& aBounds,
                          gfx::CompositionOp aCompositionOp)
    : mCtx(nullptr)
    , mCompositionOp(aCompositionOp)
  {
    mCtx = aCtx;
    mFinalTarget = aFinalTarget;

    const ContextState &state = mCtx->CurrentState();

    mSigma = state.ShadowBlurSigma();

    gfx::Rect bounds = aBounds;

    int32_t blurRadius = state.ShadowBlurRadius();

    // We actually include the bounds of the shadow blur, this makes it
    // easier to execute the actual blur on hardware, and shouldn't affect
    // the amount of pixels that need to be touched.
    bounds.Inflate(blurRadius);

    bounds.RoundOut();
    bounds.ToIntRect(&mTempRect);

    mTarget =
      mFinalTarget->CreateShadowDrawTarget(mTempRect.Size(),
                                           SurfaceFormat::B8G8R8A8, mSigma);

    if (!mTarget) {
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

  ~AdjustedTargetForShadow()
  {
    if (!mCtx) {
      return;
    }

    RefPtr<SourceSurface> snapshot = mTarget->Snapshot();

    mFinalTarget->DrawSurfaceWithShadow(snapshot, mTempRect.TopLeft(),
                                        Color::FromABGR(mCtx->CurrentState().shadowColor),
                                        mCtx->CurrentState().shadowOffset, mSigma,
                                        mCompositionOp);
  }

  DrawTarget* DT()
  {
    return mTarget;
  }

  gfx::IntPoint OffsetToFinalDT()
  {
    return mTempRect.TopLeft();
  }

private:
  RefPtr<DrawTarget> mTarget;
  RefPtr<DrawTarget> mFinalTarget;
  CanvasRenderingContext2D *mCtx;
  Float mSigma;
  gfx::IntRect mTempRect;
  gfx::CompositionOp mCompositionOp;
};

/* This is an RAII based class that can be used as a drawtarget for
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
class AdjustedTarget
{
public:
  typedef CanvasRenderingContext2D::ContextState ContextState;

  explicit AdjustedTarget(CanvasRenderingContext2D* aCtx,
                          const gfx::Rect *aBounds = nullptr)
  {
    mTarget = aCtx->mTarget;

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

    gfx::Rect bounds = maxSourceNeededBoundsForFilter;
    if (aBounds) {
      bounds = bounds.Intersect(*aBounds);
    }
    gfx::Rect boundsAfterFilter = BoundsAfterFilter(bounds, aCtx);

    mozilla::gfx::CompositionOp op = aCtx->CurrentState().op;

    gfx::IntPoint offsetToFinalDT;

    // First set up the shadow draw target, because the shadow goes outside.
    // It applies to the post-filter results, if both a filter and a shadow
    // are used.
    if (aCtx->NeedToDrawShadow()) {
      mShadowTarget = MakeUnique<AdjustedTargetForShadow>(
        aCtx, mTarget, boundsAfterFilter, op);
      mTarget = mShadowTarget->DT();
      offsetToFinalDT = mShadowTarget->OffsetToFinalDT();

      // If we also have a filter, the filter needs to be drawn with OP_OVER
      // because shadow drawing already applies op on the result.
      op = gfx::CompositionOp::OP_OVER;
    }

    // Now set up the filter draw target.
    if (aCtx->NeedToApplyFilter()) {
      bounds.RoundOut();

      gfx::IntRect intBounds;
      if (!bounds.ToIntRect(&intBounds)) {
        return;
      }
      mFilterTarget = MakeUnique<AdjustedTargetForFilter>(
        aCtx, mTarget, offsetToFinalDT, intBounds,
        gfx::RoundedToInt(boundsAfterFilter), op);
      mTarget = mFilterTarget->DT();
    }
  }

  ~AdjustedTarget()
  {
    // The order in which the targets are finalized is important.
    // Filters are inside, any shadow applies to the post-filter results.
    mFilterTarget.reset();
    mShadowTarget.reset();
  }

  operator DrawTarget*()
  {
    return mTarget;
  }

  DrawTarget* operator->() MOZ_NO_ADDREF_RELEASE_ON_RETURN
  {
    return mTarget;
  }

private:

  gfx::Rect
  MaxSourceNeededBoundsForFilter(const gfx::Rect& aDestBounds, CanvasRenderingContext2D* aCtx)
  {
    if (!aCtx->NeedToApplyFilter()) {
      return aDestBounds;
    }

    nsIntRegion sourceGraphicNeededRegion;
    nsIntRegion fillPaintNeededRegion;
    nsIntRegion strokePaintNeededRegion;

    FilterSupport::ComputeSourceNeededRegions(
      aCtx->CurrentState().filter, gfx::RoundedToInt(aDestBounds),
      sourceGraphicNeededRegion, fillPaintNeededRegion, strokePaintNeededRegion);

    return gfx::Rect(sourceGraphicNeededRegion.GetBounds());
  }

  gfx::Rect
  MaxSourceNeededBoundsForShadow(const gfx::Rect& aDestBounds, CanvasRenderingContext2D* aCtx)
  {
    if (!aCtx->NeedToDrawShadow()) {
      return aDestBounds;
    }

    const ContextState &state = aCtx->CurrentState();
    gfx::Rect sourceBounds = aDestBounds - state.shadowOffset;
    sourceBounds.Inflate(state.ShadowBlurRadius());

    // Union the shadow source with the original rect because we're going to
    // draw both.
    return sourceBounds.Union(aDestBounds);
  }

  gfx::Rect
  BoundsAfterFilter(const gfx::Rect& aBounds, CanvasRenderingContext2D* aCtx)
  {
    if (!aCtx->NeedToApplyFilter()) {
      return aBounds;
    }

    gfx::Rect bounds(aBounds);
    bounds.RoundOut();

    gfx::IntRect intBounds;
    if (!bounds.ToIntRect(&intBounds)) {
      return gfx::Rect();
    }

    nsIntRegion extents =
      gfx::FilterSupport::ComputePostFilterExtents(aCtx->CurrentState().filter,
                                                   intBounds);
    return gfx::Rect(extents.GetBounds());
  }

  RefPtr<DrawTarget> mTarget;
  UniquePtr<AdjustedTargetForShadow> mShadowTarget;
  UniquePtr<AdjustedTargetForFilter> mFilterTarget;
};

void
CanvasPattern::SetTransform(SVGMatrix& aMatrix)
{
  mTransform = ToMatrix(aMatrix.GetMatrix());
}

void
CanvasGradient::AddColorStop(float aOffset, const nsAString& aColorstr, ErrorResult& aRv)
{
  if (aOffset < 0.0 || aOffset > 1.0) {
    aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return;
  }

  nsCSSValue value;
  nsCSSParser parser;
  if (!parser.ParseColorString(aColorstr, nullptr, 0, value)) {
    aRv.Throw(NS_ERROR_DOM_SYNTAX_ERR);
    return;
  }

  nscolor color;
  nsCOMPtr<nsIPresShell> presShell = mContext ? mContext->GetPresShell() : nullptr;
  if (!nsRuleNode::ComputeColor(value, presShell ? presShell->GetPresContext() : nullptr,
                                nullptr, color)) {
    aRv.Throw(NS_ERROR_DOM_SYNTAX_ERR);
    return;
  }

  mStops = nullptr;

  GradientStop newStop;

  newStop.offset = aOffset;
  newStop.color = Color::FromABGR(color);

  mRawStops.AppendElement(newStop);
}

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(CanvasGradient, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(CanvasGradient, Release)

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(CanvasGradient, mContext)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(CanvasPattern, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(CanvasPattern, Release)

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(CanvasPattern, mContext)

class CanvasShutdownObserver final : public nsIObserver
{
public:
  explicit CanvasShutdownObserver(CanvasRenderingContext2D* aCanvas)
  : mCanvas(aCanvas)
  {}

  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER
private:
  ~CanvasShutdownObserver() {}

  CanvasRenderingContext2D* mCanvas;
};

NS_IMPL_ISUPPORTS(CanvasShutdownObserver, nsIObserver)

NS_IMETHODIMP
CanvasShutdownObserver::Observe(nsISupports *aSubject,
                                const char *aTopic,
                                const char16_t *aData)
{
  if (mCanvas && strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID) == 0) {
    mCanvas->OnShutdown();
    nsContentUtils::UnregisterShutdownObserver(this);
  }

  return NS_OK;
}

class CanvasDrawObserver
{
public:
  explicit CanvasDrawObserver(CanvasRenderingContext2D* aCanvasContext);

  // Only enumerate draw calls that could affect the heuristic
  enum DrawCallType {
    PutImageData,
    GetImageData,
    DrawImage
  };

  // This is the one that we call on relevant draw calls and count
  // GPU vs. CPU preferrable calls...
  void DidDrawCall(DrawCallType aType);

  // When this returns true, the observer is done making the decisions.
  // Right now, we expect to get rid of the observer after the FrameEnd
  // returns true, though the decision could eventually change if the
  // function calls shift.  If we change to monitor the functions called
  // and make decisions to change more than once, we would probably want
  // FrameEnd to reset the timer and counters as it returns true.
  bool FrameEnd();

private:
  // These values will be picked up from preferences:
  int32_t mMinFramesBeforeDecision;
  float mMinSecondsBeforeDecision;
  int32_t mMinCallsBeforeDecision;

  CanvasRenderingContext2D* mCanvasContext;
  int32_t mSoftwarePreferredCalls;
  int32_t mGPUPreferredCalls;
  int32_t mFramesRendered;
  TimeStamp mCreationTime;
};

// We are not checking for the validity of the preference values.  For example,
// negative values will have an effect of a quick exit, so no harm done.
CanvasDrawObserver::CanvasDrawObserver(CanvasRenderingContext2D* aCanvasContext)
 : mMinFramesBeforeDecision(gfxPrefs::CanvasAutoAccelerateMinFrames())
 , mMinSecondsBeforeDecision(gfxPrefs::CanvasAutoAccelerateMinSeconds())
 , mMinCallsBeforeDecision(gfxPrefs::CanvasAutoAccelerateMinCalls())
 , mCanvasContext(aCanvasContext)
 , mSoftwarePreferredCalls(0)
 , mGPUPreferredCalls(0)
 , mFramesRendered(0)
 , mCreationTime(TimeStamp::NowLoRes())
{}

void
CanvasDrawObserver::DidDrawCall(DrawCallType aType)
{
  switch (aType) {
    case PutImageData:
    case GetImageData:
      if (mGPUPreferredCalls == 0 && mSoftwarePreferredCalls == 0) {
        mCreationTime = TimeStamp::NowLoRes();
      }
      mSoftwarePreferredCalls++;
      break;
    case DrawImage:
      if (mGPUPreferredCalls == 0 && mSoftwarePreferredCalls == 0) {
        mCreationTime = TimeStamp::NowLoRes();
      }
      mGPUPreferredCalls++;
      break;
  }
}

// If we return true, the observer is done making the decisions...
bool
CanvasDrawObserver::FrameEnd()
{
  mFramesRendered++;

  // We log the first mMinFramesBeforeDecision frames of any
  // canvas object then make a call to determine whether it should
  // be GPU or CPU backed
  if ((mFramesRendered >= mMinFramesBeforeDecision) ||
      ((TimeStamp::NowLoRes() - mCreationTime).ToSeconds()) > mMinSecondsBeforeDecision) {

    // If we don't have enough data, don't bother changing...
    if (mGPUPreferredCalls > mMinCallsBeforeDecision ||
        mSoftwarePreferredCalls > mMinCallsBeforeDecision) {
      CanvasRenderingContext2D::RenderingMode switchToMode;
      if (mGPUPreferredCalls >= mSoftwarePreferredCalls) {
        switchToMode = CanvasRenderingContext2D::RenderingMode::OpenGLBackendMode;
      } else {
        switchToMode = CanvasRenderingContext2D::RenderingMode::SoftwareBackendMode;
      }
      if (switchToMode != mCanvasContext->mRenderingMode) {
        if (!mCanvasContext->SwitchRenderingMode(switchToMode)) {
          gfxDebug() << "Canvas acceleration failed mode switch to " << switchToMode;
        }
      }
    }

    // If we ever redesign this class to constantly monitor the functions
    // and keep making decisions, we would probably want to reset the counters
    // and the timers here...
    return true;
  }
  return false;
}

class CanvasRenderingContext2DUserData : public LayerUserData {
public:
  explicit CanvasRenderingContext2DUserData(CanvasRenderingContext2D* aContext)
    : mContext(aContext)
  {
    aContext->mUserDatas.AppendElement(this);
  }
  ~CanvasRenderingContext2DUserData()
  {
    if (mContext) {
      mContext->mUserDatas.RemoveElement(this);
    }
  }

  static void PreTransactionCallback(void* aData)
  {
    CanvasRenderingContext2DUserData* self =
      static_cast<CanvasRenderingContext2DUserData*>(aData);
    CanvasRenderingContext2D* context = self->mContext;
    if (!context || !context->mTarget)
      return;

    context->OnStableState();
  }

  static void DidTransactionCallback(void* aData)
  {
    CanvasRenderingContext2DUserData* self =
      static_cast<CanvasRenderingContext2DUserData*>(aData);
    if (self->mContext) {
      self->mContext->MarkContextClean();
      if (self->mContext->mDrawObserver) {
        if (self->mContext->mDrawObserver->FrameEnd()) {
          // Note that this call deletes and nulls out mDrawObserver:
          self->mContext->RemoveDrawObserver();
        }
      }
    }
  }
  bool IsForContext(CanvasRenderingContext2D *aContext)
  {
    return mContext == aContext;
  }
  void Forget()
  {
    mContext = nullptr;
  }

private:
  CanvasRenderingContext2D *mContext;
};

class CanvasFilterChainObserver : public nsSVGFilterChainObserver
{
public:
  CanvasFilterChainObserver(nsTArray<nsStyleFilter>& aFilters,
                            Element* aCanvasElement,
                            CanvasRenderingContext2D* aContext)
    : nsSVGFilterChainObserver(aFilters, aCanvasElement)
    , mContext(aContext)
  {
  }

  virtual void DoUpdate() override
  {
    if (!mContext) {
      MOZ_CRASH("GFX: This should never be called without a context");
    }
    // Refresh the cached FilterDescription in mContext->CurrentState().filter.
    // If this filter is not at the top of the state stack, we'll refresh the
    // wrong filter, but that's ok, because we'll refresh the right filter
    // when we pop the state stack in CanvasRenderingContext2D::Restore().
    mContext->UpdateFilter();
  }

  void DetachFromContext() { mContext = nullptr; }

private:
  CanvasRenderingContext2D *mContext;
};

NS_IMPL_CYCLE_COLLECTING_ADDREF(CanvasRenderingContext2D)
NS_IMPL_CYCLE_COLLECTING_RELEASE(CanvasRenderingContext2D)

NS_IMPL_CYCLE_COLLECTION_CLASS(CanvasRenderingContext2D)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(CanvasRenderingContext2D)
  // Make sure we remove ourselves from the list of demotable contexts (raw pointers),
  // since we're logically destructed at this point.
  CanvasRenderingContext2D::RemoveDemotableContext(tmp);
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mCanvasElement)
  for (uint32_t i = 0; i < tmp->mStyleStack.Length(); i++) {
    ImplCycleCollectionUnlink(tmp->mStyleStack[i].patternStyles[Style::STROKE]);
    ImplCycleCollectionUnlink(tmp->mStyleStack[i].patternStyles[Style::FILL]);
    ImplCycleCollectionUnlink(tmp->mStyleStack[i].gradientStyles[Style::STROKE]);
    ImplCycleCollectionUnlink(tmp->mStyleStack[i].gradientStyles[Style::FILL]);
    CanvasFilterChainObserver *filterChainObserver =
      static_cast<CanvasFilterChainObserver*>(tmp->mStyleStack[i].filterChainObserver.get());
    if (filterChainObserver) {
      filterChainObserver->DetachFromContext();
    }
    ImplCycleCollectionUnlink(tmp->mStyleStack[i].filterChainObserver);
  }
  for (size_t x = 0 ; x < tmp->mHitRegionsOptions.Length(); x++) {
    RegionInfo& info = tmp->mHitRegionsOptions[x];
    if (info.mElement) {
      ImplCycleCollectionUnlink(info.mElement);
    }
  }
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(CanvasRenderingContext2D)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mCanvasElement)
  for (uint32_t i = 0; i < tmp->mStyleStack.Length(); i++) {
    ImplCycleCollectionTraverse(cb, tmp->mStyleStack[i].patternStyles[Style::STROKE], "Stroke CanvasPattern");
    ImplCycleCollectionTraverse(cb, tmp->mStyleStack[i].patternStyles[Style::FILL], "Fill CanvasPattern");
    ImplCycleCollectionTraverse(cb, tmp->mStyleStack[i].gradientStyles[Style::STROKE], "Stroke CanvasGradient");
    ImplCycleCollectionTraverse(cb, tmp->mStyleStack[i].gradientStyles[Style::FILL], "Fill CanvasGradient");
    ImplCycleCollectionTraverse(cb, tmp->mStyleStack[i].filterChainObserver, "Filter Chain Observer");
  }
  for (size_t x = 0 ; x < tmp->mHitRegionsOptions.Length(); x++) {
    RegionInfo& info = tmp->mHitRegionsOptions[x];
    if (info.mElement) {
      ImplCycleCollectionTraverse(cb, info.mElement, "Hit region fallback element");
    }
  }
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_WRAPPERCACHE(CanvasRenderingContext2D)

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_BEGIN(CanvasRenderingContext2D)
 if (nsCCUncollectableMarker::sGeneration && tmp->IsBlack()) {
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
  return nsCCUncollectableMarker::sGeneration && tmp->IsBlack();
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_IN_CC_END

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_THIS_BEGIN(CanvasRenderingContext2D)
  return nsCCUncollectableMarker::sGeneration && tmp->IsBlack();
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_THIS_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(CanvasRenderingContext2D)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsICanvasRenderingContextInternal)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

/**
 ** CanvasRenderingContext2D impl
 **/


// Initialize our static variables.
uint32_t CanvasRenderingContext2D::sNumLivingContexts = 0;
DrawTarget* CanvasRenderingContext2D::sErrorTarget = nullptr;



CanvasRenderingContext2D::CanvasRenderingContext2D()
  : mRenderingMode(RenderingMode::OpenGLBackendMode)
  // these are the default values from the Canvas spec
  , mWidth(0), mHeight(0)
  , mZero(false), mOpaque(false)
  , mResetLayer(true)
  , mIPC(false)
  , mIsSkiaGL(false)
  , mHasPendingStableStateCallback(false)
  , mDrawObserver(nullptr)
  , mIsEntireFrameInvalid(false)
  , mPredictManyRedrawCalls(false)
  , mIsCapturedFrameInvalid(false)
  , mPathTransformWillUpdate(false)
  , mInvalidateCount(0)
{
  sNumLivingContexts++;

  mShutdownObserver = new CanvasShutdownObserver(this);
  nsContentUtils::RegisterShutdownObserver(mShutdownObserver);

  // The default is to use OpenGL mode
  if (gfxPlatform::GetPlatform()->UseAcceleratedCanvas()) {
    mDrawObserver = new CanvasDrawObserver(this);
  } else {
    mRenderingMode = RenderingMode::SoftwareBackendMode;
  }
}

CanvasRenderingContext2D::~CanvasRenderingContext2D()
{
  RemoveDrawObserver();
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
  RemoveDemotableContext(this);
}

JSObject*
CanvasRenderingContext2D::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return CanvasRenderingContext2DBinding::Wrap(aCx, this, aGivenProto);
}

bool
CanvasRenderingContext2D::ParseColor(const nsAString& aString,
                                     nscolor* aColor)
{
  nsIDocument* document = mCanvasElement
                          ? mCanvasElement->OwnerDoc()
                          : nullptr;

  // Pass the CSS Loader object to the parser, to allow parser error
  // reports to include the outer window ID.
  nsCSSParser parser(document ? document->CSSLoader() : nullptr);
  nsCSSValue value;
  if (!parser.ParseColorString(aString, nullptr, 0, value)) {
    return false;
  }

  if (value.IsNumericColorUnit()) {
    // if we already have a color we can just use it directly
    *aColor = value.GetColorValue();
  } else {
    // otherwise resolve it
    nsCOMPtr<nsIPresShell> presShell = GetPresShell();
    RefPtr<nsStyleContext> parentContext;
    if (mCanvasElement && mCanvasElement->IsInUncomposedDoc()) {
      // Inherit from the canvas element.
      parentContext = nsComputedDOMStyle::GetStyleContextForElement(
        mCanvasElement, nullptr, presShell);
    }

    Unused << nsRuleNode::ComputeColor(
      value, presShell ? presShell->GetPresContext() : nullptr, parentContext,
      *aColor);
  }
  return true;
}

nsresult
CanvasRenderingContext2D::Reset()
{
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

void
CanvasRenderingContext2D::OnShutdown()
{
  mShutdownObserver = nullptr;

  RefPtr<PersistentBufferProvider> provider = mBufferProvider;

  Reset();

  if (provider) {
    provider->OnShutdown();
  }
}

void
CanvasRenderingContext2D::RemoveShutdownObserver()
{
  if (mShutdownObserver) {
    nsContentUtils::UnregisterShutdownObserver(mShutdownObserver);
    mShutdownObserver = nullptr;
  }
}

void
CanvasRenderingContext2D::SetStyleFromString(const nsAString& aStr,
                                             Style aWhichStyle)
{
  MOZ_ASSERT(!aStr.IsVoid());

  nscolor color;
  if (!ParseColor(aStr, &color)) {
    return;
  }

  CurrentState().SetColorStyle(aWhichStyle, color);
}

void
CanvasRenderingContext2D::GetStyleAsUnion(OwningStringOrCanvasGradientOrCanvasPattern& aValue,
                                          Style aWhichStyle)
{
  const ContextState &state = CurrentState();
  if (state.patternStyles[aWhichStyle]) {
    aValue.SetAsCanvasPattern() = state.patternStyles[aWhichStyle];
  } else if (state.gradientStyles[aWhichStyle]) {
    aValue.SetAsCanvasGradient() = state.gradientStyles[aWhichStyle];
  } else {
    StyleColorToString(state.colorStyles[aWhichStyle], aValue.SetAsString());
  }
}

// static
void
CanvasRenderingContext2D::StyleColorToString(const nscolor& aColor, nsAString& aStr)
{
  // We can't reuse the normal CSS color stringification code,
  // because the spec calls for a different algorithm for canvas.
  if (NS_GET_A(aColor) == 255) {
    CopyUTF8toUTF16(nsPrintfCString("#%02x%02x%02x",
                                    NS_GET_R(aColor),
                                    NS_GET_G(aColor),
                                    NS_GET_B(aColor)),
                    aStr);
  } else {
    CopyUTF8toUTF16(nsPrintfCString("rgba(%d, %d, %d, ",
                                    NS_GET_R(aColor),
                                    NS_GET_G(aColor),
                                    NS_GET_B(aColor)),
                    aStr);
    aStr.AppendFloat(nsStyleUtil::ColorComponentToFloat(NS_GET_A(aColor)));
    aStr.Append(')');
  }
}

nsresult
CanvasRenderingContext2D::Redraw()
{
  mIsCapturedFrameInvalid = true;

  if (mIsEntireFrameInvalid) {
    return NS_OK;
  }

  mIsEntireFrameInvalid = true;

  if (!mCanvasElement) {
    NS_ASSERTION(mDocShell, "Redraw with no canvas element or docshell!");
    return NS_OK;
  }

  nsSVGEffects::InvalidateDirectRenderingObservers(mCanvasElement);

  mCanvasElement->InvalidateCanvasContent(nullptr);

  return NS_OK;
}

void
CanvasRenderingContext2D::Redraw(const gfx::Rect& aR)
{
  mIsCapturedFrameInvalid = true;

  ++mInvalidateCount;

  if (mIsEntireFrameInvalid) {
    return;
  }

  if (mPredictManyRedrawCalls ||
    mInvalidateCount > kCanvasMaxInvalidateCount) {
    Redraw();
    return;
  }

  if (!mCanvasElement) {
    NS_ASSERTION(mDocShell, "Redraw with no canvas element or docshell!");
    return;
  }

  nsSVGEffects::InvalidateDirectRenderingObservers(mCanvasElement);

  mCanvasElement->InvalidateCanvasContent(&aR);
}

void
CanvasRenderingContext2D::DidRefresh()
{
  if (IsTargetValid() && mIsSkiaGL) {
    SkiaGLGlue* glue = gfxPlatform::GetPlatform()->GetSkiaGLGlue();
    MOZ_ASSERT(glue);

    auto gl = glue->GetGLContext();
    gl->FlushIfHeavyGLCallsSinceLastFlush();
  }
}

void
CanvasRenderingContext2D::RedrawUser(const gfxRect& aR)
{
  mIsCapturedFrameInvalid = true;

  if (mIsEntireFrameInvalid) {
    ++mInvalidateCount;
    return;
  }

  gfx::Rect newr = mTarget->GetTransform().TransformBounds(ToRect(aR));
  Redraw(newr);
}

bool CanvasRenderingContext2D::SwitchRenderingMode(RenderingMode aRenderingMode)
{
  if (!IsTargetValid() || mRenderingMode == aRenderingMode) {
    return false;
  }

  MOZ_ASSERT(mBufferProvider);

#ifdef USE_SKIA_GPU
  // Do not attempt to switch into GL mode if the platform doesn't allow it.
  if ((aRenderingMode == RenderingMode::OpenGLBackendMode) &&
      !gfxPlatform::GetPlatform()->UseAcceleratedCanvas()) {
      return false;
  }
#endif

  RefPtr<PersistentBufferProvider> oldBufferProvider = mBufferProvider;

  // Return the old target to the buffer provider.
  // We need to do this before calling EnsureTarget.
  ReturnTarget();
  mTarget = nullptr;
  mBufferProvider = nullptr;
  mResetLayer = true;

  // Borrowing the snapshot must be done after ReturnTarget.
  RefPtr<SourceSurface> snapshot = oldBufferProvider->BorrowSnapshot();

  // Recreate mTarget using the new rendering mode
  RenderingMode attemptedMode = EnsureTarget(nullptr, aRenderingMode);
  if (!IsTargetValid()) {
    oldBufferProvider->ReturnSnapshot(snapshot.forget());
    return false;
  }

  // We succeeded, so update mRenderingMode to reflect reality
  mRenderingMode = attemptedMode;

  // Restore the content from the old DrawTarget
  // Clips and transform were already restored in EnsureTarget.
  mTarget->CopySurface(snapshot, IntRect(0, 0, mWidth, mHeight), IntPoint());
  oldBufferProvider->ReturnSnapshot(snapshot.forget());
  return true;
}

void CanvasRenderingContext2D::Demote()
{
  if (SwitchRenderingMode(RenderingMode::SoftwareBackendMode)) {
    RemoveDemotableContext(this);
  }
}

std::vector<CanvasRenderingContext2D*>&
CanvasRenderingContext2D::DemotableContexts()
{
  // This is a list of raw pointers to cycle-collected objects. We need to ensure
  // that we remove elements from it during UNLINK (which can happen considerably before
  // the actual destructor) since the object is logically destroyed at that point
  // and will be in an inconsistant state.
  static std::vector<CanvasRenderingContext2D*> contexts;
  return contexts;
}

void
CanvasRenderingContext2D::DemoteOldestContextIfNecessary()
{
  const size_t kMaxContexts = 64;

  std::vector<CanvasRenderingContext2D*>& contexts = DemotableContexts();
  if (contexts.size() < kMaxContexts)
    return;

  CanvasRenderingContext2D* oldest = contexts.front();
  if (oldest->SwitchRenderingMode(RenderingMode::SoftwareBackendMode)) {
    RemoveDemotableContext(oldest);
  }
}

void
CanvasRenderingContext2D::AddDemotableContext(CanvasRenderingContext2D* aContext)
{
  std::vector<CanvasRenderingContext2D*>::iterator iter = std::find(DemotableContexts().begin(), DemotableContexts().end(), aContext);
  if (iter != DemotableContexts().end())
    return;

  DemotableContexts().push_back(aContext);
}

void
CanvasRenderingContext2D::RemoveDemotableContext(CanvasRenderingContext2D* aContext)
{
  std::vector<CanvasRenderingContext2D*>::iterator iter = std::find(DemotableContexts().begin(), DemotableContexts().end(), aContext);
  if (iter != DemotableContexts().end())
    DemotableContexts().erase(iter);
}

bool
CanvasRenderingContext2D::CheckSizeForSkiaGL(IntSize aSize) {
  MOZ_ASSERT(NS_IsMainThread());

  int minsize = Preferences::GetInt("gfx.canvas.min-size-for-skia-gl", 128);
  if (aSize.width < minsize || aSize.height < minsize) {
    return false;
  }

  // Maximum pref allows 3 different options:
  //  0   means unlimited size
  //  > 0 means use value as an absolute threshold
  //  < 0 means use the number of screen pixels as a threshold
  int maxsize = Preferences::GetInt("gfx.canvas.max-size-for-skia-gl", 0);

  // unlimited max size
  if (!maxsize) {
    return true;
  }

  // absolute max size threshold
  if (maxsize > 0) {
    return aSize.width <= maxsize && aSize.height <= maxsize;
  }

  // Cache the number of pixels on the primary screen
  static int32_t gScreenPixels = -1;
  if (gScreenPixels < 0) {
    // Default to historical mobile screen size of 980x480, like FishIEtank.
    // In addition, allow skia use up to this size even if the screen is smaller.
    // A lot content expects this size to work well.
    // See Bug 999841
    if (gfxPlatform::GetPlatform()->HasEnoughTotalSystemMemoryForSkiaGL()) {
      gScreenPixels = 980 * 480;
    }

    nsCOMPtr<nsIScreenManager> screenManager =
      do_GetService("@mozilla.org/gfx/screenmanager;1");
    if (screenManager) {
      nsCOMPtr<nsIScreen> primaryScreen;
      screenManager->GetPrimaryScreen(getter_AddRefs(primaryScreen));
      if (primaryScreen) {
        int32_t x, y, width, height;
        primaryScreen->GetRect(&x, &y, &width, &height);

        gScreenPixels = std::max(gScreenPixels, width * height);
      }
    }
  }

  // Just always use a scale of 1.0. It can be changed if a lot of contents need it.
  static double gDefaultScale = 1.0;

  double scale = gDefaultScale > 0 ? gDefaultScale : 1.0;
  int32_t threshold = ceil(scale * scale * gScreenPixels);

  // screen size acts as max threshold
  return threshold < 0 || (aSize.width * aSize.height) <= threshold;
}

void
CanvasRenderingContext2D::ScheduleStableStateCallback()
{
  if (mHasPendingStableStateCallback) {
    return;
  }
  mHasPendingStableStateCallback = true;

  nsContentUtils::RunInStableState(
    NewRunnableMethod(this, &CanvasRenderingContext2D::OnStableState)
  );
}

void
CanvasRenderingContext2D::OnStableState()
{
  if (!mHasPendingStableStateCallback) {
    return;
  }

  ReturnTarget();

  mHasPendingStableStateCallback = false;
}

void
CanvasRenderingContext2D::RestoreClipsAndTransformToTarget()
{
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

  for (const auto& style : mStyleStack) {
    for (const auto& clipOrTransform : style.clipsAndTransforms) {
      if (clipOrTransform.IsClip()) {
        mTarget->PushClip(clipOrTransform.clip);
      } else {
        mTarget->SetTransform(clipOrTransform.transform);
      }
    }
  }
}

CanvasRenderingContext2D::RenderingMode
CanvasRenderingContext2D::EnsureTarget(const gfx::Rect* aCoveredRect,
                                       RenderingMode aRenderingMode)
{
  if (AlreadyShutDown()) {
    gfxCriticalError() << "Attempt to render into a Canvas2d after shutdown.";
    SetErrorState();
    return aRenderingMode;
  }

  // This would make no sense, so make sure we don't get ourselves in a mess
  MOZ_ASSERT(mRenderingMode != RenderingMode::DefaultBackendMode);

  RenderingMode mode = (aRenderingMode == RenderingMode::DefaultBackendMode) ? mRenderingMode : aRenderingMode;

  if (mTarget && mode == mRenderingMode) {
    return mRenderingMode;
  }

  // Check that the dimensions are sane
  if (mWidth > gfxPrefs::MaxCanvasSize() ||
      mHeight > gfxPrefs::MaxCanvasSize() ||
      mWidth < 0 || mHeight < 0) {

    SetErrorState();
    return aRenderingMode;
  }

  // If the next drawing command covers the entire canvas, we can skip copying
  // from the previous frame and/or clearing the canvas.
  gfx::Rect canvasRect(0, 0, mWidth, mHeight);
  bool canDiscardContent = aCoveredRect &&
    CurrentState().transform.TransformBounds(*aCoveredRect).Contains(canvasRect);

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

  IntRect persistedRect = canDiscardContent ? IntRect()
                                            : IntRect(0, 0, mWidth, mHeight);

  if (mBufferProvider && mode == mRenderingMode) {
    mTarget = mBufferProvider->BorrowDrawTarget(persistedRect);

    if (mTarget && !mBufferProvider->PreservesDrawingState()) {
      RestoreClipsAndTransformToTarget();
    }

    if (mTarget) {
      return mode;
    }
  }

  RefPtr<DrawTarget> newTarget;
  RefPtr<PersistentBufferProvider> newProvider;

  if (mode == RenderingMode::OpenGLBackendMode &&
      !TrySkiaGLTarget(newTarget, newProvider)) {
    // Fall back to software.
    mode = RenderingMode::SoftwareBackendMode;
  }

  if (mode == RenderingMode::SoftwareBackendMode &&
      !TrySharedTarget(newTarget, newProvider) &&
      !TryBasicTarget(newTarget, newProvider)) {
    SetErrorState();
    return mode;
  }

  MOZ_ASSERT(newTarget);
  MOZ_ASSERT(newProvider);

  mTarget = newTarget.forget();
  mBufferProvider = newProvider.forget();

  RegisterAllocation();

  // Skia expects the unused X channel to contains 0 even for opaque operations
  // so we can't skip clearing in that case, even if we are going to cover the
  // entire canvas in the next drawing operation.
  if (!canDiscardContent || mTarget->GetBackendType() == gfx::BackendType::SKIA) {
    mTarget->ClearRect(canvasRect);
  }

  RestoreClipsAndTransformToTarget();

  // Force a full layer transaction since we didn't have a layer before
  // and now we might need one.
  if (mCanvasElement) {
    mCanvasElement->InvalidateCanvas();
  }
  // Calling Redraw() tells our invalidation machinery that the entire
  // canvas is already invalid, which can speed up future drawing.
  Redraw();

  return mode;
}

void
CanvasRenderingContext2D::SetInitialState()
{
  // Set up the initial canvas defaults
  mPathBuilder = nullptr;
  mPath = nullptr;
  mDSPathBuilder = nullptr;

  mStyleStack.Clear();
  ContextState *state = mStyleStack.AppendElement();
  state->globalAlpha = 1.0;

  state->colorStyles[Style::FILL] = NS_RGB(0,0,0);
  state->colorStyles[Style::STROKE] = NS_RGB(0,0,0);
  state->shadowColor = NS_RGBA(0,0,0,0);
}

void
CanvasRenderingContext2D::SetErrorState()
{
  EnsureErrorTarget();

  if (mTarget && mTarget != sErrorTarget) {
    gCanvasAzureMemoryUsed -= mWidth * mHeight * 4;
  }

  mTarget = sErrorTarget;
  mBufferProvider = nullptr;

  // clear transforms, clips, etc.
  SetInitialState();
}

void
CanvasRenderingContext2D::RegisterAllocation()
{
  // XXX - It would make more sense to track the allocation in
  // PeristentBufferProvider, rather than here.
  static bool registered = false;
  // FIXME: Disable the reporter for now, see bug 1241865
  if (!registered && false) {
    registered = true;
    RegisterStrongMemoryReporter(new Canvas2dPixelsReporter());
  }

  gCanvasAzureMemoryUsed += mWidth * mHeight * 4;
  JSContext* context = nsContentUtils::GetCurrentJSContext();
  if (context) {
    JS_updateMallocCounter(context, mWidth * mHeight * 4);
  }
}

static already_AddRefed<LayerManager>
LayerManagerFromCanvasElement(nsINode* aCanvasElement)
{
  if (!aCanvasElement || !aCanvasElement->OwnerDoc()) {
    return nullptr;
  }

  return nsContentUtils::PersistentLayerManagerForDocument(aCanvasElement->OwnerDoc());
}

bool
CanvasRenderingContext2D::TrySkiaGLTarget(RefPtr<gfx::DrawTarget>& aOutDT,
                                          RefPtr<layers::PersistentBufferProvider>& aOutProvider)
{
  aOutDT = nullptr;
  aOutProvider = nullptr;


  mIsSkiaGL = false;

  IntSize size(mWidth, mHeight);
  if (!gfxPlatform::GetPlatform()->UseAcceleratedCanvas() ||
      !CheckSizeForSkiaGL(size)) {

    return false;
  }


  RefPtr<LayerManager> layerManager = LayerManagerFromCanvasElement(mCanvasElement);

  if (!layerManager) {
    return false;
  }

  DemoteOldestContextIfNecessary();
  mBufferProvider = nullptr;

#ifdef USE_SKIA_GPU
  SkiaGLGlue* glue = gfxPlatform::GetPlatform()->GetSkiaGLGlue();
  if (!glue || !glue->GetGrContext() || !glue->GetGLContext()) {
    return false;
  }

  SurfaceFormat format = GetSurfaceFormat();
  aOutDT = Factory::CreateDrawTargetSkiaWithGrContext(glue->GetGrContext(),
                                                      size, format);
  if (!aOutDT) {
    gfxCriticalNote << "Failed to create a SkiaGL DrawTarget, falling back to software\n";
    return false;
  }

  MOZ_ASSERT(aOutDT->GetType() == DrawTargetType::HARDWARE_RASTER);

  AddDemotableContext(this);
  aOutProvider = new PersistentBufferProviderBasic(aOutDT);
  mIsSkiaGL = true;
  // Drop a note in the debug builds if we ever use accelerated Skia canvas.
  gfxWarningOnce() << "Using SkiaGL canvas.";
#endif

  // could still be null if USE_SKIA_GPU is not #defined.
  return !!aOutDT;
}

bool
CanvasRenderingContext2D::TrySharedTarget(RefPtr<gfx::DrawTarget>& aOutDT,
                                          RefPtr<layers::PersistentBufferProvider>& aOutProvider)
{
  aOutDT = nullptr;
  aOutProvider = nullptr;

  if (!mCanvasElement || !mCanvasElement->OwnerDoc()) {
    return false;
  }

  RefPtr<LayerManager> layerManager = LayerManagerFromCanvasElement(mCanvasElement);

  if (!layerManager) {
    return false;
  }

  aOutProvider = layerManager->CreatePersistentBufferProvider(GetSize(), GetSurfaceFormat());

  if (!aOutProvider) {
    return false;
  }

  // We can pass an empty persisted rect since we just created the buffer
  // provider (nothing to restore).
  aOutDT = aOutProvider->BorrowDrawTarget(IntRect());
  MOZ_ASSERT(aOutDT);

  return !!aOutDT;
}

bool
CanvasRenderingContext2D::TryBasicTarget(RefPtr<gfx::DrawTarget>& aOutDT,
                                         RefPtr<layers::PersistentBufferProvider>& aOutProvider)
{
  aOutDT = gfxPlatform::GetPlatform()->CreateOffscreenCanvasDrawTarget(GetSize(),
                                                                       GetSurfaceFormat());
  if (!aOutDT) {
    return false;
  }

  aOutProvider = new PersistentBufferProviderBasic(aOutDT);
  return true;
}

int32_t
CanvasRenderingContext2D::GetWidth() const
{
  return mWidth;
}

int32_t
CanvasRenderingContext2D::GetHeight() const
{
  return mHeight;
}

NS_IMETHODIMP
CanvasRenderingContext2D::SetDimensions(int32_t aWidth, int32_t aHeight)
{
  bool retainBuffer = false;
  if (aWidth == mWidth && aHeight == mHeight) {
    retainBuffer = true;
  }
  ClearTarget(retainBuffer);

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
  mWidth = aWidth;
  mHeight = aHeight;

  return NS_OK;
}

void
CanvasRenderingContext2D::ClearTarget(bool aRetainBuffer)
{
  RefPtr<PersistentBufferProvider> provider = mBufferProvider;
  if (aRetainBuffer && provider) {
    // We should reset the buffer data before reusing the buffer.
    if (mTarget) {
      mTarget->SetTransform(Matrix());
    }
    ClearRect(0, 0, mWidth, mHeight);
  }

  Reset();

  if (aRetainBuffer) {
    mBufferProvider = provider;
  }

  mResetLayer = true;

  SetInitialState();

  // For vertical writing-mode, unless text-orientation is sideways,
  // we'll modify the initial value of textBaseline to 'middle'.
  RefPtr<nsStyleContext> canvasStyle;
  if (mCanvasElement && mCanvasElement->IsInUncomposedDoc()) {
    nsCOMPtr<nsIPresShell> presShell = GetPresShell();
    if (presShell) {
      canvasStyle =
        nsComputedDOMStyle::GetStyleContextForElement(mCanvasElement,
                                                      nullptr,
                                                      presShell);
      if (canvasStyle) {
        WritingMode wm(canvasStyle);
        if (wm.IsVertical() && !wm.IsSideways()) {
          CurrentState().textBaseline = TextBaseline::MIDDLE;
        }
      }
    }
  }
}

void
CanvasRenderingContext2D::ReturnTarget(bool aForceReset)
{
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
        // balance out here. See the comment in RestoreClipsAndTransformToTarget.
        mTarget->PopClip();
      }

      mTarget->SetTransform(Matrix());
    }

    mBufferProvider->ReturnDrawTarget(mTarget.forget());
  }
}

NS_IMETHODIMP
CanvasRenderingContext2D::InitializeWithDrawTarget(nsIDocShell* aShell,
                                                   NotNull<gfx::DrawTarget*> aTarget)
{
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

NS_IMETHODIMP
CanvasRenderingContext2D::SetIsOpaque(bool aIsOpaque)
{
  if (aIsOpaque != mOpaque) {
    mOpaque = aIsOpaque;
    ClearTarget();
  }

  return NS_OK;
}

NS_IMETHODIMP
CanvasRenderingContext2D::SetIsIPC(bool aIsIPC)
{
  if (aIsIPC != mIPC) {
    mIPC = aIsIPC;
    ClearTarget();
  }

  return NS_OK;
}

NS_IMETHODIMP
CanvasRenderingContext2D::SetContextOptions(JSContext* aCx,
                                            JS::Handle<JS::Value> aOptions,
                                            ErrorResult& aRvForDictionaryInit)
{
  if (aOptions.isNullOrUndefined()) {
    return NS_OK;
  }

  // This shouldn't be called before drawing starts, so there should be no drawtarget yet
  MOZ_ASSERT(!mTarget);

  ContextAttributes2D attributes;
  if (!attributes.Init(aCx, aOptions)) {
    aRvForDictionaryInit.Throw(NS_ERROR_UNEXPECTED);
    return NS_ERROR_UNEXPECTED;
  }

  if (Preferences::GetBool("gfx.canvas.willReadFrequently.enable", false)) {
    // Use software when there is going to be a lot of readback
    if (attributes.mWillReadFrequently) {

      // We want to lock into software, so remove the observer that
      // may potentially change that...
      RemoveDrawObserver();
      mRenderingMode = RenderingMode::SoftwareBackendMode;
    }
  }

  if (!attributes.mAlpha) {
    SetIsOpaque(true);
  }

  return NS_OK;
}

UniquePtr<uint8_t[]>
CanvasRenderingContext2D::GetImageBuffer(int32_t* aFormat)
{
  UniquePtr<uint8_t[]> ret;

  *aFormat = 0;

  RefPtr<SourceSurface> snapshot;
  if (mTarget) {
    snapshot = mTarget->Snapshot();
  } else if (mBufferProvider) {
    snapshot = mBufferProvider->BorrowSnapshot();
  } else {
    EnsureTarget();
    snapshot = mTarget->Snapshot();
  }

  if (snapshot) {
    RefPtr<DataSourceSurface> data = snapshot->GetDataSurface();
    if (data && data->GetSize() == GetSize()) {
      *aFormat = imgIEncoder::INPUT_FORMAT_HOSTARGB;
      ret = SurfaceToPackedBGRA(data);
    }
  }

  if (!mTarget && mBufferProvider) {
    mBufferProvider->ReturnSnapshot(snapshot.forget());
  }

  return ret;
}

nsString CanvasRenderingContext2D::GetHitRegion(const mozilla::gfx::Point& aPoint)
{
  for (size_t x = 0 ; x < mHitRegionsOptions.Length(); x++) {
    RegionInfo& info = mHitRegionsOptions[x];
    if (info.mPath->ContainsPoint(aPoint, Matrix())) {
      return info.mId;
    }
  }
  return nsString();
}

NS_IMETHODIMP
CanvasRenderingContext2D::GetInputStream(const char* aMimeType,
                                         const char16_t* aEncoderOptions,
                                         nsIInputStream** aStream)
{
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

SurfaceFormat
CanvasRenderingContext2D::GetSurfaceFormat() const
{
  return mOpaque ? SurfaceFormat::B8G8R8X8 : SurfaceFormat::B8G8R8A8;
}

//
// state
//

void
CanvasRenderingContext2D::Save()
{
  EnsureTarget();
  mStyleStack[mStyleStack.Length() - 1].transform = mTarget->GetTransform();
  mStyleStack.SetCapacity(mStyleStack.Length() + 1);
  mStyleStack.AppendElement(CurrentState());

  if (mStyleStack.Length() > MAX_STYLE_STACK_SIZE) {
    // This is not fast, but is better than OOMing and shouldn't be hit by
    // reasonable code.
    mStyleStack.RemoveElementAt(0);
  }
}

void
CanvasRenderingContext2D::Restore()
{
  if (mStyleStack.Length() - 1 == 0)
    return;

  TransformWillUpdate();

  for (const auto& clipOrTransform : CurrentState().clipsAndTransforms) {
    if (clipOrTransform.IsClip()) {
      mTarget->PopClip();
    }
  }

  mStyleStack.RemoveElementAt(mStyleStack.Length() - 1);

  mTarget->SetTransform(CurrentState().transform);
}

//
// transformations
//

void
CanvasRenderingContext2D::Scale(double aX, double aY, ErrorResult& aError)
{
  TransformWillUpdate();
  if (!IsTargetValid()) {
    aError.Throw(NS_ERROR_FAILURE);
    return;
  }

  Matrix newMatrix = mTarget->GetTransform();
  newMatrix.PreScale(aX, aY);

  SetTransformInternal(newMatrix);
}

void
CanvasRenderingContext2D::Rotate(double aAngle, ErrorResult& aError)
{
  TransformWillUpdate();
  if (!IsTargetValid()) {
    aError.Throw(NS_ERROR_FAILURE);
    return;
  }

  Matrix newMatrix = Matrix::Rotation(aAngle) * mTarget->GetTransform();

  SetTransformInternal(newMatrix);
}

void
CanvasRenderingContext2D::Translate(double aX, double aY, ErrorResult& aError)
{
  TransformWillUpdate();
  if (!IsTargetValid()) {
    aError.Throw(NS_ERROR_FAILURE);
    return;
  }

  Matrix newMatrix = mTarget->GetTransform();
  newMatrix.PreTranslate(aX, aY);

  SetTransformInternal(newMatrix);
}

void
CanvasRenderingContext2D::Transform(double aM11, double aM12, double aM21,
                                    double aM22, double aDx, double aDy,
                                    ErrorResult& aError)
{
  TransformWillUpdate();
  if (!IsTargetValid()) {
    aError.Throw(NS_ERROR_FAILURE);
    return;
  }

  Matrix newMatrix(aM11, aM12, aM21, aM22, aDx, aDy);
  newMatrix *= mTarget->GetTransform();

  SetTransformInternal(newMatrix);
}

void
CanvasRenderingContext2D::SetTransform(double aM11, double aM12,
                                       double aM21, double aM22,
                                       double aDx, double aDy,
                                       ErrorResult& aError)
{
  TransformWillUpdate();
  if (!IsTargetValid()) {
    aError.Throw(NS_ERROR_FAILURE);
    return;
  }

  SetTransformInternal(Matrix(aM11, aM12, aM21, aM22, aDx, aDy));
}

void
CanvasRenderingContext2D::SetTransformInternal(const Matrix& aTransform)
{
  if (!aTransform.IsFinite()) {
    return;
  }

  // Save the transform in the clip stack to be able to replay clips properly.
  auto& clipsAndTransforms = CurrentState().clipsAndTransforms;
  if (clipsAndTransforms.IsEmpty() || clipsAndTransforms.LastElement().IsClip()) {
    clipsAndTransforms.AppendElement(ClipState(aTransform));
  } else {
    // If the last item is a transform we can replace it instead of appending
    // a new item.
    clipsAndTransforms.LastElement().transform = aTransform;
  }
  mTarget->SetTransform(aTransform);
}

void
CanvasRenderingContext2D::ResetTransform(ErrorResult& aError)
{
  SetTransform(1.0, 0.0, 0.0, 1.0, 0.0, 0.0, aError);
}

static void
MatrixToJSObject(JSContext* aCx, const Matrix& aMatrix,
                 JS::MutableHandle<JSObject*> aResult, ErrorResult& aError)
{
  double elts[6] = { aMatrix._11, aMatrix._12,
                     aMatrix._21, aMatrix._22,
                     aMatrix._31, aMatrix._32 };

  // XXX Should we enter GetWrapper()'s compartment?
  JS::Rooted<JS::Value> val(aCx);
  if (!ToJSValue(aCx, elts, &val)) {
    aError.Throw(NS_ERROR_OUT_OF_MEMORY);
  } else {
    aResult.set(&val.toObject());
  }
}

static bool
ObjectToMatrix(JSContext* aCx, JS::Handle<JSObject*> aObj, Matrix& aMatrix,
               ErrorResult& aError)
{
  uint32_t length;
  if (!JS_GetArrayLength(aCx, aObj, &length) || length != 6) {
    // Not an array-like thing or wrong size
    aError.Throw(NS_ERROR_INVALID_ARG);
    return false;
  }

  Float* elts[] = { &aMatrix._11, &aMatrix._12, &aMatrix._21, &aMatrix._22,
                    &aMatrix._31, &aMatrix._32 };
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

void
CanvasRenderingContext2D::SetMozCurrentTransform(JSContext* aCx,
                                                 JS::Handle<JSObject*> aCurrentTransform,
                                                 ErrorResult& aError)
{
  EnsureTarget();
  if (!IsTargetValid()) {
    aError.Throw(NS_ERROR_FAILURE);
    return;
  }

  Matrix newCTM;
  if (ObjectToMatrix(aCx, aCurrentTransform, newCTM, aError) && newCTM.IsFinite()) {
    mTarget->SetTransform(newCTM);
  }
}

void
CanvasRenderingContext2D::GetMozCurrentTransform(JSContext* aCx,
                                                 JS::MutableHandle<JSObject*> aResult,
                                                 ErrorResult& aError) const
{
  MatrixToJSObject(aCx, mTarget ? mTarget->GetTransform() : Matrix(),
                   aResult, aError);
}

void
CanvasRenderingContext2D::SetMozCurrentTransformInverse(JSContext* aCx,
                                                        JS::Handle<JSObject*> aCurrentTransform,
                                                        ErrorResult& aError)
{
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

void
CanvasRenderingContext2D::GetMozCurrentTransformInverse(JSContext* aCx,
                                                        JS::MutableHandle<JSObject*> aResult,
                                                        ErrorResult& aError) const
{
  if (!mTarget) {
    MatrixToJSObject(aCx, Matrix(), aResult, aError);
    return;
  }

  Matrix ctm = mTarget->GetTransform();

  if (!ctm.Invert()) {
    double NaN = JS_GetNaNValue(aCx).toDouble();
    ctm = Matrix(NaN, NaN, NaN, NaN, NaN, NaN);
  }

  MatrixToJSObject(aCx, ctm, aResult, aError);
}

//
// colors
//

void
CanvasRenderingContext2D::SetStyleFromUnion(const StringOrCanvasGradientOrCanvasPattern& aValue,
                                            Style aWhichStyle)
{
  if (aValue.IsString()) {
    SetStyleFromString(aValue.GetAsString(), aWhichStyle);
    return;
  }

  if (aValue.IsCanvasGradient()) {
    SetStyleFromGradient(aValue.GetAsCanvasGradient(), aWhichStyle);
    return;
  }

  if (aValue.IsCanvasPattern()) {
    SetStyleFromPattern(aValue.GetAsCanvasPattern(), aWhichStyle);
    return;
  }

  MOZ_ASSERT_UNREACHABLE("Invalid union value");
}

void
CanvasRenderingContext2D::SetFillRule(const nsAString& aString)
{
  FillRule rule;

  if (aString.EqualsLiteral("evenodd"))
    rule = FillRule::FILL_EVEN_ODD;
  else if (aString.EqualsLiteral("nonzero"))
    rule = FillRule::FILL_WINDING;
  else
    return;

  CurrentState().fillRule = rule;
}

void
CanvasRenderingContext2D::GetFillRule(nsAString& aString)
{
  switch (CurrentState().fillRule) {
  case FillRule::FILL_WINDING:
    aString.AssignLiteral("nonzero"); break;
  case FillRule::FILL_EVEN_ODD:
    aString.AssignLiteral("evenodd"); break;
  }
}
//
// gradients and patterns
//
already_AddRefed<CanvasGradient>
CanvasRenderingContext2D::CreateLinearGradient(double aX0, double aY0, double aX1, double aY1)
{
  RefPtr<CanvasGradient> grad =
    new CanvasLinearGradient(this, Point(aX0, aY0), Point(aX1, aY1));

  return grad.forget();
}

already_AddRefed<CanvasGradient>
CanvasRenderingContext2D::CreateRadialGradient(double aX0, double aY0, double aR0,
                                               double aX1, double aY1, double aR1,
                                               ErrorResult& aError)
{
  if (aR0 < 0.0 || aR1 < 0.0) {
    aError.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return nullptr;
  }

  RefPtr<CanvasGradient> grad =
    new CanvasRadialGradient(this, Point(aX0, aY0), aR0, Point(aX1, aY1), aR1);

  return grad.forget();
}

already_AddRefed<CanvasPattern>
CanvasRenderingContext2D::CreatePattern(const CanvasImageSource& aSource,
                                        const nsAString& aRepeat,
                                        ErrorResult& aError)
{
  CanvasPattern::RepeatMode repeatMode =
    CanvasPattern::RepeatMode::NOREPEAT;

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

  Element* htmlElement;
  if (aSource.IsHTMLCanvasElement()) {
    HTMLCanvasElement* canvas = &aSource.GetAsHTMLCanvasElement();
    htmlElement = canvas;

    nsIntSize size = canvas->GetSize();
    if (size.width == 0 || size.height == 0) {
      aError.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
      return nullptr;
    }

    // Special case for Canvas, which could be an Azure canvas!
    nsICanvasRenderingContextInternal *srcCanvas = canvas->GetContextAtIndex(0);
    if (srcCanvas) {
      // This might not be an Azure canvas!
      RefPtr<SourceSurface> srcSurf = srcCanvas->GetSurfaceSnapshot();
      if (!srcSurf) {
        JSContext* context = nsContentUtils::GetCurrentJSContext();
        if (context) {
          JS_ReportWarning(context, "CanvasRenderingContext2D.createPattern() failed to snapshot source canvas.");
        }
        aError.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
        return nullptr;
      }

      RefPtr<CanvasPattern> pat =
        new CanvasPattern(this, srcSurf, repeatMode, htmlElement->NodePrincipal(), canvas->IsWriteOnly(), false);

      return pat.forget();
    }
  } else if (aSource.IsHTMLImageElement()) {
    HTMLImageElement* img = &aSource.GetAsHTMLImageElement();
    if (img->IntrinsicState().HasState(NS_EVENT_STATE_BROKEN)) {
      aError.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
      return nullptr;
    }

    htmlElement = img;
  } else if (aSource.IsHTMLVideoElement()) {
    auto& video = aSource.GetAsHTMLVideoElement();
    video.MarkAsContentSource(mozilla::dom::HTMLVideoElement::CallerAPI::CREATE_PATTERN);
    htmlElement = &video;
  } else {
    // Special case for ImageBitmap
    ImageBitmap& imgBitmap = aSource.GetAsImageBitmap();
    EnsureTarget();
    RefPtr<SourceSurface> srcSurf = imgBitmap.PrepareForDrawTarget(mTarget);
    if (!srcSurf) {
      JSContext* context = nsContentUtils::GetCurrentJSContext();
      if (context) {
        JS_ReportWarning(context, "CanvasRenderingContext2D.createPattern() failed to prepare source ImageBitmap.");
      }
      aError.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
      return nullptr;
    }

    // An ImageBitmap never taints others so we set principalForSecurityCheck to
    // nullptr and set CORSUsed to true for passing the security check in
    // CanvasUtils::DoDrawImageSecurityCheck().
    RefPtr<CanvasPattern> pat =
      new CanvasPattern(this, srcSurf, repeatMode, nullptr, false, true);

    return pat.forget();
  }

  EnsureTarget();

  // The canvas spec says that createPattern should use the first frame
  // of animated images
  nsLayoutUtils::SurfaceFromElementResult res =
    nsLayoutUtils::SurfaceFromElement(htmlElement,
      nsLayoutUtils::SFE_WANT_FIRST_FRAME, mTarget);

  if (!res.GetSourceSurface()) {
    return nullptr;
  }

  RefPtr<CanvasPattern> pat = new CanvasPattern(this, res.GetSourceSurface(), repeatMode,
                                                res.mPrincipal, res.mIsWriteOnly,
                                                res.mCORSUsed);
  return pat.forget();
}

//
// shadows
//
void
CanvasRenderingContext2D::SetShadowColor(const nsAString& aShadowColor)
{
  nscolor color;
  if (!ParseColor(aShadowColor, &color)) {
    return;
  }

  CurrentState().shadowColor = color;
}

//
// filters
//

static already_AddRefed<Declaration>
CreateDeclaration(nsINode* aNode,
  const nsCSSPropertyID aProp1, const nsAString& aValue1, bool* aChanged1,
  const nsCSSPropertyID aProp2, const nsAString& aValue2, bool* aChanged2)
{
  nsIPrincipal* principal = aNode->NodePrincipal();
  nsIDocument* document = aNode->OwnerDoc();

  nsIURI* docURL = document->GetDocumentURI();
  nsIURI* baseURL = document->GetDocBaseURI();

  // Pass the CSS Loader object to the parser, to allow parser error reports
  // to include the outer window ID.
  nsCSSParser parser(document->CSSLoader());

  RefPtr<Declaration> declaration =
    parser.ParseStyleAttribute(EmptyString(), docURL, baseURL, principal);

  if (aProp1 != eCSSProperty_UNKNOWN) {
    parser.ParseProperty(aProp1, aValue1, docURL, baseURL, principal,
                         declaration, aChanged1, false);
  }

  if (aProp2 != eCSSProperty_UNKNOWN) {
    parser.ParseProperty(aProp2, aValue2, docURL, baseURL, principal,
                         declaration, aChanged2, false);
  }

  declaration->SetImmutable();
  return declaration.forget();
}

static already_AddRefed<Declaration>
CreateFontDeclaration(const nsAString& aFont,
                      nsINode* aNode,
                      bool* aOutFontChanged)
{
  bool lineHeightChanged;
  return CreateDeclaration(aNode,
    eCSSProperty_font, aFont, aOutFontChanged,
    eCSSProperty_line_height, NS_LITERAL_STRING("normal"), &lineHeightChanged);
}

static already_AddRefed<nsStyleContext>
GetFontParentStyleContext(Element* aElement, nsIPresShell* aPresShell,
                          ErrorResult& aError)
{
  if (aElement && aElement->IsInUncomposedDoc()) {
    // Inherit from the canvas element.
    RefPtr<nsStyleContext> result =
      nsComputedDOMStyle::GetStyleContextForElement(aElement, nullptr,
                                                    aPresShell);
    if (!result) {
      aError.Throw(NS_ERROR_FAILURE);
      return nullptr;
    }
    return result.forget();
  }

  // otherwise inherit from default (10px sans-serif)

  nsStyleSet* styleSet = aPresShell->StyleSet()->GetAsGecko();
  if (!styleSet) {
    // XXXheycam ServoStyleSets do not support resolving style from a list of
    // rules yet.
    NS_ERROR("stylo: cannot resolve style for canvas from a ServoStyleSet yet");
    aError.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  bool changed;
  RefPtr<css::Declaration> parentRule =
    CreateFontDeclaration(NS_LITERAL_STRING("10px sans-serif"),
                          aPresShell->GetDocument(), &changed);

  nsTArray<nsCOMPtr<nsIStyleRule>> parentRules;
  parentRules.AppendElement(parentRule);
  RefPtr<nsStyleContext> result =
    styleSet->ResolveStyleForRules(nullptr, parentRules);

  if (!result) {
    aError.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }
  return result.forget();
}

static bool
PropertyIsInheritOrInitial(Declaration* aDeclaration, const nsCSSPropertyID aProperty)
{
  // We know the declaration is not !important, so we can use
  // GetNormalBlock().
  const nsCSSValue* filterVal =
    aDeclaration->GetNormalBlock()->ValueFor(aProperty);
  return (!filterVal || (filterVal->GetUnit() == eCSSUnit_Unset ||
                         filterVal->GetUnit() == eCSSUnit_Inherit ||
                         filterVal->GetUnit() == eCSSUnit_Initial));
}

static already_AddRefed<nsStyleContext>
GetFontStyleContext(Element* aElement, const nsAString& aFont,
                    nsIPresShell* aPresShell,
                    nsAString& aOutUsedFont,
                    ErrorResult& aError)
{
  nsStyleSet* styleSet = aPresShell->StyleSet()->GetAsGecko();
  if (!styleSet) {
    // XXXheycam ServoStyleSets do not support resolving style from a list of
    // rules yet.
    NS_ERROR("stylo: cannot resolve style for canvas from a ServoStyleSet yet");
    aError.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  bool fontParsedSuccessfully = false;
  RefPtr<css::Declaration> decl =
    CreateFontDeclaration(aFont, aPresShell->GetDocument(),
                          &fontParsedSuccessfully);

  if (!fontParsedSuccessfully) {
    // We got a syntax error.  The spec says this value must be ignored.
    return nullptr;
  }

  // In addition to unparseable values, the spec says we need to reject
  // 'inherit' and 'initial'. The easiest way to check for this is to look
  // at font-size-adjust, which the font shorthand resets to either 'none' or
  // '-moz-system-font'.
  if (PropertyIsInheritOrInitial(decl, eCSSProperty_font_size_adjust)) {
    return nullptr;
  }

  // have to get a parent style context for inherit-like relative
  // values (2em, bolder, etc.)
  RefPtr<nsStyleContext> parentContext =
    GetFontParentStyleContext(aElement, aPresShell, aError);

  if (aError.Failed()) {
    aError.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  MOZ_RELEASE_ASSERT(parentContext,
                     "GFX: GetFontParentStyleContext should have returned an error if it couldn't get a parent context.");

  MOZ_ASSERT(!aPresShell->IsDestroying(),
             "GetFontParentStyleContext should have returned an error if the presshell is being destroyed.");

  nsTArray<nsCOMPtr<nsIStyleRule>> rules;
  rules.AppendElement(decl);
  // add a rule to prevent text zoom from affecting the style
  rules.AppendElement(new nsDisableTextZoomStyleRule);

  RefPtr<nsStyleContext> sc =
    styleSet->ResolveStyleForRules(parentContext, rules);

  // The font getter is required to be reserialized based on what we
  // parsed (including having line-height removed).  (Older drafts of
  // the spec required font sizes be converted to pixels, but that no
  // longer seems to be required.)
  decl->GetValue(eCSSProperty_font, aOutUsedFont);

  return sc.forget();
}

static already_AddRefed<Declaration>
CreateFilterDeclaration(const nsAString& aFilter,
                        nsINode* aNode,
                        bool* aOutFilterChanged)
{
  bool dummy;
  return CreateDeclaration(aNode,
    eCSSProperty_filter, aFilter, aOutFilterChanged,
    eCSSProperty_UNKNOWN, EmptyString(), &dummy);
}

static already_AddRefed<nsStyleContext>
ResolveStyleForFilter(const nsAString& aFilterString,
                      nsIPresShell* aPresShell,
                      nsStyleContext* aParentContext,
                      ErrorResult& aError)
{
  nsStyleSet* styleSet = aPresShell->StyleSet()->GetAsGecko();
  if (!styleSet) {
    // XXXheycam ServoStyleSets do not support resolving style from a list of
    // rules yet.
    NS_ERROR("stylo: cannot resolve style for canvas from a ServoStyleSet yet");
    aError.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsIDocument* document = aPresShell->GetDocument();
  bool filterChanged = false;
  RefPtr<css::Declaration> decl =
    CreateFilterDeclaration(aFilterString, document, &filterChanged);

  if (!filterChanged) {
    // Refuse to accept the filter, but do not throw an error.
    return nullptr;
  }

  // In addition to unparseable values, the spec says we need to reject
  // 'inherit' and 'initial'.
  if (PropertyIsInheritOrInitial(decl, eCSSProperty_filter)) {
    return nullptr;
  }

  nsTArray<nsCOMPtr<nsIStyleRule>> rules;
  rules.AppendElement(decl);

  RefPtr<nsStyleContext> sc =
    styleSet->ResolveStyleForRules(aParentContext, rules);

  return sc.forget();
}

bool
CanvasRenderingContext2D::ParseFilter(const nsAString& aString,
                                      nsTArray<nsStyleFilter>& aFilterChain,
                                      ErrorResult& aError)
{
  if (!mCanvasElement && !mDocShell) {
    NS_WARNING("Canvas element must be non-null or a docshell must be provided");
    aError.Throw(NS_ERROR_FAILURE);
    return false;
  }

  nsCOMPtr<nsIPresShell> presShell = GetPresShell();
  if (!presShell) {
    aError.Throw(NS_ERROR_FAILURE);
    return false;
  }

  nsString usedFont;
  RefPtr<nsStyleContext> parentContext =
    GetFontStyleContext(mCanvasElement, GetFont(),
                        presShell, usedFont, aError);
  if (!parentContext) {
    aError.Throw(NS_ERROR_FAILURE);
    return false;
  }

  RefPtr<nsStyleContext> sc =
    ResolveStyleForFilter(aString, presShell, parentContext, aError);

  if (!sc) {
    return false;
  }

  aFilterChain = sc->StyleEffects()->mFilters;
  return true;
}

void
CanvasRenderingContext2D::SetFilter(const nsAString& aFilter, ErrorResult& aError)
{
  nsTArray<nsStyleFilter> filterChain;
  if (ParseFilter(aFilter, filterChain, aError)) {
    CurrentState().filterString = aFilter;
    filterChain.SwapElements(CurrentState().filterChain);
    if (mCanvasElement) {
      CurrentState().filterChainObserver =
        new CanvasFilterChainObserver(CurrentState().filterChain,
                                      mCanvasElement, this);
      UpdateFilter();
    }
  }
}

class CanvasUserSpaceMetrics : public UserSpaceMetricsWithSize
{
public:
  CanvasUserSpaceMetrics(const gfx::IntSize& aSize, const nsFont& aFont,
                         nsIAtom* aFontLanguage, bool aExplicitLanguage,
                         nsPresContext* aPresContext)
    : mSize(aSize)
    , mFont(aFont)
    , mFontLanguage(aFontLanguage)
    , mExplicitLanguage(aExplicitLanguage)
    , mPresContext(aPresContext)
  {
  }

  virtual float GetEmLength() const override
  {
    return NSAppUnitsToFloatPixels(mFont.size,
                                   nsPresContext::AppUnitsPerCSSPixel());
  }

  virtual float GetExLength() const override
  {
    nsDeviceContext* dc = mPresContext->DeviceContext();
    nsFontMetrics::Params params;
    params.language = mFontLanguage;
    params.explicitLanguage = mExplicitLanguage;
    params.textPerf = mPresContext->GetTextPerfMetrics();
    RefPtr<nsFontMetrics> fontMetrics = dc->GetMetricsFor(mFont, params);
    return NSAppUnitsToFloatPixels(fontMetrics->XHeight(),
                                   nsPresContext::AppUnitsPerCSSPixel());
  }

  virtual gfx::Size GetSize() const override
  { return Size(mSize); }

private:
  gfx::IntSize mSize;
  const nsFont& mFont;
  nsIAtom* mFontLanguage;
  bool mExplicitLanguage;
  nsPresContext* mPresContext;
};

void
CanvasRenderingContext2D::UpdateFilter()
{
  nsCOMPtr<nsIPresShell> presShell = GetPresShell();
  if (!presShell || presShell->IsDestroying()) {
    return;
  }

  // The filter might reference an SVG filter that is declared inside this
  // document. Flush frames so that we'll have an nsSVGFilterFrame to work
  // with.
  presShell->FlushPendingNotifications(Flush_Frames);

  CurrentState().filter =
    nsFilterInstance::GetFilterDescription(mCanvasElement,
      CurrentState().filterChain,
      CanvasUserSpaceMetrics(GetSize(),
                             CurrentState().fontFont,
                             CurrentState().fontLanguage,
                             CurrentState().fontExplicitLanguage,
                             presShell->GetPresContext()),
      gfxRect(0, 0, mWidth, mHeight),
      CurrentState().filterAdditionalImages);
  CurrentState().filterSourceGraphicTainted =
    (mCanvasElement && mCanvasElement->IsWriteOnly());
}

//
// rects
//

static bool
ValidateRect(double& aX, double& aY, double& aWidth, double& aHeight, bool aIsZeroSizeValid)
{
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

void
CanvasRenderingContext2D::ClearRect(double aX, double aY, double aW,
                                    double aH)
{
  // Do not allow zeros - it's a no-op at that point per spec.
  if (!ValidateRect(aX, aY, aW, aH, false)) {
    return;
  }

  gfx::Rect clearRect(aX, aY, aW, aH);

  EnsureTarget(&clearRect);

  mTarget->ClearRect(clearRect);

  RedrawUser(gfxRect(aX, aY, aW, aH));
}

void
CanvasRenderingContext2D::FillRect(double aX, double aY, double aW,
                                   double aH)
{
  const ContextState &state = CurrentState();

  if (!ValidateRect(aX, aY, aW, aH, true)) {
    return;
  }

  if (state.patternStyles[Style::FILL]) {
    CanvasPattern::RepeatMode repeat =
      state.patternStyles[Style::FILL]->mRepeat;
    // In the FillRect case repeat modes are easy to deal with.
    bool limitx = repeat == CanvasPattern::RepeatMode::NOREPEAT || repeat == CanvasPattern::RepeatMode::REPEATY;
    bool limity = repeat == CanvasPattern::RepeatMode::NOREPEAT || repeat == CanvasPattern::RepeatMode::REPEATX;

    IntSize patternSize =
      state.patternStyles[Style::FILL]->mSurface->GetSize();

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

  CompositionOp op = UsedOperation();
  bool discardContent = PatternIsOpaque(Style::FILL)
    && (op == CompositionOp::OP_OVER || op == CompositionOp::OP_SOURCE);

  const gfx::Rect fillRect(aX, aY, aW, aH);
  EnsureTarget(discardContent ? &fillRect : nullptr);

  gfx::Rect bounds;
  if (NeedToCalculateBounds()) {
    bounds = mTarget->GetTransform().TransformBounds(fillRect);
  }

  AntialiasMode antialiasMode = CurrentState().imageSmoothingEnabled ?
                                AntialiasMode::DEFAULT : AntialiasMode::NONE;

  AdjustedTarget(this, bounds.IsEmpty() ? nullptr : &bounds)->
    FillRect(gfx::Rect(aX, aY, aW, aH),
             CanvasGeneralPattern().ForStyle(this, Style::FILL, mTarget),
             DrawOptions(state.globalAlpha, op, antialiasMode));

  RedrawUser(gfxRect(aX, aY, aW, aH));
}

void
CanvasRenderingContext2D::StrokeRect(double aX, double aY, double aW,
                                     double aH)
{
  const ContextState &state = CurrentState();

  gfx::Rect bounds;

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

  if (NeedToCalculateBounds()) {
    bounds = gfx::Rect(aX - state.lineWidth / 2.0f, aY - state.lineWidth / 2.0f,
                       aW + state.lineWidth, aH + state.lineWidth);
    bounds = mTarget->GetTransform().TransformBounds(bounds);
  }

  if (!aH) {
    CapStyle cap = CapStyle::BUTT;
    if (state.lineJoin == JoinStyle::ROUND) {
      cap = CapStyle::ROUND;
    }
    AdjustedTarget(this, bounds.IsEmpty() ? nullptr : &bounds)->
      StrokeLine(Point(aX, aY), Point(aX + aW, aY),
                  CanvasGeneralPattern().ForStyle(this, Style::STROKE, mTarget),
                  StrokeOptions(state.lineWidth, state.lineJoin,
                                cap, state.miterLimit,
                                state.dash.Length(),
                                state.dash.Elements(),
                                state.dashOffset),
                  DrawOptions(state.globalAlpha, UsedOperation()));
    return;
  }

  if (!aW) {
    CapStyle cap = CapStyle::BUTT;
    if (state.lineJoin == JoinStyle::ROUND) {
      cap = CapStyle::ROUND;
    }
    AdjustedTarget(this, bounds.IsEmpty() ? nullptr : &bounds)->
      StrokeLine(Point(aX, aY), Point(aX, aY + aH),
                  CanvasGeneralPattern().ForStyle(this, Style::STROKE, mTarget),
                  StrokeOptions(state.lineWidth, state.lineJoin,
                                cap, state.miterLimit,
                                state.dash.Length(),
                                state.dash.Elements(),
                                state.dashOffset),
                  DrawOptions(state.globalAlpha, UsedOperation()));
    return;
  }

  AdjustedTarget(this, bounds.IsEmpty() ? nullptr : &bounds)->
    StrokeRect(gfx::Rect(aX, aY, aW, aH),
               CanvasGeneralPattern().ForStyle(this, Style::STROKE, mTarget),
               StrokeOptions(state.lineWidth, state.lineJoin,
                             state.lineCap, state.miterLimit,
                             state.dash.Length(),
                             state.dash.Elements(),
                             state.dashOffset),
               DrawOptions(state.globalAlpha, UsedOperation()));

  Redraw();
}

//
// path bits
//

void
CanvasRenderingContext2D::BeginPath()
{
  mPath = nullptr;
  mPathBuilder = nullptr;
  mDSPathBuilder = nullptr;
  mPathTransformWillUpdate = false;
}

void
CanvasRenderingContext2D::Fill(const CanvasWindingRule& aWinding)
{
  EnsureUserSpacePath(aWinding);

  if (!mPath) {
    return;
  }

  gfx::Rect bounds;

  if (NeedToCalculateBounds()) {
    bounds = mPath->GetBounds(mTarget->GetTransform());
  }

  AdjustedTarget(this, bounds.IsEmpty() ? nullptr : &bounds)->
    Fill(mPath, CanvasGeneralPattern().ForStyle(this, Style::FILL, mTarget),
         DrawOptions(CurrentState().globalAlpha, UsedOperation()));

  Redraw();
}

void CanvasRenderingContext2D::Fill(const CanvasPath& aPath, const CanvasWindingRule& aWinding)
{
  EnsureTarget();

  RefPtr<gfx::Path> gfxpath = aPath.GetPath(aWinding, mTarget);

  if (!gfxpath) {
    return;
  }

  gfx::Rect bounds;

  if (NeedToCalculateBounds()) {
    bounds = gfxpath->GetBounds(mTarget->GetTransform());
  }

  AdjustedTarget(this, bounds.IsEmpty() ? nullptr : &bounds)->
    Fill(gfxpath, CanvasGeneralPattern().ForStyle(this, Style::FILL, mTarget),
         DrawOptions(CurrentState().globalAlpha, UsedOperation()));

  Redraw();
}

void
CanvasRenderingContext2D::Stroke()
{
  EnsureUserSpacePath();

  if (!mPath) {
    return;
  }

  const ContextState &state = CurrentState();

  StrokeOptions strokeOptions(state.lineWidth, state.lineJoin,
                              state.lineCap, state.miterLimit,
                              state.dash.Length(), state.dash.Elements(),
                              state.dashOffset);

  gfx::Rect bounds;
  if (NeedToCalculateBounds()) {
    bounds =
      mPath->GetStrokedBounds(strokeOptions, mTarget->GetTransform());
  }

  AdjustedTarget(this, bounds.IsEmpty() ? nullptr : &bounds)->
    Stroke(mPath, CanvasGeneralPattern().ForStyle(this, Style::STROKE, mTarget),
           strokeOptions, DrawOptions(state.globalAlpha, UsedOperation()));

  Redraw();
}

void
CanvasRenderingContext2D::Stroke(const CanvasPath& aPath)
{
  EnsureTarget();

  RefPtr<gfx::Path> gfxpath = aPath.GetPath(CanvasWindingRule::Nonzero, mTarget);

  if (!gfxpath) {
    return;
  }

  const ContextState &state = CurrentState();

  StrokeOptions strokeOptions(state.lineWidth, state.lineJoin,
                              state.lineCap, state.miterLimit,
                              state.dash.Length(), state.dash.Elements(),
                              state.dashOffset);

  gfx::Rect bounds;
  if (NeedToCalculateBounds()) {
    bounds =
      gfxpath->GetStrokedBounds(strokeOptions, mTarget->GetTransform());
  }

  AdjustedTarget(this, bounds.IsEmpty() ? nullptr : &bounds)->
    Stroke(gfxpath, CanvasGeneralPattern().ForStyle(this, Style::STROKE, mTarget),
           strokeOptions, DrawOptions(state.globalAlpha, UsedOperation()));

  Redraw();
}

void CanvasRenderingContext2D::DrawFocusIfNeeded(mozilla::dom::Element& aElement,
                                                 ErrorResult& aRv)
{
  EnsureUserSpacePath();

  if (!mPath) {
    return;
  }

  if (DrawCustomFocusRing(aElement)) {
    Save();

    // set state to conforming focus state
    ContextState& state = CurrentState();
    state.globalAlpha = 1.0;
    state.shadowBlur = 0;
    state.shadowOffset.x = 0;
    state.shadowOffset.y = 0;
    state.op = mozilla::gfx::CompositionOp::OP_OVER;

    state.lineCap = CapStyle::BUTT;
    state.lineJoin = mozilla::gfx::JoinStyle::MITER_OR_BEVEL;
    state.lineWidth = 1;
    CurrentState().dash.Clear();

    // color and style of the rings is the same as for image maps
    // set the background focus color
    CurrentState().SetColorStyle(Style::STROKE, NS_RGBA(255, 255, 255, 255));
    // draw the focus ring
    Stroke();

    // set dashing for foreground
    nsTArray<mozilla::gfx::Float>& dash = CurrentState().dash;
    for (uint32_t i = 0; i < 2; ++i) {
      if (!dash.AppendElement(1, fallible)) {
        aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
        return;
      }
    }

    // set the foreground focus color
    CurrentState().SetColorStyle(Style::STROKE, NS_RGBA(0,0,0, 255));
    // draw the focus ring
    Stroke();

    Restore();
  }
}

bool CanvasRenderingContext2D::DrawCustomFocusRing(mozilla::dom::Element& aElement)
{
  EnsureUserSpacePath();

  HTMLCanvasElement* canvas = GetCanvas();

  if (!canvas|| !nsContentUtils::ContentIsDescendantOf(&aElement, canvas)) {
    return false;
  }

  nsIFocusManager* fm = nsFocusManager::GetFocusManager();
  if (fm) {
    // check that the element i focused
    nsCOMPtr<nsIDOMElement> focusedElement;
    fm->GetFocusedElement(getter_AddRefs(focusedElement));
    if (SameCOMIdentity(aElement.AsDOMNode(), focusedElement)) {
      if (nsPIDOMWindowOuter *window = aElement.OwnerDoc()->GetWindow()) {
        return window->ShouldShowFocusRing();
      }
    }
  }

  return false;
}

void
CanvasRenderingContext2D::Clip(const CanvasWindingRule& aWinding)
{
  EnsureUserSpacePath(aWinding);

  if (!mPath) {
    return;
  }

  mTarget->PushClip(mPath);
  CurrentState().clipsAndTransforms.AppendElement(ClipState(mPath));
}

void
CanvasRenderingContext2D::Clip(const CanvasPath& aPath, const CanvasWindingRule& aWinding)
{
  EnsureTarget();

  RefPtr<gfx::Path> gfxpath = aPath.GetPath(aWinding, mTarget);

  if (!gfxpath) {
    return;
  }

  mTarget->PushClip(gfxpath);
  CurrentState().clipsAndTransforms.AppendElement(ClipState(gfxpath));
}

void
CanvasRenderingContext2D::ArcTo(double aX1, double aY1, double aX2,
                                double aY2, double aRadius,
                                ErrorResult& aError)
{
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
  double dir, a2, b2, c2, cosx, sinx, d, anx, any,
         bnx, bny, x3, y3, x4, y4, cx, cy, angle0, angle1;
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
  a2 = (p0.x-aX1)*(p0.x-aX1) + (p0.y-aY1)*(p0.y-aY1);
  b2 = (aX1-aX2)*(aX1-aX2) + (aY1-aY2)*(aY1-aY2);
  c2 = (p0.x-aX2)*(p0.x-aX2) + (p0.y-aY2)*(p0.y-aY2);
  cosx = (a2+b2-c2)/(2*sqrt(a2*b2));

  sinx = sqrt(1 - cosx*cosx);
  d = aRadius / ((1 - cosx) / sinx);

  anx = (aX1-p0.x) / sqrt(a2);
  any = (aY1-p0.y) / sqrt(a2);
  bnx = (aX1-aX2) / sqrt(b2);
  bny = (aY1-aY2) / sqrt(b2);
  x3 = aX1 - anx*d;
  y3 = aY1 - any*d;
  x4 = aX1 - bnx*d;
  y4 = aY1 - bny*d;
  anticlockwise = (dir < 0);
  cx = x3 + any*aRadius*(anticlockwise ? 1 : -1);
  cy = y3 - anx*aRadius*(anticlockwise ? 1 : -1);
  angle0 = atan2((y3-cy), (x3-cx));
  angle1 = atan2((y4-cy), (x4-cx));


  LineTo(x3, y3);

  Arc(cx, cy, aRadius, angle0, angle1, anticlockwise, aError);
}

void
CanvasRenderingContext2D::Arc(double aX, double aY, double aR,
                              double aStartAngle, double aEndAngle,
                              bool aAnticlockwise, ErrorResult& aError)
{
  if (aR < 0.0) {
    aError.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return;
  }

  EnsureWritablePath();

  ArcToBezier(this, Point(aX, aY), Size(aR, aR), aStartAngle, aEndAngle, aAnticlockwise);
}

void
CanvasRenderingContext2D::Rect(double aX, double aY, double aW, double aH)
{
  EnsureWritablePath();

  if (mPathBuilder) {
    mPathBuilder->MoveTo(Point(aX, aY));
    mPathBuilder->LineTo(Point(aX + aW, aY));
    mPathBuilder->LineTo(Point(aX + aW, aY + aH));
    mPathBuilder->LineTo(Point(aX, aY + aH));
    mPathBuilder->Close();
  } else {
    mDSPathBuilder->MoveTo(mTarget->GetTransform().TransformPoint(Point(aX, aY)));
    mDSPathBuilder->LineTo(mTarget->GetTransform().TransformPoint(Point(aX + aW, aY)));
    mDSPathBuilder->LineTo(mTarget->GetTransform().TransformPoint(Point(aX + aW, aY + aH)));
    mDSPathBuilder->LineTo(mTarget->GetTransform().TransformPoint(Point(aX, aY + aH)));
    mDSPathBuilder->Close();
  }
}

void
CanvasRenderingContext2D::Ellipse(double aX, double aY, double aRadiusX, double aRadiusY,
                                  double aRotation, double aStartAngle, double aEndAngle,
                                  bool aAnticlockwise, ErrorResult& aError)
{
  if (aRadiusX < 0.0 || aRadiusY < 0.0) {
    aError.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return;
  }

  EnsureWritablePath();

  ArcToBezier(this, Point(aX, aY), Size(aRadiusX, aRadiusY), aStartAngle, aEndAngle,
              aAnticlockwise, aRotation);
}

void
CanvasRenderingContext2D::EnsureWritablePath()
{
  EnsureTarget();

  if (mDSPathBuilder) {
    return;
  }

  FillRule fillRule = CurrentState().fillRule;

  if (mPathBuilder) {
    if (mPathTransformWillUpdate) {
      mPath = mPathBuilder->Finish();
      mDSPathBuilder =
        mPath->TransformedCopyToBuilder(mPathToDS, fillRule);
      mPath = nullptr;
      mPathBuilder = nullptr;
      mPathTransformWillUpdate = false;
    }
    return;
  }

  if (!mPath) {
    NS_ASSERTION(!mPathTransformWillUpdate, "mPathTransformWillUpdate should be false, if all paths are null");
    mPathBuilder = mTarget->CreatePathBuilder(fillRule);
  } else if (!mPathTransformWillUpdate) {
    mPathBuilder = mPath->CopyToBuilder(fillRule);
  } else {
    mDSPathBuilder =
      mPath->TransformedCopyToBuilder(mPathToDS, fillRule);
    mPathTransformWillUpdate = false;
    mPath = nullptr;
  }
}

void
CanvasRenderingContext2D::EnsureUserSpacePath(const CanvasWindingRule& aWinding)
{
  FillRule fillRule = CurrentState().fillRule;
  if (aWinding == CanvasWindingRule::Evenodd)
    fillRule = FillRule::FILL_EVEN_ODD;

  EnsureTarget();

  if (!mPath && !mPathBuilder && !mDSPathBuilder) {
    mPathBuilder = mTarget->CreatePathBuilder(fillRule);
  }

  if (mPathBuilder) {
    mPath = mPathBuilder->Finish();
    mPathBuilder = nullptr;
  }

  if (mPath &&
      mPathTransformWillUpdate) {
    mDSPathBuilder =
      mPath->TransformedCopyToBuilder(mPathToDS, fillRule);
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

    mPathBuilder =
      dsPath->TransformedCopyToBuilder(inverse, fillRule);
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

void
CanvasRenderingContext2D::TransformWillUpdate()
{
  EnsureTarget();

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

void
CanvasRenderingContext2D::SetFont(const nsAString& aFont,
                                  ErrorResult& aError)
{
  SetFontInternal(aFont, aError);
}

bool
CanvasRenderingContext2D::SetFontInternal(const nsAString& aFont,
                                          ErrorResult& aError)
{
  /*
    * If font is defined with relative units (e.g. ems) and the parent
    * style context changes in between calls, setting the font to the
    * same value as previous could result in a different computed value,
    * so we cannot have the optimization where we check if the new font
    * string is equal to the old one.
    */

  if (!mCanvasElement && !mDocShell) {
    NS_WARNING("Canvas element must be non-null or a docshell must be provided");
    aError.Throw(NS_ERROR_FAILURE);
    return false;
  }

  nsCOMPtr<nsIPresShell> presShell = GetPresShell();
  if (!presShell) {
    aError.Throw(NS_ERROR_FAILURE);
    return false;
  }

  nsString usedFont;
  RefPtr<nsStyleContext> sc =
    GetFontStyleContext(mCanvasElement, aFont, presShell, usedFont, aError);
  if (!sc) {
    return false;
  }

  const nsStyleFont* fontStyle = sc->StyleFont();

  nsPresContext *c = presShell->GetPresContext();

  // Purposely ignore the font size that respects the user's minimum
  // font preference (fontStyle->mFont.size) in favor of the computed
  // size (fontStyle->mSize).  See
  // https://bugzilla.mozilla.org/show_bug.cgi?id=698652.
  MOZ_ASSERT(!fontStyle->mAllowZoom,
             "expected text zoom to be disabled on this nsStyleFont");
  nsFont resizedFont(fontStyle->mFont);
  // Create a font group working in units of CSS pixels instead of the usual
  // device pixels, to avoid being affected by page zoom. nsFontMetrics will
  // convert nsFont size in app units to device pixels for the font group, so
  // here we first apply to the size the equivalent of a conversion from device
  // pixels to CSS pixels, to adjust for the difference in expectations from
  // other nsFontMetrics clients.
  resizedFont.size =
    (fontStyle->mSize * c->AppUnitsPerDevPixel()) / c->AppUnitsPerCSSPixel();

  nsFontMetrics::Params params;
  params.language = fontStyle->mLanguage;
  params.explicitLanguage = fontStyle->mExplicitLanguage;
  params.userFontSet = c->GetUserFontSet();
  params.textPerf = c->GetTextPerfMetrics();
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

void
CanvasRenderingContext2D::SetTextAlign(const nsAString& aTextAlign)
{
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

void
CanvasRenderingContext2D::GetTextAlign(nsAString& aTextAlign)
{
  switch (CurrentState().textAlign)
  {
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

void
CanvasRenderingContext2D::SetTextBaseline(const nsAString& aTextBaseline)
{
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

void
CanvasRenderingContext2D::GetTextBaseline(nsAString& aTextBaseline)
{
  switch (CurrentState().textBaseline)
  {
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
static inline void
TextReplaceWhitespaceCharacters(nsAutoString& aStr)
{
  aStr.ReplaceChar("\x09\x0A\x0B\x0C\x0D", char16_t(' '));
}

void
CanvasRenderingContext2D::FillText(const nsAString& aText, double aX,
                                   double aY,
                                   const Optional<double>& aMaxWidth,
                                   ErrorResult& aError)
{
  aError = DrawOrMeasureText(aText, aX, aY, aMaxWidth, TextDrawOperation::FILL, nullptr);
}

void
CanvasRenderingContext2D::StrokeText(const nsAString& aText, double aX,
                                     double aY,
                                     const Optional<double>& aMaxWidth,
                                     ErrorResult& aError)
{
  aError = DrawOrMeasureText(aText, aX, aY, aMaxWidth, TextDrawOperation::STROKE, nullptr);
}

TextMetrics*
CanvasRenderingContext2D::MeasureText(const nsAString& aRawText,
                                      ErrorResult& aError)
{
  float width;
  Optional<double> maxWidth;
  aError = DrawOrMeasureText(aRawText, 0, 0, maxWidth, TextDrawOperation::MEASURE, &width);
  if (aError.Failed()) {
    return nullptr;
  }

  return new TextMetrics(width);
}

void
CanvasRenderingContext2D::AddHitRegion(const HitRegionOptions& aOptions, ErrorResult& aError)
{
  RefPtr<gfx::Path> path;
  if (aOptions.mPath) {
    EnsureTarget();
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
  RefPtr<PathBuilder> pathBuilder = path->TransformedCopyToBuilder(mTarget->GetTransform());
  info.mPath = pathBuilder->Finish();

  mHitRegionsOptions.InsertElementAt(0, info);
}

void
CanvasRenderingContext2D::RemoveHitRegion(const nsAString& aId)
{
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

void
CanvasRenderingContext2D::ClearHitRegions()
{
  mHitRegionsOptions.Clear();
}

bool
CanvasRenderingContext2D::GetHitRegionRect(Element* aElement, nsRect& aRect)
{
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
struct MOZ_STACK_CLASS CanvasBidiProcessor : public nsBidiPresUtils::BidiProcessor
{
  typedef CanvasRenderingContext2D::Style Style;

  CanvasBidiProcessor()
    : nsBidiPresUtils::BidiProcessor()
  {
    if (Preferences::GetBool(GFX_MISSING_FONTS_NOTIFY_PREF)) {
      mMissingFonts = new gfxMissingFontRecorder();
    }
  }

  ~CanvasBidiProcessor()
  {
    // notify front-end code if we encountered missing glyphs in any script
    if (mMissingFonts) {
      mMissingFonts->Flush();
    }
  }

  typedef CanvasRenderingContext2D::ContextState ContextState;

  virtual void SetText(const char16_t* aText, int32_t aLength, nsBidiDirection aDirection)
  {
    mFontgrp->UpdateUserFonts(); // ensure user font generation is current
    // adjust flags for current direction run
    uint32_t flags = mTextRunFlags;
    if (aDirection == NSBIDI_RTL) {
      flags |= gfxTextRunFactory::TEXT_IS_RTL;
    } else {
      flags &= ~gfxTextRunFactory::TEXT_IS_RTL;
    }
    mTextRun = mFontgrp->MakeTextRun(aText,
                                     aLength,
                                     mDrawTarget,
                                     mAppUnitsPerDevPixel,
                                     flags,
                                     mMissingFonts);
  }

  virtual nscoord GetWidth()
  {
    gfxTextRun::Metrics textRunMetrics = mTextRun->MeasureText(
        mDoMeasureBoundingBox ? gfxFont::TIGHT_INK_EXTENTS
                              : gfxFont::LOOSE_INK_EXTENTS, mDrawTarget);

    // this only measures the height; the total width is gotten from the
    // the return value of ProcessText.
    if (mDoMeasureBoundingBox) {
      textRunMetrics.mBoundingBox.Scale(1.0 / mAppUnitsPerDevPixel);
      mBoundingBox = mBoundingBox.Union(textRunMetrics.mBoundingBox);
    }

    return NSToCoordRound(textRunMetrics.mAdvanceWidth);
  }

  already_AddRefed<gfxPattern> GetGradientFor(Style aStyle)
  {
    RefPtr<gfxPattern> pattern;
    CanvasGradient* gradient = mState->gradientStyles[aStyle];
    CanvasGradient::Type type = gradient->GetType();

    switch (type) {
    case CanvasGradient::Type::RADIAL: {
      CanvasRadialGradient* radial =
        static_cast<CanvasRadialGradient*>(gradient);
      pattern = new gfxPattern(radial->mCenter1.x, radial->mCenter1.y,
                               radial->mRadius1, radial->mCenter2.x,
                               radial->mCenter2.y, radial->mRadius2);
      break;
    }
    case CanvasGradient::Type::LINEAR: {
      CanvasLinearGradient* linear =
        static_cast<CanvasLinearGradient*>(gradient);
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
    CanvasPattern::RepeatMode aRepeatMode)
  {
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

  already_AddRefed<gfxPattern> GetPatternFor(Style aStyle)
  {
    const CanvasPattern* pat = mState->patternStyles[aStyle];
    RefPtr<gfxPattern> pattern = new gfxPattern(pat->mSurface, Matrix());
    pattern->SetExtend(CvtCanvasRepeatToGfxRepeat(pat->mRepeat));
    return pattern.forget();
  }

  virtual void DrawText(nscoord aXOffset, nscoord aWidth)
  {
    gfxPoint point = mPt;
    bool rtl = mTextRun->IsRightToLeft();
    bool verticalRun = mTextRun->IsVertical();
    RefPtr<gfxPattern> pattern;

    gfxFloat& inlineCoord = verticalRun ? point.y : point.x;
    inlineCoord += aXOffset;

    // offset is given in terms of left side of string
    if (rtl) {
      // Bug 581092 - don't use rounded pixel width to advance to
      // right-hand end of run, because this will cause different
      // glyph positioning for LTR vs RTL drawing of the same
      // glyph string on OS X and DWrite where textrun widths may
      // involve fractional pixels.
      gfxTextRun::Metrics textRunMetrics =
        mTextRun->MeasureText(mDoMeasureBoundingBox ?
                                gfxFont::TIGHT_INK_EXTENTS :
                                gfxFont::LOOSE_INK_EXTENTS,
                              mDrawTarget);
      inlineCoord += textRunMetrics.mAdvanceWidth;
      // old code was:
      //   point.x += width * mAppUnitsPerDevPixel;
      // TODO: restore this if/when we move to fractional coords
      // throughout the text layout process
    }

    mCtx->EnsureTarget();

    // Defer the tasks to gfxTextRun which will handle color/svg-in-ot fonts
    // appropriately.
    StrokeOptions strokeOpts;
    DrawOptions drawOpts;
    Style style = (mOp == CanvasRenderingContext2D::TextDrawOperation::FILL)
                    ? Style::FILL
                    : Style::STROKE;
    RefPtr<gfxContext> thebes =
      gfxContext::CreatePreservingTransformOrNull(mCtx->mTarget);
    gfxTextRun::DrawParams params(thebes);

    if (mState->StyleIsColor(style)) { // Color
      nscolor fontColor = mState->colorStyles[style];
      if (style == Style::FILL) {
        params.context->SetColor(Color::FromABGR(fontColor));
      } else {
        params.textStrokeColor = fontColor;
      }
    } else {
      if (mState->gradientStyles[style]) { // Gradient
        pattern = GetGradientFor(style);
      } else if (mState->patternStyles[style]) { // Pattern
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

    const ContextState& state = *mState;
    drawOpts.mAlpha = state.globalAlpha;
    drawOpts.mCompositionOp = mCtx->UsedOperation();
    params.drawOpts = &drawOpts;

    if (style == Style::STROKE) {
      strokeOpts.mLineWidth = state.lineWidth;
      strokeOpts.mLineJoin = state.lineJoin;
      strokeOpts.mLineCap = state.lineCap;
      strokeOpts.mMiterLimit = state.miterLimit;
      strokeOpts.mDashLength = state.dash.Length();
      strokeOpts.mDashPattern =
        (strokeOpts.mDashLength > 0) ? state.dash.Elements() : 0;
      strokeOpts.mDashOffset = state.dashOffset;

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
  CanvasRenderingContext2D *mCtx;

  // position of the left side of the string, alphabetic baseline
  gfxPoint mPt;

  // current font
  gfxFontGroup* mFontgrp;

  // to record any unsupported characters found in the text,
  // and notify front-end if it is interested
  nsAutoPtr<gfxMissingFontRecorder> mMissingFonts;

  // dev pixel conversion factor
  int32_t mAppUnitsPerDevPixel;

  // operation (fill or stroke)
  CanvasRenderingContext2D::TextDrawOperation mOp;

  // context state
  ContextState *mState;

  // union of bounding boxes of all runs, needed for shadows
  gfxRect mBoundingBox;

  // flags to use when creating textrun, based on CSS style
  uint32_t mTextRunFlags;

  // true iff the bounding box should be measured
  bool mDoMeasureBoundingBox;
};

nsresult
CanvasRenderingContext2D::DrawOrMeasureText(const nsAString& aRawText,
                                            float aX,
                                            float aY,
                                            const Optional<double>& aMaxWidth,
                                            TextDrawOperation aOp,
                                            float* aWidth)
{
  nsresult rv;

  if (!mCanvasElement && !mDocShell) {
    NS_WARNING("Canvas element must be non-null or a docshell must be provided");
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIPresShell> presShell = GetPresShell();
  if (!presShell)
    return NS_ERROR_FAILURE;

  nsIDocument* document = presShell->GetDocument();

  // replace all the whitespace characters with U+0020 SPACE
  nsAutoString textToDraw(aRawText);
  TextReplaceWhitespaceCharacters(textToDraw);

  // According to spec, the API should return an empty array if maxWidth was provided
  // but is less than or equal to zero or equal to NaN.
  if (aMaxWidth.WasPassed() && (aMaxWidth.Value() <= 0 || IsNaN(aMaxWidth.Value()))) {
    textToDraw.Truncate();
  }

  // for now, default to ltr if not in doc
  bool isRTL = false;

  RefPtr<nsStyleContext> canvasStyle;
  if (mCanvasElement && mCanvasElement->IsInUncomposedDoc()) {
    // try to find the closest context
    canvasStyle =
      nsComputedDOMStyle::GetStyleContextForElement(mCanvasElement,
                                                    nullptr,
                                                    presShell);
    if (!canvasStyle) {
      return NS_ERROR_FAILURE;
    }

    isRTL = canvasStyle->StyleVisibility()->mDirection ==
      NS_STYLE_DIRECTION_RTL;
  } else {
    isRTL = GET_BIDI_OPTION_DIRECTION(document->GetBidiOptions()) == IBMBIDI_TEXTDIRECTION_RTL;
  }

  gfxFontGroup* currentFontStyle = GetCurrentFontStyle();
  if (!currentFontStyle) {
    return NS_ERROR_FAILURE;
  }

  MOZ_ASSERT(!presShell->IsDestroying(),
             "GetCurrentFontStyle() should have returned null if the presshell is being destroyed");

  // ensure user font set is up to date
  currentFontStyle->
    SetUserFontSet(presShell->GetPresContext()->GetUserFontSet());

  if (currentFontStyle->GetStyle()->size == 0.0F) {
    if (aWidth) {
      *aWidth = 0;
    }
    return NS_OK;
  }

  if (!IsFinite(aX) || !IsFinite(aY)) {
    return NS_OK;
  }

  const ContextState &state = CurrentState();

  // This is only needed to know if we can know the drawing bounding box easily.
  bool doCalculateBounds = NeedToCalculateBounds();

  CanvasBidiProcessor processor;

  // If we don't have a style context, we can't set up vertical-text flags
  // (for now, at least; perhaps we need new Canvas API to control this).
  processor.mTextRunFlags = canvasStyle ?
    nsLayoutUtils::GetTextRunFlagsForStyle(canvasStyle,
                                           canvasStyle->StyleFont(),
                                           canvasStyle->StyleText(),
                                           0) : 0;

  GetAppUnitsValues(&processor.mAppUnitsPerDevPixel, nullptr);
  processor.mPt = gfxPoint(aX, aY);
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
  processor.mDoMeasureBoundingBox = doCalculateBounds || !mIsEntireFrameInvalid;
  processor.mState = &CurrentState();
  processor.mFontgrp = currentFontStyle;

  nscoord totalWidthCoord;

  // calls bidi algo twice since it needs the full text width and the
  // bounding boxes before rendering anything
  nsBidi bidiEngine;
  rv = nsBidiPresUtils::ProcessText(textToDraw.get(),
                                    textToDraw.Length(),
                                    isRTL ? NSBIDI_RTL : NSBIDI_LTR,
                                    presShell->GetPresContext(),
                                    processor,
                                    nsBidiPresUtils::MODE_MEASURE,
                                    nullptr,
                                    0,
                                    &totalWidthCoord,
                                    &bidiEngine);
  if (NS_FAILED(rv)) {
    return rv;
  }

  float totalWidth = float(totalWidthCoord) / processor.mAppUnitsPerDevPixel;
  if (aWidth) {
    *aWidth = totalWidth;
  }

  // if only measuring, don't need to do any more work
  if (aOp==TextDrawOperation::MEASURE) {
    return NS_OK;
  }

  // offset pt.x based on text align
  gfxFloat anchorX;

  if (state.textAlign == TextAlign::CENTER) {
    anchorX = .5;
  } else if (state.textAlign == TextAlign::LEFT ||
            (!isRTL && state.textAlign == TextAlign::START) ||
            (isRTL && state.textAlign == TextAlign::END)) {
    anchorX = 0;
  } else {
    anchorX = 1;
  }

  processor.mPt.x -= anchorX * totalWidth;

  // offset pt.y (or pt.x, for vertical text) based on text baseline
  processor.mFontgrp->UpdateUserFonts(); // ensure user font generation is current
  const gfxFont::Metrics& fontMetrics =
    processor.mFontgrp->GetFirstValidFont()->GetMetrics(gfxFont::eHorizontal);

  gfxFloat baselineAnchor;

  switch (state.textBaseline)
  {
  case TextBaseline::HANGING:
      // fall through; best we can do with the information available
  case TextBaseline::TOP:
    baselineAnchor = fontMetrics.emAscent;
    break;
  case TextBaseline::MIDDLE:
    baselineAnchor = (fontMetrics.emAscent - fontMetrics.emDescent) * .5f;
    break;
  case TextBaseline::IDEOGRAPHIC:
    // fall through; best we can do with the information available
  case TextBaseline::ALPHABETIC:
    baselineAnchor = 0;
    break;
  case TextBaseline::BOTTOM:
    baselineAnchor = -fontMetrics.emDescent;
    break;
  default:
    MOZ_CRASH("GFX: unexpected TextBaseline");
  }

  // We can't query the textRun directly, as it may not have been created yet;
  // so instead we check the flags that will be used to initialize it.
  uint16_t runOrientation =
    (processor.mTextRunFlags & gfxTextRunFactory::TEXT_ORIENT_MASK);
  if (runOrientation != gfxTextRunFactory::TEXT_ORIENT_HORIZONTAL) {
    if (runOrientation == gfxTextRunFactory::TEXT_ORIENT_VERTICAL_MIXED ||
        runOrientation == gfxTextRunFactory::TEXT_ORIENT_VERTICAL_UPRIGHT) {
      // Adjust to account for mTextRun being shaped using center baseline
      // rather than alphabetic.
      baselineAnchor -= (fontMetrics.emAscent - fontMetrics.emDescent) * .5f;
    }
    processor.mPt.x -= baselineAnchor;
  } else {
    processor.mPt.y += baselineAnchor;
  }

  // correct bounding box to get it to be the correct size/position
  processor.mBoundingBox.width = totalWidth;
  processor.mBoundingBox.MoveBy(processor.mPt);

  processor.mPt.x *= processor.mAppUnitsPerDevPixel;
  processor.mPt.y *= processor.mAppUnitsPerDevPixel;

  EnsureTarget();
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

  rv = nsBidiPresUtils::ProcessText(textToDraw.get(),
                                    textToDraw.Length(),
                                    isRTL ? NSBIDI_RTL : NSBIDI_LTR,
                                    presShell->GetPresContext(),
                                    processor,
                                    nsBidiPresUtils::MODE_DRAW,
                                    nullptr,
                                    0,
                                    nullptr,
                                    &bidiEngine);


  mTarget->SetTransform(oldTransform);

  if (aOp == CanvasRenderingContext2D::TextDrawOperation::FILL &&
      !doCalculateBounds) {
    RedrawUser(boundingBox);
    return NS_OK;
  }

  Redraw();
  return NS_OK;
}

gfxFontGroup *CanvasRenderingContext2D::GetCurrentFontStyle()
{
  // use lazy initilization for the font group since it's rather expensive
  if (!CurrentState().fontGroup) {
    ErrorResult err;
    NS_NAMED_LITERAL_STRING(kDefaultFontStyle, "10px sans-serif");
    static float kDefaultFontSize = 10.0;
    nsCOMPtr<nsIPresShell> presShell = GetPresShell();
    bool fontUpdated = SetFontInternal(kDefaultFontStyle, err);
    if (err.Failed() || !fontUpdated) {
      err.SuppressException();
      gfxFontStyle style;
      style.size = kDefaultFontSize;
      gfxTextPerfMetrics* tp = nullptr;
      if (presShell && !presShell->IsDestroying()) {
        tp = presShell->GetPresContext()->GetTextPerfMetrics();
      }
      int32_t perDevPixel, perCSSPixel;
      GetAppUnitsValues(&perDevPixel, &perCSSPixel);
      gfxFloat devToCssSize = gfxFloat(perDevPixel) / gfxFloat(perCSSPixel);
      CurrentState().fontGroup =
        gfxPlatform::GetPlatform()->CreateFontGroup(FontFamilyList(eFamily_sans_serif),
                                                    &style, tp,
                                                    nullptr, devToCssSize);
      if (CurrentState().fontGroup) {
        CurrentState().font = kDefaultFontStyle;
      } else {
        NS_ERROR("Default canvas font is invalid");
      }
    }
  }

  return CurrentState().fontGroup;
}

//
// line caps/joins
//

void
CanvasRenderingContext2D::SetLineCap(const nsAString& aLinecapStyle)
{
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

void
CanvasRenderingContext2D::GetLineCap(nsAString& aLinecapStyle)
{
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

void
CanvasRenderingContext2D::SetLineJoin(const nsAString& aLinejoinStyle)
{
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

void
CanvasRenderingContext2D::GetLineJoin(nsAString& aLinejoinStyle, ErrorResult& aError)
{
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

void
CanvasRenderingContext2D::SetMozDash(JSContext* aCx,
                                     const JS::Value& aMozDash,
                                     ErrorResult& aError)
{
  nsTArray<Float> dash;
  aError = JSValToDashArray(aCx, aMozDash, dash);
  if (!aError.Failed()) {
    ContextState& state = CurrentState();
    state.dash = Move(dash);
    if (state.dash.IsEmpty()) {
      state.dashOffset = 0;
    }
  }
}

void
CanvasRenderingContext2D::GetMozDash(JSContext* aCx,
                                     JS::MutableHandle<JS::Value> aRetval,
                                     ErrorResult& aError)
{
  DashArrayToJSVal(CurrentState().dash, aCx, aRetval, aError);
}

void
CanvasRenderingContext2D::SetMozDashOffset(double aMozDashOffset)
{
  ContextState& state = CurrentState();
  if (!state.dash.IsEmpty()) {
    state.dashOffset = aMozDashOffset;
  }
}

void
CanvasRenderingContext2D::SetLineDash(const Sequence<double>& aSegments,
                                      ErrorResult& aRv)
{
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
  if (aSegments.Length() % 2) { // If the number of elements is odd, concatenate again
    for (uint32_t x = 0; x < aSegments.Length(); x++) {
      if (!dash.AppendElement(aSegments[x], fallible)) {
        aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
        return;
      }
    }
  }

  CurrentState().dash = Move(dash);
}

void
CanvasRenderingContext2D::GetLineDash(nsTArray<double>& aSegments) const {
  const nsTArray<mozilla::gfx::Float>& dash = CurrentState().dash;
  aSegments.Clear();

  for (uint32_t x = 0; x < dash.Length(); x++) {
    aSegments.AppendElement(dash[x]);
  }
}

void
CanvasRenderingContext2D::SetLineDashOffset(double aOffset) {
  CurrentState().dashOffset = aOffset;
}

double
CanvasRenderingContext2D::LineDashOffset() const {
  return CurrentState().dashOffset;
}

bool
CanvasRenderingContext2D::IsPointInPath(double aX, double aY, const CanvasWindingRule& aWinding)
{
  if (!FloatValidate(aX, aY)) {
    return false;
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

bool CanvasRenderingContext2D::IsPointInPath(const CanvasPath& aPath, double aX, double aY, const CanvasWindingRule& aWinding)
{
  if (!FloatValidate(aX, aY)) {
    return false;
  }

  EnsureTarget();
  RefPtr<gfx::Path> tempPath = aPath.GetPath(aWinding, mTarget);

  return tempPath->ContainsPoint(Point(aX, aY), mTarget->GetTransform());
}

bool
CanvasRenderingContext2D::IsPointInStroke(double aX, double aY)
{
  if (!FloatValidate(aX, aY)) {
    return false;
  }

  EnsureUserSpacePath();
  if (!mPath) {
    return false;
  }

  const ContextState &state = CurrentState();

  StrokeOptions strokeOptions(state.lineWidth,
                              state.lineJoin,
                              state.lineCap,
                              state.miterLimit,
                              state.dash.Length(),
                              state.dash.Elements(),
                              state.dashOffset);

  if (mPathTransformWillUpdate) {
    return mPath->StrokeContainsPoint(strokeOptions, Point(aX, aY), mPathToDS);
  }
  return mPath->StrokeContainsPoint(strokeOptions, Point(aX, aY), mTarget->GetTransform());
}

bool CanvasRenderingContext2D::IsPointInStroke(const CanvasPath& aPath, double aX, double aY)
{
  if (!FloatValidate(aX, aY)) {
    return false;
  }

  EnsureTarget();
  RefPtr<gfx::Path> tempPath = aPath.GetPath(CanvasWindingRule::Nonzero, mTarget);

  const ContextState &state = CurrentState();

  StrokeOptions strokeOptions(state.lineWidth,
                              state.lineJoin,
                              state.lineCap,
                              state.miterLimit,
                              state.dash.Length(),
                              state.dash.Elements(),
                              state.dashOffset);

  return tempPath->StrokeContainsPoint(strokeOptions, Point(aX, aY), mTarget->GetTransform());
}

// Returns a surface that contains only the part needed to draw aSourceRect.
// On entry, aSourceRect is relative to aSurface, and on return aSourceRect is
// relative to the returned surface.
static already_AddRefed<SourceSurface>
ExtractSubrect(SourceSurface* aSurface, gfx::Rect* aSourceRect, DrawTarget* aTargetDT)
{
  gfx::Rect roundedOutSourceRect = *aSourceRect;
  roundedOutSourceRect.RoundOut();
  gfx::IntRect roundedOutSourceRectInt;
  if (!roundedOutSourceRect.ToIntRect(&roundedOutSourceRectInt)) {
    RefPtr<SourceSurface> surface(aSurface);
    return surface.forget();
  }

  RefPtr<DrawTarget> subrectDT =
    aTargetDT->CreateSimilarDrawTarget(roundedOutSourceRectInt.Size(), SurfaceFormat::B8G8R8A8);

  if (!subrectDT) {
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

static void
ClipImageDimension(double& aSourceCoord, double& aSourceSize, int32_t aImageSize,
                   double& aDestCoord, double& aDestSize)
{
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
CanvasRenderingContext2D::CachedSurfaceFromElement(Element* aElement)
{
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

  res.mSourceSurface =
    CanvasImageCache::LookupAllCanvas(aElement, mIsSkiaGL);
  if (!res.mSourceSurface) {
    return res;
  }

  int32_t corsmode = imgIRequest::CORS_NONE;
  if (NS_SUCCEEDED(imgRequest->GetCORSMode(&corsmode))) {
    res.mCORSUsed = corsmode != imgIRequest::CORS_NONE;
  }

  res.mSize = res.mSourceSurface->GetSize();
  res.mPrincipal = principal.forget();
  res.mIsWriteOnly = false;
  res.mImageRequest = imgRequest.forget();

  return res;
}

// drawImage(in HTMLImageElement image, in float dx, in float dy);
//   -- render image from 0,0 at dx,dy top-left coords
// drawImage(in HTMLImageElement image, in float dx, in float dy, in float dw, in float dh);
//   -- render image from 0,0 at dx,dy top-left coords clipping it to dw,dh
// drawImage(in HTMLImageElement image, in float sx, in float sy, in float sw, in float sh, in float dx, in float dy, in float dw, in float dh);
//   -- render the region defined by (sx,sy,sw,wh) in image-local space into the region (dx,dy,dw,dh) on the canvas

// If only dx and dy are passed in then optional_argc should be 0. If only
// dx, dy, dw and dh are passed in then optional_argc should be 2. The only
// other valid value for optional_argc is 6 if sx, sy, sw, sh, dx, dy, dw and dh
// are all passed in.

void
CanvasRenderingContext2D::DrawImage(const CanvasImageSource& aImage,
                                    double aSx, double aSy, double aSw,
                                    double aSh, double aDx, double aDy,
                                    double aDw, double aDh,
                                    uint8_t aOptional_argc,
                                    ErrorResult& aError)
{
  if (mDrawObserver) {
    mDrawObserver->DidDrawCall(CanvasDrawObserver::DrawCallType::DrawImage);
  }

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

  Element* element = nullptr;

  EnsureTarget();
  if (aImage.IsHTMLCanvasElement()) {
    HTMLCanvasElement* canvas = &aImage.GetAsHTMLCanvasElement();
    element = canvas;
    nsIntSize size = canvas->GetSize();
    if (size.width == 0 || size.height == 0) {
      aError.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
      return;
    }
  } else if (aImage.IsImageBitmap()) {
    ImageBitmap& imageBitmap = aImage.GetAsImageBitmap();
    srcSurf = imageBitmap.PrepareForDrawTarget(mTarget);

    if (!srcSurf) {
      return;
    }

    imgSize = gfx::IntSize(imageBitmap.Width(), imageBitmap.Height());
  }
  else {
    if (aImage.IsHTMLImageElement()) {
      HTMLImageElement* img = &aImage.GetAsHTMLImageElement();
      element = img;
    } else {
      HTMLVideoElement* video = &aImage.GetAsHTMLVideoElement();
      video->MarkAsContentSource(mozilla::dom::HTMLVideoElement::CallerAPI::DRAW_IMAGE);
      element = video;
    }

    srcSurf =
     CanvasImageCache::LookupCanvas(element, mCanvasElement, &imgSize, mIsSkiaGL);
  }

  nsLayoutUtils::DirectDrawInfo drawInfo;

#ifdef USE_SKIA_GPU
  if (mRenderingMode == RenderingMode::OpenGLBackendMode &&
      mIsSkiaGL &&
      !srcSurf &&
      aImage.IsHTMLVideoElement() &&
      gfxPlatform::GetPlatform()->UseAcceleratedCanvas()) {
    mozilla::gl::GLContext* gl = gfxPlatform::GetPlatform()->GetSkiaGLGlue()->GetGLContext();
    MOZ_ASSERT(gl);

    HTMLVideoElement* video = &aImage.GetAsHTMLVideoElement();
    if (!video) {
      return;
    }

#ifdef MOZ_EME
    if (video->ContainsRestrictedContent()) {
      aError.Throw(NS_ERROR_NOT_AVAILABLE);
      return;
    }
#endif

    uint16_t readyState;
    if (NS_SUCCEEDED(video->GetReadyState(&readyState)) &&
        readyState < nsIDOMHTMLMediaElement::HAVE_CURRENT_DATA) {
      // still loading, just return
      return;
    }

    // If it doesn't have a principal, just bail
    nsCOMPtr<nsIPrincipal> principal = video->GetCurrentVideoPrincipal();
    if (!principal) {
      aError.Throw(NS_ERROR_NOT_AVAILABLE);
      return;
    }

    mozilla::layers::ImageContainer* container = video->GetImageContainer();
    if (!container) {
      aError.Throw(NS_ERROR_NOT_AVAILABLE);
      return;
    }

    AutoLockImage lockImage(container);
    layers::Image* srcImage = lockImage.GetImage();
    if (!srcImage) {
      aError.Throw(NS_ERROR_NOT_AVAILABLE);
      return;
    }

    gl->MakeCurrent();
    GLuint videoTexture = 0;
    gl->fGenTextures(1, &videoTexture);
    // skiaGL expect upload on drawing, and uses texture 0 for texturing,
    // so we must active texture 0 and bind the texture for it.
    gl->fActiveTexture(LOCAL_GL_TEXTURE0);
    gl->fBindTexture(LOCAL_GL_TEXTURE_2D, videoTexture);

    gl->fTexImage2D(LOCAL_GL_TEXTURE_2D, 0, LOCAL_GL_RGB, srcImage->GetSize().width, srcImage->GetSize().height, 0, LOCAL_GL_RGB, LOCAL_GL_UNSIGNED_SHORT_5_6_5, nullptr);
    gl->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_WRAP_S, LOCAL_GL_CLAMP_TO_EDGE);
    gl->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_WRAP_T, LOCAL_GL_CLAMP_TO_EDGE);
    gl->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MAG_FILTER, LOCAL_GL_LINEAR);
    gl->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MIN_FILTER, LOCAL_GL_LINEAR);

    const gl::OriginPos destOrigin = gl::OriginPos::TopLeft;
    bool ok = gl->BlitHelper()->BlitImageToTexture(srcImage, srcImage->GetSize(),
                                                   videoTexture, LOCAL_GL_TEXTURE_2D,
                                                   destOrigin);
    if (ok) {
      NativeSurface texSurf;
      texSurf.mType = NativeSurfaceType::OPENGL_TEXTURE;
      texSurf.mFormat = SurfaceFormat::R5G6B5_UINT16;
      texSurf.mSize.width = srcImage->GetSize().width;
      texSurf.mSize.height = srcImage->GetSize().height;
      texSurf.mSurface = (void*)((uintptr_t)videoTexture);

      srcSurf = mTarget->CreateSourceSurfaceFromNativeSurface(texSurf);
      if (!srcSurf) {
        gl->fDeleteTextures(1, &videoTexture);
      }
      imgSize.width = srcImage->GetSize().width;
      imgSize.height = srcImage->GetSize().height;

      int32_t displayWidth = video->VideoWidth();
      int32_t displayHeight = video->VideoHeight();
      aSw *= (double)imgSize.width / (double)displayWidth;
      aSh *= (double)imgSize.height / (double)displayHeight;
    } else {
      gl->fDeleteTextures(1, &videoTexture);
    }
    srcImage = nullptr;

    if (mCanvasElement) {
      CanvasUtils::DoDrawImageSecurityCheck(mCanvasElement,
                                            principal, false,
                                            video->GetCORSMode() != CORS_NONE);
    }
  }
#endif
  if (!srcSurf) {
    // The canvas spec says that drawImage should draw the first frame
    // of animated images. We also don't want to rasterize vector images.
    uint32_t sfeFlags = nsLayoutUtils::SFE_WANT_FIRST_FRAME |
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

    // Scale sw/sh based on aspect ratio
    if (aImage.IsHTMLVideoElement()) {
      HTMLVideoElement* video = &aImage.GetAsHTMLVideoElement();
      int32_t displayWidth = video->VideoWidth();
      int32_t displayHeight = video->VideoHeight();
      aSw *= (double)imgSize.width / (double)displayWidth;
      aSh *= (double)imgSize.height / (double)displayHeight;
    }

    if (mCanvasElement) {
      CanvasUtils::DoDrawImageSecurityCheck(mCanvasElement,
                                            res.mPrincipal, res.mIsWriteOnly,
                                            res.mCORSUsed);
    }

    if (res.mSourceSurface) {
      if (res.mImageRequest) {
        CanvasImageCache::NotifyDrawImage(element, mCanvasElement, res.mSourceSurface, imgSize, mIsSkiaGL);
      }
      srcSurf = res.mSourceSurface;
    } else {
      drawInfo = res.mDrawInfo;
    }
  }

  if (aOptional_argc == 0) {
    aSx = aSy = 0.0;
    aDw = aSw = (double) imgSize.width;
    aDh = aSh = (double) imgSize.height;
  } else if (aOptional_argc == 2) {
    aSx = aSy = 0.0;
    aSw = (double) imgSize.width;
    aSh = (double) imgSize.height;
  }

  if (aSw == 0.0 || aSh == 0.0) {
    aError.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return;
  }

  ClipImageDimension(aSx, aSw, imgSize.width, aDx, aDw);
  ClipImageDimension(aSy, aSh, imgSize.height, aDy, aDh);

  if (aSw <= 0.0 || aSh <= 0.0 ||
      aDw <= 0.0 || aDh <= 0.0) {
    // source and/or destination are fully clipped, so nothing is painted
    return;
  }

  SamplingFilter samplingFilter;
  AntialiasMode antialiasMode;

  if (CurrentState().imageSmoothingEnabled) {
    samplingFilter = gfx::SamplingFilter::LINEAR;
    antialiasMode = AntialiasMode::DEFAULT;
  } else {
    samplingFilter = gfx::SamplingFilter::POINT;
    antialiasMode = AntialiasMode::NONE;
  }

  gfx::Rect bounds;

  if (NeedToCalculateBounds()) {
    bounds = gfx::Rect(aDx, aDy, aDw, aDh);
    bounds = mTarget->GetTransform().TransformBounds(bounds);
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
    AdjustedTarget(this, bounds.IsEmpty() ? nullptr : &bounds)->
      DrawSurface(srcSurf,
                  gfx::Rect(aDx, aDy, aDw, aDh),
                  sourceRect,
                  DrawSurfaceOptions(samplingFilter, SamplingBounds::UNBOUNDED),
                  DrawOptions(CurrentState().globalAlpha, UsedOperation(), antialiasMode));
  } else {
    DrawDirectlyToCanvas(drawInfo, &bounds,
                         gfx::Rect(aDx, aDy, aDw, aDh),
                         gfx::Rect(aSx, aSy, aSw, aSh),
                         imgSize);
  }

  RedrawUser(gfxRect(aDx, aDy, aDw, aDh));
}

void
CanvasRenderingContext2D::DrawDirectlyToCanvas(
                          const nsLayoutUtils::DirectDrawInfo& aImage,
                          gfx::Rect* aBounds,
                          gfx::Rect aDest,
                          gfx::Rect aSrc,
                          gfx::IntSize aImgSize)
{
  MOZ_ASSERT(aSrc.width > 0 && aSrc.height > 0,
             "Need positive source width and height");

  gfxMatrix contextMatrix;
  AdjustedTarget tempTarget(this, aBounds->IsEmpty() ? nullptr: aBounds);

  // Get any existing transforms on the context, including transformations used
  // for context shadow.
  if (tempTarget) {
    Matrix matrix = tempTarget->GetTransform();
    contextMatrix = gfxMatrix(matrix._11, matrix._12, matrix._21,
                              matrix._22, matrix._31, matrix._32);
  }
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
  context->SetMatrix(contextMatrix.
                       Scale(1.0 / contextScale.width,
                             1.0 / contextScale.height).
                       Translate(aDest.x - aSrc.x, aDest.y - aSrc.y));

  // FLAG_CLAMP is added for increased performance, since we never tile here.
  uint32_t modifiedFlags = aImage.mDrawingFlags | imgIContainer::FLAG_CLAMP;

  CSSIntSize sz(scaledImageSize.width, scaledImageSize.height); // XXX hmm is scaledImageSize really in CSS pixels?
  SVGImageContext svgContext(sz, Nothing(), CurrentState().globalAlpha);

  auto result = aImage.mImgContainer->
    Draw(context, scaledImageSize,
         ImageRegion::Create(gfxRect(aSrc.x, aSrc.y, aSrc.width, aSrc.height)),
         aImage.mWhichFrame, SamplingFilter::GOOD, Some(svgContext), modifiedFlags);

  if (result != DrawResult::SUCCESS) {
    NS_WARNING("imgIContainer::Draw failed");
  }
}

void
CanvasRenderingContext2D::SetGlobalCompositeOperation(const nsAString& aOp,
                                                      ErrorResult& aError)
{
  CompositionOp comp_op;

#define CANVAS_OP_TO_GFX_OP(cvsop, op2d) \
  if (aOp.EqualsLiteral(cvsop))   \
    comp_op = CompositionOp::OP_##op2d;

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

void
CanvasRenderingContext2D::GetGlobalCompositeOperation(nsAString& aOp,
                                                      ErrorResult& aError)
{
  CompositionOp comp_op = CurrentState().op;

#define CANVAS_OP_TO_GFX_OP(cvsop, op2d) \
  if (comp_op == CompositionOp::OP_##op2d) \
    aOp.AssignLiteral(cvsop);

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

void
CanvasRenderingContext2D::DrawWindow(nsGlobalWindow& aWindow, double aX,
                                     double aY, double aW, double aH,
                                     const nsAString& aBgColor,
                                     uint32_t aFlags, ErrorResult& aError)
{
  MOZ_ASSERT(aWindow.IsInnerWindow());

  if (int32_t(aW) == 0 || int32_t(aH) == 0) {
    return;
  }

  // protect against too-large surfaces that will cause allocation
  // or overflow issues
  if (!Factory::CheckSurfaceSize(IntSize(int32_t(aW), int32_t(aH)), 0xffff)) {
    aError.Throw(NS_ERROR_FAILURE);
    return;
  }

  CompositionOp op = UsedOperation();
  bool discardContent = GlobalAlpha() == 1.0f
    && (op == CompositionOp::OP_OVER || op == CompositionOp::OP_SOURCE);
  const gfx::Rect drawRect(aX, aY, aW, aH);
  EnsureTarget(discardContent ? &drawRect : nullptr);

  // We can't allow web apps to call this until we fix at least the
  // following potential security issues:
  // -- rendering cross-domain IFRAMEs and then extracting the results
  // -- rendering the user's theme and then extracting the results
  // -- rendering native anonymous content (e.g., file input paths;
  // scrollbars should be allowed)
  if (!nsContentUtils::IsCallerChrome()) {
    // not permitted to use DrawWindow
    // XXX ERRMSG we need to report an error to developers here! (bug 329026)
    aError.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  // Flush layout updates
  if (!(aFlags & nsIDOMCanvasRenderingContext2D::DRAWWINDOW_DO_NOT_FLUSH)) {
    nsContentUtils::FlushLayoutForTree(aWindow.AsInner()->GetOuterWindow());
  }

  RefPtr<nsPresContext> presContext;
  nsIDocShell* docshell = aWindow.GetDocShell();
  if (docshell) {
    docshell->GetPresContext(getter_AddRefs(presContext));
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
  uint32_t renderDocFlags = (nsIPresShell::RENDER_IGNORE_VIEWPORT_SCROLLING |
                             nsIPresShell::RENDER_DOCUMENT_RELATIVE);
  if (aFlags & nsIDOMCanvasRenderingContext2D::DRAWWINDOW_DRAW_CARET) {
    renderDocFlags |= nsIPresShell::RENDER_CARET;
  }
  if (aFlags & nsIDOMCanvasRenderingContext2D::DRAWWINDOW_DRAW_VIEW) {
    renderDocFlags &= ~(nsIPresShell::RENDER_IGNORE_VIEWPORT_SCROLLING |
                        nsIPresShell::RENDER_DOCUMENT_RELATIVE);
  }
  if (aFlags & nsIDOMCanvasRenderingContext2D::DRAWWINDOW_USE_WIDGET_LAYERS) {
    renderDocFlags |= nsIPresShell::RENDER_USE_WIDGET_LAYERS;
  }
  if (aFlags & nsIDOMCanvasRenderingContext2D::DRAWWINDOW_ASYNC_DECODE_IMAGES) {
    renderDocFlags |= nsIPresShell::RENDER_ASYNC_DECODE_IMAGES;
  }
  if (aFlags & nsIDOMCanvasRenderingContext2D::DRAWWINDOW_DO_NOT_FLUSH) {
    renderDocFlags |= nsIPresShell::RENDER_DRAWWINDOW_NOT_FLUSHING;
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
  if (gfxPlatform::GetPlatform()->SupportsAzureContentForDrawTarget(mTarget) &&
      GlobalAlpha() == 1.0f &&
      UsedOperation() == CompositionOp::OP_OVER &&
      (!mBufferProvider || mBufferProvider->GetType() != LayersBackend::LAYERS_CLIENT))
  {
    thebes = gfxContext::CreateOrNull(mTarget);
    MOZ_ASSERT(thebes); // already checked the draw target above
                        // (in SupportsAzureContentForDrawTarget)
    thebes->SetMatrix(gfxMatrix(matrix._11, matrix._12, matrix._21,
                                matrix._22, matrix._31, matrix._32));
  } else {
    IntSize dtSize = IntSize::Ceil(sw, sh);
    if (!Factory::AllowedSurfaceSize(dtSize)) {
      aError.Throw(NS_ERROR_FAILURE);
      return;
    }
    drawDT =
      gfxPlatform::GetPlatform()->CreateOffscreenContentDrawTarget(dtSize,
                                                                   SurfaceFormat::B8G8R8A8);
    if (!drawDT || !drawDT->IsValid()) {
      aError.Throw(NS_ERROR_FAILURE);
      return;
    }

    thebes = gfxContext::CreateOrNull(drawDT);
    MOZ_ASSERT(thebes); // alrady checked the draw target above
    thebes->SetMatrix(gfxMatrix::Scaling(matrix._11, matrix._22));
  }

  nsCOMPtr<nsIPresShell> shell = presContext->PresShell();

  Unused << shell->RenderDocument(r, renderDocFlags, backgroundColor, thebes);
  // If this canvas was contained in the drawn window, the pre-transaction callback
  // may have returned its DT. If so, we must reacquire it here.
  EnsureTarget(discardContent ? &drawRect : nullptr);

  if (drawDT) {
    RefPtr<SourceSurface> snapshot = drawDT->Snapshot();
    if (NS_WARN_IF(!snapshot)) {
      aError.Throw(NS_ERROR_FAILURE);
      return;
    }
    RefPtr<DataSourceSurface> data = snapshot->GetDataSurface();
    if (!data || !Factory::AllowedSurfaceSize(data->GetSize())) {
      gfxCriticalError() << "Unexpected invalid data source surface " <<
        (data ? data->GetSize() : IntSize(0,0));
      aError.Throw(NS_ERROR_FAILURE);
      return;
    }

    DataSourceSurface::MappedSurface rawData;
    if (NS_WARN_IF(!data->Map(DataSourceSurface::READ, &rawData))) {
        aError.Throw(NS_ERROR_FAILURE);
        return;
    }
    RefPtr<SourceSurface> source =
      mTarget->CreateSourceSurfaceFromData(rawData.mData,
                                           data->GetSize(),
                                           rawData.mStride,
                                           data->GetFormat());
    data->Unmap();

    if (!source) {
      aError.Throw(NS_ERROR_FAILURE);
      return;
    }

    gfx::Rect destRect(0, 0, aW, aH);
    gfx::Rect sourceRect(0, 0, sw, sh);
    mTarget->DrawSurface(source, destRect, sourceRect,
                         DrawSurfaceOptions(gfx::SamplingFilter::POINT),
                         DrawOptions(GlobalAlpha(), UsedOperation(),
                                     AntialiasMode::NONE));
    mTarget->Flush();
  } else {
    mTarget->SetTransform(matrix);
  }

  // note that x and y are coordinates in the document that
  // we're drawing; x and y are drawn to 0,0 in current user
  // space.
  RedrawUser(gfxRect(0, 0, aW, aH));
}

void
CanvasRenderingContext2D::AsyncDrawXULElement(nsXULElement& aElem,
                                              double aX, double aY,
                                              double aW, double aH,
                                              const nsAString& aBgColor,
                                              uint32_t aFlags,
                                              ErrorResult& aError)
{
  // We can't allow web apps to call this until we fix at least the
  // following potential security issues:
  // -- rendering cross-domain IFRAMEs and then extracting the results
  // -- rendering the user's theme and then extracting the results
  // -- rendering native anonymous content (e.g., file input paths;
  // scrollbars should be allowed)
  if (!nsContentUtils::IsCallerChrome()) {
    // not permitted to use DrawWindow
    // XXX ERRMSG we need to report an error to developers here! (bug 329026)
    aError.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

#if 0
  nsCOMPtr<nsIFrameLoaderOwner> loaderOwner = do_QueryInterface(&elem);
  if (!loaderOwner) {
    aError.Throw(NS_ERROR_FAILURE);
    return;
  }

  RefPtr<nsFrameLoader> frameloader = loaderOwner->GetFrameLoader();
  if (!frameloader) {
    aError.Throw(NS_ERROR_FAILURE);
    return;
  }

  PBrowserParent *child = frameloader->GetRemoteBrowser();
  if (!child) {
    nsIDocShell* docShell = frameLoader->GetExistingDocShell();
    if (!docShell) {
      aError.Throw(NS_ERROR_FAILURE);
      return;
    }

    nsCOMPtr<nsPIDOMWindowOuter> window = docShell->GetWindow();
    if (!window) {
      aError.Throw(NS_ERROR_FAILURE);
      return;
    }

    return DrawWindow(window->GetCurrentInnerWindow(), aX, aY, aW, aH,
                      aBgColor, aFlags);
  }

  // protect against too-large surfaces that will cause allocation
  // or overflow issues
  if (!Factory::CheckSurfaceSize(IntSize(aW, aH), 0xffff)) {
    aError.Throw(NS_ERROR_FAILURE);
    return;
  }

  bool flush =
    (aFlags & nsIDOMCanvasRenderingContext2D::DRAWWINDOW_DO_NOT_FLUSH) == 0;

  uint32_t renderDocFlags = nsIPresShell::RENDER_IGNORE_VIEWPORT_SCROLLING;
  if (aFlags & nsIDOMCanvasRenderingContext2D::DRAWWINDOW_DRAW_CARET) {
    renderDocFlags |= nsIPresShell::RENDER_CARET;
  }
  if (aFlags & nsIDOMCanvasRenderingContext2D::DRAWWINDOW_DRAW_VIEW) {
    renderDocFlags &= ~nsIPresShell::RENDER_IGNORE_VIEWPORT_SCROLLING;
  }

  nsRect rect(nsPresContext::CSSPixelsToAppUnits(aX),
              nsPresContext::CSSPixelsToAppUnits(aY),
              nsPresContext::CSSPixelsToAppUnits(aW),
              nsPresContext::CSSPixelsToAppUnits(aH));
  if (mIPC) {
    PDocumentRendererParent *pdocrender =
      child->SendPDocumentRendererConstructor(rect,
                                              mThebes->CurrentMatrix(),
                                              nsString(aBGColor),
                                              renderDocFlags, flush,
                                              nsIntSize(mWidth, mHeight));
    if (!pdocrender)
      return NS_ERROR_FAILURE;

    DocumentRendererParent *docrender =
      static_cast<DocumentRendererParent *>(pdocrender);

    docrender->SetCanvasContext(this, mThebes);
  }
#endif
}

//
// device pixel getting/setting
//

already_AddRefed<ImageData>
CanvasRenderingContext2D::GetImageData(JSContext* aCx, double aSx,
                                       double aSy, double aSw,
                                       double aSh, ErrorResult& aError)
{
  if (mDrawObserver) {
    mDrawObserver->DidDrawCall(CanvasDrawObserver::DrawCallType::GetImageData);
  }

  if (!mCanvasElement && !mDocShell) {
    NS_ERROR("No canvas element and no docshell in GetImageData!!!");
    aError.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return nullptr;
  }

  // Check only if we have a canvas element; if we were created with a docshell,
  // then it's special internal use.
  if (mCanvasElement && mCanvasElement->IsWriteOnly() &&
      !nsContentUtils::IsCallerChrome())
  {
    // XXX ERRMSG we need to report an error to developers here! (bug 329026)
    aError.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return nullptr;
  }

  if (!IsFinite(aSx) || !IsFinite(aSy) ||
      !IsFinite(aSw) || !IsFinite(aSh)) {
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
  aError = GetImageDataArray(aCx, x, y, w, h, array.address());
  if (aError.Failed()) {
    return nullptr;
  }
  MOZ_ASSERT(array);

  RefPtr<ImageData> imageData = new ImageData(w, h, *array);
  return imageData.forget();
}

nsresult
CanvasRenderingContext2D::GetImageDataArray(JSContext* aCx,
                                            int32_t aX,
                                            int32_t aY,
                                            uint32_t aWidth,
                                            uint32_t aHeight,
                                            JSObject** aRetval)
{
  if (mDrawObserver) {
    mDrawObserver->DidDrawCall(CanvasDrawObserver::DrawCallType::GetImageData);
  }

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
  RefPtr<DataSourceSurface> readback;
  DataSourceSurface::MappedSurface rawData;
  if (!srcReadRect.IsEmpty()) {
    RefPtr<SourceSurface> snapshot;
    if (!mTarget && mBufferProvider) {
      snapshot = mBufferProvider->BorrowSnapshot();
    } else {
      EnsureTarget();
      if (!IsTargetValid()) {
        return NS_ERROR_FAILURE;
      }
      snapshot = mTarget->Snapshot();
    }

    if (snapshot) {
      readback = snapshot->GetDataSurface();
    }

    if (!mTarget && mBufferProvider) {
      mBufferProvider->ReturnSnapshot(snapshot.forget());
    }

    if (!readback || !readback->Map(DataSourceSurface::READ, &rawData)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  IntRect dstWriteRect = srcReadRect;
  dstWriteRect.MoveBy(-aX, -aY);

  JS::AutoCheckCannotGC nogc;
  bool isShared;
  uint8_t* data = JS_GetUint8ClampedArrayData(darray, &isShared, nogc);
  MOZ_ASSERT(!isShared);        // Should not happen, data was created above

  uint8_t* src;
  uint32_t srcStride;
  if (readback) {
    srcStride = rawData.mStride;
    src = rawData.mData + srcReadRect.y * srcStride + srcReadRect.x * 4;
  } else {
    src = data;
    srcStride = aWidth * 4;
  }

  uint8_t* dst = data + dstWriteRect.y * (aWidth * 4) + dstWriteRect.x * 4;

  if (mOpaque) {
    for (int32_t j = 0; j < dstWriteRect.height; ++j) {
      for (int32_t i = 0; i < dstWriteRect.width; ++i) {
        // XXX Is there some useful swizzle MMX we can use here?
#if MOZ_LITTLE_ENDIAN
        uint8_t b = *src++;
        uint8_t g = *src++;
        uint8_t r = *src++;
        src++;
#else
        src++;
        uint8_t r = *src++;
        uint8_t g = *src++;
        uint8_t b = *src++;
#endif
        *dst++ = r;
        *dst++ = g;
        *dst++ = b;
        *dst++ = 255;
      }
      src += srcStride - (dstWriteRect.width * 4);
      dst += (aWidth * 4) - (dstWriteRect.width * 4);
    }
  } else
  for (int32_t j = 0; j < dstWriteRect.height; ++j) {
    for (int32_t i = 0; i < dstWriteRect.width; ++i) {
      // XXX Is there some useful swizzle MMX we can use here?
#if MOZ_LITTLE_ENDIAN
      uint8_t b = *src++;
      uint8_t g = *src++;
      uint8_t r = *src++;
      uint8_t a = *src++;
#else
      uint8_t a = *src++;
      uint8_t r = *src++;
      uint8_t g = *src++;
      uint8_t b = *src++;
#endif
      // Convert to non-premultiplied color
      *dst++ = gfxUtils::sUnpremultiplyTable[a * 256 + r];
      *dst++ = gfxUtils::sUnpremultiplyTable[a * 256 + g];
      *dst++ = gfxUtils::sUnpremultiplyTable[a * 256 + b];
      *dst++ = a;
    }
    src += srcStride - (dstWriteRect.width * 4);
    dst += (aWidth * 4) - (dstWriteRect.width * 4);
  }

  if (readback) {
    readback->Unmap();
  }

  *aRetval = darray;
  return NS_OK;
}

void
CanvasRenderingContext2D::EnsureErrorTarget()
{
  if (sErrorTarget) {
    return;
  }

  RefPtr<DrawTarget> errorTarget = gfxPlatform::GetPlatform()->CreateOffscreenCanvasDrawTarget(IntSize(1, 1), SurfaceFormat::B8G8R8A8);
  MOZ_ASSERT(errorTarget, "Failed to allocate the error target!");

  sErrorTarget = errorTarget;
  NS_ADDREF(sErrorTarget);
}

void
CanvasRenderingContext2D::FillRuleChanged()
{
  if (mPath) {
    mPathBuilder = mPath->CopyToBuilder(CurrentState().fillRule);
    mPath = nullptr;
  }
}

void
CanvasRenderingContext2D::PutImageData(ImageData& aImageData, double aDx,
                                       double aDy, ErrorResult& aError)
{
  RootedTypedArray<Uint8ClampedArray> arr(RootingCx());
  DebugOnly<bool> inited = arr.Init(aImageData.GetDataObject());
  MOZ_ASSERT(inited);

  aError = PutImageData_explicit(JS::ToInt32(aDx), JS::ToInt32(aDy),
                                aImageData.Width(), aImageData.Height(),
                                &arr, false, 0, 0, 0, 0);
}

void
CanvasRenderingContext2D::PutImageData(ImageData& aImageData, double aDx,
                                       double aDy, double aDirtyX,
                                       double aDirtyY, double aDirtyWidth,
                                       double aDirtyHeight,
                                       ErrorResult& aError)
{
  RootedTypedArray<Uint8ClampedArray> arr(RootingCx());
  DebugOnly<bool> inited = arr.Init(aImageData.GetDataObject());
  MOZ_ASSERT(inited);

  aError = PutImageData_explicit(JS::ToInt32(aDx), JS::ToInt32(aDy),
                                aImageData.Width(), aImageData.Height(),
                                &arr, true,
                                JS::ToInt32(aDirtyX),
                                JS::ToInt32(aDirtyY),
                                JS::ToInt32(aDirtyWidth),
                                JS::ToInt32(aDirtyHeight));
}

nsresult
CanvasRenderingContext2D::PutImageData_explicit(int32_t aX, int32_t aY, uint32_t aW, uint32_t aH,
                                                dom::Uint8ClampedArray* aArray,
                                                bool aHasDirtyRect, int32_t aDirtyX, int32_t aDirtyY,
                                                int32_t aDirtyWidth, int32_t aDirtyHeight)
{
  if (mDrawObserver) {
    mDrawObserver->DidDrawCall(CanvasDrawObserver::DrawCallType::PutImageData);
  }

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

      if (!checkedDirtyX.isValid())
        return NS_ERROR_DOM_INDEX_SIZE_ERR;

      aDirtyX = checkedDirtyX.value();
      aDirtyWidth = -aDirtyWidth;
    }

    if (aDirtyHeight < 0) {
      NS_ENSURE_TRUE(aDirtyHeight != INT_MIN, NS_ERROR_DOM_INDEX_SIZE_ERR);

      CheckedInt32 checkedDirtyY = CheckedInt32(aDirtyY) + aDirtyHeight;

      if (!checkedDirtyY.isValid())
        return NS_ERROR_DOM_INDEX_SIZE_ERR;

      aDirtyY = checkedDirtyY.value();
      aDirtyHeight = -aDirtyHeight;
    }

    // bound the dirty rect within the imageData rectangle
    dirtyRect = imageDataRect.Intersect(IntRect(aDirtyX, aDirtyY, aDirtyWidth, aDirtyHeight));

    if (dirtyRect.Width() <= 0 || dirtyRect.Height() <= 0)
      return NS_OK;
  } else {
    dirtyRect = imageDataRect;
  }

  dirtyRect.MoveBy(IntPoint(aX, aY));
  dirtyRect = IntRect(0, 0, mWidth, mHeight).Intersect(dirtyRect);

  if (dirtyRect.Width() <= 0 || dirtyRect.Height() <= 0) {
    return NS_OK;
  }

  aArray->ComputeLengthAndData();

  uint32_t dataLen = aArray->Length();

  uint32_t len = aW * aH * 4;
  if (dataLen != len) {
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }

  uint32_t copyWidth = dirtyRect.Width();
  uint32_t copyHeight = dirtyRect.Height();
  RefPtr<gfxImageSurface> imgsurf = new gfxImageSurface(gfx::IntSize(copyWidth, copyHeight),
                                                          SurfaceFormat::A8R8G8B8_UINT32,
                                                          false);
  if (!imgsurf || imgsurf->CairoStatus()) {
    return NS_ERROR_FAILURE;
  }

  uint32_t copyX = dirtyRect.x - aX;
  uint32_t copyY = dirtyRect.y - aY;
  //uint8_t *src = aArray->Data();
  uint8_t *dst = imgsurf->Data();
  uint8_t* srcLine = aArray->Data() + copyY * (aW * 4) + copyX * 4;
#if 0
  printf("PutImageData_explicit: dirty x=%d y=%d w=%d h=%d copy x=%d y=%d w=%d h=%d ext x=%d y=%d w=%d h=%d\n",
       dirtyRect.x, dirtyRect.y, copyWidth, copyHeight,
       copyX, copyY, copyWidth, copyHeight,
       x, y, w, h);
#endif
  for (uint32_t j = 0; j < copyHeight; j++) {
    uint8_t *src = srcLine;
    for (uint32_t i = 0; i < copyWidth; i++) {
      uint8_t r = *src++;
      uint8_t g = *src++;
      uint8_t b = *src++;
      uint8_t a = *src++;

      // Convert to premultiplied color (losslessly if the input came from getImageData)
#if MOZ_LITTLE_ENDIAN
      *dst++ = gfxUtils::sPremultiplyTable[a * 256 + b];
      *dst++ = gfxUtils::sPremultiplyTable[a * 256 + g];
      *dst++ = gfxUtils::sPremultiplyTable[a * 256 + r];
      *dst++ = a;
#else
      *dst++ = a;
      *dst++ = gfxUtils::sPremultiplyTable[a * 256 + r];
      *dst++ = gfxUtils::sPremultiplyTable[a * 256 + g];
      *dst++ = gfxUtils::sPremultiplyTable[a * 256 + b];
#endif
    }
    srcLine += aW * 4;
  }

  // The canvas spec says that the current path, transformation matrix, shadow attributes,
  // global alpha, the clipping region, and global composition operator must not affect the
  // getImageData() and putImageData() methods.
  const gfx::Rect putRect(dirtyRect);
  EnsureTarget(&putRect);

  if (!IsTargetValid()) {
    return NS_ERROR_FAILURE;
  }

  RefPtr<SourceSurface> sourceSurface =
    mTarget->CreateSourceSurfaceFromData(imgsurf->Data(), IntSize(copyWidth, copyHeight), imgsurf->Stride(), SurfaceFormat::B8G8R8A8);

  // In certain scenarios, requesting larger than 8k image fails.  Bug 803568
  // covers the details of how to run into it, but the full detailed
  // investigation hasn't been done to determine the underlying cause.  We
  // will just handle the failure to allocate the surface to avoid a crash.
  if (!sourceSurface) {
    return NS_ERROR_FAILURE;
  }

  mTarget->CopySurface(sourceSurface,
                       IntRect(0, 0,
                               dirtyRect.width, dirtyRect.height),
                       IntPoint(dirtyRect.x, dirtyRect.y));

  Redraw(gfx::Rect(dirtyRect.x, dirtyRect.y, dirtyRect.width, dirtyRect.height));

  return NS_OK;
}

static already_AddRefed<ImageData>
CreateImageData(JSContext* aCx, CanvasRenderingContext2D* aContext,
                uint32_t aW, uint32_t aH, ErrorResult& aError)
{
  if (aW == 0)
      aW = 1;
  if (aH == 0)
      aH = 1;

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

already_AddRefed<ImageData>
CanvasRenderingContext2D::CreateImageData(JSContext* aCx, double aSw,
                                          double aSh, ErrorResult& aError)
{
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

already_AddRefed<ImageData>
CanvasRenderingContext2D::CreateImageData(JSContext* aCx,
                                          ImageData& aImagedata,
                                          ErrorResult& aError)
{
  return mozilla::dom::CreateImageData(aCx, this, aImagedata.Width(),
                                       aImagedata.Height(), aError);
}

static uint8_t g2DContextLayerUserData;


uint32_t
CanvasRenderingContext2D::SkiaGLTex() const
{
  if (!mTarget) {
    return 0;
  }
  MOZ_ASSERT(IsTargetValid());
  return (uint32_t)(uintptr_t)mTarget->GetNativeSurface(NativeSurfaceType::OPENGL_TEXTURE);
}

void CanvasRenderingContext2D::RemoveDrawObserver()
{
  if (mDrawObserver) {
    delete mDrawObserver;
    mDrawObserver = nullptr;
  }
}

already_AddRefed<Layer>
CanvasRenderingContext2D::GetCanvasLayer(nsDisplayListBuilder* aBuilder,
                                         Layer *aOldLayer,
                                         LayerManager *aManager,
                                         bool aMirror /* = false */)
{
  if (aMirror) {
    // Not supported for CanvasRenderingContext2D
    return nullptr;
  }

  if (mOpaque || mIsSkiaGL) {
    // If we're opaque then make sure we have a surface so we paint black
    // instead of transparent.
    // If we're using SkiaGL, then SkiaGLTex() below needs the target to
    // be accessible.
    EnsureTarget();
  }

  // Don't call EnsureTarget() ... if there isn't already a surface, then
  // we have nothing to paint and there is no need to create a surface just
  // to paint nothing. Also, EnsureTarget() can cause creation of a persistent
  // layer manager which must NOT happen during a paint.
  if ((!mBufferProvider && !mTarget) || !IsTargetValid()) {
    // No DidTransactionCallback will be received, so mark the context clean
    // now so future invalidations will be dispatched.
    MarkContextClean();
    return nullptr;
  }

  if (!mResetLayer && aOldLayer) {
    CanvasRenderingContext2DUserData* userData =
      static_cast<CanvasRenderingContext2DUserData*>(
        aOldLayer->GetUserData(&g2DContextLayerUserData));

    CanvasLayer::Data data;

    if (mIsSkiaGL) {
      GLuint skiaGLTex = SkiaGLTex();
      if (skiaGLTex) {
        SkiaGLGlue* glue = gfxPlatform::GetPlatform()->GetSkiaGLGlue();
        MOZ_ASSERT(glue);
        data.mGLContext = glue->GetGLContext();
        data.mFrontbufferGLTex = skiaGLTex;
      }
    }

    data.mBufferProvider = mBufferProvider;

    if (userData &&
        userData->IsForContext(this) &&
        static_cast<CanvasLayer*>(aOldLayer)->IsDataValid(data)) {
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
  CanvasRenderingContext2DUserData *userData = nullptr;
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
  canvasLayer->SetDidTransactionCallback(
          CanvasRenderingContext2DUserData::DidTransactionCallback, userData);
  canvasLayer->SetUserData(&g2DContextLayerUserData, userData);

  CanvasLayer::Data data;
  data.mSize = GetSize();
  data.mHasAlpha = !mOpaque;

  canvasLayer->SetPreTransactionCallback(
          CanvasRenderingContext2DUserData::PreTransactionCallback, userData);


  if (mIsSkiaGL) {
      GLuint skiaGLTex = SkiaGLTex();
      if (skiaGLTex) {
        SkiaGLGlue* glue = gfxPlatform::GetPlatform()->GetSkiaGLGlue();
        MOZ_ASSERT(glue);
        data.mGLContext = glue->GetGLContext();
        data.mFrontbufferGLTex = skiaGLTex;
      }
  }

  data.mBufferProvider = mBufferProvider;

  canvasLayer->Initialize(data);
  uint32_t flags = mOpaque ? Layer::CONTENT_OPAQUE : 0;
  canvasLayer->SetContentFlags(flags);
  canvasLayer->Updated();

  mResetLayer = false;

  return canvasLayer.forget();
}

void
CanvasRenderingContext2D::MarkContextClean()
{
  if (mInvalidateCount > 0) {
    mPredictManyRedrawCalls = mInvalidateCount > kCanvasMaxInvalidateCount;
  }
  mIsEntireFrameInvalid = false;
  mInvalidateCount = 0;
}

void
CanvasRenderingContext2D::MarkContextCleanForFrameCapture()
{
  mIsCapturedFrameInvalid = false;
}

bool
CanvasRenderingContext2D::IsContextCleanForFrameCapture()
{
  return !mIsCapturedFrameInvalid;
}

bool
CanvasRenderingContext2D::ShouldForceInactiveLayer(LayerManager* aManager)
{
  return !aManager->CanUseCanvasLayerForSize(GetSize());
}

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(CanvasPath, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(CanvasPath, Release)

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(CanvasPath, mParent)

CanvasPath::CanvasPath(nsISupports* aParent)
  : mParent(aParent)
{
  mPathBuilder = gfxPlatform::GetPlatform()->ScreenReferenceDrawTarget()->CreatePathBuilder();
}

CanvasPath::CanvasPath(nsISupports* aParent, already_AddRefed<PathBuilder> aPathBuilder)
  : mParent(aParent), mPathBuilder(aPathBuilder)
{
  if (!mPathBuilder) {
    mPathBuilder = gfxPlatform::GetPlatform()->ScreenReferenceDrawTarget()->CreatePathBuilder();
  }
}

JSObject*
CanvasPath::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return Path2DBinding::Wrap(aCx, this, aGivenProto);
}

already_AddRefed<CanvasPath>
CanvasPath::Constructor(const GlobalObject& aGlobal, ErrorResult& aRv)
{
  RefPtr<CanvasPath> path = new CanvasPath(aGlobal.GetAsSupports());
  return path.forget();
}

already_AddRefed<CanvasPath>
CanvasPath::Constructor(const GlobalObject& aGlobal, CanvasPath& aCanvasPath, ErrorResult& aRv)
{
  RefPtr<gfx::Path> tempPath = aCanvasPath.GetPath(CanvasWindingRule::Nonzero,
                                                   gfxPlatform::GetPlatform()->ScreenReferenceDrawTarget());

  RefPtr<CanvasPath> path = new CanvasPath(aGlobal.GetAsSupports(), tempPath->CopyToBuilder());
  return path.forget();
}

already_AddRefed<CanvasPath>
CanvasPath::Constructor(const GlobalObject& aGlobal, const nsAString& aPathString, ErrorResult& aRv)
{
  RefPtr<gfx::Path> tempPath = SVGContentUtils::GetPath(aPathString);
  if (!tempPath) {
    return Constructor(aGlobal, aRv);
  }

  RefPtr<CanvasPath> path = new CanvasPath(aGlobal.GetAsSupports(), tempPath->CopyToBuilder());
  return path.forget();
}

void
CanvasPath::ClosePath()
{
  EnsurePathBuilder();

  mPathBuilder->Close();
}

void
CanvasPath::MoveTo(double aX, double aY)
{
  EnsurePathBuilder();

  mPathBuilder->MoveTo(Point(ToFloat(aX), ToFloat(aY)));
}

void
CanvasPath::LineTo(double aX, double aY)
{
  EnsurePathBuilder();

  mPathBuilder->LineTo(Point(ToFloat(aX), ToFloat(aY)));
}

void
CanvasPath::QuadraticCurveTo(double aCpx, double aCpy, double aX, double aY)
{
  EnsurePathBuilder();

  mPathBuilder->QuadraticBezierTo(gfx::Point(ToFloat(aCpx), ToFloat(aCpy)),
                                  gfx::Point(ToFloat(aX), ToFloat(aY)));
}

void
CanvasPath::BezierCurveTo(double aCp1x, double aCp1y,
                          double aCp2x, double aCp2y,
                          double aX, double aY)
{
  BezierTo(gfx::Point(ToFloat(aCp1x), ToFloat(aCp1y)),
             gfx::Point(ToFloat(aCp2x), ToFloat(aCp2y)),
             gfx::Point(ToFloat(aX), ToFloat(aY)));
}

void
CanvasPath::ArcTo(double aX1, double aY1, double aX2, double aY2, double aRadius,
                  ErrorResult& aError)
{
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
  double dir, a2, b2, c2, cosx, sinx, d, anx, any,
         bnx, bny, x3, y3, x4, y4, cx, cy, angle0, angle1;
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
  a2 = (p0.x-aX1)*(p0.x-aX1) + (p0.y-aY1)*(p0.y-aY1);
  b2 = (aX1-aX2)*(aX1-aX2) + (aY1-aY2)*(aY1-aY2);
  c2 = (p0.x-aX2)*(p0.x-aX2) + (p0.y-aY2)*(p0.y-aY2);
  cosx = (a2+b2-c2)/(2*sqrt(a2*b2));

  sinx = sqrt(1 - cosx*cosx);
  d = aRadius / ((1 - cosx) / sinx);

  anx = (aX1-p0.x) / sqrt(a2);
  any = (aY1-p0.y) / sqrt(a2);
  bnx = (aX1-aX2) / sqrt(b2);
  bny = (aY1-aY2) / sqrt(b2);
  x3 = aX1 - anx*d;
  y3 = aY1 - any*d;
  x4 = aX1 - bnx*d;
  y4 = aY1 - bny*d;
  anticlockwise = (dir < 0);
  cx = x3 + any*aRadius*(anticlockwise ? 1 : -1);
  cy = y3 - anx*aRadius*(anticlockwise ? 1 : -1);
  angle0 = atan2((y3-cy), (x3-cx));
  angle1 = atan2((y4-cy), (x4-cx));


  LineTo(x3, y3);

  Arc(cx, cy, aRadius, angle0, angle1, anticlockwise, aError);
}

void
CanvasPath::Rect(double aX, double aY, double aW, double aH)
{
  MoveTo(aX, aY);
  LineTo(aX + aW, aY);
  LineTo(aX + aW, aY + aH);
  LineTo(aX, aY + aH);
  ClosePath();
}

void
CanvasPath::Arc(double aX, double aY, double aRadius,
                double aStartAngle, double aEndAngle, bool aAnticlockwise,
                ErrorResult& aError)
{
  if (aRadius < 0.0) {
    aError.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return;
  }

  EnsurePathBuilder();

  ArcToBezier(this, Point(aX, aY), Size(aRadius, aRadius), aStartAngle, aEndAngle, aAnticlockwise);
}

void
CanvasPath::Ellipse(double x, double y, double radiusX, double radiusY,
                    double rotation, double startAngle, double endAngle,
                    bool anticlockwise, ErrorResult& error)
{
  if (radiusX < 0.0 || radiusY < 0.0) {
    error.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return;
  }

  EnsurePathBuilder();

  ArcToBezier(this, Point(x, y), Size(radiusX, radiusY), startAngle, endAngle,
              anticlockwise, rotation);
}

void
CanvasPath::LineTo(const gfx::Point& aPoint)
{
  EnsurePathBuilder();

  mPathBuilder->LineTo(aPoint);
}

void
CanvasPath::BezierTo(const gfx::Point& aCP1,
                     const gfx::Point& aCP2,
                     const gfx::Point& aCP3)
{
  EnsurePathBuilder();

  mPathBuilder->BezierTo(aCP1, aCP2, aCP3);
}

void
CanvasPath::AddPath(CanvasPath& aCanvasPath, const Optional<NonNull<SVGMatrix>>& aMatrix)
{
  RefPtr<gfx::Path> tempPath = aCanvasPath.GetPath(CanvasWindingRule::Nonzero,
                                                   gfxPlatform::GetPlatform()->ScreenReferenceDrawTarget());

  if (aMatrix.WasPassed()) {
    const SVGMatrix& m = aMatrix.Value();
    Matrix transform(m.A(), m.B(), m.C(), m.D(), m.E(), m.F());

    if (!transform.IsIdentity()) {
      RefPtr<PathBuilder> tempBuilder = tempPath->TransformedCopyToBuilder(transform, FillRule::FILL_WINDING);
      tempPath = tempBuilder->Finish();
    }
  }

  EnsurePathBuilder(); // in case a path is added to itself
  tempPath->StreamToSink(mPathBuilder);
}

already_AddRefed<gfx::Path>
CanvasPath::GetPath(const CanvasWindingRule& aWinding, const DrawTarget* aTarget) const
{
  FillRule fillRule = FillRule::FILL_WINDING;
  if (aWinding == CanvasWindingRule::Evenodd) {
    fillRule = FillRule::FILL_EVEN_ODD;
  }

  if (mPath &&
      (mPath->GetBackendType() == aTarget->GetBackendType()) &&
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

void
CanvasPath::EnsurePathBuilder() const
{
  if (mPathBuilder) {
    return;
  }

  // if there is not pathbuilder, there must be a path
  MOZ_ASSERT(mPath);
  mPathBuilder = mPath->CopyToBuilder();
  mPath = nullptr;
}

} // namespace dom
} // namespace mozilla
