/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGPatternElement_h
#define mozilla_dom_SVGPatternElement_h

#include "nsSVGEnum.h"
#include "nsSVGLength2.h"
#include "nsSVGString.h"
#include "nsSVGElement.h"
#include "nsSVGViewBox.h"
#include "SVGAnimatedPreserveAspectRatio.h"
#include "nsSVGAnimatedTransformList.h"

class nsSVGPatternFrame;

nsresult NS_NewSVGPatternElement(nsIContent **aResult,
                                 already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

namespace mozilla {
namespace dom {
class SVGAnimatedTransformList;

typedef nsSVGElement SVGPatternElementBase;

class SVGPatternElement final : public SVGPatternElementBase
{
  friend class ::nsSVGPatternFrame;

protected:
  friend nsresult (::NS_NewSVGPatternElement(nsIContent **aResult,
                                             already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo));
  explicit SVGPatternElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo);
  virtual JSObject* WrapNode(JSContext *cx, JS::Handle<JSObject*> aGivenProto) override;

public:
  typedef mozilla::SVGAnimatedPreserveAspectRatio SVGAnimatedPreserveAspectRatio;

  // nsIContent interface
  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom* name) const override;

  virtual nsresult Clone(mozilla::dom::NodeInfo *aNodeInfo, nsINode **aResult) const override;

  // nsSVGSVGElement methods:
  virtual bool HasValidDimensions() const override;

  virtual mozilla::nsSVGAnimatedTransformList*
    GetAnimatedTransformList(uint32_t aFlags = 0) override;
  virtual nsIAtom* GetTransformListAttrName() const override {
    return nsGkAtoms::patternTransform;
  }

  // WebIDL
  already_AddRefed<SVGAnimatedRect> ViewBox();
  already_AddRefed<DOMSVGAnimatedPreserveAspectRatio> PreserveAspectRatio();
  already_AddRefed<SVGAnimatedEnumeration> PatternUnits();
  already_AddRefed<SVGAnimatedEnumeration> PatternContentUnits();
  already_AddRefed<SVGAnimatedTransformList> PatternTransform();
  already_AddRefed<SVGAnimatedLength> X();
  already_AddRefed<SVGAnimatedLength> Y();
  already_AddRefed<SVGAnimatedLength> Width();
  already_AddRefed<SVGAnimatedLength> Height();
  already_AddRefed<SVGAnimatedString> Href();

protected:

  virtual LengthAttributesInfo GetLengthInfo() override;
  virtual EnumAttributesInfo GetEnumInfo() override;
  virtual nsSVGViewBox *GetViewBox() override;
  virtual SVGAnimatedPreserveAspectRatio *GetPreserveAspectRatio() override;
  virtual StringAttributesInfo GetStringInfo() override;

  enum { ATTR_X, ATTR_Y, ATTR_WIDTH, ATTR_HEIGHT };
  nsSVGLength2 mLengthAttributes[4];
  static LengthInfo sLengthInfo[4];

  enum { PATTERNUNITS, PATTERNCONTENTUNITS };
  nsSVGEnum mEnumAttributes[2];
  static EnumInfo sEnumInfo[2];

  nsAutoPtr<mozilla::nsSVGAnimatedTransformList> mPatternTransform;

  enum { HREF };
  nsSVGString mStringAttributes[1];
  static StringInfo sStringInfo[1];

  // SVGFitToViewbox properties
  nsSVGViewBox mViewBox;
  SVGAnimatedPreserveAspectRatio mPreserveAspectRatio;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_SVGPatternElement_h
