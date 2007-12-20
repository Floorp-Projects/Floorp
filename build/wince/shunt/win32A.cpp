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
 * Jan 28, 2003.
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

#include <commdlg.h>

extern "C" {
#if 0
}
#endif

#define IS_INTRESOURCE(_r)   (((ULONG_PTR)(_r) >> 16) == 0)

/*
**  Help figure the character count of a TCHAR array.
*/

MOZCE_SHUNT_API DWORD GetModuleFileNameA(HMODULE hModule, LPSTR lpFilename, DWORD nSize)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("GetModuleFileNameA called\n");
#endif
    
    TCHAR wideStr[MAX_PATH];
    
    DWORD result = w2a_buffer(wideStr, 
                              GetModuleFileNameW(hModule, 
                                                 wideStr, 
                                                 charcount(wideStr)), 
                              lpFilename, nSize);
    lpFilename[result] = '\0';

    return result;
}

MOZCE_SHUNT_API BOOL CreateDirectoryA(LPCSTR lpPathName, LPSECURITY_ATTRIBUTES lpSecurityAttributes)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("CreateDirectoryA called (%s)\n", lpPathName);
#endif
    
    BOOL retval = FALSE;
    TCHAR wideStr[MAX_PATH];

    if(a2w_buffer(lpPathName, -1, wideStr, MAX_PATH))
    {
        retval = CreateDirectoryW(wideStr, lpSecurityAttributes);
    }
    
    return retval;
}

MOZCE_SHUNT_API BOOL RemoveDirectoryA(LPCSTR lpPathName)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("RemoveDirectoryA called\n");
#endif

    BOOL retval = FALSE;
    TCHAR wideStr[MAX_PATH];

    if(a2w_buffer(lpPathName, -1, wideStr, MAX_PATH))
    {
        retval = RemoveDirectoryW(wideStr);
    }
    return retval;
}

MOZCE_SHUNT_API BOOL DeleteFileA(LPCSTR lpFileName)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("mozce_DeleteFile called\n");
#endif

    BOOL retval = FALSE;
    TCHAR wideStr[MAX_PATH];

    if(a2w_buffer(lpFileName, -1, wideStr, MAX_PATH))
    {
        retval = DeleteFileW(wideStr);
    }
    return retval;
}

MOZCE_SHUNT_API BOOL MoveFileA(LPCSTR lpExistingFileName, LPCSTR lpNewFileName)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("MoveFileA called (%s)\n", lpExistingFileName);
#endif

    BOOL retval = FALSE;
    TCHAR wideStr[2][MAX_PATH];

    if(a2w_buffer(lpExistingFileName, -1, wideStr[0], MAX_PATH) &&
       a2w_buffer(lpNewFileName, -1, wideStr[1], MAX_PATH))
    {
        retval = MoveFileW(wideStr[0], wideStr[1]);
    }
    return retval;
}


MOZCE_SHUNT_API BOOL CopyFileA(LPCSTR lpExistingFileName, LPCSTR lpNewFileName, BOOL bFailIfExists)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("CopyFileA called\n");
#endif

    BOOL retval = FALSE;
    TCHAR wideStr[2][MAX_PATH];

    if(a2w_buffer(lpExistingFileName, -1, wideStr[0], MAX_PATH) &&
       a2w_buffer(lpNewFileName, -1, wideStr[1], MAX_PATH))
    {
        retval = CopyFileW(wideStr[0], wideStr[1], bFailIfExists);
    }
    return retval;
}


MOZCE_SHUNT_API HANDLE CreateFileA(LPCSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("CreateFileA called (%s)\n", lpFileName);
#endif

    HANDLE retval = INVALID_HANDLE_VALUE;
    TCHAR wideStr[MAX_PATH];

    if(a2w_buffer(lpFileName, -1, wideStr, MAX_PATH))
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


MOZCE_SHUNT_API DWORD GetFileAttributesA(LPCSTR lpFileName)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("GetFileAttributesA called\n");
#endif

    DWORD retval = (DWORD)-1;
    TCHAR wideStr[MAX_PATH];

    if(a2w_buffer(lpFileName, -1, wideStr, MAX_PATH))
    {
        retval = GetFileAttributesW(wideStr);
    }

    return retval;
}

