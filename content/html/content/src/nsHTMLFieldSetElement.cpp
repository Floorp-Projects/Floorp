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
#include "nsIDOMEventReceiver.h"
#include "nsGenericHTMLElement.h"
#include "nsStyleConsts.h"
#include "nsPresContext.h"
#include "nsIForm.h"
#include "nsIFormControl.h"


class nsHTMLFieldSetElement : public nsGenericHTMLFormElement,
                              public nsIDOMHTMLFieldSetElement
{
public:
  nsHTMLFieldSetElement(nsINodeInfo *aNodeInfo);
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
  NS_IMETHOD_(PRInt32) GetType() const { return NS_FORM_FIELDSET; }
  NS_IMETHOD Reset();
  NS_IMETHOD SubmitNamesValues(nsIFormSubmission* aFormSubmission,
                               nsIContent* aSubmitElement);
  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;
};

// construction, destruction


NS_IMPL_NS_NEW_HTML_ELEMENT(FieldSet)


nsHTMLFieldSetElement::nsHTMLFieldSetElement(nsINodeInfo *aNodeInfo)
  : nsGenericHTMLFormElement(aNodeInfo)
{
}

nsHTMLFieldSetElement::~nsHTMLFieldSetElement()
{
}

// nsISupports

NS_IMPL_ADDREF_INHERITED(nsHTMLFieldSetElement, nsGenericElement)
NS_IMPL_RELEASE_INHERITED(nsHTMLFieldSetElement, nsGenericElement)


// QueryInterface implementation for nsHTMLFieldSetElement
NS_HTML_CONTENT_INTERFACE_MAP_BEGIN(nsHTMLFieldSetElement,
                                    nsGenericHTMLFormElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMHTMLFieldSetElement)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(HTMLFieldSetElement)
NS_HTML_CONTENT_INTERFACE_MAP_END


// nsIDOMHTMLFieldSetElement


NS_IMPL_ELEMENT_CLONE(nsHTMLFieldSetElement)


// nsIDOMHTMLFieldSetElement

NS_IMETHODIMP
nsHTMLFieldSetElement::GetForm(nsIDOMHTMLFormElement** aForm)
{
  return nsGenericHTMLFormElement::GetForm(aForm);
}

// nsIFormControl

nsresult
nsHTMLFieldSetElement::Reset()
{
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLFieldSetElement::SubmitNamesValues(nsIFormSubmission* aFormSubmission,
                                         nsIContent* aSubmitElement)
{
  return NS_OK;
}
