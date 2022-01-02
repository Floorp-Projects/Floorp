/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_SVG_SVGSYMBOLELEMENT_H_
#define DOM_SVG_SVGSYMBOLELEMENT_H_

#include "SVGViewportElement.h"

nsresult NS_NewSVGSymbolElement(
    nsIContent** aResult, already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

namespace mozilla {
namespace dom {

using SVGSymbolElementBase = SVGViewportElement;

class SVGSymbolElement final : public SVGSymbolElementBase {
 protected:
  friend nsresult(::NS_NewSVGSymbolElement(
      nsIContent** aResult,
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo));
  explicit SVGSymbolElement(
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);
  ~SVGSymbolElement() = default;
  virtual JSObject* WrapNode(JSContext* cx,
                             JS::Handle<JSObject*> aGivenProto) override;

 public:
  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED

  virtual nsresult Clone(dom::NodeInfo*, nsINode** aResult) const override;
};

}  // namespace dom
}  // namespace mozilla

#endif  // DOM_SVG_SVGSYMBOLELEMENT_H_