MOZCE_SHUNT_API HANDLE CreateMutexA(LPSECURITY_ATTRIBUTES lpMutexAttributes, BOOL bInitialOwner, LPCSTR lpName)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("CreateMutexA called\n");
#endif
    
    if (!lpName)
        return CreateMutexW(lpMutexAttributes, bInitialOwner, NULL);
    
    LPTSTR widestr = a2w_malloc(lpName, -1, NULL);
    HANDLE h = CreateMutexW(lpMutexAttributes, bInitialOwner, widestr);
    free(widestr);
    return h;
}

MOZCE_SHUNT_API BOOL CreateProcessA(LPCSTR pszImageName, LPCSTR pszCmdLine, LPSECURITY_ATTRIBUTES psaProcess, LPSECURITY_ATTRIBUTES psaThread, BOOL fInheritHandles, DWORD fdwCreate, LPVOID pvEnvironment, LPSTR pszCurDir, LPSTARTUPINFO psiStartInfo, LPPROCESS_INFORMATION pProcInfo)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("CreateProcessA called\n");
#endif

	LPTSTR image   = a2w_malloc(pszImageName, -1, NULL);
	LPTSTR cmdline = a2w_malloc(pszCmdLine, -1, NULL);

	BOOL retval = CreateProcessW(image, cmdline, NULL, NULL, FALSE, fdwCreate, NULL, NULL, NULL, pProcInfo);

	if (image)
		free(image);
	
	if (cmdline)
		free(cmdline);

    return retval;
}


MOZCE_SHUNT_API int GetLocaleInfoA(LCID Locale, LCTYPE LCType, LPSTR lpLCData, int cchData)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("GetLocaleInfoA called\n");
#endif

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
                    retval = WideCharToMultiByte(CP_ACP,
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

MOZCE_SHUNT_API UINT GetWindowsDirectoryA(LPSTR inBuffer, UINT inSize)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("GetWindowsDirectoryA called\n");
#endif

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

    return retval;
}


MOZCE_SHUNT_API UINT GetSystemDirectoryA(LPSTR inBuffer, UINT inSize)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("GetSystemDirectoryA called\n");
#endif

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
    return retval;
}

MOZCE_SHUNT_API LONG RegOpenKeyExA(HKEY inKey, LPCSTR inSubKey, DWORD inOptions, REGSAM inSAM, PHKEY outResult)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("RegOpenKeyExA called\n");
#endif

    LONG retval = ERROR_GEN_FAILURE;

    LPTSTR wSubKey = a2w_malloc(inSubKey, -1, NULL);
    if(NULL != wSubKey)
    {
        retval = RegOpenKeyEx(inKey, wSubKey, inOptions, inSAM, outResult);
        free(wSubKey);
    }
    return retval;
}


MOZCE_SHUNT_API LONG RegQueryValueExA(HKEY inKey, LPCSTR inValueName, LPDWORD inReserved, LPDWORD outType, LPBYTE inoutBData, LPDWORD inoutDData)
{
    MOZCE_PRECHECK
        
#ifdef DEBUG
        mozce_printf("RegQueryValueExA called\n");
#endif
    
    LONG retval = ERROR_GEN_FAILURE;
    
    TCHAR wPath[MAX_PATH], tempData[MAX_PATH];
    DWORD tempSize = MAX_PATH;
    DWORD ourOutType;
    a2w_buffer(inValueName, -1, wPath, MAX_PATH);
    
	retval = RegQueryValueEx(inKey, inValueName ? wPath : NULL, inReserved, &ourOutType, (LPBYTE)tempData, &tempSize);
    
    if (ERROR_SUCCESS == retval)
    { 
        if(REG_EXPAND_SZ == ourOutType || REG_SZ == ourOutType)
        {
            w2a_buffer(tempData, tempSize, (LPSTR)inoutBData, *inoutDData);
        }
        else
        {
            memcpy(inoutBData, tempData, tempSize);
            *inoutDData = tempSize;
        }

        if (outType)
            *outType = ourOutType;
    }

    return retval;
}



