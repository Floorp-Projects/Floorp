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

#include "nsSVGElement.h"
#include "nsSVGAtoms.h"
#include "nsIDOMSVGTSpanElement.h"
#include "nsCOMPtr.h"
#include "nsSVGAnimatedLengthList.h"
#include "nsSVGLengthList.h"
#include "nsISVGSVGElement.h"
#include "nsISVGViewportAxis.h"
#include "nsISVGViewportRect.h"

typedef nsSVGElement nsSVGTSpanElementBase;

class nsSVGTSpanElement : public nsSVGTSpanElementBase,
                          public nsIDOMSVGTSpanElement // : nsIDOMSVGTextPositioningElement
                                                       // : nsIDOMSVGTextContentElement
{
protected:
  friend nsresult NS_NewSVGTSpanElement(nsIContent **aResult,
                                        nsINodeInfo *aNodeInfo);
  nsSVGTSpanElement(nsINodeInfo* aNodeInfo);
  virtual ~nsSVGTSpanElement();
  nsresult Init();
  
public:
  // interfaces:
  
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMSVGTSPANELEMENT
  NS_DECL_NSIDOMSVGTEXTPOSITIONINGELEMENT
  NS_DECL_NSIDOMSVGTEXTCONTENTELEMENT

  // xxx If xpcom allowed virtual inheritance we wouldn't need to
  // forward here :-(
  NS_FORWARD_NSIDOMNODE_NO_CLONENODE(nsSVGTSpanElementBase::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGTSpanElementBase::)
  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGTSpanElementBase::)

  // nsIStyledContent interface
  NS_IMETHOD_(PRBool) IsAttributeMapped(const nsIAtom* aAttribute) const;

  // nsISVGContent specializations:
  virtual void ParentChainChanged();

protected:

  // nsIDOMSVGTextPositioning properties:
  nsCOMPtr<nsIDOMSVGAnimatedLengthList> mX;
  nsCOMPtr<nsIDOMSVGAnimatedLengthList> mY;
  nsCOMPtr<nsIDOMSVGAnimatedLengthList> mdX;
  nsCOMPtr<nsIDOMSVGAnimatedLengthList> mdY;

};


NS_IMPL_NS_NEW_SVG_ELEMENT(TSpan)


//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGTSpanElement,nsSVGTSpanElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGTSpanElement,nsSVGTSpanElementBase)

NS_INTERFACE_MAP_BEGIN(nsSVGTSpanElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNode)
  NS_INTERFACE_MAP_ENTRY(nsIDOMElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGTSpanElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGTextPositioningElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGTextContentElement)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(SVGTSpanElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGTSpanElementBase)

//----------------------------------------------------------------------
// Implementation

nsSVGTSpanElement::nsSVGTSpanElement(nsINodeInfo *aNodeInfo)
  : nsSVGTSpanElementBase(aNodeInfo)
{

}

nsSVGTSpanElement::~nsSVGTSpanElement()
{
}

  
nsresult
nsSVGTSpanElement::Init()
{
  nsresult rv;

  // Create mapped properties:

  // DOM property: nsIDOMSVGTextPositioningElement::x, #IMPLIED attrib: x
  {
    nsCOMPtr<nsISVGLengthList> lengthList;
    rv = NS_NewSVGLengthList(getter_AddRefs(lengthList));
    NS_ENSURE_SUCCESS(rv,rv);
    rv = NS_NewSVGAnimatedLengthList(getter_AddRefs(mX),
                                     lengthList);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsSVGAtoms::x, mX);
    NS_ENSURE_SUCCESS(rv,rv);
  }
  
  // DOM property: nsIDOMSVGTextPositioningElement::y, #IMPLIED attrib: y
  {
    nsCOMPtr<nsISVGLengthList> lengthList;
    rv = NS_NewSVGLengthList(getter_AddRefs(lengthList));
    NS_ENSURE_SUCCESS(rv,rv);
    rv = NS_NewSVGAnimatedLengthList(getter_AddRefs(mY),
                                     lengthList);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsSVGAtoms::y, mY);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  return rv;
}

//----------------------------------------------------------------------
// nsIDOMNode methods


NS_IMPL_SVG_DOM_CLONENODE(TSpan)


