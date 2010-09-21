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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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
#include "nsIDOMHTMLFieldSetElement.h"
#include "nsIDOMHTMLFormElement.h"
#include "nsIDOMEventTarget.h"
#include "nsGenericHTMLElement.h"
#include "nsStyleConsts.h"
#include "nsIForm.h"
#include "nsIFormControl.h"
#include "nsIConstraintValidation.h"


class nsHTMLFieldSetElement : public nsGenericHTMLFormElement,
                              public nsIDOMHTMLFieldSetElement,
                              public nsIConstraintValidation
{
public:
  using nsIConstraintValidation::GetValidationMessage;

  nsHTMLFieldSetElement(already_AddRefed<nsINodeInfo> aNodeInfo);
  virtual ~nsHTMLFieldSetElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE(nsGenericHTMLFormElement::)

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT(nsGenericHTMLFormElement::)

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT(nsGenericHTMLFormElement::)

  // nsIDOMHTMLFieldSetElement
  NS_DECL_NSIDOMHTMLFIELDSETELEMENT

  // nsIFormControl
  NS_IMETHOD_(PRUint32) GetType() const { return NS_FORM_FIELDSET; }
  NS_IMETHOD Reset();
  NS_IMETHOD SubmitNamesValues(nsFormSubmission* aFormSubmission);
  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;
  virtual nsXPCClassInfo* GetClassInfo();

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(nsHTMLFieldSetElement,
                                           nsGenericHTMLFormElement)
private:

  // This function is used to generate the nsContentList (listed form elements).
  static PRBool MatchListedElements(nsIContent* aContent, PRInt32 aNamespaceID,
                                    nsIAtom* aAtom, void* aData);

  // listed form controls elements.
  nsRefPtr<nsContentList> mElements;
};

// construction, destruction


NS_IMPL_NS_NEW_HTML_ELEMENT(FieldSet)


nsHTMLFieldSetElement::nsHTMLFieldSetElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : nsGenericHTMLFormElement(aNodeInfo)
{
  // <fieldset> is always barred from constraint validation.
  SetBarredFromConstraintValidation(PR_TRUE);
}

nsHTMLFieldSetElement::~nsHTMLFieldSetElement()
{
}

// nsISupports

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsHTMLFieldSetElement)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mElements)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_CLASS(nsHTMLFieldSetElement)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(nsHTMLFieldSetElement,
                                                  nsGenericHTMLFormElement)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR_AMBIGUOUS(mElements, nsIDOMNodeList)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_ADDREF_INHERITED(nsHTMLFieldSetElement, nsGenericElement)
NS_IMPL_RELEASE_INHERITED(nsHTMLFieldSetElement, nsGenericElement)

DOMCI_NODE_DATA(HTMLFieldSetElement, nsHTMLFieldSetElement)

// QueryInterface implementation for nsHTMLFieldSetElement
NS_INTERFACE_TABLE_HEAD(nsHTMLFieldSetElement)
  NS_HTML_CONTENT_INTERFACE_TABLE2(nsHTMLFieldSetElement,
                                   nsIDOMHTMLFieldSetElement,
                                   nsIConstraintValidation)
  NS_HTML_CONTENT_INTERFACE_TABLE_TO_MAP_SEGUE(nsHTMLFieldSetElement,
                                               nsGenericHTMLFormElement)
NS_HTML_CONTENT_INTERFACE_TABLE_TAIL_CLASSINFO(HTMLFieldSetElement)

NS_IMPL_ELEMENT_CLONE(nsHTMLFieldSetElement)


NS_IMPL_STRING_ATTR(nsHTMLFieldSetElement, Name, name)

// nsIConstraintValidation
NS_IMPL_NSICONSTRAINTVALIDATION(nsHTMLFieldSetElement)

// nsIDOMHTMLFieldSetElement

NS_IMETHODIMP
nsHTMLFieldSetElement::GetForm(nsIDOMHTMLFormElement** aForm)
{
  return nsGenericHTMLFormElement::GetForm(aForm);
}

NS_IMETHODIMP
nsHTMLFieldSetElement::GetType(nsAString& aType)
{
  aType.AssignLiteral("fieldset");
  return NS_OK;
}

/* static */
PRBool
nsHTMLFieldSetElement::MatchListedElements(nsIContent* aContent, PRInt32 aNamespaceID,
                                           nsIAtom* aAtom, void* aData)
{
  nsCOMPtr<nsIFormControl> formControl = do_QueryInterface(aContent);
  return formControl && formControl->GetType() != NS_FORM_LABEL;
}

NS_IMETHODIMP
nsHTMLFieldSetElement::GetElements(nsIDOMHTMLCollection** aElements)
{
  if (!mElements) {
    mElements = new nsContentList(this, MatchListedElements, nsnull, nsnull, PR_TRUE);
  }

  NS_ADDREF(*aElements = mElements);

  return NS_OK;
}

// nsIFormControl

nsresult
nsHTMLFieldSetElement::Reset()
{
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLFieldSetElement::SubmitNamesValues(nsFormSubmission* aFormSubmission)
{
  return NS_OK;
}
