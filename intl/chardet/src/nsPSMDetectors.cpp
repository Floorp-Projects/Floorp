/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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

#include "nsVerifier.h"
#include "nsSJISVerifier.h"
#include "nsEUCJPVerifier.h"
#include "nsCP1252Verifier.h"
#include "nsUTF8Verifier.h"
#include "nsISO2022JPVerifier.h"
#include <stdio.h>
//---- for XPCOM
#include "nsIFactory.h"
#include "nsISupports.h"
#include "nsCharDetDll.h"
#include "pratom.h"
//---- end XPCOM
//---- for XPCOM interfaces
#include "nsICharsetDetector.h"
#include "nsICharsetDetectionObserver.h"
#include "nsIStringCharsetDetector.h"
//---- end XPCOM interfaces

#define DETECTOR_DEBUG

//==========================================================
class nsPSMDetectorBase {
public :
   nsPSMDetectorBase();
   virtual ~nsPSMDetectorBase() {};

   virtual PRBool HandleData(const char* aBuf, PRUint32 aLen);
   virtual void   DataEnd();
 
protected:
   virtual void Report(const char* charset);

   PRUint32 mItems;
   PRUint32* mState;
   PRUint32* mItemIdx;
   nsVerifier** mVerifier;
   PRBool mDone;

private:
#ifdef DETECTOR_DEBUG
   PRUint32 mDbgLen;
#endif
};
//----------------------------------------------------------
nsPSMDetectorBase::nsPSMDetectorBase()
{
  mDone= PR_FALSE;
  mItems = 0;
#ifdef DETECTOR_DEBUG
  mDbgLen = 0;
#endif
}
//----------------------------------------------------------
void nsPSMDetectorBase::Report(const char* charset)
{
  printf("%s\n", charset);
}
//----------------------------------------------------------
void nsPSMDetectorBase::DataEnd()
{
}
//----------------------------------------------------------
PRBool nsPSMDetectorBase::HandleData(const char* aBuf, PRUint32 aLen)
{
  PRUint32 i,j;
  PRUint32 st;
  for(i=0; i < aLen; i++)
  {
     char b = aBuf[i];
     for(j = 0; j < mItems; )
     {
        st = GETNEXTSTATE( mVerifier[mItemIdx[j]], b, mState[j] );
        if(eItsMe == st) 
        {
#ifdef DETECTOR_DEBUG
            printf("It's %s- byte %d\n", mVerifier[mItemIdx[j]]->charset,i+mDbgLen);
#endif
            Report( mVerifier[mItemIdx[j]]->charset);
            mDone = mDone;
            return PR_TRUE;
        } else if (eError == st) 
        {
#ifdef DETECTOR_DEBUG
            printf("It's NOT %s- byte %d\n", mVerifier[mItemIdx[j]]->charset,i+mDbgLen);
#endif
            mItems--;
            if(j < mItems )
            {
                mItemIdx[j] = mItemIdx[mItems];
                mState[j] = mState[mItems];
            } 
        } else {
            mState[j++] = st;
        } 
        if( mItems <= 1) 
        {
            if( 1 == mItems) {
#ifdef DETECTOR_DEBUG
                printf("It's %s- byte %d. The only left\n", mVerifier[mItemIdx[j]]->charset,i+mDbgLen);
#endif
                Report( mVerifier[mItemIdx[0]]->charset);
            }
            mDone = PR_TRUE;
            return mDone;
        }
     }
  }
#ifdef DETECTOR_DEBUG
  mDbgLen += aLen;
#endif
  return PR_FALSE;
}

//==========================================================
class nsJaDetector : public nsPSMDetectorBase {
public:
  nsJaDetector();
  virtual ~nsJaDetector() {};
private:
  PRUint32 mStateP[5];
  PRUint32 mItemIdxP[5];
};
//----------------------------------------------------------

