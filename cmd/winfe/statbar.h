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

///
//
// STATBAR.H
//
// DESCRIPTION:
//		This file contains the declaration of the CNetscapeStatusBar
//		class.
//
// CREATED:	03/01/96, 12:11:57		AUTHOR: Scott Jones
//
///


#if	!defined(__STATBAR_H__)
#define	__STATBAR_H__

#ifndef	__AFXWIN_H__
	#error include 'stdafx.h' before including this	file for PCH
#endif

#include "taskbar.h"

/****************************************************************************
*
*	Class: CNetscapeStatusBar
*
*	DESCRIPTION:
*		This object encapsulates the functionality of our status bar. We do
*		some special things, like the progress bar and security icon.
*
****************************************************************************/

typedef int EStatBarMode;

#define eSBM_INVALID -1
#define eSBM_Simple   0
#define eSBM_Panes    1
#define eSBM_LAST     2 // Derived classes can have modes starting with this enum

#define CNetscapeStatusBarBase	CStatusBar

class CNetscapeStatusBar : public CNetscapeStatusBarBase
{
   // Must do this so we can use IsKindOf
   DECLARE_DYNAMIC(CNetscapeStatusBar)
    
public:
   CNetscapeStatusBar();
   ~CNetscapeStatusBar();
#ifdef MOZ_OFFLINE
   BOOL Create( CWnd *pParent, BOOL bxxx = TRUE, BOOL bTaskbar = TRUE,
				BOOL bOnline = TRUE);
#else //MOZ_OFFLINE
   BOOL Create( CWnd *pParent, BOOL bxxx = TRUE, BOOL bTaskbar = TRUE );
#endif //MOZ_OFFLINE
   BOOL SetIndicators( const UINT* lpIDArray, int nIDCount );
   BOOL ResetPanes( EStatBarMode enStatBarMode, BOOL bForce = FALSE );
   
   void SetPercentDone( const int32 nPercent );
   
   EStatBarMode GetStatBarMode() { return m_enStatBarMode; }
   
   void ShowTaskBar( int iCmdShow );
   
   void StartAnimation();
   void StopAnimation();

   BOOL SetTaskBarPaneWidth( int iWidth );
   BOOL SetTaskBarSize( CDockedTaskBar *pTaskBar );
   
 #ifdef _WIN32
   CWnd *SetParent( CWnd *pWndNewParent );
 #endif
 
protected:
	virtual BOOL CreateDefaultPanes();
	virtual void SaveModeState();
	virtual void SetupMode();
   
   virtual BOOL PreTranslateMessage( MSG *pMsg );
   
protected:
   //{{AFX_MSG(CNetscapeStatusBar)
   afx_msg void OnPaint();
   afx_msg void OnDestroy();
   afx_msg void OnSize(UINT nType, int cx, int cy);
   afx_msg void OnLButtonDown( UINT nFlags, CPoint point );   
   //}}AFX_MSG

protected:
   void DrawProgressBar();
   void DrawSecureStatus(HDC hdc);
   void DrawSignedStatus(HDC hdc);
#ifdef MOZ_OFFLINE
   void DrawOnlineStatus(HDC hdc);
#endif
   
   // Pulsing vapor trails (aka Cylon) mode methods   
protected:
   void StartPulse();
   void StopPulse();
   void PulsingVaporTrails();
   static void CALLBACK EXPORT PulseTimerProc( HWND, UINT, UINT, DWORD );

private:
   class CParentSubclass // Subclasses parent's window
   {
   public:
       CParentSubclass( CWnd *pParent, CNetscapeStatusBar *pStatusBar );
       ~CParentSubclass();

   public:
       void SetPrevMode( EStatBarMode enPrevMode ) { m_enPrevMode = enPrevMode; }
       EStatBarMode GetPrevMode() { return m_enPrevMode; }

       void StartMonitor();
       void StopMonitor();
       
   public:        
       static LRESULT CALLBACK EXPORT ParentSubclassProc( HWND, UINT, WPARAM, LPARAM );
       static void CALLBACK EXPORT MonitorTimerProc( HWND, UINT, UINT, DWORD );
       
   private:
       CNetscapeStatusBar *   m_pStatusBar;
       CWnd *                 m_pParent;
       WNDPROC                m_pfOldWndProc;
       
       EStatBarMode           m_enPrevMode;
       
       UINT                   m_uTimerId;       
   };
   friend class CParentSubclass;
   friend LRESULT CALLBACK EXPORT CParentSubclass::ParentSubclassProc( HWND, UINT, WPARAM, LPARAM );
   
private:   
   DECLARE_MESSAGE_MAP()

public:
   CFrameGlue *m_pProxy2Frame; // Used while serving OLE container; mainly for JavaScript
#ifdef MOZ_OFFLINE
   static CPtrArray gStatusBars; 
#endif
protected:
   int32 m_nDone;         // Percentage for progress
   
   int32 m_iAnimRef;      // Non-zero if animation in progress for parent frame window
   
   UINT  m_uTimerId;
   UINT  m_uVaporPos;
   
   BOOL  m_bTaskbar;
   
   BOOL m_bSecurityStatus;
 #ifdef MOZ_OFFLINE
   BOOL m_bOnlineStatus;
#endif  
   EStatBarMode m_enStatBarMode;
   
   CNetscapeStatusBar::CParentSubclass *pParentSubclass;

   CNSToolTip2 *m_pTooltip;
   
   static HBITMAP sm_hbmpSecure;
   static SIZE sm_sizeSecure;
   static int sm_iRefCount;

#ifdef MOZ_OFFLINE
   static HBITMAP sm_hbmpOnline;
   static SIZE sm_sizeOnline;
   static int sm_iOnlineRefCount;
#endif //MOZ_OFFLINE

   // Mode state info for particular pane modes   
private:
   int               m_iStatBarPaneWidth; // eSBM_Panes: save the width of the Taskbar pane
   CDockedTaskBar *  m_pTaskBar; // Set if the task bar needs resizing
   
   UINT *m_anIDSaved; // Save the panes we were displaying
   int m_iSavedCount;
};

#endif // __STATBAR_H__
