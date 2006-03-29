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
 * @see http://www.w3.org/TR/xforms/slice8.html#ui-output
 */
class nsXFormsOutputElement : public nsXFormsDelegateStub
{
public:
  // nsIXFormsControl
  NS_IMETHOD Bind();
  NS_IMETHOD Refresh();

  // nsIXFormsDelegate
  NS_IMETHOD GetValue(nsAString& aValue); 

  nsXFormsOutputElement() :
    nsXFormsDelegateStub(NS_LITERAL_STRING("output")) {}


private:
  PRBool   mHasBinding;
  nsString mValue;
  PRBool   mValueIsDirty;
};

// nsIXFormsControl

nsresult
nsXFormsOutputElement::Bind()
{
  // Clear existing bound node, etc.
  mBoundNode = nsnull;
  mDependencies.Clear();
  RemoveIndexListeners();

  if (!mHasParent)
    return NS_OK;
  
  nsresult rv = mElement->HasAttribute(NS_LITERAL_STRING("ref"), &mHasBinding);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!mHasBinding) {
    rv = mElement->HasAttribute(NS_LITERAL_STRING("bind"), &mHasBinding);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!mHasBinding) {
      // If output only has a value attribute, it can't have a bound node.
      // In an effort to streamline this a bit, we'll just get mModel here and
      // add output to the deferred bind list if necessary.  This should be all
      // that we need from the services that ProcessNodeBinding provides.
      // ProcessNodeBinding is called during ::Refresh (via GetValue) so we just
      // need a few things set up before ::Refresh gets called (usually right
      // after ::Bind)
  
      nsCOMPtr<nsIDOMDocument> domDoc;
      mElement->GetOwnerDocument(getter_AddRefs(domDoc));
      if (!nsXFormsUtils::IsDocumentReadyForBind(domDoc)) {
        nsXFormsModelElement::DeferElementBind(domDoc, this);
      }
  
      return BindToModel();
    }
  }

  nsCOMPtr<nsIDOMXPathResult> result;
  rv = ProcessNodeBinding(NS_LITERAL_STRING("ref"),
                          nsIDOMXPathResult::FIRST_ORDERED_NODE_TYPE,
                          getter_AddRefs(result));
  
  if (NS_FAILED(rv)) {
    nsXFormsUtils::ReportError(NS_LITERAL_STRING("controlBindError"), mElement);
    return rv;
  }

  if (result) {
    if (mUsesModelBinding) {
      // When bound via @bind, we'll get a snapshot back
      result->SnapshotItem(0, getter_AddRefs(mBoundNode));
    } else {
      result->GetSingleNodeValue(getter_AddRefs(mBoundNode));
    }
  }

  if (mBoundNode && mModel) {
    mModel->SetStates(this, mBoundNode);
  }

  return rv;
}

NS_IMETHODIMP
nsXFormsOutputElement::Refresh()
{
  mValueIsDirty = PR_TRUE;
  return nsXFormsDelegateStub::Refresh();
}

NS_IMETHODIMP
nsXFormsOutputElement::GetValue(nsAString& aValue)
{
  NS_ENSURE_STATE(mModel);

  if (mValueIsDirty) {
    if (mHasBinding) {
      NS_ENSURE_STATE(mBoundNode);
      nsXFormsUtils::GetNodeValue(mBoundNode, mValue);
    } else {
      nsCOMPtr<nsIDOMXPathResult> result;
      nsresult rv = ProcessNodeBinding(NS_LITERAL_STRING("value"),
                                       nsIDOMXPathResult::STRING_TYPE,
                                       getter_AddRefs(result));
      NS_ENSURE_SUCCESS(rv, rv);

      if (result) {
        SetDOMStringToNull(mValue);
        rv = result->GetStringValue(mValue);
        NS_ENSURE_SUCCESS(rv, rv);
      }
    }
    mValueIsDirty = PR_FALSE;
  }

  aValue = mValue;
  return NS_OK;
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
