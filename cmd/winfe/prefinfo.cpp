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

#include "stdafx.h"
#include "prefinfo.h"
#include "prefapi.h"
#include "edt.h"
#include "prefs.h"

int PR_CALLBACK prefWatcher(const char *pPrefName, void *pData)
{
	BOOL		bReload = FALSE;
	COLORREF	rgbForegroundColor = prefInfo.m_rgbForegroundColor;
	COLORREF	rgbBackgroundColor = prefInfo.m_rgbBackgroundColor;

	switch ((int)pData) {
		case 1:
			PREF_GetColorPrefDWord("browser.anchor_color", &prefInfo.m_rgbAnchorColor);
			wfe_SetLayoutColor(LO_COLOR_LINK, prefInfo.m_rgbAnchorColor);
			bReload = TRUE;
			break;

		case 2:
			PREF_GetColorPrefDWord("browser.visited_color", &prefInfo.m_rgbVisitedColor);
			wfe_SetLayoutColor(LO_COLOR_VLINK, prefInfo.m_rgbVisitedColor);
			bReload = TRUE;
			break;

		case 3:
			/*
			 * PREF_GetBoolPref("browser.underline_anchors", &prefInfo.m_bUnderlineAnchors);
			 * bReload = TRUE;  // XXX - we really only need to refresh the page...
			 */
			break;

		case 4:
			PREF_GetBoolPref("browser.wfe.use_windows_colors", &prefInfo.m_bUseWindowsColors);
			if (prefInfo.m_bUseWindowsColors) {
				rgbForegroundColor = ::GetSysColor(COLOR_WINDOWTEXT);
				rgbBackgroundColor = ::GetSysColor(COLOR_WINDOW);
		
			} else {
				PREF_GetColorPrefDWord("browser.foreground_color", &rgbForegroundColor);
				PREF_GetColorPrefDWord("browser.background_color", &rgbBackgroundColor);
			}
			break;

		case 5:
			PREF_GetColorPrefDWord("browser.foreground_color", &rgbForegroundColor);
			break;

		case 6:
			PREF_GetColorPrefDWord("browser.background_color", &rgbBackgroundColor);
			break;

		case 7:
#ifdef MOZ_NGLAYOUT
      XP_ASSERT(0);
#else
			PREF_GetBoolPref("browser.use_document_colors", &prefInfo.m_bUseDocumentColors);
			LO_SetUserOverride(!prefInfo.m_bUseDocumentColors);
			bReload = TRUE;
#endif /* MOZ_NGLAYOUT */
			break;

		case 8:
			PREF_GetBoolPref("general.always_load_images",&prefInfo.m_bAutoLoadImages);

			CGenericFrame *pGenFrame;
			for(pGenFrame = theApp.m_pFrameList; pGenFrame; pGenFrame = pGenFrame->m_pNext) {
				pGenFrame->GetChrome()->ImagesButton(!prefInfo.m_bAutoLoadImages);
			}
			break;
	}

	// See whether the foreground or background colors changed
	if (rgbForegroundColor != prefInfo.m_rgbForegroundColor) {
		prefInfo.m_rgbForegroundColor = rgbForegroundColor;
		wfe_SetLayoutColor(LO_COLOR_FG, prefInfo.m_rgbForegroundColor);
		bReload = TRUE;
	}

	if (rgbBackgroundColor != prefInfo.m_rgbBackgroundColor) {
		prefInfo.m_rgbBackgroundColor = rgbBackgroundColor;
		wfe_SetLayoutColor(LO_COLOR_BG, prefInfo.m_rgbBackgroundColor);
		bReload = TRUE;
	}

	if (bReload)
		g_bReloadAllWindows = TRUE;

	return PREF_NOERROR;
}

int PR_CALLBACK SetToolbarButtonStyle(const char *newPref, void *data)
{
	int32 prefInt;

	PREF_GetIntPref("browser.chrome.button_style",&prefInt);
	theApp.m_pToolbarStyle = CASTINT(prefInt);

	CGenericFrame *pGenFrame;
	for(pGenFrame = theApp.m_pFrameList; pGenFrame; pGenFrame = pGenFrame->m_pNext) {
		LPNSTOOLBAR pIToolBar = NULL;
		pGenFrame->GetChrome()->QueryInterface( IID_INSToolBar, (LPVOID *) &pIToolBar );
		if (pIToolBar) {
	       pIToolBar->SetToolbarStyle(theApp.m_pToolbarStyle);
		   pIToolBar->Release();
		}
		pGenFrame->GetChrome()->SetToolbarStyle(theApp.m_pToolbarStyle);
    }

#ifdef MOZ_TASKBAR
	theApp.GetTaskBarMgr().SetTaskBarStyle(theApp.m_pToolbarStyle);
#endif /* MOZ_TASKBAR */
	
	return PREF_NOERROR;
}

CPrefInfo prefInfo;

