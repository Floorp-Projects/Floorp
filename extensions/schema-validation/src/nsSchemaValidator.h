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

#ifndef __nsSchemaValidator_h__
#define __nsSchemaValidator_h__

#include "nsSchemaValidatorUtils.h"
#include "nsISchemaValidator.h"
#include "nsISchema.h"
#include "nsCOMPtr.h"
#include "nsCOMArray.h"
#include "nsIAtom.h"

/* eced2af3-fde9-4575-b5a4-e1c830b24611 */
#define NS_SCHEMAVALIDATOR_CID \
{ 0xeced2af3, 0xfde9, 0x4575, \
  {0xb5, 0xa4, 0xe1, 0xc8, 0x30, 0xb2, 0x46, 0x11}}

#define NS_SCHEMAVALIDATOR_CONTRACTID "@mozilla.org/schemavalidator;1"

#define NS_ERROR_SCHEMAVALIDATOR_NO_SCHEMA_LOADED      NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_SCHEMA, 1)
#define NS_ERROR_SCHEMAVALIDATOR_NO_DOM_NODE_SPECIFIED NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_SCHEMA, 2)
#define NS_ERROR_SCHEMAVALIDATOR_NO_TYPE_FOUND         NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_SCHEMA, 3)
#define NS_ERROR_SCHEMAVALIDATOR_TYPE_NOT_FOUND        NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_SCHEMA, 4)

class nsSchemaValidator : public nsISchemaValidator
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISCHEMAVALIDATOR

  nsSchemaValidator();

