/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLOutputElement.h"

#include "mozAutoDocUpdate.h"
#include "mozilla/dom/HTMLFormElement.h"
#include "mozilla/dom/HTMLOutputElementBinding.h"
#include "nsContentUtils.h"
#include "nsDOMTokenList.h"

NS_IMPL_NS_NEW_HTML_ELEMENT_CHECK_PARSER(Output)

namespace mozilla::dom {

HTMLOutputElement::HTMLOutputElement(
    already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo,
    FromParser aFromParser)
    : nsGenericHTMLFormControlElement(std::move(aNodeInfo),
                                      FormControlType::Output),
      mValueModeFlag(eModeDefault),
      mIsDoneAddingChildren(!aFromParser) {
  AddMutationObserver(this);

  // <output> is always barred from constraint validation since it is not a
  // submittable element.
  SetBarredFromConstraintValidation(true);
}

HTMLOutputElement::~HTMLOutputElement() = default;

NS_IMPL_CYCLE_COLLECTION_INHERITED(HTMLOutputElement,
                                   nsGenericHTMLFormControlElement, mValidity,
                                   mTokenList)

NS_IMPL_ISUPPORTS_CYCLE_COLLECTION_INHERITED(HTMLOutputElement,
                                             nsGenericHTMLFormControlElement,
                                             nsIMutationObserver,
                                             nsIConstraintValidation)

NS_IMPL_ELEMENT_CLONE(HTMLOutputElement)

void HTMLOutputElement::SetCustomValidity(const nsAString& aError) {
  ConstraintValidation::SetCustomValidity(aError);
}

NS_IMETHODIMP
HTMLOutputElement::Reset() {
  mValueModeFlag = eModeDefault;
  // We can't pass mDefaultValue, because it'll be truncated when
  // the element's descendants are changed, so pass a copy instead.
  const nsAutoString currentDefaultValue(mDefaultValue);
  return nsContentUtils::SetNodeTextContent(this, currentDefaultValue, true);
}

bool HTMLOutputElement::ParseAttribute(int32_t aNamespaceID, nsAtom* aAttribute,
                                       const nsAString& aValue,
                                       nsIPrincipal* aMaybeScriptedPrincipal,
                                       nsAttrValue& aResult) {
  if (aNamespaceID == kNameSpaceID_None) {
    if (aAttribute == nsGkAtoms::_for) {
      aResult.ParseAtomArray(aValue);
      return true;
    }
  }

  return nsGenericHTMLFormControlElement::ParseAttribute(
      aNamespaceID, aAttribute, aValue, aMaybeScriptedPrincipal, aResult);
}

void HTMLOutputElement::DoneAddingChildren(bool aHaveNotified) {
  mIsDoneAddingChildren = true;
  // We should update DefaultValue, after parsing is done.
  DescendantsChanged();
}

void HTMLOutputElement::GetValue(nsAString& aValue) const {
  nsContentUtils::GetNodeTextContent(this, true, aValue);
}

void HTMLOutputElement::SetValue(const nsAString& aValue, ErrorResult& aRv) {
  mValueModeFlag = eModeValue;
  aRv = nsContentUtils::SetNodeTextContent(this, aValue, true);
}

void HTMLOutputElement::SetDefaultValue(const nsAString& aDefaultValue,
                                        ErrorResult& aRv) {
  mDefaultValue = aDefaultValue;
  if (mValueModeFlag == eModeDefault) {
    // We can't pass mDefaultValue, because it'll be truncated when
    // the element's descendants are changed.
    aRv = nsContentUtils::SetNodeTextContent(this, aDefaultValue, true);
  }
}

nsDOMTokenList* HTMLOutputElement::HtmlFor() {
  if (!mTokenList) {
    mTokenList = new nsDOMTokenList(this, nsGkAtoms::_for);
  }
  return mTokenList;
}

void HTMLOutputElement::DescendantsChanged() {
  if (mIsDoneAddingChildren && mValueModeFlag == eModeDefault) {
    nsContentUtils::GetNodeTextContent(this, true, mDefaultValue);
  }
}

// nsIMutationObserver

void HTMLOutputElement::CharacterDataChanged(nsIContent* aContent,
                                             const CharacterDataChangeInfo&) {
  DescendantsChanged();
}

void HTMLOutputElement::ContentAppended(nsIContent* aFirstNewContent) {
  DescendantsChanged();
}

void HTMLOutputElement::ContentInserted(nsIContent* aChild) {
  DescendantsChanged();
}

void HTMLOutputElement::ContentRemoved(nsIContent* aChild,
                                       nsIContent* aPreviousSibling) {
  DescendantsChanged();
}

JSObject* HTMLOutputElement::WrapNode(JSContext* aCx,
                                      JS::Handle<JSObject*> aGivenProto) {
  return HTMLOutputElement_Binding::Wrap(aCx, this, aGivenProto);
}

}  // namespace mozilla::dom
