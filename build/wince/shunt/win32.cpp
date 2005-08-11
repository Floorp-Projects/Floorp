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
 *   Garrett Arch Blythe, 28-January-2003
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

#define wcharcount(array) (sizeof(array) / sizeof(TCHAR))

MOZCE_SHUNT_API int mozce_MulDiv(int inNumber, int inNumerator, int inDenominator)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("mozce_MulDiv called\n");
#endif
    
    if (inDenominator == 0)
        return 0;

    return (int)(((INT64)inNumber * (INT64)inNumerator) / (INT64)inDenominator);
}


MOZCE_SHUNT_API int mozce_GetDIBits(HDC inDC, HBITMAP inBMP, UINT inStartScan, UINT inScanLines, LPVOID inBits, LPBITMAPINFO inInfo, UINT inUsage)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("-- mozce_GetDIBits called\n");
#endif

    int retval = 0;

    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);

    return retval;
}


MOZCE_SHUNT_API int mozce_SetDIBits(HDC inDC, HBITMAP inBMP, UINT inStartScan, UINT inScanLines, CONST LPVOID inBits, CONST LPBITMAPINFO inInfo, UINT inUsage)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("-- mozce_SetDIBits called\n");
#endif

    int retval = 0;

    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);

    return retval;
}


MOZCE_SHUNT_API HBITMAP mozce_CreateDIBitmap(HDC inDC, CONST BITMAPINFOHEADER *inBMIH, DWORD inInit, CONST VOID *inBInit, CONST BITMAPINFO *inBMI, UINT inUsage)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("-- mozce_CreateDIBitmap called\n");
#endif

    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);

    return NULL;
}


MOZCE_SHUNT_API int mozce_SetPolyFillMode(HDC inDC, int inPolyFillMode)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("-- mozce_SetPolyFillMode called\n");
#endif

    int retval = 0;

    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);

    return retval;
}


MOZCE_SHUNT_API int mozce_SetStretchBltMode(HDC inDC, int inStretchMode)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("-- mozce_SetStretchBltMode called\n");
#endif

    int retval = 0;

    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);

    return retval;
}


MOZCE_SHUNT_API int mozce_ExtSelectClipRgn(HDC inDC, HRGN inRGN, int inMode)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("mozce_ExtSelectClipRgn called\n");
#endif
    
    // inModes are defined as:
    // RGN_AND = 1
    // RGN_OR = 2
    // RGN_XOR = 3
    // RGN_DIFF = 4
    // RGN_COPY = 5
    
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

typedef VOID CALLBACK LINEDDAPROC(
  int X,          // x-coordinate of point
  int Y,          // y-coordinate of point
  LPARAM lpData   // application-defined data
);

MOZCE_SHUNT_API BOOL mozce_LineDDA(int inXStart, int inYStart, int inXEnd, int inYEnd, LINEDDAPROC inLineFunc, LPARAM inData)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("-- mozce_LineDDA called\n");
#endif

    BOOL retval = FALSE;

    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);

    return retval;
}


MOZCE_SHUNT_API int mozce_FrameRect(HDC inDC, CONST RECT *inRect, HBRUSH inBrush)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("mozce_FrameRect called\n");
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


MOZCE_SHUNT_API int mozce_SetArcDirection(HDC inDC, int inArcDirection)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("-- mozce_SetArcDirection called\n");
#endif

    int retval = 0;

    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);

    return retval;
}


MOZCE_SHUNT_API BOOL mozce_Arc(HDC inDC, int inLeftRect, int inTopRect, int inRightRect, int inBottomRect, int inXStartArc, int inYStartArc, int inXEndArc, int inYEndArc)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("-- mozce_Arc called\n");
#endif

    BOOL retval = FALSE;

    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);

    return retval;
}


MOZCE_SHUNT_API BOOL mozce_Pie(HDC inDC, int inLeftRect, int inTopRect, int inRightRect, int inBottomRect, int inXRadial1, int inYRadial1, int inXRadial2, int inYRadial2)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("-- mozce_Pie called\n");
#endif

    BOOL retval = FALSE;

    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);

    return retval;
}


