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


//--------------------------------------------------------------------------------------------------------
//	Unicode Virtual Font Manager used for Unicode rendering 
//--------------------------------------------------------------------------------------------------------
#include "stdafx.h"
#include "cuvfm.h"
//------------------------------------------------------------------------------------------------
//
//	CUnicodeVirtualFontMgr
//
//------------------------------------------------------------------------------------------------
#ifdef XP_WIN
#ifndef XP_WIN32
#define GDI_ERROR NULL
#endif
#endif

CUnicodeVirtualFontMgr::CUnicodeVirtualFontMgr(HDC in_hdc)
{
	TEXTMETRIC tm;
	::GetTextMetrics(in_hdc, &tm);
	m_DC = in_hdc;
	m_OrigFont = (HFONT)::SelectObject(in_hdc, ::GetStockObject(SYSTEM_FONT));
	if(m_OrigFont != (HGDIOBJ)GDI_ERROR)
	{
		CFont *pFont = CFont::FromHandle(m_OrigFont);
		if( (pFont) &&
#ifdef XP_WIN32
			(pFont->GetLogFont(& (this->m_lf))))
#else
		(::GetObject(pFont->m_hObject ,sizeof(LOGFONT), &(this->m_lf))))
#endif
		{
			m_bFixed = ( m_lf.lfPitchAndFamily & FIXED_PITCH );
			m_iOrigAscent = tm.tmAscent;
		}
		::SelectObject(in_hdc, m_OrigFont);
	}

}
//------------------------------------------------------------------------------------------------
//
//	~CUnicodeVirtualFontMgr()
//
//------------------------------------------------------------------------------------------------
CUnicodeVirtualFontMgr::~CUnicodeVirtualFontMgr()
{
	// restore the original font when we destroy.
	if(m_OrigFont != (HGDIOBJ)GDI_ERROR)
	{
		::SelectObject(m_DC, m_OrigFont);
	}
}
//------------------------------------------------------------------------------------------------
//
//	CacularAscentDelta()
//
//------------------------------------------------------------------------------------------------
int CUnicodeVirtualFontMgr::CacularAscentDelta(HDC hDC)
{
	// restore the original font when we destroy.
	if(m_OrigFont != (HGDIOBJ)GDI_ERROR)
	{
		TEXTMETRIC tm;
		::GetTextMetrics(hDC, &tm);
		return m_iOrigAscent - tm.tmAscent;
	}
	return 0;
}
//------------------------------------------------------------------------------------------------
//
//	CUnicodeVirtualFontMgr::GetCsidFont
//
//------------------------------------------------------------------------------------------------
CFont*	CUnicodeVirtualFontMgr::GetCsidFont(int16 encoding)
{
	if(m_OrigFont != (HGDIOBJ)GDI_ERROR)
	{

		CFont *pFont;
		if(! GetFontFromCache(encoding, pFont))
		{
			VERIFY(pFont = CreateFont(encoding));
			if(pFont != NULL)
			{	
				if(AddFontToCache(encoding, pFont) == FALSE)
				{
					delete pFont;
					return NULL;
				}
			}
		}
		return pFont;
	}
	return NULL;
}

CyaFont *CUnicodeVirtualFontMgr::GetCsidCyaFont(HDC hdc, int16 encoding)
{
	if(m_OrigFont != (HGDIOBJ)GDI_ERROR)
	{

		CyaFont *pFont;
		if(! GetCyaFontFromCache(encoding, pFont))
		{
			VERIFY(pFont = CreateCyaFont(hdc, encoding));
			if(pFont != NULL)
			{	
				if(AddCyaFontToCache(encoding, pFont) == FALSE)
				{
					delete pFont;
					return NULL;
				}
			}
		}
		return pFont;
	}
	return NULL;
}	// GetCsidCyaFont

