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

#ifndef __MAILMISC_H
#define __MAILMISC_H

#include "outliner.h"
#include "msgcom.h"

#define ID_COLUMN_FROM          1
#define ID_COLUMN_SUBJECT       2
#define ID_COLUMN_DATE          3
#define ID_COLUMN_READ          4
#define ID_COLUMN_THREAD		5
#define ID_COLUMN_PRIORITY		6
#define ID_COLUMN_STATUS		7
#define ID_COLUMN_LENGTH		8
#define ID_COLUMN_COUNT         9    
#define ID_COLUMN_UNREAD        10
#define ID_COLUMN_FOLDER        11
#define ID_COLUMN_MARK			12
#define ID_COLUMN_SLAMMER		13
 
// array of indexes for IDB_MAILNEWS
#define IDX_LOCALMAIL			0
#define IDX_REMOTEMAIL			1
#define IDX_NEWSHOST			2
#define IDX_MAILMESSAGE			3
#define IDX_MAILREAD			4
#define IDX_DRAFT				5
#define IDX_NEWSGROUP			6
#define IDX_NEWSARTICLE			7
#define IDX_NEWSGROUPNEW		8
#define IDX_NEWSNEW				9
#define IDX_NEWSREAD			10
#define IDX_UNREAD				11
#define IDX_READ				12
#define IDX_MARKED				13
#define IDX_CHECKMARK			14
#define IDX_CHECKBOX			15
#define IDX_MAILFOLDERCLOSED	16
#define IDX_MAILFOLDEROPEN		17
#define IDX_MAILFOLDERCLOSEDNEW	18
#define IDX_MAILFOLDEROPENNEW	19
#define IDX_INBOXCLOSED			20
#define IDX_INBOXOPEN			21
#define IDX_INBOXCLOSEDNEW		22
#define IDX_INBOXOPENNEW		23
#define IDX_OUTBOXCLOSED		24
#define IDX_OUTBOXOPEN			25
#define IDX_SENTCLOSED			26
#define IDX_SENTOPEN			27
#define IDX_DRAFTSCLOSED		28
#define IDX_DRAFTSOPEN			29
#define IDX_TRASH				30
#define IDX_TRASHOPEN			31
#define IDX_THREADCLOSED		32
#define IDX_THREADOPEN			33
#define	IDX_THREADCLOSEDNEW		34
#define IDX_THREADOPENNEW		35
#define IDX_THREADWATCHED		36
#define IDX_THREADWATCHEDNEW	37
#define IDX_TWISTYOPEN			38
#define IDX_TWISTYCLOSED		39
#define IDX_MAILNEW				40
#define IDX_COLLECTION			41
#define IDX_CONTAINER			42
#define IDX_SECNEWS				43
#define IDX_THREADIGNOREDCLOSED	44
#define IDX_THREADIGNOREDOPEN	45
#define IDX_OFFLINENEWS			46
#define IDX_FOLDEROPENOFFLINE	47
#define IDX_FOLDERCLOSEDOFFLINE	48
#define IDX_MAILOFFLINE			49
#define IDX_NEWSHOSTOFFLINE		50
#define IDX_NEWSMSGOFFLINE		51
#define IDX_NEWSREADOFFLINE		52
#define IDX_MAILREADOFFLINE		53
#define IDX_MAILMSGOFFLINE		54
#define IDX_IMAPMSGDELETED		55
#define IDX_TEMPLATECLOSE		56
#define IDX_TEMPLATEOPEN		57
#define IDX_TEMPLAT 			58
#define IDX_SHARECLOSED			59
#define IDX_SHAREOPEN			60
#define IDX_PUBLICCLOSED		61
#define IDX_PUBLICOPEN			62
#define IDX_ATTACHMENTMAIL		63

// array of indexes for IDB_MAILCOL
#define IDX_THREAD              0
#define IDX_MAILCHECKMARK       1
#define IDX_MAILMARKED          2
#define IDX_MAILUNREAD          3
#define IDX_UNTHREAD			4

#define NETSCAPE_MESSAGE_FORMAT "Netscape Message"
#define NETSCAPE_FOLDER_FORMAT "Netscape Folder"
#define NETSCAPE_SEARCH_FORMAT "Netscape Search"

