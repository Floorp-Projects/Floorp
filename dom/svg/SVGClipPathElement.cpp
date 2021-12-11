/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGClipPathElement.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/dom/SVGClipPathElementBinding.h"
#include "mozilla/dom/SVGUnitTypesBinding.h"
#include "nsGkAtoms.h"

NS_IMPL_NS_NEW_SVG_ELEMENT(ClipPath)

namespace mozilla {
namespace dom {

using namespace SVGUnitTypes_Binding;

JSObject* SVGClipPathElement::WrapNode(JSContext* aCx,
                                       JS::Handle<JSObject*> aGivenProto) {
  return SVGClipPathElement_Binding::Wrap(aCx, this, aGivenProto);
}

SVGElement::EnumInfo SVGClipPathElement::sEnumInfo[1] = {
    {nsGkAtoms::clipPathUnits, sSVGUnitTypesMap, SVG_UNIT_TYPE_USERSPACEONUSE}};

//----------------------------------------------------------------------
// Implementation

SVGClipPathElement::SVGClipPathElement(
    already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
    : SVGClipPathElementBase(std::move(aNodeInfo)) {}

already_AddRefed<DOMSVGAnimatedEnumeration>
SVGClipPathElement::ClipPathUnits() {
  return mEnumAttributes[CLIPPATHUNITS].ToDOMAnimatedEnum(this);
}

SVGElement::EnumAttributesInfo SVGClipPathElement::GetEnumInfo() {
  return EnumAttributesInfo(mEnumAttributes, sEnumInfo, ArrayLength(sEnumInfo));
}

bool SVGClipPathElement::IsUnitsObjectBoundingBox() const {
  return mEnumAttributes[CLIPPATHUNITS].GetAnimValue() ==
         SVG_UNIT_TYPE_OBJECTBOUNDINGBOX;
}

//----------------------------------------------------------------------
// nsINode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGClipPathElement)

}  // namespace dom
}  // namespace mozilla
