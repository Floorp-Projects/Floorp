/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGMaskElement_h
#define mozilla_dom_SVGMaskElement_h

#include "nsSVGEnum.h"
#include "nsSVGLength2.h"
#include "nsSVGElement.h"

class nsSVGMaskFrame;

nsresult NS_NewSVGMaskElement(nsIContent **aResult,
                              already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

namespace mozilla {
namespace dom {

//--------------------- Masks ------------------------

typedef nsSVGElement SVGMaskElementBase;

class SVGMaskElement final : public SVGMaskElementBase
{
  friend class ::nsSVGMaskFrame;

protected:
  friend nsresult (::NS_NewSVGMaskElement(nsIContent **aResult,
                                          already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo));
  explicit SVGMaskElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo);
  virtual JSObject* WrapNode(JSContext *cx, JS::Handle<JSObject*> aGivenProto) override;

public:
  // nsIContent interface
  virtual nsresult Clone(mozilla::dom::NodeInfo *aNodeInfo, nsINode **aResult,
                         bool aPreallocateChildren) const override;
  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom* aAttribute) const override;

  // nsSVGSVGElement methods:
  virtual bool HasValidDimensions() const override;

  // WebIDL
  already_AddRefed<SVGAnimatedEnumeration> MaskUnits();
  already_AddRefed<SVGAnimatedEnumeration> MaskContentUnits();
  already_AddRefed<SVGAnimatedLength> X();
  already_AddRefed<SVGAnimatedLength> Y();
  already_AddRefed<SVGAnimatedLength> Width();
  already_AddRefed<SVGAnimatedLength> Height();

protected:

  virtual LengthAttributesInfo GetLengthInfo() override;
  virtual EnumAttributesInfo GetEnumInfo() override;

  enum { ATTR_X, ATTR_Y, ATTR_WIDTH, ATTR_HEIGHT };
  nsSVGLength2 mLengthAttributes[4];
  static LengthInfo sLengthInfo[4];

  enum { MASKUNITS, MASKCONTENTUNITS };
  nsSVGEnum mEnumAttributes[2];
  static EnumInfo sEnumInfo[2];
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_SVGMaskElement_h
