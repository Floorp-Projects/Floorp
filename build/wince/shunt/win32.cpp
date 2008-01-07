/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org code, released
 * Jan 28, 20.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Garrett Arch Bly the, 28-January-2003
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "mozce_internal.h"

extern "C" {
#if 0
}
#endif

#include "kfuncs.h"
#include "wingdi.h"
#include "Windows.h"
#include "locale.h"
#include <winbase.h>

#define MOZCE_NOT_IMPLEMENTED(fname) \
  SetLastError(0); \
  mozce_printf("-- fname called\n"); \
  SetLastError(ERROR_CALL_NOT_IMPLEMENTED); \
  return 0;

#define MOZCE_NOT_IMPLEMENTED_RV(fname, rv) \
  SetLastError(0); \
  mozce_printf("-- fname called\n"); \
  SetLastError(ERROR_CALL_NOT_IMPLEMENTED); \
  return rv;

#define wcharcount(array) (sizeof(array) / sizeof(TCHAR))

#if 0   // Allready implemented

MOZCE_SHUNT_API DWORD CommDlgExtendedError()
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("CommDlgExtendedError called\n");
#endif

    return -1 /*CDERR_DIALOGFAILURE*/;
}

MOZCE_SHUNT_API int MulDiv(int inNumber, int inNumerator, int inDenominator)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("MulDiv called\n");
#endif
    
    if (inDenominator == 0)
        return 0;

    return (int)(((INT64)inNumber * (INT64)inNumerator) / (INT64)inDenominator);
}


MOZCE_SHUNT_API int GetDIBits(HDC inDC, HBITMAP inBMP, UINT inStartScan, UINT inScanLines, LPVOID inBits, LPBITMAPINFO inInfo, UINT inUsage)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("-- GetDIBits called\n");
#endif

    int retval = 0;

    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);

    return retval;
}


MOZCE_SHUNT_API int SetDIBits(HDC inDC, HBITMAP inBMP, UINT inStartScan, UINT inScanLines, CONST LPVOID inBits, CONST LPBITMAPINFO inInfo, UINT inUsage)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("-- SetDIBits called\n");
#endif

    int retval = 0;

    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);

    return retval;
}

MOZCE_SHUNT_API int SetStretchBltMode(HDC inDC, int inStretchMode)
{
    MOZCE_NOT_IMPLEMENTED(__FUNCTION__);
}


MOZCE_SHUNT_API DWORD GetFontData(HDC inDC, DWORD inTable, DWORD inOffset, LPVOID outBuffer, DWORD inData)
{
    MOZCE_NOT_IMPLEMENTED(__FUNCTION__);
}


MOZCE_SHUNT_API BOOL GetIconInfo(HICON inIcon, PICONINFO outIconinfo)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("-- GetIconInfo called\n");
#endif

    BOOL retval = FALSE;

    if(NULL != outIconinfo)
    {
        memset(outIconinfo, 0, sizeof(ICONINFO));
    }

    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);

    return retval;
}


#endif

MOZCE_SHUNT_API HBITMAP CreateDIBitmap(HDC inDC, CONST BITMAPINFOHEADER *inBMIH, DWORD inInit, CONST VOID *inBInit, CONST BITMAPINFO *inBMI, UINT inUsage)
{
    MOZCE_NOT_IMPLEMENTED(__FUNCTION__);
}


MOZCE_SHUNT_API int SetPolyFillMode(HDC inDC, int inPolyFillMode)
{
    MOZCE_NOT_IMPLEMENTED(__FUNCTION__);
}

MOZCE_SHUNT_API int SetArcDirection(HDC inDC, int inArcDirection)
{
    MOZCE_NOT_IMPLEMENTED(__FUNCTION__);
}

MOZCE_SHUNT_API BOOL Arc(HDC inDC, int inLeftRect, int inTopRect, int inRightRect, int inBottomRect, int inXStartArc, int inYStartArc, int inXEndArc, int inYEndArc)
{
    MOZCE_NOT_IMPLEMENTED(__FUNCTION__);
}


MOZCE_SHUNT_API BOOL Pie(HDC inDC, int inLeftRect, int inTopRect, int inRightRect, int inBottomRect, int inXRadial1, int inYRadial1, int inXRadial2, int inYRadial2)
{
    MOZCE_NOT_IMPLEMENTED(__FUNCTION__);
}


MOZCE_SHUNT_API BOOL LPtoDP(HDC inDC, LPPOINT inoutPoints, int inCount)
{
    MOZCE_NOT_IMPLEMENTED(__FUNCTION__);
}




MOZCE_SHUNT_API BOOL LineDDA(int inXStart, int inYStart, int inXEnd, int inYEnd, LINEDDAPROC inLineFunc, LPARAM inData)
{
    MOZCE_NOT_IMPLEMENTED(__FUNCTION__);
}

