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
 * The easy queue is a very small, very efficient ring-
 * buffer than can hold elements of type CToken*, offering
 * the following features:
 *    It's interface supports pushing and poping of children.
 *    It can iterate (via an interator class) it's children.
 *    When full, it can efficently resize dynamically.
 *
 * Note that the only bit of trickery here is that like
 * all ring buffers, the first element may not be at index[0].
 * The mOrigin member determines where the first child is.
 * This point is quietly hidden from customers of this class.
 *    
 * If we have an existing general-purpose queue, then it
 * should be trivial to substitute that implementation for
 * this one. If not, this can easily be made into a general
 * purpose container (substitue void* for CToken*).
 */


#ifndef QUEUE
#define QUEUE

#include "nsParserTypes.h"
#include "prtypes.h"

class CToken;
class CDequeIterator;

class CDeque {
  public:
                       CDeque();
                      ~CDeque();
            
            PRInt32   GetSize() const {return mSize;}
            void      Push(CToken* aToken);
            CToken*   Pop(void);
            CToken*   PopBack(void);
            CToken*   ObjectAt(int anIndex);
            void      Empty();
            void      Erase();

            CDequeIterator Begin();
            CDequeIterator End();

            static void SelfTest();

  protected:
            PRInt32   mSize;
            PRInt32   mCapacity;
            PRInt32   mOrigin;
            CToken**  mTokens;


  private: 
            CDeque(const CDeque& other);
            CDeque& operator=(const CDeque& other);
};

class CDequeIterator {
  public:
                      CDequeIterator(CDeque& aQueue,int anIndex=0) : mDeque(aQueue) {mIndex=anIndex; }
                      CDequeIterator(const CDequeIterator& aCopy) : mDeque(aCopy.mDeque), mIndex(aCopy.mIndex) { }

      CDequeIterator& operator=(CDequeIterator& aCopy) {
                        //queue's are already equal.
                        mIndex=aCopy.mIndex;
                        return *this;
                      }

        PRBool        operator!=(CDequeIterator& anIter) {return PRBool(!this->operator==(anIter));}
        PRBool        operator==(CDequeIterator& anIter) {return PRBool(((mIndex==anIter.mIndex) && (&mDeque==&anIter.mDeque)));}
        CToken*       operator++()    {return mDeque.ObjectAt(++mIndex);}
        CToken*       operator++(int) {return mDeque.ObjectAt(mIndex++);}
        CToken*       operator--()    {return mDeque.ObjectAt(--mIndex);}
        CToken*       operator--(int) {return mDeque.ObjectAt(mIndex--);}
        CToken*       operator*()     {return mDeque.ObjectAt(mIndex);}
                      operator CToken*() {return mDeque.ObjectAt(mIndex);}


  protected:
        PRInt32       mIndex;
        CDeque&       mDeque;
};


#endif
