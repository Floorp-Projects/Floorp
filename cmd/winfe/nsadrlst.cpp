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
#include "nsadrlst.h"
#include "namcomp.h"

#ifdef __APIAPIDLL
#include "festuff.h"
#else
#include "compstd.h"
#endif
#include "resource.h"

#include "addrfrm.h" //to get MOZ_NEWADDR
#include "wfemsg.h"
#include "intl_csi.h"


#define IDM_HEADER                      8192
#define MAX_HEADER_ITEMS                20


CListNameCompletionCX::CListNameCompletionCX(CListNameCompletionEntryList *pOwnerList)  
: CStubsCX(AddressCX, MWContextAddressBook)
{
	m_pOwnerList = pOwnerList;
	m_lPercent = 0;
	m_bAnimated = FALSE;
}

void CListNameCompletionCX::SetOwnerList(CListNameCompletionEntryList *pOwnerList)
{
	m_pOwnerList = pOwnerList;
}

void CListNameCompletionCX::SetProgressBarPercent(MWContext *pContext, int32 lPercent ) {
	//	Ensure the safety of the value.

	lPercent = lPercent < 0 ? 0 : ( lPercent > 100 ? 100 : lPercent );

	if ( m_lPercent == lPercent ) {
		return;
	}

	m_lPercent = lPercent;
	if (m_pOwnerList) {
		m_pOwnerList->SetProgressBarPercent(lPercent);
	}
}

void CListNameCompletionCX::Progress(MWContext *pContext, const char *pMessage) {
	if ( m_pOwnerList ) {
		m_pOwnerList->SetStatusText(pMessage);
	}
}

int32 CListNameCompletionCX::QueryProgressPercent()	{
	return m_lPercent;
}


void CListNameCompletionCX::AllConnectionsComplete(MWContext *pContext)    
{
    //  Call the base.
    CStubsCX::AllConnectionsComplete(pContext);

	//	Also, we can clear the progress bar now.
	m_lPercent = 0;
	if ( m_pOwnerList ) {
		m_pOwnerList->SetProgressBarPercent(m_lPercent);
		m_pOwnerList->AllConnectionsComplete(pContext);
	}
    if (m_pOwnerList) {
		CWnd *pOwnerWindow = m_pOwnerList->GetOwnerWindow();
		if(pOwnerWindow)
		{
			pOwnerWindow->SendMessageToDescendants(WM_IDLEUPDATECMDUI, (WPARAM)TRUE, (LPARAM)0);
		}
	}
}

void CListNameCompletionCX::UpdateStopState( MWContext *pContext )
{
    if (m_pOwnerList) {
		CWnd *pOwnerWindow = m_pOwnerList->GetOwnerWindow();
		if(pOwnerWindow)
		{
			pOwnerWindow->SendMessageToDescendants(WM_IDLEUPDATECMDUI, (WPARAM)TRUE, (LPARAM)0);
		}
	}
}

CWnd *CListNameCompletionCX::GetDialogOwner() const {
	return m_pOwnerList->GetOwnerWindow();
}

/////////////////////////////////////////////////////////////////////////////
// CListNameCompletionEntryList

STDMETHODIMP CListNameCompletionEntryList::QueryInterface(REFIID refiid, LPVOID * ppv)
{
	*ppv = NULL;
	if (IsEqualIID(refiid,IID_IUnknown))
   		*ppv = (LPUNKNOWN) this;
	else if (IsEqualIID(refiid,IID_IMsgList))
   		*ppv = (LPMSGLIST) this;
	else if (IsEqualIID(refiid,IID_IMailFrame))
		*ppv = (LPMAILFRAME) m_pMailFrame;

	if (*ppv != NULL) {
   		AddRef();
		return NOERROR;
	}
            
	return ResultFromScode(E_NOINTERFACE);
}



STDMETHODIMP_(ULONG) CListNameCompletionEntryList::AddRef(void)
{
	return ++m_ulRefCount;
}

STDMETHODIMP_(ULONG) CListNameCompletionEntryList::Release(void)
{
	ULONG ulRef;
	ulRef = --m_ulRefCount;
	if (m_ulRefCount == 0) 
		delete this;   	
	return ulRef;   	
}

void CListNameCompletionEntryList::ListChangeStarting( MSG_Pane* pane, XP_Bool asynchronous,
									   MSG_NOTIFY_CODE notify, MSG_ViewIndex where,
									   int32 num)
{
}

void CListNameCompletionEntryList::ListChangeFinished( MSG_Pane* pane, XP_Bool asynchronous,
									   MSG_NOTIFY_CODE notify, MSG_ViewIndex where,
									   int32 num)
{
	switch ( notify ) 
	{


	case MSG_NotifyInsertOrDelete:
		// if its insert or delete then tell my frame to add the next chunk of values
		// from the search
		if (notify == MSG_NotifyInsertOrDelete 
			&&  num > 0) 
		{
			if(m_bSearching)
			{
				AB_LDAPSearchResultsAB2(m_pPickerPane, where, num);
			}

		}
		else
		{
		}
		break;
	}
}

void CListNameCompletionEntryList::GetSelection( MSG_Pane* pane, MSG_ViewIndex **indices, int *count, 
							    int *focus)
{
}

void CListNameCompletionEntryList::SelectItem( MSG_Pane* pane, int item )
{
}

void CListNameCompletionEntryList::SetProgressBarPercent(int32 lPercent)
{
	m_pList->SetProgressBarPercent(lPercent);

}

void CListNameCompletionEntryList::SetStatusText(const char* pMessage)
{
	m_pList->SetStatusText(pMessage);
}

void CListNameCompletionEntryList::AllConnectionsComplete(MWContext *pContext)
{
		/*
	PerformDirectorySearch();

	int total = m_pOutliner->GetTotalLines();
	CString csStatus;
	if ( total > 1 ) {
		csStatus.Format( szLoadString( IDS_SEARCHHITS ), total );
	} else if ( total > 0 ) {
		csStatus.LoadString( IDS_SEARCHONEHIT );
	} else {
		csStatus.LoadString( IDS_SEARCHNOHITS );
	}
	m_barStatus.SetWindowText( csStatus );

	SendMessageToDescendants(WM_IDLEUPDATECMDUI, (WPARAM)TRUE, (LPARAM)0);
	*/

	CAddrFrame::HandleErrorReturn(AB_FinishSearchAB2(m_pPickerPane));
}

CWnd *CListNameCompletionEntryList::GetOwnerWindow()
{

	return m_pList->GetOwnerWindow();
}

STDMETHODIMP CListNameCompletionEntryMailFrame::QueryInterface(REFIID refiid, LPVOID * ppv)
{
	*ppv = NULL;
	if (IsEqualIID(refiid,IID_IUnknown))
   		*ppv = (LPUNKNOWN) this;
	else if (IsEqualIID(refiid,IID_IMailFrame))
		*ppv = (LPMAILFRAME) this;

	if (*ppv != NULL) {
   		AddRef();
		return NOERROR;
	}
            
	return ResultFromScode(E_NOINTERFACE);
}

STDMETHODIMP_(ULONG) CListNameCompletionEntryMailFrame::AddRef(void)
{
	return 0;
}

STDMETHODIMP_(ULONG) CListNameCompletionEntryMailFrame::Release(void)
{
	return 0;
}

// IMailFrame interface
CMailNewsFrame *CListNameCompletionEntryMailFrame::GetMailNewsFrame()
{
	return (CMailNewsFrame *) NULL; 
}

MSG_Pane *CListNameCompletionEntryMailFrame::GetPane()
{
	return (MSG_Pane*) m_pParentList->m_pPickerPane;
}

void CListNameCompletionEntryMailFrame::PaneChanged(MSG_Pane *pane, XP_Bool asynchronous, 
								 MSG_PANE_CHANGED_NOTIFY_CODE notify, int32 value)
{
	if (notify == MSG_PaneNotifyStartSearching)
	{
		m_pParentList->m_bSearching = TRUE;
		m_pParentList->m_pList->SearchStarted();
	//	m_barStatus.StartAnimation();
	}
	else if(notify == MSG_PaneNotifyStopSearching )
	{
		m_pParentList->m_bSearching = FALSE;
		m_pParentList->m_pList->SearchStopped();

	//	m_barStatus.StopAnimation();

	}
}


//============================================================ CNSAddressList
CNSAddressList::CNSAddressList()
{
   m_hPenNormal            = ::CreatePen(PS_SOLID,0,GetSysColor(COLOR_WINDOW));
   m_hPenGrid              = ::CreatePen(PS_SOLID,0,RGB(202,202,255));
   m_hPenGrey              = ::CreatePen(PS_SOLID, 0, RGB(202,202,255));
   m_hBrushNormal          = ::CreateSolidBrush(GetSysColor(COLOR_WINDOW));
   m_lastIndex             = 0;
   m_iDefaultBitmapId      = 0;
   m_bDrawTypeList         = FALSE;
   m_bArrowDown            = FALSE;
   m_iFieldControlWidth    = 0;
   m_iBitmapWidth          = 0;
   m_iTypeBitmapWidth      = 0;
   m_pNameField            = new CNSAddressNameEditField(this);
   m_pAddressTypeList      = new CNSAddressTypeControl;
   m_iItemHeight           = 0;
   m_pIAddressParent       = NULL;
   m_bCreated              = FALSE;
   m_hTextFont             = NULL;
   m_bParse                = TRUE;

   m_bDragging             = FALSE;
   m_pContext			   = NULL;

   m_pCX				    = new CListNameCompletionCX(NULL);
   INTL_CharSetInfo csi;

   csi = LO_GetDocumentCharacterSetInfo(m_pCX->GetContext());

   m_pCX->GetContext()->type = MWContextAddressBook;
   m_pCX->GetContext()->fancyFTP = TRUE;
   m_pCX->GetContext()->fancyNews = TRUE;
   m_pCX->GetContext()->intrupt = FALSE;
   m_pCX->GetContext()->reSize = FALSE;
   INTL_SetCSIWinCSID(csi, CIntlWin::GetSystemLocaleCsid());

   m_pPickerPane = NULL;
   m_bExpansion = TRUE;
#ifdef MOZ_NEWADDR
   int result;

   result = AB_CreateABPickerPane(&m_pPickerPane, m_pCX->GetContext(), WFE_MSGGetMaster(), 1);

   CListNameCompletionEntryList* pInstance = new CListNameCompletionEntryList(m_pPickerPane, this);
   pInstance->QueryInterface( IID_IMsgList, (LPVOID *) &m_pINameCompList );
   MSG_SetFEData( (MSG_Pane*) m_pPickerPane, (LPVOID) (LPUNKNOWN) (LPMAILFRAME) m_pINameCompList );
   m_pCX->SetOwnerList(pInstance);
#endif

}

LRESULT CNSAddressList::DefWindowProc(UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
        case WM_ERASEBKGND:
            return DoEraseBkgnd(m_hWnd,(HDC)wParam);
        case WM_SETFOCUS:
            CListBox::DefWindowProc(message,wParam,lParam);
            DoSetFocus((HWND)wParam);
            return 0;
        case WM_KILLFOCUS:
            DoKillFocus((HWND)wParam);
            break;
        case WM_LBUTTONDOWN:
            {
                POINT point;
                CListBox::DefWindowProc(message,wParam,lParam);
                point.x = LOWORD(lParam);  // horizontal position of cursor 
                point.y = HIWORD(lParam);  // vertical position of cursor 
                DoLButtonDown(m_hWnd,wParam,&point);
            }
            return 0;
        case WM_LBUTTONUP:
            {
                POINT point;
                CListBox::DefWindowProc(message,wParam,lParam);
                point.x = LOWORD(lParam);  // horizontal position of cursor 
                point.y = HIWORD(lParam);  // vertical position of cursor 
                DoLButtonUp(m_hWnd,wParam,&point);
            }
            return 0;
        case WM_VSCROLL:
            {
                CListBox::DefWindowProc(message,wParam,lParam);
                int nScrollCode = (int) LOWORD(wParam); // scroll bar value 
                int nPos = (short int) HIWORD(wParam);  // scroll box position 
                DoVScroll(m_hWnd, nScrollCode, nPos);
            }
            return 0;
        case WM_CHILDLOSTFOCUS:
            DoChildLostFocus();
            return 0;
        case WM_NOTIFYSELECTIONCHANGE:
            return (DoNotifySelectionChange(FALSE));
        case WM_DISPLAYTYPELIST:
            DoDisplayTypeList();
            return 0;
        case WM_COMMAND:
#ifdef XP_WIN16
            if (HIWORD(lParam) == EN_CHANGE)
#else
            if (HIWORD(wParam) == EN_CHANGE)
#endif
            {
     	        CNSAddressInfo *pAddress = (CNSAddressInfo *)GetItemDataPtr(GetActiveSelection());
                pAddress->SetExpansion(TRUE);
            }
            if (DoCommand(m_hWnd,wParam,lParam))
               return 0;
            break;
        case WM_GETDLGCODE:
          return DLGC_WANTTAB | DLGC_WANTARROWS;
        case WM_KEYDOWN:
          onKeyDown((int)wParam, (DWORD)lParam);
          break;
        case WM_MOUSEMOVE:
          onMouseMove(m_hWnd, (WORD)wParam, (int)LOWORD(lParam), (int)HIWORD(lParam));
          break;
    }
    return CListBox::DefWindowProc(message,wParam,lParam);
}

