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
//---- for verifiers
#include "nsSJISVerifier.h"
#include "nsEUCJPVerifier.h"
#include "nsCP1252Verifier.h"
#include "nsUTF8Verifier.h"
#include "nsISO2022JPVerifier.h"
#include "nsISO2022KRVerifier.h"
#include "nsISO2022CNVerifier.h"
#include "nsHZVerifier.h"
#include "nsUCS2BEVerifier.h"
#include "nsUCS2LEVerifier.h"
#include "nsBIG5Verifier.h"
#include "nsGB2312Verifier.h"
#include "nsEUCTWVerifier.h"
#include "nsEUCKRVerifier.h"
//---- end verifiers
#include <stdio.h>
//---- for XPCOM
#include "nsIFactory.h"
#include "nsISupports.h"
#include "nsCharDetDll.h"
#include "pratom.h"
// temp fix for XPCOM should be remove after alechf fix the xpcom one
#define MY_NS_IMPL_QUERY_INTERFACE(_class,_classiiddef,_interface)       \
NS_IMETHODIMP _class::QueryInterface(REFNSIID aIID, void** aInstancePtr) \
{                                                                        \
  if (NULL == aInstancePtr) {                                            \
    return NS_ERROR_NULL_POINTER;                                        \
  }                                                                      \
                                                                         \
  *aInstancePtr = NULL;                                                  \
                                                                         \
  static NS_DEFINE_IID(kClassIID, _classiiddef);                         \
  if (aIID.Equals(kClassIID)) {                                          \
    *aInstancePtr = (void*) ((_interface*)this);                         \
    NS_ADDREF_THIS();                                                    \
    return NS_OK;                                                        \
  }                                                                      \
  if (aIID.Equals(nsCOMTypeInfo<nsISupports>::GetIID())) {               \
    *aInstancePtr = (void*) ((nsISupports*)this);                        \
    NS_ADDREF_THIS();                                                    \
    return NS_OK;                                                        \
  }                                                                      \
  return NS_NOINTERFACE;                                                 \
}
#define MY_NS_IMPL_ISUPPORTS(_class,_classiiddef,_interface) \
  NS_IMPL_ADDREF(_class)                       \
  NS_IMPL_RELEASE(_class)                      \
  MY_NS_IMPL_QUERY_INTERFACE(_class,_classiiddef,_interface)
//---- end XPCOM
//---- for XPCOM based detector interfaces
#include "nsICharsetDetector.h"
#include "nsICharsetDetectionObserver.h"
#include "nsIStringCharsetDetector.h"
#include "nsPSMDetectors.h"

NS_DEFINE_CID(kJAPSMDetectorCID,  NS_JA_PSMDETECTOR_CID);
NS_DEFINE_CID(kJAStringPSMDetectorCID,  NS_JA_STRING_PSMDETECTOR_CID);
NS_DEFINE_CID(kKOPSMDetectorCID,  NS_KO_PSMDETECTOR_CID);
NS_DEFINE_CID(kKOStringPSMDetectorCID,  NS_KO_STRING_PSMDETECTOR_CID);
NS_DEFINE_CID(kZHCNPSMDetectorCID,  NS_ZHCN_PSMDETECTOR_CID);
NS_DEFINE_CID(kZHCNStringPSMDetectorCID,  NS_ZHCN_STRING_PSMDETECTOR_CID);
NS_DEFINE_CID(kZHTWPSMDetectorCID,  NS_ZHTW_PSMDETECTOR_CID);
NS_DEFINE_CID(kZHTWStringPSMDetectorCID,  NS_ZHTW_STRING_PSMDETECTOR_CID);
NS_DEFINE_CID(kZHPSMDetectorCID,  NS_ZH_PSMDETECTOR_CID);
NS_DEFINE_CID(kZHStringPSMDetectorCID,  NS_ZH_STRING_PSMDETECTOR_CID);
NS_DEFINE_CID(kCJKPSMDetectorCID,  NS_CJK_PSMDETECTOR_CID);
NS_DEFINE_CID(kCJKStringPSMDetectorCID,  NS_CJK_STRING_PSMDETECTOR_CID);
//---- end XPCOM based detector interfaces


