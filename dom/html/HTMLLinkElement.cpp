/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLLinkElement.h"

#include "mozilla/AsyncEventDispatcher.h"
#include "mozilla/Attributes.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/EventStates.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/Preferences.h"
#include "mozilla/dom/HTMLLinkElementBinding.h"
#include "nsContentUtils.h"
#include "nsGenericHTMLElement.h"
#include "nsGkAtoms.h"
#include "nsDOMTokenList.h"
#include "nsIContentInlines.h"
#include "nsIDocument.h"
#include "nsIDOMEvent.h"
#include "nsIDOMStyleSheet.h"
#include "nsINode.h"
#include "nsIStyleSheetLinkingElement.h"
#include "nsIURL.h"
#include "nsPIDOMWindow.h"
#include "nsReadableUtils.h"
#include "nsStyleConsts.h"
#include "nsStyleLinkElement.h"
#include "nsUnicharUtils.h"

#define LINK_ELEMENT_FLAG_BIT(n_) \
  NODE_FLAG_BIT(ELEMENT_TYPE_SPECIFIC_BITS_OFFSET + (n_))

// Link element specific bits
enum {
  // Indicates that a DNS Prefetch has been requested from this Link element.
  HTML_LINK_DNS_PREFETCH_REQUESTED = LINK_ELEMENT_FLAG_BIT(0),

  // Indicates that a DNS Prefetch was added to the deferral queue
  HTML_LINK_DNS_PREFETCH_DEFERRED =  LINK_ELEMENT_FLAG_BIT(1)
};

#undef LINK_ELEMENT_FLAG_BIT

ASSERT_NODE_FLAGS_SPACE(ELEMENT_TYPE_SPECIFIC_BITS_OFFSET + 2);

NS_IMPL_NS_NEW_HTML_ELEMENT(Link)

namespace mozilla {
namespace dom {

HTMLLinkElement::HTMLLinkElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo)
  : nsGenericHTMLElement(aNodeInfo)
  , Link(this)
{
}

HTMLLinkElement::~HTMLLinkElement()
{
}

NS_IMPL_CYCLE_COLLECTION_CLASS(HTMLLinkElement)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(HTMLLinkElement,
                                                  nsGenericHTMLElement)
  tmp->nsStyleLinkElement::Traverse(cb);
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mRelList)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(HTMLLinkElement,
                                                nsGenericHTMLElement)
  tmp->nsStyleLinkElement::Unlink();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mRelList)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_ISUPPORTS_CYCLE_COLLECTION_INHERITED(HTMLLinkElement,
                                             nsGenericHTMLElement,
                                             nsIStyleSheetLinkingElement,
                                             Link)

NS_IMPL_ELEMENT_CLONE(HTMLLinkElement)

bool
HTMLLinkElement::Disabled()
{
  StyleSheet* ss = GetSheet();
  return ss && ss->Disabled();
}

void
HTMLLinkElement::SetDisabled(bool aDisabled)
{
  if (StyleSheet* ss = GetSheet()) {
    ss->SetDisabled(aDisabled);
  }
}

void
HTMLLinkElement::OnDNSPrefetchRequested()
{
  UnsetFlags(HTML_LINK_DNS_PREFETCH_DEFERRED);
  SetFlags(HTML_LINK_DNS_PREFETCH_REQUESTED);
}

void
HTMLLinkElement::OnDNSPrefetchDeferred()
{
  UnsetFlags(HTML_LINK_DNS_PREFETCH_REQUESTED);
  SetFlags(HTML_LINK_DNS_PREFETCH_DEFERRED);
}

bool
HTMLLinkElement::HasDeferredDNSPrefetchRequest()
{
  return HasFlag(HTML_LINK_DNS_PREFETCH_DEFERRED);
}

nsresult
HTMLLinkElement::BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                            nsIContent* aBindingParent,
                            bool aCompileEventHandlers)
{
  Link::ResetLinkState(false, Link::ElementHasHref());

  nsresult rv = nsGenericHTMLElement::BindToTree(aDocument, aParent,
                                                 aBindingParent,
                                                 aCompileEventHandlers);
  NS_ENSURE_SUCCESS(rv, rv);

  // Link must be inert in ShadowRoot.
  if (aDocument && !GetContainingShadow()) {
    aDocument->RegisterPendingLinkUpdate(this);
  }

  if (IsInComposedDoc()) {
    TryDNSPrefetchOrPreconnectOrPrefetchOrPreloadOrPrerender();
  }

  void (HTMLLinkElement::*update)() = &HTMLLinkElement::UpdateStyleSheetInternal;
  nsContentUtils::AddScriptRunner(
    NewRunnableMethod("dom::HTMLLinkElement::BindToTree", this, update));

  CreateAndDispatchEvent(aDocument, NS_LITERAL_STRING("DOMLinkAdded"));

  return rv;
}

