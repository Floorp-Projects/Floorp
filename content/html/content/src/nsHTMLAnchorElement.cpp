/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set tw=80 expandtab softtabstop=2 ts=2 sw=2: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsGenericHTMLElement.h"
#include "nsIDOMHTMLAnchorElement.h"
#include "nsILink.h"
#include "Link.h"

#include "nsCOMPtr.h"
#include "nsContentUtils.h"
#include "nsGkAtoms.h"
#include "nsIPresShell.h"
#include "nsIDocument.h"
#include "nsPresContext.h"
#include "nsHTMLDNSPrefetch.h"

using namespace mozilla::dom;

class nsHTMLAnchorElement : public nsGenericHTMLElement,
                            public nsIDOMHTMLAnchorElement,
                            public nsILink,
                            public Link
{
public:
  using nsGenericElement::GetText;
  using nsGenericElement::SetText;

  nsHTMLAnchorElement(already_AddRefed<nsINodeInfo> aNodeInfo);
  virtual ~nsHTMLAnchorElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE(nsGenericHTMLElement::)

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT(nsGenericHTMLElement::)

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT_BASIC(nsGenericHTMLElement::)
  NS_IMETHOD Click() {
    return nsGenericHTMLElement::Click();
  }
  NS_IMETHOD GetTabIndex(PRInt32* aTabIndex);
  NS_IMETHOD SetTabIndex(PRInt32 aTabIndex);
  NS_IMETHOD Focus() {
    return nsGenericHTMLElement::Focus();
  }
  NS_IMETHOD GetDraggable(bool* aDraggable);
  NS_IMETHOD GetInnerHTML(nsAString& aInnerHTML) {
    return nsGenericHTMLElement::GetInnerHTML(aInnerHTML);
  }
  NS_IMETHOD SetInnerHTML(const nsAString& aInnerHTML) {
    return nsGenericHTMLElement::SetInnerHTML(aInnerHTML);
  }

  // nsIDOMHTMLAnchorElement
  NS_DECL_NSIDOMHTMLANCHORELEMENT  

  // DOM memory reporter participant
  NS_DECL_SIZEOF_EXCLUDING_THIS

  // nsILink
  NS_IMETHOD LinkAdded() { return NS_OK; }
  NS_IMETHOD LinkRemoved() { return NS_OK; }

  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              bool aCompileEventHandlers);
  virtual void UnbindFromTree(bool aDeep = true,
                              bool aNullParent = true);
  virtual bool IsHTMLFocusable(bool aWithMouse, bool *aIsFocusable, PRInt32 *aTabIndex);

  virtual nsresult PreHandleEvent(nsEventChainPreVisitor& aVisitor);
  virtual nsresult PostHandleEvent(nsEventChainPostVisitor& aVisitor);
  virtual bool IsLink(nsIURI** aURI) const;
  virtual void GetLinkTarget(nsAString& aTarget);
  virtual nsLinkState GetLinkState() const;
  virtual already_AddRefed<nsIURI> GetHrefURI() const;

  nsresult SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                   const nsAString& aValue, bool aNotify)
  {
    return SetAttr(aNameSpaceID, aName, nsnull, aValue, aNotify);
  }
  virtual nsresult SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                           nsIAtom* aPrefix, const nsAString& aValue,
                           bool aNotify);
  virtual nsresult UnsetAttr(PRInt32 aNameSpaceID, nsIAtom* aAttribute,
                             bool aNotify);
  virtual bool ParseAttribute(PRInt32 aNamespaceID,
                                nsIAtom* aAttribute,
                                const nsAString& aValue,
                                nsAttrValue& aResult);

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  virtual nsEventStates IntrinsicState() const;

  virtual nsXPCClassInfo* GetClassInfo();

  virtual nsIDOMNode* AsDOMNode() { return this; }
  
  virtual void OnDNSPrefetchDeferred();
  virtual void OnDNSPrefetchRequested();
  virtual bool HasDeferredDNSPrefetchRequest();

protected:
  virtual void GetItemValueText(nsAString& text);
  virtual void SetItemValueText(const nsAString& text);
};

// Indicates that a DNS Prefetch has been requested from this Anchor elem
#define HTML_ANCHOR_DNS_PREFETCH_REQUESTED \
  (1 << ELEMENT_TYPE_SPECIFIC_BITS_OFFSET)
// Indicates that a DNS Prefetch was added to the deferral queue
#define HTML_ANCHOR_DNS_PREFETCH_DEFERRED \
  (1 << (ELEMENT_TYPE_SPECIFIC_BITS_OFFSET+1))

// Make sure we have enough space for those bits
PR_STATIC_ASSERT(ELEMENT_TYPE_SPECIFIC_BITS_OFFSET+1 < 32);

NS_IMPL_NS_NEW_HTML_ELEMENT(Anchor)

nsHTMLAnchorElement::nsHTMLAnchorElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : nsGenericHTMLElement(aNodeInfo)
  , Link(this)
{
}

