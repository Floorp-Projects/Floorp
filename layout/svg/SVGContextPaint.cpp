/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SVGContextPaint.h"

#include "gfxContext.h"
#include "mozilla/gfx/2D.h"
#include "nsIDocument.h"
#include "nsSVGPaintServerFrame.h"
#include "nsSVGEffects.h"
#include "nsSVGPaintServerFrame.h"

using namespace mozilla::gfx;

namespace mozilla {

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
                      nsSVGEffects::PaintingPropertyDescriptor aProperty)
{
  const nsStyleSVG *style = aFrame->StyleSVG();
  nsSVGPaintServerFrame *ps =
    nsSVGEffects::GetPaintServer(aFrame, aFillOrStroke, aProperty);

  if (ps) {
    RefPtr<gfxPattern> pattern =
      ps->GetPaintServerPattern(aFrame, aDrawTarget, aContextMatrix,
                                aFillOrStroke, aOpacity);
    if (pattern) {
      aTargetPaint.SetPaintServer(aFrame, aContextMatrix, ps);
      return;
    }
  }
  if (aOuterContextPaint) {
    RefPtr<gfxPattern> pattern;
    switch ((style->*aFillOrStroke).mType) {
    case eStyleSVGPaintType_ContextFill:
      pattern = aOuterContextPaint->GetFillPattern(aDrawTarget, aOpacity,
                                                   aContextMatrix);
      break;
    case eStyleSVGPaintType_ContextStroke:
      pattern = aOuterContextPaint->GetStrokePattern(aDrawTarget, aOpacity,
                                                     aContextMatrix);
      break;
    default:
      ;
    }
    if (pattern) {
      aTargetPaint.SetContextPaint(aOuterContextPaint, (style->*aFillOrStroke).mType);
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
                          SVGContextPaint* aOuterContextPaint)
{
  DrawMode toDraw = DrawMode(0);

  const nsStyleSVG *style = aFrame->StyleSVG();

  // fill:
  if (style->mFill.mType == eStyleSVGPaintType_None) {
    SetFillOpacity(0.0f);
  } else {
    float opacity = nsSVGUtils::GetOpacity(style->FillOpacitySource(),
                                           style->mFillOpacity,
                                           aOuterContextPaint);

    SetupInheritablePaint(aDrawTarget, aContextMatrix, aFrame,
                          opacity, aOuterContextPaint,
                          mFillPaint, &nsStyleSVG::mFill,
                          nsSVGEffects::FillProperty());

    SetFillOpacity(opacity);

    toDraw |= DrawMode::GLYPH_FILL;
  }

  // stroke:
  if (style->mStroke.mType == eStyleSVGPaintType_None) {
    SetStrokeOpacity(0.0f);
  } else {
    float opacity = nsSVGUtils::GetOpacity(style->StrokeOpacitySource(),
                                           style->mStrokeOpacity,
                                           aOuterContextPaint);

    SetupInheritablePaint(aDrawTarget, aContextMatrix, aFrame,
                          opacity, aOuterContextPaint,
                          mStrokePaint, &nsStyleSVG::mStroke,
                          nsSVGEffects::StrokeProperty());

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

  return static_cast<SVGContextPaint*>(
           ownerDoc->GetProperty(nsGkAtoms::svgContextPaint));
}

already_AddRefed<gfxPattern>
SVGContextPaintImpl::GetFillPattern(const DrawTarget* aDrawTarget,
                                    float aOpacity,
                                    const gfxMatrix& aCTM)
{
  return mFillPaint.GetPattern(aDrawTarget, aOpacity, &nsStyleSVG::mFill, aCTM);
}

already_AddRefed<gfxPattern>
SVGContextPaintImpl::GetStrokePattern(const DrawTarget* aDrawTarget,
                                      float aOpacity,
                                      const gfxMatrix& aCTM)
{
  return mStrokePaint.GetPattern(aDrawTarget, aOpacity, &nsStyleSVG::mStroke, aCTM);
}

already_AddRefed<gfxPattern>
SVGContextPaintImpl::Paint::GetPattern(const DrawTarget* aDrawTarget,
                                       float aOpacity,
                                       nsStyleSVGPaint nsStyleSVG::*aFillOrStroke,
                                       const gfxMatrix& aCTM)
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
    pattern = mPaintDefinition.mPaintServerFrame->GetPaintServerPattern(mFrame,
                                                                        aDrawTarget,
                                                                        mContextMatrix,
                                                                        aFillOrStroke,
                                                                        aOpacity);
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
    pattern = mPaintDefinition.mContextPaint->GetFillPattern(aDrawTarget,
                                                             aOpacity, aCTM);
    // Don't cache this. mContextPaint will have cached it anyway. If we
    // cache it, we'll have to compute mPatternMatrix, which is annoying.
    return pattern.forget();
  case eStyleSVGPaintType_ContextStroke:
    pattern = mPaintDefinition.mContextPaint->GetStrokePattern(aDrawTarget,
                                                               aOpacity, aCTM);
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
                                 SVGContextPaint* aContextPaint,
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
    mSVGDocument->SetProperty(nsGkAtoms::svgContextPaint, aContextPaint);

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

} // namespace mozilla
