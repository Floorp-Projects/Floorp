// ConnDlg.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// ConnDlg dialog

class ConnDlg : public CDialog
{
// Construction
public:
	ConnDlg(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(ConnDlg)
	enum { IDD = IDD_CONNECT_DIALOG };
	CString	m_dirHost;
	int		m_dirPort;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(ConnDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(ConnDlg)
		// NOTE: the ClassWizard will add member functions here
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};
