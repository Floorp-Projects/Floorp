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

// edprops.h : header file
//
#ifndef _TAGDLG_H
#define _TAGDLG_H
#include "edtdlgs.h"
#include "callback.h"

/////////////////////////////////////////////////////////////////////////////
// CTagDlg dialog -- Arbitrary tag editor

class CTagDlg : public CDialog ,public ITagDialog,public CWFECallbacks,public CEDTCallbacks
{
// Construction
public:
	CTagDlg(HWND pParent = NULL,
            char *pTagData = NULL);

    virtual int LOADDS DoModal();
    virtual ~CTagDlg();
private:
    HWND  m_parent;
    BOOL  m_bInsert;
    BOOL  m_bValidTag;

    int32   m_iFullWidth;
    int32   m_iFullHeight;

// Dialog Data
	CString m_csTagData;


// Implementation
protected:
/* necessary overrides */
	virtual BOOL	DoTransfer(BOOL bSaveAndValidate);
    virtual BOOL	OnCommand(int id, HWND hwndCtl, UINT notifyCode);
/*elected overrides*/
    virtual BOOL InitDialog();

/*called by OnCommand*/
    void OnOK();
    void OnCancel();
	void OnHelp();
	void OnVerifyHtml();

private:
    Bool DoVerifyTag( char* pTagString );
};

#endif // _TAGDLG_H
