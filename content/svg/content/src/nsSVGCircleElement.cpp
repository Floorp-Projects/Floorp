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
#include "nsSVGAtoms.h"
#include "nsSVGAnimatedLength.h"
#include "nsSVGLength.h"
#include "nsIDOMSVGCircleElement.h"
#include "nsCOMPtr.h"
#include "nsISVGSVGElement.h"
#include "nsISVGViewportAxis.h"
#include "nsISVGViewportRect.h"

typedef nsSVGGraphicElement nsSVGCircleElementBase;

class nsSVGCircleElement : public nsSVGCircleElementBase,
                           public nsIDOMSVGCircleElement
{
protected:
  friend nsresult NS_NewSVGCircleElement(nsIContent **aResult,
                                         nsINodeInfo *aNodeInfo);
  nsSVGCircleElement(nsINodeInfo *aNodeInfo);
  virtual ~nsSVGCircleElement();
  nsresult Init();

public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMSVGCIRCLEELEMENT

  // xxx I wish we could use virtual inheritance
  NS_FORWARD_NSIDOMNODE_NO_CLONENODE(nsSVGCircleElementBase::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGCircleElementBase::)
  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGCircleElementBase::)

  // nsISVGContent specializations:
  virtual void ParentChainChanged();
    
protected:
  
  nsCOMPtr<nsIDOMSVGAnimatedLength> mCx;
  nsCOMPtr<nsIDOMSVGAnimatedLength> mCy;
  nsCOMPtr<nsIDOMSVGAnimatedLength> mR;
  
};


NS_IMPL_NS_NEW_SVG_ELEMENT(Circle)


//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGCircleElement,nsSVGCircleElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGCircleElement,nsSVGCircleElementBase)

NS_INTERFACE_MAP_BEGIN(nsSVGCircleElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNode)
  NS_INTERFACE_MAP_ENTRY(nsIDOMElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGCircleElement)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(SVGCircleElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGCircleElementBase)

//----------------------------------------------------------------------
// Implementation

nsSVGCircleElement::nsSVGCircleElement(nsINodeInfo *aNodeInfo)
  : nsSVGCircleElementBase(aNodeInfo)
{

}

nsSVGCircleElement::~nsSVGCircleElement()
{
}

  
nsresult
nsSVGCircleElement::Init()
{
  nsresult rv = nsSVGCircleElementBase::Init();
  NS_ENSURE_SUCCESS(rv,rv);

  // Create mapped properties:

  // DOM property: cx ,  #IMPLIED attrib: cx
  {
    nsCOMPtr<nsISVGLength> length;
    rv = NS_NewSVGLength(getter_AddRefs(length), 0.0f);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = NS_NewSVGAnimatedLength(getter_AddRefs(mCx), length);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsSVGAtoms::cx, mCx);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  // DOM property: cy ,  #IMPLIED attrib: cy
  {
    nsCOMPtr<nsISVGLength> length;
    rv = NS_NewSVGLength(getter_AddRefs(length), 0.0f);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = NS_NewSVGAnimatedLength(getter_AddRefs(mCy), length);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsSVGAtoms::cy, mCy);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  // DOM property: r ,  #REQUIRED  attrib: r
  // XXX: enforce requiredness
  {
    nsCOMPtr<nsISVGLength> length;
    rv = NS_NewSVGLength(getter_AddRefs(length), 0.0f);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = NS_NewSVGAnimatedLength(getter_AddRefs(mR), length);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsSVGAtoms::r, mR);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  return rv;
}

//----------------------------------------------------------------------
// nsIDOMNode methods


NS_IMPL_SVG_DOM_CLONENODE(Circle)


//----------------------------------------------------------------------
// nsIDOMSVGCircleElement methods

/* readonly attribute nsIDOMSVGAnimatedLength cx; */
NS_IMETHODIMP nsSVGCircleElement::GetCx(nsIDOMSVGAnimatedLength * *aCx)
{
  *aCx = mCx;
  NS_IF_ADDREF(*aCx);
  return NS_OK;
}

/* readonly attribute nsIDOMSVGAnimatedLength cy; */
NS_IMETHODIMP nsSVGCircleElement::GetCy(nsIDOMSVGAnimatedLength * *aCy)
{
  *aCy = mCy;
  NS_IF_ADDREF(*aCy);
  return NS_OK;
}

/* readonly attribute nsIDOMSVGAnimatedLength r; */
NS_IMETHODIMP nsSVGCircleElement::GetR(nsIDOMSVGAnimatedLength * *aR)
{
   *aR = mR;
  NS_IF_ADDREF(*aR);
  return NS_OK;
}

//----------------------------------------------------------------------
// nsISVGContent methods

void nsSVGCircleElement::ParentChainChanged()
{
  // set new context information on our length-properties:
  
  nsCOMPtr<nsIDOMSVGSVGElement> dom_elem;
  GetOwnerSVGElement(getter_AddRefs(dom_elem));
  if (!dom_elem) return;

  nsCOMPtr<nsISVGSVGElement> svg_elem = do_QueryInterface(dom_elem);
  NS_ASSERTION(svg_elem, "<svg> element missing interface");

  // cx:
  {
    nsCOMPtr<nsIDOMSVGLength> dom_length;
    mCx->GetBaseVal(getter_AddRefs(dom_length));
    nsCOMPtr<nsISVGLength> length = do_QueryInterface(dom_length);
    NS_ASSERTION(length, "svg length missing interface");

    nsCOMPtr<nsIDOMSVGRect> vp_dom;
    svg_elem->GetViewport(getter_AddRefs(vp_dom));
    nsCOMPtr<nsISVGViewportRect> vp = do_QueryInterface(vp_dom);
    nsCOMPtr<nsISVGViewportAxis> ctx;
    vp->GetXAxis(getter_AddRefs(ctx));
    
    length->SetContext(ctx);
  }

  // cy:
  {
    nsCOMPtr<nsIDOMSVGLength> dom_length;
    mCy->GetBaseVal(getter_AddRefs(dom_length));
    nsCOMPtr<nsISVGLength> length = do_QueryInterface(dom_length);
    NS_ASSERTION(length, "svg length missing interface");

    nsCOMPtr<nsIDOMSVGRect> vp_dom;
    svg_elem->GetViewport(getter_AddRefs(vp_dom));
    nsCOMPtr<nsISVGViewportRect> vp = do_QueryInterface(vp_dom);
    nsCOMPtr<nsISVGViewportAxis> ctx;
    vp->GetYAxis(getter_AddRefs(ctx));
    
    length->SetContext(ctx);
  }

  // r:
  {
    nsCOMPtr<nsIDOMSVGLength> dom_length;
    mR->GetBaseVal(getter_AddRefs(dom_length));
    nsCOMPtr<nsISVGLength> length = do_QueryInterface(dom_length);
    NS_ASSERTION(length, "svg length missing interface");

    nsCOMPtr<nsIDOMSVGRect> vp_dom;
    svg_elem->GetViewport(getter_AddRefs(vp_dom));
    nsCOMPtr<nsISVGViewportRect> vp = do_QueryInterface(vp_dom);
    nsCOMPtr<nsISVGViewportAxis> ctx;
    vp->GetUnspecifiedAxis(getter_AddRefs(ctx));
    
    length->SetContext(ctx);
  }

  // XXX call baseclass version to recurse into children?
}  
