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
 *  Allan Beaufour <allan@beaufour.dk>
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
#include "nsXFormsAtoms.h"
#include "nsXFormsUtils.h"
#include "nsXFormsControlStub.h"
#include "nsIXFormsSubmitElement.h"
#include "nsIXFormsSubmissionElement.h"
#include "nsXFormsDelegateStub.h"
#include "nsIXFormsUIWidget.h"

NS_HIDDEN_(nsresult)
NS_NewXFormsTriggerElement(nsIXTFElement **aResult)
{
  *aResult = new nsXFormsDelegateStub(NS_LITERAL_STRING("trigger"));
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*aResult);
  return NS_OK;
}

//-----------------------------------------------------------------------------

class nsXFormsSubmitElement : public nsXFormsDelegateStub,
                              public nsIXFormsSubmitElement
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIXFORMSSUBMITELEMENT

  // nsIXTFElement overrides
  NS_IMETHOD HandleDefault(nsIDOMEvent *aEvent, PRBool *aHandled);

#ifdef DEBUG_smaug
  virtual const char* Name() { return "submit"; }
#endif
};

NS_IMPL_ISUPPORTS_INHERITED1(nsXFormsSubmitElement,
                             nsXFormsDelegateStub,
                             nsIXFormsSubmitElement)

// nsIXTFElement

NS_IMETHODIMP
nsXFormsSubmitElement::HandleDefault(nsIDOMEvent *aEvent, PRBool *aHandled)
{
  nsresult rv;
  
  rv = nsXFormsDelegateStub::HandleDefault(aEvent, aHandled);
  NS_ENSURE_SUCCESS(rv, rv);
  if (*aHandled) {
    return NS_OK;
  }

  if (!nsXFormsUtils::EventHandlingAllowed(aEvent, mElement))
    return NS_OK;

  nsAutoString type;
  aEvent->GetType(type);
  if (!(*aHandled = type.EqualsLiteral("DOMActivate")))
    return NS_OK;

  NS_NAMED_LITERAL_STRING(submission, "submission");
  nsAutoString submissionID;
  mElement->GetAttribute(submission, submissionID);

  nsCOMPtr<nsIDOMElement> submissionElement;
  nsXFormsUtils::GetElementByContextId(mElement, submissionID,
                                       getter_AddRefs(submissionElement));

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
  nsCOMPtr<nsIXFormsUIWidget> widget(do_QueryInterface(mElement));
  if (widget) {
    widget->Disable(aDisable);
  }

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
