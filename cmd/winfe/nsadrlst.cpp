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
#ifdef __APIAPIDLL
#include "festuff.h"
#else
#include "compstd.h"
#endif
#include "resource.h"

#define IDM_HEADER                      8192
#define MAX_HEADER_ITEMS                20

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
   m_pNameField            = new CNSAddressNameEditField;
   m_pAddressTypeList      = new CNSAddressTypeControl;
   m_iItemHeight           = 0;
   m_pIAddressParent       = NULL;
   m_bCreated              = FALSE;
   m_hTextFont             = NULL;
   m_bParse                = TRUE;
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
            return (DoNotifySelectionChange());
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
    }
    return CListBox::DefWindowProc(message,wParam,lParam);
}

void CNSAddressList::EnableParsing(BOOL bParse)
{
    m_bParse = bParse;
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
}

BOOL CNSAddressList::DoCommand( HWND hwnd, WPARAM wParam, LPARAM lParam )
{
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
            	CNSAddressInfo *pLastAddress = 
                    (CNSAddressInfo *)GetItemDataPtr(index-1);
                cs = pLastAddress->GetType();                                    
                int index = m_pAddressTypeList->FindStringExact(-1,cs);
                if (index != LB_ERR)
                {
                    CNSAddressTypeInfo * pInfo = (CNSAddressTypeInfo *)m_pAddressTypeList->GetItemData(index);
                    ASSERT(pInfo);
                    if (pInfo->GetExclusive())
                        m_pAddressTypeList->GetText(0,cs);
                }
            }
            else
        	      m_pAddressTypeList->GetText(0,cs);
			pAddress->SetType(cs);
    		pAddress->SetBitmap(0);
			pAddress->SetEntryID();
            pAddress->SetName("");
    	}
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

    if (m_pIAddressParent)
	{
	    NSAddressListEntry entry;
	    BOOL bRetVal = GetEntry(nIndex,&entry);
		if (bRetVal)
		{
			unsigned long entryID = 0;
			UINT bitmapID = 0;
			char * pszFullName = NULL;

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
                if (bExpand)
                {
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

    if (GetCount() == 1)
    {
    	m_pNameField->SetWindowText("");
    	UpdateHeaderContents();
		if (m_pIAddressParent)
			m_pIAddressParent->DeletedItem(m_hWnd, 0, nIndex);
		return FALSE;
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
   		VERIFY( m_pNameField->Create( this ) );
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
				m_pNameField->SetFocus();
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
	        if (bErase)
                NS_FillSolidRect(pDC->GetSafeHdc(),rectBitmap,GetSysColor(COLOR_WINDOW));
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

void CNSAddressList::HeaderCommand(int nID)
{
    if (GetActiveSelection()!=LB_ERR && m_bDrawTypeList)
    {
        m_pAddressTypeList->SetCurSel(nID);
        UpdateHeaderType();
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
			CString cs;
			m_pAddressTypeList->GetText (nID, cs);
			if (!strnicmp(cs,szLoadString(IDS_ADDRESSREPLYTO),strlen(szLoadString(IDS_ADDRESSREPLYTO))))
				pInfo->SetExclusive(TRUE);
			else if (!strnicmp(cs,szLoadString(IDS_ADDRESSFOLLOWUPTO), strlen(szLoadString(IDS_ADDRESSFOLLOWUPTO))))
				pInfo->SetExclusive(TRUE);
        }
    }
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
    	if ( GetActiveSelection() == GetCount()-1 )
            ::SetFocus(m_pNameField->m_hWnd);
    	else if (m_bDrawTypeList)
            ::SetFocus(m_pAddressTypeList->m_hWnd);
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
		int iMode = pDC->SetBkMode( TRANSPARENT );

		// draw the address
    	pDC->DrawText(
			pAddress->GetName() ? pAddress->GetName() : "", 
	        -1, rectAddress, DT_LEFT | DT_BOTTOM | DT_NOPREFIX );

    	pDC->SetTextColor(cText);
    	pDC->SetBkMode(iMode);

        if (m_bDrawTypeList)
        	m_pAddressTypeList->DrawItemSoItLooksLikeAButton(
	            pDC,rectType,CString(pAddress->GetType() ? pAddress->GetType():""));

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
    ValidateRect(&rect);

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

//=============================================================== OnLButtonDown
void CNSAddressList::DoLButtonDown(HWND hwnd, UINT nFlags, LPPOINT lpPoint) 
{
    if (m_bDrawTypeList)
    {
	    BOOL bOutside;
	    int nNewSelect = ItemFromPoint(hwnd, lpPoint, &bOutside );
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
		    m_bArrowDown = TRUE;
            DisplayTypeList(nNewSelect);
	    }
    	else if (lpPoint->x < m_iFieldControlWidth)
           ::SetFocus(m_pAddressTypeList->m_hWnd);
    	else
           ::SetFocus(m_pNameField->m_hWnd);
    }
}

void CNSAddressList::DoLButtonUp(HWND hwnd, UINT nFlags, LPPOINT lpPoint)
{
	BOOL bOutside;
	int nNewSelect = ItemFromPoint(hwnd, lpPoint, &bOutside );
	if ( bOutside ) 
		return;
	if (m_bArrowDown) 
    {
		m_bArrowDown = FALSE;
	}
	else
	{

		// set the selection
    	if ( nNewSelect != GetActiveSelection() )
    	{
			SetActiveSelection( nNewSelect );
    	}

		// set the focus
		if (lpPoint->x <=  m_iFieldControlWidth)
            ::SetFocus(m_pAddressTypeList->m_hWnd);
		else 
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
	if (DoNotifySelectionChange() != -1)
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

		char * pName = m_pIAddressParent->NameCompletion((char *)entry.szName);
		if (pName)
		{
			free(pName);
			return FALSE;
		}
		
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
				address.idEntry = (ULONG)-1;
		        bExpanded = TRUE;
		        InsertEntry(nSelection+i+1,&address, FALSE);
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

int CNSAddressList::DoNotifySelectionChange()
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
                if (bExpand)
                {
            	    CNSAddressInfo *pAddress = (CNSAddressInfo *)GetItemDataPtr(GetActiveSelection());
                    if (pAddress && pAddress->GetExpansion())
                    {

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
        return AppendEntry();
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

