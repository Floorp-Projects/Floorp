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
 * The Original Code is Mozilla.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vidur Apparao <vidur@netscape.com> (original author)
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

#include "nsSchemaPrivate.h"

////////////////////////////////////////////////////////////
//
// nsSchemaBuiltinType implementation
//
////////////////////////////////////////////////////////////
nsSchemaBuiltinType::nsSchemaBuiltinType(PRUint16 aBuiltinType)
  : mBuiltinType(aBuiltinType)
{
}

nsSchemaBuiltinType::~nsSchemaBuiltinType()
{
}

NS_IMPL_ISUPPORTS4_CI(nsSchemaBuiltinType, 
                      nsISchemaComponent,
                      nsISchemaType,
                      nsISchemaSimpleType,
                      nsISchemaBuiltinType)

/* readonly attribute wstring targetNamespace; */
NS_IMETHODIMP
nsSchemaBuiltinType::GetTargetNamespace(nsAString& aTargetNamespace)
{
  aTargetNamespace.AssignLiteral(NS_SCHEMA_2001_NAMESPACE);
  
  return NS_OK;
}

/* void resolve (); */
NS_IMETHODIMP
nsSchemaBuiltinType::Resolve()
{
  return NS_OK;
}

/* void clear (); */
NS_IMETHODIMP
nsSchemaBuiltinType::Clear()
{
  return NS_OK;
}

/* readonly attribute wstring name; */
NS_IMETHODIMP
nsSchemaBuiltinType::GetName(nsAString& aName)
{
  switch(mBuiltinType) {
    case BUILTIN_TYPE_ANYTYPE:
      aName.AssignLiteral("anyType");
      break;
    case BUILTIN_TYPE_STRING:
      aName.AssignLiteral("string");
      break;
    case BUILTIN_TYPE_NORMALIZED_STRING:
      aName.AssignLiteral("normalizedString");
      break;
    case BUILTIN_TYPE_TOKEN:
      aName.AssignLiteral("token");
      break;
    case BUILTIN_TYPE_BYTE:
      aName.AssignLiteral("byte");
      break;
    case BUILTIN_TYPE_UNSIGNEDBYTE:
      aName.AssignLiteral("unsignedByte");
      break;
    case BUILTIN_TYPE_BASE64BINARY:
      aName.AssignLiteral("base64Binary");
      break;
    case BUILTIN_TYPE_HEXBINARY:
      aName.AssignLiteral("hexBinary");
      break;
    case BUILTIN_TYPE_INTEGER:
      aName.AssignLiteral("integer");
      break;
    case BUILTIN_TYPE_POSITIVEINTEGER:
      aName.AssignLiteral("positiveInteger");
      break;
    case BUILTIN_TYPE_NEGATIVEINTEGER:
      aName.AssignLiteral("negativeInteger");
      break;
    case BUILTIN_TYPE_NONNEGATIVEINTEGER:
      aName.AssignLiteral("nonNegativeInteger");
      break;
    case BUILTIN_TYPE_NONPOSITIVEINTEGER:
      aName.AssignLiteral("nonPositiveInteger");
      break;
    case BUILTIN_TYPE_INT:
      aName.AssignLiteral("int");
      break;
    case BUILTIN_TYPE_UNSIGNEDINT:
      aName.AssignLiteral("unsignedInt");
      break;
    case BUILTIN_TYPE_LONG:
      aName.AssignLiteral("long");
      break;
    case BUILTIN_TYPE_UNSIGNEDLONG:
      aName.AssignLiteral("unsignedLong");
      break;
    case BUILTIN_TYPE_SHORT:
      aName.AssignLiteral("short");
      break;
    case BUILTIN_TYPE_UNSIGNEDSHORT:
      aName.AssignLiteral("unsignedShort");
      break;
    case BUILTIN_TYPE_DECIMAL:
      aName.AssignLiteral("decimal");
      break;
    case BUILTIN_TYPE_FLOAT:
      aName.AssignLiteral("float");
      break;
    case BUILTIN_TYPE_DOUBLE:
      aName.AssignLiteral("double");
      break;
    case BUILTIN_TYPE_BOOLEAN:
      aName.AssignLiteral("boolean");
      break;
    case BUILTIN_TYPE_TIME:
      aName.AssignLiteral("time");
      break;
    case BUILTIN_TYPE_DATETIME:
      aName.AssignLiteral("dateTime");
      break;
    case BUILTIN_TYPE_DURATION:
      aName.AssignLiteral("duration");
      break;
    case BUILTIN_TYPE_DATE:
      aName.AssignLiteral("date");
      break;
    case BUILTIN_TYPE_GMONTH:
      aName.AssignLiteral("gMonth");
      break;
    case BUILTIN_TYPE_GYEAR:
      aName.AssignLiteral("gYear");
      break;
    case BUILTIN_TYPE_GYEARMONTH:
      aName.AssignLiteral("gYearMonth");
      break;
    case BUILTIN_TYPE_GDAY:
      aName.AssignLiteral("gDay");
      break;
    case BUILTIN_TYPE_GMONTHDAY:
      aName.AssignLiteral("gMonthDay");
      break;
    case BUILTIN_TYPE_NAME:
      aName.AssignLiteral("name");
      break;
    case BUILTIN_TYPE_QNAME:
      aName.AssignLiteral("QName");
      break;
    case BUILTIN_TYPE_NCNAME:
      aName.AssignLiteral("NCName");
      break;
    case BUILTIN_TYPE_ANYURI:
      aName.AssignLiteral("anyURI");
      break;
    case BUILTIN_TYPE_LANGUAGE:
      aName.AssignLiteral("language");
      break;
    case BUILTIN_TYPE_ID:
      aName.AssignLiteral("ID");
      break;
    case BUILTIN_TYPE_IDREF:
      aName.AssignLiteral("IDREF");
      break;
    case BUILTIN_TYPE_IDREFS:
      aName.AssignLiteral("IDREFS");
      break;
    case BUILTIN_TYPE_ENTITY:
      aName.AssignLiteral("ENTITY");
      break;
    case BUILTIN_TYPE_ENTITIES:
      aName.AssignLiteral("ENTITIES");
      break;
    case BUILTIN_TYPE_NOTATION:
      aName.AssignLiteral("NOTATION");
      break;
    case BUILTIN_TYPE_NMTOKEN:
      aName.AssignLiteral("NMTOKEN");
      break;
    case BUILTIN_TYPE_NMTOKENS:
      aName.AssignLiteral("NMTOKENS");
      break;
    default:
      NS_ERROR("Unknown builtin type!");
      aName.Truncate();
  }

  return NS_OK;
}

