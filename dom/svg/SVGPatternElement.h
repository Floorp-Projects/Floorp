/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_SVG_SVGPATTERNELEMENT_H_
#define DOM_SVG_SVGPATTERNELEMENT_H_

#include "SVGAnimatedEnumeration.h"
#include "SVGAnimatedLength.h"
#include "SVGAnimatedPreserveAspectRatio.h"
#include "SVGAnimatedString.h"
#include "SVGAnimatedTransformList.h"
#include "SVGAnimatedViewBox.h"
#include "mozilla/dom/SVGElement.h"
#include "mozilla/UniquePtr.h"

nsresult NS_NewSVGPatternElement(
    nsIContent** aResult, already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

namespace mozilla {
class SVGPatternFrame;

namespace dom {
class DOMSVGAnimatedTransformList;

using SVGPatternElementBase = SVGElement;

class SVGPatternElement final : public SVGPatternElementBase {
  friend class mozilla::SVGPatternFrame;

 protected:
  friend nsresult(::NS_NewSVGPatternElement(
      nsIContent** aResult,
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo));
  explicit SVGPatternElement(
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);
  virtual JSObject* WrapNode(JSContext* cx,
                             JS::Handle<JSObject*> aGivenProto) override;

 public:
  // nsIContent interface
  NS_IMETHOD_(bool) IsAttributeMapped(const nsAtom* name) const override;

  virtual nsresult Clone(dom::NodeInfo*, nsINode** aResult) const override;

  // SVGSVGElement methods:
  virtual bool HasValidDimensions() const override;

  virtual mozilla::SVGAnimatedTransformList* GetAnimatedTransformList(
      uint32_t aFlags = 0) override;
  virtual nsStaticAtom* GetTransformListAttrName() const override {
    return nsGkAtoms::patternTransform;
  }

  // WebIDL
  already_AddRefed<SVGAnimatedRect> ViewBox();
  already_AddRefed<DOMSVGAnimatedPreserveAspectRatio> PreserveAspectRatio();
  already_AddRefed<DOMSVGAnimatedEnumeration> PatternUnits();
  already_AddRefed<DOMSVGAnimatedEnumeration> PatternContentUnits();
  already_AddRefed<DOMSVGAnimatedTransformList> PatternTransform();
  already_AddRefed<DOMSVGAnimatedLength> X();
  already_AddRefed<DOMSVGAnimatedLength> Y();
  already_AddRefed<DOMSVGAnimatedLength> Width();
  already_AddRefed<DOMSVGAnimatedLength> Height();
  already_AddRefed<DOMSVGAnimatedString> Href();

 protected:
  virtual LengthAttributesInfo GetLengthInfo() override;
  virtual EnumAttributesInfo GetEnumInfo() override;
  virtual StringAttributesInfo GetStringInfo() override;
  virtual SVGAnimatedPreserveAspectRatio* GetAnimatedPreserveAspectRatio()
      override;
  virtual SVGAnimatedViewBox* GetAnimatedViewBox() override;

  enum { ATTR_X, ATTR_Y, ATTR_WIDTH, ATTR_HEIGHT };
  SVGAnimatedLength mLengthAttributes[4];
  static LengthInfo sLengthInfo[4];

  enum { PATTERNUNITS, PATTERNCONTENTUNITS };
  SVGAnimatedEnumeration mEnumAttributes[2];
  static EnumInfo sEnumInfo[2];

  UniquePtr<mozilla::SVGAnimatedTransformList> mPatternTransform;

  enum { HREF, XLINK_HREF };
  SVGAnimatedString mStringAttributes[2];
  static StringInfo sStringInfo[2];

  // SVGFitToViewbox properties
  SVGAnimatedViewBox mViewBox;
  SVGAnimatedPreserveAspectRatio mPreserveAspectRatio;
};

}  // namespace dom
}  // namespace mozilla

#endif  // DOM_SVG_SVGPATTERNELEMENT_H_
