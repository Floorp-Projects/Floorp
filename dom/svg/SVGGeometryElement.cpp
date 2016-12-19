/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SVGGeometryElement.h"

#include "DOMSVGPoint.h"
#include "gfxPlatform.h"
#include "mozilla/gfx/2D.h"
#include "nsComputedDOMStyle.h"
#include "nsSVGUtils.h"
#include "nsSVGLength2.h"
#include "SVGContentUtils.h"

using namespace mozilla;
using namespace mozilla::gfx;

nsSVGElement::NumberInfo SVGGeometryElement::sNumberInfo =
{ &nsGkAtoms::pathLength, 0, false };

//----------------------------------------------------------------------
// Implementation

SVGGeometryElement::SVGGeometryElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo)
  : SVGGeometryElementBase(aNodeInfo)
{
}

nsSVGElement::NumberAttributesInfo
SVGGeometryElement::GetNumberInfo()
{
  return NumberAttributesInfo(&mPathLength, &sNumberInfo, 1);
}

nsresult
SVGGeometryElement::AfterSetAttr(int32_t aNamespaceID, nsIAtom* aName,
                                 const nsAttrValue* aValue, bool aNotify)
{
  if (mCachedPath &&
      aNamespaceID == kNameSpaceID_None &&
      AttributeDefinesGeometry(aName)) {
    mCachedPath = nullptr;
  }
  return SVGGeometryElementBase::AfterSetAttr(aNamespaceID, aName,
                                              aValue, aNotify);
}

bool
SVGGeometryElement::AttributeDefinesGeometry(const nsIAtom *aName)
{
  if (aName == nsGkAtoms::pathLength) {
    return true;
  }

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
SVGGeometryElement::GeometryDependsOnCoordCtx()
{
  // Check the nsSVGLength2 attribute
  LengthAttributesInfo info = const_cast<SVGGeometryElement*>(this)->GetLengthInfo();
  for (uint32_t i = 0; i < info.mLengthCount; i++) {
    if (info.mLengths[i].GetSpecifiedUnitType() == nsIDOMSVGLength::SVG_LENGTHTYPE_PERCENTAGE) {
      return true;
    }   
  }
  return false;
}

bool
SVGGeometryElement::IsMarkable()
{
  return false;
}

void
SVGGeometryElement::GetMarkPoints(nsTArray<nsSVGMark> *aMarks)
{
}

already_AddRefed<Path>
SVGGeometryElement::GetOrBuildPath(const DrawTarget& aDrawTarget,
                                   FillRule aFillRule)
{
  // We only cache the path if it matches the backend used for screen painting:
  bool cacheable  = aDrawTarget.GetBackendType() ==
                    gfxPlatform::GetPlatform()->GetDefaultContentBackend();

  // Checking for and returning mCachedPath before checking the pref means
  // that the pref is only live on page reload (or app restart for SVG in
  // chrome). The benefit is that we avoid causing a CPU memory cache miss by
  // looking at the global variable that the pref's stored in.
  if (cacheable && mCachedPath) {
    if (aDrawTarget.GetBackendType() == mCachedPath->GetBackendType()) {
      RefPtr<Path> path(mCachedPath);
      return path.forget();
    }
  }
  RefPtr<PathBuilder> builder = aDrawTarget.CreatePathBuilder(aFillRule);
  RefPtr<Path> path = BuildPath(builder);
  if (cacheable && NS_SVGPathCachingEnabled()) {
    mCachedPath = path;
  }
  return path.forget();
}

already_AddRefed<Path>
SVGGeometryElement::GetOrBuildPathForMeasuring()
{
  return nullptr;
}

FillRule
SVGGeometryElement::GetFillRule()
{
  FillRule fillRule = FillRule::FILL_WINDING; // Equivalent to StyleFillRule::Nonzero

  RefPtr<nsStyleContext> styleContext =
    nsComputedDOMStyle::GetStyleContextForElementNoFlush(this, nullptr,
                                                         nullptr);
  
  if (styleContext) {
    MOZ_ASSERT(styleContext->StyleSVG()->mFillRule == StyleFillRule::Nonzero ||
               styleContext->StyleSVG()->mFillRule == StyleFillRule::Evenodd);

    if (styleContext->StyleSVG()->mFillRule == StyleFillRule::Evenodd) {
      fillRule = FillRule::FILL_EVEN_ODD;
    }
  } else {
    // ReportToConsole
    NS_WARNING("Couldn't get style context for content in GetFillRule");
  }

  return fillRule;
}

float
SVGGeometryElement::GetTotalLength()
{
  RefPtr<Path> flat = GetOrBuildPathForMeasuring();
  return flat ? flat->ComputeLength() : 0.f;
}

already_AddRefed<nsISVGPoint>
SVGGeometryElement::GetPointAtLength(float distance, ErrorResult& rv)
{
  RefPtr<Path> path = GetOrBuildPathForMeasuring();
  if (!path) {
    rv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  float totalLength = path->ComputeLength();
  if (mPathLength.IsExplicitlySet()) {
    float pathLength = mPathLength.GetAnimValue();
    if (pathLength <= 0) {
      rv.Throw(NS_ERROR_FAILURE);
      return nullptr;
    }
    distance *= totalLength / pathLength;
  }
  distance = std::max(0.f,         distance);
  distance = std::min(totalLength, distance);

  nsCOMPtr<nsISVGPoint> point =
    new DOMSVGPoint(path->ComputePointAtLength(distance));
  return point.forget();
}

already_AddRefed<SVGAnimatedNumber>
SVGGeometryElement::PathLength()
{
  return mPathLength.ToDOMAnimatedNumber(this);
}