MOZCE_SHUNT_API int ExtSelectClipRgn(HDC inDC, HRGN inRGN, int inMode)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("ExtSelectClipRgn called\n");
#endif
    
    // inModes are defined as:
    // RGN_AND = 1
    // RGN_OR = 2
    // RGN_XOR = 3
    // RGN_DIFF = 4
    // RGN_COPY = 5
    

    if (inMode == RGN_COPY)
    {
        return SelectClipRgn(inDC, inRGN);
    }

    HRGN cRGN = NULL;
    int result = GetClipRgn(inDC, cRGN);
    
    // if there is no current clipping region, set it to the
    // tightest bounding rectangle that can be drawn around
    // the current visible area on the device

    if (result != 1)
    {
        RECT cRect;
        GetClipBox(inDC,&cRect);
        cRGN = CreateRectRgn(cRect.left,cRect.top,cRect.right,cRect.bottom);
    }

    // now select the proper region as the current clipping region
    result = SelectClipRgn(inDC,cRGN);		
    
    if (result == NULLREGION) 
    {
        if (inMode == RGN_DIFF || inMode == RGN_AND)
            result = SelectClipRgn(inDC,NULL);
        else
            result = SelectClipRgn(inDC,inRGN);
        
        DeleteObject(cRGN);
        return result;
    } 
    
    if (result == SIMPLEREGION || result == COMPLEXREGION)
    {
        if (inMode == RGN_DIFF)
            CombineRgn(cRGN, cRGN, inRGN, inMode);
        else
            CombineRgn(cRGN, inRGN, cRGN, inMode);
        result = SelectClipRgn(inDC,cRGN);
        DeleteObject(cRGN);
        return result;
    }
    
    HRGN rgn = CreateRectRgn(0, 0, 32000, 32000);
    result = SelectClipRgn(inDC, rgn);
    DeleteObject(rgn);

    return result;
}


MOZCE_SHUNT_API int FrameRect(HDC inDC, CONST RECT *inRect, HBRUSH inBrush)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("FrameRect called\n");
#endif

    HBRUSH oldBrush = (HBRUSH)SelectObject(inDC, inBrush);
    RECT myRect = *inRect;
    InflateRect(&myRect, 1, 1); // The width and height of
                                // the border are always one
                                // logical unit.
    
    // 1  ---->   2
    //            
    //            |
    //            v
    //
    // 4  ---->   3 

    MoveToEx(inDC, myRect.left, myRect.top, (LPPOINT) NULL); 

    // 1 -> 2
    LineTo(inDC, myRect.right, myRect.top); 
    
    // 2 -> 3
    LineTo(inDC, myRect.right, myRect.bottom); 

    // 3 -> 4
    LineTo(inDC, myRect.left, myRect.bottom); 

    SelectObject(inDC, oldBrush);

    return 1;
}



MOZCE_SHUNT_API UINT GetTextCharset(HDC inDC)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("GetTextCharset called\n");
#endif

    UINT retval = DEFAULT_CHARSET;

    TEXTMETRIC tm;
    if(GetTextMetrics(inDC, &tm))
    {
        retval = tm.tmCharSet;
    }

    return retval;
}


MOZCE_SHUNT_API UINT GetTextCharsetInfo(HDC inDC, LPFONTSIGNATURE outSig, DWORD inFlags)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("GetTextCharsetInfo called\n");
#endif

    // Zero out the FONTSIGNATURE as we do not know how to fill it out properly.
    if(NULL != outSig)
    {
        memset(outSig, 0, sizeof(FONTSIGNATURE));
    }

    return GetTextCharset(inDC);
}

#define FACENAME_MAX 128
typedef struct __struct_CollectFaces
{
    UINT    mCount;
    LPTSTR  mNames[FACENAME_MAX];
}
CollectFaces;

static int CALLBACK collectProc(CONST LOGFONT* inLF, CONST TEXTMETRIC* inTM, DWORD inFontType, LPARAM inParam)
{
    int retval = 0;
    CollectFaces* collection = (CollectFaces*)inParam;

    if(FACENAME_MAX > collection->mCount)
    {
        retval = 1;

        collection->mNames[collection->mCount] = _tcsdup(inLF->lfFaceName);
        if(NULL != collection->mNames[collection->mCount])
        {
            collection->mCount++;
        }
    }

    return retval;
}

MOZCE_SHUNT_API int GetMapMode(HDC inDC)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("GetMapMode called\n");
#endif

    int retval = MM_TEXT;
    return retval;
}



MOZCE_SHUNT_API LONG RegCreateKey(HKEY inKey, LPCTSTR inSubKey, PHKEY outResult)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("RegCreateKey called\n");
#endif

    LONG retval = ERROR_SUCCESS;
    DWORD disp = 0;

    retval = RegCreateKeyEx(inKey, inSubKey, 0, NULL, 0, 0, NULL, outResult, &disp);

    return retval;
}


MOZCE_SHUNT_API BOOL WaitMessage(VOID)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("WaitMessage called\n");
#endif

    BOOL retval = TRUE;

    HANDLE hThread = GetCurrentThread();
    DWORD waitRes = MsgWaitForMultipleObjectsEx(1, &hThread, INFINITE, QS_ALLEVENTS, 0);
    if((DWORD)-1 == waitRes)
    {
        retval = FALSE;
    }

    return retval;
}


MOZCE_SHUNT_API BOOL FlashWindow(HWND inWnd, BOOL inInvert)
{
   MOZCE_NOT_IMPLEMENTED_RV(__FUNCTION__, FALSE);
}


