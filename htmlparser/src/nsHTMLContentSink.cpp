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

#include "nsHTMLContentSink.h"
#include "nsHTMLTokens.h"
#include "nsParserTypes.h"
#include "prtypes.h" 
#include <iostream.h>  

#define VERBOSE_DEBUG

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);                 
static NS_DEFINE_IID(kIContentSinkIID, NS_ICONTENTSINK_IID);
static NS_DEFINE_IID(kIHTMLContentSinkIID, NS_IHTMLCONTENTSINK_IID);
static NS_DEFINE_IID(kClassIID, NS_HTMLCONTENTSINK_IID); 



/**
 *  "Fakey" factory method used to create an instance of
 *  this class.
 *  
 *  @updated gess 3/25/98
 *  @param   
 *  @return  
 */
nsresult NS_NewHTMLContentSink(nsIContentSink** aInstancePtrResult)
{
  nsHTMLContentSink *it = new nsHTMLContentSink();

  if (it == 0) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return it->QueryInterface(kClassIID, (void **) aInstancePtrResult);
}


/**
 *  Default constructor
 *  
 *  @updated gess 3/25/98
 *  @param   
 *  @return  
 */
nsHTMLContentSink::nsHTMLContentSink() : nsIHTMLContentSink(), mTitle("") {
  mNodeStackPos=0;
  memset(mNodeStack,0,sizeof(mNodeStack));
}


/**
 *  Default destructor. Probably not a good idea to call 
 *  this if you created your instance via the factor method.
 *  
 *  @updated gess 3/25/98
 *  @param   
 *  @return  
 */
nsHTMLContentSink::~nsHTMLContentSink() {
}

#ifdef VERBOSE_DEBUG
static void DebugDump(const char* str1,const nsString& str2,PRInt32 tabs) {
  for(PRInt32 i=0;i<tabs;i++)
    cout << " "; //add some tabbing to debug output...
  char* cp = str2.ToNewCString();
  cout << str1 << cp << ">" << endl;
  delete cp;
}
#endif


/**
 *  This bit of magic creates the addref and release 
 *  methods for this class.
 *
 *  @updated gess 3/25/98
 *  @param  
 *  @return 
 */
NS_IMPL_ADDREF(nsHTMLContentSink)
NS_IMPL_RELEASE(nsHTMLContentSink)



/**
 *  Standard XPCOM query interface implementation. I used
 *  my own version because this class is a subclass of both
 *  ISupports and IContentSink. Perhaps there's a macro for
 *  this, but I didn't see it.
 *  
 *  @updated gess 3/25/98
 *  @param   
 *  @return  
 */
