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

#include "nsIServiceManager.h"
#include "nsMemory.h"
#include "nsIComponentManager.h"
#include "nsSOAPUtils.h"
#include "nsSOAPMessage.h"
#include "nsSOAPParameter.h"
#include "nsSOAPHeaderBlock.h"
#include "nsSOAPException.h"
#include "nsIDOMDocument.h"
#include "nsIDOMAttr.h"
#include "nsIDOMParser.h"
#include "nsIDOMElement.h"
#include "nsIDOMNamedNodeMap.h"

static NS_DEFINE_CID(kDOMParserCID, NS_DOMPARSER_CID);
/////////////////////////////////////////////
//
//
/////////////////////////////////////////////

nsSOAPMessage::nsSOAPMessage()
{
}

nsSOAPMessage::~nsSOAPMessage()
{
}

NS_IMPL_ISUPPORTS1(nsSOAPMessage, nsISOAPMessage)
/* attribute nsIDOMDocument message; */
NS_IMETHODIMP nsSOAPMessage::GetMessage(nsIDOMDocument * *aMessage)
{
  NS_ENSURE_ARG_POINTER(aMessage);
  *aMessage = mMessage;
  NS_IF_ADDREF(*aMessage);
  return NS_OK;
}

NS_IMETHODIMP nsSOAPMessage::SetMessage(nsIDOMDocument * aMessage)
{
  mMessage = aMessage;
  return NS_OK;
}

/* readonly attribute nsIDOMElement envelope; */
NS_IMETHODIMP nsSOAPMessage::GetEnvelope(nsIDOMElement * *aEnvelope)
{
  NS_ENSURE_ARG_POINTER(aEnvelope);

  if (mMessage) {
    nsCOMPtr<nsIDOMElement> root;
    mMessage->GetDocumentElement(getter_AddRefs(root));
    if (root) {
      nsAutoString namespaceURI;
      nsAutoString name;
      nsresult rc = root->GetNamespaceURI(namespaceURI);
      if (NS_FAILED(rc))
        return rc;
      rc = root->GetLocalName(name);
      if (NS_FAILED(rc))
        return rc;
      if (name.Equals(gSOAPStrings->kEnvelopeTagName)
          && (namespaceURI.
              Equals(*gSOAPStrings->kSOAPEnvURI[nsISOAPMessage::VERSION_1_2])
              || namespaceURI.
              Equals(*gSOAPStrings->kSOAPEnvURI[nsISOAPMessage::VERSION_1_1]))) {
        *aEnvelope = root;
        NS_ADDREF(*aEnvelope);
        return NS_OK;
      }
    }
  }
  *aEnvelope = nsnull;
  return NS_OK;
}

/* readonly attribute PRUint16 version; */
NS_IMETHODIMP nsSOAPMessage::GetVersion(PRUint16 * aVersion)
{
  NS_ENSURE_ARG_POINTER(aVersion);
  if (mMessage) {
    nsCOMPtr<nsIDOMElement> root;
    mMessage->GetDocumentElement(getter_AddRefs(root));
    if (root) {
      nsAutoString namespaceURI;
      nsAutoString name;
      nsresult rc = root->GetNamespaceURI(namespaceURI);
      if (NS_FAILED(rc))
        return rc;
      rc = root->GetLocalName(name);
      if (NS_FAILED(rc))
        return rc;
      if (name.Equals(gSOAPStrings->kEnvelopeTagName)) {
        if (namespaceURI.
            Equals(*gSOAPStrings->kSOAPEnvURI[nsISOAPMessage::VERSION_1_2])) {
          *aVersion = nsISOAPMessage::VERSION_1_2;
          return NS_OK;
        } else if (namespaceURI.
                   Equals(*gSOAPStrings->kSOAPEnvURI[nsISOAPMessage::VERSION_1_1])) {
          *aVersion = nsISOAPMessage::VERSION_1_1;
          return NS_OK;
        }
      }
    }
  }
  *aVersion = nsISOAPMessage::VERSION_UNKNOWN;
  return NS_OK;
}

