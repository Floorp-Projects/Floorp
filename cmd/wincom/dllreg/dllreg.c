/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifdef WIN32
#include <objbase.h>
#else
#include <windows.h>
#include <compobj.h>
#include <stdlib.h>
#endif

/*  This is mainly sample source to show how to auto[un]register a DLL.
 *  This is equivalent to REGSVR32.EXE for 32 bits (VC4), and
 *      REGSVR.EXE for 16 bits (MSVC CDK16 only).
 *
 *  Also, we can ship this, I am not sure if we can redistribute those
 *      utilities.
 */

typedef HRESULT (*DllServerFunction)(void);

#ifdef WIN32
int main(int iArgc, char *paArgv[])
#else
int PASCAL WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
#endif
{
    BOOL 	bRegister = TRUE;
    BOOL 	bRetval = FALSE;
	char	szPath[_MAX_PATH];
    LPSTR	pLib = NULL;
    LPSTR	pOldLib = NULL;

#ifdef WIN32
    if(iArgc >= 2)  {
        int iTraverse = 1;
        while(iTraverse < iArgc)    {
            if(!stricmp(paArgv[iTraverse], "/u"))    {
                bRegister = FALSE;
            }
            else    {
                pLib = paArgv[iTraverse];
            }

            iTraverse++;
        }
    }
#else
    if(lpCmdLine && *lpCmdLine)  {
        char *pTraverse = lpCmdLine;
        while(*pTraverse)   {
            if(!strnicmp(pTraverse, "/u", 2))  {
                bRegister = FALSE;

                *pTraverse = ' ';
                pTraverse++;
                *pTraverse = ' ';
            }

            pTraverse++;
        }

        pTraverse = lpCmdLine;
        while(*pTraverse && isspace(*pTraverse))  {
            pTraverse++;
        }

        pLib = pTraverse;
    }
#endif

    CoInitialize(NULL);

    if(pLib)    {
        /*  Expand to the full path or LoadLibrary likes to fail
         *      if it is too convoluted/long/relative.
         */
        pOldLib = pLib;
        pLib = _fullpath(szPath, pLib, sizeof(szPath));
    }

    if(pLib)  {
        HINSTANCE hLibInstance = hLibInstance = LoadLibrary(pLib);

#ifdef WIN32
        if(hLibInstance)
#else
        if(hLibInstance > (HINSTANCE)HINSTANCE_ERROR)
#endif
        {
            DllServerFunction RegistryFunc = NULL;

            if(bRegister)  {
                (FARPROC)RegistryFunc = GetProcAddress(hLibInstance, "DllRegisterServer");
            }
            else    {
                (FARPROC)RegistryFunc = GetProcAddress(hLibInstance, "DllUnregisterServer");
            }

            if(RegistryFunc)   {
                HRESULT hResult = (RegistryFunc)();

                if(GetScode(hResult) == S_OK)   {
                    bRetval = TRUE;
                }

                RegistryFunc = NULL;
            }
            else    {
                /* If the DLL doesn't have those functions then it just doesn't support
                 * self-registration. We don't consider that to be an error
                 *
                 * We should consider checking for the "OleSelfRegister" string in the
                 * StringFileInfo section of the version information resource. If the DLL
                 * has "OleSelfRegister" but doesn't have the self-registration functions
                 * then that would be an error
                 */
                bRetval = TRUE;
            }

            FreeLibrary(hLibInstance);
            hLibInstance = NULL;
        }
    }

    CoUninitialize();

    if(bRetval == FALSE) {
        char *pMessage;

        if(bRegister == TRUE)   {
            pMessage = "Registration did not succeed.";
        }
        else    {
            pMessage = "Unregistration did not succeed.";
        }

        if(pLib == NULL)    {
            pLib = "Usage";
            pMessage = "dllreg [/u] filename.dll";
        }

#ifndef DEBUG_blythe
#ifdef WIN32
        printf("%s:\t%s\n", pLib, pMessage);
#else
        MessageBox(NULL, pMessage, pLib, MB_OK);
#endif
#endif
    }

    if(pLib)    {
        pLib = pOldLib;
    }

#ifdef WIN32
    return(bRetval == FALSE);
#else
    return(0);
#endif
}
