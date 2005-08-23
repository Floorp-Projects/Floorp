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
 * Olli Pettay.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Olli Pettay <Olli.Pettay@helsinki.fi> (original author)
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

#include "nsXFormsActionModuleBase.h"
#include "nsIDOMDocument.h"
#include "nsIDOMDocumentEvent.h"
#include "nsIDOMEvent.h"
#include "nsIDOMElement.h"
#include "nsIDOMEventTarget.h"
#include "nsString.h"
#include "nsXFormsUtils.h"

class nsXFormsDispatchElement : public nsXFormsActionModuleBase
{
public:
  nsXFormsDispatchElement();
  NS_DECL_NSIXFORMSACTIONMODULEELEMENT
};

nsXFormsDispatchElement::nsXFormsDispatchElement()
{
}

NS_IMETHODIMP
nsXFormsDispatchElement::HandleAction(nsIDOMEvent* aEvent,
                                      nsIXFormsActionElement *aParentAction)
{
  if (!mElement)
    return NS_OK;
  
  nsAutoString name;
  mElement->GetAttribute(NS_LITERAL_STRING("name"), name);
  if (name.IsEmpty())
    return NS_OK;
  
  nsAutoString target;
  mElement->GetAttribute(NS_LITERAL_STRING("target"), target);
  if (target.IsEmpty())
    return NS_OK;

  PRBool cancelable = PR_TRUE;
  PRBool bubbles = PR_TRUE;
  if (!nsXFormsUtils::IsXFormsEvent(name, cancelable, bubbles)) {
    nsAutoString cancelableStr;
    mElement->GetAttribute(NS_LITERAL_STRING("cancelable"), cancelableStr);
    cancelable = !(cancelableStr.EqualsLiteral("false") ||
                   cancelableStr.EqualsLiteral("0"));
    
    nsAutoString bubbleStr;
    mElement->GetAttribute(NS_LITERAL_STRING("bubbles"), bubbleStr);
    bubbles = !(bubbleStr.EqualsLiteral("false") ||
                bubbleStr.EqualsLiteral("0"));

    PRBool tmp;
    if (cancelableStr.IsEmpty() && bubbleStr.IsEmpty())
      nsXFormsUtils::GetEventDefaults(name, cancelable, bubbles);
    else if (cancelableStr.IsEmpty())
      nsXFormsUtils::GetEventDefaults(name, cancelable, tmp);
    else if (bubbleStr.IsEmpty())
      nsXFormsUtils::GetEventDefaults(name, tmp, bubbles);
  }

  nsCOMPtr<nsIDOMDocument> doc;
  mElement->GetOwnerDocument(getter_AddRefs(doc));
  if (!doc)
    return NS_OK;

  nsCOMPtr<nsIDOMElement> el;
  nsXFormsUtils::GetElementById(doc, target, PR_FALSE, mElement,
                                getter_AddRefs(el));
  if (!el)
    return NS_OK;
  
  nsCOMPtr<nsIDOMDocumentEvent> docEvent = do_QueryInterface(doc);
  nsCOMPtr<nsIDOMEvent> event;
  docEvent->CreateEvent(NS_LITERAL_STRING("Events"), getter_AddRefs(event));
  event->InitEvent(name, bubbles, cancelable);

  nsCOMPtr<nsIDOMEventTarget> targetEl = do_QueryInterface(el);
  if (targetEl) {
    nsXFormsUtils::SetEventTrusted(event, el);
    PRBool defaultActionEnabled;
    targetEl->DispatchEvent(event, &defaultActionEnabled);
  }
  return NS_OK;
}

NS_HIDDEN_(nsresult)
NS_NewXFormsDispatchElement(nsIXTFElement **aResult)
{
  *aResult = new nsXFormsDispatchElement();
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*aResult);
  return NS_OK;
}

