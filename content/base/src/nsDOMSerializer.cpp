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
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsDOMSerializer.h"
#include "nsIDOMNode.h"
#include "nsIDOMElement.h"
#include "nsIDOMDocumentType.h"
#include "nsIDOMText.h"
#include "nsIDOMCDATASection.h"
#include "nsIDOMComment.h"
#include "nsIDOMProcessingInstruction.h"
#include "nsIDOMNamedNodeMap.h"
#include "nsIDOMAttr.h"
#include "nsIDOMNodeList.h"

#include "nsIOutputStream.h"
#include "nsIUnicodeEncoder.h"
#include "nsIServiceManager.h"
#include "nsICharsetConverterManager.h"

#define USE_NSICONTENT
#ifdef USE_NSICONTENT
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsINameSpaceManager.h"
#endif

static NS_DEFINE_CID(kCharsetConverterManagerCID, NS_ICHARSETCONVERTERMANAGER_CID);

typedef struct {
  nsString mPrefix;
  nsString mURI;
  nsIDOMElement* mOwner;
} NameSpaceDecl;

nsDOMSerializer::nsDOMSerializer()
{
  NS_INIT_ISUPPORTS();
  mPrefixIndex = 0;
}

nsDOMSerializer::~nsDOMSerializer()
{
}

NS_IMPL_ISUPPORTS2(nsDOMSerializer, nsIDOMSerializer, nsISecurityCheckedComponent)

void 
nsDOMSerializer::SerializeText(nsIDOMText* aText, nsString& aStr)
{
  nsAutoString data;
  if (aText) {
    aText->GetData(data);
    aStr.Append(data);
  }
}

void 
nsDOMSerializer::SerializeCDATASection(nsIDOMCDATASection* aCDATASection, 
                                       nsString& aStr)
{
  nsAutoString data;
  if (aCDATASection) {
    aCDATASection->GetData(data);
    aStr.AppendWithConversion("<![CDATA[");
    aStr.Append(data);
    aStr.AppendWithConversion("]]>");
  }
}

void 
nsDOMSerializer::SerializeProcessingInstruction(nsIDOMProcessingInstruction* aPI,
                                                nsString& aStr)
{
  nsAutoString target, data;
  if (aPI) {
    aStr.AppendWithConversion("<?");
    aPI->GetTarget(target);
    aStr.Append(target);
    aPI->GetData(data);
    if (data.Length() > 0) {
      aStr.AppendWithConversion(" ");
      aStr.Append(data);
    }
    aStr.AppendWithConversion(">");
  }
}

void 
nsDOMSerializer::SerializeComment(nsIDOMComment* aComment, 
                                  nsString& aStr)
{
  nsAutoString data;
  if (aComment) {
    aStr.AppendWithConversion("<!--");
    aComment->GetData(data);
    aStr.Append(data);
    aStr.AppendWithConversion("-->");
  }
}

void 
nsDOMSerializer::SerializeDoctype(nsIDOMDocumentType* aDoctype, 
                                  nsString& aStr)
{
  if (aDoctype) {
    nsAutoString name, publicId, systemId, internalSubset;

    aDoctype->GetName(name);
    aDoctype->GetPublicId(publicId);
    aDoctype->GetSystemId(systemId);
    aDoctype->GetInternalSubset(publicId);

    aStr.AppendWithConversion("<!DOCTYPE ");
    aStr.Append(name);
    if (publicId.Length() > 0) {
      aStr.AppendWithConversion(" PUBLIC \"");
      aStr.Append(publicId);
      aStr.AppendWithConversion("\" \"");
      aStr.Append(systemId);
      aStr.AppendWithConversion("\"");
    }
    else if (systemId.Length() > 0) {
      aStr.AppendWithConversion(" SYSTEM \"");
      aStr.Append(systemId);
      aStr.AppendWithConversion("\"");
    }

    if (internalSubset.Length() > 0) {
      aStr.AppendWithConversion(" ");
      aStr.Append(internalSubset);
    }
    
    aStr.AppendWithConversion(">");
  }
}


static const char* kXMLNS = "xmlns";

void
nsDOMSerializer::PushNameSpaceDecl(nsString& aPrefix,
                                   nsString& aURI,
                                   nsIDOMElement* aOwner)
{
  NameSpaceDecl* decl = new NameSpaceDecl();
  if (decl) {
    decl->mPrefix.Assign(aPrefix);
    decl->mURI.Assign(aURI);
    // Don't addref - this weak reference will be removed when
    // we pop the stack
    decl->mOwner = aOwner;

    mNameSpaceStack.AppendElement((void*)decl);
  }
}

