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

#include "nsSOAPUtils.h"
#include "nsIDOMText.h"
#include "nsISOAPEncoding.h"
#include "nsIDOMNamedNodeMap.h"
#include "nsIDOMAttr.h"
#include "nsCOMPtr.h"
#include "nsSOAPException.h"

nsSOAPStrings::nsSOAPStrings()
  : NS_LITERAL_STRING_INIT(kSOAPEnvURI1, "http://schemas.xmlsoap.org/soap/envelope/")
  , NS_LITERAL_STRING_INIT(kSOAPEnvURI2, "http://www.w3.org/2001/09/soap-envelope")
  , NS_LITERAL_STRING_INIT(kSOAPEncURI, "http://www.w3.org/2001/09/soap-encoding")
  , NS_LITERAL_STRING_INIT(kSOAPEncURI11, "http://schemas.xmlsoap.org/soap/encoding/")
  , NS_LITERAL_STRING_INIT(kXSIURI, "http://www.w3.org/2001/XMLSchema-instance")
  , NS_LITERAL_STRING_INIT(kXSURI, "http://www.w3.org/2001/XMLSchema")
  , NS_LITERAL_STRING_INIT(kXSIURI1999, "http://www.w3.org/1999/XMLSchema-instance")
  , NS_LITERAL_STRING_INIT(kXSURI1999, "http://www.w3.org/1999/XMLSchema")
  , NS_LITERAL_STRING_INIT(kSOAPEnvPrefix, "env")
  , NS_LITERAL_STRING_INIT(kSOAPEncPrefix, "enc")
  , NS_LITERAL_STRING_INIT(kXSIPrefix, "xsi")
  , NS_LITERAL_STRING_INIT(kXSITypeAttribute, "type")
  , NS_LITERAL_STRING_INIT(kXSPrefix, "xs")
  , NS_LITERAL_STRING_INIT(kEncodingStyleAttribute, "encodingStyle")
  , NS_LITERAL_STRING_INIT(kActorAttribute, "actor")
  , NS_LITERAL_STRING_INIT(kMustUnderstandAttribute, "mustUnderstand")
  , NS_LITERAL_STRING_INIT(kEnvelopeTagName, "Envelope")
  , NS_LITERAL_STRING_INIT(kHeaderTagName, "Header")
  , NS_LITERAL_STRING_INIT(kBodyTagName, "Body")
  , NS_LITERAL_STRING_INIT(kFaultTagName, "Fault")
  , NS_LITERAL_STRING_INIT(kFaultCodeTagName, "faultcode")
  , NS_LITERAL_STRING_INIT(kFaultStringTagName, "faultstring")
  , NS_LITERAL_STRING_INIT(kFaultActorTagName, "faultactor")
  , NS_LITERAL_STRING_INIT(kFaultDetailTagName, "detail")
  , NS_LITERAL_STRING_INIT(kEncodingSeparator, "#")
  , NS_LITERAL_STRING_INIT(kQualifiedSeparator, ":")
  , NS_LITERAL_STRING_INIT(kXMLNamespaceNamespaceURI, "http://www.w3.org/2000/xmlns/")
  , NS_LITERAL_STRING_INIT(kXMLNamespaceURI, "http://www.w3.org/XML/1998/namespace")
  , NS_LITERAL_STRING_INIT(kXMLNamespacePrefix, "xmlns:")
  , NS_LITERAL_STRING_INIT(kXMLPrefix, "xml:")
  , NS_LITERAL_STRING_INIT(kTrue, "true")
  , NS_LITERAL_STRING_INIT(kTrueA, "1")
  , NS_LITERAL_STRING_INIT(kFalse, "false")
  , NS_LITERAL_STRING_INIT(kFalseA, "0")
  , NS_LITERAL_STRING_INIT(kVerifySourceHeader, "verifySource")
  , NS_LITERAL_STRING_INIT(kVerifySourceURI, "uri")
  , NS_LITERAL_STRING_INIT(kVerifySourceNamespaceURI, "urn:inet:www.mozilla.org:user-agent")

  , NS_LITERAL_STRING_INIT(kEmpty, "")
  , NS_LITERAL_STRING_INIT(kNull, "null")
  , NS_LITERAL_STRING_INIT(kSOAPArrayTypeAttribute, "arrayType")
  , NS_LITERAL_STRING_INIT(kSOAPArrayOffsetAttribute, "offset")
  , NS_LITERAL_STRING_INIT(kSOAPArrayPositionAttribute, "position")
  , NS_LITERAL_STRING_INIT(kAnyTypeSchemaType, "anyType")
  , NS_LITERAL_STRING_INIT(kAnySimpleTypeSchemaType, "anySimpleType")
  , NS_LITERAL_STRING_INIT(kArraySOAPType, "Array")
  , NS_LITERAL_STRING_INIT(kStructSOAPType, "Struct")
  , NS_LITERAL_STRING_INIT(kStringSchemaType, "string")
  , NS_LITERAL_STRING_INIT(kBooleanSchemaType, "boolean")
  , NS_LITERAL_STRING_INIT(kFloatSchemaType, "float")
  , NS_LITERAL_STRING_INIT(kDoubleSchemaType, "double")
  , NS_LITERAL_STRING_INIT(kLongSchemaType, "long")
  , NS_LITERAL_STRING_INIT(kIntSchemaType, "int")
  , NS_LITERAL_STRING_INIT(kShortSchemaType, "short")
  , NS_LITERAL_STRING_INIT(kByteSchemaType, "byte")
  , NS_LITERAL_STRING_INIT(kUnsignedLongSchemaType, "unsignedLong")
  , NS_LITERAL_STRING_INIT(kUnsignedIntSchemaType, "unsignedInt")
  , NS_LITERAL_STRING_INIT(kUnsignedShortSchemaType, "unsignedShort")
  , NS_LITERAL_STRING_INIT(kUnsignedByteSchemaType, "unsignedByte")
  , NS_LITERAL_STRING_INIT(kNormalizedStringSchemaType, "normalizedString")
  , NS_LITERAL_STRING_INIT(kTokenSchemaType, "token")
  , NS_LITERAL_STRING_INIT(kNameSchemaType, "Name")
  , NS_LITERAL_STRING_INIT(kNCNameSchemaType, "NCName")
  , NS_LITERAL_STRING_INIT(kDecimalSchemaType, "decimal")
  , NS_LITERAL_STRING_INIT(kIntegerSchemaType, "integer")
  , NS_LITERAL_STRING_INIT(kNonPositiveIntegerSchemaType, "nonPositiveInteger")
  , NS_LITERAL_STRING_INIT(kNonNegativeIntegerSchemaType, "nonNegativeInteger")
  , NS_LITERAL_STRING_INIT(kBase64BinarySchemaType, "base64Binary")
{
  kSOAPEnvURI[0] = &kSOAPEnvURI1;
  kSOAPEnvURI[1] = &kSOAPEnvURI2;
}

