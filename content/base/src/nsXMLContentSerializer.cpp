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

#include "nsXMLContentSerializer.h"

#include "nsIDOMText.h"
#include "nsIDOMCDATASection.h"
#include "nsIDOMProcessingInstruction.h"
#include "nsIDOMComment.h"
#include "nsIDOMDocumentType.h"
#include "nsIDOMElement.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsINameSpaceManager.h"
#include "nsITextContent.h"
#include "nsTextFragment.h"
#include "nsString.h"
#include "prprf.h"

typedef struct {
  nsString mPrefix;
  nsString mURI;
  nsIDOMElement* mOwner;
} NameSpaceDecl;

nsresult NS_NewXMLContentSerializer(nsIContentSerializer** aSerializer)
{
  nsXMLContentSerializer* it = new nsXMLContentSerializer();
  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return it->QueryInterface(NS_GET_IID(nsIContentSerializer), (void**)aSerializer);
}

nsXMLContentSerializer::nsXMLContentSerializer()
{
  NS_INIT_ISUPPORTS();
  mPrefixIndex = 0;
}
 
nsXMLContentSerializer::~nsXMLContentSerializer()
{
}

NS_IMPL_ISUPPORTS1(nsXMLContentSerializer, nsIContentSerializer)

NS_IMETHODIMP 
nsXMLContentSerializer::Init(PRUint32 flags, PRUint32 aWrapColumn)
{
  return NS_OK;
}

static PRUint16 kGTVal = 62;
static const char* kEntities[] = {
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "&quot;", "", "", "", "&amp;", "&apos;",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  "&lt;", "", "&gt;"
};

void
nsXMLContentSerializer::ReplaceCharacterEntities(nsAWritableString& aStr,
                                                 PRUint32 aOffset)
{
  nsReadingIterator<PRUnichar> done_reading;
  aStr.EndReading(done_reading);

  // for each chunk of |aString|...
  PRUint32 fragmentLength = 0;
  PRUint32 offset = aOffset;
  nsReadingIterator<PRUnichar> iter;
  aStr.BeginReading(iter);

  for (iter.advance(PRInt32(offset)); 
       iter != done_reading; 
       iter.advance(PRInt32(fragmentLength))) {
    fragmentLength = iter.size_forward();
    const PRUnichar* c = iter.get();
    const PRUnichar* fragmentEnd = c + fragmentLength;
    
    // for each character in this chunk...
    for (; c < fragmentEnd; c++, offset++) {
      PRUnichar val = *c;
      if ((val <= kGTVal) && (kEntities[val][0] != 0)) {
        aStr.Cut(offset, 1);
        aStr.Insert(NS_LITERAL_STRING(kEntities[val]), offset);
        aStr.EndReading(done_reading);
        aStr.BeginReading(iter);
        fragmentLength = offset+nsCRT::strlen(kEntities[val]);
        break;
      }
    }
  }
}

nsresult
nsXMLContentSerializer::AppendTextData(nsIDOMNode* aNode, 
                                       PRInt32 aStartOffset,
                                       PRInt32 aEndOffset,
                                       nsAWritableString& aStr)
{
  nsCOMPtr<nsITextContent> content = do_QueryInterface(aNode);
  if (!content) return NS_ERROR_FAILURE;
  
  const nsTextFragment* frag;
  content->GetText(&frag);
  
  if (frag) {
    PRInt32 length = (aEndOffset == -1) ? frag->GetLength() : aEndOffset;
    length -= aStartOffset;
    if (frag->Is2b()) {
      aStr.Append(frag->Get2b()+aStartOffset, length);
    }
    else {
      aStr.Append(NS_ConvertASCIItoUCS2(frag->Get1b()+aStartOffset, length));
    }
  }
  
  return NS_OK;
}

NS_IMETHODIMP 
nsXMLContentSerializer::AppendText(nsIDOMText* aText, 
                                   PRInt32 aStartOffset,
                                   PRInt32 aEndOffset,
                                   nsAWritableString& aStr)
{
  NS_ENSURE_ARG(aText);

  PRUint32 length = aStr.Length();

  nsresult rv;
  rv = AppendTextData(aText, aStartOffset, aEndOffset, aStr);
  if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

  ReplaceCharacterEntities(aStr, length);

  return NS_OK;
}

NS_IMETHODIMP 
nsXMLContentSerializer::AppendCDATASection(nsIDOMCDATASection* aCDATASection,
                                           PRInt32 aStartOffset,
                                           PRInt32 aEndOffset,
                                           nsAWritableString& aStr)
{
  NS_ENSURE_ARG(aCDATASection);
  nsresult rv;

  aStr.Append(NS_LITERAL_STRING("<![CDATA["));
  rv = AppendTextData(aCDATASection, aStartOffset, aEndOffset, aStr);
  if (NS_FAILED(rv)) return NS_ERROR_FAILURE;  
  aStr.Append(NS_LITERAL_STRING("]]>"));

  return NS_OK;
}

