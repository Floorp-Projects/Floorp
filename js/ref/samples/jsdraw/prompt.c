/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

/* jband - 11/08/97 -  */

#include "headers.h"

typedef struct
{
    char* prompt;
    char* answer;
    int   maxAnswer;
} PROMPTDATA;

BOOL WINAPI 
PromptDlgProc(HWND hDlg, UINT msg, UINT wParam, LONG lParam)
{
    static PROMPTDATA* pd;
    switch(msg)
    {
        case WM_INITDIALOG:
        {
            pd = (PROMPTDATA*) lParam;
            SendMessage(GetDlgItem(hDlg, IDD_PROMPT_PROMPT), 
                        WM_SETTEXT, 0, (LPARAM) pd->prompt );
            SendMessage(GetDlgItem(hDlg, IDD_PROMPT_ANSWER), 
                        EM_LIMITTEXT, pd->maxAnswer, 0 );
            return TRUE;
        }

        case WM_COMMAND:
            switch( LOWORD(wParam) )
            {
                case IDOK:
                case IDCANCEL:
                    SendMessage(GetDlgItem(hDlg, IDD_PROMPT_ANSWER), 
                                WM_GETTEXT, pd->maxAnswer, 
                                (LPARAM) pd->answer );
                    EndDialog(hDlg, 0);
                    return TRUE;
                default:
                    break;
            }
        break;
     }
    return FALSE;
}

void Prompt( char* prompt, char* answer, int maxAnswer )
{
    PROMPTDATA pd;
    pd.prompt    = prompt;
    pd.answer    = answer;
    pd.maxAnswer = maxAnswer;

    DialogBoxParam(g_hInstance, MAKEINTRESOURCE(IDDIALOG_PROMPT), 
                   NULL, PromptDlgProc, (LPARAM) &pd);
}    

