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

// implementation of the CNetscapeSrvrItem class
//

#include "stdafx.h"

#include "srvritem.h"
#include "cntritem.h"

#include "cxmeta.h"
#include "template.h"
#include "mainfrm.h"
#include "ipframe.h"
#include "netsvw.h"

#ifdef _DEBUG
#undef THIS_FILE
static char BASED_CODE THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CNetscapeSrvrItem implementation

IMPLEMENT_DYNAMIC(CNetscapeSrvrItem, COleServerItem)

//	Ole menu lock.
int CNetscapeSrvrItem::m_iOLEMenuLock = 0;

void CNetscapeSrvrItem::LoadOLEMenus()
{
	if (m_ShowUI) {
		//	Up the count.
		m_iOLEMenuLock++;

		//	If it's at 1, then we really need to load them.
		if(m_iOLEMenuLock == 1)	{
			TRACE("Loading OLE Menus\n");

			HINSTANCE hInst = AfxGetResourceHandle();
			//	Embedded
			if(theApp.m_ViewTmplate->m_hMenuEmbedding == NULL)	{
				theApp.m_ViewTmplate->m_hMenuEmbedding = ::LoadMenu(hInst, MAKEINTRESOURCE(IDR_SRVR_EMBEDDED));
			}
			ASSERT(theApp.m_ViewTmplate->m_hMenuEmbedding);

			if(theApp.m_ViewTmplate->m_hAccelEmbedding == NULL)	{
				theApp.m_ViewTmplate->m_hAccelEmbedding = ::LoadAccelerators(hInst, MAKEINTRESOURCE(IDR_SRVR_EMBEDDED));
			}
			ASSERT(theApp.m_ViewTmplate->m_hAccelEmbedding);
			//	Inplace
			if(theApp.m_ViewTmplate->m_hMenuInPlaceServer == NULL)	{
				theApp.m_ViewTmplate->m_hMenuInPlaceServer = ::LoadMenu(hInst, MAKEINTRESOURCE(IDR_SRVR_INPLACE));
			}
			ASSERT(theApp.m_ViewTmplate->m_hMenuInPlaceServer);

			if(theApp.m_ViewTmplate->m_hAccelInPlaceServer == NULL)	{
				theApp.m_ViewTmplate->m_hAccelInPlaceServer = ::LoadAccelerators(hInst, MAKEINTRESOURCE(IDR_SRVR_INPLACE));
			}
			ASSERT(theApp.m_ViewTmplate->m_hAccelInPlaceServer);
		}
	}
}

void CNetscapeSrvrItem::UnloadOLEMenus()
{
	if (m_ShowUI) {
		//	Down the menu lock.
		m_iOLEMenuLock--;

		//	If it's at 0, then we really need to get rid of them.
		if(m_iOLEMenuLock == 0)	{
			TRACE("Unloading OLE Menus\n");

			//	Embedded
			if(theApp.m_ViewTmplate->m_hMenuEmbedding)	{
				::DestroyMenu(theApp.m_ViewTmplate->m_hMenuEmbedding);
				theApp.m_ViewTmplate->m_hMenuEmbedding = NULL;
			}
			if(theApp.m_ViewTmplate->m_hAccelEmbedding)	{
				::FreeResource(theApp.m_ViewTmplate->m_hAccelEmbedding);
				theApp.m_ViewTmplate->m_hAccelEmbedding = NULL;
			}

			//	Inplace
			if(theApp.m_ViewTmplate->m_hMenuInPlaceServer)	{
				::DestroyMenu(theApp.m_ViewTmplate->m_hMenuInPlaceServer);
				theApp.m_ViewTmplate->m_hMenuInPlaceServer = NULL;
			}
			if(theApp.m_ViewTmplate->m_hAccelInPlaceServer)	{
				::FreeResource(theApp.m_ViewTmplate->m_hAccelInPlaceServer);
				theApp.m_ViewTmplate->m_hAccelInPlaceServer = NULL;
			}
		}
	}
}

CNetscapeSrvrItem::CNetscapeSrvrItem(CGenericDoc* pContainerDoc)
	: COleServerItem(pContainerDoc, TRUE)
{
	TRACE("Creating CNetscapeSrvrItem %p\n", this);

	//	No size information of yet.
	//	However, we set the below as a default.
	m_sizeExtent = CSize(0, 0);

	//	Let the context know it's an OLE server.
	if(pContainerDoc->GetContext())	{
		pContainerDoc->GetContext()->EnableOleServer();
	}
	m_ShowUI = TRUE;


	//	Load up the OLE server menus.
	//	This is a good place for the delayed loading and unloading the menu's only
	//		when needed, as a server item won't exist unless needed via OLE.
	LoadOLEMenus();
}

