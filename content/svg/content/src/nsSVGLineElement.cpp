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
#include "nsIDOMSVGLineElement.h"
#include "nsCOMPtr.h"
#include "nsISVGSVGElement.h"
#include "nsSVGCoordCtxProvider.h"

typedef nsSVGGraphicElement nsSVGLineElementBase;

class nsSVGLineElement : public nsSVGLineElementBase,
                         public nsIDOMSVGLineElement
{
protected:
  friend nsresult NS_NewSVGLineElement(nsIContent **aResult,
                                         nsINodeInfo *aNodeInfo);
  nsSVGLineElement(nsINodeInfo *aNodeInfo);
  virtual ~nsSVGLineElement();
  nsresult Init();

public:
  // interfaces:

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMSVGLINEELEMENT

  // xxx I wish we could use virtual inheritance
  NS_FORWARD_NSIDOMNODE_NO_CLONENODE(nsSVGLineElementBase::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGLineElementBase::)
  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGLineElementBase::)

  // nsISVGContent specializations:
  virtual void ParentChainChanged();

protected:
  
  nsCOMPtr<nsIDOMSVGAnimatedLength> mX1;
  nsCOMPtr<nsIDOMSVGAnimatedLength> mY1;
  nsCOMPtr<nsIDOMSVGAnimatedLength> mX2;
  nsCOMPtr<nsIDOMSVGAnimatedLength> mY2;

};


NS_IMPL_NS_NEW_SVG_ELEMENT(Line)


//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGLineElement,nsSVGLineElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGLineElement,nsSVGLineElementBase)

NS_INTERFACE_MAP_BEGIN(nsSVGLineElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNode)
  NS_INTERFACE_MAP_ENTRY(nsIDOMElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGLineElement)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(SVGLineElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGLineElementBase)

//----------------------------------------------------------------------
// Implementation

nsSVGLineElement::nsSVGLineElement(nsINodeInfo *aNodeInfo)
  : nsSVGLineElementBase(aNodeInfo)
{

}

nsSVGLineElement::~nsSVGLineElement()
{
}


nsresult
nsSVGLineElement::Init()
{
  nsresult rv = nsSVGLineElementBase::Init();
  NS_ENSURE_SUCCESS(rv,rv);

  // Create mapped properties:

  // DOM property: x1 ,  #IMPLIED attrib: x1
  {
    nsCOMPtr<nsISVGLength> length;
    rv = NS_NewSVGLength(getter_AddRefs(length),
                         0.0f);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = NS_NewSVGAnimatedLength(getter_AddRefs(mX1), length);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsSVGAtoms::x1, mX1);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  // DOM property: y1 ,  #IMPLIED attrib: y1
  {
    nsCOMPtr<nsISVGLength> length;
    rv = NS_NewSVGLength(getter_AddRefs(length),
                         0.0f);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = NS_NewSVGAnimatedLength(getter_AddRefs(mY1), length);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsSVGAtoms::y1, mY1);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  // DOM property: x2 ,  #IMPLIED  attrib: x2
  {
    nsCOMPtr<nsISVGLength> length;
    rv = NS_NewSVGLength(getter_AddRefs(length),
                         0.0f);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = NS_NewSVGAnimatedLength(getter_AddRefs(mX2), length);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsSVGAtoms::x2, mX2);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  // DOM property: y2 ,  #IMPLIED  attrib: y2
  {
    nsCOMPtr<nsISVGLength> length;
    rv = NS_NewSVGLength(getter_AddRefs(length),
                         0.0f);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = NS_NewSVGAnimatedLength(getter_AddRefs(mY2), length);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsSVGAtoms::y2, mY2);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  return rv;
}

//----------------------------------------------------------------------
// nsIDOMNode methods


NS_IMPL_SVG_DOM_CLONENODE(Line)


//----------------------------------------------------------------------
// nsIDOMSVGLineElement methods

/* readonly attribute nsIDOMSVGAnimatedLength cx; */
NS_IMETHODIMP nsSVGLineElement::GetX1(nsIDOMSVGAnimatedLength * *aX1)
{
  *aX1 = mX1;
  NS_IF_ADDREF(*aX1);
  return NS_OK;
}

/* readonly attribute nsIDOMSVGAnimatedLength cy; */
NS_IMETHODIMP nsSVGLineElement::GetY1(nsIDOMSVGAnimatedLength * *aY1)
{
  *aY1 = mY1;
  NS_IF_ADDREF(*aY1);
  return NS_OK;
}

/* readonly attribute nsIDOMSVGAnimatedLength rx; */
NS_IMETHODIMP nsSVGLineElement::GetX2(nsIDOMSVGAnimatedLength * *aX2)
{
  *aX2 = mX2;
  NS_IF_ADDREF(*aX2);
  return NS_OK;
}

/* readonly attribute nsIDOMSVGAnimatedLength ry; */
NS_IMETHODIMP nsSVGLineElement::GetY2(nsIDOMSVGAnimatedLength * *aY2)
{
  *aY2 = mY2;
  NS_IF_ADDREF(*aY2);
  return NS_OK;
}

//----------------------------------------------------------------------
// nsISVGContent methods

void nsSVGLineElement::ParentChainChanged()
{
  // set new context information on our length-properties:
  
  nsCOMPtr<nsIDOMSVGSVGElement> dom_elem;
  GetOwnerSVGElement(getter_AddRefs(dom_elem));
  if (!dom_elem) return;

  nsCOMPtr<nsSVGCoordCtxProvider> ctx = do_QueryInterface(dom_elem);
  NS_ASSERTION(ctx, "<svg> element missing interface");

  // x1:
  {
    nsCOMPtr<nsIDOMSVGLength> dom_length;
    mX1->GetBaseVal(getter_AddRefs(dom_length));
    nsCOMPtr<nsISVGLength> length = do_QueryInterface(dom_length);
    NS_ASSERTION(length, "svg length missing interface");

    length->SetContext(nsRefPtr<nsSVGCoordCtx>(ctx->GetContextX()));
  }

  // y1:
  {
    nsCOMPtr<nsIDOMSVGLength> dom_length;
    mY1->GetBaseVal(getter_AddRefs(dom_length));
    nsCOMPtr<nsISVGLength> length = do_QueryInterface(dom_length);
    NS_ASSERTION(length, "svg length missing interface");

    length->SetContext(nsRefPtr<nsSVGCoordCtx>(ctx->GetContextY()));
  }

  // x2:
  {
    nsCOMPtr<nsIDOMSVGLength> dom_length;
    mX2->GetBaseVal(getter_AddRefs(dom_length));
    nsCOMPtr<nsISVGLength> length = do_QueryInterface(dom_length);
    NS_ASSERTION(length, "svg length missing interface");

    length->SetContext(nsRefPtr<nsSVGCoordCtx>(ctx->GetContextX()));
  }

  // y2:
  {
    nsCOMPtr<nsIDOMSVGLength> dom_length;
    mY2->GetBaseVal(getter_AddRefs(dom_length));
    nsCOMPtr<nsISVGLength> length = do_QueryInterface(dom_length);
    NS_ASSERTION(length, "svg length missing interface");

    length->SetContext(nsRefPtr<nsSVGCoordCtx>(ctx->GetContextY()));
  }
  
  // XXX call baseclass version to recurse into children?
}  
