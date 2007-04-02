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

#include "nsSVGStylableElement.h"
#include "nsGkAtoms.h"
#include "nsIDOMSVGTextPathElement.h"
#include "nsIDOMSVGURIReference.h"
#include "nsISVGTextContentMetrics.h"
#include "nsIFrame.h"
#include "nsSVGAnimatedString.h"
#include "nsSVGAnimatedEnumeration.h"
#include "nsSVGEnum.h"
#include "nsDOMError.h"
#include "nsSVGLength2.h"

typedef nsSVGStylableElement nsSVGTextPathElementBase;

class nsSVGTextPathElement : public nsSVGTextPathElementBase,
                             public nsIDOMSVGTextPathElement,
                             public nsIDOMSVGURIReference
{
protected:
  friend nsresult NS_NewSVGTextPathElement(nsIContent **aResult,
                                        nsINodeInfo *aNodeInfo);
  nsSVGTextPathElement(nsINodeInfo* aNodeInfo);
  nsresult Init();
  
public:
  // interfaces:
  
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMSVGTEXTPATHELEMENT
  NS_DECL_NSIDOMSVGTEXTCONTENTELEMENT
  NS_DECL_NSIDOMSVGURIREFERENCE

  // xxx If xpcom allowed virtual inheritance we wouldn't need to
  // forward here :-(
  NS_FORWARD_NSIDOMNODE(nsSVGTextPathElementBase::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGTextPathElementBase::)
  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGTextPathElementBase::)

  // nsIContent interface
  NS_IMETHOD_(PRBool) IsAttributeMapped(const nsIAtom* aAttribute) const;

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

protected:

  virtual LengthAttributesInfo GetLengthInfo();

  virtual PRBool IsEventName(nsIAtom* aName);

  already_AddRefed<nsISVGTextContentMetrics> GetTextContentMetrics();

  enum { STARTOFFSET };
  nsSVGLength2 mLengthAttributes[1];
  static LengthInfo sLengthInfo[1];

  nsCOMPtr<nsIDOMSVGAnimatedEnumeration> mMethod;
  nsCOMPtr<nsIDOMSVGAnimatedEnumeration> mSpacing;
  nsCOMPtr<nsIDOMSVGAnimatedString> mHref;
};

nsSVGElement::LengthInfo nsSVGTextPathElement::sLengthInfo[1] =
{
  { &nsGkAtoms::startOffset, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER, nsSVGUtils::X },
};

NS_IMPL_NS_NEW_SVG_ELEMENT(TextPath)

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGTextPathElement,nsSVGTextPathElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGTextPathElement,nsSVGTextPathElementBase)

NS_INTERFACE_MAP_BEGIN(nsSVGTextPathElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNode)
  NS_INTERFACE_MAP_ENTRY(nsIDOMElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGTextPathElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGTextContentElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGURIReference)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(SVGTextPathElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGTextPathElementBase)

//----------------------------------------------------------------------
// Implementation

nsSVGTextPathElement::nsSVGTextPathElement(nsINodeInfo *aNodeInfo)
  : nsSVGTextPathElementBase(aNodeInfo)
{
}

nsresult
nsSVGTextPathElement::Init()
{
  nsresult rv = nsSVGTextPathElementBase::Init();
  NS_ENSURE_SUCCESS(rv,rv);

  // enumeration mappings
  static struct nsSVGEnumMapping methodMap[] = {
    {&nsGkAtoms::align, TEXTPATH_METHODTYPE_ALIGN},
    {&nsGkAtoms::stretch, TEXTPATH_METHODTYPE_STRETCH},
    {nsnull, 0}
  };
  
  static struct nsSVGEnumMapping spacingMap[] = {
    {&nsGkAtoms::_auto, TEXTPATH_SPACINGTYPE_AUTO},
    {&nsGkAtoms::exact, TEXTPATH_SPACINGTYPE_EXACT},
    {nsnull, 0}
  };

  // Create mapped properties:

  // DOM property: method, #IMPLIED attrib: method
  {
    nsCOMPtr<nsISVGEnum> units;
    rv = NS_NewSVGEnum(getter_AddRefs(units), TEXTPATH_METHODTYPE_ALIGN,
                       methodMap);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = NS_NewSVGAnimatedEnumeration(getter_AddRefs(mMethod), units);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsGkAtoms::method, mMethod);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  // DOM property: spacing, #IMPLIED attrib: spacing
  {
    nsCOMPtr<nsISVGEnum> units;
    rv = NS_NewSVGEnum(getter_AddRefs(units), TEXTPATH_SPACINGTYPE_EXACT,
                       spacingMap);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = NS_NewSVGAnimatedEnumeration(getter_AddRefs(mSpacing), units);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsGkAtoms::spacing, mSpacing);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  // nsIDOMSVGURIReference properties

  // DOM property: href , #REQUIRED attrib: xlink:href
  // XXX: enforce requiredness
  {
    rv = NS_NewSVGAnimatedString(getter_AddRefs(mHref));
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsGkAtoms::href, mHref, kNameSpaceID_XLink);
    NS_ENSURE_SUCCESS(rv,rv);
  }
  
  return rv;
}

