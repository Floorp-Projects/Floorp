/***************************************************************************
 *
 * EXPORTED FUNCTIONS Conduit Entry Points Source File
 *
 **************************************************************************/
#define STRICT 1
#define ASSERT(f)          ((void)0)
#define TRACE0(sz)
#define TRACE(sz)

#include <malloc.h>


#include <CPalmRec.cpp>
#include <CPString.cpp>
#include <CPCatgry.cpp>

#include <windows.h>
#include <string.h>
#include <stdio.h>
#ifdef METROWERKS_WIN
#include <wmem.h>
#else
#include <memory.h>
#endif
#include <sys/stat.h>
#include <TCHAR.H>
#include <COMMCTRL.H>

#include <syncmgr.h>
#include "MozABConduitGenCond.h"
#include "resource.h"

#include <logstrng.h>

// TODO - Include custom sync header

#include <PALM_CMN.H>
#define CONDUIT_NAME "MozABConduit"
#include "MozABConduitSync.h"

HANDLE hLangInstance;
HANDLE hAppInstance;
extern HANDLE hLangInstance;
extern HANDLE hAppInstance;

long CALLBACK CondCfgDlgProc(HWND hWnd, UINT Message, WPARAM wParam, 
                             LPARAM lParam);
void LdCfgDlgBmps(HWND hDlg);

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////
//
//     Function:        DllMain()
//
//     Description:    main entry point to the MozABConduitProto component
//
//     Parameters:    hInstance - instance handle of the DLL
//                    dwReason  - why the entry point was called
//                    lpReserved - reserved
//
//     Returns:        1 if okay
//
/////////////////////////////////////////////////////////////////////////////
#ifndef _68K_

static int iTerminationCount = 0;

DWORD tId = 0;


extern "C" int APIENTRY
DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved)
{


    if (dwReason == DLL_PROCESS_ATTACH)
    {
        TRACE0("EXPORTED FUNCTIONS Initializing!\n");
        
        if (!iTerminationCount ) {
            hAppInstance = hInstance;
            // use PalmLoadLanguage here to load different languages
            hLangInstance = hInstance;
        }
        ++iTerminationCount;

        tId = TlsAlloc();
        if (tId == 0xFFFFFFFF)
            return FALSE;
    }
    else if (dwReason == DLL_PROCESS_DETACH)
    {
        TRACE0("EXPORTED FUNCTIONS Terminating!\n");

        --iTerminationCount;
        if (!iTerminationCount ) {
            // use PalmFreeLanguage here to unload different languages
        }
        TlsFree(tId);
    }
    return 1;   // ok
}
#endif


