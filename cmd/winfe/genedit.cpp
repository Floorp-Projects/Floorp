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
 
#include "stdafx.h"

#include "genedit.h"

#ifdef _DEBUG
#undef THIS_FILE
static char BASED_CODE THIS_FILE[] = __FILE__;
#endif

IMPLEMENT_DYNAMIC( CGenericEdit, CEdit )

/////////////////////////////////////////////////////////////////////////////
// CGenericEdit

CGenericEdit::CGenericEdit()
{
}

CGenericEdit::~CGenericEdit()
{
}


BEGIN_MESSAGE_MAP(CGenericEdit, CEdit)
	//{{AFX_MSG_MAP(CGenericEdit)
	ON_COMMAND(ID_EDIT_COPY, OnEditCopy)
	ON_UPDATE_COMMAND_UI(ID_EDIT_COPY, OnUpdateEditCopy)
	ON_COMMAND(ID_EDIT_CUT, OnEditCut)
	ON_UPDATE_COMMAND_UI(ID_EDIT_CUT, OnUpdateEditCut)
	ON_COMMAND(ID_EDIT_PASTE, OnEditPaste)
	ON_UPDATE_COMMAND_UI(ID_EDIT_PASTE, OnUpdateEditPaste)
	ON_COMMAND(ID_EDIT_UNDO, OnEditUndo)
	ON_UPDATE_COMMAND_UI(ID_EDIT_UNDO, OnUpdateEditUndo)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


/////////////////////////////////////////////////////////////////////////////
// CGenericEdit message handlers

void CGenericEdit::OnEditCopy() 
{
	Copy();	
}

void CGenericEdit::OnUpdateEditCopy(CCmdUI* pCmdUI) 
{
	int iStartSel, iEndSel;
	GetSel(iStartSel, iEndSel);

	if(iStartSel != iEndSel)	{
		pCmdUI->Enable(TRUE);
	}
	else	{
		pCmdUI->Enable(FALSE);
	}
}

void CGenericEdit::OnEditCut() 
{
	Cut();
}

void CGenericEdit::OnUpdateEditCut(CCmdUI* pCmdUI) 
{
	int iStartSel, iEndSel;
	GetSel(iStartSel, iEndSel);

	if(iStartSel != iEndSel)	{
		pCmdUI->Enable(TRUE);
	}
	else	{
		pCmdUI->Enable(FALSE);
	}
}

void CGenericEdit::OnEditPaste() 
{
	Paste();
}

void CGenericEdit::OnUpdateEditPaste(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(
		::IsClipboardFormatAvailable(CF_TEXT)
#ifdef XP_WIN32
			||	::IsClipboardFormatAvailable(CF_TEXT)
#endif
	);
	

}

void CGenericEdit::OnEditUndo() 
{
	Undo();
}

void CGenericEdit::OnUpdateEditUndo(CCmdUI* pCmdUI) 
{
    pCmdUI->SetText(szLoadString(IDS_UNDO));
	pCmdUI->Enable(CanUndo());
}
