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

// plginvw.cpp : implementation file
//

#include "stdafx.h"
#include "plginvw.h"

#ifdef _DEBUG
#undef THIS_FILE
static char BASED_CODE THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CPluginView

#ifndef _AFXDLL
#undef new
#endif
IMPLEMENT_DYNCREATE(CPluginView, CView)
#ifndef _AFXDLL
#define new DEBUG_NEW
#endif

CPluginView::CPluginView()
{
}

CPluginView::~CPluginView()
{
}


BEGIN_MESSAGE_MAP(CPluginView, CView)
    //{{AFX_MSG_MAP(CPluginView)
        // NOTE - the ClassWizard will add and remove mapping macros here.
    ON_WM_CREATE()
    //}}AFX_MSG_MAP
END_MESSAGE_MAP()


/////////////////////////////////////////////////////////////////////////////
// CPluginView drawing

void CPluginView::OnDraw(CDC* pDC)
{
    CDocument* pDoc = GetDocument();
    // TODO: add draw code here
}

/////////////////////////////////////////////////////////////////////////////
// CPluginView overloading

int CPluginView::OnCreate(LPCREATESTRUCT lpcs)
{
    if (CWnd::OnCreate(lpcs) == -1)
        return -1;

    return 0;   // ok
}

/////////////////////////////////////////////////////////////////////////////
// CPluginView diagnostics

#ifdef _DEBUG
void CPluginView::AssertValid() const
{
    CView::AssertValid();
}

void CPluginView::Dump(CDumpContext& dc) const
{
    CView::Dump(dc);
}
#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CPluginView message handlers