MOZCE_SHUNT_API LONG RegSetValueExA(HKEY hKey, const char *valname, DWORD dwReserved, DWORD dwType, const BYTE* lpData, DWORD dwSize)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("RegSetValueExA called\n");
#endif

  unsigned short valnamew[256];
  LONG res;

  LPTSTR wName = NULL;
  MultiByteToWideChar(CP_ACP, 0, valname, -1, valnamew, charcount(valnamew));

  if(dwType == REG_SZ || dwType == REG_EXPAND_SZ)
      wName = a2w_malloc((char*)lpData, -1, NULL);
  else
      return -1;

  res = RegSetValueExW(hKey, valname ? valnamew : NULL, dwReserved, dwType, (const BYTE*)wName, (lstrlenW(wName) + 1)*sizeof(WCHAR));


  return res;

}

MOZCE_SHUNT_API LONG RegCreateKeyExA(HKEY hKey, const char *subkey, DWORD dwRes, LPSTR lpszClass, DWORD ulOptions, REGSAM samDesired, LPSECURITY_ATTRIBUTES sec_att, PHKEY phkResult, DWORD *lpdwDisp)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("RegCreateKeyExA called\n");
#endif

  LPTSTR wName = a2w_malloc(subkey, -1, NULL);

  long res = RegCreateKeyExW(hKey, wName, dwRes, NULL, ulOptions, samDesired, NULL, phkResult, lpdwDisp);

  free(wName);
  return res;
}


MOZCE_SHUNT_API LONG RegDeleteValueA(HKEY hKey, const char* lpValueName)
{
  MOZCE_PRECHECK
        
#ifdef DEBUG
  mozce_printf("RegDeleteValueA called\n");
#endif

  LPTSTR wName = a2w_malloc(lpValueName, -1, NULL);
  long res = RegDeleteKeyW(hKey, wName);
  
  free(wName);
  return res;
}



MOZCE_SHUNT_API int MessageBoxA(HWND inWnd, LPCSTR inText, LPCSTR inCaption, UINT uType)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("MessageBoxA called\n");
#endif

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


MOZCE_SHUNT_API HANDLE OpenSemaphoreA(DWORD inDesiredAccess, BOOL inInheritHandle, LPCSTR inName)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("OpenSemaphoreA called\n");
#endif

    HANDLE retval = NULL;
    LPTSTR wName = a2w_malloc(inName, -1, NULL);

    if(NULL != wName)
    {
        extern HANDLE OpenSemaphoreW(DWORD inDesiredAccess, BOOL inInheritHandle, LPCWSTR inName);
        retval = OpenSemaphoreW(inDesiredAccess, inDesiredAccess, wName);
        free(wName);
    }

    return retval;
}


MOZCE_SHUNT_API HDC CreateDCA(LPCSTR inDriver, LPCSTR inDevice, LPCSTR inOutput, CONST DEVMODEA* inInitData)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("CreateDCA called\n");
#endif

    HDC retval = NULL;

    LPTSTR wDriver = a2w_malloc(inDriver, -1, NULL);
    LPTSTR wDevice = a2w_malloc(inDevice, -1, NULL);
    LPTSTR wOutput = a2w_malloc(inOutput, -1, NULL);

    DEVMODE wInitData;
    if (inInitData)
    {
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
    }
    retval = CreateDC(wDriver, wDevice, wOutput, inInitData ? &wInitData : NULL);
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

    return retval;
}


MOZCE_SHUNT_API HDC CreateDCA2(LPCSTR inDriver, LPCSTR inDevice, LPCSTR inOutput, CONST DEVMODE* inInitData)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("CreateDCA2 called\n");
#endif

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

    return retval;
}

MOZCE_SHUNT_API BOOL GetTextExtentExPointA(HDC inDC, const char * inStr, int inLen, int inMaxExtent, LPINT outFit, LPINT outDx, LPSIZE inSize)
{
    MOZCE_PRECHECK

#ifdef DEBUG
  mozce_printf("GetTextExtentExPointA called (%d)\n", inLen);
#endif

    BOOL retval = FALSE;

    // BUG:  if inString has any embedded nulls, this function will not produce the desired effect!
	LPTSTR wStr = a2w_malloc(inStr, inLen, NULL);
	
    if(NULL != wStr)
    {
        retval = GetTextExtentExPointW(inDC, wStr, inLen, inMaxExtent, outFit, outDx, inSize);
        free(wStr);
    }

    return retval;
}


MOZCE_SHUNT_API BOOL TextOutA(HDC hdc, int nXStart, int nYStart, const char* lpString, int cbString)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("TextOutA (%s) called\n", lpString);
#endif
    
    LPTSTR aStr = a2w_malloc(lpString, -1, NULL);
    BOOL retval = ExtTextOutW(hdc, nXStart, nYStart, 0, NULL, aStr, cbString, NULL);
    free(aStr);
    return retval;
}

