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

static int      _maxX;
static int      _maxY;
static int      _offsetX;
static int      _offsetY;
static int      _lastX;
static int      _lastY;
static COLORREF _penColor = RGB(0,0,0);
static int      _penWidth = 1;
static HPEN     _pen      = 0;

static void _newPen()
{
    if( _pen )
        DeleteObject(_pen);
    _pen = CreatePen(PS_SOLID, _penWidth, _penColor);
}    

void SetPenColor( COLORREF color )
{
    _penColor = color;
    _newPen();
}    

void SetPenWidth( int width )
{
    _penWidth = width;
    _newPen();
}    

void SetSurfaceSize( int dx, int dy )
{
    _maxX = dx;
    _maxY = dy;
}    

void SetSurfaceOffset( int dx, int dy )
{
    _offsetX = dx;
    _offsetY = dy;
}

void DlgToSurface( POINT* ppt )
{
    ppt->x -= _offsetX;
    ppt->y -= _offsetY;
}    

void SurfaceToDlg( POINT* ppt )
{
    ppt->x += _offsetX;
    ppt->y += _offsetY;
}    

void DrawMoveTo(int x, int y)
{
    x = min(_maxX-_penWidth/2,max(_penWidth/2,x));
    y = min(_maxY-_penWidth/2,max(_penWidth/2,y));

    _lastX = x;
    _lastY = y;
} 

void DrawLineTo(int x, int y)
{
    HDC hdc;
    HGDIOBJ oldPen;

    x = min(_maxX-_penWidth/2,max(_penWidth/2,x));
    y = min(_maxY-_penWidth/2,max(_penWidth/2,y));

    hdc = GetDC(g_hMainDlg);
    oldPen = SelectObject(hdc, _pen);
    MoveToEx(hdc, _lastX + _offsetX, _lastY + _offsetY, NULL);
    LineTo(hdc, x + _offsetX, y + _offsetY);
    SelectObject(hdc, oldPen);
    ReleaseDC(g_hMainDlg, hdc);
    _lastX = x;
    _lastY = y;
} 

