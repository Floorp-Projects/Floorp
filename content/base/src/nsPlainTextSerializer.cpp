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
 *   Daniel Bratell <bratell@lysator.liu.se>
 *   Ben Bucksch <mozilla@bucksch.org>
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

#include "nsPlainTextSerializer.h"
#include "nsILineBreakerFactory.h"
#include "nsLWBrkCIID.h"
#include "nsIServiceManager.h"
#include "nsHTMLAtoms.h"
#include "nsIDOMText.h"
#include "nsIDOMCDATASection.h"
#include "nsIDOMElement.h"
#include "nsINameSpaceManager.h"
#include "nsITextContent.h"
#include "nsTextFragment.h"
#include "nsContentUtils.h"
#include "nsReadableUtils.h"
#include "nsUnicharUtils.h"
#include "nsCRT.h"
#include "nsIParserService.h"

static NS_DEFINE_CID(kLWBrkCID, NS_LWBRK_CID);

#define PREF_STRUCTS "converter.html2txt.structs"
#define PREF_HEADER_STRATEGY "converter.html2txt.header_strategy"

static const  PRInt32 kTabSize=4;
static const  PRInt32 kOLNumberWidth = 3;
static const  PRInt32 kIndentSizeHeaders = 2;  /* Indention of h1, if
                                                mHeaderStrategy = 1 or = 2.
                                                Indention of other headers
                                                is derived from that.
                                                XXX center h1? */
static const  PRInt32 kIndentIncrementHeaders = 2;  /* If mHeaderStrategy = 1,
                                                indent h(x+1) this many
                                                columns more than h(x) */
static const  PRInt32 kIndentSizeList = (kTabSize > kOLNumberWidth+3) ? kTabSize: kOLNumberWidth+3;
                               // Indention of non-first lines of ul and ol
static const  PRInt32 kIndentSizeDD = kTabSize;  // Indention of <dd>

static PRInt32 HeaderLevel(eHTMLTags aTag);
static PRInt32 GetUnicharWidth(PRUnichar ucs);
static PRInt32 GetUnicharStringWidth(const PRUnichar* pwcs, PRInt32 n);

// Someday may want to make this non-const:
static const PRUint32 TagStackSize = 500;
static const PRUint32 OLStackSize = 100;

nsresult NS_NewPlainTextSerializer(nsIContentSerializer** aSerializer)
{
  nsPlainTextSerializer* it = new nsPlainTextSerializer();
  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return CallQueryInterface(it, aSerializer);
}

nsPlainTextSerializer::nsPlainTextSerializer()
  : kSpace(NS_LITERAL_STRING(" ")) // Init of "constant"
{

  mOutputString = nsnull;
  mInHead = PR_FALSE;
  mAtFirstColumn = PR_TRUE;
  mIndent = 0;
  mCiteQuoteLevel = 0;
  mStructs = PR_TRUE;       // will be read from prefs later
  mHeaderStrategy = 1 /*indent increasingly*/;   // ditto
  mQuotesPreformatted = PR_FALSE;                // ditto
  mDontWrapAnyQuotes = PR_FALSE;                 // ditto
  mSpanLevel = 0;
  for (PRInt32 i = 0; i <= 6; i++) {
    mHeaderCounter[i] = 0;
  }

  // Line breaker
  mWrapColumn = 72;     // XXX magic number, we expect someone to reset this
  mCurrentLineWidth = 0;

  // Flow
  mEmptyLines = 1; // The start of the document is an "empty line" in itself,
  mInWhitespace = PR_TRUE;
  mPreFormatted = PR_FALSE;
  mStartedOutput = PR_FALSE;

  // initialize the tag stack to zero:
  mTagStack = new nsHTMLTag[TagStackSize];
  mTagStackIndex = 0;
  mIgnoreAboveIndex = (PRUint32)kNotFound;

  // initialize the OL stack, where numbers for ordered lists are kept:
  mOLStack = new PRInt32[OLStackSize];
  mOLStackIndex = 0;

  mULCount = 0;
}

nsPlainTextSerializer::~nsPlainTextSerializer()
{
  delete[] mTagStack;
  delete[] mOLStack;
}

NS_IMPL_ISUPPORTS4(nsPlainTextSerializer, 
                   nsIContentSerializer,
                   nsIContentSink,
                   nsIHTMLContentSink,
                   nsIHTMLToTextSink)


NS_IMETHODIMP 
nsPlainTextSerializer::Init(PRUint32 aFlags, PRUint32 aWrapColumn,
                            const char* aCharSet, PRBool aIsCopying)
{
#ifdef DEBUG
  // Check if the major control flags are set correctly.
  if(aFlags & nsIDocumentEncoder::OutputFormatFlowed) {
    NS_ASSERTION(aFlags & nsIDocumentEncoder::OutputFormatted,
                 "If you want format=flowed, you must combine it with "
                 "nsIDocumentEncoder::OutputFormatted");
  }

  if(aFlags & nsIDocumentEncoder::OutputFormatted) {
    NS_ASSERTION(!(aFlags & nsIDocumentEncoder::OutputPreformatted),
                 "Can't do formatted and preformatted output at the same time!");
  }
#endif

  NS_ENSURE_TRUE(nsContentUtils::GetParserServiceWeakRef(),
                 NS_ERROR_UNEXPECTED);

  nsresult rv;
  
  mFlags = aFlags;
  mWrapColumn = aWrapColumn;

  // Only create a linebreaker if we will handle wrapping.
  if (MayWrap()) {
    nsCOMPtr<nsILineBreakerFactory> lf(do_GetService(kLWBrkCID, &rv));
    if (NS_SUCCEEDED(rv)) {
      nsAutoString lbarg;
      rv = lf->GetBreaker(lbarg, getter_AddRefs(mLineBreaker));
      if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
    }
  }

  // Set the line break character:
  if ((mFlags & nsIDocumentEncoder::OutputCRLineBreak)
      && (mFlags & nsIDocumentEncoder::OutputLFLineBreak)) {
    // Windows
    mLineBreak.Assign(NS_LITERAL_STRING("\r\n"));
  }
  else if (mFlags & nsIDocumentEncoder::OutputCRLineBreak) {
    // Mac
    mLineBreak.Assign(PRUnichar('\r'));
  }
  else if (mFlags & nsIDocumentEncoder::OutputLFLineBreak) {
    // Unix/DOM
    mLineBreak.Assign(PRUnichar('\n'));
  }
  else {
    // Platform/default
    mLineBreak.AssignWithConversion(NS_LINEBREAK);
  }

  mLineBreakDue = PR_FALSE;
  mFloatingLines = -1;

  if (mFlags & nsIDocumentEncoder::OutputFormatted) {
    // Get some prefs that controls how we do formatted output
    mStructs = nsContentUtils::GetBoolPref(PREF_STRUCTS, mStructs);

    mHeaderStrategy =
      nsContentUtils::GetIntPref(PREF_HEADER_STRATEGY, mHeaderStrategy);

    // The quotesPreformatted pref is a temporary measure. See bug 69638.
    mQuotesPreformatted =
      nsContentUtils::GetBoolPref("editor.quotesPreformatted",
                                  mQuotesPreformatted);

    // DontWrapAnyQuotes is set according to whether plaintext mail
    // is wrapping to window width -- see bug 134439.
    // We'll only want this if we're wrapping and formatted.
    if (mFlags & nsIDocumentEncoder::OutputWrap || mWrapColumn > 0) {
      mDontWrapAnyQuotes =
        nsContentUtils::GetBoolPref("mail.compose.wrap_to_window_width",
                                    mDontWrapAnyQuotes);
    }
  }

  // XXX We should let the caller pass this in.
  if (nsContentUtils::GetBoolPref("browser.frames.enabled")) {
    mFlags &= ~nsIDocumentEncoder::OutputNoFramesContent;
  }
  else {
    mFlags |= nsIDocumentEncoder::OutputNoFramesContent;
  }

  return NS_OK;
}

PRBool
nsPlainTextSerializer::GetLastBool(const nsVoidArray& aStack)
{
  PRUint32 size = aStack.Count();
  if (size == 0) {
    return PR_FALSE;
  }
  return (aStack.ElementAt(size-1) != NS_REINTERPRET_CAST(void*, PR_FALSE));
}

void
nsPlainTextSerializer::SetLastBool(nsVoidArray& aStack, PRBool aValue)
{
  PRUint32 size = aStack.Count();
  if (size > 0) {
    aStack.ReplaceElementAt(NS_REINTERPRET_CAST(void*, aValue), size-1);
  }
  else {
    NS_ERROR("There is no \"Last\" value");
  }
}

void
nsPlainTextSerializer::PushBool(nsVoidArray& aStack, PRBool aValue)
{
    aStack.AppendElement(NS_REINTERPRET_CAST(void*, aValue));
}

PRBool
nsPlainTextSerializer::PopBool(nsVoidArray& aStack)
{
  PRBool returnValue = PR_FALSE;
  PRUint32 size = aStack.Count();
  if (size > 0) {
    returnValue = (aStack.ElementAt(size-1) != NS_REINTERPRET_CAST(void*, PR_FALSE));
    aStack.RemoveElementAt(size-1);
  }
  return returnValue;
}

