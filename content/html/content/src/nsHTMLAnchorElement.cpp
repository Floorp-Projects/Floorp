/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set tw=80 expandtab softtabstop=2 ts=2 sw=2: */
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
 *   Daniel Glazman <glazman@netscape.com>
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

#include "nsCOMPtr.h"
#include "nsReadableUtils.h"
#include "nsUnicharUtils.h"
#include "nsIDOMHTMLAnchorElement.h"
#include "nsIDOMNSHTMLAnchorElement2.h"
#include "nsGenericHTMLElement.h"
#include "nsILink.h"
#include "nsGkAtoms.h"
#include "nsIPresShell.h"
#include "nsIDocument.h"
#include "nsPresContext.h"
#include "nsIEventStateManager.h"

// For GetText().
#include "nsIContentIterator.h"
#include "nsIDOMText.h"

#include "nsHTMLDNSPrefetch.h"

#include "Link.h"
using namespace mozilla::dom;

nsresult NS_NewPreContentIterator(nsIContentIterator** aInstancePtrResult);

class nsHTMLAnchorElement : public nsGenericHTMLElement,
                            public nsIDOMHTMLAnchorElement,
                            public nsIDOMNSHTMLAnchorElement2,
                            public nsILink,
                            public Link
{
public:
  using nsGenericElement::GetText;

  nsHTMLAnchorElement(nsINodeInfo *aNodeInfo);
  virtual ~nsHTMLAnchorElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE(nsGenericHTMLElement::)

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT(nsGenericHTMLElement::)

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT(nsGenericHTMLElement::)

  // nsIDOMHTMLAnchorElement
  NS_DECL_NSIDOMHTMLANCHORELEMENT  

  // nsIDOMNSHTMLAnchorElement
  NS_DECL_NSIDOMNSHTMLANCHORELEMENT

  // nsIDOMNSHTMLAnchorElement2
  NS_DECL_NSIDOMNSHTMLANCHORELEMENT2

  // nsILink
  NS_IMETHOD LinkAdded() { return NS_OK; }
  NS_IMETHOD LinkRemoved() { return NS_OK; }

  // override from nsGenericHTMLElement
  NS_IMETHOD GetDraggable(PRBool* aDraggable);

  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              PRBool aCompileEventHandlers);
  virtual void UnbindFromTree(PRBool aDeep = PR_TRUE,
                              PRBool aNullParent = PR_TRUE);
  virtual PRBool IsHTMLFocusable(PRBool *aIsFocusable, PRInt32 *aTabIndex);

  virtual nsresult PreHandleEvent(nsEventChainPreVisitor& aVisitor);
  virtual nsresult PostHandleEvent(nsEventChainPostVisitor& aVisitor);
  virtual PRBool IsLink(nsIURI** aURI) const;
  virtual void GetLinkTarget(nsAString& aTarget);
  virtual nsLinkState GetLinkState() const;
  virtual void SetLinkState(nsLinkState aState);
  virtual already_AddRefed<nsIURI> GetHrefURI() const;

  nsresult SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                   const nsAString& aValue, PRBool aNotify)
  {
    return SetAttr(aNameSpaceID, aName, nsnull, aValue, aNotify);
  }
  virtual nsresult SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                           nsIAtom* aPrefix, const nsAString& aValue,
                           PRBool aNotify);
  virtual nsresult UnsetAttr(PRInt32 aNameSpaceID, nsIAtom* aAttribute,
                             PRBool aNotify);
  virtual PRBool ParseAttribute(PRInt32 aNamespaceID,
                                nsIAtom* aAttribute,
                                const nsAString& aValue,
                                nsAttrValue& aResult);

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  virtual void DropCachedHref();

protected:
  void ResetLinkCacheState();
};


NS_IMPL_NS_NEW_HTML_ELEMENT(Anchor)

nsHTMLAnchorElement::nsHTMLAnchorElement(nsINodeInfo *aNodeInfo)
  : nsGenericHTMLElement(aNodeInfo)
{
}

nsHTMLAnchorElement::~nsHTMLAnchorElement()
{
}


