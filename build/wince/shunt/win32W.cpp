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

extern "C" {
#if 0
}
#endif

/*
**  Help figure the character count of a TCHAR array.
*/
#define wcharcount(array) (sizeof(array) / sizeof(TCHAR))


MOZCE_SHUNT_API UINT mozce_GetWindowsDirectoryW(LPWSTR inBuffer, UINT inSize)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("mozce_GetWindowsDirectoryW called\n");
#endif

    UINT retval = 0;

    if(inSize < 9)
    {
        retval = 9;
    }
    else
    {
        wcscpy(inBuffer, L"\\WINDOWS");
        retval = 8;
    }

    return retval;
}


MOZCE_SHUNT_API UINT mozce_GetSystemDirectoryW(LPWSTR inBuffer, UINT inSize)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("mozce_GetSystemDirectoryW called\n");
#endif

    UINT retval = 0;

    if(inSize < 9)
    {
        retval = 9;
    }
    else
    {
        wcscpy(inBuffer, L"\\WINDOWS");
        retval = 8;
    }

    return retval;
}


MOZCE_SHUNT_API HANDLE mozce_OpenSemaphoreW(DWORD inDesiredAccess, BOOL inInheritHandle, LPCWSTR inName)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("mozce_OpenSemaphoreW called\n");
#endif

    HANDLE retval = NULL;
    HANDLE semaphore = NULL;

    semaphore = CreateSemaphoreW(NULL, 0, 0x7fffffff, inName);
    if(NULL != semaphore)
    {
        DWORD lastErr = GetLastError();
        
        if(ERROR_ALREADY_EXISTS != lastErr)
        {
            CloseHandle(semaphore);
        }
        else
        {
            retval = semaphore;
        }
    }

    return retval;
}


MOZCE_SHUNT_API DWORD mozce_GetGlyphOutlineW(HDC inDC, WCHAR inChar, UINT inFormat, void* inGM, DWORD inBufferSize, LPVOID outBuffer, CONST VOID* inMAT2)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("mozce_GetGlyphOutlineW called\n");
#endif

    DWORD retval = GDI_ERROR;

    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);

    return retval;
}


MOZCE_SHUNT_API DWORD mozce_GetCurrentDirectoryW(DWORD inBufferLength, LPTSTR outBuffer)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("mozce_GetCurrentDirectoryW called\n");
#endif

    DWORD retval = 0;

    if(NULL != outBuffer && 0 < inBufferLength)
    {
        outBuffer[0] = _T('\0');
    }

    SetLastError(ERROR_NOT_SUPPORTED);

    return retval;
}


MOZCE_SHUNT_API BOOL mozce_SetCurrentDirectoryW(LPCTSTR inPathName)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("mozce_SetCurrentDirectoryW called\n");
#endif

    BOOL retval = FALSE;

    SetLastError(ERROR_NOT_SUPPORTED);

    return retval;
}


#if 0
{
#endif
} /* extern "C" */
