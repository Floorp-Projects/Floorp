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
#ifndef _addrdlg_h_
#define _addrdlg_h_

// ADDRDLG.H
//
// DESCRIPTION:
//		This file contains the declarations of the for the address picker 
//		dialog
//

#include "outliner.h"
#include "apimsg.h"
#include "addrbook.h"
#include "mailfrm.h"
#include "mailpriv.h"
#include "mnrccln.h"

class COutlinerView;
class CAddrDialog;
class CAddrDialogOutliner;
class CAddrDialogOutlinerParent;
class CAddrDialogDirOutliner;
class CAddrDialogDirOutlinerParent;
class CAddrDialogEntryList;

// Definitions for column headings in the outliner control

#define DEF_VISIBLE_COLUMNS			5
#define ID_COLADDR_TYPE				1
#define ID_COLADDR_NAME				2
#define ID_COLADDR_EMAIL			3
#define ID_COLADDR_COMPANY			4
#define ID_COLADDR_PHONE			5
#define ID_COLADDR_LOCALITY			6
#define ID_COLADDR_NICKNAME			7

#define DEF_DIRVISIBLE_COLUMNS		2
#define ID_COLDIR_TYPE				1
#define ID_COLDIR_NAME				2

// array of indexes for IDB_ADDRESSBOOK bitmap
#define IDX_ADDRESSBOOKPERSON   0
#define IDX_ADDRESSBOOKLIST		1
#define IDX_ADDRESSBOOKPERCARD  2

// array of indexes for IDB_DIRLIST bitmap
#define IDX_DIRLDAPAB			0
#define IDX_DIRPERSONALAB		1

class CAddressPickerDropTarget : public COleDropTarget
{
public:

	CAddrDialog* m_pOwner;
//Construction
    CAddressPickerDropTarget(CAddrDialog* pOwner) { m_pOwner = pOwner; }
    BOOL        OnDrop(CWnd *, COleDataObject *, DROPEFFECT, CPoint);
    DROPEFFECT  OnDragOver(CWnd *, COleDataObject *, DWORD, CPoint);
};

// address book context
class CAddrDialogCX: public CStubsCX
{
protected:
	CAddrDialog* m_pDialog;

	int32 m_lPercent;
	CString m_csProgress;
	BOOL m_bAnimated;

public:
	CAddrDialogCX(CAddrDialog *pDialog);
	// void DestroyContext();

public:
	int32 QueryProgressPercent();
	void SetProgressBarPercent(MWContext *pContext, int32 lPercent);

	void Progress(MWContext *pContext, const char *pMessage);
	void AllConnectionsComplete(MWContext *pContext);

	void UpdateStopState( MWContext *pContext );
    
	CWnd *GetDialogOwner() const;
};



/****************************************************************************
*
*	Class: CAddrDialogOutlinerParent
*
*	DESCRIPTION:
*		This class is the window around the column/list object in the address 
*		book.  It is mainly purpose is to draw the column headings.
*
****************************************************************************/
class CAddrDialogOutlinerParent : public COutlinerParent 
{
public:
	CAddrDialogOutlinerParent();
	virtual ~CAddrDialogOutlinerParent();
    virtual COutliner * GetOutliner ( );
    virtual void CreateColumns ( );
    virtual BOOL RenderData ( int idColumn, CRect & rect, CDC & dc, const char *);
	virtual BOOL ColumnCommand ( int idColumn );

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(CAddrDialogOutlinerParent)
	afx_msg void OnLButtonUp ( UINT nFlags, CPoint point );
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()
};

/****************************************************************************
*
*	Class: CAddrDialogDirOutlinerParent
*
*	DESCRIPTION:
*		This class is the window around the column directorylist 
*		object in the address book.  It is mainly purpose is to draw 
*		the column headings.
*
****************************************************************************/
class CAddrDialogDirOutlinerParent : public COutlinerParent 
{
public:
	CAddrDialogDirOutlinerParent();
	virtual ~CAddrDialogDirOutlinerParent();
    virtual COutliner * GetOutliner ( );
    virtual void CreateColumns ( );
    virtual BOOL RenderData ( int idColumn, CRect & rect, CDC & dc, const char *);

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(CAddrDialogDirOutlinerParent)
	afx_msg void OnLButtonUp ( UINT nFlags, CPoint point );
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()
};


