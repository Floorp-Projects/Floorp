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

// extview.cpp : implementation file
//

#include "stdafx.h"

#include "extview.h"

#ifdef _DEBUG
#undef THIS_FILE
static char BASED_CODE THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CExtView dialog

CExtView::CExtView(CWnd* pParent /*=NULL*/)
	: CDialog(CExtView::IDD, pParent)
{
	//{{AFX_DATA_INIT(CExtView)
	//}}AFX_DATA_INIT
}

void CExtView::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CExtView)
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CExtView, CDialog)
	//{{AFX_MSG_MAP(CExtView)
	ON_WM_CREATE()
	ON_BN_CLICKED(IDC_AddSubType, OnClickedAddSubType)
	ON_BN_CLICKED(IDC_LaunchExtViewer, OnClickedLaunchExtViewer)
	ON_BN_CLICKED(IDC_SaveToDisk, OnClickedSaveToDisk)
	ON_BN_CLICKED(IDC_ViewerBrowse, OnClickedViewerBrowse)
	ON_LBN_SELCHANGE(IDC_MIMETYPE, OnSelchangeMimetype)
	ON_LBN_SELCHANGE(IDC_MIMESUBTYPE, OnSelchangeMimesubtype)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CExtView message handlers

int CExtView::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CDialog::OnCreate(lpCreateStruct) == -1)
		return -1;
	
	// TODO: Add your specialized creation code here
	m_netList = cinfo_MasterListPointer();
		
	return 0;
}

BOOL CExtView::OnInitDialog() 
{
	CListBox *pMimeList = (CListBox *)GetDlgItem(IDC_MIMETYPE);

	// Fill in the possible mime types...
		
	pMimeList->AddString("audio");
	pMimeList->AddString("application");
	pMimeList->AddString("image");
	pMimeList->AddString("text");
	pMimeList->AddString("video");

	SetMimeTypeTo("image");  // select image in the mime type box ... will set rest of box up
	SetSubtypeTo("gif");
	return TRUE;
}

void CExtView::SetMimeTypeTo(const char * mime_type) {
#ifdef NOT
	CListBox *pMimeList = (CListBox *)GetDlgItem(IDC_MIMETYPE);
	CListBox *pSubtypeList = (CListBox *)GetDlgItem(IDC_MIMESUBTYPE);
	NET_cdataListItem *cd_item;
	char * pSlash;
	CEdit * pDescr = (CEdit *)GetDlgItem(IDC_Description);
	CEdit * pExt = (CEdit *)GetDlgItem(IDC_Extensions);

	pMimeList->SelectString(-1,mime_type);

	pDescr->SetWindowText(""); // fill in empty description
	pExt->SetWindowText("");  // set empty list

	pSubtypeList->ResetContent();
    cd_item = m_netList;  // Get beginning of the list

	while (cd_item) {  // iterate through the list
		if (cd_item->ci.type) {  // if it is a mime type
			if (!XP_STRNCMP(cd_item->ci.type,mime_type,4)) {  // we can distinguish the mime types via the
														   // first 2 characters but we'll compare 4 to be safe
				pSlash = XP_STRCHR(cd_item->ci.type,'//');
				if (pSlash+1) pSubtypeList->AddString(pSlash+1);
			}			
		}
		cd_item = cd_item->next;
	}
#endif
}

void CExtView::SetSubtypeTo(const char * sub_type) {
#ifdef NOT
	CListBox *pMimeList = (CListBox *)GetDlgItem(IDC_MIMETYPE);
	CListBox *pSubtypeList = (CListBox *)GetDlgItem(IDC_MIMESUBTYPE);
	CEdit * pDescr = (CEdit *)GetDlgItem(IDC_Description);
	CEdit * pExt = (CEdit *)GetDlgItem(IDC_Extensions);
	CEdit * pExtViewer = (CEdit *)GetDlgItem(IDC_ExternalViewer);
	CButton *pSaveToDisk = (CButton *)GetDlgItem(IDC_SaveToDisk);

	NET_cdataListItem *cd_item;
	int iSel = pMimeList->GetCurSel();
	CString csMimeType;
	CString csExtList;
	int idx;

	if (!sub_type) return;  // something bad

	pSubtypeList->SelectString(-1,sub_type);

	pMimeList->GetText(iSel,csMimeType); // get current mime type
	csMimeType+='/';  // add slash
	csMimeType+=sub_type; // add subtype

    cd_item = m_netList;  // Get beginning of the list

	while (cd_item) {  // iterate through the list
		if (cd_item->ci.type) {  // if it is a mime type
			if (!XP_STRCMP(cd_item->ci.type,(const char *)csMimeType)) {  // is this the type??
				if (cd_item->ci.desc) 
					pDescr->SetWindowText(cd_item->ci.desc); // fill in description
				else pDescr->SetWindowText(""); // fill in empty description
				for (idx = 0; idx < cd_item->num_exts ; idx++) {
					if (idx !=0) csExtList+=','; // if not first extension, add a comma
					if (cd_item->exts[idx]) csExtList+=cd_item->exts[idx]; // if num exts is right this will always work
				}
				if (!csExtList.IsEmpty()) 
					pExt->SetWindowText((const char *)csExtList);
				else pExt->SetWindowText("");  // set emoty list
				pSaveToDisk->SetCheck(TRUE);
			}
		}
		cd_item = cd_item->next;
	}

#endif
}

void CExtView::OnClickedAddSubType()
{
	// TODO: Add your control notification handler code here
	
}

void CExtView::OnClickedLaunchExtViewer()
{
	// TODO: Add your control notification handler code here
	
}

void CExtView::OnClickedSaveToDisk()
{
	// TODO: Add your control notification handler code here
	
}

void CExtView::OnClickedViewerBrowse()
{
	// TODO: Add your control notification handler code here
	
}

void CExtView::OnCancel()
{
	// TODO: Add extra cleanup here
	
	CDialog::OnCancel();
}

void CExtView::OnOK()
{
	// TODO: Add extra validation here
	
	CDialog::OnOK();
}

void CExtView::OnSelchangeMimetype()
{
	CListBox *pMimeList = (CListBox *)GetDlgItem(IDC_MIMETYPE);

	if (!pMimeList) return;  // something REAL bad is happening!

	int iSel = pMimeList->GetCurSel();
	CString csMimeType;

	pMimeList->GetText(iSel,csMimeType); // get mime type user selected	 
	SetMimeTypeTo(csMimeType);  // set subtypes based on mime-type	
}

void CExtView::OnSelchangeMimesubtype()
{
	// TODO: Add your control notification handler code here
	CListBox *pSubtypeList = (CListBox *)GetDlgItem(IDC_MIMESUBTYPE);

	if (!pSubtypeList) return;  // something REAL bad is happening!

	int iSel = pSubtypeList->GetCurSel();
	CString csMimeSubtype;

	pSubtypeList->GetText(iSel,csMimeSubtype); // get mime subtype user selected	 
	SetSubtypeTo(csMimeSubtype);  // set descr/ectensions/ect
	
}