void CNSAddressList::EnableParsing(BOOL bParse)
{
    m_bParse = bParse;
}

BOOL CNSAddressList::GetEnableParsing()
{
	return m_bParse;
}

void CNSAddressList::EnableExpansion(BOOL bExpansion)
{
	m_bExpansion = bExpansion;
}

BOOL CNSAddressList::GetEnableExpansion()
{
	return m_bExpansion;
}

void CNSAddressList::SetControlParent(LPADDRESSPARENT pIAddressParent)
{
    m_pIAddressParent = pIAddressParent;
    ASSERT(m_pNameField);
    m_pNameField->SetControlParent(pIAddressParent);
}

//=========================================================== ~CNSAddressList
CNSAddressList::~CNSAddressList()
{
	if (m_hTextFont) {
		theApp.ReleaseAppFont(m_hTextFont);
	}
    if (m_hPenNormal != NULL)
        ::DeleteObject((HGDIOBJ)m_hPenNormal);
    if (m_hPenGrid != NULL)
        ::DeleteObject((HGDIOBJ)m_hPenGrid);
    if (m_hBrushNormal != NULL)
        ::DeleteObject((HGDIOBJ)m_hBrushNormal);
    if (m_hPenGrey != NULL)
        ::DeleteObject((HGDIOBJ)m_hPenGrey);
    delete m_pAddressTypeList;
    delete m_pNameField;

	if(!m_pCX->IsDestroyed()) {
		m_pCX->DestroyContext();
	}
#ifdef MOZ_NEWADDR
	if(m_pPickerPane)
	{
		AB_ClosePane(m_pPickerPane);
	}

	m_pINameCompList->Release();
#endif
}

BOOL CNSAddressList::DoCommand( HWND hwnd, WPARAM wParam, LPARAM lParam )
{
#ifdef XP_WIN16
    if (HIWORD(lParam) == EN_SETFOCUS)
#else
    if (HIWORD(wParam) == EN_SETFOCUS)
#endif
    {
    	selectAllEntries(FALSE);
    	return TRUE;
    }

#ifdef XP_WIN16
    if (HIWORD(lParam) == EN_CHANGE)
#else
    if (HIWORD(wParam) == EN_CHANGE)
#endif
    {
    	UpdateHeaderContents();
    	return TRUE;
    }
#ifdef XP_WIN16
    else if (HIWORD(lParam) == LBN_SELCHANGE)
#else
    else if (HIWORD(wParam) == LBN_SELCHANGE)
#endif
    {
    	UpdateHeaderType();
    	return TRUE;
    }
#ifdef XP_WIN16
    else if (wParam >= IDM_HEADER && wParam <= IDM_HEADER+MAX_HEADER_ITEMS)
    {
    	HeaderCommand(wParam-IDM_HEADER);
    	return TRUE;
    }
#else
    else if (!HIWORD(wParam) && // zero if from a menu.
    	(LOWORD(wParam) >= IDM_HEADER && LOWORD(wParam) <= IDM_HEADER+MAX_HEADER_ITEMS))
    {
    	HeaderCommand(LOWORD(wParam)-IDM_HEADER);
    	return TRUE;
    }
#endif
    return FALSE;
}

int CNSAddressList::SetActiveSelection(int nSelect)
{
    if (GetActiveSelection() != nSelect)
    	return SetCurSel(nSelect);
    return nSelect;
}

BOOL CNSAddressList::Create(CWnd *pParent, int id)
{
    RECT rect;
    ::GetClientRect(pParent->m_hWnd, &rect);
    BOOL bRetVal = CListBox::Create(
    	WS_CLIPCHILDREN|WS_CHILD|WS_VISIBLE|WS_VSCROLL|
    	LBS_HASSTRINGS|LBS_OWNERDRAWFIXED|LBS_NOTIFY|LBS_WANTKEYBOARDINPUT|LBS_NOINTEGRALHEIGHT,
	    rect,pParent,id);
    m_bCreated = TRUE;

    return bRetVal;
}

//=============================================================== SetSel
int CNSAddressList::SetSel(int nIndex, BOOL bSelect)
{	
	int result;
	if (bSelect)
	{
		result = SetActiveSelection(nIndex);
		m_pNameField->SetSel(0,-1,TRUE);
	}
	else
	{
		result = SetActiveSelection(-1);
		m_pNameField->SetSel(-1,0,TRUE);
	}
    Invalidate();
	return result;
}

//=============================================================== AppendEntry
int CNSAddressList::AppendEntry( NSAddressListEntry *pAddressEntry, BOOL expandName )
{
	SetActiveSelection(-1);
	return InsertEntry( (int)-1, pAddressEntry, expandName );
}

//=============================================================== InsertEntry
int CNSAddressList::InsertEntry( int nIndex, NSAddressListEntry *pAddressEntry, BOOL expandName )
{
   if (pAddressEntry && pAddressEntry->szName && strlen(pAddressEntry->szName))
   {
      if (m_bDrawTypeList)
      {
         if (pAddressEntry->szType && strlen(pAddressEntry->szType))
         {
            int iPos;
            ASSERT(m_pAddressTypeList);
        		if ((iPos =m_pAddressTypeList->FindStringExact(-1,pAddressEntry->szType))!=LB_ERR)
            {
               CNSAddressTypeInfo * pInfo = (CNSAddressTypeInfo*)m_pAddressTypeList->GetItemData(iPos);
               ASSERT(pInfo);
               if (pInfo->GetExclusive())
               {
                  CString cs;
                  cs = pInfo->GetValue();
                  if (cs.GetLength())
                  {
                      cs += ",";
                      cs += pAddressEntry->szName;  
                  }
                  else
                      cs = pAddressEntry->szName;
                  pInfo->SetValue(cs);
               }
               if (pInfo->GetHidden())
                  return LB_ERR;
            }
         }
      }
	   if (GetActiveSelection()!=LB_ERR && !m_pNameField->LineLength())
	   {
	      NSAddressListEntry pTemp;
	      GetEntry(GetActiveSelection(),&pTemp);
	      if (!pTemp.szName||!strlen(pTemp.szName))
	      {
            SetEntry(GetActiveSelection(),pAddressEntry);
            CRect rect;
            GetItemRect(m_hWnd, GetActiveSelection(),&rect);
            InvalidateRect(rect);
            m_pNameField->SetFocus();
            return GetActiveSelection();
	      }
	   }
   }
	CNSAddressInfo *pAddress = new CNSAddressInfo;
	if ( pAddressEntry == NULL)
	{
		// Default to callback provided bitmap and the first
		// address type in the address type list.
    	if (m_bDrawTypeList)
    	{
		    CString cs;
			int index = GetCount();
			if (index > 0 && index != LB_ERR)
			{
				CNSAddressInfo *pLastAddress = (CNSAddressInfo *)GetItemDataPtr(index-1);
				cs = pLastAddress->GetType();                                    
				int index = m_pAddressTypeList->FindStringExact(-1,cs);
				if (index != LB_ERR)
				{
					CNSAddressTypeInfo * pInfo = (CNSAddressTypeInfo *)m_pAddressTypeList->GetItemData(index);
					ASSERT(pInfo);
					if (pInfo->GetExclusive())
					m_pAddressTypeList->GetText(index, cs);
				}
			}
			else
			{
				m_pAddressTypeList->GetText(0, cs);
			}
	        pAddress->SetType(cs);
		    pAddress->SetBitmap(0);
			pAddress->SetEntryID();
			pAddress->SetName("");

		}
#ifdef MOZ_NEWADDR
		pAddress->SetPickerPane(m_pPickerPane);
#endif

	}
	else
	{
	    if (m_bDrawTypeList)
	    {
		    BOOL bFound = FALSE;
		    for (int i = 0; i < m_pAddressTypeList->GetCount(); i++)
		    {
			    CString cs;
			    m_pAddressTypeList->GetText(i,cs);
			    if (cs == pAddressEntry->szType)
			    {
				    bFound = TRUE;
				    m_pAddressTypeList->SetCurSel(i);
				    break;
			    }
		    }
		    if (!bFound)
		    {
			    delete pAddress;
			    return (int)LB_ERR;
		    }
	    }
	    pAddress->SetType(pAddressEntry->szType);
	    pAddress->SetName(pAddressEntry->szName);
	    pAddress->SetBitmap(pAddressEntry->idBitmap);
	    pAddress->SetEntryID(pAddressEntry->idEntry);
#ifdef MOZ_NEWADDR
		pAddress->SetPickerPane(m_pPickerPane);
#endif

	}
	int index = InsertString( nIndex, (LPCTSTR)pAddress );
	if ( index < 0 )
		return FALSE;
	SetItemDataPtr( index, pAddress );
	// this ensures that there will always be a selected line if there are entries
	if ( (GetCount() == 1) || (GetActiveSelection() == LB_ERR) ) 
    {
		SetActiveSelection( 0 );
        UpdateHeaderType();
        UpdateHeaderContents();
    }

	CRect WindowRect, ItemRect;
	GetWindowRect(WindowRect);
   
    if (m_pIAddressParent && expandName)
		m_pIAddressParent->AddedItem(m_hWnd, 0, index);

    SetActiveSelection(index);
	return index;
}
//================================================================== SetEntry
void CNSAddressList::SetCSID( int16 csid )
{
	if(m_hWnd)
	{
	    HDC hDC = ::GetDC(m_hWnd);
	    LOGFONT lf;                 
	    memset(&lf,0,sizeof(LOGFONT));

    	lf.lfPitchAndFamily = FF_MODERN | FIXED_PITCH;
    	strcpy(lf.lfFaceName, IntlGetUIFixFaceName(csid));
    	lf.lfHeight = -MulDiv(NS_ADDRESSFONTSIZE,::GetDeviceCaps(hDC,LOGPIXELSY),72);
    	lf.lfQuality = PROOF_QUALITY;
    	lf.lfCharSet = IntlGetLfCharset(csid); 
		m_hTextFont = theApp.CreateAppFont( lf);
		::SendMessage(GetSafeHwnd(), WM_SETFONT, (WPARAM)m_hTextFont, FALSE);
    	::ReleaseDC(m_hWnd,hDC);
	}

	if(m_pNameField)
		m_pNameField->SetCSID(csid);
}

//==================================================================ShowNameCompletionPicker
void CNSAddressList::ShowNameCompletionPicker(CWnd* pParent)
{

	CString name;

	//Stop the current name completion
	StopNameCompletion();

	//turn parsing and expansion off while we have dialog up
	BOOL pOldParse = GetEnableParsing();
	BOOL pOldExpansion = GetEnableExpansion();

	EnableParsing(FALSE);
	EnableExpansion(FALSE);
	CEdit *pNameField = GetAddressNameField();
	if(pNameField)
	{
		pNameField->GetWindowText(name);


		CNameCompletion dialog( name, pParent);
		
		int result = dialog.DoModal();

		//reset parsing and expansion state.
		EnableParsing(pOldParse);
		EnableExpansion(pOldExpansion);

		if(result == IDOK)
		{
			AB_NameCompletionCookie *pCookie = dialog.GetNameCompletionCookie();

			if(pCookie)
			{
				SetNameCompletionCookieInfo(pCookie, 1, NC_NameComplete);
				DoNotifySelectionChange();

			}
			if ((GetActiveSelection()+1 == GetCount()))
			{
				// don't add a second blank line
				if ( m_pNameField->LineLength() != 0 )
                {
					// add the new address entry to the list
					InsertEntry( GetActiveSelection()+1,NULL);
                }
			}
			SetActiveSelection( GetActiveSelection()+1 );
			pNameField->SetFocus();

		}

		if(result == IDCANCEL)
		{
			//they have chosen not to accept a result so they have
			//turned of name completion until they type something again.
			SetNameCompletionCookieInfo(NULL, 0, NC_NameComplete);
			pNameField->SetFocus();
		}
	}

   
}

//=================================================================NameCompletionExit
int NameCompletionExit(AB_NameCompletionCookie *cookie, int numResults,
									void *FEcookie)
{


	if(FEcookie)
	{
		FENameCompletionCookieInfo *pCookieInfo = (FENameCompletionCookieInfo*)FEcookie;

		CNSAddressList *pList = pCookieInfo->GetList();
		int nIndex = pCookieInfo->GetIndex();

		if(pList)
		{
			pList->SetNameCompletionCookieInfo(cookie, numResults, NC_NameComplete, nIndex );
			//how do we make correct name show up correctly?
			CNSAddressNameEditField * nameField = (CNSAddressNameEditField *)pList->GetAddressNameField();
			nameField->DrawNameCompletion();
		}

	}
	return 0;
}

int NameExpansionExit(AB_NameCompletionCookie *cookie, int numResults, void *FEcookie)
{

	if(FEcookie)
	{
		FENameCompletionCookieInfo *pCookieInfo = (FENameCompletionCookieInfo*)FEcookie;

		CNSAddressList *pList = pCookieInfo->GetList();
		int nIndex = pCookieInfo->GetIndex();

		if(pList)
		{
			pList->SetNameCompletionCookieInfo(cookie, numResults, NC_Expand, nIndex );
		}

	}
	return 0;

}