/****************************************************************************
*
*	Class: CAddrDialogDirOutliner
*
*	DESCRIPTION:
*		This class is the column/list object for the directory list
*		in the address book
*
****************************************************************************/
class CAddrDialogDirOutliner : public CMSelectOutliner
{
friend class CAddrDialogDirOutlinerParent;

protected:
	int					m_lineindex;
	char*				m_pszExtraText;
    CLIPFORMAT			m_cfAddresses;
	ABPane*				m_pane;
	int					m_iMysticPlane;
	MWContext*			m_pContext;
	int					m_dirIndex;
    DIR_Server*			m_pDirLine;
	HFONT				m_hFont;

public:
    CAddrDialogDirOutliner ( );
    virtual ~CAddrDialogDirOutliner ( );

	void	UpdateCount();
	void	OnChangeDirectory(int dirIndex);
	void	SetDirectoryIndex (int dirIndex);
	int		GetDirectoryIndex () { return m_dirIndex; }

	virtual void OnSelChanged();
	virtual void SetTotalLines( int iLines );
	virtual void OnSelDblClk();

	virtual HFONT GetLineFont( void *pLineData );
    virtual LPCTSTR GetColumnText ( UINT iColumn, void * pLineData );
    virtual void * AcquireLineData ( int iLine );
    virtual void ReleaseLineData ( void * pLineData );
    virtual void GetTreeInfo ( int iLine, uint32 * pFlags, int * iDepth, 
							   OutlinerAncestorInfo ** pAncestor );
    virtual int TranslateIcon ( void *);
    virtual int TranslateIconFolder ( void *);
    virtual BOOL ColumnCommand ( int iColumn, int iLine );

	void SetPane( ABPane *pane );
	ABPane* GetPane() { return m_pane; }

	MWContext *GetContext() { return m_pContext; }
	void SetContext( MWContext *pContext ) { m_pContext = pContext; }

	virtual void MysticStuffStarting( XP_Bool asynchronous,
									 MSG_NOTIFY_CODE notify, 
								     MSG_ViewIndex where,
									 int32 num);
	virtual void MysticStuffFinishing( XP_Bool asynchronous,
									  MSG_NOTIFY_CODE notify, 
								      MSG_ViewIndex where,
									  int32 num);

    virtual BOOL RenderData ( UINT iColumn, CRect & rect, CDC & pdc, const char * );

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(CAddrDialogDirOutliner)
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()


};

/****************************************************************************
*
*	Class: CAddrDialog
*
*	DESCRIPTION:
*		This class is the address picker from the compose window
*
****************************************************************************/
typedef BOOL (*MAPIAddressCallbackProc)(int totalCount, int currentIndex, 
                                        int addrType, LPSTR addrString); // rhp - for MAPI
typedef BOOL (*MAPIAddressGetAddrProc)(LPSTR *name, LPSTR *address, int *addrType);    // rhp - for MAPI

class CAddrDialog : public CDialog, public IMailFrame {

// Attributes
public:

	friend class CAddrDialogEntryList;

	int			m_iWidth;
	UINT		m_uTypedownClock;
	CString		m_save;
	int			m_savedir;
	int			m_idefButtonID;

    enum { IDD = IDD_ADDRESSPICKER };

	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CAddrDialog)
public:
protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

	//{{AFX_DATA(CAddrDialog)
	CString	m_name;
	int		m_directory;
	//}}AFX_DATA

