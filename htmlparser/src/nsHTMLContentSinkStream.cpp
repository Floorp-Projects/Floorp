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
nsresult CHTMLContentSinkStream::QueryInterface(const nsIID& aIID, void** aInstancePtr)  
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
    *aInstancePtr = (CHTMLContentSinkStream*)(this);                                        
  }                 
  else {
    *aInstancePtr=0;
    return NS_NOINTERFACE;
  }
  ((nsISupports*) *aInstancePtr)->AddRef();
  return NS_OK;                                                        
}


NS_IMPL_ADDREF(CHTMLContentSinkStream)
NS_IMPL_RELEASE(CHTMLContentSinkStream)


/**
 *  This method is defined in nsIParser. It is used to 
 *  cause the COM-like construction of an nsParser.
 *  
 *  @update  gess 4/8/98
 *  @param   nsIParser** ptr to newly instantiated parser
 *  @return  NS_xxx error result
 */
NS_HTMLPARS nsresult NS_New_HTML_ContentSinkStream(CHTMLContentSinkStream** aInstancePtrResult,ostream* aStream) {
  CHTMLContentSinkStream* it = new CHTMLContentSinkStream(aStream);

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
CHTMLContentSinkStream::CHTMLContentSinkStream(ostream* aStream)  {
  mOutput=(0==aStream) ? &cout : aStream;
}


/**
 * 
 * @update	gess7/7/98
 * @param 
 * @return
 */
CHTMLContentSinkStream::~CHTMLContentSinkStream() {
  mOutput=0;  //we don't own the stream we're given; just forget it.
}


/**
 * 
 * @update	gess7/7/98
 * @param 
 * @return
 */
void WriteAttributes(const nsIParserNode& aNode,ostream& aStream) {
  int theCount=aNode.GetAttributeCount();
  if(theCount) {
    int i=0;
    for(i=0;i<theCount;i++){
      const nsString& key=aNode.GetKeyAt(i);
      key.ToCString(gBuffer,sizeof(gBuffer)-1);
      aStream << " " << gBuffer << char(kEqual);
      const nsString& value=aNode.GetValueAt(i);
      value.ToCString(gBuffer,sizeof(gBuffer)-1);
      aStream << gBuffer;
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
  WriteAttributes(aNode,aStream);
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
  OpenTag(titleStr,tab,aStream,PR_FALSE);
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
  OpenTag(titleStr,tab,aStream,PR_FALSE);
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
PRBool CHTMLContentSinkStream::SetTitle(const nsString& aValue){
  PRBool result=PR_FALSE;
  if(mOutput) {
    WritePair(eHTMLTag_title,aValue,1+mTabLevel,*mOutput);
  }
  return result;
}


/**
  * This method is used to open the outer HTML container.
  *
  * @update 4/1/98 gess
  * @param  nsIParserNode reference to parser node interface
  * @return PR_TRUE if successful. 
  */     
PRInt32 CHTMLContentSinkStream::OpenHTML(const nsIParserNode& aNode){
  PRInt32 result=0;
  mTabLevel++;
  if(mOutput) {
    const char* theTag= GetTagName(eHTMLTag_html);
    OpenTagWithAttributes(theTag,aNode,mTabLevel,*mOutput,PR_TRUE);
  }
  return result;
}


/**
  * This method is used to close the outer HTML container.
  *
  * @update 4/1/98 gess
  * @param  nsIParserNode reference to parser node interface
  * @return PR_TRUE if successful. 
  */     
PRInt32 CHTMLContentSinkStream::CloseHTML(const nsIParserNode& aNode){
  PRInt32 result=0;
  if(mOutput) {
    const char* theTag= GetTagName(eHTMLTag_html);
    CloseTag(theTag,0,*mOutput);
  }
  mTabLevel--;
  return result;
}


/**
  * This method is used to open the only HEAD container.
  *
  * @update 4/1/98 gess
  * @param  nsIParserNode reference to parser node interface
  * @return PR_TRUE if successful. 
  */     
PRInt32 CHTMLContentSinkStream::OpenHead(const nsIParserNode& aNode){
  PRInt32 result=0;
  mTabLevel++;
  if(mOutput) {
    const char* theTag= GetTagName(eHTMLTag_head);
    OpenTagWithAttributes(theTag,aNode,mTabLevel,*mOutput,PR_TRUE);
  }
  return result;
}


/**
  * This method is used to close the only HEAD container.
  *
  * @update 4/1/98 gess
  * @param  nsIParserNode reference to parser node interface
  * @return PR_TRUE if successful. 
  */     
PRInt32 CHTMLContentSinkStream::CloseHead(const nsIParserNode& aNode){
  PRInt32 result=0;
  if(mOutput) {
    const char* theTag= GetTagName(eHTMLTag_head);
    CloseTag(theTag,mTabLevel,*mOutput);
  }
  mTabLevel--;
  return result;
}


/**
  * This method is used to open the main BODY container.
  *
  * @update 4/1/98 gess
  * @param  nsIParserNode reference to parser node interface
  * @return PR_TRUE if successful. 
  */     
PRInt32 CHTMLContentSinkStream::OpenBody(const nsIParserNode& aNode){
  PRInt32 result=0;
  mTabLevel++;
  if(mOutput) {
    const char* theTag= GetTagName(eHTMLTag_body);
    OpenTagWithAttributes(theTag,aNode,mTabLevel,*mOutput,PR_TRUE);
  }
  return result;
}


/**
  * This method is used to close the main BODY container.
  *
  * @update 4/1/98 gess
  * @param  nsIParserNode reference to parser node interface
  * @return PR_TRUE if successful. 
  */     
PRInt32 CHTMLContentSinkStream::CloseBody(const nsIParserNode& aNode){
  PRInt32 result=0;
  if(mOutput) {
    const char* theTag= GetTagName(eHTMLTag_body);
    CloseTag(theTag,mTabLevel,*mOutput);
  }
  mTabLevel--;
  return result;
}


/**
  * This method is used to open a new FORM container.
  *
  * @update 4/1/98 gess
  * @param  nsIParserNode reference to parser node interface
  * @return PR_TRUE if successful. 
  */     
PRInt32 CHTMLContentSinkStream::OpenForm(const nsIParserNode& aNode){
  PRInt32 result=0;
  mTabLevel++;
  if(mOutput) {
    const char* theTag= GetTagName(eHTMLTag_form);
    OpenTagWithAttributes(theTag,aNode,mTabLevel,*mOutput,PR_TRUE);
  }
  return result;
}


/**
  * This method is used to close the outer FORM container.
  *
  * @update 4/1/98 gess
  * @param  nsIParserNode reference to parser node interface
  * @return PR_TRUE if successful. 
  */     
PRInt32 CHTMLContentSinkStream::CloseForm(const nsIParserNode& aNode){
  PRInt32 result=0;
  if(mOutput) {
    const char* theTag= GetTagName(eHTMLTag_form);
    CloseTag(theTag,mTabLevel,*mOutput);
  }
  mTabLevel--;
  return result;
}

    
/**
  * This method is used to open the FRAMESET container.
  *
  * @update 4/1/98 gess
  * @param  nsIParserNode reference to parser node interface
  * @return PR_TRUE if successful. 
  */     
PRInt32 CHTMLContentSinkStream::OpenFrameset(const nsIParserNode& aNode){
  PRInt32 result=0;
  mTabLevel++;
  if(mOutput) {
    const char* theTag= GetTagName(eHTMLTag_frameset);
    OpenTagWithAttributes(theTag,aNode,mTabLevel,*mOutput,PR_TRUE);
  }
  return result;
}


/**
  * This method is used to close the FRAMESET container.
  *
  * @update 4/1/98 gess
  * @param  nsIParserNode reference to parser node interface
  * @return PR_TRUE if successful. 
  */     
PRInt32 CHTMLContentSinkStream::CloseFrameset(const nsIParserNode& aNode){
  PRInt32 result=0;
  if(mOutput) {
    const char* theTag= GetTagName(eHTMLTag_frameset);
    CloseTag(theTag,mTabLevel,*mOutput);
  }
  mTabLevel--;
  return result;
}

    
/**
  * This method is used to a general container. 
  * This includes: OL,UL,DIR,SPAN,TABLE,H[1..6],etc.
  *
  * @update 4/1/98 gess
  * @param  nsIParserNode reference to parser node interface
  * @return PR_TRUE if successful. 
  */     
PRInt32 CHTMLContentSinkStream::OpenContainer(const nsIParserNode& aNode){
  PRInt32 result=0;
  mTabLevel++;
  if(mOutput) {

    eHTMLTags nodeType=(eHTMLTags)aNode.GetNodeType();

    switch(nodeType) {

      case eHTMLTag_html:
        result=OpenHTML(aNode); break;

      case eHTMLTag_body:
        result=OpenBody(aNode); break;

      case eHTMLTag_title:
        {
          const nsString& title=aNode.GetName();
          result=SetTitle(title); 
        }
        break;

      case eHTMLTag_head:
        result=OpenHead(aNode); break;

      case eHTMLTag_style:
        break;

      case eHTMLTag_form:
        result=OpenForm(aNode); break;

      default:
        //this is the hard one. You have to decode aNode manually.
        {
          const nsString& name=aNode.GetName();
          const char* tagName= GetTagName(nodeType);
          OpenTagWithAttributes(tagName,aNode,mTabLevel,*mOutput,PR_TRUE);
        }
        break;
    }


  }
  return result;
}


/**
  * This method is used to close a generic container.
  *
  * @update 4/1/98 gess
  * @param  nsIParserNode reference to parser node interface
  * @return PR_TRUE if successful. 
  */     
PRInt32 CHTMLContentSinkStream::CloseContainer(const nsIParserNode& aNode){
  PRInt32 result=0;
  if(mOutput) {

    eHTMLTags nodeType=(eHTMLTags)aNode.GetNodeType();
  
    switch(nodeType) {

      case eHTMLTag_html:
        result=CloseHTML(aNode); break;

      case eHTMLTag_style:
        break;

      case eHTMLTag_head:
        result=CloseHead(aNode); break; 

      case eHTMLTag_body:
        result=CloseBody(aNode); break;

      case eHTMLTag_form:
        result=CloseForm(aNode); break;

      case eHTMLTag_title:
//        result=SetTitle(aNode); break;

      default:
        {
          const nsString& name=aNode.GetName();
          const char* tagName= GetTagName(nodeType);
          //(*mOutput) << endl;
          CloseTag(tagName,mTabLevel,*mOutput);
        }
        break;
    }
  }
  mTabLevel--;
  return result;
}


/**
  * This method is used to add a leaf to the currently 
  * open container.
  *
  * @update 4/1/98 gess
  * @param  nsIParserNode reference to parser node interface
  * @return PR_TRUE if successful. 
  */     
PRInt32 CHTMLContentSinkStream::AddLeaf(const nsIParserNode& aNode){
  PRInt32 result=0;
  if(mOutput) {

    eHTMLTags nodeType=(eHTMLTags)aNode.GetNodeType();

    switch(nodeType) {
      case eHTMLTag_hr:
      case eHTMLTag_br:
        {
          nsAutoString empty;
          WriteSingleton(nodeType,empty,1+mTabLevel,*mOutput);
        }
        break;

      case eHTMLTag_style:
        {
          const nsString& skipped=aNode.GetSkippedContent();
          WritePair(nodeType,skipped,1+mTabLevel,*mOutput);
        }
        break;

      case eHTMLTag_newline:
      case eHTMLTag_whitespace:
        break;

      default:
        {
          int i=0;
          for(i=0;i<(1+mTabLevel)*gTabSize;i++) 
            (*mOutput) << " ";
    
          const nsString& text=aNode.GetText();
          text.ToCString(gBuffer,sizeof(gBuffer)-1);
          (*mOutput) << gBuffer << endl;
        }
    }//switch
  }
  return result;
}


/**
  * This method gets called when the parser begins the process
  * of building the content model via the content sink.
  *
  * @update 5/7/98 gess
  */     
void CHTMLContentSinkStream::WillBuildModel(void){
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
void CHTMLContentSinkStream::DidBuildModel(PRInt32 aQualityLevel) {
}


/**
  * This method gets called when the parser gets i/o blocked,
  * and wants to notify the sink that it may be a while before
  * more data is available.
  *
  * @update 5/7/98 gess
  */     
void CHTMLContentSinkStream::WillInterrupt(void) {
}


/**
  * This method gets called when the parser i/o gets unblocked,
  * and we're about to start dumping content again to the sink.
  *
  * @update 5/7/98 gess
  */     
void CHTMLContentSinkStream::WillResume(void) {
}


