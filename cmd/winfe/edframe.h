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

#ifdef EDITOR

#ifndef EDFRAME_H
#define EDFRAME_H

#include "mainfrm.h"
#include "edcombtb.h"
#include "pa_tags.h"   // Needed for P_... container tag defines
#include "edprops.h"
#include "edtrccln.h"

// Matches size of custom colors in Windows color picker
#define	MAX_FONT_COLOR	16

// Global for app - called during startup
BOOL FE_FindPreviousInstance(LPCSTR szURL, BOOL bStartEditor);

// Checks if there is any active editor plugin and lets the user stop it.
// Return: TRUE - no plugin was running or user stopped the active plugin
//         FALSE - user didn't want to stop the plugin
BOOL CheckAndCloseEditorPlugin(MWContext *pMWContext);

void WFE_InitComposer();

///////////////////////////////////////////////////////////////////////////////////////////////
// Class to own the Composer toolbar so it can be used by both Web and Mail Composer frames

#define WM_TOOLCONTROLLER WM_USER+512
#define DISPLAY_EDIT_TOOLBAR		0x1
#define DISPLAY_FORMAT_TOOLBAR		0x2  // NOT USED
#define DISPLAY_CHARACTER_TOOLBAR	0x4
#define DISPLAY_FORMS_TOOLBAR		0x8
#define INDEX_OTHER                 -2

class CEditToolBarController {
public:
    CEditToolBarController(CWnd * pParent = NULL);    
	~CEditToolBarController();

private:
    CComboToolBar    m_wndCharacterBar;
	CCommandToolbar* m_pCharacterToolbar;
	// Custom toolbar allows Comboboxes, but we own them
	CComboBox        m_ParagraphCombo;
	CColorComboBox   m_FontColorCombo;
	CNSComboBox      m_FontFaceCombo;
	CNSComboBox      m_FontSizeCombo;
	CWnd *			 m_pWnd;
    // Save the index in listbox for "Other..." item used to trigger dialogs
    int              m_iFontColorOtherIndex;

public:
	void ShowToolBar( BOOL bShow, CComboToolBar * pToolBar = NULL);

	inline CWnd * GetParent() { return m_pWnd; } 

    inline CComboBox* GetParagraphCombo() { return &m_ParagraphCombo; }
    inline CColorComboBox* GetFontColorCombo() { return &m_FontColorCombo; }
    inline CNSComboBox* GetFontFaceCombo() { return &m_FontFaceCombo; }
    inline CNSComboBox* GetFontSizeCombo() { return &m_FontSizeCombo; }
    inline CComboToolBar* GetCharacterBar() { return( ::IsWindow(m_wndCharacterBar.m_hWnd) ? &m_wndCharacterBar : 0); }
	inline CCommandToolbar *GetCNSToolbar() { return( ::IsWindow(m_pCharacterToolbar->m_hWnd) ? m_pCharacterToolbar : 0); }
    // Pass in MWContext to get palette to be used by comboboxes and display status messages
	BOOL CreateEditBars(MWContext *pMWContext,
	                	unsigned ett = DISPLAY_EDIT_TOOLBAR|DISPLAY_CHARACTER_TOOLBAR);
    
    // Calls appropriate GetCurSel(), but returns INDEX_OTHER if "Other..." item selected
    int GetSelectedFontFaceIndex();
    int GetSelectedFontColorIndex();
    
protected:
	CCommandToolbar* CreateCharacterToolbar(int nCount);

};

/////////////////////////////////////////////////////////////////////////////////
// This is Editors wrapper around main frame so we can handle our own UI
//  and keep out of the way of Browser crew

class CEditFrame : public CMainFrame
{
public: // create from serialization only
    CEditFrame();
    DECLARE_DYNCREATE(CEditFrame)

	CEditToolBarController * m_pToolBarController;

	// Overriden for setting the command help for editor plugin tools and edit history list
	virtual void GetMessageString(UINT MenuID, CString& Message) const;

// Attributes
public:
    // Store context associated with URL that
    //   we want to convert to a new doc
    // Signals appropriate action in FE_EditorGetUrlExitRoutine()
    MWContext      * m_pTemplateContext;
    // Set this to trigger importing a text file just like
    //  we handle a template file - change it to an Untitled new doc
    //  to prevent overwriting original file    
    BOOL             m_bImportTextFile;

private:
    // Save original state of browser toolbars
    BOOL             m_bSaveShowURLBar;
    BOOL             m_bSaveShowBrowseBar;
    BOOL             m_bSaveShowStarterBar;

// Implementation
public:
    virtual         ~CEditFrame();
    virtual BOOL    OnCreateClient(LPCREATESTRUCT lpcs, CCreateContext *pContext);

    void SetNavToolbarButtonCommandID( BOOL bOpenNewWindow );

	// Don't allow NavCenter windows to be placed in edit windows.
	inline BOOL AllowDocking() { return TRUE; }
    
	inline CComboBox* GetParagraphCombo() { return m_pToolBarController->GetParagraphCombo(); }
    inline CColorComboBox* GetFontColorCombo() { return m_pToolBarController->GetFontColorCombo(); }
    inline CComboBox* GetFontSizeCombo() { return m_pToolBarController->GetFontSizeCombo(); }
    inline CComboToolBar* GetCharacterBar() { return m_pToolBarController->GetCharacterBar(); }

    BOOL IsEditFrame() { return TRUE; }

    // Save given URL to list in template preferences.
    // If NULL, get location from "editor.template_last_loc"
    void SaveTemplateLocation(char *pLastLoc = NULL);

#ifdef _DEBUG
    virtual void AssertValid() const;
    virtual void Dump(CDumpContext& dc) const;
#endif

protected:  
    void DockControlBarLeftOf(CToolBar* Bar,CToolBar* LeftOf);
    //void BuildRecentFilesMenu(CMenu * pMenu);

    // Prompts user to supply required URL if missing from preferences.
    // Allows selecting a local file
    // nID_Caption is string ID for dialog,
    // nID_Msg used for prompt above edit field
    // nID_FileCaption is used for OpenFile dialog caption
    char *GetLocationFromPreferences(const char *pPrefName, UINT nID_Msg = 0, UINT nID_Caption = 0,  UINT nID_FileCaption = 0);

	HPALETTE m_hPal;

// Generated message map functions
protected:
    //{{AFX_MSG(CEditFrame)
	afx_msg LONG OnToolController(UINT,LONG);
	afx_msg int  OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnClose();
	afx_msg void OnFileClose();
	afx_msg void OnNewFrame();
	afx_msg BOOL OnQueryEndSession();
    afx_msg void OnEditFindReplace();
    afx_msg void OnShowBookmarkWindow();
//}}AFX_MSG
	afx_msg LRESULT OnSetMessageString(WPARAM wParam, LPARAM lParam);
    LRESULT OnButtonMenuOpen(WPARAM wParam, LPARAM lParam);

    // Callback from editor.
    void RealClose();
    static void RealCloseS(void* hook);

    DECLARE_MESSAGE_MAP()

    friend class CPrefEditorPage;    
    friend class CNetscapeEditView;
};
#endif // EDFRAME_H
#endif // EDITOR