protected:

	CNetscapeStatusBar			m_barStatus;

	CMailNewsSplitter			*m_pSplitter;
	CAddrDialogOutlinerParent	*m_pOutlinerParent;
	CAddrDialogOutliner			*m_pOutliner;
	CAddrDialogDirOutliner		*m_pDirOutliner;
	CAddrDialogDirOutlinerParent	*m_pDirOutlinerParent;
	CAddrDialogCX				*m_pCX;
	HFONT						m_pFont;

	LPMSGLIST			m_pIAddrList;
	ABPane				*m_addrBookPane;
	BOOL				m_bSearching;
	CMailNewsResourceSwitcher m_MailNewsResourceSwitcher;

    CAddressPickerDropTarget *m_pDropTarget;

  // rhp - MAPI stuff...
  BOOL                    m_isMAPI;
  LPSTR                   m_mapiHeader;
  MAPIAddressCallbackProc m_mapiCBProc; 
  MAPIAddressGetAddrProc  m_mapiGetAddrProc;
  void                    ProcessMAPIOnDone(void);
  void                    ProcessMAPIAddressPopulation(void);

	// IUnknown Interface
	STDMETHODIMP			QueryInterface(REFIID,LPVOID *);
	STDMETHODIMP_(ULONG)	AddRef(void);
	STDMETHODIMP_(ULONG)	Release(void);

// Support for IMsgList Interface (Called by CAddrDialogEntryList)
	virtual void ListChangeStarting( MSG_Pane* pane, XP_Bool asynchronous,
									 MSG_NOTIFY_CODE notify, MSG_ViewIndex where,
									 int32 num);
	virtual void ListChangeFinished( MSG_Pane* pane, XP_Bool asynchronous,
									 MSG_NOTIFY_CODE notify, MSG_ViewIndex where,
									 int32 num);

	CListBox * GetBucket() { return (CListBox*)GetDlgItem(IDC_ADDRESSBKT); }

	DIR_Server* GetCurrentDirectoryServer ();
	void PerformDirectorySearch ();
	void PerformChangeDirectory (int dirIndex);
	void PerformTypedown (char* searchString);
	void PerformListDirectory (char* searchString);
	void UpdateDirectories();
	void CleanupOnClose();
	void GetFormattedString(char* fullname, MSG_HEADER_SET header, char** formatted);
	void AddStringToBucket(CListBox *pBucket, MSG_HEADER_SET header, 
		                   char* fullname, ABID type, ABID entryID);

// Operations
public:
	CAddrDialog(CWnd* pParent = NULL, 
    BOOL isMapi = FALSE, LPSTR winText = NULL, 
    MAPIAddressCallbackProc mapiCB = NULL,
    MAPIAddressGetAddrProc mapiGetProc = NULL); // rhp - MAPI

	void OnUpdateDirectorySelection (int dirIndex);
	int GetDefaultButtonID () { return m_idefButtonID; }
	void SetDefaultButtonID (int newDefault) { m_idefButtonID = newDefault; }
    void MoveSelections(MSG_HEADER_SET header);
	enum { ToolInvalid = -1, ToolText = 0, ToolPictures = 1, ToolBoth = 2 };

	// From CStubsCX
	void Progress(const char *pMessage);
	void SetProgressBarPercent(int32 lPercent);
	void AllConnectionsComplete(MWContext *pContext);
	void SetStatusText(const char* pMessage);
	void DoUpdateWidget( int command, AB_CommandType cmd, BOOL bUseCheck );

	// Support for IMailFrame
	virtual CMailNewsFrame *GetMailNewsFrame();
	virtual MSG_Pane *GetPane();
	virtual void PaneChanged( MSG_Pane *pane, XP_Bool asynchronous, 
							  MSG_PANE_CHANGED_NOTIFY_CODE, int32 value);
	virtual void AttachmentCount(MSG_Pane *messagepane, void* closure,
								 int32 numattachments, XP_Bool finishedloading) {};
	virtual void UserWantsToSeeAttachments(MSG_Pane *messagepane, void *closure) {};

	// Callback from LDAP search
	void SetSearchResults(MSG_ViewIndex index, int32 num);
	XP_Bool IsSearching () { return m_bSearching; }

	static void HandleErrorReturn(int errorID);

	void Create();
	void DoUpdateAddressBook( CCmdUI* pCmdUI, AB_CommandType cmd, BOOL bUseCheck = TRUE );

	//drop
	BOOL IsDragInListBox(CPoint *pPoint);
    BOOL ProcessVCardData(COleDataObject * pDataObject,CPoint &point);

	void UpdateMsgButtons();

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CAddrDialog)
	public:
	virtual  BOOL OnInitDialog( );
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	protected:
	//}}AFX_VIRTUAL

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(CAddrDialog)
	afx_msg void OnKeyDown ( UINT nChar, UINT nRepCnt, UINT nFlags );
	afx_msg int OnCreate( LPCREATESTRUCT );
	afx_msg void OnSize( UINT, int, int);
	afx_msg void OnTimer( UINT );
	afx_msg void OnSetFocusName();
	afx_msg void OnSetFocusBucket();
	afx_msg void OnChangeName();
	afx_msg void OnChangeDirectory();
	afx_msg void OnDirectorySearch();
	afx_msg void OnStopSearch();
	afx_msg void OnDone();
	afx_msg void OnCancel();
	afx_msg void OnComposeMsg(void);
	afx_msg void OnComposeCCMsg(void);
	afx_msg void OnComposeBCCMsg(void);
	afx_msg void OnGetProperties(void);
	afx_msg void OnRemove(void);
	afx_msg	void OnSortType(void);
	afx_msg	void OnSortName(void);
	afx_msg	void OnSortNickName(void);
	afx_msg	void OnSortEmailAddress(void);
	afx_msg	void OnSortCompany(void);
	afx_msg	void OnSortLocality(void);
	afx_msg	void OnSortAscending(void);
	afx_msg	void OnSortDescending(void);
	afx_msg void OnSelchange();
	afx_msg void OnHelp();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};


