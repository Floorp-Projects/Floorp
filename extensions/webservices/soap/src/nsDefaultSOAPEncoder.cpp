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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include "nsDefaultSOAPEncoder.h"
#include "nsSOAPUtils.h"
#include "nsSOAPParameter.h"
#include "nsISOAPAttachments.h"
#include "nsXPIDLString.h"
#include "nsIDOMDocument.h"
#include "nsIDOMText.h"
#include "nsCOMPtr.h"
#include "nsISchema.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsXPCOM.h"
#include "nsISupportsPrimitives.h"
#include "nsIDOMParser.h"
#include "nsSOAPUtils.h"
#include "nsISOAPEncoding.h"
#include "nsISOAPEncoder.h"
#include "nsISOAPDecoder.h"
#include "nsISOAPMessage.h"
#include "nsSOAPException.h"
#include "prprf.h"
#include "prdtoa.h"
#include "plbase64.h"
#include "prmem.h"
#include "nsReadableUtils.h"
#include "nsIDOMNamedNodeMap.h"
#include "nsIDOMAttr.h"
#include "nsPrintfCString.h"
#include "nsISOAPPropertyBagMutator.h"
#include "nsIPropertyBag.h"
#include "nsSupportsArray.h"


#define MAX_ARRAY_DIMENSIONS 100

//
// Macros to declare and implement the default encoder classes
//