/* readonly attribute unsigned short schemaType; */
NS_IMETHODIMP
nsSchemaBuiltinType::GetSchemaType(PRUint16 *aSchemaType)
{
  NS_ENSURE_ARG_POINTER(aSchemaType);

  *aSchemaType = nsISchemaType::SCHEMA_TYPE_SIMPLE;

  return NS_OK;
}

/* readonly attribute unsigned short simpleType; */
NS_IMETHODIMP
nsSchemaBuiltinType::GetSimpleType(PRUint16 *aSimpleType)
{
  NS_ENSURE_ARG_POINTER(aSimpleType);

  *aSimpleType = nsISchemaSimpleType::SIMPLE_TYPE_BUILTIN;

  return NS_OK;
}

/* readonly attribute unsigned short builtinType; */
NS_IMETHODIMP
nsSchemaBuiltinType::GetBuiltinType(PRUint16 *aBuiltinType)
{
  NS_ENSURE_ARG_POINTER(aBuiltinType);
  
  *aBuiltinType = mBuiltinType;

  return NS_OK;
}

////////////////////////////////////////////////////////////
//
// nsSchemaListType implementation
//
////////////////////////////////////////////////////////////
nsSchemaListType::nsSchemaListType(nsSchema* aSchema, 
                                   const nsAString& aName)
  : nsSchemaComponentBase(aSchema), mName(aName)
{
}

nsSchemaListType::~nsSchemaListType()
{
}

NS_IMPL_ISUPPORTS4_CI(nsSchemaListType, 
                      nsISchemaComponent,
                      nsISchemaType,
                      nsISchemaSimpleType,
                      nsISchemaListType)

