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

/* imgcnvdl.cpp*/
/* new dialog box class derived from Troy Chevalier's CDialog class*/

// CImageConversionDialog.cpp : implementation file
//

#include "stdafx.h"
#include "edtrcdll.h"//to get g_instance
#include "platform.h"
#include "edtdlgs.h"
#include "imgcnvdl.h"
#include <assert.h>
#include "windowsx.h"
#include "xp_help.h"

int LOADDS 
CImageConversionDialog::DoModal()
{
    return CDialog::DoModal(m_parent);
}

/////////////////////////////////////////////////////////////////////////////
// CImageConversionDialog dialog


CImageConversionDialog::CImageConversionDialog(HWND pParent /*=NULL*/)
:CDialog(g_instance,IDD_FECONVERTIMAGE),m_parent(pParent)
{
	m_Doutputimagetype=0;
    m_Doptionarraycount=0;
    m_filenamelock=FALSE;
    m_listboxindex=0;
    m_wfeiface=NULL;
}

void
CImageConversionDialog::setListBoxChange(int p_index)
{
    if (checkLock())
        return;//someone has modified the filename by hand do NOT TOUCH
    if ((p_index<0)||(p_index>=(int)m_Doptionarraycount))
    {
        assert(FALSE);
        return;
    }
    attachExtention(m_outfilevalue,m_Doptionarray[p_index].m_pfileextention);
}


void
CImageConversionDialog::attachExtention(CString &p_string,const CString &p_ext)
{
    CString t_string(p_string);
    int t_index=t_string.ReverseFind('.');
    if ((t_index>0)&&((t_string.GetLength()-t_index)<5))//trying to avoid file names of c:\fred.stuff\hi
    {
        t_string=t_string.Mid(0,t_index);
        p_string=t_string;
    }
    p_string+='.';
    p_string+=p_ext;

}

BOOL
CImageConversionDialog::OnCommand(int id, HWND hwndCtl, UINT notifyCode)
{
    CString t_teststring;
    if (id==IDC_NETHELP)
    {
        if (m_wfeiface)
            m_wfeiface->WinHelp(HELP_IMAGE_CONVERSION);
    }

    switch (notifyCode)
    {
    case LBN_SELCHANGE:
        {
        DoTransfer(TRUE);
        setListBoxChange(m_listboxindex);
        DoTransfer(FALSE);
        return TRUE;
        break;
        }
    case EN_CHANGE :
        {
            DoTransfer(TRUE);
            if (m_listboxindex<0) //too early
                return TRUE;
            attachExtention(m_oldstring,m_Doptionarray[m_listboxindex].m_pfileextention);
            if (strcmp(m_oldstring,m_outfilevalue)) //if non 0 returns then it is different!
                m_filenamelock=TRUE;
            else
                m_filenamelock=FALSE;
            return TRUE;
        break;
        }
    default:
        break;
    }
    return CDialog::OnCommand(id, hwndCtl,notifyCode);
}

BOOL
CImageConversionDialog::DoTransfer(BOOL bSaveAndValidate)
{
    EditFieldTransfer(IDC_OUTFILE,m_outfilevalue,bSaveAndValidate);
    ListBoxTransfer(IDC_CONVERTLIST,m_listboxindex,bSaveAndValidate);
    return TRUE;//no validation
}



/////////////////////////////////////////////////////////////////////////////
// CImageConversionDialog message handlers

