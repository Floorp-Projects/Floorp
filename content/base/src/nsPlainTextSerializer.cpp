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

#include "nsPlainTextSerializer.h"
#include "nsILineBreakerFactory.h"
#include "nsLWBrkCIID.h"
#include "nsIPref.h"
#include "nsIServiceManager.h"
#include "nsHTMLAtoms.h"
#include "nsIDocumentEncoder.h"
#include "nsIDOMText.h"
#include "nsIDOMElement.h"
#include "nsINameSpaceManager.h"
#include "nsIHTMLContent.h"
#include "nsITextContent.h"
#include "nsTextFragment.h"
#include "nsParserCIID.h"

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
static PRInt32 unicharwidth(PRUnichar ucs);
static PRInt32 unicharwidth(const PRUnichar* pwcs, PRInt32 n);

// Someday may want to make this non-const:
static const PRUint32 TagStackSize = 500;
static const PRUint32 OLStackSize = 100;

nsPlainTextSerializer::nsPlainTextSerializer()
{
  NS_INIT_ISUPPORTS();

  mOutputString = nsnull;
  mInHead = PR_FALSE;
  mColPos = 0;
  mIndent = 0;
  mCiteQuoteLevel = 0;
  mStructs = PR_TRUE;       // will be read from prefs later
  mHeaderStrategy = 1 /*indent increasingly*/;   // ditto
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
  mCacheLine = PR_FALSE;
  mStartedOutput = PR_FALSE;

  // initialize the tag stack to zero:
  mTagStack = new nsHTMLTag[TagStackSize];
  mTagStackIndex = 0;

  // initialize the OL stack, where numbers for ordered lists are kept:
  mOLStack = new PRInt32[OLStackSize];
  mOLStackIndex = 0;
}

nsPlainTextSerializer::~nsPlainTextSerializer()
{
  delete[] mTagStack;
  delete[] mOLStack;
}

NS_IMPL_ISUPPORTS3(nsPlainTextSerializer, 
                   nsIContentSerializer,
                   nsIContentSink,
                   nsIHTMLContentSink)


NS_IMETHODIMP 
nsPlainTextSerializer::Init(PRUint32 aFlags, PRUint32 aWrapColumn)
{
  mFlags = aFlags;

  nsresult rv;
  NS_WITH_SERVICE(nsILineBreakerFactory, lf, kLWBrkCID, &rv);
  if (NS_SUCCEEDED(rv)) {
    nsAutoString lbarg;
    rv = lf->GetBreaker(lbarg, getter_AddRefs(mLineBreaker));
    if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
  }

  // Turn on caching if we are wrapping or we want formatting.
  // We need this even when flags indicate preformatted,
  // in order to wrap textareas with wrap=hard.
  if((mFlags & nsIDocumentEncoder::OutputFormatted) ||
     (mFlags & nsIDocumentEncoder::OutputWrap))
  {
    mCacheLine = PR_TRUE;
  }

  // Set the line break character:
  if ((mFlags & nsIDocumentEncoder::OutputCRLineBreak)
      && (mFlags & nsIDocumentEncoder::OutputLFLineBreak)) // Windows/mail
    mLineBreak.AssignWithConversion("\r\n");
  else if (mFlags & nsIDocumentEncoder::OutputCRLineBreak) // Mac
    mLineBreak.AssignWithConversion("\r");
  else if (mFlags & nsIDocumentEncoder::OutputLFLineBreak) // Unix/DOM
    mLineBreak.AssignWithConversion("\n");
  else
    mLineBreak.AssignWithConversion(NS_LINEBREAK);         // Platform/default

  // Get some prefs
  NS_WITH_SERVICE(nsIPref, prefs, NS_PREF_PROGID, &rv);
  if (NS_SUCCEEDED(rv) && prefs)
  {
    prefs->GetBoolPref(PREF_STRUCTS, &mStructs);
    prefs->GetIntPref(PREF_HEADER_STRATEGY, &mHeaderStrategy);
  }

  return NS_OK;
}

