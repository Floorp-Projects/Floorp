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

#include "mozilla/Util.h"

#include "nsCOMPtr.h"
#include "nsSVGForeignObjectElement.h"

using namespace mozilla;

nsSVGElement::LengthInfo nsSVGForeignObjectElement::sLengthInfo[4] =
{
  { &nsGkAtoms::x, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER, nsSVGUtils::X },
  { &nsGkAtoms::y, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER, nsSVGUtils::Y },
  { &nsGkAtoms::width, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER, nsSVGUtils::X },
  { &nsGkAtoms::height, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER, nsSVGUtils::Y },
};

NS_IMPL_NS_NEW_SVG_ELEMENT(ForeignObject)

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGForeignObjectElement,nsSVGForeignObjectElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGForeignObjectElement,nsSVGForeignObjectElementBase)

DOMCI_NODE_DATA(SVGForeignObjectElement, nsSVGForeignObjectElement)

NS_INTERFACE_TABLE_HEAD(nsSVGForeignObjectElement)
  NS_NODE_INTERFACE_TABLE5(nsSVGForeignObjectElement, nsIDOMNode, nsIDOMElement,
                           nsIDOMSVGElement, nsIDOMSVGTests,
                           nsIDOMSVGForeignObjectElement)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGForeignObjectElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGForeignObjectElementBase)

//----------------------------------------------------------------------
// Implementation

nsSVGForeignObjectElement::nsSVGForeignObjectElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : nsSVGForeignObjectElementBase(aNodeInfo)
{
}

//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(nsSVGForeignObjectElement)

//----------------------------------------------------------------------
// nsIDOMSVGForeignObjectElement methods:

/* readonly attribute nsIDOMSVGAnimatedLength x; */
NS_IMETHODIMP nsSVGForeignObjectElement::GetX(nsIDOMSVGAnimatedLength * *aX)
{
  return mLengthAttributes[X].ToDOMAnimatedLength(aX, this);
}

/* readonly attribute nsIDOMSVGAnimatedLength y; */
NS_IMETHODIMP nsSVGForeignObjectElement::GetY(nsIDOMSVGAnimatedLength * *aY)
{
  return mLengthAttributes[Y].ToDOMAnimatedLength(aY, this);
}

/* readonly attribute nsIDOMSVGAnimatedLength width; */
NS_IMETHODIMP nsSVGForeignObjectElement::GetWidth(nsIDOMSVGAnimatedLength * *aWidth)
{
  return mLengthAttributes[WIDTH].ToDOMAnimatedLength(aWidth, this);
}

/* readonly attribute nsIDOMSVGAnimatedLength height; */
NS_IMETHODIMP nsSVGForeignObjectElement::GetHeight(nsIDOMSVGAnimatedLength * *aHeight)
{
  return mLengthAttributes[HEIGHT].ToDOMAnimatedLength(aHeight, this);
}

//----------------------------------------------------------------------
// nsSVGElement methods

/* virtual */ gfxMatrix
nsSVGForeignObjectElement::PrependLocalTransformsTo(const gfxMatrix &aMatrix,
                                                    TransformTypes aWhich) const
{
  NS_ABORT_IF_FALSE(aWhich != eChildToUserSpace || aMatrix.IsIdentity(),
                    "Skipping eUserSpaceToParent transforms makes no sense");

  // 'transform' attribute:
  gfxMatrix fromUserSpace =
    nsSVGForeignObjectElementBase::PrependLocalTransformsTo(aMatrix, aWhich);
  if (aWhich == eUserSpaceToParent) {
    return fromUserSpace;
  }
  // our 'x' and 'y' attributes:
  float x, y;
  const_cast<nsSVGForeignObjectElement*>(this)->
    GetAnimatedLengthValues(&x, &y, nsnull);
  gfxMatrix toUserSpace = gfxMatrix().Translate(gfxPoint(x, y));
  if (aWhich == eChildToUserSpace) {
    return toUserSpace;
  }
  NS_ABORT_IF_FALSE(aWhich == eAllTransforms, "Unknown TransformTypes");
  return toUserSpace * fromUserSpace;
}

/* virtual */ bool
nsSVGForeignObjectElement::HasValidDimensions() const
{
  return mLengthAttributes[WIDTH].IsExplicitlySet() &&
         mLengthAttributes[WIDTH].GetAnimValInSpecifiedUnits() > 0 &&
         mLengthAttributes[HEIGHT].IsExplicitlySet() &&
         mLengthAttributes[HEIGHT].GetAnimValInSpecifiedUnits() > 0;
}

//----------------------------------------------------------------------
// nsIContent methods

NS_IMETHODIMP_(bool)
nsSVGForeignObjectElement::IsAttributeMapped(const nsIAtom* name) const
{
  static const MappedAttributeEntry* const map[] = {
    sFEFloodMap,
    sFiltersMap,
    sFontSpecificationMap,
    sGradientStopMap,
    sLightingEffectsMap,
    sMarkersMap,
    sTextContentElementsMap,
    sViewportsMap
  };

  return FindAttributeDependence(name, map) ||
    nsSVGForeignObjectElementBase::IsAttributeMapped(name);
}

//----------------------------------------------------------------------
// nsSVGElement methods

nsSVGElement::LengthAttributesInfo
nsSVGForeignObjectElement::GetLengthInfo()
{
  return LengthAttributesInfo(mLengthAttributes, sLengthInfo,
                              ArrayLength(sLengthInfo));
}
