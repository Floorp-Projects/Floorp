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

#ifndef __GenericFrame_H
#define __GenericFrame_H

// genframe.h : header file
//
#include "frameglu.h"
#include "htrdf.h"

#define WIN_ANIMATE_ICON_TIMER	4
#define MSG_TASK_NOTIFY			(WM_USER + 1)

// Longest menu item length for Composer's last-edited history submenus
#define MAX_MENU_ITEM_LENGTH	45

//cmanske: As much as I dislike assume fixed menu positions,
//  it infinitely simplifies finding submenus when building menus at runtime
enum {
    ED_MENU_FILE,
    ED_MENU_EDIT,
    ED_MENU_VIEW,
    ED_MENU_INSERT,
    ED_MENU_FORMAT,
    ED_MENU_TABLE,
    ED_MENU_TOOLS,
    ED_MENU_COMMUNICATOR,
    ED_MENU_HELP
};

// Load a URL into a new Browser window
// This simply creates the URL struct and calls CFE_CreateNewDocWindow
MWContext * wfe_CreateNavigator(char * pURL = NULL);
/////////////////////////////////////////////////////////////////////////////
// CGenericFrame frame

class CNSGenFrame : public CFrameWnd
{
	DECLARE_DYNCREATE(CNSGenFrame)
public:
	CNSNavFrame* GetDockedNavCenter();
	virtual BOOL AllowDocking();
protected:
	afx_msg void OnActivate( UINT nState, CWnd* pWndOther, BOOL bMinimized);
	DECLARE_MESSAGE_MAP()
};

class CGenericFrame : public CNSGenFrame, public CFrameGlue
{
	DECLARE_DYNCREATE(CGenericFrame)
protected:
	CGenericFrame();           // protected constructor used by dynamic creation
	BOOL m_bPreCreated;

	CTreeMenu *m_pBookmarksMenu;
	CTreeMenu *m_pFileBookmarkMenu;
	CTreeItemList *m_BookmarksGarbageList;
	CTreeItemList *m_OriginalBookmarksGarbageList;
	CBitmap *m_pBookmarkBitmap;
	CBitmap *m_pBookmarkFolderBitmap;
	CBitmap *m_pBookmarkFolderOpenBitmap;

	CTreeMenu* m_pCachedTreeMenu;
	int m_nCachedStartPoint;

	HT_Pane m_BookmarkMenuPane; // Added for Aurora (Dave Hyatt 2/12/97)

	void BuildBookmarkMenu(CTreeMenu* pMenu, HT_Resource pRoot, int nStart = 0); 
	
	void BuildFileBookmarkMenu(CTreeMenu* pMenu, HT_Resource pRoot);
	void BuildDirectoryMenu(CMenu* pMenu);
	
	//Returns TRUE if pFolder is changed, FALSE otherwise
	virtual BOOL FileBookmark(HT_Resource pFolder);
	void InitFileBookmarkMenu(void);
	void LoadBookmarkMenuBitmaps(void);

// Menu Map 
// Everything below is for the case where a frame wants to dynamically load its
// menus.  This is done to save user resources in Win16.
protected:
#ifdef _WIN32
	CMapPtrToPtr *m_pMenuMap;
#else
	CMapWordToPtr *m_pMenuMap;
#endif

	void AddToMenuMap(int nIndex, UINT nID);
	void LoadFrameMenu(CMenu *pPopup, UINT nIndex);
	void DeleteFrameMenu(HMENU hMenu);
	void DeleteMenuMapMenus(void);
	void DeleteMenuMapIDs(void);


public:
	CTreeMenu * GetBookmarksMenu(void) { return m_pBookmarksMenu; }
	void FinishMenuExpansion(HT_Resource pRoot); 


#ifdef XP_WIN32
	// jliu added the following to support CJK caption print
	virtual int16 GetTitleWinCSID();
#endif


public:
    // ugh this should really be private
    CGenericFrame  * m_pNext;  // the next frame in the theApp.m_pFrameList
    // Use only by editor to avoid double-prompting for saving changes
    //  when we call CGenericDoc::CanCloseFrame followed by frame's OnClose()
    BOOL m_bSkipSaveEditChanges;
	BOOL m_isClosing;
	BOOL m_DockOrientation;

//	CFrameGlue required overrides
public:
	virtual CFrameWnd *GetFrameWnd();
	virtual void UpdateHistoryDialog();
	void		 BuildHelpMenu(CMenu * pMenu);

