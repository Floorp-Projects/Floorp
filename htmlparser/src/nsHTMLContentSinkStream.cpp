/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
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
#include "nsHTMLTokens.h"
#include <iostream.h>
#include "nsstring.h"
#include "nsParserTypes.h"

static NS_DEFINE_IID(kISupportsIID,   NS_ISUPPORTS_IID);                 
static NS_DEFINE_IID(kIContentSinkIID,NS_IHTMLCONTENTSINK_IID);
static NS_DEFINE_IID(kClassIID,       NS_HTMLCONTENTSINK_STREAM_IID); 


static char*    gHeaderComment = "<!-- This page was created by the NGLayout output system. -->";
static char*    gDocTypeHeader = "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 3.2//EN\">";
const  int      gTabSize=2;
static char     gBuffer[500];

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
nsresult nsHTMLContentSinkStream::QueryInterface(const nsIID& aIID, void** aInstancePtr)  
{                                                                        
  if (NULL == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      

  if(aIID.Equals(kISupportsIID))    {  //do IUnknown...
    *aInstancePtr = (nsIContentSink*)(this);                                        
  }
  else if(aIID.Equals(kIContentSinkIID)) {  //do IParser base class...
    *aInstancePtr = (nsIContentSink*)(this);                                        
  }
  else if(aIID.Equals(kClassIID)) {  //do this class...
    *aInstancePtr = (nsHTMLContentSinkStream*)(this);                                        
  }                 
  else {
    *aInstancePtr=0;
    return NS_NOINTERFACE;
  }
  ((nsISupports*) *aInstancePtr)->AddRef();
  return NS_OK;                                                        
}


NS_IMPL_ADDREF(nsHTMLContentSinkStream)
NS_IMPL_RELEASE(nsHTMLContentSinkStream)


/**
 *  This method is defined in nsIParser. It is used to 
 *  cause the COM-like construction of an nsParser.
 *  
 *  @update  gess 4/8/98
 *  @param   nsIParser** ptr to newly instantiated parser
 *  @return  NS_xxx error result
 */
NS_HTMLPARS nsresult NS_New_HTML_ContentSinkStream(nsIHTMLContentSink** aInstancePtrResult) {
  nsHTMLContentSinkStream* it = new nsHTMLContentSinkStream();

  if (it == 0) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return it->QueryInterface(kClassIID, (void **) aInstancePtrResult);
}

/**
 * Construct a content sink stream.
 * @update	gess7/7/98
 * @param 
 * @return
 */
nsHTMLContentSinkStream::nsHTMLContentSinkStream()  {
  mOutput=&cout;
  mLowerCaseTags = PR_TRUE;  
  memset(mHTMLTagStack,0,sizeof(mHTMLTagStack));
  mHTMLStackPos = 0;
  mColPos = 0;
  mIndent = 0;
}

/**
 * Construct a content sink stream.
 * @update	gess7/7/98
 * @param 
 * @return
 */
nsHTMLContentSinkStream::nsHTMLContentSinkStream(ostream& aStream)  {
  mOutput = &aStream;
  mLowerCaseTags = PR_TRUE;  
  memset(mHTMLTagStack,0,sizeof(mHTMLTagStack));
  mHTMLStackPos = 0;
  mColPos = 0;
  mIndent = 0;
}


/**
 * 
 * @update	gess7/7/98
 * @param 
 * @return
 */
nsHTMLContentSinkStream::~nsHTMLContentSinkStream() {
  mOutput=0;  //we don't own the stream we're given; just forget it.
}


/**
 * 
 * @update	gess7/7/98
 * @param 
 * @return
 */
void nsHTMLContentSinkStream::SetOutputStream(ostream& aStream) {
  mOutput=&aStream;
}


/**
 * 
 * @update	gess7/7/98
 * @param 
 * @return
 */
void nsHTMLContentSinkStream::WriteAttributes(const nsIParserNode& aNode,ostream& aStream) {
  int theCount=aNode.GetAttributeCount();
  if(theCount) {
    int i=0;
    for(i=0;i<theCount;i++){
      const nsString& temp=aNode.GetKeyAt(i);
      nsString key = temp;
      
      if (mLowerCaseTags == PR_TRUE)
        key.ToLowerCase();
      else
        key.ToUpperCase();


      key.ToCString(gBuffer,sizeof(gBuffer)-1);
      
      aStream << " " << gBuffer << char(kEqual);
      mColPos += 1 + strlen(gBuffer) + 1;
      
      const nsString& value=aNode.GetValueAt(i);
      value.ToCString(gBuffer,sizeof(gBuffer)-1);
      aStream << "\"" << gBuffer << "\"";
      mColPos += 1 + strlen(gBuffer) + 1;
    }
  }
}


/**
 * 
 * @update	gess7/5/98
 * @param 
 * @return
 */
void OpenTagWithAttributes(const char* theTag,const nsIParserNode& aNode,int tab,ostream& aStream,PRBool aNewline) {
  int i=0;
  for(i=0;i<tab*gTabSize;i++) 
    aStream << " ";
  aStream << (char)kLessThan << theTag;
//  WriteAttributes(aNode,aStream);
  aStream << (char)kGreaterThan;
  if(aNewline)
    aStream << endl;
}


/**
 * 
 * @update	gess7/5/98
 * @param 
 * @return
 */
void OpenTag(const char* theTag,int tab,ostream& aStream,PRBool aNewline) {
  int i=0;
  for(i=0;i<tab*gTabSize;i++) 
    aStream << " ";
  aStream << (char)kLessThan << theTag << (char)kGreaterThan;
  if(aNewline)
    aStream << endl;
}


/**
 * 
 * @update	gess7/5/98
 * @param 
 * @return
 */
void CloseTag(const char* theTag,int tab,ostream& aStream) {
  int i=0;
  for(i=0;i<tab*gTabSize;i++) 
    aStream << " ";
  aStream << (char)kLessThan << (char)kForwardSlash << theTag << (char)kGreaterThan << endl;
}


/**
 * 
 * @update	gess7/5/98
 * @param 
 * @return
 */
void WritePair(eHTMLTags aTag,const nsString& theContent,int tab,ostream& aStream) {
  const char* titleStr = GetTagName(aTag);
  OpenTag(titleStr,tab,aStream,false);
  theContent.ToCString(gBuffer,sizeof(gBuffer)-1);
  aStream << gBuffer;
  CloseTag(titleStr,0,aStream);
}



/**
 * 
 * @update	gess7/5/98
 * @param 
 * @return
 */
void WriteSingleton(eHTMLTags aTag,const nsString& theContent,int tab,ostream& aStream) {
  const char* titleStr = GetTagName(aTag);
  OpenTag(titleStr,tab,aStream,true);

  if(theContent.Length()) {
    theContent.ToCString(gBuffer,sizeof(gBuffer)-1);
    aStream << gBuffer;
  }
  aStream << endl;
}



/**
  * This method gets called by the parser when it encounters
  * a title tag and wants to set the document title in the sink.
  *
  * @update 4/1/98 gess
  * @param  nsString reference to new title value
  * @return PR_TRUE if successful. 
  */     
PRBool nsHTMLContentSinkStream::SetTitle(const nsString& aValue){
  PRBool result=PR_FALSE;
  if(mOutput) {
    WritePair(eHTMLTag_title,aValue,1+mTabLevel,*mOutput);
  }
  return result;
}


/**
  * This method is used to open the outer HTML container.
  *
  * @update 07/12/98 gpk
  * @param  nsIParserNode reference to parser node interface
  * @return PR_TRUE if successful. 
  */     
PRInt32 nsHTMLContentSinkStream::OpenHTML(const nsIParserNode& aNode){
  PRInt32 result=0;
  if(mOutput) {
    eHTMLTags tag = (eHTMLTags)aNode.GetNodeType();
    if (tag == eHTMLTag_html)
     AddStartTag(aNode,*mOutput);
  }
  return result;
}


/**
  * This method is used to close the outer HTML container.
  *
  * @update 07/12/98 gpk
  * @param  nsIParserNode reference to parser node interface
  * @return PR_TRUE if successful. 
  */     
PRInt32 nsHTMLContentSinkStream::CloseHTML(const nsIParserNode& aNode){
  PRInt32 result=0;
  if(mOutput) {
    eHTMLTags tag = (eHTMLTags)aNode.GetNodeType();
    if (tag == eHTMLTag_html)
      AddEndTag(aNode,*mOutput);
      mOutput->flush();
  }
  return result;
}


/**
  * This method is used to open the only HEAD container.
  *
  * @update 07/12/98 gpk
  * @param  nsIParserNode reference to parser node interface
  * @return PR_TRUE if successful. 
  */     
PRInt32 nsHTMLContentSinkStream::OpenHead(const nsIParserNode& aNode){
  PRInt32 result=0;
  if(mOutput) {
    eHTMLTags tag = (eHTMLTags)aNode.GetNodeType();
    if (tag == eHTMLTag_head)
     AddStartTag(aNode,*mOutput);
  }
  return result;
}


/**
  * This method is used to close the only HEAD container.
  *
  * @update 07/12/98 gpk
  * @param  nsIParserNode reference to parser node interface
  * @return PR_TRUE if successful. 
  */     
PRInt32 nsHTMLContentSinkStream::CloseHead(const nsIParserNode& aNode){
  PRInt32 result=0;
  if(mOutput) {
    eHTMLTags tag = (eHTMLTags)aNode.GetNodeType();
    if (tag == eHTMLTag_head)
      AddEndTag(aNode,*mOutput);
  }
  return result;
}


/**
  * This method is used to open the main BODY container.
  *
  * @update 07/12/98 gpk
  * @param  nsIParserNode reference to parser node interface
  * @return PR_TRUE if successful. 
  */     
PRInt32 nsHTMLContentSinkStream::OpenBody(const nsIParserNode& aNode){
  PRInt32 result=0;
  if(mOutput) {
    eHTMLTags tag = (eHTMLTags)aNode.GetNodeType();
    if (tag == eHTMLTag_body)
      AddStartTag(aNode,*mOutput);
  }
  return result;
}


/**
  * This method is used to close the main BODY container.
  *
  * @update 07/12/98 gpk
  * @param  nsIParserNode reference to parser node interface
  * @return PR_TRUE if successful. 
  */     
PRInt32 nsHTMLContentSinkStream::CloseBody(const nsIParserNode& aNode){
  PRInt32 result=0;
  if(mOutput) {
    eHTMLTags tag = (eHTMLTags)aNode.GetNodeType();
    if (tag == eHTMLTag_body)
      AddEndTag(aNode,*mOutput);
  }
  return result;
}


/**
  * This method is used to open a new FORM container.
  *
  * @update 07/12/98 gpk
  * @param  nsIParserNode reference to parser node interface
  * @return PR_TRUE if successful. 
  */     
PRInt32 nsHTMLContentSinkStream::OpenForm(const nsIParserNode& aNode){
  PRInt32 result=0;
  if(mOutput) {
    eHTMLTags tag = (eHTMLTags)aNode.GetNodeType();
    if (tag == eHTMLTag_form)
      AddStartTag(aNode,*mOutput);
  }
  return result;
}


/**
  * This method is used to close the outer FORM container.
  *
  * @update 07/12/98 gpk
  * @param  nsIParserNode reference to parser node interface
  * @return PR_TRUE if successful. 
  */     
PRInt32 nsHTMLContentSinkStream::CloseForm(const nsIParserNode& aNode){
  PRInt32 result=0;
  if(mOutput) {
    eHTMLTags tag = (eHTMLTags)aNode.GetNodeType();
    if (tag == eHTMLTag_form)
      AddEndTag(aNode,*mOutput);
  }
  return result;
}

    
/**
  * This method is used to open the FRAMESET container.
  *
  * @update 07/12/98 gpk
  * @param  nsIParserNode reference to parser node interface
  * @return PR_TRUE if successful. 
  */     
PRInt32 nsHTMLContentSinkStream::OpenFrameset(const nsIParserNode& aNode){
  PRInt32 result=0;
  if(mOutput) {
    eHTMLTags tag = (eHTMLTags)aNode.GetNodeType();
    if (tag == eHTMLTag_frameset)
      AddStartTag(aNode,*mOutput);
  }
  return result;
}


/**
  * This method is used to close the FRAMESET container.
  *
  * @update 07/12/98 gpk
  * @param  nsIParserNode reference to parser node interface
  * @return PR_TRUE if successful. 
  */     
PRInt32 nsHTMLContentSinkStream::CloseFrameset(const nsIParserNode& aNode){
  PRInt32 result=0;
  if(mOutput) {
    eHTMLTags tag = (eHTMLTags)aNode.GetNodeType();
    if (tag == eHTMLTag_frameset)
      AddEndTag(aNode,*mOutput);
  }
  return result;
}


void nsHTMLContentSinkStream::AddIndent(ostream& aStream)
{
  for (PRInt32 i = mIndent; --i >= 0; ) 
  {
    aStream << "  ";
    mColPos += 2;
  }
}



void nsHTMLContentSinkStream::AddStartTag(const nsIParserNode& aNode, ostream& aStream)
{
  eHTMLTags         tag = (eHTMLTags)aNode.GetNodeType();
  const nsString&   name = aNode.GetText();
  nsString          tagName;

  mHTMLTagStack[mHTMLStackPos++] = tag;
  tagName = name;
  
  if (mLowerCaseTags == PR_TRUE)
    tagName.ToLowerCase();
  else
    tagName.ToUpperCase();

  
  if (mColPos != 0 && BreakBeforeOpen(tag))
  {
    aStream << endl;
    mColPos = 0;
  }

  if (PermitWSBeforeOpen(tag))
    AddIndent(aStream);

  tagName.ToCString(gBuffer,sizeof(gBuffer)-1);
  aStream << (char)kLessThan << gBuffer;
  mColPos += 1 + tagName.Length();

  if (tag == eHTMLTag_style)
  {
    aStream << (char)kGreaterThan << endl;
    const   nsString& data = aNode.GetSkippedContent();
    PRInt32 size = data.Length();
    char*   buffer = new char[size+1];
    data.ToCString(buffer,size+1);
    aStream << buffer;
    delete buffer;
  }
  else
  {
    WriteAttributes(aNode,aStream);
    aStream << (char)kGreaterThan;
    mColPos += 1;
  }

  if (BreakAfterOpen(tag))
  {
    aStream << endl;
    mColPos = 0;
  }

  if (IndentChildren(tag))
    mIndent++;
}




void nsHTMLContentSinkStream::AddEndTag(const nsIParserNode& aNode, ostream& aStream)
{
  eHTMLTags         tag = (eHTMLTags)aNode.GetNodeType();
  const nsString&   name = aNode.GetText();
  nsString          tagName;

  if (tag == eHTMLTag_unknown)
  {
    tagName = aNode.GetText();
  }
  else
  {
    const char*  name =  NS_EnumToTag(tag);
    tagName = name;
  }
  if (mLowerCaseTags == PR_TRUE)
    tagName.ToLowerCase();
  else
    tagName.ToUpperCase();

  if (IndentChildren(tag))
    mIndent--;

  if (BreakBeforeClose(tag))
  {
    if (mColPos != 0)
    {
      aStream << endl;
      mColPos = 0;
    }
    AddIndent(aStream);
  }

  tagName.ToCString(gBuffer,sizeof(gBuffer)-1);
  aStream << (char)kLessThan << (char)kForwardSlash << gBuffer << (char)kGreaterThan;
  mColPos += 1 + 1 + strlen(gBuffer) + 1;
  
  if (BreakAfterClose(tag))
  {
    aStream << endl;
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
PRInt32 nsHTMLContentSinkStream::AddLeaf(const nsIParserNode& aNode, ostream& aStream){
  PRInt32   result=kNoError;
  eHTMLTags type = (eHTMLTags)aNode.GetNodeType();
  eHTMLTags tag = mHTMLTagStack[mHTMLStackPos-1];
  
  PRBool    preformatted = PR_FALSE;



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
    AddStartTag(aNode,aStream);
    mHTMLTagStack[--mHTMLStackPos] = eHTMLTag_unknown;
  }
  if (type == eHTMLTag_text)
  {
    const nsString& text = aNode.GetText();
    if (preformatted == PR_TRUE)
    {
      text.ToCString(gBuffer,sizeof(gBuffer)-1);
      aStream << gBuffer;
      mColPos += text.Length();
    }
    else
    {
      PRInt32 mMaxColumn = 72;

      // 1. Determine the length of the input string
      PRInt32 length = text.Length();

      // 2. If the offset plus the length of the text is smaller
      // than the max then just add it 
      if (mColPos + length < mMaxColumn)
      {
        text.ToCString(gBuffer,sizeof(gBuffer)-1);
        aStream << gBuffer;
        mColPos += text.Length();
      }
      else
      {
        nsString  str = text;
        PRBool    done = PR_FALSE;
        PRInt32   index = 0;
        PRInt32   offset = mColPos;

        while (!done)
        {        
          // find the next break
          PRInt32 start = mMaxColumn-offset;
          if (start < 0)
            start = 0;
          
          index = str.Find(' ',start);

          // if there is no break than just add it
          if (index == kNotFound)
          {
            str.ToCString(gBuffer,sizeof(gBuffer)-1);
            aStream << gBuffer;
            mColPos += str.Length();
            done = PR_TRUE;
          }
          else
          {
            // make first equal to the str from the 
            // beginning to the index
            nsString  first = str;

            first.Truncate(index);
  
            first.ToCString(gBuffer,sizeof(gBuffer)-1);
            aStream << gBuffer << endl;
            mColPos = 0;
  
            // cut the string from the beginning to the index
            str.Cut(0,index);
            offset = 0;
          }
        }
      }
    }
  }
  else if (type == eHTMLTag_whitespace)
  {
    if (preformatted || IgnoreWS(tag) == PR_FALSE)
    {
      const nsString& text = aNode.GetText();
      text.ToCString(gBuffer,sizeof(gBuffer)-1);
      aStream << gBuffer;
      mColPos += text.Length();
    }
  }
  else if (type == eHTMLTag_newline)
  {
    if (preformatted)
    {
      const nsString& text = aNode.GetText();
      text.ToCString(gBuffer,sizeof(gBuffer)-1);
      aStream << gBuffer;
      mColPos = 0;
    }
  }

  return result;
}


    
/**
  * This method is used to a general container. 
  * This includes: OL,UL,DIR,SPAN,TABLE,H[1..6],etc.
  *
  * @update 07/12/98 gpk
  * @param  nsIParserNode reference to parser node interface
  * @return PR_TRUE if successful. 
  */     
PRInt32 nsHTMLContentSinkStream::OpenContainer(const nsIParserNode& aNode){
  PRInt32 result=0;
  if(mOutput) {
    AddStartTag(aNode,*mOutput);
    eHTMLTags  tag = (eHTMLTags)aNode.GetNodeType();
  }
  return result;
}


/**
  * This method is used to close a generic container.
  *
  * @update 07/12/98 gpk
  * @param  nsIParserNode reference to parser node interface
  * @return PR_TRUE if successful. 
  */     
PRInt32 nsHTMLContentSinkStream::CloseContainer(const nsIParserNode& aNode){
  PRInt32 result=0;
  if(mOutput) {
    AddEndTag(aNode,*mOutput);
  }
  return result;
}


/**
  * This method is used to add a leaf to the currently 
  * open container.
  *
  * @update 07/12/98 gpk
  * @param  nsIParserNode reference to parser node interface
  * @return PR_TRUE if successful. 
  */     
PRInt32 nsHTMLContentSinkStream::AddLeaf(const nsIParserNode& aNode){
  PRInt32 result=0;
  if(mOutput) {  
    AddLeaf(aNode,*mOutput);
  }
  return result;
}


/**
  * This method gets called when the parser begins the process
  * of building the content model via the content sink.
  *
  * @update 5/7/98 gess
  */     
void nsHTMLContentSinkStream::WillBuildModel(void){
  mTabLevel=-1;
  if(mOutput) {
    (*mOutput) << gHeaderComment << endl;
    (*mOutput) << gDocTypeHeader << endl;
  }
}


/**
  * This method gets called when the parser concludes the process
  * of building the content model via the content sink.
  *
  * @param  aQualityLevel describes how well formed the doc was.
  *         0=GOOD; 1=FAIR; 2=POOR;
  * @update 5/7/98 gess
  */     
void nsHTMLContentSinkStream::DidBuildModel(PRInt32 aQualityLevel) {
}


/**
  * This method gets called when the parser gets i/o blocked,
  * and wants to notify the sink that it may be a while before
  * more data is available.
  *
  * @update 5/7/98 gess
  */     
void nsHTMLContentSinkStream::WillInterrupt(void) {
}


/**
  * This method gets called when the parser i/o gets unblocked,
  * and we're about to start dumping content again to the sink.
  *
  * @update 5/7/98 gess
  */     
void nsHTMLContentSinkStream::WillResume(void) {
}




/**
  * **** Pretty Printing Methods ******
  *
  */
 


PRBool nsHTMLContentSinkStream::IsInline(eHTMLTags aTag) const
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
  }
  return result;
}

