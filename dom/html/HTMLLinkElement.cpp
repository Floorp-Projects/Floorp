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
#include "mozilla/dom/DocumentInlines.h"
#include "mozilla/dom/HTMLLinkElementBinding.h"
#include "nsContentUtils.h"
#include "nsGenericHTMLElement.h"
#include "nsGkAtoms.h"
#include "nsDOMTokenList.h"
#include "nsIContentInlines.h"
#include "mozilla/dom/Document.h"
#include "nsINode.h"
#include "nsIStyleSheetLinkingElement.h"
#include "nsIURL.h"
#include "nsPIDOMWindow.h"
#include "nsReadableUtils.h"
#include "nsStyleConsts.h"
#include "nsStyleLinkElement.h"
#include "nsUnicharUtils.h"
#include "nsWindowSizes.h"
#include "nsIContentPolicy.h"
#include "nsMimeTypes.h"
#include "imgLoader.h"
#include "MediaContainerType.h"
#include "DecoderDoctorDiagnostics.h"
#include "DecoderTraits.h"
#include "MediaList.h"
#include "nsAttrValueInlines.h"

#define LINK_ELEMENT_FLAG_BIT(n_) \
  NODE_FLAG_BIT(ELEMENT_TYPE_SPECIFIC_BITS_OFFSET + (n_))

// Link element specific bits
enum {
  // Indicates that a DNS Prefetch has been requested from this Link element.
  HTML_LINK_DNS_PREFETCH_REQUESTED = LINK_ELEMENT_FLAG_BIT(0),

  // Indicates that a DNS Prefetch was added to the deferral queue
  HTML_LINK_DNS_PREFETCH_DEFERRED = LINK_ELEMENT_FLAG_BIT(1)
};

#undef LINK_ELEMENT_FLAG_BIT

ASSERT_NODE_FLAGS_SPACE(ELEMENT_TYPE_SPECIFIC_BITS_OFFSET + 2);

NS_IMPL_NS_NEW_HTML_ELEMENT(Link)

