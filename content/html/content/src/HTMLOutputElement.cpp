/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLOutputElement.h"

#include "mozilla/dom/HTMLOutputElementBinding.h"
#include "nsFormSubmission.h"
#include "nsDOMSettableTokenList.h"
#include "nsEventStates.h"
#include "mozAutoDocUpdate.h"
#include "mozilla/dom/HTMLFormElement.h"

NS_IMPL_NS_NEW_HTML_ELEMENT(Output)

namespace mozilla {
namespace dom {

HTMLOutputElement::HTMLOutputElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : nsGenericHTMLFormElement(aNodeInfo)
  , mValueModeFlag(eModeDefault)
{
  AddMutationObserver(this);

  // We start out valid and ui-valid (since we have no form).
  AddStatesSilently(NS_EVENT_STATE_VALID | NS_EVENT_STATE_MOZ_UI_VALID);
}

HTMLOutputElement::~HTMLOutputElement()
{
  if (mTokenList) {
    mTokenList->DropReference();
  }
}

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(HTMLOutputElement,
                                                nsGenericHTMLFormElement)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mValidity)
  if (tmp->mTokenList) {
    tmp->mTokenList->DropReference();
    NS_IMPL_CYCLE_COLLECTION_UNLINK(mTokenList)
  }
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(HTMLOutputElement,
                                                  nsGenericHTMLFormElement)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mValidity)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mTokenList)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_ADDREF_INHERITED(HTMLOutputElement, Element)
NS_IMPL_RELEASE_INHERITED(HTMLOutputElement, Element)

NS_INTERFACE_TABLE_HEAD_CYCLE_COLLECTION_INHERITED(HTMLOutputElement)
  NS_HTML_CONTENT_INTERFACES(nsGenericHTMLFormElement)
  NS_INTERFACE_TABLE_INHERITED3(HTMLOutputElement,
                                nsIDOMHTMLOutputElement,
                                nsIMutationObserver,
                                nsIConstraintValidation)
  NS_INTERFACE_TABLE_TO_MAP_SEGUE
NS_ELEMENT_INTERFACE_MAP_END

NS_IMPL_ELEMENT_CLONE(HTMLOutputElement)


NS_IMPL_STRING_ATTR(HTMLOutputElement, Name, name)

// nsIConstraintValidation
NS_IMPL_NSICONSTRAINTVALIDATION_EXCEPT_SETCUSTOMVALIDITY(HTMLOutputElement)

NS_IMETHODIMP
HTMLOutputElement::SetCustomValidity(const nsAString& aError)
{
  nsIConstraintValidation::SetCustomValidity(aError);

  UpdateState(true);

  return NS_OK;
}

NS_IMETHODIMP
HTMLOutputElement::Reset()
{
  mValueModeFlag = eModeDefault;
  return nsContentUtils::SetNodeTextContent(this, mDefaultValue, true);
}

NS_IMETHODIMP
HTMLOutputElement::SubmitNamesValues(nsFormSubmission* aFormSubmission)
{
  // The output element is not submittable.
  return NS_OK;
}

bool
HTMLOutputElement::ParseAttribute(int32_t aNamespaceID, nsIAtom* aAttribute,
                                  const nsAString& aValue, nsAttrValue& aResult)
{
  if (aNamespaceID == kNameSpaceID_None) {
    if (aAttribute == nsGkAtoms::_for) {
      aResult.ParseAtomArray(aValue);
      return true;
    }
  }

  return nsGenericHTMLFormElement::ParseAttribute(aNamespaceID, aAttribute,
                                                  aValue, aResult);
}

nsEventStates
HTMLOutputElement::IntrinsicState() const
{
  nsEventStates states = nsGenericHTMLFormElement::IntrinsicState();

  // We don't have to call IsCandidateForConstraintValidation()
  // because <output> can't be barred from constraint validation.
  if (IsValid()) {
    states |= NS_EVENT_STATE_VALID;
    if (!mForm || !mForm->HasAttr(kNameSpaceID_None, nsGkAtoms::novalidate)) {
      states |= NS_EVENT_STATE_MOZ_UI_VALID;
    }
  } else {
    states |= NS_EVENT_STATE_INVALID;
    if (!mForm || !mForm->HasAttr(kNameSpaceID_None, nsGkAtoms::novalidate)) {
      states |= NS_EVENT_STATE_MOZ_UI_INVALID;
    }
  }

  return states;
}

