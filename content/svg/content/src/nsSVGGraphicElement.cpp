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
 *   Jonathan Watt <jonathan.watt@strath.ac.uk>
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
#include "nsSVGTransformList.h"
#include "nsSVGAnimatedTransformList.h"
#include "nsGkAtoms.h"
#include "nsSVGMatrix.h"
#include "nsISVGSVGElement.h"
#include "nsIDOMEventTarget.h"
#include "nsBindingManager.h"
#include "nsIFrame.h"
#include "nsISVGChildFrame.h"
#include "nsIDOMSVGPoint.h"
#include "nsSVGUtils.h"
#include "nsDOMError.h"

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGGraphicElement, nsSVGGraphicElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGGraphicElement, nsSVGGraphicElementBase)

NS_INTERFACE_MAP_BEGIN(nsSVGGraphicElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGLocatable)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGTransformable)
NS_INTERFACE_MAP_END_INHERITING(nsSVGGraphicElementBase)

//----------------------------------------------------------------------
// Implementation

nsSVGGraphicElement::nsSVGGraphicElement(nsINodeInfo *aNodeInfo)
  : nsSVGGraphicElementBase(aNodeInfo)
{
}

//----------------------------------------------------------------------
// nsIDOMSVGLocatable methods

/* readonly attribute nsIDOMSVGElement nearestViewportElement; */
NS_IMETHODIMP nsSVGGraphicElement::GetNearestViewportElement(nsIDOMSVGElement * *aNearestViewportElement)
{
  NS_NOTYETIMPLEMENTED("nsSVGGraphicElement::GetNearestViewportElement");
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute nsIDOMSVGElement farthestViewportElement; */
NS_IMETHODIMP nsSVGGraphicElement::GetFarthestViewportElement(nsIDOMSVGElement * *aFarthestViewportElement)
{
  NS_NOTYETIMPLEMENTED("nsSVGGraphicElement::GetFarthestViewportElement");
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* nsIDOMSVGRect getBBox (); */
NS_IMETHODIMP nsSVGGraphicElement::GetBBox(nsIDOMSVGRect **_retval)
{
  *_retval = nsnull;

  nsIFrame* frame = GetPrimaryFrame(Flush_Layout);

  if (!frame || (frame->GetStateBits() & NS_STATE_SVG_NONDISPLAY_CHILD))
    return NS_ERROR_FAILURE;

  nsISVGChildFrame* svgframe;
  CallQueryInterface(frame, &svgframe);
  NS_ASSERTION(svgframe, "wrong frame type");
  if (svgframe) {
    svgframe->SetMatrixPropagation(PR_FALSE);
    svgframe->NotifyCanvasTMChanged(PR_TRUE);
    nsresult rv = svgframe->GetBBox(_retval);
    svgframe->SetMatrixPropagation(PR_TRUE);
    svgframe->NotifyCanvasTMChanged(PR_TRUE);
    return rv;
  }
  return NS_ERROR_FAILURE;
}

/* Helper for GetCTM and GetScreenCTM */
nsresult
nsSVGGraphicElement::AppendLocalTransform(nsIDOMSVGMatrix *aCTM,
                                          nsIDOMSVGMatrix **_retval)
{
  if (!mTransforms) {
    *_retval = aCTM;
    NS_ADDREF(*_retval);
    return NS_OK;
  }

  // append our local transformations
  nsCOMPtr<nsIDOMSVGTransformList> transforms;
  mTransforms->GetAnimVal(getter_AddRefs(transforms));
  NS_ENSURE_TRUE(transforms, NS_ERROR_FAILURE);
  nsCOMPtr<nsIDOMSVGMatrix> matrix =
    nsSVGTransformList::GetConsolidationMatrix(transforms);
  if (!matrix) {
    *_retval = aCTM;
    NS_ADDREF(*_retval);
    return NS_OK;
  }
  return aCTM->Multiply(matrix, _retval);  // addrefs, so we don't
}

/* nsIDOMSVGMatrix getCTM (); */
NS_IMETHODIMP nsSVGGraphicElement::GetCTM(nsIDOMSVGMatrix **_retval)
{
  nsresult rv;
  *_retval = nsnull;

  nsBindingManager *bindingManager = nsnull;
  // XXXbz I _think_ this is right.  We want to be using the binding manager
  // that would have attached the binding that gives us our anonymous parent.
  // That's the binding manager for the document we actually belong to, which
  // is our owner doc.
  nsIDocument* ownerDoc = GetOwnerDoc();
  if (ownerDoc) {
    bindingManager = ownerDoc->BindingManager();
  }

  nsIContent* parent = nsnull;
  nsCOMPtr<nsIDOMSVGMatrix> parentCTM;

  if (bindingManager) {
    // check for an anonymous parent first
    parent = bindingManager->GetInsertionParent(this);
  }
  if (!parent) {
    // if we didn't find an anonymous parent, use the explicit one
    parent = GetParent();
  }

  nsCOMPtr<nsIDOMSVGLocatable> locatableElement = do_QueryInterface(parent);
  if (!locatableElement) {
    // we don't have an SVGLocatable parent so we aren't even rendered
    NS_WARNING("SVGGraphicElement without an SVGLocatable parent");
    return NS_ERROR_FAILURE;
  }

  // get our parent's CTM
  rv = locatableElement->GetCTM(getter_AddRefs(parentCTM));
  if (NS_FAILED(rv)) return rv;

  return AppendLocalTransform(parentCTM, _retval);
}

/* nsIDOMSVGMatrix getScreenCTM (); */
NS_IMETHODIMP nsSVGGraphicElement::GetScreenCTM(nsIDOMSVGMatrix **_retval)
{
  nsresult rv;
  *_retval = nsnull;

  nsBindingManager *bindingManager = nsnull;
  // XXXbz I _think_ this is right.  We want to be using the binding manager
  // that would have attached the binding that gives us our anonymous parent.
  // That's the binding manager for the document we actually belong to, which
  // is our owner doc.
  nsIDocument* ownerDoc = GetOwnerDoc();
  if (ownerDoc) {
    bindingManager = ownerDoc->BindingManager();
  }

  nsIContent* parent = nsnull;
  nsCOMPtr<nsIDOMSVGMatrix> parentScreenCTM;

  if (bindingManager) {
    // check for an anonymous parent first
    parent = bindingManager->GetInsertionParent(this);
  }
  if (!parent) {
    // if we didn't find an anonymous parent, use the explicit one
    parent = GetParent();
  }

  nsCOMPtr<nsIDOMSVGLocatable> locatableElement = do_QueryInterface(parent);
  if (!locatableElement) {
    // we don't have an SVGLocatable parent so we aren't even rendered
    NS_WARNING("SVGGraphicElement without an SVGLocatable parent");
    return NS_ERROR_FAILURE;
  }

  // get our parent's "screen" CTM
  rv = locatableElement->GetScreenCTM(getter_AddRefs(parentScreenCTM));
  if (NS_FAILED(rv)) return rv;

  return AppendLocalTransform(parentScreenCTM, _retval);
}

/* nsIDOMSVGMatrix getTransformToElement (in nsIDOMSVGElement element); */
NS_IMETHODIMP nsSVGGraphicElement::GetTransformToElement(nsIDOMSVGElement *element, nsIDOMSVGMatrix **_retval)
{
  if (!element)
    return NS_ERROR_DOM_SVG_WRONG_TYPE_ERR;

  nsresult rv;
  *_retval = nsnull;
  nsCOMPtr<nsIDOMSVGMatrix> ourScreenCTM;
  nsCOMPtr<nsIDOMSVGMatrix> targetScreenCTM;
  nsCOMPtr<nsIDOMSVGMatrix> tmp;
  nsCOMPtr<nsIDOMSVGLocatable> target = do_QueryInterface(element, &rv);
  if (NS_FAILED(rv)) return rv;

  // the easiest way to do this (if likely to increase rounding error):
  rv = GetScreenCTM(getter_AddRefs(ourScreenCTM));
  if (NS_FAILED(rv)) return rv;
  rv = target->GetScreenCTM(getter_AddRefs(targetScreenCTM));
  if (NS_FAILED(rv)) return rv;
  rv = targetScreenCTM->Inverse(getter_AddRefs(tmp));
  if (NS_FAILED(rv)) return rv;
  return tmp->Multiply(ourScreenCTM, _retval);  // addrefs, so we don't
}

//----------------------------------------------------------------------
// nsIDOMSVGTransformable methods
/* readonly attribute nsIDOMSVGAnimatedTransformList transform; */

NS_IMETHODIMP nsSVGGraphicElement::GetTransform(nsIDOMSVGAnimatedTransformList * *aTransform)
{
  if (!mTransforms && NS_FAILED(CreateTransformList()))
    return NS_ERROR_OUT_OF_MEMORY;
      
  *aTransform = mTransforms;
  NS_ADDREF(*aTransform);
  return NS_OK;
}

//----------------------------------------------------------------------
// nsIContent methods

NS_IMETHODIMP_(PRBool)
nsSVGGraphicElement::IsAttributeMapped(const nsIAtom* name) const
{
  static const MappedAttributeEntry* const map[] = {
    sFillStrokeMap,
    sGraphicsMap,
    sColorMap
  };
  
  return FindAttributeDependence(name, map, NS_ARRAY_LENGTH(map)) ||
    nsSVGGraphicElementBase::IsAttributeMapped(name);
}

//----------------------------------------------------------------------
// nsSVGElement overrides

PRBool
nsSVGGraphicElement::IsEventName(nsIAtom* aName)
{
  return IsGraphicElementEventName(aName);
}

already_AddRefed<nsIDOMSVGMatrix>
nsSVGGraphicElement::GetLocalTransformMatrix()
{
  if (!mTransforms)
    return nsnull;

  nsresult rv;

  nsCOMPtr<nsIDOMSVGTransformList> transforms;
  rv = mTransforms->GetAnimVal(getter_AddRefs(transforms));
  NS_ENSURE_SUCCESS(rv, nsnull);

  return nsSVGTransformList::GetConsolidationMatrix(transforms);
}

nsresult
nsSVGGraphicElement::BeforeSetAttr(PRInt32 aNamespaceID, nsIAtom* aName,
                                   const nsAString* aValue, PRBool aNotify)
{
  if (aNamespaceID == kNameSpaceID_None &&
      aName == nsGkAtoms::transform &&
      !mTransforms &&
      NS_FAILED(CreateTransformList()))
    return NS_ERROR_OUT_OF_MEMORY;

  return nsSVGGraphicElementBase::BeforeSetAttr(aNamespaceID, aName,
                                                aValue, aNotify);
}

nsresult
nsSVGGraphicElement::CreateTransformList()
{
  nsresult rv;

  // DOM property: transform, #IMPLIED attrib: transform
  nsCOMPtr<nsIDOMSVGTransformList> transformList;
  rv = nsSVGTransformList::Create(getter_AddRefs(transformList));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = NS_NewSVGAnimatedTransformList(getter_AddRefs(mTransforms),
                                      transformList);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = AddMappedSVGValue(nsGkAtoms::transform, mTransforms);
  if (NS_FAILED(rv)) {
    mTransforms = nsnull;
    return rv;
  }

  return NS_OK;
}