void nsSOAPUtils::GetSpecificChildElement(nsISOAPEncoding * aEncoding,
                                          nsIDOMElement * aParent,
                                          const nsAString & aNamespace,
                                          const nsAString & aType,
                                          nsIDOMElement * *aElement)
{
  nsCOMPtr<nsIDOMElement> sibling;

  *aElement = nsnull;
  GetFirstChildElement(aParent, getter_AddRefs(sibling));
  if (sibling) {
    GetSpecificSiblingElement(aEncoding, sibling, aNamespace, aType, aElement);
  }
}

void nsSOAPUtils::GetSpecificSiblingElement(nsISOAPEncoding * aEncoding,
                                            nsIDOMElement * aSibling,
                                            const nsAString & aNamespace,
                                            const nsAString & aType,
                                            nsIDOMElement * *aElement)
{
  nsCOMPtr<nsIDOMElement> sibling;

  *aElement = nsnull;
  sibling = aSibling;
  do {
    nsAutoString name, namespaceURI;
    sibling->GetLocalName(name);
    if (name.Equals(aType)) {
      if (aEncoding) {
        nsAutoString temp;
        sibling->GetNamespaceURI(temp);
        aEncoding->GetInternalSchemaURI(temp, namespaceURI);
      }
      else {
        sibling->GetNamespaceURI(namespaceURI);
      }
      if (namespaceURI.Equals(aNamespace)) {
        *aElement = sibling;
        NS_ADDREF(*aElement);
        return;
      }
    }
    nsCOMPtr<nsIDOMElement> temp = sibling;
    GetNextSiblingElement(temp, getter_AddRefs(sibling));
  } while (sibling);
}

