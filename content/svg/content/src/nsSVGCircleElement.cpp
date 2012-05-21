/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Util.h"

#include "nsSVGPathGeometryElement.h"
#include "nsIDOMSVGCircleElement.h"
#include "nsSVGLength2.h"
#include "nsGkAtoms.h"
#include "nsSVGUtils.h"
#include "gfxContext.h"

using namespace mozilla;

typedef nsSVGPathGeometryElement nsSVGCircleElementBase;

class nsSVGCircleElement : public nsSVGCircleElementBase,
                           public nsIDOMSVGCircleElement
{
protected:
  friend nsresult NS_NewSVGCircleElement(nsIContent **aResult,
                                         already_AddRefed<nsINodeInfo> aNodeInfo);
  nsSVGCircleElement(already_AddRefed<nsINodeInfo> aNodeInfo);

public:
  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMSVGCIRCLEELEMENT

  // xxx I wish we could use virtual inheritance
  NS_FORWARD_NSIDOMNODE(nsSVGCircleElementBase::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGCircleElementBase::)
  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGCircleElementBase::)

  // nsSVGSVGElement methods:
  virtual bool HasValidDimensions() const;

  // nsSVGPathGeometryElement methods:
  virtual void ConstructPath(gfxContext *aCtx);

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  virtual nsXPCClassInfo* GetClassInfo();

  virtual nsIDOMNode* AsDOMNode() { return this; }
protected:

  virtual LengthAttributesInfo GetLengthInfo();

  enum { CX, CY, R };
  nsSVGLength2 mLengthAttributes[3];
  static LengthInfo sLengthInfo[3];
};

nsSVGElement::LengthInfo nsSVGCircleElement::sLengthInfo[3] =
{
  { &nsGkAtoms::cx, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER, nsSVGUtils::X },
  { &nsGkAtoms::cy, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER, nsSVGUtils::Y },
  { &nsGkAtoms::r, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER, nsSVGUtils::XY }
};

NS_IMPL_NS_NEW_SVG_ELEMENT(Circle)

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGCircleElement,nsSVGCircleElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGCircleElement,nsSVGCircleElementBase)

DOMCI_NODE_DATA(SVGCircleElement, nsSVGCircleElement)

NS_INTERFACE_TABLE_HEAD(nsSVGCircleElement)
  NS_NODE_INTERFACE_TABLE5(nsSVGCircleElement, nsIDOMNode, nsIDOMElement,
                           nsIDOMSVGElement, nsIDOMSVGTests,
                           nsIDOMSVGCircleElement)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGCircleElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGCircleElementBase)

//----------------------------------------------------------------------
// Implementation

nsSVGCircleElement::nsSVGCircleElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : nsSVGCircleElementBase(aNodeInfo)
{
}

//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(nsSVGCircleElement)

//----------------------------------------------------------------------
// nsIDOMSVGCircleElement methods

/* readonly attribute nsIDOMSVGAnimatedLength cx; */
NS_IMETHODIMP nsSVGCircleElement::GetCx(nsIDOMSVGAnimatedLength * *aCx)
{
  return mLengthAttributes[CX].ToDOMAnimatedLength(aCx, this);
}

/* readonly attribute nsIDOMSVGAnimatedLength cy; */
NS_IMETHODIMP nsSVGCircleElement::GetCy(nsIDOMSVGAnimatedLength * *aCy)
{
  return mLengthAttributes[CY].ToDOMAnimatedLength(aCy, this);
}

/* readonly attribute nsIDOMSVGAnimatedLength r; */
NS_IMETHODIMP nsSVGCircleElement::GetR(nsIDOMSVGAnimatedLength * *aR)
{
  return mLengthAttributes[R].ToDOMAnimatedLength(aR, this);
}

//----------------------------------------------------------------------
// nsSVGElement methods

/* virtual */ bool
nsSVGCircleElement::HasValidDimensions() const
{
  return mLengthAttributes[R].IsExplicitlySet() &&
         mLengthAttributes[R].GetAnimValInSpecifiedUnits() > 0;
}

nsSVGElement::LengthAttributesInfo
nsSVGCircleElement::GetLengthInfo()
{
  return LengthAttributesInfo(mLengthAttributes, sLengthInfo,
                              ArrayLength(sLengthInfo));
}

//----------------------------------------------------------------------
// nsSVGPathGeometryElement methods

void
nsSVGCircleElement::ConstructPath(gfxContext *aCtx)
{
  float x, y, r;

  GetAnimatedLengthValues(&x, &y, &r, nsnull);

  if (r > 0.0f)
    aCtx->Arc(gfxPoint(x, y), r, 0, 2*M_PI);
}