typedef struct ECWWindows
{
    LPARAM      params;
    WNDENUMPROC func;
    HWND        parent;
} ECWWindows;

static BOOL CALLBACK MyEnumWindowsProc(HWND hwnd, LPARAM lParam)
{
    ECWWindows *myParams = (ECWWindows*) lParam;

    if (IsChild(myParams->parent, hwnd))
    {
        return myParams->func(hwnd, myParams->params);
    }

    return TRUE;
}

MOZCE_SHUNT_API BOOL EnumChildWindows(HWND inParent, WNDENUMPROC inFunc, LPARAM inParam)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("EnumChildWindows called\n");
#endif

    ECWWindows myParams;
    myParams.params = inParam;
    myParams.func   = inFunc;
    myParams.parent = inParent;

    return EnumWindows(MyEnumWindowsProc, (LPARAM) &myParams);
}


MOZCE_SHUNT_API BOOL EnumThreadWindows(DWORD inThreadID, WNDENUMPROC inFunc, LPARAM inParam)
{
    MOZCE_NOT_IMPLEMENTED_RV(__FUNCTION__, FALSE);
}


MOZCE_SHUNT_API BOOL IsIconic(HWND inWnd)
{
   MOZCE_NOT_IMPLEMENTED_RV(__FUNCTION__, FALSE);

}


MOZCE_SHUNT_API BOOL OpenIcon(HWND inWnd)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("OpenIcon called\n");
#endif
    return SetActiveWindow(inWnd) ? 1:0;
}


MOZCE_SHUNT_API HHOOK SetWindowsHookEx(int inType, void* inFunc, HINSTANCE inMod, DWORD inThreadId)
{
   MOZCE_NOT_IMPLEMENTED_RV(__FUNCTION__, NULL);
}


MOZCE_SHUNT_API BOOL UnhookWindowsHookEx(HHOOK inHook)
{
   MOZCE_NOT_IMPLEMENTED_RV(__FUNCTION__, FALSE);
}


MOZCE_SHUNT_API LRESULT CallNextHookEx(HHOOK inHook, int inCode, WPARAM wParam, LPARAM lParam)
{
   MOZCE_NOT_IMPLEMENTED_RV(__FUNCTION__, NULL);

}


MOZCE_SHUNT_API BOOL InvertRgn(HDC inDC, HRGN inRGN)
{
   MOZCE_NOT_IMPLEMENTED_RV(__FUNCTION__, FALSE);

}


MOZCE_SHUNT_API int GetScrollPos(HWND inWnd, int inBar)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("GetScrollPos called\n");
#endif

    int retval = 0;
    SCROLLINFO info;

    if(GetScrollInfo(inWnd, inBar, &info))
    {
        return info.nPos;
    }

    return retval;
}


MOZCE_SHUNT_API BOOL GetScrollRange(HWND inWnd, int inBar, LPINT outMinPos, LPINT outMaxPos)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("GetScrollRange called\n");
#endif

    BOOL retval = FALSE;
    SCROLLINFO info;

    if((retval = GetScrollInfo(inWnd, inBar, &info)))
    {
        if(NULL != outMinPos)
        {
            *outMinPos = info.nMin;
        }
        if(NULL != outMaxPos)
        {
            *outMaxPos = info.nMax;
        }
    }

    return retval;
}


MOZCE_SHUNT_API HRESULT CoLockObjectExternal(IUnknown* inUnk, BOOL inLock, BOOL inLastUnlockReleases)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("CoLockObjectExternal called\n");
#endif

    HRESULT retval = S_OK;

    if(NULL != inUnk)
    {
        if(FALSE == inLock)
        {
            inUnk->Release();
        }
        else
        {
            inUnk->AddRef();
        }
    }
    else
    {
        retval = E_INVALIDARG;
    }

    return retval;
}

MOZCE_SHUNT_API HRESULT CoInitialize(LPVOID pvReserved)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("-- mozce_Conitialize called\n");
#endif

    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    return S_OK;

}

MOZCE_SHUNT_API LRESULT OleInitialize(LPVOID pvReserved)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("-- OleInitialize called\n");
#endif

    return S_OK;
}

MOZCE_SHUNT_API void OleUninitialize()
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("-- OleUninitialize called\n");
#endif
}

MOZCE_SHUNT_API HRESULT OleQueryLinkFromData(IDataObject* inSrcDataObject)
{
   MOZCE_NOT_IMPLEMENTED_RV(__FUNCTION__, E_NOTIMPL);
}

//LPITEMIDLIST
MOZCE_SHUNT_API void* mozce_SHBrowseForFolder(void* /*LPBROWSEINFOS*/ inBI)
{
   MOZCE_NOT_IMPLEMENTED_RV(__FUNCTION__, 0);
}


MOZCE_SHUNT_API BOOL SetMenu(HWND inWnd, HMENU inMenu)
{
   MOZCE_NOT_IMPLEMENTED_RV(__FUNCTION__, FALSE);

}


MOZCE_SHUNT_API BOOL GetUserName(LPTSTR inBuffer, LPDWORD inoutSize)
{
    *inoutSize = 0;
    MOZCE_NOT_IMPLEMENTED_RV(__FUNCTION__, FALSE);
}


