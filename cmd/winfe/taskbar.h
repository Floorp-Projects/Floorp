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
// $Revision: 3.1 $
//
// TASKBAR.H
//
// DESCRIPTION:
//		This file contains the declarations of the various task bar related
//		classes.
//
// AUTHOR: Scott Jones
//
///


#if	!defined(__TASKBAR_H__)
#define	__TASKBAR_H__

#ifndef	__AFXWIN_H__
	#error include 'stdafx.h' before including this	file for PCH
#endif

#include "tlbutton.h"

#define BROWSER_ICON_INDEX	0
#define INBOX_ICON_INDEX	1
#define UNKNOWN_MAIL_ICON_INDEX 2
#define NEW_MAIL_ICON_INDEX 3
#define NEWS_ICON_INDEX		4
#define COMPOSE_ICON_INDEX	5

/****************************************************************************
*
*	Class: CTaskBarButtonDropTarget
*
*	DESCRIPTION:
*		
*		Some taskbar buttons may want to be drop targets.  So here's the class.
*
****************************************************************************/

#define CTaskBarButtonDropTargetBase	CToolbarButtonDropTarget

class CTaskBarButtonDropTarget : public CTaskBarButtonDropTargetBase
{
	public:
		CTaskBarButtonDropTarget(){m_pButton = NULL;}
	protected:
		virtual DROPEFFECT ProcessDragEnter(CWnd *pWnd, COleDataObject *pDataObject, 
			DWORD dwKeyState, CPoint point);
		virtual DROPEFFECT ProcessDragOver(CWnd *pWnd, COleDataObject *pDataObject, 
			DWORD dwKeyState, CPoint point);
		virtual BOOL ProcessDrop(CWnd *pWnd, COleDataObject *pDataObject, 
			DROPEFFECT dropEffect, CPoint point) = 0;

}; 

#define CBrowserButtonDropTargetBase	CTaskBarButtonDropTarget

class CBrowserButtonDropTarget : public CBrowserButtonDropTargetBase
{
	public:
		CBrowserButtonDropTarget(){m_pButton = NULL;}
	protected:
		virtual BOOL ProcessDrop(CWnd *pWnd, COleDataObject *pDataObject, 
			DROPEFFECT dropEffect, CPoint point);

}; 

#define CComposerButtonDropTargetBase	CTaskBarButtonDropTarget

class CComposerButtonDropTarget : public CComposerButtonDropTargetBase
{
	public:
		CComposerButtonDropTarget(){m_pButton = NULL;}
	protected:
		virtual DROPEFFECT ProcessDragEnter(CWnd *pWnd, COleDataObject *pDataObject, 
			DWORD dwKeyState, CPoint point);
		virtual DROPEFFECT ProcessDragOver(CWnd *pWnd, COleDataObject *pDataObject, 
			DWORD dwKeyState, CPoint point);
		virtual BOOL ProcessDrop(CWnd *pWnd, COleDataObject *pDataObject, 
			DROPEFFECT dropEffect, CPoint point);

}; 

#define CMessengerButtonDropTargetBase	CTaskBarButtonDropTarget

class CMessengerButtonDropTarget : public CMessengerButtonDropTargetBase
{
	public:
		CMessengerButtonDropTarget(){m_pButton = NULL;}
	protected:
		virtual DROPEFFECT ProcessDragEnter(CWnd *pWnd, COleDataObject *pDataObject, 
			DWORD dwKeyState, CPoint point);
		virtual DROPEFFECT ProcessDragOver(CWnd *pWnd, COleDataObject *pDataObject, 
			DWORD dwKeyState, CPoint point);
		virtual BOOL ProcessDrop(CWnd *pWnd, COleDataObject *pDataObject, 
			DROPEFFECT dropEffect, CPoint point);

}; 

#define CCollabraButtonDropTargetBase	CTaskBarButtonDropTarget

