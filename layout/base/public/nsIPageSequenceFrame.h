/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 */
#ifndef nsIPageSequenceFrame_h___
#define nsIPageSequenceFrame_h___

#include "nslayout.h"
#include "nsISupports.h"

class nsIPresContext;
class  nsIPrintOptions;

// IID for the nsIPageSequenceFrame interface 
// a6cf90d2-15b3-11d2-932e-00805f8add32
#define NS_IPAGESEQUENCEFRAME_IID \
 { 0xa6cf90d2, 0x15b3, 0x11d2,{0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32}}

// IID for the nsIPrintStatusCallback interface
// a6cf90d3-15b3-11d2-932e-00805f8add32
#define NS_IPRINTSTATUSCALLBACK_IID \
 { 0xa6cf90d3, 0x15b3, 0x11d2,{0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32}}

//----------------------------------------------------------------------

/**
 * List of status codes that provide additional information about the
 * progress of the print operation.
 * @see   nsIPrintStatusCallback#OnProgress()
 */
enum nsPrintStatus {
  ePrintStatus_StartPage,   // beginning the specified page
  ePrintStatus_EndPage      // finished with the specified page
};

/**
 * List of print error codes.
 * @see   nsIPrintStatusCallback##OnError()
 */
enum nsPrintError {
  ePrintError_Error,        // unspecified error
  ePrintError_Abort,        // operation was aborted by the user
  ePrintError_OutOfDisk,    // system is out of disk space
  ePrintError_OutOfMemory   // system is out of memory
};

/**
 * The page sequence frame provides information on the print operation by
 * calling notification methods on the client's nsIPrintStatusCallback
 * interface.
 */
class nsIPrintStatusCallback : public nsISupports {
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IPRINTSTATUSCALLBACK_IID)

  /**
   * Indicates the current progress of the print operation.
   *
   * @param   aPageNumber the number of the current page
   * @param   aTotalPages the total number of pages
   * @param   aStatusCode additional information regarding the progress
   * @param   aContinuePrinting return PR_TRUE to continue printing and
   *            PR_FALSE to cancel the printing operation
   */
  NS_IMETHOD  OnProgress(PRInt32       aPageNumber,
                         PRInt32       aTotalPages,
                         nsPrintStatus aStatusCode,
                         PRBool&       aContinuePrinting) = 0;

  /**
   * Notification that an error has occured.
   */
  NS_IMETHOD  OnError(nsPrintError aErrorCode) = 0;
};

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
   * @see     nsIPrintStatusCallback#OnProgress()
   */
  NS_IMETHOD Print(nsIPresContext*         aPresContext,
                   nsIPrintOptions*        aPrintOptions,
                   nsIPrintStatusCallback* aStatusCallback) = 0;

private:
  NS_IMETHOD_(nsrefcnt) AddRef(void) = 0;
  NS_IMETHOD_(nsrefcnt) Release(void) = 0;
};

#endif /* nsIPageSequenceFrame_h___ */


