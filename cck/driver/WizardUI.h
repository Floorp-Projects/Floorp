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

// WizardUI.h : header file
//
#include "ProgressDialog.h"
#include "WizardTypes.h"

/////////////////////////////////////////////////////////////////////////////
// CWizardUI dialog
#include <afxmt.h>

class CWizardUI : public CPropertyPage
{
	DECLARE_DYNCREATE(CWizardUI)

// Construction
public:
	CWizardUI();
	~CWizardUI();
//	void SetModified(BOOL how =TRUE);

	// WizardMachine needs this
	static CString GetScreenValue(WIDGET *curWidget);

	// Not actually being used anywhere anymore
	static BOOL ActCommand(WIDGET *curWidget);
	static BOOL SortList(WIDGET *curWidget);

// Dialog Data
	//{{AFX_DATA(CWizardUI)
	enum { IDD = IDD_BASE_DIALOG };
		// NOTE - ClassWizard will add data members here.
		//    DO NOT EDIT what you see in these blocks of generated code !
	//}}AFX_DATA


// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(CWizardUI)
	public:
	virtual BOOL OnKillActive();
	virtual BOOL OnSetActive();
	virtual LRESULT OnWizardBack();
	virtual LRESULT OnWizardNext();
	virtual BOOL OnWizardFinish();
	virtual BOOL OnApply();
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual BOOL OnCommand(WPARAM wParam, LPARAM lParam);
	//}}AFX_VIRTUAL

// Implementation
	CProgressDialog *m_pProgressDlg;
	CFont* m_pFont;
	CFont* m_pNavFont;
	CFont* m_pCurFont;
	CFont* m_pJapaneseFont;
	CFont* m_pChineseSimpFont;

	int m_pControlCount;
	int m_pControlID;
	BOOL containsImage;

	BOOL SetDescription(WIDGET *w);
	void SortWidgetsForTabOrder();
	void EnableWidget(WIDGET *curWidget);
	void UpdateScreenWidget(WIDGET *curWidget);
	void CreateControls();
	void DisplayControls();
	void UpdateGlobals();
	void DestroyCurrentScreenWidgets();
	//void DoBuildInstallers();

	// Dlls related stuff
	HINSTANCE hModule;
	typedef void (MYFUNCTION) ();
	MYFUNCTION* pMyFunction;

	typedef CString (MYBUILDFUNC) ();
	MYBUILDFUNC* pMyBuildFunc;

	typedef void (MYDLLPATH) (CString);
	MYDLLPATH* pMyDllPath;
		
	HINSTANCE hGlobal;
	typedef void (MYSETGLOBAL) (char*, char*);
	MYSETGLOBAL* pMySetGlobal;
	void LoadGlobals();
	void ReadIniFile();
	void MergeFiles();
	void CreateMedia();
	void ExitDemo();

protected:
	// Generated message map functions
	//{{AFX_MSG(CWizardUI)
	afx_msg void OnPaint();
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

};
