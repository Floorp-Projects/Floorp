/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

// PropSheet.cpp : implementation file
//

#include "stdafx.h"
#include "WizardMachine.h"
#include "PropSheet.h"
#include "globals.h" //To access global variables
#include "Interpret.h" //Adding to enable OpenBrowser function.


#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CPropSheet

IMPLEMENT_DYNAMIC(CPropSheet, CPropertySheet)

CPropSheet::CPropSheet(UINT nIDCaption, CWnd* pParentWnd, UINT iSelectPage)
	:CPropertySheet(nIDCaption, pParentWnd, iSelectPage)
{
}

CPropSheet::CPropSheet(LPCTSTR pszCaption, CWnd* pParentWnd, UINT iSelectPage)
	:CPropertySheet(pszCaption, pParentWnd, iSelectPage)
{
}

CPropSheet::~CPropSheet()
{
}


BEGIN_MESSAGE_MAP(CPropSheet, CPropertySheet)
	//{{AFX_MSG_MAP(CPropSheet)
		// NOTE - the ClassWizard will add and remove mapping macros here.
	//}}AFX_MSG_MAP
	ON_BN_CLICKED(IDC_FEEDBACK_BUTTON, OnButtonCopy)
	ON_BN_CLICKED(IDCANCEL, OnCancelButn)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CPropSheet message handlers
extern CInterpret *theInterpreter;
BOOL CPropSheet::OnInitDialog() 
{
	BOOL bResult = CPropertySheet::OnInitDialog();
	
	// TODO: Add your specialized code here
	CRect rect, tabrect;// = CRect (800, 440, 950, 645);//, tabrect;
	int width;

	//Get button sizes and positions
	GetDlgItem(IDCANCEL)->GetWindowRect(rect);
	GetTabControl()->GetWindowRect(tabrect);
	ScreenToClient(rect); ScreenToClient(tabrect);
	
	//New button - width, height and Y-coordiate of IDOK
	//           - X-coordinate of tab control
	width = rect.Width();
	rect.left = tabrect.left; rect.right = tabrect.left + width;
	CString FeedbackChoice = GetGlobal("FeedbackChoice");
	CString FeedbackButtonName = GetGlobal("FeedbackButtonName");
	if (FeedbackChoice.Compare("Yes") == 0)
	{
	//Create new "Add" button and set standard font
	m_ButtonCopy.Create(FeedbackButtonName,
			BS_PUSHBUTTON|WS_CHILD|WS_VISIBLE|WS_TABSTOP,
			rect, this, IDC_FEEDBACK_BUTTON);
	m_ButtonCopy.SetFont(GetFont());
	}
	else
	;
	SetIcon(AfxGetApp()->LoadIcon(IDR_MAINFRAME), FALSE);
	SetIcon(AfxGetApp()->LoadIcon(IDR_MAINFRAME), TRUE);
	return bResult;
}

void CPropSheet::OnButtonCopy()
{
	CString Feedback = GetGlobal("Feedback");

	theInterpreter->OpenBrowser((char*)(LPCTSTR)Feedback);

}

void CPropSheet::OnCancelButn()
{

	if(::MessageBox(m_hWnd, "Are you sure you want to exit?",
       "Confirmation", MB_YESNO | MB_DEFBUTTON2 | MB_ICONQUESTION) == IDNO)
		return;
	CPropSheet::EndDialog(IDCANCEL);
}
