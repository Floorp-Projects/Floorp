/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is Mozilla.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications.  Portions created by Netscape Communications are
 * Copyright (C) 2001 by Netscape Communications.  All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *   Vidur Apparao <vidur@netscape.com> (original author)
 */

#include "nsSchemaPrivate.h"
#include "nsSchemaLoader.h"



// content includes
#include "nsIContent.h"
#include "nsINameSpaceManager.h"
#include "nsINodeInfo.h"

// string includes
#include "nsReadableUtils.h"

////////////////////////////////////////////////////////////
//
// nsSchemaAtoms implementation
//
////////////////////////////////////////////////////////////

// Statics
nsIAtom* nsSchemaAtoms::sString_atom = nsnull;
nsIAtom* nsSchemaAtoms::sNormalizedString_atom = nsnull;
nsIAtom* nsSchemaAtoms::sToken_atom = nsnull;
nsIAtom* nsSchemaAtoms::sByte_atom = nsnull;
nsIAtom* nsSchemaAtoms::sUnsignedByte_atom = nsnull;
nsIAtom* nsSchemaAtoms::sBase64Binary_atom = nsnull;
nsIAtom* nsSchemaAtoms::sHexBinary_atom = nsnull;
nsIAtom* nsSchemaAtoms::sInteger_atom = nsnull;
nsIAtom* nsSchemaAtoms::sPositiveInteger_atom = nsnull;
nsIAtom* nsSchemaAtoms::sNegativeInteger_atom = nsnull;
nsIAtom* nsSchemaAtoms::sNonnegativeInteger_atom = nsnull;
nsIAtom* nsSchemaAtoms::sNonpositiveInteger_atom = nsnull;
nsIAtom* nsSchemaAtoms::sInt_atom = nsnull;
nsIAtom* nsSchemaAtoms::sUnsignedInt_atom = nsnull;
nsIAtom* nsSchemaAtoms::sLong_atom = nsnull;
nsIAtom* nsSchemaAtoms::sUnsignedLong_atom = nsnull;
nsIAtom* nsSchemaAtoms::sShort_atom = nsnull;
nsIAtom* nsSchemaAtoms::sUnsignedShort_atom = nsnull;
nsIAtom* nsSchemaAtoms::sDecimal_atom = nsnull;
nsIAtom* nsSchemaAtoms::sFloat_atom = nsnull;
nsIAtom* nsSchemaAtoms::sDouble_atom = nsnull;
nsIAtom* nsSchemaAtoms::sBoolean_atom = nsnull;
nsIAtom* nsSchemaAtoms::sTime_atom = nsnull;
nsIAtom* nsSchemaAtoms::sDateTime_atom = nsnull;
nsIAtom* nsSchemaAtoms::sDuration_atom = nsnull;
nsIAtom* nsSchemaAtoms::sDate_atom = nsnull;
nsIAtom* nsSchemaAtoms::sGMonth_atom = nsnull;
nsIAtom* nsSchemaAtoms::sGYear_atom = nsnull;
nsIAtom* nsSchemaAtoms::sGYearMonth_atom = nsnull;
nsIAtom* nsSchemaAtoms::sGDay_atom = nsnull;
nsIAtom* nsSchemaAtoms::sGMonthDay_atom = nsnull;
nsIAtom* nsSchemaAtoms::sName_atom = nsnull;
nsIAtom* nsSchemaAtoms::sQName_atom = nsnull;
nsIAtom* nsSchemaAtoms::sNCName_atom = nsnull;
nsIAtom* nsSchemaAtoms::sAnyUri_atom = nsnull;
nsIAtom* nsSchemaAtoms::sLanguage_atom = nsnull;
nsIAtom* nsSchemaAtoms::sID_atom = nsnull;
nsIAtom* nsSchemaAtoms::sIDREF_atom = nsnull;
nsIAtom* nsSchemaAtoms::sIDREFS_atom = nsnull;
nsIAtom* nsSchemaAtoms::sENTITY_atom = nsnull;
nsIAtom* nsSchemaAtoms::sENTITIES_atom = nsnull;
nsIAtom* nsSchemaAtoms::sNOTATION_atom = nsnull;
nsIAtom* nsSchemaAtoms::sNMTOKEN_atom = nsnull;
nsIAtom* nsSchemaAtoms::sNMTOKENS_atom = nsnull;

nsIAtom* nsSchemaAtoms::sElement_atom = nsnull;
nsIAtom* nsSchemaAtoms::sModelGroup_atom = nsnull;
nsIAtom* nsSchemaAtoms::sAttribute_atom = nsnull;
nsIAtom* nsSchemaAtoms::sAttributeGroup_atom = nsnull;
nsIAtom* nsSchemaAtoms::sSimpleType_atom = nsnull;
nsIAtom* nsSchemaAtoms::sComplexType_atom = nsnull;
nsIAtom* nsSchemaAtoms::sSimpleContent_atom = nsnull;
nsIAtom* nsSchemaAtoms::sComplexContent_atom = nsnull;
nsIAtom* nsSchemaAtoms::sAll_atom = nsnull;
nsIAtom* nsSchemaAtoms::sChoice_atom = nsnull;
nsIAtom* nsSchemaAtoms::sSequence_atom = nsnull;
nsIAtom* nsSchemaAtoms::sAnyAttribute_atom = nsnull;
nsIAtom* nsSchemaAtoms::sRestriction_atom = nsnull;
nsIAtom* nsSchemaAtoms::sExtension_atom = nsnull;
nsIAtom* nsSchemaAtoms::sAnnotation_atom = nsnull;

void
nsSchemaAtoms::CreateSchemaAtoms()
{
  sString_atom = NS_NewAtom("string");
  sNormalizedString_atom = NS_NewAtom("normalizedString");
  sToken_atom = NS_NewAtom("token");
  sByte_atom = NS_NewAtom("byte");
  sUnsignedByte_atom = NS_NewAtom("unsignedByte");
  sBase64Binary_atom = NS_NewAtom("base64Binary");
  sHexBinary_atom = NS_NewAtom("hexBinary");
  sInteger_atom = NS_NewAtom("integer");
  sPositiveInteger_atom = NS_NewAtom("positiveInteger");
  sNegativeInteger_atom = NS_NewAtom("negativeInteger");
  sNonnegativeInteger_atom = NS_NewAtom("nonnegativeInteger");
  sNonpositiveInteger_atom = NS_NewAtom("nonpositiveInteger");
  sInt_atom = NS_NewAtom("int");
  sUnsignedInt_atom = NS_NewAtom("unsignedInt");
  sLong_atom = NS_NewAtom("long");
  sUnsignedLong_atom = NS_NewAtom("unsignedLong");
  sShort_atom = NS_NewAtom("short");
  sUnsignedShort_atom = NS_NewAtom("unsignedShort");
  sDecimal_atom = NS_NewAtom("decimal");
  sFloat_atom = NS_NewAtom("float");
  sDouble_atom = NS_NewAtom("double");
  sBoolean_atom = NS_NewAtom("boolean");
  sTime_atom = NS_NewAtom("time");
  sDateTime_atom = NS_NewAtom("dateTime");
  sDuration_atom = NS_NewAtom("duration");
  sDate_atom = NS_NewAtom("date");
  sGMonth_atom = NS_NewAtom("gMonth");
  sGYear_atom = NS_NewAtom("gYear");
  sGYearMonth_atom = NS_NewAtom("gYearMonth");
  sGDay_atom = NS_NewAtom("gDay");
  sGMonthDay_atom = NS_NewAtom("gMonthDay");
  sName_atom = NS_NewAtom("name");
  sQName_atom = NS_NewAtom("QName");
  sNCName_atom = NS_NewAtom("NCName");
  sAnyUri_atom = NS_NewAtom("anyUri");
  sLanguage_atom = NS_NewAtom("language");
  sID_atom = NS_NewAtom("ID");
  sIDREF_atom = NS_NewAtom("IDREF");
  sIDREFS_atom = NS_NewAtom("IDREFS");
  sENTITY_atom = NS_NewAtom("ENTITY");
  sENTITIES_atom = NS_NewAtom("ENTITIES");
  sNOTATION_atom = NS_NewAtom("NOTATION");
  sNMTOKEN_atom = NS_NewAtom("NMTOKEN");
  sNMTOKENS_atom = NS_NewAtom("NMTOKENS");

  sElement_atom = NS_NewAtom("element");
  sModelGroup_atom = NS_NewAtom("group");
  sAttribute_atom = NS_NewAtom("attribute");
  sAttributeGroup_atom = NS_NewAtom("attributeGroup");
  sSimpleType_atom = NS_NewAtom("simpleType");
  sComplexType_atom = NS_NewAtom("complexType");
  sSimpleContent_atom = NS_NewAtom("simpleContent");
  sComplexContent_atom = NS_NewAtom("complexContent");
  sAll_atom = NS_NewAtom("all");
  sChoice_atom = NS_NewAtom("choice");
  sSequence_atom = NS_NewAtom("sequence");
  sAnyAttribute_atom = NS_NewAtom("anyAttribute");
  sRestriction_atom = NS_NewAtom("restriction");
  sExtension_atom = NS_NewAtom("extension");
  sAnnotation_atom = NS_NewAtom("annotation");
}

