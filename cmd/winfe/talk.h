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

// TALK.H : interface of the CTalkSiteMgr and CTalkNav classes
//
/////////////////////////////////////////////////////////////////////////////
#ifdef EDITOR
#ifdef _WIN32

#ifndef TALK_H
#define TALK_H

// Finds an existing site manager instance by enumerating existing windows
BOOL FE_FindSiteMgr();

#define SM_QUERY_WINDOW  1
#define SM_IS_ALIVE      2
#define SM_IS_DEAD       3

/////////////////////////////////////////////////////////////////////////////
// CTalkNav command target

class CTalkNav : public CCmdTarget
{
	DECLARE_DYNCREATE(CTalkNav)

	CTalkNav();           // protected constructor used by dynamic creation

// Attributes
public:

// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CTalkNav)
	public:
	virtual void OnFinalRelease();
	//}}AFX_VIRTUAL

// Implementation
protected:
	virtual ~CTalkNav();

	// Generated message map functions
	//{{AFX_MSG(CTalkNav)
		// NOTE - the ClassWizard will add and remove member functions here.
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()
	DECLARE_OLECREATE(CTalkNav)

	// Generated OLE dispatch map functions
	//{{AFX_DISPATCH(CTalkNav)
	afx_msg long BrowseURL(LPCTSTR url);
	afx_msg long EditURL(LPCTSTR url);
	//}}AFX_DISPATCH
	DECLARE_DISPATCH_MAP()
	DECLARE_INTERFACE_MAP()
};

/////////////////////////////////////////////////////////////////////////////
//
//  ITalkSMClient usage:
//
//  Create an global instance of the class during app initialization:
//
//      talkSM = new ITalkSMClient;
//
//  You can query this instance to know whether SM server is registered:
//
//      if (talkSM->IsRegistered()) ...
//
//  During app intitialization, you need to find out whether SM is alive. Do
//  this using "__talkSM_SM_IsAlive__" registered Windows message, with wParam
//  set to 1. A return value of 0x015DEAD0 indicates that SM is alive. If this
//  is the case, do the following:
//
//      talkSM->SetKnownSMState(TRUE);
//
//  While the navigator is running, SM may send a message telling it that it
//  is either being openned or shut down. This will be done using the same
//  registered Windows message "__talkSM_SM_IsAlive__" with wParam set to 2
//  (for openning) or 3 (for shutting down). When this happens, you should
//  call SetKnownSMState with either TRUE or FALSE as appropriate.
//
//  If these rules are followed, then you can just call SavedURL, LoadingURL,
//  and BecomeActive as needed.
//
//
/////////////////////////////////////////////////////////////////////////////
// ITalkSMClient wrapper class

class ITalkSMClient : public COleDispatchDriver
{
private:
    BOOL m_registered;
    BOOL m_alive;
    BOOL m_connected;
    BOOL m_retried;

    BOOL Connect(void);
    void Disconnect(void);
    BOOL Reconnect(void);
    BOOL IsConnected(void);

public:
    ITalkSMClient(void);
    ~ITalkSMClient(void);

    inline BOOL IsRegistered(void) {return m_registered;}
    inline void SetKnownSMState(BOOL isAlive) {m_alive = isAlive;}

    long SavedURL(LPCTSTR url);
    long LoadingURL(LPCTSTR url);
    long BecomeActive();
};

#endif //TALK_H
#endif //_WIN32
#endif //EDITOR
