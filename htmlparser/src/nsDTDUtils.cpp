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


#include "nsDTDUtils.h"

 
/***************************************************************
  First, define the tagstack class
 ***************************************************************/


/**
 * Default constructor
 * @update	gess9/10/98
 */
nsTagStack::nsTagStack(int aDefaultSize) {
#ifdef _dynstack
  mSize=aDefaultSize;
  mTags =new eHTMLTags[mSize];
  mBits =new PRBool[mSize];
#else
  mSize=eStackSize;
#endif
  mCount=0;
  mPrevious=0;
  nsCRT::zero(mTags,mSize*sizeof(eHTMLTags));
  nsCRT::zero(mBits,mSize*sizeof(PRBool));
}

/**
 * Default destructor
 * @update  gess7/9/98
 */
nsTagStack::~nsTagStack() {
#ifdef _dynstack
    delete mTags;
    delete mBits;
    mTags=0;
    mBits=0;
#endif
    mSize=mCount=0;
  }

/**
 * Resets state of stack to be empty.
 * @update  gess7/9/98
 */
void nsTagStack::Empty(void) {
  mCount=0;
}

/**
 * 
 * @update  gess7/9/98
 */
void nsTagStack::Push(eHTMLTags aTag) {

  if(mCount>=mSize) {
#ifdef _dynstack
      //regrow the dynamic stack... 
    eHTMLTags* tmp=new eHTMLTags[2*mSize];
    nsCRT::zero(tmp,2*mSize*sizeof(eHTMLTag_html));
    nsCRT::memcpy(tmp,mTags,mSize*sizeof(eHTMLTag_html));
    delete mTags;
    mTags=tmp;

    PRBool* tmp2=new PRBool[2*mSize];
    nsCRT::zero(tmp2,2*mSize*sizeof(PRBool));
    nsCRT::memcpy(tmp2,mBits,mSize*sizeof(PRBool));
    delete mBits;
    mBits=tmp2;
    mSize*=2;
#else
    NS_PRECONDITION(mCount<eStackSize,"TagStack Overflow: DEBUG VERSION!");
#endif
  }

  mTags[mCount++]=aTag;
}

/**
 * 
 * @update  gess7/9/98
 */
eHTMLTags nsTagStack::Pop() {
  eHTMLTags result=eHTMLTag_unknown;
  if(mCount>0) {
    result=mTags[--mCount];
    mTags[mCount]=eHTMLTag_unknown;
    mBits[mCount]=PR_FALSE;
  }
  return result;
}

/**
 * 
 * @update  gess7/9/98
 */
eHTMLTags nsTagStack::First() const {
  if(mCount>0)
    return mTags[0];
  return eHTMLTag_unknown;
}

/**
 * 
 * @update  gess7/9/98
 */
eHTMLTags nsTagStack::Last() const {
  if(mCount>0)
    return mTags[mCount-1];
  return eHTMLTag_unknown;
}


/***************************************************************
  Now define the dtdcontext class
 ***************************************************************/


/**
 * 
 * @update	gess9/10/98
 */
nsDTDContext::nsDTDContext(int aDefaultSize) : mElements(aDefaultSize) {
  mStyles=new nsTagStack(aDefaultSize);
}
 

/**
 * 
 * @update	gess9/10/98
 */
nsDTDContext::~nsDTDContext() {
  while(mStyles){
    nsTagStack* next=mStyles->mPrevious;
    delete mStyles;
    mStyles=next;
  }
}


/**
 * Creates and pushes a new style stack onto our
 * internal list of style stacks.
 * @update	gess9/10/98
 */
void nsDTDContext::pushStyleStack(nsTagStack* aStyleStack){
  if(!aStyleStack)
    aStyleStack=new nsTagStack();
  if(aStyleStack){
    mStyles->mPrevious=mStyles;
    mStyles=aStyleStack;
  }
}


/**
 * Pops the topmost style stack off our internal
 * list of style stacks.
 * @update	gess9/10/98
 */
nsTagStack* nsDTDContext::popStyleStack(){
  nsTagStack* result=0;
  if(mStyles) {
    result=mStyles;
    mStyles=mStyles->mPrevious;
  }
  return result;
}




