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

#include "nsHTMLContentSerializer.h"

#include "nsIDOMElement.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsINameSpaceManager.h"
#include "nsString.h"
#include "nsXPIDLString.h"
#include "nsParserCIID.h"
#include "nsIServiceManager.h"
#include "nsIDocumentEncoder.h"
#include "nsLayoutAtoms.h"
#include "nsHTMLAtoms.h"
#include "nsIURI.h"
#include "nsNetUtil.h"

static NS_DEFINE_CID(kParserServiceCID, NS_PARSERSERVICE_CID);
static NS_DEFINE_CID(kEntityConverterCID, NS_ENTITYCONVERTER_CID);

#define kIndentStr "  "
#define kMozStr "_moz"
#define kLessThan "<"
#define kGreaterThan ">"
#define kEndTag "</"

static const PRInt32 kLongLineLen = 128;

nsresult NS_NewHTMLContentSerializer(nsIContentSerializer** aSerializer)
{
  nsHTMLContentSerializer* it = new nsHTMLContentSerializer();
  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return it->QueryInterface(NS_GET_IID(nsIContentSerializer), (void**)aSerializer);
}

nsHTMLContentSerializer::nsHTMLContentSerializer()
{
  mColPos = 0;
  mIndent = 0;
  mInBody = PR_FALSE;
}

nsHTMLContentSerializer::~nsHTMLContentSerializer()
{
}

nsresult
nsHTMLContentSerializer::GetEntityConverter(nsIEntityConverter** aConverter)
{
  if (!mEntityConverter) {
    nsresult rv;
    rv = nsComponentManager::CreateInstance(kEntityConverterCID, NULL, 
                                            NS_GET_IID(nsIEntityConverter),
                                            getter_AddRefs(mEntityConverter));
    if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
  }

  CallQueryInterface(mEntityConverter.get(), aConverter);
  return NS_OK;
}

nsresult
nsHTMLContentSerializer::GetParserService(nsIParserService** aParserService)
{
  if (!mParserService) {
    nsresult rv;
    mParserService = do_GetService(kParserServiceCID, &rv);
    if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
  }

  CallQueryInterface(mParserService.get(), aParserService);
  return NS_OK;
}

NS_IMETHODIMP 
nsHTMLContentSerializer::Init(PRUint32 aFlags, PRUint32 aWrapColumn)
{
  mFlags = aFlags;
  if (!aWrapColumn) {
    mMaxColumn = 72;
  }

  mDoFormat = (mFlags & nsIDocumentEncoder::OutputFormatted) ? PR_TRUE
                                                             : PR_FALSE;
  mBodyOnly = (mFlags & nsIDocumentEncoder::OutputBodyOnly) ? PR_TRUE
                                                            : PR_FALSE;
  // Set the line break character:
  if ((mFlags & nsIDocumentEncoder::OutputCRLineBreak)
      && (mFlags & nsIDocumentEncoder::OutputLFLineBreak)) { // Windows/mail
    mLineBreak.AssignWithConversion("\r\n");
    mLineBreakLen = 2;
  }
  else if (mFlags & nsIDocumentEncoder::OutputCRLineBreak) { // Mac
    mLineBreak.AssignWithConversion("\r");
    mLineBreakLen = 1;
  }
  else if (mFlags & nsIDocumentEncoder::OutputLFLineBreak) { // Unix/DOM
    mLineBreak.AssignWithConversion("\n");
    mLineBreakLen = 1;
  }
  else {
    mLineBreak.AssignWithConversion(NS_LINEBREAK);         // Platform/default
    mLineBreakLen = NS_LINEBREAK_LEN;
  }

  mPreLevel = 0;

  return NS_OK;
}