void
nsDOMSerializer::PopNameSpaceDeclsFor(nsIDOMElement* aOwner)
{
  PRInt32 index, count;

  count = mNameSpaceStack.Count();
  for (index = count; index >= 0; index--) {
    NameSpaceDecl* decl = (NameSpaceDecl*)mNameSpaceStack.ElementAt(index);
    if (decl) {
      if (decl->mOwner != aOwner) {
        break;
      }
      mNameSpaceStack.RemoveElementAt(index);
      delete decl;
    }
  }
}

PRBool
nsDOMSerializer::ConfirmPrefix(nsString& aPrefix,
                               nsString& aURI)
{
  if (aPrefix.EqualsWithConversion(kXMLNS)) {
    return PR_FALSE;
  }
  if (aURI.Length() == 0) {
    aPrefix.Truncate();
    return PR_FALSE;
  }
  PRInt32 index, count;
  nsAutoString closestURIMatch;
  PRBool uriMatch = PR_FALSE;

  count = mNameSpaceStack.Count();
  for (index = count; index >= 0; index--) {
    NameSpaceDecl* decl = (NameSpaceDecl*)mNameSpaceStack.ElementAt(index);
    if (decl) {
      // Check if we've found a prefix match
      if (aPrefix.Equals(decl->mPrefix)) {
        
        // If the URI's match, we don't have to add a namespace decl
        if (aURI.Equals(decl->mURI)) {
          return PR_FALSE;
        }
        // If they don't, we can't use this prefix
        else {
          aPrefix.Truncate();
        }
      }
      // If we've found a URI match, then record the first one
      else if (!uriMatch && aURI.Equals(decl->mURI)) {
        uriMatch = PR_TRUE;
        closestURIMatch.Assign(decl->mPrefix);
      }
    }
  }

  // There are no namespace declarations that match the prefix, uri pair. 
  // If there's another prefix that matches that URI, us it.
  if (uriMatch) {
    aPrefix.Assign(closestURIMatch);
    return PR_FALSE;
  }
  // If we don't have a prefix, create one
  else if (aPrefix.Length() == 0) {
    aPrefix.AssignWithConversion("a");
    aPrefix.AppendInt(mPrefixIndex++);    
  }

  // Indicate that we need to create a namespace decl for the
  // final prefix
  return PR_TRUE;
}

void
nsDOMSerializer::SerializeAttr(nsString& aPrefix,
                               nsString& aName,
                               nsString& aValue,
                               nsString& aStr)
{
  aStr.AppendWithConversion(" ");
  if (aPrefix.Length() > 0) {
    aStr.Append(aPrefix);
    aStr.AppendWithConversion(":");
  }
  aStr.Append(aName);
  
  aStr.AppendWithConversion("=\"");
  aStr.Append(aValue);
  aStr.AppendWithConversion("\"");
}

