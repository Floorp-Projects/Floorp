#ifndef _EDITFLOAT_H
#define _EDITFLOAT_H

class CEnderBar : public CToolBar
{
// Construction
public:
    DECLARE_DYNAMIC(CEnderBar)
	CEnderBar();
	BOOL Init(CWnd* pParentWnd, BOOL bToolTips);
	BOOL SetColor(BOOL bColor);
	BOOL SetHorizontal();
	BOOL SetVertical();

// Attributes
public:

// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CEnderBar)
	//}}AFX_VIRTUAL

private:
	BOOL m_bVertical;
	CPoint m_location;
// Implementation
public:

	virtual ~CEnderBar();
//	virtual CSize CalcFixedLayout(BOOL bStretch, BOOL bHorz);
	virtual CSize CalcDynamicLayout(int nLength, DWORD dwMode);

	// Generated message map functions
protected:
	//{{AFX_MSG(CEnderBar)
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

#endif //_EDITFLOAT_H
