/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
   Version 1.0 (the "License"); you may not use this file except in
   compliance with the License. You may obtain a copy of the License at
   http://www.mozilla.org/NPL/ 

   Software distributed under the License is distributed on an "AS IS" basis,
   WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
   for the specific language governing rights and limitations under the License. 

   The Original Code is Mozilla Communicator client code, released March 31, 1998. 

   The Initial Developer of the Original Code is Netscape Communications Corporation.
   Portions created by Netscape are Copyright (C) 1998 Netscape Communications Corporation.
   All Rights Reserved.
 */
#include <windows.h>
#include <cpl.h>

extern const char * const module_name;

#define MessageBox(a,b,c,d)

static HWND hwndControlPanel = NULL;
static HINSTANCE hinst = NULL;

LONG APIENTRY CPlApplet( HWND hwndCPl,      // handle to Control Panel window
                         UINT uMsg,	        // message
                         LONG lParam1,	    // first message parameter
                         LONG lParam2 ) {   // second message parameter
    LONG result = 0;

    NEWCPLINFO *pNewCPlInfo; 

    switch ( uMsg ) {
        case CPL_INIT:
            MessageBox( hwndControlPanel, "CPL_INIT", "Mozilla Control Panel", MB_OK );
            hinst = GetModuleHandle( module_name );
            result = 1;
            break;

        case CPL_GETCOUNT:         
            MessageBox( hwndControlPanel, "CPL_GETCOUNT", "Mozilla Control Panel", MB_OK );
            result = 1;
            break;

        case CPL_NEWINQUIRE:
            MessageBox( hwndControlPanel, "CPL_NEWINQUIRE", "Mozilla Control Panel", MB_OK );
            pNewCPlInfo = (NEWCPLINFO*) lParam2; 
 
            pNewCPlInfo->dwSize = (DWORD) sizeof(NEWCPLINFO); 
            pNewCPlInfo->dwFlags = 0; 
            pNewCPlInfo->dwHelpContext = 0; 
            pNewCPlInfo->lData = 0; 
            pNewCPlInfo->hIcon = LoadIcon( hinst, MAKEINTRESOURCE(101) ); 
            pNewCPlInfo->szHelpFile[0] = '\0'; 
 
            LoadString( hinst, 102, pNewCPlInfo->szName, sizeof pNewCPlInfo->szName ); 
            LoadString( hinst, 103, pNewCPlInfo->szInfo, sizeof pNewCPlInfo->szInfo ); 

            break;

        case CPL_SELECT:
            MessageBox( hwndControlPanel, "CPL_SELECT", "Mozilla Control Panel", MB_OK );
            break;

        case CPL_DBLCLK:
            MessageBox( hwndControlPanel, "CPL_DBLCLK", "Mozilla Control Panel", MB_OK );
            (MessageBoxA)( hwndControlPanel, "Mozilla preferences pane coming soon!", "Preferences", MB_OKCANCEL );
            //ShellExecute( NULL, "open", "e:\\x86Dbg\\mozilla.exe", "-prefs", "e:\\x86Dbg", SW_SHOW );
            break;

        case CPL_STOP:
            MessageBox( hwndControlPanel, "CPL_STOP", "Mozilla Control Panel", MB_OK );
            break;

        case CPL_EXIT:
            MessageBox( hwndControlPanel, "CPL_EXIT", "Mozilla Control Panel", MB_OK );
            break;

        default:
            MessageBox( hwndControlPanel, "CPL_?????", "Mozilla Control Panel", MB_OK );
            result = -1;
            break;
    }

    return result;
}
