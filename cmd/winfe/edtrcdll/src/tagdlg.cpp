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
// edprops.cpp : implementation file
//
#include "stdafx.h"
#include "edtrcdll.h"//to get g_instance
#include "edtdlgs.h"
#include "tagdlg.h"

#ifndef ID_HELP
#ifdef _WIN32
#define ID_HELP 0xE146
#else
#define ID_HELP 0xE145
#endif
#endif

/////////////////////////////////////////////////////////////////////////////
// CTagDlg dialog - The Arbitrary tag editor (Yes we can do forms!)

CTagDlg::~CTagDlg()
{
}

         
CTagDlg::CTagDlg(HWND pParent, 
                 char *pTagData)
	: CDialog(g_instance,IDD_PROPS_HTML_TAG),
      m_bInsert(0),
      m_bValidTag(0),
      m_parent(pParent)
{
	if (pTagData)
    {
        m_csTagData = pTagData;
        //new char[strlen(pTagData)+1];
        //strcpy(m_csTagData,pTagData);
    }
    else
    {
        //m_csTagData=new char[255];
        //m_csTagData[0]=0;
    }
}


int LOADDS
CTagDlg::DoModal()
{
    return CDialog::DoModal(m_parent);
}

BOOL CTagDlg::DoTransfer(BOOL bSaveAndValidate)
{
    EditFieldTransfer(IDC_EDIT_TAG,m_csTagData,bSaveAndValidate);
    return TRUE;
}

BOOL
CTagDlg::OnCommand(int id, HWND hwndCtl, UINT notifyCode)
{
    switch(id)
    {
    case IDOK: OnOK();return TRUE;break;
    case IDCANCEL: OnCancel();return TRUE;break;
    case ID_HELP: OnHelp(); return TRUE; break;
    case IDC_VERIFY_HTML: OnVerifyHtml(); return TRUE; break;
    default: return FALSE;
    }
    return FALSE;
}


/////////////////////////////////////////////////////////////////////////////
// CTagDlg message handlers

BOOL CTagDlg::InitDialog() 
{
//XPCALLS THAT NEED TO BE DONT IN CONSTRUCTOR ARE MOVED HERE
    getWFE()->GetLayoutViewSize(&m_iFullWidth, &m_iFullHeight);
//END XPCALLS FROM CONSTRUCTOR
    
    if( ED_ELEMENT_UNKNOWN_TAG == getEDT()->GetCurrentElementType() ) {
        m_csTagData = getEDT()->GetUnknownTagData();
    } else {
        m_bInsert = TRUE;
    }
    DoTransfer(FALSE);
  	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

Bool CTagDlg::DoVerifyTag( char* pTagString ){
    ED_TagValidateResult e = getEDT()->ValidateTag((char*)LPCSTR(m_csTagData), FALSE );
    UINT nIDS = 0;
    switch( e ){
        case ED_TAG_OK:
            return TRUE;
        case ED_TAG_UNOPENED:
            nIDS = IDS_TAG_UNOPENED;
            break;
        case ED_TAG_UNCLOSED:
            nIDS = IDS_TAG_UNCLOSED;
            break;
        case ED_TAG_UNTERMINATED_STRING:
            nIDS = IDS_TAG_UNTERMINATED_STRING;
            break;
        case ED_TAG_PREMATURE_CLOSE:
            nIDS = IDS_TAG_PREMATURE_CLOSE;
            break;
        case ED_TAG_TAGNAME_EXPECTED:
            nIDS = IDS_TAG_TAGNAME_EXPECTED;
            break;
        default:
            nIDS = IDS_TAG_ERROR;
    }

    MessageBox( m_hwndDlg,getWFE()->szLoadString(nIDS), 
                getWFE()->szLoadString(IDS_ERROR_HTML_CAPTION),
                MB_OK | MB_ICONEXCLAMATION);
    return FALSE;
}

void CTagDlg::OnOK() 
{
    DoTransfer(TRUE);
    if( !DoVerifyTag( (char*)LPCSTR(m_csTagData) ) ){
        return;
    }

    getEDT()->BeginBatchChanges();
	
    if( m_bInsert ){
        getEDT()->InsertUnknownTag((char*)LPCSTR(m_csTagData));
    } else {
        getEDT()->SetUnknownTagData((char*)LPCSTR(m_csTagData));
    }
    getEDT()->EndBatchChanges();

    //Note: For Attributes-only editing(e.g., HREF JavaScript),
    //      caller must get data from m_csTagData;
    EndDialog(m_hwndDlg,IDOK);
}

void CTagDlg::OnCancel() 
{
	// TODO: Add extra cleanup here
    EndDialog(m_hwndDlg,IDCANCEL);
}


void CTagDlg::OnHelp() 
{
    getWFE()->WinHelp(0);
}

void CTagDlg::OnVerifyHtml() 
{
    DoTransfer(TRUE);
    DoVerifyTag((char*)LPCSTR(m_csTagData));
}

#if 0
//TODO: Extend this to include a list of built-in tag "templates"
void CTagDlg::OnAddNewTag() 
{
    // Set the starting page for Tag Templates in the
    //  editor preferences and run it
    theApp.m_pLastEditorPrefPage->Set(EDIT_PAGE_TAGS);
    ((CGenericFrame*)(WINCX(m_pMWContext)->GetFrame()))->OnPrefsEditor();
}

void CTagDlg::OnSelchangeTagList() 
{
	// TODO: Add your control notification handler code here
	
}
#endif

