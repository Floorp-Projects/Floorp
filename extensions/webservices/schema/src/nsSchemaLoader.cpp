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
#include "nsSchemaLoader.h"
#include "nsIWebServiceErrorHandler.h"

// content includes
#include "nsIContent.h"
#include "nsINodeInfo.h"
#include "nsIDOMDocument.h"
#include "nsIDOM3Node.h"

// loading includes
#include "nsIXMLHttpRequest.h"
#include "nsIDOMEvent.h"
#include "nsIDOMEventListener.h"
#include "nsIDOMEventTarget.h"
#include "nsNetUtil.h"

// string includes
#include "nsReadableUtils.h"

// XPConnect includes
#include "nsIXPConnect.h"
#include "nsIScriptSecurityManager.h"
#include "nsIPrincipal.h"

// XPCOM includes
#include "nsIServiceManager.h"
#include "nsIComponentManager.h"
#include "nsStaticAtom.h"

////////////////////////////////////////////////////////////
//
// nsSchemaAtoms implementation
//
////////////////////////////////////////////////////////////

// define storage for all atoms
#define SCHEMA_ATOM(_name, _value) nsIAtom* nsSchemaAtoms::_name;
#include "nsSchemaAtomList.h"
#undef SCHEMA_ATOM

static const nsStaticAtom atomInfo[] = {
#define SCHEMA_ATOM(_name, _value) { _value, &nsSchemaAtoms::_name },
#include "nsSchemaAtomList.h"
#undef SCHEMA_ATOM
};

nsresult
nsSchemaAtoms::AddRefAtoms()
{
  return NS_RegisterStaticAtoms(atomInfo, NS_ARRAY_LENGTH(atomInfo));
}

////////////////////////////////////////////////////////////
//
// LoadListener implementation
//
////////////////////////////////////////////////////////////

class LoadListener : public nsIDOMEventListener {
public:
  LoadListener(nsSchemaLoader* aLoader,
               nsISchemaLoadListener* aListener,
               nsIXMLHttpRequest* aRequest);
  virtual ~LoadListener();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMEVENTLISTENER

private:
  nsSchemaLoader* mLoader;
  nsCOMPtr<nsISchemaLoadListener> mListener;
  nsCOMPtr<nsIXMLHttpRequest> mRequest;
  nsString mURI;
};

LoadListener::LoadListener(nsSchemaLoader* aLoader,
                           nsISchemaLoadListener* aListener,
                           nsIXMLHttpRequest* aRequest) 
{
  mLoader = aLoader;
  NS_ADDREF(mLoader);
  mListener = aListener;
  mRequest = aRequest;
}
  
LoadListener::~LoadListener() 
{
  NS_IF_RELEASE(mLoader);
}

NS_IMPL_ISUPPORTS1(LoadListener, nsIDOMEventListener)

/* void handleEvent (in nsIDOMEvent event); */
NS_IMETHODIMP 
LoadListener::HandleEvent(nsIDOMEvent *event)
{
  nsresult rv;
  nsAutoString eventType;
  
  event->GetType(eventType);
  
  if (eventType.EqualsLiteral("load")) {
    nsCOMPtr<nsIDOMDocument> document;
    nsCOMPtr<nsISchema> schema;
    
    rv = mRequest->GetResponseXML(getter_AddRefs(document));
    if (NS_SUCCEEDED(rv)) {
      nsCOMPtr<nsIDOMElement> element;
      
      if (document)
        document->GetDocumentElement(getter_AddRefs(element));

      if (element) {
        //XXXTelemac TODO Use an nsIWebServiceErrorHandler instead of nsnull
        rv = mLoader->ProcessSchemaElement(nsnull, element, getter_AddRefs(schema));
      }
      else {
        rv = NS_ERROR_SCHEMA_NOT_SCHEMA_ELEMENT;
      }
    }

    if (NS_SUCCEEDED(rv)) {
      mListener->OnLoad(schema);
    }
    //XXXTelemac OnError call replace by use of nsIWebServiceErrorHandler 
    //XXXTelemac in sub-processing methods.
  }
  else if (eventType.EqualsLiteral("error") &&
           mListener) {
    mListener->OnError(NS_ERROR_SCHEMA_LOADING_ERROR, 
                       NS_LITERAL_STRING("Failure loading"));
  }
  NS_IF_RELEASE(mLoader);
  mListener = nsnull;
  mRequest = nsnull;

  return NS_OK;
}

////////////////////////////////////////////////////////////
//
// nsBuiltinSchemaCollection implementation
//
////////////////////////////////////////////////////////////
nsBuiltinSchemaCollection::nsBuiltinSchemaCollection()
{
}

nsBuiltinSchemaCollection::~nsBuiltinSchemaCollection()
{
  mBuiltinTypesHash.Reset();
  mSOAPTypeHash.Reset();
}

NS_IMPL_ISUPPORTS1(nsBuiltinSchemaCollection,
                   nsISchemaCollection)

/* nsISchema getSchema (in AString targetNamespace); */
NS_IMETHODIMP 
nsBuiltinSchemaCollection::GetSchema(const nsAString & targetNamespace,
                                     nsISchema **_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = nsnull;
  return NS_ERROR_SCHEMA_UNKNOWN_TARGET_NAMESPACE;
}

/* nsISchemaElement getElement (in AString name, in AString namespace); */
NS_IMETHODIMP 
nsBuiltinSchemaCollection::GetElement(const nsAString & aName, 
                                      const nsAString & aNamespace, 
                                      nsISchemaElement **_retval)
{
  if (aNamespace.IsEmpty()) {
    NS_WARNING("nsSchemaLoader::GetSchema(nsAString,nsISchema): "
               "Invalid |targetNamespace| is empty");
    
    return NS_ERROR_INVALID_ARG;
  }

  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = nsnull;
  return NS_ERROR_FAILURE;
}

/* nsISchemaAttribute getAttribute (in AString name, in AString namespace); */
NS_IMETHODIMP 
nsBuiltinSchemaCollection::GetAttribute(const nsAString & aName, 
                                        const nsAString & aNamespace, 
                                        nsISchemaAttribute **_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = nsnull;
  return NS_ERROR_FAILURE;
}

static PRBool
IsSchemaNamespace(const nsAString& aNamespace)
{
  if (aNamespace.EqualsLiteral(NS_SCHEMA_2001_NAMESPACE) ||
      aNamespace.EqualsLiteral(NS_SCHEMA_1999_NAMESPACE)) {
    return PR_TRUE;
  }
  else {
    return PR_FALSE;
  }
}

static PRBool
IsSOAPNamespace(const nsAString& aNamespace)
{
  if (aNamespace.EqualsLiteral(NS_SOAP_1_1_ENCODING_NAMESPACE) ||
      aNamespace.EqualsLiteral(NS_SOAP_1_2_ENCODING_NAMESPACE)) {
    return PR_TRUE;
  }
  else {
    return PR_FALSE;
  }  
}

/* nsISchemaType getType (in AString name, in AString namespace); */
NS_IMETHODIMP 
nsBuiltinSchemaCollection::GetType(const nsAString & aName, 
                                   const nsAString & aNamespace, 
                                   nsISchemaType **_retval)
{
  if (IsSchemaNamespace(aNamespace)) {
    return GetBuiltinType(aName, aNamespace, _retval);
  }

  if (IsSOAPNamespace(aNamespace)) {
    return GetSOAPType(aName, aNamespace, _retval);
  }
  
  return NS_ERROR_SCHEMA_UNKNOWN_TYPE;
}