#define DECLARE_ENCODER(name) \
class ns##name##Encoder : \
  public nsISOAPEncoder, \
  public nsISOAPDecoder \
{\
public:\
  ns##name##Encoder();\
  ns##name##Encoder(PRUint16 aSOAPVersion);\
  virtual ~ns##name##Encoder();\
  PRUint16 mSOAPVersion;\
  NS_DECL_ISUPPORTS\
  NS_DECL_NSISOAPENCODER\
  NS_DECL_NSISOAPDECODER\
};\
NS_IMPL_ISUPPORTS2(ns##name##Encoder,nsISOAPEncoder,nsISOAPDecoder) \
ns##name##Encoder::ns##name##Encoder(PRUint16 aSOAPVersion) {mSOAPVersion=aSOAPVersion;}\
ns##name##Encoder::~ns##name##Encoder() {}

// All encoders must be first declared and then registered.
    DECLARE_ENCODER(Default)
    DECLARE_ENCODER(AnyType)
    DECLARE_ENCODER(AnySimpleType)
    DECLARE_ENCODER(Array)
    DECLARE_ENCODER(Struct)
    DECLARE_ENCODER(String)
    DECLARE_ENCODER(Boolean)
    DECLARE_ENCODER(Double)
    DECLARE_ENCODER(Float)
    DECLARE_ENCODER(Long)
    DECLARE_ENCODER(Int)
    DECLARE_ENCODER(Short)
    DECLARE_ENCODER(Byte)
    DECLARE_ENCODER(UnsignedLong)
    DECLARE_ENCODER(UnsignedInt)
    DECLARE_ENCODER(UnsignedShort) 
    DECLARE_ENCODER(UnsignedByte)
    DECLARE_ENCODER(Base64Binary)

/**
 * This now separates the version with respect to the SOAP specification from the version
 * with respect to the schema version (using the same constants for both).  This permits
 * a user of a SOAP 1.1 or 1.2 encoding to choose which encoding to encode to.
 */
#define REGISTER_ENCODER(name,type,uri) \
{\
  ns##name##Encoder *handler = new ns##name##Encoder(version);\
  nsAutoString encodingKey;\
  SOAPEncodingKey(uri, gSOAPStrings->k##name##type##Type, encodingKey);\
  SetEncoder(encodingKey, handler); \
  SetDecoder(encodingKey, handler); \
}

#define REGISTER_SCHEMA_ENCODER(name) REGISTER_ENCODER(name,Schema,gSOAPStrings->kXSURI)
#define REGISTER_SOAP_ENCODER(name) REGISTER_ENCODER(name,SOAP,gSOAPStrings->kSOAPEncURI)

#define REGISTER_ENCODERS \
  {\
    nsDefaultEncoder *handler = new nsDefaultEncoder(version);\
    SetDefaultEncoder(handler);\
    SetDefaultDecoder(handler);\
  }\
  REGISTER_SCHEMA_ENCODER(AnyType) \
  REGISTER_SCHEMA_ENCODER(AnySimpleType) \
  REGISTER_SOAP_ENCODER(Array)\
  REGISTER_SOAP_ENCODER(Struct)\
  REGISTER_SCHEMA_ENCODER(String)\
  REGISTER_SCHEMA_ENCODER(Boolean)\
  REGISTER_SCHEMA_ENCODER(Double)\
  REGISTER_SCHEMA_ENCODER(Float)\
  REGISTER_SCHEMA_ENCODER(Long)\
  REGISTER_SCHEMA_ENCODER(Int)\
  REGISTER_SCHEMA_ENCODER(Short)\
  REGISTER_SCHEMA_ENCODER(Byte)\
  REGISTER_SCHEMA_ENCODER(UnsignedLong)\
  REGISTER_SCHEMA_ENCODER(UnsignedInt)\
  REGISTER_SCHEMA_ENCODER(UnsignedShort)\
  REGISTER_SCHEMA_ENCODER(UnsignedByte)\
  REGISTER_SCHEMA_ENCODER(Base64Binary)

//
// Default SOAP Encodings
//

NS_IMPL_ADDREF(nsDefaultSOAPEncoding_1_1)
NS_IMPL_RELEASE(nsDefaultSOAPEncoding_1_1)

nsDefaultSOAPEncoding_1_1::nsDefaultSOAPEncoding_1_1() 
: nsSOAPEncoding(gSOAPStrings->kSOAPEncURI11, nsnull, nsnull)
{
  PRUint16 version = nsISOAPMessage::VERSION_1_1;
  PRBool result;
  MapSchemaURI(gSOAPStrings->kXSURI1999,gSOAPStrings->kXSURI,PR_TRUE,&result);
  MapSchemaURI(gSOAPStrings->kXSIURI1999,gSOAPStrings->kXSIURI,PR_TRUE,&result);
  MapSchemaURI(gSOAPStrings->kSOAPEncURI11,gSOAPStrings->kSOAPEncURI,PR_TRUE,&result);
  REGISTER_ENCODERS
}

NS_IMPL_ADDREF(nsDefaultSOAPEncoding_1_2)
NS_IMPL_RELEASE(nsDefaultSOAPEncoding_1_2)

nsDefaultSOAPEncoding_1_2::nsDefaultSOAPEncoding_1_2() 
: nsSOAPEncoding(gSOAPStrings->kSOAPEncURI, nsnull, nsnull)
{
  PRUint16 version = nsISOAPMessage::VERSION_1_2;
  PRBool result;
  MapSchemaURI(gSOAPStrings->kXSURI1999,gSOAPStrings->kXSURI,PR_FALSE,&result);
  MapSchemaURI(gSOAPStrings->kXSIURI1999,gSOAPStrings->kXSIURI,PR_FALSE,&result);
  MapSchemaURI(gSOAPStrings->kSOAPEncURI11,gSOAPStrings->kSOAPEncURI,PR_FALSE,&result);
  REGISTER_ENCODERS
}

//
// Default Encoders -- static helper functions intermixed
//

//  Getting the immediate supertype of any type
static nsresult GetSupertype(nsISOAPEncoding * aEncoding, nsISchemaType* aType, nsISchemaType** _retval)
{
  PRUint16 typevalue;
  nsresult rc = aType->GetSchemaType(&typevalue);
  if (NS_FAILED(rc))
    return rc;
  nsCOMPtr<nsISchemaType> base;
  nsAutoString name;
  switch (typevalue) {
    case nsISchemaType::SCHEMA_TYPE_COMPLEX:
    {
      nsCOMPtr<nsISchemaComplexType> type =
        do_QueryInterface(aType);
      rc = type->GetBaseType(getter_AddRefs(base));
      if (NS_FAILED(rc))
        return rc;
      break;
    }
    case nsISchemaType::SCHEMA_TYPE_SIMPLE:
    {
      nsCOMPtr<nsISchemaSimpleType> type =
        do_QueryInterface(aType);
      PRUint16 simpletypevalue;
      rc = type->GetSimpleType(&simpletypevalue);
      if (NS_FAILED(rc))
        return rc;
      switch (simpletypevalue) {
        //  Ultimately, this is wrong because in XML types are value spaces
        //  We have not handled unions and lists.
        //  A union might be considered a supertype of anything it joins
        //  but it is the supertype of all types with value spaces it includes.
        //  SOAP is an attempt to treat XML types as though they were
        //  data types, which are governed by labels instead of value spaces.
        //  So two unrelated values may coexist, but we will disallow it
        //  because the caller probably wants type guarantees, not value
        //  guarantees.
        case nsISchemaSimpleType::SIMPLE_TYPE_RESTRICTION:
        {
          nsCOMPtr<nsISchemaRestrictionType> simpletype =
            do_QueryInterface(type);
          nsCOMPtr<nsISchemaSimpleType> simplebasetype;
          rc = simpletype->GetBaseType(getter_AddRefs(simplebasetype));
          if (NS_FAILED(rc))
            return rc;
          base = simplebasetype;
          break;
        }
        case nsISchemaSimpleType::SIMPLE_TYPE_BUILTIN:
        {
          nsCOMPtr<nsISchemaBuiltinType> builtintype = 
            do_QueryInterface(type);
          PRUint16 builtintypevalue;
          rc = builtintype->GetBuiltinType(&builtintypevalue);
          if (NS_FAILED(rc))
            return rc;
          switch(builtintypevalue) {
            case nsISchemaBuiltinType::BUILTIN_TYPE_ANYTYPE:  //  Root of all types
              *_retval = nsnull;
              return NS_OK;
            case nsISchemaBuiltinType::BUILTIN_TYPE_STRING:
//              name = kAnySimpleTypeSchemaType;
              name = gSOAPStrings->kAnyTypeSchemaType;
              break;
            case nsISchemaBuiltinType::BUILTIN_TYPE_NORMALIZED_STRING:
              name = gSOAPStrings->kStringSchemaType;
              break;
            case nsISchemaBuiltinType::BUILTIN_TYPE_TOKEN:
              name = gSOAPStrings->kNormalizedStringSchemaType;
              break;
            case nsISchemaBuiltinType::BUILTIN_TYPE_BYTE:
              name = gSOAPStrings->kShortSchemaType;
              break;
            case nsISchemaBuiltinType::BUILTIN_TYPE_UNSIGNEDBYTE:
              name = gSOAPStrings->kUnsignedShortSchemaType;
              break;
            case nsISchemaBuiltinType::BUILTIN_TYPE_BASE64BINARY:
//              name = kAnySimpleTypeSchemaType;
              name = gSOAPStrings->kAnyTypeSchemaType;
              break;
            case nsISchemaBuiltinType::BUILTIN_TYPE_HEXBINARY:
//              name = kAnySimpleTypeSchemaType;
              name = gSOAPStrings->kAnyTypeSchemaType;
              break;
            case nsISchemaBuiltinType::BUILTIN_TYPE_INTEGER:
              name = gSOAPStrings->kDecimalSchemaType;
              break;
            case nsISchemaBuiltinType::BUILTIN_TYPE_POSITIVEINTEGER:
              name = gSOAPStrings->kNonNegativeIntegerSchemaType;
              break;
            case nsISchemaBuiltinType::BUILTIN_TYPE_NEGATIVEINTEGER:
              name = gSOAPStrings->kNonPositiveIntegerSchemaType;
              break;
            case nsISchemaBuiltinType::BUILTIN_TYPE_NONNEGATIVEINTEGER:
              name = gSOAPStrings->kIntegerSchemaType;
              break;
            case nsISchemaBuiltinType::BUILTIN_TYPE_NONPOSITIVEINTEGER:
              name = gSOAPStrings->kIntegerSchemaType;
              break;
            case nsISchemaBuiltinType::BUILTIN_TYPE_INT:
              name = gSOAPStrings->kLongSchemaType;
              break;
            case nsISchemaBuiltinType::BUILTIN_TYPE_UNSIGNEDINT:
              name = gSOAPStrings->kUnsignedLongSchemaType;
              break;
            case nsISchemaBuiltinType::BUILTIN_TYPE_LONG:
              name = gSOAPStrings->kIntegerSchemaType;
              break;
            case nsISchemaBuiltinType::BUILTIN_TYPE_UNSIGNEDLONG:
              name = gSOAPStrings->kNonNegativeIntegerSchemaType;
              break;
            case nsISchemaBuiltinType::BUILTIN_TYPE_SHORT:
              name = gSOAPStrings->kIntSchemaType;
              break;
            case nsISchemaBuiltinType::BUILTIN_TYPE_UNSIGNEDSHORT:
              name = gSOAPStrings->kUnsignedIntSchemaType;
              break;
            case nsISchemaBuiltinType::BUILTIN_TYPE_DECIMAL:
//              name = kAnySimpleTypeSchemaType;
              name = gSOAPStrings->kAnyTypeSchemaType;
              break;
            case nsISchemaBuiltinType::BUILTIN_TYPE_FLOAT:
//              name = kAnySimpleTypeSchemaType;
              name = gSOAPStrings->kAnyTypeSchemaType;
              break;
            case nsISchemaBuiltinType::BUILTIN_TYPE_DOUBLE:
//              name = kAnySimpleTypeSchemaType;
              name = gSOAPStrings->kAnyTypeSchemaType;
              break;
            case nsISchemaBuiltinType::BUILTIN_TYPE_BOOLEAN:
//              name = kAnySimpleTypeSchemaType;
              name = gSOAPStrings->kAnyTypeSchemaType;
              break;
            case nsISchemaBuiltinType::BUILTIN_TYPE_TIME:
//              name = kAnySimpleTypeSchemaType;
              name = gSOAPStrings->kAnyTypeSchemaType;
              break;
            case nsISchemaBuiltinType::BUILTIN_TYPE_DATETIME:
//              name = kAnySimpleTypeSchemaType;
              name = gSOAPStrings->kAnyTypeSchemaType;
              break;
            case nsISchemaBuiltinType::BUILTIN_TYPE_DURATION:
//              name = kAnySimpleTypeSchemaType;
              name = gSOAPStrings->kAnyTypeSchemaType;
              break;
            case nsISchemaBuiltinType::BUILTIN_TYPE_DATE:
//              name = kAnySimpleTypeSchemaType;
              name = gSOAPStrings->kAnyTypeSchemaType;
              break;
            case nsISchemaBuiltinType::BUILTIN_TYPE_GMONTH:
//              name = kAnySimpleTypeSchemaType;
              name = gSOAPStrings->kAnyTypeSchemaType;
              break;
            case nsISchemaBuiltinType::BUILTIN_TYPE_GYEAR:
//              name = kAnySimpleTypeSchemaType;
              name = gSOAPStrings->kAnyTypeSchemaType;
              break;
            case nsISchemaBuiltinType::BUILTIN_TYPE_GYEARMONTH:
//              name = kAnySimpleTypeSchemaType;
              name = gSOAPStrings->kAnyTypeSchemaType;
              break;
            case nsISchemaBuiltinType::BUILTIN_TYPE_GDAY:
//              name = kAnySimpleTypeSchemaType;
              name = gSOAPStrings->kAnyTypeSchemaType;
              break;
            case nsISchemaBuiltinType::BUILTIN_TYPE_GMONTHDAY:
//              name = kAnySimpleTypeSchemaType;
              name = gSOAPStrings->kAnyTypeSchemaType;
              break;
            case nsISchemaBuiltinType::BUILTIN_TYPE_NAME:
//              name = kAnySimpleTypeSchemaType;
              name = gSOAPStrings->kAnyTypeSchemaType;
              break;
            case nsISchemaBuiltinType::BUILTIN_TYPE_QNAME:
//              name = kAnySimpleTypeSchemaType;
              name = gSOAPStrings->kAnyTypeSchemaType;
              break;
            case nsISchemaBuiltinType::BUILTIN_TYPE_NCNAME:
              name = gSOAPStrings->kNameSchemaType;
              break;
            case nsISchemaBuiltinType::BUILTIN_TYPE_ANYURI:
//              name = kAnySimpleTypeSchemaType;
              name = gSOAPStrings->kAnyTypeSchemaType;
              break;
            case nsISchemaBuiltinType::BUILTIN_TYPE_LANGUAGE:
              name = gSOAPStrings->kTokenSchemaType;
              break;
            case nsISchemaBuiltinType::BUILTIN_TYPE_ID:
              name = gSOAPStrings->kNCNameSchemaType;
              break;
            case nsISchemaBuiltinType::BUILTIN_TYPE_IDREF:
              name = gSOAPStrings->kNCNameSchemaType;
              break;
            case nsISchemaBuiltinType::BUILTIN_TYPE_IDREFS:
              name = gSOAPStrings->kNormalizedStringSchemaType;  //  Really a list...
              break;
            case nsISchemaBuiltinType::BUILTIN_TYPE_ENTITY:
              name = gSOAPStrings->kNCNameSchemaType;
              break;
            case nsISchemaBuiltinType::BUILTIN_TYPE_ENTITIES:
              name = gSOAPStrings->kNormalizedStringSchemaType;  //  Really a list...
              break;
            case nsISchemaBuiltinType::BUILTIN_TYPE_NOTATION:
//              name = kAnySimpleTypeSchemaType;
              name = gSOAPStrings->kAnyTypeSchemaType;
              break;
            case nsISchemaBuiltinType::BUILTIN_TYPE_NMTOKEN:
              name = gSOAPStrings->kTokenSchemaType;
              break;
            case nsISchemaBuiltinType::BUILTIN_TYPE_NMTOKENS:
              name = gSOAPStrings->kNormalizedStringSchemaType;  //  Really a list...
              break;
          }
        }
      }
      break;
    }
  }
  if (!base) {
    if (name.IsEmpty()) {
      switch (typevalue) {
        case nsISchemaType::SCHEMA_TYPE_COMPLEX:
          name = gSOAPStrings->kAnyTypeSchemaType;
          break;
        default:
//          name = kAnySimpleTypeSchemaType;
          name = gSOAPStrings->kAnyTypeSchemaType;
      }
    }
    nsCOMPtr<nsISchemaCollection> collection;
    rc = aEncoding->GetSchemaCollection(getter_AddRefs(collection));
    if (NS_FAILED(rc))
      return rc;
    rc = collection->GetType(name,
                             gSOAPStrings->kXSURI,
                             getter_AddRefs(base));
//    if (NS_FAILED(rc)) return rc;
  }
  *_retval = base;
  NS_IF_ADDREF(*_retval);
  return NS_OK;
}

static nsresult
EncodeSimpleValue(nsISOAPEncoding * aEncoding,
                  const nsAString & aValue,
                  const nsAString & aNamespaceURI,
                  const nsAString & aName,
                  nsISchemaType * aSchemaType,
                  nsIDOMElement * aDestination, nsIDOMElement ** _retval)
{
  nsresult rc;
  PRBool needType = PR_FALSE;
  nsAutoString typeName;
  nsAutoString typeNS;
  if (aSchemaType) {
    rc = aSchemaType->GetName(typeName);
    if (NS_FAILED(rc))
      return rc;
    rc = aSchemaType->GetTargetNamespace(typeNS);
    if (NS_FAILED(rc))
      return rc;
    needType = (!typeName.IsEmpty()                   &&
                !(typeName.Equals(gSOAPStrings->kAnyTypeSchemaType) && 
                typeNS.Equals(gSOAPStrings->kXSURI)));
  }
  nsAutoString name;      //  First choose the appropriate name and namespace for the element.
  nsAutoString ns;
  if (aName.IsEmpty()) {  //  We automatically choose appropriate element names where none exist.
    // The idea here seems to be to walk up the schema hierarcy to
    // find the base type and use the name of that as the element name.
    ns = gSOAPStrings->kSOAPEncURI;
    nsAutoString currentURI = ns;
    nsCOMPtr<nsISchemaType> currentType = aSchemaType;
    while (currentType
           && !(currentURI.Equals(gSOAPStrings->kXSURI)
                || currentURI.Equals(gSOAPStrings->kSOAPEncURI))) {
      nsCOMPtr<nsISchemaType> supertype;
      rc = GetSupertype(aEncoding, currentType, getter_AddRefs(supertype));
      if (NS_FAILED(rc))
        return rc;
      if (!currentType) {
        break;
      }
      currentType = supertype;
      rc = currentType->GetTargetNamespace(currentURI);
      if (NS_FAILED(rc))
        return rc;
    }
    if (currentType) {
      rc = aSchemaType->GetName(name);
      if (NS_FAILED(rc))
        return rc;
      needType = needType && (currentType != aSchemaType);  //  We may not need type
    }
    else {
      name = gSOAPStrings->kAnyTypeSchemaType;
      needType = PR_FALSE;
    }
    rc = aEncoding->GetExternalSchemaURI(gSOAPStrings->kSOAPEncURI, ns);
  }
  else {
    name = aName;
    rc = aEncoding->GetExternalSchemaURI(aNamespaceURI, ns);
  }
  if (NS_FAILED(rc))
    return rc;
  nsCOMPtr<nsIDOMDocument> document;
  rc = aDestination->GetOwnerDocument(getter_AddRefs(document));
  if (NS_FAILED(rc))
    return rc;
  nsCOMPtr<nsIDOMElement> element;
  rc = document->CreateElementNS(ns, name, getter_AddRefs(element));
  if (NS_FAILED(rc))
    return rc;
  nsCOMPtr<nsIDOMNode> ignore;
  rc = aDestination->AppendChild(element, getter_AddRefs(ignore));
  if (NS_FAILED(rc))
    return rc;
  if (needType) {
    nsAutoString type;
    rc = nsSOAPUtils::MakeNamespacePrefix(aEncoding, element,
                                            typeNS, type);
    if (NS_FAILED(rc))
      return rc;
    type.Append(gSOAPStrings->kQualifiedSeparator);
    type.Append(typeName);
    rc = aEncoding->GetExternalSchemaURI(gSOAPStrings->kXSIURI, ns);
    if (NS_FAILED(rc))
      return rc;
    rc = (element)->
        SetAttributeNS(ns, gSOAPStrings->kXSITypeAttribute, type);
    if (NS_FAILED(rc))
      return rc;
  }
  if (!aValue.IsEmpty()) {
    nsCOMPtr<nsIDOMText> text;
    rc = document->CreateTextNode(aValue, getter_AddRefs(text));
    if (NS_FAILED(rc))
      return rc;
    rc = (element)->AppendChild(text, getter_AddRefs(ignore));
    if (NS_FAILED(rc))
      return rc;
  }
  *_retval = element;
  NS_IF_ADDREF(*_retval);
  return rc;
}

//  Testing for a simple value
static nsresult HasSimpleValue(nsISchemaType * aSchemaType, PRBool * aResult) {
  PRUint16 typevalue;
  nsresult rc = aSchemaType->GetSchemaType(&typevalue);
  if (NS_FAILED(rc))
    return rc;
  if (typevalue == nsISchemaComplexType::SCHEMA_TYPE_COMPLEX) {
    nsCOMPtr<nsISchemaComplexType> ct = do_QueryInterface(aSchemaType);
    rc = ct->GetContentModel(&typevalue);
    if (NS_FAILED(rc))
      return rc;
    *aResult = typevalue == nsISchemaComplexType::CONTENT_MODEL_SIMPLE;
  } else {
    *aResult = PR_TRUE;
  }
  return NS_OK;
}

//  Default

NS_IMETHODIMP
    nsDefaultEncoder::Encode(nsISOAPEncoding * aEncoding,
                             nsIVariant * aSource,
                             const nsAString & aNamespaceURI,
                             const nsAString & aName,
                             nsISchemaType * aSchemaType,
                             nsISOAPAttachments * aAttachments,
                             nsIDOMElement * aDestination,
                             nsIDOMElement * *aReturnValue)
{
  NS_ENSURE_ARG_POINTER(aEncoding);
  NS_ENSURE_ARG_POINTER(aDestination);
  NS_ENSURE_ARG_POINTER(aReturnValue);
  *aReturnValue = nsnull;
  if (aSource == nsnull) {
    nsAutoString ns;
    nsCOMPtr<nsIDOMElement> cloneable;
    nsresult rc = aEncoding->GetExternalSchemaURI(gSOAPStrings->kXSIURI, ns);
    if (NS_FAILED(rc))
      return rc;
    nsAutoString name;
    if (!aName.IsEmpty())
      name.Assign(gSOAPStrings->kNull);
    rc = EncodeSimpleValue(aEncoding, gSOAPStrings->kEmpty, gSOAPStrings->kEmpty, 
      name, nsnull, aDestination, aReturnValue);
    if (NS_FAILED(rc))
      return rc;
    rc = (*aReturnValue)->SetAttributeNS(ns, gSOAPStrings->kNull, gSOAPStrings->kTrueA);
    if (NS_FAILED(rc))
      return rc;
  }
  nsCOMPtr<nsISOAPEncoder> encoder;
  if (aSchemaType) {
    nsCOMPtr<nsISchemaType> lookupType = aSchemaType;
    do {
      nsAutoString schemaType;
      nsAutoString schemaURI;
      nsAutoString encodingKey;
      nsresult rc = lookupType->GetName(schemaType);
      if (NS_FAILED(rc))
        return rc;
      rc = lookupType->GetTargetNamespace(schemaURI);
      if (NS_FAILED(rc))
        return rc;
      SOAPEncodingKey(schemaURI, schemaType, encodingKey);
      rc = aEncoding->GetEncoder(encodingKey, getter_AddRefs(encoder));
      if (NS_FAILED(rc))
        return rc;
      if (encoder)
        break;
      nsCOMPtr<nsISchemaType> supertype;
      rc = GetSupertype(aEncoding, lookupType, getter_AddRefs(supertype));
      if (NS_FAILED(rc))
        return rc;
      lookupType = supertype;
    }
    while (lookupType);
  }
  if (!encoder) {
    nsAutoString encodingKey;
    SOAPEncodingKey(gSOAPStrings->kXSURI,
                    gSOAPStrings->kAnyTypeSchemaType, encodingKey);
    nsresult rc =
        aEncoding->GetEncoder(encodingKey, getter_AddRefs(encoder));
    if (NS_FAILED(rc))
      return rc;
  }
  if (encoder) {
    return encoder->Encode(aEncoding, aSource, aNamespaceURI, aName,
                           aSchemaType, aAttachments, aDestination,
                           aReturnValue);
  }
  return SOAP_EXCEPTION(NS_ERROR_NOT_IMPLEMENTED,"SOAP_NO_ENCODER_FOR_TYPE","The default encoder finds no encoder for specific type");
}

static void
GetNativeType(PRUint16 aType, nsAString & aSchemaNamespaceURI, nsAString & aSchemaType)
{
  aSchemaNamespaceURI.Assign(gSOAPStrings->kXSURI);
  switch (aType) {
  case nsIDataType::VTYPE_CHAR_STR:
  case nsIDataType::VTYPE_WCHAR_STR:
  case nsIDataType::VTYPE_CHAR:
  case nsIDataType::VTYPE_WCHAR:
  case nsIDataType::VTYPE_STRING_SIZE_IS:
  case nsIDataType::VTYPE_WSTRING_SIZE_IS:
  case nsIDataType::VTYPE_ASTRING:
  case nsIDataType::VTYPE_DOMSTRING:
  case nsIDataType::VTYPE_CSTRING:
  case nsIDataType::VTYPE_UTF8STRING:
    aSchemaType.Assign(gSOAPStrings->kStringSchemaType);
    break;
  case nsIDataType::VTYPE_INT8:
    aSchemaType.Assign(gSOAPStrings->kByteSchemaType);
    break;
  case nsIDataType::VTYPE_INT16:
    aSchemaType.Assign(gSOAPStrings->kShortSchemaType);
    break;
  case nsIDataType::VTYPE_INT32:
    aSchemaType.Assign(gSOAPStrings->kIntSchemaType);
    break;
  case nsIDataType::VTYPE_INT64:
    aSchemaType.Assign(gSOAPStrings->kLongSchemaType);
    break;
  case nsIDataType::VTYPE_UINT8:
    aSchemaType.Assign(gSOAPStrings->kUnsignedByteSchemaType);
    break;
  case nsIDataType::VTYPE_UINT16:
    aSchemaType.Assign(gSOAPStrings->kUnsignedShortSchemaType);
    break;
  case nsIDataType::VTYPE_UINT32:
    aSchemaType.Assign(gSOAPStrings->kUnsignedIntSchemaType);
    break;
  case nsIDataType::VTYPE_UINT64:
    aSchemaType.Assign(gSOAPStrings->kUnsignedLongSchemaType);
    break;
  case nsIDataType::VTYPE_FLOAT:
    aSchemaType.Assign(gSOAPStrings->kFloatSchemaType);
    break;
  case nsIDataType::VTYPE_DOUBLE:
    aSchemaType.Assign(gSOAPStrings->kDoubleSchemaType);
    break;
  case nsIDataType::VTYPE_BOOL:
    aSchemaType.Assign(gSOAPStrings->kBooleanSchemaType);
    break;
  case nsIDataType::VTYPE_ARRAY:
  case nsIDataType::VTYPE_EMPTY_ARRAY:
    aSchemaType.Assign(gSOAPStrings->kArraySOAPType);
    aSchemaNamespaceURI.Assign(gSOAPStrings->kSOAPEncURI);
    break;
//  case nsIDataType::VTYPE_VOID:
//  case nsIDataType::VTYPE_EMPTY:
//  Empty may be either simple or complex.
    break;
  case nsIDataType::VTYPE_INTERFACE_IS:
  case nsIDataType::VTYPE_INTERFACE:
    aSchemaType.Assign(gSOAPStrings->kStructSOAPType);
    aSchemaNamespaceURI.Assign(gSOAPStrings->kSOAPEncURI);
    break;
  default:
    aSchemaType.Assign(gSOAPStrings->kAnySimpleTypeSchemaType);
  }
}

NS_IMETHODIMP
    nsAnyTypeEncoder::Encode(nsISOAPEncoding * aEncoding,
                             nsIVariant * aSource,
                             const nsAString & aNamespaceURI,
                             const nsAString & aName,
                             nsISchemaType * aSchemaType,
                             nsISOAPAttachments * aAttachments,
                             nsIDOMElement * aDestination,
                             nsIDOMElement * *aReturnValue)
{
  NS_ENSURE_ARG_POINTER(aEncoding);
  NS_ENSURE_ARG_POINTER(aDestination);
  NS_ENSURE_ARG_POINTER(aReturnValue);
  *aReturnValue = nsnull;
  nsAutoString nativeSchemaType;
  nsAutoString nativeSchemaURI;
  PRUint16 typevalue;
  nsresult rc = aSource->GetDataType(&typevalue);
  if (NS_FAILED(rc))
    return rc;
  //  If there is a schema type then regular native types will not avail us anything.
  if (aSchemaType) {
    PRBool simple = PR_FALSE;
    rc = HasSimpleValue(aSchemaType, &simple);
    if (NS_FAILED(rc))
      return rc;
    if (simple) {
      switch (typevalue) {
      case nsIDataType::VTYPE_ARRAY:
      case nsIDataType::VTYPE_EMPTY_ARRAY:
      case nsIDataType::VTYPE_INTERFACE_IS:
      case nsIDataType::VTYPE_INTERFACE:
        simple = PR_FALSE;
        break;
      }
    }
    if (simple) {
      nativeSchemaType.Assign(gSOAPStrings->kAnySimpleTypeSchemaType);
      nativeSchemaURI.Assign(gSOAPStrings->kXSURI);
    }
    else {
      nativeSchemaType.Assign(gSOAPStrings->kStructSOAPType);
      nativeSchemaURI.Assign(gSOAPStrings->kSOAPEncURI);
    }
  }
  else {
    GetNativeType(typevalue, nativeSchemaURI, nativeSchemaType);
  }

  nsCOMPtr<nsISOAPEncoder> encoder;
  nsAutoString encodingKey;
  SOAPEncodingKey(nativeSchemaURI, nativeSchemaType, encodingKey);
  rc = aEncoding->GetEncoder(encodingKey, getter_AddRefs(encoder));
  if (NS_FAILED(rc))
    return rc;
  if (encoder) {
    nsCOMPtr<nsISchemaType> type;
    if (aSchemaType) {
      type = aSchemaType;
    }
    else {
      nsCOMPtr<nsISchemaCollection> collection;
      nsresult rc =
          aEncoding->GetSchemaCollection(getter_AddRefs(collection));
      if (NS_FAILED(rc))
        return rc;
      rc = collection->GetType(nativeSchemaType,
                             nativeSchemaURI,
                             getter_AddRefs(type));
//    if (NS_FAILED(rc)) return rc;
    }

    return encoder->Encode(aEncoding, aSource, aNamespaceURI, aName,
                           type, aAttachments, aDestination,
                           aReturnValue);
  }
  return SOAP_EXCEPTION(NS_ERROR_NOT_IMPLEMENTED,"SOAP_NO_ENCODER_FOR_TYPE","The any type encoder finds no encoder for specific data");
}

static nsresult EncodeStructParticle(nsISOAPEncoding* aEncoding, nsIPropertyBag* aPropertyBag, 
                               nsISchemaParticle* aParticle, 
                               nsISOAPAttachments * aAttachments, nsIDOMElement* aDestination)
{
  nsresult rc;
  if (aParticle) {
    PRUint32 minOccurs;
    rc = aParticle->GetMinOccurs(&minOccurs);
    if (NS_FAILED(rc))
      return rc;
    PRUint32 maxOccurs;
    rc = aParticle->GetMaxOccurs(&maxOccurs);
    if (NS_FAILED(rc))
      return rc;
    PRUint16 particleType;
    rc = aParticle->GetParticleType(&particleType);
    if (NS_FAILED(rc))
      return rc;
    switch(particleType) {
      case nsISchemaParticle::PARTICLE_TYPE_ELEMENT: {
        if (maxOccurs > 1) {  //  Todo: Try to make this thing work as an array?
          return NS_ERROR_NOT_AVAILABLE; //  For now, we just try something else if we can (recoverable)
        }
        nsCOMPtr<nsISchemaElement> element = do_QueryInterface(aParticle);
        nsAutoString name;
        rc = element->GetTargetNamespace(name);
        if (NS_FAILED(rc))
          return rc;
        if (!name.IsEmpty()) {
          rc = NS_ERROR_NOT_AVAILABLE; //  No known way to use namespace qualification in struct
        }
        else {
          rc = element->GetName(name);
          if (NS_FAILED(rc))
            return rc;
          rc = element->GetName(name);
          if (NS_FAILED(rc))
            return rc;
          nsCOMPtr<nsISchemaType> type;
          rc = element->GetType(getter_AddRefs(type));
          if (NS_FAILED(rc))
            return rc;
          nsCOMPtr<nsIVariant> value;
          rc = aPropertyBag->GetProperty(name, getter_AddRefs(value));
          if (NS_SUCCEEDED(rc)) {
            nsCOMPtr<nsIDOMElement> dummy;
            rc = aEncoding->Encode(value, gSOAPStrings->kEmpty, name, type, aAttachments, aDestination, getter_AddRefs(dummy));
            if (NS_FAILED(rc))
              return rc;
          }
        }
        if (minOccurs == 0 && rc == NS_ERROR_NOT_AVAILABLE) //  If we succeeded or failed recoverably, but we were permitted to, then return success
          rc = NS_OK;
        return rc;
      }
      case nsISchemaParticle::PARTICLE_TYPE_MODEL_GROUP: 
      {
        if (maxOccurs > 1) {  //  Todo: Try to make this thing work as an array?
          return NS_ERROR_NOT_AVAILABLE; //  For now, we just try something else if we can (recoverable)
        }
        nsCOMPtr<nsISchemaModelGroup> modelGroup = do_QueryInterface(aParticle);
        PRUint16 compositor;
        rc = modelGroup->GetCompositor(&compositor);
        if (NS_FAILED(rc))
          return rc;
        PRUint32 particleCount;
        rc = modelGroup->GetParticleCount(&particleCount);
        if (NS_FAILED(rc))
          return rc;
        PRUint32 i;
        for (i = 0; i < particleCount; i++) {
          nsCOMPtr<nsISchemaParticle> child;
          rc = modelGroup->GetParticle(i, getter_AddRefs(child));
          if (NS_FAILED(rc))
            return rc;
          rc = EncodeStructParticle(aEncoding, aPropertyBag, child, aAttachments, aDestination);
          if (compositor == nsISchemaModelGroup::COMPOSITOR_CHOICE) {
            if (NS_SUCCEEDED(rc)) {
              return NS_OK;
            }
            if (rc == NS_ERROR_NOT_AVAILABLE) {  //  In a choice, recoverable model failures are OK.
              rc = NS_OK;
            }
          }
          else if (i > 0 && rc == NS_ERROR_NOT_AVAILABLE) {  //  This detects ambiguous model (non-deterministic choice which fails after succeeding on first)
            return SOAP_EXCEPTION(NS_ERROR_ILLEGAL_VALUE,"SOAP_AMBIGUOUS_ENCODING","Cannot proceed due to ambiguity or error in content model");
          }
          if (NS_FAILED(rc))
            break;
        }
        if (compositor == nsISchemaModelGroup::COMPOSITOR_CHOICE)  //  If choice selected nothing, this is recoverable failure
          rc = NS_ERROR_NOT_AVAILABLE;
        if (minOccurs == 0 && rc == NS_ERROR_NOT_AVAILABLE)  //  If we succeeded or failed recoverably, but we were permitted to, then return success
          rc = NS_OK;
        return rc;                    //  Return status
      }
      case nsISchemaParticle::PARTICLE_TYPE_ANY:
//  No model available here (we may wish to handle strict versus lazy, but what does that mean with only local accessor names)
      default:
        break;
    }
  }

  nsCOMPtr<nsISimpleEnumerator> e;
  rc = aPropertyBag->GetEnumerator(getter_AddRefs(e));
  if (NS_FAILED(rc))
    return rc;
  PRBool more;
  rc = e->HasMoreElements(&more);
  if (NS_FAILED(rc))
    return rc;
  while (more) {
    nsCOMPtr<nsIProperty> p;
    rc = e->GetNext(getter_AddRefs(p));
    if (NS_FAILED(rc))
      return rc;
    nsAutoString name;
    rc = p->GetName(name);
    if (NS_FAILED(rc))
      return rc;
    nsCOMPtr<nsIVariant>value;
    rc = p->GetValue(getter_AddRefs(value));
    if (NS_FAILED(rc))
      return rc;
    nsCOMPtr<nsIDOMElement>result;
    rc = aEncoding->Encode(value,gSOAPStrings->kEmpty,name,nsnull,aAttachments,aDestination,getter_AddRefs(result));
    if (NS_FAILED(rc))
      return rc;
    rc = e->HasMoreElements(&more);
    if (NS_FAILED(rc))
      return rc;
  }
  return NS_OK;
}

NS_IMETHODIMP
    nsStructEncoder::Encode(nsISOAPEncoding * aEncoding,
                             nsIVariant * aSource,
                             const nsAString & aNamespaceURI,
                             const nsAString & aName,
                             nsISchemaType * aSchemaType,
                             nsISOAPAttachments * aAttachments,
                             nsIDOMElement * aDestination,
                             nsIDOMElement * *aReturnValue)
{
  NS_ENSURE_ARG_POINTER(aEncoding);
  NS_ENSURE_ARG_POINTER(aDestination);
  NS_ENSURE_ARG_POINTER(aReturnValue);
  *aReturnValue = nsnull;
  nsIID* iid;
  nsCOMPtr<nsISupports> ptr;
  nsresult rc = aSource->GetAsInterface(&iid, getter_AddRefs(ptr));
  if (NS_FAILED(rc))
    return rc;
  nsCOMPtr<nsIPropertyBag>pbptr = do_QueryInterface(ptr);
  if (pbptr) {  //  Do any object that can QI to a property bag.
    nsCOMPtr<nsISchemaModelGroup>modelGroup;
    if (aSchemaType) {
      nsCOMPtr<nsISchemaComplexType>ctype = do_QueryInterface(aSchemaType);
      if (ctype) {
        rc = ctype->GetModelGroup(getter_AddRefs(modelGroup));
        if (NS_FAILED(rc))
          return rc;
      }
    }
//  We still have to fake this one, because there is no struct type in schema.
    if (aName.IsEmpty() && !aSchemaType) {
      rc = EncodeSimpleValue(aEncoding, gSOAPStrings->kEmpty,
                             gSOAPStrings->kSOAPEncURI,
                             gSOAPStrings->kStructSOAPType,
                             aSchemaType, aDestination,
                             aReturnValue);
    }
    else {
      rc = EncodeSimpleValue(aEncoding, gSOAPStrings->kEmpty,
                             aNamespaceURI, aName, aSchemaType, aDestination,
                             aReturnValue);
    }
    if (NS_FAILED(rc))
      return rc;
    return EncodeStructParticle(aEncoding, pbptr, modelGroup, aAttachments, *aReturnValue);
  }
  return SOAP_EXCEPTION(NS_ERROR_ILLEGAL_VALUE,"SOAP_PROPERTYBAG_REQUIRED","When encoding as a struct, an object with properties is required");
}

//  AnySimpleType

NS_IMETHODIMP
    nsAnySimpleTypeEncoder::Encode(nsISOAPEncoding * aEncoding,
                                   nsIVariant * aSource,
                                   const nsAString & aNamespaceURI,
                                   const nsAString & aName,
                                   nsISchemaType * aSchemaType,
                                   nsISOAPAttachments * aAttachments,
                                   nsIDOMElement * aDestination,
                                   nsIDOMElement * *aReturnValue)
{
  NS_ENSURE_ARG_POINTER(aEncoding);
  NS_ENSURE_ARG_POINTER(aDestination);
  NS_ENSURE_ARG_POINTER(aReturnValue);
  *aReturnValue = nsnull;
  nsresult rc;
  nsAutoString value;
  rc = aSource->GetAsAString(value);
  if (NS_FAILED(rc))
    return rc;
//  We still have to fake this one, because there is no any simple type in schema.
  if (aName.IsEmpty() && !aSchemaType) {
    return EncodeSimpleValue(aEncoding, 
                             value,
                             gSOAPStrings->kSOAPEncURI,
                             gSOAPStrings->kAnySimpleTypeSchemaType,
                             aSchemaType,
                             aDestination,
                             aReturnValue);
  }
  return EncodeSimpleValue(aEncoding,
                           value,
                           aNamespaceURI,
                           aName,
                           aSchemaType,
                           aDestination,
                           aReturnValue);
}

/**
 * Recursive method used by array encoding which counts the sizes of the specified dimensions
 * and does a very primitive determination whether all the members of the array are of a single
 * homogenious type.  This intelligently skips nulls wherever they occur.
 */
static nsresult GetArrayType(nsIVariant* aSource, PRUint32 aDimensionCount, PRUint32 * aDimensionSizes, PRUint16 * aType)
{
  if (!aSource) {
    *aType = nsIDataType::VTYPE_EMPTY;
    return NS_OK;
  }
  PRUint16 type;
  nsIID iid;
  PRUint32 count;
  void* array;
  nsresult rc;
  PRUint32 i;
  rc = aSource->GetDataType(&type);
  if (NS_FAILED(rc))
    return rc;
  if (type == nsIDataType::VTYPE_EMPTY
      || type == nsIDataType::VTYPE_VOID
      || type == nsIDataType::VTYPE_EMPTY_ARRAY) {
    rc = NS_OK;
    count = 0;
    type = nsIDataType::VTYPE_EMPTY;
    array = nsnull;
  }
  else {
    rc = aSource->GetAsArray(&type, &iid, &count, &array);        // First, get the array, if any.
    if (NS_FAILED(rc))
      return rc;
  }
  if (count > aDimensionSizes[0]) {
    aDimensionSizes[0] = count;
  }
  if (aDimensionCount > 1) {
    if (type != nsIDataType::VTYPE_INTERFACE_IS
      || !iid.Equals(NS_GET_IID(nsIVariant))) {
      rc = SOAP_EXCEPTION(NS_ERROR_ILLEGAL_VALUE,"SOAP_ARRAY_OBJECTS","When encoding as an array, an array of array objects is required");
                                  //  All nested arrays (which is what multi-dimensional arrays are) are variants.
    }
    else {
      nsIVariant** a = NS_STATIC_CAST(nsIVariant**,array);
      PRUint16 rtype = nsIDataType::VTYPE_EMPTY;
      for (i = 0; i < count; i++) {
        PRUint16 nexttype;
        rc = GetArrayType(a[i], aDimensionCount - 1, aDimensionSizes + 1, &nexttype);
        if (NS_FAILED(rc))
          break;
        if (rtype == nsIDataType::VTYPE_EMPTY)
          rtype = nexttype;
        else if (nexttype != nsIDataType::VTYPE_EMPTY
                 && nexttype != rtype)
          rtype = nsIDataType::VTYPE_INTERFACE_IS;
      }
      *aType = rtype;
    }
  }
  else {
    *aType = type;
  }
  //  The memory model for variant arrays' GetAsArray is difficult to manage
  switch (type) {
    case nsIDataType::VTYPE_INTERFACE_IS:
      {
        nsISupports** values = NS_STATIC_CAST(nsISupports**,array);
        for (i = 0; i < count; i++)
          NS_RELEASE(values[i]);
      }
      break;
    case nsIDataType::VTYPE_WCHAR_STR:
    case nsIDataType::VTYPE_CHAR_STR:
      {
        void** ptrs = NS_STATIC_CAST(void**,array);
        for (i = 0; i < count; i++) {
          nsMemory::Free(ptrs[i]);
        }
      }
      break;
  }
  nsMemory::Free(array);
  {  //  Individual lengths guaranteed to fit because variant array length is 32-bit.
    PRUint64 tot = 1;  //  Collect in 64 bits, just to make sure combo fits
    for (i = 0; i < aDimensionCount; i++) {
      tot = tot * aDimensionSizes[i];
      if (tot > 0xffffffffU) {
        return SOAP_EXCEPTION(NS_ERROR_ILLEGAL_VALUE,"SOAP_ARRAY_TOO_BIG","When encoding an object as an array, the total count of items exceeded maximum.");
      }
    }
  }
  return rc;
}
/**
 * Recursive method used by array encoding to encode the next level of the array into the
 * established array element.  If dimension count is > 1, then it recursively doles out
 * the work.  This intelligently skips nulls wherever they occur.
 */
static nsresult EncodeArray(nsISOAPEncoding* aEncoding, nsIVariant* aSource, nsISchemaType* aSchemaType,
  nsISOAPAttachments* aAttachments, nsIDOMElement* aArray, PRUint32 aDimensionCount, PRUint32* aDimensionSizes)
{
  nsresult rc;
  PRUint16 type;
  nsIID iid;
  PRUint32 count;
  void *array;
  if (aSource != nsnull) {
    nsresult rc = aSource->GetDataType(&type);
    if (NS_FAILED(rc))
      return rc;
    if (type == nsIDataType::VTYPE_EMPTY
        || type == nsIDataType::VTYPE_VOID
        || type == nsIDataType::VTYPE_EMPTY_ARRAY) {
      rc = NS_OK;
      count = 0;
      type = nsIDataType::VTYPE_EMPTY;
      array = nsnull;
    }
    else {
      rc = aSource->GetAsArray(&type, &iid, &count, &array);        // First, get the array, if any.
      if (NS_FAILED(rc))
        return rc;
    }
  }
  else {  //  If the source is null, then just add a bunch of nulls to the array.
    count = (PRUint32)aDimensionSizes[--aDimensionCount];
    while (aDimensionCount)
      count *= (PRUint32)aDimensionSizes[--aDimensionCount];
    if (count) {
      nsAutoString ns;
      nsCOMPtr<nsIDOMElement> cloneable;
      rc = aEncoding->GetExternalSchemaURI(gSOAPStrings->kXSIURI, ns);
      if (NS_FAILED(rc))
        return rc;
      rc = EncodeSimpleValue(aEncoding, gSOAPStrings->kEmpty, gSOAPStrings->kEmpty, 
        gSOAPStrings->kNull, nsnull, aArray, getter_AddRefs(cloneable));
      if (NS_FAILED(rc))
        return rc;
      rc = cloneable->SetAttributeNS(ns, gSOAPStrings->kNull, gSOAPStrings->kTrueA);
      if (NS_FAILED(rc))
        return rc;
      nsCOMPtr<nsIDOMNode> clone;
      nsCOMPtr<nsIDOMNode> dummy;
      for (;--count;) {
        rc = cloneable->CloneNode(PR_TRUE, getter_AddRefs(clone));// No children so deep == shallow
        if (NS_FAILED(rc))
          return rc;
        rc = aArray->AppendChild(clone, getter_AddRefs(dummy));
        if (NS_FAILED(rc))
          return rc;
      }
    }
  }
  nsCOMPtr<nsIDOMElement> dummy;
  PRBool freeptrs = PR_FALSE;
  PRUint32 i;

//  The more-robust way of encoding is to construct variants and call the encoder directly,
//  but for now, we short-circuit it for simple types.

#define ENCODE_SIMPLE_ARRAY(XPType, VType, Source) \
      {\
        XPType* values = NS_STATIC_CAST(XPType*, array);\
        nsCOMPtr<nsIWritableVariant> p =\
           do_CreateInstance(NS_VARIANT_CONTRACTID, &rc);\
        if (NS_FAILED(rc)) break;\
        for (i = 0; i < count; i++) {\
          if (NS_FAILED(rc))\
            break;\
          rc = p->SetAs##VType(Source);\
          if (NS_FAILED(rc))\
            break;\
          rc = aEncoding->Encode(p,\
                       gSOAPStrings->kEmpty,\
                       gSOAPStrings->kEmpty,\
                       aSchemaType,\
                       aAttachments,\
                       aArray,\
                       getter_AddRefs(dummy));\
          if (NS_FAILED(rc)) break;\
        }\
        break;\
      }

  if (aDimensionCount > 1) {
    switch (type) {
      case nsIDataType::VTYPE_INTERFACE_IS:
        {
          nsIVariant** values = NS_STATIC_CAST(nsIVariant**, array);//  If not truly a variant, we only release.
          if (iid.Equals(NS_GET_IID(nsIVariant))) {  //  Only do variants for now.
            for (i = 0; i < count; i++) {
              rc = EncodeArray(aEncoding, values[i],
                             aSchemaType,
                             aAttachments,
                             aArray,
                             aDimensionCount - 1,
                             aDimensionSizes + 1);
              if (NS_FAILED(rc)) break;
            }
          }
          for (i = 0; i < count; i++)
            NS_RELEASE(values[i]);
          break;
        }
      case nsIDataType::VTYPE_WCHAR_STR:
      case nsIDataType::VTYPE_CHAR_STR:
        freeptrs = PR_TRUE;
      default:
        rc = SOAP_EXCEPTION(NS_ERROR_ILLEGAL_VALUE,"SOAP_ARRAY_OBJECTS","When encoding as an array, an array of array objects is required");
    }
  } else switch (type) {
    case nsIDataType::VTYPE_INT8:
      ENCODE_SIMPLE_ARRAY(PRUint8, Int8,
                      (signed char) values[i]);
    case nsIDataType::VTYPE_INT16:
      ENCODE_SIMPLE_ARRAY(PRInt16, Int16, values[i]);
    case nsIDataType::VTYPE_INT32:
      ENCODE_SIMPLE_ARRAY(PRInt32, Int32, values[i]);
    case nsIDataType::VTYPE_INT64:
      ENCODE_SIMPLE_ARRAY(PRInt64, Int64, values[i]);
    case nsIDataType::VTYPE_UINT8:
      ENCODE_SIMPLE_ARRAY(PRUint8, Uint8, values[i]);
    case nsIDataType::VTYPE_UINT16:
      ENCODE_SIMPLE_ARRAY(PRUint16, Uint16, values[i]);
    case nsIDataType::VTYPE_UINT32:
      ENCODE_SIMPLE_ARRAY(PRUint32, Uint32, values[i]);
    case nsIDataType::VTYPE_UINT64:
      ENCODE_SIMPLE_ARRAY(PRUint64, Uint64, values[i]);
    case nsIDataType::VTYPE_FLOAT:
      ENCODE_SIMPLE_ARRAY(float, Float, values[i]);
    case nsIDataType::VTYPE_DOUBLE:
      ENCODE_SIMPLE_ARRAY(double, Double, values[i]);
    case nsIDataType::VTYPE_BOOL:
      ENCODE_SIMPLE_ARRAY(PRBool, Bool, (PRUint16) values[i]);
    case nsIDataType::VTYPE_ID:
    case nsIDataType::VTYPE_CHAR_STR:
      freeptrs = PR_TRUE;
      ENCODE_SIMPLE_ARRAY(char *, String, values[i]);
    case nsIDataType::VTYPE_WCHAR_STR:
      freeptrs = PR_TRUE;
      ENCODE_SIMPLE_ARRAY(PRUnichar *, WString, values[i]);
    case nsIDataType::VTYPE_CHAR:
      ENCODE_SIMPLE_ARRAY(char, Char, values[i]);
    case nsIDataType::VTYPE_WCHAR:
      ENCODE_SIMPLE_ARRAY(PRUnichar, WChar, values[i]);
    case nsIDataType::VTYPE_INTERFACE_IS:
      {
        nsIVariant** values = NS_STATIC_CAST(nsIVariant**, array);//  If not truly a variant, we only use as nsISupports
        if (iid.Equals(NS_GET_IID(nsIVariant))) {  //  Only do variants for now.
          for (i = 0; i < count; i++) {
            rc = aEncoding->Encode(values[i],
                           gSOAPStrings->kEmpty,
                           gSOAPStrings->kEmpty,
                           aSchemaType,
                           aAttachments,
                           aArray,
                           getter_AddRefs(dummy));
            if (NS_FAILED(rc)) break;
          }
        }
        else {
          nsCOMPtr<nsIWritableVariant> p =
            do_CreateInstance(NS_VARIANT_CONTRACTID, &rc);
          if (NS_FAILED(rc)) break;
          for (i = 0; i < count; i++) {
            if (NS_FAILED(rc))
              break;
            rc = p->SetAsInterface(iid, values[i]);
            if (NS_FAILED(rc))
              break;
            rc = aEncoding->Encode(p,
                           gSOAPStrings->kEmpty,
                           gSOAPStrings->kEmpty,
                           aSchemaType,
                           aAttachments,
                           aArray,
                           getter_AddRefs(dummy));
            if (NS_FAILED(rc)) break;
          }
        }
        for (i = 0; i < count; i++)
          NS_RELEASE(values[i]);
        break;
      }
  
    case nsIDataType::VTYPE_EMPTY_ARRAY:
    case nsIDataType::VTYPE_EMPTY:
      break;  //  I think an empty array needs no elements?
  //  Don't support these array types, as they seem meaningless.
    case nsIDataType::VTYPE_ASTRING:
    case nsIDataType::VTYPE_VOID:
    case nsIDataType::VTYPE_INTERFACE:
    case nsIDataType::VTYPE_ARRAY:
      rc = SOAP_EXCEPTION(NS_ERROR_ILLEGAL_VALUE,"SOAP_ARRAY_TYPES","When encoding an array, unable to handle array elements");
  }
  if (freeptrs) {
    void** ptrs = NS_STATIC_CAST(void**,array);
    for (i = 0; i < count; i++) {
      nsMemory::Free(ptrs[i]);
    }
    nsMemory::Free(array);
  }
//  We know that count does not exceed size of dimension, but it may be less
  return rc;
}

NS_IMETHODIMP
    nsArrayEncoder::Encode(nsISOAPEncoding * aEncoding,
                           nsIVariant * aSource,
                           const nsAString & aNamespaceURI,
                           const nsAString & aName,
                           nsISchemaType * aSchemaType,
                           nsISOAPAttachments * aAttachments,
                           nsIDOMElement * aDestination,
                           nsIDOMElement * *aReturnValue)
{
  NS_ENSURE_ARG_POINTER(aEncoding);
  NS_ENSURE_ARG_POINTER(aDestination);
  NS_ENSURE_ARG_POINTER(aReturnValue);
  *aReturnValue = nsnull;
  PRUint16 arrayNativeType;
  PRUint32 dimensionSizes[MAX_ARRAY_DIMENSIONS];
  PRUint32 i;
  PRUint32 dimensionCount = 1;
  nsCOMPtr<nsISchemaType> schemaArrayType;
  if (aSchemaType) {
    PRUint16 type;
    nsresult rc = aSchemaType->GetSchemaType(&type);
    if (NS_FAILED(rc))
      return rc;
    if (type == nsISchemaType::SCHEMA_TYPE_COMPLEX) {
      nsCOMPtr<nsISchemaComplexType> ct = do_QueryInterface(aSchemaType);
      nsresult rc = ct->GetArrayDimension(&dimensionCount);
      if (NS_FAILED(rc))
        return rc;
      if (dimensionCount == 0) {
        dimensionCount = 1;
      }
      else {
//  Arrays with no defaults are supposed to return 0, but apparently do not
        rc = ct->GetArrayType(getter_AddRefs(schemaArrayType));
        if (NS_FAILED(rc))
          return rc;
      }
    }
  }
  for (i = 0; i < dimensionCount; i++)
    dimensionSizes[i] = 0;
  //  Look over the array and find its dimensions and common type.
  nsresult rc = GetArrayType(aSource, dimensionCount, dimensionSizes, &arrayNativeType);
  if (NS_FAILED(rc))
    return rc;
  nsAutoString arrayTypeSchemaURI;
  nsAutoString arrayTypeSchemaName;
  if (!schemaArrayType) {
    switch (arrayNativeType) {
      case nsIDataType::VTYPE_INTERFACE:  //  In a variant, an interface is a struct, but here it is any
      case nsIDataType::VTYPE_INTERFACE_IS:
        arrayTypeSchemaName = gSOAPStrings->kAnyTypeSchemaType;
        arrayTypeSchemaURI = gSOAPStrings->kXSURI;
        break;
      default:  //  Everything else can be interpreted correctly
        GetNativeType(arrayNativeType, arrayTypeSchemaURI, arrayTypeSchemaName);
    }
    nsCOMPtr<nsISchemaCollection> collection;
    nsresult rc =
        aEncoding->GetSchemaCollection(getter_AddRefs(collection));
    if (NS_FAILED(rc))
      return rc;
    rc = collection->GetType(arrayTypeSchemaName,
                             arrayTypeSchemaName,
                             getter_AddRefs(schemaArrayType));
//    if (NS_FAILED(rc)) return rc;
  }
  else {
    rc = schemaArrayType->GetTargetNamespace(arrayTypeSchemaURI);
    if (NS_FAILED(rc))
      return rc;
    rc = schemaArrayType->GetName(arrayTypeSchemaName);
    if (NS_FAILED(rc))
      return rc;
  }
  rc = EncodeSimpleValue(aEncoding, gSOAPStrings->kEmpty,
                         aNamespaceURI,
                         aName, aSchemaType, aDestination, aReturnValue);
  if (NS_FAILED(rc))
    return rc;

  //  This needs a real live interpretation of the type.

  {
    nsAutoString value;
    nsSOAPUtils::MakeNamespacePrefix(aEncoding,*aReturnValue,arrayTypeSchemaURI,value);
    value.Append(gSOAPStrings->kQualifiedSeparator);
    value.Append(arrayTypeSchemaName);
    value.Append(PRUnichar('['));
    for (i = 0; i < dimensionCount; i++) {
      if (i > 0)
        value.Append(PRUnichar(','));
      char* ptr = PR_smprintf("%d", dimensionSizes[i]);
      AppendUTF8toUTF16(ptr, value);
      PR_smprintf_free(ptr);
    }
    value.Append(PRUnichar(']'));
    nsAutoString encURI;
    rc = aEncoding->GetExternalSchemaURI(gSOAPStrings->kSOAPEncURI,encURI);
    if (NS_FAILED(rc))
      return rc;

    rc = (*aReturnValue)->SetAttributeNS(encURI, gSOAPStrings->kSOAPArrayTypeAttribute, value);
    if (NS_FAILED(rc))
      return rc;
  }

//  For efficiency, we should perform encoder lookup once here.

  return EncodeArray(aEncoding, aSource, schemaArrayType, aAttachments, *aReturnValue, dimensionCount, dimensionSizes);
}

//  String

NS_IMETHODIMP
    nsStringEncoder::Encode(nsISOAPEncoding * aEncoding,
                            nsIVariant * aSource,
                            const nsAString & aNamespaceURI,
                            const nsAString & aName,
                            nsISchemaType * aSchemaType,
                            nsISOAPAttachments * aAttachments,
                            nsIDOMElement * aDestination,
                            nsIDOMElement * *aReturnValue)
{
  NS_ENSURE_ARG_POINTER(aEncoding);
  NS_ENSURE_ARG_POINTER(aDestination);
  NS_ENSURE_ARG_POINTER(aReturnValue);
  *aReturnValue = nsnull;
  nsresult rc;
  nsAutoString value;
  rc = aSource->GetAsAString(value);
  if (NS_FAILED(rc))
    return rc;
  return EncodeSimpleValue(aEncoding, value,
                           aNamespaceURI, aName, aSchemaType, aDestination,
                           aReturnValue);
}

//  PRBool

NS_IMETHODIMP
    nsBooleanEncoder::Encode(nsISOAPEncoding * aEncoding,
                             nsIVariant * aSource,
                             const nsAString & aNamespaceURI,
                             const nsAString & aName,
                             nsISchemaType * aSchemaType,
                             nsISOAPAttachments * aAttachments,
                             nsIDOMElement * aDestination,
                             nsIDOMElement * *aReturnValue)
{
  NS_ENSURE_ARG_POINTER(aEncoding);
  NS_ENSURE_ARG_POINTER(aDestination);
  NS_ENSURE_ARG_POINTER(aReturnValue);
  *aReturnValue = nsnull;
  nsresult rc;
  PRBool b;
  rc = aSource->GetAsBool(&b);
  if (NS_FAILED(rc))
    return rc;
  return EncodeSimpleValue(aEncoding, b ? gSOAPStrings->kTrueA : gSOAPStrings->kFalseA,
                           aNamespaceURI, aName, aSchemaType, aDestination,
                           aReturnValue);
}

//  Double

NS_IMETHODIMP
    nsDoubleEncoder::Encode(nsISOAPEncoding * aEncoding,
                            nsIVariant * aSource,
                            const nsAString & aNamespaceURI,
                            const nsAString & aName,
                            nsISchemaType * aSchemaType,
                            nsISOAPAttachments * aAttachments,
                            nsIDOMElement * aDestination,
                            nsIDOMElement * *aReturnValue)
{
  NS_ENSURE_ARG_POINTER(aEncoding);
  NS_ENSURE_ARG_POINTER(aDestination);
  NS_ENSURE_ARG_POINTER(aReturnValue);
  *aReturnValue = nsnull;
  nsresult rc;
  double f;
  rc = aSource->GetAsDouble(&f);        //  Check that double works.
  if (NS_FAILED(rc))
    return rc;
  char *ptr = PR_smprintf("%lf", f);
  if (!ptr)
    return NS_ERROR_OUT_OF_MEMORY;
  nsAutoString value;
  CopyASCIItoUCS2(nsDependentCString(ptr), value);
  PR_smprintf_free(ptr);
  return EncodeSimpleValue(aEncoding, value,
                           aNamespaceURI, aName, aSchemaType, aDestination,
                           aReturnValue);
}

//  Float

NS_IMETHODIMP
    nsFloatEncoder::Encode(nsISOAPEncoding * aEncoding,
                           nsIVariant * aSource,
                           const nsAString & aNamespaceURI,
                           const nsAString & aName,
                           nsISchemaType * aSchemaType,
                           nsISOAPAttachments * aAttachments,
                           nsIDOMElement * aDestination,
                           nsIDOMElement * *aReturnValue)
{
  NS_ENSURE_ARG_POINTER(aEncoding);
  NS_ENSURE_ARG_POINTER(aDestination);
  NS_ENSURE_ARG_POINTER(aReturnValue);
  *aReturnValue = nsnull;
  nsresult rc;
  float f;
  rc = aSource->GetAsFloat(&f);        //  Check that float works.
  if (NS_FAILED(rc))
    return rc;
  char *ptr = PR_smprintf("%f", f);
  if (!ptr)
    return NS_ERROR_OUT_OF_MEMORY;
  nsAutoString value;
  CopyASCIItoUCS2(nsDependentCString(ptr), value);
  PR_smprintf_free(ptr);
  return EncodeSimpleValue(aEncoding, value,
                           aNamespaceURI, aName, aSchemaType, aDestination,
                           aReturnValue);
}

//  PRInt64

NS_IMETHODIMP
    nsLongEncoder::Encode(nsISOAPEncoding * aEncoding,
                          nsIVariant * aSource,
                          const nsAString & aNamespaceURI,
                          const nsAString & aName,
                          nsISchemaType * aSchemaType,
                          nsISOAPAttachments * aAttachments,
                          nsIDOMElement * aDestination,
                          nsIDOMElement * *aReturnValue)
{
  NS_ENSURE_ARG_POINTER(aEncoding);
  NS_ENSURE_ARG_POINTER(aDestination);
  NS_ENSURE_ARG_POINTER(aReturnValue);
  *aReturnValue = nsnull;
  nsresult rc;
  PRInt64 f;
  rc = aSource->GetAsInt64(&f);        //  Get as a long number.
  if (NS_FAILED(rc))
    return rc;
  char *ptr = PR_smprintf("%lld", f);
  if (!ptr)
    return NS_ERROR_OUT_OF_MEMORY;
  nsAutoString value;
  CopyASCIItoUCS2(nsDependentCString(ptr), value);
  PR_smprintf_free(ptr);
  return EncodeSimpleValue(aEncoding, value,
                           aNamespaceURI, aName, aSchemaType, aDestination,
                           aReturnValue);
}

//  PRInt32

NS_IMETHODIMP
    nsIntEncoder::Encode(nsISOAPEncoding * aEncoding,
                         nsIVariant * aSource,
                         const nsAString & aNamespaceURI,
                         const nsAString & aName,
                         nsISchemaType * aSchemaType,
                         nsISOAPAttachments * aAttachments,
                         nsIDOMElement * aDestination,
                         nsIDOMElement * *aReturnValue)
{
  NS_ENSURE_ARG_POINTER(aEncoding);
  NS_ENSURE_ARG_POINTER(aDestination);
  NS_ENSURE_ARG_POINTER(aReturnValue);
  *aReturnValue = nsnull;
  nsresult rc;
  PRInt32 f;
  rc = aSource->GetAsInt32(&f);        //  Get as a long number.
  if (NS_FAILED(rc))
    return rc;
  char *ptr = PR_smprintf("%d", f);
  if (!ptr)
    return NS_ERROR_OUT_OF_MEMORY;
  nsAutoString value;
  CopyASCIItoUCS2(nsDependentCString(ptr), value);
  PR_smprintf_free(ptr);
  return EncodeSimpleValue(aEncoding, value,
                           aNamespaceURI, aName, aSchemaType, aDestination,
                           aReturnValue);
}

//  PRInt16

NS_IMETHODIMP
    nsShortEncoder::Encode(nsISOAPEncoding * aEncoding,
                           nsIVariant * aSource,
                           const nsAString & aNamespaceURI,
                           const nsAString & aName,
                           nsISchemaType * aSchemaType,
                           nsISOAPAttachments * aAttachments,
                           nsIDOMElement * aDestination,
                           nsIDOMElement * *aReturnValue)
{
  NS_ENSURE_ARG_POINTER(aEncoding);
  NS_ENSURE_ARG_POINTER(aDestination);
  NS_ENSURE_ARG_POINTER(aReturnValue);
  *aReturnValue = nsnull;
  nsresult rc;
  PRInt16 f;
  rc = aSource->GetAsInt16(&f);        //  Get as a long number.
  if (NS_FAILED(rc))
    return rc;
  char *ptr = PR_smprintf("%d", (PRInt32) f);
  if (!ptr)
    return NS_ERROR_OUT_OF_MEMORY;
  nsAutoString value;
  CopyASCIItoUCS2(nsDependentCString(ptr), value);
  PR_smprintf_free(ptr);
  return EncodeSimpleValue(aEncoding, value,
                           aNamespaceURI, aName, aSchemaType, aDestination,
                           aReturnValue);
}

//  Byte

NS_IMETHODIMP
    nsByteEncoder::Encode(nsISOAPEncoding * aEncoding,
                          nsIVariant * aSource,
                          const nsAString & aNamespaceURI,
                          const nsAString & aName,
                          nsISchemaType * aSchemaType,
                          nsISOAPAttachments * aAttachments,
                          nsIDOMElement * aDestination,
                          nsIDOMElement * *aReturnValue)
{
  NS_ENSURE_ARG_POINTER(aEncoding);
  NS_ENSURE_ARG_POINTER(aDestination);
  NS_ENSURE_ARG_POINTER(aReturnValue);
  *aReturnValue = nsnull;
  nsresult rc;
  PRUint8 f;
  rc = aSource->GetAsInt8(&f);        //  Get as a long number.
  if (NS_FAILED(rc))
    return rc;
  char *ptr = PR_smprintf("%d", (PRInt32) (signed char) f);
  if (!ptr)
    return NS_ERROR_OUT_OF_MEMORY;
  nsAutoString value;
  CopyASCIItoUCS2(nsDependentCString(ptr), value);
  PR_smprintf_free(ptr);
  return EncodeSimpleValue(aEncoding, value,
                           aNamespaceURI, aName, aSchemaType, aDestination,
                           aReturnValue);
}

//  PRUint64

NS_IMETHODIMP
    nsUnsignedLongEncoder::Encode(nsISOAPEncoding * aEncoding,
                                  nsIVariant * aSource,
                                  const nsAString & aNamespaceURI,
                                  const nsAString & aName,
                                  nsISchemaType * aSchemaType,
                                  nsISOAPAttachments * aAttachments,
                                  nsIDOMElement * aDestination,
                                  nsIDOMElement * *aReturnValue)
{
  NS_ENSURE_ARG_POINTER(aEncoding);
  NS_ENSURE_ARG_POINTER(aDestination);
  NS_ENSURE_ARG_POINTER(aReturnValue);
  *aReturnValue = nsnull;
  nsresult rc;
  PRUint64 f;
  rc = aSource->GetAsUint64(&f);        //  Get as a long number.
  if (NS_FAILED(rc))
    return rc;
  char *ptr = PR_smprintf("%llu", f);
  if (!ptr)
    return NS_ERROR_OUT_OF_MEMORY;
  nsAutoString value;
  CopyASCIItoUCS2(nsDependentCString(ptr), value);
  PR_smprintf_free(ptr);
  return EncodeSimpleValue(aEncoding, value,
                           aNamespaceURI, aName, aSchemaType, aDestination,
                           aReturnValue);
}

//  PRUint32

NS_IMETHODIMP
    nsUnsignedIntEncoder::Encode(nsISOAPEncoding * aEncoding,
                                 nsIVariant * aSource,
                                 const nsAString & aNamespaceURI,
                                 const nsAString & aName,
                                 nsISchemaType * aSchemaType,
                                 nsISOAPAttachments * aAttachments,
                                 nsIDOMElement * aDestination,
                                 nsIDOMElement * *aReturnValue)
{
  NS_ENSURE_ARG_POINTER(aEncoding);
  NS_ENSURE_ARG_POINTER(aDestination);
  NS_ENSURE_ARG_POINTER(aReturnValue);
  *aReturnValue = nsnull;
  nsresult rc;
  PRUint32 f;
  rc = aSource->GetAsUint32(&f);        //  Get as a long number.
  if (NS_FAILED(rc))
    return rc;
  char *ptr = PR_smprintf("%u", f);
  if (!ptr)
    return NS_ERROR_OUT_OF_MEMORY;
  nsAutoString value;
  CopyASCIItoUCS2(nsDependentCString(ptr), value);
  PR_smprintf_free(ptr);
  return EncodeSimpleValue(aEncoding, value,
                           aNamespaceURI, aName, aSchemaType, aDestination,
                           aReturnValue);
}

NS_IMETHODIMP
nsBase64BinaryEncoder::Encode(nsISOAPEncoding * aEncoding,
                              nsIVariant * aSource,
                              const nsAString & aNamespaceURI,
                              const nsAString & aName,
                              nsISchemaType * aSchemaType,
                              nsISOAPAttachments * aAttachments,
                              nsIDOMElement * aDestination,
                              nsIDOMElement * *aReturnValue)
{
  NS_ENSURE_ARG_POINTER(aSource);
  NS_ENSURE_ARG_POINTER(aEncoding);
  NS_ENSURE_ARG_POINTER(aDestination);
  NS_ENSURE_ARG_POINTER(aReturnValue);

  *aReturnValue = nsnull;

  PRUint16 typeValue;
  nsresult rv = aSource->GetDataType(&typeValue);
  NS_ENSURE_SUCCESS(rv, rv);

  if (typeValue != nsIDataType::VTYPE_ARRAY) {
    return NS_ERROR_FAILURE;
  }

  nsIID iid;
  PRUint32 count;
  void* array;
  rv = aSource->GetAsArray(&typeValue, &iid, &count, &array);
  NS_ENSURE_SUCCESS(rv, rv);
    
  if (typeValue != nsIDataType::VTYPE_UINT8) {
    return NS_ERROR_FAILURE;
  }

  char* encodedVal = PL_Base64Encode(NS_STATIC_CAST(const char*, array), count, nsnull);
  if (!encodedVal) {
    return NS_ERROR_FAILURE;
  }
  nsAdoptingCString encodedString(encodedVal);

  nsAutoString name, ns;
  if (aName.IsEmpty()) {
    // If we don't have a name, we pick soapenc:base64Binary.
    rv = aEncoding->GetStyleURI(ns);
    NS_ENSURE_SUCCESS(rv, rv);
    name.Append(gSOAPStrings->kBase64BinarySchemaType);
  }
  else {
    name = aName;
    // ns remains empty. This is ok.
  }

  nsCOMPtr<nsIDOMDocument> document;
  rv = aDestination->GetOwnerDocument(getter_AddRefs(document));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMElement> element;
  rv = document->CreateElementNS(ns, name, getter_AddRefs(element));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMNode> ignore;
  rv = aDestination->AppendChild(element, getter_AddRefs(ignore));
  NS_ENSURE_SUCCESS(rv, rv);

  if (aSchemaType) {
    nsAutoString typeName, typeNS;
    rv = aSchemaType->GetName(typeName);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = aSchemaType->GetTargetNamespace(typeNS);
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoString qname;
    rv = nsSOAPUtils::MakeNamespacePrefix(nsnull, element, typeNS, qname);
    NS_ENSURE_SUCCESS(rv, rv);
    
    qname.Append(gSOAPStrings->kQualifiedSeparator + typeName);
    
    nsAutoString ns;
    rv = aEncoding->GetExternalSchemaURI(gSOAPStrings->kXSIURI, ns);
    NS_ENSURE_SUCCESS(rv, rv);
    
    rv = element->SetAttributeNS(ns, gSOAPStrings->kXSITypeAttribute, qname);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsCOMPtr<nsIDOMText> text;
  rv = document->CreateTextNode(NS_ConvertASCIItoUTF16(encodedString),
                                getter_AddRefs(text));
  NS_ENSURE_SUCCESS(rv, rv);
    
  rv = element->AppendChild(text, getter_AddRefs(ignore));
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ADDREF(*aReturnValue = element);

  return rv;
}


//  PRUint16

NS_IMETHODIMP
    nsUnsignedShortEncoder::Encode(nsISOAPEncoding * aEncoding,
                                   nsIVariant * aSource,
                                   const nsAString & aNamespaceURI,
                                   const nsAString & aName,
                                   nsISchemaType * aSchemaType,
                                   nsISOAPAttachments * aAttachments,
                                   nsIDOMElement * aDestination,
                                   nsIDOMElement * *aReturnValue)
{
  NS_ENSURE_ARG_POINTER(aEncoding);
  NS_ENSURE_ARG_POINTER(aDestination);
  NS_ENSURE_ARG_POINTER(aReturnValue);
  *aReturnValue = nsnull;
  nsresult rc;
  PRUint16 f;
  rc = aSource->GetAsUint16(&f);        //  Get as a long number.
  if (NS_FAILED(rc))
    return rc;
  char *ptr = PR_smprintf("%u", (PRUint32) f);
  if (!ptr)
    return NS_ERROR_OUT_OF_MEMORY;
  nsAutoString value;
  CopyASCIItoUCS2(nsDependentCString(ptr), value);
  PR_smprintf_free(ptr);
  return EncodeSimpleValue(aEncoding, value,
                           aNamespaceURI, aName, aSchemaType, aDestination,
                           aReturnValue);
}

//  Unsigned Byte

NS_IMETHODIMP
    nsUnsignedByteEncoder::Encode(nsISOAPEncoding * aEncoding,
                                  nsIVariant * aSource,
                                  const nsAString & aNamespaceURI,
                                  const nsAString & aName,
                                  nsISchemaType * aSchemaType,
                                  nsISOAPAttachments * aAttachments,
                                  nsIDOMElement * aDestination,
                                  nsIDOMElement * *aReturnValue)
{
  NS_ENSURE_ARG_POINTER(aEncoding);
  NS_ENSURE_ARG_POINTER(aDestination);
  NS_ENSURE_ARG_POINTER(aReturnValue);
  *aReturnValue = nsnull;
  nsresult rc;
  PRUint8 f;
  rc = aSource->GetAsUint8(&f);        //  Get as a long number.
  if (NS_FAILED(rc))
    return rc;
  char *ptr = PR_smprintf("%u", (PRUint32) f);
  if (!ptr)
    return NS_ERROR_OUT_OF_MEMORY;
  nsAutoString value;
  CopyASCIItoUCS2(nsDependentCString(ptr), value);
  PR_smprintf_free(ptr);
  return EncodeSimpleValue(aEncoding, value,
                           aNamespaceURI, aName, aSchemaType, aDestination,
                           aReturnValue);
}

NS_IMETHODIMP
    nsDefaultEncoder::Decode(nsISOAPEncoding * aEncoding,
                             nsIDOMElement * aSource,
                             nsISchemaType * aSchemaType,
                             nsISOAPAttachments * aAttachments,
                             nsIVariant ** _retval)
{
  NS_ENSURE_ARG_POINTER(aEncoding);
  NS_ENSURE_ARG_POINTER(aSource);
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = nsnull;
  nsCOMPtr<nsISOAPEncoding> encoding = aEncoding;        //  First, handle encoding redesignation, if any

  {
    nsCOMPtr<nsIDOMAttr> enc;
    nsresult rv =
        aSource->
        GetAttributeNodeNS(*gSOAPStrings->kSOAPEnvURI[mSOAPVersion],
                           gSOAPStrings->kEncodingStyleAttribute,
                           getter_AddRefs(enc));
    if (NS_FAILED(rv))
      return rv;
    if (enc) {
      nsAutoString oldstyle;
      rv = encoding->GetStyleURI(oldstyle);
      if (NS_FAILED(rv))
        return rv;
      nsAutoString style;
      rv = enc->GetNodeValue(style);
      if (NS_FAILED(rv))
        return rv;
      if (!style.Equals(oldstyle)) {
        nsCOMPtr<nsISOAPEncoding> newencoding;
        rv = encoding->GetAssociatedEncoding(style, PR_FALSE,
                                             getter_AddRefs(newencoding));
        if (NS_FAILED(rv))
          return rv;
        if (newencoding) {
          return newencoding->Decode(aSource, aSchemaType, aAttachments, _retval);
        }
      }
    }
  }

  //  Handle xsi:null="true"

  nsAutoString nullstr;
  if (nsSOAPUtils::GetAttribute(aEncoding, aSource, gSOAPStrings->kXSIURI, gSOAPStrings->kNull, nullstr)) {
    if (nullstr.Equals(gSOAPStrings->kTrue)
             || nullstr.Equals(gSOAPStrings->kTrueA)) {
      *_retval = nsnull;
      return NS_OK;
    }
    else if (!(nullstr.Equals(gSOAPStrings->kFalse)
             || nullstr.Equals(gSOAPStrings->kFalseA)))
      return SOAP_EXCEPTION(NS_ERROR_ILLEGAL_VALUE,"SOAP_NILL_VALUE","The value of the nill attribute must be true or false.");
  }

  nsCOMPtr<nsISchemaType> type = aSchemaType;
  nsCOMPtr<nsISOAPDecoder> decoder;  //  All that comes out of this block is decoder, type, and some checks.
  {                        //  Look up type element and schema attribute, if possible
    nsCOMPtr<nsISchemaType> subType;
    nsCOMPtr<nsISchemaCollection> collection;
    nsresult rc =
        aEncoding->GetSchemaCollection(getter_AddRefs(collection));
    if (NS_FAILED(rc))
      return rc;
    nsAutoString ns;
    nsAutoString name;

    rc = aSource->GetNamespaceURI(name);
    if (NS_FAILED(rc))
      return rc;
    rc = aEncoding->GetInternalSchemaURI(name, ns);
    if (NS_FAILED(rc))
      return rc;
    rc = aSource->GetLocalName(name);
    if (NS_FAILED(rc))
      return rc;
    nsCOMPtr<nsISchemaElement> element;
    rc = collection->GetElement(name, ns, getter_AddRefs(element));
//      if (NS_FAILED(rc)) return rc;
    if (element) {
      rc = element->GetType(getter_AddRefs(subType));
      if (NS_FAILED(rc))
        return rc;
    } else {
      nsAutoString internal;
      rc = aEncoding->GetInternalSchemaURI(ns, internal);
      if (NS_FAILED(rc))
        return rc;
      if (internal.Equals(gSOAPStrings->kSOAPEncURI)) {        //  Last-ditch hack to get undeclared types from SOAP namespace
        if (name.Equals(gSOAPStrings->kArraySOAPType)
            || name.Equals(gSOAPStrings->kStructSOAPType)) {        //  This should not be needed if schema has these declarations
          rc = collection->GetType(name, internal, getter_AddRefs(subType));
        } else {
          rc = collection->GetType(name,
                                   gSOAPStrings->kXSURI,
                                   getter_AddRefs(subType));
        }
      }
//        if (NS_FAILED(rc)) return rc;
    }
    if (!subType)
      subType = type;

    nsCOMPtr<nsISchemaType> subsubType;
    nsAutoString explicitType;
    if (nsSOAPUtils::GetAttribute(aEncoding, aSource, gSOAPStrings->kXSIURI,
                                 gSOAPStrings->kXSITypeAttribute,
                                 explicitType)) {
      rc = nsSOAPUtils::GetNamespaceURI(aEncoding, aSource, explicitType, ns);
      if (NS_FAILED(rc))
        return rc;
      rc = nsSOAPUtils::GetLocalName(explicitType, name);
      if (NS_FAILED(rc))
        return rc;
      rc = collection->GetType(name, ns, getter_AddRefs(subsubType));
//      if (NS_FAILED(rc)) return rc;
    }
    if (!subsubType)
      subsubType = subType;
  
    if (subsubType) {  //  Loop up the hierarchy, to check and look for decoders
      for(;;) {
        nsCOMPtr<nsISchemaType> lookupType = subsubType;
        do {
          if (lookupType == subType) {  //  Tick off the located super classes
            subType = nsnull;
          }
          if (lookupType == type) {  //  Tick off the located super classes
            type = nsnull;
          }
          if (!decoder) {
            nsAutoString schemaType;
            nsAutoString schemaURI;
            nsresult rc = lookupType->GetName(schemaType);
            if (NS_FAILED(rc))
              return rc;
            rc = lookupType->GetTargetNamespace(schemaURI);
            if (NS_FAILED(rc))
              return rc;
            nsAutoString encodingKey;
            SOAPEncodingKey(schemaURI, schemaType, encodingKey);
            rc = aEncoding->GetDecoder(encodingKey, getter_AddRefs(decoder));
            if (NS_FAILED(rc))
              return rc;
          }
          nsCOMPtr<nsISchemaType> supertype;
          rc = GetSupertype(aEncoding, lookupType, getter_AddRefs(supertype));
          if (NS_FAILED(rc))
            return rc;
          lookupType = supertype;
        } while (lookupType);
        if (!type) {
          type = subsubType;
          break;
        }
        decoder = nsnull;
        if (!subType) {
          subType = type;
        }
        subsubType = subType;
      }
    }
  }
  if (!decoder) {
    PRBool simple = PR_TRUE;
    if (type) {
      nsresult rc = HasSimpleValue(type, &simple);
      if (NS_FAILED(rc))
        return rc;
    }
    if (simple) {
      nsCOMPtr<nsIDOMElement> child;
      nsSOAPUtils::GetFirstChildElement(aSource, getter_AddRefs(child));
      simple = !child;
    }
    nsAutoString decodingKey;
    if (!simple) {
      SOAPEncodingKey(gSOAPStrings->kSOAPEncURI,
                      gSOAPStrings->kStructSOAPType, decodingKey);
    } else {
      SOAPEncodingKey(gSOAPStrings->kXSURI,
                      gSOAPStrings->kAnySimpleTypeSchemaType, decodingKey);
    }
    nsresult rc =
      aEncoding->GetDecoder(decodingKey, getter_AddRefs(decoder));
    if (NS_FAILED(rc))
      return rc;
  }
  if (decoder) {
    return decoder->Decode(aEncoding, aSource, type, aAttachments,
                           _retval);
  }
  return SOAP_EXCEPTION(NS_ERROR_NOT_IMPLEMENTED,"SOAP_NO_DECODER_FOR_TYPE","The default decoder finds no decoder for specific type");
}

NS_IMETHODIMP
    nsAnyTypeEncoder::Decode(nsISOAPEncoding * aEncoding,
                             nsIDOMElement * aSource,
                             nsISchemaType * aSchemaType,
                             nsISOAPAttachments * aAttachments,
                             nsIVariant ** _retval)
{
  NS_ENSURE_ARG_POINTER(aEncoding);
  NS_ENSURE_ARG_POINTER(aSource);
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = nsnull;
  PRBool simple = PR_TRUE;
  if (aSchemaType) {
    nsresult rc = HasSimpleValue(aSchemaType, &simple);
    if (NS_FAILED(rc))
      return rc;
  }
  if (simple) {
    nsCOMPtr<nsIDOMElement> child;
    nsSOAPUtils::GetFirstChildElement(aSource, getter_AddRefs(child));
    simple = !child;
  }
  nsAutoString decodingKey;
  if (!simple) {
    SOAPEncodingKey(gSOAPStrings->kSOAPEncURI,
                    gSOAPStrings->kStructSOAPType, decodingKey);
  } else {
    SOAPEncodingKey(gSOAPStrings->kXSURI,
                    gSOAPStrings->kAnySimpleTypeSchemaType, decodingKey);
  }
  nsCOMPtr<nsISOAPDecoder> decoder;
  nsresult rc =
      aEncoding->GetDecoder(decodingKey, getter_AddRefs(decoder));
  if (NS_FAILED(rc))
    return rc;
  if (decoder) {
    return decoder->Decode(aEncoding, aSource,
                         aSchemaType, aAttachments, _retval);
    return rc;
  }
  return SOAP_EXCEPTION(NS_ERROR_NOT_IMPLEMENTED,"SOAP_NO_DECODER_FOR_TYPE","The any type decoder finds no decoder for specific element");
}

static nsresult DecodeStructParticle(nsISOAPEncoding* aEncoding, nsIDOMElement* aElement, 
                               nsISchemaParticle* aParticle, 
                               nsISOAPAttachments * aAttachments, nsISOAPPropertyBagMutator* aDestination,
                               nsIDOMElement** _retElement)
{
  nsresult rc;
  *_retElement = nsnull;
  if (aParticle) {
    PRUint32 minOccurs;
    rc = aParticle->GetMinOccurs(&minOccurs);
    if (NS_FAILED(rc))
      return rc;
    PRUint32 maxOccurs;
    rc = aParticle->GetMaxOccurs(&maxOccurs);
    if (NS_FAILED(rc))
      return rc;
    PRUint16 particleType;
    rc = aParticle->GetParticleType(&particleType);
    if (NS_FAILED(rc))
      return rc;
    switch(particleType) {
      case nsISchemaParticle::PARTICLE_TYPE_ELEMENT: {
        if (maxOccurs > 1) {  //  Todo: Try to make this thing work as an array?
          return NS_ERROR_NOT_AVAILABLE; //  For now, we just try something else if we can (recoverable)
        }
        nsCOMPtr<nsISchemaElement> element = do_QueryInterface(aParticle);
        nsAutoString name;
        rc = element->GetTargetNamespace(name);
        if (NS_FAILED(rc))
          return rc;
        if (!name.IsEmpty()) {
          rc = NS_ERROR_NOT_AVAILABLE; //  No known way to use namespace qualification in struct
        }
        else {
          rc = element->GetName(name);
          if (NS_FAILED(rc))
            return rc;
          nsAutoString ename;
          if (aElement) {                //  Permits aElement to be null and fail recoverably
            nsAutoString temp;
            rc = aElement->GetNamespaceURI(ename);
            if (NS_FAILED(rc))
              return rc;
            if (ename.IsEmpty()) {  //  Only get an ename if there is an empty namespaceURI
              rc = aElement->GetLocalName(ename);
              if (NS_FAILED(rc))
                return rc;
            }
          }
          if (!ename.Equals(name))
            rc = NS_ERROR_NOT_AVAILABLE; //  The element must be a declaration of the next element
        }
        if (NS_SUCCEEDED(rc)) {
          nsCOMPtr<nsISchemaType> type;
          rc = element->GetType(getter_AddRefs(type));
          if (NS_FAILED(rc))
            return rc;
          nsCOMPtr<nsIVariant> value;
          rc = aEncoding->Decode(aElement, type, aAttachments, getter_AddRefs(value));
          if (NS_FAILED(rc))
            return rc;
          if (!value) {
            nsCOMPtr<nsIWritableVariant> nullVariant(do_CreateInstance("@mozilla.org/variant;1"));
            if (nullVariant) {
              nullVariant->SetAsISupports(nsnull);
              value = do_QueryInterface(nullVariant);
            }
          }
          rc = aDestination->AddProperty(name, value);
          if (NS_FAILED(rc))
            return rc;
          nsSOAPUtils::GetNextSiblingElement(aElement, _retElement);
        }
        if (minOccurs == 0 && rc == NS_ERROR_NOT_AVAILABLE) { //  If we failed recoverably, but we were permitted to, then return success
          *_retElement = aElement;
          NS_IF_ADDREF(*_retElement);
          rc = NS_OK;
        }
        return rc;
      }
      case nsISchemaParticle::PARTICLE_TYPE_MODEL_GROUP: 
      {
        if (maxOccurs > 1) {  //  Todo: Try to make this thing work as an array?
          return NS_ERROR_NOT_AVAILABLE; //  For now, we just try something else if we can (recoverable)
        }
        nsCOMPtr<nsISchemaModelGroup> modelGroup = do_QueryInterface(aParticle);
        PRUint16 compositor;
        rc = modelGroup->GetCompositor(&compositor);
        if (NS_FAILED(rc))
          return rc;
        PRUint32 particleCount;
        rc = modelGroup->GetParticleCount(&particleCount);
        if (NS_FAILED(rc))
          return rc;
        PRUint32 i;
        if (compositor == nsISchemaModelGroup::COMPOSITOR_ALL) {  //  This handles out-of-order appearances.
          nsCOMPtr<nsISupportsArray> all = new nsSupportsArray(); //  Create something we can mutate
          if (!all)
            return NS_ERROR_OUT_OF_MEMORY;

          all->SizeTo(particleCount);
          nsCOMPtr<nsISchemaParticle> child;
          PRBool mangled = PR_FALSE;
          for (i = 0; i < particleCount; i++) {
            rc = modelGroup->GetParticle(i, getter_AddRefs(child));
            if (NS_FAILED(rc))
              return rc;
            rc = all->AppendElement(child);
            if (NS_FAILED(rc))
              return rc;
          }
          nsCOMPtr<nsIDOMElement> next = aElement;
          while (particleCount > 0) {
            for (i = 0; i < particleCount; i++) {
              child = dont_AddRef(NS_STATIC_CAST
                    (nsISchemaParticle*, all->ElementAt(i)));
              nsCOMPtr<nsIDOMElement> after;
              rc = DecodeStructParticle(aEncoding, next, child, aAttachments, aDestination, getter_AddRefs(after));
              if (NS_SUCCEEDED(rc)) {
                next = after;
                mangled = PR_TRUE;
                rc = all->RemoveElementAt(i);
                particleCount--;
                if (NS_FAILED(rc))
                  return rc;
                break;
              }
              if (rc != NS_ERROR_NOT_AVAILABLE) {
                break;
              }
            }
            if (mangled && rc == NS_ERROR_NOT_AVAILABLE) {  //  This detects ambiguous model (non-deterministic choice which fails after succeeding on first)
              rc = SOAP_EXCEPTION(NS_ERROR_ILLEGAL_VALUE,"SOAP_AMBIGUOUS_DECODING","Cannot proceed due to ambiguity or error in content model");
              //  Error is not considered recoverable due to partially-created output.
            }
            if (NS_FAILED(rc))
              break;
          }
          if (NS_SUCCEEDED(rc)) {
            *_retElement = next;
            NS_IF_ADDREF(*_retElement);
          }
          if (minOccurs == 0 && rc == NS_ERROR_NOT_AVAILABLE) {  //  If we succeeded or failed recoverably, but we were permitted to, then return success
            *_retElement = aElement;
            NS_IF_ADDREF(*_retElement);
            rc = NS_OK;
          }
        }
        else {  //  This handles sequences and choices.
          nsCOMPtr<nsIDOMElement> next = aElement;
          for (i = 0; i < particleCount; i++) {
            nsCOMPtr<nsISchemaParticle> child;
            rc = modelGroup->GetParticle(i, getter_AddRefs(child));
            if (NS_FAILED(rc))
              return rc;
            nsCOMPtr<nsIDOMElement> after;
            rc = DecodeStructParticle(aEncoding, next, child, aAttachments, aDestination, getter_AddRefs(after));
            if (NS_SUCCEEDED(rc)) {
               next = after;
            }
            if (compositor == nsISchemaModelGroup::COMPOSITOR_CHOICE) {
              if (rc == NS_ERROR_NOT_AVAILABLE) {
                rc = NS_OK;
              }
              else {
                if (NS_SUCCEEDED(rc)) {
                  *_retElement = next;
                  NS_IF_ADDREF(*_retElement);
                }
                return rc;
              }
            }
            else if (i > 0 && rc == NS_ERROR_NOT_AVAILABLE) {  //  This detects ambiguous model (non-deterministic choice which fails after succeeding on first)
              rc = SOAP_EXCEPTION(NS_ERROR_ILLEGAL_VALUE,"SOAP_AMBIGUOUS_DECODING","Cannot proceed due to ambiguity or error in content model");
              //  Error is not considered recoverable due to partially-created output.
            }
            if (NS_FAILED(rc))
              break;
          }
          if (compositor == nsISchemaModelGroup::COMPOSITOR_CHOICE)
            rc = NS_ERROR_NOT_AVAILABLE;
          if (NS_SUCCEEDED(rc)) {
            *_retElement = next;
            NS_IF_ADDREF(*_retElement);
          }
          if (minOccurs == 0 && rc == NS_ERROR_NOT_AVAILABLE) {  //  If we succeeded or failed recoverably, but we were permitted to, then return success
            *_retElement = aElement;
            NS_IF_ADDREF(*_retElement);
            rc = NS_OK;
          }
        }
        return rc;                    //  Return status
      }
      case nsISchemaParticle::PARTICLE_TYPE_ANY:
//  No model available here (we may wish to handle strict versus lazy, but what does that mean with only local accessor names)
      default:
        break;
    }
  }

  nsCOMPtr<nsIDOMElement> child = aElement;
  while (child) {
    nsAutoString name;
    nsAutoString namespaceURI;
    nsCOMPtr<nsIVariant>value;
    rc = child->GetLocalName(name);
    if (NS_FAILED(rc))
      return rc;
    rc = child->GetNamespaceURI(namespaceURI);
    if (NS_FAILED(rc))
      return rc;
    if (!namespaceURI.IsEmpty()) {    //  If we ever figure out what to do with namespaces, get an internal one
      return SOAP_EXCEPTION(NS_ERROR_ILLEGAL_VALUE,"SOAP_GLOBAL_ACCESSOR","Decoded struct contained global accessor, which does not map well into a property name.");
    }
    rc = aEncoding->Decode(child, nsnull, aAttachments, getter_AddRefs(value));
    if (NS_FAILED(rc))
      return rc;
    if (!value) {
      nsCOMPtr<nsIWritableVariant> nullVariant(do_CreateInstance("@mozilla.org/variant;1"));
      if (nullVariant) {
        nullVariant->SetAsISupports(nsnull);
        value = do_QueryInterface(nullVariant);
      }
    }
    rc = aDestination->AddProperty(name, value);
    if (NS_FAILED(rc))
      return rc;

    nsCOMPtr<nsIDOMElement> nextchild;
    nsSOAPUtils::GetNextSiblingElement(child, getter_AddRefs(nextchild));
    child = nextchild;
  }
  *_retElement = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
    nsStructEncoder::Decode(nsISOAPEncoding * aEncoding,
                             nsIDOMElement * aSource,
                             nsISchemaType * aSchemaType,
                             nsISOAPAttachments * aAttachments,
                             nsIVariant ** _retval)
{
  NS_ENSURE_ARG_POINTER(aEncoding);
  NS_ENSURE_ARG_POINTER(aSource);
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = nsnull;
  nsresult rc;
  nsCOMPtr<nsISOAPPropertyBagMutator> mutator = do_CreateInstance(NS_SOAPPROPERTYBAGMUTATOR_CONTRACTID, &rc);
  if (NS_FAILED(rc))
    return rc;
  nsCOMPtr<nsISchemaModelGroup>modelGroup;
  if (aSchemaType) {
    nsCOMPtr<nsISchemaComplexType>ctype = do_QueryInterface(aSchemaType);
    if (ctype) {
      rc = ctype->GetModelGroup(getter_AddRefs(modelGroup));
      if (NS_FAILED(rc))
        return rc;
    }
  }
  nsCOMPtr<nsIDOMElement> child;
  nsSOAPUtils::GetFirstChildElement(aSource, getter_AddRefs(child));
  nsCOMPtr<nsIDOMElement> result;
  rc =  DecodeStructParticle(aEncoding, child, modelGroup, aAttachments, mutator, getter_AddRefs(result));
  if (NS_SUCCEEDED(rc)  //  If there were elements left over, then we failed to decode everything.
      && result)
    rc = SOAP_EXCEPTION(NS_ERROR_ILLEGAL_VALUE,"SOAP_LEFTOVERS","Decoded struct contained extra items not mentioned in the content model.");
  if (NS_FAILED(rc))
    return rc;
  nsCOMPtr<nsIPropertyBag> bag;
  rc = mutator->GetPropertyBag(getter_AddRefs(bag));
  if (NS_FAILED(rc))
    return rc;
  nsCOMPtr<nsIWritableVariant> p =
      do_CreateInstance(NS_VARIANT_CONTRACTID, &rc);
  if (NS_FAILED(rc))
    return rc;
  rc = p->SetAsInterface(NS_GET_IID(nsIPropertyBag), bag);
  if (NS_FAILED(rc))
    return rc;
  *_retval = p;
  NS_ADDREF(*_retval);
  return NS_OK;
}

NS_IMETHODIMP
    nsAnySimpleTypeEncoder::Decode(nsISOAPEncoding * aEncoding,
                                   nsIDOMElement * aSource,
                                   nsISchemaType * aSchemaType,
                                   nsISOAPAttachments * aAttachments,
                                   nsIVariant ** _retval)
{
  NS_ENSURE_ARG_POINTER(aEncoding);
  NS_ENSURE_ARG_POINTER(aSource);
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = nsnull;
  nsAutoString value;
  nsresult rc = nsSOAPUtils::GetElementTextContent(aSource, value);
  if (NS_FAILED(rc))
    return rc;
  nsCOMPtr<nsIWritableVariant> p =
      do_CreateInstance(NS_VARIANT_CONTRACTID, &rc);
  if (NS_FAILED(rc))
    return rc;
  rc = p->SetAsAString(value);
  if (NS_FAILED(rc))
    return rc;
  *_retval = p;
  NS_ADDREF(*_retval);
  return NS_OK;
}

/**
 * Extract multiple bracketted numbers from the end of
 * the string and return the string with the number
 * removed or return the original string and -1.  Either
 * the number of dimensions or the size of any particular
 * dimension can be returned as -1.  An over-all 0
 * means that either there were no dimensions or there
 * was a fundamental problem interpreting it.  A
 * -1 on any particular size of a dimension means that
 * that particular size was not available or was not
 * interpretable.  That may be a recoverable error
 * if the values represented a size, because we can
 * manually scan the array, but that shouldbe fatal
 * if specifying a position.  In these cases, the
 * bracketted values are removed.
 */
static PRUint32 DecodeArrayDimensions(const nsAString& src, PRInt32* aDimensionSizes, nsAString & dst)
{
  dst.Assign(src);
  nsReadingIterator < PRUnichar > i1;
  nsReadingIterator < PRUnichar > i2;
  src.BeginReading(i1);
  src.EndReading(i2);
  if (src.IsEmpty()) return 0;
  while (i1 != i2      //  Loop past white space
    && *(--i2) <= ' ') //  In XML, all valid characters <= space are the only whitespace
    ;
  if (*i2 != ']') {                  //  In this case, not an array dimension
    PRInt32 len = Distance(i1, i2) - 1;  //  This is the size to truncate to at the end.
    dst = Substring(src, 0, len);              //  Truncate the string.
    return 0;                       //  Eliminated white space.
  }

  PRInt32 dimensionCount = 1;    //  Counting the dimensions
  for (;;) {        //  First look for the matching bracket from reverse and commas.
    if (i1 == i2) {                  //  No matching bracket.
      return 0;
    }
    PRUnichar c = *(--i2);
    if (c == '[') {                  //  Matching bracket found!
      break;
    }
    if (c == ',') {
      dimensionCount++;
    }
  }
  PRInt32 len;
  {
    nsReadingIterator < PRUnichar > i3 = i2++;  //  Cover any extra white space
    while (i1 != i3) {      //  Loop past white space
      if (*(--i3) > ' ') { //  In XML, all valid characters <= space are the only whitespace
        i3++;
        break;
      }
    }
    len = Distance(i1, i3);        //  Length remaining in string after operation
  }

  if (dimensionCount > MAX_ARRAY_DIMENSIONS) {  //  Completely ignore it if too many dimensions.
    return 0;
  }

  i1 = i2;
  src.EndReading(i2);
  while (*(--i2) != ']')           //  Find end bracket again
    ;

  dimensionCount = 0;                           //  Start with first dimension.
  aDimensionSizes[dimensionCount] = -1;
  PRBool finished = PR_FALSE;      //  Disallow space within numbers

  while (i1 != i2) {
    PRUnichar c = *(i1++);
    if (c < '0' || c > '9') {
//  There may be slightly more to do here if alternative radixes are supported.
      if (c <= ' ') {              //  In XML, all valid characters <= space are the only whitespace
        if (aDimensionSizes[dimensionCount] >= 0) {
          finished = PR_TRUE;
        }
      }
      else if (c == ',') {         //  Introducing new dimension
        aDimensionSizes[++dimensionCount] = -1;             //  Restarting it at -1
        finished = PR_FALSE;
      }
      else
        return 0;                 //  Unrecognized character
    } else {
      if (finished) {
        return 0;                 //  Numbers not allowed after white space
      }
      if (aDimensionSizes[dimensionCount] == -1)
        aDimensionSizes[dimensionCount] = 0;
      if (aDimensionSizes[dimensionCount] < 214748364) {
        aDimensionSizes[dimensionCount] = aDimensionSizes[dimensionCount] * 10 + c - '0';
      }
      else {
        return 0;                 //  Number got too big.
      }
    }
  }
  dst = Substring(src, 0, len);              //  Truncate the string.
  return dimensionCount + 1;                    //  Return the number of dimensions
}

/**
 * Extract multiple bracketted numbers from the end of
 * the string and reconcile with a passed-in set of
 * dimensions, computing the offset in the array.
 * Presumes that the caller already knows the dimensions
 * fit into 32-bit signed integer, due to computing
 * total size of array.
 *
 * If there is extra garbage within the 
 * Any blank or unreadable dimensions or extra garbage
 * within the string result in a return of -1, which is
 * bad wherever a position string was interpreted.
 */
static PRInt32 DecodeArrayPosition(const nsAString& src, PRUint32 aDimensionCount, PRInt32* aDimensionSizes)
{
  PRInt32 pos[MAX_ARRAY_DIMENSIONS];
  nsAutoString leftover;
  PRUint32 i = DecodeArrayDimensions(src, pos, leftover);
  if (i != aDimensionCount                //  Easy cases where something went wrong
    || !leftover.IsEmpty())
    return -1;
  PRInt32 result = 0;
  for (i = 0;;) {
    PRInt32 next = pos[i];
    if (next == -1
      || next >= aDimensionSizes[i])
      return -1;
    result = result + next;
    if (++i < aDimensionCount)                 //  Multiply for next round.
      result = result * aDimensionSizes[i];
    else
      break;
  }
  return result;
}

/**
 * Expand the resulting array out into a nice pseudo-multi-dimensional
 * array.  We trust that the caller guaranteed aDimensionCount >= 1 and that 
 * the other sizes are reasonable (or they couldn't pass us a resultant 
 * array).  * The result is produced recursively as:
 * an array [of arrays [...]] of the specified type.  
 * Variants are used to embed arrays inside of * arrays.
 */
static nsresult CreateArray(nsIWritableVariant* aResult, PRUint16 aType, const nsIID* aIID, 
  PRUint32 aDimensionCount, PRInt32* aDimensionSizes, PRUint32 aSizeof, PRUint8* aArray)
{
  if (aSizeof == 0) {  //  Variants do not support construction of null-sized arrays
    return aResult->SetAsEmptyArray();
  }
  if (aDimensionCount > 1) {                  //  We cannot reuse variants because they are kept by resulting array
    PRInt32 count = aDimensionSizes[0];
    PRUint32 size = aSizeof / count;
    PRInt32 i;
    nsIVariant** a = new nsIVariant*[count];  //  Create variant array.
    if (!a)
      return NS_ERROR_OUT_OF_MEMORY;

    nsresult rc = NS_OK;

    for (i = 0; i < count; i++) {
      nsCOMPtr<nsIWritableVariant> v = do_CreateInstance(NS_VARIANT_CONTRACTID, &rc);
      if (NS_FAILED(rc))
        break;
      nsresult rc = CreateArray(v, aType, aIID, aDimensionCount - 1, aDimensionSizes + 1,
        size, aArray);
      if (NS_FAILED(rc))
        break;
      a[i] = v;
      NS_ADDREF(a[i]);                       //  Addref for array reference
      aArray += size;
    }
    if (NS_SUCCEEDED(rc)) {
      rc = aResult->SetAsArray(nsIDataType::VTYPE_INTERFACE_IS,&NS_GET_IID(nsIVariant),count,a);
    }
    for (i = 0; i < count; i++) {            //  Release variants for array
      nsIVariant* v = a[i];
      if (v)
        NS_RELEASE(v);
    }
    delete[] a;
    return rc;
  }
  else {
    return aResult->SetAsArray(aType,aIID,aDimensionSizes[0],aArray);
  }
}

//  Incomplete -- becomes very complex due to variant arrays
NS_IMETHODIMP
    nsArrayEncoder::Decode(nsISOAPEncoding * aEncoding,
                           nsIDOMElement * aSource,
                           nsISchemaType * aSchemaType,
                           nsISOAPAttachments * aAttachments,
                           nsIVariant ** _retval)
{
  NS_ENSURE_ARG_POINTER(aEncoding);
  NS_ENSURE_ARG_POINTER(aSource);
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = nsnull;
  nsAutoString ns;
  nsAutoString name;
  nsCOMPtr<nsISchemaType> schemaArrayType;
  nsAutoString value;
  PRUint32 dimensionCount = 0;                  //  Number of dimensions
  PRInt32 dimensionSizes[MAX_ARRAY_DIMENSIONS];
  PRInt32 size = -1;
  nsresult rc;
  PRUint32 i;
  if (aSchemaType) {
    PRUint16 type;
    nsresult rc = aSchemaType->GetSchemaType(&type);
    if (NS_FAILED(rc))
      return rc;
    if (type == nsISchemaType::SCHEMA_TYPE_COMPLEX) {
      nsCOMPtr<nsISchemaComplexType> ct = do_QueryInterface(aSchemaType);
      nsresult rc = ct->GetArrayDimension(&dimensionCount);
      if (NS_FAILED(rc))
        return rc;
      rc = ct->GetArrayType(getter_AddRefs(schemaArrayType));
      if (NS_FAILED(rc))
        return rc;
    }
  }
  if (nsSOAPUtils::GetAttribute(aEncoding, aSource, gSOAPStrings->kSOAPEncURI,
                                gSOAPStrings->kSOAPArrayTypeAttribute, value)) {
    nsAutoString dst;
    PRUint32 n = DecodeArrayDimensions(value, dimensionSizes, dst);
    if (n > 0) {
      if (dimensionCount == n
        || dimensionCount == 0) {
        dimensionCount = n;
      }
      else {
        return SOAP_EXCEPTION(NS_ERROR_ILLEGAL_VALUE,"SOAP_WRONG_ARRAY_SIZE","Array declares different number of dimensions from what schema declared.");
        //  We cannot get conflicting information from schema and content.
      }
    }
    value.Assign(dst);

    if (dimensionCount > 0) {
      PRInt64 tot = 1;  //  Collect in 64 bits, just to make sure it fits
      for (i = 0; i < dimensionCount; i++) {
        PRInt32 next = dimensionSizes[i];
        if (next == -1) {
          tot = -1;
          break;
        }
        tot = tot * next;
        if (tot > 0x7fffffff) {
          return SOAP_EXCEPTION(NS_ERROR_ILLEGAL_VALUE,"SOAP_ARRAY_TOO_BIG","When decoding an object as an array, the total count of items exceeded maximum.");
        }
      }
      size = (PRInt32)tot;
    }
    else {
      return SOAP_EXCEPTION(NS_ERROR_ILLEGAL_VALUE,"SOAP_ARRAY_UNDECLARED","Array type did not end with proper array dimensions.");
      //  A dimension count must be part of the arrayType
    }

    //  The array type is either array if ']' or other specific type.
    nsCOMPtr<nsISchemaCollection> collection;
    rc = aEncoding->GetSchemaCollection(getter_AddRefs(collection));
    if (NS_FAILED(rc))
      return rc;
    if (value.Last() ==']') {
      ns.Assign(gSOAPStrings->kSOAPEncURI);
      name.Assign(gSOAPStrings->kArraySOAPType);
    }
    else {
      rc = nsSOAPUtils::GetNamespaceURI(aEncoding, aSource, value, ns);
      if (NS_FAILED(rc))
        return rc;
      rc = nsSOAPUtils::GetLocalName(value, name);
      if (NS_FAILED(rc))
        return rc;
    }
    nsCOMPtr<nsISchemaType> subtype;
    rc = collection->GetType(name, ns, getter_AddRefs(subtype));
//      if (NS_FAILED(rc)) return rc;
    if (!subtype)
      subtype = schemaArrayType;
  
    if (subtype) {  //  Loop up the hierarchy, to ensure suitability of subtype
      if (schemaArrayType) {
        nsCOMPtr<nsISchemaType> lookupType = subtype;
        do {
          if (lookupType == schemaArrayType) {  //  Tick off the located super classes
            schemaArrayType = nsnull;
            break;
          }
          nsCOMPtr<nsISchemaType> supertype;
          rc = GetSupertype(aEncoding, lookupType, getter_AddRefs(supertype));
          if (NS_FAILED(rc))
            return rc;
          lookupType = supertype;
        } while (lookupType);
      }
      if (schemaArrayType)  //  If the proper subclass relationship didn't exist, then error return.
        return SOAP_EXCEPTION(NS_ERROR_ILLEGAL_VALUE,"SOAP_ARRAY_TYPE","The type of the array must be a subclass of the declared type.");
      schemaArrayType = subtype;    //  If they did, then we now have a new, better type.
    }
  }
  PRUint32 offset;           //  Computing offset trickier, because size may be unspecified.
  if (nsSOAPUtils::GetAttribute(aEncoding, aSource, gSOAPStrings->kSOAPEncURI,
                                gSOAPStrings->kSOAPArrayOffsetAttribute, value)) {
    PRInt32 pos[MAX_ARRAY_DIMENSIONS];
    nsAutoString leftover;
    offset = DecodeArrayDimensions(value, pos, leftover);
    if (dimensionCount == 0)
      dimensionCount = offset;
    if (offset == 0        //  We have to understand this or report an error
        || offset != dimensionCount      //  But the offset does not need to be understood
        || !leftover.IsEmpty())
      return SOAP_EXCEPTION(NS_ERROR_ILLEGAL_VALUE,"SOAP_ARRAY_OFFSET","Illegal value given for array offset");
    PRInt32 old0 = dimensionSizes[0];
    if (dimensionSizes[0] == -1) {    //  It is OK to have a offset where dimension 0 is unspecified
       dimensionSizes[0] = 2147483647;
    }
    offset  = 0;
    for (i = 0;;) {
      PRInt64 next = pos[i];
      if (next == -1
        || next >= dimensionSizes[i]) {
        rc = NS_ERROR_ILLEGAL_VALUE;
        break;
      }
      next = (offset + next);
      if (next > 2147483647) {
        rc = NS_ERROR_ILLEGAL_VALUE;
        break;
      }
      offset = (PRInt32)next;
      if (++i < dimensionCount) {
        next = offset * dimensionSizes[i];
        if (next > 2147483647) {
          rc = NS_ERROR_ILLEGAL_VALUE;
          break;
        }
        offset = (PRInt32)next;
      }
      else {
        rc = NS_OK;
        break;
      }
    }
    if (NS_FAILED(rc))
      return SOAP_EXCEPTION(NS_ERROR_ILLEGAL_VALUE,"SOAP_ARRAY_OFFSET","Illegal value given for array offset");
    dimensionSizes[0] = old0;
  }
  else {
    offset = 0;
  }
  if (size == -1) {  //  If no known size, we have to go through and pre-count.
    nsCOMPtr<nsIDOMElement> child;
    nsSOAPUtils::GetFirstChildElement(aSource, getter_AddRefs(child));
    PRInt32 pp[MAX_ARRAY_DIMENSIONS];
    if (dimensionCount != 0) {
      for (i = dimensionCount; i-- != 0;) {
        pp[i] = 0;
      }
    }
    size = 0;
    PRInt32 next = offset;
    while (child) {
      nsAutoString pos;
      if (nsSOAPUtils::GetAttribute(aEncoding, aSource, gSOAPStrings->kSOAPEncURI,
                                    gSOAPStrings->kSOAPArrayPositionAttribute, pos)) {
        nsAutoString leftover;
        PRInt32 inc[MAX_ARRAY_DIMENSIONS];
        i = DecodeArrayDimensions(pos, inc, leftover);
        if (i == 0        //  We have to understand this or report an error
            || !leftover.IsEmpty()
            || (dimensionCount !=0 
               && dimensionCount != i))
          return SOAP_EXCEPTION(NS_ERROR_ILLEGAL_VALUE,"SOAP_ARRAY_POSITION","Illegal value given for array element position");
        if (dimensionCount == 0) {
          dimensionCount = i;             //  If we never had dimension count before, we do now.
          for (i = dimensionCount; i-- != 0;) {
            pp[i] = 0;
          }
        }
        for (i = 0; i < dimensionCount; i++) {
          PRInt32 n = inc[i];
          if (n == -1) {  //  Positions must be concrete
            return SOAP_EXCEPTION(NS_ERROR_ILLEGAL_VALUE,"SOAP_ARRAY_POSITION","Illegal value given for array element position");
          }
          if (n >= pp[i])
            pp[i] = n + 1;
        }
      }
      else {
        next++;            //  Keep tabs on how many unnumbered items there are
      }

      nsCOMPtr<nsIDOMElement> nextchild;
      nsSOAPUtils::GetNextSiblingElement(child, getter_AddRefs(nextchild));
      child = nextchild;
    }
    if (dimensionCount == 0) {         //  If unknown or 1 dimension, unpositioned entries can help
      dimensionCount = 1;
      pp[0] = next;
    }
    else if (dimensionCount == 1
      && next > pp[0]) {
      pp[0] = next;
    }
    PRInt64 tot = 1;  //  Collect in 64 bits, just to make sure it fits
    for (i = 0; i < dimensionCount; i++) {
      PRInt32 next = dimensionSizes[i];
      if (next == -1) {          //  Only derive those with no other declaration
        dimensionSizes[i] = next = pp[i];
      }
      tot = tot * next;
      if (tot > 0x7fffffff) {
        return SOAP_EXCEPTION(NS_ERROR_ILLEGAL_VALUE,"SOAP_ARRAY_TOO_BIG","When decoding an object as an array, the total count of items exceeded maximum.");
      }
    }
    size = (PRInt32)tot;  //  At last, we know the dimensions of the array.
  }

//  After considerable work, we may have a schema type and a size.
  
  nsCOMPtr<nsIWritableVariant> result = do_CreateInstance(NS_VARIANT_CONTRACTID, &rc);
  PRInt32 si;

#define DECODE_ARRAY(XPType, VTYPE, iid, Convert, Free) \
      XPType* a = new XPType[size];\
      if (!a)\
        return NS_ERROR_OUT_OF_MEMORY;\
      for (si = 0; si < size; si++) a[si] = 0;\
      nsCOMPtr<nsIDOMElement> child;\
      nsSOAPUtils::GetFirstChildElement(aSource, getter_AddRefs(child));\
      PRUint32 next = offset;\
      while (child) {\
        nsAutoString pos;\
        PRInt32 p;\
        if (nsSOAPUtils::GetAttribute(aEncoding, aSource, gSOAPStrings->kSOAPEncURI,\
                                      gSOAPStrings->kSOAPArrayPositionAttribute, pos)) {\
          p = DecodeArrayPosition(pos, dimensionCount, dimensionSizes);\
          if (p == -1) {\
            rc = NS_ERROR_ILLEGAL_VALUE;\
            break;\
          }\
        }\
        else {\
          p = next++;\
        }\
        if (p >= size\
           || a[p]) {\
          rc = NS_ERROR_ILLEGAL_VALUE;\
          break;\
        }\
        nsCOMPtr<nsIVariant> v;\
 \
        rc = aEncoding->Decode(child, schemaArrayType, aAttachments, getter_AddRefs(v));\
        if (NS_FAILED(rc))\
          break;\
        Convert \
\
        nsCOMPtr<nsIDOMElement> next;\
        nsSOAPUtils::GetNextSiblingElement(child, getter_AddRefs(next));\
        child = next;\
      }\
      if (NS_SUCCEEDED(rc)) {\
        rc = CreateArray(result, nsIDataType::VTYPE_##VTYPE,iid,dimensionCount,dimensionSizes,sizeof(a[0])*size,(PRUint8*)a);\
      }\
      Free\
      delete[] a;\

#define DECODE_SIMPLE_ARRAY(XPType, VType, VTYPE) \
  DECODE_ARRAY(XPType, VTYPE, nsnull, rc = v->GetAs##VType(a + p);if(NS_FAILED(rc))break;,do{}while(0);)

  if (rc == NS_ERROR_ILLEGAL_VALUE)
    return SOAP_EXCEPTION(NS_ERROR_ILLEGAL_VALUE,"SOAP_ARRAY_POSITIONS","Colliding array positions discovered.");
  if (NS_FAILED(rc))
    return rc;
  PRBool unhandled = PR_FALSE;
  if (ns.Equals(gSOAPStrings->kXSURI)) {
    if (name.Equals(gSOAPStrings->kStringSchemaType)) {
      DECODE_ARRAY(PRUnichar*,WCHAR_STR,nsnull,rc = v->GetAsWString(a + p);if(NS_FAILED(rc))break;,
                  for (si = 0; si < size; si++) nsMemory::Free(a[si]););
    } else if (name.Equals(gSOAPStrings->kBooleanSchemaType)) {
      DECODE_SIMPLE_ARRAY(PRBool,Bool,BOOL);
    } else if (name.Equals(gSOAPStrings->kFloatSchemaType)) {
      DECODE_SIMPLE_ARRAY(float,Float,FLOAT);
    } else if (name.Equals(gSOAPStrings->kDoubleSchemaType)) {
      DECODE_SIMPLE_ARRAY(double,Double,DOUBLE);
    } else if (name.Equals(gSOAPStrings->kLongSchemaType)) {
      DECODE_SIMPLE_ARRAY(PRInt64,Int64,INT64);
    } else if (name.Equals(gSOAPStrings->kIntSchemaType)) {
      DECODE_SIMPLE_ARRAY(PRInt32,Int32,INT32);
    } else if (name.Equals(gSOAPStrings->kShortSchemaType)) {
      DECODE_SIMPLE_ARRAY(PRInt16,Int16,INT16);
    } else if (name.Equals(gSOAPStrings->kByteSchemaType)) {
      DECODE_SIMPLE_ARRAY(PRUint8,Int8,INT8);
    } else if (name.Equals(gSOAPStrings->kUnsignedLongSchemaType)) {
      DECODE_SIMPLE_ARRAY(PRUint64,Uint64,UINT64);
    } else if (name.Equals(gSOAPStrings->kUnsignedIntSchemaType)) {
      DECODE_SIMPLE_ARRAY(PRUint32,Uint32,UINT32);
    } else if (name.Equals(gSOAPStrings->kUnsignedShortSchemaType)) {
      DECODE_SIMPLE_ARRAY(PRUint16,Uint16,UINT16);
    } else if (name.Equals(gSOAPStrings->kUnsignedByteSchemaType)) {
      DECODE_SIMPLE_ARRAY(PRUint8,Uint8,UINT8);
    } else {
      unhandled = PR_TRUE;
    }
  } else {
    unhandled = PR_TRUE;
  }
  if (unhandled) {  //  Handle all the other cases
    DECODE_ARRAY(nsIVariant*,INTERFACE_IS,&NS_GET_IID(nsIVariant),
      a[p] = v;NS_ADDREF(a[p]);,
      for (si = 0; si < size; si++) NS_RELEASE(a[si]););
  }
  if (NS_FAILED(rc))\
    return rc;
  *_retval = result;
  NS_ADDREF(*_retval);
  return NS_OK;
}

NS_IMETHODIMP
    nsStringEncoder::Decode(nsISOAPEncoding * aEncoding,
                            nsIDOMElement * aSource,
                            nsISchemaType * aSchemaType,
                            nsISOAPAttachments * aAttachments,
                            nsIVariant ** _retval)
{
  NS_ENSURE_ARG_POINTER(aEncoding);
  NS_ENSURE_ARG_POINTER(aSource);
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = nsnull;
  nsAutoString value;
  nsresult rc = nsSOAPUtils::GetElementTextContent(aSource, value);
  if (NS_FAILED(rc))
    return rc;
  nsCOMPtr<nsIWritableVariant> p =
      do_CreateInstance(NS_VARIANT_CONTRACTID, &rc);
  if (NS_FAILED(rc))
    return rc;
  rc = p->SetAsAString(value);
  if (NS_FAILED(rc))
    return rc;
  *_retval = p;
  NS_ADDREF(*_retval);
  return NS_OK;
}

NS_IMETHODIMP
    nsBooleanEncoder::Decode(nsISOAPEncoding * aEncoding,
                             nsIDOMElement * aSource,
                             nsISchemaType * aSchemaType,
                             nsISOAPAttachments * aAttachments,
                             nsIVariant ** _retval)
{
  NS_ENSURE_ARG_POINTER(aEncoding);
  NS_ENSURE_ARG_POINTER(aSource);
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = nsnull;
  nsAutoString value;
  nsresult rc = nsSOAPUtils::GetElementTextContent(aSource, value);
  if (NS_FAILED(rc))
    return rc;
  PRBool b;
  if (value.Equals(gSOAPStrings->kTrue)
      || value.Equals(gSOAPStrings->kTrueA)) {
    b = PR_TRUE;
  } else if (value.Equals(gSOAPStrings->kFalse)
             || value.Equals(gSOAPStrings->kFalseA)) {
    b = PR_FALSE;
  } else
    return SOAP_EXCEPTION(NS_ERROR_ILLEGAL_VALUE,"SOAP_ILLEGAL_BOOLEAN","Illegal value discovered for boolean");

  nsCOMPtr<nsIWritableVariant> p =
      do_CreateInstance(NS_VARIANT_CONTRACTID,&rc);
  if (NS_FAILED(rc))
    return rc;
  p->SetAsBool(b);
  *_retval = p;
  NS_ADDREF(*_retval);
  return NS_OK;
}

NS_IMETHODIMP
    nsDoubleEncoder::Decode(nsISOAPEncoding * aEncoding,
                            nsIDOMElement * aSource,
                            nsISchemaType * aSchemaType,
                            nsISOAPAttachments * aAttachments,
                            nsIVariant ** _retval)
{
  NS_ENSURE_ARG_POINTER(aEncoding);
  NS_ENSURE_ARG_POINTER(aSource);
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = nsnull;
  nsAutoString value;
  nsresult rc = nsSOAPUtils::GetElementTextContent(aSource, value);
  if (NS_FAILED(rc))
    return rc;
  double f;
  PRUint32 n;
  PRInt32 r = PR_sscanf(NS_ConvertUCS2toUTF8(value).get(), " %lf %n", &f, &n);
  if (r == 0 || n < value.Length())
    return SOAP_EXCEPTION(NS_ERROR_ILLEGAL_VALUE,"SOAP_ILLEGAL_DOUBLE","Illegal value discovered for double");

  nsCOMPtr<nsIWritableVariant> p =
      do_CreateInstance(NS_VARIANT_CONTRACTID,&rc);
  if (NS_FAILED(rc))
    return rc;
  p->SetAsDouble(f);
  *_retval = p;
  NS_ADDREF(*_retval);
  return NS_OK;
}

NS_IMETHODIMP
    nsFloatEncoder::Decode(nsISOAPEncoding * aEncoding,
                           nsIDOMElement * aSource,
                           nsISchemaType * aSchemaType,
                           nsISOAPAttachments * aAttachments,
                           nsIVariant ** _retval)
{
  NS_ENSURE_ARG_POINTER(aEncoding);
  NS_ENSURE_ARG_POINTER(aSource);
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = nsnull;
  nsAutoString value;
  nsresult rc = nsSOAPUtils::GetElementTextContent(aSource, value);
  if (NS_FAILED(rc))
    return rc;
  float f;
  PRUint32 n;
  PRInt32 r = PR_sscanf(NS_ConvertUCS2toUTF8(value).get(), " %f %n", &f, &n);
  if (r == 0 || n < value.Length())
    return SOAP_EXCEPTION(NS_ERROR_ILLEGAL_VALUE,"SOAP_ILLEGAL_FLOAT","Illegal value discovered for float");

  nsCOMPtr<nsIWritableVariant> p =
      do_CreateInstance(NS_VARIANT_CONTRACTID,&rc);
  if (NS_FAILED(rc))
    return rc;
  p->SetAsFloat(f);
  *_retval = p;
  NS_ADDREF(*_retval);
  return NS_OK;
}

NS_IMETHODIMP
    nsLongEncoder::Decode(nsISOAPEncoding * aEncoding,
                          nsIDOMElement * aSource,
                          nsISchemaType * aSchemaType,
                          nsISOAPAttachments * aAttachments,
                          nsIVariant ** _retval)
{
  NS_ENSURE_ARG_POINTER(aEncoding);
  NS_ENSURE_ARG_POINTER(aSource);
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = nsnull;
  nsAutoString value;
  nsresult rc = nsSOAPUtils::GetElementTextContent(aSource, value);
  if (NS_FAILED(rc))
    return rc;
  PRInt64 f;
  PRUint32 n;
  PRInt32 r = PR_sscanf(NS_ConvertUCS2toUTF8(value).get(), " %lld %n", &f, &n);
  if (r == 0 || n < value.Length())
    return SOAP_EXCEPTION(NS_ERROR_ILLEGAL_VALUE,"SOAP_ILLEGAL_LONG","Illegal value discovered for long");

  nsCOMPtr<nsIWritableVariant> p =
      do_CreateInstance(NS_VARIANT_CONTRACTID,&rc);
  if (NS_FAILED(rc))
    return rc;
  p->SetAsInt64(f);
  *_retval = p;
  NS_ADDREF(*_retval);
  return NS_OK;
}

NS_IMETHODIMP
    nsIntEncoder::Decode(nsISOAPEncoding * aEncoding,
                         nsIDOMElement * aSource,
                         nsISchemaType * aSchemaType,
                         nsISOAPAttachments * aAttachments,
                         nsIVariant ** _retval)
{
  NS_ENSURE_ARG_POINTER(aEncoding);
  NS_ENSURE_ARG_POINTER(aSource);
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = nsnull;
  nsAutoString value;
  nsresult rc = nsSOAPUtils::GetElementTextContent(aSource, value);
  if (NS_FAILED(rc))
    return rc;
  PRInt32 f;
  PRUint32 n;
  PRInt32 r = PR_sscanf(NS_ConvertUCS2toUTF8(value).get(), " %ld %n", &f, &n);
  if (r == 0 || n < value.Length())
    return SOAP_EXCEPTION(NS_ERROR_ILLEGAL_VALUE,"SOAP_ILLEGAL_INT","Illegal value discovered for int");

  nsCOMPtr<nsIWritableVariant> p =
      do_CreateInstance(NS_VARIANT_CONTRACTID,&rc);
  if (NS_FAILED(rc))
    return rc;
  p->SetAsInt32(f);
  *_retval = p;
  NS_ADDREF(*_retval);
  return NS_OK;
}

NS_IMETHODIMP
    nsShortEncoder::Decode(nsISOAPEncoding * aEncoding,
                           nsIDOMElement * aSource,
                           nsISchemaType * aSchemaType,
                           nsISOAPAttachments * aAttachments,
                           nsIVariant ** _retval)
{
  NS_ENSURE_ARG_POINTER(aEncoding);
  NS_ENSURE_ARG_POINTER(aSource);
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = nsnull;
  nsAutoString value;
  nsresult rc = nsSOAPUtils::GetElementTextContent(aSource, value);
  if (NS_FAILED(rc))
    return rc;
  PRInt16 f;
  PRUint32 n;
  PRInt32 r = PR_sscanf(NS_ConvertUCS2toUTF8(value).get(), " %hd %n", &f, &n);
  if (r == 0 || n < value.Length())
    return SOAP_EXCEPTION(NS_ERROR_ILLEGAL_VALUE,"SOAP_ILLEGAL_SHORT","Illegal value discovered for short");

  nsCOMPtr<nsIWritableVariant> p =
      do_CreateInstance(NS_VARIANT_CONTRACTID,&rc);
  if (NS_FAILED(rc))
    return rc;
  p->SetAsInt16(f);
  *_retval = p;
  NS_ADDREF(*_retval);
  return NS_OK;
}

NS_IMETHODIMP
    nsByteEncoder::Decode(nsISOAPEncoding * aEncoding,
                          nsIDOMElement * aSource,
                          nsISchemaType * aSchemaType,
                          nsISOAPAttachments * aAttachments,
                          nsIVariant ** _retval)
{
  NS_ENSURE_ARG_POINTER(aEncoding);
  NS_ENSURE_ARG_POINTER(aSource);
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = nsnull;
  nsAutoString value;
  nsresult rc = nsSOAPUtils::GetElementTextContent(aSource, value);
  if (NS_FAILED(rc))
    return rc;
  PRInt16 f;
  PRUint32 n;
  PRInt32 r = PR_sscanf(NS_ConvertUCS2toUTF8(value).get(), " %hd %n", &f, &n);
  if (r == 0 || n < value.Length() || f < -128 || f > 127)
    return SOAP_EXCEPTION(NS_ERROR_ILLEGAL_VALUE,"SOAP_ILLEGAL_BYTE","Illegal value discovered for byte");

  nsCOMPtr<nsIWritableVariant> p =
      do_CreateInstance(NS_VARIANT_CONTRACTID,&rc);
  if (NS_FAILED(rc))
    return rc;
  p->SetAsInt8((PRUint8) f);
  *_retval = p;
  NS_ADDREF(*_retval);
  return NS_OK;
}

NS_IMETHODIMP
    nsUnsignedLongEncoder::Decode(nsISOAPEncoding * aEncoding,
                                  nsIDOMElement * aSource,
                                  nsISchemaType * aSchemaType,
                                  nsISOAPAttachments * aAttachments,
                                  nsIVariant ** _retval)
{
  NS_ENSURE_ARG_POINTER(aEncoding);
  NS_ENSURE_ARG_POINTER(aSource);
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = nsnull;
  nsAutoString value;
  nsresult rc = nsSOAPUtils::GetElementTextContent(aSource, value);
  if (NS_FAILED(rc))
    return rc;
  PRUint64 f;
  PRUint32 n;
  PRInt32 r = PR_sscanf(NS_ConvertUCS2toUTF8(value).get(), " %llu %n", &f, &n);
  if (r == 0 || n < value.Length())
    return SOAP_EXCEPTION(NS_ERROR_ILLEGAL_VALUE,"SOAP_ILLEGAL_ULONG","Illegal value discovered for unsigned long");

  nsCOMPtr<nsIWritableVariant> p =
      do_CreateInstance(NS_VARIANT_CONTRACTID,&rc);
  if (NS_FAILED(rc))
    return rc;
  p->SetAsUint64(f);
  *_retval = p;
  NS_ADDREF(*_retval);
  return NS_OK;
}

NS_IMETHODIMP
    nsUnsignedIntEncoder::Decode(nsISOAPEncoding * aEncoding,
                                 nsIDOMElement * aSource,
                                 nsISchemaType * aSchemaType,
                                 nsISOAPAttachments * aAttachments,
                                 nsIVariant ** _retval)
{
  NS_ENSURE_ARG_POINTER(aEncoding);
  NS_ENSURE_ARG_POINTER(aSource);
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = nsnull;
  nsAutoString value;
  nsresult rc = nsSOAPUtils::GetElementTextContent(aSource, value);
  if (NS_FAILED(rc))
    return rc;
  PRUint32 f;
  PRUint32 n;
  PRInt32 r = PR_sscanf(NS_ConvertUCS2toUTF8(value).get(), " %lu %n", &f, &n);
  if (r == 0 || n < value.Length())
    return SOAP_EXCEPTION(NS_ERROR_ILLEGAL_VALUE,"SOAP_ILLEGAL_UINT","Illegal value discovered for unsigned int");

  nsCOMPtr<nsIWritableVariant> p =
      do_CreateInstance(NS_VARIANT_CONTRACTID,&rc);
  if (NS_FAILED(rc))
    return rc;
  p->SetAsUint32(f);
  *_retval = p;
  NS_ADDREF(*_retval);
  return NS_OK;
}

NS_IMETHODIMP
nsBase64BinaryEncoder::Decode(nsISOAPEncoding * aEncoding,
                              nsIDOMElement * aSource,
                              nsISchemaType * aSchemaType,
                              nsISOAPAttachments * aAttachments,
                              nsIVariant ** _retval)
{
  NS_ENSURE_ARG_POINTER(aEncoding);
  NS_ENSURE_ARG_POINTER(aSource);
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = nsnull;


  nsString value;
  nsresult rv = nsSOAPUtils::GetElementTextContent(aSource, value);
  NS_ENSURE_SUCCESS(rv, rv);

  NS_LossyConvertUTF16toASCII valueStr(value);
  valueStr.StripChars(" \n\r\t");

  char* decodedVal = PL_Base64Decode(valueStr.get(), valueStr.Length(), nsnull);
  if (!decodedVal) {
    return SOAP_EXCEPTION(NS_ERROR_ILLEGAL_VALUE,
                          "SOAP_ILLEGAL_BASE64",
                          "Data cannot be decoded as Base64");
  }

  nsCOMPtr<nsIWritableVariant> p = 
    do_CreateInstance(NS_VARIANT_CONTRACTID, &rv);

  if (NS_SUCCEEDED(rv)) {

    rv = p->SetAsArray(nsIDataType::VTYPE_UINT8, nsnull,
                       strlen(decodedVal), decodedVal);
  }

  PR_Free(decodedVal);

  NS_ENSURE_SUCCESS(rv, rv);

  NS_ADDREF(*_retval = p);

  return NS_OK;
}

NS_IMETHODIMP
    nsUnsignedShortEncoder::Decode(nsISOAPEncoding * aEncoding,
                                   nsIDOMElement * aSource,
                                   nsISchemaType * aSchemaType,
                                   nsISOAPAttachments * aAttachments,
                                   nsIVariant ** _retval)
{
  NS_ENSURE_ARG_POINTER(aEncoding);
  NS_ENSURE_ARG_POINTER(aSource);
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = nsnull;
  nsAutoString value;
  nsresult rc = nsSOAPUtils::GetElementTextContent(aSource, value);
  if (NS_FAILED(rc))
    return rc;
  PRUint16 f;
  PRUint32 n;
  PRInt32 r = PR_sscanf(NS_ConvertUCS2toUTF8(value).get(), " %hu %n", &f, &n);
  if (r == 0 || n < value.Length())
    return SOAP_EXCEPTION(NS_ERROR_ILLEGAL_VALUE,"SOAP_ILLEGAL_USHORT","Illegal value discovered for unsigned short");

  nsCOMPtr<nsIWritableVariant> p =
      do_CreateInstance(NS_VARIANT_CONTRACTID,&rc);
  if (NS_FAILED(rc))
    return rc;
  p->SetAsUint16(f);
  *_retval = p;
  NS_ADDREF(*_retval);
  return NS_OK;
}

NS_IMETHODIMP
    nsUnsignedByteEncoder::Decode(nsISOAPEncoding * aEncoding,
                                  nsIDOMElement * aSource,
                                  nsISchemaType * aSchemaType,
                                  nsISOAPAttachments * aAttachments,
                                  nsIVariant ** _retval)
{
  NS_ENSURE_ARG_POINTER(aEncoding);
  NS_ENSURE_ARG_POINTER(aSource);
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = nsnull;
  nsAutoString value;
  nsresult rc = nsSOAPUtils::GetElementTextContent(aSource, value);
  if (NS_FAILED(rc))
    return rc;
  PRUint16 f;
  PRUint32 n;
  PRInt32 r = PR_sscanf(NS_ConvertUCS2toUTF8(value).get(), " %hu %n", &f, &n);
  if (r == 0 || n < value.Length() || f > 255)
    return SOAP_EXCEPTION(NS_ERROR_ILLEGAL_VALUE,"SOAP_ILLEGAL_UBYTE","Illegal value discovered for unsigned byte");

  nsCOMPtr<nsIWritableVariant> p =
      do_CreateInstance(NS_VARIANT_CONTRACTID,&rc);
  if (NS_FAILED(rc))
    return rc;
  p->SetAsUint8((PRUint8) f);
  *_retval = p;
  NS_ADDREF(*_retval);
  return NS_OK;
}
