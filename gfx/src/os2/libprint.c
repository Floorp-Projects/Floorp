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

/* Simple printing API.  I've not done this before, so bear with me... */

#define INCL_PM
#define INCL_DOS
#define INCL_DOSERRORS
#define INCL_SPLDOSPRINT
#include <os2.h>
#include <stdlib.h>
#include <string.h>

#include "libprint.h"
#include "libprres.h"

/* print-queue structure, opaque so I can change it. */
struct _PRTQUEUE
{
   PRQINFO3 q;
};

/* local functions */
static PRTQUEUE *prnGetDefaultPrinter( void);
static PRQINFO3 *prnGetAllQueues( PULONG pNumQueues);
static BOOL prnEscape( HDC hdc, long lEscape);
static PRTQUEUE *prnCreatePRTQUEUE( PRQINFO3 *pInfo);

MRESULT EXPENTRY prnDlgProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2);

/****************************************************************************/
/*  Library-level data and functions                                        */
/****************************************************************************/

static HMODULE hmodRes;

BOOL PrnInitialize( HMODULE hmodResources)
{
   hmodRes = hmodResources;
   return TRUE;
}

BOOL PrnTerminate()
{
   /* nop for now, may do something eventually */
   return TRUE;
}

/****************************************************************************/
/*  Printer Selection                                                       */
/****************************************************************************/

/* Main work function - run a dialog to pick a printer */
PRTQUEUE *PrnSelectPrinter( HWND hwndOwner, BOOL bQuiet)
{
   PRTQUEUE *pQueue = 0;

   if( bQuiet)
      pQueue = prnGetDefaultPrinter();
   else
   {
      ULONG rc = WinDlgBox( HWND_DESKTOP, hwndOwner,
                            prnDlgProc,
                            hmodRes, IDD_PICKPRINTER,
                            &pQueue);
      if( rc == DID_CANCEL)
         pQueue = 0;
   }

   return pQueue;
}

BOOL PrnClosePrinter( PRTQUEUE *pPrintQueue)
{
   BOOL rc = FALSE;

   if( pPrintQueue)
   {
      free( pPrintQueue);
      rc = TRUE;
   }

   return rc;
}

PRTQUEUE *prnGetDefaultPrinter()
{
   ULONG     ulQueues = 0;
   PRQINFO3 *pInfo = prnGetAllQueues( &ulQueues);
   PRTQUEUE *pq = 0;

   if( ulQueues > 0)
   {
      /* find the default one */
      ULONG i;
      for( i = 0; i < ulQueues; i++)
         if( pInfo[ i].fsType & PRQ3_TYPE_APPDEFAULT)
            break;

      /* must have a default printer... */
      if( i == ulQueues)
         i = 0;

      pq = prnCreatePRTQUEUE( pInfo + i);

      free( pInfo);
   }

   return pq;
}

PRTQUEUE *prnCreatePRTQUEUE( PRQINFO3 *pInfo)
{
   PRTQUEUE *pq = (PRTQUEUE *) malloc( sizeof( PRTQUEUE));
   memcpy( &pq->q, pInfo, sizeof( PRQINFO3));
   return pq;
}

PRQINFO3 *prnGetAllQueues( PULONG pNumQueues)
{
   ULONG  ulReturned = 0, ulNeeded = 0, ulTotal = 0;
   PBYTE  pBuffer = NULL;
   SPLERR rc = 0;

   /* first work out how much space we need */
   rc = SplEnumQueue( NULL, 3, pBuffer, 0, &ulReturned,
                      &ulTotal, &ulNeeded, NULL);

   pBuffer = (PBYTE) malloc( ulNeeded);

   /* now get the queue-infos */
   rc = SplEnumQueue( NULL, 3, pBuffer, ulNeeded, &ulReturned,
                      &ulTotal, &ulNeeded, NULL);

   *pNumQueues = ulReturned;

   return (PRQINFO3 *) pBuffer;
}

BOOL PrnDoJobProperties( PRTQUEUE *pInfo)
{
   BOOL  rc = FALSE;

   if( pInfo)
   {
      char *pszPrinter = strdup( pInfo->q.pszPrinters);
      char *pszDriverName = strdup( pInfo->q.pszDriverName);
      char *pszDeviceName = 0;
      char *c;
      long  lRC;
   
      if( 0 != (c = strchr( pszPrinter, ',')))
         *c = '\0';
   
      if( 0 != (c = strchr( pszDriverName, '.')))
      {
         *c = '\0';
         pszDeviceName = c + 1;
      }
   
      lRC = DevPostDeviceModes( 0 /*hab*/,
                                pInfo->q.pDriverData,
                                pszDriverName,
                                pszDeviceName,
                                pszPrinter,
                                DPDM_POSTJOBPROP);
      free( pszPrinter);
      free( pszDriverName);
   
      rc = (lRC != DPDM_ERROR);
   }

   return rc;
}

