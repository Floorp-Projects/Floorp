/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGSetElement_h
#define mozilla_dom_SVGSetElement_h

#include "mozilla/Attributes.h"
#include "mozilla/dom/SVGAnimationElement.h"
#include "nsSMILSetAnimationFunction.h"

nsresult NS_NewSVGSetElement(nsIContent **aResult,
                             already_AddRefed<nsINodeInfo>&& aNodeInfo);

namespace mozilla {
namespace dom {

class SVGSetElement MOZ_FINAL : public SVGAnimationElement
{
protected:
  SVGSetElement(already_AddRefed<nsINodeInfo>& aNodeInfo);

  nsSMILSetAnimationFunction mAnimationFunction;

  friend nsresult (::NS_NewSVGSetElement(nsIContent **aResult,
                                         already_AddRefed<nsINodeInfo>&& aNodeInfo));

  virtual JSObject* WrapNode(JSContext *aCx,
                             JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;

public:
  // nsIDOMNode
  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const MOZ_OVERRIDE;

  // SVGAnimationElement
  virtual nsSMILAnimationFunction& AnimationFunction();
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_SVGSetElement_h