NS_IMETHODIMP 
nsHTMLContentSerializer::AppendText(nsIDOMText* aText, 
                                    PRInt32 aStartOffset,
                                    PRInt32 aEndOffset,
                                    nsAWritableString& aStr)
{
  NS_ENSURE_ARG(aText);

  nsAutoString data;

  nsresult rv;
  rv = AppendTextData((nsIDOMNode*)aText, aStartOffset, 
                      aEndOffset, data);
  if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

  if (mPreLevel || (!mDoFormat && !HasLongLines(data))) {
    AppendToString(data,
                   data.Length(),
                   aStr,
                   PR_TRUE);
  }
  else {
    AppendToStringWrapped(data,
                          data.Length(),
                          aStr,
                          PR_TRUE);
  }

  return NS_OK;
}


void 
nsHTMLContentSerializer::SerializeAttributes(nsIContent* aContent,
                                             nsIAtom* aTagName,
                                             nsAWritableString& aStr)
{
  nsresult rv;
  PRInt32 index, count;
  nsAutoString nameStr, valueStr;
  PRInt32 namespaceID;
  nsCOMPtr<nsIAtom> attrName, attrPrefix;

  aContent->GetAttributeCount(count);

  for (index = 0; index < count; index++) {
    aContent->GetAttributeNameAt(index, 
                                 namespaceID,
                                 *getter_AddRefs(attrName),
                                 *getter_AddRefs(attrPrefix));

    // Filter out any attribute starting with _moz
    nsXPIDLString sharedName;
    attrName->GetUnicode(getter_Shares(sharedName));
    if (nsCRT::strncmp(sharedName, 
                       NS_ConvertASCIItoUCS2(kMozStr), 
                       sizeof(kMozStr)-1) == 0) {
      continue;
    }

    aContent->GetAttribute(namespaceID, attrName, valueStr);

    // 
    // Filter out special case of <br type="_moz"> or <br _moz*>,
    // used by the editor.  Bug 16988.  Yuck.
    //
    if ((aTagName == nsHTMLAtoms::br) &&
        (attrName.get() == nsHTMLAtoms::type) &&
        (valueStr.EqualsWithConversion(kMozStr, PR_FALSE, sizeof(kMozStr)-1))) {
      continue;
    }

    // Make all links absolute when converting only the selection:
    if ((mFlags & nsIDocumentEncoder::OutputAbsoluteLinks) &&
        ((attrName.get() == nsHTMLAtoms::href) || 
         (attrName.get() == nsHTMLAtoms::src))) {
      // Would be nice to handle OBJECT and APPLET tags,
      // but that gets more complicated since we have to
      // search the tag list for CODEBASE as well.
      // For now, just leave them relative.
      nsCOMPtr<nsIDocument> document;
      aContent->GetDocument(*getter_AddRefs(document));
      if (document) {
        nsCOMPtr<nsIURI> uri = dont_AddRef(document->GetDocumentURL());
        if (uri) {
          nsAutoString absURI;
          rv = NS_MakeAbsoluteURI(absURI, valueStr, uri);
          if (NS_SUCCEEDED(rv)) {
            valueStr = absURI;
          }
        }
      }
    }

    attrName->ToString(nameStr);
    
    SerializeAttr(nsAutoString(), nameStr, valueStr, aStr);
  }
}

NS_IMETHODIMP
nsHTMLContentSerializer::AppendElementStart(nsIDOMElement *aElement,
                                            nsAWritableString& aStr)
{
  NS_ENSURE_ARG(aElement);
  
  nsCOMPtr<nsIContent> content = do_QueryInterface(aElement);
  if (!content) return NS_ERROR_FAILURE;
  
  // The _moz_dirty attribute is emitted by the editor to
  // indicate that this element should be pretty printed
  // even if we're not in pretty printing mode
  PRBool hasDirtyAttr = HasDirtyAttr(content);

  nsCOMPtr<nsIAtom> name;
  content->GetTag(*getter_AddRefs(name));

  if (name.get() == nsHTMLAtoms::body) {
    mInBody = PR_TRUE;
  }

  if (LineBreakBeforeOpen(name, hasDirtyAttr)) {
    AppendToString(mLineBreak, mLineBreakLen, aStr);
    mColPos = 0;
  }

  if (name.get() == nsHTMLAtoms::pre) {
    mPreLevel++;
  }
  
  StartIndentation(name, hasDirtyAttr, aStr);

  AppendToString(NS_LITERAL_STRING(kLessThan), -1, aStr);

  nsXPIDLString sharedName;
  name->GetUnicode(getter_Shares(sharedName));
  AppendToString(sharedName, nsCRT::strlen(sharedName), aStr);

  SerializeAttributes(content, name, aStr);

  AppendToString(NS_LITERAL_STRING(kGreaterThan), -1, aStr);

  if (LineBreakAfterOpen(name, hasDirtyAttr)) {
    AppendToString(mLineBreak, mLineBreakLen, aStr);
    mColPos = 0;
  }
    
  return NS_OK;
}
  
