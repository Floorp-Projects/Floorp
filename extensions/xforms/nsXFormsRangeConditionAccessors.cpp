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
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Aaron Reed <aaronr@us.ibm.com>
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

#include "nsXFormsRangeConditionAccessors.h"
#include "nsIContent.h"
#include "nsIDOMElement.h"
#include "nsIXTFElementWrapper.h"
#include "nsIXFormsControl.h"
#include "nsIEventStateManager.h"
#include "nsXFormsUtils.h"

NS_IMPL_ISUPPORTS_INHERITED2(nsXFormsRangeConditionAccessors,
                             nsXFormsAccessors,
                             nsIXFormsRangeConditionAccessors,
                             nsIClassInfo)

// nsXFormsRangeConditionAccessors

NS_IMETHODIMP
nsXFormsRangeConditionAccessors::IsInRange(PRBool *aInRange)
{
  return GetState(NS_EVENT_STATE_INRANGE, aInRange);
}

NS_IMETHODIMP
nsXFormsRangeConditionAccessors::SetInRange(PRBool aInRange)
{
  PRBool currRange;
  nsresult rv = IsInRange(&currRange);
  NS_ENSURE_SUCCESS(rv, rv);
  if (currRange == aInRange) {
    return NS_OK;
  }

  /* when this call changes the control's previous in or out of range state,
   * then we need to set the state to the new value and dispatch
   * xforms-in-range or xforms-out-of-range
   */
  nsCOMPtr<nsIXFormsControl> control(do_QueryInterface(mDelegate));
  NS_ENSURE_STATE(control);

  nsCOMPtr<nsIDOMElement> element;
  control->GetElement(getter_AddRefs(element));
  nsCOMPtr<nsIXTFElementWrapper> xtfWrap(do_QueryInterface(element));
  NS_ENSURE_STATE(xtfWrap);
  nsCOMPtr<nsIContent> content(do_QueryInterface(element));
  NS_ENSURE_STATE(content);

  PRInt32 state = content->IntrinsicState();
  if (!aInRange) {
    state &= ~NS_EVENT_STATE_INRANGE;
    state |= NS_EVENT_STATE_OUTOFRANGE;
  } else {
    state &= ~NS_EVENT_STATE_OUTOFRANGE;
    state |= NS_EVENT_STATE_INRANGE;
  }

  rv = xtfWrap->SetIntrinsicState(state);
  NS_ENSURE_SUCCESS(rv, rv);

  nsXFormsUtils::DispatchEvent(element,
                               aInRange ? eEvent_InRange : eEvent_OutOfRange);

  return NS_OK;
}

// nsIClassInfo implementation

static const nsIID sScriptingIIDs[] = {
  NS_IXFORMSACCESSORS_IID,
  NS_IXFORMSRANGECONDITIONACCESSORS_IID
};

NS_IMETHODIMP
nsXFormsRangeConditionAccessors::GetInterfaces(PRUint32 *aCount,
                                               nsIID * **aArray)
{
  return nsXFormsUtils::CloneScriptingInterfaces(sScriptingIIDs,
                                                 NS_ARRAY_LENGTH(sScriptingIIDs),
                                                 aCount, aArray);
}