void
nsSchemaAtoms::DestroySchemaAtoms()
{
  NS_RELEASE(sString_atom);
  NS_RELEASE(sNormalizedString_atom);
  NS_RELEASE(sToken_atom);
  NS_RELEASE(sByte_atom);
  NS_RELEASE(sUnsignedByte_atom);
  NS_RELEASE(sBase64Binary_atom);
  NS_RELEASE(sHexBinary_atom);
  NS_RELEASE(sInteger_atom);
  NS_RELEASE(sPositiveInteger_atom);
  NS_RELEASE(sNegativeInteger_atom);
  NS_RELEASE(sNonnegativeInteger_atom);
  NS_RELEASE(sNonpositiveInteger_atom);
  NS_RELEASE(sInt_atom);
  NS_RELEASE(sUnsignedInt_atom);
  NS_RELEASE(sLong_atom);
  NS_RELEASE(sUnsignedLong_atom);
  NS_RELEASE(sShort_atom);
  NS_RELEASE(sUnsignedShort_atom);
  NS_RELEASE(sDecimal_atom);
  NS_RELEASE(sFloat_atom);
  NS_RELEASE(sDouble_atom);
  NS_RELEASE(sBoolean_atom);
  NS_RELEASE(sTime_atom);
  NS_RELEASE(sDateTime_atom);
  NS_RELEASE(sDuration_atom);
  NS_RELEASE(sDate_atom);
  NS_RELEASE(sGMonth_atom);
  NS_RELEASE(sGYear_atom);
  NS_RELEASE(sGYearMonth_atom);
  NS_RELEASE(sGDay_atom);
  NS_RELEASE(sGMonthDay_atom);
  NS_RELEASE(sName_atom);
  NS_RELEASE(sQName_atom);
  NS_RELEASE(sNCName_atom);
  NS_RELEASE(sAnyUri_atom);
  NS_RELEASE(sLanguage_atom);
  NS_RELEASE(sID_atom);
  NS_RELEASE(sIDREF_atom);
  NS_RELEASE(sIDREFS_atom);
  NS_RELEASE(sENTITY_atom);
  NS_RELEASE(sENTITIES_atom);
  NS_RELEASE(sNOTATION_atom);
  NS_RELEASE(sNMTOKEN_atom);
  NS_RELEASE(sNMTOKENS_atom);

  NS_RELEASE(sElement_atom);
  NS_RELEASE(sModelGroup_atom);
  NS_RELEASE(sAttribute_atom);
  NS_RELEASE(sAttributeGroup_atom);
  NS_RELEASE(sSimpleType_atom);
  NS_RELEASE(sComplexType_atom);
  NS_RELEASE(sSimpleContent_atom);
  NS_RELEASE(sComplexContent_atom);
  NS_RELEASE(sAll_atom);
  NS_RELEASE(sChoice_atom);
  NS_RELEASE(sSequence_atom);
  NS_RELEASE(sAnyAttribute_atom);
  NS_RELEASE(sRestriction_atom);
  NS_RELEASE(sExtension_atom);
  NS_RELEASE(sAnnotation_atom);
}

////////////////////////////////////////////////////////////
//
// nsSchemaLoader implementation
//
////////////////////////////////////////////////////////////

nsSchemaLoader::nsSchemaLoader()
{
  NS_INIT_ISUPPORTS();
}

nsSchemaLoader::~nsSchemaLoader()
{
  mBuiltinTypesHash.Reset();
}

NS_IMPL_ISUPPORTS1(nsSchemaLoader, nsISchemaLoader)

