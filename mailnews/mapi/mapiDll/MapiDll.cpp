/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corp.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): Krishna Mohan Khandrika (kkhandrika@netscape.com)
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

#include <windows.h>
#include <tchar.h>
#include <assert.h>
#include <mapidefs.h>
#include "mapi.h"
#include "msgMapi.h"
#include "msgMapiMain.h"

nsIMapi *pNsMapi = NULL;
BOOL bUnInitialize = FALSE;

const CLSID CLSID_nsMapiImp = {0x29f458be, 0x8866, 0x11d5,
                              {0xa3, 0xdd, 0x0, 0xb0, 0xd0, 0xf3, 0xba, 0xa7}};
const IID IID_nsIMapi = {0x6EDCD38E,0x8861,0x11d5,
                        {0xA3,0xDD,0x00,0xB0,0xD0,0xF3,0xBA,0xA7}};

HANDLE hMutex;

BOOL StartMozilla()
{
    HRESULT hRes = 0;

    hRes = ::CoInitialize(NULL);
    bUnInitialize = (hRes == S_OK);

    hRes = ::CoCreateInstance(CLSID_nsMapiImp, NULL, CLSCTX_LOCAL_SERVER,
                              IID_nsIMapi, (LPVOID *)&pNsMapi);
    if (hRes != 0)
    {
        SetLastError(hRes);
        return FALSE;
    }

    hRes = pNsMapi->Initialize();
    if (hRes != 0)
    {
        pNsMapi->Release();
        pNsMapi = NULL;
        SetLastError(hRes);
        return FALSE;
    }

    return TRUE;
}

void ShutDownMozilla()
{
    WaitForSingleObject(hMutex, INFINITE);
    if (pNsMapi != NULL)
    {
        pNsMapi->CleanUp();
        pNsMapi->Release();
        pNsMapi = NULL;
    }
    ReleaseMutex(hMutex);
    if (bUnInitialize)
        ::CoUninitialize();
}

BOOL CheckMozilla()
{
    BOOL bRetValue = TRUE;

    WaitForSingleObject(hMutex, INFINITE);
    if (pNsMapi == NULL)
        bRetValue = StartMozilla();

    ReleaseMutex(hMutex);

    return bRetValue;
}

BOOL WINAPI DllMain(HINSTANCE aInstance, DWORD aReason, LPVOID aReserved)
{
    switch (aReason)
    {
        case DLL_PROCESS_ATTACH :
        {
            hMutex = CreateMutex(NULL, FALSE, NULL);
            break;
        }
        case DLL_PROCESS_DETACH :
        {
            ShutDownMozilla();
            ReleaseMutex(hMutex);
            CloseHandle(hMutex);
            break;
        }
    }

    return TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////
// The MAPILogon function begins a Simple MAPI session, loading the default message ////
// store and address book providers                            ////
////////////////////////////////////////////////////////////////////////////////////////

ULONG FAR PASCAL MAPILogon(ULONG aUIParam, LPTSTR aProfileName,
                            LPTSTR aPassword, FLAGS aFlags,
                            ULONG aReserved, LPLHANDLE aSession)
{
    HRESULT hr = 0;
    ULONG nSessionId = 0;

    if (CheckMozilla() == FALSE)
        return MAPI_E_FAILURE;

    if (!(aFlags & MAPI_UNICODE))
    {
        // Need to convert the parameters to Unicode.

        char *pUserName = (char *) aProfileName;
        char *pPassWord = (char *) aPassword;

        TCHAR ProfileName[MAX_NAME_LEN] = {0};
        TCHAR PassWord[MAX_PW_LEN] = {0};

        if (pUserName != NULL)
        {
            if (!MultiByteToWideChar(CP_ACP, 0, pUserName, -1, ProfileName,
                                                            MAX_NAME_LEN))
                return MAPI_E_FAILURE;
        }

        if (pPassWord != NULL)
        {
            if (!MultiByteToWideChar(CP_ACP, 0, pPassWord, -1, PassWord,
                                                            MAX_NAME_LEN))
                return MAPI_E_FAILURE;
        }

        hr = pNsMapi->Login(aUIParam, ProfileName, PassWord, aFlags,
                                                        &nSessionId);
    }
    else
        hr = pNsMapi->Login(aUIParam, aProfileName, aPassword,
                                                aFlags, &nSessionId);


    if (hr != 0)
    {
        return nSessionId;
    }

    *aSession = (LHANDLE) nSessionId;
    return SUCCESS_SUCCESS;
}

ULONG FAR PASCAL MAPILogoff(LHANDLE aSession, ULONG aUIParam,
                                            FLAGS aFlags, ULONG aReserved)
{
    HRESULT hr = 0;

    hr = pNsMapi->Logoff((ULONG) aSession);
    if (hr != 0)
    {
        return MAPI_E_INVALID_SESSION;
    }

    return SUCCESS_SUCCESS;
}