//----------------------------------------------------------------------
// nsIDOMSVGTSpanElement methods

// - no methods -

//----------------------------------------------------------------------
// nsIDOMSVGTextPositioningElement methods

/* readonly attribute nsIDOMSVGAnimatedLengthList x; */
NS_IMETHODIMP nsSVGTSpanElement::GetX(nsIDOMSVGAnimatedLengthList * *aX)
{
  *aX = mX;
  NS_IF_ADDREF(*aX);
  return NS_OK;
}

/* readonly attribute nsIDOMSVGAnimatedLengthList y; */
NS_IMETHODIMP nsSVGTSpanElement::GetY(nsIDOMSVGAnimatedLengthList * *aY)
{
  *aY = mY;
  NS_IF_ADDREF(*aY);
  return NS_OK;
}

/* readonly attribute nsIDOMSVGAnimatedLengthList dx; */
NS_IMETHODIMP nsSVGTSpanElement::GetDx(nsIDOMSVGAnimatedLengthList * *aDx)
{
  NS_NOTYETIMPLEMENTED("write me!");
  return NS_ERROR_UNEXPECTED;
}

/* readonly attribute nsIDOMSVGAnimatedLengthList dy; */
NS_IMETHODIMP nsSVGTSpanElement::GetDy(nsIDOMSVGAnimatedLengthList * *aDy)
{
  NS_NOTYETIMPLEMENTED("write me!");
  return NS_ERROR_UNEXPECTED;
}

/* readonly attribute nsIDOMSVGAnimatedNumberList rotate; */
NS_IMETHODIMP nsSVGTSpanElement::GetRotate(nsIDOMSVGAnimatedNumberList * *aRotate)
{
  NS_NOTYETIMPLEMENTED("write me!");
  return NS_ERROR_UNEXPECTED;
}

//----------------------------------------------------------------------
// nsIDOMSVGTextContentElement methods

/* readonly attribute nsIDOMSVGAnimatedLength textLength; */
NS_IMETHODIMP nsSVGTSpanElement::GetTextLength(nsIDOMSVGAnimatedLength * *aTextLength)
{
  NS_NOTYETIMPLEMENTED("write me!");
  return NS_ERROR_UNEXPECTED;
}

/* readonly attribute nsIDOMSVGAnimatedEnumeration lengthAdjust; */
NS_IMETHODIMP nsSVGTSpanElement::GetLengthAdjust(nsIDOMSVGAnimatedEnumeration * *aLengthAdjust)
{
  NS_NOTYETIMPLEMENTED("write me!");
  return NS_ERROR_UNEXPECTED;
}

/* long getNumberOfChars (); */
NS_IMETHODIMP nsSVGTSpanElement::GetNumberOfChars(PRInt32 *_retval)
{
  NS_NOTYETIMPLEMENTED("write me!");
  return NS_ERROR_UNEXPECTED;
}

/* float getComputedTextLength (); */
NS_IMETHODIMP nsSVGTSpanElement::GetComputedTextLength(float *_retval)
{
  NS_NOTYETIMPLEMENTED("write me!");
  return NS_ERROR_UNEXPECTED;
}

/* float getSubStringLength (in unsigned long charnum, in unsigned long nchars); */
NS_IMETHODIMP nsSVGTSpanElement::GetSubStringLength(PRUint32 charnum, PRUint32 nchars, float *_retval)
{
  NS_NOTYETIMPLEMENTED("write me!");
  return NS_ERROR_UNEXPECTED;
}

/* nsIDOMSVGPoint getStartPositionOfChar (in unsigned long charnum); */
NS_IMETHODIMP nsSVGTSpanElement::GetStartPositionOfChar(PRUint32 charnum, nsIDOMSVGPoint **_retval)
{
  NS_NOTYETIMPLEMENTED("write me!");
  return NS_ERROR_UNEXPECTED;
}

/* nsIDOMSVGPoint getEndPositionOfChar (in unsigned long charnum); */
NS_IMETHODIMP nsSVGTSpanElement::GetEndPositionOfChar(PRUint32 charnum, nsIDOMSVGPoint **_retval)
{
  NS_NOTYETIMPLEMENTED("write me!");
  return NS_ERROR_UNEXPECTED;
}