/* void resolve (); */
NS_IMETHODIMP
nsSchemaListType::Resolve()
{
  if (mIsResolved) {
    return NS_OK;
  }

  nsresult rv = NS_OK;
  mIsResolved = PR_TRUE;
  if (mListType && mSchema) {
    nsCOMPtr<nsISchemaType> type;
    rv = mSchema->ResolveTypePlaceholder(mListType, getter_AddRefs(type));
    if (NS_FAILED(rv)) {
      return NS_ERROR_FAILURE;
    }
    mListType = do_QueryInterface(type);
    if (!mListType) {
      return NS_ERROR_FAILURE;
    }
  }
  rv = mListType->Resolve();

  return rv;
}

/* void clear (); */
NS_IMETHODIMP
nsSchemaListType::Clear()
{
  if (mIsCleared) {
    return NS_OK;
  }

  mIsCleared = PR_TRUE;
  if (mListType) {
    mListType->Clear();
    mListType = nsnull;
  }

  return NS_OK;
}

/* readonly attribute wstring name; */
NS_IMETHODIMP
nsSchemaListType::GetName(nsAString& aName)
{
  aName.Assign(mName);
  
  return NS_OK;
}

/* readonly attribute unsigned short schemaType; */
NS_IMETHODIMP
nsSchemaListType::GetSchemaType(PRUint16 *aSchemaType)
{
  NS_ENSURE_ARG_POINTER(aSchemaType);

  *aSchemaType = nsISchemaType::SCHEMA_TYPE_SIMPLE;

  return NS_OK;
}

/* readonly attribute unsigned short simpleType; */
NS_IMETHODIMP
nsSchemaListType::GetSimpleType(PRUint16 *aSimpleType)
{
  NS_ENSURE_ARG_POINTER(aSimpleType);

  *aSimpleType = nsISchemaSimpleType::SIMPLE_TYPE_LIST;

  return NS_OK;
}

/* readonly attribute nsISchemaSimpleType listType; */
NS_IMETHODIMP
nsSchemaListType::GetListType(nsISchemaSimpleType * *aListType)
{
  NS_ENSURE_ARG_POINTER(aListType);

  NS_IF_ADDREF(*aListType = mListType);

  return NS_OK;
}

NS_IMETHODIMP
nsSchemaListType::SetListType(nsISchemaSimpleType* aListType)
{
  mListType = aListType;

  return NS_OK;
}

////////////////////////////////////////////////////////////
//
// nsSchemaUnionType implementation
//
////////////////////////////////////////////////////////////
nsSchemaUnionType::nsSchemaUnionType(nsSchema* aSchema, 
                                     const nsAString& aName)
  : nsSchemaComponentBase(aSchema), mName(aName)
{
}

nsSchemaUnionType::~nsSchemaUnionType()
{
}

NS_IMPL_ISUPPORTS4_CI(nsSchemaUnionType, 
                      nsISchemaComponent,
                      nsISchemaType,
                      nsISchemaSimpleType,
                      nsISchemaUnionType)

/* void resolve (); */
NS_IMETHODIMP
nsSchemaUnionType::Resolve()
{
  if (mIsResolved) {
    return NS_OK;
  }

  mIsResolved = PR_TRUE;
  if (mSchema) {
    PRUint32 i, count;
    count = mUnionTypes.Count();
    for (i = 0; i < count; ++i) {
      nsCOMPtr<nsISchemaType> type;
      nsresult rv = mSchema->ResolveTypePlaceholder(mUnionTypes.ObjectAt(i),
                                                    getter_AddRefs(type));
      if (NS_FAILED(rv)) {
        return NS_ERROR_FAILURE;
      }
      nsCOMPtr<nsISchemaSimpleType> simpleType = do_QueryInterface(type);
      mUnionTypes.ReplaceObjectAt(simpleType, i);
      rv = type->Resolve();
      if (NS_FAILED(rv)) {
        return rv;
      }
    }
  }

  return NS_OK;
}

/* void clear (); */
NS_IMETHODIMP
nsSchemaUnionType::Clear()
{
  if (mIsCleared) {
    return NS_OK;
  }

  mIsCleared = PR_TRUE;
  PRUint32 i, count;
  count = mUnionTypes.Count();
  for (i = 0; i < count; ++i) {
    mUnionTypes.ObjectAt(i)->Clear();
  }
  mUnionTypes.Clear();

  return NS_OK;
}

