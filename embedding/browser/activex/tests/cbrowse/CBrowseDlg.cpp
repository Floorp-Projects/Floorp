// CBrowseDlg.cpp : implementation file
//

#include "stdafx.h"

#include "cbrowse.h"
#include "CBrowseDlg.h"
#include "ControlEventSink.h"
#include "..\..\src\control\DHTMLCmdIds.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#include <math.h>
#include <fpieee.h>
#include <float.h>

#include <signal.h>

#include <stack>

void __cdecl fperr(int sig)
{
	CString sError;
	sError.Format("FP Error %08x", sig);
	AfxMessageBox(sError);
}

TCHAR *aURLs[] =
{
	_T("http://whippy/calendar.html"),
	_T("http://www.mozilla.org"),
	_T("http://www.yahoo.com"),
	_T("http://www.netscape.com"),
	_T("http://www.microsoft.com")
};

CBrowseDlg *CBrowseDlg::m_pBrowseDlg = NULL;

/////////////////////////////////////////////////////////////////////////////
// CBrowseDlg dialog

CBrowseDlg::CBrowseDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CBrowseDlg::IDD, pParent)
{
	signal(SIGFPE, fperr);
	double x = 0.0;
	double y = 1.0/x;

	//{{AFX_DATA_INIT(CBrowseDlg)
	m_bNewWindow = FALSE;
	//}}AFX_DATA_INIT
	// Note that LoadIcon does not require a subsequent DestroyIcon in Win32
	m_pBrowseDlg = this;
	m_pControlSite = NULL;
	m_clsid = CLSID_NULL;
	m_bUseCustomPopupMenu = FALSE;
	m_bUseCustomDropTarget = FALSE;
}

void CBrowseDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CBrowseDlg)
	DDX_Control(pDX, IDC_EDITMODE, m_btnEditMode);
	DDX_Control(pDX, IDC_URL, m_cmbURLs);
	DDX_Check(pDX, IDC_NEWWINDOW, m_bNewWindow);
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CBrowseDlg, CDialog)
	//{{AFX_MSG_MAP(CBrowseDlg)
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDC_GO, OnGo)
	ON_BN_CLICKED(IDC_BACKWARD, OnBackward)
	ON_BN_CLICKED(IDC_FORWARD, OnForward)
	ON_WM_CLOSE()
	ON_WM_DESTROY()
	ON_WM_SIZE()
	ON_BN_CLICKED(IDC_EDITMODE, OnEditMode)
	ON_COMMAND(ID_FILE_EXIT, OnFileExit)
	ON_COMMAND(ID_VIEW_GOTO_BACK, OnViewGotoBack)
	ON_COMMAND(ID_VIEW_GOTO_FORWARD, OnViewGotoForward)
	ON_COMMAND(ID_VIEW_GOTO_HOME, OnViewGotoHome)
	ON_COMMAND(ID_EDIT_COPY, OnEditCopy)
	ON_COMMAND(ID_EDIT_CUT, OnEditCut)
	ON_COMMAND(ID_EDIT_PASTE, OnEditPaste)
	ON_COMMAND(ID_HELP_ABOUT, OnHelpAbout)
	ON_UPDATE_COMMAND_UI(ID_VIEW_GOTO_BACK, OnUpdateViewGotoBack)
	ON_UPDATE_COMMAND_UI(ID_VIEW_GOTO_FORWARD, OnUpdateViewGotoForward)
	ON_COMMAND(ID_EDIT_SELECTALL, OnEditSelectAll)
	ON_COMMAND(ID_VIEW_REFRESH, OnViewRefresh)
	ON_COMMAND(ID_VIEW_VIEWSOURCE, OnViewViewSource)
	ON_BN_CLICKED(IDC_STOP, OnStop)
	ON_COMMAND(ID_FILE_SAVEAS, OnFileSaveAs)
	ON_COMMAND(ID_FILE_PRINT, OnFilePrint)
	ON_COMMAND(ID_DEBUG_VISIBLE, OnDebugVisible)
	ON_UPDATE_COMMAND_UI(ID_DEBUG_VISIBLE, OnUpdateDebugVisible)
	//}}AFX_MSG_MAP
	ON_COMMAND(IDB_BOLD, OnEditBold)
	ON_COMMAND(IDB_ITALIC, OnEditItalic)
	ON_COMMAND(IDB_UNDERLINE, OnEditUnderline)