static nsVerifier *gJaVerifierSet[5] = {
      &nsSJISVerifier,
      &nsEUCJPVerifier,
      &nsISO2022JPVerifier,
      &nsCP1252Verifier,
      &nsUTF8Verifier
};
//----------------------------------------------------------
nsJaDetector::nsJaDetector() 
{
  mState = mStateP; 
  mItemIdx = mItemIdxP; 
  mVerifier = gJaVerifierSet; 
  mItems = 5;
  for(PRUint32 i = 0; i < 5 ; i++) {
     mItemIdx[i] = i;
     mState[i] = 0;
  }
}
//==========================================================
class nsXPCOMDetectorBase : 
      public nsPSMDetectorBase , // 
      public nsICharsetDetector // Implement the interface 
{
public:
    nsXPCOMDetectorBase();
    virtual ~nsXPCOMDetectorBase();
  NS_IMETHOD Init(nsICharsetDetectionObserver* aObserver);
  NS_IMETHOD DoIt(const char* aBuf, PRUint32 aLen, PRBool* oDontFeedMe);
  NS_IMETHOD Done();
protected:
  virtual void Report(const char* charset);
private:
  nsICharsetDetectionObserver* mObserver;
};
//----------------------------------------------------------
nsXPCOMDetectorBase::nsXPCOMDetectorBase()
{
  mObserver = nsnull;
}
//----------------------------------------------------------
nsXPCOMDetectorBase::~nsXPCOMDetectorBase()
{
  NS_IF_RELEASE(mObserver);
}
//----------------------------------------------------------
NS_IMETHODIMP nsXPCOMDetectorBase::Init(
  nsICharsetDetectionObserver* aObserver)
{
  NS_ASSERTION(mObserver == nsnull , "Init twice");
  if(nsnull == aObserver)
     return NS_ERROR_ILLEGAL_VALUE;

  NS_IF_ADDREF(aObserver);
  mObserver = aObserver;
  return NS_OK;
}
//----------------------------------------------------------
NS_IMETHODIMP nsXPCOMDetectorBase::DoIt(
  const char* aBuf, PRUint32 aLen, PRBool* oDontFeedMe)
{
  NS_ASSERTION(mObserver != nsnull , "have not init yet");
  NS_ASSERTION(mDone == PR_FALSE , "don't call DoIt if we return PR_TRUE in oDon");

  if((nsnull == aBuf) || (nsnull == oDontFeedMe))
     return NS_ERROR_ILLEGAL_VALUE;

  this->HandleData(aBuf, aLen);
  *oDontFeedMe = mDone;
  return NS_OK;
}
//----------------------------------------------------------
NS_IMETHODIMP nsXPCOMDetectorBase::Done()
{
  NS_ASSERTION(mObserver != nsnull , "have not init yet");
  return NS_OK;
}
//----------------------------------------------------------
void nsXPCOMDetectorBase::Report(const char* charset)
{
  mObserver->Notify(charset, eSureAnswer);
}
//==========================================================
class nsXPCOMStringDetectorBase : 
      public nsPSMDetectorBase , // 
      public nsIStringCharsetDetector // Implement the interface 
{
public:
    nsXPCOMStringDetectorBase();
    virtual ~nsXPCOMStringDetectorBase();
    NS_IMETHOD DoIt(const char* aBuf, PRUint32 aLen, 
                   const char** oCharset, 
                   nsDetectionConfident &oConfident);
protected:
  virtual void Report(const char* charset);
private:
  const char* mResult;
};
//----------------------------------------------------------
nsXPCOMStringDetectorBase::nsXPCOMStringDetectorBase()
{
}
//----------------------------------------------------------
nsXPCOMStringDetectorBase::~nsXPCOMStringDetectorBase()
{
}
//----------------------------------------------------------
void nsXPCOMStringDetectorBase::Report(const char* charset)
{
  mResult = charset;
}
//----------------------------------------------------------
NS_IMETHODIMP nsXPCOMStringDetectorBase::DoIt(const char* aBuf, PRUint32 aLen, 
                   const char** oCharset, 
                   nsDetectionConfident &oConfident)
{
  mResult = nsnull;
  this->HandleData(aBuf, aLen);

  if( nsnull == mResult) {
     // If we have no result and detector is done - answer no match
     if(mDone) 
     {
        *oCharset = nsnull;
        oConfident = eNoAnswerMatch;
     } else {
        // if we have no answer force the Done method and find the answer
        // if we find one, return it as eBestAnswer
        this->DataEnd();
        *oCharset = mResult;
        oConfident = (mResult) ? eBestAnswer : eNoAnswerMatch ;
     }
  } else {
     // If we have answer, return as eSureAnswer
     *oCharset = mResult;
     oConfident = eSureAnswer;
  }
  return NS_OK;
}
//==========================================================
class nsXPCOMJaDetector :
        public nsXPCOMDetectorBase, // Inheriate the implementation
        public nsJaDetector         // Inheriate the implementation
{
  NS_DECL_ISUPPORTS
  public:
    nsXPCOMJaDetector();
    virtual ~nsXPCOMJaDetector();
};
//----------------------------------------------------------
NS_IMPL_ISUPPORTS(nsXPCOMJaDetector, nsICharsetDetector::GetIID())

