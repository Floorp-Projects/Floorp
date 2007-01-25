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
#include "nsISchema.h"
#include "nsIStringBundle.h"
#include "nsServiceManagerUtils.h"

class nsXFormsInputElement : public nsXFormsDelegateStub
{
public:

  NS_IMETHOD IsTypeAllowed(PRUint16 aType, PRBool *aIsAllowed,
                           nsRestrictionFlag *aRestriction,
                           nsAString &aUnallowedTypes);

  virtual PRBool IsContentAllowed();

  nsXFormsInputElement(const nsAString& aType)
    : nsXFormsDelegateStub(aType)
    {}
};

NS_IMETHODIMP
nsXFormsInputElement::IsTypeAllowed(PRUint16 aType, PRBool *aIsAllowed,
                                    nsRestrictionFlag *aRestriction,
                                    nsAString &aUnallowedTypes)
{
  NS_ENSURE_ARG_POINTER(aRestriction);
  NS_ENSURE_ARG_POINTER(aIsAllowed);
  *aRestriction = eTypes_Exclusive;
  *aIsAllowed = PR_FALSE;

  // For input and secret elements, the bound type can't be xsd:hexBinary
  // or xsd:base64Binary

  if (aType != nsISchemaBuiltinType::BUILTIN_TYPE_HEXBINARY &&
      aType != nsISchemaBuiltinType::BUILTIN_TYPE_BASE64BINARY) {
    *aIsAllowed = PR_TRUE;
    return NS_OK;
  }

  // build the string of types that inputs can't bind to
  aUnallowedTypes.AssignLiteral("xsd:base64Binary xsd:hexBinary");
  return NS_OK;
}

PRBool
nsXFormsInputElement::IsContentAllowed()
{
  PRBool isAllowed = PR_TRUE;

  // For input and secret elements, complexContent is not allowed.
  PRBool isComplex = IsContentComplex();
  if (isComplex) {
    isAllowed = PR_FALSE;
  }
  return isAllowed;
}

class nsXFormsTextareaElement : public nsXFormsDelegateStub
{
public:

  NS_IMETHOD IsTypeAllowed(PRUint16 aType, PRBool *aIsAllowed,
                           nsRestrictionFlag *aRestriction,
                           nsAString &aAllowedTypes);

  virtual PRBool IsContentAllowed();

  nsXFormsTextareaElement()
    : nsXFormsDelegateStub(NS_LITERAL_STRING("textarea"))
    {}
};

NS_IMETHODIMP
nsXFormsTextareaElement::IsTypeAllowed(PRUint16 aType, PRBool *aIsAllowed,
                                       nsRestrictionFlag *aRestriction,
                                       nsAString &aAllowedTypes)
{
  NS_ENSURE_ARG_POINTER(aRestriction);
  NS_ENSURE_ARG_POINTER(aIsAllowed);
  *aRestriction = eTypes_Inclusive;
  *aIsAllowed = PR_FALSE;

  // Textareas can only be bound to types of xsd:string (or a type derived from
  // xsd:string).

  if (aType == nsISchemaBuiltinType::BUILTIN_TYPE_STRING) {
    *aIsAllowed = PR_TRUE;
    return NS_OK;
  }

  // build the string of types that textareas can bind to
  aAllowedTypes.AssignLiteral("xsd:string");
  return NS_OK;
}

PRBool
nsXFormsTextareaElement::IsContentAllowed()
{
  PRBool isAllowed = PR_TRUE;

  // Textarea elements may not be bound to complexContent.
  PRBool isComplex = IsContentComplex();
  if (isComplex) {
    isAllowed = PR_FALSE;
  }
  return isAllowed;
}

// Creators

NS_HIDDEN_(nsresult)
NS_NewXFormsInputElement(nsIXTFElement **aResult)
{
  *aResult = new nsXFormsInputElement(NS_LITERAL_STRING("input"));
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*aResult);
  return NS_OK;
}

NS_HIDDEN_(nsresult)
NS_NewXFormsSecretElement(nsIXTFElement **aResult)
{
  *aResult = new nsXFormsInputElement(NS_LITERAL_STRING("secret"));
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*aResult);
  return NS_OK;
}

NS_HIDDEN_(nsresult)
NS_NewXFormsTextAreaElement(nsIXTFElement **aResult)
{
  *aResult = new nsXFormsTextareaElement();
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*aResult);
  return NS_OK;
}