NS_IMETHODIMP 
nsPlainTextSerializer::AppendText(nsIDOMText* aText, 
                                  PRInt32 aStartOffset,
                                  PRInt32 aEndOffset, 
                                  nsAWritableString& aStr)
{
  NS_ENSURE_ARG(aText);

  nsresult rv;
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

  nsAutoString linebuffer;

  // We have to split the string across newlines
  // to match parser behavior
  PRInt32 start = 0;
  PRInt32 offset = textstr.FindCharInSet("\n\r");
  while (offset != kNotFound) {

    // Pass in the line
    textstr.Mid(linebuffer, start, offset-start);
    rv = DoAddLeaf(eHTMLTag_text, linebuffer);
    if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

    // Pass in a newline
    rv = DoAddLeaf(eHTMLTag_newline, mLineBreak);
    if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
    
    start = offset+1;
    offset = textstr.FindCharInSet("\n\r", start);
  }

  // Consume the last bit of the string if there's any left
  if (start < length) {
    if (start) {
      textstr.Mid(linebuffer, start, offset-start);
      return DoAddLeaf(eHTMLTag_text, linebuffer);
    }
    else {
      return DoAddLeaf(eHTMLTag_text, textstr);
    }
  }
  
  return NS_OK;
}

NS_IMETHODIMP 
nsPlainTextSerializer::AppendElementStart(nsIDOMElement *aElement,
                                          nsAWritableString& aStr)
{
  NS_ENSURE_ARG(aElement);

  mContent = do_QueryInterface(aElement);
  if (!mContent) return NS_ERROR_FAILURE;

  nsCOMPtr<nsIAtom> tagname;
  mContent->GetTag(*getter_AddRefs(tagname));

  nsresult rv;
  PRInt32 id;
  rv = GetIdForContent(mContent, &id);
  if (NS_FAILED(rv)) return rv;

  PRBool isContainer = IsContainer(tagname);

  mOutputString = &aStr;

  if (isContainer) {
    rv = DoOpenContainer(id, tagname);
  }
  else {
    nsAutoString empty;
    rv = DoAddLeaf(id, empty);
  }

  mContent = 0;
  mOutputString = nsnull;

  return rv;
} 
 
NS_IMETHODIMP 
nsPlainTextSerializer::AppendElementEnd(nsIDOMElement *aElement,
                                        nsAWritableString& aStr)
{
  NS_ENSURE_ARG(aElement);

  mContent = do_QueryInterface(aElement);
  if (!mContent) return NS_ERROR_FAILURE;

  nsCOMPtr<nsIAtom> tagname;
  mContent->GetTag(*getter_AddRefs(tagname));

  nsresult rv;
  PRInt32 id;
  rv = GetIdForContent(mContent, &id);
  if (NS_FAILED(rv)) return rv;

  PRBool isContainer = IsContainer(tagname);

  mOutputString = &aStr;

  rv = NS_OK;
  if (isContainer) {
    rv = DoCloseContainer(id, tagname);
  }

  mContent = 0;
  mOutputString = nsnull;

  return rv;
}

NS_IMETHODIMP
nsPlainTextSerializer::OpenContainer(const nsIParserNode& aNode)
{
  PRInt32 type = aNode.GetNodeType();
  const nsString&   namestr = aNode.GetText();
  nsCOMPtr<nsIAtom> name = dont_AddRef(NS_NewAtom(namestr));

  return DoOpenContainer(type, name);
}

NS_IMETHODIMP 
nsPlainTextSerializer::CloseContainer(const nsIParserNode& aNode)
{
  PRInt32 type = aNode.GetNodeType();
  const nsString&   namestr = aNode.GetText();
  nsCOMPtr<nsIAtom> name = dont_AddRef(NS_NewAtom(namestr));
  
  return DoCloseContainer(type, name);
}
 