/* Internal method for getting  envelope and  version */
PRUint16 nsSOAPMessage::GetEnvelopeWithVersion(nsIDOMElement * *aEnvelope)
{
  if (mMessage) {
    nsCOMPtr<nsIDOMElement> root;
    mMessage->GetDocumentElement(getter_AddRefs(root));
    if (root) {
      nsAutoString namespaceURI;
      nsAutoString name;
      root->GetNamespaceURI(namespaceURI);
      root->GetLocalName(name);
      if (name.Equals(gSOAPStrings->kEnvelopeTagName)) {
        if (namespaceURI.
            Equals(*gSOAPStrings->kSOAPEnvURI[nsISOAPMessage::VERSION_1_2])) {
          *aEnvelope = root;
          NS_ADDREF(*aEnvelope);
          return nsISOAPMessage::VERSION_1_2;
        } else if (namespaceURI.
                   Equals(*gSOAPStrings->kSOAPEnvURI[nsISOAPMessage::VERSION_1_1])) {
          *aEnvelope = root;
          NS_ADDREF(*aEnvelope);
          return nsISOAPMessage::VERSION_1_1;
        }
      }
    }
  }
  *aEnvelope = nsnull;
  return nsISOAPMessage::VERSION_UNKNOWN;
}

/* readonly attribute nsIDOMElement header; */
NS_IMETHODIMP nsSOAPMessage::GetHeader(nsIDOMElement * *aHeader)
{
  NS_ENSURE_ARG_POINTER(aHeader);
  nsCOMPtr<nsIDOMElement> env;
  PRUint16 version = GetEnvelopeWithVersion(getter_AddRefs(env));
  if (env) {
    nsSOAPUtils::GetSpecificChildElement(nsnull, env,
                                         *gSOAPStrings->kSOAPEnvURI[version],
                                         gSOAPStrings->kHeaderTagName,
                                         aHeader);
  } else {
    *aHeader = nsnull;
  }
  return NS_OK;
}

/* readonly attribute nsIDOMElement body; */
NS_IMETHODIMP nsSOAPMessage::GetBody(nsIDOMElement * *aBody)
{
  NS_ENSURE_ARG_POINTER(aBody);
  nsCOMPtr<nsIDOMElement> env;
  PRUint16 version = GetEnvelopeWithVersion(getter_AddRefs(env));
  if (env) {
    nsSOAPUtils::GetSpecificChildElement(nsnull, env,
                                         *gSOAPStrings->kSOAPEnvURI[version],
                                         gSOAPStrings->kBodyTagName, aBody);
  } else {
    *aBody = nsnull;
  }
  return NS_OK;
}

/* attribute DOMString actionURI; */
NS_IMETHODIMP nsSOAPMessage::GetActionURI(nsAString & aActionURI)
{
  aActionURI.Assign(mActionURI);
  return NS_OK;
}

NS_IMETHODIMP nsSOAPMessage::SetActionURI(const nsAString & aActionURI)
{
  mActionURI.Assign(aActionURI);
  return NS_OK;
}

/* readonly attribute AString methodName; */
NS_IMETHODIMP nsSOAPMessage::GetMethodName(nsAString & aMethodName)
{
  nsCOMPtr<nsIDOMElement> body;
  GetBody(getter_AddRefs(body));
  if (body) {
    nsCOMPtr<nsIDOMElement> method;
    nsSOAPUtils::GetFirstChildElement(body, getter_AddRefs(method));
    if (method) {
      body->GetLocalName(aMethodName);
      return NS_OK;
    }
  }
  aMethodName.Truncate();
  return NS_OK;
}