END_MESSAGE_MAP()

#define IL_CLOSEDFOLDER	0
#define IL_OPENFOLDER	1
#define IL_TEST			2
#define IL_TESTFAILED	3
#define IL_TESTPASSED	4
#define IL_TESTPARTIAL  5
#define IL_NODE			IL_TEST

/////////////////////////////////////////////////////////////////////////////
// CBrowseDlg message handlers

BOOL CBrowseDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	CWinApp *pApp = AfxGetApp();
	m_szTestURL = pApp->GetProfileString(SECTION_TEST, KEY_TESTURL, KEY_TESTURL_DEFAULTVALUE);
	m_szTestCGI = pApp->GetProfileString(SECTION_TEST, KEY_TESTCGI, KEY_TESTCGI_DEFAULTVALUE);

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon

	CRect rcTabMarker;
	GetDlgItem(IDC_TAB_MARKER)->GetWindowRect(&rcTabMarker);
	ScreenToClient(rcTabMarker);

    m_dlgPropSheet.AddPage(&m_TabMessages);
    m_dlgPropSheet.AddPage(&m_TabTests);
    m_dlgPropSheet.AddPage(&m_TabDOM);

	m_TabMessages.m_pBrowseDlg = this;
	m_TabTests.m_pBrowseDlg = this;
	m_TabDOM.m_pBrowseDlg = this;

    m_dlgPropSheet.Create(this, WS_CHILD | WS_VISIBLE, 0);
    m_dlgPropSheet.ModifyStyleEx (0, WS_EX_CONTROLPARENT);
    m_dlgPropSheet.ModifyStyle( 0, WS_TABSTOP );
    m_dlgPropSheet.SetWindowPos( NULL, rcTabMarker.left-7, rcTabMarker.top-7, 0, 0, SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE );

	// Image list
	m_cImageList.Create(16, 16, ILC_COLOR | ILC_MASK, 0, 10);
	m_cImageList.Add(AfxGetApp()->LoadIcon(IDI_CLOSEDFOLDER));
	m_cImageList.Add(AfxGetApp()->LoadIcon(IDI_OPENFOLDER));
	m_cImageList.Add(AfxGetApp()->LoadIcon(IDI_TEST));
	m_cImageList.Add(AfxGetApp()->LoadIcon(IDI_TESTFAILED));
	m_cImageList.Add(AfxGetApp()->LoadIcon(IDI_TESTPASSED));
	m_cImageList.Add(AfxGetApp()->LoadIcon(IDI_TESTPARTIAL));

	// Set up the editor bar
	CRect rcEditBarMarker;
	GetDlgItem(IDC_EDITBAR_MARKER)->GetWindowRect(&rcEditBarMarker);
	ScreenToClient(rcEditBarMarker);
	GetDlgItem(IDC_EDITBAR_MARKER)->DestroyWindow();

	m_EditBar.Create(this);
	m_EditBar.LoadToolBar(IDR_DHTMLEDIT);
	m_EditBar.SetWindowPos(&wndTop, rcEditBarMarker.left, rcEditBarMarker.top,
		rcEditBarMarker.Width(), rcEditBarMarker.Height(), SWP_SHOWWINDOW);

	// Set up some URLs. The first couple are internal
	m_cmbURLs.AddString(m_szTestURL);
	for (int i = 0; i < sizeof(aURLs) / sizeof(aURLs[0]); i++)
	{
		m_cmbURLs.AddString(aURLs[i]);
	}
	m_cmbURLs.SetCurSel(0);

	// Create the contained web browser
	CreateWebBrowser();

	// Load the menu
	m_menu.LoadMenu(IDR_MAIN);
	SetMenu(&m_menu);
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);

	return TRUE;  // return TRUE  unless you set the focus to a control
}


// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CBrowseDlg::OnPaint() 
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
		CPaintDC dc(this);

		m_pControlSite->Draw(dc.m_hDC);
	}
}

// The system calls this to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CBrowseDlg::OnQueryDragIcon()
{
	return (HCURSOR) m_hIcon;
}

struct EnumData
{
	CBrowseDlg *pBrowseDlg;
	CSize sizeDelta;
};

