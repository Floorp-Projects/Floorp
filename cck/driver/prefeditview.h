// PrefEditView.h : interface of the CPrefEditView class
//
/////////////////////////////////////////////////////////////////////////////

#if !defined(AFX_PREFEDITVIEW_H__6ACBEE69_AA14_43BA_B736_DF17EA66A7CE__INCLUDED_)
#define AFX_PREFEDITVIEW_H__6ACBEE69_AA14_43BA_B736_DF17EA66A7CE__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000


#include <afxcview.h>
#include "xmlparse.h"

class CPrefElement;

class CPrefEditView : public CTreeView
{
protected: 
  CPrefEditView();

// Attributes
public:

	CPrefEditView(CString strXMLFile);
	DECLARE_DYNCREATE(CPrefEditView)

// Operations
public:

  BOOL DoSavePrefsTree(CString strFileName);  // save the XML to a file
  void DoOpenItem();    // expand the tree, or open the selected item for edit
  void DoFindFirst();   // open the Find Pref dialog
  void DoFindNext();    // find next item
  void DoAdd();         // open the Add Pref dialog
  BOOL CheckForRemoteAdmins(); // see if any prefs were marked remote admin

  // These are only for the XML parser to call.
  void startElement(const char *name, const char **atts);
  void characterData(const XML_Char *s, int len);
  void endElement(const char *name);


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CPrefEditView)
	public:
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CPrefEditView();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

protected:

// Generated message map functions
protected:
	//{{AFX_MSG(CPrefEditView)
	afx_msg void OnDblclk(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnDestroy();
	afx_msg void OnEditPrefItem();
  afx_msg void OnFindPref();
  afx_msg void OnFindNextPref();
  afx_msg void OnAddPref();
  afx_msg void OnDelPref();
	afx_msg void OnRclick(NMHDR* pNMHDR, LRESULT* pResult);
  afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
  afx_msg void OnExpanded(NMHDR* pNMHDR, LRESULT* pResult);

	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

private:
  CImageList m_imageList;             // padlocks to show which prefs are locked
  CString m_strXMLFile;               // the XML file used to load the m_pPrefXMLTree, and hence the tree control
  CString m_strXMLVersion;            // the client version for the XML file
  CString m_strXMLSubVersion;         // minor version inside the XML file

  // Stuff for Find and FindNext.
  HTREEITEM m_hNextFind;
  CString m_strFind;
  BOOL FindFirst(CString& rstrFind);
  BOOL FindNext();
  HTREEITEM GetNextItem(HTREEITEM hItem);

  // Stuff for building the tree control from the XML file.
  HTREEITEM m_hgroup;
  CPrefElement* m_pParsingPrefElement;

  HTREEITEM FindTreeItemFromPrefname(HTREEITEM hItem, CString& rstrPrefName);
  HTREEITEM InsertPrefElement(CPrefElement* pe, HTREEITEM group);
  HTREEITEM AddPref(CString& rstrPrefName, CString& rstrPrefDesc, CString& rstrPrefType);
  bool IsRemoteAdministered(HTREEITEM hItem);

  BOOL LoadTreeControl();
  void WriteXMLItem(FILE* fp, int iLevel, HTREEITEM hItem);

  void DeleteTreeCtrl(HTREEITEM hParent);
  void EditSelectedPrefsItem();

  void ShowPopupMenu( CPoint& point, int submenu );

};


/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_PREFEDITVIEW_H__6ACBEE69_AA14_43BA_B736_DF17EA66A7CE__INCLUDED_)
