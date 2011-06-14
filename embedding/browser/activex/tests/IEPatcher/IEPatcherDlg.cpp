// IEPatcherDlg.cpp : implementation file
//

#include "stdafx.h"
#include "IEPatcher.h"
#include "IEPatcherDlg.h"
#include "ScanForFilesDlg.h"
#include "ScannerThread.h"
#include "DlgProxy.h"

#include <sys/types.h> 
#include <sys/stat.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

const CLSID CLSID_WebBrowser_V1 = { 0xEAB22AC3, 0x30C1, 0x11CF, { 0xA7, 0xEB, 0x00, 0x00, 0xC0, 0x5B, 0xAE, 0x0B } };
const CLSID CLSID_WebBrowser = { 0x8856F961, 0x340A, 0x11D0, { 0xA9, 0x6B, 0x00, 0xC0, 0x4F, 0xD7, 0x05, 0xA2 } };
const CLSID CLSID_InternetExplorer = { 0x0002DF01, 0x0000, 0x0000, { 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46 } };
const CLSID CLSID_MozillaBrowser = {0x1339B54C,0x3453,0x11D2,{0x93,0xB9,0x00,0x00,0x00,0x00,0x00,0x00}};
const IID IID_IWebBrowser = { 0xEAB22AC1, 0x30C1, 0x11CF, { 0xA7, 0xEB, 0x00, 0x00, 0xC0, 0x5B, 0xAE, 0x0B } };
const IID IID_IWebBrowser2 = { 0xD30C1661, 0xCDAF, 0x11d0, { 0x8A, 0x3E, 0x00, 0xC0, 0x4F, 0xC9, 0xE2, 0x6E } };
const IID IID_IWebBrowserApp = { 0x0002DF05, 0x0000, 0x0000, { 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46 } };


#define ITEM_PATCHSTATUS 1

#define IMG_DOESNTCONTAINIE	0
#define IMG_CONTAINSIE		1
#define IMG_CONTAINSMOZILLA	2
#define IMG_UNKNOWNSTATUS	3

/////////////////////////////////////////////////////////////////////////////
// CIEPatcherDlg dialog

IMPLEMENT_DYNAMIC(CIEPatcherDlg, CDialog);

CIEPatcherDlg::CIEPatcherDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CIEPatcherDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CIEPatcherDlg)
	//}}AFX_DATA_INIT
	// Note that LoadIcon does not require a subsequent DestroyIcon in Win32
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
	m_pAutoProxy = NULL;
}


CIEPatcherDlg::~CIEPatcherDlg()
{
	// If there is an automation proxy for this dialog, set
	//  its back pointer to this dialog to NULL, so it knows
	//  the dialog has been deleted.
	if (m_pAutoProxy != NULL)
		m_pAutoProxy->m_pDialog = NULL;
}


void CIEPatcherDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CIEPatcherDlg)
	DDX_Control(pDX, IDC_PATCH, m_btnPatch);
	DDX_Control(pDX, IDC_FILELIST, m_cFileList);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CIEPatcherDlg, CDialog)
	//{{AFX_MSG_MAP(CIEPatcherDlg)
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_WM_CLOSE()
	ON_BN_CLICKED(IDC_SCAN, OnScan)
	ON_NOTIFY(NM_CLICK, IDC_FILELIST, OnClickFilelist)
	ON_BN_CLICKED(IDC_PATCH, OnPatch)
	//}}AFX_MSG_MAP
	ON_MESSAGE(WM_UPDATEFILESTATUS, OnUpdateFileStatus)
END_MESSAGE_MAP()


/////////////////////////////////////////////////////////////////////////////
// CIEPatcherDlg message handlers