/* readonly attribute AString targetObjectURI; */
NS_IMETHODIMP nsSOAPMessage::
GetTargetObjectURI(nsAString & aTargetObjectURI)
{
  nsCOMPtr<nsIDOMElement> body;
  GetBody(getter_AddRefs(body));
  if (body) {
    nsCOMPtr<nsIDOMElement> method;
    nsSOAPUtils::GetFirstChildElement(body, getter_AddRefs(method));
    if (method) {
      nsCOMPtr<nsISOAPEncoding> encoding;
      PRUint16 version;
      nsresult rv = GetEncodingWithVersion(method, &version, getter_AddRefs(encoding));
      if (NS_FAILED(rv))
        return rv;
      nsAutoString temp;
      rv = method->GetNamespaceURI(temp);
      if (NS_FAILED(rv))
        return rv;
      return encoding->GetInternalSchemaURI(temp, aTargetObjectURI);
    }
  }
  aTargetObjectURI.Truncate();
  return NS_OK;
}

NS_IMETHODIMP
    nsSOAPMessage::Encode(PRUint16 aVersion, const nsAString & aMethodName,
                          const nsAString & aTargetObjectURI,
                          PRUint32 aHeaderBlockCount,
                          nsISOAPHeaderBlock ** aHeaderBlocks,
                          PRUint32 aParameterCount,
                          nsISOAPParameter ** aParameters)
{
  static NS_NAMED_LITERAL_STRING(realEmptySOAPDocStr1,
                        "<env:Envelope xmlns:env=\"http://schemas.xmlsoap.org/soap/envelope/\" xmlns:enc=\"http://schemas.xmlsoap.org/soap/encoding/\"><env:Header/><env:Body/></env:Envelope>");
  static NS_NAMED_LITERAL_STRING(realEmptySOAPDocStr2,
                        "<env:Envelope xmlns:env=\"http://www.w3.org/2001/09/soap-envelope\" xmlns:enc=\"http://www.w3.org/2001/09/soap-encoding\"><env:Header/><env:Body/></env:Envelope>");
  static const nsAString *kEmptySOAPDocStr[] = {
    &realEmptySOAPDocStr1, &realEmptySOAPDocStr2
  };

  if (aVersion != nsISOAPMessage::VERSION_1_1
      && aVersion != nsISOAPMessage::VERSION_1_2)
    return SOAP_EXCEPTION(NS_ERROR_ILLEGAL_VALUE,"SOAP_BAD_VALUE","Cannot encode message blocks without a valid SOAP version specified.");

//  Construct the message skeleton

  nsresult rv;
  nsCOMPtr<nsIDOMNode> ignored;
  nsCOMPtr<nsIDOMParser> parser = do_CreateInstance(kDOMParserCID, &rv);
  if (NS_FAILED(rv))
    return rv;

  rv = parser->ParseFromString(nsString(*kEmptySOAPDocStr[aVersion]).get(),
                               NS_LITERAL_CSTRING("text/xml"), getter_AddRefs(mMessage));
  if (NS_FAILED(rv))
    return rv;

//  Declare the default encoding.  This should always be non-null, but may be empty string.

  nsCOMPtr<nsISOAPEncoding> encoding;
  rv = GetEncoding(getter_AddRefs(encoding));
  if (NS_FAILED(rv))
    return rv;
  nsCOMPtr<nsIDOMElement> envelope;
  rv = GetEnvelope(getter_AddRefs(envelope));
  if (NS_FAILED(rv))
    return rv;
  if (envelope) {
    nsAutoString enc;
    rv = mEncoding->GetStyleURI(enc);
    if (NS_FAILED(rv))
      return rv;
    if (!enc.IsEmpty()) {
      rv = envelope->SetAttributeNS(*gSOAPStrings->kSOAPEnvURI[aVersion],
                                    gSOAPStrings->kEncodingStyleAttribute, enc);
        if (NS_FAILED(rv))
          return rv;
    }
  }
//  Declare the schema namespaces, taking into account any mappings that are present.

  nsAutoString temp;
  nsAutoString temp2;
  temp.Assign(gSOAPStrings->kXMLNamespacePrefix);
  temp.Append(gSOAPStrings->kXSPrefix);
  rv = encoding->GetExternalSchemaURI(gSOAPStrings->kXSURI, temp2);
  if (NS_FAILED(rv))
    return rv;
  rv = envelope->SetAttributeNS(gSOAPStrings->kXMLNamespaceNamespaceURI, temp, temp2);
  if (NS_FAILED(rv))
    return rv;
  temp.Assign(gSOAPStrings->kXMLNamespacePrefix);
  temp.Append(gSOAPStrings->kXSIPrefix);
  rv = encoding->GetExternalSchemaURI(gSOAPStrings->kXSIURI, temp2);
  if (NS_FAILED(rv))
    return rv;
  rv = envelope->SetAttributeNS(gSOAPStrings->kXMLNamespaceNamespaceURI, temp, temp2);
  if (NS_FAILED(rv))
    return rv;

//  Encode and add headers, if any were specified 

  if (aHeaderBlockCount) {
    nsCOMPtr<nsIDOMElement> parent;
    rv = GetHeader(getter_AddRefs(parent));
    if (NS_FAILED(rv))
      return rv;
    nsCOMPtr<nsISOAPHeaderBlock> header;
    nsCOMPtr<nsIDOMElement> element;
    nsAutoString name;
    nsAutoString namespaceURI;
    PRUint32 i;
    for (i = 0; i < aHeaderBlockCount; i++) {
      header = aHeaderBlocks[i];
      if (!header)
        return SOAP_EXCEPTION(NS_ERROR_ILLEGAL_VALUE,"SOAP_NULL_HEADER","Cannot encode null in header array.");
      rv = header->GetElement(getter_AddRefs(element));
      if (element) {
        nsCOMPtr<nsIDOMNode> node1;
        node1 = element;
        nsCOMPtr<nsIDOMNode> node2;
        rv = mMessage->ImportNode(node1, PR_TRUE, getter_AddRefs(node1));
        if (NS_FAILED(rv))
          return rv;
        rv = parent->AppendChild(node2, getter_AddRefs(node1));
        if (NS_FAILED(rv))
          return rv;
        element = do_QueryInterface(node1);
      } else {
        rv = header->GetNamespaceURI(namespaceURI);
        if (NS_FAILED(rv))
          return rv;
        rv = header->GetName(name);
        if (NS_FAILED(rv))
          return rv;
        nsAutoString actorURI;
        rv = header->GetActorURI(actorURI);
        if (NS_FAILED(rv))
          return rv;
        PRBool mustUnderstand;
        rv = header->GetMustUnderstand(&mustUnderstand);
        if (NS_FAILED(rv))
          return rv;
        rv = header->GetEncoding(getter_AddRefs(encoding));
        if (NS_FAILED(rv))
          return rv;
        if (!encoding) {
          rv = GetEncoding(getter_AddRefs(encoding));
          if (NS_FAILED(rv))
            return rv;
        }
        nsCOMPtr<nsISchemaType> schemaType;
        rv = header->GetSchemaType(getter_AddRefs(schemaType));
        if (NS_FAILED(rv))
          return rv;
        nsCOMPtr<nsIVariant> value;
        rv = header->GetValue(getter_AddRefs(value));
        if (NS_FAILED(rv))
          return rv;
        rv = encoding->Encode(value, namespaceURI, name,
                              schemaType, nsnull, parent,
                              getter_AddRefs(element));
        if (NS_FAILED(rv))
          return rv;
        if (!actorURI.IsEmpty()) {
          element->SetAttributeNS(gSOAPStrings->kSOAPEnvPrefix,
                                  gSOAPStrings->kActorAttribute, actorURI);
          if (NS_FAILED(rv))
            return rv;
        }
        if (mustUnderstand) {
          element->SetAttributeNS(gSOAPStrings->kSOAPEnvPrefix,
                                  gSOAPStrings->kMustUnderstandAttribute,
                                  gSOAPStrings->kTrueA);
          if (NS_FAILED(rv))
            return rv;
        }
        if (mEncoding != encoding) {
          nsAutoString enc;
          encoding->GetStyleURI(enc);
          element->
              SetAttributeNS(*gSOAPStrings->kSOAPEnvURI[aVersion],
                             gSOAPStrings->kEncodingStyleAttribute, enc);
        }
      }
    }
  }
  nsCOMPtr<nsIDOMElement> body;
  rv = GetBody(getter_AddRefs(body));
  if (NS_FAILED(rv))
    return rv;

//  Only produce a call element if mMethodName was non-empty

  if (!aMethodName.IsEmpty()) {
    nsAutoString temp;
    rv = encoding->GetExternalSchemaURI(aTargetObjectURI, temp);
    nsCOMPtr<nsIDOMElement> call;
    rv = mMessage->CreateElementNS(temp, aMethodName,
                                   getter_AddRefs(call));
    if (NS_FAILED(rv))
      return rv;
    nsCOMPtr<nsIDOMNode> ignored;
    rv = body->AppendChild(call, getter_AddRefs(ignored));
    if (NS_FAILED(rv))
      return rv;
    body = call;
  }
//  Encode and add all of the parameters into the body

  nsCOMPtr<nsISOAPParameter> param;
  nsCOMPtr<nsIDOMElement> element;
  nsCOMPtr<nsISOAPEncoding> newencoding;
  nsAutoString name;
  nsAutoString namespaceURI;
  PRUint32 i;
  for (i = 0; i < aParameterCount; i++) {
    param = aParameters[i];
    if (!param)
      return SOAP_EXCEPTION(NS_ERROR_ILLEGAL_VALUE,"SOAP_NULL_PARAMETER","Cannot encode null in parameter array.");
    rv = param->GetElement(getter_AddRefs(element));
    if (element) {
      nsCOMPtr<nsIDOMNode> node1;
      node1 = element;
      nsCOMPtr<nsIDOMNode> node2;
      rv = mMessage->ImportNode(node1, PR_TRUE, getter_AddRefs(node1));
      if (NS_FAILED(rv))
        return rv;
      rv = body->AppendChild(node2, getter_AddRefs(node1));
      if (NS_FAILED(rv))
        return rv;
      element = do_QueryInterface(node1);
    } else {
      rv = param->GetNamespaceURI(namespaceURI);
      if (NS_FAILED(rv))
        return rv;
      rv = param->GetName(name);
      if (NS_FAILED(rv))
        return rv;
      rv = param->GetEncoding(getter_AddRefs(newencoding));
      if (NS_FAILED(rv))
        return rv;
      if (!newencoding) {
        newencoding = encoding;
      }
      nsCOMPtr<nsISchemaType> schemaType;
      rv = param->GetSchemaType(getter_AddRefs(schemaType));
      if (NS_FAILED(rv))
        return rv;
      nsCOMPtr<nsIVariant> value;
      rv = param->GetValue(getter_AddRefs(value));
      if (NS_FAILED(rv))
        return rv;
      rv = newencoding->Encode(value, namespaceURI, name,
                               schemaType, nsnull, body,
                               getter_AddRefs(element));
      if (NS_FAILED(rv))
        return rv;
      if (encoding != newencoding) {
        nsAutoString enc;
        newencoding->GetStyleURI(enc);
        element->SetAttributeNS(*gSOAPStrings->kSOAPEnvURI[aVersion],
                                gSOAPStrings->kEncodingStyleAttribute, enc);
      }
    }
  }
  return NS_OK;
}

