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

#include "cxmeta.h"

//	The purpose of this file is to output a load to a DC.
//	The name MetaFile is just a term, the DC need not be a MetaFileDC.
//	If however, the DC passed in is a MetaFile DC, then all setup of DC
//		must occur outside of this code.
//	Further, the DC must remain valid throughout the course of the load.
//	It may be more easier for you to derive from this class and handle creation
//		and destruction of your DC therein.

//	Indirect construction of the Class which loads a URL and
//		returns the context ID, 0 on failure.
//	Calling code can track when the MetaFile is finished, by realizing
//		when the context ID is no longer found in the context list.
DWORD CMetaFileCX::MetaFileAnchorObject(CDC * pMetaFileDC, SIZE& pSize, URL_Struct *pUrl)
{
	//	Create a new object.
	CMetaFileCX *pDontCare = new CMetaFileCX(pMetaFileDC, pSize, pUrl);
	if(pDontCare != NULL)	{
		//	The return value is!
		DWORD dwID = pDontCare->GetContextID();

		//	Load the URL.
		pDontCare->GetUrl(pUrl, FO_CACHE_AND_PRESENT);

		//	Success as far as we are concerned.
		return(dwID);
	}
	
	//	Failure.
	return(0);
}

//	Construct the class.
CMetaFileCX::CMetaFileCX(CDC* pMetaFileDC, SIZE& pSize, URL_Struct *pUrl)
{
	TRACE("Creating CMetaFileCX %p\n", this);

	//	Set the context type.
	m_cxType = MetaFile;
	GetContext()->type = MWContextMetaFile;
	// this is a hack to save the layout data for embedlist.  For embed layout will
	// make a copy of the pSavedData.  We need to free the memory here.
	m_embedList = (lo_SavedEmbedListData*)pUrl->savedData.EmbedList;

	//	Initialize the class members.
	m_pMetaFileDC = NULL;
	m_csViewport.cx = m_csViewport.cy = 0;

	//	Set them.
	m_pMetaFileDC = pMetaFileDC;
	ASSERT(m_pMetaFileDC);

	m_csViewport = pSize;
	ASSERT(m_csViewport.cx != 0 && m_csViewport.cy != 0);

	//	Should be able to initialize ourselves safely now.
	Initialize(TRUE);
}

//	We are now destroyed.
//	Callers can tell when we've gone away by checking for our context ID
//		in the list of all contexts -- Returned by MetaFileAnchorObject.
CMetaFileCX::~CMetaFileCX()
{
	TRACE("Destroying CMetaFileCX %p\n", this);
	// MWH - this is a hack to free the embed list that layout make copy from the original
	// SavedData.  I removed the freeing from lib\layout\layfree.c lo_FreeDocumentEmbedListData.
	// and free the data here.  This will fix an OLE printing problem.  The problem is when a .doc file
	// is on the net, i.e. http://....//xxx.doc.  When layout free the EmbedList in 
	// lo_FreeDocumentEmbedListData will cause the page not printed.  Since for printing we need
	// to use the cached data.

	if (m_embedList && (m_embedList->embed_data_list != NULL)) {
		int32 i;
		lo_EmbedDataElement* embed_data_list;

		PA_LOCK(embed_data_list, lo_EmbedDataElement*, m_embedList->embed_data_list);
		for (i=0; i < m_embedList->embed_count; i++)
		{
			if (embed_data_list[i].freeProc && embed_data_list[i].data)
				(*(embed_data_list[i].freeProc))(GetContext(), embed_data_list[i].data);
		}
		PA_UNLOCK(m_embedList->embed_data_list);
		PA_FREE(m_embedList->embed_data_list);

		m_embedList->embed_count = 0;
		m_embedList->embed_data_list = NULL;

	}
}

//	Returns the DC for this context.
HDC CMetaFileCX::GetContextDC()
{
	return(m_pMetaFileDC->GetSafeHdc());
}

//	Releases the DC for this context.
//	Since the DC is static, there is no need to really do anything fancy here.
void CMetaFileCX::ReleaseContextDC(HDC pDC)
{
}

//	Initializes the context for work after all the members are set.
//	We leave the DC as persistant, as it doesn't really change while we're doing
//		this (it better!).
void CMetaFileCX::Initialize(BOOL bOwnDC, RECT *pRect, BOOL bInitialPalette, BOOL bNewMemDC)
{
	//	We're going to use MM_TEXT for simplicity sake.
	//	Another reason, is simply, that on 16 bit windows, all the GDI functions
	//		take 16 bit ints, which wrap if the page gets too long.
	//	The page will be increadibly long, since this is a metafile, and this
	//		will prolong the wrapping.
	m_MM = MM_TEXT;

	//	Set our width and height to that specified in the constructor.
	m_lWidth = Twips2PixX(Metric2TwipsX(m_csViewport.cx));
	m_lHeight = Twips2PixY(Metric2TwipsY(m_csViewport.cy));
	TRACE("Metafile width is %ld\n", m_lWidth);
	TRACE("Metafile height is %ld\n", m_lHeight);

	//	Call the base.
	CDCCX::Initialize(bOwnDC, pRect, bInitialPalette, bNewMemDC);
}

//	A new document is starting to be laid out.
//	Inform layout of our dimensions, etc.
void CMetaFileCX::LayoutNewDocument(MWContext *pContext, URL_Struct *pURL, int32 *pWidth, int32 *pHeight, int32 *pmWidth, int32 *pmHeight)
{
	//	Turn off all display blocking, since we allow the metafile to
	//		be the full representation of a document.
	DisableDisplayBlocking();

	//	Call the base
	CDCCX::LayoutNewDocument(pContext, pURL, pWidth, pHeight, pmWidth, pmHeight);
}

//	The load is finished for all intents and purposes.
//	Get rid of ourselves.
void CMetaFileCX::AllConnectionsComplete(MWContext *pContext)
{
	//	Call the base.
	CDCCX::AllConnectionsComplete(pContext);

	//	We're done.
	DestroyContext();
}

//	Do not allow incremental image display.
PRBool CMetaFileCX::ResolveIncrementalImages()
{
	return(PR_FALSE);
}
