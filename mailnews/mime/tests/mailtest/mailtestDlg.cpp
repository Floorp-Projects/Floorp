// mailtestDlg.cpp : implementation file
//
#include "stdafx.h"
#include "mailtest.h"
#include "mailtestDlg.h"
#include "ExDisp.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

//
// Necessary variables...
//
const IID IID_IWebBrowser = {0xEAB22AC1,0x30C1,0x11CF,{0xA7,0xEB,0x00,0x00,0xC0,0x5B,0xAE,0x0B}};
static const CLSID clsidMozilla = { 0x1339B54C, 0x3453, 0x11D2, { 0x93, 0xB9, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } };

/////////////////////////////////////////////////////////////////////////////
// CAboutDlg dialog used for App About

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
// CMailtestDlg dialog

CMailtestDlg::CMailtestDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CMailtestDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CMailtestDlg)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
	// Note that LoadIcon does not require a subsequent DestroyIcon in Win32
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);

	// rhp stuff here
	m_pWndBrowser = NULL;
}

void CMailtestDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CMailtestDlg)
		// NOTE: the ClassWizard will add DDX and DDV calls here
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CMailtestDlg, CDialog)
	//{{AFX_MSG_MAP(CMailtestDlg)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_LBN_DBLCLK(IDC_HEADERS, OnDblclkHeaders)
	ON_WM_CLOSE()
	ON_WM_SIZE()
	ON_COMMAND(ID_ABOUT_MENU, OnAbout)
	ON_COMMAND(ID_HELP_MENU, OnHelp)
  ON_COMMAND(ID_EXIT_MENU, OnExit)
	ON_COMMAND(ID_OPEN_MENU, OnOpen)
	ON_BN_CLICKED(IDC_BUTTON1, OnLoadURL)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CMailtestDlg message handlers

BOOL CMailtestDlg::OnInitDialog()
{
void    FixURL(char *tmpBuf);

	CDialog::OnInitDialog();

	// Add "About..." menu item to system menu.

	// IDM_ABOUTBOX must be in the system command range.
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);
 
	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != NULL)
	{
		CString strAboutMenu;
		strAboutMenu.LoadString(IDS_ABOUTBOX);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon
	
	// TODO: Add extra initialization here
	// rhp Stuff here
	// Create the control for the web browser
	// Get the position of the browser marker
	CRect rcMarker;
	GetDlgItem(IDC_BROWSER_MARKER)->GetWindowRect(&rcMarker);
	rcMarker.DeflateRect(2, 2);
	ScreenToClient(rcMarker);

	//
	// Initialization code...
	//
	m_tempFileCount = 0;
	m_pWndBrowser = new CWnd;
	m_pWndBrowser->CreateControl(clsidMozilla, _T("Mail"), WS_VISIBLE, rcMarker, this, 1000);

  OnHelp();

  // Load the sample mailbox...
  char    dirName[_MAX_PATH] = "";
  char    url[_MAX_PATH] = "";

  GetModuleFileName(GetModuleHandle(NULL), dirName, sizeof(dirName));
  char *ptr = strrchr(dirName, '\\');
  if (ptr) *ptr = '\0';
  if (dirName[0] != '\0')
  {
    sprintf(url, "%s\\Mailbox", dirName);
    ProcessMailbox(url);
  }

  return TRUE;  // return TRUE  unless you set the focus to a control
}

void CMailtestDlg::OnSysCommand(UINT nID, LPARAM lParam)
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

void CMailtestDlg::OnPaint() 
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
HCURSOR CMailtestDlg::OnQueryDragIcon()
{
	return (HCURSOR) m_hIcon;
}


// 
// This is a call to load a URL (one of the temp files)
//
void CMailtestDlg::LoadURL(CString urlString) 
{
	UpdateData();

	// If there's a browser control, use it to navigate to the specified URL

	if (m_pWndBrowser)
	{
		IUnknown *pIUnkBrowser = m_pWndBrowser->GetControlUnknown();
		if (pIUnkBrowser)
		{
			IWebBrowser *pIWebBrowser = NULL;
			pIUnkBrowser->QueryInterface(IID_IWebBrowser, (void **) &pIWebBrowser);
			if (pIWebBrowser)
			{
				BSTR bstrURL = urlString.AllocSysString();
				pIWebBrowser->Navigate(bstrURL, NULL, NULL, NULL, NULL);
				::SysFreeString(bstrURL);
				pIWebBrowser->Release();
			}
		}
	}
}

void CMailtestDlg::OnAbout()
{
	CAboutDlg dlgAbout;
	dlgAbout.DoModal();
}

void CMailtestDlg::OnExit()
{
	OnClose();
}

void CMailtestDlg::OnOpen()
{
	CFileDialog		fileDlgOpen(TRUE, // TRUE for FileOpen, FALSE for FileSaveAs
		NULL, NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, "*.*||", this);

	if (fileDlgOpen.DoModal() == IDOK)
	{
		CString url = fileDlgOpen.GetPathName();
		ProcessMailbox(url);
	}

	delete fileDlgOpen;
}

CString
GetTempDirName(void)
{
static CString tmpDirName;

	if (getenv("TEMP"))
		tmpDirName = getenv("TEMP");
	else if (getenv("TMP"))
		tmpDirName = getenv("TMP");
	return tmpDirName;
}

void
FixURL(char *tmpBuf)
{
  // Translate '\' to '/'
  for (unsigned int i = 0; i < strlen(tmpBuf); i++) {
    if (tmpBuf[i] == '\\') {
      tmpBuf[i] = '/';
    }
  }
}

