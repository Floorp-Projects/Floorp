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

#include "nsSchemaValidator.h"

// content includes
#include "nsIDOMNode.h"
#include "nsIDOMElement.h"
#include "nsIDOMNodeList.h"

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

#include <stdlib.h>
#include <math.h>
#include "prprf.h"
#include "prtime.h"
#include "plbase64.h"

#define NS_SCHEMA_1999_NAMESPACE "http://www.w3.org/1999/XMLSchema"
#define NS_SCHEMA_2001_NAMESPACE "http://www.w3.org/2001/XMLSchema"
#define kREGEXP_CID "@mozilla.org/xmlextras/schemas/schemavalidatorregexp;1"

NS_IMPL_ISUPPORTS1_CI(nsSchemaValidator, nsISchemaValidator)

NS_NAMED_LITERAL_STRING(EmptyLiteralString, "");

nsSchemaValidator::nsSchemaValidator()
  : mSchema(nsnull)
{}

nsSchemaValidator::~nsSchemaValidator()
{
  /* destructor code */
}

////////////////////////////////////////////////////////////
//
// nsSchemaValidator implementation
//
////////////////////////////////////////////////////////////

NS_IMETHODIMP nsSchemaValidator::LoadSchema(nsISchema* aSchema)
{
  mSchema = aSchema;
  return NS_OK;
}

NS_IMETHODIMP nsSchemaValidator::ValidateString(const nsAString & aValue,
                                                const nsAString & aType,
                                                const nsAString & aNamespace,
                                                PRBool *aResult)
{
  nsresult rv;
  PRBool isValid = PR_FALSE;

#ifdef DEBUG
  printf("\n --------- nsSchemaValidator::ValidateString called ---------");
#endif

  if (aValue.IsEmpty() || aType.IsEmpty() || aNamespace.IsEmpty())
    return NS_ERROR_NOT_AVAILABLE;

  // no schemas loaded and type is not builtin, abort
  if (!mSchema && !aNamespace.EqualsLiteral(NS_SCHEMA_1999_NAMESPACE) &&
      !aNamespace.EqualsLiteral(NS_SCHEMA_2001_NAMESPACE))
    return NS_ERROR_NOT_AVAILABLE;;

  // figure out if its a simple or complex type
  nsCOMPtr<nsISchemaType> type;
  rv = GetType(aType, aNamespace, getter_AddRefs(type));
  NS_ENSURE_SUCCESS(rv, rv);

  if (!type)
    return NS_ERROR_UNEXPECTED;

  PRUint16 typevalue;
  rv = type->GetSchemaType(&typevalue);
  NS_ENSURE_SUCCESS(rv, rv);

  if (typevalue == nsISchemaType::SCHEMA_TYPE_SIMPLE) {
    nsCOMPtr<nsISchemaSimpleType> simpleType = do_QueryInterface(type);

    if (simpleType) {
#ifdef DEBUG
      printf("\n  Type is a simple type!");
      printf("\n  String to validate is: |%s|", NS_ConvertUTF16toUTF8(aValue).get());
#endif
      rv = ValidateSimpletype(aValue, simpleType, &isValid);
    }
  } else {
    // if its not a simpletype, validating a string makes no sense.
    rv = NS_ERROR_UNEXPECTED;
  }

  *aResult = isValid;
  return rv;
}

NS_IMETHODIMP nsSchemaValidator::Validate(nsIDOMNode* aElement, PRBool *aResult)
{
  nsresult rv;
  PRBool isValid = PR_FALSE;

#ifdef DEBUG
  printf("\n \n \n nsSchemaValidator::Validate called.");
#endif

  if (!mSchema)
    return NS_ERROR_SCHEMAVALIDATOR_NO_SCHEMA_LOADED;

  if (!aElement)
    return NS_ERROR_SCHEMAVALIDATOR_NO_DOM_NODE_LOADED;

  // get the Schema "type" attribute
  nsAutoString elementType;

  nsCOMPtr<nsIDOMElement> domElement = do_QueryInterface(aElement);
  if (!domElement) 
    return NS_ERROR_UNEXPECTED;

  rv = domElement->GetAttributeNS(NS_LITERAL_STRING(NS_SCHEMA_1999_NAMESPACE),
                             NS_LITERAL_STRING("type"),
                             elementType);
  NS_ENSURE_SUCCESS(rv, rv);

  if (elementType.IsEmpty())
    return NS_ERROR_SCHEMAVALIDATOR_NO_TYPE_FOUND;

#ifdef DEBUG
  printf("\n  Type to validate against is %s", 
         NS_LossyConvertUTF16toASCII(elementType).get());
#endif  

  // get the namespace the node is in
  nsAutoString elementNamespace;
  rv = aElement->GetNamespaceURI(elementNamespace);
  NS_ENSURE_SUCCESS(rv, rv);

  // no namespace found, abort
  if (elementNamespace.IsEmpty())
    return NS_ERROR_UNEXPECTED;

  // get the nsISchemaCollection from the nsISchema
  nsCOMPtr<nsISchemaCollection> schemaCollection;
  rv = mSchema->GetCollection(getter_AddRefs(schemaCollection));
  NS_ENSURE_SUCCESS(rv, rv);

  if (!schemaCollection) 
    return NS_ERROR_UNEXPECTED;

  // figure out if its a simple or complex type
  nsCOMPtr<nsISchemaType> type;
  rv = GetType(elementType, elementNamespace, getter_AddRefs(type));
  NS_ENSURE_SUCCESS(rv, rv);

  if (!type)
    return NS_ERROR_UNEXPECTED;

  PRUint16 typevalue;
  rv = type->GetSchemaType(&typevalue);
  NS_ENSURE_SUCCESS(rv, rv);

  if (typevalue == nsISchemaType::SCHEMA_TYPE_SIMPLE) {
    nsCOMPtr<nsISchemaSimpleType> simpleType = do_QueryInterface(type);

    if (simpleType) {
      // get the nodevalue
      nsAutoString nodeValue;
      nsCOMPtr<nsIDOMNode> childNode;

      rv = aElement->GetFirstChild(getter_AddRefs(childNode));
      NS_ENSURE_SUCCESS(rv, rv);
      
      if (!childNode)
        return NS_ERROR_UNEXPECTED;

      childNode->GetNodeValue(nodeValue);

#ifdef DEBUG
      printf("\n  Type is a simple type!");
      printf("\n  String to validate is: |%s|", NS_ConvertUTF16toUTF8(nodeValue).get());
#endif
      rv = ValidateSimpletype(nodeValue, simpleType, &isValid);
    }
  } else if (typevalue == nsISchemaType::SCHEMA_TYPE_COMPLEX) {
    // XXX to be coded
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
nsresult nsSchemaValidator::GetType(const nsAString & aType,
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

    nsCOMPtr<nsISchemaCollection> collection(do_QueryInterface(schemaLoader));
    rv = collection->GetType(aType, aNamespace, aSchemaType);
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    // get the nsISchemaCollection from the nsISchema
    nsCOMPtr<nsISchemaCollection> schemaCollection;
    mSchema->GetCollection(getter_AddRefs(schemaCollection));
    if (!schemaCollection) 
      return NS_ERROR_NULL_POINTER;

    // First try looking for xsd:type
    rv = schemaCollection->GetType(aType, aNamespace, aSchemaType);
    NS_ENSURE_SUCCESS(rv, rv);

    // maybe its a xsd:element
    if (!*aSchemaType) {
      nsCOMPtr<nsISchemaElement> schemaElement;
      rv = schemaCollection->GetElement(aType, aNamespace,
                                        getter_AddRefs(schemaElement));
      NS_ENSURE_SUCCESS(rv, rv);

      if (schemaElement){
        rv = schemaElement->GetType(aSchemaType);
        NS_ENSURE_SUCCESS(rv, rv);
      }
    }

    if (!aSchemaType) {
      // if its not a built-in type as well, its an unknown schema type.
      rv = NS_ERROR_SCHEMAVALIDATOR_TYPE_NOT_FOUND;
    }
  }

  return rv;
}

nsresult nsSchemaValidator::ValidateSimpletype(const nsAString & aNodeValue, 
  nsISchemaSimpleType *aSchemaSimpleType, PRBool *aResult)
{
  nsresult rv;
  PRBool isValid = PR_FALSE;

  // get the simpletype type.
  PRUint16 simpleTypeValue;
  rv = aSchemaSimpleType->GetSimpleType(&simpleTypeValue);
  NS_ENSURE_SUCCESS(rv, rv);

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
    
    case nsISchemaSimpleType::SIMPLE_TYPE_LIST:
    case nsISchemaSimpleType::SIMPLE_TYPE_UNION: {
      // XXX to be coded
      rv = NS_ERROR_NOT_IMPLEMENTED;
      break;
    }
  }

  *aResult = isValid;
  return rv;
}

