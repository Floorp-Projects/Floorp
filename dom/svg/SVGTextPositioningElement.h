/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGTextPositioningElement_h
#define mozilla_dom_SVGTextPositioningElement_h

#include "mozilla/dom/SVGTextContentElement.h"
#include "SVGAnimatedLengthList.h"
#include "SVGAnimatedNumberList.h"

namespace mozilla {
class SVGAnimatedLengthList;
class DOMSVGAnimatedLengthList;
class DOMSVGAnimatedNumberList;

namespace dom {
typedef SVGTextContentElement SVGTextPositioningElementBase;

class SVGTextPositioningElement : public SVGTextPositioningElementBase
{
public:
  // WebIDL
  already_AddRefed<DOMSVGAnimatedLengthList> X();
  already_AddRefed<DOMSVGAnimatedLengthList> Y();
  already_AddRefed<DOMSVGAnimatedLengthList> Dx();
  already_AddRefed<DOMSVGAnimatedLengthList> Dy();
  already_AddRefed<DOMSVGAnimatedNumberList> Rotate();

protected:

  explicit SVGTextPositioningElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo)
    : SVGTextPositioningElementBase(aNodeInfo)
  {}

  virtual LengthListAttributesInfo GetLengthListInfo() override;
  virtual NumberListAttributesInfo GetNumberListInfo() override;

  enum { ATTR_X, ATTR_Y, ATTR_DX, ATTR_DY };
  SVGAnimatedLengthList mLengthListAttributes[4];
  static LengthListInfo sLengthListInfo[4];

  enum { ROTATE };
  SVGAnimatedNumberList mNumberListAttributes[1];
  static NumberListInfo sNumberListInfo[1];
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_SVGTextPositioningElement_h
