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

#ifndef _MAILPRIV_H
#define _MAILPRIV_H

#include "dateedit.h"
#include "widgetry.h"
#include "mailmisc.h"
#include "statbar.h"
#include "property.h"

///////////////////////////////////////////////////////////////////////
// CMarkReadDateDlg 

class CDiskSpacePropertyPage;
class CDownLoadPPNews;
class CDownLoadPPMail;


class CMarkReadDateDlg: public CDialog {
protected:
	CNSDateEdit wndDateTo;

public:
	CTime dateTo;
	enum { IDD = IDD_MARKREADDATE };

	CMarkReadDateDlg( UINT nIDTemplate, CWnd* pParentWnd ):
		CDialog( nIDTemplate, pParentWnd ) {}

	virtual BOOL OnInitDialog( );
	virtual void OnOK();
};

///////////////////////////////////////////////////////////////////////
// CNavCombo

class CNavCombo: public CMailFolderCombo {
friend class CFolderInfoBar;

protected:
	BOOL m_bFirst;
	RECT m_rcList;

	HFONT m_hFont, m_hBigFont;
public:
	CNavCombo();
	~CNavCombo();

	virtual void SetFont( CFont *pFont, CFont *pBigFont );

protected:

	virtual void DrawItem( LPDRAWITEMSTRUCT lpDrawItemStruct );
	
	afx_msg void OnPaint( );
	afx_msg HBRUSH OnCtlColor( CDC* pDC, CWnd* pWnd, UINT nCtlColor );
	DECLARE_MESSAGE_MAP()
};

///////////////////////////////////////////////////////////////////////
// CMailInfoBar

#ifdef XP_WIN16
class CMailInfoBar: public CNetscapeControlBar {
#else
class CMailInfoBar: public CControlBar {
#endif

protected:
	BOOL m_bEraseBackground;

	// Attributes
	int m_iCSID;
	HFONT m_hFont, m_hBoldFont;
	HFONT m_hIntlFont, m_hBoldIntlFont;

	MSG_Pane *m_pPane;
	int m_idxImage;

	CNSToolTip2 m_wndToolTip;

	HBITMAP m_hbmBanner;
	SIZE m_sizeBanner;

    LPUNKNOWN m_pUnkImage;
    LPIMAGEMAP m_pIImage;

public:
	CMailInfoBar();
	virtual ~CMailInfoBar();

	BOOL Create( CWnd *pWnd, MSG_Pane *pPane );
	void SetPane( MSG_Pane *pPane ) { m_pPane = pPane; }
	virtual void SetCSID(int csid);
	virtual void Update() {};

protected:
	virtual void DragProxie();

	void DrawInfoText( HDC hdc, LPCSTR lpText, LPRECT rect );
	void DrawInfoText( int iCSID, HDC hdc, LPCSTR lpText, LPRECT rect );
	void MeasureInfoText( HDC hdc, LPCSTR lpText, LPRECT rect );
	void MeasureInfoText( int iCSID, HDC hdc, LPCSTR lpText, LPRECT rect );

	void PaintBackground( HDC hdc );

	virtual void OnUpdateCmdUI( CFrameWnd* pTarget, BOOL bDisableIfNoHndler );
	virtual CSize CalcFixedLayout( BOOL bStretch, BOOL bHorz );
	virtual BOOL PreTranslateMessage(MSG *pMsg);

	afx_msg int OnCreate(LPCREATESTRUCT lpcs);
	afx_msg void OnLButtonDown( UINT nFlags, CPoint point );
	afx_msg void OnLButtonUp( UINT nFlags, CPoint point );
	afx_msg void OnMouseMove( UINT nFlags, CPoint point );
	afx_msg void OnShowWindow( BOOL bShow, UINT nStatus );
	afx_msg BOOL OnEraseBkgnd( CDC* pDC );
	afx_msg BOOL OnSetCursor( CWnd* pWnd, UINT nHitTest, UINT message );
	DECLARE_MESSAGE_MAP()
};

///////////////////////////////////////////////////////////////////////
// CContainerInfoBar

class CContainerInfoBar: public CMailInfoBar {
protected:
	// Attributes
	CString m_csBanner;

public:
	virtual void Update();

protected:
	virtual void DragProxie();
  
	afx_msg int OnCreate(LPCREATESTRUCT lpcs);
	afx_msg void OnPaint( );
	DECLARE_MESSAGE_MAP()
};

///////////////////////////////////////////////////////////////////////
// CFolderInfoBar

class CFolderInfoBar: public CMailInfoBar {
protected:
	// Attributes
	MSG_FolderInfo *m_folderOld;
	CString m_csFolderName;
	CString m_csFolderCounts;

