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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
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

/**
 * MODULE NOTES:
 * 
 * This file declares the concrete HTMLContentSink class.
 * This class is used during the parsing process as the
 * primary interface between the parser and the content
 * model.
 */

#include "nsHTMLContentSinkStream.h"
#include "nsIParserNode.h"
#include <ctype.h>
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsIParser.h"
#include "nsICharsetAlias.h"
#include "nsIServiceManager.h"
#include "nsIEntityConverter.h"
#include "nsCRT.h"
#include "nsIDocumentEncoder.h"   // for output flags
#include "nsHTMLTokens.h"

#include "nsIOutputStream.h"
#include "nsFileStream.h"

#include "nsNetUtil.h"           // for NS_MakeAbsoluteURI

static NS_DEFINE_CID(kSaveAsCharsetCID, NS_SAVEASCHARSET_CID);
static NS_DEFINE_CID(kEntityConverterCID, NS_ENTITYCONVERTER_CID);

static NS_DEFINE_IID(kCParserIID, NS_IPARSER_IID);
static NS_DEFINE_CID(kCParserCID, NS_PARSER_CID);

const  int            gTabSize=2;

#define gMozDirty NS_LITERAL_STRING("_moz_dirty")

/**
 *  This method gets called as part of our COM-like interfaces.
 *  Its purpose is to create an interface to parser object
 *  of some type.
 *  
 *  @update   gess 4/8/98
 *  @param    nsIID  id of object to discover
 *  @param    aInstancePtr ptr to newly discovered interface
 *  @return   NS_xxx result code
 */