//=================================================================StartNameCompletion
void CNSAddressList::StartNameCompletion(int nIndex)
{

	CString text;

	m_pNameField->GetWindowText(text);

	int result;

	if(nIndex != -1 && (nIndex < 0 ||  nIndex >= GetCount()))
	{
		return;
	}

	if(nIndex == -1)
		nIndex = GetActiveSelection();

	if(nIndex != LB_ERR)
	{
		//dont do if name completion is turned off for this index
		CNSAddressInfo *info = (CNSAddressInfo*)GetItemDataPtr(nIndex);

		if(info->GetNameCompletionEnum() != NC_None)
		{
			FENameCompletionCookieInfo *pCookieInfo = new FENameCompletionCookieInfo();

			pCookieInfo->SetList(this);
			pCookieInfo->SetIndex(nIndex);

			if(info)
			{
#ifdef FE_IMPLEMENTS_VISIBLE_NC
				result = AB_NameCompletionSearch(info->GetPickerPane(), (const char*) text,
											 NameCompletionExit, FALSE, pCookieInfo);
#else
				result = AB_NameCompletionSearch(info->GetPickerPane(), (const char*) text,
											 NameCompletionExit, pCookieInfo);
#endif
			}
		}
	}
}


//=================================================================StopNameCompletion
void CNSAddressList::StopNameCompletion(int nIndex, BOOL bEraseCookie)
{

	CString text;

	int result;

	if(nIndex != -1 && (nIndex < 0 ||  nIndex >= GetCount()))
	{
		return;
	}

	if(nIndex == -1)
		nIndex = GetActiveSelection();

	if(nIndex != LB_ERR)
	{
		CNSAddressInfo *info = (CNSAddressInfo*)GetItemDataPtr(nIndex);

		if(bEraseCookie)
		{
			AB_NameCompletionCookie *pOldCookie = info->GetNameCompletionCookie();

			if(pOldCookie)
			{
				AB_FreeNameCompletionCookie(pOldCookie);
			}

			info->SetNameCompletionCookie(NULL);
			info->SetNumNameCompletionResults(-1);
		}

		//make sure m_pNameField doesn't keep setting timer off
		m_pNameField->StopNameCompletion();
		if(info)
		{
#ifdef FE_IMPLEMENTS_VISIBLE_NC
			result = AB_NameCompletionSearch(info->GetPickerPane(), (const char*) NULL,
										 NameCompletionExit, FALSE, NULL);
#else
			result = AB_NameCompletionSearch(info->GetPickerPane(), (const char*) NULL,
										 NameCompletionExit, NULL);
#endif
		}
	}
}

//=================================================================StartNameExpansion
void CNSAddressList::StartNameExpansion(int nIndex)
{

	CString text;

	m_pNameField->GetWindowText(text);

	int result;

	if(nIndex != -1 && (nIndex < 0 ||  nIndex >= GetCount()))
	{
		return;
	}

	if(nIndex == -1)
		nIndex = GetActiveSelection();

	if(nIndex != LB_ERR)
	{
		CNSAddressInfo *info = (CNSAddressInfo*)GetItemDataPtr(nIndex);

		FENameCompletionCookieInfo *pCookieInfo = new FENameCompletionCookieInfo();

		pCookieInfo->SetList(this);
		pCookieInfo->SetIndex(nIndex);

		if(info)
		{
#ifdef FE_IMPLEMENTS_VISIBLE_NC
			result = AB_NameCompletionSearch(info->GetPickerPane(), (const char*) text,
										 NameExpansionExit, FALSE, pCookieInfo);
#else
			result = AB_NameCompletionSearch(info->GetPickerPane(), (const char*) text,
										 NameExpansionExit, pCookieInfo);
#endif
		}
	}
}

//================================================================== SetEntryHasNameCompletion
void CNSAddressList::SetEntryHasNameCompletion(BOOL bHasNameCompletion, int nIndex)
{
	if(nIndex != -1 && (nIndex < 0 ||  nIndex >= GetCount()))
	{
		return;
	}

	if(nIndex == -1)
	{
		nIndex = GetActiveSelection();
	}

	if(nIndex != LB_ERR)
	{
		CNSAddressInfo *pAddress = (CNSAddressInfo *)GetItemDataPtr( nIndex );
		if(bHasNameCompletion)
		{
			pAddress->SetNameCompletionEnum(NC_NameComplete);
			StartNameCompletion(nIndex);
		}
		else
		{
			pAddress->SetNameCompletionEnum(NC_None);
			StopNameCompletion(nIndex);
			Invalidate();
		}

	}
}

//================================================================== GetEntryHasNameCompletion
BOOL CNSAddressList::GetEntryHasNameCompletion(int nIndex)
{
	if(nIndex != -1 && (nIndex < 0 ||  nIndex >= GetCount()))
	{
		return FALSE;
	}

	if(nIndex == -1)
	{
		nIndex = GetActiveSelection();
	}

	if(nIndex != LB_ERR)
	{
		CNSAddressInfo *pAddress = (CNSAddressInfo *)GetItemDataPtr( nIndex );
		return (pAddress->GetNameCompletionEnum() != NC_None);
	}
	return FALSE;
}


//================================================================== SetContext
void CNSAddressList::SetContext(MWContext *pContext)
{
	m_pContext = pContext;
}

//================================================================== GetContext
MWContext *CNSAddressList::GetContext()
{
	return m_pContext;
}

//==============================================SetNameCompletionCookieInfo
void CNSAddressList::SetNameCompletionCookieInfo(AB_NameCompletionCookie *pCookie,
												 int nNumResults,  
												 NameCompletionEnum ncEnum, int nIndex)
{
	if(nIndex != -1 && nIndex < 0 &&  nIndex > GetCount())
	{
		return;
	}

	if(nIndex == -1)
	{
		nIndex = GetActiveSelection();
	}

	if(nIndex != LB_ERR)
	{
		CNSAddressInfo *pAddress = (CNSAddressInfo *)GetItemDataPtr( nIndex );
		AB_NameCompletionCookie *pOldCookie = pAddress->GetNameCompletionCookie();

		if(pOldCookie != NULL && pOldCookie == pCookie)
		{
			int i = 0;
		}
		if(pOldCookie)
		{
			AB_FreeNameCompletionCookie(pOldCookie);
		}
		pAddress->SetNameCompletionCookie(pCookie);
		pAddress->SetNumNameCompletionResults(nNumResults);
		pAddress->SetNameCompletionEnum(ncEnum);
		//if we are supposed to expand then expand the address
		if(ncEnum == NC_Expand)
		{
			if(pCookie != NULL && nNumResults == 1)
			{
				char *pName = AB_GetHeaderString(pCookie);

				pAddress->SetName(pName);
				XP_FREE(pName);
				Invalidate();
			}
		}
	}


}

void CNSAddressList::GetNameCompletionCookieInfo(AB_NameCompletionCookie **pCookie,
													int *pNumResults, int nIndex)
{

	*pCookie = NULL;
	*pNumResults = 0;

	if(nIndex != -1 && nIndex < 0 && nIndex > GetCount())
	{
		return;
	}

	if(nIndex == -1)
	{
		nIndex = GetActiveSelection();
	}

	if(nIndex != LB_ERR)
	{
		CNSAddressInfo *pAddress = (CNSAddressInfo *)GetItemDataPtr( nIndex );
		*pCookie = pAddress->GetNameCompletionCookie();
		*pNumResults = pAddress->GetNumNameCompletionResults();
	}

}

//================================================================== SearchStarted
void CNSAddressList::SearchStarted()
{
	if(m_pIAddressParent)
	{
		m_pIAddressParent->StartNameCompletionSearch();
	}
}

//================================================================== SearchStopped
void CNSAddressList::SearchStopped()
{
	if(m_pIAddressParent)
	{
		m_pIAddressParent->StopNameCompletionSearch();
	}
}

void CNSAddressList::SetProgressBarPercent(int32 lPercent)
{
	if(m_pIAddressParent)
	{
		m_pIAddressParent->SetProgressBarPercent(lPercent);
	}
}

void CNSAddressList::SetStatusText(const char* pMessage)
{
	if(m_pIAddressParent)
	{
		m_pIAddressParent->SetStatusText(pMessage);
	}

}

CWnd *CNSAddressList::GetOwnerWindow()
{
	if(m_pIAddressParent)
	{
		return m_pIAddressParent->GetOwnerWindow();
	}
	return NULL;
}

//================================================================== SetEntry
BOOL CNSAddressList::SetEntry( int nIndex, NSAddressListEntry *pNewAddressEntry )
{
	// sanity checks
	if ( (int)nIndex >= GetCount() )
		return FALSE;
    if (m_bDrawTypeList)
    	if (IsWindow(m_pAddressTypeList->m_hWnd))
	    	if ( m_pAddressTypeList->FindStringExact( -1, pNewAddressEntry->szType ) == LB_ERR )
		    	return FALSE;
	// update entry
	CNSAddressInfo *pAddress = (CNSAddressInfo *)GetItemDataPtr( nIndex );
    pAddress->SetType(pNewAddressEntry->szType);
	pAddress->SetName(pNewAddressEntry->szName);
	pAddress->SetBitmap(pNewAddressEntry->idBitmap);
	pAddress->SetEntryID(pNewAddressEntry->idEntry);


	unsigned long entryID = 0;
	UINT bitmapID = 0;
	char * pszFullName = NULL;

#ifdef MOZ_NEWADDR
	//since we just changed the name up there we should notify parent.
	m_pIAddressParent->ChangedItem((char*)pNewAddressEntry->szName, nIndex, m_hWnd, 
				&pszFullName, &entryID, &bitmapID);

#endif

    if (m_pIAddressParent)
	{
	    NSAddressListEntry entry;
	    BOOL bRetVal = GetEntry(nIndex,&entry);
		if (bRetVal)
		{



        	CNSAddressInfo *pAddress = (CNSAddressInfo *)GetItemDataPtr( nIndex );
            if (pAddress && pAddress->GetExpansion())
            {
                BOOL bExpand = TRUE;
                if (m_bDrawTypeList)
                {
		            CNSAddressTypeInfo * pInfo = (CNSAddressTypeInfo *)m_pAddressTypeList->GetItemData(
                        m_pAddressTypeList->GetCurSel());

                    bExpand = pInfo->GetExpand();
                }
                if (bExpand && m_bExpansion)
                {
#ifdef MOZ_NEWADDR
						AB_NameCompletionCookie *pCookie = pAddress->GetNameCompletionCookie();
						int nNumResults = pAddress->GetNumNameCompletionResults();
						BOOL bExpandName = FALSE;

						if(pCookie != NULL && nNumResults == 1)
						{
							pszFullName = AB_GetHeaderString(pCookie);

						}
						//if we have multiple matches and preference is set.
						else if(pCookie != NULL && nNumResults > 1 && g_MsgPrefs.m_bShowCompletionPicker)
						{
							ShowNameCompletionPicker(this);
							return TRUE;
						}
						else
						{
							pszFullName = XP_STRDUP((char*)entry.szName);
						}

						//since we just changed the name, we need to notify parent.
    					m_pIAddressParent->ChangedItem((char*)entry.szName, nIndex, m_hWnd, 
							&pszFullName, &entryID, &bitmapID);

	    				if (pszFullName != NULL)
		    			{
							pAddress->SetName(pszFullName);
							if (bitmapID)
    							SetItemBitmap(GetActiveSelection(), bitmapID);
    						if (entryID)
    						SetItemEntryID(GetActiveSelection(), entryID);
    						free(pszFullName);
	   					}

#else
    			    m_pIAddressParent->ChangedItem((char*)entry.szName, nIndex, m_hWnd, 
    				    &pszFullName, &entryID, &bitmapID);
    			    if (pszFullName != NULL)
    			    {
                        pAddress->SetName(pszFullName);
    				    if (bitmapID)
    					    SetItemBitmap(nIndex, bitmapID);
    				    if (entryID)
    					    SetItemEntryID(nIndex, entryID);
    				    free(pszFullName);
					}
#endif
                }
            }
		}
	}

	return TRUE;
}


//================================================================== GetEntry
BOOL CNSAddressList::GetEntry( int nIndex, NSAddressListEntry *pAddressEntry )
{
	if ( (int)nIndex >= GetCount() )
		return FALSE;
	// fill out the NSAddressListEntry to return it to the caller
	CNSAddressInfo *pAddress = (CNSAddressInfo *)GetItemDataPtr( nIndex );
	if ( pAddress == (CNSAddressInfo *)-1 || !pAddress->GetName())
		return FALSE;
	pAddressEntry->szType = pAddress->GetType();
	pAddressEntry->szName = pAddress->GetName();
	pAddressEntry->idBitmap = pAddress->GetBitmap();
	pAddressEntry->idEntry = pAddress->GetEntryID();
	return TRUE;
}


