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
#include "nsIDOMUIEvent.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMDocumentEvent.h"
#include "nsIDOMDocument.h"
#include "nsIDOMHTMLElement.h"
#include "nsIDOMHTMLButtonElement.h"
#include "nsIDOMDocumentView.h"
#include "nsIDOMAbstractView.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsIXTFXMLVisualWrapper.h"
#include "nsXFormsAtoms.h"
#include "nsXFormsUtils.h"
#include "nsXFormsControlStub.h"
#include "nsIXFormsSubmitElement.h"
#include "nsIXFormsSubmissionElement.h"

class nsXFormsTriggerElement : public nsXFormsControlStub
{
public:
  // nsIXTFXMLVisual overrides
  NS_IMETHOD OnCreated(nsIXTFXMLVisualWrapper *aWrapper);

  // nsIXTFVisual overrides
  NS_IMETHOD GetVisualContent(nsIDOMElement **aElement);
  NS_IMETHOD GetInsertionPoint(nsIDOMElement **aElement);

  // nsIXTFElement overrides
  NS_IMETHOD BeginAddingChildren();
  NS_IMETHOD DoneAddingChildren();
  NS_IMETHOD AttributeRemoved(nsIAtom *aName);
  NS_IMETHOD AttributeSet(nsIAtom *aName, const nsAString &aValue);

  // nsIXFormsControl
  NS_IMETHOD HandleDefault(nsIDOMEvent *aEvent, PRBool *aHandled);
  NS_IMETHOD Refresh();
  NS_IMETHOD TryFocus(PRBool* aOK);

  // nsXFormsTriggerElement
  nsXFormsTriggerElement() :
    mIsMinimal(PR_FALSE), mDoneAddingChildren(PR_TRUE) {}

protected:
  nsCOMPtr<nsIDOMElement>     mVisualContent;
  nsCOMPtr<nsIDOMHTMLElement> mHTMLElement;
  nsCOMPtr<nsIDOMHTMLElement> mInsertionPoint;
  PRPackedBool                mIsMinimal;
  PRPackedBool                mDoneAddingChildren;
};

// nsIXTFXMLVisual

NS_IMETHODIMP
nsXFormsTriggerElement::OnCreated(nsIXTFXMLVisualWrapper *aWrapper)
{
  nsresult rv = nsXFormsControlStub::OnCreated(aWrapper);
  NS_ENSURE_SUCCESS(rv, rv);

  aWrapper->SetNotificationMask(kStandardNotificationMask |
                                nsIXTFElement::NOTIFY_HANDLE_DEFAULT |
                                nsIXTFElement::NOTIFY_BEGIN_ADDING_CHILDREN |
                                nsIXTFElement::NOTIFY_DONE_ADDING_CHILDREN |
                                nsIXTFElement::NOTIFY_ATTRIBUTE_REMOVED);

  ResetHelpAndHint(PR_TRUE);

  nsCOMPtr<nsIDOMDocument> domDoc;
  mElement->GetOwnerDocument(getter_AddRefs(domDoc));
  NS_ENSURE_STATE(domDoc);

  // Create visual (span + button)
  domDoc->CreateElementNS(NS_LITERAL_STRING(NS_NAMESPACE_XHTML),
                          NS_LITERAL_STRING("span"),
                          getter_AddRefs(mVisualContent));

  nsCOMPtr<nsIDOMElement> button;
  domDoc->CreateElementNS(NS_LITERAL_STRING(NS_NAMESPACE_XHTML),
                          NS_LITERAL_STRING("button"),
                          getter_AddRefs(button));

  nsCOMPtr<nsIDOMNode> insertedButton;
  mVisualContent->AppendChild(button, getter_AddRefs(insertedButton));
  mHTMLElement = do_QueryInterface(insertedButton);
  NS_ENSURE_STATE(mHTMLElement);

  // Create insertion point
  nsCOMPtr<nsIDOMElement> span;
  domDoc->CreateElementNS(NS_LITERAL_STRING(NS_NAMESPACE_XHTML),
                          NS_LITERAL_STRING("span"),
                          getter_AddRefs(span));
  NS_ENSURE_STATE(span);

  nsCOMPtr<nsIDOMNode> insertedSpan;
  rv = mHTMLElement->AppendChild(span, getter_AddRefs(insertedSpan));
  NS_ENSURE_SUCCESS(rv, rv);

  mInsertionPoint = do_QueryInterface(insertedSpan);
  NS_ENSURE_STATE(mInsertionPoint);

  return NS_OK;
}