nsresult
nsBuiltinSchemaCollection::GetBuiltinType(const nsAString& aName,
                                          const nsAString& aNamespace,
                                          nsISchemaType** aType)
{
  nsresult rv = NS_OK;
  nsStringKey key(aName);
  nsCOMPtr<nsISupports> sup = dont_AddRef(mBuiltinTypesHash.Get(&key));
  if (sup) {
    rv = CallQueryInterface(sup, aType);
  }
  else {
    nsCOMPtr<nsIAtom> typeName = do_GetAtom(aName);
    PRUint16 typeVal;
    if (typeName == nsSchemaAtoms::sAnyType_atom) {
      typeVal = nsISchemaBuiltinType::BUILTIN_TYPE_ANYTYPE;
    }
    else if (typeName == nsSchemaAtoms::sString_atom) {
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
    else if (typeName == nsSchemaAtoms::sAnyURI_atom) {
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
      return NS_ERROR_SCHEMA_UNKNOWN_TYPE;
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
nsBuiltinSchemaCollection::GetSOAPType(const nsAString& aName,
                                       const nsAString& aNamespace,
                                       nsISchemaType** aType)
{
  nsresult rv = NS_OK;
  nsStringKey key(aName);
  nsCOMPtr<nsISupports> sup = dont_AddRef(mSOAPTypeHash.Get(&key));
  if (sup) {
    rv = CallQueryInterface(sup, aType);
  }
  else {
    if (aName.EqualsLiteral("Array")) {
      nsCOMPtr<nsISchemaType> anyType;
      rv = GetBuiltinType(NS_LITERAL_STRING("anyType"),
                          NS_LITERAL_STRING(NS_SCHEMA_2001_NAMESPACE),
                          getter_AddRefs(anyType));
      if (NS_FAILED(rv)) {
        return rv;
      }

      nsSOAPArray* array = new nsSOAPArray(anyType);
      if (!array) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
      sup = array;
      mSOAPTypeHash.Put(&key, sup);
    
      *aType = array;
      NS_ADDREF(*aType);
    }
    else if (aName.EqualsLiteral("arrayType")) {
      nsSOAPArrayType* arrayType = new nsSOAPArrayType();
      if (!arrayType) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
      sup = arrayType;
      mSOAPTypeHash.Put(&key, sup);
    
      *aType = arrayType;
      NS_ADDREF(*aType);
    }
    else {
      rv = NS_ERROR_SCHEMA_UNKNOWN_TYPE;
    }
  }

  return rv;
}

////////////////////////////////////////////////////////////
//
// nsSchemaLoader implementation
//
////////////////////////////////////////////////////////////

nsSchemaLoader::nsSchemaLoader()
{
  mBuiltinCollection = do_GetService(NS_BUILTINSCHEMACOLLECTION_CONTRACTID);
}

nsSchemaLoader::~nsSchemaLoader()
{
}

NS_IMPL_ISUPPORTS2_CI(nsSchemaLoader, 
                      nsISchemaLoader, 
                      nsISchemaCollection)


/* nsISchema getSchema (in AString targetNamespace); */
NS_IMETHODIMP 
nsSchemaLoader::GetSchema(const nsAString & targetNamespace, 
                          nsISchema **_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  nsStringKey key(targetNamespace);
  nsCOMPtr<nsISupports> sup = dont_AddRef(mSchemas.Get(&key));
  nsCOMPtr<nsISchema> schema(do_QueryInterface(sup));

  if (!schema) {
    return NS_ERROR_SCHEMA_UNKNOWN_TARGET_NAMESPACE;  // no schema for specified targetname
  }

  *_retval = schema;
  NS_ADDREF(*_retval);

  return NS_OK;
}

/* nsISchemaElement getElement (in AString name, in AString namespace); */
NS_IMETHODIMP 
nsSchemaLoader::GetElement(const nsAString & aName, 
                           const nsAString & aNamespace, 
                           nsISchemaElement **_retval)
{
  nsCOMPtr<nsISchema> schema;
  nsresult rv = GetSchema(aNamespace, getter_AddRefs(schema));
  if (NS_FAILED(rv)) {
    return rv;
  }

  return schema->GetElementByName(aName, _retval);
}

/* nsISchemaAttribute getAttribute (in AString name, in AString namespace); */
NS_IMETHODIMP 
nsSchemaLoader::GetAttribute(const nsAString & aName, 
                             const nsAString & aNamespace, 
                             nsISchemaAttribute **_retval)
{
  nsCOMPtr<nsISchema> schema;
  nsresult rv = GetSchema(aNamespace, getter_AddRefs(schema));
  if (NS_FAILED(rv)) {
    return rv;
  }

  return schema->GetAttributeByName(aName, _retval);
}

/* nsISchemaType getType (in AString name, in AString namespace); */
NS_IMETHODIMP 
nsSchemaLoader::GetType(const nsAString & aName, 
                        const nsAString & aNamespace, 
                        nsISchemaType **_retval)
{
  nsresult rv = NS_OK;

  if (IsSchemaNamespace(aNamespace) || IsSOAPNamespace(aNamespace)) {
    rv = mBuiltinCollection->GetType(aName, aNamespace, _retval);

    if (NS_FAILED(rv)) {
      nsAutoString errorMsg(NS_LITERAL_STRING("nsSchemaLoader::GetType: "));
      errorMsg.AppendLiteral("Failure processing schema: cannot get schema type \"");
      errorMsg.Append(aName);
      errorMsg.AppendLiteral("\"");
      NS_ERROR(NS_ConvertUTF16toUTF8(errorMsg).get());

      return rv;
    }

    return NS_OK;
  }

  nsCOMPtr<nsISchema> schema;
  rv = GetSchema(aNamespace, getter_AddRefs(schema));
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = schema->GetTypeByName(aName, _retval);

  if (NS_FAILED(rv)) {
    nsAutoString msg(NS_LITERAL_STRING("nsSchemaLoader::GetType: "));
    msg.AppendLiteral("Failure processing schema: ");
    msg.AppendLiteral("cannot get schema type \"");
    msg.Append(aName);
    msg.AppendLiteral("\"");
    NS_ERROR(NS_ConvertUTF16toUTF8(msg).get());

    return rv;
  }

  return NS_OK;
}

nsresult
nsSchemaLoader::GetResolvedURI(const nsAString& aSchemaURI,
                               const char* aMethod,
                               nsIURI** aURI)
{
  nsresult rv;
  nsCOMPtr<nsIXPCNativeCallContext> cc;
  nsCOMPtr<nsIXPConnect> xpc(do_GetService(nsIXPConnect::GetCID(), &rv));
  if(NS_SUCCEEDED(rv)) {
    rv = xpc->GetCurrentNativeCallContext(getter_AddRefs(cc));
  }

  if (NS_SUCCEEDED(rv) && cc) {
    JSContext* cx;
    rv = cc->GetJSContext(&cx);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIScriptSecurityManager> secMan(do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIURI> baseURI;
    nsCOMPtr<nsIPrincipal> principal;
    rv = secMan->GetSubjectPrincipal(getter_AddRefs(principal));
    if (NS_SUCCEEDED(rv)) {
      principal->GetURI(getter_AddRefs(baseURI));
    }
    
    rv = NS_NewURI(aURI, aSchemaURI, nsnull, baseURI);
    if (NS_FAILED(rv)) return rv;
    
    rv = secMan->CheckLoadURIFromScript(cx, *aURI);
    if (NS_FAILED(rv))
    {
      // Security check failed. The above call set a JS exception. The
      // following lines ensure that the exception is propagated.
      cc->SetExceptionWasThrown(PR_TRUE);
      return rv;
    }
  }
  else {
    rv = NS_NewURI(aURI, aSchemaURI, nsnull);
    if (NS_FAILED(rv)) return rv;
  }

  return NS_OK;
}

/* nsISchema load (in AString schemaURI); */
NS_IMETHODIMP 
nsSchemaLoader::Load(const nsAString& schemaURI, 
                     nsISchema **_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  
  nsCOMPtr<nsIURI> resolvedURI;
  nsresult rv = GetResolvedURI(schemaURI, "load", getter_AddRefs(resolvedURI));
  if (NS_FAILED(rv)) {
    return rv;
  }
  nsCAutoString spec;
  resolvedURI->GetSpec(spec);
  
  nsCOMPtr<nsIXMLHttpRequest> request(do_CreateInstance(NS_XMLHTTPREQUEST_CONTRACTID, &rv));
  if (!request) {
    return rv;
  }

  const nsAString& empty = EmptyString();
  rv = request->OpenRequest(NS_LITERAL_CSTRING("GET"), spec, PR_FALSE, empty,
                            empty);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Force the mimetype of the returned stream to be xml.
  rv = request->OverrideMimeType(NS_LITERAL_CSTRING("text/xml"));
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = request->Send(nsnull);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsIDOMDocument> document;
  rv = request->GetResponseXML(getter_AddRefs(document));
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsIDOMElement> element;
  document->GetDocumentElement(getter_AddRefs(element));
  if (element) {
    rv = ProcessSchemaElement(nsnull, element, _retval); //XXXTelemac TODO Have an error handler there instead or nsnull
  }
  else {
    rv = NS_ERROR_SCHEMA_NOT_SCHEMA_ELEMENT;
  }
  
  return rv;
}

/* void loadAsync (in AString schemaURI, in nsISchemaLoadListener listener); */
NS_IMETHODIMP 
nsSchemaLoader::LoadAsync(const nsAString& schemaURI, 
                          nsISchemaLoadListener *aListener)
{
  NS_ENSURE_ARG(aListener);

  nsCOMPtr<nsIURI> resolvedURI;
  nsresult rv = GetResolvedURI(schemaURI, "loadAsync", getter_AddRefs(resolvedURI));
  if (NS_FAILED(rv)) {
    return rv;
  }
  nsCAutoString spec;
  resolvedURI->GetSpec(spec);

  nsCOMPtr<nsIXMLHttpRequest> request(do_CreateInstance(NS_XMLHTTPREQUEST_CONTRACTID, &rv));
  if (!request) {
    return rv;
  }

  const nsAString& empty = EmptyString();
  rv = request->OpenRequest(NS_LITERAL_CSTRING("GET"), spec, PR_TRUE, empty,
                            empty);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Force the mimetype of the returned stream to be xml.
  rv = request->OverrideMimeType(NS_LITERAL_CSTRING("text/xml"));
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsIDOMEventListener> listener;
  LoadListener* listenerInst = new LoadListener(this, aListener, request);
  if (!listenerInst) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  listener = listenerInst;

  nsCOMPtr<nsIDOMEventTarget> target(do_QueryInterface(request));
  if (!target) {
    return NS_ERROR_UNEXPECTED;
  }
  
  rv = target->AddEventListener(NS_LITERAL_STRING("load"),
                                listener, PR_FALSE);
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = target->AddEventListener(NS_LITERAL_STRING("error"),
                                listener, PR_FALSE);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // The listener keeps the request alive until its complete
  rv = request->Send(nsnull);
  
  return rv;
}

static const char* kSchemaNamespaces[] = {NS_SCHEMA_1999_NAMESPACE, 
                                          NS_SCHEMA_2001_NAMESPACE};
static PRUint32 kSchemaNamespacesLength = sizeof(kSchemaNamespaces) / sizeof(const char*);

/* nsISchema processSchemaElement (in nsIWebServiceErrorHandler aErrorHandler, in nsIDOMElement element); */
NS_IMETHODIMP 
nsSchemaLoader::ProcessSchemaElement(nsIWebServiceErrorHandler* aErrorHandler,
                                     nsIDOMElement* aElement, 
                                     nsISchema **_retval)
{
  NS_ENSURE_ARG(aElement);
  NS_ENSURE_ARG_POINTER(_retval);

  nsresult rv = NS_OK;

  nsSchema* schemaInst = new nsSchema(this, aElement);
  nsCOMPtr<nsISchema> schema = schemaInst;
  if (!schema) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsAutoString targetNamespace;
  schema->GetTargetNamespace(targetNamespace);
  nsStringKey key(targetNamespace);
  nsCOMPtr<nsISupports> old = getter_AddRefs(mSchemas.Get(&key));
  nsCOMPtr<nsISchema> os = do_QueryInterface(old);
  
  if (os) {
    *_retval = os;
    NS_ADDREF(*_retval);
    
    return NS_OK;
  }

  rv = schemaInst->Init();
  NS_ENSURE_SUCCESS(rv, rv);

  nsChildElementIterator iterator(aElement, 
                                  kSchemaNamespaces, kSchemaNamespacesLength);
  nsCOMPtr<nsIDOMElement> childElement;
  nsCOMPtr<nsIAtom> tagName;

  while (NS_SUCCEEDED(iterator.GetNextChild(getter_AddRefs(childElement),
                                            getter_AddRefs(tagName))) &&
         childElement) {
    if (tagName == nsSchemaAtoms::sElement_atom) {
      nsCOMPtr<nsISchemaElement> schemaElement;
      rv = ProcessElement(aErrorHandler, schemaInst, childElement,  
                          getter_AddRefs(schemaElement));
      if (NS_SUCCEEDED(rv)) {
        rv = schemaInst->AddElement(schemaElement);
      }
    }
    else if (tagName == nsSchemaAtoms::sComplexType_atom) {
      nsCOMPtr<nsISchemaComplexType> complexType;
      rv = ProcessComplexType(aErrorHandler, schemaInst, childElement,
                              getter_AddRefs(complexType));
      if (NS_SUCCEEDED(rv)) {
        rv = schemaInst->AddType(complexType);
      }
    }
    else if (tagName == nsSchemaAtoms::sSimpleType_atom) {
      nsCOMPtr<nsISchemaSimpleType> simpleType;
      rv = ProcessSimpleType(aErrorHandler, schemaInst, childElement,
                             getter_AddRefs(simpleType));
      if (NS_SUCCEEDED(rv)) {
        rv = schemaInst->AddType(simpleType);
      }
    }
    else if (tagName == nsSchemaAtoms::sAttribute_atom) {
      nsCOMPtr<nsISchemaAttribute> attribute;
      rv = ProcessAttribute(aErrorHandler, schemaInst, childElement,
                            getter_AddRefs(attribute));
      if (NS_SUCCEEDED(rv)) {
        rv = schemaInst->AddAttribute(attribute);
      }
    }
    else if (tagName == nsSchemaAtoms::sAttributeGroup_atom) {
      nsCOMPtr<nsISchemaAttributeGroup> attributeGroup;
      rv = ProcessAttributeGroup(aErrorHandler, schemaInst, childElement,
                                 getter_AddRefs(attributeGroup));
      if (NS_SUCCEEDED(rv)) {
        rv = schemaInst->AddAttributeGroup(attributeGroup);
      }
    }
    else if (tagName == nsSchemaAtoms::sModelGroup_atom) {
      nsCOMPtr<nsISchemaModelGroup> modelGroup;
      rv = ProcessModelGroup(aErrorHandler, schemaInst, childElement,
                             tagName, nsnull, getter_AddRefs(modelGroup));
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
      nsAutoString elementName;
      nsresult rc = aElement->GetTagName(elementName);
      NS_ENSURE_SUCCESS(rc, rc);
      
      nsAutoString errorMsg;
      errorMsg.AppendLiteral("Failure processing schema, unexpected element \"");
      errorMsg.Append(elementName);
      errorMsg.AppendLiteral("\" in <schema .../>");
      
      NS_SCHEMALOADER_FIRE_ERROR(rv, errorMsg);
      
      return rv;
    }
  }

  // Resolve all forward references 
  rv = schemaInst->Resolve(aErrorHandler);
  if (NS_FAILED(rv)) {
    return rv;
  }

  mSchemas.Put(&key, schema);

  *_retval = schema;
  NS_ADDREF(*_retval);

  return NS_OK;
}

PRBool 
ParseQualifiedName(nsIDOMElement* aContext,
                   const nsAString& aQualifiedName,
                   nsAString& aPrefix,
                   nsAString& aLocalName,
                   nsAString& aNamespaceURI) 
{
  nsReadingIterator<PRUnichar> pos, begin, end;
  
  aQualifiedName.BeginReading(begin);
  aQualifiedName.EndReading(end); 
  pos = begin;
  
  if (FindCharInReadable(PRUnichar(':'), pos, end)) {
    CopyUnicodeTo(begin, pos, aPrefix);
    CopyUnicodeTo(++pos, end, aLocalName);
  }
  else {
    CopyUnicodeTo(begin, end, aLocalName);
  }
  
  nsCOMPtr<nsIDOM3Node> node(do_QueryInterface(aContext));
  
  return node->LookupNamespaceURI(aPrefix, aNamespaceURI);
}

nsresult
nsSchemaLoader::GetNewOrUsedType(nsSchema* aSchema,
                                 nsIDOMElement* aContext,
                                 const nsAString& aTypeName,
                                 nsISchemaType** aType)
{
  nsresult rv = NS_OK;
  nsAutoString prefix, localName, namespaceURI;

  // See if there's a prefix and get the
  // namespace associated with the prefix
  rv = ParseQualifiedName(aContext, aTypeName, prefix, 
                          localName, namespaceURI);
  if (!prefix.IsEmpty() && NS_FAILED(rv)) {
    // Unknown prefix
    return NS_ERROR_SCHEMA_UNKNOWN_PREFIX;
  }

  *aType = nsnull;
  nsAutoString targetNamespace;
  aSchema->GetTargetNamespace(targetNamespace);
  if (namespaceURI.IsEmpty() || namespaceURI.Equals(targetNamespace)) {
    // It's a local type 
    rv = aSchema->GetTypeByName(localName, aType);
  }
  else {
    rv = GetType(localName, namespaceURI, aType);
    if (!*aType) {
      return NS_ERROR_SCHEMA_UNKNOWN_TARGET_NAMESPACE;
    }
  }

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

  return rv;
}

/**
 * @param aErrorHandler Error handler (in).
 * @param aSchema Schema in which is the element, not null (in).
 * @param aElement DOM element <element .../>, not null (in).
 * @param aSchemaElement Schema element from processing DOM element (out).
 */
nsresult 
nsSchemaLoader::ProcessElement(nsIWebServiceErrorHandler* aErrorHandler,
                               nsSchema* aSchema, 
                               nsIDOMElement* aElement,
                               nsISchemaElement** aSchemaElement)
{
  nsresult rv = NS_OK;

  nsCOMPtr<nsISchemaElement> schemaElement;
  PRUint32 minOccurs, maxOccurs;
  GetMinAndMax(aElement, &minOccurs, &maxOccurs);

  // See if it's a reference or an actual element declaration
  nsAutoString ref;
  aElement->GetAttribute(NS_LITERAL_STRING("ref"), ref);
  if (!ref.IsEmpty()) {
    nsSchemaElementRef* elementRef;
    
    elementRef = new nsSchemaElementRef(aSchema, ref);
    if (!elementRef) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    schemaElement = elementRef;

    elementRef->SetMinOccurs(minOccurs);
    elementRef->SetMaxOccurs(maxOccurs);
  }
  else {
    nsAutoString value;
    nsSchemaElement* elementInst;
    const nsAString& empty = EmptyString();

    rv = aElement->GetAttributeNS(empty, NS_LITERAL_STRING("name"), value);
    
    if (NS_FAILED(rv)) {
      nsresult rc = aElement->GetTagName(value);
      NS_ENSURE_SUCCESS(rc, rc);

      nsAutoString errorMsg;
      errorMsg.AppendLiteral("Failure processing schema element, cannot get ");
      errorMsg.AppendLiteral("attribute \"name\" of element \"");
      errorMsg.Append(value);
      errorMsg.AppendLiteral("\"");

      NS_SCHEMALOADER_FIRE_ERROR(rv, errorMsg);

      return rv;
    }

    value.Trim(" \r\n\t");
    elementInst = new nsSchemaElement(aSchema, value);
    if (!elementInst) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    schemaElement = elementInst;

    elementInst->SetMinOccurs(minOccurs);
    elementInst->SetMaxOccurs(maxOccurs);

    nsAutoString defaultValue, fixedValue;
    rv = aElement->GetAttributeNS(empty, NS_LITERAL_STRING("default"),
                                  defaultValue);
    if (NS_FAILED(rv)) {
      nsresult rc = aElement->GetTagName(value);
      NS_ENSURE_SUCCESS(rc, rc);

      nsAutoString errorMsg;
      errorMsg.AppendLiteral("Failure processing schema element, cannot get ");
      errorMsg.AppendLiteral("attribute \"default\" of element \"");
      errorMsg.Append(value);
      errorMsg.AppendLiteral("\"");

      NS_SCHEMALOADER_FIRE_ERROR(rv, errorMsg);

      return rv;
    }
    
    rv = aElement->GetAttributeNS(empty, NS_LITERAL_STRING("fixed"), 
                                  fixedValue);
    if (NS_FAILED(rv)) {
      nsresult rc = aElement->GetTagName(value);
      NS_ENSURE_SUCCESS(rc, rc);

      nsAutoString errorMsg;
      errorMsg.AppendLiteral("Failure processing schema element, cannot get ");
      errorMsg.AppendLiteral("attribute \"fixed\" of element \"");
      errorMsg.Append(value);
      errorMsg.AppendLiteral("\"");

      NS_SCHEMALOADER_FIRE_ERROR(rv, errorMsg);

      return rv;
    }

    elementInst->SetConstraints(defaultValue, fixedValue);

    rv = aElement->GetAttributeNS(empty, NS_LITERAL_STRING("nillable"), value);
    if (NS_FAILED(rv)) {
      nsresult rc = aElement->GetTagName(value);
      NS_ENSURE_SUCCESS(rc, rc);

      nsAutoString errorMsg;
      errorMsg.AppendLiteral("Failure processing schema element, cannot get ");
      errorMsg.AppendLiteral("attribute \"nillable\" of element \"");
      errorMsg.Append(value);
      errorMsg.AppendLiteral("\"");

      NS_SCHEMALOADER_FIRE_ERROR(rv, errorMsg);

      return rv;
    }
    value.Trim(" \r\n\t");

    PRInt32 flags = 0;
    if (value.EqualsLiteral("true"))
      flags |= nsSchemaElement::NILLABLE;

    rv = aElement->GetAttributeNS(empty, NS_LITERAL_STRING("abstract"), value);
    if (NS_FAILED(rv)) {
      nsresult rc = aElement->GetTagName(value);
      NS_ENSURE_SUCCESS(rc, rc);

      nsAutoString errorMsg;
      errorMsg.AppendLiteral("Failure processing schema element, cannot get ");
      errorMsg.AppendLiteral("attribute \"abstract\" of element \"");
      errorMsg.Append(value);
      errorMsg.AppendLiteral("\"");

      NS_SCHEMALOADER_FIRE_ERROR(rv, errorMsg);

      return rv;
    }
    value.Trim(" \r\n\t");

    if (value.EqualsLiteral("true"))
      flags |= nsSchemaElement::ABSTRACT;

    nsCOMPtr<nsIDOMNode> parent;
    rv = aElement->GetParentNode(getter_AddRefs(parent));
    if (NS_FAILED(rv))
      return rv;
    parent->GetLocalName(value);
    
    // Check if the schema element's targetNamespace applies to <element>.
    // Note: If the <element> element information item has <schema> as its 
    // parent,then the actual value of the targetNamespace is that of the
    // parent <schema> element information item, or absent if there is 
    // none. Otherwise if the <element> element information item has 
    // <schema> element as an ancestor then if "form" is present and its actual
    // value is qualified, or if "form" is absent and the actual value of 
    // elementFormDefault on the <schema> ancestor is qualified, then the
    // actual value of the  targetNamespace [attribute] is that of the ancestor
    // <schema> element information item, or absent if there is none.
    if (value.EqualsLiteral("schema")) {
      flags |= nsSchemaElement::FORM_QUALIFIED;
    }
    else {
      rv = aElement->GetAttributeNS(empty, NS_LITERAL_STRING("form"), 
                                    value);
      if (NS_FAILED(rv)) {
        nsAutoString errorMsg;
        errorMsg.AppendLiteral("Failure processing schema element, cannot get ");
        errorMsg.AppendLiteral("attribute \"form\" of element \"");
        errorMsg.Append(value);
        errorMsg.AppendLiteral("\"");

        NS_SCHEMALOADER_FIRE_ERROR(rv, errorMsg);

        return rv;
      }
      value.Trim(" \r\n\t");
      if (value.IsEmpty()) {
        if (aSchema->IsElementFormQualified()) {
           flags |= nsSchemaElement::FORM_QUALIFIED;
        }
        else {
           flags &= ~nsSchemaElement::FORM_QUALIFIED;
        }
      }
      else if (value.EqualsLiteral("qualified")) {
        flags |= nsSchemaElement::FORM_QUALIFIED;
      }
      else {
         flags &= ~nsSchemaElement::FORM_QUALIFIED;
      }
    }

    elementInst->SetFlags(flags);

    nsCOMPtr<nsISchemaType> schemaType;
    nsAutoString typeStr;
    aElement->GetAttribute(NS_LITERAL_STRING("type"), typeStr);
    if (typeStr.Length() > 0) {
      rv = GetNewOrUsedType(aSchema, aElement, typeStr, 
                            getter_AddRefs(schemaType));
      if (NS_FAILED(rv)) {
        nsAutoString errorMsg;
        errorMsg.AppendLiteral("Failure processing schema, unknown type \"");
        errorMsg.Append(typeStr);
        errorMsg.AppendLiteral("\"");

        NS_SCHEMALOADER_FIRE_ERROR(rv, errorMsg);

        return rv;
      }
    }
    // Look for the type as a child of the element 
    else {
      nsChildElementIterator iterator(aElement, 
                                      kSchemaNamespaces, 
                                      kSchemaNamespacesLength);
      nsCOMPtr<nsIDOMElement> childElement;
      nsCOMPtr<nsIAtom> tagName;

      while (NS_SUCCEEDED(iterator.GetNextChild(getter_AddRefs(childElement),
                                                getter_AddRefs(tagName))) &&
             childElement) {
        if (tagName == nsSchemaAtoms::sSimpleType_atom) {
          nsCOMPtr<nsISchemaSimpleType> simpleType;
          
          rv = ProcessSimpleType(aErrorHandler, aSchema, childElement,
                                 getter_AddRefs(simpleType));
          if (NS_FAILED(rv)) {
            return rv;
          }
          schemaType = simpleType;
          break;
        }
        else if (tagName == nsSchemaAtoms::sComplexType_atom) {
          nsCOMPtr<nsISchemaComplexType> complexType;
          
          rv = ProcessComplexType(aErrorHandler, aSchema, childElement,
                                  getter_AddRefs(complexType));
          if (NS_FAILED(rv)) {
            return rv;
          }
          schemaType = complexType;
          break;
        }
      }
    }

    if (!schemaType) {
      nsAutoString ns;
      aElement->GetNamespaceURI(ns);
      rv = GetType(NS_LITERAL_STRING("anyType"),
                   ns,
                   getter_AddRefs(schemaType));
      if (NS_FAILED(rv)) {
        nsAutoString errorMsg;
        errorMsg.AppendLiteral("Failure processing schema, cannot find \'anyType\' ");
        errorMsg.AppendLiteral("placeholder type in namespace \"");
        errorMsg.Append(ns);
        errorMsg.AppendLiteral("\"");

        NS_SCHEMALOADER_FIRE_ERROR(rv, errorMsg);

        return rv;
      }
    }

    rv = elementInst->SetType(schemaType);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  *aSchemaElement = schemaElement;
  NS_ADDREF(*aSchemaElement);

  return NS_OK;
}

/**
 * Handle <complexType ...>
 *
 * @param aErrorHandler Webservice error handler.
 * @param aSchema Owning schema (in)
 * @param aElement <complexType> element (in)
 * @param aComplexType Schema complex type built from |aElement|
 */
nsresult 
nsSchemaLoader::ProcessComplexType(nsIWebServiceErrorHandler* aErrorHandler,
                                   nsSchema* aSchema, 
                                   nsIDOMElement* aElement,
                                   nsISchemaComplexType** aComplexType)
{
  nsresult rv = NS_OK;
  nsCOMPtr<nsISchemaComplexType> complexType;

  nsAutoString abstract, name;
  aElement->GetAttribute(NS_LITERAL_STRING("abstract"), abstract);
  aElement->GetAttribute(NS_LITERAL_STRING("name"), name);

  nsSchemaComplexType* typeInst;
  typeInst = new nsSchemaComplexType(aSchema, name, 
                                     abstract.EqualsLiteral("true"));
  if (!typeInst) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  complexType = typeInst;

  rv = typeInst->Init();
  NS_ENSURE_SUCCESS(rv, rv);

  nsChildElementIterator iterator(aElement, 
                                  kSchemaNamespaces, 
                                  kSchemaNamespacesLength);
  nsCOMPtr<nsIDOMElement> childElement;
  nsCOMPtr<nsIAtom> tagName;

  PRUint16 contentModel = nsISchemaComplexType::CONTENT_MODEL_EMPTY;
  PRUint16 derivation = nsISchemaComplexType::DERIVATION_SELF_CONTAINED;
  nsCOMPtr<nsISchemaType> baseType;
  nsCOMPtr<nsISchemaModelGroup> modelGroup;
  
  while (NS_SUCCEEDED(iterator.GetNextChild(getter_AddRefs(childElement),
                                            getter_AddRefs(tagName))) &&
         childElement) {
    if (tagName == nsSchemaAtoms::sSimpleContent_atom) {
      contentModel = nsISchemaComplexType::CONTENT_MODEL_SIMPLE;
      
      rv = ProcessSimpleContent(aErrorHandler, aSchema, 
                                childElement, typeInst,
                                &derivation, getter_AddRefs(baseType));
      break;
    }
    else if (tagName == nsSchemaAtoms::sComplexContent_atom) {       
      rv = ProcessComplexContent(aErrorHandler, aSchema, 
                                 childElement, typeInst, 
                                 &contentModel, &derivation,
                                 getter_AddRefs(baseType));
      break;                                   
    }
    else if (tagName != nsSchemaAtoms::sAnnotation_atom) {
      rv = ProcessComplexTypeBody(aErrorHandler, aSchema, 
                                  aElement, typeInst, nsnull,
                                  &contentModel);
      break;
    }
    else {
      // Unexpected schema element
      nsAutoString elementName;
      rv = aElement->GetTagName(elementName);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = NS_ERROR_UNEXPECTED;

      nsAutoString errorMsg;
      errorMsg.AppendLiteral("Failure processing schema, unexpected element \"");
      errorMsg.Append(elementName);
      errorMsg.AppendLiteral("\" in <complexType />, should be <simpleContent .../>");
      errorMsg.AppendLiteral(", <complexContent ../>, <annotation .../>");

      NS_SCHEMALOADER_FIRE_ERROR(rv, errorMsg);

      break;
    }
  }

  if (NS_FAILED(rv)) {
    return rv;
  }

  nsAutoString mixed;
  aElement->GetAttribute(NS_LITERAL_STRING("mixed"), mixed);
  if (mixed.EqualsLiteral("true")) {
    contentModel = nsISchemaComplexType::CONTENT_MODEL_MIXED;
  }

  typeInst->SetContentModel(contentModel);
  typeInst->SetDerivation(derivation, baseType);

  *aComplexType = complexType;
  NS_ADDREF(*aComplexType);

  return NS_OK;
}

void
nsSchemaLoader::ConstructArrayName(nsISchemaType* aType,
                                   nsAString& aName)
{
  nsAutoString typeName;
  
  aType->GetName(typeName);
  aName.Assign(NS_LITERAL_STRING("ArrayOf") + typeName);
}

nsresult
nsSchemaLoader::ParseDimensions(nsSchema* aSchema,
                                nsIDOMElement* aAttrElement,
                                const nsAString& aStr,
                                nsISchemaType* aBaseType,
                                nsISchemaType** aArrayType,
                                PRUint32* aDimension)
{
  nsReadingIterator<PRUnichar> iter, done_reading;
  aStr.BeginReading(iter);
  aStr.EndReading(done_reading);

  PRUint32 dimension = 1;
  PRUnichar uc = *iter++;
  if (uc != PRUnichar('[')) {
    return NS_ERROR_UNEXPECTED;
  }

  while (iter != done_reading) {
    uc = *iter++;
    if (uc == PRUnichar(',')) {
      dimension++;
    }
    else if (uc == PRUnichar(']')) {
      break;
    }
  }
  *aDimension = dimension;

  while ((iter != done_reading) && (*iter == PRUnichar(' '))) {
    ++iter;
  }

  // If there's still more to go, then create an array type
  // based on the base and continue to parse
  if ((iter != done_reading) && (*iter == PRUnichar('['))) {
    nsAutoString name;
    nsCOMPtr<nsISchemaType> myArrayType;
    PRUint32 myDimension;
    
    nsresult rv = ParseDimensions(aSchema, aAttrElement,
                                  nsDependentSubstring(iter, done_reading),
                                  aBaseType, getter_AddRefs(myArrayType), 
                                  &myDimension);
    if (NS_FAILED(rv)) {
      return rv;
    }

    ConstructArrayName(myArrayType, name);
    nsSchemaComplexType* typeInst = new nsSchemaComplexType(aSchema,
                                                            name, 
                                                            PR_FALSE);
    if (!typeInst) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    nsCOMPtr<nsISchemaComplexType> complexType = typeInst;

    rv = typeInst->Init();
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsISchemaType> soapArray;
    rv = GetType(NS_LITERAL_STRING("Array"),
                 NS_LITERAL_STRING(NS_SOAP_1_2_ENCODING_NAMESPACE),
                 getter_AddRefs(soapArray));
    if (NS_FAILED(rv)) {
      return rv;
    }

    typeInst->SetContentModel(nsISchemaComplexType::CONTENT_MODEL_ELEMENT_ONLY);
    typeInst->SetDerivation(nsISchemaComplexType::DERIVATION_RESTRICTION_COMPLEX,
                            soapArray);
    typeInst->SetArrayInfo(myArrayType, myDimension);

    *aArrayType = typeInst;
  }
  else {
    *aArrayType = aBaseType;
  }
  NS_ADDREF(*aArrayType);

  return NS_OK;
}

nsresult
nsSchemaLoader::ParseArrayType(nsSchema* aSchema,
                               nsIDOMElement* aAttrElement,
                               const nsAString& aStr,
                               nsISchemaType** aType,
                               PRUint32* aDimension)
{
  PRInt32 offset;
  
  offset = aStr.FindChar(PRUnichar('['));
  if (offset == -1) {
    return NS_ERROR_SCHEMA_UNKNOWN_TYPE;
  }
  nsDependentSubstring typeStr(aStr, 0, offset);
          
  nsCOMPtr<nsISchemaType> type;
  nsresult rv = GetNewOrUsedType(aSchema, aAttrElement, typeStr, 
                                 getter_AddRefs(type));
  if (NS_FAILED(rv)) {
    return rv;
  }
  
  nsDependentSubstring dimensionStr(aStr, offset, 
                                    aStr.Length() - offset);
  return ParseDimensions(aSchema, aAttrElement, dimensionStr, type,
                         aType, aDimension);
}


nsresult
nsSchemaLoader::ProcessComplexTypeBody(nsIWebServiceErrorHandler* aErrorHandler,
                                       nsSchema* aSchema, 
                                       nsIDOMElement* aElement,
                                       nsSchemaComplexType* aComplexType,
                                       nsSchemaModelGroup* aSequence,
                                       PRUint16* aContentModel)
{
  nsresult rv = NS_OK;
  nsChildElementIterator iterator(aElement, 
                                  kSchemaNamespaces, 
                                  kSchemaNamespacesLength);
  nsCOMPtr<nsIDOMElement> childElement;
  nsCOMPtr<nsIAtom> tagName;

  *aContentModel = nsISchemaComplexType::CONTENT_MODEL_EMPTY;

  nsCOMPtr<nsISchemaModelGroup> modelGroup;
  
  while (NS_SUCCEEDED(iterator.GetNextChild(getter_AddRefs(childElement),
                                            getter_AddRefs(tagName))) &&
         childElement) {
    if ((tagName == nsSchemaAtoms::sModelGroup_atom) ||
        (tagName == nsSchemaAtoms::sAll_atom) ||
        (tagName == nsSchemaAtoms::sChoice_atom) || 
        (tagName == nsSchemaAtoms::sSequence_atom)) {

      if (modelGroup) {
        // We shouldn't already have a model group
        nsAutoString elementName;
        nsresult rv = childElement->GetTagName(elementName);
        NS_ENSURE_SUCCESS(rv, rv);

        nsAutoString errorMsg;
        errorMsg.AppendLiteral("Failure processing schema, must have ");
        errorMsg.AppendLiteral("model group in <complexType .../>, ");
        errorMsg.AppendLiteral("unexpected element \"");
        errorMsg.Append(elementName);
        errorMsg.AppendLiteral("\"");

        NS_SCHEMALOADER_FIRE_ERROR(NS_ERROR_SCHEMA_INVALID_STRUCTURE, errorMsg);

        return NS_ERROR_SCHEMA_INVALID_STRUCTURE;
      }
      
      rv = ProcessModelGroup(aErrorHandler, aSchema, 
                             childElement, tagName,
                             aSequence, getter_AddRefs(modelGroup));
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
        // Check if we were collapsed
        if (modelGroup.get() != NS_STATIC_CAST(nsISchemaModelGroup*, 
                                               aSequence)) {
          rv = aSequence->AddParticle(modelGroup);
        }
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
      
      rv = ProcessAttributeComponent(aErrorHandler, aSchema, 
                                     childElement, tagName,
                                     getter_AddRefs(attribute));
      if (NS_FAILED(rv)) {
        return rv;
      }

      rv = aComplexType->AddAttribute(attribute);
      if (NS_FAILED(rv)) {
        nsAutoString elementName;
        nsAutoString attributeName;
        nsresult rc = childElement->GetTagName(elementName);
        NS_ENSURE_SUCCESS(rc, rc);

        rc = attribute->GetName(attributeName);
        NS_ENSURE_SUCCESS(rc, rc);

        nsAutoString errorMsg;
        errorMsg.AppendLiteral("Failure processing schema, cannot process attribute \"");
        errorMsg.Append(attributeName);
        errorMsg.AppendLiteral("\" of element \"");
        errorMsg.Append(elementName);
        errorMsg.AppendLiteral("\"");

        NS_SCHEMALOADER_FIRE_ERROR(rv, errorMsg);

        return rv;
      }

      // XXX WSDL ugliness making itself into schemas. Hopefully this
      // mechanism for specifying an array type in schemas will die
      // when the Schema WG address qualified names in attribute
      // default values.
      if (tagName == nsSchemaAtoms::sAttribute_atom) {
#define NS_WSDL_NAMESPACE "http://schemas.xmlsoap.org/wsdl/"
        nsAutoString arrayType;
        childElement->GetAttributeNS(NS_LITERAL_STRING(NS_WSDL_NAMESPACE),
                                     NS_LITERAL_STRING("arrayType"), 
                                     arrayType);
        if (!arrayType.IsEmpty()) {
          nsCOMPtr<nsISchemaType> arraySchemaType;
          PRUint32 arrayDimension;
          rv = ParseArrayType(aSchema, 
                              childElement,
                              arrayType, 
                              getter_AddRefs(arraySchemaType),
                              &arrayDimension);
          if (NS_FAILED(rv)) {
            nsAutoString errorMsg;
            errorMsg.AppendLiteral("Failure processing schema, ");
            errorMsg.AppendLiteral("cannot process array type \"");
            errorMsg.Append(arrayType);
            errorMsg.AppendLiteral("\"");

            NS_SCHEMALOADER_FIRE_ERROR(rv, errorMsg);

            return rv;
          }

          rv = aComplexType->SetArrayInfo(arraySchemaType, arrayDimension);
          if (NS_FAILED(rv)) {
            nsAutoString errorMsg;
            errorMsg.AppendLiteral("Failure processing schema, cannot set ");
            errorMsg.AppendLiteral("array information for array type \"");
            errorMsg.Append(arrayType);
            errorMsg.AppendLiteral("\"");

            NS_SCHEMALOADER_FIRE_ERROR(rv, errorMsg);

            return rv;
          }
        }
      }
    }
  }

  return rv;
}

nsresult 
nsSchemaLoader::ProcessSimpleContent(nsIWebServiceErrorHandler* aErrorHandler,
                                     nsSchema* aSchema, 
                                     nsIDOMElement* aElement,
                                     nsSchemaComplexType* aComplexType,
                                     PRUint16* aDerivation,
                                     nsISchemaType** aBaseType)
{
  nsresult rv = NS_OK;

  nsCOMPtr<nsISchemaType> baseType;

  nsChildElementIterator iterator(aElement, 
                                  kSchemaNamespaces, 
                                  kSchemaNamespacesLength);
  nsCOMPtr<nsIDOMElement> childElement;
  nsCOMPtr<nsIAtom> tagName;
  
  // A simpleContent element must have children
  if (!iterator.HasChildNodes()) {
    nsAutoString errorMsg(NS_LITERAL_STRING("Failure processing schema, "));
    errorMsg.AppendLiteral("<simpleContent .../> invalid structure, should contains ");
    errorMsg.AppendLiteral("<restriction .../> or <extension .../>");

    NS_SCHEMALOADER_FIRE_ERROR(NS_ERROR_SCHEMA_INVALID_STRUCTURE, errorMsg);

    return NS_ERROR_SCHEMA_INVALID_STRUCTURE;
  }

  while (NS_SUCCEEDED(iterator.GetNextChild(getter_AddRefs(childElement),
                                            getter_AddRefs(tagName))) &&
         childElement) {       
    nsAutoString baseStr;
    if ((tagName == nsSchemaAtoms::sRestriction_atom) ||
        (tagName == nsSchemaAtoms::sExtension_atom)) {
      childElement->GetAttribute(NS_LITERAL_STRING("base"), baseStr);
      if (baseStr.IsEmpty()) {
        nsAutoString elementName;
        rv = childElement->GetTagName(elementName);
        NS_ENSURE_SUCCESS(rv, rv);

        nsAutoString errorMsg;
        errorMsg.AppendLiteral("Failure processing schema, \"");
        errorMsg.Append(elementName);
        errorMsg.AppendLiteral("\" must have a \"base\" attribute in order ");
        errorMsg.AppendLiteral("to specify base type");

        NS_SCHEMALOADER_FIRE_ERROR(NS_ERROR_SCHEMA_MISSING_TYPE, errorMsg);

        return NS_ERROR_SCHEMA_MISSING_TYPE;
      }
      
      rv = GetNewOrUsedType(aSchema, childElement, baseStr, 
                            getter_AddRefs(baseType));
      if (NS_FAILED(rv)) {
        nsAutoString errorMsg;
        errorMsg.AppendLiteral("Failure processing schema, unknown base type \"");
        errorMsg.Append(baseStr);
        errorMsg.AppendLiteral("\"");

        NS_SCHEMALOADER_FIRE_ERROR(rv, errorMsg);

        return rv;
      }

      nsCOMPtr<nsISchemaSimpleType> simpleBaseType;
      if (tagName == nsSchemaAtoms::sRestriction_atom) {
        *aDerivation = nsISchemaComplexType::DERIVATION_RESTRICTION_SIMPLE;
        rv = ProcessSimpleContentRestriction(aErrorHandler, aSchema, childElement,
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
              nsAutoString errorMsg;
              errorMsg.AppendLiteral("Failure processing schema, cannot clone ");
              errorMsg.AppendLiteral("attributes from base type \"");
              errorMsg.Append(baseStr);
              errorMsg.AppendLiteral("\"");

              NS_SCHEMALOADER_FIRE_ERROR(rv, errorMsg);

              return rv;
            }

            rv = aComplexType->AddAttribute(attribute);
            if (NS_FAILED(rv)) {
              nsAutoString errorMsg;
              errorMsg.AppendLiteral("Failure processing schema, cannot clone ");
              errorMsg.AppendLiteral("attributes from base type \"");
              errorMsg.Append(baseStr);
              errorMsg.AppendLiteral("\"");
              
              NS_SCHEMALOADER_FIRE_ERROR(rv, errorMsg);

              return rv;
            }
          }
        }
        
        rv = ProcessSimpleContentExtension(aErrorHandler, aSchema, childElement,
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

  *aBaseType = baseType;
  NS_IF_ADDREF(*aBaseType);

  return NS_OK;
}

nsresult 
nsSchemaLoader::ProcessSimpleContentRestriction(nsIWebServiceErrorHandler* aErrorHandler,
                                                nsSchema* aSchema, 
                                                nsIDOMElement* aElement,
                                                nsSchemaComplexType* aComplexType, 
                                                nsISchemaType* aBaseType,
                                                nsISchemaSimpleType** aSimpleBaseType)
{
  nsresult rv = NS_OK;

  nsChildElementIterator iterator(aElement, 
                                  kSchemaNamespaces, 
                                  kSchemaNamespacesLength);
  nsCOMPtr<nsIDOMElement> childElement;
  nsCOMPtr<nsIAtom> tagName;
  
  nsSchemaRestrictionType* restrictionInst;
  nsCOMPtr<nsISchemaSimpleType> simpleBase;
 
  restrictionInst = new nsSchemaRestrictionType(aSchema, EmptyString());
  if (!restrictionInst) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  simpleBase = restrictionInst;
  
  // The base type must actually be a complex type (which itself must
  // have a simple base type.
  nsCOMPtr<nsISchemaComplexType> complexBase = do_QueryInterface(aBaseType);
  if (!complexBase) {
    nsAutoString baseStr;
    rv = aBaseType->GetName(baseStr);
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoString errorMsg;
    errorMsg.AppendLiteral("Failure processing schema, base type \"");
    errorMsg.Append(baseStr);
    errorMsg.AppendLiteral("\" of restriction must be a complex type ");
    errorMsg.AppendLiteral("which itself must be based on a simple type");

    NS_SCHEMALOADER_FIRE_ERROR(NS_ERROR_SCHEMA_INVALID_TYPE_USAGE, errorMsg);

    return NS_ERROR_SCHEMA_INVALID_TYPE_USAGE;
  }

  nsCOMPtr<nsISchemaSimpleType> parentSimpleBase;
  complexBase->GetSimpleBaseType(getter_AddRefs(parentSimpleBase));
  
  if (parentSimpleBase) {
    rv = restrictionInst->SetBaseType(parentSimpleBase);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  while (NS_SUCCEEDED(iterator.GetNextChild(getter_AddRefs(childElement),
                                            getter_AddRefs(tagName))) &&
         childElement) {       
    if (tagName == nsSchemaAtoms::sSimpleType_atom) {
      nsCOMPtr<nsISchemaSimpleType> simpleType;
      
      rv = ProcessSimpleType(aErrorHandler, aSchema, childElement, 
                             getter_AddRefs(simpleType));
      if (NS_FAILED(rv)) {
        return rv;
      }

      rv = restrictionInst->SetBaseType(simpleType);
      if (NS_FAILED(rv)) {
        return rv;
      }
    }
    else if ((tagName == nsSchemaAtoms::sMinExclusive_atom) ||
             (tagName == nsSchemaAtoms::sMinInclusive_atom) ||
             (tagName == nsSchemaAtoms::sMaxExclusive_atom) ||
             (tagName == nsSchemaAtoms::sMaxInclusive_atom) ||
             (tagName == nsSchemaAtoms::sTotalDigits_atom) ||
             (tagName == nsSchemaAtoms::sFractionDigits_atom) ||
             (tagName == nsSchemaAtoms::sLength_atom) ||
             (tagName == nsSchemaAtoms::sMinLength_atom) ||
             (tagName == nsSchemaAtoms::sMaxLength_atom) ||
             (tagName == nsSchemaAtoms::sEnumeration_atom) ||
             (tagName == nsSchemaAtoms::sWhiteSpace_atom) ||
             (tagName == nsSchemaAtoms::sPattern_atom)) {
      nsCOMPtr<nsISchemaFacet> facet;
      
      rv = ProcessFacet(aErrorHandler, aSchema, childElement, 
                        tagName, getter_AddRefs(facet));
      if (NS_FAILED(rv)) {
        return rv;
      }

      rv = restrictionInst->AddFacet(facet);
      if (NS_FAILED(rv)) {
        return rv;
      }
    }
    else if ((tagName == nsSchemaAtoms::sAttribute_atom) ||
             (tagName == nsSchemaAtoms::sAttributeGroup_atom) ||
             (tagName == nsSchemaAtoms::sAnyAttribute_atom)) {
      nsCOMPtr<nsISchemaAttributeComponent> attribute;
      
      rv = ProcessAttributeComponent(aErrorHandler, aSchema,
                                     childElement, tagName,
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
  
  *aSimpleBaseType = simpleBase;
  NS_IF_ADDREF(*aSimpleBaseType);

  return NS_OK;
}
 
nsresult 
nsSchemaLoader::ProcessSimpleContentExtension(nsIWebServiceErrorHandler* aErrorHandler,
                                              nsSchema* aSchema, 
                                              nsIDOMElement* aElement,
                                              nsSchemaComplexType* aComplexType,
                                              nsISchemaType* aBaseType,
                                              nsISchemaSimpleType** aSimpleBaseType)
{
  nsresult rv = NS_OK;

  nsChildElementIterator iterator(aElement, 
                                  kSchemaNamespaces, 
                                  kSchemaNamespacesLength);
  nsCOMPtr<nsIDOMElement> childElement;
  nsCOMPtr<nsIAtom> tagName;

  // If the base type is a complex type, it must itself have a simple
  // base type
  nsCOMPtr<nsISchemaComplexType> complexBase = do_QueryInterface(aBaseType);
  if (complexBase) {
    complexBase->GetSimpleBaseType(aSimpleBaseType);
  }
  else {
    aBaseType->QueryInterface(NS_GET_IID(nsISchemaSimpleType),
                              (void**)aSimpleBaseType);
  }

  while (NS_SUCCEEDED(iterator.GetNextChild(getter_AddRefs(childElement),
                                            getter_AddRefs(tagName))) &&
         childElement) {       
    if ((tagName == nsSchemaAtoms::sAttribute_atom) ||
        (tagName == nsSchemaAtoms::sAttributeGroup_atom) ||
        (tagName == nsSchemaAtoms::sAnyAttribute_atom)) {
      nsCOMPtr<nsISchemaAttributeComponent> attribute;
      
      rv = ProcessAttributeComponent(aErrorHandler, aSchema, 
                                     childElement, tagName,
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
  
  return NS_OK;
}
 
nsresult 
nsSchemaLoader::ProcessComplexContent(nsIWebServiceErrorHandler* aErrorHandler,
                                      nsSchema* aSchema, 
                                      nsIDOMElement* aElement,
                                      nsSchemaComplexType* aComplexType,
                                      PRUint16* aContentModel,
                                      PRUint16* aDerivation,
                                      nsISchemaType** aBaseType)
{
  nsresult rv = NS_OK;

  nsCOMPtr<nsISchemaType> baseType;
  nsChildElementIterator iterator(aElement, 
                                  kSchemaNamespaces, 
                                  kSchemaNamespacesLength);
  nsCOMPtr<nsIDOMElement> childElement;
  nsCOMPtr<nsIAtom> tagName;
  
  // A complexContent element must have children
  if (!iterator.HasChildNodes()) {
    nsAutoString errorMsg(NS_LITERAL_STRING("Failure processing schema, "));
    errorMsg.AppendLiteral("<complexContent .../> must contains ");
    errorMsg.AppendLiteral("<restriction .../> or <extension .../>");
    NS_SCHEMALOADER_FIRE_ERROR(NS_ERROR_SCHEMA_INVALID_STRUCTURE, errorMsg);

    return NS_ERROR_SCHEMA_INVALID_STRUCTURE;
  }
  
  while (NS_SUCCEEDED(iterator.GetNextChild(getter_AddRefs(childElement),
                                            getter_AddRefs(tagName))) &&
         childElement) {       
    nsAutoString baseStr;
    if ((tagName == nsSchemaAtoms::sRestriction_atom) ||
        (tagName == nsSchemaAtoms::sExtension_atom)) {
      childElement->GetAttribute(NS_LITERAL_STRING("base"), baseStr);
      if (baseStr.IsEmpty()) {
        nsAutoString errorMsg;
        errorMsg.AppendLiteral("Failure processing schema, ");
        
        if (tagName == nsSchemaAtoms::sRestriction_atom) {
          errorMsg.AppendLiteral("restriction");
        }
        else {
          errorMsg.AppendLiteral("extension");
        }
        
        errorMsg.AppendLiteral(" must have a \"base\" attribute in order to ");
        errorMsg.AppendLiteral("specify base type");
        
        NS_SCHEMALOADER_FIRE_ERROR(NS_ERROR_SCHEMA_MISSING_TYPE, errorMsg);
        
        return NS_ERROR_SCHEMA_MISSING_TYPE;
      }
      
      rv = GetNewOrUsedType(aSchema, childElement, baseStr, 
                            getter_AddRefs(baseType));
      if (NS_FAILED(rv)) {
        nsAutoString errorMsg;
        errorMsg.AppendLiteral("Failure processing schema, unknown base type \"");
        errorMsg.Append(baseStr);
        errorMsg.AppendLiteral("\"");

        NS_SCHEMALOADER_FIRE_ERROR(rv, errorMsg);

        return rv;
      }
      
      if (tagName == nsSchemaAtoms::sRestriction_atom) {
        *aDerivation = nsISchemaComplexType::DERIVATION_RESTRICTION_COMPLEX;
        rv = ProcessComplexTypeBody(aErrorHandler, aSchema, childElement,
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
            nsAutoString errorMsg;
            errorMsg.AppendLiteral("Failure processing schema, extension for type \"");
            errorMsg.Append(baseStr);
            errorMsg.AppendLiteral("\" does not contains any model group");
            errorMsg.AppendLiteral("such as <all>, <choice>, <sequence>, or <group>");

            NS_SCHEMALOADER_FIRE_ERROR(rv, errorMsg);

            return rv;
          }
          
          if (baseGroup) {
            // Create a new model group that's going to be the a sequence
            // of the base model group and the content below
            sequenceInst = new nsSchemaModelGroup(aSchema, EmptyString());
            if (!sequenceInst) {
              return NS_ERROR_OUT_OF_MEMORY;
            }
            sequence = sequenceInst;

            PRUint16 compositor;
            baseGroup->GetCompositor(&compositor);

            PRUint32 minOccurs, maxOccurs;
            baseGroup->GetMinOccurs(&minOccurs);
            baseGroup->GetMaxOccurs(&maxOccurs);

            // If the base group also a sequence, we can collapse the 
            // two sequences.
            if ((compositor == nsISchemaModelGroup::COMPOSITOR_SEQUENCE) &&
                (minOccurs == 1) && (maxOccurs == 1)) {
              PRUint32 pIndex, pCount;
              baseGroup->GetParticleCount(&pCount);
              for (pIndex = 0; pIndex < pCount; pIndex++) {
                nsCOMPtr<nsISchemaParticle> particle;
                
                rv = baseGroup->GetParticle(pIndex, getter_AddRefs(particle));
                if (NS_FAILED(rv)) {
                  nsAutoString errorMsg;
                  errorMsg.AppendLiteral("Failure processing schema, failure ");
                  errorMsg.AppendLiteral("processing model group for extension ");
                  errorMsg.AppendLiteral("of type \"");
                  errorMsg.Append(baseStr);
                  errorMsg.AppendLiteral("\"");

                  NS_SCHEMALOADER_FIRE_ERROR(rv, errorMsg);

                  return rv;
                }
                
                rv = sequenceInst->AddParticle(particle);
                if (NS_FAILED(rv)) {
                  nsAutoString errorMsg;
                  errorMsg.AppendLiteral("Failure processing schema, failure ");
                  errorMsg.AppendLiteral("processing model group for extension ");
                  errorMsg.AppendLiteral("of type \"");
                  errorMsg.Append(baseStr);
                  errorMsg.AppendLiteral("\"");

                  NS_SCHEMALOADER_FIRE_ERROR(rv, errorMsg);

                  return rv;
                }
              }
            }
            else {
              sequenceInst->AddParticle(baseGroup);
            }
            
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
              nsAutoString errorMsg;
              errorMsg.AppendLiteral("Failure processing schema, cannot clone ");
              errorMsg.AppendLiteral("attributes from base type \"");
              errorMsg.Append(baseStr);
              errorMsg.AppendLiteral("\"");

              NS_SCHEMALOADER_FIRE_ERROR(rv, errorMsg);

              return rv;
            }

            rv = aComplexType->AddAttribute(attribute);
            if (NS_FAILED(rv)) {
              nsAutoString errorMsg;
              errorMsg.AppendLiteral("Failure processing schema, cannot clone ");
              errorMsg.AppendLiteral("attributes from base type \"");
              errorMsg.Append(baseStr);
              errorMsg.AppendLiteral("\"");

              NS_SCHEMALOADER_FIRE_ERROR(rv, errorMsg);

              return rv;
            }
          }
        }
        
        PRUint16 explicitContent;
        rv = ProcessComplexTypeBody(aErrorHandler, aSchema, childElement,
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
        else {
          *aContentModel = explicitContent;
        }
      }
      break;
    }
  }

  nsAutoString mixed;
  aElement->GetAttribute(NS_LITERAL_STRING("mixed"), mixed);
  if (mixed.EqualsLiteral("true")) {
    *aContentModel = nsISchemaComplexType::CONTENT_MODEL_MIXED;
  }

  *aBaseType = baseType;
  NS_IF_ADDREF(*aBaseType);

  return NS_OK;
}

nsresult 
nsSchemaLoader::ProcessSimpleType(nsIWebServiceErrorHandler* aErrorHandler,
                                  nsSchema* aSchema, 
                                  nsIDOMElement* aElement,
                                  nsISchemaSimpleType** aSimpleType)
{
  nsresult rv = NS_OK;

  nsAutoString name;
  aElement->GetAttribute(NS_LITERAL_STRING("name"), name);

  nsChildElementIterator iterator(aElement, 
                                  kSchemaNamespaces, 
                                  kSchemaNamespacesLength);
  nsCOMPtr<nsIDOMElement> childElement;
  nsCOMPtr<nsIAtom> tagName;

  while (NS_SUCCEEDED(iterator.GetNextChild(getter_AddRefs(childElement),
                                            getter_AddRefs(tagName))) &&
         childElement) {
    if (tagName == nsSchemaAtoms::sRestriction_atom) {
      rv = ProcessSimpleTypeRestriction(aErrorHandler, aSchema, childElement,
                                        name, aSimpleType);
      break;
    }
    else if (tagName == nsSchemaAtoms::sList_atom) {
      rv = ProcessSimpleTypeList(aErrorHandler, aSchema, childElement,
                                 name, aSimpleType);
      break;
    }
    else if (tagName = nsSchemaAtoms::sUnion_atom) {
      rv = ProcessSimpleTypeUnion(aErrorHandler, aSchema, childElement,
                                  name, aSimpleType);
      break;
    }
  }  
  
  return rv;
}

nsresult 
nsSchemaLoader::ProcessSimpleTypeRestriction(nsIWebServiceErrorHandler* aErrorHandler,
                                             nsSchema* aSchema, 
                                             nsIDOMElement* aElement,
                                             const nsAString& aName,
                                             nsISchemaSimpleType** aSimpleType)
{
  nsresult rv = NS_OK;

  nsSchemaRestrictionType* restrictionInst;
  nsCOMPtr<nsISchemaSimpleType> restriction;
 
  restrictionInst = new nsSchemaRestrictionType(aSchema, aName);
  if (!restrictionInst) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  restriction = restrictionInst;

  nsCOMPtr<nsISchemaType> baseType;
  nsAutoString baseStr;
  aElement->GetAttribute(NS_LITERAL_STRING("base"), baseStr);
  if (!baseStr.IsEmpty()) {
    rv = GetNewOrUsedType(aSchema, aElement, baseStr, 
                          getter_AddRefs(baseType));
    if (NS_FAILED(rv)) {
      nsAutoString errorMsg;
      errorMsg.AppendLiteral("Failure processing schema, unknown base type \"");
      errorMsg.Append(baseStr);
      errorMsg.AppendLiteral("\"");

      NS_SCHEMALOADER_FIRE_ERROR(rv, errorMsg);

      return rv;
    }

    nsCOMPtr<nsISchemaSimpleType> simpleBase(do_QueryInterface(baseType));
    if (!simpleBase) {
      nsAutoString errorMsg;
      errorMsg.AppendLiteral("Failure processing schema, base type \"");
      errorMsg.Append(baseStr);
      errorMsg.AppendLiteral("\" should be a simple type");

      NS_SCHEMALOADER_FIRE_ERROR(NS_ERROR_SCHEMA_INVALID_TYPE_USAGE, errorMsg);

      return NS_ERROR_SCHEMA_INVALID_TYPE_USAGE;
    }
    rv = restrictionInst->SetBaseType(simpleBase);
  }

  nsChildElementIterator iterator(aElement, 
                                  kSchemaNamespaces, 
                                  kSchemaNamespacesLength);
  nsCOMPtr<nsIDOMElement> childElement;
  nsCOMPtr<nsIAtom> tagName;

  while (NS_SUCCEEDED(iterator.GetNextChild(getter_AddRefs(childElement),
                                            getter_AddRefs(tagName))) &&
         childElement) {
    if ((tagName == nsSchemaAtoms::sSimpleType_atom) &&
        !baseType) {
      nsCOMPtr<nsISchemaSimpleType> simpleType;
      
      rv = ProcessSimpleType(aErrorHandler, aSchema, childElement, 
                             getter_AddRefs(simpleType));
      if (NS_FAILED(rv)) {
        return rv;
      }

      rv = restrictionInst->SetBaseType(simpleType);
      if (NS_FAILED(rv)) {
        return rv;
      }
      baseType = simpleType;
    }
    else if ((tagName == nsSchemaAtoms::sMinExclusive_atom) ||
             (tagName == nsSchemaAtoms::sMinInclusive_atom) ||
             (tagName == nsSchemaAtoms::sMaxExclusive_atom) ||
             (tagName == nsSchemaAtoms::sMaxInclusive_atom) ||
             (tagName == nsSchemaAtoms::sTotalDigits_atom) ||
             (tagName == nsSchemaAtoms::sFractionDigits_atom) ||
             (tagName == nsSchemaAtoms::sLength_atom) ||
             (tagName == nsSchemaAtoms::sMinLength_atom) ||
             (tagName == nsSchemaAtoms::sMaxLength_atom) ||
             (tagName == nsSchemaAtoms::sEnumeration_atom) ||
             (tagName == nsSchemaAtoms::sWhiteSpace_atom) ||
             (tagName == nsSchemaAtoms::sPattern_atom)) {
      nsCOMPtr<nsISchemaFacet> facet;
      
      rv = ProcessFacet(aErrorHandler, aSchema, childElement, 
                        tagName, getter_AddRefs(facet));
      if (NS_FAILED(rv)) {
        return rv;
      }

      rv = restrictionInst->AddFacet(facet);
      if (NS_FAILED(rv)) {
        return rv;
      }
    }
  }

  *aSimpleType = restriction;
  NS_ADDREF(*aSimpleType);

  return NS_OK;
}
 
nsresult 
nsSchemaLoader::ProcessSimpleTypeList(nsIWebServiceErrorHandler* aErrorHandler,
                                      nsSchema* aSchema, 
                                      nsIDOMElement* aElement,
                                      const nsAString& aName,
                                      nsISchemaSimpleType** aSimpleType)
{
  nsresult rv = NS_OK;

  nsSchemaListType* listInst;
  nsCOMPtr<nsISchemaSimpleType> list;

  listInst = new nsSchemaListType(aSchema, aName);
  if (!listInst) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  list = listInst;

  nsAutoString itemTypeStr;
  aElement->GetAttribute(NS_LITERAL_STRING("itemType"), itemTypeStr);

  nsCOMPtr<nsISchemaSimpleType> itemType;
  if (!itemTypeStr.IsEmpty()) {
    nsCOMPtr<nsISchemaType> type;
    rv = GetNewOrUsedType(aSchema, aElement, itemTypeStr, 
                          getter_AddRefs(type));
    if (NS_FAILED(rv)) {
      nsAutoString errorMsg;
      errorMsg.AppendLiteral("Failure processing schema, unknown item type \"");
      errorMsg.Append(itemTypeStr);
      errorMsg.AppendLiteral("\"");

      NS_SCHEMALOADER_FIRE_ERROR(rv, errorMsg);

      return rv;
    }

    itemType = do_QueryInterface(type);
  }
  else {
    nsChildElementIterator iterator(aElement, 
                                    kSchemaNamespaces, 
                                    kSchemaNamespacesLength);
    nsCOMPtr<nsIDOMElement> childElement;
    nsCOMPtr<nsIAtom> tagName;
    
    while (NS_SUCCEEDED(iterator.GetNextChild(getter_AddRefs(childElement),
                                              getter_AddRefs(tagName))) &&
           childElement) {
      if (tagName == nsSchemaAtoms::sSimpleType_atom) {
        rv = ProcessSimpleType(aErrorHandler, aSchema, childElement,
                               getter_AddRefs(itemType));
        if (NS_FAILED(rv)) {
          return rv;
        }
        break;
      }
    }
  }

  if (!itemType) {
    nsAutoString errorMsg;
    errorMsg.AppendLiteral("Failure processing schema, no item type ");
    errorMsg.AppendLiteral("for simple type \"");
    errorMsg.Append(aName);
    errorMsg.AppendLiteral("\"");

    NS_SCHEMALOADER_FIRE_ERROR(NS_ERROR_SCHEMA_MISSING_TYPE, errorMsg);

    return NS_ERROR_SCHEMA_MISSING_TYPE;
  }
  listInst->SetListType(itemType);

  *aSimpleType = list;
  NS_ADDREF(*aSimpleType);

  return NS_OK;
}

nsresult 
nsSchemaLoader::ProcessSimpleTypeUnion(nsIWebServiceErrorHandler* aErrorHandler, 
                                       nsSchema* aSchema, 
                                       nsIDOMElement* aElement,
                                       const nsAString& aName,
                                       nsISchemaSimpleType** aSimpleType)
{
  nsresult rv = NS_OK;

  nsSchemaUnionType* unionInst;
  nsCOMPtr<nsISchemaSimpleType> unionType;

  unionInst = new nsSchemaUnionType(aSchema, aName);
  if (!unionInst) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  unionType = unionInst;

  nsCOMPtr<nsISchemaSimpleType> memberType;
  nsAutoString memberTypes;
  aElement->GetAttribute(NS_LITERAL_STRING("memberTypes"), memberTypes);
  if (!memberTypes.IsEmpty()) {
    nsReadingIterator<PRUnichar> begin, end, tokenEnd;

    memberTypes.BeginReading(tokenEnd);
    memberTypes.EndReading(end);

    while (tokenEnd != end) {
      nsAutoString typeStr;
      begin = tokenEnd;
      if (FindCharInReadable(PRUnichar(' '), tokenEnd, end)) {
        CopyUnicodeTo(begin, tokenEnd, typeStr);
        ++tokenEnd;
      }
      else {
        CopyUnicodeTo(begin, end, typeStr);
      }

      nsCOMPtr<nsISchemaType> type;
      rv = GetNewOrUsedType(aSchema, aElement, typeStr, 
                            getter_AddRefs(type));
      if (NS_FAILED(rv)) {
        nsAutoString errorMsg;
        errorMsg.AppendLiteral("Failure processing schema, unknown type \"");
        errorMsg.Append(typeStr);
        errorMsg.AppendLiteral("\"");

        NS_SCHEMALOADER_FIRE_ERROR(rv, errorMsg);

        return rv;
      }
      
      memberType = do_QueryInterface(type);
      if (!memberType) {
        nsAutoString errorMsg;
        errorMsg.AppendLiteral("Failure processing schema, invalid member type \"");
        errorMsg.Append(typeStr);
        errorMsg.AppendLiteral("\" for union about simple type \"");
        errorMsg.Append(aName);
        errorMsg.AppendLiteral("\"");

        NS_SCHEMALOADER_FIRE_ERROR(NS_ERROR_SCHEMA_INVALID_TYPE_USAGE, errorMsg);

        return NS_ERROR_SCHEMA_INVALID_TYPE_USAGE;
      }

      rv = unionInst->AddUnionType(memberType);
      if (NS_FAILED(rv)) {
        return rv;
      }            
    }
  }
  
  nsChildElementIterator iterator(aElement, 
                                  kSchemaNamespaces, 
                                  kSchemaNamespacesLength);
  nsCOMPtr<nsIDOMElement> childElement;
  nsCOMPtr<nsIAtom> tagName;
  
  while (NS_SUCCEEDED(iterator.GetNextChild(getter_AddRefs(childElement),
                                            getter_AddRefs(tagName))) &&
         childElement) {
    if (tagName == nsSchemaAtoms::sSimpleType_atom) {
      rv = ProcessSimpleType(aErrorHandler, aSchema, childElement,
                             getter_AddRefs(memberType));
      if (NS_FAILED(rv)) {
        return rv;
      }
      
      rv = unionInst->AddUnionType(memberType);
      if (NS_FAILED(rv)) {
        return rv;
      }      
    }
  }

  *aSimpleType = unionType;
  NS_ADDREF(*aSimpleType);

  return NS_OK;
}

nsresult 
nsSchemaLoader::ProcessModelGroup(nsIWebServiceErrorHandler* aErrorHandler,
                                  nsSchema* aSchema, 
                                  nsIDOMElement* aElement,
                                  nsIAtom* aTagName,
                                  nsSchemaModelGroup* aParentSequence,
                                  nsISchemaModelGroup** aModelGroup)
{
  nsresult rv = NS_OK;

  nsCOMPtr<nsISchemaModelGroup> modelGroup;
  PRUint32 minOccurs, maxOccurs;
  GetMinAndMax(aElement, &minOccurs, &maxOccurs);

  // Check for a ref attribute
  nsAutoString ref;
  aElement->GetAttribute(NS_LITERAL_STRING("ref"), ref);
  
  if ((aTagName == nsSchemaAtoms::sModelGroup_atom) &&
      !ref.IsEmpty()) {
    nsSchemaModelGroupRef* modelGroupRef = new nsSchemaModelGroupRef(aSchema, 
                                                                     ref);
    if (!modelGroupRef) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    modelGroup = modelGroupRef;
    
    modelGroupRef->SetMinOccurs(minOccurs);
    modelGroupRef->SetMaxOccurs(maxOccurs);
  }
  else {
    nsAutoString name;
    aElement->GetAttribute(NS_LITERAL_STRING("name"), name);

    nsChildElementIterator iterator(aElement, 
                                    kSchemaNamespaces, 
                                    kSchemaNamespacesLength);
    nsCOMPtr<nsIDOMElement> childElement;
    nsCOMPtr<nsIAtom> tagName = aTagName;

    // If this is a group element, find the first compositor
    // child and continue with that.
    if (aTagName == nsSchemaAtoms::sModelGroup_atom) {
      while (NS_SUCCEEDED(iterator.GetNextChild(getter_AddRefs(childElement),
                                                getter_AddRefs(tagName))) &&
             childElement) {
        if ((tagName == nsSchemaAtoms::sAll_atom) ||
            (tagName == nsSchemaAtoms::sChoice_atom) ||
            (tagName == nsSchemaAtoms::sSequence_atom)) {
          iterator.SetElement(childElement);
          break;
        }
      }
    }

    nsSchemaModelGroup* modelGroupInst;

    // If we have a parent sequence and we're a sequence that
    // only appears once, then collapse us.
    if (aParentSequence && 
        (tagName == nsSchemaAtoms::sSequence_atom) &&
        (minOccurs == 1) && (maxOccurs == 1)) {
      modelGroupInst = aParentSequence;
      modelGroup = modelGroupInst;
    }
    else {
      modelGroupInst = new nsSchemaModelGroup(aSchema, name);
      if (!modelGroupInst) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
      modelGroup = modelGroupInst;
      
      modelGroupInst->SetMinOccurs(minOccurs);
      modelGroupInst->SetMaxOccurs(maxOccurs);


      if (tagName == nsSchemaAtoms::sAll_atom) {
        modelGroupInst->SetCompositor(nsISchemaModelGroup::COMPOSITOR_ALL);
      }
      else if (tagName == nsSchemaAtoms::sChoice_atom) {
        modelGroupInst->SetCompositor(nsISchemaModelGroup::COMPOSITOR_CHOICE);
      }
      else if (tagName == nsSchemaAtoms::sSequence_atom) {
        modelGroupInst->SetCompositor(nsISchemaModelGroup::COMPOSITOR_SEQUENCE);
      }
    }
    
    while (NS_SUCCEEDED(iterator.GetNextChild(getter_AddRefs(childElement),
                                              getter_AddRefs(tagName))) &&
           childElement) {
      if (tagName != nsSchemaAtoms::sAnnotation_atom) {
        nsCOMPtr<nsISchemaParticle> particle;

        rv = ProcessParticle(aErrorHandler, aSchema, childElement,
                             tagName, getter_AddRefs(particle));
        if (NS_FAILED(rv)) {
          return rv;
        }

        rv = modelGroupInst->AddParticle(particle);
        if (NS_FAILED(rv)) {
          return rv;
        }
      }
    }   
  }

  *aModelGroup = modelGroup;
  NS_ADDREF(*aModelGroup);

  return NS_OK;
}

nsresult 
nsSchemaLoader::ProcessParticle(nsIWebServiceErrorHandler* aErrorHandler,
                                nsSchema* aSchema, 
                                nsIDOMElement* aElement,
                                nsIAtom* aTagName,
                                nsISchemaParticle** aParticle)
{
  nsresult rv;

  if (aTagName == nsSchemaAtoms::sElement_atom) {
    nsCOMPtr<nsISchemaElement> element;

    rv = ProcessElement(aErrorHandler, aSchema, aElement, getter_AddRefs(element));
    if (NS_FAILED(rv)) {
      return rv;
    }
    *aParticle = element;
    NS_IF_ADDREF(*aParticle);
  }
  else if ((aTagName == nsSchemaAtoms::sModelGroup_atom) ||
           (aTagName == nsSchemaAtoms::sChoice_atom) ||
           (aTagName == nsSchemaAtoms::sSequence_atom)) {
    nsCOMPtr<nsISchemaModelGroup> modelGroup;
    
    rv = ProcessModelGroup(aErrorHandler, aSchema, aElement, 
                           aTagName, nsnull, getter_AddRefs(modelGroup));
    if (NS_FAILED(rv)) {
      return rv;
    }
    *aParticle = modelGroup;
    NS_IF_ADDREF(*aParticle);
  }
  else if (aTagName == nsSchemaAtoms::sAny_atom) {

    nsCOMPtr<nsISchemaParticle> anyParticle;
    nsSchemaAnyParticle* anyParticleInst = new nsSchemaAnyParticle(aSchema);
    if (!anyParticleInst) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    anyParticle = anyParticleInst;

    PRUint32 minOccurs, maxOccurs;
    GetMinAndMax(aElement, &minOccurs, &maxOccurs);
    anyParticleInst->SetMinOccurs(minOccurs);
    anyParticleInst->SetMaxOccurs(maxOccurs);

    PRUint16 process;
    GetProcess(aElement, &process);
    anyParticleInst->SetProcess(process);

    nsAutoString namespaceStr;
    aElement->GetAttribute(NS_LITERAL_STRING("namespace"), namespaceStr);
    anyParticleInst->SetNamespace(namespaceStr);

    *aParticle = anyParticle;
    NS_ADDREF(*aParticle);
  
  }

  return NS_OK;
}

nsresult 
nsSchemaLoader::ProcessAttributeComponent(nsIWebServiceErrorHandler* aErrorHandler,
                                          nsSchema* aSchema, 
                                          nsIDOMElement* aElement,
                                          nsIAtom* aTagName,
                                          nsISchemaAttributeComponent** aAttribute)
{
  nsresult rv;

  if (aTagName == nsSchemaAtoms::sAttribute_atom) {
    nsCOMPtr<nsISchemaAttribute> attribute;

    rv = ProcessAttribute(aErrorHandler, aSchema, aElement, 
                          getter_AddRefs(attribute));
    if (NS_FAILED(rv)) {
      return rv;
    }
    *aAttribute = attribute;
    NS_IF_ADDREF(*aAttribute);
  }
  else if (aTagName == nsSchemaAtoms::sAttributeGroup_atom) {
    nsCOMPtr<nsISchemaAttributeGroup> attributeGroup;

    rv = ProcessAttributeGroup(aErrorHandler, aSchema, aElement, 
                               getter_AddRefs(attributeGroup));
    if (NS_FAILED(rv)) {
      return rv;
    }
    *aAttribute = attributeGroup;
    NS_IF_ADDREF(*aAttribute);
  }
  else if (aTagName == nsSchemaAtoms::sAnyAttribute_atom) {
    nsCOMPtr<nsISchemaAttributeComponent> anyAttribute;
    nsSchemaAnyAttribute* anyAttributeInst = new nsSchemaAnyAttribute(aSchema);
    if (!anyAttributeInst) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    anyAttribute = anyAttributeInst;

    PRUint16 process;
    GetProcess(aElement, &process);
    anyAttributeInst->SetProcess(process);

    nsAutoString namespaceStr;
    aElement->GetAttribute(NS_LITERAL_STRING("namespace"), namespaceStr);
    anyAttributeInst->SetNamespace(namespaceStr);
    
    *aAttribute = anyAttribute;
    NS_ADDREF(*aAttribute);

  }

  return NS_OK;
}

nsresult 
nsSchemaLoader::ProcessAttribute(nsIWebServiceErrorHandler* aErrorHandler,
                                 nsSchema* aSchema, 
                                 nsIDOMElement* aElement,
                                 nsISchemaAttribute** aAttribute)
{
  nsresult rv;

  nsCOMPtr<nsISchemaAttribute> attribute;

  nsAutoString defaultValue, fixedValue;
  aElement->GetAttribute(NS_LITERAL_STRING("default"), defaultValue);
  aElement->GetAttribute(NS_LITERAL_STRING("fixed"), fixedValue);

  PRUint16 use;
  GetUse(aElement, &use);

  nsAutoString ref;
  aElement->GetAttribute(NS_LITERAL_STRING("ref"), ref);
  if (!ref.IsEmpty()) {
    nsSchemaAttributeRef* attributeRef = new nsSchemaAttributeRef(aSchema,
                                                                  ref);
    if (!attributeRef) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    attribute = attributeRef;

    attributeRef->SetConstraints(defaultValue, fixedValue);
    attributeRef->SetUse(use);
  }
  else {
    nsAutoString name;
    aElement->GetAttribute(NS_LITERAL_STRING("name"), name);
    
    nsSchemaAttribute* attributeInst = new nsSchemaAttribute(aSchema,
                                                             name);
    if (!attributeInst) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    attribute = attributeInst;

    attributeInst->SetConstraints(defaultValue, fixedValue);
    attributeInst->SetUse(use);

    nsCOMPtr<nsISchemaSimpleType> simpleType;

    nsChildElementIterator iterator(aElement, 
                                    kSchemaNamespaces, 
                                    kSchemaNamespacesLength);
    nsCOMPtr<nsIDOMElement> childElement;
    nsCOMPtr<nsIAtom> tagName;

    while (NS_SUCCEEDED(iterator.GetNextChild(getter_AddRefs(childElement),
                                              getter_AddRefs(tagName))) &&
           childElement) {
      if (tagName == nsSchemaAtoms::sSimpleType_atom) {
        rv = ProcessSimpleType(aErrorHandler, aSchema, childElement,
                               getter_AddRefs(simpleType));
        if (NS_FAILED(rv)) {
          return rv;
        }
        break;
      }
    }

    if (!simpleType) {
      nsAutoString typeStr;
      aElement->GetAttribute(NS_LITERAL_STRING("type"), typeStr);

      if (!typeStr.IsEmpty()) {
        nsCOMPtr<nsISchemaType> schemaType;
        rv = GetNewOrUsedType(aSchema, aElement, typeStr, 
                              getter_AddRefs(schemaType));
        if (NS_FAILED(rv)) {
          nsAutoString errorMsg;
          errorMsg.AppendLiteral("Failure processing schema, unknown type \"");
          errorMsg.Append(typeStr);
          errorMsg.AppendLiteral("\"");

          NS_SCHEMALOADER_FIRE_ERROR(rv, errorMsg);

          return rv;
        }

        simpleType = do_QueryInterface(schemaType);
        if (!simpleType) {
          nsAutoString errorMsg;
          errorMsg.AppendLiteral("Failure processing schema, invalid type \"");
          errorMsg.Append(typeStr);
          errorMsg.AppendLiteral("\" for attribute \"");
          errorMsg.Append(name);
          errorMsg.AppendLiteral("\"");

          NS_SCHEMALOADER_FIRE_ERROR(NS_ERROR_SCHEMA_INVALID_TYPE_USAGE, errorMsg);

          return NS_ERROR_SCHEMA_INVALID_TYPE_USAGE;
        }
      }
    }

    attributeInst->SetType(simpleType);
  }

  *aAttribute = attribute;
  NS_ADDREF(*aAttribute);

  return NS_OK;
}

nsresult 
nsSchemaLoader::ProcessAttributeGroup(nsIWebServiceErrorHandler* aErrorHandler,
                                      nsSchema* aSchema, 
                                      nsIDOMElement* aElement,
                                      nsISchemaAttributeGroup** aAttributeGroup)
{
  nsresult rv;

  nsCOMPtr<nsISchemaAttributeGroup> attributeGroup;

  nsAutoString ref;
  aElement->GetAttribute(NS_LITERAL_STRING("ref"), ref);

  if (!ref.IsEmpty()) {
    nsSchemaAttributeGroupRef* attrRef = new nsSchemaAttributeGroupRef(aSchema,
                                                                       ref);
    if (!attrRef) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    attributeGroup = attrRef;
  }
  else {
    nsAutoString name;
    aElement->GetAttribute(NS_LITERAL_STRING("name"), name);
    
    nsSchemaAttributeGroup* attrInst = new nsSchemaAttributeGroup(aSchema,
                                                                  name);
    if (!attrInst) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    attributeGroup = attrInst;

    rv = attrInst->Init();
    NS_ENSURE_SUCCESS(rv, rv);

    nsChildElementIterator iterator(aElement, 
                                    kSchemaNamespaces, 
                                    kSchemaNamespacesLength);
    nsCOMPtr<nsIDOMElement> childElement;
    nsCOMPtr<nsIAtom> tagName;

    while (NS_SUCCEEDED(iterator.GetNextChild(getter_AddRefs(childElement),
                                              getter_AddRefs(tagName))) &&
           childElement) {
      if ((tagName == nsSchemaAtoms::sAttribute_atom) ||
          (tagName == nsSchemaAtoms::sAttributeGroup_atom) ||
          (tagName == nsSchemaAtoms::sAnyAttribute_atom)) {
        nsCOMPtr<nsISchemaAttributeComponent> attribute;
        
        rv = ProcessAttributeComponent(aErrorHandler, aSchema, 
                                       childElement, tagName,
                                       getter_AddRefs(attribute));
        if (NS_FAILED(rv)) {
          return rv;
        }
        
        rv = attrInst->AddAttribute(attribute);
        if (NS_FAILED(rv)) {
          return rv;
        }
      }    
    }
  }

  *aAttributeGroup = attributeGroup;
  NS_ADDREF(*aAttributeGroup);

  return NS_OK;
}
 
nsresult 
nsSchemaLoader::ProcessFacet(nsIWebServiceErrorHandler* aErrorHandler,
                             nsSchema* aSchema, 
                             nsIDOMElement* aElement,
                             nsIAtom* aTagName,
                             nsISchemaFacet** aFacet)
{
  PRInt32 rv;

  nsCOMPtr<nsISchemaFacet> facet;
  nsSchemaFacet* facetInst = new nsSchemaFacet(aSchema);
  if (!facetInst) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  facet = facetInst;

  PRUint16 facetType;
  if (aTagName == nsSchemaAtoms::sLength_atom) {
    facetType = nsISchemaFacet::FACET_TYPE_LENGTH;
  }
  else if (aTagName == nsSchemaAtoms::sMinLength_atom) {
    facetType = nsISchemaFacet::FACET_TYPE_MINLENGTH;
  }
  else if (aTagName == nsSchemaAtoms::sMaxLength_atom) {
    facetType = nsISchemaFacet::FACET_TYPE_MAXLENGTH;
  }
  else if (aTagName == nsSchemaAtoms::sPattern_atom) {
    facetType = nsISchemaFacet::FACET_TYPE_PATTERN;
  }
  else if (aTagName == nsSchemaAtoms::sEnumeration_atom) {
    facetType = nsISchemaFacet::FACET_TYPE_ENUMERATION;
  }
  else if (aTagName == nsSchemaAtoms::sWhiteSpace_atom) {
    facetType = nsISchemaFacet::FACET_TYPE_WHITESPACE;
  }
  else if (aTagName == nsSchemaAtoms::sMaxInclusive_atom) {
    facetType = nsISchemaFacet::FACET_TYPE_MAXINCLUSIVE;
  }
  else if (aTagName == nsSchemaAtoms::sMinInclusive_atom) {
    facetType = nsISchemaFacet::FACET_TYPE_MININCLUSIVE;
  }
  else if (aTagName == nsSchemaAtoms::sMaxExclusive_atom) {
    facetType = nsISchemaFacet::FACET_TYPE_MAXEXCLUSIVE;
  }
  else if (aTagName == nsSchemaAtoms::sMinExclusive_atom) {
    facetType = nsISchemaFacet::FACET_TYPE_MINEXCLUSIVE;
  }
  else if (aTagName == nsSchemaAtoms::sTotalDigits_atom) {
    facetType = nsISchemaFacet::FACET_TYPE_TOTALDIGITS;
  }
  else if (aTagName == nsSchemaAtoms::sFractionDigits_atom) {
    facetType = nsISchemaFacet::FACET_TYPE_FRACTIONDIGITS;
  }
  else {
    nsAutoString elementName;
    rv = aElement->GetTagName(elementName);
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoString errorMsg;
    errorMsg.AppendLiteral("Failure processing schema, unknown type of facet \"");
    errorMsg.Append(elementName);
    errorMsg.AppendLiteral("\"");

    NS_SCHEMALOADER_FIRE_ERROR(NS_ERROR_UNEXPECTED, errorMsg);

    return NS_ERROR_UNEXPECTED;
  }
  facetInst->SetFacetType(facetType);

  nsAutoString valueStr;
  aElement->GetAttribute(NS_LITERAL_STRING("value"), valueStr);
  if (valueStr.IsEmpty()) {
    nsAutoString elementName;
    rv = aElement->GetTagName(elementName);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = NS_ERROR_SCHEMA_FACET_VALUE_ERROR;

    nsAutoString errorMsg; 
    errorMsg.AppendLiteral("Failure processing schema, invalid empty value ");
    errorMsg.AppendLiteral("for facet \"");
    errorMsg.Append(elementName);
    errorMsg.AppendLiteral("\"");

    NS_SCHEMALOADER_FIRE_ERROR(rv, errorMsg);

    return rv;
  }

  if ((aTagName == nsSchemaAtoms::sLength_atom) ||
      (aTagName == nsSchemaAtoms::sMinLength_atom) ||
      (aTagName == nsSchemaAtoms::sMaxLength_atom) ||
      (aTagName == nsSchemaAtoms::sTotalDigits_atom) ||
      (aTagName == nsSchemaAtoms::sFractionDigits_atom)) {
    PRInt32 intVal = valueStr.ToInteger(&rv);

    if (NS_FAILED(rv) ||
        (intVal < 0) ||
        ((aTagName == nsSchemaAtoms::sTotalDigits_atom) && (intVal == 0))) {
      nsAutoString elementName;
      rv = aElement->GetTagName(elementName);
      NS_ENSURE_SUCCESS(rv, rv);

      nsAutoString errorMsg; 
      errorMsg.AppendLiteral("Failure processing schema, invalid value for facet \"");
      errorMsg.Append(elementName);
      errorMsg.AppendLiteral("\", <=0");

      NS_SCHEMALOADER_FIRE_ERROR(NS_ERROR_SCHEMA_FACET_VALUE_ERROR, errorMsg);

      return NS_ERROR_SCHEMA_FACET_VALUE_ERROR;
    }

    facetInst->SetUintValue((PRUint32)intVal);
  }
  else if (aTagName == nsSchemaAtoms::sWhiteSpace_atom) {
    PRUint16 whiteSpaceVal;
    if (valueStr.EqualsLiteral("collapse")) {
      whiteSpaceVal = nsSchemaFacet::WHITESPACE_COLLAPSE;
    }
    else if (valueStr.EqualsLiteral("preserve")) {
      whiteSpaceVal = nsSchemaFacet::WHITESPACE_PRESERVE;
    }
    else if (valueStr.EqualsLiteral("replace")) {
      whiteSpaceVal = nsSchemaFacet::WHITESPACE_REPLACE;
    }
    else {
      nsAutoString elementName;
      rv = aElement->GetTagName(elementName);
      NS_ENSURE_SUCCESS(rv, rv);

      nsAutoString errorMsg; 
      errorMsg.AppendLiteral("Failure processing schema, invalid value for facet \"");
      errorMsg.Append(elementName);
      errorMsg.AppendLiteral("\", should be \"collapse\", \"preserve\" or \"replace\"");

      NS_SCHEMALOADER_FIRE_ERROR(NS_ERROR_SCHEMA_FACET_VALUE_ERROR, errorMsg);

      return NS_ERROR_SCHEMA_FACET_VALUE_ERROR;
    }

    facetInst->SetWhitespaceValue(whiteSpaceVal);
  }
  else {
    facetInst->SetValue(valueStr);
  }
  
  nsAutoString isFixed;
  aElement->GetAttribute(NS_LITERAL_STRING("fixed"), isFixed);
  facetInst->SetIsFixed(isFixed.EqualsLiteral("true"));

  *aFacet = facet;
  NS_ADDREF(*aFacet);

  return NS_OK;
}

void
nsSchemaLoader::GetUse(nsIDOMElement* aElement, 
                       PRUint16* aUse)
{
  *aUse = nsISchemaAttribute::USE_OPTIONAL;

  nsAutoString use;
  aElement->GetAttribute(NS_LITERAL_STRING("use"), use);
  
  if (use.EqualsLiteral("prohibited")) {
    *aUse = nsISchemaAttribute::USE_PROHIBITED;
  }
  else if (use.EqualsLiteral("required")) {
    *aUse = nsISchemaAttribute::USE_REQUIRED;
  }
}

void
nsSchemaLoader::GetProcess(nsIDOMElement* aElement, 
                           PRUint16* aProcess)
{
  *aProcess = nsISchemaAnyParticle::PROCESS_STRICT;

  nsAutoString process;
  aElement->GetAttribute(NS_LITERAL_STRING("process"), process);

  if (process.EqualsLiteral("lax")) {
    *aProcess = nsISchemaAnyParticle::PROCESS_LAX;
  }
  else if (process.EqualsLiteral("skip")) {
    *aProcess = nsISchemaAnyParticle::PROCESS_SKIP;
  }
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
  if (!minStr.IsEmpty()) {
    PRInt32 minVal = minStr.ToInteger(&rv);
    if (NS_SUCCEEDED(rv) && (minVal >= 0)) {
      *aMinOccurs = (PRUint32)minVal;
    }
  }

  if (!maxStr.IsEmpty()) {
    if (maxStr.EqualsLiteral("unbounded")) {
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

