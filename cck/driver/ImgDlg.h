#if !defined(AFX_IMGDLG_H__53403F00_0198_11D3_B1F3_006008A6BBCE__INCLUDED_)
#define AFX_IMGDLG_H__53403F00_0198_11D3_B1F3_006008A6BBCE__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// ImgDlg.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CImgDlg dialog

class CImgDlg : public CDialog
{
// Construction
public:
	CImgDlg(CWnd* pParent = NULL);   // standard constructor
	CImgDlg(CString theIniFileName, CWnd* pParent = NULL);   // standard constructor

	// Dialog Data
	//{{AFX_DATA(CImgDlg)
	enum { IDD = IDD_IMG_DLG };
		// NOTE: the ClassWizard will add data members here
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CImgDlg)
	public:
	virtual int DoModal();
	virtual BOOL Create(LPCTSTR lpszClassName, LPCTSTR lpszWindowName, DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID, CCreateContext* pContext = NULL);
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual BOOL OnCommand(WPARAM wParam, LPARAM lParam);
	//}}AFX_VIRTUAL

// Implementation

	typedef struct DIMENSION
	{
		int width;
		int height;
	}DIMENSION;

	typedef struct IMAGE
	{
		CString name;
		POINT location;
		DIMENSION size;	
		HBITMAP hBitmap;
	}IMAGE;

	CString imageSectionName;
	CString iniFileName;
	IMAGE image;

	void ReadImageFromIniFile();
	
protected:

	// Generated message map functions
	//{{AFX_MSG(CImgDlg)
	afx_msg void OnHelpButton();
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnPaint();
	afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
	afx_msg BOOL OnHelpInfo(HELPINFO* pHelpInfo);
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_IMGDLG_H__53403F00_0198_11D3_B1F3_006008A6BBCE__INCLUDED_)
