/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsIPageSequenceFrame_h___
#define nsIPageSequenceFrame_h___

#include "nsQueryFrame.h"
#include "nsRect.h"

class nsPresContext;
class nsIPrintSettings;
class nsITimerCallback;

/**
 * Interface for accessing special capabilities of the page sequence frame.
 *
 * Today all that exists are member functions for printing.
 */
class nsIPageSequenceFrame : public nsQueryFrame
{
public:
  NS_DECL_QUERYFRAME_TARGET(nsIPageSequenceFrame)

  /**
   * Print the set of pages.
   *
   * @param   aPrintOptions options for printing
   * @param   aStatusCallback interface that the client provides to receive
   *            progress notifications. Can be NULL
   * @return  NS_OK if successful
   *          NS_ERROR_ABORT if the client cancels printing using the callback
   *            interface
   *          NS_ERROR_INVALID_ARG if printing a range of pages (not all pages)
   *            and the start page is greater than the total number of pages
   *          NS_ERROR_FAILURE if there is an error
   */
  NS_IMETHOD StartPrint(nsPresContext*  aPresContext,
                        nsIPrintSettings* aPrintOptions,
                        PRUnichar* aDocTitle,
                        PRUnichar* aDocURL) = 0;

  NS_IMETHOD PrePrintNextPage(nsITimerCallback* aCallback, bool* aDone) = 0;
  NS_IMETHOD PrintNextPage() = 0;
  NS_IMETHOD ResetPrintCanvasList() = 0;
  NS_IMETHOD GetCurrentPageNum(int32_t* aPageNum) = 0;
  NS_IMETHOD GetNumPages(int32_t* aNumPages) = 0;
  NS_IMETHOD IsDoingPrintRange(bool* aDoing) = 0;
  NS_IMETHOD GetPrintRange(int32_t* aFromPage, int32_t* aToPage) = 0;

  NS_IMETHOD DoPageEnd() = 0;
  NS_IMETHOD SetSelectionHeight(nscoord aYOffset, nscoord aHeight) = 0;

  NS_IMETHOD SetTotalNumPages(int32_t aTotal) = 0;

  // For Shrink To Fit
  NS_IMETHOD GetSTFPercent(float& aSTFPercent) = 0;
};

#endif /* nsIPageSequenceFrame_h___ */


