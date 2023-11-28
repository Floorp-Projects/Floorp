/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLAnchorElement.h"

#include "mozilla/dom/BindContext.h"
#include "mozilla/dom/HTMLAnchorElementBinding.h"
#include "mozilla/dom/HTMLDNSPrefetch.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/MemoryReporting.h"
#include "nsCOMPtr.h"
#include "nsContentUtils.h"
#include "nsGkAtoms.h"
#include "nsAttrValueOrString.h"
#include "mozilla/dom/Document.h"
#include "nsPresContext.h"
#include "nsIURI.h"
#include "nsWindowSizes.h"

NS_IMPL_NS_NEW_HTML_ELEMENT(Anchor)

namespace mozilla::dom {

HTMLAnchorElement::~HTMLAnchorElement() {
  SupportsDNSPrefetch::Destroyed(*this);
}

bool HTMLAnchorElement::IsInteractiveHTMLContent() const {
  return HasAttr(nsGkAtoms::href) ||
         nsGenericHTMLElement::IsInteractiveHTMLContent();
}

NS_IMPL_ISUPPORTS_CYCLE_COLLECTION_INHERITED(HTMLAnchorElement,
                                             nsGenericHTMLElement, Link)

NS_IMPL_CYCLE_COLLECTION_INHERITED(HTMLAnchorElement, nsGenericHTMLElement,
                                   mRelList)

NS_IMPL_ELEMENT_CLONE(HTMLAnchorElement)

JSObject* HTMLAnchorElement::WrapNode(JSContext* aCx,
                                      JS::Handle<JSObject*> aGivenProto) {
  return HTMLAnchorElement_Binding::Wrap(aCx, this, aGivenProto);
}

int32_t HTMLAnchorElement::TabIndexDefault() { return 0; }

bool HTMLAnchorElement::Draggable() const {
  // links can be dragged as long as there is an href and the
  // draggable attribute isn't false
  if (!HasAttr(nsGkAtoms::href)) {
    // no href, so just use the same behavior as other elements
    return nsGenericHTMLElement::Draggable();
  }

  return !AttrValueIs(kNameSpaceID_None, nsGkAtoms::draggable,
                      nsGkAtoms::_false, eIgnoreCase);
}

nsresult HTMLAnchorElement::BindToTree(BindContext& aContext,
                                       nsINode& aParent) {
  nsresult rv = nsGenericHTMLElement::BindToTree(aContext, aParent);
  NS_ENSURE_SUCCESS(rv, rv);

  Link::BindToTree(aContext);

  // Prefetch links
  if (IsInComposedDoc()) {
    TryDNSPrefetch(*this);
  }

  return rv;
}

void HTMLAnchorElement::UnbindFromTree(bool aNullParent) {
  // Cancel any DNS prefetches
  // Note: Must come before ResetLinkState.  If called after, it will recreate
  // mCachedURI based on data that is invalid - due to a call to Link::GetURI()
  // via GetURIForDNSPrefetch().
  CancelDNSPrefetch(*this);

  nsGenericHTMLElement::UnbindFromTree(aNullParent);

  // Without removing the link state we risk a dangling pointer in the
  // mStyledLinks hashtable
  Link::UnbindFromTree();
}

bool HTMLAnchorElement::IsHTMLFocusable(bool aWithMouse, bool* aIsFocusable,
                                        int32_t* aTabIndex) {
  if (nsGenericHTMLElement::IsHTMLFocusable(aWithMouse, aIsFocusable,
                                            aTabIndex)) {
    return true;
  }

  // cannot focus links if there is no link handler
  if (!OwnerDoc()->LinkHandlingEnabled()) {
    *aIsFocusable = false;
    return false;
  }

  // Links that are in an editable region should never be focusable, even if
  // they are in a contenteditable="false" region.
  if (nsContentUtils::IsNodeInEditableRegion(this)) {
    if (aTabIndex) {
      *aTabIndex = -1;
    }

    *aIsFocusable = false;

    return true;
  }

  if (GetTabIndexAttrValue().isNothing()) {
    // check whether we're actually a link
    if (!IsLink()) {
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

void HTMLAnchorElement::GetEventTargetParent(EventChainPreVisitor& aVisitor) {
  GetEventTargetParentForAnchors(aVisitor);
}

nsresult HTMLAnchorElement::PostHandleEvent(EventChainPostVisitor& aVisitor) {
  return PostHandleEventForAnchors(aVisitor);
}

void HTMLAnchorElement::GetLinkTarget(nsAString& aTarget) {
  GetAttr(nsGkAtoms::target, aTarget);
  if (aTarget.IsEmpty()) {
    GetBaseTarget(aTarget);
  }
}

void HTMLAnchorElement::GetTarget(nsAString& aValue) const {
  if (!GetAttr(nsGkAtoms::target, aValue)) {
    GetBaseTarget(aValue);
  }
}

nsDOMTokenList* HTMLAnchorElement::RelList() {
  if (!mRelList) {
    mRelList = new nsDOMTokenList(this, nsGkAtoms::rel, sSupportedRelValues);
  }
  return mRelList;
}

void HTMLAnchorElement::GetText(nsAString& aText,
                                mozilla::ErrorResult& aRv) const {
  if (NS_WARN_IF(
          !nsContentUtils::GetNodeTextContent(this, true, aText, fallible))) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
  }
}

void HTMLAnchorElement::SetText(const nsAString& aText, ErrorResult& aRv) {
  aRv = nsContentUtils::SetNodeTextContent(this, aText, false);
}

already_AddRefed<nsIURI> HTMLAnchorElement::GetHrefURI() const {
  if (nsCOMPtr<nsIURI> uri = GetCachedURI()) {
    return uri.forget();
  }
  return GetHrefURIForAnchors();
}

void HTMLAnchorElement::BeforeSetAttr(int32_t aNamespaceID, nsAtom* aName,
                                      const nsAttrValue* aValue, bool aNotify) {
  if (aNamespaceID == kNameSpaceID_None && aName == nsGkAtoms::href) {
    CancelDNSPrefetch(*this);
  }
  return nsGenericHTMLElement::BeforeSetAttr(aNamespaceID, aName, aValue,
                                             aNotify);
}

void HTMLAnchorElement::AfterSetAttr(int32_t aNamespaceID, nsAtom* aName,
                                     const nsAttrValue* aValue,
                                     const nsAttrValue* aOldValue,
                                     nsIPrincipal* aSubjectPrincipal,
                                     bool aNotify) {
  if (aNamespaceID == kNameSpaceID_None) {
    if (aName == nsGkAtoms::href) {
      Link::ResetLinkState(aNotify, !!aValue);
      if (aValue && IsInComposedDoc()) {
        TryDNSPrefetch(*this);
      }
    }
  }

  return nsGenericHTMLElement::AfterSetAttr(
      aNamespaceID, aName, aValue, aOldValue, aSubjectPrincipal, aNotify);
}

void HTMLAnchorElement::AddSizeOfExcludingThis(nsWindowSizes& aSizes,
                                               size_t* aNodeSize) const {
  nsGenericHTMLElement::AddSizeOfExcludingThis(aSizes, aNodeSize);
  *aNodeSize += Link::SizeOfExcludingThis(aSizes.mState);
}

}  // namespace mozilla::dom