NS_IMETHODIMP
nsPlainTextSerializer::Initialize(nsAString* aOutString,
                                  PRUint32 aFlags, PRUint32 aWrapCol)
{
  nsresult rv = Init(aFlags, aWrapCol, nsnull, PR_FALSE);
  NS_ENSURE_SUCCESS(rv, rv);

  // XXX This is wrong. It violates XPCOM string ownership rules.
  // We're only getting away with this because instances of this
  // class are restricted to single function scope.
  mOutputString = aOutString;

  return NS_OK;
}

NS_IMETHODIMP 
nsPlainTextSerializer::AppendText(nsIDOMText* aText, 
                                  PRInt32 aStartOffset,
                                  PRInt32 aEndOffset, 
                                  nsAString& aStr)
{
  if (mIgnoreAboveIndex != (PRUint32)kNotFound) {
    return NS_OK;
  }
    
  NS_ASSERTION(aStartOffset >= 0, "Negative start offset for text fragment!");
  if ( aStartOffset < 0 )
    return NS_ERROR_INVALID_ARG;

  NS_ENSURE_ARG(aText);

  nsresult rv = NS_OK;
  PRInt32 length = 0;
  nsAutoString textstr;

  nsCOMPtr<nsITextContent> content = do_QueryInterface(aText);
  if (!content) return NS_ERROR_FAILURE;
  
  const nsTextFragment* frag = content->Text();

  if (frag) {
    PRInt32 endoffset = (aEndOffset == -1) ? frag->GetLength() : aEndOffset;
    NS_ASSERTION(aStartOffset <= endoffset, "A start offset is beyond the end of the text fragment!");

    length = endoffset - aStartOffset;
    if (length <= 0) {
      return NS_OK;
    }

    if (frag->Is2b()) {
      textstr.Assign(frag->Get2b() + aStartOffset, length);
    }
    else {
      textstr.AssignWithConversion(frag->Get1b()+aStartOffset, length);
    }
  }

  mOutputString = &aStr;

  // We have to split the string across newlines
  // to match parser behavior
  PRInt32 start = 0;
  PRInt32 offset = textstr.FindCharInSet("\n\r");
  while (offset != kNotFound) {

    if(offset>start) {
      // Pass in the line
      rv = DoAddLeaf(nsnull,
                     eHTMLTag_text,
                     Substring(textstr, start, offset-start));
      if (NS_FAILED(rv)) break;
    }

    // Pass in a newline
    rv = DoAddLeaf(nsnull, eHTMLTag_newline, mLineBreak);
    if (NS_FAILED(rv)) break;
    
    start = offset+1;
    offset = textstr.FindCharInSet("\n\r", start);
  }

  // Consume the last bit of the string if there's any left
  if (NS_SUCCEEDED(rv) && start < length) {
    if (start) {
      rv = DoAddLeaf(nsnull,
                     eHTMLTag_text,
                     Substring(textstr, start, length-start));
    }
    else {
      rv = DoAddLeaf(nsnull, eHTMLTag_text, textstr);
    }
  }
  
  mOutputString = nsnull;

  return rv;
}

NS_IMETHODIMP
nsPlainTextSerializer::AppendCDATASection(nsIDOMCDATASection* aCDATASection,
                                          PRInt32 aStartOffset,
                                          PRInt32 aEndOffset,
                                          nsAString& aStr)
{
  return AppendText(aCDATASection, aStartOffset, aEndOffset, aStr);
}

NS_IMETHODIMP
nsPlainTextSerializer::AppendElementStart(nsIDOMElement *aElement,
                                          PRBool aHasChildren,
                                          nsAString& aStr)
{
  NS_ENSURE_ARG(aElement);

  mContent = do_QueryInterface(aElement);
  if (!mContent) return NS_ERROR_FAILURE;

  nsresult rv;
  PRInt32 id = GetIdForContent(mContent);

  PRBool isContainer = IsContainer(id);

  mOutputString = &aStr;

  if (isContainer) {
    rv = DoOpenContainer(nsnull, id);
  }
  else {
    nsAutoString empty;
    rv = DoAddLeaf(nsnull, id, empty);
  }

  mContent = 0;
  mOutputString = nsnull;

  if (!mInHead && id == eHTMLTag_head)
    mInHead = PR_TRUE;    

  return rv;
} 
 
NS_IMETHODIMP 
nsPlainTextSerializer::AppendElementEnd(nsIDOMElement *aElement,
                                        nsAString& aStr)
{
  NS_ENSURE_ARG(aElement);

  mContent = do_QueryInterface(aElement);
  if (!mContent) return NS_ERROR_FAILURE;

  nsresult rv;
  PRInt32 id = GetIdForContent(mContent);

  PRBool isContainer = IsContainer(id);

  mOutputString = &aStr;

  rv = NS_OK;
  if (isContainer) {
    rv = DoCloseContainer(id);
  }

  mContent = 0;
  mOutputString = nsnull;

  if (mInHead && id == eHTMLTag_head)
    mInHead = PR_FALSE;    

  return rv;
}

