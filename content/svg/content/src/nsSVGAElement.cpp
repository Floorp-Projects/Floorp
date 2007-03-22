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
 * The Original Code is Mozilla SVG Project code.
 *
 * The Initial Developer of the Original Code is Jonathan Watt.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jonathan Watt <jwatt@jwatt.org> (original author)
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
#include "nsIDOMSVGAElement.h"
#include "nsIDOMSVGURIReference.h"
#include "nsILink.h"
#include "nsSVGAnimatedString.h"
#include "nsCOMPtr.h"
#include "nsGkAtoms.h"

typedef nsSVGGraphicElement nsSVGAElementBase;

class nsSVGAElement : public nsSVGAElementBase,
                      public nsIDOMSVGAElement,
                      public nsIDOMSVGURIReference,
                      public nsILink
{
protected:
  friend nsresult NS_NewSVGAElement(nsIContent **aResult,
                                    nsINodeInfo *aNodeInfo);
  nsSVGAElement(nsINodeInfo *aNodeInfo);
  nsresult Init();

public:
  // interfaces:

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMSVGAELEMENT
  NS_DECL_NSIDOMSVGURIREFERENCE

  // XXX: I wish we could use virtual inheritance
  NS_FORWARD_NSIDOMNODE(nsSVGAElementBase::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGAElementBase::)
  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGAElementBase::)

  // nsINode interface methods
  virtual nsresult PostHandleEvent(nsEventChainPostVisitor& aVisitor);
  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  // nsILink
  NS_IMETHOD GetLinkState(nsLinkState &aState);
  NS_IMETHOD SetLinkState(nsLinkState aState);
  NS_IMETHOD GetHrefURI(nsIURI** aURI);
  NS_IMETHOD LinkAdded() { return NS_OK; }
  NS_IMETHOD LinkRemoved() { return NS_OK; }

  // nsIContent
  virtual PRBool IsFocusable(PRInt32 *aTabIndex = nsnull);
  virtual PRBool IsLink(nsIURI** aURI) const;

protected:

  nsCOMPtr<nsIDOMSVGAnimatedString> mHref;
  nsCOMPtr<nsIDOMSVGAnimatedString> mTarget;

  // The cached visited state (for the implementation of nsILink)
  nsLinkState mLinkState;
};

NS_IMPL_NS_NEW_SVG_ELEMENT(A)


//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGAElement, nsSVGAElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGAElement, nsSVGAElementBase)

NS_INTERFACE_MAP_BEGIN(nsSVGAElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNode)
  NS_INTERFACE_MAP_ENTRY(nsIDOMElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGAElement)
  NS_INTERFACE_MAP_ENTRY(nsILink)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(SVGAElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGAElementBase)


//----------------------------------------------------------------------
// Implementation

nsSVGAElement::nsSVGAElement(nsINodeInfo *aNodeInfo)
  : nsSVGAElementBase(aNodeInfo),
    mLinkState(eLinkState_Unknown)
{
}