BOOL CALLBACK EnumChildProc(HWND hwnd, LPARAM lParam)
{
	EnumData *pData = (EnumData *) lParam;
	CBrowseDlg *pThis = pData->pBrowseDlg;

	switch (::GetDlgCtrlID(hwnd))
	{
	case IDC_BROWSER_MARKER:
		{
			CWnd *pMarker = pThis->GetDlgItem(IDC_BROWSER_MARKER);
			CRect rcMarker;
			pMarker->GetWindowRect(&rcMarker);
			pThis->ScreenToClient(rcMarker);

			rcMarker.right += pData->sizeDelta.cx;
			rcMarker.bottom += pData->sizeDelta.cy;

			if (rcMarker.Width() > 10 && rcMarker.Height() > 10)
			{
				pMarker->SetWindowPos(&CWnd::wndBottom, 0, 0, rcMarker.Width(), rcMarker.Height(), 
						SWP_NOMOVE | SWP_NOACTIVATE | SWP_HIDEWINDOW);
				pThis->m_pControlSite->SetPosition(rcMarker);
			}
		}
		break;
	case IDC_TAB_MARKER:
		{
			CWnd *pMarker = pThis->GetDlgItem(IDC_TAB_MARKER);
			CRect rcMarker;
			pMarker->GetWindowRect(&rcMarker);
			pThis->ScreenToClient(rcMarker);

			rcMarker.top += pData->sizeDelta.cy;

			if (rcMarker.top > 70)
			{
				pMarker->SetWindowPos(&CWnd::wndBottom, rcMarker.left, rcMarker.top, 0, 0, 
						SWP_NOSIZE | SWP_NOACTIVATE | SWP_HIDEWINDOW);
				pThis->m_dlgPropSheet.SetWindowPos(NULL, rcMarker.left - 7, rcMarker.top - 7, 0, 0, 
						SWP_NOREPOSITION | SWP_NOSIZE | SWP_NOACTIVATE);
			}
		}

	}

	return TRUE;
}

void CBrowseDlg::OnSize(UINT nType, int cx, int cy) 
{
	CDialog::OnSize(nType, cx, cy);

	if (m_hWnd == NULL)
	{
		return;
	}

	static CSize sizeOld(-1, -1);
	CSize sizeNew(cx, cy);

	if (sizeOld.cx != -1)
	{
		EnumData data;
		data.pBrowseDlg = this;
		data.sizeDelta = sizeNew - sizeOld;
		::EnumChildWindows(GetSafeHwnd(), EnumChildProc, (LPARAM) &data);
	}
	sizeOld = sizeNew;
}

HRESULT CBrowseDlg::CreateWebBrowser()
{
	// Get the position of the browser marker
	CRect rcMarker;
	GetDlgItem(IDC_BROWSER_MARKER)->GetWindowRect(&rcMarker);
	ScreenToClient(rcMarker);
	GetDlgItem(IDC_BROWSER_MARKER)->ShowWindow(FALSE);

//	GetDlgItem(IDC_BROWSER_MARKER)->DestroyWindow();

	CBrowserCtlSiteInstance::CreateInstance(&m_pControlSite);
	if (m_pControlSite == NULL)
	{
		OutputString(_T("Error: could not create control site"));
		return E_OUTOFMEMORY;
	}

	CControlEventSinkInstance *pEventSink = NULL;
	CControlEventSinkInstance::CreateInstance(&pEventSink);
	if (pEventSink == NULL)
	{
		m_pControlSite->Release();
		m_pControlSite = NULL;
		OutputString(_T("Error: could not create event sink"));
		return E_OUTOFMEMORY;
	}
	pEventSink->m_pBrowseDlg = this;

	HRESULT hr;

	PropertyList pl;
	m_pControlSite->AddRef();
	m_pControlSite->m_bUseCustomPopupMenu = m_bUseCustomPopupMenu;
	m_pControlSite->m_bUseCustomDropTarget = m_bUseCustomDropTarget;
	m_pControlSite->Create(m_clsid, pl);
	hr = m_pControlSite->Attach(GetSafeHwnd(), rcMarker, NULL);
	if (hr != S_OK)
	{
		OutputString(_T("Error: Cannot attach to browser control, hr = 0x%08x"), hr);
	}
	else
	{
		OutputString(_T("Sucessfully attached to browser control"));
	}
	
	m_pControlSite->SetPosition(rcMarker);
	hr = m_pControlSite->Advise(pEventSink, DIID_DWebBrowserEvents2, &m_dwCookie);
	if (hr != S_OK)
	{
		OutputString(_T("Error: Cannot subscribe to DIID_DWebBrowserEvents2 events, hr = 0x%08x"), hr);
	}
	else
	{
		OutputString(_T("Sucessfully subscribed to events"));
	}

	CComPtr<IUnknown> spUnkBrowser;
	m_pControlSite->GetControlUnknown(&spUnkBrowser);

	CIPtr(IWebBrowser2) spWebBrowser = spUnkBrowser;
	if (spWebBrowser)
	{
		spWebBrowser->put_RegisterAsDropTarget(VARIANT_TRUE);
	}

	return S_OK;
}


