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

// mainfrm.h : interface of the CMainFrame class
//
/////////////////////////////////////////////////////////////////////////////

#ifndef MAINFRAME_H
#define MAINFRAME_H

#include "tlbutton.h"
#include "urlbar.h"

typedef struct big_rect {
	long top, bottom, left, right;
} BIG_RECT;

//////////////////////////////////////////////////////////////////////            
//////////////////////////////////////////////////////////////////////            
//////////////////////////////////////////////////////////////////////

extern IL_RGB animationPalette[];
extern int iLowerSystemColors;
extern int iLowerAnimationColors;
extern int iLowerColors;
extern int colorCubeSize;

class CMainFrame : public CGenericFrame
{
public: // create from serialization only
    CMainFrame();
    DECLARE_DYNCREATE(CMainFrame)

// Attributes
public:
    CString       m_csText;
    CStringList   m_csImageList;

public:
	CWnd        * m_pHistoryWindow;
	CWnd        * m_pDocInfoWindow;

private:
//	Some static public data, initialized in framinit.cpp
    static int    m_FirstFrame;
	CURLBar		* m_barLocation;
	CRDFToolbar *m_barLinks;
	CCommandToolbar *m_pCommandToolbar;


private :
//#ifndef NO_TAB_NAVIGATION
	// BOOL CMainFrame::setNextTabableFrame( CMainFrame * pCurrentFrame, int forward );
	BOOL CMainFrame::setNextTabFocus( int forward );
	int m_SrvrItemCount; // reference counting the server item.

	UINT	m_tabFocusInMainFrm; 
public :
	enum  { TAB_FOCUS_IN_NULL, TAB_FOCUS_IN_CHROME,TAB_FOCUS_IN_GRID };
	void SetTabFocusFlag( int nn ) { m_tabFocusInMainFrm = nn; }
//#endif /* NO_TAB_NAVIGATION */

// Implementation
public:
    virtual         ~CMainFrame();
    virtual BOOL    OnCreateClient(LPCREATESTRUCT lpcs, CCreateContext *pContext);
	void AddRef() {	m_SrvrItemCount++;} 
	void Release() {m_SrvrItemCount--;}
	BOOL HasSrvrItem() { return m_SrvrItemCount > 0; }

	BOOL 		PreTranslateMessage(MSG *pMsg);
    BOOL        PreCreateWindow(CREATESTRUCT& cs);
	virtual void GetMessageString( UINT nID, CString& rMessage ) const;

    void        Alert(char * Msg);
    int         Confirm(char * Msg);
    char      * Prompt(const char * Msg, const char * Dflt);
    char      * PromptPassword(char * Msg);

	void 		BuildHistoryMenu(CMenu* pMenu);
	void		OnLoadHomePage();
	const char *FindHistoryToolTipText(UINT nCommand);
	void		FillPlacesMenu(HMENU hMenu);

	int			CreateLocationBar(void);
	int			CreateLinkBar(void);
	int			CreateMainToolbar(void);

	virtual void RefreshNewEncoding(int16 csid, BOOL bIgnore=TRUE);

	void LoadShortcut(int iShortcutID);

	// Needed public for preference callback routines
	afx_msg void OnToggleImageLoad();
	afx_msg void OnOptionsShowstarterbuttons();
	afx_msg void OnOptionsViewToolBar();

#ifdef _DEBUG
    virtual void AssertValid() const;
    virtual void Dump(CDumpContext& dc) const;
#endif

	virtual BOOL AllowDocking() {return TRUE;}
// Generated message map functions
protected:
    //{{AFX_MSG(CMainFrame)
    afx_msg void OnShowWindow(BOOL bShow, UINT nStatus);
    afx_msg BOOL OnCommand(UINT wParam,LONG lParam);
    afx_msg void OnInitMenuPopup(CMenu* pPopup, UINT nIndex, BOOL bSysMenu);
    afx_msg void OnMenuSelect(UINT nItemID, UINT nFl, HMENU hSysMenu);
    afx_msg void OnOptionsTitlelocationBar();
    afx_msg void OnUpdateOptionsTitlelocationBar(CCmdUI* pCmdUI);
    afx_msg void OnDropdownUrl();
    afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
    afx_msg void OnDestroy();
	afx_msg void OnOptionsTogglenetdebug();
	afx_msg void OnShowTransferStatus();
	afx_msg void OnUpdateToggleImageLoad(CCmdUI* pCmdUI);
	afx_msg void OnUpdateShowTransferStatus(CCmdUI* pCmdUI);
	afx_msg void OnUpdateOptionsTogglenetdebug(CCmdUI* pCmdUI);
	afx_msg void OnNetscapeHome();
	afx_msg void OnGuide();
	afx_msg void OnStartingPoints();
	afx_msg void OnMetaIndex();
	afx_msg void OnHotlistHotlist();
	afx_msg LRESULT OnFindReplace(WPARAM wParam, LPARAM lParam);
	afx_msg void OnFlushCache();
	afx_msg void OnToggleFancyFtp();
	afx_msg void OnUpdateToggleFancyFtp(CCmdUI* pCmdUI);

	afx_msg void OnUpdateOptionsShowstarterbuttons(CCmdUI* pCmdUI);
	afx_msg void OnUpdateSecurity(CCmdUI* pCmdUI);
	afx_msg void OnUpdateSecurityStatus(CCmdUI *pCmdUI);
	afx_msg void OnShortcut1();
	afx_msg void OnShortcut2();
	afx_msg void OnShortcut3();
	afx_msg void OnShortcut4();
	afx_msg void OnShortcut5();
	afx_msg void OnShortcut6();
	afx_msg void OnClose();
    afx_msg void OnLocalHelp();
	afx_msg void OnDropFiles(HDROP hDropInfo);
	afx_msg BOOL OnQueryOpen();
	afx_msg void OnHelpSecurity();
    afx_msg void OnAboutPlugins();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnUpdateEditUndo(CCmdUI* pCmdUI);
    afx_msg void OnHelpMenu();
	afx_msg LRESULT OnButtonMenuOpen(WPARAM, LPARAM); 
	afx_msg LRESULT OnFillInToolTip(WPARAM, LPARAM); 
	afx_msg LRESULT OnFillInToolbarButtonStatus(WPARAM, LPARAM); 
	afx_msg void OnIncreaseFont();
	afx_msg void OnDecreaseFont();
	afx_msg void OnNetSearch();
	afx_msg void OnUpdateNetSearch(CCmdUI *pCmdUI);
	afx_msg void OnUpdateViewCommandToolbar(CCmdUI* pCmdUI);
	afx_msg void OnUpdateViewLocationToolbar(CCmdUI* pCmdUI);
	afx_msg void OnUpdateViewCustomToolbar(CCmdUI* pCmdUI);


	//}}AFX_MSG

	BOOL FileBookmark(HT_Resource pFolder);

    DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////

#endif // MAINFRAME_H