MOZCE_SHUNT_API DWORD mozce_GetFontData(HDC inDC, DWORD inTable, DWORD inOffset, LPVOID outBuffer, DWORD inData)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("-- mozce_GetFontData called\n");
#endif

    DWORD retval = GDI_ERROR;
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);

    return retval;
}


MOZCE_SHUNT_API UINT mozce_GetTextCharset(HDC inDC)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("mozce_GetTextCharset called\n");
#endif

    UINT retval = DEFAULT_CHARSET;

    TEXTMETRIC tm;
    if(GetTextMetrics(inDC, &tm))
    {
        retval = tm.tmCharSet;
    }

    return retval;
}


MOZCE_SHUNT_API UINT mozce_GetTextCharsetInfo(HDC inDC, LPFONTSIGNATURE outSig, DWORD inFlags)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("mozce_GetTextCharsetInfo called\n");
#endif

    // Zero out the FONTSIGNATURE as we do not know how to fill it out properly.
    if(NULL != outSig)
    {
        memset(outSig, 0, sizeof(FONTSIGNATURE));
    }

    return mozce_GetTextCharset(inDC);
}


MOZCE_SHUNT_API UINT mozce_GetOutlineTextMetrics(HDC inDC, UINT inData, void* outOTM)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("-- mozce_GetOutlineTextMetrics called.\n");
#endif

    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return 0;
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

MOZCE_SHUNT_API int mozce_EnumFontFamiliesEx(HDC inDC, LPLOGFONT inLogfont, FONTENUMPROC inFunc, LPARAM inParam, DWORD inFlags)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("mozce_EnumFontFamiliesEx called\n");
#endif

    int retval = 0;

    //  We support only one case.
    //  Callback should be oldstyle EnumFonts.
    if(DEFAULT_CHARSET != inLogfont->lfCharSet)
    {
#ifdef DEBUG
        mozce_printf("mozce_EnumFontFamiliesEx failed\n");
#endif
        SetLastError(ERROR_NOT_SUPPORTED);
        return retval;
    }
     
    CollectFaces collection;
    collection.mCount = 0;
    
    EnumFonts(inDC, NULL, collectProc, (LPARAM)&collection);
    
    UINT loop;
    for(loop = 0; loop < collection.mCount; loop++)
    {
        retval = EnumFonts(inDC, collection.mNames[loop], inFunc, inParam);
    }
    
    for(loop = 0; loop < collection.mCount; loop++)
    {
        free(collection.mNames[loop]);
    }

    return retval;
}

MOZCE_SHUNT_API int mozce_GetMapMode(HDC inDC)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("mozce_GetMapMode called\n");
#endif

    int retval = MM_TEXT;
    return retval;
}


MOZCE_SHUNT_API BOOL mozce_GetIconInfo(HICON inIcon, PICONINFO outIconinfo)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("-- mozce_GetIconInfo called\n");
#endif

    BOOL retval = FALSE;

    if(NULL != outIconinfo)
    {
        memset(outIconinfo, 0, sizeof(ICONINFO));
    }

    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);

    return retval;
}


MOZCE_SHUNT_API BOOL mozce_LPtoDP(HDC inDC, LPPOINT inoutPoints, int inCount)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("-- mozce_LPtoDP called\n");
#endif

    BOOL retval = TRUE;

    return retval;
}


MOZCE_SHUNT_API LONG mozce_RegCreateKey(HKEY inKey, LPCTSTR inSubKey, PHKEY outResult)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("mozce_RegCreateKey called\n");
#endif

    LONG retval = ERROR_SUCCESS;
    DWORD disp = 0;

    retval = RegCreateKeyEx(inKey, inSubKey, 0, NULL, 0, 0, NULL, outResult, &disp);

    return retval;
}