NS_IMETHODIMP 
nsPlainTextSerializer::AddLeaf(const nsIParserNode& aNode)
{
  PRInt32 type = aNode.GetNodeType();
  const nsString& text = aNode.GetText();

  return DoAddLeaf(type, text);
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
nsPlainTextSerializer::OpenNoscript(const nsIParserNode& aNode)
{
  return OpenContainer(aNode);
}

NS_IMETHODIMP 
nsPlainTextSerializer::CloseNoscript(const nsIParserNode& aNode)
{
  return CloseContainer(aNode);
}

NS_IMETHODIMP 
nsPlainTextSerializer::DoFragment(PRBool aFlag)
{
  return NS_OK;
}

nsresult
nsPlainTextSerializer::DoOpenContainer(PRInt32 aTag, 
                                       nsIAtom* aName)
{
  eHTMLTags type = (eHTMLTags)aTag;

  if (mTagStackIndex < TagStackSize) {
    mTagStack[mTagStackIndex++] = type;
  }

  if (type == eHTMLTag_body) {
    //    body ->  can turn on cacheing unless it's already preformatted
    if(!(mFlags & nsIDocumentEncoder::OutputPreformatted) && 
       ((mFlags & nsIDocumentEncoder::OutputFormatted) ||
        (mFlags & nsIDocumentEncoder::OutputWrap))) {
      mCacheLine = PR_TRUE;
    }

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
       (-1 != (whitespace = style.Find("white-space:")))) {

      if (-1 != style.Find("-moz-pre-wrap", PR_TRUE, whitespace)) {
#ifdef DEBUG_preformatted
        printf("Set mPreFormatted based on style moz-pre-wrap\n");
#endif
        mPreFormatted = PR_TRUE;
        mCacheLine = PR_TRUE;
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
      else if (-1 != style.Find("pre", PR_TRUE, whitespace)) {
#ifdef DEBUG_preformatted
        printf("Set mPreFormatted based on style pre\n");
#endif
        mPreFormatted = PR_TRUE;
        mCacheLine = PR_TRUE;
        mWrapColumn = 0;
      }
    } 
    else {
      mPreFormatted = PR_FALSE;
      mCacheLine = PR_TRUE; // Cache lines unless something else tells us not to
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
      if(mCacheLine) {
        AddToLine(NS_ConvertASCIItoUCS2(" "), 1);
      } 
      else {
        nsAutoString space;
        space.AssignWithConversion(" ");
        WriteSimple(space);
      }
      mInWhitespace = PR_TRUE;
    }
  }
  // Else make sure we'll separate block level tags,
  // even if we're about to leave, before doing any other formatting.
  else if (IsBlockLevel(aName)) {
    EnsureVerticalSpace(0);
  }

  // The rest of this routine is formatted output stuff,
  // which we should skip if we're not formatted:
  if (!(mFlags & nsIDocumentEncoder::OutputFormatted)) {
    return NS_OK;
  }

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
      Write(NS_ConvertASCIItoUCS2(leadup.GetBuffer()));
    }
    else if (mHeaderStrategy == 1) { // indent increasingly
      mIndent += kIndentSizeHeaders;
      for (PRInt32 i = HeaderLevel(type); i > 1; i--) {
           // for h(x), run x-1 times
        mIndent += kIndentIncrementHeaders;
      }
    }
  }
  else if (type == eHTMLTag_ul) {
    // Indent here to support nested list, which aren't included in li :-(
    EnsureVerticalSpace(1); // Must end the current line before we change indent.
    mIndent += kIndentSizeList;
  }
  else if (type == eHTMLTag_ol) {
    EnsureVerticalSpace(1); // Must end the current line before we change indent.
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
        mInIndentString.AppendWithConversion("#");
      }

      mInIndentString.AppendWithConversion('.');

    }
    else {
      mInIndentString.AppendWithConversion('*');
    }
    
    mInIndentString.AppendWithConversion(' ');
  }
  else if (type == eHTMLTag_dl) {
    EnsureVerticalSpace(1);
  }
  else if (type == eHTMLTag_dd) {
    mIndent += kIndentSizeDD;
  }
  else if (type == eHTMLTag_blockquote) {
    EnsureVerticalSpace(1);

    // Find out whether it's a type=cite, and insert "> " instead.
    // Eventually we should get the value of the pref controlling citations,
    // and handle AOL-style citations as well.
    // If we want to support RFC 2646 (and we do!) we have to have:
    // >>>> text
    // >>> fdfd
    // when a mail is sent.
    nsAutoString value;
    nsresult rv = GetAttributeValue(nsHTMLAtoms::type, value);

    if (NS_SUCCEEDED(rv) && value.EqualsWithConversion("cite", PR_TRUE)) {
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
    Write(NS_ConvertASCIItoUCS2("\""));
  }
  else if (type == eHTMLTag_sup && mStructs && !IsCurrentNodeConverted()) {
    Write(NS_ConvertASCIItoUCS2("^"));
  }
  else if (type == eHTMLTag_sub && mStructs && !IsCurrentNodeConverted()) { 
    Write(NS_ConvertASCIItoUCS2("_"));
  }
  else if (type == eHTMLTag_code && mStructs && !IsCurrentNodeConverted()) {
    Write(NS_ConvertASCIItoUCS2("|"));
  }
  else if ((type == eHTMLTag_strong || type == eHTMLTag_b)
           && mStructs && !IsCurrentNodeConverted()) {
    Write(NS_ConvertASCIItoUCS2("*"));
  }
  else if ((type == eHTMLTag_em || type == eHTMLTag_i)
           && mStructs && !IsCurrentNodeConverted()) {
    Write(NS_ConvertASCIItoUCS2("/"));
  }
  else if (type == eHTMLTag_u && mStructs && !IsCurrentNodeConverted()) {
    Write(NS_ConvertASCIItoUCS2("_"));
  }

  return NS_OK;

}

