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

/*
**  Help figure the character count of a TCHAR array.
*/
#define wcharcount(array) (sizeof(array) / sizeof(TCHAR))


MOZCE_SHUNT_API UINT mozce_GetWindowsDirectoryW(LPWSTR inBuffer, UINT inSize)
{
    printf("mozce_GetWindowsDirectoryW called\n");

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

    printf("GetWindowsDirectoryW\n");
    return retval;
}


MOZCE_SHUNT_API UINT mozce_GetSystemDirectoryW(LPWSTR inBuffer, UINT inSize)
{
    printf("mozce_GetSystemDirectoryW called\n");

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

    printf("GetSystemDirectoryW\n");
    return retval;
}


MOZCE_SHUNT_API HANDLE mozce_OpenSemaphoreW(DWORD inDesiredAccess, BOOL inInheritHandle, LPCWSTR inName)
{
    printf("mozce_OpenSemaphoreW called\n");

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

    printf("OpenSemaphoreW\n");
    return retval;
}


MOZCE_SHUNT_API DWORD mozce_GetGlyphOutlineW(HDC inDC, WCHAR inChar, UINT inFormat, void* inGM, DWORD inBufferSize, LPVOID outBuffer, CONST mozce_MAT2* inMAT2)
{
    printf("mozce_GetGlyphOutlineW called\n");

    DWORD retval = GDI_ERROR;

    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);

    printf("GetGlyphOutlineW\n");
    return retval;
}


MOZCE_SHUNT_API DWORD mozce_GetCurrentDirectoryW(DWORD inBufferLength, LPTSTR outBuffer)
{
    printf("mozce_GetCurrentDirectoryW called\n");

    DWORD retval = 0;

    if(NULL != outBuffer && 0 < inBufferLength)
    {
        outBuffer[0] = _T('\0');
    }

    SetLastError(ERROR_NOT_SUPPORTED);

    printf("GetCurrentDirectoryW\n");
    return retval;
}


MOZCE_SHUNT_API BOOL mozce_SetCurrentDirectoryW(LPCTSTR inPathName)
{
    printf("mozce_SetCurrentDirectoryW called\n");

    BOOL retval = FALSE;

    SetLastError(ERROR_NOT_SUPPORTED);

    printf("SetCurrentDirectoryW\n");
    return retval;
}


#if 0
{
#endif
} /* extern "C" */