/* nsIDOMSVGRect getExtentOfChar (in unsigned long charnum); */
NS_IMETHODIMP nsSVGTSpanElement::GetExtentOfChar(PRUint32 charnum, nsIDOMSVGRect **_retval)
{
  NS_NOTYETIMPLEMENTED("write me!");
  return NS_ERROR_UNEXPECTED;
}

/* float getRotationOfChar (in unsigned long charnum); */
NS_IMETHODIMP nsSVGTSpanElement::GetRotationOfChar(PRUint32 charnum, float *_retval)
{
  NS_NOTYETIMPLEMENTED("write me!");
  return NS_ERROR_UNEXPECTED;
}

/* long getCharNumAtPosition (in nsIDOMSVGPoint point); */
NS_IMETHODIMP nsSVGTSpanElement::GetCharNumAtPosition(nsIDOMSVGPoint *point, PRInt32 *_retval)
{
  NS_NOTYETIMPLEMENTED("write me!");
  return NS_ERROR_UNEXPECTED;
}

/* void selectSubString (in unsigned long charnum, in unsigned long nchars); */
NS_IMETHODIMP nsSVGTSpanElement::SelectSubString(PRUint32 charnum, PRUint32 nchars)
{
  NS_NOTYETIMPLEMENTED("write me!");
  return NS_ERROR_UNEXPECTED;
}

//----------------------------------------------------------------------
// nsIStyledContent methods

NS_IMETHODIMP_(PRBool)
nsSVGTSpanElement::IsAttributeMapped(const nsIAtom* name) const
{
  static const MappedAttributeEntry* const map[] = {
    sFillStrokeMap,
    sGraphicsMap,
    sTextContentElementsMap,
    sFontSpecificationMap
  };
  
  return FindAttributeDependence(name, map, NS_ARRAY_LENGTH(map)) ||
    nsSVGTSpanElementBase::IsAttributeMapped(name);
}

//----------------------------------------------------------------------
// nsISVGContent methods

void nsSVGTSpanElement::ParentChainChanged()
{
  // set new context information on our length-properties:
  
  nsCOMPtr<nsIDOMSVGSVGElement> dom_elem;
  GetOwnerSVGElement(getter_AddRefs(dom_elem));
  if (!dom_elem) return;

  nsCOMPtr<nsISVGSVGElement> svg_elem = do_QueryInterface(dom_elem);
  NS_ASSERTION(svg_elem, "<svg> element missing interface");

  // x:
  {
    nsCOMPtr<nsIDOMSVGLengthList> dom_lengthlist;
    mX->GetBaseVal(getter_AddRefs(dom_lengthlist));
    nsCOMPtr<nsISVGLengthList> lengthlist = do_QueryInterface(dom_lengthlist);
    NS_ASSERTION(lengthlist, "svg lengthlist missing interface");

    nsCOMPtr<nsIDOMSVGRect> vp_dom;
    svg_elem->GetViewport(getter_AddRefs(vp_dom));
    nsCOMPtr<nsISVGViewportRect> vp = do_QueryInterface(vp_dom);
    nsCOMPtr<nsISVGViewportAxis> ctx;
    vp->GetXAxis(getter_AddRefs(ctx));
    
    lengthlist->SetContext(ctx);
  }

  // y:
  {
    nsCOMPtr<nsIDOMSVGLengthList> dom_lengthlist;
    mY->GetBaseVal(getter_AddRefs(dom_lengthlist));
    nsCOMPtr<nsISVGLengthList> lengthlist = do_QueryInterface(dom_lengthlist);
    NS_ASSERTION(lengthlist, "svg lengthlist missing interface");

    nsCOMPtr<nsIDOMSVGRect> vp_dom;
    svg_elem->GetViewport(getter_AddRefs(vp_dom));
    nsCOMPtr<nsISVGViewportRect> vp = do_QueryInterface(vp_dom);
    nsCOMPtr<nsISVGViewportAxis> ctx;
    vp->GetYAxis(getter_AddRefs(ctx));
    
    lengthlist->SetContext(ctx);
  }

  // recurse into child content:
  nsSVGTSpanElementBase::ParentChainChanged();
}  
