// PrefEditView.h : interface of the CPrefEditView class
//
/////////////////////////////////////////////////////////////////////////////

#if !defined(AFX_PREFEDITVIEW_H__6ACBEE69_AA14_43BA_B736_DF17EA66A7CE__INCLUDED_)
#define AFX_PREFEDITVIEW_H__6ACBEE69_AA14_43BA_B736_DF17EA66A7CE__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#import "msxml.dll"
using namespace MSXML;

#include <afxcview.h>
#include "XMLDOMHelper.h"


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
	afx_msg void OnRclick(NMHDR* pNMHDR, LRESULT* pResult);
  afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);

	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

private:
  CImageList m_imageList;             // padlocks to show which prefs are locked
	IXMLDOMDocumentPtr m_pPrefXMLTree;  // the actual XML tree which describes the tree control contents
  CString m_strXMLFile;               // the XML file used to load the m_pPrefXMLTree, and hence the tree control

  // Stuff for Find and FindNext.
  IXMLDOMNodeListPtr m_pPrefsList;
  int m_iNextElement;
  CString m_strFind;
  BOOL FindFirst(CString& rstrFind);
  BOOL FindNext();

  
  BOOL InitXMLTree();
  HTREEITEM AddNodeToTreeCtrl(IXMLDOMNodePtr prefsTreeNode, HTREEITEM hTreeCtrlParent);
  HTREEITEM FindTreeItemFromPrefname(HTREEITEM hItem, CString& rstrPrefName);
  IXMLDOMElementPtr FindElementFromPrefname(CString& rstrPrefString);
  void SelectPref(CString& rstrPrefName);
  void DeleteTreeCtrl(HTREEITEM hParent);

  HTREEITEM AddPref(CString& rstrPrefName, CString& rstrPrefDesc, CString& rstrPrefType);

  void ShowPopupMenu( CPoint& point, int submenu );
  void EditSelectedPrefsItem();

};


/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_PREFEDITVIEW_H__6ACBEE69_AA14_43BA_B736_DF17EA66A7CE__INCLUDED_)
