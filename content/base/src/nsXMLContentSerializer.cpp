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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsXMLContentSerializer.h"

#include "nsHTMLAtoms.h"
#include "nsIDOMText.h"
#include "nsIDOMCDATASection.h"
#include "nsIDOMProcessingInstruction.h"
#include "nsIDOMComment.h"
#include "nsIDOMDocument.h"
#include "nsIDOMDocumentType.h"
#include "nsIDOMElement.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsINameSpaceManager.h"
#include "nsITextContent.h"
#include "nsTextFragment.h"
#include "nsString.h"
#include "prprf.h"
#include "nsUnicharUtils.h"
#include "nsCRT.h"
#include "nsContentUtils.h"

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

  return CallQueryInterface(it, aSerializer);
}

nsXMLContentSerializer::nsXMLContentSerializer()
  : mPrefixIndex(0),
    mInAttribute(PR_FALSE),
    mAddNewline(PR_FALSE)
{
}
 
nsXMLContentSerializer::~nsXMLContentSerializer()
{
}

NS_IMPL_ISUPPORTS1(nsXMLContentSerializer, nsIContentSerializer)

NS_IMETHODIMP 
nsXMLContentSerializer::Init(PRUint32 flags, PRUint32 aWrapColumn,
                             const char* aCharSet, PRBool aIsCopying)
{
  return NS_OK;
}

nsresult
nsXMLContentSerializer::AppendTextData(nsIDOMNode* aNode, 
                                       PRInt32 aStartOffset,
                                       PRInt32 aEndOffset,
                                       nsAString& aStr,
                                       PRBool aTranslateEntities,
                                       PRBool aIncrColumn)
{
  nsCOMPtr<nsITextContent> content(do_QueryInterface(aNode));
  if (!content) return NS_ERROR_FAILURE;

  const nsTextFragment* frag = content->Text();

  PRInt32 endoffset = (aEndOffset == -1) ? frag->GetLength() : aEndOffset;
  PRInt32 length = endoffset - aStartOffset;

  NS_ASSERTION(aStartOffset >= 0, "Negative start offset for text fragment!");
  NS_ASSERTION(aStartOffset <= endoffset, "A start offset is beyond the end of the text fragment!");

  if (length <= 0) {
    // XXX Zero is a legal value, maybe non-zero values should be an
    // error.

    return NS_OK;
  }
    
  if (frag->Is2b()) {
    const PRUnichar *strStart = frag->Get2b() + aStartOffset;
    AppendToString(Substring(strStart, strStart + length), aStr,
                   aTranslateEntities, aIncrColumn);
  }
  else {
    AppendToString(NS_ConvertASCIItoUCS2(frag->Get1b()+aStartOffset, length),
                   aStr, aTranslateEntities, aIncrColumn);
  }

  return NS_OK;
}

NS_IMETHODIMP 
nsXMLContentSerializer::AppendText(nsIDOMText* aText, 
                                   PRInt32 aStartOffset,
                                   PRInt32 aEndOffset,
                                   nsAString& aStr)
{
  NS_ENSURE_ARG(aText);

  return AppendTextData(aText, aStartOffset, aEndOffset, aStr, PR_TRUE, PR_TRUE);
}

NS_IMETHODIMP 
nsXMLContentSerializer::AppendCDATASection(nsIDOMCDATASection* aCDATASection,
                                           PRInt32 aStartOffset,
                                           PRInt32 aEndOffset,
                                           nsAString& aStr)
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
                                                    nsAString& aStr)
{
  NS_ENSURE_ARG(aPI);
  nsresult rv;
  nsAutoString target, data;

  MaybeAddNewline(aStr);

  rv = aPI->GetTarget(target);
  if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

  rv = aPI->GetData(data);
  if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

  AppendToString(NS_LITERAL_STRING("<?"), aStr);
  AppendToString(target, aStr);
  if (!data.IsEmpty()) {
    AppendToString(NS_LITERAL_STRING(" "), aStr);
    AppendToString(data, aStr);
  }
  AppendToString(NS_LITERAL_STRING("?>"), aStr);
  MaybeFlagNewline(aPI);
  
  return NS_OK;
}

