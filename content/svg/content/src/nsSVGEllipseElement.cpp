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
 *   William Cook <william.cook@crocodile-clips.com> (original author)
 *   Alex Fritze <alex.fritze@crocodile-clips.com>
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
#include "nsIDOMSVGEllipseElement.h"
#include "nsCOMPtr.h"
#include "nsISVGSVGElement.h"
#include "nsSVGCoordCtxProvider.h"

typedef nsSVGGraphicElement nsSVGEllipseElementBase;

class nsSVGEllipseElement : public nsSVGEllipseElementBase,
                            public nsIDOMSVGEllipseElement
{
protected:
  friend nsresult NS_NewSVGEllipseElement(nsIContent **aResult,
                                         nsINodeInfo *aNodeInfo);
  nsSVGEllipseElement(nsINodeInfo *aNodeInfo);
  virtual ~nsSVGEllipseElement();
  nsresult Init();

public:
  // interfaces:

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMSVGELLIPSEELEMENT

  // xxx I wish we could use virtual inheritance
  NS_FORWARD_NSIDOMNODE_NO_CLONENODE(nsSVGEllipseElementBase::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGEllipseElementBase::)
  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGEllipseElementBase::)

  // nsISVGContent specializations:
  virtual void ParentChainChanged();

protected:

  nsCOMPtr<nsIDOMSVGAnimatedLength> mCx;
  nsCOMPtr<nsIDOMSVGAnimatedLength> mCy;
  nsCOMPtr<nsIDOMSVGAnimatedLength> mRx;
  nsCOMPtr<nsIDOMSVGAnimatedLength> mRy;

};


NS_IMPL_NS_NEW_SVG_ELEMENT(Ellipse)


//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGEllipseElement,nsSVGEllipseElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGEllipseElement,nsSVGEllipseElementBase)

NS_INTERFACE_MAP_BEGIN(nsSVGEllipseElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNode)
  NS_INTERFACE_MAP_ENTRY(nsIDOMElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGEllipseElement)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(SVGEllipseElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGEllipseElementBase)

//----------------------------------------------------------------------
// Implementation

nsSVGEllipseElement::nsSVGEllipseElement(nsINodeInfo *aNodeInfo)
  : nsSVGEllipseElementBase(aNodeInfo)
{

}

nsSVGEllipseElement::~nsSVGEllipseElement()
{
}


nsresult
nsSVGEllipseElement::Init()
{
  nsresult rv = nsSVGEllipseElementBase::Init();
  NS_ENSURE_SUCCESS(rv,rv);

  // Create mapped properties:

  // DOM property: cx ,  #IMPLIED attrib: cx
  {
    nsCOMPtr<nsISVGLength> length;
    rv = NS_NewSVGLength(getter_AddRefs(length),
                         0.0f);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = NS_NewSVGAnimatedLength(getter_AddRefs(mCx), length);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsSVGAtoms::cx, mCx);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  // DOM property: cy ,  #IMPLIED attrib: cy
  {
    nsCOMPtr<nsISVGLength> length;
    rv = NS_NewSVGLength(getter_AddRefs(length),
                         0.0f);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = NS_NewSVGAnimatedLength(getter_AddRefs(mCy), length);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsSVGAtoms::cy, mCy);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  // DOM property: rx ,  #REQUIRED  attrib: rx
  // XXX: enforce requiredness
  {
    nsCOMPtr<nsISVGLength> length;
    rv = NS_NewSVGLength(getter_AddRefs(length),
                         0.0f);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = NS_NewSVGAnimatedLength(getter_AddRefs(mRx), length);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsSVGAtoms::rx, mRx);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  // DOM property: ry ,  #REQUIRED  attrib: ry
  // XXX: enforce requiredness
  {
    nsCOMPtr<nsISVGLength> length;
    rv = NS_NewSVGLength(getter_AddRefs(length),
                         0.0f);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = NS_NewSVGAnimatedLength(getter_AddRefs(mRy), length);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsSVGAtoms::ry, mRy);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  return rv;
}