class CCollabraButtonDropTarget : public CCollabraButtonDropTargetBase
{
	public:
		CCollabraButtonDropTarget(){m_pButton = NULL;}
	protected:
		virtual DROPEFFECT ProcessDragEnter(CWnd *pWnd, COleDataObject *pDataObject, 
			DWORD dwKeyState, CPoint point);
		virtual DROPEFFECT ProcessDragOver(CWnd *pWnd, COleDataObject *pDataObject, 
			DWORD dwKeyState, CPoint point);
		virtual BOOL ProcessDrop(CWnd *pWnd, COleDataObject *pDataObject, 
			DROPEFFECT dropEffect, CPoint point);

}; 

/****************************************************************************
*
*	Class: CTaskIcon
*
*	DESCRIPTION:
*		This class represents the abstraction of a task icon object. It
*		encapsulates the data that is used to construct CTaskIconWnd objects.
*
****************************************************************************/

#define CTaskIconBase	CObject

class CTaskIcon : public CTaskIconBase
{
	public:
		CTaskIcon(UINT idTask, CWnd * pwndNotify, DWORD dwMessage,
			UINT idBmpLarge, int indexBmpLarge, UINT idBmpSmall,
			int indexBmpSmall, UINT idHorizText, UINT idVertText,
			UINT idDockedTip, UINT idFloatingTip);
		virtual ~CTaskIcon();
		 
		const UINT GetTaskID() const
		{
			return(m_idTask);
		}
		CWnd * GetNotifyWnd() const
		{
			return(m_pwndNotify);
		}
		const DWORD GetNotifyMessage() const
		{
			return(m_dwMessage);
		}
		const UINT GetLargeBmpID() const
		{
			return(m_idBmpLarge);
		}
		const UINT GetSmallBmpID() const
		{
			return(m_idBmpSmall);
		}
		void SetLargeBmpID(UINT idBmp)
		{
			m_idBmpLarge = idBmp;
		}
		void SetSmallBmpID(UINT idBmp)
		{
			m_idBmpSmall = idBmp;
		}

		const int GetLargeBitmapIndex() const
		{
			return (m_indexBmpLarge);
		}
		const int GetSmallBitmapIndex() const
		{
			return(m_indexBmpSmall);
		}
		void SetLargeBitmapIndex(int indexBmp)
		{
			m_indexBmpLarge = indexBmp;
		}
		void SetSmallBitmapIndex(int indexBmp)
		{
			m_indexBmpSmall = indexBmp;
		}

		const UINT GetHorizTextID()
		{
			return m_idHorizText;
		}

		const UINT GetVertTextID()
		{
			return m_idVertText;
		}

		const UINT GetDockedTipID()
		{
			return m_idDockedTip;
		}

		const UINT GetFloatingTipID()
		{
			return m_idFloatingTip;
		}

	protected:
		CWnd * m_pwndNotify;	// Notifications go to this window
		DWORD m_dwMessage;		// Callback message
		UINT m_idTask;			// Task identifier for this icon
		UINT m_idBmpLarge;		// Bitmap resource ID for large icon
		UINT m_idBmpSmall;		// Bitmap resource ID for small icon
		int m_indexBmpLarge;    // index for large bitmap
		int m_indexBmpSmall;    // index for small bitmap
		UINT m_idHorizText;		// String resource ID of horizontal text
		UINT m_idVertText;		// String resource ID of vertical text
		UINT m_idDockedTip;		// String resource ID of docked tool tip text
		UINT m_idFloatingTip;	// String resource ID of floating tool tip text

	private:

}; // END OF CLASS CTaskIcon()


/****************************************************************************
*
*	Class: CTaskIconArray
*
*	DESCRIPTION:
*		This is a container class for holding CTaskIcon objects.
*
****************************************************************************/

#define CTaskIconArrayBase	CObArray

class CTaskIconArray : public CTaskIconArrayBase
{
	public:
		CTaskIconArray(){}
		~CTaskIconArray()
		{
			DeleteAll();
		}
		
		int Add(CTaskIcon * pIcon)
		{
			return(CTaskIconArrayBase::Add(pIcon));
		}
		CTaskIcon * Get(int nIndex) const
		{
			return((CTaskIcon *)GetAt(nIndex));
		}
		int FindByID(UINT idTask);
		void DeleteAll();
		
	protected:

	private:

}; // END OF CLASS CTaskIconArray()


