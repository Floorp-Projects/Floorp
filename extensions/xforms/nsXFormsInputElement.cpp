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
 * The Initial Developer of the Original Code is
 * IBM Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Brian Ryner <bryner@brianryner.com>
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

#include "nsIDOMEventTarget.h"
#include "nsIDOM3Node.h"
#include "nsIDOMElement.h"
#include "nsMemory.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsIXTFXMLVisualWrapper.h"
#include "nsIDOMDocument.h"
#include "nsIXFormsControl.h"
#include "nsXFormsControlStub.h"
#include "nsISchema.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsIDOMHTMLTextAreaElement.h"
#include "nsAutoPtr.h"
#include "nsIDOMXPathResult.h"
#include "nsIDOMFocusListener.h"
#include "nsXFormsUtils.h"
#include "nsIModelElementPrivate.h"
#include "nsIContent.h"
#include "nsIDOMXPathExpression.h"

/**
 * Implementation of the \<input\>, \<secret\>, and \<textarea\> elements.
 */
class nsXFormsInputElement : public nsIDOMFocusListener,
                             public nsXFormsControlStub
{
public:
  NS_DECL_ISUPPORTS_INHERITED

  // nsIXTFXMLVisual overrides
  NS_IMETHOD OnCreated(nsIXTFXMLVisualWrapper *aWrapper);

  // nsIXTFVisual overrides
  NS_IMETHOD GetVisualContent(nsIDOMElement **aElement);
  NS_IMETHOD GetInsertionPoint(nsIDOMElement **aPoint);

  // nsIXTFElement overrides
  NS_IMETHOD OnDestroyed();

  // nsIXFormsControl
  NS_IMETHOD Refresh();
  NS_IMETHOD TryFocus(PRBool* aOK);

  // nsIDOMEventListener
  NS_IMETHOD HandleEvent(nsIDOMEvent *aEvent);

  // nsIDOMFocusListener
  NS_IMETHOD Focus(nsIDOMEvent *aEvent);
  NS_IMETHOD Blur(nsIDOMEvent *aEvent);

  enum ControlType {
    eType_Input,
    eType_Secret,
    eType_TextArea
  };

  // nsXFormsInputElement
  nsXFormsInputElement(ControlType aType)
    : mType(aType)
    {}

private:
  nsCOMPtr<nsIDOMElement>          mLabel;
  nsCOMPtr<nsIDOMElement>          mControl;
  ControlType                      mType;
};

NS_IMPL_ISUPPORTS_INHERITED3(nsXFormsInputElement,
                             nsXFormsXMLVisualStub,
                             nsIXFormsControl,
                             nsIDOMFocusListener,
                             nsIDOMEventListener)

// nsIXTFXMLVisual

NS_IMETHODIMP
nsXFormsInputElement::OnCreated(nsIXTFXMLVisualWrapper *aWrapper)
{
  nsresult rv = nsXFormsControlStub::OnCreated(aWrapper);
  NS_ENSURE_SUCCESS(rv, rv);

  // Our anonymous content structure will look like this:
  //
  // <label>                         (mLabel)
  //   <span/>                       (insertion point)
  //   <input/> or <textarea/>       (mControl)
  // </label>

  nsCOMPtr<nsIDOMDocument> domDoc;
  mElement->GetOwnerDocument(getter_AddRefs(domDoc));

  domDoc->CreateElementNS(NS_LITERAL_STRING(NS_NAMESPACE_XHTML),
                          NS_LITERAL_STRING("label"),
                          getter_AddRefs(mLabel));
  NS_ENSURE_STATE(mLabel);

  nsCOMPtr<nsIDOMElement> element;
  nsCOMPtr<nsIDOMNode> childReturn;

  domDoc->CreateElementNS(NS_LITERAL_STRING(NS_NAMESPACE_XHTML),
                          NS_LITERAL_STRING("span"),
                          getter_AddRefs(element));
  NS_ENSURE_STATE(element);

  mLabel->AppendChild(element, getter_AddRefs(childReturn));

  domDoc->CreateElementNS(NS_LITERAL_STRING(NS_NAMESPACE_XHTML),
                          mType == eType_TextArea
                              ? NS_LITERAL_STRING("textarea")
                              : NS_LITERAL_STRING("input"),
                          getter_AddRefs(mControl));
  NS_ENSURE_STATE(mControl);

  if (mType == eType_Secret) {
    nsCOMPtr<nsIDOMHTMLInputElement> input = do_QueryInterface(mControl);
    NS_ENSURE_STATE(input);

    input->SetType(NS_LITERAL_STRING("password"));
  }

  mLabel->AppendChild(mControl, getter_AddRefs(childReturn));

  // We can't use xtf handleDefault here because editor stops blur events from
  // bubbling, and we can't use a system event group handler because blur
  // events don't go to the system event group (bug 263240).  So, just install
  // a bubbling listener in the normal event group.  This works out because
  // content can't get at the anonymous content to install an event handler,
  // and the event doesn't bubble up.

  nsCOMPtr<nsIDOMEventTarget> targ = do_QueryInterface(mControl);
  NS_ASSERTION(targ, "input must be an event target!");

  targ->AddEventListener(NS_LITERAL_STRING("blur"), this, PR_FALSE);
  return NS_OK;
}

// nsIXTFVisual

