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


#include "nsDeque.h"
#include "nsToken.h"

const PRInt32 kGrowthDelta = 10;

/**-------------------------------------------------------
 *  Standard constructor
 *
 *  @updated  	gess 28Feb98
 *  @param
 *  @return
 *------------------------------------------------------*/
CDeque::CDeque() {
  mCapacity=kGrowthDelta;
  mOrigin=mSize=0;
  mTokens=new CToken*[mCapacity];
}

/**-------------------------------------------------------
 *  Standard destructor
 *
 *  @updated  	gess 28Feb98
 *  @param
 *  @return
 *------------------------------------------------------*/
CDeque::~CDeque() {
  Erase();
  delete [] mTokens;
}

/*-------------------------------------------------------
 *  Remove all items from the queue without destroying them.
 *  @updated gess 28Feb98
 *  @param
 *  @return
 *------------------------------------------------------*/
void CDeque::Empty() {
  for(PRInt32 i=0;i<mCapacity;i++)
    mTokens[i]=0;
  mSize=0;
  mOrigin=0;
}

/*-------------------------------------------------------
 *  Remove AND DELETE! all items from the queue 
 *  @updated gess 28Feb98
 *  @param
 *  @return
 *------------------------------------------------------*/
void CDeque::Erase() {
  CDequeIterator iter1=Begin();
  CDequeIterator iter2=End();
  CToken* temp;
  while(iter1!=iter2) {
    temp=(iter1++);
    delete temp;
  }
  Empty();
}

/*-------------------------------------------------------
 *  This method adds a token to the end of the queue. 
 *  This operation has the potential to cause the 
 *  underlying buffer to resize.
 *
 *  @updated  gess 28Feb98
 *  @param    aToken: new token to be added to queue
 *  @return   nada
 *------------------------------------------------------*/
void CDeque::Push(CToken* aToken) {
  if(mSize==mCapacity) {
    CToken** temp=new CToken*[mCapacity+kGrowthDelta];

    //so here's the problem. You can't just move the elements
    //directy (in situ) from the old buffer to the new one.
    //Since capacity has changed, the old origin doesn't make
    //sense anymore. It's better to resequence the elements now.
    
    PRInt32 tempi=0;
    for(PRInt32 i=mOrigin;i<mCapacity;i++) temp[tempi++]=mTokens[i]; //copy the leading elements...
    for(PRInt32 j=0;j<mOrigin;j++) temp[tempi++]=mTokens[j];         //copy the trailing elements...
    mCapacity+=kGrowthDelta;
    mOrigin=0;       //now realign the origin...
    delete[]mTokens;
    mTokens=temp;
  }
  PRInt32 offset=mOrigin+mSize;
  if(offset<mCapacity)
    mTokens[offset]=aToken;
  else mTokens[offset-mCapacity]=aToken;
  mSize++;
}

/*-------------------------------------------------------
 *  This method gets called when a caller wants
 *  to remove the first element from the queue.
 *  Pop() never causes the underlying buffer to resize.
 *
 *  @updated gess 28Feb98
 *  @param   nada
 *  @return  first token in queue (if any) or null
 *------------------------------------------------------*/
CToken* CDeque::Pop() {
  CToken* result=0;
  if(mSize>0) {
    result=mTokens[mOrigin];
    mTokens[mOrigin++]=0;     //zero it out for debugging purposes.
    mSize--;
    if(0==mSize)
      mOrigin=0;
  }
  return result;
}

/*-------------------------------------------------------
 *  Get the ith element (in relative terms) not absolute 
 *  index pos. Don't forget the origin may be anywhere.
 *
 *  @updated gess 28Feb98
 *  @param   anIndex: 0 relative index
 *  @return  CToken* or null.
 *------------------------------------------------------*/
CToken* CDeque::ObjectAt(PRInt32 anIndex) {
  CToken* result=0;
  if((anIndex>=0) && (anIndex<mSize))
  {
    PRInt32 pos=mOrigin+anIndex;
    if(pos<mCapacity) 
      result=mTokens[pos];
    else result=mTokens[anIndex-1];
  }
  return result;
}

/*-------------------------------------------------------
 *  Create and return an iterator pointing to
 *  the beginning of the queue. Note that this
 *  takes the circular buffer semantics into account.
 *
 *  @updated  gess 28Feb98
 *  @param    none
 *  @return   a newly created CDequeIterator()
 *------------------------------------------------------*/
CDequeIterator CDeque::Begin(void) {
  return CDequeIterator(*this,0);
}

/*-------------------------------------------------------
 *  Create and return an iterator pointing to
 *  the end of the queue. Note that this takes the 
 *  circular buffer semantics into account.
 *
 *  @updated  gess 28Feb98
 *  @param    none
 *  @return   a newly created CDequeIterator()
 *------------------------------------------------------*/
CDequeIterator CDeque::End(void) {
  return CDequeIterator(*this,mSize);
}

/**-------------------------------------------------------
 *  @updated gess 28Feb98
 *  @param
 *  @return
 *------------------------------------------------------*/
void CDeque::SelfTest(void) {

#ifdef _DEBUG
  CDeque* theDeck= new CDeque();
  CToken* a=new CToken(nsAutoString("")); a->SetOrdinal(10);
  CToken* b=new CToken(nsAutoString("")); b->SetOrdinal(20);
  CToken* c=new CToken(nsAutoString("")); c->SetOrdinal(30);
  CToken* d=new CToken(nsAutoString("")); d->SetOrdinal(40);
  CToken* e=new CToken(nsAutoString("")); e->SetOrdinal(50);
  CToken* f=new CToken(nsAutoString("")); f->SetOrdinal(60);
  theDeck->Push(a);
  theDeck->Push(b);
  theDeck->Push(c);
  CToken* temp=theDeck->Pop();  //temp should == a
  temp=theDeck->Pop(); //temp should == b
  theDeck->Push(d);
  theDeck->Push(e);

  CDequeIterator iter1=theDeck->Begin();
  CDequeIterator iter2=theDeck->End();
  while(iter1!=iter2) {
    temp=(iter1++);
    cout << temp->GetOrdinal();
  }

  theDeck->Push(f); //should cause queue to resize and resequence.
  CDequeIterator iter3=theDeck->Begin();
  CDequeIterator iter4=theDeck->End();
  while(iter3!=iter4) {
    temp=(iter3++);
    cout << temp->GetOrdinal();
  }
  delete a;
  delete b;
  delete c;
  delete d;
  delete e;
  delete f;
  delete theDeck;
#endif
}

