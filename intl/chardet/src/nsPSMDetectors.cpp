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
//---- end XPCOM based detector interfaces


#define DETECTOR_DEBUG

/*
 In the current design, we know the following combination of verifiers 
 are not good-

 1. Two or more of the following verifier in one detector:
      nsISO2022JPVerifer nsISO2022KRVerifier nsISO2022CNVerifier

    We can improve this by making the state machine more complete, not
    just goto eItsMe state when it hit a single ESC, but the whole ESC
    sequence.
      

 2. Two or more of the following verifier in one detector:
      nsEUCJPVerifer 
      nsGB2312Verifier
      nsEUCKRVerifer 
      nsEUCTWVerifier
      nsBIG5Verifer 

 */

//==========================================================
class nsPSMDetectorBase {
public :
   nsPSMDetectorBase();
   virtual ~nsPSMDetectorBase() {};

   virtual PRBool HandleData(const char* aBuf, PRUint32 aLen);
   virtual void   DataEnd();
 
protected:
   virtual void Report(const char* charset) = 0;

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
#define ZHTW_DETECTOR_NUM_VERIFIERS 6

/*
   This class won't detect x-euc-tw for now. It can  only 
   tell a Big5 document is not x-euc-tw , but cannot tell
   a x-euc-tw docuement is not Big5 unless we hit characters
   defined in CNS 11643 plane 2.
   
   May need improvement ....
 */
/*
 ALSO, we need to add nsISO2022CNVerifier here...
 */
class nsZhTwDetector : public nsPSMDetectorBase {
public:
  nsZhTwDetector();
  virtual ~nsZhTwDetector() {};
private:
  PRUint32 mStateP[ZHTW_DETECTOR_NUM_VERIFIERS];
  PRUint32 mItemIdxP[ZHTW_DETECTOR_NUM_VERIFIERS];
};
//----------------------------------------------------------

static nsVerifier *gZhTwVerifierSet[ZHTW_DETECTOR_NUM_VERIFIERS] = {
      &nsCP1252Verifier,
      &nsUTF8Verifier,
      &nsUCS2BEVerifier,
      &nsUCS2LEVerifier,
      &nsBIG5Verifier,
      &nsEUCTWVerifier
};
//----------------------------------------------------------
nsZhTwDetector::nsZhTwDetector() 
{
  mState = mStateP; 
  mItemIdx = mItemIdxP; 
  mVerifier = gZhTwVerifierSet; 
  mItems = ZHTW_DETECTOR_NUM_VERIFIERS;
  for(PRUint32 i = 0; i < mItems ; i++) {
     mItemIdx[i] = i;
     mState[i] = 0;
  }
}
//==========================================================
#define KO_DETECTOR_NUM_VERIFIERS 5
/*
 We need to add nsISO2022KRVerifier here...
 */

class nsKoDetector : public nsPSMDetectorBase {
public:
  nsKoDetector();
  virtual ~nsKoDetector() {};
private:
  PRUint32 mStateP[KO_DETECTOR_NUM_VERIFIERS];
  PRUint32 mItemIdxP[KO_DETECTOR_NUM_VERIFIERS];
};
//----------------------------------------------------------

static nsVerifier *gKoVerifierSet[KO_DETECTOR_NUM_VERIFIERS] = {
      &nsCP1252Verifier,
      &nsUTF8Verifier,
      &nsUCS2BEVerifier,
      &nsUCS2LEVerifier,
      &nsEUCKRVerifier
};
//----------------------------------------------------------
nsKoDetector::nsKoDetector() 
{
  mState = mStateP; 
  mItemIdx = mItemIdxP; 
  mVerifier = gKoVerifierSet; 
  mItems = KO_DETECTOR_NUM_VERIFIERS;
  for(PRUint32 i = 0; i < mItems ; i++) {
     mItemIdx[i] = i;
     mState[i] = 0;
  }
}
//==========================================================
#define ZHCN_DETECTOR_NUM_VERIFIERS 5

/*
 We need to add nsISO2022CNVerifier  and nsHZVerifier here...
 */
class nsZhCnDetector : public nsPSMDetectorBase {
public:
  nsZhCnDetector();
  virtual ~nsZhCnDetector() {};
private:
  PRUint32 mStateP[ZHCN_DETECTOR_NUM_VERIFIERS];
  PRUint32 mItemIdxP[ZHCN_DETECTOR_NUM_VERIFIERS];
};
//----------------------------------------------------------

static nsVerifier *gZhCnVerifierSet[ZHCN_DETECTOR_NUM_VERIFIERS] = {
      &nsCP1252Verifier,
      &nsUTF8Verifier,
      &nsUCS2BEVerifier,
      &nsUCS2LEVerifier,
      &nsGB2312Verifier
};
//----------------------------------------------------------
nsZhCnDetector::nsZhCnDetector() 
{
  mState = mStateP; 
  mItemIdx = mItemIdxP; 
  mVerifier = gZhCnVerifierSet; 
  mItems = ZHCN_DETECTOR_NUM_VERIFIERS;
  for(PRUint32 i = 0; i < mItems ; i++) {
     mItemIdx[i] = i;
     mState[i] = 0;
  }
}
//==========================================================
#define JA_DETECTOR_NUM_VERIFIERS 7

class nsJaDetector : public nsPSMDetectorBase {
public:
  nsJaDetector();
  virtual ~nsJaDetector() {};
private:
  PRUint32 mStateP[JA_DETECTOR_NUM_VERIFIERS];
  PRUint32 mItemIdxP[JA_DETECTOR_NUM_VERIFIERS];
};
//----------------------------------------------------------

static nsVerifier *gJaVerifierSet[JA_DETECTOR_NUM_VERIFIERS] = {
      &nsCP1252Verifier,
      &nsUTF8Verifier,
      &nsUCS2BEVerifier,
      &nsUCS2LEVerifier,
      &nsSJISVerifier,
      &nsEUCJPVerifier,
      &nsISO2022JPVerifier
};
//----------------------------------------------------------
nsJaDetector::nsJaDetector() 
{
  mState = mStateP; 
  mItemIdx = mItemIdxP; 
  mVerifier = gJaVerifierSet; 
  mItems = JA_DETECTOR_NUM_VERIFIERS;
  for(PRUint32 i = 0; i < mItems ; i++) {
     mItemIdx[i] = i;
     mState[i] = 0;
  }
}
//==========================================================
#define ZH_DETECTOR_NUM_VERIFIERS 7

class nsZhDetector : public nsPSMDetectorBase {
public:
  nsZhDetector();
  virtual ~nsZhDetector() {};
private:
  PRUint32 mStateP[ZH_DETECTOR_NUM_VERIFIERS];
  PRUint32 mItemIdxP[ZH_DETECTOR_NUM_VERIFIERS];
};
//----------------------------------------------------------

static nsVerifier *gZhVerifierSet[ZH_DETECTOR_NUM_VERIFIERS] = {
      &nsCP1252Verifier,
      &nsUTF8Verifier,
      &nsUCS2BEVerifier,
      &nsUCS2LEVerifier,
      &nsGB2312Verifier,
      &nsBIG5Verifier,
      &nsEUCTWVerifier
};
//----------------------------------------------------------
nsZhDetector::nsZhDetector() 
{
  mState = mStateP; 
  mItemIdx = mItemIdxP; 
  mVerifier = gZhVerifierSet; 
  mItems = ZH_DETECTOR_NUM_VERIFIERS;
  for(PRUint32 i = 0; i < mItems ; i++) {
     mItemIdx[i] = i;
     mState[i] = 0;
  }
}
//==========================================================
#define CJK_DETECTOR_NUM_VERIFIERS 11

class nsCJKDetector : public nsPSMDetectorBase {
public:
  nsCJKDetector();
  virtual ~nsCJKDetector() {};
private:
  PRUint32 mStateP[CJK_DETECTOR_NUM_VERIFIERS];
  PRUint32 mItemIdxP[CJK_DETECTOR_NUM_VERIFIERS];
};
//----------------------------------------------------------

static nsVerifier *gCJKVerifierSet[CJK_DETECTOR_NUM_VERIFIERS] = {
      &nsCP1252Verifier,
      &nsUTF8Verifier,
      &nsUCS2BEVerifier,
      &nsUCS2LEVerifier,
      &nsSJISVerifier,
      &nsEUCJPVerifier,
      &nsISO2022JPVerifier,
      &nsEUCKRVerifier,
      &nsBIG5Verifier,
      &nsEUCTWVerifier,
      &nsGB2312Verifier
};
//----------------------------------------------------------
nsCJKDetector::nsCJKDetector() 
{
  mState = mStateP; 
  mItemIdx = mItemIdxP; 
  mVerifier = gCJKVerifierSet; 
  mItems = CJK_DETECTOR_NUM_VERIFIERS;
  for(PRUint32 i = 0; i < mItems ; i++) {
     mItemIdx[i] = i;
     mState[i] = 0;
  }
}
//==========================================================
class nsXPCOMDetectorBase : 
      public nsICharsetDetector // Implement the interface 
{
public:
    nsXPCOMDetectorBase();
    virtual ~nsXPCOMDetectorBase();
  NS_IMETHOD Init(nsICharsetDetectionObserver* aObserver);
  NS_IMETHOD DoIt(const char* aBuf, PRUint32 aLen, PRBool* oDontFeedMe);
  NS_IMETHOD Done();

  virtual PRBool HandleData(const char* aBuf, PRUint32 aLen) = 0;
  virtual void   DataEnd() = 0;
 
protected:
  virtual void Report(const char* charset);
  virtual PRBool IsDone() = 0;

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

  if((nsnull == aBuf) || (nsnull == oDontFeedMe))
     return NS_ERROR_ILLEGAL_VALUE;

  this->HandleData(aBuf, aLen);
  *oDontFeedMe = this->IsDone();
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
      public nsIStringCharsetDetector // Implement the interface 
{
public:
    nsXPCOMStringDetectorBase();
    virtual ~nsXPCOMStringDetectorBase();
    NS_IMETHOD DoIt(const char* aBuf, PRUint32 aLen, 
                   const char** oCharset, 
                   nsDetectionConfident &oConfident);
  virtual PRBool HandleData(const char* aBuf, PRUint32 aLen) = 0;
  virtual void   DataEnd() = 0;
protected:
  virtual void Report(const char* charset);
  virtual PRBool IsDone() = 0;
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
     if(this->IsDone()) 
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
class nsXPCOMZhTwDetector :
        public nsXPCOMDetectorBase, // Inheriate the implementation
        public nsZhTwDetector         // Inheriate the implementation
{
  NS_DECL_ISUPPORTS
  public:
    nsXPCOMZhTwDetector()
    {
      NS_INIT_REFCNT();
      PR_AtomicIncrement(&g_InstanceCount);
    };
    virtual ~nsXPCOMZhTwDetector()
    {
      PR_AtomicDecrement(&g_InstanceCount);
    }
    virtual PRBool HandleData(const char* aBuf, PRUint32 aLen) 
                      { return nsZhTwDetector::HandleData(aBuf, aLen); };
    virtual void   DataEnd() 
                      { nsZhTwDetector::DataEnd(); };
  protected:
    virtual void   Report(const char* charset)
                      { nsXPCOMDetectorBase::Report(charset); };
    virtual PRBool IsDone() { return mDone; }
};
//----------------------------------------------------------
MY_NS_IMPL_ISUPPORTS(nsXPCOMZhTwDetector,nsICharsetDetector::GetIID(), nsICharsetDetector)

//==========================================================
class nsXPCOMZhTwStringDetector :
        public nsXPCOMStringDetectorBase, // Inheriate the implementation
        public nsZhTwDetector               // Inheriate the implementation
{
  NS_DECL_ISUPPORTS
  public:
    nsXPCOMZhTwStringDetector()
    {
      NS_INIT_REFCNT();
      PR_AtomicIncrement(&g_InstanceCount);
    }
    virtual ~nsXPCOMZhTwStringDetector()
    {
      PR_AtomicDecrement(&g_InstanceCount);
    }
    virtual PRBool HandleData(const char* aBuf, PRUint32 aLen) 
                      { return nsZhTwDetector::HandleData(aBuf, aLen); };
    virtual void   DataEnd() 
                      { nsZhTwDetector::DataEnd(); };
  protected:
    virtual void   Report(const char* charset)
                      { nsXPCOMStringDetectorBase::Report(charset); };
    virtual PRBool IsDone() { return mDone; }
};
//----------------------------------------------------------
MY_NS_IMPL_ISUPPORTS(nsXPCOMZhTwStringDetector,nsIStringCharsetDetector::GetIID(), nsIStringCharsetDetector)
//==========================================================
class nsXPCOMZhCnDetector :
        public nsXPCOMDetectorBase, // Inheriate the implementation
        public nsZhCnDetector         // Inheriate the implementation
{
  NS_DECL_ISUPPORTS
  public:
    nsXPCOMZhCnDetector()
    {
      NS_INIT_REFCNT();
      PR_AtomicIncrement(&g_InstanceCount);
    };
    virtual ~nsXPCOMZhCnDetector()
    {
      PR_AtomicDecrement(&g_InstanceCount);
    }
    virtual PRBool HandleData(const char* aBuf, PRUint32 aLen) 
                      { return nsZhCnDetector::HandleData(aBuf, aLen); };
    virtual void   DataEnd() 
                      { nsZhCnDetector::DataEnd(); };
  protected:
    virtual void   Report(const char* charset)
                      { nsXPCOMDetectorBase::Report(charset); };
    virtual PRBool IsDone() { return mDone; }
};
//----------------------------------------------------------
MY_NS_IMPL_ISUPPORTS(nsXPCOMZhCnDetector,nsICharsetDetector::GetIID(), nsICharsetDetector)

//==========================================================
class nsXPCOMZhCnStringDetector :
        public nsXPCOMStringDetectorBase, // Inheriate the implementation
        public nsZhCnDetector               // Inheriate the implementation
{
  NS_DECL_ISUPPORTS
  public:
    nsXPCOMZhCnStringDetector()
    {
      NS_INIT_REFCNT();
      PR_AtomicIncrement(&g_InstanceCount);
    }
    virtual ~nsXPCOMZhCnStringDetector()
    {
      PR_AtomicDecrement(&g_InstanceCount);
    }
    virtual PRBool HandleData(const char* aBuf, PRUint32 aLen) 
                      { return nsZhCnDetector::HandleData(aBuf, aLen); };
    virtual void   DataEnd() 
                      { nsZhCnDetector::DataEnd(); };
  protected:
    virtual void   Report(const char * charset)
                      { nsXPCOMStringDetectorBase::Report(charset); };
    virtual PRBool IsDone() { return mDone; }
};
//----------------------------------------------------------
MY_NS_IMPL_ISUPPORTS(nsXPCOMZhCnStringDetector,nsIStringCharsetDetector::GetIID(), nsIStringCharsetDetector)
//==========================================================
class nsXPCOMKoDetector :
        public nsXPCOMDetectorBase, // Inheriate the implementation
        public nsKoDetector         // Inheriate the implementation
{
  NS_DECL_ISUPPORTS
  public:
    nsXPCOMKoDetector()
    {
      NS_INIT_REFCNT();
      PR_AtomicIncrement(&g_InstanceCount);
    };
    virtual ~nsXPCOMKoDetector()
    {
      PR_AtomicDecrement(&g_InstanceCount);
    }
    virtual PRBool HandleData(const char* aBuf, PRUint32 aLen) 
                      { return nsKoDetector::HandleData(aBuf, aLen); };
    virtual void   DataEnd() 
                      { nsKoDetector::DataEnd(); };
  protected:
    virtual void   Report(const char* charset)
                      { nsXPCOMDetectorBase::Report(charset); };
    virtual PRBool IsDone() { return mDone; }
};
//----------------------------------------------------------
MY_NS_IMPL_ISUPPORTS(nsXPCOMKoDetector,nsICharsetDetector::GetIID(), nsICharsetDetector)

//==========================================================
class nsXPCOMKoStringDetector :
        public nsXPCOMStringDetectorBase, // Inheriate the implementation
        public nsKoDetector               // Inheriate the implementation
{
  NS_DECL_ISUPPORTS
  public:
    nsXPCOMKoStringDetector()
    {
      NS_INIT_REFCNT();
      PR_AtomicIncrement(&g_InstanceCount);
    }
    virtual ~nsXPCOMKoStringDetector()
    {
      PR_AtomicDecrement(&g_InstanceCount);
    }
    virtual PRBool HandleData(const char* aBuf, PRUint32 aLen) 
                      { return nsKoDetector::HandleData(aBuf, aLen); };
    virtual void   DataEnd() 
                      { nsKoDetector::DataEnd(); };
  protected:
    virtual void   Report(const char *charset)
                      { nsXPCOMStringDetectorBase::Report(charset); };
    virtual PRBool IsDone() { return mDone; }
};
//----------------------------------------------------------
MY_NS_IMPL_ISUPPORTS(nsXPCOMKoStringDetector,nsIStringCharsetDetector::GetIID(), nsIStringCharsetDetector)

//==========================================================
class nsXPCOMJaDetector :
        public nsXPCOMDetectorBase, // Inheriate the implementation
        public nsJaDetector         // Inheriate the implementation
{
  NS_DECL_ISUPPORTS
  public:
    nsXPCOMJaDetector()
    {
      NS_INIT_REFCNT();
      PR_AtomicIncrement(&g_InstanceCount);
    };
    virtual ~nsXPCOMJaDetector()
    {
      PR_AtomicDecrement(&g_InstanceCount);
    }
    virtual PRBool HandleData(const char* aBuf, PRUint32 aLen) 
                      { return nsJaDetector::HandleData(aBuf, aLen); };
    virtual void   DataEnd() 
                      { nsJaDetector::DataEnd(); };
  protected:
    virtual void   Report(const char* charset)
                      { nsXPCOMDetectorBase::Report(charset); };
    virtual PRBool IsDone() { return mDone; }
};
//----------------------------------------------------------
MY_NS_IMPL_ISUPPORTS(nsXPCOMJaDetector,nsICharsetDetector::GetIID(), nsICharsetDetector)

//==========================================================
class nsXPCOMJaStringDetector :
        public nsXPCOMStringDetectorBase, // Inheriate the implementation
        public nsJaDetector               // Inheriate the implementation
{
  NS_DECL_ISUPPORTS
  public:
    nsXPCOMJaStringDetector()
    {
      NS_INIT_REFCNT();
      PR_AtomicIncrement(&g_InstanceCount);
    }
    virtual ~nsXPCOMJaStringDetector()
    {
      PR_AtomicDecrement(&g_InstanceCount);
    }
    virtual PRBool HandleData(const char* aBuf, PRUint32 aLen) 
                      { return nsJaDetector::HandleData(aBuf, aLen); };
    virtual void   DataEnd() 
                      { nsJaDetector::DataEnd(); };
  protected:
    virtual void   Report(const char *charset)
                      { nsXPCOMStringDetectorBase::Report(charset);};
    virtual PRBool IsDone() { return mDone; }
};
//----------------------------------------------------------
MY_NS_IMPL_ISUPPORTS(nsXPCOMJaStringDetector,nsIStringCharsetDetector::GetIID(), nsIStringCharsetDetector)
//==========================================================
class nsXPCOMZhDetector :
        public nsXPCOMDetectorBase, // Inheriate the implementation
        public nsZhDetector         // Inheriate the implementation
{
  NS_DECL_ISUPPORTS
  public:
    nsXPCOMZhDetector()
    {
      NS_INIT_REFCNT();
      PR_AtomicIncrement(&g_InstanceCount);
    };
    virtual ~nsXPCOMZhDetector()
    {
      PR_AtomicDecrement(&g_InstanceCount);
    }
    virtual PRBool HandleData(const char* aBuf, PRUint32 aLen) 
                      { return nsZhDetector::HandleData(aBuf, aLen); };
    virtual void   DataEnd() 
                      { nsZhDetector::DataEnd(); };
  protected:
    virtual void   Report(const char* charset)
                      { nsXPCOMDetectorBase::Report(charset); };
    virtual PRBool IsDone() { return mDone; }
};
//----------------------------------------------------------
MY_NS_IMPL_ISUPPORTS(nsXPCOMZhDetector,nsICharsetDetector::GetIID(), nsICharsetDetector)

//==========================================================
class nsXPCOMZhStringDetector :
        public nsXPCOMStringDetectorBase, // Inheriate the implementation
        public nsZhDetector               // Inheriate the implementation
{
  NS_DECL_ISUPPORTS
  public:
    nsXPCOMZhStringDetector()
    {
      NS_INIT_REFCNT();
      PR_AtomicIncrement(&g_InstanceCount);
    }
    virtual ~nsXPCOMZhStringDetector()
    {
      PR_AtomicDecrement(&g_InstanceCount);
    }
    virtual PRBool HandleData(const char* aBuf, PRUint32 aLen) 
                      { return nsZhDetector::HandleData(aBuf, aLen); };
    virtual void   DataEnd() 
                      { nsZhDetector::DataEnd(); };
  protected:
    virtual void   Report(const char *charset)
                      { nsXPCOMStringDetectorBase::Report(charset);};
    virtual PRBool IsDone() { return mDone; }
};
//----------------------------------------------------------
MY_NS_IMPL_ISUPPORTS(nsXPCOMZhStringDetector,nsIStringCharsetDetector::GetIID(), nsIStringCharsetDetector)
//==========================================================
class nsXPCOMCJKDetector :
        public nsXPCOMDetectorBase, // Inheriate the implementation
        public nsCJKDetector         // Inheriate the implementation
{
  NS_DECL_ISUPPORTS
  public:
    nsXPCOMCJKDetector()
    {
      NS_INIT_REFCNT();
      PR_AtomicIncrement(&g_InstanceCount);
    };
    virtual ~nsXPCOMCJKDetector()
    {
      PR_AtomicDecrement(&g_InstanceCount);
    }
    virtual PRBool HandleData(const char* aBuf, PRUint32 aLen) 
                      { return nsCJKDetector::HandleData(aBuf, aLen); };
    virtual void   DataEnd() 
                      { nsCJKDetector::DataEnd(); };
  protected:
    virtual void   Report(const char* charset)
                      { nsXPCOMDetectorBase::Report(charset); };
    virtual PRBool IsDone() { return mDone; }
};
//----------------------------------------------------------
MY_NS_IMPL_ISUPPORTS(nsXPCOMCJKDetector,nsICharsetDetector::GetIID(), nsICharsetDetector)

//==========================================================
class nsXPCOMCJKStringDetector :
        public nsXPCOMStringDetectorBase, // Inheriate the implementation
        public nsCJKDetector               // Inheriate the implementation
{
  NS_DECL_ISUPPORTS
  public:
    nsXPCOMCJKStringDetector()
    {
      NS_INIT_REFCNT();
      PR_AtomicIncrement(&g_InstanceCount);
    }
    virtual ~nsXPCOMCJKStringDetector()
    {
      PR_AtomicDecrement(&g_InstanceCount);
    }
    virtual PRBool HandleData(const char* aBuf, PRUint32 aLen) 
                      { return nsCJKDetector::HandleData(aBuf, aLen); };
    virtual void   DataEnd() 
                      { nsCJKDetector::DataEnd(); };
  protected:
    virtual void   Report(const char *charset)
                      { nsXPCOMStringDetectorBase::Report(charset);};
    virtual PRBool IsDone() { return mDone; }
};
//----------------------------------------------------------
MY_NS_IMPL_ISUPPORTS(nsXPCOMCJKStringDetector,nsIStringCharsetDetector::GetIID(), nsIStringCharsetDetector)

//==========================================================
typedef enum {
  eJaDetector,
  eKoDetector,
  eZhCnDetector,
  eZhTwDetector,
  eZhDetector,
  eCJKDetector,
  eJaStringDetector,
  eKoStringDetector,
  eZhCnStringDetector,
  eZhTwStringDetector,
  eZhStringDetector,
  eCJKStringDetector
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
    case eKoDetector:
      inst = new nsXPCOMKoDetector();
      break;
    case eZhCnDetector:
      inst = new nsXPCOMZhCnDetector();
      break;
    case eZhTwDetector:
      inst = new nsXPCOMZhTwDetector();
      break;
    case eZhDetector:
      inst = new nsXPCOMZhDetector();
      break;
    case eCJKDetector:
      inst = new nsXPCOMCJKDetector();
      break;
    case eJaStringDetector:
      inst = new nsXPCOMJaStringDetector();
      break;
    case eKoStringDetector:
      inst = new nsXPCOMKoStringDetector();
      break;
    case eZhCnStringDetector:
      inst = new nsXPCOMZhCnStringDetector();
      break;
    case eZhTwStringDetector:
      inst = new nsXPCOMZhTwStringDetector();
      break;
    case eCJKStringDetector:
      inst = new nsXPCOMCJKStringDetector();
      break;
    case eZhStringDetector:
      inst = new nsXPCOMZhStringDetector();
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
nsIFactory* NEW_KO_PSMDETECTOR_FACTORY() {
  return new nsXPCOMDetectorFactory(eKoDetector);
};
nsIFactory* NEW_ZHCN_PSMDETECTOR_FACTORY() {
  return new nsXPCOMDetectorFactory(eZhCnDetector);
};
nsIFactory* NEW_ZHTW_PSMDETECTOR_FACTORY() {
  return new nsXPCOMDetectorFactory(eZhTwDetector);
};
nsIFactory* NEW_ZH_PSMDETECTOR_FACTORY() {
  return new nsXPCOMDetectorFactory(eZhDetector);
};
nsIFactory* NEW_CJK_PSMDETECTOR_FACTORY() {
  return new nsXPCOMDetectorFactory(eCJKDetector);
};

nsIFactory* NEW_JA_STRING_PSMDETECTOR_FACTORY() {
  return new nsXPCOMDetectorFactory(eJaStringDetector);
};
nsIFactory* NEW_KO_STRING_PSMDETECTOR_FACTORY() {
  return new nsXPCOMDetectorFactory(eKoStringDetector);
};
nsIFactory* NEW_ZHCN_STRING_PSMDETECTOR_FACTORY() {
  return new nsXPCOMDetectorFactory(eZhCnStringDetector);
};
nsIFactory* NEW_ZHTW_STRING_PSMDETECTOR_FACTORY() {
  return new nsXPCOMDetectorFactory(eZhTwStringDetector);
};
nsIFactory* NEW_ZH_STRING_PSMDETECTOR_FACTORY() {
  return new nsXPCOMDetectorFactory(eZhStringDetector);
};
nsIFactory* NEW_CJK_STRING_PSMDETECTOR_FACTORY() {
  return new nsXPCOMDetectorFactory(eCJKStringDetector);
};


       