#define DETECTOR_DEBUG

/*
 In the current design, we know the following combination of verifiers 
 are not good-

 1. Two or more of the following verifier in one detector:
      nsEUCJPVerifer 
      nsGB2312Verifier
      nsEUCKRVerifer 
      nsEUCTWVerifier
      nsBIG5Verifer 

 */

//==========================================================

#define MAX_VERIFIERS 16
class nsPSMDetector {
public :
   nsPSMDetector(PRUint8 aItems, nsVerifier** aVerifierSet);
   virtual ~nsPSMDetector() {};

   virtual PRBool HandleData(const char* aBuf, PRUint32 aLen);
   virtual void   DataEnd();
 
protected:
   virtual void Report(const char* charset) = 0;

   PRUint8 mItems;
   PRUint8 mState[MAX_VERIFIERS];
   PRUint8 mItemIdx[MAX_VERIFIERS];
   nsVerifier** mVerifier;
   PRBool mDone;

private:
#ifdef DETECTOR_DEBUG
   PRUint32 mDbgTest;
   PRUint32 mDbgLen;
#endif

};
//----------------------------------------------------------
nsPSMDetector::nsPSMDetector(PRUint8 aItems, nsVerifier** aVerifierSet)
{
  mDone= PR_FALSE;
  mItems = aItems;
  NS_ASSERTION(MAX_VERIFIERS >= aItems , "MAX_VERIFIERS is too small!");
  mVerifier = aVerifierSet;
  NS_ASSERTION(aItems <= MAX_VERIFIERS , "Too many verifiers");
  for(PRUint8 i = 0; i < mItems ; i++)
  {
     mState[i] = 0;
     mItemIdx[i] = i;
  }
#ifdef DETECTOR_DEBUG
  mDbgLen = mDbgTest = 0;
#endif
}
//----------------------------------------------------------
void nsPSMDetector::DataEnd()
{
}
//----------------------------------------------------------

// #define ftang_TRACE_STATE
// #define TRACE_VERIFIER nsCP1252Verifier

