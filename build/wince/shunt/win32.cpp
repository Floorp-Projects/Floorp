/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code, released
 * Jan 28, 2003.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 2003 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *    Garrett Arch Blythe, 28-January-2003
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the MPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the MPL or the GPL.
 */

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
    printf("mozce_MulDiv called\n");
    
    if (inDenominator == 0)
        return 0;

    return (int)(((INT64)inNumber * (INT64)inNumerator) / (INT64)inDenominator);
}


MOZCE_SHUNT_API int mozce_GetDIBits(HDC inDC, HBITMAP inBMP, UINT inStartScan, UINT inScanLines, LPVOID inBits, LPBITMAPINFO inInfo, UINT inUsage)
{
    printf("-- mozce_GetDIBits called\n");

    int retval = 0;

    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);

    return retval;
}


MOZCE_SHUNT_API int mozce_SetDIBits(HDC inDC, HBITMAP inBMP, UINT inStartScan, UINT inScanLines, CONST LPVOID inBits, CONST LPBITMAPINFO inInfo, UINT inUsage)
{
    printf("-- mozce_SetDIBits called\n");

    int retval = 0;

    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);

    return retval;
}


MOZCE_SHUNT_API HBITMAP mozce_CreateDIBitmap(HDC inDC, CONST BITMAPINFOHEADER *inBMIH, DWORD inInit, CONST VOID *inBInit, CONST BITMAPINFO *inBMI, UINT inUsage)
{
    printf("-- mozce_CreateDIBitmap called\n");

    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);

    return retval;
}


MOZCE_SHUNT_API int mozce_SetPolyFillMode(HDC inDC, int inPolyFillMode)
{
    printf("-- mozce_SetPolyFillMode called\n");

    int retval = 0;

    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);

    return retval;
}


MOZCE_SHUNT_API int mozce_SetStretchBltMode(HDC inDC, int inStretchMode)
{
    printf("-- mozce_SetStretchBltMode called\n");

    int retval = 0;

    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);

    return retval;
}


MOZCE_SHUNT_API int mozce_ExtSelectClipRgn(HDC inDC, HRGN inRGN, int inMode)
{
    printf("-- mozce_ExtSelectClipRgn called\n");

    int retval = ERROR;

    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);

    return retval;
}

typedef VOID CALLBACK LINEDDAPROC(
  int X,          // x-coordinate of point
  int Y,          // y-coordinate of point
  LPARAM lpData   // application-defined data
);

MOZCE_SHUNT_API BOOL mozce_LineDDA(int inXStart, int inYStart, int inXEnd, int inYEnd, LINEDDAPROC inLineFunc, LPARAM inData)
{
    printf("-- mozce_LineDDA called\n");

    BOOL retval = FALSE;

    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);

    return retval;
}


MOZCE_SHUNT_API int mozce_FrameRect(HDC inDC, CONST RECT *inRect, HBRUSH inBrush)
{
    printf("-- mozce_FrameRect called\n");

    int retval = 0;

    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);

    return retval;
}


MOZCE_SHUNT_API int mozce_SetArcDirection(HDC inDC, int inArcDirection)
{
    printf("-- mozce_SetArcDirection called\n");

    int retval = 0;

    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);

    return retval;
}


MOZCE_SHUNT_API BOOL mozce_Arc(HDC inDC, int inLeftRect, int inTopRect, int inRightRect, int inBottomRect, int inXStartArc, int inYStartArc, int inXEndArc, int inYEndArc)
{
    printf("-- mozce_Arc called\n");

    BOOL retval = FALSE;

    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);

    return retval;
}


MOZCE_SHUNT_API BOOL mozce_Pie(HDC inDC, int inLeftRect, int inTopRect, int inRightRect, int inBottomRect, int inXRadial1, int inYRadial1, int inXRadial2, int inYRadial2)
{
    printf("-- mozce_Pie called\n");

    BOOL retval = FALSE;

    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);

    return retval;
}