HRESULT CBrowseDlg::DestroyWebBrowser()
{
	if (m_pControlSite)
	{
		m_pControlSite->Unadvise(DIID_DWebBrowserEvents2, m_dwCookie);
		m_pControlSite->Detach();
		m_pControlSite->Release();
		m_pControlSite = NULL;
	}

	return S_OK;
}

void CBrowseDlg::PopulateTests()
{
	// Create the test tree
	CTreeCtrl &tc = m_TabTests.m_tcTests;

	tc.SetImageList(&m_cImageList, TVSIL_NORMAL);
	for (int i = 0; i < nTestSets; i++)
	{
		TestSet *pTestSet = &aTestSets[i];
		HTREEITEM hParent = tc.InsertItem(pTestSet->szName, IL_CLOSEDFOLDER, IL_CLOSEDFOLDER);
		m_TabTests.m_tcTests.SetItemData(hParent, (DWORD) pTestSet);

		if (pTestSet->pfnPopulator)
		{
			pTestSet->pfnPopulator(pTestSet);
		}

		for (int j = 0; j < pTestSet->nTests; j++)
		{
			Test *pTest = &pTestSet->aTests[j];
			HTREEITEM hTest = tc.InsertItem(pTest->szName, IL_TEST, IL_TEST, hParent);
			if (hTest)
			{
				tc.SetItemData(hTest, (DWORD) pTest);
			}
		}
	}
}


void CBrowseDlg::UpdateURL()
{
    CIPtr(IWebBrowser) spBrowser;

    GetWebBrowser(&spBrowser);
    if (spBrowser)
    {
        USES_CONVERSION;
        BSTR szLocation = NULL;
        spBrowser->get_LocationURL(&szLocation);
        m_cmbURLs.SetWindowText(W2T(szLocation));
        SysFreeString(szLocation);
    }
}


HRESULT CBrowseDlg::GetWebBrowser(IWebBrowser **pWebBrowser)
{
	if (pWebBrowser == NULL)
	{
		return E_INVALIDARG;
	}

	*pWebBrowser = NULL;

	if (m_pControlSite)
	{
		IUnknown *pIUnkBrowser = NULL;
		m_pControlSite->GetControlUnknown(&pIUnkBrowser);
		if (pIUnkBrowser)
		{
			pIUnkBrowser->QueryInterface(IID_IWebBrowser, (void **) pWebBrowser);
			pIUnkBrowser->Release();
			if (*pWebBrowser)
			{
				return S_OK;
			}
		}
	}

	return E_FAIL;
}

void CBrowseDlg::OnGo() 
{
	UpdateData();

	IWebBrowser *pIWebBrowser = NULL;
	if (SUCCEEDED(GetWebBrowser(&pIWebBrowser)))
	{
		CString szURL;
		m_cmbURLs.GetWindowText(szURL);
		CComVariant vFlags(m_bNewWindow ? navOpenInNewWindow : 0);
		BSTR bstrURL = szURL.AllocSysString();
		HRESULT hr = pIWebBrowser->Navigate(bstrURL, &vFlags, NULL, NULL, NULL);
        if (FAILED(hr))
        {
            OutputString("Navigate failed (hr=0x%08x)", hr);
        }
		::SysFreeString(bstrURL);
		pIWebBrowser->Release();
	}
}


void CBrowseDlg::OnStop() 
{
	IWebBrowser *pIWebBrowser = NULL;
	if (SUCCEEDED(GetWebBrowser(&pIWebBrowser)))
	{
		pIWebBrowser->Stop();
		pIWebBrowser->Release();
	}
}


void CBrowseDlg::OnBackward() 
{
	IWebBrowser *pIWebBrowser = NULL;
	if (SUCCEEDED(GetWebBrowser(&pIWebBrowser)))
	{
		pIWebBrowser->GoBack();
		pIWebBrowser->Release();
	}
}