/* readonly attribute wstring name; */
NS_IMETHODIMP
nsSchemaUnionType::GetName(nsAString& aName)
{
  aName.Assign(mName);
  
  return NS_OK;
}

/* readonly attribute unsigned short schemaType; */
NS_IMETHODIMP
nsSchemaUnionType::GetSchemaType(PRUint16 *aSchemaType)
{
  NS_ENSURE_ARG_POINTER(aSchemaType);

  *aSchemaType = nsISchemaType::SCHEMA_TYPE_SIMPLE;

  return NS_OK;
}

/* readonly attribute unsigned short simpleType; */
NS_IMETHODIMP
nsSchemaUnionType::GetSimpleType(PRUint16 *aSimpleType)
{
  NS_ENSURE_ARG_POINTER(aSimpleType);

  *aSimpleType = nsISchemaSimpleType::SIMPLE_TYPE_UNION;

  return NS_OK;
}

/* readonly attribute PRUint32 unionTypeCount; */
NS_IMETHODIMP
nsSchemaUnionType::GetUnionTypeCount(PRUint32 *aUnionTypeCount)
{
  NS_ENSURE_ARG_POINTER(aUnionTypeCount);

  *aUnionTypeCount = mUnionTypes.Count();

  return NS_OK;
}

/* nsISchemaSimpleType getUnionType (in PRUint32 index); */
NS_IMETHODIMP
nsSchemaUnionType::GetUnionType(PRUint32 aIndex, nsISchemaSimpleType** aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);

  if (aIndex >= (PRUint32)mUnionTypes.Count()) {
    return NS_ERROR_FAILURE;
  }

  NS_ADDREF(*aResult = mUnionTypes.ObjectAt(aIndex));

  return NS_OK;
}

NS_IMETHODIMP
nsSchemaUnionType::AddUnionType(nsISchemaSimpleType* aType)
{
  NS_ENSURE_ARG(aType);

  return mUnionTypes.AppendObject(aType) ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

////////////////////////////////////////////////////////////
//
// nsSchemaRestrictionType implementation
//
////////////////////////////////////////////////////////////
nsSchemaRestrictionType::nsSchemaRestrictionType(nsSchema* aSchema, 
                                                 const nsAString& aName)
  : nsSchemaComponentBase(aSchema), mName(aName)
{
}

nsSchemaRestrictionType::~nsSchemaRestrictionType()
{
}

NS_IMPL_ISUPPORTS4_CI(nsSchemaRestrictionType, 
                      nsISchemaComponent,
                      nsISchemaType,
                      nsISchemaSimpleType,
                      nsISchemaRestrictionType)

/* void resolve (); */
NS_IMETHODIMP
nsSchemaRestrictionType::Resolve()
{
  if (mIsResolved) {
    return NS_OK;
  }

  nsresult rv = NS_OK;
  mIsResolved = PR_TRUE;
  if (mBaseType && mSchema) {
    nsCOMPtr<nsISchemaType> type;
    rv = mSchema->ResolveTypePlaceholder(mBaseType, getter_AddRefs(type));
    if (NS_FAILED(rv)) {
      return NS_ERROR_FAILURE;
    }
    mBaseType = do_QueryInterface(type);
    if (!mBaseType) {
      return NS_ERROR_FAILURE;
    }
    rv = mBaseType->Resolve();
  }

  return rv;
}

/* void clear (); */
NS_IMETHODIMP
nsSchemaRestrictionType::Clear()
{
  if (mIsCleared) {
    return NS_OK;
  }

  mIsCleared = PR_TRUE;
  if (mBaseType) {
    mBaseType->Clear();
    mBaseType = nsnull;
  }

  PRUint32 i, count;
  count = mFacets.Count();
  for (i = 0; i < count; ++i) {
    mFacets.ObjectAt(i)->Clear();
  }
  mFacets.Clear();

  return NS_OK;
}

/* readonly attribute wstring name; */
NS_IMETHODIMP
nsSchemaRestrictionType::GetName(nsAString& aName)
{
  aName.Assign(mName);
  
  return NS_OK;
}

/* readonly attribute unsigned short schemaType; */
NS_IMETHODIMP
nsSchemaRestrictionType::GetSchemaType(PRUint16 *aSchemaType)
{
  NS_ENSURE_ARG_POINTER(aSchemaType);

  *aSchemaType = nsISchemaType::SCHEMA_TYPE_SIMPLE;

  return NS_OK;
}

/* readonly attribute unsigned short simpleType; */
NS_IMETHODIMP
nsSchemaRestrictionType::GetSimpleType(PRUint16 *aSimpleType)
{
  NS_ENSURE_ARG_POINTER(aSimpleType);

  *aSimpleType = nsISchemaSimpleType::SIMPLE_TYPE_RESTRICTION;

  return NS_OK;
}

/* readonly attribute nsISchemaSimpleType baseType; */
NS_IMETHODIMP
nsSchemaRestrictionType::GetBaseType(nsISchemaSimpleType * *aBaseType)
{
  NS_ENSURE_ARG_POINTER(aBaseType);

  NS_IF_ADDREF(*aBaseType = mBaseType);

  return NS_OK;
}

/* readonly attribute PRUint32 facetCount; */
NS_IMETHODIMP
nsSchemaRestrictionType::GetFacetCount(PRUint32 *aFacetCount)
{
  NS_ENSURE_ARG_POINTER(aFacetCount);

  *aFacetCount = mFacets.Count();

  return NS_OK;
}

/* nsISchemaFacet getFacet(in PRUint32 index); */
NS_IMETHODIMP
nsSchemaRestrictionType::GetFacet(PRUint32 aIndex, nsISchemaFacet** aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);

  if (aIndex >= (PRUint32)mFacets.Count()) {
    return NS_ERROR_FAILURE;
  }

  NS_ADDREF(*aResult = mFacets.ObjectAt(aIndex));

  return NS_OK;
}

