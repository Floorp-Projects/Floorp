/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLMarqueeElement.h"
#include "nsGenericHTMLElement.h"
#include "nsStyleConsts.h"
#include "nsMappedAttributes.h"
#include "mozilla/AsyncEventDispatcher.h"
#include "mozilla/dom/HTMLMarqueeElementBinding.h"
#include "mozilla/dom/CustomEvent.h"
// This is to pick up the definition of FunctionStringCallback:
#include "mozilla/dom/DataTransferItemBinding.h"
#include "mozilla/dom/ShadowRoot.h"

NS_IMPL_NS_NEW_HTML_ELEMENT(Marquee)

namespace mozilla {
namespace dom {

HTMLMarqueeElement::~HTMLMarqueeElement() {}

NS_IMPL_CYCLE_COLLECTION_INHERITED(HTMLMarqueeElement, nsGenericHTMLElement,
                                   mStartStopCallback)

NS_IMPL_ISUPPORTS_CYCLE_COLLECTION_INHERITED_0(HTMLMarqueeElement,
                                               nsGenericHTMLElement)

NS_IMPL_ELEMENT_CLONE(HTMLMarqueeElement)

static const nsAttrValue::EnumTable kBehaviorTable[] = {
    {"scroll", 1}, {"slide", 2}, {"alternate", 3}, {nullptr, 0}};

// Default behavior value is "scroll".
static const nsAttrValue::EnumTable* kDefaultBehavior = &kBehaviorTable[0];

static const nsAttrValue::EnumTable kDirectionTable[] = {
    {"left", 1}, {"right", 2}, {"up", 3}, {"down", 4}, {nullptr, 0}};

// Default direction value is "left".
static const nsAttrValue::EnumTable* kDefaultDirection = &kDirectionTable[0];

bool HTMLMarqueeElement::IsEventAttributeNameInternal(nsAtom* aName) {
  return nsContentUtils::IsEventAttributeName(
      aName, EventNameType_HTML | EventNameType_HTMLMarqueeOnly);
}

JSObject* HTMLMarqueeElement::WrapNode(JSContext* aCx,
                                       JS::Handle<JSObject*> aGivenProto) {
  return dom::HTMLMarqueeElement_Binding::Wrap(aCx, this, aGivenProto);
}

nsresult HTMLMarqueeElement::BindToTree(nsIDocument* aDocument,
                                        nsIContent* aParent,
                                        nsIContent* aBindingParent) {
  nsresult rv =
      nsGenericHTMLElement::BindToTree(aDocument, aParent, aBindingParent);
  NS_ENSURE_SUCCESS(rv, rv);

  if (nsContentUtils::IsUAWidgetEnabled() && IsInComposedDoc()) {
    AttachAndSetUAShadowRoot();
    NotifyUAWidgetSetupOrChange();
  }

  return rv;
}

void HTMLMarqueeElement::UnbindFromTree(bool aDeep, bool aNullParent) {
  if (nsContentUtils::IsUAWidgetEnabled() && IsInComposedDoc()) {
    // We don't want to unattach the shadow root because it used to
    // contain a <slot>.
    NotifyUAWidgetTeardown(UnattachShadowRoot::No);
  }

  nsGenericHTMLElement::UnbindFromTree(aDeep, aNullParent);
}

void HTMLMarqueeElement::SetStartStopCallback(
    FunctionStringCallback* aCallback) {
  mStartStopCallback = aCallback;
}

void HTMLMarqueeElement::GetBehavior(nsAString& aValue) {
  GetEnumAttr(nsGkAtoms::behavior, kDefaultBehavior->tag, aValue);
}

void HTMLMarqueeElement::GetDirection(nsAString& aValue) {
  GetEnumAttr(nsGkAtoms::direction, kDefaultDirection->tag, aValue);
}

bool HTMLMarqueeElement::ParseAttribute(int32_t aNamespaceID,
                                        nsAtom* aAttribute,
                                        const nsAString& aValue,
                                        nsIPrincipal* aMaybeScriptedPrincipal,
                                        nsAttrValue& aResult) {
  if (aNamespaceID == kNameSpaceID_None) {
    if ((aAttribute == nsGkAtoms::width) || (aAttribute == nsGkAtoms::height)) {
      return aResult.ParseSpecialIntValue(aValue);
    }
    if (aAttribute == nsGkAtoms::bgcolor) {
      return aResult.ParseColor(aValue);
    }
    if (aAttribute == nsGkAtoms::behavior) {
      return aResult.ParseEnumValue(aValue, kBehaviorTable, false,
                                    kDefaultBehavior);
    }
    if (aAttribute == nsGkAtoms::direction) {
      return aResult.ParseEnumValue(aValue, kDirectionTable, false,
                                    kDefaultDirection);
    }
    if ((aAttribute == nsGkAtoms::hspace) ||
        (aAttribute == nsGkAtoms::vspace)) {
      return aResult.ParseIntWithBounds(aValue, 0);
    }

    if (aAttribute == nsGkAtoms::loop) {
      return aResult.ParseIntValue(aValue);
    }

    if (aAttribute == nsGkAtoms::scrollamount ||
        aAttribute == nsGkAtoms::scrolldelay) {
      return aResult.ParseNonNegativeIntValue(aValue);
    }
  }

  return nsGenericHTMLElement::ParseAttribute(aNamespaceID, aAttribute, aValue,
                                              aMaybeScriptedPrincipal, aResult);
}

nsresult HTMLMarqueeElement::AfterSetAttr(int32_t aNameSpaceID, nsAtom* aName,
                                          const nsAttrValue* aValue,
                                          const nsAttrValue* aOldValue,
                                          nsIPrincipal* aMaybeScriptedPrincipal,
                                          bool aNotify) {
  if (nsContentUtils::IsUAWidgetEnabled() && IsInComposedDoc() &&
      aNameSpaceID == kNameSpaceID_None && aName == nsGkAtoms::direction) {
    NotifyUAWidgetSetupOrChange();
  }
  return nsGenericHTMLElement::AfterSetAttr(
      aNameSpaceID, aName, aValue, aOldValue, aMaybeScriptedPrincipal, aNotify);
}

void HTMLMarqueeElement::MapAttributesIntoRule(
    const nsMappedAttributes* aAttributes, MappedDeclarations& aDecls) {
  nsGenericHTMLElement::MapImageMarginAttributeInto(aAttributes, aDecls);
  nsGenericHTMLElement::MapImageSizeAttributesInto(aAttributes, aDecls);
  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aDecls);
  nsGenericHTMLElement::MapBGColorInto(aAttributes, aDecls);
}

NS_IMETHODIMP_(bool)
HTMLMarqueeElement::IsAttributeMapped(const nsAtom* aAttribute) const {
  static const MappedAttributeEntry* const map[] = {
      sImageMarginSizeAttributeMap, sBackgroundColorAttributeMap,
      sCommonAttributeMap};
  return FindAttributeDependence(aAttribute, map);
}

nsMapRuleToAttributesFunc HTMLMarqueeElement::GetAttributeMappingFunction()
    const {
  return &MapAttributesIntoRule;
}

void HTMLMarqueeElement::DispatchEventToShadowRoot(
    const nsAString& aEventTypeArg) {
  // Dispatch the event to the UA Widget Shadow Root, make it inaccessible to
  // document.
  RefPtr<nsINode> shadow = GetShadowRoot();
  MOZ_ASSERT(shadow);
  RefPtr<Event> event = new Event(shadow, nullptr, nullptr);
  event->InitEvent(aEventTypeArg, false, false);
  event->SetTrusted(true);
  shadow->DispatchEvent(*event, IgnoreErrors());
}

void HTMLMarqueeElement::Start() {
  if (GetShadowRoot()) {
    DispatchEventToShadowRoot(NS_LITERAL_STRING("marquee-start"));
  } else if (mStartStopCallback) {
    mStartStopCallback->Call(NS_LITERAL_STRING("start"));
  }
}

void HTMLMarqueeElement::Stop() {
  if (GetShadowRoot()) {
    DispatchEventToShadowRoot(NS_LITERAL_STRING("marquee-stop"));
  } else if (mStartStopCallback) {
    mStartStopCallback->Call(NS_LITERAL_STRING("stop"));
  }
}

}  // namespace dom
}  // namespace mozilla
