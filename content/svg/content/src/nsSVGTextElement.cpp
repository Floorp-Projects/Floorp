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

#include "nsSVGGraphicElement.h"
#include "nsSVGAtoms.h"
#include "nsIDOMSVGTextElement.h"
#include "nsCOMPtr.h"
#include "nsSVGAnimatedLengthList.h"
#include "nsSVGLengthList.h"
#include "nsISVGSVGElement.h"
#include "nsSVGCoordCtxProvider.h"
#include "nsISVGTextContentMetrics.h"
#include "nsIPresShell.h"
#include "nsIFrame.h"
#include "nsIDocument.h"

typedef nsSVGGraphicElement nsSVGTextElementBase;

class nsSVGTextElement : public nsSVGTextElementBase,
                         public nsIDOMSVGTextElement // : nsIDOMSVGTextPositioningElement
                                                     // : nsIDOMSVGTextContentElement
{
protected:
  friend nsresult NS_NewSVGTextElement(nsIContent **aResult,
                                       nsINodeInfo *aNodeInfo);
  nsSVGTextElement(nsINodeInfo* aNodeInfo);
  virtual ~nsSVGTextElement();
  nsresult Init();
  
public:
  // interfaces:
  
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMSVGTEXTELEMENT
  NS_DECL_NSIDOMSVGTEXTPOSITIONINGELEMENT
  NS_DECL_NSIDOMSVGTEXTCONTENTELEMENT

  // xxx If xpcom allowed virtual inheritance we wouldn't need to
  // forward here :-(
  NS_FORWARD_NSIDOMNODE_NO_CLONENODE(nsSVGTextElementBase::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGTextElementBase::)
  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGTextElementBase::)

  // nsIStyledContent interface
  NS_IMETHOD_(PRBool) IsAttributeMapped(const nsIAtom* aAttribute) const;
  
  // nsISVGContent specializations:
  virtual void ParentChainChanged();

protected:

  already_AddRefed<nsISVGTextContentMetrics> GetTextContentMetrics();
  
  // nsIDOMSVGTextPositioning properties:
  nsCOMPtr<nsIDOMSVGAnimatedLengthList> mX;
  nsCOMPtr<nsIDOMSVGAnimatedLengthList> mY;
  nsCOMPtr<nsIDOMSVGAnimatedLengthList> mdX;
  nsCOMPtr<nsIDOMSVGAnimatedLengthList> mdY;

};


NS_IMPL_NS_NEW_SVG_ELEMENT(Text)


//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGTextElement,nsSVGTextElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGTextElement,nsSVGTextElementBase)

NS_INTERFACE_MAP_BEGIN(nsSVGTextElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNode)
  NS_INTERFACE_MAP_ENTRY(nsIDOMElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGTextElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGTextPositioningElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGTextContentElement)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(SVGTextElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGTextElementBase)

//----------------------------------------------------------------------
// Implementation

nsSVGTextElement::nsSVGTextElement(nsINodeInfo* aNodeInfo)
  : nsSVGTextElementBase(aNodeInfo)
{

}

