/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_SVG_SVGPOLYGONELEMENT_H_
#define DOM_SVG_SVGPOLYGONELEMENT_H_

#include "mozilla/Attributes.h"
#include "SVGPolyElement.h"

nsresult NS_NewSVGPolygonElement(
    nsIContent** aResult, already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

namespace mozilla::dom {

using SVGPolygonElementBase = SVGPolyElement;

class SVGPolygonElement final : public SVGPolygonElementBase {
 protected:
  explicit SVGPolygonElement(
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);
  JSObject* WrapNode(JSContext* cx, JS::Handle<JSObject*> aGivenProto) override;
  friend nsresult(::NS_NewSVGPolygonElement(
      nsIContent** aResult,
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo));

 public:
  // SVGGeometryElement methods:
  void GetMarkPoints(nsTArray<SVGMark>* aMarks) override;
  already_AddRefed<Path> BuildPath(PathBuilder* aBuilder) override;
  bool IsClosedLoop() const override { return true; }

  nsresult Clone(dom::NodeInfo*, nsINode** aResult) const override;
};

}  // namespace mozilla::dom

#endif  // DOM_SVG_SVGPOLYGONELEMENT_H_
