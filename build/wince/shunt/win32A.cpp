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

#include <commdlg.h>

extern "C" {
#if 0
}
#endif

/*
**  Help figure the character count of a TCHAR array.
*/

MOZCE_SHUNT_API DWORD mozce_GetModuleFileNameA(HMODULE hModule, LPSTR lpFilename, DWORD nSize)
{
    printf("mozce_GetModuleFileNameA called\n");
    
    TCHAR wideStr[MAX_PATH];
    
    DWORD result = w2a_buffer(wideStr, 
                              GetModuleFileNameW(hModule, 
                                                 wideStr, 
                                                 charcount(wideStr)), 
                              lpFilename, nSize);
    lpFilename[result] = '\0';

    return result;
}

MOZCE_SHUNT_API BOOL mozce_CreateDirectoryA(LPCSTR lpPathName, LPSECURITY_ATTRIBUTES lpSecurityAttributes)
{
    printf("mozce_CreateDirectoryA called (%s)\n", lpPathName);
    
    BOOL retval = FALSE;
    TCHAR wideStr[MAX_PATH];

    if(a2w_buffer(lpPathName, -1, wideStr, charcount(wideStr)))
    {
        retval = CreateDirectoryW(wideStr, lpSecurityAttributes);
    }
    
    return retval;
}

MOZCE_SHUNT_API BOOL mozce_RemoveDirectoryA(LPCSTR lpPathName)
{
    printf("mozce_RemoveDirectoryA called\n");

    BOOL retval = FALSE;
    TCHAR wideStr[MAX_PATH];

    if(a2w_buffer(lpPathName, -1, wideStr, charcount(wideStr)))
    {
        retval = RemoveDirectoryW(wideStr);
    }
    return retval;
}

MOZCE_SHUNT_API BOOL mozce_DeleteFileA(LPCSTR lpFileName)
{
    printf("mozce_DeleteFile called\n");

    BOOL retval = FALSE;
    TCHAR wideStr[MAX_PATH];

    if(a2w_buffer(lpFileName, -1, wideStr, charcount(wideStr)))
    {
        retval = DeleteFileW(wideStr);
    }
    return retval;
}

MOZCE_SHUNT_API BOOL mozce_MoveFileA(LPCSTR lpExistingFileName, LPCSTR lpNewFileName)
{
    printf("mozce_MoveFileA called (%s)\n", lpExistingFileName);

    BOOL retval = FALSE;
    TCHAR wideStr[2][MAX_PATH];

    if(a2w_buffer(lpExistingFileName, -1, wideStr[0], charcount(wideStr[0])) &&
       a2w_buffer(lpNewFileName, -1, wideStr[1], charcount(wideStr[1])))
    {
        retval = MoveFileW(wideStr[0], wideStr[1]);
    }
    return retval;
}


MOZCE_SHUNT_API BOOL mozce_CopyFileA(LPCSTR lpExistingFileName, LPCSTR lpNewFileName, BOOL bFailIfExists)
{
    printf("mozce_CopyFileA called\n");

    BOOL retval = FALSE;
    TCHAR wideStr[2][MAX_PATH];

    if(
        a2w_buffer(lpExistingFileName, -1, wideStr[0], charcount(wideStr[0])) &&
        a2w_buffer(lpNewFileName, -1, wideStr[1], charcount(wideStr[1]))
        )
    {
        retval = CopyFileW(wideStr[0], wideStr[1], bFailIfExists);
    }
    printf("CopyFileA\n");

    return retval;
}


MOZCE_SHUNT_API HANDLE mozce_CreateFileA(LPCSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile)
{
    printf("mozce_CreateFileA called (%s)\n", lpFileName);

    HANDLE retval = INVALID_HANDLE_VALUE;
    TCHAR wideStr[MAX_PATH];

    if(a2w_buffer(lpFileName, -1, wideStr, charcount(wideStr)))
    {
        retval = CreateFileW(wideStr, 
                             dwDesiredAccess, 
                             dwShareMode, 
                             lpSecurityAttributes, 
                             dwCreationDisposition, 
                             dwFlagsAndAttributes, 
                             hTemplateFile);
    }
    return retval;
}


MOZCE_SHUNT_API DWORD mozce_GetFileAttributesA(LPCSTR lpFileName)
{
    printf("mozce_GetFileAttributesA called\n");

    DWORD retval = (DWORD)-1;
    TCHAR wideStr[MAX_PATH];

    if(a2w_buffer(lpFileName, -1, wideStr, charcount(wideStr)))
    {
        retval = GetFileAttributesW(wideStr);
    }

    printf("GetFileAttributesA\n");
    return retval;
}

MOZCE_SHUNT_API BOOL mozce_CreateProcessA(LPCSTR pszImageName, LPCSTR pszCmdLine, LPSECURITY_ATTRIBUTES psaProcess, LPSECURITY_ATTRIBUTES psaThread, BOOL fInheritHandles, DWORD fdwCreate, LPVOID pvEnvironment, LPSTR pszCurDir, LPSTARTUPINFO psiStartInfo, LPPROCESS_INFORMATION pProcInfo)
{
    printf("mozce_CreateProcessA called\n");

    BOOL retval = FALSE;
    TCHAR pszImageNameW[MAX_PATH];

    if(a2w_buffer(pszImageName, -1, pszImageNameW, charcount(pszImageNameW)))
    {
        LPTSTR pszCmdLineW = NULL;

        pszCmdLineW = a2w_malloc(pszCmdLine, -1, NULL);
        if(NULL != pszCmdLineW || NULL == pszCmdLine)
        {
            retval = CreateProcessW(pszImageNameW, pszCmdLineW, NULL, NULL, FALSE, fdwCreate, NULL, NULL, NULL, pProcInfo);

            if(NULL != pszCmdLineW)
            {
                free(pszCmdLineW);
            }
        }
    }

    return retval;
}


