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
#ifndef nsIPageSequenceFrame_h___
#define nsIPageSequenceFrame_h___

#include "nsISupports.h"
#include "nsRect.h"

class nsPresContext;
class nsIPrintSettings;

// IID for the nsIPageSequenceFrame interface 
// a6cf90d2-15b3-11d2-932e-00805f8add32
#define NS_IPAGESEQUENCEFRAME_IID \
 { 0xa6cf90d2, 0x15b3, 0x11d2,{0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32}}

//----------------------------------------------------------------------

/**
 * Interface for accessing special capabilities of the page sequence frame.
 *
 * Today all that exists are member functions for printing.
 */
class nsIPageSequenceFrame : public nsISupports {
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IPAGESEQUENCEFRAME_IID)

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
  NS_IMETHOD PrintNextPage(nsPresContext*  aPresContext) = 0;
  NS_IMETHOD GetCurrentPageNum(PRInt32* aPageNum) = 0;
  NS_IMETHOD GetNumPages(PRInt32* aNumPages) = 0;
  NS_IMETHOD IsDoingPrintRange(PRBool* aDoing) = 0;
  NS_IMETHOD GetPrintRange(PRInt32* aFromPage, PRInt32* aToPage) = 0;

  NS_IMETHOD SkipPageBegin() = 0;
  NS_IMETHOD SkipPageEnd() = 0;

  NS_IMETHOD DoPageEnd(nsPresContext* aPresContext) = 0;
  NS_IMETHOD GetPrintThisPage(PRBool* aPrintThisPage) = 0;
  NS_IMETHOD SetOffset(nscoord aX, nscoord aY) = 0;
  NS_IMETHOD SuppressHeadersAndFooters(PRBool aDoSup) = 0;
  NS_IMETHOD SetClipRect(nsPresContext*  aPresContext, nsRect* aSize) = 0;
  NS_IMETHOD SetSelectionHeight(nscoord aYOffset, nscoord aHeight) = 0;

  NS_IMETHOD SetTotalNumPages(PRInt32 aTotal) = 0;

  // Gets the dead space (the gray area) around the Print Preview Page
  NS_IMETHOD GetDeadSpaceValue(nscoord* aValue) = 0;

  // For Shrink To Fit
  NS_IMETHOD GetSTFPercent(float& aSTFPercent) = 0;
  
private:
  NS_IMETHOD_(nsrefcnt) AddRef(void) = 0;
  NS_IMETHOD_(nsrefcnt) Release(void) = 0;
};

#endif /* nsIPageSequenceFrame_h___ */


