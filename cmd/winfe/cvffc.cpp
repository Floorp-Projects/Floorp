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
//    Author: Frank Tang ftang@netscape.com x2913
//
//	Virtual Font Cacahe used for Unicode rendering 
//--------------------------------------------------------------------------------------------------------
#include "stdafx.h"
#include "cvffc.h"
//------------------------------------------------------------------------------------------------
//
//	CVirtualFontFontCache
//
//------------------------------------------------------------------------------------------------
CMapWordToOb* CVirtualFontFontCache::m_cache = NULL;
//#ifdef netscape_font_module
CMapWordToOb* CVirtualFontFontCache::m_Cyacache = NULL;
//#endif	//netscape_font_module
void	CVirtualFontFontCache::Init()
{
	CVirtualFontFontCache::m_cache = NULL;
	// CVirtualFontFontCache::m_Cyacache = NULL;
}
void	CVirtualFontFontCache::Exit()
{
	CVirtualFontFontCache::Reset();
}
//------------------------------------------------------------------------------------------------
//
//	CVirtualFontFontCache::Get
//
//------------------------------------------------------------------------------------------------
#define STYLEKEY(f,b,i,u)	( ((f) ? 8 : 0) | ((b) ? 4 : 0) | ((i) ? 2 : 0) | ((u) ? 1 : 0) )
BOOL CVirtualFontFontCache::Get(int16 encoding, int size, BOOL fixed, BOOL bold, BOOL italic, BOOL underline, CFont*& pFont )
{
	if(CVirtualFontFontCache::m_cache)
	{
		CMapWordToOb*	pSizecache;
		if(CVirtualFontFontCache::m_cache->Lookup(encoding, (CObject*&)pSizecache))
		{
			CMapWordToOb*	pStylecache;
			if(pSizecache->Lookup(size, (CObject*&)pStylecache))
			{
				BOOL ret = pStylecache->Lookup( STYLEKEY(fixed,bold,italic,underline), (CObject*&)pFont);
				return ret;
			}
		}
	}
	return FALSE;
}
 
//#ifdef netscape_font_module
BOOL CVirtualFontFontCache::Get(int16 encoding, int size, BOOL fixed, BOOL bold, 
								BOOL italic, BOOL underline, CyaFont*& pFont )
{
	if(CVirtualFontFontCache::m_Cyacache)
	{
		CMapWordToOb*	pSizecache;
		if(CVirtualFontFontCache::m_Cyacache->Lookup(encoding, (CObject*&)pSizecache))
		{
			CMapWordToOb*	pStylecache;
			if(pSizecache->Lookup(size, (CObject*&)pStylecache))
			{
				BOOL ret = pStylecache->Lookup( STYLEKEY(fixed,bold,italic,underline), (CObject*&)pFont);
				return ret;
			}
		}
	}
	return FALSE;
}
//#endif  //netscape_font_module

//------------------------------------------------------------------------------------------------
//
//	CVirtualFontFontCache::Add
//
//------------------------------------------------------------------------------------------------
BOOL CVirtualFontFontCache::Add(int16 encoding, int size, BOOL fixed, BOOL bold, BOOL italic, BOOL underline, CFont*& pFont )
{
	ASSERT(pFont);
	if(! CVirtualFontFontCache::m_cache)
	{
		// *** Fix me, we better tune the init of pSizecache
		VERIFY(CVirtualFontFontCache::m_cache = new CMapWordToOb);
		if(CVirtualFontFontCache::m_cache == NULL )
			return FALSE;
	}
	CMapWordToOb*	pSizecache;
	if(! CVirtualFontFontCache::m_cache->Lookup(encoding, (CObject*&)pSizecache))
	{	
		// *** Fix me, we better tune the init of pSizecache
		VERIFY(pSizecache = new CMapWordToOb);
		if(pSizecache == NULL)
			return FALSE;	
		CVirtualFontFontCache::m_cache->SetAt(encoding, (CObject*&)pSizecache);
	}
	CMapWordToOb*	pStylecache;
	if(! pSizecache->Lookup(size, (CObject*&)pStylecache))
	{
		// *** Fix me, we better tune the init of pSizecache
		VERIFY(pStylecache = new CMapWordToOb);
		if(pStylecache == NULL )
			return FALSE;
		pSizecache->SetAt(size, (CObject*&)pStylecache);
	}
	pStylecache->SetAt(STYLEKEY(fixed,bold,italic,underline), (CObject*&)pFont);
	return TRUE;
}