int WFE_MSGTranslateFolderIcon( uint8 level, int32 iFlags, BOOL bOpen );
void WFE_MSGBuildMessagePopup( HMENU hmenu, BOOL bNews, BOOL bInHeaders = FALSE ,
							   MWContext *pContext = NULL);

///////////////////////////////////////////////////////////////////////
// CMessagePrefs

class CMsgPrefs {
private:
	MSG_Master *m_pMaster;

public:
	CMsgPrefs();
	~CMsgPrefs();

	void Init();
	void Shutdown();

	BOOL IsValid() const;
	MSG_Master *GetMaster();
	MSG_Master *GetMasterValue();

	CGenericDocTemplate *m_pFolderTemplate;
	CGenericDocTemplate *m_pThreadTemplate;
	CGenericDocTemplate *m_pMsgTemplate;

	// Lib Msg prefs structure
	MSG_Prefs *m_pMsgPrefs;

	// Biff Context
    MWContext *m_pBiffContext;

	CString m_csMailDir;
	CString m_csMailHost;
	CString m_csNewsDir;
	CString m_csNewsHost;
	CString m_csUsersEmailAddr;
	CString m_csUsersFullName;
	CString m_csUsersOrganization;
	LPSTR m_pszUserSig;

	BOOL m_bInitialized;
	BOOL m_bThreadPaneMaxed;
	BOOL m_bMessageReuse;
	BOOL m_bThreadReuse;
	BOOL m_bShowCompletionPicker;
};

extern CMsgPrefs g_MsgPrefs;

typedef struct {
	MSG_Pane *m_pane;
	int m_count;
	MSG_ViewIndex *m_indices;
} MailNewsDragData;

///////////////////////////////////////////////////////////////////////
// CMailNewsOutliner

class CMailNewsOutliner: public CMSelectOutliner 
{
protected:
	MSG_Pane *m_pPane;
	int m_iMysticPlane;
	BOOL m_bSelChanged;
	int m_iSelBlock;
	BOOL m_bExpandOrCollapse;
	int  m_nCurrentSelected;

public:
	CMailNewsOutliner();
	~CMailNewsOutliner();

	virtual void SetPane( MSG_Pane *pane );
	MSG_Pane *GetPane() { return m_pPane; }
	void SetCurrentSelected(int nIndex) { m_nCurrentSelected = nIndex; }
	int	 GetCurrentSelected() { return m_nCurrentSelected; }

	virtual void MysticStuffStarting( XP_Bool asynchronous,
									 MSG_NOTIFY_CODE notify, 
								     MSG_ViewIndex where,
									 int32 num);
	virtual void MysticStuffFinishing( XP_Bool asynchronous,
									  MSG_NOTIFY_CODE notify, 
								      MSG_ViewIndex where,
									  int32 num);

	int GetMysticStuffDepth() const { return m_iMysticPlane; }

	void BlockSelNotify(BOOL bBlock) { m_iSelBlock += (bBlock ? 1 : -1); }

protected:
	// Basic Overrides
    virtual int ToggleExpansion( int iLine );
    virtual int Expand( int iLine );
    virtual int Collapse( int iLine );

	afx_msg BOOL OnSetCursor( CWnd* pWnd, UINT nHitTest, UINT message );
	afx_msg void OnLButtonDown( UINT nFlags, CPoint point );
	afx_msg void OnRButtonDown( UINT nFlags, CPoint point );
	afx_msg void OnKeyDown( UINT nChar, UINT nRepCnt, UINT nFlags );
	afx_msg void OnKeyUp( UINT nChar, UINT nRepCnt, UINT nFlags );
	DECLARE_MESSAGE_MAP()
};

///////////////////////////////////////////////////////////////////////
// CFolderOutliner

class CFolderOutliner : public CMailNewsOutliner
{
protected:
    MSG_FolderLine m_FolderLine;
    OutlinerAncestorInfo * m_pAncestor;
    LPTSTR m_pszExtraText;
    CLIPFORMAT m_cfMessages, m_cfFolders, m_cfSearchMessages;

    DWORD m_dwPrevTime; 
    UINT m_uTimer; 
	BOOL m_bDoubleClicked;
	BOOL m_b3PaneParent;
	BOOL m_bRButtonDown;