MOZCE_SHUNT_API DWORD GetShortPathName(LPCSTR inLongPath, LPSTR outShortPath, DWORD inBufferSize)
{
    MOZCE_PRECHECK
        
#ifdef DEBUG
   mozce_printf("GetShortPathName called\n");
#endif
    
    DWORD retval = strlen(inLongPath);
    strncpy(outShortPath, inLongPath, inBufferSize);
    return ((retval > inBufferSize) ? inBufferSize : retval);
}

MOZCE_SHUNT_API DWORD GetShortPathNameW(LPCWSTR inLongPath, LPWSTR outShortPath, DWORD inBufferSize)
{
    MOZCE_PRECHECK
        
#ifdef DEBUG
   mozce_printf("GetShortPathNameW called\n");
#endif
    
    DWORD retval = wcslen(inLongPath);
    wcsncpy(outShortPath, inLongPath, inBufferSize);
    return ((retval > inBufferSize) ? inBufferSize : retval);
}

MOZCE_SHUNT_API DWORD GetEnvironmentVariable(LPCSTR lpName, LPCSTR lpBuffer, DWORD nSize)
{
    MOZCE_NOT_IMPLEMENTED_RV(__FUNCTION__, 0);
}

MOZCE_SHUNT_API DWORD GetEnvironmentVariableW(LPCWSTR lpName, LPWSTR lpBuffer, DWORD nSize)
{
    MOZCE_NOT_IMPLEMENTED_RV(__FUNCTION__, 0);
}

MOZCE_SHUNT_API void GetSystemTimeAsFileTime(LPFILETIME lpSystemTimeAsFileTime)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("GetSystemTimeAsFileTime called\n");
#endif

    SYSTEMTIME st;
    GetSystemTime(&st);
    SystemTimeToFileTime(&st,lpSystemTimeAsFileTime);
}

#ifndef MIN
#define MIN(a,b) (((a)<(b)) ? (a) : (b))
#endif


#ifndef MAX
#define MAX(a,b) (((a)>(b)) ? (a) : (b))
#endif

MOZCE_SHUNT_API DWORD GetFullPathName(const char* lpFileName, 
                                            DWORD nBufferLength, 
                                            const char* lpBuffer, 
                                            const char** lpFilePart)
{
#ifdef DEBUG
    mozce_printf("GetFullPathName called\n");
#endif

    DWORD len = strlen(lpFileName);
    if (len > nBufferLength)
        return len;
    
    strncpy((char*)lpBuffer, lpFileName, len);
    ((char*)lpBuffer)[len] = '\0';
    
    if(lpFilePart)
    {
        char* sep = strrchr(lpBuffer, '\\');
        if (sep) {
            sep++; // pass the seperator
            *lpFilePart = sep;
        }
        else
            *lpFilePart = lpBuffer;
    }
    
#ifdef DEBUG
    mozce_printf("GetFullPathName called %s (%s)\n", lpBuffer, *lpFilePart);
#endif
    return len;	
}

static LONG gGetMessageTime = 0;

MOZCE_SHUNT_API BOOL mozce_GetMessage(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax )
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("mozce_GetMessage called\n");
#endif

    BOOL b = GetMessage(lpMsg, hWnd, wMsgFilterMin, wMsgFilterMin);

    if (b)
        gGetMessageTime = lpMsg->time;

    return b;
}


MOZCE_SHUNT_API BOOL mozce_PeekMessage(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax, UINT wRemoveMsg)
{
    MOZCE_PRECHECK

#ifdef LOUD_PEEKMESSAGE
#ifdef DEBUG
    mozce_printf("mozce_PeekMessageA called\n");
#endif
#endif

    BOOL b = PeekMessageW(lpMsg, hWnd, wMsgFilterMin, wMsgFilterMax, wRemoveMsg);
    
    if (b && wRemoveMsg == PM_REMOVE)
        gGetMessageTime = lpMsg->time; 
    
    return b;
}


MOZCE_SHUNT_API LONG GetMessageTime(void)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("GetMessageTime called\n");
#endif

  return gGetMessageTime;
}

MOZCE_SHUNT_API UINT mozce_GetACP(void)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("mozce_GetACP called\n");
#endif

    return GetACP();
}



MOZCE_SHUNT_API DWORD ExpandEnvironmentStrings(LPCTSTR lpSrc, LPTSTR lpDst, DWORD nSize)
{
    MOZCE_NOT_IMPLEMENTED_RV(__FUNCTION__, 0);
}

MOZCE_SHUNT_API DWORD ExpandEnvironmentStringsW(const unsigned short* lpSrc, const unsigned short* lpDst, DWORD nSize)
{
     MOZCE_NOT_IMPLEMENTED_RV(__FUNCTION__, 0);
}

MOZCE_SHUNT_API BOOL GdiFlush(void)
{
    MOZCE_PRECHECK

    return TRUE;
}

MOZCE_SHUNT_API BOOL GetWindowPlacement(HWND hWnd, WINDOWPLACEMENT *lpwndpl)
{
   MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("GetWindowPlacement called\n");
#endif

   memset(lpwndpl, 0, sizeof(WINDOWPLACEMENT));
   
   // This is wrong when the window is minimized.
   lpwndpl->showCmd = SW_SHOWNORMAL;
   GetWindowRect(hWnd, &lpwndpl->rcNormalPosition);
   
   return TRUE;
}