MOZCE_SHUNT_API DWORD GetGlyphOutlineA(HDC inDC, CHAR inChar, UINT inFormat, void* inGM, DWORD inBufferSize, LPVOID outBuffer, CONST MAT2* inMAT2)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("-- GetGlyphOutlineA called\n");
#endif

    DWORD retval = GDI_ERROR;

    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);

    return retval;
}

MOZCE_SHUNT_API DWORD GetCurrentDirectoryA(DWORD inBufferLength, LPSTR outBuffer)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("-- GetCurrentDirectoryA called\n");
#endif

    DWORD retval = 0;

    if(NULL != outBuffer && 0 < inBufferLength)
    {
        outBuffer[0] = '\0';
    }

    SetLastError(ERROR_NOT_SUPPORTED);
    return retval;
}


MOZCE_SHUNT_API BOOL SetCurrentDirectoryA(LPCSTR inPathName)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("-- SetCurrentDirectoryA called\n");
#endif

    BOOL retval = FALSE;

    SetLastError(ERROR_NOT_SUPPORTED);
    return retval;
}

MOZCE_SHUNT_API LONG RegEnumKeyExA(HKEY inKey, DWORD inIndex, LPSTR outName, LPDWORD inoutName, LPDWORD inReserved, LPSTR outClass, LPDWORD inoutClass, PFILETIME inLastWriteTime)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("RegEnumKeyExA called\n");
#endif

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


MOZCE_SHUNT_API BOOL VerQueryValueA(const LPVOID inBlock, LPSTR inSubBlock, LPVOID *outBuffer, PUINT outLen)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("VerQueryValueA called.  Incomplete implementation.  Your code will have to manually convert strings.\n");
#endif

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

MOZCE_SHUNT_API int LoadStringA(HINSTANCE inInstance, UINT inID, LPSTR outBuffer, int inBufferMax)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("LoadStringA called\n");
#endif

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


MOZCE_SHUNT_API VOID OutputDebugStringA(LPCSTR inOutputString)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("OutputDebugStringA called\n");
#endif

    LPTSTR wideStr = NULL;

    wideStr = a2w_malloc(inOutputString, -1, NULL);
    if(NULL != wideStr)
    {
        OutputDebugString(wideStr);
        free(wideStr);
    }
}


MOZCE_SHUNT_API int DrawTextA(HDC inDC, LPCSTR inString, int inCount, LPRECT inRect, UINT inFormat)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("DrawTextA called\n");
#endif

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


MOZCE_SHUNT_API BOOL SetDlgItemTextA(HWND inDlg, int inIDDlgItem, LPCSTR inString)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("SetDlgItemTextA called\n");
#endif

    BOOL retval = FALSE;
    LPTSTR wString = a2w_malloc(inString, -1, NULL);
    if(NULL != wString)
    {
        retval = SetDlgItemText(inDlg, inIDDlgItem, wString);
        free(wString);
    }

    return retval;
}


MOZCE_SHUNT_API HANDLE LoadImageA(HINSTANCE inInst, LPCSTR inName, UINT inType, int inCX, int inCY, UINT inLoad)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("LoadImageA called\n");
#endif

    HANDLE retval = NULL;

    LPTSTR wName = a2w_malloc(inName, -1, NULL);
    if(NULL != wName)
    {
        retval = LoadImage(inInst, wName, inType, inCX, inCY, inLoad);
        free(wName);
    }

    return retval;
}


MOZCE_SHUNT_API HWND FindWindowA(LPCSTR inClass, LPCSTR inWindow)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("FindWindowA called\n");
#endif

    HWND retval = NULL;

    LPTSTR wClass = a2w_malloc(inClass, -1, NULL);
    LPTSTR wWindow = a2w_malloc(inWindow, -1, NULL); 
   
    retval = FindWindow(wClass, NULL);
    
    if (wWindow)
        free(wWindow);

    if (wClass)
        free(wClass);

    return retval;
}


