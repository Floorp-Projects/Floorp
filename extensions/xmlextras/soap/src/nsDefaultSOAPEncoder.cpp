/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

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
#include "nsISupportsPrimitives.h"
#include "nsIDOMParser.h"
#include "nsSOAPUtils.h"
#include "nsISOAPEncoding.h"
#include "nsISOAPEncoder.h"
#include "nsISOAPDecoder.h"
#include "nsISOAPMessage.h"
#include "prprf.h"
#include "prdtoa.h"
#include "nsReadableUtils.h"
#include "nsIDOMNamedNodeMap.h"
#include "nsIDOMAttr.h"
#include "nsPrintfCString.h"
#include "nsISOAPPropertyBagMutator.h"
#include "nsIPropertyBag.h"

NS_NAMED_LITERAL_STRING(kEmpty, "");

NS_NAMED_LITERAL_STRING(kSOAPArrayTypeAttribute, "arrayType");
NS_NAMED_LITERAL_STRING(kSOAPArrayOffsetAttribute, "offset");
NS_NAMED_LITERAL_STRING(kSOAPArrayPositionAttribute, "position");

NS_NAMED_LITERAL_STRING(kAnyTypeSchemaType, "anyType");
NS_NAMED_LITERAL_STRING(kAnySimpleTypeSchemaType, "anySimpleType");
NS_NAMED_LITERAL_STRING(kArraySOAPType, "Array");
NS_NAMED_LITERAL_STRING(kStructSOAPType, "Struct");

NS_NAMED_LITERAL_STRING(kStringSchemaType, "string");
NS_NAMED_LITERAL_STRING(kBooleanSchemaType, "boolean");
NS_NAMED_LITERAL_STRING(kFloatSchemaType, "float");
NS_NAMED_LITERAL_STRING(kDoubleSchemaType, "double");
NS_NAMED_LITERAL_STRING(kLongSchemaType, "long");
NS_NAMED_LITERAL_STRING(kIntSchemaType, "int");
NS_NAMED_LITERAL_STRING(kShortSchemaType, "short");
NS_NAMED_LITERAL_STRING(kByteSchemaType, "byte");
NS_NAMED_LITERAL_STRING(kUnsignedLongSchemaType, "unsignedLong");
NS_NAMED_LITERAL_STRING(kUnsignedIntSchemaType, "unsignedInt");
NS_NAMED_LITERAL_STRING(kUnsignedShortSchemaType, "unsignedShort");
NS_NAMED_LITERAL_STRING(kUnsignedByteSchemaType, "unsignedByte");

NS_NAMED_LITERAL_STRING(kDurationSchemaType, "duration");
NS_NAMED_LITERAL_STRING(kDateTimeSchemaType, "dateTime");
NS_NAMED_LITERAL_STRING(kTimeSchemaType, "time");
NS_NAMED_LITERAL_STRING(kDateSchemaType, "date");
NS_NAMED_LITERAL_STRING(kGYearMonthSchemaType, "gYearMonth");
NS_NAMED_LITERAL_STRING(kGYearSchemaType, "gYear");
NS_NAMED_LITERAL_STRING(kGMonthDaySchemaType, "gMonthDay");
NS_NAMED_LITERAL_STRING(kGDaySchemaType, "gDay");
NS_NAMED_LITERAL_STRING(kGMonthSchemaType, "gMonth");
NS_NAMED_LITERAL_STRING(kHexBinarySchemaType, "hexBinary");
NS_NAMED_LITERAL_STRING(kBase64BinarySchemaType, "base64Binary");
NS_NAMED_LITERAL_STRING(kAnyURISchemaType, "anyURI");
NS_NAMED_LITERAL_STRING(kQNameSchemaType, "QName");
NS_NAMED_LITERAL_STRING(kNOTATIONSchemaType, "NOTATION");
NS_NAMED_LITERAL_STRING(kNormalizedStringSchemaType, "normalizedString");
NS_NAMED_LITERAL_STRING(kTokenSchemaType, "token");
NS_NAMED_LITERAL_STRING(kLanguageSchemaType, "language");
NS_NAMED_LITERAL_STRING(kNMTOKENSchemaType, "NMTOKEN");
NS_NAMED_LITERAL_STRING(kNMTOKENSSchemaType, "NMTOKENS");
NS_NAMED_LITERAL_STRING(kNameSchemaType, "Name");
NS_NAMED_LITERAL_STRING(kNCNameSchemaType, "NCName");
NS_NAMED_LITERAL_STRING(kIDSchemaType, "ID");
NS_NAMED_LITERAL_STRING(kIDREFSchemaType, "IDREF");
NS_NAMED_LITERAL_STRING(kIDREFSSchemaType, "IDREFS");
NS_NAMED_LITERAL_STRING(kENTITYSchemaType, "ENTITY");
NS_NAMED_LITERAL_STRING(kENTITIESSchemaType, "ENTITIES");
NS_NAMED_LITERAL_STRING(kDecimalSchemaType, "decimal");
NS_NAMED_LITERAL_STRING(kIntegerSchemaType, "integer");
NS_NAMED_LITERAL_STRING(kNonPositiveIntegerSchemaType,
			"nonPositiveInteger");
NS_NAMED_LITERAL_STRING(kNegativeIntegerSchemaType, "negativeInteger");
NS_NAMED_LITERAL_STRING(kNonNegativeIntegerSchemaType,
			"nonNegativeInteger");
NS_NAMED_LITERAL_STRING(kPositiveIntegerSchemaType, "positiveInteger");

