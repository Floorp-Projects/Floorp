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

// edcombtb.h : header file
//
//  Custom Toolbar class to facilitate using ComboBoxes in CToolBar type of toolbar
//  
//  Features:
//    - Manages placement of CComboBox (or derived class) withing toolbar;
//        (Owner creates the ComboBox(es) and manages messages)
//    - Accessor functions are command ID - driven, rather than index into controll array
//    - Easy to use Enable, SetCheck functions that redraw only when state changes
//    - Allows user configuring: hiding controls with space compaction
//      (relative position is cannot be changed, it just turn each on/off)
//
//  Owner must fill comboboxes and manage command messages
//
//  Created: 8/38/95 by Charles Manske
//
#ifdef EDITOR
#ifndef _EDCOMBTB_H
#define _EDCOMBTB_H

#include    "edres2.h"  // For IDD_CONFIG_COMBOBAR

#include "toolbar2.h"

#define     SEPARATOR_WIDTH     8   // 6-pixel gap + 2 for overlap

// Use this ID to place-hold for ComboBox in ID Array
#define     ID_COMBOBOX         (UINT)-1

typedef struct _TB_CONTROLINFO {
    UINT        nID;                // Button or combobox Command ID, or ID_SEPARATOR
    BOOL        bIsButton;          // Test this for Check/Uncheck functions
    BOOL        bDoOnButtonDown;    // Trigger action on button down (normal is button up)
    int         nImageIndex;        // Relative to images in Bitmap (ignored for sep.,Combo)
    int         nWidth;             // Width of separator or ComboBox
    BOOL        bShow;              // If FALSE, control doesn't appear on toolbar
    BOOL        bComboBox;          // TRUE if combobox
    CComboBox * pComboBox;          // Must be supplied by owner of ComboBox
} TB_CONTROLINFO, *LPTB_CONTROLINFO;

class CConfigComboBar;

/////////////////////////////////////////////////////////////////////////////
// CComboToolBar window

class CComboToolBar : public CToolBar
{
// Construction
public:
	CComboToolBar();

// Attributes
public:

    // Should be same as public CToolTar::m_nCount (includes separators)
    // int               m_nItemCount;      
    int               m_nButtonCount;
    int               m_nComboBoxCount;
    int               m_nControlCount;   // Count of buttons + comboboxes
    CSize             m_sizeButton;

    LPTB_CONTROLINFO  m_pInfo;
    SIZE              m_sizeImage;  // Size of 1 button image
    UINT              m_nIDBitmap;

private:
    UINT              m_nComboTop;
    BOOL *            m_pEnableConfig;
	CNSToolbar2*	  m_pToolbar;

#ifdef XP_WIN16
	CNSToolTip      * m_pToolTip;
#endif

// Operations
public:

    // Combination of CToolBar::Create(), LoadBitmap(), and SetButtons() 
    //  Assumes docking at all sides if no comboxes, or just Top or Bottom if combobox(es) used.
    //  If no nBitmapID, then only ComboBoxes in toolbar
    //
    //  lpIDArray is same as for CToolBar, and MUST be supplied,
    //   But use ID_COMBOBOX to place-hold where comboboxes are desired
    //
    //  nIDCount must be >=1
    //  All size values will assume Windows small-button guidelines defaults if 0
    //
    BOOL Create( BOOL bIsPageComposer, CWnd* pParent, UINT nIDBar, UINT nIDCaption,
                 UINT * pIDArray, int nIDCount,      // Command ID array and count
                 UINT nIDBitmap, SIZE sizeButton, SIZE sizeImage );

    // After creating toobar, call this to enable/disable action on button down
    // Used primarily when action is creation of a CDropdownToolbar
    void SetDoOnButtonDown( UINT nID, BOOL bSet );

    // After creating toobar, call this to set combobox command ID and its full size
    //   (including dropdown height) of each combobox used
    // If nHeight = 0, Height is calculated from number of items.
    // Call in the same order as appearance in toolbar
    void SetComboBox( UINT nID, CComboBox * pComboBox, UINT nWidth, UINT nListWidth, UINT nListHeight );
    
    // Get the rect of a button in screen coordinates
    // Used primarily with CDropdownToolbar
    BOOL GetButtonRect( UINT nID, RECT * pRect );
    
    // CToolBar has public GetCount() to return m_nCount
    // inline UINT GetCount() { return m_nItemCount; }
    inline UINT GetButtonCount() { return m_nButtonCount; }
    inline UINT GetComboBoxCount() { return m_nComboBoxCount; }

    // These are better than CCmdUI versions because they
    //   change state only if current state is different

    void EnableAll( BOOL bEnable );           // Enable/Disable all controls
    void Enable( UINT nID, BOOL bEnable );    // or just one
    void _Enable( int iIndex, BOOL bEnable ); // Index into array of all controls
    
    // Removes/Restores item from configure dialog
    void EnableConfigure( UINT nID, BOOL bEnable );

    // iCheck: 0 = no, 1 = checked, 2 = indeterminate 
    void SetCheck( UINT nID, int iCheck );  

    // Set all states with array of new values
    void SetCheckAll( int *  pCheckArray );
    
    // SetCheck for just one item
    void _SetCheck( int iIndex, int iCheck );

    // Supply an array of new Show states for ALL items
    // must point to array of length m_nCount;
    void ShowAll( BOOL * pShowArray );
    void Show( UINT nID, BOOL bShow );              // or change just one control
    void ShowByIndex( int iIndex, BOOL bShow );     // or one control using index
    
    // Allow user to select which controls to show/hide on the toolbar!
//   void ConfigureToolBar(CWnd *pParent = NULL);

    // Get pointer to toolbar item info
    inline LPTB_CONTROLINFO GetInfoPtr() { return m_pInfo; }

    // Use with GetWindowRect when real window
    //  size is needed for docked toolbars
    //  (GetWindowRect reports full client width if docked top/bottom,
    //   or client height if docked on side).  If bCNSToolbar is true then include
	// the CNSToolbar in the width.
    int GetToolbarWidth(BOOL bCNSToolbar = TRUE);
    //void GetToolbarWindowRect(RECT * pRect);

   // Used after changing Show state of 1 or more items 
    //  to move comboboxes and adjust spacing
    // Use nStart to recalc starting at the control just changed
    // Win16: This MUST be called at least once after all comboboxes
    //   are set and layout is final -- to assign Win16 tooltips
    void RecalcLayout( int nStart = 0);

	// Set a child toolbar to appear on the right side of this toolbar
	void SetCNSToolbar(CNSToolbar2 *pToolbar);

	// so we can pass this event on to the buttons
	virtual void OnUpdateCmdUI( CFrameWnd* pTarget, BOOL bDisableIfNoHndler );

	// How big do we need to be
	CSize CalcDynamicLayout(int nLength, DWORD dwMode );


// Implementation
public:
	virtual ~CComboToolBar();

	// Generated message map functions
protected:
#ifdef XP_WIN16
    BOOL PreTranslateMessage(MSG* pMsg);    // For tooltip message routing
#endif
	//{{AFX_MSG(CComboToolBar)
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnSize( UINT nType, int cx, int cy );
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()
};

#endif // _EDCOMBTB_H
#endif // EDITOR