void nsSOAPUtils::GetFirstChildElement(nsIDOMElement * aParent,
                                       nsIDOMElement ** aElement)
{
  nsCOMPtr<nsIDOMNode> child;

  *aElement = nsnull;
  aParent->GetFirstChild(getter_AddRefs(child));
  while (child) {
    PRUint16 type;
    child->GetNodeType(&type);
    if (nsIDOMNode::ELEMENT_NODE == type) {
      child->QueryInterface(NS_GET_IID(nsIDOMElement), (void **) aElement);
      break;
    }
    nsCOMPtr<nsIDOMNode> temp = child;
    GetNextSibling(temp, getter_AddRefs(child));
  }
}

void nsSOAPUtils::GetNextSiblingElement(nsIDOMElement * aStart,
                                        nsIDOMElement ** aElement)
{
  nsCOMPtr<nsIDOMNode> sibling;

  *aElement = nsnull;
  GetNextSibling(aStart, getter_AddRefs(sibling));
  while (sibling) {
    PRUint16 type;
    sibling->GetNodeType(&type);
    if (nsIDOMNode::ELEMENT_NODE == type) {
      sibling->QueryInterface(NS_GET_IID(nsIDOMElement),
                              (void **) aElement);
      break;
    }
    nsCOMPtr<nsIDOMNode> temp = sibling;
    GetNextSibling(temp, getter_AddRefs(sibling));
  }
}

nsresult
    nsSOAPUtils::GetElementTextContent(nsIDOMElement * aElement,
                                       nsAString & aText)
{
  aText.Truncate();
  nsCOMPtr<nsIDOMNode> child;
  nsAutoString rtext;
  aElement->GetFirstChild(getter_AddRefs(child));
  while (child) {
    PRUint16 type;
    child->GetNodeType(&type);
    if (nsIDOMNode::TEXT_NODE == type
        || nsIDOMNode::CDATA_SECTION_NODE == type) {
      nsCOMPtr<nsIDOMText> text = do_QueryInterface(child);
      nsAutoString data;
      text->GetData(data);
      rtext.Append(data);
    } else if (nsIDOMNode::ELEMENT_NODE == type) {
      return SOAP_EXCEPTION(NS_ERROR_ILLEGAL_VALUE,"SOAP_UNEXPECTED_ELEMENT", "Unable to retrieve simple content because a child element was present.");
    }
    nsCOMPtr<nsIDOMNode> temp = child;
    GetNextSibling(temp, getter_AddRefs(child));
  }
  aText.Assign(rtext);
  return NS_OK;
}

PRBool nsSOAPUtils::HasChildElements(nsIDOMElement * aElement)
{
  nsCOMPtr<nsIDOMNode> child;

  aElement->GetFirstChild(getter_AddRefs(child));
  while (child) {
    PRUint16 type;
    child->GetNodeType(&type);
    if (nsIDOMNode::ELEMENT_NODE == type) {
      return PR_TRUE;
    }
    nsCOMPtr<nsIDOMNode> temp = child;
    GetNextSibling(temp, getter_AddRefs(child));
  }

  return PR_FALSE;
}

void nsSOAPUtils::GetNextSibling(nsIDOMNode * aSibling,
                                 nsIDOMNode ** aNext)
{
  nsCOMPtr<nsIDOMNode> last;
  nsCOMPtr<nsIDOMNode> current;
  PRUint16 type;

  *aNext = nsnull;
  last = aSibling;

  last->GetNodeType(&type);
  if (nsIDOMNode::ENTITY_REFERENCE_NODE == type) {
    last->GetFirstChild(getter_AddRefs(current));
    if (!last) {
      last->GetNextSibling(getter_AddRefs(current));
    }
  } else {
    last->GetNextSibling(getter_AddRefs(current));
  }
  while (!current) {
    last->GetParentNode(getter_AddRefs(current));
    current->GetNodeType(&type);
    if (nsIDOMNode::ENTITY_REFERENCE_NODE == type) {
      last = current;
      last->GetNextSibling(getter_AddRefs(current));
    } else {
      current = nsnull;
      break;
    }
  }
  *aNext = current;
  NS_IF_ADDREF(*aNext);
}

