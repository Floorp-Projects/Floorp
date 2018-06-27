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
#include "nsAttrValueOrString.h"
#include "nsIDocument.h"
#include "nsIPresShell.h"
#include "nsPresContext.h"
#include "nsIURI.h"
#include "nsWindowSizes.h"

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

// static
const DOMTokenListSupportedToken HTMLAnchorElement::sSupportedRelValues[] = {
  "noreferrer",
  "noopener",
  nullptr
};

HTMLAnchorElement::~HTMLAnchorElement()
{
}

bool
HTMLAnchorElement::IsInteractiveHTMLContent(bool aIgnoreTabindex) const
{
  return HasAttr(kNameSpaceID_None, nsGkAtoms::href) ||
         nsGenericHTMLElement::IsInteractiveHTMLContent(aIgnoreTabindex);
}

NS_IMPL_ISUPPORTS_CYCLE_COLLECTION_INHERITED(HTMLAnchorElement,
                                             nsGenericHTMLElement,
                                             Link)

NS_IMPL_CYCLE_COLLECTION_INHERITED(HTMLAnchorElement,
                                   nsGenericHTMLElement,
                                   mRelList)

NS_IMPL_ELEMENT_CLONE(HTMLAnchorElement)

JSObject*
HTMLAnchorElement::WrapNode(JSContext *aCx, JS::Handle<JSObject*> aGivenProto)
{
  return HTMLAnchorElement_Binding::Wrap(aCx, this, aGivenProto);
}

int32_t
HTMLAnchorElement::TabIndexDefault()
{
  return 0;
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
    nsPresContext* presContext = doc->GetPresContext();
    if (presContext && !presContext->GetLinkHandler()) {
      *aIsFocusable = false;
      return false;
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

void
HTMLAnchorElement::GetEventTargetParent(EventChainPreVisitor& aVisitor)
{
  GetEventTargetParentForAnchors(aVisitor);
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

void
HTMLAnchorElement::GetTarget(nsAString& aValue)
{
  if (!GetAttr(kNameSpaceID_None, nsGkAtoms::target, aValue)) {
    GetBaseTarget(aValue);
  }
}

nsDOMTokenList*
HTMLAnchorElement::RelList()
{
  if (!mRelList) {
    mRelList = new nsDOMTokenList(this, nsGkAtoms::rel, sSupportedRelValues);
  }
  return mRelList;
}

void
HTMLAnchorElement::GetText(nsAString& aText, mozilla::ErrorResult& aRv)
{
  if (NS_WARN_IF(!nsContentUtils::GetNodeTextContent(this, true, aText, fallible))) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
  }
}

void
HTMLAnchorElement::SetText(const nsAString& aText, ErrorResult& aRv)
{
  aRv = nsContentUtils::SetNodeTextContent(this, aText, false);
}

void
HTMLAnchorElement::ToString(nsAString& aSource)
{
  return GetHref(aSource);
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
HTMLAnchorElement::BeforeSetAttr(int32_t aNamespaceID, nsAtom* aName,
                                 const nsAttrValueOrString* aValue,
                                 bool aNotify)
{
  if (aNamespaceID == kNameSpaceID_None) {
    if (aName == nsGkAtoms::href) {
      CancelDNSPrefetch(HTML_ANCHOR_DNS_PREFETCH_DEFERRED,
                        HTML_ANCHOR_DNS_PREFETCH_REQUESTED);
    }
  }

  return nsGenericHTMLElement::BeforeSetAttr(aNamespaceID, aName, aValue,
                                             aNotify);
}

nsresult
HTMLAnchorElement::AfterSetAttr(int32_t aNamespaceID, nsAtom* aName,
                                const nsAttrValue* aValue,
                                const nsAttrValue* aOldValue,
                                nsIPrincipal* aSubjectPrincipal,
                                bool aNotify)
{
  if (aNamespaceID == kNameSpaceID_None) {
    if (aName == nsGkAtoms::href) {
      Link::ResetLinkState(aNotify, !!aValue);
      if (aValue && IsInComposedDoc()) {
        TryDNSPrefetch();
      }
    }
  }

  return nsGenericHTMLElement::AfterSetAttr(aNamespaceID, aName,
                                            aValue, aOldValue, aSubjectPrincipal, aNotify);
}

EventStates
HTMLAnchorElement::IntrinsicState() const
{
  return Link::LinkState() | nsGenericHTMLElement::IntrinsicState();
}

void
HTMLAnchorElement::AddSizeOfExcludingThis(nsWindowSizes& aSizes,
                                          size_t* aNodeSize) const
{
  nsGenericHTMLElement::AddSizeOfExcludingThis(aSizes, aNodeSize);
  *aNodeSize += Link::SizeOfExcludingThis(aSizes.mState);
}

} // namespace dom
} // namespace mozilla
