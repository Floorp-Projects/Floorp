/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLAnchorElement.h"

#include "mozilla/dom/HTMLAnchorElementBinding.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/EventStates.h"
#include "mozilla/MemoryReporting.h"
#include "nsCOMPtr.h"
#include "nsContentUtils.h"
#include "nsGkAtoms.h"
#include "nsHTMLDNSPrefetch.h"
#include "nsIDocument.h"
#include "nsIPresShell.h"
#include "nsPresContext.h"
#include "nsIURI.h"

NS_IMPL_NS_NEW_HTML_ELEMENT(Anchor)

namespace mozilla {
namespace dom {

#define ANCHOR_ELEMENT_FLAG_BIT(n_) NODE_FLAG_BIT(ELEMENT_TYPE_SPECIFIC_BITS_OFFSET + (n_))

// Anchor element specific bits
enum {
  // Indicates that a DNS Prefetch has been requested from this Anchor elem
  HTML_ANCHOR_DNS_PREFETCH_REQUESTED =    ANCHOR_ELEMENT_FLAG_BIT(0),

  // Indicates that a DNS Prefetch was added to the deferral queue
  HTML_ANCHOR_DNS_PREFETCH_DEFERRED =     ANCHOR_ELEMENT_FLAG_BIT(1)
};

ASSERT_NODE_FLAGS_SPACE(ELEMENT_TYPE_SPECIFIC_BITS_OFFSET + 2);

#undef ANCHOR_ELEMENT_FLAG_BIT

HTMLAnchorElement::~HTMLAnchorElement()
{
}

bool
HTMLAnchorElement::IsInteractiveHTMLContent(bool aIgnoreTabindex) const
{
  return HasAttr(kNameSpaceID_None, nsGkAtoms::href) ||
         nsGenericHTMLElement::IsInteractiveHTMLContent(aIgnoreTabindex);
}

NS_INTERFACE_TABLE_HEAD_CYCLE_COLLECTION_INHERITED(HTMLAnchorElement)
  NS_INTERFACE_TABLE_INHERITED(HTMLAnchorElement,
                               nsIDOMHTMLAnchorElement,
                               Link)
NS_INTERFACE_TABLE_TAIL_INHERITING(nsGenericHTMLElement)

NS_IMPL_ADDREF_INHERITED(HTMLAnchorElement, Element)
NS_IMPL_RELEASE_INHERITED(HTMLAnchorElement, Element)

NS_IMPL_CYCLE_COLLECTION_CLASS(HTMLAnchorElement)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(HTMLAnchorElement,
                                                  nsGenericHTMLElement)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mRelList)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(HTMLAnchorElement,
                                                nsGenericHTMLElement)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mRelList)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_ELEMENT_CLONE(HTMLAnchorElement)

JSObject*
HTMLAnchorElement::WrapNode(JSContext *aCx, JS::Handle<JSObject*> aGivenProto)
{
  return HTMLAnchorElementBinding::Wrap(aCx, this, aGivenProto);
}

NS_IMPL_STRING_ATTR(HTMLAnchorElement, Charset, charset)
NS_IMPL_STRING_ATTR(HTMLAnchorElement, Coords, coords)
NS_IMPL_URI_ATTR(HTMLAnchorElement, Href, href)
NS_IMPL_STRING_ATTR(HTMLAnchorElement, Hreflang, hreflang)
NS_IMPL_STRING_ATTR(HTMLAnchorElement, Name, name)
NS_IMPL_STRING_ATTR(HTMLAnchorElement, Rel, rel)
NS_IMPL_STRING_ATTR(HTMLAnchorElement, Rev, rev)
NS_IMPL_STRING_ATTR(HTMLAnchorElement, Shape, shape)
NS_IMPL_STRING_ATTR(HTMLAnchorElement, Type, type)
NS_IMPL_STRING_ATTR(HTMLAnchorElement, Download, download)

int32_t
HTMLAnchorElement::TabIndexDefault()
{
  return 0;
}

void
HTMLAnchorElement::GetItemValueText(DOMString& aValue)
{
  GetHref(aValue);
}

void
HTMLAnchorElement::SetItemValueText(const nsAString& aValue)
{
  SetHref(aValue);
}

