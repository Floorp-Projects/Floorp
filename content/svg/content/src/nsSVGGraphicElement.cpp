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

#include "nsSVGGraphicElement.h"
#include "nsSVGTransformList.h"
#include "nsSVGAnimatedTransformList.h"
#include "nsSVGAtoms.h"
#include "nsSVGMatrix.h"
#include "nsISVGSVGElement.h"
#include "nsIDOMEventTarget.h"
#include "nsIBindingManager.h"
#include "nsIDocument.h"
#include "nsIPresShell.h"
#include "nsIFrame.h"
#include "nsISVGChildFrame.h"

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

nsresult
nsSVGGraphicElement::Init()
{
  nsresult rv = nsSVGGraphicElementBase::Init();
  NS_ENSURE_SUCCESS(rv,rv);

  // Create mapped properties:

  // DOM property: transform, #IMPLIED attrib: transform
  {
    nsCOMPtr<nsIDOMSVGTransformList> transformList;
    rv = nsSVGTransformList::Create(getter_AddRefs(transformList));
    NS_ENSURE_SUCCESS(rv,rv);
    rv = NS_NewSVGAnimatedTransformList(getter_AddRefs(mTransforms),
                                        transformList);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsSVGAtoms::transform, mTransforms);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  return rv;
}

//----------------------------------------------------------------------
// nsIDOMSVGLocatable methods