NS_IMETHODIMP
nsSchemaRestrictionType::SetBaseType(nsISchemaSimpleType* aBaseType)
{
  NS_ENSURE_ARG(aBaseType);

  mBaseType = aBaseType;

  return NS_OK;
}

NS_IMETHODIMP
nsSchemaRestrictionType::AddFacet(nsISchemaFacet* aFacet)
{
  NS_ENSURE_ARG(aFacet);

  return mFacets.AppendObject(aFacet) ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

////////////////////////////////////////////////////////////
//
// nsSchemaTypePlaceholder implementation
//
////////////////////////////////////////////////////////////
nsSchemaTypePlaceholder::nsSchemaTypePlaceholder(nsSchema* aSchema,
                                                 const nsAString& aName)
  : nsSchemaComponentBase(aSchema), mName(aName)
{
}

nsSchemaTypePlaceholder::~nsSchemaTypePlaceholder()
{
}

NS_IMPL_ISUPPORTS3_CI(nsSchemaTypePlaceholder, 
                      nsISchemaComponent,
                      nsISchemaType,
                      nsISchemaSimpleType)


/* void resolve (); */
NS_IMETHODIMP
nsSchemaTypePlaceholder::Resolve()
{
  return NS_OK;
}

/* void clear (); */
NS_IMETHODIMP
nsSchemaTypePlaceholder::Clear()
{
  return NS_OK;
}

/* readonly attribute wstring name; */
NS_IMETHODIMP
nsSchemaTypePlaceholder::GetName(nsAString& aName)
{
  aName.Assign(mName);
  
  return NS_OK;
}

/* readonly attribute unsigned short schemaType; */
NS_IMETHODIMP
nsSchemaTypePlaceholder::GetSchemaType(PRUint16 *aSchemaType)
{
  NS_ENSURE_ARG_POINTER(aSchemaType);

  *aSchemaType = nsISchemaType::SCHEMA_TYPE_PLACEHOLDER;

  return NS_OK;
}

/* readonly attribute unsigned short simpleType; */
NS_IMETHODIMP
nsSchemaTypePlaceholder::GetSimpleType(PRUint16 *aSimpleType)
{
  return NS_ERROR_FAILURE;
}


////////////////////////////////////////////////////////////
//
// nsSchemaFacet implementation
//
////////////////////////////////////////////////////////////
nsSchemaFacet::nsSchemaFacet(nsSchema* aSchema)
  : nsSchemaComponentBase(aSchema), mIsFixed(PR_FALSE)
{
}

nsSchemaFacet::~nsSchemaFacet()
{
}

NS_IMPL_ISUPPORTS2_CI(nsSchemaFacet,
                      nsISchemaComponent,
                      nsISchemaFacet)

/* void resolve (); */
NS_IMETHODIMP
nsSchemaFacet::Resolve()
{
  return NS_OK;
}

/* void clear (); */
NS_IMETHODIMP
nsSchemaFacet::Clear()
{
  return NS_OK;
}

NS_IMETHODIMP
nsSchemaFacet::SetFacetType(PRUint16 aFacetType)
{
  mFacetType = aFacetType;

  return NS_OK;
}

NS_IMETHODIMP
nsSchemaFacet::SetIsFixed(PRBool aIsFixed) 
{
  mIsFixed = aIsFixed;

  return NS_OK;
}

/* readonly attribute unsigned short facetType; */
NS_IMETHODIMP
nsSchemaFacet::GetFacetType(PRUint16 *aFacetType)
{
  NS_ENSURE_ARG_POINTER(aFacetType);

  *aFacetType = mFacetType;
  
  return NS_OK;
}

/* readonly attribute AString value; */
NS_IMETHODIMP
nsSchemaFacet::GetValue(nsAString & aValue)
{
  if ((mFacetType == FACET_TYPE_TOTALDIGITS) ||
      (mFacetType == FACET_TYPE_FRACTIONDIGITS) ||
      (mFacetType == FACET_TYPE_WHITESPACE) ||
      (mFacetType == FACET_TYPE_LENGTH) ||
      (mFacetType == FACET_TYPE_MINLENGTH) ||
      (mFacetType == FACET_TYPE_MAXLENGTH)) {
    return NS_ERROR_ILLEGAL_VALUE;
  }

  aValue.Assign(mStrValue);

  return NS_OK;
}

/* readonly attribute PRUint32 lengthValue; */
NS_IMETHODIMP
nsSchemaFacet::GetLengthValue(PRUint32 *aLengthValue)
{
  NS_ENSURE_ARG_POINTER(aLengthValue);

  if ((mFacetType != FACET_TYPE_LENGTH) &&
      (mFacetType != FACET_TYPE_MINLENGTH) &&
      (mFacetType != FACET_TYPE_MAXLENGTH)) {
    return NS_ERROR_ILLEGAL_VALUE;
  }

  *aLengthValue = mUintValue;

  return NS_OK;
}

/* readonly attribute PRUint32 digitsValue; */
NS_IMETHODIMP
nsSchemaFacet::GetDigitsValue(PRUint32 *aDigitsValue)
{
  NS_ENSURE_ARG_POINTER(aDigitsValue);

  if ((mFacetType != FACET_TYPE_TOTALDIGITS) &&
      (mFacetType != FACET_TYPE_FRACTIONDIGITS)) {
    return NS_ERROR_ILLEGAL_VALUE;
  }

  *aDigitsValue = mUintValue;

  return NS_OK;
}

/* readonly attribute unsigned short whitespaceValue; */
NS_IMETHODIMP
nsSchemaFacet::GetWhitespaceValue(PRUint16 *aWhitespaceValue)
{
  NS_ENSURE_ARG_POINTER(aWhitespaceValue);

  if (mFacetType != FACET_TYPE_WHITESPACE) {
    return NS_ERROR_ILLEGAL_VALUE;
  }

  *aWhitespaceValue = mWhitespaceValue;

  return NS_OK;
}

/* readonly attribute boolean isfixed; */
NS_IMETHODIMP
nsSchemaFacet::GetIsfixed(PRBool *aIsFixed)
{
  NS_ENSURE_ARG_POINTER(aIsFixed);
  
  *aIsFixed = mIsFixed;

  return NS_OK;
}

NS_IMETHODIMP
nsSchemaFacet::SetValue(const nsAString& aStrValue)
{
  mStrValue.Assign(aStrValue);

  return NS_OK;
}

NS_IMETHODIMP
nsSchemaFacet::SetUintValue(PRUint32 aUintValue)
{
  mUintValue = aUintValue;

  return NS_OK;
}

NS_IMETHODIMP
nsSchemaFacet::SetWhitespaceValue(PRUint16 aWhitespaceValue)
{
  mWhitespaceValue = aWhitespaceValue;

  return NS_OK;
}
