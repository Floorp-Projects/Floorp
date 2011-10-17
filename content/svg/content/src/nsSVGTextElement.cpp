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
 * Portions created by the Initial Developer are Copyright (C) 2002
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

#include "nsSVGGraphicElement.h"
#include "nsGkAtoms.h"
#include "nsIDOMSVGTextElement.h"
#include "nsCOMPtr.h"
#include "nsSVGSVGElement.h"
#include "nsSVGTextPositioningElement.h"
#include "nsIFrame.h"
#include "nsDOMError.h"
#include "SVGAnimatedLengthList.h"
#include "DOMSVGAnimatedLengthList.h"
#include "SVGLengthList.h"
#include "SVGNumberList.h"
#include "SVGAnimatedNumberList.h"
#include "DOMSVGAnimatedNumberList.h"
#include "DOMSVGPoint.h"

using namespace mozilla;

typedef nsSVGGraphicElement nsSVGTextElementBase;

/**
 * This class does not inherit nsSVGTextPositioningElement - it reimplements it
 * instead.
 *
 * Ideally this class would inherit nsSVGTextPositioningElement in addition to
 * nsSVGGraphicElement, but we don't want two instances of nsSVGStylableElement
 * and all the classes it inherits. Instead we choose to inherit one of the
 * classes (nsSVGGraphicElement) and reimplement the missing pieces from the
 * other (nsSVGTextPositioningElement (and thus nsSVGTextContentElement)). Care
 * must be taken when making changes to the reimplemented pieces to keep
 * nsSVGTextPositioningElement in sync (and vice versa).
 */
class nsSVGTextElement : public nsSVGTextElementBase,
                         public nsIDOMSVGTextElement // nsIDOMSVGTextPositioningElement
{
protected:
  friend nsresult NS_NewSVGTextElement(nsIContent **aResult,
                                       already_AddRefed<nsINodeInfo> aNodeInfo);
  nsSVGTextElement(already_AddRefed<nsINodeInfo> aNodeInfo);
  
public:
  // interfaces:
  
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMSVGTEXTELEMENT
  NS_DECL_NSIDOMSVGTEXTPOSITIONINGELEMENT
  NS_DECL_NSIDOMSVGTEXTCONTENTELEMENT

  // xxx If xpcom allowed virtual inheritance we wouldn't need to
  // forward here :-(
  NS_FORWARD_NSIDOMNODE(nsSVGTextElementBase::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGTextElementBase::)
  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGTextElementBase::)

  // nsIContent interface
  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom* aAttribute) const;

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  virtual nsXPCClassInfo* GetClassInfo();
protected:
  nsSVGTextContainerFrame* GetTextContainerFrame() {
    return do_QueryFrame(GetPrimaryFrame(Flush_Layout));
  }

  virtual LengthListAttributesInfo GetLengthListInfo();
  virtual NumberListAttributesInfo GetNumberListInfo();

  // nsIDOMSVGTextPositioning properties:

  enum { X, Y, DX, DY };
  SVGAnimatedLengthList mLengthListAttributes[4];
  static LengthListInfo sLengthListInfo[4];

  enum { ROTATE };
  SVGAnimatedNumberList mNumberListAttributes[1];
  static NumberListInfo sNumberListInfo[1];
};


NS_IMPL_NS_NEW_SVG_ELEMENT(Text)


//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGTextElement,nsSVGTextElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGTextElement,nsSVGTextElementBase)

DOMCI_NODE_DATA(SVGTextElement, nsSVGTextElement)

NS_INTERFACE_TABLE_HEAD(nsSVGTextElement)
  NS_NODE_INTERFACE_TABLE6(nsSVGTextElement, nsIDOMNode, nsIDOMElement,
                           nsIDOMSVGElement, nsIDOMSVGTextElement,
                           nsIDOMSVGTextPositioningElement,
                           nsIDOMSVGTextContentElement)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGTextElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGTextElementBase)

//----------------------------------------------------------------------
// Implementation

nsSVGTextElement::nsSVGTextElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : nsSVGTextElementBase(aNodeInfo)
{

}
  
//----------------------------------------------------------------------
// nsIDOMNode methods


NS_IMPL_ELEMENT_CLONE_WITH_INIT(nsSVGTextElement)


//----------------------------------------------------------------------
// nsIDOMSVGTextElement methods

// - no methods -


//----------------------------------------------------------------------
// nsIDOMSVGTextPositioningElement methods