MOZCE_SHUNT_API UINT RegisterClipboardFormatA(LPCSTR inFormat)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("RegisterClipboardFormatA called\n");
#endif

    UINT retval = 0;

    LPTSTR wFormat = a2w_malloc(inFormat, -1, NULL);
    if(NULL != wFormat)
    {
        retval = RegisterClipboardFormat(wFormat);
        free(wFormat);
    }

    return retval;
}


MOZCE_SHUNT_API DWORD GetEnvironmentVariableA(LPSTR lpName, LPSTR lpBuffer, DWORD nSize)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("-- GetEnvironmentVariableA called\n");
#endif

    DWORD retval = -1;

    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);

    return retval; 
}

MOZCE_SHUNT_API DWORD SetEnvironmentVariableA(LPSTR lpName, LPSTR lpBuffer)
{    
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("-- SetEnvironmentVariableA called\n");
#endif

    DWORD retval = -1;

    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);

    return retval; 
}


MOZCE_SHUNT_API LRESULT SendMessageA(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("SendMessageA called\n");
#endif

    unsigned short *lpNewText = NULL;
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
#ifdef DEBUG
        mozce_printf("  need possible conversion %d\n", msg);
#endif
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
#ifdef DEBUG
        mozce_printf("  need possible out conversion %d\n", msg);        
#endif
    }
#endif

    if(lpNewText)
        free(lpNewText);

  return res;
}


MOZCE_SHUNT_API ATOM GlobalAddAtomA(LPCSTR lpString)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("GlobalAddAtomA called (%s)\n", lpString);
#endif

    LPTSTR watom = a2w_malloc(lpString, -1, NULL);
    if (!watom)
        return NULL;

    ATOM a = GlobalAddAtomW(watom);

    free(watom);
    return a; 
}

MOZCE_SHUNT_API BOOL GetVersionExA(LPOSVERSIONINFOA lpv)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("GetVersionExA called\n");
#endif

    OSVERSIONINFOW vw;
    vw.dwOSVersionInfoSize = sizeof(vw);

    GetVersionExW(&vw);

    memcpy(lpv, &vw, 5 * sizeof(DWORD));

    w2a_buffer(vw.szCSDVersion, -1, lpv->szCSDVersion, sizeof(lpv->szCSDVersion));    
    return TRUE;
}


MOZCE_SHUNT_API LRESULT DefWindowProcA(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("DefWindowProcA called\n");
#endif

    return DefWindowProcW(hWnd, Msg, wParam, lParam);
}


MOZCE_SHUNT_API HWND CreateWindowExA(DWORD dwExStyle, 
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


MOZCE_SHUNT_API ATOM RegisterClassA(CONST WNDCLASSA *lpwc)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("RegisterClassA called (%s)\n", lpwc->lpszClassName);
#endif

    WNDCLASSW wcW;

    LPTSTR wClassName = a2w_malloc(lpwc->lpszClassName, -1, NULL);

    memcpy(&wcW, lpwc, sizeof(WNDCLASSA));    

    wcW.lpszMenuName  = NULL;
    wcW.lpszClassName = wClassName;
    
    return RegisterClassW(&wcW);   
}

MOZCE_SHUNT_API BOOL UnregisterClassA(LPCSTR lpClassName, HINSTANCE hInstance)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("UnregisterClassA called\n");
#endif
    LPTSTR w = a2w_malloc(lpClassName, -1, NULL);
    BOOL result = UnregisterClassW(w, hInstance);
    free(w);
    return result;
}
MOZCE_SHUNT_API UINT RegisterWindowMessageA(LPCSTR s)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("RegisterWindowMessageA called\n");
#endif

    LPTSTR w = a2w_malloc(s, -1, NULL);
    UINT result = RegisterWindowMessageW(w);
    free(w);
    return result;
}

MOZCE_SHUNT_API HINSTANCE LoadLibraryA(LPCSTR lpLibFileName)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("LoadLibraryA called (%s)\n", lpLibFileName);
#endif

    HINSTANCE retval = NULL;

    LPTSTR wPath = a2w_malloc(lpLibFileName, -1, NULL);

    if(wPath) {
        retval = LoadLibraryW(wPath);
    }
    
#ifdef DEBUG
    if (!retval) {
        DWORD error = GetLastError();
        mozce_printf("LoadLibraryA failure (14==OOM)! %d\n", error);
        
        if (error == 14)
            MessageBoxW(NULL, L"Failed to Load Library. Out Of Memory.", wPath, 0);
    }
