// RegMozCtlDlg.cpp : implementation file
//

#include "stdafx.h"
#include "RegMozCtl.h"
#include "RegMozCtlDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define MOZ_CONTROL_REG_KEY                  _T("Software\\Mozilla\\")
#define MOZ_CONTROL_REG_VALUE_DIR            _T("Dir")
#define MOZ_CONTROL_REG_VALUE_COMPONENT_PATH _T("ComponentPath")
#define MOZ_CONTROL_REG_VALUE_COMPONENT_FILE _T("ComponentFile")

/////////////////////////////////////////////////////////////////////////////
// CRegMozCtlDlg dialog

CRegMozCtlDlg::CRegMozCtlDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CRegMozCtlDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CRegMozCtlDlg)
	m_szMozillaDir = _T("");
	m_szComponentPath = _T("");
	m_szComponentFile = _T("");
	m_bAutomatic = FALSE;
	//}}AFX_DATA_INIT
	// Note that LoadIcon does not require a subsequent DestroyIcon in Win32
	m_hIcon = AfxGetApp()->LoadIcon(IDI_MOZILLA);

	m_bAutomatic = TRUE;
	GetCurrentDirectory(1024, m_szMozillaDir.GetBuffer(1024));
	m_szMozillaDir.ReleaseBuffer();
}

void CRegMozCtlDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CRegMozCtlDlg)
	DDX_Control(pDX, IDC_COMPONENTPATH, m_edtComponentPath);
	DDX_Control(pDX, IDC_COMPONENTFILE, m_edtComponentFile);
	DDX_Control(pDX, IDC_PICKCOMPONENTPATH, m_btnPickComponentPath);
	DDX_Control(pDX, IDC_PICKCOMPONENTFILE, m_btnPickComponentFile);
	DDX_Text(pDX, IDC_MOZILLADIR, m_szMozillaDir);
	DDX_Text(pDX, IDC_COMPONENTPATH, m_szComponentPath);
	DDX_Text(pDX, IDC_COMPONENTFILE, m_szComponentFile);
	DDX_Check(pDX, IDC_AUTOMATIC, m_bAutomatic);
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CRegMozCtlDlg, CDialog)
	//{{AFX_MSG_MAP(CRegMozCtlDlg)
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDC_REGISTER, OnRegister)
	ON_BN_CLICKED(IDC_UNREGISTER, OnUnregister)
	ON_BN_CLICKED(IDC_PICKDIR, OnPickDir)
	ON_BN_CLICKED(IDC_PICKCOMPONENTFILE, OnPickComponentFile)
	ON_BN_CLICKED(IDC_PICKCOMPONENTPATH, OnPickComponentPath)
	ON_BN_CLICKED(IDC_AUTOMATIC, OnAutomatic)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CRegMozCtlDlg message handlers