BOOL CIEPatcherDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	// Start the scanner thread
	AfxBeginThread(RUNTIME_CLASS(CScannerThread), 0);

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon

	// Add icons to image list
	m_cImageList.Create(16, 16, ILC_MASK, 0, 5);
	m_cImageList.Add(AfxGetApp()->LoadIcon(IDI_DOESNTCONTAINIE));
	m_cImageList.Add(AfxGetApp()->LoadIcon(IDI_CONTAINSIE));
	m_cImageList.Add(AfxGetApp()->LoadIcon(IDI_CONTAINSMOZILLA));
	m_cImageList.Add(AfxGetApp()->LoadIcon(IDI_UNKNOWNSTATUS));

	// Associate image list with file list
	m_cFileList.SetImageList(&m_cImageList, LVSIL_SMALL);

	struct ColumnData
	{
		TCHAR *sColumnTitle;
		int nPercentageWidth;
		int nFormat;
	};

	ColumnData aColData[] =
	{
		{ _T("File"),			70, LVCFMT_LEFT },
		{ _T("Patch Status"),	30, LVCFMT_LEFT }
	};

	// Get the size of the file list control and neatly
	// divide it into columns

	CRect rcFileList;
	m_cFileList.GetClientRect(&rcFileList);

	int nColTotal = sizeof(aColData) / sizeof(aColData[0]);
	int nWidthRemaining = rcFileList.Width();
	
	for (int nCol = 0; nCol < nColTotal; nCol++)
	{
		ColumnData *pData = &aColData[nCol];

		int nColWidth = (rcFileList.Width() * pData->nPercentageWidth) / 100;
		if (nCol == nColTotal - 1)
		{
			nColWidth = nWidthRemaining;
		}
		else if (nColWidth > nWidthRemaining)
		{
			nColWidth = nWidthRemaining;
		}

		m_cFileList.InsertColumn(nCol, pData->sColumnTitle, pData->nFormat, nColWidth);

		nWidthRemaining -= nColWidth;
		if (nColWidth <= 0)
		{
			break;
		}
	}

	return TRUE;  // return TRUE  unless you set the focus to a control
}


// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CIEPatcherDlg::OnPaint() 
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
HCURSOR CIEPatcherDlg::OnQueryDragIcon()
{
	return (HCURSOR) m_hIcon;
}


// Automation servers should not exit when a user closes the UI
//  if a controller still holds on to one of its objects.  These
//  message handlers make sure that if the proxy is still in use,
//  then the UI is hidden but the dialog remains around if it
//  is dismissed.

void CIEPatcherDlg::OnClose() 
{
	if (CanExit())
		CDialog::OnClose();
}


LONG CIEPatcherDlg::OnUpdateFileStatus(WPARAM wParam, LPARAM lParam)
{
	PatchStatus ps = (PatchStatus) wParam;
	TCHAR *pszFile = (TCHAR *) lParam;
	UpdateFileStatus(pszFile, ps);
	return 0;
}


void CIEPatcherDlg::OnScan() 
{
	CScanForFilesDlg dlg;
	
	if (dlg.DoModal() == IDOK)
	{
		CWaitCursor wc;

		CFileFind op;
		if (op.FindFile(dlg.m_sFilePattern))
		{
			op.FindNextFile();
			do {
				if (!op.IsDirectory())
				{
					AddFileToList(op.GetFilePath());
				}
			} while (op.FindNextFile());
			op.Close();
		}
	}
}


void CIEPatcherDlg::OnPatch() 
{
	int nItem = -1;
	do {
		nItem = m_cFileList.GetNextItem(nItem, LVNI_ALL | LVNI_SELECTED);
		if (nItem != -1)
		{
			m_cQueuedFileDataList[nItem]->ps = psPatchPending;
			UpdateFileStatus(nItem, m_cQueuedFileDataList[nItem]->sFileName, m_cQueuedFileDataList[nItem]->ps);
		}
	} while (nItem != -1);
}


void CIEPatcherDlg::OnClickFilelist(NMHDR* pNMHDR, LRESULT* pResult) 
{
	// Check if files are selected	

	*pResult = 0;
}


///////////////////////////////////////////////////////////////////////////////


BOOL CIEPatcherDlg::CanExit()
{
	// If the proxy object is still around, then the automation
	//  controller is still holding on to this application.  Leave
	//  the dialog around, but hide its UI.
	if (m_pAutoProxy != NULL)
	{
		ShowWindow(SW_HIDE);
		return FALSE;
	}

	return TRUE;
}


PatchStatus CIEPatcherDlg::ScanFile(const CString &sFileName)
{
	char *pszBuffer = NULL;
	long nBufferSize = 0;
	if (!ReadFileToBuffer(sFileName, &pszBuffer, &nBufferSize))
	{
		return psFileError;
	}

	PatchStatus ps = ScanBuffer(pszBuffer, nBufferSize, FALSE);
	delete []pszBuffer;
	return ps;
}


BOOL CIEPatcherDlg::WriteBufferToFile(const CString &sFileName, char *pszBuffer, long nBufferSize)
{
	CString sMessage;
	sMessage.Format("Saving patched file \"%s\"", sFileName);
	TraceProgress(sMessage);

	FILE *fDest = fopen(sFileName, "wb");
	if (fDest == NULL)
	{
		TraceProgress("Error: Could not open destination file");
		return FALSE;
	}

	fwrite(pszBuffer, 1, nBufferSize, fDest);
	fclose(fDest);
	TraceProgress("File saved");

	return TRUE;
}


