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

#ifndef _TOOLTIP_H
#define _TOOLTIP_H

#include <afxwin.h>
#include <afxext.h>
#include <afxpriv.h>
#include <afxole.h>
#include <afxdisp.h>
#include <afxodlgs.h>
#ifdef _WIN32
#include <afxcmn.h>
#endif

#ifndef XP_WIN32
#ifdef FEATURE_TOOLTIPS
#include "tooltip.i00"
#endif
#else
class CNSToolTip2 : public CToolTipCtrl {


public:
	//Construction
	CNSToolTip2();
	~CNSToolTip2();
protected:
	virtual LRESULT WindowProc( UINT message, WPARAM wParam, LPARAM lParam );
 

	// Generated message map functions
	//{{AFX_MSG(CNSToolTip2)
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

public:
    void SetCSID(int csid);
    void SetBounding(int *coord, int num, int x = 0, int y = 0);
};
#endif

#ifndef XP_WIN32
typedef class CNSToolTip CNSToolTip2;
#else
typedef class CNSToolTip2 CNSToolTip;
#endif

#endif