/*
    Simpletype restrictions allow restricting a built-in type with several
    facets, such as totalDigits or maxLength.
*/
nsresult nsSchemaValidator::ValidateRestrictionSimpletype(const nsAString & aNodeValue,
  nsISchemaSimpleType *aSchemaSimpleType, PRBool *aResult)
{
  nsresult rv;
  PRBool isValid = PR_FALSE;

  nsCOMPtr<nsISchemaRestrictionType> restrictionType =
    do_QueryInterface(aSchemaSimpleType);

  if (!restrictionType)
    return NS_ERROR_UNEXPECTED;

#ifdef DEBUG
  printf("\n  The simpletype definition contains restrictions.");
#endif

  nsCOMPtr<nsISchemaSimpleType> simpleBaseType;
  rv = restrictionType->GetBaseType(getter_AddRefs(simpleBaseType));
  NS_ENSURE_SUCCESS(rv, rv);

  // get the amount of restriction facet defined.
  PRUint32 facetCount;
  rv = restrictionType->GetFacetCount(&facetCount);
  NS_ENSURE_SUCCESS(rv, rv);

#ifdef DEBUG
  printf("\n    %d facet(s) defined.", facetCount);
#endif

  // we create variables for all possible restriction facets and populate them
  // if we run into them.  This is faster than having the same code repeated
  // for each built-in type and only handling the relevant facets.
  PRUint32 length    = nsnull;
  PRUint32 minLength = nsnull;
  PRUint32 maxLength = nsnull;
  nsAutoString pattern;
  unsigned short whitespace = nsnull;
  nsAutoString maxInclusive;
  nsAutoString minInclusive;
  nsAutoString maxExclusive;
  nsAutoString minExclusive;
  PRUint32 totalDigits = nsnull;
  PRUint32 fractionDigits = nsnull;

  nsCOMPtr<nsISchemaFacet> facet;
  PRUint32 facetCounter;
  PRUint16 facetType;

  // handle all defined facets
  for (facetCounter = 0; facetCounter < facetCount; facetCounter++){
    rv = restrictionType->GetFacet(facetCounter, getter_AddRefs(facet));
    NS_ENSURE_SUCCESS(rv, rv);

    facet->GetFacetType(&facetType);

    switch (facetType) {
      case nsISchemaFacet::FACET_TYPE_LENGTH: {
        facet->GetLengthValue(&length);

#ifdef DEBUG
        printf("\n  - Length Facet found (value is %d)", length);
#endif
        break;
      }

      case nsISchemaFacet::FACET_TYPE_MINLENGTH: {
        facet->GetLengthValue(&minLength);

#ifdef DEBUG
        printf("\n  - Min Length Facet found (value is %d)", minLength);
#endif
        break;
      }

      case nsISchemaFacet::FACET_TYPE_MAXLENGTH: {
        facet->GetLengthValue(&maxLength);
#ifdef DEBUG
        printf("\n  - Max Length Facet found (value is %d)", maxLength);
#endif
        break;
      }

      case nsISchemaFacet::FACET_TYPE_PATTERN: {
        facet->GetValue(pattern);
        break;
      }

      case nsISchemaFacet::FACET_TYPE_ENUMERATION: {
        // not supported
        break;
      }

      case nsISchemaFacet::FACET_TYPE_WHITESPACE: {
        facet->GetWhitespaceValue(&whitespace);
        break;
      }
      
      case nsISchemaFacet::FACET_TYPE_MAXINCLUSIVE: {
        facet->GetValue(maxInclusive);
#ifdef DEBUG
        printf("\n  - Max Inclusive Facet found (value is %s)",  NS_ConvertUTF16toUTF8(maxInclusive).get());
#endif
        break;
      }

      case nsISchemaFacet::FACET_TYPE_MININCLUSIVE: {
        facet->GetValue(minInclusive);

#ifdef DEBUG
        printf("\n  - Min Inclusive Facet found (value is %s)",  NS_ConvertUTF16toUTF8(minInclusive).get());
#endif
        break;
      }

      case nsISchemaFacet::FACET_TYPE_MAXEXCLUSIVE: {
        facet->GetValue(maxExclusive);

#ifdef DEBUG
        printf("\n  - Max Exclusive Facet found (value is %s)",  NS_ConvertUTF16toUTF8(maxExclusive).get());
#endif
        break;
      }

      case nsISchemaFacet::FACET_TYPE_MINEXCLUSIVE: {
        facet->GetValue(minExclusive);

#ifdef DEBUG
        printf("\n  - Min Exclusive Facet found (value is %s)",  NS_ConvertUTF16toUTF8(minExclusive).get());
#endif
        break;
      }

      case nsISchemaFacet::FACET_TYPE_TOTALDIGITS: {
        facet->GetDigitsValue(&totalDigits);

#ifdef DEBUG
        printf("\n  - Totaldigits Facet found (value is %d)", totalDigits);
#endif
        break;
      }

      case nsISchemaFacet::FACET_TYPE_FRACTIONDIGITS: {
        facet->GetDigitsValue(&fractionDigits);
        break;
      }
    }
  }

  // now that we have loaded all the restriction facets, 
  // check the base type and validate
  PRUint16 builtinTypeValue;
  nsCOMPtr<nsISchemaBuiltinType> schemaBuiltinType = 
    do_QueryInterface(simpleBaseType);

  if (!schemaBuiltinType)
    return NS_ERROR_UNEXPECTED;

  schemaBuiltinType->GetBuiltinType(&builtinTypeValue);

#ifdef DEBUG
  DumpBaseType(schemaBuiltinType);
#endif

  switch(builtinTypeValue) {
    case nsISchemaBuiltinType::BUILTIN_TYPE_STRING: {
      rv = ValidateBuiltinTypeString(aNodeValue, length, minLength,
                                maxLength, &isValid);
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

    case nsISchemaBuiltinType::BUILTIN_TYPE_INTEGER: {
      rv = ValidateBuiltinTypeInteger(aNodeValue, totalDigits, maxExclusive,
             minExclusive, maxInclusive, minInclusive, &isValid);
      break;
    }

    /* http://w3.org/TR/xmlschema-2/#nonPositiveInteger */
    case nsISchemaBuiltinType::BUILTIN_TYPE_NONPOSITIVEINTEGER: {
      rv = ValidateBuiltinTypeInteger(aNodeValue, totalDigits,
             maxExclusive.IsEmpty() ? (const nsAString&)NS_LITERAL_STRING("1") : (const nsAString&)maxExclusive,
             minExclusive,
             maxInclusive.IsEmpty() ? (const nsAString&)NS_LITERAL_STRING("0") : (const nsAString&)maxInclusive,
             minInclusive, &isValid);
      break;
    }

    /* http://www.w3.org/TR/xmlschema-2/#negativeInteger */
    case nsISchemaBuiltinType::BUILTIN_TYPE_NEGATIVEINTEGER: {
      rv = ValidateBuiltinTypeInteger(aNodeValue, totalDigits, 
        maxExclusive.IsEmpty() ? (const nsAString&)NS_LITERAL_STRING("0") : (const nsAString&)maxExclusive, minExclusive, 
        maxInclusive.IsEmpty() ? (const nsAString&)NS_LITERAL_STRING("-1") : (const nsAString&) maxInclusive, minInclusive, 
        &isValid);
      break;
    }

    case nsISchemaBuiltinType::BUILTIN_TYPE_BYTE: {
      rv = ValidateBuiltinTypeByte(aNodeValue, totalDigits, maxExclusive, minExclusive,
             maxInclusive, minInclusive, &isValid);
      break;
    }

    case nsISchemaBuiltinType::BUILTIN_TYPE_FLOAT: {
      rv = ValidateBuiltinTypeFloat(aNodeValue, totalDigits, maxExclusive, minExclusive,
             maxInclusive, minInclusive, &isValid);
      break;
    }

   case nsISchemaBuiltinType::BUILTIN_TYPE_DECIMAL: {
      rv = ValidateBuiltinTypeDecimal(aNodeValue, totalDigits, maxExclusive, minExclusive,
             maxInclusive, minInclusive, &isValid);
      break;
    }

    case nsISchemaBuiltinType::BUILTIN_TYPE_ANYURI: {
      rv = ValidateBuiltinTypeAnyURI(aNodeValue, length, minLength,
             maxLength, &isValid);
      break;
    }

    case nsISchemaBuiltinType::BUILTIN_TYPE_BASE64BINARY: {
      rv = ValidateBuiltinTypeBase64Binary(aNodeValue, length, minLength,
             maxLength, &isValid);
      break;
    }


    default:
      rv = NS_ERROR_SCHEMAVALIDATOR_TYPE_NOT_FOUND;
      break;
  }

  // finally check if a pattern is defined, as all types can be contrained by
  // regexp patterns.
  if (isValid && !pattern.IsEmpty()) {
    // check if the pattern matches
    nsCOMPtr<nsISchemaValidatorRegexp> regexp = do_GetService(kREGEXP_CID);
    rv = regexp->RunRegexp(aNodeValue,
                           pattern,
                           "g", &isValid);
#ifdef DEBUG
    printf("\n  Regular Expression Pattern Facet found");

    if (isValid) {
      printf("\n    -- pattern validates!");
    } else {
      printf("\n    -- pattern does not validate!");
    }
#endif
  }

  *aResult = isValid;
  return rv;
}

/*
  Handles validation of built-in schema types.
*/
nsresult nsSchemaValidator::ValidateBuiltinType(const nsAString & aNodeValue, 
  nsISchemaSimpleType *aSchemaSimpleType, PRBool *aResult)
{
  nsresult rv;
  PRBool isValid = PR_FALSE;

  nsCOMPtr<nsISchemaBuiltinType> builtinType = do_QueryInterface(aSchemaSimpleType);

  if (!builtinType)
    return NS_ERROR_UNEXPECTED;

  PRUint16 builtinTypeValue;
  rv = builtinType->GetBuiltinType(&builtinTypeValue);
  NS_ENSURE_SUCCESS(rv, rv);

#ifdef DEBUG
  DumpBaseType(builtinType);
#endif

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

    case nsISchemaBuiltinType::BUILTIN_TYPE_INTEGER: {
      isValid = nsSchemaValidatorUtils::IsValidSchemaInteger(aNodeValue, nsnull);
      break;
    }

    /* http://w3.org/TR/xmlschema-2/#nonPositiveInteger */
    case nsISchemaBuiltinType::BUILTIN_TYPE_NONPOSITIVEINTEGER: {
      // nonPositiveInteger inherits from integer, with maxExclusive 
      // being 1
      ValidateBuiltinTypeInteger(aNodeValue, nsnull, 
        NS_LITERAL_STRING("1"), EmptyLiteralString, EmptyLiteralString, 
        EmptyLiteralString, &isValid);
      break;
    }

    /* http://www.w3.org/TR/xmlschema-2/#negativeInteger */
    case nsISchemaBuiltinType::BUILTIN_TYPE_NEGATIVEINTEGER: {
      // negativeInteger inherits from integer, with maxExclusive 
      // being 0 (only negative numbers)
      ValidateBuiltinTypeInteger(aNodeValue, nsnull, 
        NS_LITERAL_STRING("0"), EmptyLiteralString, EmptyLiteralString, 
        EmptyLiteralString, &isValid);
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
      nsAutoString wholePart;
      nsAutoString fractionPart;
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

    default:
      rv = NS_ERROR_SCHEMAVALIDATOR_TYPE_NOT_FOUND;
      break;
  }

  *aResult = isValid;
  return rv;
}

/* http://www.w3.org/TR/xmlschema-2/#string */
nsresult nsSchemaValidator::ValidateBuiltinTypeString(const nsAString & aNodeValue,
  PRUint32 aLength, PRUint32 aMinLength, PRUint32 aMaxLength, PRBool *aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  PRBool isValid = PR_TRUE;

  PRUint32 length = aNodeValue.Length();

  if (aLength && (length != aLength)) {  
    isValid = PR_FALSE;
#ifdef DEBUG
    printf("\n  Not valid: Not the right length (%d)", length);
#endif
  }

  if (isValid && aMinLength && (length < aMinLength)) {
    isValid = PR_FALSE;
#ifdef DEBUG
    printf("\n  Not valid: Length (%d) is too small", length);
#endif
  }

  if (isValid && aMaxLength && (length > aMaxLength)) {
    isValid = PR_FALSE;
#ifdef DEBUG
    printf("\n  Not valid: Length (%d) is too large", length);
#endif  
  }

#ifdef DEBUG
  if (isValid)
    printf("\n  Value is valid!");
#endif

 *aResult = isValid;
  return NS_OK;
}

/* http://www.w3.org/TR/xmlschema-2/#boolean */
nsresult nsSchemaValidator::ValidateBuiltinTypeBoolean(const nsAString & aNodeValue,
  PRBool *aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  PRBool isValid = PR_FALSE;

  // possible values are 0, 1, true, false.  Spec makes no mention if TRUE is
  // valid, so performing a case insenstive comparision to be safe
  if (aNodeValue.Equals(NS_LITERAL_STRING("false"), nsCaseInsensitiveStringComparator())
      || aNodeValue.Equals(NS_LITERAL_STRING("true"), nsCaseInsensitiveStringComparator())
      || aNodeValue.EqualsLiteral("1")
      || aNodeValue.EqualsLiteral("0")
     )
  {
    isValid = PR_TRUE;
#ifdef DEBUG
    printf("\n  Value is valid!");
#endif
  }

#ifdef DEBUG
  if (!isValid)
    printf("\n  Not valid: Value (%s) is not in {1,0,true,false}.", 
      NS_ConvertUTF16toUTF8(aNodeValue).get());
#endif

 *aResult = isValid;
  return NS_OK;
}

/* http://www.w3.org/TR/xmlschema-2/#gDay */
nsresult nsSchemaValidator::ValidateBuiltinTypeGDay(const nsAString & aNodeValue, 
  const nsAString & aMaxExclusive, const nsAString & aMinExclusive,
  const nsAString & aMaxInclusive, const nsAString & aMinInclusive,
  PRBool *aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  PRBool isValidGDay;
  Schema_GDay gday;

  PRBool isValid = IsValidSchemaGDay(aNodeValue, &gday);

  if (isValid && !aMaxExclusive.IsEmpty()) {
    Schema_GDay maxExclusive;
    isValidGDay = IsValidSchemaGDay(aMaxExclusive, &maxExclusive);

    if (isValidGDay) {
      if (gday.day >= maxExclusive.day) {
        isValid = PR_FALSE;
#ifdef DEBUG
        printf("\n  Not valid: Value (%d) is too large", gday.day);
#endif
      }
    }
  }

  if (isValid && !aMinExclusive.IsEmpty()) {
    Schema_GDay minExclusive;
    isValidGDay = IsValidSchemaGDay(aMinExclusive, &minExclusive);

    if (isValidGDay) {
      if (gday.day <= minExclusive.day) {
        isValid = PR_FALSE;
#ifdef DEBUG
        printf("\n  Not valid: Value (%d) is too small", gday.day);
#endif
      }
    }
  }

  if (isValid && !aMaxInclusive.IsEmpty()) {
    Schema_GDay maxInclusive;
    isValidGDay = IsValidSchemaGDay(aMaxInclusive, &maxInclusive);
    
    if (isValidGDay) {
      if (gday.day > maxInclusive.day) {
        isValid = PR_FALSE;
#ifdef DEBUG
        printf("\n  Not valid: Value (%d) is too large", gday.day);
#endif
      }
    }
  }

  if (isValid && !aMinInclusive.IsEmpty()) {
    Schema_GDay minInclusive;
    isValidGDay = IsValidSchemaGDay(aMinInclusive, &minInclusive);
    
    if (isValidGDay) {
      if (gday.day < minInclusive.day) {
        isValid = PR_FALSE;
#ifdef DEBUG
        printf("\n  Not valid: Value (%d) is too small", gday.day);
#endif
      }
    }
  }

#ifdef DEBUG
  if (isValid)
    printf("\n  Value is valid!");
#endif

 *aResult = isValid;
  return NS_OK;
}

PRBool nsSchemaValidator::IsValidSchemaGDay(const nsAString & aNodeValue, Schema_GDay *aResult){
  //---DD(Z|(+|-)hh:mm)

  int strLength = aNodeValue.Length();
  //   ---DD               ---DDZ              ---DD(+|-)hh:mm
  if ((strLength != 5) && (strLength != 6) && (strLength != 11))
    return PR_FALSE;

  PRBool isValid = PR_FALSE;
  NS_ConvertUTF16toUTF8 str(aNodeValue);

  char day[3] = "";
  char timezoneHour[3] = "";
  char timezoneMinute[3] = "";
  int dayInt;

  char delimeter[4] = "";
  strncpy(delimeter, str.get(), 3);
  delimeter[sizeof(delimeter)-1] = '\0';

  // check for delimeter
  if (strcmp(delimeter, "---") == 0) {
    strncpy(day, str.get()+3, 2);
    day[sizeof(day)-1] = '\0';

    // validate the day
    isValid = IsValidSchemaGType(NS_ConvertASCIItoUTF16(day), 1, 30, &dayInt);

    // validate the timezone - Z or (+|-)hh:mm
    if (isValid && (strLength > 5)) {
      if (strLength == 6) {
        // timezone is one character, so has to be 'Z'
        if (str[5] != 'Z')
          isValid = PR_FALSE;
      } else {
        // ---DD(+|-)hh:mm

        if ((str[5] == '+') || (str[5] == '-')) {
          char timezone[6] = "";
          strncpy(timezone, str.get()+6, 5);
          timezone[sizeof(timezone)-1] = '\0';

          isValid = nsSchemaValidatorUtils::ParseSchemaTimeZone(timezone,
                      timezoneHour, timezoneMinute);
        } else {
          // timezone does not start with +/-
          isValid = PR_FALSE;
        }
      }
    }
  }
  
  if (isValid && aResult) {
    char * pEnd;
    aResult->day = dayInt;
    aResult->tz_negative = (str[5] == '-') ? PR_TRUE : PR_FALSE;
    aResult->tz_hour = (timezoneHour[0] == '\0') ? nsnull : strtol(timezoneHour, &pEnd, 10);
    aResult->tz_minute = (timezoneMinute[0] == '\0') ? nsnull : strtol(timezoneMinute, &pEnd, 10);
  }

  return isValid;
}

// Helper function used to validate gregorian date types
PRBool nsSchemaValidator::IsValidSchemaGType(const nsAString & aNodeValue, 
    long aMinValue, long aMaxValue, int *aResult){

  PRBool isValid = PR_FALSE;
  long intValue;
  isValid = nsSchemaValidatorUtils::IsValidSchemaInteger(aNodeValue, &intValue);

  if (isValid && ( (intValue < aMinValue) || (intValue > aMaxValue) ))
    isValid = PR_FALSE;

  *aResult = intValue;
  return isValid;
}

/* http://www.w3.org/TR/xmlschema-2/#gMonth */
nsresult nsSchemaValidator::ValidateBuiltinTypeGMonth(const nsAString & aNodeValue, 
  const nsAString & aMaxExclusive, const nsAString & aMinExclusive,
  const nsAString & aMaxInclusive, const nsAString & aMinInclusive,
  PRBool *aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  PRBool isValidGMonth;
  Schema_GMonth gmonth;
  PRBool isValid = IsValidSchemaGMonth(aNodeValue, &gmonth);

  if (isValid && !aMaxExclusive.IsEmpty()) {
    Schema_GMonth maxExclusive;
    isValidGMonth = IsValidSchemaGMonth(aMaxExclusive, &maxExclusive);
    
    if (isValidGMonth) {
      if (gmonth.month >= maxExclusive.month) {
        isValid = PR_FALSE;
#ifdef DEBUG
        printf("\n  Not valid: Value (%d) is too large", gmonth.month);
#endif
      }
    }
  }

  if (isValid && !aMinExclusive.IsEmpty()) {
    Schema_GMonth minExclusive;
    isValidGMonth = IsValidSchemaGMonth(aMinExclusive, &minExclusive);
    
    if (isValidGMonth) {
      if (gmonth.month <= minExclusive.month) {
        isValid = PR_FALSE;
#ifdef DEBUG
        printf("\n  Not valid: Value (%d) is too small", gmonth.month);
#endif
      }
    }
  }

  if (isValid && !aMaxInclusive.IsEmpty()) {
    Schema_GMonth maxInclusive;
    isValidGMonth = IsValidSchemaGMonth(aMaxInclusive, &maxInclusive);
    
    if (isValidGMonth) {
      if (gmonth.month > maxInclusive.month) {
        isValid = PR_FALSE;
#ifdef DEBUG
        printf("\n  Not valid: Value (%d) is too large", gmonth.month);
#endif
      }
    }
  }

  if (isValid && !aMinInclusive.IsEmpty()) {
    Schema_GMonth minInclusive;
    isValidGMonth = IsValidSchemaGMonth(aMinInclusive, &minInclusive);
    
    if (isValidGMonth) {
      if (gmonth.month < minInclusive.month) {
        isValid = PR_FALSE;
#ifdef DEBUG
        printf("\n  Not valid: Value (%d) is too small", gmonth.month);
#endif
      }
    }
  }

#ifdef DEBUG
  if (isValid)
    printf("\n  Value is valid!");
#endif

 *aResult = isValid;
  return NS_OK;
}

PRBool nsSchemaValidator::IsValidSchemaGMonth(const nsAString & aNodeValue, 
  Schema_GMonth *aResult){
  // --MM--(Z|(+|-)hh:mm)
 
  int strLength = aNodeValue.Length();
  
  //   --MM--              --MM--Z             --MM--(+|-)hh:mm
  if ((strLength != 6) && (strLength != 7) && (strLength != 12))
    return PR_FALSE;

  PRBool isValid = PR_FALSE;  
  char month[3] = "";
  char timezoneHour[3] = "";
  char timezoneMinute[3] = "";
  int monthInt;
  NS_ConvertUTF16toUTF8 str(aNodeValue);

  char delimeter[3] = "";  
  strncpy(delimeter, str.get(), 2);
  delimeter[sizeof(delimeter)-1] = '\0';

  // check for the first |--| delimeter
  if (strcmp(delimeter, "--") == 0) {
    // check for the 2nd |--| delimeter
    strncpy(delimeter, str.get()+4, 2);
    delimeter[sizeof(delimeter)-1] = '\0';

    if (strcmp(delimeter, "--") == 0)
      isValid = PR_TRUE;
  }

  // check if month is valid
  if (isValid) {
    strncpy(month, str.get()+2, 2);
    month[sizeof(month)-1] = '\0';

    isValid = IsValidSchemaGType(NS_ConvertASCIItoUTF16(month), 1, 12, &monthInt);
  }

  // validate the timezone, if it exists
  if (isValid && (strLength > 6)) {
    if (strLength == 7) {
      if (str[6] != 'Z')
        isValid = PR_FALSE;
    } else {
      // timezone needs to start with +/-
      if ((str[6] == '+') || (str[6] == '-')){
        // (+|-)hh:mm
        char timezone[6] = "";
        strncpy(timezone, str.get()+7, 5);
        timezone[sizeof(timezone)-1] = '\0';

        isValid = nsSchemaValidatorUtils::ParseSchemaTimeZone(timezone, timezoneHour, timezoneMinute);
      } else {
        isValid = PR_FALSE;
      }
    }
  }
  
  if (isValid && aResult) {
    char * pEnd;
    aResult->month = monthInt;
    aResult->tz_negative = (str[6] == '-') ? PR_TRUE : PR_FALSE;
    aResult->tz_hour = (timezoneHour[0] == '\0') ? nsnull : strtol(timezoneHour, &pEnd, 10);
    aResult->tz_minute = (timezoneMinute[0] == '\0') ? nsnull : strtol(timezoneMinute, &pEnd, 10);
  }

  return isValid;
}

/* http://www.w3.org/TR/xmlschema-2/#gYear */
nsresult nsSchemaValidator::ValidateBuiltinTypeGYear(const nsAString & aNodeValue, 
  const nsAString & aMaxExclusive, const nsAString & aMinExclusive,
  const nsAString & aMaxInclusive, const nsAString & aMinInclusive,
  PRBool *aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  PRBool isValidGYear;
  Schema_GYear gyear;
  PRBool isValid = IsValidSchemaGYear(aNodeValue, &gyear);

  if (isValid && !aMaxExclusive.IsEmpty()) {
    Schema_GYear maxExclusive;
    isValidGYear = IsValidSchemaGYear(aMaxExclusive, &maxExclusive);
    
    if (isValidGYear) {
      if (gyear.year >= maxExclusive.year) {
        isValid = PR_FALSE;
#ifdef DEBUG
        printf("\n  Not valid: Value (%ld) is too large", gyear.year);
#endif
      }
    }
  }

  if (isValid && !aMinExclusive.IsEmpty()) {
    Schema_GYear minExclusive;
    isValidGYear = IsValidSchemaGYear(aMinExclusive, &minExclusive);
    
    if (isValidGYear) {
      if (gyear.year <= minExclusive.year) {
        isValid = PR_FALSE;
#ifdef DEBUG
        printf("\n  Not valid: Value (%ld) is too small", gyear.year);
#endif
      }
    }
  }

  if (isValid && !aMaxInclusive.IsEmpty()) {
    Schema_GYear maxInclusive;
    isValidGYear = IsValidSchemaGYear(aMaxInclusive, &maxInclusive);
    
    if (isValidGYear) {
      if (gyear.year > maxInclusive.year) {
        isValid = PR_FALSE;
#ifdef DEBUG
        printf("\n  Not valid: Value (%ld) is too large", gyear.year);
#endif
      }
    }
  }

  if (isValid && !aMinInclusive.IsEmpty()) {
    Schema_GYear minInclusive;
    isValidGYear = IsValidSchemaGYear(aMinInclusive, &minInclusive);
    
    if (isValidGYear) {
      if (gyear.year < minInclusive.year) {
        isValid = PR_FALSE;
#ifdef DEBUG
        printf("\n  Not valid: Value (%ld) is too small", gyear.year);
#endif
      }
    }
  }

#ifdef DEBUG
  if (isValid)
    printf("\n  Value is valid!");
#endif

 *aResult = isValid;
  return NS_OK;
}

PRBool nsSchemaValidator::IsValidSchemaGYear(const nsAString & aNodeValue, Schema_GYear *aResult){
  // (-)CCYY(Z|(+|-)hh:mm)
  PRBool isValid = PR_FALSE;
  
  NS_ConvertUTF16toUTF8 str(aNodeValue);
  int strLength = aNodeValue.Length();

  // we will split the string into the following:
  char year[80] = "";
  char timezoneHour[3] = "";
  char timezoneMinute[3] = "";

  long yearNum;
  PRBool isNegativeYear = (aNodeValue.First() == '-');

  // figure out where the year ends, taking negative years
  // into account.
  int yearend = strcspn(isNegativeYear ? str.get()+1 : str.get(), "Z+-");
  if (isNegativeYear)
    yearend += 1;

  // make sure the year will fit into our variable
  if (yearend >= sizeof(year)) {
    return PR_FALSE;
  }

  // copy and validate the year
  strncpy(year, str.get(), yearend);
  year[sizeof(year)-1] = '\0';

  isValid = nsSchemaValidatorUtils::IsValidSchemaInteger(year, &yearNum);

  // 0 is an invalid year per the spec
  if (isValid && (yearNum == 0))
    isValid = PR_FALSE;

  // only continue if its valid and there is more after the year part
  if (isValid && (yearend != strLength)) {
    // check if 'Z' is last character
    if ((yearend + 1) == strLength){
      if (str.get()[strLength - 1] != 'Z')
        isValid = PR_FALSE;
    } else {
      if ( (strLength - yearend) != 6 ){
        // timezone has to be 6 chars
        isValid = PR_FALSE;
      } else {
        // handle (+|-)hh:mm
        char timezone[6] = "";
        strncpy(timezone, str.get() + (yearend + 1), 5);
        timezone[sizeof(timezone)-1] = '\0';

        isValid = nsSchemaValidatorUtils::ParseSchemaTimeZone(timezone, timezoneHour, timezoneMinute);
      }
    }
  }

  if (isValid && aResult) {
    char * pEnd;
    aResult->year = yearNum;
    aResult->tz_negative = (str.get()[yearend + 1] == '-') ? PR_TRUE : PR_FALSE;
    aResult->tz_hour = (timezoneHour[0] == '\0') ? nsnull : strtol(timezoneHour, &pEnd, 10);
    aResult->tz_minute = (timezoneMinute[0] == '\0') ? nsnull : strtol(timezoneMinute, &pEnd, 10);
  }

  return isValid;
}

/* http://www.w3.org/TR/xmlschema-2/#gYearMonth */
nsresult nsSchemaValidator::ValidateBuiltinTypeGYearMonth(const nsAString & aNodeValue, 
  const nsAString & aMaxExclusive, const nsAString & aMinExclusive,
  const nsAString & aMaxInclusive, const nsAString & aMinInclusive,
  PRBool *aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  PRBool isValidGYearMonth;
  Schema_GYearMonth gyearmonth;
  
  PRBool isValid = IsValidSchemaGYearMonth(aNodeValue, &gyearmonth);

  if (isValid && !aMaxExclusive.IsEmpty()) {
    Schema_GYearMonth maxExclusive;
    isValidGYearMonth = IsValidSchemaGYearMonth(aMaxExclusive, &maxExclusive);

    if (isValidGYearMonth) {
      if (nsSchemaValidatorUtils::CompareGYearMonth(gyearmonth, maxExclusive) > -1) {
        isValid = PR_FALSE;
#ifdef DEBUG
        printf("\n  Not valid: Value (%ld-%d) is too large", gyearmonth.gYear.year,
          gyearmonth.gMonth.month);
#endif
      }
    }
  }

  if (isValid && !aMinExclusive.IsEmpty()) {
    Schema_GYearMonth minExclusive;
    isValidGYearMonth = IsValidSchemaGYearMonth(aMinExclusive, &minExclusive);
    
    if (isValidGYearMonth) {
      if (nsSchemaValidatorUtils::CompareGYearMonth(gyearmonth, minExclusive) < 1) {
        isValid = PR_FALSE;
#ifdef DEBUG
        printf("\n  Not valid: Value (%ld-%d) is too small", gyearmonth.gYear.year,
          gyearmonth.gMonth.month);
#endif
      }
    }
  }

  if (isValid && !aMaxInclusive.IsEmpty()) {
    Schema_GYearMonth maxInclusive;
    isValidGYearMonth = IsValidSchemaGYearMonth(aMaxInclusive, &maxInclusive);
    
    if (isValidGYearMonth) {
      if (nsSchemaValidatorUtils::CompareGYearMonth(gyearmonth, maxInclusive) > 0) {
        isValid = PR_FALSE;
#ifdef DEBUG
        printf("\n  Not valid: Value (%ld-%d) is too large", gyearmonth.gYear.year,
          gyearmonth.gMonth.month);
#endif
      }
    }
  }

  if (isValid && !aMinInclusive.IsEmpty()) {
    Schema_GYearMonth minInclusive;
    isValidGYearMonth = IsValidSchemaGYearMonth(aMinInclusive, &minInclusive);
    
    if (isValidGYearMonth) {
      if (nsSchemaValidatorUtils::CompareGYearMonth(gyearmonth, minInclusive) < 0) {
        isValid = PR_FALSE;
#ifdef DEBUG
        printf("\n  Not valid: Value (%ld-%d) is too small", gyearmonth.gYear.year,
          gyearmonth.gMonth.month);
#endif
      }
    }
  }

#ifdef DEBUG
  if (isValid)
    printf("\n  Value is valid!");
#endif

 *aResult = isValid;
  return NS_OK;
}

PRBool nsSchemaValidator::IsValidSchemaGYearMonth(const nsAString & aNodeValue, Schema_GYearMonth *aYearMonth){
  //(-)CCYY-MM(Z|(+|-)hh:mm)

  NS_ConvertUTF16toUTF8 str(aNodeValue);

  int strLength = aNodeValue.Length();
  PRBool isNegativeYear = (aNodeValue.First() == '-');

  // figure out end of year
  int yearend = strcspn(isNegativeYear ? str.get()+1 : str.get(), "-");
  if (isNegativeYear)
    yearend += 1;

  char gyearStr[80] = "";
  // overflow check for year
  if (yearend >= sizeof(gyearStr)) {
    return PR_FALSE;
  }

  // figure out the length of the month part
  int monthLength = strLength - yearend;

  //   -MMZ                  -MM(+|-)hh:mm
  if ((monthLength != 4) && (monthLength != 9))
    return PR_FALSE;

  strncpy(gyearStr, str.get(), yearend);
  // add 'Z' to make IsValidSchemaGYear work.
  strcat(gyearStr, "Z");
  gyearStr[sizeof(gyearStr)-1] = '\0';

  PRBool isValid = IsValidSchemaGYear(NS_ConvertASCIItoUTF16(gyearStr), 
                                      &aYearMonth->gYear);
  if (isValid) {
    // validate month part
    char gmonthStr[13] = "";

    // --MM--(Z|(+|-)hh:mm)
    strcat(gmonthStr, "--");
    strncpy(&gmonthStr[2], str.get() + (yearend + 1), 2);
    strcat(gmonthStr, "--");
    strncpy(&gmonthStr[6], str.get() + (yearend + 3), (monthLength + 2));
    gmonthStr[sizeof(gmonthStr)-1] = '\0';

    isValid = IsValidSchemaGMonth(NS_ConvertASCIItoUTF16(gmonthStr), 
                &aYearMonth->gMonth);
  }
 
  return isValid;
}

/* http://www.w3.org/TR/xmlschema-2/#gMonthDay */
nsresult nsSchemaValidator::ValidateBuiltinTypeGMonthDay(const nsAString & aNodeValue, 
  const nsAString & aMaxExclusive, const nsAString & aMinExclusive,
  const nsAString & aMaxInclusive, const nsAString & aMinInclusive,
  PRBool *aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  PRBool isValidGMonthDay;
  Schema_GMonthDay gmonthday;

  PRBool isValid = IsValidSchemaGMonthDay(aNodeValue, &gmonthday);

  if (isValid && !aMaxExclusive.IsEmpty()) {
    Schema_GMonthDay maxExclusive;
    isValidGMonthDay = IsValidSchemaGMonthDay(aMaxExclusive, &maxExclusive);

    if (isValidGMonthDay) {
      if (nsSchemaValidatorUtils::CompareGMonthDay(gmonthday, maxExclusive) > -1) {
        isValid = PR_FALSE;
#ifdef DEBUG
        printf("\n  Not valid: Value (%d-%d) is too large", gmonthday.gMonth.month,
          gmonthday.gDay.day);
#endif
      }
    }
  }

  if (isValid && !aMinExclusive.IsEmpty()) {
    Schema_GMonthDay minExclusive;
    isValidGMonthDay = IsValidSchemaGMonthDay(aMinExclusive, &minExclusive);
    
    if (isValidGMonthDay) {
      if (nsSchemaValidatorUtils::CompareGMonthDay(gmonthday, minExclusive) < 1) {
        isValid = PR_FALSE;
#ifdef DEBUG
        printf("\n  Not valid: Value (%d-%d) is too small", gmonthday.gMonth.month,
          gmonthday.gDay.day);
#endif
      }
    }
  }

  if (isValid && !aMaxInclusive.IsEmpty()) {
    Schema_GMonthDay maxInclusive;
    isValidGMonthDay = IsValidSchemaGMonthDay(aMaxInclusive, &maxInclusive);
    
    if (isValidGMonthDay) {
      if (nsSchemaValidatorUtils::CompareGMonthDay(gmonthday, maxInclusive) > 0) {
        isValid = PR_FALSE;
#ifdef DEBUG
        printf("\n  Not valid: Value (%d-%d) is too large", gmonthday.gMonth.month,
          gmonthday.gDay.day);
#endif
      }
    }
  }

  if (isValid && !aMinInclusive.IsEmpty()) {
    Schema_GMonthDay minInclusive;
    isValidGMonthDay = IsValidSchemaGMonthDay(aMinInclusive, &minInclusive);
    
    if (isValidGMonthDay) {
      if (nsSchemaValidatorUtils::CompareGMonthDay(gmonthday, minInclusive) < 0) {
        isValid = PR_FALSE;
#ifdef DEBUG
        printf("\n  Not valid: Value (%d-%d) is too small", gmonthday.gMonth.month,
          gmonthday.gDay.day);
#endif
      }
    }
  }

#ifdef DEBUG
  if (isValid)
    printf("\n  Value is valid!");
#endif

 *aResult = isValid;
  return NS_OK;
}

PRBool nsSchemaValidator::IsValidSchemaGMonthDay(const nsAString & aNodeValue, 
  Schema_GMonthDay *aMonthDay){
  // --MM-DD(Z|(+|-)hh:mm)

  int strLength = aNodeValue.Length();

  //   --MM-DD             --MM-DDZ            --MM-DD(+|-)hh:mm
  if ((strLength != 7) && (strLength != 8) && (strLength != 13))
    return PR_FALSE;

  PRBool isValid = PR_FALSE;

  // parsed day/month go into these:
  char gdayStr[12] = "";
  char gmonthStr[7] = "";
  
  NS_ConvertUTF16toUTF8 str(aNodeValue);

  // gMonth is --MM--
  strncpy(gmonthStr, str.get(), 4);
  strcat(gmonthStr, "--");
  gmonthStr[sizeof(gmonthStr)-1] = '\0';

  isValid = IsValidSchemaGMonth(NS_ConvertASCIItoUTF16(gmonthStr), &aMonthDay->gMonth);

  if (isValid) {
    // validate day part ---DD(timezone)
    strcat(gdayStr, "---");
    strncpy(&gdayStr[3], str.get()+5, 2);

    // is there a timezone?
    if (strLength > 7) {
      if (strLength == 8) {
        // timezone is one character only
        strcat(gdayStr, str.get()+7);
      } else {
        // timezone should be (+|-)hh:mm
        strncpy(&gdayStr[5], str.get()+7, 6);
      }
    }

    gdayStr[sizeof(gdayStr)-1] = '\0';
    isValid = IsValidSchemaGDay(NS_ConvertASCIItoUTF16(gdayStr), &aMonthDay->gDay);
  }
 
  return isValid;
}

/* http://www.w3.org/TR/xmlschema-2/#time */
nsresult nsSchemaValidator::ValidateBuiltinTypeTime(const nsAString & aNodeValue,
  const nsAString & aMaxExclusive, const nsAString & aMinExclusive,
  const nsAString & aMaxInclusive, const nsAString & aMinInclusive,
  PRBool *aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  PRBool isValid = PR_FALSE;
  PRBool isValidTime;
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

    isValidTime = IsValidSchemaTime(aMinExclusive, &minExclusive);
    if (isValidTime) {
      PR_ExplodeTime(minExclusive, PR_GMTParameters, &minExclusiveExploded);

      timeCompare = 
        nsSchemaValidatorUtils::CompareExplodedTime(explodedTime, minExclusiveExploded);

      if (timeCompare < 1) {
        isValid = PR_FALSE;
#ifdef DEBUG
        printf("\n  Not valid: Value is too small");
#endif
      }
    }
  }

  if (isValid && !aMaxExclusive.IsEmpty()) {
    PRTime maxExclusive;
    PRExplodedTime maxExclusiveExploded;

    isValidTime = IsValidSchemaTime(aMaxExclusive, &maxExclusive);
    if (isValidTime) {
      PR_ExplodeTime(maxExclusive, PR_GMTParameters, &maxExclusiveExploded);

      timeCompare = 
        nsSchemaValidatorUtils::CompareExplodedTime(explodedTime, maxExclusiveExploded);

      if (timeCompare > -1) {
        isValid = PR_FALSE;
#ifdef DEBUG
        printf("\n  Not valid: Value is too large");
#endif
      }
    }
  }

  if (isValid && !aMaxInclusive.IsEmpty()) {
    PRTime maxInclusive;
    PRExplodedTime maxInclusiveExploded;

    isValidTime = IsValidSchemaTime(aMaxInclusive, &maxInclusive);
    if (isValidTime) {
      PR_ExplodeTime(maxInclusive, PR_GMTParameters, &maxInclusiveExploded);

      timeCompare = 
        nsSchemaValidatorUtils::CompareExplodedTime(explodedTime, maxInclusiveExploded);

      if (timeCompare > 0) {
        isValid = PR_FALSE;
#ifdef DEBUG
        printf("\n  Not valid: Value is too large");
#endif
      }
    }
  }

  if (isValid && !aMinInclusive.IsEmpty()) {
    PRTime minInclusive;
    PRExplodedTime minInclusiveExploded;

    isValidTime = IsValidSchemaTime(aMinInclusive, &minInclusive);
    if (isValidTime) {
      PR_ExplodeTime(minInclusive, PR_GMTParameters, &minInclusiveExploded);

      timeCompare = 
        nsSchemaValidatorUtils::CompareExplodedTime(explodedTime, minInclusiveExploded);

      if (timeCompare < 0) {
        isValid = PR_FALSE;
#ifdef DEBUG
        printf("\n  Not valid: Value is too small");
#endif
      }
    }
  }

 *aResult = isValid;
  return NS_OK;
}

