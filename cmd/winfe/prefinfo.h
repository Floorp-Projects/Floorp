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

#ifndef __CPrefInfo_H
#define __CPrefInfo_H

class CPrefInfo {
public:
    CPrefInfo();

public:
    //  Whether or not we've read and set up the prefs.
    //  Can't do this until JS and PREFLIB is ready.
    void Initialize();

	// Call this when the system colors change
	void	SysColorChange();

//  general.* Prefs
public:
	XP_Bool		m_bAutoLoadImages;

    //  browser.* Prefs
public:
	COLORREF	m_rgbBackgroundColor;
	COLORREF	m_rgbForegroundColor;
	XP_Bool		m_bUnderlineAnchors;

private:
	COLORREF	m_rgbAnchorColor;
	COLORREF	m_rgbVisitedColor;
	XP_Bool		m_bUseWindowsColors;
	XP_Bool		m_bUseDocumentColors;

	void		UpdateAllWindows();

	friend int PR_CALLBACK	prefWatcher(LPCSTR lpszPrefName, LPVOID lpvData);
	friend int PR_CALLBACK SetToolbarButtonStyle(const char *newPref, void *data);

};

extern CPrefInfo prefInfo;

#endif // __CPrefInfo_H