MOZCE_SHUNT_API HINSTANCE ShellExecute(HWND hwnd, 
                                             LPCSTR lpOperation, 
                                             LPCSTR lpFile, 
                                             LPCSTR lpParameters, 
                                             LPCSTR lpDirectory, 
                                             INT nShowCmd)
{
    
    LPTSTR op   = a2w_malloc(lpOperation, -1, NULL);
    LPTSTR file = a2w_malloc(lpFile, -1, NULL);
    LPTSTR parm = a2w_malloc(lpParameters, -1, NULL);
    LPTSTR dir  = a2w_malloc(lpDirectory, -1, NULL);
    
    SHELLEXECUTEINFO info;
    info.cbSize = sizeof(SHELLEXECUTEINFO);
    info.fMask  = SEE_MASK_NOCLOSEPROCESS;
    info.hwnd   = hwnd;
    info.lpVerb = op;
    info.lpFile = file;
    info.lpParameters = parm;
    info.lpDirectory  = dir;
    info.nShow  = nShowCmd;
    
    BOOL b = ShellExecuteEx(&info);
    
    if (op)
        free(op);
    if (file)
        free(file);
    if (parm)
        free(parm);
    if (dir)
        free(dir);
    
    return (HINSTANCE) info.hProcess;
}

MOZCE_SHUNT_API HINSTANCE ShellExecuteW(HWND hwnd, LPCWSTR lpOperation, LPCWSTR lpFile, LPCWSTR lpParameters, LPCWSTR lpDirectory, INT nShowCmd)
{
   MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("ShellExecuteW called\n");
#endif

    SHELLEXECUTEINFO info;
    info.cbSize = sizeof(SHELLEXECUTEINFO);
    info.fMask  = SEE_MASK_NOCLOSEPROCESS;
    info.hwnd   = hwnd;
    info.lpVerb = lpOperation;
    info.lpFile = lpFile;
    info.lpParameters = lpParameters;
    info.lpDirectory  = lpDirectory;
    info.nShow  = nShowCmd;
    
    BOOL b = ShellExecuteEx(&info);

    return (HINSTANCE) info.hProcess;
}

struct lconv s_locale_conv =
{
    ".",   /* decimal_point */
    ",",   /* thousands_sep */
    "333", /* grouping */
    "$",   /* int_curr_symbol */
    "$",   /* currency_symbol */
    "",    /* mon_decimal_point */
    "",    /* mon_thousands_sep */
    "",    /* mon_grouping */
    "+",   /* positive_sign */
    "-",   /* negative_sign */
    '2',   /* int_frac_digits */
    '2',   /* frac_digits */
    1,     /* p_cs_precedes */
    1,     /* p_sep_by_space */
    1,     /* n_cs_precedes */
    1,     /* n_sep_by_space */
    1,     /* p_sign_posn */
    1,     /* n_sign_posn */
};

MOZCE_SHUNT_API struct lconv * mozce_localeconv(void)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("mozce_localeconv called\n");
#endif
    return &s_locale_conv;
}

MOZCE_SHUNT_API BOOL CreatePipe(PHANDLE hReadPipe, PHANDLE hWritePipe, LPSECURITY_ATTRIBUTES lpPipeAttributes, DWORD nSize)
{
    MOZCE_NOT_IMPLEMENTED_RV(__FUNCTION__, FALSE);
}
MOZCE_SHUNT_API DWORD_PTR SetThreadAffinityMask(HANDLE hThread, DWORD_PTR dwThreadAffinityMask)
{
    MOZCE_NOT_IMPLEMENTED_RV(__FUNCTION__, 0);
}
MOZCE_SHUNT_API BOOL GetProcessAffinityMask(HANDLE hProcess, PDWORD_PTR lpProcessAffinityMask, PDWORD_PTR lpSystemAffinityMask)
{
    MOZCE_NOT_IMPLEMENTED_RV(__FUNCTION__, FALSE);
}

MOZCE_SHUNT_API HANDLE OpenFileMapping(DWORD dwDesiredAccess, BOOL bInheritHandle, LPCTSTR lpName)
{
    MOZCE_NOT_IMPLEMENTED_RV(__FUNCTION__, 0);
}

MOZCE_SHUNT_API UINT GetDriveType(const char* lpRootPathName)
{
    MOZCE_NOT_IMPLEMENTED_RV(__FUNCTION__, 0);
}

MOZCE_SHUNT_API BOOL SetHandleInformation(HANDLE hObject, DWORD dwMask, DWORD dwFlags)
{
    MOZCE_NOT_IMPLEMENTED_RV(__FUNCTION__, 0);
}

MOZCE_SHUNT_API BOOL GetHandleInformation(HANDLE hObject, LPDWORD lpdwFlags)
{
    MOZCE_NOT_IMPLEMENTED_RV(__FUNCTION__, 0);
}