/****************************************************************************/
/*  Job management                                                          */
/****************************************************************************/

HDC PrnOpenDC( PRTQUEUE *pInfo, PSZ pszApplicationName)
{
   HDC hdc = 0;

   if( pInfo && pszApplicationName)
   {
      char *pszDriverName = strdup( pInfo->q.pszDriverName), *c;
      DEVOPENSTRUC dop = { pInfo->q.pszName,
                           pszDriverName,
                           pInfo->q.pDriverData,
                           "PM_Q_STD",
                           pszApplicationName,
                           pInfo->q.pszPrProc,
                           0, 0, 0 };
   
      if( 0 != (c = strchr( pszDriverName, '.')))
         *c = '\0';
   
      hdc = DevOpenDC( 0 /*hab*/,
                       OD_QUEUED,
                       "*",
                       6,
                       (PDEVOPENDATA) &dop,
                       NULLHANDLE);
   
      free( pszDriverName);
   }

   return hdc;
}

BOOL PrnCloseDC( HDC hdc)
{
   return (hdc != 0 && DEV_OK == DevCloseDC( hdc));
}

BOOL PrnStartJob( HDC hdc, PSZ pszJobName)
{
   BOOL rc = FALSE;

   if( hdc && pszJobName)
   {
      long lDummy = 0;
      long lResult = DevEscape( hdc, DEVESC_STARTDOC,
                                (long) strlen( pszJobName) + 1, pszJobName,
                                &lDummy, NULL);
      rc = (lResult == DEV_OK);
   }

   return rc;
}

BOOL PrnNewPage( HDC hdc)
{
   return prnEscape( hdc, DEVESC_NEWFRAME);
}

BOOL prnEscape( HDC hdc, long lEscape)
{
   BOOL rc = FALSE;

   if( hdc)
   {
      long lDummy = 0;
      long lResult = DevEscape( hdc, lEscape,
                                0, NULL, &lDummy, NULL);
      rc = (lResult == DEV_OK);
   }

   return rc;
}

BOOL PrnAbortJob( HDC hdc)
{
   return prnEscape( hdc, DEVESC_ABORTDOC);
}

BOOL PrnEndJob( HDC hdc)
{
   BOOL rc = FALSE;

   if( hdc)
   {
      long   lOutCount = 2;
      USHORT usJobID = 0;
      long   lResult = DevEscape( hdc, DEVESC_ENDDOC,
                                  0, NULL, &lOutCount, (PBYTE) &usJobID);
      rc = (lResult == DEV_OK);
   }

   return rc;
}

/* find the selected form */
BOOL PrnQueryHardcopyCaps( HDC hdc, PHCINFO pHCInfo)
{
   BOOL rc = FALSE;

   if( hdc && pHCInfo)
   {
      PHCINFO pBuffer;
      long    lAvail, i;

      /* query how many forms are available */
      lAvail = DevQueryHardcopyCaps( hdc, 0, 0, NULL);

      pBuffer = (PHCINFO) malloc( lAvail * sizeof(HCINFO));

      DevQueryHardcopyCaps( hdc, 0, lAvail, pBuffer);

      for( i = 0; i < lAvail; i++)
         if( pBuffer[ i].flAttributes & HCAPS_CURRENT)
         {
            memcpy( pHCInfo, pBuffer + i, sizeof(HCINFO));
            rc = TRUE;
            break;
         }

      free( pBuffer);
   }

   return rc;
}

/****************************************************************************/
/*  Print dialog stuff                                                      */
/****************************************************************************/

typedef struct _PRNDLG
{
   HWND       hwndLbox;
   PRQINFO3  *pQueues;
   ULONG      ulQueues;
   HPOINTER   hPointer;
   PRTQUEUE **ppQueue;
} PRNDLG, *PPRNDLG;

#define PRM_QUERYQUEUE     (WM_USER)  /* returns PRQINFO3* */
#define PRM_JOBPROPERTIES  (WM_USER + 1)