#endif

	if (wPath)
		free(wPath);
    return retval;
}


MOZCE_SHUNT_API int GetObjectA(HGDIOBJ hgdiobj, int cbBuffer, LPVOID lpvObject)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("GetObjectA called\n");
#endif

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


MOZCE_SHUNT_API HBITMAP LoadBitmapA(HINSTANCE hInstance, LPCSTR lpCursorName)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("LoadBitmapA called\n");
#endif

    unsigned short *bitmapNameW;
    HBITMAP hc;
    
    if(lpCursorName > (LPCSTR) 0xFFFF)
    {
        int len = strlen(lpCursorName);
        
        bitmapNameW = (unsigned short*)_alloca((len + 1) * 2);
        MultiByteToWideChar(CP_ACP, 0, lpCursorName, -1, bitmapNameW, len + 1);
    }
    else
    {
        bitmapNameW = (LPWSTR) (LPSTR) lpCursorName;
    }
    
    hc = LoadBitmapW(hInstance, bitmapNameW);
    
    return hc;
}

MOZCE_SHUNT_API HCURSOR LoadCursorA(HINSTANCE hInstance, LPCSTR lpCursorName)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("-- LoadCursorA called\n");
#endif

	return NULL;

    unsigned short *lpCursorNameW;
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



MOZCE_SHUNT_API BOOL GetOpenFileNameA(LPOPENFILENAMEA lpofna)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("-- GetOpenFileNameA called\n");
#endif

    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return FALSE;
}

MOZCE_SHUNT_API BOOL GetSaveFileNameA(LPOPENFILENAMEA lpofna)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("-- GetSaveFileNameA called\n");
#endif

    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return FALSE;
}


MOZCE_SHUNT_API HMODULE GetModuleHandleA(const char *lpName)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("GetModuleHandleA called (%s)\n", lpName);
#endif

    if (lpName == NULL)
        return GetModuleHandleW(NULL);

    TCHAR wideStr[MAX_PATH];
    if(a2w_buffer(lpName, -1, wideStr, MAX_PATH))
        return GetModuleHandleW(wideStr);

    return NULL;
}

MOZCE_SHUNT_API HICON LoadIconA(HINSTANCE hInstance, LPCSTR lpIconName)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("LoadIconA called\n");
#endif

    HICON hi = NULL;

    if (! IS_INTRESOURCE(lpIconName))
    {

        TCHAR wideStr[MAX_PATH];
        if(a2w_buffer(lpIconName, -1, wideStr, MAX_PATH))
        {
            hi = LoadIconW(hInstance, wideStr);
        }
    }
    else
        hi = LoadIconW(hInstance, (const unsigned short*) lpIconName);

    return hi;
}

MOZCE_SHUNT_API HRSRC FindResourceA(HMODULE  hModule, LPCSTR  lpName, LPCSTR  lpType)
{
    MOZCE_PRECHECK

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

#ifdef DEBUG
    if (hr == NULL)
        mozce_printf("hr == %d\n", GetLastError());
#endif
    return hr;
}

MOZCE_SHUNT_API UINT GetDlgItemTextA(HWND hDlg, int nIDDlgItem, LPSTR lpString, int nMaxCount)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("GetDlgItemTextA called\n");
#endif
    
    UINT res;
    
    LPWSTR stringW = (unsigned short*) malloc ((nMaxCount + 1) * 2);
    
    res = GetDlgItemTextW(hDlg, nIDDlgItem, stringW, nMaxCount);
    
    if(res >= 0)
        w2a_buffer(stringW, -1, lpString, nMaxCount);

    free(stringW);
    return res;
}

MOZCE_SHUNT_API HANDLE CreateEventA(LPSECURITY_ATTRIBUTES lpEventAttributes, BOOL bManualReset, BOOL bInitialState, const char *lpName)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("CreateEventA called\n");
#endif
    
    HANDLE hEvent;
    unsigned short *lpNameNew = NULL;
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

MOZCE_SHUNT_API HMENU LoadMenuA(HINSTANCE hInstance, LPCSTR lpMenuName)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("LoadMenuA called\n");
#endif
    
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


#ifdef GetProp
#undef GetProp
#endif
#define GetProp GetProp