BOOL CIEPatcherDlg::ReadFileToBuffer(const CString &sFileName, char **ppszBuffer, long *pnBufferSize)
{
	CString sProcessing;
	sProcessing.Format("Processing file \"%s\"", sFileName);
	TraceProgress(sProcessing);

	// Allocate a memory buffer to slurp up the whole file
	struct _stat srcStat;
	_stat(sFileName, &srcStat);

	char *pszBuffer = new char[srcStat.st_size / sizeof(char)];
	if (pszBuffer == NULL)
	{
		TraceProgress("Error: Could not allocate buffer for file");
		return FALSE;
	}
	FILE *fSrc = fopen(sFileName, "rb");
	if (fSrc == NULL)
	{
		TraceProgress("Error: Could not open file");
		delete []pszBuffer;
		return FALSE;
	}
	size_t sizeSrc = srcStat.st_size;
	size_t sizeRead = 0;

	// Dumb but effective
	sizeRead = fread(pszBuffer, 1, sizeSrc, fSrc);
	fclose(fSrc);

	if (sizeRead != sizeSrc)
	{
		TraceProgress("Error: Could not read all of file");
		delete []pszBuffer;
		return FALSE;
	}

	*ppszBuffer = pszBuffer;
	*pnBufferSize = sizeRead;

	return TRUE;
}


PatchStatus CIEPatcherDlg::ScanBuffer(char *pszBuffer, long nBufferSize, BOOL bApplyPatch)
{
	if (nBufferSize < sizeof(CLSID))
	{
		return psNotPatchable;
	}
		
	TraceProgress("Scanning for IE...");

	BOOL bPatchApplied = FALSE;
	PatchStatus ps = psUnknownStatus;

	// Scan through buffer, one byte at a time doing a memcmp
	for (size_t i = 0; i < nBufferSize - sizeof(CLSID); i++)
	{
		if (memcmp(&pszBuffer[i], &CLSID_MozillaBrowser, sizeof(CLSID)) == 0)
		{
			TraceProgress("Found CLSID_MozillaBrowser");
			if (ps == psUnknownStatus)
			{
				ps = psAlreadyPatched;
			}
		}
		else if (memcmp(&pszBuffer[i], &CLSID_WebBrowser_V1, sizeof(CLSID)) == 0)
		{
			TraceProgress("Found CLSID_WebBrowser_V1");
			if (ps == psUnknownStatus)
			{
				ps = psPatchable;
			}
			if (bApplyPatch)
			{
				TraceProgress("Patching with CLSID_MozillaBrowser");
				memcpy(&pszBuffer[i], &CLSID_MozillaBrowser, sizeof(CLSID));
				bPatchApplied = TRUE;
			}
		}
		else if (memcmp(&pszBuffer[i], &CLSID_WebBrowser, sizeof(CLSID)) == 0)
		{
			TraceProgress("Found CLSID_WebBrowser");
			if (ps == psUnknownStatus)
			{
				ps = psPatchable;
			}
			if (bApplyPatch)
			{
				TraceProgress("Patching with CLSID_MozillaBrowser");
				memcpy(&pszBuffer[i], &CLSID_MozillaBrowser, sizeof(CLSID));
				TraceProgress("Patching with CLSID_MozillaBrowser");
				bPatchApplied = TRUE;
			}
		}
		else if (memcmp(&pszBuffer[i], &IID_IWebBrowser, sizeof(CLSID)) == 0)
		{
			TraceProgress("Found IID_IWebBrowser");
			if (ps == psUnknownStatus)
			{
				ps = psPatchable;
			}
		}
		else if (memcmp(&pszBuffer[i], &CLSID_InternetExplorer, sizeof(CLSID)) == 0)
		{
			TraceProgress("Warning: Found CLSID_InternetExplorer, patching might not work");
			if (ps == psUnknownStatus)
			{
				ps = psMayBePatchable;
			}
		}
		else if (memcmp(&pszBuffer[i], &IID_IWebBrowser2, sizeof(CLSID)) == 0)
		{
			TraceProgress("Found IID_IWebBrowser2");
			if (ps == psUnknownStatus)
			{
				ps = psPatchable;
			}
		}
		else if (memcmp(&pszBuffer[i], &IID_IWebBrowserApp, sizeof(CLSID)) == 0)
		{
			TraceProgress("Found IID_IWebBrowserApp");
			if (ps == psUnknownStatus)
			{
				ps = psPatchable;
			}
		}
	}

	if (ps == psUnknownStatus)
	{
		ps = psNotPatchable;
	}
	if (bApplyPatch)
	{
		ps = (bPatchApplied) ? psAlreadyPatched : psNotPatchable;
	}

	TraceProgress("Scan completed");

	return ps;
}