namespace mozilla {
namespace dom {

HTMLLinkElement::HTMLLinkElement(
    already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
    : nsGenericHTMLElement(std::move(aNodeInfo)), Link(this) {}

HTMLLinkElement::~HTMLLinkElement() {}

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
                                             nsIStyleSheetLinkingElement, Link)

NS_IMPL_ELEMENT_CLONE(HTMLLinkElement)

bool HTMLLinkElement::Disabled() const {
  if (StaticPrefs::dom_link_disabled_attribute_enabled()) {
    return GetBoolAttr(nsGkAtoms::disabled);
  }
  StyleSheet* ss = GetSheet();
  return ss && ss->Disabled();
}

void HTMLLinkElement::SetDisabled(bool aDisabled, ErrorResult& aRv) {
  if (StaticPrefs::dom_link_disabled_attribute_enabled()) {
    return SetHTMLBoolAttr(nsGkAtoms::disabled, aDisabled, aRv);
  }
  if (StyleSheet* ss = GetSheet()) {
    ss->SetDisabled(aDisabled);
  }
}

void HTMLLinkElement::OnDNSPrefetchRequested() {
  UnsetFlags(HTML_LINK_DNS_PREFETCH_DEFERRED);
  SetFlags(HTML_LINK_DNS_PREFETCH_REQUESTED);
}

void HTMLLinkElement::OnDNSPrefetchDeferred() {
  UnsetFlags(HTML_LINK_DNS_PREFETCH_REQUESTED);
  SetFlags(HTML_LINK_DNS_PREFETCH_DEFERRED);
}

bool HTMLLinkElement::HasDeferredDNSPrefetchRequest() {
  return HasFlag(HTML_LINK_DNS_PREFETCH_DEFERRED);
}

nsresult HTMLLinkElement::BindToTree(Document* aDocument, nsIContent* aParent,
                                     nsIContent* aBindingParent) {
  Link::ResetLinkState(false, Link::ElementHasHref());

  nsresult rv =
      nsGenericHTMLElement::BindToTree(aDocument, aParent, aBindingParent);
  NS_ENSURE_SUCCESS(rv, rv);

  if (Document* doc = GetComposedDoc()) {
    if (!doc->NodePrincipal()->IsSystemPrincipal()) {
      doc->RegisterPendingLinkUpdate(this);
    }
    TryDNSPrefetchOrPreconnectOrPrefetchOrPreloadOrPrerender();
  }

  void (HTMLLinkElement::*update)() =
      &HTMLLinkElement::UpdateStyleSheetInternal;
  nsContentUtils::AddScriptRunner(
      NewRunnableMethod("dom::HTMLLinkElement::BindToTree", this, update));

  if (aDocument && AttrValueIs(kNameSpaceID_None, nsGkAtoms::rel,
                               nsGkAtoms::localization, eIgnoreCase)) {
    aDocument->LocalizationLinkAdded(this);
  }

  CreateAndDispatchEvent(aDocument, NS_LITERAL_STRING("DOMLinkAdded"));

  return rv;
}

void HTMLLinkElement::LinkAdded() {
  CreateAndDispatchEvent(OwnerDoc(), NS_LITERAL_STRING("DOMLinkAdded"));
}

void HTMLLinkElement::LinkRemoved() {
  CreateAndDispatchEvent(OwnerDoc(), NS_LITERAL_STRING("DOMLinkRemoved"));
}

void HTMLLinkElement::UnbindFromTree(bool aDeep, bool aNullParent) {
  // Cancel any DNS prefetches
  // Note: Must come before ResetLinkState.  If called after, it will recreate
  // mCachedURI based on data that is invalid - due to a call to GetHostname.
  CancelDNSPrefetch(HTML_LINK_DNS_PREFETCH_DEFERRED,
                    HTML_LINK_DNS_PREFETCH_REQUESTED);
  CancelPrefetchOrPreload();

  // Without removing the link state we risk a dangling pointer
  // in the mStyledLinks hashtable
  Link::ResetLinkState(false, Link::ElementHasHref());

  // If this is reinserted back into the document it will not be
  // from the parser.
  Document* oldDoc = GetUncomposedDoc();
  ShadowRoot* oldShadowRoot = GetContainingShadow();

  // We want to update the localization but only if the
  // link is removed from a DOM change, and not
  // because the document is going away.
  bool ignore;
  if (oldDoc && oldDoc->GetScriptHandlingObject(ignore) &&
      this->AttrValueIs(kNameSpaceID_None, nsGkAtoms::rel,
                        nsGkAtoms::localization, eIgnoreCase)) {
    oldDoc->LocalizationLinkRemoved(this);
  }

  CreateAndDispatchEvent(oldDoc, NS_LITERAL_STRING("DOMLinkRemoved"));
  nsGenericHTMLElement::UnbindFromTree(aDeep, aNullParent);

  Unused << UpdateStyleSheetInternal(oldDoc, oldShadowRoot);
}

bool HTMLLinkElement::ParseAttribute(int32_t aNamespaceID, nsAtom* aAttribute,
                                     const nsAString& aValue,
                                     nsIPrincipal* aMaybeScriptedPrincipal,
                                     nsAttrValue& aResult) {
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

void HTMLLinkElement::CreateAndDispatchEvent(Document* aDoc,
                                             const nsAString& aEventName) {
  if (!aDoc) return;

  // In the unlikely case that both rev is specified *and* rel=stylesheet,
  // this code will cause the event to fire, on the principle that maybe the
  // page really does want to specify that its author is a stylesheet. Since
  // this should never actually happen and the performance hit is minimal,
  // doing the "right" thing costs virtually nothing here, even if it doesn't
  // make much sense.
  static Element::AttrValuesArray strings[] = {nsGkAtoms::_empty,
                                               nsGkAtoms::stylesheet, nullptr};

  if (!nsContentUtils::HasNonEmptyAttr(this, kNameSpaceID_None,
                                       nsGkAtoms::rev) &&
      FindAttrValueIn(kNameSpaceID_None, nsGkAtoms::rel, strings,
                      eIgnoreCase) != ATTR_VALUE_NO_MATCH)
    return;

  RefPtr<AsyncEventDispatcher> asyncDispatcher = new AsyncEventDispatcher(
      this, aEventName, CanBubble::eYes, ChromeOnlyDispatch::eYes);
  // Always run async in order to avoid running script when the content
  // sink isn't expecting it.
  asyncDispatcher->PostDOMEvent();
}

nsresult HTMLLinkElement::BeforeSetAttr(int32_t aNameSpaceID, nsAtom* aName,
                                        const nsAttrValueOrString* aValue,
                                        bool aNotify) {
  if (aNameSpaceID == kNameSpaceID_None &&
      (aName == nsGkAtoms::href || aName == nsGkAtoms::rel)) {
    CancelDNSPrefetch(HTML_LINK_DNS_PREFETCH_DEFERRED,
                      HTML_LINK_DNS_PREFETCH_REQUESTED);
    CancelPrefetchOrPreload();
  }

  return nsGenericHTMLElement::BeforeSetAttr(aNameSpaceID, aName, aValue,
                                             aNotify);
}

nsresult HTMLLinkElement::AfterSetAttr(int32_t aNameSpaceID, nsAtom* aName,
                                       const nsAttrValue* aValue,
                                       const nsAttrValue* aOldValue,
                                       nsIPrincipal* aSubjectPrincipal,
                                       bool aNotify) {
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

  // If a link's `rel` attribute was changed from or to `localization`,
  // update the list of localization links.
  if (aNameSpaceID == kNameSpaceID_None && aName == nsGkAtoms::rel) {
    Document* doc = GetComposedDoc();
    if (doc) {
      if ((aValue && aValue->Equals(nsGkAtoms::localization, eIgnoreCase)) &&
          (!aOldValue ||
           !aOldValue->Equals(nsGkAtoms::localization, eIgnoreCase))) {
        doc->LocalizationLinkAdded(this);
      } else if ((aOldValue &&
                  aOldValue->Equals(nsGkAtoms::localization, eIgnoreCase)) &&
                 (!aValue ||
                  !aValue->Equals(nsGkAtoms::localization, eIgnoreCase))) {
        doc->LocalizationLinkRemoved(this);
      }
    }
  }

  // If the link has `rel=localization` and its `href` attribute is changed,
  // update the list of localization links.
  if (aNameSpaceID == kNameSpaceID_None && aName == nsGkAtoms::href &&
      AttrValueIs(kNameSpaceID_None, nsGkAtoms::rel, nsGkAtoms::localization,
                  eIgnoreCase)) {
    Document* doc = GetComposedDoc();
    if (doc) {
      if (aOldValue) {
        doc->LocalizationLinkRemoved(this);
      }
      if (aValue) {
        doc->LocalizationLinkAdded(this);
      }
    }
  }

  if (aValue) {
    if (aNameSpaceID == kNameSpaceID_None &&
        (aName == nsGkAtoms::href || aName == nsGkAtoms::rel ||
         aName == nsGkAtoms::title || aName == nsGkAtoms::media ||
         aName == nsGkAtoms::type || aName == nsGkAtoms::as ||
         aName == nsGkAtoms::crossorigin ||
         (aName == nsGkAtoms::disabled &&
          StaticPrefs::dom_link_disabled_attribute_enabled()))) {
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

      const bool forceUpdate =
          dropSheet || aName == nsGkAtoms::title || aName == nsGkAtoms::media ||
          aName == nsGkAtoms::type || aName == nsGkAtoms::disabled;

      Unused << UpdateStyleSheetInternal(
          nullptr, nullptr, forceUpdate ? ForceUpdate::Yes : ForceUpdate::No);
    }
  } else {
    if (aNameSpaceID == kNameSpaceID_None) {
      if (aName == nsGkAtoms::disabled &&
          StaticPrefs::dom_link_disabled_attribute_enabled()) {
        mExplicitlyEnabled = true;
      }
      // Since removing href or rel makes us no longer link to a stylesheet,
      // force updates for those too.
      if (aName == nsGkAtoms::href || aName == nsGkAtoms::rel ||
          aName == nsGkAtoms::title || aName == nsGkAtoms::media ||
          aName == nsGkAtoms::type ||
          (aName == nsGkAtoms::disabled &&
           StaticPrefs::dom_link_disabled_attribute_enabled())) {
        Unused << UpdateStyleSheetInternal(nullptr, nullptr, ForceUpdate::Yes);
      }
      if ((aName == nsGkAtoms::as || aName == nsGkAtoms::type ||
           aName == nsGkAtoms::crossorigin || aName == nsGkAtoms::media) &&
          IsInComposedDoc()) {
        UpdatePreload(aName, aValue, aOldValue);
      }
    }
  }

  return nsGenericHTMLElement::AfterSetAttr(
      aNameSpaceID, aName, aValue, aOldValue, aSubjectPrincipal, aNotify);
}

void HTMLLinkElement::GetEventTargetParent(EventChainPreVisitor& aVisitor) {
  GetEventTargetParentForAnchors(aVisitor);
}

nsresult HTMLLinkElement::PostHandleEvent(EventChainPostVisitor& aVisitor) {
  return PostHandleEventForAnchors(aVisitor);
}

bool HTMLLinkElement::IsLink(nsIURI** aURI) const { return IsHTMLLink(aURI); }

void HTMLLinkElement::GetLinkTarget(nsAString& aTarget) {
  GetAttr(kNameSpaceID_None, nsGkAtoms::target, aTarget);
  if (aTarget.IsEmpty()) {
    GetBaseTarget(aTarget);
  }
}

static const DOMTokenListSupportedToken sSupportedRelValues[] = {
    // Keep this in sync with ToLinkMask in nsStyleLinkElement.cpp.
    // "preload" must come first because it can be disabled.
    "preload",   "prefetch",   "dns-prefetch", "stylesheet", "next",
    "alternate", "preconnect", "icon",         "search",     nullptr};

nsDOMTokenList* HTMLLinkElement::RelList() {
  if (!mRelList) {
    if (Preferences::GetBool("network.preload")) {
      mRelList = new nsDOMTokenList(this, nsGkAtoms::rel, sSupportedRelValues);
    } else {
      mRelList =
          new nsDOMTokenList(this, nsGkAtoms::rel, &sSupportedRelValues[1]);
    }
  }
  return mRelList;
}

already_AddRefed<nsIURI> HTMLLinkElement::GetHrefURI() const {
  return GetHrefURIForAnchors();
}

Maybe<nsStyleLinkElement::SheetInfo> HTMLLinkElement::GetStyleSheetInfo() {
  nsAutoString rel;
  GetAttr(kNameSpaceID_None, nsGkAtoms::rel, rel);
  uint32_t linkTypes = nsStyleLinkElement::ParseLinkTypes(rel);
  if (!(linkTypes & nsStyleLinkElement::eSTYLESHEET)) {
    return Nothing();
  }

  if (!IsCSSMimeTypeAttribute(*this)) {
    return Nothing();
  }

  if (StaticPrefs::dom_link_disabled_attribute_enabled() && Disabled()) {
    return Nothing();
  }

  nsAutoString title;
  nsAutoString media;
  GetTitleAndMediaForElement(*this, title, media);

  bool alternate = linkTypes & nsStyleLinkElement::eALTERNATE;
  if (alternate && title.IsEmpty()) {
    // alternates must have title.
    return Nothing();
  }

  nsAutoString href;
  GetAttr(kNameSpaceID_None, nsGkAtoms::href, href);
  if (href.IsEmpty()) {
    return Nothing();
  }

  nsCOMPtr<nsIURI> uri = Link::GetURI();
  nsCOMPtr<nsIPrincipal> prin = mTriggeringPrincipal;
  return Some(SheetInfo{
      *OwnerDoc(),
      this,
      uri.forget(),
      prin.forget(),
      GetReferrerPolicyAsEnum(),
      GetCORSMode(),
      title,
      media,
      alternate ? HasAlternateRel::Yes : HasAlternateRel::No,
      IsInline::No,
      mExplicitlyEnabled ? IsExplicitlyEnabled::Yes : IsExplicitlyEnabled::No,
  });
}

EventStates HTMLLinkElement::IntrinsicState() const {
  return Link::LinkState() | nsGenericHTMLElement::IntrinsicState();
}

void HTMLLinkElement::AddSizeOfExcludingThis(nsWindowSizes& aSizes,
                                             size_t* aNodeSize) const {
  nsGenericHTMLElement::AddSizeOfExcludingThis(aSizes, aNodeSize);
  *aNodeSize += Link::SizeOfExcludingThis(aSizes.mState);
}

JSObject* HTMLLinkElement::WrapNode(JSContext* aCx,
                                    JS::Handle<JSObject*> aGivenProto) {
  return HTMLLinkElement_Binding::Wrap(aCx, this, aGivenProto);
}

void HTMLLinkElement::GetAs(nsAString& aResult) {
  GetEnumAttr(nsGkAtoms::as, EmptyCString().get(), aResult);
}

// We will use official mime-types from:
// https://www.iana.org/assignments/media-types/media-types.xhtml#font
// We do not support old deprecated mime-types for preload feature.
// (We currectly do not support font/collection)
static uint32_t StyleLinkElementFontMimeTypesNum = 5;
static const char* StyleLinkElementFontMimeTypes[] = {
    "font/otf", "font/sfnt", "font/ttf", "font/woff", "font/woff2"};

bool IsFontMimeType(const nsAString& aType) {
  if (aType.IsEmpty()) {
    return true;
  }
  for (uint32_t i = 0; i < StyleLinkElementFontMimeTypesNum; i++) {
    if (aType.EqualsASCII(StyleLinkElementFontMimeTypes[i])) {
      return true;
    }
  }
  return false;
}

bool HTMLLinkElement::CheckPreloadAttrs(const nsAttrValue& aAs,
                                        const nsAString& aType,
                                        const nsAString& aMedia,
                                        Document* aDocument) {
  nsContentPolicyType policyType = Link::AsValueToContentPolicy(aAs);
  if (policyType == nsIContentPolicy::TYPE_INVALID) {
    return false;
  }

  // Check if media attribute is valid.
  if (!aMedia.IsEmpty()) {
    RefPtr<MediaList> mediaList = MediaList::Create(aMedia);
    if (!mediaList->Matches(*aDocument)) {
      return false;
    }
  }

  if (aType.IsEmpty()) {
    return true;
  }

  nsString type = nsString(aType);
  ToLowerCase(type);

  if (policyType == nsIContentPolicy::TYPE_OTHER) {
    return true;

  } else if (policyType == nsIContentPolicy::TYPE_MEDIA) {
    if (aAs.GetEnumValue() == DESTINATION_TRACK) {
      if (type.EqualsASCII("text/vtt")) {
        return true;
      } else {
        return false;
      }
    }
    Maybe<MediaContainerType> mimeType = MakeMediaContainerType(aType);
    if (!mimeType) {
      return false;
    }
    DecoderDoctorDiagnostics diagnostics;
    CanPlayStatus status =
        DecoderTraits::CanHandleContainerType(*mimeType, &diagnostics);
    // Preload if this return CANPLAY_YES and CANPLAY_MAYBE.
    if (status == CANPLAY_NO) {
      return false;
    } else {
      return true;
    }

  } else if (policyType == nsIContentPolicy::TYPE_FONT) {
    if (IsFontMimeType(type)) {
      return true;
    } else {
      return false;
    }

  } else if (policyType == nsIContentPolicy::TYPE_IMAGE) {
    if (imgLoader::SupportImageWithMimeType(
            NS_ConvertUTF16toUTF8(type).get(),
            AcceptedMimeTypes::IMAGES_AND_DOCUMENTS)) {
      return true;
    } else {
      return false;
    }

  } else if (policyType == nsIContentPolicy::TYPE_SCRIPT) {
    if (nsContentUtils::IsJavascriptMIMEType(type)) {
      return true;
    } else {
      return false;
    }

  } else if (policyType == nsIContentPolicy::TYPE_STYLESHEET) {
    if (type.EqualsASCII("text/css")) {
      return true;
    } else {
      return false;
    }
  }
  return false;
}

}  // namespace dom
}  // namespace mozilla
