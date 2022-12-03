/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_SVG_SVGCLIPPATHELEMENT_H_
#define DOM_SVG_SVGCLIPPATHELEMENT_H_

#include "SVGAnimatedEnumeration.h"
#include "mozilla/dom/SVGTransformableElement.h"

nsresult NS_NewSVGClipPathElement(
    nsIContent** aResult, already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

namespace mozilla {
class SVGClipPathFrame;

namespace dom {

using SVGClipPathElementBase = SVGTransformableElement;

class SVGClipPathElement final : public SVGClipPathElementBase {
  friend class mozilla::SVGClipPathFrame;

 protected:
  friend nsresult(::NS_NewSVGClipPathElement(
      nsIContent** aResult,
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo));
  explicit SVGClipPathElement(
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);
  JSObject* WrapNode(JSContext* cx, JS::Handle<JSObject*> aGivenProto) override;

 public:
  nsresult Clone(dom::NodeInfo*, nsINode** aResult) const override;

  // WebIDL
  already_AddRefed<DOMSVGAnimatedEnumeration> ClipPathUnits();

  // This is an internal method that does not flush style, and thus
  // the answer may be out of date if there's a pending style flush.
  bool IsUnitsObjectBoundingBox() const;

 protected:
  enum { CLIPPATHUNITS };
  SVGAnimatedEnumeration mEnumAttributes[1];
  static EnumInfo sEnumInfo[1];

  EnumAttributesInfo GetEnumInfo() override;
};

}  // namespace dom
}  // namespace mozilla

#endif  // DOM_SVG_SVGCLIPPATHELEMENT_H_
