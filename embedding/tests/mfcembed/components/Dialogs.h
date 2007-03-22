/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: Mozilla-sample-code 1.0
 *
 * Copyright (c) 2002 Netscape Communications Corporation and
 * other contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this Mozilla sample software and associated documentation files
 * (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to permit
 * persons to whom the Software is furnished to do so, subject to the
 * following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Contributor(s):
 *   Chak Nanga <chak@netscape.com>
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef _DIALOGS_H_
#define _DIALOGS_H_

#include "resource.h"

class CPromptDialog : public CDialog
{
public:
    CPromptDialog(CWnd* pParent, const TCHAR* pTitle, const TCHAR* pText,
                  const TCHAR* pInitPromptText,
                  BOOL bHasCheck, const TCHAR* pCheckText, int initCheckVal);
	
	// Dialog Data
    enum { IDD = IDD_PROMPT_DIALOG };
    CString m_csPromptAnswer;

    CString m_csDialogTitle;
	CString m_csPromptText;
	BOOL m_bHasCheckBox;
	CString m_csCheckBoxText;
	int m_bCheckBoxValue;
    
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CPromptDialog)
	protected:
    virtual void DoDataExchange(CDataExchange* pDX);
	//}}AFX_VIRTUAL

	//{{AFX_MSG(CPromptDialog)
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
    DECLARE_MESSAGE_MAP()
};

class CPromptPasswordDialog : public CDialog
{
public:
    CPromptPasswordDialog(CWnd* pParent, const TCHAR* pTitle, const TCHAR* pText,
                          const TCHAR* pInitPasswordText,
                          BOOL bHasCheck, const TCHAR* pCheckText, int initCheckVal);
	
	// Dialog Data
    enum { IDD = IDD_PROMPT_PASSWORD_DIALOG };

    CString m_csDialogTitle;
	CString m_csPromptText;
	CString m_csPassword;
	BOOL m_bHasCheckBox;
	CString m_csCheckBoxText;
	int m_bCheckBoxValue;

	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CPromptPasswordDialog)		
	protected:
    virtual void DoDataExchange(CDataExchange* pDX);
	//}}AFX_VIRTUAL

	//{{AFX_MSG(CPromptPasswordDialog)
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
    DECLARE_MESSAGE_MAP()
};

class CPromptUsernamePasswordDialog : public CDialog
{
public:
    CPromptUsernamePasswordDialog(CWnd* pParent, const TCHAR* pTitle, const TCHAR* pText,
                                  const TCHAR* pInitUsername, const TCHAR* pInitPassword, 
		                          BOOL bHasCheck, const TCHAR* pCheckText, int initCheckVal);
	
	// Dialog Data
    enum { IDD = IDD_PROMPT_USERPASS_DIALOG };

    CString m_csDialogTitle;
	CString m_csPromptText;
	CString m_csUserNameLabel;
	CString m_csPasswordLabel;
	CString m_csPassword;
	CString m_csUserName;
	BOOL m_bHasCheckBox;
	CString m_csCheckBoxText;
	int m_bCheckBoxValue;

	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CPromptUsernamePasswordDialog)	
	protected:
    virtual void DoDataExchange(CDataExchange* pDX);
	//}}AFX_VIRTUAL

	//{{AFX_MSG(CPromptUsernamePasswordDialog)
	virtual BOOL OnInitDialog();
	//}}AFX_MSG

    DECLARE_MESSAGE_MAP()
};

class CAlertCheckDialog : public CDialog
{
public:
    CAlertCheckDialog(CWnd* pParent, const TCHAR* pTitle, const TCHAR* pText,
                  const TCHAR* pCheckText, int initCheckVal);
	
    // Dialog Data
    enum { IDD = IDD_ALERT_CHECK_DIALOG };

    CString m_csDialogTitle;
    CString m_csMsgText;
    CString m_csCheckBoxText;
    int m_bCheckBoxValue;
    
    // ClassWizard generated virtual function overrides
    //{{AFX_VIRTUAL(CAlertCheckDialog)
    protected:
    virtual void DoDataExchange(CDataExchange* pDX);
    //}}AFX_VIRTUAL

    //{{AFX_MSG(CAlertCheckDialog)
    virtual BOOL OnInitDialog();
    //}}AFX_MSG

    DECLARE_MESSAGE_MAP()
};

class CConfirmCheckDialog : public CDialog
{
public:
    CConfirmCheckDialog(CWnd* pParent, const TCHAR* pTitle, const TCHAR* pText,
                  const TCHAR* pCheckText, int initCheckVal,
                  const TCHAR *pBtn1Text, const TCHAR *pBtn2Text, 
                  const TCHAR *pBtn3Text);
	
    // Dialog Data
    enum { IDD = IDD_CONFIRM_CHECK_DIALOG };

    CString m_csDialogTitle;
    CString m_csMsgText;
    CString m_csCheckBoxText;
    int m_bCheckBoxValue;
    CString m_csBtn1Text;
    CString m_csBtn2Text;
    CString m_csBtn3Text;

    // ClassWizard generated virtual function overrides
    //{{AFX_VIRTUAL(CConfirmCheckDialog)
    protected:
    virtual void DoDataExchange(CDataExchange* pDX);
    //}}AFX_VIRTUAL

    //{{AFX_MSG(CConfirmCheckDialog)
    virtual BOOL OnInitDialog();
    afx_msg void OnBtn1Clicked();
    afx_msg void OnBtn2Clicked();
    afx_msg void OnBtn3Clicked();
    //}}AFX_MSG

    DECLARE_MESSAGE_MAP()
};

#endif //_DIALOG_H_
