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

// cntritem.cpp : implementation of the CNetscapeCntrItem class
//

#include "stdafx.h"

#include "cntritem.h"

#ifdef _DEBUG
#undef THIS_FILE
static char BASED_CODE THIS_FILE[] = __FILE__;
#endif
#ifndef XP_WIN32
#include "olenls.h"
#define OLESTR(str) str
#endif
/////////////////////////////////////////////////////////////////////////////
// CNetscapeCntrItem implementation

#ifndef _AFXDLL
#undef new
#endif
IMPLEMENT_SERIAL(CNetscapeCntrItem, COleClientItem, 0)
#ifndef _AFXDLL
#define new DEBUG_NEW
#endif
extern void  FE_ConvertSpace(char *newName);

CNetscapeCntrItem::CNetscapeCntrItem(CGenericDoc* pContainer)
	: COleClientItem(pContainer)
{
    m_bLoading = FALSE; //  Not loading.
    m_bBroken = FALSE;  //  Not broken.
    m_bDelayed = FALSE; //  Not delayed.
    m_iLock = 0;    //  No one is referencing us.
	m_pOriginalItem = NULL;
	m_isFullPage = FALSE;
	m_isDirty = FALSE;
	m_bIsOleItem = FALSE;
	m_bSetExtents = FALSE;
	m_bCanSavedByOLE = FALSE;
	m_idSavedAs =DISPID_UNKNOWN; 

}

CNetscapeCntrItem::~CNetscapeCntrItem()
{
    //  If we've something loaded, then release it.
    if(m_bLoading == FALSE && m_bBroken == FALSE && m_bDelayed == FALSE)    {
        //  Assuming we're valid.
        Release(OLECLOSE_SAVEIFDIRTY);
    }
}
void CNetscapeCntrItem::OnActivate()
{
	char * pSource = NULL;
	const char* ptr;
	LPDISPATCH pdisp;
	HRESULT hr;
	int _convert;
	if (m_lpObject->QueryInterface(IID_IDispatch, (void**)&pdisp) == S_OK){
#ifdef XP_WIN32
		LPCOLESTR lpOleStr = T2COLE("SaveAs");
		hr = pdisp->GetIDsOfNames(IID_NULL, (unsigned short **)&lpOleStr, 1, LOCALE_USER_DEFAULT, &m_idSavedAs);
		pdisp->Release();
		if (hr == S_OK)	
			m_bCanSavedByOLE = TRUE;
		else
			m_idSavedAs = DISPID_UNKNOWN;
#else
			m_idSavedAs = DISPID_UNKNOWN;
#endif
	}

	const char* ptr1 = m_csAddress; 
	if (NET_IsLocalFileURL((char*)ptr1)) {
		XP_ConvertUrlToLocalFile(m_csAddress, &pSource);
		m_csDosName = *pSource;  // pick up the drive name.
		m_csDosName += ":";
		m_csDosName += strchr(pSource, '\\');  // pick up the real file name.
		ptr = m_csDosName;	 
		FE_ConvertSpace((char*)ptr);
		XP_FREE(pSource);
	}

	// the object does not support ole automation, try to find out if this is a storagefile, so
	// we can use OLESave().
	if (!m_bCanSavedByOLE) {
#ifdef XP_WIN32		// we will only want to handle saving when the object had storage file.
		int _convert;
		LPCOLESTR lpsz = A2CW(m_csDosName);
		if (StgIsStorageFile(lpsz) == S_OK) 
			m_bCanSavedByOLE = TRUE;
#else                       
		HRESULT sc1 = StgIsStorageFile(m_csDosName);
		if (GetScode(sc1) == S_OK) 
			m_bCanSavedByOLE = TRUE;
#endif
		}

	CGenericView* pView = GetActiveView();
	CFrameGlue *pFrameGlue = pView->GetFrame();
	if(pFrameGlue != NULL)	{
		m_bLocationBarShowing = pFrameGlue->GetChrome()->GetToolbarVisible(ID_LOCATION_TOOLBAR);
	}

	COleClientItem::OnActivate();
}