PRBool nsSchemaValidator::IsValidSchemaTime(const nsAString & aNodeValue,
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

  // if no timezone ([+/-]hh:ss), add a Z to the end so that 
  // nsSchemaValidatorUtils::ParseSchemaTime can parse it
  if ( (timeString.FindChar('-') == kNotFound) && (timeString.FindChar('+') == kNotFound) ){
    timeString.Append('Z');
  }

#ifdef DEBUG
  printf("\n  Validating Time :");
#endif

  NS_ConvertUTF16toUTF8 strValue(timeString);
  isValid = nsSchemaValidatorUtils::ParseSchemaTime(strValue.get(), hour, minute, 
              second, fraction_seconds);

  if (isValid && aResult) {
    // generate a string nspr can handle
    // for example: 22-AUG-1993 10:59:12.82
    sprintf(fulldate, "22-AUG-1993 %s:%s:%s", hour, minute, second);

    if (strlen(fraction_seconds) > 0) {
      strcat(fulldate, ".");
      strcat(fulldate, fraction_seconds);
    }

#ifdef DEBUG
    printf("\n    new date is %s", fulldate);
#endif

    PRStatus status = PR_ParseTimeString(fulldate,
                        PR_TRUE, &dateTime);  
    if (status == -1)
      isValid = PR_FALSE;
    else
      *aResult = dateTime;
  }

  return isValid;
}

