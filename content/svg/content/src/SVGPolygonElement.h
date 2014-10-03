/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

class SVGPolygonElement MOZ_FINAL : public SVGPolygonElementBase
{
protected:
  explicit SVGPolygonElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo);
  virtual JSObject* WrapNode(JSContext *cx) MOZ_OVERRIDE;
  friend nsresult (::NS_NewSVGPolygonElement(nsIContent **aResult,
                                             already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo));

public:
  // nsSVGPathGeometryElement methods:
  virtual void GetMarkPoints(nsTArray<nsSVGMark> *aMarks) MOZ_OVERRIDE;

  virtual nsresult Clone(mozilla::dom::NodeInfo *aNodeInfo, nsINode **aResult) const;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_SVGPolygonElement_h
