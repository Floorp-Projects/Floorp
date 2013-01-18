/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGPatternElement_h
#define mozilla_dom_SVGPatternElement_h

#include "nsIDOMSVGFitToViewBox.h"
#include "nsIDOMSVGPatternElement.h"
#include "nsIDOMSVGUnitTypes.h"
#include "nsIDOMSVGURIReference.h"
#include "nsSVGEnum.h"
#include "nsSVGLength2.h"
#include "nsSVGString.h"
#include "nsSVGElement.h"
#include "nsSVGViewBox.h"
#include "SVGAnimatedPreserveAspectRatio.h"
#include "SVGAnimatedTransformList.h"

class nsSVGPatternFrame;

nsresult NS_NewSVGPatternElement(nsIContent **aResult,
                                 already_AddRefed<nsINodeInfo> aNodeInfo);

namespace mozilla {
class DOMSVGAnimatedTransformList;

namespace dom {

typedef nsSVGElement SVGPatternElementBase;

class SVGPatternElement MOZ_FINAL : public SVGPatternElementBase,
                                    public nsIDOMSVGPatternElement,
                                    public nsIDOMSVGURIReference,
                                    public nsIDOMSVGFitToViewBox,
                                    public nsIDOMSVGUnitTypes
{
  friend class ::nsSVGPatternFrame;

protected:
  friend nsresult (::NS_NewSVGPatternElement(nsIContent **aResult,
                                             already_AddRefed<nsINodeInfo> aNodeInfo));
  SVGPatternElement(already_AddRefed<nsINodeInfo> aNodeInfo);
  virtual JSObject* WrapNode(JSContext *cx, JSObject *scope, bool *triedToWrap) MOZ_OVERRIDE;

public:
  typedef mozilla::SVGAnimatedPreserveAspectRatio SVGAnimatedPreserveAspectRatio;

  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED

  // Pattern Element
  NS_DECL_NSIDOMSVGPATTERNELEMENT

  // URI Reference
  NS_DECL_NSIDOMSVGURIREFERENCE

  // FitToViewbox
  NS_DECL_NSIDOMSVGFITTOVIEWBOX

  NS_FORWARD_NSIDOMNODE_TO_NSINODE
  NS_FORWARD_NSIDOMELEMENT_TO_GENERIC
  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGElement::)

  // nsIContent interface
  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom* name) const;

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  virtual nsXPCClassInfo* GetClassInfo();

  virtual nsIDOMNode* AsDOMNode() { return this; }

  // nsSVGSVGElement methods:
  virtual bool HasValidDimensions() const;

  virtual mozilla::SVGAnimatedTransformList*
    GetAnimatedTransformList(uint32_t aFlags = 0);
  virtual nsIAtom* GetTransformListAttrName() const {
    return nsGkAtoms::patternTransform;
  }

  // WebIDL
  already_AddRefed<nsIDOMSVGAnimatedRect> ViewBox();
  already_AddRefed<DOMSVGAnimatedPreserveAspectRatio> PreserveAspectRatio();
  already_AddRefed<nsIDOMSVGAnimatedEnumeration> PatternUnits();
  already_AddRefed<nsIDOMSVGAnimatedEnumeration> PatternContentUnits();
  already_AddRefed<DOMSVGAnimatedTransformList> PatternTransform();
  already_AddRefed<nsIDOMSVGAnimatedLength> X();
  already_AddRefed<nsIDOMSVGAnimatedLength> Y();
  already_AddRefed<nsIDOMSVGAnimatedLength> Width();
  already_AddRefed<nsIDOMSVGAnimatedLength> Height();
  already_AddRefed<nsIDOMSVGAnimatedString> Href();

protected:

  virtual LengthAttributesInfo GetLengthInfo();
  virtual EnumAttributesInfo GetEnumInfo();
  virtual nsSVGViewBox *GetViewBox();
  virtual SVGAnimatedPreserveAspectRatio *GetPreserveAspectRatio();
  virtual StringAttributesInfo GetStringInfo();

  // nsIDOMSVGPatternElement values
  enum { ATTR_X, ATTR_Y, ATTR_WIDTH, ATTR_HEIGHT };
  nsSVGLength2 mLengthAttributes[4];
  static LengthInfo sLengthInfo[4];

  enum { PATTERNUNITS, PATTERNCONTENTUNITS };
  nsSVGEnum mEnumAttributes[2];
  static EnumInfo sEnumInfo[2];

  nsAutoPtr<mozilla::SVGAnimatedTransformList> mPatternTransform;

  // nsIDOMSVGURIReference properties
  enum { HREF };
  nsSVGString mStringAttributes[1];
  static StringInfo sStringInfo[1];

  // nsIDOMSVGFitToViewbox properties
  nsSVGViewBox mViewBox;
  SVGAnimatedPreserveAspectRatio mPreserveAspectRatio;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_SVGPatternElement_h
