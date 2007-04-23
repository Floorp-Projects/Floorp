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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
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

#include "nsXMLElement.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsIRefreshURI.h"
#include "nsPresContext.h"
#include "nsContentErrors.h"
#include "nsIDocument.h"

nsresult
NS_NewXMLElement(nsIContent** aInstancePtrResult, nsINodeInfo *aNodeInfo)
{
  nsXMLElement* it = new nsXMLElement(aNodeInfo);
  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  NS_ADDREF(*aInstancePtrResult = it);

  return NS_OK;
}

nsXMLElement::nsXMLElement(nsINodeInfo *aNodeInfo)
  : nsGenericElement(aNodeInfo)
{
}


// QueryInterface implementation for nsXMLElement
NS_IMETHODIMP 
nsXMLElement::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  NS_ENSURE_ARG_POINTER(aInstancePtr);
  *aInstancePtr = nsnull;

  nsresult rv = nsGenericElement::QueryInterface(aIID, aInstancePtr);

  if (NS_SUCCEEDED(rv))
    return rv;

  nsISupports *inst = nsnull;

  if (aIID.Equals(NS_GET_IID(nsIDOMNode))) {
    inst = NS_STATIC_CAST(nsIDOMNode *, this);
  } else if (aIID.Equals(NS_GET_IID(nsIDOMElement))) {
    inst = NS_STATIC_CAST(nsIDOMElement *, this);
  } else if (aIID.Equals(NS_GET_IID(nsIClassInfo))) {
    inst = NS_GetDOMClassInfoInstance(eDOMClassInfo_Element_id);
    NS_ENSURE_TRUE(inst, NS_ERROR_OUT_OF_MEMORY);
  } else {
    return PostQueryInterface(aIID, aInstancePtr);
  }

  NS_ADDREF(inst);

  *aInstancePtr = inst;

  return NS_OK;
}


NS_IMPL_ADDREF_INHERITED(nsXMLElement, nsGenericElement)
NS_IMPL_RELEASE_INHERITED(nsXMLElement, nsGenericElement)


static nsresult
DocShellToPresContext(nsIDocShell *aShell, nsPresContext **aPresContext)
{
  *aPresContext = nsnull;

  nsresult rv;
  nsCOMPtr<nsIDocShell> ds = do_QueryInterface(aShell,&rv);
  if (NS_FAILED(rv))
    return rv;

  return ds->GetPresContext(aPresContext);
}

nsresult
nsXMLElement::PreHandleEvent(nsEventChainPreVisitor& aVisitor)
{
  nsresult rv = nsGenericElement::PreHandleEvent(aVisitor);
  NS_ENSURE_SUCCESS(rv, rv);

  return PreHandleEventForLinks(aVisitor);
}

nsresult
nsXMLElement::PostHandleEvent(nsEventChainPostVisitor& aVisitor)
{
  return PostHandleEventForLinks(aVisitor);
}

nsresult
nsXMLElement::MaybeTriggerAutoLink(nsIDocShell *aShell)
{
  NS_ENSURE_ARG_POINTER(aShell);

  // We require an xlink:href, xlink:type="simple" and xlink:actuate="onLoad"
  // XXX: as of XLink 1.1, elements will be links even without xlink:type set
  if (!HasAttr(kNameSpaceID_XLink, nsGkAtoms::href) ||
      !AttrValueIs(kNameSpaceID_XLink, nsGkAtoms::type,
                   nsGkAtoms::simple, eCaseMatters) ||
      !AttrValueIs(kNameSpaceID_XLink, nsGkAtoms::actuate,
                   nsGkAtoms::onLoad, eCaseMatters)) {
    return NS_OK;
  }

  // Disable in Mail/News for now. We may want a pref to control
  // this at some point.
  nsCOMPtr<nsIDocShellTreeItem> docShellItem = do_QueryInterface(aShell);
  if (docShellItem) {
    nsCOMPtr<nsIDocShellTreeItem> rootItem;
    docShellItem->GetRootTreeItem(getter_AddRefs(rootItem));
    nsCOMPtr<nsIDocShell> docshell = do_QueryInterface(rootItem);
    if (docshell) {
      PRUint32 appType;
      if (NS_SUCCEEDED(docshell->GetAppType(&appType)) &&
          appType == nsIDocShell::APP_TYPE_MAIL) {
        return NS_OK;
      }
    }
  }

  // Get absolute URI
  nsCOMPtr<nsIURI> absURI;
  nsAutoString href;
  GetAttr(kNameSpaceID_XLink, nsGkAtoms::href, href);
  nsCOMPtr<nsIURI> baseURI = GetBaseURI();
  nsContentUtils::NewURIWithDocumentCharset(getter_AddRefs(absURI), href,
                                            GetOwnerDoc(), baseURI);
  if (!absURI) {
    return NS_OK;
  }

  // Check that the link's URI isn't the same as its document's URI, or else
  // we'll recursively load the document forever (possibly in new windows!)
  PRBool isDocURI;
  absURI->Equals(GetOwnerDoc()->GetDocumentURI(), &isDocURI);
  if (isDocURI) {
    return NS_OK;
  }

  // Get target
  nsAutoString target;
  nsresult special_rv = GetLinkTargetAndAutoType(target);
  // Ignore this link if xlink:show has a value we don't implement
  if (NS_FAILED(special_rv)) return NS_OK;

  // Attempt to load the URI
  nsCOMPtr<nsPresContext> pc;
  nsresult rv = DocShellToPresContext(aShell, getter_AddRefs(pc));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = TriggerLink(pc, absURI, target, PR_TRUE, PR_FALSE);
  NS_ENSURE_SUCCESS(rv, rv);

  return special_rv; // return GetLinkTargetAndAutoType's special rv!
}

