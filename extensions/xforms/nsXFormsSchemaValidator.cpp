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
 * The Original Code is Mozilla XForms Schema Validation.
 *
 * The Initial Developer of the Original Code is
 * IBM Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * IBM Corporation. All Rights Reserved.
 *
 * Contributor(s):
 *   Doron Rosenberg <doronr@us.ibm.com> (original author)
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

// XPCOM includes
#include "nsIServiceManager.h"
#include "nsString.h"

#include "nsXFormsUtils.h"
#include "nsISchemaValidator.h"
#include "nsISchemaDuration.h"
#include "nsXFormsSchemaValidator.h"

#include "nsIDOM3Node.h"

#define NS_SCHEMAVALIDATOR_CONTRACTID "@mozilla.org/schemavalidator;1"

nsXFormsSchemaValidator::nsXFormsSchemaValidator()
{
  mSchemaValidator = do_GetService(NS_SCHEMAVALIDATOR_CONTRACTID);
}

nsresult
nsXFormsSchemaValidator::LoadSchema(nsISchema* aSchema)
{
  NS_ENSURE_TRUE(mSchemaValidator, NS_ERROR_UNEXPECTED);

  return mSchemaValidator->LoadSchema(aSchema);
}

PRBool
nsXFormsSchemaValidator::ValidateString(const nsAString & aValue,
                                        const nsAString & aType,
                                        const nsAString & aNamespace)
{
  PRBool isValid = PR_FALSE;

  NS_ENSURE_TRUE(mSchemaValidator, isValid);

  // if it is the XForms namespace, handle it internally, else delegate to
  // nsISchemaValidator
  if (aNamespace.EqualsLiteral(NS_NAMESPACE_XFORMS)) {
    isValid = ValidateXFormsTypeString(aValue, aType);
  } else {
    mSchemaValidator->ValidateString(aValue, aType, aNamespace, &isValid);
  }

  return isValid;
}

PRBool
nsXFormsSchemaValidator::ValidateNode(nsIDOMNode* aElement,
                                      const nsAString & aType,
                                      const nsAString & aNamespace)
{
  PRBool isValid = PR_FALSE;
  NS_ENSURE_TRUE(mSchemaValidator, isValid);

  if (aNamespace.EqualsLiteral(NS_NAMESPACE_XFORMS)) {
    // XXX: currently, we will only support validating a lone node.  When
    // we get complex type support, we should load the schema for the xforms
    // types and use ValidateAgainstType like below.
    nsAutoString nodeValue;
    nsCOMPtr<nsIDOM3Node> domNode3 = do_QueryInterface(aElement);
    domNode3->GetTextContent(nodeValue);

    isValid = ValidateString(nodeValue, aType, aNamespace);
  } else {
    nsCOMPtr<nsISchemaType> type;
    if (GetType(aType, aNamespace, getter_AddRefs(type))) {
      mSchemaValidator->ValidateAgainstType(aElement, type, &isValid);
    }
  }

  return isValid;
}

PRBool
nsXFormsSchemaValidator::GetType(const nsAString & aType,
                                 const nsAString & aNamespace,
                                 nsISchemaType **aSchemaType)
{
  NS_ENSURE_TRUE(mSchemaValidator, PR_FALSE);

  nsresult rv = mSchemaValidator->GetType(aType, aNamespace, aSchemaType);

  return NS_SUCCEEDED(rv);
}

// xforms schema types
PRBool
nsXFormsSchemaValidator::ValidateXFormsTypeString(const nsAString & aValue,
                                                  const nsAString & aType)
{
  if (aType.IsEmpty()) {
    return PR_FALSE;
  }

  PRBool isValid = PR_FALSE;

  if (aType.EqualsLiteral("yearMonthDuration")) {
    isValid = IsValidSchemaYearMonthDuration(aValue);
  } else if (aType.EqualsLiteral("dayTimeDuration")) {
    isValid = IsValidSchemaDayTimeDuration(aValue);
  } else if (aType.EqualsLiteral("listItem")) {
    isValid = IsValidSchemaListItem(aValue);
  } else if (aType.EqualsLiteral("listItems")) {
    isValid = IsValidSchemaListItems(aValue);
  }

  return isValid;
}

PRBool
nsXFormsSchemaValidator::IsValidSchemaYearMonthDuration(const nsAString & aValue)
{
  PRBool isValid = PR_FALSE;

  nsCOMPtr<nsISchemaDuration> duration;
  nsresult rv =
    mSchemaValidator->ValidateBuiltinTypeDuration(aValue,
                                                  getter_AddRefs(duration));

  // valid duration
  if (NS_SUCCEEDED(rv)) {
    // check if no days/hours/minutes/seconds/fractionseconds were set
    PRUint32 temp;
    double dbltemp;
    duration->GetDays(&temp);

    if (temp == 0) {
      duration->GetHours(&temp);
      if (temp == 0) {
        duration->GetMinutes(&temp);
        if (temp == 0) {
          duration->GetSeconds(&temp);
          if (temp == 0) {
            duration->GetFractionSeconds(&dbltemp);
            if (dbltemp == 0) {
              isValid = PR_TRUE;
            }
          }
        }
      }
    }
  }

  return isValid;
}

PRBool
nsXFormsSchemaValidator::IsValidSchemaDayTimeDuration(const nsAString & aValue)
{
  PRBool isValid = PR_FALSE;

  nsCOMPtr<nsISchemaDuration> duration;
  nsresult rv =
    mSchemaValidator->ValidateBuiltinTypeDuration(aValue,
                                                  getter_AddRefs(duration));

  if (NS_SUCCEEDED(rv)) {
    PRUint32 years, months;
    duration->GetYears(&years);
    duration->GetMonths(&months);

    // if years/months exist, invalid
    if ((years == 0) && (months == 0))
      isValid = PR_TRUE;
  }

  return isValid;
}

PRBool
nsXFormsSchemaValidator::IsValidSchemaListItem(const nsAString & aValue)
{
  PRBool isValid = PR_FALSE;

  // like a string, but no whitespace
  nsAutoString string(aValue);
  if (string.FindCharInSet(" \t\r\n") == kNotFound) {
    mSchemaValidator->ValidateString(aValue, NS_LITERAL_STRING("string"),
                                     NS_LITERAL_STRING("http://www.w3.org/1999/XMLSchema"),
                                     &isValid);
  }

  return isValid;
}

PRBool
nsXFormsSchemaValidator::IsValidSchemaListItems(const nsAString & aValue)
{
  PRBool isValid = PR_FALSE;

  // listItem is like a string, but no whitespace.  listItems is a whitespace
  // delimited list of listItem, so therefore just need to see if it is a valid
  // xsd:string
  mSchemaValidator->ValidateString(aValue, NS_LITERAL_STRING("string"),
                                   NS_LITERAL_STRING("http://www.w3.org/1999/XMLSchema"),
                                   &isValid);

  return isValid;
}