PRBool nsHTMLContentSinkStream::IsBlockLevel(eHTMLTags aTag) const
{
  return !IsInline(aTag);
}


/**
  * Desired line break state before the open tag.
  */
PRBool nsHTMLContentSinkStream::BreakBeforeOpen(eHTMLTags aTag) const {
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
PRBool nsHTMLContentSinkStream::BreakAfterOpen(eHTMLTags aTag) const {
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
      result = PR_TRUE;
    break;
  }
  return result;
}

/**
  * Desired line break state before the close tag.
  */
PRBool nsHTMLContentSinkStream::BreakBeforeClose(eHTMLTags aTag) const {
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
  }
  return result;
}

/**
  * Desired line break state after the close tag.
  */
PRBool nsHTMLContentSinkStream::BreakAfterClose(eHTMLTags aTag) const {
  PRBool  result = PR_FALSE;

  switch (aTag)
  {
    case  eHTMLTag_html:
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
PRBool nsHTMLContentSinkStream::IndentChildren(eHTMLTags aTag) const {
  
  PRBool result = PR_FALSE;

  switch (aTag)
  {
    case eHTMLTag_html:
    case eHTMLTag_pre:
    case eHTMLTag_body:
    case eHTMLTag_style:
      result = PR_FALSE;
    break;

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
PRBool nsHTMLContentSinkStream::PreformattedChildren(eHTMLTags aTag) const {
  PRBool result = PR_FALSE;
  if (aTag == eHTMLTag_pre)
  {
    result = PR_TRUE;
  }
  return result;
}

/**
  * Eat the open tag.  Pretty much just for <P*>.
  */
PRBool nsHTMLContentSinkStream::EatOpen(eHTMLTags aTag) const {
  return PR_FALSE;
}

/**
  * Eat the close tag.  Pretty much just for </P>.
  */
PRBool nsHTMLContentSinkStream::EatClose(eHTMLTags aTag) const {
  return PR_FALSE;
}

/**
  * Are we allowed to insert new white space before the open tag.
  *
  * Returning false does not prevent inserting WS
  * before the tag if WS insertion is allowed for another reason,
  * e.g. there is already WS there or we are after a tag that
  * has PermitWSAfter*().
  */
PRBool nsHTMLContentSinkStream::PermitWSBeforeOpen(eHTMLTags aTag) const {
  PRBool result = IsInline(aTag) == PR_FALSE;
  return result;
}

/** @see PermitWSBeforeOpen */
PRBool nsHTMLContentSinkStream::PermitWSAfterOpen(eHTMLTags aTag) const {
  if (aTag == eHTMLTag_pre)
  {
    return PR_FALSE;
  }
  return PR_TRUE;
}

/** @see PermitWSBeforeOpen */
PRBool nsHTMLContentSinkStream::PermitWSBeforeClose(eHTMLTags aTag) const {
  if (aTag == eHTMLTag_pre)
  {
    return PR_FALSE;
  }
  return PR_TRUE;
}

/** @see PermitWSBeforeOpen */
PRBool nsHTMLContentSinkStream::PermitWSAfterClose(eHTMLTags aTag) const {
  return PR_TRUE;
}


/** @see PermitWSBeforeOpen */
PRBool nsHTMLContentSinkStream::IgnoreWS(eHTMLTags aTag) const {
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
  }

  return result;
}