/**
 * Internally used to track down the encoding to be used at the headers
 * or parameters.   We know the version is legal, or we couldn't have
 * found a starting point, so it is used but not checked again.  We
 * also know that since there is a version, there is an encoding.
 */
nsresult
    nsSOAPMessage::GetEncodingWithVersion(nsIDOMElement * aFirst,
                                          PRUint16 * aVersion,
                                          nsISOAPEncoding ** aEncoding)
{
  nsCOMPtr<nsISOAPEncoding> encoding;
  nsresult rv = GetEncoding(getter_AddRefs(encoding));
  if (NS_FAILED(rv))
    return rv;
  rv = GetVersion(aVersion);
  if (NS_FAILED(rv))
    return rv;
  nsCOMPtr<nsIDOMElement> element = aFirst;

  // Check for stray encodingStyle attributes.  If none found, then
  // use empty string encoding style.

  nsAutoString style;
  for (;;) {
    nsCOMPtr<nsIDOMAttr> enc;
    rv = element->GetAttributeNodeNS(*gSOAPStrings->kSOAPEnvURI[*aVersion],
                                     gSOAPStrings->kEncodingStyleAttribute,
                                     getter_AddRefs(enc));
    if (NS_FAILED(rv))
      return rv;
    if (enc) {
      rv = enc->GetNodeValue(style);
      if (NS_FAILED(rv))
        return rv;
      break;
    } else {
      nsCOMPtr<nsIDOMNode> next;
      rv = element->GetParentNode(getter_AddRefs(next));
      if (NS_FAILED(rv))
        return rv;
      if (next) {
        PRUint16 type;
        rv = next->GetNodeType(&type);
        if (NS_FAILED(rv))
          return rv;
        if (type != nsIDOMNode::ELEMENT_NODE) {
          next = nsnull;
        }
      }
      if (next) {
        element = do_QueryInterface(next);
      } else {
        break;
      }
    }
  }
  return encoding->GetAssociatedEncoding(style, PR_TRUE, aEncoding);
}

