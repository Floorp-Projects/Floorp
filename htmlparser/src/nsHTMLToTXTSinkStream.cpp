/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
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
#include <iostream.h>
#include "nsString.h"
#include "nsIParser.h"
#include "nsHTMLEntities.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);                 
static NS_DEFINE_IID(kIContentSinkIID, NS_ICONTENT_SINK_IID);
static NS_DEFINE_IID(kIHTMLContentSinkIID, NS_IHTML_CONTENT_SINK_IID);

const  int               gTabSize=2;

static PRBool IsInline(eHTMLTags aTag);
static PRBool IsBlockLevel(eHTMLTags aTag);





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
  if(aIID.Equals(kISupportsIID)) {
    *aInstancePtr = (nsIContentSink*)(this);
  }
  else if(aIID.Equals(kIContentSinkIID)) {
    *aInstancePtr = (nsIContentSink*)(this);
  }
  else if(aIID.Equals(kIHTMLContentSinkIID)) {
    *aInstancePtr = (nsIHTMLContentSink*)(this);
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


/**
 *  This method is defined in nsIParser. It is used to 
 *  cause the COM-like construction of an nsParser.
 *  
 *  @update  gpk02/03/99
 *  @param   nsIParser** ptr to newly instantiated parser
 *  @return  NS_xxx error result
 */
NS_HTMLPARS nsresult
NS_New_HTMLToTXT_SinkStream(nsIHTMLContentSink** aInstancePtrResult) {
  nsHTMLToTXTSinkStream* it = new nsHTMLToTXTSinkStream();
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return it->QueryInterface(kIHTMLContentSinkIID, (void **)aInstancePtrResult);
}

/**
 * Construct a content sink stream.
 * @update	gpk02/03/99
 * @param 
 * @return
 */
nsHTMLToTXTSinkStream::nsHTMLToTXTSinkStream()  {
  NS_INIT_REFCNT();
  mOutput=&cout;
  mColPos = 0;
  mIndent = 0;
  mDoOutput = PR_FALSE;
  mBufferSize = 0;
  mBuffer = nsnull;
}

/**
 * Construct a content sink stream.
 * @update	gpk02/03/99
 * @param 
 * @return
 */
nsHTMLToTXTSinkStream::nsHTMLToTXTSinkStream(ostream& aStream)  {
  NS_INIT_REFCNT();
  mOutput = &aStream;
  mColPos = 0;
  mIndent = 0;
  mDoOutput = PR_FALSE;
  mBufferSize = 0;
  mBuffer = nsnull;
}


/**
 * 
 * @update	gpk02/03/99
 * @param 
 * @return
 */
nsHTMLToTXTSinkStream::~nsHTMLToTXTSinkStream() {
  mOutput=0;  //we don't own the stream we're given; just forget it.
  delete [] mBuffer;
}


/**
 * 
 * @update	gpk02/03/99
 * @param 
 * @return
 */
NS_IMETHODIMP_(void)
nsHTMLToTXTSinkStream::SetOutputStream(ostream& aStream){
  mOutput=&aStream;
}



/**
 * 
 * @update	gpk02/03/99
 * @param 
 * @return
 */
static
void OpenTagWithAttributes(const char* theTag,const nsIParserNode& aNode,int tab,ostream& aStream,PRBool aNewline) {
}


/**
 * 
 * @update	gpk02/03/99
 * @param 
 * @return
 */
static
void OpenTag(const char* theTag,int tab,ostream& aStream,PRBool aNewline) {
}


/**
 * 
 * @update	gpk02/03/99
 * @param 
 * @return
 */
static
void CloseTag(const char* theTag,int tab,ostream& aStream) {
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
nsHTMLToTXTSinkStream::SetTitle(const nsString& aValue){
  return NS_OK;
}


/**
  * This method is used to open the outer HTML container.
  *
  * @update 07/12/98 gpk
  * @param  nsIParserNode reference to parser node interface
  * @return PR_TRUE if successful. 
  */     
NS_IMETHODIMP
nsHTMLToTXTSinkStream::OpenHTML(const nsIParserNode& aNode){
  return NS_OK;
}


/**
  * This method is used to close the outer HTML container.
  *
  * @update 07/12/98 gpk
  * @param  nsIParserNode reference to parser node interface
  * @return PR_TRUE if successful. 
  */     
NS_IMETHODIMP
nsHTMLToTXTSinkStream::CloseHTML(const nsIParserNode& aNode){
  return NS_OK;
}


/**
  * This method is used to open the only HEAD container.
  *
  * @update 07/12/98 gpk
  * @param  nsIParserNode reference to parser node interface
  * @return PR_TRUE if successful. 
  */     
NS_IMETHODIMP
nsHTMLToTXTSinkStream::OpenHead(const nsIParserNode& aNode){
  return NS_OK;
}


/**
  * This method is used to close the only HEAD container.
  *
  * @update 07/12/98 gpk
  * @param  nsIParserNode reference to parser node interface
  * @return PR_TRUE if successful. 
  */     
NS_IMETHODIMP
nsHTMLToTXTSinkStream::CloseHead(const nsIParserNode& aNode){
  return NS_OK;
}


/**
  * This method is used to open the main BODY container.
  *
  * @update 07/12/98 gpk
  * @param  nsIParserNode reference to parser node interface
  * @return PR_TRUE if successful. 
  */     
NS_IMETHODIMP
nsHTMLToTXTSinkStream::OpenBody(const nsIParserNode& aNode){
  return NS_OK;
}


/**
  * This method is used to close the main BODY container.
  *
  * @update 07/12/98 gpk
  * @param  nsIParserNode reference to parser node interface
  * @return PR_TRUE if successful. 
  */     
NS_IMETHODIMP
nsHTMLToTXTSinkStream::CloseBody(const nsIParserNode& aNode){
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
nsHTMLToTXTSinkStream::OpenForm(const nsIParserNode& aNode){
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
nsHTMLToTXTSinkStream::CloseForm(const nsIParserNode& aNode){
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
nsHTMLToTXTSinkStream::OpenMap(const nsIParserNode& aNode){
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
nsHTMLToTXTSinkStream::CloseMap(const nsIParserNode& aNode){
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
nsHTMLToTXTSinkStream::OpenFrameset(const nsIParserNode& aNode){
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
nsHTMLToTXTSinkStream::CloseFrameset(const nsIParserNode& aNode){
  return NS_OK;
}


NS_IMETHODIMP
nsHTMLToTXTSinkStream::DoFragment(PRBool aFlag) 
{
  if (aFlag)
    mDoOutput = PR_TRUE;
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

void nsHTMLToTXTSinkStream::EnsureBufferSize(PRInt32 aNewSize)
{
  if (mBufferSize < aNewSize)
  {
    delete [] mBuffer;
    mBufferSize = 2*aNewSize+1; // make the twice as large
    mBuffer = new char[mBufferSize];
    mBuffer[0] = 0;
  }
}


void nsHTMLToTXTSinkStream::UnicodeToTXTString(const nsString& aSrc)
{
#define CH_NBSP 160
#define CH_QUOT 34
#define CH_AMP  38
#define CH_LT   60
#define CH_GT   62

  PRInt32       length = aSrc.Length();
  PRUnichar     ch; 
  const char*   entity = nsnull;
  PRUint32      offset = 0;
  PRUint32      addedLength = 0;

  if (length > 0)
  {
    EnsureBufferSize(length);
    for (PRInt32 i = 0; i < length; i++)
    {
      ch = aSrc[i];
      switch (ch)
      {  
        case CH_QUOT: ch = '"'; break;
        case CH_AMP:  ch = '&'; break;
        case CH_GT:   ch = '>'; break;
        case CH_LT:   ch = '<'; break;
        case CH_NBSP: ch = ' '; break;
      }

      if (ch < 128)
      {
        mBuffer[offset++] = (unsigned char)ch;
        mBuffer[offset] = 0;
      }
    }
  }
}


NS_IMETHODIMP
nsHTMLToTXTSinkStream::GetStringBuffer(nsString & aStrBuffer) 
{
  aStrBuffer = mStrBuffer;
  return NS_OK; 
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
nsHTMLToTXTSinkStream::AddLeaf(const nsIParserNode& aNode, ostream& aStream)
{
  eHTMLTags type = (eHTMLTags)aNode.GetNodeType();
  
  const nsString& text = aNode.GetText();

  if (mDoOutput == PR_FALSE)
    return NS_OK;

  if (type == eHTMLTag_text) {

    UnicodeToTXTString(text);
    aStream << mBuffer;
    mStrBuffer.Append(mBuffer);
    mColPos += text.Length();
  } 
  else if (type == eHTMLTag_whitespace)
  {
    if (PR_TRUE)
    {
      const nsString& text = aNode.GetText();
      UnicodeToTXTString(text);
      aStream << mBuffer;
      mStrBuffer.Append(mBuffer);
      mColPos += text.Length();
    }
  }
  else if (type == eHTMLTag_br)
  {
    if (PR_TRUE)
    {
      aStream << endl;
      mStrBuffer.Append("\n");
      mColPos += 1;
    }
  }


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
 *  This gets called by the parser when you want to add
 *  a comment node to the current container in the content
 *  model.
 *  
 *  @updated gpk02/03/99
 *  @param   
 *  @return  
 */
NS_IMETHODIMP
nsHTMLToTXTSinkStream::AddComment(const nsIParserNode& aNode){
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
nsHTMLToTXTSinkStream::OpenContainer(const nsIParserNode& aNode){
  eHTMLTags type = (eHTMLTags)aNode.GetNodeType();
  const nsString&   name = aNode.GetText();

  if (type == eHTMLTag_body)
    mDoOutput = PR_TRUE;
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
nsHTMLToTXTSinkStream::CloseContainer(const nsIParserNode& aNode){
  eHTMLTags type = (eHTMLTags)aNode.GetNodeType();
  const nsString&   name = aNode.GetText();

  if (type == eHTMLTag_body)
    mDoOutput = PR_FALSE;

  if (IsBlockLevel(type))
  {
    if (mColPos != 0)
    {  
      if (mOutput)
        *mOutput << endl;
      mStrBuffer.Append("\n");
      mColPos = 0;
    }
  }
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
nsHTMLToTXTSinkStream::AddLeaf(const nsIParserNode& aNode){
  nsresult result = NS_OK;
  if(mOutput) {  
    result = AddLeaf(aNode,*mOutput);
  }
  return result;
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


PRBool IsInline(eHTMLTags aTag)
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
    case  eHTMLTag_td:
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

PRBool IsBlockLevel(eHTMLTags aTag) 
{
  return !IsInline(aTag);
}

