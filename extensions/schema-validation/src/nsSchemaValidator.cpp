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
 * Portions created by the Initial Developer are Copyright (C) 2004-2005
 * IBM Corporation. All Rights Reserved.
 *
 * Contributor(s):
 *   Doron Rosenberg <doronr@us.ibm.com> (original author)
 *   Laurent Jouanneau <laurent@xulfr.org>
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

#include "nsSchemaValidator.h"

// content includes
#include "nsIDOMNode.h"
#include "nsIDOM3Node.h"
#include "nsIDOMElement.h"
#include "nsIDOMNodeList.h"
#include "nsIParserService.h"
#include "nsIDOMNamedNodeMap.h"

// string includes
#include "nsReadableUtils.h"
#include "nsString.h"
#include "nsUnicharUtils.h"

// XPCOM includes
#include "nsMemory.h"
#include "nsIServiceManager.h"
#include "nsIComponentManager.h"

#include "nsISchema.h"
#include "nsISchemaLoader.h"
#include "nsSchemaValidatorUtils.h"
#include "nsISchemaValidatorRegexp.h"

#include "nsNetUtil.h"
#include "prlog.h"
#include "nspr.h"

#include <stdlib.h>
#include <math.h>
#include "prprf.h"
#include "prtime.h"
#include "plbase64.h"

#define NS_SCHEMA_1999_NAMESPACE "http://www.w3.org/1999/XMLSchema"
#define NS_SCHEMA_2001_NAMESPACE "http://www.w3.org/2001/XMLSchema"
#define kREGEXP_CID "@mozilla.org/xmlextras/schemas/schemavalidatorregexp;1"

#ifdef PR_LOGGING
PRLogModuleInfo *gSchemaValidationLog = PR_NewLogModule("schemaValidation");

#define LOG(x) PR_LOG(gSchemaValidationLog, PR_LOG_DEBUG, x)
#define LOG_ENABLED() PR_LOG_TEST(gSchemaValidationLog, PR_LOG_DEBUG)
#else
#define LOG(x)
#endif

NS_IMPL_ISUPPORTS1_CI(nsSchemaValidator, nsISchemaValidator)

nsSchemaValidator::nsSchemaValidator()
{
}

nsSchemaValidator::~nsSchemaValidator()
{
}

////////////////////////////////////////////////////////////
//
// nsSchemaValidator implementation
//
////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsSchemaValidator::LoadSchema(nsISchema* aSchema)
{
  if (aSchema)
    aSchema->GetCollection(getter_AddRefs(mSchema));
  return NS_OK;
}

NS_IMETHODIMP
nsSchemaValidator::ValidateString(const nsAString & aValue,
                                  const nsAString & aType,
                                  const nsAString & aNamespace,
                                  PRBool *aResult)
{
  LOG((" --------- nsSchemaValidator::ValidateString called --------- "));

  // empty type is invalid
  if (aType.IsEmpty())
    return NS_ERROR_NOT_AVAILABLE;

  // no schemas loaded and type is not builtin, abort
  if (!mSchema && !aNamespace.EqualsLiteral(NS_SCHEMA_1999_NAMESPACE) &&
      !aNamespace.EqualsLiteral(NS_SCHEMA_2001_NAMESPACE))
    return NS_ERROR_SCHEMAVALIDATOR_NO_SCHEMA_LOADED;

  // figure out if it's a simple or complex type
  nsCOMPtr<nsISchemaType> type;
  nsresult rv = GetType(aType, aNamespace, getter_AddRefs(type));
  NS_ENSURE_SUCCESS(rv, rv);

  PRUint16 typevalue;
  rv = type->GetSchemaType(&typevalue);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool isValid = PR_FALSE;
  if (typevalue == nsISchemaType::SCHEMA_TYPE_SIMPLE) {
    nsCOMPtr<nsISchemaSimpleType> simpleType = do_QueryInterface(type);
    NS_ENSURE_TRUE(simpleType, NS_ERROR_FAILURE);

    LOG((" Type is a simple type! \n String to validate is: |%s|",
         NS_ConvertUTF16toUTF8(aValue).get()));

    rv = ValidateSimpletype(aValue, simpleType, &isValid);
  } else {
    // if its not a simpletype, validating a string makes no sense.
    rv = NS_ERROR_UNEXPECTED;
  }

  *aResult = isValid;
  return rv;
}

