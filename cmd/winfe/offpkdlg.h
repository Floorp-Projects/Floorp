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

// offpkdlg.h : header file
//

#ifndef OFFPKDLG_H
#define OFFPKDLG_H

#include "property.h"
#include "outliner.h"
#include "apiimg.h"
#include "apimsg.h"
#include "mailmisc.h"
#include "msgcom.h"
#include "mnrccln.h"


//offline outliner item data container.
typedef struct ItemData
{
	MSG_FolderInfo *infoData;
	BOOL bDownLoad;
}ItemData;

// Definitions for column headings in the outliner control
#define ID_COL_NAME         1
#define ID_COL_DOWNLOAD     2

class COfflinePickerOutliner;
class CDlgOfflinePicker;
class COfflineList;


/////////////////////////////////////////////////////////////////////////////
//	Class: COfflinePickerCX
class COfflinePickerCX: public CStubsCX
{
protected:
	CDlgOfflinePicker* m_pDialog;

public:
	COfflinePickerCX(ContextType type,CDlgOfflinePicker* pDialog);
	void Alert(MWContext *pContext, const char *pMessage){return;}

	virtual CWnd *GetDialogOwner() const { return (CWnd*)m_pDialog;}	

};

/////////////////////////////////////////////////////////////////////////////
//	Class: COfflinePickerOutlinerParent
class COfflinePickerOutlinerParent : public COutlinerParent 
{
public:
	COfflinePickerOutlinerParent();
	virtual ~COfflinePickerOutlinerParent();
    virtual COutliner * GetOutliner ( );
    virtual void CreateColumns ( );
    virtual BOOL RenderData ( int idColumn, CRect & rect, CDC & dc, const char *);

	void SetDialogOwner(CDlgOfflinePicker* pDialog) { m_pDialog = pDialog; }

// Implementation
protected:

	CDlgOfflinePicker* m_pDialog;

	DECLARE_MESSAGE_MAP()
};



/////////////////////////////////////////////////////////////////////////////
// CDlgOfflinePicker dialog

class CDlgOfflinePicker : public CDialog
{

public:
	CMailNewsResourceSwitcher m_MNResourceSwitcher;

	int m_nDiscussionSelectionCount;
	int m_nMailSelectionCount;
	int m_nDirectorySelectionCount;
	int m_nPublicFolderSelectionCount;
	int m_iIndex;

	int nameWidth;
	int downloadWidth;
	int namePos;
	int downloadPos;

	BOOL m_bListChangeStarting;
	BOOL m_bProcessGetDeletion;


    CWnd *m_pParent;
	MSG_Pane *m_pPane;
	MSG_Master *m_pMaster;
	COfflinePickerCX *m_pOfflineCX;
	COfflineList *m_pPickerList;

// Construction
public:
	CDlgOfflinePicker(CWnd* pParent = NULL);   // standard constructor
	~CDlgOfflinePicker();

	MWContext* GetContext();
	COfflinePickerCX* GetPickerContext() {return m_pOfflineCX;};

	MSG_Pane * GetPane() {return m_pPane;}; 
	COfflineList*  GetList() {return m_pPickerList;};  
	COfflineList**  GetListHandle() {return &m_pPickerList;};  
	COfflinePickerOutliner * GetOutliner() { return m_pOutliner; };

	void SetPickerContext(COfflinePickerCX* pCX) {m_pOfflineCX = pCX;};
	void SetPane(MSG_Pane *pPane) {m_pPane = pPane;}; 
	void SetList(COfflineList* pList) {m_pPickerList = pList;}; 

	Bool IsOutlinerHasFocus();
	void EnableAllControls(BOOL bEnable);
	void DoStopListChange();
	void ClearPickerSelection();

	virtual void ListChangeStarting(MSG_Pane* pane, XP_Bool asynchronous,
									MSG_NOTIFY_CODE notify, MSG_ViewIndex where,
									 int32 num);
	virtual void ListChangeFinished(MSG_Pane* pane, XP_Bool asynchronous,
									MSG_NOTIFY_CODE notify, MSG_ViewIndex where,
									int32 num);

	// Dialog Data
	//{{AFX_DATA(CDlgOfflinePicker)
	enum { IDD = IDD_OFFLINE_PICKER };
		// NOTE: the ClassWizard will add data members here
	//}}AFX_DATA