nsresult
nsHTMLContentSinkStream::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{                                                                        
  if (NULL == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      
  if (aIID.Equals(NS_GET_IID(nsISupports))) {
    *aInstancePtr = (nsIContentSink*)(this);
  }
  else if (aIID.Equals(NS_GET_IID(nsIContentSink))) {
    *aInstancePtr = (nsIContentSink*)(this);
  }
  else if (aIID.Equals(NS_GET_IID(nsIHTMLContentSink))) {
    *aInstancePtr = (nsIHTMLContentSink*)(this);
  }
  else if (aIID.Equals(NS_GET_IID(nsIHTMLContentSinkStream))) {
    *aInstancePtr = (nsIHTMLContentSinkStream*)(this);
  }
  else {
    *aInstancePtr=0;
    return NS_NOINTERFACE;
  }
  NS_ADDREF_THIS();
  return NS_OK;                                                        
}

NS_IMPL_ADDREF(nsHTMLContentSinkStream)
NS_IMPL_RELEASE(nsHTMLContentSinkStream)

/**
 * Construct a content sink stream.
 * @update	gess7/7/98
 * @param 
 * @return
 */
nsHTMLContentSinkStream::nsHTMLContentSinkStream()
{
  NS_INIT_REFCNT();
  mLowerCaseTags = PR_TRUE;  
  memset(mHTMLTagStack,0,sizeof(mHTMLTagStack));
  memset(mDirtyStack,0,sizeof(mDirtyStack));
  mDTD = 0;
  mHTMLStackPos = 0;
  mColPos = 0;
  mIndent = 0;
  mInBody = PR_FALSE;
  mBuffer = nsnull;
  mBufferSize = 0;
  mBufferLength = 0;
  mFlags = 0;
  mHasOpenHtmlTag=PR_FALSE;
}

NS_IMETHODIMP
nsHTMLContentSinkStream::Initialize(nsIOutputStream* aOutStream, 
                                    nsAWritableString* aOutString,
                                    const nsAReadableString* aCharsetOverride,
                                    PRUint32 aFlags)
{
  mDoFormat = (aFlags & nsIDocumentEncoder::OutputFormatted) ? PR_TRUE
                                                             : PR_FALSE;

  mBodyOnly = (aFlags & nsIDocumentEncoder::OutputBodyOnly) ? PR_TRUE
                                                            : PR_FALSE;
  mMaxColumn = 72;
  mFlags = aFlags;

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

  mStream = aOutStream;
  mString = aOutString;
  if (aCharsetOverride != nsnull)
    mCharsetOverride.Assign(*aCharsetOverride);

  mPreLevel = 0;

  return NS_OK;
}

nsHTMLContentSinkStream::~nsHTMLContentSinkStream()
{
  NS_IF_RELEASE(mDTD);

  if (mBuffer)
    delete [] mBuffer;
}

/**
 * This method tells the sink whether or not it is 
 * encoding an HTML fragment or the whole document.
 * By default, the entire document is encoded.
 *
 * @update 03/14/99 gpk
 * @param  aFlag set to true if only encoding a fragment
 */     
NS_IMETHODIMP
nsHTMLContentSinkStream::DoFragment(PRBool aFlag) 
{
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
nsHTMLContentSinkStream::BeginContext(PRInt32 aPosition) 
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
nsHTMLContentSinkStream::EndContext(PRInt32 aPosition)
{
  return NS_OK;
}

/**
 * Initialize the Unicode encoder with our current mCharsetOverride.
 */
NS_IMETHODIMP
nsHTMLContentSinkStream::InitEncoders()
{
  nsresult res;

  // Initialize an entity encoder if we're using the string interface:
  if (mString && (mFlags & nsIDocumentEncoder::OutputEncodeEntities))
    res = nsComponentManager::CreateInstance(kEntityConverterCID, NULL, 
                                             NS_GET_IID(nsIEntityConverter),
                                             getter_AddRefs(mEntityConverter));

  // Initialize a charset encoder if we're using the stream interface
  if (mStream)
  {
    nsAutoString charsetName; charsetName.Assign(mCharsetOverride);
    nsCOMPtr<nsICharsetAlias> calias(do_GetService(kCharsetAliasCID, &res));
    if (NS_SUCCEEDED(res) && calias) {
      nsAutoString temp; temp.Assign(mCharsetOverride);
      res = calias->GetPreferred(temp, charsetName);
    }
    if (NS_FAILED(res))
    {
      // failed - unknown alias , fallback to ISO-8859-1
      charsetName.AssignWithConversion("ISO-8859-1");
    }

    res = nsComponentManager::CreateInstance(kSaveAsCharsetCID, NULL, 
                                             NS_GET_IID(nsISaveAsCharset),
                                             getter_AddRefs(mCharsetEncoder));
    if (NS_FAILED(res))
      return res;
    // SaveAsCharset requires a const char* in its first argument:
    nsCAutoString charsetCString; charsetCString.AssignWithConversion(charsetName);
    // For ISO-8859-1 only, convert to entity first (always generate entites like &nbsp;).
    res = mCharsetEncoder->Init(charsetCString,
                                charsetName.EqualsIgnoreCase("ISO-8859-1") ?
                                nsISaveAsCharset::attr_htmlTextDefault :
                                nsISaveAsCharset::attr_EntityAfterCharsetConv
                                 + nsISaveAsCharset::attr_FallbackDecimalNCR,
                                nsIEntityConverter::html32);
  }

  return res;
}

void nsHTMLContentSinkStream::EnsureBufferSize(PRInt32 aNewSize)
{
  if (mBufferSize < aNewSize) {
    if(mBuffer) nsMemory::Free(mBuffer);

    mBufferSize = 2*aNewSize+1; // make this twice as large
    mBuffer = NS_STATIC_CAST(char *, nsMemory::Alloc(mBufferSize));
    if(mBuffer){ 
      mBuffer[0] = 0;
    }
  }
}

/**
 * Writes to the buffer/stream.
 * If we do both string and stream output, stream chars will override string.
 *
 * @param aString - the string to write.
 * @return The number of characters written.
 */
PRInt32 nsHTMLContentSinkStream::Write(const nsAReadableString& aString)
{
  if (mBodyOnly && !mInBody)
    return 0;

  int charsWritten = 0;

  // For the string case, we don't want to do charset conversion,
  // but we still want to encode entities.
  if (mString)
  {
    if (!mEntityConverter && (mFlags & nsIDocumentEncoder::OutputEncodeEntities))
      InitEncoders();
    if (mEntityConverter && (mFlags & nsIDocumentEncoder::OutputEncodeEntities))
    {
      nsresult res;
      PRUnichar *encodedBuffer = nsnull;
      res = mEntityConverter->ConvertToEntities(PromiseFlatString(aString).get(),
                                                nsIEntityConverter::html40Latin1,
                                                &encodedBuffer);
      if (NS_SUCCEEDED(res) && encodedBuffer)
      {
        PRInt32 len = nsCRT::strlen(encodedBuffer);
        mString->Append(encodedBuffer, len);
        nsCRT::free(encodedBuffer);
        charsWritten = len;
      }
      else {
        charsWritten = aString.Length();
        mString->Append(aString);
      }
    }
    else {
      charsWritten = aString.Length();
      mString->Append(aString);
    }
  }

  if (!mStream)
    return charsWritten;

  // Now handle the stream case:
  nsOutputStream out(mStream);

  // If an encoder is being used then convert first convert the input string
  char *encodedBuffer = nsnull;
  nsresult res;

  // Initialize the encoder if we haven't already
  if (!mCharsetEncoder)
    InitEncoders();

  if (mCharsetEncoder)
  {
    // Call the converter to convert to the target charset.
    // Convert() takes a char* output param even though it's writing unicode.
    res = mCharsetEncoder->Convert(PromiseFlatString(aString).get(), &encodedBuffer);
    if (NS_SUCCEEDED(res) && encodedBuffer)
    {
      charsWritten = nsCRT::strlen(encodedBuffer);
      out.write(encodedBuffer, charsWritten);
      nsCRT::free(encodedBuffer);
    }

    // If it didn't work, just write the unicode
    else
    {
      charsWritten = aString.Length();
      out.write(PromiseFlatString(aString).get(), charsWritten);
    }
  }

  // If we couldn't get an encoder, just write the unicode
  else
  {
    charsWritten = aString.Length();
    out.write(PromiseFlatString(aString).get(), charsWritten);
  }

  return charsWritten;
}

void nsHTMLContentSinkStream::Write(const char* aData)
{
  if (mBodyOnly && !mInBody)
    return;

  if (mStream)
  {
    nsOutputStream out(mStream);
    out << aData;
  }
  if (mString)
  {
    mString->Append(NS_ConvertASCIItoUCS2(aData));
  }
}

void nsHTMLContentSinkStream::Write(char aData)
{
  if (mBodyOnly && !mInBody)
    return;

  if (mStream)
  {
    nsOutputStream out(mStream);
    out << aData;
  }
  if (mString)
  {
    mString->Append(NS_ConvertASCIItoUCS2(aData));
  }
}

/**
 * Write the attributes of the current tag.
 * 
 * @param aNode The parser node currently in play.
 */
void nsHTMLContentSinkStream::WriteAttributes(const nsIParserNode& aNode)
{
  int theCount=aNode.GetAttributeCount();
  if(theCount) {
    int i=0;
    for(i=0;i<theCount;i++){
      nsAutoString key(aNode.GetKeyAt(i));
      
      // See if there's an attribute:
      // note that we copy here, because we're going to have to trim quotes.
      nsAutoString value (aNode.GetValueAt(i));

      // strip double quotes from beginning and end
      value.Trim("\"", PR_TRUE, PR_TRUE);

      // Filter out any attribute starting with _moz
      if (key.EqualsWithConversion("_moz", PR_TRUE, 4))
        continue;

      // 
      // Filter out special case of _moz_dirty
      // Not needed if we're filtering out all _moz* tags.
      //if (key == gMozDirty)
      //  continue;

      // 
      // Filter out special case of <br type="_moz"> or <br _moz*>,
      // used by the editor.  Bug 16988.  Yuck.
      //
      if ((eHTMLTags)aNode.GetNodeType() == eHTMLTag_br
          && ((key.EqualsWithConversion("type", PR_TRUE)
               && value.EqualsWithConversion("_moz"))))
        continue;

      if (mLowerCaseTags == PR_TRUE)
        key.ToLowerCase();
      else
        key.ToUpperCase();

      EnsureBufferSize(key.Length() + 1);
      key.ToCString(mBuffer,mBufferSize);

        // send to ouput " [KEY]="
      Write(' ');
      Write(mBuffer);
      mColPos += 1 + strlen(mBuffer) + 1;

      // Make all links absolute when converting only the selection:
      if ((mFlags & nsIDocumentEncoder::OutputAbsoluteLinks)
          && (key.EqualsWithConversion("href", PR_TRUE)
              || key.EqualsWithConversion("src", PR_TRUE)
              // Would be nice to handle OBJECT and APPLET tags,
              // but that gets more complicated since we have to
              // search the tag list for CODEBASE as well.
              // For now, just leave them relative.
            ))
      {
        if (mURI)
        {
          nsAutoString absURI;
          if (NS_SUCCEEDED(NS_MakeAbsoluteURI(absURI, value, mURI))
              && !absURI.IsEmpty())
            value = absURI;
        }
      }

      if (value.Length() > 0)
      {
        Write(char(kEqual));
        mColPos += 1 + strlen(mBuffer) + 1;

        // send to ouput "\"[VALUE]\""
        Write('\"');
        Write(value);
        Write('\"');
      }

      mColPos += 1 + strlen(mBuffer) + 1;
    }
  }
}

/**
  * This method gets called by the parser when it encounters
  * a title tag and wants to set the document title in the sink.
  *
  * @update	04/30/99 gpk
  * @param  nsString reference to new title value
  * @return PR_TRUE if successful. 
  */     
NS_IMETHODIMP
nsHTMLContentSinkStream::SetTitle(const nsString& aValue)
{
    const char* tagName = GetTagName(eHTMLTag_title);
    Write(kLessThan);
    Write(tagName);
    Write(kGreaterThan);

    Write(aValue);

    Write(kLessThan);
    Write(kForwardSlash);
    Write(tagName);
    Write(kGreaterThan);

  return NS_OK;
}

/**
  * This method is used to open the outer HTML container.
  *
  * XXX OpenHTML never gets called; AddStartTag gets called on
  * XXX the html tag from OpenContainer, from nsXIFDTD::StartTopOfStack,
  * XXX from nsXIFDTD::HandleStartToken.
  *
  * @param  nsIParserNode reference to parser node interface
  * @return PR_TRUE if successful. 
  */     
NS_IMETHODIMP
nsHTMLContentSinkStream::OpenHTML(const nsIParserNode& aNode)
{
  eHTMLTags tag = (eHTMLTags)aNode.GetNodeType();
  if (tag == eHTMLTag_html)
  {
    if(!mHasOpenHtmlTag) {
      AddStartTag(aNode);
      mHasOpenHtmlTag=PR_TRUE;
    }
    else {
      PRInt32 ac=aNode.GetAttributeCount();
      if(ac>0) {
        Write(kLessThan);
        nsAutoString tagname;
        tagname.AssignWithConversion(nsHTMLTags::GetStringValue(tag));
        Write(tagname);
        WriteAttributes(aNode);
        Write(kGreaterThan);
      }
    }
  }
  return NS_OK;
}

/**
  * All these HTML-specific methods may be called, or may not,
  * depending on whether the parser is parsing XIF or HTML.
  * So we can't depend on them; instead, we have Open/CloseContainer
  * do all the specialized work, and the html-specific Open/Close
  * methods must call the more general methods.
  *
  * Since there are so many of them, make macros:
  */

#define USE_GENERAL_OPEN_METHOD(methodname, tagtype) \
NS_IMETHODIMP nsHTMLContentSinkStream::methodname(const nsIParserNode& aNode) \
{ \
  if ((eHTMLTags)aNode.GetNodeType() == tagtype) \
    AddStartTag(aNode); \
  return NS_OK; \
}

#define USE_GENERAL_CLOSE_METHOD(methodname, tagtype) \
NS_IMETHODIMP nsHTMLContentSinkStream::methodname(const nsIParserNode& aNode) \
{ \
  if ((eHTMLTags)aNode.GetNodeType() == tagtype) \
    AddEndTag(aNode); \
  return NS_OK; \
}

USE_GENERAL_CLOSE_METHOD(CloseHTML, eHTMLTag_html)
USE_GENERAL_OPEN_METHOD(OpenHead, eHTMLTag_head)
USE_GENERAL_CLOSE_METHOD(CloseHead, eHTMLTag_head)
USE_GENERAL_OPEN_METHOD(OpenBody, eHTMLTag_body)
USE_GENERAL_CLOSE_METHOD(CloseBody, eHTMLTag_body)
USE_GENERAL_OPEN_METHOD(OpenForm, eHTMLTag_form)
USE_GENERAL_CLOSE_METHOD(CloseForm, eHTMLTag_form)
USE_GENERAL_OPEN_METHOD(OpenMap, eHTMLTag_map)
USE_GENERAL_CLOSE_METHOD(CloseMap, eHTMLTag_map)
USE_GENERAL_OPEN_METHOD(OpenFrameset, eHTMLTag_frameset)
USE_GENERAL_CLOSE_METHOD(CloseFrameset, eHTMLTag_frameset)

/**
 *
 * Check whether a node has the attribute _moz_dirty.
 * If it does, we'll prettyprint it, otherwise we adhere to the
 * surrounding text/whitespace/newline nodes provide formatting.
 */
PRBool nsHTMLContentSinkStream::IsDirty(const nsIParserNode& aNode)
{
  // Apparently there's no way to just ask for a particular attribute
  // without looping over the list.
  int theCount = aNode.GetAttributeCount();
  if (theCount)
  {
    for(int i=0; i < theCount; i++)
    {
      const nsAReadableString& key = (nsString&)aNode.GetKeyAt(i);
      if (key == gMozDirty)
        return PR_TRUE;
    }
  }
  return PR_FALSE;
}

void nsHTMLContentSinkStream::AddIndent()
{
  nsAutoString padding; padding.AssignWithConversion("  ");
  for (PRInt32 i = mIndent; --i >= 0; ) 
  {
    Write(padding);
    mColPos += 2;
  }
}

void nsHTMLContentSinkStream::AddStartTag(const nsIParserNode& aNode)
{
  eHTMLTags         tag = (eHTMLTags)aNode.GetNodeType();
  PRBool            isDirty = IsDirty(aNode);

  const nsAReadableString&   name = aNode.GetText();
  nsAutoString      tagName;

  if (tag == eHTMLTag_body)
    mInBody = PR_TRUE;

  mHTMLTagStack[mHTMLStackPos] = tag;
  mDirtyStack[mHTMLStackPos++] = isDirty;
  tagName = name;

  if (tag == eHTMLTag_doctypeDecl || tag == eHTMLTag_markupDecl)
  {
    if (!(mFlags & nsIDocumentEncoder::OutputSelectionOnly))
    {
      Write("<!"); // mdo => Markup Declaration Open.
    }
    return;
  }
  // Quoted plaintext mail/news lives in a pre tag.
  // The editor has substituted <br> tags for all the newlines in the pre,
  // in order to get clickable blank lines.
  // We can't emit these <br> tags formatted, or we'll get
  // double-spacing (one for the br, one for the line break);
  // but we can't emit them unformatted, either,
  // because then long quoted passages will make html source lines
  // too long for news servers (and some mail servers) to handle.
  // So we map all <br> tags inside <pre> to line breaks.
  // If this turns out to be a problem, we could do this only if gMozDirty.
  else if (tag == eHTMLTag_br && mPreLevel > 0)
  {
    Write(mLineBreak);
    return;
  }

  if (mLowerCaseTags == PR_TRUE)
    tagName.ToLowerCase();
  else
    tagName.ToUpperCase();

#ifdef DEBUG_prettyprint
  if (isDirty)
    printf("AddStartTag(%s): BBO=%d, BAO=%d, BBC=%d, BAC=%d\n",
           NS_LossyConvertUCS2toASCII(name).get(),
           BreakBeforeOpen(tag),
           BreakAfterOpen(tag),
           BreakBeforeClose(tag),
           BreakAfterClose(tag));
#endif

  if ((mDoFormat || isDirty) && mPreLevel == 0 && mColPos != 0
      && BreakBeforeOpen(tag))
  {
    Write(mLineBreak);
    mColPos = 0;
  }
  if ((mDoFormat || isDirty) && mPreLevel == 0 && mColPos == 0)
    AddIndent();

  EnsureBufferSize(tagName.Length() + 1);
  tagName.ToCString(mBuffer,mBufferSize);

  Write(kLessThan);
  Write(mBuffer);

  mColPos += 1 + tagName.Length();

  if ((mDoFormat || isDirty) && mPreLevel == 0 && tag == eHTMLTag_style)
  {
    Write(kGreaterThan);
    Write(mLineBreak);
    const   nsString& data = aNode.GetSkippedContent();
    PRInt32 size = data.Length();
    char*   buffer = new char[size+1];
    if(buffer){
      data.ToCString(buffer,size+1);
      Write(buffer);
      delete[] buffer;
    }
  }
  else
  {
    WriteAttributes(aNode);
    Write(kGreaterThan);
    mColPos += 1;
  }

  if (tag == eHTMLTag_pre)
    ++mPreLevel;

  if (((mDoFormat || isDirty) && mPreLevel == 0 && BreakAfterOpen(tag)))
  {
    Write(mLineBreak);
    mColPos = 0;
  }

  if (IndentChildren(tag))
    mIndent++;
}

void nsHTMLContentSinkStream::AddEndTag(const nsIParserNode& aNode)
{
  eHTMLTags         tag = (eHTMLTags)aNode.GetNodeType();
  nsAutoString      tagName;
  PRBool            isDirty = mDirtyStack[mHTMLStackPos-1];

#ifdef DEBUG_prettyprint
  if (isDirty)
    printf("AddEndTag(%s): BBO=%d, BAO=%d, BBC=%d, BAC=%d\n",
           (const char*)NS_ConvertUCS2toUTF8(aNode.GetText()),
           BreakBeforeOpen(tag),
           BreakAfterOpen(tag),
           BreakBeforeClose(tag),
           BreakAfterClose(tag));
#endif

  if (tag == eHTMLTag_unknown)
  {
    tagName.Assign(aNode.GetText());
  }
  else if (tag == eHTMLTag_pre)
  {
    --mPreLevel;
    tagName.Assign(aNode.GetText());
  }
  else if (tag == eHTMLTag_comment)
  {
    tagName.AssignWithConversion("--");
  }
  else if (tag == eHTMLTag_doctypeDecl || tag == eHTMLTag_markupDecl)
  {
    if (!(mFlags & nsIDocumentEncoder::OutputSelectionOnly))
    {
      Write(kGreaterThan);
      Write(mLineBreak);
    }
    if ( mHTMLTagStack[mHTMLStackPos-1] == eHTMLTag_doctypeDecl || mHTMLTagStack[mHTMLStackPos-1] == eHTMLTag_markupDecl)
    {
      mHTMLTagStack[--mHTMLStackPos] = eHTMLTag_unknown;
    }
    return;
  }
  else if (tag == eHTMLTag_userdefined)
  {
    // nsHTMLTags::GetStringValue doesn't work for userdefined tags
    tagName = aNode.GetText();
  }
  else
  {
    tagName.AssignWithConversion(nsHTMLTags::GetStringValue(tag));
  }
  if (mLowerCaseTags == PR_TRUE)
    tagName.ToLowerCase();
//  else
//    tagName.ToUpperCase();

  if (IndentChildren(tag))
    mIndent--;

  if ((mDoFormat || isDirty) && mPreLevel == 0 && BreakBeforeClose(tag))
  {
    if (mColPos != 0)
    {
      Write(mLineBreak);
      mColPos = 0;
    }
  }
  if ((mDoFormat || isDirty) && mPreLevel == 0 && mColPos == 0)
    AddIndent();

  EnsureBufferSize(tagName.Length() + 1);
  tagName.ToCString(mBuffer,mBufferSize);

  if (tag != eHTMLTag_comment)
  {
    Write(kLessThan);
    Write(kForwardSlash);
    mColPos += 1 + 1;
  }
  
  Write(mBuffer);
  Write(kGreaterThan);

  mColPos += strlen(mBuffer) + 1;

  if (tag == eHTMLTag_body)
    mInBody = PR_FALSE;

  if (((mDoFormat || isDirty) && mPreLevel == 0 && BreakAfterClose(tag))
      || tag == eHTMLTag_body || tag == eHTMLTag_html)
  {
    Write(mLineBreak);
    mColPos = 0;
  }
  mHTMLTagStack[--mHTMLStackPos] = eHTMLTag_unknown;
}

/**
 *  This gets called by the parser when you want to add
 *  a leaf node to the current container in the content
 *  model.
 */
nsresult
nsHTMLContentSinkStream::AddLeaf(const nsIParserNode& aNode)
{
  eHTMLTags type = (eHTMLTags)aNode.GetNodeType();
  eHTMLTags tag = eHTMLTag_unknown;
  if (mHTMLStackPos > 0)
    tag = mHTMLTagStack[mHTMLStackPos-1];
  
  if (type ==  eHTMLTag_area     ||
      type ==  eHTMLTag_base     ||
      type ==  eHTMLTag_basefont ||
      type ==  eHTMLTag_br       ||
      type ==  eHTMLTag_col      ||
      type ==  eHTMLTag_frame    ||
      type ==  eHTMLTag_hr       ||
      type ==  eHTMLTag_img      ||
      type ==  eHTMLTag_image    ||
      type ==  eHTMLTag_input    ||
      type ==  eHTMLTag_isindex  ||
      type ==  eHTMLTag_link     ||
      type ==  eHTMLTag_meta     ||
      type ==  eHTMLTag_param    ||
      type ==  eHTMLTag_sound)
  {
    AddStartTag(aNode);
    mHTMLTagStack[--mHTMLStackPos] = eHTMLTag_unknown;
  }
  else if (type == eHTMLTag_entity)
  {
    Write('&');
    const nsAReadableString& entity = aNode.GetText();
    mColPos += Write(entity) + 1;
    // Don't write the semicolon;
    // rely on the DTD to include it if one is wanted.
  }
  else if (type == eHTMLTag_text)
  {
    if ((mHTMLStackPos > 0)
        && (mHTMLTagStack[mHTMLStackPos-1] == eHTMLTag_doctypeDecl || mHTMLTagStack[mHTMLStackPos-1] == eHTMLTag_markupDecl)
        && (mFlags & nsIDocumentEncoder::OutputSelectionOnly))
      return NS_OK;

    const nsAReadableString& text = aNode.GetText();
    if (mPreLevel > 0)
    {
      Write(text);
      mColPos += text.Length();
    }
    else if (!mDoFormat)
    {
      if (HasLongLines(text))
      {
        WriteWrapped(text);
      }
      else
      {
        Write(text);
        mColPos += text.Length();
      }
    }
    else
    {
      WriteWrapped(text);
    }
  }
  else if (type == eHTMLTag_whitespace)
  {
    if (!mDoFormat || mPreLevel > 0)
    {
      const nsAReadableString& text = aNode.GetText();
      Write(text);
      mColPos += text.Length();
    }
  }
  else if (type == eHTMLTag_newline)
  {
    if (!mDoFormat || mPreLevel > 0)
    {
      Write(mLineBreak);
      mColPos = 0;
    }
  }

  return NS_OK;
}

// See if the string has any lines longer than longLineLen:
// if so, we presume formatting is wonky (e.g. the node has been edited)
// and we'd better rewrap the whole text node.
PRBool nsHTMLContentSinkStream::HasLongLines(const nsAReadableString& text)
{
  const PRUint32 longLineLen = 128;
  PRUint32 start=0;
  PRUint32 theLen=text.Length();
  for (start = 0; start < theLen; )
  {
    PRInt32 eol = text.FindChar('\n', start);
    if (eol < 0) eol = text.Length();
    if ((PRUint32)(eol - start) > longLineLen)
      return PR_TRUE;
    start = eol+1;
  }
  return PR_FALSE;
}

void nsHTMLContentSinkStream::WriteWrapped(const nsAReadableString& text)
{
      // 1. Determine the length of the input string
  PRInt32 length = text.Length();

  // 2. If the offset plus the length of the text is smaller
  // than the max then just add it 
  if (mColPos + length < mMaxColumn)
  {
    Write(text);
    mColPos += text.Length();
  }
  else
  {
    nsAutoString  str(text);
    PRBool    done = PR_FALSE;
    PRInt32   indx = 0;
    PRInt32   offset = mColPos;

    while (!done)
    {        
      // find the next break
      PRInt32 start = mMaxColumn-offset;
      if (start < 0)
        start = 0;
          
      indx = str.FindChar(' ', PR_FALSE, start);

      // if there is no break than just add it
      if (indx == kNotFound)
      {
        Write(str);
        mColPos += str.Length();
        done = PR_TRUE;
      }
      else
      {
        // make first equal to the str from the 
        // beginning to the index
        nsString  first = str;

        first.Truncate(indx);
  
        Write(first);
        Write(mLineBreak);
        mColPos = 0;
  
        // cut the string from the beginning to the index
        str.Cut(0,indx);
        offset = 0;
      }
    }
  }
}

/**
 *  This gets called by the parser when you want to add
 *  a PI node to the current container in the content
 *  model.
 *  
 *  @updated gess 3/25/98
 *  @param   
 *  @return  
 */
NS_IMETHODIMP
nsHTMLContentSinkStream::AddProcessingInstruction(const nsIParserNode& aNode){

#ifdef VERBOSE_DEBUG
  DebugDump("<",aNode.GetText(),(mNodeStackPos)*2);
#endif

  return NS_OK;
}

/**
 *  This gets called by the parser when it encounters
 *  a DOCTYPE declaration in the HTML document.
 */

NS_IMETHODIMP
nsHTMLContentSinkStream::AddDocTypeDecl(const nsIParserNode& aNode, PRInt32 aMode)
{
#ifdef VERBOSE_DEBUG
  DebugDump("<",aNode.GetText(),(mNodeStackPos)*2);
#endif
   
 // Write("<!");

  return NS_OK;
}

/**
 *  This gets called by the parser when you want to add
 *  a comment node to the current container in the content
 *  model.
 */
NS_IMETHODIMP
nsHTMLContentSinkStream::AddComment(const nsIParserNode& aNode){

#ifdef VERBOSE_DEBUG
  DebugDump("<",aNode.GetText(),(mNodeStackPos)*2);
#endif

  Write(aNode.GetText());

  return NS_OK;
}

    
/**
  * This method is used to a general container. 
  * This includes: OL,UL,DIR,SPAN,TABLE,H[1..6],etc.
  *
  * @param  nsIParserNode reference to parser node interface
  * @return PR_TRUE if successful. 
  */     
NS_IMETHODIMP
nsHTMLContentSinkStream::OpenContainer(const nsIParserNode& aNode)
{
  // Look for XIF document_info tag.  This has a type of userdefined;
  // GetText() is slow, so don't call it unless we see the right node type.
  eHTMLTags tag = (eHTMLTags)aNode.GetNodeType();
  if (tag == eHTMLTag_userdefined)
  {
    nsAutoString name; name.Assign(aNode.GetText());
    if (name.EqualsWithConversion("document_info"))
    {
      PRInt32 count=aNode.GetAttributeCount();
      for(PRInt32 i=0;i<count;i++)
      {
        const nsAReadableString& key=aNode.GetKeyAt(i);

        if (key.Equals(NS_LITERAL_STRING("charset")))
        {
          const nsString& value=aNode.GetValueAt(i);
          if (mCharsetOverride.IsEmpty())
            mCharsetOverride.Assign(value);
          InitEncoders();
        }
        else if (key.Equals(NS_LITERAL_STRING("uri")))
        {
          nsAutoString uristring; uristring.Assign(aNode.GetValueAt(i));

          // strip double quotes from beginning and end
          uristring.Trim("\"", PR_TRUE, PR_TRUE);

          // And make it into a URI:
          if (!uristring.IsEmpty())
            NS_NewURI(getter_AddRefs(mURI), uristring);
        }
      }
      return NS_OK;
    }
    // else if not document_info, fall through to the normal AddStartTag call
  }

  AddStartTag(aNode);

  return NS_OK;
}


/**
  * This method is used to close a generic container.
  *
  * @update	04/30/99 gpk
  * @param  nsIParserNode reference to parser node interface
  * @return PR_TRUE if successful. 
  */     
NS_IMETHODIMP
nsHTMLContentSinkStream::CloseContainer(const nsIParserNode& aNode){
  AddEndTag(aNode);
  return NS_OK;
}

/**
  * Find out from the parser whether a node is a block node.
  */
PRBool nsHTMLContentSinkStream::IsBlockLevel(eHTMLTags aTag) 
{
  if (!mDTD)
  {
    nsresult rv;
    nsCOMPtr<nsIParser> parser( do_CreateInstance(kCParserCID, &rv) );
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

/**
  * **** Pretty Printing Methods ******
  *
  */

/**
  * Desired line break state before the open tag.
  */
PRBool nsHTMLContentSinkStream::BreakBeforeOpen(eHTMLTags aTag)
{
 PRBool  result = PR_FALSE;
  switch (aTag)
  {
    case  eHTMLTag_html:
      result = PR_FALSE;
      break;
    case  eHTMLTag_title:
      result = PR_TRUE;
      break;
    default:
      result = IsBlockLevel(aTag);
  }
  return result;
}

/**
  * Desired line break state after the open tag.
  */
PRBool nsHTMLContentSinkStream::BreakAfterOpen(eHTMLTags aTag)
{
  PRBool  result = PR_FALSE;
  switch (aTag)
  {
    case eHTMLTag_html:
    case eHTMLTag_head:
    case eHTMLTag_body:
    case eHTMLTag_ul:
    case eHTMLTag_ol:
    case eHTMLTag_table:
    case eHTMLTag_tbody:
    case eHTMLTag_style:
    case eHTMLTag_tr:
    case eHTMLTag_br:
      result = PR_TRUE;
      break;

    default:
      break;
  }
  return result;
}

/**
  * Desired line break state before the close tag.
  */
PRBool nsHTMLContentSinkStream::BreakBeforeClose(eHTMLTags aTag)
{
  PRBool  result = PR_FALSE;

  switch (aTag)
  {
    case eHTMLTag_html:
    case eHTMLTag_head:
    case eHTMLTag_body:
    case eHTMLTag_ul:
    case eHTMLTag_ol:
    case eHTMLTag_table:
    case eHTMLTag_tbody:
    case eHTMLTag_style:
      result = PR_TRUE;
      break;

    default:
      break;
  }
  return result;
}

/**
  * Desired line break state after the close tag.
  */
PRBool nsHTMLContentSinkStream::BreakAfterClose(eHTMLTags aTag)
{
  PRBool  result = PR_FALSE;

  switch (aTag)
  {
    case eHTMLTag_html:
    case eHTMLTag_head:
    case eHTMLTag_body:
    case eHTMLTag_tr:
    case eHTMLTag_th:
    case eHTMLTag_td:
    case eHTMLTag_pre:
    case eHTMLTag_title:
    case eHTMLTag_meta:
    case eHTMLTag_li:
    case eHTMLTag_dt:
    case eHTMLTag_dd:
    case eHTMLTag_blockquote:
    case eHTMLTag_p:
    case eHTMLTag_div:
      result = PR_TRUE;
    break;

    default:
      result = IsBlockLevel(aTag);
  }
  return result;
}

/**
  * Indent/outdent when the open/close tags are encountered.
  * This implies that BreakAfterOpen() and BreakBeforeClose()
  * are true no matter what those methods return.
  */
PRBool nsHTMLContentSinkStream::IndentChildren(eHTMLTags aTag)
{
  PRBool result = PR_FALSE;

  switch (aTag)
  {
    case eHTMLTag_head:
    case eHTMLTag_table:
    case eHTMLTag_tr:
    case eHTMLTag_ul:
    case eHTMLTag_ol:
    case eHTMLTag_tbody:
    case eHTMLTag_form:
    case eHTMLTag_frameset:
    case eHTMLTag_li:
    case eHTMLTag_dt:
    case eHTMLTag_dd:
    case eHTMLTag_blockquote:
      result = PR_TRUE;      
      break;

    default:
      result = PR_FALSE;
      break;
  }
  return result;  
}

/**
  * This method gets called when the parser begins the process
  * of building the content model via the content sink.
  *
  * @update 5/7/98 gess
  */     
NS_IMETHODIMP
nsHTMLContentSinkStream::WillBuildModel(void)
{
  mTabLevel=-1;

  return NS_OK;
}


/**
  * This method gets called when the parser concludes the process
  * of building the content model via the content sink.
  *
  * @param  aQualityLevel describes how well formed the doc was.
  *         0=GOOD; 1=FAIR; 2=POOR;
  * @update 5/7/98 gess
  */     
NS_IMETHODIMP
nsHTMLContentSinkStream::DidBuildModel(PRInt32 aQualityLevel) {
  return NS_OK;
}


/**
  * This method gets called when the parser gets i/o blocked,
  * and wants to notify the sink that it may be a while before
  * more data is available.
  *
  * @update 5/7/98 gess
  */     
NS_IMETHODIMP
nsHTMLContentSinkStream::WillInterrupt(void) {
  return NS_OK;
}


/**
  * This method gets called when the parser i/o gets unblocked,
  * and we're about to start dumping content again to the sink.
  *
  * @update 5/7/98 gess
  */     
NS_IMETHODIMP
nsHTMLContentSinkStream::WillResume(void) {
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLContentSinkStream::SetParser(nsIParser* aParser) {
  return NS_OK;
}

NS_IMETHODIMP 
nsHTMLContentSinkStream::NotifyError(const nsParserError* aError)
{
  return NS_OK;
}