MOZCE_SHUNT_API HANDLE GetPropA(HWND hWnd, const char* lpString)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("GetPropA called\n");
#endif
    
    HANDLE h = NULL;

    if (IS_INTRESOURCE(lpString))
        return GetProp(hWnd, (const unsigned short *)lpString);

    LPTSTR wString = a2w_malloc(lpString, -1, NULL);
    if (wString) {
        h = GetProp(hWnd, wString);
        free(wString);
    }
    return h;
}

#ifdef SetProp
#undef SetProp
#endif
#define SetProp SetProp

MOZCE_SHUNT_API BOOL SetPropA(HWND hWnd, const char* lpString, HANDLE hData)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("SetPropA called\n");
#endif
    
    BOOL b = FALSE;
    if (!lpString)
        return b;

    if (IS_INTRESOURCE(lpString))
        return SetProp(hWnd, (const unsigned short *)lpString, hData);

    LPTSTR wString = a2w_malloc(lpString, -1, NULL);
    if (wString) {
        b = SetProp(hWnd, wString, hData);
        free(wString);
    }
    return b;
}
#ifdef RemoveProp
#undef RemoveProp
#endif
#define RemoveProp RemoveProp

MOZCE_SHUNT_API HANDLE RemovePropA(HWND hWnd, const char* lpString)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("RemovePropA called\n");
#endif
    
    HANDLE h = NULL;

    if (IS_INTRESOURCE(lpString))
        return RemoveProp(hWnd, (const unsigned short *)lpString);

    unsigned short* wString = a2w_malloc(lpString, -1, NULL);
    if (wString) {
        h = RemoveProp(hWnd, wString);
        free(wString);
    }
    return h;
}

MOZCE_SHUNT_API HANDLE FindFirstFileA(const char* lpFileName, LPWIN32_FIND_DATAA lpFindFileData)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("FindFirstFileA called\n");
#endif
    
    HANDLE h = NULL;
    if (!lpFindFileData || !lpFileName)
        return h;

    LPTSTR wString = a2w_malloc(lpFileName, -1, NULL);
    if (!wString) 
        return INVALID_HANDLE_VALUE;

    WIN32_FIND_DATAW findData;
    h = FindFirstFileW(wString, &findData);
    free(wString);
        
    if (!h || h == INVALID_HANDLE_VALUE)
        return INVALID_HANDLE_VALUE;

    lpFindFileData->dwFileAttributes   = findData.dwFileAttributes;
    lpFindFileData->dwReserved0        = findData.dwOID;
    lpFindFileData->ftCreationTime     = findData.ftCreationTime;
    lpFindFileData->ftLastAccessTime   = findData.ftLastAccessTime;
    lpFindFileData->ftLastWriteTime    = findData.ftLastWriteTime;
    lpFindFileData->nFileSizeHigh      = findData.nFileSizeHigh;
    lpFindFileData->nFileSizeLow       = findData.nFileSizeLow;    
    lpFindFileData->cAlternateFileName[0] = NULL;    
    
    if (!w2a_buffer(findData.cFileName, -1, lpFindFileData->cFileName, charcount(lpFindFileData->cFileName)))
        return INVALID_HANDLE_VALUE;
    
    return h;
}

MOZCE_SHUNT_API BOOL FindNextFileA(HANDLE hFindFile, WIN32_FIND_DATAA* lpFindFileData)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("FindNextFileA called\n");
#endif
    
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
 
MOZCE_SHUNT_API HANDLE CreateFileMappingA(HANDLE hFile, LPSECURITY_ATTRIBUTES lpFileMappingAttributes, DWORD flProtect, DWORD dwMaximumSizeHigh, DWORD dwMaximumSizeLow, LPCSTR lpName)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("CreateFileMappingA called\n");
#endif
    
    HANDLE h = NULL;
    
    LPTSTR wString = a2w_malloc(lpName, -1, NULL);
    if (wString) {
        h = CreateFileMappingW(hFile, lpFileMappingAttributes, flProtect, dwMaximumSizeHigh, dwMaximumSizeLow, wString);
        free(wString);
    }
    return h;
}

MOZCE_SHUNT_API DWORD FormatMessageA(DWORD dwFlags, LPCVOID lpSource, DWORD dwMessageId, DWORD dwLanguageId, LPSTR lpBuffer, DWORD nSize, va_list* Arguments)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("FormatMessageA called\n");
#endif
    
    DWORD d = -1;
    LPTSTR wString = a2w_malloc(lpBuffer, nSize, NULL);
    if (wString) {
        d = FormatMessageW(dwFlags, lpSource, dwMessageId, dwLanguageId, wString, nSize*2, Arguments); 
        free(wString);
    }
    return d;
}

