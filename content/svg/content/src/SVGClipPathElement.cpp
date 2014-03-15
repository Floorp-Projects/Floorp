/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"

#include "mozilla/dom/SVGClipPathElement.h"
#include "mozilla/dom/SVGClipPathElementBinding.h"
#include "nsGkAtoms.h"

NS_IMPL_NS_NEW_NAMESPACED_SVG_ELEMENT(ClipPath)

namespace mozilla {
namespace dom {

JSObject*
SVGClipPathElement::WrapNode(JSContext *aCx, JS::Handle<JSObject*> aScope)
{
  return SVGClipPathElementBinding::Wrap(aCx, aScope, this);
}

nsSVGElement::EnumInfo SVGClipPathElement::sEnumInfo[1] =
{
  { &nsGkAtoms::clipPathUnits,
    sSVGUnitTypesMap,
    SVG_UNIT_TYPE_USERSPACEONUSE
  }
};

//----------------------------------------------------------------------
// Implementation

SVGClipPathElement::SVGClipPathElement(already_AddRefed<nsINodeInfo>& aNodeInfo)
  : SVGClipPathElementBase(aNodeInfo)
{
}

already_AddRefed<SVGAnimatedEnumeration>
SVGClipPathElement::ClipPathUnits()
{
  return mEnumAttributes[CLIPPATHUNITS].ToDOMAnimatedEnum(this);
}

nsSVGElement::EnumAttributesInfo
SVGClipPathElement::GetEnumInfo()
{
  return EnumAttributesInfo(mEnumAttributes, sEnumInfo,
                            ArrayLength(sEnumInfo));
}

//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGClipPathElement)

} // namespace dom
} // namespace mozilla
