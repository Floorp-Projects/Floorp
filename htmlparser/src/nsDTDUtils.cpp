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

 
/***************************************************************
  First, define the tagstack class
 ***************************************************************/


/**
 * Default constructor
 * @update	harishd 04/04/99
 */
nsTagStack::nsTagStack(int aDefaultSize) {
  mTags =new nsDeque(nsnull);
}

/**
 * Default destructor
 * @update  harishd 04/04/99
 */
nsTagStack::~nsTagStack() {
  delete mTags;
}

/**
 * Resets state of stack to be empty.
 * @update harishd 04/04/99
 */
void nsTagStack::Empty(void) {
  mTags->Empty();
}

/**
 * 
 * @update  harishd 04/04/99
 */
void nsTagStack::Push(eHTMLTags aTag) {
  nsTags* result = new nsTags();
  result->mTag=aTag;
  mTags->Push(result);
}


/**
 * 
 * @update  harishd 04/04/99
 */
eHTMLTags nsTagStack::Pop() {
  eHTMLTags result=eHTMLTag_unknown;
  if(mTags->GetSize() > 0) {
    nsTags* t = (nsTags*)mTags->Pop();
    result=t->mTag;
    delete t;
  }
  return result;
}

/**
 * 
 * @update  harishd 04/04/99
 */
eHTMLTags nsTagStack::First() const {
  if(mTags->GetSize() > 0) {
    nsTags* result = (nsTags*)mTags->Peek();
    return result->mTag;
  }
  return eHTMLTag_unknown;
}

/**
 * 
 * @update  harishd 04/04/99
 */
eHTMLTags nsTagStack::TagAt(PRInt32 anIndex) const {
  if((anIndex>=0) && (anIndex<mTags->GetSize())) {
    nsTags* result = (nsTags*)mTags->ObjectAt(anIndex);
    return result->mTag; 
  }
  return eHTMLTag_unknown;

}

/**
 * 
 * @update  harishd 04/04/99
 */
eHTMLTags nsTagStack::operator[](PRInt32 anIndex) const {
  if((anIndex>=0) && (anIndex<mTags->GetSize())) {
    nsTags* result = (nsTags*)mTags->ObjectAt(anIndex);
    return result->mTag; 
  }
  return eHTMLTag_unknown;
}


/**
 * 
 * @update  harishd 04/04/99
 */
eHTMLTags nsTagStack::Last() const {
  PRInt32 size = mTags->GetSize();
  if(size > 0) {
    nsTags* result = (nsTags*)mTags->ObjectAt(size - 1);
    return result->mTag;
  }
  return eHTMLTag_unknown;

}

/**
 * 
 * @update  harishd 04/04/99
 */
PRInt32 nsTagStack::GetTopmostIndexOf(eHTMLTags aTag) const {
  int theIndex=0;
  nsTags* result;
  for(theIndex=(mTags->GetSize() - 1);theIndex>=0;theIndex--){
    result = (nsTags*)mTags->ObjectAt(theIndex);
    if(result->mTag==aTag)
      return theIndex;
  }
  return kNotFound;

}

/**
 * 
 * @update  harishd 04/04/99
 */
void nsTagStack::SaveToken(CToken* aToken, PRInt32 aID)
{ 
  NS_PRECONDITION(aID <= mTags->GetSize() && aID > -1,"Out of bounds");

  if(aToken) {
    nsTags* result = (nsTags*)mTags->ObjectAt(aID);
    result->mTokenBank->Push(aToken);
  }
}

/**
 * 
 * @update  harishd 04/04/99
 */
CToken*  nsTagStack::RestoreTokenFrom(PRInt32 aID)
{ 
  NS_PRECONDITION(aID <= mTags->GetSize() && aID > -1,"Out of bounds");

  if(mTags->GetSize() > 0) {
    nsTags* result = (nsTags*)mTags->ObjectAt(aID);
    return (CToken*)result->mTokenBank->PopFront();
  }
  return nsnull;
}