MOZCE_SHUNT_API DWORD mozce_GetFontData(HDC inDC, DWORD inTable, DWORD inOffset, LPVOID outBuffer, DWORD inData)
{
    printf("-- mozce_GetFontData called\n");

    DWORD retval = GDI_ERROR;

    if(NULL != outBuffer && 0 < inData)
    {
        memset(outBuffer, 0, inData);
    }

    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);

    return retval;
}


MOZCE_SHUNT_API UINT mozce_GetTextCharset(HDC inDC)
{
    printf("mozce_GetTextCharset called\n");

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
    printf("mozce_GetTextCharsetInfo called\n");

    // A broken implementation.
    if(NULL != outSig)
    {
        memset(outSig, 0, sizeof(FONTSIGNATURE));
    }

    return mozce_GetTextCharset(inDC);
}


MOZCE_SHUNT_API UINT mozce_GetOutlineTextMetrics(HDC inDC, UINT inData, void* outOTM)
{
    printf("-- mozce_GetOutlineTextMetrics called\n");

    UINT retval = 0;

    if(NULL != outOTM)
    {
        //memset(outOTM, 0, sizeof(OUTLINETEXTMETRIC));
    }

    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);

    return retval;
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
    printf("mozce_EnumFontFamiliesEx called\n");

    int retval = 0;

    //  We support only one case.
    //  Callback should be oldstyle EnumFonts.
    if(DEFAULT_CHARSET == inLogfont->lfCharSet)
    {
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
    }
    else
    {
        SetLastError(ERROR_NOT_SUPPORTED);
    }

    return retval;
}


MOZCE_SHUNT_API int mozce_GetMapMode(HDC inDC)
{
    printf("mozce_GetMapMode called\n");

    int retval = -1; //MM_TEXT;

    return retval;
}


MOZCE_SHUNT_API BOOL mozce_GetIconInfo(HICON inIcon, PICONINFO outIconinfo)
{
    printf("-- mozce_GetIconInfo called\n");

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
    printf("-- mozce_LPtoDP called\n");

    BOOL retval = TRUE;

    return retval;
}


MOZCE_SHUNT_API LONG mozce_RegCreateKey(HKEY inKey, LPCTSTR inSubKey, PHKEY outResult)
{
    printf("mozce_RegCreateKey called\n");

    LONG retval = ERROR_SUCCESS;
    DWORD disp = 0;

    retval = RegCreateKeyEx(inKey, inSubKey, 0, NULL, 0, 0, NULL, outResult, &disp);

    return retval;
}


MOZCE_SHUNT_API BOOL mozce_WaitMessage(VOID)
{
    printf("mozce_WaitMessage called\n");

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
    printf("-- mozce_FlashWindow called\n");

    BOOL retval = FALSE;

    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);

    return retval;
}


#define ECW_SIZEBY  0x100
typedef struct __struct_ECWWindows
{
    DWORD       mCount;
    DWORD       mCapacity;
    HWND*       mArray;
}
ECWWindows;

static BOOL ECWHelper(HWND inParent, ECWWindows* inChildren, BOOL inRecurse)
{
    BOOL retval = TRUE;

    HWND child = GetWindow(inParent, GW_CHILD);
    while(NULL != child && FALSE != retval)
    {
        if(inChildren->mCount >= inChildren->mCapacity)
        {
            void* moved = realloc(inChildren->mArray, sizeof(HWND) * (ECW_SIZEBY + inChildren->mCapacity));
            if(NULL != moved)
            {
                inChildren->mCapacity += ECW_SIZEBY;
                inChildren->mArray = (HWND*)moved;
            }
            else
            {
                retval = FALSE;
                break;
            }
        }

        inChildren->mArray[inChildren->mCount] = child;
        inChildren->mCount++;
        if(FALSE != inRecurse)
        {
            retval = ECWHelper(child, inChildren, inRecurse);
        }

        child = GetWindow(child, GW_HWNDNEXT);
    }

    return retval;
}