BOOL CRegMozCtlDlg::OnInitDialog()
{
	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon
	
	// Get values from the registry
	TCHAR szValue[1024];
	DWORD dwSize = sizeof(szValue) / sizeof(szValue[0]);
	CRegKey cKey;
	cKey.Create(HKEY_LOCAL_MACHINE, MOZ_CONTROL_REG_KEY);
	
	memset(szValue, 0, sizeof(szValue));
	if (cKey.QueryValue(szValue, MOZ_CONTROL_REG_VALUE_COMPONENT_PATH, &dwSize) == ERROR_SUCCESS)
	{
		m_szComponentPath = CString(szValue);
	}

	dwSize = sizeof(szValue) / sizeof(szValue[0]);
	memset(szValue, 0, sizeof(szValue));
	if (cKey.QueryValue(szValue, MOZ_CONTROL_REG_VALUE_COMPONENT_FILE, &dwSize) == ERROR_SUCCESS)
	{
		m_szComponentFile = CString(szValue);
	}

	dwSize = sizeof(szValue) / sizeof(szValue[0]);
	memset(szValue, 0, sizeof(szValue));
	if (cKey.QueryValue(szValue, MOZ_CONTROL_REG_VALUE_DIR, &dwSize) == ERROR_SUCCESS)
	{
		m_szMozillaDir = CString(szValue);
	}
	
	CDialog::OnInitDialog();

	return TRUE;  // return TRUE  unless you set the focus to a control
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CRegMozCtlDlg::OnPaint() 
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
HCURSOR CRegMozCtlDlg::OnQueryDragIcon()
{
	return (HCURSOR) m_hIcon;
}

void CRegMozCtlDlg::OnRegister() 
{
	RegisterMozillaControl(TRUE);
}

void CRegMozCtlDlg::OnUnregister() 
{
	RegisterMozillaControl(FALSE);
}

void CRegMozCtlDlg::RegisterMozillaControl(BOOL bRegister)
{
	UpdateData();

	CFileFind cFind;
	CString szFile;
	CString szPath;

	SetCurrentDirectory(m_szMozillaDir);

	CRegKey cKey;
	if (cKey.Open(HKEY_LOCAL_MACHINE, _T("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\SharedDlls")) != ERROR_SUCCESS)
	{
		AfxMessageBox(_T("Can't open registry key \"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\SharedDlls\""));
		return;
	}

	// Iterate through directory registering each DLL as a shared DLL
	BOOL bWorking = cFind.FindFile("*.dll");
	while (bWorking)
	{
		bWorking = cFind.FindNextFile();
		szFile = cFind.GetFileName();
		szPath = m_szMozillaDir + CString(_T("\\")) + szFile;
		if (bRegister)
		{
			cKey.SetValue(1, szPath);
		}
		else
		{
			cKey.DeleteValue(szPath);
		}
	}
	cKey.Close();

	cKey.Create(HKEY_LOCAL_MACHINE, MOZ_CONTROL_REG_KEY);
	cKey.SetValue(m_szMozillaDir, MOZ_CONTROL_REG_VALUE_DIR);
	cKey.Close();

	// Now register the mozilla control
	CString szMozCtl = m_szMozillaDir + CString(_T("\\npmozctl.dll"));
	HINSTANCE hMod = LoadLibrary(szMozCtl);
	if (hMod == NULL)
	{
		AfxMessageBox(_T("Can't find npmozctl.dll in current directory"));
	}
	FARPROC pfn = GetProcAddress(hMod, bRegister ? _T("DllRegisterServer") : _T("DllUnregisterServer"));
	if (pfn)
	{
		pfn();
	}
	FreeLibrary(hMod);

	AfxMessageBox(bRegister ? _T("Register completed") : _T("Unregister completed"));
}

void CRegMozCtlDlg::OnPickDir() 
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

				m_szMozillaDir = CString(szPath);
				if (m_bAutomatic)
				{
					m_szComponentPath = m_szMozillaDir + "\\components";
					m_szComponentFile = m_szMozillaDir + "\\component.reg";
				}

				UpdateData(FALSE);
			}

			pShellAllocator->Free(pItemList);
			pShellAllocator->Release();
		}
	}
}

void CRegMozCtlDlg::OnPickComponentFile() 
{
	CFileDialog dlg(TRUE, NULL, m_szComponentFile);
	if (dlg.DoModal() == IDOK)
	{
		m_szComponentFile = dlg.GetPathName();
		UpdateData(FALSE);
	}
}

void CRegMozCtlDlg::OnPickComponentPath() 
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

				m_szComponentPath = CString(szPath);
				UpdateData(FALSE);
			}

			pShellAllocator->Free(pItemList);
			pShellAllocator->Release();
		}
	}
}

void CRegMozCtlDlg::OnAutomatic() 
{
	UpdateData(TRUE);
	m_edtComponentFile.EnableWindow(!m_bAutomatic);
	m_btnPickComponentFile.EnableWindow(!m_bAutomatic);
	m_edtComponentPath.EnableWindow(!m_bAutomatic);
	m_btnPickComponentPath.EnableWindow(!m_bAutomatic);
}

CString CRegMozCtlDlg::GetSystemPath()
{
	// TODO
	return _T("");
}

BOOL CRegMozCtlDlg::SetSystemPath(const CString &szNewPath)
{
	// TODO
	return TRUE;
}