CNetscapeSrvrItem::~CNetscapeSrvrItem()
{
	TRACE("Destroying CNetscapeSrvrItem %p\n", this);

	//	Since the server item is now gone, it also cleaned up the view and
	//		frame, however, it did not clean up our extra component, the context.
	CGenericDoc *pDoc = GetDocument();
	if(pDoc != NULL)	{
		CDCCX *pCX = pDoc->GetContext();
		if(pCX != NULL)	{
			TRACE("Destroying document context from server item\n");
			pCX->DestroyContext();
		}
#ifdef DEBUG
		else	{
			TRACE("No document context to destroy from server item\n");
		}
#endif
	}

	//	Unload OLE menu's.
	//	This is a good place for the delayed loading and unloading the menu's only
	//		when needed, as a server item won't exist unless needed via OLE.
	UnloadOLEMenus();
}

void CNetscapeSrvrItem::Serialize(CArchive& ar)
{
	TRACE("Serializing CNetscapeSrvrItem\n");

	// CNetscapeSrvrItem::Serialize will be called by the framework if
	//  the item is copied to the clipboard.  This can happen automatically
	//  through the OLE callback OnGetClipboardData.  A good default for
	//  the embedded item is simply to delegate to the document's Serialize
	//  function.  If you support links, then you will want to serialize
	//  just a portion of the document.

	if (!IsLinkedItem())
	{
		CGenericDoc* pDoc = GetDocument();
		ASSERT_VALID(pDoc);
		pDoc->Serialize(ar);
	}
}

BOOL CNetscapeSrvrItem::OnGetExtent(DVASPECT dwDrawAspect, CSize& rSize)
{
	TRACE("CNetscapeSrvrItem::OnGetExtent(dwDrawAspect, size %p\n", this);

    BOOL bRetval = TRUE;

	// Most applications, like this one, only handle drawing the content
	//  aspect of the item.  If you wish to support other aspects, such
	//  as DVASPECT_THUMBNAIL (by overriding OnDrawEx), then this
	//  implementation of OnGetExtent should be modified to handle the
	//  additional aspect(s).

	if (dwDrawAspect != DVASPECT_CONTENT)
		return COleServerItem::OnGetExtent(dwDrawAspect, rSize);

	// CNetscapeSrvrItem::OnGetExtent is called to get the extent in
	//  HIMETRIC units of the entire item.
	
	//	If we've never sized before, set the m_sizeExtent, and return it.
	if(m_sizeExtent.cx == 0 || m_sizeExtent.cy == 0)	{
		CGenericDoc* pDoc = GetDocument();
		if(pDoc != NULL)	{
			//	Return the size of the normal netscape view.
			//	This will cause the window to come up in a correct size depending on the
			//		area of the item.
			CWinCX *pCX = VOID2CX(pDoc->GetContext(), CWinCX);
			if(pCX != NULL)	{
				HWND hView = pCX->GetPane();
				if(hView != NULL)	{
					CRect crDim;
                    ::GetClientRect(hView, crDim);

					int32 lWidth = crDim.Width();
					int32 lHeight = crDim.Height();

					if (lWidth == 0) 
						lWidth = 7000;
					else
						lWidth = pCX->Twips2MetricX(lWidth);
					if (lHeight == 0) 
						lHeight = 7000;
					else
						lHeight = pCX->Twips2MetricY(lHeight);

					m_sizeExtent = CSize(CASTINT(lWidth), CASTINT(lHeight));
				}
			}
		}
	}
	//	Return the extents set previously, or set by OnSetExtent.
	rSize = m_sizeExtent;

    TRACE("GetExtents are %ld,%ld\n", m_sizeExtent.cx, m_sizeExtent.cy);
    ASSERT(bRetval);
	return(bRetval);
}

BOOL CNetscapeSrvrItem::OnSetExtent(DVASPECT nDrawAspect, const CSize& size)	{
	TRACE("CNetscapeSrvrItem::OnSetExtent(nDrawAspect, size) %p\n", this);

    CGenericDoc *pDoc = GetDocument();
    ASSERT(pDoc);

	//	Don't handle any others but direct content for now.
	BOOL bRetval = COleServerItem::OnSetExtent(nDrawAspect, size);
	m_sizeExtent = size;
	if(nDrawAspect == DVASPECT_CONTENT)	{
        //  Set our default extents to be reported by the document.
        pDoc->m_csDocumentExtent = m_sizeExtent;
        bRetval = TRUE;
	}

    TRACE("SetExtents are %ld,%ld\n", m_sizeExtent.cx, m_sizeExtent.cy);
    ASSERT(bRetval);
	return(bRetval);
}


