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

class CBrowserView;

class CFindDialog : public CFindReplaceDialog    
{
public:
    CFindDialog(CString& csSearchStr, PRBool bMatchCase,
                PRBool bMatchWholeWord, PRBool bWrapAround,
                PRBool bSearchBackwards, CBrowserView* pOwner);
    BOOL WrapAround();
    BOOL SearchBackwards();

private:
    CString m_csSearchStr;
    PRBool m_bMatchCase;
    PRBool m_bMatchWholeWord;
    PRBool m_bWrapAround;
    PRBool m_bSearchBackwards;
    CBrowserView* m_pOwner;

protected:
    virtual BOOL OnInitDialog();
    virtual void PostNcDestroy();
};

/////////////////////////////////////////////////////////////////////////////
// CLinkPropertiesDlg dialog

class CLinkPropertiesDlg : public CDialog
{
public:
	CLinkPropertiesDlg(CWnd* pParent = NULL);

	enum { IDD = IDD_DIALOG_LINK_PROPERTIES };
	CString	m_LinkText;
	CString	m_LinkLocation;

protected:
	virtual void DoDataExchange(CDataExchange* pDX);
	virtual void OnOK();

	DECLARE_MESSAGE_MAP()
};

#endif //_DIALOG_H_
