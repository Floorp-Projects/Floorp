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
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

/**
 * MODULE NOTES:
 * @update  gess 4/1/98
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
#include "nsIParser.h"
#include "nsICharsetAlias.h"
#include "nsIServiceManager.h"
#include "nsIEntityConverter.h"
#include "nsCRT.h"
#include "nsIDocumentEncoder.h"   // for output flags
#include "nshtmlpars.h"

#include "nsIOutputStream.h"
#include "nsFileStream.h"

#include "nsNetUtil.h"           // for NS_MakeAbsoluteURI

static NS_DEFINE_CID(kSaveAsCharsetCID, NS_SAVEASCHARSET_CID);
static NS_DEFINE_CID(kEntityConverterCID, NS_ENTITYCONVERTER_CID);

static char*          gHeaderComment = "<!-- This page was created by the Gecko output system. -->";
static char*          gDocTypeHeader = "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 3.2//EN\">";
const  int            gTabSize=2;

static const nsString gMozDirty ("_moz_dirty");

static PRBool IsInline(eHTMLTags aTag);
static PRBool IsBlockLevel(eHTMLTags aTag);
static PRInt32 BreakBeforeOpen(eHTMLTags aTag);
static PRInt32 BreakAfterOpen(eHTMLTags aTag);
static PRInt32 BreakBeforeClose(eHTMLTags aTag);
static PRInt32 BreakAfterClose(eHTMLTags aTag);
static PRBool IndentChildren(eHTMLTags aTag);
static PRBool PreformattedChildren(eHTMLTags aTag);


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
  mHTMLStackPos = 0;
  mColPos = 0;
  mIndent = 0;
  mInBody = PR_FALSE;
  mBuffer = nsnull;
  mBufferSize = 0;
  mBufferLength = 0;
  mFlags = 0;
}

NS_IMETHODIMP
nsHTMLContentSinkStream::Initialize(nsIOutputStream* aOutStream, 
                                    nsString* aOutString,
                                    const nsString* aCharsetOverride,
                                    PRUint32 aFlags)
{
  mDoFormat = (aFlags & nsIDocumentEncoder::OutputFormatted) ? PR_TRUE
                                                             : PR_FALSE;
      // I don't think anyone calls us with OutputFormatted
#ifdef DEBUG_akkana
  NS_ASSERTION(!mDoFormat, "nsHTMLContentSinkStream called with OutputFormatted!\n");
#endif

  mBodyOnly = (aFlags & nsIDocumentEncoder::OutputBodyOnly) ? PR_TRUE
                                                            : PR_FALSE;
  mDoHeader = (!mBodyOnly) && (mDoFormat) &&
               ((aFlags & nsIDocumentEncoder::OutputNoDoctype) ? PR_FALSE
                                                               : PR_TRUE);
  mMaxColumn = 72;
  mFlags = aFlags;

  mStream = aOutStream;
  mString = aOutString;
  if (aCharsetOverride != nsnull)
    mCharsetOverride.Assign(*aCharsetOverride);

  return NS_OK;
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
    nsAutoString charsetName = mCharsetOverride;
    NS_WITH_SERVICE(nsICharsetAlias, calias, kCharsetAliasCID, &res);
    if (NS_SUCCEEDED(res) && calias)
      res = calias->GetPreferred(mCharsetOverride, charsetName);
    if (NS_FAILED(res))
    {
      // failed - unknown alias , fallback to ISO-8859-1
      charsetName = "ISO-8859-1";
    }

    res = nsComponentManager::CreateInstance(kSaveAsCharsetCID, NULL, 
                                             NS_GET_IID(nsISaveAsCharset),
                                             getter_AddRefs(mCharsetEncoder));
    if (NS_FAILED(res))
      return res;
    // SaveAsCharset requires a const char* in its first argument:
    nsCAutoString charsetCString (charsetName);
    // For ISO-8859-1 only, convert to entity first (always generate entites like &nbsp;).
    res = mCharsetEncoder->Init(charsetCString,
                                charsetName.EqualsIgnoreCase("ISO-8859-1") ?
                                nsISaveAsCharset::attr_htmlTextDefault :
                                nsISaveAsCharset::attr_EntityAfterCharsetConv
                                 + nsISaveAsCharset::attr_FallbackDecimalNCR,
                                nsIEntityConverter::html40);
  }

  return res;
}

//
// Write(nsString) returns the number of characters written.
// If we do both string and stream output, stream chars will override string.
//
PRInt32 nsHTMLContentSinkStream::Write(const nsString& aString)
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
      res = mEntityConverter->ConvertToEntities(aString.GetUnicode(),
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
    res = mCharsetEncoder->Convert(aString.GetUnicode(), &encodedBuffer);
    if (NS_SUCCEEDED(res) && encodedBuffer)
    {
      charsWritten = nsCRT::strlen(encodedBuffer);
      out.write(encodedBuffer, charsWritten);
      nsCRT::free(encodedBuffer);
    }

    // If it didn't work, just write the unicode
    else
    {
      const PRUnichar* unicode = aString.GetUnicode();
      charsWritten = aString.Length();
      out.write(unicode, charsWritten);
    }
  }

  // If we couldn't get an encoder, just write the unicode
  else
  {
    const PRUnichar* unicode = aString.GetUnicode();
    charsWritten = aString.Length();
    out.write(unicode, charsWritten);
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
    mString->Append(aData);
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
    mString->Append(aData);
  }
}


/**
 * 
 * @update	04/30/99 gpk
 * @param 
 * @return
 */