NS_IMETHODIMP 
nsXMLContentSerializer::AppendComment(nsIDOMComment* aComment,
                                      PRInt32 aStartOffset,
                                      PRInt32 aEndOffset,
                                      nsAString& aStr)
{
  NS_ENSURE_ARG(aComment);
  nsresult rv;
  nsAutoString data;

  rv = aComment->GetData(data);
  if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

  MaybeAddNewline(aStr);

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
  MaybeFlagNewline(aComment);
  
  return NS_OK;
}

NS_IMETHODIMP 
nsXMLContentSerializer::AppendDoctype(nsIDOMDocumentType *aDoctype,
                                      nsAString& aStr)
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

  MaybeAddNewline(aStr);

  AppendToString(NS_LITERAL_STRING("<!DOCTYPE "), aStr);
  AppendToString(name, aStr);
  PRUnichar quote;
  if (!publicId.IsEmpty()) {
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

    if (!systemId.IsEmpty()) {
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
  else if (!systemId.IsEmpty()) {
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
  
  if (!internalSubset.IsEmpty()) {
    AppendToString(NS_LITERAL_STRING(" ["), aStr);
    AppendToString(internalSubset, aStr);
    AppendToString(PRUnichar(']'), aStr);
  }
    
  AppendToString(PRUnichar('>'), aStr);
  MaybeFlagNewline(aDoctype);

  return NS_OK;
}

#define kXMLNS NS_LITERAL_STRING("xmlns")

nsresult
nsXMLContentSerializer::PushNameSpaceDecl(const nsAString& aPrefix,
                                          const nsAString& aURI,
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
nsXMLContentSerializer::ConfirmPrefix(nsAString& aPrefix,
                                      const nsAString& aURI)
{
  if (aPrefix.Equals(kXMLNS)) {
    return PR_FALSE;
  }
  if (aURI.IsEmpty()) {
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
  else if (aPrefix.IsEmpty()) {
    aPrefix.Assign(NS_LITERAL_STRING("a"));
    char buf[128];
    PR_snprintf(buf, sizeof(buf), "%d", mPrefixIndex++);
    AppendASCIItoUTF16(buf, aPrefix);
  }

  // Indicate that we need to create a namespace decl for the
  // final prefix
  return PR_TRUE;
}

void
nsXMLContentSerializer::SerializeAttr(const nsAString& aPrefix,
                                      const nsAString& aName,
                                      const nsAString& aValue,
                                      nsAString& aStr,
                                      PRBool aDoEscapeEntities)
{
  AppendToString(PRUnichar(' '), aStr);
  if (!aPrefix.IsEmpty()) {
    AppendToString(aPrefix, aStr);
    AppendToString(PRUnichar(':'), aStr);
  }
  AppendToString(aName, aStr);
  
  if ( aDoEscapeEntities ) {
    // if problem characters are turned into character entity references
    // then there will be no problem with the value delimiter characters
    AppendToString(NS_LITERAL_STRING("=\""), aStr);

    mInAttribute = PR_TRUE;
    AppendToString(aValue, aStr, PR_TRUE);
    mInAttribute = PR_FALSE;

    AppendToString(PRUnichar('"'), aStr);
  }
  else {
    // Depending on whether the attribute value contains quotes or apostrophes we
    // need to select the delimiter character and escape characters using
    // character entity references, ignoring the value of aDoEscapeEntities.
    // See http://www.w3.org/TR/REC-html40/appendix/notes.html#h-B.3.2.2 for
    // the standard on character entity references in values. 
    PRBool bIncludesSingle = PR_FALSE;
    PRBool bIncludesDouble = PR_FALSE;
    nsAString::const_iterator iCurr, iEnd;
    PRUint32 uiSize, i;
    aValue.BeginReading(iCurr);
    aValue.EndReading(iEnd);
    for ( ; iCurr != iEnd; iCurr.advance(uiSize) ) {
      const PRUnichar * buf = iCurr.get();
      uiSize = iCurr.size_forward();
      for ( i = 0; i < uiSize; i++, buf++ ) {
        if ( *buf == PRUnichar('\'') )
        {
          bIncludesSingle = PR_TRUE;
          if ( bIncludesDouble ) break;
        }
        else if ( *buf == PRUnichar('"') )
        {
          bIncludesDouble = PR_TRUE;
          if ( bIncludesSingle ) break;
        }
      }
      // if both have been found we don't need to search further
      if ( bIncludesDouble && bIncludesSingle ) break;
    }

    // Delimiter and escaping is according to the following table
    //    bIncludesDouble     bIncludesSingle     Delimiter       Escape Double Quote
    //    FALSE               FALSE               "               FALSE
    //    FALSE               TRUE                "               FALSE
    //    TRUE                FALSE               '               FALSE
    //    TRUE                TRUE                "               TRUE
    PRUnichar cDelimiter = 
        (bIncludesDouble && !bIncludesSingle) ? PRUnichar('\'') : PRUnichar('"');
    AppendToString(PRUnichar('='), aStr);
    AppendToString(cDelimiter, aStr);
    if (bIncludesDouble && bIncludesSingle) {
      nsAutoString sValue(aValue);
      sValue.ReplaceSubstring(NS_LITERAL_STRING("\"").get(), NS_LITERAL_STRING("&quot;").get());
      mInAttribute = PR_TRUE;
      AppendToString(sValue, aStr, PR_FALSE);
      mInAttribute = PR_FALSE;
    }
    else {
      mInAttribute = PR_TRUE;
      AppendToString(aValue, aStr, PR_FALSE);
      mInAttribute = PR_FALSE;
    }
    AppendToString(cDelimiter, aStr);
  }
}

NS_IMETHODIMP 
nsXMLContentSerializer::AppendElementStart(nsIDOMElement *aElement,
                                           PRBool aHasChildren,
                                           nsAString& aStr)
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

  PRInt32 namespaceID;
    
  PRUint32 index, count;
  nsAutoString nameStr, prefixStr, uriStr, valueStr;
  nsCOMPtr<nsIAtom> attrName, attrPrefix;

  count = content->GetAttrCount();

  // First scan for namespace declarations, pushing each on the stack
  for (index = 0; index < count; index++) {
    
    content->GetAttrNameAt(index,
                           &namespaceID,
                           getter_AddRefs(attrName),
                           getter_AddRefs(attrPrefix));
    
    if (namespaceID == kNameSpaceID_XMLNS) {
      content->GetAttr(namespaceID, attrName, uriStr);

      if (!attrPrefix) {
        // Default NS attribute does not have prefix (and the name is "xmlns")
        PushNameSpaceDecl(nsString(), uriStr, aElement);
      } else {
        attrName->ToString(nameStr);
        PushNameSpaceDecl(nameStr, uriStr, aElement);
      }
    }
  }

  PRBool addNSAttr;
    
  MaybeAddNewline(aStr);

  addNSAttr = ConfirmPrefix(tagPrefix, tagNamespaceURI);
  // Serialize the qualified name of the element
  AppendToString(NS_LITERAL_STRING("<"), aStr);
  if (!tagPrefix.IsEmpty()) {
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
  for (index = 0; index < count; index++) {
    content->GetAttrNameAt(index,
                           &namespaceID,
                           getter_AddRefs(attrName),
                           getter_AddRefs(attrPrefix));

    if (attrPrefix) {
      attrPrefix->ToString(prefixStr);
    }
    else {
      prefixStr.Truncate();
    }

    addNSAttr = PR_FALSE;
    if (kNameSpaceID_XMLNS != namespaceID) {
      nsContentUtils::GetNSManagerWeakRef()->GetNameSpaceURI(namespaceID,
                                                             uriStr);
      addNSAttr = ConfirmPrefix(prefixStr, uriStr);
    }
    
    content->GetAttr(namespaceID, attrName, valueStr);
    attrName->ToString(nameStr);

    // XXX Hack to get around the fact that MathML can add
    //     attributes starting with '-', which makes them
    //     invalid XML.
    if (!nameStr.IsEmpty() && nameStr.First() == '-')
      continue;

    if (namespaceID == kNameSpaceID_None) {
      PRInt32 elementNsID;
      content->GetNameSpaceID(&elementNsID);
      if (elementNsID == kNameSpaceID_XHTML) {
        if (IsShorthandAttr(attrName, content->Tag()) &&
            valueStr.IsEmpty()) {
          valueStr = nameStr;
        }
      }
    }
    SerializeAttr(prefixStr, nameStr, valueStr, aStr, PR_TRUE);
    
    if (addNSAttr) {
      SerializeAttr(xmlnsStr, prefixStr, uriStr, aStr, PR_TRUE);
      PushNameSpaceDecl(prefixStr, uriStr, aElement);
    }
  }

  // We don't output a separate end tag for empty element
  if (!aHasChildren) {
    AppendToString(NS_LITERAL_STRING("/>"), aStr);    
    MaybeFlagNewline(aElement);
  } else {
    AppendToString(NS_LITERAL_STRING(">"), aStr);    
  }
  
  return NS_OK;
}

NS_IMETHODIMP 
nsXMLContentSerializer::AppendElementEnd(nsIDOMElement *aElement,
                                         nsAString& aStr)
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

  ConfirmPrefix(tagPrefix, tagNamespaceURI);

  AppendToString(NS_LITERAL_STRING("</"), aStr);
  if (!tagPrefix.IsEmpty()) {
    AppendToString(tagPrefix, aStr);
    AppendToString(NS_LITERAL_STRING(":"), aStr);
  }
  AppendToString(tagLocalName, aStr);
  AppendToString(NS_LITERAL_STRING(">"), aStr);
  MaybeFlagNewline(aElement);
  
  PopNameSpaceDeclsFor(aElement);
  
  return NS_OK;
}

void
nsXMLContentSerializer::AppendToString(const PRUnichar* aStr,
                                       PRInt32 aLength,
                                       nsAString& aOutputStr)
{
  PRInt32 length = (aLength == -1) ? nsCRT::strlen(aStr) : aLength;
  
  aOutputStr.Append(aStr, length);
}

void 
nsXMLContentSerializer::AppendToString(const PRUnichar aChar,
                                       nsAString& aOutputStr)
{
  aOutputStr.Append(aChar);
}

static const PRUint16 kGTVal = 62;
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
  "", "", "", "", "&quot;", "", "", "", "&amp;", "",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  "&lt;", "", "&gt;"
};

void
nsXMLContentSerializer::AppendToString(const nsAString& aStr,
                                       nsAString& aOutputStr,
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
        AppendASCIItoUTF16(entityText, aOutputStr);
        advanceLength++;
      }
    }

    return;
  }
  
  aOutputStr.Append(aStr);
}