void
nsDOMSerializer::SerializeElementStart(nsIDOMElement* aElement, nsString& aStr)
{
  if (aElement) {
    nsAutoString tagPrefix, tagLocalName, tagNamespaceURI;
    nsAutoString xmlnsStr, defaultnsStr;
    xmlnsStr.AssignWithConversion(kXMLNS);
    defaultnsStr.AssignWithConversion("");

    aElement->GetPrefix(tagPrefix);
    aElement->GetLocalName(tagLocalName);
    aElement->GetNamespaceURI(tagNamespaceURI);
    
#ifdef USE_NSICONTENT
    nsCOMPtr<nsIContent> content = do_QueryInterface(aElement);
    PRInt32 index, count;
    nsAutoString nameStr, prefixStr, uriStr, valueStr;
    PRInt32 namespaceID;
    nsCOMPtr<nsIAtom> attrName, attrPrefix;
    if (content) {    
      content->GetAttributeCount(count);

      // First scan for namespace declarations, pushing each on the stack
      for (index = 0; index < count; index++) {

        content->GetAttributeNameAt(index, 
                                    namespaceID,
                                    *getter_AddRefs(attrName),
                                    *getter_AddRefs(attrPrefix));

        if (attrPrefix) { 
          attrPrefix->ToString(prefixStr);
        }
        else {
          prefixStr.Truncate();
        }
        attrName->ToString(nameStr);

        content->GetAttribute(namespaceID, attrName, uriStr);
        if ((namespaceID == kNameSpaceID_XMLNS) ||
            prefixStr.EqualsWithConversion(kXMLNS)) {
          PushNameSpaceDecl(nameStr, uriStr, aElement);
        }
        else if (nameStr.EqualsWithConversion(kXMLNS)) {
          PushNameSpaceDecl(defaultnsStr, uriStr, aElement);
        }
      }
    }
#else
    PRInt32 index, count;
    nsAutoString nameStr, prefixStr, uriStr, valueStr;
    nsCOMPtr<nsIDOMNamedNodeMap> attrMap;
    nsCOMPtr<nsIDOMNode> attrNode;
    nsCOMPtr<nsIDOMAttr> attrObj;
    
    aElement->GetAttributes(getter_AddRefs(attrMap));
    if (attrMap) {
      attrMap->GetLength((PRUint32*)&count);

      // First scan for namespace declarations, pushing each on the stack
      for (index = 0; index < count; index++) {
        attrMap->Item(index, getter_AddRefs(attrNode));
        attrObj = do_QueryInterface(attrNode);
        if (attrObj) {
          attrObj->GetPrefix(prefixStr);
          attrObj->GetLocalName(nameStr);
          attrObj->GetNamespaceURI(uriStr);
          if (prefixStr.EqualsWithConversion(kXMLNS)) {
            PushNameSpaceDecl(nameStr, uriStr, aElement);
          }
          else if (nameStr.EqualsWithConversion(kXMLNS)) {
            PushNameSpaceDecl(nsAutoString(), uriStr, aElement);
          }
        }
      }
    }
#endif

    PRBool addNSAttr;
    
    // Serialize the qualified name of the element
    addNSAttr = ConfirmPrefix(tagPrefix, tagNamespaceURI);
    aStr.AppendWithConversion("<");
    if (tagPrefix.Length() > 0) {
      aStr.Append(tagPrefix);
      aStr.AppendWithConversion(":");
    }
    aStr.Append(tagLocalName);
    
    // If we had to add a new namespace declaration, serialize
    // and push it on the namespace stack
    if (addNSAttr) {
      SerializeAttr(xmlnsStr, tagPrefix, tagNamespaceURI, aStr);
      PushNameSpaceDecl(tagPrefix, tagNamespaceURI, aElement);
    }

#ifdef USE_NSICONTENT
    if (content) {
      // Now serialize each of the attributes
      // XXX Unfortunately we need a namespace manager to get
      // attribute URIs.
      nsCOMPtr<nsIDocument> document;
      nsCOMPtr<nsINameSpaceManager> nsmanager;
      content->GetDocument(*getter_AddRefs(document));
      if (document) {
        document->GetNameSpaceManager(*getter_AddRefs(nsmanager));
      }

      for (index = 0; index < count; index++) {
        content->GetAttributeNameAt(index, 
                                    namespaceID,
                                    *getter_AddRefs(attrName),
                                    *getter_AddRefs(attrPrefix));
        if (attrPrefix) {
          attrPrefix->ToString(prefixStr);
        }
        else {
          prefixStr.Truncate();
        }

        addNSAttr = PR_FALSE;
        if (kNameSpaceID_XMLNS == namespaceID) {
          prefixStr.AssignWithConversion(kXMLNS);
        }
        else if (nsmanager) {
          nsmanager->GetNameSpaceURI(namespaceID, uriStr);
          addNSAttr = ConfirmPrefix(prefixStr, uriStr);
        }
        
        content->GetAttribute(namespaceID, attrName, valueStr);
        attrName->ToString(nameStr);
        
        SerializeAttr(prefixStr, nameStr, valueStr, aStr);
        
        if (addNSAttr) {
          SerializeAttr(xmlnsStr, prefixStr, uriStr, aStr);
          PushNameSpaceDecl(prefixStr, uriStr, aElement);
        }
      }
    }
#else
    if (attrMap) {
      // Now serialize each of the attributes
      for (index = 0; index < count; index++) {
        aStr.AppendWithConversion(" ");
        attrMap->Item(index, getter_AddRefs(attrNode));
        attrObj = do_QueryInterface(attrNode);
        if (attrObj) {
          attrObj->GetPrefix(prefixStr);
          attrObj->GetLocalName(nameStr);
          attrObj->GetNamespaceURI(uriStr);
          
          addNSAttr = ConfirmPrefix(prefixStr, uriStr);
          attrObj->GetNodeValue(valueStr);
          
          SerializeAttr(prefixStr, nameStr, valueStr, aStr);
          
          if (addNSAttr) {
            SerializeAttr(xmlnsStr, prefixStr, uriStr, aStr);
            PushNameSpaceDecl(prefixStr, uriStr, aElement);
          }
        }
      }
    }  
#endif

    aStr.AppendWithConversion(">");    
  }
}