BOOL CNSAddressList::RemoveSelection(int nIndex)
{
    if (nIndex == -1)
    {
	int iSel = GetActiveSelection();
	if (iSel != LB_ERR)
	    return DeleteEntry(iSel);
	return FALSE;
    }
    return DeleteEntry(nIndex);
}
//=============================================================== DeleteEntry
BOOL CNSAddressList::DeleteEntry( int nIndex )
{
	// sanity check
	if( (int)nIndex >= GetCount() )
		return FALSE;

    if (m_bDrawTypeList)
    {
        CNSAddressTypeInfo * pInfo = (CNSAddressTypeInfo *)m_pAddressTypeList->GetItemData(
            m_pAddressTypeList->GetCurSel());
        if (pInfo)
        {
            if (pInfo->GetExclusive())
                pInfo->SetValue(NULL);
        }
    }

	if(m_bDrawTypeList)
	{
		if (GetCount() == 1)
		{
    		m_pNameField->SetWindowText("");
    		UpdateHeaderContents();
			if (m_pIAddressParent)
				m_pIAddressParent->DeletedItem(m_hWnd, 0, nIndex);
			return FALSE;
		}
	}

	// handle selection if deleting selected address
	if ( (int)nIndex == GetActiveSelection() )
	{
		// delete only entry?
		if ( GetCount() == 1 )
		{
			if (m_bDrawTypeList)
				m_pAddressTypeList->ShowWindow( SW_HIDE );
			m_pNameField->ShowWindow( SW_HIDE );
		}
		else if ( nIndex > 0 ) 
    	{
	        SetActiveSelection(nIndex-1);
    	}
	}
	// delete this entry
	if ( DeleteString( nIndex ) == LB_ERR )
		return FALSE;

    if (m_pIAddressParent)
	m_pIAddressParent->DeletedItem(m_hWnd, 0, nIndex);

	return TRUE;
}


//================================================================= FindEntry
int CNSAddressList::FindEntry( int nStart, LPCTSTR lpszName )
{
	CNSAddressInfo *pAddress;
	if ( nStart == (int)-1 )
		nStart = 0;
	for ( int i = nStart; i < GetCount(); i++ )
	{
		pAddress = (CNSAddressInfo *)GetItemDataPtr(i);
	if (pAddress->GetName())
		if ( strcmp( pAddress->GetName(), lpszName ) == 0 )
			return i;
	}
	return (int)LB_ERR;
}


BOOL CNSAddressList::AddAddressType(
    char * pszChoice,
    UINT pidBitmap,
    BOOL bExpand,
    BOOL bHidden,
    BOOL bExclusive,
    DWORD dwUserData)
{
	// Create the ComboBox if necessary
    if (pszChoice != NULL)
    {
    	if ( !IsWindow( m_pAddressTypeList->m_hWnd ) )
	    	VERIFY( m_pAddressTypeList->Create( this ) );
    	// create the edit field if necessary
        int position = m_pAddressTypeList->InsertString( -1, pszChoice );
        if (position == LB_ERR)
            return FALSE;
        CNSAddressTypeInfo * pInfo = new CNSAddressTypeInfo(pidBitmap,bHidden,bExclusive,dwUserData,bExpand);
        if (pInfo == NULL)
            return FALSE;
        m_pAddressTypeList->SetItemData(position,(DWORD)pInfo);
    	m_bDrawTypeList = TRUE;
    	if (m_pAddressTypeList->GetCurSel()==LB_ERR)
    		m_pAddressTypeList->SetCurSel(0);
    }
   	if ( !IsWindow( m_pNameField->m_hWnd ) )
   		VERIFY( m_pNameField->Create( this, m_pContext ) );
    return TRUE;
}



//==================================================== GetAddressTypeComboBox
CListBox * CNSAddressList::GetAddressTypeComboBox( void )
{
	// Create the ComboBox if necessary
	if ( !IsWindow( m_pAddressTypeList->m_hWnd ) )
		VERIFY( m_pAddressTypeList->Create( this ) );
	return (CListBox*)m_pAddressTypeList;
}


//======================================================= GetAddressNameField
CEdit * CNSAddressList::GetAddressNameField( void )
{
	return m_pNameField;
}


//=========================================================== EnableGridLines
void CNSAddressList::EnableGridLines( BOOL bEnable )
{
	if ( m_bGridLines == bEnable )
		return;
	m_bGridLines = bEnable;
	// redraw the control using the new grid-lines setting
	Invalidate( TRUE );
}


int CNSAddressList::GetItemFromPoint(LPPOINT lpPoint)
{
   BOOL bOutside;
   return ItemFromPoint(m_hWnd,lpPoint,&bOutside);
}

/////////////////////////////////////////////////////////////////////////////
// CNSAddressList private API




UINT CNSAddressList::ItemFromPoint(HWND hwnd, LPPOINT lpPoint, BOOL * bOutside) const
{
	RECT rect;
	::GetClientRect(hwnd, &rect);

	int iHeight = (int)::SendMessage(hwnd, LB_GETITEMHEIGHT, 0, 0);
	int iCount = (int)::SendMessage(hwnd, LB_GETCOUNT, 0, 0);
	int iTopIndex = (int)::SendMessage(hwnd, LB_GETTOPINDEX, 0, 0);

	int iListHeight = iHeight * ( iCount - iTopIndex );
	rect.bottom = rect.bottom < iListHeight ? rect.bottom : iListHeight;

	*bOutside = !::PtInRect(&rect, *lpPoint);

	if ( *bOutside ) {
		return 0;
	} 

	return (lpPoint->y / iHeight) + iTopIndex; 
}

//================================================================ OnKeyPress
BOOL CNSAddressList::OnKeyPress( CWnd *pChildControl, UINT nKey, UINT nRepCnt, UINT nFlags )
{
	switch (nKey) 
	{
#ifdef MOZ_NEWADDR
		case 'J':
		case 'j':
			{
    			BOOL bCtrl = GetKeyState( VK_CONTROL ) & 0x8000;
				if(bCtrl)
				{
					ShowNameCompletionPicker(this);
					return TRUE;
				}
			}
			break;
#endif
		case VK_HOME:
			if (GetActiveSelection()) 
				SetActiveSelection(0);
			else if (m_bDrawTypeList)
				m_pAddressTypeList->SetFocus();
			break;
		case VK_END:
			if (GetActiveSelection() != (GetCount()-1)) 
				SetActiveSelection(GetCount()-1);
			else
				m_pNameField->SetFocus();
			break;
		case VK_UP:
			if (GetActiveSelection()) 
				SetActiveSelection(GetActiveSelection()-1);
			break;
		case VK_DOWN:
			if (GetActiveSelection()<GetCount()) 
				SetActiveSelection(GetActiveSelection()+1);
			break;
    	case VK_DELETE:
            if (GetActiveSelection()!=LB_ERR)
            {
                int nSelect = GetActiveSelection();
                if ((nSelect+1)<GetCount())
                    DeleteEntry(GetActiveSelection());
                int iLineLength = m_pNameField->LineLength();
                if (nKey == VK_DELETE)
                {
                    if (nSelect >= GetCount())
                    nSelect = GetCount()-1;
                    SetActiveSelection(nSelect);
                    Invalidate();
                }
                else
                    m_pNameField->SetSel(iLineLength,iLineLength,TRUE);
                return TRUE;
            }
            break;
        case VK_BACK:
            if (!m_pNameField->LineLength())
            {
                if (GetCount()>1 && GetActiveSelection())
                {
                    DeleteEntry(GetActiveSelection());
                    if (GetActiveSelection()==LB_ERR)
                        SetActiveSelection(0);
                    UpdateWindow();
                    int iLineLength = m_pNameField->LineLength();
                    m_pNameField->SetSel(iLineLength,iLineLength,TRUE);
                }
                else
                    return FALSE;
                return TRUE;
            }
           break;
	    case VK_RETURN:
        if (DoNotifySelectionChange() != -1)
			  {
				  if ((GetActiveSelection()+1 == GetCount()))
				  {
					  // don't add a second blank line
					  if ( m_pNameField->LineLength() == 0 )
                      {
				          CWnd *pNextWnd = GetNextWindow(GW_HWNDNEXT);
				          if (pNextWnd) 
					          pNextWnd->SetFocus();
				          else
					          GetParent()->SendMessage(WM_LEAVINGLASTFIELD);
                          return TRUE;
                      }
					  // add the new address entry to the list
					  InsertEntry( GetActiveSelection()+1,NULL);
					  // select the new address and make sure its visible
					  SetActiveSelection( GetActiveSelection()+1 );
					  return TRUE;
				  } 
				  else 
				  {
					  SetActiveSelection( GetActiveSelection()+1 );
				  }
				  return TRUE;
			  }
			else
				SetSel( GetActiveSelection(), TRUE);

			return TRUE;
	        break;
	}

	if (nKey == VK_TAB)
	{
    	if (DoNotifySelectionChange() != -1)
		{
    		BOOL bShift = GetKeyState( VK_SHIFT ) & 0x8000;
			if ( !bShift && (pChildControl->m_hWnd == m_pAddressTypeList->m_hWnd) )
      {
				m_pNameField->SetFocus();
      }
			else if ( m_bDrawTypeList && bShift && (pChildControl->m_hWnd == m_pNameField->m_hWnd) )
				m_pAddressTypeList->SetFocus();
			else if ( !bShift && (GetActiveSelection()+1 == GetCount()) )
			{
				CWnd *pNextWnd = GetNextWindow(GW_HWNDNEXT);
				if (pNextWnd) 
					pNextWnd->SetFocus();
				else
					GetParent()->SendMessage(WM_LEAVINGLASTFIELD);
			}
			else if ( bShift && (GetActiveSelection() == 0) ) // first line
			{
				CWnd *pNextWnd = GetNextWindow( GW_HWNDPREV );
				if (pNextWnd)
				pNextWnd->SetFocus();
			}
			else // middle line
			{
				// select the next or previous line in the address list
				if ( bShift )
				{
					SetActiveSelection( GetActiveSelection()-1 );
					m_pNameField->SetFocus();
				}
				else
				{
					SetActiveSelection( GetActiveSelection()+1 );
					if (m_bDrawTypeList)
						m_pAddressTypeList->SetFocus();
				}
			}
		}
		else
			SetSel( GetActiveSelection(), TRUE);

	return TRUE;
	}

	return FALSE; // not handled - let the child continue processing this key
}

BOOL CNSAddressList::isPointInItemBitmap(LPPOINT pPoint, int iIndex)
{
  CRect rect;
  if(GetItemRect(m_hWnd, iIndex, rect) == LB_ERR)
    return FALSE;

	CRect rectBitmap(m_iFieldControlWidth, 
                   rect.top, 
                   m_iFieldControlWidth + m_iBitmapWidth, 
                   rect.bottom );
	if (!m_bDrawTypeList)
	{
	    rectBitmap.left = rect.left;
	    rectBitmap.right = rect.left + m_iBitmapWidth;
	}
  
  BOOL bRet = rectBitmap.PtInRect(*pPoint);
  return bRet;
}

void CNSAddressList::selectEntry(int iIndex, BOOL bState)
{
  if(iIndex >= GetCount())
    return;
  CNSAddressInfo *pAddress = (CNSAddressInfo *)GetItemDataPtr(iIndex);
  if(pAddress == NULL)
    return;
  if(pAddress->getEntrySelectedState() == bState)
    return;
  pAddress->setEntrySelectedState(bState);
  RECT rcItem;
  GetItemRect(m_hWnd, iIndex, &rcItem);
  InvalidateRect(&rcItem);
}

BOOL CNSAddressList::isEntrySelected(int iIndex)
{
  CNSAddressInfo *pAddress = (CNSAddressInfo *)GetItemDataPtr(iIndex);
  if(pAddress == NULL)
    return FALSE;
  return pAddress->getEntrySelectedState();
}

void CNSAddressList::selectAllEntries(BOOL bState)
{
  for(int i = 0; i < GetCount(); i++)
    selectEntry(i, bState);
}

int CNSAddressList::getEntryMultipleSelectionStatus(BOOL * pbContinuous, int * piFirst, int * piLast)
{
  if(GetCount() == 0)
    return 0;

  int iFirst = -1;
  int iLast = -1;
  
  int iCounter = 0;
  BOOL bContinuousSelectionStarted = FALSE;
  BOOL bContinuousSelectionStopped = FALSE;
  BOOL bBroken = FALSE;

  for(int i = 0; i < GetCount(); i++)
  {
    if(isEntrySelected(i))
    {
      if(iFirst == -1)
        iFirst = i;
      iLast = i;
      if(bContinuousSelectionStopped)
        bBroken = TRUE;
      bContinuousSelectionStarted = TRUE;
      iCounter++;
      continue;
    }

    if(bContinuousSelectionStarted)
      bContinuousSelectionStopped = TRUE;
  }

  *pbContinuous = !bBroken;
  *piFirst = iFirst;
  *piLast = iLast;
  return iCounter;
}