/* http://www.w3.org/TR/xmlschema-2/#date */
nsresult nsSchemaValidator::ValidateBuiltinTypeDate(const nsAString & aNodeValue,
  const nsAString & aMaxExclusive, const nsAString & aMinExclusive,
  const nsAString & aMaxInclusive, const nsAString & aMinInclusive,
  PRBool *aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  PRBool isValid = PR_FALSE;
  PRBool isValidDate;
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

    isValidDate = IsValidSchemaDate(aMaxExclusive, &maxExclusive);
    if (isValidDate) {
      PR_ExplodeTime(maxExclusive, PR_GMTParameters, &maxExclusiveExploded);

      dateCompare = 
        nsSchemaValidatorUtils::CompareExplodedDate(
          explodedDate, maxExclusiveExploded);

      if (dateCompare > -1) {
        isValid = PR_FALSE;
#ifdef DEBUG
        printf("\n  Not valid: Value is too large");
#endif
      }
    }
  }

  if (isValid && !aMinExclusive.IsEmpty()) {
    PRTime minExclusive;
    PRExplodedTime minExclusiveExploded;

    isValidDate = IsValidSchemaDate(aMinExclusive, &minExclusive);
    if (isValidDate) {
      PR_ExplodeTime(minExclusive, PR_GMTParameters, &minExclusiveExploded);

      dateCompare = 
        nsSchemaValidatorUtils ::CompareExplodedDate(
          explodedDate, minExclusiveExploded);

      if (dateCompare < 1) {
        isValid = PR_FALSE;
#ifdef DEBUG
        printf("\n  Not valid: Value is too small");
#endif
      }
    }
  }

  if (isValid && !aMaxInclusive.IsEmpty()) {
    PRTime maxInclusive;
    PRExplodedTime maxInclusiveExploded;

    isValidDate = IsValidSchemaDate(aMaxInclusive, &maxInclusive);
    if (isValidDate) {
      PR_ExplodeTime(maxInclusive, PR_GMTParameters, &maxInclusiveExploded);

      dateCompare = 
        nsSchemaValidatorUtils::CompareExplodedDate(
          explodedDate, maxInclusiveExploded);

      if (dateCompare > 0) {
        isValid = PR_FALSE;
#ifdef DEBUG
        printf("\n  Not valid: Value is too large");
#endif
      }
    }
  }

  if (isValid && !aMinInclusive.IsEmpty()) {
    PRTime minInclusive;
    PRExplodedTime minInclusiveExploded;

    isValidDate = IsValidSchemaDate(aMinInclusive, &minInclusive);
    if (isValidDate) {
      PR_ExplodeTime(minInclusive, PR_GMTParameters, &minInclusiveExploded);

      dateCompare = 
        nsSchemaValidatorUtils::CompareExplodedDate(
          explodedDate, minInclusiveExploded);

      if (dateCompare < 0) {
        isValid = PR_FALSE;
#ifdef DEBUG
        printf("\n  Not valid: Value is too small");
#endif
      }
    }
  }

 *aResult = isValid;
  return NS_OK;
}