/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////
//
//     Function:        OpenConduit()
//
//     Description:  Extern "C" entry point into this conduit which starts 
//                   the process of synchronizing the local database table 
//                   with a remote conterpart residing on the remote view 
//                   device. 
//
//     Parameters:   Pointer to a callback function used to report progress.
//                    
//                
//
//     Returns:        
//
/////////////////////////////////////////////////////////////////////////////
ExportFunc long OpenConduit(PROGRESSFN pFn, CSyncProperties& rProps)
{
    long retval = -1;
    if (pFn)
    {
       CMozABConduitSync * pABSync = new CMozABConduitSync(rProps);
        if (pABSync)
		{
            retval = pABSync->Perform();
            delete pABSync;
        }
    }
    return(retval);
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////
//
//       Function:              GetConduitName()
//
//       Description:  Extern "C" entry point into this conduit which returns
//                                the name to be used when display messages 
//                                regarding this conduit.
//
//       Parameters:   pszName - buffer in which to place the name
//                                 nLen - maximum number of bytes of buffer     
//                                      
//                              
//
//       Returns:          -1 indicates erros
//
/////////////////////////////////////////////////////////////////////////////
ExportFunc long GetConduitName(char* pszName,WORD nLen)
{
    strncpy(pszName,CONDUIT_NAME, nLen-1);   
    *(pszName+nLen-1) = '\0';

    return 0;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////
//
//       Function:     GetConduitVersion()
//
//       Description:  Extern "C" entry point into this conduit which returns
//                     the conduits version
//
//       Parameters:   none
//
//       Returns:      DWORD indicating major and minor version number
//                        HIWORD - reserved
//                        HIBYTE(LOWORD) - major number
//                        LOBYTE(LOWORD) - minor number
//
/////////////////////////////////////////////////////////////////////////////
ExportFunc DWORD GetConduitVersion()
{
    return GENERIC_CONDUIT_VERSION;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////
//
//       Function:     ConfigureConduit
//
//       Description:  Extern "C" entry point into this conduit which returns
//                     should display the UI necessary to configure this 
//                     conduit.
//
//       Parameters:   none
//
//       Returns:      0 - success, !0 - failure
//
/////////////////////////////////////////////////////////////////////////////
ExportFunc long ConfigureConduit(CSyncPreference& pref)
{

    long nRtn = -1;
    CfgConduitInfoType cfgcond;
    cfgcond.dwVersion = CFGCONDUITINFO_VERSION_1;
    cfgcond.dwSize  = sizeof(CfgConduitInfoType);
    cfgcond.dwCreatorId = 0;
    cfgcond.dwUserId = 0;
    memset(cfgcond.szUser , 0, sizeof(cfgcond.szUser));
    memset(cfgcond.m_PathName, 0, sizeof(cfgcond.m_PathName)); 
    cfgcond.syncPermanent = pref.m_SyncType;
    cfgcond.syncTemporary = pref.m_SyncType;
    cfgcond.syncNew = pref.m_SyncType;
    cfgcond.syncPref = eTemporaryPreference; 

    int irv;
    irv = DialogBoxParam((HINSTANCE)hLangInstance, 
              MAKEINTRESOURCE(IDD_CONDUIT_ACTION), 
              GetForegroundWindow(), 
              (DLGPROC)CondCfgDlgProc,
              (LPARAM)&cfgcond);
    if (irv == 0) 
	{
        pref.m_SyncType = cfgcond.syncNew;
        pref.m_SyncPref = cfgcond.syncPref;
        nRtn = 0;
    }

    return nRtn;
}

/////////////////////////////////////////////////////////////////////////////
// 
/////////////////////////////////////////////////////////////////////////////
//
//   Method:        GetConduitInfo
//
//   Description:   This function provides a way for a Conduit to provide 
//                  info to the caller.  In this version of the call, MFC 
//                  Version, Conduit Name, and Default sync action are the 
//                  types of information this call will return.
//
//   Parameters:    ConduitInfoEnum infoType - enum specifying what info is 
//                                             being requested.
//
//                  void *pInArgs - This parameter may be null, except for 
//                                  the Conduit name enum, this value will 
//                                  be a ConduitRequestInfoType structure.
//
//                  This following two parameters vary depending upon the 
//                  info being requested. 
//
//                  For enum eConduitName
//                  void *pOut - will be a pointer to a character buffer
//                  DWORD *pdwOutSize - will be a pointer to a DWORD 
//                                      specifying the size of the 
//                                      character buffer.
//
//                  For enum eMfcVersion
//                  void *pOut - will be a pointer to a DWORD
//                  DWORD *pdwOutSize - will be a pointer to a DWORD 
//                                      specifying the size of pOut.
//
//                  For enum eDefaultAction
//                  void *pOut - will be a pointer to a eSyncType variable
//                  DWORD *pdwOutSize - will be a pointer to a DWORD 
//                                      specifying the size of pOut.
//
//   Returns:       0   - Success.
//                  !0  - error code.
//
/////////////////////////////////////////////////////////////////////////////
ExportFunc long GetConduitInfo(ConduitInfoEnum infoType, void *pInArgs, 
                               void *pOut, DWORD *pdwOutSize)
{
    if (!pOut)
        return CONDERR_INVALID_PTR;
    if (!pdwOutSize)
        return CONDERR_INVALID_OUTSIZE_PTR;

    switch (infoType) 
	{
        case eConduitName:

            // This code is for example. This conduit does not use this code
            
            if (!pInArgs)
                return CONDERR_INVALID_INARGS_PTR;
            ConduitRequestInfoType *pInfo;
            pInfo = (ConduitRequestInfoType *)pInArgs;
            if ((pInfo->dwVersion != CONDUITREQUESTINFO_VERSION_1) ||
                (pInfo->dwSize != SZ_CONDUITREQUESTINFO))
                return CONDERR_INVALID_INARGS_STRUCT;
           
                pOut = CONDUIT_NAME;
                return CONDERR_CONDUIT_RESOURCE_FAILURE;
            break;
        case eDefaultAction:
            if (*pdwOutSize != sizeof(eSyncTypes))
                return CONDERR_INVALID_BUFFER_SIZE;
            (*(eSyncTypes*)pOut) = eFast;
            break;
        case eMfcVersion:
            if (*pdwOutSize != sizeof(DWORD))
                return CONDERR_INVALID_BUFFER_SIZE;
            (*(DWORD*)pOut) = MFC_NOT_USED;
            break;
        default:
            return CONDERR_UNSUPPORTED_CONDUITINFO_ENUM;
    }
    return 0;
}

long CALLBACK CondCfgDlgProc(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
    static CfgConduitInfoType *pCfgInfo;
    TCHAR szPath[256];

    switch (Message) 
	{
      case WM_INITDIALOG:

        if (lParam != 0) {
          TCHAR szBuffer[256];
          TCHAR szBuf2[256];
          LoadString((HINSTANCE)hLangInstance, IDS_SYNC_ACTION_TEXT, szBuffer, 
                     sizeof(szBuffer));
          SetDlgItemText(hWnd, IDC_ACTIONGROUPBOXTEXT, szBuffer);

          LoadString((HINSTANCE)hLangInstance, IDS_CONDUIT_NAME, szBuffer, 
                     sizeof(szBuffer));
          SetWindowText(hWnd, szBuffer);
     
          // Load the bitmaps properly
          LdCfgDlgBmps(hWnd);

          pCfgInfo = (CfgConduitInfoType *)lParam;
          switch (pCfgInfo->syncTemporary)
		  {
            case eFast:
            case eSlow:
              CheckRadioButton(hWnd, IDC_RADIO_SYNC, IDC_RADIO_DONOTHING, 
                               IDC_RADIO_SYNC);
              LoadString((HINSTANCE)hLangInstance, IDS_SYNC_FILES, szBuffer, 
                         sizeof(szBuffer));
              break;
            case ePCtoHH:
              CheckRadioButton( hWnd, IDC_RADIO_SYNC, IDC_RADIO_DONOTHING, IDC_RADIO_PCTOHH);
              LoadString((HINSTANCE)hLangInstance, IDS_PCTOHH, szBuffer, sizeof(szBuffer));
              break;
            case eHHtoPC:
              CheckRadioButton( hWnd, IDC_RADIO_SYNC, IDC_RADIO_DONOTHING, IDC_RADIO_HHTOPC);
              LoadString((HINSTANCE)hLangInstance, IDS_HHTOPC, szBuffer, sizeof(szBuffer));
              break;
            case eDoNothing:
            default:
              CheckRadioButton(hWnd, IDC_RADIO_SYNC, IDC_RADIO_DONOTHING, 
                               IDC_RADIO_DONOTHING);
              LoadString((HINSTANCE)hLangInstance, IDS_DO_NOTHING, szBuffer,
                         sizeof(szBuffer));
              break;
          }
          // did we get called from the old config call or the new cfg call?
          if (pCfgInfo->dwCreatorId != 0) 
		  {
            SetDlgItemText(hWnd, IDC_STATIC_TEMPORARY, szBuffer);

            switch (pCfgInfo->syncPermanent)
			{
              case eFast:
              case eSlow:
                LoadString((HINSTANCE)hLangInstance, IDS_SYNC_FILES, szBuffer,
                           sizeof(szBuffer));
                break;
              case ePCtoHH:
                LoadString((HINSTANCE)hLangInstance, IDS_PCTOHH, szBuffer,
                           sizeof(szBuffer));
                break;

              case eHHtoPC:
                LoadString((HINSTANCE)hLangInstance, IDS_HHTOPC, szBuffer,
                           sizeof(szBuffer));
                break;
              case eDoNothing:
              default:
                LoadString((HINSTANCE)hLangInstance, IDS_DO_NOTHING, szBuffer,
                           sizeof(szBuffer));
                break;
            }
            SetDlgItemText(hWnd, IDC_STATIC_PERMANENT, szBuffer);

            LoadString((HINSTANCE)hLangInstance, IDS_CURRENT_SETTINGS_GROUP, 
                       szBuffer, sizeof(szBuffer));
            wsprintf(szBuf2, szBuffer, pCfgInfo->szUser);
            SetDlgItemText(hWnd, IDC_CURRENT_SETTINGS_GROUP, szBuf2);
          }
        }
        break;
      case WM_COMMAND:
        switch (wParam) 
		{
          case IDC_RADIO_SYNC:
          case IDC_RADIO_DONOTHING:
          case IDC_RADIO_HHTOPC:
          case IDC_RADIO_PCTOHH:
            CheckRadioButton( hWnd, IDC_RADIO_SYNC, IDC_RADIO_DONOTHING, wParam);
            break;
          case IDCANCEL:
            EndDialog(hWnd, 1);
            return TRUE;
          case IDOK:
            if (IsDlgButtonChecked(hWnd, IDC_RADIO_SYNC)) 
                pCfgInfo->syncNew = eFast;
            else if (IsDlgButtonChecked(hWnd, IDC_RADIO_PCTOHH)) 
                pCfgInfo->syncNew = ePCtoHH;
            else if (IsDlgButtonChecked(hWnd, IDC_RADIO_HHTOPC))
                pCfgInfo->syncNew = eHHtoPC;
            else
                pCfgInfo->syncNew = eDoNothing;
            pCfgInfo->syncPref = (IsDlgButtonChecked(hWnd, IDC_MAKEDEFAULT))
				?  ePermanentPreference : eTemporaryPreference;
            EndDialog(hWnd, 0);
            return TRUE;
          default:
            break;
        }
        break;

      case WM_SYSCOLORCHANGE:
        LdCfgDlgBmps(hWnd);
        break;

      default:
        break;
    }
    return FALSE;
}

void LdCfgDlgBmps(HWND hDlg)
{
  COLORMAP    cMap;
  HWND        hwndButton;
  HBITMAP     hBmp, hOldBmp;

  //
  // setup the bitmaps
  //
  cMap.to = GetSysColor(COLOR_BTNFACE);
  cMap.from = RGB(192,192,192);

  // Sync 
  hBmp = CreateMappedBitmap((HINSTANCE)hLangInstance, IDB_SYNC, 0, &cMap, 1);
  // associate the bitmap with the button.
  if ((hwndButton = GetDlgItem(hDlg, IDC_SYNC)) != NULL)
  {
    hOldBmp = (HBITMAP)SendMessage(hwndButton, STM_SETIMAGE,
                                   (WPARAM)IMAGE_BITMAP, (LPARAM)(HANDLE)hBmp);
    if (hOldBmp != NULL)
      DeleteObject((HGDIOBJ)hOldBmp);
  }

  // Do Nothing 
  hBmp = CreateMappedBitmap((HINSTANCE)hLangInstance, IDB_DONOTHING, 0, &cMap, 1);
  // associate the bitmap with the button.
  if ((hwndButton = GetDlgItem(hDlg, IDC_DONOTHING)) != NULL)
  {
    hOldBmp = (HBITMAP)SendMessage(hwndButton, STM_SETIMAGE,
                                   (WPARAM)IMAGE_BITMAP, (LPARAM)(HANDLE)hBmp);
    if (hOldBmp != NULL)
      DeleteObject((HGDIOBJ)hOldBmp);
  }
  hBmp = CreateMappedBitmap((HINSTANCE)hLangInstance, IDB_PCTOHH, 0, &cMap, 1);
  // associate the bitmap with the button.
  if ((hwndButton = GetDlgItem(hDlg, IDC_PCTOHH)) != NULL)
  {
    hOldBmp = (HBITMAP)SendMessage(hwndButton, STM_SETIMAGE,
                                   (WPARAM)IMAGE_BITMAP, (LPARAM)(HANDLE)hBmp);
    if (hOldBmp != NULL)
      DeleteObject((HGDIOBJ)hOldBmp);
  }
  hBmp = CreateMappedBitmap((HINSTANCE)hLangInstance, IDB_HHTOPC, 0, &cMap, 1);
  // associate the bitmap with the button.
  if ((hwndButton = GetDlgItem(hDlg, IDC_HHTOPC)) != NULL)
  {
    hOldBmp = (HBITMAP)SendMessage(hwndButton, STM_SETIMAGE,
                                   (WPARAM)IMAGE_BITMAP, (LPARAM)(HANDLE)hBmp);
    if (hOldBmp != NULL)
      DeleteObject((HGDIOBJ)hOldBmp);
  }
}