/**
 * 
 * @update  harishd 04/04/99
 */
PRInt32  nsTagStack::TokenCountAt(PRInt32 aID) 
{ 
  NS_PRECONDITION(aID <= mTags->GetSize(),"Out of bounds");
  if(aID < 0)
    return kNotFound;
  nsTags* result = (nsTags*)mTags->ObjectAt(aID);
  return result->mTokenBank->GetSize();
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
  nsCRT::zero(mStyles,aDefaultSize*sizeof(void*));
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
 * @update  gess7/9/98, harishd 04/04/99 
 */
PRInt32 nsDTDContext::GetCount(void) {
  return mTags.mTags->GetSize();
}

/**
 * 
 * @update  gess7/9/98, harishd 04/04/99
 */
void nsDTDContext::Push(eHTMLTags aTag) {
#ifndef NS_DEBUG
  NS_PRECONDITION(GetCount()<nsTagStack::eStackSize,"TagStack Overflow: DEBUG VERSION!");
  
  if(GetCount()>=nsTagStack::eStackSize) {
    nsTagStack** tmp2=new nsTagStack*[2*(nsTagStack::eStackSize)];
    nsCRT::zero(tmp2,2*(nsTagStack::eStackSize)*sizeof(void*));
    nsCRT::memcpy(tmp2,mStyles,(nsTagStack::eStackSize)*sizeof(void*));
    delete mStyles;
    mStyles=tmp2;
  }
#endif
  mTags.Push(aTag);
}

/** 
 * @update  gess7/9/98, harishd 04/04/99
 */
eHTMLTags nsDTDContext::Pop() {
  eHTMLTags result=eHTMLTag_unknown;
  PRInt32   size = GetCount();
  if(size>0) {
    result=mTags.Pop();
    size--;
    mStyles[size]=0;
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
    mTokenCache[i]=new nsDeque(new CTokenDeallocator());
#ifdef NS_DEBUG
    mTotals[i]=0;
#endif
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
    if(0!=mTokenCache[i]) {
      delete mTokenCache[i];
      mTokenCache[i]=0;
    }
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
#ifdef  NS_DEBUG
    mTotals[aType-1]++;
#endif
    switch(aType){
      case eToken_start:      result=new CStartToken(aTag); break;
      case eToken_end:        result=new CEndToken(aTag); break;
      case eToken_comment:    result=new CCommentToken(); break;
      case eToken_entity:     result=new CEntityToken(); break;
      case eToken_whitespace: result=new CWhitespaceToken(); break;
      case eToken_newline:    result=new CNewlineToken(); break;
      case eToken_text:       result=new CTextToken(aString); break;
      case eToken_attribute:  result=new CAttributeToken(); break;
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

/**
 * 
 * @update	vidur 11/12/98
 * @param 
 * @return
 */
CToken* CTokenRecycler::CreateTokenOfType(eHTMLTokenTypes aType,eHTMLTags aTag) {

  CToken* result=(CToken*)mTokenCache[aType-1]->Pop();

  static nsAutoString theEmpty;
  if(result) {
    result->Reinitialize(aTag,theEmpty);
  }
  else {
#ifdef  NS_DEBUG
    mTotals[aType-1]++;
#endif
    switch(aType){
      case eToken_start:            result=new CStartToken(aTag); break;
      case eToken_end:              result=new CEndToken(aTag); break;
      case eToken_comment:          result=new CCommentToken(); break;
      case eToken_attribute:        result=new CAttributeToken(); break;
      case eToken_entity:           result=new CEntityToken(); break;
      case eToken_whitespace:       result=new CWhitespaceToken(); break;
      case eToken_newline:          result=new CNewlineToken(); break;
      case eToken_text:             result=new CTextToken(theEmpty); break;
      case eToken_script:           result=new CScriptToken(); break;
      case eToken_style:            result=new CStyleToken(); break;
      case eToken_skippedcontent:   result=new CSkippedContentToken(theEmpty); break;
      case eToken_instruction:      result=new CInstructionToken(); break;
      case eToken_cdatasection:     result=new CCDATASectionToken(); break;
      case eToken_error:            result=new CErrorToken(); break;
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


/*************************************************************************
 *  The table lookup technique was adapted from the algorithm described  *
 *  by Avram Perez, Byte-wise CRC Calculations, IEEE Micro 3, 40 (1983). *
 *************************************************************************/

#define POLYNOMIAL 0x04c11db7L

static PRUint32 crc_table[256];

static
void gen_crc_table() {
 /* generate the table of CRC remainders for all possible bytes */
  int i, j;  
  PRUint32 crc_accum;
  for ( i = 0;  i < 256;  i++ ) { 
    crc_accum = ( (unsigned long) i << 24 );
    for ( j = 0;  j < 8;  j++ ) { 
      if ( crc_accum & 0x80000000L )
        crc_accum = ( crc_accum << 1 ) ^ POLYNOMIAL;
      else crc_accum = ( crc_accum << 1 ); 
    }
    crc_table[i] = crc_accum; 
  }
  return; 
}

class CRCInitializer {
  public: 
    CRCInitializer() {
      gen_crc_table();
    }
};
CRCInitializer gCRCInitializer;


PRUint32 AccumulateCRC(PRUint32 crc_accum, char *data_blk_ptr, int data_blk_size)  {
 /* update the CRC on the data block one byte at a time */
  int i, j;
  for ( j = 0;  j < data_blk_size;  j++ ) { 
    i = ( (int) ( crc_accum >> 24) ^ *data_blk_ptr++ ) & 0xff;
    crc_accum = ( crc_accum << 8 ) ^ crc_table[i]; 
  }
  return crc_accum; 
}

/******************************************************************************
  This class is used to store ref's to tag observers during the parse phase.
  Note that for simplicity, this is a singleton that is constructed in the
  CNavDTD and shared for the duration of the application session. Later on it
  might be nice to use a more dynamic approach that would permit observers to
  come and go on a document basis.
 ******************************************************************************/

CObserverDictionary::CObserverDictionary() {
  nsCRT::zero(mObservers,sizeof(mObservers));
  RegisterObservers();
}

CObserverDictionary::~CObserverDictionary() {
  UnregisterObservers();
}

void CObserverDictionary::UnregisterObservers() {
  int theIndex=0;
  for(theIndex=0;theIndex<NS_HTML_TAG_MAX;theIndex++){
    if(mObservers[theIndex]){
      /*
      nsIObserver* theObserver=0;
      while(theObserver=mObserver[theIndex]->Pop()){
        NS_RELEASE(theObserver);
      }
      */
    }
  }
}

void CObserverDictionary::RegisterObservers() {
  /*
    nsIObserverService* theObserverService=GetService("observer"); //or whatever the call is here...
    if(theObserverService){
      nsIObserverEnumerator* theEnum=theObserverService->GetObserversForTopic("htmlparser"); //again, put the real call here!
      if(theEnum){
        nsIObserver* theObserver=theEnum->First();
        while(theObserver){
          const char* theTagStr=theObserver->GetTag();
          if(theTagStr){
            eHTMLTags theTag=NS_TagToEnum(theTagStr);
            if(eHTMLTag_userdefined!=theTag){
              nsDeque* theDeque=mObservers[theTag];
              if(theDeque){
                NS_ADDREF(theObserver);
                theDeque->Push(theObserver);
              }
            }
          }
          theObserver=theEnum->Next();
        }
      }
    }
  */
}

nsDeque* CObserverDictionary::GetObserversForTag(eHTMLTags aTag) {
  nsDeque* result=mObservers[aTag];
  return result;
}
