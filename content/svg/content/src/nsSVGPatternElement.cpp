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
 * Scooter Morris.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Scooter Morris <scootermorris@comcast.net>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsSVGTransformList.h"
#include "nsSVGAnimatedTransformList.h"
#include "nsIDOMMutationEvent.h"
#include "nsCOMPtr.h"
#include "nsGkAtoms.h"
#include "nsSVGPatternElement.h"
#include "nsIFrame.h"

//--------------------- Patterns ------------------------

nsSVGElement::LengthInfo nsSVGPatternElement::sLengthInfo[4] =
{
  { &nsGkAtoms::x, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_PERCENTAGE, nsSVGUtils::X },
  { &nsGkAtoms::y, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_PERCENTAGE, nsSVGUtils::Y },
  { &nsGkAtoms::width, 100, nsIDOMSVGLength::SVG_LENGTHTYPE_PERCENTAGE, nsSVGUtils::X },
  { &nsGkAtoms::height, 100, nsIDOMSVGLength::SVG_LENGTHTYPE_PERCENTAGE, nsSVGUtils::Y },
};

nsSVGElement::EnumInfo nsSVGPatternElement::sEnumInfo[2] =
{
  { &nsGkAtoms::patternUnits,
    sSVGUnitTypesMap,
    nsIDOMSVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX
  },
  { &nsGkAtoms::patternContentUnits,
    sSVGUnitTypesMap,
    nsIDOMSVGUnitTypes::SVG_UNIT_TYPE_USERSPACEONUSE
  }
};

nsSVGElement::StringInfo nsSVGPatternElement::sStringInfo[1] =
{
  { &nsGkAtoms::href, kNameSpaceID_XLink, PR_TRUE }
};

NS_IMPL_NS_NEW_SVG_ELEMENT(Pattern)

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGPatternElement,nsSVGPatternElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGPatternElement,nsSVGPatternElementBase)

DOMCI_NODE_DATA(SVGPatternElement, nsSVGPatternElement)

NS_INTERFACE_TABLE_HEAD(nsSVGPatternElement)
  NS_NODE_INTERFACE_TABLE7(nsSVGPatternElement, nsIDOMNode, nsIDOMElement,
                           nsIDOMSVGElement, nsIDOMSVGFitToViewBox,
                           nsIDOMSVGURIReference, nsIDOMSVGPatternElement,
                           nsIDOMSVGUnitTypes)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGPatternElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGPatternElementBase)

//----------------------------------------------------------------------
// Implementation

nsSVGPatternElement::nsSVGPatternElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : nsSVGPatternElementBase(aNodeInfo)
{
}

nsresult
nsSVGPatternElement::CreateTransformList()
{
  nsresult rv;

  // DOM property: transform, #IMPLIED attrib: transform
  nsCOMPtr<nsIDOMSVGTransformList> transformList;
  rv = nsSVGTransformList::Create(getter_AddRefs(transformList));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = NS_NewSVGAnimatedTransformList(getter_AddRefs(mPatternTransform),
                                      transformList);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = AddMappedSVGValue(nsGkAtoms::patternTransform, mPatternTransform);
  if (NS_FAILED(rv)) {
    mPatternTransform = nsnull;
    return rv;
  }

  return NS_OK;
}

nsresult
nsSVGPatternElement::BeforeSetAttr(PRInt32 aNamespaceID, nsIAtom* aName,
                                   const nsAString* aValue, PRBool aNotify)
{
  if (aNamespaceID == kNameSpaceID_None &&
      aName == nsGkAtoms::patternTransform &&
      !mPatternTransform &&
      NS_FAILED(CreateTransformList()))
    return NS_ERROR_OUT_OF_MEMORY;

  return nsSVGPatternElementBase::BeforeSetAttr(aNamespaceID, aName,
                                                aValue, aNotify);
}

//----------------------------------------------------------------------
// nsIDOMNode method

NS_IMPL_ELEMENT_CLONE_WITH_INIT(nsSVGPatternElement)

//----------------------------------------------------------------------
// nsIDOMSVGFitToViewBox methods

/* readonly attribute nsIDOMSVGAnimatedRect viewBox; */
NS_IMETHODIMP nsSVGPatternElement::GetViewBox(nsIDOMSVGAnimatedRect * *aViewBox)
{
  return mViewBox.ToDOMAnimatedRect(aViewBox, this);
}

/* readonly attribute nsIDOMSVGAnimatedPreserveAspectRatio preserveAspectRatio; */
NS_IMETHODIMP
nsSVGPatternElement::GetPreserveAspectRatio(nsIDOMSVGAnimatedPreserveAspectRatio
                                            **aPreserveAspectRatio)
{
  return mPreserveAspectRatio.ToDOMAnimatedPreserveAspectRatio(aPreserveAspectRatio, this);
}

