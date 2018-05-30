/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGAnimateElement_h
#define mozilla_dom_SVGAnimateElement_h

#include "mozilla/Attributes.h"
#include "mozilla/dom/SVGAnimationElement.h"
#include "nsSMILAnimationFunction.h"

nsresult NS_NewSVGAnimateElement(nsIContent **aResult,
                                 already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

namespace mozilla {
namespace dom {

class SVGAnimateElement final : public SVGAnimationElement
{
protected:
  explicit SVGAnimateElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo);

  nsSMILAnimationFunction mAnimationFunction;
  friend nsresult
    (::NS_NewSVGAnimateElement(nsIContent **aResult,
                               already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo));

  virtual JSObject* WrapNode(JSContext *aCx, JS::Handle<JSObject*> aGivenProto) override;

public:
  // nsINode
  virtual nsresult Clone(mozilla::dom::NodeInfo *aNodeInfo, nsINode **aResult,
                         bool aPreallocateChildren) const override;

  // SVGAnimationElement
  virtual nsSMILAnimationFunction& AnimationFunction() override;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_SVGAnimateElement_h
