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
/* , 1997 */
//--------------------------------------------------------------------------------------------------------
//	Author: Frank Tang ftang@netscape.com x2913
//
//	Unicode Virtual Font Manager used for Unicode rendering 
//--------------------------------------------------------------------------------------------------------
#ifndef __CUVFM_H
#define __CUVFM_H
#include "stdafx.h"
#include "cvffc.h"
//------------------------------------------------------------------------------------------------
//
//	CUnicodeVirtualFontMgr
//
//	The constructor save the font on dc
//  The destructor restore the font on dc
//
//------------------------------------------------------------------------------------------------
class CUnicodeVirtualFontMgr {
public:
	CUnicodeVirtualFontMgr(HDC in_hdc);
	~CUnicodeVirtualFontMgr();
//#ifdef netscape_font_module
	CyaFont *CUnicodeVirtualFontMgr::GetCsidCyaFont(HDC hdc, int16 encoding);
//#endif  //netscape_font_module
	CFont* GetCsidFont(int16 encoding);
	static void ExitInstance();
	int		CacularAscentDelta(HDC hDC);
private:
	CFont*	CreateFont(int16 encoding);
//#ifdef netscape_font_module
	CyaFont*	CreateCyaFont(HDC hdc, int16 encoding);
	BOOL	GetCyaFontFromCache(int16 encoding, CyaFont*& pFont);
	BOOL	AddCyaFontToCache(int16 encoding, CyaFont* pFont);
//#endif  //netscape_font_module
	void	UpdateLOGFONTForEncoding(int16 encoding);
	BOOL	GetFontFromCache(int16 encoding, CFont*& pFont);
	BOOL	AddFontToCache(int16 encoding, CFont* pFont);
private:
	HDC		m_DC;
	HFONT	m_OrigFont;
	LOGFONT	m_lf;
	BOOL	m_bFixed;
	int		m_iOrigAscent;
};
#endif
