class CChooseFolderDialog : public CDialog 
{
// Attributes
public:

	CString m_szFolder;
	CString m_szServer;
	CString m_szPrefUrl;

	CChooseFolderDialog(CWnd* pParent = NULL, char *pFolderPath = NULL, int nType = 0 );

    enum { IDD = IDD_PREF_CHOOSE_FOLDER};


	//{{AFX_VIRTUAL(CChooseFolderDialog)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
 
	int m_nTypeID;
	int m_nDefaultID;
	//CMailFolderCombo m_FolderCombo;
	//CMailFolderCombo m_ServerCombo;
	char* m_pFolderPath;

    virtual void OnOK();
	virtual BOOL OnInitDialog();

	//void OnNewFolder();
	//void OnSelectServer();
	//void OnSelectfolder();

	afx_msg void OnNewFolder();
	afx_msg void OnSelectServer();
	afx_msg void OnSelectFolder();
	DECLARE_MESSAGE_MAP()
};