//----------------------------------------------------------------------
// nsIDOMNode methods


NS_IMPL_SVG_DOM_CLONENODE(Ellipse)


//----------------------------------------------------------------------
// nsIDOMSVGEllipseElement methods

/* readonly attribute nsIDOMSVGAnimatedLength cx; */
NS_IMETHODIMP nsSVGEllipseElement::GetCx(nsIDOMSVGAnimatedLength * *aCx)
{
  *aCx = mCx;
  NS_IF_ADDREF(*aCx);
  return NS_OK;
}

/* readonly attribute nsIDOMSVGAnimatedLength cy; */
NS_IMETHODIMP nsSVGEllipseElement::GetCy(nsIDOMSVGAnimatedLength * *aCy)
{
  *aCy = mCy;
  NS_IF_ADDREF(*aCy);
  return NS_OK;
}

/* readonly attribute nsIDOMSVGAnimatedLength rx; */
NS_IMETHODIMP nsSVGEllipseElement::GetRx(nsIDOMSVGAnimatedLength * *aRx)
{
  *aRx = mRx;
  NS_IF_ADDREF(*aRx);
  return NS_OK;
}

/* readonly attribute nsIDOMSVGAnimatedLength ry; */
NS_IMETHODIMP nsSVGEllipseElement::GetRy(nsIDOMSVGAnimatedLength * *aRy)
{
  *aRy = mRy;
  NS_IF_ADDREF(*aRy);
  return NS_OK;
}

//----------------------------------------------------------------------
// nsISVGContent methods


void nsSVGEllipseElement::ParentChainChanged()
{
  // set new context information on our length-properties:
  
  nsCOMPtr<nsIDOMSVGSVGElement> dom_elem;
  GetOwnerSVGElement(getter_AddRefs(dom_elem));
  if (!dom_elem) return;

  nsCOMPtr<nsSVGCoordCtxProvider> ctx = do_QueryInterface(dom_elem);
  NS_ASSERTION(ctx, "<svg> element missing interface");

  // cx:
  {
    nsCOMPtr<nsIDOMSVGLength> dom_length;
    mCx->GetBaseVal(getter_AddRefs(dom_length));
    nsCOMPtr<nsISVGLength> length = do_QueryInterface(dom_length);
    NS_ASSERTION(length, "svg length missing interface");

    length->SetContext(nsRefPtr<nsSVGCoordCtx>(ctx->GetContextX()));
  }

  // cy:
  {
    nsCOMPtr<nsIDOMSVGLength> dom_length;
    mCy->GetBaseVal(getter_AddRefs(dom_length));
    nsCOMPtr<nsISVGLength> length = do_QueryInterface(dom_length);
    NS_ASSERTION(length, "svg length missing interface");

    length->SetContext(nsRefPtr<nsSVGCoordCtx>(ctx->GetContextY()));
  }

  // rx:
  {
    nsCOMPtr<nsIDOMSVGLength> dom_length;
    mRx->GetBaseVal(getter_AddRefs(dom_length));
    nsCOMPtr<nsISVGLength> length = do_QueryInterface(dom_length);
    NS_ASSERTION(length, "svg length missing interface");

    length->SetContext(nsRefPtr<nsSVGCoordCtx>(ctx->GetContextX()));
  }
  
  // rx:
  {
    nsCOMPtr<nsIDOMSVGLength> dom_length;
    mRy->GetBaseVal(getter_AddRefs(dom_length));
    nsCOMPtr<nsISVGLength> length = do_QueryInterface(dom_length);
    NS_ASSERTION(length, "svg length missing interface");

    length->SetContext(nsRefPtr<nsSVGCoordCtx>(ctx->GetContextY()));
  }

  // XXX call baseclass version to recurse into children?
}  