nsresult
    nsSOAPUtils::GetNamespaceURI(nsISOAPEncoding * aEncoding,
                                 nsIDOMElement * aScope,
                                 const nsAString & aQName,
                                 nsAString & aURI)
{
  aURI.Truncate();
  PRInt32 i = aQName.FindChar(':');
  if (i < 0) {
    return NS_OK;
  }
  nsAutoString prefix;
  prefix = Substring(aQName, 0, i);

  nsAutoString result;
  if (prefix.Equals(gSOAPStrings->kXMLPrefix)) {
    result.Assign(gSOAPStrings->kXMLNamespaceURI);
  }
  else {

    nsresult rc;
    nsCOMPtr<nsIDOMNode> current = aScope;
    nsCOMPtr<nsIDOMNamedNodeMap> attrs;
    nsCOMPtr<nsIDOMNode> temp;
    nsAutoString value;
    while (current) {
      rc = current->GetAttributes(getter_AddRefs(attrs));
      if (NS_FAILED(rc))
        return rc;
      if (attrs) {
        rc = attrs->GetNamedItemNS(gSOAPStrings->kXMLNamespaceNamespaceURI, prefix,
                                   getter_AddRefs(temp));
        if (NS_FAILED(rc))
          return rc;
        if (temp) {
          rc = temp->GetNodeValue(result);
          if (NS_FAILED(rc))
            return rc;
          break;
        }
      }
      rc = current->GetParentNode(getter_AddRefs(temp));
      if (NS_FAILED(rc))
        return rc;
      current = temp;
    }
    if (!current)
      return SOAP_EXCEPTION(NS_ERROR_FAILURE,"SOAP_NAMESPACE", "Unable to resolve prefix in attribute value to namespace URI");
  }
  if (aEncoding) {
    return aEncoding->GetInternalSchemaURI(result,aURI);
  }
  aURI.Assign(result);
  return NS_OK;
}

nsresult
    nsSOAPUtils::GetLocalName(const nsAString & aQName,
                              nsAString & aLocalName)
{
  PRInt32 i = aQName.FindChar(':');
  if (i < 0)
    aLocalName = aQName;
  else
    aLocalName = Substring(aQName, i+1, aQName.Length() - (i+1));
  return NS_OK;
}

nsresult
    nsSOAPUtils::MakeNamespacePrefix(nsISOAPEncoding * aEncoding,
                                     nsIDOMElement * aScope,
                                     const nsAString & aURI,
                                     nsAString & aPrefix)
{
//  This may change for level 3 serialization, so be sure to gut this
//  and call the standardized level 3 method when it is available.
  nsAutoString externalURI;
  if (aEncoding) {
    nsresult rc = aEncoding->GetExternalSchemaURI(aURI,externalURI);
    if (NS_FAILED(rc))
      return rc;
  }
  else {
    externalURI.Assign(aURI);
  }
  aPrefix.Truncate();
  if (externalURI.IsEmpty())
    return NS_OK;
  if (externalURI.Equals(gSOAPStrings->kXMLNamespaceURI)) {
    aPrefix.Assign(gSOAPStrings->kXMLPrefix);
    return NS_OK;
  }
  nsCOMPtr<nsIDOMNode> current = aScope;
  nsCOMPtr<nsIDOMNamedNodeMap> attrs;
  nsCOMPtr<nsIDOMNode> temp;
  nsAutoString tstr;
  nsresult rc;
  PRUint32 maxns = 0;                //  Keep track of max generated NS
  for (;;) {
    rc = current->GetAttributes(getter_AddRefs(attrs));
    if (NS_FAILED(rc))
      return rc;
    if (attrs) {
      PRUint32 i;
      PRUint32 count;
      rc = attrs->GetLength(&count);
      if (NS_FAILED(rc))
        return PR_FALSE;
      for (i = 0;i < count;i++) {
        attrs->Item(i, getter_AddRefs(temp));
        if (!temp)
          break;
        temp->GetNamespaceURI(tstr);
        if (!tstr.Equals(gSOAPStrings->kXMLNamespaceNamespaceURI))
          continue;
        temp->GetNodeValue(tstr);
        if (tstr.Equals(externalURI)) {
          nsAutoString prefix;
          rc = temp->GetLocalName(prefix);
          if (NS_FAILED(rc))
            return rc;
          nsCOMPtr<nsIDOMNode> check = aScope;
          PRBool hasDecl;
          nsCOMPtr<nsIDOMElement> echeck;
          while (check != current) {        // Make sure prefix is not overridden
            echeck = do_QueryInterface(check);
            if (echeck) {
              rc = echeck->
                  HasAttributeNS(gSOAPStrings->kXMLNamespaceNamespaceURI, prefix,
                                 &hasDecl);
              if (NS_FAILED(rc))
                return rc;
              if (hasDecl)
                break;
              echeck->GetParentNode(getter_AddRefs(check));
            }
          }
          if (check == current) {
            aPrefix.Assign(prefix);
            return NS_OK;
          }
        }
        rc = temp->GetLocalName(tstr);
        if (NS_FAILED(rc))
          return rc;
        else {                        //  Decode the generated namespace into a number
          nsReadingIterator < PRUnichar > i1;
          nsReadingIterator < PRUnichar > i2;
          tstr.BeginReading(i1);
          tstr.EndReading(i2);
          if (i1 == i2 || *i1 != 'n')
            continue;
          i1++;
          if (i1 == i2 || *i1 != 's')
            continue;
          i1++;
          PRUint32 n = 0;
          while (i1 != i2) {
            PRUnichar c = *i1;
            i1++;
            if (c < '0' || c > '9') {
              n = 0;
              break;
            }
            n = n * 10 + (c - '0');
          }
          if (n > maxns)
            maxns = n;
        }
      }
    }
    current->GetParentNode(getter_AddRefs(temp));
    if (temp)
      current = temp;
    else
      break;
  }
// Create a unique prefix...
  PRUint32 len = 3;
  PRUint32 c = maxns + 1;
  while (c >= 10) {
    c = c / 10;
    len++;
  }
// Set the length and write it backwards since that's the easiest way..
  aPrefix.SetLength(len);
  nsWritingIterator < PRUnichar > i2;
  aPrefix.EndWriting(i2);
  c = maxns + 1;
  while (c > 0) {
    PRUint32 r = c % 10;
    c = c / 10;
    i2--;
    *i2 = (PRUnichar) (r + '0');
  }
  i2--;
  *i2 = 's';
  i2--;
  *i2 = 'n';

  // Declare the fabricated prefix
  if (aScope) {
    tstr.Assign(gSOAPStrings->kXMLNamespacePrefix);
    tstr.Append(aPrefix);
    rc = aScope->SetAttributeNS(gSOAPStrings->kXMLNamespaceNamespaceURI,
                                tstr, externalURI);
  }
  return NS_OK;
}

