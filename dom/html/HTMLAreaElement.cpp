/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLAreaElement.h"

#include "mozilla/Attributes.h"
#include "mozilla/dom/HTMLAnchorElement.h"
#include "mozilla/dom/HTMLAreaElementBinding.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/EventStates.h"
#include "mozilla/MemoryReporting.h"
#include "nsWindowSizes.h"

NS_IMPL_NS_NEW_HTML_ELEMENT(Area)

namespace mozilla {
namespace dom {

HTMLAreaElement::HTMLAreaElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo)
  : nsGenericHTMLElement(aNodeInfo)
  , Link(this)
{
}

HTMLAreaElement::~HTMLAreaElement()
{
}

NS_IMPL_ISUPPORTS_CYCLE_COLLECTION_INHERITED(HTMLAreaElement,
                                             nsGenericHTMLElement,
                                             Link)

NS_IMPL_CYCLE_COLLECTION_INHERITED(HTMLAreaElement,
                                   nsGenericHTMLElement,
                                   mRelList)

NS_IMPL_ELEMENT_CLONE(HTMLAreaElement)

int32_t
HTMLAreaElement::TabIndexDefault()
{
  return 0;
}

void
HTMLAreaElement::GetTarget(DOMString& aValue)
{
  if (!GetAttr(kNameSpaceID_None, nsGkAtoms::target, aValue)) {
    GetBaseTarget(aValue);
  }
}

void
HTMLAreaElement::GetEventTargetParent(EventChainPreVisitor& aVisitor)
{
  GetEventTargetParentForAnchors(aVisitor);
}

nsresult
HTMLAreaElement::PostHandleEvent(EventChainPostVisitor& aVisitor)
{
  return PostHandleEventForAnchors(aVisitor);
}

bool
HTMLAreaElement::IsLink(nsIURI** aURI) const
{
  return IsHTMLLink(aURI);
}

void
HTMLAreaElement::GetLinkTarget(nsAString& aTarget)
{
  GetAttr(kNameSpaceID_None, nsGkAtoms::target, aTarget);
  if (aTarget.IsEmpty()) {
    GetBaseTarget(aTarget);
  }
}

nsDOMTokenList*
HTMLAreaElement::RelList()
{
  if (!mRelList) {
    mRelList = new nsDOMTokenList(this, nsGkAtoms::rel,
                                  HTMLAnchorElement::sSupportedRelValues);
  }
  return mRelList;
}

nsresult
HTMLAreaElement::BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                            nsIContent* aBindingParent,
                            bool aCompileEventHandlers)
{
  Link::ResetLinkState(false, Link::ElementHasHref());
  nsresult rv = nsGenericHTMLElement::BindToTree(aDocument, aParent,
                                                 aBindingParent,
                                                 aCompileEventHandlers);
  NS_ENSURE_SUCCESS(rv, rv);

  nsIDocument* doc = GetComposedDoc();
  if (doc) {
    doc->RegisterPendingLinkUpdate(this);
  }
  return rv;
}

void
HTMLAreaElement::UnbindFromTree(bool aDeep, bool aNullParent)
{
  // If this link is ever reinserted into a document, it might
  // be under a different xml:base, so forget the cached state now.
  Link::ResetLinkState(false, Link::ElementHasHref());

  nsGenericHTMLElement::UnbindFromTree(aDeep, aNullParent);
}

nsresult
HTMLAreaElement::AfterSetAttr(int32_t aNamespaceID, nsAtom* aName,
                              const nsAttrValue* aValue,
                              const nsAttrValue* aOldValue,
                              nsIPrincipal* aSubjectPrincipal,
                              bool aNotify)
{
  if (aNamespaceID == kNameSpaceID_None) {
    // This must happen after the attribute is set. We will need the updated
    // attribute value because notifying the document that content states have
    // changed will call IntrinsicState, which will try to get updated
    // information about the visitedness from Link.
    if (aName == nsGkAtoms::href) {
      Link::ResetLinkState(aNotify, !!aValue);
    }
  }

  return nsGenericHTMLElement::AfterSetAttr(aNamespaceID, aName, aValue,
                                            aOldValue, aSubjectPrincipal, aNotify);
}

void
HTMLAreaElement::ToString(nsAString& aSource)
{
  GetHref(aSource);
}

already_AddRefed<nsIURI>
HTMLAreaElement::GetHrefURI() const
{
  return GetHrefURIForAnchors();
}

EventStates
HTMLAreaElement::IntrinsicState() const
{
  return Link::LinkState() | nsGenericHTMLElement::IntrinsicState();
}

void
HTMLAreaElement::AddSizeOfExcludingThis(nsWindowSizes& aSizes,
                                        size_t* aNodeSize) const
{
  nsGenericHTMLElement::AddSizeOfExcludingThis(aSizes, aNodeSize);
  *aNodeSize += Link::SizeOfExcludingThis(aSizes.mState);
}

JSObject*
HTMLAreaElement::WrapNode(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return HTMLAreaElement_Binding::Wrap(aCx, this, aGivenProto);
}

} // namespace dom
} // namespace mozilla