NS_IMPL_ADDREF_INHERITED(nsHTMLAnchorElement, nsGenericElement) 
NS_IMPL_RELEASE_INHERITED(nsHTMLAnchorElement, nsGenericElement) 


// QueryInterface implementation for nsHTMLAnchorElement
NS_INTERFACE_TABLE_HEAD(nsHTMLAnchorElement)
  NS_HTML_CONTENT_INTERFACE_TABLE4(nsHTMLAnchorElement,
                                   nsIDOMHTMLAnchorElement,
                                   nsIDOMNSHTMLAnchorElement,
                                   nsIDOMNSHTMLAnchorElement2,
                                   nsILink)
  NS_HTML_CONTENT_INTERFACE_TABLE_TO_MAP_SEGUE(nsHTMLAnchorElement,
                                               nsGenericHTMLElement)
NS_HTML_CONTENT_INTERFACE_TABLE_TAIL_CLASSINFO(HTMLAnchorElement)


NS_IMPL_ELEMENT_CLONE(nsHTMLAnchorElement)


NS_IMPL_STRING_ATTR(nsHTMLAnchorElement, Charset, charset)
NS_IMPL_STRING_ATTR(nsHTMLAnchorElement, Coords, coords)
NS_IMPL_URI_ATTR(nsHTMLAnchorElement, Href, href)
NS_IMPL_STRING_ATTR(nsHTMLAnchorElement, Hreflang, hreflang)
NS_IMPL_STRING_ATTR(nsHTMLAnchorElement, Name, name)
NS_IMPL_STRING_ATTR(nsHTMLAnchorElement, Rel, rel)
NS_IMPL_STRING_ATTR(nsHTMLAnchorElement, Rev, rev)
NS_IMPL_STRING_ATTR(nsHTMLAnchorElement, Shape, shape)
NS_IMPL_INT_ATTR_DEFAULT_VALUE(nsHTMLAnchorElement, TabIndex, tabindex, 0)
NS_IMPL_STRING_ATTR(nsHTMLAnchorElement, Type, type)
NS_IMPL_STRING_ATTR(nsHTMLAnchorElement, AccessKey, accesskey)

NS_IMETHODIMP
nsHTMLAnchorElement::GetDraggable(PRBool* aDraggable)
{
  // links can be dragged as long as there is an href and the
  // draggable attribute isn't false
  if (HasAttr(kNameSpaceID_None, nsGkAtoms::href)) {
    *aDraggable = !AttrValueIs(kNameSpaceID_None, nsGkAtoms::draggable,
                               nsGkAtoms::_false, eIgnoreCase);
    return NS_OK;
  }

  // no href, so just use the same behavior as other elements
  return nsGenericHTMLElement::GetDraggable(aDraggable);
}

nsresult
nsHTMLAnchorElement::BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                                nsIContent* aBindingParent,
                                PRBool aCompileEventHandlers)
{
  nsresult rv = nsGenericHTMLElement::BindToTree(aDocument, aParent,
                                                 aBindingParent,
                                                 aCompileEventHandlers);
  NS_ENSURE_SUCCESS(rv, rv);

  if (aDocument) {
    RegUnRegAccessKey(PR_TRUE);
  }

  // Prefetch links
  if (aDocument && nsHTMLDNSPrefetch::IsAllowed(GetOwnerDoc())) {
    nsHTMLDNSPrefetch::PrefetchLow(this);
  }
  return rv;
}

void
nsHTMLAnchorElement::UnbindFromTree(PRBool aDeep, PRBool aNullParent)
{
  if (IsInDoc()) {
    RegUnRegAccessKey(PR_FALSE);
    ResetLinkCacheState();
  }
    
  nsGenericHTMLElement::UnbindFromTree(aDeep, aNullParent);
}

NS_IMETHODIMP
nsHTMLAnchorElement::Blur()
{
  return nsGenericHTMLElement::Blur();
}

NS_IMETHODIMP
nsHTMLAnchorElement::Focus()
{
  return nsGenericHTMLElement::Focus();
}

