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
#ifndef _abmldlg_h_
#define _abmldlg_h_

// ABMLDLG.H
//
// DESCRIPTION:
//		This file contains the declarations of the for the mailing list
//		dialog
//

#include "addrfrm.h"
#include "apimsg.h"
#include "xp_core.h"
#include "addrbook.h"
#include "property.h"
#include "apiaddr.h"
#include "mnrccln.h"

class CNSAddressList;
class CABMLDialog;
class CABMLDialogEntryList;


class CMailListDropTarget : public COleDropTarget
{
public:

	CABMLDialog* m_pOwner;
//Construction
    CMailListDropTarget(CABMLDialog* pOwner) { m_pOwner = pOwner; }
    BOOL        OnDrop(CWnd *, COleDataObject *, DROPEFFECT, CPoint);
    DROPEFFECT  OnDragOver(CWnd *, COleDataObject *, DWORD, CPoint);
};

/****************************************************************************
*
*	Class: CABMLDialog
*
*	DESCRIPTION:
*		This class is the address picker from the compose window
*
****************************************************************************/
class CABMLDialog : public CDialog,
                    public IAddressParent {
// Attributes
public:

	friend class CABMLDialogEntryList;

// Dialog Data
	//{{AFX_DATA(CABMLDialog)
	enum { IDD = IDD_ADDRESS_LIST };
	CString	m_description;
	CString	m_name;
	CString	m_nickname;
	//}}AFX_DATA

protected:

    LPADDRESSCONTROL    m_pIAddressList;
    LPUNKNOWN           m_pUnkAddress;
	int					m_iMysticPlane;
	HFONT				m_pFont;

	LPMSGLIST			m_pIAddrList;
	MLPane				*m_addrBookPane;
	DIR_Server			*m_dir;
	ABID				m_entryID;
	BOOL				m_addingEntries;
	BOOL				m_saved;
	int					m_errorCode;
	BOOL				m_changingEntry;

    CMailListDropTarget *m_pDropTarget;

	// Support loading resources from dll
	CMailNewsResourceSwitcher m_MailNewsResourceSwitcher;

// Support for IMsgList Interface (Called by CABMLDialogEntryList)
	virtual void ListChangeStarting( MSG_Pane* pane, XP_Bool asynchronous,
									 MSG_NOTIFY_CODE notify, MSG_ViewIndex where,
									 int32 num);
	virtual void ListChangeFinished( MSG_Pane* pane, XP_Bool asynchronous,
									 MSG_NOTIFY_CODE notify, MSG_ViewIndex where,
									 int32 num);
	void CleanupOnClose();
	void AddEntriesToList(int index, int num);

    // IAddressParent stuff
    virtual void AddedItem (HWND hwnd, LONG id,int index);
    virtual int	 ChangedItem (char * pString, int index, HWND hwnd, char** ppFullName, unsigned long* entryID, UINT* bitmapID);
    virtual void DeletedItem (HWND hwnd, LONG id,int index);
    virtual char * NameCompletion (char *);

// Operations
public:
	CABMLDialog(DIR_Server* dir, CWnd* pParent = NULL, ABID listID = NULL, MWContext* context = NULL);
	~CABMLDialog();

	
	BOOL Create( CWnd *pParent ) { return CDialog::Create( IDD, pParent ); };

	enum { ToolInvalid = -1, ToolText = 0, ToolPictures = 1, ToolBoth = 2 };

	static void HandleErrorReturn(int errorID, CWnd* parent = NULL);
	MLPane*	GetPane() { return m_addrBookPane; }
	ABID	GetEntryID() { return m_entryID; }

    CNSAddressList * GetAddressWidget();
  
	void DoUpdateAddressBook( CCmdUI* pCmdUI, AB_CommandType cmd, BOOL bUseCheck = TRUE );

	// Drop
	BOOL IsDragInListBox(CPoint *pPoint);
    BOOL ProcessVCardData(COleDataObject * pDataObject,CPoint &point);

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CABMLDialog)
	public:
	virtual  BOOL OnInitDialog( );
	BOOL PreTranslateMessage( MSG* pMsg );

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual void PostNcDestroy( );
	//}}AFX_VIRTUAL

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(CABMLDialog)
	afx_msg int OnCreate( LPCREATESTRUCT );
	afx_msg void OnDestroy( );
	afx_msg void OnOK();
	afx_msg void OnCancel();
    afx_msg void OnRemoveEntry();
    afx_msg void OnHelp();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};


#endif