NS_IMETHODIMP 
nsHTMLContentSerializer::AppendElementEnd(nsIDOMElement *aElement,
                                          nsAWritableString& aStr)
{
  NS_ENSURE_ARG(aElement);

  nsCOMPtr<nsIContent> content = do_QueryInterface(aElement);
  if (!content) return NS_ERROR_FAILURE;

  PRBool hasDirtyAttr = HasDirtyAttr(content);

  nsCOMPtr<nsIAtom> name;
  content->GetTag(*getter_AddRefs(name));

  if (name.get() == nsHTMLAtoms::pre) {
    mPreLevel--;
  }

  nsXPIDLString sharedName;
  name->GetUnicode(getter_Shares(sharedName));

  nsCOMPtr<nsIParserService> parserService;
  GetParserService(getter_AddRefs(parserService));
  if (parserService) {
    nsAutoString nameStr(sharedName);
    PRBool isContainer;

    parserService->IsContainer(nameStr, isContainer);
    if (!isContainer) return NS_OK;
  }

  if (LineBreakBeforeClose(name, hasDirtyAttr)) {
    AppendToString(mLineBreak, mLineBreakLen, aStr);
    mColPos = 0;
  }

  EndIndentation(name, hasDirtyAttr, aStr);

  AppendToString(NS_LITERAL_STRING(kEndTag), -1, aStr);
  AppendToString(sharedName, -1, aStr);
  AppendToString(NS_LITERAL_STRING(kGreaterThan), -1, aStr);

  if (LineBreakAfterClose(name, hasDirtyAttr)) {
    AppendToString(mLineBreak, mLineBreakLen, aStr);
    mColPos = 0;
  }

  return NS_OK;
}

void
nsHTMLContentSerializer::AppendToString(const PRUnichar* aStr,
                                        PRInt32 aLength,
                                        nsAWritableString& aOutputStr)
{
  if (mBodyOnly && !mInBody) {
    return;
  }

  PRInt32 length = (aLength == -1) ? nsCRT::strlen(aStr) : aLength;
  
  mColPos += length;

  aOutputStr.Append(aStr);
}

void 
nsHTMLContentSerializer::AppendToStringWrapped(const nsAReadableString& aStr,
                                               PRInt32 aLength,
                                               nsAWritableString& aOutputStr,
                                               PRBool aTranslateEntities)
{
  PRInt32 length = (aLength == -1) ? aStr.Length() : aLength;

  if ((mColPos + length) < mMaxColumn) {
    AppendToString(aStr, length, aOutputStr, aTranslateEntities);
  }
  else {
    nsAutoString line;
    PRBool    done = PR_FALSE;
    PRInt32   indx = 0;
    PRInt32   strOffset = 0;
    PRInt32   lineLength;
    
    while ((!done) && (strOffset < length)) {
      // find the next break
      PRInt32 start = mMaxColumn - mColPos;
      if (start < 0)
        start = 0;
      
      indx = aStr.FindChar(PRUnichar(' '), strOffset + start);
      // if there is no break than just add the entire string
      if (indx == kNotFound)
      {
        if (strOffset == 0) {
          AppendToString(aStr, length, aOutputStr, aTranslateEntities);
        }
        else {
          lineLength = length - strOffset;
          aStr.Right(line, lineLength);
          AppendToString(line, lineLength, 
                         aOutputStr, aTranslateEntities);
        }
        done = PR_TRUE;
      }
      else {
        lineLength = indx - strOffset;
        aStr.Mid(line, strOffset, lineLength);
        AppendToString(line, lineLength, 
                       aOutputStr, aTranslateEntities);
        AppendToString(mLineBreak, mLineBreakLen, aOutputStr);
        strOffset = indx+1;
        mColPos = 0;
      }
    }
  }
}

