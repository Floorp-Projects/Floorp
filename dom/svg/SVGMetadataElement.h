/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_SVG_SVGMETADATAELEMENT_H_
#define DOM_SVG_SVGMETADATAELEMENT_H_

#include "mozilla/Attributes.h"
#include "SVGElement.h"

nsresult NS_NewSVGMetadataElement(
    nsIContent** aResult, already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

namespace mozilla {
namespace dom {

using SVGMetadataElementBase = SVGElement;

class SVGMetadataElement final : public SVGMetadataElementBase {
 protected:
  friend nsresult(::NS_NewSVGMetadataElement(
      nsIContent** aResult,
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo));
  explicit SVGMetadataElement(
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

  virtual JSObject* WrapNode(JSContext* aCx,
                             JS::Handle<JSObject*> aGivenProto) override;
  nsresult Init();

 public:
  virtual nsresult Clone(dom::NodeInfo*, nsINode** aResult) const override;
};

}  // namespace dom
}  // namespace mozilla

#endif  // DOM_SVG_SVGMETADATAELEMENT_H_