MOZCE_SHUNT_API BOOL mozce_EnumChildWindows(HWND inParent, WNDENUMPROC inFunc, LPARAM inParam)
{
    printf("mozce_EnumChildWindows called\n");

    BOOL retval = FALSE;

    if(NULL != inFunc)
    {
        if(NULL == inParent)
        {
            inParent = GetDesktopWindow();
        }

        ECWWindows children;
        memset(&children, 0, sizeof(children));
        children.mArray = (HWND*)malloc(sizeof(HWND) * ECW_SIZEBY);
        if(NULL != children.mArray)
        {
            children.mCapacity = ECW_SIZEBY;

            BOOL helperRes = ECWHelper(inParent, &children, TRUE);
            if(FALSE != helperRes)
            {
                DWORD loop = 0;
                for(loop = 0; loop < children.mCount; loop++)
                {
                    if(IsWindow(children.mArray[loop])) // validate
                    {
                        if(FALSE == inFunc(children.mArray[loop], inParam))
                        {
                            break;
                        }
                    }
                }
            }

            free(children.mArray);
        }
    }

    return retval;
}


MOZCE_SHUNT_API BOOL mozce_EnumThreadWindows(DWORD inThreadID, WNDENUMPROC inFunc, LPARAM inParam)
{
    printf("mozce_EnumThreadWindows called\n");

    BOOL retval = FALSE;

    if(NULL != inFunc)
    {
        ECWWindows children;
        memset(&children, 0, sizeof(children));
        children.mArray = (HWND*)malloc(sizeof(HWND) * ECW_SIZEBY);
        if(NULL != children.mArray)
        {
            children.mCapacity = ECW_SIZEBY;

            BOOL helperRes = ECWHelper(GetDesktopWindow(), &children, FALSE);
            if(FALSE != helperRes)
            {
                DWORD loop = 0;
                for(loop = 0; loop < children.mCount; loop++)
                {
                    if(IsWindow(children.mArray[loop]) && inThreadID == GetWindowThreadProcessId(children.mArray[loop], NULL)) // validate
                    {
                        if(FALSE == inFunc(children.mArray[loop], inParam))
                        {
                            break;
                        }
                    }
                }
            }

            free(children.mArray);
        }
    }

    return retval;
}


MOZCE_SHUNT_API BOOL mozce_IsIconic(HWND inWnd)
{
    printf("-- mozce_IsIconic called\n");

    BOOL retval = FALSE;

    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);

    return retval;
}


MOZCE_SHUNT_API BOOL mozce_OpenIcon(HWND inWnd)
{
    printf("-- mozce_OpenIcon called\n");

    BOOL retval = FALSE;

    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);

    return retval;
}


MOZCE_SHUNT_API HHOOK mozce_SetWindowsHookEx(int inType, void* inFunc, HINSTANCE inMod, DWORD inThreadId)
{
    printf("-- mozce_SetWindowsHookEx called\n");

    HHOOK retval = NULL;

    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);

    return retval;
}


MOZCE_SHUNT_API BOOL mozce_UnhookWindowsHookEx(HHOOK inHook)
{
    printf("-- mozce_UnhookWindowsHookEx called\n");

    BOOL retval = FALSE;

    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);

    return retval;
}


MOZCE_SHUNT_API LRESULT mozce_CallNextHookEx(HHOOK inHook, int inCode, WPARAM wParam, LPARAM lParam)
{
    printf("-- mozce_CallNextHookEx called\n");

    LRESULT retval = NULL;

    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);

    return retval;
}


MOZCE_SHUNT_API BOOL mozce_InvertRgn(HDC inDC, HRGN inRGN)
{
    printf("-- mozce_InvertRgn called\n");

    BOOL retval = FALSE;

    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);

    return retval;
}


MOZCE_SHUNT_API int mozce_GetScrollPos(HWND inWnd, int inBar)
{
    printf("mozce_GetScrollPos called\n");

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
    printf("mozce_GetScrollRange called\n");

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
    printf("mozce_CoLockObjectExternal called\n");

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


MOZCE_SHUNT_API HRESULT mozce_OleSetClipboard(IDataObject* inDataObj)
{
    printf("-- mozce_OleSetClipboard called\n");

    HRESULT retval = E_NOTIMPL;

    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);

    return retval;
}