//#ifdef netscape_font_module
BOOL CVirtualFontFontCache::Add(int16 encoding, int size, BOOL fixed, BOOL bold, 
								BOOL italic, BOOL underline, CyaFont*& pFont )
{
	ASSERT(pFont);
	if(! CVirtualFontFontCache::m_Cyacache)
	{
		// *** Fix me, we better tune the init of pSizecache
		VERIFY(CVirtualFontFontCache::m_Cyacache = new CMapWordToOb);
		if(CVirtualFontFontCache::m_Cyacache == NULL )
			return FALSE;
	}
	CMapWordToOb*	pSizecache;
	if(! CVirtualFontFontCache::m_Cyacache->Lookup(encoding, (CObject*&)pSizecache))
	{	
		// *** Fix me, we better tune the init of pSizecache
		VERIFY(pSizecache = new CMapWordToOb);
		if(pSizecache == NULL)
			return FALSE;	
		CVirtualFontFontCache::m_Cyacache->SetAt(encoding, (CObject*&)pSizecache);
	}
	CMapWordToOb*	pStylecache;
	if(! pSizecache->Lookup(size, (CObject*&)pStylecache))
	{
		// *** Fix me, we better tune the init of pSizecache
		VERIFY(pStylecache = new CMapWordToOb);
		if(pStylecache == NULL )
			return FALSE;
		pSizecache->SetAt(size, (CObject*&)pStylecache);
	}
	pStylecache->SetAt(STYLEKEY(fixed,bold,italic,underline), (CObject*&)pFont);
	return TRUE;
}
//#endif  //netscape_font_module



//------------------------------------------------------------------------------------------------
//
//	CVirtualFontFontCache::Reset
//
//------------------------------------------------------------------------------------------------

void CVirtualFontFontCache::Reset()
{
	CMapWordToOb*	pEncodingcache = CVirtualFontFontCache::m_cache;
	CVirtualFontFontCache::m_cache = NULL;
	// we should really lock this function
	if(pEncodingcache)
	{
		int16	encoding;
		CMapWordToOb*	pSizecache;
		POSITION i;
		for(i = pEncodingcache->GetStartPosition(); i != NULL;)
		{
			pEncodingcache->GetNextAssoc(i, (WORD&)encoding, (CObject*&)pSizecache);
			// pEncodingcache->RemoveKey(encoding);
			ASSERT(pSizecache);
			if(pSizecache)
			{
				POSITION j;
				for(j = pSizecache->GetStartPosition(); j != NULL;)
				{		
					int size;
					CMapWordToOb*	pStylecache;
					pSizecache->GetNextAssoc(j, (WORD&)size, (CObject*&)pStylecache);
					// pSizecache->RemoveKey(size);
					ASSERT(pStylecache);
					if(pStylecache)
					{
						POSITION k;
						for(k = pStylecache->GetStartPosition(); k != NULL;)
						{		
							WORD style;
							CFont* pFont = NULL;
							pStylecache->GetNextAssoc(k, style, (CObject*&)pFont);
							// pStylecache->RemoveKey(style);
							ASSERT(pFont);
							if(pFont)
							{
								delete pFont;
							}
						}	// for each style
						pStylecache->RemoveAll();
						ASSERT( pStylecache->GetCount() == 0);
						ASSERT( pStylecache->IsEmpty());
						delete pStylecache;
					}
				}	// for each size
				pSizecache->RemoveAll();
				ASSERT( pSizecache->GetCount() == 0);
				ASSERT( pSizecache->IsEmpty());
				delete pSizecache;
			}
		}	// for each encoding
		pEncodingcache->RemoveAll();
		ASSERT( pEncodingcache->GetCount() == 0);
		ASSERT( pEncodingcache->IsEmpty());
		delete pEncodingcache;
	}

//#ifdef netscape_font_module
	CVirtualFontFontCache::ResetCyacache();
//#endif  //netscape_font_module
}

//#ifdef netscape_font_module
void CVirtualFontFontCache::ResetCyacache()
{
	CMapWordToOb*	pEncodingcache = CVirtualFontFontCache::m_Cyacache;
	CVirtualFontFontCache::m_Cyacache = NULL;
	// we should really lock this function
	if(pEncodingcache)
	{
		int16	encoding;
		CMapWordToOb*	pSizecache;
		POSITION i;
		for(i = pEncodingcache->GetStartPosition(); i != NULL;)
		{
			pEncodingcache->GetNextAssoc(i, (WORD&)encoding, (CObject*&)pSizecache);
			// pEncodingcache->RemoveKey(encoding);
			ASSERT(pSizecache);
			if(pSizecache)
			{
				POSITION j;
				for(j = pSizecache->GetStartPosition(); j != NULL;)
				{		
					int size;
					CMapWordToOb*	pStylecache;
					pSizecache->GetNextAssoc(j, (WORD&)size, (CObject*&)pStylecache);
					// pSizecache->RemoveKey(size);
					ASSERT(pStylecache);
					if(pStylecache)
					{
						POSITION k;
						for(k = pStylecache->GetStartPosition(); k != NULL;)
						{		
							WORD style;
							CyaFont* pFont = NULL;
							pStylecache->GetNextAssoc(k, style, (CObject*&)pFont);
							// pStylecache->RemoveKey(style);
							ASSERT(pFont);
							if(pFont)
							{
								delete pFont;
							}
						}	// for each style
						pStylecache->RemoveAll();
						ASSERT( pStylecache->GetCount() == 0);
						ASSERT( pStylecache->IsEmpty());
						delete pStylecache;
					}
				}	// for each size
				pSizecache->RemoveAll();
				ASSERT( pSizecache->GetCount() == 0);
				ASSERT( pSizecache->IsEmpty());
				delete pSizecache;
			}
		}	// for each encoding
		pEncodingcache->RemoveAll();
		ASSERT( pEncodingcache->GetCount() == 0);
		ASSERT( pEncodingcache->IsEmpty());
		delete pEncodingcache;
	}
}
//#endif  //netscape_font_module