void CNSAddressList::DrawEntryBitmap(
    int iSel, 
    CNSAddressInfo * pAddress, 
    CDC * pDC,
    BOOL bErase )
{
    CRect rect;
    if (GetItemRect(m_hWnd,iSel,rect)!=LB_ERR)
    {
	    CRect rectBitmap( m_iFieldControlWidth, rect.top, 
		    m_iFieldControlWidth + m_iBitmapWidth, rect.bottom );
	    if (!m_bDrawTypeList)
	    {
	        rectBitmap.left = rect.left;
	        rectBitmap.right = rect.left + m_iBitmapWidth;
	    }
	    int iBitmap = pAddress->GetBitmap();
	    if (!iBitmap)
	        iBitmap = GetDefaultBitmapId();
        if (iBitmap)
        {
          if(!isEntrySelected(iSel))
            NS_FillSolidRect(pDC->GetSafeHdc(),rectBitmap,GetSysColor(COLOR_WINDOW));
          else
          {
            rectBitmap.InflateRect(0, -1);
            NS_FillSolidRect(pDC->GetSafeHdc(),rectBitmap,GetSysColor(COLOR_HIGHLIGHT));
            rectBitmap.InflateRect(0, 1);
          }

          BITMAP bitmap;
	        CBitmap cbitmap;
	        cbitmap.LoadBitmap(MAKEINTRESOURCE(iBitmap));
            cbitmap.GetObject(sizeof(BITMAP),&bitmap);
            int center_x = rectBitmap.left + (rectBitmap.Width() - bitmap.bmWidth)/2;
	        int center_y = rectBitmap.top + (rectBitmap.Height()-bitmap.bmHeight)/2;
            DrawTransparentBitmap( 
                pDC->GetSafeHdc(), 
                (HBITMAP)cbitmap.GetSafeHandle(), 
                center_x, center_y, 
                RGB(255,0,255));
	        cbitmap.DeleteObject();
        }
    }
}

void CNSAddressList::UpdateHeaderType(void)
{
	CNSAddressInfo *pAddress = (CNSAddressInfo *)GetItemDataPtr(GetActiveSelection());
	if (pAddress != (CNSAddressInfo *)-1 && IsWindow(m_pAddressTypeList->m_hWnd) && m_bDrawTypeList)
    {
        CString cs;
        m_pAddressTypeList->GetText(m_pAddressTypeList->GetCurSel(),cs);
        if (strcmp(pAddress->GetType(),cs))
        {
            pAddress->SetType(cs);
            int iSel = m_pAddressTypeList->GetCurSel();
            if (iSel != -1)
            {
	            CNSAddressTypeInfo * pInfo = (CNSAddressTypeInfo*)m_pAddressTypeList->GetItemData(iSel);
            	ASSERT(pInfo!=(CNSAddressTypeInfo*)-1);
            	UINT idBitmap = pInfo->GetBitmap();
            	pAddress->SetBitmap(idBitmap);
            	CRect rect;
            	CDC * pDC = GetDC();
            	DrawEntryBitmap(GetActiveSelection(), pAddress, pDC, TRUE);
            	ReleaseDC(pDC);
            }
        }
        GetTypeFieldLength();        
    }
}

int CNSAddressList::GetTypeFieldLength()
{
    CString cs = "";
    char * pszString;
    HDC hDC = ::GetDC(m_hWnd);
    HGDIOBJ hOldFont =  ::SelectObject(hDC, (HFONT) ::SendMessage(m_pAddressTypeList->GetSafeHwnd(), WM_GETFONT, 0 ,0));
    int iMaxLength = 0;
    SIZE ptSize;
    for (int i =0; i<GetCount();i++ )
    {
        CNSAddressInfo *pAddress = (CNSAddressInfo *)GetItemDataPtr(i);
        pszString = pAddress->GetType();
        ::GetTextExtentPoint(hDC,pszString,strlen(pszString),&ptSize);
        int iTemp = ptSize.cx + (TEXT_PAD * 2) + m_iTypeBitmapWidth;
        if (iTemp > iMaxLength)
            iMaxLength = iTemp;
    }
    pszString = "A&";
     ::GetTextExtentPoint(hDC,pszString,strlen(pszString),&ptSize);
	iMaxLength -= ptSize.cx;
    ::SelectObject(hDC,hOldFont);
    ::ReleaseDC(m_hWnd,hDC);
    if (iMaxLength != m_iFieldControlWidth)
    {
        m_iFieldControlWidth = iMaxLength;
        ::InvalidateRect(m_hWnd,NULL,TRUE);
        ::UpdateWindow(m_hWnd);
    }
    return iMaxLength;
}

void CNSAddressList::UpdateHeaderContents(void)
{
	CNSAddressInfo *pAddress = (CNSAddressInfo *)GetItemDataPtr(GetActiveSelection());
	if (pAddress != (CNSAddressInfo *)-1)
    {
        CString cs;
        m_pNameField->GetWindowText(cs);
        pAddress->SetName(cs);
        if (m_bDrawTypeList)
        {
            CNSAddressTypeInfo * pInfo = 
                (CNSAddressTypeInfo *)m_pAddressTypeList->GetItemData(m_pAddressTypeList->GetCurSel());
            if (pInfo != (CNSAddressTypeInfo*)-1)
            {
                if (pInfo->GetExclusive())
                    pInfo->SetValue(cs);
            }
        }
    }
}

void CNSAddressList::SingleHeaderCommand(int nID)
{
  if (GetActiveSelection()!=LB_ERR && m_bDrawTypeList)
  {
    m_pAddressTypeList->SetCurSel(nID);
    UpdateHeaderType();
    if(!isEntrySelected(GetActiveSelection()))
      m_pNameField->SetFocus();
    CNSAddressTypeInfo * pInfo = (CNSAddressTypeInfo*)m_pAddressTypeList->GetItemData(nID);
    ASSERT(pInfo);
    if (pInfo->GetExclusive())
    {
      CString cs;
      m_pNameField->GetWindowText(cs);
      if (!cs.GetLength())
         m_pNameField->SetWindowText(pInfo->GetValue());
      pInfo->SetHidden(FALSE);
      pInfo->SetExclusive(FALSE);
      UpdateHeaderContents();
      int iLineLength = m_pNameField->LineLength();
      UpdateWindow();
      m_pNameField->SetSel(iLineLength,iLineLength,TRUE);
    }
	else
    {
      pInfo->SetExclusive(TRUE);
    }
  }
}

void CNSAddressList::HeaderCommand(int nID)
{
    BOOL bCont;
    int iFirst = 0;
    int iLast = 0;
    int iCount = getEntryMultipleSelectionStatus(&bCont, &iFirst, &iLast);

    if(iCount <= 0)
    {
      SingleHeaderCommand(nID);
      return;
    }

    int iCurSel = GetActiveSelection();

    for(int i = iFirst; i <= iLast; i++)
    {
      if(!isEntrySelected(i))
        continue;
      SetActiveSelection(i);
      SingleHeaderCommand(nID);
    }

    SetActiveSelection(iCurSel);
}

void CNSAddressList::DoSetFocus(HWND hwnd) 
{
	if ( (GetCount() > 0)
		&& (GetActiveSelection() != LB_ERR)
   )
	{
        if (m_bDrawTypeList)
            ::ShowWindow(m_pAddressTypeList->m_hWnd, SW_SHOW);
        m_pNameField->ShowWindow( SW_SHOW );

    if(m_EntrySelector.WhatEntryBitmapClicked() == -1)
    {
      if ( GetActiveSelection() == GetCount()-1 )
            ::SetFocus(m_pNameField->m_hWnd);
    	else if (m_bDrawTypeList)
            ::SetFocus(m_pAddressTypeList->m_hWnd);
    }
	}
}


void CNSAddressList::DoKillFocus(HWND hNewWnd) 
{
    if (::GetFocus() == m_pAddressTypeList->m_hWnd || ::GetFocus() == m_pNameField->m_hWnd || 
        ::GetFocus() == m_hWnd )
	{
		// focus is going to one the child controls
		return;
	}
    if (m_bDrawTypeList)
        ::ShowWindow(m_pAddressTypeList->m_hWnd,SW_HIDE);
	if (IsWindow(m_pNameField->m_hWnd))
        ::ShowWindow(m_pNameField->m_hWnd,SW_HIDE);
}


/////////////////////////////////////////////////////////////////////////////
// CNSAddressList virtual functions (overrides)

void CNSAddressList::DrawGridLine(CRect &rect, CDC * pDC)
{
    HDC hdc = pDC->GetSafeHdc();
    HPEN hOldPen = (HPEN)::SelectObject(hdc,(HGDIOBJ)m_hPenGrey);
    ::MoveToEx(hdc, rect.left + m_iFieldControlWidth, rect.bottom-1, NULL);
    ::LineTo(hdc, rect.right, rect.bottom-1);
    ::SelectObject(hdc,(HGDIOBJ)hOldPen);
}

void CNSAddressList::ComputeFieldWidths(CDC * pDC)
{
    if (m_bDrawTypeList)
    {
        m_iFieldControlWidth = 0;
        CBitmap cbitmap;
        cbitmap.LoadBitmap(IDB_ARROW3D);
        BITMAP bitmap;
        cbitmap.GetObject(sizeof(BITMAP), &bitmap );
        m_iTypeBitmapWidth = bitmap.bmWidth;
        cbitmap.DeleteObject();
    }

	m_iBitmapWidth = 24;
}

//=============================================================== DrawAddress
void CNSAddressList::DrawAddress( int nIndex, CRect &rect, CDC *pDC, BOOL bSelected )
{
	CNSAddressInfo *pAddress = (CNSAddressInfo *)GetItemDataPtr(nIndex);
	if (!pAddress)
		return;

	if (!m_iFieldControlWidth) 
		ComputeFieldWidths(pDC);

	// create CRects for each of the fields
	CRect rectType( rect.left, rect.top, m_iFieldControlWidth, rect.bottom );
	CRect rectAddress( 
    	(m_bDrawTypeList ? rectType.right : rect.left) + m_iBitmapWidth, rect.top, 
    	rect.right, rect.bottom );
	// end CRects

	// shrink the rectangle so that the controls draw within the grid
     rectAddress.bottom--;

	// If the bitmap is not provided, ask for it

	if( pAddress->GetBitmap() == NULL && m_bDrawTypeList )
	{
		// send notification to parent
		int position = m_pAddressTypeList->FindString(-1,pAddress->GetType());
		if (position != LB_ERR)
        {
            CNSAddressTypeInfo * pInfo = (CNSAddressTypeInfo*)m_pAddressTypeList->GetItemData(position);
            ASSERT(pInfo);
			pAddress->SetBitmap(pInfo->GetBitmap());
        }
	}

    int oldLine = m_lastIndex;

	int ioffset = ((rectAddress.Height()-m_pNameField->m_iTextHeight)+1)/2;
	int iremain = ((rectAddress.Height()-m_pNameField->m_iTextHeight)+1)%2;
	rectAddress.top += (ioffset+iremain);
	rectAddress.bottom -= ioffset;

  // draw the address type and address name fields
  if ( bSelected )
	{
	    if (m_bDrawTypeList)
	    {
		        // set the combobox
	        m_pAddressTypeList->MoveWindow( rectType, FALSE );
	        m_pAddressTypeList->SelectString(-1, pAddress->GetType());
            CNSAddressTypeInfo * pInfo = (CNSAddressTypeInfo *)m_pAddressTypeList->GetItemData(
               m_pAddressTypeList->GetCurSel());
            m_pNameField->SetNameCompletionFlag(pInfo->GetExpand());
	    }

	    int nStart, nEnd;
	    m_pNameField->GetSel(nStart,nEnd);

      m_pNameField->SetWindowText(pAddress->GetName());
	    m_pNameField->MoveWindow( rectAddress, FALSE );
      m_pNameField->UpdateWindow();

      m_lastIndex = nIndex;
      m_pNameField->SetSel(nStart,nEnd);
	}

	if (!bSelected || !m_pNameField->IsWindowVisible())
	{
    	COLORREF cText = pDC->SetTextColor( GetSysColor( COLOR_WINDOWTEXT ) );

      int iMode = pDC->SetBkMode(TRANSPARENT);

      // draw the address
    	pDC->DrawText(
			pAddress->GetName() ? pAddress->GetName() : "", 
	        -1, rectAddress, DT_LEFT | DT_BOTTOM | DT_NOPREFIX );

        if (m_bDrawTypeList)
        	m_pAddressTypeList->DrawItemSoItLooksLikeAButton(
	            pDC,rectType,CString(pAddress->GetType() ? pAddress->GetType():""));

      pDC->SetTextColor(cText);
      pDC->SetBkMode(iMode);
	}
  // end fields

	// draw the bitmap
    DrawEntryBitmap(nIndex,pAddress,pDC, 
		(GetCurSel()==(int)nIndex)?TRUE:FALSE);

    HDC hdc = pDC->GetSafeHdc();
	if (GetCurSel()==(int)nIndex)
	{
        HPEN hOldPen = (HPEN)::SelectObject(hdc, m_hPenNormal);
		if (GetCurSel()==GetTopIndex())
		{
            ::MoveToEx(hdc,m_iFieldControlWidth,rect.top, NULL);
            ::LineTo(hdc,rect.right,rect.top);
		}
        ::MoveToEx(hdc,m_iFieldControlWidth,rect.bottom-1, NULL);
        ::LineTo(hdc,rect.right,rect.bottom-1);
        ::SelectObject(hdc,(HGDIOBJ)hOldPen);
	}

   	DrawGridLine(rect, pDC);
    //ValidateRect(&rect);

    // draw the last line
    if (m_lastIndex != oldLine)
    {
        CRect badRect(0, 0, 0, 0);
        GetItemRect(m_hWnd,oldLine,badRect);
        InvalidateRect(badRect);
    }
}