	int GetSelectionCount() {return m_nDiscussionSelectionCount;};
	int GetMailSelectionCount() {return m_nMailSelectionCount;};

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CDlgOfflinePicker)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	public:
	virtual int DoModal ();  
	//}}AFX_VIRTUAL

// Implementation
protected:
    BOOL m_bActivated;
    BOOL m_bSelChanged;
    BOOL m_bNotifyAll;  //MAG_NotifyALl when outliner is not visible
    BOOL m_bInitDialog;
	BOOL m_bDoShowWindow;

	COfflinePickerOutlinerParent	*m_pOutlinerParent;
	COfflinePickerOutliner			*m_pOutliner;

	void CleanupOnClose();
	// Generated message map functions
	//{{AFX_MSG(CDlgOfflinePicker)
	virtual void OnOK();
	virtual BOOL OnInitDialog();
	virtual void OnCancel();
	virtual void OnHelp();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};


class COfflinePickerOutliner : public CMailNewsOutliner
{
friend class COfflinePickerOutlinerParent;

protected:
	char*				m_pszExtraText;
    OutlinerAncestorInfo * m_pAncestor;
    MSG_FolderLine		m_FolderLine;
 	BOOL				m_bSelChanged;
	CDlgOfflinePicker* m_pDialog;

public:
    COfflinePickerOutliner ( );
    virtual ~COfflinePickerOutliner ( );

	MSG_Pane * GetPane() {return m_pPane;};
	void SetDlg(CDlgOfflinePicker *pDialog) { m_pDialog = pDialog; }
	CDlgOfflinePicker * GetDlg() { return m_pDialog; }
	void DeselectItem();
	BOOL SelectInitialItem();
	BOOL HasSelectableItems();

	virtual void OnSelChanged();
	virtual void OnSelDblClk();
    virtual int ToggleExpansion ( int iLine );

	virtual int GetDepth( int iLine );
	virtual int GetNumChildren( int iLine );
	virtual BOOL IsCollapsed( int iLine );
	virtual BOOL ColumnCommand(int iColumn, int iLine);

    virtual LPCTSTR GetColumnText ( UINT iColumn, void * pLineData );
    virtual void * AquireLineData ( int iLine );
    virtual void ReleaseLineData ( void * pLineData );
    virtual void GetTreeInfo ( int iLine, uint32 * pFlags, int * iDepth, 
							   OutlinerAncestorInfo ** pAncestor );
    virtual BOOL RenderData ( UINT idColumn, CRect & rect, CDC & dc, const char * text);
    virtual int TranslateIcon ( void *);
    virtual int TranslateIconFolder ( void *);

};


/////////////////////////////////////////////////////////////////////////////
//	Class: COfflineList
class COfflineList: public IMsgList 
{

	CDlgOfflinePicker* m_pDialog;

	unsigned long m_ulRefCount;

public:
// IUnknown Interface
	STDMETHODIMP			QueryInterface(REFIID,LPVOID *);
	STDMETHODIMP_(ULONG)	AddRef(void);
	STDMETHODIMP_(ULONG)	Release(void);

// IMsgList Interface
	virtual void ListChangeStarting(MSG_Pane* pane, XP_Bool asynchronous,
									MSG_NOTIFY_CODE notify, MSG_ViewIndex where,
									 int32 num);
	virtual void ListChangeFinished(MSG_Pane* pane, XP_Bool asynchronous,
									MSG_NOTIFY_CODE notify, MSG_ViewIndex where,
									int32 num);
	virtual void GetSelection(MSG_Pane* pane, MSG_ViewIndex **indices, int *count, 
							    int *focus);
	virtual void SelectItem(MSG_Pane* pane, int item);
	virtual void CopyMessagesInto( MSG_Pane *pane, MSG_ViewIndex *indices, int count,
								   MSG_FolderInfo *folderInfo) {}
	virtual void MoveMessagesInto( MSG_Pane *pane, MSG_ViewIndex *indices, int count,
								   MSG_FolderInfo *folderInfo) {}

	void SetPickerDialog(CDlgOfflinePicker * pDialog) 
		{ m_pDialog = pDialog; } 

	COfflineList(CDlgOfflinePicker *pDialog) 
	{
		m_ulRefCount = 0;
		m_pDialog = pDialog;
	}
};

#endif OFFPKDLG_H