MOZCE_SHUNT_API int mozce_GetLocaleInfoA(LCID Locale, LCTYPE LCType, LPSTR lpLCData, int cchData)
{
    printf("mozce_GetLocaleInfoA called\n");

    int retval = 0;
    int neededChars = 0;

    neededChars = GetLocaleInfoW(Locale, LCType, NULL, 0);
    if(0 != neededChars)
    {
        LPTSTR buffer = NULL;

        buffer = (LPTSTR)malloc(neededChars * sizeof(TCHAR));
        if(NULL != buffer)
        {
            int gotChars = 0;

            gotChars = GetLocaleInfoW(Locale, LCType, buffer, neededChars);
            if(0 != gotChars)
            {
                if(0 == cchData)
                {
                    retval = WideCharToMultiByte(
                        CP_ACP,
                        0,
                        buffer,
                        neededChars,
                        NULL,
                        0,
                        NULL,
                        NULL
                        );

                }
                else
                {
                    retval = w2a_buffer(buffer, neededChars, lpLCData, cchData);
                }
            }

            free(buffer);
        }
    }

    return retval;
}

MOZCE_SHUNT_API UINT mozce_GetWindowsDirectoryA(LPSTR inBuffer, UINT inSize)
{
    printf("mozce_GetWindowsDirectoryA called\n");

    UINT retval = 0;

    if(inSize < 9)
    {
        retval = 9;
    }
    else
    {
        strcpy(inBuffer, "\\WINDOWS");
        retval = 8;
    }

    printf("GetWindowsDirectoryA\n");
    return retval;
}


MOZCE_SHUNT_API UINT mozce_GetSystemDirectoryA(LPSTR inBuffer, UINT inSize)
{
    printf("mozce_GetSystemDirectoryA called\n");

    UINT retval = 0;

    if(inSize < 9)
    {
        retval = 9;
    }
    else
    {
        strcpy(inBuffer, "\\WINDOWS");
        retval = 8;
    }

    printf("GetSystemDirectoryA\n");
    return retval;
}

MOZCE_SHUNT_API LONG mozce_RegOpenKeyExA(HKEY inKey, LPCSTR inSubKey, DWORD inOptions, REGSAM inSAM, PHKEY outResult)
{
    printf("mozce_RegOpenKeyExA called\n");

    LONG retval = ERROR_GEN_FAILURE;

    LPTSTR wSubKey = a2w_malloc(inSubKey, -1, NULL);
    if(NULL != wSubKey)
    {
        retval = RegOpenKeyEx(inKey, wSubKey, inOptions, inSAM, outResult);
        free(wSubKey);
    }

    printf("RegOpenKeyExA\n");
    return retval;
}


MOZCE_SHUNT_API LONG mozce_RegQueryValueExA(HKEY inKey, LPCSTR inValueName, LPDWORD inReserved, LPDWORD outType, LPBYTE inoutBData, LPDWORD inoutDData)
{
    printf("mozce_RegQueryValueExA called\n");

    LONG retval = ERROR_GEN_FAILURE;

    LPTSTR wName = a2w_malloc(inValueName, -1, NULL);
    if(NULL != wName)
    {
        DWORD tempSize = *inoutDData * sizeof(TCHAR); /* in bytes */
        LPTSTR tempData = (LPTSTR)malloc(tempSize);
        if(NULL != tempData)
        {
            retval = RegQueryValueEx(inKey, wName, inReserved, outType, (LPBYTE)tempData, &tempSize);

            /*
            **  Convert to ANSI if a string....
            */
            if(ERROR_SUCCESS == retval && (
                REG_EXPAND_SZ == *outType ||
                REG_MULTI_SZ == *outType ||
                REG_SZ == *outType
                ))
            {
                *inoutDData = (DWORD)w2a_buffer(tempData, tempSize / sizeof(TCHAR), (LPSTR)inoutBData, *inoutDData);
            }
            else
            {
                memcpy(inoutBData, tempData, tempSize);
                *inoutDData = tempSize;
            }

            free(tempData);
        }

        free(wName);
    }

    printf("RegQueryValueExA\n");
    return retval;
}

MOZCE_SHUNT_API int mozce_MessageBoxA(HWND inWnd, LPCSTR inText, LPCSTR inCaption, UINT uType)
{
    printf("mozce_MessageBoxA called\n");

    int retval = 0;
    LPTSTR wCaption = a2w_malloc(inCaption, -1, NULL);

    if(NULL != wCaption)
    {
        LPTSTR wText = a2w_malloc(inText, -1, NULL);

        if(NULL != wText)
        {
            retval = MessageBox(inWnd, wText, wCaption, uType);
            free(wText);
        }
        free(wCaption);
    }

    return retval;
}


MOZCE_SHUNT_API HANDLE mozce_OpenSemaphoreA(DWORD inDesiredAccess, BOOL inInheritHandle, LPCSTR inName)
{
    printf("mozce_OpenSemaphoreA called\n");

    HANDLE retval = NULL;
    LPTSTR wName = a2w_malloc(inName, -1, NULL);

    if(NULL != wName)
    {
        extern HANDLE mozce_OpenSemaphoreW(DWORD inDesiredAccess, BOOL inInheritHandle, LPCWSTR inName);
        retval = mozce_OpenSemaphoreW(inDesiredAccess, inDesiredAccess, wName);
        free(wName);
    }

    printf("OpenSemaphoreA\n");
    return retval;
}


