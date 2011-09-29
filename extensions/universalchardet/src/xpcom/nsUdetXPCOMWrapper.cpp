/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Universal charset detector code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *          Shy Shalom <shooshX@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nscore.h"

#include "nsUniversalDetector.h"
#include "nsUdetXPCOMWrapper.h"
#include "nsCharSetProber.h" // for DumpStatus

#include "nsUniversalCharDetDll.h"
//---- for XPCOM
#include "nsIFactory.h"
#include "nsISupports.h"
#include "pratom.h"
#include "prmem.h"
#include "nsCOMPtr.h"

static NS_DEFINE_CID(kUniversalDetectorCID, NS_UNIVERSAL_DETECTOR_CID);
static NS_DEFINE_CID(kUniversalStringDetectorCID, NS_UNIVERSAL_STRING_DETECTOR_CID);

//---------------------------------------------------------------------
nsXPCOMDetector:: nsXPCOMDetector(PRUint32 aLanguageFilter)
 : nsUniversalDetector(aLanguageFilter)
{
}
//---------------------------------------------------------------------
nsXPCOMDetector::~nsXPCOMDetector() 
{
}
//---------------------------------------------------------------------

NS_IMPL_ISUPPORTS1(nsXPCOMDetector, nsICharsetDetector)

//---------------------------------------------------------------------
NS_IMETHODIMP nsXPCOMDetector::Init(
              nsICharsetDetectionObserver* aObserver)
{
  NS_ASSERTION(mObserver == nsnull , "Init twice");
  if(nsnull == aObserver)
    return NS_ERROR_ILLEGAL_VALUE;

  mObserver = aObserver;
  return NS_OK;
}
//----------------------------------------------------------
NS_IMETHODIMP nsXPCOMDetector::DoIt(const char* aBuf,
              PRUint32 aLen, bool* oDontFeedMe)
{
  NS_ASSERTION(mObserver != nsnull , "have not init yet");

  if((nsnull == aBuf) || (nsnull == oDontFeedMe))
    return NS_ERROR_ILLEGAL_VALUE;

  this->Reset();
  nsresult rv = this->HandleData(aBuf, aLen);
  if (NS_FAILED(rv))
    return rv;

  if (mDone)
  {
    if (mDetectedCharset)
      Report(mDetectedCharset);

    *oDontFeedMe = PR_TRUE;
  }
  *oDontFeedMe = PR_FALSE;
  return NS_OK;
}
//----------------------------------------------------------
NS_IMETHODIMP nsXPCOMDetector::Done()
{
  NS_ASSERTION(mObserver != nsnull , "have not init yet");
#ifdef DEBUG_chardet
  for (PRInt32 i = 0; i < NUM_OF_CHARSET_PROBERS; i++)
  {
    // If no data was received the array might stay filled with nulls
    // the way it was initialized in the constructor.
    if (mCharSetProbers[i])
      mCharSetProbers[i]->DumpStatus();
  }
#endif

  this->DataEnd();
  return NS_OK;
}
//----------------------------------------------------------
void nsXPCOMDetector::Report(const char* aCharset)
{
  NS_ASSERTION(mObserver != nsnull , "have not init yet");
#ifdef DEBUG_chardet
  printf("Universal Charset Detector report charset %s . \r\n", aCharset);
#endif
  mObserver->Notify(aCharset, eBestAnswer);
}


//---------------------------------------------------------------------
nsXPCOMStringDetector:: nsXPCOMStringDetector(PRUint32 aLanguageFilter)
  : nsUniversalDetector(aLanguageFilter)
{
}
//---------------------------------------------------------------------
nsXPCOMStringDetector::~nsXPCOMStringDetector() 
{
}
//---------------------------------------------------------------------
NS_IMPL_ISUPPORTS1(nsXPCOMStringDetector, nsIStringCharsetDetector)
//---------------------------------------------------------------------
void nsXPCOMStringDetector::Report(const char *aCharset) 
{
  mResult = aCharset;
#ifdef DEBUG_chardet
  printf("New Charset Prober report charset %s . \r\n", aCharset);
#endif
}
//---------------------------------------------------------------------
NS_IMETHODIMP nsXPCOMStringDetector::DoIt(const char* aBuf,
                     PRUint32 aLen, const char** oCharset,
                     nsDetectionConfident &oConf)
{
  mResult = nsnull;
  this->Reset();
  nsresult rv = this->HandleData(aBuf, aLen); 
  if (NS_FAILED(rv))
    return rv;
  this->DataEnd();
  if (mResult)
  {
    *oCharset=mResult;
    oConf = eBestAnswer;
  }
  return NS_OK;
}