//================================================================== DrawItem
void CNSAddressList::DrawItem( LPDRAWITEMSTRUCT lpDrawItemStruct ) 
{
	if (lpDrawItemStruct->itemID != -1) 
	{
		CRect rect(lpDrawItemStruct->rcItem);
        if (!m_iItemHeight)
            m_iItemHeight = rect.Height();
		CDC dc;
		dc.Attach(lpDrawItemStruct->hDC);
		DrawAddress(
			lpDrawItemStruct->itemID,
			rect,&dc,
			(int)lpDrawItemStruct->itemID == GetActiveSelection());

    	dc.Detach();
	}
}


//=============================================================== MeasureItem
void CNSAddressList::MeasureItem( LPMEASUREITEMSTRUCT lpMeasureItemStruct )
{
    static int iHeight = 0;
    if (!iHeight)
    {
        CDC * pdc = GetDC();
        TEXTMETRIC tm;
        pdc->GetTextMetrics(&tm);
        iHeight = tm.tmHeight;
        ReleaseDC(pdc);
    }
	lpMeasureItemStruct->itemHeight = iHeight + 1;
}


//================================================================ DeleteItem
void CNSAddressList::DeleteItem( LPDELETEITEMSTRUCT lpDeleteItemStruct ) 
{
	delete (CNSAddressInfo *)GetItemDataPtr( lpDeleteItemStruct->itemID );
}

void CNSAddressList::DisplayTypeList(int item)
{
	CRect rect;
    if (item == -1)
	item = GetActiveSelection();
	CMenu cmenu;
	if (cmenu.CreatePopupMenu()) 
	{
		for (int i = 0; i < m_pAddressTypeList->GetCount(); i++) 
		{
			CString cs;
			m_pAddressTypeList->GetText(i,cs);
			cmenu.AppendMenu(MF_STRING, IDM_HEADER+i, cs.Right(cs.GetLength()-1));
		}
		GetItemRect(m_hWnd,item,rect);
		CPoint pt(rect.left + m_iTypeBitmapWidth,rect.bottom);
		ClientToScreen(&pt);
		pt.y -= 2;
		cmenu.TrackPopupMenu(TPM_LEFTALIGN, pt.x, pt.y, this);
    }
}

int CNSAddressList::GetItemRect(HWND hwnd, int nIndex, LPRECT lpRect) const
{ 
    ASSERT(::IsWindow(hwnd)); 
    return (int)::SendMessage(m_hWnd, LB_GETITEMRECT, nIndex, (LPARAM)lpRect); 
}

static BOOL IsShiftPressed()
{
  short sShiftState = GetKeyState(VK_SHIFT);
  BOOL bDown = (sShiftState < 0);
  return bDown;
}

void CNSAddressList::onKeyDown(int iVirtKey, DWORD dwFlags)
{
  switch (iVirtKey)
  {
    case VK_TAB:
      selectAllEntries(FALSE);
      ::SetFocus((m_pNameField->m_hWnd));
      break;
    case VK_UP:
    case VK_LEFT:
    {
      if(GetCount() <= 1)
        break;

      int iIndex = GetActiveSelection();
			if(iIndex <= 0) 
        break;

      if(IsShiftPressed())
      {
        if(!isEntrySelected(iIndex - 1))
          selectEntry(iIndex - 1, TRUE);
        else
          selectEntry(iIndex, FALSE);
      }
      else
      {
        BOOL bCont;
        int iFirst = 0;
        int iLast = 0;

        int iCount = getEntryMultipleSelectionStatus(&bCont, &iFirst, &iLast);

        if(iCount < 1)
          break;

        if(iCount > 1)
          selectAllEntries(FALSE);
        else
          selectEntry(iIndex, FALSE);

        selectEntry(iIndex - 1, TRUE);
      }
      break;
    }
    case VK_DOWN:
    case VK_RIGHT:
    {
      if(GetCount() <= 1)
        break;

      int iIndex = GetActiveSelection();
			if(iIndex >= GetCount() - 1) 
        break;

      if(IsShiftPressed())
      {
        if(!isEntrySelected(iIndex + 1))
          selectEntry(iIndex + 1, TRUE);
        else
          selectEntry(iIndex, FALSE);
      }
      else
      {
        BOOL bCont;
        int iFirst = 0;
        int iLast = 0;
        int iCount = getEntryMultipleSelectionStatus(&bCont, &iFirst, &iLast);

        if(iCount < 1)
          break;

        if(iCount > 1)
          selectAllEntries(FALSE);
        else
          selectEntry(iIndex, FALSE);

        selectEntry(iIndex + 1, TRUE);
      }
      break;
    }
    case VK_DELETE:
    case VK_BACK:
    {
      BOOL bCont;
      int iFirst = 0;
      int iLast = 0;
      int iCount = getEntryMultipleSelectionStatus(&bCont, &iFirst, &iLast);

      for(int i = GetCount() - 1; i >= 0; i--)
      {
        if(isEntrySelected(i))
        {
          if(!DeleteEntry(i))
            break;
        }
      }

      if(GetCount() == 1)
        selectEntry(0, FALSE);

      SetActiveSelection((iLast < GetCount()) ? iLast : GetCount() - 1);

      ::InvalidateRect(m_hWnd, NULL, TRUE);
      ::UpdateWindow(m_hWnd);

      ::SetFocus((m_pNameField->m_hWnd));

      break;
    }
    default:
      break;
  }
}

static HCURSOR setDragYesCursor()
{
  HINSTANCE hInst =
#ifdef _AFXDLL
	AfxFindResourceHandle(MAKEINTRESOURCE(IDC_MOVEBUTTON), RT_GROUP_CURSOR);
#else
	AfxGetResourceHandle();
#endif
  HCURSOR hCursor = ::LoadCursor(hInst, MAKEINTRESOURCE(IDC_TEXT_MOVE));
  HCURSOR hCursorOld = ::SetCursor(hCursor);
  return hCursorOld;
}

static HCURSOR setDragNoCursor()
{
  HCURSOR hCursor = ::LoadCursor(NULL, IDC_NO);
  HCURSOR hCursorOld = ::SetCursor(hCursor);
  return hCursorOld;
}

static BOOL bDrugYesCursor = TRUE;

void CNSAddressList::onMouseMove(HWND hWnd, WORD wFlags, int iX, int iY)
{
  if(!(wFlags & MK_LBUTTON))
    return;
  if((wFlags & MK_CONTROL) || (wFlags & MK_SHIFT))
    return;
  if(m_EntrySelector.WhatEntryBitmapClicked() == -1)
    return;

  BOOL bCont;
  int iFirst = 0;
  int iLast = 0;
  int iCount = getEntryMultipleSelectionStatus(&bCont, &iFirst, &iLast);
  if(iCount == GetCount())
    return;

  if(!m_bDragging)
  {
    int iDelta = 3;
    if((abs(m_EntrySelector.m_iX - iX) <= iDelta) && (abs(m_EntrySelector.m_iY - iY) <= iDelta))
      return;

    ::SetCapture(hWnd);
    m_hCursorBackup = setDragYesCursor();
    m_bDragging = TRUE;
  }
  else
  {
    RECT rc;
    ::GetClientRect(m_hWnd, &rc);

    if((iY < rc.top) || (iY > rc.bottom) || (iX < rc.left) || (iX > rc.right))
    {
      if(bDrugYesCursor)
      {
        setDragNoCursor();
        bDrugYesCursor = FALSE;
      }
    }
    else
    {
      if(!bDrugYesCursor)
      {
        setDragYesCursor();
        bDrugYesCursor = TRUE;
      }
    }
  }
}

static int minFromThree(int i1, int i2, int i3)
{
  int iRes = min(min(i1, i2), i3);
  return iRes;
}

static int maxFromThree(int i1, int i2, int i3)
{
  int iRes = max(max(i1, i2), i3);
  return iRes;
}

//=============================================================== OnLButtonDown
void CNSAddressList::DoLButtonDown(HWND hwnd, UINT nFlags, LPPOINT lpPoint) 
{
    if (m_bDrawTypeList)
    {
	    BOOL bOutside;
	    int nNewSelect = ItemFromPoint(hwnd, lpPoint, &bOutside );

      if(!isPointInItemBitmap(lpPoint, nNewSelect))
      {
        RECT rect;
	      GetItemRect(m_hWnd,nNewSelect,&rect);
          int iHeight = rect.bottom - rect.top;
	      if (((lpPoint->y+iHeight)/iHeight)+GetTopIndex()>GetCount())
	      {
	          if (GetCount())
	          {
                  CNSAddressInfo *pAddress = (CNSAddressInfo *)GetItemDataPtr(GetCount()-1);
                  if (!pAddress->GetName()||!strlen(pAddress->GetName()))
                  {
                      SetActiveSelection(GetCount()-1);
                      ::SetFocus(m_pNameField->m_hWnd);
                      return;
                  }
	          }
	          AppendEntry();
              ::SetFocus(m_pNameField->m_hWnd);
	          return;
	      }
	      SetActiveSelection(nNewSelect);
      
        rect.right = m_iFieldControlWidth;
	      if ((lpPoint->x >= rect.left) && (lpPoint->x <= rect.right) &&
		      (lpPoint->y >= rect.top) && (lpPoint->y <= rect.bottom))
	      {
          if(!isEntrySelected(nNewSelect))
            selectAllEntries(FALSE);

		      m_bArrowDown = TRUE;
              DisplayTypeList(nNewSelect);
	      }
    	  else if (lpPoint->x < m_iFieldControlWidth)
        {
             ::SetFocus(m_pAddressTypeList->m_hWnd);
        }
    	  else
             ::SetFocus(m_pNameField->m_hWnd);
      }
      else
      {
	      SetActiveSelection(nNewSelect);
      
        m_EntrySelector.LButtonDown(nNewSelect);
        m_EntrySelector.m_iX = (int)lpPoint->x;
        m_EntrySelector.m_iY = (int)lpPoint->y;

        if(!(nFlags & MK_CONTROL))
        {
          BOOL bCont = FALSE;
          int iCount = 0;
          int iFirst = 0;
          int iLast = 0;

          if(nFlags & MK_SHIFT)
          {
            iCount = getEntryMultipleSelectionStatus(&bCont, &iFirst, &iLast);

            int iMin = minFromThree(nNewSelect, iFirst, iLast);
            int iMax = maxFromThree(nNewSelect, iFirst, iLast);

            int iStart = 0;
            int iStop = 0;

            if(iCount == 0)
            {
              iStart = 0;
              iStop = nNewSelect;
            }
            else if(iCount == 1)
            {
              iStart = min(iFirst, nNewSelect);
              iStop = max(nNewSelect, iLast);
            }
            else if(nNewSelect == iMin)
            {
              iStart = nNewSelect;
              iStop = iLast;
            }
            else if(nNewSelect == iMax)
            {
              iStart = iMin;
              iStop = nNewSelect;
            }
            else
            {
              iStart = iMin;
              iStop = nNewSelect;
            }

            selectAllEntries(FALSE);

            for(int i = iStart; i <= iStop; i++)
            {
              selectEntry(i, TRUE);
            }
          }
          else
          {
            if(!isEntrySelected(nNewSelect))
            {
              selectAllEntries(FALSE);
              selectEntry(nNewSelect, TRUE);
            }
            else
            {
              iCount = getEntryMultipleSelectionStatus(&bCont, &iFirst, &iLast);
              if(!bCont)
              {
                selectAllEntries(FALSE);
                selectEntry(nNewSelect, TRUE);
              }
              else
                m_EntrySelector.m_bPostponedTillButtonUp = TRUE;
            }
          }
        }
        ::SetFocus(::GetParent(m_pAddressTypeList->m_hWnd));

        return;
      }
    }
}