nsresult
nsPlainTextSerializer::DoCloseContainer(PRInt32 aTag, 
                                        nsIAtom* aName)
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
           (type == eHTMLTag_pre) ||
           (type == eHTMLTag_dd) ||
           (type == eHTMLTag_dt)) {
    // Items that should always end a line, but get no more whitespace
    EnsureVerticalSpace(0);
  } 
  else if (IsBlockLevel(aName)
           && type != eHTMLTag_blockquote
           && type != eHTMLTag_script
           && type != eHTMLTag_markupDecl) {
    // All other blocks get 1 vertical space after them
    // in formatted mode, otherwise 0.
    // This is hard. Sometimes 0 is a better number, but
    // how to know?
    EnsureVerticalSpace((mFlags & nsIDocumentEncoder::OutputFormatted)
                        ? 1 : 0);
  }

  // The rest of this routine is formatted output stuff,
  // which we should skip if we're not formatted:
  if (!(mFlags & nsIDocumentEncoder::OutputFormatted)) {
    return NS_OK;
  }

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
  else if (type == eHTMLTag_ul) {
    mIndent -= kIndentSizeList;
  }
  else if (type == eHTMLTag_ol) {
    FlushLine(); // Doing this after decreasing OLStackIndex would be wrong.
    --mOLStackIndex;
    mIndent -= kIndentSizeList;
  }
  else if (type == eHTMLTag_dd) {
    mIndent -= kIndentSizeDD;
  }
  else if (type == eHTMLTag_blockquote) {
    FlushLine();    // Is this needed?

    nsAutoString value;
    nsresult rv = GetAttributeValue(nsHTMLAtoms::type, value);

    if (NS_SUCCEEDED(rv)  && value.EqualsWithConversion("cite", PR_TRUE)) {
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
    temp.AppendWithConversion(">");
    Write(temp);
    mURL.Truncate();
  }
  else if (type == eHTMLTag_q) {
    Write(NS_ConvertASCIItoUCS2("\""));
  }
  else if ((type == eHTMLTag_sup || type == eHTMLTag_sub) 
           && mStructs && !IsCurrentNodeConverted()) {
    Write(NS_ConvertASCIItoUCS2(" "));
  }
  else if (type == eHTMLTag_code && mStructs && !IsCurrentNodeConverted()) {
    Write(NS_ConvertASCIItoUCS2("|"));
  }
  else if ((type == eHTMLTag_strong || type == eHTMLTag_b)
           && mStructs && !IsCurrentNodeConverted()) {
    Write(NS_ConvertASCIItoUCS2("*"));
  }
  else if ((type == eHTMLTag_em || type == eHTMLTag_i)
           && mStructs && !IsCurrentNodeConverted()) {
    Write(NS_ConvertASCIItoUCS2("/"));
  }
  else if (type == eHTMLTag_u && mStructs && !IsCurrentNodeConverted()) {
    Write(NS_ConvertASCIItoUCS2("_"));
  }

  return NS_OK;
}