	CNavCombo m_wndNavButton;
	CCommandToolbarButton m_wndBackButton;
	
public:
	CFolderInfoBar();
	virtual ~CFolderInfoBar();

	virtual void SetCSID(int csid);
	virtual void Update();

protected:
	virtual void DragProxie();

	afx_msg int OnCreate( LPCREATESTRUCT lpCreateStruct );
	afx_msg void OnSize( UINT nType, int cx, int cy );
	afx_msg void OnDropDown();
	afx_msg void OnCloseUp();
	afx_msg void OnContainer();
	afx_msg void OnPaint( );
	DECLARE_MESSAGE_MAP()
};

///////////////////////////////////////////////////////////////////////
// CMessageInfoBar

class CMessageInfoBar: public CMailInfoBar {
protected:
	// Attributes
	CString m_csMessageName;
	CString m_csMessageAuthor;
	CString m_csFolderTip;
	CString m_csFolderStatus;

	CCommandToolbarButton m_wndBackButton;

public:
	CMessageInfoBar();
	virtual ~CMessageInfoBar();

	virtual void SetCSID(int csid);
	virtual void Update();

protected:
	virtual void DragProxie();

	afx_msg int OnCreate( LPCREATESTRUCT lpCreateStruct );
	afx_msg void OnSize( UINT nType, int cx, int cy );
	afx_msg void OnPaint( );
	afx_msg void OnContainer();
	DECLARE_MESSAGE_MAP()
};

///////////////////////////////////////////////////////////////////////
//CFolderPropertyPage
//Mail folder property page.
class CNewsFolderPropertySheet;
class CFolderPropertyPage: public CNetscapePropertyPage {

protected:
	MSG_FolderInfo *m_folderInfo;
	MSG_Pane *m_pPane;
	CNewsFolderPropertySheet *m_pParent;

public:
	enum { IDD = IDD_PP_FOLDER };
	CString m_strFolderName;

	CFolderPropertyPage(CWnd *pWnd = NULL);
	void SetFolderInfo(MSG_FolderInfo *folderInfo, MSG_Pane *pPane);

	virtual BOOL OnInitDialog();
	virtual void OnOK();
protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	afx_msg void OnChangeFolderName();
	afx_msg void OnCleanUpWastedSpace();

	DECLARE_MESSAGE_MAP()

};

///////////////////////////////////////////////////////////////////////
//CFolderSharingPage
//Mail folder property page.
class CFolderSharingPage: public CNetscapePropertyPage {

protected:
	MSG_FolderInfo *m_folderInfo;
	MWContext *m_pContext;
	MSG_Pane *m_pPane;
	CNewsFolderPropertySheet *m_pParent;

public:
	enum { IDD = IDD_PP_SHARING };

	CFolderSharingPage(CWnd *pWnd = NULL);
	void SetFolderInfo(MSG_FolderInfo *folderInfo, MSG_Pane *pPane, MWContext *pContext);

	virtual BOOL OnInitDialog();
	virtual void OnOK();
protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	afx_msg void OnClickPrivileges();
	DECLARE_MESSAGE_MAP()

};

///////////////////////////////////////////////////////////////////////
///News Host property page.
class CNewsHostGeneralPropertyPage: public CNetscapePropertyPage {

protected:
	MSG_FolderInfo *m_folderInfo;
	MSG_NewsHost	*m_pNewsHost;
public:
	CNewsHostGeneralPropertyPage();

	//dialog data
	enum { IDD = IDD_PP_NEWSHOST_GENERAL };
	int		m_nRadioValue;
	BOOL	m_bCanReceiveHTML;
	//end dialog data 

	void SetFolderInfo(MSG_FolderInfo *folderInfo, MSG_NewsHost *pNewsHost);
public:
	virtual void OnOK();
	virtual BOOL OnInitDialog();

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	DECLARE_MESSAGE_MAP()

};

//forward
class CNewsFolderPropertySheet;

/////////////////////////////////////////////////////////////////////////
//General page for news folders

class CNewsGeneralPropertyPage: public CNetscapePropertyPage {

protected:
	MSG_FolderInfo *m_folderInfo;
	MWContext *m_pContext;
	CNewsFolderPropertySheet *m_pParent;
public:
	CNewsGeneralPropertyPage(CNewsFolderPropertySheet *pParent);

