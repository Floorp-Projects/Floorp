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
 * The Initial Developer of the Original Code is IBM Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2005
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

#include "nsSVGStylableElement.h"
#include "nsGkAtoms.h"
#include "nsIDOMSVGTextPathElement.h"
#include "nsIDOMSVGURIReference.h"
#include "nsIFrame.h"
#include "nsSVGTextPathElement.h"
#include "nsDOMError.h"
#include "nsContentUtils.h"

using namespace mozilla;

nsSVGElement::LengthInfo nsSVGTextPathElement::sLengthInfo[1] =
{
  { &nsGkAtoms::startOffset, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER, nsSVGUtils::X },
};

nsSVGEnumMapping nsSVGTextPathElement::sMethodMap[] = {
  {&nsGkAtoms::align, nsIDOMSVGTextPathElement::TEXTPATH_METHODTYPE_ALIGN},
  {&nsGkAtoms::stretch, nsIDOMSVGTextPathElement::TEXTPATH_METHODTYPE_STRETCH},
  {nsnull, 0}
};

nsSVGEnumMapping nsSVGTextPathElement::sSpacingMap[] = {
  {&nsGkAtoms::_auto, nsIDOMSVGTextPathElement::TEXTPATH_SPACINGTYPE_AUTO},
  {&nsGkAtoms::exact, nsIDOMSVGTextPathElement::TEXTPATH_SPACINGTYPE_EXACT},
  {nsnull, 0}
};

nsSVGElement::EnumInfo nsSVGTextPathElement::sEnumInfo[2] =
{
  { &nsGkAtoms::method,
    sMethodMap,
    nsIDOMSVGTextPathElement::TEXTPATH_METHODTYPE_ALIGN
  },
  { &nsGkAtoms::spacing,
    sSpacingMap,
    nsIDOMSVGTextPathElement::TEXTPATH_SPACINGTYPE_EXACT
  }
};

nsSVGElement::StringInfo nsSVGTextPathElement::sStringInfo[1] =
{
  { &nsGkAtoms::href, kNameSpaceID_XLink, PR_TRUE }
};

NS_IMPL_NS_NEW_SVG_ELEMENT(TextPath)

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGTextPathElement,nsSVGTextPathElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGTextPathElement,nsSVGTextPathElementBase)

DOMCI_NODE_DATA(SVGTextPathElement, nsSVGTextPathElement)

NS_INTERFACE_TABLE_HEAD(nsSVGTextPathElement)
  NS_NODE_INTERFACE_TABLE6(nsSVGTextPathElement, nsIDOMNode, nsIDOMElement,
                           nsIDOMSVGElement, nsIDOMSVGTextPathElement,
                           nsIDOMSVGTextContentElement, nsIDOMSVGURIReference)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGTextPathElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGTextPathElementBase)

//----------------------------------------------------------------------
// Implementation

nsSVGTextPathElement::nsSVGTextPathElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : nsSVGTextPathElementBase(aNodeInfo)
{
}

//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(nsSVGTextPathElement)

//----------------------------------------------------------------------
// nsIDOMSVGURIReference methods

/* readonly attribute nsIDOMSVGAnimatedString href; */
NS_IMETHODIMP nsSVGTextPathElement::GetHref(nsIDOMSVGAnimatedString * *aHref)
{
  return mStringAttributes[HREF].ToDOMAnimatedString(aHref, this);
}

//----------------------------------------------------------------------
// nsIDOMSVGTextPathElement methods

NS_IMETHODIMP nsSVGTextPathElement::GetStartOffset(nsIDOMSVGAnimatedLength * *aStartOffset)
{
  return mLengthAttributes[STARTOFFSET].ToDOMAnimatedLength(aStartOffset, this);
}

/* readonly attribute nsIDOMSVGAnimatedEnumeration method; */
NS_IMETHODIMP nsSVGTextPathElement::GetMethod(nsIDOMSVGAnimatedEnumeration * *aMethod)
{
  return mEnumAttributes[METHOD].ToDOMAnimatedEnum(aMethod, this);
}

/* readonly attribute nsIDOMSVGAnimatedEnumeration spacing; */
NS_IMETHODIMP nsSVGTextPathElement::GetSpacing(nsIDOMSVGAnimatedEnumeration * *aSpacing)
{
  return mEnumAttributes[SPACING].ToDOMAnimatedEnum(aSpacing, this);
}

//----------------------------------------------------------------------
// nsIContent methods

NS_IMETHODIMP_(bool)
nsSVGTextPathElement::IsAttributeMapped(const nsIAtom* name) const
{
  static const MappedAttributeEntry* const map[] = {
    sColorMap,
    sFillStrokeMap,
    sFontSpecificationMap,
    sGraphicsMap,
    sTextContentElementsMap
  };
  
  return FindAttributeDependence(name, map, ArrayLength(map)) ||
    nsSVGTextPathElementBase::IsAttributeMapped(name);
}

//----------------------------------------------------------------------
// nsSVGElement overrides

bool
nsSVGTextPathElement::IsEventName(nsIAtom* aName)
{
  return nsContentUtils::IsEventAttributeName(aName, EventNameType_SVGGraphic);
}

nsSVGElement::LengthAttributesInfo
nsSVGTextPathElement::GetLengthInfo()
{
  return LengthAttributesInfo(mLengthAttributes, sLengthInfo,
                              ArrayLength(sLengthInfo));
}

nsSVGElement::EnumAttributesInfo
nsSVGTextPathElement::GetEnumInfo()
{
  return EnumAttributesInfo(mEnumAttributes, sEnumInfo,
                            ArrayLength(sEnumInfo));
}

nsSVGElement::StringAttributesInfo
nsSVGTextPathElement::GetStringInfo()
{
  return StringAttributesInfo(mStringAttributes, sStringInfo,
                              ArrayLength(sStringInfo));
}
