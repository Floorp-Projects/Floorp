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

#include <stdio.h> // DJ - only for debug
#include <stdlib.h>
#include <string.h>

#include "libprint.h"
#include "libprres.h"


static HMODULE hmodRes;
static BOOL prnEscape (HDC hdc, long lEscape);
MRESULT EXPENTRY prnDlgProc (HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2);



class PRTQUEUE
{
public:
   PRTQUEUE (const PRQINFO3* pPQI3)  { InitWithPQI3 (pPQI3); }
   PRTQUEUE (const PRTQUEUE& PQInfo) { InitWithPQI3 (&PQInfo.PQI3 ()); }
  ~PRTQUEUE (void) { free (mpPQI3); }

   PRQINFO3& PQI3 () const { return *mpPQI3; }
   const char* DriverName () const { return mDriverName; }
   const char* DeviceName () const { return mDeviceName; }
   const char* PrinterName() const { return mPrinterName; }
   const char* QueueName  () const { return mpPQI3->pszComment; }
   
private:
   PRTQUEUE& operator = (const PRTQUEUE& z);         // prevent copying
   void InitWithPQI3 (const PRQINFO3* pInfo);

   PRQINFO3* mpPQI3;
   CHAR  mDriverName  [DRIV_NAME_SIZE + 1];          // Driver name
   CHAR  mDeviceName  [DRIV_DEVICENAME_SIZE + 1];    // Device name
   CHAR  mPrinterName [PRINTERNAME_SIZE + 1];        // Printer name
};


void PRTQUEUE::InitWithPQI3 (const PRQINFO3* pInfo)
{
   // Make local copy of PPRQINFO3 object
   ULONG SizeNeeded;
   ::SplQueryQueue (NULL, pInfo->pszName, 3, NULL, 0, &SizeNeeded);
   mpPQI3 = (PRQINFO3*)malloc (SizeNeeded);
   ::SplQueryQueue (NULL, pInfo->pszName, 3, mpPQI3, SizeNeeded, &SizeNeeded);


   PCHAR sep = strchr (pInfo->pszDriverName, '.');

   if (sep)
   {
      *sep = '\0';
      strcpy (mDriverName, pInfo->pszDriverName);
      strcpy (mDeviceName, sep + 1);
      *sep = '.';
   } else
   {
      strcpy (mDriverName, pInfo->pszDriverName);
      mDeviceName [0] = '\0';
   }


   sep = strchr (pInfo->pszPrinters, ',');

   if (sep)
   {
      *sep = '\0';
      strcpy (mPrinterName, pInfo->pszPrinters);
      *sep = '.';
   } else
   {
      strcpy (mPrinterName, pInfo->pszPrinters);
   }
}


//===========================================================================

PRINTDLG::PRINTDLG ()
{
  mbSelected = FALSE;
  mDefaultQueue = 0;
  mSelectedQueue = 0;
  mQueueCount = 0;

  ULONG TotalQueues = 0;
  ULONG MemNeeded = 0;
  SPLERR rc;
  
  rc = ::SplEnumQueue (NULL, 3, NULL, 0, &mQueueCount, &TotalQueues, &MemNeeded, NULL);
  PRQINFO3* pPQI3Buf = (PRQINFO3*) malloc (MemNeeded);
  rc = ::SplEnumQueue (NULL, 3, pPQI3Buf, MemNeeded, &mQueueCount, &TotalQueues, &MemNeeded, NULL);

  if (mQueueCount > MAX_PRINT_QUEUES)
    mQueueCount = MAX_PRINT_QUEUES;

  for (ULONG cnt = 0 ; cnt < mQueueCount ; cnt++)
  {
    if (pPQI3Buf [cnt].fsType & PRQ3_TYPE_APPDEFAULT)
      mDefaultQueue = cnt;

    mPQBuf [cnt] = new PRTQUEUE (&pPQI3Buf [cnt]);
  }

  free (pPQI3Buf);
}

