/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGForeignObjectElement_h
#define mozilla_dom_SVGForeignObjectElement_h

#include "mozilla/dom/SVGGraphicsElement.h"
#include "SVGAnimatedLength.h"

nsresult NS_NewSVGForeignObjectElement(
    nsIContent** aResult, already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

class nsSVGForeignObjectFrame;

namespace mozilla {
namespace dom {

class SVGForeignObjectElement final : public SVGGraphicsElement {
  friend class ::nsSVGForeignObjectFrame;

 protected:
  friend nsresult(::NS_NewSVGForeignObjectElement(
      nsIContent** aResult,
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo));
  explicit SVGForeignObjectElement(
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);
  virtual JSObject* WrapNode(JSContext* cx,
                             JS::Handle<JSObject*> aGivenProto) override;

 public:
  // SVGElement specializations:
  virtual gfxMatrix PrependLocalTransformsTo(
      const gfxMatrix& aMatrix,
      SVGTransformTypes aWhich = eAllTransforms) const override;
  virtual bool HasValidDimensions() const override;

  // nsIContent interface
  NS_IMETHOD_(bool) IsAttributeMapped(const nsAtom* name) const override;

  virtual nsresult Clone(dom::NodeInfo*, nsINode** aResult) const override;

  // WebIDL
  already_AddRefed<DOMSVGAnimatedLength> X();
  already_AddRefed<DOMSVGAnimatedLength> Y();
  already_AddRefed<DOMSVGAnimatedLength> Width();
  already_AddRefed<DOMSVGAnimatedLength> Height();

 protected:
  virtual LengthAttributesInfo GetLengthInfo() override;

  enum { ATTR_X, ATTR_Y, ATTR_WIDTH, ATTR_HEIGHT };
  SVGAnimatedLength mLengthAttributes[4];
  static LengthInfo sLengthInfo[4];
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_SVGForeignObjectElement_h