MOZCE_SHUNT_API HDC mozce_CreateDCA(LPCSTR inDriver, LPCSTR inDevice, LPCSTR inOutput, CONST DEVMODEA* inInitData)
{
    printf("mozce_CreateDCA called\n");

    HDC retval = NULL;

    LPTSTR wDriver = a2w_malloc(inDriver, -1, NULL);
    LPTSTR wDevice = a2w_malloc(inDevice, -1, NULL);
    LPTSTR wOutput = a2w_malloc(inOutput, -1, NULL);

    DEVMODE wInitData;
    memset(&wInitData, 0, sizeof(wInitData));

    wInitData.dmSpecVersion = inInitData->dmSpecVersion;
    wInitData.dmDriverVersion = inInitData->dmDriverVersion;
    wInitData.dmSize = inInitData->dmSize;
    wInitData.dmDriverExtra = inInitData->dmDriverExtra;
    wInitData.dmFields = inInitData->dmFields;
    wInitData.dmOrientation = inInitData->dmOrientation;
    wInitData.dmPaperSize = inInitData->dmPaperSize;
    wInitData.dmPaperLength = inInitData->dmPaperLength;
    wInitData.dmPaperWidth = inInitData->dmPaperWidth;
    wInitData.dmScale = inInitData->dmScale;
    wInitData.dmCopies = inInitData->dmCopies;
    wInitData.dmDefaultSource = inInitData->dmDefaultSource;
    wInitData.dmPrintQuality = inInitData->dmPrintQuality;
    wInitData.dmColor = inInitData->dmColor;
    wInitData.dmDuplex = inInitData->dmDuplex;
    wInitData.dmYResolution = inInitData->dmYResolution;
    wInitData.dmTTOption = inInitData->dmTTOption;
    wInitData.dmCollate = inInitData->dmCollate;
    wInitData.dmLogPixels = inInitData->dmLogPixels;
    wInitData.dmBitsPerPel = inInitData->dmBitsPerPel;
    wInitData.dmPelsWidth = inInitData->dmPelsWidth;
    wInitData.dmPelsHeight = inInitData->dmPelsHeight;
    wInitData.dmDisplayFlags = inInitData->dmDisplayFlags;
    wInitData.dmDisplayFrequency = inInitData->dmDisplayFrequency;

    a2w_buffer((LPCSTR)inInitData->dmDeviceName, -1, wInitData.dmDeviceName, charcount(wInitData.dmDeviceName));
    a2w_buffer((LPCSTR)inInitData->dmFormName, -1, wInitData.dmFormName, charcount(wInitData.dmFormName));

    retval = CreateDC(wDriver, wDevice, wOutput, &wInitData);

    if(NULL != wDriver)
    {
        free(wDriver);
        wDriver = NULL;
    }
    if(NULL != wDevice)
    {
        free(wDevice);
        wDevice = NULL;
    }
    if(NULL != wOutput)
    {
        free(wOutput);
        wOutput = NULL;
    }

    printf("_CreateDCA\n");
    return retval;
}


MOZCE_SHUNT_API HDC mozce_CreateDCA2(LPCSTR inDriver, LPCSTR inDevice, LPCSTR inOutput, CONST DEVMODE* inInitData)
{
    printf("mozce_CreateDCA2 called\n");

    HDC retval = NULL;

    LPTSTR wDriver = a2w_malloc(inDriver, -1, NULL);
    LPTSTR wDevice = a2w_malloc(inDevice, -1, NULL);
    LPTSTR wOutput = a2w_malloc(inOutput, -1, NULL);

    retval = CreateDC(wDriver, wDevice, wOutput, inInitData);

    if(NULL != wDriver)
    {
        free(wDriver);
        wDriver = NULL;
    }
    if(NULL != wDevice)
    {
        free(wDevice);
        wDevice = NULL;
    }
    if(NULL != wOutput)
    {
        free(wOutput);
        wOutput = NULL;
    }

    printf("_CreateDCA2\n");
    return retval;
}


MOZCE_SHUNT_API BOOL mozce_GetTextExtentExPointA(HDC inDC, LPCSTR inStr, int inLen, int inMaxExtent, LPINT outFit, LPINT outDx, LPSIZE inSize)
{
    printf("mozce_GetTextExtentExPointA called\n");

    BOOL retval = FALSE;

    if (!inStr)
        return retval;

    if(inLen == -1)
        inLen = strlen(inStr);

    int wLen = 0;
    LPTSTR wStr = a2w_malloc(inStr, inLen, &wLen);

    if(NULL != wStr)
    {
        retval = GetTextExtentExPointW(inDC, wStr, wLen, inMaxExtent, outFit, outDx, inSize);

        free(wStr);
    }
    return retval;
}


MOZCE_SHUNT_API BOOL mozce_ExtTextOutA(HDC inDC, int inX, int inY, UINT inOptions, const LPRECT inRect, LPCSTR inString, UINT inCount, const LPINT inDx)
{
    printf("mozce_ExtTextOutA called\n");

    BOOL retval = false;

    int wLen = 0;
    LPTSTR wStr = a2w_malloc(inString, inCount, &wLen);

    if(NULL != wStr)
    {
        retval = ExtTextOutW(inDC, inX, inY, inOptions, inRect, wStr, wLen, inDx);

        free(wStr);
        wStr = NULL;
    }

    printf("_ExtTextOutA\n");
    return retval;
}
MOZCE_SHUNT_API DWORD mozce_GetGlyphOutlineA(HDC inDC, CHAR inChar, UINT inFormat, void* inGM, DWORD inBufferSize, LPVOID outBuffer, CONST mozce_MAT2* inMAT2)
{
    printf("-- mozce_GetGlyphOutlineA called\n");

    DWORD retval = GDI_ERROR;

    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);

    printf("GetGlyphOutlineA\n");
    return retval;
}

MOZCE_SHUNT_API DWORD mozce_GetCurrentDirectoryA(DWORD inBufferLength, LPSTR outBuffer)
{
    printf("-- mozce_GetCurrentDirectoryA called\n");

    DWORD retval = 0;

    if(NULL != outBuffer && 0 < inBufferLength)
    {
        outBuffer[0] = '\0';
    }

    SetLastError(ERROR_NOT_SUPPORTED);
    return retval;
}


MOZCE_SHUNT_API BOOL mozce_SetCurrentDirectoryA(LPCSTR inPathName)
{
    printf("-- mozce_SetCurrentDirectoryA called\n");

    BOOL retval = FALSE;

    SetLastError(ERROR_NOT_SUPPORTED);
    return retval;
}

MOZCE_SHUNT_API LONG mozce_RegEnumKeyExA(HKEY inKey, DWORD inIndex, LPSTR outName, LPDWORD inoutName, LPDWORD inReserved, LPSTR outClass, LPDWORD inoutClass, PFILETIME inLastWriteTime)
{
    printf("mozce_RegEnumKeyExA called\n");

    LONG retval = ERROR_NOT_ENOUGH_MEMORY;

    LPTSTR wName = (LPTSTR)malloc(sizeof(TCHAR) * *inoutName);
    DWORD wNameChars = *inoutName;
    if(NULL != wName)
    {
        LPTSTR wClass = NULL;
        DWORD wClassChars = 0;

        if(NULL != outClass)
        {
            wClass = (LPTSTR)malloc(sizeof(TCHAR) * *inoutClass);
            wClassChars = *inoutClass;
        }

        if(NULL == outClass || NULL != wClass)
        {
            retval = RegEnumKeyEx(inKey, inIndex, wName, &wNameChars, inReserved, wClass, &wClassChars, inLastWriteTime);
            if(ERROR_SUCCESS == retval)
            {
                *inoutName = w2a_buffer(wName, wNameChars + 1, outName, *inoutName);
                if(NULL != wClass)
                {
                    *inoutClass = w2a_buffer(wClass, wClassChars + 1, outClass, *inoutClass);
                }
            }
        }

        if(NULL != wClass)
        {
            free(wClass);
        }
        free(wName);
    }

    return retval;
}