//  Return TRUE in most cases unless serious error.
//  Nothing to draw is OK, but unable to draw is not OK.
BOOL CNetscapeSrvrItem::OnDraw(CDC* pDC, CSize& rSize)
{
	TRACE("CNetscapeSrvrItem::OnDraw %p\n", this);
	ASSERT_VALID(pDC);

	CGenericDoc* pDoc = GetDocument();
	ASSERT_VALID(pDoc);

	//	Determine the URL we're going to draw.
	CString csDraw;
	pDoc->GetContextHistoryAddress(csDraw);
	if(csDraw.IsEmpty())	{
		TRACE("Nothing to draw.\n");
		return(TRUE);
	}

	//	Allocate the url.
	URL_Struct *pUrl = NET_CreateURLStruct((const char *)csDraw, NET_DONT_RELOAD);
	if(pUrl == NULL)	{
		TRACE("Nothing to draw without URL.\n");
		return(TRUE);
	}

	//	Before we do this, make sure we know the current size of the view
    //      not the document constant extent.
	CSize csViewport = pDoc->m_csViewExtent;

	//	Have a new context draw the item into the metafile.
	//	We have to block until completion.
	DWORD dwBlockID = CMetaFileCX::MetaFileAnchorObject(pDC, csViewport, pUrl);
	if(dwBlockID == 0)	{
		//	Counld not create new meta file context.
		return(FALSE);
	}

	//	Block return until the context is destroyed.
	FEU_BlockUntilDestroyed(dwBlockID);

	return(TRUE);
}

#define NO_UI_EMBEDDING 5

void CNetscapeSrvrItem::OnDoVerb(LONG iVerb)
{
	switch (iVerb)
	{
	// open - maps to OnOpen
	case OLEIVERB_OPEN:
	case -OLEIVERB_OPEN-1:  // allows positive OLEIVERB_OPEN-1 in registry
		OnOpen();
		break;

	// primary, show, and unknown map to OnShow
	case OLEIVERB_PRIMARY:  // OLEIVERB_PRIMARY is 0 and "Edit" in registry
	case OLEIVERB_SHOW:
	case OLEIVERB_UIACTIVATE: {
		COleServerDoc* pDoc = GetDocument();
		m_ShowUI = TRUE;
		OnShow();
		break;
	}					  

	case OLEIVERB_INPLACEACTIVATE: {
		COleServerDoc* pDoc = GetDocument();
		m_ShowUI = FALSE;
		OnShow();
		break;
   }

	// hide maps to OnHide
	case OLEIVERB_HIDE:
	case -OLEIVERB_HIDE-1:  // allows positive OLEIVERB_HIDE-1 in registry
		OnHide();
		break;


	case NO_UI_EMBEDDING:	{
		//Get InPlaceFrame ptr.
		CGenericDoc *pDoc = GetDocument();
		ASSERT(pDoc);

		POSITION pos = pDoc->GetFirstViewPosition();
		CView *pView = pDoc->GetNextView(pos);
		ASSERT(pView);

		m_ShowUI = FALSE;

		OnShow();

		CInPlaceFrame *pFrm = (CInPlaceFrame *)pView->GetParent();
		ASSERT(pFrm);

		pFrm->DestroyResizeBar();

		break;
		}
	default:
		// negative verbs not understood should return E_NOTIMPL
		if (iVerb < 0)
			AfxThrowOleException(E_NOTIMPL);

		// positive verb not processed --
		//  according to OLE spec, primary verb should be executed
		//  instead.
		OnDoVerb(OLEIVERB_PRIMARY);

		// also, OLEOBJ_S_INVALIDVERB should be returned.
		AfxThrowOleException(OLEOBJ_S_INVALIDVERB);
	}
}

/////////////////////////////////////////////////////////////////////////////
// CNetscapeSrvrItem diagnostics

#ifdef _DEBUG
void CNetscapeSrvrItem::AssertValid() const
{
	COleServerItem::AssertValid();
}

void CNetscapeSrvrItem::Dump(CDumpContext& dc) const
{
	COleServerItem::Dump(dc);
}
#endif

/////////////////////////////////////////////////////////////////////////////