/****************************************************************************
*
*	Class: CTaskIconWnd
*
*	DESCRIPTION:
*		This object represents a notification icon. It can be embeded within
*		a task bar and provides mouse notifications to a given window. It is
*		a window that paints its own bitmap, displays tool tip text, processes
*		mouse events, etc.
*
****************************************************************************/

#define CTaskIconWndBase	CStationaryToolbarButton

class CTaskIconWnd : public CTaskIconWndBase
{
	public:
		CTaskIconWnd();
		
		BOOL CTaskIconWnd::Create(UINT idTask, CWnd * pwndNotify, DWORD dwMessage,
								  UINT idBmp, int nBitmapIndex, UINT idHorizText, UINT idVertText, UINT idText, UINT idTip, BOOL bNoviceMode,
								  CSize noviceSize, CSize advancedSize, CSize bitmapSize, CWnd * pParent,
								  const CRect & rc = CRect(0,0,0,0));
		
		// Inline access functions
		const UINT GetTaskID() const
		{
			return(m_idTask);
		}
		void SetBmpID(UINT idBmp)
		{
		 	m_idBmp = idBmp;
		}
		
		const UINT GetHorizTextID(void) { return m_idHorizText; }
		const UINT GetVertTextID(void) { return m_idVertText; }

	protected:
		// Protected destructor so no one tries to instantiate us on the
		// stack (we are an auto-deleting object)
		virtual ~CTaskIconWnd();
		virtual void PostNcDestroy();
		virtual void AddDropTargetIfStandardButton(void);

	
	//{{AFX_MSG(CTaskIconWnd)
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnRButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	//}}AFX_MSG

	private:
		CWnd * m_pwndNotify;	// Notifications go to this window
		DWORD m_dwMessage;		// Callback message
		UINT m_idTask;			// Task identifier for this icon
		UINT m_idBmp;			// Bitmap identifier for this icon
		UINT m_idHorizText;		// id when button is in horizontal mode
		UINT m_idVertText;		// id when button is in vertical mode
		
	DECLARE_MESSAGE_MAP()
	
}; // END OF CLASS CTaskIconWnd()


/****************************************************************************
*
*	Class: CTaskIconWndArray
*
*	DESCRIPTION:
*		This is a container class for holding CTaskIconWnd objects.
*
****************************************************************************/

#define CTaskIconWndArrayBase	CObArray

class CTaskIconWndArray : public CTaskIconWndArrayBase
{
	public:
		CTaskIconWndArray(){}
		~CTaskIconWndArray(){}
		
		int Add(CTaskIconWnd * pIcon)
		{
			return(CTaskIconWndArrayBase::Add(pIcon));
		}
		CTaskIconWnd * Get(int nIndex) const
		{
			return((CTaskIconWnd *)GetAt(nIndex));
		}
		int FindByID(UINT idTask);
		void DeleteAll();
		
	protected:

	private:

}; // END OF CLASS CTaskIconWndArray()


/****************************************************************************
*
*	Class: CTaskBar
*
*	DESCRIPTION:
*		This is the base class for task bar objects. All polymorphic functions
*		common to the floating, docked, or other derived task bars are
*		implemented here.
*
*		This is an abstract base class - you must instantiate one of the
*		derived types. Also, objects of this class are auto-deleting, you
*		must allocate them on the heap.
*
****************************************************************************/

#define CTaskBarBase	CWnd

class CTaskBar : public CTaskBarBase
{
	public:
		CTaskBar(int nToolbarStyle);
		virtual BOOL Create(CWnd * pParent) = 0;
		
		virtual BOOL AddTaskIcon(UINT idTask, CWnd * pwndNotify, DWORD dwMessage,
								 HBITMAP hBitmap, int nBitmapIndex, UINT idHorizText, UINT idVertText,
							  	 UINT idText, UINT idTip, int nToolbarStyle);