void
nsHTMLContentSerializer::AppendToString(const nsAReadableString& aStr,
                                        PRInt32 aLength,
                                        nsAWritableString& aOutputStr,
                                        PRBool aTranslateEntities)
{
  if (mBodyOnly && !mInBody) {
    return;
  }

  nsresult rv;
  PRInt32 length = (aLength == -1) ? aStr.Length() : aLength;
  
  mColPos += length;

  if (aTranslateEntities) {
    nsCOMPtr<nsIEntityConverter> converter;

    GetEntityConverter(getter_AddRefs(converter));
    if (converter) {
      PRUnichar *encodedBuffer;
      rv = mEntityConverter->ConvertToEntities(nsPromiseFlatString(aStr),
                                               nsIEntityConverter::html40Latin1,
                                               &encodedBuffer);
      if (NS_SUCCEEDED(rv)) {
        aOutputStr.Append(encodedBuffer);
        nsCRT::free(encodedBuffer);
        return;
      }
    }
  }
  
  aOutputStr.Append(aStr);
}

PRBool
nsHTMLContentSerializer::HasDirtyAttr(nsIContent* aContent)
{
  nsAutoString val;

  if (NS_CONTENT_ATTR_NOT_THERE != aContent->GetAttribute(kNameSpaceID_None,
                                                          nsLayoutAtoms::mozdirty,
                                                          val)) {
    return PR_TRUE;
  }
  else {
    return PR_FALSE;
  }
}

PRBool 
nsHTMLContentSerializer::LineBreakBeforeOpen(nsIAtom* aName, 
                                             PRBool aHasDirtyAttr)
{
  if ((!mDoFormat && !aHasDirtyAttr) || mPreLevel || !mColPos) {
    return PR_FALSE;
  }
        
  if (aName == nsHTMLAtoms::html) {
    return PR_FALSE;
  }
  else if (aName == nsHTMLAtoms::title) {
    return PR_TRUE;
  }
  else {
    nsCOMPtr<nsIParserService> parserService;
    GetParserService(getter_AddRefs(parserService));
    
    if (parserService) {
      nsAutoString str;
      aName->ToString(str);
      PRBool res;
      parserService->IsBlock(str, res);
      return res;
    }
  }

  return PR_FALSE;
}

PRBool 
nsHTMLContentSerializer::LineBreakAfterOpen(nsIAtom* aName, 
                                            PRBool aHasDirtyAttr)
{
  if ((!mDoFormat && !aHasDirtyAttr) || mPreLevel) {
    return PR_FALSE;
  }

  if ((aName == nsHTMLAtoms::html) ||
      (aName == nsHTMLAtoms::head) ||
      (aName == nsHTMLAtoms::body) ||
      (aName == nsHTMLAtoms::ul) ||
      (aName == nsHTMLAtoms::ol) ||
      (aName == nsHTMLAtoms::table) ||
      (aName == nsHTMLAtoms::tbody) ||
      (aName == nsHTMLAtoms::style) ||
      (aName == nsHTMLAtoms::tr) ||
      (aName == nsHTMLAtoms::br)) {
    return PR_TRUE;
  }

  return PR_FALSE;
}

PRBool 
nsHTMLContentSerializer::LineBreakBeforeClose(nsIAtom* aName, 
                                              PRBool aHasDirtyAttr)
{
  if ((!mDoFormat && !aHasDirtyAttr) || mPreLevel || !mColPos) {
    return PR_FALSE;
  }

  if ((aName == nsHTMLAtoms::html) ||
      (aName == nsHTMLAtoms::head) ||
      (aName == nsHTMLAtoms::body) ||
      (aName == nsHTMLAtoms::ul) ||
      (aName == nsHTMLAtoms::ol) ||
      (aName == nsHTMLAtoms::table) ||
      (aName == nsHTMLAtoms::tbody) ||
      (aName == nsHTMLAtoms::style)) {
    return PR_TRUE;
  }
  
  return PR_FALSE;
}

