#if !defined(AFX_WIZHELP_H__8A0EF609_08A7_11D3_B200_006008A6BBCE__INCLUDED_)
#define AFX_WIZHELP_H__8A0EF609_08A7_11D3_B200_006008A6BBCE__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// WizHelp.h : header file
//
typedef struct AREA
{
	int width;
	int height;
}AREA;

typedef struct WID
{
	CString value;
	CString target;
	POINT location;
	AREA size;
	int ButtonID;
}WID;


/////////////////////////////////////////////////////////////////////////////
// CWizHelp dialog

class CWizHelp : public CDialog
{
// Construction
public:
	CWizHelp(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CWizHelp)
	enum { IDD = IDD_WIZ_HELP };
		// NOTE: the ClassWizard will add data members here
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CWizHelp)
	public:
	virtual BOOL Create(LPCTSTR lpszClassName, LPCTSTR lpszWindowName, DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID, CCreateContext* pContext = NULL);
	virtual int DoModal();
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual BOOL OnCommand(WPARAM wParam, LPARAM lParam);
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CWizHelp)
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_WIZHELP_H__8A0EF609_08A7_11D3_B200_006008A6BBCE__INCLUDED_)
