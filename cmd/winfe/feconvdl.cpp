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

// FE_DialogConvert.cpp : implementation file
//

#include "stdafx.h"
#include "resource.h"
#include "feconvdl.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// FE_DialogConvert dialog


FE_DialogConvert::FE_DialogConvert(CWnd* pParent /*=NULL*/)
	: CDialog(FE_DialogConvert::IDD, pParent)
{
	//{{AFX_DATA_INIT(FE_DialogConvert)
	m_imagetypevalue = -1;
	m_outfilevalue = _T("");
	m_inputimagesize = _T("");
	m_inputimagetype = _T("");
	m_qualityvalue = -1;
	//}}AFX_DATA_INIT
	m_Doutputimagetype1=0;
	m_Doutputimagequality1=0;
}


void FE_DialogConvert::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(FE_DialogConvert)
	DDX_Radio(pDX, IDC_IMAGETYPE, m_imagetypevalue);
	DDX_Text(pDX, IDC_OUTFILE, m_outfilevalue);
	DDX_Text(pDX, IDC_INPUTIMAGESIZE, m_inputimagesize);
	DDX_Text(pDX, IDC_INPUTIMAGETYPE, m_inputimagetype);
	DDX_Radio(pDX, IDC_QUALITY1, m_qualityvalue);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(FE_DialogConvert, CDialog)
	//{{AFX_MSG_MAP(FE_DialogConvert)
	ON_BN_CLICKED(IDC_IMAGETYPE, OnJPEG)
	ON_BN_CLICKED(IDC_IMAGETYPE2, OnPNG)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// FE_DialogConvert message handlers

BOOL FE_DialogConvert::OnInitDialog() 
{
	CDialog::OnInitDialog();
	
	// TODO: Add extra initialization here
	m_outfilevalue=m_Doutfilename1;
	m_imagetypevalue=m_Doutputimagetype1;
    if (m_Doutputimagequality1<=33)
        m_qualityvalue=0;
    else if (m_Doutputimagequality1<=66)
        m_qualityvalue=1;
    else if (m_Doutputimagequality1>66)
        m_qualityvalue=2;

    m_inputimagetype=m_Dinputimagetype;
	_ultoa(m_Dinputimagesize,m_inputimagesize.GetBuffer(255),10);
	m_inputimagesize.ReleaseBuffer();
	UpdateData(FALSE);
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void FE_DialogConvert::OnOK() 
{
	// TODO: Add extra validation here
	UpdateData(TRUE);
	m_Doutfilename1=m_outfilevalue;
	m_Doutputimagetype1=m_imagetypevalue;
	switch (m_qualityvalue)
    {
    case 0:m_Doutputimagequality1=25;break;
    case 1:m_Doutputimagequality1=75;break;
    case 2:m_Doutputimagequality1=100;break;
    }

    CDialog::OnOK();
}

void FE_DialogConvert::OnCancel() 
{
	// TODO: Add extra cleanup here
	
	CDialog::OnCancel();
}

void FE_DialogConvert::OnJPEG() 
{
	// TODO: Add your control notification handler code here
	
}

void FE_DialogConvert::OnPNG() 
{
	// TODO: Add your control notification handler code here
	
}
