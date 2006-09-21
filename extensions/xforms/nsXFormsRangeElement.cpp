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
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include "nsXFormsDelegateStub.h"
#include "nsXFormsRangeAccessors.h"
#include "nsIEventStateManager.h"
#include "nsISchema.h"
#include "nsIStringBundle.h"
#include "nsServiceManagerUtils.h"

/**
 * Implementation of the XForms \<range\> element
 * @see http://www.w3.org/TR/xforms/slice8.html#ui-range
 *
 * @todo Check data binding restrictions (XXX)
 */
class nsXFormsRangeElement : public nsXFormsDelegateStub
{
public:

  // nsIXFormsControl
  NS_IMETHOD GetDefaultIntrinsicState(PRInt32 *aState);
  NS_IMETHOD GetDisabledIntrinsicState(PRInt32 *aState);

  // nsIXFormsDelegate overrides
  NS_IMETHOD GetXFormsAccessors(nsIXFormsAccessors **aAccessor);

  // nsXFormsDelegateStub override
  NS_IMETHOD IsTypeAllowed(PRUint16 aType, PRBool *aIsAllowed,
                           nsRestrictionFlag *aRestriction,
                           nsAString &aAllowedTypes);

#ifdef DEBUG_smaug
  virtual const char* Name() { return "range"; }
#endif
};

// nsIXFormsControl

NS_IMETHODIMP
nsXFormsRangeElement::IsTypeAllowed(PRUint16 aType, PRBool *aIsAllowed,
                                    nsRestrictionFlag *aRestriction,
                                    nsAString &aAllowedTypes)
{
  NS_ENSURE_ARG_POINTER(aRestriction);
  NS_ENSURE_ARG_POINTER(aIsAllowed);
  *aRestriction = eTypes_Inclusive;
  *aIsAllowed = PR_FALSE;

  // Range needs to have the type duration, date, time, dateTime, gYearMonth,
  // gYear, gMonthDay, gDay, gMonth, float, decimal or double.  Or an
  // extension or derivation of one of these types.  If not, then put an error
  // in the console.  CSS and XBL will make sure that the control won't appear
  // in the form.

  if (aType == nsISchemaBuiltinType::BUILTIN_TYPE_DURATION ||
      aType == nsISchemaBuiltinType::BUILTIN_TYPE_DATE ||
      aType == nsISchemaBuiltinType::BUILTIN_TYPE_TIME ||
      aType == nsISchemaBuiltinType::BUILTIN_TYPE_DATETIME ||
      aType == nsISchemaBuiltinType::BUILTIN_TYPE_GYEARMONTH ||
      aType == nsISchemaBuiltinType::BUILTIN_TYPE_GYEAR ||
      aType == nsISchemaBuiltinType::BUILTIN_TYPE_GMONTHDAY ||
      aType == nsISchemaBuiltinType::BUILTIN_TYPE_GDAY ||
      aType == nsISchemaBuiltinType::BUILTIN_TYPE_GMONTH ||
      aType == nsISchemaBuiltinType::BUILTIN_TYPE_FLOAT ||
      aType == nsISchemaBuiltinType::BUILTIN_TYPE_DECIMAL ||
      aType == nsISchemaBuiltinType::BUILTIN_TYPE_DOUBLE) {

    *aIsAllowed = PR_TRUE;
    return NS_OK;
  }

  // build the string of types that range can bind to
  aAllowedTypes.AssignLiteral("xsd:duration xsd:date xsd:time xsd:dateTime xsd:gYearMonth xsd:gYear xsd:gMonthDay xsd:gDay xsd:gMonth xsd:float xsd:decimal xsd:double");
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsRangeElement::GetDefaultIntrinsicState(PRInt32 *aState)
{
  NS_ENSURE_ARG_POINTER(aState);
  nsXFormsDelegateStub::GetDefaultIntrinsicState(aState);
  *aState |= NS_EVENT_STATE_INRANGE;
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsRangeElement::GetDisabledIntrinsicState(PRInt32 *aState)
{
  NS_ENSURE_ARG_POINTER(aState);
  nsXFormsDelegateStub::GetDisabledIntrinsicState(aState);
  *aState |= NS_EVENT_STATE_INRANGE;
  return NS_OK;
}

// nsIXFormsDelegate

NS_IMETHODIMP
nsXFormsRangeElement::GetXFormsAccessors(nsIXFormsAccessors **aAccessor)
{
  if (!mAccessor) {
    mAccessor = new nsXFormsRangeAccessors(this, mElement);
    if (!mAccessor) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }
  NS_ADDREF(*aAccessor = mAccessor);
  return NS_OK;
}


// Creator

NS_HIDDEN_(nsresult)
NS_NewXFormsRangeElement(nsIXTFElement **aResult)
{
  *aResult = new nsXFormsRangeElement();
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*aResult);
  return NS_OK;
}