MOZCE_SHUNT_API BOOL LockFile(HANDLE hFile, DWORD dwFileOffsetLow, DWORD dwFileOffsetHigh,
                                    DWORD nNumberOfBytesToLockLow, DWORD nNumberOfBytesToLockHigh)
{
  OVERLAPPED over = {0};
  over.Offset = dwFileOffsetLow;
  over.OffsetHigh = dwFileOffsetHigh;
  return ::LockFileEx(hFile, 0, 0, nNumberOfBytesToLockLow, nNumberOfBytesToLockHigh, &over);
}

MOZCE_SHUNT_API BOOL UnlockFile(HANDLE hFile, DWORD dwFileOffsetLow, DWORD dwFileOffsetHigh,
                                    DWORD nNumberOfBytesToLockLow, DWORD nNumberOfBytesToLockHigh)
{
  OVERLAPPED over = {0};
  over.Offset = dwFileOffsetLow;
  over.OffsetHigh = dwFileOffsetHigh;
  return ::UnlockFileEx(hFile, 0, nNumberOfBytesToLockLow, nNumberOfBytesToLockHigh, &over);

}

MOZCE_SHUNT_API BOOL GetDiskFreeSpaceA(LPCTSTR lpRootPathName, LPDWORD lpSectorsPerCluster, 
                                            LPDWORD lpBytesPerSector, LPDWORD lpNumberOfFreeClusters, LPDWORD lpTotalNumberOfClusters)
{
    MOZCE_NOT_IMPLEMENTED_RV(__FUNCTION__, 0);
}


MOZCE_SHUNT_API BOOL SetWorldTransform(HDC hdc, CONST XFORM *lpXform )
{
    MOZCE_NOT_IMPLEMENTED(__FUNCTION__);
}

MOZCE_SHUNT_API BOOL GetWorldTransform(HDC hdc, LPXFORM lpXform )
{
 MOZCE_NOT_IMPLEMENTED(__FUNCTION__);
}

MOZCE_SHUNT_API int  SetGraphicsMode(HDC hdc, int iMode)
{
  MOZCE_NOT_IMPLEMENTED(__FUNCTION__);
}


MOZCE_SHUNT_API HRESULT ScriptFreeCache(SCRIPT_CACHE* psc )
{
  MOZCE_NOT_IMPLEMENTED(__FUNCTION__);
}
MOZCE_SHUNT_API DWORD WINAPI GetGlyphIndicesA( HDC hdc,LPCSTR lpstr, int c, LPWORD pgi, DWORD fl)
{
  MOZCE_NOT_IMPLEMENTED(__FUNCTION__);

}
MOZCE_SHUNT_API DWORD WINAPI GetGlyphIndicesW( HDC hdc, LPCWSTR lpstr, int c,  LPWORD pgi, DWORD fl)
{
  MOZCE_NOT_IMPLEMENTED(__FUNCTION__);
}
MOZCE_SHUNT_API HRESULT WINAPI ScriptIsComplex(const WCHAR *pwcInChars, int cInChars, DWORD   dwFlags)
{
  MOZCE_NOT_IMPLEMENTED(__FUNCTION__);
}
MOZCE_SHUNT_API BOOL GetTextExtentExPointI( HDC hdc,    LPWORD pgiIn, int cgi,int nMaxExtent, LPINT lpnFit, LPINT alpDx, LPSIZE lpSize)
{
  MOZCE_NOT_IMPLEMENTED(__FUNCTION__);
}
MOZCE_SHUNT_API HRESULT WINAPI ScriptGetProperties( const SCRIPT_PROPERTIES ***ppSp, int *piNumScripts)
{
  MOZCE_NOT_IMPLEMENTED(__FUNCTION__);
}
MOZCE_SHUNT_API HRESULT WINAPI ScriptGetFontProperties(HDC hdc, SCRIPT_CACHE *psc, SCRIPT_FONTPROPERTIES *sfp )
{
  MOZCE_NOT_IMPLEMENTED(__FUNCTION__);
}
MOZCE_SHUNT_API HRESULT WINAPI ScriptBreak(  const WCHAR *pwcChars, int cChars, const SCRIPT_ANALYSIS *psa, SCRIPT_LOGATTR *psla )
{
  MOZCE_NOT_IMPLEMENTED(__FUNCTION__);
}

MOZCE_SHUNT_API HRESULT WINAPI ScriptItemize(const WCHAR *pwcInChars, int cInChars, int cMaxItems, const SCRIPT_CONTROL *psControl, 
                                             const SCRIPT_STATE *psState,  SCRIPT_ITEM *pItems, int *pcItems )
{
  MOZCE_NOT_IMPLEMENTED(__FUNCTION__);
}

MOZCE_SHUNT_API BOOL WINAPI GetICMProfile(HDC hDC, LPDWORD lpcbName,LPWSTR lpszFilename)
{
  MOZCE_NOT_IMPLEMENTED(__FUNCTION__);
}
MOZCE_SHUNT_API DWORD WINAPI GetGuiResources(HANDLE hProcess,DWORD uiFlags)
{
  MOZCE_NOT_IMPLEMENTED(__FUNCTION__);
}
MOZCE_SHUNT_API HRESULT WINAPI ScriptRecordDigitSubstitution(LCID Locale, SCRIPT_DIGITSUBSTITUTE  *psds)
{
  MOZCE_NOT_IMPLEMENTED(__FUNCTION__);
}
MOZCE_SHUNT_API HWND WINAPI GetTopWindow(HWND hWnd)
{
  MOZCE_NOT_IMPLEMENTED(__FUNCTION__);
}

