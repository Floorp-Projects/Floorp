/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

// The CNetscapePropertySheet is the same as a CPropertySheet except that
//    it calls all of its children with the IDOK message instead of just
//    the active page.  It also assumes all of its children panes are of
//    type CNetscapePropertyPage and calls OnHelp() on them whenever it
//    receives a ID_HELP commend.
//

#ifndef __PROPERTY_H
#define __PROPERTY_H

// Tests if the Apply button in a property page is enabled
// If Apply enable state is carefully managed, this can be used
//   to test if any data has been modifed in a property page
#define IS_APPLY_ENABLED(pPage)  (pPage && pPage->GetParent()->GetDlgItem(ID_APPLY_NOW)->IsWindowEnabled())

/////////////////////////////////////////////////////////////////////////////
// CNetscapePropertySheet dialog
class CNetscapePropertySheet : public CPropertySheet
{
// Construction
public:
    CNetscapePropertySheet(const char * pName, CWnd * parent = NULL, UINT nSelectedPage = 0,
                           MWContext *pMWContext = NULL,
                           BOOL bUseApplyButton = FALSE);
    virtual void OnOK();
    virtual void OnHelp();

    // Change the text of a property page during runtime
    void SetPageTitle(int nPage, LPCTSTR pszText);

// TODO: We can remove this check once everyone moves to MSVC 4.0
#if defined(MSVC4)
    //CLM: The change to MFC broke this -- it doesn't return last page
    //     once the dialog is closed, which is how we always use it!
    //     see genframe.cpp for new function
    int			 GetCurrentPage();
    int          m_nCurrentPage;
	BOOL		 SetCurrentPage(int iPage);
#else	
	int			 GetCurrentPage() { return m_nCurPage; };
	int			 SetCurrentPage(int iPage);
#endif	// MSVC4
	
protected:              
    BOOL        m_bUseApplyButton;

    // Generated message map functions
	//{{AFX_MSG(CNetscapePropertySheet)
	afx_msg int  OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnDestroy();
	//}}AFX_MSG
#ifdef XP_WIN32
    afx_msg BOOL OnHelpInfo(HELPINFO *);
#endif
	DECLARE_MESSAGE_MAP()

private:
    MWContext *m_pMWContext; //For Editor Undo
};

/////////////////////////////////////////////////////////////////////////////
// CNetscapePropertyPage dialog
class CNetscapePropertyPage : public CPropertyPage
{

protected:

    UINT m_nIDFocus;

public:
    // CLM: Added to pass in Caption ID and initial control to set focus
    CNetscapePropertyPage(UINT nID, UINT nIDCaption = 0, UINT nIDFocus = 0);

    // Use instead of MFC's CancelToClose, which doesn't work as advertised!
    // Changes "OK" button text to "Close" and dissables "Cancel" button
    // Also does SetModified(FALSE), which is always needed when this is used
    void OkToClose();

	// for help works in property page, OnHelp should be a virtual, not 
	// from message map
    virtual void OnHelp();

protected:              
    DECLARE_MESSAGE_MAP()
    
    BOOL SetInitialFocus( UINT nID );
};

#endif // __PROPERTY_H
