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


/* Globals */

extern HINSTANCE g_hInstance;
extern HWND      g_hMainDlg;

BOOL WINAPI MainDlgProc(HWND hDlg, UINT msg, UINT wParam, LONG lParam);

/* from prompt.c */
void Prompt( char* prompt, char* answer, int maxAnswer );

/* from draw.c */
void SetSurfaceSize( int dx, int dy );
void SetSurfaceOffset( int dx, int dy );
void DlgToSurface( POINT* ppt );
void SurfaceToDlg( POINT* ppt );
void SetPenColor( COLORREF color );
void SetPenWidth( int width );
void DrawMoveTo(int x, int y);
void DrawLineTo(int x, int y);
BOOL DoingRedraw();

BOOL SaveRecordedHistory(const char* filename);

BOOL Eval(char* str, int lineno);