    // override this only if you want to
    virtual void OnConstructWindowMenu ( CMenu * pMenu );
	void		OnSaveConfiguration();

// Attributes
public:
	// Used for bookmark menu items and history menu items
	int				m_nBookmarkItems;	// current number of bookmark menu items
	int				m_nFileBookmarkItems;
#ifdef XP_WIN32
	CMapPtrToPtr*	m_pSubmenuMap;  	// HMENU -> BM_Entry*
#else
	CMapWordToPtr*	m_pSubmenuMap;  	// HMENU -> BM_Entry*
#endif

#ifdef XP_WIN32
	CMapPtrToPtr*	m_pFileSubmenuMap;  	// HMENU -> BM_Entry*
#else
	CMapWordToPtr*	m_pFileSubmenuMap;  	// HMENU -> BM_Entry*
#endif

    CMapWordToPtr*	m_pHotlistMenuMap;  // command ID -> URL and
										// command ID -> History_Entry*

//	We may or may not allow the window to resize.
private:
	BOOL m_bCanResize;
    CRect m_crMinMaxRect;
	BOOL m_bZOrderLocked;
	BOOL m_bBottommost;
	BOOL m_bDisableHotkeys;
	DWORD m_wAddedExStyles;
	DWORD m_wRemovedExStyles;

#ifdef XP_WIN32
	// jliu added the following to support CJK caption print
	void SetupCapFont( int16 csid, BOOL force = FALSE );
	UINT  capStyle;
	BOOL  m_bActive;
	CFont hCapFont;
	int16 m_csid;				// WinCSID of current document
#endif

	HWND  m_hPopupParent;
	BOOL  m_bConference;


public:
	void EnableResize(BOOL bEnable = TRUE);
	BOOL CanResize(){ return m_bCanResize; }
	void SetZOrder(BOOL bZLock = FALSE, BOOL bBottommost = FALSE);
	BOOL IsZOrderLocked(){ return m_bZOrderLocked; }
	BOOL IsBottommost(){ return m_bBottommost; }
	void DisableHotkeys(BOOL bDisable = FALSE);
	BOOL HotkeysDisabled(){ return m_bDisableHotkeys; }
#ifdef WIN32
	void SetExStyles(DWORD wAddedExStyles, DWORD wRemovedExStyles);
	DWORD GetRemovedExStyles();
#endif
	void SetAsPopup(HWND hPopupParent);

// Operations
public:

//	Some static public data, initialized in genframe.cpp
//  used to offset frame windows from one another
    static int      m_FirstFrame;

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CGenericFrame)
	public:
    virtual void RecalcLayout( BOOL bNotify = TRUE );
    virtual BOOL PreTranslateMessage(MSG *pMsg);
    virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	virtual BOOL OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo);
	protected:
	virtual BOOL OnCommand(WPARAM wParam, LPARAM lParam);
	//}}AFX_VIRTUAL

public:
    // assorted people can cause us to open windows.  Make these public so we
    //   can try to localize the functionality
	afx_msg void OnOpenMailWindow();
	afx_msg void OnOpenInboxWindow();
	afx_msg void OnOpenNewsWindow();
	afx_msg void OnToolsWeb();
    afx_msg void OnOpenComposerWindow(); // Implemented in edframe.cpp


	afx_msg void OnToggleEncoding(UINT nID);
	afx_msg void OnUpdateEncoding(CCmdUI* pCmdUI);

