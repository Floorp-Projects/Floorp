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

// WizardMachineDlg.cpp : implementation file
//

#include "stdafx.h"
#include "WizardMachine.h"
#include "WizardMachineDlg.h"
#include "WizardUI.h"
#include "ImgDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CAboutDlg dialog used for App About

extern CWizardMachineApp theApp;
extern NODE *CurrentNode;
extern NODE* WizardTree;
HBITMAP hBitmap;
HBRUSH m_hBlueBrush;

class CAboutDlg : public CDialog
{
public:
	CAboutDlg();

// Dialog Data
	//{{AFX_DATA(CAboutDlg)
	enum { IDD = IDD_ABOUTBOX };
	//}}AFX_DATA

	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CAboutDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	//{{AFX_MSG(CAboutDlg)
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialog(CAboutDlg::IDD)
{
	//{{AFX_DATA_INIT(CAboutDlg)
	//}}AFX_DATA_INIT
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CAboutDlg)
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialog)
	//{{AFX_MSG_MAP(CAboutDlg)
		// No message handlers
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CWizardMachineDlg dialog

CWizardMachineDlg::CWizardMachineDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CWizardMachineDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CWizardMachineDlg)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
	// Note that LoadIcon does not require a subsequent DestroyIcon in Win32
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CWizardMachineDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CWizardMachineDlg)
		// NOTE: the ClassWizard will add DDX and DDV calls here
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CWizardMachineDlg, CDialog)
	//{{AFX_MSG_MAP(CWizardMachineDlg)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_WM_ERASEBKGND()
	ON_WM_CTLCOLOR()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CWizardMachineDlg message handlers

BOOL CWizardMachineDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	// Add "About..." menu item to system menu.

	// IDM_ABOUTBOX must be in the system command range.
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	CString strAboutMenu;
	strAboutMenu.LoadString(IDS_ABOUTBOX);
	if (!strAboutMenu.IsEmpty())
	{
		pSysMenu->AppendMenu(MF_SEPARATOR);
		pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
	}

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon
	
	// TODO: Add extra initialization here
        // Hide "Help" button
//         CWnd* pWnd = GetDlgItem(IDHELP);
//       ASSERT_VALID(pWnd);
//		pWnd = GetDlgItem(IDHELP);
//       ASSERT_VALID(pWnd);
//        if (pWnd)
//        {
//               pWnd->ShowWindow(SW_HIDE);
//        }


	return TRUE;  // return TRUE  unless you set the focus to a control
}

void CWizardMachineDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialog::OnSysCommand(nID, lParam);
	}
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CWizardMachineDlg::OnPaint() 
{
	if (IsIconic())
	{
		CPaintDC dc(this); // device context for painting

		SendMessage(WM_ICONERASEBKGND, (WPARAM) dc.GetSafeHdc(), 0);

		// Center icon in client rectangle
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Draw the icon
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialog::OnPaint();
	}
}

// The system calls this to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CWizardMachineDlg::OnQueryDragIcon()
{
	return (HCURSOR) m_hIcon;
}

void CWizardMachineDlg::OnOK() 
{
	// TODO: Add extra validation here
	//CDialog::OnOK();
	PropertySheetDlg = new CPropSheet(IDD_WIZARDMACHINE_DIALOG);	

	CWizardUI* pages[99];	

	for (int i = 0; i < 99; i++) {
		pages[i] = new CWizardUI;
		PropertySheetDlg->AddPage (pages[i]);
	}

	hBitmap = (HBITMAP)LoadImage(NULL, "bg.bmp", IMAGE_BITMAP, 0, 0,
											LR_LOADFROMFILE|LR_CREATEDIBSECTION);

	PropertySheetDlg->SetWizardMode();
	int PageReturnValue = PropertySheetDlg->DoModal();
	delete(PropertySheetDlg);

	if (PageReturnValue == ID_WIZFINISH)
	{
		theApp.CreateNewCache();

		NODE* tmpNode = WizardTree->childNodes[0];
		while (!tmpNode->numWidgets) {
			tmpNode = tmpNode->childNodes[0];
		}

		CurrentNode = tmpNode;
	}

	if (PageReturnValue == IDCANCEL)
	{
		theApp.CreateNewCache();

		NODE* tmpNode = WizardTree->childNodes[0];
		while (!tmpNode->numWidgets) {
			tmpNode = tmpNode->childNodes[0];
		}

		CurrentNode = tmpNode;
	}

}

BOOL CWizardMachineDlg::OnEraseBkgnd(CDC* pDC) 
{
  // Set brush to desired background color
    CBrush backBrush(RGB(192, 192, 192));

    // Save the old brush
    CBrush* pOldBrush = pDC->SelectObject(&backBrush);

    // Get the current clipping boundary
    CRect rect;
    pDC->GetClipBox(&rect);

    // Erase the area needed
    pDC->PatBlt(rect.left, rect.top, rect.Width(), rect.Height(),
         PATCOPY);

    pDC->SelectObject(pOldBrush); // Select the old brush back
	/**
	CString tmp="Testing";
	HBITMAP hBitmap = (HBITMAP)LoadImage(NULL, "bg.bmp", IMAGE_BITMAP, 0, 0,
											LR_LOADFROMFILE|LR_CREATEDIBSECTION);

	CPaintDC dc(this);
	//dc(this); // device context for painting
	
	// TODO: Add your message handler code here
	CRect rect(0, 0, 4, 8);
	MapDialogRect(&rect);

	int baseWidth = rect.Width();
	int baseHeight = rect.Height();

	CClientDC cdc(this);
	HBITMAP hbmpOld;
	CDC dcMem;

	dcMem.CreateCompatibleDC(&cdc);

	hbmpOld = (HBITMAP)::SelectObject(dcMem, hBitmap);

	dc.BitBlt((int)((float)(0.0) * (float)baseWidth / 4.0),
		(int)((float)(0.0) * (float)baseHeight / 8.0),
		(int)((float)(373.0) * (float)baseWidth / 4.0),
		(int)((float)(258.0) * (float)baseHeight / 8.0),
		&dcMem,  
		0, 
		0, 
		SRCCOPY);
	**/
	return 1;
}

HBRUSH CWizardMachineDlg::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor) 
{
	HBRUSH hbr = CDialog::OnCtlColor(pDC, pWnd, nCtlColor);

	/**
 if(pWnd -> GetDlgCtrlID() == IDC_CHECK_BUTTON)     // chexbox is set to
>"Pushlike"
> {
> pDC -> SetTextColor(m_TextColor);      // Both member variables are type
>COLORREF
> pDC ->SetBkColor(m_BkColor);
> }

in the ChangeTextColor() function
>{
> CWnd* pWnd;
> pWnd = (CWnd*)GetDlgItem(IDC_CHECK_BUTTON);
> pWnd -> Invalidate(TRUE);
>}
>
**/
	// TODO: Change any attributes of the DC here
	if (nCtlColor == CTLCOLOR_STATIC) {
		pDC->SetBkMode(OPAQUE); //Blue color
		pDC->SetBkColor(RGB(255, 255, 255)); //Blue color
		//pDC->SetForeColor(RGB(255, 255, 255)); //Blue color

		return m_hBlueBrush;
	}

	
	// TODO: Return a different brush if the default is not desired
	
	return hbr;
}

