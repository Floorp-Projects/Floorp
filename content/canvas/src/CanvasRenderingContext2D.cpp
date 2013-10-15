/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CanvasRenderingContext2D.h"

#include "nsXULElement.h"

#include "nsIServiceManager.h"
#include "nsMathUtils.h"

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

#include "nsTArray.h"

#include "imgIEncoder.h"

#include "gfxContext.h"
#include "gfxASurface.h"
#include "gfxImageSurface.h"
#include "gfxPlatform.h"
#include "gfxFont.h"
#include "gfxBlur.h"
#include "gfxUtils.h"

#include "nsFrameManager.h"
#include "nsFrameLoader.h"
#include "nsBidi.h"
#include "nsBidiPresUtils.h"
#include "Layers.h"
#include "CanvasUtils.h"
#include "nsIMemoryReporter.h"
#include "nsStyleUtil.h"
#include "CanvasImageCache.h"

#include <algorithm>

#include "jsapi.h"
#include "jsfriendapi.h"

#include "mozilla/Alignment.h"
#include "mozilla/Assertions.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/ImageData.h"
#include "mozilla/dom/PBrowserParent.h"
#include "mozilla/dom/TypedArray.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/PathHelpers.h"
#include "mozilla/ipc/DocumentRendererParent.h"
#include "mozilla/ipc/PDocumentRendererParent.h"
#include "mozilla/MathAlgorithms.h"
#include "mozilla/Preferences.h"
#include "mozilla/Telemetry.h"
#include "mozilla/unused.h"
#include "nsCCUncollectableMarker.h"
#include "nsWrapperCacheInlines.h"
#include "mozilla/dom/CanvasRenderingContext2DBinding.h"
#include "mozilla/dom/HTMLImageElement.h"
#include "mozilla/dom/HTMLVideoElement.h"
#include "mozilla/dom/TextMetrics.h"
#include "mozilla/dom/UnionTypes.h"

#ifdef USE_SKIA_GPU
#undef free // apparently defined by some windows header, clashing with a free()
            // method in SkTypes.h
#include "GLContext.h"
#include "GLContextProvider.h"
#include "GLContextSkia.h"
#include "SurfaceTypes.h"
#include "nsIGfxInfo.h"
using mozilla::gl::GLContext;
using mozilla::gl::GLContextProvider;
#endif

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
using namespace mozilla::ipc;
using namespace mozilla::layers;

namespace mgfx = mozilla::gfx;

namespace mozilla {
namespace dom {

// Cap sigma to avoid overly large temp surfaces.
const Float SIGMA_MAX = 100;

/* Memory reporter stuff */
static int64_t gCanvasAzureMemoryUsed = 0;

// This is KIND_OTHER because it's not always clear where in memory the pixels
// of a canvas are stored.  Furthermore, this memory will be tracked by the
// underlying surface implementations.  See bug 655638 for details.
class Canvas2dPixelsReporter MOZ_FINAL : public MemoryUniReporter
{
  public:
    Canvas2dPixelsReporter()
      : MemoryUniReporter("canvas-2d-pixels", KIND_OTHER, UNITS_BYTES,
"Memory used by 2D canvases. Each canvas requires (width * height * 4) bytes.")
    {}
private:
    int64_t Amount() MOZ_OVERRIDE { return gCanvasAzureMemoryUsed; }
};

class CanvasRadialGradient : public CanvasGradient
{
public:
  CanvasRadialGradient(CanvasRenderingContext2D* aContext,
                       const Point &aBeginOrigin, Float aBeginRadius,
                       const Point &aEndOrigin, Float aEndRadius)
    : CanvasGradient(aContext, RADIAL)
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
                       const Point &aBegin, const Point &aEnd)
    : CanvasGradient(aContext, LINEAR)
    , mBegin(aBegin)
    , mEnd(aEnd)
  {
  }

protected:
  friend class CanvasGeneralPattern;

  // Beginning of linear gradient.
  Point mBegin;
  // End of linear gradient.
  Point mEnd;
};

// This class is named 'GeneralCanvasPattern' instead of just
// 'GeneralPattern' to keep Windows PGO builds from confusing the
// GeneralPattern class in gfxContext.cpp with this one.

class CanvasGeneralPattern
{
public:
  typedef CanvasRenderingContext2D::Style Style;
  typedef CanvasRenderingContext2D::ContextState ContextState;

  CanvasGeneralPattern() : mPattern(nullptr) {}
  ~CanvasGeneralPattern()
  {
    if (mPattern) {
      mPattern->~Pattern();
    }
  }

  Pattern& ForStyle(CanvasRenderingContext2D *aCtx,
                    Style aStyle,
                    DrawTarget *aRT)
  {
    // This should only be called once or the mPattern destructor will
    // not be executed.
    NS_ASSERTION(!mPattern, "ForStyle() should only be called once on CanvasGeneralPattern!");

    const ContextState &state = aCtx->CurrentState();

    if (state.StyleIsColor(aStyle)) {
      mPattern = new (mColorPattern.addr()) ColorPattern(Color::FromABGR(state.colorStyles[aStyle]));
    } else if (state.gradientStyles[aStyle] &&
               state.gradientStyles[aStyle]->GetType() == CanvasGradient::LINEAR) {
      CanvasLinearGradient *gradient =
        static_cast<CanvasLinearGradient*>(state.gradientStyles[aStyle].get());

      mPattern = new (mLinearGradientPattern.addr())
        LinearGradientPattern(gradient->mBegin, gradient->mEnd,
                              gradient->GetGradientStopsForTarget(aRT));
    } else if (state.gradientStyles[aStyle] &&
               state.gradientStyles[aStyle]->GetType() == CanvasGradient::RADIAL) {
      CanvasRadialGradient *gradient =
        static_cast<CanvasRadialGradient*>(state.gradientStyles[aStyle].get());

      mPattern = new (mRadialGradientPattern.addr())
        RadialGradientPattern(gradient->mCenter1, gradient->mCenter2, gradient->mRadius1,
                              gradient->mRadius2, gradient->GetGradientStopsForTarget(aRT));
    } else if (state.patternStyles[aStyle]) {
      if (aCtx->mCanvasElement) {
        CanvasUtils::DoDrawImageSecurityCheck(aCtx->mCanvasElement,
                                              state.patternStyles[aStyle]->mPrincipal,
                                              state.patternStyles[aStyle]->mForceWriteOnly,
                                              state.patternStyles[aStyle]->mCORSUsed);
      }

      ExtendMode mode;
      if (state.patternStyles[aStyle]->mRepeat == CanvasPattern::NOREPEAT) {
        mode = EXTEND_CLAMP;
      } else {
        mode = EXTEND_REPEAT;
      }
      mPattern = new (mSurfacePattern.addr())
        SurfacePattern(state.patternStyles[aStyle]->mSurface, mode);
    }

    return *mPattern;
  }

  union {
    AlignedStorage2<ColorPattern> mColorPattern;
    AlignedStorage2<LinearGradientPattern> mLinearGradientPattern;
    AlignedStorage2<RadialGradientPattern> mRadialGradientPattern;
    AlignedStorage2<SurfacePattern> mSurfacePattern;
  };
  Pattern *mPattern;
};

/* This is an RAII based class that can be used as a drawtarget for
 * operations that need a shadow drawn. It will automatically provide a
 * temporary target when needed, and if so blend it back with a shadow.
 *
 * aBounds specifies the bounds of the drawing operation that will be
 * drawn to the target, it is given in device space! This function will
 * change aBounds to incorporate shadow bounds. If this is nullptr the drawing
 * operation will be assumed to cover an infinite rect.
 */
class AdjustedTarget
{
public:
  typedef CanvasRenderingContext2D::ContextState ContextState;

  AdjustedTarget(CanvasRenderingContext2D *ctx,
                 mgfx::Rect *aBounds = nullptr)
    : mCtx(nullptr)
  {
    if (!ctx->NeedToDrawShadow()) {
      mTarget = ctx->mTarget;
      return;
    }
    mCtx = ctx;

    const ContextState &state = mCtx->CurrentState();

    mSigma = state.shadowBlur / 2.0f;

    if (mSigma > SIGMA_MAX) {
      mSigma = SIGMA_MAX;
    }

    Matrix transform = mCtx->mTarget->GetTransform();

    mTempRect = mgfx::Rect(0, 0, ctx->mWidth, ctx->mHeight);

    static const gfxFloat GAUSSIAN_SCALE_FACTOR = (3 * sqrt(2 * M_PI) / 4) * 1.5;
    int32_t blurRadius = (int32_t) floor(mSigma * GAUSSIAN_SCALE_FACTOR + 0.5);

    // We need to enlarge and possibly offset our temporary surface
    // so that things outside of the canvas may cast shadows.
    mTempRect.Inflate(Margin(blurRadius + std::max<Float>(state.shadowOffset.y, 0),
                             blurRadius + std::max<Float>(-state.shadowOffset.x, 0),
                             blurRadius + std::max<Float>(-state.shadowOffset.y, 0),
                             blurRadius + std::max<Float>(state.shadowOffset.x, 0)));

    if (aBounds) {
      // We actually include the bounds of the shadow blur, this makes it
      // easier to execute the actual blur on hardware, and shouldn't affect
      // the amount of pixels that need to be touched.
      aBounds->Inflate(Margin(blurRadius, blurRadius,
                              blurRadius, blurRadius));
      mTempRect = mTempRect.Intersect(*aBounds);
    }

    mTempRect.ScaleRoundOut(1.0f);

    transform._31 -= mTempRect.x;
    transform._32 -= mTempRect.y;

    mTarget =
      mCtx->mTarget->CreateShadowDrawTarget(IntSize(int32_t(mTempRect.width), int32_t(mTempRect.height)),
                                            FORMAT_B8G8R8A8, mSigma);

    if (!mTarget) {
      // XXX - Deal with the situation where our temp size is too big to
      // fit in a texture.
      mTarget = ctx->mTarget;
      mCtx = nullptr;
    } else {
      mTarget->SetTransform(transform);
    }
  }

  ~AdjustedTarget()
  {
    if (!mCtx) {
      return;
    }

    RefPtr<SourceSurface> snapshot = mTarget->Snapshot();

    mCtx->mTarget->DrawSurfaceWithShadow(snapshot, mTempRect.TopLeft(),
                                         Color::FromABGR(mCtx->CurrentState().shadowColor),
                                         mCtx->CurrentState().shadowOffset, mSigma,
                                         mCtx->CurrentState().op);
  }