/****************************************************************************
*
*	Class: CAddrDialogOutliner
*
*	DESCRIPTION:
*		This class is the column/list object in the address book
*
****************************************************************************/
class CAddrDialogOutliner : public CMSelectOutliner
{
friend class CAddrDialogOutlinerParent;

protected:
	int					m_attribSortBy;
	int					m_lineindex;
	char*				m_pszExtraText;
    CLIPFORMAT			m_cfAddresses;
	ABPane*				m_pane;
	int					m_iMysticPlane;
	BOOL				m_bSortAscending;
	MWContext*			m_pContext;
	int					m_dirIndex;
    AB_EntryLine		m_EntryLine;
	HFONT				m_hFont;
	CString				m_psTypedown;
	UINT				m_uTypedownClock;

public:
    CAddrDialogOutliner ( );
    virtual ~CAddrDialogOutliner ( );

	void	UpdateCount();
	void	OnTypedown (char* name);
	void	OnChangeDirectory(int dirIndex);
	int		GetSortBy() { return m_attribSortBy; }
	void	SetDirectoryIndex (int dirIndex);
	int		GetDirectoryIndex () { return m_dirIndex; }
	BOOL	GetSortAscending() { return m_bSortAscending; }
	DIR_Server* GetCurrentDirectoryServer ();

	virtual void OnSelChanged();
	virtual void SetTotalLines( int iLines );
	virtual void OnSelDblClk();

	virtual HFONT GetLineFont( void *pLineData );
    virtual LPCTSTR GetColumnText ( UINT iColumn, void * pLineData );
    virtual void * AcquireLineData ( int iLine );
    virtual void ReleaseLineData ( void * pLineData );
    virtual void GetTreeInfo ( int iLine, uint32 * pFlags, int * iDepth, 
							   OutlinerAncestorInfo ** pAncestor );
    virtual int TranslateIcon ( void *);
    virtual int TranslateIconFolder ( void *);

	void SetPane( ABPane *pane );
	ABPane* GetPane() { return m_pane; }

	MWContext *GetContext() { return m_pContext; }
	void SetContext( MWContext *pContext ) { m_pContext = pContext; }

	virtual void MysticStuffStarting( XP_Bool asynchronous,
									 MSG_NOTIFY_CODE notify, 
								     MSG_ViewIndex where,
									 int32 num);
	virtual void MysticStuffFinishing( XP_Bool asynchronous,
									  MSG_NOTIFY_CODE notify, 
								      MSG_ViewIndex where,
									  int32 num);

    virtual BOOL RenderData ( UINT iColumn, CRect & rect, CDC & pdc, const char * );

    virtual BOOL ColumnCommand ( int iColumn, int iLine );

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(CAddrDialogOutliner)
	afx_msg void OnTimer( UINT );
	afx_msg void OnKeyDown ( UINT nChar, UINT nRepCnt, UINT nFlags );
	afx_msg void OnKillFocus( CWnd* pNewWnd );
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()


};

#endif