PRBool nsPSMDetector::HandleData(const char* aBuf, PRUint32 aLen)
{
  PRUint32 i,j;
  PRUint32 st;
  for(i=0; i < aLen; i++)
  {
     char b = aBuf[i];
     for(j = 0; j < mItems; )
     {
#ifdef ftang_TRACE_STATE
       if(  mVerifier[mItemIdx[j]] == & TRACE_VERIFIER )
       {
           printf("%d = %d\n", i + mDbgLen, mState[j]);
       }
#endif
#ifdef DETECTOR_DEBUG
        mDbgTest++;
#endif 
        st = GETNEXTSTATE( mVerifier[mItemIdx[j]], b, mState[j] );
        if(eItsMe == st) 
        {
#ifdef DETECTOR_DEBUG
            printf("It's %s- byte %d(%x) test %d\n", 
                    mVerifier[mItemIdx[j]]->charset,
                    i+mDbgLen,
                    i+mDbgLen,
                    mDbgTest
                  );
#endif
            Report( mVerifier[mItemIdx[j]]->charset);
            mDone = mDone;
            return PR_TRUE;
        } else if (eError == st) 
        {
#ifdef DETECTOR_DEBUG
            printf("It's NOT %s- byte %d(%x)\n", 
                    mVerifier[mItemIdx[j]]->charset,
                    i+mDbgLen,
                    i+mDbgLen);
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
     }
     if( mItems <= 1) 
     {
         if( 1 == mItems) {
#ifdef DETECTOR_DEBUG
             printf("It's %s- byte %d (%x) Test %d. The only left\n", 
                       mVerifier[mItemIdx[0]]->charset,
                       i+mDbgLen,
                       i+mDbgLen,
                       mDbgTest);
#endif
             Report( mVerifier[mItemIdx[0]]->charset);
         }
         mDone = PR_TRUE;
         return mDone;
     }
  }
#ifdef DETECTOR_DEBUG
  mDbgLen += aLen;
#endif
  return PR_FALSE;
}

//==========================================================
/*
   This class won't detect x-euc-tw for now. It can  only 
   tell a Big5 document is not x-euc-tw , but cannot tell
   a x-euc-tw docuement is not Big5 unless we hit characters
   defined in CNS 11643 plane 2.
   
   May need improvement ....
 */

#define ZHTW_DETECTOR_NUM_VERIFIERS 7
static nsVerifier *gZhTwVerifierSet[ZHTW_DETECTOR_NUM_VERIFIERS] = {
      &nsUTF8Verifier,
      &nsBIG5Verifier,
      &nsISO2022CNVerifier,
      &nsEUCTWVerifier,
      &nsCP1252Verifier,
      &nsUCS2BEVerifier,
      &nsUCS2LEVerifier
};
//==========================================================
#define KO_DETECTOR_NUM_VERIFIERS 6
static nsVerifier *gKoVerifierSet[KO_DETECTOR_NUM_VERIFIERS] = {
      &nsUTF8Verifier,
      &nsEUCKRVerifier,
      &nsISO2022KRVerifier,
      &nsCP1252Verifier,
      &nsUCS2BEVerifier,
      &nsUCS2LEVerifier
};
//==========================================================
#define ZHCN_DETECTOR_NUM_VERIFIERS 7
static nsVerifier *gZhCnVerifierSet[ZHCN_DETECTOR_NUM_VERIFIERS] = {
      &nsUTF8Verifier,
      &nsGB2312Verifier,
      &nsISO2022CNVerifier,
      &nsHZVerifier,
      &nsCP1252Verifier,
      &nsUCS2BEVerifier,
      &nsUCS2LEVerifier
};
//==========================================================
#define JA_DETECTOR_NUM_VERIFIERS 7
static nsVerifier *gJaVerifierSet[JA_DETECTOR_NUM_VERIFIERS] = {
      &nsUTF8Verifier,
      &nsSJISVerifier,
      &nsEUCJPVerifier,
      &nsISO2022JPVerifier,
      &nsCP1252Verifier,
      &nsUCS2BEVerifier,
      &nsUCS2LEVerifier
};
//==========================================================
#define ZH_DETECTOR_NUM_VERIFIERS 9
static nsVerifier *gZhVerifierSet[ZH_DETECTOR_NUM_VERIFIERS] = {
      &nsUTF8Verifier,
      &nsGB2312Verifier,
      &nsBIG5Verifier,
      &nsISO2022CNVerifier,
      &nsHZVerifier,
      &nsEUCTWVerifier,
      &nsCP1252Verifier,
      &nsUCS2BEVerifier,
      &nsUCS2LEVerifier
};
//==========================================================
#define CJK_DETECTOR_NUM_VERIFIERS 14
static nsVerifier *gCJKVerifierSet[CJK_DETECTOR_NUM_VERIFIERS] = {
      &nsUTF8Verifier,
      &nsSJISVerifier,
      &nsEUCJPVerifier,
      &nsISO2022JPVerifier,
      &nsEUCKRVerifier,
      &nsISO2022KRVerifier,
      &nsBIG5Verifier,
      &nsEUCTWVerifier,
      &nsGB2312Verifier,
      &nsISO2022CNVerifier,
      &nsHZVerifier,
      &nsCP1252Verifier,
      &nsUCS2BEVerifier,
      &nsUCS2LEVerifier
};
//==========================================================
class nsXPCOMDetector : 
      nsPSMDetector,
      public nsICharsetDetector // Implement the interface 
{
  NS_DECL_ISUPPORTS
public:
    nsXPCOMDetector(PRUint8 aItems, nsVerifier** aVer);
    virtual ~nsXPCOMDetector();
  NS_IMETHOD Init(nsICharsetDetectionObserver* aObserver);
  NS_IMETHOD DoIt(const char* aBuf, PRUint32 aLen, PRBool* oDontFeedMe);
  NS_IMETHOD Done();

protected:
  virtual void Report(const char* charset);

private:
  nsICharsetDetectionObserver* mObserver;
};
//----------------------------------------------------------
nsXPCOMDetector::nsXPCOMDetector(PRUint8 aItems, nsVerifier **aVer)
   : nsPSMDetector( aItems, aVer)
{
  NS_INIT_REFCNT();
  PR_AtomicIncrement(&g_InstanceCount);
  mObserver = nsnull;
}
//----------------------------------------------------------
nsXPCOMDetector::~nsXPCOMDetector()
{
  NS_IF_RELEASE(mObserver);
  PR_AtomicDecrement(&g_InstanceCount);
}
//----------------------------------------------------------
MY_NS_IMPL_ISUPPORTS(nsXPCOMDetector,nsICharsetDetector::GetIID(), nsICharsetDetector)
//----------------------------------------------------------
NS_IMETHODIMP nsXPCOMDetector::Init(
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
NS_IMETHODIMP nsXPCOMDetector::DoIt(
  const char* aBuf, PRUint32 aLen, PRBool* oDontFeedMe)
{
  NS_ASSERTION(mObserver != nsnull , "have not init yet");

  if((nsnull == aBuf) || (nsnull == oDontFeedMe))
     return NS_ERROR_ILLEGAL_VALUE;

  this->HandleData(aBuf, aLen);
  *oDontFeedMe = mDone;
  return NS_OK;
}
//----------------------------------------------------------
NS_IMETHODIMP nsXPCOMDetector::Done()
{
  NS_ASSERTION(mObserver != nsnull , "have not init yet");
  return NS_OK;
}
//----------------------------------------------------------
void nsXPCOMDetector::Report(const char* charset)
{
  mObserver->Notify(charset, eSureAnswer);
}
//==========================================================
class nsXPCOMStringDetector : 
      nsPSMDetector,
      public nsIStringCharsetDetector // Implement the interface 
{
  NS_DECL_ISUPPORTS
public:
    nsXPCOMStringDetector(PRUint8 aItems, nsVerifier** aVer);
    virtual ~nsXPCOMStringDetector();
    NS_IMETHOD DoIt(const char* aBuf, PRUint32 aLen, 
                   const char** oCharset, 
                   nsDetectionConfident &oConfident);
protected:
  virtual void Report(const char* charset);
private:
  const char* mResult;
};
//----------------------------------------------------------
nsXPCOMStringDetector::nsXPCOMStringDetector(PRUint8 aItems, nsVerifier** aVer)
   : nsPSMDetector( aItems, aVer)
{
  NS_INIT_REFCNT();
  PR_AtomicIncrement(&g_InstanceCount);
}
//----------------------------------------------------------
nsXPCOMStringDetector::~nsXPCOMStringDetector()
{
  PR_AtomicDecrement(&g_InstanceCount);
}
//----------------------------------------------------------
MY_NS_IMPL_ISUPPORTS(nsXPCOMStringDetector,nsIStringCharsetDetector::GetIID(), nsIStringCharsetDetector)
//----------------------------------------------------------
void nsXPCOMStringDetector::Report(const char* charset)
{
  mResult = charset;
}
//----------------------------------------------------------
NS_IMETHODIMP nsXPCOMStringDetector::DoIt(const char* aBuf, PRUint32 aLen, 
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

class nsXPCOMDetectorFactory : public nsIFactory {
   NS_DECL_ISUPPORTS

public:
   nsXPCOMDetectorFactory(const nsCID& aCID) {
     NS_INIT_REFCNT();
     mCID = aCID;
     PR_AtomicIncrement(&g_InstanceCount);
   }
   virtual ~nsXPCOMDetectorFactory() {
     PR_AtomicDecrement(&g_InstanceCount);
   }

   NS_IMETHOD CreateInstance(nsISupports* aDelegate, const nsIID& aIID, void** aResult);
   NS_IMETHOD LockFactory(PRBool aLock);
private:
   nsCID mCID;

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

  nsXPCOMDetector *inst1 = nsnull;
  nsXPCOMStringDetector *inst2 = nsnull;
 
  if (mCID.Equals(kJAPSMDetectorCID)) {
      inst1 = new nsXPCOMDetector(JA_DETECTOR_NUM_VERIFIERS, gJaVerifierSet);
  } else if (mCID.Equals(kKOPSMDetectorCID)) {
      inst1 = new nsXPCOMDetector(KO_DETECTOR_NUM_VERIFIERS, gKoVerifierSet);
  } else if (mCID.Equals(kZHCNPSMDetectorCID)) {
      inst1 = new nsXPCOMDetector(ZHCN_DETECTOR_NUM_VERIFIERS, gZhCnVerifierSet);
  } else if (mCID.Equals(kZHTWPSMDetectorCID)) {
      inst1 = new nsXPCOMDetector(ZHTW_DETECTOR_NUM_VERIFIERS, gZhTwVerifierSet);
  } else if (mCID.Equals(kZHPSMDetectorCID)) {
      inst1 = new nsXPCOMDetector(ZH_DETECTOR_NUM_VERIFIERS, gZhVerifierSet);
  } else if (mCID.Equals(kCJKPSMDetectorCID)) {
      inst1 = new nsXPCOMDetector(CJK_DETECTOR_NUM_VERIFIERS, gCJKVerifierSet);
  } else if (mCID.Equals(kJAStringPSMDetectorCID)) {
      inst2 = new nsXPCOMStringDetector(JA_DETECTOR_NUM_VERIFIERS - 3, gJaVerifierSet);
  } else if (mCID.Equals(kKOStringPSMDetectorCID)) {
      inst2 = new nsXPCOMStringDetector(KO_DETECTOR_NUM_VERIFIERS - 3, gKoVerifierSet);
  } else if (mCID.Equals(kZHCNStringPSMDetectorCID)) {
      inst2 = new nsXPCOMStringDetector(ZHCN_DETECTOR_NUM_VERIFIERS - 3, gZhCnVerifierSet);
  } else if (mCID.Equals(kZHTWStringPSMDetectorCID)) {
      inst2 = new nsXPCOMStringDetector(ZHTW_DETECTOR_NUM_VERIFIERS - 3, gZhTwVerifierSet);
  } else if (mCID.Equals(kZHStringPSMDetectorCID)) {
      inst2 = new nsXPCOMStringDetector(ZH_DETECTOR_NUM_VERIFIERS - 3, gZhVerifierSet);
  } else if (mCID.Equals(kCJKStringPSMDetectorCID)) {
      inst2 = new nsXPCOMStringDetector(CJK_DETECTOR_NUM_VERIFIERS - 3, gCJKVerifierSet);
  }
  if((NULL == inst1) && (NULL == inst2)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  nsresult res = NS_OK;

  if(inst1)
     res =inst1->QueryInterface(aIID, aResult);
  else
     res =inst2->QueryInterface(aIID, aResult);

  if(NS_FAILED(res)) {
     if(inst1)
       delete inst1;
     if(inst2)
       delete inst2;
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
nsIFactory* NEW_PSMDETECTOR_FACTORY(const nsCID& aClass) {
  return new nsXPCOMDetectorFactory(aClass);
};


       
