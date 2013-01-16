/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIDOMHTMLOutputElement.h"
#include "nsGenericHTMLElement.h"
#include "nsFormSubmission.h"
#include "nsDOMSettableTokenList.h"
#include "nsStubMutationObserver.h"
#include "nsIConstraintValidation.h"
#include "nsEventStates.h"
#include "mozAutoDocUpdate.h"
#include "nsHTMLFormElement.h"

using namespace mozilla::dom;

class nsHTMLOutputElement : public nsGenericHTMLFormElement,
                            public nsIDOMHTMLOutputElement,
                            public nsStubMutationObserver,
                            public nsIConstraintValidation
{
public:
  using nsIConstraintValidation::GetValidationMessage;

  nsHTMLOutputElement(already_AddRefed<nsINodeInfo> aNodeInfo);
  virtual ~nsHTMLOutputElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE_TO_NSINODE

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT_TO_GENERIC

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT_TO_GENERIC

  // nsIDOMHTMLOutputElement
  NS_DECL_NSIDOMHTMLOUTPUTELEMENT

  // nsIFormControl
  NS_IMETHOD_(uint32_t) GetType() const { return NS_FORM_OUTPUT; }
  NS_IMETHOD Reset();
  NS_IMETHOD SubmitNamesValues(nsFormSubmission* aFormSubmission);

  virtual bool IsDisabled() const { return false; }

  nsresult Clone(nsINodeInfo* aNodeInfo, nsINode** aResult) const;

  bool ParseAttribute(int32_t aNamespaceID, nsIAtom* aAttribute,
                        const nsAString& aValue, nsAttrValue& aResult);

  nsEventStates IntrinsicState() const;

  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                               nsIContent* aBindingParent,
                               bool aCompileEventHandlers);

  // This function is called when a callback function from nsIMutationObserver
  // has to be used to update the defaultValue attribute.
  void DescendantsChanged();

  // nsIMutationObserver
  NS_DECL_NSIMUTATIONOBSERVER_CHARACTERDATACHANGED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTAPPENDED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTINSERTED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTREMOVED

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(nsHTMLOutputElement,
                                           nsGenericHTMLFormElement)

  virtual nsXPCClassInfo* GetClassInfo();

  virtual nsIDOMNode* AsDOMNode() { return this; }
protected:
  enum ValueModeFlag {
    eModeDefault,
    eModeValue
  };

  ValueModeFlag                     mValueModeFlag;
  nsString                          mDefaultValue;
  nsRefPtr<nsDOMSettableTokenList>  mTokenList;
};


NS_IMPL_NS_NEW_HTML_ELEMENT(Output)


nsHTMLOutputElement::nsHTMLOutputElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : nsGenericHTMLFormElement(aNodeInfo)
  , mValueModeFlag(eModeDefault)
{
  AddMutationObserver(this);

  // We start out valid and ui-valid (since we have no form).
  AddStatesSilently(NS_EVENT_STATE_VALID | NS_EVENT_STATE_MOZ_UI_VALID);
}

nsHTMLOutputElement::~nsHTMLOutputElement()
{
  if (mTokenList) {
    mTokenList->DropReference();
  }
}

NS_IMPL_CYCLE_COLLECTION_CLASS(nsHTMLOutputElement)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(nsHTMLOutputElement,
                                                nsGenericHTMLFormElement)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mValidity)
  if (tmp->mTokenList) {
    tmp->mTokenList->DropReference();
    NS_IMPL_CYCLE_COLLECTION_UNLINK(mTokenList)
  }
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(nsHTMLOutputElement,
                                                  nsGenericHTMLFormElement)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mValidity)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mTokenList)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_ADDREF_INHERITED(nsHTMLOutputElement, Element)
NS_IMPL_RELEASE_INHERITED(nsHTMLOutputElement, Element)

DOMCI_NODE_DATA(HTMLOutputElement, nsHTMLOutputElement)

NS_INTERFACE_TABLE_HEAD_CYCLE_COLLECTION_INHERITED(nsHTMLOutputElement)
  NS_HTML_CONTENT_INTERFACE_TABLE3(nsHTMLOutputElement,
                                   nsIDOMHTMLOutputElement,
                                   nsIMutationObserver,
                                   nsIConstraintValidation)
  NS_HTML_CONTENT_INTERFACE_TABLE_TO_MAP_SEGUE(nsHTMLOutputElement,
                                               nsGenericHTMLFormElement)