//----------------------------------------------------------
nsXPCOMJaDetector::nsXPCOMJaDetector()
{
  NS_INIT_REFCNT();
  PR_AtomicIncrement(&g_InstanceCount);
}
//----------------------------------------------------------
nsXPCOMJaDetector::~nsXPCOMJaDetector()
{
  PR_AtomicDecrement(&g_InstanceCount);
}
//==========================================================
class nsXPCOMJaStringDetector :
        public nsXPCOMStringDetectorBase, // Inheriate the implementation
        public nsJaDetector               // Inheriate the implementation
{
  NS_DECL_ISUPPORTS
  public:
    nsXPCOMJaStringDetector();
    virtual ~nsXPCOMJaStringDetector();
};
//----------------------------------------------------------
NS_IMPL_ISUPPORTS(nsXPCOMJaStringDetector, nsIStringCharsetDetector::GetIID())

//----------------------------------------------------------
nsXPCOMJaStringDetector::nsXPCOMJaStringDetector()
{
  NS_INIT_REFCNT();
  PR_AtomicIncrement(&g_InstanceCount);
}
//----------------------------------------------------------
nsXPCOMJaStringDetector::~nsXPCOMJaStringDetector()
{
  PR_AtomicDecrement(&g_InstanceCount);
}
//==========================================================
typedef enum {
  eJaDetector,
  eJaStringDetector
} nsXPCOMDetectorSel;
//==========================================================

class nsXPCOMDetectorFactory : public nsIFactory {
   NS_DECL_ISUPPORTS

public:
   nsXPCOMDetectorFactory(nsXPCOMDetectorSel aSel) {
     NS_INIT_REFCNT();
     mSel = aSel;
     PR_AtomicIncrement(&g_InstanceCount);
   }
   virtual ~nsXPCOMDetectorFactory() {
     PR_AtomicDecrement(&g_InstanceCount);
   }

   NS_IMETHOD CreateInstance(nsISupports* aDelegate, const nsIID& aIID, void** aResult);
   NS_IMETHOD LockFactory(PRBool aLock);
private:
   nsXPCOMDetectorSel mSel;

};

//--------------------------------------------------------------
NS_DEFINE_IID( kIFactoryIID, NS_IFACTORY_IID);
NS_IMPL_ISUPPORTS( nsXPCOMDetectorFactory , kIFactoryIID);

NS_IMETHODIMP nsXPCOMDetectorFactory::CreateInstance(
    nsISupports* aDelegate, const nsIID &aIID, void** aResult)
{
  if(NULL == aResult)
        return NS_ERROR_NULL_POINTER;
  if(NULL != aDelegate)
        return NS_ERROR_NO_AGGREGATION;

  *aResult = NULL;

  nsISupports *inst = nsnull;
  switch(mSel)
  {
    case eJaDetector:
      inst = new nsXPCOMJaDetector();
      break;
    case eJaStringDetector:
      inst = new nsXPCOMJaStringDetector();
      break;
    default:
       return NS_ERROR_NOT_IMPLEMENTED;
  } 
  if(NULL == inst) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  nsresult res =inst->QueryInterface(aIID, aResult);
  if(NS_FAILED(res)) {
     delete inst;
  }

  return res;
}
//--------------------------------------------------------------
NS_IMETHODIMP nsXPCOMDetectorFactory::LockFactory(PRBool aLock)
{
  if(aLock)
     PR_AtomicIncrement( &g_LockCount );
  else
     PR_AtomicDecrement( &g_LockCount );
  return NS_OK;
}
//--------------------------------------------------------------
nsIFactory* NEW_JA_PSMDETECTOR_FACTORY() {
  return new nsXPCOMDetectorFactory(eJaDetector);
};

nsIFactory* NEW_JA_STRING_PSMDETECTOR_FACTORY() {
  return new nsXPCOMDetectorFactory(eJaStringDetector);
};


       