//----------------------------------------------------------------------
// nsIDOMSVGPatternElement methods

/* readonly attribute nsIDOMSVGAnimatedEnumeration patternUnits; */
NS_IMETHODIMP nsSVGPatternElement::GetPatternUnits(nsIDOMSVGAnimatedEnumeration * *aPatternUnits)
{
  return mEnumAttributes[PATTERNUNITS].ToDOMAnimatedEnum(aPatternUnits, this);
}

/* readonly attribute nsIDOMSVGAnimatedEnumeration patternContentUnits; */
NS_IMETHODIMP nsSVGPatternElement::GetPatternContentUnits(nsIDOMSVGAnimatedEnumeration * *aPatternUnits)
{
  return mEnumAttributes[PATTERNCONTENTUNITS].ToDOMAnimatedEnum(aPatternUnits, this);
}

/* readonly attribute nsIDOMSVGAnimatedTransformList patternTransform; */
NS_IMETHODIMP nsSVGPatternElement::GetPatternTransform(nsIDOMSVGAnimatedTransformList * *aPatternTransform)
{
  if (!mPatternTransform && NS_FAILED(CreateTransformList()))
    return NS_ERROR_OUT_OF_MEMORY;

  *aPatternTransform = mPatternTransform;
  NS_IF_ADDREF(*aPatternTransform);
  return NS_OK;
}

/* readonly attribute nsIDOMSVGAnimatedLength x; */
NS_IMETHODIMP nsSVGPatternElement::GetX(nsIDOMSVGAnimatedLength * *aX)
{
  return mLengthAttributes[X].ToDOMAnimatedLength(aX, this);
}

/* readonly attribute nsIDOMSVGAnimatedLength y; */
NS_IMETHODIMP nsSVGPatternElement::GetY(nsIDOMSVGAnimatedLength * *aY)
{
  return mLengthAttributes[Y].ToDOMAnimatedLength(aY, this);
}

/* readonly attribute nsIDOMSVGAnimatedLength width; */
NS_IMETHODIMP nsSVGPatternElement::GetWidth(nsIDOMSVGAnimatedLength * *aWidth)
{
  return mLengthAttributes[WIDTH].ToDOMAnimatedLength(aWidth, this);
}

/* readonly attribute nsIDOMSVGAnimatedLength height; */
NS_IMETHODIMP nsSVGPatternElement::GetHeight(nsIDOMSVGAnimatedLength * *aHeight)
{
  return mLengthAttributes[HEIGHT].ToDOMAnimatedLength(aHeight, this);
}


//----------------------------------------------------------------------
// nsIDOMSVGURIReference methods:

/* readonly attribute nsIDOMSVGAnimatedString href; */
NS_IMETHODIMP
nsSVGPatternElement::GetHref(nsIDOMSVGAnimatedString * *aHref)
{
  return mStringAttributes[HREF].ToDOMAnimatedString(aHref, this);
}

//----------------------------------------------------------------------
// nsIContent methods

NS_IMETHODIMP_(PRBool)
nsSVGPatternElement::IsAttributeMapped(const nsIAtom* name) const
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

  return FindAttributeDependence(name, map, NS_ARRAY_LENGTH(map)) ||
    nsSVGPatternElementBase::IsAttributeMapped(name);
}

//----------------------------------------------------------------------
// nsSVGElement methods

void
nsSVGPatternElement::DidAnimateTransform()
{
  nsIFrame* frame = GetPrimaryFrame();
  
  if (frame) {
    frame->AttributeChanged(kNameSpaceID_None,
                            nsGkAtoms::patternTransform,
                            nsIDOMMutationEvent::MODIFICATION);
  }
}

nsSVGElement::LengthAttributesInfo
nsSVGPatternElement::GetLengthInfo()
{
  return LengthAttributesInfo(mLengthAttributes, sLengthInfo,
                              NS_ARRAY_LENGTH(sLengthInfo));
}

nsSVGElement::EnumAttributesInfo
nsSVGPatternElement::GetEnumInfo()
{
  return EnumAttributesInfo(mEnumAttributes, sEnumInfo,
                            NS_ARRAY_LENGTH(sEnumInfo));
}

nsSVGViewBox *
nsSVGPatternElement::GetViewBox()
{
  return &mViewBox;
}

nsSVGPreserveAspectRatio *
nsSVGPatternElement::GetPreserveAspectRatio()
{
  return &mPreserveAspectRatio;
}

nsSVGElement::StringAttributesInfo
nsSVGPatternElement::GetStringInfo()
{
  return StringAttributesInfo(mStringAttributes, sStringInfo,
                              NS_ARRAY_LENGTH(sStringInfo));
}

