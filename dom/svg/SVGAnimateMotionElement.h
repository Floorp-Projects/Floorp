/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_SVG_SVGANIMATEMOTIONELEMENT_H_
#define DOM_SVG_SVGANIMATEMOTIONELEMENT_H_

#include "mozilla/Attributes.h"
#include "mozilla/dom/SVGAnimationElement.h"
#include "SVGMotionSMILAnimationFunction.h"

nsresult NS_NewSVGAnimateMotionElement(
    nsIContent** aResult, already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

namespace mozilla {
namespace dom {

class SVGAnimateMotionElement final : public SVGAnimationElement {
 protected:
  explicit SVGAnimateMotionElement(
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

  SVGMotionSMILAnimationFunction mAnimationFunction;
  friend nsresult(::NS_NewSVGAnimateMotionElement(
      nsIContent** aResult,
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo));

  virtual JSObject* WrapNode(JSContext* aCx,
                             JS::Handle<JSObject*> aGivenProto) override;

 public:
  // nsINode specializations
  virtual nsresult Clone(dom::NodeInfo*, nsINode** aResult) const override;

  // SVGAnimationElement
  virtual SMILAnimationFunction& AnimationFunction() override;
  virtual bool GetTargetAttributeName(int32_t* aNamespaceID,
                                      nsAtom** aLocalName) const override;

  // SVGElement
  virtual nsStaticAtom* GetPathDataAttrName() const override {
    return nsGkAtoms::path;
  }

  // Utility method to let our <mpath> children tell us when they've changed,
  // so we can make sure our mAnimationFunction is marked as having changed.
  void MpathChanged() { mAnimationFunction.MpathChanged(); }
};

}  // namespace dom
}  // namespace mozilla

#endif  // DOM_SVG_SVGANIMATEMOTIONELEMENT_H_
