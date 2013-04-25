/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGLineElement_h
#define mozilla_dom_SVGLineElement_h

#include "nsSVGPathGeometryElement.h"
#include "nsSVGLength2.h"

nsresult NS_NewSVGLineElement(nsIContent **aResult,
                              already_AddRefed<nsINodeInfo> aNodeInfo);

namespace mozilla {
namespace dom {

typedef nsSVGPathGeometryElement SVGLineElementBase;

class SVGLineElement MOZ_FINAL : public SVGLineElementBase
{
protected:
  SVGLineElement(already_AddRefed<nsINodeInfo> aNodeInfo);
  virtual JSObject* WrapNode(JSContext *cx,
                             JS::Handle<JSObject*> scope) MOZ_OVERRIDE;
  friend nsresult (::NS_NewSVGLineElement(nsIContent **aResult,
                                          already_AddRefed<nsINodeInfo> aNodeInfo));

public:
  // nsIContent interface
  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom* name) const;

  // nsSVGPathGeometryElement methods:
  virtual bool IsMarkable() { return true; }
  virtual void GetMarkPoints(nsTArray<nsSVGMark> *aMarks);
  virtual void ConstructPath(gfxContext *aCtx);

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  // WebIDL
  already_AddRefed<SVGAnimatedLength> X1();
  already_AddRefed<SVGAnimatedLength> Y1();
  already_AddRefed<SVGAnimatedLength> X2();
  already_AddRefed<SVGAnimatedLength> Y2();

protected:

  virtual LengthAttributesInfo GetLengthInfo();

  enum { ATTR_X1, ATTR_Y1, ATTR_X2, ATTR_Y2 };
  nsSVGLength2 mLengthAttributes[4];
  static LengthInfo sLengthInfo[4];
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_SVGLineElement_h
