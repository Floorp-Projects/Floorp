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

static BOOL _doingRedraw = FALSE;

BOOL DoingRedraw() 
{
    return _doingRedraw;
}

void AddToHistory( char* str )
{
    int len = strlen(str);
    HWND list = GetDlgItem(g_hMainDlg, IDD_HISTORY);
    if(len && '\n' == str[len-1])
        str[len-1] = 0;
    SendMessage(list, LB_ADDSTRING, 0, (LPARAM) str );
    SendMessage(list, LB_SETCURSEL, 
                (WPARAM) SendMessage(list, LB_GETCOUNT, 0, 0) - 1, 0 );
    if(len && 0 == str[len-1])
        str[len-1] = '\n';
}

void ClearHistory()
{
    HWND list = GetDlgItem(g_hMainDlg, IDD_HISTORY);
    int i;

    for( i = SendMessage(list, LB_GETCOUNT, 0, 0); i >= 0; i-- )
        SendMessage(list, LB_DELETESTRING, i, 0 );
}    

void EvalAllOfHistory()
{
    HWND list = GetDlgItem(g_hMainDlg, IDD_HISTORY);
    int count = SendMessage(list, LB_GETCOUNT, 0, 0);
    int cursize;
    int size = 0;
    int index = 0;
    int i;
    char* buf;

    _doingRedraw = TRUE;

    for( i = 0; i < count; i++ )
    {
        cursize = SendMessage(list, LB_GETTEXTLEN, i, 0);
        if(-1 != cursize)
            size += cursize+1;
    }
    if( ! size)
    {
        _doingRedraw = FALSE;
        return;
    }

    buf = malloc(size+1);
    if(! buf)
    {
        _doingRedraw = FALSE;
        return;
    }

    for( i = 0; i < count; i++ )
    {
        cursize = SendMessage(list, LB_GETTEXT, i, (LPARAM)&buf[index]);
        if(-1 != cursize)
        {
            index += cursize+1;
            buf[index-1] = '\n';
        }
    }
    buf[index] = 0;
    if(index)
        Eval(buf,1);
    free(buf);
    _doingRedraw = FALSE;
}

BOOL 
SaveRecordedHistory(const char* filename)
{
    HWND list = GetDlgItem(g_hMainDlg, IDD_HISTORY);
    int count = SendMessage(list, LB_GETCOUNT, 0, 0);
    int size =  0;
    int start = 0;
    char* buf;
    int fh;
    int i;

    for( i = 0; i < count; i++ )
        size = max(SendMessage(list, LB_GETTEXTLEN, i, 0),size);

    if( ! size )
        return FALSE;

    buf = malloc(size+2);
    if(! buf)
        return FALSE;

    for( i = 0; i < count; i++ )
    {
        if(-1 != SendMessage(list, LB_GETTEXT, i, (LPARAM)buf))
        {
            if( strstr(buf, "start_record") )
                start = i+1;
        }
    }

    if( start == count )
    {
        free(buf);
        return FALSE;
    }

    fh = _open(filename, _O_CREAT | _O_TRUNC | _O_BINARY | _O_RDWR);
    if(-1 ==fh)
    {
        free(buf);
        return FALSE;
    }

    for( i = start; i < count; i++ )
    {
        if(-1 != SendMessage(list, LB_GETTEXT, i, (LPARAM)buf))
        {
            strcat(buf, "\n");
            _write(fh, buf, strlen(buf));
        }
    }

    _close(fh);
    free(buf);
    return TRUE;
}

static BOOL
EvalAndAddToHistory(char* expr, int* lineno)
{
    if( Eval(expr, *(lineno)+1) )
    {
        AddToHistory(expr);
        *(lineno)++;
        return TRUE;
    }
    else
    {
        EvalAllOfHistory();
        return FALSE;
    }
}

