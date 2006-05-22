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
 * Novell, Inc.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Allan Beaufour <abeaufour@novell.com>
 *  Olli Pettay <Olli.Pettay@helsinki.fi>
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

#ifdef DEBUG
//#define DEBUG_XF_OUTPUT
#endif

#include "nsIXTFXMLVisualWrapper.h"

#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsMemory.h"
#include "nsString.h"
#include "nsDOMString.h"
#include "nsIDOM3Node.h"
#include "nsIDOMDocument.h"
#include "nsIDOMElement.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMXPathExpression.h"
#include "nsIDOMXPathResult.h"
#include "nsISchema.h"

#include "nsIModelElementPrivate.h"
#include "nsXFormsDelegateStub.h"
#include "nsXFormsAtoms.h"
#include "nsXFormsUtils.h"
#include "nsIXFormsUIWidget.h"
#include "nsXFormsModelElement.h"


/**
 * Implementation of the XForms \<output\> element.
 *
 * \<output\> is not really that different from \<input\>
 * (ie. nsXFormsDelegateStub), except that it has an "value" attribute that
 * must be handled seperately because it is evaluated to a string result.
 *
 * @see http://www.w3.org/TR/xforms/slice8.html#ui-output
 */
class nsXFormsOutputElement : public nsXFormsDelegateStub
{
public:
  // nsIXFormsControl
  NS_IMETHOD Bind(PRBool* aContextChanged);
  NS_IMETHOD Refresh();
  NS_IMETHOD GetBoundNode(nsIDOMNode **aBoundNode);

  // nsIXFormsDelegate
  NS_IMETHOD GetValue(nsAString& aValue);
  NS_IMETHOD SetValue(const nsAString& aValue);
  NS_IMETHOD GetHasBoundNode(PRBool *aHasBoundNode);

  nsXFormsOutputElement();

private:
  // The value of the "value" attribute (if any). Updated by Bind()
  nsString       mValue;

  // Use the "value" attribute
  PRBool         mUseValueAttribute;
};

nsXFormsOutputElement::nsXFormsOutputElement()
  : nsXFormsDelegateStub(NS_LITERAL_STRING("output")),
    mUseValueAttribute(PR_FALSE)
{
}

// nsIXFormsControl

nsresult
nsXFormsOutputElement::Bind(PRBool *aContextChanged)
{
  SetDOMStringToNull(mValue);
  mUseValueAttribute = PR_FALSE;

  nsresult rv = nsXFormsDelegateStub::Bind(aContextChanged);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!mHasParent || !mElement || rv == NS_OK_XFORMS_DEFERRED)
    return NS_OK;

  // Besides the standard single node binding (SNB) attributes, \<output\>
  // also has a "value" attribute, which is used when there are not other SNB
  // attributes.
  if (!HasBindingAttribute()) {
    mUseValueAttribute = PR_TRUE;
  } else {
    PRBool hasAttr;
    rv = mElement->HasAttribute(NS_LITERAL_STRING("ref"), &hasAttr);
    NS_ENSURE_SUCCESS(rv, rv);
    if (!hasAttr) {
      rv = mElement->HasAttribute(NS_LITERAL_STRING("bind"), &hasAttr);
      NS_ENSURE_SUCCESS(rv, rv);
      mUseValueAttribute = !hasAttr;
    }
  }

  if (!mUseValueAttribute)
    return NS_OK;

  // Bind to model and set mBoundNode . The bound node is used for setting
  // context for our children (our parent's context in the case of @value),
  // and the call to ProcessNodeBinding() will not set it.
  rv = BindToModel(PR_TRUE);
  NS_ENSURE_SUCCESS(rv, rv);

  // Process node binding
  nsCOMPtr<nsIDOMXPathResult> result;
  rv = ProcessNodeBinding(NS_LITERAL_STRING("value"),
                          nsIDOMXPathResult::STRING_TYPE,
                          getter_AddRefs(result));
  NS_ENSURE_SUCCESS(rv, rv);

  if (rv == NS_OK_XFORMS_DEFERRED) {
    return NS_OK;
  }

  if (result) {
    rv = result->GetStringValue(mValue);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsOutputElement::Refresh()
{
  return nsXFormsDelegateStub::Refresh();
}

NS_IMETHODIMP
nsXFormsOutputElement::GetBoundNode(nsIDOMNode **aBoundNode)
{
  return !mUseValueAttribute ? nsXFormsDelegateStub::GetBoundNode(aBoundNode)
                             : NS_OK;
}

NS_IMETHODIMP
nsXFormsOutputElement::GetHasBoundNode(PRBool *aHasBoundNode)
{
  NS_ENSURE_ARG_POINTER(aHasBoundNode);
  *aHasBoundNode = (mBoundNode && !mUseValueAttribute) ? PR_TRUE : PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsOutputElement::GetValue(nsAString& aValue)
{
  if (mUseValueAttribute) {
    aValue = mValue;
    return NS_OK;
  }

  return nsXFormsDelegateStub::GetValue(aValue);
}

NS_IMETHODIMP
nsXFormsOutputElement::SetValue(const nsAString& aValue)
{
  // Setting the value on an output controls seems wrong semantically.
  return NS_ERROR_NOT_AVAILABLE;
}

NS_HIDDEN_(nsresult)
NS_NewXFormsOutputElement(nsIXTFElement **aResult)
{
  *aResult = new nsXFormsOutputElement();
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*aResult);
  return NS_OK;
}
