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

#ifndef __FrameGlue_H
//	Avoid include redundancy
//
#define __FrameGlue_H

//	Purpose:	Provide a class from which all frame windows are derived from
//					so that appropriate aggregation can occur between frames.
//	Comments:	Created mainly because In place frames have a different derivation
//					tree than normal frame windows.  The only way to pass around a
//					common pointer is to introduce a third class which holds a common
//					API to the frame windows.
//	Revision History:
//		07-31-95	created GAB
//

//	Required Includes
//
#include "apichrom.h"
//	Constants
//


//	Structures
//
class CFrameGlue	{

//	Frames have concepts of multiple contexts.
//	Provide a way to get the active (or last active) context.
//	Provide a way to get the Main context (frames should always have one context).
private:
	CAbstractCX *m_pMainContext;
	CAbstractCX *m_pActiveContext;
	BOOL m_isBackgroundPalette;


//	Need a constructor for initializations....
protected:
	CFrameGlue();
public:
	virtual ~CFrameGlue();

//	How to get the frame glue from a frame window.
public:
	static CFrameGlue *GetFrameGlue(CFrameWnd *pFrame);

protected:
	friend class CGenericView;	//	Views can manipulate the frames active context.
	friend class CInPlaceFrame;

	void SetMainContext(CAbstractCX *pContext);
	void SetActiveContext(CAbstractCX *pContext);
public:
	virtual CAbstractCX *GetMainContext() const;
	virtual CAbstractCX *GetActiveContext() const;
	// For all those routines that think contexts are window contexts
	virtual CWinCX *GetMainWinContext() const;
	virtual CWinCX *GetActiveWinContext() const;
	BOOL IsBackGroundPalette() {return 	m_isBackgroundPalette;}
	void SetIsBackgroundPalette(BOOL flag) {m_isBackgroundPalette = flag;}
	BOOL RealizePalette(CWnd *pWnd, HWND hFocusWnd,BOOL background);

	void ClearContext(CAbstractCX *pGone);
	
    // Override this in Editors to return TRUE. Default = FALSE
    virtual BOOL IsEditFrame();

protected:
	IChrome	*m_pChrome;

private:

	BOOL m_bCanRestoreState;
	int m_iSaveToolBarStyle;
	BOOL m_bSaveToolBarVisible;
	BOOL m_bSaveLocationBarVisible;
	BOOL m_bSaveStarterBarVisible;

public:
	void SaveBarState()	{
		m_bCanRestoreState = TRUE;
		m_bSaveToolBarVisible = m_pChrome->GetToolbarVisible(ID_NAVIGATION_TOOLBAR);
		m_bSaveLocationBarVisible = m_pChrome->GetToolbarVisible(ID_LOCATION_TOOLBAR) ||
									m_pChrome->GetToolbarVisible(ID_MESSENGER_INFOBAR);
		m_bSaveStarterBarVisible = m_pChrome->GetToolbarVisible(ID_PERSONAL_TOOLBAR);
	}
	void RestoreBarState()	{
		if(m_bCanRestoreState == TRUE)	{
			m_pChrome->ShowToolbar(ID_NAVIGATION_TOOLBAR, m_bSaveToolBarVisible);
			m_pChrome->ShowToolbar(ID_LOCATION_TOOLBAR, m_bSaveLocationBarVisible);
			m_pChrome->ShowToolbar(ID_MESSENGER_INFOBAR, m_bSaveLocationBarVisible);
			m_pChrome->ShowToolbar(ID_PERSONAL_TOOLBAR, m_bSaveStarterBarVisible);
		}
	}

	IChrome *GetChrome() const { return m_pChrome; }

//	You don't necessarily have to use these, but here they are.
public:
    int			  m_iCSID;

//	Mostly shared prefs.
public:
	XP_Bool m_bShowToolbar;
	XP_Bool m_bLocationBar;
	XP_Bool m_bStarter;

//	Keep track of the find replace dialog.
//	There can be only one.
private:
	CNetscapeFindReplaceDialog *m_pFindReplace;
public:
	BOOL CanFindReplace() const	{
		return(m_pFindReplace == NULL);
	}
	void SetFindReplace(CNetscapeFindReplaceDialog *pDialog)	{
		ASSERT(m_pFindReplace == NULL);
		m_pFindReplace = pDialog;
	}
	CNetscapeFindReplaceDialog *GetFindReplace() {
		return m_pFindReplace;
	}
	void ClearFindReplace()	{
		m_pFindReplace = NULL;
	}

//	Keeping track of the last active frame...
protected:
	static CPtrList m_cplActiveFrameStack;
	static CPtrList m_cplActiveNotifyCBList;
	static CPtrList m_cplActiveContextCBList;

	void SetAsActiveFrame();

public:
	typedef void (*ActiveNotifyCB)(CFrameGlue *);
	typedef void (*ActiveContextCB)(CFrameGlue *, CAbstractCX *);

	static void AddActiveNotifyCB( ActiveNotifyCB cb );
	static void RemoveActiveNotifyCB( ActiveNotifyCB cb );
	static void AddActiveContextCB( ActiveContextCB cb );
	static void RemoveActiveContextCB( ActiveContextCB cb );

	static CFrameGlue *GetLastActiveFrame(MWContextType cxType = MWContextAny, int nFindEditor = FEU_FINDBROWSERONLY);

	//  bUseSaveInfo is true if custToolbar's save info field should be used to disregard frames if save info is FALSE
	static CFrameGlue *GetLastActiveFrameByCustToolbarType(CString custToolbar, CFrameWnd *pCurrentFrame, BOOL bUseSaveInfo);
	static CFrameGlue *GetBottomFrame(MWContextType cxType = MWContextAny, int nFindEditor = FEU_FINDBROWSERONLY);

	static int GetNumActiveFrames(MWContextType cxType = MWContextAny, int nFindEditor = FEU_FINDBROWSERONLY);
	
//	How to look up a frame window via context ID.

	static CFrameGlue *FindFrameByID(DWORD dwID, MWContextType cxType = MWContextAny);

//	A common command ID handler for all frames.
//	Should be called by your derived OnCommand implementation.
protected:
// This is a quick hack until the full security advisor is done.
	void SecurityDialog();
public:
	BOOL CommonCommand(UINT wParam, LONG lParam);

// Helper funtion to toggle WS_CLIPCHILDREN for a branch of the frame heirarchy
protected:
    CMapPtrToPtr * m_pClipChildMap;
public:
    void ClipChildren(CWnd *pWnd, BOOL bSet);

//	Pure virtual APIs, must be correctly defined by the deriving frame window class.
public:
	//	Access to the CFrameWnd pointer.
	//	This could possibly return NULL if someone is not really a frame window, but
	//		wants to act like one via deriving from CFrameGlue....
	virtual CFrameWnd *GetFrameWnd() = 0;

	//	Updating of the history window, if around.
	virtual void UpdateHistoryDialog() = 0;
	virtual void SetSecurityStatus(int) {}
};

class CNullFrame : public CFrameGlue	{
	virtual CAbstractCX *GetMainContext() const;
	virtual CAbstractCX *GetActiveContext() const;
	virtual CFrameWnd *GetFrameWnd();
	virtual void UpdateHistoryDialog();
};

//	Global variables
//

//	Macros
//

//	Function declarations
//
CFrameGlue *GetFrame(MWContext *pContext);


#endif // __FrameGlue_H