/* void getHeaderBlocks (out PRUint32 aCount, [array, size_is (aCount), retval] out nsISOAPHeaderBlock aHeaderBlocks); */
NS_IMETHODIMP
    nsSOAPMessage::GetHeaderBlocks(PRUint32 * aCount,
                                   nsISOAPHeaderBlock *** aHeaderBlocks)
{
  NS_ENSURE_ARG_POINTER(aHeaderBlocks);
  nsISOAPHeaderBlock** headerBlocks = nsnull;
  *aCount = 0;
  *aHeaderBlocks = nsnull;
  int count = 0;
  int length = 0;

  nsCOMPtr<nsIDOMElement> element;
  nsresult rv = GetHeader(getter_AddRefs(element));
  if (NS_FAILED(rv) || !element)
    return rv;
  nsCOMPtr<nsISOAPEncoding> encoding;
  PRUint16 version;
  rv = GetEncodingWithVersion(element, &version, getter_AddRefs(encoding));
  if (NS_FAILED(rv))
    return rv;
  nsCOMPtr<nsIDOMElement> next;

  nsCOMPtr<nsISOAPHeaderBlock> header;
  nsSOAPUtils::GetFirstChildElement(element, getter_AddRefs(next));
  while (next) {
    if (length == count) {
      length = length ? 2 * length : 10;
      headerBlocks =
          (nsISOAPHeaderBlock * *)nsMemory::Realloc(headerBlocks,
                                                  length *
                                                  sizeof(*headerBlocks));
    }
    element = next;
    header = do_CreateInstance(NS_SOAPHEADERBLOCK_CONTRACTID);
    if (!header) {
      rv = NS_ERROR_OUT_OF_MEMORY;
      break;
    }
    header->Init(nsnull, version);

    (headerBlocks)[(count)] = header;
    NS_ADDREF((headerBlocks)[(count)]);
    (count)++;

    rv = header->SetElement(element);
    if (NS_FAILED(rv))
      break;
    rv = header->SetEncoding(encoding);
    if (NS_FAILED(rv))
      break;
    nsSOAPUtils::GetNextSiblingElement(element, getter_AddRefs(next));
  }
  if (NS_SUCCEEDED(rv)) {
    if (count) {
      headerBlocks =
          (nsISOAPHeaderBlock * *)nsMemory::Realloc(headerBlocks,
                                                    count *
                                                    sizeof(*headerBlocks));
    }
  }
  else {
    while (--count >= 0) {
      NS_IF_RELEASE(headerBlocks[count]);
    }
    count = 0;
    nsMemory::Free(headerBlocks);
    headerBlocks = nsnull;
  }
  *aCount = count;
  *aHeaderBlocks = headerBlocks;
  return rv;
}

