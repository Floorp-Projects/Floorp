/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLLegendElement.h"
#include "mozilla/dom/ElementBinding.h"
#include "mozilla/dom/HTMLLegendElementBinding.h"
#include "nsFocusManager.h"
#include "nsIFrame.h"

NS_IMPL_NS_NEW_HTML_ELEMENT(Legend)

namespace mozilla::dom {

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
                              const CallerType aCallerType,
                              ErrorResult& aError) {
  nsIFrame* frame = GetPrimaryFrame();
  if (!frame) {
    return;
  }

  if (frame->IsFocusable()) {
    nsGenericHTMLElement::Focus(aOptions, aCallerType, aError);
    return;
  }

  // If the legend isn't focusable, focus whatever is focusable following
  // the legend instead, bug 81481.
  nsFocusManager* fm = nsFocusManager::GetFocusManager();
  if (!fm) {
    return;
  }

  RefPtr<Element> result;
  aError = fm->MoveFocus(nullptr, this, nsIFocusManager::MOVEFOCUS_FORWARD,
                         nsIFocusManager::FLAG_NOPARENTFRAME |
                             nsFocusManager::ProgrammaticFocusFlags(aOptions),
                         getter_AddRefs(result));
}

bool HTMLLegendElement::PerformAccesskey(bool aKeyCausesActivation,
                                         bool aIsTrustedEvent) {
  FocusOptions options;
  ErrorResult rv;

  Focus(options, CallerType::System, rv);
  return NS_SUCCEEDED(rv.StealNSResult());
}

HTMLLegendElement::LegendAlignValue HTMLLegendElement::LogicalAlign(
    mozilla::WritingMode aCBWM) const {
  const nsAttrValue* attr = GetParsedAttr(nsGkAtoms::align);
  if (!attr || attr->Type() != nsAttrValue::eEnum) {
    return LegendAlignValue::InlineStart;
  }

  auto value = static_cast<LegendAlignValue>(attr->GetEnumValue());
  switch (value) {
    case LegendAlignValue::Left:
      return aCBWM.IsBidiLTR() ? LegendAlignValue::InlineStart
                               : LegendAlignValue::InlineEnd;
    case LegendAlignValue::Right:
      return aCBWM.IsBidiLTR() ? LegendAlignValue::InlineEnd
                               : LegendAlignValue::InlineStart;
    default:
      return value;
  }
}

HTMLFormElement* HTMLLegendElement::GetForm() const {
  nsCOMPtr<nsIFormControl> fieldsetControl = do_QueryInterface(GetFieldSet());
  return fieldsetControl ? fieldsetControl->GetForm() : nullptr;
}

JSObject* HTMLLegendElement::WrapNode(JSContext* aCx,
                                      JS::Handle<JSObject*> aGivenProto) {
  return HTMLLegendElement_Binding::Wrap(aCx, this, aGivenProto);
}

}  // namespace mozilla::dom