NS_IMETHODIMP 
nsPlainTextSerializer::Flush(nsAString& aStr)
{
  mOutputString = &aStr;
  FlushLine();
  mOutputString = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsPlainTextSerializer::AppendDocumentStart(nsIDOMDocument *aDocument,
                                             nsAString& aStr)
{
  return NS_OK;
}

NS_IMETHODIMP
nsPlainTextSerializer::OpenContainer(const nsIParserNode& aNode)
{
  PRInt32 type = aNode.GetNodeType();

  return DoOpenContainer(&aNode, type);
}

NS_IMETHODIMP 
nsPlainTextSerializer::CloseContainer(const nsHTMLTag aTag)
{
  return DoCloseContainer(aTag);
}

NS_IMETHODIMP 
nsPlainTextSerializer::AddHeadContent(const nsIParserNode& aNode)
{
  OpenHead(aNode);
  nsresult rv = AddLeaf(aNode);
  CloseHead();
  return rv;
}

NS_IMETHODIMP 
nsPlainTextSerializer::AddLeaf(const nsIParserNode& aNode)
{
  if (mIgnoreAboveIndex != (PRUint32)kNotFound) {
    return NS_OK;
  }

  eHTMLTags type = (eHTMLTags)aNode.GetNodeType();
  const nsAString& text = aNode.GetText();

  if ((type == eHTMLTag_text) ||
      (type == eHTMLTag_whitespace) ||
      (type == eHTMLTag_newline)) {
    // Copy the text out, stripping out CRs
    nsAutoString str;
    PRUint32 length;
    str.SetCapacity(text.Length());
    nsReadingIterator<PRUnichar> srcStart, srcEnd;
    length = nsContentUtils::CopyNewlineNormalizedUnicodeTo(text.BeginReading(srcStart), text.EndReading(srcEnd), str);
    str.SetLength(length);
    return DoAddLeaf(&aNode, type, str);
  }
  else {
    return DoAddLeaf(&aNode, type, text);
  }
}

NS_IMETHODIMP
nsPlainTextSerializer::OpenHTML(const nsIParserNode& aNode)
{
  return OpenContainer(aNode);
}

NS_IMETHODIMP 
nsPlainTextSerializer::CloseHTML()
{
  return CloseContainer(eHTMLTag_html);
}

NS_IMETHODIMP 
nsPlainTextSerializer::OpenHead(const nsIParserNode& aNode)
{
  mInHead = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP 
nsPlainTextSerializer::CloseHead()
{
  mInHead = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP 
nsPlainTextSerializer::OpenBody(const nsIParserNode& aNode)
{
  return OpenContainer(aNode);
}

NS_IMETHODIMP 
nsPlainTextSerializer::CloseBody()
{
  return CloseContainer(eHTMLTag_body);
}

NS_IMETHODIMP 
nsPlainTextSerializer::OpenForm(const nsIParserNode& aNode)
{
  return OpenContainer(aNode);
}

NS_IMETHODIMP 
nsPlainTextSerializer::CloseForm()
{
  return CloseContainer(eHTMLTag_form);
}

NS_IMETHODIMP 
nsPlainTextSerializer::OpenMap(const nsIParserNode& aNode)
{
  return OpenContainer(aNode);
}

NS_IMETHODIMP 
nsPlainTextSerializer::CloseMap()
{
  return CloseContainer(eHTMLTag_map);
}

NS_IMETHODIMP 
nsPlainTextSerializer::OpenFrameset(const nsIParserNode& aNode)
{
  return OpenContainer(aNode);
}

NS_IMETHODIMP 
nsPlainTextSerializer::CloseFrameset()
{
  return CloseContainer(eHTMLTag_frameset);
}

NS_IMETHODIMP
nsPlainTextSerializer::IsEnabled(PRInt32 aTag, PRBool* aReturn)
{
  nsHTMLTag theHTMLTag = nsHTMLTag(aTag);

  if (theHTMLTag == eHTMLTag_script) {
    *aReturn = !(mFlags & nsIDocumentEncoder::OutputNoScriptContent);
  }
  else if (theHTMLTag == eHTMLTag_frameset) {
    *aReturn = !(mFlags & nsIDocumentEncoder::OutputNoFramesContent);
  }
  else {
    *aReturn = PR_FALSE;
  }

  return NS_OK;
}

/**
 * aNode may be null when we're working with the DOM, but then mContent is
 * useable instead.
 */
nsresult
nsPlainTextSerializer::DoOpenContainer(const nsIParserNode* aNode, PRInt32 aTag)
{
  if (mFlags & nsIDocumentEncoder::OutputRaw) {
    // Raw means raw.  Don't even think about doing anything fancy
    // here like indenting, adding line breaks or any other
    // characters such as list item bullets, quote characters
    // around <q>, etc.  I mean it!  Don't make me smack you!

    return NS_OK;
  }

  eHTMLTags type = (eHTMLTags)aTag;

  if (mTagStackIndex < TagStackSize) {
    mTagStack[mTagStackIndex++] = type;
  }

  if (mIgnoreAboveIndex != (PRUint32)kNotFound) {
    return NS_OK;
  }

  if (mLineBreakDue)
    EnsureVerticalSpace(mFloatingLines);

  // Check if this tag's content that should not be output
  if ((type == eHTMLTag_noscript &&
       !(mFlags & nsIDocumentEncoder::OutputNoScriptContent)) ||
      ((type == eHTMLTag_iframe || type == eHTMLTag_noframes) &&
       !(mFlags & nsIDocumentEncoder::OutputNoFramesContent))) {
    // Ignore everything that follows the current tag in 
    // question until a matching end tag is encountered.
    mIgnoreAboveIndex = mTagStackIndex - 1;
    return NS_OK;
  }

  if (type == eHTMLTag_body) {
    // Try to figure out here whether we have a
    // preformatted style attribute.
    //
    // Trigger on the presence of a "-moz-pre-wrap" in the
    // style attribute. That's a very simplistic way to do
    // it, but better than nothing.
    // Also set mWrapColumn to the value given there
    // (which arguably we should only do if told to do so).
    nsAutoString style;
    PRInt32 whitespace;
    if(NS_SUCCEEDED(GetAttributeValue(aNode, nsHTMLAtoms::style, style)) &&
       (kNotFound != (whitespace = style.Find("white-space:")))) {

      if (kNotFound != style.Find("-moz-pre-wrap", PR_TRUE, whitespace)) {
#ifdef DEBUG_preformatted
        printf("Set mPreFormatted based on style moz-pre-wrap\n");
#endif
        mPreFormatted = PR_TRUE;
        PRInt32 widthOffset = style.Find("width:");
        if (widthOffset >= 0) {
          // We have to search for the ch before the semicolon,
          // not for the semicolon itself, because nsString::ToInteger()
          // considers 'c' to be a valid numeric char (even if radix=10)
          // but then gets confused if it sees it next to the number
          // when the radix specified was 10, and returns an error code.
          PRInt32 semiOffset = style.Find("ch", widthOffset+6);
          PRInt32 length = (semiOffset > 0 ? semiOffset - widthOffset - 6
                            : style.Length() - widthOffset);
          nsAutoString widthstr;
          style.Mid(widthstr, widthOffset+6, length);
          PRInt32 err;
          PRInt32 col = widthstr.ToInteger(&err);

          if (NS_SUCCEEDED(err)) {
            mWrapColumn = (PRUint32)col;
#ifdef DEBUG_preformatted
            printf("Set wrap column to %d based on style\n", mWrapColumn);
#endif
          }
        }
      }
      else if (kNotFound != style.Find("pre", PR_TRUE, whitespace)) {
#ifdef DEBUG_preformatted
        printf("Set mPreFormatted based on style pre\n");
#endif
        mPreFormatted = PR_TRUE;
        mWrapColumn = 0;
      }
    } 
    else {
      mPreFormatted = PR_FALSE;
    }

    return NS_OK;
  }

  if (!DoOutput()) {
    return NS_OK;
  }

  if (type == eHTMLTag_p || type == eHTMLTag_pre) {
    EnsureVerticalSpace(1); // Should this be 0 in unformatted case?
  }
  else if (type == eHTMLTag_tr) {
    PushBool(mHasWrittenCellsForRow, PR_FALSE);
  }
  else if (type == eHTMLTag_td || type == eHTMLTag_th) {
    // We must make sure that the content of two table cells get a
    // space between them.

    // To make the separation between cells most obvious and
    // importable, we use a TAB.
    if (GetLastBool(mHasWrittenCellsForRow)) {
      // Bypass |Write| so that the TAB isn't compressed away.
      AddToLine(NS_LITERAL_STRING("\t").get(), 1);
      mInWhitespace = PR_TRUE;
    }
    else if (mHasWrittenCellsForRow.Count() == 0) {
      // We don't always see a <tr> (nor a <table>) before the <td> if we're
      // copying part of a table
      PushBool(mHasWrittenCellsForRow, PR_TRUE); // will never be popped
    }
    else {
      SetLastBool(mHasWrittenCellsForRow, PR_TRUE);
    }
  }
  else if (type == eHTMLTag_ul) {
    // Indent here to support nested lists, which aren't included in li :-(
    EnsureVerticalSpace(mULCount + mOLStackIndex == 0 ? 1 : 0);
         // Must end the current line before we change indention
    mIndent += kIndentSizeList;
    mULCount++;
  }
  else if (type == eHTMLTag_ol) {
    EnsureVerticalSpace(mULCount + mOLStackIndex == 0 ? 1 : 0);
    // Must end the current line before we change indention
    if (mOLStackIndex < OLStackSize) {
      nsAutoString startAttr;
      PRInt32 startVal = 1;
      if(NS_SUCCEEDED(GetAttributeValue(aNode, nsHTMLAtoms::start, startAttr))){
        PRInt32 rv = 0;
        startVal = startAttr.ToInteger(&rv);
        if (NS_FAILED(rv))
          startVal = 1;
      }
      mOLStack[mOLStackIndex++] = startVal;
    }
    mIndent += kIndentSizeList;  // see ul
  }
  else if (type == eHTMLTag_li) {
    if (mTagStackIndex > 1 && IsInOL()) {
      if (mOLStackIndex > 0) {
        nsAutoString valueAttr;
        if(NS_SUCCEEDED(GetAttributeValue(aNode, nsHTMLAtoms::value, valueAttr))){
          PRInt32 rv = 0;
          PRInt32 valueAttrVal = valueAttr.ToInteger(&rv);
          if (NS_SUCCEEDED(rv))
            mOLStack[mOLStackIndex-1] = valueAttrVal;
        }
        // This is what nsBulletFrame does for OLs:
        mInIndentString.AppendInt(mOLStack[mOLStackIndex-1]++, 10);
      }
      else {
        mInIndentString.Append(PRUnichar('#'));
      }

      mInIndentString.Append(PRUnichar('.'));

    }
    else {
      static char bulletCharArray[] = "*o+#";
      NS_ASSERTION(mULCount > 0, "mULCount should be greater than 0 here");
      char bulletChar = bulletCharArray[(mULCount - 1) % 4];
      mInIndentString.Append(PRUnichar(bulletChar));
    }
    
    mInIndentString.Append(PRUnichar(' '));
  }
  else if (type == eHTMLTag_dl) {
    EnsureVerticalSpace(1);
  }
  else if (type == eHTMLTag_dt) {
    EnsureVerticalSpace(0);
  }
  else if (type == eHTMLTag_dd) {
    EnsureVerticalSpace(0);
    mIndent += kIndentSizeDD;
  }
  else if (type == eHTMLTag_span) {
    ++mSpanLevel;
  }
  else if (type == eHTMLTag_blockquote) {
    EnsureVerticalSpace(1);

    nsAutoString value;
    nsresult rv = GetAttributeValue(aNode, nsHTMLAtoms::type, value);

    PRBool isInCiteBlockquote =
      NS_SUCCEEDED(rv) && value.EqualsIgnoreCase("cite");
    // Push
    PushBool(mIsInCiteBlockquote, isInCiteBlockquote);
    if (isInCiteBlockquote) {
      mCiteQuoteLevel++;
    }
    else {
      mIndent += kTabSize; // Check for some maximum value?
    }
  }

  // Else make sure we'll separate block level tags,
  // even if we're about to leave, before doing any other formatting.
  else if (IsBlockLevel(aTag)) {
    EnsureVerticalSpace(0);
  }

  //////////////////////////////////////////////////////////////
  if (!(mFlags & nsIDocumentEncoder::OutputFormatted)) {
    return NS_OK;
  }
  //////////////////////////////////////////////////////////////
  // The rest of this routine is formatted output stuff,
  // which we should skip if we're not formatted:
  //////////////////////////////////////////////////////////////

  // Push on stack
  PRBool currentNodeIsConverted = IsCurrentNodeConverted(aNode);
  PushBool(mCurrentNodeIsConverted, currentNodeIsConverted);

  if (type == eHTMLTag_h1 || type == eHTMLTag_h2 ||
      type == eHTMLTag_h3 || type == eHTMLTag_h4 ||
      type == eHTMLTag_h5 || type == eHTMLTag_h6)
  {
    EnsureVerticalSpace(2);
    if (mHeaderStrategy == 2) {  // numbered
      mIndent += kIndentSizeHeaders;
      // Caching
      PRInt32 level = HeaderLevel(type);
      // Increase counter for current level
      mHeaderCounter[level]++;
      // Reset all lower levels
      PRInt32 i;

      for (i = level + 1; i <= 6; i++) {
        mHeaderCounter[i] = 0;
      }

      // Construct numbers
      nsAutoString leadup;
      for (i = 1; i <= level; i++) {
        leadup.AppendInt(mHeaderCounter[i]);
        leadup.Append(PRUnichar('.'));
      }
      leadup.Append(PRUnichar(' '));
      Write(leadup);
    }
    else if (mHeaderStrategy == 1) { // indent increasingly
      mIndent += kIndentSizeHeaders;
      for (PRInt32 i = HeaderLevel(type); i > 1; i--) {
           // for h(x), run x-1 times
        mIndent += kIndentIncrementHeaders;
      }
    }
  }
  else if (type == eHTMLTag_a && !currentNodeIsConverted) {
    nsAutoString url;
    if (NS_SUCCEEDED(GetAttributeValue(aNode, nsHTMLAtoms::href, url))
        && !url.IsEmpty()) {
      mURL = url;
    }
  }
  else if (type == eHTMLTag_q) {
    Write(NS_LITERAL_STRING("\""));
  }
  else if (type == eHTMLTag_sup && mStructs && !currentNodeIsConverted) {
    Write(NS_LITERAL_STRING("^"));
  }
  else if (type == eHTMLTag_sub && mStructs && !currentNodeIsConverted) { 
    Write(NS_LITERAL_STRING("_"));
  }
  else if (type == eHTMLTag_code && mStructs && !currentNodeIsConverted) {
    Write(NS_LITERAL_STRING("|"));
  }
  else if ((type == eHTMLTag_strong || type == eHTMLTag_b)
           && mStructs && !currentNodeIsConverted) {
    Write(NS_LITERAL_STRING("*"));
  }
  else if ((type == eHTMLTag_em || type == eHTMLTag_i)
           && mStructs && !currentNodeIsConverted) {
    Write(NS_LITERAL_STRING("/"));
  }
  else if (type == eHTMLTag_u && mStructs && !currentNodeIsConverted) {
    Write(NS_LITERAL_STRING("_"));
  }

  return NS_OK;
}

nsresult
nsPlainTextSerializer::DoCloseContainer(PRInt32 aTag)
{
  if (mFlags & nsIDocumentEncoder::OutputRaw) {
    // Raw means raw.  Don't even think about doing anything fancy
    // here like indenting, adding line breaks or any other
    // characters such as list item bullets, quote characters
    // around <q>, etc.  I mean it!  Don't make me smack you!

    return NS_OK;
  }

  if (mTagStackIndex > 0) {
    --mTagStackIndex;
  }

  if (mTagStackIndex >= mIgnoreAboveIndex) {
    if (mTagStackIndex == mIgnoreAboveIndex) {
      // We're dealing with the close tag whose matching
      // open tag had set the mIgnoreAboveIndex value.
      // Reset mIgnoreAboveIndex before discarding this tag.
      mIgnoreAboveIndex = (PRUint32)kNotFound;
    }
    return NS_OK;
  }

  eHTMLTags type = (eHTMLTags)aTag;
  // End current line if we're ending a block level tag
  if((type == eHTMLTag_body) || (type == eHTMLTag_html)) {
    // We want the output to end with a new line,
    // but in preformatted areas like text fields,
    // we can't emit newlines that weren't there.
    // So add the newline only in the case of formatted output.
    if (mFlags & nsIDocumentEncoder::OutputFormatted) {
      EnsureVerticalSpace(0);
    }
    else {
      FlushLine();
    }
    // We won't want to do anything with these in formatted mode either,
    // so just return now:
    return NS_OK;
  }
  else if (type == eHTMLTag_tr) {
    PopBool(mHasWrittenCellsForRow);
    // Should always end a line, but get no more whitespace
    if (mFloatingLines < 0)
      mFloatingLines = 0;
    mLineBreakDue = PR_TRUE;
  } 
  else if ((type == eHTMLTag_li) ||
           (type == eHTMLTag_dt)) {
    // Items that should always end a line, but get no more whitespace
    if (mFloatingLines < 0)
      mFloatingLines = 0;
    mLineBreakDue = PR_TRUE;
  } 
  else if (type == eHTMLTag_pre) {
    mFloatingLines = 1;
    mLineBreakDue = PR_TRUE;
  }
  else if (type == eHTMLTag_ul) {
    FlushLine();
    mIndent -= kIndentSizeList;
    if (--mULCount + mOLStackIndex == 0) {
      mFloatingLines = 1;
      mLineBreakDue = PR_TRUE;
    }
  }
  else if (type == eHTMLTag_ol) {
    FlushLine(); // Doing this after decreasing OLStackIndex would be wrong.
    mIndent -= kIndentSizeList;
    mOLStackIndex--;
    if (mULCount + mOLStackIndex == 0) {
      mFloatingLines = 1;
      mLineBreakDue = PR_TRUE;
    }
  }  
  else if (type == eHTMLTag_dl) {
    mFloatingLines = 1;
    mLineBreakDue = PR_TRUE;
  }
  else if (type == eHTMLTag_dd) {
    FlushLine();
    mIndent -= kIndentSizeDD;
  }
  else if (type == eHTMLTag_span) {
    --mSpanLevel;
  }
  else if (type == eHTMLTag_div) {
    if (mFloatingLines < 0)
      mFloatingLines = 0;
    mLineBreakDue = PR_TRUE;
  }
  else if (type == eHTMLTag_blockquote) {
    FlushLine();    // Is this needed?

    // Pop
    PRBool isInCiteBlockquote = PopBool(mIsInCiteBlockquote);

    if (isInCiteBlockquote) {
      mCiteQuoteLevel--;
    }
    else {
      mIndent -= kTabSize;
    }

    mFloatingLines = 1;
    mLineBreakDue = PR_TRUE;
  }
  else if (IsBlockLevel(aTag)
           && type != eHTMLTag_script
           && type != eHTMLTag_doctypeDecl
           && type != eHTMLTag_markupDecl) {
    // All other blocks get 1 vertical space after them
    // in formatted mode, otherwise 0.
    // This is hard. Sometimes 0 is a better number, but
    // how to know?
    if (mFlags & nsIDocumentEncoder::OutputFormatted)
      EnsureVerticalSpace(1);
    else {
      if (mFloatingLines < 0)
        mFloatingLines = 0;
      mLineBreakDue = PR_TRUE;
    }
  }

  //////////////////////////////////////////////////////////////
  if (!(mFlags & nsIDocumentEncoder::OutputFormatted)) {
    return NS_OK;
  }
  //////////////////////////////////////////////////////////////
  // The rest of this routine is formatted output stuff,
  // which we should skip if we're not formatted:
  //////////////////////////////////////////////////////////////

  // Pop the currentConverted stack
  PRBool currentNodeIsConverted = PopBool(mCurrentNodeIsConverted);
  
  if (type == eHTMLTag_h1 || type == eHTMLTag_h2 ||
      type == eHTMLTag_h3 || type == eHTMLTag_h4 ||
      type == eHTMLTag_h5 || type == eHTMLTag_h6) {
    
    if (mHeaderStrategy) {  /*numbered or indent increasingly*/ 
      mIndent -= kIndentSizeHeaders;
    }
    if (mHeaderStrategy == 1 /*indent increasingly*/ ) {
      for (PRInt32 i = HeaderLevel(type); i > 1; i--) {
           // for h(x), run x-1 times
        mIndent -= kIndentIncrementHeaders;
      }
    }
    EnsureVerticalSpace(1);
  }
  else if (type == eHTMLTag_a && !currentNodeIsConverted && !mURL.IsEmpty()) {
    nsAutoString temp; 
    temp.Assign(NS_LITERAL_STRING(" <"));
    temp += mURL;
    temp.Append(PRUnichar('>'));
    Write(temp);
    mURL.Truncate();
  }
  else if (type == eHTMLTag_q) {
    Write(NS_LITERAL_STRING("\""));
  }
  else if ((type == eHTMLTag_sup || type == eHTMLTag_sub) 
           && mStructs && !currentNodeIsConverted) {
    Write(kSpace);
  }
  else if (type == eHTMLTag_code && mStructs && !currentNodeIsConverted) {
    Write(NS_LITERAL_STRING("|"));
  }
  else if ((type == eHTMLTag_strong || type == eHTMLTag_b)
           && mStructs && !currentNodeIsConverted) {
    Write(NS_LITERAL_STRING("*"));
  }
  else if ((type == eHTMLTag_em || type == eHTMLTag_i)
           && mStructs && !currentNodeIsConverted) {
    Write(NS_LITERAL_STRING("/"));
  }
  else if (type == eHTMLTag_u && mStructs && !currentNodeIsConverted) {
    Write(NS_LITERAL_STRING("_"));
  }

  return NS_OK;
}

/**
 * aNode may be null when we're working with the DOM, but then mContent is
 * useable instead.
 */
nsresult
nsPlainTextSerializer::DoAddLeaf(const nsIParserNode *aNode, PRInt32 aTag, 
                                 const nsAString& aText)
{
  // If we don't want any output, just return
  if (!DoOutput()) {
    return NS_OK;
  }
  
  if (mLineBreakDue)
    EnsureVerticalSpace(mFloatingLines);

  eHTMLTags type = (eHTMLTags)aTag;
  
  if ((mTagStackIndex > 1 &&
       mTagStack[mTagStackIndex-2] == eHTMLTag_select) ||
      (mTagStackIndex > 0 &&
        mTagStack[mTagStackIndex-1] == eHTMLTag_select)) {
    // Don't output the contents of SELECT elements;
    // Might be nice, eventually, to output just the selected element.
    // Read more in bug 31994.
    return NS_OK;
  }
  else if (mTagStackIndex > 0 && mTagStack[mTagStackIndex-1] == eHTMLTag_script) {
    // Don't output the contents of <script> tags;
    return NS_OK;
  }
  else if (type == eHTMLTag_text) {
    /* Check, if we are in a link (symbolized with mURL containing the URL)
       and the text is equal to the URL. In that case we don't want to output
       the URL twice so we scrap the text in mURL. */
    if (!mURL.IsEmpty() && mURL.Equals(aText)) {
      mURL.Truncate();
    }
    Write(aText);
  }
  else if (type == eHTMLTag_entity) {
    nsIParserService* parserService =
      nsContentUtils::GetParserServiceWeakRef();
    if (parserService) {
      nsAutoString str(aText);
      PRInt32 entity;
      parserService->HTMLConvertEntityToUnicode(str, &entity);
      if (entity == -1 && 
          !str.IsEmpty() &&
          str.First() == (PRUnichar) '#') {
        PRInt32 err = 0;
        entity = str.ToInteger(&err, kAutoDetect);  // NCR
      }
      nsAutoString temp;
      temp.Append(PRUnichar(entity));
      Write(temp);
    }
  }
  else if (type == eHTMLTag_br) {
    // Another egregious editor workaround, see bug 38194:
    // ignore the bogus br tags that the editor sticks here and there.
    nsAutoString typeAttr;
    if (NS_FAILED(GetAttributeValue(aNode, nsHTMLAtoms::type, typeAttr))
        || !typeAttr.EqualsLiteral("_moz")) {
      EnsureVerticalSpace(mEmptyLines+1);
    }
  }
  else if (type == eHTMLTag_whitespace) {
    // The only times we want to pass along whitespace from the original
    // html source are if we're forced into preformatted mode via flags,
    // or if we're prettyprinting and we're inside a <pre>.
    // Otherwise, either we're collapsing to minimal text, or we're
    // prettyprinting to mimic the html format, and in neither case
    // does the formatting of the html source help us.
    // One exception: at the very beginning of a selection,
    // we want to preserve whitespace.
    if (mFlags & nsIDocumentEncoder::OutputPreformatted ||
        (mPreFormatted && !mWrapColumn) ||
        IsInPre()) {
      Write(aText);
    }
    else if(!mInWhitespace ||
            (!mStartedOutput
             && mFlags | nsIDocumentEncoder::OutputSelectionOnly)) {
      mInWhitespace = PR_FALSE;
      Write(kSpace);
      mInWhitespace = PR_TRUE;
    }
  }
  else if (type == eHTMLTag_newline) {
    if (mFlags & nsIDocumentEncoder::OutputPreformatted ||
        (mPreFormatted && !mWrapColumn) ||
        IsInPre()) {
      EnsureVerticalSpace(mEmptyLines+1);
    }
    else {
      Write(kSpace);
    }
  }
  else if (type == eHTMLTag_hr &&
           (mFlags & nsIDocumentEncoder::OutputFormatted)) {
    EnsureVerticalSpace(0);

    // Make a line of dashes as wide as the wrap width
    // XXX honoring percentage would be nice
    nsAutoString line;
    PRUint32 width = (mWrapColumn > 0 ? mWrapColumn : 25);
    while (line.Length() < width) {
      line.Append(PRUnichar('-'));
    }
    Write(line);

    EnsureVerticalSpace(0);
  }
  else if (type == eHTMLTag_img) {
    /* Output (in decreasing order of preference)
       alt, title or nothing */
    // See <http://www.w3.org/TR/REC-html40/struct/objects.html#edef-IMG>
    nsAutoString imageDescription;
    if (NS_SUCCEEDED(GetAttributeValue(aNode,
                                       nsHTMLAtoms::alt,
                                       imageDescription))) {
      // If the alt attribute has an empty value (|alt=""|), output nothing
    }
    else if (NS_SUCCEEDED(GetAttributeValue(aNode,
                                            nsHTMLAtoms::title,
                                            imageDescription))
             && !imageDescription.IsEmpty()) {
      imageDescription = NS_LITERAL_STRING(" [") +
                         imageDescription +
                         NS_LITERAL_STRING("] ");
    }
   
    Write(imageDescription);
  }


  return NS_OK;
}

/**
 * Adds as many newline as necessary to get |noOfRows| empty lines
 *
 * noOfRows = -1    :   Being in the middle of some line of text
 * noOfRows =  0    :   Being at the start of a line
 * noOfRows =  n>0  :   Having n empty lines before the current line.
 */
void
nsPlainTextSerializer::EnsureVerticalSpace(PRInt32 noOfRows)
{
  // If we have something in the indent we probably want to output
  // it and it's not included in the count for empty lines so we don't
  // realize that we should start a new line.
  if(noOfRows >= 0 && !mInIndentString.IsEmpty()) {
    EndLine(PR_FALSE);
  }

  while(mEmptyLines < noOfRows) {
    EndLine(PR_FALSE);
  }
  mLineBreakDue = PR_FALSE;
  mFloatingLines = -1;
}

/**
 * This empties the current line cache without adding a NEWLINE.
 * Should not be used if line wrapping is of importance since
 * this function destroys the cache information.
 *
 * It will also write indentation and quotes if we believe us to be
 * at the start of the line.
 */
void
nsPlainTextSerializer::FlushLine()
{
  if(!mCurrentLine.IsEmpty()) {
    if(mAtFirstColumn) {
      OutputQuotesAndIndent(); // XXX: Should we always do this? Bug?
    }

    Output(mCurrentLine);
    mAtFirstColumn = mAtFirstColumn && mCurrentLine.IsEmpty();
    mCurrentLine.Truncate();
    mCurrentLineWidth = 0;
  }
}

/**
 * Prints the text to output to our current output device (the string mOutputString).
 * The only logic here is to replace non breaking spaces with a normal space since
 * most (all?) receivers of the result won't understand the nbsp and even be
 * confused by it.
 */
void 
nsPlainTextSerializer::Output(nsString& aString)
{
  if (!aString.IsEmpty()) {
    mStartedOutput = PR_TRUE;
  }

  // First, replace all nbsp characters with spaces,
  // which the unicode encoder won't do for us.
  static PRUnichar nbsp = 160;
  static PRUnichar space = ' ';
  aString.ReplaceChar(nbsp, space);

  mOutputString->Append(aString);
}

/**
 * This function adds a piece of text to the current stored line. If we are
 * wrapping text and the stored line will become too long, a suitable
 * location to wrap will be found and the line that's complete will be
 * output.
 */
void
nsPlainTextSerializer::AddToLine(const PRUnichar * aLineFragment, 
                                 PRInt32 aLineFragmentLength)
{
  PRUint32 prefixwidth = (mCiteQuoteLevel > 0 ? mCiteQuoteLevel + 1:0)+mIndent;
  
  if (mLineBreakDue)
    EnsureVerticalSpace(mFloatingLines);

  PRInt32 linelength = mCurrentLine.Length();
  if(0 == linelength) {
    if(0 == aLineFragmentLength) {
      // Nothing at all. Are you kidding me?
      return;
    }

    if(mFlags & nsIDocumentEncoder::OutputFormatFlowed) {
      if(
         (
          '>' == aLineFragment[0] ||
          ' ' == aLineFragment[0] ||
          !nsCRT::strncmp(aLineFragment, NS_LITERAL_STRING("From ").get(), 5)
          )
         && mCiteQuoteLevel == 0  // We space-stuff quoted lines anyway
         )
        {
          // Space stuffing a la RFC 2646 (format=flowed).
          mCurrentLine.Append(PRUnichar(' '));
          
          if(MayWrap()) {
            mCurrentLineWidth += GetUnicharWidth(' ');
#ifdef DEBUG_wrapping
            NS_ASSERTION(GetUnicharStringWidth(mCurrentLine.get(),
                                               mCurrentLine.Length()) ==
                         (PRInt32)mCurrentLineWidth,
                         "mCurrentLineWidth and reality out of sync!");
#endif
          }
        }
    }
    mEmptyLines=-1;
  }
    
  mCurrentLine.Append(aLineFragment, aLineFragmentLength);
  if(MayWrap()) {
    mCurrentLineWidth += GetUnicharStringWidth(aLineFragment,
                                               aLineFragmentLength);
#ifdef DEBUG_wrapping
    NS_ASSERTION(GetUnicharstringWidth(mCurrentLine.get(),
                                       mCurrentLine.Length()) ==
                 (PRInt32)mCurrentLineWidth,
                 "mCurrentLineWidth and reality out of sync!");
#endif
  }

  linelength = mCurrentLine.Length();

  //  Wrap?
  if(MayWrap())
  {
#ifdef DEBUG_wrapping
    NS_ASSERTION(GetUnicharstringWidth(mCurrentLine.get(),
                                  mCurrentLine.Length()) ==
                 (PRInt32)mCurrentLineWidth,
                 "mCurrentLineWidth and reality out of sync!");
#endif
    // Yes, wrap!
    // The "+4" is to avoid wrap lines that only would be a couple
    // of letters too long. We give this bonus only if the
    // wrapcolumn is more than 20.
    PRUint32 bonuswidth = (mWrapColumn > 20) ? 4 : 0;

    // XXX: Should calculate prefixwidth with GetUnicharStringWidth
    while(mCurrentLineWidth+prefixwidth > mWrapColumn+bonuswidth) {
      // Must wrap. Let's find a good place to do that.
      nsresult result = NS_OK;
      
      // We go from the end removing one letter at a time until
      // we have a reasonable width
      PRInt32 goodSpace = mCurrentLine.Length();
      PRUint32 width = mCurrentLineWidth;
      while(goodSpace > 0 && (width+prefixwidth > mWrapColumn)) {
        goodSpace--;
        width -= GetUnicharWidth(mCurrentLine[goodSpace]);
      }

      goodSpace++;
      
      PRBool oNeedMoreText;
      if (nsnull != mLineBreaker) {
        result = mLineBreaker->Prev(mCurrentLine.get(), 
                                    mCurrentLine.Length(), goodSpace,
                                    (PRUint32 *) &goodSpace, &oNeedMoreText);
        if (oNeedMoreText) {
          goodSpace = -1;
        }
        else if (nsCRT::IsAsciiSpace(mCurrentLine.CharAt(goodSpace-1))) {
          --goodSpace;    // adjust the position since line breaker returns a position next to space
        }
      }
      // fallback if the line breaker is unavailable or failed
      if (nsnull == mLineBreaker || NS_FAILED(result)) {
        goodSpace = mWrapColumn-prefixwidth;
        while (goodSpace >= 0 &&
               !nsCRT::IsAsciiSpace(mCurrentLine.CharAt(goodSpace))) {
          goodSpace--;
        }
      }
      
      nsAutoString restOfLine;
      if (goodSpace < 0) {
        // If we don't found a good place to break, accept long line and
        // try to find another place to break
        goodSpace=(prefixwidth>mWrapColumn+1)?1:mWrapColumn-prefixwidth+1;
        result = NS_OK;
        if (nsnull != mLineBreaker) {
          result = mLineBreaker->Next(mCurrentLine.get(), 
                                      mCurrentLine.Length(), goodSpace,
                                      (PRUint32 *) &goodSpace, &oNeedMoreText);
        }
        // fallback if the line breaker is unavailable or failed
        if (nsnull == mLineBreaker || NS_FAILED(result)) {
          goodSpace=(prefixwidth>mWrapColumn)?1:mWrapColumn-prefixwidth;
          while (goodSpace < linelength &&
                 !nsCRT::IsAsciiSpace(mCurrentLine.CharAt(goodSpace))) {
            goodSpace++;
          }
        }
      }
      
      if((goodSpace < linelength) && (goodSpace > 0)) {
        // Found a place to break

        // -1 (trim a char at the break position)
        // only if the line break was a space.
        if (nsCRT::IsAsciiSpace(mCurrentLine.CharAt(goodSpace))) {
          mCurrentLine.Right(restOfLine, linelength-goodSpace-1);
        }
        else {
          mCurrentLine.Right(restOfLine, linelength-goodSpace);
        }
        mCurrentLine.Truncate(goodSpace); 
        EndLine(PR_TRUE);
        mCurrentLine.Truncate();
        // Space stuff new line?
        if(mFlags & nsIDocumentEncoder::OutputFormatFlowed) {
          if(
              !restOfLine.IsEmpty()
              &&
              (
                restOfLine[0] == '>' ||
                restOfLine[0] == ' ' ||
                StringBeginsWith(restOfLine, NS_LITERAL_STRING("From "))
              )
              && mCiteQuoteLevel == 0  // We space-stuff quoted lines anyway
            )
          {
            // Space stuffing a la RFC 2646 (format=flowed).
            mCurrentLine.Append(PRUnichar(' '));
            //XXX doesn't seem to work correctly for ' '
          }
        }
        mCurrentLine.Append(restOfLine);
        mCurrentLineWidth = GetUnicharStringWidth(mCurrentLine.get(),
                                                  mCurrentLine.Length());
        linelength = mCurrentLine.Length();
        mEmptyLines = -1;
      } 
      else {
        // Nothing to do. Hopefully we get more data later
        // to use for a place to break line
        break;
      }
    }
  } 
  else {
    // No wrapping.
  }
}

/**
 * Outputs the contents of mCurrentLine, and resets line specific
 * variables. Also adds an indentation and prefix if there is
 * one specified. Strips ending spaces from the line if it isn't
 * preformatted.
 */
void
nsPlainTextSerializer::EndLine(PRBool aSoftlinebreak)
{
  PRUint32 currentlinelength = mCurrentLine.Length();

  if(aSoftlinebreak && 0 == currentlinelength) {
    // No meaning
    return;
  }
  
  // In non-preformatted mode, remove SPACE from the end
  // of the line, unless we got "-- " in a format=flowed
  // output. "-- " is the sig delimiter by convention and
  // shouldn't be touched even in format=flowed
  // (see RFC 2646). We only check for "-- " when it's a hard line
  // break for obvious reasons.
  if(!(mFlags & nsIDocumentEncoder::OutputPreformatted) &&
     (aSoftlinebreak || !mCurrentLine.EqualsLiteral("-- "))) {
    // Remove SPACE:s from the end of the line.
    while(currentlinelength > 0 &&
          mCurrentLine[currentlinelength-1] == ' ') {
      --currentlinelength;
    }
    mCurrentLine.SetLength(currentlinelength);
  }
  
  if(aSoftlinebreak &&
     (mFlags & nsIDocumentEncoder::OutputFormatFlowed) &&
     (mIndent == 0)) {
    // Add the soft part of the soft linebreak (RFC 2646 4.1)
    // We only do this when there is no indentation since format=flowed
    // lines and indentation doesn't work well together.
    mCurrentLine.Append(PRUnichar(' '));
  }

  if(aSoftlinebreak) {
    mEmptyLines=0;
  } 
  else {
    // Hard break
    if(!mCurrentLine.IsEmpty() || !mInIndentString.IsEmpty()) {
      mEmptyLines=-1;
    }

    mEmptyLines++;
  }

  if(mAtFirstColumn) {
    // If we don't have anything "real" to output we have to
    // make sure the indent doesn't end in a space since that
    // would trick a format=flowed-aware receiver.
    PRBool stripTrailingSpaces = mCurrentLine.IsEmpty();
    OutputQuotesAndIndent(stripTrailingSpaces);
  }

  mCurrentLine.Append(mLineBreak);
  Output(mCurrentLine);
  mCurrentLine.Truncate();
  mCurrentLineWidth = 0;
  mAtFirstColumn=PR_TRUE;
  mInWhitespace=PR_TRUE;
  mLineBreakDue = PR_FALSE;
  mFloatingLines = -1;
}


/**
 * Outputs the calculated and stored indent and text in the indentation. That is
 * quote chars and numbers for numbered lists and such. It will also reset any
 * stored text to put in the indentation after using it.
 */
void
nsPlainTextSerializer::OutputQuotesAndIndent(PRBool stripTrailingSpaces /* = PR_FALSE */)
{
  nsAutoString stringToOutput;
  
  // Put the mail quote "> " chars in, if appropriate:
  if (mCiteQuoteLevel > 0) {
    nsAutoString quotes;
    for(int i=0; i < mCiteQuoteLevel; i++) {
      quotes.Append(PRUnichar('>'));
    }
    if (!mCurrentLine.IsEmpty()) {
      /* Better don't output a space here, if the line is empty,
         in case a recieving f=f-aware UA thinks, this were a flowed line,
         which it isn't - it's just empty.
         (Flowed lines may be joined with the following one,
         so the empty line may be lost completely.) */
      quotes.Append(PRUnichar(' '));
    }
    stringToOutput = quotes;
    mAtFirstColumn = PR_FALSE;
  }
  
  // Indent if necessary
  PRInt32 indentwidth = mIndent - mInIndentString.Length();
  if (indentwidth > 0
      && (!mCurrentLine.IsEmpty() || !mInIndentString.IsEmpty())
      // Don't make empty lines look flowed
      ) {
    nsAutoString spaces;
    for (int i=0; i < indentwidth; ++i)
      spaces.Append(PRUnichar(' '));
    stringToOutput += spaces;
    mAtFirstColumn = PR_FALSE;
  }
  
  if(!mInIndentString.IsEmpty()) {
    stringToOutput += mInIndentString;
    mAtFirstColumn = PR_FALSE;
    mInIndentString.Truncate();
  }

  if(stripTrailingSpaces) {
    PRInt32 lineLength = stringToOutput.Length();
    while(lineLength > 0 &&
          ' ' == stringToOutput[lineLength-1]) {
      --lineLength;
    }
    stringToOutput.SetLength(lineLength);
  }

  if(!stringToOutput.IsEmpty()) {
    Output(stringToOutput);
  }
    
}

/**
 * Write a string. This is the highlevel function to use to get text output.
 * By using AddToLine, Output, EndLine and other functions it handles quotation,
 * line wrapping, indentation, whitespace compression and other things.
 */
void
nsPlainTextSerializer::Write(const nsAString& aString)
{
#ifdef DEBUG_wrapping
  printf("Write(%s): wrap col = %d\n",
         NS_ConvertUCS2toUTF8(aString).get(), mWrapColumn);
#endif

  PRInt32 bol = 0;
  PRInt32 newline;
  
  PRInt32 totLen = aString.Length();

  // If the string is empty, do nothing:
  if (totLen <= 0) return;

  // We have two major codepaths here. One that does preformatted text and one
  // that does normal formatted text. The one for preformatted text calls
  // Output directly while the other code path goes through AddToLine.
  if ((mPreFormatted && !mWrapColumn) || IsInPre()
      || ((((!mQuotesPreformatted && mSpanLevel > 0) || mDontWrapAnyQuotes))
          && mEmptyLines >= 0 && aString.First() == PRUnichar('>'))) {
    // No intelligent wrapping.

    // This mustn't be mixed with intelligent wrapping without clearing
    // the mCurrentLine buffer before!!!
    NS_WARN_IF_FALSE(mCurrentLine.IsEmpty(),
                 "Mixed wrapping data and nonwrapping data on the same line");
    if (!mCurrentLine.IsEmpty()) {
      FlushLine();
    }
    
    // Put the mail quote "> " chars in, if appropriate.
    // Have to put it in before every line.
    while(bol<totLen) {
      if(mAtFirstColumn) {
        OutputQuotesAndIndent();
      }

      // Find one of '\n' or '\r' using iterators since nsAString
      // doesn't have the old FindCharInSet function.
      nsAString::const_iterator iter;           aString.BeginReading(iter);
      nsAString::const_iterator done_searching; aString.EndReading(done_searching);
      iter.advance(bol); 
      PRInt32 new_newline = bol;
      newline = kNotFound;
      while(iter != done_searching) {
        if('\n' == *iter || '\r' == *iter) {
          newline = new_newline;
          break;
        }
        ++new_newline;
        ++iter;
      }

      // Done searching
      if(newline == kNotFound) {
        // No new lines.
        nsAutoString stringpart(Substring(aString, bol, totLen - bol));
        if(!stringpart.IsEmpty()) {
          PRUnichar lastchar = stringpart[stringpart.Length()-1];
          if((lastchar == '\t') || (lastchar == ' ') ||
             (lastchar == '\r') ||(lastchar == '\n')) {
            mInWhitespace = PR_TRUE;
          } 
          else {
            mInWhitespace = PR_FALSE;
          }
        }
        Output(stringpart);
        mEmptyLines=-1;
        mAtFirstColumn = mAtFirstColumn && (totLen-bol)==0;
        bol = totLen;
      } 
      else {
        // There is a newline
        nsAutoString stringpart(Substring(aString, bol, newline-bol));
        mInWhitespace = PR_TRUE;
        Output(stringpart);
        // and write the newline
        Output(mLineBreak);
        mEmptyLines=0;
        mAtFirstColumn = PR_TRUE;
        bol = newline+1;
        if('\r' == *iter && bol < totLen && '\n' == *++iter) {
          // There was a CRLF in the input. This used to be illegal and
          // stripped by the parser. Apparently not anymore. Let's skip
          // over the LF.
          bol++;
        }
      }
    }

#ifdef DEBUG_wrapping
    printf("No wrapping: newline is %d, totLen is %d\n",
           newline, totLen);
#endif
    return;
  }

  // XXX Copy necessary to use nsString methods and gain
  // access to underlying buffer
  nsAutoString str(aString);

  // Intelligent handling of text
  // If needed, strip out all "end of lines"
  // and multiple whitespace between words
  PRInt32 nextpos;
  nsAutoString tempstr;
  const PRUnichar * offsetIntoBuffer = nsnull;
  
  while (bol < totLen) {    // Loop over lines
    // Find a place where we may have to do whitespace compression
    nextpos = str.FindCharInSet(" \t\n\r", bol);
#ifdef DEBUG_wrapping
    nsAutoString remaining;
    str.Right(remaining, totLen - bol);
    foo = ToNewCString(remaining);
    //    printf("Next line: bol = %d, newlinepos = %d, totLen = %d, string = '%s'\n",
    //           bol, nextpos, totLen, foo);
    nsMemory::Free(foo);
#endif

    if(nextpos == kNotFound) {
      // The rest of the string
      offsetIntoBuffer = str.get() + bol;
      AddToLine(offsetIntoBuffer, totLen-bol);
      bol=totLen;
      mInWhitespace=PR_FALSE;
    } 
    else {
      // There's still whitespace left in the string
      if (nextpos != 0 && (nextpos + 1) < totLen) {
        offsetIntoBuffer = str.get() + nextpos;
        // skip '\n' if it is between CJ chars
        if (offsetIntoBuffer[0] == '\n' && IS_CJ_CHAR(offsetIntoBuffer[-1]) && IS_CJ_CHAR(offsetIntoBuffer[1])) {
          offsetIntoBuffer = str.get() + bol;
          AddToLine(offsetIntoBuffer, nextpos-bol);
          bol = nextpos + 1;
          continue;
        }
      }
      // If we're already in whitespace and not preformatted, just skip it:
      if (mInWhitespace && (nextpos == bol) && !mPreFormatted &&
          !(mFlags & nsIDocumentEncoder::OutputPreformatted)) {
        // Skip whitespace
        bol++;
        continue;
      }

      if(nextpos == bol) {
        // Note that we are in whitespace.
        mInWhitespace = PR_TRUE;
        offsetIntoBuffer = str.get() + nextpos;
        AddToLine(offsetIntoBuffer, 1);
        bol++;
        continue;
      }
      
      mInWhitespace = PR_TRUE;
      
      offsetIntoBuffer = str.get() + bol;
      if(mPreFormatted || (mFlags & nsIDocumentEncoder::OutputPreformatted)) {
        // Preserve the real whitespace character
        nextpos++;
        AddToLine(offsetIntoBuffer, nextpos-bol);
        bol = nextpos;
      } 
      else {
        // Replace the whitespace with a space
        AddToLine(offsetIntoBuffer, nextpos-bol);
        AddToLine(kSpace.get(),1);
        bol = nextpos + 1; // Let's eat the whitespace
      }
    }
  } // Continue looping over the string
}


/**
 * Gets the value of an attribute in a string. If the function returns
 * NS_ERROR_NOT_AVAILABLE, there was none such attribute specified.
 */
nsresult
nsPlainTextSerializer::GetAttributeValue(const nsIParserNode* aNode,
                                         nsIAtom* aName,
                                         nsString& aValueRet)
{
  if (mContent) {
    if (NS_CONTENT_ATTR_NOT_THERE != mContent->GetAttr(kNameSpaceID_None,
                                                       aName, aValueRet)) {
      return NS_OK;
    }
  }
  else if (aNode) {
    nsAutoString name; 
    aName->ToString(name);

    PRInt32 count = aNode->GetAttributeCount();
    for (PRInt32 i=0;i<count;i++) {
      const nsAString& key = aNode->GetKeyAt(i);
      if (key.Equals(name, nsCaseInsensitiveStringComparator())) {
        aValueRet = aNode->GetValueAt(i);
        return NS_OK;
      }
    }
  }

  return NS_ERROR_NOT_AVAILABLE;
}

/**
 * Returns true, if the element was inserted by Moz' TXT->HTML converter.
 * In this case, we should ignore it.
 */
PRBool 
nsPlainTextSerializer::IsCurrentNodeConverted(const nsIParserNode* aNode)
{
  nsAutoString value;
  nsresult rv = GetAttributeValue(aNode, nsHTMLAtoms::kClass, value);
  return (NS_SUCCEEDED(rv) &&
          (value.EqualsIgnoreCase("moz-txt", 7) ||
           value.EqualsIgnoreCase("\"moz-txt", 8)));
}


// static
PRInt32
nsPlainTextSerializer::GetIdForContent(nsIContent* aContent)
{
  if (!aContent->IsContentOfType(nsIContent::eHTML)) {
    return eHTMLTag_unknown;
  }

  nsIParserService* parserService = nsContentUtils::GetParserServiceWeakRef();

  PRInt32 id;
  nsresult rv = parserService->HTMLAtomTagToId(aContent->Tag(), &id);
  NS_ASSERTION(NS_SUCCEEDED(rv), "Can't map HTML tag to id!");

  return id;
}

/**
 * Returns true if the id represents an element of block type.
 * Can be used to determine if a new paragraph should be started.
 */
PRBool 
nsPlainTextSerializer::IsBlockLevel(PRInt32 aId)
{
  PRBool isBlock = PR_FALSE;

  nsIParserService* parserService = nsContentUtils::GetParserServiceWeakRef();
  if (parserService) {
    parserService->IsBlock(aId, isBlock);
  }

  return isBlock;
}

/**
 * Returns true if the id represents a container.
 */
PRBool 
nsPlainTextSerializer::IsContainer(PRInt32 aId)
{
  PRBool isContainer = PR_FALSE;

  nsIParserService* parserService = nsContentUtils::GetParserServiceWeakRef();
  if (parserService) {
    parserService->IsContainer(aId, isContainer);
  }

  return isContainer;
}

/**
 * Returns true if we currently are inside a <pre>. The check is done
 * by traversing the tag stack looking for <pre> until we hit a block
 * level tag which is assumed to override any <pre>:s below it in
 * the stack. To do this correctly to a 100% would require access
 * to style which we don't support in this converter.
 */  
PRBool
nsPlainTextSerializer::IsInPre()
{
  PRInt32 i = mTagStackIndex;
  while(i > 0) {
    if(mTagStack[i-1] == eHTMLTag_pre)
      return PR_TRUE;
    if(IsBlockLevel(mTagStack[i-1])) {
      // We assume that every other block overrides a <pre>
      return PR_FALSE;
    }
    --i;
  }

  // Not a <pre> in the whole stack
  return PR_FALSE;
}

/**
 * This method is required only to indentify LI's inside OL.
 * Returns TRUE if we are inside an OL tag and FALSE otherwise.
 */
PRBool
nsPlainTextSerializer::IsInOL()
{
  PRInt32 i = mTagStackIndex;
  while(--i >= 0) {
    if(mTagStack[i] == eHTMLTag_ol)
      return PR_TRUE;
    if (mTagStack[i] == eHTMLTag_ul) {
      // If a UL is reached first, LI belongs the UL nested in OL.
      return PR_FALSE;
    }
  }
  // We may reach here for orphan LI's.
  return PR_FALSE;
}

/*
  @return 0 = no header, 1 = h1, ..., 6 = h6
*/
PRInt32 HeaderLevel(eHTMLTags aTag)
{
  PRInt32 result;
  switch (aTag)
  {
    case eHTMLTag_h1:
      result = 1; break;
    case eHTMLTag_h2:
      result = 2; break;
    case eHTMLTag_h3:
      result = 3; break;
    case eHTMLTag_h4:
      result = 4; break;
    case eHTMLTag_h5:
      result = 5; break;
    case eHTMLTag_h6:
      result = 6; break;
    default:
      result = 0; break;
  }
  return result;
}


/*
 * This is an implementation of GetUnicharWidth() and
 * GetUnicharStringWidth() as defined in
 * "The Single UNIX Specification, Version 2, The Open Group, 1997"
 * <http://www.UNIX-systems.org/online.html>
 *
 * Markus Kuhn -- 2000-02-08 -- public domain
 *
 * Minor alterations to fit Mozilla's data types by Daniel Bratell
 */

/* These functions define the column width of an ISO 10646 character
 * as follows:
 *
 *    - The null character (U+0000) has a column width of 0.
 *
 *    - Other C0/C1 control characters and DEL will lead to a return
 *      value of -1.
 *
 *    - Non-spacing and enclosing combining characters (general
 *      category code Mn or Me in the Unicode database) have a
 *      column width of 0.
 *
 *    - Spacing characters in the East Asian Wide (W) or East Asian
 *      FullWidth (F) category as defined in Unicode Technical
 *      Report #11 have a column width of 2.
 *
 *    - All remaining characters (including all printable
 *      ISO 8859-1 and WGL4 characters, Unicode control characters,
 *      etc.) have a column width of 1.
 *
 * This implementation assumes that wchar_t characters are encoded
 * in ISO 10646.
 */

PRInt32 GetUnicharWidth(PRUnichar ucs)
{
  /* sorted list of non-overlapping intervals of non-spacing characters */
  static const struct interval {
    PRUint16 first;
    PRUint16 last;
  } combining[] = {
    { 0x0300, 0x034E }, { 0x0360, 0x0362 }, { 0x0483, 0x0486 },
    { 0x0488, 0x0489 }, { 0x0591, 0x05A1 }, { 0x05A3, 0x05B9 },
    { 0x05BB, 0x05BD }, { 0x05BF, 0x05BF }, { 0x05C1, 0x05C2 },
    { 0x05C4, 0x05C4 }, { 0x064B, 0x0655 }, { 0x0670, 0x0670 },
    { 0x06D6, 0x06E4 }, { 0x06E7, 0x06E8 }, { 0x06EA, 0x06ED },
    { 0x0711, 0x0711 }, { 0x0730, 0x074A }, { 0x07A6, 0x07B0 },
    { 0x0901, 0x0902 }, { 0x093C, 0x093C }, { 0x0941, 0x0948 },
    { 0x094D, 0x094D }, { 0x0951, 0x0954 }, { 0x0962, 0x0963 },
    { 0x0981, 0x0981 }, { 0x09BC, 0x09BC }, { 0x09C1, 0x09C4 },
    { 0x09CD, 0x09CD }, { 0x09E2, 0x09E3 }, { 0x0A02, 0x0A02 },
    { 0x0A3C, 0x0A3C }, { 0x0A41, 0x0A42 }, { 0x0A47, 0x0A48 },
    { 0x0A4B, 0x0A4D }, { 0x0A70, 0x0A71 }, { 0x0A81, 0x0A82 },
    { 0x0ABC, 0x0ABC }, { 0x0AC1, 0x0AC5 }, { 0x0AC7, 0x0AC8 },
    { 0x0ACD, 0x0ACD }, { 0x0B01, 0x0B01 }, { 0x0B3C, 0x0B3C },
    { 0x0B3F, 0x0B3F }, { 0x0B41, 0x0B43 }, { 0x0B4D, 0x0B4D },
    { 0x0B56, 0x0B56 }, { 0x0B82, 0x0B82 }, { 0x0BC0, 0x0BC0 },
    { 0x0BCD, 0x0BCD }, { 0x0C3E, 0x0C40 }, { 0x0C46, 0x0C48 },
    { 0x0C4A, 0x0C4D }, { 0x0C55, 0x0C56 }, { 0x0CBF, 0x0CBF },
    { 0x0CC6, 0x0CC6 }, { 0x0CCC, 0x0CCD }, { 0x0D41, 0x0D43 },
    { 0x0D4D, 0x0D4D }, { 0x0DCA, 0x0DCA }, { 0x0DD2, 0x0DD4 },
    { 0x0DD6, 0x0DD6 }, { 0x0E31, 0x0E31 }, { 0x0E34, 0x0E3A },
    { 0x0E47, 0x0E4E }, { 0x0EB1, 0x0EB1 }, { 0x0EB4, 0x0EB9 },
    { 0x0EBB, 0x0EBC }, { 0x0EC8, 0x0ECD }, { 0x0F18, 0x0F19 },
    { 0x0F35, 0x0F35 }, { 0x0F37, 0x0F37 }, { 0x0F39, 0x0F39 },
    { 0x0F71, 0x0F7E }, { 0x0F80, 0x0F84 }, { 0x0F86, 0x0F87 },
    { 0x0F90, 0x0F97 }, { 0x0F99, 0x0FBC }, { 0x0FC6, 0x0FC6 },
    { 0x102D, 0x1030 }, { 0x1032, 0x1032 }, { 0x1036, 0x1037 },
    { 0x1039, 0x1039 }, { 0x1058, 0x1059 }, { 0x17B7, 0x17BD },
    { 0x17C6, 0x17C6 }, { 0x17C9, 0x17D3 }, { 0x18A9, 0x18A9 },
    { 0x20D0, 0x20E3 }, { 0x302A, 0x302F }, { 0x3099, 0x309A },
    { 0xFB1E, 0xFB1E }, { 0xFE20, 0xFE23 }
  };
  PRInt32 min = 0;
  PRInt32 max = sizeof(combining) / sizeof(struct interval) - 1;
  PRInt32 mid;

  /* test for 8-bit control characters */
  if (ucs == 0)
    return 0;
  if (ucs < 32 || (ucs >= 0x7f && ucs < 0xa0))
    return -1;

  /* first quick check for Latin-1 etc. characters */
  if (ucs < combining[0].first)
    return 1;

  /* binary search in table of non-spacing characters */
  while (max >= min) {
    mid = (min + max) / 2;
    if (combining[mid].last < ucs)
      min = mid + 1;
    else if (combining[mid].first > ucs)
      max = mid - 1;
    else if (combining[mid].first <= ucs && combining[mid].last >= ucs)
      return 0;
  }

  /* if we arrive here, ucs is not a combining or C0/C1 control character */

  /* fast test for majority of non-wide scripts */
  if (ucs < 0x1100)
    return 1;

  return 1 +
    ((ucs >= 0x1100 && ucs <= 0x115f) || /* Hangul Jamo */
     (ucs >= 0x2e80 && ucs <= 0xa4cf && (ucs & ~0x0011) != 0x300a &&
      ucs != 0x303f) ||                  /* CJK ... Yi */
     (ucs >= 0xac00 && ucs <= 0xd7a3) || /* Hangul Syllables */
     (ucs >= 0xf900 && ucs <= 0xfaff) || /* CJK Compatibility Ideographs */
     (ucs >= 0xfe30 && ucs <= 0xfe6f) || /* CJK Compatibility Forms */
     (ucs >= 0xff00 && ucs <= 0xff5f) || /* Fullwidth Forms */
     (ucs >= 0xffe0 && ucs <= 0xffe6));
}


PRInt32 GetUnicharStringWidth(const PRUnichar* pwcs, PRInt32 n)
{
  PRInt32 w, width = 0;

  for (;*pwcs && n-- > 0; pwcs++)
    if ((w = GetUnicharWidth(*pwcs)) < 0)
      ++width; // Taking 1 as the width of non-printable character, for bug# 94475.
    else
      width += w;

  return width;
}

