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

#include "nsIXFormsValueElement.h"
#include "nsXFormsStubElement.h"
#include "nsIXTFGenericElementWrapper.h"
#include "nsIDOM3Node.h"
#include "nsXFormsUtils.h"
#include "nsIDOMElement.h"
#include "nsString.h"

/**
 * Implementation of the XForms \<value\> element.
 *
 * @note The value element does not display any content, it simply provides
 * a value from inline text or instance data.
 */

class nsXFormsValueElement : public nsXFormsStubElement,
                             public nsIXFormsValueElement
{
public:
  nsXFormsValueElement() : mElement(nsnull) {}

  NS_DECL_ISUPPORTS_INHERITED

  // nsIXTFGenericElement overrides
  NS_IMETHOD OnCreated(nsIXTFGenericElementWrapper *aWrapper);

  // nsIXFormsValueElement
  NS_DECL_NSIXFORMSVALUEELEMENT

private:
  nsIDOMElement *mElement;
};

NS_IMPL_ISUPPORTS_INHERITED1(nsXFormsValueElement,
                             nsXFormsStubElement,
                             nsIXFormsValueElement)

NS_IMETHODIMP
nsXFormsValueElement::OnCreated(nsIXTFGenericElementWrapper *aWrapper)
{
  nsCOMPtr<nsIDOMElement> node;
  aWrapper->GetElementNode(getter_AddRefs(node));

  // It's ok to keep pointer to mElement.  mElement will have an
  // owning reference to this object, so as long as we null out mElement in
  // OnDestroyed, it will always be valid.

  mElement = node;
  NS_ASSERTION(mElement, "Wrapper is not an nsIDOMElement, we'll crash soon");

  return NS_OK;
}

// nsIXFormsValueElement

NS_IMETHODIMP
nsXFormsValueElement::GetValue(nsAString &aValue)
{
  nsString value;
  PRBool singleNodeVal =
    nsXFormsUtils::GetSingleNodeBindingValue(mElement, value);
  if (singleNodeVal) {
    aValue.Assign(value);
    return NS_OK;
  }

  nsCOMPtr<nsIDOM3Node> node3 = do_QueryInterface(mElement);
  NS_ASSERTION(node3, "dom elements must support dom3");

  return node3->GetTextContent(aValue);
}

NS_HIDDEN_(nsresult)
NS_NewXFormsValueElement(nsIXTFElement **aResult)
{
  *aResult = new nsXFormsValueElement();
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*aResult);
  return NS_OK;
}