//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(nsSVGTextPathElement)

//----------------------------------------------------------------------
// nsIDOMSVGURIReference methods

/* readonly attribute nsIDOMSVGAnimatedString href; */
NS_IMETHODIMP nsSVGTextPathElement::GetHref(nsIDOMSVGAnimatedString * *aHref)
{
  *aHref = mHref;
  NS_IF_ADDREF(*aHref);
  return NS_OK;
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
  *aMethod = mMethod;
  NS_IF_ADDREF(*aMethod);
  return NS_OK;
}

/* readonly attribute nsIDOMSVGAnimatedEnumeration spacing; */
NS_IMETHODIMP nsSVGTextPathElement::GetSpacing(nsIDOMSVGAnimatedEnumeration * *aSpacing)
{
  *aSpacing = mSpacing;
  NS_IF_ADDREF(*aSpacing);
  return NS_OK;
}

//----------------------------------------------------------------------
// nsIDOMSVGTextContentElement methods

/* readonly attribute nsIDOMSVGAnimatedLength textLength; */
NS_IMETHODIMP nsSVGTextPathElement::GetTextLength(nsIDOMSVGAnimatedLength * *aTextLength)
{
  NS_NOTYETIMPLEMENTED("nsSVGTextPathElement::GetTextLength!");
  return NS_ERROR_UNEXPECTED;
}

/* readonly attribute nsIDOMSVGAnimatedEnumeration lengthAdjust; */
NS_IMETHODIMP nsSVGTextPathElement::GetLengthAdjust(nsIDOMSVGAnimatedEnumeration * *aLengthAdjust)
{
  NS_NOTYETIMPLEMENTED("nsSVGTextPathElement::GetLengthAdjust!");
  return NS_ERROR_UNEXPECTED;
}

/* long getNumberOfChars (); */
NS_IMETHODIMP nsSVGTextPathElement::GetNumberOfChars(PRInt32 *_retval)
{
  nsCOMPtr<nsISVGTextContentMetrics> metrics = GetTextContentMetrics();

  if (metrics)
    return metrics->GetNumberOfChars(_retval);

  *_retval = 0;
  return NS_OK;
}

/* float getComputedTextLength (); */
NS_IMETHODIMP nsSVGTextPathElement::GetComputedTextLength(float *_retval)
{
  nsCOMPtr<nsISVGTextContentMetrics> metrics = GetTextContentMetrics();

  if (metrics)
    return metrics->GetComputedTextLength(_retval);

  *_retval = 0.0;
  return NS_OK;
}

/* float getSubStringLength (in unsigned long charnum, in unsigned long nchars); */
NS_IMETHODIMP nsSVGTextPathElement::GetSubStringLength(PRUint32 charnum, PRUint32 nchars, float *_retval)
{
  nsCOMPtr<nsISVGTextContentMetrics> metrics = GetTextContentMetrics();

  if (metrics)
    return metrics->GetSubStringLength(charnum, nchars, _retval);

  *_retval = 0.0;
  return NS_OK;
}

/* nsIDOMSVGPoint getStartPositionOfChar (in unsigned long charnum); */
NS_IMETHODIMP nsSVGTextPathElement::GetStartPositionOfChar(PRUint32 charnum, nsIDOMSVGPoint **_retval)
{
  *_retval = nsnull;
  nsCOMPtr<nsISVGTextContentMetrics> metrics = GetTextContentMetrics();

  if (!metrics) return NS_ERROR_FAILURE;

  return metrics->GetStartPositionOfChar(charnum, _retval);
}

