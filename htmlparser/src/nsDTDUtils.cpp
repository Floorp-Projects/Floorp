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
#include "CNavDTD.h" 

static CTokenDeallocator gTokenKiller;
 
/***************************************************************
  First, define the tagstack class
 ***************************************************************/


/**
 * Default constructor
 * @update	gess9/10/98
 */
nsTagStack::nsTagStack(int aDefaultSize) {
  mCapacity=aDefaultSize;
  mCount=0;
#ifndef NS_DEBUG
  mTags =new eHTMLTags[mCapacity];
#endif
  nsCRT::zero(mTags,mCapacity*sizeof(eHTMLTags));
}

/**
 * Default destructor
 * @update  gess7/9/98
 */
nsTagStack::~nsTagStack() {
#ifndef NS_DEBUG
  delete mTags;
  mTags=0;
#endif
  mCapacity=mCount=0; 
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
  if(mCount>=mCapacity) {
#ifndef NS_DEBUG
    eHTMLTags* tmp=new eHTMLTags[2*mCapacity];
    nsCRT::zero(tmp,2*mCapacity*sizeof(eHTMLTag_html));
    nsCRT::memcpy(tmp,mTags,mCapacity*sizeof(eHTMLTag_html));
    delete mTags;
    mTags=tmp;
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
eHTMLTags nsTagStack::TagAt(PRInt32 anIndex) const {
  if((anIndex>=0) && (anIndex<mCount))
    return mTags[anIndex];
  return eHTMLTag_unknown;
}


/**
 * 
 * @update  gess7/9/98
 */
eHTMLTags nsTagStack::operator[](PRInt32 anIndex) const {
  if((anIndex>=0) && (anIndex<mCount))
    return mTags[anIndex];
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

/**
 * 
 * @update  gess7/9/98
 */
PRInt32 nsTagStack::GetTopmostIndexOf(eHTMLTags aTag) const {
  int theIndex=0;
  for(theIndex=mCount-1;theIndex>=0;theIndex--){
    if(mTags[theIndex]==aTag)
      return theIndex;
  }
  return kNotFound;
}


/***************************************************************
  Now define the dtdcontext class
 ***************************************************************/


/**
 * 
 * @update	gess9/10/98
 */
nsDTDContext::nsDTDContext(int aDefaultSize) {
#ifndef NS_DEBUG
  mStyles =new nsTagStack*[aDefaultSize];
#endif
  mOpenStyles=0;
  nsCRT::zero(mStyles,mTags.mCapacity*sizeof(void*));
} 
 

/**
 * 
 * @update	gess9/10/98
 */
nsDTDContext::~nsDTDContext() {
#ifndef NS_DEBUG
  delete mStyles;
  mStyles=0;
#endif
}

/**
 * 
 * @update  gess7/9/98 
 */
PRInt32 nsDTDContext::GetCount(void) {
  return mTags.mCount;
}

/**
 * 
 * @update  gess7/9/98 
 */
void nsDTDContext::Push(eHTMLTags aTag) {
  if(mTags.mCount>=mTags.mCapacity) {
#ifndef NS_DEBUG
    nsTagStack** tmp2=new nsTagStack*[2*mTags.mCapacity];
    nsCRT::zero(tmp2,2*mTags.mCapacity*sizeof(void*));
    nsCRT::memcpy(tmp2,mStyles,mTags.mCapacity*sizeof(void*));
    delete mStyles;
    mStyles=tmp2;
    //mCapacity*=2;
#else
    NS_PRECONDITION(mTags.mCount<nsTagStack::eStackSize,"TagStack Overflow: DEBUG VERSION!");
#endif
  }
  mTags.Push(aTag);
}

/**
 * 
 * @update  gess7/9/98
 */
eHTMLTags nsDTDContext::Pop() {
  eHTMLTags result=eHTMLTag_unknown;
  if(mTags.mCount>0) {
    result=mTags.Pop();
    mStyles[mTags.mCount]=0;
  }
  return result;
}

/**
 * 
 * @update  gess7/9/98
 */
eHTMLTags nsDTDContext::First() const {
  return mTags.First();
}

/**
 * 
 * @update  gess7/9/98
 */
eHTMLTags nsDTDContext::TagAt(PRInt32 anIndex) const {
  return mTags.TagAt(anIndex);
}


/**
 * 
 * @update  gess7/9/98
 */
eHTMLTags nsDTDContext::operator[](PRInt32 anIndex) const {
  return mTags[anIndex];
}

/**
 * 
 * @update  gess7/9/98
 */
eHTMLTags nsDTDContext::Last() const {
  return mTags.Last();
}


/**************************************************************
  Now define the tokenrecycler class...
 **************************************************************/

/**
 * 
 * @update  gess7/25/98
 * @param 
 */
CTokenRecycler::CTokenRecycler() : nsITokenRecycler() {
  int i=0;
  for(i=0;i<eToken_last-1;i++) {
    mTokenCache[i]=new nsDeque(gTokenKiller);
    //mTotals[i]=0;
  }
}

/**
 * Destructor for the token factory
 * @update  gess7/25/98
 */
CTokenRecycler::~CTokenRecycler() {
  //begin by deleting all the known (recycled) tokens...
  //We're also deleting the cache-deques themselves.
  int i;
  for(i=0;i<eToken_last-1;i++) {
    delete mTokenCache[i];
  }
}

class CTokenFinder: public nsDequeFunctor{
public:
  CTokenFinder(CToken* aToken) {mToken=aToken;}
  virtual void* operator()(void* anObject) {
    if(anObject==mToken) {
      return anObject;
    }
    return 0;
  }
  CToken* mToken;
};

/**
 * This method gets called when someone wants to recycle a token
 * @update  gess7/24/98
 * @param   aToken -- token to be recycled.
 * @return  nada
 */
void CTokenRecycler::RecycleToken(CToken* aToken) {
  if(aToken) {
    PRInt32 theType=aToken->GetTokenType();
    CTokenFinder finder(aToken);
    CToken* theMatch=(CToken*)mTokenCache[theType-1]->FirstThat(finder);
    if(theMatch) {
      int x=5;
    }
    mTokenCache[theType-1]->Push(aToken);
  }
}


/**
 * 
 * @update	vidur 11/12/98
 * @param 
 * @return
 */
CToken* CTokenRecycler::CreateTokenOfType(eHTMLTokenTypes aType,eHTMLTags aTag, const nsString& aString) {

  CToken* result=(CToken*)mTokenCache[aType-1]->Pop();

  if(result) {
    result->Reinitialize(aTag,aString);
  }
  else {
    //mTotals[aType-1]++;
    switch(aType){
      case eToken_start:      result=new CStartToken(aTag); break;
      case eToken_end:        result=new CEndToken(aTag); break;
      case eToken_comment:    result=new CCommentToken(); break;
      case eToken_attribute:  result=new CAttributeToken(); break;
      case eToken_entity:     result=new CEntityToken(); break;
      case eToken_whitespace: result=new CWhitespaceToken(); break;
      case eToken_newline:    result=new CNewlineToken(); break;
      case eToken_text:       result=new CTextToken(aString); break;
      case eToken_script:     result=new CScriptToken(); break;
      case eToken_style:      result=new CStyleToken(); break;
      case eToken_skippedcontent: result=new CSkippedContentToken(aString); break;
      case eToken_instruction:result=new CInstructionToken(); break;
      case eToken_cdatasection:result=new CCDATASectionToken(); break;
        default:
          break;
    }
  }
  return result;
}

void DebugDumpContainmentRules(nsIDTD& theDTD,const char* aFilename,const char* aTitle) {
  const char* prefix="     ";
  fstream out(aFilename,ios::out);
  out << "==================================================" << endl;
  out << aTitle << endl;
  out << "==================================================";
  int i,j=0;
  int written;
  for(i=1;i<eHTMLTag_text;i++){
    const char* tag=NS_EnumToTag((eHTMLTags)i);
    out << endl << endl << "Tag: <" << tag << ">" << endl;
    out << prefix;
    written=0;
    if(theDTD.IsContainer(i)) {
      for(j=1;j<eHTMLTag_text;j++){
        if(15==written) {
          out << endl << prefix;
          written=0;
        }
        if(theDTD.CanContain(i,j)){
          tag=NS_EnumToTag((eHTMLTags)j);
          if(tag) {
            out<< tag << ", ";
            written++;
          }
        }
      }//for
    }
    else out<<"(not container)" << endl;
  }
}


