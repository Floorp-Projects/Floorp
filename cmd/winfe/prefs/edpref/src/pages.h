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

#ifndef __EDPAGES_H_
#define __EDPAGES_H_

#include "cppageex.h"

/////////////////////////////////////////////////////////////////////////////
// Helper functions

// CString interface to PREF_GetCharPref routine
int
PREF_GetStringPref(LPCSTR lpszPref, CString &str);

/////////////////////////////////////////////////////////////////////////////
// CEditorPropertyPage

// Simple base class that handles reference counting of the DLL object and
// caching the size of the property page. Note: all property pages derived
// from this class MUST be the same size
class CEditorPropertyPage : public CPropertyPageEx {
	public:
		CEditorPropertyPage(UINT nTemplateID, LPCSTR lpszHelpTopic);

	protected:
		HRESULT		GetPageSize(SIZE &);

	private:
		CRefDll	m_refDll;

		static SIZE	m_size;  // cached size
};

/////////////////////////////////////////////////////////////////////////////
// CEditorPrefs

class CEditorPrefs : public CEditorPropertyPage {
	public:
		CEditorPrefs();

	protected:
		STDMETHODIMP Activate(HWND hwndParent, LPCRECT lprc, BOOL bModal);
		BOOL		 DoTransfer(BOOL bSaveAndValidate);
		BOOL		 ApplyChanges();
		// Event processing
        BOOL	OnCommand(int id, HWND hwndCtl, UINT notifyCode);
        BOOL    GetApplicationFilename(CString& strFilename, UINT nIDCaption);
		BOOL    InitDialog();

	private:
        // Member variables
        CString m_strAuthor;
        CString m_strHTML_Editor;
        CString m_strImageEditor;
        int     m_iFontSizeMode;
        BOOL    m_bAutoSave;
        int     m_iAutoSaveMinutes;
};


/////////////////////////////////////////////////////////////////////////////
// CPublishPrefs

class CPublishPrefs : public CEditorPropertyPage {
	public:
		CPublishPrefs();

	protected:
		STDMETHODIMP Activate(HWND hwndParent, LPCRECT lprc, BOOL bModal);
		BOOL		 DoTransfer(BOOL bSaveAndValidate);
		BOOL		 ApplyChanges();
		BOOL	     InitDialog();

	private:
        // Member variables
        BOOL        m_bAutoAdjustLinks;
        BOOL        m_bKeepImagesWithDoc;
        CString     m_strPublishLocation;
        CString     m_strBrowseLocation;
};



/////////////////////////////////////////////////////////////////////////////
// CEditorPrefs2

class CEditorPrefs2 : public CEditorPropertyPage {
	public:
		CEditorPrefs2();

	protected:
		STDMETHODIMP Activate(HWND hwndParent, LPCRECT lprc, BOOL bModal);
		BOOL		 DoTransfer(BOOL bSaveAndValidate);
		BOOL		 ApplyChanges();
		BOOL	     InitDialog();

	private:
        // Member variables
        BOOL        m_bUpDownMoveCursor;
};

#endif /* __EDPAGES_H_ */