BOOL CIEPatcherDlg::PatchFile(const CString & sFileFrom, const CString & sFileTo, PatchStatus *pPatchStatus)
{
	if (sFileTo.IsEmpty())
	{
		return FALSE;
	}
	if (sFileTo == sFileFrom)
	{
		TraceProgress("Warning: Patching disabled for safety reasons, source and destination names must differ");
		return FALSE;
	}
	
	char *pszBuffer = NULL;
	long nBufferSize = 0;

	if (!ReadFileToBuffer(sFileFrom, &pszBuffer, &nBufferSize))
	{
		return FALSE;
	}

	// Scan and patch the buffer
	*pPatchStatus = ScanBuffer(pszBuffer, nBufferSize, TRUE);
	if (*pPatchStatus == psNotPatchable)
	{
		TraceProgress("Error: Nothing was found to patch");
	}
	else
	{
		// Write out the patch file
		WriteBufferToFile(sFileTo, pszBuffer, nBufferSize);
	}

	delete []pszBuffer;
	return FALSE;
}


void CIEPatcherDlg::TraceProgress(const CString &sProgress)
{
	// TODO
}


void CIEPatcherDlg::AddFileToList(const CString & sFile)
{
	CSingleLock sl(&m_csQueuedFileDataList, TRUE);

	int nIndex = m_cFileList.GetItemCount();
	m_cFileList.InsertItem(nIndex, sFile, IMG_UNKNOWNSTATUS);

	QueuedFileData *pData = new QueuedFileData;
	pData->ps = psUnknownStatus;
	pData->sFileName = sFile;
	pData->sPatchedFileName = sFile;
	m_cQueuedFileDataList.Add(pData);

	UpdateFileStatus(nIndex, pData->ps);
}


void CIEPatcherDlg::UpdateFileStatus(int nFileIndex, const CString &sFile, PatchStatus ps)
{
	CString sStatus;
	int nIconStatus = -1;

	switch (ps)
	{
	case psFileError:
		sStatus = _T("File error");
		break;

	case psNotPatchable:
		sStatus = _T("Nothing to patch");
		nIconStatus = IMG_DOESNTCONTAINIE;
		break;

	case psPatchable:
		sStatus = _T("Patchable");
		nIconStatus = IMG_CONTAINSIE;
		break;

	case psMayBePatchable:
		sStatus = _T("Maybe patchable");
		nIconStatus = IMG_CONTAINSIE;
		break;

	case psAlreadyPatched:
		sStatus = _T("Already patched");
		nIconStatus = IMG_CONTAINSMOZILLA;
		break;

	case psUnknownStatus:
		sStatus = _T("Status pending");
		nIconStatus = IMG_UNKNOWNSTATUS;
		break;

	case psPatchPending:
		sStatus = _T("Patch pending");
		break;

	default:
		sStatus = _T("*** ERROR ***");
		break;
	}

	if (nIconStatus != -1)
	{
		m_cFileList.SetItem(nFileIndex, -1, LVIF_IMAGE, NULL, nIconStatus, 0, 0, 0);
	}
	m_cFileList.SetItemText(nFileIndex, ITEM_PATCHSTATUS, sStatus);
}


void CIEPatcherDlg::UpdateFileStatus(const CString &sFile, PatchStatus ps)
{
	CSingleLock sl(&m_csQueuedFileDataList, TRUE);

	for (int n = 0; n < m_cQueuedFileDataList.GetSize(); n++)
	{
		if (m_cQueuedFileDataList[n]->sFileName == sFile)
		{
			UpdateFileStatus(n, sFile, ps);
			m_cQueuedFileDataList[n]->ps = ps;
			return;
		}
	}
}


BOOL CIEPatcherDlg::GetPendingFileToScan(CString & sFileToScan)
{
	CSingleLock sl(&m_csQueuedFileDataList, TRUE);

	for (int n = 0; n < m_cQueuedFileDataList.GetSize(); n++)
	{
		if (m_cQueuedFileDataList[n]->ps == psUnknownStatus)
		{
			sFileToScan = m_cQueuedFileDataList[n]->sFileName;
			return TRUE;
		}
	}
	return FALSE;
}


BOOL CIEPatcherDlg::GetPendingFileToPatch(CString & sFileToPatch)
{
	CSingleLock sl(&m_csQueuedFileDataList, TRUE);

	for (int n = 0; n < m_cQueuedFileDataList.GetSize(); n++)
	{
		if (m_cQueuedFileDataList[n]->ps == psPatchPending)
		{
			sFileToPatch = m_cQueuedFileDataList[n]->sFileName;
			return TRUE;
		}
	}
	return FALSE;
}
