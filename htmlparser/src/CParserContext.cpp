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
 */


#include "CParserContext.h"
#include "nsToken.h"


class CTokenDeallocator: public nsDequeFunctor{
public:
  virtual void* operator()(void* anObject) {
    CToken* aToken = (CToken*)anObject;
    delete aToken;
    return 0;
  }
};

CTokenDeallocator gTokenDeallocator;


CParserContext::CParserContext(CScanner* aScanner,
                               CParserContext* aPreviousContext,
                               nsIStreamObserver* aListener) :
  mSourceType(),
  mTokenDeque(gTokenDeallocator)
{
  mScanner=aScanner;
  mPrevContext=aPreviousContext;
  mListener=aListener;
  NS_IF_ADDREF(mListener);
  mMajorIteration=mMinorIteration=-1;
  mParseMode=eParseMode_unknown;
  mAutoDetectStatus=eUnknownDetect;
  mTransferBuffer=new char[eTransferBufferSize+1];
  mCurrentPos=0;
  mMarkPos=0;
  mDTD=0;
}


/**
 * Destructor for parser context
 * NOTE: DO NOT destroy the dtd here.
 * @update	gess7/11/98
 */
CParserContext::~CParserContext(){

  if(mCurrentPos)
    delete mCurrentPos;
  if(mMarkPos)
    delete mMarkPos;

  if(mScanner)
    delete mScanner;

  if(mTransferBuffer)
    delete [] mTransferBuffer;

  //Remember that it's ok to simply
  //ignore the DTD and the prevcontext.

}