MOZCE_SHUNT_API BOOL mozce_GetFileVersionInfoA(LPSTR inFilename, DWORD inHandle, DWORD inLen, LPVOID outData)
{
    printf("mozce_GetFileVersionInfoA called\n");

    BOOL retval = FALSE;
    TCHAR wPath[MAX_PATH];

    if(0 != a2w_buffer(inFilename, -1, wPath, charcount(wPath)))
    {
        retval = GetFileVersionInfo(wPath, inHandle, inLen, outData);
    }
    return retval;
}


MOZCE_SHUNT_API DWORD mozce_GetFileVersionInfoSizeA(LPSTR inFilename, LPDWORD outHandle)
{
    printf("mozce_GetFileVersionInfoSizeA called\n");

    DWORD retval = 0;

    TCHAR wPath[MAX_PATH];

    if(0 != a2w_buffer(inFilename, -1, wPath, charcount(wPath)))
    {
        retval = GetFileVersionInfoSize(wPath, outHandle);
    }
    return retval;
}


MOZCE_SHUNT_API BOOL mozce_VerQueryValueA(const LPVOID inBlock, LPSTR inSubBlock, LPVOID *outBuffer, PUINT outLen)
{
    printf("mozce_VerQueryValueA called\n");

    BOOL retval = FALSE;
    LPTSTR wBlock = NULL;

    wBlock = a2w_malloc(inSubBlock, -1, NULL);
    if(NULL != wBlock)
    {
        retval = VerQueryValue(inBlock, wBlock, outBuffer, outLen);
        free(wBlock);
        wBlock = NULL;
    }
    return retval;
}

MOZCE_SHUNT_API int mozce_LoadStringA(HINSTANCE inInstance, UINT inID, LPSTR outBuffer, int inBufferMax)
{
    printf("mozce_LoadStringA called\n");

    int retval = 0;

    if(NULL != outBuffer && 0 < inBufferMax)
    {
        LPTSTR wBuffer = (LPTSTR)malloc(sizeof(TCHAR) * inBufferMax);
        if(NULL != wBuffer)
        {
            retval = LoadString(inInstance, inID, wBuffer, inBufferMax);
            if(0 < retval)
            {
                retval = w2a_buffer(wBuffer, retval + 1, outBuffer, inBufferMax);
            }
            free(wBuffer);
        }
    }

    return retval;
}


MOZCE_SHUNT_API VOID mozce_OutputDebugStringA(LPCSTR inOutputString)
{
    printf("mozce_OutputDebugStringA called\n");

    LPTSTR wideStr = NULL;

    wideStr = a2w_malloc(inOutputString, -1, NULL);
    if(NULL != wideStr)
    {
        OutputDebugString(wideStr);
        free(wideStr);
    }
}


MOZCE_SHUNT_API int mozce_DrawTextA(HDC inDC, LPCSTR inString, int inCount, LPRECT inRect, UINT inFormat)
{
    printf("mozce_DrawTextA called\n");

    int retval = 0;
    int wStringLen = 0;
    LPTSTR wString = a2w_malloc(inString, inCount, &wStringLen);
    if(NULL != wString)
    {
        retval = DrawText(inDC, wString, wStringLen, inRect, inFormat);
        free(wString);
    }

    return retval;
}


MOZCE_SHUNT_API BOOL mozce_SetDlgItemTextA(HWND inDlg, int inIDDlgItem, LPCSTR inString)
{
    printf("mozce_SetDlgItemTextA called\n");

    BOOL retval = FALSE;
    LPTSTR wString = a2w_malloc(inString, -1, NULL);
    if(NULL != wString)
    {
        retval = SetDlgItemText(inDlg, inIDDlgItem, wString);
        free(wString);
    }

    return retval;
}


MOZCE_SHUNT_API HANDLE mozce_LoadImageA(HINSTANCE inInst, LPCSTR inName, UINT inType, int inCX, int inCY, UINT inLoad)
{
    printf("mozce_LoadImageA called\n");

    HANDLE retval = NULL;

    LPTSTR wName = a2w_malloc(inName, -1, NULL);
    if(NULL != wName)
    {
        retval = LoadImage(inInst, wName, inType, inCX, inCY, inLoad);
        free(wName);
    }

    return retval;
}


MOZCE_SHUNT_API HWND mozce_FindWindowA(LPCSTR inClass, LPCSTR inWindow)
{
    printf("mozce_FindWindowA called\n");

    HWND retval = NULL;

    LPTSTR wClass = a2w_malloc(inClass, -1, NULL);
    if(NULL != wClass)
    {
        if(NULL == inWindow)
        {
            retval = FindWindow(wClass, NULL);
        }
        else
        {
            LPTSTR wWindow = a2w_malloc(inWindow, -1, NULL);
            if(NULL != wWindow)
            {
                retval = FindWindow(wClass, wWindow);
                free(wWindow);
            }
        }
        free(wClass);
    }

    return retval;
}


MOZCE_SHUNT_API UINT mozce_RegisterClipboardFormatA(LPCSTR inFormat)
{
    printf("mozce_RegisterClipboardFormatA called\n");

    UINT retval = 0;

    LPTSTR wFormat = a2w_malloc(inFormat, -1, NULL);
    if(NULL != wFormat)
    {
        retval = RegisterClipboardFormat(wFormat);
        free(wFormat);
    }

    return retval;
}


MOZCE_SHUNT_API DWORD mozce_GetEnvironmentVariableA(LPSTR lpName, LPSTR lpBuffer, DWORD nSize)
{
    printf("-- mozce_GetEnvironmentVariableA called\n");

    DWORD retval = -1;

    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);

    printf("mozce_GetEnvironmentVariableA\n");
    return retval; 
}