	MSG_FolderInfo **m_pMysticStuff;
	MSG_FolderInfo *m_MysticFocus;
	MSG_FolderInfo *m_MysticSelection;

public:
    CFolderOutliner ( );
    ~CFolderOutliner ( );
	
	BOOL IsParent3PaneFrame() { return m_b3PaneParent; }

	virtual void MysticStuffStarting( XP_Bool asynchronous,
									 MSG_NOTIFY_CODE notify, 
								     MSG_ViewIndex where,
									 int32 num );
	virtual void MysticStuffFinishing( XP_Bool asynchronous,
									  MSG_NOTIFY_CODE notify, 
								      MSG_ViewIndex where,
									  int32 num );

protected:
	virtual void OnSelChanged();
	virtual void OnSelDblClk();

	// Tree Info stuff
	virtual int GetDepth( int iLine );
	virtual int GetNumChildren( int iLine );
	virtual BOOL IsCollapsed( int iLine );

    virtual LPCTSTR GetColumnText( UINT iColumn, void * pLineData );
    virtual LPCTSTR GetColumnTip( UINT iColumn, void * pLineData );
    virtual void * AcquireLineData( int iLine );
    virtual void ReleaseLineData( void * pLineData );
    virtual void GetTreeInfo( int iLine, uint32 * pFlags, int * iDepth, 
							   OutlinerAncestorInfo ** pAncestor );
    virtual HFONT GetLineFont ( void * pLineData );
    virtual int TranslateIcon ( void *);
    virtual int TranslateIconFolder ( void *);

    virtual void PropertyMenu ( int iSel, UINT flags );

	// Drag-Drop Stuff
	// clip format
    virtual void InitializeClipFormats(void);
    virtual CLIPFORMAT * GetClipFormatList(void);

	// initiate drag
    virtual COleDataSource * GetDataSource(void);

	// accept drop
	virtual DROPEFFECT DropSelect(int iLineNo, COleDataObject *object);
    virtual void AcceptDrop(int iLineNo, COleDataObject *object, DROPEFFECT effect);

	afx_msg int OnCreate(LPCREATESTRUCT lpcs);
	afx_msg void OnSetFocus(CWnd* pOldWnd);
	afx_msg void OnTimer(UINT nIDEvent);
	afx_msg void OnDestroy();
    afx_msg void OnRButtonDown( UINT nFlags, CPoint pt );
    afx_msg void OnRButtonUp( UINT nFlags, CPoint pt );
   DECLARE_MESSAGE_MAP()
};

///////////////////////////////////////////////////////////////////////
// CMessageOutliner

class CMessageOutliner : public CMailNewsOutliner
{
protected:
    MSG_MessageLine m_MessageLine;
    OutlinerAncestorInfo * m_pAncestor;
    LPTSTR m_pszExtraText;
    CLIPFORMAT m_cfMessages;

	MessageKey *m_pMysticStuff;
	MessageKey m_MysticFocus;
	MessageKey m_MysticSelection;
	BOOL m_bNews, m_bDrafts;

    UINT m_uTimer; 
	BOOL m_bDoubleClicked;
    DWORD m_dwPrevTime; 

public:
    CMessageOutliner ( );
    ~CMessageOutliner ( );

	virtual void MysticStuffStarting( XP_Bool asynchronous,
									 MSG_NOTIFY_CODE notify, 
								     MSG_ViewIndex where,
									 int32 num);
	virtual void MysticStuffFinishing( XP_Bool asynchronous,
									  MSG_NOTIFY_CODE notify, 
								      MSG_ViewIndex where,
									  int32 num);

	void SelectThread( int iLine, UINT flags = 0 );
	void SelectFlagged();

	BOOL IsNews() const { return m_bNews; }
	void SetNews(BOOL bNews) { m_bNews = bNews;}

	void EnsureFlagsVisible();

	void SelectAllMessages();

protected:
	virtual void OnSelChanged();
	virtual void OnSelDblClk();

	// Tree Info stuff
	virtual int GetDepth( int iLine );
	virtual int GetNumChildren( int iLine );
	virtual int GetParentIndex( int iLine );
	virtual BOOL IsCollapsed( int iLine );