void
nsDOMSerializer::SerializeElementEnd(nsIDOMElement* aElement, nsString& aStr)
{
  if (aElement) {
    nsAutoString tagPrefix, tagLocalName, tagNamespaceURI;

    aElement->GetPrefix(tagPrefix);
    aElement->GetLocalName(tagLocalName);
    aElement->GetNamespaceURI(tagNamespaceURI);

    ConfirmPrefix(tagPrefix, tagNamespaceURI);
    aStr.AppendWithConversion("</");
    if (tagPrefix.Length() > 0) {
      aStr.Append(tagPrefix);
      aStr.AppendWithConversion(":");
    }
    aStr.Append(tagLocalName);
    aStr.AppendWithConversion(">");

    PopNameSpaceDeclsFor(aElement);
  }
}


nsresult
nsDOMSerializer::SerializeNodeStart(nsIDOMNode* aNode, nsString& aStr)
{
  PRUint16 type;

  aNode->GetNodeType(&type);
  switch (type) {
    case nsIDOMNode::ELEMENT_NODE:
    {
      nsCOMPtr<nsIDOMElement> element = do_QueryInterface(aNode);
      SerializeElementStart(element, aStr);
      break;
    }
    case nsIDOMNode::TEXT_NODE:
    {
      nsCOMPtr<nsIDOMText> text = do_QueryInterface(aNode);
      SerializeText(text, aStr);
      break;
    }
    case nsIDOMNode::CDATA_SECTION_NODE:
    {
      nsCOMPtr<nsIDOMCDATASection> cdata = do_QueryInterface(aNode);
      SerializeCDATASection(cdata, aStr);
      break;
    }
    case nsIDOMNode::PROCESSING_INSTRUCTION_NODE:
    {
      nsCOMPtr<nsIDOMProcessingInstruction> pi = do_QueryInterface(aNode);
      SerializeProcessingInstruction(pi, aStr);
      break;
    }
    case nsIDOMNode::COMMENT_NODE:
    {
      nsCOMPtr<nsIDOMComment> comment = do_QueryInterface(aNode);
      SerializeComment(comment, aStr);
      break;
    }
    case nsIDOMNode::DOCUMENT_TYPE_NODE:
    {
      nsCOMPtr<nsIDOMDocumentType> doctype = do_QueryInterface(aNode);
      SerializeDoctype(doctype, aStr);
      break;
    }
  }
  
  return NS_OK;
}

nsresult
nsDOMSerializer::SerializeNodeEnd(nsIDOMNode* aNode, nsString& aStr)
{
  PRUint16 type;

  aNode->GetNodeType(&type);
  switch (type) {
    case nsIDOMNode::ELEMENT_NODE:
    {
      nsCOMPtr<nsIDOMElement> element = do_QueryInterface(aNode);
      SerializeElementEnd(element, aStr);
      break;
    }
  }

  return NS_OK;
}

nsresult
nsDOMSerializer::SerializeToStringRecursive(nsIDOMNode* aNode, nsString& aStr)
{
  nsresult rv = SerializeNodeStart(aNode, aStr);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMNodeList> childNodes;
  rv = aNode->GetChildNodes(getter_AddRefs(childNodes));
  NS_ENSURE_SUCCESS(rv, rv);
  
  if (childNodes) {
    PRInt32 index, count;
    
    childNodes->GetLength((PRUint32*)&count);
    for (index = 0; index < count; index++) {
      nsCOMPtr<nsIDOMNode> child;

      rv = childNodes->Item(index, getter_AddRefs(child));
      NS_ENSURE_SUCCESS(rv, rv);

      rv = SerializeToStringRecursive(child, aStr);
      NS_ENSURE_SUCCESS(rv, rv);     
    }
  }

  rv = SerializeNodeEnd(aNode, aStr);
  return rv;
}

NS_IMETHODIMP
nsDOMSerializer::SerializeToString(nsIDOMNode *root, PRUnichar **_retval)
{
  NS_ENSURE_ARG_POINTER(root);
  NS_ENSURE_ARG_POINTER(_retval);  
  nsresult rv;
  nsAutoString str;

  rv = SerializeToStringRecursive(root, str);
  NS_ENSURE_SUCCESS(rv, rv);
  *_retval = str.ToNewUnicode();
  if (nsnull == *_retval) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_OK;
}

