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
/* PRTQUEUE *pq = PrnSelectPrinter(...)                                     */
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

#ifdef __cplusplus
extern "C" {
#endif

/* Library init and term; job properties per queue are cached during run.   */
BOOL PrnInitialize( HMODULE hmodResources);
BOOL PrnTerminate( void);

/* opaque type to describe a print queue (printer)                          */
class PRTQUEUE;

/* Select a printer.  If bQuiet is set, the default one is used; otherwise, */
/* a dialog is popped up to allow the user to choose.  Job properties       */
/* configuration can be done at this stage, from the dialog.                */
/* Returns null if use cancels.                                             */
PRTQUEUE *PrnSelectPrinter( HWND hwndOwner, BOOL bQuiet);

/* Release app. resources associated with a printer                         */
BOOL PrnClosePrinter( PRTQUEUE *pPrintQueue);

/* Display job-properties dialog.  Returns success.                         */
BOOL PrnDoJobProperties( PRTQUEUE *pPrintQueue);

/* Get a DC for the selected printer.  Must supply the application name.    */
HDC PrnOpenDC( PRTQUEUE *pPrintQueue, PSZ pszApplicationName);

/* Close the print DC; your PS should have been disassociated.              */
BOOL PrnCloseDC( HDC hdc);

/* Get the hardcopy caps for the selected form                              */
BOOL PrnQueryHardcopyCaps( HDC hdc, PHCINFO pHCInfo);

/* Begin a print job.  A PS should have been associated with the dc         */
/* (returned from PrnOpenDC()), a job name is required.                     */
BOOL PrnStartJob( HDC hdc, PSZ pszJobName);

/* End a print job previously started with PrnStartJob().                   */
BOOL PrnEndJob( HDC hdc);

/* Abort the current job started with PrnStartJob().                        */
BOOL PrnAbortJob( HDC hdc);

/* Make a page-break.                                                       */
BOOL PrnNewPage( HDC hdc);

#ifdef __cplusplus
}
#endif

#endif