		BOOL ReplaceTaskIcon(UINT idTask, UINT idBmp, int nBitmapIndex);
		BOOL RemoveTaskIcon(UINT idTask);
		virtual CSize CalcDesiredDim();
		virtual CSize GetButtonDimensions();
		int GetTaskBarStyle(void) { return m_nToolbarStyle; }
		void SetTaskBarStyle(int nToolbarStyle);
		void ChangeButtonText(void);
		void ReplaceButtonBitmap(int nIndex, HBITMAP hBitmap);
		
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CTaskBar)
	//}}AFX_VIRTUAL

	protected:
		// Protected destructor so no one tries to instantiate us on the
		// stack (we are an auto-deleting object)
		virtual ~CTaskBar();
		virtual void PostNcDestroy();
		virtual void DoPaint(CPaintDC & dc) = 0;
		void LayoutIcons();
		BOOL DragBarHitTest(const CPoint & pt);
		
		void ChangeButtonStyle(void);
		CTaskIconWndArray m_TaskIconWndList;
		CSize m_noviceButtonSize;
		CSize m_advancedButtonSize;
		CSize m_IconSize;

		int m_nMaxButtonWidth;
		int m_nMaxButtonHeight;
		

		int m_nDragBarWidth;		// when horizontal
		int m_nDragBarHeight;		// when vertical
		int m_nIconSpace;
		int m_nToolbarStyle;

		BOOL m_bHorizontal;	// TRUE when oriented horizontally
		BOOL m_bShowText;	// TRUE when icon text is to be shown


	//{{AFX_MSG(CTaskBar)
	afx_msg void OnPaint();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnPaletteChanged( CWnd* pFocusWnd );
	afx_msg void OnSysColorChange();

	//}}AFX_MSG
	
	private:

	DECLARE_MESSAGE_MAP()
	
}; // END OF CLASS CTaskBar()


/****************************************************************************
*
*	Class: CDockButton
*
*	DESCRIPTION:
*		This class represents the docking (minimize) button for the floating
*		task bar.
*
****************************************************************************/

#define CDockButtonBase	CButton

class CDockButton : public CDockButtonBase
{
	public:
		CDockButton();
		BOOL Create(const CRect & rect, CWnd* pwndParent, UINT uID);
		 
	protected:
		void DrawItem(LPDRAWITEMSTRUCT lpDrawItem);
		void DrawImage(CDC * pDC, CRect & rect);
		void DrawUpButton(CDC * pDC, CRect & rect);
		void DrawDownButton(CDC * pDC, CRect & rect);
		
	//{{AFX_MSG(CDockButton)
	//}}AFX_MSG

	private:

	DECLARE_MESSAGE_MAP()
	
}; // END OF CLASS CDockButton()


/****************************************************************************
*
*	Class: CFloatingTaskBar
*
*	DESCRIPTION:
*		This derived version of CTaskBar provides a "floating" task bar. It
*		is in the form of a custom popup window.
*
****************************************************************************/

#define CFloatingTaskBarBase	CTaskBar

class CFloatingTaskBar : public CFloatingTaskBarBase
{
	public:
		CFloatingTaskBar(int nToolbarStyle, BOOL bOnTop = TRUE, BOOL bHorizontal = TRUE );
		BOOL Create(CWnd * pParent);
		 
	protected:
		// Protected destructor so no one tries to instantiate us on the
		// stack (we are an auto-deleting object)
		virtual ~CFloatingTaskBar();
		virtual void DoPaint(CPaintDC & dc);
		void PaintDragBar(CDC * pdc);
		void SetMenuState(CMenu * pMenu);

		CDockButton m_btnDock;		// Docking button
		BOOL m_bActive;				// Maintains our active state
		
	//{{AFX_MSG(CFloatingTaskBar)
	afx_msg void OnClose();
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnDock();
	afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
	afx_msg void OnRButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnMove(int x, int y);
	afx_msg LRESULT OnAddMenu(WPARAM, LPARAM); 
	afx_msg void OnAlwaysOnTop();
	afx_msg void OnShowText();
	afx_msg void OnPosition();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnInitMenu(CMenu *pMenu);
	afx_msg int OnCreate( LPCREATESTRUCT lpCreateStruct );
  

	//}}AFX_MSG
	
	private:
		BOOL m_bOnTop;		// TRUE when 'always on top' property is set
//		BOOL m_bHorizontal;	// TRUE when oriented horizontally

	DECLARE_MESSAGE_MAP()
	
}; // END OF CLASS CFloatingTaskBar()