nsHTMLAnchorElement::~nsHTMLAnchorElement()
{
}


NS_IMPL_ADDREF_INHERITED(nsHTMLAnchorElement, nsGenericElement) 
NS_IMPL_RELEASE_INHERITED(nsHTMLAnchorElement, nsGenericElement) 


DOMCI_NODE_DATA(HTMLAnchorElement, nsHTMLAnchorElement)

// QueryInterface implementation for nsHTMLAnchorElement
NS_INTERFACE_TABLE_HEAD(nsHTMLAnchorElement)
  NS_HTML_CONTENT_INTERFACE_TABLE3(nsHTMLAnchorElement,
                                   nsIDOMHTMLAnchorElement,
                                   nsILink,
                                   Link)
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
NS_IMPL_INT_ATTR(nsHTMLAnchorElement, TabIndex, tabindex)
NS_IMPL_STRING_ATTR(nsHTMLAnchorElement, Type, type)

void
nsHTMLAnchorElement::GetItemValueText(nsAString& aValue)
{
  GetHref(aValue);
}

void
nsHTMLAnchorElement::SetItemValueText(const nsAString& aValue)
{
  SetHref(aValue);
}

NS_IMETHODIMP
nsHTMLAnchorElement::GetDraggable(bool* aDraggable)
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

void
nsHTMLAnchorElement::OnDNSPrefetchRequested()
{
  UnsetFlags(HTML_ANCHOR_DNS_PREFETCH_DEFERRED);
  SetFlags(HTML_ANCHOR_DNS_PREFETCH_REQUESTED);
}

void
nsHTMLAnchorElement::OnDNSPrefetchDeferred()
{
  UnsetFlags(HTML_ANCHOR_DNS_PREFETCH_REQUESTED);
  SetFlags(HTML_ANCHOR_DNS_PREFETCH_DEFERRED);
}

bool
nsHTMLAnchorElement::HasDeferredDNSPrefetchRequest()
{
  return HasFlag(HTML_ANCHOR_DNS_PREFETCH_DEFERRED);
}

nsresult
nsHTMLAnchorElement::BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                                nsIContent* aBindingParent,
                                bool aCompileEventHandlers)
{
  Link::ResetLinkState(false);

  nsresult rv = nsGenericHTMLElement::BindToTree(aDocument, aParent,
                                                 aBindingParent,
                                                 aCompileEventHandlers);
  NS_ENSURE_SUCCESS(rv, rv);

  // Prefetch links
  if (aDocument) {
    aDocument->RegisterPendingLinkUpdate(this);
    if (nsHTMLDNSPrefetch::IsAllowed(OwnerDoc())) {
      nsHTMLDNSPrefetch::PrefetchLow(this);
    }
  }

  return rv;
}

void
nsHTMLAnchorElement::UnbindFromTree(bool aDeep, bool aNullParent)
{
  // Cancel any DNS prefetches
  // Note: Must come before ResetLinkState.  If called after, it will recreate
  // mCachedURI based on data that is invalid - due to a call to GetHostname.

  // If prefetch was deferred, clear flag and move on
  if (HasFlag(HTML_ANCHOR_DNS_PREFETCH_DEFERRED))
    UnsetFlags(HTML_ANCHOR_DNS_PREFETCH_DEFERRED);
  // Else if prefetch was requested, clear flag and send cancellation
  else if (HasFlag(HTML_ANCHOR_DNS_PREFETCH_REQUESTED)) {
    UnsetFlags(HTML_ANCHOR_DNS_PREFETCH_REQUESTED);
    // Possible that hostname could have changed since binding, but since this
    // covers common cases, most DNS prefetch requests will be canceled
    nsHTMLDNSPrefetch::CancelPrefetchLow(this, NS_ERROR_ABORT);
  }
  
  // If this link is ever reinserted into a document, it might
  // be under a different xml:base, so forget the cached state now.
  Link::ResetLinkState(false);
  
  nsIDocument* doc = GetCurrentDoc();
  if (doc) {
    doc->UnregisterPendingLinkUpdate(this);
  }

  nsGenericHTMLElement::UnbindFromTree(aDeep, aNullParent);
}

