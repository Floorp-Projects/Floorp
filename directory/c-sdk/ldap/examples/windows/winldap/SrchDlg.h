// SrchDlg.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// SearchDlg dialog

class SearchDlg : public CDialog
{
// Construction
public:
	SearchDlg(CWnd* pParent = NULL);   // standard constructor

public:
	void SetScope( int scope ) { m_scope = scope; }
	int GetScope() { return m_scope; }

private:
	int m_scope;

public:
// Dialog Data
	//{{AFX_DATA(SearchDlg)
	enum { IDD = IDD_SEARCH_DIALOG };
	CString	m_searchBase;
	CString	m_searchFilter;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(SearchDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(SearchDlg)
	virtual BOOL OnInitDialog();
	afx_msg void OnScopeBase();
	afx_msg void OnScopeOne();
	afx_msg void OnScopeSub();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};