PRBool
nsXMLElement::IsFocusable(PRInt32 *aTabIndex)
{
  nsCOMPtr<nsIURI> absURI;
  if (IsLink(getter_AddRefs(absURI))) {
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
nsXMLElement::IsLink(nsIURI** aURI) const
{
  NS_PRECONDITION(aURI, "Must provide aURI out param");

  // To be an XLink for styling and interaction purposes, we require:
  //
  //   xlink:href          - must be set
  //   xlink:type          - must be set to "simple"
  //     xlink:_moz_target - must be set, OR
  //     xlink:show        - must be unset or set to "", "new" or "replace"
  //   xlink:actuate       - must be unset or set to "" or "onRequest"
  //
  // For any other values, we're either not a *clickable* XLink, or the end
  // result is poorly specified. Either way, we return PR_FALSE.

  static nsIContent::AttrValuesArray sShowVals[] =
    { &nsGkAtoms::_empty, &nsGkAtoms::_new, &nsGkAtoms::replace, nsnull };

  static nsIContent::AttrValuesArray sActuateVals[] =
    { &nsGkAtoms::_empty, &nsGkAtoms::onRequest, nsnull };

  // Optimization: check for href first for early return
  const nsAttrValue* href = mAttrsAndChildren.GetAttr(nsGkAtoms::href,
                                                      kNameSpaceID_XLink);
  if (href &&
      AttrValueIs(kNameSpaceID_XLink, nsGkAtoms::type,
                  nsGkAtoms::simple, eCaseMatters) &&
      (HasAttr(kNameSpaceID_XLink, nsGkAtoms::_moz_target) ||
       FindAttrValueIn(kNameSpaceID_XLink, nsGkAtoms::show,
                       sShowVals, eCaseMatters) !=
                       nsIContent::ATTR_VALUE_NO_MATCH) &&
      FindAttrValueIn(kNameSpaceID_XLink, nsGkAtoms::actuate,
                      sActuateVals, eCaseMatters) !=
                      nsIContent::ATTR_VALUE_NO_MATCH) {
    // Get absolute URI
    nsCOMPtr<nsIURI> baseURI = GetBaseURI();
    nsContentUtils::NewURIWithDocumentCharset(aURI, href->GetStringValue(),
                                              GetOwnerDoc(), baseURI);
    return !!*aURI; // must promise out param is non-null if we return true
  }

  *aURI = nsnull;
  return PR_FALSE;
}

void
nsXMLElement::GetLinkTarget(nsAString& aTarget)
{
  GetLinkTargetAndAutoType(aTarget);
}

nsresult
nsXMLElement::GetLinkTargetAndAutoType(nsAString& aTarget)
{
  // Mozilla extension xlink:_moz_target overrides xlink:show
  if (GetAttr(kNameSpaceID_XLink, nsGkAtoms::_moz_target, aTarget)) {
    return aTarget.IsEmpty() ? NS_XML_AUTOLINK_REPLACE : NS_OK;
  }

  // Try xlink:show if no xlink:_moz_target
  GetAttr(kNameSpaceID_XLink, nsGkAtoms::show, aTarget);
  if (aTarget.IsEmpty()) {
    return NS_XML_AUTOLINK_UNDEFINED;
  }
  if (aTarget.EqualsLiteral("new")) {
    aTarget.AssignLiteral("_blank");
    return NS_XML_AUTOLINK_NEW;
  }
  if (aTarget.EqualsLiteral("replace")) {
    aTarget.Truncate();
    return NS_XML_AUTOLINK_REPLACE;
  }
  // xlink:show="embed" isn't handled by this code path

  aTarget.Truncate();
  return NS_ERROR_FAILURE;
}


NS_IMPL_ELEMENT_CLONE(nsXMLElement)
