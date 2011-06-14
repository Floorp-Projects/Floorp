// IEPatcherDlg.h : header file
//

#if !defined(AFX_IEPATCHERDLG_H__A603167C_3B36_11D2_B44D_00600819607E__INCLUDED_)
#define AFX_IEPATCHERDLG_H__A603167C_3B36_11D2_B44D_00600819607E__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

class CIEPatcherDlgAutoProxy;

enum PatchStatus
{
	psFileError,
	psUnknownStatus,
	psPatchPending,
	psNotPatchable,
	psPatchable,
	psMayBePatchable,
	psAlreadyPatched
};

struct QueuedFileData
{
	CString sFileName;
	CString sPatchedFileName;
	PatchStatus ps;
};

#define WM_UPDATEFILESTATUS WM_USER+1

/////////////////////////////////////////////////////////////////////////////
// CIEPatcherDlg dialog

class CIEPatcherDlg : public CDialog
{
	DECLARE_DYNAMIC(CIEPatcherDlg);
	friend class CIEPatcherDlgAutoProxy;

// Construction
public:
	void AddFileToList(const CString &sFile);
	BOOL GetPendingFileToScan(CString &sFileToScan);
	BOOL GetPendingFileToPatch(CString &sFileToPatch);
	
	static PatchStatus ScanFile(const CString &sFileName);
	static PatchStatus ScanBuffer(char *pszBuffer, long nBufferSize, BOOL bApplyPatches);
	static BOOL PatchFile(const CString & sFileFrom, const CString & sFileTo, PatchStatus *pPatchStatus);
	
	static BOOL WriteBufferToFile(const CString &sFileName, char *pszBuffer, long nBufferSize);
	static BOOL ReadFileToBuffer(const CString &sFileName, char **ppszBuffer, long *pnBufferSize);
	static void TraceProgress(const CString &sProgress);
	
	void UpdateFileStatus(const CString &sFile, PatchStatus ps);
	void UpdateFileStatus(int nFileIndex, const CString &sFile, PatchStatus ps);


	CIEPatcherDlg(CWnd* pParent = NULL);	// standard constructor
	virtual ~CIEPatcherDlg();

	CImageList m_cImageList;

	CArray<QueuedFileData *, QueuedFileData *> m_cQueuedFileDataList;
	CCriticalSection m_csQueuedFileDataList;

// Dialog Data
	//{{AFX_DATA(CIEPatcherDlg)
	enum { IDD = IDD_IEPATCHER_DIALOG };
	CButton	m_btnPatch;
	CListCtrl	m_cFileList;
	//}}AFX_DATA

	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CIEPatcherDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	CIEPatcherDlgAutoProxy* m_pAutoProxy;
	HICON m_hIcon;

	BOOL CanExit();

	// Generated message map functions
	//{{AFX_MSG(CIEPatcherDlg)
	virtual BOOL OnInitDialog();
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	afx_msg void OnClose();
	afx_msg void OnScan();
	afx_msg void OnClickFilelist(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnPatch();
	//}}AFX_MSG
	afx_msg LONG OnUpdateFileStatus(WPARAM, LPARAM);
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_IEPATCHERDLG_H__A603167C_3B36_11D2_B44D_00600819607E__INCLUDED_)