MOZCE_SHUNT_API DWORD mozce_SetEnvironmentVariableA(LPSTR lpName, LPSTR lpBuffer)
{    
    printf("-- mozce_SetEnvironmentVariableA called\n");

    DWORD retval = -1;

    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);

    printf("mozce_SetEnvironmentVariableA\n");
    return retval; 
}


MOZCE_SHUNT_API LRESULT mozce_SendMessageA(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    printf("mozce_SendMessageA called\n");

    wchar_t *lpNewText = NULL;
    LRESULT res;
    LPARAM lParamOrig = lParam;
    
    if(msg == CB_ADDSTRING    || msg == CB_FINDSTRING   || msg == CB_FINDSTRINGEXACT ||
       msg == CB_INSERTSTRING || msg == CB_SELECTSTRING || msg == EM_REPLACESEL ||
       msg == LB_ADDSTRING    || msg == LB_FINDSTRING   || msg == LB_FINDSTRINGEXACT ||
       msg == LB_INSERTSTRING || msg == LB_SELECTSTRING || msg == WM_SETTEXT)
    {
        lParam = (LPARAM) a2w_malloc((char*)lParam, -1, NULL);
    }
    else if(msg == WM_GETTEXT)
    {
        lpNewText = (unsigned short*) malloc(wParam * 2);
        lParam = (LPARAM) lpNewText;
        lpNewText[0] = 0;
    }
    else if(msg == LB_GETTEXT || msg == CB_GETLBTEXT)
    {
        lpNewText = (unsigned short*) malloc(512);
        lParam = (LPARAM) lpNewText;
        lpNewText[0] = 0;
    }
#ifdef DEBUG
    else if (msg >= WM_USER)
    {
        // user message -- how the heck can this be converted, or does it need to??
    }
    else
    {
        // need possible conversion??
        printf("  need possible conversion %d\n", msg);
    }
#endif
    
    res = SendMessageW(hWnd, msg, wParam, lParam);
    
    if(msg == WM_GETTEXT)
    {
        w2a_buffer(lpNewText, -1, (char*) lParamOrig, wParam);
    }
    else if(msg == LB_GETTEXT)
    {
        w2a_buffer(lpNewText, -1, (char*) lParamOrig, 512);
    }
#ifdef DEBUG
    else if (msg == CB_ADDSTRING)
    {
    }
    else
    {
        printf("  need possible out conversion %d\n", msg);        
    }
#endif

    if(lpNewText)
        free(lpNewText);

  return res;
}


MOZCE_SHUNT_API ATOM mozce_GlobalAddAtomA(LPCSTR lpString)
{
    printf("mozce_GlobalAddAtomA called (%s)\n", lpString);

    LPTSTR watom = a2w_malloc(lpString, -1, NULL);
    if (!watom)
        return NULL;

    ATOM a = GlobalAddAtomW(watom);

    free(watom);
    printf("returning %x\n", a);

    return a; 
}

MOZCE_SHUNT_API LRESULT mozce_PostMessageA(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    printf("mozce_PostMessageA called\n");

    return PostMessageW(hWnd, msg, wParam, lParam);
}

MOZCE_SHUNT_API BOOL mozce_GetVersionExA(LPOSVERSIONINFOA lpv)
{
    printf("mozce_GetVersionExA called\n");

    OSVERSIONINFOW vw;
    vw.dwOSVersionInfoSize = sizeof(vw);

    GetVersionExW(&vw);

    memcpy(lpv, &vw, 5 * sizeof(DWORD));

    w2a_buffer(vw.szCSDVersion, -1, lpv->szCSDVersion, sizeof(lpv->szCSDVersion));    
    return TRUE;
}

MOZCE_SHUNT_API BOOL mozce_PeekMessageA(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax, UINT wRemoveMsg)
{
#ifdef LOUD_PEEKMESSAGE
    printf("mozce_PeekMessageA called\n");
#endif

    return PeekMessageW(lpMsg, hWnd, wMsgFilterMin, wMsgFilterMax, wRemoveMsg);
}


MOZCE_SHUNT_API LRESULT mozce_DefWindowProcA(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
    printf("mozce_DefWindowProcA called\n");

    return DefWindowProcW(hWnd, Msg, wParam, lParam);
}


MOZCE_SHUNT_API HWND mozce_CreateWindowExA(DWORD dwExStyle, 
                                           LPCSTR lpClassName, 
                                           LPCSTR lpWindowName, 
                                           DWORD dwStyle, 
                                           int x, int y, 
                                           int nWidth, int nHeight, 
                                           HWND hWndParent, 
                                           HMENU hMenu, 
                                           HINSTANCE hInstance, 
                                           LPVOID lpParam )
{
    TCHAR classNameW[MAX_PATH];
    TCHAR windowNameW[MAX_PATH];
    TCHAR *pWindowNameW;
    TCHAR *pClassNameW;
    
    HWND hwnd;
    
    if(IsBadReadPtr(lpClassName, 1))
    {
        pClassNameW = (WCHAR *) lpClassName;
    }
    else
    {
        a2w_buffer(lpClassName, -1, classNameW, MAX_PATH);
        pClassNameW = classNameW;
    }
    
    if(lpWindowName)
    {
        a2w_buffer(lpWindowName, -1, windowNameW, MAX_PATH);
        pWindowNameW = windowNameW;
    }
    else
    {
        pWindowNameW = NULL;
    }
    
    hwnd = CreateWindowExW(dwExStyle, 
                           pClassNameW, 
                           pWindowNameW, 
                           dwStyle,
                           x, y, 
                           nWidth, nHeight, 
                           hWndParent, 
                           hMenu,
                           hInstance, 
                           lpParam);
    
    return hwnd;
}


MOZCE_SHUNT_API ATOM mozce_RegisterClassA(CONST WNDCLASSA *lpwc)
{
    printf("mozce_RegisterClassA called (%s)\n", lpwc->lpszClassName);

    WNDCLASSW wcW;

    LPTSTR wClassName = a2w_malloc(lpwc->lpszClassName, -1, NULL);

    memcpy(&wcW, lpwc, sizeof(WNDCLASSA));    
    wcW.lpszClassName = wClassName;
    
    return RegisterClassW(&wcW);   
}

