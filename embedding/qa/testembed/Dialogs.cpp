/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Chak Nanga <chak@netscape.com> 
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "stdafx.h"
#include "Dialogs.h"
#include "BrowserView.h"

// File overview....
//
// contains Find dialog box code
//

//--------------------------------------------------------------------------//
//				CFindDialog Stuff
//--------------------------------------------------------------------------//

CFindDialog::CFindDialog(CString& csSearchStr, PRBool bMatchCase,
				PRBool bMatchWholeWord, PRBool bWrapAround,
				PRBool bSearchBackwards, CBrowserView* pOwner)
				: CFindReplaceDialog()
{
	// Save these initial settings off in member vars
	// We'll use these to initialize the controls
	// in InitDialog()
	m_csSearchStr = csSearchStr;
	m_bMatchCase = bMatchCase;
	m_bMatchWholeWord = bMatchWholeWord;
	m_bWrapAround = bWrapAround;
	m_bSearchBackwards = bSearchBackwards;
	m_pOwner = pOwner;

	// Set up to load our customized Find dialog template
	// rather than the default one MFC provides
	m_fr.Flags |= FR_ENABLETEMPLATE;
	m_fr.hInstance = AfxGetInstanceHandle();
	m_fr.lpTemplateName = MAKEINTRESOURCE(IDD_FINDDLG);
}

BOOL CFindDialog::OnInitDialog() 
{
	CFindReplaceDialog::OnInitDialog();

	CEdit* pEdit = (CEdit *)GetDlgItem(IDC_FIND_EDIT);
	if(pEdit)
		pEdit->SetWindowText(m_csSearchStr);

	CButton* pChk = (CButton *)GetDlgItem(IDC_MATCH_CASE);
	if(pChk)
		pChk->SetCheck(m_bMatchCase);

	pChk = (CButton *)GetDlgItem(IDC_MATCH_WHOLE_WORD);
	if(pChk)
		pChk->SetCheck(m_bMatchWholeWord);

	pChk = (CButton *)GetDlgItem(IDC_WRAP_AROUND);	
	if(pChk)
		pChk->SetCheck(m_bWrapAround);

	pChk = (CButton *)GetDlgItem(IDC_SEARCH_BACKWARDS);
	if(pChk)
		pChk->SetCheck(m_bSearchBackwards);

	return TRUE; 
}

void CFindDialog::PostNcDestroy()	
{
	// Let the owner know we're gone
	if(m_pOwner != NULL)	
		m_pOwner->ClearFindDialog();

	CFindReplaceDialog::PostNcDestroy();
}

BOOL CFindDialog::WrapAround()
{
	CButton* pChk = (CButton *)GetDlgItem(IDC_WRAP_AROUND);

	return pChk ? pChk->GetCheck() : FALSE;
}

BOOL CFindDialog::SearchBackwards()
{
	CButton* pChk = (CButton *)GetDlgItem(IDC_SEARCH_BACKWARDS);

	return pChk ? pChk->GetCheck() : FALSE;
}
