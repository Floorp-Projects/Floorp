/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_SVG_SVGANIMATETRANSFORMELEMENT_H_
#define DOM_SVG_SVGANIMATETRANSFORMELEMENT_H_

#include "mozilla/Attributes.h"
#include "mozilla/dom/SVGAnimationElement.h"
#include "mozilla/SMILAnimationFunction.h"

nsresult NS_NewSVGAnimateTransformElement(
    nsIContent** aResult, already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

namespace mozilla {
namespace dom {

class SVGAnimateTransformElement final : public SVGAnimationElement {
 protected:
  explicit SVGAnimateTransformElement(
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

  SMILAnimationFunction mAnimationFunction;
  friend nsresult(::NS_NewSVGAnimateTransformElement(
      nsIContent** aResult,
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo));

  virtual JSObject* WrapNode(JSContext* aCx,
                             JS::Handle<JSObject*> aGivenProto) override;

 public:
  // nsINode specializations
  virtual nsresult Clone(dom::NodeInfo*, nsINode** aResult) const override;

  // Element specializations
  virtual bool ParseAttribute(int32_t aNamespaceID, nsAtom* aAttribute,
                              const nsAString& aValue,
                              nsIPrincipal* aMaybeScriptedPrincipal,
                              nsAttrValue& aResult) override;

  // SVGAnimationElement
  virtual SMILAnimationFunction& AnimationFunction() override;
};

}  // namespace dom
}  // namespace mozilla

#endif  // DOM_SVG_SVGANIMATETRANSFORMELEMENT_H_