MOZCE_SHUNT_API UINT mozce_RegisterWindowMessageA(LPCSTR s)
{
    printf("mozce_RegisterWindowMessageA called\n");

    LPTSTR w = a2w_malloc(s, -1, NULL);
    UINT result = RegisterWindowMessageW(w);
    free(w);
    return result;
}


MOZCE_SHUNT_API BOOL mozce_GetClassInfoA(HINSTANCE hInstance, LPCSTR lpClassName, LPWNDCLASS lpWndClass)
{
    printf("mozce_GetClassInfoA called\n");

    LPTSTR wClassName = a2w_malloc(lpClassName, -1, NULL);
    BOOL result = GetClassInfoW(hInstance, wClassName, lpWndClass);
    free(wClassName);
    return result;
}


MOZCE_SHUNT_API HINSTANCE mozce_LoadLibraryA(LPCSTR lpLibFileName)
{
    printf("mozce_LoadLibraryA called (%s)\n", lpLibFileName);

    HINSTANCE retval = NULL;
    TCHAR wPath[MAX_PATH];

    if(0 != a2w_buffer(lpLibFileName, -1, wPath, charcount(wPath)))
    {
        retval = LoadLibraryW(wPath);
    }

    if (retval == NULL)
        printf("   FAILED!!\n");

    return retval;
}


MOZCE_SHUNT_API int mozce_GetObjectA(HGDIOBJ hgdiobj, int cbBuffer, LPVOID lpvObject)
{
    printf("mozce_GetObjectA called\n");

    if(cbBuffer == sizeof(LOGFONTA))
    {
        LOGFONTW lfw;
        LOGFONTA *plfa = (LOGFONTA *) lpvObject;
        int res;
        
        res = GetObjectW(hgdiobj, sizeof(LOGFONTW), &lfw);
        
        memcpy(plfa, &lfw, sizeof(LOGFONTA));
        
        w2a_buffer(lfw.lfFaceName, -1, plfa->lfFaceName, charcount(plfa->lfFaceName));
        return res;
    }
    return GetObjectW(hgdiobj, cbBuffer, lpvObject);
}

MOZCE_SHUNT_API FARPROC mozce_GetProcAddressA(HMODULE hMod, const char *name)
{
    printf("mozce_GetProcAddressA called (%s)\n", name);

    LPTSTR wName = a2w_malloc(name, -1, NULL);

    FARPROC p = GetProcAddressW(hMod, wName);
    free(wName);

    if (p == NULL)
        printf("  FAILED!!\n");

    return p;
}

MOZCE_SHUNT_API HCURSOR mozce_LoadCursorA(HINSTANCE hInstance, LPCSTR lpCursorName)
{
    printf("mozce_LoadCursorA called\n");

    wchar_t *lpCursorNameW;
    HCURSOR hc;
    
    if(lpCursorName > (LPCSTR) 0xFFFF)
    {
        int len = strlen(lpCursorName);
        
        lpCursorNameW = (unsigned short*)_alloca((len + 1) * 2);
        MultiByteToWideChar(CP_ACP, 0, lpCursorName, -1, lpCursorNameW, len + 1);
    }
    else
    {
        lpCursorNameW = (LPWSTR) (LPSTR) lpCursorName;
    }
    
    hc = LoadCursorW(hInstance, lpCursorNameW);
    
    return hc;
}


MOZCE_SHUNT_API int mozce_GetClassNameA(HWND hWnd, LPTSTR lpClassName, int nMaxCount)
{
    printf("mozce_GetClassNameA called\n");

    WCHAR classNameW[126];
    int res;
    
    if((res = GetClassNameW(hWnd, classNameW, sizeof(classNameW))) == 0)
        return res;
    
    w2a_buffer(classNameW, -1, (char*)lpClassName, nMaxCount);
    return res;
}



MOZCE_SHUNT_API BOOL mozce_GetOpenFileNameA(LPOPENFILENAMEA lpofna)
{
    printf("-- mozce_GetOpenFileNameA called\n");

    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return FALSE;
}

MOZCE_SHUNT_API BOOL mozce_GetSaveFileNameA(LPOPENFILENAMEA lpofna)
{
    printf("-- mozce_GetSaveFileNameA called\n");

    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return FALSE;
}


MOZCE_SHUNT_API HMODULE mozce_GetModuleHandleA(const char *lpName)
{
    printf("mozce_GetModuleHandleA called (%s)\n", lpName);

    if (lpName == NULL)
        return GetModuleHandleW(NULL);

    TCHAR wideStr[MAX_PATH];
    if(a2w_buffer(lpName, -1, wideStr, charcount(wideStr)))
        return GetModuleHandleW(wideStr);

    printf("  FAILED!!\n");
    return NULL;
}

MOZCE_SHUNT_API HICON mozce_LoadIconA(HINSTANCE hInstance, LPCSTR lpIconName)
{
    printf("mozce_LoadIconA called\n");

    HICON hi = NULL;

    TCHAR wideStr[MAX_PATH];
    if(a2w_buffer(lpIconName, -1, wideStr, charcount(wideStr)))
    {
        hi = LoadIconW(hInstance, wideStr);
    }
    return hi;
}

#define IS_INTRESOURCE(_r)   (((ULONG_PTR)(_r) >> 16) == 0)

MOZCE_SHUNT_API HRSRC mozce_FindResourceA(HMODULE  hModule, LPCSTR  lpName, LPCSTR  lpType)
{
    HRSRC hr;
    if (! IS_INTRESOURCE(lpName) && IS_INTRESOURCE(lpType))
    {
        LPTSTR wName = a2w_malloc(lpName, -1, NULL);
        LPTSTR wType = a2w_malloc(lpType, -1, NULL);
        
        hr = FindResourceW(hModule, wName, wType);
        free(wName);
        free(wType);
    }
    else
    {
        hr = FindResourceW(hModule, (const unsigned short*)lpName, (const unsigned short*)lpType);
    }

    if (hr == NULL)
        printf("hr == %d\n", GetLastError());
    return hr;
}