MOZCE_SHUNT_API HANDLE CreateSemaphoreA(LPSECURITY_ATTRIBUTES lpSemaphoreAttributes, LONG lInitialCount, LONG lMaximumCount, LPCSTR lpName)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("CreateSemaphoreA called\n");
#endif
    
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

MOZCE_SHUNT_API HFONT CreateFontIndirectA(CONST LOGFONTA* lplf)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("CreateFontIndirectA called\n");
#endif
    
    LOGFONTW lfw;
    HFONT hFont;
    
    lfw.lfHeight         = lplf->lfHeight;
    lfw.lfWidth          = lplf->lfWidth;
    lfw.lfEscapement     = lplf->lfEscapement;
    lfw.lfOrientation    = lplf->lfOrientation;
    lfw.lfWeight         = lplf->lfWeight;
    lfw.lfItalic         = lplf->lfItalic;
    lfw.lfUnderline      = lplf->lfUnderline;
    lfw.lfStrikeOut      = lplf->lfStrikeOut;
    lfw.lfCharSet        = lplf->lfCharSet;
    lfw.lfOutPrecision   = lplf->lfOutPrecision;
    lfw.lfClipPrecision  = lplf->lfClipPrecision;
    lfw.lfQuality        = lplf->lfQuality;
    lfw.lfPitchAndFamily = lplf->lfPitchAndFamily;
    
    a2w_buffer(lplf->lfFaceName, -1, lfw.lfFaceName, LF_FACESIZE);
    hFont = CreateFontIndirectW(&lfw);
    
#ifdef DEBUG
    mozce_printf("CreateFontIndirectW %x\n", hFont);
#endif
    return hFont;
}



typedef struct _MyEnumFontFamArg
{
  FONTENUMPROC fn;
  LPARAM lParam;
} MYENUMFONTFAMARG;



// typedef int (CALLBACK* FONTENUMPROC)(CONST LOGFONT *, CONST TEXTMETRIC *, DWORD, LPARAM);

static int CALLBACK
MyEnumFontFamProc(CONST LOGFONT *lf, CONST TEXTMETRIC *tm, DWORD fonttype, LPARAM lParam)
{
    MYENUMFONTFAMARG *parg = (MYENUMFONTFAMARG *) lParam;
    FONTENUMPROC fn = parg->fn;

    LOGFONTW lfw;
    memcpy(&lfw, lf, sizeof(LOGFONTA));    
    a2w_buffer((const char*)lf->lfFaceName, -1, lfw.lfFaceName, LF_FACESIZE);

    return (*fn) (&lfw, tm, fonttype, parg->lParam);
}

MOZCE_SHUNT_API BOOL GetTextMetricsA(HDC hdc, TEXTMETRICA*  lptma)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("GetTextMetricsA called\n");
#endif

    if (!lptma)
        return 0;

    TEXTMETRICW tmw;
    BOOL res;
    
    res = GetTextMetricsW(hdc, &tmw);
    
    if (res==0)
        return res;
    
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
    lptma->tmItalic = tmw.tmItalic;
    lptma->tmUnderlined = tmw.tmUnderlined;
    lptma->tmStruckOut = tmw.tmStruckOut;
    lptma->tmPitchAndFamily = tmw.tmPitchAndFamily;
    lptma->tmCharSet = tmw.tmCharSet;

    w2a_buffer(&tmw.tmFirstChar, 1, &lptma->tmFirstChar, 1);
    w2a_buffer(&tmw.tmDefaultChar, 1, &lptma->tmDefaultChar, 1);
    w2a_buffer(&tmw.tmBreakChar, 1, &lptma->tmBreakChar, 1);

    return res;
}

MOZCE_SHUNT_API BOOL SetWindowTextA(HWND hWnd, LPCSTR lpString)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("SetWindowTextA called\n");
#endif

    LPTSTR wstr = a2w_malloc(lpString, -1, NULL);
    BOOL result = SetWindowTextW(hWnd, wstr); 
    
    if (wstr)
        free(wstr);

    return result;
}


#if 0
{
#endif
} /* extern "C" */
