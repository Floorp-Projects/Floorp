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

// ImageDialog.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CImageDialog dialog

class CImageDialog : public CDialog
{
// Construction
public:
	CImageDialog(CWnd* pParent = NULL);   // standard constructor
	CImageDialog(CString theIniFileName, CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CImageDialog)
	enum { IDD = IDD_IMAGE_DIALOG };
		// NOTE: the ClassWizard will add data members here
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CImageDialog)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
	typedef struct DIMENSION
	{
		int width;
		int height;
	}DIMENSION;

	typedef struct IMAGE
	{
		CString name;
		POINT location;
		DIMENSION size;	
		HBITMAP hBitmap;
	}IMAGE;

	CString imageSectionName;
	CString iniFileName;
	IMAGE image;

	void ReadImageFromIniFile();
protected:

	// Generated message map functions
	//{{AFX_MSG(CImageDialog)
	afx_msg void OnPaint();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};