MOZCE_SHUNT_API UINT mozce_GetDlgItemTextA(HWND hDlg, int nIDDlgItem, LPSTR lpString, int nMaxCount)
{
    printf("mozce_GetDlgItemTextA called\n");
    
    LPWSTR lpStringW;
    UINT res;
    
    lpStringW = (unsigned short*)_alloca((nMaxCount + 1) * 2);
    
    res = GetDlgItemTextW(hDlg, nIDDlgItem, lpStringW, nMaxCount);
    
    if(res >= 0)
    {
        WideCharToMultiByte(CP_ACP, 0, lpStringW, -1, lpString, 
                            nMaxCount, NULL, NULL);
    }
    return res;
}

MOZCE_SHUNT_API HANDLE mozce_CreateEventA(LPSECURITY_ATTRIBUTES lpEventAttributes, BOOL bManualReset, BOOL bInitialState, const char *lpName)
{
    printf("mozce_CreateEventA called\n");
    
    HANDLE hEvent;
    wchar_t *lpNameNew = NULL;
    int len;
    
    if(lpName)
    {
        len = strlen(lpName) + 1;
        lpNameNew = (unsigned short*)malloc(len * 2);
        
        MultiByteToWideChar(CP_ACP, 0, lpName, -1, lpNameNew, len);
    }
    
    hEvent = CreateEventW(lpEventAttributes, bManualReset, bInitialState, lpNameNew);
    
    free(lpNameNew);
    
    return hEvent;
}

MOZCE_SHUNT_API HMENU mozce_LoadMenuA(HINSTANCE hInstance, LPCSTR lpMenuName)
{
    printf("mozce_LoadMenuA called\n");
    
    HMENU hr;
    if (! IS_INTRESOURCE(lpMenuName))
    {
        LPTSTR wName = a2w_malloc(lpMenuName, -1, NULL);
        hr = LoadMenuW(hInstance, wName);
        free(wName);
    }
    else
    {
        hr = LoadMenuW(hInstance, (const unsigned short*)lpMenuName);
    }
    
    return NULL;
}



MOZCE_SHUNT_API HANDLE mozce_GetPropA(HWND hWnd, LPCSTR lpString)
{
    printf("mozce_GetPropA called\n");
    
    HANDLE h = NULL;

    LPTSTR wString = a2w_malloc(lpString, -1, NULL);
    if (wString) {
        h = GetProp(hWnd, wString);
        free(wString);
    }
    return h;
}

MOZCE_SHUNT_API BOOL mozce_SetPropA(HWND hWnd, LPCSTR lpString, HANDLE hData)
{
    printf("mozce_SetPropA called\n");
    
    BOOL b = FALSE;

    LPTSTR wString = a2w_malloc(lpString, -1, NULL);
    if (wString) {
        b = SetProp(hWnd, wString, hData);
        free(wString);
    }
    return b;
}

MOZCE_SHUNT_API HANDLE mozce_RemovePropA(HWND hWnd, LPCSTR lpString)
{
    printf("mozce_RemovePropA called\n");
    
    HANDLE h = NULL;

    LPTSTR wString = a2w_malloc(lpString, -1, NULL);
    if (wString) {
        h = RemoveProp(hWnd, wString);
        free(wString);
    }
    return h;
}


MOZCE_SHUNT_API HANDLE mozce_FindFirstFileA(LPCSTR lpFileName, LPWIN32_FIND_DATAA lpFindFileData)
{
    printf("mozce_FindFirstFileA called\n");
    
    HANDLE h = NULL;
    if (!lpFindFileData || !lpFileName)
        return h;

    LPTSTR wString = a2w_malloc(lpFileName, -1, NULL);
    if (!wString) 
        return NULL;

    WIN32_FIND_DATAW findData;
    h = FindFirstFileW(wString, &findData);
    free(wString);
        
    if (!h || h == INVALID_HANDLE_VALUE)
        return NULL;

    lpFindFileData->dwFileAttributes   = findData.dwFileAttributes;
    lpFindFileData->dwReserved0        = findData.dwOID;
    lpFindFileData->ftCreationTime     = findData.ftCreationTime;
    lpFindFileData->ftLastAccessTime   = findData.ftLastAccessTime;
    lpFindFileData->ftLastWriteTime    = findData.ftLastWriteTime;
    lpFindFileData->nFileSizeHigh      = findData.nFileSizeHigh;
    lpFindFileData->nFileSizeLow       = findData.nFileSizeLow;    
    lpFindFileData->cAlternateFileName[0] = NULL;    
    
    if (!w2a_buffer(findData.cFileName, -1, lpFindFileData->cFileName, charcount(lpFindFileData->cFileName)))
        return NULL;
    
    return h;
}

MOZCE_SHUNT_API BOOL mozce_FindNextFileA(HANDLE hFindFile, LPWIN32_FIND_DATAA lpFindFileData)
{
    printf("mozce_FindNextFileA called\n");
    
    WIN32_FIND_DATAW findData;
    
    findData.dwFileAttributes  = lpFindFileData->dwFileAttributes;
    findData.dwOID             = lpFindFileData->dwReserved0;
    findData.ftCreationTime    = lpFindFileData->ftCreationTime;
    findData.ftLastAccessTime  = lpFindFileData->ftLastAccessTime;
    findData.ftLastWriteTime   = lpFindFileData->ftLastWriteTime;
    findData.nFileSizeHigh     = lpFindFileData->nFileSizeHigh;
    findData.nFileSizeLow      = lpFindFileData->nFileSizeLow;    

    
    if (FindNextFileW(hFindFile, &findData))
    {
        lpFindFileData->dwFileAttributes      = findData.dwFileAttributes;
        lpFindFileData->ftCreationTime        = findData.ftCreationTime;
        lpFindFileData->ftLastAccessTime      = findData.ftLastAccessTime;
        lpFindFileData->ftLastWriteTime       = findData.ftLastWriteTime;
        lpFindFileData->nFileSizeHigh         = findData.nFileSizeHigh;
        lpFindFileData->nFileSizeLow          = findData.nFileSizeLow;    
        lpFindFileData->dwReserved0           = findData.dwOID;
        lpFindFileData->cAlternateFileName[0] = NULL;

        w2a_buffer(findData.cFileName, -1, lpFindFileData->cFileName, MAX_PATH);

        return TRUE;
    }
    return FALSE;
}
 
