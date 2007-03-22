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
#include "nsIDOMSVGLineElement.h"
#include "nsSVGLength2.h"
#include "nsGkAtoms.h"

typedef nsSVGPathGeometryElement nsSVGLineElementBase;

class nsSVGLineElement : public nsSVGLineElementBase,
                         public nsIDOMSVGLineElement
{
protected:
  friend nsresult NS_NewSVGLineElement(nsIContent **aResult,
                                         nsINodeInfo *aNodeInfo);
  nsSVGLineElement(nsINodeInfo *aNodeInfo);

public:
  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMSVGLINEELEMENT

  // xxx I wish we could use virtual inheritance
  NS_FORWARD_NSIDOMNODE(nsSVGLineElementBase::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGLineElementBase::)
  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGLineElementBase::)

  // nsIContent interface
  NS_IMETHODIMP_(PRBool) IsAttributeMapped(const nsIAtom* name) const;

  // nsSVGPathGeometryElement methods:
  virtual PRBool IsMarkable() { return PR_TRUE; }
  virtual void GetMarkPoints(nsTArray<nsSVGMark> *aMarks);
  virtual void ConstructPath(cairo_t *aCtx);

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

protected:

  virtual LengthAttributesInfo GetLengthInfo();

  enum { X1, Y1, X2, Y2 };
  nsSVGLength2 mLengthAttributes[4];
  static LengthInfo sLengthInfo[4];
};

nsSVGElement::LengthInfo nsSVGLineElement::sLengthInfo[4] =
{
  { &nsGkAtoms::x1, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER, nsSVGUtils::X },
  { &nsGkAtoms::y1, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER, nsSVGUtils::Y },
  { &nsGkAtoms::x2, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER, nsSVGUtils::X },
  { &nsGkAtoms::y2, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER, nsSVGUtils::Y },
};

NS_IMPL_NS_NEW_SVG_ELEMENT(Line)

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGLineElement,nsSVGLineElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGLineElement,nsSVGLineElementBase)

NS_INTERFACE_MAP_BEGIN(nsSVGLineElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNode)
  NS_INTERFACE_MAP_ENTRY(nsIDOMElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGLineElement)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(SVGLineElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGLineElementBase)

//----------------------------------------------------------------------
// Implementation

nsSVGLineElement::nsSVGLineElement(nsINodeInfo *aNodeInfo)
  : nsSVGLineElementBase(aNodeInfo)
{
}

//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(nsSVGLineElement)

//----------------------------------------------------------------------
// nsIDOMSVGLineElement methods

/* readonly attribute nsIDOMSVGAnimatedLength cx; */
NS_IMETHODIMP nsSVGLineElement::GetX1(nsIDOMSVGAnimatedLength * *aX1)
{
  return mLengthAttributes[X1].ToDOMAnimatedLength(aX1, this);
}

/* readonly attribute nsIDOMSVGAnimatedLength cy; */
NS_IMETHODIMP nsSVGLineElement::GetY1(nsIDOMSVGAnimatedLength * *aY1)
{
  return mLengthAttributes[Y1].ToDOMAnimatedLength(aY1, this);
}

/* readonly attribute nsIDOMSVGAnimatedLength rx; */
NS_IMETHODIMP nsSVGLineElement::GetX2(nsIDOMSVGAnimatedLength * *aX2)
{
  return mLengthAttributes[X2].ToDOMAnimatedLength(aX2, this);
}

/* readonly attribute nsIDOMSVGAnimatedLength ry; */
NS_IMETHODIMP nsSVGLineElement::GetY2(nsIDOMSVGAnimatedLength * *aY2)
{
  return mLengthAttributes[Y2].ToDOMAnimatedLength(aY2, this);
}

//----------------------------------------------------------------------
// nsIContent methods

NS_IMETHODIMP_(PRBool)
nsSVGLineElement::IsAttributeMapped(const nsIAtom* name) const
{
  static const MappedAttributeEntry* const map[] = {
    sMarkersMap
  };
  
  return FindAttributeDependence(name, map, NS_ARRAY_LENGTH(map)) ||
    nsSVGLineElementBase::IsAttributeMapped(name);
}

//----------------------------------------------------------------------
// nsSVGElement methods

nsSVGElement::LengthAttributesInfo
nsSVGLineElement::GetLengthInfo()
{
  return LengthAttributesInfo(mLengthAttributes, sLengthInfo,
                              NS_ARRAY_LENGTH(sLengthInfo));
}

//----------------------------------------------------------------------
// nsSVGPathGeometryElement methods

void
nsSVGLineElement::GetMarkPoints(nsTArray<nsSVGMark> *aMarks) {
  float x1, y1, x2, y2;

  GetAnimatedLengthValues(&x1, &y1, &x2, &y2, nsnull);

  float angle = atan2(y2 - y1, x2 - x1);

  aMarks->AppendElement(nsSVGMark(x1, y1, angle));
  aMarks->AppendElement(nsSVGMark(x2, y2, angle));
}

void
nsSVGLineElement::ConstructPath(cairo_t *aCtx)
{
  float x1, y1, x2, y2;

  GetAnimatedLengthValues(&x1, &y1, &x2, &y2, nsnull);

  cairo_move_to(aCtx, x1, y1);
  cairo_line_to(aCtx, x2, y2);
}
