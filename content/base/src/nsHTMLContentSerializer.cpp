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

#include "nsHTMLContentSerializer.h"

#include "nsIDOMElement.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsINameSpaceManager.h"
#include "nsString.h"
#include "nsUnicharUtils.h"
#include "nsXPIDLString.h"
#include "nsIServiceManager.h"
#include "nsIDocumentEncoder.h"
#include "nsLayoutAtoms.h"
#include "nsHTMLAtoms.h"
#include "nsIURI.h"
#include "nsNetUtil.h"
#include "nsEscape.h"
#include "nsITextToSubURI.h"
#include "nsCRT.h"
#include "nsIHTMLContent.h"
#include "nsIParserService.h"
#include "nsContentUtils.h"

#define kIndentStr NS_LITERAL_STRING("  ")
#define kLessThan NS_LITERAL_STRING("<")
#define kGreaterThan NS_LITERAL_STRING(">")
#define kEndTag NS_LITERAL_STRING("</")

static const PRUnichar kMozStr[] = {'m', 'o', 'z', 0};
static const PRInt32 kMozStrLength = 3;

static const PRInt32 kLongLineLen = 128;

nsresult NS_NewHTMLContentSerializer(nsIContentSerializer** aSerializer)
{
  nsHTMLContentSerializer* it = new nsHTMLContentSerializer();
  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return CallQueryInterface(it, aSerializer);
}

nsHTMLContentSerializer::nsHTMLContentSerializer()
{
  mColPos = 0;
  mIndent = 0;
  mAddSpace = PR_FALSE;
  mInBody = PR_FALSE;
  mInCDATA = PR_FALSE;
}

nsHTMLContentSerializer::~nsHTMLContentSerializer()
{
  NS_ASSERTION(mOLStateStack.Count() == 0, "Expected OL State stack to be empty");
  if (mOLStateStack.Count() > 0){
    for (PRInt32 i = 0; i < mOLStateStack.Count(); i++){
      olState* state = (olState*)mOLStateStack[i];
      delete state;
      mOLStateStack.RemoveElementAt(i);
    }
  }
}

