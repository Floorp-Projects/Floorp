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
#include <stdio.h>

#include "nsCOMPtr.h"
#include "nsISupports.h"
#include "nsICharsetDetector.h"
#include "nsCharDetDll.h"
#include "nsCyrillicDetector.h"

//----------------------------------------------------------------------
// Interface nsISupports [implementation]
NS_IMPL_ISUPPORTS1(nsCyrXPCOMDetector, nsICharsetDetector);
NS_IMPL_ISUPPORTS1(nsCyrXPCOMStringDetector, nsIStringCharsetDetector);

void nsCyrillicDetector::HandleData(const char* aBuf, PRUint32 aLen)
{
   PRUint8 cls;
   const char* b;
   PRUint32 i;
   if(mDone) 
      return;
   for(i=0, b=aBuf;i<aLen;i++,b++)
   {
     for(PRUintn j=0;j<mItems;j++)
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
      printf("Charset %s->\t%d\n", mCharsets[j], mProb[j]);
#endif
   this->Report(mCharsets[maxIdx]);
   mDone = PR_TRUE;
}

//---------------------------------------------------------------------
nsCyrXPCOMDetector:: nsCyrXPCOMDetector(PRUint8 aItems, 
                      PRUint8 ** aCyrillicClass, 
                      const char **aCharsets)
	     : nsCyrillicDetector(aItems, aCyrillicClass, aCharsets)
{
    NS_INIT_REFCNT();
    mObserver = nsnull;
}

//---------------------------------------------------------------------
nsCyrXPCOMDetector::~nsCyrXPCOMDetector() 
{
}

//---------------------------------------------------------------------
NS_IMETHODIMP nsCyrXPCOMDetector::Init(
  nsICharsetDetectionObserver* aObserver)
{
  NS_ASSERTION(mObserver == nsnull , "Init twice");
  if(nsnull == aObserver)
     return NS_ERROR_ILLEGAL_VALUE;

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

//---------------------------------------------------------------------
nsCyrXPCOMStringDetector:: nsCyrXPCOMStringDetector(PRUint8 aItems, 
                      PRUint8 ** aCyrillicClass, 
                      const char **aCharsets)
	     : nsCyrillicDetector(aItems, aCyrillicClass, aCharsets)
{
    NS_INIT_REFCNT();
}

//---------------------------------------------------------------------
nsCyrXPCOMStringDetector::~nsCyrXPCOMStringDetector() 
{
}

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
   mDone = PR_FALSE;
   this->HandleData(aBuf, aLen); 
   this->DataEnd();
   *oCharset=mResult;
   oConf = eBestAnswer;
   return NS_OK;
}
       