PRBool
nsHTMLAnchorElement::IsHTMLFocusable(PRBool *aIsFocusable, PRInt32 *aTabIndex)
{
  if (nsGenericHTMLElement::IsHTMLFocusable(aIsFocusable, aTabIndex)) {
    return PR_TRUE;
  }

  // cannot focus links if there is no link handler
  nsIDocument* doc = GetCurrentDoc();
  if (doc) {
    nsIPresShell* presShell = doc->GetPrimaryShell();
    if (presShell) {
      nsPresContext* presContext = presShell->GetPresContext();
      if (presContext && !presContext->GetLinkHandler()) {
        *aIsFocusable = PR_FALSE;
        return PR_FALSE;
      }
    }
  }

  if (IsEditable()) {
    if (aTabIndex) {
      *aTabIndex = -1;
    }

    *aIsFocusable = PR_FALSE;

    return PR_TRUE;
  }

  if (!HasAttr(kNameSpaceID_None, nsGkAtoms::tabindex)) {
    // check whether we're actually a link
    nsCOMPtr<nsIURI> absURI;
    if (!IsLink(getter_AddRefs(absURI))) {
      // Not tabbable or focusable without href (bug 17605), unless
      // forced to be via presence of nonnegative tabindex attribute
      if (aTabIndex) {
        *aTabIndex = -1;
      }

      *aIsFocusable = PR_FALSE;

      return PR_FALSE;
    }
  }

  if (aTabIndex && (sTabFocusModel & eTabFocus_linksMask) == 0) {
    *aTabIndex = -1;
  }

  *aIsFocusable = PR_TRUE;

  return PR_FALSE;
}

nsresult
nsHTMLAnchorElement::PreHandleEvent(nsEventChainPreVisitor& aVisitor)
{
  return PreHandleEventForAnchors(aVisitor);
}

nsresult
nsHTMLAnchorElement::PostHandleEvent(nsEventChainPostVisitor& aVisitor)
{
  return PostHandleEventForAnchors(aVisitor);
}

PRBool
nsHTMLAnchorElement::IsLink(nsIURI** aURI) const
{
  return IsHTMLLink(aURI);
}

void
nsHTMLAnchorElement::GetLinkTarget(nsAString& aTarget)
{
  GetAttr(kNameSpaceID_None, nsGkAtoms::target, aTarget);
  if (aTarget.IsEmpty()) {
    GetBaseTarget(aTarget);
  }
}

