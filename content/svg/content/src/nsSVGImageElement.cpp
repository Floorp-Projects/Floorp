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
 * Portions created by the Initial Developer are Copyright (C) 2003
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
#include "nsIDOMSVGImageElement.h"
#include "nsIDOMSVGURIReference.h"
#include "nsSVGLength.h"
#include "nsSVGAnimatedLength.h"
#include "nsSVGAnimatedString.h"
#include "nsCOMPtr.h"
#include "nsISVGSVGElement.h"
#include "nsSVGCoordCtxProvider.h"
#include "nsIURI.h"
#include "nsNetUtil.h"

typedef nsSVGGraphicElement nsSVGImageElementBase;

class nsSVGImageElement : public nsSVGImageElementBase,
                          public nsIDOMSVGImageElement,
                          public nsIDOMSVGURIReference
{
protected:
  friend nsresult NS_NewSVGImageElement(nsIContent **aResult,
                                        nsINodeInfo *aNodeInfo);
  nsSVGImageElement(nsINodeInfo *aNodeInfo);
  virtual ~nsSVGImageElement();
  nsresult Init();

public:
  // interfaces:
  
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMSVGIMAGEELEMENT
  NS_DECL_NSIDOMSVGURIREFERENCE

  // xxx I wish we could use virtual inheritance
  NS_FORWARD_NSIDOMNODE_NO_CLONENODE(nsSVGImageElementBase::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGImageElementBase::)
  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGImageElementBase::)

  // nsISVGContent specializations:
  virtual void ParentChainChanged();

protected:
  void GetSrc(nsAString& src);
  
  nsCOMPtr<nsIDOMSVGAnimatedLength> mX;
  nsCOMPtr<nsIDOMSVGAnimatedLength> mY;
  nsCOMPtr<nsIDOMSVGAnimatedLength> mWidth;
  nsCOMPtr<nsIDOMSVGAnimatedLength> mHeight;
  nsCOMPtr<nsIDOMSVGAnimatedString> mHref;

};


NS_IMPL_NS_NEW_SVG_ELEMENT(Image)


//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGImageElement,nsSVGImageElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGImageElement,nsSVGImageElementBase)

NS_INTERFACE_MAP_BEGIN(nsSVGImageElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNode)
  NS_INTERFACE_MAP_ENTRY(nsIDOMElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGImageElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGURIReference)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(SVGImageElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGImageElementBase)

//----------------------------------------------------------------------
// Implementation

nsSVGImageElement::nsSVGImageElement(nsINodeInfo *aNodeInfo)
  : nsSVGImageElementBase(aNodeInfo)
{

}

nsSVGImageElement::~nsSVGImageElement()
{
}

  
nsresult
nsSVGImageElement::Init()
{
  nsresult rv = nsSVGImageElementBase::Init();
  NS_ENSURE_SUCCESS(rv,rv);

  // Create mapped properties:
  

  // nsIDOMSVGImage properties
  
  // DOM property: x ,  #IMPLIED attrib: x
  {
    nsCOMPtr<nsISVGLength> length;
    rv = NS_NewSVGLength(getter_AddRefs(length),
                         0.0f);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = NS_NewSVGAnimatedLength(getter_AddRefs(mX), length);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsSVGAtoms::x, mX);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  // DOM property: y ,  #IMPLIED attrib: y
  {
    nsCOMPtr<nsISVGLength> length;
    rv = NS_NewSVGLength(getter_AddRefs(length),
                         0.0f);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = NS_NewSVGAnimatedLength(getter_AddRefs(mY), length);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsSVGAtoms::y, mY);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  // DOM property: width ,  #REQUIRED  attrib: width
  // XXX: enforce requiredness
  {
    nsCOMPtr<nsISVGLength> length;
    rv = NS_NewSVGLength(getter_AddRefs(length),
                         100.0f);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = NS_NewSVGAnimatedLength(getter_AddRefs(mWidth), length);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsSVGAtoms::width, mWidth);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  // DOM property: height ,  #REQUIRED  attrib: height
  // XXX: enforce requiredness
  {
    nsCOMPtr<nsISVGLength> length;
    rv = NS_NewSVGLength(getter_AddRefs(length),
                         100.0f);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = NS_NewSVGAnimatedLength(getter_AddRefs(mHeight), length);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsSVGAtoms::height, mHeight);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  // nsIDOMSVGURIReference properties

  // DOM property: href , #REQUIRED attrib: xlink:href
  // XXX: enforce requiredness
  {
    rv = NS_NewSVGAnimatedString(getter_AddRefs(mHref));
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsSVGAtoms::href, mHref, kNameSpaceID_XLink);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  return rv;
}

//----------------------------------------------------------------------
// nsIDOMNode methods


NS_IMPL_SVG_DOM_CLONENODE(Image)