/* readonly attribute nsIDOMSVGElement nearestViewportElement; */
NS_IMETHODIMP nsSVGGraphicElement::GetNearestViewportElement(nsIDOMSVGElement * *aNearestViewportElement)
{
  NS_NOTYETIMPLEMENTED("write me!");
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute nsIDOMSVGElement farthestViewportElement; */
NS_IMETHODIMP nsSVGGraphicElement::GetFarthestViewportElement(nsIDOMSVGElement * *aFarthestViewportElement)
{
  NS_NOTYETIMPLEMENTED("write me!");
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* nsIDOMSVGRect getBBox (); */
NS_IMETHODIMP nsSVGGraphicElement::GetBBox(nsIDOMSVGRect **_retval)
{
  *_retval = nsnull;
  
  if (!mDocument) return NS_ERROR_FAILURE;
  nsIPresShell *presShell = mDocument->GetShellAt(0);
  NS_ASSERTION(presShell, "no presShell");
  if (!presShell) return NS_ERROR_FAILURE;

  nsIFrame* frame;
  presShell->GetPrimaryFrameFor(NS_STATIC_CAST(nsIStyledContent*, this), &frame);

  NS_ASSERTION(frame, "can't get bounding box for element without frame");

  if (frame) {
    nsISVGChildFrame* svgframe;
    frame->QueryInterface(NS_GET_IID(nsISVGChildFrame),(void**)&svgframe);
    NS_ASSERTION(svgframe, "wrong frame type");
    if (svgframe) {
      return svgframe->GetBBox(_retval);
    }
  }
  return NS_ERROR_FAILURE;
}

/* nsIDOMSVGMatrix getCTM (); */
NS_IMETHODIMP nsSVGGraphicElement::GetCTM(nsIDOMSVGMatrix **_retval)
{
  nsCOMPtr<nsIDOMSVGMatrix> CTM;

  nsIBindingManager *bindingManager = nsnull;
  if (mDocument) {
    bindingManager = mDocument->GetBindingManager();
  }

  nsCOMPtr<nsIContent> parent;
  
  if (bindingManager) {
    // we have a binding manager -- do we have an anonymous parent?
    bindingManager->GetInsertionParent(this, getter_AddRefs(parent));
  }

  if (!parent) {
    // if we didn't find an anonymous parent, use the explicit one,
    // whether it's null or not...
    parent = GetParent();
  }
  
  while (parent) {
    nsCOMPtr<nsIDOMSVGSVGElement> viewportElement = do_QueryInterface(parent);
    if (viewportElement) {
      // Our nearest SVG parent is a viewport element.
      viewportElement->GetViewboxToViewportTransform(getter_AddRefs(CTM));
      break; 
    }
    
    nsCOMPtr<nsIDOMSVGLocatable> locatableElement = do_QueryInterface(parent);
    if (locatableElement) {
      // Our nearest SVG parent is a locatable object that is not a
      // viewport. Its GetCTM function will give us a ctm from the
      // viewport to itself:
      locatableElement->GetCTM(getter_AddRefs(CTM));
      break;
    }

    // Our parent was not svg content. We allow interdispersed non-SVG
    // content to coexist with XBL. Loop until we find the first SVG
    // parent.
    
    nsCOMPtr<nsIContent> next;

    if (bindingManager) {
      bindingManager->GetInsertionParent(parent, getter_AddRefs(next));
    }

    if (!next) {
      // no anonymous parent, so use explicit one
      next = parent->GetParent();
    }

    parent = next;
  }

  if (!CTM) {
    // We either didn't find an SVG parent, or our parent failed in
    // giving us a CTM. In either case:
    NS_WARNING("Couldn't get CTM");
    nsSVGMatrix::Create(getter_AddRefs(CTM));
  }

  // append our local transformations:
  nsCOMPtr<nsIDOMSVGTransformList> transforms;
  mTransforms->GetAnimVal(getter_AddRefs(transforms));
  NS_ENSURE_TRUE(transforms, NS_ERROR_FAILURE);
  nsCOMPtr<nsIDOMSVGMatrix> matrix;
  transforms->GetConsolidation(getter_AddRefs(matrix));

  return CTM->Multiply(matrix, _retval);
}

/* nsIDOMSVGMatrix getScreenCTM (); */
NS_IMETHODIMP nsSVGGraphicElement::GetScreenCTM(nsIDOMSVGMatrix **_retval)
{
  nsCOMPtr<nsIDOMSVGMatrix> screenCTM;

  nsIBindingManager *bindingManager = nsnull;
  if (mDocument) {
    bindingManager = mDocument->GetBindingManager();
  }

  nsCOMPtr<nsIContent> parent;
  
  if (bindingManager) {
    // we have a binding manager -- do we have an anonymous parent?
    bindingManager->GetInsertionParent(this, getter_AddRefs(parent));
  }

  if (!parent) {
    // if we didn't find an anonymous parent, use the explicit one,
    // whether it's null or not...
    parent = GetParent();
  }
  
  while (parent) {
    
    nsCOMPtr<nsIDOMSVGLocatable> locatableElement = do_QueryInterface(parent);
    if (locatableElement) {
      nsCOMPtr<nsIDOMSVGMatrix> ctm;
      locatableElement->GetScreenCTM(getter_AddRefs(ctm));
      if (!ctm) break;
      
      nsCOMPtr<nsIDOMSVGSVGElement> viewportElement = do_QueryInterface(parent);
      if (viewportElement) {
        // It is a viewport element. we need to append the viewbox xform:
        nsCOMPtr<nsIDOMSVGMatrix> matrix;
        viewportElement->GetViewboxToViewportTransform(getter_AddRefs(matrix));
        ctm->Multiply(matrix, getter_AddRefs(screenCTM));
      }
      else
        screenCTM = ctm;

      break;
    }

    // Our parent was not svg content. We allow interdispersed non-SVG
    // content to coexist with XBL. Loop until we find the first SVG
    // parent.
    
    nsCOMPtr<nsIContent> next;

    if (bindingManager) {
      bindingManager->GetInsertionParent(parent, getter_AddRefs(next));
    }

    if (!next) {
      // no anonymous parent, so use explicit one
      next = parent->GetParent();
    }

    parent = next;
  }

  if (!screenCTM) {
    // We either didn't find an SVG parent, or our parent failed in
    // giving us a CTM. In either case:
    NS_ERROR("couldn't get ctm");
    nsSVGMatrix::Create(getter_AddRefs(screenCTM));
  }

  // append our local transformations:
  nsCOMPtr<nsIDOMSVGTransformList> transforms;
  mTransforms->GetAnimVal(getter_AddRefs(transforms));
  NS_ENSURE_TRUE(transforms, NS_ERROR_FAILURE);
  nsCOMPtr<nsIDOMSVGMatrix> matrix;
  transforms->GetConsolidation(getter_AddRefs(matrix));

  return screenCTM->Multiply(matrix, _retval);
}

/* nsIDOMSVGMatrix getTransformToElement (in nsIDOMSVGElement element); */
NS_IMETHODIMP nsSVGGraphicElement::GetTransformToElement(nsIDOMSVGElement *element, nsIDOMSVGMatrix **_retval)
{
  NS_NOTYETIMPLEMENTED("write me!");
  return NS_ERROR_NOT_IMPLEMENTED;
}

//----------------------------------------------------------------------
// nsIDOMSVGTransformable methods
/* readonly attribute nsIDOMSVGAnimatedTransformList transform; */

NS_IMETHODIMP nsSVGGraphicElement::GetTransform(nsIDOMSVGAnimatedTransformList * *aTransform)
{
  *aTransform = mTransforms;
  NS_IF_ADDREF(*aTransform);
  return NS_OK;
}


//----------------------------------------------------------------------
// nsIStyledContent methods

NS_IMETHODIMP_(PRBool)
nsSVGGraphicElement::IsAttributeMapped(const nsIAtom* name) const
{
  static const MappedAttributeEntry* const map[] = {
    sFillStrokeMap,
    sGraphicsMap,
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
