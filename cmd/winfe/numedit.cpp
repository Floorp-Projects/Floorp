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
//NumEdit.cpp :implementation file

#include "stdafx.h"
#include "numedit.h"


#ifdef DEBUG
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


CNSNumEdit::CNSNumEdit()
{
}


//=========================================================== ~CNSNumEdit
CNSNumEdit::~CNSNumEdit()
{
}

BOOL CNSNumEdit::PreTranslateMessage( MSG* pMsg )
{
    return CEdit::PreTranslateMessage(pMsg);
}


/////////////////////////////////////////////////////////////////////////////
// CNSNumEdit message handlers

BEGIN_MESSAGE_MAP(CNSNumEdit, CEdit)
    ON_WM_CHAR()
    ON_WM_SETFOCUS()
END_MESSAGE_MAP()

//==================================================================== OnChar
void CNSNumEdit::OnChar( UINT nChar, UINT nRepCnt, UINT nFlags )
{
    if ( ((nChar >= '0') && (nChar <= '9')) || (nChar == 0x08) ) 
	{
        CEdit::OnChar(nChar, nRepCnt, nFlags);
    }
    else
	{
        MessageBeep( MB_OK );
    }
}

//=============================================================== OnSetFocus
void CNSNumEdit::OnSetFocus( CWnd* pOldWnd )
{
    CEdit::OnSetFocus( pOldWnd );
    SetSel(0,-1);
}

//================================================================== SetValue
int CNSNumEdit::SetValue( int nNewValue )
{
    char buff[10];
	if (nNewValue > 0)
    	SetWindowText( itoa( nNewValue, buff, 10 ) );
	else
    	SetWindowText("");
    return nNewValue;
}


//================================================================== GetValue
int CNSNumEdit::GetValue( void )
{
    char buff[10];
    GetWindowText( buff, 10 );
    if ( strlen( buff ) > 0 )
            return atoi( buff );
    return 0;
}
