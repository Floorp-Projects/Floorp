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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *     Greg Kostello (original structure)
 *     Akkana Peck <akkana@netscape.com>
 *     Daniel Bratell <bratell@lysator.liu.se>
 *     Ben Bucksch <mozilla@bucksch.org>
 *     Pierre Phaneuf <pp@ludusdesign.com>
 *     Markus Kuhn <Markus.Kuhn@cl.cam.ac.uk>
 */

/**
 * MODULE NOTES:
 * 
 * This file declares the concrete TXT ContentSink class.
 * This class is used during the parsing process as the
 * primary interface between the parser and the content
 * model.
 */

#include "nsHTMLToTXTSinkStream.h"
#include "nsHTMLTokens.h"
#include "nsString.h"
#include "nsIParser.h"
#include "nsHTMLEntities.h"
#include "nsXIFDTD.h"
#include "prprf.h"         // For PR_snprintf()
#include "nsIDocumentEncoder.h"    // for output flags
#include "nsIUnicodeEncoder.h"
#include "nsICharsetAlias.h"
#include "nsIServiceManager.h"
#include "nsICharsetConverterManager.h"
#include "nsILineBreakerFactory.h"
#include "nsLWBrkCIID.h"
#include "nsIOutputStream.h"
#include "nsFileStream.h"
#include "nsIPref.h"

static NS_DEFINE_CID(kCharsetConverterManagerCID, NS_ICHARSETCONVERTERMANAGER_CID);
static NS_DEFINE_CID(kLWBrkCID,                   NS_LWBRK_CID);
static NS_DEFINE_CID(kPrefServiceCID, NS_PREF_CID);
static NS_DEFINE_IID(kCParserIID, NS_IPARSER_IID);
static NS_DEFINE_IID(kCParserCID, NS_PARSER_IID);

#define PREF_STRUCTS "converter.html2txt.structs"
#define PREF_HEADER_STRATEGY "converter.html2txt.header_strategy"
const  PRInt32 gTabSize=4;
const  PRInt32 gOLNumberWidth = 3;
const  PRInt32 gIndentSizeHeaders = 2;       /* Indention of h1, if
                                                mHeaderStrategy = 1 or = 2.
                                                Indention of other headers
                                                is derived from that.
                                                XXX center h1? */
const  PRInt32 gIndentIncrementHeaders = 2;  /* If mHeaderStrategy = 1,
                                                indent h(x+1) this many
                                                columns more than h(x) */
const  PRInt32 gIndentSizeList = (gTabSize > gOLNumberWidth+3) ? gTabSize: gOLNumberWidth+3;
                               // Indention of non-first lines of ul and ol
const  PRInt32 gIndentSizeDD = gTabSize;  // Indention of <dd>

static PRInt32 HeaderLevel(eHTMLTags aTag);
static PRInt32 unicharwidth(PRUnichar ucs);
static PRInt32 unicharwidth(const PRUnichar* pwcs, PRInt32 n);

/**
 *  Inits the encoder instance variable for the sink based on the charset 
 *  
 *  @update  gpk 4/21/99
 *  @param   aCharset
 *  @return  NS_xxx error result
 */
nsresult nsHTMLToTXTSinkStream::InitEncoder(const nsString& aCharset)
{
  nsresult res = NS_OK;
  
  // If the converter is ucs2, then do not use a converter
  if (aCharset.EqualsWithConversion("ucs2"))
  {
    NS_IF_RELEASE(mUnicodeEncoder);
    return res;
  }

  nsICharsetAlias* calias = nsnull;
  res = nsServiceManager::GetService(kCharsetAliasCID,
                                     kICharsetAliasIID,
                                     (nsISupports**)&calias);

  NS_ASSERTION( nsnull != calias, "cannot find charset alias");
  nsAutoString charsetName;charsetName.Assign(aCharset);
  if( NS_SUCCEEDED(res) && (nsnull != calias))
  {
    res = calias->GetPreferred(aCharset, charsetName);
    nsServiceManager::ReleaseService(kCharsetAliasCID, calias);

    if(NS_FAILED(res))
    {
       // failed - unknown alias , fallback to ISO-8859-1
      charsetName.AssignWithConversion("ISO-8859-1");
    }

    nsICharsetConverterManager * ccm = nsnull;
    res = nsServiceManager::GetService(kCharsetConverterManagerCID, 
                                       NS_GET_IID(nsICharsetConverterManager), 
                                       (nsISupports**)&ccm);
    if(NS_SUCCEEDED(res) && (nsnull != ccm))
    {
      nsIUnicodeEncoder * encoder = nsnull;
      res = ccm->GetUnicodeEncoder(&charsetName, &encoder);
      if(NS_SUCCEEDED(res) && (nsnull != encoder))
      {
        NS_IF_RELEASE(mUnicodeEncoder);
        mUnicodeEncoder = encoder;
      }    
      nsServiceManager::ReleaseService(kCharsetConverterManagerCID, ccm);
    }
  }
  return res;
}

/**
 *  This method gets called as part of our COM-like interfaces.
 *  Its purpose is to create an interface to parser object
 *  of some type.
 *  
 *  @update   gpk02/03/99
 *  @param    nsIID  id of object to discover
 *  @param    aInstancePtr ptr to newly discovered interface
 *  @return   NS_xxx result code
 */
