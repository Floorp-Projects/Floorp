/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsSVGPathGeometryElement.h"

#include "gfxPlatform.h"
#include "mozilla/gfx/2D.h"
#include "nsComputedDOMStyle.h"
#include "nsSVGLength2.h"
#include "SVGContentUtils.h"

using namespace mozilla;
using namespace mozilla::gfx;

//----------------------------------------------------------------------
// Implementation

nsSVGPathGeometryElement::nsSVGPathGeometryElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : nsSVGPathGeometryElementBase(aNodeInfo)
{
}

bool
nsSVGPathGeometryElement::AttributeDefinesGeometry(const nsIAtom *aName)
{
  // Check for nsSVGLength2 attribute
  LengthAttributesInfo info = GetLengthInfo();
  for (uint32_t i = 0; i < info.mLengthCount; i++) {
    if (aName == *info.mLengthInfo[i].mName) {
      return true;
    }
  }

  return false;
}

bool
nsSVGPathGeometryElement::GeometryDependsOnCoordCtx()
{
  // Check the nsSVGLength2 attribute
  LengthAttributesInfo info = const_cast<nsSVGPathGeometryElement*>(this)->GetLengthInfo();
  for (uint32_t i = 0; i < info.mLengthCount; i++) {
    if (info.mLengths[i].GetSpecifiedUnitType() == nsIDOMSVGLength::SVG_LENGTHTYPE_PERCENTAGE) {
      return true;
    }   
  }
  return false;
}

bool
nsSVGPathGeometryElement::IsMarkable()
{
  return false;
}

void
nsSVGPathGeometryElement::GetMarkPoints(nsTArray<nsSVGMark> *aMarks)
{
}

TemporaryRef<Path>
nsSVGPathGeometryElement::GetPathForLengthOrPositionMeasuring()
{
  return nullptr;
}

TemporaryRef<PathBuilder>
nsSVGPathGeometryElement::CreatePathBuilder()
{
  RefPtr<DrawTarget> drawTarget =
    gfxPlatform::GetPlatform()->ScreenReferenceDrawTarget();
  NS_ASSERTION(gfxPlatform::GetPlatform()->
                 SupportsAzureContentForDrawTarget(drawTarget),
               "Should support Moz2D content drawing");

  // The fill rule that we pass to CreatePathBuilder must be the current
  // computed value of our CSS 'fill-rule' property if the path that we return
  // will be used for painting or hit-testing. For all other uses (bounds
  // calculatons, length measurement, position-at-offset calculations) the fill
  // rule that we pass doesn't matter. As a result we can just pass the current
  // computed value regardless of who's calling us, or what they're going to do
  // with the path that we return.

  return drawTarget->CreatePathBuilder(GetFillRule());
}

FillRule
nsSVGPathGeometryElement::GetFillRule()
{
  FillRule fillRule = FILL_WINDING; // Equivalent to NS_STYLE_FILL_RULE_NONZERO

  nsRefPtr<nsStyleContext> styleContext =
    nsComputedDOMStyle::GetStyleContextForElementNoFlush(this, nullptr,
                                                         nullptr);
  
  if (styleContext) {
    MOZ_ASSERT(styleContext->StyleSVG()->mFillRule ==
                                           NS_STYLE_FILL_RULE_NONZERO ||
               styleContext->StyleSVG()->mFillRule ==
                                           NS_STYLE_FILL_RULE_EVENODD);

    if (styleContext->StyleSVG()->mFillRule == NS_STYLE_FILL_RULE_EVENODD) {
      fillRule = FILL_EVEN_ODD;
    }
  } else {
    // ReportToConsole
    NS_WARNING("Couldn't get style context for content in GetFillRule");
  }

  return fillRule;
}

Float
nsSVGPathGeometryElement::GetStrokeWidth()
{
  nsRefPtr<nsStyleContext> styleContext =
    nsComputedDOMStyle::GetStyleContextForElementNoFlush(this, nullptr,
                                                         nullptr);
  return styleContext ?
    SVGContentUtils::CoordToFloat(styleContext->PresContext(), this,
                                  styleContext->StyleSVG()->mStrokeWidth) :
    0.0f;
}
