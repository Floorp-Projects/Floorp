/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mounir Lamouri <mounir.lamouri@mozilla.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsIDOMHTMLOutputElement.h"
#include "nsGenericHTMLElement.h"
#include "nsFormSubmission.h"
#include "nsDOMSettableTokenList.h"
#include "nsStubMutationObserver.h"
#include "nsIConstraintValidation.h"


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
  NS_FORWARD_NSIDOMNODE(nsGenericHTMLFormElement::)

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT(nsGenericHTMLFormElement::)

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT(nsGenericHTMLFormElement::)

  // nsIDOMHTMLOutputElement
  NS_DECL_NSIDOMHTMLOUTPUTELEMENT

  // nsIFormControl
  NS_IMETHOD_(PRUint32) GetType() const { return NS_FORM_OUTPUT; }
  NS_IMETHOD Reset();
  NS_IMETHOD SubmitNamesValues(nsFormSubmission* aFormSubmission);

  virtual bool IsDisabled() const { return PR_FALSE; }

  nsresult Clone(nsINodeInfo* aNodeInfo, nsINode** aResult) const;

  PRBool ParseAttribute(PRInt32 aNamespaceID, nsIAtom* aAttribute,
                        const nsAString& aValue, nsAttrValue& aResult);

  // This function is called when a callback function from nsIMutationObserver
  // has to be used to update the defaultValue attribute.
  void DescendantsChanged();

  // nsIMutationObserver
  NS_DECL_NSIMUTATIONOBSERVER_CHARACTERDATACHANGED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTAPPENDED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTINSERTED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTREMOVED

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED_NO_UNLINK(nsHTMLOutputElement,
                                                     nsGenericHTMLFormElement)

  virtual nsXPCClassInfo* GetClassInfo();
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
  // <output> is always barred from constraint validation.
  SetBarredFromConstraintValidation(PR_TRUE);
}

nsHTMLOutputElement::~nsHTMLOutputElement()
{
  if (mTokenList) {
    mTokenList->DropReference();
  }
}


NS_IMPL_ADDREF_INHERITED(nsHTMLOutputElement, nsGenericElement)
NS_IMPL_RELEASE_INHERITED(nsHTMLOutputElement, nsGenericElement)

DOMCI_NODE_DATA(HTMLOutputElement, nsHTMLOutputElement)

NS_INTERFACE_TABLE_HEAD(nsHTMLOutputElement)
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
NS_IMPL_NSICONSTRAINTVALIDATION(nsHTMLOutputElement)

NS_IMETHODIMP
nsHTMLOutputElement::Reset()
{
  mValueModeFlag = eModeDefault;
  nsresult rv = nsContentUtils::SetNodeTextContent(this, mDefaultValue,
                                                   PR_TRUE);
  return rv;
}

NS_IMETHODIMP
nsHTMLOutputElement::SubmitNamesValues(nsFormSubmission* aFormSubmission)
{
  // The output element is not submittable.
  return NS_OK;
}

PRBool
nsHTMLOutputElement::ParseAttribute(PRInt32 aNamespaceID, nsIAtom* aAttribute,
                                    const nsAString& aValue, nsAttrValue& aResult)
{
  if (aNamespaceID == kNameSpaceID_None) {
    if (aAttribute == nsGkAtoms::_for) {
      aResult.ParseAtomArray(aValue);
      return PR_TRUE;
    }
  }

  return nsGenericHTMLFormElement::ParseAttribute(aNamespaceID, aAttribute,
                                                  aValue, aResult);
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
  nsContentUtils::GetNodeTextContent(this, PR_TRUE, aValue);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLOutputElement::SetValue(const nsAString& aValue)
{
  mValueModeFlag = eModeValue;
  return nsContentUtils::SetNodeTextContent(this, aValue, PR_TRUE);
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
    return nsContentUtils::SetNodeTextContent(this, mDefaultValue, PR_TRUE);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLOutputElement::GetHtmlFor(nsIDOMDOMSettableTokenList** aResult)
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
    nsContentUtils::GetNodeTextContent(this, PR_TRUE, mDefaultValue);
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
                                          PRInt32 aNewIndexInContainer)
{
  DescendantsChanged();
}

void nsHTMLOutputElement::ContentInserted(nsIDocument* aDocument,
                                          nsIContent* aContainer,
                                          nsIContent* aChild,
                                          PRInt32 aIndexInContainer)
{
  DescendantsChanged();
}

void nsHTMLOutputElement::ContentRemoved(nsIDocument* aDocument,
                                         nsIContent* aContainer,
                                         nsIContent* aChild,
                                         PRInt32 aIndexInContainer,
                                         nsIContent* aPreviousSibling)
{
  DescendantsChanged();
}

