// LdapView.h : interface of the LdapView class
//
/////////////////////////////////////////////////////////////////////////////

class LdapView : public CView
{
protected: // create from serialization only
	LdapView();
	DECLARE_DYNCREATE(LdapView)

// Attributes
public:
	LdapDoc* GetDocument();

private:
	CListBox m_list;

// Operations
public:
	void AddLine( LPCSTR line, const char *dn=NULL );
	void ClearLines();

private:
	void showProperties( LDAP *ld, char *dn );

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(LdapView)
	public:
	virtual void OnDraw(CDC* pDC);  // overridden to draw this view
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	protected:
	virtual void OnInitialUpdate(); // called first time after construct
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~LdapView();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

protected:

// Generated message map functions
protected:
	//{{AFX_MSG(LdapView)
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnListDoubleClick();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

#ifndef _DEBUG  // debug version in LdapView.cpp
inline LdapDoc* LdapView::GetDocument()
   { return (LdapDoc*)m_pDocument; }
#endif

/////////////////////////////////////////////////////////////////////////////