	//Dialog Data
	enum { IDD = IDD_PP_NEWS_GENERAL };
	BOOL m_bCanReceiveHTML;
	//End Dialog Data

	void SetFolderInfo(MSG_FolderInfo *folderInfo, MWContext *pContext);

public://virtuals
	virtual void OnOK();
	virtual BOOL OnInitDialog();

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	afx_msg void OnDownLoadButton();
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////
//CNewsFolderPropertySheet
//Used in Edit Properties of the MailFolder frame.
class CNewsFolderPropertySheet : public CNetscapePropertySheet
{
public:
	LOGFONT m_LogFont;
	CFont m_Font;

	BOOL m_bDownLoadNow;
	BOOL m_bSynchronizeNow;
	BOOL m_bCleanUpNow;

	CWnd * m_pParent;
public:

    CNewsFolderPropertySheet(LPCTSTR pszCaption, CWnd *pParent);
    ~CNewsFolderPropertySheet();
    
	BOOL DownLoadNow() {return m_bDownLoadNow;};
	BOOL SynchronizeNow() {return m_bSynchronizeNow;};
	BOOL CleanUpNow()	{return m_bCleanUpNow;};
	void CleanUpWastedSpace();

	virtual void OnHelp();

public:	             
	CNewsGeneralPropertyPage	 *m_pNewsFolderPage;
	CFolderPropertyPage			 *m_pFolderPage;
	CFolderSharingPage			 *m_pSharingPage;
    CDiskSpacePropertyPage		 *m_pDiskSpacePage;
    CDownLoadPPNews				 *m_pDownLoadPageNews;
	CDownLoadPPMail				 *m_pDownLoadPageMail;
	CNewsHostGeneralPropertyPage *m_pNewsHostPage;
public:	

	afx_msg void OnDownLoadButton();
	afx_msg void OnSynchronizeButton();

	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////
//General page for attachments

class CAttachmentGeneralPage: public CNetscapePropertyPage {
protected:
	CString m_csName;
	CString m_csType;
	CString m_csDescription;

	enum { IDD = IDD_PP_ATTACHMENT_GENERAL };

public:
	CAttachmentGeneralPage(LPCTSTR lpszName, LPCTSTR lpszType, LPCTSTR lpszDescription);

	virtual void DoDataExchange(CDataExchange* pDX);
};

/////////////////////////////////////////////////////////////////////////
// Property sheet for attachments

class CAttachmentSheet: public CNetscapePropertySheet {
protected:
	CAttachmentGeneralPage *m_pGeneral;

	CString m_csName;
	CString m_csType;
	CString m_csDescription;

public:
	CAttachmentSheet(CWnd *pParentWnd,
					 LPCTSTR lpszName, LPCTSTR lpszType, LPCTSTR lpszDescription);
	~CAttachmentSheet();
};

/////////////////////////////////////////////////////////////////////
//
// CThreadStatusBar
//
// Status bar with little "expando" widget on the left
//

class CThreadStatusBar: public CNetscapeStatusBar {

protected:
	HBITMAP m_hbmExpando;
	SIZE m_sizeExpando;

	BOOL m_bExpandoed, m_bDepressed;

// Mode state info for particular pane modes   
private:
    int   m_iStatBarPaneWidth; // eSBM_Expando: save the width of the Taskbar pane

public:
	CThreadStatusBar();
	~CThreadStatusBar();

	BOOL Create( CWnd *pParent );

	void Expando( BOOL bExpando );

// CNetscapeStatusBar overrides
protected:
   virtual void SetupMode();

protected:
	afx_msg void OnLButtonDown( UINT nFlags, CPoint point );
	afx_msg void OnMouseMove( UINT nFlags, CPoint point );
	afx_msg void OnLButtonUp( UINT nFlags, CPoint point );
	afx_msg void OnPaint();
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////
//
// CProgressDialog
//
// Dialog for stand-along mail downloading
//

typedef void (*PROGRESSCALLBACK)(HWND,MSG_Pane *, void*);
typedef BOOL (*SHOWPROGRESSCALLBACK)(HWND, MSG_Pane*, void*);

#define	WM_REQUESTPARENT	WM_USER+1442

class CProgressDialog: public CDialog, public CStubsCX, public IMailFrame {
protected:
	int32 m_lPercent;
	CProgressMeter m_progressMeter;
	MSG_Pane *m_pPane;
	CWnd * m_pParent;
	char * m_pszTitle;
	void *m_closure;
	PROGRESSCALLBACK m_cbDone;
	UINT m_uTimerId;       
	UINT m_uProgressPos;
	BOOL m_bProgressShown;

public:
   CProgressDialog( CWnd *pParent, 
      MSG_Pane *parentPane, 
      PROGRESSCALLBACK callback, void * closure = NULL,
	  char * pszTitle = NULL,
	  PROGRESSCALLBACK cbDone = NULL, 
	  SHOWPROGRESSCALLBACK showCallback = NULL);
   ~CProgressDialog() {
      if (m_pszTitle)
         XP_FREE(m_pszTitle);
      }

