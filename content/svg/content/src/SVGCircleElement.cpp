/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGCircleElement.h"
#include "mozilla/gfx/2D.h"
#include "nsGkAtoms.h"
#include "gfxContext.h"
#include "mozilla/dom/SVGCircleElementBinding.h"

NS_IMPL_NS_NEW_NAMESPACED_SVG_ELEMENT(Circle)

using namespace mozilla::gfx;

namespace mozilla {
namespace dom {

JSObject*
SVGCircleElement::WrapNode(JSContext *aCx, JS::Handle<JSObject*> aScope)
{
  return SVGCircleElementBinding::Wrap(aCx, aScope, this);
}

nsSVGElement::LengthInfo SVGCircleElement::sLengthInfo[3] =
{
  { &nsGkAtoms::cx, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER, SVGContentUtils::X },
  { &nsGkAtoms::cy, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER, SVGContentUtils::Y },
  { &nsGkAtoms::r, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER, SVGContentUtils::XY }
};

//----------------------------------------------------------------------
// Implementation

SVGCircleElement::SVGCircleElement(already_AddRefed<nsINodeInfo>& aNodeInfo)
  : SVGCircleElementBase(aNodeInfo)
{
}

//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGCircleElement)

//----------------------------------------------------------------------

already_AddRefed<SVGAnimatedLength>
SVGCircleElement::Cx()
{
  return mLengthAttributes[ATTR_CX].ToDOMAnimatedLength(this);
}

already_AddRefed<SVGAnimatedLength>
SVGCircleElement::Cy()
{
  return mLengthAttributes[ATTR_CY].ToDOMAnimatedLength(this);
}

already_AddRefed<SVGAnimatedLength>
SVGCircleElement::R()
{
  return mLengthAttributes[ATTR_R].ToDOMAnimatedLength(this);
}

//----------------------------------------------------------------------
// nsSVGElement methods

/* virtual */ bool
SVGCircleElement::HasValidDimensions() const
{
  return mLengthAttributes[ATTR_R].IsExplicitlySet() &&
         mLengthAttributes[ATTR_R].GetAnimValInSpecifiedUnits() > 0;
}

nsSVGElement::LengthAttributesInfo
SVGCircleElement::GetLengthInfo()
{
  return LengthAttributesInfo(mLengthAttributes, sLengthInfo,
                              ArrayLength(sLengthInfo));
}

//----------------------------------------------------------------------
// nsSVGPathGeometryElement methods

void
SVGCircleElement::ConstructPath(gfxContext *aCtx)
{
  float x, y, r;

  GetAnimatedLengthValues(&x, &y, &r, nullptr);

  if (r > 0.0f)
    aCtx->Arc(gfxPoint(x, y), r, 0, 2*M_PI);
}

TemporaryRef<Path>
SVGCircleElement::BuildPath()
{
  float x, y, r;
  GetAnimatedLengthValues(&x, &y, &r, nullptr);

  if (r <= 0.0f) {
    return nullptr;
  }

  RefPtr<PathBuilder> pathBuilder = CreatePathBuilder();

  pathBuilder->Arc(Point(x, y), r, 0, Float(2*M_PI));

  return pathBuilder->Finish();
}

} // namespace dom
} // namespace mozilla