nsHTMLContentSinkStream::~nsHTMLContentSinkStream()
{
    if (mBuffer)
      nsAllocator::Free(mBuffer);
}



/**
 * 
 * @update	gess7/7/98
 * @param 
 * @return
 */
void nsHTMLContentSinkStream::WriteAttributes(const nsIParserNode& aNode)
{
  int theCount=aNode.GetAttributeCount();
  if(theCount) {
    int i=0;
    for(i=0;i<theCount;i++){
      nsString& key = (nsString&)aNode.GetKeyAt(i);
      
      // See if there's an attribute:
      // note that we copy here, because we're going to have to trim quotes.
      nsString value (aNode.GetValueAt(i));

      // strip double quotes from beginning and end
      value.Trim("\"", PR_TRUE, PR_TRUE);

      // 
      // Filter out special case of <br type="_moz"> or <br _moz*>,
      // used by the editor.  Bug 16988.  Yuck.
      //
      if ((eHTMLTags)aNode.GetNodeType() == eHTMLTag_br
          && ((key.Equals("type", PR_TRUE) && value.Equals("_moz"))
              || key.Equals("_moz", PR_TRUE, 4)))
        continue;

      // 
      // Filter out special case of _moz_dirty
      //
      if (key.Equals(gMozDirty))
        continue;

      if (mLowerCaseTags == PR_TRUE)
        key.ToLowerCase();
      else
        key.ToUpperCase();

      EnsureBufferSize(key.Length());
      key.ToCString(mBuffer,mBufferSize);

        // send to ouput " [KEY]="
      Write(' ');
      Write(mBuffer);
      mColPos += 1 + strlen(mBuffer) + 1;

      // Make all links absolute when converting only the selection:
      if ((mFlags & nsIDocumentEncoder::OutputAbsoluteLinks)
          && (key.Equals("href", PR_TRUE) || key.Equals("src", PR_TRUE)
              // Would be nice to handle OBJECT and APPLET tags,
              // but that gets more complicated since we have to
              // search the tag list for CODEBASE as well.
              // For now, just leave them relative.
            ))
      {
        if (mURI)
        {
          nsAutoString absURI;
          if (NS_SUCCEEDED(NS_MakeAbsoluteURI(value, mURI, absURI))
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
nsHTMLContentSinkStream::SetTitle(const nsString& aValue){
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



// XXX OpenHTML never gets called; AddStartTag gets called on
// XXX the html tag from OpenContainer, from nsXIFDTD::StartTopOfStack,
// XXX from nsXIFDTD::HandleStartToken.
/**
  * This method is used to open the outer HTML container.
  *
  * @update	04/30/99 gpk
  * @param  nsIParserNode reference to parser node interface
  * @return PR_TRUE if successful. 
  */     
NS_IMETHODIMP
nsHTMLContentSinkStream::OpenHTML(const nsIParserNode& aNode)
{
  eHTMLTags tag = (eHTMLTags)aNode.GetNodeType();
  if (tag == eHTMLTag_html)
  {
    // See bug 20246: the html tag doesn't have "html" in its text,
    // so AddStartTag will do the wrong thing
    Write(kLessThan);
    nsAutoCString tagname (nsHTMLTags::GetStringValue(tag));
    Write(tagname);
    Write(kGreaterThan);
  }
  return NS_OK;
}


/**
  * This method is used to close the outer HTML container.
  *
  * @update	04/30/99 gpk
  * @param  nsIParserNode reference to parser node interface
  * @return PR_TRUE if successful. 
  */     
NS_IMETHODIMP
nsHTMLContentSinkStream::CloseHTML(const nsIParserNode& aNode){
  eHTMLTags tag = (eHTMLTags)aNode.GetNodeType();
  if (tag == eHTMLTag_html)
    AddEndTag(aNode);
  return NS_OK;
}


/**
  * This method is used to open the only HEAD container.
  *
  * @update	04/30/99 gpk
  * @param  nsIParserNode reference to parser node interface
  * @return PR_TRUE if successful. 
  */     
NS_IMETHODIMP
nsHTMLContentSinkStream::OpenHead(const nsIParserNode& aNode)
{
  eHTMLTags tag = (eHTMLTags)aNode.GetNodeType();
  if (tag == eHTMLTag_head)
  AddStartTag(aNode);

  return NS_OK;
}


/**
  * This method is used to close the only HEAD container.
  *
  * @update	04/30/99 gpk
  * @param  nsIParserNode reference to parser node interface
  * @return PR_TRUE if successful. 
  */     
NS_IMETHODIMP
nsHTMLContentSinkStream::CloseHead(const nsIParserNode& aNode){
  eHTMLTags tag = (eHTMLTags)aNode.GetNodeType();
  if (tag == eHTMLTag_head)
    AddEndTag(aNode);
  return NS_OK;
}


/**
  * This method is used to open the main BODY container.
  *
  * @update	04/30/99 gpk
  * @param  nsIParserNode reference to parser node interface
  * @return PR_TRUE if successful. 
  */     
NS_IMETHODIMP
nsHTMLContentSinkStream::OpenBody(const nsIParserNode& aNode){
  eHTMLTags tag = (eHTMLTags)aNode.GetNodeType();
  if (tag == eHTMLTag_body)
    AddStartTag(aNode);
  return NS_OK;
}


/**
  * This method is used to close the main BODY container.
  *
  * @update	04/30/99 gpk
  * @param  nsIParserNode reference to parser node interface
  * @return PR_TRUE if successful. 
  */     
NS_IMETHODIMP
nsHTMLContentSinkStream::CloseBody(const nsIParserNode& aNode){
  eHTMLTags tag = (eHTMLTags)aNode.GetNodeType();
  if (tag == eHTMLTag_body)
    AddEndTag(aNode);
  return NS_OK;
}


/**
  * This method is used to open a new FORM container.
  *
  * @update 07/12/98 gpk
  * @param  nsIParserNode reference to parser node interface
  * @return PR_TRUE if successful. 
  */     
NS_IMETHODIMP
nsHTMLContentSinkStream::OpenForm(const nsIParserNode& aNode){
  eHTMLTags tag = (eHTMLTags)aNode.GetNodeType();
  if (tag == eHTMLTag_form)
    AddStartTag(aNode);
  return NS_OK;
}


/**
  * This method is used to close the outer FORM container.
  *
  * @update 07/12/98 gpk
  * @param  nsIParserNode reference to parser node interface
  * @return PR_TRUE if successful. 
  */     
NS_IMETHODIMP
nsHTMLContentSinkStream::CloseForm(const nsIParserNode& aNode){
  eHTMLTags tag = (eHTMLTags)aNode.GetNodeType();
  if (tag == eHTMLTag_form)
    AddEndTag(aNode);
  return NS_OK;
}

/**
  * This method is used to open a new FORM container.
  *
  * @update 07/12/98 gpk
  * @param  nsIParserNode reference to parser node interface
  * @return PR_TRUE if successful. 
  */     
NS_IMETHODIMP
nsHTMLContentSinkStream::OpenMap(const nsIParserNode& aNode){
  eHTMLTags tag = (eHTMLTags)aNode.GetNodeType();
  if (tag == eHTMLTag_map)
    AddStartTag(aNode);
  return NS_OK;
}


/**
  * This method is used to close the outer FORM container.
  *
  * @update 07/12/98 gpk
  * @param  nsIParserNode reference to parser node interface
  * @return PR_TRUE if successful. 
  */     
NS_IMETHODIMP
nsHTMLContentSinkStream::CloseMap(const nsIParserNode& aNode){
  eHTMLTags tag = (eHTMLTags)aNode.GetNodeType();
  if (tag == eHTMLTag_map)
    AddEndTag(aNode);
return NS_OK;
}

    
/**
  * This method is used to open the FRAMESET container.
  *
  * @update 07/12/98 gpk
  * @param  nsIParserNode reference to parser node interface
  * @return PR_TRUE if successful. 
  */     
NS_IMETHODIMP
nsHTMLContentSinkStream::OpenFrameset(const nsIParserNode& aNode){
  eHTMLTags tag = (eHTMLTags)aNode.GetNodeType();
  if (tag == eHTMLTag_frameset)
    AddStartTag(aNode);
  return NS_OK;
}


/**
  * This method is used to close the FRAMESET container.
  *
  * @update 07/12/98 gpk
  * @param  nsIParserNode reference to parser node interface
  * @return PR_TRUE if successful. 
  */     
NS_IMETHODIMP
nsHTMLContentSinkStream::CloseFrameset(const nsIParserNode& aNode){
  eHTMLTags tag = (eHTMLTags)aNode.GetNodeType();
  if (tag == eHTMLTag_frameset)
    AddEndTag(aNode);
  return NS_OK;
}


void nsHTMLContentSinkStream::AddIndent()
{
  nsString padding("  ");
  for (PRInt32 i = mIndent; --i >= 0; ) 
  {
    Write(padding);
    mColPos += 2;
  }
}

void nsHTMLContentSinkStream::EnsureBufferSize(PRInt32 aNewSize) {
  if (mBufferSize < aNewSize) {
    if(mBuffer) delete [] mBuffer;

    mBufferSize = 2*aNewSize+1; // make this twice as large
    mBuffer = new char[mBufferSize];
    if(mBuffer){ 
      mBuffer[0] = 0;
    }
  }
}

//
// Check whether a node has the attribute _moz_dirty.
// If it does, we'll prettyprint it, otherwise we adhere to the
// surrounding text/whitespace/newline nodes provide formatting.
//
PRBool nsHTMLContentSinkStream::IsDirty(const nsIParserNode& aNode)
{
  // Apparently there's no way to just ask for a particlar attribute
  // without looping over the list.
  int theCount = aNode.GetAttributeCount();
  if (theCount)
  {
    for(int i=0; i < theCount; i++)
    {
      nsString& key = (nsString&)aNode.GetKeyAt(i);
      if (key.Equals(gMozDirty))
        return PR_TRUE;
    }
  }
  return PR_FALSE;
}

void nsHTMLContentSinkStream::AddStartTag(const nsIParserNode& aNode)
{
  eHTMLTags         tag = (eHTMLTags)aNode.GetNodeType();
  PRBool            isDirty = IsDirty(aNode);

  if(tag==eHTMLTag_markupDecl) { 
    Write("<!"); // mdo => Markup Declaration Open.
    return;
  }

  const nsString&   name = aNode.GetText();
  nsString          tagName;

  if (tag == eHTMLTag_body)
    mInBody = PR_TRUE;

  mHTMLTagStack[mHTMLStackPos] = tag;
  mDirtyStack[mHTMLStackPos++] = isDirty;
  tagName = name;

  if (mLowerCaseTags == PR_TRUE)
    tagName.ToLowerCase();
  else
    tagName.ToUpperCase();

#ifdef DEBUG_prettyprint
  if (isDirty)
    printf("AddStartTag(%s): BBO=%d, BAO=%d, BBC=%d, BAC=%d\n",
           name.ToNewCString(),
           BreakBeforeOpen(tag),
           BreakAfterOpen(tag),
           BreakBeforeClose(tag),
           BreakAfterClose(tag));
#endif

  if ((mDoFormat || isDirty) && mColPos != 0 && BreakBeforeOpen(tag))
  {
    Write(NS_LINEBREAK);
    mColPos = 0;
  }
  if ((mDoFormat || isDirty) && mColPos == 0)
    AddIndent();

  EnsureBufferSize(tagName.Length());
  tagName.ToCString(mBuffer,mBufferSize);

  Write(kLessThan);
  Write(mBuffer);

  mColPos += 1 + tagName.Length();

  if ((mDoFormat || isDirty) && tag == eHTMLTag_style)
  {
    Write(kGreaterThan);
    Write(NS_LINEBREAK);
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

  if (((mDoFormat || isDirty) && BreakAfterOpen(tag))
      || (tag == eHTMLTag_pre))
  {
    Write(NS_LINEBREAK);
    mColPos = 0;
  }

  if (IndentChildren(tag))
    mIndent++;

  if (tag == eHTMLTag_head)
  {
    if(mDoHeader)
    {
      Write(gHeaderComment);
      Write(NS_LINEBREAK);
      Write(gDocTypeHeader);
      Write(NS_LINEBREAK);
    }
  }
}


void nsHTMLContentSinkStream::AddEndTag(const nsIParserNode& aNode)
{
  eHTMLTags         tag = (eHTMLTags)aNode.GetNodeType();
  nsAutoString      tagName;
  PRBool            isDirty = mDirtyStack[mHTMLStackPos-1];

#ifdef DEBUG_prettyprint
  if (isDirty)
    printf("AddEndTag(%s): BBO=%d, BAO=%d, BBC=%d, BAC=%d\n",
           aNode.GetText().ToNewCString(),
           BreakBeforeOpen(tag),
           BreakAfterOpen(tag),
           BreakBeforeClose(tag),
           BreakAfterClose(tag));
#endif

  if (tag == eHTMLTag_unknown)
  {
    tagName = aNode.GetText();
  }
  else if (tag == eHTMLTag_comment)
  {
    tagName = "--";
  }
  else if(tag == eHTMLTag_markupDecl) 
  {
    // mod => Markup Declaration Open, i.e., "<!"
    Write(kGreaterThan);
    Write(NS_LINEBREAK);
    return;
  }
  else
  {
    tagName = nsHTMLTags::GetStringValue(tag);
  }
  if (mLowerCaseTags == PR_TRUE)
    tagName.ToLowerCase();
//  else
//    tagName.ToUpperCase();

  if (IndentChildren(tag))
    mIndent--;

  if ((mDoFormat || isDirty) && BreakBeforeClose(tag))
  {
    if (mColPos != 0)
    {
      Write(NS_LINEBREAK);
      mColPos = 0;
    }
  }
  if ((mDoFormat || isDirty) && mColPos == 0)
    AddIndent();

  EnsureBufferSize(tagName.Length());
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

  if (((mDoFormat || isDirty) && BreakAfterClose(tag))
      || tag == eHTMLTag_body || tag == eHTMLTag_html)
  {
    Write(NS_LINEBREAK);
    mColPos = 0;
  }
  mHTMLTagStack[--mHTMLStackPos] = eHTMLTag_unknown;
}



/**
 *  This gets called by the parser when you want to add
 *  a leaf node to the current container in the content
 *  model.
 *  
 *  @updated gpk 06/18/98
 *  @param   
 *  @return  
 */
nsresult
nsHTMLContentSinkStream::AddLeaf(const nsIParserNode& aNode){
  eHTMLTags type = (eHTMLTags)aNode.GetNodeType();
  eHTMLTags tag = eHTMLTag_unknown;
  if (mHTMLStackPos > 0)
    tag = mHTMLTagStack[mHTMLStackPos-1];
  
  PRBool preformatted = PR_FALSE;

  for (PRInt32 i = mHTMLStackPos-1; i >= 0; i--)
  {
    preformatted |= PreformattedChildren(mHTMLTagStack[i]);
    if (preformatted)
      break;
  }

  if (type == eHTMLTag_br ||
      type == eHTMLTag_hr ||
      type == eHTMLTag_meta || 
      type == eHTMLTag_style)
  {
    AddStartTag(aNode);
    mHTMLTagStack[--mHTMLStackPos] = eHTMLTag_unknown;
  }
  else if (type == eHTMLTag_entity)
  {
    Write('&');
    const nsString& entity = aNode.GetText();
    mColPos += Write(entity) + 1;
    // Don't write the semicolon;
    // rely on the DTD to include it if one is wanted.
  }
  else if (type == eHTMLTag_text)
  {
    const nsString& text = aNode.GetText();
    if (preformatted)
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
    if (!mDoFormat || preformatted)
    {
      const nsString& text = aNode.GetText();
      Write(text);
      mColPos += text.Length();
    }
  }
  else if (type == eHTMLTag_newline)
  {
    if (!mDoFormat || preformatted)
    {
      Write(NS_LINEBREAK);
      mColPos = 0;
    }
  }

  return NS_OK;
}

// See if the string has any lines longer than longLineLen:
// if so, we presume formatting is wonky (e.g. the node has been edited)
// and we'd better rewrap the whole text node.
PRBool nsHTMLContentSinkStream::HasLongLines(const nsString& text)
{
  const PRInt32 longLineLen = 128;
  nsString str = text;
  PRUint32 start=0;
  PRUint32 theLen=text.Length();
  for (start = 0; start < theLen; )
  {
    PRInt32 eol = text.FindChar('\n', PR_FALSE, start);
    if (eol < 0) eol = text.Length();
    if ((PRInt32)(eol - start) > longLineLen)
      return PR_TRUE;
    start = eol+1;
  }
  return PR_FALSE;
}

void nsHTMLContentSinkStream::WriteWrapped(const nsString& text)
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
    nsString  str = text;
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
        Write(NS_LINEBREAK);
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
   
  Write("<!");

  return NS_OK;
}

/**
 *  This gets called by the parser when you want to add
 *  a comment node to the current container in the content
 *  model.
 *  
 *  @updated gess 3/25/98
 *  @param   
 *  @return  
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
  * @update 07/12/98 gpk
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
    nsAutoString name = aNode.GetText();
    if (name.Equals("document_info"))
    {
      PRInt32 count=aNode.GetAttributeCount();
      for(PRInt32 i=0;i<count;i++)
      {
        const nsString& key=aNode.GetKeyAt(i);

        if (key.Equals("charset"))
        {
          const nsString& value=aNode.GetValueAt(i);
          if (mCharsetOverride.IsEmpty())
            mCharsetOverride.Assign(value);
          InitEncoders();
        }
        else if (key.Equals("uri"))
        {
          nsAutoString uristring (aNode.GetValueAt(i));

          // strip double quotes from beginning and end
          uristring.Trim("\"", PR_TRUE, PR_TRUE);

          // And make it into a URI:
          if (!uristring.IsEmpty())
            NS_NewURI(getter_AddRefs(mURI), uristring);
        }
      }
    }
  }
  else
  {
    AddStartTag(aNode);
  }
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

/////////////////////////////////////////////////////////////
////  Useful static methods
/////////////////////////////////////////////////////////////

static PRBool IsInline(eHTMLTags aTag)
{
  PRBool  result = PR_FALSE;

  switch (aTag)
  {
    case  eHTMLTag_a:
    case  eHTMLTag_address:
    case  eHTMLTag_big:
    case  eHTMLTag_blink:
    case  eHTMLTag_b:
    case  eHTMLTag_br:
    case  eHTMLTag_cite:
    case  eHTMLTag_code:
    case  eHTMLTag_dfn:
    case  eHTMLTag_em:
    case  eHTMLTag_font:
    case  eHTMLTag_img:
    case  eHTMLTag_i:
    case  eHTMLTag_kbd:
    case  eHTMLTag_keygen:
    case  eHTMLTag_nobr:
    case  eHTMLTag_samp:
    case  eHTMLTag_small:
    case  eHTMLTag_spacer:
    case  eHTMLTag_span:      
    case  eHTMLTag_strike:
    case  eHTMLTag_strong:
    case  eHTMLTag_sub:
    case  eHTMLTag_sup:
    case  eHTMLTag_textarea:
    case  eHTMLTag_tt:
    case  eHTMLTag_var:
    case  eHTMLTag_wbr:
           
      result = PR_TRUE;
      break;

    default:
      break;

  }
  return result;
}

static PRBool IsBlockLevel(eHTMLTags aTag) 
{
  return !IsInline(aTag);
}

/**
  * **** Pretty Printing Methods ******
  *
  */

/**
  * Desired line break state before the open tag.
  */
static PRBool BreakBeforeOpen(eHTMLTags aTag)
{
 PRBool  result = PR_FALSE;
  switch (aTag)
  {
    case  eHTMLTag_html:
      result = PR_FALSE;
    break;

    default:
      result = IsBlockLevel(aTag);
  }
  return result;
}

/**
  * Desired line break state after the open tag.
  */
static PRBool BreakAfterOpen(eHTMLTags aTag)
{
  PRBool  result = PR_FALSE;
  switch (aTag)
  {
    case eHTMLTag_html:
    case eHTMLTag_body:
    case eHTMLTag_ul:
    case eHTMLTag_ol:
    case eHTMLTag_table:
    case eHTMLTag_tbody:
    case eHTMLTag_style:
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
static PRBool BreakBeforeClose(eHTMLTags aTag)
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
static PRBool BreakAfterClose(eHTMLTags aTag)
{
  PRBool  result = PR_FALSE;

  switch (aTag)
  {
    case  eHTMLTag_html:
    case  eHTMLTag_tr:
    case  eHTMLTag_th:
    case  eHTMLTag_td:
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
static PRBool IndentChildren(eHTMLTags aTag)
{
  PRBool result = PR_FALSE;

  switch (aTag)
  {
    case eHTMLTag_table:
    case eHTMLTag_ul:
    case eHTMLTag_ol:
    case eHTMLTag_tbody:
    case eHTMLTag_form:
    case eHTMLTag_frameset:
      result = PR_TRUE;      
      break;

    default:
      result = PR_FALSE;
      break;
  }
  return result;  
}

/**
  * All tags after this tag and before the closing tag will be output with no
  * formatting.
  */
static PRBool PreformattedChildren(eHTMLTags aTag)
{
  PRBool result = PR_FALSE;
  if (aTag == eHTMLTag_pre)
  {
    result = PR_TRUE;
  }
  return result;
}

///////////////////////////////////////////////////////////////////////////
//////    OBSOLETE CODE FROM HERE TO END OF FILE.
//////             All Ifdef'ed out.
///////////////////////////////////////////////////////////////////////////

#ifdef OBSOLETE
/**
  * Eat the open tag.  Pretty much just for <P*>.
  */
PRBool EatOpen(eHTMLTags aTag)  {
  return PR_FALSE;
}

/**
  * Eat the close tag.  Pretty much just for </P>.
  */
PRBool EatClose(eHTMLTags aTag)  {
  return PR_FALSE;
}

/** @see PermitWSBeforeOpen */
PRBool PermitWSAfterOpen(eHTMLTags aTag)  {
  if (aTag == eHTMLTag_pre)
  {
    return PR_FALSE;
  }
  return PR_TRUE;
}

/**
  * Are we allowed to insert new white space before the open tag.
  *
  * Returning false does not prevent inserting WS
  * before the tag if WS insertion is allowed for another reason,
  * e.g. there is already WS there or we are after a tag that
  * has PermitWSAfter*().
  */
static PRBool PermitWSBeforeOpen(eHTMLTags aTag)
{
  PRBool result = IsInline(aTag) == PR_FALSE;
  return result;
}

/** @see PermitWSBeforeOpen */
PRBool PermitWSBeforeClose(eHTMLTags aTag)  {
  if (aTag == eHTMLTag_pre)
  {
    return PR_FALSE;
  }
  return PR_TRUE;
}

/** @see PermitWSBeforeOpen */
PRBool PermitWSAfterClose(eHTMLTags aTag)  {
  return PR_TRUE;
}

/** @see PermitWSBeforeOpen */
PRBool IgnoreWS(eHTMLTags aTag)  {
  PRBool result = PR_FALSE;

  switch (aTag)
  {
    case eHTMLTag_html:
    case eHTMLTag_head:
    case eHTMLTag_body:
    case eHTMLTag_ul:
    case eHTMLTag_ol:
    case eHTMLTag_li:
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

 /** PRETTY PRINTING PROTOTYPES **/

class nsTagFormat
{
public:
  void Init(PRBool aBefore, PRBool aStart, PRBool aEnd, PRBool aAfter);
  void SetIndentGroup(PRUint8 aGroup);
  void SetFormat(PRBool aOnOff);

public:
  PRBool    mBreakBefore;
  PRBool    mBreakStart;
  PRBool    mBreakEnd;
  PRBool    mBreakAfter;
  
  PRUint8   mIndentGroup; // zero for none
  PRBool    mFormat;      // format (on|off)
};

void nsTagFormat::Init(PRBool aBefore, PRBool aStart, PRBool aEnd, PRBool aAfter)
{
  mBreakBefore = aBefore;
  mBreakStart = aStart;
  mBreakEnd = aEnd;
  mBreakAfter = aAfter;
  mFormat = PR_TRUE;
}

void nsTagFormat::SetIndentGroup(PRUint8 aGroup)
{
  mIndentGroup = aGroup;
}

void nsTagFormat::SetFormat(PRBool aOnOff)
{
  mFormat = aOnOff; 
}

class nsPrettyPrinter
{
public:

  void Init(PRBool aIndentEnable = PR_TRUE, PRUint8 aColSize = 2, PRUint8 aTabSize = 8, PRBool aUseTabs = PR_FALSE );    
  
  PRBool      mIndentEnable;
  PRUint8     mIndentColSize;
  PRUint8     mIndentTabSize;
  PRBool      mIndentUseTabs;

  PRBool      mAutowrapEnable;
  PRUint32    mAutoWrapColWidth;

  nsTagFormat mTagFormat[NS_HTML_TAG_MAX+1];
};


void nsPrettyPrinter::Init(PRBool aIndentEnable, PRUint8 aColSize, PRUint8 aTabSize, PRBool aUseTabs)
{
  mIndentEnable = aIndentEnable;
  mIndentColSize = aColSize;
  mIndentTabSize = aTabSize;
  mIndentUseTabs = aUseTabs;

  mAutowrapEnable = PR_TRUE;
  mAutoWrapColWidth = 72;

  for (PRUint32 i = 0; i < NS_HTML_TAG_MAX; i++)
    mTagFormat[i].Init(PR_FALSE,PR_FALSE,PR_FALSE,PR_FALSE);

  mTagFormat[eHTMLTag_a].Init(PR_TRUE,PR_FALSE,PR_FALSE,PR_TRUE);
  mTagFormat[eHTMLTag_abbr].Init(PR_FALSE,PR_FALSE,PR_FALSE,PR_FALSE);
  mTagFormat[eHTMLTag_applet].Init(PR_FALSE,PR_TRUE,PR_TRUE,PR_FALSE);
  mTagFormat[eHTMLTag_area].Init(PR_TRUE,PR_FALSE,PR_FALSE,PR_TRUE);
  mTagFormat[eHTMLTag_b].Init(PR_FALSE,PR_FALSE,PR_FALSE,PR_FALSE);
  mTagFormat[eHTMLTag_base].Init(PR_TRUE,PR_FALSE,PR_FALSE,PR_TRUE);
  mTagFormat[eHTMLTag_blockquote].Init(PR_TRUE,PR_FALSE,PR_FALSE,PR_TRUE);
  mTagFormat[eHTMLTag_body].Init(PR_TRUE,PR_TRUE,PR_TRUE,PR_TRUE);
  mTagFormat[eHTMLTag_br].Init(PR_FALSE,PR_TRUE,PR_FALSE,PR_TRUE);
  mTagFormat[eHTMLTag_caption].Init(PR_TRUE,PR_FALSE,PR_FALSE,PR_TRUE);
  mTagFormat[eHTMLTag_center].Init(PR_TRUE,PR_TRUE,PR_TRUE,PR_TRUE);
  mTagFormat[eHTMLTag_dd].Init(PR_TRUE,PR_FALSE,PR_FALSE,PR_TRUE);
  mTagFormat[eHTMLTag_dir].Init(PR_TRUE,PR_FALSE,PR_FALSE,PR_TRUE);
  mTagFormat[eHTMLTag_div].Init(PR_TRUE,PR_TRUE,PR_TRUE,PR_TRUE);
  mTagFormat[eHTMLTag_dl].Init(PR_TRUE,PR_FALSE,PR_FALSE,PR_TRUE);
  mTagFormat[eHTMLTag_dt].Init(PR_TRUE,PR_FALSE,PR_FALSE,PR_TRUE);
  mTagFormat[eHTMLTag_embed].Init(PR_TRUE,PR_FALSE,PR_FALSE,PR_TRUE);
  mTagFormat[eHTMLTag_form].Init(PR_TRUE,PR_TRUE,PR_TRUE,PR_TRUE);
  mTagFormat[eHTMLTag_frame].Init(PR_TRUE,PR_FALSE,PR_FALSE,PR_TRUE);
  mTagFormat[eHTMLTag_frameset].Init(PR_TRUE,PR_FALSE,PR_FALSE,PR_TRUE);
  mTagFormat[eHTMLTag_h1].Init(PR_TRUE,PR_FALSE,PR_FALSE,PR_TRUE);
  mTagFormat[eHTMLTag_h2].Init(PR_TRUE,PR_FALSE,PR_FALSE,PR_TRUE);
  mTagFormat[eHTMLTag_h3].Init(PR_TRUE,PR_FALSE,PR_FALSE,PR_TRUE);
  mTagFormat[eHTMLTag_h4].Init(PR_TRUE,PR_FALSE,PR_FALSE,PR_TRUE);
  mTagFormat[eHTMLTag_h5].Init(PR_TRUE,PR_FALSE,PR_FALSE,PR_TRUE);
  mTagFormat[eHTMLTag_h6].Init(PR_TRUE,PR_FALSE,PR_FALSE,PR_TRUE);
  mTagFormat[eHTMLTag_head].Init(PR_TRUE,PR_TRUE,PR_TRUE,PR_TRUE);
  mTagFormat[eHTMLTag_hr].Init(PR_TRUE,PR_FALSE,PR_FALSE,PR_TRUE);
  mTagFormat[eHTMLTag_html].Init(PR_TRUE,PR_TRUE,PR_TRUE,PR_TRUE);
  mTagFormat[eHTMLTag_ilayer].Init(PR_TRUE,PR_TRUE,PR_TRUE,PR_TRUE);
  mTagFormat[eHTMLTag_input].Init(PR_TRUE,PR_FALSE,PR_FALSE,PR_TRUE);
  mTagFormat[eHTMLTag_isindex].Init(PR_TRUE,PR_FALSE,PR_FALSE,PR_TRUE);
  mTagFormat[eHTMLTag_layer].Init(PR_TRUE,PR_TRUE,PR_TRUE,PR_TRUE);
  mTagFormat[eHTMLTag_li].Init(PR_TRUE,PR_FALSE,PR_FALSE,PR_TRUE);
  mTagFormat[eHTMLTag_link].Init(PR_TRUE,PR_FALSE,PR_FALSE,PR_TRUE);
  mTagFormat[eHTMLTag_map].Init(PR_FALSE,PR_TRUE,PR_TRUE,PR_FALSE);
  mTagFormat[eHTMLTag_menu].Init(PR_TRUE,PR_FALSE,PR_FALSE,PR_TRUE);
  mTagFormat[eHTMLTag_meta].Init(PR_TRUE,PR_FALSE,PR_FALSE,PR_TRUE);
  mTagFormat[eHTMLTag_object].Init(PR_FALSE,PR_TRUE,PR_TRUE,PR_FALSE);
  mTagFormat[eHTMLTag_ol].Init(PR_TRUE,PR_TRUE,PR_TRUE,PR_TRUE);
  mTagFormat[eHTMLTag_option].Init(PR_TRUE,PR_FALSE,PR_FALSE,PR_TRUE);
  mTagFormat[eHTMLTag_p].Init(PR_TRUE,PR_FALSE,PR_FALSE,PR_TRUE);
  mTagFormat[eHTMLTag_param].Init(PR_TRUE,PR_FALSE,PR_FALSE,PR_TRUE);
  mTagFormat[eHTMLTag_pre].Init(PR_TRUE,PR_FALSE,PR_FALSE,PR_TRUE);
  mTagFormat[eHTMLTag_script].Init(PR_TRUE,PR_FALSE,PR_FALSE,PR_TRUE);
  mTagFormat[eHTMLTag_select].Init(PR_TRUE,PR_TRUE,PR_TRUE,PR_TRUE);
  mTagFormat[eHTMLTag_style].Init(PR_TRUE,PR_FALSE,PR_FALSE,PR_TRUE);
  mTagFormat[eHTMLTag_table].Init(PR_TRUE,PR_TRUE,PR_TRUE,PR_TRUE);
  mTagFormat[eHTMLTag_td].Init(PR_TRUE,PR_FALSE,PR_FALSE,PR_TRUE);
  mTagFormat[eHTMLTag_textarea].Init(PR_TRUE,PR_FALSE,PR_FALSE,PR_TRUE);
  mTagFormat[eHTMLTag_th].Init(PR_TRUE,PR_FALSE,PR_FALSE,PR_TRUE);
  mTagFormat[eHTMLTag_title].Init(PR_TRUE,PR_FALSE,PR_FALSE,PR_TRUE);
  mTagFormat[eHTMLTag_tr].Init(PR_TRUE,PR_FALSE,PR_FALSE,PR_TRUE);
  mTagFormat[eHTMLTag_ul].Init(PR_TRUE,PR_TRUE,PR_TRUE,PR_TRUE);
}

static PRBool EatOpen(eHTMLTags aTag);
static PRBool EatClose(eHTMLTags aTag);
static PRBool PermitWSAfterOpen(eHTMLTags aTag);
static PRBool PermitWSBeforeClose(eHTMLTags aTag);
static PRBool PermitWSAfterClose(eHTMLTags aTag);
static PRBool IgnoreWS(eHTMLTags aTag);

#endif // OBSOLETE


 