BOOL CImageConversionDialog::InitDialog() 
{
    CDialog::InitDialog();
    //init listbox
    HWND t_wnd;
    t_wnd=GetDlgItem(m_hwndDlg,IDC_CONVERTLIST);
    if(!t_wnd)
        return FALSE;
    CString t_string;
    CString t_append;
    for (DWORD i=0;i<m_Doptionarraycount;i++)
    {
        t_string=m_Doptionarray[i].m_pencodername;
        t_string+=" (.";
        t_string+=m_Doptionarray[i].m_pfileextention;
        t_string+=')';
        if (m_Doptionarray[i].m_builtin)
            t_append.LoadString(g_instance, IDS_BUILT_IN);
        else 
            t_append.LoadString(g_instance, IDS_PLUGIN);
        
        t_string+=t_append;
        ListBox_AddString(t_wnd,t_string);
    }
    ListBox_SetCurSel(t_wnd,m_Doutputimagetype);
	// TODO: Add extra initialization here
    m_outfilevalue=m_Doutfilename1;
    m_oldstring=m_outfilevalue;//initialize m_oldstring to be the same so m_filenamelock remains false BEFORE you put on the extention!
    SetFocus(t_wnd);//set focus on the listbox
    setListBoxChange(m_Doutputimagetype);
    DoTransfer(FALSE);
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void CImageConversionDialog::OnOK() 
{
	// TODO: Add extra validation here
	DoTransfer(TRUE);
    m_Doutfilename1=m_outfilevalue;

    HWND t_wnd;
    t_wnd=GetDlgItem(m_hwndDlg,IDC_CONVERTLIST);
    m_Doutputimagetype=ListBox_GetCurSel(t_wnd);
    assert(LB_ERR!=m_Doutputimagetype);
    EndDialog(m_hwndDlg,IDOK);
}

void CImageConversionDialog::OnCancel() 
{
	// TODO: Add extra cleanup here
	
    EndDialog(m_hwndDlg,IDCANCEL);
}


void LOADDS 
CImageConversionDialog::setOutputFileName1(const char *p_string)
{
    m_Doutfilename1=p_string;
}


//////////////////////////////////////////////////////
//JPEG OPTIONS DIALOG
//////////////////////////////////////////////////////


#define J_LOWQUALITY 25
#define J_MEDQUALITY 75
#define J_HIGHQUALITY 100

int LOADDS 
CJpegOptionsDialog::DoModal()
{
    return CDialog::DoModal(m_parent);
}

/////////////////////////////////////////////////////////////////////////////
// CJpegOptionsDialog dialog


CJpegOptionsDialog::CJpegOptionsDialog(HWND pParent /*=NULL*/)
:CDialog(g_instance,IDD_FEJPEGOPTIONS),m_parent(pParent)
{
	m_Doutputquality=0;
}


BOOL
CJpegOptionsDialog::OnCommand(int id, HWND hwndCtl, UINT notifyCode)
{
    return CDialog::OnCommand(id, hwndCtl,notifyCode);
}

BOOL
CJpegOptionsDialog::DoTransfer(BOOL bSaveAndValidate)
{
    RadioButtonTransfer(IDC_QUALITY3,m_outputquality,bSaveAndValidate);
    return TRUE;//no validation
}



/////////////////////////////////////////////////////////////////////////////
// CJpegOptionsDialog message handlers

BOOL CJpegOptionsDialog::InitDialog() 
{
	CDialog::InitDialog();
	
	// TODO: Add extra initialization here
    m_outputquality=0;
    if (m_Doutputquality<=J_LOWQUALITY)//33
        m_outputquality=2;
    else if (m_Doutputquality<=J_MEDQUALITY)//66
        m_outputquality=1;
    else 
        m_outputquality=0;//100
    DoTransfer(FALSE);
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void CJpegOptionsDialog::OnOK() 
{
	// TODO: Add extra validation here
	DoTransfer(TRUE);
    if (2==m_outputquality)
        m_Doutputquality=J_LOWQUALITY;
    if (1==m_outputquality)
        m_Doutputquality=J_MEDQUALITY;
    if (0==m_outputquality)
        m_Doutputquality=J_HIGHQUALITY;
    EndDialog(m_hwndDlg,IDOK);
}

void CJpegOptionsDialog::OnCancel() 
{
	// TODO: Add extra cleanup here
	
    EndDialog(m_hwndDlg,IDCANCEL);
}
 
