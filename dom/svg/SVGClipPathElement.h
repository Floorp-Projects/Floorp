/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGClipPathElement_h
#define mozilla_dom_SVGClipPathElement_h

#include "SVGAnimatedEnumeration.h"
#include "mozilla/dom/SVGTransformableElement.h"

class nsSVGClipPathFrame;

nsresult NS_NewSVGClipPathElement(
    nsIContent** aResult, already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

namespace mozilla {
namespace dom {

typedef SVGTransformableElement SVGClipPathElementBase;

class SVGClipPathElement final : public SVGClipPathElementBase {
  friend class ::nsSVGClipPathFrame;

 protected:
  friend nsresult(::NS_NewSVGClipPathElement(
      nsIContent** aResult,
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo));
  explicit SVGClipPathElement(
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);
  virtual JSObject* WrapNode(JSContext* cx,
                             JS::Handle<JSObject*> aGivenProto) override;

 public:
  virtual nsresult Clone(dom::NodeInfo*, nsINode** aResult) const override;

  // WebIDL
  already_AddRefed<DOMSVGAnimatedEnumeration> ClipPathUnits();

  // This is an internal method that does not flush style, and thus
  // the answer may be out of date if there's a pending style flush.
  bool IsUnitsObjectBoundingBox() const;

 protected:
  enum { CLIPPATHUNITS };
  SVGAnimatedEnumeration mEnumAttributes[1];
  static EnumInfo sEnumInfo[1];

  virtual EnumAttributesInfo GetEnumInfo() override;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_SVGClipPathElement_h