MOZCE_SHUNT_API BOOL mozce_WaitMessage(VOID)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("mozce_WaitMessage called\n");
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


MOZCE_SHUNT_API BOOL mozce_FlashWindow(HWND inWnd, BOOL inInvert)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("-- mozce_FlashWindow called\n");
#endif

    BOOL retval = FALSE;

    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);

    return retval;
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

MOZCE_SHUNT_API BOOL mozce_EnumChildWindows(HWND inParent, WNDENUMPROC inFunc, LPARAM inParam)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("mozce_EnumChildWindows called\n");
#endif

    ECWWindows myParams;
    myParams.params = inParam;
    myParams.func   = inFunc;
    myParams.parent = inParent;

    return EnumWindows(MyEnumWindowsProc, (LPARAM) &myParams);
}


MOZCE_SHUNT_API BOOL mozce_EnumThreadWindows(DWORD inThreadID, WNDENUMPROC inFunc, LPARAM inParam)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("-- mozce_EnumThreadWindows called\n");
#endif
    return FALSE; // Stop Enumerating
}


MOZCE_SHUNT_API BOOL mozce_IsIconic(HWND inWnd)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("mozce_IsIconic called\n");
#endif

    BOOL retval = FALSE;
    return retval;
}


MOZCE_SHUNT_API BOOL mozce_OpenIcon(HWND inWnd)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("mozce_OpenIcon called\n");
#endif
    return SetActiveWindow(inWnd) ? 1:0;
}


MOZCE_SHUNT_API HHOOK mozce_SetWindowsHookEx(int inType, void* inFunc, HINSTANCE inMod, DWORD inThreadId)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("-- mozce_SetWindowsHookEx called\n");
#endif

    HHOOK retval = NULL;

    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);

    return retval;
}


MOZCE_SHUNT_API BOOL mozce_UnhookWindowsHookEx(HHOOK inHook)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("-- mozce_UnhookWindowsHookEx called\n");
#endif

    BOOL retval = FALSE;

    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);

    return retval;
}


MOZCE_SHUNT_API LRESULT mozce_CallNextHookEx(HHOOK inHook, int inCode, WPARAM wParam, LPARAM lParam)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("-- mozce_CallNextHookEx called\n");
#endif

    LRESULT retval = NULL;

    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);

    return retval;
}


MOZCE_SHUNT_API BOOL mozce_InvertRgn(HDC inDC, HRGN inRGN)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("-- mozce_InvertRgn called\n");
#endif

    BOOL retval = FALSE;

    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);

    return retval;
}


MOZCE_SHUNT_API int mozce_GetScrollPos(HWND inWnd, int inBar)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("mozce_GetScrollPos called\n");
#endif

    int retval = 0;
    SCROLLINFO info;

    if(GetScrollInfo(inWnd, inBar, &info))
    {
        return info.nPos;
    }

    return retval;
}


MOZCE_SHUNT_API BOOL mozce_GetScrollRange(HWND inWnd, int inBar, LPINT outMinPos, LPINT outMaxPos)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("mozce_GetScrollRange called\n");
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


MOZCE_SHUNT_API HRESULT mozce_CoLockObjectExternal(IUnknown* inUnk, BOOL inLock, BOOL inLastUnlockReleases)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("mozce_CoLockObjectExternal called\n");
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
MOZCE_SHUNT_API HRESULT mozce_OleQueryLinkFromData(IDataObject* inSrcDataObject)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("-- mozce_OleQueryLinkFromData called\n");
#endif

    HRESULT retval = E_NOTIMPL;

    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);

    return retval;
}

//LPITEMIDLIST
MOZCE_SHUNT_API void* mozce_SHBrowseForFolder(void* /*LPBROWSEINFOS*/ inBI)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("-- mozce_SHBrowseForFolder called\n");
#endif

    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return NULL;
}


MOZCE_SHUNT_API BOOL mozce_SetMenu(HWND inWnd, HMENU inMenu)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("-- mozce_SetMenu called\n");
#endif
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return FALSE;
}


