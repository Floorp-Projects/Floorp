/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

// ImageDialog.cpp : implementation file
//

#include "stdafx.h"
#include "WizardMachine.h"
#include "ImageDialog.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CImageDialog dialog

extern char iniFilePath[MAX_SIZE];
extern char imagesPath[MAX_SIZE];

CImageDialog::CImageDialog(CWnd* pParent /*=NULL*/)
	: CDialog(CImageDialog::IDD, pParent)
{
	//{{AFX_DATA_INIT(CImageDialog)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
}

CImageDialog::CImageDialog(CString theIniFileName, CWnd* pParent /*=NULL*/)
	: CDialog(CImageDialog::IDD, pParent)
{
	//{{AFX_DATA_INIT(CImageDialog)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
	imageSectionName = "IMAGE";

	// All the images to be displayed on button clicks are 
	// in this iniFile. So iniFileName is initialized thus.
	iniFileName = CString(iniFilePath) + theIniFileName;
	ReadImageFromIniFile();
}

void CImageDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CImageDialog)
		// NOTE: the ClassWizard will add DDX and DDV calls here
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CImageDialog, CDialog)
	//{{AFX_MSG_MAP(CImageDialog)
	ON_WM_PAINT()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CImageDialog message handlers

void CImageDialog::OnPaint() 
{
	CPaintDC dc(this); // device context for painting
	
	// TODO: Add your message handler code here

	CRect rect(0, 0, 4, 8);
	MapDialogRect(&rect);

	int baseWidth = rect.Width();
	int baseHeight = rect.Height();

	CClientDC cdc(this);
	HBITMAP hbmpOld;
	CDC dcMem;

	dcMem.CreateCompatibleDC(&cdc);

	hbmpOld = (HBITMAP)::SelectObject(dcMem, image.hBitmap);

	dc.BitBlt((int)((float)(image.location.x) * (float)baseWidth / 4.0),
		(int)((float)(image.location.y) * (float)baseHeight / 8.0),
		(int)((float)(image.size.width) * (float)baseWidth / 4.0),
		(int)((float)(image.size.height) * (float)baseHeight / 8.0),
		&dcMem,  
		0, 
		0, 
		SRCCOPY);

	// Do not call CDialog::OnPaint() for painting messages
}

void CImageDialog::ReadImageFromIniFile()
{
	char buffer[500];
	GetPrivateProfileString(imageSectionName, "Name", "", buffer, 250, iniFileName);
	image.name = CString(imagesPath) + CString(buffer);

	GetPrivateProfileString(imageSectionName, "start_X", "", buffer, 250, iniFileName);
	image.location.x = atoi(buffer);

	GetPrivateProfileString(imageSectionName, "start_Y", "", buffer, 250, iniFileName);
	image.location.y = atoi(buffer);

	GetPrivateProfileString(imageSectionName, "width", "", buffer, 250, iniFileName);
	image.size.width = atoi(buffer);

	GetPrivateProfileString(imageSectionName, "height", "", buffer, 250, iniFileName);
	image.size.height = atoi(buffer);

	image.hBitmap = (HBITMAP)LoadImage(NULL, image.name, IMAGE_BITMAP, 0, 0,
											LR_LOADFROMFILE|LR_CREATEDIBSECTION);
}

void CImageDialog::OnHelp() 
{
		CWnd Mywnd;
		Mywnd.MessageBox("hello","hello",MB_OK);

	// TODO: Add your control notification handler code here
	
}
