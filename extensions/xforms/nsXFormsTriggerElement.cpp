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
 * The Original Code is Mozilla XForms support.
 *
 * The Initial Developer of the Original Code is IBM Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Darin Fisher <darin@meer.net>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsIXTFXMLVisual.h"
#include "nsIDOM3Node.h"
#include "nsIDOMElement.h"
#include "nsIDOMEvent.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMDocumentEvent.h"
#include "nsIDOMDocument.h"
#include "nsIDOMHTMLButtonElement.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsIXTFXMLVisualWrapper.h"
#include "nsIXFormsControl.h"
#include "nsXFormsStubElement.h"
#include "nsXFormsUtils.h"

class nsXFormsTriggerElement : public nsXFormsXMLVisualStub,
                               public nsIXFormsControl
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIXFORMSCONTROL

  // nsIXTFXMLVisual overrides
  NS_IMETHOD OnCreated(nsIXTFXMLVisualWrapper *aWrapper);

  // nsIXTFVisual overrides
  NS_IMETHOD GetVisualContent(nsIDOMElement **aElement);
  NS_IMETHOD GetInsertionPoint(nsIDOMElement **aElement);

  nsXFormsTriggerElement() : mElement(nsnull) {}

protected:
  nsCOMPtr<nsIDOMHTMLButtonElement> mButton;
  nsIDOMElement *mElement;
};

NS_IMPL_ISUPPORTS_INHERITED1(nsXFormsTriggerElement,
                             nsXFormsXMLVisualStub,
                             nsIXFormsControl)

// nsIXTFXMLVisual

NS_IMETHODIMP
nsXFormsTriggerElement::OnCreated(nsIXTFXMLVisualWrapper *aWrapper)
{
  aWrapper->SetNotificationMask(nsIXTFElement::NOTIFY_HANDLE_DEFAULT);

  nsCOMPtr<nsIDOMElement> node;
  aWrapper->GetElementNode(getter_AddRefs(node));

  // It's ok to keep a weak pointer to mElement.  mElement will have an
  // owning reference to this object, so as long as we null out mElement in
  // OnDestroyed, it will always be valid.

  mElement = node;
  NS_ASSERTION(mElement, "Wrapper is not an nsIDOMElement, we'll crash soon");

  nsCOMPtr<nsIDOMDocument> domDoc;
  node->GetOwnerDocument(getter_AddRefs(domDoc));
  NS_ENSURE_STATE(domDoc);

  nsCOMPtr<nsIDOMElement> inputElement;
  domDoc->CreateElementNS(NS_LITERAL_STRING(NS_NAMESPACE_XHTML),
                          NS_LITERAL_STRING("button"),
                          getter_AddRefs(inputElement));

  mButton = do_QueryInterface(inputElement);
  NS_ENSURE_TRUE(mButton, NS_ERROR_FAILURE);

  return NS_OK;
}

// nsIXTFVisual

NS_IMETHODIMP
nsXFormsTriggerElement::GetVisualContent(nsIDOMElement **aElement)
{
  NS_ADDREF(*aElement = mButton);
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsTriggerElement::GetInsertionPoint(nsIDOMElement **aElement)
{
  NS_ADDREF(*aElement = mButton);
  return NS_OK;
}

// other methods

NS_IMETHODIMP
nsXFormsTriggerElement::Refresh()
{
  return NS_OK;
}

//-----------------------------------------------------------------------------

class nsXFormsSubmitElement : public nsXFormsTriggerElement
{
public:
  // nsIXTFElement overrides
  NS_IMETHOD HandleDefault(nsIDOMEvent *aEvent, PRBool *aHandled);
};

// nsIXTFElement

NS_IMETHODIMP
nsXFormsSubmitElement::HandleDefault(nsIDOMEvent *aEvent, PRBool *aHandled)
{
  nsAutoString type;
  aEvent->GetType(type);
  if (!(*aHandled = type.EqualsLiteral("DOMActivate")))
    return NS_OK;

  nsAutoString submissionID;
  mElement->GetAttribute(NS_LITERAL_STRING("submission"), submissionID);

  nsCOMPtr<nsIDOMDocument> ownerDoc;
  mElement->GetOwnerDocument(getter_AddRefs(ownerDoc));
  NS_ENSURE_STATE(ownerDoc);

  nsCOMPtr<nsIDOMElement> submissionElement;
  ownerDoc->GetElementById(submissionID, getter_AddRefs(submissionElement));
  NS_ENSURE_STATE(submissionElement);

  nsCOMPtr<nsIDOMEventTarget> targ = do_QueryInterface(submissionElement);
  NS_ASSERTION(targ, "All elements should support nsIDOMEventTarget");

  nsCOMPtr<nsIDOMDocumentEvent> docEvent = do_QueryInterface(ownerDoc);
  NS_ASSERTION(targ, "All documents should support nsIDOMDocumentEvent");

  nsCOMPtr<nsIDOMEvent> event;
  docEvent->CreateEvent(NS_LITERAL_STRING("Events"), getter_AddRefs(event));
  NS_ENSURE_STATE(event);

  nsresult rv = event->InitEvent(NS_LITERAL_STRING("xforms-submit"), PR_TRUE, PR_TRUE);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_UNEXPECTED);

  PRBool unused;
  rv = targ->DispatchEvent(event, &unused);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_UNEXPECTED);

  *aHandled = PR_TRUE;
  return NS_OK;
}

//-----------------------------------------------------------------------------

NS_HIDDEN_(nsresult)
NS_NewXFormsTriggerElement(nsIXTFElement **aResult)
{
  *aResult = new nsXFormsTriggerElement();
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*aResult);
  return NS_OK;
}

NS_HIDDEN_(nsresult)
NS_NewXFormsSubmitElement(nsIXTFElement **aResult)
{
  *aResult = new nsXFormsSubmitElement();
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*aResult);
  return NS_OK;
}