nsresult
nsPlainTextSerializer::DoAddLeaf(PRInt32 aTag, 
                                 const nsString& aText)
{
  // If we don't want any output, just return
  if (!DoOutput()) {
    return NS_OK;
  }
  
  eHTMLTags type = (eHTMLTags)aTag;
  
  if (mTagStackIndex > 1 && mTagStack[mTagStackIndex-2] == eHTMLTag_select) {
    // Don't output the contents of SELECT elements;
    // Might be nice, eventually, to output just the selected element.
    return NS_OK;
  }
  else if (mTagStackIndex > 0 && mTagStack[mTagStackIndex-1] == eHTMLTag_script) {
    // Don't output the contents of <script> tags;
    return NS_OK;
  }
  else if (type == eHTMLTag_text) {
    /* Check, if some other MUA (e.g. 4.x) recognized the URL in
       plain text and inserted an <a> element. If yes, output only once. */
    if (!mURL.IsEmpty() && mURL.Equals(aText)) {
      mURL.Truncate();
    }
    if (
         // Bug 31994 says we shouldn't output the contents of SELECT elements.
         mTagStackIndex <= 0 ||
         mTagStack[mTagStackIndex-1] != eHTMLTag_select
         ) {
      Write(aText);
    }
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
        ((mFlags & nsIDocumentEncoder::OutputFormatted)
         && (mTagStackIndex > 0)
         && (mTagStack[mTagStackIndex-1] == eHTMLTag_pre)) ||
        (mPreFormatted && !mWrapColumn)) {
      Write(aText); // XXX: spacestuffing (maybe call AddToLine if mCacheLine==true)
    }
    else if(!mInWhitespace ||
            (!mStartedOutput
             && mFlags | nsIDocumentEncoder::OutputSelectionOnly)) {
      mInWhitespace = PR_FALSE;
      Write( NS_ConvertToString(" ") );
      mInWhitespace = PR_TRUE;
    }
  }
  else if (type == eHTMLTag_newline) {
    if (mFlags & nsIDocumentEncoder::OutputPreformatted ||
        ((mFlags & nsIDocumentEncoder::OutputFormatted)
         && (mTagStackIndex > 0)
         && (mTagStack[mTagStackIndex-1] == eHTMLTag_pre)) ||
        (mPreFormatted && !mWrapColumn)) {
      EnsureVerticalSpace(mEmptyLines+1);
    }
    else {
      Write(NS_ConvertASCIItoUCS2(" "));
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
      line.AppendWithConversion('-');
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
        temp.AppendWithConversion(" ["); // Should we output chars at all here?
        desc.StripChars("\"");
        temp += desc;
        temp.AppendWithConversion("] ");
      }
      // If the alt attribute has an empty value (|alt=""|), output nothing
    }
    else if (NS_SUCCEEDED(GetAttributeValue(nsHTMLAtoms::title, desc))
             && !desc.IsEmpty()) {
      temp.AppendWithConversion(" [");
      desc.StripChars("\"");
      temp += desc;
      temp.AppendWithConversion("] ");
    }
    else if (NS_SUCCEEDED(GetAttributeValue(nsHTMLAtoms::src, desc))
             && !desc.IsEmpty()) {
      temp.AppendWithConversion(" <");
      desc.StripChars("\"");
      temp += desc;
      temp.AppendWithConversion("> ");
    }
    if (!temp.IsEmpty()) {
      Write(temp);
    }
  }

  return NS_OK;
}

void
nsPlainTextSerializer::EnsureVerticalSpace(PRInt32 noOfRows)
{
  while(mEmptyLines < noOfRows) {
    EndLine(PR_FALSE);
  }
}

// This empties the current line cache without adding a NEWLINE.
// Should not be used if line wrapping is of importance since
// this function destroys the cache information.
void
nsPlainTextSerializer::FlushLine()
{
  if(mCurrentLine.Length()>0) {
    if(0 == mColPos) {
      WriteQuotesAndIndent();
    }

    WriteSimple(mCurrentLine);
    mColPos += mCurrentLine.Length();
    mCurrentLine.Truncate();
    mCurrentLineWidth = 0;
  }
}