MOZCE_SHUNT_API BOOL WINAPI UpdateLayeredWindow(HWND hWnd, HDC hdcDst, POINT *pptDst,
                                SIZE *psize, HDC hdcSrc, POINT *pptSrc,
                                COLORREF crKey, BLENDFUNCTION *pblend,
                                DWORD dwFlags)
{
    MOZCE_NOT_IMPLEMENTED(__FUNCTION__);
}

MOZCE_SHUNT_API BOOL
WINAPI
InitializeCriticalSectionAndSpinCount(
    __out LPCRITICAL_SECTION InitializeCriticalSection,
    __in  DWORD dwSpinCount
    )
{
    ::InitializeCriticalSection(InitializeCriticalSection);
    return TRUE;
}

MOZCE_SHUNT_API
DWORD
WINAPI
SetCriticalSectionSpinCount(
    __inout LPCRITICAL_SECTION lpCriticalSection,
    __in    DWORD dwSpinCount
    )
{
    return 0;
}
MOZCE_SHUNT_API
BOOL
WINAPI
GetSystemTimeAdjustment(
    __out PDWORD lpTimeAdjustment,
    __out PDWORD lpTimeIncrement,
    __out PBOOL  lpTimeAdjustmentDisabled
    )
{
    *lpTimeAdjustmentDisabled = TRUE;
    return TRUE;
}

MOZCE_SHUNT_API BOOL  WINAPI PolyBezierTo(__in HDC hdc, __in_ecount(cpt) CONST POINT * apt, __in DWORD cpt){
 MOZCE_NOT_IMPLEMENTED(__FUNCTION__); }

MOZCE_SHUNT_API BOOL WINAPI CloseFigure(__in HDC hdc){
 MOZCE_NOT_IMPLEMENTED(__FUNCTION__); 
}

MOZCE_SHUNT_API BOOL WINAPI SelectClipPath(__in HDC hdc, __in int mode){
 MOZCE_NOT_IMPLEMENTED(__FUNCTION__); 
}

MOZCE_SHUNT_API BOOL WINAPI EndPath(__in HDC hdc){
 MOZCE_NOT_IMPLEMENTED(__FUNCTION__); 
}

MOZCE_SHUNT_API BOOL WINAPI BeginPath(__in HDC hdc){
 MOZCE_NOT_IMPLEMENTED(__FUNCTION__); 
}

MOZCE_SHUNT_API BOOL WINAPI ModifyWorldTransform( __in HDC hdc, __in_opt CONST XFORM * lpxf, __in DWORD mode){
 MOZCE_NOT_IMPLEMENTED(__FUNCTION__); 
}

MOZCE_SHUNT_API BOOL WINAPI WidenPath(__in HDC hdc){
 MOZCE_NOT_IMPLEMENTED(__FUNCTION__); 
}

MOZCE_SHUNT_API BOOL WINAPI StrokePath(__in HDC hdc){
 MOZCE_NOT_IMPLEMENTED(__FUNCTION__); 
}

MOZCE_SHUNT_API HPEN WINAPI ExtCreatePen( __in DWORD iPenStyle,
                                    __in DWORD cWidth,
                                    __in CONST LOGBRUSH *plbrush,
                                    __in DWORD cStyle,
                                    __in_ecount_opt(cStyle) CONST DWORD *pstyle){
 MOZCE_NOT_IMPLEMENTED(__FUNCTION__); 
}

MOZCE_SHUNT_API BOOL WINAPI SetMiterLimit(__in HDC hdc, __in FLOAT limit, __out_opt PFLOAT old){
 MOZCE_NOT_IMPLEMENTED(__FUNCTION__); 
}

MOZCE_SHUNT_API BOOL WINAPI FillPath(__in HDC hdc){
 MOZCE_NOT_IMPLEMENTED(__FUNCTION__); 
}

MOZCE_SHUNT_API BOOL	      WINAPI GetICMProfileW(    __in HDC hdc, 
                                                __inout LPDWORD pBufSize,
                                                __out_ecount_opt(*pBufSize) LPWSTR pszFilename)
{
 MOZCE_NOT_IMPLEMENTED(__FUNCTION__); 
}

HRESULT WINAPI ScriptShape(
    HDC                 hdc,            // In    Optional (see under caching)
    SCRIPT_CACHE       *psc,            // InOut Cache handle
    const WCHAR        *pwcChars,       // In    Logical unicode run
    int                 cChars,         // In    Length of unicode run
    int                 cMaxGlyphs,     // In    Max glyphs to generate
    SCRIPT_ANALYSIS    *psa,            // InOut Result of ScriptItemize (may have fNoGlyphIndex set)
    WORD               *pwOutGlyphs,    // Out   Output glyph buffer
    WORD               *pwLogClust,     // Out   Logical clusters
    SCRIPT_VISATTR     *psva,           // Out   Visual glyph attributes
    int                *pcGlyphs){
 MOZCE_NOT_IMPLEMENTED(__FUNCTION__); 
}
      // Out   Count of glyphs generated

