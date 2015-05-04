/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGLineElement_h
#define mozilla_dom_SVGLineElement_h

#include "nsSVGPathGeometryElement.h"
#include "nsSVGLength2.h"

nsresult NS_NewSVGLineElement(nsIContent **aResult,
                              already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

namespace mozilla {
namespace dom {

typedef nsSVGPathGeometryElement SVGLineElementBase;

class SVGLineElement final : public SVGLineElementBase
{
protected:
  explicit SVGLineElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo);
  virtual JSObject* WrapNode(JSContext *cx, JS::Handle<JSObject*> aGivenProto) override;
  friend nsresult (::NS_NewSVGLineElement(nsIContent **aResult,
                                          already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo));

public:
  // nsIContent interface
  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom* name) const override;

  // nsSVGPathGeometryElement methods:
  virtual bool IsMarkable() override { return true; }
  virtual void GetMarkPoints(nsTArray<nsSVGMark> *aMarks) override;
  virtual void GetAsSimplePath(SimplePath* aSimplePath) override;
  virtual TemporaryRef<Path> BuildPath(PathBuilder* aBuilder) override;
  virtual bool GetGeometryBounds(Rect* aBounds, const StrokeOptions& aStrokeOptions,
                                 const Matrix& aTransform) override;

  virtual nsresult Clone(mozilla::dom::NodeInfo *aNodeInfo, nsINode **aResult) const override;

  // WebIDL
  already_AddRefed<SVGAnimatedLength> X1();
  already_AddRefed<SVGAnimatedLength> Y1();
  already_AddRefed<SVGAnimatedLength> X2();
  already_AddRefed<SVGAnimatedLength> Y2();

protected:

  virtual LengthAttributesInfo GetLengthInfo() override;

  enum { ATTR_X1, ATTR_Y1, ATTR_X2, ATTR_Y2 };
  nsSVGLength2 mLengthAttributes[4];
  static LengthInfo sLengthInfo[4];
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_SVGLineElement_h