nsresult
HTMLOutputElement::BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              bool aCompileEventHandlers)
{
  nsresult rv = nsGenericHTMLFormElement::BindToTree(aDocument, aParent,
                                                     aBindingParent,
                                                     aCompileEventHandlers);
  NS_ENSURE_SUCCESS(rv, rv);

  // Unfortunately, we can actually end up having to change our state
  // as a result of being bound to a tree even from the parser: we
  // might end up a in a novalidate form, and unlike other form
  // controls that on its own is enough to make change ui-valid state.
  // So just go ahead and update our state now.
  UpdateState(false);

  return rv;
}

NS_IMETHODIMP
HTMLOutputElement::GetForm(nsIDOMHTMLFormElement** aForm)
{
  return nsGenericHTMLFormElement::GetForm(aForm);
}

NS_IMETHODIMP
HTMLOutputElement::GetType(nsAString& aType)
{
  aType.AssignLiteral("output");
  return NS_OK;
}

NS_IMETHODIMP
HTMLOutputElement::GetValue(nsAString& aValue)
{
  nsContentUtils::GetNodeTextContent(this, true, aValue);
  return NS_OK;
}

NS_IMETHODIMP
HTMLOutputElement::SetValue(const nsAString& aValue)
{
  mValueModeFlag = eModeValue;
  return nsContentUtils::SetNodeTextContent(this, aValue, true);
}

NS_IMETHODIMP
HTMLOutputElement::GetDefaultValue(nsAString& aDefaultValue)
{
  aDefaultValue = mDefaultValue;
  return NS_OK;
}

NS_IMETHODIMP
HTMLOutputElement::SetDefaultValue(const nsAString& aDefaultValue)
{
  mDefaultValue = aDefaultValue;
  if (mValueModeFlag == eModeDefault) {
    return nsContentUtils::SetNodeTextContent(this, mDefaultValue, true);
  }

  return NS_OK;
}

nsDOMSettableTokenList*
HTMLOutputElement::HtmlFor()
{
  if (!mTokenList) {
    mTokenList = new nsDOMSettableTokenList(this, nsGkAtoms::_for);
  }
  return mTokenList;
}

NS_IMETHODIMP
HTMLOutputElement::GetHtmlFor(nsISupports** aResult)
{
  NS_ADDREF(*aResult = HtmlFor());
  return NS_OK;
}

void HTMLOutputElement::DescendantsChanged()
{
  if (mValueModeFlag == eModeDefault) {
    nsContentUtils::GetNodeTextContent(this, true, mDefaultValue);
  }
}

// nsIMutationObserver

void HTMLOutputElement::CharacterDataChanged(nsIDocument* aDocument,
                                             nsIContent* aContent,
                                             CharacterDataChangeInfo* aInfo)
{
  DescendantsChanged();
}

void HTMLOutputElement::ContentAppended(nsIDocument* aDocument,
                                        nsIContent* aContainer,
                                        nsIContent* aFirstNewContent,
                                        int32_t aNewIndexInContainer)
{
  DescendantsChanged();
}

void HTMLOutputElement::ContentInserted(nsIDocument* aDocument,
                                        nsIContent* aContainer,
                                        nsIContent* aChild,
                                        int32_t aIndexInContainer)
{
  DescendantsChanged();
}

void HTMLOutputElement::ContentRemoved(nsIDocument* aDocument,
                                       nsIContent* aContainer,
                                       nsIContent* aChild,
                                       int32_t aIndexInContainer,
                                       nsIContent* aPreviousSibling)
{
  DescendantsChanged();
}

JSObject*
HTMLOutputElement::WrapNode(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return HTMLOutputElementBinding::Wrap(aCx, aScope, this);
}

} // namespace dom
} // namespace mozilla
