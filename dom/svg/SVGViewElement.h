/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_SVG_SVGVIEWELEMENT_H_
#define DOM_SVG_SVGVIEWELEMENT_H_

#include "SVGAnimatedEnumeration.h"
#include "SVGAnimatedPreserveAspectRatio.h"
#include "SVGAnimatedViewBox.h"
#include "SVGStringList.h"
#include "mozilla/dom/SVGElement.h"

nsresult NS_NewSVGViewElement(
    nsIContent** aResult, already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

namespace mozilla {
class SVGFragmentIdentifier;
class SVGOuterSVGFrame;

namespace dom {
class SVGViewportElement;

using SVGViewElementBase = SVGElement;

class SVGViewElement final : public SVGViewElementBase {
 protected:
  friend class mozilla::SVGFragmentIdentifier;
  friend class mozilla::SVGOuterSVGFrame;
  friend class SVGSVGElement;
  friend class SVGViewportElement;
  explicit SVGViewElement(already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);
  friend nsresult(::NS_NewSVGViewElement(
      nsIContent** aResult,
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo));
  JSObject* WrapNode(JSContext* cx, JS::Handle<JSObject*> aGivenProto) override;

 public:
  NS_IMPL_FROMNODE_WITH_TAG(SVGViewElement, kNameSpaceID_SVG, view)

  nsresult Clone(dom::NodeInfo*, nsINode** aResult) const override;

  // WebIDL
  uint16_t ZoomAndPan() { return mEnumAttributes[ZOOMANDPAN].GetAnimValue(); }
  void SetZoomAndPan(uint16_t aZoomAndPan, ErrorResult& rv);
  already_AddRefed<SVGAnimatedRect> ViewBox();
  already_AddRefed<DOMSVGAnimatedPreserveAspectRatio> PreserveAspectRatio();

 private:
  // SVGElement overrides

  EnumAttributesInfo GetEnumInfo() override;

  enum { ZOOMANDPAN };
  SVGAnimatedEnumeration mEnumAttributes[1];
  static SVGEnumMapping sZoomAndPanMap[];
  static EnumInfo sEnumInfo[1];

  SVGAnimatedViewBox* GetAnimatedViewBox() override;
  SVGAnimatedPreserveAspectRatio* GetAnimatedPreserveAspectRatio() override;

  SVGAnimatedViewBox mViewBox;
  SVGAnimatedPreserveAspectRatio mPreserveAspectRatio;
};

}  // namespace dom
}  // namespace mozilla

#endif  // DOM_SVG_SVGVIEWELEMENT_H_