bool
nsHTMLAnchorElement::IsHTMLFocusable(bool aWithMouse,
                                     bool *aIsFocusable, PRInt32 *aTabIndex)
{
  if (nsGenericHTMLElement::IsHTMLFocusable(aWithMouse, aIsFocusable, aTabIndex)) {
    return true;
  }

  // cannot focus links if there is no link handler
  nsIDocument* doc = GetCurrentDoc();
  if (doc) {
    nsIPresShell* presShell = doc->GetShell();
    if (presShell) {
      nsPresContext* presContext = presShell->GetPresContext();
      if (presContext && !presContext->GetLinkHandler()) {
        *aIsFocusable = false;
        return false;
      }
    }
  }

  if (IsEditable()) {
    if (aTabIndex) {
      *aTabIndex = -1;
    }

    *aIsFocusable = false;

    return true;
  }

  if (!HasAttr(kNameSpaceID_None, nsGkAtoms::tabindex)) {
    // check whether we're actually a link
    if (!Link::HasURI()) {
      // Not tabbable or focusable without href (bug 17605), unless
      // forced to be via presence of nonnegative tabindex attribute
      if (aTabIndex) {
        *aTabIndex = -1;
      }

      *aIsFocusable = false;

      return false;
    }
  }

  if (aTabIndex && (sTabFocusModel & eTabFocus_linksMask) == 0) {
    *aTabIndex = -1;
  }

  *aIsFocusable = true;

  return false;
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

bool
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
  return SetAttr(kNameSpaceID_None, nsGkAtoms::target, aValue, true);
}

#define IMPL_URI_PART(_part)                                 \
  NS_IMETHODIMP                                              \
  nsHTMLAnchorElement::Get##_part(nsAString& a##_part)       \
  {                                                          \
    return Link::Get##_part(a##_part);                       \
  }                                                          \
  NS_IMETHODIMP                                              \
  nsHTMLAnchorElement::Set##_part(const nsAString& a##_part) \
  {                                                          \
    return Link::Set##_part(a##_part);                       \
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
  nsContentUtils::GetNodeTextContent(this, true, aText);
  return NS_OK;
}

NS_IMETHODIMP    
nsHTMLAnchorElement::SetText(const nsAString& aText)
{
  return nsContentUtils::SetNodeTextContent(this, aText, false);
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
  return SetAttr(kNameSpaceID_None, nsGkAtoms::ping, aValue, true);
}

nsLinkState
nsHTMLAnchorElement::GetLinkState() const
{
  return Link::GetLinkState();
}

already_AddRefed<nsIURI>
nsHTMLAnchorElement::GetHrefURI() const
{
  nsIURI* uri = Link::GetCachedURI();
  if (uri) {
    NS_ADDREF(uri);
    return uri;
  }

  return GetHrefURIForAnchors();
}

nsresult
nsHTMLAnchorElement::SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                             nsIAtom* aPrefix, const nsAString& aValue,
                             bool aNotify)
{
  bool reset = false;
  if (aName == nsGkAtoms::href && kNameSpaceID_None == aNameSpaceID) {
    // If we do not have a cached URI, we have some value here so we must reset
    // our link state after calling the parent.
    if (!Link::HasCachedURI()) {
      reset = true;
    }
    // However, if we have a cached URI, we'll want to see if the value changed.
    else {
      nsAutoString val;
      GetHref(val);
      if (!val.Equals(aValue)) {
        reset = true;
      }
    }
  }

  nsresult rv = nsGenericHTMLElement::SetAttr(aNameSpaceID, aName, aPrefix,
                                              aValue, aNotify);

  // The ordering of the parent class's SetAttr call and Link::ResetLinkState
  // is important here!  The attribute is not set until SetAttr returns, and
  // we will need the updated attribute value because notifying the document
  // that content states have changed will call IntrinsicState, which will try
  // to get updated information about the visitedness from Link.
  if (reset) {
    Link::ResetLinkState(!!aNotify);
  }

  return rv;
}

nsresult
nsHTMLAnchorElement::UnsetAttr(PRInt32 aNameSpaceID, nsIAtom* aAttribute,
                               bool aNotify)
{
  nsresult rv = nsGenericHTMLElement::UnsetAttr(aNameSpaceID, aAttribute,
                                                aNotify);

  // The ordering of the parent class's UnsetAttr call and Link::ResetLinkState
  // is important here!  The attribute is not unset until UnsetAttr returns, and
  // we will need the updated attribute value because notifying the document
  // that content states have changed will call IntrinsicState, which will try
  // to get updated information about the visitedness from Link.
  if (aAttribute == nsGkAtoms::href && kNameSpaceID_None == aNameSpaceID) {
    Link::ResetLinkState(!!aNotify);
  }

  return rv;
}

bool
nsHTMLAnchorElement::ParseAttribute(PRInt32 aNamespaceID,
                                    nsIAtom* aAttribute,
                                    const nsAString& aValue,
                                    nsAttrValue& aResult)
{
  return nsGenericHTMLElement::ParseAttribute(aNamespaceID, aAttribute, aValue,
                                              aResult);
}

nsEventStates
nsHTMLAnchorElement::IntrinsicState() const
{
  return Link::LinkState() | nsGenericHTMLElement::IntrinsicState();
}

size_t
nsHTMLAnchorElement::SizeOfExcludingThis(nsMallocSizeOfFun aMallocSizeOf) const
{
  return nsGenericHTMLElement::SizeOfExcludingThis(aMallocSizeOf) +
         Link::SizeOfExcludingThis(aMallocSizeOf);
}

