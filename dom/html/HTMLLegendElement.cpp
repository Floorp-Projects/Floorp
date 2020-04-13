/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLLegendElement.h"
#include "mozilla/dom/HTMLLegendElementBinding.h"
#include "nsFocusManager.h"
#include "nsIFrame.h"

NS_IMPL_NS_NEW_HTML_ELEMENT(Legend)

namespace mozilla {
namespace dom {

HTMLLegendElement::~HTMLLegendElement() = default;

NS_IMPL_ELEMENT_CLONE(HTMLLegendElement)

nsIContent* HTMLLegendElement::GetFieldSet() const {
  nsIContent* parent = GetParent();

  if (parent && parent->IsHTMLElement(nsGkAtoms::fieldset)) {
    return parent;
  }

  return nullptr;
}

bool HTMLLegendElement::ParseAttribute(int32_t aNamespaceID, nsAtom* aAttribute,
                                       const nsAString& aValue,
                                       nsIPrincipal* aMaybeScriptedPrincipal,
                                       nsAttrValue& aResult) {
  // this contains center, because IE4 does
  static const nsAttrValue::EnumTable kAlignTable[] = {
      {"left", LegendAlignValue::Left},
      {"right", LegendAlignValue::Right},
      {"center", LegendAlignValue::Center},
      {nullptr, 0}};

  if (aAttribute == nsGkAtoms::align && aNamespaceID == kNameSpaceID_None) {
    return aResult.ParseEnumValue(aValue, kAlignTable, false);
  }

  return nsGenericHTMLElement::ParseAttribute(aNamespaceID, aAttribute, aValue,
                                              aMaybeScriptedPrincipal, aResult);
}

nsChangeHint HTMLLegendElement::GetAttributeChangeHint(const nsAtom* aAttribute,
                                                       int32_t aModType) const {
  nsChangeHint retval =
      nsGenericHTMLElement::GetAttributeChangeHint(aAttribute, aModType);
  if (aAttribute == nsGkAtoms::align) {
    retval |= NS_STYLE_HINT_REFLOW;
  }
  return retval;
}

nsresult HTMLLegendElement::BindToTree(BindContext& aContext,
                                       nsINode& aParent) {
  return nsGenericHTMLElement::BindToTree(aContext, aParent);
}

void HTMLLegendElement::UnbindFromTree(bool aNullParent) {
  nsGenericHTMLElement::UnbindFromTree(aNullParent);
}

void HTMLLegendElement::Focus(const FocusOptions& aOptions,
                              const mozilla::dom::CallerType aCallerType,
                              ErrorResult& aError) {
  nsIFrame* frame = GetPrimaryFrame();
  if (!frame) {
    return;
  }

  int32_t tabIndex;
  if (frame->IsFocusable(&tabIndex, false)) {
    nsGenericHTMLElement::Focus(aOptions, aCallerType, aError);
    return;
  }

  // If the legend isn't focusable, focus whatever is focusable following
  // the legend instead, bug 81481.
  nsIFocusManager* fm = nsFocusManager::GetFocusManager();
  if (!fm) {
    return;
  }

  RefPtr<Element> result;
  aError = fm->MoveFocus(
      nullptr, this, nsIFocusManager::MOVEFOCUS_FORWARD,
      nsIFocusManager::FLAG_NOPARENTFRAME |
          nsFocusManager::FocusOptionsToFocusManagerFlags(aOptions),
      getter_AddRefs(result));
}

bool HTMLLegendElement::PerformAccesskey(bool aKeyCausesActivation,
                                         bool aIsTrustedEvent) {
  FocusOptions options;
  ErrorResult rv;

  Focus(options, CallerType::System, rv);
  return NS_SUCCEEDED(rv.StealNSResult());
}

already_AddRefed<HTMLFormElement> HTMLLegendElement::GetForm() {
  Element* form = GetFormElement();
  MOZ_ASSERT_IF(form, form->IsHTMLElement(nsGkAtoms::form));
  RefPtr<HTMLFormElement> ret = static_cast<HTMLFormElement*>(form);
  return ret.forget();
}

JSObject* HTMLLegendElement::WrapNode(JSContext* aCx,
                                      JS::Handle<JSObject*> aGivenProto) {
  return HTMLLegendElement_Binding::Wrap(aCx, this, aGivenProto);
}

}  // namespace dom
}  // namespace mozilla
