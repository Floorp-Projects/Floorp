/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGGElement_h
#define mozilla_dom_SVGGElement_h

#include "mozilla/dom/SVGGraphicsElement.h"

nsresult NS_NewSVGGElement(nsIContent **aResult,
                           already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

namespace mozilla {
namespace dom {

class SVGGElement final : public SVGGraphicsElement
{
protected:
  explicit SVGGElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo);
  virtual JSObject* WrapNode(JSContext *cx, JS::Handle<JSObject*> aGivenProto) override;
  friend nsresult (::NS_NewSVGGElement(nsIContent **aResult,
                                       already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo));

public:
  // nsIContent
  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom* aAttribute) const override;

  virtual nsresult Clone(mozilla::dom::NodeInfo *aNodeInfo, nsINode **aResult) const override;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_SVGGElement_h
