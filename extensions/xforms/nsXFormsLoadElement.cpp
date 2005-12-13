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
#include "nsXFormsActionElement.h"
#include "nsIDOMDocument.h"
#include "nsIDOMWindowInternal.h"
#include "nsIDOMLocation.h"
#include "nsIDOMDocumentView.h"
#include "nsIDOMAbstractView.h"
#include "nsXFormsUtils.h"

class nsXFormsLoadElement : public nsXFormsActionModuleBase
{
public:
  nsXFormsLoadElement();
  NS_DECL_NSIXFORMSACTIONMODULEELEMENT
};

nsXFormsLoadElement::nsXFormsLoadElement()
{
}

NS_IMETHODIMP
nsXFormsLoadElement::HandleAction(nsIDOMEvent* aEvent,
                                  nsIXFormsActionElement *aParentAction)
{
  if(!mElement)
    return NS_OK;
  //If the element has 'resource' and single node binding, the
  //action has no effect.
  PRBool hasBind;
  PRBool hasRef;
  mElement->HasAttribute(NS_LITERAL_STRING("bind"), &hasBind);
  mElement->HasAttribute(NS_LITERAL_STRING("ref"), &hasRef);
  nsAutoString resource;
  mElement->GetAttribute(NS_LITERAL_STRING("resource"), resource);

  if (!resource.IsEmpty() && (hasBind || hasRef))
    return NS_OK;
  nsAutoString urlstr;
  if (!resource.IsEmpty())
    urlstr = resource;
  else if (!nsXFormsUtils::GetSingleNodeBindingValue(mElement, urlstr))
    return NS_OK;

  nsCOMPtr<nsIDOMDocument> doc;
  nsCOMPtr<nsIDOMWindowInternal> internal;
  mElement->GetOwnerDocument(getter_AddRefs(doc));
  nsXFormsUtils::GetWindowFromDocument(doc, getter_AddRefs(internal));
  if (!internal) {
    NS_ASSERTION(internal, "No AbstractView or it isn't an nsIDOMWindowInternal");
    return NS_OK;
  }
  
  PRBool openNew = PR_FALSE;
  nsAutoString show;
  mElement->GetAttribute(NS_LITERAL_STRING("show"), show);
  if (show.EqualsLiteral("new")) {
    openNew = PR_TRUE;
  }
  //Otherwise 'replace'
  if (openNew) {
    nsCOMPtr<nsIDOMWindow> messageWindow;
    internal->Open(urlstr, EmptyString(), EmptyString(),
                   getter_AddRefs(messageWindow));
  }
  else {
    nsCOMPtr<nsIDOMLocation> location;
    internal->GetLocation(getter_AddRefs(location));
    if (location)
      location->Assign(urlstr);
  }
  return NS_OK;
}

NS_HIDDEN_(nsresult)
NS_NewXFormsLoadElement(nsIXTFElement **aResult)
{
  *aResult = new nsXFormsLoadElement();
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*aResult);
  return NS_OK;
}

