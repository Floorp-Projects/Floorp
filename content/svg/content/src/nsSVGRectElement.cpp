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

#include "nsSVGPathGeometryElement.h"
#include "nsIDOMSVGRectElement.h"
#include "nsSVGLength2.h"
#include "nsGkAtoms.h"
#include "gfxContext.h"

typedef nsSVGPathGeometryElement nsSVGRectElementBase;

class nsSVGRectElement : public nsSVGRectElementBase,
                         public nsIDOMSVGRectElement
{
protected:
  friend nsresult NS_NewSVGRectElement(nsIContent **aResult,
                                       nsINodeInfo *aNodeInfo);
  nsSVGRectElement(nsINodeInfo* aNodeInfo);

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

NS_INTERFACE_MAP_BEGIN(nsSVGRectElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNode)
  NS_INTERFACE_MAP_ENTRY(nsIDOMElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGRectElement)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(SVGRectElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGRectElementBase)

//----------------------------------------------------------------------
// Implementation

nsSVGRectElement::nsSVGRectElement(nsINodeInfo *aNodeInfo)
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
                              NS_ARRAY_LENGTH(sLengthInfo));
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
  if (width <= 0 || height <= 0 || ry < 0 || rx < 0)
    return;

  /* optimize the no rounded corners case */
  if (rx == 0 && ry == 0) {
    aCtx->Rectangle(gfxRect(x, y, width, height));
    return;
  }

  /* Clamp rx and ry to half the rect's width and height respectively. */
  float halfWidth  = width/2;
  float halfHeight = height/2;
  if (rx > halfWidth)
    rx = halfWidth;
  if (ry > halfHeight)
    ry = halfHeight;

  /* If either the 'rx' or the 'ry' attribute isn't set in the markup, then we
     have to set it to the value of the other. We do this after clamping rx and
     ry since omitting one of the attributes implicitly means they should both
     be the same. */
  PRBool hasRx = HasAttr(kNameSpaceID_None, nsGkAtoms::rx);
  PRBool hasRy = HasAttr(kNameSpaceID_None, nsGkAtoms::ry);
  if (hasRx && !hasRy)
    ry = rx;
  else if (hasRy && !hasRx)
    rx = ry;

  /* However, we may now have made rx > width/2 or else ry > height/2. (If this
     is the case, we know we must be giving rx and ry the same value.) */
  if (rx > halfWidth)
    rx = ry = halfWidth;
  else if (ry > halfHeight)
    rx = ry = halfHeight;

  // Conversion factor used for ellipse to bezier conversion.
  // Gives radial error of 0.0273% in circular case.
  // See comp.graphics.algorithms FAQ 4.04
  const float magic = 4*(sqrt(2.)-1)/3;
  const float magic_x = magic*rx;
  const float magic_y = magic*ry;

  aCtx->MoveTo(gfxPoint(x + rx, y));
  aCtx->LineTo(gfxPoint(x + width - rx, y));

  aCtx->CurveTo(gfxPoint(x + width - rx + magic_x, y),
                gfxPoint(x + width, y + ry - magic_y),
                gfxPoint(x + width, y + ry));

  aCtx->LineTo(gfxPoint(x + width, y + height - ry));

  aCtx->CurveTo(gfxPoint(x + width, y + height - ry + magic_y),
                gfxPoint(x + width - rx + magic_x, y+height),
                gfxPoint(x + width - rx, y + height));

  aCtx->LineTo(gfxPoint(x + rx, y + height));

  aCtx->CurveTo(gfxPoint(x + rx - magic_x, y + height),
                gfxPoint(x, y + height - ry + magic_y),
                gfxPoint(x, y + height - ry));

  aCtx->LineTo(gfxPoint(x, y + ry));

  aCtx->CurveTo(gfxPoint(x, y + ry - magic_y),
                gfxPoint(x + rx - magic_x, y),
                gfxPoint(x + rx, y));

  aCtx->ClosePath();
}
