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

#define kIndentStr NS_LITERAL_STRING("  ")
#define kMozStr "_moz"
#define kLessThan NS_LITERAL_STRING("<")
#define kGreaterThan NS_LITERAL_STRING(">")
#define kEndTag NS_LITERAL_STRING("</")

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
  else {
    mMaxColumn = aWrapColumn;
  }

  mDoFormat = (mFlags & nsIDocumentEncoder::OutputFormatted) ? PR_TRUE
                                                             : PR_FALSE;
  mBodyOnly = (mFlags & nsIDocumentEncoder::OutputBodyOnly) ? PR_TRUE
                                                            : PR_FALSE;
  // Set the line break character:
  if ((mFlags & nsIDocumentEncoder::OutputCRLineBreak)
      && (mFlags & nsIDocumentEncoder::OutputLFLineBreak)) { // Windows/mail
    mLineBreak.AssignWithConversion("\r\n");
  }
  else if (mFlags & nsIDocumentEncoder::OutputCRLineBreak) { // Mac
    mLineBreak.AssignWithConversion("\r");
  }
  else if (mFlags & nsIDocumentEncoder::OutputLFLineBreak) { // Unix/DOM
    mLineBreak.AssignWithConversion("\n");
  }
  else {
    mLineBreak.AssignWithConversion(NS_LINEBREAK);         // Platform/default
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
                      aEndOffset, data, PR_TRUE, PR_FALSE);
  if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

  PRInt32 lastNewlineOffset = kNotFound;
  PRBool hasLongLines = HasLongLines(data, lastNewlineOffset);

  if (mPreLevel || (!mDoFormat && !hasLongLines)) {
    AppendToString(data, aStr);

    if (lastNewlineOffset != kNotFound) {
      mColPos = data.Length() - lastNewlineOffset;
    }   
  }
  else {
    AppendToStringWrapped(data, aStr, PR_FALSE);
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
    AppendToString(mLineBreak, aStr);
    mColPos = 0;
  }

  StartIndentation(name, hasDirtyAttr, aStr);

  if ((name.get() == nsHTMLAtoms::pre) ||
      (name.get() == nsHTMLAtoms::script) ||
      (name.get() == nsHTMLAtoms::style)) {
    mPreLevel++;
  }
  
  AppendToString(kLessThan, aStr);

  nsXPIDLString sharedName;
  name->GetUnicode(getter_Shares(sharedName));
  AppendToString(sharedName, -1, aStr);

  SerializeAttributes(content, name, aStr);

  AppendToString(kGreaterThan, aStr);

  if (LineBreakAfterOpen(name, hasDirtyAttr)) {
    AppendToString(mLineBreak, aStr);
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

  if ((name.get() == nsHTMLAtoms::pre) ||
      (name.get() == nsHTMLAtoms::script) ||
      (name.get() == nsHTMLAtoms::style)) {
    mPreLevel--;
  }

  nsXPIDLString sharedName;
  name->GetUnicode(getter_Shares(sharedName));

  nsCOMPtr<nsIParserService> parserService;
  GetParserService(getter_AddRefs(parserService));
  if (parserService) {
    nsAutoString nameStr(sharedName);
    PRBool isContainer;
    PRInt32 id;

    parserService->HTMLStringTagToId(nameStr, &id);
    parserService->IsContainer(id, isContainer);
    if (!isContainer) return NS_OK;
  }

  if (LineBreakBeforeClose(name, hasDirtyAttr)) {
    AppendToString(mLineBreak, aStr);
    mColPos = 0;
  }

  EndIndentation(name, hasDirtyAttr, aStr);

  AppendToString(kEndTag, aStr);
  AppendToString(sharedName, -1, aStr);
  AppendToString(kGreaterThan, aStr);

  if (LineBreakAfterClose(name, hasDirtyAttr)) {
    AppendToString(mLineBreak, aStr);
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

  aOutputStr.Append(aStr, length);
}

void 
nsHTMLContentSerializer::AppendToString(const PRUnichar aChar,
                                        nsAWritableString& aOutputStr)
{
  if (mBodyOnly && !mInBody) {
    return;
  }

  mColPos += 1;

  aOutputStr.Append(aChar);
}

void 
nsHTMLContentSerializer::AppendToStringWrapped(const nsAReadableString& aStr,
                                               nsAWritableString& aOutputStr,
                                               PRBool aTranslateEntities)
{
  PRInt32 length = aStr.Length();

  nsAutoString line;
  PRBool    done = PR_FALSE;
  PRInt32   indx = 0;
  PRInt32   strOffset = 0;
  PRInt32   oldLineOffset = 0;
  PRInt32   lineLength, oldLineEnd;
  
  // Make sure we haven't gone too far already
  if (mColPos > mMaxColumn) {
    AppendToString(mLineBreak, aOutputStr);
    mColPos = 0;
  }
  
  // Find the end of the first old line
  oldLineEnd = aStr.FindChar(PRUnichar('\n'), 0);
  
  while ((!done) && (strOffset < length)) {
    // This is how much is needed to fill up the new line
    PRInt32 leftInLine = mMaxColumn - mColPos;
    
    // This is the last position in the current old line
    PRInt32 oldLineLimit;
    if (oldLineEnd == kNotFound) {
      oldLineLimit = length;
    }
    else {
      oldLineLimit = oldLineEnd;
    }
    
    PRBool addLineBreak = PR_FALSE;

    // if we can fill up the new line with less than what's
    // in the current old line...
    if ((strOffset + leftInLine) < oldLineLimit) {
      addLineBreak = PR_TRUE;
      
      // Look for the next word end to break
      indx = aStr.FindChar(PRUnichar(' '), strOffset + leftInLine);
      
      // If it's after the end of the current line, then break at
      // the current line
      if ((indx == kNotFound) || 
          ((oldLineEnd != kNotFound) && (oldLineEnd < indx))) {
        indx = oldLineEnd;
      }
    }
    else {
      indx = oldLineEnd;
    }
    
    // if there was no place to break, then just add the entire string
    if (indx == kNotFound) {
      if (strOffset == 0) {
        AppendToString(aStr, aOutputStr, aTranslateEntities);
      }
      else {
        lineLength = length - strOffset;
        aStr.Right(line, lineLength);
        AppendToString(line, aOutputStr, aTranslateEntities);
      }
      done = PR_TRUE;
    }
    else {
      // Add the part of the current old line that's part of the 
      // new line
      lineLength = indx - strOffset;
      aStr.Mid(line, strOffset, lineLength);
      AppendToString(line, aOutputStr, aTranslateEntities);
      
      // if we've reached the end of an old line, don't add the
      // old line break and find the end of the next old line.
      if (indx == oldLineEnd) {
        oldLineEnd = aStr.FindChar(PRUnichar('\n'), indx+1);
      }
      
      if (addLineBreak) {
        AppendToString(mLineBreak, aOutputStr);
        mColPos = 0;
      }
      strOffset = indx+1;
    }
  }
}

void
nsHTMLContentSerializer::AppendToString(const nsAReadableString& aStr,
                                        nsAWritableString& aOutputStr,
                                        PRBool aTranslateEntities,
                                        PRBool aIncrColumn)
{
  if (mBodyOnly && !mInBody) {
    return;
  }

  nsresult rv;

  if (aIncrColumn) {
    mColPos += aStr.Length();
  }
  
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
        
  if (aName == nsHTMLAtoms::title ||
      aName == nsHTMLAtoms::meta  ||
      aName == nsHTMLAtoms::link  ||
      aName == nsHTMLAtoms::style ||
      aName == nsHTMLAtoms::script ||
      aName == nsHTMLAtoms::html) {
    return PR_TRUE;
  }
  else {
    nsCOMPtr<nsIParserService> parserService;
    GetParserService(getter_AddRefs(parserService));
    
    if (parserService) {
      nsAutoString str;
      aName->ToString(str);
      PRBool res;
      PRInt32 id;

      parserService->HTMLStringTagToId(str, &id);
      parserService->IsBlock(id, res);
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
      (aName == nsHTMLAtoms::dl) ||
      (aName == nsHTMLAtoms::table) ||
      (aName == nsHTMLAtoms::tbody) ||
      (aName == nsHTMLAtoms::tr) ||
      (aName == nsHTMLAtoms::br) ||
      (aName == nsHTMLAtoms::meta) ||
      (aName == nsHTMLAtoms::link) ||
      (aName == nsHTMLAtoms::script) ||
      (aName == nsHTMLAtoms::style)) {
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
      (aName == nsHTMLAtoms::dl) ||
      (aName == nsHTMLAtoms::table) ||
      (aName == nsHTMLAtoms::tbody)) {
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
      (aName == nsHTMLAtoms::li) ||
      (aName == nsHTMLAtoms::dt) ||
      (aName == nsHTMLAtoms::dd) ||
      (aName == nsHTMLAtoms::blockquote) ||
      (aName == nsHTMLAtoms::p) ||
      (aName == nsHTMLAtoms::div)) {
    return PR_TRUE;
  }
  else {
    nsCOMPtr<nsIParserService> parserService;
    GetParserService(getter_AddRefs(parserService));
    
    if (parserService) {
      nsAutoString str;
      aName->ToString(str);
      PRBool res;
      PRInt32 id;

      parserService->HTMLStringTagToId(str, &id);
      parserService->IsBlock(id, res);
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
      AppendToString(kIndentStr, -1, 
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
      (aName == nsHTMLAtoms::frameset) ||
      (aName == nsHTMLAtoms::blockquote) ||
      (aName == nsHTMLAtoms::li) ||
      (aName == nsHTMLAtoms::dt) ||
      (aName == nsHTMLAtoms::dd)) {
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
      (aName == nsHTMLAtoms::li) ||
      (aName == nsHTMLAtoms::tbody) ||
      (aName == nsHTMLAtoms::form) ||
      (aName == nsHTMLAtoms::frameset)) {
    mIndent--;
  }

  if ((mDoFormat || aHasDirtyAttr) && !mPreLevel && !mColPos) {
    for (PRInt32 i = mIndent; --i >= 0; ) {
      AppendToString(kIndentStr, -1,
                     aStr);
    }
  }
}

// See if the string has any lines longer than longLineLen:
// if so, we presume formatting is wonky (e.g. the node has been edited)
// and we'd better rewrap the whole text node.
PRBool 
nsHTMLContentSerializer::HasLongLines(const nsString& text, PRInt32& aLastNewlineOffset)
{
  PRUint32 start=0;
  PRUint32 theLen=text.Length();
  PRBool rv = PR_FALSE;
  aLastNewlineOffset = kNotFound;
  for (start = 0; start < theLen; )
  {
    PRInt32 eol = text.FindChar('\n', PR_FALSE, start);
    if (eol < 0) {
      eol = text.Length();
    }
    else {
      aLastNewlineOffset = eol;
    }
    if (PRInt32(eol - start) > kLongLineLen)
      rv = PR_TRUE;
    start = eol+1;
  }
  return rv;
}