/****************************************************************************
*
*	Class: CDockedTaskBar
*
*	DESCRIPTION:
*		This derived version of CTaskBar provides a "docked" task bar. It is
*		in the form of a mini child window embedded within its parent
*		(normally a CNetscapeStatusBar).
*
****************************************************************************/

#define CDockedTaskBarBase	CTaskBar

class CDockedTaskBar : public CDockedTaskBarBase
{
	public:
		CDockedTaskBar(int nToolbarStyle);
		BOOL Create(CWnd * pParent);
		 
	protected:
		// Protected destructor so no one tries to instantiate us on the
		// stack (we are an auto-deleting object)
		virtual ~CDockedTaskBar();
		virtual void DoPaint(CPaintDC & dc);
		void OnUnDock(CPoint & ptUL = CPoint(-1, -1));

	//{{AFX_MSG(CDockedTaskBar)
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	//}}AFX_MSG
	
	private:

	DECLARE_MESSAGE_MAP()
	
}; // END OF CLASS CDockedTaskBar()


/****************************************************************************
*
*	Class: CTaskBarArray
*
*	DESCRIPTION:
*		This is a container class for holding CTaskBar objects.
*
****************************************************************************/

#define CTaskBarArrayBase	CObArray

class CTaskBarArray : public CTaskBarArrayBase
{
	public:
		CTaskBarArray(){}
		~CTaskBarArray(){}
		
		int Add(CTaskBar * pTaskBar)
		{
			return(CTaskBarArrayBase::Add(pTaskBar));
		}
		CTaskBar * Get(int nIndex) const
		{
			return((CTaskBar *)GetAt(nIndex));
		}
		int Find(CTaskBar * pTaskBar);
		void DeleteAll();
		
	protected:

	private:

}; // END OF CLASS CTaskBarArray()


/****************************************************************************
*
*	Class: CTaskBarMgr
*
*	DESCRIPTION:
*		This class provides an object for managing all task bars within
*		the system. It maintains the abstract data for all active task icons
*		and handles the generation and switching between floating, docked or
*		other style task bars. All task bar operations should be piped though
*		this object so it can handle propagation to the appropriate active
*		task bar(s).
*
*		There are also some convenience functions available, for such actions
*		as adding a common set of task icons.
*
****************************************************************************/

// State flag definitions
#define TBAR_FLOATING	0x1L
#define TBAR_ONTOP		0x2L
#define TBAR_SHOWTEXT	0x4L
#define TBAR_HORIZONTAL	0x8L

// Forward declarations
class CNetscapeStatusBar;

#define CTaskBarMgrBase	CObject

class CTaskBarMgr : public CTaskBarMgrBase
{
	public:
		CTaskBarMgr();
		virtual ~CTaskBarMgr();
		
		BOOL Init();
		//if bAlwaysDock is TRUE then ignore the docked preference
		void LoadPrefs(BOOL bAlwaysDock);
		void SavePrefs(void);
		void RegisterStatusBar(CNetscapeStatusBar * pStatBar);
		void UnRegisterStatusBar(CNetscapeStatusBar * pStatBar);
		void OnSizeStatusBar(CNetscapeStatusBar * pStatBar);
		BOOL AddStandardIcons();
		BOOL AddTaskIcon(UINT idTask, CWnd * pwndNotify, DWORD dwMessage,
						 UINT idBmpLarge, int nLargeIndex, UINT idBmpSmall, int nSmallIndex,
						 UINT idHorizText, UINT idVertText, UINT idDockedTip, UINT idFloatingTip);
		BOOL AddTaskIcon(UINT idTask, UINT idBmpLarge, int nLargeIndex, UINT idBmpSmall,
						 int nSmallIndex, UINT idHorizText, UINT idVertText,
							  UINT idDockedTip, UINT idFloatingTip);
		BOOL ReplaceTaskIcon(UINT idTask, UINT idBmpLarge, UINT idBmpSmall, int nIndex = 0);
		BOOL RemoveTaskIcon(UINT idTask);
		void OnDockTaskBar();
		void OnUnDockTaskBar(CPoint & ptUL = CPoint(-1, -1));
		void SetTaskBarStyle(int nTaskBarStyle);
		void SetSeparateTaskBarStyle(int nTaskBarStyle);
		void ReloadIconBitmaps(CTaskBar *pTaskBar);
		// if bAdd is TRUE then add a reference, otherwise, remove one.
		void Reference(BOOL bAdd);
		void ChangeTaskBarsPalette(HWND hFocus);