void CNetscapeCntrItem::OnChange(OLE_NOTIFICATION nCode, DWORD dwParam)
{
	COleClientItem::OnChange(nCode, dwParam);

	// this is a hack to test if we need to ask user to save the file or not.
	// since I can not get lpPersistStorage->IsDirty() to give me the correct 
	// answer.  
	if (nCode == OLE_CLOSED)
		m_isDirty = FALSE;

	if (nCode == OLE_CHANGED && dwParam == DVASPECT_CONTENT)
		m_isDirty = TRUE;

    //  Update all references to the item manually.
    MWContext *pContext = GetDocument()->GetContext()->GetContext();
    POSITION rIndex = m_cplElements.GetHeadPosition();
    LO_EmbedStruct *pLayoutData;
    while(rIndex != NULL)   {
        pLayoutData = (LO_EmbedStruct *)m_cplElements.GetNext(rIndex);
        if (pContext->compositor) {
            XP_Rect rect;
            
            CL_GetLayerBbox(pLayoutData->objTag.layer, &rect);
            CL_UpdateLayerRect(CL_GetLayerCompositor(pLayoutData->objTag.layer),
                               pLayoutData->objTag.layer, &rect, PR_FALSE);
        }
        else
            pContext->funcs->DisplayEmbed(pContext, FE_VIEW, pLayoutData);
    }
}

BOOL CNetscapeCntrItem::OnChangeItemPosition(const CRect& rectPos)
{
    //  Change the position of the item.
    //  Let this happen for them, we won't change layout, and we'll see how it goes.
    if(FALSE == COleClientItem::OnChangeItemPosition(rectPos))  {
        return(FALSE);
    }
    return(TRUE);
}

void CNetscapeCntrItem::OnGetItemPosition(CRect& rectPos)   {
    //  Give them coordinates to the position of the in place activated item.
    //  So, which item was selected?  We need layout data.
    CGenericDoc *pDoc = GetDocument();
    if(!pDoc)   {
        return;
    }
    CDCCX *pContextCX = pDoc->GetContext();
	if(!pContextCX || pContextCX->IsFrameContext() == FALSE)	{
		return;
	}

	CWinCX *pWinCX = VOID2CX(pContextCX, CWinCX);
    if(pWinCX->m_pSelected == NULL)   {
        //  Don't do anything, we don't know of any selection.
        return;
    }

    //  Okay, we know about the selected item.
    //  Now, we need to figure out where it is located in the view.
    HDC hdc = pWinCX->GetContextDC();

    long lLeft = pWinCX->m_pSelected->objTag.x + pWinCX->m_pSelected->objTag.x_offset - pWinCX->GetOriginX();
    long lRight = lLeft + pWinCX->m_pSelected->objTag.width;

    long lTop = pWinCX->m_pSelected->objTag.y + pWinCX->m_pSelected->objTag.y_offset - pWinCX->GetOriginY();
    long lBottom = lTop + pWinCX->m_pSelected->objTag.height;

    //  Convert our twips into pixels.
    RECT crConvert;
	::SetRect(&crConvert, CASTINT(lLeft), CASTINT(lTop), CASTINT(lRight), CASTINT(lBottom));
    ::LPtoDP(hdc, (POINT*) &crConvert, 2);

    rectPos = crConvert;

	pWinCX->ReleaseContextDC(hdc);
}

void CNetscapeCntrItem::OnDeactivateUI(BOOL bUndoable)
{

	COleClientItem::OnDeactivateUI(bUndoable);
	// Close an in-place active item whenever it removes the user
	//  interface.  The action here should match as closely as possible
	//  to the handling of the escape key in the view.
	if( IsInPlaceActive() == TRUE)   {
		TRY	{
	        Deactivate();

		}
		CATCH(CException, e)	{
			//	Something went wrong in OLE (other app down).
			//	No complicated handling here, just keep running.
		}
		END_CATCH
	}
}