/* nsISchema load (in nsIURI schemaURI); */
NS_IMETHODIMP 
nsSchemaLoader::Load(nsIURI *schemaURI, nsISchema **_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void loadAsync (in nsIURI schemaURI, in nsISchemaLoadListener listener); */
NS_IMETHODIMP 
nsSchemaLoader::LoadAsync(nsIURI *schemaURI, nsISchemaLoadListener *listener)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* nsISchema processSchemaElement (in nsIDOMElement element); */
NS_IMETHODIMP 
nsSchemaLoader::ProcessSchemaElement(nsIDOMElement *element, 
                                     nsISchema **_retval)
{
  NS_ENSURE_ARG(element);
  NS_ENSURE_ARG_POINTER(_retval);

  nsresult rv = NS_OK;

  // Get target namespace and create the schema instance
  nsAutoString targetNamespace;
  element->GetAttribute(NS_LITERAL_STRING("targetNamespace"), 
                        targetNamespace);
  nsSchema* schemaInst = new nsSchema(targetNamespace);
  nsCOMPtr<nsISchema> schema = schemaInst;
  if (!schema) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  nsSchemaLoadingContext context;

  // Iterate over the first level child elements of the schema
  // element, processing them as we go
  nsCOMPtr<nsIDOMNodeList> childNodes;
  element->GetChildNodes(getter_AddRefs(childNodes));

  if (childNodes) {
    PRUint32 i, count;   
    childNodes->GetLength(&count);

    for (i = 0; i < count; i++) {
      nsCOMPtr<nsIDOMNode> child;     
      childNodes->Item(i, getter_AddRefs(child));

      nsCOMPtr<nsIDOMElement> childElement(do_QueryInterface(child, &rv));
      if (NS_SUCCEEDED(rv) && childElement) {
        nsAutoString namespaceURI;
        
        childElement->GetNamespaceURI(namespaceURI);
        if (namespaceURI.Equals(NS_LITERAL_STRING(NS_SCHEMA_NAMESPACE))) {

          nsCOMPtr<nsIContent> content(do_QueryInterface(childElement));
          NS_ASSERTION(content, "Element is not content");
          if (!content) {
            return NS_ERROR_FAILURE;
          }
          
          nsCOMPtr<nsINodeInfo> nodeInfo;
          content->GetNodeInfo(*getter_AddRefs(nodeInfo));
          if (!nodeInfo) {
            return NS_ERROR_FAILURE;
          }

          nsCOMPtr<nsIAtom> tagName;
          nodeInfo->GetNameAtom(*getter_AddRefs(tagName));

          if (tagName == nsSchemaAtoms::sElement_atom) {
            nsCOMPtr<nsISchemaElement> schemaElement;
            rv = ProcessElement(schemaInst, &context, childElement,  
                                getter_AddRefs(schemaElement));
            if (NS_SUCCEEDED(rv)) {
              rv = schemaInst->AddElement(schemaElement);
            }
          }
          else if (tagName == nsSchemaAtoms::sComplexType_atom) {
            nsCOMPtr<nsISchemaComplexType> complexType;
            rv = ProcessComplexType(schemaInst, &context, childElement,
                                    getter_AddRefs(complexType));
            if (NS_SUCCEEDED(rv)) {
              rv = schemaInst->AddType(complexType);
            }
          }
          else if (tagName == nsSchemaAtoms::sSimpleType_atom) {
            nsCOMPtr<nsISchemaSimpleType> simpleType;
            rv = ProcessSimpleType(schemaInst, &context, childElement,
                                   getter_AddRefs(simpleType));
            if (NS_SUCCEEDED(rv)) {
              rv = schemaInst->AddType(simpleType);
            }
          }
          else if (tagName == nsSchemaAtoms::sAttribute_atom) {
            nsCOMPtr<nsISchemaAttribute> attribute;
            rv = ProcessAttribute(schemaInst, &context, childElement,
                                  getter_AddRefs(attribute));
            if (NS_SUCCEEDED(rv)) {
              rv = schemaInst->AddAttribute(attribute);
            }
          }
          else if (tagName == nsSchemaAtoms::sAttributeGroup_atom) {
            nsCOMPtr<nsISchemaAttributeGroup> attributeGroup;
            rv = ProcessAttributeGroup(schemaInst, &context, childElement,
                                       getter_AddRefs(attributeGroup));
            if (NS_SUCCEEDED(rv)) {
              rv = schemaInst->AddAttributeGroup(attributeGroup);
            }
          }
          else if (tagName == nsSchemaAtoms::sModelGroup_atom) {
            nsCOMPtr<nsISchemaModelGroup> modelGroup;
            rv = ProcessModelGroup(schemaInst, &context, childElement,
                                   getter_AddRefs(modelGroup));
            if (NS_SUCCEEDED(rv)) {
              rv = schemaInst->AddModelGroup(modelGroup);
            }
          }
          // For now, ignore the following
          // annotations
          // include
          // import
          // redefine
          // notation
          // identity-constraint elements
          if (NS_FAILED(rv)) {
            return rv;
          }
        }
      }
    }
  }

  // Resolve all forward references 
  rv = schemaInst->Resolve();
  if (NS_FAILED(rv)) {
    return rv;
  }

  *_retval = schema;
  NS_ADDREF(*_retval);

  return NS_OK;
}

static void
SeparateNamespacePrefix(const nsAReadableString& aString,
                        nsAWritableString& aPrefix,
                        nsAWritableString& aLocalName)
{
  nsReadingIterator<PRUnichar> pos, begin, end;

  aString.BeginReading(begin);
  aString.EndReading(end); 
  pos = begin;

  if (FindCharInReadable(PRUnichar(':'), pos, end)) {
    CopyUnicodeTo(begin, pos, aPrefix);
    CopyUnicodeTo(++pos, end, aLocalName);
  }
  else {
    CopyUnicodeTo(begin, end, aLocalName);
  }
}

nsresult
nsSchemaLoader::GetBuiltinType(const nsAReadableString& aName,
                               nsISchemaType** aType)
{
  nsresult rv = NS_OK;
  nsStringKey key(aName);
  nsCOMPtr<nsISupports> sup = dont_AddRef(mBuiltinTypesHash.Get(&key));
  if (sup) {
    rv = CallQueryInterface(sup, aType);
  }
  else {
    nsCOMPtr<nsIAtom> typeName = dont_AddRef(NS_NewAtom(aName));
    PRUint16 typeVal = nsISchemaBuiltinType::BUILTIN_TYPE_URTYPE;
    if (typeName == nsSchemaAtoms::sString_atom) {
      typeVal = nsISchemaBuiltinType::BUILTIN_TYPE_STRING;
    }
    else if (typeName == nsSchemaAtoms::sNormalizedString_atom) {
      typeVal = nsISchemaBuiltinType::BUILTIN_TYPE_NORMALIZED_STRING;
    }
    else if (typeName == nsSchemaAtoms::sToken_atom) {
      typeVal = nsISchemaBuiltinType::BUILTIN_TYPE_TOKEN;
    }
    else if (typeName == nsSchemaAtoms::sByte_atom) {
      typeVal = nsISchemaBuiltinType::BUILTIN_TYPE_BYTE;
    }
    else if (typeName == nsSchemaAtoms::sUnsignedByte_atom) {
      typeVal = nsISchemaBuiltinType::BUILTIN_TYPE_UNSIGNEDBYTE;
    }
    else if (typeName == nsSchemaAtoms::sBase64Binary_atom) {
      typeVal = nsISchemaBuiltinType::BUILTIN_TYPE_BASE64BINARY;
    }
    else if (typeName == nsSchemaAtoms::sHexBinary_atom) {
      typeVal = nsISchemaBuiltinType::BUILTIN_TYPE_HEXBINARY;
    }
    else if (typeName == nsSchemaAtoms::sInteger_atom) {
      typeVal = nsISchemaBuiltinType::BUILTIN_TYPE_INTEGER;
    }
    else if (typeName == nsSchemaAtoms::sPositiveInteger_atom) {
      typeVal = nsISchemaBuiltinType::BUILTIN_TYPE_POSITIVEINTEGER;
    }
    else if (typeName == nsSchemaAtoms::sNegativeInteger_atom) {
      typeVal = nsISchemaBuiltinType::BUILTIN_TYPE_NEGATIVEINTEGER;
    }
    else if (typeName == nsSchemaAtoms::sNonnegativeInteger_atom) {
      typeVal = nsISchemaBuiltinType::BUILTIN_TYPE_NONNEGATIVEINTEGER;
    }
    else if (typeName == nsSchemaAtoms::sNonpositiveInteger_atom) {
      typeVal = nsISchemaBuiltinType::BUILTIN_TYPE_NONPOSITIVEINTEGER;
    }
    else if (typeName == nsSchemaAtoms::sInt_atom) {
      typeVal = nsISchemaBuiltinType::BUILTIN_TYPE_INT;
    }
    else if (typeName == nsSchemaAtoms::sUnsignedInt_atom) {
      typeVal = nsISchemaBuiltinType::BUILTIN_TYPE_UNSIGNEDINT;
    }
    else if (typeName == nsSchemaAtoms::sLong_atom) {
      typeVal = nsISchemaBuiltinType::BUILTIN_TYPE_LONG;
    }
    else if (typeName == nsSchemaAtoms::sUnsignedLong_atom) {
      typeVal = nsISchemaBuiltinType::BUILTIN_TYPE_UNSIGNEDLONG;
    }
    else if (typeName == nsSchemaAtoms::sShort_atom) {
      typeVal = nsISchemaBuiltinType::BUILTIN_TYPE_SHORT;
    }
    else if (typeName == nsSchemaAtoms::sUnsignedShort_atom) {
      typeVal = nsISchemaBuiltinType::BUILTIN_TYPE_UNSIGNEDSHORT;
    }
    else if (typeName == nsSchemaAtoms::sDecimal_atom) {
      typeVal = nsISchemaBuiltinType::BUILTIN_TYPE_DECIMAL;
    }
    else if (typeName == nsSchemaAtoms::sFloat_atom) {
      typeVal = nsISchemaBuiltinType::BUILTIN_TYPE_FLOAT;
    }
    else if (typeName == nsSchemaAtoms::sDouble_atom) {
      typeVal = nsISchemaBuiltinType::BUILTIN_TYPE_DOUBLE;
    }
    else if (typeName == nsSchemaAtoms::sBoolean_atom) {
      typeVal = nsISchemaBuiltinType::BUILTIN_TYPE_BOOLEAN;
    }
    else if (typeName == nsSchemaAtoms::sTime_atom) {
      typeVal = nsISchemaBuiltinType::BUILTIN_TYPE_TIME;
    }
    else if (typeName == nsSchemaAtoms::sDateTime_atom) {
      typeVal = nsISchemaBuiltinType::BUILTIN_TYPE_DATETIME;
    }
    else if (typeName == nsSchemaAtoms::sDuration_atom) {
      typeVal = nsISchemaBuiltinType::BUILTIN_TYPE_DURATION;
    }
    else if (typeName == nsSchemaAtoms::sDate_atom) {
      typeVal = nsISchemaBuiltinType::BUILTIN_TYPE_DATE;
    }
    else if (typeName == nsSchemaAtoms::sGMonth_atom) {
      typeVal = nsISchemaBuiltinType::BUILTIN_TYPE_GMONTH;
    }
    else if (typeName == nsSchemaAtoms::sGYear_atom) {
      typeVal = nsISchemaBuiltinType::BUILTIN_TYPE_GYEAR;
    }
    else if (typeName == nsSchemaAtoms::sGYearMonth_atom) {
      typeVal = nsISchemaBuiltinType::BUILTIN_TYPE_GYEARMONTH;
    }
    else if (typeName == nsSchemaAtoms::sGDay_atom) {
      typeVal = nsISchemaBuiltinType::BUILTIN_TYPE_GDAY;
    }
    else if (typeName == nsSchemaAtoms::sGMonthDay_atom) {
      typeVal = nsISchemaBuiltinType::BUILTIN_TYPE_GMONTHDAY;
    }
    else if (typeName == nsSchemaAtoms::sName_atom) {
      typeVal = nsISchemaBuiltinType::BUILTIN_TYPE_NAME;
    }
    else if (typeName == nsSchemaAtoms::sQName_atom) {
      typeVal = nsISchemaBuiltinType::BUILTIN_TYPE_QNAME;
    }
    else if (typeName == nsSchemaAtoms::sNCName_atom) {
      typeVal = nsISchemaBuiltinType::BUILTIN_TYPE_NCNAME;
    }
    else if (typeName == nsSchemaAtoms::sAnyUri_atom) {
      typeVal = nsISchemaBuiltinType::BUILTIN_TYPE_ANYURI;
    }
    else if (typeName == nsSchemaAtoms::sLanguage_atom) {
      typeVal = nsISchemaBuiltinType::BUILTIN_TYPE_LANGUAGE;
    }
    else if (typeName == nsSchemaAtoms::sID_atom) {
      typeVal = nsISchemaBuiltinType::BUILTIN_TYPE_ID;
    }
    else if (typeName == nsSchemaAtoms::sIDREF_atom) {
      typeVal = nsISchemaBuiltinType::BUILTIN_TYPE_IDREF;
    }
    else if (typeName == nsSchemaAtoms::sIDREFS_atom) {
      typeVal = nsISchemaBuiltinType::BUILTIN_TYPE_IDREFS;
    }
    else if (typeName == nsSchemaAtoms::sENTITY_atom) {
      typeVal = nsISchemaBuiltinType::BUILTIN_TYPE_ENTITY;
    }
    else if (typeName == nsSchemaAtoms::sENTITIES_atom) {
      typeVal = nsISchemaBuiltinType::BUILTIN_TYPE_ENTITIES;
    }
    else if (typeName == nsSchemaAtoms::sNOTATION_atom) {
      typeVal = nsISchemaBuiltinType::BUILTIN_TYPE_NOTATION;
    }
    else if (typeName == nsSchemaAtoms::sNMTOKEN_atom) {
      typeVal = nsISchemaBuiltinType::BUILTIN_TYPE_NMTOKEN;
    }
    else if (typeName == nsSchemaAtoms::sNMTOKENS_atom) {
      typeVal = nsISchemaBuiltinType::BUILTIN_TYPE_NMTOKENS;
    }
    else {
      NS_ERROR("Unknown builtin type");
      return NS_ERROR_FAILURE;
    }

    nsSchemaBuiltinType* builtin = new nsSchemaBuiltinType(typeVal);
    if (!builtin) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    sup = builtin;
    mBuiltinTypesHash.Put(&key, sup);
    
    *aType = builtin;
    NS_ADDREF(*aType);
  }

  return NS_OK;
}

nsresult
nsSchemaLoader::GetNewOrUsedType(nsSchema* aSchema,
                                 nsSchemaLoadingContext* aContext,
                                 const nsAReadableString& aTypeName,
                                 nsISchemaType** aType)
{
  nsresult rv = NS_OK;
  nsAutoString prefix, localName, namespaceURI;

  // See if there's a prefix and, if so, get the
  // namespace associated with the prefix
  SeparateNamespacePrefix(aTypeName, prefix, localName);
  if ((prefix.Length() > 0) &&
      !aContext->GetNamespaceURIForPrefix(prefix, namespaceURI)) {
    // Unknown prefix
    return NS_ERROR_FAILURE;
  }
  
  if (namespaceURI.Length() > 0) {
    // XXX Currently we only understand the schema namespace. When
    // we start dealing with includes and imports, we can search
    // for types in other namespaces.
    if (!namespaceURI.Equals(NS_LITERAL_STRING(NS_SCHEMA_NAMESPACE))) {
      return NS_ERROR_FAILURE;
    }  

    rv = GetBuiltinType(localName, aType);
  }
  // We don't have a namespace, so get the local type 
  else {
    rv = aSchema->GetTypeByName(localName, aType);
    
    // If we didn't get a type, we need to create a placeholder
    if (NS_SUCCEEDED(rv) && !*aType) {
      nsSchemaTypePlaceholder* placeholder = new nsSchemaTypePlaceholder(aSchema,
                                                                         localName);
      if (!placeholder) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
      *aType = placeholder;
      NS_ADDREF(*aType);
    }
  }

  return rv;
}

nsresult 
nsSchemaLoader::ProcessElement(nsSchema* aSchema, 
                               nsSchemaLoadingContext* aContext,
                               nsIDOMElement* aElement,
                               nsISchemaElement** aSchemaElement)
{
  nsresult rv = NS_OK;
  nsCOMPtr<nsISchemaElement> schemaElement;
  PRUint32 minOccurs, maxOccurs;

  aContext->PushNamespaceDecls(aElement);

  // See if it's a reference or an actual element declaration
  nsAutoString ref;
  aElement->GetAttribute(NS_LITERAL_STRING("ref"), ref);
  if (ref.Length() > 0) {
    nsSchemaElementRef* elementRef;
    
    elementRef = new nsSchemaElementRef(aSchema, ref);
    if (!elementRef) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    schemaElement = elementRef;

    GetMinAndMax(aElement, &minOccurs, &maxOccurs);
    elementRef->SetMinOccurs(minOccurs);
    elementRef->SetMaxOccurs(maxOccurs);
  }
  else {
    nsAutoString name;
    nsSchemaElement* elementInst;

    aElement->GetAttribute(NS_LITERAL_STRING("name"), name);
    elementInst = new nsSchemaElement(aSchema, name);
    if (!elementInst) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    schemaElement = elementInst;

    GetMinAndMax(aElement, &minOccurs, &maxOccurs);
    elementInst->SetMinOccurs(minOccurs);
    elementInst->SetMaxOccurs(maxOccurs);

    nsAutoString defaultValue, fixedValue;
    aElement->GetAttribute(NS_LITERAL_STRING("defaultValue"), defaultValue);
    aElement->GetAttribute(NS_LITERAL_STRING("fixedValue"), fixedValue);
    elementInst->SetConstraints(defaultValue, fixedValue);

    nsAutoString nillable, abstract;
    aElement->GetAttribute(NS_LITERAL_STRING("nillable"), nillable);
    aElement->GetAttribute(NS_LITERAL_STRING("abstract"), abstract);
    elementInst->SetFlags(nillable.Equals(NS_LITERAL_STRING("true")),
                          abstract.Equals(NS_LITERAL_STRING("true")));

    nsCOMPtr<nsISchemaType> schemaType;
    nsAutoString typeStr;
    aElement->GetAttribute(NS_LITERAL_STRING("type"), typeStr);
    if (typeStr.Length() > 0) {
      rv = GetNewOrUsedType(aSchema, aContext, typeStr, 
                            getter_AddRefs(schemaType));
      if (NS_FAILED(rv)) {
        return rv;
      }
    }
    // Look for the type as a child of the element 
    else {
      nsCOMPtr<nsIDOMNodeList> childNodes;
      aElement->GetChildNodes(getter_AddRefs(childNodes));
      
      // An element must have a type
      if (!childNodes) {
        return NS_ERROR_FAILURE;
      }

      PRUint32 i, count;
      
      childNodes->GetLength(&count);
      for (i = 0; i < count; i++) {
        nsCOMPtr<nsIDOMNode> child;
        
        childNodes->Item(i, getter_AddRefs(child));
        nsCOMPtr<nsIDOMElement> childElement(do_QueryInterface(child));
        if (!childElement) {
          continue;
        }

        nsAutoString namespaceURI;           
        childElement->GetNamespaceURI(namespaceURI);

        if (namespaceURI.Equals(NS_LITERAL_STRING(NS_SCHEMA_NAMESPACE))) {

          nsCOMPtr<nsIContent> content(do_QueryInterface(childElement));
          NS_ASSERTION(content, "Element is not content");
          if (!content) {
            return NS_ERROR_FAILURE;
          }
          
          nsCOMPtr<nsINodeInfo> nodeInfo;
          content->GetNodeInfo(*getter_AddRefs(nodeInfo));
          if (!nodeInfo) {
            return NS_ERROR_FAILURE;
          }

          nsCOMPtr<nsIAtom> tagName;
          nodeInfo->GetNameAtom(*getter_AddRefs(tagName));
          
          if (tagName == nsSchemaAtoms::sSimpleType_atom) {
            nsCOMPtr<nsISchemaSimpleType> simpleType;
            
            rv = ProcessSimpleType(aSchema, aContext, childElement,
                                   getter_AddRefs(simpleType));
            if (NS_FAILED(rv)) {
              return rv;
            }
            schemaType = simpleType;
            break;
          }
          else if (tagName == nsSchemaAtoms::sComplexType_atom) {
            nsCOMPtr<nsISchemaComplexType> complexType;
            
            rv = ProcessComplexType(aSchema, aContext, childElement,
                                    getter_AddRefs(complexType));
            if (NS_FAILED(rv)) {
              return rv;
            }
            schemaType = complexType;
            break;
          }
        }
      }
    }

    rv = elementInst->SetType(schemaType);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  *aSchemaElement = schemaElement;
  NS_ADDREF(*aSchemaElement);

  aContext->PopNamespaceDecls(aElement);

  return NS_OK;
}

nsresult 
nsSchemaLoader::ProcessComplexType(nsSchema* aSchema, 
                                   nsSchemaLoadingContext* aContext,
                                   nsIDOMElement* aElement,
                                   nsISchemaComplexType** aComplexType)
{
  nsresult rv = NS_OK;
  nsCOMPtr<nsISchemaComplexType> complexType;

  aContext->PushNamespaceDecls(aElement);

  nsAutoString abstract, name;
  aElement->GetAttribute(NS_LITERAL_STRING("abstract"), abstract);
  aElement->GetAttribute(NS_LITERAL_STRING("name"), name);

  nsSchemaComplexType* typeInst;
  typeInst = new nsSchemaComplexType(aSchema, name, 
                                     abstract.Equals(NS_LITERAL_STRING("true")));
  if (!typeInst) {
    return NS_ERROR_FAILURE;
  }
  complexType = typeInst;


  nsCOMPtr<nsIDOMNodeList> childNodes;
  aElement->GetChildNodes(getter_AddRefs(childNodes));
  
  PRUint16 contentModel = nsISchemaComplexType::CONTENT_MODEL_EMPTY;
  PRUint16 derivation = nsISchemaComplexType::DERIVATION_SELF_CONTAINED;
  nsCOMPtr<nsISchemaType> baseType;
  nsCOMPtr<nsISchemaModelGroup> modelGroup;
  
  PRUint32 i, count; 
  childNodes->GetLength(&count);

  for (i = 0; i < count; i++) {
    nsCOMPtr<nsIDOMNode> child;
    
    childNodes->Item(i, getter_AddRefs(child));
    nsCOMPtr<nsIDOMElement> childElement(do_QueryInterface(child));
    if (!childElement) {
      continue;
    }

    nsAutoString namespaceURI;           
    childElement->GetNamespaceURI(namespaceURI);
    
    if (namespaceURI.Equals(NS_LITERAL_STRING(NS_SCHEMA_NAMESPACE))) {
      
      nsCOMPtr<nsIContent> content(do_QueryInterface(childElement));
      NS_ASSERTION(content, "Element is not content");
      if (!content) {
        return NS_ERROR_FAILURE;
      }
      
      nsCOMPtr<nsINodeInfo> nodeInfo;
      content->GetNodeInfo(*getter_AddRefs(nodeInfo));
      if (!nodeInfo) {
        return NS_ERROR_FAILURE;
      }

      nsCOMPtr<nsIAtom> tagName;
      nodeInfo->GetNameAtom(*getter_AddRefs(tagName));
      
      if (tagName == nsSchemaAtoms::sSimpleContent_atom) {
        contentModel = nsISchemaComplexType::CONTENT_MODEL_SIMPLE;

        rv = ProcessSimpleContent(aSchema, aContext,
                                  childElement, typeInst,
                                  &derivation, getter_AddRefs(baseType));
        break;
      }
      else if (tagName == nsSchemaAtoms::sComplexContent_atom) {       
        rv = ProcessComplexContent(aSchema, aContext,
                                   childElement, typeInst, 
                                   &contentModel, &derivation,
                                   getter_AddRefs(baseType));
        break;                                   
      }
      else if (tagName != nsSchemaAtoms::sAnnotation_atom) {
        rv = ProcessComplexTypeBody(aSchema, aContext,
                                    aElement, typeInst, nsnull,
                                    &contentModel);
        break;
      }
    }
  }

  if (NS_FAILED(rv)) {
    return rv;
  }

  nsAutoString mixed;
  aElement->GetAttribute(NS_LITERAL_STRING("mixed"), mixed);
  if (mixed.Equals(NS_LITERAL_STRING("true"))) {
    contentModel = nsISchemaComplexType::CONTENT_MODEL_MIXED;
  }

  typeInst->SetContentModel(contentModel);
  typeInst->SetDerivation(derivation, baseType);

  aContext->PopNamespaceDecls(aElement);
  return NS_OK;
}

nsresult
nsSchemaLoader::ProcessComplexTypeBody(nsSchema* aSchema, 
                                       nsSchemaLoadingContext* aContext,
                                       nsIDOMElement* aElement,
                                       nsSchemaComplexType* aComplexType,
                                       nsSchemaModelGroup* aSequence,
                                       PRUint16* aContentModel)
{
  nsresult rv = NS_OK;
  nsCOMPtr<nsIDOMNodeList> childNodes;
  aElement->GetChildNodes(getter_AddRefs(childNodes));

  *aContentModel = nsISchemaComplexType::CONTENT_MODEL_EMPTY;

  nsCOMPtr<nsISchemaType> baseType;
  nsCOMPtr<nsISchemaModelGroup> modelGroup;
  
  PRUint32 i, count; 
  childNodes->GetLength(&count);

  for (i = 0; i < count; i++) {
    nsCOMPtr<nsIDOMNode> child;
    
    childNodes->Item(i, getter_AddRefs(child));
    nsCOMPtr<nsIDOMElement> childElement(do_QueryInterface(child));
    if (!childElement) {
      continue;
    }

    nsAutoString namespaceURI;           
    childElement->GetNamespaceURI(namespaceURI);
    
    if (namespaceURI.Equals(NS_LITERAL_STRING(NS_SCHEMA_NAMESPACE))) {
      
      nsCOMPtr<nsIContent> content(do_QueryInterface(childElement));
      NS_ASSERTION(content, "Element is not content");
      if (!content) {
        return NS_ERROR_FAILURE;
      }
      
      nsCOMPtr<nsINodeInfo> nodeInfo;
      content->GetNodeInfo(*getter_AddRefs(nodeInfo));
      if (!nodeInfo) {
        return NS_ERROR_FAILURE;
      }

      nsCOMPtr<nsIAtom> tagName;
      nodeInfo->GetNameAtom(*getter_AddRefs(tagName));
      
      if ((tagName == nsSchemaAtoms::sModelGroup_atom) ||
          (tagName == nsSchemaAtoms::sAll_atom) ||
          (tagName == nsSchemaAtoms::sChoice_atom) || 
          (tagName == nsSchemaAtoms::sSequence_atom)) {
        // We shouldn't already have a model group
        if (modelGroup) {
          return NS_ERROR_FAILURE;
        }

        rv = ProcessModelGroup(aSchema, aContext,
                               childElement, 
                               getter_AddRefs(modelGroup));
        if (NS_FAILED(rv)) {
          return rv;
        }

        PRUint32 particleCount;
        modelGroup->GetParticleCount(&particleCount);
        if (particleCount) {
          *aContentModel = nsISchemaComplexType::CONTENT_MODEL_ELEMENT_ONLY;
        }
        else {
          PRUint16 compositor;
          modelGroup->GetCompositor(&compositor);
          
          PRUint32 minOccurs;
          modelGroup->GetMinOccurs(&minOccurs);

          if ((compositor == nsISchemaModelGroup::COMPOSITOR_CHOICE) &&
              (minOccurs > 0)) {
            *aContentModel = nsISchemaComplexType::CONTENT_MODEL_ELEMENT_ONLY;
          }
        }

        if (aSequence) {
          rv = aSequence->AddParticle(modelGroup);
        }
        else {
          rv = aComplexType->SetModelGroup(modelGroup);
        }
        if (NS_FAILED(rv)) {
          return rv;
        }        
      }
      else if ((tagName == nsSchemaAtoms::sAttribute_atom) ||
               (tagName == nsSchemaAtoms::sAttributeGroup_atom) ||
               (tagName == nsSchemaAtoms::sAnyAttribute_atom)) {
        nsCOMPtr<nsISchemaAttributeComponent> attribute;
        
        rv = ProcessAttributeComponent(aSchema, aContext,
                                       childElement, 
                                       getter_AddRefs(attribute));
        if (NS_FAILED(rv)) {
          return rv;
        }

        rv = aComplexType->AddAttribute(attribute);
        if (NS_FAILED(rv)) {
          return rv;
        }
      }
    }
  }

  return rv;
}

nsresult 
nsSchemaLoader::ProcessSimpleType(nsSchema* aSchema, 
                                  nsSchemaLoadingContext* aContext,
                                  nsIDOMElement* aElement,
                                  nsISchemaSimpleType** aSimpleType)
{
  aContext->PushNamespaceDecls(aElement);
  aContext->PopNamespaceDecls(aElement);
  return NS_OK;
}

nsresult 
nsSchemaLoader::ProcessAttribute(nsSchema* aSchema, 
                                 nsSchemaLoadingContext* aContext,
                                 nsIDOMElement* aElement,
                                 nsISchemaAttribute** aAttribute)
{
  aContext->PushNamespaceDecls(aElement);
  aContext->PopNamespaceDecls(aElement);
  return NS_OK;
}

nsresult 
nsSchemaLoader::ProcessAttributeGroup(nsSchema* aSchema, 
                                      nsSchemaLoadingContext* aContext,
                                      nsIDOMElement* aElement,
                                      nsISchemaAttributeGroup** aAttributeGroup)
{
  aContext->PushNamespaceDecls(aElement);
  aContext->PopNamespaceDecls(aElement);
  return NS_OK;
}

nsresult 
nsSchemaLoader::ProcessModelGroup(nsSchema* aSchema, 
                                  nsSchemaLoadingContext* aContext,
                                  nsIDOMElement* aElement,
                                  nsISchemaModelGroup** aModelGroup)
{
  aContext->PushNamespaceDecls(aElement);
  aContext->PopNamespaceDecls(aElement);
  return NS_OK;
}

nsresult 
nsSchemaLoader::ProcessSimpleContent(nsSchema* aSchema, 
                                     nsSchemaLoadingContext* aContext,
                                     nsIDOMElement* aElement,
                                     nsSchemaComplexType* aComplexType,
                                     PRUint16* aDerivation,
                                     nsISchemaType** aBaseType)
{
  nsresult rv = NS_OK;
  aContext->PushNamespaceDecls(aElement);

  nsCOMPtr<nsISchemaType> baseType;
  nsCOMPtr<nsIDOMNodeList> childNodes;
  aElement->GetChildNodes(getter_AddRefs(childNodes));
  
  // A simpleContent element must have children
  if (!childNodes) {
    return NS_ERROR_FAILURE;
  }

  PRUint32 i, count;
      
  childNodes->GetLength(&count);
  for (i = 0; i < count; i++) {
    nsCOMPtr<nsIDOMNode> child;
    
    childNodes->Item(i, getter_AddRefs(child));
    nsCOMPtr<nsIDOMElement> childElement(do_QueryInterface(child));
    if (!childElement) {
      continue;
    }
    
    nsAutoString namespaceURI;           
    childElement->GetNamespaceURI(namespaceURI);
    
    if (namespaceURI.Equals(NS_LITERAL_STRING(NS_SCHEMA_NAMESPACE))) {
      
      nsCOMPtr<nsIContent> content(do_QueryInterface(childElement));
      NS_ASSERTION(content, "Element is not content");
      if (!content) {
        return NS_ERROR_FAILURE;
      }
      
      nsCOMPtr<nsINodeInfo> nodeInfo;
      content->GetNodeInfo(*getter_AddRefs(nodeInfo));
      if (!nodeInfo) {
        return NS_ERROR_FAILURE;
      }
      
      nsCOMPtr<nsIAtom> tagName;
      nodeInfo->GetNameAtom(*getter_AddRefs(tagName));
        
      nsAutoString baseStr;
      if ((tagName == nsSchemaAtoms::sRestriction_atom) ||
          (tagName == nsSchemaAtoms::sExtension_atom)) {
        childElement->GetAttribute(NS_LITERAL_STRING("base"), baseStr);
        if (baseStr.Length() == 0) {
          return NS_ERROR_FAILURE;
        }
        
        rv = GetNewOrUsedType(aSchema, aContext, baseStr, 
                              getter_AddRefs(baseType));
        if (NS_FAILED(rv)) {
          return rv;
        }

        nsCOMPtr<nsISchemaSimpleType> simpleBaseType;
        if (tagName == nsSchemaAtoms::sRestriction_atom) {
          *aDerivation = nsISchemaComplexType::DERIVATION_RESTRICTION_SIMPLE;
          rv = ProcessSimpleContentRestriction(aSchema, aContext, childElement,
                                               aComplexType, baseType,
                                               getter_AddRefs(simpleBaseType));
          if (NS_FAILED(rv)) {
            return rv;
          }
        }
        else {
          *aDerivation = nsISchemaComplexType::DERIVATION_EXTENSION_SIMPLE;

          nsCOMPtr<nsISchemaComplexType> complexBaseType(do_QueryInterface(baseType));
          if (complexBaseType) {
            // Copy over the attributes from the base type
            // XXX Should really be cloning
            PRUint32 attrIndex, attrCount;
            complexBaseType->GetAttributeCount(&attrCount);

            for (attrIndex = 0; attrIndex < attrCount; attrIndex++) {
              nsCOMPtr<nsISchemaAttributeComponent> attribute;
              
              rv = complexBaseType->GetAttributeByIndex(attrIndex,
                                                        getter_AddRefs(attribute));
              if (NS_FAILED(rv)) {
                return rv;
              }
              aComplexType->AddAttribute(attribute);
            }
          }

          rv = ProcessSimpleContentExtension(aSchema, aContext, childElement,
                                             aComplexType, baseType,
                                             getter_AddRefs(simpleBaseType));
          if (NS_FAILED(rv)) {
            return rv;
          }
        }

        if (simpleBaseType) {
          rv = aComplexType->SetSimpleBaseType(simpleBaseType);
          if (NS_FAILED(rv)) {
            return rv;
          }
        }
        break;
      }
    }
  }

  *aBaseType = baseType;
  NS_IF_ADDREF(*aBaseType);

  aContext->PopNamespaceDecls(aElement);
  return NS_OK;
}

nsresult 
nsSchemaLoader::ProcessSimpleContentRestriction(nsSchema* aSchema, 
                                                nsSchemaLoadingContext* aContext, 
                                                nsIDOMElement* aElement,
                                                nsSchemaComplexType* aComplexType, 
                                                nsISchemaType* aBaseType,
                                                nsISchemaSimpleType** aSimpleBaseType)
{
  aContext->PushNamespaceDecls(aElement);
  aContext->PopNamespaceDecls(aElement);
  return NS_OK;
}
 
nsresult 
nsSchemaLoader::ProcessSimpleContentExtension(nsSchema* aSchema, 
                                              nsSchemaLoadingContext* aContext, 
                                              nsIDOMElement* aElement,
                                              nsSchemaComplexType* aComplexType,
                                              nsISchemaType* aBaseType,
                                              nsISchemaSimpleType** aSimpleBaseType)
{
  aContext->PushNamespaceDecls(aElement);
  aContext->PopNamespaceDecls(aElement);
  return NS_OK;
}
 
nsresult 
nsSchemaLoader::ProcessComplexContent(nsSchema* aSchema, 
                                      nsSchemaLoadingContext* aContext,
                                      nsIDOMElement* aElement,
                                      nsSchemaComplexType* aComplexType,
                                      PRUint16* aContentModel,
                                      PRUint16* aDerivation,
                                      nsISchemaType** aBaseType)
{
  nsresult rv = NS_OK;
  aContext->PushNamespaceDecls(aElement);

  nsCOMPtr<nsISchemaType> baseType;
  nsCOMPtr<nsIDOMNodeList> childNodes;
  aElement->GetChildNodes(getter_AddRefs(childNodes));
  
  // A complexContent element must have children
  if (!childNodes) {
    return NS_ERROR_FAILURE;
  }

  PRUint32 i, count;
      
  childNodes->GetLength(&count);
  for (i = 0; i < count; i++) {
    nsCOMPtr<nsIDOMNode> child;
    
    childNodes->Item(i, getter_AddRefs(child));
    nsCOMPtr<nsIDOMElement> childElement(do_QueryInterface(child));
    if (!childElement) {
      continue;
    }
    
    nsAutoString namespaceURI;           
    childElement->GetNamespaceURI(namespaceURI);
    
    if (namespaceURI.Equals(NS_LITERAL_STRING(NS_SCHEMA_NAMESPACE))) {
      
      nsCOMPtr<nsIContent> content(do_QueryInterface(childElement));
      NS_ASSERTION(content, "Element is not content");
      if (!content) {
        return NS_ERROR_FAILURE;
      }
      
      nsCOMPtr<nsINodeInfo> nodeInfo;
      content->GetNodeInfo(*getter_AddRefs(nodeInfo));
      if (!nodeInfo) {
        return NS_ERROR_FAILURE;
      }
      
      nsCOMPtr<nsIAtom> tagName;
      nodeInfo->GetNameAtom(*getter_AddRefs(tagName));
        
      nsAutoString baseStr;
      if ((tagName == nsSchemaAtoms::sRestriction_atom) ||
          (tagName == nsSchemaAtoms::sExtension_atom)) {
        childElement->GetAttribute(NS_LITERAL_STRING("base"), baseStr);
        if (baseStr.Length() == 0) {
          return NS_ERROR_FAILURE;
        }
        
        rv = GetNewOrUsedType(aSchema, aContext, baseStr, 
                              getter_AddRefs(baseType));
        if (NS_FAILED(rv)) {
          return rv;
        }

        if (tagName == nsSchemaAtoms::sRestriction_atom) {
          *aDerivation = nsISchemaComplexType::DERIVATION_RESTRICTION_COMPLEX;
          rv = ProcessComplexTypeBody(aSchema, aContext, childElement,
                                      aComplexType, nsnull, aContentModel);
        }
        else {
          *aDerivation = nsISchemaComplexType::DERIVATION_EXTENSION_COMPLEX;

          nsCOMPtr<nsISchemaModelGroup> sequence;
          nsSchemaModelGroup* sequenceInst = nsnull;
          nsCOMPtr<nsISchemaComplexType> complexBaseType(do_QueryInterface(baseType));
          if (complexBaseType) {
            // XXX Should really be cloning
            nsCOMPtr<nsISchemaModelGroup> baseGroup;
            rv = complexBaseType->GetModelGroup(getter_AddRefs(baseGroup));
            if (NS_FAILED(rv)) {
              return rv;
            }

            if (baseGroup) {
              // Create a new model group that's going to be the a sequence
              // of the base model group and the content below
              sequenceInst = new nsSchemaModelGroup(aSchema,
                                                    NS_LITERAL_STRING(""),
                                                    nsISchemaModelGroup::COMPOSITOR_SEQUENCE);
              if (!sequenceInst) {
                return NS_ERROR_OUT_OF_MEMORY;
              }
              sequence = sequenceInst;
              sequenceInst->AddParticle(baseGroup);

              aComplexType->SetModelGroup(sequence);
            }            

            
            // Copy over the attributes from the base type
            // XXX Should really be cloning
            PRUint32 attrIndex, attrCount;
            complexBaseType->GetAttributeCount(&attrCount);

            for (attrIndex = 0; attrIndex < attrCount; attrIndex++) {
              nsCOMPtr<nsISchemaAttributeComponent> attribute;
              
              rv = complexBaseType->GetAttributeByIndex(attrIndex,
                                                        getter_AddRefs(attribute));
              if (NS_FAILED(rv)) {
                return rv;
              }
              aComplexType->AddAttribute(attribute);
            }
          }

          PRUint16 explicitContent;
          rv = ProcessComplexTypeBody(aSchema, aContext, childElement,
                                      aComplexType, sequenceInst,
                                      &explicitContent);
          if (NS_FAILED(rv)) {
            return rv;
          }
          // If the explicit content is empty, get the content type
          // from the base
          if ((explicitContent == nsISchemaComplexType::CONTENT_MODEL_EMPTY) &&
              complexBaseType) {
            complexBaseType->GetContentModel(aContentModel);
          }
        }
        break;
      }
    }
  }

  nsAutoString mixed;
  aElement->GetAttribute(NS_LITERAL_STRING("mixed"), mixed);
  if (mixed.Equals(NS_LITERAL_STRING("true"))) {
    *aContentModel = nsISchemaComplexType::CONTENT_MODEL_MIXED;
  }

  *aBaseType = baseType;
  NS_IF_ADDREF(*aBaseType);

  aContext->PopNamespaceDecls(aElement);
  return NS_OK;
}
 
nsresult 
nsSchemaLoader::ProcessAttributeComponent(nsSchema* aSchema, 
                                          nsSchemaLoadingContext* aContext,
                                          nsIDOMElement* aElement,
                                          nsISchemaAttributeComponent** aAttribute)
{
  aContext->PushNamespaceDecls(aElement);
  aContext->PopNamespaceDecls(aElement);
  return NS_OK;
}

void
nsSchemaLoader::GetMinAndMax(nsIDOMElement* aElement,
                             PRUint32* aMinOccurs,
                             PRUint32* aMaxOccurs)
{
  *aMinOccurs = 1;
  *aMaxOccurs = 1;

  nsAutoString minStr, maxStr;
  aElement->GetAttribute(NS_LITERAL_STRING("minOccurs"), minStr);
  aElement->GetAttribute(NS_LITERAL_STRING("maxOccurs"), maxStr);
  
  PRInt32 rv;
  if (minStr.Length() > 0) {
    PRInt32 minVal = minStr.ToInteger(&rv);
    if (NS_SUCCEEDED(rv) && (minVal >= 0)) {
      *aMinOccurs = (PRUint32)minVal;
    }
  }

  if (maxStr.Length() > 0) {
    if (maxStr.Equals(NS_LITERAL_STRING("unbounded"))) {
      *aMaxOccurs = nsISchemaParticle::OCCURRENCE_UNBOUNDED;
    }
    else {
      PRInt32 maxVal = maxStr.ToInteger(&rv);
      if (NS_SUCCEEDED(rv) && (maxVal >= 0)) {
        *aMaxOccurs = (PRUint32)maxVal;
      }
    }
  }
}

////////////////////////////////////////////////////////////
//
// nsSchemaLoadingContext implementation
//
////////////////////////////////////////////////////////////

typedef struct {
  nsString mPrefix;
  nsString mURI;
  nsIDOMElement* mOwner;  // Weak
} NamespaceDecl;

nsSchemaLoadingContext::nsSchemaLoadingContext()
{
}

nsSchemaLoadingContext::~nsSchemaLoadingContext()
{
  PRInt32 i, count = mNamespaceStack.Count();
  for (i = 0; i < count; i++) {
    NamespaceDecl* decl = (NamespaceDecl*)mNamespaceStack.ElementAt(i);
    delete decl;
  }
}

nsresult 
nsSchemaLoadingContext::PushNamespaceDecls(nsIDOMElement* aElement)
{
  nsCOMPtr<nsIContent> content(do_QueryInterface(aElement));
  NS_ASSERTION(content, "Element is not content");
  if (!content) {
    return NS_ERROR_FAILURE;
  }

  PRInt32 i, count;
  content->GetAttributeCount(count);
  for (i = 0; i < count; i++) {
    nsCOMPtr<nsIAtom> attrName, attrPrefix;
    PRInt32 namespaceID;
    nsAutoString nameStr, uriStr;

    content->GetAttributeNameAt(i, namespaceID,
                                *getter_AddRefs(attrName),
                                *getter_AddRefs(attrPrefix));

    if (namespaceID == kNameSpaceID_XMLNS) {
      content->GetAttribute(namespaceID, attrName, uriStr);
      
      NamespaceDecl* decl = new NamespaceDecl();
      if (!decl) {
        return NS_ERROR_FAILURE;
      }

      // If we had a xmlns: prefix, use the attribute name
      // as the prefix for the namespace declaration.
      if (attrPrefix) {
        attrName->ToString(decl->mPrefix);
      }        
      // Otherwise we're talking about a default namespace 
      // declaration, with no prefix.

      decl->mURI.Assign(uriStr);
      decl->mOwner = aElement;
      mNamespaceStack.AppendElement((void*)decl);
    }
  }

  return NS_OK;
}

nsresult
nsSchemaLoadingContext::PopNamespaceDecls(nsIDOMElement* aElement)
{
  PRInt32 i, count;

  count = mNamespaceStack.Count();
  for (i = count - 1; i >= 0; i--) {
    NamespaceDecl* decl = (NamespaceDecl*)mNamespaceStack.ElementAt(i);
    if (decl->mOwner != aElement) {
      break;
    }
    mNamespaceStack.RemoveElementAt(i);
    delete decl;
  }

  return NS_OK;
}

PRBool 
nsSchemaLoadingContext::GetNamespaceURIForPrefix(const nsAReadableString& aPrefix,
                                                 nsAWritableString& aURI)
{
  PRInt32 i, count;

  count = mNamespaceStack.Count();
  for (i = count - 1; i >= 0; i--) {
    NamespaceDecl* decl = (NamespaceDecl*)mNamespaceStack.ElementAt(i);

    if (decl->mPrefix.Equals(aPrefix)) {
      aURI.Assign(decl->mURI);
      return PR_TRUE;
    }
  }

  return PR_FALSE;
}