/* http://www.w3.org/TR/xmlschema-2/#dateTime */
nsresult nsSchemaValidator::ValidateBuiltinTypeDateTime(const nsAString & aNodeValue,
  const nsAString & aMaxExclusive, const nsAString & aMinExclusive,
  const nsAString & aMaxInclusive, const nsAString & aMinInclusive,
  PRBool *aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  PRBool isValid = PR_FALSE;
  PRBool isValidDateTime;
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

    isValidDateTime = IsValidSchemaDateTime(aMaxExclusive, &maxExclusive);
    if (isValidDateTime) {
      int dateTimeCompare = CompareSchemaDateTime(
        explodedDateTime, 
        isDateTimeNegative,
        maxExclusive,
        (aMaxExclusive.First() == PRUnichar('-')) ? PR_TRUE : PR_FALSE
      );

      if (dateTimeCompare > -1) {
        isValid = PR_FALSE;
#ifdef DEBUG
        printf("\n  Not valid: Value is too large");
#endif
      }
    }
  }

  if (isValid && !aMinExclusive.IsEmpty()) {
    PRTime minExclusive;

    isValidDateTime = IsValidSchemaDateTime(aMinExclusive, &minExclusive);
    if (isValidDateTime) {
      int dateTimeCompare = CompareSchemaDateTime(
        explodedDateTime, 
        isDateTimeNegative,
        minExclusive,
        (aMinExclusive.First() == PRUnichar('-')) ? PR_TRUE : PR_FALSE
      );

      if (dateTimeCompare < 1) {
        isValid = PR_FALSE;
#ifdef DEBUG
        printf("\n  Not valid: Value is too small");
#endif
      }
    }
  }

  if (isValid && !aMaxInclusive.IsEmpty()) {
    PRTime maxInclusive;

    isValidDateTime = IsValidSchemaDateTime(aMaxInclusive, &maxInclusive);
    if (isValidDateTime) {
      int dateTimeCompare = CompareSchemaDateTime(
        explodedDateTime, 
        isDateTimeNegative,
        maxInclusive,
        (aMaxInclusive.First() == PRUnichar('-')) ? PR_TRUE : PR_FALSE
      );

      if (dateTimeCompare > 0) {
        isValid = PR_FALSE;
#ifdef DEBUG
        printf("\n  Not valid: Value is too large");
#endif
      }
    }
  }

  if (isValid && !aMinInclusive.IsEmpty()) {
    PRTime minInclusive;

    isValidDateTime = IsValidSchemaDateTime(aMinInclusive, &minInclusive);
    if (isValidDateTime) {
      int dateTimeCompare = CompareSchemaDateTime(
        explodedDateTime, 
        isDateTimeNegative,
        minInclusive,
        (aMinInclusive.First() == PRUnichar('-')) ? PR_TRUE : PR_FALSE
      );

      if (dateTimeCompare < 0) {
        isValid = PR_FALSE;
#ifdef DEBUG
        printf("\n  Not valid: Value is too small");
#endif
      }
    }
  }

 *aResult = isValid;
  return NS_OK;
}