MRESULT EXPENTRY prnDlgProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
   PPRNDLG pData = (PPRNDLG) WinQueryWindowPtr( hwnd, 0);

   switch( msg)
   {
      case WM_INITDLG:
         /* set up our data structure */
         pData = (PPRNDLG) malloc( sizeof( PRNDLG));
         pData->pQueues = prnGetAllQueues( &pData->ulQueues);
         pData->hwndLbox = WinWindowFromID( hwnd, IDLB_QUEUES);
         pData->hPointer = WinLoadPointer( HWND_DESKTOP, hmodRes, IDICO_PRINTER);
         pData->ppQueue = (PRTQUEUE**) mp2;
         WinSetWindowPtr( hwnd, 0, pData);
   
         /* set up the dialog */
         WinSetPresParam( hwnd, PP_FONTNAMESIZE, 7, "8.Helv");
         WinEnableWindow( WinWindowFromID( hwnd, IDB_HELP), FALSE);
         WinSendMsg( hwnd, WM_SETICON, MPFROMLONG(pData->hPointer), 0);

         /* populate listbox */
         if( pData->ulQueues == 0)
         {
            WinEnableWindow( WinWindowFromID( hwnd, DID_OK), FALSE);
            WinSendMsg( pData->hwndLbox, LM_INSERTITEM, MPFROMSHORT(0),
                        MPFROMP("(no printers available)"));
            WinEnableWindow( pData->hwndLbox, FALSE);
            WinEnableWindow( WinWindowFromID( hwnd, IDB_JOBPROPERTIES), FALSE);
         }
         else
         {
            ULONG i;
   
            for( i = 0; i < pData->ulQueues; i++)
            {
               WinSendMsg( pData->hwndLbox, LM_INSERTITEM, MPFROMSHORT(i),
                           MPFROMP( pData->pQueues[i].pszComment));
               WinSendMsg( pData->hwndLbox, LM_SETITEMHANDLE, MPFROMSHORT(i),
                           MPFROMP( pData->pQueues + i));
               if( pData->pQueues[i].fsType & PRQ3_TYPE_APPDEFAULT)
                  WinSendMsg( pData->hwndLbox, LM_SELECTITEM,
                              MPFROMSHORT(i), MPFROMSHORT(TRUE));
            }
         }
         // Center over owner
         {
            RECTL rclMe, rclOwner;
            LONG lX, lY;
            BOOL rc;
            WinQueryWindowRect( WinQueryWindow( hwnd, QW_PARENT), &rclOwner);
            WinQueryWindowRect( hwnd, &rclMe);
            lX = (rclOwner.xRight - rclMe.xRight) / 2;
            lY = (rclOwner.yTop - rclMe.yTop) / 2;
            rc = WinSetWindowPos( hwnd, 0, lX, lY, 0, 0, SWP_MOVE);
         }
         break;

      case PRM_QUERYQUEUE:
      {
         /* find which lbox item is selected & return its handle */
         MRESULT sel;
         sel = WinSendMsg( pData->hwndLbox, LM_QUERYSELECTION, 0, 0);
         return WinSendMsg( pData->hwndLbox, LM_QUERYITEMHANDLE, sel, 0);
      }

      case PRM_JOBPROPERTIES:
      {
         /* do job properties dialog for selected printer */
         PPRQINFO3 pInfo;
         pInfo = (PPRQINFO3) WinSendMsg( hwnd, PRM_QUERYQUEUE, 0, 0);
         PrnDoJobProperties( (PRTQUEUE*) pInfo); /* !! */
         return 0;
      }

      case WM_CONTROL:
         if( SHORT2FROMMP(mp1) == LN_ENTER)
            return WinSendMsg( hwnd, PRM_JOBPROPERTIES, 0, 0);
         break;

      case WM_COMMAND:
         switch( SHORT1FROMMP(mp1))
         {
            case IDB_JOBPROPERTIES:
               WinSendMsg( hwnd, PRM_JOBPROPERTIES, 0, 0);
               return 0;

            case DID_OK:
            {
               /* set return value */
               PPRQINFO3 pInfo;
               pInfo = (PPRQINFO3) WinSendMsg( hwnd, PRM_QUERYQUEUE, 0, 0);
               *(pData->ppQueue) = prnCreatePRTQUEUE( pInfo);
               break; /* dismiss dialog normally */
            }
         }
         break;

      case WM_DESTROY:
      {
         if( pData->pQueues)
            free( pData->pQueues);
         free( pData);
         break;
      }
   }

   return WinDefDlgProc( hwnd, msg, mp1, mp2);
}