// Implementation
protected:

	virtual ~CGenericFrame();
	virtual void GetMessageString( UINT nID, CString& rMessage ) const;

	// Generated message map functions
    afx_msg void OnEnterIdle(UINT nWhy, CWnd* pWho );
    afx_msg void OnInitMenuPopup(CMenu * pPopup, UINT nIndex, BOOL bSysMenu);
	afx_msg void OnMenuSelect( UINT nItemID, UINT nFlags, HMENU hSysMenu );
	afx_msg void OnMeasureItem(int nIDCtl, LPMEASUREITEMSTRUCT lpMeasureItemStruct);
	afx_msg LRESULT OnMenuChar( UINT nChar, UINT nFlags, CMenu* pMenu );
	afx_msg LRESULT OnSetMessageString(WPARAM wParam, LPARAM lParam);
    afx_msg void OnClose();
	afx_msg void OnDestroy();
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnSysColorChange();
	afx_msg BOOL OnQueryNewPalette();
	afx_msg void OnPaletteChanged(CWnd* pFocusWnd);

	afx_msg void OnNewFrame();
	afx_msg void OnUpdateNewFrame(CCmdUI* pCmdUI);
	afx_msg void OnFileOpenurl();
	afx_msg void OnUpdateFileOpenurl(CCmdUI* pCmdUI);
	afx_msg void OnHotlistAddcurrenttohotlist();
	afx_msg void OnUpdateHotlistAddcurrenttohotlist(CCmdUI* pCmdUI);
	afx_msg void OnHotlistAddcurrenttotoolbar();
	afx_msg void OnUpdateHotlistAddcurrenttotoolbar(CCmdUI* pCmdUI);
	afx_msg void OnUpdateOpenMailWindow(CCmdUI* pCmdUI);
	afx_msg void OnUpdateOpenNewsWindow(CCmdUI* pCmdUI);
	afx_msg void OnUpdateTaskbar(CCmdUI *pCmdUI);
	afx_msg void OnAbout();
    afx_msg void OnDocumentTop();
    afx_msg void OnDocumentBottom();
	afx_msg void OnFishCam(); 
    afx_msg void OnShowBookmarkWindow();
    afx_msg void OnShowAddressBookWindow();
#if defined(JAVA) || defined(OJI)
	afx_msg void OnToggleJavaConsole();
	afx_msg void OnUpdateJavaConsole(CCmdUI* pCmdUI);
#endif
	afx_msg void OnSecurity();
	afx_msg void OnUpdateSecurity(CCmdUI *pCmdUI);
	afx_msg void OnViewCommandToolbar();
	afx_msg void OnUpdateViewCommandToolbar(CCmdUI* pCmdUI);
	afx_msg void OnViewLocationToolbar();
	afx_msg void OnUpdateViewLocationToolbar(CCmdUI* pCmdUI);
	afx_msg void OnViewCustomToolbar();
	afx_msg void OnUpdateViewCustomToolbar(CCmdUI* pCmdUI);
	afx_msg void OnViewNavCenter();
	afx_msg void OnUpdateViewNavCenter(CCmdUI* pCmdUI);
	afx_msg void OnUpdatePlaces(CCmdUI* pCmdUI);
	afx_msg void OnUpdateCustomizeToolbar(CCmdUI *pCmdUI);
	afx_msg void OnCustomizeToolbar();
	afx_msg void OnIncreaseFont();
	afx_msg void OnDecreaseFont();
	afx_msg void OnGoHistory();
	afx_msg void OnFileMailNew();
	afx_msg void OnTaskbar();
	afx_msg void OnUpdateLiveCall(CCmdUI *pCmdUI);
	afx_msg void OnLiveCall();
	afx_msg void OnCalendar();
	afx_msg void OnIBMHostOnDemand();
 	afx_msg void OnNetcaster();
	afx_msg void OnAim();
    afx_msg void OnDisplayPreferences();
    afx_msg void OnPrefsGeneral();
    afx_msg void OnPrefsMailNews();
    afx_msg void OnPrefsNetwork();
    afx_msg void OnPrefsSecurity();
	afx_msg void OnAnimationBonk();
	afx_msg void OnUpdateFileRoot(CCmdUI* pCmdUI);
	afx_msg void OnGoOffline();
	afx_msg void OnUpdateGoOffline(CCmdUI* pCmdUI);
	afx_msg void OnDoneGoingOffline();
#ifdef FORTEZZA
    afx_msg void OnStartFortezzaCard();
    afx_msg void OnStartFortezzaChange();
    afx_msg void OnStartFortezzaView();
    afx_msg void OnDoFortezzaInfo();
    afx_msg void OnDoFortezzaLog();
