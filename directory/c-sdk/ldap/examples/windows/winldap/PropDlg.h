// PropDlg.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// PropDlg dialog

class PropDlg : public CDialog
{
// Construction
public:
	PropDlg(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(PropDlg)
	enum { IDD = IDD_ENTRY_PROPERTIES };
		// NOTE: the ClassWizard will add data members here
	//}}AFX_DATA

	void SetTitle( const char *str ) { m_title = str; }

private:
	CListBox m_list;
	CStringList m_strings;
	CString m_title;

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(PropDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
public:
	void AddLine( const char *line );

protected:

	// Generated message map functions
	//{{AFX_MSG(PropDlg)
	afx_msg void OnSize(UINT nType, int cx, int cy);
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};