void
HTMLLinkElement::LinkAdded()
{
  CreateAndDispatchEvent(OwnerDoc(), NS_LITERAL_STRING("DOMLinkAdded"));
}

void
HTMLLinkElement::LinkRemoved()
{
  CreateAndDispatchEvent(OwnerDoc(), NS_LITERAL_STRING("DOMLinkRemoved"));
}

void
HTMLLinkElement::UnbindFromTree(bool aDeep, bool aNullParent)
{
  // Cancel any DNS prefetches
  // Note: Must come before ResetLinkState.  If called after, it will recreate
  // mCachedURI based on data that is invalid - due to a call to GetHostname.
  CancelDNSPrefetch(HTML_LINK_DNS_PREFETCH_DEFERRED,
                    HTML_LINK_DNS_PREFETCH_REQUESTED);
  CancelPrefetchOrPreload();

  // If this link is ever reinserted into a document, it might
  // be under a different xml:base, so forget the cached state now.
  Link::ResetLinkState(false, Link::ElementHasHref());

  // If this is reinserted back into the document it will not be
  // from the parser.
  nsCOMPtr<nsIDocument> oldDoc = GetUncomposedDoc();

  // Check for a ShadowRoot because link elements are inert in a
  // ShadowRoot.
  ShadowRoot* oldShadowRoot = GetBindingParent() ?
    GetBindingParent()->GetShadowRoot() : nullptr;

  CreateAndDispatchEvent(oldDoc, NS_LITERAL_STRING("DOMLinkRemoved"));
  nsGenericHTMLElement::UnbindFromTree(aDeep, aNullParent);

  UpdateStyleSheetInternal(oldDoc, oldShadowRoot);
}

bool
HTMLLinkElement::ParseAttribute(int32_t aNamespaceID,
                                nsAtom* aAttribute,
                                const nsAString& aValue,
                                nsIPrincipal* aMaybeScriptedPrincipal,
                                nsAttrValue& aResult)
{
  if (aNamespaceID == kNameSpaceID_None) {
    if (aAttribute == nsGkAtoms::crossorigin) {
      ParseCORSValue(aValue, aResult);
      return true;
    }

    if (aAttribute == nsGkAtoms::as) {
      ParseAsValue(aValue, aResult);
      return true;
    }

    if (aAttribute == nsGkAtoms::sizes) {
      aResult.ParseAtomArray(aValue);
      return true;
    }

    if (aAttribute == nsGkAtoms::integrity) {
      aResult.ParseStringOrAtom(aValue);
      return true;
    }
  }

  return nsGenericHTMLElement::ParseAttribute(aNamespaceID, aAttribute, aValue,
                                              aMaybeScriptedPrincipal, aResult);
}

void
HTMLLinkElement::CreateAndDispatchEvent(nsIDocument* aDoc,
                                        const nsAString& aEventName)
{
  if (!aDoc)
    return;

  // In the unlikely case that both rev is specified *and* rel=stylesheet,
  // this code will cause the event to fire, on the principle that maybe the
  // page really does want to specify that its author is a stylesheet. Since
  // this should never actually happen and the performance hit is minimal,
  // doing the "right" thing costs virtually nothing here, even if it doesn't
  // make much sense.
  static nsIContent::AttrValuesArray strings[] =
    {&nsGkAtoms::_empty, &nsGkAtoms::stylesheet, nullptr};

  if (!nsContentUtils::HasNonEmptyAttr(this, kNameSpaceID_None,
                                       nsGkAtoms::rev) &&
      FindAttrValueIn(kNameSpaceID_None, nsGkAtoms::rel,
                      strings, eIgnoreCase) != ATTR_VALUE_NO_MATCH)
    return;

  RefPtr<AsyncEventDispatcher> asyncDispatcher =
    new AsyncEventDispatcher(this, aEventName, true, true);
  // Always run async in order to avoid running script when the content
  // sink isn't expecting it.
  asyncDispatcher->PostDOMEvent();
}

nsresult
HTMLLinkElement::BeforeSetAttr(int32_t aNameSpaceID, nsAtom* aName,
                               const nsAttrValueOrString* aValue, bool aNotify)
{
  if (aNameSpaceID == kNameSpaceID_None &&
      (aName == nsGkAtoms::href || aName == nsGkAtoms::rel)) {
    CancelDNSPrefetch(HTML_LINK_DNS_PREFETCH_DEFERRED,
                      HTML_LINK_DNS_PREFETCH_REQUESTED);
    CancelPrefetchOrPreload();
  }

  return nsGenericHTMLElement::BeforeSetAttr(aNameSpaceID, aName,
                                             aValue, aNotify);
}

