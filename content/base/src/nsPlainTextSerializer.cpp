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
 *     Daniel Bratell <bratell@lysator.liu.se>
 *     Ben Bucksch <mozilla@bucksch.org>
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

#include "nsPlainTextSerializer.h"
#include "nsILineBreakerFactory.h"
#include "nsLWBrkCIID.h"
#include "nsIPref.h"
#include "nsIServiceManager.h"
#include "nsHTMLAtoms.h"
#include "nsIDOMText.h"
#include "nsIDOMElement.h"
#include "nsINameSpaceManager.h"
#include "nsIHTMLContent.h"
#include "nsITextContent.h"
#include "nsTextFragment.h"
#include "nsParserCIID.h"
#include "nsContentUtils.h"

static NS_DEFINE_CID(kLWBrkCID, NS_LWBRK_CID);
static NS_DEFINE_CID(kPrefServiceCID, NS_PREF_CID);
static NS_DEFINE_CID(kParserServiceCID, NS_PARSERSERVICE_CID);

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

  return it->QueryInterface(NS_GET_IID(nsIContentSerializer),
                            (void**)aSerializer);
}

nsPlainTextSerializer::nsPlainTextSerializer()
  : kSpace(NS_LITERAL_STRING(" ")) // Init of "constant"
{
  NS_INIT_ISUPPORTS();

  mOutputString = nsnull;
  mInHead = PR_FALSE;
  mAtFirstColumn = PR_TRUE;
  mIndent = 0;
  mCiteQuoteLevel = 0;
  mStructs = PR_TRUE;       // will be read from prefs later
  mHeaderStrategy = 1 /*indent increasingly*/;   // ditto
  mQuotesPreformatted = PR_FALSE;                // ditto
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
                            nsIAtom* aCharSet)
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
  
  nsresult rv;
  
  mFlags = aFlags;
  mWrapColumn = aWrapColumn;

  // Only create a linebreaker if we will handle wrapping.
  if(MayWrap()) {
    nsCOMPtr<nsILineBreakerFactory> lf(do_GetService(kLWBrkCID, &rv));
    if (NS_SUCCEEDED(rv)) {
      nsAutoString lbarg;
      rv = lf->GetBreaker(lbarg, getter_AddRefs(mLineBreaker));
      if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
    }
  }

  // Set the line break character:
  if ((mFlags & nsIDocumentEncoder::OutputCRLineBreak)
      && (mFlags & nsIDocumentEncoder::OutputLFLineBreak)) // Windows
    mLineBreak.AssignWithConversion("\r\n");
  else if (mFlags & nsIDocumentEncoder::OutputCRLineBreak) // Mac
    mLineBreak.Assign(PRUnichar('\r'));
  else if (mFlags & nsIDocumentEncoder::OutputLFLineBreak) // Unix/DOM
    mLineBreak.Assign(PRUnichar('\n'));
  else
    mLineBreak.AssignWithConversion(NS_LINEBREAK);         // Platform/default

  
  if(mFlags & nsIDocumentEncoder::OutputFormatted) {
    // Get some prefs that controls how we do formatted output
    nsCOMPtr<nsIPref> prefs(do_GetService(NS_PREF_CONTRACTID, &rv));
    if (NS_SUCCEEDED(rv) && prefs) {
      prefs->GetBoolPref(PREF_STRUCTS, &mStructs);
      prefs->GetIntPref(PREF_HEADER_STRATEGY, &mHeaderStrategy);
      // The quotesPreformatted pref is a temporary measure. See bug 69638.
      prefs->GetBoolPref("editor.quotesPreformatted", &mQuotesPreformatted);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsPlainTextSerializer::Initialize(nsAWritableString* aOutString,
                                  PRUint32 aFlags, PRUint32 aWrapCol)
{
  nsresult rv = Init(aFlags, aWrapCol, nsnull);
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
                                  nsAWritableString& aStr)
{
  NS_ENSURE_ARG(aText);

  nsresult rv = NS_OK;
  PRInt32 length = 0;
  nsAutoString textstr;

  nsCOMPtr<nsITextContent> content = do_QueryInterface(aText);
  if (!content) return NS_ERROR_FAILURE;
  
  const nsTextFragment* frag;
  content->GetText(&frag);

  if (frag) {
    length = ((aEndOffset == -1) ? frag->GetLength() : aEndOffset) - aStartOffset;
    if (frag->Is2b()) {
      textstr.Assign(frag->Get2b() + aStartOffset, length);
    }
    else {
      textstr.AssignWithConversion(frag->Get1b()+aStartOffset, length);
    }
  }

  mOutputString = &aStr;

  nsAutoString linebuffer;

  // We have to split the string across newlines
  // to match parser behavior
  PRInt32 start = 0;
  PRInt32 offset = textstr.FindCharInSet("\n\r");
  while (offset != kNotFound) {

    if(offset>start) {
      // Pass in the line
      textstr.Mid(linebuffer, start, offset-start);
      rv = DoAddLeaf(eHTMLTag_text, linebuffer);
      if (NS_FAILED(rv)) break;
    }

    // Pass in a newline
    rv = DoAddLeaf(eHTMLTag_newline, mLineBreak);
    if (NS_FAILED(rv)) break;
    
    start = offset+1;
    offset = textstr.FindCharInSet("\n\r", start);
  }

  // Consume the last bit of the string if there's any left
  if (NS_SUCCEEDED(rv) & (start < length)) {
    if (start) {
      textstr.Mid(linebuffer, start, offset-start);
      rv = DoAddLeaf(eHTMLTag_text, linebuffer);
    }
    else {
      rv = DoAddLeaf(eHTMLTag_text, textstr);
    }
  }
  
  mOutputString = nsnull;

  return rv;
}

NS_IMETHODIMP 
nsPlainTextSerializer::AppendElementStart(nsIDOMElement *aElement,
                                          nsAWritableString& aStr)
{
  NS_ENSURE_ARG(aElement);

  mContent = do_QueryInterface(aElement);
  if (!mContent) return NS_ERROR_FAILURE;

  nsresult rv;
  PRInt32 id;
  rv = GetIdForContent(mContent, &id);
  if (NS_FAILED(rv)) return rv;

  PRBool isContainer = IsContainer(id);

  mOutputString = &aStr;

  if (isContainer) {
    rv = DoOpenContainer(id);
  }
  else {
    nsAutoString empty;
    rv = DoAddLeaf(id, empty);
  }

  mContent = 0;
  mOutputString = nsnull;

  if (!mInHead && id == eHTMLTag_head)
    mInHead = PR_TRUE;    

  return rv;
} 
 
NS_IMETHODIMP 
nsPlainTextSerializer::AppendElementEnd(nsIDOMElement *aElement,
                                        nsAWritableString& aStr)
{
  NS_ENSURE_ARG(aElement);

  mContent = do_QueryInterface(aElement);
  if (!mContent) return NS_ERROR_FAILURE;

  nsresult rv;
  PRInt32 id;
  rv = GetIdForContent(mContent, &id);
  if (NS_FAILED(rv)) return rv;

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
nsPlainTextSerializer::Flush(nsAWritableString& aStr)
{
  mOutputString = &aStr;
  FlushLine();
  mOutputString = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsPlainTextSerializer::OpenContainer(const nsIParserNode& aNode)
{
  PRInt32 type = aNode.GetNodeType();

  mParserNode = NS_CONST_CAST(nsIParserNode *, &aNode);
  return DoOpenContainer(type);
}

NS_IMETHODIMP 
nsPlainTextSerializer::CloseContainer(const nsIParserNode& aNode)
{
  PRInt32 type = aNode.GetNodeType();
  const nsAReadableString&   namestr = aNode.GetText();
  nsCOMPtr<nsIAtom> name = dont_AddRef(NS_NewAtom(namestr));
  
  mParserNode = NS_CONST_CAST(nsIParserNode *, &aNode);
  return DoCloseContainer(type);
}
 
NS_IMETHODIMP 
nsPlainTextSerializer::AddLeaf(const nsIParserNode& aNode)
{
  eHTMLTags type = (eHTMLTags)aNode.GetNodeType();
  const nsAReadableString& text = aNode.GetText();

  mParserNode = NS_CONST_CAST(nsIParserNode *, &aNode);
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
    return DoAddLeaf(type, str);
  }
  else {
    return DoAddLeaf(type, text);
  }
}

NS_IMETHODIMP
nsPlainTextSerializer::OpenHTML(const nsIParserNode& aNode)
{
  return OpenContainer(aNode);
}

NS_IMETHODIMP 
nsPlainTextSerializer::CloseHTML(const nsIParserNode& aNode)
{
  return CloseContainer(aNode);
}

NS_IMETHODIMP 
nsPlainTextSerializer::OpenHead(const nsIParserNode& aNode)
{
  mInHead = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP 
nsPlainTextSerializer::CloseHead(const nsIParserNode& aNode)
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
nsPlainTextSerializer::CloseBody(const nsIParserNode& aNode)
{
  return CloseContainer(aNode);
}

NS_IMETHODIMP 
nsPlainTextSerializer::OpenForm(const nsIParserNode& aNode)
{
  return OpenContainer(aNode);
}

NS_IMETHODIMP 
nsPlainTextSerializer::CloseForm(const nsIParserNode& aNode)
{
  return CloseContainer(aNode);
}

NS_IMETHODIMP 
nsPlainTextSerializer::OpenMap(const nsIParserNode& aNode)
{
  return OpenContainer(aNode);
}

NS_IMETHODIMP 
nsPlainTextSerializer::CloseMap(const nsIParserNode& aNode)
{
  return CloseContainer(aNode);
}

NS_IMETHODIMP 
nsPlainTextSerializer::OpenFrameset(const nsIParserNode& aNode)
{
  return OpenContainer(aNode);
}

NS_IMETHODIMP 
nsPlainTextSerializer::CloseFrameset(const nsIParserNode& aNode)
{
  return CloseContainer(aNode);
}

NS_IMETHODIMP 
nsPlainTextSerializer::DoFragment(PRBool aFlag)
{
  return NS_OK;
}

nsresult
nsPlainTextSerializer::DoOpenContainer(PRInt32 aTag)
{
  eHTMLTags type = (eHTMLTags)aTag;

  if (mTagStackIndex < TagStackSize) {
    mTagStack[mTagStackIndex++] = type;
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
    if(NS_SUCCEEDED(GetAttributeValue(nsHTMLAtoms::style, style)) &&
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
  else if (type == eHTMLTag_td || type == eHTMLTag_th) {
    // We must make sure that the content of two table cells get a
    // space between them.
    
    // Fow now, I will only add a SPACE. Could be a TAB or something
    // else but I'm not sure everything can handle the TAB so SPACE
    // seems like a better solution.
    if(!mInWhitespace) {
      // Maybe add something else? Several spaces? A TAB? SPACE+TAB?
      AddToLine(kSpace.get(), 1);
      mInWhitespace = PR_TRUE;
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
      mOLStack[mOLStackIndex++] = 1;  // XXX should get it from the node!
    }
    mIndent += kIndentSizeList;  // see ul
  }
  else if (type == eHTMLTag_li) {
    if (mTagStackIndex > 1 && mTagStack[mTagStackIndex-2] == eHTMLTag_ol) {
      if (mOLStackIndex > 0) {
        // This is what nsBulletFrame does for OLs:
        mInIndentString.AppendInt(mOLStack[mOLStackIndex-1]++, 10);
      }
      else {
        mInIndentString.Append(PRUnichar('#'));
      }

      mInIndentString.Append(PRUnichar('.'));

    }
    else {
      mInIndentString.Append(PRUnichar('*'));
    }
    
    mInIndentString.Append(PRUnichar(' '));
  }
  else if (type == eHTMLTag_dl) {
    EnsureVerticalSpace(1);
  }
  else if (type == eHTMLTag_dd) {
    mIndent += kIndentSizeDD;
  }
  else if (type == eHTMLTag_span) {
    ++mSpanLevel;
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

  if (type == eHTMLTag_h1 || type == eHTMLTag_h2 ||
      type == eHTMLTag_h3 || type == eHTMLTag_h4 ||
      type == eHTMLTag_h5 || type == eHTMLTag_h6)
  {
    EnsureVerticalSpace(2);
    if (mHeaderStrategy == 2) {  // numbered
      mIndent += kIndentSizeHeaders;
      // Caching
      nsCAutoString leadup;
      PRInt32 level = HeaderLevel(type);
      // Increase counter for current level
      mHeaderCounter[level]++;
      // Reset all lower levels
      PRInt32 i;

      for (i = level + 1; i <= 6; i++) {
        mHeaderCounter[i] = 0;
      }

      // Construct numbers
      for (i = 1; i <= level; i++) {
        leadup.AppendInt(mHeaderCounter[i]);
        leadup += ".";
      }
      leadup += " ";
      Write(NS_ConvertASCIItoUCS2(leadup.get()));
    }
    else if (mHeaderStrategy == 1) { // indent increasingly
      mIndent += kIndentSizeHeaders;
      for (PRInt32 i = HeaderLevel(type); i > 1; i--) {
           // for h(x), run x-1 times
        mIndent += kIndentIncrementHeaders;
      }
    }
  }
  else if (type == eHTMLTag_blockquote) {
    EnsureVerticalSpace(1);

    nsAutoString value;
    nsresult rv = GetAttributeValue(nsHTMLAtoms::type, value);

    if (NS_SUCCEEDED(rv) && value.EqualsIgnoreCase("cite")) {
      mCiteQuoteLevel++;
    }
    else {
      mIndent += kTabSize; // Check for some maximum value?
    }
  }
  else if (type == eHTMLTag_a && !IsCurrentNodeConverted()) {
    nsAutoString url;
    if (NS_SUCCEEDED(GetAttributeValue(nsHTMLAtoms::href, url))
        && !url.IsEmpty()) {
      mURL = url;
    }
  }
  else if (type == eHTMLTag_q) {
    Write(NS_LITERAL_STRING("\""));
  }
  else if (type == eHTMLTag_sup && mStructs && !IsCurrentNodeConverted()) {
    Write(NS_LITERAL_STRING("^"));
  }
  else if (type == eHTMLTag_sub && mStructs && !IsCurrentNodeConverted()) { 
    Write(NS_LITERAL_STRING("_"));
  }
  else if (type == eHTMLTag_code && mStructs && !IsCurrentNodeConverted()) {
    Write(NS_LITERAL_STRING("|"));
  }
  else if ((type == eHTMLTag_strong || type == eHTMLTag_b)
           && mStructs && !IsCurrentNodeConverted()) {
    Write(NS_LITERAL_STRING("*"));
  }
  else if ((type == eHTMLTag_em || type == eHTMLTag_i)
           && mStructs && !IsCurrentNodeConverted()) {
    Write(NS_LITERAL_STRING("/"));
  }
  else if (type == eHTMLTag_u && mStructs && !IsCurrentNodeConverted()) {
    Write(NS_LITERAL_STRING("_"));
  }

  return NS_OK;

}

nsresult
nsPlainTextSerializer::DoCloseContainer(PRInt32 aTag)
{
  eHTMLTags type = (eHTMLTags)aTag;

  if (mTagStackIndex > 0) {
    --mTagStackIndex;
  }

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
  else if ((type == eHTMLTag_tr) ||
           (type == eHTMLTag_li) ||
           (type == eHTMLTag_dt)) {
    // Items that should always end a line, but get no more whitespace
    EnsureVerticalSpace(0);
  } 
  else if (type == eHTMLTag_pre) {
    EnsureVerticalSpace(1);
  }
  else if (type == eHTMLTag_ul) {
    mIndent -= kIndentSizeList;
    if (--mULCount + mOLStackIndex == 0)
      EnsureVerticalSpace(1);
  }
  else if (type == eHTMLTag_ol) {
    FlushLine(); // Doing this after decreasing OLStackIndex would be wrong.
    mIndent -= kIndentSizeList;
    mOLStackIndex--;
    if (mULCount + mOLStackIndex == 0)
      EnsureVerticalSpace(1);
  }
  else if (type == eHTMLTag_dd) {
    mIndent -= kIndentSizeDD;
  }
  else if (type == eHTMLTag_span) {
    --mSpanLevel;
  }

  else if (IsBlockLevel(aTag)
           && type != eHTMLTag_blockquote
           && type != eHTMLTag_script
           && type != eHTMLTag_doctypeDecl
           && type != eHTMLTag_markupDecl) {
    // All other blocks get 1 vertical space after them
    // in formatted mode, otherwise 0.
    // This is hard. Sometimes 0 is a better number, but
    // how to know?
    EnsureVerticalSpace((mFlags & nsIDocumentEncoder::OutputFormatted)
                        ? 1 : 0);
  }

  //////////////////////////////////////////////////////////////
  if (!(mFlags & nsIDocumentEncoder::OutputFormatted)) {
    return NS_OK;
  }
  //////////////////////////////////////////////////////////////
  // The rest of this routine is formatted output stuff,
  // which we should skip if we're not formatted:
  //////////////////////////////////////////////////////////////

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
  else if (type == eHTMLTag_blockquote) {
    FlushLine();    // Is this needed?

    nsAutoString value;
    nsresult rv = GetAttributeValue(nsHTMLAtoms::type, value);

    if (NS_SUCCEEDED(rv)  && value.EqualsIgnoreCase("cite")) {
      mCiteQuoteLevel--;
    }
    else {
      mIndent -= kTabSize;
    }

    EnsureVerticalSpace(1);
  }
  else if (type == eHTMLTag_a && !IsCurrentNodeConverted() && !mURL.IsEmpty()) {
    nsAutoString temp; 
    temp.AssignWithConversion(" <");
    temp += mURL;
    temp.Append(PRUnichar('>'));
    Write(temp);
    mURL.Truncate();
  }
  else if (type == eHTMLTag_q) {
    Write(NS_LITERAL_STRING("\""));
  }
  else if ((type == eHTMLTag_sup || type == eHTMLTag_sub) 
           && mStructs && !IsCurrentNodeConverted()) {
    Write(kSpace);
  }
  else if (type == eHTMLTag_code && mStructs && !IsCurrentNodeConverted()) {
    Write(NS_LITERAL_STRING("|"));
  }
  else if ((type == eHTMLTag_strong || type == eHTMLTag_b)
           && mStructs && !IsCurrentNodeConverted()) {
    Write(NS_LITERAL_STRING("*"));
  }
  else if ((type == eHTMLTag_em || type == eHTMLTag_i)
           && mStructs && !IsCurrentNodeConverted()) {
    Write(NS_LITERAL_STRING("/"));
  }
  else if (type == eHTMLTag_u && mStructs && !IsCurrentNodeConverted()) {
    Write(NS_LITERAL_STRING("_"));
  }

  return NS_OK;
}

nsresult
nsPlainTextSerializer::DoAddLeaf(PRInt32 aTag, 
                                 const nsAReadableString& aText)
{
  // If we don't want any output, just return
  if (!DoOutput()) {
    return NS_OK;
  }
  
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
    nsCOMPtr<nsIParserService> parserService;
    GetParserService(getter_AddRefs(parserService));
    if (parserService) {
      nsAutoString str(aText);
      PRInt32 entity;
      parserService->HTMLConvertEntityToUnicode(str, &entity);
      nsAutoString temp(entity);
      Write(temp);
    }
  }
  else if (type == eHTMLTag_br) {
    // Another egregious editor workaround, see bug 38194:
    // ignore the bogus br tags that the editor sticks here and there.
    nsAutoString typeAttr;
    if (NS_FAILED(GetAttributeValue(nsHTMLAtoms::type, typeAttr))
        || !typeAttr.EqualsWithConversion("_moz")) {
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
        ((mFlags & nsIDocumentEncoder::OutputFormatted)
         && IsInPre())) {
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
       alt, title or src (URI) attribute */
    // See <http://www.w3.org/TR/REC-html40/struct/objects.html#edef-IMG>
    nsAutoString desc, temp;
    if (NS_SUCCEEDED(GetAttributeValue(nsHTMLAtoms::alt, desc))) {
      if (!desc.IsEmpty()) {
        temp.Append(NS_LITERAL_STRING(" [")); // Should we output chars at all here?
        desc.StripChars("\"");
        temp += desc;
        temp.Append(NS_LITERAL_STRING("] "));
      }
      // If the alt attribute has an empty value (|alt=""|), output nothing
    }
    else if (NS_SUCCEEDED(GetAttributeValue(nsHTMLAtoms::title, desc))
             && !desc.IsEmpty()) {
      temp.Append(NS_LITERAL_STRING(" ["));
      desc.StripChars("\"");
      temp += desc;
      temp.Append(NS_LITERAL_STRING("] "));
    }
    else if (NS_SUCCEEDED(GetAttributeValue(nsHTMLAtoms::src, desc))
             && !desc.IsEmpty()) {
      temp.Append(NS_LITERAL_STRING(" <"));
      desc.StripChars("\"");
      temp += desc;
      temp.Append(NS_LITERAL_STRING("> "));
    }
    if (!temp.IsEmpty()) {
      Write(temp);
    }
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
  if(noOfRows >= 0 && mInIndentString.Length() > 0) {
    EndLine(PR_FALSE);
  }

  while(mEmptyLines < noOfRows) {
    EndLine(PR_FALSE);
  }
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
  if(mCurrentLine.Length()>0) {
    if(mAtFirstColumn) {
      OutputQuotesAndIndent(); // XXX: Should we always do this? Bug?
    }

    Output(mCurrentLine);
    mAtFirstColumn = mAtFirstColumn && mCurrentLine.Length() == 0;
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
  if (aString.Length() > 0) {
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
          !nsCRT::strncmp(aLineFragment, "From ", 5)
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
              restOfLine.Length() > 0
              &&
              (
                restOfLine[0] == '>' ||
                restOfLine[0] == ' ' ||
                restOfLine.EqualsWithConversion("From ",PR_FALSE,5)
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
     (aSoftlinebreak || !mCurrentLine.EqualsWithConversion("-- "))) {
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
    if(mCurrentLine.Length()>0 || mInIndentString.Length()>0) {
      mEmptyLines=-1;
    }

    mEmptyLines++;
  }

  if(mAtFirstColumn) {
    // If we don't have anything "real" to output we have to
    // make sure the indent doesn't end in a space since that
    // would trick a format=flowed-aware receiver.
    PRBool stripTrailingSpaces = (mCurrentLine.Length() == 0);
    OutputQuotesAndIndent(stripTrailingSpaces);
  }

  mCurrentLine.Append(mLineBreak);
  Output(mCurrentLine);
  mCurrentLine.Truncate();
  mCurrentLineWidth = 0;
  mAtFirstColumn=PR_TRUE;
  mInWhitespace=PR_TRUE;
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
    if (mCurrentLine.Length() > 0) {
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
      && (mCurrentLine.Length() > 0 || mInIndentString.Length() > 0)
      // Don't make empty lines look flowed
      ) {
    nsAutoString spaces;
    for (int i=0; i < indentwidth; ++i)
      spaces.Append(PRUnichar(' '));
    stringToOutput += spaces;
    mAtFirstColumn = PR_FALSE;
  }
  
  if(mInIndentString.Length() > 0) {
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

  if(stringToOutput.Length() > 0) {
    Output(stringToOutput);
  }
    
}

/**
 * Write a string. This is the highlevel function to use to get text output.
 * By using AddToLine, Output, EndLine and other functions it handles quotation,
 * line wrapping, indentation, whitespace compression and other things.
 */
void
nsPlainTextSerializer::Write(const nsAReadableString& aString)
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
      || (!mQuotesPreformatted && mSpanLevel > 0
          //&& Substring(aString, 0, 1) == NS_LITERAL_STRING(">"))) {
          && aString.First() == PRUnichar('>'))) {
    // No intelligent wrapping.

    // This mustn't be mixed with intelligent wrapping without clearing
    // the mCurrentLine buffer before!!!
    NS_WARN_IF_FALSE(mCurrentLine.Length() == 0,
                 "Mixed wrapping data and nonwrapping data on the same line");
    if (mCurrentLine.Length() > 0) {
      FlushLine();
    }
    
    // Put the mail quote "> " chars in, if appropriate.
    // Have to put it in before every line.
    while(bol<totLen) {
      if(mAtFirstColumn) {
        OutputQuotesAndIndent();
      }

      // Find one of '\n' or '\r' using iterators since nsAReadableString
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
        nsAutoString stringpart;
        aString.Right(stringpart, totLen-bol);
        if(stringpart.Length() > 0) {
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
        nsAutoString stringpart;
        aString.Mid(stringpart, bol, newline-bol);
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
    foo = remaining.ToNewCString();
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
nsPlainTextSerializer::GetAttributeValue(nsIAtom* aName,
                                         nsString& aValueRet)
{
  if (mContent) {
    if (NS_CONTENT_ATTR_NOT_THERE != mContent->GetAttr(kNameSpaceID_None,
                                                       aName, aValueRet)) {
      return NS_OK;
    }
  }
  else if (mParserNode) {
    const PRUnichar *name; 
    aName->GetUnicode(&name);

    PRInt32 count = mParserNode->GetAttributeCount();
    for (PRInt32 i=0;i<count;i++) {
      const nsAReadableString& key = mParserNode->GetKeyAt(i);
      if (key.Equals(name)) {
        aValueRet = mParserNode->GetValueAt(i);
        aValueRet.StripChars("\"");
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
nsPlainTextSerializer::IsCurrentNodeConverted()
{
  nsAutoString value;
  nsresult rv = GetAttributeValue(nsHTMLAtoms::kClass, value);
  return (NS_SUCCEEDED(rv) &&
          (value.EqualsIgnoreCase("moz-txt", 7) ||
           value.EqualsIgnoreCase("\"moz-txt", 8)));
}


nsresult
nsPlainTextSerializer::GetIdForContent(nsIContent* aContent,
                                       PRInt32* aID)
{
  nsCOMPtr<nsIHTMLContent> htmlcontent = do_QueryInterface(aContent);
  if (!htmlcontent) {
    *aID = eHTMLTag_unknown;
    return NS_OK;
  }

  nsCOMPtr<nsIAtom> tagname;
  mContent->GetTag(*getter_AddRefs(tagname));
  if (!tagname) return NS_ERROR_FAILURE;
  
  nsAutoString namestr;
  tagname->ToString(namestr);
  
  nsresult rv;
  nsCOMPtr<nsIParserService> parserService;
  rv = GetParserService(getter_AddRefs(parserService));
  if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

  rv = parserService->HTMLStringTagToId(namestr, aID);
  if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

  return NS_OK;
}

nsresult
nsPlainTextSerializer::GetParserService(nsIParserService** aParserService)
{
  if (!mParserService) {
    nsresult rv;
    mParserService = do_GetService(kParserServiceCID, &rv);
    if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
  }

  CallQueryInterface(mParserService.get(), aParserService);
  return NS_OK;
}

/**
 * Returns true if the id represents an element of block type.
 * Can be used to determine if a new paragraph should be started.
 */
PRBool 
nsPlainTextSerializer::IsBlockLevel(PRInt32 aId)
{
  PRBool isBlock = PR_FALSE;

  nsCOMPtr<nsIParserService> parserService;
  GetParserService(getter_AddRefs(parserService));
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

  nsCOMPtr<nsIParserService> parserService;
  GetParserService(getter_AddRefs(parserService));
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
      return -1;
    else
      width += w;

  return width;
}