void CMailtestDlg::OnDblclkHeaders() 
{
	//
	// TODO: Add your control notification handler code here	
	//
	char msg[512] = "";
	char url[512] = "";

	int	sel = GetDlgItem(IDC_HEADERS)->SendMessage(LB_GETCURSEL);
	GetDlgItem(IDC_HEADERS)->SendMessage(LB_GETTEXT, sel, (long) msg);

	if (msg[0] == '\0')
		return;

  CString tmpFileName;
  tmpFileName.Format("file://%s/RFC822-%d.eml", GetTempDirName(), sel+1);

  FixURL(tmpFileName.GetBuffer(0));
  LoadURL(tmpFileName);
}

void CMailtestDlg::OnClose() 
{
	// TODO: Add your message handler code here and/or call default
	if (m_pWndBrowser)
	{
		delete m_pWndBrowser;
		m_pWndBrowser = NULL;
	}

	CleanupMailbox();
	PostMessage(WM_CLOSE);
	CDialog::OnClose();
}

void CMailtestDlg::CleanupMailbox() 
{
	int	i;
	CString tmpDirName = GetTempDirName() + "\\RFC822-";
	CString tmpFileName;

	for (i=1; i<=m_tempFileCount; i++)
	{
		tmpFileName.Format("%s%d.eml", tmpDirName, i);
		unlink(tmpFileName.GetBuffer(0));
	}
}

void CMailtestDlg::ProcessMailbox(CString mBoxName) 
{
	char		nextLine[512];
	char		*compString = "From - ";
	CString		tmpDirName;
	CString		tmpFileName;
	CString		subjectLine = "Subject";
	CStdioFile 	mboxFile(mBoxName, CFile::modeRead | CFile::typeText ); 
	CStdioFile 	newMailFile;

	// Make sure the file opened...
	if (!mboxFile)
		return;

//	CMailtestApp::DoMessageBox("Unable to locate the systems TEMP directory.", MB_OK | MB_APPLMODAL | MB_ICONEXCLAMATION, NULL);

	// Clear out the inbox
	GetDlgItem(IDC_HEADERS)->SendMessage(LB_RESETCONTENT, 0, 0);	
	CleanupMailbox();
	m_tempFileCount = 0;

	tmpDirName = GetTempDirName() + "\\RFC822-";
	while (mboxFile.ReadString( nextLine, sizeof(nextLine) ) != NULL)
	{
		// If this is true...new message!
		if (strncmp(nextLine, compString, strlen(compString)) == 0)
		{
			if (newMailFile.m_pStream)
				newMailFile.Close();

			if (m_tempFileCount != 0)
				GetDlgItem(IDC_HEADERS)->SendMessage(LB_INSERTSTRING, (m_tempFileCount-1), (long) subjectLine.GetBuffer(0));

			++m_tempFileCount;
			tmpFileName.Format("%s%d.eml", tmpDirName, m_tempFileCount);
			newMailFile.Open(tmpFileName, CFile::modeCreate | CFile::modeWrite | CFile::typeText );
			if (!newMailFile.m_pStream)
				break;

			continue;
		}

		// If we are here, just check for a subject and output the line
		if (strncmp(nextLine, "Subject: ", 9) == 0)
		{
			subjectLine = nextLine + 9;
			subjectLine.TrimRight();
		}

		newMailFile.WriteString(nextLine);
	}

	if (newMailFile.m_pStream)
	{
		GetDlgItem(IDC_HEADERS)->SendMessage(LB_INSERTSTRING, (m_tempFileCount-1), (long) subjectLine.GetBuffer(0));
		newMailFile.Close();
	}

	mboxFile.Close();
}


void CMailtestDlg::OnSize(UINT nType, int cx, int cy) 
{
	CDialog::OnSize(nType, cx, cy);
	
	// TODO: Add your message handler code here
  if (m_pWndBrowser)
  {
    CRect rcMarker;
    m_pWndBrowser->GetWindowRect(&rcMarker);
    ScreenToClient(rcMarker);

    m_pWndBrowser->MoveWindow( rcMarker.TopLeft().x, rcMarker.TopLeft().y, 
                   cx-16, cy-rcMarker.TopLeft().y - 8, TRUE);
    GetDlgItem(IDC_BROWSER_MARKER)->MoveWindow(rcMarker.TopLeft().x, 
          rcMarker.TopLeft().y, cx-16, cy-rcMarker.TopLeft().y - 8, TRUE );

    GetDlgItem(IDC_HEADERS)->GetWindowRect(&rcMarker);
    ScreenToClient(rcMarker);
    GetDlgItem(IDC_HEADERS)->MoveWindow(rcMarker.TopLeft().x, 
      rcMarker.TopLeft().y, cx-16, rcMarker.Height());
  }

}

void 
CMailtestDlg::OnHelp(void)
{
char    dirName[_MAX_PATH] = "";
char    url[_MAX_PATH] = "";

  GetModuleFileName(GetModuleHandle(NULL), dirName, sizeof(dirName));
  char *ptr = strrchr(dirName, '\\');
  if (ptr) *ptr = '\0';
  if (dirName[0] == '\0')
    LoadURL("http://messenger.netscape.com/bookmark/4_5/messengerstart.html");
  else
  {
    sprintf(url, "file://%s\\about.html", dirName);
    FixURL(url);
    LoadURL(url);
  }
}

void CMailtestDlg::OnLoadURL() 
{
  char    url[256] = "";

	// TODO: Add your control notification handler code here
  GetDlgItemText( IDC_URL , url, sizeof(url));
  if (url[0] != '\0')
    LoadURL(url);
}
