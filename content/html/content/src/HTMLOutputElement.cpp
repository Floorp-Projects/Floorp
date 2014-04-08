/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLOutputElement.h"

#include "mozAutoDocUpdate.h"
#include "mozilla/EventStates.h"
#include "mozilla/dom/HTMLFormElement.h"
#include "mozilla/dom/HTMLOutputElementBinding.h"
#include "nsContentUtils.h"
#include "nsDOMSettableTokenList.h"
#include "nsFormSubmission.h"

NS_IMPL_NS_NEW_HTML_ELEMENT(Output)

namespace mozilla {
namespace dom {

HTMLOutputElement::HTMLOutputElement(already_AddRefed<nsINodeInfo>& aNodeInfo)
  : nsGenericHTMLFormElement(aNodeInfo)
  , mValueModeFlag(eModeDefault)
{
  AddMutationObserver(this);

  // We start out valid and ui-valid (since we have no form).
  AddStatesSilently(NS_EVENT_STATE_VALID | NS_EVENT_STATE_MOZ_UI_VALID);
}

HTMLOutputElement::~HTMLOutputElement()
{
}

NS_IMPL_CYCLE_COLLECTION_CLASS(HTMLOutputElement)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(HTMLOutputElement,
                                                nsGenericHTMLFormElement)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mValidity)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mTokenList)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(HTMLOutputElement,
                                                  nsGenericHTMLFormElement)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mValidity)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mTokenList)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_ADDREF_INHERITED(HTMLOutputElement, Element)
NS_IMPL_RELEASE_INHERITED(HTMLOutputElement, Element)

NS_INTERFACE_TABLE_HEAD_CYCLE_COLLECTION_INHERITED(HTMLOutputElement)
  NS_INTERFACE_TABLE_INHERITED2(HTMLOutputElement,
                                nsIMutationObserver,
                                nsIConstraintValidation)
NS_INTERFACE_TABLE_TAIL_INHERITING(nsGenericHTMLFormElement)

NS_IMPL_ELEMENT_CLONE(HTMLOutputElement)

void
HTMLOutputElement::SetCustomValidity(const nsAString& aError)
{
  nsIConstraintValidation::SetCustomValidity(aError);

  UpdateState(true);
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

EventStates
HTMLOutputElement::IntrinsicState() const
{
  EventStates states = nsGenericHTMLFormElement::IntrinsicState();

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

void
HTMLOutputElement::GetValue(nsAString& aValue)
{
  if (!nsContentUtils::GetNodeTextContent(this, true, aValue)) {
    NS_RUNTIMEABORT("OOM");
  }
}

void
HTMLOutputElement::SetValue(const nsAString& aValue, ErrorResult& aRv)
{
  mValueModeFlag = eModeValue;
  aRv = nsContentUtils::SetNodeTextContent(this, aValue, true);
}

void
HTMLOutputElement::SetDefaultValue(const nsAString& aDefaultValue, ErrorResult& aRv)
{
  mDefaultValue = aDefaultValue;
  if (mValueModeFlag == eModeDefault) {
    aRv = nsContentUtils::SetNodeTextContent(this, mDefaultValue, true);
  }
}

nsDOMSettableTokenList*
HTMLOutputElement::HtmlFor()
{
  if (!mTokenList) {
    mTokenList = new nsDOMSettableTokenList(this, nsGkAtoms::_for);
  }
  return mTokenList;
}

void HTMLOutputElement::DescendantsChanged()
{
  if (mValueModeFlag == eModeDefault) {
    if (!nsContentUtils::GetNodeTextContent(this, true, mDefaultValue)) {
      NS_RUNTIMEABORT("OOM");
    }
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
HTMLOutputElement::WrapNode(JSContext* aCx)
{
  return HTMLOutputElementBinding::Wrap(aCx, this);
}

} // namespace dom
} // namespace mozilla