/* void getParameters (in boolean aDocumentStyle, out PRUint32 aCount, [array, size_is (aCount), retval] out nsISOAPParameter aParameters); */
NS_IMETHODIMP
    nsSOAPMessage::GetParameters(PRBool aDocumentStyle, PRUint32 * aCount,
                                 nsISOAPParameter *** aParameters)
{
  NS_ENSURE_ARG_POINTER(aParameters);
  nsISOAPParameter** parameters = nsnull;
  *aCount = 0;
  *aParameters = nsnull;
  int count = 0;
  int length = 0;
  nsCOMPtr<nsIDOMElement> element;
  nsresult rv = GetBody(getter_AddRefs(element));
  if (NS_FAILED(rv) || !element)
    return rv;
  nsCOMPtr<nsIDOMElement> next;
  nsCOMPtr<nsISOAPParameter> param;
  nsSOAPUtils::GetFirstChildElement(element, getter_AddRefs(next));
  if (!aDocumentStyle) {
    element = next;
    if (!element)
      return SOAP_EXCEPTION(NS_ERROR_ILLEGAL_VALUE,"SOAP_MISSING_METHOD","Cannot decode rpc-style message due to missing method element.");
    nsSOAPUtils::GetFirstChildElement(element, getter_AddRefs(next));
  }
  nsCOMPtr<nsISOAPEncoding> encoding;
  PRUint16 version;
  rv = GetEncodingWithVersion(element, &version, getter_AddRefs(encoding));
  if (NS_FAILED(rv))
    return rv;
  while (next) {
    if (length == count) {
      length = length ? 2 * length : 10;
      parameters =
          (nsISOAPParameter * *)nsMemory::Realloc(parameters,
                                                length *
                                                sizeof(*parameters));
    }
    element = next;
    param = do_CreateInstance(NS_SOAPPARAMETER_CONTRACTID);
    if (!param) {
      rv = NS_ERROR_OUT_OF_MEMORY;
      break;
    }
    parameters[count] = param;
    NS_ADDREF(parameters[count]);
    count++;

    rv = param->SetElement(element);
    if (NS_FAILED(rv))
      break;
    rv = param->SetEncoding(encoding);
    if (NS_FAILED(rv))
      break;
    nsSOAPUtils::GetNextSiblingElement(element, getter_AddRefs(next));
  }
  if (NS_SUCCEEDED(rv)) {
    if (count) {
      parameters =
          (nsISOAPParameter * *)nsMemory::Realloc(parameters,
                                                  count *
                                                  sizeof(*parameters));
    }
  }
  else {
    while (--count >= 0) {
      NS_IF_RELEASE(parameters[count]);
    }
    count = 0;
    nsMemory::Free(parameters);
    parameters = nsnull;
  }
  *aCount = count;
  *aParameters = parameters;
  return rv;
}

