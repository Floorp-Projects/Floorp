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

#ifndef __PluginView_H
#define __PluginView_H
// plginvw.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CPluginView view

class CPluginView : public CView
{
protected:
    DECLARE_DYNCREATE(CPluginView)

// Attributes
public:

// Operations
public:
    CPluginView();           // protected constructor used by dynamic creation
    virtual ~CPluginView();

// Overrides
    // ClassWizard generated virtual function overrides
    //{{AFX_VIRTUAL(CPluginView)
    protected:
    virtual void OnDraw(CDC* pDC);      // overridden to draw this view
    //}}AFX_VIRTUAL

// Implementation
protected:
#ifdef _DEBUG
    virtual void AssertValid() const;
    virtual void Dump(CDumpContext& dc) const;
#endif

    // Generated message map functions
protected:
    //{{AFX_MSG(CPluginView)
        // NOTE - the ClassWizard will add and remove member functions here.
    afx_msg int OnCreate(LPCREATESTRUCT lpcs);
    //}}AFX_MSG
    DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////
#endif  // __PluginView_H