NS_IMETHODIMP 
nsXMLContentSerializer::AppendProcessingInstruction(nsIDOMProcessingInstruction* aPI,
                                                    PRInt32 aStartOffset,
                                                    PRInt32 aEndOffset,
                                                    nsAWritableString& aStr)
{
  NS_ENSURE_ARG(aPI);
  nsresult rv;
  nsAutoString target, data;

  rv = aPI->GetTarget(target);
  if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

  rv = aPI->GetData(data);
  if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

  aStr.Append(NS_LITERAL_STRING("<?"));
  aStr.Append(target);
  if (data.Length() > 0) {
    aStr.Append(NS_LITERAL_STRING(" "));
    aStr.Append(data);
  }
  aStr.Append(NS_LITERAL_STRING(">"));
  
  return NS_OK;
}

NS_IMETHODIMP 
nsXMLContentSerializer::AppendComment(nsIDOMComment* aComment,
                                      PRInt32 aStartOffset,
                                      PRInt32 aEndOffset,
                                      nsAWritableString& aStr)
{
  NS_ENSURE_ARG(aComment);
  nsresult rv;
  nsAutoString data;

  rv = aComment->GetData(data);
  if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

  aStr.Append(NS_LITERAL_STRING("<!--"));
  if (aStartOffset || (aEndOffset != -1)) {
    PRInt32 length = (aEndOffset == -1) ? data.Length() : aEndOffset;
    length -= aStartOffset;

    nsAutoString frag;
    data.Mid(frag, aStartOffset, length);
    aStr.Append(frag);
  }
  else {
    aStr.Append(data);
  }
  aStr.Append(NS_LITERAL_STRING("-->"));
  
  return NS_OK;
}

NS_IMETHODIMP 
nsXMLContentSerializer::AppendDoctype(nsIDOMDocumentType *aDoctype,
                                      nsAWritableString& aStr)
{
  NS_ENSURE_ARG(aDoctype);
  nsresult rv;
  nsAutoString name, publicId, systemId, internalSubset;

  rv = aDoctype->GetName(name);
  if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
  rv = aDoctype->GetPublicId(publicId);
  if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
  rv = aDoctype->GetSystemId(systemId);
  if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
  rv = aDoctype->GetInternalSubset(publicId);
  if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

  aStr.Append(NS_LITERAL_STRING("<!DOCTYPE "));
  aStr.Append(name);
  PRUnichar quote;
  if (publicId.Length() > 0) {
    aStr.Append(NS_LITERAL_STRING(" PUBLIC "));
    if (publicId.FindChar(PRUnichar('"')) == -1) {
      quote = PRUnichar('"');
    }
    else {
      quote = PRUnichar('\'');
    }
    aStr.Append(quote);
    aStr.Append(publicId);
    aStr.Append(quote);
    aStr.Append(PRUnichar(' '));
    if (systemId.FindChar(PRUnichar('"')) == -1) {
      quote = PRUnichar('"');
    }
    else {
      quote = PRUnichar('\'');
    }
    aStr.Append(quote);
    aStr.Append(systemId);
    aStr.Append(quote);
  }
  else if (systemId.Length() > 0) {
    if (systemId.FindChar(PRUnichar('"')) == -1) {
      quote = PRUnichar('"');
    }
    else {
      quote = PRUnichar('\'');
    }
    aStr.Append(NS_LITERAL_STRING(" SYSTEM "));
    aStr.Append(quote);
    aStr.Append(systemId);
    aStr.Append(quote);
  }
  
  if (internalSubset.Length() > 0) {
    aStr.Append(PRUnichar(' '));
    aStr.Append(internalSubset);
  }
    
  aStr.Append(NS_LITERAL_STRING(">"));

  return NS_OK;
}

static const char* kXMLNS = "xmlns";

nsresult
nsXMLContentSerializer::PushNameSpaceDecl(const nsAReadableString& aPrefix,
                                          const nsAReadableString& aURI,
                                          nsIDOMElement* aOwner)
{
  NameSpaceDecl* decl = new NameSpaceDecl();
  if (!decl) return NS_ERROR_OUT_OF_MEMORY;

  decl->mPrefix.Assign(aPrefix);
  decl->mURI.Assign(aURI);
  // Don't addref - this weak reference will be removed when
  // we pop the stack
  decl->mOwner = aOwner;

  mNameSpaceStack.AppendElement((void*)decl);
  return NS_OK;
}

void
nsXMLContentSerializer::PopNameSpaceDeclsFor(nsIDOMElement* aOwner)
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
nsXMLContentSerializer::ConfirmPrefix(nsAWritableString& aPrefix,
                                      const nsAReadableString& aURI)
{
  if (aPrefix.Equals(NS_LITERAL_STRING(kXMLNS))) {
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
    aPrefix.Assign(NS_LITERAL_STRING("a"));
    char buf[128];
    PR_snprintf(buf, sizeof(buf), "%d", mPrefixIndex++);
    aPrefix.Append(NS_ConvertASCIItoUCS2(buf));    
  }

  // Indicate that we need to create a namespace decl for the
  // final prefix
  return PR_TRUE;
}