void 
nsPlainTextSerializer::WriteSimple(nsString& aString)
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
      if(('>' == aLineFragment[0]) ||
         (' ' == aLineFragment[0]) ||
         (!nsCRT::strncmp(aLineFragment, "From ", 5))) {

        // Space stuffing a la RFC 2646 (format=flowed).
        mCurrentLine.AppendWithConversion(' ');

        if(MayWrap()) {
          mCurrentLineWidth += unicharwidth(' ');
#ifdef DEBUG_wrapping
          NS_ASSERTION(unicharwidth(mCurrentLine.GetUnicode(),
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
    mCurrentLineWidth += unicharwidth(aLineFragment, aLineFragmentLength);
#ifdef DEBUG_wrapping
    NS_ASSERTION(unicharwidth(mCurrentLine.GetUnicode(),
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
    NS_ASSERTION(unicharwidth(mCurrentLine.GetUnicode(),
                                  mCurrentLine.Length()) ==
                 (PRInt32)mCurrentLineWidth,
                 "mCurrentLineWidth and reality out of sync!");
#endif
    // Yes, wrap!
    // The "+4" is to avoid wrap lines that only would be a couple
    // of letters too long. We give this bonus only if the
    // wrapcolumn is more than 20.
    PRUint32 bonuswidth = (mWrapColumn > 20) ? 4 : 0;

    // XXX: Should calculate prefixwidth with unicharwidth
    while(mCurrentLineWidth+prefixwidth > mWrapColumn+bonuswidth) {
      // Must wrap. Let's find a good place to do that.
      nsresult result = NS_OK;
      
      // We go from the end removing one letter at a time until
      // we have a reasonable width
      PRInt32 goodSpace = mCurrentLine.Length();
      PRUint32 width = mCurrentLineWidth;
      while(goodSpace > 0 && (width+prefixwidth > mWrapColumn)) {
        goodSpace--;
        width -= unicharwidth(mCurrentLine[goodSpace]);
      }

      goodSpace++;
      
      PRBool oNeedMoreText;
      if (nsnull != mLineBreaker) {
        result = mLineBreaker->Prev(mCurrentLine.GetUnicode(), 
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
        goodSpace=mWrapColumn-prefixwidth+1;
        result = NS_OK;
        if (nsnull != mLineBreaker) {
          result = mLineBreaker->Next(mCurrentLine.GetUnicode(), 
                                      mCurrentLine.Length(), goodSpace,
                                      (PRUint32 *) &goodSpace, &oNeedMoreText);
        }
        // fallback if the line breaker is unavailable or failed
        if (nsnull == mLineBreaker || NS_FAILED(result)) {
          goodSpace=mWrapColumn-prefixwidth;
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
          if((restOfLine.Length()>0) &&
             ((restOfLine[0] == '>') ||
              (restOfLine[0] == ' ') ||
              (!restOfLine.CompareWithConversion("From ",PR_FALSE,5)))) {
            // Space stuffing a la RFC 2646 (format=flowed).
            mCurrentLine.AppendWithConversion(' ');
          }
        }
        mCurrentLine.Append(restOfLine);
        mCurrentLineWidth = unicharwidth(mCurrentLine.GetUnicode(),
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

void
nsPlainTextSerializer::EndLine(PRBool softlinebreak)
{
  if(softlinebreak) {
    if(0 == mCurrentLine.Length()) {
      // No meaning
      return;
    }
    WriteQuotesAndIndent();
    // Remove SPACE from the end of the line.
    PRUint32 linelength = mCurrentLine.Length();
    while(linelength > 0 &&
          ' ' == mCurrentLine[--linelength]) {
      mCurrentLine.SetLength(linelength);
    }
    if(mFlags & nsIDocumentEncoder::OutputFormatFlowed) {
      // Add the soft part of the soft linebreak (RFC 2646 4.1)
      mCurrentLine.AppendWithConversion(' ');
    }
    mCurrentLine.Append(mLineBreak);
    WriteSimple(mCurrentLine);
    mCurrentLine.Truncate();
    mCurrentLineWidth = 0;
    mColPos=0;
    mEmptyLines=0;
    mInWhitespace=PR_TRUE;
  } 
  else {
    // Hard break
    if(0 == mColPos) {
      WriteQuotesAndIndent();
    }
    if(mCurrentLine.Length()>0) {
      mEmptyLines=-1;
    }

    // Output current line
    // Remove SPACE from the end of the line, unless we got
    // "-- " in a format=flowed output. "-- " is the sig delimiter
    // by convention and shouldn't be touched even in format=flowed
    // (see RFC 2646).
    nsAutoString sig_delimiter;
    sig_delimiter.AssignWithConversion("-- ");
    PRUint32 currentlinelength = mCurrentLine.Length();
    while((currentlinelength > 0) &&
          (' ' == mCurrentLine[currentlinelength-1]) &&
          (sig_delimiter != mCurrentLine)) {
      mCurrentLine.SetLength(--currentlinelength);
    }

    mCurrentLine.Append(mLineBreak);
    WriteSimple(mCurrentLine);
    mCurrentLine.Truncate();
    mCurrentLineWidth = 0;
    mColPos=0;
    mEmptyLines++;
    mInWhitespace=PR_TRUE;
  }
}

void
nsPlainTextSerializer::WriteQuotesAndIndent()
{
  // Put the mail quote "> " chars in, if appropriate:
  if (mCiteQuoteLevel > 0) {
    nsAutoString quotes;
    for(int i=0; i < mCiteQuoteLevel; i++) {
      quotes.AppendWithConversion('>');
    }
    quotes.AppendWithConversion(' ');
    WriteSimple(quotes);
    mColPos += (mCiteQuoteLevel+1);
  }
  
  // Indent if necessary
  PRInt32 indentwidth = mIndent - mInIndentString.Length();
  if (indentwidth > 0) {
    nsAutoString spaces;
    for (int i=0; i < indentwidth; ++i)
      spaces.AppendWithConversion(' ');
    WriteSimple(spaces);
    mColPos += indentwidth;
  }
  
  if(mInIndentString.Length() > 0) {
    WriteSimple(mInIndentString);
    mColPos += mInIndentString.Length();
    mInIndentString.Truncate();
  }
}

//
// Write a string, wrapping appropriately to mWrapColumn.
// This routine also handles indentation and mail-quoting,
// and so should be used for formatted output even if we're not wrapping.
//
void
nsPlainTextSerializer::Write(const nsString& aString)
{
#ifdef DEBUG_wrapping
  char* foo = aString.ToNewCString();
  printf("Write(%s): wrap col = %d, mColPos = %d\n", foo, mWrapColumn, mColPos);
  nsMemory::Free(foo);
#endif

  PRInt32 bol = 0;
  PRInt32 newline;
  
  PRInt32 totLen = aString.Length();
  
  if (((mTagStackIndex > 0) &&
       (mTagStack[mTagStackIndex-1] == eHTMLTag_pre)) ||
      (mPreFormatted && !mWrapColumn)) {
    // No intelligent wrapping. This mustn't be mixed with
    // intelligent wrapping without clearing the mCurrentLine
    // buffer before!!!

    NS_WARN_IF_FALSE(mCurrentLine.Length() == 0,
                 "Mixed wrapping data and nonwrapping data on the same line");
    if (mCurrentLine.Length() > 0) {
      FlushLine();
    }
    
    // Put the mail quote "> " chars in, if appropriate.
    // Have to put it in before every line.
    while(bol<totLen) {
      if(0 == mColPos) {
        WriteQuotesAndIndent();
      }
      
      newline = aString.FindChar('\n',PR_FALSE,bol);
      
      if(newline < 0) {
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
        WriteSimple(stringpart);
        mEmptyLines=-1;
        mColPos += totLen-bol;
        bol = totLen;
      } 
      else {
        nsAutoString stringpart;
        aString.Mid(stringpart, bol, newline-bol+1);
        mInWhitespace = PR_TRUE;
        WriteSimple(stringpart);
        mEmptyLines=0;
        mColPos=0;
        bol = newline+1;
      }
    }

#ifdef DEBUG_wrapping
    printf("No wrapping: newline is %d, totLen is %d; leaving mColPos = %d\n",
           newline, totLen, mColPos);
#endif
    return;
  }

  // Intelligent handling of text
  // If needed, strip out all "end of lines"
  // and multiple whitespace between words
  PRInt32 nextpos;
  nsAutoString tempstr;
  const PRUnichar * offsetIntoBuffer = nsnull;
  
  while (bol < totLen) {    // Loop over lines
    // Find a place where we may have to do whitespace compression
    nextpos = aString.FindCharInSet(" \t\n\r", bol);
#ifdef DEBUG_wrapping
    nsString remaining;
    aString.Right(remaining, totLen - bol);
    foo = remaining.ToNewCString();
    //    printf("Next line: bol = %d, newlinepos = %d, totLen = %d, string = '%s'\n",
    //           bol, nextpos, totLen, foo);
    nsMemory::Free(foo);
#endif

    if(nextpos < 0) {
      // The rest of the string
      if(!mCacheLine) {
        aString.Right(tempstr, totLen-bol);
        WriteSimple(tempstr);
      } 
      else {
        offsetIntoBuffer = aString.GetUnicode();
        offsetIntoBuffer = &offsetIntoBuffer[bol];
        AddToLine(offsetIntoBuffer, totLen-bol);
      }
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
        if(!mCacheLine) {
          nsAutoString whitestring(aString[nextpos]);
          WriteSimple(whitestring);
        } 
        else {
          offsetIntoBuffer = aString.GetUnicode();
          offsetIntoBuffer = &offsetIntoBuffer[nextpos];
          AddToLine(offsetIntoBuffer, 1);
        }
        bol++;
        continue;
      }
      
      if(!mCacheLine) {
        aString.Mid(tempstr,bol,nextpos-bol);
        if(mFlags & nsIDocumentEncoder::OutputPreformatted) {
          bol = nextpos;
        } 
        else {
          tempstr.AppendWithConversion(" ");
          bol = nextpos + 1;
          mInWhitespace = PR_TRUE;
        }
        WriteSimple(tempstr);
      } 
      else {
         mInWhitespace = PR_TRUE;
         
         offsetIntoBuffer = aString.GetUnicode();
         offsetIntoBuffer = &offsetIntoBuffer[bol];
         if(mPreFormatted || (mFlags & nsIDocumentEncoder::OutputPreformatted)) {
           // Preserve the real whitespace character
           nextpos++;
           AddToLine(offsetIntoBuffer, nextpos-bol);
           bol = nextpos;
         } 
         else {
           // Replace the whitespace with a space
           AddToLine(offsetIntoBuffer, nextpos-bol);
           AddToLine(NS_ConvertToString(" ").GetUnicode(),1);
           bol = nextpos + 1; // Let's eat the whitespace
         }
      }
    }
  } // Continue looping over the string
}

PRBool 
nsPlainTextSerializer::MayWrap()
{
  return mWrapColumn &&
    ((mFlags & nsIDocumentEncoder::OutputFormatted) ||
     (mFlags & nsIDocumentEncoder::OutputWrap));

}

nsresult
nsPlainTextSerializer::GetAttributeValue(nsIAtom* aName,
                                         nsString& aValueRet)
{
  if (mContent) {
    if (NS_CONTENT_ATTR_NOT_THERE != mContent->GetAttribute(kNameSpaceID_None,
                                                            aName, aValueRet)) {
      return NS_OK;
    }
  }
  else if (mParserNode) {
    nsAutoString name; 
    aName->ToString(name);

    PRInt32 count = mParserNode->GetAttributeCount();
    for (PRInt32 i=0;i<count;i++) {
      const nsString& key = mParserNode->GetKeyAt(i);
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
          (value.EqualsWithConversion("txt", PR_TRUE, 3) ||
           value.EqualsWithConversion("\"txt", PR_TRUE, 4)));
}

PRBool 
nsPlainTextSerializer::DoOutput()
{
  return !mInHead;
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

PRBool 
nsPlainTextSerializer::IsBlockLevel(nsIAtom* aAtom)
{
  PRBool isBlock = PR_FALSE;

  nsCOMPtr<nsIParserService> parserService;
  GetParserService(getter_AddRefs(parserService));
  if (parserService) {
    nsAutoString name;
    aAtom->ToString(name);
    parserService->IsBlock(name, isBlock);
  }

  return isBlock;
}

PRBool 
nsPlainTextSerializer::IsContainer(nsIAtom* aAtom)
{
  PRBool isContainer = PR_FALSE;

  nsCOMPtr<nsIParserService> parserService;
  GetParserService(getter_AddRefs(parserService));
  if (parserService) {
    nsAutoString name;
    aAtom->ToString(name);
    parserService->IsContainer(name, isContainer);
  }

  return isContainer;
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
 * This is an implementation of wcwidth() and wcswidth() as defined in
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

PRInt32 unicharwidth(PRUnichar ucs)
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


PRInt32 unicharwidth(const PRUnichar* pwcs, PRInt32 n)
{
  PRInt32 w, width = 0;

  for (;*pwcs && n-- > 0; pwcs++)
    if ((w = unicharwidth(*pwcs)) < 0)
      return -1;
    else
      width += w;

  return width;
}