void CBrowseDlg::OnForward() 
{
	IWebBrowser *pIWebBrowser = NULL;
	if (SUCCEEDED(GetWebBrowser(&pIWebBrowser)))
	{
		pIWebBrowser->GoForward();
		pIWebBrowser->Release();
	}
}

void CBrowseDlg::RunTestSet(TestSet *pTestSet)
{
	ASSERT(pTestSet);
	if (pTestSet == NULL)
	{
		return;
	}

	for (int j = 0; j < pTestSet->nTests; j++)
	{
		Test *pTest = &pTestSet->aTests[j];
		RunTest(pTest);
	}
}


TestResult CBrowseDlg::RunTest(Test *pTest)
{
	ASSERT(pTest);
	TestResult nResult = trFailed;

	CString szMsg;
	szMsg.Format(_T("Running test \"%s\""), pTest->szName);
	OutputString(szMsg);

	if (pTest && pTest->pfn)
	{
		BrowserInfo cInfo;

		cInfo.pTest = pTest;
		cInfo.clsid = m_clsid;
		cInfo.pControlSite = m_pControlSite;
		cInfo.pIUnknown = NULL;
		cInfo.pBrowseDlg = this;
		cInfo.szTestURL = m_szTestURL;
		cInfo.szTestCGI = m_szTestCGI;
		if (cInfo.pControlSite)
		{
			cInfo.pControlSite->GetControlUnknown(&cInfo.pIUnknown);
		}
		nResult = pTest->pfn(cInfo);
		pTest->nLastResult = nResult;
		if (cInfo.pIUnknown)
		{
			cInfo.pIUnknown->Release();
		}
	}

	switch (nResult)
	{
	case trFailed:
		OutputString(_T("Test failed"));
		break;
	case trPassed:
		OutputString(_T("Test passed"));
		break;
	case trPartial:
		OutputString(_T("Test partial"));
		break;
	default:
		break;
	}

	return nResult;
}

void CBrowseDlg::OutputString(const TCHAR *szMessage, ...)
{
	if (m_pBrowseDlg == NULL)
	{
		return;
	}

	TCHAR szBuffer[256];

	va_list cArgs;
	va_start(cArgs, szMessage);
	_vstprintf(szBuffer, szMessage, cArgs);
	va_end(cArgs);

	CString szOutput;
	szOutput.Format(_T("%s"), szBuffer);

	m_TabMessages.m_lbMessages.AddString(szOutput);
	m_TabMessages.m_lbMessages.SetTopIndex(m_TabMessages.m_lbMessages.GetCount() - 1);
}

void CBrowseDlg::UpdateTest(HTREEITEM hItem, TestResult nResult)
{
	if (nResult == trPassed)
	{
		m_TabTests.m_tcTests.SetItemImage(hItem, IL_TESTPASSED, IL_TESTPASSED);
	}
	else if (nResult == trFailed)
	{
		m_TabTests.m_tcTests.SetItemImage(hItem, IL_TESTFAILED, IL_TESTFAILED);
	}
	else if (nResult == trPartial)
	{
		m_TabTests.m_tcTests.SetItemImage(hItem, IL_TESTPARTIAL, IL_TESTPARTIAL);
	}
}

void CBrowseDlg::UpdateTestSet(HTREEITEM hItem)
{
	// Examine the results
	HTREEITEM hTest = m_TabTests.m_tcTests.GetNextItem(hItem, TVGN_CHILD);
	while (hTest)
	{
		Test *pTest = (Test *) m_TabTests.m_tcTests.GetItemData(hTest);
		UpdateTest(hTest, pTest->nLastResult);
		hTest = m_TabTests.m_tcTests.GetNextItem(hTest, TVGN_NEXT);
	}
}

void CBrowseDlg::OnRunTest() 
{
	HTREEITEM hItem = m_TabTests.m_tcTests.GetNextItem(NULL, TVGN_FIRSTVISIBLE);
	while (hItem)
	{
		UINT nState = m_TabTests.m_tcTests.GetItemState(hItem, TVIS_SELECTED);
		if (!(nState & TVIS_SELECTED))
		{
			hItem = m_TabTests.m_tcTests.GetNextItem(hItem, TVGN_NEXTVISIBLE);
			continue;
		}

		if (m_TabTests.m_tcTests.ItemHasChildren(hItem))
		{
			// Run complete set of tests
			TestSet *pTestSet = (TestSet *) m_TabTests.m_tcTests.GetItemData(hItem);
			RunTestSet(pTestSet);
			UpdateTestSet(hItem);
		}
		else
		{
			// Find the test
			Test *pTest = (Test *) m_TabTests.m_tcTests.GetItemData(hItem);
			TestResult nResult = RunTest(pTest);
			UpdateTest(hItem, nResult);
		}

		hItem = m_TabTests.m_tcTests.GetNextItem(hItem, TVGN_NEXTVISIBLE);
	}
}

