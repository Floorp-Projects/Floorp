/*
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is the Mozilla OS/2 libraries.
 *
 * The Initial Developer of the Original Code is John Fairhurst,
 * <john_fairhurst@iname.com>.  Portions created by John Fairhurst are
 * Copyright (C) 1999 John Fairhurst. All Rights Reserved.
 *
 * Contributor(s): 
 *
 */

/* (Over?) Simple printing API */

/****************************************************************************/
/* Example usage:                                                           */
/*                                                                          */
/* PRINTDLG pdlg;                                                           */
/* PRTQUEUE *pq = pdlg.SelectPrinter(...)                                   */
/* HDC       dcPrint = PrnOpenDC( pq, ...)                                  */
/*                                                                          */
/* GpiAssociate( hps, dcPrint)                                              */
/*                                                                          */
/* PrnStartJob( dcPrint, ...)                                               */
/* .                                                                        */
/* .                                                                        */
/* Gpi calls to `hps'                                                       */
/* .                                                                        */
/* .                                                                        */
/* PrnEndJob( dcPrint)                                                      */
/*                                                                          */
/* GpiAssociate( hps, NULLHANDLE)                                           */
/*                                                                          */
/* PrnCloseDC( dcPrint)                                                     */
/* PrnClosePrinter( pq)                                                     */
/****************************************************************************/

#ifndef _libprint_h
#define _libprint_h

#define INCL_PM
#define INCL_DOS
#define INCL_DOSERRORS
#define INCL_SPLDOSPRINT

#include <os2.h>


// Library init and term; job properties per queue are cached during run.
BOOL PrnInitialize (HMODULE hmodResources);
BOOL PrnTerminate (void);


// opaque type to describe a print queue (printer)
class PRTQUEUE;

#define MAX_PRINT_QUEUES  (128)


class PRINTDLG
{
  friend MRESULT EXPENTRY prnDlgProc (HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2);

public:
   PRINTDLG ();
  ~PRINTDLG ();
   PRTQUEUE* SelectPrinter (HWND hwndOwner, BOOL bQuiet);

private:
  HWND      mHwndListbox;
  HPOINTER  mHPointer;

  BOOL      mbSelected;
  ULONG     mDefaultQueue;
  ULONG     mSelectedQueue;
  ULONG     mQueueCount;
  PRTQUEUE* mPQBuf [MAX_PRINT_QUEUES];
};



// Release app. resources associated with a printer
BOOL PrnClosePrinter( PRTQUEUE *pPrintQueue);

// Display job-properties dialog.  Returns success.
BOOL PrnDoJobProperties( PRTQUEUE *pPrintQueue);

// Get a DC for the selected printer.  Must supply the application name.
HDC PrnOpenDC( PRTQUEUE *pPrintQueue, PSZ pszApplicationName);

// Close the print DC; your PS should have been disassociated.
BOOL PrnCloseDC( HDC hdc);

// Get the hardcopy caps for the selected form
BOOL PrnQueryHardcopyCaps( HDC hdc, PHCINFO pHCInfo);

// Begin a print job.  A PS should have been associated with the dc
// (returned from PrnOpenDC()), a job name is required.
BOOL PrnStartJob( HDC hdc, PSZ pszJobName);

// End a print job previously started with PrnStartJob().
BOOL PrnEndJob( HDC hdc);

// Abort the current job started with PrnStartJob().
BOOL PrnAbortJob( HDC hdc);

// Make a page-break.
BOOL PrnNewPage( HDC hdc);


#endif