NS_IMETHODIMP
nsSchemaValidator::Validate(nsIDOMNode* aElement, PRBool *aResult)
{
  LOG((" \n \n nsSchemaValidator::Validate called."));

  if (!aElement)
    return NS_ERROR_SCHEMAVALIDATOR_NO_DOM_NODE_SPECIFIED;

  nsCOMPtr<nsIDOMElement> domElement = do_QueryInterface(aElement);
  NS_ENSURE_STATE(domElement);

  nsAutoString typeAttribute;
  nsresult rv = domElement->GetAttributeNS(NS_LITERAL_STRING(NS_SCHEMA_1999_NAMESPACE),
                                           NS_LITERAL_STRING("type"),
                                           typeAttribute);
  NS_ENSURE_SUCCESS(rv, rv);
  LOG((" \n Type is: %s \n", NS_ConvertUTF16toUTF8(typeAttribute).get()));
  if (typeAttribute.IsEmpty())
    return NS_ERROR_SCHEMAVALIDATOR_NO_TYPE_FOUND;

  // split type (ns:type) into namespace and type.
  nsCOMPtr<nsIParserService> parserService =
    do_GetService("@mozilla.org/parser/parser-service;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  const nsAFlatString& qName = PromiseFlatString(typeAttribute);
  const PRUnichar *colon;
  rv = parserService->CheckQName(qName, PR_TRUE, &colon);
  NS_ENSURE_SUCCESS(rv, rv);

  const PRUnichar* end;
  qName.EndReading(end);

  nsCOMPtr<nsIDOM3Node> domNode3 = do_QueryInterface(aElement);
  NS_ENSURE_STATE(domNode3);

  nsAutoString schemaTypePrefix, schemaType, schemaTypeNamespace;
  if (!colon) {
    // colon not found, so no prefix
    schemaType.Assign(typeAttribute);

    // get namespace from node
    aElement->GetNamespaceURI(schemaTypeNamespace);
  } else {
    schemaTypePrefix.Assign(Substring(qName.get(), colon));
    schemaType.Assign(Substring(colon + 1, end));

    // get the namespace url from the prefix
    rv = domNode3->LookupNamespaceURI(schemaTypePrefix, schemaTypeNamespace);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  LOG(("  Type to validate against is %s:%s",
    NS_LossyConvertUTF16toASCII(schemaTypePrefix).get(),
    NS_LossyConvertUTF16toASCII(schemaType).get()));

  // no schemas loaded and type is not builtin, abort
  if (!mSchema &&
      !schemaTypeNamespace.EqualsLiteral(NS_SCHEMA_1999_NAMESPACE) &&
      !schemaTypeNamespace.EqualsLiteral(NS_SCHEMA_2001_NAMESPACE))
    return NS_ERROR_SCHEMAVALIDATOR_NO_SCHEMA_LOADED;

  // get the type
  nsCOMPtr<nsISchemaType> type;
  rv = GetType(schemaType, schemaTypeNamespace, getter_AddRefs(type));
  NS_ENSURE_SUCCESS(rv, rv);
  return ValidateAgainstType(aElement, type, aResult);
}

NS_IMETHODIMP
nsSchemaValidator::ValidateAgainstType(nsIDOMNode* aElement,
                                       nsISchemaType* aType,
                                       PRBool *aResult)
{
  if (!aElement)
    return NS_ERROR_SCHEMAVALIDATOR_NO_DOM_NODE_SPECIFIED;

  NS_ENSURE_STATE(aType);

  PRUint16 typevalue;
  nsresult rv = aType->GetSchemaType(&typevalue);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool isValid = PR_FALSE;
  if (typevalue == nsISchemaType::SCHEMA_TYPE_SIMPLE) {
    nsCOMPtr<nsISchemaSimpleType> simpleType = do_QueryInterface(aType);

    if (simpleType) {
      // get the nodevalue using DOM 3's textContent
      nsAutoString nodeValue;
      nsCOMPtr<nsIDOM3Node> domNode3 = do_QueryInterface(aElement);
      domNode3->GetTextContent(nodeValue);

      LOG(("  Type is a simple type!"));
      LOG(("  String to validate is: |%s|",
        NS_ConvertUTF16toUTF8(nodeValue).get()));

      rv = ValidateSimpletype(nodeValue, simpleType, &isValid);
    }
  } else if (typevalue == nsISchemaType::SCHEMA_TYPE_COMPLEX) {
    rv = NS_ERROR_NOT_IMPLEMENTED;
  } else {
    rv = NS_ERROR_UNEXPECTED;
  }

  *aResult = isValid;
  return rv;
}

/*
  Returns the nsISchemaType for a given value/type/namespace pair.
*/
NS_IMETHODIMP
nsSchemaValidator::GetType(const nsAString & aType,
                           const nsAString & aNamespace,
                           nsISchemaType ** aSchemaType)
{
  nsresult rv;

  if (!mSchema) {
    // if we are a built-in type, we can get a nsISchemaType for it from
    // nsISchemaCollection->GetType.
    nsCOMPtr<nsISchemaLoader> schemaLoader =
      do_GetService("@mozilla.org/xmlextras/schemas/schemaloader;1", &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    mSchema = do_QueryInterface(schemaLoader);
    NS_ENSURE_STATE(mSchema);
  }

  // First try looking for xsd:type
  rv = mSchema->GetType(aType, aNamespace, aSchemaType);
  NS_ENSURE_SUCCESS(rv, rv);

  // maybe its a xsd:element
  if (!*aSchemaType) {
    nsCOMPtr<nsISchemaElement> schemaElement;
    rv = mSchema->GetElement(aType, aNamespace, getter_AddRefs(schemaElement));
    NS_ENSURE_SUCCESS(rv, rv);

    if (schemaElement) {
      rv = schemaElement->GetType(aSchemaType);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    if (!*aSchemaType) {
      // if its not a built-in type as well, its an unknown schema type.
      LOG(("    Error: The Schema type was not found.\n"));
      rv = NS_ERROR_SCHEMAVALIDATOR_TYPE_NOT_FOUND;
    }
  }

  return rv;
}

nsresult
nsSchemaValidator::ValidateSimpletype(const nsAString & aNodeValue,
                                      nsISchemaSimpleType *aSchemaSimpleType,
                                      PRBool *aResult)
{
  // get the simpletype type.
  PRUint16 simpleTypeValue;
  nsresult rv = aSchemaSimpleType->GetSimpleType(&simpleTypeValue);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool isValid = PR_FALSE;
  switch (simpleTypeValue) {
    case nsISchemaSimpleType::SIMPLE_TYPE_RESTRICTION: {
      // handle simpletypes with restrictions
      rv = ValidateRestrictionSimpletype(aNodeValue, aSchemaSimpleType, &isValid);
      break;
    }

    case nsISchemaSimpleType::SIMPLE_TYPE_BUILTIN: {
      // handle built-in types
      rv = ValidateBuiltinType(aNodeValue, aSchemaSimpleType, &isValid);
      break;
    }

    case nsISchemaSimpleType::SIMPLE_TYPE_LIST: {
      // handle lists
      rv = ValidateListSimpletype(aNodeValue, aSchemaSimpleType, &isValid);
      break;
    }

    case nsISchemaSimpleType::SIMPLE_TYPE_UNION: {
      // handle unions
      rv = ValidateUnionSimpletype(aNodeValue, aSchemaSimpleType, &isValid);
      break;
    }
  }

  *aResult = isValid;
  return rv;
}

/**
 *  Simpletype restrictions allow restricting a built-in type with several
 *  facets, such as totalDigits or maxLength.
 */
nsresult
nsSchemaValidator::ValidateRestrictionSimpletype(const nsAString & aNodeValue,
                                                 nsISchemaSimpleType *aType,
                                                 PRBool *aResult)
{
  nsCOMPtr<nsISchemaRestrictionType> restrictionType = do_QueryInterface(aType);
  NS_ENSURE_STATE(restrictionType);

  LOG(("  The simpletype definition contains restrictions."));

  nsCOMPtr<nsISchemaSimpleType> simpleBaseType;
  nsresult rv = restrictionType->GetBaseType(getter_AddRefs(simpleBaseType));
  NS_ENSURE_SUCCESS(rv, rv);

  // get the amount of restriction facet defined.
  PRUint32 facetCount;
  rv = restrictionType->GetFacetCount(&facetCount);
  NS_ENSURE_SUCCESS(rv, rv);
  LOG(("    %d facet(s) defined.", facetCount));

  // we create variables for all possible restriction facets and populate them
  // if we run into them.  This is faster than having the same code repeated
  // for each built-in type and only handling the relevant facets.
  PRBool isLengthDefined = PR_FALSE;
  PRUint32 length = 0;
  PRBool isMinLengthDefined = PR_FALSE;
  PRUint32 minLength = 0;
  PRBool isMaxLengthDefined = PR_FALSE;
  PRUint32 maxLength = 0;
  nsAutoString pattern;
  unsigned short whitespace = 0;
  nsAutoString maxInclusive;
  nsAutoString minInclusive;
  nsAutoString maxExclusive;
  nsAutoString minExclusive;
  PRUint32 totalDigits = 0;
  // since fractionDigits is 0..n, need a bool to remember if it was set or not.
  PRBool fractionDigitsSet = PR_FALSE;
  PRUint32 fractionDigits = 0;
  nsAutoString enumeration;
  nsStringArray enumerationList;

  nsCOMPtr<nsISchemaFacet> facet;
  PRUint32 facetCounter;
  PRUint16 facetType;

  // handle all defined facets
  for (facetCounter = 0; facetCounter < facetCount; ++facetCounter) {
    rv = restrictionType->GetFacet(facetCounter, getter_AddRefs(facet));
    NS_ENSURE_SUCCESS(rv, rv);

    facet->GetFacetType(&facetType);

    switch (facetType) {
      case nsISchemaFacet::FACET_TYPE_LENGTH: {
        isLengthDefined = PR_TRUE;
        facet->GetLengthValue(&length);
        LOG(("  - Length Facet found (value is %d)", length));
        break;
      }

      case nsISchemaFacet::FACET_TYPE_MINLENGTH: {
        isMinLengthDefined = PR_TRUE;
        facet->GetLengthValue(&minLength);
        LOG(("  - Min Length Facet found (value is %d)", minLength));
        break;
      }

      case nsISchemaFacet::FACET_TYPE_MAXLENGTH: {
        isMaxLengthDefined = PR_TRUE;
        facet->GetLengthValue(&maxLength);
        LOG(("  - Max Length Facet found (value is %d)", maxLength));
        break;
      }

      case nsISchemaFacet::FACET_TYPE_PATTERN: {
        facet->GetValue(pattern);
        LOG(("  - Pattern Facet found (value is %s)",
             NS_ConvertUTF16toUTF8(pattern).get()));
        break;
      }

      case nsISchemaFacet::FACET_TYPE_ENUMERATION: {
        facet->GetValue(enumeration);
        enumerationList.AppendString(enumeration);
        LOG(("  - Enumeration found (%s)",
             NS_ConvertUTF16toUTF8(enumeration).get()));
        break;
      }

      case nsISchemaFacet::FACET_TYPE_WHITESPACE: {
        // XXX whitespace not supported yet
        facet->GetWhitespaceValue(&whitespace);
        break;
      }

      case nsISchemaFacet::FACET_TYPE_MAXINCLUSIVE: {
        facet->GetValue(maxInclusive);
        LOG(("  - Max Inclusive Facet found (value is %s)",
          NS_ConvertUTF16toUTF8(maxInclusive).get()));
        break;
      }

      case nsISchemaFacet::FACET_TYPE_MININCLUSIVE: {
        facet->GetValue(minInclusive);
        LOG(("  - Min Inclusive Facet found (value is %s)",
          NS_ConvertUTF16toUTF8(minInclusive).get()));
        break;
      }

      case nsISchemaFacet::FACET_TYPE_MAXEXCLUSIVE: {
        facet->GetValue(maxExclusive);
        LOG(("  - Max Exclusive Facet found (value is %s)",
          NS_ConvertUTF16toUTF8(maxExclusive).get()));
        break;
      }

      case nsISchemaFacet::FACET_TYPE_MINEXCLUSIVE: {
        facet->GetValue(minExclusive);
        LOG(("  - Min Exclusive Facet found (value is %s)",
          NS_ConvertUTF16toUTF8(minExclusive).get()));
        break;
      }

      case nsISchemaFacet::FACET_TYPE_TOTALDIGITS: {
        facet->GetDigitsValue(&totalDigits);
        LOG(("  - Totaldigits Facet found (value is %d)", totalDigits));
        break;
      }

      case nsISchemaFacet::FACET_TYPE_FRACTIONDIGITS: {
        fractionDigitsSet = PR_TRUE;
        facet->GetDigitsValue(&fractionDigits);
        LOG(("  - Fractiondigits Facet found (value is %d)", fractionDigits));
        break;
      }
    }
  }

  // now that we have loaded all the restriction facets,
  // check the base type and validate
  PRUint16 builtinTypeValue;
  nsCOMPtr<nsISchemaBuiltinType> schemaBuiltinType =
    do_QueryInterface(simpleBaseType);
  NS_ENSURE_STATE(schemaBuiltinType);

  schemaBuiltinType->GetBuiltinType(&builtinTypeValue);

#ifdef PR_LOGGING
  DumpBaseType(schemaBuiltinType);
#endif

  PRBool isValid = PR_FALSE;
  switch(builtinTypeValue) {
    case nsISchemaBuiltinType::BUILTIN_TYPE_STRING: {
      rv = ValidateBuiltinTypeString(aNodeValue, length, isLengthDefined,
                                     minLength, isMinLengthDefined,
                                     maxLength, isMaxLengthDefined,
                                     &enumerationList, &isValid);
      break;
    }

    case nsISchemaBuiltinType::BUILTIN_TYPE_BOOLEAN: {
      rv = ValidateBuiltinTypeBoolean(aNodeValue, &isValid);
      break;
    }

    case nsISchemaBuiltinType::BUILTIN_TYPE_GDAY: {
      rv = ValidateBuiltinTypeGDay(aNodeValue, maxExclusive, minExclusive,
                                   maxInclusive, minInclusive, &isValid);
      break;
    }

    case nsISchemaBuiltinType::BUILTIN_TYPE_GMONTH: {
      rv = ValidateBuiltinTypeGMonth(aNodeValue, maxExclusive, minExclusive,
                                     maxInclusive, minInclusive, &isValid);
      break;
    }

    case nsISchemaBuiltinType::BUILTIN_TYPE_GYEAR: {
      rv = ValidateBuiltinTypeGYear(aNodeValue, maxExclusive, minExclusive,
                                    maxInclusive, minInclusive, &isValid);
      break;
    }

    case nsISchemaBuiltinType::BUILTIN_TYPE_GYEARMONTH: {
      rv = ValidateBuiltinTypeGYearMonth(aNodeValue, maxExclusive, minExclusive,
                                         maxInclusive, minInclusive, &isValid);
      break;
    }

    case nsISchemaBuiltinType::BUILTIN_TYPE_GMONTHDAY: {
      rv = ValidateBuiltinTypeGMonthDay(aNodeValue, maxExclusive, minExclusive,
                                        maxInclusive, minInclusive, &isValid);
      break;
    }

    case nsISchemaBuiltinType::BUILTIN_TYPE_DATE: {
      rv = ValidateBuiltinTypeDate(aNodeValue, maxExclusive, minExclusive,
                                   maxInclusive, minInclusive, &isValid);
      break;
    }

    case nsISchemaBuiltinType::BUILTIN_TYPE_TIME: {
      rv = ValidateBuiltinTypeTime(aNodeValue, maxExclusive, minExclusive,
                                   maxInclusive, minInclusive, &isValid);
      break;
    }

    case nsISchemaBuiltinType::BUILTIN_TYPE_DATETIME: {
      rv = ValidateBuiltinTypeDateTime(aNodeValue, maxExclusive, minExclusive,
                                       maxInclusive, minInclusive, &isValid);
      break;
    }

    case nsISchemaBuiltinType::BUILTIN_TYPE_DURATION: {
      rv = ValidateBuiltinTypeDuration(aNodeValue, maxExclusive, minExclusive,
                                       maxInclusive, minInclusive, &isValid);
      break;
    }

    case nsISchemaBuiltinType::BUILTIN_TYPE_INTEGER: {
      rv = ValidateBuiltinTypeInteger(aNodeValue, totalDigits, maxExclusive,
                                      minExclusive, maxInclusive, minInclusive,
                                      &enumerationList, &isValid);
      break;
    }

    /* http://w3.org/TR/xmlschema-2/#nonPositiveInteger */
    case nsISchemaBuiltinType::BUILTIN_TYPE_NONPOSITIVEINTEGER: {
      if (maxExclusive.IsEmpty()) {
        maxExclusive.AssignLiteral("1");
      }

      if (minInclusive.IsEmpty()) {
        minInclusive.AssignLiteral("0");
      }

      rv = ValidateBuiltinTypeInteger(aNodeValue, totalDigits, maxExclusive,
                                      minExclusive, maxInclusive, minInclusive,
                                      &enumerationList, &isValid);
      break;
    }

    /* http://www.w3.org/TR/xmlschema-2/#negativeInteger */
    case nsISchemaBuiltinType::BUILTIN_TYPE_NEGATIVEINTEGER: {
      if (maxExclusive.IsEmpty()) {
        maxExclusive.AssignLiteral("0");
      }

      if (minInclusive.IsEmpty()) {
        minInclusive.AssignLiteral("-1");
      }

      rv = ValidateBuiltinTypeInteger(aNodeValue, totalDigits,maxExclusive,
                                      minExclusive, maxInclusive, minInclusive,
                                      &enumerationList, &isValid);
      break;
    }

    case nsISchemaBuiltinType::BUILTIN_TYPE_BYTE: {
      rv = ValidateBuiltinTypeByte(aNodeValue, totalDigits, maxExclusive,
                                   minExclusive, maxInclusive, minInclusive,
                                   &enumerationList, &isValid);
      break;
    }

    case nsISchemaBuiltinType::BUILTIN_TYPE_FLOAT: {
      rv = ValidateBuiltinTypeFloat(aNodeValue, totalDigits, maxExclusive,
                                    minExclusive, maxInclusive, minInclusive,
                                    &enumerationList, &isValid);
      break;
    }

    case nsISchemaBuiltinType::BUILTIN_TYPE_DECIMAL: {
      rv = ValidateBuiltinTypeDecimal(aNodeValue, totalDigits, fractionDigits,
                                      fractionDigitsSet, maxExclusive,
                                      minExclusive, maxInclusive, minInclusive,
                                      &enumerationList, &isValid);
      break;
    }

    case nsISchemaBuiltinType::BUILTIN_TYPE_ANYURI: {
      rv = ValidateBuiltinTypeAnyURI(aNodeValue, length, minLength, maxLength,
                                     &enumerationList, &isValid);
      break;
    }

    case nsISchemaBuiltinType::BUILTIN_TYPE_BASE64BINARY: {
      rv = ValidateBuiltinTypeBase64Binary(aNodeValue, length, isLengthDefined,
                                           minLength, isMinLengthDefined,
                                           maxLength, isMaxLengthDefined,
                                           &enumerationList, &isValid);
      break;
    }

    case nsISchemaBuiltinType::BUILTIN_TYPE_QNAME: {
      rv = ValidateBuiltinTypeQName(aNodeValue, length, isLengthDefined,
                                    minLength, isMinLengthDefined,
                                    maxLength, isMaxLengthDefined,
                                    &enumerationList, &isValid);
      break;
    }

    default:
      rv = NS_ERROR_SCHEMAVALIDATOR_TYPE_NOT_FOUND;
      break;
  }

  // finally check if a pattern is defined, as all types can be constrained by
  // regexp patterns.
  if (isValid && !pattern.IsEmpty()) {
    // check if the pattern matches
    nsCOMPtr<nsISchemaValidatorRegexp> regexp = do_GetService(kREGEXP_CID);
    rv = regexp->RunRegexp(aNodeValue, pattern, "g", &isValid);

#ifdef PR_LOGGING
    LOG(("  Checking Regular Expression"));
    if (isValid) {
      LOG(("    -- pattern validates!"));
    } else {
      LOG(("    -- pattern does not validate!"));
    }
#endif
  }

  *aResult = isValid;
  return rv;
}

/*
  Handles validation of built-in schema types.
*/
nsresult
nsSchemaValidator::ValidateBuiltinType(const nsAString & aNodeValue,
                                       nsISchemaSimpleType *aSchemaSimpleType,
                                       PRBool *aResult)
{
  nsCOMPtr<nsISchemaBuiltinType> builtinType =
    do_QueryInterface(aSchemaSimpleType);
  NS_ENSURE_STATE(builtinType);

  PRUint16 builtinTypeValue;
  nsresult rv = builtinType->GetBuiltinType(&builtinTypeValue);
  NS_ENSURE_SUCCESS(rv, rv);

#ifdef PR_LOGGING
  DumpBaseType(builtinType);
#endif

  PRBool isValid = PR_FALSE;
  switch(builtinTypeValue) {
    case nsISchemaBuiltinType::BUILTIN_TYPE_STRING: {
      isValid = PR_TRUE;
      break;
    }

    case nsISchemaBuiltinType::BUILTIN_TYPE_BOOLEAN: {
      rv = ValidateBuiltinTypeBoolean(aNodeValue, &isValid);
      break;
    }

    case nsISchemaBuiltinType::BUILTIN_TYPE_GDAY: {
      isValid = IsValidSchemaGDay(aNodeValue, nsnull);
      break;
    }

    case nsISchemaBuiltinType::BUILTIN_TYPE_GMONTH: {
      isValid = IsValidSchemaGMonth(aNodeValue, nsnull);
      break;
    }

    case nsISchemaBuiltinType::BUILTIN_TYPE_GYEAR: {
      isValid = IsValidSchemaGYear(aNodeValue, nsnull);
      break;
    }

    case nsISchemaBuiltinType::BUILTIN_TYPE_GYEARMONTH: {
      isValid = IsValidSchemaGYearMonth(aNodeValue, nsnull);
      break;
    }

    case nsISchemaBuiltinType::BUILTIN_TYPE_GMONTHDAY: {
      isValid = IsValidSchemaGMonthDay(aNodeValue, nsnull);
      break;
    }

    case nsISchemaBuiltinType::BUILTIN_TYPE_DATE: {
      isValid = IsValidSchemaDate(aNodeValue, nsnull);
      break;
    }

    case nsISchemaBuiltinType::BUILTIN_TYPE_TIME: {
      isValid = IsValidSchemaTime(aNodeValue, nsnull);
      break;
    }

    case nsISchemaBuiltinType::BUILTIN_TYPE_DATETIME: {
      isValid = IsValidSchemaDateTime(aNodeValue, nsnull);
      break;
    }

    case nsISchemaBuiltinType::BUILTIN_TYPE_DURATION: {
      nsCOMPtr<nsISchemaDuration> temp;
      isValid = IsValidSchemaDuration(aNodeValue, getter_AddRefs(temp));
      break;
    }

    case nsISchemaBuiltinType::BUILTIN_TYPE_INTEGER: {
      isValid = nsSchemaValidatorUtils::IsValidSchemaInteger(aNodeValue, nsnull);
      break;
    }

    /* http://w3.org/TR/xmlschema-2/#nonPositiveInteger */
    case nsISchemaBuiltinType::BUILTIN_TYPE_NONPOSITIVEINTEGER: {
      // nonPositiveInteger inherits from integer, with maxExclusive
      // being 1
      ValidateBuiltinTypeInteger(aNodeValue, nsnull, NS_LITERAL_STRING("1"),
                                 EmptyString(), EmptyString(), EmptyString(),
                                 nsnull, &isValid);
      break;
    }

    /* http://www.w3.org/TR/xmlschema-2/#negativeInteger */
    case nsISchemaBuiltinType::BUILTIN_TYPE_NEGATIVEINTEGER: {
      // negativeInteger inherits from integer, with maxExclusive
      // being 0 (only negative numbers)
      ValidateBuiltinTypeInteger(aNodeValue, nsnull, NS_LITERAL_STRING("0"),
                                 EmptyString(), EmptyString(), EmptyString(),
                                 nsnull, &isValid);
      break;
    }

    case nsISchemaBuiltinType::BUILTIN_TYPE_BYTE: {
      isValid = IsValidSchemaByte(aNodeValue, nsnull);
      break;
    }

    case nsISchemaBuiltinType::BUILTIN_TYPE_FLOAT: {
      isValid = IsValidSchemaFloat(aNodeValue, nsnull);
      break;
    }

    case nsISchemaBuiltinType::BUILTIN_TYPE_DECIMAL: {
      nsAutoString wholePart, fractionPart;
      isValid = IsValidSchemaDecimal(aNodeValue, wholePart, fractionPart);
      break;
    }

    case nsISchemaBuiltinType::BUILTIN_TYPE_ANYURI: {
      isValid = IsValidSchemaAnyURI(aNodeValue);
      break;
    }

    case nsISchemaBuiltinType::BUILTIN_TYPE_BASE64BINARY: {
      char* decodedString;
      isValid = IsValidSchemaBase64Binary(aNodeValue, &decodedString);
      nsMemory::Free(decodedString);
      break;
    }

    case nsISchemaBuiltinType::BUILTIN_TYPE_QNAME: {
      isValid = IsValidSchemaQName(aNodeValue);
      break;
    }

    default:
      rv = NS_ERROR_SCHEMAVALIDATOR_TYPE_NOT_FOUND;
      break;
  }

  *aResult = isValid;
  return rv;
}

/* http://www.w3.org/TR/xmlschema-2/#dt-list */
nsresult
nsSchemaValidator::ValidateListSimpletype(const nsAString & aNodeValue,
                                          nsISchemaSimpleType *aSchemaSimpleType,
                                          PRBool *aResult)
{
  PRBool isValid = PR_FALSE;

  nsresult rv;
  nsCOMPtr<nsISchemaListType> listType = do_QueryInterface(aSchemaSimpleType, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsISchemaSimpleType> listSimpleType;
  rv = listType->GetListType(getter_AddRefs(listSimpleType));
  NS_ENSURE_SUCCESS(rv, rv);

  if (listSimpleType) {
    nsCStringArray stringArray;
    stringArray.ParseString(NS_ConvertUTF16toUTF8(aNodeValue).get(), " \t\r\n");

    PRInt32 count = stringArray.Count();
    nsAutoString tmp;

    for (PRInt32 i=0; i < count; ++i) {
      CopyUTF8toUTF16(stringArray[i]->get(), tmp);
      LOG(("  Validating List Item (%d): %s", i, NS_ConvertUTF16toUTF8(tmp).get()));
      rv = ValidateSimpletype(tmp, listSimpleType, &isValid);

      if (!isValid)
        break;
    }
  }

  *aResult = isValid;
  return rv;
}

/* http://www.w3.org/TR/xmlschema-2/#dt-union */
nsresult
nsSchemaValidator::ValidateUnionSimpletype(const nsAString & aNodeValue,
                                           nsISchemaSimpleType *aSchemaSimpleType,
                                           PRBool *aResult)
{
  PRBool isValid = PR_FALSE;

  nsresult rv;
  nsCOMPtr<nsISchemaUnionType> unionType = do_QueryInterface(aSchemaSimpleType, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsISchemaSimpleType> unionSimpleType;
  PRUint32 count;
  unionType->GetUnionTypeCount(&count);

  // compare against the union simpletypes in order until a match is found
  for (PRUint32 i=0; i < count; ++i) {
    rv = unionType->GetUnionType(i, getter_AddRefs(unionSimpleType));
    NS_ENSURE_SUCCESS(rv, rv);

    LOG(("  Validating Untion Type #%d", i));
    rv = ValidateSimpletype(aNodeValue, unionSimpleType, &isValid);

    if (isValid)
      break;
  }

  *aResult = isValid;
  return rv;
}

/* http://www.w3.org/TR/xmlschema-2/#string */
nsresult
nsSchemaValidator::ValidateBuiltinTypeString(const nsAString & aNodeValue,
                                             PRUint32 aLength,
                                             PRBool aLengthDefined,
                                             PRUint32 aMinLength,
                                             PRBool aMinLengthDefined,
                                             PRUint32 aMaxLength,
                                             PRBool aMaxLengthDefined,
                                             nsStringArray *aEnumerationList,
                                             PRBool *aResult)
{
  // XXX needs to check if all chars are legal per spec
  // IsUTF8(NS_ConvertUTF16toUTF8(theString).get())
  PRBool isValid = PR_TRUE;
  PRUint32 length = aNodeValue.Length();

  if (aLengthDefined && (length != aLength)) {
    isValid = PR_FALSE;
    LOG(("  Not valid: Not the right length (%d)", length));
  }

  if (isValid && aMinLengthDefined && (length < aMinLength)) {
    isValid = PR_FALSE;
    LOG(("  Not valid: Length (%d) is too small", length));
  }

  if (isValid && aMaxLengthDefined && (length > aMaxLength)) {
    isValid = PR_FALSE;
    LOG(("  Not valid: Length (%d) is too large", length));
  }

  if (isValid && aEnumerationList && (aEnumerationList->Count() > 0)) {
    isValid = nsSchemaValidatorUtils::HandleEnumeration(aNodeValue, *aEnumerationList);
  }

#ifdef PR_LOGGING
  if (isValid)
    LOG(("  Value is valid!"));
#endif

 *aResult = isValid;
  return NS_OK;
}

/* http://www.w3.org/TR/xmlschema-2/#boolean */
nsresult 
nsSchemaValidator::ValidateBuiltinTypeBoolean(const nsAString & aNodeValue,
                                              PRBool *aResult)
{
  // possible values are 0, 1, true, false.  Spec makes no mention if TRUE is
  // valid, so performing a case insensitive comparision to be safe
  *aResult = aNodeValue.EqualsLiteral("false") ||
             aNodeValue.EqualsLiteral("true") ||
             aNodeValue.EqualsLiteral("1") ||
             aNodeValue.EqualsLiteral("0");

#ifdef PR_LOGGING
  if (*aResult) {
    LOG(("  Value is valid."));
  } else {
    LOG(("  Not valid: Value (%s) is not in {1,0,true,false}.",
      NS_ConvertUTF16toUTF8(aNodeValue).get()));
  }
#endif

  return NS_OK;
}

/* http://www.w3.org/TR/xmlschema-2/#gDay */
nsresult
nsSchemaValidator::ValidateBuiltinTypeGDay(const nsAString & aNodeValue,
                                           const nsAString & aMaxExclusive,
                                           const nsAString & aMinExclusive,
                                           const nsAString & aMaxInclusive,
                                           const nsAString & aMinInclusive,
                                           PRBool *aResult)
{
  PRBool isValidGDay;
  nsSchemaGDay gday;
  PRBool isValid = IsValidSchemaGDay(aNodeValue, &gday);

  if (isValid && !aMaxExclusive.IsEmpty()) {
    nsSchemaGDay maxExclusive;
    isValidGDay = IsValidSchemaGDay(aMaxExclusive, &maxExclusive);

    if (isValidGDay) {
      if (gday.day >= maxExclusive.day) {
        isValid = PR_FALSE;
        LOG(("  Not valid: Value (%d) is too large", gday.day));
      }
    }
  }

  if (isValid && !aMinExclusive.IsEmpty()) {
    nsSchemaGDay minExclusive;
    isValidGDay = IsValidSchemaGDay(aMinExclusive, &minExclusive);

    if (isValidGDay) {
      if (gday.day <= minExclusive.day) {
        isValid = PR_FALSE;
        LOG(("  Not valid: Value (%d) is too small", gday.day));
      }
    }
  }

  if (isValid && !aMaxInclusive.IsEmpty()) {
    nsSchemaGDay maxInclusive;
    isValidGDay = IsValidSchemaGDay(aMaxInclusive, &maxInclusive);

    if (isValidGDay) {
      if (gday.day > maxInclusive.day) {
        isValid = PR_FALSE;
        LOG(("  Not valid: Value (%d) is too large", gday.day));
      }
    }
  }

  if (isValid && !aMinInclusive.IsEmpty()) {
    nsSchemaGDay minInclusive;
    isValidGDay = IsValidSchemaGDay(aMinInclusive, &minInclusive);

    if (isValidGDay) {
      if (gday.day < minInclusive.day) {
        isValid = PR_FALSE;
        LOG(("  Not valid: Value (%d) is too small", gday.day));
      }
    }
  }

#ifdef PR_LOGGING
    LOG((isValid ? ("  Value is valid!") : (" Value is not valid!")));
#endif

 *aResult = isValid;
  return NS_OK;
}

PRBool
nsSchemaValidator::IsValidSchemaGDay(const nsAString & aNodeValue,
                                     nsSchemaGDay *aResult)
{
  // GDay looks like this: ---DD(Z|(+|-)hh:mm)

  PRUint32 strLength = aNodeValue.Length();
  //   ---DD               ---DDZ              ---DD(+|-)hh:mm
  if (strLength != 5 && strLength != 6 && strLength != 11)
    return PR_FALSE;

  char timezoneHour[3] = "";
  char timezoneMinute[3] = "";
  PRUnichar tzSign = PRUnichar(' ');
  int dayInt;

  // iterate over the string and parse/validate
  nsAString::const_iterator start, end;
  aNodeValue.BeginReading(start);
  aNodeValue.EndReading(end);
  nsAutoString nodeValue(aNodeValue);

  // validate the ---DD part
  PRBool isValid = Substring(start.get(), start.get()+3).EqualsLiteral("---") &&
                   IsValidSchemaGType(Substring(start.get()+3, start.get()+5),
                                      1, 31, &dayInt);
  if (isValid) {
    tzSign = nodeValue.CharAt(5);
    if (strLength == 6) {
      isValid &= (tzSign == 'Z');
    } else if (strLength == 11) {
      const nsAString &tz = Substring(start.get()+6, end.get());

      isValid &= ((tzSign == '+' || tzSign == '-') &&
                  nsSchemaValidatorUtils::ParseSchemaTimeZone(tz, timezoneHour,
                                                              timezoneMinute));
    }
  }

  if (isValid && aResult) {
    char * pEnd;
    aResult->day = dayInt;
    aResult->tz_negative = (tzSign == '-') ? PR_TRUE : PR_FALSE;
    aResult->tz_hour = (timezoneHour[0] == '\0') ? nsnull : strtol(timezoneHour, &pEnd, 10);
    aResult->tz_minute = (timezoneMinute[0] == '\0') ? nsnull : strtol(timezoneMinute, &pEnd, 10);
  }

  return isValid;
}

// Helper function used to validate gregorian date types
PRBool
nsSchemaValidator::IsValidSchemaGType(const nsAString & aNodeValue,
                                      long aMinValue, long aMaxValue,
                                      int *aResult)
{
  long intValue;
  PRBool isValid =
    nsSchemaValidatorUtils::IsValidSchemaInteger(aNodeValue, &intValue);

  *aResult = intValue;
  return isValid && (intValue >= aMinValue) && (intValue <= aMaxValue);
}

/* http://www.w3.org/TR/xmlschema-2/#gMonth */
nsresult
nsSchemaValidator::ValidateBuiltinTypeGMonth(const nsAString & aNodeValue,
                                             const nsAString & aMaxExclusive,
                                             const nsAString & aMinExclusive,
                                             const nsAString & aMaxInclusive,
                                             const nsAString & aMinInclusive,
                                             PRBool *aResult)
{
  nsSchemaGMonth gmonth;
  PRBool isValid = IsValidSchemaGMonth(aNodeValue, &gmonth);

  if (isValid && !aMaxExclusive.IsEmpty()) {
    nsSchemaGMonth maxExclusive;

    if(IsValidSchemaGMonth(aMaxExclusive, &maxExclusive) &&
       gmonth.month >= maxExclusive.month) {
      isValid = PR_FALSE;
      LOG(("  Not valid: Value (%d) is too large", gmonth.month));
    }
  }

  if (isValid && !aMinExclusive.IsEmpty()) {
    nsSchemaGMonth minExclusive;

    if (IsValidSchemaGMonth(aMinExclusive, &minExclusive) &&
        gmonth.month <= minExclusive.month) {
      isValid = PR_FALSE;
      LOG(("  Not valid: Value (%d) is too small", gmonth.month));
    }
  }

  if (isValid && !aMaxInclusive.IsEmpty()) {
    nsSchemaGMonth maxInclusive;

    if (IsValidSchemaGMonth(aMaxInclusive, &maxInclusive) &&
        gmonth.month > maxInclusive.month) {
      isValid = PR_FALSE;
      LOG(("  Not valid: Value (%d) is too large", gmonth.month));
    }
  }

  if (isValid && !aMinInclusive.IsEmpty()) {
    nsSchemaGMonth minInclusive;

    if (IsValidSchemaGMonth(aMinInclusive, &minInclusive) &&
        gmonth.month < minInclusive.month) {
      isValid = PR_FALSE;
      LOG(("  Not valid: Value (%d) is too small", gmonth.month));
    }
  }

#ifdef PR_LOGGING
  LOG((isValid ? ("  Value is valid!") : ("  Value is not valid!")));
#endif

 *aResult = isValid;
  return NS_OK;
}

PRBool
nsSchemaValidator::IsValidSchemaGMonth(const nsAString & aNodeValue,
                                       nsSchemaGMonth *aResult)
{
  // GMonth looks like this: --MM(Z|(+|-)hh:mm)

  PRUint32 strLength = aNodeValue.Length();
  //   --MM              --MMZ             --MM(+|-)hh:mm
  if ((strLength != 4) && (strLength != 5) && (strLength != 10))
    return PR_FALSE;

  char timezoneHour[3] = "";
  char timezoneMinute[3] = "";
  PRUnichar tzSign = 0;
  int monthInt;

  nsAString::const_iterator start, end;
  aNodeValue.BeginReading(start);
  aNodeValue.EndReading(end);
  nsAutoString nodeValue(aNodeValue);

  // validate the --MM-- part
  PRBool isValid = Substring(start.get(), start.get()+2).EqualsLiteral("--") &&
                   IsValidSchemaGType(Substring(start.get()+2, start.get()+4),
                                      1, 12, &monthInt);
  if (isValid) {
    tzSign = nodeValue.CharAt(4);
    if (strLength == 5) {
      isValid &= (tzSign == 'Z');
    } else if (strLength == 10) {
      const nsAString &tz = Substring(start.get()+5, end.get());

      isValid &= ((tzSign == '+' || tzSign == '-') &&
                  nsSchemaValidatorUtils::ParseSchemaTimeZone(tz, timezoneHour,
                                                              timezoneMinute));
    }
  }

  if (isValid && aResult) {
    char * pEnd;
    aResult->month = monthInt;
    aResult->tz_negative = (tzSign == '-') ? PR_TRUE : PR_FALSE;
    aResult->tz_hour = (timezoneHour[0] == '\0') ? nsnull : strtol(timezoneHour, &pEnd, 10);
    aResult->tz_minute = (timezoneMinute[0] == '\0') ? nsnull : strtol(timezoneMinute, &pEnd, 10);
  }

  return isValid;
}

/* http://www.w3.org/TR/xmlschema-2/#gYear */
nsresult
nsSchemaValidator::ValidateBuiltinTypeGYear(const nsAString & aNodeValue,
                                            const nsAString & aMaxExclusive,
                                            const nsAString & aMinExclusive,
                                            const nsAString & aMaxInclusive,
                                            const nsAString & aMinInclusive,
                                            PRBool *aResult)
{
  nsSchemaGYear gyear;
  PRBool isValid = IsValidSchemaGYear(aNodeValue, &gyear);

  if (isValid && !aMaxExclusive.IsEmpty()) {
    nsSchemaGYear maxExclusive;

    if (IsValidSchemaGYear(aMaxExclusive, &maxExclusive) &&
        gyear.year >= maxExclusive.year) {
      isValid = PR_FALSE;
      LOG(("  Not valid: Value (%ld) is too large", gyear.year));
    }
  }

  if (isValid && !aMinExclusive.IsEmpty()) {
    nsSchemaGYear minExclusive;

    if (IsValidSchemaGYear(aMinExclusive, &minExclusive) &&
        gyear.year <= minExclusive.year) {
      isValid = PR_FALSE;
      LOG(("  Not valid: Value (%ld) is too small", gyear.year));
    }
  }

  if (isValid && !aMaxInclusive.IsEmpty()) {
    nsSchemaGYear maxInclusive;

    if (IsValidSchemaGYear(aMaxInclusive, &maxInclusive) &&
        gyear.year > maxInclusive.year) {
      isValid = PR_FALSE;
      LOG(("  Not valid: Value (%ld) is too large", gyear.year));
    }
  }

  if (isValid && !aMinInclusive.IsEmpty()) {
    nsSchemaGYear minInclusive;

    if (IsValidSchemaGYear(aMinInclusive, &minInclusive) &&
        gyear.year < minInclusive.year) {
      isValid = PR_FALSE;
      LOG(("  Not valid: Value (%ld) is too small", gyear.year));
    }
  }

#ifdef PR_LOGGING
  LOG((isValid ? ("  Value is valid!") : ("  Value is not valid!")));
#endif

 *aResult = isValid;
  return NS_OK;
}

PRBool
nsSchemaValidator::IsValidSchemaGYear(const nsAString & aNodeValue,
                                      nsSchemaGYear *aResult)
{
  // GYear looks like this: (-)CCYY(Z|(+|-)hh:mm)
  PRBool isValid = PR_FALSE;
  PRUint32 strLength = aNodeValue.Length();

  char timezoneHour[3] = "";
  char timezoneMinute[3] = "";
  PRUnichar tzSign = 0;

  nsAString::const_iterator start, end, buffStart;
  aNodeValue.BeginReading(start);
  aNodeValue.BeginReading(buffStart);
  aNodeValue.EndReading(end);
  PRUint32 state = 0;
  PRUint32 buffLength = 0, yearLength = 0;
  PRBool done = PR_FALSE;

  long yearNum;

  if (aNodeValue.First() == '-') {
    // jump the negative sign
    *start++;
    buffLength++;
  }

  while ((start != end) && !done) {
    // 0 - year
    // 1 - timezone

    if (state == 0) {
      // year
      PRUnichar temp = *start++;
      // walk till the first non-numerical char
      while (((temp >= '0') && (temp <= '9')) && (buffLength < strLength)) {
        buffLength++;
        temp = *start++;
        yearLength++;
      }

      // if we are not at the end of the string, back up one, as we hit the
      // timezone separator.
      if (buffLength < strLength)
        *start--;

      // need a minimum of 4 digits for year
      if (yearLength >= 4) {
        isValid = nsSchemaValidatorUtils::IsValidSchemaInteger(
                    Substring(buffStart, start), &yearNum);
      }

      // 0 is an invalid year per the spec
      if (isValid && (yearNum == 0))
        isValid = PR_FALSE;

      if (isValid && (strLength > buffLength)) {
        state = 1;
      } else {
        done = PR_TRUE;
      }
    } else if (state == 1) {
      // timezone
      int lengthDiff = (strLength - buffLength);

      if (lengthDiff == 0){
        // timezone is optional
        isValid = PR_TRUE;
      } else if (lengthDiff == 1) {
        // timezone is one character, so has to be 'Z'
        isValid = (*start == 'Z');
      } else if (lengthDiff == 6){
        // timezone is (+|-)hh:mm
        tzSign = *start++;
        if ((tzSign == '+') || (tzSign == '-')) {
          isValid = nsSchemaValidatorUtils::ParseSchemaTimeZone(
                      Substring(start, end), timezoneHour, timezoneMinute);
        }
      } else {
        // invalid length for timezone
        isValid = PR_FALSE;
      }

      done = PR_TRUE;
    }
  }

  if (isValid && aResult) {
    char * pEnd;
    aResult->year = yearNum;
    aResult->tz_negative = (tzSign == '-') ? PR_TRUE : PR_FALSE;
    aResult->tz_hour = (timezoneHour[0] == '\0') ? nsnull : strtol(timezoneHour, &pEnd, 10);
    aResult->tz_minute = (timezoneMinute[0] == '\0') ? nsnull : strtol(timezoneMinute, &pEnd, 10);
  }

  return isValid;
}

/* http://www.w3.org/TR/xmlschema-2/#gYearMonth */
nsresult
nsSchemaValidator::ValidateBuiltinTypeGYearMonth(const nsAString & aNodeValue,
                                                 const nsAString & aMaxExclusive,
                                                 const nsAString & aMinExclusive,
                                                 const nsAString & aMaxInclusive, 
                                                 const nsAString & aMinInclusive,
                                                 PRBool *aResult)
{
  nsSchemaGYearMonth gyearmonth;
  
  PRBool isValid = IsValidSchemaGYearMonth(aNodeValue, &gyearmonth);

  if (isValid && !aMaxExclusive.IsEmpty()) {
    nsSchemaGYearMonth maxExclusive;

    if (IsValidSchemaGYearMonth(aMaxExclusive, &maxExclusive) &&
        (nsSchemaValidatorUtils::CompareGYearMonth(gyearmonth, maxExclusive) > -1)) {
      isValid = PR_FALSE;
      LOG(("  Not valid: Value (%ld-%d) is too large", gyearmonth.gYear.year,
        gyearmonth.gMonth.month));
    }
  }

  if (isValid && !aMinExclusive.IsEmpty()) {
    nsSchemaGYearMonth minExclusive;

    if (IsValidSchemaGYearMonth(aMinExclusive, &minExclusive) &&
        (nsSchemaValidatorUtils::CompareGYearMonth(gyearmonth, minExclusive) < 1)) {
      isValid = PR_FALSE;
      LOG(("  Not valid: Value (%ld-%d) is too small", gyearmonth.gYear.year,
        gyearmonth.gMonth.month));
    }
  }

  if (isValid && !aMaxInclusive.IsEmpty()) {
    nsSchemaGYearMonth maxInclusive;

    if (IsValidSchemaGYearMonth(aMaxInclusive, &maxInclusive) &&
        (nsSchemaValidatorUtils::CompareGYearMonth(gyearmonth, maxInclusive) > 0)) {
      isValid = PR_FALSE;
      LOG(("  Not valid: Value (%ld-%d) is too large", gyearmonth.gYear.year,
        gyearmonth.gMonth.month));
    }
  }

  if (isValid && !aMinInclusive.IsEmpty()) {
    nsSchemaGYearMonth minInclusive;

    if (IsValidSchemaGYearMonth(aMinInclusive, &minInclusive) &&
        (nsSchemaValidatorUtils::CompareGYearMonth(gyearmonth, minInclusive) < 0)) {
      isValid = PR_FALSE;
      LOG(("  Not valid: Value (%ld-%d) is too small", gyearmonth.gYear.year,
        gyearmonth.gMonth.month));
    }
  }

#ifdef PR_LOGGING
  LOG((isValid ? ("  Value is valid!") : ("  Value is not valid!")));
#endif

 *aResult = isValid;
  return NS_OK;
}

PRBool
nsSchemaValidator::IsValidSchemaGYearMonth(const nsAString & aNodeValue,
                                          nsSchemaGYearMonth *aYearMonth)
{
  // GYearMonth looks like this: (-)CCYY-MM(Z|(+|-)hh:mm)
  PRBool isValid = PR_FALSE;

  nsAString::const_iterator start, end, buffStart;
  aNodeValue.BeginReading(start);
  aNodeValue.BeginReading(buffStart);
  aNodeValue.EndReading(end);
  PRUint32 buffLength = 0;
  PRBool done = PR_FALSE;

  PRUint32 strLength = aNodeValue.Length();

  if (aNodeValue.First() == '-') {
    // jump the negative sign
    *start++;
    *buffStart++;
    buffLength++;
  }

  while ((start != end) && !done) {
    if (*start++ == '-') {
      // separator found

      int monthLength = strLength - buffLength;
      //   -MMZ                  -MM(+|-)hh:mm         -MM
      if ((monthLength != 4) && (monthLength != 9) && (monthLength != 3))
        return PR_FALSE;

      // validate year
      nsAutoString year;
      // back one up since we have the separator included
      year = Substring(buffStart, --start);

      isValid = IsValidSchemaGYear(year, aYearMonth ? &aYearMonth->gYear : nsnull);

      if (isValid) {
        nsAutoString month;
        month.AppendLiteral("--");
        start++;
        month.Append(Substring(start, end));

        isValid = IsValidSchemaGMonth(month, aYearMonth ? &aYearMonth->gMonth : nsnull);
      }
      done = PR_TRUE;
    } else {
      buffLength++;
    }
  }

  return isValid;
}

/* http://www.w3.org/TR/xmlschema-2/#gMonthDay */
nsresult
nsSchemaValidator::ValidateBuiltinTypeGMonthDay(const nsAString & aNodeValue,
                                                const nsAString & aMaxExclusive,
                                                const nsAString & aMinExclusive,
                                                const nsAString & aMaxInclusive,
                                                const nsAString & aMinInclusive,
                                                PRBool *aResult)
{
  nsSchemaGMonthDay gmonthday;

  PRBool isValid = IsValidSchemaGMonthDay(aNodeValue, &gmonthday);

  if (isValid && !aMaxExclusive.IsEmpty()) {
    nsSchemaGMonthDay maxExclusive;

    if (IsValidSchemaGMonthDay(aMaxExclusive, &maxExclusive) &&
        (nsSchemaValidatorUtils::CompareGMonthDay(gmonthday, maxExclusive) > -1)) {
      isValid = PR_FALSE;
      LOG(("  Not valid: Value (%d-%d) is too large", gmonthday.gMonth.month,
        gmonthday.gDay.day));
    }
  }

  if (isValid && !aMinExclusive.IsEmpty()) {
    nsSchemaGMonthDay minExclusive;

    if (IsValidSchemaGMonthDay(aMinExclusive, &minExclusive) &&
        (nsSchemaValidatorUtils::CompareGMonthDay(gmonthday, minExclusive) < 1)) {
      isValid = PR_FALSE;
      LOG(("  Not valid: Value (%d-%d) is too small", gmonthday.gMonth.month,
        gmonthday.gDay.day));
    }
  }

  if (isValid && !aMaxInclusive.IsEmpty()) {
    nsSchemaGMonthDay maxInclusive;

    if (IsValidSchemaGMonthDay(aMaxInclusive, &maxInclusive) &&
        (nsSchemaValidatorUtils::CompareGMonthDay(gmonthday, maxInclusive) > 0)) {
      isValid = PR_FALSE;
      LOG(("  Not valid: Value (%d-%d) is too large", gmonthday.gMonth.month,
        gmonthday.gDay.day));
    }
  }

  if (isValid && !aMinInclusive.IsEmpty()) {
    nsSchemaGMonthDay minInclusive;

    if (IsValidSchemaGMonthDay(aMinInclusive, &minInclusive) &&
        (nsSchemaValidatorUtils::CompareGMonthDay(gmonthday, minInclusive) < 0)) {
      isValid = PR_FALSE;
      LOG(("  Not valid: Value (%d-%d) is too small", gmonthday.gMonth.month,
        gmonthday.gDay.day));
    }
  }

#ifdef PR_LOGGING
  LOG((isValid ? ("  Value is valid!") : ("  Value is not valid!")));
#endif

 *aResult = isValid;
  return NS_OK;
}

PRBool
nsSchemaValidator::IsValidSchemaGMonthDay(const nsAString & aNodeValue,
                                          nsSchemaGMonthDay *aMonthDay)
{
  // GMonthDay looks like this: --MM-DD(Z|(+|-)hh:mm)
  PRBool isValid = PR_FALSE;

  nsAString::const_iterator start, end, buffStart;
  aNodeValue.BeginReading(start);
  aNodeValue.BeginReading(buffStart);
  aNodeValue.EndReading(end);
  PRUint32 buffLength = 0;
  PRUint32 state = 0;
  PRBool done = PR_FALSE;

  PRUint32 strLength = aNodeValue.Length();
  //   --MM-DD             --MM-DDZ            --MM-DD(+|-)hh:mm
  if ((strLength != 7) && (strLength != 8) && (strLength != 13))
    return PR_FALSE;

  while ((start != end) && !done) {
    if (state == 0) {
      // separator
      if (*start++ == '-') {
        buffLength++;

        if (buffLength == 2) {
          state = 1;
          buffStart = start;
        }
      } else {
        done = PR_TRUE;
      }
    } else if (state == 1) {
      // month part -  construct an --MM--

      nsAutoString month;
      month.AppendLiteral("--");
      month.Append(Substring(buffStart, start.advance(2)));
      isValid = IsValidSchemaGMonth(month, aMonthDay ? &aMonthDay->gMonth : nsnull);

      if (isValid) {
        buffStart = start;
        state = 2;
      } else {
        done = PR_TRUE;
      }
    } else if (state == 2) {
      // day part - construct ---DD(timezone)
      nsAutoString day;
      day.AppendLiteral("---");
      day.Append(Substring(++buffStart, end));
      isValid = IsValidSchemaGDay(day, aMonthDay ? &aMonthDay->gDay : nsnull);
      done = PR_TRUE;
    }
  }

  return isValid;
}

/* http://www.w3.org/TR/xmlschema-2/#time */
nsresult
nsSchemaValidator::ValidateBuiltinTypeTime(const nsAString & aNodeValue,
                                           const nsAString & aMaxExclusive,
                                           const nsAString & aMinExclusive,
                                           const nsAString & aMaxInclusive,
                                           const nsAString & aMinInclusive,
                                           PRBool *aResult)
{
  PRBool isValid = PR_FALSE;
  PRTime dateTime;
  PRExplodedTime explodedTime;
  int timeCompare;

  isValid = IsValidSchemaTime(aNodeValue, &dateTime);

  if (isValid) {  
    // convert it to a PRExplodedTime 
    PR_ExplodeTime(dateTime, PR_GMTParameters, &explodedTime);
  }

  if (isValid && !aMinExclusive.IsEmpty()) {
    PRTime minExclusive;
    PRExplodedTime minExclusiveExploded;

    if (IsValidSchemaTime(aMinExclusive, &minExclusive)) {
      PR_ExplodeTime(minExclusive, PR_GMTParameters, &minExclusiveExploded);

      timeCompare =
        nsSchemaValidatorUtils::CompareExplodedTime(explodedTime, minExclusiveExploded);

      if (timeCompare < 1) {
        isValid = PR_FALSE;
        LOG(("  Not valid: Value is too small"));
      }
    }
  }

  if (isValid && !aMaxExclusive.IsEmpty()) {
    PRTime maxExclusive;
    PRExplodedTime maxExclusiveExploded;

    if (IsValidSchemaTime(aMaxExclusive, &maxExclusive)) {
      PR_ExplodeTime(maxExclusive, PR_GMTParameters, &maxExclusiveExploded);

      timeCompare =
        nsSchemaValidatorUtils::CompareExplodedTime(explodedTime, maxExclusiveExploded);

      if (timeCompare > -1) {
        isValid = PR_FALSE;
        LOG(("  Not valid: Value is too large"));
      }
    }
  }

  if (isValid && !aMaxInclusive.IsEmpty()) {
    PRTime maxInclusive;
    PRExplodedTime maxInclusiveExploded;

    if (IsValidSchemaTime(aMaxInclusive, &maxInclusive)) {
      PR_ExplodeTime(maxInclusive, PR_GMTParameters, &maxInclusiveExploded);

      timeCompare =
        nsSchemaValidatorUtils::CompareExplodedTime(explodedTime, maxInclusiveExploded);

      if (timeCompare > 0) {
        isValid = PR_FALSE;
        LOG(("  Not valid: Value is too large"));
      }
    }
  }

  if (isValid && !aMinInclusive.IsEmpty()) {
    PRTime minInclusive;
    PRExplodedTime minInclusiveExploded;

    if (IsValidSchemaTime(aMinInclusive, &minInclusive)) {
      PR_ExplodeTime(minInclusive, PR_GMTParameters, &minInclusiveExploded);

      timeCompare =
        nsSchemaValidatorUtils::CompareExplodedTime(explodedTime, minInclusiveExploded);

      if (timeCompare < 0) {
        isValid = PR_FALSE;
        LOG(("  Not valid: Value is too small"));
      }
    }
  }

 *aResult = isValid;
  return NS_OK;
}

NS_IMETHODIMP
nsSchemaValidator::ValidateBuiltinTypeTime(const nsAString & aValue,
                                           PRTime *aResult)
{
  nsresult rv = NS_OK;
  PRTime time;

  if (IsValidSchemaTime(aValue, &time)) {
    *aResult = time;
  } else {
    *aResult = nsnull;
    rv = NS_ERROR_ILLEGAL_VALUE;
  }

  return rv;
}

PRBool
nsSchemaValidator::IsValidSchemaTime(const nsAString & aNodeValue,
                                     PRTime *aResult)
{
  PRBool isValid = PR_FALSE;

  char hour[3] = "";
  char minute[3] = "";
  char second[3] = "";
  char fraction_seconds[80] = "";
  PRTime dateTime;

  char fulldate[100] = "";

  nsAutoString timeString(aNodeValue);

  // if no timezone ([+/-]hh:ss) or no timezone Z, add a Z to the end so that
  // nsSchemaValidatorUtils::ParseSchemaTime can parse it
  if ((timeString.FindChar('-') == kNotFound) &&
      (timeString.FindChar('+') == kNotFound) &&
       timeString.Last() != 'Z'){
    timeString.Append('Z');
  }

  LOG(("  Validating Time: "));

  isValid = nsSchemaValidatorUtils::ParseSchemaTime(timeString, hour, minute,
                                                    second, fraction_seconds);

  if (isValid && aResult) {
    // generate a string nspr can handle
    // for example: 22-AUG-1993 10:59:12.82
    sprintf(fulldate, "22-AUG-1993 %s:%s:%s", hour, minute, second);

    if (strlen(fraction_seconds) > 0) {
      strcat(fulldate, ".");
      strcat(fulldate, fraction_seconds);
    }

    LOG(("    new date is %s", fulldate));

    PRStatus status = PR_ParseTimeString(fulldate, PR_TRUE, &dateTime);
    if (status == -1)
      isValid = PR_FALSE;
    else
      *aResult = dateTime;
  }

  return isValid;
}

/* http://www.w3.org/TR/xmlschema-2/#date */
nsresult
nsSchemaValidator::ValidateBuiltinTypeDate(const nsAString & aNodeValue,
                                           const nsAString & aMaxExclusive,
                                           const nsAString & aMinExclusive,
                                           const nsAString & aMaxInclusive,
                                           const nsAString & aMinInclusive,
                                           PRBool *aResult)
{
  PRBool isValid = PR_FALSE;
  PRTime dateTime;
  PRExplodedTime explodedDate;
  int dateCompare;

  isValid = IsValidSchemaDate(aNodeValue, &dateTime);

  if (isValid) {  
    // convert it to a PRExplodedTime 
    PR_ExplodeTime(dateTime, PR_GMTParameters, &explodedDate);
  }

  if (isValid && !aMaxExclusive.IsEmpty()) {
    PRTime maxExclusive;
    PRExplodedTime maxExclusiveExploded;

    if (IsValidSchemaDate(aMaxExclusive, &maxExclusive)) {
      PR_ExplodeTime(maxExclusive, PR_GMTParameters, &maxExclusiveExploded);

      dateCompare =
        nsSchemaValidatorUtils::CompareExplodedDate(explodedDate,
                                                    maxExclusiveExploded);

      if (dateCompare > -1) {
        isValid = PR_FALSE;
        LOG(("  Not valid: Value is too large"));
      }
    }
  }

  if (isValid && !aMinExclusive.IsEmpty()) {
    PRTime minExclusive;
    PRExplodedTime minExclusiveExploded;

    if (IsValidSchemaDate(aMinExclusive, &minExclusive)) {
      PR_ExplodeTime(minExclusive, PR_GMTParameters, &minExclusiveExploded);

      dateCompare =
        nsSchemaValidatorUtils ::CompareExplodedDate(explodedDate,
                                                     minExclusiveExploded);

      if (dateCompare < 1) {
        isValid = PR_FALSE;
        LOG(("  Not valid: Value is too small"));
      }
    }
  }

  if (isValid && !aMaxInclusive.IsEmpty()) {
    PRTime maxInclusive;
    PRExplodedTime maxInclusiveExploded;

    if (IsValidSchemaDate(aMaxInclusive, &maxInclusive)) {
      PR_ExplodeTime(maxInclusive, PR_GMTParameters, &maxInclusiveExploded);

      dateCompare =
        nsSchemaValidatorUtils::CompareExplodedDate(explodedDate,
                                                    maxInclusiveExploded);

      if (dateCompare > 0) {
        isValid = PR_FALSE;
        LOG(("  Not valid: Value is too large"));
      }
    }
  }

  if (isValid && !aMinInclusive.IsEmpty()) {
    PRTime minInclusive;
    PRExplodedTime minInclusiveExploded;

    if (IsValidSchemaDate(aMinInclusive, &minInclusive)) {
      PR_ExplodeTime(minInclusive, PR_GMTParameters, &minInclusiveExploded);

      dateCompare =
        nsSchemaValidatorUtils::CompareExplodedDate(explodedDate,
                                                    minInclusiveExploded);

      if (dateCompare < 0) {
        isValid = PR_FALSE;
        LOG(("  Not valid: Value is too small"));
      }
    }
  }

 *aResult = isValid;
  return NS_OK;
}

NS_IMETHODIMP
nsSchemaValidator::ValidateBuiltinTypeDate(const nsAString & aValue,
                                           PRTime *aResult)
{
  nsresult rv = NS_OK;
  PRTime time;

  if (IsValidSchemaDate(aValue, &time)) {
    *aResult = time;
  } else {
    *aResult = nsnull;
    rv = NS_ERROR_ILLEGAL_VALUE;
  }

  return rv;
}


/* http://www.w3.org/TR/xmlschema-2/#dateTime */
nsresult
nsSchemaValidator::ValidateBuiltinTypeDateTime(const nsAString & aNodeValue,
                                           const nsAString & aMaxExclusive,
                                           const nsAString & aMinExclusive,
                                           const nsAString & aMaxInclusive,
                                           const nsAString & aMinInclusive,
                                           PRBool *aResult)
{
  PRBool isValid = PR_FALSE;
  PRTime dateTime;
  PRExplodedTime explodedDateTime;
  PRBool isDateTimeNegative = PR_FALSE;

  isValid = IsValidSchemaDateTime(aNodeValue, &dateTime);

  if (isValid) {  
    // convert it to a PRExplodedTime 
    PR_ExplodeTime(dateTime, PR_GMTParameters, &explodedDateTime);

    if (aNodeValue.First() == PRUnichar('-'))
      isDateTimeNegative = PR_TRUE;
  }

  if (isValid && !aMaxExclusive.IsEmpty()) {
    PRTime maxExclusive;

    if (IsValidSchemaDateTime(aMaxExclusive, &maxExclusive) &&
        (CompareSchemaDateTime(explodedDateTime, isDateTimeNegative, maxExclusive,
                               (aMaxExclusive.First() == PRUnichar('-')) ? PR_TRUE : PR_FALSE) > -1)) {
      isValid = PR_FALSE;
      LOG(("  Not valid: Value is too large"));
    }
  }

  if (isValid && !aMinExclusive.IsEmpty()) {
    PRTime minExclusive;

    if (IsValidSchemaDateTime(aMinExclusive, &minExclusive) &&
        (CompareSchemaDateTime(explodedDateTime, isDateTimeNegative, minExclusive,
                               (aMinExclusive.First() == PRUnichar('-')) ? PR_TRUE : PR_FALSE) < 1)) {
      isValid = PR_FALSE;
      LOG(("  Not valid: Value is too small"));
    }
  }

  if (isValid && !aMaxInclusive.IsEmpty()) {
    PRTime maxInclusive;

    if (IsValidSchemaDateTime(aMaxInclusive, &maxInclusive) &&
        (CompareSchemaDateTime(explodedDateTime, isDateTimeNegative, maxInclusive,
                               (aMaxInclusive.First() == PRUnichar('-')) ? PR_TRUE : PR_FALSE) > 0)) {
      isValid = PR_FALSE;
      LOG(("  Not valid: Value is too large"));
    }
  }

  if (isValid && !aMinInclusive.IsEmpty()) {
    PRTime minInclusive;

    if (IsValidSchemaDateTime(aMinInclusive, &minInclusive) &&
        (CompareSchemaDateTime(explodedDateTime, isDateTimeNegative, minInclusive,
                               (aMinInclusive.First() == PRUnichar('-')) ? PR_TRUE : PR_FALSE) < 0)) {
      isValid = PR_FALSE;
      LOG(("  Not valid: Value is too small"));
    }
  }

 *aResult = isValid;
  return NS_OK;
}

NS_IMETHODIMP
nsSchemaValidator::ValidateBuiltinTypeDateTime(const nsAString & aValue,
                                               PRTime *aResult)
{
  nsresult rv = NS_OK;
  PRTime time;

  if (IsValidSchemaDateTime(aValue, &time)) {
    *aResult = time;
  } else {
    *aResult = nsnull;
    rv = NS_ERROR_ILLEGAL_VALUE;
  }

  return rv;
}

int
nsSchemaValidator::CompareSchemaDateTime(PRExplodedTime datetime1,
                                         PRBool isDateTime1Negative,
                                         PRTime datetime2,
                                         PRBool isDateTime2Negative)
{
  PRExplodedTime datetime2Exploded;
  PR_ExplodeTime(datetime2, PR_GMTParameters, &datetime2Exploded);

  return nsSchemaValidatorUtils::CompareExplodedDateTime(datetime1,
                                                         isDateTime1Negative,
                                                         datetime2Exploded,
                                                         isDateTime2Negative);
}

PRBool
nsSchemaValidator::IsValidSchemaDate(const nsAString & aNodeValue,
                                     PRTime *aResult)
{
  PRBool isValid = PR_FALSE;
  PRBool isNegativeYear = PR_FALSE;

  int run = 0;

  char year[80] = "";
  char month[3] = "";
  char day[3] = "";
  PRTime dateTime;

  char fulldate[100] = "";

  LOG(("  Validating Date:"));

  nsAutoString dateString(aNodeValue); 
  if (dateString.First() == '-') {
    /* nspr can't handle negative years it seems */
    isNegativeYear = PR_TRUE;
    run = 1;
 }

 /*
    http://www.w3.org/TR/xmlschema-2/#date
    (-)CCYY-MM-DDT
      then either: Z
      or [+/-]hh:mm
  */

  // append 'T' to end to make ParseSchemaDate happy.
  dateString.Append('T');

  isValid = nsSchemaValidatorUtils::ParseSchemaDate(dateString, year, month, day);

  if (isValid && aResult) {
    // create something like 22-AUG-1993 for nspr to parse
    nsCAutoString monthShorthand;
    nsSchemaValidatorUtils::GetMonthShorthand(month, monthShorthand);

    sprintf(fulldate, "%s-%s-%s", day, monthShorthand.get(), year);

    PRStatus status = PR_ParseTimeString(fulldate, PR_TRUE, &dateTime);  
    if (status == -1)
      isValid = PR_FALSE;
    else
      *aResult = dateTime;
  }

  return isValid;
}

PRBool
nsSchemaValidator::IsValidSchemaDateTime(const nsAString & aNodeValue,
                                         PRTime *aResult)
{
  return nsSchemaValidatorUtils::GetPRTimeFromDateTime(aNodeValue, aResult);
}

/* http://w3.org/TR/xmlschema-2/#duration */
nsresult
nsSchemaValidator::ValidateBuiltinTypeDuration(const nsAString & aNodeValue,
                                           const nsAString & aMaxExclusive,
                                           const nsAString & aMinExclusive,
                                           const nsAString & aMaxInclusive,
                                           const nsAString & aMinInclusive,
                                           PRBool *aResult)
{
  PRBool isValid = PR_FALSE;

  nsCOMPtr<nsISchemaDuration> duration;
  isValid = IsValidSchemaDuration(aNodeValue, getter_AddRefs(duration));

  if (isValid && !aMaxExclusive.IsEmpty()) {
    nsCOMPtr<nsISchemaDuration> maxExclusiveDuration;

    if (IsValidSchemaDuration(aMaxExclusive, getter_AddRefs(maxExclusiveDuration)) &&
        nsSchemaValidatorUtils::CompareDurations(duration, maxExclusiveDuration) == 1){
      isValid = PR_FALSE;
      LOG(("  Not valid: Value is too large or indeterminate"));
    }
  }

  if (isValid && !aMinExclusive.IsEmpty()) {
    nsCOMPtr<nsISchemaDuration> minExclusiveDuration;

    if (IsValidSchemaDuration(aMinExclusive, getter_AddRefs(minExclusiveDuration)) &&
        nsSchemaValidatorUtils::CompareDurations(minExclusiveDuration, duration) == 1){
      isValid = PR_FALSE;
      LOG(("  Not valid: Value is too small or indeterminate"));
    }
  }

  if (isValid && !aMaxInclusive.IsEmpty()) {
    nsCOMPtr<nsISchemaDuration> maxInclusiveDuration;

    if (IsValidSchemaDuration(aMaxInclusive, getter_AddRefs(maxInclusiveDuration)) && 
        nsSchemaValidatorUtils::CompareDurations(duration, maxInclusiveDuration) == 1){
      isValid = PR_FALSE;
      LOG(("  Not valid: Value is too large or indeterminate"));
    }
  }

  if (isValid && !aMinInclusive.IsEmpty()) {
    nsCOMPtr<nsISchemaDuration> minInclusiveDuration;

    if (IsValidSchemaDuration(aMinInclusive, getter_AddRefs(minInclusiveDuration)) &&
        nsSchemaValidatorUtils::CompareDurations(minInclusiveDuration, duration) == 1){
      isValid = PR_FALSE;
      LOG(("  Not valid: Value is too small or indeterminate"));
    }
  }

 *aResult = isValid;
  return NS_OK;
}

// scriptable method for validating and parsing durations
NS_IMETHODIMP
nsSchemaValidator::ValidateBuiltinTypeDuration(const nsAString & aValue,
                                               nsISchemaDuration **aDuration)
{
  nsresult rv = NS_OK;
  *aDuration = nsnull;
  nsCOMPtr<nsISchemaDuration> duration;

  if (IsValidSchemaDuration(aValue, getter_AddRefs(duration))) {
    duration.swap(*aDuration);
  } else {
    rv = NS_ERROR_ILLEGAL_VALUE;
  }

  return rv;
}

PRBool
nsSchemaValidator::IsValidSchemaDuration(const nsAString & aNodeValue,
                                         nsISchemaDuration **aResult)
{
  PRBool isValid = PR_FALSE;
  nsCOMPtr<nsISchemaDuration> duration;

  isValid = nsSchemaValidatorUtils::ParseSchemaDuration(aNodeValue, getter_AddRefs(duration));

  duration.swap(*aResult);
  return isValid;
}

/* http://w3.org/TR/xmlschema-2/#integer */
nsresult
nsSchemaValidator::ValidateBuiltinTypeInteger(const nsAString & aNodeValue,
                                              PRUint32 aTotalDigits,
                                              const nsAString & aMaxExclusive,
                                              const nsAString & aMinExclusive,
                                              const nsAString & aMaxInclusive,
                                              const nsAString & aMinInclusive,
                                              nsStringArray *aEnumerationList,
                                              PRBool *aResult)
{
  PRBool isValid = PR_FALSE;
  long intValue;
  isValid = nsSchemaValidatorUtils::IsValidSchemaInteger(aNodeValue, &intValue);

  if (isValid && aTotalDigits) {
    PRUint32 length = aNodeValue.Length();

    if (aNodeValue.First() == PRUnichar('-'))
      length -= 1;

    if (length > aTotalDigits) {
      isValid = PR_FALSE;
      LOG(("  Not valid: Too many digits (%d)", length));
    }
  }

  if (isValid && !aMaxExclusive.IsEmpty()){
    long maxExclusive;

    if (nsSchemaValidatorUtils::IsValidSchemaInteger(aMaxExclusive, &maxExclusive)
        && (nsSchemaValidatorUtils::CompareStrings(aNodeValue, aMaxExclusive) >= 0)) {
      isValid = PR_FALSE;
    }
  }

  if (isValid && !aMinExclusive.IsEmpty()){
    long minExclusive;

    if (nsSchemaValidatorUtils::IsValidSchemaInteger(aMinExclusive, &minExclusive)
        && (nsSchemaValidatorUtils::CompareStrings(aNodeValue, aMinExclusive) <= 0)) {
      isValid = PR_FALSE;
    }
  }

  if (isValid && !aMaxInclusive.IsEmpty()){
    long maxInclusive;

    if (nsSchemaValidatorUtils::IsValidSchemaInteger(aMaxInclusive, &maxInclusive)
        && (nsSchemaValidatorUtils::CompareStrings(aNodeValue, aMaxInclusive) > 0)) {
      isValid = PR_FALSE;
    }
  }

  if (isValid && !aMinInclusive.IsEmpty()){
    long minInclusive;

    if (nsSchemaValidatorUtils::IsValidSchemaInteger(aMinInclusive, &minInclusive)
        && (nsSchemaValidatorUtils::CompareStrings(aNodeValue, aMinInclusive) < 0)) {
      isValid = PR_FALSE;
    }
  }

  if (isValid && aEnumerationList && (aEnumerationList->Count() > 0)) {
    isValid = nsSchemaValidatorUtils::HandleEnumeration(aNodeValue, *aEnumerationList);
  }

 *aResult = isValid;
  return NS_OK;
}

nsresult
nsSchemaValidator::ValidateBuiltinTypeByte(const nsAString & aNodeValue,
                                           PRUint32 aTotalDigits,
                                           const nsAString & aMaxExclusive,
                                           const nsAString & aMinExclusive,
                                           const nsAString & aMaxInclusive,
                                           const nsAString & aMinInclusive,
                                           nsStringArray *aEnumerationList,
                                           PRBool *aResult)
{
  PRBool isValid = PR_FALSE;

  long intValue;
  isValid = IsValidSchemaByte(aNodeValue, &intValue);

  if (aTotalDigits) {
    PRUint32 length = aNodeValue.Length();

    if (aNodeValue.First() == PRUnichar('-'))
      length -= 1;

    if (length > aTotalDigits) {
      isValid = PR_FALSE;
      LOG(("  Not valid: Too many digits (%d)", length));
    }
  }

  if (isValid && !aMaxExclusive.IsEmpty()){
    long maxExclusive;

    if (IsValidSchemaByte(aMaxExclusive, &maxExclusive) &&
        (intValue >= maxExclusive)) {
      isValid = PR_FALSE;
      LOG(("  Not valid: Value is too large"));
    }
  }

  if (isValid && !aMinExclusive.IsEmpty()){
    long minExclusive;

    if (IsValidSchemaByte(aMinExclusive, &minExclusive) &&
        (intValue <= minExclusive)) {
      isValid = PR_FALSE;
      LOG(("  Not valid: Value is too small"));
    }
  }

  if (isValid && !aMaxInclusive.IsEmpty()){
    long maxInclusive;

    if (IsValidSchemaByte(aMaxInclusive, &maxInclusive) &&
        (intValue > maxInclusive)) {
      isValid = PR_FALSE;
      LOG(("  Not valid: Value is too large"));
    }
  }

  if (isValid && !aMinInclusive.IsEmpty()){
    long minInclusive;

    if (IsValidSchemaByte(aMinInclusive, &minInclusive) &&
        (intValue < minInclusive)) {
      isValid = PR_FALSE;
      LOG(("  Not valid: Value is too small"));
    }
  }

  if (isValid && aEnumerationList && (aEnumerationList->Count() > 0)) {
    isValid = nsSchemaValidatorUtils::HandleEnumeration(aNodeValue, *aEnumerationList);
  }

 *aResult = isValid;
  return NS_OK;
}

/* http://w3.org/TR/xmlschema-2/#byte */
PRBool
nsSchemaValidator::IsValidSchemaByte(const nsAString & aNodeValue, long *aResult)
{
  PRBool isValid = PR_FALSE;
  long byteValue;

  isValid = nsSchemaValidatorUtils::IsValidSchemaInteger(aNodeValue, &byteValue);

  if (isValid && ((byteValue > 127) || (byteValue < -128)))
    isValid = PR_FALSE;

  if (aResult)
    *aResult = byteValue;
  return isValid;
}

/* http://www.w3.org/TR/xmlschema-2/#float */
nsresult
nsSchemaValidator::ValidateBuiltinTypeFloat(const nsAString & aNodeValue,
                                            PRUint32 aTotalDigits,
                                            const nsAString & aMaxExclusive,
                                            const nsAString & aMinExclusive,
                                            const nsAString & aMaxInclusive,
                                            const nsAString & aMinInclusive,
                                            nsStringArray *aEnumerationList,
                                            PRBool *aResult)
{
  PRBool isValid = PR_FALSE;

  float floatValue;
  isValid = IsValidSchemaFloat(aNodeValue, &floatValue);

  if (isValid && !aMaxExclusive.IsEmpty()){
    float maxExclusive;

    if (IsValidSchemaFloat(aMaxExclusive, &maxExclusive) &&
        (floatValue >= maxExclusive)) {
      isValid = PR_FALSE;
      LOG(("  Not valid: Value (%f) is too big", floatValue));
    }
  }

  if (isValid && !aMinExclusive.IsEmpty()){
    float minExclusive;

    if (IsValidSchemaFloat(aMinExclusive, &minExclusive) &&
        (floatValue <= minExclusive)) {
      isValid = PR_FALSE;
      LOG(("  Not valid: Value (%f) is too small", floatValue));
    }
  }

  if (isValid && !aMaxInclusive.IsEmpty()){
    float maxInclusive;

    if (IsValidSchemaFloat(aMaxInclusive, &maxInclusive) &&
        (floatValue > maxInclusive)) {
      isValid = PR_FALSE;
      LOG(("  Not valid: Value (%f) is too big", floatValue));
    }
  }

  if (isValid && !aMinInclusive.IsEmpty()){
    float minInclusive;

    if (IsValidSchemaFloat(aMinInclusive, &minInclusive) &&
        (floatValue < minInclusive)) {
      isValid = PR_FALSE;
      LOG(("  Not valid: Value (%f) is too small", floatValue));
    }
  }

  if (isValid && aEnumerationList && (aEnumerationList->Count() > 0)) {
    isValid = nsSchemaValidatorUtils::HandleEnumeration(aNodeValue, *aEnumerationList);
  }

#ifdef PR_LOGGING
  LOG((isValid ? ("  Value is valid!") : ("  Value is not valid!")));
#endif

 *aResult = isValid;
  return NS_OK;
}

PRBool
nsSchemaValidator::IsValidSchemaFloat(const nsAString & aNodeValue,
                                      float *aResult)
{
  PRBool isValid = PR_TRUE;
  nsAutoString temp(aNodeValue);

  PRInt32 errorCode;
  float floatValue = temp.ToFloat(&errorCode);
  if (NS_FAILED(errorCode))
    isValid = PR_FALSE;

  if (aResult)
    *aResult = floatValue;

  return isValid;
}

/* http://www.w3.org/TR/xmlschema-2/#decimal */
nsresult
nsSchemaValidator::ValidateBuiltinTypeDecimal(const nsAString & aNodeValue,
                                              PRUint32 aTotalDigits,
                                              PRUint32 aTotalFractionDigits,
                                              PRBool aFractionDigitsSet,
                                              const nsAString & aMaxExclusive,
                                              const nsAString & aMinExclusive,
                                              const nsAString & aMaxInclusive,
                                              const nsAString & aMinInclusive,
                                              nsStringArray *aEnumerationList,
                                              PRBool *aResult)
{
  PRBool isValid = PR_FALSE;
  int strcmp;

  nsAutoString wholePart;
  nsAutoString wholePartFacet;
  nsAutoString fractionPart;
  nsAutoString fractionPartFacet;

  isValid = IsValidSchemaDecimal(aNodeValue, wholePart, fractionPart);

  // totalDigits is supposed to be positiveInteger, 1..n
  if (isValid && (aTotalDigits > 0)) {
    if (wholePart.Length() > aTotalDigits) {
     isValid = PR_FALSE;
      LOG(("  Not valid: Expected a maximum of %d digits, but found %d.",
           aTotalDigits, wholePart.Length()));
    }
  }

  // fractionDigits is nonNegativeInteger, 0..n
  if (isValid && aFractionDigitsSet) {
    if (fractionPart.Length() > aTotalFractionDigits) {
     isValid = PR_FALSE;
     LOG(("  Not valid: Expected a maximum of %d fraction digits, but found %d.",
          aTotalFractionDigits, fractionPart.Length()));
    }
  }

  if (isValid && !aMaxExclusive.IsEmpty() &&
      IsValidSchemaDecimal(aMaxExclusive, wholePartFacet, fractionPartFacet)) {
    strcmp = nsSchemaValidatorUtils::CompareStrings(wholePart, wholePartFacet);
    if (strcmp > 0) {
      isValid = PR_FALSE;
    } else if (strcmp == 0) {
      // if equal check the fraction part
      strcmp = CompareFractionStrings(fractionPart, fractionPartFacet);
      if (strcmp >= 0)
        isValid = PR_FALSE;
    }

    if (!isValid)
      LOG(("  Not valid: Value is too big"));
  }

  if (isValid && !aMinExclusive.IsEmpty() &&
      IsValidSchemaDecimal(aMinExclusive, wholePartFacet, fractionPartFacet)) {
    strcmp = nsSchemaValidatorUtils::CompareStrings(wholePart, wholePartFacet);
    if (strcmp < 0) {
      isValid = PR_FALSE;
    } else if (strcmp == 0) {
      // if equal check the fraction part
      strcmp = CompareFractionStrings(fractionPart, fractionPartFacet);
      if (strcmp <= 0)
        isValid = PR_FALSE;
    }

    if (!isValid)
      LOG(("  Not valid: Value is too small"));
  }

  if (isValid && !aMaxInclusive.IsEmpty() &&
      IsValidSchemaDecimal(aMaxInclusive, wholePartFacet, fractionPartFacet)) {
    strcmp = nsSchemaValidatorUtils::CompareStrings(wholePart, wholePartFacet);
    if (strcmp > 0) {
      isValid = PR_FALSE;
    } else if (strcmp == 0) {
      // if equal check the fraction part
      strcmp = CompareFractionStrings(fractionPart, fractionPartFacet);
      if (strcmp > 0)
        isValid = PR_FALSE;
    }

    if (!isValid)
      LOG(("  Not valid: Value is too big"));
  }

  if (isValid && !aMinInclusive.IsEmpty() &&
      IsValidSchemaDecimal(aMinInclusive, wholePartFacet, fractionPartFacet)) {
    strcmp = nsSchemaValidatorUtils::CompareStrings(wholePart, wholePartFacet);
    if (strcmp < 0) {
      isValid = PR_FALSE;
    } else if (strcmp == 0) {
      // if equal check the fraction part
      strcmp = CompareFractionStrings(fractionPart, fractionPartFacet);
      if (strcmp < 0)
        isValid = PR_FALSE;
    }

    if (!isValid)
      LOG(("  Not valid: Value is too small"));
  }

  if (isValid && aEnumerationList && (aEnumerationList->Count() > 0)) {
    isValid = nsSchemaValidatorUtils::HandleEnumeration(aNodeValue, *aEnumerationList);
  }

#ifdef PR_LOGGING
  LOG((isValid ? ("  Value is valid!") : ("  Value is not valid!")));
#endif

 *aResult = isValid;
  return NS_OK;
}

PRBool
nsSchemaValidator::IsValidSchemaDecimal(const nsAString & aNodeValue,
  nsAString & aWholePart, nsAString & aFractionPart)
{
  PRBool isValid = PR_FALSE;

  int findString = aNodeValue.FindChar('.');

  if (findString == kNotFound) {
    aWholePart.Assign(aNodeValue);
  } else {
    aWholePart = Substring(aNodeValue, 0, findString);
    aFractionPart = Substring(aNodeValue, findString+1,
                              aNodeValue.Length() - findString - 1);
  }

  // to make test easier for example 
  //   nsAutoString wh1, wh2, fr1, fr2;
  //   IsValidSchemaDecimal(NS_LITERAL_STRING("123.456"), wh1,fr1);
  //   IsValidSchemaDecimal(NS_LITERAL_STRING("000123.4560000"), wh2,fr2);
  //   if(wh1.Equals(wh2) && fr1.Equals(fr2)){ /* number are equal */ }
  nsSchemaValidatorUtils::RemoveLeadingZeros(aWholePart);
  nsSchemaValidatorUtils::RemoveTrailingZeros(aFractionPart);

  long temp;
  isValid = nsSchemaValidatorUtils::IsValidSchemaInteger(aWholePart, &temp);

  if (isValid && (findString != kNotFound)) {
    // XX: assuming "2." is not valid
    if (aFractionPart.IsEmpty())
      isValid = PR_FALSE;
    else if ((aFractionPart.First() == '-') || (aFractionPart.First() == '+'))
      isValid = PR_FALSE;
    else
      isValid = nsSchemaValidatorUtils::IsValidSchemaInteger(aFractionPart, &temp);
  }

  return isValid;
}

/* compares 2 strings that contain fraction parts of a decimal

 -1 - aString1 < aString2
  0 - equal
  1 - aString1 > aString2

 */
int
nsSchemaValidator::CompareFractionStrings(const nsAString & aString1,
                                          const nsAString & aString2)
{
  int cmpresult;

  // are the equal?
  if (aString1.Equals(aString2))
    return 0;

  nsAutoString compareString1;
  nsAutoString compareString2;

  // put the shorter string in compareString1
  if (aString1.Length() < aString2.Length()) {
    compareString1.Assign(aString1);
    compareString2.Assign(aString2);
  } else {
    compareString1.Assign(aString2);
    compareString2.Assign(aString1);
  }

  nsAString::const_iterator start1, end1, start2, end2;
  compareString1.BeginReading(start1);
  compareString1.EndReading(end1);

  compareString2.BeginReading(start2);
  compareString2.EndReading(end2);

  PRBool done = PR_FALSE;

  while ((start1 != end1) && !done)
  {
    if (*++start1 != *++start2)
      done = PR_TRUE;
  }

  // first string has been iterated through, and all matched
  // we know the 2 cannot be the same due to the .Equals() check above,
  // so this means that string2 is equal to string1 and then has some more
  // numbers, meaning string2 is bigger
  if ((start1 == end1) && !done) {
    cmpresult = -1;
  } else if (done) {
    // *start1 is != *start2
    if (*start1 < *start2)
      cmpresult = 1;
    else
      cmpresult = -1;
  }

  return cmpresult;
}

/* http://w3c.org/TR/xmlschema-2/#anyURI */
nsresult
nsSchemaValidator::ValidateBuiltinTypeAnyURI(const nsAString & aNodeValue,
                                             PRUint32 aLength,
                                             PRUint32 aMinLength,
                                             PRUint32 aMaxLength,
                                             nsStringArray *aEnumerationList,
                                             PRBool *aResult)
{
  PRUint32 length = aNodeValue.Length();
  PRBool isValid = PR_FALSE;

  isValid = (IsValidSchemaAnyURI(aNodeValue)) &&
            (!aLength || length == aLength) &&
            (!aMinLength || length >= aMinLength) &&
            (!aMaxLength || length <= aMaxLength);

  if (isValid && aEnumerationList && (aEnumerationList->Count() > 0)) {
    isValid = nsSchemaValidatorUtils::HandleEnumeration(aNodeValue, *aEnumerationList);
  }

  *aResult = isValid;
  return NS_OK;
}

PRBool
nsSchemaValidator::IsValidSchemaAnyURI(const nsAString & aString)
{
  PRBool isValid = PR_FALSE;
  nsresult rv;

  nsCOMPtr<nsIURI> uri;
  rv = NS_NewURI(getter_AddRefs(uri), aString);

  if (rv == NS_OK)
    isValid = PR_TRUE;

  return isValid;
}

// http://w3c.org/TR/xmlschema-2/#base64Binary
nsresult
nsSchemaValidator::ValidateBuiltinTypeBase64Binary(const nsAString & aNodeValue,
                                                   PRUint32 aLength,
                                                   PRBool aLengthDefined,
                                                   PRUint32 aMinLength,
                                                   PRBool aMinLengthDefined,
                                                   PRUint32 aMaxLength,
                                                   PRBool aMaxLengthDefined,
                                                   nsStringArray *aEnumerationList,
                                                   PRBool *aResult)
{
  PRBool isValid = PR_FALSE;
  PRUint32 length;

  char* decodedString;
  isValid = IsValidSchemaBase64Binary(aNodeValue, &decodedString);

  if (isValid) {
    length = strlen(decodedString);

    if (aLengthDefined && (length != aLength)) {  
      isValid = PR_FALSE;
      LOG(("  Not valid: Not the right length (%d)", length));
    }

    if (aMinLengthDefined && (length < aMinLength)) {
      isValid = PR_FALSE;
      LOG(("  Not valid: Length (%d) is too small", length));
    }

    if (aMaxLengthDefined && (length > aMaxLength)) {
      isValid = PR_FALSE;
      LOG(("  Not valid: Length (%d) is too large", length));
    }
  }

  if (isValid && aEnumerationList && (aEnumerationList->Count() > 0)) {
    isValid = nsSchemaValidatorUtils::HandleEnumeration(aNodeValue, *aEnumerationList);
  }

#ifdef PR_LOGGING
  LOG((isValid ? ("  Value is valid!") : ("  Value is not valid!")));
#endif

  nsMemory::Free(decodedString);
  *aResult = isValid;
  return NS_OK;
}

PRBool
nsSchemaValidator::IsValidSchemaBase64Binary(const nsAString & aString,
                                             char** aDecodedString)
{
  *aDecodedString = PL_Base64Decode(NS_ConvertUTF16toUTF8(aString).get(),
                                    aString.Length(), nsnull);
  return (*aDecodedString != nsnull);
}

// http://www.w3.org/TR/xmlschema-2/#QName
nsresult
nsSchemaValidator::ValidateBuiltinTypeQName(const nsAString & aNodeValue,
                                            PRUint32 aLength,
                                            PRBool aLengthDefined,
                                            PRUint32 aMinLength,
                                            PRBool aMinLengthDefined,
                                            PRUint32 aMaxLength,
                                            PRBool aMaxLengthDefined,
                                            nsStringArray *aEnumerationList,
                                            PRBool *aResult)
{
  PRBool isValid = PR_FALSE;
  PRUint32 length;

  isValid = IsValidSchemaQName(aNodeValue);

  if (isValid) {
    length = aNodeValue.Length();

    if (aLengthDefined && (length != aLength)) {  
      isValid = PR_FALSE;
      LOG(("  Not valid: Not the right length (%d)", length));
    }

    if (aMinLengthDefined && (length < aMinLength)) {
      isValid = PR_FALSE;
      LOG(("  Not valid: Length (%d) is too small", length));
    }

    if (aMaxLengthDefined && (length > aMaxLength)) {
      isValid = PR_FALSE;
      LOG(("  Not valid: Length (%d) is too large", length));
    }
  }

  if (isValid && aEnumerationList && (aEnumerationList->Count() > 0)) {
    isValid = nsSchemaValidatorUtils::HandleEnumeration(aNodeValue, *aEnumerationList);
  }

#ifdef PR_LOGGING
  LOG((isValid ? ("  Value is valid!") : ("  Value is not valid!")));
#endif

  *aResult = isValid;
  return NS_OK;
}

PRBool
nsSchemaValidator::IsValidSchemaQName(const nsAString & aString)
{
  PRBool isValid = PR_FALSE;

  nsresult rv;
  // split type (ns:type) into namespace and type.
  nsCOMPtr<nsIParserService> parserService =
    do_GetService("@mozilla.org/parser/parser-service;1", &rv);
  NS_ENSURE_SUCCESS(rv, PR_FALSE);

  const PRUnichar *colon;
  const nsAFlatString& qName = PromiseFlatString(aString);
  rv = parserService->CheckQName(qName, PR_TRUE, &colon);
  if (NS_SUCCEEDED(rv)) {
    isValid = PR_TRUE;
  }

  return isValid;
}

#ifdef DEBUG
void
nsSchemaValidator::DumpBaseType(nsISchemaBuiltinType *aBuiltInType)
{
  nsAutoString typeName;
  aBuiltInType->GetName(typeName);
  PRUint16 foo;
  aBuiltInType->GetBuiltinType(&foo);

  LOG(("  Base Type is %s (%d)", NS_ConvertUTF16toUTF8(typeName).get(),foo));
}
#endif