MOZCE_SHUNT_API HANDLE mozce_CreateFileMappingA(HANDLE hFile, LPSECURITY_ATTRIBUTES lpFileMappingAttributes, DWORD flProtect, DWORD dwMaximumSizeHigh, DWORD dwMaximumSizeLow, LPCSTR lpName)
{
    printf("mozce_CreateFileMappingA called\n");
    
    HANDLE h = NULL;
    
    LPTSTR wString = a2w_malloc(lpName, -1, NULL);
    if (wString) {
        h = CreateFileMappingW(hFile, lpFileMappingAttributes, flProtect, dwMaximumSizeHigh, dwMaximumSizeLow, wString);
        free(wString);
    }
    return h;
}

MOZCE_SHUNT_API DWORD mozce_FormatMessageA(DWORD dwFlags, LPCVOID lpSource, DWORD dwMessageId, DWORD dwLanguageId, LPSTR lpBuffer, DWORD nSize, va_list* Arguments)
{
    printf("mozce_FormatMessageA called\n");
    
    DWORD d = -1;
    LPTSTR wString = a2w_malloc(lpBuffer, nSize, NULL);
    if (wString) {
        d = FormatMessageW(dwFlags, lpSource, dwMessageId, dwLanguageId, wString, nSize*2, Arguments); 
        free(wString);
    }
    return d;
}

MOZCE_SHUNT_API HANDLE mozce_CreateSemaphoreA(LPSECURITY_ATTRIBUTES lpSemaphoreAttributes, LONG lInitialCount, LONG lMaximumCount, LPCSTR lpName)
{
    printf("mozce_CreateSemaphoreA called\n");
    
    HANDLE h = NULL;
    
    if (!lpName)
        return CreateSemaphoreW(lpSemaphoreAttributes, lInitialCount, lMaximumCount, NULL);

    LPTSTR wString = a2w_malloc(lpName, -1, NULL);
    if (wString) {
        h = CreateSemaphoreW(lpSemaphoreAttributes, lInitialCount, lMaximumCount, wString);
        free(wString);
    }
    return h;
}

MOZCE_SHUNT_API int mozce_StartDocA(HDC hdc, CONST DOCINFO* lpdi)
{
    printf("-- mozce_StartDocA called\n");

    DWORD retval = GDI_ERROR;

    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);

    return retval;
}

MOZCE_SHUNT_API HFONT mozce_CreateFontIndirectA(CONST LOGFONTA* lplf)
{
    printf("mozce_CreateFontIndirectA called\n");
    
    LOGFONTW lfw;
    HFONT hFont;
    
    lfw.lfHeight = lplf->lfHeight;
    lfw.lfWidth = lplf->lfWidth;
    lfw.lfEscapement = lplf->lfEscapement;
    lfw.lfOrientation = lplf->lfOrientation;
    lfw.lfWeight = lplf->lfWeight;
    lfw.lfItalic = lplf->lfItalic;
    lfw.lfUnderline = lplf->lfUnderline;
    lfw.lfStrikeOut = lplf->lfStrikeOut;
    lfw.lfCharSet = lplf->lfCharSet;
    lfw.lfOutPrecision = lplf->lfOutPrecision;
    lfw.lfClipPrecision = lplf->lfClipPrecision;
    lfw.lfQuality = lplf->lfQuality;
    lfw.lfPitchAndFamily = lplf->lfPitchAndFamily;
    
    a2w_buffer(lplf->lfFaceName, -1, lfw.lfFaceName, charcount(lfw.lfFaceName));
    
    hFont = CreateFontIndirectW(&lfw);
    
    return hFont;
}


MOZCE_SHUNT_API int mozce_EnumFontFamiliesA(HDC hdc, LPCTSTR lpszFamily, FONTENUMPROC lpEnumFontFamProc, LPARAM lParam)
{
    printf("-- mozce_EnumFontFamilies called\n");
    return 0;
}


MOZCE_SHUNT_API int mozce_GetTextFaceA(HDC hdc, int nCount,  LPCSTR lpFaceName)
{
    printf("mozce_GetTextFace called\n");
    
    int res;
    
    LPWSTR lpFaceNameW = (LPWSTR) malloc((nCount + 1) * 2);
    
    res = GetTextFaceW(hdc, nCount, lpFaceNameW);
    
    if(!res && lpFaceName) {
        WideCharToMultiByte(CP_ACP, 0, lpFaceNameW, -1, (char*) lpFaceName, nCount, NULL, NULL);
    }

    return res;
}

MOZCE_SHUNT_API BOOL mozce_GetTextMetricsA(HDC hdc, LPTEXTMETRICA lptma)
{
    printf("mozce_GetTextMetrics called\n");

    TEXTMETRICW tmw;
    BOOL res;
    
    res = GetTextMetricsW(hdc, &tmw);
    
    lptma->tmHeight = tmw.tmHeight;
    lptma->tmAscent = tmw.tmAscent;
    lptma->tmDescent = tmw.tmDescent;
    lptma->tmInternalLeading = tmw.tmInternalLeading;
    lptma->tmExternalLeading = tmw.tmExternalLeading;
    lptma->tmAveCharWidth = tmw.tmAveCharWidth;
    lptma->tmMaxCharWidth = tmw.tmMaxCharWidth;
    lptma->tmWeight = tmw.tmWeight;
    lptma->tmOverhang = tmw.tmOverhang;
    lptma->tmDigitizedAspectX = tmw.tmDigitizedAspectX;
    lptma->tmDigitizedAspectY = tmw.tmDigitizedAspectY;
    lptma->tmFirstChar = (char)tmw.tmFirstChar;
    lptma->tmLastChar = (char)tmw.tmLastChar;
    lptma->tmDefaultChar = (char)tmw.tmDefaultChar;
    lptma->tmBreakChar = (char)tmw.tmBreakChar;
    lptma->tmItalic = tmw.tmItalic;
    lptma->tmUnderlined = tmw.tmUnderlined;
    lptma->tmStruckOut = tmw.tmStruckOut;
    lptma->tmPitchAndFamily = tmw.tmPitchAndFamily;
    lptma->tmCharSet = tmw.tmCharSet;

    return res;
}

#if 0
{
#endif
} /* extern "C" */
