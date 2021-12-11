/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGViewElement.h"
#include "mozilla/dom/SVGViewElementBinding.h"

NS_IMPL_NS_NEW_SVG_ELEMENT(View)

namespace mozilla {
namespace dom {

using namespace SVGViewElement_Binding;

JSObject* SVGViewElement::WrapNode(JSContext* aCx,
                                   JS::Handle<JSObject*> aGivenProto) {
  return SVGViewElement_Binding::Wrap(aCx, this, aGivenProto);
}

SVGEnumMapping SVGViewElement::sZoomAndPanMap[] = {
    {nsGkAtoms::disable, SVG_ZOOMANDPAN_DISABLE},
    {nsGkAtoms::magnify, SVG_ZOOMANDPAN_MAGNIFY},
    {nullptr, 0}};

SVGElement::EnumInfo SVGViewElement::sEnumInfo[1] = {
    {nsGkAtoms::zoomAndPan, sZoomAndPanMap, SVG_ZOOMANDPAN_MAGNIFY}};

//----------------------------------------------------------------------
// Implementation

SVGViewElement::SVGViewElement(
    already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
    : SVGViewElementBase(std::move(aNodeInfo)) {}

//----------------------------------------------------------------------
// nsINode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGViewElement)

void SVGViewElement::SetZoomAndPan(uint16_t aZoomAndPan, ErrorResult& rv) {
  if (aZoomAndPan == SVG_ZOOMANDPAN_DISABLE ||
      aZoomAndPan == SVG_ZOOMANDPAN_MAGNIFY) {
    ErrorResult nestedRv;
    mEnumAttributes[ZOOMANDPAN].SetBaseValue(aZoomAndPan, this, nestedRv);
    MOZ_ASSERT(!nestedRv.Failed(),
               "We already validated our aZoomAndPan value!");
    return;
  }

  rv.ThrowRangeError<MSG_INVALID_ZOOMANDPAN_VALUE_ERROR>();
}

//----------------------------------------------------------------------

already_AddRefed<SVGAnimatedRect> SVGViewElement::ViewBox() {
  return mViewBox.ToSVGAnimatedRect(this);
}

already_AddRefed<DOMSVGAnimatedPreserveAspectRatio>
SVGViewElement::PreserveAspectRatio() {
  return mPreserveAspectRatio.ToDOMAnimatedPreserveAspectRatio(this);
}

//----------------------------------------------------------------------
// SVGElement methods

SVGElement::EnumAttributesInfo SVGViewElement::GetEnumInfo() {
  return EnumAttributesInfo(mEnumAttributes, sEnumInfo, ArrayLength(sEnumInfo));
}

SVGAnimatedViewBox* SVGViewElement::GetAnimatedViewBox() { return &mViewBox; }

SVGAnimatedPreserveAspectRatio*
SVGViewElement::GetAnimatedPreserveAspectRatio() {
  return &mPreserveAspectRatio;
}

}  // namespace dom
}  // namespace mozilla
