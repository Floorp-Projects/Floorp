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

#include "pch.h"
#include <assert.h>
#include "dllcom.h"
#include "pages.h"
#include "xp_core.h"
#include "prefapi.h"

/////////////////////////////////////////////////////////////////////////////
// Helper functions

int
PREF_GetStringPref(LPCSTR lpszPref, CString &str)
{
	int		nBufLength = 0;

	if (PREF_GetCharPref(lpszPref, NULL, &nBufLength) == PREF_ERROR)
		return PREF_ERROR;

	if (nBufLength <= 1) {
		str.Empty();
		return PREF_NOERROR;

	} else {
		return PREF_GetCharPref(lpszPref, str.BufferSetLength(nBufLength - 1), &nBufLength);
	}
}

/////////////////////////////////////////////////////////////////////////////
// CMailNewsPropertyPage implementation

CMailNewsPropertyPage::CMailNewsPropertyPage(UINT nTemplateID, LPCSTR lpszHelpTopic)
	: CPropertyPageEx(CComDll::m_hInstance, nTemplateID, lpszHelpTopic)
{
}

SIZE	CMailNewsPropertyPage::m_size = {0, 0};

HRESULT CMailNewsPropertyPage::GetPageSize(SIZE &size)
{
	// All of our pages are the same size. See if we already
	// have the size
	if (m_size.cx == 0) {
		// Ask our base class to determine the size
		HRESULT hres = CPropertyPageEx::GetPageSize(m_size);

		if (FAILED(hres))
			return hres;
	}

	// Use the cached size
	assert(m_size.cx > 0 && m_size.cy > 0);
	size = m_size;
	return NOERROR;
}

BOOL CMailNewsPropertyPage::CheckIfLockedPref(LPCSTR lpszPref, UINT nCtlID)
{
	if (MNPREF_PrefIsLocked(lpszPref)) {
		EnableDlgItem(nCtlID, FALSE);
		return TRUE;
	}

	return FALSE;
}
