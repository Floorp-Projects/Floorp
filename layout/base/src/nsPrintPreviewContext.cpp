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
#include "nsPresContext.h"
#include "nsIDeviceContext.h"
#include "nsUnitConversion.h"
#include "nsIView.h"
#include "nsIWidget.h"
#include "nsGfxCIID.h"
#include "nsLayoutAtoms.h"
#include "prlog.h"

// Print Options
#include "nsIPrintOptions.h"
#include "nsGfxCIID.h"
#include "nsIServiceManager.h"
static NS_DEFINE_CID(kPrintOptionsCID, NS_PRINTOPTIONS_CID);

class PrintPreviewContext : public nsPresContext {
public:
  PrintPreviewContext();
  ~PrintPreviewContext();

  NS_IMETHOD GetMedium(nsIAtom** aMedium);
  NS_IMETHOD IsPaginated(PRBool* aResult);
  NS_IMETHOD GetPageDim(nsRect* aActualRect, nsRect* aAdjRect);
  NS_IMETHOD SetPageDim(nsRect* aRect);

#ifdef NS_DEBUG
  static PRBool UseFakePageSize();
#endif
protected:
  nsRect mPageDim;
};

#ifdef NS_DEBUG
PRBool
PrintPreviewContext::UseFakePageSize()
{
  static PRLogModuleInfo* pageSizeLM;
  static PRBool useFakePageSize = PR_FALSE;
  if (nsnull == pageSizeLM) {
    pageSizeLM = PR_NewLogModule("pagesize");
    if (nsnull != pageSizeLM) {
      useFakePageSize = 0 != pageSizeLM->level;
    }
  }
  return useFakePageSize;
}
#endif

PrintPreviewContext::PrintPreviewContext() :
  mPageDim(-1,-1,-1,-1)
{
}

PrintPreviewContext::~PrintPreviewContext()
{
}

NS_IMETHODIMP
PrintPreviewContext::GetMedium(nsIAtom** aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = nsLayoutAtoms::print;
  NS_ADDREF(*aResult);
  return NS_OK;
}

NS_IMETHODIMP
PrintPreviewContext::IsPaginated(PRBool* aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP
PrintPreviewContext::GetPageDim(nsRect* aActualRect, nsRect* aAdjRect)
{
  NS_ENSURE_ARG_POINTER(aActualRect);
  NS_ENSURE_ARG_POINTER(aAdjRect);

  // XXX maybe we get the size of the default printer instead
  nsresult rv;
  nsCOMPtr<nsIPrintOptions> printService = 
           do_GetService(kPrintOptionsCID, &rv);
  // Setting what would be the "default" case here, because
  // getting the PrintService could fail
  aActualRect->width  = (nscoord) NS_INCHES_TO_TWIPS(8.5);
  aActualRect->height = (nscoord) NS_INCHES_TO_TWIPS(11);
  if (NS_SUCCEEDED(rv) && printService) {
    PRInt32 paperSize = nsIPrintOptions::kLetterPaperSize; 
    printService->GetPaperSize(&paperSize);
    switch (paperSize) {
      case nsIPrintOptions::kLegalPaperSize :
        aActualRect->width  = (nscoord) NS_INCHES_TO_TWIPS(8.5);
        aActualRect->height = (nscoord) NS_INCHES_TO_TWIPS(14);
        break;

      case nsIPrintOptions::kExecutivePaperSize :
        aActualRect->width  = (nscoord) NS_INCHES_TO_TWIPS(7.5);
        aActualRect->height = (nscoord) NS_INCHES_TO_TWIPS(10.5);
        break;

      case nsIPrintOptions::kA3PaperSize :
        aActualRect->width  = (nscoord) NS_MILLIMETERS_TO_TWIPS(297);
        aActualRect->height = (nscoord) NS_MILLIMETERS_TO_TWIPS(420);
        break;

      case nsIPrintOptions::kA4PaperSize :
        aActualRect->width  = (nscoord) NS_MILLIMETERS_TO_TWIPS(210);
        aActualRect->height = (nscoord) NS_MILLIMETERS_TO_TWIPS(297);
        break;

    } // switch 
    PRInt32 orientation = nsIPrintOptions::kPortraitOrientation;
    printService->GetOrientation(&orientation);
    if (orientation == nsIPrintOptions::kLandscapeOrientation) {
      // swap
      nscoord temp;
      temp = aActualRect->width;
      aActualRect->width = aActualRect->height;
      aActualRect->height = temp;
    }
  }

#ifdef NS_DEBUG
  if (UseFakePageSize()) {
    // For testing purposes make the page width smaller than the visible area
    float sbWidth, sbHeight;
    mDeviceContext->GetScrollBarDimensions(sbWidth, sbHeight);
    nscoord sbar = NSToCoordRound(sbWidth);
    aActualRect->width  = mVisibleArea.width - sbar - 2*100;
    aActualRect->height = mVisibleArea.height * 60 / 100;
  }
#endif
  *aAdjRect = mPageDim;

  return NS_OK;
}

NS_IMETHODIMP
PrintPreviewContext::SetPageDim(nsRect* aPageDim)
{
  NS_ENSURE_ARG_POINTER(aPageDim);
  mPageDim = *aPageDim;
  return NS_OK;
}

NS_LAYOUT nsresult
NS_NewPrintPreviewContext(nsIPresContext** aInstancePtrResult)
{
  if (aInstancePtrResult == nsnull) {
    return NS_ERROR_NULL_POINTER;
  }

  PrintPreviewContext *it = new PrintPreviewContext();

  if (it == nsnull) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return it->QueryInterface(NS_GET_IID(nsIPresContext), (void **) aInstancePtrResult);
}