static nsresult
ConvertAndWrite(nsString& aString,
                nsIOutputStream* aStream,
                nsIUnicodeEncoder* aEncoder)
{
  NS_ENSURE_ARG_POINTER(aStream);
  NS_ENSURE_ARG_POINTER(aEncoder);
  nsresult rv;
  PRInt32 charLength;
  PRUnichar* unicodeBuf = (PRUnichar*)aString.GetUnicode();
  PRInt32 unicodeLength = aString.Length();

  rv = aEncoder->GetMaxLength(unicodeBuf, unicodeLength, &charLength);
  if (NS_SUCCEEDED(rv)) {
    nsCAutoString charXferString;
    charXferString.SetCapacity(charLength);
    char* charXferBuf = (char*)charXferString.GetBuffer();

    rv = aEncoder->Convert(unicodeBuf, &unicodeLength, charXferBuf, &charLength);
    if (NS_SUCCEEDED(rv)) {
      PRUint32 written;
      rv = aStream->Write(charXferBuf, charLength, &written);
    }
  }

  return rv;
}

nsresult
nsDOMSerializer::SerializeToStreamRecursive(nsIDOMNode* aNode, 
                                            nsIOutputStream* aStream,
                                            nsIUnicodeEncoder* aEncoder)
{
  nsAutoString start;
  nsresult rv = SerializeNodeStart(aNode, start);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = ConvertAndWrite(start, aStream, aEncoder);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMNodeList> childNodes;
  rv = aNode->GetChildNodes(getter_AddRefs(childNodes));
  NS_ENSURE_SUCCESS(rv, rv);
  
  if (childNodes) {
    PRInt32 index, count;
    
    childNodes->GetLength((PRUint32*)&count);
    for (index = 0; index < count; index++) {
      nsCOMPtr<nsIDOMNode> child;

      rv = childNodes->Item(index, getter_AddRefs(child));
      NS_ENSURE_SUCCESS(rv, rv);

      rv = SerializeToStreamRecursive(child, aStream, aEncoder);
      NS_ENSURE_SUCCESS(rv, rv);     
    }
  }

  nsAutoString end;
  rv = SerializeNodeEnd(aNode, end);
  NS_ENSURE_SUCCESS(rv, rv);     

  rv = ConvertAndWrite(end, aStream, aEncoder);
  return rv;
}

NS_IMETHODIMP
nsDOMSerializer::SerializeToStream(nsIDOMNode *root, 
                                   nsIOutputStream *stream, 
                                   const char *charset)
{
  NS_ENSURE_ARG_POINTER(root);
  NS_ENSURE_ARG_POINTER(stream);
  NS_ENSURE_ARG_POINTER(charset);

  nsresult rv;
  nsCOMPtr<nsIUnicodeEncoder> encoder;

  NS_WITH_SERVICE(nsICharsetConverterManager,
                  charsetConv, 
                  kCharsetConverterManagerCID,
                  &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoString charsetStr;
  charsetStr.AssignWithConversion(charset);
  rv = charsetConv->GetUnicodeEncoder(&charsetStr,
                                      getter_AddRefs(encoder));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = SerializeToStreamRecursive(root, stream, encoder);

  return rv;
}

static const char* kAllAccess = "AllAccess";

/* string canCreateWrapper (in nsIIDPtr iid); */
NS_IMETHODIMP 
nsDOMSerializer::CanCreateWrapper(const nsIID * iid, char **_retval)
{
  if (iid->Equals(NS_GET_IID(nsIDOMSerializer))) {
    *_retval = nsCRT::strdup(kAllAccess);
  }

  return NS_OK;
}

/* string canCallMethod (in nsIIDPtr iid, in wstring methodName); */
NS_IMETHODIMP 
nsDOMSerializer::CanCallMethod(const nsIID * iid, const PRUnichar *methodName, char **_retval)
{
  if (iid->Equals(NS_GET_IID(nsIDOMSerializer))) {
    *_retval = nsCRT::strdup(kAllAccess);
  }

  return NS_OK;
}

/* string canGetProperty (in nsIIDPtr iid, in wstring propertyName); */
NS_IMETHODIMP 
nsDOMSerializer::CanGetProperty(const nsIID * iid, const PRUnichar *propertyName, char **_retval)
{
  if (iid->Equals(NS_GET_IID(nsIDOMSerializer))) {
    *_retval = nsCRT::strdup(kAllAccess);
  }

  return NS_OK;
}

/* string canSetProperty (in nsIIDPtr iid, in wstring propertyName); */
NS_IMETHODIMP 
nsDOMSerializer::CanSetProperty(const nsIID * iid, const PRUnichar *propertyName, char **_retval)
{
  if (iid->Equals(NS_GET_IID(nsIDOMSerializer))) {
    *_retval = nsCRT::strdup(kAllAccess);
  }

  return NS_OK;
}