//----------------------------------------------------------------------
// nsIDOMSVGImageElement methods:

/* readonly attribute nsIDOMSVGAnimatedLength x; */
NS_IMETHODIMP nsSVGImageElement::GetX(nsIDOMSVGAnimatedLength * *aX)
{
  *aX = mX;
  NS_IF_ADDREF(*aX);
  return NS_OK;
}

/* readonly attribute nsIDOMSVGAnimatedLength y; */
NS_IMETHODIMP nsSVGImageElement::GetY(nsIDOMSVGAnimatedLength * *aY)
{
  *aY = mY;
  NS_IF_ADDREF(*aY);
  return NS_OK;
}

/* readonly attribute nsIDOMSVGAnimatedLength width; */
NS_IMETHODIMP nsSVGImageElement::GetWidth(nsIDOMSVGAnimatedLength * *aWidth)
{
  *aWidth = mWidth;
  NS_IF_ADDREF(*aWidth);
  return NS_OK;
}

/* readonly attribute nsIDOMSVGAnimatedLength height; */
NS_IMETHODIMP nsSVGImageElement::GetHeight(nsIDOMSVGAnimatedLength * *aHeight)
{
  *aHeight = mHeight;
  NS_IF_ADDREF(*aHeight);
  return NS_OK;
}

/* readonly attribute nsIDOMSVGAnimatedPreserveAspectRatio preserveAspectRatio; */
NS_IMETHODIMP
nsSVGImageElement::GetPreserveAspectRatio(nsIDOMSVGAnimatedPreserveAspectRatio
                                          **aPreserveAspectRatio)
{
  NS_NOTYETIMPLEMENTED("write me!");
  return NS_ERROR_UNEXPECTED;
}

//----------------------------------------------------------------------
// nsIDOMSVGURIReference methods:

/* readonly attribute nsIDOMSVGAnimatedString href; */
NS_IMETHODIMP
nsSVGImageElement::GetHref(nsIDOMSVGAnimatedString * *aHref)
{
  *aHref = mHref;
  NS_IF_ADDREF(*aHref);
  return NS_OK;
}


//----------------------------------------------------------------------
// nsISVGContent methods

void nsSVGImageElement::ParentChainChanged()
{
  // set new context information on our length-properties:
  
  nsCOMPtr<nsIDOMSVGSVGElement> dom_elem;
  GetOwnerSVGElement(getter_AddRefs(dom_elem));
  if (!dom_elem) return;

  nsCOMPtr<nsSVGCoordCtxProvider> ctx = do_QueryInterface(dom_elem);
  NS_ASSERTION(ctx, "<svg> element missing interface");

  // x:
  {
    nsCOMPtr<nsIDOMSVGLength> dom_length;
    mX->GetBaseVal(getter_AddRefs(dom_length));
    nsCOMPtr<nsISVGLength> length = do_QueryInterface(dom_length);
    NS_ASSERTION(length, "svg length missing interface");

    length->SetContext(nsRefPtr<nsSVGCoordCtx>(ctx->GetContextX()));
  }

  // y:
  {
    nsCOMPtr<nsIDOMSVGLength> dom_length;
    mY->GetBaseVal(getter_AddRefs(dom_length));
    nsCOMPtr<nsISVGLength> length = do_QueryInterface(dom_length);
    NS_ASSERTION(length, "svg length missing interface");
    
    length->SetContext(nsRefPtr<nsSVGCoordCtx>(ctx->GetContextY()));
  }

  // width:
  {
    nsCOMPtr<nsIDOMSVGLength> dom_length;
    mWidth->GetBaseVal(getter_AddRefs(dom_length));
    nsCOMPtr<nsISVGLength> length = do_QueryInterface(dom_length);
    NS_ASSERTION(length, "svg length missing interface");
    
    length->SetContext(nsRefPtr<nsSVGCoordCtx>(ctx->GetContextX()));
  }

  // height:
  {
    nsCOMPtr<nsIDOMSVGLength> dom_length;
    mHeight->GetBaseVal(getter_AddRefs(dom_length));
    nsCOMPtr<nsISVGLength> length = do_QueryInterface(dom_length);
    NS_ASSERTION(length, "svg length missing interface");
    
    length->SetContext(nsRefPtr<nsSVGCoordCtx>(ctx->GetContextY()));
  }
}  


//----------------------------------------------------------------------

void nsSVGImageElement::GetSrc(nsAString& src)
{
  // resolve href attribute

  nsCOMPtr<nsIURI> baseURI = GetBaseURI();

  nsAutoString relURIStr;
  mHref->GetAnimVal(relURIStr);
  relURIStr.Trim(" \t\n\r");

  if (baseURI && !relURIStr.IsEmpty()) 
    NS_MakeAbsoluteURI(src, relURIStr, baseURI);
  else
    src = relURIStr;
}