NS_IMETHODIMP 
nsHTMLContentSerializer::Init(PRUint32 aFlags, PRUint32 aWrapColumn,
                              nsIAtom* aCharSet, PRBool aIsCopying)
{
  mFlags = aFlags;
  if (!aWrapColumn) {
    mMaxColumn = 72;
  }
  else {
    mMaxColumn = aWrapColumn;
  }

  mIsCopying = aIsCopying;
  mIsFirstChildOfOL = PR_FALSE;
  mDoFormat = (mFlags & nsIDocumentEncoder::OutputFormatted) ? PR_TRUE
                                                             : PR_FALSE;
  mBodyOnly = (mFlags & nsIDocumentEncoder::OutputBodyOnly) ? PR_TRUE
                                                            : PR_FALSE;
  // Set the line break character:
  if ((mFlags & nsIDocumentEncoder::OutputCRLineBreak)
      && (mFlags & nsIDocumentEncoder::OutputLFLineBreak)) { // Windows
    mLineBreak.Assign(NS_LITERAL_STRING("\r\n"));
  }
  else if (mFlags & nsIDocumentEncoder::OutputCRLineBreak) { // Mac
    mLineBreak.Assign(NS_LITERAL_STRING("\r"));
  }
  else if (mFlags & nsIDocumentEncoder::OutputLFLineBreak) { // Unix/DOM
    mLineBreak.Assign(NS_LITERAL_STRING("\n"));
  }
  else {
    mLineBreak.AssignWithConversion(NS_LINEBREAK);         // Platform/default
  }

  mPreLevel = 0;

  mCharSet = aCharSet;
  mIsLatin1 = PR_FALSE;
  if (aCharSet) {
    const PRUnichar *charset;
    aCharSet->GetUnicode(&charset);

    if (NS_LITERAL_STRING("ISO-8859-1").Equals(charset)) {
      mIsLatin1 = PR_TRUE;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP 
nsHTMLContentSerializer::AppendText(nsIDOMText* aText, 
                                    PRInt32 aStartOffset,
                                    PRInt32 aEndOffset,
                                    nsAString& aStr)
{
  NS_ENSURE_ARG(aText);

  nsAutoString data;

  nsresult rv;
  rv = AppendTextData((nsIDOMNode*)aText, aStartOffset, 
                      aEndOffset, data, PR_TRUE, PR_FALSE);
  if (NS_FAILED(rv))
    return NS_ERROR_FAILURE;

  if (mPreLevel > 0) {
    AppendToStringConvertLF(data, aStr);
  }
  else if (!mDoFormat) {
    PRInt32 lastNewlineOffset = kNotFound;
    PRBool hasLongLines = HasLongLines(data, lastNewlineOffset);
    if (hasLongLines) {
      // We have long lines, rewrap
      AppendToStringWrapped(data, aStr, PR_FALSE);
      if (lastNewlineOffset != kNotFound)
        mColPos = data.Length() - lastNewlineOffset;
    }
    else {
      AppendToStringConvertLF(data, aStr);
    }
  }
  else if (mFlags & nsIDocumentEncoder::OutputRaw) {
    PRInt32 lastNewlineOffset = data.RFindChar('\n');
    AppendToString(data, aStr);
    if (lastNewlineOffset != kNotFound)
      mColPos = data.Length() - lastNewlineOffset;
  }
  else {
    AppendToStringWrapped(data, aStr, PR_FALSE);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLContentSerializer::AppendDocumentStart(nsIDOMDocument *aDocument,
                                             nsAString& aStr)
{
  return NS_OK;
}

PRBool
nsHTMLContentSerializer::IsJavaScript(nsIAtom* aAttrNameAtom, const nsAString& aValueString)
{
  if (aAttrNameAtom == nsHTMLAtoms::href
  || aAttrNameAtom == nsHTMLAtoms::src) {
    // note that there is a problem in that if this value starts with leading spaces we won't do the right thing
    // this is covered in bug #59604
    static const char kJavaScript[] = "javascript";
    PRInt32 pos = aValueString.FindChar(':');
    const nsAutoString scheme(Substring(aValueString, 0, pos));
    if ((pos == (PRInt32)(sizeof kJavaScript - 1)) &&
        scheme.EqualsIgnoreCase(kJavaScript))
      return PR_TRUE;
    else
      return PR_FALSE;  
  }

  PRBool result = 
                 (aAttrNameAtom == nsLayoutAtoms::onblur)      || (aAttrNameAtom == nsLayoutAtoms::onchange)
              || (aAttrNameAtom == nsLayoutAtoms::onclick)     || (aAttrNameAtom == nsLayoutAtoms::ondblclick)
              || (aAttrNameAtom == nsLayoutAtoms::onfocus)     || (aAttrNameAtom == nsLayoutAtoms::onkeydown)
              || (aAttrNameAtom == nsLayoutAtoms::onkeypress)  || (aAttrNameAtom == nsLayoutAtoms::onkeyup)
              || (aAttrNameAtom == nsLayoutAtoms::onload)      || (aAttrNameAtom == nsLayoutAtoms::onmousedown)
              || (aAttrNameAtom == nsLayoutAtoms::onmousemove) || (aAttrNameAtom == nsLayoutAtoms::onmouseout)
              || (aAttrNameAtom == nsLayoutAtoms::onmouseover) || (aAttrNameAtom == nsLayoutAtoms::onmouseup)
              || (aAttrNameAtom == nsLayoutAtoms::onreset)     || (aAttrNameAtom == nsLayoutAtoms::onselect)
              || (aAttrNameAtom == nsLayoutAtoms::onsubmit)    || (aAttrNameAtom == nsLayoutAtoms::onunload)
              || (aAttrNameAtom == nsLayoutAtoms::onabort)     || (aAttrNameAtom == nsLayoutAtoms::onerror)
              || (aAttrNameAtom == nsLayoutAtoms::onpaint)     || (aAttrNameAtom == nsLayoutAtoms::onresize)
              || (aAttrNameAtom == nsLayoutAtoms::onscroll)    || (aAttrNameAtom == nsLayoutAtoms::onbroadcast)
              || (aAttrNameAtom == nsLayoutAtoms::onclose)     || (aAttrNameAtom == nsLayoutAtoms::oncontextmenu)
              || (aAttrNameAtom == nsLayoutAtoms::oncommand)   || (aAttrNameAtom == nsLayoutAtoms::oncommandupdate)
              || (aAttrNameAtom == nsLayoutAtoms::ondragdrop)  || (aAttrNameAtom == nsLayoutAtoms::ondragenter)
              || (aAttrNameAtom == nsLayoutAtoms::ondragexit)  || (aAttrNameAtom == nsLayoutAtoms::ondraggesture)
              || (aAttrNameAtom == nsLayoutAtoms::ondragover)  || (aAttrNameAtom == nsLayoutAtoms::oninput);
  return result;
}

nsresult 
nsHTMLContentSerializer::EscapeURI(const nsAString& aURI, nsAString& aEscapedURI)
{
  // URL escape %xx cannot be used in JS.
  // No escaping if the scheme is 'javascript'.
  if (IsJavaScript(nsHTMLAtoms::href, aURI)) {
    aEscapedURI = aURI;
    return NS_OK;
  }

  // nsITextToSubURI does charset convert plus uri escape
  // This is needed to convert to a document charset which is needed to support existing browsers.
  // But we eventually want to use UTF-8 instead of a document charset, then the code would be much simpler.
  // See HTML 4.01 spec, "Appendix B.2.1 Non-ASCII characters in URI attribute values"
  nsCOMPtr<nsITextToSubURI> textToSubURI;
  nsAutoString uri(aURI); // in order to use FindCharInSet(), IsASCII()
  nsXPIDLCString documentCharset;
  nsresult rv = NS_OK;


  if (mCharSet && !uri.IsASCII()) {
    const PRUnichar *charset;
    mCharSet->GetUnicode(&charset);
    documentCharset.Adopt(ToNewUTF8String(nsDependentString(charset)));
    textToSubURI = do_GetService(NS_ITEXTTOSUBURI_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  PRInt32 start = 0;
  PRInt32 end;
  nsAutoString part;
  nsXPIDLCString escapedURI;
  aEscapedURI.Truncate(0);

  // Loop and escape parts by avoiding escaping reserved characters (and '%', '#' ).
  while ((end = uri.FindCharInSet("%#;/?:@&=+$,", start)) != -1) {
    part = Substring(aURI, start, (end-start));
    if (textToSubURI && !part.IsASCII()) {
      rv = textToSubURI->ConvertAndEscape(documentCharset, part.get(), getter_Copies(escapedURI));
      NS_ENSURE_SUCCESS(rv, rv);
    }
    else {
      escapedURI.Adopt(nsEscape(NS_ConvertUCS2toUTF8(part).get(), url_Path));
    }
    aEscapedURI.Append(NS_ConvertASCIItoUCS2(escapedURI));

    // Append a reserved character without escaping.
    part = Substring(aURI, end, 1);
    aEscapedURI.Append(part);
    start = end + 1;
  }

  if (start < (PRInt32) aURI.Length()) {
    // Escape the remaining part.
    part = Substring(aURI, start, aURI.Length()-start);
    if (textToSubURI) {
      rv = textToSubURI->ConvertAndEscape(documentCharset, part.get(), getter_Copies(escapedURI));
      NS_ENSURE_SUCCESS(rv, rv);
    }
    else {
      escapedURI.Adopt(nsEscape(NS_ConvertUCS2toUTF8(part).get(), url_Path));
    }
    aEscapedURI.Append(NS_ConvertASCIItoUCS2(escapedURI));
  }

  return rv;
}

void 
nsHTMLContentSerializer::SerializeAttributes(nsIContent* aContent,
                                             nsIAtom* aTagName,
                                             nsAString& aStr)
{
  nsresult rv;
  PRInt32 index, count;
  nsAutoString nameStr, valueStr;
  PRInt32 namespaceID;
  nsCOMPtr<nsIAtom> attrName, attrPrefix;

  aContent->GetAttrCount(count);

  for (index = 0; index < count; index++) {
    aContent->GetAttrNameAt(index, 
                            namespaceID,
                            *getter_AddRefs(attrName),
                            *getter_AddRefs(attrPrefix));

    // Filter out any attribute starting with [-|_]moz
    const PRUnichar* sharedName;
    attrName->GetUnicode(&sharedName);
    if ((('_' == *sharedName) || ('-' == *sharedName)) &&
        !nsCRT::strncmp(sharedName+1, kMozStr, kMozStrLength)) {
      continue;
    }

    aContent->GetAttr(namespaceID, attrName, valueStr);

    // 
    // Filter out special case of <br type="_moz"> or <br _moz*>,
    // used by the editor.  Bug 16988.  Yuck.
    //
    if ((aTagName == nsHTMLAtoms::br) &&
        (attrName.get() == nsHTMLAtoms::type) &&
        !valueStr.IsEmpty() && ('_' == valueStr[0]) &&
        !nsCRT::strncmp(valueStr.get()+1, kMozStr, kMozStrLength)) {
      continue;
    }
    
    // XXX: This special cased textarea code should be
    //      removed when bug #17003 is fixed.  
    if ( (aTagName == nsHTMLAtoms::textarea) &&
         ((attrName.get() == nsHTMLAtoms::value) || 
         (attrName.get() == nsHTMLAtoms::defaultvalue)) ){
        continue;
    }

    if ( mIsCopying && mIsFirstChildOfOL && (aTagName == nsHTMLAtoms::li) && 
         (attrName.get() == nsHTMLAtoms::value)){
      // This is handled separately in SerializeLIValueAttribute()
      continue;
    }
    PRBool isJS = IsJavaScript(attrName, valueStr);
    
    if (((attrName.get() == nsHTMLAtoms::href) || 
         (attrName.get() == nsHTMLAtoms::src))) {
      // Make all links absolute when converting only the selection:
      if (mFlags & nsIDocumentEncoder::OutputAbsoluteLinks) {
        // Would be nice to handle OBJECT and APPLET tags,
        // but that gets more complicated since we have to
        // search the tag list for CODEBASE as well.
        // For now, just leave them relative.
        nsCOMPtr<nsIDocument> document;
        aContent->GetDocument(*getter_AddRefs(document));
        if (document) {
          nsCOMPtr<nsIURI> uri;
          document->GetBaseURL(*getter_AddRefs(uri));
          if (!uri)
            document->GetDocumentURL(getter_AddRefs(uri));
          if (uri) {
            nsAutoString absURI;
            rv = NS_MakeAbsoluteURI(absURI, valueStr, uri);
            if (NS_SUCCEEDED(rv)) {
              valueStr = absURI;
            }
          }
        }
      }
      // Need to escape URI.
      nsAutoString tempURI(valueStr);
      if (!isJS && NS_FAILED(EscapeURI(tempURI, valueStr)))
        valueStr = tempURI;
    }

    attrName->ToString(nameStr);
    
    /*If we already crossed the MaxColumn limit or 
    * if this attr name-value pair(including a space,=,opening and closing quotes) is greater than MaxColumn limit
    * then start the attribute from a new line.
    */

    if (mDoFormat && (mColPos >= mMaxColumn || ((mColPos + nameStr.Length() + valueStr.Length() + 4) > mMaxColumn))) {
        aStr.Append(mLineBreak);
        mColPos = 0;
    }

    // Expand shorthand attribute.
    if (IsShorthandAttr(attrName, aTagName) && valueStr.IsEmpty()) {
      valueStr = nameStr;
    }
    SerializeAttr(nsAutoString(), nameStr, valueStr, aStr, !isJS);
  }
}

NS_IMETHODIMP
nsHTMLContentSerializer::AppendElementStart(nsIDOMElement *aElement,
                                            PRBool aHasChildren,
                                            nsAString& aStr)
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

  if (name.get() == nsHTMLAtoms::br && mPreLevel > 0
      && (mFlags & nsIDocumentEncoder::OutputNoFormattingInPre)) {
    AppendToString(mLineBreak, aStr);
    mColPos = 0;
    return NS_OK;
  }

  if (name.get() == nsHTMLAtoms::body) {
    mInBody = PR_TRUE;
  }

  if (LineBreakBeforeOpen(name, hasDirtyAttr)) {
    AppendToString(mLineBreak, aStr);
    mColPos = 0;
    mAddSpace = PR_FALSE;
  }
  else if (mAddSpace) {
    AppendToString(PRUnichar(' '), aStr);
    mAddSpace = PR_FALSE;
  }

  StartIndentation(name, hasDirtyAttr, aStr);

  if ((name.get() == nsHTMLAtoms::pre) ||
      (name.get() == nsHTMLAtoms::script) ||
      (name.get() == nsHTMLAtoms::style)) {
    mPreLevel++;
  }
  
  AppendToString(kLessThan, aStr);

  const PRUnichar* sharedName;
  name->GetUnicode(&sharedName);
  AppendToString(sharedName, -1, aStr);

  // Need to keep track of OL and LI elements in order to get ordinal number 
  // for the LI.
  if (mIsCopying && name.get() == nsHTMLAtoms::ol){
    // We are copying and current node is an OL;
    // Store it's start attribute value in olState->startVal.
    nsAutoString start;
    PRInt32 startAttrVal = 0;
    aElement->GetAttribute(NS_LITERAL_STRING("start"), start);
    if (!start.IsEmpty()){
      PRInt32 rv = 0;
      startAttrVal = start.ToInteger(&rv);
      //If OL has "start" attribute, first LI element has to start with that value
      //Therefore subtracting 1 as all the LI elements are incrementing it before using it;
      //In failure of ToInteger(), default StartAttrValue to 0.
      if (NS_SUCCEEDED(rv))
        startAttrVal--; 
      else
        startAttrVal = 0;
    }
    olState* state = new olState(startAttrVal, PR_TRUE);
    if (state)
      mOLStateStack.AppendElement(state);
  }

  if (mIsCopying && name.get() == nsHTMLAtoms::li) {
    mIsFirstChildOfOL = IsFirstChildOfOL(aElement);
    if (mIsFirstChildOfOL){
      // If OL is parent of this LI, serialize attributes in different manner.
      SerializeLIValueAttribute(aElement, aStr);
    }
  }

  // Even LI passed above have to go through this 
  // for serializing attributes other than "value".
  SerializeAttributes(content, name, aStr);

  AppendToString(kGreaterThan, aStr);

  if (LineBreakAfterOpen(name, hasDirtyAttr)) {
    AppendToString(mLineBreak, aStr);
    mColPos = 0;
  }

  // XXX: This special cased textarea code should be
  //      removed when bug #17003 is fixed.  
  if (name.get() == nsHTMLAtoms::textarea)
  {
    nsAutoString valueStr;
    content->GetAttr(kNameSpaceID_None, nsHTMLAtoms::value, valueStr);
    AppendToString(valueStr, aStr);
  }

  if ((name.get() == nsHTMLAtoms::script) ||
      (name.get() == nsHTMLAtoms::style) ||
      (name.get() == nsHTMLAtoms::noscript) ||
      (name.get() == nsHTMLAtoms::noframes)) {
    mInCDATA = PR_TRUE;
  }

  return NS_OK;
}
  
NS_IMETHODIMP 
nsHTMLContentSerializer::AppendElementEnd(nsIDOMElement *aElement,
                                          nsAString& aStr)
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

  if (mIsCopying && (name.get() == nsHTMLAtoms::ol)){
    NS_ASSERTION((mOLStateStack.Count() > 0), "Cannot have an empty OL Stack");
    /* Though at this point we must always have an state to be deleted as all 
    the OL opening tags are supposed to push an olState object to the stack*/
    if (mOLStateStack.Count() > 0) {
      olState* state = (olState*)mOLStateStack.ElementAt(mOLStateStack.Count() -1);
      mOLStateStack.RemoveElementAt(mOLStateStack.Count() -1);
      delete state;
    }
  }
  
  const PRUnichar* sharedName;
  name->GetUnicode(&sharedName);

  nsIParserService* parserService = nsContentUtils::GetParserServiceWeakRef();

  if (parserService && (name.get() != nsHTMLAtoms::style)) {
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
    mAddSpace = PR_FALSE;
  }
  else if (mAddSpace) {
    AppendToString(PRUnichar(' '), aStr);
    mAddSpace = PR_FALSE;
  }

  EndIndentation(name, hasDirtyAttr, aStr);

  AppendToString(kEndTag, aStr);
  AppendToString(sharedName, -1, aStr);
  AppendToString(kGreaterThan, aStr);

  if (LineBreakAfterClose(name, hasDirtyAttr)) {
    AppendToString(mLineBreak, aStr);
    mColPos = 0;
  }

  mInCDATA = PR_FALSE;

  return NS_OK;
}

void
nsHTMLContentSerializer::AppendToString(const PRUnichar* aStr,
                                        PRInt32 aLength,
                                        nsAString& aOutputStr)
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
                                        nsAString& aOutputStr)
{
  if (mBodyOnly && !mInBody) {
    return;
  }

  mColPos += 1;

  aOutputStr.Append(aChar);
}

void 
nsHTMLContentSerializer::AppendToStringWrapped(const nsASingleFragmentString& aStr,
                                               nsAString& aOutputStr,
                                               PRBool aTranslateEntities)
{
  // indicates a space has been seen, position is stored in lastSpace
  PRBool    spaceSeen = PR_FALSE;

  // indicates non-whitespace has been seen, position is stored in lastChar
  PRBool    charSeen = PR_FALSE;

  PRBool    addLineBreak = PR_FALSE;

  nsASingleFragmentString::const_char_iterator pos, end, segStart, lastSpace, lastChar;

  aStr.BeginReading(pos);
  aStr.EndReading(end);
  if (pos == end) {
    return;
  }

  // if the current line already has text on it, such as a tag,
  // leading whitespace is significant, so add a space back
  // after skipping over the whitespace
  if ((mColPos > 0) && (*pos == ' ' || *pos == '\n')) {
    mAddSpace = PR_TRUE;
  }

  for (;;) {

    // skip leading spaces
    while (*pos == ' ' || *pos == '\n') {
      ++pos;
      if (pos == end) {
        return;
      }
    }
    segStart = pos;
    lastChar = pos;
    spaceSeen = PR_FALSE;
    charSeen = PR_TRUE;

    if (addLineBreak) {
      aOutputStr.Append(mLineBreak);
      mAddSpace = PR_FALSE;
      mColPos = 0;
    }

    while (mColPos < mMaxColumn) {
      PRUnichar c = *pos;

      if (c == ' ') {
        lastSpace = pos;
        spaceSeen = PR_TRUE;
      }
      else if (c == '\n') {
        if (charSeen) {
          if (mAddSpace) {
            aOutputStr.Append(PRUnichar(' '));
          }

          aOutputStr.Append(segStart, lastChar - segStart + 1);
          charSeen = PR_FALSE;
        }
        mAddSpace = PR_TRUE;
        segStart = pos;
        spaceSeen = PR_FALSE;
        ++segStart;
      }
      else {
        lastChar = pos;
        charSeen = PR_TRUE;
      }

      ++pos;
      ++mColPos;

      if (pos == end) {
        if (!charSeen || pos == segStart) {
          // nothing to append, or nothing meaningful to append
          return;
        }
        if (mAddSpace) {
          aOutputStr.Append(PRUnichar(' '));
          mAddSpace = PR_FALSE;
        }

        aOutputStr.Append(segStart, lastChar - segStart + 1);

        // if the string ended in whitespace, set mAddSpace to true
        if (pos != lastChar+1) {
          mAddSpace = PR_TRUE;
        }
        return;
      }
    }

    if (spaceSeen) {
      if (mAddSpace) {
        aOutputStr.Append(PRUnichar(' '));
        mAddSpace = PR_FALSE;
      }

      // write up to the last space encountered
      aOutputStr.Append(segStart, lastSpace - segStart);

      // back up to that wrapping point for the next run through the loop
      pos = lastSpace;
      // add a line break before any more text
      addLineBreak = PR_TRUE;
    }
    else {

      // if we're past the wrapping width with no place to wrap at,
      // find the next whitespace and wrap there
      while (pos != end && *pos != ' ' && *pos != '\n') {
        ++pos;
      }

      if (mAddSpace) {
        // whitespace was needed before the next segment, so we can put
        // a newline instead of a space, and avoid getting a lone line
        aOutputStr.Append(mLineBreak);
        addLineBreak = PR_FALSE;

        mColPos = pos - segStart;

        // if the string doesn't end in whitespace, set mAddSpace to false
        if (pos == end) {
          mAddSpace = PR_FALSE;
        }
      }
      else {
        // no choice but to write a long line and wrap immediately after it
        addLineBreak = PR_TRUE;
      }
      aOutputStr.Append(segStart, pos - segStart);

      if (pos == end) {
        return;
      }
    }
  }
}

static PRUint16 kValNBSP = 160;
static const char* kEntityNBSP = "nbsp";

static PRUint16 kGTVal = 62;
static const char* kEntities[] = {
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "amp", "",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  "lt", "", "gt"
};

static const char* kAttrEntities[] = {
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "quot", "", "", "", "amp", "",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  "lt", "", "gt"
};

void
nsHTMLContentSerializer::AppendToString(const nsAString& aStr,
                                        nsAString& aOutputStr,
                                        PRBool aTranslateEntities,
                                        PRBool aIncrColumn)
{
  if (mBodyOnly && !mInBody) {
    return;
  }

  if (aIncrColumn) {
    mColPos += aStr.Length();
  }

  if (aTranslateEntities && !mInCDATA) {
    if (mFlags & nsIDocumentEncoder::OutputEncodeEntities) {
      nsIParserService* parserService =
        nsContentUtils::GetParserServiceWeakRef();

      if (!parserService) {
        NS_ERROR("Can't get parser service");
        return;
      }

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
        nsCAutoString entityReplacement;

        advanceLength = 0;
        // for each character in this chunk, check if it
        // needs to be replaced
        for (; c < fragmentEnd; c++, advanceLength++) {
          PRUnichar val = *c;
          if (val == kValNBSP) {
            entityText = kEntityNBSP;
            break;
          }
          else if ((val <= kGTVal) && (entityTable[val][0] != 0)) {
            entityText = entityTable[val];
            break;
          } else if (mIsLatin1 && val > 127 && val < 256) {
            parserService->HTMLConvertUnicodeToEntity(val, entityReplacement);

            if (!entityReplacement.IsEmpty()) {
              entityText = entityReplacement.get();
              break;
            }
          }
        }

        aOutputStr.Append(fragmentStart, advanceLength);
        if (entityText) {
          aOutputStr.Append(PRUnichar('&'));
          aOutputStr.Append(NS_ConvertASCIItoUCS2(entityText));
          aOutputStr.Append(PRUnichar(';'));
          advanceLength++;
        }
      }
    } else {
      nsXMLContentSerializer::AppendToString(aStr, aOutputStr, aTranslateEntities, aIncrColumn);
    }

    return;
  }

  aOutputStr.Append(aStr);
}

void
nsHTMLContentSerializer::AppendToStringConvertLF(const nsAString& aStr,
                                                 nsAString& aOutputStr)
{
  // Convert line-endings to mLineBreak
  PRUint32 start = 0;
  PRUint32 theLen = aStr.Length();
  while (start < theLen) {
    PRInt32 eol = aStr.FindChar('\n', start);
    if (eol == kNotFound) {
      nsDependentSubstring dataSubstring(aStr, start, theLen - start);
      AppendToString(dataSubstring, aOutputStr);
      start = theLen;
    }
    else {
      nsDependentSubstring dataSubstring(aStr, start, eol - start);
      AppendToString(dataSubstring, aOutputStr);
      AppendToString(mLineBreak, aOutputStr);
      start = eol + 1;
      if (start == theLen)
        mColPos = 0;
    }
  }
}

PRBool
nsHTMLContentSerializer::HasDirtyAttr(nsIContent* aContent)
{
  nsAutoString val;

  if (NS_CONTENT_ATTR_NOT_THERE != aContent->GetAttr(kNameSpaceID_None,
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
  if ((!mDoFormat && !aHasDirtyAttr) || mPreLevel || !mColPos ||
      (mFlags & nsIDocumentEncoder::OutputRaw)) {
    return PR_FALSE;
  }
        
  if (aName == nsHTMLAtoms::title ||
      aName == nsHTMLAtoms::meta  ||
      aName == nsHTMLAtoms::link  ||
      aName == nsHTMLAtoms::style ||
      aName == nsHTMLAtoms::select ||
      aName == nsHTMLAtoms::option ||
      aName == nsHTMLAtoms::script ||
      aName == nsHTMLAtoms::html) {
    return PR_TRUE;
  }
  else {
    nsIParserService* parserService =
      nsContentUtils::GetParserServiceWeakRef();
    
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
  if ((!mDoFormat && !aHasDirtyAttr) || mPreLevel ||
      (mFlags & nsIDocumentEncoder::OutputRaw)) {
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
      (aName == nsHTMLAtoms::select) ||
      (aName == nsHTMLAtoms::map) ||
      (aName == nsHTMLAtoms::area) ||
      (aName == nsHTMLAtoms::style)) {
    return PR_TRUE;
  }

  return PR_FALSE;
}

PRBool 
nsHTMLContentSerializer::LineBreakBeforeClose(nsIAtom* aName, 
                                              PRBool aHasDirtyAttr)
{
  if ((!mDoFormat && !aHasDirtyAttr) || mPreLevel || !mColPos ||
      (mFlags & nsIDocumentEncoder::OutputRaw)) {
    return PR_FALSE;
  }

  if ((aName == nsHTMLAtoms::html) ||
      (aName == nsHTMLAtoms::head) ||
      (aName == nsHTMLAtoms::body) ||
      (aName == nsHTMLAtoms::ul) ||
      (aName == nsHTMLAtoms::ol) ||
      (aName == nsHTMLAtoms::dl) ||
      (aName == nsHTMLAtoms::select) ||
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
  if ((!mDoFormat && !aHasDirtyAttr) || mPreLevel ||
      (mFlags & nsIDocumentEncoder::OutputRaw)) {
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
      (aName == nsHTMLAtoms::select) ||
      (aName == nsHTMLAtoms::option) ||
      (aName == nsHTMLAtoms::p) ||
      (aName == nsHTMLAtoms::map) ||
      (aName == nsHTMLAtoms::div)) {
    return PR_TRUE;
  }
  else {
    nsIParserService* parserService =
      nsContentUtils::GetParserServiceWeakRef();
    
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
                                          nsAString& aStr)
{
  if ((mDoFormat || aHasDirtyAttr) && !mPreLevel && !mColPos) {
    for (PRInt32 i = mIndent; --i >= 0; ) {
      AppendToString(kIndentStr, aStr);
    }
  }

  if ((aName == nsHTMLAtoms::head) ||
      (aName == nsHTMLAtoms::table) ||
      (aName == nsHTMLAtoms::tr) ||
      (aName == nsHTMLAtoms::ul) ||
      (aName == nsHTMLAtoms::ol) ||
      (aName == nsHTMLAtoms::dl) ||
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
                                        nsAString& aStr)
{
  if ((aName == nsHTMLAtoms::head) ||
      (aName == nsHTMLAtoms::table) ||
      (aName == nsHTMLAtoms::tr) ||
      (aName == nsHTMLAtoms::ul) ||
      (aName == nsHTMLAtoms::ol) ||
      (aName == nsHTMLAtoms::dl) ||
      (aName == nsHTMLAtoms::li) ||
      (aName == nsHTMLAtoms::tbody) ||
      (aName == nsHTMLAtoms::form) ||
      (aName == nsHTMLAtoms::blockquote) ||
      (aName == nsHTMLAtoms::dt) ||
      (aName == nsHTMLAtoms::dd) ||
      (aName == nsHTMLAtoms::frameset)) {
    mIndent--;
  }

  if ((mDoFormat || aHasDirtyAttr) && !mPreLevel && !mColPos) {
    for (PRInt32 i = mIndent; --i >= 0; ) {
      AppendToString(kIndentStr, aStr);
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
    PRInt32 eol = text.FindChar('\n', start);
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

void 
nsHTMLContentSerializer::SerializeLIValueAttribute(nsIDOMElement* aElement,
                                                   nsAString& aStr)
{
  // We are copying and we are at the "first" LI node of OL in selected range.
  // It may not be the first LI child of OL but it's first in the selected range.
  // Note that we get into this condition only once per a OL.
  nsCOMPtr<nsIDOMNode> node = do_QueryInterface(aElement);
  PRBool found = PR_FALSE;
  nsIDOMNode* currNode = node;
  nsAutoString valueStr;
  PRInt32 offset = 0;
  olState defaultOLState(0, PR_FALSE);
  olState* state = nsnull;
  if (mOLStateStack.Count() > 0) 
    state = (olState*)mOLStateStack.ElementAt(mOLStateStack.Count()-1);
  /* Though we should never reach to a "state" as null or mOLStateStack.Count() == 0 
  at this point as all LI are supposed to be inside some OL and OL tag should have 
  pushed a state to the olStateStack.*/
  if (!state || mOLStateStack.Count() == 0)
    state = &defaultOLState;
  PRInt32 startVal = state->startVal;
  state->isFirstListItem = PR_FALSE;
  // Traverse previous siblings until we find one with "value" attribute.
  // offset keeps track of how many previous siblings we had tocurrNode traverse.
  while (currNode && !found) {
    nsCOMPtr<nsIDOMElement> currElement = do_QueryInterface(currNode);
    // currElement may be null if it were a text node.
    if (currElement) {
      nsAutoString tagName;
      currElement->GetTagName(tagName);
      if (tagName.EqualsIgnoreCase("LI")) {
        currElement->GetAttribute(NS_LITERAL_STRING("value"), valueStr);
        if (valueStr.IsEmpty())
          offset++;
        else {
          found = PR_TRUE;
          PRInt32 rv = 0;
          startVal = valueStr.ToInteger(&rv); 
        }
      }
    }
    currNode->GetPreviousSibling(&currNode);
  }
  // If LI was not having "value", Set the "value" attribute for it.
  // Note that We are at the first LI in the selected range of OL.
  if (offset == 0 && found) {
    // offset = 0 => LI itself has the value attribute and we did not need to traverse back.
    // Just serialize value attribute like other tags.
    SerializeAttr(nsAutoString(), NS_LITERAL_STRING("value"), valueStr, aStr, PR_FALSE);
  }
  else if (offset == 1 && !found) {
    /*(offset = 1 && !found) means either LI is the first child node of OL 
    and LI is not having "value" attribute. 
    In that case we would not like to set "value" attribute to reduce the changes.
    */
     //do nothing...
  }
  else if (offset > 0) {
    // Set value attribute.
    nsAutoString valueStr;

    //As serializer needs to use this valueAttr we are creating here, 
    valueStr.AppendInt(startVal + offset);
    SerializeAttr(nsAutoString(), NS_LITERAL_STRING("value"), valueStr, aStr, PR_FALSE);
  }
}

PRBool
nsHTMLContentSerializer::IsFirstChildOfOL(nsIDOMElement* aElement){
  nsCOMPtr<nsIDOMNode> node = do_QueryInterface(aElement);
  nsIDOMNode* parentNode;
  nsAutoString parentName;
  node->GetParentNode(&parentNode);
  if (parentNode)
    parentNode->GetNodeName(parentName);
  else
    return PR_FALSE;
  
  if (parentName.EqualsIgnoreCase("OL")) {
    olState defaultOLState(0, PR_FALSE);
    olState* state = nsnull;
    if (mOLStateStack.Count() > 0) 
      state = (olState*)mOLStateStack.ElementAt(mOLStateStack.Count()-1);
    /* Though we should never reach to a "state" as null at this point as 
    all LI are supposed to be inside some OL and OL tag should have pushed
    a state to the mOLStateStack.*/
    if (!state)
      state = &defaultOLState;
    
    if (state->isFirstListItem)
      return PR_TRUE;

    return PR_FALSE;
  }
  else
    return PR_FALSE;
}
