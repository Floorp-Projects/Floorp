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

// FE_DialogConvert.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// FE_DialogConvert dialog

class FE_DialogConvert : public CDialog
{
// Construction
public:
	FE_DialogConvert(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(FE_DialogConvert)
	enum { IDD = IDD_FECONVERTIMAGE };
	int		m_imagetypevalue;
	CString	m_outfilevalue;
	CString	m_inputimagesize;
	CString	m_inputimagetype;
	int		m_qualityvalue;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(FE_DialogConvert)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(FE_DialogConvert)
	virtual BOOL OnInitDialog();
	virtual void OnOK();
	virtual void OnCancel();
	afx_msg void OnJPEG();
	afx_msg void OnPNG();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
private:
	int m_Doutputimagetype1; //0=JPEG 1=GIF
	int m_Doutputimagequality1;//0-100
	CString m_Doutfilename1;
	CString m_Dinputimagetype;
	int m_Dinputimagesize;
public:
	void setOutputImageType1(int p_int){m_Doutputimagetype1=p_int;}
	void setOutputImageQuality1(int p_int){m_Doutputimagequality1=p_int;}
	void setOutputFileName1(const char *p_string){m_Doutfilename1=p_string;}

    void setInputImageTypeString(const char *p_string){m_Dinputimagetype=p_string;}
	void setInputImageSize(DWORD p_dword){m_Dinputimagesize=CASTINT(p_dword);}

    const CString &getOutputFileName1(){return m_Doutfilename1;}
	int getOutputImageQuality1(){return m_Doutputimagequality1;}
	int getOutputImageType1(){return m_Doutputimagetype1;}

};