/* attribute nsISOAPEncoding encoding; */
NS_IMETHODIMP nsSOAPMessage::GetEncoding(nsISOAPEncoding * *aEncoding)
{
  NS_ENSURE_ARG_POINTER(aEncoding);
  if (!mEncoding) {
    PRUint16 version;
    nsresult rc = GetVersion(&version);
    if (NS_FAILED(rc))
      return rc;
    if (version != nsISOAPMessage::VERSION_UNKNOWN) {
      nsCOMPtr<nsISOAPEncoding> encoding =
          do_CreateInstance(NS_SOAPENCODING_CONTRACTID);
      if (!encoding)
        return NS_ERROR_OUT_OF_MEMORY;
      if (version == nsISOAPMessage::VERSION_1_1) {
        rc = encoding->
            GetAssociatedEncoding(gSOAPStrings->kSOAPEncURI11,
                                  PR_FALSE, getter_AddRefs(mEncoding));
      }
      else {
        rc = encoding->
            GetAssociatedEncoding(gSOAPStrings->kSOAPEncURI,
                                  PR_FALSE, getter_AddRefs(mEncoding));
      }
      if (NS_FAILED(rc))
        return rc;
    }
  }
  *aEncoding = mEncoding;
  NS_IF_ADDREF(*aEncoding);
  return NS_OK;
}

NS_IMETHODIMP nsSOAPMessage::SetEncoding(nsISOAPEncoding * aEncoding)
{
  mEncoding = aEncoding;
  return NS_OK;
}
