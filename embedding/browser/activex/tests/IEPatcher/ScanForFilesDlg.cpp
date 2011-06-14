// ScanForFilesDlg.cpp : implementation file
//

#include "stdafx.h"
#include "IEPatcher.h"
#include "ScanForFilesDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CScanForFilesDlg dialog


CScanForFilesDlg::CScanForFilesDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CScanForFilesDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CScanForFilesDlg)
	m_sFilePattern = _T("");
	//}}AFX_DATA_INIT
}


void CScanForFilesDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CScanForFilesDlg)
	DDX_Text(pDX, IDC_FILEPATTERN, m_sFilePattern);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CScanForFilesDlg, CDialog)
	//{{AFX_MSG_MAP(CScanForFilesDlg)
	ON_BN_CLICKED(IDC_SELECTFILE, OnSelectFile)
	ON_BN_CLICKED(IDC_SELECTFOLDER, OnSelectFolder)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CScanForFilesDlg message handlers

void CScanForFilesDlg::OnOK() 
{
	UpdateData();
	CDialog::OnOK();
}

void CScanForFilesDlg::OnSelectFile() 
{
	CFileDialog dlg(TRUE, NULL, _T("*.*"));
	
	if (dlg.DoModal() == IDOK)
	{
		m_sFilePattern = dlg.GetPathName();
		UpdateData(FALSE);
	}
}

void CScanForFilesDlg::OnSelectFolder() 
{
	BROWSEINFO bi;
	TCHAR szFolder[MAX_PATH + 1];

	memset(szFolder, 0, sizeof(szFolder));

	memset(&bi, 0, sizeof(bi));
	bi.hwndOwner = GetSafeHwnd();
	bi.pidlRoot = NULL;
	bi.pszDisplayName = szFolder;
	bi.lpszTitle = _T("Pick a folder to scan");

	// Open the folder browser dialog
	LPITEMIDLIST pItemList = SHBrowseForFolder(&bi);
	if (pItemList)
	{
		IMalloc *pShellAllocator = NULL;
		
		SHGetMalloc(&pShellAllocator);
		if (pShellAllocator)
		{
			char szPath[MAX_PATH + 1];

			if (SHGetPathFromIDList(pItemList, szPath))
			{
				// Chop off the end path separator
				int nPathSize = strlen(szPath);
				if (nPathSize > 0)
				{
					if (szPath[nPathSize - 1] == '\\')
					{
						szPath[nPathSize - 1] = '\0';
					}
				}

				// Form the file pattern
				USES_CONVERSION;
				m_sFilePattern.Format(_T("%s\\*.*"), A2T(szPath));
				UpdateData(FALSE);
			}

			pShellAllocator->Free(pItemList);
			pShellAllocator->Release();
		}

	}
}