CPrefInfo::CPrefInfo()
{
	m_rgbBackgroundColor = 0;
	m_rgbForegroundColor = 0;
	m_bUnderlineAnchors = FALSE;
	m_rgbAnchorColor = 0;
	m_rgbVisitedColor = 0;
	m_bUseWindowsColors = FALSE;
	m_bUseDocumentColors = TRUE;
	m_bAutoLoadImages = TRUE;
}

void CPrefInfo::UpdateAllWindows()
{
	// Update layout in all current windows    
	for (CGenericFrame *f = theApp.m_pFrameList; f; f = f->m_pNext) {
		CWinCX *pContext = f->GetMainWinContext();

		if (pContext && pContext->GetContext()) {
#ifdef EDITOR
			if (EDT_IS_EDITOR(pContext->GetContext())) {
				// Edit can relayout page without having to do NET_GetURL
				EDT_RefreshLayout(pContext->GetContext());

			} else 
#endif // EDITOR
            {
				pContext->NiceReload();
			}
		}
	}
}

void CPrefInfo::SysColorChange()
{
	if (m_bUseWindowsColors) {
		COLORREF	rgbForegroundColor = ::GetSysColor(COLOR_WINDOWTEXT);
		COLORREF	rgbBackgroundColor = ::GetSysColor(COLOR_WINDOW);
		BOOL		bReload = FALSE;

		// See whether the foreground or background colors changed
		if (rgbForegroundColor != m_rgbForegroundColor) {
			m_rgbForegroundColor = rgbForegroundColor;
			wfe_SetLayoutColor(LO_COLOR_FG, m_rgbForegroundColor);
			bReload = TRUE;
		}
	
		if (rgbBackgroundColor != m_rgbBackgroundColor) {
			m_rgbBackgroundColor = rgbBackgroundColor;
			wfe_SetLayoutColor(LO_COLOR_BG, m_rgbBackgroundColor);
			bReload = TRUE;
		}

		if (bReload)
			UpdateAllWindows();
	}
}

void CPrefInfo::Initialize()
{
	int32 prefInt;

	// Unvisited link color
	PREF_GetColorPrefDWord("browser.anchor_color", &m_rgbAnchorColor);
	wfe_SetLayoutColor(LO_COLOR_LINK, m_rgbAnchorColor);
    PREF_RegisterCallback("browser.anchor_color", prefWatcher, (void *)1);

	// Visited link color
	PREF_GetColorPrefDWord("browser.visited_color", &m_rgbVisitedColor);
	wfe_SetLayoutColor(LO_COLOR_VLINK, m_rgbVisitedColor);
    PREF_RegisterCallback("browser.visited_color", prefWatcher, (void *)2);
	
	// Underline links
	/* 
	 * PREF_GetBoolPref("browser.underline_anchors", &m_bUnderlineAnchors);
    	 * PREF_RegisterCallback("browser.underline_anchors", prefWatcher, (void *)3);
	 */
    
	// Text and background color. See if we should use the Windows colors
	PREF_GetBoolPref("browser.wfe.use_windows_colors", &m_bUseWindowsColors);
    PREF_RegisterCallback("browser.wfe.use_windows_colors", prefWatcher, (void *)4);
	
	if (m_bUseWindowsColors) {
		m_rgbForegroundColor = ::GetSysColor(COLOR_WINDOWTEXT);
		m_rgbBackgroundColor = ::GetSysColor(COLOR_WINDOW);

	} else {
		PREF_GetColorPrefDWord("browser.foreground_color", &m_rgbForegroundColor);
		PREF_GetColorPrefDWord("browser.background_color", &m_rgbBackgroundColor);
	}

	wfe_SetLayoutColor(LO_COLOR_FG, m_rgbForegroundColor);
	PREF_RegisterCallback("browser.foreground_color", prefWatcher, (void *)5);
	wfe_SetLayoutColor(LO_COLOR_BG, m_rgbBackgroundColor);
	PREF_RegisterCallback("browser.background_color", prefWatcher, (void *)6);
	
#ifndef MOZ_NGLAYOUT
	// See if the user's choices override the document
	PREF_GetBoolPref("browser.use_document_colors", &m_bUseDocumentColors);
    LO_SetUserOverride(!m_bUseDocumentColors);
	PREF_RegisterCallback("browser.use_document_colors", prefWatcher, (void *)7);
#endif	

	// Always Load Images
	PREF_GetBoolPref("general.always_load_images", &m_bAutoLoadImages);
    PREF_RegisterCallback("general.always_load_images", prefWatcher, (void *)8);

	   // type of toolbar buttons
 	PREF_GetIntPref("browser.chrome.button_style",&prefInt);
	theApp.m_pToolbarStyle = CASTINT(prefInt);
	PREF_RegisterCallback("browser.chrome.button_style", SetToolbarButtonStyle, NULL);


}