void CNSAddressList::DoLButtonUp(HWND hwnd, UINT nFlags, LPPOINT lpPoint)
{
	BOOL bOutside;
	int nNewSelect = ItemFromPoint(hwnd, lpPoint, &bOutside );

  if(GetCount() > 1)
  {
    if(isPointInItemBitmap(lpPoint, nNewSelect) && !m_bDragging)
    {
      if(m_EntrySelector.m_bPostponedTillButtonUp)
      {
        selectAllEntries(FALSE);
        selectEntry(nNewSelect, TRUE);
      }
      if((nFlags & MK_CONTROL) && (m_EntrySelector.WhatEntryBitmapClicked() == nNewSelect))
      {
        selectEntry(nNewSelect, !isEntrySelected(nNewSelect));
      }
    }
    m_EntrySelector.LButtonUp();
  }

  if(m_bDragging)
  {
    int iItemHeight = GetItemHeight(0);
    BOOL bAtTheVeryEnd = (lpPoint->y > iItemHeight * GetCount());

    int iInsertBefore = bAtTheVeryEnd ? GetCount() : nNewSelect;

    BOOL bCont = FALSE;
    int iCount = 0;
    int iFirst = 0;
    int iLast = 0;

    iCount = getEntryMultipleSelectionStatus(&bCont, &iFirst, &iLast);

    ASSERT(bCont);
    ASSERT(iFirst != -1);
    ASSERT(iLast != -1);

    if(!bCont || (iFirst == -1) || (iLast == -1))
      goto DontDrop;

    if((iInsertBefore <= iLast + 1) && (iInsertBefore >= iFirst))
      goto DontDrop;

    {
      NSAddressListEntry nsale;
      BOOL bDraggingDown = TRUE;

      if(iInsertBefore < iFirst)
        bDraggingDown = FALSE;

      int iCount = 0;

      for(int i = iFirst; i <= iLast; i++)
      {
        if(bDraggingDown)
        {
          GetEntry(iFirst, &nsale);
          InsertEntry(iInsertBefore, &nsale);
          selectEntry(iInsertBefore, TRUE);
          DeleteEntry(iFirst);
        }
        else
        {
          int iCount = i - iFirst;
          GetEntry(iFirst + iCount, &nsale);
          InsertEntry(iInsertBefore + iCount, &nsale);
          selectEntry(iInsertBefore + iCount, TRUE);
          DeleteEntry(iFirst + iCount + 1);
        }
      }

      if(bAtTheVeryEnd)
      {
        ::InvalidateRect(m_hWnd, NULL, TRUE);
        ::UpdateWindow(m_hWnd);
      }
    }

    if(bAtTheVeryEnd)
    {
      ::InvalidateRect(m_hWnd, NULL, TRUE);
      ::UpdateWindow(m_hWnd);
    }

DontDrop:
    if(m_hCursorBackup != NULL)
      ::SetCursor(m_hCursorBackup);
    ::ReleaseCapture();
    m_bDragging = FALSE;
  }

  m_EntrySelector.m_bPostponedTillButtonUp = FALSE;

	if ( bOutside ) 
		return;
	if (m_bArrowDown) 
    {
		m_bArrowDown = FALSE;
	}
	else
	{
    if(!isPointInItemBitmap(lpPoint, nNewSelect))
   {
      // set the selection
    	if ( nNewSelect != GetActiveSelection() )
  			SetActiveSelection( nNewSelect );
    }

		// set the focus
		if (lpPoint->x <=  m_iFieldControlWidth)
            ::SetFocus(m_pAddressTypeList->m_hWnd);
		else if((!isPointInItemBitmap(lpPoint, nNewSelect)) && (GetCount() > 1))
            ::SetFocus(m_pNameField->m_hWnd);
	}
}

void CNSAddressList::DoVScroll(HWND hwnd, UINT nSBCode, UINT nPos) 
{
    ::SetScrollPos(hwnd, SB_VERT, GetTopIndex(), TRUE);
}

//========================================================== OnChildLostFocus
// This method is called when one of the child control looses
// focus without preparation (ie no Tab or Enter).

void CNSAddressList::DoChildLostFocus()
{
	if (DoNotifySelectionChange(FALSE) != -1)
		DoKillFocus(NULL);
}

BOOL CNSAddressList::ParseAddressEntry(int nSelection)
{
    if (!m_bParse)
        return FALSE;
    NSAddressListEntry entry;
    if (GetEntry(nSelection, &entry))
    {
		int iCount;
		char * pNames = NULL, * pAddresses = NULL;
		if (!entry.szName || XP_STRLEN (entry.szName) == 0)
			return FALSE;
#ifndef MOZ_NEWADDR
		char * pName = m_pIAddressParent->NameCompletion((char *)entry.szName);
		if (pName)
		{
			free(pName);
			return FALSE;
		}
#endif
		
#ifdef MOZ_NEWADDR

// if it's got a cookie and has one result then we don't want to parse it
   	    CNSAddressInfo *pAddress = (CNSAddressInfo *)GetItemDataPtr(GetActiveSelection());
        if (pAddress)
        {
			AB_NameCompletionCookie *pCookie = pAddress->GetNameCompletionCookie();
			int nNumResults = pAddress->GetNumNameCompletionResults();

			if(pCookie && nNumResults == 1)
				return FALSE;

			//likewise, if we are in show picker mode and we have more than one 
			//result, we don't want to parse it either.
			if(pCookie && nNumResults > 1 && g_MsgPrefs.m_bShowCompletionPicker)
				return FALSE;

			//or if we are in show picker mode and don't have a cookie we don't want to
			//show it
			if(pCookie == NULL && g_MsgPrefs.m_bShowCompletionPicker)
				return FALSE;
		}
#endif
		iCount = MSG_ParseRFC822Addresses(entry.szName, &pNames, &pAddresses);
        if (iCount > 1)
        {
            char * p1, * p2;
            char *pLastType = NULL;
            BOOL bExpanded = FALSE;
            p1 = pNames;
            p2 = pAddresses;

			LockWindowUpdate();
	        for (int i = 0; i<iCount; i++)
	        {
		        NSAddressListEntry address;
		        memset(&address,'\0',sizeof(address));
				address.szType = entry.szType;
				if (pLastType)
					free(pLastType);
				if (address.szType)
					pLastType = strdup(address.szType);
		        address.szName = MSG_MakeFullAddress(p1,p2);
                while (*p1 != '\0') p1++;
                p1++;
                while (*p2 != '\0') p2++;
                p2++;
#ifdef MOZ_NEWADDR
				AB_NameCompletionCookie* pCookie =
					AB_GetNameCompletionCookieForNakedAddress(address.szName);
				//pAddress->SetNameCompletionCookie(pCookie);
				//			pAddress->SetNumNameCompletionResults(1);
				XP_FREE((void*)address.szName);
				address.szName = AB_GetHeaderString(pCookie);
#endif
				address.idEntry = (ULONG)-1;
		        bExpanded = TRUE;
		        InsertEntry(nSelection+i+1,&address, FALSE);
			    if (m_pIAddressParent)
					m_pIAddressParent->AddedItem(m_hWnd, 0, nSelection+i+1);

	        }

	        if (bExpanded)
	        {
    		    DeleteEntry(nSelection);
		        NSAddressListEntry address;
		        memset(&address,'\0',sizeof(address));
                char * pNextType = NULL;
				if (pLastType)
				{
					if (!strnicmp(pLastType,szLoadString(IDS_ADDRESSTO),strlen(szLoadString(IDS_ADDRESSTO))))
						pNextType = szLoadString(IDS_ADDRESSCC);
					else if (!strnicmp(pLastType,szLoadString(IDS_ADDRESSCC), strlen(szLoadString(IDS_ADDRESSCC))))
						pNextType = szLoadString(IDS_ADDRESSBCC);
				}
		        address.szName = "";
                address.szType = pNextType;
                address.idEntry = (ULONG)-1;
                AppendEntry(pNextType != NULL ? &address : NULL);
	        }
            if (pLastType)
                free(pLastType);
            ::LockWindowUpdate(NULL);
            Invalidate();
	        return TRUE;
	    }

		if (pNames)
			XP_FREE(pNames);
		if (pAddresses)
			XP_FREE(pAddresses);

    }
    return FALSE;
}

int CNSAddressList::DoNotifySelectionChange(BOOL bShowPicker)
{
	int result = TRUE;
    if (!ParseAddressEntry(GetActiveSelection()))
    {
    	if (m_pIAddressParent)
		{
		    NSAddressListEntry entry;
		    BOOL bRetVal = GetEntry(GetActiveSelection(),&entry);
			if (bRetVal)
			{
				unsigned long entryID = entry.idEntry;
				UINT bitmapID = entry.idBitmap;
				char * pszFullName = NULL;
                BOOL bExpand = TRUE;
                if (m_bDrawTypeList)
                {
                    CNSAddressTypeInfo * pInfo = (CNSAddressTypeInfo *)m_pAddressTypeList->GetItemData(
                        m_pAddressTypeList->GetCurSel());
                    bExpand = pInfo->GetExpand();
                }
                if (bExpand && m_bExpansion)
                {
            	    CNSAddressInfo *pAddress = (CNSAddressInfo *)GetItemDataPtr(GetActiveSelection());
                    if (pAddress && pAddress->GetExpansion())
                    {
#ifdef MOZ_NEWADDR
						AB_NameCompletionCookie *pCookie = pAddress->GetNameCompletionCookie();
						int nNumResults = pAddress->GetNumNameCompletionResults();
						BOOL bExpandName = FALSE;

						if(pCookie != NULL && nNumResults == 1)
						{
							pszFullName = AB_GetHeaderString(pCookie);

						}
						//if we have multiple matches and preference is set and we've been told it's
						//ok to show the picker(i.e. KillFocus doesn't show picker).
						else if((pCookie != NULL && nNumResults > 1 && bShowPicker
								&& g_MsgPrefs.m_bShowCompletionPicker )||
								(pCookie == NULL && nNumResults != 0 && g_MsgPrefs.m_bShowCompletionPicker))
						{
							ShowNameCompletionPicker(this);
							return -1;
						}
						//otherwise it's a naked address, so let's append the default domain.
						else
						{
							pCookie = AB_GetNameCompletionCookieForNakedAddress(entry.szName);
							pAddress->SetNameCompletionCookie(pCookie);
							pAddress->SetNumNameCompletionResults(1);
							pszFullName = AB_GetHeaderString(pCookie);
						}


		   			    result = m_pIAddressParent->ChangedItem(
			   			    (char*)entry.szName,GetActiveSelection(),m_hWnd, &pszFullName, &entryID, &bitmapID);
		    		    if (pszFullName != NULL)
			    	    {
                            pAddress->SetName(pszFullName);
					        if (bitmapID)
    						    SetItemBitmap(GetActiveSelection(), bitmapID);
    					    if (entryID)
    						    SetItemEntryID(GetActiveSelection(), entryID);
    					    free(pszFullName);
	    			    }
						StopNameCompletion(-1, FALSE);
						

#else
        			    result = m_pIAddressParent->ChangedItem(
	        			    (char*)entry.szName,GetActiveSelection(),m_hWnd, &pszFullName, &entryID, &bitmapID);
		    		    if (pszFullName != NULL)
			    	    {
                            pAddress->SetName(pszFullName);
					        if (bitmapID)
    						    SetItemBitmap(GetActiveSelection(), bitmapID);
    					    if (entryID)
    						    SetItemEntryID(GetActiveSelection(), entryID);
    					    free(pszFullName);
	    			    }
#endif
                    }
                }
			}
			Invalidate();
		}
    }
	return result;
}

void CNSAddressList::DoDisplayTypeList()
{
    DisplayTypeList();
}

void CNSAddressList::SetItemName(int nIndex, char * text)
{
	CNSAddressInfo *pAddress = (CNSAddressInfo *)GetItemDataPtr( nIndex );
    pAddress->SetName(text);
}

void CNSAddressList::SetItemBitmap(int nIndex, UINT id)
{
	CNSAddressInfo *pAddress = (CNSAddressInfo *)GetItemDataPtr( nIndex );
    pAddress->SetBitmap(id);
	CRect rect;
	GetItemRect(m_hWnd,nIndex,rect);
}

void CNSAddressList::SetItemEntryID(int nIndex, unsigned long id)
{
	CNSAddressInfo *pAddress = (CNSAddressInfo *)GetItemDataPtr( nIndex );
    pAddress->SetEntryID(id);
	CRect rect;
	GetItemRect(m_hWnd,nIndex,rect);
}

BOOL CNSAddressList::DoEraseBkgnd(HWND hwnd, HDC hdc) 
{
    RECT rect;
    ::GetClientRect(hwnd,&rect);
    if (!m_iItemHeight)
        m_iItemHeight = GetItemHeight(0);
    int iGridLines = (rect.bottom-rect.top)/m_iItemHeight;
    HBRUSH hOldBrush = (HBRUSH)::SelectObject(hdc,(HGDIOBJ)m_hBrushNormal);
    HPEN hOldPen = (HPEN)::SelectObject(hdc,(HGDIOBJ)m_hPenNormal);
    int iTotal = GetCount() - GetTopIndex();
    for (int i = 0; i <= iGridLines; i++)
    {
	    CRect GridRect(
	        rect.left,rect.top+(i*m_iItemHeight),
	        rect.right,rect.top + ((i+1)*m_iItemHeight) - 1);
        GridRect.top--;
	    if (i < iTotal)
	        GridRect.left = m_iFieldControlWidth;
	    GridRect.top++;
        NS_FillSolidRect(hdc,GridRect,GetSysColor(COLOR_WINDOW));
	    GridRect.top--;
        ::SelectObject(hdc,(HGDIOBJ)m_hPenGrid);
        ::MoveToEx(hdc,GridRect.left, GridRect.bottom , NULL);
        ::LineTo(hdc,GridRect.right, GridRect.bottom );
	    if (m_bDrawTypeList && m_pAddressTypeList->GetCount() && (i >= iTotal))
	    {
            ::MoveToEx(hdc,GridRect.left+m_iFieldControlWidth,GridRect.top, NULL);
            ::LineTo(hdc,GridRect.left+m_iFieldControlWidth,GridRect.bottom );
	    }
        ::SelectObject(hdc,(HGDIOBJ)m_hPenNormal);
    }
    ::SelectObject(hdc,(HGDIOBJ)hOldPen);
    ::SelectObject(hdc,(HGDIOBJ)hOldBrush);
	return TRUE;
}