PRBool
nsXMLContentSerializer::IsShorthandAttr(const nsIAtom* aAttrName,
                                        const nsIAtom* aElementName)
{
  // checked
  if ((aAttrName == nsHTMLAtoms::checked) &&
      (aElementName == nsHTMLAtoms::input)) {
    return PR_TRUE;
  }

  // compact
  if ((aAttrName == nsHTMLAtoms::compact) &&
      (aElementName == nsHTMLAtoms::dir || 
       aElementName == nsHTMLAtoms::dl ||
       aElementName == nsHTMLAtoms::menu ||
       aElementName == nsHTMLAtoms::ol ||
       aElementName == nsHTMLAtoms::ul)) {
    return PR_TRUE;
  }

  // declare
  if ((aAttrName == nsHTMLAtoms::declare) &&
      (aElementName == nsHTMLAtoms::object)) {
    return PR_TRUE;
  }

  // defer
  if ((aAttrName == nsHTMLAtoms::defer) &&
      (aElementName == nsHTMLAtoms::script)) {
    return PR_TRUE;
  }

  // disabled
  if ((aAttrName == nsHTMLAtoms::disabled) &&
      (aElementName == nsHTMLAtoms::button ||
       aElementName == nsHTMLAtoms::input ||
       aElementName == nsHTMLAtoms::optgroup ||
       aElementName == nsHTMLAtoms::option ||
       aElementName == nsHTMLAtoms::select ||
       aElementName == nsHTMLAtoms::textarea)) {
    return PR_TRUE;
  }

  // ismap
  if ((aAttrName == nsHTMLAtoms::ismap) &&
      (aElementName == nsHTMLAtoms::img ||
       aElementName == nsHTMLAtoms::input)) {
    return PR_TRUE;
  }

  // multiple
  if ((aAttrName == nsHTMLAtoms::multiple) &&
      (aElementName == nsHTMLAtoms::select)) {
    return PR_TRUE;
  }

  // noresize
  if ((aAttrName == nsHTMLAtoms::noresize) &&
      (aElementName == nsHTMLAtoms::frame)) {
    return PR_TRUE;
  }

  // noshade
  if ((aAttrName == nsHTMLAtoms::noshade) &&
      (aElementName == nsHTMLAtoms::hr)) {
    return PR_TRUE;
  }

  // nowrap
  if ((aAttrName == nsHTMLAtoms::nowrap) &&
      (aElementName == nsHTMLAtoms::td ||
       aElementName == nsHTMLAtoms::th)) {
    return PR_TRUE;
  }

  // readonly
  if ((aAttrName == nsHTMLAtoms::readonly) &&
      (aElementName == nsHTMLAtoms::input ||
       aElementName == nsHTMLAtoms::textarea)) {
    return PR_TRUE;
  }

  // selected
  if ((aAttrName == nsHTMLAtoms::selected) &&
      (aElementName == nsHTMLAtoms::option)) {
    return PR_TRUE;
  }

  return PR_FALSE;
}