MOZCE_SHUNT_API BOOL mozce_GetUserName(LPTSTR inBuffer, LPDWORD inoutSize)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("-- mozce_GetUserName called\n");
#endif

    BOOL retval = FALSE;

    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    *inoutSize = 0;

    return retval;
}


MOZCE_SHUNT_API DWORD mozce_GetShortPathName(LPCTSTR inLongPath, LPTSTR outShortPath, DWORD inBufferSize)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("-- mozce_GetShortPathName called\n");
#endif

    DWORD retval = 0;

    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);

    return retval;
}

MOZCE_SHUNT_API DWORD mozce_GetEnvironmentVariable(LPCSTR lpName, LPCSTR lpBuffer, DWORD nSize)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("-- mozce_GetEnvironmentVariable called\n");
#endif

    DWORD retval = 0;

    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);

    return retval;
}

MOZCE_SHUNT_API void mozce_GetSystemTimeAsFileTime(LPFILETIME lpSystemTimeAsFileTime)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("mozce_GetSystemTimeAsFileTime called\n");
#endif

    SYSTEMTIME st;
    GetSystemTime(&st);
    SystemTimeToFileTime(&st,lpSystemTimeAsFileTime);
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

MOZCE_SHUNT_API DWORD mozce_GetCurrentThreadId(void)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("mozce_GetCurrentThreadId called\n");
#endif
    return GetCurrentThreadId();
}


MOZCE_SHUNT_API DWORD mozce_GetCurrentProcessId(void)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("mozce_GetCurrentProcessId called\n");
#endif
    return GetCurrentProcessId();
}

MOZCE_SHUNT_API DWORD mozce_TlsAlloc(void)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("mozce_TlsAlloc called\n");
#endif
    return TlsAlloc();
}

MOZCE_SHUNT_API BOOL mozce_TlsFree(DWORD dwTlsIndex)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("mozce_TlsFree called\n");
#endif
    return TlsFree(dwTlsIndex);
}

MOZCE_SHUNT_API HANDLE mozce_GetCurrentProcess(void)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("mozce_GetCurrentProcess called\n");
#endif
    return GetCurrentProcess();
}

#ifndef MIN
#define MIN(a,b) (((a)<(b)) ? (a) : (b))
#endif

MOZCE_SHUNT_API DWORD mozce_GetFullPathName(LPCSTR lpFileName, 
                                            DWORD nBufferLength, 
                                            LPCSTR lpBuffer, 
                                            LPCSTR* lpFilePart)
{
#ifdef DEBUG
    mozce_printf("mozce_GetFullPathName called\n");
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
    mozce_printf("mozce_GetFullPathName called %s (%s)\n", lpBuffer, *lpFilePart);
#endif
    return len;	
}

MOZCE_SHUNT_API DWORD mozce_MsgWaitForMultipleObjects(DWORD nCount, const HANDLE* pHandles, BOOL bWaitAll, DWORD dwMilliseconds, DWORD dwWakeMask)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("mozce_MsgWaitForMultipleObjects called\n");
#endif

    return MsgWaitForMultipleObjects(nCount, (HANDLE*) pHandles, bWaitAll, dwMilliseconds, dwWakeMask);
}

MOZCE_SHUNT_API LONG mozce_GetMessageTime(void)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("mozce_GetMessageTime called\n");
#endif
  // Close enough guess?
  return GetTickCount();
}

MOZCE_SHUNT_API UINT mozce_GetACP(void)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("mozce_GetACP called\n");
#endif

    return GetACP();
}



MOZCE_SHUNT_API DWORD mozce_ExpandEnvironmentStrings(LPCTSTR lpSrc, LPTSTR lpDst, DWORD nSize)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("mozce_ExpandEnvironmentStrings called\n");
#endif

    return 0;
}

MOZCE_SHUNT_API BOOL mozce_GdiFlush(void)
{
    MOZCE_PRECHECK

    return TRUE;
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
