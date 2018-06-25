/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SVGGeometryElement.h"

#include "DOMSVGPoint.h"
#include "gfxPlatform.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/dom/SVGLengthBinding.h"
#include "nsComputedDOMStyle.h"
#include "nsSVGUtils.h"
#include "nsSVGLength2.h"
#include "SVGContentUtils.h"

using namespace mozilla;
using namespace mozilla::gfx;
using namespace mozilla::dom;

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
SVGGeometryElement::AfterSetAttr(int32_t aNamespaceID, nsAtom* aName,
                                 const nsAttrValue* aValue,
                                 const nsAttrValue* aOldValue,
                                 nsIPrincipal* aSubjectPrincipal,
                                 bool aNotify)
{
  if (mCachedPath &&
      aNamespaceID == kNameSpaceID_None &&
      AttributeDefinesGeometry(aName)) {
    mCachedPath = nullptr;
  }
  return SVGGeometryElementBase::AfterSetAttr(aNamespaceID, aName,
                                              aValue, aOldValue,
                                              aSubjectPrincipal,
                                              aNotify);
}

bool
SVGGeometryElement::IsNodeOfType(uint32_t aFlags) const
{
  return !(aFlags & ~eSHAPE);
}

bool
SVGGeometryElement::AttributeDefinesGeometry(const nsAtom *aName)
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
    if (info.mLengths[i].GetSpecifiedUnitType() ==
          SVGLength_Binding::SVG_LENGTHTYPE_PERCENTAGE) {
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
SVGGeometryElement::GetOrBuildPath(const DrawTarget* aDrawTarget,
                                   FillRule aFillRule)
{
  // We only cache the path if it matches the backend used for screen painting:
  bool cacheable  = aDrawTarget->GetBackendType() ==
                    gfxPlatform::GetPlatform()->GetDefaultContentBackend();

  if (cacheable && mCachedPath && mCachedPath->GetFillRule() == aFillRule &&
      aDrawTarget->GetBackendType() == mCachedPath->GetBackendType()) {
    RefPtr<Path> path(mCachedPath);
    return path.forget();
  }
  RefPtr<PathBuilder> builder = aDrawTarget->CreatePathBuilder(aFillRule);
  RefPtr<Path> path = BuildPath(builder);
  if (cacheable) {
    mCachedPath = path;
  }
  return path.forget();
}

already_AddRefed<Path>
SVGGeometryElement::GetOrBuildPathForMeasuring()
{
  RefPtr<DrawTarget> drawTarget =
    gfxPlatform::GetPlatform()->ScreenReferenceDrawTarget();
  FillRule fillRule = mCachedPath ? mCachedPath->GetFillRule() : GetFillRule();
  return GetOrBuildPath(drawTarget, fillRule);
}

FillRule
SVGGeometryElement::GetFillRule()
{
  FillRule fillRule = FillRule::FILL_WINDING; // Equivalent to StyleFillRule::Nonzero

  RefPtr<ComputedStyle> computedStyle =
    nsComputedDOMStyle::GetComputedStyleNoFlush(this, nullptr);

  if (computedStyle) {
    MOZ_ASSERT(computedStyle->StyleSVG()->mFillRule == StyleFillRule::Nonzero ||
               computedStyle->StyleSVG()->mFillRule == StyleFillRule::Evenodd);

    if (computedStyle->StyleSVG()->mFillRule == StyleFillRule::Evenodd) {
      fillRule = FillRule::FILL_EVEN_ODD;
    }
  } else {
    // ReportToConsole
    NS_WARNING("Couldn't get ComputedStyle for content in GetFillRule");
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

  nsCOMPtr<nsISVGPoint> point =
    new DOMSVGPoint(path->ComputePointAtLength(
      clamped(distance, 0.f, path->ComputeLength())));
  return point.forget();
}

float
SVGGeometryElement::GetPathLengthScale(PathLengthScaleForType aFor)
{
  MOZ_ASSERT(aFor == eForTextPath || aFor == eForStroking,
             "Unknown enum");
  if (mPathLength.IsExplicitlySet()) {
    float authorsPathLengthEstimate = mPathLength.GetAnimValue();
    if (authorsPathLengthEstimate > 0) {
      RefPtr<Path> path = GetOrBuildPathForMeasuring();
      if (!path) {
        // The path is empty or invalid so its length must be zero and
        // we know that 0 / authorsPathLengthEstimate = 0.
        return 0.0;
      }
      if (aFor == eForTextPath) {
        // For textPath, a transform on the referenced path affects the
        // textPath layout, so when calculating the actual path length
        // we need to take that into account.
        gfxMatrix matrix = PrependLocalTransformsTo(gfxMatrix());
        if (!matrix.IsIdentity()) {
          RefPtr<PathBuilder> builder =
            path->TransformedCopyToBuilder(ToMatrix(matrix));
          path = builder->Finish();
        }
      }
      return path->ComputeLength() / authorsPathLengthEstimate;
    }
  }
  return 1.0;
}

already_AddRefed<SVGAnimatedNumber>
SVGGeometryElement::PathLength()
{
  return mPathLength.ToDOMAnimatedNumber(this);
}
