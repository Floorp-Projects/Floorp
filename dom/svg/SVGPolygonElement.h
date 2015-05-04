/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGPolygonElement_h
#define mozilla_dom_SVGPolygonElement_h

#include "mozilla/Attributes.h"
#include "nsSVGPolyElement.h"

nsresult NS_NewSVGPolygonElement(nsIContent **aResult,
                                 already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

typedef nsSVGPolyElement SVGPolygonElementBase;

namespace mozilla {
namespace dom {

class SVGPolygonElement final : public SVGPolygonElementBase
{
protected:
  explicit SVGPolygonElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo);
  virtual JSObject* WrapNode(JSContext *cx, JS::Handle<JSObject*> aGivenProto) override;
  friend nsresult (::NS_NewSVGPolygonElement(nsIContent **aResult,
                                             already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo));

public:
  // nsSVGPathGeometryElement methods:
  virtual void GetMarkPoints(nsTArray<nsSVGMark> *aMarks) override;
  virtual mozilla::TemporaryRef<Path> BuildPath(PathBuilder* aBuilder) override;

  virtual nsresult Clone(mozilla::dom::NodeInfo *aNodeInfo, nsINode **aResult) const override;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_SVGPolygonElement_h