struct _ElementPos
{
	HTREEITEM m_htiParent;
	CIPtr(IHTMLElementCollection) m_cpElementCollection;
	int m_nIndex;

	_ElementPos(HTREEITEM htiParent, IHTMLElementCollection *pElementCollection, int nIndex)
	{
		m_htiParent = htiParent;
		m_cpElementCollection = pElementCollection;
		m_nIndex = nIndex;
	}
	_ElementPos()
	{
	}
};

void CBrowseDlg::OnRefreshDOM() 
{
	m_TabDOM.m_tcDOM.DeleteAllItems();

	std::stack<_ElementPos> cStack;

	CComPtr<IUnknown> cpUnkPtr;
	m_pControlSite->GetControlUnknown(&cpUnkPtr);
	CIPtr(IWebBrowserApp) cpWebBrowser = cpUnkPtr;
	if (cpWebBrowser == NULL)
	{
		return;
	}

	CIPtr(IDispatch) cpDispDocument;
	cpWebBrowser->get_Document(&cpDispDocument);
	if (cpDispDocument == NULL)
	{
		return;
	}

	// Recurse the DOM, building a tree
	
	CIPtr(IHTMLDocument2) cpDocElement = cpDispDocument;
	
	CIPtr(IHTMLElementCollection) cpColl;
	HRESULT hr = cpDocElement->get_all( &cpColl );

	cStack.push(_ElementPos(NULL, cpColl, 0));
	while (!cStack.empty())
	{
		// Pop next position from stack
		_ElementPos pos = cStack.top();
		cStack.pop();

		// Iterate through elemenets in collection
		LONG nElements = 0;;
		pos.m_cpElementCollection->get_length(&nElements);
		for (int i = pos.m_nIndex; i < nElements; i++ )
		{
			CComVariant vName(i);
			CComVariant vIndex;
			CIPtr(IDispatch) cpDisp;

			hr = pos.m_cpElementCollection->item( vName, vIndex, &cpDisp );
			if ( hr != S_OK )
			{
				continue;
			}
			CIPtr(IHTMLElement) cpElem = cpDisp;
			if (cpElem == NULL)
			{
				continue;
			}

			// Get tag name
			BSTR bstrTagName = NULL;
			hr = cpElem->get_tagName(&bstrTagName);
			CString szTagName = bstrTagName;
			SysFreeString(bstrTagName);

			// Add an icon to the tree
			HTREEITEM htiParent = m_TabDOM.m_tcDOM.InsertItem(szTagName, IL_CLOSEDFOLDER, IL_CLOSEDFOLDER, pos.m_htiParent);

			CIPtr(IDispatch) cpDispColl;
			hr = cpElem->get_children(&cpDispColl);
			if (hr == S_OK)
			{
				CIPtr(IHTMLElementCollection) cpChildColl = cpDispColl;
				cStack.push(_ElementPos(pos.m_htiParent, pos.m_cpElementCollection, pos.m_nIndex + 1));
				cStack.push(_ElementPos(htiParent, cpChildColl, 0));
				break;
			}
		}
	}
}


void CBrowseDlg::OnClose() 
{
	DestroyWebBrowser();
	DestroyWindow();
}


void CBrowseDlg::OnDestroy() 
{
	CDialog::OnDestroy();
	delete this;	
}

void CBrowseDlg::ExecOleCommand(const GUID *pguidGroup, DWORD nCmdId)
{
	CComPtr<IUnknown> spUnkBrowser;
	m_pControlSite->GetControlUnknown(&spUnkBrowser);

	CIPtr(IOleCommandTarget) spCommandTarget = spUnkBrowser;
	if (spCommandTarget)
	{
		HRESULT hr = spCommandTarget->Exec(&CGID_MSHTML, nCmdId, 0, NULL, NULL);
		OutputString(_T("Exec(%d), returned %08x"), (int) nCmdId, hr);
	}
	else
	{
		OutputString(_T("Error: Browser does not support IOleCommandTarget"));
	}
}