#define DECLARE_ENCODER(name) \
class ns##name##Encoder : \
  public nsISOAPEncoder, \
  public nsISOAPDecoder \
{\
public:\
  ns##name##Encoder();\
  ns##name##Encoder(PRUint16 aSOAPVersion, PRUint16 aSchemaVersion);\
  virtual ~ns##name##Encoder();\
  PRUint16 mSOAPVersion;\
  PRUint16 mSchemaVersion;\
  NS_DECL_ISUPPORTS\
  NS_DECL_NSISOAPENCODER\
  NS_DECL_NSISOAPDECODER\
};\
NS_IMPL_ISUPPORTS2(ns##name##Encoder,nsISOAPEncoder,nsISOAPDecoder) \
ns##name##Encoder::ns##name##Encoder(PRUint16 aSOAPVersion,PRUint16 aSchemaVersion) {NS_INIT_ISUPPORTS();mSOAPVersion=aSOAPVersion;mSchemaVersion=aSchemaVersion;}\
ns##name##Encoder::~ns##name##Encoder() {}

// All encoders must be first declared and then registered.
DECLARE_ENCODER(Default)
    DECLARE_ENCODER(AnyType)
    DECLARE_ENCODER(AnySimpleType)
    DECLARE_ENCODER(Array)
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
    DECLARE_ENCODER(UnsignedShort) DECLARE_ENCODER(UnsignedByte)
/**
 * This now separates the version with respect to the SOAP specification from the version
 * with respect to the schema version (using the same constants for both).  This permits
 * a user of a SOAP 1.1 or 1.2 encoding to choose which encoding to encode to.
 */
#define REGISTER_ENCODER(name) \
{\
  ns##name##Encoder *handler = new ns##name##Encoder(version,nsISOAPMessage::VERSION_1_1);\
  nsAutoString encodingKey;\
  SOAPEncodingKey(*nsSOAPUtils::kXSURI[nsISOAPMessage::VERSION_1_1], k##name##SchemaType, encodingKey);\
  SetEncoder(encodingKey, handler); \
  SetDecoder(encodingKey, handler); \
  handler = new ns##name##Encoder(version,nsISOAPMessage::VERSION_1_2);\
  SOAPEncodingKey(*nsSOAPUtils::kXSURI[nsISOAPMessage::VERSION_1_2], k##name##SchemaType, encodingKey);\
  SetEncoder(encodingKey, handler); \
  SetDecoder(encodingKey, handler); \
}
nsDefaultSOAPEncoder_1_1::nsDefaultSOAPEncoder_1_1():nsSOAPEncoding(*nsSOAPUtils::kSOAPEncURI[nsISOAPMessage::VERSION_1_1],
	       nsnull,
	       nsnull)
{
  PRUint16 version = nsISOAPMessage::VERSION_1_1;
  {				//  Schema type defaults to default for SOAP level
    nsDefaultEncoder *handler = new nsDefaultEncoder(version, version);
    SetDefaultEncoder(handler);
    SetDefaultDecoder(handler);
  }
  REGISTER_ENCODER(AnyType) REGISTER_ENCODER(AnySimpleType) {	//  There is no ability to preserve non-standard schema choice under single array type.
    nsArrayEncoder *handler = new nsArrayEncoder(version, version);
    nsAutoString encodingKey;
    SOAPEncodingKey(*nsSOAPUtils::kSOAPEncURI[version], kArraySOAPType,
		    encodingKey);
    SetEncoder(encodingKey, handler);
    SetDecoder(encodingKey, handler);
  }
  REGISTER_ENCODER(String)
      REGISTER_ENCODER(Boolean)
      REGISTER_ENCODER(Double)
      REGISTER_ENCODER(Float)
      REGISTER_ENCODER(Long)
      REGISTER_ENCODER(Int)
      REGISTER_ENCODER(Short)
      REGISTER_ENCODER(Byte)
      REGISTER_ENCODER(UnsignedLong)
      REGISTER_ENCODER(UnsignedInt)
REGISTER_ENCODER(UnsignedShort) REGISTER_ENCODER(UnsignedByte)}

nsDefaultSOAPEncoder_1_2::nsDefaultSOAPEncoder_1_2():nsSOAPEncoding(*nsSOAPUtils::kSOAPEncURI[nsISOAPMessage::VERSION_1_2],
	       nsnull,
	       nsnull)
{
  PRUint16 version = nsISOAPMessage::VERSION_1_2;
  {				//  Schema type defaults to default for SOAP level
    nsDefaultEncoder *handler = new nsDefaultEncoder(version, version);
    SetDefaultEncoder(handler);
    SetDefaultDecoder(handler);
  }
  REGISTER_ENCODER(AnyType) REGISTER_ENCODER(AnySimpleType) {	//  There is no ability to preserve non-standard schema choice under single array type.
    nsArrayEncoder *handler = new nsArrayEncoder(version, version);
    nsAutoString encodingKey;
    SOAPEncodingKey(*nsSOAPUtils::kSOAPEncURI[version], kArraySOAPType,
		    encodingKey);
    SetEncoder(encodingKey, handler);
    SetDecoder(encodingKey, handler);
  }
  REGISTER_ENCODER(String)
      REGISTER_ENCODER(Boolean)
      REGISTER_ENCODER(Double)
      REGISTER_ENCODER(Float)
      REGISTER_ENCODER(Long)
      REGISTER_ENCODER(Int)
      REGISTER_ENCODER(Short)
      REGISTER_ENCODER(Byte)
      REGISTER_ENCODER(UnsignedLong)
      REGISTER_ENCODER(UnsignedInt)
REGISTER_ENCODER(UnsignedShort) REGISTER_ENCODER(UnsignedByte)}

//  Here is the implementation of the encoders.
static
    nsresult
EncodeSimpleValue(const nsAString & aValue,
		  const nsAString & aNamespaceURI,
		  const nsAString & aName,
		  nsIDOMElement * aDestination, nsIDOMElement ** _retval)
{
  nsCOMPtr < nsIDOMDocument > document;
  nsresult rc = aDestination->GetOwnerDocument(getter_AddRefs(document));
  if (NS_FAILED(rc))
    return rc;
  nsCOMPtr < nsIDOMElement > element;
  rc = document->CreateElementNS(aNamespaceURI, aName, _retval);
  if (NS_FAILED(rc))
    return rc;
  nsCOMPtr < nsIDOMNode > ignore;
  rc = aDestination->AppendChild(*_retval, getter_AddRefs(ignore));
  if (NS_FAILED(rc))
    return rc;
  if (!aValue.IsEmpty()) {
    nsCOMPtr < nsIDOMText > text;
    rc = document->CreateTextNode(aValue, getter_AddRefs(text));
    if (NS_FAILED(rc))
      return rc;
    return (*_retval)->AppendChild(text, getter_AddRefs(ignore));
  }
  return rc;
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
  nsCOMPtr < nsISOAPEncoder > encoder;
  if (aSchemaType) {
    nsCOMPtr < nsISchemaType > lookupType = aSchemaType;
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
      PRUint16 typevalue;
      rc = lookupType->GetSchemaType(&typevalue);
      if (NS_FAILED(rc))
	return rc;
      if (typevalue == nsISchemaType::SCHEMA_TYPE_COMPLEX) {
	nsCOMPtr < nsISchemaComplexType > oldtype =
	    do_QueryInterface(lookupType);
	oldtype->GetBaseType(getter_AddRefs(lookupType));
      } else {
	break;
      }
    }
    while (lookupType);
  }
  if (!encoder) {
    PRUint16 typevalue;
    if (aSchemaType) {
      nsresult rc = aSchemaType->GetSchemaType(&typevalue);
      if (NS_FAILED(rc))
	return rc;
    } else {
      typevalue = nsISchemaType::SCHEMA_TYPE_COMPLEX;
    }
    nsAutoString encodingKey;
    if (typevalue == nsISchemaType::SCHEMA_TYPE_COMPLEX) {
      SOAPEncodingKey(*nsSOAPUtils::kXSURI[mSchemaVersion],
		      kAnyTypeSchemaType, encodingKey);
    } else {
      SOAPEncodingKey(*nsSOAPUtils::kXSURI[mSchemaVersion],
		      kAnySimpleTypeSchemaType, encodingKey);
    }
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
  return NS_ERROR_NOT_IMPLEMENTED;
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
  nsAutoString nativeSchemaType;
  nsAutoString nativeSchemaURI;
  PRBool mustBeSimple = PR_FALSE;
  PRBool mustBeComplex = PR_FALSE;
  if (aSchemaType) {
    PRUint16 typevalue;
    nsresult rc = aSchemaType->GetSchemaType(&typevalue);
    if (NS_FAILED(rc))
      return rc;
    if (typevalue == nsISchemaType::SCHEMA_TYPE_COMPLEX) {
      mustBeComplex = PR_TRUE;
    } else {
      mustBeSimple = PR_TRUE;
    }
  }
  PRUint16 typevalue;
  nativeSchemaURI.Assign(*nsSOAPUtils::kXSURI[mSchemaVersion]);
  nsresult rc = aSource->GetDataType(&typevalue);
  if (NS_FAILED(rc))
    return rc;
  switch (typevalue) {
  case nsIDataType::VTYPE_INT8:
    if (mustBeComplex)
      return NS_ERROR_ILLEGAL_VALUE;
    nativeSchemaType.Assign(kByteSchemaType);
    break;
  case nsIDataType::VTYPE_INT16:
    if (mustBeComplex)
      return NS_ERROR_ILLEGAL_VALUE;
    nativeSchemaType.Assign(kShortSchemaType);
    break;
  case nsIDataType::VTYPE_INT32:
    if (mustBeComplex)
      return NS_ERROR_ILLEGAL_VALUE;
    nativeSchemaType.Assign(kIntSchemaType);
    break;
  case nsIDataType::VTYPE_INT64:
    if (mustBeComplex)
      return NS_ERROR_ILLEGAL_VALUE;
    nativeSchemaType.Assign(kLongSchemaType);
    break;
  case nsIDataType::VTYPE_UINT8:
    if (mustBeComplex)
      return NS_ERROR_ILLEGAL_VALUE;
    nativeSchemaType.Assign(kUnsignedByteSchemaType);
    break;
  case nsIDataType::VTYPE_UINT16:
    if (mustBeComplex)
      return NS_ERROR_ILLEGAL_VALUE;
    nativeSchemaType.Assign(kUnsignedShortSchemaType);
    break;
  case nsIDataType::VTYPE_UINT32:
    if (mustBeComplex)
      return NS_ERROR_ILLEGAL_VALUE;
    nativeSchemaType.Assign(kUnsignedIntSchemaType);
    break;
  case nsIDataType::VTYPE_UINT64:
    if (mustBeComplex)
      return NS_ERROR_ILLEGAL_VALUE;
    nativeSchemaType.Assign(kUnsignedLongSchemaType);
    break;
  case nsIDataType::VTYPE_FLOAT:
    if (mustBeComplex)
      return NS_ERROR_ILLEGAL_VALUE;
    nativeSchemaType.Assign(kFloatSchemaType);
    break;
  case nsIDataType::VTYPE_DOUBLE:
    if (mustBeComplex)
      return NS_ERROR_ILLEGAL_VALUE;
    nativeSchemaType.Assign(kDoubleSchemaType);
    break;
  case nsIDataType::VTYPE_BOOL:
    if (mustBeComplex)
      return NS_ERROR_ILLEGAL_VALUE;
    nativeSchemaType.Assign(kBooleanSchemaType);
    break;
  case nsIDataType::VTYPE_ARRAY:
    if (mustBeSimple)
      return NS_ERROR_ILLEGAL_VALUE;
    nativeSchemaType.Assign(kArraySOAPType);
    nativeSchemaURI.Assign(*nsSOAPUtils::kSOAPEncURI[mSOAPVersion]);
    break;
#if 0
    {
      nsIID *iid;
      nsCOMPtr < nsISupports > ptr;
      nsresult rv = foo->GetAsInterface(&iid, getter_AddRefs(ptr));
      if (compare_IIDs(iid, NS_GET_IID(nsIPropertyBag)) == 0) {
	schemaType.Assign(kAnyTypeSchemaType);
	break;
      }
    }
    {
      nsCOMPtr < nsISupports > ptr;
      nsresult rv = foo->GetAsISupports(getter_AddRefs(ptr));
    }
#endif
  case nsIDataType::VTYPE_VOID:
  case nsIDataType::VTYPE_EMPTY:
//  Empty may be either simple or complex.
    break;
  case nsIDataType::VTYPE_INTERFACE_IS:
  case nsIDataType::VTYPE_INTERFACE:
    if (mustBeSimple)
      return NS_ERROR_ILLEGAL_VALUE;
    break;
  case nsIDataType::VTYPE_ID:
    if (mustBeComplex)
      return NS_ERROR_ILLEGAL_VALUE;
    nativeSchemaType.Assign(kAnySimpleTypeSchemaType);
    break;
//  case nsIDataType::VTYPE_CHAR_STR:
//  case nsIDataType::VTYPE_WCHAR_STR:
//  case nsIDataType::VTYPE_CHAR:
//  case nsIDataType::VTYPE_WCHAR:
//  case nsIDataType::VTYPE_ASTRING:
  default:
    nativeSchemaType.Assign(kStringSchemaType);
  }
  if (!nativeSchemaType.IsEmpty()) {
    nsCOMPtr < nsISOAPEncoder > encoder;
    nsAutoString encodingKey;
    SOAPEncodingKey(nativeSchemaURI, nativeSchemaType, encodingKey);
    nsresult rc =
	aEncoding->GetEncoder(encodingKey, getter_AddRefs(encoder));
    if (NS_FAILED(rc))
      return rc;
    if (encoder) {
      nsresult rc =
	  encoder->Encode(aEncoding, aSource, aNamespaceURI, aName,
			  aSchemaType, aAttachments, aDestination,
			  aReturnValue);
      if (NS_FAILED(rc))
	return rc;
      if (*aReturnValue		//  If we are not schema-controlled, then add a type attribute as a hint about what we did unless unnamed.
	  && !aSchemaType && !aName.IsEmpty()) {
	nsAutoString type;
	rc = nsSOAPUtils::MakeNamespacePrefix(*aReturnValue,
					      nativeSchemaURI, type);
	if (NS_FAILED(rc))
	  return rc;
	type.Append(nsSOAPUtils::kQualifiedSeparator);
	type.Append(nativeSchemaType);
	rc = (*aReturnValue)->
	    SetAttributeNS(*nsSOAPUtils::kXSIURI[mSchemaVersion],
			   nsSOAPUtils::kXSITypeAttribute, type);
      }
      return rc;
    }
  }

//  Implement complex types with property bags here.
//  Look at the types as we do it later.

  nsIID* iid;
  nsCOMPtr<nsISupports> ptr;
  rc = aSource->GetAsInterface(&iid, getter_AddRefs(ptr));
  if (NS_FAILED(rc))
    return rc;
  if (iid->Equals(NS_GET_IID(nsIPropertyBag))) {  //  Only do explicit property bags for now.
    if (aName.IsEmpty()) {
      rc = EncodeSimpleValue(kEmpty,
			     *nsSOAPUtils::kSOAPEncURI[mSOAPVersion],
			     kStructSOAPType, aDestination,
			     aReturnValue);
    }
    else {
      rc = EncodeSimpleValue(kEmpty,
			   aNamespaceURI, aName, aDestination,
			   aReturnValue);
    }
    if (NS_FAILED(rc))
      return rc;
    nsCOMPtr<nsISimpleEnumerator> e;
    ((nsIPropertyBag*)ptr.get())->GetEnumerator(getter_AddRefs(e));
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
      rc = aEncoding->Encode(value,kEmpty,name,nsnull,aAttachments,*aReturnValue,getter_AddRefs(result));
      if (NS_FAILED(rc))
        return rc;
      rc = e->HasMoreElements(&more);
      if (NS_FAILED(rc))
        return rc;
    }
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
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
  nsresult rc;
  nsAutoString value;
  rc = aSource->GetAsAString(value);
  if (NS_FAILED(rc))
    return rc;
  if (aName.IsEmpty()) {
    return EncodeSimpleValue(value,
			     *nsSOAPUtils::kSOAPEncURI[mSOAPVersion],
			     kAnySimpleTypeSchemaType,
			     aDestination, aReturnValue);
  }
  return EncodeSimpleValue(value,
			   aNamespaceURI, aName, aDestination,
			   aReturnValue);
}

//  Array

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
  PRUint16 type;
  nsIID iid;
  PRUint32 count;
  void *array;
  nsresult rc = aSource->GetAsArray(&type, &iid, &count, &array);	// First, get the array, if any.
  if (NS_FAILED(rc))
    return rc;
  if (aName.IsEmpty()) {	//  Now create the element to hold the array
    rc = EncodeSimpleValue(kEmpty,
			   *nsSOAPUtils::kSOAPEncURI[mSOAPVersion],
			   kArraySOAPType, aDestination, aReturnValue);
  } else {
    rc = EncodeSimpleValue(kEmpty,
			   aNamespaceURI,
			   aName, aDestination, aReturnValue);
  }
  if (NS_FAILED(rc))
    return rc;

//  Here, we ought to find the real array type from the schema or the
//  native type of the array, and attach it as the arrayType attribute
//  and use it when encoding the array elements.  Not complete.
  nsCOMPtr < nsIWritableVariant > p =
      do_CreateInstance(NS_VARIANT_CONTRACTID, &rc);
  if (NS_FAILED(rc))
    return rc;
  switch (type) {
#define ENCODE_SIMPLE_ARRAY(XPType, SOAPType, Format, Source) \
      {\
        nsAutoString value;\
        rc = nsSOAPUtils::MakeNamespacePrefix(*aReturnValue, *nsSOAPUtils::kXSURI[mSchemaVersion], value);\
        if (NS_FAILED(rc)) return rc;\
        value.Append(nsSOAPUtils::kQualifiedSeparator);\
        value.Append(k##SOAPType##SchemaType);\
        if (count) { \
          value.Append(NS_LITERAL_STRING("[") + \
                       NS_ConvertUTF8toUCS2(nsPrintfCString("%d", count)) + \
                       NS_LITERAL_STRING("]")); \
        } \
        rc = (*aReturnValue)->SetAttributeNS(*nsSOAPUtils::kSOAPEncURI[mSOAPVersion], kSOAPArrayTypeAttribute, value);\
        if (NS_FAILED(rc)) return rc;\
	XPType* values = NS_STATIC_CAST(XPType*, array);\
	nsCOMPtr<nsIDOMElement> dummy;\
        for (PRUint32 i = 0; i < count; i++) {\
          char* ptr = PR_smprintf(Format,Source);\
          if (!ptr) return NS_ERROR_OUT_OF_MEMORY;\
          nsAutoString value;\
          value.Assign(NS_ConvertUTF8toUCS2(nsDependentCString(ptr)).get());\
          PR_smprintf_free(ptr);\
          rc = EncodeSimpleValue(value,\
		       *nsSOAPUtils::kSOAPEncURI[mSOAPVersion],\
		       k##SOAPType##SchemaType,\
		       *aReturnValue,\
		       getter_AddRefs(dummy));\
          if (NS_FAILED(rc)) return rc;\
        }\
	return rc;\
      }
  case nsIDataType::VTYPE_INT8:
    ENCODE_SIMPLE_ARRAY(PRUint8, Byte, "%hd",
		    (PRInt16) (signed char) values[i]);
  case nsIDataType::VTYPE_INT16:
    ENCODE_SIMPLE_ARRAY(PRInt16, Short, "%hd", values[i]);
  case nsIDataType::VTYPE_INT32:
    ENCODE_SIMPLE_ARRAY(PRInt32, Int, "%ld", values[i]);
  case nsIDataType::VTYPE_INT64:
    ENCODE_SIMPLE_ARRAY(PRInt64, Long, "%lld", values[i]);
  case nsIDataType::VTYPE_UINT8:
    ENCODE_SIMPLE_ARRAY(PRUint8, UnsignedByte, "%hu", (PRUint16) values[i]);
  case nsIDataType::VTYPE_UINT16:
    ENCODE_SIMPLE_ARRAY(PRUint16, UnsignedShort, "%hu", values[i]);
  case nsIDataType::VTYPE_UINT32:
    ENCODE_SIMPLE_ARRAY(PRUint32, UnsignedInt, "%lu", values[i]);
  case nsIDataType::VTYPE_UINT64:
    ENCODE_SIMPLE_ARRAY(PRUint64, UnsignedLong, "%llu", values[i]);
  case nsIDataType::VTYPE_FLOAT:
    ENCODE_SIMPLE_ARRAY(float, Float, "%f", values[i]);
  case nsIDataType::VTYPE_DOUBLE:
    ENCODE_SIMPLE_ARRAY(double, Double, "%lf", values[i]);
  case nsIDataType::VTYPE_BOOL:
    ENCODE_SIMPLE_ARRAY(PRBool, Boolean, "%hu", (PRUint16) values[i]);
  case nsIDataType::VTYPE_CHAR_STR:
    ENCODE_SIMPLE_ARRAY(char *, String, "%s", values[i]);
  case nsIDataType::VTYPE_WCHAR_STR:
    ENCODE_SIMPLE_ARRAY(PRUnichar *, String, "%s", NS_ConvertUCS2toUTF8
      (values[i]).get());
  case nsIDataType::VTYPE_CHAR:
    ENCODE_SIMPLE_ARRAY(char, String, "%c", values[i]);
  case nsIDataType::VTYPE_ASTRING:
    ENCODE_SIMPLE_ARRAY(nsAString, String, "%s",
      NS_ConvertUCS2toUTF8(values[i]).get())
  case nsIDataType::VTYPE_ARRAY:
//  Don't support these array types just now (needs more work).
  case nsIDataType::VTYPE_WCHAR:
  case nsIDataType::VTYPE_ID:
  case nsIDataType::VTYPE_VOID:
  case nsIDataType::VTYPE_EMPTY:
  case nsIDataType::VTYPE_INTERFACE_IS:
  case nsIDataType::VTYPE_INTERFACE:
    break;
  }
  return NS_ERROR_ILLEGAL_VALUE;
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
  nsresult rc;
  nsAutoString value;
  rc = aSource->GetAsAString(value);
  if (NS_FAILED(rc))
    return rc;
  if (aName.IsEmpty()) {
    return EncodeSimpleValue(value,
			     *nsSOAPUtils::kSOAPEncURI[mSOAPVersion],
			     kStringSchemaType,
			     aDestination, aReturnValue);
  }
  return EncodeSimpleValue(value,
			   aNamespaceURI, aName, aDestination,
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
  nsresult rc;
  PRBool b;
  rc = aSource->GetAsBool(&b);
  if (NS_FAILED(rc))
    return rc;
  if (aName.IsEmpty()) {
    return EncodeSimpleValue(b ? nsSOAPUtils::kTrueA : nsSOAPUtils::
			     kFalseA,
			     *nsSOAPUtils::kSOAPEncURI[mSOAPVersion],
			     kBooleanSchemaType, aDestination,
			     aReturnValue);
  }
  return EncodeSimpleValue(b ? nsSOAPUtils::kTrueA : nsSOAPUtils::kFalseA,
			   aNamespaceURI, aName, aDestination,
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
  nsresult rc;
  double f;
  rc = aSource->GetAsDouble(&f);	//  Check that double works.
  if (NS_FAILED(rc))
    return rc;
  char *ptr = PR_smprintf("%lf", f);
  if (!ptr)
    return NS_ERROR_OUT_OF_MEMORY;
  nsAutoString value;
  CopyASCIItoUCS2(nsDependentCString(ptr), value);
  PR_smprintf_free(ptr);
  if (aName.IsEmpty()) {
    return EncodeSimpleValue(value,
			     *nsSOAPUtils::kSOAPEncURI[mSOAPVersion],
			     kDoubleSchemaType,
			     aDestination, aReturnValue);
  }
  return EncodeSimpleValue(value,
			   aNamespaceURI, aName, aDestination,
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
  nsresult rc;
  float f;
  rc = aSource->GetAsFloat(&f);	//  Check that float works.
  if (NS_FAILED(rc))
    return rc;
  char *ptr = PR_smprintf("%f", f);
  if (!ptr)
    return NS_ERROR_OUT_OF_MEMORY;
  nsAutoString value;
  CopyASCIItoUCS2(nsDependentCString(ptr), value);
  PR_smprintf_free(ptr);
  if (aName.IsEmpty()) {
    return EncodeSimpleValue(value,
			     *nsSOAPUtils::kSOAPEncURI[mSOAPVersion],
			     kFloatSchemaType, aDestination, aReturnValue);
  }
  return EncodeSimpleValue(value,
			   aNamespaceURI, aName, aDestination,
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
  nsresult rc;
  PRInt64 f;
  rc = aSource->GetAsInt64(&f);	//  Get as a long number.
  if (NS_FAILED(rc))
    return rc;
  char *ptr = PR_smprintf("%lld", f);
  if (!ptr)
    return NS_ERROR_OUT_OF_MEMORY;
  nsAutoString value;
  CopyASCIItoUCS2(nsDependentCString(ptr), value);
  PR_smprintf_free(ptr);
  if (aName.IsEmpty()) {
    return EncodeSimpleValue(value,
			     *nsSOAPUtils::kSOAPEncURI[mSOAPVersion],
			     kLongSchemaType, aDestination, aReturnValue);
  }
  return EncodeSimpleValue(value,
			   aNamespaceURI, aName, aDestination,
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
  nsresult rc;
  PRInt32 f;
  rc = aSource->GetAsInt32(&f);	//  Get as a long number.
  if (NS_FAILED(rc))
    return rc;
  char *ptr = PR_smprintf("%d", f);
  if (!ptr)
    return NS_ERROR_OUT_OF_MEMORY;
  nsAutoString value;
  CopyASCIItoUCS2(nsDependentCString(ptr), value);
  PR_smprintf_free(ptr);
  if (aName.IsEmpty()) {
    return EncodeSimpleValue(value,
			     *nsSOAPUtils::kSOAPEncURI[mSOAPVersion],
			     kIntSchemaType, aDestination, aReturnValue);
  }
  return EncodeSimpleValue(value,
			   aNamespaceURI, aName, aDestination,
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
  nsresult rc;
  PRInt16 f;
  rc = aSource->GetAsInt16(&f);	//  Get as a long number.
  if (NS_FAILED(rc))
    return rc;
  char *ptr = PR_smprintf("%d", (PRInt32) f);
  if (!ptr)
    return NS_ERROR_OUT_OF_MEMORY;
  nsAutoString value;
  CopyASCIItoUCS2(nsDependentCString(ptr), value);
  PR_smprintf_free(ptr);
  if (aName.IsEmpty()) {
    return EncodeSimpleValue(value,
			     *nsSOAPUtils::kSOAPEncURI[mSOAPVersion],
			     kShortSchemaType, aDestination, aReturnValue);
  }
  return EncodeSimpleValue(value,
			   aNamespaceURI, aName, aDestination,
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
  nsresult rc;
  PRUint8 f;
  rc = aSource->GetAsInt8(&f);	//  Get as a long number.
  if (NS_FAILED(rc))
    return rc;
  char *ptr = PR_smprintf("%d", (PRInt32) (signed char) f);
  if (!ptr)
    return NS_ERROR_OUT_OF_MEMORY;
  nsAutoString value;
  CopyASCIItoUCS2(nsDependentCString(ptr), value);
  PR_smprintf_free(ptr);
  if (aName.IsEmpty()) {
    return EncodeSimpleValue(value,
			     *nsSOAPUtils::kSOAPEncURI[mSOAPVersion],
			     kByteSchemaType, aDestination, aReturnValue);
  }
  return EncodeSimpleValue(value,
			   aNamespaceURI, aName, aDestination,
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
  nsresult rc;
  PRUint64 f;
  rc = aSource->GetAsUint64(&f);	//  Get as a long number.
  if (NS_FAILED(rc))
    return rc;
  char *ptr = PR_smprintf("%llu", f);
  if (!ptr)
    return NS_ERROR_OUT_OF_MEMORY;
  nsAutoString value;
  CopyASCIItoUCS2(nsDependentCString(ptr), value);
  PR_smprintf_free(ptr);
  if (aName.IsEmpty()) {
    return EncodeSimpleValue(value,
			     *nsSOAPUtils::kSOAPEncURI[mSOAPVersion],
			     kLongSchemaType, aDestination, aReturnValue);
  }
  return EncodeSimpleValue(value,
			   aNamespaceURI, aName, aDestination,
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
  nsresult rc;
  PRUint32 f;
  rc = aSource->GetAsUint32(&f);	//  Get as a long number.
  if (NS_FAILED(rc))
    return rc;
  char *ptr = PR_smprintf("%u", f);
  if (!ptr)
    return NS_ERROR_OUT_OF_MEMORY;
  nsAutoString value;
  CopyASCIItoUCS2(nsDependentCString(ptr), value);
  PR_smprintf_free(ptr);
  if (aName.IsEmpty()) {
    return EncodeSimpleValue(value,
			     *nsSOAPUtils::kSOAPEncURI[mSOAPVersion],
			     kIntSchemaType, aDestination, aReturnValue);
  }
  return EncodeSimpleValue(value,
			   aNamespaceURI, aName, aDestination,
			   aReturnValue);
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
  nsresult rc;
  PRUint16 f;
  rc = aSource->GetAsUint16(&f);	//  Get as a long number.
  if (NS_FAILED(rc))
    return rc;
  char *ptr = PR_smprintf("%u", (PRUint32) f);
  if (!ptr)
    return NS_ERROR_OUT_OF_MEMORY;
  nsAutoString value;
  CopyASCIItoUCS2(nsDependentCString(ptr), value);
  PR_smprintf_free(ptr);
  if (aName.IsEmpty()) {
    return EncodeSimpleValue(value,
			     *nsSOAPUtils::kSOAPEncURI[mSOAPVersion],
			     kShortSchemaType, aDestination, aReturnValue);
  }
  return EncodeSimpleValue(value,
			   aNamespaceURI, aName, aDestination,
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
  nsresult rc;
  PRUint8 f;
  rc = aSource->GetAsUint8(&f);	//  Get as a long number.
  if (NS_FAILED(rc))
    return rc;
  char *ptr = PR_smprintf("%u", (PRUint32) f);
  if (!ptr)
    return NS_ERROR_OUT_OF_MEMORY;
  nsAutoString value;
  CopyASCIItoUCS2(nsDependentCString(ptr), value);
  PR_smprintf_free(ptr);
  if (aName.IsEmpty()) {
    return EncodeSimpleValue(value,
			     *nsSOAPUtils::kSOAPEncURI[mSOAPVersion],
			     kByteSchemaType, aDestination, aReturnValue);
  }
  return EncodeSimpleValue(value,
			   aNamespaceURI, aName, aDestination,
			   aReturnValue);
}

NS_IMETHODIMP
    nsDefaultEncoder::Decode(nsISOAPEncoding * aEncoding,
			     nsIDOMElement * aSource,
			     nsISchemaType * aSchemaType,
			     nsISOAPAttachments * aAttachments,
			     nsIVariant ** _retval)
{
  nsCOMPtr < nsISOAPDecoder > decoder;
  nsCOMPtr < nsISOAPEncoding > encoding = aEncoding;	//  First, handle encoding redesignation, if any

  {
    nsCOMPtr < nsIDOMAttr > enc;
    nsresult rv =
	aSource->
	GetAttributeNodeNS(*nsSOAPUtils::kSOAPEnvURI[mSOAPVersion],
			   nsSOAPUtils::kEncodingStyleAttribute,
			   getter_AddRefs(enc));
    if (NS_FAILED(rv))
      return rv;
    if (enc) {
      nsAutoString style;
      encoding->GetStyleURI(style);
      if (NS_FAILED(rv))
	return rv;
      nsCOMPtr < nsISOAPEncoding > newencoding;
      encoding->GetAssociatedEncoding(style, PR_FALSE,
				      getter_AddRefs(newencoding));
      if (NS_FAILED(rv))
	return rv;
      if (newencoding) {
	encoding = newencoding;
      }
    }
  }

  nsCOMPtr < nsISchemaType > type = aSchemaType;
  if (!type) {			//  Where no type has been specified, look one up from schema attribute, if it exists
    nsCOMPtr < nsISchemaCollection > collection;
    nsresult rc =
	aEncoding->GetSchemaCollection(getter_AddRefs(collection));
    if (NS_FAILED(rc))
      return rc;
    nsAutoString explicittype;
    rc = aSource->GetAttributeNS(*nsSOAPUtils::kXSIURI[mSchemaVersion],
				 nsSOAPUtils::kXSITypeAttribute,
				 explicittype);
    if (NS_FAILED(rc))
      return rc;
    nsAutoString ns;
    nsAutoString name;
    if (!explicittype.IsEmpty()) {
      rc = nsSOAPUtils::GetNamespaceURI(aSource, explicittype, ns);
      if (NS_FAILED(rc))
	return rc;
      rc = nsSOAPUtils::GetLocalName(explicittype, name);
      if (NS_FAILED(rc))
	return rc;
      rc = collection->GetType(name, ns, getter_AddRefs(type));
//      if (NS_FAILED(rc)) return rc;
    }
    if (!type) {
      rc = aSource->GetNamespaceURI(ns);
      if (NS_FAILED(rc))
	return rc;
      rc = aSource->GetLocalName(name);
      if (NS_FAILED(rc))
	return rc;
      nsCOMPtr < nsISchemaElement > element;
      rc = collection->GetElement(name, ns, getter_AddRefs(element));
//      if (NS_FAILED(rc)) return rc;
      if (element) {
	rc = element->GetType(getter_AddRefs(type));
	if (NS_FAILED(rc))
	  return rc;
      } else if (ns.Equals(*nsSOAPUtils::kSOAPEncURI[mSOAPVersion])) {	//  Last-ditch hack to get undeclared types from SOAP namespace
	if (name.Equals(kArraySOAPType)) {	//  This should not be needed if schema has these declarations
	  rc = collection->GetType(name, ns, getter_AddRefs(type));
	} else {
	  rc = collection->GetType(name,
				   *nsSOAPUtils::
				   kXSURI[mSchemaVersion],
				   getter_AddRefs(type));
	}
//        if (NS_FAILED(rc)) return rc;
      }
    }
  }
  if (type) {
    nsCOMPtr < nsISchemaType > lookupType = type;
    do {
      nsAutoString schemaType;
      nsAutoString schemaURI;
      nsresult rc = lookupType->GetName(schemaType);
      if (NS_FAILED(rc))
	return rc;
      rc = lookupType->GetTargetNamespace(schemaURI);
      if (NS_FAILED(rc))
	return rc;
      PRUint16 typevalue;
      rc = lookupType->GetSchemaType(&typevalue);
      if (NS_FAILED(rc))
	return rc;
      // Special case builtin types so that the namespace for the
      // type is the version that corresponds to our SOAP version.
      if (typevalue == nsISchemaType::SCHEMA_TYPE_SIMPLE) {
	nsCOMPtr < nsISchemaSimpleType > simpleType =
	    do_QueryInterface(lookupType);
	PRUint16 simpleTypeValue;
	rc = simpleType->GetSimpleType(&simpleTypeValue);
	if (NS_FAILED(rc))
	  return rc;
	if (simpleTypeValue == nsISchemaSimpleType::SIMPLE_TYPE_BUILTIN) {
	  schemaURI.Assign(*nsSOAPUtils::kXSURI[mSchemaVersion]);
	}
      }
      nsAutoString encodingKey;
      SOAPEncodingKey(schemaURI, schemaType, encodingKey);
      rc = aEncoding->GetDecoder(encodingKey, getter_AddRefs(decoder));
      if (NS_FAILED(rc))
	return rc;
      if (decoder)
	break;
      if (typevalue == nsISchemaType::SCHEMA_TYPE_COMPLEX) {
	nsCOMPtr < nsISchemaComplexType > oldtype =
	    do_QueryInterface(lookupType);
	oldtype->GetBaseType(getter_AddRefs(lookupType));
      } else {
	break;
      }
    }
    while (lookupType);
  }
  if (!decoder) {
    PRUint16 typevalue;
    if (type) {
      nsresult rc = type->GetSchemaType(&typevalue);
      if (NS_FAILED(rc))
	return rc;
    } else {
      typevalue = nsISchemaType::SCHEMA_TYPE_COMPLEX;
    }
    nsAutoString encodingKey;
    if (typevalue == nsISchemaType::SCHEMA_TYPE_COMPLEX) {
      SOAPEncodingKey(*nsSOAPUtils::kXSURI[mSchemaVersion],
		      kAnyTypeSchemaType, encodingKey);
    } else {
      SOAPEncodingKey(*nsSOAPUtils::kXSURI[mSchemaVersion],
		      kAnySimpleTypeSchemaType, encodingKey);
    }
    nsresult rc =
	aEncoding->GetDecoder(encodingKey, getter_AddRefs(decoder));
    if (NS_FAILED(rc))
      return rc;
  }
  if (decoder) {
    return decoder->Decode(aEncoding, aSource, type, aAttachments,
			   _retval);
  }
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
    nsAnyTypeEncoder::Decode(nsISOAPEncoding * aEncoding,
			     nsIDOMElement * aSource,
			     nsISchemaType * aSchemaType,
			     nsISOAPAttachments * aAttachments,
			     nsIVariant ** _retval)
{
  PRBool mustBeSimple = PR_FALSE;
  PRBool mustBeComplex = PR_FALSE;
  if (aSchemaType) {
    PRUint16 typevalue;
    nsresult rc = aSchemaType->GetSchemaType(&typevalue);
    if (NS_FAILED(rc))
      return rc;
    if (typevalue == nsISchemaType::SCHEMA_TYPE_COMPLEX) {
      mustBeComplex = PR_TRUE;
    } else {
      mustBeSimple = PR_TRUE;
    }
  }
  if (!mustBeComplex) {
    nsAutoString value;
    nsresult rc = nsSOAPUtils::GetElementTextContent(aSource, value);
    if (rc == NS_ERROR_ILLEGAL_VALUE && !mustBeSimple) {
      mustBeComplex = PR_TRUE;
    } else if (NS_FAILED(rc))
      return rc;
    else {
//  Here we have a simple value which has no decoder.  Make it a string.
      nsCOMPtr < nsIWritableVariant > p =
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
  }

//  Here we have a complex value, so produce a property bag.  Implement type-driven
//  decoding later.

  nsresult rc;
  nsCOMPtr<nsISOAPPropertyBagMutator> c = do_CreateInstance(NS_SOAPPROPERTYBAGMUTATOR_CONTRACTID, &rc);
  if (NS_FAILED(rc))
    return rc;
  nsCOMPtr<nsIDOMElement> child;
  nsSOAPUtils::GetFirstChildElement(aSource, getter_AddRefs(child));
  while (child != nsnull) {
    nsAutoString name;
    nsAutoString namespaceURI;
    nsCOMPtr<nsIVariant>value;
    rc = child->GetLocalName(name);
    if (NS_FAILED(rc))
      return rc;
    rc = child->GetNamespaceURI(namespaceURI);
    if (NS_FAILED(rc))
      return rc;
    if (!namespaceURI.IsEmpty()) {
      return NS_ERROR_ILLEGAL_VALUE;  //  We only know how to put local values into structures.
    }
    rc = aEncoding->Decode(child, nsnull, aAttachments, getter_AddRefs(value));
    if (NS_FAILED(rc))
      return rc;
    rc = c->AddProperty(name, value);
    if (NS_FAILED(rc))
      return rc;
  }
  nsCOMPtr<nsIPropertyBag> pb;
  c->GetPropertyBag(getter_AddRefs(pb));
  if (NS_FAILED(rc))
    return rc;
  nsCOMPtr<nsIWritableVariant > p =
      do_CreateInstance(NS_VARIANT_CONTRACTID, &rc);
  if (NS_FAILED(rc))
    return rc;
  p->SetAsInterface(NS_GET_IID(nsIPropertyBag), pb);
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
  nsAutoString value;
  nsresult rc = nsSOAPUtils::GetElementTextContent(aSource, value);
  if (NS_FAILED(rc))
    return rc;
  nsCOMPtr < nsIWritableVariant > p =
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

#define MAX_ARRAY_DIMENSIONS 100
/**
 * Extract multiple bracketted numbers from the end of
 * the string and return the string with the number
 * removed or return the original string and -1.  Either
 * the number of dimensions or the size of any particular
 * dimension can be returned as -1.  An over-all -1
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
static PRInt32 GetArrayDimensions(const nsAString& src, PRInt32* dim, nsAString & dst)
{
  dst.Assign(src);
  nsReadingIterator < PRUnichar > i1;
  nsReadingIterator < PRUnichar > i2;
  src.BeginReading(i1);
  src.EndReading(i2);
  if (src.IsEmpty()) return -1;
  while (i1 != i2      //  Loop past white space
    && *(--i2) <= ' ')
    ;
  if (*i2 != ']') {                  //  In this case, not an array dimension
    int len = Distance(i1, i2) - 1;  //  This is the size to truncate to at the end.
    src.Left(dst, len);              //  Truncate the string.
    return -1;                       //  Eliminated white space.
  }

  int d = 1;    //  Counting the dimensions
  for (;;) {	//  First look for the matching bracket from reverse and commas.
    if (i1 == i2) {                  //  No matching bracket.
      return -1;
    }
    PRUnichar c = *(--i2);
    if (c == '[') {                  //  Matching bracket found!
      break;
    }
    if (c == ',') {
      d++;
    }
  }
  int len;
  {
    nsReadingIterator < PRUnichar > i3 = i2++;  //  Cover any extra white space
    while (i1 != i2      //  Loop past white space
      && *(--i2) <= ' ')
      ;
    len = Distance(i1, i3);        //  Length remaining in string after operation
  }

  if (d > MAX_ARRAY_DIMENSIONS) {  //  Completely ignore it if too many dimensions.
    return -1;
  }

  i1 = i2;
  src.EndReading(i2);
  while (*(--i2) != ']')           //  Find end bracket again
    ;

  d = 0;                           //  Start with first dimension.
  dim[d] = -1;
  PRBool finished = PR_FALSE;      //  Disallow space within numbers

  while (i1 != i2) {
    PRUnichar c = *(i1++);
    if (c < '0' || c > '9') {
//  There may be slightly more to do here if alternative radixes are supported.
      if (c <= ' ') {              //  Accept anything < space as whitespace
        if (dim[d] >= 0) {
	  finished = PR_TRUE;
        }
      }
      else if (c == ',') {         //  Introducing new dimension
	dim[++d] = -1;             //  Restarting it at -1
	finished = PR_FALSE;
      }
      else
        return -1;                 //  Unrecognized character
    } else {
      if (finished) {
        return -1;                 //  Numbers not allowed after white space
      }
      if (dim[d] == -1)
	dim[d] = 0;
      if (dim[d] < 214748364) {
        dim[d] = dim[d] * 10 + c - '0';
      }
      else {
	return -1;                 //  Number got too big.
      }
    }
  }
  src.Left(dst, len);              //  Truncate the string.
  return d + 1;                    //  Return the number of dimensions
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
static PRInt32 GetArrayPosition(const nsAString& src, PRInt32 d, PRInt32* dim)
{
  PRInt32 pos[MAX_ARRAY_DIMENSIONS];
  nsAutoString leftover;
  PRInt32 result = GetArrayDimensions(src, pos, leftover);
  if (result != d                //  Easy cases where something went wrong
    || !leftover.IsEmpty())
    return -1;
  result = 0;
  for (PRInt32 i = 0;;) {
    PRInt32 next = pos[i];
    if (next == -1
      || next >= dim[i])
      return -1;
    result = result + next;
    if (++i < d)                 //  Multiply for next round.
      result = result * dim[i];
    else
      return result;
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
  nsAutoString ns;
  nsAutoString name;
  nsCOMPtr < nsISchemaType > subtype;
  nsAutoString value;
  nsresult rc =
      aSource->GetAttributeNS(*nsSOAPUtils::kSOAPEncURI[mSOAPVersion],
			      kSOAPArrayTypeAttribute, value);
  if (NS_FAILED(rc))
    return rc;
  PRInt32 d;                  //  Number of dimensions
  PRInt32 dim[MAX_ARRAY_DIMENSIONS];
  PRInt32 size = -1;
  if (!value.IsEmpty()) {
    nsAutoString dst;
    d = GetArrayDimensions(value, dim, dst);
    value.Assign(dst);

    if (d > 0) {
      PRInt64 tot = 1;  //  Collect in 64 bits, just to make sure it fits
      for (int i = 0; i < d; i++) {
	PRInt32 next = dim[i];
	if (next == -1) {
          tot = -1;
	  break;
	}
	tot = tot * next;
	if (tot > 2147483647) {
          return NS_ERROR_FAILURE;
	}
      }
      size = (PRInt32)tot;
    }

    nsCOMPtr < nsISchemaCollection > collection;
    rc = aEncoding->GetSchemaCollection(getter_AddRefs(collection));
    if (NS_FAILED(rc))
      return rc;
    if (value.Last() ==']') {
      ns.Assign(*nsSOAPUtils::kSOAPEncURI[mSOAPVersion]);
      name.Assign(kArraySOAPType);
    }
    else {
      rc = nsSOAPUtils::GetNamespaceURI(aSource, value, ns);
      if (NS_FAILED(rc))
        return rc;
      rc = nsSOAPUtils::GetLocalName(value, name);
      if (NS_FAILED(rc))
        return rc;
    }
    rc = collection->GetType(name, ns, getter_AddRefs(subtype));
//      if (NS_FAILED(rc)) return rc;
  }
  rc =
      aSource->GetAttributeNS(*nsSOAPUtils::kSOAPEncURI[mSOAPVersion],
			      kSOAPArrayOffsetAttribute, value);
  if (NS_FAILED(rc))
    return rc;
  PRInt32 offset;           //  Computing offset ismuch trickier, because size may be unspecified.
  if (!value.IsEmpty()) {  //  See if the array has an initial position.
    PRInt32 pos[MAX_ARRAY_DIMENSIONS];
    nsAutoString leftover;
    offset = GetArrayDimensions(value, pos, leftover);
    if (d == -1)
      d = offset;
    if (offset == -1        //  We have to understand this or report an error
        || offset != d      //  But the offset does not need to be understood
        || !leftover.IsEmpty())
      return NS_ERROR_ILLEGAL_VALUE;
    PRInt32 old0 = dim[0];
    if (dim[0] == -1) {    //  It is OK to have a offset where dimension 0 is unspecified
       dim[0] = 2147483647;
    }
    offset  = 0;
    for (PRInt32 i = 0;;) {
      PRInt64 next = pos[i];
      if (next == -1
        || next >= dim[i])
        return NS_ERROR_ILLEGAL_VALUE;
      next = (offset + next);
      if (next > 2147483647)
        return NS_ERROR_ILLEGAL_VALUE;
      offset = (PRInt32)next;
      if (++i < d) {
	next = offset * dim[i];
	if (next > 2147483647)
          return NS_ERROR_ILLEGAL_VALUE;
        offset = (PRInt32)next;
      }
      else {
	break;
      }
    }
    dim[0] = old0;
  }
  else {
    offset = 0;
  }
  if (size == -1) {  //  If no known size, we have to go through and pre-count.
    nsCOMPtr<nsIDOMElement> child;
    nsSOAPUtils::GetFirstChildElement(aSource, getter_AddRefs(child));
    PRInt32 pp[MAX_ARRAY_DIMENSIONS];
    if (d != -1) {
      for (int i = d; i-- != 0;) {
	pp[i] = 0;
      }
    }
    size = 0;
    PRInt32 next = offset;
    while (child) {
      nsAutoString pos;
      nsresult rc =
        child->GetAttributeNS(*nsSOAPUtils::kSOAPEncURI[mSOAPVersion],
			      kSOAPArrayPositionAttribute, pos);
      if (NS_FAILED(rc))
        return rc;
      if (!pos.IsEmpty()) {  //  See if the item in the array has a position
        nsAutoString leftover;
        PRInt32 inc[MAX_ARRAY_DIMENSIONS];
        PRInt32 i = GetArrayDimensions(value, inc, leftover);
        if (i == -1        //  We have to understand this or report an error
            || !leftover.IsEmpty()
	    || (d != -1
               && d != i))
          return NS_ERROR_ILLEGAL_VALUE;
	if (d == -1) {
	  d = i;             //  If we never had dimension count before, we do now.
          for (int i = d; i-- != 0;) {
	    pp[i] = 0;
          }
	}
	for (i = 0; i < d; i++) {
          PRInt32 n = inc[i];
	  if (n == -1) {  //  Positions must be concrete
            return NS_ERROR_ILLEGAL_VALUE;
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
    if (d == -1) {         //  If unknown or 1 dimension, unpositioned entries can help
      d = 1;
      pp[0] = next;
    }
    else if (d == 1
      && next > pp[0]) {
      pp[0] = next;
    }
    PRInt64 tot = 1;  //  Collect in 64 bits, just to make sure it fits
    for (int i = 0; i < d; i++) {
      PRInt32 next = dim[i];
      if (next == -1) {          //  Only derive those with no other declaration
        dim[i] = next = pp[i];
      }
      tot = tot * next;
      if (tot > 2147483647) {
        return NS_ERROR_FAILURE;
      }
    }
    size = (PRInt32)tot;  //  At last, we know the dimensions of the array.
  }

//  After considerable work, we may have a schema type and a size.
  
  nsCOMPtr<nsIWritableVariant> result = do_CreateInstance(NS_VARIANT_CONTRACTID, &rc);

#define DECODE_SIMPLE_ARRAY(XPType, VType, VTYPE, Test, Ref) \
      XPType* a = new XPType[size];\
      nsCOMPtr<nsIDOMElement> child;\
      nsSOAPUtils::GetFirstChildElement(aSource, getter_AddRefs(child));\
      PRUint32 next = offset;\
      while (child) {\
        nsAutoString pos;\
        rc =\
          child->GetAttributeNS(*nsSOAPUtils::kSOAPEncURI[mSOAPVersion],\
			        kSOAPArrayPositionAttribute, pos);\
        if (NS_FAILED(rc))\
          break;\
        PRInt32 p;\
        if (!pos.IsEmpty()) {\
          PRInt32 p = GetArrayPosition(value, d, dim);\
          if (p == -1) {\
            rc = NS_ERROR_ILLEGAL_VALUE;\
	    break;\
	  }\
        }\
        else {\
	  p = next++;\
        }\
	if (p >= size\
	   || Test) {\
          rc = NS_ERROR_ILLEGAL_VALUE;\
	  break;\
	}\
	nsCOMPtr<nsIVariant> v;\
 \
        rc = aEncoding->Decode(child, subtype, aAttachments, getter_AddRefs(v));\
        if (NS_FAILED(rc))\
          break;\
\
	rc = v->GetAs##VType(Ref(a + p));\
        if (NS_FAILED(rc))\
          break;\
	\
        nsCOMPtr<nsIDOMElement> next;\
        nsSOAPUtils::GetNextSiblingElement(child, getter_AddRefs(next));\
        child = next;\
      }\
      if (!NS_FAILED(rc)) {\
        rc = result->SetAsArray(nsIDataType::VTYPE_##VTYPE,nsnull,size,a);\
      }\
      delete[] a;\
      if (NS_FAILED(rc))\
	return rc;

  if (NS_FAILED(rc))
    return rc;
  if (ns.Equals(*nsSOAPUtils::kXSURI[mSchemaVersion])) {
    if (name.Equals(kStringSchemaType)) {
      DECODE_SIMPLE_ARRAY(nsString,AString,ASTRING,!a[p].IsEmpty(),*);
    } else if (name.Equals(kBooleanSchemaType)) {
      DECODE_SIMPLE_ARRAY(PRBool,Bool,BOOL,a[p],);
    } else if (name.Equals(kFloatSchemaType)) {
      DECODE_SIMPLE_ARRAY(float,Float,FLOAT,a[p],);
    } else if (name.Equals(kDoubleSchemaType)) {
      DECODE_SIMPLE_ARRAY(double,Double,DOUBLE,a[p],);
    } else if (name.Equals(kLongSchemaType)) {
      DECODE_SIMPLE_ARRAY(PRInt64,Int64,INT64,a[p],);
    } else if (name.Equals(kIntSchemaType)) {
      DECODE_SIMPLE_ARRAY(PRInt32,Int32,INT32,a[p],);
    } else if (name.Equals(kShortSchemaType)) {
      DECODE_SIMPLE_ARRAY(PRInt16,Int16,INT16,a[p],);
    } else if (name.Equals(kByteSchemaType)) {
      DECODE_SIMPLE_ARRAY(PRUint8,Int8,INT8,a[p],);
    } else if (name.Equals(kUnsignedLongSchemaType)) {
      DECODE_SIMPLE_ARRAY(PRUint64,Uint64,UINT64,a[p],);
    } else if (name.Equals(kUnsignedIntSchemaType)) {
      DECODE_SIMPLE_ARRAY(PRUint32,Uint32,UINT32,a[p],);
    } else if (name.Equals(kUnsignedShortSchemaType)) {
      DECODE_SIMPLE_ARRAY(PRUint16,Uint16,UINT16,a[p],);
    } else if (name.Equals(kUnsignedByteSchemaType)) {
      DECODE_SIMPLE_ARRAY(PRUint8,Uint8,UINT8,a[p],);
    } else
      return NS_ERROR_ILLEGAL_VALUE;
  } else if (ns.Equals(*nsSOAPUtils::kSOAPEncURI[mSOAPVersion])) {
    if (name.Equals(kArraySOAPType)) {
      return NS_ERROR_ILLEGAL_VALUE;	//  Fix nested arrays later
    } else if (name.Equals(kStructSOAPType)) {
      return NS_ERROR_ILLEGAL_VALUE;	//  Fix nested structs later
    } else
      return NS_ERROR_ILLEGAL_VALUE;
  } else
    return NS_ERROR_ILLEGAL_VALUE;

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
  nsAutoString value;
  nsresult rc = nsSOAPUtils::GetElementTextContent(aSource, value);
  if (NS_FAILED(rc))
    return rc;
  nsCOMPtr < nsIWritableVariant > p =
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
  nsAutoString value;
  nsresult rc = nsSOAPUtils::GetElementTextContent(aSource, value);
  if (NS_FAILED(rc))
    return rc;
  bool b;
  if (value.Equals(nsSOAPUtils::kTrue)
      || value.Equals(nsSOAPUtils::kTrueA)) {
    b = PR_TRUE;
  } else if (value.Equals(nsSOAPUtils::kFalse)
	     || value.Equals(nsSOAPUtils::kFalseA)) {
    b = PR_FALSE;
  } else
    return NS_ERROR_ILLEGAL_VALUE;

  nsCOMPtr < nsIWritableVariant > p =
      do_CreateInstance(NS_VARIANT_CONTRACTID);
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
  nsAutoString value;
  nsresult rc = nsSOAPUtils::GetElementTextContent(aSource, value);
  if (NS_FAILED(rc))
    return rc;
  double f;
  unsigned int n;
  int r = PR_sscanf(NS_ConvertUCS2toUTF8(value).get(), " %lf %n", &f, &n);
  if (r == 0 || n < value.Length())
    return NS_ERROR_ILLEGAL_VALUE;

  nsCOMPtr < nsIWritableVariant > p =
      do_CreateInstance(NS_VARIANT_CONTRACTID);
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
  nsAutoString value;
  nsresult rc = nsSOAPUtils::GetElementTextContent(aSource, value);
  if (NS_FAILED(rc))
    return rc;
  float f;
  unsigned int n;
  int r = PR_sscanf(NS_ConvertUCS2toUTF8(value).get(), " %f %n", &f, &n);
  if (r == 0 || n < value.Length())
    return NS_ERROR_ILLEGAL_VALUE;

  nsCOMPtr < nsIWritableVariant > p =
      do_CreateInstance(NS_VARIANT_CONTRACTID);
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
  nsAutoString value;
  nsresult rc = nsSOAPUtils::GetElementTextContent(aSource, value);
  if (NS_FAILED(rc))
    return rc;
  PRInt64 f;
  unsigned int n;
  int r = PR_sscanf(NS_ConvertUCS2toUTF8(value).get(), " %lld %n", &f, &n);
  if (r == 0 || n < value.Length())
    return NS_ERROR_ILLEGAL_VALUE;

  nsCOMPtr < nsIWritableVariant > p =
      do_CreateInstance(NS_VARIANT_CONTRACTID);
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
  nsAutoString value;
  nsresult rc = nsSOAPUtils::GetElementTextContent(aSource, value);
  if (NS_FAILED(rc))
    return rc;
  PRInt32 f;
  unsigned int n;
  int r = PR_sscanf(NS_ConvertUCS2toUTF8(value).get(), " %ld %n", &f, &n);
  if (r == 0 || n < value.Length())
    return NS_ERROR_ILLEGAL_VALUE;

  nsCOMPtr < nsIWritableVariant > p =
      do_CreateInstance(NS_VARIANT_CONTRACTID);
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
  nsAutoString value;
  nsresult rc = nsSOAPUtils::GetElementTextContent(aSource, value);
  if (NS_FAILED(rc))
    return rc;
  PRInt16 f;
  unsigned int n;
  int r = PR_sscanf(NS_ConvertUCS2toUTF8(value).get(), " %hd %n", &f, &n);
  if (r == 0 || n < value.Length())
    return NS_ERROR_ILLEGAL_VALUE;

  nsCOMPtr < nsIWritableVariant > p =
      do_CreateInstance(NS_VARIANT_CONTRACTID);
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
  nsAutoString value;
  nsresult rc = nsSOAPUtils::GetElementTextContent(aSource, value);
  if (NS_FAILED(rc))
    return rc;
  PRInt16 f;
  unsigned int n;
  int r = PR_sscanf(NS_ConvertUCS2toUTF8(value).get(), " %hd %n", &f, &n);
  if (r == 0 || n < value.Length() || f < -128 || f > 127)
    return NS_ERROR_ILLEGAL_VALUE;

  nsCOMPtr < nsIWritableVariant > p =
      do_CreateInstance(NS_VARIANT_CONTRACTID);
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
  nsAutoString value;
  nsresult rc = nsSOAPUtils::GetElementTextContent(aSource, value);
  if (NS_FAILED(rc))
    return rc;
  PRUint64 f;
  unsigned int n;
  int r = PR_sscanf(NS_ConvertUCS2toUTF8(value).get(), " %llu %n", &f, &n);
  if (r == 0 || n < value.Length())
    return NS_ERROR_ILLEGAL_VALUE;

  nsCOMPtr < nsIWritableVariant > p =
      do_CreateInstance(NS_VARIANT_CONTRACTID);
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
  nsAutoString value;
  nsresult rc = nsSOAPUtils::GetElementTextContent(aSource, value);
  if (NS_FAILED(rc))
    return rc;
  PRUint32 f;
  unsigned int n;
  int r = PR_sscanf(NS_ConvertUCS2toUTF8(value).get(), " %lu %n", &f, &n);
  if (r == 0 || n < value.Length())
    return NS_ERROR_ILLEGAL_VALUE;

  nsCOMPtr < nsIWritableVariant > p =
      do_CreateInstance(NS_VARIANT_CONTRACTID);
  p->SetAsUint32(f);
  *_retval = p;
  NS_ADDREF(*_retval);
  return NS_OK;
}

NS_IMETHODIMP
    nsUnsignedShortEncoder::Decode(nsISOAPEncoding * aEncoding,
				   nsIDOMElement * aSource,
				   nsISchemaType * aSchemaType,
				   nsISOAPAttachments * aAttachments,
				   nsIVariant ** _retval)
{
  nsAutoString value;
  nsresult rc = nsSOAPUtils::GetElementTextContent(aSource, value);
  if (NS_FAILED(rc))
    return rc;
  PRUint16 f;
  unsigned int n;
  int r = PR_sscanf(NS_ConvertUCS2toUTF8(value).get(), " %hu %n", &f, &n);
  if (r == 0 || n < value.Length())
    return NS_ERROR_ILLEGAL_VALUE;

  nsCOMPtr < nsIWritableVariant > p =
      do_CreateInstance(NS_VARIANT_CONTRACTID);
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
  nsAutoString value;
  nsresult rc = nsSOAPUtils::GetElementTextContent(aSource, value);
  if (NS_FAILED(rc))
    return rc;
  PRUint16 f;
  unsigned int n;
  int r = PR_sscanf(NS_ConvertUCS2toUTF8(value).get(), " %hu %n", &f, &n);
  if (r == 0 || n < value.Length() || f > 255)
    return NS_ERROR_ILLEGAL_VALUE;

  nsCOMPtr < nsIWritableVariant > p =
      do_CreateInstance(NS_VARIANT_CONTRACTID);
  p->SetAsUint8((PRUint8) f);
  *_retval = p;
  NS_ADDREF(*_retval);
  return NS_OK;
}
