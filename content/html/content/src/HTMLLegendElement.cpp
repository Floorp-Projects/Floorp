/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLLegendElement.h"
#include "mozilla/dom/HTMLLegendElementBinding.h"
#include "nsIDOMHTMLFormElement.h"
#include "nsFocusManager.h"
#include "nsIFrame.h"

NS_IMPL_NS_NEW_HTML_ELEMENT(Legend)

namespace mozilla {
namespace dom {


HTMLLegendElement::~HTMLLegendElement()
{
}


NS_IMPL_ADDREF_INHERITED(HTMLLegendElement, Element)
NS_IMPL_RELEASE_INHERITED(HTMLLegendElement, Element)


// QueryInterface implementation for HTMLLegendElement
NS_INTERFACE_TABLE_HEAD(HTMLLegendElement)
  NS_HTML_CONTENT_INTERFACES(nsGenericHTMLElement)
  NS_HTML_CONTENT_INTERFACE_TABLE1(HTMLLegendElement, nsIDOMHTMLLegendElement)
  NS_HTML_CONTENT_INTERFACE_TABLE_TO_MAP_SEGUE(HTMLLegendElement,
                                               nsGenericHTMLElement)
NS_HTML_CONTENT_INTERFACE_MAP_END


// nsIDOMHTMLLegendElement


NS_IMPL_ELEMENT_CLONE(HTMLLegendElement)


NS_IMETHODIMP
HTMLLegendElement::GetForm(nsIDOMHTMLFormElement** aForm)
{
  Element* form = GetFormElement();

  return form ? CallQueryInterface(form, aForm) : NS_OK;
}


NS_IMPL_STRING_ATTR(HTMLLegendElement, Align, align)

// this contains center, because IE4 does
static const nsAttrValue::EnumTable kAlignTable[] = {
  { "left", NS_STYLE_TEXT_ALIGN_LEFT },
  { "right", NS_STYLE_TEXT_ALIGN_RIGHT },
  { "center", NS_STYLE_TEXT_ALIGN_CENTER },
  { "bottom", NS_STYLE_VERTICAL_ALIGN_BOTTOM },
  { "top", NS_STYLE_VERTICAL_ALIGN_TOP },
  { 0 }
};

nsIContent*
HTMLLegendElement::GetFieldSet()
{
  nsIContent* parent = GetParent();

  if (parent && parent->IsHTML(nsGkAtoms::fieldset)) {
    return parent;
  }

  return nullptr;
}

bool
HTMLLegendElement::ParseAttribute(int32_t aNamespaceID,
                                  nsIAtom* aAttribute,
                                  const nsAString& aValue,
                                  nsAttrValue& aResult)
{
  if (aAttribute == nsGkAtoms::align && aNamespaceID == kNameSpaceID_None) {
    return aResult.ParseEnumValue(aValue, kAlignTable, false);
  }

  return nsGenericHTMLElement::ParseAttribute(aNamespaceID, aAttribute, aValue,
                                              aResult);
}

nsChangeHint
HTMLLegendElement::GetAttributeChangeHint(const nsIAtom* aAttribute,
                                          int32_t aModType) const
{
  nsChangeHint retval =
      nsGenericHTMLElement::GetAttributeChangeHint(aAttribute, aModType);
  if (aAttribute == nsGkAtoms::align) {
    NS_UpdateHint(retval, NS_STYLE_HINT_REFLOW);
  }
  return retval;
}

nsresult
HTMLLegendElement::SetAttr(int32_t aNameSpaceID, nsIAtom* aAttribute,
                           nsIAtom* aPrefix, const nsAString& aValue,
                           bool aNotify)
{
  return nsGenericHTMLElement::SetAttr(aNameSpaceID, aAttribute,
                                       aPrefix, aValue, aNotify);
}
nsresult
HTMLLegendElement::UnsetAttr(int32_t aNameSpaceID, nsIAtom* aAttribute,
                             bool aNotify)
{
  return nsGenericHTMLElement::UnsetAttr(aNameSpaceID, aAttribute, aNotify);
}

nsresult
HTMLLegendElement::BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              bool aCompileEventHandlers)
{
  return nsGenericHTMLElement::BindToTree(aDocument, aParent,
                                          aBindingParent,
                                          aCompileEventHandlers);
}

void
HTMLLegendElement::UnbindFromTree(bool aDeep, bool aNullParent)
{
  nsGenericHTMLElement::UnbindFromTree(aDeep, aNullParent);
}

void
HTMLLegendElement::Focus(ErrorResult& aError)
{
  nsIFrame* frame = GetPrimaryFrame();
  if (!frame) {
    return;
  }

  int32_t tabIndex;
  if (frame->IsFocusable(&tabIndex, false)) {
    nsGenericHTMLElement::Focus(aError);
    return;
  }

  // If the legend isn't focusable, focus whatever is focusable following
  // the legend instead, bug 81481.
  nsIFocusManager* fm = nsFocusManager::GetFocusManager();
  if (!fm) {
    return;
  }

  nsCOMPtr<nsIDOMElement> result;
  aError = fm->MoveFocus(nullptr, this, nsIFocusManager::MOVEFOCUS_FORWARD,
                         nsIFocusManager::FLAG_NOPARENTFRAME,
                         getter_AddRefs(result));
}

void
HTMLLegendElement::PerformAccesskey(bool aKeyCausesActivation,
                                    bool aIsTrustedEvent)
{
  // just use the same behaviour as the focus method
  ErrorResult rv;
  Focus(rv);
}

already_AddRefed<nsHTMLFormElement>
HTMLLegendElement::GetForm()
{
  Element* form = GetFormElement();
  MOZ_ASSERT_IF(form, form->IsHTML(nsGkAtoms::form));
  nsRefPtr<nsHTMLFormElement> ret = static_cast<nsHTMLFormElement*>(form);
  return ret.forget();
}

JSObject*
HTMLLegendElement::WrapNode(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return HTMLLegendElementBinding::Wrap(aCx, aScope, this);
}

} // namespace dom
} // namespace mozilla