HRESULT WINAPI ScriptPlace(
    HDC                     hdc,        // In    Optional (see under caching)
    SCRIPT_CACHE           *psc,        // InOut Cache handle
    const WORD             *pwGlyphs,   // In    Glyph buffer from prior ScriptShape call
    int                     cGlyphs,    // In    Number of glyphs
    const SCRIPT_VISATTR   *psva,       // In    Visual glyph attributes
    SCRIPT_ANALYSIS        *psa,        // InOut Result of ScriptItemize (may have fNoGlyphIndex set)
    int                    *piAdvance,  // Out   Advance wdiths
    GOFFSET                *pGoffset,   // Out   x,y offset for combining glyph
    ABC                    *pABC){
 MOZCE_NOT_IMPLEMENTED(__FUNCTION__); 
}
      // Out   Composite ABC for the whole run (Optional)

MOZCE_SHUNT_API int   WINAPI SetMapMode(HDC, int)
{
    MOZCE_NOT_IMPLEMENTED(__FUNCTION__);
}

MOZCE_SHUNT_API DWORD WINAPI GetCharacterPlacementW(  __in HDC hdc, __in_ecount(nCount) LPCWSTR lpString, __in int nCount, __in int nMexExtent, __inout LPGCP_RESULTSW lpResults, __in DWORD dwFlags)
{
    MOZCE_NOT_IMPLEMENTED(__FUNCTION__);
}

MOZCE_SHUNT_API wchar_t *_wgetcwd(wchar_t *buffer, int maxlen)
{
    MOZCE_NOT_IMPLEMENTED(__FUNCTION__);
}

MOZCE_SHUNT_API int  _wrmdir(const wchar_t * _Path)
{
    return ::RemoveDirectoryW(_Path);
}

MOZCE_SHUNT_API int _wremove(const wchar_t * _Filename)
{
    return ::DeleteFileW(_Filename);
}
MOZCE_SHUNT_API int _wchmod(const wchar_t * _Filename, int _Mode)
{
    MOZCE_NOT_IMPLEMENTED_RV(__FUNCTION__, -1);
}

// Copied from msaa.h
MOZCE_SHUNT_API HWND GetAncestor(HWND hwnd, UINT gaFlags)
{
    HWND	hwndParent;
    HWND	hwndDesktop;
    DWORD   dwStyle;

     // HERE IS THE FAKE IMPLEMENTATION
    if (!IsWindow(hwnd))
        return(NULL);

    hwndDesktop = GetDesktopWindow();
    if (hwnd == hwndDesktop)
        return(NULL);
    dwStyle = GetWindowLong (hwnd,GWL_STYLE);

    switch (gaFlags)
    {
        case GA_PARENT:
            if (dwStyle & WS_CHILD)
                hwndParent = GetParent(hwnd);
            else
                hwndParent = GetWindow (hwnd,GW_OWNER);

            if (hwndParent == NULL)
                hwndParent = hwnd;
            break;

        case GA_ROOT:
            if (dwStyle & WS_CHILD)
                hwndParent = GetParent(hwnd);
            else
                hwndParent = GetWindow (hwnd,GW_OWNER);

            while (hwndParent != hwndDesktop &&
                   hwndParent != NULL)
            {
                hwnd = hwndParent;
                dwStyle = GetWindowLong(hwnd,GWL_STYLE);
                if (dwStyle & WS_CHILD)
                    hwndParent = GetParent(hwnd);
                else
                    hwndParent = GetWindow (hwnd,GW_OWNER);
            }
            break;

        case GA_ROOTOWNER:
            while (hwndParent = GetParent(hwnd))
                hwnd = hwndParent;
            break;
        default:
            return NULL;
    }

    return(hwndParent);
}

MOZCE_SHUNT_API UINT MapVirtualKeyA(UINT uCode, UINT uMapType)
{
    return MapVirtualKeyW(uCode, uMapType);
}

MOZCE_SHUNT_API BOOL GetClassInfoA(HINSTANCE hInstance, LPCSTR lpClassName, LPWNDCLASSA lpWndClass)
{
    wchar_t className[MAX_PATH];
    a2w_buffer(lpClassName, MAX_PATH, className, MAX_PATH);
    BOOL retval = ::GetClassInfoW(hInstance, className, (LPWNDCLASSW) lpWndClass);  // doug, is this typecast ok. What happens when I register with A, and GetClassInfo with W?
    return retval;
}

MOZCE_SHUNT_API int WINAPI StartDocA(HDC, CONST DOCINFOA *)
{
    MOZCE_NOT_IMPLEMENTED_RV(__FUNCTION__, 0); // Did not implement it because we are not likely to print from WinCE?
}

#if 0
{
#endif
} /* extern "C" */

void dumpMemoryInfo()
{
    MEMORYSTATUS ms;
    ms.dwLength = sizeof(MEMORYSTATUS);
    
    
    GlobalMemoryStatus(&ms);
    
    wprintf(L"-> %d %d %d %d %d %d %d\n", 
            ms.dwMemoryLoad, 
            ms.dwTotalPhys, 
            ms.dwAvailPhys, 
            ms.dwTotalPageFile, 
            ms.dwAvailPageFile, 
            ms.dwTotalVirtual, 
            ms.dwAvailVirtual);
}