NS_HTML_CONTENT_INTERFACE_TABLE_TAIL_CLASSINFO(HTMLOutputElement)

NS_IMPL_ELEMENT_CLONE(nsHTMLOutputElement)


NS_IMPL_STRING_ATTR(nsHTMLOutputElement, Name, name)

// nsIConstraintValidation
NS_IMPL_NSICONSTRAINTVALIDATION_EXCEPT_SETCUSTOMVALIDITY(nsHTMLOutputElement)

NS_IMETHODIMP
nsHTMLOutputElement::SetCustomValidity(const nsAString& aError)
{
  nsIConstraintValidation::SetCustomValidity(aError);

  UpdateState(true);

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLOutputElement::Reset()
{
  mValueModeFlag = eModeDefault;
  return nsContentUtils::SetNodeTextContent(this, mDefaultValue, true);
}

NS_IMETHODIMP
nsHTMLOutputElement::SubmitNamesValues(nsFormSubmission* aFormSubmission)
{
  // The output element is not submittable.
  return NS_OK;
}

bool
nsHTMLOutputElement::ParseAttribute(int32_t aNamespaceID, nsIAtom* aAttribute,
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
nsHTMLOutputElement::IntrinsicState() const
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
nsHTMLOutputElement::BindToTree(nsIDocument* aDocument, nsIContent* aParent,
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
nsHTMLOutputElement::GetForm(nsIDOMHTMLFormElement** aForm)
{
  return nsGenericHTMLFormElement::GetForm(aForm);
}

NS_IMETHODIMP
nsHTMLOutputElement::GetType(nsAString& aType)
{
  aType.AssignLiteral("output");
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLOutputElement::GetValue(nsAString& aValue)
{
  nsContentUtils::GetNodeTextContent(this, true, aValue);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLOutputElement::SetValue(const nsAString& aValue)
{
  mValueModeFlag = eModeValue;
  return nsContentUtils::SetNodeTextContent(this, aValue, true);
}

NS_IMETHODIMP
nsHTMLOutputElement::GetDefaultValue(nsAString& aDefaultValue)
{
  aDefaultValue = mDefaultValue;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLOutputElement::SetDefaultValue(const nsAString& aDefaultValue)
{
  mDefaultValue = aDefaultValue;
  if (mValueModeFlag == eModeDefault) {
    return nsContentUtils::SetNodeTextContent(this, mDefaultValue, true);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLOutputElement::GetHtmlFor(nsISupports** aResult)
{
  if (!mTokenList) {
    mTokenList = new nsDOMSettableTokenList(this, nsGkAtoms::_for);
  }

  NS_ADDREF(*aResult = mTokenList);

  return NS_OK;
}

void nsHTMLOutputElement::DescendantsChanged()
{
  if (mValueModeFlag == eModeDefault) {
    nsContentUtils::GetNodeTextContent(this, true, mDefaultValue);
  }
}

// nsIMutationObserver

void nsHTMLOutputElement::CharacterDataChanged(nsIDocument* aDocument,
                                               nsIContent* aContent,
                                               CharacterDataChangeInfo* aInfo)
{
  DescendantsChanged();
}

void nsHTMLOutputElement::ContentAppended(nsIDocument* aDocument,
                                          nsIContent* aContainer,
                                          nsIContent* aFirstNewContent,
                                          int32_t aNewIndexInContainer)
{
  DescendantsChanged();
}

void nsHTMLOutputElement::ContentInserted(nsIDocument* aDocument,
                                          nsIContent* aContainer,
                                          nsIContent* aChild,
                                          int32_t aIndexInContainer)
{
  DescendantsChanged();
}

void nsHTMLOutputElement::ContentRemoved(nsIDocument* aDocument,
                                         nsIContent* aContainer,
                                         nsIContent* aChild,
                                         int32_t aIndexInContainer,
                                         nsIContent* aPreviousSibling)
{
  DescendantsChanged();
}