bool
HTMLAnchorElement::Draggable() const
{
  // links can be dragged as long as there is an href and the
  // draggable attribute isn't false
  if (!HasAttr(kNameSpaceID_None, nsGkAtoms::href)) {
    // no href, so just use the same behavior as other elements
    return nsGenericHTMLElement::Draggable();
  }

  return !AttrValueIs(kNameSpaceID_None, nsGkAtoms::draggable,
                      nsGkAtoms::_false, eIgnoreCase);
}

void
HTMLAnchorElement::OnDNSPrefetchRequested()
{
  UnsetFlags(HTML_ANCHOR_DNS_PREFETCH_DEFERRED);
  SetFlags(HTML_ANCHOR_DNS_PREFETCH_REQUESTED);
}

void
HTMLAnchorElement::OnDNSPrefetchDeferred()
{
  UnsetFlags(HTML_ANCHOR_DNS_PREFETCH_REQUESTED);
  SetFlags(HTML_ANCHOR_DNS_PREFETCH_DEFERRED);
}

bool
HTMLAnchorElement::HasDeferredDNSPrefetchRequest()
{
  return HasFlag(HTML_ANCHOR_DNS_PREFETCH_DEFERRED);
}

nsresult
HTMLAnchorElement::BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              bool aCompileEventHandlers)
{
  Link::ResetLinkState(false, Link::ElementHasHref());

  nsresult rv = nsGenericHTMLElement::BindToTree(aDocument, aParent,
                                                 aBindingParent,
                                                 aCompileEventHandlers);
  NS_ENSURE_SUCCESS(rv, rv);

  // Prefetch links
  nsIDocument* doc = GetComposedDoc();
  if (doc) {
    doc->RegisterPendingLinkUpdate(this);
    TryDNSPrefetch();
  }

  return rv;
}

void
HTMLAnchorElement::UnbindFromTree(bool aDeep, bool aNullParent)
{
  // Cancel any DNS prefetches
  // Note: Must come before ResetLinkState.  If called after, it will recreate
  // mCachedURI based on data that is invalid - due to a call to GetHostname.
  CancelDNSPrefetch(HTML_ANCHOR_DNS_PREFETCH_DEFERRED,
                    HTML_ANCHOR_DNS_PREFETCH_REQUESTED);

  // If this link is ever reinserted into a document, it might
  // be under a different xml:base, so forget the cached state now.
  Link::ResetLinkState(false, Link::ElementHasHref());

  // Note, we need to use OwnerDoc() here, since GetComposedDoc() might
  // return null.
  nsIDocument* doc = OwnerDoc();
  if (doc) {
    doc->UnregisterPendingLinkUpdate(this);
  }

  nsGenericHTMLElement::UnbindFromTree(aDeep, aNullParent);
}

static bool
IsNodeInEditableRegion(nsINode* aNode)
{
  while (aNode) {
    if (aNode->IsEditable()) {
      return true;
    }
    aNode = aNode->GetParent();
  }
  return false;
}