PRINTDLG::~PRINTDLG ()
{
  for (int cnt = 0 ; cnt < mQueueCount ; cnt++)
    delete mPQBuf [cnt];
}

PRTQUEUE* PRINTDLG::SelectPrinter (HWND hwndOwner, BOOL bQuiet)
{
  mbSelected = FALSE;
  PRTQUEUE* pPQ = NULL;

  if (mQueueCount == 0)
    return NULL;


  if (bQuiet)
  {
    pPQ = mPQBuf [mDefaultQueue];
  } else
  {
    ::WinDlgBox (HWND_DESKTOP, hwndOwner, prnDlgProc, hmodRes, IDD_PICKPRINTER, this);

    if (mbSelected)	// dialog procedure updated this
      pPQ = mPQBuf [mSelectedQueue];
    else
      return NULL;
  }

  mbSelected = TRUE;

  return new PRTQUEUE (*pPQ);
}



/****************************************************************************/
/*  Library-level data and functions                                        */
/****************************************************************************/

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

BOOL PrnClosePrinter( PRTQUEUE *pPrintQueue)
{
   BOOL rc = FALSE;

   if (pPrintQueue)
   {
      delete pPrintQueue;
      rc = TRUE;
   }

   return rc;
}


BOOL PrnDoJobProperties( PRTQUEUE *pInfo)
{
   BOOL  rc = FALSE;

   if( pInfo)
   {
      LONG lRC = ::DevPostDeviceModes( 0 /*hab*/,
                                       pInfo->PQI3 ().pDriverData,
                                       pInfo->DriverName (),
                                       pInfo->DeviceName (),
                                       pInfo->PrinterName (),
                                       DPDM_POSTJOBPROP);
   
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
      DEVOPENSTRUC dop;

      dop.pszLogAddress      = pInfo->PQI3 ().pszName;
      dop.pszDriverName      = (char*)pInfo->DriverName ();
      dop.pdriv              = pInfo->PQI3 ().pDriverData;
      dop.pszDataType        = "PM_Q_STD";
      dop.pszComment         = pszApplicationName;
      dop.pszQueueProcName   = pInfo->PQI3 ().pszPrProc;;     
      dop.pszQueueProcParams = 0;   
      dop.pszSpoolerParams   = 0;     
      dop.pszNetworkParams   = 0;     

      hdc = ::DevOpenDC( 0, OD_QUEUED, "*", 6, (PDEVOPENDATA) &dop, NULLHANDLE);

if (hdc == 0)
{
  ULONG ErrorCode = ERRORIDERROR (::WinGetLastError (0));
  printf ("!ERROR! - Can't open DC for printer %04X\a\n", ErrorCode);
}   
   }

   return hdc;
}

BOOL PrnCloseDC( HDC hdc)
{
   return (hdc != 0 && DEV_OK == ::DevCloseDC( hdc));
}