// nsIXTFVisual

NS_IMETHODIMP
nsXFormsTriggerElement::GetVisualContent(nsIDOMElement **aElement)
{
  NS_ADDREF(*aElement = mVisualContent);
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsTriggerElement::GetInsertionPoint(nsIDOMElement **aElement)
{
  NS_ADDREF(*aElement = mInsertionPoint);
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsTriggerElement::BeginAddingChildren()
{
  mDoneAddingChildren = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsTriggerElement::DoneAddingChildren()
{
  mDoneAddingChildren = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsTriggerElement::AttributeRemoved(nsIAtom *aName)
{
  return aName == nsXFormsAtoms::appearance
                  ? AttributeSet(aName, EmptyString())
                  : nsXFormsControlStub::AttributeRemoved(aName);
}

NS_IMETHODIMP
nsXFormsTriggerElement::AttributeSet(nsIAtom *aName, const nsAString &aValue)
{
  if (aName == nsXFormsAtoms::appearance) {
    if (mIsMinimal != aValue.EqualsLiteral("minimal")) {
      nsCOMPtr<nsIDOMNode> parent, next, tmp;
      if (mDoneAddingChildren) {
        mElement->GetParentNode(getter_AddRefs(parent));
        if (parent) {
          mElement->GetNextSibling(getter_AddRefs(next));
          // Removing this element temporarily from the document so that
          // the UI will be re-created properly later.
          parent->RemoveChild(mElement, getter_AddRefs(tmp));
        }
      }
      // Switch state
      mIsMinimal = !mIsMinimal;

      // Create new element
      nsCOMPtr<nsIDOMDocument> domDoc;
      mVisualContent->GetOwnerDocument(getter_AddRefs(domDoc));
      NS_ENSURE_STATE(domDoc);
      nsCOMPtr<nsIDOMElement> htmlElement;
      if (mIsMinimal) {
        domDoc->CreateElementNS(NS_LITERAL_STRING(NS_NAMESPACE_XHTML),
                                NS_LITERAL_STRING("span"),
                                getter_AddRefs(htmlElement));
      } else {
        domDoc->CreateElementNS(NS_LITERAL_STRING(NS_NAMESPACE_XHTML),
                                NS_LITERAL_STRING("button"),
                                getter_AddRefs(htmlElement));
      }
      NS_ENSURE_STATE(htmlElement);

      // Append inner content (insertion point)
      nsCOMPtr<nsIDOMNode> newnode;
      nsresult rv = htmlElement->AppendChild(mInsertionPoint, getter_AddRefs(newnode));
      NS_ENSURE_SUCCESS(rv, rv);

      // Replace current visual
      rv = mVisualContent->ReplaceChild(htmlElement, mHTMLElement, getter_AddRefs(tmp));
      mHTMLElement = do_QueryInterface(htmlElement);
      
      if (mDoneAddingChildren && parent)
        parent->InsertBefore(mElement, next, getter_AddRefs(tmp));
    }
  } else {
    nsXFormsControlStub::AttributeSet(aName, aValue);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsTriggerElement::HandleDefault(nsIDOMEvent *aEvent, PRBool *aHandled)
{
  nsresult rv;
  
  rv = nsXFormsControlStub::HandleDefault(aEvent, aHandled);
  NS_ENSURE_SUCCESS(rv, rv);
  if (*aHandled || !mIsMinimal) {
    return NS_OK;
  }

  nsAutoString type;
  aEvent->GetType(type);

  // Check for click on minimal trigger
  if (!(*aHandled = type.EqualsLiteral("click")))
    return NS_OK;

  // We need to dend DOMActivate
  nsCOMPtr<nsIDOMEventTarget> target = do_QueryInterface(mElement);
  NS_ENSURE_STATE(target);

  nsCOMPtr<nsIDOMDocument> domDoc;
  mElement->GetOwnerDocument(getter_AddRefs(domDoc));

  nsCOMPtr<nsIDOMDocumentEvent> doc = do_QueryInterface(domDoc);
  NS_ENSURE_STATE(doc);

  nsCOMPtr<nsIDOMDocumentView> dView = do_QueryInterface(domDoc);
  NS_ENSURE_STATE(dView);
  nsCOMPtr<nsIDOMAbstractView> aView;
  dView->GetDefaultView(getter_AddRefs(aView));  
  NS_ENSURE_STATE(aView);

  nsCOMPtr<nsIDOMEvent> event;
  doc->CreateEvent(NS_LITERAL_STRING("UIEvents"), getter_AddRefs(event));
  nsCOMPtr<nsIDOMUIEvent> uiEvent = do_QueryInterface(event);
  NS_ENSURE_TRUE(uiEvent, NS_ERROR_OUT_OF_MEMORY);

  uiEvent->InitUIEvent(NS_LITERAL_STRING("DOMActivate"),
                       PR_TRUE,
                       PR_TRUE,
                       aView,
                       1); // Simple click

  // XXX: What about uiEvent->SetTrusted(?), should these events be
  // trusted or not?

  PRBool cancelled;
  return target->DispatchEvent(uiEvent, &cancelled);
}

// nsIXFormsControl

NS_IMETHODIMP
nsXFormsTriggerElement::Refresh()
{
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsTriggerElement::TryFocus(PRBool *aOK)
{
  *aOK = GetRelevantState() && nsXFormsUtils::FocusControl(mHTMLElement);
  return NS_OK;
}

//-----------------------------------------------------------------------------

class nsXFormsSubmitElement : public nsXFormsTriggerElement,
                              public nsIXFormsSubmitElement
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIXFORMSSUBMITELEMENT

  // nsIXTFElement overrides
  NS_IMETHOD HandleDefault(nsIDOMEvent *aEvent, PRBool *aHandled);
};

NS_IMPL_ISUPPORTS_INHERITED1(nsXFormsSubmitElement,
                             nsXFormsTriggerElement,
                             nsIXFormsSubmitElement)

// nsIXTFElement

NS_IMETHODIMP
nsXFormsSubmitElement::HandleDefault(nsIDOMEvent *aEvent, PRBool *aHandled)
{
  nsresult rv;
  
  rv = nsXFormsTriggerElement::HandleDefault(aEvent, aHandled);
  NS_ENSURE_SUCCESS(rv, rv);
  if (*aHandled) {
    return NS_OK;
  }

  nsAutoString type;
  aEvent->GetType(type);
  if (!(*aHandled = type.EqualsLiteral("DOMActivate")))
    return NS_OK;

  NS_NAMED_LITERAL_STRING(submission, "submission");
  nsAutoString submissionID;
  mElement->GetAttribute(submission, submissionID);

  nsCOMPtr<nsIDOMDocument> ownerDoc;
  mElement->GetOwnerDocument(getter_AddRefs(ownerDoc));
  NS_ENSURE_STATE(ownerDoc);

  nsCOMPtr<nsIDOMElement> submissionElement;
  ownerDoc->GetElementById(submissionID, getter_AddRefs(submissionElement));
  nsCOMPtr<nsIXFormsSubmissionElement> xfSubmission(do_QueryInterface(submissionElement));
  
  if (!xfSubmission) {
    const PRUnichar *strings[] = { submissionID.get(), submission.get() };
    nsXFormsUtils::ReportError(NS_LITERAL_STRING("idRefError"),
                               strings, 2, mElement, mElement);
    return nsXFormsUtils::DispatchEvent(mElement, eEvent_BindingException);
  }

  xfSubmission->SetActivator(this);
  nsXFormsUtils::DispatchEvent(submissionElement, eEvent_Submit);

  *aHandled = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsSubmitElement::SetDisabled(PRBool aDisable)
{
  ///
  /// @todo Fix SetDisabled() behaviour when mIsMinimal == PR_TRUE (XXX)
  nsCOMPtr<nsIDOMHTMLButtonElement> button = do_QueryInterface(mHTMLElement);
  if (button)
    button->SetDisabled(aDisable);
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
