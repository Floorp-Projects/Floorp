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

#ifndef __HiddenFrame_H
#define __HiddenFrame_H

#ifdef MOZ_MAIL_NEWS
#include "addrdlg.h"    // rhp - for MAPI
#endif /* MOZ_MAIL_NEWS */

// hiddenfr.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CHiddenFrame frame

#define MSG_TASK_NOTIFY 		(WM_USER + 1)
#define MSG_EXIT_INSTANCE_STATUS			(WM_USER + 999)

class CHiddenFrame : public CFrameWnd
{
	DECLARE_DYNCREATE(CHiddenFrame)
protected:
	CHiddenFrame(); 	  // protected constructor used by dynamic creation
	friend class CNetscapeApp;	//	Allow the application to create us though.

// Attributes
public:

// Operations
public:

#ifdef MOZ_MAIL_NEWS
  void AddressDialog(LPSTR winText, MAPIAddressCallbackProc mapiCB, MAPIAddressGetAddrProc getProc); // rhp - for MAPI
#endif /* MOZ_MAIL_NEWS */

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CHiddenFrame)
	//}}AFX_VIRTUAL

// Implementation
protected:
	virtual ~CHiddenFrame();

	void HandleWebNotification(LONG lMessage);
#ifdef MOZ_MAIL_NEWS
	void HandleInboxNotification(LONG lMessage);
	void HandleMailNotification(LONG lMessage);
	void HandleNewsNotification(LONG lMessage);
#endif /* MOZ_MAIL_NEWS */
#ifdef EDITOR
    void HandleEditorNotification(LONG lMessage);
#endif /* EDITOR */
#ifdef MOZ_MAIL_NEWS
	void HandleComposeNotification(LONG lMessage);
	void HandleAddressBookNotification(LONG lMessage);
#endif /* MOZ_MAIL_NEWS */

	// Generated message map functions
	//{{AFX_MSG(CHiddenFrame)
		// NOTE - the ClassWizard will add and remove member functions here.
    afx_msg LONG OnNetworkActivity(UINT socket, LONG lParam);
    afx_msg LONG OnFoundDNS(WPARAM wParam, LONG lParam);
    afx_msg LONG OnForceIOSelect(WPARAM wParam, LONG lParam);
	afx_msg LONG OnTaskNotify(WPARAM wParam, LPARAM lParam);
	afx_msg void OnDestroy(void);
    afx_msg void OnEndSession(BOOL bEnding);
	afx_msg LONG OnRequestExitStatus(WPARAM wParam/*0*/,LPARAM lParam/*0*/);
	//}}AFX_MSG
#ifdef XP_WIN16
    afx_msg LONG OnNetworkAccept(UINT socket, LONG lParam);
    afx_msg LONG OnNetworkRead(UINT socket, LONG lParam);
    afx_msg LONG OnNetworkWrite(UINT socket, LONG lParam);
    afx_msg LONG OnNetworkClose(UINT socket, LONG lParam);
#endif
#ifdef MOZ_MAIL_NEWS
	afx_msg LONG OnAltMailBiffNotification(WPARAM wParam, LPARAM lParam);
#endif
    afx_msg BOOL OnQueryEndSession(); //~~av

#ifdef MOZ_MAIL_NEWS
    afx_msg LONG OnProcessIPCHook(UINT, LONG);   // rhp - for IPC
#endif

	DECLARE_MESSAGE_MAP()
};

//	Global Vars
//
extern CMapStringToOb DNSCacheMap;
extern const UINT NEAR msg_FoundDNS;
extern UINT NEAR msg_NetActivity;
#ifdef XP_WIN16
extern UINT NEAR msg_NetAccept;
extern UINT NEAR msg_NetRead;
extern UINT NEAR msg_NetWrite;
extern UINT NEAR msg_NetClose;
#endif
extern UINT NEAR msg_ForceIOSelect;

//  Net dike.
//  Give priority to all events except msg_ForceIOSelect and msg_NetActivity
//	(both registered with RegisterWindowMessage) function every X number
//	of MessagePumps (keeps IO going even if message queue flooded).
//  The goal here is to keep the application responsive while heavy network or file IO
//	activity is occuring from the msg_ForceIOSelect or msg_NetActivity
//	registered message.  Note that msg_NetActivity is equal to
//	msg_ForceIOSelect + 1 (we assume, but handle correctly below anyhow).
extern int gNetFloodStage;
#define NET_FLOWCONTROL 16  //	Set higher for better UI responsivness.
#define NET_MESSAGERANGE (MIN(msg_ForceIOSelect, msg_NetActivity) - 1)


/////////////////////////////////////////////////////////////////////////////
#endif // __HiddenFrame_H