nsSVGTextElement::~nsSVGTextElement()
{
}

  
nsresult
nsSVGTextElement::Init()
{
  nsresult rv = nsSVGTextElementBase::Init();
  NS_ENSURE_SUCCESS(rv,rv);

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


NS_IMPL_SVG_DOM_CLONENODE(Text)


//----------------------------------------------------------------------
// nsIDOMSVGTextElement methods

// - no methods -

//----------------------------------------------------------------------
// nsIDOMSVGTextPositioningElement methods

/* readonly attribute nsIDOMSVGAnimatedLengthList x; */
NS_IMETHODIMP nsSVGTextElement::GetX(nsIDOMSVGAnimatedLengthList * *aX)
{
  *aX = mX;
  NS_IF_ADDREF(*aX);
  return NS_OK;
}

/* readonly attribute nsIDOMSVGAnimatedLengthList y; */
NS_IMETHODIMP nsSVGTextElement::GetY(nsIDOMSVGAnimatedLengthList * *aY)
{
  *aY = mY;
  NS_IF_ADDREF(*aY);
  return NS_OK;
}

/* readonly attribute nsIDOMSVGAnimatedLengthList dx; */
NS_IMETHODIMP nsSVGTextElement::GetDx(nsIDOMSVGAnimatedLengthList * *aDx)
{
  NS_NOTYETIMPLEMENTED("write me!");
  return NS_ERROR_UNEXPECTED;
}

/* readonly attribute nsIDOMSVGAnimatedLengthList dy; */
NS_IMETHODIMP nsSVGTextElement::GetDy(nsIDOMSVGAnimatedLengthList * *aDy)
{
  NS_NOTYETIMPLEMENTED("write me!");
  return NS_ERROR_UNEXPECTED;
}

/* readonly attribute nsIDOMSVGAnimatedNumberList rotate; */
NS_IMETHODIMP nsSVGTextElement::GetRotate(nsIDOMSVGAnimatedNumberList * *aRotate)
{
  NS_NOTYETIMPLEMENTED("write me!");
  return NS_ERROR_UNEXPECTED;
}

//----------------------------------------------------------------------
// nsIDOMSVGTextContentElement methods

/* readonly attribute nsIDOMSVGAnimatedLength textLength; */
NS_IMETHODIMP nsSVGTextElement::GetTextLength(nsIDOMSVGAnimatedLength * *aTextLength)
{
  NS_NOTYETIMPLEMENTED("write me!");
  return NS_ERROR_UNEXPECTED;
}

/* readonly attribute nsIDOMSVGAnimatedEnumeration lengthAdjust; */
NS_IMETHODIMP nsSVGTextElement::GetLengthAdjust(nsIDOMSVGAnimatedEnumeration * *aLengthAdjust)
{
  NS_NOTYETIMPLEMENTED("write me!");
  return NS_ERROR_UNEXPECTED;
}

/* long getNumberOfChars (); */
NS_IMETHODIMP nsSVGTextElement::GetNumberOfChars(PRInt32 *_retval)
{
  NS_NOTYETIMPLEMENTED("write me!");
  return NS_ERROR_UNEXPECTED;
}

/* float getComputedTextLength (); */
NS_IMETHODIMP nsSVGTextElement::GetComputedTextLength(float *_retval)
{
  NS_NOTYETIMPLEMENTED("write me!");
  return NS_ERROR_UNEXPECTED;
}

/* float getSubStringLength (in unsigned long charnum, in unsigned long nchars); */
NS_IMETHODIMP nsSVGTextElement::GetSubStringLength(PRUint32 charnum, PRUint32 nchars, float *_retval)
{
  NS_NOTYETIMPLEMENTED("write me!");
  return NS_ERROR_UNEXPECTED;
}

/* nsIDOMSVGPoint getStartPositionOfChar (in unsigned long charnum); */
NS_IMETHODIMP nsSVGTextElement::GetStartPositionOfChar(PRUint32 charnum, nsIDOMSVGPoint **_retval)
{
  NS_NOTYETIMPLEMENTED("write me!");
  return NS_ERROR_UNEXPECTED;
}

/* nsIDOMSVGPoint getEndPositionOfChar (in unsigned long charnum); */
NS_IMETHODIMP nsSVGTextElement::GetEndPositionOfChar(PRUint32 charnum, nsIDOMSVGPoint **_retval)
{
  NS_NOTYETIMPLEMENTED("write me!");
  return NS_ERROR_UNEXPECTED;
}

/* nsIDOMSVGRect getExtentOfChar (in unsigned long charnum); */
NS_IMETHODIMP nsSVGTextElement::GetExtentOfChar(PRUint32 charnum, nsIDOMSVGRect **_retval)
{
  *_retval = nsnull;
  nsCOMPtr<nsISVGTextContentMetrics> metrics = GetTextContentMetrics();

  if (!metrics) return NS_ERROR_FAILURE;

  return metrics->GetExtentOfChar(charnum, _retval);
}

/* float getRotationOfChar (in unsigned long charnum); */
NS_IMETHODIMP nsSVGTextElement::GetRotationOfChar(PRUint32 charnum, float *_retval)
{
  NS_NOTYETIMPLEMENTED("write me!");
  return NS_ERROR_UNEXPECTED;
}

/* long getCharNumAtPosition (in nsIDOMSVGPoint point); */
NS_IMETHODIMP nsSVGTextElement::GetCharNumAtPosition(nsIDOMSVGPoint *point, PRInt32 *_retval)
{
  NS_NOTYETIMPLEMENTED("write me!");
  return NS_ERROR_UNEXPECTED;
}

/* void selectSubString (in unsigned long charnum, in unsigned long nchars); */
NS_IMETHODIMP nsSVGTextElement::SelectSubString(PRUint32 charnum, PRUint32 nchars)
{
  NS_NOTYETIMPLEMENTED("write me!");
  return NS_ERROR_UNEXPECTED;
}

//----------------------------------------------------------------------
// nsIStyledContent methods

NS_IMETHODIMP_(PRBool)
nsSVGTextElement::IsAttributeMapped(const nsIAtom* name) const
{
  static const MappedAttributeEntry* const map[] = {
    sTextContentElementsMap,
    sFontSpecificationMap
  };

  return FindAttributeDependence(name, map, NS_ARRAY_LENGTH(map)) ||
    nsSVGTextElementBase::IsAttributeMapped(name);
}

//----------------------------------------------------------------------
// nsISVGContent methods

void nsSVGTextElement::ParentChainChanged()
{
  // set new context information on our length-properties:
  
  nsCOMPtr<nsIDOMSVGSVGElement> dom_elem;
  GetOwnerSVGElement(getter_AddRefs(dom_elem));
  if (!dom_elem) return;

  nsCOMPtr<nsSVGCoordCtxProvider> ctx = do_QueryInterface(dom_elem);
  NS_ASSERTION(ctx, "<svg> element missing interface");

  // x:
  {
    nsCOMPtr<nsIDOMSVGLengthList> dom_lengthlist;
    mX->GetBaseVal(getter_AddRefs(dom_lengthlist));
    nsCOMPtr<nsISVGLengthList> lengthlist = do_QueryInterface(dom_lengthlist);
    NS_ASSERTION(lengthlist, "svg lengthlist missing interface");
    
    lengthlist->SetContext(nsRefPtr<nsSVGCoordCtx>(ctx->GetContextX()));
  }

  // y:
  {
    nsCOMPtr<nsIDOMSVGLengthList> dom_lengthlist;
    mY->GetBaseVal(getter_AddRefs(dom_lengthlist));
    nsCOMPtr<nsISVGLengthList> lengthlist = do_QueryInterface(dom_lengthlist);
    NS_ASSERTION(lengthlist, "svg lengthlist missing interface");
    
    lengthlist->SetContext(nsRefPtr<nsSVGCoordCtx>(ctx->GetContextY()));
  }

  // recurse into child content:
  nsSVGTextElementBase::ParentChainChanged();
}  

//----------------------------------------------------------------------
// implementation helpers:

already_AddRefed<nsISVGTextContentMetrics>
nsSVGTextElement::GetTextContentMetrics()
{
  nsIDocument* doc = GetCurrentDoc();
  if (!doc) {
    NS_ERROR("no document");
    return nsnull;
  }
  
  nsIPresShell* presShell = doc->GetShellAt(0);
  if (!presShell) {
    NS_ERROR("no presshell");
    return nsnull;
  }

  nsIFrame* frame;
  presShell->GetPrimaryFrameFor(NS_STATIC_CAST(nsIStyledContent*, this), &frame);

  if (!frame) {
    NS_ERROR("no frame");
    return nsnull;
  }
  
  nsISVGTextContentMetrics* metrics;
  frame->QueryInterface(NS_GET_IID(nsISVGTextContentMetrics),(void**)&metrics);
  NS_ASSERTION(metrics, "wrong frame type");
  return metrics;
}
