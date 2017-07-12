/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SVGContextPaint.h"

#include "gfxContext.h"
#include "gfxUtils.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/Preferences.h"
#include "nsIDocument.h"
#include "nsSVGPaintServerFrame.h"
#include "nsSVGEffects.h"
#include "nsSVGPaintServerFrame.h"

using namespace mozilla::gfx;
using namespace mozilla::image;

namespace mozilla {

using image::imgDrawingParams;

/* static */ bool
SVGContextPaint::IsAllowedForImageFromURI(nsIURI* aURI)
{
  static bool sEnabledForContent = false;
  static bool sEnabledForContentCached = false;

  if (!sEnabledForContentCached) {
    Preferences::AddBoolVarCache(&sEnabledForContent,
                                 "svg.context-properties.content.enabled", false);
    sEnabledForContentCached = true;
  }

  if (sEnabledForContent) {
    return true;
  }

  // Context paint is pref'ed off for Web content.  Ideally we'd have some
  // easy means to determine whether the frame that has linked to the image
  // is a frame for a content node that originated from Web content.
  // Unfortunately different types of anonymous content, about: documents
  // such as about:reader, etc. that are "our" code that we ship are
  // sometimes hard to distinguish from real Web content.  As a result,
  // instead of trying to figure out what content is "ours" we instead let
  // any content provide image context paint, but only if the image is
  // chrome:// or resource:// do we return true.  This should be sufficient
  // to stop the image context paint feature being useful to (and therefore
  // used by and relied upon by) Web content.  (We don't want Web content to
  // use this feature because we're not sure that image context paint is a
  // good mechanism for wider use, or suitable for specification.)
  //
  // One case that is not covered by chrome:// or resource:// are WebExtensions,
  // specifically ones that are "ours". WebExtensions are moz-extension://
  // regardless if the extension is in-tree or not. Since we don't want
  // extension developers coming to rely on image context paint either, we only
  // enable context-paint for extensions that are signed by Mozilla.
  //
  nsAutoCString scheme;
  if (NS_SUCCEEDED(aURI->GetScheme(scheme)) &&
      (scheme.EqualsLiteral("chrome") || scheme.EqualsLiteral("resource"))) {
    return true;
  }
  RefPtr<BasePrincipal> principal = BasePrincipal::CreateCodebasePrincipal(aURI, OriginAttributes());
  nsString addonId;
  if (NS_SUCCEEDED(principal->GetAddonId(addonId))) {
    return StringEndsWith(addonId, NS_LITERAL_STRING("@mozilla.org"));
  }
  return false;
}

/**
 * Stores in |aTargetPaint| information on how to reconstruct the current
 * fill or stroke pattern. Will also set the paint opacity to transparent if
 * the paint is set to "none".
 * @param aOuterContextPaint pattern information from the outer text context
 * @param aTargetPaint where to store the current pattern information
 * @param aFillOrStroke member pointer to the paint we are setting up
 * @param aProperty the frame property descriptor of the fill or stroke paint
 *   server frame
 */
static void
SetupInheritablePaint(const DrawTarget* aDrawTarget,
                      const gfxMatrix& aContextMatrix,
                      nsIFrame* aFrame,
                      float& aOpacity,
                      SVGContextPaint* aOuterContextPaint,
                      SVGContextPaintImpl::Paint& aTargetPaint,
                      nsStyleSVGPaint nsStyleSVG::*aFillOrStroke,
                      nsSVGEffects::PaintingPropertyDescriptor aProperty,
                      imgDrawingParams& aImgParams)
{
  const nsStyleSVG *style = aFrame->StyleSVG();
  nsSVGPaintServerFrame *ps =
    nsSVGEffects::GetPaintServer(aFrame, aFillOrStroke, aProperty);

  if (ps) {
    RefPtr<gfxPattern> pattern =
      ps->GetPaintServerPattern(aFrame, aDrawTarget, aContextMatrix,
                                aFillOrStroke, aOpacity, aImgParams);

    if (pattern) {
      aTargetPaint.SetPaintServer(aFrame, aContextMatrix, ps);
      return;
    }
  }

  if (aOuterContextPaint) {
    RefPtr<gfxPattern> pattern;
    switch ((style->*aFillOrStroke).Type()) {
    case eStyleSVGPaintType_ContextFill:
      pattern =
        aOuterContextPaint->GetFillPattern(aDrawTarget, aOpacity,
                                           aContextMatrix, aImgParams);
      break;
    case eStyleSVGPaintType_ContextStroke:
       pattern =
         aOuterContextPaint->GetStrokePattern(aDrawTarget, aOpacity,
                                              aContextMatrix, aImgParams);
      break;
    default:
      ;
    }
    if (pattern) {
      aTargetPaint.SetContextPaint(aOuterContextPaint, (style->*aFillOrStroke).Type());
      return;
    }
  }

  nscolor color =
    nsSVGUtils::GetFallbackOrPaintColor(aFrame->StyleContext(), aFillOrStroke);
  aTargetPaint.SetColor(color);
}

DrawMode
SVGContextPaintImpl::Init(const DrawTarget* aDrawTarget,
                          const gfxMatrix& aContextMatrix,
                          nsIFrame* aFrame,
                          SVGContextPaint* aOuterContextPaint,
                          imgDrawingParams& aImgParams)
{
  DrawMode toDraw = DrawMode(0);

  const nsStyleSVG *style = aFrame->StyleSVG();

  // fill:
  if (style->mFill.Type() == eStyleSVGPaintType_None) {
    SetFillOpacity(0.0f);
  } else {
    float opacity = nsSVGUtils::GetOpacity(style->FillOpacitySource(),
                                           style->mFillOpacity,
                                           aOuterContextPaint);

    SetupInheritablePaint(aDrawTarget, aContextMatrix, aFrame, opacity,
                          aOuterContextPaint, mFillPaint, &nsStyleSVG::mFill,
                          nsSVGEffects::FillProperty(), aImgParams);

    SetFillOpacity(opacity);

    toDraw |= DrawMode::GLYPH_FILL;
  }

  // stroke:
  if (style->mStroke.Type() == eStyleSVGPaintType_None) {
    SetStrokeOpacity(0.0f);
  } else {
    float opacity = nsSVGUtils::GetOpacity(style->StrokeOpacitySource(),
                                           style->mStrokeOpacity,
                                           aOuterContextPaint);

    SetupInheritablePaint(aDrawTarget, aContextMatrix, aFrame, opacity,
                          aOuterContextPaint, mStrokePaint,
                          &nsStyleSVG::mStroke, nsSVGEffects::StrokeProperty(),
                          aImgParams);

    SetStrokeOpacity(opacity);

    toDraw |= DrawMode::GLYPH_STROKE;
  }

  return toDraw;
}

void
SVGContextPaint::InitStrokeGeometry(gfxContext* aContext,
                                    float devUnitsPerSVGUnit)
{
  mStrokeWidth = aContext->CurrentLineWidth() / devUnitsPerSVGUnit;
  aContext->CurrentDash(mDashes, &mDashOffset);
  for (uint32_t i = 0; i < mDashes.Length(); i++) {
    mDashes[i] /= devUnitsPerSVGUnit;
  }
  mDashOffset /= devUnitsPerSVGUnit;
}

/* static */ SVGContextPaint*
SVGContextPaint::GetContextPaint(nsIContent* aContent)
{
  nsIDocument* ownerDoc = aContent->OwnerDoc();

  if (!ownerDoc->IsBeingUsedAsImage()) {
    return nullptr;
  }

  // XXX The SVGContextPaint that was passed to SetProperty was const. Ideally
  // we could and should re-apply that constness to the SVGContextPaint that
  // we get here (SVGImageContext is never changed after it is initialized).
  // Unfortunately lazy initialization of SVGContextPaint (which is a member of
  // SVGImageContext, and also conceptually never changes after construction)
  // prevents some of SVGContextPaint's conceptually const methods from being
  // const.  Trying to fix SVGContextPaint (perhaps by using |mutable|) is a
  // bit of a headache so for now we punt on that, don't reapply the constness
  // to the SVGContextPaint here, and trust that no one will add code that
  // actually modifies the object.

  return static_cast<SVGContextPaint*>(
           ownerDoc->GetProperty(nsGkAtoms::svgContextPaint));
}

already_AddRefed<gfxPattern>
SVGContextPaintImpl::GetFillPattern(const DrawTarget* aDrawTarget,
                                    float aOpacity,
                                    const gfxMatrix& aCTM,
                                    imgDrawingParams& aImgParams)
{
  return mFillPaint.GetPattern(aDrawTarget, aOpacity, &nsStyleSVG::mFill, aCTM,
                               aImgParams);
}

already_AddRefed<gfxPattern>
SVGContextPaintImpl::GetStrokePattern(const DrawTarget* aDrawTarget,
                                      float aOpacity,
                                      const gfxMatrix& aCTM,
                                      imgDrawingParams& aImgParams)
{
  return mStrokePaint.GetPattern(aDrawTarget, aOpacity, &nsStyleSVG::mStroke,
                                 aCTM, aImgParams);
}

already_AddRefed<gfxPattern>
SVGContextPaintImpl::Paint::GetPattern(const DrawTarget* aDrawTarget,
                                       float aOpacity,
                                       nsStyleSVGPaint nsStyleSVG::*aFillOrStroke,
                                       const gfxMatrix& aCTM,
                                       imgDrawingParams& aImgParams)
{
  RefPtr<gfxPattern> pattern;
  if (mPatternCache.Get(aOpacity, getter_AddRefs(pattern))) {
    // Set the pattern matrix just in case it was messed with by a previous
    // caller. We should get the same matrix each time a pattern is constructed
    // so this should be fine.
    pattern->SetMatrix(aCTM * mPatternMatrix);
    return pattern.forget();
  }

  switch (mPaintType) {
  case eStyleSVGPaintType_None:
    pattern = new gfxPattern(Color());
    mPatternMatrix = gfxMatrix();
    break;
  case eStyleSVGPaintType_Color: {
    Color color = Color::FromABGR(mPaintDefinition.mColor);
    color.a *= aOpacity;
    pattern = new gfxPattern(color);
    mPatternMatrix = gfxMatrix();
    break;
  }
  case eStyleSVGPaintType_Server:
    pattern =
      mPaintDefinition.mPaintServerFrame->GetPaintServerPattern(mFrame,
                                                                aDrawTarget,
                                                                mContextMatrix,
                                                                aFillOrStroke,
                                                                aOpacity,
                                                                aImgParams);
    {
      // m maps original-user-space to pattern space
      gfxMatrix m = pattern->GetMatrix();
      gfxMatrix deviceToOriginalUserSpace = mContextMatrix;
      if (!deviceToOriginalUserSpace.Invert()) {
        return nullptr;
      }
      // mPatternMatrix maps device space to pattern space via original user space
      mPatternMatrix = deviceToOriginalUserSpace * m;
    }
    pattern->SetMatrix(aCTM * mPatternMatrix);
    break;
  case eStyleSVGPaintType_ContextFill:
    pattern =
      mPaintDefinition.mContextPaint->GetFillPattern(aDrawTarget,
                                                     aOpacity, aCTM,
                                                     aImgParams);
    // Don't cache this. mContextPaint will have cached it anyway. If we
    // cache it, we'll have to compute mPatternMatrix, which is annoying.
    return pattern.forget();
  case eStyleSVGPaintType_ContextStroke:
    pattern =
      mPaintDefinition.mContextPaint->GetStrokePattern(aDrawTarget,
                                                       aOpacity, aCTM,
                                                       aImgParams);
    // Don't cache this. mContextPaint will have cached it anyway. If we
    // cache it, we'll have to compute mPatternMatrix, which is annoying.
    return pattern.forget();
  default:
    MOZ_ASSERT(false, "invalid paint type");
    return nullptr;
  }

  mPatternCache.Put(aOpacity, pattern);
  return pattern.forget();
}

AutoSetRestoreSVGContextPaint::AutoSetRestoreSVGContextPaint(
                                 const SVGContextPaint* aContextPaint,
                                 nsIDocument* aSVGDocument)
  : mSVGDocument(aSVGDocument)
  , mOuterContextPaint(aSVGDocument->GetProperty(nsGkAtoms::svgContextPaint))
{
  // The way that we supply context paint is to temporarily set the context
  // paint on the owner document of the SVG that we're painting while it's
  // being painted.

  MOZ_ASSERT(aContextPaint);
  MOZ_ASSERT(aSVGDocument->IsBeingUsedAsImage(),
             "SVGContextPaint::GetContextPaint assumes this");

  if (mOuterContextPaint) {
    mSVGDocument->UnsetProperty(nsGkAtoms::svgContextPaint);
  }

  DebugOnly<nsresult> res =
    mSVGDocument->SetProperty(nsGkAtoms::svgContextPaint,
                              const_cast<SVGContextPaint*>(aContextPaint));

  NS_WARNING_ASSERTION(NS_SUCCEEDED(res), "Failed to set context paint");
}

AutoSetRestoreSVGContextPaint::~AutoSetRestoreSVGContextPaint()
{
  mSVGDocument->UnsetProperty(nsGkAtoms::svgContextPaint);
  if (mOuterContextPaint) {
    DebugOnly<nsresult> res =
      mSVGDocument->SetProperty(nsGkAtoms::svgContextPaint, mOuterContextPaint);

    NS_WARNING_ASSERTION(NS_SUCCEEDED(res), "Failed to restore context paint");
  }
}


// SVGEmbeddingContextPaint

already_AddRefed<gfxPattern>
SVGEmbeddingContextPaint::GetFillPattern(const DrawTarget* aDrawTarget,
                                         float aFillOpacity,
                                         const gfxMatrix& aCTM,
                                         imgDrawingParams& aImgParams)
{
  if (!mFill) {
    return nullptr;
  }
  // The gfxPattern that we create below depends on aFillOpacity, and since
  // different elements in the SVG image may pass in different values for
  // fill opacities we don't try to cache the gfxPattern that we create.
  Color fill = *mFill;
  fill.a *= aFillOpacity;
  return do_AddRef(new gfxPattern(fill));
}

already_AddRefed<gfxPattern>
SVGEmbeddingContextPaint::GetStrokePattern(const DrawTarget* aDrawTarget,
                                           float aStrokeOpacity,
                                           const gfxMatrix& aCTM,
                                           imgDrawingParams& aImgParams)
{
  if (!mStroke) {
    return nullptr;
  }
  Color stroke = *mStroke;
  stroke.a *= aStrokeOpacity;
  return do_AddRef(new gfxPattern(stroke));
}

uint32_t
SVGEmbeddingContextPaint::Hash() const
{
  uint32_t hash = 0;

  if (mFill) {
    hash = HashGeneric(hash, mFill->ToABGR());
  } else {
    // Arbitrary number, just to avoid trivial hash collisions between pairs of
    // instances where one embedding context has fill set to the same value as
    // another context has stroke set to.
    hash = 1;
  }

  if (mStroke) {
    hash = HashGeneric(hash, mStroke->ToABGR());
  }

  if (mFillOpacity != 1.0f) {
    hash = HashGeneric(hash, mFillOpacity);
  }

  if (mStrokeOpacity != 1.0f) {
    hash = HashGeneric(hash, mStrokeOpacity);
  }

  return hash;
}

} // namespace mozilla