nsresult
HTMLLinkElement::AfterSetAttr(int32_t aNameSpaceID, nsAtom* aName,
                              const nsAttrValue* aValue,
                              const nsAttrValue* aOldValue,
                              nsIPrincipal* aSubjectPrincipal,
                              bool aNotify)
{
  // It's safe to call ResetLinkState here because our new attr value has
  // already been set or unset.  ResetLinkState needs the updated attribute
  // value because notifying the document that content states have changed will
  // call IntrinsicState, which will try to get updated information about the
  // visitedness from Link.
  if (aName == nsGkAtoms::href && kNameSpaceID_None == aNameSpaceID) {
    bool hasHref = aValue;
    Link::ResetLinkState(!!aNotify, hasHref);
    if (IsInUncomposedDoc()) {
      CreateAndDispatchEvent(OwnerDoc(), NS_LITERAL_STRING("DOMLinkChanged"));
    }
  }

  if (aNameSpaceID == kNameSpaceID_None && aName == nsGkAtoms::href) {
    mTriggeringPrincipal = nsContentUtils::GetAttrTriggeringPrincipal(
        this, aValue ? aValue->GetStringValue() : EmptyString(),
        aSubjectPrincipal);
  }

  if (aValue) {
    if (aNameSpaceID == kNameSpaceID_None &&
        (aName == nsGkAtoms::href ||
         aName == nsGkAtoms::rel ||
         aName == nsGkAtoms::title ||
         aName == nsGkAtoms::media ||
         aName == nsGkAtoms::type ||
         aName == nsGkAtoms::as ||
         aName == nsGkAtoms::crossorigin)) {
      bool dropSheet = false;
      if (aName == nsGkAtoms::rel) {
        nsAutoString value;
        aValue->ToString(value);
        uint32_t linkTypes = nsStyleLinkElement::ParseLinkTypes(value);
        if (GetSheet()) {
          dropSheet = !(linkTypes & nsStyleLinkElement::eSTYLESHEET);
        }
      }

      if ((aName == nsGkAtoms::rel || aName == nsGkAtoms::href) &&
          IsInComposedDoc()) {
        TryDNSPrefetchOrPreconnectOrPrefetchOrPreloadOrPrerender();
      }

      if ((aName == nsGkAtoms::as || aName == nsGkAtoms::type ||
           aName == nsGkAtoms::crossorigin || aName == nsGkAtoms::media) &&
          IsInComposedDoc()) {
        UpdatePreload(aName, aValue, aOldValue);
      }

      UpdateStyleSheetInternal(nullptr, nullptr,
                               dropSheet ||
                               (aName == nsGkAtoms::title ||
                                aName == nsGkAtoms::media ||
                                aName == nsGkAtoms::type));
    }
  } else {
    // Since removing href or rel makes us no longer link to a
    // stylesheet, force updates for those too.
    if (aNameSpaceID == kNameSpaceID_None) {
      if (aName == nsGkAtoms::href ||
          aName == nsGkAtoms::rel ||
          aName == nsGkAtoms::title ||
          aName == nsGkAtoms::media ||
          aName == nsGkAtoms::type) {
        UpdateStyleSheetInternal(nullptr, nullptr, true);
      }
      if ((aName == nsGkAtoms::as || aName == nsGkAtoms::type ||
           aName == nsGkAtoms::crossorigin || aName == nsGkAtoms::media) &&
          IsInComposedDoc()) {
        UpdatePreload(aName, aValue, aOldValue);
      }
    }
  }

  return nsGenericHTMLElement::AfterSetAttr(aNameSpaceID, aName, aValue,
                                            aOldValue, aSubjectPrincipal, aNotify);
}

nsresult
HTMLLinkElement::GetEventTargetParent(EventChainPreVisitor& aVisitor)
{
  return GetEventTargetParentForAnchors(aVisitor);
}

nsresult
HTMLLinkElement::PostHandleEvent(EventChainPostVisitor& aVisitor)
{
  return PostHandleEventForAnchors(aVisitor);
}

bool
HTMLLinkElement::IsLink(nsIURI** aURI) const
{
  return IsHTMLLink(aURI);
}

void
HTMLLinkElement::GetLinkTarget(nsAString& aTarget)
{
  GetAttr(kNameSpaceID_None, nsGkAtoms::target, aTarget);
  if (aTarget.IsEmpty()) {
    GetBaseTarget(aTarget);
  }
}

