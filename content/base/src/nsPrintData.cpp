/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsPrintData.h"

#include "nsIStringBundle.h"
#include "nsIServiceManager.h"
#include "nsReadableUtils.h"
#include "nsCRT.h"

#include "nsISelection.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDocShell.h"
#include "nsIURI.h"
#include "nsINodeInfo.h"

//-----------------------------------------------------
// PR LOGGING
#ifdef MOZ_LOGGING
#define FORCE_PR_LOG /* Allow logging in the release build */
#endif

#include "prlog.h"

#ifdef PR_LOGGING

#ifdef NS_DEBUG
// PR_LOGGING is force to always be on (even in release builds)
// but we only want some of it on,
//#define EXTENDED_DEBUG_PRINTING 
#endif

#define DUMP_LAYOUT_LEVEL 9 // this turns on the dumping of each doucment's layout info
static PRLogModuleInfo * kPrintingLogMod = PR_NewLogModule("printing");
#define PR_PL(_p1)  PR_LOG(kPrintingLogMod, PR_LOG_DEBUG, _p1);
#else
#define PRT_YESNO(_p)
#define PR_PL(_p1)
#endif

//---------------------------------------------------
//-- nsPrintData Class Impl
//---------------------------------------------------
nsPrintData::nsPrintData(ePrintDataType aType) :
  mType(aType), mPrintView(nsnull), mDebugFilePtr(nsnull), mPrintObject(nsnull), mSelectedPO(nsnull),
  mShowProgressDialog(PR_TRUE), mProgressDialogIsShown(PR_FALSE), mPrintDocList(nsnull), mIsIFrameSelected(PR_FALSE),
  mIsParentAFrameSet(PR_FALSE), mPrintingAsIsSubDoc(PR_FALSE), mOnStartSent(PR_FALSE),
  mIsAborted(PR_FALSE), mPreparingForPrint(PR_FALSE), mDocWasToBeDestroyed(PR_FALSE),
  mShrinkToFit(PR_FALSE), mPrintFrameType(nsIPrintSettings::kFramesAsIs), 
  mNumPrintableDocs(0), mNumDocsPrinted(0), mNumPrintablePages(0), mNumPagesPrinted(0),
  mShrinkRatio(1.0), mOrigDCScale(1.0), mOrigTextZoom(1.0), mOrigZoom(1.0), mPPEventListeners(NULL), 
  mBrandName(nsnull)
{

  nsCOMPtr<nsIStringBundle> brandBundle;
  nsCOMPtr<nsIStringBundleService> svc( do_GetService( NS_STRINGBUNDLE_CONTRACTID ) );
  if (svc) {
    svc->CreateBundle( "chrome://global/locale/brand.properties", getter_AddRefs( brandBundle ) );
    if (brandBundle) {
      brandBundle->GetStringFromName(NS_LITERAL_STRING("brandShortName").get(), &mBrandName );
    }
  }

  if (!mBrandName) {
    mBrandName = ToNewUnicode(NS_LITERAL_STRING("Mozilla Document"));
  }

}

nsPrintData::~nsPrintData()
{

  // Set the cached Zoom value back into the DC
  if (mPrintDC) {
    mPrintDC->SetTextZoom(mOrigTextZoom);
    mPrintDC->SetZoom(mOrigZoom);
  }

  // remove the event listeners
  if (mPPEventListeners) {
    mPPEventListeners->RemoveListeners();
    NS_RELEASE(mPPEventListeners);
  }

  // Only Send an OnEndPrinting if we have started printing
  if (mOnStartSent && mType != eIsPrintPreview) {
    OnEndPrinting();
  }

  if (mPrintDC && !mDebugFilePtr) {
    PR_PL(("****************** End Document ************************\n"));
    PR_PL(("\n"));
    PRBool isCancelled = PR_FALSE;
    mPrintSettings->GetIsCancelled(&isCancelled);

    nsresult rv = NS_OK;
    if (mType == eIsPrinting) {
      if (!isCancelled && !mIsAborted) {
        rv = mPrintDC->EndDocument();
      } else {
        rv = mPrintDC->AbortDocument();  
      }
      if (NS_FAILED(rv)) {
        // XXX nsPrintData::ShowPrintErrorDialog(rv);
      }
    }
  }

  delete mPrintObject;

  if (mPrintDocList != nsnull) {
    mPrintDocList->Clear();
    delete mPrintDocList;
  }

  if (mBrandName) {
    nsCRT::free(mBrandName);
  }

  for (PRInt32 i=0;i<mPrintProgressListeners.Count();i++) {
    nsIWebProgressListener* wpl = NS_STATIC_CAST(nsIWebProgressListener*, mPrintProgressListeners.ElementAt(i));
    NS_ASSERTION(wpl, "nsIWebProgressListener is NULL!");
    NS_RELEASE(wpl);
  }

}

void nsPrintData::OnStartPrinting()
{
  if (!mOnStartSent) {
    DoOnProgressChange(mPrintProgressListeners, 0, 0, PR_TRUE, nsIWebProgressListener::STATE_START|nsIWebProgressListener::STATE_IS_DOCUMENT);
    mOnStartSent = PR_TRUE;
  }
}

void nsPrintData::OnEndPrinting()
{
  DoOnProgressChange(mPrintProgressListeners, 100, 100, PR_TRUE, nsIWebProgressListener::STATE_STOP|nsIWebProgressListener::STATE_IS_DOCUMENT);
  if (mPrintProgress && mShowProgressDialog) {
    mPrintProgress->CloseProgressDialog(PR_TRUE);
  }
}

void
nsPrintData::DoOnProgressChange(nsVoidArray& aListeners,
                              PRInt32      aProgess,
                              PRInt32      aMaxProgress,
                              PRBool       aDoStartStop,
                              PRInt32      aFlag)
{
  if (aProgess == 0) return;

  for (PRInt32 i=0;i<aListeners.Count();i++) {
    nsIWebProgressListener* wpl = NS_STATIC_CAST(nsIWebProgressListener*, aListeners.ElementAt(i));
    NS_ASSERTION(wpl, "nsIWebProgressListener is NULL!");
    wpl->OnProgressChange(nsnull, nsnull, aProgess, aMaxProgress, aProgess, aMaxProgress);
    if (aDoStartStop) {
      wpl->OnStateChange(nsnull, nsnull, aFlag, 0);
    }
  }
}




