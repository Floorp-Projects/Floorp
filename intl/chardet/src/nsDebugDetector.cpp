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

#include "nsDebugDetector.h"
#include "pratom.h"
#include "nsCharDetDll.h"


#define REPORT_CHARSET "ISO-8859-7"
#define REPORT_CONFIDENT  eSureAnswer

//--------------------------------------------------------------------
nsDebugDetector::nsDebugDetector( nsDebugDetectorSel aSel)
{
  mSel = aSel;
  mBlks = 0;
  mObserver = nsnull;
  mStop = PR_FALSE;
  NS_INIT_REFCNT();
}
//--------------------------------------------------------------------
nsDebugDetector::~nsDebugDetector()
{
}
//--------------------------------------------------------------------
NS_IMETHODIMP nsDebugDetector::Init(nsICharsetDetectionObserver* aObserver)
{
  NS_ASSERTION(mObserver == nsnull , "Init twice");
  if(nsnull == aObserver)
     return NS_ERROR_ILLEGAL_VALUE;

  mObserver = aObserver;
  return NS_OK;
}
//--------------------------------------------------------------------

NS_IMETHODIMP nsDebugDetector::DoIt(const char* aBytesArray, PRUint32 aLen, PRBool* oDontFeedMe)
{
  NS_ASSERTION(mObserver != nsnull , "have not init yet");
  NS_ASSERTION(mStop == PR_FALSE , "don't call DoIt if we return PR_TRUE in oDontFeedMe");

  if((nsnull == aBytesArray) || (nsnull == oDontFeedMe))
     return NS_ERROR_ILLEGAL_VALUE;

  mBlks++;
  if((k1stBlk == mSel) && (1 == mBlks)) {
     *oDontFeedMe = mStop = PR_TRUE;
     Report();
  } else if((k2ndBlk == mSel) && (2 == mBlks)) {
     *oDontFeedMe = mStop = PR_TRUE;
     Report();
  } else {
     *oDontFeedMe = mStop = PR_FALSE;
  }
   
  return NS_OK;
}

//--------------------------------------------------------------------
NS_IMETHODIMP nsDebugDetector::Done()
{
  NS_ASSERTION(mObserver != nsnull , "have not init yet");
  if(klastBlk == mSel)
     Report();
  return NS_OK;
}
//--------------------------------------------------------------------
void nsDebugDetector::Report()
{
  mObserver->Notify( REPORT_CHARSET, REPORT_CONFIDENT);
}


NS_IMPL_ISUPPORTS1(nsDebugDetector, nsICharsetDetector)

