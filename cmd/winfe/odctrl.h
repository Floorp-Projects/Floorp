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

#ifndef __ODCTRL_H
#define __ODCTRL_H

//
//	This file contains the declaration of 
//		CODNetscapeButton
//		CODMochaListBox
//		CODMochaComboBox
//
//	Why we need the ownerdrawn version ?
//	1. So we could display UTF8 widget
//	2. So we coudl use Unicode Font on US Win95 to display non-Latin 
//		characters.


//	Requried include file
#include "button.h"
#include "medit.h"

class CODNetscapeButton : public CNetscapeButton {
public:

        CODNetscapeButton(MWContext*, LO_FormElementStruct *, CWnd* pParent=NULL);
	virtual void 	DrawItem	(LPDRAWITEMSTRUCT);
	virtual void 	Draw3DBox	(CDC*, CRect&, BOOL);
	virtual void 	DrawContent	(CDC*, LPDRAWITEMSTRUCT);
	virtual void 	DrawCenterText	(CDC*, LPCSTR, CRect& );

};

class CODMochaListBox : public CMochaListBox {
public:
	virtual void	MeasureItem	(LPMEASUREITEMSTRUCT);
	virtual void 	DrawItem	(LPDRAWITEMSTRUCT);
};

class CODMochaComboBox : public CMochaComboBox {
public:
	virtual void	MeasureItem	(LPMEASUREITEMSTRUCT);
	virtual void 	DrawItem	(LPDRAWITEMSTRUCT);
};

class CODNetscapeEdit : public CNetscapeEdit {
public:
	CODNetscapeEdit();
	~CODNetscapeEdit();
	virtual void	SetWindowText(LPCTSTR lpszString);
	virtual	int		GetWindowText(LPTSTR lpszStringBuf, int nMaxCount);
	virtual	void	GetWindowText(CString& rString);
	virtual	int		GetWindowTextLength();

#ifdef XP_WIN32
	// advanced creation (allows access to extended styles)
	virtual BOOL CreateEx(DWORD dwExStyle, LPCTSTR lpszClassName,
		LPCTSTR lpszWindowName, DWORD dwStyle,
		int x, int y, int nWidth, int nHeight,
		HWND hWndParent, HMENU nIDorHMenu, LPVOID lpParam = NULL);
#endif

private:
	int16	m_set_text_wincsid;
	unsigned char*  m_convertedBuf;
	int				m_convertedBufSize;
	unsigned char*  m_ucs2Buf;
	int				m_ucs2BufSize;
	unsigned char*  m_gettextBuf;
	int				m_gettextBufSize;
protected:
	void		Convert(int16 from, int16 to, unsigned char* lpszString);
	int16		GetWinCSID();
	int16		GetSetWindowTextCSID();
	void		SetSetWindowTextCSID(int16 csid) { m_set_text_wincsid = csid; };

#ifdef XP_WIN32
	// We need to overload SetWindowTextW and GetWindowTextW() since the 
	void		SetWindowTextW(HWND hWnd, LPCWSTR lpString);
	int			GetWindowTextW(HWND hWnd, LPWSTR lpString, int nMaxCount);
#endif

};


#endif // __ODCTRL_H
