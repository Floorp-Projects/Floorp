/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=80: */
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

/*
 * nsIContentSerializer implementation that can be used with an
 * nsIDocumentEncoder to convert an HTML (not XHTML!) DOM to an HTML
 * string that could be parsed into more or less the original DOM.
 */

#include "nsHTMLContentSerializer.h"

#include "nsIDOMElement.h"
#include "nsIDOMText.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsINameSpaceManager.h"
#include "nsString.h"
#include "nsUnicharUtils.h"
#include "nsXPIDLString.h"
#include "nsIServiceManager.h"
#include "nsIDocumentEncoder.h"
#include "nsGkAtoms.h"
#include "nsIURI.h"
#include "nsNetUtil.h"
#include "nsEscape.h"
#include "nsITextToSubURI.h"
#include "nsCRT.h"
#include "nsIParserService.h"
#include "nsContentUtils.h"
#include "nsLWBrkCIID.h"
#include "nsIScriptElement.h"
#include "nsAttrName.h"

#define kIndentStr NS_LITERAL_STRING("  ")
#define kLessThan NS_LITERAL_STRING("<")
#define kGreaterThan NS_LITERAL_STRING(">")
#define kEndTag NS_LITERAL_STRING("</")

static const char kMozStr[] = "moz";

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
: mIndent(0),
  mColPos(0),
  mInBody(PR_FALSE),
  mAddSpace(PR_FALSE),
  mMayIgnoreLineBreakSequence(PR_FALSE),
  mInCDATA(PR_FALSE),
  mNeedLineBreaker(PR_TRUE)
{
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
                              const char* aCharSet, PRBool aIsCopying)
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
    mLineBreak.AssignLiteral("\r\n");
  }
  else if (mFlags & nsIDocumentEncoder::OutputCRLineBreak) { // Mac
    mLineBreak.AssignLiteral("\r");
  }
  else if (mFlags & nsIDocumentEncoder::OutputLFLineBreak) { // Unix/DOM
    mLineBreak.AssignLiteral("\n");
  }
  else {
    mLineBreak.AssignLiteral(NS_LINEBREAK);         // Platform/default
  }

  mPreLevel = 0;

  mCharset = aCharSet;

  // set up entity converter if we are going to need it
  if (mFlags & nsIDocumentEncoder::OutputEncodeW3CEntities) {
    mEntityConverter = do_CreateInstance(NS_ENTITYCONVERTER_CONTRACTID);
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

  if (mNeedLineBreaker) {
    mNeedLineBreaker = PR_FALSE;

    nsCOMPtr<nsIDOMDocument> domDoc;
    aText->GetOwnerDocument(getter_AddRefs(domDoc));
    nsCOMPtr<nsIDocument> document = do_QueryInterface(domDoc);
  }

  nsAutoString data;

  nsresult rv;
  rv = AppendTextData((nsIDOMNode*)aText, aStartOffset, 
                      aEndOffset, data, PR_TRUE, PR_FALSE);
  if (NS_FAILED(rv))
    return NS_ERROR_FAILURE;

  if (mPreLevel > 0) {
    AppendToStringConvertLF(data, aStr);
  }
  else if (mFlags & nsIDocumentEncoder::OutputRaw) {
    PRInt32 lastNewlineOffset = data.RFindChar('\n');
    AppendToString(data, aStr);
    if (lastNewlineOffset != kNotFound)
      mColPos = data.Length() - lastNewlineOffset;
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
  else {
    AppendToStringWrapped(data, aStr, PR_FALSE);
  }

  return NS_OK;
}

void nsHTMLContentSerializer::AppendWrapped_WhitespaceSequence(
                               nsASingleFragmentString::const_char_iterator &aPos,
                               const nsASingleFragmentString::const_char_iterator aEnd,
                               const nsASingleFragmentString::const_char_iterator aSequenceStart,
                               PRBool &aMayIgnoreStartOfLineWhitespaceSequence,
                               nsAString &aOutputStr)
{
  // Handle the complete sequence of whitespace.
  // Continue to iterate until we find the first non-whitespace char.
  // Updates "aPos" to point to the first unhandled char.
  // Also updates the aMayIgnoreStartOfLineWhitespaceSequence flag,
  // as well as the other "global" state flags.

  PRBool sawBlankOrTab = PR_FALSE;
  PRBool leaveLoop = PR_FALSE;

  do {
    switch (*aPos) {
      case ' ':
      case '\t':
        sawBlankOrTab = PR_TRUE;
        // no break
      case '\n':
        ++aPos;
        // do not increase mColPos,
        // because we will reduce the whitespace to a single char
        break;
      default:
        leaveLoop = PR_TRUE;
        break;
    }
  } while (!leaveLoop && aPos < aEnd);

  if (mAddSpace) {
    // if we had previously been asked to add space,
    // our situation has not changed
  }
  else if (!sawBlankOrTab && mMayIgnoreLineBreakSequence) {
    // nothing to do
    mMayIgnoreLineBreakSequence = PR_FALSE;
  }
  else if (aMayIgnoreStartOfLineWhitespaceSequence) {
    // nothing to do
    aMayIgnoreStartOfLineWhitespaceSequence = PR_FALSE;
  }
  else {
    if (sawBlankOrTab) {
      if (mColPos + 1 >= mMaxColumn) {
        // no much sense in delaying, we only have one slot left,
        // let's write a break now
        aOutputStr.Append(mLineBreak);
        mColPos = 0;
      }
      else {
        // do not write out yet, we may write out either a space or a linebreak
        // let's delay writing it out until we know more

        mAddSpace = PR_TRUE;
        ++mColPos; // eat a slot of available space
      }
    }
    else {
      // Asian text usually does not contain spaces, therefore we should not
      // transform a linebreak into a space.
      // Since we only saw linebreaks, but no spaces or tabs,
      // let's write a linebreak now.
      aOutputStr.Append(mLineBreak);
      mMayIgnoreLineBreakSequence = PR_TRUE;
      mColPos = 0;
    }
  }
}

void nsHTMLContentSerializer::AppendWrapped_NonWhitespaceSequence(
                               nsASingleFragmentString::const_char_iterator &aPos,
                               const nsASingleFragmentString::const_char_iterator aEnd,
                               const nsASingleFragmentString::const_char_iterator aSequenceStart,
                               PRBool &aMayIgnoreStartOfLineWhitespaceSequence,
                               nsAString& aOutputStr)
{
  mMayIgnoreLineBreakSequence = PR_FALSE;
  aMayIgnoreStartOfLineWhitespaceSequence = PR_FALSE;

  // Handle the complete sequence of non-whitespace in this block
  // Iterate until we find the first whitespace char or an aEnd condition
  // Updates "aPos" to point to the first unhandled char.
  // Also updates the aMayIgnoreStartOfLineWhitespaceSequence flag,
  // as well as the other "global" state flags.

  PRBool thisSequenceStartsAtBeginningOfLine = !mColPos;
  PRBool onceAgainBecauseWeAddedBreakInFront;
  PRBool foundWhitespaceInLoop;

  do {
    onceAgainBecauseWeAddedBreakInFront = PR_FALSE;
    foundWhitespaceInLoop = PR_FALSE;

    do {
      if (*aPos == ' ' || *aPos == '\t' || *aPos == '\n') {
        foundWhitespaceInLoop = PR_TRUE;
        break;
      }

      ++aPos;
      ++mColPos;
    } while (mColPos < mMaxColumn && aPos < aEnd);

    if (aPos == aEnd || foundWhitespaceInLoop) {
      // there is enough room for the complete block we found

      if (mAddSpace) {
        aOutputStr.Append(PRUnichar(' '));
        mAddSpace = PR_FALSE;
      }

      aOutputStr.Append(aSequenceStart, aPos - aSequenceStart);
      // We have not yet reached the max column, we will continue to
      // fill the current line in the next outer loop iteration.
    }
    else { // mColPos == mMaxColumn
      if (!thisSequenceStartsAtBeginningOfLine && mAddSpace) {
        // We can avoid to wrap.

        aOutputStr.Append(mLineBreak);
        mAddSpace = PR_FALSE;
        aPos = aSequenceStart;
        mColPos = 0;
        thisSequenceStartsAtBeginningOfLine = PR_TRUE;
        onceAgainBecauseWeAddedBreakInFront = PR_TRUE;
      }
      else {
        // we must wrap

        PRBool foundWrapPosition = PR_FALSE;
        nsILineBreaker *lineBreaker = nsContentUtils::LineBreaker();

        PRInt32 wrapPosition;

        wrapPosition = lineBreaker->Prev(aSequenceStart,
                                         (aEnd - aSequenceStart),
                                         (aPos - aSequenceStart) + 1);
        if (wrapPosition != NS_LINEBREAKER_NEED_MORE_TEXT) {
          foundWrapPosition = PR_TRUE;
        }
        else {
          wrapPosition = lineBreaker->Next(aSequenceStart,
                                           (aEnd - aSequenceStart),
                                           (aPos - aSequenceStart));
          if (wrapPosition != NS_LINEBREAKER_NEED_MORE_TEXT) {
            foundWrapPosition = PR_TRUE;
          }
        }

        if (foundWrapPosition) {
          if (mAddSpace) {
            aOutputStr.Append(PRUnichar(' '));
            mAddSpace = PR_FALSE;
          }

          aOutputStr.Append(aSequenceStart, wrapPosition);
          aOutputStr.Append(mLineBreak);
          aPos = aSequenceStart + wrapPosition;
          mColPos = 0;
          aMayIgnoreStartOfLineWhitespaceSequence = PR_TRUE;
          mMayIgnoreLineBreakSequence = PR_TRUE;
        }
        else {
          // try some simple fallback logic
          // go forward up to the next whitespace position,
          // in the worst case this will be all the rest of the data

          do {
            if (*aPos == ' ' || *aPos == '\t' || *aPos == '\n') {
              break;
            }

            ++aPos;
            ++mColPos;
          } while (aPos < aEnd);

          if (mAddSpace) {
            aOutputStr.Append(PRUnichar(' '));
            mAddSpace = PR_FALSE;
          }

          aOutputStr.Append(aSequenceStart, aPos - aSequenceStart);
        }
      }
    }
  } while (onceAgainBecauseWeAddedBreakInFront);
}

void 
nsHTMLContentSerializer::AppendToStringWrapped(const nsASingleFragmentString& aStr,
                                               nsAString& aOutputStr,
                                               PRBool aTranslateEntities)
{
  nsASingleFragmentString::const_char_iterator pos, end, sequenceStart;

  aStr.BeginReading(pos);
  aStr.EndReading(end);

  // if the current line already has text on it, such as a tag,
  // leading whitespace is significant

  PRBool mayIgnoreStartOfLineWhitespaceSequence = !mColPos;

  while (pos < end) {
    sequenceStart = pos;

    // if beginning of a whitespace sequence
    if (*pos == ' ' || *pos == '\n' || *pos == '\t') {
      AppendWrapped_WhitespaceSequence(pos, end, sequenceStart, 
        mayIgnoreStartOfLineWhitespaceSequence, aOutputStr);
    }
    else { // any other non-whitespace char
      AppendWrapped_NonWhitespaceSequence(pos, end, sequenceStart, 
        mayIgnoreStartOfLineWhitespaceSequence, aOutputStr);
    }
  }
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
  if (aAttrNameAtom == nsGkAtoms::href ||
      aAttrNameAtom == nsGkAtoms::src) {
    static const char kJavaScript[] = "javascript";
    PRInt32 pos = aValueString.FindChar(':');
    if (pos < (PRInt32)(sizeof kJavaScript - 1))
        return PR_FALSE;
    nsAutoString scheme(Substring(aValueString, 0, pos));
    scheme.StripWhitespace();
    if ((scheme.Length() == (sizeof kJavaScript - 1)) &&
        scheme.EqualsIgnoreCase(kJavaScript))
      return PR_TRUE;
    else
      return PR_FALSE;  
  }

  PRBool result = 
                 (aAttrNameAtom == nsGkAtoms::onblur)      || (aAttrNameAtom == nsGkAtoms::onchange)
              || (aAttrNameAtom == nsGkAtoms::onclick)     || (aAttrNameAtom == nsGkAtoms::ondblclick)
              || (aAttrNameAtom == nsGkAtoms::onfocus)     || (aAttrNameAtom == nsGkAtoms::onkeydown)
              || (aAttrNameAtom == nsGkAtoms::onkeypress)  || (aAttrNameAtom == nsGkAtoms::onkeyup)
              || (aAttrNameAtom == nsGkAtoms::onload)      || (aAttrNameAtom == nsGkAtoms::onmousedown)
              || (aAttrNameAtom == nsGkAtoms::onpageshow)  || (aAttrNameAtom == nsGkAtoms::onpagehide)
              || (aAttrNameAtom == nsGkAtoms::onmousemove) || (aAttrNameAtom == nsGkAtoms::onmouseout)
              || (aAttrNameAtom == nsGkAtoms::onmouseover) || (aAttrNameAtom == nsGkAtoms::onmouseup)
              || (aAttrNameAtom == nsGkAtoms::onreset)     || (aAttrNameAtom == nsGkAtoms::onselect)
              || (aAttrNameAtom == nsGkAtoms::onsubmit)    || (aAttrNameAtom == nsGkAtoms::onunload)
              || (aAttrNameAtom == nsGkAtoms::onabort)     || (aAttrNameAtom == nsGkAtoms::onerror)
              || (aAttrNameAtom == nsGkAtoms::onpaint)     || (aAttrNameAtom == nsGkAtoms::onresize)
              || (aAttrNameAtom == nsGkAtoms::onscroll)    || (aAttrNameAtom == nsGkAtoms::onbroadcast)
              || (aAttrNameAtom == nsGkAtoms::onclose)     || (aAttrNameAtom == nsGkAtoms::oncontextmenu)
              || (aAttrNameAtom == nsGkAtoms::oncommand)   || (aAttrNameAtom == nsGkAtoms::oncommandupdate)
              || (aAttrNameAtom == nsGkAtoms::ondragdrop)  || (aAttrNameAtom == nsGkAtoms::ondragenter)
              || (aAttrNameAtom == nsGkAtoms::ondragexit)  || (aAttrNameAtom == nsGkAtoms::ondraggesture)
              || (aAttrNameAtom == nsGkAtoms::ondragover)  || (aAttrNameAtom == nsGkAtoms::oninput);
  return result;
}

nsresult 
nsHTMLContentSerializer::EscapeURI(const nsAString& aURI, nsAString& aEscapedURI)
{
  // URL escape %xx cannot be used in JS.
  // No escaping if the scheme is 'javascript'.
  if (IsJavaScript(nsGkAtoms::href, aURI)) {
    aEscapedURI = aURI;
    return NS_OK;
  }

  // nsITextToSubURI does charset convert plus uri escape
  // This is needed to convert to a document charset which is needed to support existing browsers.
  // But we eventually want to use UTF-8 instead of a document charset, then the code would be much simpler.
  // See HTML 4.01 spec, "Appendix B.2.1 Non-ASCII characters in URI attribute values"
  nsCOMPtr<nsITextToSubURI> textToSubURI;
  nsAutoString uri(aURI); // in order to use FindCharInSet()
  nsresult rv = NS_OK;


  if (!mCharset.IsEmpty() && !IsASCII(uri)) {
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
    if (textToSubURI && !IsASCII(part)) {
      rv = textToSubURI->ConvertAndEscape(mCharset.get(), part.get(), getter_Copies(escapedURI));
      NS_ENSURE_SUCCESS(rv, rv);
    }
    else {
      escapedURI.Adopt(nsEscape(NS_ConvertUTF16toUTF8(part).get(), url_Path));
    }
    AppendASCIItoUTF16(escapedURI, aEscapedURI);

    // Append a reserved character without escaping.
    part = Substring(aURI, end, 1);
    aEscapedURI.Append(part);
    start = end + 1;
  }

  if (start < (PRInt32) aURI.Length()) {
    // Escape the remaining part.
    part = Substring(aURI, start, aURI.Length()-start);
    if (textToSubURI) {
      rv = textToSubURI->ConvertAndEscape(mCharset.get(), part.get(), getter_Copies(escapedURI));
      NS_ENSURE_SUCCESS(rv, rv);
    }
    else {
      escapedURI.Adopt(nsEscape(NS_ConvertUTF16toUTF8(part).get(), url_Path));
    }
    AppendASCIItoUTF16(escapedURI, aEscapedURI);
  }

  return rv;
}

void 
nsHTMLContentSerializer::SerializeAttributes(nsIContent* aContent,
                                             nsIAtom* aTagName,
                                             nsAString& aStr)
{
  nsresult rv;
  PRUint32 index, count;
  nsAutoString nameStr, valueStr;

  count = aContent->GetAttrCount();

  NS_NAMED_LITERAL_STRING(_mozStr, "_moz");

  // Loop backward over the attributes, since the order they are stored in is
  // the opposite of the order they were parsed in (see bug 213347 for reason).
  // index is unsigned, hence index >= 0 is always true.
  for (index = count; index > 0; ) {
    --index;
    const nsAttrName* name = aContent->GetAttrNameAt(index);
    PRInt32 namespaceID = name->NamespaceID();
    nsIAtom* attrName = name->LocalName();

    // Filter out any attribute starting with [-|_]moz
    const char* sharedName;
    attrName->GetUTF8String(&sharedName);
    if ((('_' == *sharedName) || ('-' == *sharedName)) &&
        !nsCRT::strncmp(sharedName+1, kMozStr, PRUint32(sizeof(kMozStr)-1))) {
      continue;
    }
    aContent->GetAttr(namespaceID, attrName, valueStr);

    // 
    // Filter out special case of <br type="_moz"> or <br _moz*>,
    // used by the editor.  Bug 16988.  Yuck.
    //
    if (aTagName == nsGkAtoms::br && attrName == nsGkAtoms::type &&
        StringBeginsWith(valueStr, _mozStr)) {
      continue;
    }

    if (mIsCopying && mIsFirstChildOfOL && (aTagName == nsGkAtoms::li) && 
        (attrName == nsGkAtoms::value)){
      // This is handled separately in SerializeLIValueAttribute()
      continue;
    }
    PRBool isJS = IsJavaScript(attrName, valueStr);
    
    if (((attrName == nsGkAtoms::href) || 
         (attrName == nsGkAtoms::src))) {
      // Make all links absolute when converting only the selection:
      if (mFlags & nsIDocumentEncoder::OutputAbsoluteLinks) {
        // Would be nice to handle OBJECT and APPLET tags,
        // but that gets more complicated since we have to
        // search the tag list for CODEBASE as well.
        // For now, just leave them relative.
        nsCOMPtr<nsIURI> uri = aContent->GetBaseURI();
        if (uri) {
          nsAutoString absURI;
          rv = NS_MakeAbsoluteURI(absURI, valueStr, uri);
          if (NS_SUCCEEDED(rv)) {
            valueStr = absURI;
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

    if (mDoFormat
        && (mColPos >= mMaxColumn
            || ((PRInt32)(mColPos + nameStr.Length() +
                          valueStr.Length() + 4) > mMaxColumn))) {
        aStr.Append(mLineBreak);
        mColPos = 0;
    }

    // Expand shorthand attribute.
    if (IsShorthandAttr(attrName, aTagName) && valueStr.IsEmpty()) {
      valueStr = nameStr;
    }
    SerializeAttr(EmptyString(), nameStr, valueStr, aStr, !isJS);
  }
}

NS_IMETHODIMP
nsHTMLContentSerializer::AppendElementStart(nsIDOMElement *aElement,
                                            nsIDOMElement *aOriginalElement,
                                            nsAString& aStr)
{
  NS_ENSURE_ARG(aElement);

  nsCOMPtr<nsIContent> content = do_QueryInterface(aElement);
  if (!content) return NS_ERROR_FAILURE;

  // The _moz_dirty attribute is emitted by the editor to
  // indicate that this element should be pretty printed
  // even if we're not in pretty printing mode
  PRBool hasDirtyAttr = content->HasAttr(kNameSpaceID_None,
                                         nsGkAtoms::mozdirty);

  nsIAtom *name = content->Tag();

  if (name == nsGkAtoms::br && mPreLevel > 0
      && (mFlags & nsIDocumentEncoder::OutputNoFormattingInPre)) {
    AppendToString(mLineBreak, aStr);
    mMayIgnoreLineBreakSequence = PR_TRUE;
    mColPos = 0;
    return NS_OK;
  }

  if (name == nsGkAtoms::body) {
    mInBody = PR_TRUE;
  }

  if (LineBreakBeforeOpen(name, hasDirtyAttr)) {
    AppendToString(mLineBreak, aStr);
    mMayIgnoreLineBreakSequence = PR_TRUE;
    mColPos = 0;
    mAddSpace = PR_FALSE;
  }
  else if (mAddSpace) {
    AppendToString(PRUnichar(' '), aStr);
    mAddSpace = PR_FALSE;
  }
  else {
    MaybeAddNewline(aStr);
  }
  // Always reset to avoid false newlines in case MaybeAddNewline wasn't
  // called
  mAddNewline = PR_FALSE;

  StartIndentation(name, hasDirtyAttr, aStr);

  if (name == nsGkAtoms::pre ||
      name == nsGkAtoms::script ||
      name == nsGkAtoms::style) {
    mPreLevel++;
  }
  
  AppendToString(kLessThan, aStr);

  nsAutoString nameStr;
  name->ToString(nameStr);
  AppendToString(nameStr.get(), -1, aStr);

  // Need to keep track of OL and LI elements in order to get ordinal number 
  // for the LI.
  if (mIsCopying && name == nsGkAtoms::ol){
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

  if (mIsCopying && name == nsGkAtoms::li) {
    mIsFirstChildOfOL = IsFirstChildOfOL(aOriginalElement);
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
    mMayIgnoreLineBreakSequence = PR_TRUE;
    mColPos = 0;
  }

  if (name == nsGkAtoms::script ||
      name == nsGkAtoms::style ||
      name == nsGkAtoms::noscript ||
      name == nsGkAtoms::noframes) {
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

  PRBool hasDirtyAttr = content->HasAttr(kNameSpaceID_None,
                                         nsGkAtoms::mozdirty);

  nsIAtom *name = content->Tag();

  if (name == nsGkAtoms::script) {
    nsCOMPtr<nsIScriptElement> script = do_QueryInterface(aElement);
    NS_ASSERTION(script, "What kind of weird script element is this?");

    if (script->IsMalformed()) {
      // We're looking at a malformed script tag. This means that the end tag
      // was missing in the source. Imitate that here by not serializing the end
      // tag.
      return NS_OK;
    }
  }

  if (name == nsGkAtoms::pre ||
      name == nsGkAtoms::script ||
      name == nsGkAtoms::style) {
    mPreLevel--;
  }

  if (mIsCopying && (name == nsGkAtoms::ol)){
    NS_ASSERTION((mOLStateStack.Count() > 0), "Cannot have an empty OL Stack");
    /* Though at this point we must always have an state to be deleted as all 
    the OL opening tags are supposed to push an olState object to the stack*/
    if (mOLStateStack.Count() > 0) {
      olState* state = (olState*)mOLStateStack.ElementAt(mOLStateStack.Count() -1);
      mOLStateStack.RemoveElementAt(mOLStateStack.Count() -1);
      delete state;
    }
  }
  
  nsIParserService* parserService = nsContentUtils::GetParserService();

  if (parserService && (name != nsGkAtoms::style)) {
    PRBool isContainer;

    parserService->IsContainer(parserService->HTMLAtomTagToId(name),
                               isContainer);
    if (!isContainer) return NS_OK;
  }

  if (LineBreakBeforeClose(name, hasDirtyAttr)) {
    AppendToString(mLineBreak, aStr);
    mMayIgnoreLineBreakSequence = PR_TRUE;
    mColPos = 0;
    mAddSpace = PR_FALSE;
  }
  else if (mAddSpace) {
    AppendToString(PRUnichar(' '), aStr);
    mAddSpace = PR_FALSE;
  }

  EndIndentation(name, hasDirtyAttr, aStr);

  nsAutoString nameStr;
  name->ToString(nameStr);

  AppendToString(kEndTag, aStr);
  AppendToString(nameStr.get(), -1, aStr);
  AppendToString(kGreaterThan, aStr);

  if (LineBreakAfterClose(name, hasDirtyAttr)) {
    AppendToString(mLineBreak, aStr);
    mMayIgnoreLineBreakSequence = PR_TRUE;
    mColPos = 0;
  }
  else {
    MaybeFlagNewline(aElement);
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

static const PRUint16 kValNBSP = 160;
static const char kEntityNBSP[] = "nbsp";

static const PRUint16 kGTVal = 62;
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
    if (mFlags & (nsIDocumentEncoder::OutputEncodeBasicEntities  |
                  nsIDocumentEncoder::OutputEncodeLatin1Entities |
                  nsIDocumentEncoder::OutputEncodeHTMLEntities   |
                  nsIDocumentEncoder::OutputEncodeW3CEntities)) {
      nsIParserService* parserService = nsContentUtils::GetParserService();

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
        PRUint32 lengthReplaced = 0; // the number of UTF-16 codepoints
                                     //  replaced by a particular entity
        const PRUnichar* c = iter.get();
        const PRUnichar* fragmentStart = c;
        const PRUnichar* fragmentEnd = c + fragmentLength;
        const char* entityText = nsnull;
        nsCAutoString entityReplacement;
        char* fullEntityText = nsnull;

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
          } else if (val > 127 &&
                    ((val < 256 &&
                      mFlags & nsIDocumentEncoder::OutputEncodeLatin1Entities) ||
                      mFlags & nsIDocumentEncoder::OutputEncodeHTMLEntities)) {
            parserService->HTMLConvertUnicodeToEntity(val, entityReplacement);

            if (!entityReplacement.IsEmpty()) {
              entityText = entityReplacement.get();
              break;
            }
          }
          else if (val > 127 && 
                   mFlags & nsIDocumentEncoder::OutputEncodeW3CEntities &&
                   mEntityConverter) {
            if (NS_IS_HIGH_SURROGATE(val) &&
                c + 1 < fragmentEnd &&
                NS_IS_LOW_SURROGATE(*(c + 1))) {
              PRUint32 valUTF32 = SURROGATE_TO_UCS4(val, *(++c));
              if (NS_SUCCEEDED(mEntityConverter->ConvertUTF32ToEntity(valUTF32,
                               nsIEntityConverter::entityW3C, &fullEntityText))) {
                lengthReplaced = 2;
                break;
              }
              else {
                advanceLength++;
              }
            }
            else if (NS_SUCCEEDED(mEntityConverter->ConvertToEntity(val,
                                  nsIEntityConverter::entityW3C, 
                                  &fullEntityText))) {
              lengthReplaced = 1;
              break;
            }
          }
        }

        aOutputStr.Append(fragmentStart, advanceLength);
        if (entityText) {
          aOutputStr.Append(PRUnichar('&'));
          AppendASCIItoUTF16(entityText, aOutputStr);
          aOutputStr.Append(PRUnichar(';'));
          advanceLength++;
        }
        // if it comes from nsIEntityConverter, it already has '&' and ';'
        else if (fullEntityText) {
          AppendASCIItoUTF16(fullEntityText, aOutputStr);
          nsMemory::Free(fullEntityText);
          advanceLength += lengthReplaced;
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
nsHTMLContentSerializer::LineBreakBeforeOpen(nsIAtom* aName, 
                                             PRBool aHasDirtyAttr)
{
  if ((!mDoFormat && !aHasDirtyAttr) || mPreLevel || !mColPos ||
      (mFlags & nsIDocumentEncoder::OutputRaw)) {
    return PR_FALSE;
  }
        
  if (aName == nsGkAtoms::title ||
      aName == nsGkAtoms::meta  ||
      aName == nsGkAtoms::link  ||
      aName == nsGkAtoms::style ||
      aName == nsGkAtoms::select ||
      aName == nsGkAtoms::option ||
      aName == nsGkAtoms::script ||
      aName == nsGkAtoms::html) {
    return PR_TRUE;
  }
  else {
    nsIParserService* parserService = nsContentUtils::GetParserService();
    
    if (parserService) {
      PRBool res;
      parserService->IsBlock(parserService->HTMLAtomTagToId(aName), res);
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

  if ((aName == nsGkAtoms::html) ||
      (aName == nsGkAtoms::head) ||
      (aName == nsGkAtoms::body) ||
      (aName == nsGkAtoms::ul) ||
      (aName == nsGkAtoms::ol) ||
      (aName == nsGkAtoms::dl) ||
      (aName == nsGkAtoms::table) ||
      (aName == nsGkAtoms::tbody) ||
      (aName == nsGkAtoms::tr) ||
      (aName == nsGkAtoms::br) ||
      (aName == nsGkAtoms::meta) ||
      (aName == nsGkAtoms::link) ||
      (aName == nsGkAtoms::script) ||
      (aName == nsGkAtoms::select) ||
      (aName == nsGkAtoms::map) ||
      (aName == nsGkAtoms::area) ||
      (aName == nsGkAtoms::style)) {
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

  if ((aName == nsGkAtoms::html) ||
      (aName == nsGkAtoms::head) ||
      (aName == nsGkAtoms::body) ||
      (aName == nsGkAtoms::ul) ||
      (aName == nsGkAtoms::ol) ||
      (aName == nsGkAtoms::dl) ||
      (aName == nsGkAtoms::select) ||
      (aName == nsGkAtoms::table) ||
      (aName == nsGkAtoms::tbody)) {
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

  if ((aName == nsGkAtoms::html) ||
      (aName == nsGkAtoms::head) ||
      (aName == nsGkAtoms::body) ||
      (aName == nsGkAtoms::tr) ||
      (aName == nsGkAtoms::th) ||
      (aName == nsGkAtoms::td) ||
      (aName == nsGkAtoms::pre) ||
      (aName == nsGkAtoms::title) ||
      (aName == nsGkAtoms::li) ||
      (aName == nsGkAtoms::dt) ||
      (aName == nsGkAtoms::dd) ||
      (aName == nsGkAtoms::blockquote) ||
      (aName == nsGkAtoms::select) ||
      (aName == nsGkAtoms::option) ||
      (aName == nsGkAtoms::p) ||
      (aName == nsGkAtoms::map) ||
      (aName == nsGkAtoms::div)) {
    return PR_TRUE;
  }
  else {
    nsIParserService* parserService = nsContentUtils::GetParserService();
    
    if (parserService) {
      PRBool res;
      parserService->IsBlock(parserService->HTMLAtomTagToId(aName), res);
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

  if ((aName == nsGkAtoms::head) ||
      (aName == nsGkAtoms::table) ||
      (aName == nsGkAtoms::tr) ||
      (aName == nsGkAtoms::ul) ||
      (aName == nsGkAtoms::ol) ||
      (aName == nsGkAtoms::dl) ||
      (aName == nsGkAtoms::tbody) ||
      (aName == nsGkAtoms::form) ||
      (aName == nsGkAtoms::frameset) ||
      (aName == nsGkAtoms::blockquote) ||
      (aName == nsGkAtoms::li) ||
      (aName == nsGkAtoms::dt) ||
      (aName == nsGkAtoms::dd)) {
    mIndent++;
  }
}

void
nsHTMLContentSerializer::EndIndentation(nsIAtom* aName,
                                        PRBool aHasDirtyAttr,
                                        nsAString& aStr)
{
  if ((aName == nsGkAtoms::head) ||
      (aName == nsGkAtoms::table) ||
      (aName == nsGkAtoms::tr) ||
      (aName == nsGkAtoms::ul) ||
      (aName == nsGkAtoms::ol) ||
      (aName == nsGkAtoms::dl) ||
      (aName == nsGkAtoms::li) ||
      (aName == nsGkAtoms::tbody) ||
      (aName == nsGkAtoms::form) ||
      (aName == nsGkAtoms::blockquote) ||
      (aName == nsGkAtoms::dt) ||
      (aName == nsGkAtoms::dd) ||
      (aName == nsGkAtoms::frameset)) {
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
      if (tagName.LowerCaseEqualsLiteral("li")) {
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
    SerializeAttr(EmptyString(), NS_LITERAL_STRING("value"), valueStr, aStr, PR_FALSE);
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
    SerializeAttr(EmptyString(), NS_LITERAL_STRING("value"), valueStr, aStr, PR_FALSE);
  }
}

PRBool
nsHTMLContentSerializer::IsFirstChildOfOL(nsIDOMElement* aElement){
  nsCOMPtr<nsIDOMNode> node = do_QueryInterface(aElement);
  nsAutoString parentName;
  {
    nsCOMPtr<nsIDOMNode> parentNode;
    node->GetParentNode(getter_AddRefs(parentNode));
    if (parentNode)
      parentNode->GetNodeName(parentName);
    else
      return PR_FALSE;
  }
  
  if (parentName.LowerCaseEqualsLiteral("ol")) {
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