	// Tree manip stuff
	virtual int ExpandAll( int iLine );
	virtual int CollapseAll( int iLine );

    virtual LPCTSTR GetColumnText ( UINT iColumn, void * pLineData );
    virtual void * AcquireLineData ( int iLine );
    virtual void ReleaseLineData ( void * pLineData );
    virtual void GetTreeInfo ( int iLine, uint32 * pFlags, int * iDepth, 
							   OutlinerAncestorInfo ** pAncestor );
    virtual HFONT GetLineFont ( void * pLineData );
    virtual int TranslateIcon ( void *);
    virtual int TranslateIconFolder ( void *);

    virtual BOOL RenderData ( UINT iColumn, CRect & rect, CDC & pdc, LPCTSTR );

    virtual BOOL ColumnCommand ( int iColumn, int iLine );
    virtual void PropertyMenu ( int iSel, UINT flags );

	// Drag Drop stuff

	// clip formats
    virtual void InitializeClipFormats(void);
    virtual CLIPFORMAT * GetClipFormatList(void);

	// initiate drag
    virtual COleDataSource * GetDataSource(void);

	// accept drop
	virtual DROPEFFECT DropSelect( int iLineNo, COleDataObject *object );
    virtual void AcceptDrop( int iLineNo, COleDataObject *object, DROPEFFECT effect );

	afx_msg void OnTimer(UINT nIDEvent);
	afx_msg void OnSetFocus(CWnd* pOldWnd);
	afx_msg void OnDestroy();
   DECLARE_MESSAGE_MAP()
};

///////////////////////////////////////////////////////////////////////
// CFolderOutlinerParent

class CFolderOutlinerParent : public COutlinerParent 
{
public:
    CFolderOutlinerParent();
    ~CFolderOutlinerParent();
    virtual COutliner *GetOutliner();
    virtual void CreateColumns ( void );

protected:
	afx_msg void OnDestroy();
    DECLARE_MESSAGE_MAP()
};

// CMessageOutlinerParent

class CMessageOutlinerParent : public COutlinerParent 
{
protected:
	MWContextType m_contextType;

public:
    CMessageOutlinerParent();
    ~CMessageOutlinerParent();
    virtual COutliner *GetOutliner();
    virtual void CreateColumns ( void );
    virtual BOOL RenderData ( int idColumn, CRect & rect, CDC & dc, LPCTSTR );

protected:
    virtual BOOL ColumnCommand ( int iColumn );

	afx_msg void OnDestroy();
    DECLARE_MESSAGE_MAP()
};

///////////////////////////////////////////////////////////////////////
// CFolderView

class CFolderView   :   public COutlinerView
{
public:
    CFolderView ( )
    {
        m_pOutlinerParent = (COutlinerParent *) new CFolderOutlinerParent;
    }
    DECLARE_DYNCREATE(CFolderView)
};

/////////////////////////////////////////////////////////////////////////////
// CMailFolderHelper
//
// It's such a pain to use the same owner draw routines for a combo box
// and a list using MFC. This just pulls out the important stuff into its
// own, multiple inheritable class

class CMailFolderHelper {
protected:
	BOOL m_bStaticCtl;

	UINT m_nAddString, m_nSetItemData, m_nResetContent;

	int m_iInitialDepth;

	MSG_Master *m_pMaster;
	LPIMAGEMAP m_pIImageMap;
	LPUNKNOWN m_pIImageUnk;

	HFONT m_hFont, m_hBoldFont;

	CMailFolderHelper(UINT nAddString, UINT nSetItemData, UINT nResetContent );
	~CMailFolderHelper();

	void InitFonts( HDC hdc );

	void DoDrawItem( HWND hWnd, LPDRAWITEMSTRUCT lpDrawItemStruct, BOOL bNoPrettyName = FALSE);
	void DoMeasureItem( HWND hWnd, LPMEASUREITEMSTRUCT lpMeasureItemStruct );

