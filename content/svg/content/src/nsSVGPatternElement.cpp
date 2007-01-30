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
#include "nsSVGEnum.h"
#include "nsSVGAnimatedEnumeration.h"
#include "nsIDOMSVGAnimatedEnum.h"
#include "nsSVGAnimatedString.h"
#include "nsCOMPtr.h"
#include "nsISVGSVGElement.h"
#include "nsGkAtoms.h"
#include "nsSVGAnimatedRect.h"
#include "nsSVGRect.h"
#include "nsSVGMatrix.h"
#include "nsSVGAnimatedPreserveAspectRatio.h"
#include "nsSVGPreserveAspectRatio.h"
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

NS_IMPL_NS_NEW_SVG_ELEMENT(Pattern)

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGPatternElement,nsSVGPatternElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGPatternElement,nsSVGPatternElementBase)

NS_INTERFACE_MAP_BEGIN(nsSVGPatternElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNode)
  NS_INTERFACE_MAP_ENTRY(nsIDOMElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGFitToViewBox)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGURIReference)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGPatternElement)
  NS_INTERFACE_MAP_ENTRY(nsIMutationObserver)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(SVGPatternElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGPatternElementBase)

//----------------------------------------------------------------------
// Implementation

nsSVGPatternElement::nsSVGPatternElement(nsINodeInfo* aNodeInfo)
  : nsSVGPatternElementBase(aNodeInfo)
{
  AddMutationObserver(this);
}

nsresult
nsSVGPatternElement::Init()
{
  nsresult rv = nsSVGPatternElementBase::Init();
  NS_ENSURE_SUCCESS(rv,rv);

  // Define enumeration mappings
  static struct nsSVGEnumMapping pUnitMap[] = {
        {&nsGkAtoms::objectBoundingBox, nsIDOMSVGPatternElement::SVG_PUNITS_OBJECTBOUNDINGBOX},
        {&nsGkAtoms::userSpaceOnUse, nsIDOMSVGPatternElement::SVG_PUNITS_USERSPACEONUSE},
        {nsnull, 0}
  };

  // Create mapped attributes

  // DOM property: patternUnits ,  #IMPLIED attrib: patternUnits
  {
    nsCOMPtr<nsISVGEnum> units;
    rv = NS_NewSVGEnum(getter_AddRefs(units),
                       nsIDOMSVGPatternElement::SVG_PUNITS_OBJECTBOUNDINGBOX, pUnitMap);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = NS_NewSVGAnimatedEnumeration(getter_AddRefs(mPatternUnits), units);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsGkAtoms::patternUnits, mPatternUnits);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  // DOM property: patternContentUnits ,  #IMPLIED attrib: patternContentUnits
  {
    nsCOMPtr<nsISVGEnum> units;
    rv = NS_NewSVGEnum(getter_AddRefs(units),
                       nsIDOMSVGPatternElement::SVG_PUNITS_USERSPACEONUSE, pUnitMap);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = NS_NewSVGAnimatedEnumeration(getter_AddRefs(mPatternContentUnits), units);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsGkAtoms::patternContentUnits, mPatternContentUnits);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  // DOM property: patternTransform ,  #IMPLIED attrib: patternTransform
  {
    nsCOMPtr<nsIDOMSVGTransformList> transformList;
    rv = nsSVGTransformList::Create(getter_AddRefs(transformList));
    NS_ENSURE_SUCCESS(rv,rv);
    rv = NS_NewSVGAnimatedTransformList(getter_AddRefs(mPatternTransform),
                                        transformList);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsGkAtoms::patternTransform, mPatternTransform);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  // nsIDOMSVGURIReference properties

  // DOM property: href , #IMPLIED attrib: xlink:href
  {
    rv = NS_NewSVGAnimatedString(getter_AddRefs(mHref));
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsGkAtoms::href, mHref, kNameSpaceID_XLink);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  // nsIDOMSVGFitToViewBox properties

  // DOM property: viewBox
  {
    nsCOMPtr<nsIDOMSVGRect> viewbox;
    rv = NS_NewSVGRect(getter_AddRefs(viewbox));
    NS_ENSURE_SUCCESS(rv,rv);
    rv = NS_NewSVGAnimatedRect(getter_AddRefs(mViewBox), viewbox);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsGkAtoms::viewBox, mViewBox);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  // DOM property: preserveAspectRatio
  {
    nsCOMPtr<nsIDOMSVGPreserveAspectRatio> preserveAspectRatio;
    rv = NS_NewSVGPreserveAspectRatio(getter_AddRefs(preserveAspectRatio));
    NS_ENSURE_SUCCESS(rv,rv);
    rv = NS_NewSVGAnimatedPreserveAspectRatio(
                                          getter_AddRefs(mPreserveAspectRatio),
                                          preserveAspectRatio);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsGkAtoms::preserveAspectRatio,
                           mPreserveAspectRatio);
    NS_ENSURE_SUCCESS(rv,rv);
  }


  return NS_OK;
}