BOOL WINAPI 
MainDlgProc(HWND hDlg, UINT msg, UINT wParam, LONG lParam)
{
    static BOOL _drawing = FALSE;
    static RECT _rectSurface;
    static lineno = 0;

    switch(msg)
    {
        case WM_INITDIALOG:
        {
            RECT r;
            POINT pt;

            g_hMainDlg = hDlg;

            SetPenColor(RGB(255,0,0));
            SetPenWidth(2);

            SendMessage( GetDlgItem(hDlg, IDD_CONSOLE), WM_SETFONT, 
                         (WPARAM)GetStockObject(ANSI_FIXED_FONT),0);
           
            SendMessage( GetDlgItem(hDlg, IDD_HISTORY), WM_SETFONT, 
                         (WPARAM)GetStockObject(ANSI_FIXED_FONT),0);

            /* Calc offsets for our drawing surface */
            GetClientRect(hDlg, &r);
            pt.x = r.left;
            pt.y = r.top;
            ClientToScreen(hDlg, &pt);
            GetWindowRect(GetDlgItem(hDlg, IDD_SURFACE), &_rectSurface);

            SetSurfaceSize(_rectSurface.right - _rectSurface.left,
                           _rectSurface.bottom - _rectSurface.top);

            SetSurfaceOffset(_rectSurface.left - pt.x,
                             _rectSurface.top  - pt.y);

            OffsetRect(&_rectSurface, -pt.x, -pt.y);

            SetForegroundWindow(hDlg);

            return TRUE;
        }
        case WM_ERASEBKGND:
        {
            /* This is a hack to give us a white drawing surface */
            static HRGN hrgnSurface = 0;
            static HRGN hrgnTemp = 0;
            RECT r;

            if( 0 == hrgnSurface )
                hrgnSurface = CreateRectRgn(_rectSurface.left,
                                            _rectSurface.top,
                                            _rectSurface.right,
                                            _rectSurface.bottom);
            if( 0 == hrgnTemp )
                hrgnTemp = CreateRectRgn(0,0,1,1);

            GetClipRgn( (HDC)wParam, hrgnTemp );
            FillRect((HDC)wParam, &_rectSurface, GetStockObject(WHITE_BRUSH));
            ExtSelectClipRgn((HDC)wParam,hrgnSurface,RGN_XOR);
            GetClientRect(hDlg, &r);
            FillRect((HDC)wParam, &r, GetStockObject(LTGRAY_BRUSH));
            SelectClipRgn((HDC)wParam,hrgnTemp);

            return TRUE;
        }
        case WM_PAINT:
        {
            EvalAllOfHistory();
            return FALSE;
        }
        case WM_LBUTTONDOWN:
        {
            POINT pt = {LOWORD(lParam), HIWORD(lParam)};
            if( PtInRect(&_rectSurface, pt) )
            {
                char buf[32];
                RECT r;
                GetWindowRect(GetDlgItem(hDlg, IDD_SURFACE), &r);
                ClipCursor(&r);
                SetCapture(hDlg);
                _drawing = TRUE;
                DlgToSurface(&pt);
                sprintf( buf, "MoveTo(%d,%d)\n", pt.x, pt.y );
                EvalAndAddToHistory(buf,&lineno);
            }
            return TRUE;
        }

        case WM_MOUSEMOVE:
            if( _drawing )
            {
                POINT pt = {LOWORD(lParam), HIWORD(lParam)};
                char buf[32];
                DlgToSurface(&pt);
                sprintf( buf, "LineTo(%d,%d)\n", pt.x, pt.y );
                EvalAndAddToHistory(buf,&lineno);
            }
            return TRUE;

        case WM_LBUTTONUP:
            if( _drawing )
            {
                POINT pt = {LOWORD(lParam), HIWORD(lParam)};
                char buf[32];
                DlgToSurface(&pt);
                sprintf( buf, "LineTo(%d,%d)\n", pt.x, pt.y );
                EvalAndAddToHistory(buf,&lineno);
                ReleaseCapture();
                ClipCursor(NULL);
                _drawing = FALSE;
            }
            return TRUE;

        case WM_COMMAND:
            switch( LOWORD(wParam) )
            {
                case IDOK:
                case IDCANCEL:
                    EndDialog(hDlg, 0);
                    return TRUE;
                case IDD_CLEAR:
                    if( BN_CLICKED == HIWORD(wParam) )
                    {
                        ClearHistory();
                        lineno = 0;
                        InvalidateRect(hDlg, NULL, TRUE);
                    }
                    return TRUE;
                case IDD_EVAL:
                    if( BN_CLICKED == HIWORD(wParam) )
                    {
                        char* p;
                        int len;
                        HWND edit = GetDlgItem(hDlg, IDD_CONSOLE);
                        len = SendMessage(edit, WM_GETTEXTLENGTH, 0, 0 );
                        p = malloc(len+2);
                        SendMessage(edit, WM_GETTEXT, len+1, (LPARAM) p);
                        strcat(p,"\n");
                        if( EvalAndAddToHistory(p,&lineno) )
                            SendMessage(edit, WM_SETTEXT, 0, (LPARAM)"");
                        free(p);
                        SetFocus(edit);
                    }
                    return TRUE;
                case IDD_SET_COLOR:
                    if( BN_CLICKED == HIWORD(wParam) )
                    {
                        char Buf[256];
                        char Buf2[256];
                        Prompt( "Enter value or quoted color name", (char*)&Buf, 256 );
                        sprintf(Buf2, "setPenColor(%s);\n", Buf);
                        EvalAndAddToHistory(Buf2,&lineno);
                    }
                    return TRUE;
                case IDD_SET_WIDTH:
                    if( BN_CLICKED == HIWORD(wParam) )
                    {
                        char Buf[256];
                        char Buf2[256];
                        Prompt( "Enter New Width (1-100)", (char*)&Buf, 256 );
                        sprintf(Buf2, "setPenWidth(%s);\n", Buf);
                        EvalAndAddToHistory(Buf2,&lineno);
                    }
                    return TRUE;
                case IDD_RECORD:
                    if( BN_CLICKED == HIWORD(wParam) )
                    {
                        EvalAndAddToHistory("start_record()\n",&lineno);
                    }
                    return TRUE;
                case IDD_LOAD:
                    if( BN_CLICKED == HIWORD(wParam) )
                    {
                        char Buf[_MAX_PATH+1];
                        char Buf2[_MAX_PATH+32];
                        OPENFILENAME ofn;
                        
                        memset(&ofn,0,sizeof(ofn));
                        Buf[0] = 0;
                        
                        ofn.lStructSize = sizeof(ofn);
                        ofn.lpstrFilter = "JavaScript Source (*.js)\0*.JS\0All Files (*.*)\0*.*\0\0";
                        ofn.lpstrFile = Buf;
                        ofn.nMaxFile = _MAX_PATH;
                        ofn.lpstrTitle = "Load";
                        ofn.lpstrDefExt  = "JS";
                        ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
                        
                        if( GetOpenFileName(&ofn) )
                        {
                            // escape all backslashes
                            char c;
                            int i1 = 0;
                            int i2 = 0;
                            while( 0 != (c = Buf2[i2++] = Buf[i1++]) )
                            {
                                if( c == '\\' )
                                    Buf2[i2++] = '\\';
                            }

                            sprintf(Buf, "load(\"%s\");\n", Buf2);
                            EvalAndAddToHistory(Buf,&lineno);
                        }
                    }
                    return TRUE;
                case IDD_SAVE:
                    if( BN_CLICKED == HIWORD(wParam) )
                    {
                        char Buf[_MAX_PATH+1];
                        char Buf2[_MAX_PATH+32];
                        OPENFILENAME ofn;
                        
                        memset(&ofn,0,sizeof(ofn));
                        Buf[0] = 0;
                        
                        ofn.lStructSize = sizeof(ofn);
                        ofn.lpstrFilter = "JavaScript Source (*.js)\0*.JS\0All Files (*.*)\0*.*\0\0";
                        ofn.lpstrFile = Buf;
                        ofn.nMaxFile = _MAX_PATH;
                        ofn.lpstrTitle = "Save As";
                        ofn.lpstrDefExt  = "JS";
                        ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
                        
                        if( GetSaveFileName(&ofn) )
                        {
                            // escape all backslashes
                            char c;
                            int i1 = 0;
                            int i2 = 0;
                            while( 0 != (c = Buf2[i2++] = Buf[i1++]) )
                            {
                                if( c == '\\' )
                                    Buf2[i2++] = '\\';
                            }

                            sprintf(Buf, "save(\"%s\");\n", Buf2);
                            EvalAndAddToHistory(Buf,&lineno);
                        }
                    }
                    return TRUE;
                default:
                    break;
            }
        break;
     }
    return FALSE;
}