nsresult
nsSVGAElement::Init()
{
  nsresult rv = nsSVGAElementBase::Init();
  NS_ENSURE_SUCCESS(rv,rv);

  // nsIDOMSVGURIReference properties

  // DOM property: href , #REQUIRED attrib: xlink:href
  // XXX: enforce requiredness
  {
    rv = NS_NewSVGAnimatedString(getter_AddRefs(mHref));
    NS_ENSURE_SUCCESS(rv, rv);
    rv = AddMappedSVGValue(nsGkAtoms::href, mHref, kNameSpaceID_XLink);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // nsIDOMSVGURIReference properties

  // DOM property: target , #IMPLIED attrib: target
  {
    rv = NS_NewSVGAnimatedString(getter_AddRefs(mTarget));
    NS_ENSURE_SUCCESS(rv, rv);
    rv = AddMappedSVGValue(nsGkAtoms::target, mTarget);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}


//----------------------------------------------------------------------
// nsIDOMSVGURIReference methods

/* readonly attribute nsIDOMSVGAnimatedString href; */
NS_IMETHODIMP
nsSVGAElement::GetHref(nsIDOMSVGAnimatedString * *aHref)
{
  *aHref = mHref;
  NS_IF_ADDREF(*aHref);
  return NS_OK;
}


//----------------------------------------------------------------------
// nsINode methods

nsresult
nsSVGAElement::PostHandleEvent(nsEventChainPostVisitor& aVisitor)
{
  return PostHandleEventForLinks(aVisitor);
}

NS_IMPL_ELEMENT_CLONE_WITH_INIT(nsSVGAElement)


//----------------------------------------------------------------------
// nsIDOMSVGAElement methods

/* readonly attribute nsIDOMSVGAnimatedString target; */
NS_IMETHODIMP
nsSVGAElement::GetTarget(nsIDOMSVGAnimatedString * *aTarget)
{
  *aTarget = mTarget;
  NS_IF_ADDREF(*aTarget);
  return NS_OK;
}


//----------------------------------------------------------------------
// nsILink methods

NS_IMETHODIMP
nsSVGAElement::GetLinkState(nsLinkState &aState)
{
  aState = mLinkState;
  return NS_OK;
}

NS_IMETHODIMP
nsSVGAElement::SetLinkState(nsLinkState aState)
{
  mLinkState = aState;
  return NS_OK;
}

NS_IMETHODIMP
nsSVGAElement::GetHrefURI(nsIURI** aURI)
{
  *aURI = nsnull;
  return NS_OK; // XXX GetHrefURIForAnchors(aURI);
}


//----------------------------------------------------------------------
// nsIContent methods

PRBool
nsSVGAElement::IsFocusable(PRInt32 *aTabIndex)
{
  nsCOMPtr<nsIURI> uri;
  if (IsLink(getter_AddRefs(uri))) {
    if (aTabIndex) {
      *aTabIndex = ((sTabFocusModel & eTabFocus_linksMask) == 0 ? -1 : 0);
    }
    return PR_TRUE;
  }

  if (aTabIndex) {
    *aTabIndex = -1;
  }

  return PR_FALSE;
}

PRBool
nsSVGAElement::IsLink(nsIURI** aURI) const
{
  // To be a clickable XLink for styling and interaction purposes, we require:
  //
  //   xlink:href    - must be set
  //   xlink:type    - must be unset or set to "" or set to "simple"
  //   xlink:show    - must be unset or set to "", "new" or "replace"
  //   xlink:actuate - must be unset or set to "" or "onRequest"
  //
  // For any other values, we're either not a *clickable* XLink, or the end
  // result is poorly specified. Either way, we return PR_FALSE.

  static nsIContent::AttrValuesArray sTypeVals[] =
    { &nsGkAtoms::_empty, &nsGkAtoms::simple, nsnull };

  static nsIContent::AttrValuesArray sShowVals[] =
    { &nsGkAtoms::_empty, &nsGkAtoms::_new, &nsGkAtoms::replace, nsnull };

  static nsIContent::AttrValuesArray sActuateVals[] =
    { &nsGkAtoms::_empty, &nsGkAtoms::onRequest, nsnull };

  // Optimization: check for href first for early return
  const nsAttrValue* href = mAttrsAndChildren.GetAttr(nsGkAtoms::href,
                                                      kNameSpaceID_XLink);
  if (href &&
      FindAttrValueIn(kNameSpaceID_XLink, nsGkAtoms::type,
                      sTypeVals, eCaseMatters) !=
                      nsIContent::ATTR_VALUE_NO_MATCH &&
      FindAttrValueIn(kNameSpaceID_XLink, nsGkAtoms::show,
                      sShowVals, eCaseMatters) !=
                      nsIContent::ATTR_VALUE_NO_MATCH &&
      FindAttrValueIn(kNameSpaceID_XLink, nsGkAtoms::actuate,
                      sActuateVals, eCaseMatters) !=
                      nsIContent::ATTR_VALUE_NO_MATCH) {
    nsCOMPtr<nsIURI> baseURI = GetBaseURI();
    // Get absolute URI
    // XXX: should really be using href->GetStringValue(), but nsSVGElement::
    // ParseAttribute has set the nsAttrValue type to eSVGValue, so we need
    // to use the more expensive ToString (generates, rather than fetches).
    nsAutoString hrefStr;
    href->ToString(hrefStr);
    nsContentUtils::NewURIWithDocumentCharset(aURI, hrefStr,
                                              GetOwnerDoc(), baseURI);
    // must promise out param is non-null if we return true
    return !!*aURI;
  }

  *aURI = nsnull;
  return PR_FALSE;
}

