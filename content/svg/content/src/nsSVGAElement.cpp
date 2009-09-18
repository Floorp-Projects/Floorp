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

#include "nsSVGAElement.h"
#include "nsSVGGraphicElement.h"
#include "nsIDOMSVGAElement.h"
#include "nsIDOMSVGURIReference.h"
#include "nsILink.h"
#include "nsSVGString.h"
#include "nsCOMPtr.h"
#include "nsGkAtoms.h"


nsSVGElement::StringInfo nsSVGAElement::sStringInfo[2] =
{
  { &nsGkAtoms::href, kNameSpaceID_XLink },
  { &nsGkAtoms::target, kNameSpaceID_None }
};

NS_IMPL_NS_NEW_SVG_ELEMENT(A)


//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGAElement, nsSVGAElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGAElement, nsSVGAElementBase)

NS_INTERFACE_TABLE_HEAD(nsSVGAElement)
  NS_NODE_INTERFACE_TABLE5(nsSVGAElement, nsIDOMNode, nsIDOMElement,
                           nsIDOMSVGElement, nsIDOMSVGAElement, nsILink)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(SVGAElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGAElementBase)


//----------------------------------------------------------------------
// Implementation

nsSVGAElement::nsSVGAElement(nsINodeInfo *aNodeInfo)
  : nsSVGAElementBase(aNodeInfo),
    mLinkState(eLinkState_Unknown)
{
}

//----------------------------------------------------------------------
// nsIDOMSVGURIReference methods

/* readonly attribute nsIDOMSVGAnimatedString href; */
NS_IMETHODIMP
nsSVGAElement::GetHref(nsIDOMSVGAnimatedString * *aHref)
{
  return mStringAttributes[HREF].ToDOMAnimatedString(aHref, this);
}


//----------------------------------------------------------------------
// nsINode methods

nsresult
nsSVGAElement::PreHandleEvent(nsEventChainPreVisitor& aVisitor)
{
  nsresult rv = nsGenericElement::PreHandleEvent(aVisitor);
  NS_ENSURE_SUCCESS(rv, rv);

  return PreHandleEventForLinks(aVisitor);
}

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
  return mStringAttributes[TARGET].ToDOMAnimatedString(aTarget, this);
}


//----------------------------------------------------------------------
// nsIContent methods

void
nsSVGAElement::UnbindFromTree(PRBool aDeep, PRBool aNullParent)
{
  if (HasAttr(kNameSpaceID_XLink, nsGkAtoms::href)) {
    nsIDocument *doc = GetCurrentDoc();
    if (doc) {
      doc->ForgetLink(this);
    }
  }

  nsSVGAElementBase::UnbindFromTree(aDeep, aNullParent);
}

nsresult
nsSVGAElement::SetAttr(PRInt32 aNamespaceID, nsIAtom *aName,
                       nsIAtom *aPrefix, const nsAString& aValue,
                       PRBool aNotify)
{
  if (kNameSpaceID_XLink == aNamespaceID && nsGkAtoms::href == aName) {
    // XLink URI(s) might be changing. Drop the link from the map. If it
    // is still style relevant it will be re-added by
    // nsStyleUtil::IsLink. Make sure to keep the style system
    // consistent so this remains true! In particular if the style system
    // were to get smarter and not restyling an XLink element if the href
    // doesn't change in a "significant" way, we'd need to do the same
    // significance check here.
    nsIDocument *doc = GetCurrentDoc();
    if (doc) {
      doc->ForgetLink(this);
    }
  }

  return nsSVGAElementBase::SetAttr(aNamespaceID, aName, aPrefix, aValue,
                                    aNotify);
}

nsresult
nsSVGAElement::UnsetAttr(PRInt32 aNamespaceID, nsIAtom *aName,
                         PRBool aNotify)
{
  if (kNameSpaceID_XLink == aNamespaceID && nsGkAtoms::href == aName) {
    nsIDocument *doc = GetCurrentDoc();
    if (doc) {
      // XLink URI might be changing.
      doc->ForgetLink(this);
    }
  }

  return nsSVGAElementBase::UnsetAttr(aNamespaceID, aName, aNotify);
}

nsLinkState
nsSVGAElement::GetLinkState() const
{
  return mLinkState;
}

void
nsSVGAElement::SetLinkState(nsLinkState aState)
{
  mLinkState = aState;
}

already_AddRefed<nsIURI>
nsSVGAElement::GetHrefURI() const
{
  return nsnull; // XXX GetHrefURIForAnchors();
}


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
    nsAutoString str;
    mStringAttributes[HREF].GetAnimValue(str, this);
    nsContentUtils::NewURIWithDocumentCharset(aURI, str,
                                              GetOwnerDoc(), baseURI);
    // must promise out param is non-null if we return true
    return !!*aURI;
  }

  *aURI = nsnull;
  return PR_FALSE;
}

void
nsSVGAElement::GetLinkTarget(nsAString& aTarget)
{
  mStringAttributes[TARGET].GetAnimValue(aTarget, this);
  if (aTarget.IsEmpty()) {

    static nsIContent::AttrValuesArray sShowVals[] =
      { &nsGkAtoms::_new, &nsGkAtoms::replace, nsnull };

    switch (FindAttrValueIn(kNameSpaceID_XLink, nsGkAtoms::show,
                            sShowVals, eCaseMatters)) {
    case 0:
      aTarget.AssignLiteral("_blank");
      return;
    case 1:
      return;
    }
    nsIDocument* ownerDoc = GetOwnerDoc();
    if (ownerDoc) {
      ownerDoc->GetBaseTarget(aTarget);
    }
  }
}

//----------------------------------------------------------------------
// nsSVGElement methods

nsSVGElement::StringAttributesInfo
nsSVGAElement::GetStringInfo()
{
  return StringAttributesInfo(mStringAttributes, sStringInfo,
                              NS_ARRAY_LENGTH(sStringInfo));
}