	virtual void SubPopulate( HWND hWnd, int &index, MSG_FolderInfo *folder );

public:
	int Populate( HWND hWnd, MSG_Master *pMaster, MSG_FolderInfo *folderInfo );
	int PopulateMailServer( HWND hWnd, MSG_Master *pMaster, BOOL bRoots = TRUE);
	int PopulateMail( HWND hWnd, MSG_Master *pMaster, BOOL bRoots = TRUE);
	virtual int PopulateNews( HWND hWnd, MSG_Master *pMaster, BOOL bRoots = TRUE );
};

/////////////////////////////////////////////////////////////////////////////
// CMailFolderCombo

class CMailFolderCombo: public CComboBox, public CMailFolderHelper {

protected:

	BOOL m_bNoPrettyName;

	virtual void DrawItem( LPDRAWITEMSTRUCT lpDrawItemStruct )
	{
		DoDrawItem( m_hWnd, lpDrawItemStruct, m_bNoPrettyName);
	}
	virtual void MeasureItem( LPMEASUREITEMSTRUCT lpMeasureItemStruct )
	{
		DoMeasureItem( m_hWnd, lpMeasureItemStruct );
	}

public:
	CMailFolderCombo(): CMailFolderHelper( CB_ADDSTRING, CB_SETITEMDATA, CB_RESETCONTENT ) 
		{ m_bNoPrettyName = FALSE; }

	void NoPrettyName()	{ m_bNoPrettyName = TRUE; }

	int Populate( MSG_Master *pMaster, MSG_FolderInfo *folderInfo )
	{
		return CMailFolderHelper::Populate( m_hWnd, pMaster, folderInfo );
	}

	int PopulateMail( MSG_Master *pMaster, BOOL bRoots = TRUE )
	{
		return CMailFolderHelper::PopulateMail( m_hWnd, pMaster, bRoots );
	}

	int PopulateMailServer( MSG_Master *pMaster, BOOL bRoots = TRUE )
	{
		return CMailFolderHelper::PopulateMailServer( m_hWnd, pMaster, bRoots );
	}
};

/////////////////////////////////////////////////////////////////////////////
// CMailFolderList

class CMailFolderList: public CListBox, public CMailFolderHelper {

protected:
	virtual void DrawItem( LPDRAWITEMSTRUCT lpDrawItemStruct )
	{
		DoDrawItem( m_hWnd, lpDrawItemStruct );
	}
	virtual void MeasureItem( LPMEASUREITEMSTRUCT lpMeasureItemStruct )
	{
		DoMeasureItem( m_hWnd, lpMeasureItemStruct );
	}

public:
	CMailFolderList(): CMailFolderHelper( LB_ADDSTRING, LB_SETITEMDATA, LB_RESETCONTENT ) {}

	virtual int Populate( MSG_Master *pMaster, MSG_FolderInfo *folderInfo )
	{
		return CMailFolderHelper::Populate( m_hWnd, pMaster, folderInfo );
	}
};

/////////////////////////////////////////////////////////////////////////////
// CMiscFolderCombo

class CMiscFolderCombo: public CMailFolderCombo {

protected:

	virtual void DrawItem( LPDRAWITEMSTRUCT lpDrawItemStruct )
	{
		DoDrawItem( m_hWnd, lpDrawItemStruct );
	}

	virtual void MeasureItem( LPMEASUREITEMSTRUCT lpMeasureItemStruct )
	{
		DoMeasureItem( m_hWnd, lpMeasureItemStruct );
	}

public:
	CMiscFolderCombo(): CMailFolderCombo() {}

	virtual int Populate( MSG_Master *pMaster, MSG_FolderInfo *folderInfo )
	{
		return CMailFolderCombo::Populate( pMaster, folderInfo );
	}
};

class CMailNewsFrame;

class IMailFrame: public IUnknown
{
public:
	virtual CMailNewsFrame *GetMailNewsFrame() = 0;
	virtual MSG_Pane *GetPane() = 0;
	virtual void PaneChanged( MSG_Pane *pane, XP_Bool asynchronous, 
							  MSG_PANE_CHANGED_NOTIFY_CODE, int32 value) = 0;
	virtual void AttachmentCount(MSG_Pane *messagepane, void* closure,
								 int32 numattachments, XP_Bool finishedloading) = 0;
	virtual void UserWantsToSeeAttachments(MSG_Pane *messagepane, void *closure) = 0;
};

typedef IMailFrame *LPMAILFRAME;

#endif