/* readonly attribute nsIDOMSVGAnimatedLengthList x; */
NS_IMETHODIMP
nsSVGTextElement::GetX(nsIDOMSVGAnimatedLengthList * *aX)
{
  *aX = DOMSVGAnimatedLengthList::GetDOMWrapper(&mLengthListAttributes[X],
                                                this, X, nsSVGUtils::X).get();
  return NS_OK;
}

/* readonly attribute nsIDOMSVGAnimatedLengthList y; */
NS_IMETHODIMP
nsSVGTextElement::GetY(nsIDOMSVGAnimatedLengthList * *aY)
{
  *aY = DOMSVGAnimatedLengthList::GetDOMWrapper(&mLengthListAttributes[Y],
                                                this, Y, nsSVGUtils::Y).get();
  return NS_OK;
}

/* readonly attribute nsIDOMSVGAnimatedLengthList dx; */
NS_IMETHODIMP
nsSVGTextElement::GetDx(nsIDOMSVGAnimatedLengthList * *aDx)
{
  *aDx = DOMSVGAnimatedLengthList::GetDOMWrapper(&mLengthListAttributes[DX],
                                                 this, DX, nsSVGUtils::X).get();
  return NS_OK;
}

/* readonly attribute nsIDOMSVGAnimatedLengthList dy; */
NS_IMETHODIMP
nsSVGTextElement::GetDy(nsIDOMSVGAnimatedLengthList * *aDy)
{
  *aDy = DOMSVGAnimatedLengthList::GetDOMWrapper(&mLengthListAttributes[DY],
                                                 this, DY, nsSVGUtils::Y).get();
  return NS_OK;
}

/* readonly attribute nsIDOMSVGAnimatedNumberList rotate; */
NS_IMETHODIMP
nsSVGTextElement::GetRotate(nsIDOMSVGAnimatedNumberList * *aRotate)
{
  *aRotate = DOMSVGAnimatedNumberList::GetDOMWrapper(&mNumberListAttributes[ROTATE],
                                                     this, ROTATE).get();
  return NS_OK;
}


//----------------------------------------------------------------------
// nsIDOMSVGTextContentElement methods