		CTaskIconArray &GetIconArray(void) { return m_IconList; }

		const BOOL IsInitialized() const
		{
			return(m_bInitialized);
		}
		const CPoint & GetLastFloatPos() const
		{
			return(m_ptLastFloatPos);
		}
		void SetLastFloatPos(const CPoint & pt)
		{
			m_ptLastFloatPos = pt;
		}
		const DWORD GetStateFlags() const
		{
			return(m_dwStateFlags);
		}
		void SetStateFlags(DWORD dwStateFlags)
		{
			m_dwStateFlags = dwStateFlags;
		}
		 
		
		// Inline accessors for the state flags
		const BOOL IsFloating() const
		{
			return !!(m_dwStateFlags & TBAR_FLOATING);
		}
		void SetFloating(BOOL bFloating)
		{
			if (bFloating)
			{
				m_dwStateFlags |= TBAR_FLOATING;
			}
			else
			{
				m_dwStateFlags &= ~TBAR_FLOATING;
			}
		}
		
		const BOOL IsOnTop() const
		{
			return !!(m_dwStateFlags & TBAR_ONTOP);
		}
		void SetOnTop(BOOL bOnTop)
		{
			if (bOnTop)
			{
				m_dwStateFlags |= TBAR_ONTOP;
			}
			else
			{
				m_dwStateFlags &= ~TBAR_ONTOP;
			}
		}
		
		const BOOL IsShowText() const
		{
			return !!(m_dwStateFlags & TBAR_SHOWTEXT);
		}
		void SetShowText(BOOL bShowText)
		{
			if (bShowText)
			{
				m_dwStateFlags |= TBAR_SHOWTEXT;
			}
			else
			{
				m_dwStateFlags &= ~TBAR_SHOWTEXT;
			}
			PositionFloatingTaskBar();

		}
		
		const BOOL IsHorizontal() const
		{
			return !!(m_dwStateFlags & TBAR_HORIZONTAL);
		}
		void SetHorizontal(BOOL bHorizontal)
			{
			if (bHorizontal)
			{
				m_dwStateFlags |= TBAR_HORIZONTAL;
			}
			else
			{
				m_dwStateFlags &= ~TBAR_HORIZONTAL;
			}

			PositionFloatingTaskBar();
		}
		
	protected:
		BOOL CreateAllTaskBars();
		void DestroyAllTaskBars();
		BOOL CreateFloatingTaskBar();
		BOOL CreateDockedTaskBar(CNetscapeStatusBar * pStatBar);
		void PlaceOnStatusBar(CDockedTaskBar * pTaskBar,
			CNetscapeStatusBar * pStatBar);
		void AdjustStatusPane(CDockedTaskBar * pTaskBar,
			CNetscapeStatusBar * pStatBar);
		BOOL AddIconsToTaskBar(CTaskBar * pTaskBar);
		void PositionDockedTaskBars();
		void PositionFloatingTaskBar();
		void ChangeTaskBarStyle(int nTaskBarStyle);

		
		// State flags (these get saved in the registry by NETSCAPE.CPP)
		DWORD m_dwStateFlags;			// These bits indicate various states
										//   of the task bar
		CPoint m_ptLastFloatPos;		// Upper left corner of floating TBar
		
		BOOL m_bInitialized;			// TRUE when we've been initialized
		BOOL m_bSeparateTaskBarStyle;	// Are we getting style from out menu or externally?
		CTaskIconArray m_IconList;		// Contains list of task icon objects
		CTaskBarArray m_TaskBarList;	// Contains active task bar pointers
		CObArray m_StatBarList;			// Contains status bar pointers
		CMapPtrToPtr m_StatBarMap;		// Maps sbar pointers to tbar pointers
		int m_nTaskBarStyle;			// The current style of the taskbar
		int m_nReference;				// The number of references we have
	private:

}; // END OF CLASS CTaskBarMgr()


#endif // __TASKBAR_H__