static const DOMTokenListSupportedToken sSupportedRelValues[] = {
  // Keep this in sync with ToLinkMask in nsStyleLinkElement.cpp.
  // "preload" must come first because it can be disabled.
  "preload",
  "prefetch",
  "dns-prefetch",
  "stylesheet",
  "next",
  "alternate",
  "preconnect",
  "icon",
  "search",
  nullptr
};

nsDOMTokenList*
HTMLLinkElement::RelList()
{
  if (!mRelList) {
    if (Preferences::GetBool("network.preload")) {
      mRelList = new nsDOMTokenList(this, nsGkAtoms::rel, sSupportedRelValues);
    } else {
      mRelList = new nsDOMTokenList(this, nsGkAtoms::rel, &sSupportedRelValues[1]);
    }
  }
  return mRelList;
}

already_AddRefed<nsIURI>
HTMLLinkElement::GetHrefURI() const
{
  return GetHrefURIForAnchors();
}

already_AddRefed<nsIURI>
HTMLLinkElement::GetStyleSheetURL(bool* aIsInline, nsIPrincipal** aTriggeringPrincipal)
{
  *aIsInline = false;
  *aTriggeringPrincipal = nullptr;

  nsAutoString href;
  GetAttr(kNameSpaceID_None, nsGkAtoms::href, href);
  if (href.IsEmpty()) {
    return nullptr;
  }

  nsCOMPtr<nsIPrincipal> prin = mTriggeringPrincipal;
  prin.forget(aTriggeringPrincipal);

  nsCOMPtr<nsIURI> uri = Link::GetURI();
  return uri.forget();
}

void
HTMLLinkElement::GetStyleSheetInfo(nsAString& aTitle,
                                   nsAString& aType,
                                   nsAString& aMedia,
                                   bool* aIsScoped,
                                   bool* aIsAlternate)
{
  aTitle.Truncate();
  aType.Truncate();
  aMedia.Truncate();
  *aIsScoped = false;
  *aIsAlternate = false;

  nsAutoString rel;
  GetAttr(kNameSpaceID_None, nsGkAtoms::rel, rel);
  uint32_t linkTypes = nsStyleLinkElement::ParseLinkTypes(rel);
  // Is it a stylesheet link?
  if (!(linkTypes & nsStyleLinkElement::eSTYLESHEET)) {
    return;
  }

  nsAutoString title;
  GetAttr(kNameSpaceID_None, nsGkAtoms::title, title);
  title.CompressWhitespace();
  aTitle.Assign(title);

  // If alternate, does it have title?
  if (linkTypes & nsStyleLinkElement::eALTERNATE) {
    if (aTitle.IsEmpty()) { // alternates must have title
      return;
    } else {
      *aIsAlternate = true;
    }
  }

  GetAttr(kNameSpaceID_None, nsGkAtoms::media, aMedia);
  // The HTML5 spec is formulated in terms of the CSSOM spec, which specifies
  // that media queries should be ASCII lowercased during serialization.
  nsContentUtils::ASCIIToLower(aMedia);

  nsAutoString mimeType;
  nsAutoString notUsed;
  GetAttr(kNameSpaceID_None, nsGkAtoms::type, aType);
  nsContentUtils::SplitMimeType(aType, mimeType, notUsed);
  if (!mimeType.IsEmpty() && !mimeType.LowerCaseEqualsLiteral("text/css")) {
    return;
  }

  // If we get here we assume that we're loading a css file, so set the
  // type to 'text/css'
  aType.AssignLiteral("text/css");
}

CORSMode
HTMLLinkElement::GetCORSMode() const
{
  return AttrValueToCORSMode(GetParsedAttr(nsGkAtoms::crossorigin));
}

EventStates
HTMLLinkElement::IntrinsicState() const
{
  return Link::LinkState() | nsGenericHTMLElement::IntrinsicState();
}

void
HTMLLinkElement::AddSizeOfExcludingThis(nsWindowSizes& aSizes,
                                        size_t* aNodeSize) const
{
  nsGenericHTMLElement::AddSizeOfExcludingThis(aSizes, aNodeSize);
  *aNodeSize += Link::SizeOfExcludingThis(aSizes.mState);
}

JSObject*
HTMLLinkElement::WrapNode(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return HTMLLinkElementBinding::Wrap(aCx, this, aGivenProto);
}

void
HTMLLinkElement::GetAs(nsAString& aResult)
{
  GetEnumAttr(nsGkAtoms::as, EmptyCString().get(), aResult);
}

} // namespace dom
} // namespace mozilla