/* nsIDOMSVGPoint getEndPositionOfChar (in unsigned long charnum); */
NS_IMETHODIMP nsSVGTextPathElement::GetEndPositionOfChar(PRUint32 charnum, nsIDOMSVGPoint **_retval)
{
  *_retval = nsnull;
  nsCOMPtr<nsISVGTextContentMetrics> metrics = GetTextContentMetrics();

  if (!metrics) return NS_ERROR_FAILURE;

  return metrics->GetEndPositionOfChar(charnum, _retval);
}

/* nsIDOMSVGRect getExtentOfChar (in unsigned long charnum); */
NS_IMETHODIMP nsSVGTextPathElement::GetExtentOfChar(PRUint32 charnum, nsIDOMSVGRect **_retval)
{
  *_retval = nsnull;
  nsCOMPtr<nsISVGTextContentMetrics> metrics = GetTextContentMetrics();

  if (!metrics) return NS_ERROR_FAILURE;

  return metrics->GetExtentOfChar(charnum, _retval);
}

/* float getRotationOfChar (in unsigned long charnum); */
NS_IMETHODIMP nsSVGTextPathElement::GetRotationOfChar(PRUint32 charnum, float *_retval)
{
  *_retval = 0.0;

  nsCOMPtr<nsISVGTextContentMetrics> metrics = GetTextContentMetrics();

  if (!metrics) return NS_ERROR_FAILURE;

  return metrics->GetRotationOfChar(charnum, _retval);
}

/* long getCharNumAtPosition (in nsIDOMSVGPoint point); */
NS_IMETHODIMP nsSVGTextPathElement::GetCharNumAtPosition(nsIDOMSVGPoint *point,
                                                         PRInt32 *_retval)
{
  // null check when implementing - this method can be used by scripts!
  if (!point)
    return NS_ERROR_DOM_SVG_WRONG_TYPE_ERR;

  nsCOMPtr<nsISVGTextContentMetrics> metrics = GetTextContentMetrics();

  if (metrics)
    return metrics->GetCharNumAtPosition(point, _retval);

  *_retval = -1;
  return NS_OK;
}

/* void selectSubString (in unsigned long charnum, in unsigned long nchars); */
NS_IMETHODIMP nsSVGTextPathElement::SelectSubString(PRUint32 charnum, PRUint32 nchars)
{
  NS_NOTYETIMPLEMENTED("nsSVGTextPathElement::SelectSubString!");
  return NS_ERROR_UNEXPECTED;
}

//----------------------------------------------------------------------
// nsIContent methods

NS_IMETHODIMP_(PRBool)
nsSVGTextPathElement::IsAttributeMapped(const nsIAtom* name) const
{
  static const MappedAttributeEntry* const map[] = {
    sColorMap,
    sFillStrokeMap,
    sFontSpecificationMap,
    sGraphicsMap,
    sTextContentElementsMap
  };
  
  return FindAttributeDependence(name, map, NS_ARRAY_LENGTH(map)) ||
    nsSVGTextPathElementBase::IsAttributeMapped(name);
}

//----------------------------------------------------------------------
// nsSVGElement overrides

PRBool
nsSVGTextPathElement::IsEventName(nsIAtom* aName)
{
  return nsContentUtils::IsEventAttributeName(aName, EventNameType_SVGGraphic);
}

nsSVGElement::LengthAttributesInfo
nsSVGTextPathElement::GetLengthInfo()
{
  return LengthAttributesInfo(mLengthAttributes, sLengthInfo,
                              NS_ARRAY_LENGTH(sLengthInfo));
}

//----------------------------------------------------------------------
// implementation helpers:

already_AddRefed<nsISVGTextContentMetrics>
nsSVGTextPathElement::GetTextContentMetrics()
{
  nsIFrame* frame = GetPrimaryFrame(Flush_Layout);

  if (!frame) {
    return nsnull;
  }
  
  nsISVGTextContentMetrics* metrics;
  CallQueryInterface(frame, &metrics);
  return metrics;
}