void
nsXMLContentSerializer::SerializeAttr(const nsAReadableString& aPrefix,
                                      const nsAReadableString& aName,
                                      const nsAReadableString& aValue,
                                      nsAWritableString& aStr)
{
  aStr.Append(NS_LITERAL_STRING(" "));
  if (aPrefix.Length() > 0) {
    aStr.Append(aPrefix);
    aStr.Append(NS_LITERAL_STRING(":"));
  }
  aStr.Append(aName);
  
  aStr.Append(NS_LITERAL_STRING("=\""));
  aStr.Append(aValue);
  aStr.Append(NS_LITERAL_STRING("\""));
}

NS_IMETHODIMP 
nsXMLContentSerializer::AppendElementStart(nsIDOMElement *aElement,
                                           nsAWritableString& aStr)
{
  NS_ENSURE_ARG(aElement);

  nsAutoString tagPrefix, tagLocalName, tagNamespaceURI;
  nsAutoString xmlnsStr, defaultnsStr;
  xmlnsStr.Assign(NS_LITERAL_STRING(kXMLNS));
  defaultnsStr.Assign(NS_LITERAL_STRING(""));

  aElement->GetPrefix(tagPrefix);
  aElement->GetLocalName(tagLocalName);
  aElement->GetNamespaceURI(tagNamespaceURI);
    
  nsCOMPtr<nsIContent> content = do_QueryInterface(aElement);
  if (!content) return NS_ERROR_FAILURE;

  PRInt32 index, count;
  nsAutoString nameStr, prefixStr, uriStr, valueStr;
  PRInt32 namespaceID;
  nsCOMPtr<nsIAtom> attrName, attrPrefix;

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
        prefixStr.Equals(NS_LITERAL_STRING(kXMLNS))) {
      PushNameSpaceDecl(nameStr, uriStr, aElement);
    }
    else if (nameStr.Equals(NS_LITERAL_STRING(kXMLNS))) {
      PushNameSpaceDecl(defaultnsStr, uriStr, aElement);
    }
  }

  PRBool addNSAttr;
    
  // Serialize the qualified name of the element
  addNSAttr = ConfirmPrefix(tagPrefix, tagNamespaceURI);
  aStr.Append(NS_LITERAL_STRING("<"));
  if (tagPrefix.Length() > 0) {
    aStr.Append(tagPrefix);
    aStr.Append(NS_LITERAL_STRING(":"));
  }
  aStr.Append(tagLocalName);
    
  // If we had to add a new namespace declaration, serialize
  // and push it on the namespace stack
  if (addNSAttr) {
    SerializeAttr(xmlnsStr, tagPrefix, tagNamespaceURI, aStr);
    PushNameSpaceDecl(tagPrefix, tagNamespaceURI, aElement);
  }

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
      prefixStr.Assign(NS_LITERAL_STRING(kXMLNS));
    }
    else if (nsmanager) {
      nsmanager->GetNameSpaceURI(namespaceID, uriStr);
      addNSAttr = ConfirmPrefix(prefixStr, uriStr);
    }
        
    content->GetAttribute(namespaceID, attrName, valueStr);
    attrName->ToString(nameStr);
    
    ReplaceCharacterEntities(valueStr, 0);
    SerializeAttr(prefixStr, nameStr, valueStr, aStr);
    
    if (addNSAttr) {
      SerializeAttr(xmlnsStr, prefixStr, uriStr, aStr);
      PushNameSpaceDecl(prefixStr, uriStr, aElement);
    }
  }

  aStr.Append(NS_LITERAL_STRING(">"));    
  
  return NS_OK;
}

NS_IMETHODIMP 
nsXMLContentSerializer::AppendElementEnd(nsIDOMElement *aElement,
                                         nsAWritableString& aStr)
{
  NS_ENSURE_ARG(aElement);

  nsAutoString tagPrefix, tagLocalName, tagNamespaceURI;
  
  aElement->GetPrefix(tagPrefix);
  aElement->GetLocalName(tagLocalName);
  aElement->GetNamespaceURI(tagNamespaceURI);

  ConfirmPrefix(tagPrefix, tagNamespaceURI);
  aStr.Append(NS_LITERAL_STRING("</"));
  if (tagPrefix.Length() > 0) {
    aStr.Append(tagPrefix);
    aStr.Append(NS_LITERAL_STRING(":"));
  }
  aStr.Append(tagLocalName);
  aStr.Append(NS_LITERAL_STRING(">"));
  
  PopNameSpaceDeclsFor(aElement);
  
  return NS_OK;
}
