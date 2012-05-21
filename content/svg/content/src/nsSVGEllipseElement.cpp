/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Util.h"

#include "nsSVGPathGeometryElement.h"
#include "nsIDOMSVGEllipseElement.h"
#include "nsSVGLength2.h"
#include "nsGkAtoms.h"
#include "nsSVGUtils.h"
#include "gfxContext.h"

using namespace mozilla;

typedef nsSVGPathGeometryElement nsSVGEllipseElementBase;

class nsSVGEllipseElement : public nsSVGEllipseElementBase,
                            public nsIDOMSVGEllipseElement
{
protected:
  friend nsresult NS_NewSVGEllipseElement(nsIContent **aResult,
                                          already_AddRefed<nsINodeInfo> aNodeInfo);
  nsSVGEllipseElement(already_AddRefed<nsINodeInfo> aNodeInfo);

public:
  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMSVGELLIPSEELEMENT

  // xxx I wish we could use virtual inheritance
  NS_FORWARD_NSIDOMNODE(nsSVGEllipseElementBase::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGEllipseElementBase::)
  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGEllipseElementBase::)

  // nsSVGSVGElement methods:
  virtual bool HasValidDimensions() const;

  // nsSVGPathGeometryElement methods:
  virtual void ConstructPath(gfxContext *aCtx);

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  virtual nsXPCClassInfo* GetClassInfo();

  virtual nsIDOMNode* AsDOMNode() { return this; }
protected:

  virtual LengthAttributesInfo GetLengthInfo();

  enum { CX, CY, RX, RY };
  nsSVGLength2 mLengthAttributes[4];
  static LengthInfo sLengthInfo[4];
};

nsSVGElement::LengthInfo nsSVGEllipseElement::sLengthInfo[4] =
{
  { &nsGkAtoms::cx, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER, nsSVGUtils::X },
  { &nsGkAtoms::cy, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER, nsSVGUtils::Y },
  { &nsGkAtoms::rx, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER, nsSVGUtils::X },
  { &nsGkAtoms::ry, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER, nsSVGUtils::Y },
};

NS_IMPL_NS_NEW_SVG_ELEMENT(Ellipse)

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGEllipseElement,nsSVGEllipseElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGEllipseElement,nsSVGEllipseElementBase)

DOMCI_NODE_DATA(SVGEllipseElement, nsSVGEllipseElement)

NS_INTERFACE_TABLE_HEAD(nsSVGEllipseElement)
  NS_NODE_INTERFACE_TABLE5(nsSVGEllipseElement, nsIDOMNode, nsIDOMElement,
                           nsIDOMSVGElement, nsIDOMSVGTests,
                           nsIDOMSVGEllipseElement)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGEllipseElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGEllipseElementBase)

//----------------------------------------------------------------------
// Implementation

nsSVGEllipseElement::nsSVGEllipseElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : nsSVGEllipseElementBase(aNodeInfo)
{
}

//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(nsSVGEllipseElement)

//----------------------------------------------------------------------
// nsIDOMSVGEllipseElement methods

/* readonly attribute nsIDOMSVGAnimatedLength cx; */
NS_IMETHODIMP nsSVGEllipseElement::GetCx(nsIDOMSVGAnimatedLength * *aCx)
{
  return mLengthAttributes[CX].ToDOMAnimatedLength(aCx, this);
}

/* readonly attribute nsIDOMSVGAnimatedLength cy; */
NS_IMETHODIMP nsSVGEllipseElement::GetCy(nsIDOMSVGAnimatedLength * *aCy)
{
  return mLengthAttributes[CY].ToDOMAnimatedLength(aCy, this);
}

/* readonly attribute nsIDOMSVGAnimatedLength rx; */
NS_IMETHODIMP nsSVGEllipseElement::GetRx(nsIDOMSVGAnimatedLength * *aRx)
{
  return mLengthAttributes[RX].ToDOMAnimatedLength(aRx, this);
}

/* readonly attribute nsIDOMSVGAnimatedLength ry; */
NS_IMETHODIMP nsSVGEllipseElement::GetRy(nsIDOMSVGAnimatedLength * *aRy)
{
  return mLengthAttributes[RY].ToDOMAnimatedLength(aRy, this);
}

//----------------------------------------------------------------------
// nsSVGElement methods

/* virtual */ bool
nsSVGEllipseElement::HasValidDimensions() const
{
  return mLengthAttributes[RX].IsExplicitlySet() &&
         mLengthAttributes[RX].GetAnimValInSpecifiedUnits() > 0 &&
         mLengthAttributes[RY].IsExplicitlySet() &&
         mLengthAttributes[RY].GetAnimValInSpecifiedUnits() > 0;
}

nsSVGElement::LengthAttributesInfo
nsSVGEllipseElement::GetLengthInfo()
{
  return LengthAttributesInfo(mLengthAttributes, sLengthInfo,
                              ArrayLength(sLengthInfo));
}

//----------------------------------------------------------------------
// nsSVGPathGeometryElement methods

void
nsSVGEllipseElement::ConstructPath(gfxContext *aCtx)
{
  float x, y, rx, ry;

  GetAnimatedLengthValues(&x, &y, &rx, &ry, nsnull);

  if (rx > 0.0f && ry > 0.0f) {
    aCtx->Save();
    aCtx->Translate(gfxPoint(x, y));
    aCtx->Scale(rx, ry);
    aCtx->Arc(gfxPoint(0, 0), 1, 0, 2 * M_PI);
    aCtx->Restore();
  }
}