nsresult nsHTMLContentSink::QueryInterface(const nsIID& aIID, void** aInstancePtr)  
{                                                                        
  if (NULL == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      

  if(aIID.Equals(kISupportsIID))    {  //do IUnknown...
    *aInstancePtr = (nsIContentSink*)(this);                                        
  }
  else if(aIID.Equals(kIContentSinkIID)) {  //do nsIContentSink base class...
    *aInstancePtr = (nsIContentSink*)(this);                                        
  }
  else if(aIID.Equals(kIHTMLContentSinkIID)) {  //do nsIHTMLContentSink base class...
    *aInstancePtr = (nsIHTMLContentSink*)(this);                                        
  }
  else if(aIID.Equals(kClassIID)) {  //do this class...
    *aInstancePtr = (nsHTMLContentSink*)(this);                                        
  }                 
  else {
    *aInstancePtr=0;
    return NS_NOINTERFACE;
  }
  ((nsISupports*) *aInstancePtr)->AddRef();
  return NS_OK;                                                        
}


/**
 *  This method gets called by the parser when a <HTML> 
 *  tag has been consumed.
 *  
 *  @updated gess 3/25/98
 *  @param   
 *  @return  
 */
PRInt32 nsHTMLContentSink::OpenHTML(const nsIParserNode& aNode) {

  PRInt32 result=kNoError;
  mNodeStack[mNodeStackPos++]=(eHTMLTags)aNode.GetNodeType();

#ifdef VERBOSE_DEBUG
  DebugDump("<",aNode.GetText(),(mNodeStackPos-1)*2);
#endif

  return result;
}

/**
 *  This method gets called by the parser when a </HTML> 
 *  tag has been consumed.
 *  @updated gess 3/25/98
 *  @param   
 *  @return  
 */
PRInt32 nsHTMLContentSink::CloseHTML(const nsIParserNode& aNode){

  NS_PRECONDITION(mNodeStackPos > 0, "node stack empty");

  PRInt32 result=kNoError;
  mNodeStack[--mNodeStackPos]=eHTMLTag_unknown;

#ifdef VERBOSE_DEBUG
  DebugDump("</",aNode.GetText(),(mNodeStackPos-1)*2);
#endif

  return result;
}

/**
 *  This method gets called by the parser <i>any time</i>
 *  head data gets consumed by the parser. Currently, that
 *  list includes <META>, <ISINDEX>, <LINK>, <SCRIPT>,
 *  <STYLE>, <TITLE>.
 *
 *  @updated gess 3/25/98
 *  @param   
 *  @return  
 */
PRInt32 nsHTMLContentSink::OpenHead(const nsIParserNode& aNode) {
  PRInt32 result=kNoError;
  mNodeStack[mNodeStackPos++]=(eHTMLTags)aNode.GetNodeType();

#ifdef VERBOSE_DEBUG
  DebugDump("<",aNode.GetText(),(mNodeStackPos-1)*2);
#endif

  return result;
}

/**
 *  This method gets called by the parser when a </HEAD>
 *  tag has been seen (either implicitly or explicitly).
 *
 *  @updated gess 3/25/98
 *  @param   
 *  @return  
 */
PRInt32 nsHTMLContentSink::CloseHead(const nsIParserNode& aNode) {
  NS_PRECONDITION(mNodeStackPos > 0, "node stack empty");

  PRInt32 result=kNoError;
  mNodeStack[--mNodeStackPos]=eHTMLTag_unknown;

#ifdef VERBOSE_DEBUG
  DebugDump("</",aNode.GetText(),(mNodeStackPos)*2);
#endif

  return result;
}

/**
 *  This gets called by the parser when a <TITLE> tag 
 *  gets consumed.
 *  
 *  @updated gess 3/25/98
 *  @param   
 *  @return  
 */
PRInt32 nsHTMLContentSink::SetTitle(const nsString& aValue){
  PRInt32 result=kNoError;
  mTitle=aValue;
  return result;
}

/**
 *  This method gets called by the parser when a <BODY> 
 *  tag has been consumed.
 *  
 *  @updated gess 3/25/98
 *  @param   
 *  @return  
 */
PRInt32 nsHTMLContentSink::OpenBody(const nsIParserNode& aNode) {
  PRInt32 result=kNoError;
  mNodeStack[mNodeStackPos++]=(eHTMLTags)aNode.GetNodeType();

  #ifdef VERBOSE_DEBUG
  DebugDump("<",aNode.GetText(),(mNodeStackPos-1)*2);
  #endif

  return result;
}

/**
 *  This method gets called by the parser when a </BODY> 
 *  tag has been consumed.
 *  
 *  @updated gess 3/25/98
 *  @param   
 *  @return  
 */
PRInt32 nsHTMLContentSink::CloseBody(const nsIParserNode& aNode){
  NS_PRECONDITION(mNodeStackPos > 0, "node stack empty");
  PRInt32 result=kNoError;
  mNodeStack[--mNodeStackPos]=eHTMLTag_unknown;

#ifdef VERBOSE_DEBUG
  DebugDump("</",aNode.GetText(),(mNodeStackPos)*2);
#endif

  return result;
}

/**
 *  This method gets called by the parser when a <FORM> 
 *  tag has been consumed.
 *  
 *  @updated gess 3/25/98
 *  @param   
 *  @return  
 */
PRInt32 nsHTMLContentSink::OpenForm(const nsIParserNode& aNode) {
  PRInt32 result=kNoError;
  mNodeStack[mNodeStackPos++]=(eHTMLTags)aNode.GetNodeType();

#ifdef VERBOSE_DEBUG
  DebugDump("<",aNode.GetText(),(mNodeStackPos-1)*2);
#endif

  return result;
}

/**
 *  This method gets called by the parser when a </FORM> 
 *  tag has been consumed.
 *   
 *  @updated gess 3/25/98
 *  @param   
 *  @return  
 */
PRInt32 nsHTMLContentSink::CloseForm(const nsIParserNode& aNode){
  NS_PRECONDITION(mNodeStackPos > 0, "node stack empty");

  PRInt32 result=kNoError;
  mNodeStack[--mNodeStackPos]=eHTMLTag_unknown;

#ifdef VERBOSE_DEBUG
  DebugDump("</",aNode.GetText(),(mNodeStackPos)*2);
#endif

  return result;
}

/**
 *  This method gets called by the parser when a <FRAMESET> 
 *  tag has been consumed.
 *  
 *  @updated gess 3/25/98
 *  @param   
 *  @return  
 */
PRInt32 nsHTMLContentSink::OpenFrameset(const nsIParserNode& aNode) {
  PRInt32 result=kNoError;
  mNodeStack[mNodeStackPos++]=(eHTMLTags)aNode.GetNodeType();

  #ifdef VERBOSE_DEBUG
  DebugDump("<",aNode.GetText(),(mNodeStackPos-1)*2);
  #endif

  return result;
}

/**
 *  This method gets called by the parser when a </FRAMESET> 
 *  tag has been consumed.
 *  
 *  @updated gess 3/25/98
 *  @param   
 *  @return  
 */
PRInt32 nsHTMLContentSink::CloseFrameset(const nsIParserNode& aNode){
  NS_PRECONDITION(mNodeStackPos > 0, "node stack empty");

  PRInt32 result=kNoError;
  mNodeStack[--mNodeStackPos]=eHTMLTag_unknown;

#ifdef VERBOSE_DEBUG
  DebugDump("</",aNode.GetText(),(mNodeStackPos)*2);
#endif

  return result;
}

/**
 *  This method gets called by the parser when any general
 *  type of container has been consumed and needs to be 
 *  opened. This includes things like <OL>, <Hn>, etc...
 *  
 *  @updated gess 3/25/98
 *  @param   
 *  @return  
 */
PRInt32 nsHTMLContentSink::OpenContainer(const nsIParserNode& aNode){  
  PRInt32 result=kNoError;
  mNodeStack[mNodeStackPos++]=(eHTMLTags)aNode.GetNodeType();

#ifdef VERBOSE_DEBUG
  DebugDump("<",aNode.GetText(),(mNodeStackPos-1)*2);
#endif

  return result;
}

/**
 *  This method gets called by the parser when a close
 *  container tag has been consumed and needs to be closed.
 *  
 *  @updated gess 3/25/98
 *  @param  
 *  @return 
 */
PRInt32 nsHTMLContentSink::CloseContainer(const nsIParserNode& aNode){
  NS_PRECONDITION(mNodeStackPos > 0, "node stack empty");

  PRInt32 result=kNoError;
  mNodeStack[--mNodeStackPos]=eHTMLTag_unknown;

#ifdef VERBOSE_DEBUG
  DebugDump("</",aNode.GetText(),(mNodeStackPos)*2);
#endif

  return result;
}

/**
 *  This causes the topmost container to be closed, 
 *  regardless of its type.
 *  
 *  @updated gess 3/25/98
 *  @param   
 *  @return  
 */
PRInt32 nsHTMLContentSink::CloseTopmostContainer(){
  NS_PRECONDITION(mNodeStackPos > 0, "node stack empty");
  PRInt32 result=kNoError;
  mNodeStack[mNodeStackPos--]=eHTMLTag_unknown;
  return result;
}

/**
 *  This gets called by the parser when you want to add
 *  a leaf node to the current container in the content
 *  model.
 *  
 *  @updated gess 3/25/98
 *  @param   
 *  @return  
 */
PRInt32 nsHTMLContentSink::AddLeaf(const nsIParserNode& aNode){
  PRInt32 result=kNoError;

#ifdef VERBOSE_DEBUG
  DebugDump("<",aNode.GetText(),(mNodeStackPos)*2);
#endif

  return result;
}

 /**
  * This method gets called when the parser begins the process
  * of building the content model via the content sink.
  *
  * @update 5/7/98 gess
 */     
void nsHTMLContentSink::WillBuildModel(void){
}

 /**
  * This method gets called when the parser concludes the process
  * of building the content model via the content sink.
  *
  * @update 5/7/98 gess
  */     
void nsHTMLContentSink::DidBuildModel(PRInt32 aQualityLevel){
}

/**
 * This method gets called when the parser gets i/o blocked,
 * and wants to notify the sink that it may be a while before
 * more data is available.
 *
 * @update 5/7/98 gess
 */     
void nsHTMLContentSink::WillInterrupt(void) {
}

/**
 * This method gets called when the parser i/o gets unblocked,
 * and we're about to start dumping content again to the sink.
 *
 * @update 5/7/98 gess
 */     
void nsHTMLContentSink::WillResume(void) {
}