// TODO: We can remove this check once everyone moves to MSVC 4.0
#if _MSC_VER >= 1000
BOOL CNetscapeCntrItem::OnUpdateFrameTitle()    {
    //  Nothing doing.  We stay the same.
    //  This ASSERTs anyhow, if it continues on down.
    return FALSE;
}
#else
void CNetscapeCntrItem::OnUpdateFrameTitle()    {
    //  Nothing doing.  We stay the same.
    //  This ASSERTs anyhow, if it continues on down.
    return;
}
#endif

BOOL CNetscapeCntrItem::OnGetWindowContext(CFrameWnd **ppMainFrame, CFrameWnd **ppDocFrame, LPOLEINPLACEFRAMEINFO lpFrameInfo) {
    //  First call the base.
    BOOL bRetval = COleClientItem::OnGetWindowContext(ppMainFrame, ppDocFrame, lpFrameInfo);

    //  Now, override the values with ones that make sense to us.
    CGenericDoc *pDoc = GetDocument();
	CDCCX *pCX = pDoc->GetContext();
	if(pCX != NULL && pCX->IsFrameContext() == TRUE)	{
		//	Get the frame from the window context.
		CWinCX *pWinCX = (CWinCX *)pCX;
		*ppMainFrame = pWinCX->GetFrame()->GetFrameWnd();
		*ppDocFrame = NULL;	//	Act like SDI.

		//	It's possible that there is no frame.
		//	In which case we should do what?  Fail.
		if(pWinCX->GetFrame()->GetFrameWnd() == NULL)	{
			bRetval = FALSE;
		}
	}
	else	{
		bRetval = FALSE;
	    *ppMainFrame = NULL;
	    *ppDocFrame = NULL;
	}

    return(bRetval);
}

BOOL CNetscapeCntrItem::OnShowControlBars(CFrameWnd *pFrameWnd, BOOL bShow)    {
    //  Call the base, they'll at least handle our normal control bar.
    BOOL bToolBarChanged = FALSE;
	//	Get the frame glue.
	CFrameGlue *pFrameGlue = CFrameGlue::GetFrameGlue(pFrameWnd);
	if(pFrameGlue != NULL)	{
	    if(bShow == FALSE)  {
	        //  Hide our stuff.
			if(m_bLocationBarShowing){
				pFrameGlue->GetChrome()->ShowToolbar(ID_LOCATION_TOOLBAR, FALSE);
				bToolBarChanged = TRUE;
			}
	    }
	    else  {
	        //  Show our stuff.
	        //  Assuming that we'll always be called to hide before we're called to show.
	       	if(m_bLocationBarShowing){
				pFrameGlue->GetChrome()->ShowToolbar(ID_LOCATION_TOOLBAR, m_bLocationBarShowing);
			 	bToolBarChanged = TRUE;
			}
	    }
	}
	BOOL bToolBarChanged1 = COleClientItem::OnShowControlBars(pFrameWnd, bShow);
    return(bToolBarChanged || bToolBarChanged1);
}

void CNetscapeCntrItem::Serialize(CArchive& ar)
{
	ASSERT_VALID(this);

	// Call base class first to read in COleClientItem data.
	// Since this sets up the m_pDocument pointer returned from
	//  CNetscapeCntrItem::GetDocument, it is a good idea to call
	//  the base class Serialize first.
	COleClientItem::Serialize(ar);

	// now store/retrieve data specific to CNetscapeCntrItem
	if (ar.IsStoring())
	{
	}
	else
	{
	}
}

BOOL CNetscapeCntrItem::CanActivate()	{
	//	Can't activate if the document itself is inplace active.
	CGenericDoc *pDoc = GetDocument();
	ASSERT_VALID(pDoc);
	ASSERT(pDoc->IsKindOf(RUNTIME_CLASS(COleServerDoc)));

	if(pDoc->IsInPlaceActive())	{
		return(FALSE);
	}

	return(COleClientItem::CanActivate());
}





/////////////////////////////////////////////////////////////////////////////
// CNetscapeCntrItem diagnostics

#ifdef _DEBUG
void CNetscapeCntrItem::AssertValid() const
{
	COleClientItem::AssertValid();
}

void CNetscapeCntrItem::Dump(CDumpContext& dc) const
{
	COleClientItem::Dump(dc);
}
#endif

/////////////////////////////////////////////////////////////////////////////
