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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsPagePrintTimer.h"
#include "nsPrintEngine.h"
#include "nsIContentViewer.h"
#include "nsIServiceManager.h"

NS_IMPL_ISUPPORTS1(nsPagePrintTimer, nsITimerCallback)

nsPagePrintTimer::nsPagePrintTimer() :
  mDocViewerPrint(nsnull), 
  mPresContext(nsnull), 
  mPrintSettings(nsnull), 
  mDelay(0)
{
}

nsPagePrintTimer::~nsPagePrintTimer()
{
  if (mTimer) {
    mTimer->Cancel();
  }
  mPrintEngine->SetIsPrinting(PR_FALSE); // this will notify the DV also

  nsCOMPtr<nsIContentViewer> cv(do_QueryInterface(mDocViewerPrint));
  if (cv) {
    cv->Destroy();
  }
}

nsresult 
nsPagePrintTimer::StartTimer(PRBool aUseDelay)
{
  nsresult result;
  mTimer = do_CreateInstance("@mozilla.org/timer;1", &result);
  if (NS_FAILED(result)) {
    NS_WARNING("unable to start the timer");
  } else {
    mTimer->InitWithCallback(this, aUseDelay?mDelay:0, nsITimer::TYPE_ONE_SHOT);
  }
  return result;
}



// nsITimerCallback
NS_IMETHODIMP
nsPagePrintTimer::Notify(nsITimer *timer)
{
  if (mPresContext && mDocViewerPrint) {
    PRPackedBool initNewTimer = PR_TRUE;
    // Check to see if we are done
    // donePrinting will be true if it completed successfully or
    // if the printing was cancelled
    PRBool inRange;
    PRBool donePrinting = mPrintEngine->PrintPage(mPresContext, mPrintSettings, mPrintObj, inRange);
    if (donePrinting) {
      // now clean up print or print the next webshell
      if (mPrintEngine->DonePrintingPages(mPrintObj, NS_OK)) {
        initNewTimer = PR_FALSE;
      }
    }

    Stop();
    if (initNewTimer) {
      nsresult result = StartTimer(inRange);
      if (NS_FAILED(result)) {
        donePrinting = PR_TRUE;     // had a failure.. we are finished..
        mPrintEngine->SetIsPrinting(PR_FALSE);
      }
    }
  }
  return NS_OK;
}

void 
nsPagePrintTimer::Init(nsPrintEngine*          aPrintEngine,
                       nsIDocumentViewerPrint* aDocViewerPrint,
                       nsPresContext*         aPresContext,
                       nsIPrintSettings*       aPrintSettings,
                       nsPrintObject*          aPO,
                       PRUint32                aDelay)
{
  mPrintEngine     = aPrintEngine;
  mDocViewerPrint  = aDocViewerPrint;

  mPresContext     = aPresContext;
  mPrintSettings   = aPrintSettings;
  mPrintObj        = aPO;
  mDelay           = aDelay;
}

nsresult 
nsPagePrintTimer::Start(nsPrintEngine*          aPrintEngine,
                        nsIDocumentViewerPrint* aDocViewerPrint,
                        nsPresContext*         aPresContext,
                        nsIPrintSettings*       aPrintSettings,
                        nsPrintObject*          aPO,
                        PRUint32                aDelay)
{
  Init(aPrintEngine, aDocViewerPrint, aPresContext, aPrintSettings, aPO, aDelay);
  return StartTimer(PR_FALSE);
}


void  
nsPagePrintTimer::Stop()
{
  if (mTimer) {
    mTimer->Cancel();
    mTimer = nsnull;
  }
}

nsresult NS_NewPagePrintTimer(nsPagePrintTimer **aResult)
{

  NS_PRECONDITION(aResult, "null param");

  nsPagePrintTimer* result = new nsPagePrintTimer;

  if (!result) {
    *aResult = nsnull;
    return NS_ERROR_OUT_OF_MEMORY;
  }

  NS_ADDREF(result);
  *aResult = result;

  return NS_OK;
}

