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
#include "nsIWebFormsOutputElement.h"
//#include "nsIDOMEventReceiver.h"
#include "nsIHTMLContent.h"
#include "nsGenericHTMLElement.h"
#include "nsHTMLAtoms.h"
#include "nsStyleConsts.h"
#include "nsIPresContext.h"
#include "nsMappedAttributes.h"
#include "nsRuleNode.h"


class nsHTMLOutputElement : public nsGenericHTMLFormElement,
                            public nsIWebFormsOutputElement
{
public:
  nsHTMLOutputElement(nsINodeInfo *aNodeInfo);
  virtual ~nsHTMLOutputElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIWebFormsOutputElement
  NS_DECL_NSIWEBFORMSOUTPUTELEMENT

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE_NO_CLONENODE(nsGenericHTMLFormElement::)

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT(nsGenericHTMLFormElement::)

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT(nsGenericHTMLFormElement::)

  // nsIFormControl
  NS_IMETHOD_(PRInt32) GetType()
  {
    return NS_FORM_OUTPUT;
  }
  NS_IMETHOD Reset();
  NS_IMETHOD SubmitNamesValues(nsIFormSubmission* aFormSubmission,
                               nsIContent* aSubmitElement);
};


NS_IMPL_NS_NEW_HTML_ELEMENT(Output)


nsHTMLOutputElement::nsHTMLOutputElement(nsINodeInfo *aNodeInfo)
  : nsGenericHTMLFormElement(aNodeInfo)
{
}

nsHTMLOutputElement::~nsHTMLOutputElement()
{
}


NS_IMPL_ADDREF_INHERITED(nsHTMLOutputElement, nsGenericElement) 
NS_IMPL_RELEASE_INHERITED(nsHTMLOutputElement, nsGenericElement) 



// QueryInterface implementation for nsHTMLOutputElement
NS_HTML_CONTENT_INTERFACE_MAP_BEGIN(nsHTMLOutputElement,
                                    nsGenericHTMLFormElement)
  NS_INTERFACE_MAP_ENTRY(nsIWebFormsOutputElement)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(HTMLOutputElement)
NS_HTML_CONTENT_INTERFACE_MAP_END


NS_IMPL_HTML_DOM_CLONENODE(Output)


/* attribute DOMString defaultValue; */
NS_IMETHODIMP
nsHTMLOutputElement::GetDefaultValue(nsAString & aDefaultValue)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP
nsHTMLOutputElement::SetDefaultValue(const nsAString & aDefaultValue)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute nsIDOMHTMLFormElement form; */
NS_IMETHODIMP
nsHTMLOutputElement::GetForm(nsIDOMHTMLFormElement * *aForm)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute DOMString name; */
NS_IMETHODIMP
nsHTMLOutputElement::GetName(nsAString & aName)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP
nsHTMLOutputElement::SetName(const nsAString & aName)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute DOMString value; */
NS_IMETHODIMP
nsHTMLOutputElement::GetValue(nsAString & aValue)
{
  nsresult rv;
  nsCOMPtr<nsIDOM3Node> node3 =
    do_QueryInterface(NS_STATIC_CAST(nsIWebFormsOutputElement *, this), &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  return node3->GetTextContent(aValue);
}

NS_IMETHODIMP
nsHTMLOutputElement::SetValue(const nsAString & aValue)
{
  nsresult rv;
  nsCOMPtr<nsIDOM3Node> node3 =
    do_QueryInterface(NS_STATIC_CAST(nsIWebFormsOutputElement *, this), &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  return node3->SetTextContent(aValue);
}

NS_IMETHODIMP
nsHTMLOutputElement::Reset()
{
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLOutputElement::SubmitNamesValues(nsIFormSubmission* aFormSubmission,
                                       nsIContent* aSubmitElement)
{
  return NS_OK;
}
