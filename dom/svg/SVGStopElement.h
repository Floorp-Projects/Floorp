/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGStopElement_h
#define mozilla_dom_SVGStopElement_h

#include "mozilla/dom/SVGElement.h"
#include "SVGAnimatedNumber.h"

nsresult NS_NewSVGStopElement(
    nsIContent** aResult, already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

namespace mozilla {
namespace dom {

typedef SVGElement SVGStopElementBase;

class SVGStopElement final : public SVGStopElementBase {
 protected:
  friend nsresult(::NS_NewSVGStopElement(
      nsIContent** aResult,
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo));
  explicit SVGStopElement(already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);
  virtual JSObject* WrapNode(JSContext* aCx,
                             JS::Handle<JSObject*> aGivenProto) override;

 public:
  // nsIContent interface
  NS_IMETHOD_(bool) IsAttributeMapped(const nsAtom* aAttribute) const override;

  virtual nsresult Clone(dom::NodeInfo*, nsINode** aResult) const override;

  // WebIDL
  already_AddRefed<DOMSVGAnimatedNumber> Offset();

 protected:
  virtual NumberAttributesInfo GetNumberInfo() override;
  SVGAnimatedNumber mOffset;
  static NumberInfo sNumberInfo;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_SVGStopElement_h
