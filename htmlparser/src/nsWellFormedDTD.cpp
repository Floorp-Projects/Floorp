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
 * @update  gess 4/8/98
 * 
 *         
 */

/**
 * TRANSIENT STYLE-HANDLING NOTES:
 * @update  gess 6/15/98
 * 
 * ...add comments here about transient style stack.
 *         
   */

#include "nsIDTDDebug.h"
#include "nsWellFormedDTD.h"
#include "nsCRT.h"
#include "nsParser.h"
#include "nsScanner.h"
#include "nsParserTypes.h"
#include "nsTokenHandler.h"

#include "prenv.h"  //this is here for debug reasons...
#include "prtypes.h"  //this is here for debug reasons...
#include "prio.h"
#include "plstr.h"

#ifdef XP_PC
#include <direct.h> //this is here for debug reasons...
#endif
#include "prmem.h"


static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);                 
static NS_DEFINE_IID(kIDTDIID,      NS_IDTD_IID);
static NS_DEFINE_IID(kClassIID,     NS_WELLFORMED_DTD_IID); 


static const char* kNullURL = "Error: Null URL given";
static const char* kNullFilename= "Error: Null filename given";
static const char* kNullTokenizer = "Error: Unable to construct tokenizer";
static const char* kNullToken = "Error: Null token given";
static const char* kInvalidTagStackPos = "Error: invalid tag stack position";
static const char* kRTFTextContentType = "text/xml";

static nsAutoString gEmpty;


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
nsresult CWellFormedDTD::QueryInterface(const nsIID& aIID, void** aInstancePtr)  
{                                                                        
  if (NULL == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      

  if(aIID.Equals(kISupportsIID))    {  //do IUnknown...
    *aInstancePtr = (nsIDTD*)(this);                                        
  }
  else if(aIID.Equals(kIDTDIID)) {  //do IParser base class...
    *aInstancePtr = (nsIDTD*)(this);                                        
  }
  else if(aIID.Equals(kClassIID)) {  //do this class...
    *aInstancePtr = (CWellFormedDTD*)(this);                                        
  }                 
  else {
    *aInstancePtr=0;
    return NS_NOINTERFACE;
  }
  ((nsISupports*) *aInstancePtr)->AddRef();
  return NS_OK;                                                        
}

/**
 *  This method is defined in nsIParser. It is used to 
 *  cause the COM-like construction of an nsParser.
 *  
 *  @update  gess 4/8/98
 *  @param   nsIParser** ptr to newly instantiated parser
 *  @return  NS_xxx error result
 */
NS_HTMLPARS nsresult NS_NewWellFormed_DTD(nsIDTD** aInstancePtrResult)
{
  CWellFormedDTD* it = new CWellFormedDTD();

  if (it == 0) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return it->QueryInterface(kClassIID, (void **) aInstancePtrResult);
}


NS_IMPL_ADDREF(CWellFormedDTD)
NS_IMPL_RELEASE(CWellFormedDTD)


/**
 *  Default constructor
 *  
 *  @update  gess 4/9/98
 *  @param   
 *  @return  
 */
CWellFormedDTD::CWellFormedDTD() : nsIDTD() {
  NS_INIT_REFCNT();
  mParser=0;
  mFilename=0;
}

/**
 *  Default destructor
 *  
 *  @update  gess 4/9/98
 *  @param   
 *  @return  
 */
CWellFormedDTD::~CWellFormedDTD(){
}

/**
 * This method is called to determine if the given DTD can parse
 * a document in a given source-type. 
 * NOTE: Parsing always assumes that the end result will involve
 *       storing the result in the main content model.
 * @update	gess6/24/98
 * @param   
 * @return  TRUE if this DTD can satisfy the request; FALSE otherwise.
 */
PRBool CWellFormedDTD::CanParse(nsString& aContentType, PRInt32 aVersion){
  PRBool result=aContentType.Equals(kRTFTextContentType);
  return result;
}


/**
 * 
 * @update	gess7/7/98
 * @param 
 * @return
 */
eAutoDetectResult CWellFormedDTD::AutoDetectContentType(nsString& aBuffer,nsString& aType){
  eAutoDetectResult result=eUnknownDetect;
  return result;
}

/**
 * 
 * @update	gess5/18/98
 * @param 
 * @return
 */
PRInt32 CWellFormedDTD::WillBuildModel(nsString& aFilename){
  PRInt32 result=0;
  return result;
}

/**
 * 
 * @update	gess5/18/98
 * @param 
 * @return
 */
PRInt32 CWellFormedDTD::DidBuildModel(PRInt32 anErrorCode){
  PRInt32 result=0;

  return result;
}

/**
 * 
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return 
 */
void CWellFormedDTD::SetParser(nsIParser* aParser) {
  mParser=(nsParser*)aParser;
}

/**
 *  This method gets called in order to set the content
 *  sink for this parser to dump nodes to.
 *  
 *  @update  gess 3/25/98
 *  @param   nsIContentSink interface for node receiver
 *  @return  
 */
nsIContentSink* CWellFormedDTD::SetContentSink(nsIContentSink* aSink) {
  return 0;
}


/*******************************************************************
  These methods use to be hidden in the tokenizer-delegate. 
  That file merged with the DTD, since the separation wasn't really
  buying us anything.
 *******************************************************************/

/**
 *  This method repeatedly called by the tokenizer. 
 *  Each time, we determine the kind of token were about to 
 *  read, and then we call the appropriate method to handle
 *  that token type.
 *  
 *  @update gess 3/25/98
 *  @param  aChar: last char read
 *  @param  aScanner: see nsScanner.h
 *  @param  anErrorCode: arg that will hold error condition
 *  @return new token or null 
 */
PRInt32 CWellFormedDTD::ConsumeToken(CToken*& aToken){
  
  CScanner* theScanner=mParser->GetScanner();

  PRUnichar aChar;
  PRInt32   result=theScanner->GetChar(aChar);

  switch(result) {
    case kEOF:
      break;

    case kInterrupted:
      theScanner->RewindToMark();
      break; 

    case kNoError:
    default:
      switch(aChar) {
        case kLessThan:
        default:
          break;
      } //switch
      break; 
  } //switch
  if(kNoError==result)
    result=theScanner->Eof();
  return result;
}

/**
 * 
 * @update	gess5/18/98
 * @param 
 * @return
 */
void CWellFormedDTD::WillResumeParse(void){
  return;
}

/**
 * 
 * @update	gess5/18/98
 * @param 
 * @return
 */
void CWellFormedDTD::WillInterruptParse(void){
  return;
}

/**
 * 
 * @update	jevering6/23/98
 * @param 
 * @return
 */
void CWellFormedDTD::SetDTDDebug(nsIDTDDebug * aDTDDebug) {
}

/**
 *  This method is called to determine whether or not a tag
 *  of one type can contain a tag of another type.
 *  
 *  @update  gess 3/25/98
 *  @param   aParent -- int tag of parent container
 *  @param   aChild -- int tag of child container
 *  @return  PR_TRUE if parent can contain child
 */
PRBool CWellFormedDTD::CanContain(PRInt32 aParent,PRInt32 aChild){
  PRBool result=PR_FALSE;
  return result;
}

/**
 *  
 *  @update  gess 3/25/98
 *  @param   aToken -- token object to be put into content model
 *  @return  0 if all is well; non-zero is an error
 */
PRInt32 CWellFormedDTD::HandleToken(CToken* aToken) {
  PRInt32 result=0;
  return result;
}