#endif
    afx_msg void OnFrameExit();
	afx_msg void OnSetFocus(CWnd* pOldWnd);
	afx_msg void OnWindowPosChanged(WINDOWPOS* lpwndpos);
	afx_msg void OnWindowPosChanging(WINDOWPOS* lpwndpos);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnSizing(UINT nSide, LPRECT lpRect);
	afx_msg void OnNextWindow();
    afx_msg void OnSaveOptions();
    afx_msg LRESULT OnFindReplace(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnHelpMsg(WPARAM wParam, LPARAM lParam);
	afx_msg void OnMove(int x, int y);
	afx_msg void OnFilePageSetup();
	afx_msg void OnGetMinMaxInfo(MINMAXINFO FAR* lpMMI);
	afx_msg LONG OnTaskNotify(WPARAM wParam, LPARAM lParam);
	afx_msg void OnShowAdvanced();
	afx_msg void OnUpdateShowAdvanced( CCmdUI *pCmdUI );
    afx_msg void OnHelpMenu();
    afx_msg void OnLDAPSearch();
	afx_msg void OnPageFromWizard();
#ifdef EDITOR
	afx_msg void OnEditNewBlankDocument();
    afx_msg void OnActivateSiteManager();
    afx_msg void OnUpdateActivateSiteManager(CCmdUI* pCmdUI);
    // Response to Registered messages to detect 1st instance
    afx_msg LRESULT OnNetscapeGoldIsActive(WPARAM wParam, LPARAM lParam);
    // Response to Registered messages to open Editor or Navigator
    //  from attempted 2nd instance or Site Manager
    afx_msg LRESULT OnOpenEditor(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnOpenNavigator(WPARAM wParam, LPARAM lParam);
#ifdef XP_WIN32
    afx_msg LRESULT OnSiteMgrMessage(WPARAM wParam, LPARAM lParam);
#endif
    // These are hooked up to menu commands...
    afx_msg void OnNavigateToEdit();
    afx_msg void OnEditFrame();
    // ...and call this after setting appropriate m_iLoadUrlEditorState
    void OpenEditorWindow(int iStyle);
    
    // This is used for all of above - prevent interaction
    //  with menus when no context, edit buffer, etc exists
    afx_msg void OnUpdateCanInteract(CCmdUI* pCmdUI);
	afx_msg void OnUpdateEditFrame(CCmdUI* pCmdUI);
    afx_msg void OnEditNewDocFromTemplate();
#endif


#ifdef _WIN32
    afx_msg LONG OnHackedMouseWheel(WPARAM wParam, LPARAM lParam);
	// jliu added the following to support CJK caption print
	afx_msg void OnNcPaint();
	afx_msg BOOL OnNcActivate( BOOL );
	afx_msg LRESULT OnSetText(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnSettingChange(WPARAM, LPARAM);
#endif
	afx_msg void OnActivate( UINT nState, CWnd* pWndOther, BOOL bMinimized);
#ifdef DEBUG_WHITEBOX
	afx_msg void OnWhiteBox();
#endif
	DECLARE_MESSAGE_MAP()

#ifdef EDITOR
public:
    // Load a URL into a new edit window,
    //  OR the existing window if bNewWindow is FALSE
    //     (used only when Composer is first window)
    void LoadUrlEditor(char * pURL = NULL,
                       BOOL bNewWindow = TRUE,
                       int iLoadStyle = 0, 
                       MWContext * pCopyHistoryContext = NULL);

    // Called from Edit view only after CheckAndSaveDocument
    void OpenNavigatorWindow(MWContext * pMWContext);

    // Currently used only when canceling an already-loaded URL
    //   in editor and user doesn't want to save to disk
    //   (replaces editor with a browser with the same URL)
	void EditToNavigate(MWContext * pEditContext, BOOL bNewDocument );
    
    // Put all Composer-only code here - called only from CGenericFrame::OnMenuSelect()
    void OnMenuSelectComposer(UINT nItemID, UINT nFlags, HMENU hSysMenu);
    BOOL m_bCloseFrame;

#endif

	virtual void RefreshNewEncoding(int16 csid, BOOL bIgnore=TRUE);
};

#ifdef EDITOR
// Global helpers so CEditFrame and CComposeFrame (mail message) can share 
//   dynamic menu functionality
BOOL edt_GetMessageString(CView *pView, UINT MenuId, CString& Message);
BOOL edt_IsEditorDynamicMenu(WPARAM wParam);
#endif

CGenericFrame *wfe_FrameFromXPContext(MWContext * pXPCX);

static UINT NEAR WM_FINDREPLACE = ::RegisterWindowMessage(FINDMSGSTRING);
static UINT NEAR WM_HELPMSG = ::RegisterWindowMessage(HELPMSGSTRING);
#endif