MOZCE_SHUNT_API HRESULT mozce_OleGetClipboard(IDataObject** outDataObj)
{
    printf("-- mozce_OleGetClipboard called\n");

    HRESULT retval = E_NOTIMPL;

    if(NULL != outDataObj)
    {
        *outDataObj = NULL;
    }
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);

    return retval;
}


MOZCE_SHUNT_API HRESULT mozce_OleFlushClipboard(void)
{
    printf("-- mozce_OleFlushClipboard called\n");

    HRESULT retval = E_NOTIMPL;

    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);

    return retval;
}


MOZCE_SHUNT_API HRESULT mozce_OleQueryLinkFromData(IDataObject* inSrcDataObject)
{
    printf("-- mozce_OleQueryLinkFromData called\n");

    HRESULT retval = E_NOTIMPL;

    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);

    return retval;
}

//LPITEMIDLIST
MOZCE_SHUNT_API void* mozce_SHBrowseForFolder(void* /*LPBROWSEINFOS*/ inBI)
{
    printf("-- mozce_SHBrowseForFolder called\n");

    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return NULL;
}


MOZCE_SHUNT_API BOOL mozce_SetMenu(HWND inWnd, HMENU inMenu)
{
    printf("-- mozce_SetMenu called\n");

    BOOL retval = FALSE;

    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);

    return retval;
}


MOZCE_SHUNT_API BOOL mozce_GetUserName(LPTSTR inBuffer, LPDWORD inoutSize)
{
    printf("-- mozce_GetUserName called\n");

    BOOL retval = FALSE;

    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    *inoutSize = 0;

    return retval;
}


MOZCE_SHUNT_API DWORD mozce_GetShortPathName(LPCTSTR inLongPath, LPTSTR outShortPath, DWORD inBufferSize)
{
    printf("-- mozce_GetShortPathName called\n");

    DWORD retval = 0;

    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);

    return retval;
}

MOZCE_SHUNT_API DWORD mozce_GetEnvironmentVariable(LPCTSTR lpName, LPSTR lpBuffer, DWORD nSize)
{
    printf("-- mozce_GetEnvironmentVariable called\n");

    DWORD retval = 0;

    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);

    return retval;
}

MOZCE_SHUNT_API void mozce_GetSystemTimeAsFileTime(LPFILETIME lpSystemTimeAsFileTime)
{
    printf("mozce_GetSystemTimeAsFileTime called\n");

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
    printf("mozce_localeconv called\n");
    return &s_locale_conv;
}

MOZCE_SHUNT_API DWORD mozce_GetCurrentThreadId(void)
{
    printf("mozce_GetCurrentThreadId called\n");
    return GetCurrentThreadId();
}

MOZCE_SHUNT_API DWORD mozce_TlsAlloc(void)
{
    printf("mozce_TlsAlloc called\n");
    return TlsAlloc();
}

MOZCE_SHUNT_API BOOL mozce_TlsFree(DWORD dwTlsIndex)
{
    printf("mozce_TlsFree called\n");
    return TlsFree(dwTlsIndex);
}

MOZCE_SHUNT_API HANDLE mozce_GetCurrentProcess(void)
{
    printf("mozce_GetCurrentProcess called\n");
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

    printf("mozce_GetFullPathName called %s (%s)\n", lpBuffer, *lpFilePart);
    return len;	
}

MOZCE_SHUNT_API DWORD mozce_MsgWaitForMultipleObjects(DWORD nCount, const HANDLE* pHandles, BOOL bWaitAll, DWORD dwMilliseconds, DWORD dwWakeMask)
{
    return MsgWaitForMultipleObjects(nCount, (HANDLE*) pHandles, bWaitAll, dwMilliseconds, dwWakeMask);
}


#if 0
{
#endif
} /* extern "C" */
