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

// offlndlg.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// COfflineDlg dialog

#ifndef OFFLNDLG_H
#define OFFLNDLG_H

#include "mnrccln.h"

class COfflineInfo
{

protected:
	BOOL m_bDownloadMail;
	BOOL m_bDownloadNews;
	BOOL m_bDownloadDirectories;
	BOOL m_bSendMail;
	BOOL m_bDownloadFlagged;
	BOOL m_bGoOffline;
	BOOL m_bChangeState;

public:
	COfflineInfo(BOOL bDownloadMail, BOOL bDownloadNews, BOOL bDownloadDirectoires,
				BOOL bSendMail, BOOL bDownloadFlagged, BOOL bGoOffline, BOOL bChangeState);

	BOOL DownloadMail();
	BOOL DownloadNews();
	BOOL DownloadDirectories();
	BOOL SendMail();
	BOOL DownloadFlagged();
	BOOL GoOffline();
	BOOL ChangeState();

};

class COfflineDlg : public CDialog
{

public:
	LOGFONT  m_LogFont;
	CFont    m_Font;
	CFont    m_Font2;
	BOOL	 m_bMode;
	int m_nNumberSelected;

	CMailNewsResourceSwitcher m_MNResourceSwitcher;
// Construction
public:
	COfflineDlg(CWnd* pParent = NULL);   // standard constructor
	
	void InitDialog(BOOL bMode) {m_bMode = bMode;};  //Sets button window font sizes
	static void ShutDownFrameCallBack(HWND hwnd, MSG_Pane *pane, void * closure);
	static BOOL ShowOnlineCallBack(HWND hWnd, MSG_Pane *pane, void *closure);
	BOOL DownloadItems();  // should anything be downloaded?

// Dialog Data
#ifdef WIN32
	enum { IDD = IDD_DOWNLOAD_CONTROL_PANEL };
#else
	enum { IDD = IDD_DWNLD_CTRL_PANEL_W16 };
#endif
	BOOL	m_bDownLoadDiscussions;
	BOOL	m_bDownLoadMail;
	BOOL	m_bDownLoadDirectories;
	BOOL	m_bSendMessages;
	BOOL	m_bDownLoadFlagged;
	BOOL	m_bGoOffline;

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	public:
	virtual int DoModal( );

// Implementation
protected:
	afx_msg void OnButtonSelect();
	virtual void OnOK();
	virtual BOOL OnInitDialog();
	virtual void OnHelp();
	afx_msg void OnDestroy( );
	DECLARE_MESSAGE_MAP()
};

//CGoOfflineAndSynchDlg prompts user to synchronize before going offline.
class CGoOfflineAndSynchDlg : public CDialog
{

public:
	LOGFONT  m_LogFont;
	CFont    m_Font;
	CFont    m_Font2;
	BOOL	 m_bMode;

	CMailNewsResourceSwitcher m_MNResourceSwitcher;
// Construction
public:
	CGoOfflineAndSynchDlg(CWnd* pParent = NULL);   // standard constructor
	
	void InitDialog(BOOL bMode) {m_bMode = bMode;};  //Sets button window font sizes
	static void ShutDownFrameCallBack(HWND hwnd, MSG_Pane *pane, void * closure);
// Dialog Data
	enum { IDD = IDD_SYNCHRONIZE_ONOFF };
	BOOL	m_bDownLoadDiscussions;
	BOOL	m_bDownLoadMail;
	BOOL	m_bDownLoadDirectories;
#ifdef WIN32
	BOOL	m_bDownLoadPages;
	BOOL	m_bDownLoadChannels;
#endif
	BOOL	m_bSendMessages;
	BOOL	m_bGoOffline;


// Implementation
public:
	virtual int DoModal( );

protected:
	virtual void OnOK();
	afx_msg void OnNo();
	virtual void OnCancel();
	virtual void OnHelp();
	virtual BOOL OnInitDialog();
	DECLARE_MESSAGE_MAP()
};


class CAskSynchronizeExitDlg : public CDialog
{

public:

// Construction
public:
	CAskSynchronizeExitDlg(CWnd* pParent = NULL);   // standard constructor
	
// Dialog Data
	enum { IDD = IDD_SYNCHRONIZE_EXIT };

	BOOL	m_bDontAskAgain;

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	void CommonClose();
// Implementation
protected:
	virtual void OnYes();
	virtual void OnNo();
	DECLARE_MESSAGE_MAP()
};

#endif 