int CNSAddressList::SetAddressList (
    LPNSADDRESSLIST pAddressList,
    int count 
    )
{
    ResetContent();
    if (pAddressList)
    {
        for (int index = 0; index < count; index++)
        {
    	    NSAddressListEntry address;
    	    address.szType = MSG_HeaderMaskToString(pAddressList[index].ulHeaderType);
			address.idBitmap = pAddressList[index].idBitmap;
	        address.idEntry = pAddressList[index].idEntry;
			if (pAddressList[index].szAddress)
            {
    	        address.szName = strdup (pAddressList[index].szAddress);
                free (pAddressList[index].szAddress);
            }
			AppendEntry(&address);
        }
        free(pAddressList);
    }
    return count;
}


int CNSAddressList::AppendEntry(
	BOOL expandName,
    LPCTSTR szType, 
    LPCTSTR szName, 
    UINT idBitmap, 
    unsigned long idEntry)
{
    if (!szType && !szName && !idBitmap && !idEntry)
        return AppendEntry(NULL, expandName);
    NSAddressListEntry address;
    address.szType = szType;
    address.szName = szName;
    address.idBitmap = idBitmap;
    address.idEntry = idEntry;
    return AppendEntry(&address, expandName);
}

int CNSAddressList::InsertEntry(
    int nIndex, 
	BOOL expandName,
    LPCTSTR szType, 
    LPCTSTR szName, 
    UINT idBitmap, 
    unsigned long idEntry)
{
    if (!szType && !szName && !idBitmap && !idEntry)
        return InsertEntry(nIndex,NULL);
    NSAddressListEntry address;
    address.szType = szType;
    address.szName = szName;
    address.idBitmap = idBitmap;
    address.idEntry = idEntry;
    return InsertEntry(nIndex,&address, expandName);
}

BOOL CNSAddressList::SetEntry( 
    int nIndex, 
    LPCTSTR szType, 
    LPCTSTR szName, 
    UINT idBitmap, 
    unsigned long idEntry)
{
    NSAddressListEntry address;
    address.szType = szType;
    address.szName = szName;
    address.idBitmap = idBitmap;
    address.idEntry = idEntry;
    return SetEntry(nIndex,&address);
}

#pragma optimize
void CNSAddressList::GetTypeInfo(
    int nIndex, 
    ADDRESS_TYPE_FLAG flag, 
    void ** pData)
{
    CListBox * pTypeList = GetAddressTypeComboBox();
    CNSAddressTypeInfo * pInfo = (CNSAddressTypeInfo*)pTypeList->GetItemData(nIndex);
    ASSERT(pData);
    *pData = NULL;
    if (flag & ADDRESS_TYPE_FLAG_VALUE)
        *pData = (void*)pInfo->GetValue();
    else if (flag & ADDRESS_TYPE_FLAG_HIDDEN)
        *pData = (void*)pInfo->GetHidden();
    else if (flag & ADDRESS_TYPE_FLAG_EXCLUSIVE)
        *pData = (void*)pInfo->GetExclusive();
    else if (flag & ADDRESS_TYPE_FLAG_USER)
        *pData = (void*)pInfo->GetUserData();
    else if (flag & ADDRESS_TYPE_FLAG_BITMAP)
        *pData = (void*)pInfo->GetBitmap();
}

BOOL CNSAddressList::GetEntry( 
    int nIndex, 
    char **szType, 
    char **szName, 
    UINT *idBitmap, 
    unsigned long *idEntry)
{
    NSAddressListEntry address;
    BOOL bRetVal = GetEntry(nIndex,&address);
    if (bRetVal)
    {
        if (szType)
            *szType = (char*)address.szType;
        if (szName)
            *szName = (char*)address.szName;
        if (idBitmap)
            *idBitmap = address.idBitmap;
        if (idEntry)
            *idEntry = address.idEntry;
    }
    return bRetVal;
}


int CNSAddressList::GetAddressList (
    LPNSADDRESSLIST * ppAddressList
    )
{
    int count = GetCount();
    if (count)
    {
        *ppAddressList = (LPNSADDRESSLIST)calloc(count,sizeof(NSADDRESSLIST));
        if (*ppAddressList)
        {
            for (int index = 0; index < count; index++)
            {
            	CNSAddressInfo *pAddress = (CNSAddressInfo *)GetItemDataPtr( index );
                ASSERT(pAddress);
                if (pAddress->GetType())
                    (*ppAddressList)[index].ulHeaderType = 
                    MSG_StringToHeaderMask(pAddress->GetType());
                if (pAddress->GetName())
                    (*ppAddressList)[index].szAddress = strdup(pAddress->GetName());
                (*ppAddressList)[index].idEntry = pAddress->GetEntryID();
                (*ppAddressList)[index].idBitmap = pAddress->GetBitmap();
            }
        }
    }
    return count;
}

STDMETHODIMP CNSAddressList::QueryInterface(
   REFIID refiid,
   LPVOID * ppv)
{
    *ppv = NULL;

    if (IsEqualIID(refiid,IID_IAddressControl))
        *ppv = (LPADDRESSCONTROL) this;

    if (*ppv != NULL) {
         AddRef();
         return NOERROR;
    }

    return CGenericObject::QueryInterface(refiid,ppv);
}

STDMETHODIMP_(ULONG) CNSAddressList::Release(void)
{
   ULONG ulRef;
   ulRef = --m_ulRefCount;
   if (m_ulRefCount == 0) {
      LPAPIAPI  pApiapi = GetAPIAPI();
      pApiapi->RemoveInstance(this);
      delete this;
   }
	return ulRef;   	
}

// Address Control Factory
////////////////////////////////////////////////////////////////////////////////////////

class CNSAddressControlFactory :  public  CGenericFactory
{
public:
    CNSAddressControlFactory();
    ~CNSAddressControlFactory();
    STDMETHODIMP CreateInstance(LPUNKNOWN pUnkOuter,REFIID refiid, LPVOID * ppvObj);
};

CNSAddressControlFactory::CNSAddressControlFactory()
{
    ApiApiPtr(api);
	api->RegisterClassFactory(APICLASS_ADDRESSCONTROL,this);
}

CNSAddressControlFactory::~CNSAddressControlFactory()
{
}

STDMETHODIMP CNSAddressControlFactory::CreateInstance(
    LPUNKNOWN pUnkOuter,
    REFIID refiid,
    LPVOID * ppvObj)
{
    CNSAddressList * pAddressControl = new CNSAddressList;
    *ppvObj = (LPVOID)((LPUNKNOWN)pAddressControl);
    return NOERROR;
}


static void FillSolidRect(HDC hdc, LPCRECT lpRect, COLORREF clr)
{
	::SetBkColor(hdc, clr);
	::ExtTextOut(hdc, 0, 0, ETO_OPAQUE, lpRect, NULL, 0, NULL);
}

static void FillSolidRect(HDC hdc, int x, int y, int cx, int cy, COLORREF clr)
{
	::SetBkColor(hdc, clr);
	RECT rect;
    rect.left = x;
    rect.top = y;
    rect.right =  x + cx;
    rect.bottom = y + cy;
	::ExtTextOut(hdc, 0, 0, ETO_OPAQUE, &rect, NULL, 0, NULL);
}

static void Draw3dRect(HDC hdc, int x, int y, int cx, int cy, COLORREF clrTopLeft, COLORREF clrBottomRight)
{
	FillSolidRect(hdc, x, y, cx - 1, 1, clrTopLeft);
	FillSolidRect(hdc, x, y, 1, cy - 1, clrTopLeft);
	FillSolidRect(hdc, x + cx, y, -1, cy, clrBottomRight);
	FillSolidRect(hdc, x, y + cy, cx, -1, clrBottomRight);
}

static void Draw3dRect(HDC hdc, LPCRECT lpRect, COLORREF clrTopLeft, COLORREF clrBottomRight)
{
	Draw3dRect(hdc, lpRect->left, lpRect->top, lpRect->right - lpRect->left,
		lpRect->bottom - lpRect->top, clrTopLeft, clrBottomRight);
}

void NS_FillSolidRect(HDC hdc, LPCRECT crRect, COLORREF rgbFill)	{
    FillSolidRect(hdc,crRect, rgbFill);
}

void NS_Draw3dRect(HDC hdc, LPCRECT crRect, COLORREF rgbTL, COLORREF rgbBR)	
{
#ifdef XP_WIN32
	Draw3dRect(hdc, crRect, rgbTL, rgbBR);
#else
	CRect crMunge = crRect;
	crMunge.right -= 1;
	crMunge.bottom = crMunge.top + 1;	
	NS_FillSolidRect(hdc, crMunge, rgbTL);
	
	crMunge = crRect;
	crMunge.right = crMunge.left + 1;
	crMunge.bottom -= 1;
	NS_FillSolidRect(hdc, crMunge, rgbTL);
	
	crMunge = crRect;
	crMunge.left += crMunge.right;
	crMunge.right -= 1;
	NS_FillSolidRect(hdc, crMunge, rgbBR);
	
	crMunge = crRect;
	crMunge.top = crMunge.bottom;
	crMunge.bottom -= 1;
	NS_FillSolidRect(hdc, crMunge, rgbBR);
#endif	
}

//============================================================= DrawHighlight
// Draw the highlight around the area
static void DrawHighlight( HDC hDC, LPRECT lpRect, COLORREF clrTopLeft, COLORREF clrBottomRight )
{
	HPEN hpenTopLeft = ::CreatePen( PS_SOLID, 0, clrTopLeft );
	HPEN hpenBottomRight = ::CreatePen( PS_SOLID, 0, clrBottomRight );
	HPEN hpenOld = (HPEN) ::SelectObject( hDC, hpenTopLeft );

	::MoveToEx( hDC, lpRect->left, lpRect->bottom, NULL );
	::LineTo( hDC, lpRect->left, lpRect->top );
	::LineTo( hDC, lpRect->right - 1, lpRect->top );

	::SelectObject( hDC, hpenBottomRight );
	::LineTo( hDC, lpRect->right - 1, lpRect->bottom - 1);
	::LineTo( hDC, lpRect->left, lpRect->bottom - 1);

	::SelectObject( hDC, hpenOld );

	VERIFY(::DeleteObject( hpenTopLeft ));
	VERIFY(::DeleteObject( hpenBottomRight ));
}


//============================================================ DrawRaisedRect
void NS_DrawRaisedRect( HDC hDC, LPRECT lpRect )
{
	RECT rcTmp = *lpRect;
	::InflateRect( &rcTmp, -1, -1 );
#ifdef _WIN32
	DrawHighlight( hDC, &rcTmp, 
				   GetSysColor( COLOR_3DLIGHT ), 
				   GetSysColor( COLOR_3DSHADOW ) );
	DrawHighlight( hDC, lpRect, 
				   GetSysColor( COLOR_3DHILIGHT ), 
				   GetSysColor( COLOR_3DDKSHADOW ) );
#else
	DrawHighlight( hDC, lpRect, 
				   GetSysColor( COLOR_BTNHIGHLIGHT ), 
				   GetSysColor( COLOR_BTNSHADOW ) );
	DrawHighlight( hDC, &rcTmp, 
				   GetSysColor( COLOR_BTNHIGHLIGHT ), 
				   GetSysColor( COLOR_BTNSHADOW ) );
#endif
}


//=========================================================== DrawLoweredRect
void NS_DrawLoweredRect( HDC hDC, LPRECT lpRect )
{
	RECT rcTmp = *lpRect;
	::InflateRect( &rcTmp, -1, -1 );
#ifdef _WIN32
	DrawHighlight( hDC, &rcTmp, 
				   GetSysColor( COLOR_3DSHADOW ), 
				   GetSysColor( COLOR_3DLIGHT ) );
	DrawHighlight( hDC, lpRect, 
				   GetSysColor( COLOR_3DDKSHADOW ), 
				   GetSysColor( COLOR_3DHILIGHT) );
#else
	DrawHighlight( hDC, &rcTmp, 
				   GetSysColor( COLOR_BTNSHADOW ), 
				   GetSysColor( COLOR_BTNHIGHLIGHT ) );
	DrawHighlight( hDC, lpRect, 
				   GetSysColor( COLOR_BTNSHADOW ), 
				   GetSysColor( COLOR_BTNHIGHLIGHT ) );
#endif
}

void NS_Draw3DButtonRect( HDC hDC, LPRECT lpRect, BOOL bPushed )
{
	if ( bPushed ) {
		NS_DrawLoweredRect( hDC, lpRect );
	} else {
		NS_DrawRaisedRect( hDC, lpRect );
	}
}

#if defined(__APIAPIDLL)
__declspec( dllexport )
int ApiBooter(char * pszClassName)
{
    if (!stricmp(pszClassName,APICLASS_ADDRESSCONTROL))
        new CNSAddressControlFactory;
    return 1;
}
#else
DECLARE_FACTORY(CNSAddressControlFactory);
#endif

// Address Control Object

