/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */
#include "nscore.h"
#include "nsCyrillicProb.h"
#include "nsCyrillicClass.h"
#include <stdio.h>

#include "nsCyrillicDetector.h"
#include "nsISupports.h"
#include "nsICharsetDetector.h"
#include "nsIStringCharsetDetector.h"
#include "nsICharsetDetectionObserver.h"

#include "nsCharDetDll.h"
//---- for XPCOM
#include "nsIFactory.h"
#include "nsISupports.h"
#include "nsCharDetDll.h"
#include "pratom.h"
#include "nslog.h"

NS_IMPL_LOG(nsCyrillicDetectorLog)
#define PRINTF NS_LOG_PRINTF(nsCyrillicDetectorLog)
#define FLUSH  NS_LOG_FLUSH(nsCyrillicDetectorLog)

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
  if (aIID.Equals(NS_GET_IID(nsISupports))) {                            \
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

NS_DEFINE_CID(kRUProbDetectorCID, NS_RU_PROBDETECTOR_CID);
NS_DEFINE_CID(kUKProbDetectorCID, NS_UK_PROBDETECTOR_CID);
NS_DEFINE_CID(kRUStringProbDetectorCID, NS_RU_STRING_PROBDETECTOR_CID);
NS_DEFINE_CID(kUKStringProbDetectorCID, NS_UK_STRING_PROBDETECTOR_CID);

#define NUM_CYR_CHARSET 5

class nsCyrillicDetector {
public:
   nsCyrillicDetector(PRUint8 aItems, 
                      PRUint8 ** aCyrillicClass, 
                      const char **aCharsets)
     {
         mItems = aItems;
         mCyrillicClass = aCyrillicClass;
         mCharsets = aCharsets;
         for(PRUint8 i=0;i<mItems;i++)
            mProb[i] = mLastCls[i] =0;
         mDone = PR_FALSE;
     };
   virtual ~nsCyrillicDetector() {};
   virtual void HandleData(const char* aBuf, PRUint32 aLen);
   virtual void   DataEnd();
protected:
   virtual void Report(const char* aCharset) = 0;

private:
   PRUint8  mItems;
   PRUint8 ** mCyrillicClass;
   const char** mCharsets;
   PRUint32 mProb[NUM_CYR_CHARSET];
   PRUint8 mLastCls[NUM_CYR_CHARSET];
   PRBool  mDone;
};

//---------------------------------------------------------------------
void nsCyrillicDetector::HandleData(const char* aBuf, PRUint32 aLen)
{
   PRUint8 cls;
   const char* b;
   PRUint32 i;
   if(mDone) 
      return;
   for(i=0, b=aBuf;i<aLen;i++,b++)
   {
     for(PRUint8 j=0;j<mItems;j++)
     {
        if( 0x80 & *b)
           cls = mCyrillicClass[j][(*b) & 0x7F];
        else 
           cls = 0;
        NS_ASSERTION( cls <= 32 , "illegal character class");
        mProb[j] += gCyrillicProb[mLastCls[j]][cls];
        mLastCls[j] = cls;
     } 
   }
   // We now only based on the first block we receive
   DataEnd();
}
//---------------------------------------------------------------------
#define THRESHOLD_RATIO 1.5f
void nsCyrillicDetector::DataEnd()
{
   PRUint32 max=0;
   PRUint8  maxIdx=0;
   PRUint8 j;
   if(mDone) 
      return;
   for(j=0;j<mItems;j++) {
      if(mProb[j] > max)
      {
           max = mProb[j];
           maxIdx= j;
      }
   }

   if( 0 == max ) // if we didn't get any 8 bits data 
     return;

#ifdef DEBUG
   for(j=0;j<mItems;j++) 
     PRINTF("Charset %s->\t%d\n", mCharsets[j], mProb[j]);
#endif
   this->Report(mCharsets[maxIdx]);
   mDone = PR_TRUE;
}
//=====================================================================
class nsCyrXPCOMDetector :  
      public nsCyrillicDetector,
      public nsICharsetDetector
{
  NS_DECL_ISUPPORTS
  public:
     nsCyrXPCOMDetector(PRUint8 aItems, 
                      PRUint8 ** aCyrillicClass, 
                      const char **aCharsets);
     virtual ~nsCyrXPCOMDetector();
     NS_IMETHOD Init(nsICharsetDetectionObserver* aObserver);
     NS_IMETHOD DoIt(const char* aBuf, PRUint32 aLen, PRBool *oDontFeedMe);
     NS_IMETHOD Done();
  protected:
     virtual void Report(const char* aCharset);
  private:
     nsICharsetDetectionObserver* mObserver;
};
//---------------------------------------------------------------------
nsCyrXPCOMDetector:: nsCyrXPCOMDetector(PRUint8 aItems, 
                      PRUint8 ** aCyrillicClass, 
                      const char **aCharsets)
	     : nsCyrillicDetector(aItems, aCyrillicClass, aCharsets)
{
    NS_INIT_REFCNT();
    PR_AtomicIncrement(&g_InstanceCount);
    mObserver = nsnull;
}
//---------------------------------------------------------------------
nsCyrXPCOMDetector::~nsCyrXPCOMDetector() 
{
    NS_IF_RELEASE(mObserver);
    PR_AtomicDecrement(&g_InstanceCount);
}
//---------------------------------------------------------------------
MY_NS_IMPL_ISUPPORTS(nsCyrXPCOMDetector, NS_GET_IID(nsICharsetDetector), nsICharsetDetector)
//---------------------------------------------------------------------
NS_IMETHODIMP nsCyrXPCOMDetector::Init(
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
NS_IMETHODIMP nsCyrXPCOMDetector::DoIt(
  const char* aBuf, PRUint32 aLen, PRBool* oDontFeedMe)
{
  NS_ASSERTION(mObserver != nsnull , "have not init yet");

  if((nsnull == aBuf) || (nsnull == oDontFeedMe))
     return NS_ERROR_ILLEGAL_VALUE;

  this->HandleData(aBuf, aLen);
  *oDontFeedMe = PR_FALSE;
  return NS_OK;
}
//----------------------------------------------------------
NS_IMETHODIMP nsCyrXPCOMDetector::Done()
{
  NS_ASSERTION(mObserver != nsnull , "have not init yet");
  this->DataEnd();
  return NS_OK;
}
//----------------------------------------------------------
void nsCyrXPCOMDetector::Report(const char* aCharset)
{
  NS_ASSERTION(mObserver != nsnull , "have not init yet");
  mObserver->Notify(aCharset, eBestAnswer);
}


//=====================================================================
class nsCyrXPCOMStringDetector :  
      public nsCyrillicDetector,
      public nsIStringCharsetDetector
{
  NS_DECL_ISUPPORTS
  public:
     nsCyrXPCOMStringDetector(PRUint8 aItems, 
                      PRUint8 ** aCyrillicClass, 
                      const char **aCharsets);
     virtual ~nsCyrXPCOMStringDetector();
     NS_IMETHOD DoIt(const char* aBuf, PRUint32 aLen, 
                     const char** oCharset, nsDetectionConfident &oConf);
  protected:
     virtual void Report(const char* aCharset);
  private:
     nsICharsetDetectionObserver* mObserver;
     const char* mResult;
};
//---------------------------------------------------------------------
nsCyrXPCOMStringDetector:: nsCyrXPCOMStringDetector(PRUint8 aItems, 
                      PRUint8 ** aCyrillicClass, 
                      const char **aCharsets)
	     : nsCyrillicDetector(aItems, aCyrillicClass, aCharsets)
{
    NS_INIT_REFCNT();
    PR_AtomicIncrement(&g_InstanceCount);
}
//---------------------------------------------------------------------
nsCyrXPCOMStringDetector::~nsCyrXPCOMStringDetector() 
{
    PR_AtomicDecrement(&g_InstanceCount);
}
//---------------------------------------------------------------------
MY_NS_IMPL_ISUPPORTS(nsCyrXPCOMStringDetector, 
  NS_GET_IID(nsIStringCharsetDetector), nsIStringCharsetDetector)
//---------------------------------------------------------------------
void nsCyrXPCOMStringDetector::Report(const char *aCharset) 
{
   mResult = aCharset;
}
//---------------------------------------------------------------------
NS_IMETHODIMP nsCyrXPCOMStringDetector::DoIt(const char* aBuf, PRUint32 aLen, 
                     const char** oCharset, nsDetectionConfident &oConf)
{
   mResult = nsnull;
   this->HandleData(aBuf, aLen); 
   this->DataEnd();
   *oCharset=mResult;
   oConf = eBestAnswer;
   return NS_OK;
}
//=====================================================================
static PRUint8 *gCyrillicCls[5] =
{
   CP1251Map,
   KOI8Map,
   ISO88595Map,
   MacCyrillicMap,
   IBM866Map
};
static const char* gRussian[5] = {
  "windows-1251", 
  "KOI8-R", 
  "ISO-8859-5", 
  "x-mac-cyrillic",
  "IBM866"
};
static const char* gUkrainian[5] = {
  "windows-1251", 
  "KOI8-U", 
  "ISO-8859-5", 
  "x-mac-ukrainian",
  "IBM866"
};
//=====================================================================

class MyFactory : public nsIFactory {
   NS_DECL_ISUPPORTS

public:
   MyFactory(const nsCID& aCID) {
     NS_INIT_REFCNT();
     mCID = aCID;
     PR_AtomicIncrement(&g_InstanceCount);
   }
   virtual ~MyFactory() {
     PR_AtomicDecrement(&g_InstanceCount);
   }

   NS_IMETHOD CreateInstance(nsISupports* aDelegate, const nsIID& aIID, void** aResult);
   NS_IMETHOD LockFactory(PRBool aLock);
private:
   nsCID mCID;

};

//--------------------------------------------------------------
NS_DEFINE_IID( kIFactoryIID, NS_IFACTORY_IID);
NS_IMPL_ISUPPORTS( MyFactory , kIFactoryIID);

NS_IMETHODIMP MyFactory::CreateInstance(
    nsISupports* aDelegate, const nsIID &aIID, void** aResult)
{
  if(NULL == aResult)
        return NS_ERROR_NULL_POINTER;
  if(NULL != aDelegate)
        return NS_ERROR_NO_AGGREGATION;

  *aResult = NULL;

  nsCyrXPCOMDetector *inst1 = nsnull;
  nsCyrXPCOMStringDetector *inst2 = nsnull;
 
  if (mCID.Equals(kRUProbDetectorCID)) {
      inst1 = new nsCyrXPCOMDetector(5, gCyrillicCls, gRussian);
  } else if (mCID.Equals(kRUStringProbDetectorCID)) {
      inst2 = new nsCyrXPCOMStringDetector(5, gCyrillicCls, gRussian);
  } else if (mCID.Equals(kUKProbDetectorCID)) {
      inst1 = new nsCyrXPCOMDetector(5, gCyrillicCls, gUkrainian);
  } else if (mCID.Equals(kUKStringProbDetectorCID)) {
      inst2 = new nsCyrXPCOMStringDetector(5, gCyrillicCls, gUkrainian);
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
NS_IMETHODIMP MyFactory::LockFactory(PRBool aLock)
{
  if(aLock)
     PR_AtomicIncrement( &g_LockCount );
  else
     PR_AtomicDecrement( &g_LockCount );
  return NS_OK;
}
//--------------------------------------------------------------
nsIFactory* NEW_PROBDETECTOR_FACTORY(const nsCID& aClass) {
  return new MyFactory(aClass);
};


       

