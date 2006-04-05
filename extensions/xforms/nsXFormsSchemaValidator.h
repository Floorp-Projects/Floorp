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
 * The Original Code is Mozilla Schema Validation.
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

#include "nsCOMPtr.h"
#include "nsISchemaValidator.h"

class nsXFormsSchemaValidator
{
public:
  nsXFormsSchemaValidator();

  /**
   * Loads a schema into the validator
   *
   * @param aSchema           The schema to load
   * @return                  If it succeeded or not
   */
  nsresult LoadSchema(nsISchema* aSchema);

  /**
   * Validates a string against a namespace and schema type
   *
   * @param aValue            Value to validate
   * @param aType             Type
   * @param aNamespace        Namespace
   * @return                  Whether the string is valid or not
   */
  PRBool ValidateString(const nsAString & aValue, const nsAString & aType,
                        const nsAString & aNamespace);

  /**
   * Validates a node and its subtree against the loaded schema(s)
   *
   * @param aElement          Node to validate
   * @return                  Whether the string is valid or not
   */
  PRBool Validate(nsIDOMNode* aElement);

  /**
   * Returns a nsISchemaType given an type and namespace
   *
   * @param aType             Type
   * @param aNamespace        Namespace
   * @param aSchemaType       The nsISchemaType is stored here.
   * @return                  Whether a type was found or not
   */
  PRBool GetType(const nsAString & aType, const nsAString & aNamespace,
                 nsISchemaType **aSchemaType);

  /**
   * Validates a string against an XForms Schema Type
   *
   * @param aValue            Value to validate
   * @param aType             Type
   * @return                  Whether the string is valid or not
   */
  PRBool ValidateXFormsTypeString(const nsAString & aValue,
                                  const nsAString & aType);

  // Returns if the string is a valid XForms YearMonth duration
  PRBool IsValidSchemaYearMonthDuration(const nsAString & aValue);

  // Returns if the string is a valid XForms DayTime duration
  PRBool IsValidSchemaDayTimeDuration(const nsAString & aValue);

  // Returns if the string is a valid XForms listitem
  PRBool IsValidSchemaListItem(const nsAString & aValue);

  // Returns if the string is a valid XForms list of listitems
  PRBool IsValidSchemaListItems(const nsAString & aValue);

protected:
  nsCOMPtr<nsISchemaValidator> mSchemaValidator;
};

