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

// dlghtmrp.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CHtmlRecipientsDlg dialog

#ifndef __dlghtmrp__
#define __dlghtmrp__

//Recipients List box:  We need to due extra sorting on the data we display
//so we are having to do an ownerdraw list box.

class CListBoxRecipients : public CListBox
{
public:
	int	m_iPosIndex;
	int m_iPosName;
	int m_iPosStatus;

protected:
	virtual int CompareItem( LPCOMPAREITEMSTRUCT lpCompareItemStruct);
	virtual void DeleteItem( LPDELETEITEMSTRUCT lpDeleteItemStruct);

	virtual void DrawItem( LPDRAWITEMSTRUCT lpDrawItemStruct );

	virtual void MeasureItem( LPMEASUREITEMSTRUCT lpMeasureItemStruct )
	{
			lpMeasureItemStruct->itemHeight = 16;
	}

public:
	CListBoxRecipients();

	void SetColumnPositions(int iPosIndex, int iPosName, int iPosStatus);
};



//Recipients Dialog class

class CHtmlRecipientsDlg : public CDialog
{
// Construction
public:
	CHtmlRecipientsDlg(MSG_Pane* pComposePane,
					   MSG_RecipientList* nohtml,
					   MSG_RecipientList* htmlok,
					   CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CHtmlRecipientsDlg)
	enum { IDD = IDD_HTML_RECIPIENTS };
		// NOTE: the ClassWizard will add data members here
	//}}AFX_DATA
	MSG_Pane* m_pComposePane;
	MSG_RecipientList *m_pNoHtml;
	MSG_RecipientList *m_pHtmlOk;

	CListBoxRecipients m_ListBox1; //Doesn't prefer HTML list
	CListBoxRecipients m_ListBox2; //Pefers HTML list


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CHtmlRecipientsDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	BOOL PopulateLists();

	// Generated message map functions
	//{{AFX_MSG(CHtmlRecipientsDlg)
	virtual void OnOK();
	virtual void OnCancel();
	afx_msg void OnHelp();
	afx_msg void OnBtnAdd();
	afx_msg void OnBtnRemove();
	virtual BOOL OnInitDialog();
	afx_msg void OnSetfocusList1();
	afx_msg void OnSetfocusList2();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//Used to launch the dialog when passed in as a callback
int CreateRecipientsDialog(MSG_Pane* composepane, void* closure,
								  MSG_RecipientList* nohtml,
								  MSG_RecipientList* htmlok,void *pWnd);

#endif