/**
 * Get an attribute, keeping in mind that input NamespaceURIs
 * may have all been mapped, so we don't know exactly what
 * we are looking for.  First, directly lookup what tyis type
 * outputs as.  If that fails, then loop through all attributes
 * looking for one that matches after mapping to internal types.
 * Fortunately, where these are used there are not many attributes.
 */
PRBool nsSOAPUtils::GetAttribute(nsISOAPEncoding *aEncoding,
                                  nsIDOMElement * aElement,
                                  const nsAString & aNamespaceURI,
                                  const nsAString & aLocalName,
                                  nsAString & aValue)
{
  nsAutoString value;
  nsresult rc = aEncoding->GetExternalSchemaURI(aNamespaceURI, value);  //  Try most-likely result first.
  if (NS_FAILED(rc))
    return PR_FALSE;
  {
    nsCOMPtr<nsIDOMAttr> attr;
    rc = aElement->GetAttributeNodeNS(value, aLocalName, getter_AddRefs(attr));
    if (NS_FAILED(rc))
      return PR_FALSE;
    if (attr) {
      rc = attr->GetNodeValue(aValue);
      if (NS_FAILED(rc))
        return PR_FALSE;
      return PR_TRUE;
    }
  }
  nsCOMPtr<nsIDOMNamedNodeMap> attrs;
  rc = aElement->GetAttributes(getter_AddRefs(attrs));
  if (NS_FAILED(rc))
    return PR_FALSE;
  PRUint32 count;
  rc = attrs->GetLength(&count);
  if (NS_FAILED(rc))
    return PR_FALSE;
  PRUint32 i;
  for (i = 0; i < count; i++) {
    nsCOMPtr<nsIDOMNode> attrnode;
    rc = attrs->Item(i, getter_AddRefs(attrnode));
    if (NS_FAILED(rc))
      return PR_FALSE;
    rc = attrnode->GetLocalName(value);
    if (NS_FAILED(rc))
      return PR_FALSE;
    if (!aLocalName.Equals(value))
      continue;
    rc = attrnode->GetNamespaceURI(value);
    if (NS_FAILED(rc))
      return PR_FALSE;
    nsAutoString internal;
    rc = aEncoding->GetInternalSchemaURI(value, internal);
    if (NS_FAILED(rc))
      return PR_FALSE;
    if (!aNamespaceURI.Equals(internal))
      continue;
    rc = attrnode->GetNodeValue(aValue);
    if (NS_FAILED(rc))
      return PR_FALSE;
    return PR_TRUE;
  }
  SetAStringToNull(aValue);
  return PR_FALSE;
}