  DrawTarget* operator->()
  {
    return mTarget;
  }

private:
  RefPtr<DrawTarget> mTarget;
  CanvasRenderingContext2D *mCtx;
  Float mSigma;
  mgfx::Rect mTempRect;
};

void
CanvasGradient::AddColorStop(float offset, const nsAString& colorstr, ErrorResult& rv)
{
  if (offset < 0.0 || offset > 1.0) {
    rv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return;
  }

  nsCSSValue value;
  nsCSSParser parser;
  if (!parser.ParseColorString(colorstr, nullptr, 0, value)) {
    rv.Throw(NS_ERROR_DOM_SYNTAX_ERR);
    return;
  }

  nscolor color;
  if (!nsRuleNode::ComputeColor(value, nullptr, nullptr, color)) {
    rv.Throw(NS_ERROR_DOM_SYNTAX_ERR);
    return;
  }

  mStops = nullptr;

  GradientStop newStop;

  newStop.offset = offset;
  newStop.color = Color::FromABGR(color);

  mRawStops.AppendElement(newStop);
}

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(CanvasGradient, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(CanvasGradient, Release)

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_1(CanvasGradient, mContext)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(CanvasPattern, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(CanvasPattern, Release)

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_1(CanvasPattern, mContext)

class CanvasRenderingContext2DUserData : public LayerUserData {
public:
    CanvasRenderingContext2DUserData(CanvasRenderingContext2D *aContext)
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

#ifdef USE_SKIA_GPU
  static void PreTransactionCallback(void* aData)
  {
    CanvasRenderingContext2DUserData* self =
      static_cast<CanvasRenderingContext2DUserData*>(aData);
    CanvasRenderingContext2D* context = self->mContext;
    if (!context)
      return;

    GLContext* glContext = static_cast<GLContext*>(context->mTarget->GetGLContext());
    if (!glContext)
      return;

    if (context->mTarget) {
      // Since SkiaGL default to store drawing command until flush
      // We will have to flush it before present.
      context->mTarget->Flush();
    }
    glContext->MakeCurrent();
    glContext->PublishFrame();
  }
#endif

  static void DidTransactionCallback(void* aData)
  {
    CanvasRenderingContext2DUserData* self =
      static_cast<CanvasRenderingContext2DUserData*>(aData);
    if (self->mContext) {
      self->mContext->MarkContextClean();
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

NS_IMPL_CYCLE_COLLECTING_ADDREF(CanvasRenderingContext2D)
NS_IMPL_CYCLE_COLLECTING_RELEASE(CanvasRenderingContext2D)

NS_IMPL_CYCLE_COLLECTION_CLASS(CanvasRenderingContext2D)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(CanvasRenderingContext2D)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mCanvasElement)
  for (uint32_t i = 0; i < tmp->mStyleStack.Length(); i++) {
    ImplCycleCollectionUnlink(tmp->mStyleStack[i].patternStyles[STYLE_STROKE]);
    ImplCycleCollectionUnlink(tmp->mStyleStack[i].patternStyles[STYLE_FILL]);
    ImplCycleCollectionUnlink(tmp->mStyleStack[i].gradientStyles[STYLE_STROKE]);
    ImplCycleCollectionUnlink(tmp->mStyleStack[i].gradientStyles[STYLE_FILL]);
  }
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(CanvasRenderingContext2D)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mCanvasElement)
  for (uint32_t i = 0; i < tmp->mStyleStack.Length(); i++) {
    ImplCycleCollectionTraverse(cb, tmp->mStyleStack[i].patternStyles[STYLE_STROKE], "Stroke CanvasPattern");
    ImplCycleCollectionTraverse(cb, tmp->mStyleStack[i].patternStyles[STYLE_FILL], "Fill CanvasPattern");
    ImplCycleCollectionTraverse(cb, tmp->mStyleStack[i].gradientStyles[STYLE_STROKE], "Stroke CanvasGradient");
    ImplCycleCollectionTraverse(cb, tmp->mStyleStack[i].gradientStyles[STYLE_FILL], "Fill CanvasGradient");
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
  : mZero(false), mOpaque(false), mResetLayer(true)
  , mIPC(false)
  , mIsEntireFrameInvalid(false)
  , mPredictManyRedrawCalls(false), mPathTransformWillUpdate(false)
  , mInvalidateCount(0)
{
  sNumLivingContexts++;
  SetIsDOMBinding();

#ifdef USE_SKIA_GPU
  mForceSoftware = false;
#endif
}

CanvasRenderingContext2D::~CanvasRenderingContext2D()
{
  Reset();
  // Drop references from all CanvasRenderingContext2DUserData to this context
  for (uint32_t i = 0; i < mUserDatas.Length(); ++i) {
    mUserDatas[i]->Forget();
  }
  sNumLivingContexts--;
  if (!sNumLivingContexts) {
    NS_IF_RELEASE(sErrorTarget);
  }

#ifdef USE_SKIA_GPU
  RemoveDemotableContext(this);
#endif
}

JSObject*
CanvasRenderingContext2D::WrapObject(JSContext *cx, JS::Handle<JSObject*> scope)
{
  return CanvasRenderingContext2DBinding::Wrap(cx, scope, this);
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

  if (value.GetUnit() == eCSSUnit_Color) {
    // if we already have a color we can just use it directly
    *aColor = value.GetColorValue();
  } else {
    // otherwise resolve it
    nsIPresShell* presShell = GetPresShell();
    nsRefPtr<nsStyleContext> parentContext;
    if (mCanvasElement && mCanvasElement->IsInDoc()) {
      // Inherit from the canvas element.
      parentContext = nsComputedDOMStyle::GetStyleContextForElement(
        mCanvasElement, nullptr, presShell);
    }

    unused << nsRuleNode::ComputeColor(
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

  mTarget = nullptr;

  // Since the target changes the backing texture will change, and this will
  // no longer be valid.
  mIsEntireFrameInvalid = false;
  mPredictManyRedrawCalls = false;

  return NS_OK;
}

void
CanvasRenderingContext2D::SetStyleFromString(const nsAString& str,
                                             Style whichStyle)
{
  MOZ_ASSERT(!str.IsVoid());

  nscolor color;
  if (!ParseColor(str, &color)) {
    return;
  }

  CurrentState().SetColorStyle(whichStyle, color);
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
CanvasRenderingContext2D::Redraw(const mgfx::Rect &r)
{
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

  mCanvasElement->InvalidateCanvasContent(&r);
}

void
CanvasRenderingContext2D::RedrawUser(const gfxRect& r)
{
  if (mIsEntireFrameInvalid) {
    ++mInvalidateCount;
    return;
  }

  mgfx::Rect newr =
    mTarget->GetTransform().TransformBounds(ToRect(r));
  Redraw(newr);
}

void CanvasRenderingContext2D::Demote()
{
#ifdef  USE_SKIA_GPU
  if (!IsTargetValid() || mForceSoftware || !mTarget->GetGLContext())
    return;

  RemoveDemotableContext(this);

  RefPtr<SourceSurface> snapshot = mTarget->Snapshot();
  RefPtr<DrawTarget> oldTarget = mTarget;
  mTarget = nullptr;
  mResetLayer = true;
  mForceSoftware = true;

  // Recreate target, now demoted to software only
  EnsureTarget();
  if (!IsTargetValid())
    return;

  // Restore the content from the old DrawTarget
  mgfx::Rect r(0, 0, mWidth, mHeight);
  mTarget->DrawSurface(snapshot, r, r);

  // Restore the clips and transform
  for (uint32_t i = 0; i < CurrentState().clipsPushed.size(); i++) {
    mTarget->PushClip(CurrentState().clipsPushed[i]);
  }

  mTarget->SetTransform(oldTarget->GetTransform());
#endif
}

#ifdef USE_SKIA_GPU

std::vector<CanvasRenderingContext2D*>&
CanvasRenderingContext2D::DemotableContexts()
{
  static std::vector<CanvasRenderingContext2D*> contexts;
  return contexts;
}

void
CanvasRenderingContext2D::DemoteOldestContextIfNecessary()
{
#ifdef MOZ_GFX_OPTIMIZE_MOBILE
  const size_t kMaxContexts = 2;
#else
  const size_t kMaxContexts = 16;
#endif

  std::vector<CanvasRenderingContext2D*>& contexts = DemotableContexts();
  if (contexts.size() < kMaxContexts)
    return;

  CanvasRenderingContext2D* oldest = contexts.front();
  oldest->Demote();
}

void
CanvasRenderingContext2D::AddDemotableContext(CanvasRenderingContext2D* context)
{
  std::vector<CanvasRenderingContext2D*>::iterator iter = std::find(DemotableContexts().begin(), DemotableContexts().end(), context);
  if (iter != DemotableContexts().end())
    return;

  DemotableContexts().push_back(context);
}

void
CanvasRenderingContext2D::RemoveDemotableContext(CanvasRenderingContext2D* context)
{
  std::vector<CanvasRenderingContext2D*>::iterator iter = std::find(DemotableContexts().begin(), DemotableContexts().end(), context);
  if (iter != DemotableContexts().end())
    DemotableContexts().erase(iter);
}

bool
CheckSizeForSkiaGL(IntSize size) {
  int minsize = Preferences::GetInt("gfx.canvas.min-size-for-skia-gl", 128);
  return size.width >= minsize && size.height >= minsize;
}

#endif

void
CanvasRenderingContext2D::EnsureTarget()
{
  if (mTarget) {
    return;
  }

   // Check that the dimensions are sane
  IntSize size(mWidth, mHeight);
  if (size.width <= 0xFFFF && size.height <= 0xFFFF &&
      size.width >= 0 && size.height >= 0) {
    SurfaceFormat format = GetSurfaceFormat();
    nsIDocument* ownerDoc = nullptr;
    if (mCanvasElement) {
      ownerDoc = mCanvasElement->OwnerDoc();
    }

    nsRefPtr<LayerManager> layerManager = nullptr;

    if (ownerDoc) {
      layerManager =
        nsContentUtils::PersistentLayerManagerForDocument(ownerDoc);
    }

     if (layerManager) {
#ifdef USE_SKIA_GPU
      if (gfxPlatform::GetPlatform()->UseAcceleratedSkiaCanvas()) {
        SurfaceCaps caps = SurfaceCaps::ForRGBA();
        caps.preserve = true;

#ifdef MOZ_WIDGET_GONK
        layers::ShadowLayerForwarder *forwarder = layerManager->AsShadowForwarder();
        if (forwarder) {
          caps.surfaceAllocator = static_cast<layers::ISurfaceAllocator*>(forwarder);
        }
#endif

        DemoteOldestContextIfNecessary();

        nsRefPtr<GLContext> glContext;
        nsCOMPtr<nsIGfxInfo> gfxInfo = do_GetService("@mozilla.org/gfx/info;1");
        nsString vendor;

        if (!mForceSoftware && CheckSizeForSkiaGL(size))
        {
          glContext = GLContextProvider::CreateOffscreen(gfxIntSize(size.width, size.height),
                                                         caps, gl::ContextFlagsNone);
        }

        if (glContext) {
          SkAutoTUnref<GrGLInterface> i(CreateGrGLInterfaceFromGLContext(glContext));
          mTarget = Factory::CreateDrawTargetSkiaWithGLContextAndGrGLInterface(glContext, i, size, format);
          AddDemotableContext(this);
        } else {
          mTarget = layerManager->CreateDrawTarget(size, format);
        }
      } else
#endif
       mTarget = layerManager->CreateDrawTarget(size, format);
     } else {
       mTarget = gfxPlatform::GetPlatform()->CreateOffscreenCanvasDrawTarget(size, format);
     }
  }

  if (mTarget) {
    static bool registered = false;
    if (!registered) {
      registered = true;
      NS_RegisterMemoryReporter(new Canvas2dPixelsReporter());
    }

    gCanvasAzureMemoryUsed += mWidth * mHeight * 4;
    JSContext* context = nsContentUtils::GetCurrentJSContext();
    if (context) {
      JS_updateMallocCounter(context, mWidth * mHeight * 4);
    }

    mTarget->ClearRect(mgfx::Rect(Point(0, 0), Size(mWidth, mHeight)));
    // Force a full layer transaction since we didn't have a layer before
    // and now we might need one.
    if (mCanvasElement) {
      mCanvasElement->InvalidateCanvas();
    }
    // Calling Redraw() tells our invalidation machinery that the entire
    // canvas is already invalid, which can speed up future drawing.
    Redraw();
  } else {
    EnsureErrorTarget();
    mTarget = sErrorTarget;
  }
}

#ifdef DEBUG
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
#endif

NS_IMETHODIMP
CanvasRenderingContext2D::SetDimensions(int32_t width, int32_t height)
{
  ClearTarget();

  // Zero sized surfaces cause issues, so just go with 1x1.
  if (height == 0 || width == 0) {
    mZero = true;
    mWidth = 1;
    mHeight = 1;
  } else {
    mZero = false;
    mWidth = width;
    mHeight = height;
  }

  return NS_OK;
}

void
CanvasRenderingContext2D::ClearTarget()
{
  Reset();

  mResetLayer = true;

  // set up the initial canvas defaults
  mStyleStack.Clear();
  mPathBuilder = nullptr;
  mPath = nullptr;
  mDSPathBuilder = nullptr;

  ContextState *state = mStyleStack.AppendElement();
  state->globalAlpha = 1.0;

  state->colorStyles[STYLE_FILL] = NS_RGB(0,0,0);
  state->colorStyles[STYLE_STROKE] = NS_RGB(0,0,0);
  state->shadowColor = NS_RGBA(0,0,0,0);
}

NS_IMETHODIMP
CanvasRenderingContext2D::InitializeWithSurface(nsIDocShell *shell,
                                                gfxASurface *surface,
                                                int32_t width,
                                                int32_t height)
{
  mDocShell = shell;

  SetDimensions(width, height);
  mTarget = gfxPlatform::GetPlatform()->
    CreateDrawTargetForSurface(surface, IntSize(width, height));

  if (!mTarget) {
    EnsureErrorTarget();
    mTarget = sErrorTarget;
  }

  return NS_OK;
}

NS_IMETHODIMP
CanvasRenderingContext2D::SetIsOpaque(bool isOpaque)
{
  if (isOpaque != mOpaque) {
    mOpaque = isOpaque;
    ClearTarget();
  }

  return NS_OK;
}

NS_IMETHODIMP
CanvasRenderingContext2D::SetIsIPC(bool isIPC)
{
  if (isIPC != mIPC) {
    mIPC = isIPC;
    ClearTarget();
  }

  return NS_OK;
}

NS_IMETHODIMP
CanvasRenderingContext2D::Render(gfxContext *ctx, GraphicsFilter aFilter, uint32_t aFlags)
{
  nsresult rv = NS_OK;

  EnsureTarget();
  if (!IsTargetValid()) {
    return NS_ERROR_FAILURE;
  }

  nsRefPtr<gfxASurface> surface;

  if (NS_FAILED(GetThebesSurface(getter_AddRefs(surface)))) {
    return NS_ERROR_FAILURE;
  }

  nsRefPtr<gfxPattern> pat = new gfxPattern(surface);

  pat->SetFilter(aFilter);
  pat->SetExtend(gfxPattern::EXTEND_PAD);

  gfxContext::GraphicsOperator op = ctx->CurrentOperator();
  if (mOpaque)
      ctx->SetOperator(gfxContext::OPERATOR_SOURCE);

  // XXX I don't want to use PixelSnapped here, but layout doesn't guarantee
  // pixel alignment for this stuff!
  ctx->NewPath();
  ctx->PixelSnappedRectangleAndSetPattern(gfxRect(0, 0, mWidth, mHeight), pat);
  ctx->Fill();

  if (mOpaque)
      ctx->SetOperator(op);

  if (!(aFlags & RenderFlagPremultAlpha)) {
      nsRefPtr<gfxASurface> curSurface = ctx->CurrentSurface();
      nsRefPtr<gfxImageSurface> gis = curSurface->GetAsImageSurface();
      MOZ_ASSERT(gis, "If non-premult alpha, must be able to get image surface!");

      gfxUtils::UnpremultiplyImageSurface(gis);
  }

  return rv;
}

NS_IMETHODIMP
CanvasRenderingContext2D::GetInputStream(const char *aMimeType,
                                         const PRUnichar *aEncoderOptions,
                                         nsIInputStream **aStream)
{
  EnsureTarget();
  if (!IsTargetValid()) {
    return NS_ERROR_FAILURE;
  }

  nsRefPtr<gfxASurface> surface;

  if (NS_FAILED(GetThebesSurface(getter_AddRefs(surface)))) {
    return NS_ERROR_FAILURE;
  }

  nsresult rv;
  const char encoderPrefix[] = "@mozilla.org/image/encoder;2?type=";
  static const fallible_t fallible = fallible_t();
  nsAutoArrayPtr<char> conid(new (fallible) char[strlen(encoderPrefix) + strlen(aMimeType) + 1]);

  if (!conid) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  strcpy(conid, encoderPrefix);
  strcat(conid, aMimeType);

  nsCOMPtr<imgIEncoder> encoder = do_CreateInstance(conid);
  if (!encoder) {
    return NS_ERROR_FAILURE;
  }

  nsAutoArrayPtr<uint8_t> imageBuffer(new (fallible) uint8_t[mWidth * mHeight * 4]);
  if (!imageBuffer) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsRefPtr<gfxImageSurface> imgsurf =
    new gfxImageSurface(imageBuffer.get(),
                        gfxIntSize(mWidth, mHeight),
                        mWidth * 4,
                        gfxImageFormatARGB32);

  if (!imgsurf || imgsurf->CairoStatus()) {
    return NS_ERROR_FAILURE;
  }

  nsRefPtr<gfxContext> ctx = new gfxContext(imgsurf);

  if (!ctx || ctx->HasError()) {
    return NS_ERROR_FAILURE;
  }

  ctx->SetOperator(gfxContext::OPERATOR_SOURCE);
  ctx->SetSource(surface, gfxPoint(0, 0));
  ctx->Paint();

  rv = encoder->InitFromData(imageBuffer.get(),
                              mWidth * mHeight * 4, mWidth, mHeight, mWidth * 4,
                              imgIEncoder::INPUT_FORMAT_HOSTARGB,
                              nsDependentString(aEncoderOptions));
  NS_ENSURE_SUCCESS(rv, rv);

  return CallQueryInterface(encoder, aStream);
}

SurfaceFormat
CanvasRenderingContext2D::GetSurfaceFormat() const
{
  return mOpaque ? FORMAT_B8G8R8X8 : FORMAT_B8G8R8A8;
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
}

void
CanvasRenderingContext2D::Restore()
{
  if (mStyleStack.Length() - 1 == 0)
    return;

  TransformWillUpdate();

  for (uint32_t i = 0; i < CurrentState().clipsPushed.size(); i++) {
    mTarget->PopClip();
  }

  mStyleStack.RemoveElementAt(mStyleStack.Length() - 1);

  mTarget->SetTransform(CurrentState().transform);
}

//
// transformations
//

void
CanvasRenderingContext2D::Scale(double x, double y, ErrorResult& error)
{
  TransformWillUpdate();
  if (!IsTargetValid()) {
    error.Throw(NS_ERROR_FAILURE);
    return;
  }

  Matrix newMatrix = mTarget->GetTransform();
  mTarget->SetTransform(newMatrix.Scale(x, y));
}

void
CanvasRenderingContext2D::Rotate(double angle, ErrorResult& error)
{
  TransformWillUpdate();
  if (!IsTargetValid()) {
    error.Throw(NS_ERROR_FAILURE);
    return;
  }

  Matrix rotation = Matrix::Rotation(angle);
  mTarget->SetTransform(rotation * mTarget->GetTransform());
}

void
CanvasRenderingContext2D::Translate(double x, double y, ErrorResult& error)
{
  TransformWillUpdate();
  if (!IsTargetValid()) {
    error.Throw(NS_ERROR_FAILURE);
    return;
  }

  Matrix newMatrix = mTarget->GetTransform();
  mTarget->SetTransform(newMatrix.Translate(x, y));
}

void
CanvasRenderingContext2D::Transform(double m11, double m12, double m21,
                                    double m22, double dx, double dy,
                                    ErrorResult& error)
{
  TransformWillUpdate();
  if (!IsTargetValid()) {
    error.Throw(NS_ERROR_FAILURE);
    return;
  }

  Matrix matrix(m11, m12, m21, m22, dx, dy);
  mTarget->SetTransform(matrix * mTarget->GetTransform());
}

void
CanvasRenderingContext2D::SetTransform(double m11, double m12,
                                       double m21, double m22,
                                       double dx, double dy,
                                       ErrorResult& error)
{
  TransformWillUpdate();
  if (!IsTargetValid()) {
    error.Throw(NS_ERROR_FAILURE);
    return;
  }

  Matrix matrix(m11, m12, m21, m22, dx, dy);
  mTarget->SetTransform(matrix);
}

JSObject*
MatrixToJSObject(JSContext* cx, const Matrix& matrix, ErrorResult& error)
{
  JS::Value elts[] = {
    DOUBLE_TO_JSVAL(matrix._11), DOUBLE_TO_JSVAL(matrix._12),
    DOUBLE_TO_JSVAL(matrix._21), DOUBLE_TO_JSVAL(matrix._22),
    DOUBLE_TO_JSVAL(matrix._31), DOUBLE_TO_JSVAL(matrix._32)
  };

  // XXX Should we enter GetWrapper()'s compartment?
  JSObject* obj = JS_NewArrayObject(cx, 6, elts);
  if  (!obj) {
    error.Throw(NS_ERROR_OUT_OF_MEMORY);
  }
  return obj;
}

static bool
ObjectToMatrix(JSContext* cx, JS::Handle<JSObject*> obj, Matrix& matrix,
               ErrorResult& error)
{
  uint32_t length;
  if (!JS_GetArrayLength(cx, obj, &length) || length != 6) {
    // Not an array-like thing or wrong size
    error.Throw(NS_ERROR_INVALID_ARG);
    return false;
  }

  Float* elts[] = { &matrix._11, &matrix._12, &matrix._21, &matrix._22,
                    &matrix._31, &matrix._32 };
  for (uint32_t i = 0; i < 6; ++i) {
    JS::Rooted<JS::Value> elt(cx);
    double d;
    if (!JS_GetElement(cx, obj, i, &elt)) {
      error.Throw(NS_ERROR_FAILURE);
      return false;
    }
    if (!CoerceDouble(elt, &d)) {
      error.Throw(NS_ERROR_INVALID_ARG);
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
CanvasRenderingContext2D::SetMozCurrentTransform(JSContext* cx,
                                                 JS::Handle<JSObject*> currentTransform,
                                                 ErrorResult& error)
{
  EnsureTarget();
  if (!IsTargetValid()) {
    error.Throw(NS_ERROR_FAILURE);
    return;
  }

  Matrix newCTM;
  if (ObjectToMatrix(cx, currentTransform, newCTM, error)) {
    mTarget->SetTransform(newCTM);
  }
}

JSObject*
CanvasRenderingContext2D::GetMozCurrentTransform(JSContext* cx,
                                                 ErrorResult& error) const
{
  return MatrixToJSObject(cx, mTarget ? mTarget->GetTransform() : Matrix(), error);
}

void
CanvasRenderingContext2D::SetMozCurrentTransformInverse(JSContext* cx,
                                                        JS::Handle<JSObject*> currentTransform,
                                                        ErrorResult& error)
{
  EnsureTarget();
  if (!IsTargetValid()) {
    error.Throw(NS_ERROR_FAILURE);
    return;
  }

  Matrix newCTMInverse;
  if (ObjectToMatrix(cx, currentTransform, newCTMInverse, error)) {
    // XXX ERRMSG we need to report an error to developers here! (bug 329026)
    if (newCTMInverse.Invert()) {
      mTarget->SetTransform(newCTMInverse);
    }
  }
}

JSObject*
CanvasRenderingContext2D::GetMozCurrentTransformInverse(JSContext* cx,
                                                        ErrorResult& error) const
{
  if (!mTarget) {
    return MatrixToJSObject(cx, Matrix(), error);
  }

  Matrix ctm = mTarget->GetTransform();

  if (!ctm.Invert()) {
    double NaN = JSVAL_TO_DOUBLE(JS_GetNaNValue(cx));
    ctm = Matrix(NaN, NaN, NaN, NaN, NaN, NaN);
  }

  return MatrixToJSObject(cx, ctm, error);
}

//
// colors
//

void
CanvasRenderingContext2D::SetStyleFromUnion(const StringOrCanvasGradientOrCanvasPattern& value,
                                            Style whichStyle)
{
  if (value.IsString()) {
    SetStyleFromString(value.GetAsString(), whichStyle);
    return;
  }

  if (value.IsCanvasGradient()) {
    SetStyleFromGradient(value.GetAsCanvasGradient(), whichStyle);
    return;
  }

  if (value.IsCanvasPattern()) {
    SetStyleFromPattern(value.GetAsCanvasPattern(), whichStyle);
    return;
  }

  MOZ_ASSUME_UNREACHABLE("Invalid union value");
}

void
CanvasRenderingContext2D::SetFillRule(const nsAString& aString)
{
  FillRule rule;

  if (aString.EqualsLiteral("evenodd"))
    rule = FILL_EVEN_ODD;
  else if (aString.EqualsLiteral("nonzero"))
    rule = FILL_WINDING;
  else
    return;

  CurrentState().fillRule = rule;
}

void
CanvasRenderingContext2D::GetFillRule(nsAString& aString)
{
  switch (CurrentState().fillRule) {
  case FILL_WINDING:
    aString.AssignLiteral("nonzero"); break;
  case FILL_EVEN_ODD:
    aString.AssignLiteral("evenodd"); break;
  }
}
//
// gradients and patterns
//
already_AddRefed<CanvasGradient>
CanvasRenderingContext2D::CreateLinearGradient(double x0, double y0, double x1, double y1)
{
  nsRefPtr<CanvasGradient> grad =
    new CanvasLinearGradient(this, Point(x0, y0), Point(x1, y1));

  return grad.forget();
}

already_AddRefed<CanvasGradient>
CanvasRenderingContext2D::CreateRadialGradient(double x0, double y0, double r0,
                                               double x1, double y1, double r1,
                                               ErrorResult& aError)
{
  if (r0 < 0.0 || r1 < 0.0) {
    aError.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return nullptr;
  }

  nsRefPtr<CanvasGradient> grad =
    new CanvasRadialGradient(this, Point(x0, y0), r0, Point(x1, y1), r1);

  return grad.forget();
}

already_AddRefed<CanvasPattern>
CanvasRenderingContext2D::CreatePattern(const HTMLImageOrCanvasOrVideoElement& element,
                                        const nsAString& repeat,
                                        ErrorResult& error)
{
  CanvasPattern::RepeatMode repeatMode =
    CanvasPattern::NOREPEAT;

  if (repeat.IsEmpty() || repeat.EqualsLiteral("repeat")) {
    repeatMode = CanvasPattern::REPEAT;
  } else if (repeat.EqualsLiteral("repeat-x")) {
    repeatMode = CanvasPattern::REPEATX;
  } else if (repeat.EqualsLiteral("repeat-y")) {
    repeatMode = CanvasPattern::REPEATY;
  } else if (repeat.EqualsLiteral("no-repeat")) {
    repeatMode = CanvasPattern::NOREPEAT;
  } else {
    error.Throw(NS_ERROR_DOM_SYNTAX_ERR);
    return nullptr;
  }

  Element* htmlElement;
  if (element.IsHTMLCanvasElement()) {
    HTMLCanvasElement* canvas = &element.GetAsHTMLCanvasElement();
    htmlElement = canvas;

    nsIntSize size = canvas->GetSize();
    if (size.width == 0 || size.height == 0) {
      error.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
      return nullptr;
    }

    // Special case for Canvas, which could be an Azure canvas!
    nsICanvasRenderingContextInternal *srcCanvas = canvas->GetContextAtIndex(0);
    if (srcCanvas) {
      // This might not be an Azure canvas!
      RefPtr<SourceSurface> srcSurf = srcCanvas->GetSurfaceSnapshot();

      nsRefPtr<CanvasPattern> pat =
        new CanvasPattern(this, srcSurf, repeatMode, htmlElement->NodePrincipal(), canvas->IsWriteOnly(), false);

      return pat.forget();
    }
  } else if (element.IsHTMLImageElement()) {
    htmlElement = &element.GetAsHTMLImageElement();
  } else {
    htmlElement = &element.GetAsHTMLVideoElement();
  }

  // The canvas spec says that createPattern should use the first frame
  // of animated images
  nsLayoutUtils::SurfaceFromElementResult res =
    nsLayoutUtils::SurfaceFromElement(htmlElement,
      nsLayoutUtils::SFE_WANT_FIRST_FRAME | nsLayoutUtils::SFE_WANT_NEW_SURFACE);

  if (!res.mSurface) {
    error.Throw(NS_ERROR_NOT_AVAILABLE);
    return nullptr;
  }

  // Ignore nullptr cairo surfaces! See bug 666312.
  if (!res.mSurface->CairoSurface() || res.mSurface->CairoStatus()) {
    error.Throw(NS_ERROR_NOT_AVAILABLE);
    return nullptr;
  }

  EnsureTarget();
  RefPtr<SourceSurface> srcSurf =
    gfxPlatform::GetPlatform()->GetSourceSurfaceForSurface(mTarget, res.mSurface);

  nsRefPtr<CanvasPattern> pat =
    new CanvasPattern(this, srcSurf, repeatMode, res.mPrincipal,
                             res.mIsWriteOnly, res.mCORSUsed);

  return pat.forget();
}

//
// shadows
//
void
CanvasRenderingContext2D::SetShadowColor(const nsAString& shadowColor)
{
  nscolor color;
  if (!ParseColor(shadowColor, &color)) {
    return;
  }

  CurrentState().shadowColor = color;
}

//
// rects
//

void
CanvasRenderingContext2D::ClearRect(double x, double y, double w,
                                    double h)
{
  if (!mTarget) {
    return;
  }

  mTarget->ClearRect(mgfx::Rect(x, y, w, h));

  RedrawUser(gfxRect(x, y, w, h));
}

void
CanvasRenderingContext2D::FillRect(double x, double y, double w,
                                   double h)
{
  const ContextState &state = CurrentState();

  if (state.patternStyles[STYLE_FILL]) {
    CanvasPattern::RepeatMode repeat =
      state.patternStyles[STYLE_FILL]->mRepeat;
    // In the FillRect case repeat modes are easy to deal with.
    bool limitx = repeat == CanvasPattern::NOREPEAT || repeat == CanvasPattern::REPEATY;
    bool limity = repeat == CanvasPattern::NOREPEAT || repeat == CanvasPattern::REPEATX;

    IntSize patternSize =
      state.patternStyles[STYLE_FILL]->mSurface->GetSize();

    // We always need to execute painting for non-over operators, even if
    // we end up with w/h = 0.
    if (limitx) {
      if (x < 0) {
        w += x;
        if (w < 0) {
          w = 0;
        }

        x = 0;
      }
      if (x + w > patternSize.width) {
        w = patternSize.width - x;
        if (w < 0) {
          w = 0;
        }
      }
    }
    if (limity) {
      if (y < 0) {
        h += y;
        if (h < 0) {
          h = 0;
        }

        y = 0;
      }
      if (y + h > patternSize.height) {
        h = patternSize.height - y;
        if (h < 0) {
          h = 0;
        }
      }
    }
  }

  mgfx::Rect bounds;

  EnsureTarget();
  if (NeedToDrawShadow()) {
    bounds = mgfx::Rect(x, y, w, h);
    bounds = mTarget->GetTransform().TransformBounds(bounds);
  }

  AdjustedTarget(this, bounds.IsEmpty() ? nullptr : &bounds)->
    FillRect(mgfx::Rect(x, y, w, h),
             CanvasGeneralPattern().ForStyle(this, STYLE_FILL, mTarget),
             DrawOptions(state.globalAlpha, UsedOperation()));

  RedrawUser(gfxRect(x, y, w, h));
}

void
CanvasRenderingContext2D::StrokeRect(double x, double y, double w,
                                     double h)
{
  const ContextState &state = CurrentState();

  mgfx::Rect bounds;

  if (!w && !h) {
    return;
  }

  EnsureTarget();
  if (!IsTargetValid()) {
    return;
  }

  if (NeedToDrawShadow()) {
    bounds = mgfx::Rect(x - state.lineWidth / 2.0f, y - state.lineWidth / 2.0f,
                        w + state.lineWidth, h + state.lineWidth);
    bounds = mTarget->GetTransform().TransformBounds(bounds);
  }

  if (!h) {
    CapStyle cap = CAP_BUTT;
    if (state.lineJoin == JOIN_ROUND) {
      cap = CAP_ROUND;
    }
    AdjustedTarget(this, bounds.IsEmpty() ? nullptr : &bounds)->
      StrokeLine(Point(x, y), Point(x + w, y),
                  CanvasGeneralPattern().ForStyle(this, STYLE_STROKE, mTarget),
                  StrokeOptions(state.lineWidth, state.lineJoin,
                                cap, state.miterLimit,
                                state.dash.Length(),
                                state.dash.Elements(),
                                state.dashOffset),
                  DrawOptions(state.globalAlpha, UsedOperation()));
    return;
  }

  if (!w) {
    CapStyle cap = CAP_BUTT;
    if (state.lineJoin == JOIN_ROUND) {
      cap = CAP_ROUND;
    }
    AdjustedTarget(this, bounds.IsEmpty() ? nullptr : &bounds)->
      StrokeLine(Point(x, y), Point(x, y + h),
                  CanvasGeneralPattern().ForStyle(this, STYLE_STROKE, mTarget),
                  StrokeOptions(state.lineWidth, state.lineJoin,
                                cap, state.miterLimit,
                                state.dash.Length(),
                                state.dash.Elements(),
                                state.dashOffset),
                  DrawOptions(state.globalAlpha, UsedOperation()));
    return;
  }

  AdjustedTarget(this, bounds.IsEmpty() ? nullptr : &bounds)->
    StrokeRect(mgfx::Rect(x, y, w, h),
                CanvasGeneralPattern().ForStyle(this, STYLE_STROKE, mTarget),
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
CanvasRenderingContext2D::Fill(const CanvasWindingRule& winding)
{
  EnsureUserSpacePath(winding);

  if (!mPath) {
    return;
  }

  mgfx::Rect bounds;

  if (NeedToDrawShadow()) {
    bounds = mPath->GetBounds(mTarget->GetTransform());
  }

  AdjustedTarget(this, bounds.IsEmpty() ? nullptr : &bounds)->
    Fill(mPath, CanvasGeneralPattern().ForStyle(this, STYLE_FILL, mTarget),
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

  mgfx::Rect bounds;
  if (NeedToDrawShadow()) {
    bounds =
      mPath->GetStrokedBounds(strokeOptions, mTarget->GetTransform());
  }

  AdjustedTarget(this, bounds.IsEmpty() ? nullptr : &bounds)->
    Stroke(mPath, CanvasGeneralPattern().ForStyle(this, STYLE_STROKE, mTarget),
           strokeOptions, DrawOptions(state.globalAlpha, UsedOperation()));

  Redraw();
}

void
CanvasRenderingContext2D::Clip(const CanvasWindingRule& winding)
{
  EnsureUserSpacePath(winding);

  if (!mPath) {
    return;
  }

  mTarget->PushClip(mPath);
  CurrentState().clipsPushed.push_back(mPath);
}

void
CanvasRenderingContext2D::ArcTo(double x1, double y1, double x2,
                                double y2, double radius,
                                ErrorResult& error)
{
  if (radius < 0) {
    error.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
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

    p0 = invTransform * mDSPathBuilder->CurrentPoint();
  }

  Point p1(x1, y1);
  Point p2(x2, y2);

  // Execute these calculations in double precision to avoid cumulative
  // rounding errors.
  double dir, a2, b2, c2, cosx, sinx, d, anx, any,
         bnx, bny, x3, y3, x4, y4, cx, cy, angle0, angle1;
  bool anticlockwise;

  if (p0 == p1 || p1 == p2 || radius == 0) {
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
  a2 = (p0.x-x1)*(p0.x-x1) + (p0.y-y1)*(p0.y-y1);
  b2 = (x1-x2)*(x1-x2) + (y1-y2)*(y1-y2);
  c2 = (p0.x-x2)*(p0.x-x2) + (p0.y-y2)*(p0.y-y2);
  cosx = (a2+b2-c2)/(2*sqrt(a2*b2));

  sinx = sqrt(1 - cosx*cosx);
  d = radius / ((1 - cosx) / sinx);

  anx = (x1-p0.x) / sqrt(a2);
  any = (y1-p0.y) / sqrt(a2);
  bnx = (x1-x2) / sqrt(b2);
  bny = (y1-y2) / sqrt(b2);
  x3 = x1 - anx*d;
  y3 = y1 - any*d;
  x4 = x1 - bnx*d;
  y4 = y1 - bny*d;
  anticlockwise = (dir < 0);
  cx = x3 + any*radius*(anticlockwise ? 1 : -1);
  cy = y3 - anx*radius*(anticlockwise ? 1 : -1);
  angle0 = atan2((y3-cy), (x3-cx));
  angle1 = atan2((y4-cy), (x4-cx));


  LineTo(x3, y3);

  Arc(cx, cy, radius, angle0, angle1, anticlockwise, error);
}

void
CanvasRenderingContext2D::Arc(double x, double y, double r,
                              double startAngle, double endAngle,
                              bool anticlockwise, ErrorResult& error)
{
  if (r < 0.0) {
    error.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return;
  }

  EnsureWritablePath();

  ArcToBezier(this, Point(x, y), r, startAngle, endAngle, anticlockwise);
}

void
CanvasRenderingContext2D::Rect(double x, double y, double w, double h)
{
  EnsureWritablePath();

  if (mPathBuilder) {
    mPathBuilder->MoveTo(Point(x, y));
    mPathBuilder->LineTo(Point(x + w, y));
    mPathBuilder->LineTo(Point(x + w, y + h));
    mPathBuilder->LineTo(Point(x, y + h));
    mPathBuilder->Close();
  } else {
    mDSPathBuilder->MoveTo(mTarget->GetTransform() * Point(x, y));
    mDSPathBuilder->LineTo(mTarget->GetTransform() * Point(x + w, y));
    mDSPathBuilder->LineTo(mTarget->GetTransform() * Point(x + w, y + h));
    mDSPathBuilder->LineTo(mTarget->GetTransform() * Point(x, y + h));
    mDSPathBuilder->Close();
  }
}

void
CanvasRenderingContext2D::EnsureWritablePath()
{
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

  EnsureTarget();
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
CanvasRenderingContext2D::EnsureUserSpacePath(const CanvasWindingRule& winding)
{
  FillRule fillRule = CurrentState().fillRule;
  if(winding == CanvasWindingRule::Evenodd)
    fillRule = FILL_EVEN_ODD;

  if (!mPath && !mPathBuilder && !mDSPathBuilder) {
    EnsureTarget();
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

/**
 * Helper function for SetFont that creates a style rule for the given font.
 * @param aFont The CSS font string
 * @param aNode The canvas element
 * @param aResult Pointer in which to place the new style rule.
 * @remark Assumes all pointer arguments are non-null.
 */
static nsresult
CreateFontStyleRule(const nsAString& aFont,
                    nsINode* aNode,
                    StyleRule** aResult)
{
  nsRefPtr<StyleRule> rule;
  bool changed;

  nsIPrincipal* principal = aNode->NodePrincipal();
  nsIDocument* document = aNode->OwnerDoc();

  nsIURI* docURL = document->GetDocumentURI();
  nsIURI* baseURL = document->GetDocBaseURI();

  // Pass the CSS Loader object to the parser, to allow parser error reports
  // to include the outer window ID.
  nsCSSParser parser(document->CSSLoader());

  nsresult rv = parser.ParseStyleAttribute(EmptyString(), docURL, baseURL,
                                           principal, getter_AddRefs(rule));
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = parser.ParseProperty(eCSSProperty_font, aFont, docURL, baseURL,
                            principal, rule->GetDeclaration(), &changed,
                            false);
  if (NS_FAILED(rv))
    return rv;

  rv = parser.ParseProperty(eCSSProperty_line_height,
                            NS_LITERAL_STRING("normal"), docURL, baseURL,
                            principal, rule->GetDeclaration(), &changed,
                            false);
  if (NS_FAILED(rv)) {
    return rv;
  }

  rule->RuleMatched();

  rule.forget(aResult);
  return NS_OK;
}

void
CanvasRenderingContext2D::SetFont(const nsAString& font,
                                  ErrorResult& error)
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
    error.Throw(NS_ERROR_FAILURE);
    return;
  }

  nsIPresShell* presShell = GetPresShell();
  if (!presShell) {
    error.Throw(NS_ERROR_FAILURE);
    return;
  }
  nsIDocument* document = presShell->GetDocument();

  nsRefPtr<css::StyleRule> rule;
  error = CreateFontStyleRule(font, document, getter_AddRefs(rule));

  if (error.Failed()) {
    return;
  }

  css::Declaration *declaration = rule->GetDeclaration();
  // The easiest way to see whether we got a syntax error or whether
  // we got 'inherit' or 'initial' is to look at font-size-adjust,
  // which the shorthand resets to either 'none' or
  // '-moz-system-font'.
  // We know the declaration is not !important, so we can use
  // GetNormalBlock().
  const nsCSSValue *fsaVal =
    declaration->GetNormalBlock()->ValueFor(eCSSProperty_font_size_adjust);
  if (!fsaVal || (fsaVal->GetUnit() != eCSSUnit_None &&
                  fsaVal->GetUnit() != eCSSUnit_System_Font)) {
      // We got an all-property value or a syntax error.  The spec says
      // this value must be ignored.
    return;
  }

  nsTArray< nsCOMPtr<nsIStyleRule> > rules;
  rules.AppendElement(rule);

  nsStyleSet* styleSet = presShell->StyleSet();

  // have to get a parent style context for inherit-like relative
  // values (2em, bolder, etc.)
  nsRefPtr<nsStyleContext> parentContext;

  if (mCanvasElement && mCanvasElement->IsInDoc()) {
      // inherit from the canvas element
      parentContext = nsComputedDOMStyle::GetStyleContextForElement(
              mCanvasElement,
              nullptr,
              presShell);
  } else {
    // otherwise inherit from default (10px sans-serif)
    nsRefPtr<css::StyleRule> parentRule;
    error = CreateFontStyleRule(NS_LITERAL_STRING("10px sans-serif"),
                                document,
                                getter_AddRefs(parentRule));

    if (error.Failed()) {
      return;
    }

    nsTArray< nsCOMPtr<nsIStyleRule> > parentRules;
    parentRules.AppendElement(parentRule);
    parentContext = styleSet->ResolveStyleForRules(nullptr, parentRules);
  }

  if (!parentContext) {
    error.Throw(NS_ERROR_FAILURE);
    return;
  }

  // add a rule to prevent text zoom from affecting the style
  rules.AppendElement(new nsDisableTextZoomStyleRule);

  nsRefPtr<nsStyleContext> sc =
      styleSet->ResolveStyleForRules(parentContext, rules);
  if (!sc) {
    error.Throw(NS_ERROR_FAILURE);
    return;
  }

  const nsStyleFont* fontStyle = sc->StyleFont();

  NS_ASSERTION(fontStyle, "Could not obtain font style");

  nsIAtom* language = sc->StyleFont()->mLanguage;
  if (!language) {
    language = presShell->GetPresContext()->GetLanguageFromCharset();
  }

  // use CSS pixels instead of dev pixels to avoid being affected by page zoom
  const uint32_t aupcp = nsPresContext::AppUnitsPerCSSPixel();

  bool printerFont = (presShell->GetPresContext()->Type() == nsPresContext::eContext_PrintPreview ||
                      presShell->GetPresContext()->Type() == nsPresContext::eContext_Print);

  // Purposely ignore the font size that respects the user's minimum
  // font preference (fontStyle->mFont.size) in favor of the computed
  // size (fontStyle->mSize).  See
  // https://bugzilla.mozilla.org/show_bug.cgi?id=698652.
  MOZ_ASSERT(!fontStyle->mAllowZoom,
             "expected text zoom to be disabled on this nsStyleFont");
  gfxFontStyle style(fontStyle->mFont.style,
                     fontStyle->mFont.weight,
                     fontStyle->mFont.stretch,
                     NSAppUnitsToFloatPixels(fontStyle->mSize, float(aupcp)),
                     language,
                     fontStyle->mFont.sizeAdjust,
                     fontStyle->mFont.systemFont,
                     printerFont,
                     fontStyle->mFont.languageOverride);

  fontStyle->mFont.AddFontFeaturesToStyle(&style);

  CurrentState().fontGroup =
      gfxPlatform::GetPlatform()->CreateFontGroup(fontStyle->mFont.name,
                                                  &style,
                                                  presShell->GetPresContext()->GetUserFontSet());
  NS_ASSERTION(CurrentState().fontGroup, "Could not get font group");

  // The font getter is required to be reserialized based on what we
  // parsed (including having line-height removed).  (Older drafts of
  // the spec required font sizes be converted to pixels, but that no
  // longer seems to be required.)
  declaration->GetValue(eCSSProperty_font, CurrentState().font);
}

void
CanvasRenderingContext2D::SetTextAlign(const nsAString& ta)
{
  if (ta.EqualsLiteral("start"))
    CurrentState().textAlign = TEXT_ALIGN_START;
  else if (ta.EqualsLiteral("end"))
    CurrentState().textAlign = TEXT_ALIGN_END;
  else if (ta.EqualsLiteral("left"))
    CurrentState().textAlign = TEXT_ALIGN_LEFT;
  else if (ta.EqualsLiteral("right"))
    CurrentState().textAlign = TEXT_ALIGN_RIGHT;
  else if (ta.EqualsLiteral("center"))
    CurrentState().textAlign = TEXT_ALIGN_CENTER;
}

void
CanvasRenderingContext2D::GetTextAlign(nsAString& ta)
{
  switch (CurrentState().textAlign)
  {
  case TEXT_ALIGN_START:
    ta.AssignLiteral("start");
    break;
  case TEXT_ALIGN_END:
    ta.AssignLiteral("end");
    break;
  case TEXT_ALIGN_LEFT:
    ta.AssignLiteral("left");
    break;
  case TEXT_ALIGN_RIGHT:
    ta.AssignLiteral("right");
    break;
  case TEXT_ALIGN_CENTER:
    ta.AssignLiteral("center");
    break;
  }
}

void
CanvasRenderingContext2D::SetTextBaseline(const nsAString& tb)
{
  if (tb.EqualsLiteral("top"))
    CurrentState().textBaseline = TEXT_BASELINE_TOP;
  else if (tb.EqualsLiteral("hanging"))
    CurrentState().textBaseline = TEXT_BASELINE_HANGING;
  else if (tb.EqualsLiteral("middle"))
    CurrentState().textBaseline = TEXT_BASELINE_MIDDLE;
  else if (tb.EqualsLiteral("alphabetic"))
    CurrentState().textBaseline = TEXT_BASELINE_ALPHABETIC;
  else if (tb.EqualsLiteral("ideographic"))
    CurrentState().textBaseline = TEXT_BASELINE_IDEOGRAPHIC;
  else if (tb.EqualsLiteral("bottom"))
    CurrentState().textBaseline = TEXT_BASELINE_BOTTOM;
}

void
CanvasRenderingContext2D::GetTextBaseline(nsAString& tb)
{
  switch (CurrentState().textBaseline)
  {
  case TEXT_BASELINE_TOP:
    tb.AssignLiteral("top");
    break;
  case TEXT_BASELINE_HANGING:
    tb.AssignLiteral("hanging");
    break;
  case TEXT_BASELINE_MIDDLE:
    tb.AssignLiteral("middle");
    break;
  case TEXT_BASELINE_ALPHABETIC:
    tb.AssignLiteral("alphabetic");
    break;
  case TEXT_BASELINE_IDEOGRAPHIC:
    tb.AssignLiteral("ideographic");
    break;
  case TEXT_BASELINE_BOTTOM:
    tb.AssignLiteral("bottom");
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
TextReplaceWhitespaceCharacters(nsAutoString& str)
{
  str.ReplaceChar("\x09\x0A\x0B\x0C\x0D", PRUnichar(' '));
}

void
CanvasRenderingContext2D::FillText(const nsAString& text, double x,
                                   double y,
                                   const Optional<double>& maxWidth,
                                   ErrorResult& error)
{
  error = DrawOrMeasureText(text, x, y, maxWidth, TEXT_DRAW_OPERATION_FILL, nullptr);
}

void
CanvasRenderingContext2D::StrokeText(const nsAString& text, double x,
                                     double y,
                                     const Optional<double>& maxWidth,
                                     ErrorResult& error)
{
  error = DrawOrMeasureText(text, x, y, maxWidth, TEXT_DRAW_OPERATION_STROKE, nullptr);
}

TextMetrics*
CanvasRenderingContext2D::MeasureText(const nsAString& rawText,
                                      ErrorResult& error)
{
  float width;
  Optional<double> maxWidth;
  error = DrawOrMeasureText(rawText, 0, 0, maxWidth, TEXT_DRAW_OPERATION_MEASURE, &width);
  if (error.Failed()) {
    return nullptr;
  }

  return new TextMetrics(width);
}

/**
 * Used for nsBidiPresUtils::ProcessText
 */
struct MOZ_STACK_CLASS CanvasBidiProcessor : public nsBidiPresUtils::BidiProcessor
{
  typedef CanvasRenderingContext2D::ContextState ContextState;

  virtual void SetText(const PRUnichar* text, int32_t length, nsBidiDirection direction)
  {
    mFontgrp->UpdateFontList(); // ensure user font generation is current
    mTextRun = mFontgrp->MakeTextRun(text,
                                     length,
                                     mThebes,
                                     mAppUnitsPerDevPixel,
                                     direction==NSBIDI_RTL ? gfxTextRunFactory::TEXT_IS_RTL : 0);
  }

  virtual nscoord GetWidth()
  {
    gfxTextRun::Metrics textRunMetrics = mTextRun->MeasureText(0,
                                                               mTextRun->GetLength(),
                                                               mDoMeasureBoundingBox ?
                                                                 gfxFont::TIGHT_INK_EXTENTS :
                                                                 gfxFont::LOOSE_INK_EXTENTS,
                                                               mThebes,
                                                               nullptr);

    // this only measures the height; the total width is gotten from the
    // the return value of ProcessText.
    if (mDoMeasureBoundingBox) {
      textRunMetrics.mBoundingBox.Scale(1.0 / mAppUnitsPerDevPixel);
      mBoundingBox = mBoundingBox.Union(textRunMetrics.mBoundingBox);
    }

    return NSToCoordRound(textRunMetrics.mAdvanceWidth);
  }

  virtual void DrawText(nscoord xOffset, nscoord width)
  {
    gfxPoint point = mPt;
    point.x += xOffset;

    // offset is given in terms of left side of string
    if (mTextRun->IsRightToLeft()) {
      // Bug 581092 - don't use rounded pixel width to advance to
      // right-hand end of run, because this will cause different
      // glyph positioning for LTR vs RTL drawing of the same
      // glyph string on OS X and DWrite where textrun widths may
      // involve fractional pixels.
      gfxTextRun::Metrics textRunMetrics =
        mTextRun->MeasureText(0,
                              mTextRun->GetLength(),
                              mDoMeasureBoundingBox ?
                                  gfxFont::TIGHT_INK_EXTENTS :
                                  gfxFont::LOOSE_INK_EXTENTS,
                              mThebes,
                              nullptr);
      point.x += textRunMetrics.mAdvanceWidth;
      // old code was:
      //   point.x += width * mAppUnitsPerDevPixel;
      // TODO: restore this if/when we move to fractional coords
      // throughout the text layout process
    }

    uint32_t numRuns;
    const gfxTextRun::GlyphRun *runs = mTextRun->GetGlyphRuns(&numRuns);
    const int32_t appUnitsPerDevUnit = mAppUnitsPerDevPixel;
    const double devUnitsPerAppUnit = 1.0/double(appUnitsPerDevUnit);
    Point baselineOrigin =
      Point(point.x * devUnitsPerAppUnit, point.y * devUnitsPerAppUnit);

    float advanceSum = 0;

    mCtx->EnsureTarget();
    for (uint32_t c = 0; c < numRuns; c++) {
      gfxFont *font = runs[c].mFont;
      uint32_t endRun = 0;
      if (c + 1 < numRuns) {
        endRun = runs[c + 1].mCharacterOffset;
      } else {
        endRun = mTextRun->GetLength();
      }

      const gfxTextRun::CompressedGlyph *glyphs = mTextRun->GetCharacterGlyphs();

      RefPtr<ScaledFont> scaledFont =
        gfxPlatform::GetPlatform()->GetScaledFontForFont(mCtx->mTarget, font);

      if (!scaledFont) {
        // This can occur when something switched DirectWrite off.
        return;
      }

      GlyphBuffer buffer;

      std::vector<Glyph> glyphBuf;

      for (uint32_t i = runs[c].mCharacterOffset; i < endRun; i++) {
        Glyph newGlyph;
        if (glyphs[i].IsSimpleGlyph()) {
          newGlyph.mIndex = glyphs[i].GetSimpleGlyph();
          if (mTextRun->IsRightToLeft()) {
            newGlyph.mPosition.x = baselineOrigin.x - advanceSum -
              glyphs[i].GetSimpleAdvance() * devUnitsPerAppUnit;
          } else {
            newGlyph.mPosition.x = baselineOrigin.x + advanceSum;
          }
          newGlyph.mPosition.y = baselineOrigin.y;
          advanceSum += glyphs[i].GetSimpleAdvance() * devUnitsPerAppUnit;
          glyphBuf.push_back(newGlyph);
          continue;
        }

        if (!glyphs[i].GetGlyphCount()) {
          continue;
        }

        gfxTextRun::DetailedGlyph *detailedGlyphs =
          mTextRun->GetDetailedGlyphs(i);

        if (glyphs[i].IsMissing()) {
          newGlyph.mIndex = 0;
          if (mTextRun->IsRightToLeft()) {
            newGlyph.mPosition.x = baselineOrigin.x - advanceSum -
              detailedGlyphs[0].mAdvance * devUnitsPerAppUnit;
          } else {
            newGlyph.mPosition.x = baselineOrigin.x + advanceSum;
          }
          newGlyph.mPosition.y = baselineOrigin.y;
          advanceSum += detailedGlyphs[0].mAdvance * devUnitsPerAppUnit;
          glyphBuf.push_back(newGlyph);
          continue;
        }

        for (uint32_t c = 0; c < glyphs[i].GetGlyphCount(); c++) {
          newGlyph.mIndex = detailedGlyphs[c].mGlyphID;
          if (mTextRun->IsRightToLeft()) {
            newGlyph.mPosition.x = baselineOrigin.x + detailedGlyphs[c].mXOffset * devUnitsPerAppUnit -
              advanceSum - detailedGlyphs[c].mAdvance * devUnitsPerAppUnit;
          } else {
            newGlyph.mPosition.x = baselineOrigin.x + detailedGlyphs[c].mXOffset * devUnitsPerAppUnit + advanceSum;
          }
          newGlyph.mPosition.y = baselineOrigin.y + detailedGlyphs[c].mYOffset * devUnitsPerAppUnit;
          glyphBuf.push_back(newGlyph);
          advanceSum += detailedGlyphs[c].mAdvance * devUnitsPerAppUnit;
        }
      }

      if (!glyphBuf.size()) {
        // This may happen for glyph runs for a 0 size font.
        continue;
      }

      buffer.mGlyphs = &glyphBuf.front();
      buffer.mNumGlyphs = glyphBuf.size();

      Rect bounds = mCtx->mTarget->GetTransform().
        TransformBounds(Rect(mBoundingBox.x, mBoundingBox.y,
                             mBoundingBox.width, mBoundingBox.height));
      if (mOp == CanvasRenderingContext2D::TEXT_DRAW_OPERATION_FILL) {
        AdjustedTarget(mCtx, &bounds)->
          FillGlyphs(scaledFont, buffer,
                     CanvasGeneralPattern().
                       ForStyle(mCtx, CanvasRenderingContext2D::STYLE_FILL, mCtx->mTarget),
                     DrawOptions(mState->globalAlpha, mCtx->UsedOperation()));
      } else if (mOp == CanvasRenderingContext2D::TEXT_DRAW_OPERATION_STROKE) {
        // stroke glyphs one at a time to avoid poor CoreGraphics performance
        // when stroking a path with a very large number of points
        buffer.mGlyphs = &glyphBuf.front();
        buffer.mNumGlyphs = 1;
        const ContextState& state = *mState;
        AdjustedTarget target(mCtx, &bounds);
        const StrokeOptions strokeOpts(state.lineWidth, state.lineJoin,
                                       state.lineCap, state.miterLimit,
                                       state.dash.Length(),
                                       state.dash.Elements(),
                                       state.dashOffset);
        CanvasGeneralPattern cgp;
        const Pattern& patForStyle
          (cgp.ForStyle(mCtx, CanvasRenderingContext2D::STYLE_STROKE, mCtx->mTarget));
        const DrawOptions drawOpts(state.globalAlpha, mCtx->UsedOperation());

        for (unsigned i = glyphBuf.size(); i > 0; --i) {
          RefPtr<Path> path = scaledFont->GetPathForGlyphs(buffer, mCtx->mTarget);
          target->Stroke(path, patForStyle, strokeOpts, drawOpts);
          buffer.mGlyphs++;
        }
      }
    }
  }

  // current text run
  nsAutoPtr<gfxTextRun> mTextRun;

  // pointer to a screen reference context used to measure text and such
  nsRefPtr<gfxContext> mThebes;

  // Pointer to the draw target we should fill our text to
  CanvasRenderingContext2D *mCtx;

  // position of the left side of the string, alphabetic baseline
  gfxPoint mPt;

  // current font
  gfxFontGroup* mFontgrp;

  // dev pixel conversion factor
  int32_t mAppUnitsPerDevPixel;

  // operation (fill or stroke)
  CanvasRenderingContext2D::TextDrawOperation mOp;

  // context state
  ContextState *mState;

  // union of bounding boxes of all runs, needed for shadows
  gfxRect mBoundingBox;

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

  // spec isn't clear on what should happen if aMaxWidth <= 0, so
  // treat it as an invalid argument
  // technically, 0 should be an invalid value as well, but 0 is the default
  // arg, and there is no way to tell if the default was used
  if (aMaxWidth.WasPassed() && aMaxWidth.Value() < 0)
    return NS_ERROR_INVALID_ARG;

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

  // for now, default to ltr if not in doc
  bool isRTL = false;

  if (mCanvasElement && mCanvasElement->IsInDoc()) {
    // try to find the closest context
    nsRefPtr<nsStyleContext> canvasStyle =
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
  NS_ASSERTION(currentFontStyle, "font group is null");

  if (currentFontStyle->GetStyle()->size == 0.0F) {
    if (aWidth) {
      *aWidth = 0;
    }
    return NS_OK;
  }

  const ContextState &state = CurrentState();

  // This is only needed to know if we can know the drawing bounding box easily.
  bool doDrawShadow = NeedToDrawShadow();

  CanvasBidiProcessor processor;

  GetAppUnitsValues(&processor.mAppUnitsPerDevPixel, nullptr);
  processor.mPt = gfxPoint(aX, aY);
  processor.mThebes =
    new gfxContext(gfxPlatform::GetPlatform()->ScreenReferenceSurface());

  // If we don't have a target then we don't have a transform. A target won't
  // be needed in the case where we're measuring the text size. This allows
  // to avoid creating a target if it's only being used to measure text sizes.
  if (mTarget) {
    Matrix matrix = mTarget->GetTransform();
    processor.mThebes->SetMatrix(gfxMatrix(matrix._11, matrix._12, matrix._21, matrix._22, matrix._31, matrix._32));
  }
  processor.mCtx = this;
  processor.mOp = aOp;
  processor.mBoundingBox = gfxRect(0, 0, 0, 0);
  processor.mDoMeasureBoundingBox = doDrawShadow || !mIsEntireFrameInvalid;
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
  if (aOp==TEXT_DRAW_OPERATION_MEASURE) {
    return NS_OK;
  }

  // offset pt.x based on text align
  gfxFloat anchorX;

  if (state.textAlign == TEXT_ALIGN_CENTER) {
    anchorX = .5;
  } else if (state.textAlign == TEXT_ALIGN_LEFT ||
            (!isRTL && state.textAlign == TEXT_ALIGN_START) ||
            (isRTL && state.textAlign == TEXT_ALIGN_END)) {
    anchorX = 0;
  } else {
    anchorX = 1;
  }

  processor.mPt.x -= anchorX * totalWidth;

  // offset pt.y based on text baseline
  processor.mFontgrp->UpdateFontList(); // ensure user font generation is current
  NS_ASSERTION(processor.mFontgrp->FontListLength()>0, "font group contains no fonts");
  const gfxFont::Metrics& fontMetrics = processor.mFontgrp->GetFontAt(0)->GetMetrics();

  gfxFloat anchorY;

  switch (state.textBaseline)
  {
  case TEXT_BASELINE_HANGING:
      // fall through; best we can do with the information available
  case TEXT_BASELINE_TOP:
    anchorY = fontMetrics.emAscent;
    break;
  case TEXT_BASELINE_MIDDLE:
    anchorY = (fontMetrics.emAscent - fontMetrics.emDescent) * .5f;
    break;
  case TEXT_BASELINE_IDEOGRAPHIC:
    // fall through; best we can do with the information available
  case TEXT_BASELINE_ALPHABETIC:
    anchorY = 0;
    break;
  case TEXT_BASELINE_BOTTOM:
    anchorY = -fontMetrics.emDescent;
    break;
  default:
    MOZ_CRASH("unexpected TextBaseline");
  }

  processor.mPt.y += anchorY;

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
    newTransform.Translate(aX, 0);
    newTransform.Scale(aMaxWidth.Value() / totalWidth, 1);
    newTransform.Translate(-aX, 0);
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

  if (aOp == CanvasRenderingContext2D::TEXT_DRAW_OPERATION_FILL &&
      !doDrawShadow) {
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
    SetFont(kDefaultFontStyle, err);
    if (err.Failed()) {
      gfxFontStyle style;
      style.size = kDefaultFontSize;
      CurrentState().fontGroup =
        gfxPlatform::GetPlatform()->CreateFontGroup(NS_LITERAL_STRING("sans-serif"),
                                                    &style,
                                                    nullptr);
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
CanvasRenderingContext2D::SetLineCap(const nsAString& capstyle)
{
  CapStyle cap;

  if (capstyle.EqualsLiteral("butt")) {
    cap = CAP_BUTT;
  } else if (capstyle.EqualsLiteral("round")) {
    cap = CAP_ROUND;
  } else if (capstyle.EqualsLiteral("square")) {
    cap = CAP_SQUARE;
  } else {
    // XXX ERRMSG we need to report an error to developers here! (bug 329026)
    return;
  }

  CurrentState().lineCap = cap;
}

void
CanvasRenderingContext2D::GetLineCap(nsAString& capstyle)
{
  switch (CurrentState().lineCap) {
  case CAP_BUTT:
    capstyle.AssignLiteral("butt");
    break;
  case CAP_ROUND:
    capstyle.AssignLiteral("round");
    break;
  case CAP_SQUARE:
    capstyle.AssignLiteral("square");
    break;
  }
}

void
CanvasRenderingContext2D::SetLineJoin(const nsAString& joinstyle)
{
  JoinStyle j;

  if (joinstyle.EqualsLiteral("round")) {
    j = JOIN_ROUND;
  } else if (joinstyle.EqualsLiteral("bevel")) {
    j = JOIN_BEVEL;
  } else if (joinstyle.EqualsLiteral("miter")) {
    j = JOIN_MITER_OR_BEVEL;
  } else {
    // XXX ERRMSG we need to report an error to developers here! (bug 329026)
    return;
  }

  CurrentState().lineJoin = j;
}

void
CanvasRenderingContext2D::GetLineJoin(nsAString& joinstyle, ErrorResult& error)
{
  switch (CurrentState().lineJoin) {
  case JOIN_ROUND:
    joinstyle.AssignLiteral("round");
    break;
  case JOIN_BEVEL:
    joinstyle.AssignLiteral("bevel");
    break;
  case JOIN_MITER_OR_BEVEL:
    joinstyle.AssignLiteral("miter");
    break;
  default:
    error.Throw(NS_ERROR_FAILURE);
  }
}

void
CanvasRenderingContext2D::SetMozDash(JSContext* cx,
                                     const JS::Value& mozDash,
                                     ErrorResult& error)
{
  FallibleTArray<Float> dash;
  error = JSValToDashArray(cx, mozDash, dash);
  if (!error.Failed()) {
    ContextState& state = CurrentState();
    state.dash = dash;
    if (state.dash.IsEmpty()) {
      state.dashOffset = 0;
    }
  }
}

JS::Value
CanvasRenderingContext2D::GetMozDash(JSContext* cx, ErrorResult& error)
{
  return DashArrayToJSVal(CurrentState().dash, cx, error);
}

void
CanvasRenderingContext2D::SetMozDashOffset(double mozDashOffset)
{
  ContextState& state = CurrentState();
  if (!state.dash.IsEmpty()) {
    state.dashOffset = mozDashOffset;
  }
}

bool
CanvasRenderingContext2D::IsPointInPath(double x, double y, const CanvasWindingRule& winding)
{
  if (!FloatValidate(x,y)) {
    return false;
  }

  EnsureUserSpacePath(winding);
  if (!mPath) {
    return false;
  }

  if (mPathTransformWillUpdate) {
    return mPath->ContainsPoint(Point(x, y), mPathToDS);
  }

  return mPath->ContainsPoint(Point(x, y), mTarget->GetTransform());
}

bool
CanvasRenderingContext2D::IsPointInStroke(double x, double y)
{
  if (!FloatValidate(x,y)) {
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
    return mPath->StrokeContainsPoint(strokeOptions, Point(x, y), mPathToDS);
  }
  return mPath->StrokeContainsPoint(strokeOptions, Point(x, y), mTarget->GetTransform());
}

//
// image
//

// drawImage(in HTMLImageElement image, in float dx, in float dy);
//   -- render image from 0,0 at dx,dy top-left coords
// drawImage(in HTMLImageElement image, in float dx, in float dy, in float sw, in float sh);
//   -- render image from 0,0 at dx,dy top-left coords clipping it to sw,sh
// drawImage(in HTMLImageElement image, in float sx, in float sy, in float sw, in float sh, in float dx, in float dy, in float dw, in float dh);
//   -- render the region defined by (sx,sy,sw,wh) in image-local space into the region (dx,dy,dw,dh) on the canvas

// If only dx and dy are passed in then optional_argc should be 0. If only
// dx, dy, dw and dh are passed in then optional_argc should be 2. The only
// other valid value for optional_argc is 6 if sx, sy, sw, sh, dx, dy, dw and dh
// are all passed in.

void
CanvasRenderingContext2D::DrawImage(const HTMLImageOrCanvasOrVideoElement& image,
                                    double sx, double sy, double sw,
                                    double sh, double dx, double dy,
                                    double dw, double dh,
                                    uint8_t optional_argc,
                                    ErrorResult& error)
{
  MOZ_ASSERT(optional_argc == 0 || optional_argc == 2 || optional_argc == 6);

  RefPtr<SourceSurface> srcSurf;
  gfxIntSize imgSize;

  Element* element;

  EnsureTarget();
  if (image.IsHTMLCanvasElement()) {
    HTMLCanvasElement* canvas = &image.GetAsHTMLCanvasElement();
    element = canvas;
    nsIntSize size = canvas->GetSize();
    if (size.width == 0 || size.height == 0) {
      error.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
      return;
    }

    // Special case for Canvas, which could be an Azure canvas!
    nsICanvasRenderingContextInternal *srcCanvas = canvas->GetContextAtIndex(0);
    if (srcCanvas == this) {
      // Self-copy.
      srcSurf = mTarget->Snapshot();
      imgSize = gfxIntSize(mWidth, mHeight);
    } else if (srcCanvas) {
      // This might not be an Azure canvas!
      srcSurf = srcCanvas->GetSurfaceSnapshot();

      if (srcSurf) {
        if (mCanvasElement) {
          // Do security check here.
          CanvasUtils::DoDrawImageSecurityCheck(mCanvasElement,
                                                element->NodePrincipal(),
                                                canvas->IsWriteOnly(),
                                                false);
        }
        imgSize = gfxIntSize(srcSurf->GetSize().width, srcSurf->GetSize().height);
      }
    }
  } else {
    if (image.IsHTMLImageElement()) {
      HTMLImageElement* img = &image.GetAsHTMLImageElement();
      element = img;
    } else {
      HTMLVideoElement* video = &image.GetAsHTMLVideoElement();
      element = video;
    }

    gfxASurface* imgsurf =
      CanvasImageCache::Lookup(element, mCanvasElement, &imgSize);
    if (imgsurf) {
      srcSurf = gfxPlatform::GetPlatform()->GetSourceSurfaceForSurface(mTarget, imgsurf);
    }
  }

  if (!srcSurf) {
    // The canvas spec says that drawImage should draw the first frame
    // of animated images
    uint32_t sfeFlags = nsLayoutUtils::SFE_WANT_FIRST_FRAME;
    nsLayoutUtils::SurfaceFromElementResult res =
      nsLayoutUtils::SurfaceFromElement(element, sfeFlags);

    if (!res.mSurface) {
      // Spec says to silently do nothing if the element is still loading.
      if (!res.mIsStillLoading) {
        error.Throw(NS_ERROR_NOT_AVAILABLE);
      }
      return;
    }

    // Ignore cairo surfaces that are bad! See bug 666312.
    if (res.mSurface->CairoStatus()) {
      return;
    }

    imgSize = res.mSize;

    if (mCanvasElement) {
      CanvasUtils::DoDrawImageSecurityCheck(mCanvasElement,
                                            res.mPrincipal, res.mIsWriteOnly,
                                            res.mCORSUsed);
    }

    if (res.mImageRequest) {
      CanvasImageCache::NotifyDrawImage(element, mCanvasElement,
                                        res.mImageRequest, res.mSurface, imgSize);
    }

    srcSurf = gfxPlatform::GetPlatform()->GetSourceSurfaceForSurface(mTarget, res.mSurface);
  }

  if (optional_argc == 0) {
    sx = sy = 0.0;
    dw = sw = (double) imgSize.width;
    dh = sh = (double) imgSize.height;
  } else if (optional_argc == 2) {
    sx = sy = 0.0;
    sw = (double) imgSize.width;
    sh = (double) imgSize.height;
  }

  if (sw == 0.0 || sh == 0.0) {
    error.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return;
  }

  if (dw == 0.0 || dh == 0.0) {
    // not really failure, but nothing to do --
    // and noone likes a divide-by-zero
    return;
  }

  if (sx < 0.0 || sy < 0.0 ||
      sw < 0.0 || sw > (double) imgSize.width ||
      sh < 0.0 || sh > (double) imgSize.height ||
      dw < 0.0 || dh < 0.0) {
    // XXX - Unresolved spec issues here, for now return error.
    error.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return;
  }

  Filter filter;

  if (CurrentState().imageSmoothingEnabled)
    filter = mgfx::FILTER_LINEAR;
  else
    filter = mgfx::FILTER_POINT;

  mgfx::Rect bounds;

  if (NeedToDrawShadow()) {
    bounds = mgfx::Rect(dx, dy, dw, dh);
    bounds = mTarget->GetTransform().TransformBounds(bounds);
  }

  AdjustedTarget(this, bounds.IsEmpty() ? nullptr : &bounds)->
    DrawSurface(srcSurf,
                mgfx::Rect(dx, dy, dw, dh),
                mgfx::Rect(sx, sy, sw, sh),
                DrawSurfaceOptions(filter),
                DrawOptions(CurrentState().globalAlpha, UsedOperation()));

  RedrawUser(gfxRect(dx, dy, dw, dh));
}

#ifdef USE_SKIA_GPU
static bool
IsStandardCompositeOp(CompositionOp op)
{
    return (op == OP_SOURCE ||
            op == OP_ATOP ||
            op == OP_IN ||
            op == OP_OUT ||
            op == OP_OVER ||
            op == OP_DEST_IN ||
            op == OP_DEST_OUT ||
            op == OP_DEST_OVER ||
            op == OP_DEST_ATOP ||
            op == OP_ADD ||
            op == OP_XOR);
}
#endif

void
CanvasRenderingContext2D::SetGlobalCompositeOperation(const nsAString& op,
                                                      ErrorResult& error)
{
  CompositionOp comp_op;

#define CANVAS_OP_TO_GFX_OP(cvsop, op2d) \
  if (op.EqualsLiteral(cvsop))   \
    comp_op = OP_##op2d;

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

#ifdef USE_SKIA_GPU
  if (!IsStandardCompositeOp(comp_op)) {
    Demote();
  }
#endif

#undef CANVAS_OP_TO_GFX_OP
  CurrentState().op = comp_op;
}

void
CanvasRenderingContext2D::GetGlobalCompositeOperation(nsAString& op,
                                                      ErrorResult& error)
{
  CompositionOp comp_op = CurrentState().op;

#define CANVAS_OP_TO_GFX_OP(cvsop, op2d) \
  if (comp_op == OP_##op2d) \
    op.AssignLiteral(cvsop);

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
    error.Throw(NS_ERROR_FAILURE);
  }

#ifdef USE_SKIA_GPU
  if (!IsStandardCompositeOp(comp_op)) {
    Demote();
  }
#endif

#undef CANVAS_OP_TO_GFX_OP
}

void
CanvasRenderingContext2D::DrawWindow(nsIDOMWindow* window, double x,
                                     double y, double w, double h,
                                     const nsAString& bgColor,
                                     uint32_t flags, ErrorResult& error)
{
  // protect against too-large surfaces that will cause allocation
  // or overflow issues
  if (!gfxASurface::CheckSurfaceSize(gfxIntSize(int32_t(w), int32_t(h)),
                                     0xffff)) {
    error.Throw(NS_ERROR_FAILURE);
    return;
  }

  EnsureTarget();
  // We can't allow web apps to call this until we fix at least the
  // following potential security issues:
  // -- rendering cross-domain IFRAMEs and then extracting the results
  // -- rendering the user's theme and then extracting the results
  // -- rendering native anonymous content (e.g., file input paths;
  // scrollbars should be allowed)
  if (!nsContentUtils::IsCallerChrome()) {
    // not permitted to use DrawWindow
    // XXX ERRMSG we need to report an error to developers here! (bug 329026)
    error.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  // Flush layout updates
  if (!(flags & nsIDOMCanvasRenderingContext2D::DRAWWINDOW_DO_NOT_FLUSH)) {
    nsContentUtils::FlushLayoutForTree(window);
  }

  nsRefPtr<nsPresContext> presContext;
  nsCOMPtr<nsPIDOMWindow> win = do_QueryInterface(window);
  if (win) {
    nsIDocShell* docshell = win->GetDocShell();
    if (docshell) {
      docshell->GetPresContext(getter_AddRefs(presContext));
    }
  }
  if (!presContext) {
    error.Throw(NS_ERROR_FAILURE);
    return;
  }

  nscolor backgroundColor;
  if (!ParseColor(bgColor, &backgroundColor)) {
    error.Throw(NS_ERROR_FAILURE);
    return;
  }

  nsRect r(nsPresContext::CSSPixelsToAppUnits((float)x),
           nsPresContext::CSSPixelsToAppUnits((float)y),
           nsPresContext::CSSPixelsToAppUnits((float)w),
           nsPresContext::CSSPixelsToAppUnits((float)h));
  uint32_t renderDocFlags = (nsIPresShell::RENDER_IGNORE_VIEWPORT_SCROLLING |
                             nsIPresShell::RENDER_DOCUMENT_RELATIVE);
  if (flags & nsIDOMCanvasRenderingContext2D::DRAWWINDOW_DRAW_CARET) {
    renderDocFlags |= nsIPresShell::RENDER_CARET;
  }
  if (flags & nsIDOMCanvasRenderingContext2D::DRAWWINDOW_DRAW_VIEW) {
    renderDocFlags &= ~(nsIPresShell::RENDER_IGNORE_VIEWPORT_SCROLLING |
                        nsIPresShell::RENDER_DOCUMENT_RELATIVE);
  }
  if (flags & nsIDOMCanvasRenderingContext2D::DRAWWINDOW_USE_WIDGET_LAYERS) {
    renderDocFlags |= nsIPresShell::RENDER_USE_WIDGET_LAYERS;
  }
  if (flags & nsIDOMCanvasRenderingContext2D::DRAWWINDOW_ASYNC_DECODE_IMAGES) {
    renderDocFlags |= nsIPresShell::RENDER_ASYNC_DECODE_IMAGES;
  }
  if (flags & nsIDOMCanvasRenderingContext2D::DRAWWINDOW_DO_NOT_FLUSH) {
    renderDocFlags |= nsIPresShell::RENDER_DRAWWINDOW_NOT_FLUSHING;
  }

  // gfxContext-over-Azure may modify the DrawTarget's transform, so
  // save and restore it
  Matrix matrix = mTarget->GetTransform();
  double sw = matrix._11 * w;
  double sh = matrix._22 * h;
  nsRefPtr<gfxContext> thebes;
  nsRefPtr<gfxASurface> drawSurf;
  RefPtr<DrawTarget> drawDT;
  if (gfxPlatform::GetPlatform()->SupportsAzureContentForDrawTarget(mTarget)) {
    thebes = new gfxContext(mTarget);
    thebes->SetMatrix(gfxMatrix(matrix._11, matrix._12, matrix._21,
                                matrix._22, matrix._31, matrix._32));
  } else if (gfxPlatform::GetPlatform()->SupportsAzureContent()) {
    drawDT =
      gfxPlatform::GetPlatform()->CreateOffscreenContentDrawTarget(IntSize(ceil(sw), ceil(sh)),
                                                                   FORMAT_B8G8R8A8);
    if (!drawDT) {
      error.Throw(NS_ERROR_FAILURE);
      return;
    }

    thebes = new gfxContext(drawDT);
    thebes->Scale(matrix._11, matrix._22);
  } else {
    drawSurf =
      gfxPlatform::GetPlatform()->CreateOffscreenSurface(gfxIntSize(ceil(sw), ceil(sh)),
                                                         GFX_CONTENT_COLOR_ALPHA);
    if (!drawSurf) {
      error.Throw(NS_ERROR_FAILURE);
      return;
    }

    thebes = new gfxContext(drawSurf);
    thebes->Scale(matrix._11, matrix._22);
  }

  nsCOMPtr<nsIPresShell> shell = presContext->PresShell();
  unused << shell->RenderDocument(r, renderDocFlags, backgroundColor, thebes);
  if (drawSurf || drawDT) {
    RefPtr<SourceSurface> source;

    if (drawSurf) {
      gfxIntSize size = drawSurf->GetSize();

      drawSurf->SetDeviceOffset(gfxPoint(0, 0));
      nsRefPtr<gfxImageSurface> img = drawSurf->GetAsReadableARGB32ImageSurface();
      if (!img || !img->Data()) {
        error.Throw(NS_ERROR_FAILURE);
        return;
      }

      source =
        mTarget->CreateSourceSurfaceFromData(img->Data(),
                                             IntSize(size.width, size.height),
                                             img->Stride(),
                                             FORMAT_B8G8R8A8);
    } else {
      RefPtr<SourceSurface> snapshot = drawDT->Snapshot();
      RefPtr<DataSourceSurface> data = snapshot->GetDataSurface();

      source =
        mTarget->CreateSourceSurfaceFromData(data->GetData(),
                                             data->GetSize(),
                                             data->Stride(),
                                             data->GetFormat());
    }

    mgfx::Rect destRect(0, 0, w, h);
    mgfx::Rect sourceRect(0, 0, sw, sh);
    mTarget->DrawSurface(source, destRect, sourceRect,
                         DrawSurfaceOptions(mgfx::FILTER_POINT),
                         DrawOptions(1.0f, OP_SOURCE, AA_NONE));
    mTarget->Flush();
  } else {
    mTarget->SetTransform(matrix);
  }

  // note that x and y are coordinates in the document that
  // we're drawing; x and y are drawn to 0,0 in current user
  // space.
  RedrawUser(gfxRect(0, 0, w, h));
}

void
CanvasRenderingContext2D::AsyncDrawXULElement(nsXULElement& elem,
                                              double x, double y,
                                              double w, double h,
                                              const nsAString& bgColor,
                                              uint32_t flags,
                                              ErrorResult& error)
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
    error.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

#if 0
  nsCOMPtr<nsIFrameLoaderOwner> loaderOwner = do_QueryInterface(&elem);
  if (!loaderOwner) {
    error.Throw(NS_ERROR_FAILURE);
    return;
  }

  nsRefPtr<nsFrameLoader> frameloader = loaderOwner->GetFrameLoader();
  if (!frameloader) {
    error.Throw(NS_ERROR_FAILURE);
    return;
  }

  PBrowserParent *child = frameloader->GetRemoteBrowser();
  if (!child) {
    nsCOMPtr<nsIDOMWindow> window =
      do_GetInterface(frameloader->GetExistingDocShell());
    if (!window) {
      error.Throw(NS_ERROR_FAILURE);
      return;
    }

    return DrawWindow(window, x, y, w, h, bgColor, flags);
  }

  // protect against too-large surfaces that will cause allocation
  // or overflow issues
  if (!gfxASurface::CheckSurfaceSize(gfxIntSize(w, h), 0xffff)) {
    error.Throw(NS_ERROR_FAILURE);
    return;
  }

  bool flush =
    (flags & nsIDOMCanvasRenderingContext2D::DRAWWINDOW_DO_NOT_FLUSH) == 0;

  uint32_t renderDocFlags = nsIPresShell::RENDER_IGNORE_VIEWPORT_SCROLLING;
  if (flags & nsIDOMCanvasRenderingContext2D::DRAWWINDOW_DRAW_CARET) {
    renderDocFlags |= nsIPresShell::RENDER_CARET;
  }
  if (flags & nsIDOMCanvasRenderingContext2D::DRAWWINDOW_DRAW_VIEW) {
    renderDocFlags &= ~nsIPresShell::RENDER_IGNORE_VIEWPORT_SCROLLING;
  }

  nsRect rect(nsPresContext::CSSPixelsToAppUnits(x),
              nsPresContext::CSSPixelsToAppUnits(y),
              nsPresContext::CSSPixelsToAppUnits(w),
              nsPresContext::CSSPixelsToAppUnits(h));
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
                                       double aSh, ErrorResult& error)
{
  EnsureTarget();
  if (!IsTargetValid()) {
    error.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  if (!mCanvasElement && !mDocShell) {
    NS_ERROR("No canvas element and no docshell in GetImageData!!!");
    error.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return nullptr;
  }

  // Check only if we have a canvas element; if we were created with a docshell,
  // then it's special internal use.
  if (mCanvasElement && mCanvasElement->IsWriteOnly() &&
      !nsContentUtils::IsCallerChrome())
  {
    // XXX ERRMSG we need to report an error to developers here! (bug 329026)
    error.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return nullptr;
  }

  if (!NS_finite(aSx) || !NS_finite(aSy) ||
      !NS_finite(aSw) || !NS_finite(aSh)) {
    error.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
    return nullptr;
  }

  if (!aSw || !aSh) {
    error.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return nullptr;
  }

  int32_t x = JS_DoubleToInt32(aSx);
  int32_t y = JS_DoubleToInt32(aSy);
  int32_t wi = JS_DoubleToInt32(aSw);
  int32_t hi = JS_DoubleToInt32(aSh);

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
  error = GetImageDataArray(aCx, x, y, w, h, array.address());
  if (error.Failed()) {
    return nullptr;
  }
  MOZ_ASSERT(array);

  nsRefPtr<ImageData> imageData = new ImageData(w, h, *array);
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

  IntRect srcRect(0, 0, mWidth, mHeight);
  IntRect destRect(aX, aY, aWidth, aHeight);
  IntRect srcReadRect = srcRect.Intersect(destRect);
  RefPtr<DataSourceSurface> readback;
  if (!srcReadRect.IsEmpty() && !mZero) {
    RefPtr<SourceSurface> snapshot = mTarget->Snapshot();
    if (snapshot) {
      readback = snapshot->GetDataSurface();
    }
    if (!readback || !readback->GetData()) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  JS::Rooted<JSObject*> darray(aCx, JS_NewUint8ClampedArray(aCx, len.value()));
  if (!darray) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  if (mZero) {
    *aRetval = darray;
    return NS_OK;
  }

  uint8_t* data = JS_GetUint8ClampedArrayData(darray);

  IntRect dstWriteRect = srcReadRect;
  dstWriteRect.MoveBy(-aX, -aY);

  uint8_t* src = data;
  uint32_t srcStride = aWidth * 4;
  if (readback) {
    srcStride = readback->Stride();
    src = readback->GetData() + srcReadRect.y * srcStride + srcReadRect.x * 4;
  }

  // NOTE! dst is the same as src, and this relies on reading
  // from src and advancing that ptr before writing to dst.
  // NOTE! I'm not sure that it is, I think this comment might have been
  // inherited from Thebes canvas and is no longer true
  uint8_t* dst = data + dstWriteRect.y * (aWidth * 4) + dstWriteRect.x * 4;

  for (int32_t j = 0; j < dstWriteRect.height; ++j) {
    for (int32_t i = 0; i < dstWriteRect.width; ++i) {
      // XXX Is there some useful swizzle MMX we can use here?
#ifdef IS_LITTLE_ENDIAN
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

  *aRetval = darray;
  return NS_OK;
}

void
CanvasRenderingContext2D::EnsureErrorTarget()
{
  if (sErrorTarget) {
    return;
  }

  RefPtr<DrawTarget> errorTarget = gfxPlatform::GetPlatform()->CreateOffscreenCanvasDrawTarget(IntSize(1, 1), FORMAT_B8G8R8A8);
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
CanvasRenderingContext2D::PutImageData(ImageData& imageData, double dx,
                                       double dy, ErrorResult& error)
{
  dom::Uint8ClampedArray arr(imageData.GetDataObject());

  error = PutImageData_explicit(JS_DoubleToInt32(dx), JS_DoubleToInt32(dy),
                                imageData.Width(), imageData.Height(),
                                arr.Data(), arr.Length(), false, 0, 0, 0, 0);
}

void
CanvasRenderingContext2D::PutImageData(ImageData& imageData, double dx,
                                       double dy, double dirtyX,
                                       double dirtyY, double dirtyWidth,
                                       double dirtyHeight,
                                       ErrorResult& error)
{
  dom::Uint8ClampedArray arr(imageData.GetDataObject());

  error = PutImageData_explicit(JS_DoubleToInt32(dx), JS_DoubleToInt32(dy),
                                imageData.Width(), imageData.Height(),
                                arr.Data(), arr.Length(), true,
                                JS_DoubleToInt32(dirtyX),
                                JS_DoubleToInt32(dirtyY),
                                JS_DoubleToInt32(dirtyWidth),
                                JS_DoubleToInt32(dirtyHeight));
}

// void putImageData (in ImageData d, in float x, in float y);
// void putImageData (in ImageData d, in double x, in double y, in double dirtyX, in double dirtyY, in double dirtyWidth, in double dirtyHeight);

nsresult
CanvasRenderingContext2D::PutImageData_explicit(int32_t x, int32_t y, uint32_t w, uint32_t h,
                                                unsigned char *aData, uint32_t aDataLen,
                                                bool hasDirtyRect, int32_t dirtyX, int32_t dirtyY,
                                                int32_t dirtyWidth, int32_t dirtyHeight)
{
  if (w == 0 || h == 0) {
    return NS_ERROR_DOM_SYNTAX_ERR;
  }

  IntRect dirtyRect;
  IntRect imageDataRect(0, 0, w, h);

  if (hasDirtyRect) {
    // fix up negative dimensions
    if (dirtyWidth < 0) {
      NS_ENSURE_TRUE(dirtyWidth != INT_MIN, NS_ERROR_DOM_INDEX_SIZE_ERR);

      CheckedInt32 checkedDirtyX = CheckedInt32(dirtyX) + dirtyWidth;

      if (!checkedDirtyX.isValid())
        return NS_ERROR_DOM_INDEX_SIZE_ERR;

      dirtyX = checkedDirtyX.value();
      dirtyWidth = -dirtyWidth;
    }

    if (dirtyHeight < 0) {
      NS_ENSURE_TRUE(dirtyHeight != INT_MIN, NS_ERROR_DOM_INDEX_SIZE_ERR);

      CheckedInt32 checkedDirtyY = CheckedInt32(dirtyY) + dirtyHeight;

      if (!checkedDirtyY.isValid())
        return NS_ERROR_DOM_INDEX_SIZE_ERR;

      dirtyY = checkedDirtyY.value();
      dirtyHeight = -dirtyHeight;
    }

    // bound the dirty rect within the imageData rectangle
    dirtyRect = imageDataRect.Intersect(IntRect(dirtyX, dirtyY, dirtyWidth, dirtyHeight));

    if (dirtyRect.Width() <= 0 || dirtyRect.Height() <= 0)
      return NS_OK;
  } else {
    dirtyRect = imageDataRect;
  }

  dirtyRect.MoveBy(IntPoint(x, y));
  dirtyRect = IntRect(0, 0, mWidth, mHeight).Intersect(dirtyRect);

  if (dirtyRect.Width() <= 0 || dirtyRect.Height() <= 0) {
    return NS_OK;
  }

  uint32_t len = w * h * 4;
  if (aDataLen != len) {
    return NS_ERROR_DOM_SYNTAX_ERR;
  }

  nsRefPtr<gfxImageSurface> imgsurf = new gfxImageSurface(gfxIntSize(w, h),
                                                          gfxImageFormatARGB32,
                                                          false);
  if (!imgsurf || imgsurf->CairoStatus()) {
    return NS_ERROR_FAILURE;
  }

  uint8_t *src = aData;
  uint8_t *dst = imgsurf->Data();

  for (uint32_t j = 0; j < h; j++) {
    for (uint32_t i = 0; i < w; i++) {
      uint8_t r = *src++;
      uint8_t g = *src++;
      uint8_t b = *src++;
      uint8_t a = *src++;

      // Convert to premultiplied color (losslessly if the input came from getImageData)
#ifdef IS_LITTLE_ENDIAN
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
  }

  EnsureTarget();
  if (!IsTargetValid()) {
    return NS_ERROR_FAILURE;
  }

  RefPtr<SourceSurface> sourceSurface =
    mTarget->CreateSourceSurfaceFromData(imgsurf->Data(), IntSize(w, h), imgsurf->Stride(), FORMAT_B8G8R8A8);

  // In certain scenarios, requesting larger than 8k image fails.  Bug 803568
  // covers the details of how to run into it, but the full detailed
  // investigation hasn't been done to determine the underlying cause.  We
  // will just handle the failure to allocate the surface to avoid a crash.
  if (!sourceSurface) {
    return NS_ERROR_FAILURE;
  }

  mTarget->CopySurface(sourceSurface,
                       IntRect(dirtyRect.x - x, dirtyRect.y - y,
                               dirtyRect.width, dirtyRect.height),
                       IntPoint(dirtyRect.x, dirtyRect.y));

  Redraw(mgfx::Rect(dirtyRect.x, dirtyRect.y, dirtyRect.width, dirtyRect.height));

  return NS_OK;
}

NS_IMETHODIMP
CanvasRenderingContext2D::GetThebesSurface(gfxASurface **surface)
{
  EnsureTarget();

  nsRefPtr<gfxASurface> thebesSurface =
      gfxPlatform::GetPlatform()->GetThebesSurfaceForDrawTarget(mTarget);

  if (!thebesSurface) {
    return NS_ERROR_FAILURE;
  }

  *surface = thebesSurface;
  NS_ADDREF(*surface);

  return NS_OK;
}

static already_AddRefed<ImageData>
CreateImageData(JSContext* cx, CanvasRenderingContext2D* context,
                uint32_t w, uint32_t h, ErrorResult& error)
{
  if (w == 0)
      w = 1;
  if (h == 0)
      h = 1;

  CheckedInt<uint32_t> len = CheckedInt<uint32_t>(w) * h * 4;
  if (!len.isValid()) {
    error.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return nullptr;
  }

  // Create the fast typed array; it's initialized to 0 by default.
  JSObject* darray = Uint8ClampedArray::Create(cx, context, len.value());
  if (!darray) {
    error.Throw(NS_ERROR_OUT_OF_MEMORY);
    return nullptr;
  }

  nsRefPtr<mozilla::dom::ImageData> imageData =
    new mozilla::dom::ImageData(w, h, *darray);
  return imageData.forget();
}

already_AddRefed<ImageData>
CanvasRenderingContext2D::CreateImageData(JSContext* cx, double sw,
                                          double sh, ErrorResult& error)
{
  if (!sw || !sh) {
    error.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return nullptr;
  }

  int32_t wi = JS_DoubleToInt32(sw);
  int32_t hi = JS_DoubleToInt32(sh);

  uint32_t w = Abs(wi);
  uint32_t h = Abs(hi);
  return mozilla::dom::CreateImageData(cx, this, w, h, error);
}

already_AddRefed<ImageData>
CanvasRenderingContext2D::CreateImageData(JSContext* cx,
                                          ImageData& imagedata,
                                          ErrorResult& error)
{
  return mozilla::dom::CreateImageData(cx, this, imagedata.Width(),
                                       imagedata.Height(), error);
}

static uint8_t g2DContextLayerUserData;

already_AddRefed<CanvasLayer>
CanvasRenderingContext2D::GetCanvasLayer(nsDisplayListBuilder* aBuilder,
                                         CanvasLayer *aOldLayer,
                                         LayerManager *aManager)
{
  // Don't call EnsureTarget() ... if there isn't already a surface, then
  // we have nothing to paint and there is no need to create a surface just
  // to paint nothing. Also, EnsureTarget() can cause creation of a persistent
  // layer manager which must NOT happen during a paint.
  if (!mTarget || !IsTargetValid()) {
    // No DidTransactionCallback will be received, so mark the context clean
    // now so future invalidations will be dispatched.
    MarkContextClean();
    return nullptr;
  }

  mTarget->Flush();

  if (!mResetLayer && aOldLayer) {
    CanvasRenderingContext2DUserData* userData =
      static_cast<CanvasRenderingContext2DUserData*>(
        aOldLayer->GetUserData(&g2DContextLayerUserData));
    if (userData && userData->IsForContext(this)) {
      nsRefPtr<CanvasLayer> ret = aOldLayer;
      return ret.forget();
    }
  }

  nsRefPtr<CanvasLayer> canvasLayer = aManager->CreateCanvasLayer();
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
#ifdef USE_SKIA_GPU
  GLContext* glContext = static_cast<GLContext*>(mTarget->GetGLContext());
  if (glContext) {
    canvasLayer->SetPreTransactionCallback(
            CanvasRenderingContext2DUserData::PreTransactionCallback, userData);
    data.mGLContext = glContext;
  } else
#endif
  {
    data.mDrawTarget = mTarget;
  }

  data.mSize = nsIntSize(mWidth, mHeight);

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


bool
CanvasRenderingContext2D::ShouldForceInactiveLayer(LayerManager *aManager)
{
  return !aManager->CanUseCanvasLayerForSize(gfxIntSize(mWidth, mHeight));
}

}
}
