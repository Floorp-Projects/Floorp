/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
  mInAttribute = PR_FALSE;
}
 
nsXMLContentSerializer::~nsXMLContentSerializer()
{
}

NS_IMPL_ISUPPORTS1(nsXMLContentSerializer, nsIContentSerializer)

NS_IMETHODIMP 
nsXMLContentSerializer::Init(PRUint32 flags, PRUint32 aWrapColumn,
                             nsIAtom* aCharSet)
{
  return NS_OK;
}

nsresult
nsXMLContentSerializer::AppendTextData(nsIDOMNode* aNode, 
                                       PRInt32 aStartOffset,
                                       PRInt32 aEndOffset,
                                       nsAWritableString& aStr,
                                       PRBool aTranslateEntities,
                                       PRBool aIncrColumn)
{
  nsCOMPtr<nsITextContent> content(do_QueryInterface(aNode));
  if (!content) return NS_ERROR_FAILURE;
  
  const nsTextFragment* frag;
  content->GetText(&frag);

  if (frag) {
    PRInt32 length = ((aEndOffset == -1) ? frag->GetLength() : aEndOffset) - aStartOffset;
    if (frag->Is2b()) {
      AppendToString(nsDependentString(frag->Get2b()+aStartOffset, length),
                     aStr,
                     aTranslateEntities,
                     aIncrColumn);
    }
    else {
      AppendToString(NS_ConvertASCIItoUCS2(frag->Get1b()+aStartOffset, length),
                     aStr,
                     aTranslateEntities,
                     aIncrColumn);
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

  return AppendTextData(aText, aStartOffset, aEndOffset, aStr, PR_TRUE, PR_TRUE);
}

NS_IMETHODIMP 
nsXMLContentSerializer::AppendCDATASection(nsIDOMCDATASection* aCDATASection,
                                           PRInt32 aStartOffset,
                                           PRInt32 aEndOffset,
                                           nsAWritableString& aStr)
{
  NS_ENSURE_ARG(aCDATASection);
  nsresult rv;

  AppendToString(NS_LITERAL_STRING("<![CDATA["), aStr);
  rv = AppendTextData(aCDATASection, aStartOffset, aEndOffset, aStr, PR_FALSE, PR_TRUE);
  if (NS_FAILED(rv)) return NS_ERROR_FAILURE;  
  AppendToString(NS_LITERAL_STRING("]]>"), aStr);

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

  AppendToString(NS_LITERAL_STRING("<?"), aStr);
  AppendToString(target, aStr);
  if (data.Length() > 0) {
    AppendToString(NS_LITERAL_STRING(" "), aStr);
    AppendToString(data, aStr);
  }
  AppendToString(NS_LITERAL_STRING("?>"), aStr);
  
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

  AppendToString(NS_LITERAL_STRING("<!--"), aStr);
  if (aStartOffset || (aEndOffset != -1)) {
    PRInt32 length = (aEndOffset == -1) ? data.Length() : aEndOffset;
    length -= aStartOffset;

    nsAutoString frag;
    data.Mid(frag, aStartOffset, length);
    AppendToString(frag, aStr);
  }
  else {
    AppendToString(data, aStr);
  }
  AppendToString(NS_LITERAL_STRING("-->"), aStr);
  
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
  rv = aDoctype->GetInternalSubset(internalSubset);
  if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

  AppendToString(NS_LITERAL_STRING("<!DOCTYPE "), aStr);
  AppendToString(name, aStr);
  PRUnichar quote;
  if (publicId.Length() > 0) {
    AppendToString(NS_LITERAL_STRING(" PUBLIC "), aStr);
    if (publicId.FindChar(PRUnichar('"')) == -1) {
      quote = PRUnichar('"');
    }
    else {
      quote = PRUnichar('\'');
    }
    AppendToString(quote, aStr);
    AppendToString(publicId, aStr);
    AppendToString(quote, aStr);

    if (systemId.Length()) {
      AppendToString(PRUnichar(' '), aStr);
      if (systemId.FindChar(PRUnichar('"')) == -1) {
        quote = PRUnichar('"');
      }
      else {
        quote = PRUnichar('\'');
      }
      AppendToString(quote, aStr);
      AppendToString(systemId, aStr);
      AppendToString(quote, aStr);
    }
  }
  else if (systemId.Length() > 0) {
    if (systemId.FindChar(PRUnichar('"')) == -1) {
      quote = PRUnichar('"');
    }
    else {
      quote = PRUnichar('\'');
    }
    AppendToString(NS_LITERAL_STRING(" SYSTEM "), aStr);
    AppendToString(quote, aStr);
    AppendToString(systemId, aStr);
    AppendToString(quote, aStr);
  }
  
  if (internalSubset.Length() > 0) {
    AppendToString(PRUnichar(' '), aStr);
    AppendToString(internalSubset, aStr);
  }
    
  AppendToString(NS_LITERAL_STRING(">"), aStr);

  return NS_OK;
}

#define kXMLNS NS_LITERAL_STRING("xmlns")

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
  for (index = count - 1; index >= 0; index--) {
    NameSpaceDecl* decl = (NameSpaceDecl*)mNameSpaceStack.ElementAt(index);
    if (decl->mOwner != aOwner) {
      break;
    }
    mNameSpaceStack.RemoveElementAt(index);
    delete decl;
  }
}

/* ConfirmPrefix() is needed for cases where scripts have 
 * moved/modified elements/attributes 
 */
PRBool
nsXMLContentSerializer::ConfirmPrefix(nsAWritableString& aPrefix,
                                      const nsAReadableString& aURI)
{
  if (aPrefix.Equals(kXMLNS)) {
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
  for (index = count - 1; index >= 0; index--) {
    NameSpaceDecl* decl = (NameSpaceDecl*)mNameSpaceStack.ElementAt(index);
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
                                      nsAWritableString& aStr,
                                      PRBool aDoEscapeEntities)
{
  AppendToString(PRUnichar(' '), aStr);
  if (aPrefix.Length() > 0) {
    AppendToString(aPrefix, aStr);
    AppendToString(NS_LITERAL_STRING(":"), aStr);
  }
  AppendToString(aName, aStr);
  
  AppendToString(NS_LITERAL_STRING("=\""), aStr);

  mInAttribute = PR_TRUE;
  AppendToString(aValue, aStr, aDoEscapeEntities);
  mInAttribute = PR_FALSE;

  AppendToString(NS_LITERAL_STRING("\""), aStr);
}

NS_IMETHODIMP 
nsXMLContentSerializer::AppendElementStart(nsIDOMElement *aElement,
                                           nsAWritableString& aStr)
{
  NS_ENSURE_ARG(aElement);

  nsAutoString tagPrefix, tagLocalName, tagNamespaceURI;
  nsAutoString xmlnsStr;
  xmlnsStr.Assign(kXMLNS);

  nsCOMPtr<nsIContent> content(do_QueryInterface(aElement));
  if (!content) return NS_ERROR_FAILURE;

  aElement->GetPrefix(tagPrefix);
  aElement->GetLocalName(tagLocalName);
  aElement->GetNamespaceURI(tagNamespaceURI);

  PRInt32 namespaceID, elementNamespaceID;
  content->GetNameSpaceID(elementNamespaceID);
  if (elementNamespaceID == kNameSpaceID_HTML)
    tagLocalName.ToLowerCase(); // XXX We shouldn't need this hack
    
  PRInt32 index, count;
  nsAutoString nameStr, prefixStr, uriStr, valueStr;
  nsCOMPtr<nsIAtom> attrName, attrPrefix;

  content->GetAttrCount(count);
  
  // First scan for namespace declarations, pushing each on the stack
  for (index = 0; index < count; index++) {
    
    content->GetAttrNameAt(index, 
                           namespaceID,
                           *getter_AddRefs(attrName),
                           *getter_AddRefs(attrPrefix));
    
    if (namespaceID == kNameSpaceID_XMLNS ||
      elementNamespaceID == kNameSpaceID_HTML /*XXX Hack*/) {
      PRBool hasPrefix = attrPrefix ? PR_TRUE : PR_FALSE;
      content->GetAttr(namespaceID, attrName, uriStr);

      attrName->ToString(nameStr);
      // XXX We shouldn't need this hack
      if (elementNamespaceID == kNameSpaceID_HTML) {
        if (nameStr.EqualsWithConversion("xmlns:",PR_FALSE,6)) {
          nameStr.Cut(0,6);
          hasPrefix = PR_TRUE;
        } else if (!nameStr.Equals(kXMLNS)) {
          continue;
        }
      }

      if (!hasPrefix) {
        // Default NS attribute does not have prefix (and the name is "xmlns")
        PushNameSpaceDecl(nsString(), uriStr, aElement);
      } else {
        PushNameSpaceDecl(nameStr, uriStr, aElement);
      }
    }
  }

  PRBool addNSAttr;
    
  addNSAttr = ConfirmPrefix(tagPrefix, tagNamespaceURI);
  // Serialize the qualified name of the element
  AppendToString(NS_LITERAL_STRING("<"), aStr);
  if (tagPrefix.Length() > 0 &&
    !(elementNamespaceID == kNameSpaceID_HTML && tagPrefix.Equals(kXMLNS) /*XXX Hack*/)) {
    AppendToString(tagPrefix, aStr);
    AppendToString(NS_LITERAL_STRING(":"), aStr);
  }
  AppendToString(tagLocalName, aStr);
    
  // If we had to add a new namespace declaration, serialize
  // and push it on the namespace stack
  if (addNSAttr) {
    SerializeAttr(xmlnsStr, tagPrefix, tagNamespaceURI, aStr, PR_TRUE);
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
    content->GetAttrNameAt(index, 
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
    if (kNameSpaceID_XMLNS != namespaceID && nsmanager) {
      nsmanager->GetNameSpaceURI(namespaceID, uriStr);
      addNSAttr = ConfirmPrefix(prefixStr, uriStr);
    }
    
    content->GetAttr(namespaceID, attrName, valueStr);
    attrName->ToString(nameStr);
    
    if (elementNamespaceID == kNameSpaceID_HTML && nameStr.Equals(NS_LITERAL_STRING("xmlns:xmlns")))
      nameStr.Assign(kXMLNS); // XXX Shouldn't need this hack, breaks case where there really is xmlns:xmlns
    SerializeAttr(prefixStr, nameStr, valueStr, aStr, PR_TRUE);
    
    if (addNSAttr) {
      SerializeAttr(xmlnsStr, prefixStr, uriStr, aStr, PR_TRUE);
      PushNameSpaceDecl(prefixStr, uriStr, aElement);
    }
  }

  // We don't output a separate end tag for empty element
  nsCOMPtr<nsIDOMNode> node(do_QueryInterface(aElement));
  PRBool hasChildren;
  if (NS_SUCCEEDED(node->HasChildNodes(&hasChildren)) && !hasChildren) {
    AppendToString(NS_LITERAL_STRING("/>"), aStr);    
  } else {
    AppendToString(NS_LITERAL_STRING(">"), aStr);    
  }
  
  return NS_OK;
}

NS_IMETHODIMP 
nsXMLContentSerializer::AppendElementEnd(nsIDOMElement *aElement,
                                         nsAWritableString& aStr)
{
  NS_ENSURE_ARG(aElement);

  // We don't output a separate end tag for empty element
  nsCOMPtr<nsIDOMNode> node(do_QueryInterface(aElement));
  PRBool hasChildren;
  if (NS_SUCCEEDED(node->HasChildNodes(&hasChildren)) && !hasChildren) {
    return NS_OK;
  }
  
  nsCOMPtr<nsIContent> content(do_QueryInterface(aElement));
  if (!content) return NS_ERROR_FAILURE;

  nsAutoString tagPrefix, tagLocalName, tagNamespaceURI;
  
  aElement->GetPrefix(tagPrefix);
  aElement->GetLocalName(tagLocalName);
  aElement->GetNamespaceURI(tagNamespaceURI);

  PRInt32 namespaceID;
  content->GetNameSpaceID(namespaceID);
  if (namespaceID == kNameSpaceID_HTML)
    tagLocalName.ToLowerCase(); // XXX We shouldn't need this hack

  ConfirmPrefix(tagPrefix, tagNamespaceURI);

  AppendToString(NS_LITERAL_STRING("</"), aStr);
  if (tagPrefix.Length() > 0 &&
    !(namespaceID == kNameSpaceID_HTML && tagPrefix.Equals(kXMLNS) /*XXX Hack*/)) {
    AppendToString(tagPrefix, aStr);
    AppendToString(NS_LITERAL_STRING(":"), aStr);
  }
  AppendToString(tagLocalName, aStr);
  AppendToString(NS_LITERAL_STRING(">"), aStr);
  
  PopNameSpaceDeclsFor(aElement);
  
  return NS_OK;
}

void
nsXMLContentSerializer::AppendToString(const PRUnichar* aStr,
                                       PRInt32 aLength,
                                       nsAWritableString& aOutputStr)
{
  PRInt32 length = (aLength == -1) ? nsCRT::strlen(aStr) : aLength;
  
  aOutputStr.Append(aStr, length);
}

void 
nsXMLContentSerializer::AppendToString(const PRUnichar aChar,
                                       nsAWritableString& aOutputStr)
{
  aOutputStr.Append(aChar);
}

static PRUint16 kGTVal = 62;
static const char* kEntities[] = {
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "&amp;", "",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  "&lt;", "", "&gt;"
};

static const char* kAttrEntities[] = {
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "&quot;", "", "", "", "&amp;", "&apos;",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  "&lt;", "", "&gt;"
};

void
nsXMLContentSerializer::AppendToString(const nsAReadableString& aStr,
                                       nsAWritableString& aOutputStr,
                                       PRBool aTranslateEntities,
                                       PRBool aIncrColumn)
{
  if (aTranslateEntities) {
    nsReadingIterator<PRUnichar> done_reading;
    aStr.EndReading(done_reading);

    // for each chunk of |aString|...
    PRUint32 advanceLength = 0;
    nsReadingIterator<PRUnichar> iter;

    const char **entityTable = mInAttribute ? kAttrEntities : kEntities;

    for (aStr.BeginReading(iter); 
         iter != done_reading; 
         iter.advance(PRInt32(advanceLength))) {
      PRUint32 fragmentLength = iter.size_forward();
      const PRUnichar* c = iter.get();
      const PRUnichar* fragmentStart = c;
      const PRUnichar* fragmentEnd = c + fragmentLength;
      const char* entityText = nsnull;

      advanceLength = 0;
      // for each character in this chunk, check if it
      // needs to be replaced
      for (; c < fragmentEnd; c++, advanceLength++) {
        PRUnichar val = *c;
        if ((val <= kGTVal) && (entityTable[val][0] != 0)) {
          entityText = entityTable[val];
          break;
        }
      }

      aOutputStr.Append(fragmentStart, advanceLength);
      if (entityText) {
        aOutputStr.Append(NS_ConvertASCIItoUCS2(entityText));
        advanceLength++;
      }
    }

    return;
  }
  
  aOutputStr.Append(aStr);
}