//------------------------------------------------------------------------------------------------
//
//	CUnicodeVirtualFontMgr::CreateFont
//
//------------------------------------------------------------------------------------------------
void CUnicodeVirtualFontMgr::UpdateLOGFONTForEncoding(int16 encoding)
{
	m_lf.lfCharSet = DEFAULT_CHARSET;
	m_lf.lfPitchAndFamily = DEFAULT_PITCH;
	switch(encoding)
	{
		case CS_DINGBATS:
			{
				strcpy(m_lf.lfFaceName, "Wingdings");
				if( (! theApp.m_bUseUnicodeFont) )
					m_lf.lfCharSet = SYMBOL_CHARSET;
			}
			break;
		case CS_SYMBOL:
			{
				strcpy(m_lf.lfFaceName, "Symbol");
				if( (! theApp.m_bUseUnicodeFont) )
					m_lf.lfCharSet = SYMBOL_CHARSET;
			}
			break;
		default:
			{
				EncodingInfo *pEncoding = theApp.m_pIntlFont->GetEncodingInfo(theApp.m_pIntlFont->DocCSIDtoID(encoding & ~CS_AUTO));
				XP_ASSERT(pEncoding);
				if(pEncoding)
				{
					strcpy(m_lf.lfFaceName, ((m_bFixed) ? pEncoding->szFixName : pEncoding->szPropName ));
					if(sysInfo.m_bWin4)
					{
						if( (! theApp.m_bUseUnicodeFont) )
						{
							m_lf.lfCharSet = ((m_bFixed) ? 
								pEncoding->iFixCharset : pEncoding->iPropCharset);
						}
					}
				}
			}
			break;
	}
}
CFont* CUnicodeVirtualFontMgr::CreateFont(int16 encoding)
{
	UpdateLOGFONTForEncoding(encoding);
	CFont *pSelectThis = new CFont();
	VERIFY(pSelectThis->CreateFontIndirect(&m_lf));
	return pSelectThis;
}

CyaFont* CUnicodeVirtualFontMgr::CreateCyaFont(HDC hdc, int16 encoding)
{
	int   isUnicode;
	if ( CIntlWin::UseUnicodeFontAPI( encoding )) 
	{
		isUnicode = 1;
	}
	else	
	{
		isUnicode = 0;
	}
	UpdateLOGFONTForEncoding(encoding); 
	CyaFont *pSelectThis = new CyaFont();
	// todo: pass in context as first paramer, it will be used
	// to get current URL, for webfont security.
	VERIFY(pSelectThis->CreateNetscapeFontWithLOGFONT(NULL,hdc, &m_lf, isUnicode) == FONTERR_OK );
	return pSelectThis;
}

BOOL CUnicodeVirtualFontMgr::GetFontFromCache(int16 encoding, CFont*& pFont)
{
	return CVirtualFontFontCache::Get(encoding, 
		m_lf.lfHeight, m_bFixed, (m_lf.lfWeight == FW_BOLD), 
		m_lf.lfItalic, m_lf.lfUnderline ,pFont);
}
BOOL CUnicodeVirtualFontMgr::AddFontToCache(int16 encoding, CFont* pFont)	
{ 
	return CVirtualFontFontCache::Add(encoding, 
		m_lf.lfHeight, m_bFixed, (m_lf.lfWeight == FW_BOLD), 
		m_lf.lfItalic, m_lf.lfUnderline ,pFont);	
}

BOOL CUnicodeVirtualFontMgr::GetCyaFontFromCache(int16 encoding, CyaFont*& pFont)
{
	return CVirtualFontFontCache::Get(encoding, 
		m_lf.lfHeight, m_bFixed, (m_lf.lfWeight == FW_BOLD), 
		m_lf.lfItalic, m_lf.lfUnderline ,pFont);
}
BOOL CUnicodeVirtualFontMgr::AddCyaFontToCache(int16 encoding, CyaFont* pFont)	
{ 
	return CVirtualFontFontCache::Add(encoding, 
		m_lf.lfHeight, m_bFixed, (m_lf.lfWeight == FW_BOLD), 
		m_lf.lfItalic, m_lf.lfUnderline ,pFont);	
}