NS_IMETHODIMP
nsXFormsInputElement::GetVisualContent(nsIDOMElement **aElement)
{
  NS_ADDREF(*aElement = mLabel);
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsInputElement::GetInsertionPoint(nsIDOMElement **aPoint)
{
  nsCOMPtr<nsIDOMNode> childNode;
  mLabel->GetFirstChild(getter_AddRefs(childNode));
  return CallQueryInterface(childNode, aPoint);
}

// nsIXTFElement

NS_IMETHODIMP
nsXFormsInputElement::OnDestroyed()
{
  nsCOMPtr<nsIDOMEventTarget> targ = do_QueryInterface(mControl);
  if (NS_LIKELY(targ != nsnull)) {
    targ->RemoveEventListener(NS_LITERAL_STRING("blur"), this, PR_FALSE);
  }

  nsXFormsControlStub::OnDestroyed();

  return NS_OK;
}

// nsIDOMEventListener

NS_IMETHODIMP
nsXFormsInputElement::HandleEvent(nsIDOMEvent *aEvent)
{
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsInputElement::Focus(nsIDOMEvent *aEvent)
{
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsInputElement::Blur(nsIDOMEvent *aEvent)
{
  if (!mControl && !mBoundNode && !mModel)
    return NS_OK;

  nsAutoString value;
  if (mType == eType_TextArea) {
    nsCOMPtr<nsIDOMHTMLTextAreaElement> textArea = do_QueryInterface(mControl);
    NS_ENSURE_STATE(textArea);

    textArea->GetValue(value);
  } else {
    nsCOMPtr<nsIDOMHTMLInputElement> input = do_QueryInterface(mControl);
    NS_ENSURE_STATE(input);

    nsAutoString type;
    input->GetType(type);
    if (mType == eType_Input && type.EqualsLiteral("checkbox")) {
      PRBool checked;
      input->GetChecked(&checked);
      value.AssignASCII(checked ? "1" : "0", 1);
    } else {
      input->GetValue(value);
    }
  }

  PRBool changed;
  nsresult rv = mModel->SetNodeValue(mBoundNode, value, &changed);
  NS_ENSURE_SUCCESS(rv, rv);
  if (changed) {
    nsCOMPtr<nsIDOMNode> model = do_QueryInterface(mModel);
 
    if (model) {
      rv = nsXFormsUtils::DispatchEvent(model, eEvent_Recalculate);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = nsXFormsUtils::DispatchEvent(model, eEvent_Revalidate);
      NS_ENSURE_SUCCESS(rv, rv);        
      rv = nsXFormsUtils::DispatchEvent(model, eEvent_Refresh);
      NS_ENSURE_SUCCESS(rv, rv);        
    }
  }

  return NS_OK;
}

// nsIXFormsControl

NS_IMETHODIMP
nsXFormsInputElement::Refresh()
{
  if (!mControl || !mModel)
    return NS_OK;
  
  nsAutoString text;
  PRBool readonly = GetReadOnlyState();    
  if (mBoundNode) {
    nsXFormsUtils::GetNodeValue(mBoundNode, text);
  }

  if (mType == eType_TextArea) {
    nsCOMPtr<nsIDOMHTMLTextAreaElement> textArea = do_QueryInterface(mControl);
    NS_ENSURE_STATE(textArea);

    textArea->SetValue(text);
    textArea->SetReadOnly(readonly);  
  } else {
    nsCOMPtr<nsIDOMHTMLInputElement> input = do_QueryInterface(mControl);
    NS_ENSURE_STATE(input);

    if (mType == eType_Input) {
      nsCOMPtr<nsISchemaType> type;
      mModel->GetTypeForControl(this, getter_AddRefs(type));
      nsCOMPtr<nsISchemaBuiltinType> biType = do_QueryInterface(type);
      PRUint16 typeValue = nsISchemaBuiltinType::BUILTIN_TYPE_STRING;
      if (biType)
        biType->GetBuiltinType(&typeValue);

      if (typeValue == nsISchemaBuiltinType::BUILTIN_TYPE_BOOLEAN) {
        input->SetAttribute(NS_LITERAL_STRING("type"),
                             NS_LITERAL_STRING("checkbox"));

        input->SetChecked(text.EqualsLiteral("true") ||
                           text.EqualsLiteral("1"));
      } else {
        input->RemoveAttribute(NS_LITERAL_STRING("type"));
        input->SetValue(text);
      }
    } else {
      input->SetValue(text);
    }

    input->SetReadOnly(readonly);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsInputElement::TryFocus(PRBool* aOK)
{
  *aOK = GetRelevantState() && nsXFormsUtils::FocusControl(mControl);
  return NS_OK;
}

NS_HIDDEN_(nsresult)
NS_NewXFormsInputElement(nsIXTFElement **aResult)
{
  *aResult = new nsXFormsInputElement(nsXFormsInputElement::eType_Input);
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*aResult);
  return NS_OK;
}

NS_HIDDEN_(nsresult)
NS_NewXFormsSecretElement(nsIXTFElement **aResult)
{
  *aResult = new nsXFormsInputElement(nsXFormsInputElement::eType_Secret);
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*aResult);
  return NS_OK;
}

NS_HIDDEN_(nsresult)
NS_NewXFormsTextAreaElement(nsIXTFElement **aResult)
{
  *aResult = new nsXFormsInputElement(nsXFormsInputElement::eType_TextArea);
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*aResult);
  return NS_OK;
}