bool
HTMLAnchorElement::IsHTMLFocusable(bool aWithMouse,
                                   bool *aIsFocusable, int32_t *aTabIndex)
{
  if (nsGenericHTMLElement::IsHTMLFocusable(aWithMouse, aIsFocusable, aTabIndex)) {
    return true;
  }

  // cannot focus links if there is no link handler
  nsIDocument* doc = GetComposedDoc();
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

  // Links that are in an editable region should never be focusable, even if
  // they are in a contenteditable="false" region.
  if (IsNodeInEditableRegion(this)) {
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
HTMLAnchorElement::PreHandleEvent(EventChainPreVisitor& aVisitor)
{
  return PreHandleEventForAnchors(aVisitor);
}

nsresult
HTMLAnchorElement::PostHandleEvent(EventChainPostVisitor& aVisitor)
{
  return PostHandleEventForAnchors(aVisitor);
}

bool
HTMLAnchorElement::IsLink(nsIURI** aURI) const
{
  return IsHTMLLink(aURI);
}

void
HTMLAnchorElement::GetLinkTarget(nsAString& aTarget)
{
  GetAttr(kNameSpaceID_None, nsGkAtoms::target, aTarget);
  if (aTarget.IsEmpty()) {
    GetBaseTarget(aTarget);
  }
}

NS_IMETHODIMP
HTMLAnchorElement::GetTarget(nsAString& aValue)
{
  if (!GetAttr(kNameSpaceID_None, nsGkAtoms::target, aValue)) {
    GetBaseTarget(aValue);
  }
  return NS_OK;
}

NS_IMETHODIMP
HTMLAnchorElement::SetTarget(const nsAString& aValue)
{
  return SetAttr(kNameSpaceID_None, nsGkAtoms::target, aValue, true);
}

nsDOMTokenList*
HTMLAnchorElement::RelList()
{
  if (!mRelList) {
    mRelList = new nsDOMTokenList(this, nsGkAtoms::rel);
  }
  return mRelList;
}

#define IMPL_URI_PART(_part)                                 \
  NS_IMETHODIMP                                              \
  HTMLAnchorElement::Get##_part(nsAString& a##_part)         \
  {                                                          \
    Link::Get##_part(a##_part);                              \
    return NS_OK;                                            \
  }                                                          \
  NS_IMETHODIMP                                              \
  HTMLAnchorElement::Set##_part(const nsAString& a##_part)   \
  {                                                          \
    Link::Set##_part(a##_part);                              \
    return NS_OK;                                            \
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
HTMLAnchorElement::GetText(nsAString& aText)
{
  if(!nsContentUtils::GetNodeTextContent(this, true, aText, fallible)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return NS_OK;
}

NS_IMETHODIMP    
HTMLAnchorElement::SetText(const nsAString& aText)
{
  return nsContentUtils::SetNodeTextContent(this, aText, false);
}

NS_IMETHODIMP
HTMLAnchorElement::ToString(nsAString& aSource)
{
  return GetHref(aSource);
}

NS_IMETHODIMP    
HTMLAnchorElement::GetPing(nsAString& aValue)
{
  return GetURIListAttr(nsGkAtoms::ping, aValue);
}

NS_IMETHODIMP
HTMLAnchorElement::SetPing(const nsAString& aValue)
{
  return SetAttr(kNameSpaceID_None, nsGkAtoms::ping, aValue, true);
}

already_AddRefed<nsIURI>
HTMLAnchorElement::GetHrefURI() const
{
  nsCOMPtr<nsIURI> uri = Link::GetCachedURI();
  if (uri) {
    return uri.forget();
  }

  return GetHrefURIForAnchors();
}

nsresult
HTMLAnchorElement::SetAttr(int32_t aNameSpaceID, nsIAtom* aName,
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
    if (reset) {
      CancelDNSPrefetch(HTML_ANCHOR_DNS_PREFETCH_DEFERRED,
                        HTML_ANCHOR_DNS_PREFETCH_REQUESTED);
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
    Link::ResetLinkState(!!aNotify, true);
    if (IsInComposedDoc()) {
      TryDNSPrefetch();
    }
  }

  return rv;
}

nsresult
HTMLAnchorElement::UnsetAttr(int32_t aNameSpaceID, nsIAtom* aAttribute,
                             bool aNotify)
{
  bool href =
    (aAttribute == nsGkAtoms::href && kNameSpaceID_None == aNameSpaceID);

  if (href) {
    CancelDNSPrefetch(HTML_ANCHOR_DNS_PREFETCH_DEFERRED,
                      HTML_ANCHOR_DNS_PREFETCH_REQUESTED);
  }

  nsresult rv = nsGenericHTMLElement::UnsetAttr(aNameSpaceID, aAttribute,
                                                aNotify);

  // The ordering of the parent class's UnsetAttr call and Link::ResetLinkState
  // is important here!  The attribute is not unset until UnsetAttr returns, and
  // we will need the updated attribute value because notifying the document
  // that content states have changed will call IntrinsicState, which will try
  // to get updated information about the visitedness from Link.
  if (href) {
    Link::ResetLinkState(!!aNotify, false);
  }

  return rv;
}

bool
HTMLAnchorElement::ParseAttribute(int32_t aNamespaceID,
                                  nsIAtom* aAttribute,
                                  const nsAString& aValue,
                                  nsAttrValue& aResult)
{
  return nsGenericHTMLElement::ParseAttribute(aNamespaceID, aAttribute, aValue,
                                              aResult);
}

EventStates
HTMLAnchorElement::IntrinsicState() const
{
  return Link::LinkState() | nsGenericHTMLElement::IntrinsicState();
}

size_t
HTMLAnchorElement::SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
{
  return nsGenericHTMLElement::SizeOfExcludingThis(aMallocSizeOf) +
         Link::SizeOfExcludingThis(aMallocSizeOf);
}

} // namespace dom
} // namespace mozilla