void CBrowseDlg::OnEditMode() 
{
	DWORD nCmdID = (m_btnEditMode.GetCheck()) ? IDM_EDITMODE : IDM_BROWSEMODE;
	ExecOleCommand(&CGID_MSHTML, nCmdID);

//	if (m_pControlSite)
//	{
//		m_pControlSite->SetAmbientUserMode((m_btnEditMode.GetCheck() == 0) ? FALSE : TRUE);
//	}
}


void CBrowseDlg::OnEditBold()
{
	ExecOleCommand(&CGID_MSHTML, IDM_BOLD);
}

void CBrowseDlg::OnEditItalic()
{
	ExecOleCommand(&CGID_MSHTML, IDM_ITALIC);
}

void CBrowseDlg::OnEditUnderline()
{
	ExecOleCommand(&CGID_MSHTML, IDM_UNDERLINE);
}

void CBrowseDlg::OnFileExit() 
{
	OnClose();
}

void CBrowseDlg::OnViewRefresh() 
{
	IWebBrowser *pIWebBrowser = NULL;
	if (SUCCEEDED(GetWebBrowser(&pIWebBrowser)))
	{
		CComVariant vValue(REFRESH_COMPLETELY);
		pIWebBrowser->Refresh2(&vValue);
		pIWebBrowser->Release();
	}
}

void CBrowseDlg::OnViewGotoBack() 
{
	OnBackward();
}

void CBrowseDlg::OnViewGotoForward() 
{
	OnForward();
}

void CBrowseDlg::OnUpdateViewGotoBack(CCmdUI* pCmdUI) 
{
	// TODO: Add your command update UI handler code here
}

void CBrowseDlg::OnUpdateViewGotoForward(CCmdUI* pCmdUI) 
{
	// TODO: Add your command update UI handler code here
}

void CBrowseDlg::OnViewGotoHome() 
{
	IWebBrowser *pIWebBrowser = NULL;
	if (SUCCEEDED(GetWebBrowser(&pIWebBrowser)))
	{
		pIWebBrowser->GoHome();
		pIWebBrowser->Release();
	}
}

void CBrowseDlg::OnEditCopy() 
{
	ExecOleCommand(NULL, OLECMDID_COPY);
}

void CBrowseDlg::OnEditCut() 
{
	ExecOleCommand(NULL, OLECMDID_CUT);
}

void CBrowseDlg::OnEditPaste() 
{
	ExecOleCommand(NULL, OLECMDID_PASTE);
}

void CBrowseDlg::OnEditSelectAll() 
{
	ExecOleCommand(NULL, OLECMDID_SELECTALL);
}

void CBrowseDlg::OnHelpAbout() 
{
	AfxMessageBox(_T("CBrowse - Browser Control Test Harness"));
}

void CBrowseDlg::OnViewViewSource() 
{
	ExecOleCommand(&CGID_MSHTML, 3);
}

void CBrowseDlg::OnFileSaveAs() 
{
	ExecOleCommand(NULL, OLECMDID_SAVEAS);
}

void CBrowseDlg::OnFilePrint() 
{
	ExecOleCommand(NULL, OLECMDID_PRINT);
}

void CBrowseDlg::OnDebugVisible() 
{
    VARIANT_BOOL visible = VARIANT_TRUE;
	IWebBrowser *pIWebBrowser = NULL;
	if (SUCCEEDED(GetWebBrowser(&pIWebBrowser)))
	{
       	CIPtr(IWebBrowserApp) cpWebBrowser = pIWebBrowser;
        cpWebBrowser->get_Visible(&visible);
        cpWebBrowser->put_Visible(visible == VARIANT_TRUE ? VARIANT_FALSE : VARIANT_TRUE);
		pIWebBrowser->Release();
	}
}

void CBrowseDlg::OnUpdateDebugVisible(CCmdUI* pCmdUI) 
{
    VARIANT_BOOL visible = VARIANT_TRUE;
	IWebBrowser *pIWebBrowser = NULL;
	if (SUCCEEDED(GetWebBrowser(&pIWebBrowser)))
	{
       	CIPtr(IWebBrowserApp) cpWebBrowser = pIWebBrowser;
		cpWebBrowser->get_Visible(&visible);
		pIWebBrowser->Release();
	}

    pCmdUI->SetCheck(visible == VARIANT_TRUE ? 1 : 0);
}