	enum { IDD = IDD_NEWMAIL };

// IUnknown Interface
	STDMETHODIMP			QueryInterface(REFIID,LPVOID *);
	STDMETHODIMP_(ULONG)	AddRef(void);
	STDMETHODIMP_(ULONG)	Release(void);

// Support for IMailFrame
	virtual CMailNewsFrame *GetMailNewsFrame() { return NULL; }
	virtual MSG_Pane *GetPane() { return m_pPane; }
	virtual void PaneChanged( MSG_Pane *pane, XP_Bool asynchronous, 
							  MSG_PANE_CHANGED_NOTIFY_CODE, int32 value);
	virtual void AttachmentCount(MSG_Pane *messagepane, void* closure,
								 int32 numattachments, XP_Bool finishedloading);
	virtual void UserWantsToSeeAttachments(MSG_Pane *messagepane, void *closure);

protected:
	virtual BOOL OnInitDialog( );
	virtual void OnCancel();

	virtual void PostNcDestroy( ) {} // Prevent window destroy auto delete.

	afx_msg void OnDestroy();
	afx_msg LONG OnRequestParent(WPARAM,LPARAM);
	afx_msg void OnTimer(UINT nIDEvent);
	DECLARE_MESSAGE_MAP()


public:
	int32 QueryProgressPercent();
	void SetProgressBarPercent(MWContext *pContext, int32 lPercent);

	void SetDocTitle( MWContext *pContext, char *pTitle );

	void StartAnimation();
	void StopAnimation();

	void Progress(MWContext *pContext, const char *pMessage);
	void AllConnectionsComplete(MWContext *pContext);

	void UpdateStopState( MWContext *pContext );
    
	CWnd *GetDialogOwner() const;
	// returns TRUE if we get past the test callback function
	// used to determine if we should destruct now.
	BOOL GetProgressShown() {return m_bProgressShown;}
};

/////////////////////////////////////////////////////////////////////
//
// COfflineProgressDialog
//
// Progress for offline synchronizing
//

class COfflineProgressDialog: public CProgressDialog {

protected:
	BOOL m_bQuitOnCompletion;

public:
   COfflineProgressDialog( CWnd *pParent, 
      MSG_Pane *parentPane, 
      PROGRESSCALLBACK callback, void * closure = NULL,
	  char * pszTitle = NULL,
	  PROGRESSCALLBACK cbDone = NULL, BOOL bQuitOnCompletion = FALSE);

	  void AllConnectionsComplete(MWContext *pContext);

protected:

};

/////////////////////////////////////////////////////////////////////
//
// CNewFolderDialog
//
// Dialog for mail folder creation
//

class CNewFolderDialog: public CDialog, public IMailFrame {
protected:
	MSG_Pane *m_pPane;
	MSG_FolderInfo *m_pParentFolder;
	CMailFolderCombo m_wndCombo;
	BOOL m_bEnabled;

public:
	CNewFolderDialog( CWnd *pParent, MSG_Pane *pPane, MSG_FolderInfo *folderInfo );

// IUnknown Interface
	STDMETHODIMP			QueryInterface(REFIID,LPVOID *);
	STDMETHODIMP_(ULONG)	AddRef(void);
	STDMETHODIMP_(ULONG)	Release(void);

// Support for IMailFrame
	virtual CMailNewsFrame *GetMailNewsFrame() { return NULL; }
	virtual MSG_Pane *GetPane() { return m_pPane; }
	virtual void PaneChanged( MSG_Pane *pane, XP_Bool asynchronous, 
							  MSG_PANE_CHANGED_NOTIFY_CODE, int32 value);
	virtual void AttachmentCount(MSG_Pane *messagepane, void* closure,
								 int32 numattachments, XP_Bool finishedloading);
	virtual void UserWantsToSeeAttachments(MSG_Pane *messagepane, void *closure);
	
	enum { IDD = IDD_NEWFOLDER };

protected:
	virtual BOOL OnInitDialog( );
	virtual void OnCancel();
	virtual void OnOK();