PRBool 
nsHTMLContentSerializer::LineBreakAfterClose(nsIAtom* aName, 
                                             PRBool aHasDirtyAttr)
{
  if ((!mDoFormat && !aHasDirtyAttr) || mPreLevel) {
    return PR_FALSE;
  }

  if ((aName == nsHTMLAtoms::html) ||
      (aName == nsHTMLAtoms::head) ||
      (aName == nsHTMLAtoms::body) ||
      (aName == nsHTMLAtoms::tr) ||
      (aName == nsHTMLAtoms::th) ||
      (aName == nsHTMLAtoms::td) ||
      (aName == nsHTMLAtoms::pre) ||
      (aName == nsHTMLAtoms::title) ||
      (aName == nsHTMLAtoms::meta)) {
    return PR_TRUE;
  }
  else {
    nsCOMPtr<nsIParserService> parserService;
    GetParserService(getter_AddRefs(parserService));
    
    if (parserService) {
      nsAutoString str;
      aName->ToString(str);
      PRBool res;
      parserService->IsBlock(str, res);
      return res;
    }
  }

  return PR_FALSE;
}

void
nsHTMLContentSerializer::StartIndentation(nsIAtom* aName,
                                          PRBool aHasDirtyAttr,
                                          nsAWritableString& aStr)
{
  if ((mDoFormat || aHasDirtyAttr) && !mPreLevel && !mColPos) {
    for (PRInt32 i = mIndent; --i >= 0; ) {
      AppendToString(NS_ConvertASCIItoUCS2(kIndentStr), -1, 
                     aStr);
    }
  }

  if ((aName == nsHTMLAtoms::head) ||
      (aName == nsHTMLAtoms::table) ||
      (aName == nsHTMLAtoms::tr) ||
      (aName == nsHTMLAtoms::ul) ||
      (aName == nsHTMLAtoms::ol) ||
      (aName == nsHTMLAtoms::tbody) ||
      (aName == nsHTMLAtoms::form) ||
      (aName == nsHTMLAtoms::frameset)) {
    mIndent++;
  }
}

void
nsHTMLContentSerializer::EndIndentation(nsIAtom* aName,
                                        PRBool aHasDirtyAttr,
                                        nsAWritableString& aStr)
{
  if ((aName == nsHTMLAtoms::head) ||
      (aName == nsHTMLAtoms::table) ||
      (aName == nsHTMLAtoms::tr) ||
      (aName == nsHTMLAtoms::ul) ||
      (aName == nsHTMLAtoms::ol) ||
      (aName == nsHTMLAtoms::tbody) ||
      (aName == nsHTMLAtoms::form) ||
      (aName == nsHTMLAtoms::frameset)) {
    mIndent--;
  }

  if ((mDoFormat || aHasDirtyAttr) && !mPreLevel && !mColPos) {
    for (PRInt32 i = mIndent; --i >= 0; ) {
      AppendToString(NS_ConvertASCIItoUCS2(kIndentStr), -1,
                     aStr);
    }
  }
}

// See if the string has any lines longer than longLineLen:
// if so, we presume formatting is wonky (e.g. the node has been edited)
// and we'd better rewrap the whole text node.
PRBool 
nsHTMLContentSerializer::HasLongLines(const nsString& text)
{
  PRUint32 start=0;
  PRUint32 theLen=text.Length();
  for (start = 0; start < theLen; )
  {
    PRInt32 eol = text.FindChar('\n', PR_FALSE, start);
    if (eol < 0) eol = text.Length();
    if (PRInt32(eol - start) > kLongLineLen)
      return PR_TRUE;
    start = eol+1;
  }
  return PR_FALSE;
}