/* readonly attribute nsIDOMSVGAnimatedLength textLength; */
NS_IMETHODIMP
nsSVGTextElement::GetTextLength(nsIDOMSVGAnimatedLength * *aTextLength)
{
  NS_NOTYETIMPLEMENTED("nsSVGTextElement::GetTextLength");
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute nsIDOMSVGAnimatedEnumeration lengthAdjust; */
NS_IMETHODIMP
nsSVGTextElement::GetLengthAdjust(nsIDOMSVGAnimatedEnumeration * *aLengthAdjust)
{
  NS_NOTYETIMPLEMENTED("nsSVGTextElement::GetLengthAdjust");
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* long getNumberOfChars (); */
NS_IMETHODIMP
nsSVGTextElement::GetNumberOfChars(PRInt32 *_retval)
{
  *_retval = 0;

  nsSVGTextContainerFrame* metrics = GetTextContainerFrame();
  if (metrics)
    *_retval = metrics->GetNumberOfChars();

  return NS_OK;
}

/* float getComputedTextLength (); */
NS_IMETHODIMP
nsSVGTextElement::GetComputedTextLength(float *_retval)
{
  *_retval = 0.0;

  nsSVGTextContainerFrame* metrics = GetTextContainerFrame();
  if (metrics)
    *_retval = metrics->GetComputedTextLength();

  return NS_OK;
}

/* float getSubStringLength (in unsigned long charnum, in unsigned long nchars); */
NS_IMETHODIMP
nsSVGTextElement::GetSubStringLength(PRUint32 charnum, PRUint32 nchars, float *_retval)
{
  *_retval = 0.0f;
  nsSVGTextContainerFrame* metrics = GetTextContainerFrame();
  if (!metrics)
    return NS_OK;

  PRUint32 charcount = metrics->GetNumberOfChars();
  if (charcount <= charnum || nchars > charcount - charnum)
    return NS_ERROR_DOM_INDEX_SIZE_ERR;

  if (nchars == 0)
    return NS_OK;

  *_retval = metrics->GetSubStringLength(charnum, nchars);
  return NS_OK;
}

/* nsIDOMSVGPoint getStartPositionOfChar (in unsigned long charnum); */
NS_IMETHODIMP
nsSVGTextElement::GetStartPositionOfChar(PRUint32 charnum, nsIDOMSVGPoint **_retval)
{
  *_retval = nsnull;
  nsSVGTextContainerFrame* metrics = GetTextContainerFrame();

  if (!metrics) return NS_ERROR_FAILURE;

  return metrics->GetStartPositionOfChar(charnum, _retval);
}

/* nsIDOMSVGPoint getEndPositionOfChar (in unsigned long charnum); */
NS_IMETHODIMP
nsSVGTextElement::GetEndPositionOfChar(PRUint32 charnum, nsIDOMSVGPoint **_retval)
{
  *_retval = nsnull;
  nsSVGTextContainerFrame* metrics = GetTextContainerFrame();

  if (!metrics) return NS_ERROR_FAILURE;

  return metrics->GetEndPositionOfChar(charnum, _retval);
}

/* nsIDOMSVGRect getExtentOfChar (in unsigned long charnum); */
NS_IMETHODIMP
nsSVGTextElement::GetExtentOfChar(PRUint32 charnum, nsIDOMSVGRect **_retval)
{
  *_retval = nsnull;
  nsSVGTextContainerFrame* metrics = GetTextContainerFrame();

  if (!metrics) return NS_ERROR_FAILURE;

  return metrics->GetExtentOfChar(charnum, _retval);
}

/* float getRotationOfChar (in unsigned long charnum); */
NS_IMETHODIMP
nsSVGTextElement::GetRotationOfChar(PRUint32 charnum, float *_retval)
{
  *_retval = 0.0;

  nsSVGTextContainerFrame* metrics = GetTextContainerFrame();

  if (!metrics) return NS_ERROR_FAILURE;

  return metrics->GetRotationOfChar(charnum, _retval);
}

/* long getCharNumAtPosition (in nsIDOMSVGPoint point); */
NS_IMETHODIMP
nsSVGTextElement::GetCharNumAtPosition(nsIDOMSVGPoint *point, PRInt32 *_retval)
{
  nsCOMPtr<DOMSVGPoint> p = do_QueryInterface(point);
  if (!p)
    return NS_ERROR_DOM_SVG_WRONG_TYPE_ERR;

  *_retval = -1;

  nsSVGTextContainerFrame* metrics = GetTextContainerFrame();
  if (metrics)
    *_retval = metrics->GetCharNumAtPosition(point);

  return NS_OK;
}

/* void selectSubString (in unsigned long charnum, in unsigned long nchars); */
NS_IMETHODIMP
nsSVGTextElement::SelectSubString(PRUint32 charnum, PRUint32 nchars)
{
  NS_NOTYETIMPLEMENTED("nsSVGTextElement::SelectSubString");
  return NS_ERROR_NOT_IMPLEMENTED;
}


//----------------------------------------------------------------------
// nsIContent methods

NS_IMETHODIMP_(bool)
nsSVGTextElement::IsAttributeMapped(const nsIAtom* name) const
{
  static const MappedAttributeEntry* const map[] = {
    sTextContentElementsMap,
    sFontSpecificationMap
  };

  return FindAttributeDependence(name, map, ArrayLength(map)) ||
    nsSVGTextElementBase::IsAttributeMapped(name);
}

//----------------------------------------------------------------------
// nsSVGElement methods

nsSVGElement::LengthListInfo nsSVGTextElement::sLengthListInfo[4] =
{
  { &nsGkAtoms::x,  nsSVGUtils::X, PR_FALSE },
  { &nsGkAtoms::y,  nsSVGUtils::Y, PR_FALSE },
  { &nsGkAtoms::dx, nsSVGUtils::X, PR_TRUE },
  { &nsGkAtoms::dy, nsSVGUtils::Y, PR_TRUE }
};

nsSVGElement::LengthListAttributesInfo
nsSVGTextElement::GetLengthListInfo()
{
  return LengthListAttributesInfo(mLengthListAttributes, sLengthListInfo,
                                  ArrayLength(sLengthListInfo));
}

nsSVGElement::NumberListInfo nsSVGTextElement::sNumberListInfo[1] =
{
  { &nsGkAtoms::rotate }
};

nsSVGElement::NumberListAttributesInfo
nsSVGTextElement::GetNumberListInfo()
{
  return NumberListAttributesInfo(mNumberListAttributes, sNumberListInfo,
                                  ArrayLength(sNumberListInfo));
}