int nsSchemaValidator::CompareSchemaDateTime(
  PRExplodedTime datetime1,
  PRBool isDateTime1Negative,
  PRTime datetime2,
  PRBool isDateTime2Negative)
{
  PRExplodedTime datetime2Exploded;
  PR_ExplodeTime(datetime2, PR_GMTParameters, &datetime2Exploded);

  int dateTimeCompare = 
    nsSchemaValidatorUtils::CompareExplodedDateTime(
      datetime1,
      isDateTime1Negative,
      datetime2Exploded,
      isDateTime2Negative
    );
  
  return dateTimeCompare;
}

PRBool nsSchemaValidator::IsValidSchemaDate(const nsAString & aNodeValue,
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

#ifdef DEBUG
  printf("\n  Validating Date:");
#endif

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

  NS_ConvertUTF16toUTF8 strValue(dateString);

  isValid = nsSchemaValidatorUtils::ParseSchemaDate(strValue.get(), year, month, day);

  if (isValid && aResult) {
    // 22-AUG-1993

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

PRBool nsSchemaValidator::IsValidSchemaDateTime(const nsAString & aNodeValue,
  PRTime *aResult)
{
  PRBool isValid = PR_FALSE;
  PRBool isNegativeYear = PR_FALSE;
  
  int run = 0;
  PRBool doneParsing = PR_FALSE;

  char year[80] = "";
  char month[3] = "";
  char day[3] = "";
  char hour[3] = "";
  char minute[3] = "";
  char second[3] = "";
  char fraction_seconds[80] = "";
  PRTime dateTime;

  char fulldate[100] = "";
  char date[80] = "";
  char time[80] = "";

  nsAutoString datetimeString(aNodeValue); 
  
  if (datetimeString.First() == '-') {
    isNegativeYear = PR_TRUE;
    run = 1;
    /* nspr can't handle negative years it seems.*/
 }
  /*
    http://www.w3.org/TR/xmlschema-2/#dateTime
    (-)CCYY-MM-DDThh:mm:ss(.sss...)
      then either: Z
      or [+/-]hh:mm
  */

  // first handle the date part
  // search for 'T'

  printf("\n  Validating DateTime:");
  
  int findString = datetimeString.FindChar('T');
  NS_ConvertUTF16toUTF8 strValue(datetimeString);
  
  if (findString >= 0) {
    strncpy(date, strValue.get(), (findString + 1));
    date[sizeof(date)-1] = '\0';
    isValid = nsSchemaValidatorUtils::ParseSchemaDate(date, year, month, day);

    if (isValid) {
      const char * pch;
      pch = strchr(strValue.get(),'T');
      pch++;
      strncat(time, pch, (strValue.Length() - findString - 1));
      time[sizeof(time)-1] = '\0';
      isValid = nsSchemaValidatorUtils::ParseSchemaTime(time, hour, minute, second, fraction_seconds);
    }
  } else {
    // no T, invalid
    doneParsing = PR_TRUE;
  }

  if (isValid && aResult) {
    nsCAutoString monthShorthand;
    nsSchemaValidatorUtils::GetMonthShorthand(month, monthShorthand);

    // 22-AUG-1993 10:59:12.82
    sprintf(fulldate, "%s-%s-%s %s:%s:%s",
      day,
      monthShorthand.get(),
      year,
      hour,
      minute,
      second);

    printf("\n    new date is %s", fulldate);
    PRStatus status = PR_ParseTimeString(fulldate, 
                                      PR_TRUE, &dateTime);  
    if (status == -1)
      isValid = PR_FALSE;
    else
      *aResult = dateTime;
  }

  return isValid;
}

/* http://w3.org/TR/xmlschema-2/#integer */
nsresult nsSchemaValidator::ValidateBuiltinTypeInteger(
  const nsAString & aNodeValue, PRUint32 aTotalDigits, 
  const nsAString & aMaxExclusive, const nsAString & aMinExclusive, 
  const nsAString & aMaxInclusive, const nsAString & aMinInclusive, 
  PRBool *aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  PRBool isValid = PR_FALSE;
  PRBool isValidInteger;
  
  long intValue;
  int strrv;
  isValid = nsSchemaValidatorUtils::IsValidSchemaInteger(aNodeValue, &intValue);

  if (aTotalDigits) {
    PRUint32 length = aNodeValue.Length();
    
    if (aNodeValue.First() == PRUnichar('-'))
      length -= 1;
    
    if (length > aTotalDigits) {
      isValid = PR_FALSE;
#ifdef DEBUG
  printf("\n  Not valid: Too many digits (%d)", length);
#endif
    }
  }

  if (isValid && !aMaxExclusive.IsEmpty()){
    long maxExclusive;
    isValidInteger = nsSchemaValidatorUtils::IsValidSchemaInteger(aMaxExclusive, &maxExclusive);

    if (isValidInteger) {
      strrv = CompareStrings(aNodeValue, aMaxExclusive);

      if (strrv >= 0)
        isValid = PR_FALSE;
    }
  }

  if (isValid && !aMinExclusive.IsEmpty()){
    long minExclusive;
    isValidInteger = nsSchemaValidatorUtils::IsValidSchemaInteger(aMinExclusive, &minExclusive);

    if (isValidInteger) {
      strrv = CompareStrings(aNodeValue, aMinExclusive);

      if (strrv <= 0)
        isValid = PR_FALSE;
    }
  }

  if (isValid && !aMaxInclusive.IsEmpty()){
    long maxInclusive;
    isValidInteger = nsSchemaValidatorUtils::IsValidSchemaInteger(aMaxInclusive, &maxInclusive);

    if (isValidInteger) {
      strrv = CompareStrings(aNodeValue, aMaxInclusive);

      if (strrv > 0)
        isValid = PR_FALSE;
    }
  }

  if (isValid && !aMinInclusive.IsEmpty()){
    long minInclusive;
    isValidInteger = nsSchemaValidatorUtils::IsValidSchemaInteger(aMinInclusive, &minInclusive);

    if (isValidInteger) {
      strrv = CompareStrings(aNodeValue, aMinInclusive);

      if (strrv < 0)
        isValid = PR_FALSE;
    }
  }

 *aResult = isValid;
  return NS_OK;
}

nsresult nsSchemaValidator::ValidateBuiltinTypeByte(const nsAString & aNodeValue, 
  PRUint32 aTotalDigits, const nsAString & aMaxExclusive, const nsAString & aMinExclusive,
  const nsAString & aMaxInclusive, const nsAString & aMinInclusive,
  PRBool *aResult)
{
  PRBool isValid = PR_FALSE;
  PRBool isValidByte;
  
  long intValue;
  isValid = IsValidSchemaByte(aNodeValue, &intValue);

  if (aTotalDigits) {
    PRUint32 length = aNodeValue.Length();
    
    if (aNodeValue.First() == PRUnichar('-'))
      length -= 1;
    
    if (length > aTotalDigits) {
      isValid = PR_FALSE;
#ifdef DEBUG
  printf("\n  Not valid: Too many digits (%d)", length);
#endif
    }
  }

  if (isValid && !aMaxExclusive.IsEmpty()){
    long maxExclusive;
    isValidByte = IsValidSchemaByte(aMaxExclusive, &maxExclusive);

    if (isValidByte && (intValue >= maxExclusive)) {
      isValid = PR_FALSE;
#ifdef DEBUG
      printf("\n  Not valid: Value is too large");
#endif
    }
  }

  if (isValid && !aMinExclusive.IsEmpty()){
    long minExclusive;
    isValidByte = IsValidSchemaByte(aMinExclusive, &minExclusive);

    if (isValidByte && (intValue <= minExclusive)) {
      isValid = PR_FALSE;
#ifdef DEBUG
      printf("\n  Not valid: Value is too small");
#endif
    }
  }

  if (isValid && !aMaxInclusive.IsEmpty()){
    long maxInclusive;
    isValidByte = IsValidSchemaByte(aMaxInclusive, &maxInclusive);

    if (isValidByte && (intValue > maxInclusive)) {
      isValid = PR_FALSE;
#ifdef DEBUG
      printf("\n  Not valid: Value is too large");
#endif
    }
  }

  if (isValid && !aMinInclusive.IsEmpty()){
    long minInclusive;
    isValidByte = IsValidSchemaByte(aMinInclusive, &minInclusive);

    if (isValidByte && (intValue < minInclusive)) {
      isValid = PR_FALSE;
#ifdef DEBUG
      printf("\n  Not valid: Value is too small");
#endif
    }
  }

 *aResult = isValid;
  return NS_OK;
}

/* http://w3.org/TR/xmlschema-2/#byte */
PRBool nsSchemaValidator::IsValidSchemaByte(const nsAString & aNodeValue, long *aResult){
  PRBool isValid = PR_FALSE;
  long byteValue;

  isValid = nsSchemaValidatorUtils::IsValidSchemaInteger(aNodeValue, &byteValue);
  
  if (isValid) {
    if ((byteValue > 127) || (byteValue < -128))
      isValid = PR_FALSE;
  }

  if (aResult)
    *aResult = byteValue;
  return isValid;
}

/* compares 2 strings that contain integers.
   Schema Integers have no limit, thus converting the strings
   into numbers won't work.

 -1 - aString1 < aString2
  0 - equal
  1 - aString1 > aString2

 */
int nsSchemaValidator::CompareStrings(const nsAString & aString1, const nsAString & aString2){
  int rv;

  PRBool isNegative1 = (aString1.First() == PRUnichar('-'));
  PRBool isNegative2 = (aString2.First() == PRUnichar('-'));

  if (isNegative1 && !isNegative2) {
    // negative is always smaller than positive
    return -1;
  } else if (!isNegative1 && isNegative2) {
    // positive is always bigger than negative
    return 1;
  }

  nsAutoString compareString1(aString1);
  nsAutoString compareString2(aString2);

  // remove negative sign
  if (isNegative1)
    compareString1.Cut(0,1);

  if (isNegative2)
    compareString2.Cut(0,1);

  // strip leading 0s
  nsSchemaValidatorUtils::RemoveLeadingZeros(compareString1);
  nsSchemaValidatorUtils::RemoveLeadingZeros(compareString2);

  // after removing leading 0s, check if they are the same
  if (compareString1.Equals(compareString2)) {
    return 0;
  }

  if (compareString1.Length() > compareString2.Length())
    rv = 1;
  else if (compareString1.Length() < compareString2.Length())
    rv = -1;
  else
    rv = strcmp(NS_ConvertUTF16toUTF8(compareString1).get(), NS_ConvertUTF16toUTF8(compareString2).get());

  // 3>2, but -2>-3
  if (isNegative1 && isNegative2) {
    if (rv == 1)
      rv = -1;
    else
      rv = 1;
  }

  return rv;
}

/* http://www.w3.org/TR/xmlschema-2/#float */
nsresult nsSchemaValidator::ValidateBuiltinTypeFloat(const nsAString & aNodeValue, 
  PRUint32 aTotalDigits, const nsAString & aMaxExclusive, const nsAString & aMinExclusive,
  const nsAString & aMaxInclusive, const nsAString & aMinInclusive,
  PRBool *aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  PRBool isValid = PR_FALSE;
  PRBool isValidFloat;
  
  float floatValue;
  isValid = IsValidSchemaFloat(aNodeValue, &floatValue);

  if (isValid && !aMaxExclusive.IsEmpty()){
    float maxExclusive;
    isValidFloat = IsValidSchemaFloat(aMaxExclusive, &maxExclusive);

    if (isValidFloat) {
      if (floatValue >= maxExclusive) {
        isValid = PR_FALSE;
#ifdef DEBUG
        printf("\n  Not valid: Value (%f) is too big", floatValue);
#endif
      }
    }
  }

  if (isValid && !aMinExclusive.IsEmpty()){
    float minExclusive;
    isValidFloat = IsValidSchemaFloat(aMinExclusive, &minExclusive);

    if (isValidFloat) {
      if (floatValue <= minExclusive) {
        isValid = PR_FALSE;
#ifdef DEBUG
        printf("\n  Not valid: Value (%f) is too small", floatValue);
#endif
      }
    }
  }

  if (isValid && !aMaxInclusive.IsEmpty()){
    float maxInclusive;
    isValidFloat = IsValidSchemaFloat(aMaxInclusive, &maxInclusive);

    if (isValidFloat) {
      if (floatValue > maxInclusive) {
        isValid = PR_FALSE;
#ifdef DEBUG
        printf("\n  Not valid: Value (%f) is too big", floatValue);
#endif
      }
    }
  }

  if (isValid && !aMinInclusive.IsEmpty()){
    float minInclusive;
    isValidFloat = IsValidSchemaFloat(aMinInclusive, &minInclusive);

    if (isValidFloat) {
      if (floatValue < minInclusive) {
        isValid = PR_FALSE;
#ifdef DEBUG
        printf("\n  Not valid: Value (%f) is too small", floatValue);
#endif
      }
    }
  }

#ifdef DEBUG
  if (isValid)
    printf("\n  Value is valid!");
#endif

 *aResult = isValid;
  return NS_OK;
}

PRBool nsSchemaValidator::IsValidSchemaFloat(const nsAString & aNodeValue, float *aResult){
  PRBool isValid = PR_FALSE;
  NS_ConvertUTF16toUTF8 temp(aNodeValue);
  char * pEnd;
  float floatValue = strtod(temp.get(), &pEnd);

  if (*pEnd == '\0')
    isValid = PR_TRUE;

  // convert back to string and compare
  char floatStr[50];
  PR_snprintf(floatStr, sizeof(floatStr), "%f", floatValue);
  
  if (strcmp(temp.get(), floatStr) == 0)
    isValid = PR_FALSE;

#ifdef DEBUG
  if (!isValid)
    printf("\n  Not valid float: %f", floatValue);
#endif

  if (aResult)
    *aResult = floatValue;
  return isValid;
}

/* http://www.w3.org/TR/xmlschema-2/#decimal */
nsresult nsSchemaValidator::ValidateBuiltinTypeDecimal(const nsAString & aNodeValue, 
  PRUint32 aTotalDigits, const nsAString & aMaxExclusive, const nsAString & aMinExclusive,
  const nsAString & aMaxInclusive, const nsAString & aMinInclusive,
  PRBool *aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  PRBool isValid = PR_FALSE;
  PRBool isValidDecimal;
  int strcmp;

  nsAutoString wholePart;
  nsAutoString wholePartFacet;
  nsAutoString fractionPart;
  nsAutoString fractionPartFacet;

  isValid = IsValidSchemaDecimal(aNodeValue, wholePart, fractionPart);

  if (isValid && !aMaxExclusive.IsEmpty()){
    isValidDecimal = IsValidSchemaDecimal(aMaxExclusive, wholePartFacet, fractionPartFacet);

    if (isValidDecimal) {
      strcmp = CompareStrings(wholePart, wholePartFacet);
      if (strcmp > 0) {
        isValid = PR_FALSE;
      } else if (strcmp == 0) {
        // if equal check the fraction part
        strcmp = CompareFractionStrings(fractionPart, fractionPartFacet);
        if (strcmp >= 0)
          isValid = PR_FALSE;
      }

#ifdef DEBUG
      if (!isValid)
        printf("\n  Not valid: Value is too big");
#endif
    }
  }

  if (isValid && !aMinExclusive.IsEmpty()){
    isValidDecimal = IsValidSchemaDecimal(aMinExclusive, wholePartFacet, fractionPartFacet);

    if (isValidDecimal) {
      strcmp = CompareStrings(wholePart, wholePartFacet);
      if (strcmp < 0) {
        isValid = PR_FALSE;
      } else if (strcmp == 0) {
        // if equal check the fraction part
        strcmp = CompareFractionStrings(fractionPart, fractionPartFacet);
        if (strcmp <= 0)
          isValid = PR_FALSE;
      }

#ifdef DEBUG
      if (!isValid)
        printf("\n  Not valid: Value is too small");
#endif
    }
  }

 if (isValid && !aMaxInclusive.IsEmpty()){
    isValidDecimal = IsValidSchemaDecimal(aMaxInclusive, wholePartFacet, fractionPartFacet);

    if (isValidDecimal) {
      strcmp = CompareStrings(wholePart, wholePartFacet);
      if (strcmp > 0) {
        isValid = PR_FALSE;
      } else if (strcmp == 0) {
        // if equal check the fraction part
        strcmp = CompareFractionStrings(fractionPart, fractionPartFacet);
        if (strcmp > 0)
          isValid = PR_FALSE;
      }

#ifdef DEBUG
      if (!isValid)
        printf("\n  Not valid: Value is too big");
#endif
    }
  }

  if (isValid && !aMinInclusive.IsEmpty()){
    isValidDecimal = IsValidSchemaDecimal(aMinInclusive, wholePartFacet, fractionPartFacet);

    if (isValidDecimal) {
      strcmp = CompareStrings(wholePart, wholePartFacet);
      if (strcmp < 0) {
        isValid = PR_FALSE;
      } else if (strcmp == 0) {
        // if equal check the fraction part
        strcmp = CompareFractionStrings(fractionPart, fractionPartFacet);
        if (strcmp < 0)
          isValid = PR_FALSE;
      }

#ifdef DEBUG
      if (!isValid)
        printf("\n  Not valid: Value is too small");
#endif
    }
  }

#ifdef DEBUG
  if (isValid)
    printf("\n  Value is valid!");
#endif

 *aResult = isValid;
  return NS_OK;
}

PRBool nsSchemaValidator::IsValidSchemaDecimal(const nsAString & aNodeValue, 
  nsAString & aWholePart, nsAString & aFractionPart)
{
  PRBool isValid = PR_FALSE;

  int findString = aNodeValue.FindChar('.');

  if (findString == kNotFound) {
    aWholePart.Assign(aNodeValue);
  } else {
    aWholePart = Substring(aNodeValue, 0, findString);
    aFractionPart = Substring(aNodeValue, findString+1, aNodeValue.Length() - findString - 1);
  }

#ifdef DEBUG
  printf("\n    whole part: %s  fraction part: %s",
          NS_ConvertUTF16toUTF8(aWholePart).get(),
          NS_ConvertUTF16toUTF8(aFractionPart).get());
#endif

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
int nsSchemaValidator::CompareFractionStrings(const nsAString & aString1, const nsAString & aString2){
  int rv;

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
    rv = -1;
  } else if (done) {
    // *start1 is != *start2
    if (*start1 < *start2)
      rv = 1;
    else
      rv = -1;
  }

  return rv;
}

/* http://w3c.org/TR/xmlschema-2/#anyURI */
nsresult nsSchemaValidator::ValidateBuiltinTypeAnyURI(
  const nsAString & aNodeValue, PRUint32 aLength, PRUint32 aMinLength, 
  PRUint32 aMaxLength, PRBool *aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  PRBool isValid = PR_FALSE;
  PRUint32 length = aNodeValue.Length();

  isValid = IsValidSchemaAnyURI(aNodeValue);

  if (isValid && aLength && (length != aLength)) {  
    isValid = PR_FALSE;
#ifdef DEBUG
    printf("\n  Not valid: Not the right length (%d)", length);
#endif
  }

  if (isValid && aMinLength && (length < aMinLength)) {
    isValid = PR_FALSE;
#ifdef DEBUG
    printf("\n  Not valid: Length (%d) is too small", length);
#endif
  }

  if (isValid && aMaxLength && (length > aMaxLength)) {
    isValid = PR_FALSE;
#ifdef DEBUG
    printf("\n  Not valid: Length (%d) is too large", length);
#endif  
  }

#ifdef DEBUG
  if (isValid)
    printf("\n  Value is valid!");
#endif

  *aResult = isValid;
  return NS_OK;
}

PRBool nsSchemaValidator::IsValidSchemaAnyURI(const nsAString & aString)
{
  PRBool isValid = PR_FALSE;
  nsresult rv;

  nsCOMPtr<nsIIOService> ioService = do_GetIOService();
  if (ioService) {
    nsCOMPtr<nsIURI> uri;
    rv = ioService->NewURI(NS_ConvertUTF16toUTF8(aString), nsnull, nsnull, getter_AddRefs(uri));
    
    if (rv == NS_OK)
      isValid = PR_TRUE;
  }
  
  return isValid;
}

// http://w3c.org/TR/xmlschema-2/#base64Binary
nsresult nsSchemaValidator::ValidateBuiltinTypeBase64Binary(
  const nsAString & aNodeValue, PRUint32 aLength, PRUint32 aMinLength, 
  PRUint32 aMaxLength, PRBool *aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  PRBool isValid = PR_FALSE;
  PRUint32 length;

  char* decodedString;
  isValid = IsValidSchemaBase64Binary(aNodeValue, &decodedString);
  length = strlen(decodedString);

  if (isValid) {
    length = strlen(decodedString);
  }

  if (isValid && aLength && (length != aLength)) {  
    isValid = PR_FALSE;
#ifdef DEBUG
    printf("\n  Not valid: Not the right length (%d)", length);
#endif
  }

  if (isValid && aMinLength && (length < aMinLength)) {
    isValid = PR_FALSE;
#ifdef DEBUG
    printf("\n  Not valid: Length (%d) is too small", length);
#endif
  }

  if (isValid && aMaxLength && (length > aMaxLength)) {
    isValid = PR_FALSE;
#ifdef DEBUG
    printf("\n  Not valid: Length (%d) is too large", length);
#endif  
  }

#ifdef DEBUG
  if (isValid)
    printf("\n  Value is valid!");
#endif

  nsMemory::Free(decodedString);
  *aResult = isValid;
  return NS_OK;
}

PRBool nsSchemaValidator::IsValidSchemaBase64Binary(const nsAString & aString, char** aDecodedString)
{
  PRBool isValid = PR_FALSE;

  *aDecodedString = PL_Base64Decode(NS_ConvertUTF16toUTF8(aString).get(),
                                     aString.Length(), nsnull);
  if (*aDecodedString)
    isValid = PR_TRUE;

  return isValid;
}

void nsSchemaValidator::DumpBaseType(nsISchemaBuiltinType *aBuiltInType)
{
  nsAutoString typeName;
  aBuiltInType->GetName(typeName);
  PRUint16 foo;
  aBuiltInType->GetBuiltinType(&foo);

#ifdef DEBUG
  printf("\n  Base Type is %s (%d)", NS_ConvertUTF16toUTF8(typeName).get(),foo);
#endif
}


/*
  // check if its in the valid format
 // nsCOMPtr<nsISchemaValidatorRegexp> regexp = do_GetService(kREGEXP_CID);
//  regexp->RunRegexp(aNodeValue,
  //                  NS_LITERAL_STRING("[+\-]?[0-9]{4,}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}"),
  //                  "g", &isValid);
*/

/*

NS_IMETHODIMP nsSchemaValidator::IsLong(const nsAString & nodeValue, PRBool *aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  nsresult isValid = PR_FALSE;

  NS_ConvertUTF16toUTF8 temp(nodeValue);
  char * pEnd;
  long longValue = strtol(temp.get(), &pEnd, 10);

  if (*pEnd == '\0')
    isValid = PR_TRUE;

  if (isValid) {
    PRUint32 length = nodeValue.Length();
    if (length >= 13) {
      if ((strcmp(temp.get(),"9223372036854775807") > 0) || (strcmp(temp.get(),"-9223372036854775808") > 0))
        isValid = PR_FALSE;
    }
  }

  *aResult = isValid;
  return NS_OK;
}

NS_IMETHODIMP nsSchemaValidator::IsInt(const nsAString & nodeValue, PRBool *aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  nsresult isValid = PR_FALSE;

  NS_ConvertUTF16toUTF8 temp(nodeValue);
  char * pEnd;
  long intValue = strtol(temp.get(), &pEnd, 10);
*/
  /*XXX -2147483648 is the actuall mininclusive, but as jjones says, won't work.
   1000 0000
   0111 1111 1's complement
   1000 0000 2's complement - back to the original number */
/*
  if ((*pEnd == '\0') && (intValue <= 2147483647) && (intValue >= -2147483647))
    isValid = PR_TRUE;

  *aResult = isValid;
  return NS_OK;
}

NS_IMETHODIMP nsSchemaValidator::IsShort(const nsAString & nodeValue, PRBool *aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  nsresult isValid = PR_FALSE;

  NS_ConvertUTF16toUTF8 temp(nodeValue);
  char * pEnd;
  long shortValue = strtol(temp.get(), &pEnd, 10);

  if ((*pEnd == '\0') && (shortValue <= 32767) && (shortValue >= -32768))
    isValid = PR_TRUE;

  *aResult = isValid;
  return NS_OK;
}

NS_IMETHODIMP nsSchemaValidator::IsByte(const nsAString & nodeValue, PRBool *aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  nsresult isValid = PR_FALSE;

  NS_ConvertUTF16toUTF8 temp(nodeValue);
  char * pEnd;
  short byteValue = strtol(temp.get(), &pEnd, 10);

  if ((*pEnd == '\0') && (byteValue <= 127) && (byteValue >= -128))
    isValid = PR_TRUE;

  *aResult = isValid;
  return NS_OK;
}
*/