NS_IMETHODIMP
nsHTMLAnchorElement::GetTarget(nsAString& aValue)
{
  if (!GetAttr(kNameSpaceID_None, nsGkAtoms::target, aValue)) {
    GetBaseTarget(aValue);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLAnchorElement::SetTarget(const nsAString& aValue)
{
  return SetAttr(kNameSpaceID_None, nsGkAtoms::target, aValue, PR_TRUE);
}

#define IMPL_URI_PART(_part)                                 \
  NS_IMETHODIMP                                              \
  nsHTMLAnchorElement::Get##_part(nsAString& a##_part)       \
  {                                                          \
    return Get##_part##FromHrefURI(a##_part);                \
  }                                                          \
  NS_IMETHODIMP                                              \
  nsHTMLAnchorElement::Set##_part(const nsAString& a##_part) \
  {                                                          \
    return Set##_part##InHrefURI(a##_part);                  \
  }

IMPL_URI_PART(Protocol)
IMPL_URI_PART(Host)
IMPL_URI_PART(Hostname)
IMPL_URI_PART(Pathname)
IMPL_URI_PART(Search)
IMPL_URI_PART(Port)
IMPL_URI_PART(Hash)

#undef IMPL_URI_PART

NS_IMETHODIMP    
nsHTMLAnchorElement::GetText(nsAString& aText)
{
  aText.Truncate();

  // Since this is a Netscape 4 proprietary attribute, we have to implement
  // the same behavior. Basically it is returning the last text node of
  // of the anchor. Returns an empty string if there is no text node.
  // The nsIContentIterator does exactly what we want, if we start the 
  // iteration from the end.
  nsCOMPtr<nsIContentIterator> iter;
  nsresult rv = NS_NewPreContentIterator(getter_AddRefs(iter));
  NS_ENSURE_SUCCESS(rv, rv);

  // Initialize the content iterator with the children of the anchor
  iter->Init(this);

  // Last() positions the iterator to the last child of the anchor,
  // starting at the deepest level of children, just like NS4 does.
  iter->Last();

  while (!iter->IsDone()) {
    nsCOMPtr<nsIDOMText> textNode(do_QueryInterface(iter->GetCurrentNode()));
    if(textNode) {
      // The current node is a text node. Get its value and break the loop.
      textNode->GetData(aText);
      break;
    }

    iter->Prev();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLAnchorElement::ToString(nsAString& aSource)
{
  return GetHref(aSource);
}

NS_IMETHODIMP    
nsHTMLAnchorElement::GetPing(nsAString& aValue)
{
  return GetURIListAttr(nsGkAtoms::ping, aValue);
}

NS_IMETHODIMP
nsHTMLAnchorElement::SetPing(const nsAString& aValue)
{
  return SetAttr(kNameSpaceID_None, nsGkAtoms::ping, aValue, PR_TRUE);
}

nsLinkState
nsHTMLAnchorElement::GetLinkState() const
{
  return Link::GetLinkState();
}

void
nsHTMLAnchorElement::SetLinkState(nsLinkState aState)
{
  Link::SetLinkState(aState);
}

already_AddRefed<nsIURI>
nsHTMLAnchorElement::GetHrefURI() const
{
  return GetHrefURIForAnchors();
}

nsresult
nsHTMLAnchorElement::SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                             nsIAtom* aPrefix, const nsAString& aValue,
                             PRBool aNotify)
{
  if (aName == nsGkAtoms::href && kNameSpaceID_None == aNameSpaceID) {
    nsAutoString val;
    GetHref(val);
    if (!val.Equals(aValue)) {
      ResetLinkCacheState();
    }
  }

  if (aName == nsGkAtoms::accesskey && kNameSpaceID_None == aNameSpaceID) {
    RegUnRegAccessKey(PR_FALSE);
  }

  nsresult rv = nsGenericHTMLElement::SetAttr(aNameSpaceID, aName, aPrefix,
                                              aValue, aNotify);

  if (aName == nsGkAtoms::accesskey && kNameSpaceID_None == aNameSpaceID &&
      !aValue.IsEmpty()) {
    RegUnRegAccessKey(PR_TRUE);
  }

  return rv;
}

nsresult
nsHTMLAnchorElement::UnsetAttr(PRInt32 aNameSpaceID, nsIAtom* aAttribute,
                               PRBool aNotify)
{
  if (aAttribute == nsGkAtoms::href && kNameSpaceID_None == aNameSpaceID) {
    ResetLinkCacheState();
  }

  if (aAttribute == nsGkAtoms::accesskey &&
      kNameSpaceID_None == aNameSpaceID) {
    RegUnRegAccessKey(PR_FALSE);
  }

  return nsGenericHTMLElement::UnsetAttr(aNameSpaceID, aAttribute, aNotify);
}

PRBool
nsHTMLAnchorElement::ParseAttribute(PRInt32 aNamespaceID,
                                nsIAtom* aAttribute,
                                const nsAString& aValue,
                                nsAttrValue& aResult)
{
  if (aNamespaceID == kNameSpaceID_None && aAttribute == nsGkAtoms::href) {
    return aResult.ParseLazyURIValue(aValue);
  }

  return nsGenericHTMLElement::ParseAttribute(aNamespaceID, aAttribute, aValue,
                                              aResult);
}

void
nsHTMLAnchorElement::DropCachedHref()
{
  nsAttrValue* attr =
    const_cast<nsAttrValue*>(mAttrsAndChildren.GetAttr(nsGkAtoms::href));

  if (!attr || attr->Type() != nsAttrValue::eLazyURIValue)
    return;

  attr->DropCachedURI();
}

void
nsHTMLAnchorElement::ResetLinkCacheState()
{
  Link::ResetLinkState();

  // Clear our cached URI _after_ we ForgetLink(), since ForgetLink()
  // wants that URI.
  const nsAttrValue* attr = mAttrsAndChildren.GetAttr(nsGkAtoms::href);
  if (attr && attr->Type() == nsAttrValue::eLazyURIValue) {
    const_cast<nsAttrValue*>(attr)->DropCachedURI();
  }
}