void
nsXMLContentSerializer::MaybeAddNewline(nsAString& aStr)
{
  if (mAddNewline) {
    aStr.Append((PRUnichar)'\n');
    mAddNewline = PR_FALSE;
  }
}

void
nsXMLContentSerializer::MaybeFlagNewline(nsIDOMNode* aNode)
{
  nsCOMPtr<nsIDOMNode> parent;
  aNode->GetParentNode(getter_AddRefs(parent));
  if (parent) {
    PRUint16 type;
    parent->GetNodeType(&type);
    mAddNewline = type == nsIDOMNode::DOCUMENT_NODE;
  }
}

NS_IMETHODIMP
nsXMLContentSerializer::AppendDocumentStart(nsIDOMDocument *aDocument,
                                            nsAString& aStr)
{
  NS_ENSURE_ARG_POINTER(aDocument);

  nsCOMPtr<nsIDocument> doc(do_QueryInterface(aDocument));
  if (!doc) {
    return NS_OK;
  }

  nsAutoString version, encoding, standalone;
  doc->GetXMLDeclaration(version, encoding, standalone);

  if (version.IsEmpty())
    return NS_OK; // A declaration must have version, or there is no decl

  NS_NAMED_LITERAL_STRING(endQuote, "\"");

  aStr += NS_LITERAL_STRING("<?xml version=\"") + version + endQuote;
  
  if (!encoding.IsEmpty()) {
    aStr += NS_LITERAL_STRING(" encoding=\"") + encoding + endQuote;
  }

  if (!standalone.IsEmpty()) {
    aStr += NS_LITERAL_STRING(" standalone=\"") + standalone + endQuote;
  }

  aStr += NS_LITERAL_STRING("?>");
  mAddNewline = PR_TRUE;

  return NS_OK;
}
