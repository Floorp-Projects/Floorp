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

#ifndef MetaFileContext__H
#define MetaFileContext__H

//	Deriving from CDCCX
#include "cxdc.h"

class CMetaFileCX : public CDCCX	{
//	construction, destructions, indirect construction
public:
	CMetaFileCX(CDC* pMetaFileDC, SIZE& pSize, URL_Struct *pUrl);
	~CMetaFileCX();
	static DWORD MetaFileAnchorObject(CDC *pMetaFileDC, SIZE& pSize, URL_Struct *pUrl);

//	CDC Access.
private:
	CDC *m_pMetaFileDC;
	lo_SavedEmbedListData* m_embedList;		/* to save the savedData from the URL struct.*/

public:
	virtual HDC GetContextDC();
	virtual BOOL IsDeviceDC() {return TRUE;}
	virtual HDC GetAttribDC() {return m_pMetaFileDC->m_hAttribDC;}
	virtual void ReleaseContextDC(HDC pDC);

//	The size of the viewport which we'll be formatting layout to.
protected:
	SIZE m_csViewport;

//	Post Initialization
public:
	virtual void Initialize(BOOL bOwnDC, RECT *pRect = NULL, BOOL bInitialPalette = TRUE, BOOL bNewMemDC = TRUE);

//	Context sensitive attribute mapping.
public:
	virtual PRBool ResolveIncrementalImages();

//	Overrides
public:
	//	All connections complete.
	virtual void AllConnectionsComplete(MWContext *pContext);
	//	Layout initialization respecting page size.
	virtual void LayoutNewDocument(MWContext *pContext, URL_Struct *pURL, int32 *pWidth, int32 *pHeight, int32 *pmWidth, int32 *pmHeight);

};

#endif // MetaFileContext__H
