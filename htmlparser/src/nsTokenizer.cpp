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


#include <fstream.h>
#include "nsTokenizer.h"
#include "nsToken.h"
#include "nsScanner.h"
#include "nsIURL.h"


/**
 *  Default constructor
 *  
 *  @update gess 3/25/98
 *  @param  aFilename -- name of file to be tokenized
 *  @param  aDelegate -- ref to delegate to be used to tokenize
 *  @return 
 */
CTokenizer::CTokenizer(nsIURL* aURL,ITokenizerDelegate* aDelegate,eParseMode aMode) :
  mTokenDeque() {
  mDelegate=aDelegate;
  mScanner=new CScanner(aURL,aMode);
  mParseMode=aMode;
}

/**
 *  Default constructor
 *  
 *  @update gess 3/25/98
 *  @param  aFilename -- name of file to be tokenized
 *  @param  aDelegate -- ref to delegate to be used to tokenize
 *  @return 
 */
CTokenizer::CTokenizer(const char* aFilename,ITokenizerDelegate* aDelegate,eParseMode aMode) :
  mTokenDeque() {
  mDelegate=aDelegate;
  mScanner=new CScanner(aFilename,aMode);
  mParseMode=aMode;
}

/**
 *  Default constructor
 *  
 *  @update gess 3/25/98
 *  @param  aFilename -- name of file to be tokenized
 *  @param  aDelegate -- ref to delegate to be used to tokenize
 *  @return 
 */
CTokenizer::CTokenizer(ITokenizerDelegate* aDelegate,eParseMode aMode) :
  mTokenDeque() {
  mDelegate=aDelegate;
  mScanner=new CScanner(aMode);
  mParseMode=aMode;
}

/**
 *  default destructor
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
CTokenizer::~CTokenizer() {
  delete mScanner;
  delete mDelegate;
  mScanner=0;
}


/**
 *  
 *  
 *  @update  gess 5/13/98
 *  @param   
 *  @return  
 */
PRBool CTokenizer::Append(nsString& aBuffer) {
  if(mScanner)
    return mScanner->Append(aBuffer);
  return PR_FALSE;
}


/**
 *  
 *  
 *  @update  gess 5/21/98
 *  @param   
 *  @return  
 */
PRBool CTokenizer::Append(const char* aBuffer, PRInt32 aLen){
  if(mScanner)
    return mScanner->Append(aBuffer,aLen);
  return PR_FALSE;
}

/**
 * Retrieve a reference to the internal token deque.
 *
 * @update  gess 4/20/98
 * @return  deque reference
 */
nsDeque& CTokenizer::GetDeque(void) {
  return mTokenDeque;
}

/**
 *  Cause the tokenizer to consume the next token, and 
 *  return an error result.
 *  
 *  @update  gess 3/25/98
 *  @param   anError -- ref to error code
 *  @return  new token or null
 */
PRInt32 CTokenizer::GetToken(CToken*& aToken) {
  PRInt32 result=mDelegate->GetToken(*mScanner,aToken);
  return result;
}

/**
 *  Retrieve the number of elements in the deque
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  int containing element count
 */
PRInt32 CTokenizer::GetSize(void) {
  return mTokenDeque.GetSize();
}


/**
 *  Part of the code sandwich, this gets called right before
 *  the tokenization process begins. The main reason for
 *  this call is to allow the delegate to do initialization.
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  TRUE if it's ok to proceed
 */
PRBool CTokenizer::WillTokenize(PRBool aIncremental){
  PRBool result=PR_TRUE;
  result=mDelegate->WillTokenize(aIncremental);
  return result;
}

/**
 *  
 *  @update  gess 3/25/98
 *  @return  TRUE if it's ok to proceed
 */
PRInt32 CTokenizer::Tokenize(nsString& aSourceBuffer,PRBool appendTokens){
  CToken* theToken=0;
  PRInt32 result=kNoError;
  
  WillTokenize(PR_TRUE);

  while(kNoError==result) {
    result=GetToken(theToken);
    if(theToken && (kNoError==result)) {

#ifdef VERBOSE_DEBUG
        theToken->DebugDumpToken(cout);
#endif
      if(mDelegate->WillAddToken(*theToken)) {
        mTokenDeque.Push(theToken);
      }
    }
  } 
  if(kEOF==result)
    result=kNoError;
  DidTokenize(PR_TRUE);
  return result;
}

/**
 *  This is the primary control routine. It iteratively
 *  consumes tokens until an error occurs or you run out
 *  of data.
 *  
 *  @update  gess 3/25/98
 *  @return  error code 
 */
PRInt32 CTokenizer::Tokenize(int anIteration) {
  CToken* theToken=0;
  PRInt32 result=kNoError;
  PRBool  done=(0==anIteration) ? (!WillTokenize(PR_TRUE)) : PR_FALSE;
  

  while((PR_FALSE==done) && (kNoError==result)) {
    mScanner->Mark();
    result=GetToken(theToken);
    if(kNoError==result) {
      if(theToken) {

  #ifdef VERBOSE_DEBUG
          theToken->DebugDumpToken(cout);
  #endif

        if(mDelegate->WillAddToken(*theToken)) {
          mTokenDeque.Push(theToken);
        }
      }

    }
    else {
      if(theToken)
        delete theToken;
      mScanner->RewindToMark();
    }
  } 
  if((PR_TRUE==done)  && (kInterrupted!=result))
    DidTokenize(PR_TRUE);
  return result;
}

/**
 *  This is the tail-end of the code sandwich for the
 *  tokenization process. It gets called once tokenziation
 *  has completed.
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  TRUE if all went well
 */
PRBool CTokenizer::DidTokenize(PRBool aIncremental) {
  PRBool result=mDelegate->DidTokenize(aIncremental);

#ifdef VERBOSE_DEBUG
    DebugDumpTokens(cout);
#endif

  return result;
}

/**
 *  This debug routine is used to cause the tokenizer to
 *  iterate its token list, asking each token to dump its
 *  contents to the given output stream.
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
void CTokenizer::DebugDumpTokens(ostream& out) {
  nsDequeIterator b=mTokenDeque.Begin();
  nsDequeIterator e=mTokenDeque.End();

  CToken* theToken;
  while(b!=e) {
    theToken=(CToken*)(b++);
    theToken->DebugDumpToken(out);
  }
}


/**
 *  This debug routine is used to cause the tokenizer to
 *  iterate its token list, asking each token to dump its
 *  contents to the given output stream.
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
void CTokenizer::DebugDumpSource(ostream& out) {
  nsDequeIterator b=mTokenDeque.Begin();
  nsDequeIterator e=mTokenDeque.End();

  CToken* theToken;
  while(b!=e) {
    theToken=(CToken*)(b++);
    theToken->DebugDumpSource(out);
  }

}


/**
 *  
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
void CTokenizer::SelfTest(void) {
#ifdef _DEBUG
#endif
}


