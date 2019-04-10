/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGViewElement_h
#define mozilla_dom_SVGViewElement_h

#include "SVGAnimatedEnumeration.h"
#include "SVGAnimatedPreserveAspectRatio.h"
#include "SVGAnimatedViewBox.h"
#include "SVGStringList.h"
#include "mozilla/dom/SVGElement.h"

class nsSVGOuterSVGFrame;

nsresult NS_NewSVGViewElement(
    nsIContent** aResult, already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

namespace mozilla {
class SVGFragmentIdentifier;

namespace dom {
class SVGViewportElement;

typedef SVGElement SVGViewElementBase;

class SVGViewElement : public SVGViewElementBase {
 protected:
  friend class mozilla::SVGFragmentIdentifier;
  friend class SVGSVGElement;
  friend class SVGViewportElement;
  friend class ::nsSVGOuterSVGFrame;
  explicit SVGViewElement(already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);
  friend nsresult(::NS_NewSVGViewElement(
      nsIContent** aResult,
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo));
  virtual JSObject* WrapNode(JSContext* cx,
                             JS::Handle<JSObject*> aGivenProto) override;

 public:
  virtual nsresult Clone(dom::NodeInfo*, nsINode** aResult) const override;

  // WebIDL
  uint16_t ZoomAndPan() { return mEnumAttributes[ZOOMANDPAN].GetAnimValue(); }
  void SetZoomAndPan(uint16_t aZoomAndPan, ErrorResult& rv);
  already_AddRefed<SVGAnimatedRect> ViewBox();
  already_AddRefed<DOMSVGAnimatedPreserveAspectRatio> PreserveAspectRatio();

 private:
  // SVGElement overrides

  virtual EnumAttributesInfo GetEnumInfo() override;

  enum { ZOOMANDPAN };
  SVGAnimatedEnumeration mEnumAttributes[1];
  static SVGEnumMapping sZoomAndPanMap[];
  static EnumInfo sEnumInfo[1];

  virtual SVGAnimatedViewBox* GetAnimatedViewBox() override;
  virtual SVGAnimatedPreserveAspectRatio* GetAnimatedPreserveAspectRatio()
      override;

  SVGAnimatedViewBox mViewBox;
  SVGAnimatedPreserveAspectRatio mPreserveAspectRatio;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_SVGViewElement_h