	DECLARE_MESSAGE_MAP()
	afx_msg void OnDestroy();
	afx_msg void OnEnable( CCmdUI *pCmdUI );
};

/////////////////////////////////////////////////////////////////////
//
// CPrefNewFolderDialog
//
// Dialog for mail folder creation in preference
//

class CPrefNewFolderDialog: public CDialog
{
protected:
	MSG_FolderInfo *m_pFolder;
	CMailFolderCombo m_wndCombo;
	BOOL m_bCreating;

public:
	CPrefNewFolderDialog( CWnd *pParent, MSG_FolderInfo *pFolderInfo );
	MSG_FolderInfo *GetNewFolder() { return m_pFolder; }

	enum { IDD = IDD_NEWFOLDER };

protected:
	virtual BOOL OnInitDialog( );
	virtual void OnCancel();
	virtual void OnOK();
	virtual BOOL OnCommand(WPARAM wParam, LPARAM lParam);


	DECLARE_MESSAGE_MAP()
};

///////////////////////////////////////////////////////////////////////
// CMailNewsSplitter
// A view window can have max 2 panes.  Split vertically or horizontally
// Capable of adding or deleting(or hiding/closing) one pane on the fly
// 

class CMailNewsSplitter: public CView {
	DECLARE_DYNCREATE(CMailNewsSplitter);

protected:

	BOOL m_bEraseBackground;
	HBRUSH	m_hSliderBrush;

	CMailNewsFrame* m_pNotifyFrame;
	CWnd 	*m_pWnd1, *m_pWnd2;
	BOOL	m_bVertical;
	BOOL	m_bTrackSlider;
	BOOL	m_bMouseMove;
	BOOL	m_bZapperDown;
	BOOL	m_bZapped;
	BOOL	m_bDoubleClicked;
	BOOL	m_bLoadMessage;
	RECT	m_rcSlider;
	POINT	m_ptHit;
	POINT	m_ptFirstHit;

	int		m_nSliderWidth;
	int		m_nPaneSize;
	int		m_nPrevSize;
 
    CMailNewsSplitter();
		
public:

	~CMailNewsSplitter();

	void SetNotifyFrame(CMailNewsFrame* pFrame = NULL) { m_pNotifyFrame = pFrame; }
	void SetLoadMessage(BOOL bLoad = FALSE) { m_bLoadMessage = bLoad; }
	void AddPanes(CWnd *pWnd1, CWnd *pWnd2 = NULL, int nSize = -1, BOOL bVertical = TRUE);
	void AddOnePane(CWnd *pWnd, BOOL bFirstPane = FALSE, BOOL bVertical = TRUE);
	void RemoveOnePane(CWnd *pWnd);
	void SetPaneSize(CWnd *pWnd, int nSize);
	int	 GetPaneSize();
	int  GetPreviousPaneSize();


	BOOL IsOnePaneClosed() const;
	void SetSliderWidth(int nWidth) { m_nSliderWidth = nWidth;	}

protected:

	void UpdateSplitter();
	BOOL IsInZapper(POINT point);
	void DeleteBitmaps();
	void CreateBitmaps(HDC hDC);
	void LoadingMessage();
	void CheckFocusWindow();
	void UpdateZapper();

	virtual void PositionWindows(int cx, int cy);
	virtual void InvertSlider(RECT* pRect);
	virtual void SetSliderRect(int cx, int cy);

	virtual void OnInitialUpdate();
	virtual BOOL PreTranslateMessage( MSG* pMsg );
    virtual void OnDraw(CDC *pDC);

	afx_msg int OnCreate( LPCREATESTRUCT lpCreateStruct );
	afx_msg void OnSetFocus( CWnd* pOldWnd );
	afx_msg void OnLButtonDown( UINT nFlags, CPoint point );
	afx_msg void OnMouseMove( UINT nFlags, CPoint point ); 
	afx_msg void OnLButtonUp( UINT nFlags, CPoint point );
	afx_msg void OnLButtonDblClk( UINT nFlags, CPoint point );
	afx_msg BOOL OnSetCursor( CWnd* pWnd, UINT nHitTest, UINT message );
	afx_msg void OnSize( UINT nType, int cx, int cy );
	afx_msg void OnShowWindow( BOOL bShow, UINT nStatus );
	afx_msg BOOL OnEraseBkgnd( CDC* pDC );
	afx_msg int	OnMouseActivate(CWnd* pDesktopWnd, UINT nHitTest, UINT message);
	DECLARE_MESSAGE_MAP()
};

#endif // _MAILPRIV_H