private:
  ~nsSchemaValidator();

  // methods dealing with simpletypes
  nsresult ValidateSimpletype(const nsAString & aNodeValue,
                              nsISchemaSimpleType *aSchemaSimpleType,
                              PRBool *aResult);
  nsresult ValidateDerivedSimpletype(const nsAString & aNodeValue,
                                     nsSchemaDerivedSimpleType *aDerived,
                                     PRBool *aResult);

  nsresult ValidateRestrictionSimpletype(const nsAString & aNodeValue,
                                         nsISchemaSimpleType *aSchemaSimpleType,
                                         PRBool *aResult);
  nsresult ValidateDerivedBuiltinType(const nsAString & aNodeValue,
                                      nsSchemaDerivedSimpleType *aDerived,
                                      PRBool *aResult);

  nsresult ValidateBuiltinType(const nsAString & aNodeValue,
                               nsISchemaSimpleType *aSchemaSimpleType,
                               PRBool *aResult);

  nsresult ValidateListSimpletype(const nsAString & aNodeValue,
                                  nsISchemaSimpleType *aSchemaSimpleType,
                                  nsSchemaDerivedSimpleType *aDerived,
                                  PRBool *aResult);

  nsresult ValidateUnionSimpletype(const nsAString & aNodeValue,
                                   nsISchemaSimpleType *aSchemaSimpleType,
                                   PRBool *aResult);
  nsresult ValidateDerivedUnionSimpletype(const nsAString & aNodeValue,
                                          nsSchemaDerivedSimpleType *aDerived,
                                          PRBool *aResult);

  // methods dealing with validation of built-in types
  nsresult ValidateBuiltinTypeString(const nsAString & aNodeValue,
                                     PRUint32 aLength, PRBool aLengthDefined,
                                     PRUint32 aMinLength, PRBool aMinLengthDefined,
                                     PRUint32 aMaxLength, PRBool aMaxLengthDefined,
                                     nsStringArray *aEnumerationList,
                                     PRBool *aResult);

  nsresult ValidateBuiltinTypeBoolean(const nsAString & aNodeValue,
                                      PRBool *aResult);

  nsresult ValidateBuiltinTypeGDay(const nsAString & aNodeValue,
                                   const nsAString & aMaxExclusive,
                                   const nsAString & aMinExclusive,
                                   const nsAString & aMaxInclusive,
                                   const nsAString & aMinInclusive,
                                   PRBool *aResult);
  PRBool IsValidSchemaGDay(const nsAString & aNodeValue, nsSchemaGDay *aResult);

  PRBool IsValidSchemaGType(const nsAString & aNodeValue, long aMinValue,
                            long aMaxValue, int *aResult);

  nsresult ValidateBuiltinTypeGMonth(const nsAString & aNodeValue,
                                     const nsAString & aMaxExclusive,
                                     const nsAString & aMinExclusive,
                                     const nsAString & aMaxInclusive,
                                     const nsAString & aMinInclusive,
                                     PRBool *aResult);
  PRBool IsValidSchemaGMonth(const nsAString & aNodeValue,
                             nsSchemaGMonth *aResult);

  nsresult ValidateBuiltinTypeGYear(const nsAString & aNodeValue,
                                    const nsAString & aMaxExclusive,
                                    const nsAString & aMinExclusive,
                                    const nsAString & aMaxInclusive,
                                    const nsAString & aMinInclusive,
                                    PRBool *aResult);
  PRBool IsValidSchemaGYear(const nsAString & aNodeValue,
                            nsSchemaGYear *aResult);

  nsresult ValidateBuiltinTypeGYearMonth(const nsAString & aNodeValue,
                                         const nsAString & aMaxExclusive,
                                         const nsAString & aMinExclusive,
                                         const nsAString & aMaxInclusive,
                                         const nsAString & aMinInclusive,
                                         PRBool *aResult);
  PRBool IsValidSchemaGYearMonth(const nsAString & aNodeValue,
                                 nsSchemaGYearMonth *aYearMonth);

  nsresult ValidateBuiltinTypeGMonthDay(const nsAString & aNodeValue,
                                        const nsAString & aMaxExclusive,
                                        const nsAString & aMinExclusive,
                                        const nsAString & aMaxInclusive,
                                        const nsAString & aMinInclusive,
                                        PRBool *aResult);
  PRBool IsValidSchemaGMonthDay(const nsAString & aNodeValue,
                                nsSchemaGMonthDay *aYearMonth);

  nsresult ValidateBuiltinTypeDateTime(const nsAString & aNodeValue,
                                       const nsAString & aMaxExclusive,
                                       const nsAString & aMinExclusive,
                                       const nsAString & aMaxInclusive,
                                       const nsAString & aMinInclusive,
                                       PRBool *aResult);
  int CompareSchemaDateTime(nsSchemaDateTime datetime1,
                            nsSchemaDateTime datetime2);
  PRBool IsValidSchemaDateTime(const nsAString & aNodeValue,
                               nsSchemaDateTime *aResult);

  nsresult ValidateBuiltinTypeDate(const nsAString & aNodeValue,
                                   const nsAString & aMaxExclusive,
                                   const nsAString & aMinExclusive,
                                   const nsAString & aMaxInclusive,
                                   const nsAString & aMinInclusive,
                                   PRBool *aResult);
  PRBool IsValidSchemaDate(const nsAString & aNodeValue, nsSchemaDate *aResult);

  nsresult ValidateBuiltinTypeTime(const nsAString & aNodeValue,
                                   const nsAString & aMaxExclusive,
                                   const nsAString & aMinExclusive,
                                   const nsAString & aMaxInclusive,
                                   const nsAString & aMinInclusive,
                                   PRBool *aResult);
  PRBool IsValidSchemaTime(const nsAString & aNodeValue, nsSchemaTime *aResult);

  nsresult ValidateBuiltinTypeDuration(const nsAString & aNodeValue,
                                       const nsAString & aMaxExclusive,
                                       const nsAString & aMinExclusive,
                                       const nsAString & aMaxInclusive,
                                       const nsAString & aMinInclusive,
                                       PRBool *aResult);
  PRBool IsValidSchemaDuration(const nsAString & aNodeValue,
                               nsISchemaDuration **aResult);

  nsresult ValidateBuiltinTypeInteger(const nsAString & aNodeValue,
                                      PRUint32 aTotalDigits,
                                      const nsAString & aMaxExclusive,
                                      const nsAString & aMinExclusive,
                                      const nsAString & aMaxInclusive,
                                      const nsAString & aMinInclusive,
                                      nsStringArray *aEnumerationList,
                                      PRBool *aResult);

  nsresult ValidateBuiltinTypeFloat(const nsAString & aNodeValue,
                                    PRUint32 aTotalDigits,
                                    const nsAString & aMaxExclusive,
                                    const nsAString & aMinExclusive,
                                    const nsAString & aMaxInclusive,
                                    const nsAString & aMinInclusive,
                                    nsStringArray *aEnumerationList,
                                    PRBool *aResult);
  PRBool IsValidSchemaFloat(const nsAString & aNodeValue, float *aResult);

  nsresult ValidateBuiltinTypeDouble(const nsAString & aNodeValue,
                                    PRUint32 aTotalDigits,
                                    const nsAString & aMaxExclusive,
                                    const nsAString & aMinExclusive,
                                    const nsAString & aMaxInclusive,
                                    const nsAString & aMinInclusive,
                                    nsStringArray *aEnumerationList,
                                    PRBool *aResult);
  PRBool IsValidSchemaDouble(const nsAString & aNodeValue, double *aResult);

  nsresult ValidateBuiltinTypeByte(const nsAString & aNodeValue,
                                   PRUint32 aTotalDigits,
                                   const nsAString & aMaxExclusive,
                                   const nsAString & aMaxInclusive,
                                   const nsAString & aMinInclusive,
                                   const nsAString & aMinExclusive,
                                   nsStringArray *aEnumerationList,
                                   PRBool *aResult);
  PRBool IsValidSchemaByte(const nsAString & aNodeValue, long *aResult);

  nsresult ValidateBuiltinTypeDecimal(const nsAString & aNodeValue,
                                      PRUint32 aTotalDigits,
                                      PRUint32 aTotalFractionDigits,
                                      PRBool aFractionDigitsSet,
                                      const nsAString & aMaxExclusive,
                                      const nsAString & aMinExclusive,
                                      const nsAString & aMaxInclusive,
                                      const nsAString & aMinInclusive,
                                      nsStringArray *aEnumerationList,
                                      PRBool *aResult);
  PRBool IsValidSchemaDecimal(const nsAString & aNodeValue, nsAString & aWholePart,
                              nsAString & aFractionPart);
  int CompareFractionStrings(const nsAString & aString1,
                             const nsAString & aString2);

  nsresult ValidateBuiltinTypeAnyURI(const nsAString & aNodeValue,
                                     PRUint32 aLength, PRUint32 aMinLength,
                                     PRUint32 aMaxLength,
                                     nsStringArray *aEnumerationList,
                                     PRBool *aResult);
  PRBool IsValidSchemaAnyURI(const nsAString & aString);

  nsresult ValidateBuiltinTypeBase64Binary(const nsAString & aNodeValue,
                                           PRUint32 aLength,
                                           PRBool aLengthDefined,
                                           PRUint32 aMinLength,
                                           PRBool aMinLengthDefined,
                                           PRUint32 aMaxLength,
                                           PRBool aMaxLengthDefined,
                                           nsStringArray *aEnumerationList,
                                           PRBool *aResult);
  PRBool IsValidSchemaBase64Binary(const nsAString & aString,
                                   char** aDecodedString);


  nsresult ValidateBuiltinTypeHexBinary(const nsAString & aNodeValue,
                                        PRUint32 aLength,
                                        PRBool aLengthDefined,
                                        PRUint32 aMinLength,
                                        PRBool aMinLengthDefined,
                                        PRUint32 aMaxLength,
                                        PRBool aMaxLengthDefined,
                                        nsStringArray *aEnumerationList,
                                        PRBool *aResult);
  PRBool IsValidSchemaHexBinary(const nsAString & aString);


  nsresult ValidateBuiltinTypeQName(const nsAString & aNodeValue,
                                    PRUint32 aLength,
                                    PRBool aLengthDefined,
                                    PRUint32 aMinLength,
                                    PRBool aMinLengthDefined,
                                    PRUint32 aMaxLength,
                                    PRBool aMaxLengthDefined,
                                    nsStringArray *aEnumerationList,
                                    PRBool *aResult);
  PRBool IsValidSchemaQName(const nsAString & aString);

  // helper methods
  void DumpBaseType(nsISchemaBuiltinType *aBuiltInType);

  // methods dealing with complextypes
  nsresult ValidateComplextype(nsIDOMNode *aNode,
                               nsISchemaComplexType *aSchemaComplexType,
                               PRBool *aResult);

  nsresult ValidateComplexModelElement(nsIDOMNode *aNode,
                                       nsISchemaComplexType *aSchemaComplexType,
                                       PRBool *aResult);

  nsresult ValidateComplexModelEmpty(nsIDOMNode    *aNode,
                              nsISchemaComplexType *aSchemaComplexType,
                              PRBool               *aResult);

  nsresult ValidateComplexModelSimple(nsIDOMNode *aNode,
                                      nsISchemaComplexType *aSchemaComplexType,
                                      PRBool *aResult);

  nsresult ValidateComplexModelGroup(nsIDOMNode* aNode,
                                     nsISchemaModelGroup *aSchemaModelGroup,
                                     nsIDOMNode **aLeftOvers, PRBool *aResult);

  nsresult ValidateComplexSequence(nsIDOMNode *aStartNode,
                                   nsISchemaModelGroup *aSchemaModelGroup,
                                   nsIDOMNode **aLeftOvers, PRBool *aNotFound,
                                   PRBool *aResult);

  nsresult ValidateComplexParticle(nsIDOMNode* aNode,
                                   nsISchemaParticle *aSchemaParticle,
                                   nsIDOMNode **aLeftOvers, PRBool *aNotFound,
                                   PRBool *aResult);

  nsresult ValidateComplexElement(nsIDOMNode* aNode,
                                  nsISchemaParticle *aSchemaParticle,
                                  PRBool *aResult);

  nsresult ValidateComplexChoice(nsIDOMNode* aStartNode,
                                 nsISchemaModelGroup *aSchemaModelGroup,
                                 nsIDOMNode **aLeftOvers, PRBool *aNotFound,
                                 PRBool *aResult);

  nsresult ValidateComplexAll(nsIDOMNode* aStartNode,
                              nsISchemaModelGroup *aSchemaModelGroup,
                              nsIDOMNode **aLeftOvers, PRBool *aNotFound,
                              PRBool *aResult);

  nsresult ValidateAttributeComponent(nsIDOMNode* aNode,
                                      nsISchemaAttributeComponent *aAttrComp,
                                      PRUint32 *aFoundAttrCount, PRBool *aResult);
  nsresult ValidateSchemaAttribute(nsIDOMNode* aNode, nsISchemaAttribute *aAttr,
                                   const nsAString & aAttrName,
                                   PRUint32 *aFoundAttrCount, PRBool *aResult);
  nsresult ValidateSchemaAttributeGroup(nsIDOMNode* aNode,
                                        nsISchemaAttributeGroup *aAttr,
                                        const nsAString & aAttrName,
                                        PRUint32 *aFoundAttrCount,
                                        PRBool *aResult);

static void
ReleaseObject(void    *aObject,
              nsIAtom *aPropertyName,
              void    *aPropertyValue,
              void    *aData)
{
  NS_STATIC_CAST(nsISupports *, aPropertyValue)->Release();
}

protected:
  nsCOMPtr<nsISchemaCollection> mSchema;
  PRBool mForceInvalid;
};

#endif // __nsSchemaValidator_h__
