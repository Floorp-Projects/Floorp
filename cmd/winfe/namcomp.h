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
#ifndef _namcomp_h_
#define _namcomp_h_

// NAMCOMP.H
//
// DESCRIPTION:
//		This file contains the declarations of the for the name completion 
//		dialog
//

#include "outliner.h"
#include "apimsg.h"
#include "addrbook.h"
#include "mailfrm.h"
#include "mailpriv.h"
#include "mnrccln.h"

class CNameCompletion;
class CNameCompletionOutliner;
class CNameCompletionOutlinerParent;
class CNameCompletionEntryList;

// Definitions for column headings in the outliner control
#define DEF_VISIBLE_COLUMNS			5
#define ID_COLADDR_TYPE				1
#define ID_COLADDR_NAME				2
#define ID_COLADDR_EMAIL			3
#define ID_COLADDR_COMPANY			4
#define ID_COLADDR_PHONE			5
#define ID_COLADDR_LOCALITY			6
#define ID_COLADDR_NICKNAME			7

// array of indexes for IDB_ADDRESSBOOK bitmap
#define IDX_ADDRESSBOOKPERSON   0
#define IDX_ADDRESSBOOKLIST		1
#define IDX_ADDRESSBOOKPERCARD  2

// name completion context
class CNameCompletionCX: public CStubsCX
{
protected:
	CNameCompletion* m_pDialog;

	int32 m_lPercent;
	BOOL m_bAnimated;

public:
	CNameCompletionCX(CNameCompletion *pDialog);

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
*	Class: CNameCompletionOutlinerParent
*
*	DESCRIPTION:
*		This class is the window around the column/list object in the name 
*		completion picker.  It is mainly purpose is to draw the column headings.
*
****************************************************************************/
class CNameCompletionOutlinerParent : public COutlinerParent 
{
public:
	CNameCompletionOutlinerParent();
	virtual ~CNameCompletionOutlinerParent();
    virtual COutliner * GetOutliner ( );
    virtual void CreateColumns ( );
    virtual BOOL RenderData ( int idColumn, CRect & rect, CDC & dc, const char *);
	virtual BOOL ColumnCommand ( int idColumn );

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(CNameCompletionOutlinerParent)
	afx_msg void OnLButtonUp ( UINT nFlags, CPoint point );
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()
};


/****************************************************************************
*
*	Class: CNameCompletion
*
*	DESCRIPTION:
*		This class is the name completion from the compose window
*
****************************************************************************/
class CNameCompletion : public CDialog, public IMailFrame {

// Attributes
public:

	friend class CNameCompletionEntryList;
	DIR_Server*		m_directory;
	int			m_iWidth;

    enum { IDD = IDD_COMPLETION_PICKER };

	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CNameCompletion)
public:
protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

	//{{AFX_DATA(CNameCompletion)
	CString	m_searchString;
	//}}AFX_DATA

protected:

	CNetscapeStatusBar				m_barStatus;
	CNameCompletionCX				*m_pCX;
	HFONT							m_pFont;
	CNameCompletionOutlinerParent	*m_pOutlinerParent;
	CNameCompletionOutliner			*m_pOutliner;

	LPMSGLIST						m_pIAddrList;
	ABPane							*m_addrBookPane;
	BOOL							m_bSearching;
	CMailNewsResourceSwitcher		m_MailNewsResourceSwitcher;
	LPCTSTR							m_lpszSearchString;

	// IUnknown Interface
	STDMETHODIMP			QueryInterface(REFIID,LPVOID *);
	STDMETHODIMP_(ULONG)	AddRef(void);
	STDMETHODIMP_(ULONG)	Release(void);

// Support for IMsgList Interface (Called by CNameCompletionList)
	virtual void ListChangeStarting( MSG_Pane* pane, XP_Bool asynchronous,
									 MSG_NOTIFY_CODE notify, MSG_ViewIndex where,
									 int32 num);
	virtual void ListChangeFinished( MSG_Pane* pane, XP_Bool asynchronous,
									 MSG_NOTIFY_CODE notify, MSG_ViewIndex where,
									 int32 num);

	void CleanupOnClose();

// Operations
public:
	CNameCompletion(LPCTSTR lpszSearchString, CWnd * parent = NULL);

	enum { ToolInvalid = -1, ToolText = 0, ToolPictures = 1, ToolBoth = 2 };

	// From CStubsCX
	void Progress(const char *pMessage);
	void SetProgressBarPercent(int32 lPercent);
	void AllConnectionsComplete(MWContext *pContext);
	void SetStatusText(const char* pMessage);

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

	void UpdateButtons();
	void PerformDirectorySearch();

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CNameCompletion)
	public:
	virtual  BOOL OnInitDialog( );
	protected:
	//}}AFX_VIRTUAL

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(CNameCompletion)
	afx_msg int OnCreate( LPCREATESTRUCT );
	afx_msg void OnSize( UINT, int, int);
	afx_msg void OnOK();
	afx_msg void OnCancel();
	afx_msg void OnComposeMsg(void);
	afx_msg void OnAddToAddressBook(void);
	afx_msg void OnSelchange();
	afx_msg void OnHelp();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};


/****************************************************************************
*
*	Class: CNameCompletionOutliner
*
*	DESCRIPTION:
*		This class is the column/list object in the name completion dialog
*
****************************************************************************/
class CNameCompletionOutliner : public COutliner
{
friend class CNameCompletionOutlinerParent;

protected:
	int					m_lineindex;
	char*				m_pszExtraText;
	ABPane*				m_pane;
	int					m_iMysticPlane;
	MWContext*			m_pContext;
    AB_EntryLine		m_EntryLine;
	HFONT				m_hFont;

public:
    CNameCompletionOutliner ( );
    virtual ~CNameCompletionOutliner ( );

	void	UpdateCount();
	void	OnChangeDirectory(int dirIndex);
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
	void SetColumnsForDirectory (DIR_Server* pServer);

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
	//{{AFX_MSG(CNameCompletionOutliner)
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()


};

/////////////////////////////////////////////////////////////////////////////
// CNameCompletionEntryList

class CNameCompletionEntryList: public IMsgList {

	CNameCompletion *m_pNameCompletion;
	unsigned long m_ulRefCount;

public:
// IUnknown Interface
	STDMETHODIMP			QueryInterface(REFIID,LPVOID *);
	STDMETHODIMP_(ULONG)	AddRef(void);
	STDMETHODIMP_(ULONG)	Release(void);

// IMsgList Interface
	virtual void ListChangeStarting( MSG_Pane* pane, XP_Bool asynchronous,
									 MSG_NOTIFY_CODE notify, MSG_ViewIndex where,
									 int32 num);
	virtual void ListChangeFinished( MSG_Pane* pane, XP_Bool asynchronous,
									 MSG_NOTIFY_CODE notify, MSG_ViewIndex where,
									 int32 num);
	virtual void GetSelection( MSG_Pane* pane, MSG_ViewIndex **indices, int *count, 
							    int *focus);
	virtual void SelectItem( MSG_Pane* pane, int item );

	virtual void CopyMessagesInto( MSG_Pane *pane, MSG_ViewIndex *indices, int count,
								   MSG_FolderInfo *folderInfo) {}
	virtual void MoveMessagesInto( MSG_Pane *pane, MSG_ViewIndex *indices, int count,
								   MSG_FolderInfo *folderInfo) {}

	CNameCompletionEntryList( CNameCompletion *pNameCompletion ) {
		m_ulRefCount = 0;
		m_pNameCompletion = pNameCompletion;
	}
};

#endif
