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
 *   Alex Fritze <alex.fritze@crocodile-clips.com> (original author)
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
#include "nsIDOMSVGCircleElement.h"
#include "nsSVGLength2.h"
#include "nsGkAtoms.h"
#include "nsSVGUtils.h"
#include "gfxContext.h"

typedef nsSVGPathGeometryElement nsSVGCircleElementBase;

class nsSVGCircleElement : public nsSVGCircleElementBase,
                           public nsIDOMSVGCircleElement
{
protected:
  friend nsresult NS_NewSVGCircleElement(nsIContent **aResult,
                                         nsINodeInfo *aNodeInfo);
  nsSVGCircleElement(nsINodeInfo *aNodeInfo);

public:
  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMSVGCIRCLEELEMENT

  // xxx I wish we could use virtual inheritance
  NS_FORWARD_NSIDOMNODE(nsSVGCircleElementBase::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGCircleElementBase::)
  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGCircleElementBase::)

  // nsSVGPathGeometryElement methods:
  virtual void ConstructPath(gfxContext *aCtx);

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

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

NS_INTERFACE_MAP_BEGIN(nsSVGCircleElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNode)
  NS_INTERFACE_MAP_ENTRY(nsIDOMElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGCircleElement)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(SVGCircleElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGCircleElementBase)

//----------------------------------------------------------------------
// Implementation

nsSVGCircleElement::nsSVGCircleElement(nsINodeInfo *aNodeInfo)
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

nsSVGElement::LengthAttributesInfo
nsSVGCircleElement::GetLengthInfo()
{
  return LengthAttributesInfo(mLengthAttributes, sLengthInfo,
                              NS_ARRAY_LENGTH(sLengthInfo));
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
