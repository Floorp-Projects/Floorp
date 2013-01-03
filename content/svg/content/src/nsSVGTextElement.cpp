/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Util.h"

#include "nsSVGGraphicElement.h"
#include "nsGkAtoms.h"
#include "nsIDOMSVGTextElement.h"
#include "nsCOMPtr.h"
#include "nsSVGSVGElement.h"
#include "nsSVGTextPositioningElement.h"
#include "nsError.h"
#include "SVGAnimatedLengthList.h"
#include "DOMSVGAnimatedLengthList.h"
#include "SVGLengthList.h"
#include "SVGNumberList.h"
#include "SVGAnimatedNumberList.h"
#include "DOMSVGAnimatedNumberList.h"
#include "DOMSVGPoint.h"
#include "DOMSVGTests.h"

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
                         public nsIDOMSVGTextElement, // nsIDOMSVGTextPositioningElement
                         public DOMSVGTests
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
  NS_FORWARD_NSIDOMNODE_TO_NSINODE
  NS_FORWARD_NSIDOMELEMENT_TO_GENERIC
  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGTextElementBase::)

  // nsIContent interface
  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom* aAttribute) const;

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  virtual nsXPCClassInfo* GetClassInfo();

  virtual nsIDOMNode* AsDOMNode() { return this; }
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
  NS_NODE_INTERFACE_TABLE7(nsSVGTextElement, nsIDOMNode, nsIDOMElement,
                           nsIDOMSVGElement, nsIDOMSVGTextElement,
                           nsIDOMSVGTextPositioningElement,
                           nsIDOMSVGTextContentElement,
                           nsIDOMSVGTests)
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

/* readonly attribute DOMSVGAnimatedLengthList x; */
NS_IMETHODIMP
nsSVGTextElement::GetX(nsISupports * *aX)
{
  *aX = DOMSVGAnimatedLengthList::GetDOMWrapper(&mLengthListAttributes[X],
                                                this, X, SVGContentUtils::X).get();
  return NS_OK;
}

/* readonly attribute DOMSVGAnimatedLengthList y; */
NS_IMETHODIMP
nsSVGTextElement::GetY(nsISupports * *aY)
{
  *aY = DOMSVGAnimatedLengthList::GetDOMWrapper(&mLengthListAttributes[Y],
                                                this, Y, SVGContentUtils::Y).get();
  return NS_OK;
}

/* readonly attribute DOMSVGAnimatedLengthList dx; */
NS_IMETHODIMP
nsSVGTextElement::GetDx(nsISupports * *aDx)
{
  *aDx = DOMSVGAnimatedLengthList::GetDOMWrapper(&mLengthListAttributes[DX],
                                                 this, DX, SVGContentUtils::X).get();
  return NS_OK;
}

/* readonly attribute DOMSVGAnimatedLengthList dy; */
NS_IMETHODIMP
nsSVGTextElement::GetDy(nsISupports * *aDy)
{
  *aDy = DOMSVGAnimatedLengthList::GetDOMWrapper(&mLengthListAttributes[DY],
                                                 this, DY, SVGContentUtils::Y).get();
  return NS_OK;
}

/* readonly attribute DOMSVGAnimatedNumberList rotate; */
NS_IMETHODIMP
nsSVGTextElement::GetRotate(nsISupports * *aRotate)
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
nsSVGTextElement::GetNumberOfChars(int32_t *_retval)
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
nsSVGTextElement::GetSubStringLength(uint32_t charnum, uint32_t nchars, float *_retval)
{
  *_retval = 0.0f;
  nsSVGTextContainerFrame* metrics = GetTextContainerFrame();
  if (!metrics)
    return NS_OK;

  uint32_t charcount = metrics->GetNumberOfChars();
  if (charcount <= charnum || nchars > charcount - charnum)
    return NS_ERROR_DOM_INDEX_SIZE_ERR;

  if (nchars == 0)
    return NS_OK;

  *_retval = metrics->GetSubStringLength(charnum, nchars);
  return NS_OK;
}

/* DOMSVGPoint getStartPositionOfChar (in unsigned long charnum); */
NS_IMETHODIMP
nsSVGTextElement::GetStartPositionOfChar(uint32_t charnum, nsISupports **_retval)
{
  *_retval = nullptr;
  nsSVGTextContainerFrame* metrics = GetTextContainerFrame();

  if (!metrics) return NS_ERROR_FAILURE;

  return metrics->GetStartPositionOfChar(charnum, _retval);
}

/* DOMSVGPoint getEndPositionOfChar (in unsigned long charnum); */
NS_IMETHODIMP
nsSVGTextElement::GetEndPositionOfChar(uint32_t charnum, nsISupports **_retval)
{
  *_retval = nullptr;
  nsSVGTextContainerFrame* metrics = GetTextContainerFrame();

  if (!metrics) return NS_ERROR_FAILURE;

  return metrics->GetEndPositionOfChar(charnum, _retval);
}

/* nsIDOMSVGRect getExtentOfChar (in unsigned long charnum); */
NS_IMETHODIMP
nsSVGTextElement::GetExtentOfChar(uint32_t charnum, nsIDOMSVGRect **_retval)
{
  *_retval = nullptr;
  nsSVGTextContainerFrame* metrics = GetTextContainerFrame();

  if (!metrics) return NS_ERROR_FAILURE;

  return metrics->GetExtentOfChar(charnum, _retval);
}

/* float getRotationOfChar (in unsigned long charnum); */
NS_IMETHODIMP
nsSVGTextElement::GetRotationOfChar(uint32_t charnum, float *_retval)
{
  *_retval = 0.0;

  nsSVGTextContainerFrame* metrics = GetTextContainerFrame();

  if (!metrics) return NS_ERROR_FAILURE;

  return metrics->GetRotationOfChar(charnum, _retval);
}

/* long getCharNumAtPosition (in DOMSVGPoint point); */
NS_IMETHODIMP
nsSVGTextElement::GetCharNumAtPosition(nsISupports *point, int32_t *_retval)
{
  *_retval = -1;

  nsCOMPtr<DOMSVGPoint> domPoint = do_QueryInterface(point);
  if (!domPoint) {
    return NS_ERROR_DOM_SVG_WRONG_TYPE_ERR;
  }

  nsSVGTextContainerFrame* metrics = GetTextContainerFrame();
  if (metrics)
    *_retval = metrics->GetCharNumAtPosition(domPoint);

  return NS_OK;
}

/* void selectSubString (in unsigned long charnum, in unsigned long nchars); */
NS_IMETHODIMP
nsSVGTextElement::SelectSubString(uint32_t charnum, uint32_t nchars)
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

  return FindAttributeDependence(name, map) ||
    nsSVGTextElementBase::IsAttributeMapped(name);
}

//----------------------------------------------------------------------
// nsSVGElement methods

nsSVGElement::LengthListInfo nsSVGTextElement::sLengthListInfo[4] =
{
  { &nsGkAtoms::x,  SVGContentUtils::X, false },
  { &nsGkAtoms::y,  SVGContentUtils::Y, false },
  { &nsGkAtoms::dx, SVGContentUtils::X, true },
  { &nsGkAtoms::dy, SVGContentUtils::Y, true }
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

