/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Mozilla SVG project.
 *
 * The Initial Developer of the Original Code is
 * Crocodile Clips Ltd..
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   William Cook <william.cook@crocodile-clips.com> (original author)
 *   Alex Fritze <alex.fritze@crocodile-clips.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "mozilla/Util.h"

#include "nsSVGPathGeometryElement.h"
#include "nsIDOMSVGRectElement.h"
#include "nsSVGLength2.h"
#include "nsGkAtoms.h"
#include "gfxContext.h"

using namespace mozilla;

typedef nsSVGPathGeometryElement nsSVGRectElementBase;

class nsSVGRectElement : public nsSVGRectElementBase,
                         public nsIDOMSVGRectElement
{
protected:
  friend nsresult NS_NewSVGRectElement(nsIContent **aResult,
                                       already_AddRefed<nsINodeInfo> aNodeInfo);
  nsSVGRectElement(already_AddRefed<nsINodeInfo> aNodeInfo);

public:
  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMSVGRECTELEMENT

  // xxx I wish we could use virtual inheritance
  NS_FORWARD_NSIDOMNODE(nsSVGRectElementBase::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGRectElementBase::)
  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGRectElementBase::)

  // nsSVGPathGeometryElement methods:
  virtual void ConstructPath(gfxContext *aCtx);

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  virtual nsXPCClassInfo* GetClassInfo();
protected:

  virtual LengthAttributesInfo GetLengthInfo();
 
  enum { X, Y, WIDTH, HEIGHT, RX, RY };
  nsSVGLength2 mLengthAttributes[6];
  static LengthInfo sLengthInfo[6];
};

nsSVGElement::LengthInfo nsSVGRectElement::sLengthInfo[6] =
{
  { &nsGkAtoms::x, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER, nsSVGUtils::X },
  { &nsGkAtoms::y, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER, nsSVGUtils::Y },
  { &nsGkAtoms::width, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER, nsSVGUtils::X },
  { &nsGkAtoms::height, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER, nsSVGUtils::Y },
  { &nsGkAtoms::rx, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER, nsSVGUtils::X },
  { &nsGkAtoms::ry, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER, nsSVGUtils::Y }
};

NS_IMPL_NS_NEW_SVG_ELEMENT(Rect)

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGRectElement,nsSVGRectElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGRectElement,nsSVGRectElementBase)

DOMCI_NODE_DATA(SVGRectElement, nsSVGRectElement)

NS_INTERFACE_TABLE_HEAD(nsSVGRectElement)
  NS_NODE_INTERFACE_TABLE4(nsSVGRectElement, nsIDOMNode, nsIDOMElement,
                           nsIDOMSVGElement, nsIDOMSVGRectElement)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGRectElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGRectElementBase)

//----------------------------------------------------------------------
// Implementation

nsSVGRectElement::nsSVGRectElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : nsSVGRectElementBase(aNodeInfo)
{
}

//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(nsSVGRectElement)

//----------------------------------------------------------------------
// nsIDOMSVGRectElement methods

/* readonly attribute nsIDOMSVGAnimatedLength x; */
NS_IMETHODIMP nsSVGRectElement::GetX(nsIDOMSVGAnimatedLength * *aX)
{
  return mLengthAttributes[X].ToDOMAnimatedLength(aX, this);
}

/* readonly attribute nsIDOMSVGAnimatedLength y; */
NS_IMETHODIMP nsSVGRectElement::GetY(nsIDOMSVGAnimatedLength * *aY)
{
  return mLengthAttributes[Y].ToDOMAnimatedLength(aY, this);
}

/* readonly attribute nsIDOMSVGAnimatedLength width; */
NS_IMETHODIMP nsSVGRectElement::GetWidth(nsIDOMSVGAnimatedLength * *aWidth)
{
  return mLengthAttributes[WIDTH].ToDOMAnimatedLength(aWidth, this);
}

/* readonly attribute nsIDOMSVGAnimatedLength height; */
NS_IMETHODIMP nsSVGRectElement::GetHeight(nsIDOMSVGAnimatedLength * *aHeight)
{
  return mLengthAttributes[HEIGHT].ToDOMAnimatedLength(aHeight, this);
}

/* readonly attribute nsIDOMSVGAnimatedLength rx; */
NS_IMETHODIMP nsSVGRectElement::GetRx(nsIDOMSVGAnimatedLength * *aRx)
{
  return mLengthAttributes[RX].ToDOMAnimatedLength(aRx, this);
}

/* readonly attribute nsIDOMSVGAnimatedLength ry; */
NS_IMETHODIMP nsSVGRectElement::GetRy(nsIDOMSVGAnimatedLength * *aRy)
{
  return mLengthAttributes[RY].ToDOMAnimatedLength(aRy, this);
}

//----------------------------------------------------------------------
// nsSVGElement methods

nsSVGElement::LengthAttributesInfo
nsSVGRectElement::GetLengthInfo()
{
  return LengthAttributesInfo(mLengthAttributes, sLengthInfo,
                              ArrayLength(sLengthInfo));
}

//----------------------------------------------------------------------
// nsSVGPathGeometryElement methods

void
nsSVGRectElement::ConstructPath(gfxContext *aCtx)
{
  float x, y, width, height, rx, ry;

  GetAnimatedLengthValues(&x, &y, &width, &height, &rx, &ry, nsnull);

  /* In a perfect world, this would be handled by the DOM, and
     return a DOM exception. */
  if (width <= 0 || height <= 0)
    return;

  rx = NS_MAX(rx, 0.0f);
  ry = NS_MAX(ry, 0.0f);

  /* optimize the no rounded corners case */
  if (rx == 0 && ry == 0) {
    aCtx->Rectangle(gfxRect(x, y, width, height));
    return;
  }

  /* If either the 'rx' or the 'ry' attribute isn't set, then we
     have to set it to the value of the other. */
  bool hasRx = mLengthAttributes[RX].IsExplicitlySet();
  bool hasRy = mLengthAttributes[RY].IsExplicitlySet();
  if (hasRx && !hasRy)
    ry = rx;
  else if (hasRy && !hasRx)
    rx = ry;

  /* Clamp rx and ry to half the rect's width and height respectively. */
  float halfWidth  = width/2;
  float halfHeight = height/2;
  if (rx > halfWidth)
    rx = halfWidth;
  if (ry > halfHeight)
    ry = halfHeight;

  gfxSize corner(rx, ry);
  aCtx->RoundedRectangle(gfxRect(x, y, width, height),
                         gfxCornerSizes(corner, corner, corner, corner));
}
