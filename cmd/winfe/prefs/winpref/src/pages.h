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

#ifndef __PAGES_H_
#define __PAGES_H_

#include "cppageex.h"
#include "xp_list.h"

#include "nsIDefaultBrowser.h"

//also in brpref\src\advpages.cpp, winfe\mnprefs.cpp
#define WINPREF_PrefIsLocked(pPrefName) PREF_PrefIsLocked(pPrefName)

/////////////////////////////////////////////////////////////////////////////
// Helper functions

// CString interface to PREF_GetCharPref routine
int
PREF_GetStringPref(LPCSTR lpszPref, CString &str);

/////////////////////////////////////////////////////////////////////////////
// CBasicWindowsPrefs

class CBasicWindowsPrefs : public CPropertyPageEx {
	public:
		CBasicWindowsPrefs(nsIDefaultBrowser *pDefaultBrowser);
        ~CBasicWindowsPrefs();

	protected:
		BOOL		 InitDialog();
 		STDMETHODIMP Activate(HWND hwndParent, LPCRECT lprc, BOOL bModal);
		BOOL		 DoTransfer(BOOL bSaveAndValidate);
		BOOL		 ApplyChanges();

		// Event Processing
		LRESULT	WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
		BOOL	OnCommand(int id, HWND hwndCtl, UINT notifyCode);

	private:
        nsIDefaultBrowser* m_pDefaultBrowser;
        BOOL m_bCheckOnStart;
        nsIDefaultBrowser::Prefs m_originalPrefs;
        nsIDefaultBrowser::Prefs m_currentPrefs;
};

#endif /* __PAGES_H_ */

