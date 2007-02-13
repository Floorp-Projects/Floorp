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
 *  Doron Rosenberg <doronr@us.ibm.com>
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

#include "nsXFormsUtils.h"
#include "nsXFormsControlStub.h"
#include "nsXFormsDelegateStub.h"
#include "nsXFormsAtoms.h"
#include "nsCOMPtr.h"
#include "nsIDOMElement.h"
#include "nsIDOMXPathResult.h"
#include "nsIDOM3Node.h"
#include "nsIDOMDocument.h"
#include "nsIDOMText.h"
#include "nsString.h"
#include "nsIXFormsUIWidget.h"
#include "nsIDocument.h"
#include "nsNetUtil.h"
#include "nsXFormsModelElement.h"
#include "nsXFormsRangeConditionAccessors.h"
#include "nsIEventStateManager.h"

class nsXFormsSelectElement : public nsXFormsDelegateStub
{
public:
  NS_DECL_ISUPPORTS_INHERITED

  NS_IMETHOD OnCreated(nsIXTFElementWrapper *aWrapper);

  // nsIXTFElement overrides
  NS_IMETHOD ChildInserted(nsIDOMNode *aChild, PRUint32 aIndex);
  NS_IMETHOD ChildAppended(nsIDOMNode *aChild);
  NS_IMETHOD ChildRemoved(PRUint32 aIndex);

  // nsIXFormsControl
  NS_IMETHOD Refresh();
  NS_IMETHOD GetDefaultIntrinsicState(PRInt32 *aState);
  NS_IMETHOD GetDisabledIntrinsicState(PRInt32 *aState);

  // nsIXFormsDelegate overrides
  NS_IMETHOD GetXFormsAccessors(nsIXFormsAccessors **aAccessor);

  virtual PRBool IsContentAllowed();

#ifdef DEBUG_smaug
  virtual const char* Name() { return "select"; }
#endif
};

NS_IMPL_ISUPPORTS_INHERITED0(nsXFormsSelectElement,
                             nsXFormsDelegateStub)

NS_IMETHODIMP
nsXFormsSelectElement::OnCreated(nsIXTFElementWrapper *aWrapper)
{
  nsresult rv = nsXFormsDelegateStub::OnCreated(aWrapper);
  NS_ENSURE_SUCCESS(rv, rv);

  aWrapper->SetNotificationMask(kStandardNotificationMask |
                                nsIXTFElement::NOTIFY_CHILD_INSERTED |
                                nsIXTFElement::NOTIFY_CHILD_APPENDED |
                                nsIXTFElement::NOTIFY_CHILD_REMOVED |
                                nsIXTFElement::NOTIFY_PERFORM_ACCESSKEY);
  return NS_OK;
}

// nsIXTFElement overrides

NS_IMETHODIMP
nsXFormsSelectElement::ChildInserted(nsIDOMNode *aChild, PRUint32 aIndex)
{
  Refresh();
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsSelectElement::ChildAppended(nsIDOMNode *aChild)
{
  Refresh();
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsSelectElement::ChildRemoved(PRUint32 aIndex)
{
  Refresh();
  return NS_OK;
}

// nsIXFormsControl

NS_IMETHODIMP
nsXFormsSelectElement::Refresh()
{
  PRBool delayRefresh = nsXFormsModelElement::ContainerNeedsPostRefresh(this);
  if (delayRefresh) {
    return NS_OK;
  }

  return nsXFormsDelegateStub::Refresh();
}

NS_IMETHODIMP
nsXFormsSelectElement::GetDefaultIntrinsicState(PRInt32 *aState)
{
  NS_ENSURE_ARG_POINTER(aState);
  nsXFormsDelegateStub::GetDefaultIntrinsicState(aState);
  *aState |= NS_EVENT_STATE_INRANGE;
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsSelectElement::GetDisabledIntrinsicState(PRInt32 *aState)
{
  NS_ENSURE_ARG_POINTER(aState);
  nsXFormsDelegateStub::GetDisabledIntrinsicState(aState);
  *aState |= NS_EVENT_STATE_INRANGE;
  return NS_OK;
}

// nsIXFormsDelegate

NS_IMETHODIMP
nsXFormsSelectElement::GetXFormsAccessors(nsIXFormsAccessors **aAccessor)
{
  if (!mAccessor) {
    mAccessor = new nsXFormsRangeConditionAccessors(this, mElement);
    if (!mAccessor) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }
  NS_ADDREF(*aAccessor = mAccessor);
  return NS_OK;
}

PRBool
nsXFormsSelectElement::IsContentAllowed()
{
  PRBool isAllowed = PR_TRUE;

  // For select elements, non-simpleContent is only allowed if the select
  // element contains an itemset.
  PRBool isComplex = IsContentComplex();
  if (isComplex && !nsXFormsUtils::NodeHasItemset(mElement)) {
    isAllowed = PR_FALSE;
  }

  return isAllowed;
}

NS_HIDDEN_(nsresult)
NS_NewXFormsSelectElement(nsIXTFElement **aResult)
{
  *aResult = new nsXFormsSelectElement();
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*aResult);
  return NS_OK;
}