//----------------------------------------------------------------------
// nsIDOMNode method

NS_IMPL_ELEMENT_CLONE_WITH_INIT(nsSVGPatternElement)

//----------------------------------------------------------------------
// nsIDOMSVGFitToViewBox methods

/* readonly attribute nsIDOMSVGAnimatedRect viewBox; */
NS_IMETHODIMP nsSVGPatternElement::GetViewBox(nsIDOMSVGAnimatedRect * *aViewBox)
{
  *aViewBox = mViewBox;
  NS_ADDREF(*aViewBox);
  return NS_OK;
}

/* readonly attribute nsIDOMSVGAnimatedPreserveAspectRatio preserveAspectRatio; */
NS_IMETHODIMP
nsSVGPatternElement::GetPreserveAspectRatio(nsIDOMSVGAnimatedPreserveAspectRatio * *aPreserveAspectRatio)
{
  *aPreserveAspectRatio = mPreserveAspectRatio;
  NS_ADDREF(*aPreserveAspectRatio);
  return NS_OK;
}

//----------------------------------------------------------------------
// nsIDOMSVGPatternElement methods

/* readonly attribute nsIDOMSVGAnimatedEnumeration patternUnits; */
NS_IMETHODIMP nsSVGPatternElement::GetPatternUnits(nsIDOMSVGAnimatedEnumeration * *aPatternUnits)
{
  *aPatternUnits = mPatternUnits;
  NS_IF_ADDREF(*aPatternUnits);
  return NS_OK;
}

/* readonly attribute nsIDOMSVGAnimatedEnumeration patternContentUnits; */
NS_IMETHODIMP nsSVGPatternElement::GetPatternContentUnits(nsIDOMSVGAnimatedEnumeration * *aPatternUnits)
{
  *aPatternUnits = mPatternContentUnits;
  NS_IF_ADDREF(*aPatternUnits);
  return NS_OK;
}

/* readonly attribute nsIDOMSVGAnimatedTransformList patternTransform; */
NS_IMETHODIMP nsSVGPatternElement::GetPatternTransform(nsIDOMSVGAnimatedTransformList * *aPatternTransform)
{
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
  *aHref = mHref;
  NS_IF_ADDREF(*aHref);
  return NS_OK;
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
    sMarkersMap,
    sTextContentElementsMap,
    sViewportsMap
  };

  return FindAttributeDependence(name, map, NS_ARRAY_LENGTH(map)) ||
    nsSVGPatternElementBase::IsAttributeMapped(name);
}

//----------------------------------------------------------------------
// nsSVGElement methods

nsSVGElement::LengthAttributesInfo
nsSVGPatternElement::GetLengthInfo()
{
  return LengthAttributesInfo(mLengthAttributes, sLengthInfo,
                              NS_ARRAY_LENGTH(sLengthInfo));
}

//----------------------------------------------------------------------
// nsIMutationObserver methods

void
nsSVGPatternElement::PushUpdate()
{
  nsIFrame *frame = GetPrimaryFrame();

  if (frame) {
    nsISVGValue *value = nsnull;
    CallQueryInterface(frame, &value);
    if (value) {
      value->BeginBatchUpdate();
      value->EndBatchUpdate();
    }
  }
}

void
nsSVGPatternElement::CharacterDataChanged(nsIDocument *aDocument,
                                          nsIContent *aContent,
                                          CharacterDataChangeInfo *aInfo)
{
  PushUpdate();
}

void
nsSVGPatternElement::AttributeChanged(nsIDocument *aDocument,
                                      nsIContent *aContent,
                                      PRInt32 aNameSpaceID,
                                      nsIAtom *aAttribute,
                                      PRInt32 aModType)
{
  PushUpdate();
}

void
nsSVGPatternElement::ContentAppended(nsIDocument *aDocument,
                                     nsIContent *aContainer,
                                     PRInt32 aNewIndexInContainer)
{
  PushUpdate();
}

void
nsSVGPatternElement::ContentInserted(nsIDocument *aDocument,
                                     nsIContent *aContainer,
                                     nsIContent *aChild,
                                     PRInt32 aIndexInContainer)
{
  PushUpdate();
}

void
nsSVGPatternElement::ContentRemoved(nsIDocument *aDocument,
                                    nsIContent *aContainer,
                                    nsIContent *aChild,
                                    PRInt32 aIndexInContainer)
{
  PushUpdate();
}

void
nsSVGPatternElement::NodeWillBeDestroyed(const nsINode *aNode)
{
}
