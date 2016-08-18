/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsSVGPathGeometryElement.h"

#include "gfxPlatform.h"
#include "mozilla/gfx/2D.h"
#include "nsComputedDOMStyle.h"
#include "nsSVGUtils.h"
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

nsresult
nsSVGPathGeometryElement::AfterSetAttr(int32_t aNamespaceID, nsIAtom* aName,
                                       const nsAttrValue* aValue, bool aNotify)
{
  if (mCachedPath &&
      aNamespaceID == kNameSpaceID_None &&
      AttributeDefinesGeometry(aName)) {
    mCachedPath = nullptr;
  }
  return nsSVGPathGeometryElementBase::AfterSetAttr(aNamespaceID, aName,
                                                    aValue, aNotify);
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

already_AddRefed<Path>
nsSVGPathGeometryElement::GetOrBuildPath(const DrawTarget& aDrawTarget,
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
nsSVGPathGeometryElement::GetOrBuildPathForMeasuring()
{
  return nullptr;
}

FillRule
nsSVGPathGeometryElement::GetFillRule()
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
