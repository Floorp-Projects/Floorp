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

nsSVGPathGeometryElement::nsSVGPathGeometryElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo)
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

FillRule
nsSVGPathGeometryElement::GetFillRule()
{
  FillRule fillRule = FillRule::FILL_WINDING; // Equivalent to NS_STYLE_FILL_RULE_NONZERO

  nsRefPtr<nsStyleContext> styleContext =
    nsComputedDOMStyle::GetStyleContextForElementNoFlush(this, nullptr,
                                                         nullptr);
  
  if (styleContext) {
    MOZ_ASSERT(styleContext->StyleSVG()->mFillRule ==
                                           NS_STYLE_FILL_RULE_NONZERO ||
               styleContext->StyleSVG()->mFillRule ==
                                           NS_STYLE_FILL_RULE_EVENODD);

    if (styleContext->StyleSVG()->mFillRule == NS_STYLE_FILL_RULE_EVENODD) {
      fillRule = FillRule::FILL_EVEN_ODD;
    }
  } else {
    // ReportToConsole
    NS_WARNING("Couldn't get style context for content in GetFillRule");
  }

  return fillRule;
}