nsresult
nsHTMLToTXTSinkStream::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{                                                                        
  if (NULL == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      
  if(aIID.Equals(NS_GET_IID(nsISupports))) {
    *aInstancePtr = (nsIContentSink*)(this);
  }
  else if(aIID.Equals(NS_GET_IID(nsIContentSink))) {
    *aInstancePtr = (nsIContentSink*)(this);
  }
  else if(aIID.Equals(NS_GET_IID(nsIHTMLContentSink))) {
    *aInstancePtr = (nsIHTMLContentSink*)(this);
  }
  else if(aIID.Equals(NS_GET_IID(nsIHTMLToTXTSinkStream))) {
    *aInstancePtr = (nsIHTMLToTXTSinkStream*)(this);
  }
  else {
    *aInstancePtr=0;
    return NS_NOINTERFACE;
  }
  NS_ADDREF_THIS();
  return NS_OK;                                                        
}

NS_IMPL_ADDREF(nsHTMLToTXTSinkStream)
NS_IMPL_RELEASE(nsHTMLToTXTSinkStream)

// Someday may want to make this non-const:
static const PRUint32 TagStackSize = 500;
static const PRUint32 OLStackSize = 100;

/**
 * Construct a content sink stream.
 * @update      gpk02/03/99
 * @param 
 * @return
 */
nsHTMLToTXTSinkStream::nsHTMLToTXTSinkStream()
{
  NS_INIT_REFCNT();
  mDTD = 0;
  mColPos = 0;
  mIndent = 0;
  mCiteQuoteLevel = 0;
  mDoFragment = PR_FALSE;
  mBufferSize = 0;
  mBufferLength = 0;
  mBuffer = nsnull;
  mUnicodeEncoder = nsnull;
  mStructs = PR_TRUE;       // will be read from prefs later
  mHeaderStrategy = 1 /*indent increasingly*/;   // ditto
  for (PRInt32 i = 0; i <= 6; i++)
    mHeaderCounter[i] = 0;

  // Line breaker
  mLineBreaker = nsnull;
  mWrapColumn = 72;     // XXX magic number, we expect someone to reset this
  mCurrentLineWidth = 0;

  // Flow
  mEmptyLines=1; // The start of the document is an "empty line" in itself,
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

/**
 * 
 * @update      gpk02/03/99
 * @param 
 * @return
 */
nsHTMLToTXTSinkStream::~nsHTMLToTXTSinkStream()
{
  if (mCurrentLine.Length() > 0)
    FlushLine(); // We have some left over text in current line. flush it out.
  // This means we didn't have a body or html node -- probably a text control.

  if(mBuffer) 
    delete[] mBuffer;
  delete[] mTagStack;
  delete[] mOLStack;
  NS_IF_RELEASE(mDTD);
  NS_IF_RELEASE(mUnicodeEncoder);
  NS_IF_RELEASE(mLineBreaker);
}

/**
 * 
 * @update      gpk04/30/99
 * @param 
 * @return
 */
NS_IMETHODIMP
nsHTMLToTXTSinkStream::Initialize(nsIOutputStream* aOutStream, 
                                  nsAWritableString* aOutString,
                                  PRUint32 aFlags)
{
  mStream = aOutStream;
  // XXX This is wrong. It violates XPCOM string ownership rules.
  // We're only getting away with this because instances of this
  // class are restricted to single function scope.
  mString = aOutString;
  mFlags = aFlags;

  nsILineBreakerFactory *lf;
  nsresult result = NS_OK;
  
  result = nsServiceManager::GetService(kLWBrkCID,
                                        NS_GET_IID(nsILineBreakerFactory),
                                        (nsISupports **)&lf);
  if (NS_SUCCEEDED(result)) {
    nsAutoString lbarg;
    result = lf->GetBreaker(lbarg, &mLineBreaker);
    if(NS_FAILED(result)) {
      mLineBreaker = nsnull;
    }
    result = nsServiceManager::ReleaseService(kLWBrkCID, lf);
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
  nsresult rv;
  NS_WITH_SERVICE(nsIPref, prefs, NS_PREF_PROGID, &rv);
  if (NS_SUCCEEDED(rv) && prefs)
  {
    rv = prefs->GetBoolPref(PREF_STRUCTS, &mStructs);
    rv = prefs->GetIntPref(PREF_HEADER_STRATEGY, &mHeaderStrategy);
  }

  return result;
}

NS_IMETHODIMP
nsHTMLToTXTSinkStream::SetCharsetOverride(const nsAReadableString* aCharset)
{
  if (aCharset)
  {
    mCharsetOverride.Assign(*aCharset);
    InitEncoder(mCharsetOverride);
  }
  return NS_OK;
}

/**
  * This method gets called by the parser when it encounters
  * a title tag and wants to set the document title in the sink.
  *
  * @update gpk02/03/99
  * @param  nsString reference to new title value
  * @return PR_TRUE if successful. 
  */
NS_IMETHODIMP
nsHTMLToTXTSinkStream::SetTitle(const nsString& aValue)
{
  return NS_OK;
}

/**
  * All these HTML-specific methods may be called, or may not,
  * depending on whether the parser is parsing XIF or HTML.
  * So we can't depend on them; instead, we have Open/CloseContainer
  * do all the specialized work, and the html-specific Open/Close
  * methods must call the more general methods.
  * Since there are so many of them, make a macro:
  */

#define USE_GENERAL_OPEN_METHOD(opentag) \
NS_IMETHODIMP \
nsHTMLToTXTSinkStream::opentag(const nsIParserNode& aNode) \
{ return OpenContainer(aNode); }

#define USE_GENERAL_CLOSE_METHOD(closetag) \
NS_IMETHODIMP \
nsHTMLToTXTSinkStream::closetag(const nsIParserNode& aNode) \
{ return CloseContainer(aNode); }

USE_GENERAL_OPEN_METHOD(OpenHTML)
USE_GENERAL_CLOSE_METHOD(CloseHTML)
USE_GENERAL_OPEN_METHOD(OpenHead)
USE_GENERAL_CLOSE_METHOD(CloseHead)
USE_GENERAL_OPEN_METHOD(OpenBody)
USE_GENERAL_CLOSE_METHOD(CloseBody)
USE_GENERAL_OPEN_METHOD(OpenForm)
USE_GENERAL_CLOSE_METHOD(CloseForm)
USE_GENERAL_OPEN_METHOD(OpenMap)
USE_GENERAL_CLOSE_METHOD(CloseMap)
USE_GENERAL_OPEN_METHOD(OpenFrameset)
USE_GENERAL_CLOSE_METHOD(CloseFrameset)
USE_GENERAL_OPEN_METHOD(OpenNoscript)
USE_GENERAL_CLOSE_METHOD(CloseNoscript)

NS_IMETHODIMP
nsHTMLToTXTSinkStream::DoFragment(PRBool aFlag) 
{
  mDoFragment = aFlag;
  return NS_OK; 
}

/**
 * This gets called when handling illegal contents, especially
 * in dealing with tables. This method creates a new context.
 * 
 * @update 04/04/99 harishd
 * @param aPosition - The position from where the new context begins.
 */
NS_IMETHODIMP
nsHTMLToTXTSinkStream::BeginContext(PRInt32 aPosition) 
{
  return NS_OK;
}

/**
 * This method terminates any new context that got created by
 * BeginContext and switches back to the main context.  
 *
 * @update 04/04/99 harishd
 * @param aPosition - Validates the end of a context.
 */
NS_IMETHODIMP
nsHTMLToTXTSinkStream::EndContext(PRInt32 aPosition)
{
  return NS_OK;
}

/**
 *  This gets called by the parser when you want to add
 *  a PI node to the current container in the content
 *  model.
 *  
 *  @updated gpk02/03/99
 *  @param   
 *  @return  
 */
NS_IMETHODIMP
nsHTMLToTXTSinkStream::AddProcessingInstruction(const nsIParserNode& aNode){
  return NS_OK;
}

/**
 *  This gets called by the parser when it encounters
 *  a DOCTYPE declaration in the HTML document.
 */
NS_IMETHODIMP
nsHTMLToTXTSinkStream::AddDocTypeDecl(const nsIParserNode& aNode, PRInt32 aMode)
{
  // Should probably set DTD
  return NS_OK;
}

/**
 *  This gets called by the parser when you want to add
 *  a comment node to the current container in the content
 *  model.
 *  
 *  @updated gpk02/03/99
 *  @param   
 *  @return  
 */
NS_IMETHODIMP
nsHTMLToTXTSinkStream::AddComment(const nsIParserNode& aNode)
{
  // Skip comments in plaintext output
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLToTXTSinkStream::GetValueOfAttribute(const nsIParserNode& aNode,
                                           char* aMatchKey,
                                           nsString& aValueRet)
{
  nsAutoString matchKey; matchKey.AssignWithConversion(aMatchKey);
  PRInt32 count=aNode.GetAttributeCount();
  for (PRInt32 i=0;i<count;i++)
  {
    const nsString& key = aNode.GetKeyAt(i);
    if (key == matchKey)
    {
      aValueRet = aNode.GetValueAt(i);
      return NS_OK;
    }
  }
  return NS_ERROR_NOT_AVAILABLE;
}

/**
 * Returns true, if the element was inserted by Moz' TXT->HTML converter.
 * In this case, we should ignore it.
 */
PRBool nsHTMLToTXTSinkStream::IsConverted(const nsIParserNode& aNode)
{
  nsAutoString value;
  nsresult rv = GetValueOfAttribute(aNode, "class", value);
  return
    (
      NS_SUCCEEDED(rv)
      &&
      (
        value.EqualsWithConversion("txt", PR_TRUE, 3) ||
        value.EqualsWithConversion("\"txt", PR_TRUE, 4)
      )
    );
}

PRBool nsHTMLToTXTSinkStream::DoOutput()
{
  PRBool inBody = PR_FALSE;

  // Loop over the tag stack and see if we're inside a body,
  // and not inside a markup_declaration
  for (PRUint32 i = 0; i < mTagStackIndex; ++i)
  {
    if (mTagStack[i] == eHTMLTag_markupDecl
        || mTagStack[i] == eHTMLTag_comment)
      return PR_FALSE;

    if (mTagStack[i] == eHTMLTag_body)
      inBody = PR_TRUE;
  }

  return mDoFragment || inBody;
}

    
/**
  * This method is used to open a general container. 
  * This includes: OL,UL,DIR,SPAN,TABLE,H[1..6],etc.
  *
  * @param  nsIParserNode reference to parser node interface
  * @return PR_TRUE if successful. 
  */     
NS_IMETHODIMP
nsHTMLToTXTSinkStream::OpenContainer(const nsIParserNode& aNode)
{
  eHTMLTags type = (eHTMLTags)aNode.GetNodeType();
  const nsString&   name = aNode.GetText();
  if (name.EqualsWithConversion("document_info"))
  {
    nsString value;
    if (NS_SUCCEEDED(GetValueOfAttribute(aNode, "charset", value)))
    {
      if (mCharsetOverride.Length() == 0)
        InitEncoder(value);
      else
        InitEncoder(mCharsetOverride);
    }
    return NS_OK;
  }

  if (mTagStackIndex < TagStackSize)
    mTagStack[mTagStackIndex++] = type;

  if (type == eHTMLTag_body)
  {
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
    nsString style;
    PRInt32 whitespace;
    if(NS_SUCCEEDED(GetValueOfAttribute(aNode, "style", style)) &&
       (-1 != (whitespace = style.Find("white-space:"))))
    {
      if (-1 != style.Find("-moz-pre-wrap", PR_TRUE, whitespace))
      {
#ifdef DEBUG_preformatted
        printf("Set mPreFormatted based on style moz-pre-wrap\n");
#endif
        mPreFormatted = PR_TRUE;
        mCacheLine = PR_TRUE;
        PRInt32 widthOffset = style.Find("width:");
        if (widthOffset >= 0)
        {
          // We have to search for the ch before the semicolon,
          // not for the semicolon itself, because nsString::ToInteger()
          // considers 'c' to be a valid numeric char (even if radix=10)
          // but then gets confused if it sees it next to the number
          // when the radix specified was 10, and returns an error code.
          PRInt32 semiOffset = style.Find("ch", widthOffset+6);
          PRInt32 length = (semiOffset > 0 ? semiOffset - widthOffset - 6
                            : style.Length() - widthOffset);
          nsString widthstr;
          style.Mid(widthstr, widthOffset+6, length);
          PRInt32 err;
          PRInt32 col = widthstr.ToInteger(&err);
          if (NS_SUCCEEDED(err))
          {
            SetWrapColumn((PRUint32)col);
#ifdef DEBUG_preformatted
            printf("Set wrap column to %d based on style\n", mWrapColumn);
#endif
          }
        }
      }
      else if (-1 != style.Find("pre", PR_TRUE, whitespace))
      {
#ifdef DEBUG_preformatted
        printf("Set mPreFormatted based on style pre\n");
#endif
        mPreFormatted = PR_TRUE;
        mCacheLine = PR_TRUE;
        SetWrapColumn(0);
      }
    } else {
      mPreFormatted = PR_FALSE;
      mCacheLine = PR_TRUE; // Cache lines unless something else tells us not to
    }

    return NS_OK;
  }

  if (!DoOutput())
    return NS_OK;

  if (type == eHTMLTag_p || type == eHTMLTag_pre)
    EnsureVerticalSpace(1); // Should this be 0 in unformatted case?

  else if (type == eHTMLTag_td || type == eHTMLTag_th)
  {
    // We must make sure that the content of two table cells get a
    // space between them.
    
    // Fow now, I will only add a SPACE. Could be a TAB or something
    // else but I'm not sure everything can handle the TAB so SPACE
    // seems like a better solution.
    if(!mInWhitespace) {
      // Maybe add something else? Several spaces? A TAB? SPACE+TAB?
      if(mCacheLine) {
        AddToLine(NS_ConvertToString(" ").GetUnicode(), 1);
      } else {
        nsAutoString space(NS_ConvertToString(" "));
        WriteSimple(space);
      }
      mInWhitespace = PR_TRUE;
    }
  }

  // Else make sure we'll separate block level tags,
  // even if we're about to leave, before doing any other formatting.
  else if (IsBlockLevel(type))
    EnsureVerticalSpace(0);

  // The rest of this routine is formatted output stuff,
  // which we should skip if we're not formatted:
  if (!(mFlags & nsIDocumentEncoder::OutputFormatted))
    return NS_OK;

  if (type == eHTMLTag_h1 || type == eHTMLTag_h2 ||
      type == eHTMLTag_h3 || type == eHTMLTag_h4 ||
      type == eHTMLTag_h5 || type == eHTMLTag_h6)
  {
    EnsureVerticalSpace(2);
    if (mHeaderStrategy == 2)  // numbered
    {
      mIndent += gIndentSizeHeaders;
      // Caching
      nsCAutoString leadup;
      PRInt32 level = HeaderLevel(type);
      // Increase counter for current level
      mHeaderCounter[level]++;
      // Reset all lower levels
      PRInt32 i;
      for (i = level + 1; i <= 6; i++)
        mHeaderCounter[i] = 0;
      // Construct numbers
      for (i = 1; i <= level; i++)
      {
        leadup.AppendInt(mHeaderCounter[i]);
        leadup += ".";
      }
      leadup += " ";
      Write(NS_ConvertASCIItoUCS2(leadup.GetBuffer()));
    }
    else if (mHeaderStrategy == 1)  // indent increasingly
    {
      mIndent += gIndentSizeHeaders;
      for (PRInt32 i = HeaderLevel(type); i > 1; i--)
           // for h(x), run x-1 times
        mIndent += gIndentIncrementHeaders;
    }
  }
  else if (type == eHTMLTag_ul)
  {
    // Indent here to support nested list, which aren't included in li :-(
    EnsureVerticalSpace(1); // Must end the current line before we change indent.
    mIndent += gIndentSizeList;
  }
  else if (type == eHTMLTag_ol)
  {
    EnsureVerticalSpace(1); // Must end the current line before we change indent.
    if (mOLStackIndex < OLStackSize)
      mOLStack[mOLStackIndex++] = 1;  // XXX should get it from the node!
    mIndent += gIndentSizeList;  // see ul
  }
  else if (type == eHTMLTag_li)
  {
    if (mTagStackIndex > 1 && mTagStack[mTagStackIndex-2] == eHTMLTag_ol)
    {
      if (mOLStackIndex > 0)
        // This is what nsBulletFrame does for OLs:
        mInIndentString.AppendInt(mOLStack[mOLStackIndex-1]++, 10);
      else
        mInIndentString.AppendWithConversion("#");

      mInIndentString.AppendWithConversion('.');

    }
    else
      mInIndentString.AppendWithConversion('*');
    
    mInIndentString.AppendWithConversion(' ');
  }
  else if (type == eHTMLTag_dl)
    EnsureVerticalSpace(1);
  else if (type == eHTMLTag_dd)
    mIndent += gIndentSizeDD;
  else if (type == eHTMLTag_blockquote)
  {
    EnsureVerticalSpace(1);

    // Find out whether it's a type=cite, and insert "> " instead.
    // Eventually we should get the value of the pref controlling citations,
    // and handle AOL-style citations as well.
    // If we want to support RFC 2646 (and we do!) we have to have:
    // >>>> text
    // >>> fdfd
    // when a mail is sent.
    nsString value;
    nsresult rv = GetValueOfAttribute(aNode, "type", value);
    if ( NS_SUCCEEDED(rv) )
      value.StripChars("\"");

    if (NS_SUCCEEDED(rv)  && value.EqualsWithConversion("cite", PR_TRUE))
      mCiteQuoteLevel++;
    else
      mIndent += gTabSize; // Check for some maximum value?
  }

  else if (type == eHTMLTag_a && !IsConverted(aNode))
  {
    nsAutoString url;
    if (NS_SUCCEEDED(GetValueOfAttribute(aNode, "href", url))
        && !url.IsEmpty())
    {
      url.StripChars("\"");
      mURL = url;
    }
  }
  else if (type == eHTMLTag_q)
    Write(NS_ConvertASCIItoUCS2("\""));
  else if (type == eHTMLTag_sup && mStructs && !IsConverted(aNode))
    Write(NS_ConvertASCIItoUCS2("^"));
  else if (type == eHTMLTag_sub && mStructs && !IsConverted(aNode))
    Write(NS_ConvertASCIItoUCS2("_"));
  else if (type == eHTMLTag_code && mStructs && !IsConverted(aNode))
    Write(NS_ConvertASCIItoUCS2("|"));
  else if ((type == eHTMLTag_strong || type == eHTMLTag_b)
           && mStructs && !IsConverted(aNode))
    Write(NS_ConvertASCIItoUCS2("*"));
  else if ((type == eHTMLTag_em || type == eHTMLTag_i)
           && mStructs && !IsConverted(aNode))
    Write(NS_ConvertASCIItoUCS2("/"));
  else if (type == eHTMLTag_u && mStructs && !IsConverted(aNode))
    Write(NS_ConvertASCIItoUCS2("_"));

  return NS_OK;
}

/**
  * This method is used to close a generic container.
  *
  * @update 07/12/98 gpk
  * @param  nsIParserNode reference to parser node interface
  * @return PR_TRUE if successful. 
  */     
NS_IMETHODIMP
nsHTMLToTXTSinkStream::CloseContainer(const nsIParserNode& aNode)
{
  eHTMLTags type = (eHTMLTags)aNode.GetNodeType();
  if (mTagStackIndex > 0)
    --mTagStackIndex;

  // End current line if we're ending a block level tag
  if((type == eHTMLTag_body) || (type == eHTMLTag_html)) {
    // We want the output to end with a new line,
    // but in preformatted areas like text fields,
    // we can't emit newlines that weren't there.
    // So add the newline only in the case of formatted output.
    if (mFlags & nsIDocumentEncoder::OutputFormatted)
      EnsureVerticalSpace(0);
    else
      FlushLine();
    // We won't want to do anything with these in formatted mode either,
    // so just return now:
    return NS_OK;
  } else if ((type == eHTMLTag_tr) ||
             (type == eHTMLTag_li) ||
             (type == eHTMLTag_pre) ||
             (type == eHTMLTag_dd) ||
             (type == eHTMLTag_dt)) {
    // Items that should always end a line, but get no more whitespace
    EnsureVerticalSpace(0);
  } else if (IsBlockLevel(type)
             && type != eHTMLTag_blockquote
             && type != eHTMLTag_script
             && type != eHTMLTag_markupDecl)
  {
    // All other blocks get 1 vertical space after them
    // in formatted mode, otherwise 0.
    // This is hard. Sometimes 0 is a better number, but
    // how to know?
    EnsureVerticalSpace((mFlags & nsIDocumentEncoder::OutputFormatted)
                        ? 1 : 0);
  }

  // The rest of this routine is formatted output stuff,
  // which we should skip if we're not formatted:
  if (!(mFlags & nsIDocumentEncoder::OutputFormatted))
    return NS_OK;

  if (type == eHTMLTag_h1 || type == eHTMLTag_h2 ||
      type == eHTMLTag_h3 || type == eHTMLTag_h4 ||
      type == eHTMLTag_h5 || type == eHTMLTag_h6)
  {
    if (mHeaderStrategy /*numbered or indent increasingly*/ )
      mIndent -= gIndentSizeHeaders;
    if (mHeaderStrategy == 1 /*indent increasingly*/ )
    {
      for (PRInt32 i = HeaderLevel(type); i > 1; i--)
           // for h(x), run x-1 times
        mIndent -= gIndentIncrementHeaders;
    }
    EnsureVerticalSpace(1);
  }
  else if (type == eHTMLTag_ul)
  {
    mIndent -= gIndentSizeList;
  }
  else if (type == eHTMLTag_ol)
  {
    FlushLine(); // Doing this after decreasing OLStackIndex would be wrong.
    --mOLStackIndex;
    mIndent -= gIndentSizeList;
  }
  else if (type == eHTMLTag_dd)
  {
    mIndent -= gIndentSizeDD;
  }
  else if (type == eHTMLTag_blockquote)
  {
    FlushLine();    // Is this needed?

    nsString value;
    nsresult rv = GetValueOfAttribute(aNode, "type", value);
    if ( NS_SUCCEEDED(rv) )
      value.StripChars("\"");

    if (NS_SUCCEEDED(rv)  && value.EqualsWithConversion("cite", PR_TRUE))
      mCiteQuoteLevel--;
    else
      mIndent -= gTabSize;

    EnsureVerticalSpace(1);
  }
  else if (type == eHTMLTag_a && !IsConverted(aNode) && !mURL.IsEmpty())
  {
    nsAutoString temp; temp.AssignWithConversion(" <");
    temp += mURL;
    temp.AppendWithConversion(">");
    Write(temp);
    mURL.Truncate();
  }
  else if (type == eHTMLTag_q)
    Write(NS_ConvertASCIItoUCS2("\""));
  else if ((type == eHTMLTag_sup || type == eHTMLTag_sub)
           && mStructs && !IsConverted(aNode))
    Write(NS_ConvertASCIItoUCS2(" "));
  else if (type == eHTMLTag_code && mStructs && !IsConverted(aNode))
    Write(NS_ConvertASCIItoUCS2("|"));
  else if ((type == eHTMLTag_strong || type == eHTMLTag_b)
           && mStructs && !IsConverted(aNode))
    Write(NS_ConvertASCIItoUCS2("*"));
  else if ((type == eHTMLTag_em || type == eHTMLTag_i)
           && mStructs && !IsConverted(aNode))
    Write(NS_ConvertASCIItoUCS2("/"));
  else if (type == eHTMLTag_u && mStructs && !IsConverted(aNode))
    Write(NS_ConvertASCIItoUCS2("_"));

  return NS_OK;
}

/**
  * This method is used to add a leaf to the currently 
  * open container.
  *
  * @update 07/12/98 gpk
  * @param  nsIParserNode reference to parser node interface
  * @return PR_TRUE if successful. 
  */     
NS_IMETHODIMP
nsHTMLToTXTSinkStream::AddLeaf(const nsIParserNode& aNode)
{
  // If we don't want any output, just return
  if (!DoOutput())
    return NS_OK;
  
  eHTMLTags type = (eHTMLTags)aNode.GetNodeType();
  
  nsString text = aNode.GetText();

  if (mTagStackIndex > 1 && mTagStack[mTagStackIndex-2] == eHTMLTag_select)
  {
    // Don't output the contents of SELECT elements;
    // Might be nice, eventually, to output just the selected element.
    return NS_OK;
  }
  else if (mTagStackIndex > 0 && mTagStack[mTagStackIndex-1] == eHTMLTag_script)
  {
    // Don't output the contents of <script> tags;
    return NS_OK;
  }
  else if (type == eHTMLTag_text)
  {
    /* Check, if some other MUA (e.g. 4.x) recognized the URL in
       plain text and inserted an <a> element. If yes, output only once. */
    if (!mURL.IsEmpty() && mURL == text)
      mURL.Truncate();
    if (
         // Bug 31994 says we shouldn't output the contents of SELECT elements.
         mTagStackIndex <= 0 ||
         mTagStack[mTagStackIndex-1] != eHTMLTag_select
       )
      Write(text);
  }
  else if (type == eHTMLTag_entity)
  {
    PRUnichar entity = nsHTMLEntities::EntityToUnicode(aNode.GetText());
    nsAutoString temp(entity);
    Write(temp);
  }
  else if (type == eHTMLTag_br)
  {
    // Another egregious editor workaround, see bug 38194:
    // ignore the bogus br tags that the editor sticks here and there.
    nsAutoString typeAttr;
    if (NS_FAILED(GetValueOfAttribute(aNode, "type", typeAttr))
        || !typeAttr.EqualsWithConversion("_moz"))
      EnsureVerticalSpace(mEmptyLines+1);
  }
  else if (type == eHTMLTag_whitespace)
  {
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
        (mPreFormatted && !mWrapColumn))
    {
      Write(text); // XXX: spacestuffing (maybe call AddToLine if mCacheLine==true)
    }
    else if(!mInWhitespace ||
              (!mStartedOutput
               && mFlags | nsIDocumentEncoder::OutputSelectionOnly))
    {
      mInWhitespace = PR_FALSE;
      Write( NS_ConvertToString(" ") );
      mInWhitespace = PR_TRUE;
    }
  }
  else if (type == eHTMLTag_newline)
  {
    if (mFlags & nsIDocumentEncoder::OutputPreformatted ||
        ((mFlags & nsIDocumentEncoder::OutputFormatted)
         && (mTagStackIndex > 0)
         && (mTagStack[mTagStackIndex-1] == eHTMLTag_pre)) ||
        (mPreFormatted && !mWrapColumn))
    {
      EnsureVerticalSpace(mEmptyLines+1);
    }
    else Write(NS_ConvertASCIItoUCS2(" "));
  }
  else if (type == eHTMLTag_hr &&
           (mFlags & nsIDocumentEncoder::OutputFormatted))
  {
    EnsureVerticalSpace(0);

    // Make a line of dashes as wide as the wrap width
    // XXX honoring percentage would be nice
    nsAutoString line;
    PRUint32 width = (mWrapColumn > 0 ? mWrapColumn : 25);
    while (line.Length() < width)
      line.AppendWithConversion('-');
    Write(line);

    EnsureVerticalSpace(0);
  }
  else if (type == eHTMLTag_img)
  {
    /* Output (in decreasing order of preference)
       alt, title or src (URI) attribute */
    // See <http://www.w3.org/TR/REC-html40/struct/objects.html#edef-IMG>
    nsAutoString desc, temp;
    if (NS_SUCCEEDED(GetValueOfAttribute(aNode, "alt", desc)))
    {
      if (!desc.IsEmpty())
      {
        temp.AppendWithConversion(" ["); // Should we output chars at all here?
        desc.StripChars("\"");
        temp += desc;
        temp.AppendWithConversion("]");
      }
      // If the alt attribute has an empty value (|alt=""|), output nothing
    }
    else if (NS_SUCCEEDED(GetValueOfAttribute(aNode, "title", desc))
             && !desc.IsEmpty())
    {
      temp.AppendWithConversion(" [");
      desc.StripChars("\"");
      temp += desc;
      temp.AppendWithConversion("] ");
    }
    else if (NS_SUCCEEDED(GetValueOfAttribute(aNode, "src", desc))
             && !desc.IsEmpty())
    {
      temp.AppendWithConversion(" <");
      desc.StripChars("\"");
      temp += desc;
      temp.AppendWithConversion("> ");
    }
    if (!temp.IsEmpty())
      Write(temp);
  }

  return NS_OK;
}

void nsHTMLToTXTSinkStream::EnsureBufferSize(PRInt32 aNewSize)
{
  if (mBufferSize < aNewSize)
  {
    nsMemory::Free(mBuffer);
    mBufferSize = 2*aNewSize+1; // make the twice as large
    mBuffer = NS_STATIC_CAST(char*, nsMemory::Alloc(mBufferSize));
    if(mBuffer){
      mBuffer[0] = 0;
      mBufferLength = 0;
    }
  }
}

void nsHTMLToTXTSinkStream::EncodeToBuffer(nsString& aSrc)
{
  if (mUnicodeEncoder == nsnull)
  {
    NS_WARNING("The unicode encoder needs to be initialized");
    EnsureBufferSize(aSrc.Length()+1);
    aSrc.ToCString ( mBuffer, aSrc.Length()+1 );
    return;
  }

  PRInt32       length = aSrc.Length();
  nsresult      result;

  if (mUnicodeEncoder != nsnull && length > 0)
  {
    EnsureBufferSize(length);
    mBufferLength = mBufferSize;
    
    mUnicodeEncoder->Reset();
    result = mUnicodeEncoder->Convert(aSrc.GetUnicode(), &length, mBuffer, &mBufferLength);
    mBuffer[mBufferLength] = 0;
    PRInt32 temp = mBufferLength;
    if (NS_SUCCEEDED(result))
      result = mUnicodeEncoder->Finish(mBuffer,&temp);
  }
}

void
nsHTMLToTXTSinkStream::EnsureVerticalSpace(PRInt32 noOfRows)
{
  while(mEmptyLines < noOfRows)
    EndLine(PR_FALSE);
}

// This empties the current line cache without adding a NEWLINE.
// Should not be used if line wrapping is of importance since
// this function destroys the cache information.
void
nsHTMLToTXTSinkStream::FlushLine()
{
  if(mCurrentLine.Length()>0) {
    if(0 == mColPos)
      WriteQuotesAndIndent();


    WriteSimple(mCurrentLine);
    mColPos += mCurrentLine.Length();
    mCurrentLine.Truncate();
    mCurrentLineWidth = 0;
  }
}

/**
 *  WriteSimple places the contents of aString into either the output stream
 *  or the output string.
 *  When going to the stream, all data is run through the encoder.
 *  No formatting or wrapping is done here; that happens in ::Write.
 *  
 *  @updated gpk02/03/99
 *  @param   
 *  @return  
 */
void nsHTMLToTXTSinkStream::WriteSimple(nsString& aString)
{
  if (aString.Length() > 0)
    mStartedOutput = PR_TRUE;

  // First, replace all nbsp characters with spaces,
  // which the unicode encoder won't do for us.
  static PRUnichar nbsp = 160;
  static PRUnichar space = ' ';
  aString.ReplaceChar(nbsp, space);

  // If a encoder is being used then convert first convert the input string
  if (mUnicodeEncoder != nsnull)
  {
    EncodeToBuffer(aString);
    if (mStream != nsnull)
    {
      nsOutputStream out(mStream);
      out.write(mBuffer,mBufferLength);
    }
    if (mString != nsnull)
    {
      mString->Append(NS_ConvertASCIItoUCS2(mBuffer));
    }
  }
  else
  {
    if (mStream != nsnull)
    {
      nsOutputStream out(mStream);
      const PRUnichar* unicode = aString.GetUnicode();
      PRUint32   length = aString.Length();
      out.write(unicode,length);
    }
    else
    {
      mString->Append(aString);
    }
  }
}

void
nsHTMLToTXTSinkStream::AddToLine(const PRUnichar * aLineFragment, PRInt32 aLineFragmentLength)
{
  PRUint32 prefixwidth = (mCiteQuoteLevel>0?mCiteQuoteLevel+1:0)+mIndent;
  
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
      while(goodSpace > 0 && (width+prefixwidth > mWrapColumn))
      {
        goodSpace--;
        width -= unicharwidth(mCurrentLine[goodSpace]);
      }

      goodSpace++;
      
      PRBool oNeedMoreText;
      if (nsnull != mLineBreaker) {
        result = mLineBreaker->Prev(mCurrentLine.GetUnicode(), mCurrentLine.Length(), goodSpace,
                                    (PRUint32 *) &goodSpace, &oNeedMoreText);
        if (oNeedMoreText)
          goodSpace = -1;
        else if (nsCRT::IsAsciiSpace(mCurrentLine.CharAt(goodSpace-1))) 
          --goodSpace;    // adjust the position since line breaker returns a position next to space
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
      if(goodSpace<0) {
        // If we don't found a good place to break, accept long line and
        // try to find another place to break
        goodSpace=mWrapColumn-prefixwidth+1;
        result = NS_OK;
        if (nsnull != mLineBreaker) {
          result = mLineBreaker->Next(mCurrentLine.GetUnicode(), mCurrentLine.Length(), goodSpace,
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
      
      if(goodSpace < linelength && goodSpace > 0) {
        // Found a place to break

        // -1 (trim a char at the break position)
        // only if the line break was a space.
        if (nsCRT::IsAsciiSpace(mCurrentLine.CharAt(goodSpace)))
          mCurrentLine.Right(restOfLine, linelength-goodSpace-1);
        else
          mCurrentLine.Right(restOfLine, linelength-goodSpace);
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
      } else {
        // Nothing to do. Hopefully we get more data later
        // to use for a place to break line
        break;
      }
    }
  } else {
    // No wrapping.
  }
}

void
nsHTMLToTXTSinkStream::EndLine(PRBool softlinebreak)
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
          ' ' == mCurrentLine[--linelength])
      mCurrentLine.SetLength(linelength);
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
  } else {
    // Hard break
    if(0 == mColPos) {
      WriteQuotesAndIndent();
    }
    if(mCurrentLine.Length()>0)
      mEmptyLines=-1;

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
          (sig_delimiter != mCurrentLine))
      mCurrentLine.SetLength(--currentlinelength);

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
nsHTMLToTXTSinkStream::WriteQuotesAndIndent()
{
  // Put the mail quote "> " chars in, if appropriate:
  if (mCiteQuoteLevel>0) {
    nsAutoString quotes;
    for(int i=0; i<mCiteQuoteLevel; i++) {
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
    for (int i=0; i<indentwidth; ++i)
      spaces.AppendWithConversion(' ');
    WriteSimple(spaces);
    mColPos += indentwidth;
  }
  
  if(mInIndentString.Length()>0) {
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
nsHTMLToTXTSinkStream::Write(const nsString& aString)
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
      (mPreFormatted && !mWrapColumn))
  {
    // No intelligent wrapping. This mustn't be mixed with
    // intelligent wrapping without clearing the mCurrentLine
    // buffer before!!!

    NS_WARN_IF_FALSE(mCurrentLine.Length() == 0,
                 "Mixed wrapping data and nonwrapping data on the same line");
    if (mCurrentLine.Length() > 0)
      FlushLine();
    
    // Put the mail quote "> " chars in, if appropriate.
    // Have to put it in before every line.
    while(bol<totLen) {
      if(0 == mColPos)
        WriteQuotesAndIndent();
      
      newline = aString.FindChar('\n',PR_FALSE,bol);
      
      if(newline < 0) {
        // No new lines.
        nsAutoString stringpart;
        aString.Right(stringpart, totLen-bol);
        if(stringpart.Length()>0) {
          PRUnichar lastchar = stringpart[stringpart.Length()-1];
          if((lastchar == '\t') || (lastchar == ' ') ||
             (lastchar == '\r') ||(lastchar == '\n')) {
            mInWhitespace = PR_TRUE;
          } else {
            mInWhitespace = PR_FALSE;
          }
        }
        WriteSimple(stringpart);
        mEmptyLines=-1;
        mColPos += totLen-bol;
        bol = totLen;
      } else {
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
      } else {
        offsetIntoBuffer = aString.GetUnicode();
        offsetIntoBuffer = &offsetIntoBuffer[bol];
        AddToLine(offsetIntoBuffer, totLen-bol);
      }
      bol=totLen;
      mInWhitespace=PR_FALSE;
    } else {
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
        } else {
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
        } else {
          tempstr.AppendWithConversion(" ");
          bol = nextpos + 1;
          mInWhitespace = PR_TRUE;
        }
        WriteSimple(tempstr);
      } else {
         mInWhitespace = PR_TRUE;
         
         offsetIntoBuffer = aString.GetUnicode();
         offsetIntoBuffer = &offsetIntoBuffer[bol];
         if(mPreFormatted || (mFlags & nsIDocumentEncoder::OutputPreformatted)) {
           // Preserve the real whitespace character
           nextpos++;
           AddToLine(offsetIntoBuffer, nextpos-bol);
           bol = nextpos;
         } else {
           // Replace the whitespace with a space
           AddToLine(offsetIntoBuffer, nextpos-bol);
           AddToLine(NS_ConvertToString(" ").GetUnicode(),1);
           bol = nextpos + 1; // Let's eat the whitespace
         }
      }
    }
  } // Continue looping over the string
}

/**
  * This method gets called when the parser begins the process
  * of building the content model via the content sink.
  *
  * @update gpk02/03/99
  */     
NS_IMETHODIMP
nsHTMLToTXTSinkStream::WillBuildModel(void){
  return NS_OK;
}

/**
  * This method gets called when the parser concludes the process
  * of building the content model via the content sink.
  *
  * @param  aQualityLevel describes how well formed the doc was.
  *         0=GOOD; 1=FAIR; 2=POOR;
  * @update gpk02/03/99
  */     
NS_IMETHODIMP
nsHTMLToTXTSinkStream::DidBuildModel(PRInt32 aQualityLevel) {
  return NS_OK;
}

/**
  * This method gets called when the parser gets i/o blocked,
  * and wants to notify the sink that it may be a while before
  * more data is available.
  *
  * @update gpk02/03/99
  */     
NS_IMETHODIMP
nsHTMLToTXTSinkStream::WillInterrupt(void) {
  return NS_OK;
}

/**
  * This method gets called when the parser i/o gets unblocked,
  * and we're about to start dumping content again to the sink.
  *
  * @update gpk02/03/99
  */     
NS_IMETHODIMP
nsHTMLToTXTSinkStream::WillResume(void) {
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLToTXTSinkStream::SetParser(nsIParser* aParser) {
  return NS_OK;
}

NS_IMETHODIMP 
nsHTMLToTXTSinkStream::NotifyError(const nsParserError* aError)
{
  return NS_OK;
}

PRBool nsHTMLToTXTSinkStream::IsBlockLevel(eHTMLTags aTag)
{
  if (!mDTD)
  {
    nsCOMPtr<nsIParser> parser;
    nsresult rv = nsComponentManager::CreateInstance(kCParserCID, 
                                                     nsnull, 
                                                     kCParserIID, 
                                                     (void **)&parser);
    if (NS_FAILED(rv)) return rv;
    if (!parser) return NS_ERROR_FAILURE;

    nsAutoString htmlmime (NS_LITERAL_STRING("text/html"));
    rv = parser->CreateCompatibleDTD(&mDTD, 0, eViewNormal,
                                     &htmlmime, eDTDMode_transitional);
  /* XXX Note: We output linebreaks for blocks.
     I.e. we output linebreaks for "unknown" inline tags.
     I just hunted such a bug for <q>, same for <ins>, <col> etc..
     Better fallback to inline. /BenB */
    if (NS_FAILED(rv) || !mDTD)
      return PR_FALSE;
  }

  // Now we can get the inline status from the DTD:
  return mDTD->IsBlockElement(aTag, eHTMLTag_unknown);
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

PRBool nsHTMLToTXTSinkStream::MayWrap()
{
  return mWrapColumn &&
    ((mFlags & nsIDocumentEncoder::OutputFormatted) ||
     (mFlags & nsIDocumentEncoder::OutputWrap));

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