BOOL PrnStartJob( HDC hdc, PSZ pszJobName)
{
   BOOL rc = FALSE;

   if( hdc && pszJobName)
   {
      long lDummy = 0;
      long lResult = ::DevEscape( hdc, DEVESC_STARTDOC,
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
      long lResult = ::DevEscape( hdc, lEscape, 0, NULL, &lDummy, NULL);
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
      long   lResult = ::DevEscape( hdc, DEVESC_ENDDOC,
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
      lAvail = ::DevQueryHardcopyCaps( hdc, 0, 0, NULL);

      pBuffer = (PHCINFO) malloc( lAvail * sizeof(HCINFO));

      ::DevQueryHardcopyCaps( hdc, 0, lAvail, pBuffer);

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

#define PRM_JOBPROPERTIES  (WM_USER + 1)

MRESULT EXPENTRY prnDlgProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
   PRINTDLG* pPrintDlg = (PRINTDLG*)::WinQueryWindowPtr (hwnd, 0);

   switch (msg)
   {
      case WM_INITDLG:
         pPrintDlg = (PRINTDLG*)mp2;  // Get pointer to class. We passed it as parameter to WinDlgBox
         pPrintDlg->mHwndListbox = ::WinWindowFromID (hwnd, IDLB_QUEUES);
         pPrintDlg->mHPointer = ::WinLoadPointer (HWND_DESKTOP, hmodRes, IDICO_PRINTER);
         ::WinSetWindowPtr (hwnd, 0, pPrintDlg);
   
         /* set up the dialog */
//         ::WinSetPresParam (hwnd, PP_FONTNAMESIZE, 7, "8.Helv");
//         ::WinEnableWindow (::WinWindowFromID (hwnd, IDB_HELP), FALSE);
         ::WinShowWindow (::WinWindowFromID (hwnd, IDB_HELP), FALSE);
         ::WinSendMsg (hwnd, WM_SETICON, MPFROMLONG (pPrintDlg->mHPointer), 0);

         /* populate listbox */
         if (pPrintDlg->mQueueCount == 0)
         {
            ::WinEnableWindow (::WinWindowFromID (hwnd, DID_OK), FALSE);
            ::WinSendMsg (pPrintDlg->mHwndListbox, LM_INSERTITEM, MPFROMSHORT (0),
                          MPFROMP("(no printers available)"));
            ::WinEnableWindow (pPrintDlg->mHwndListbox, FALSE);
            ::WinEnableWindow (::WinWindowFromID (hwnd, IDB_JOBPROPERTIES), FALSE);
         }
         else
         {
            for (ULONG i = 0 ; i < pPrintDlg->mQueueCount ; i++)
            {
               ::WinSendMsg (pPrintDlg->mHwndListbox, LM_INSERTITEM, MPFROMSHORT (i),
                             MPFROMP (pPrintDlg->mPQBuf [i]->QueueName ()));

               if (i == pPrintDlg->mDefaultQueue)
                 ::WinSendMsg (pPrintDlg->mHwndListbox, LM_SELECTITEM, MPFROMSHORT (i), MPFROMSHORT (TRUE));
            }
         }
         // Center over owner
         {
            RECTL rclMe, rclOwner;
            LONG lX, lY;
            BOOL rc;
            ::WinQueryWindowRect (::WinQueryWindow (hwnd, QW_PARENT), &rclOwner);
            ::WinQueryWindowRect (hwnd, &rclMe);
            lX = (rclOwner.xRight - rclMe.xRight) / 2;
            lY = (rclOwner.yTop - rclMe.yTop) / 2;
            rc = ::WinSetWindowPos (hwnd, 0, lX, lY, 0, 0, SWP_MOVE);
         }
         break;

      case PRM_JOBPROPERTIES:
      {
         /* do job properties dialog for selected printer */
         pPrintDlg->mSelectedQueue = (ULONG)::WinSendMsg (pPrintDlg->mHwndListbox, LM_QUERYSELECTION, 0, 0);
         PrnDoJobProperties (pPrintDlg->mPQBuf [pPrintDlg->mSelectedQueue]);
         return 0;
      }

      case WM_CONTROL:
         if (SHORT2FROMMP (mp1) == LN_ENTER)
            return ::WinSendMsg (hwnd, PRM_JOBPROPERTIES, 0, 0);
         break;

      case WM_COMMAND:
         switch (SHORT1FROMMP (mp1))
         {
            case IDB_JOBPROPERTIES:
               ::WinSendMsg (hwnd, PRM_JOBPROPERTIES, 0, 0);
               return 0;

            case DID_OK:
            {
               pPrintDlg->mbSelected = TRUE;
               pPrintDlg->mSelectedQueue = (ULONG)::WinSendMsg (pPrintDlg->mHwndListbox, LM_QUERYSELECTION, 0, 0);
               break;
            }

            case DID_CANCEL:
            {
               pPrintDlg->mbSelected = FALSE;
               break;
            }

         }
         break;

   }

   return ::WinDefDlgProc (hwnd, msg, mp1, mp2);
}
